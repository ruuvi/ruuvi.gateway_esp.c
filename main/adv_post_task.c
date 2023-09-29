/**
 * @file adv_post_task.c
 * @author TheSomeMan
 * @date 2023-09-04
 * @copyright Ruuvi Innovations Ltd, license BSD-3-Clause.
 */

#include "adv_post_task.h"
#include <stdbool.h>
#include <esp_task_wdt.h>
#include "os_signal.h"
#include "adv_post_internal.h"
#include "adv_post_signals.h"
#include "adv_post_events.h"
#include "adv_post_timers.h"
#include "http.h"
#if defined(RUUVI_TESTS) && RUUVI_TESTS
#define LOG_LOCAL_DISABLED 1
#define LOG_LOCAL_LEVEL    LOG_LEVEL_NONE
#else
#define LOG_LOCAL_LEVEL LOG_LEVEL_INFO
#endif
#include "log.h"
static const char* TAG = "ADV_POST_TASK";

static void
adv_post_wdt_add_and_start(void)
{
    LOG_INFO("TaskWatchdog: Register current thread");
    const esp_err_t err = esp_task_wdt_add(xTaskGetCurrentTaskHandle());
    if (ESP_OK != err)
    {
        LOG_ERR_ESP(err, "%s failed", "esp_task_wdt_add");
    }
    LOG_INFO("TaskWatchdog: Start timer");
    adv_post_timers_start_timer_sig_watchdog_feed();
}

void
adv_post_task(void)
{
    esp_log_level_set(TAG, LOG_LOCAL_LEVEL);

    adv_post_signals_init();
    adv_post_subscribe_events();
    adv_post_create_timers();

    if (!os_signal_register_cur_thread(adv_post_signals_get()))
    {
        LOG_ERR("%s failed", "os_signal_register_cur_thread");
        adv_post_delete_timers();
        adv_post_unsubscribe_events();
        adv_post_signals_deinit();
        return;
    }

    LOG_INFO("%s started", __func__);
    adv_post_timers_start_timer_sig_network_watchdog();
    adv_post_timers_start_timer_sig_recv_adv_timeout();

    adv_post_wdt_add_and_start();

    adv_post_state_t adv_post_state = {
        .flag_primary_time_sync_is_done  = false,
        .flag_network_connected          = false,
        .flag_async_comm_in_progress     = false,
        .flag_need_to_send_advs1         = false,
        .flag_need_to_send_advs2         = false,
        .flag_need_to_send_statistics    = false,
        .flag_need_to_send_mqtt_periodic = false,
        .flag_relaying_enabled           = true,
        .flag_use_timestamps             = false,
        .flag_stop                       = false,
    };

    while (!adv_post_state.flag_stop)
    {
        os_signal_events_t sig_events = { 0 };
        if (!os_signal_wait_with_timeout(adv_post_signals_get(), OS_DELTA_TICKS_INFINITE, &sig_events))
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
            const adv_post_sig_e adv_post_sig = adv_post_conv_from_sig_num(sig_num);
            if (adv_post_handle_sig(adv_post_sig, &adv_post_state))
            {
                break;
            }
        }
    }
    LOG_INFO("Stop task adv_post");

    LOG_INFO("TaskWatchdog: Unregister current thread");
    esp_task_wdt_delete(xTaskGetCurrentTaskHandle());

    adv_post_delete_timers();
    adv_post_unsubscribe_events();

    http_abort_any_req_during_processing();

    adv_post_signals_deinit();
}

bool
adv_post_task_is_initialized(void)
{
    return adv_post_signals_is_thread_registered();
}

void
adv_post_task_stop(void)
{
    LOG_INFO("adv_post_task_stop");
    adv_post_signals_send_sig(ADV_POST_SIG_STOP);
}
