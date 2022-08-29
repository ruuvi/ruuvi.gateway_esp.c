/**
 * @file leds.c
 * @author Jukka Saari
 * @date 2019-11-27
 * @copyright Ruuvi Innovations Ltd, license BSD-3-Clause.
 */

#include "leds.h"
#include <stdbool.h>
#include <stdio.h>
#include <assert.h>
#include "driver/ledc.h"
#include "driver/gpio.h"
#include <esp_task_wdt.h>
#include "os_task.h"
#include "os_mutex.h"
#include "os_signal.h"
#include "os_timer_sig.h"
#include "attribs.h"
#include "time_units.h"
#include "ruuvi_board_gwesp.h"
#include "gpio_switch_ctrl.h"
#include "esp_type_wrapper.h"

#define LOG_LOCAL_LEVEL LOG_LEVEL_INFO
#include "log.h"

static const char *TAG = "LEDS";

#define LED_PIN (RB_ESP32_GPIO_MUX_LED)

#define LEDC_HS_TIMER       LEDC_TIMER_1
#define LEDC_HS_MODE        LEDC_HIGH_SPEED_MODE
#define LEDC_HS_CH0_GPIO    LED_PIN
#define LEDC_HS_CH0_CHANNEL LEDC_CHANNEL_0

#define LEDC_TEST_DUTY_OFF  (1023 - 0 /* 0% */)
#define LEDC_TEST_DUTY_ON   (1023 - 256 /* 256 / 1024 = 25% */)
#define LEDC_TEST_FADE_TIME (25)

#define LEDS_DEFAULT_TIMER_PERIOD_MS (1000U)
#define LEDS_DUTY_CYCLE_PERCENT_0    (0U)
#define LEDS_DUTY_CYCLE_PERCENT_100  (100U)

#define LEDS_TASK_PRIORITY (6)

#define LEDS_BLINKING_ON_CONFIGURE_BUTTON_PRESS_PERIOD     (500U /* 2 Hz */)
#define LEDS_BLINKING_ON_CONFIGURE_BUTTON_PRESS_DUTY_CYCLE (50U)

#define LEDS_BLINKING_ON_HOTSPOT_ACTIVATION_PERIOD     (2000U /* 0.5 Hz */)
#define LEDS_BLINKING_ON_HOTSPOT_ACTIVATION_DUTY_CYCLE (50U)

#define LEDS_BLINKING_ON_NETWORK_PROBLEM_PERIOD     (200U /* 5 Hz */)
#define LEDS_BLINKING_ON_NETWORK_PROBLEM_DUTY_CYCLE (50U)

#define LEDS_BLINKING_ON_NRF52_FW_UPDATING_PERIOD     (1000U /* 1 Hz */)
#define LEDS_BLINKING_ON_NRF52_FW_UPDATING_DUTY_CYCLE (5U)

typedef enum leds_task_sig_e
{
    LEDS_TASK_SIG_TURN_ON            = OS_SIGNAL_NUM_0,
    LEDS_TASK_SIG_TURN_OFF           = OS_SIGNAL_NUM_1,
    LEDS_TASK_SIG_TASK_WATCHDOG_FEED = OS_SIGNAL_NUM_2,
} leds_task_sig_e;

#define LEDS_TASK_SIG_FIRST (LEDS_TASK_SIG_TURN_ON)
#define LEDS_TASK_SIG_LAST  (LEDS_TASK_SIG_TASK_WATCHDOG_FEED)

static ledc_channel_config_t ledc_channel[1] = {
    {
        .gpio_num   = LEDC_HS_CH0_GPIO,
        .speed_mode = LEDC_HS_MODE,
        .channel    = LEDC_HS_CH0_CHANNEL,
        .intr_type  = LEDC_INTR_DISABLE,
        .timer_sel  = LEDC_HS_TIMER,
        .duty       = 0,
        .hpoint     = 0,
    },
};

static os_mutex_t                     g_p_leds_mutex;
static os_mutex_static_t              g_leds_mutex_mem;
static os_signal_t *                  g_p_leds_signal;
static os_signal_static_t             g_leds_signal_mem;
static os_timer_sig_periodic_t *      g_p_leds_timer_sig_turn_on;
static os_timer_sig_periodic_static_t g_leds_timer_sig_turn_on_mem;
static os_timer_sig_one_shot_t *      g_p_leds_timer_sig_turn_off;
static os_timer_sig_one_shot_static_t g_leds_timer_sig_turn_off_mem;
static os_timer_sig_periodic_t *      g_p_leds_timer_sig_watchdog_feed;
static os_timer_sig_periodic_static_t g_leds_timer_sig_watchdog_feed_mem;
static TimeUnitsMilliSeconds_t        g_leds_period_ms;
static uint32_t                       g_leds_duty_cycle_percent;
static bool                           g_leds_gpio_switch_ctrl_activated;

ATTR_PURE
static os_signal_num_e
leds_task_conv_to_sig_num(const leds_task_sig_e sig)
{
    return (os_signal_num_e)sig;
}

static leds_task_sig_e
leds_task_conv_from_sig_num(const os_signal_num_e sig_num)
{
    assert(((os_signal_num_e)LEDS_TASK_SIG_FIRST <= sig_num) && (sig_num <= (os_signal_num_e)LEDS_TASK_SIG_LAST));
    return (leds_task_sig_e)sig_num;
}

static void
leds_timer_sig_turn_on_start(const TimeUnitsMilliSeconds_t period_ms)
{
    const os_delta_ticks_t period_ticks = pdMS_TO_TICKS(period_ms);
    LOG_DBG("Start timer:ON: period: %u ms (%u ticks)", (printf_uint_t)period_ms, (printf_uint_t)period_ticks);
    os_timer_sig_periodic_restart(g_p_leds_timer_sig_turn_on, period_ticks);
    os_signal_send(g_p_leds_signal, leds_task_conv_to_sig_num(LEDS_TASK_SIG_TURN_ON));
}

static void
leds_timer_sig_turn_on_stop(void)
{
    LOG_DBG("Stop timer:ON");
    os_timer_sig_periodic_stop(g_p_leds_timer_sig_turn_on);
}

static void
leds_timer_sig_turn_off_start(const TimeUnitsMilliSeconds_t period_ms, const uint32_t duty_cycle_percent)
{
    const TimeUnitsMilliSeconds_t delay_ms    = (period_ms * duty_cycle_percent) / LEDS_DUTY_CYCLE_PERCENT_100;
    const os_delta_ticks_t        delay_ticks = pdMS_TO_TICKS(delay_ms);
    LOG_DBG("Start timer:OFF: delay: %u ms (%u ticks)", (printf_uint_t)delay_ms, (printf_uint_t)delay_ticks);
    os_timer_sig_one_shot_restart(g_p_leds_timer_sig_turn_off, delay_ticks);
}

static void
leds_timer_sig_turn_off_stop(void)
{
    LOG_DBG("Stop timer:OFF");
    os_timer_sig_one_shot_stop(g_p_leds_timer_sig_turn_off);
}

void
leds_on(void)
{
    LOG_INFO("LED: ON");
    assert(NULL != g_p_leds_mutex);
    os_mutex_lock(g_p_leds_mutex);
    leds_timer_sig_turn_on_stop();
    leds_timer_sig_turn_off_stop();
    g_leds_period_ms          = LEDS_DEFAULT_TIMER_PERIOD_MS;
    g_leds_duty_cycle_percent = LEDS_DUTY_CYCLE_PERCENT_100;
    os_signal_send(g_p_leds_signal, leds_task_conv_to_sig_num(LEDS_TASK_SIG_TURN_ON));
    os_mutex_unlock(g_p_leds_mutex);
}

void
leds_off(void)
{
    LOG_INFO("LED: OFF");
    assert(NULL != g_p_leds_mutex);
    os_mutex_lock(g_p_leds_mutex);
    leds_timer_sig_turn_on_stop();
    leds_timer_sig_turn_off_stop();
    g_leds_period_ms          = LEDS_DEFAULT_TIMER_PERIOD_MS;
    g_leds_duty_cycle_percent = LEDS_DUTY_CYCLE_PERCENT_0;
    os_signal_send(g_p_leds_signal, leds_task_conv_to_sig_num(LEDS_TASK_SIG_TURN_OFF));
    os_mutex_unlock(g_p_leds_mutex);
}

LEDS_STATIC
void
leds_start_blink(const TimeUnitsMilliSeconds_t interval_ms, const uint32_t duty_cycle_percent)
{
    LOG_INFO(
        "LED: Start blinking, interval: %u ms, duty cycle: %u%%",
        (printf_uint_t)interval_ms,
        (printf_uint_t)duty_cycle_percent);
    assert(NULL != g_p_leds_mutex);
    os_mutex_lock(g_p_leds_mutex);
    leds_timer_sig_turn_on_stop();
    leds_timer_sig_turn_off_stop();

    if (0 == interval_ms)
    {
        g_leds_period_ms          = LEDS_DEFAULT_TIMER_PERIOD_MS;
        g_leds_duty_cycle_percent = LEDS_DUTY_CYCLE_PERCENT_100;
        os_signal_send(g_p_leds_signal, leds_task_conv_to_sig_num(LEDS_TASK_SIG_TURN_ON));
    }
    else
    {
        g_leds_period_ms          = interval_ms;
        g_leds_duty_cycle_percent = duty_cycle_percent;
        leds_timer_sig_turn_on_start(interval_ms);
    }

    os_mutex_unlock(g_p_leds_mutex);
}

static void
leds_task_watchdog_feed(void)
{
    LOG_DBG("Feed watchdog");
    const esp_err_t err = esp_task_wdt_reset();
    if (ESP_OK != err)
    {
        LOG_ERR_ESP(err, "%s failed", "esp_task_wdt_reset");
    }
}

static void
leds_task_handle_sig(
    const leds_task_sig_e         leds_task_sig,
    const TimeUnitsMilliSeconds_t period_ms,
    const uint32_t                duty_cycle_percent)
{
    switch (leds_task_sig)
    {
        case LEDS_TASK_SIG_TURN_ON:
            LOG_DBG(
                "SIG_TURN_ON: period %u ms, duty_cycle %u%%",
                (printf_uint_t)period_ms,
                (printf_uint_t)duty_cycle_percent);
            if (LEDS_DUTY_CYCLE_PERCENT_0 == duty_cycle_percent)
            {
                leds_off();
                break;
            }
            if (!g_leds_gpio_switch_ctrl_activated)
            {
                gpio_switch_ctrl_activate();
                g_leds_gpio_switch_ctrl_activated = true;
            }
            ledc_set_fade_with_time(
                ledc_channel[0].speed_mode,
                ledc_channel[0].channel,
                LEDC_TEST_DUTY_ON,
                LEDC_TEST_FADE_TIME);
            ledc_fade_start(ledc_channel[0].speed_mode, ledc_channel[0].channel, LEDC_FADE_NO_WAIT);

            if (LEDS_DUTY_CYCLE_PERCENT_100 != duty_cycle_percent)
            {
                leds_timer_sig_turn_off_start(period_ms, duty_cycle_percent);
            }
            break;

        case LEDS_TASK_SIG_TURN_OFF:
            LOG_DBG("SIG_TURN_OFF");
            ledc_set_fade_with_time(
                ledc_channel[0].speed_mode,
                ledc_channel[0].channel,
                LEDC_TEST_DUTY_OFF,
                LEDC_TEST_FADE_TIME);
            ledc_fade_start(ledc_channel[0].speed_mode, ledc_channel[0].channel, LEDC_FADE_WAIT_DONE);
            if (g_leds_gpio_switch_ctrl_activated)
            {
                gpio_switch_ctrl_deactivate();
                g_leds_gpio_switch_ctrl_activated = false;
            }
            break;

        case LEDS_TASK_SIG_TASK_WATCHDOG_FEED:
            leds_task_watchdog_feed();
            break;

        default:
            LOG_ERR("Unhanded sig: %d", (int)leds_task_sig);
            assert(0);
            break;
    }
}

static void
leds_wdt_add_and_start(void)
{
    LOG_INFO("TaskWatchdog: Register current thread");
    const esp_err_t err = esp_task_wdt_add(xTaskGetCurrentTaskHandle());
    if (ESP_OK != err)
    {
        LOG_ERR_ESP(err, "%s failed", "esp_task_wdt_add");
    }
    LOG_INFO("TaskWatchdog: Start timer");
    os_timer_sig_periodic_start(g_p_leds_timer_sig_watchdog_feed);
}

ATTR_NORETURN
static void
leds_task(void)
{
    LOG_INFO("%s started", __func__);

    os_signal_register_cur_thread(g_p_leds_signal);

    leds_wdt_add_and_start();

    for (;;)
    {
        os_signal_events_t sig_events = { 0 };
        os_signal_wait(g_p_leds_signal, &sig_events);
        for (;;)
        {
            const os_signal_num_e sig_num = os_signal_num_get_next(&sig_events);
            if (OS_SIGNAL_NUM_NONE == sig_num)
            {
                break;
            }
            const leds_task_sig_e time_task_sig = leds_task_conv_from_sig_num(sig_num);
            os_mutex_lock(g_p_leds_mutex);
            const TimeUnitsMilliSeconds_t period_ms          = g_leds_period_ms;
            const uint32_t                duty_cycle_percent = g_leds_duty_cycle_percent;
            os_mutex_unlock(g_p_leds_mutex);
            leds_task_handle_sig(time_task_sig, period_ms, duty_cycle_percent);
        }
    }
}

void
leds_init(void)
{
    LOG_INFO("%s", __func__);

    g_p_leds_mutex = os_mutex_create_static(&g_leds_mutex_mem);

    g_p_leds_signal = os_signal_create_static(&g_leds_signal_mem);
    os_signal_add(g_p_leds_signal, leds_task_conv_to_sig_num(LEDS_TASK_SIG_TURN_ON));
    os_signal_add(g_p_leds_signal, leds_task_conv_to_sig_num(LEDS_TASK_SIG_TURN_OFF));
    os_signal_add(g_p_leds_signal, leds_task_conv_to_sig_num(LEDS_TASK_SIG_TASK_WATCHDOG_FEED));

    g_p_leds_timer_sig_turn_on = os_timer_sig_periodic_create_static(
        &g_leds_timer_sig_turn_on_mem,
        "leds:on",
        g_p_leds_signal,
        leds_task_conv_to_sig_num(LEDS_TASK_SIG_TURN_ON),
        pdMS_TO_TICKS(LEDS_DEFAULT_TIMER_PERIOD_MS));

    g_p_leds_timer_sig_turn_off = os_timer_sig_one_shot_create_static(
        &g_leds_timer_sig_turn_off_mem,
        "leds:off",
        g_p_leds_signal,
        leds_task_conv_to_sig_num(LEDS_TASK_SIG_TURN_OFF),
        pdMS_TO_TICKS(LEDS_DEFAULT_TIMER_PERIOD_MS));

    LOG_INFO("TaskWatchdog: leds_task: Create timer");
    g_p_leds_timer_sig_watchdog_feed = os_timer_sig_periodic_create_static(
        &g_leds_timer_sig_watchdog_feed_mem,
        "leds:wdog",
        g_p_leds_signal,
        leds_task_conv_to_sig_num(LEDS_TASK_SIG_TASK_WATCHDOG_FEED),
        pdMS_TO_TICKS(CONFIG_ESP_TASK_WDT_TIMEOUT_S * 1000U / 3U));

    ledc_timer_config_t ledc_timer = {
        .duty_resolution = LEDC_TIMER_10_BIT, // resolution of PWM duty
        .freq_hz         = 5000,              // frequency of PWM signal
        .speed_mode      = LEDC_HS_MODE,      // timer mode
        .timer_num       = LEDC_HS_TIMER,     // timer index
        .clk_cfg         = LEDC_AUTO_CLK,     // Auto select the source clock
    };

    ledc_timer_config(&ledc_timer);
    ledc_channel_config(&ledc_channel[0]);
    ledc_fade_func_install(0);

    const uint32_t stack_size = 2U * 1024U;

    os_task_handle_t h_task = NULL;
    if (!os_task_create_without_param(&leds_task, "leds_task", stack_size, LEDS_TASK_PRIORITY, &h_task))
    {
        LOG_ERR("Can't create thread");
    }
}

void
leds_indication_on_configure_button_press(void)
{
    LOG_INFO("### %s", __func__);
    leds_start_blink(
        LEDS_BLINKING_ON_CONFIGURE_BUTTON_PRESS_PERIOD,
        LEDS_BLINKING_ON_CONFIGURE_BUTTON_PRESS_DUTY_CYCLE);
}

void
leds_indication_on_hotspot_activation(void)
{
    LOG_INFO("### %s", __func__);
    leds_start_blink(LEDS_BLINKING_ON_HOTSPOT_ACTIVATION_PERIOD, LEDS_BLINKING_ON_HOTSPOT_ACTIVATION_DUTY_CYCLE);
}

void
leds_indication_network_no_connection(void)
{
    LOG_INFO("### %s", __func__);
    leds_start_blink(LEDS_BLINKING_ON_NETWORK_PROBLEM_PERIOD, LEDS_BLINKING_ON_NETWORK_PROBLEM_DUTY_CYCLE);
}

void
leds_indication_on_network_ok(void)
{
    LOG_INFO("### %s", __func__);
    leds_off();
}

void
leds_indication_on_nrf52_fw_updating(void)
{
    LOG_INFO("### %s", __func__);
    leds_start_blink(LEDS_BLINKING_ON_NRF52_FW_UPDATING_PERIOD, LEDS_BLINKING_ON_NRF52_FW_UPDATING_DUTY_CYCLE);
}
