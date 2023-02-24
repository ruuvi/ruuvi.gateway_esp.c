/**
 * @file reset_task.c
 * @author TheSomeMan
 * @date 2021-03-31
 * @copyright Ruuvi Innovations Ltd, license BSD-3-Clause.
 */

#include "reset_task.h"
#include <esp_task_wdt.h>
#include "os_signal.h"
#include "os_timer_sig.h"
#include "ethernet.h"
#include "wifi_manager.h"
#include "leds.h"
#include "ruuvi_gateway.h"
#include "mqtt.h"
#include "event_mgr.h"
#include "reset_info.h"

#define LOG_LOCAL_LEVEL LOG_LEVEL_INFO
#include "log.h"

typedef enum reset_task_sig_e
{
    RESET_TASK_SIG_CONFIGURE_BUTTON_PRESSED   = OS_SIGNAL_NUM_0,
    RESET_TASK_SIG_CONFIGURE_BUTTON_RELEASED  = OS_SIGNAL_NUM_1,
    RESET_TASK_SIG_INCREMENT_UPTIME_COUNTER   = OS_SIGNAL_NUM_2,
    RESET_TASK_SIG_REBOOT_BY_CONFIGURE_BUTTON = OS_SIGNAL_NUM_3,
    RESET_TASK_SIG_REBOOT_BY_COMMAND          = OS_SIGNAL_NUM_4,
    RESET_TASK_SIG_TASK_WATCHDOG_FEED         = OS_SIGNAL_NUM_5,
    RESET_TASK_SIG_RESTART                    = OS_SIGNAL_NUM_6,
} reset_task_sig_e;

#define RESET_TASK_SIG_FIRST (RESET_TASK_SIG_CONFIGURE_BUTTON_PRESSED)
#define RESET_TASK_SIG_LAST  (RESET_TASK_SIG_RESTART)

#define RESET_TASK_TIMEOUT_AFTER_PRESSING_CONFIGURE_BUTTON (5)
#define RESET_TASK_TIMEOUT_AFTER_COMMAND                   (3)

static const char* TAG = "reset_task";

static os_task_handle_t               g_p_reset_task_handle;
static os_timer_sig_one_shot_t*       g_p_timer_sig_reset_by_configure_button;
static os_timer_sig_one_shot_static_t g_timer_sig_reset_by_configure_button_mem;
static os_timer_sig_one_shot_t*       g_p_timer_sig_reset_by_command;
static os_timer_sig_one_shot_static_t g_timer_sig_reset_by_command_mem;
static os_timer_sig_periodic_t*       g_p_timer_sig_uptime_counter;
static os_timer_sig_periodic_static_t g_timer_sig_uptime_counter_mem;
static os_timer_sig_periodic_t*       g_p_timer_sig_watchdog_feed;
static os_timer_sig_periodic_static_t g_timer_sig_watchdog_feed_mem;
static os_signal_t*                   g_p_reset_task_signal;
static os_signal_static_t             g_reset_task_signal_mem;
static event_mgr_ev_info_static_t     g_reset_task_ev_reboot_mem;

volatile uint32_t g_cnt_cfg_button_pressed;
volatile uint32_t g_uptime_counter;

ATTR_PURE
static os_signal_num_e
reset_task_conv_to_sig_num(const reset_task_sig_e sig)
{
    return (os_signal_num_e)sig;
}

static reset_task_sig_e
reset_task_conv_from_sig_num(const os_signal_num_e sig_num)
{
    assert(((os_signal_num_e)RESET_TASK_SIG_FIRST <= sig_num) && (sig_num <= (os_signal_num_e)RESET_TASK_SIG_LAST));
    return (reset_task_sig_e)sig_num;
}

void
reset_task_notify_configure_button_pressed(void)
{
    if (!gw_cfg_is_initialized())
    {
        return;
    }
    os_signal_send(g_p_reset_task_signal, reset_task_conv_to_sig_num(RESET_TASK_SIG_CONFIGURE_BUTTON_PRESSED));
}

void
reset_task_notify_configure_button_released(void)
{
    if (!gw_cfg_is_initialized())
    {
        return;
    }
    os_signal_send(g_p_reset_task_signal, reset_task_conv_to_sig_num(RESET_TASK_SIG_CONFIGURE_BUTTON_RELEASED));
}

static void
reset_task_watchdog_feed(void)
{
    LOG_DBG("Feed watchdog");
    const esp_err_t err = esp_task_wdt_reset();
    if (ESP_OK != err)
    {
        LOG_ERR_ESP(err, "%s failed", "esp_task_wdt_reset");
    }
}

static void
reset_task_handle_sig(const reset_task_sig_e reset_task_sig)
{
    switch (reset_task_sig)
    {
        case RESET_TASK_SIG_CONFIGURE_BUTTON_PRESSED:
            LOG_INFO(
                "The CONFIGURE button has been pressed - start timer for %u seconds",
                RESET_TASK_TIMEOUT_AFTER_PRESSING_CONFIGURE_BUTTON);
            g_cnt_cfg_button_pressed += 1;
            os_timer_sig_one_shot_start(g_p_timer_sig_reset_by_configure_button);
            break;
        case RESET_TASK_SIG_CONFIGURE_BUTTON_RELEASED:
            LOG_INFO("The CONFIGURE button has been released");
            os_timer_sig_one_shot_stop(g_p_timer_sig_reset_by_configure_button);
            if (mqtt_app_is_working())
            {
                LOG_INFO("MQTT is active - stop it");
                mqtt_app_stop();
            }
            else
            {
                LOG_INFO("MQTT is not active");
            }
            if (gw_cfg_get_eth_use_eth())
            {
                LOG_INFO("Disconnect from Ethernet");
                wifi_manager_disconnect_eth();
                ethernet_stop();
            }
            else
            {
                LOG_INFO("Disconnect from WiFi");
                wifi_manager_disconnect_wifi();
            }
            if (!wifi_manager_is_ap_active())
            {
                LOG_INFO("WiFi AP is not active - start WiFi AP");
                const bool flag_block_req_from_lan = true;
                wifi_manager_start_ap(flag_block_req_from_lan);
            }
            else
            {
                LOG_INFO("WiFi AP is already active");
            }
            break;
        case RESET_TASK_SIG_INCREMENT_UPTIME_COUNTER:
            g_uptime_counter += 1;
            break;
        case RESET_TASK_SIG_REBOOT_BY_CONFIGURE_BUTTON:
            // restart the Gateway,
            // on boot it will check if RB_BUTTON_RESET_PIN is pressed and erase the
            // settings in flash.
            gateway_restart("System restart is activated by the Configure button");
            break;
        case RESET_TASK_SIG_REBOOT_BY_COMMAND:
            gateway_restart("System restart is activated by the Command");
            break;
        case RESET_TASK_SIG_TASK_WATCHDOG_FEED:
            reset_task_watchdog_feed();
            break;
        case RESET_TASK_SIG_RESTART:
            LOG_WARN("RESET_TASK_SIG_RESTART");
            // leds_task can be blocked for 25 ms when the red LED is on/off,
            // so we need to wait some time to give leds_task time
            // to complete the current operation and turn off the green and red LEDs
            vTaskDelay(pdMS_TO_TICKS(100));
            esp_restart();
            break;
        default:
            LOG_ERR("Unhanded sig: %d", (int)reset_task_sig);
            assert(0);
            break;
    }
}

static void
reset_task_wdt_add_and_start(void)
{
    LOG_INFO("TaskWatchdog: Register current thread");
    const esp_err_t err = esp_task_wdt_add(xTaskGetCurrentTaskHandle());
    if (ESP_OK != err)
    {
        LOG_ERR_ESP(err, "%s failed", "esp_task_wdt_add");
    }
    LOG_INFO("TaskWatchdog: Start timer");
    os_timer_sig_periodic_start(g_p_timer_sig_watchdog_feed);
}

ATTR_NORETURN
static void
reset_task(void)
{
    if (!os_signal_register_cur_thread(g_p_reset_task_signal))
    {
        LOG_ERR("%s failed", "os_signal_register_cur_thread");
    }

    LOG_INFO("Reset task started");

    reset_task_wdt_add_and_start();
    os_timer_sig_periodic_start(g_p_timer_sig_uptime_counter);

    for (;;)
    {
        os_signal_events_t sig_events = { 0 };
        if (!os_signal_wait_with_timeout(g_p_reset_task_signal, OS_DELTA_TICKS_INFINITE, &sig_events))
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
            const reset_task_sig_e reset_task_sig = reset_task_conv_from_sig_num(sig_num);
            reset_task_handle_sig(reset_task_sig);
        }
    }
}

bool
reset_task_init(void)
{
    g_p_reset_task_signal = os_signal_create_static(&g_reset_task_signal_mem);
    os_signal_add(g_p_reset_task_signal, reset_task_conv_to_sig_num(RESET_TASK_SIG_CONFIGURE_BUTTON_PRESSED));
    os_signal_add(g_p_reset_task_signal, reset_task_conv_to_sig_num(RESET_TASK_SIG_CONFIGURE_BUTTON_RELEASED));
    os_signal_add(g_p_reset_task_signal, reset_task_conv_to_sig_num(RESET_TASK_SIG_INCREMENT_UPTIME_COUNTER));
    os_signal_add(g_p_reset_task_signal, reset_task_conv_to_sig_num(RESET_TASK_SIG_REBOOT_BY_CONFIGURE_BUTTON));
    os_signal_add(g_p_reset_task_signal, reset_task_conv_to_sig_num(RESET_TASK_SIG_REBOOT_BY_COMMAND));
    os_signal_add(g_p_reset_task_signal, reset_task_conv_to_sig_num(RESET_TASK_SIG_TASK_WATCHDOG_FEED));
    os_signal_add(g_p_reset_task_signal, reset_task_conv_to_sig_num(RESET_TASK_SIG_RESTART));

    g_p_timer_sig_reset_by_configure_button = os_timer_sig_one_shot_create_static(
        &g_timer_sig_reset_by_configure_button_mem,
        "reset_by_button",
        g_p_reset_task_signal,
        reset_task_conv_to_sig_num(RESET_TASK_SIG_REBOOT_BY_CONFIGURE_BUTTON),
        pdMS_TO_TICKS(RESET_TASK_TIMEOUT_AFTER_PRESSING_CONFIGURE_BUTTON * 1000));

    g_p_timer_sig_reset_by_command = os_timer_sig_one_shot_create_static(
        &g_timer_sig_reset_by_command_mem,
        "reset_by_cmd",
        g_p_reset_task_signal,
        reset_task_conv_to_sig_num(RESET_TASK_SIG_REBOOT_BY_COMMAND),
        pdMS_TO_TICKS(RESET_TASK_TIMEOUT_AFTER_COMMAND * 1000));

    g_p_timer_sig_uptime_counter = os_timer_sig_periodic_create_static(
        &g_timer_sig_uptime_counter_mem,
        "reset:uptime",
        g_p_reset_task_signal,
        reset_task_conv_to_sig_num(RESET_TASK_SIG_INCREMENT_UPTIME_COUNTER),
        pdMS_TO_TICKS(1000U));

    LOG_INFO("TaskWatchdog: reset_task: Create timer");
    g_p_timer_sig_watchdog_feed = os_timer_sig_periodic_create_static(
        &g_timer_sig_watchdog_feed_mem,
        "reset:wdog",
        g_p_reset_task_signal,
        reset_task_conv_to_sig_num(RESET_TASK_SIG_TASK_WATCHDOG_FEED),
        pdMS_TO_TICKS(1000U));

    event_mgr_subscribe_sig_static(
        &g_reset_task_ev_reboot_mem,
        EVENT_MGR_EV_REBOOT,
        g_p_reset_task_signal,
        reset_task_conv_to_sig_num(RESET_TASK_SIG_RESTART));

    const uint32_t stack_size_for_reset_task = 4 * 1024;
    if (!os_task_create_without_param(&reset_task, "reset_task", stack_size_for_reset_task, 1, &g_p_reset_task_handle))
    {
        LOG_ERR("Can't create thread");
        return false;
    }
    while (!os_signal_is_any_thread_registered(g_p_reset_task_signal))
    {
        vTaskDelay(1);
    }
    return true;
}

void
reset_task_reboot_after_timeout(void)
{
    LOG_INFO("Got the command to reboot the gateway after %u seconds", RESET_TASK_TIMEOUT_AFTER_COMMAND);
    os_timer_sig_one_shot_start(g_p_timer_sig_reset_by_command);
}

void
gateway_restart(const char* const p_msg)
{
    reset_info_set_sw(p_msg);
    LOG_INFO("%s", p_msg);
    assert(NULL != g_p_reset_task_handle);
    event_mgr_notify(EVENT_MGR_EV_REBOOT);

    if (os_task_get_cur_task_handle() != g_p_reset_task_handle)
    {
        while (1)
        {
            vTaskDelay(1);
        }
    }
}
