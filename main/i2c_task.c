/**
 * @file i2c_task.c
 * @author TheSomeMan
 * @date 2023-07-03
 * @copyright Ruuvi Innovations Ltd, license BSD-3-Clause.
 */

#include "i2c_task.h"
#include <string.h>
#include <esp_task_wdt.h>
#include "os_malloc.h"
#include "os_signal.h"
#include "os_timer_sig.h"
#include "os_mutex.h"
#include "driver/i2c.h"
#include "sen5x_wrap.h"
#include "scd4x_wrap.h"
#include "ruuvi_endpoint_6.h"
#include "ruuvi_endpoint_ca_uart.h"
#include "mac_addr.h"
#include "gw_cfg.h"
#include "adv_post.h"
#include "ble_adv.h"
#define LOG_LOCAL_LEVEL LOG_LEVEL_INFO
#include "log.h"

#define I2C_MASTER_FREQ_HZ 100000
#define I2C_TASK_I2C_NUM   (I2C_NUM_0)
#define I2C_TASK_I2C_SCL   (GPIO_NUM_32)
#define I2C_TASK_I2C_SDA   (GPIO_NUM_33)

#define I2C_TASK_SEN5X_POLL_PERIOD_MS                   (1000U)
#define I2C_TASK_SEN5X_CHECK_PERIOD_MS                  (100U)
#define I2C_TASK_SEN5X_CHECK_MAX_ERR_CNT                (10U)
#define I2C_TASK_SCD4X_POLL_PERIOD_MS                   (5000U)
#define I2C_TASK_SCD4X_CHECK_PERIOD_MS                  (100U)
#define I2C_TASK_SCD4X_CHECK_MAX_ERR_CNT                (10U)
#define I2C_TASK_SCD4X_SEND_BLE_DEVICE_NAME_PERIOD_MS   (3011U)
#define I2C_TASK_SCD4X_SEND_BLE_DEVICE_NAME_DURATION_MS (100U)

#define I2C_TASK_WATCHDOG_FEEDING_PERIOD_TICKS pdMS_TO_TICKS(1000)

#define I2C_TASK_BITS_PER_BYTE (8U)

typedef enum i2c_task_sig_e
{
    I2C_TASK_SIG_POLL_SEN5X                 = OS_SIGNAL_NUM_0,
    I2C_TASK_SIG_POLL_SCD4X                 = OS_SIGNAL_NUM_1,
    I2C_TASK_SIG_SEND_BLE_DEVICE_NAME_START = OS_SIGNAL_NUM_2,
    I2C_TASK_SIG_SEND_BLE_DEVICE_NAME_STOP  = OS_SIGNAL_NUM_3,
    I2C_TASK_SIG_TASK_WATCHDOG_FEED         = OS_SIGNAL_NUM_4,
} i2c_task_sig_e;

#define I2C_TASK_SIG_FIRST (I2C_TASK_SIG_POLL_SEN5X)
#define I2C_TASK_SIG_LAST  (I2C_TASK_SIG_TASK_WATCHDOG_FEED)

typedef struct i2c_task_t
{
    bool                           flag_sen5x_present;
    bool                           flag_scd4x_present;
    mac_address_bin_t              mac_addr_bin;
    uint64_t                       mac_addr;
    os_mutex_t                     p_mutex;
    os_mutex_static_t              mutex_mem;
    os_signal_t*                   p_signal;
    os_signal_static_t             signal_mem;
    os_timer_sig_one_shot_t*       p_timer_sig_poll_sen5x;
    os_timer_sig_one_shot_static_t timer_sig_poll_sen5x_mem;
    os_timer_sig_one_shot_t*       p_timer_sig_poll_scd4x;
    os_timer_sig_one_shot_static_t timer_sig_poll_scd4x_mem;
    os_timer_sig_periodic_t*       p_timer_sig_send_ble_device_name_start;
    os_timer_sig_periodic_static_t timer_sig_send_ble_device_name_start_mem;
    os_timer_sig_one_shot_t*       p_timer_sig_send_ble_device_name_stop;
    os_timer_sig_one_shot_static_t timer_sig_send_ble_device_name_stop_mem;
    os_timer_sig_periodic_t*       p_timer_sig_watchdog_feed;
    os_timer_sig_periodic_static_t timer_sig_watchdog_feed_mem;
    sen5x_wrap_measurement_t       measurement_sen5x;
    scd4x_wrap_measurement_t       measurement_scd4x;
    uint16_t                       measurement_count;
    uint16_t                       err_cnt_sen5x;
    uint16_t                       err_cnt_scd4x;
} i2c_task_t;

static const char* TAG = "I2C_TASK";

static i2c_task_t* g_p_i2c_task;

static const uint8_t g_i2c_task_ble_packet_prefix[RE_6_OFFSET_PAYLOAD] = { 0x02, 0x01, 0x06, 0x1B, 0xFF, 0x99, 0x04 };

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
    vTaskDelay(pdMS_TO_TICKS(10));

    const int state1 = gpio_get_level(gpio_pin);

    io_conf.pull_up_en = GPIO_PULLUP_ENABLE;
    gpio_config(&io_conf);
    vTaskDelay(pdMS_TO_TICKS(10));

    const int state2 = gpio_get_level(gpio_pin);

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
    i2c_config_t conf = {
        .mode             = I2C_MODE_MASTER,
        .sda_io_num       = I2C_TASK_I2C_SDA,
        .sda_pullup_en    = GPIO_PULLUP_ENABLE,
        .scl_io_num       = I2C_TASK_I2C_SCL,
        .scl_pullup_en    = GPIO_PULLUP_ENABLE,
        .master.clk_speed = I2C_MASTER_FREQ_HZ,
    };
    i2c_param_config(I2C_TASK_I2C_NUM, &conf);
    const esp_err_t err = i2c_driver_install(I2C_TASK_I2C_NUM, conf.mode, 0, 0, 0);
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
    os_signal_add(g_p_i2c_task->p_signal, i2c_task_conv_to_sig_num(I2C_TASK_SIG_POLL_SEN5X));
    os_signal_add(g_p_i2c_task->p_signal, i2c_task_conv_to_sig_num(I2C_TASK_SIG_POLL_SCD4X));
    os_signal_add(g_p_i2c_task->p_signal, i2c_task_conv_to_sig_num(I2C_TASK_SIG_SEND_BLE_DEVICE_NAME_START));
    os_signal_add(g_p_i2c_task->p_signal, i2c_task_conv_to_sig_num(I2C_TASK_SIG_SEND_BLE_DEVICE_NAME_STOP));
    os_signal_add(g_p_i2c_task->p_signal, i2c_task_conv_to_sig_num(I2C_TASK_SIG_TASK_WATCHDOG_FEED));
}

static void
i2c_task_create_timers(void)
{
    if (g_p_i2c_task->flag_sen5x_present)
    {
        g_p_i2c_task->p_timer_sig_poll_sen5x = os_timer_sig_one_shot_create_static(
            &g_p_i2c_task->timer_sig_poll_sen5x_mem,
            "i2c_task:sen5x",
            g_p_i2c_task->p_signal,
            i2c_task_conv_to_sig_num(I2C_TASK_SIG_POLL_SEN5X),
            pdMS_TO_TICKS(I2C_TASK_SIG_POLL_SEN5X));
    }

    if (g_p_i2c_task->flag_scd4x_present)
    {
        g_p_i2c_task->p_timer_sig_poll_scd4x = os_timer_sig_one_shot_create_static(
            &g_p_i2c_task->timer_sig_poll_scd4x_mem,
            "i2c_task:scd4x",
            g_p_i2c_task->p_signal,
            i2c_task_conv_to_sig_num(I2C_TASK_SIG_POLL_SCD4X),
            pdMS_TO_TICKS(I2C_TASK_SCD4X_POLL_PERIOD_MS));
    }

    g_p_i2c_task->p_timer_sig_send_ble_device_name_start = os_timer_sig_periodic_create_static(
        &g_p_i2c_task->timer_sig_send_ble_device_name_start_mem,
        "i2c_task:bt_dev_name:1",
        g_p_i2c_task->p_signal,
        i2c_task_conv_to_sig_num(I2C_TASK_SIG_SEND_BLE_DEVICE_NAME_START),
        pdMS_TO_TICKS(I2C_TASK_SCD4X_SEND_BLE_DEVICE_NAME_PERIOD_MS));

    g_p_i2c_task->p_timer_sig_send_ble_device_name_stop = os_timer_sig_one_shot_create_static(
        &g_p_i2c_task->timer_sig_send_ble_device_name_stop_mem,
        "i2c_task:bt_dev_name:2",
        g_p_i2c_task->p_signal,
        i2c_task_conv_to_sig_num(I2C_TASK_SIG_SEND_BLE_DEVICE_NAME_STOP),
        pdMS_TO_TICKS(I2C_TASK_SCD4X_SEND_BLE_DEVICE_NAME_DURATION_MS));

    g_p_i2c_task->p_timer_sig_watchdog_feed = os_timer_sig_periodic_create_static(
        &g_p_i2c_task->timer_sig_watchdog_feed_mem,
        "i2c_task:wdog",
        g_p_i2c_task->p_signal,
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
    os_timer_sig_periodic_start(g_p_i2c_task->p_timer_sig_watchdog_feed);
}

static re_6_data_t
i2c_task_get_re6_data(void)
{
    const sen5x_wrap_measurement_t* const p_sen5x = &g_p_i2c_task->measurement_sen5x;
    const scd4x_wrap_measurement_t* const p_scd4x = &g_p_i2c_task->measurement_scd4x;

    os_mutex_lock(g_p_i2c_task->p_mutex);

    const re_6_data_t ep_data = {
        .pm1p0_ppm         = sen5x_wrap_conv_raw_to_float_pm(p_sen5x->mass_concentration_pm1p0),
        .pm2p5_ppm         = sen5x_wrap_conv_raw_to_float_pm(p_sen5x->mass_concentration_pm2p5),
        .pm4p0_ppm         = sen5x_wrap_conv_raw_to_float_pm(p_sen5x->mass_concentration_pm4p0),
        .pm10p0_ppm        = sen5x_wrap_conv_raw_to_float_pm(p_sen5x->mass_concentration_pm10p0),
        .co2               = scd4x_wrap_conv_raw_to_float_co2(p_scd4x->co2),
        .humidity_rh       = sen5x_wrap_conv_raw_to_float_humidity(p_sen5x->ambient_humidity),
        .voc_index         = sen5x_wrap_conv_raw_to_float_voc_index(p_sen5x->voc_index),
        .nox_index         = sen5x_wrap_conv_raw_to_float_nox_index(p_sen5x->nox_index),
        .temperature_c     = sen5x_wrap_conv_raw_to_float_temperature(p_sen5x->ambient_temperature),
        .measurement_count = g_p_i2c_task->measurement_count,
        .address           = g_p_i2c_task->mac_addr,
    };

    os_mutex_unlock(g_p_i2c_task->p_mutex);

    return ep_data;
}

bool
i2c_task_get_ble_adv_packet(i2c_task_ble_adv_packet_t* const p_packet)
{
    if (NULL == g_p_i2c_task)
    {
        return false;
    }

    const re_6_data_t ep_data = i2c_task_get_re6_data();

    LOG_DBG(
        "PM=%.1f,%.1f,%.1f,%.1f µg/m³, H=%.2f %%RH, T=%.3f °C, VOC=%.1f, NOX=%.1f, CO2=%.0f, cnt=%u",
        ep_data.pm1p0_ppm,
        ep_data.pm2p5_ppm,
        ep_data.pm4p0_ppm,
        ep_data.pm10p0_ppm,
        ep_data.humidity_rh,
        ep_data.temperature_c,
        ep_data.voc_index,
        ep_data.nox_index,
        ep_data.co2,
        (printf_uint_t)ep_data.measurement_count);

    uint8_t payload_buf[RE_6_DATA_LENGTH];
    re_6_encode(payload_buf, &ep_data);

    memcpy(&p_packet->buf[0], g_i2c_task_ble_packet_prefix, sizeof(g_i2c_task_ble_packet_prefix));
    memcpy(&p_packet->buf[RE_6_OFFSET_PAYLOAD], payload_buf, sizeof(payload_buf));
    p_packet->len = RE_6_OFFSET_PAYLOAD + sizeof(payload_buf);

    return true;
}

static void
i2c_task_send_data(void)
{
    i2c_task_ble_adv_packet_t adv_packet = { 0 };
    i2c_task_get_ble_adv_packet(&adv_packet);

    if (++g_p_i2c_task->measurement_count == (UINT16_MAX - 1))
    {
        g_p_i2c_task->measurement_count = 0;
    }

    re_ca_uart_payload_t payload = {
        .cmd = RE_CA_UART_ADV_RPRT,
        .params = {
            .adv = {
                .mac = {0},
                .adv = {0},
                .adv_len = adv_packet.len,
                .rssi_db = -1,
            },
        },
    };

    memcpy(&payload.params.adv.mac, g_p_i2c_task->mac_addr_bin.mac, sizeof(payload.params.adv.mac));
    memcpy(&payload.params.adv.adv[0], adv_packet.buf, adv_packet.len);

    adv_post_send_payload(&payload);
}

static void
i2c_task_handle_sig_poll_sen5x(void)
{
    sen5x_wrap_measurement_t measurement = {
        .mass_concentration_pm1p0  = SEN5X_INVALID_RAW_VALUE_PM,
        .mass_concentration_pm2p5  = SEN5X_INVALID_RAW_VALUE_PM,
        .mass_concentration_pm4p0  = SEN5X_INVALID_RAW_VALUE_PM,
        .mass_concentration_pm10p0 = SEN5X_INVALID_RAW_VALUE_PM,
        .ambient_humidity          = SEN5X_INVALID_RAW_VALUE_HUMIDITY,
        .ambient_temperature       = SEN5X_INVALID_RAW_VALUE_TEMPERATURE,
        .voc_index                 = SEN5X_INVALID_RAW_VALUE_VOC,
        .nox_index                 = SEN5X_INVALID_RAW_VALUE_NOX,
    };
    if (!sen5x_wrap_read_measured_values(&measurement))
    {
        if (++g_p_i2c_task->err_cnt_sen5x < I2C_TASK_SEN5X_CHECK_MAX_ERR_CNT)
        {
            os_timer_sig_one_shot_restart(
                g_p_i2c_task->p_timer_sig_poll_sen5x,
                pdMS_TO_TICKS(I2C_TASK_SEN5X_CHECK_PERIOD_MS));
            return;
        }
        g_p_i2c_task->err_cnt_sen5x = I2C_TASK_SEN5X_CHECK_MAX_ERR_CNT;
        if (sen5x_wrap_check())
        {
            if (!sen5x_wrap_start_measurement())
            {
                LOG_ERR("%s failed", "sen5x_wrap_start_measurement");
            }
        }
        else
        {
            LOG_ERR("%s failed", "sen5x_wrap_check");
        }
        os_timer_sig_one_shot_restart(
            g_p_i2c_task->p_timer_sig_poll_sen5x,
            pdMS_TO_TICKS(I2C_TASK_SEN5X_POLL_PERIOD_MS * 2));
    }
    else
    {
        g_p_i2c_task->err_cnt_sen5x = 0;

        os_timer_sig_one_shot_restart(
            g_p_i2c_task->p_timer_sig_poll_sen5x,
            pdMS_TO_TICKS(I2C_TASK_SEN5X_POLL_PERIOD_MS - I2C_TASK_SEN5X_CHECK_PERIOD_MS));
    }

    os_mutex_lock(g_p_i2c_task->p_mutex);
    g_p_i2c_task->measurement_sen5x = measurement;
    os_mutex_unlock(g_p_i2c_task->p_mutex);

    i2c_task_send_data();
}

static void
i2c_task_handle_sig_poll_scd4x(void)
{
    scd4x_wrap_measurement_t measurement = {
        .co2                   = SCD4X_INVALID_RAW_VALUE_CO2,
        .temperature_m_deg_c   = SCD4X_INVALID_RAW_VALUE_TEMPERATURE,
        .humidity_m_percent_rh = SCD4X_INVALID_RAW_VALUE_HUMIDITY,
    };
    if (!scd4x_wrap_read_measurement(&measurement))
    {
        LOG_ERR("%s failed", "scd4x_wrap_read_measurement");
        if (++g_p_i2c_task->err_cnt_scd4x < I2C_TASK_SCD4X_CHECK_MAX_ERR_CNT)
        {
            os_timer_sig_one_shot_restart(
                g_p_i2c_task->p_timer_sig_poll_scd4x,
                pdMS_TO_TICKS(I2C_TASK_SCD4X_CHECK_PERIOD_MS));
            return;
        }
        g_p_i2c_task->err_cnt_scd4x = I2C_TASK_SCD4X_CHECK_MAX_ERR_CNT;
        if (scd4x_wrap_check())
        {
            if (!scd4x_wrap_start_periodic_measurement())
            {
                LOG_ERR("%s failed", "sen5x_wrap_start_measurement");
            }
        }
        else
        {
            LOG_ERR("%s failed", "scd4x_wrap_check");
        }
        os_timer_sig_one_shot_restart(
            g_p_i2c_task->p_timer_sig_poll_scd4x,
            pdMS_TO_TICKS(I2C_TASK_SCD4X_POLL_PERIOD_MS * 2));
    }
    else
    {
        g_p_i2c_task->err_cnt_scd4x = 0;

        os_timer_sig_one_shot_restart(
            g_p_i2c_task->p_timer_sig_poll_scd4x,
            pdMS_TO_TICKS(I2C_TASK_SCD4X_POLL_PERIOD_MS - I2C_TASK_SCD4X_CHECK_PERIOD_MS));
    }

    os_mutex_lock(g_p_i2c_task->p_mutex);
    g_p_i2c_task->measurement_scd4x = measurement;
    os_mutex_unlock(g_p_i2c_task->p_mutex);

    i2c_task_send_data();
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
i2c_task_handle_sig_send_ble_device_name_start(void)
{
    os_timer_sig_one_shot_start(g_p_i2c_task->p_timer_sig_send_ble_device_name_stop);
    ble_adv_send_device_name();
}

static void
i2c_task_handle_sig_send_ble_device_name_stop(void)
{
    i2c_task_ble_adv_packet_t packet = { 0 };
    if (!i2c_task_get_ble_adv_packet(&packet))
    {
        return;
    }
    ble_adv_send(packet.buf);
}

static void
i2c_task_handle_sig(const i2c_task_sig_e i2c_task_sig)
{
    switch (i2c_task_sig)
    {
        case I2C_TASK_SIG_POLL_SEN5X:
            LOG_DBG("I2C_TASK_SIG_POLL_SEN5X");
            i2c_task_handle_sig_poll_sen5x();
            break;
        case I2C_TASK_SIG_POLL_SCD4X:
            LOG_DBG("I2C_TASK_SIG_POLL_SCD4X");
            i2c_task_handle_sig_poll_scd4x();
            break;
        case I2C_TASK_SIG_SEND_BLE_DEVICE_NAME_START:
            LOG_DBG("I2C_TASK_SIG_SEND_BLE_DEVICE_NAME_START");
            i2c_task_handle_sig_send_ble_device_name_start();
            break;
        case I2C_TASK_SIG_SEND_BLE_DEVICE_NAME_STOP:
            LOG_DBG("I2C_TASK_SIG_SEND_BLE_DEVICE_NAME_STOP");
            i2c_task_handle_sig_send_ble_device_name_stop();
            break;
        case I2C_TASK_SIG_TASK_WATCHDOG_FEED:
            LOG_DBG("I2C_TASK_SIG_TASK_WATCHDOG_FEED");
            i2c_task_handle_sig_task_watchdog_feed();
            break;
    }
}

ATTR_NORETURN
static void
i2c_task(void)
{
    if (!os_signal_register_cur_thread(g_p_i2c_task->p_signal))
    {
        LOG_ERR("%s failed", "os_signal_register_cur_thread");
    }

    i2c_task_wdt_add_and_start();
    if (g_p_i2c_task->flag_sen5x_present)
    {
        os_timer_sig_one_shot_start(g_p_i2c_task->p_timer_sig_poll_sen5x);
    }
    if (g_p_i2c_task->flag_scd4x_present)
    {
        os_timer_sig_one_shot_start(g_p_i2c_task->p_timer_sig_poll_scd4x);
    }
    os_timer_sig_periodic_start(g_p_i2c_task->p_timer_sig_send_ble_device_name_start);

    LOG_INFO("%s started", __func__);

    for (;;)
    {
        os_signal_events_t sig_events = { 0 };
        if (!os_signal_wait_with_timeout(g_p_i2c_task->p_signal, OS_DELTA_TICKS_INFINITE, &sig_events))
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

    i2c_task_t* p_i2c_task = os_calloc(1, sizeof(*p_i2c_task));
    if (NULL == p_i2c_task)
    {
        LOG_ERR("Can't allocate memory for i2c_task");
        return false;
    }

    const mac_address_str_t* p_mac_addr_str = gw_cfg_get_esp32_mac_addr_bluetooth();
    mac_addr_from_str(p_mac_addr_str->str_buf, &p_i2c_task->mac_addr_bin);
    p_i2c_task->mac_addr = 0;
    for (uint32_t i = 0; i < sizeof(p_i2c_task->mac_addr_bin.mac); ++i)
    {
        p_i2c_task->mac_addr <<= I2C_TASK_BITS_PER_BYTE;
        p_i2c_task->mac_addr |= p_i2c_task->mac_addr_bin.mac[i];
    }

    p_i2c_task->flag_sen5x_present = sen5x_wrap_check();
    p_i2c_task->flag_scd4x_present = scd4x_wrap_check();
    if (p_i2c_task->flag_sen5x_present)
    {
        if (!sen5x_wrap_start_measurement())
        {
            LOG_ERR("%s failed", "sen5x_wrap_start_measurement");
            p_i2c_task->flag_sen5x_present = false;
        }
    }
    if (p_i2c_task->flag_scd4x_present)
    {
        if (!scd4x_wrap_start_periodic_measurement())
        {
            LOG_ERR("%s failed", "scd4x_wrap_start_periodic_measurement");
            p_i2c_task->flag_scd4x_present = false;
        }
    }

    if (!p_i2c_task->flag_sen5x_present && !p_i2c_task->flag_scd4x_present)
    {
        os_free(p_i2c_task);
        return false;
    }

    p_i2c_task->measurement_count = 0;
    p_i2c_task->err_cnt_sen5x     = 0;
    p_i2c_task->err_cnt_scd4x     = 0;

    p_i2c_task->measurement_sen5x.mass_concentration_pm1p0  = SEN5X_INVALID_RAW_VALUE_PM;
    p_i2c_task->measurement_sen5x.mass_concentration_pm2p5  = SEN5X_INVALID_RAW_VALUE_PM;
    p_i2c_task->measurement_sen5x.mass_concentration_pm4p0  = SEN5X_INVALID_RAW_VALUE_PM;
    p_i2c_task->measurement_sen5x.mass_concentration_pm10p0 = SEN5X_INVALID_RAW_VALUE_PM;
    p_i2c_task->measurement_sen5x.ambient_humidity          = SEN5X_INVALID_RAW_VALUE_HUMIDITY;
    p_i2c_task->measurement_sen5x.ambient_temperature       = SEN5X_INVALID_RAW_VALUE_TEMPERATURE;
    p_i2c_task->measurement_sen5x.voc_index                 = SEN5X_INVALID_RAW_VALUE_VOC;
    p_i2c_task->measurement_sen5x.nox_index                 = SEN5X_INVALID_RAW_VALUE_NOX;

    p_i2c_task->measurement_scd4x.co2                   = SCD4X_INVALID_RAW_VALUE_CO2;
    p_i2c_task->measurement_scd4x.temperature_m_deg_c   = SCD4X_INVALID_RAW_VALUE_TEMPERATURE;
    p_i2c_task->measurement_scd4x.humidity_m_percent_rh = SCD4X_INVALID_RAW_VALUE_HUMIDITY;

    p_i2c_task->p_mutex  = os_mutex_create_static(&p_i2c_task->mutex_mem);
    p_i2c_task->p_signal = os_signal_create_static(&p_i2c_task->signal_mem);

    g_p_i2c_task = p_i2c_task;

    i2c_task_register_signals();
    i2c_task_create_timers();

    const uint32_t           stack_size    = 1024U * 3U;
    const os_task_priority_t task_priority = 5;
    os_task_handle_t         h_task        = NULL;
    if (!os_task_create_without_param(&i2c_task, "i2c_task", stack_size, task_priority, &h_task))
    {
        LOG_ERR("Can't create thread");
    }
    while (!os_signal_is_any_thread_registered(g_p_i2c_task->p_signal))
    {
        vTaskDelay(1);
    }

    return true;
}
