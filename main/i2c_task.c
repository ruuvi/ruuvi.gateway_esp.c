/**
 * @file i2c_task.c
 * @author TheSomeMan
 * @date 2023-07-03
 * @copyright Ruuvi Innovations Ltd, license BSD-3-Clause.
 */

#include "i2c_task.h"
#include <esp_task_wdt.h>
#include "os_signal.h"
#include "os_timer_sig.h"
#include "driver/i2c.h"
#include "sen5x_wrap.h"
#include "scd4x_wrap.h"
#define LOG_LOCAL_LEVEL LOG_LEVEL_INFO
#include "log.h"

#define I2C_MASTER_FREQ_HZ 100000
#define I2C_TASK_I2C_NUM   (I2C_NUM_0)
#define I2C_TASK_I2C_SCL   (GPIO_NUM_32)
#define I2C_TASK_I2C_SDA   (GPIO_NUM_33)

#define I2C_TASK_SEN5X_POLL_PERIOD_MS  (1000U)
#define I2C_TASK_SEN5X_CHECK_PERIOD_MS (100U)
#define I2C_TASK_SCD4X_POLL_PERIOD_MS  (5000U)
#define I2C_TASK_SCD4X_CHECK_PERIOD_MS (100U)

#define I2C_TASK_WATCHDOG_FEEDING_PERIOD_TICKS pdMS_TO_TICKS(1000)

typedef enum i2c_task_sig_e
{
    I2C_TASK_SIG_POLL_SEN5X         = OS_SIGNAL_NUM_0,
    I2C_TASK_SIG_POLL_SCD4X         = OS_SIGNAL_NUM_1,
    I2C_TASK_SIG_TASK_WATCHDOG_FEED = OS_SIGNAL_NUM_2,
} i2c_task_sig_e;

#define I2C_TASK_SIG_FIRST (I2C_TASK_SIG_POLL_SEN5X)
#define I2C_TASK_SIG_LAST  (I2C_TASK_SIG_TASK_WATCHDOG_FEED)

static const char* TAG = "I2C_TASK";

static bool                           g_i2c_task_flag_sen5x_present = false;
static bool                           g_i2c_task_flag_scd4x_present = false;
static os_signal_t*                   g_p_i2c_task_sig;
static os_signal_static_t             g_i2c_task_sig_mem;
static os_timer_sig_one_shot_t*       g_p_i2c_task_timer_sig_poll_sen5x;
static os_timer_sig_one_shot_static_t g_i2c_task_timer_sig_poll_sen5x_mem;
static os_timer_sig_one_shot_t*       g_p_i2c_task_timer_sig_poll_scd4x;
static os_timer_sig_one_shot_static_t g_i2c_task_timer_sig_poll_scd4x_mem;
static os_timer_sig_periodic_t*       g_p_i2c_task_timer_sig_watchdog_feed;
static os_timer_sig_periodic_static_t g_i2c_task_timer_sig_watchdog_feed_mem;

ATTR_PURE
static os_signal_num_e
i2c_task_conv_to_sig_num(const i2c_task_sig_e sig)
{
    return (os_signal_num_e)sig;
}

static i2c_task_sig_e
i2c_task_conv_from_sig_num(const os_signal_num_e sig_num)
{
    assert(((os_signal_num_e)I2C_TASK_SIG_FIRST <= sig_num) && (sig_num <= (os_signal_num_e)I2C_TASK_SIG_LAST));
    return (i2c_task_sig_e)sig_num;
}

static bool
i2c_task_check_if_external_pull_up_exist(const uint32_t gpio_pin)
{
    gpio_config_t io_conf = {
        .pin_bit_mask = 1ULL << gpio_pin,
        .mode         = GPIO_MODE_INPUT,
        .pull_up_en   = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type    = GPIO_INTR_DISABLE,
    };

    gpio_config(&io_conf);

    int state1 = gpio_get_level(gpio_pin);

    io_conf.pull_up_en = GPIO_PULLUP_ENABLE;
    gpio_config(&io_conf);

    int state2 = gpio_get_level(gpio_pin);

    io_conf.pull_up_en = GPIO_PULLUP_DISABLE;
    io_conf.mode       = GPIO_MODE_DISABLE;
    gpio_config(&io_conf);

    if ((0 == state1) && (1 == state2))
    {
        return false;
    }
    return true;
}

static bool
i2c_task_master_init(void)
{
    const int    i2c_master_port = I2C_TASK_I2C_NUM;
    i2c_config_t conf            = {
                   .mode             = I2C_MODE_MASTER,
                   .sda_io_num       = I2C_TASK_I2C_SDA,
                   .sda_pullup_en    = GPIO_PULLUP_ENABLE,
                   .scl_io_num       = I2C_TASK_I2C_SCL,
                   .scl_pullup_en    = GPIO_PULLUP_ENABLE,
                   .master.clk_speed = I2C_MASTER_FREQ_HZ,
    };
    i2c_param_config(i2c_master_port, &conf);
    const esp_err_t err = i2c_driver_install(i2c_master_port, conf.mode, 0, 0, 0);
    if (0 != err)
    {
        LOG_ERR_ESP(err, "%s failed", "i2c_driver_install");
        return false;
    }
    return true;
}

static void
i2c_task_register_signals(void)
{
    os_signal_add(g_p_i2c_task_sig, i2c_task_conv_to_sig_num(I2C_TASK_SIG_POLL_SEN5X));
    os_signal_add(g_p_i2c_task_sig, i2c_task_conv_to_sig_num(I2C_TASK_SIG_POLL_SCD4X));
    os_signal_add(g_p_i2c_task_sig, i2c_task_conv_to_sig_num(I2C_TASK_SIG_TASK_WATCHDOG_FEED));
}

static void
i2c_task_create_timers(void)
{
    if (g_i2c_task_flag_sen5x_present)
    {
        g_p_i2c_task_timer_sig_poll_sen5x = os_timer_sig_one_shot_create_static(
            &g_i2c_task_timer_sig_poll_sen5x_mem,
            "i2c_task:sen5x",
            g_p_i2c_task_sig,
            i2c_task_conv_to_sig_num(I2C_TASK_SIG_POLL_SEN5X),
            pdMS_TO_TICKS(I2C_TASK_SIG_POLL_SEN5X));
    }

    if (g_i2c_task_flag_scd4x_present)
    {
        g_p_i2c_task_timer_sig_poll_scd4x = os_timer_sig_one_shot_create_static(
            &g_i2c_task_timer_sig_poll_scd4x_mem,
            "i2c_task:scd4x",
            g_p_i2c_task_sig,
            i2c_task_conv_to_sig_num(I2C_TASK_SIG_POLL_SCD4X),
            pdMS_TO_TICKS(I2C_TASK_SCD4X_POLL_PERIOD_MS));
    }

    g_p_i2c_task_timer_sig_watchdog_feed = os_timer_sig_periodic_create_static(
        &g_i2c_task_timer_sig_watchdog_feed_mem,
        "i2c_task:wdog",
        g_p_i2c_task_sig,
        i2c_task_conv_to_sig_num(I2C_TASK_SIG_TASK_WATCHDOG_FEED),
        I2C_TASK_WATCHDOG_FEEDING_PERIOD_TICKS);
}

static void
i2c_task_wdt_add_and_start(void)
{
    LOG_INFO("TaskWatchdog: Register current thread");
    const esp_err_t err = esp_task_wdt_add(xTaskGetCurrentTaskHandle());
    if (ESP_OK != err)
    {
        LOG_ERR_ESP(err, "%s failed", "esp_task_wdt_add");
    }
    LOG_INFO("TaskWatchdog: Start timer");
    os_timer_sig_periodic_start(g_p_i2c_task_timer_sig_watchdog_feed);
}

static void
i2c_task_handle_sig_poll_sen5x(void)
{
    sen5x_wrap_measurement_t sen5x_measurement = { 0 };
    if (!sen5x_wrap_read_measured_values(&sen5x_measurement))
    {
        os_timer_sig_one_shot_restart(g_p_i2c_task_timer_sig_poll_sen5x, pdMS_TO_TICKS(I2C_TASK_SEN5X_CHECK_PERIOD_MS));
        return;
    }

    os_timer_sig_one_shot_restart(
        g_p_i2c_task_timer_sig_poll_sen5x,
        pdMS_TO_TICKS(I2C_TASK_SEN5X_POLL_PERIOD_MS - I2C_TASK_SEN5X_CHECK_PERIOD_MS));

    LOG_INFO(
        "Mass concentration pm1p0: %.1f µg/m³",
        sen5x_measurement.mass_concentration_pm1p0 / 10.0f);
    LOG_INFO(
        "Mass concentration pm2p5: %.1f µg/m³",
        sen5x_measurement.mass_concentration_pm2p5 / 10.0f);
    LOG_INFO(
        "Mass concentration pm4p0: %.1f µg/m³",
        sen5x_measurement.mass_concentration_pm4p0 / 10.0f);
    LOG_INFO(
        "Mass concentration pm10p0: %.1f µg/m³",
        sen5x_measurement.mass_concentration_pm10p0 / 10.0f);
    if (sen5x_measurement.ambient_humidity == 0x7fff)
    {
        LOG_INFO("Ambient humidity: n/a");
    }
    else
    {
        LOG_INFO("Ambient humidity: %.1f %%RH", sen5x_measurement.ambient_humidity / 100.0f);
    }
    if (sen5x_measurement.ambient_temperature == 0x7fff)
    {
        LOG_INFO("Ambient temperature: n/a");
    }
    else
    {
        LOG_INFO("Ambient temperature: %.1f °C", sen5x_measurement.ambient_temperature / 200.0f);
    }
    if (sen5x_measurement.voc_index == 0x7fff)
    {
        LOG_INFO("Voc index: n/a");
    }
    else
    {
        LOG_INFO("Voc index: %.1f", sen5x_measurement.voc_index / 10.0f);
    }
    if (sen5x_measurement.nox_index == 0x7fff)
    {
        LOG_INFO("Nox index: n/a");
    }
    else
    {
        LOG_INFO("Nox index: %.1f", sen5x_measurement.nox_index / 10.0f);
    }
}

static void
i2c_task_handle_sig_poll_scd4x(void)
{
    scd4x_wrap_measurement_t measurement = { 0 };
    if (!scd4x_wrap_read_measurement(&measurement))
    {
        LOG_ERR("%s failed", "scd4x_wrap_read_measurement");
        os_timer_sig_one_shot_restart(g_p_i2c_task_timer_sig_poll_scd4x, pdMS_TO_TICKS(I2C_TASK_SCD4X_CHECK_PERIOD_MS));
        return;
    }
    os_timer_sig_one_shot_restart(
        g_p_i2c_task_timer_sig_poll_scd4x,
        pdMS_TO_TICKS(I2C_TASK_SCD4X_POLL_PERIOD_MS - I2C_TASK_SCD4X_CHECK_PERIOD_MS));
    LOG_INFO("SCD4X: CO2: %u", measurement.co2);
    LOG_INFO("SCD4X: Temperature: %d m°C", measurement.temperature);
    LOG_INFO("SCD4X: Humidity: %d mRH", measurement.humidity);
}

static void
i2c_task_handle_sig_task_watchdog_feed(void)
{
    LOG_DBG("Feed watchdog");
    const esp_err_t err = esp_task_wdt_reset();
    if (ESP_OK != err)
    {
        LOG_ERR_ESP(err, "%s failed", "esp_task_wdt_reset");
    }
}

static void
i2c_task_handle_sig(const i2c_task_sig_e i2c_task_sig)
{
    switch (i2c_task_sig)
    {
        case I2C_TASK_SIG_POLL_SEN5X:
            i2c_task_handle_sig_poll_sen5x();
            break;
        case I2C_TASK_SIG_POLL_SCD4X:
            i2c_task_handle_sig_poll_scd4x();
            break;
        case I2C_TASK_SIG_TASK_WATCHDOG_FEED:
            i2c_task_handle_sig_task_watchdog_feed();
            break;
    }
}

ATTR_NORETURN
static void
i2c_task(void)
{
    if (!os_signal_register_cur_thread(g_p_i2c_task_sig))
    {
        LOG_ERR("%s failed", "os_signal_register_cur_thread");
    }

    i2c_task_wdt_add_and_start();
    if (g_i2c_task_flag_sen5x_present)
    {
        os_timer_sig_one_shot_start(g_p_i2c_task_timer_sig_poll_sen5x);
    }
    if (g_i2c_task_flag_scd4x_present)
    {
        os_timer_sig_one_shot_start(g_p_i2c_task_timer_sig_poll_scd4x);
    }

    LOG_INFO("%s started", __func__);

    for (;;)
    {
        os_signal_events_t sig_events = { 0 };
        if (!os_signal_wait_with_timeout(g_p_i2c_task_sig, OS_DELTA_TICKS_INFINITE, &sig_events))
        {
            continue;
        }
        for (;;)
        {
            const os_signal_num_e sig_num = os_signal_num_get_next(&sig_events);
            if (OS_SIGNAL_NUM_NONE == sig_num)
            {
                break;
            }
            const i2c_task_sig_e i2c_task_sig = i2c_task_conv_from_sig_num(sig_num);
            i2c_task_handle_sig(i2c_task_sig);
        }
    }
}

bool
i2c_task_init(void)
{
    if (!i2c_task_check_if_external_pull_up_exist(I2C_TASK_I2C_SCL))
    {
        LOG_INFO("No external pull-up resistor detected on I2C SCL - don't start I2C polling");
        return true;
    }
    LOG_INFO("External pull-up resistor detected on I2C SCL - start I2C polling");

    if (!i2c_task_master_init())
    {
        LOG_ERR("i2c_task_master_init failed");
        return false;
    }
    g_i2c_task_flag_sen5x_present = sen5x_wrap_check();
    g_i2c_task_flag_scd4x_present = scd4x_wrap_check();
    if (g_i2c_task_flag_sen5x_present)
    {
        if (!sen5x_wrap_start_measurement())
        {
            LOG_ERR("%s failed", "sen5x_wrap_start_measurement");
            g_i2c_task_flag_sen5x_present = false;
        }
    }
    if (g_i2c_task_flag_scd4x_present)
    {
        if (!scd4x_wrap_start_periodic_measurement())
        {
            LOG_ERR("%s failed", "scd4x_wrap_start_periodic_measurement");
            g_i2c_task_flag_scd4x_present = false;
        }
    }

    if (!g_i2c_task_flag_sen5x_present && !g_i2c_task_flag_scd4x_present)
    {
        return false;
    }

    g_p_i2c_task_sig = os_signal_create_static(&g_i2c_task_sig_mem);
    i2c_task_register_signals();
    i2c_task_create_timers();

    const uint32_t           stack_size    = 1024U * 3U;
    const os_task_priority_t task_priority = 5;
    os_task_handle_t         h_task        = NULL;
    if (!os_task_create_without_param(&i2c_task, "i2c_task", stack_size, task_priority, &h_task))
    {
        LOG_ERR("Can't create thread");
    }
    while (!os_signal_is_any_thread_registered(g_p_i2c_task_sig))
    {
        vTaskDelay(1);
    }

    return true;
}