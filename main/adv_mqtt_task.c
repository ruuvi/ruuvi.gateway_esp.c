/**
 * @file adv_mqtt.c
 * @author TheSomeMan
 * @date 2023-10-27
 * @copyright Ruuvi Innovations Ltd, license BSD-3-Clause.
 */

#include "adv_mqtt_task.h"
#include <stdint.h>
#include <stdbool.h>
#include <esp_task_wdt.h>
#include "os_signal.h"
#include "adv_mqtt_internal.h"
#include "adv_mqtt_signals.h"
#include "adv_mqtt_events.h"
#include "adv_mqtt_timers.h"

#if defined(RUUVI_TESTS) && RUUVI_TESTS
#define LOG_LOCAL_DISABLED 1
#define LOG_LOCAL_LEVEL    LOG_LEVEL_NONE
#else
#define LOG_LOCAL_LEVEL LOG_LEVEL_INFO
#endif
#include "log.h"
static const char TAG[] = "ADV_MQTT";

static void
adv_mqtt_wdt_add_and_start(void)
{
    LOG_INFO("TaskWatchdog: Register current thread");
    const esp_err_t err = esp_task_wdt_add(xTaskGetCurrentTaskHandle());
    if (ESP_OK != err)
    {
        LOG_ERR_ESP(err, "%s failed", "esp_task_wdt_add");
    }
    LOG_INFO("TaskWatchdog: Start timer");
    adv_mqtt_timers_start_timer_sig_watchdog_feed();
}

void
adv_mqtt_task(void)
{
    esp_log_level_set(TAG, LOG_LOCAL_LEVEL);

    adv_mqtt_signals_init();
    adv_mqtt_subscribe_events();
    adv_mqtt_create_timers();

    if (!os_signal_register_cur_thread(adv_mqtt_signals_get()))
    {
        LOG_ERR("%s failed", "os_signal_register_cur_thread");
        adv_mqtt_delete_timers();
        adv_mqtt_unsubscribe_events();
        adv_mqtt_signals_deinit();
        return;
    }

    LOG_INFO("%s started", __func__);

    adv_mqtt_wdt_add_and_start();

    adv_mqtt_state_t adv_mqtt_state = {
        .flag_stop = false,
    };

    while (!adv_mqtt_state.flag_stop)
    {
        os_signal_events_t sig_events = { 0 };
        if (!os_signal_wait_with_timeout(adv_mqtt_signals_get(), OS_DELTA_TICKS_INFINITE, &sig_events))
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
            const adv_mqtt_sig_e adv_mqtt_sig = adv_mqtt_conv_from_sig_num(sig_num);
            if (adv_mqtt_handle_sig(adv_mqtt_sig, &adv_mqtt_state))
            {
                break;
            }
        }
#if !defined(RUUVI_TESTS) || !RUUVI_TESTS
        taskYIELD();
#endif
    }
    LOG_INFO("Stop task adv_mqtt");

    LOG_INFO("TaskWatchdog: Unregister current thread");
    esp_task_wdt_delete(xTaskGetCurrentTaskHandle());

    adv_mqtt_delete_timers();
    adv_mqtt_unsubscribe_events();

    adv_mqtt_signals_deinit();
}

bool
adv_mqtt_task_is_initialized(void)
{
    return adv_mqtt_signals_is_thread_registered();
}

void
adv_mqtt_task_stop(void)
{
    LOG_INFO("adv_mqtt_task_stop");
    adv_mqtt_signals_send_sig(ADV_MQTT_SIG_STOP);
}
