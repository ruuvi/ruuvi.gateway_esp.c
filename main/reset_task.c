/**
 * @file reset_task.c
 * @author TheSomeMan
 * @date 2021-03-31
 * @copyright Ruuvi Innovations Ltd, license BSD-3-Clause.
 */

#include "reset_task.h"
#include "os_signal.h"
#include "os_timer_sig.h"
#include "ethernet.h"
#include "wifi_manager.h"
#include "leds.h"

#define LOG_LOCAL_LEVEL LOG_LEVEL_DEBUG
#include "log.h"

typedef enum reset_task_sig_e
{
    RESET_TASK_SIG_CONFIGURE_BUTTON_PRESSED   = OS_SIGNAL_NUM_0,
    RESET_TASK_SIG_CONFIGURE_BUTTON_RELEASED  = OS_SIGNAL_NUM_1,
    RESET_TASK_SIG_REBOOT_BY_CONFIGURE_BUTTON = OS_SIGNAL_NUM_2,
    RESET_TASK_SIG_REBOOT_BY_AP_TIMEOUT       = OS_SIGNAL_NUM_3,
    RESET_TASK_SIG_REBOOT_AFTER_DELAY         = OS_SIGNAL_NUM_4,
} reset_task_sig_e;

#define RESET_TASK_SIG_FIRST (RESET_TASK_SIG_CONFIGURE_BUTTON_PRESSED)
#define RESET_TASK_SIG_LAST  (RESET_TASK_SIG_REBOOT_AFTER_DELAY)

#define RESET_TASK_TIMEOUT_AFTER_PRESSING_CONFIGURE_BUTTON     (5)
#define RESET_TASK_TIMEOUT_AFTER_MANUAL_HOTSPOT_ACTIVATION_SEC (60)
#define RESET_TASK_DELAY_BEFORE_REBOOTING_SEC                  (3)

static const char *TAG = "reset_task";

static os_timer_sig_one_shot_t *      g_p_timer_sig_reset_by_configure_button;
static os_timer_sig_one_shot_static_t g_timer_sig_reset_by_configure_button_mem;
static os_timer_sig_one_shot_t *      g_p_timer_sig_reset_after_hotspot_activation;
static os_timer_sig_one_shot_static_t g_timer_sig_reset_after_hotspot_activation_mem;
static os_timer_sig_one_shot_t *      g_p_timer_sig_reset_after_delay;
static os_timer_sig_one_shot_static_t g_timer_sig_reset_after_delay_mem;
static os_signal_t *                  g_p_signal_reset_task;
static os_signal_static_t             signal_reset_task_mem;

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
reset_task_start_timer_after_hotspot_activation(void)
{
    LOG_INFO("Start AP timer for %u seconds", RESET_TASK_TIMEOUT_AFTER_MANUAL_HOTSPOT_ACTIVATION_SEC);
    os_timer_sig_one_shot_start(g_p_timer_sig_reset_after_hotspot_activation);
}

void
reset_task_stop_timer_after_hotspot_activation(void)
{
    LOG_INFO("Stop AP timer");
    os_timer_sig_one_shot_stop(g_p_timer_sig_reset_after_hotspot_activation);
}

void
reset_task_notify_configure_button_pressed(void)
{
    os_signal_send(g_p_signal_reset_task, reset_task_conv_to_sig_num(RESET_TASK_SIG_CONFIGURE_BUTTON_PRESSED));
}

void
reset_task_notify_configure_button_released(void)
{
    os_signal_send(g_p_signal_reset_task, reset_task_conv_to_sig_num(RESET_TASK_SIG_CONFIGURE_BUTTON_RELEASED));
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
            os_timer_sig_one_shot_start(g_p_timer_sig_reset_by_configure_button);
            leds_indication_on_configure_button_press();
            break;
        case RESET_TASK_SIG_CONFIGURE_BUTTON_RELEASED:
            LOG_INFO("The CONFIGURE button has been released");
            os_timer_sig_one_shot_stop(g_p_timer_sig_reset_by_configure_button);
            if (wifi_manager_is_connected_to_ethernet())
            {
                LOG_INFO("Disconnect from Ethernet");
                wifi_manager_disconnect_eth();
            }
            if (wifi_manager_is_connected_to_wifi())
            {
                LOG_INFO("Disconnect from WiFi");
                wifi_manager_disconnect_wifi();
            }
            if (!wifi_manager_is_ap_active())
            {
                LOG_INFO("WiFi AP is not active - stop Ethernet and start WiFi AP");
                ethernet_stop();
                wifi_manager_start_ap();
                leds_indication_on_hotspot_activation();
                reset_task_start_timer_after_hotspot_activation();
            }
            else
            {
                LOG_INFO("WiFi AP is already active");
            }
            break;
        case RESET_TASK_SIG_REBOOT_BY_CONFIGURE_BUTTON:
            LOG_INFO("System restart is activated by the Configure button");
            // restart the Gateway,
            // on boot it will check if RB_BUTTON_RESET_PIN is pressed and erase the
            // settings in flash.
            esp_restart();
            break;
        case RESET_TASK_SIG_REBOOT_BY_AP_TIMEOUT:
            LOG_INFO("System restart is activated by WiFi AP timeout");
            esp_restart();
            break;
        case RESET_TASK_SIG_REBOOT_AFTER_DELAY:
            LOG_INFO("System restart is activated by command");
            esp_restart();
            break;
        default:
            LOG_ERR("Unhanded sig: %d", (int)reset_task_sig);
            assert(0);
            break;
    }
}

ATTR_NORETURN
static void
reset_task(void)
{
    if (!os_signal_register_cur_thread(g_p_signal_reset_task))
    {
        LOG_ERR("%s failed", "os_signal_register_cur_thread");
    }

    LOG_INFO("Reset task started");
    for (;;)
    {
        os_signal_events_t sig_events = { 0 };
        if (!os_signal_wait_with_timeout(g_p_signal_reset_task, OS_DELTA_TICKS_INFINITE, &sig_events))
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
    g_p_signal_reset_task = os_signal_create_static(&signal_reset_task_mem);
    os_signal_add(g_p_signal_reset_task, reset_task_conv_to_sig_num(RESET_TASK_SIG_CONFIGURE_BUTTON_PRESSED));
    os_signal_add(g_p_signal_reset_task, reset_task_conv_to_sig_num(RESET_TASK_SIG_CONFIGURE_BUTTON_RELEASED));
    os_signal_add(g_p_signal_reset_task, reset_task_conv_to_sig_num(RESET_TASK_SIG_REBOOT_BY_CONFIGURE_BUTTON));
    os_signal_add(g_p_signal_reset_task, reset_task_conv_to_sig_num(RESET_TASK_SIG_REBOOT_BY_AP_TIMEOUT));
    os_signal_add(g_p_signal_reset_task, reset_task_conv_to_sig_num(RESET_TASK_SIG_REBOOT_AFTER_DELAY));

    g_p_timer_sig_reset_by_configure_button = os_timer_sig_one_shot_create_static(
        &g_timer_sig_reset_by_configure_button_mem,
        "reset_by_button",
        g_p_signal_reset_task,
        reset_task_conv_to_sig_num(RESET_TASK_SIG_REBOOT_BY_CONFIGURE_BUTTON),
        pdMS_TO_TICKS(RESET_TASK_TIMEOUT_AFTER_PRESSING_CONFIGURE_BUTTON * 1000));

    g_p_timer_sig_reset_after_hotspot_activation = os_timer_sig_one_shot_create_static(
        &g_timer_sig_reset_after_hotspot_activation_mem,
        "reset_by_ap",
        g_p_signal_reset_task,
        reset_task_conv_to_sig_num(RESET_TASK_SIG_REBOOT_BY_AP_TIMEOUT),
        pdMS_TO_TICKS(RESET_TASK_TIMEOUT_AFTER_MANUAL_HOTSPOT_ACTIVATION_SEC * 1000));

    g_p_timer_sig_reset_after_delay = os_timer_sig_one_shot_create_static(
        &g_timer_sig_reset_after_delay_mem,
        "reset_after_delay",
        g_p_signal_reset_task,
        reset_task_conv_to_sig_num(RESET_TASK_SIG_REBOOT_AFTER_DELAY),
        pdMS_TO_TICKS(RESET_TASK_DELAY_BEFORE_REBOOTING_SEC * 1000));

    const uint32_t   stack_size_for_reset_task = 4 * 1024;
    os_task_handle_t ph_task_reset             = NULL;
    if (!os_task_create_without_param(&reset_task, "reset_task", stack_size_for_reset_task, 1, &ph_task_reset))
    {
        LOG_ERR("Can't create thread");
        return false;
    }
    return true;
}

void
reset_task_activate_after_delay(void)
{
    os_timer_sig_one_shot_start(g_p_timer_sig_reset_after_delay);
}
