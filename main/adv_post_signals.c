/**
 * @file adv_post_signals.c
 * @author TheSomeMan
 * @date 2023-09-04
 * @copyright Ruuvi Innovations Ltd, license BSD-3-Clause.
 */

#include "adv_post_signals.h"
#include <string.h>
#include <esp_task_wdt.h>
#include "os_malloc.h"
#include "event_mgr.h"
#include "adv_table.h"
#include "gw_status.h"
#include "ruuvi_gateway.h"
#include "http.h"
#include "leds.h"
#include "reset_task.h"
#include "adv_post.h"
#include "adv_post_internal.h"
#include "adv_post_async_comm.h"
#include "network_timeout.h"
#include "adv_post_green_led.h"
#include "adv_post_timers.h"
#include "adv_post_cfg_cache.h"
#if defined(RUUVI_TESTS) && RUUVI_TESTS
#define LOG_LOCAL_DISABLED 1
#define LOG_LOCAL_LEVEL    LOG_LEVEL_NONE
#else
#define LOG_LOCAL_LEVEL LOG_LEVEL_INFO
#endif
#include "log.h"
static const char* TAG = "ADV_POST_TASK";

typedef void (*adv_post_sig_handler_t)(adv_post_state_t* const p_adv_post_state);

static os_signal_t*       g_p_adv_post_sig;
static os_signal_static_t g_adv_post_sig_mem;

ATTR_PURE
os_signal_num_e
adv_post_conv_to_sig_num(const adv_post_sig_e sig)
{
    return (os_signal_num_e)sig;
}

adv_post_sig_e
adv_post_conv_from_sig_num(const os_signal_num_e sig_num)
{
    assert(((os_signal_num_e)ADV_POST_SIG_FIRST <= sig_num) && (sig_num <= (os_signal_num_e)ADV_POST_SIG_LAST));
    return (adv_post_sig_e)sig_num;
}

os_signal_t*
adv_post_signals_get(void)
{
    return g_p_adv_post_sig;
}

void
adv_post_signals_send_sig(const adv_post_sig_e adv_post_sig)
{
    if (!os_signal_send(g_p_adv_post_sig, adv_post_conv_to_sig_num(adv_post_sig)))
    {
        LOG_ERR("%s failed, sig=%d", "os_signal_send", adv_post_sig);
    }
}

bool
adv_post_signals_is_thread_registered(void)
{
    return os_signal_is_any_thread_registered(g_p_adv_post_sig);
}

static void
adv_post_restart_pending_retransmissions(const adv_post_state_t* const p_adv_post_state)
{
    LOG_DBG("%s", __func__);
    if (p_adv_post_state->flag_need_to_send_advs1)
    {
        LOG_INFO("Force pending advs1 retransmission");
        adv_post_timers_relaunch_timer_sig_retransmit_to_http_ruuvi();
    }
    if (p_adv_post_state->flag_need_to_send_advs2)
    {
        LOG_INFO("Force pending advs2 retransmission");
        adv_post_timers_relaunch_timer_sig_retransmit_to_http_custom();
    }
    if (p_adv_post_state->flag_need_to_send_statistics)
    {
        LOG_INFO("Force pending statistics retransmission");
        adv_post_timers_relaunch_timer_sig_send_statistics();
    }
}

static void
adv_post_handle_sig_stop(adv_post_state_t* p_adv_post_state)
{
    LOG_INFO("Got ADV_POST_SIG_STOP");
    p_adv_post_state->flag_stop = true;
}

static void
adv_post_handle_sig_network_disconnected(adv_post_state_t* const p_adv_post_state)
{
    LOG_INFO("Handle event: NETWORK_DISCONNECTED");
    p_adv_post_state->flag_network_connected = false;
}

static void
adv_post_handle_sig_network_connected(adv_post_state_t* const p_adv_post_state)
{
    LOG_INFO("Handle event: NETWORK_CONNECTED");
    p_adv_post_state->flag_network_connected = true;
    adv_post_restart_pending_retransmissions(p_adv_post_state);
}

static void
adv_post_handle_sig_time_synchronized(adv_post_state_t* const p_adv_post_state)
{
    if (!p_adv_post_state->flag_primary_time_sync_is_done)
    {
        p_adv_post_state->flag_primary_time_sync_is_done = true;
        LOG_INFO("Remove all accumulated data with zero timestamps");
        LOG_INFO("Clear adv_table");
        adv_table_clear();
        if (gw_cfg_get_http_stat_use_http_stat())
        {
            adv_post_timers_postpone_sending_statistics();
        }
    }
    adv_post_restart_pending_retransmissions(p_adv_post_state);
}

static void
adv_post_handle_sig_retransmit(adv_post_state_t* const p_adv_post_state)
{
    LOG_INFO("Got ADV_POST_SIG_RETRANSMIT");
    if (p_adv_post_state->flag_relaying_enabled && gw_cfg_get_http_use_http_ruuvi())
    {
        p_adv_post_state->flag_need_to_send_advs1 = true;
        os_signal_send(g_p_adv_post_sig, adv_post_conv_to_sig_num(ADV_POST_SIG_DO_ASYNC_COMM));
    }
}

static void
adv_post_handle_sig_retransmit2(adv_post_state_t* const p_adv_post_state)
{
    LOG_INFO("Got ADV_POST_SIG_RETRANSMIT2");
    if (p_adv_post_state->flag_relaying_enabled && gw_cfg_get_http_use_http())
    {
        p_adv_post_state->flag_need_to_send_advs2 = true;
        os_signal_send(g_p_adv_post_sig, adv_post_conv_to_sig_num(ADV_POST_SIG_DO_ASYNC_COMM));
    }
}

static void
adv_post_handle_sig_retransmit_mqtt(adv_post_state_t* const p_adv_post_state)
{
    LOG_INFO("Got ADV_POST_SIG_RETRANSMIT_MQTT");
    if (p_adv_post_state->flag_relaying_enabled && gw_cfg_get_mqtt_use_mqtt())
    {
        p_adv_post_state->flag_need_to_send_mqtt_periodic = true;
        os_signal_send(g_p_adv_post_sig, adv_post_conv_to_sig_num(ADV_POST_SIG_DO_ASYNC_COMM));
    }
}

static void
adv_post_handle_sig_send_statistics(adv_post_state_t* const p_adv_post_state)
{
    LOG_INFO("Got ADV_POST_SIG_SEND_STATISTICS");
    if (p_adv_post_state->flag_relaying_enabled && gw_cfg_get_http_stat_use_http_stat())
    {
        p_adv_post_state->flag_need_to_send_statistics = true;
        os_signal_send(g_p_adv_post_sig, adv_post_conv_to_sig_num(ADV_POST_SIG_DO_ASYNC_COMM));
    }
}

static void
adv_post_handle_sig_relaying_mode_changed(adv_post_state_t* const p_adv_post_state)
{
    p_adv_post_state->flag_relaying_enabled = gw_status_is_relaying_via_http_enabled();
    LOG_INFO(
        "ADV_POST_SIG_RELAYING_MODE_CHANGED: flag_relaying_enabled=%d",
        (printf_int_t)p_adv_post_state->flag_relaying_enabled);
    if ((!p_adv_post_state->flag_relaying_enabled) && p_adv_post_state->flag_async_comm_in_progress)
    {
        adv_post_timers_stop_timer_sig_do_async_comm();
        http_abort_any_req_during_processing();
        p_adv_post_state->flag_async_comm_in_progress = false;
        LOG_DBG("http_server_mutex_unlock");
        http_server_mutex_unlock();
    }
    if (!p_adv_post_state->flag_relaying_enabled)
    {
        if (gw_cfg_get_http_use_http_ruuvi())
        {
            leds_notify_http1_data_sent_fail();
        }
        if (gw_cfg_get_http_use_http())
        {
            leds_notify_http2_data_sent_fail();
        }
    }
    gw_status_clear_http_relaying_cmd();
}

static void
adv_post_handle_sig_network_watchdog(ATTR_UNUSED adv_post_state_t* const p_adv_post_state) // NOSONAR
{
    if (network_timeout_check())
    {
        LOG_INFO(
            "No networking for %lu seconds - reboot the gateway",
            (printf_ulong_t)RUUVI_NETWORK_WATCHDOG_TIMEOUT_SECONDS);
        gateway_restart("Network watchdog");
    }
}

static void
adv_post_handle_sig_task_watchdog_feed(ATTR_UNUSED adv_post_state_t* const p_adv_post_state) // NOSONAR
{
    LOG_DBG("Feed watchdog");
    LOG_INFO("Advs cnt: %lu", (printf_ulong_t)adv_post_advs_cnt_get_and_clear());
    const esp_err_t err = esp_task_wdt_reset();
    if (ESP_OK != err)
    {
        LOG_ERR_ESP(err, "%s failed", "esp_task_wdt_reset");
    }
}

static bool
adv_post_on_gw_cfg_change_handle_scan_filter(
    adv_post_cfg_cache_t* const             p_cfg_cache,
    const ruuvi_gw_cfg_scan_filter_t* const p_scan_filter)
{
    p_cfg_cache->scan_filter_allow_listed = p_scan_filter->scan_filter_allow_listed;

    if (0 != p_scan_filter->scan_filter_length)
    {
        p_cfg_cache->p_arr_of_scan_filter_mac = os_calloc(
            p_scan_filter->scan_filter_length,
            sizeof(*p_cfg_cache->p_arr_of_scan_filter_mac));
        if (NULL == p_cfg_cache->p_arr_of_scan_filter_mac)
        {
            LOG_ERR("Can't allocate memory for scan_filter");
            p_cfg_cache->scan_filter_length = 0;
            return false;
        }
    }
    if (NULL != p_cfg_cache->p_arr_of_scan_filter_mac)
    {
        for (uint32_t i = 0; i < p_scan_filter->scan_filter_length; ++i)
        {
            memcpy(
                &p_cfg_cache->p_arr_of_scan_filter_mac[i],
                &p_scan_filter->scan_filter_list[i],
                sizeof(p_cfg_cache->p_arr_of_scan_filter_mac[i]));
        }
        p_cfg_cache->scan_filter_length = p_scan_filter->scan_filter_length;
    }
    else
    {
        p_cfg_cache->scan_filter_length = 0;
    }
    return true;
}

static void
adv_post_on_gw_cfg_change(adv_post_state_t* const p_adv_post_state)
{
    p_adv_post_state->flag_use_timestamps = gw_cfg_get_ntp_use();

    adv_post_cfg_cache_t* p_cfg_cache = adv_post_cfg_cache_mutex_lock();

    p_cfg_cache->flag_use_ntp = p_adv_post_state->flag_use_timestamps;
    if (NULL != p_cfg_cache->p_arr_of_scan_filter_mac)
    {
        p_cfg_cache->scan_filter_length = 0;
        os_free(p_cfg_cache->p_arr_of_scan_filter_mac);
        p_cfg_cache->p_arr_of_scan_filter_mac = NULL;
    }

    const gw_cfg_t* p_gw_cfg = gw_cfg_lock_ro();
    const bool      res = adv_post_on_gw_cfg_change_handle_scan_filter(p_cfg_cache, &p_gw_cfg->ruuvi_cfg.scan_filter);
    gw_cfg_unlock_ro(&p_gw_cfg);
    if (!res)
    {
        gateway_restart("Low memory on gw_cfg_change");
        return;
    }

    if (gw_cfg_get_http_use_http_ruuvi())
    {
        LOG_INFO("Start timer for advs1 retransmission");
        adv1_post_timer_relaunch_with_default_period();
        p_adv_post_state->flag_need_to_send_advs1 = true;
    }
    else
    {
        LOG_INFO("Stop timer for advs1 retransmission");
        adv1_post_timer_stop();
        p_adv_post_state->flag_need_to_send_advs1 = false;
    }

    if (gw_cfg_get_http_use_http())
    {
        LOG_INFO("Start timer for advs2 retransmission");
        adv2_post_timer_set_default_period(gw_cfg_get_http_period() * TIME_UNITS_MS_PER_SECOND);
        adv2_post_timer_relaunch_with_default_period();
        p_adv_post_state->flag_need_to_send_advs2 = true;
    }
    else
    {
        LOG_INFO("Stop timer for advs2 retransmission");
        adv2_post_timer_stop();
        p_adv_post_state->flag_need_to_send_advs2 = false;
    }

    const uint32_t mqtt_sending_interval = gw_cfg_get_mqtt_sending_interval();
    if (gw_cfg_get_mqtt_use_mqtt() && (0 != mqtt_sending_interval))
    {
        LOG_INFO("Start timer for relaying to MQTT");
        adv_post_timers_restart_timer_sig_mqtt(pdMS_TO_TICKS(mqtt_sending_interval) * TIME_UNITS_MS_PER_SECOND);
    }
    else
    {
        LOG_INFO("Stop timer for relaying to MQTT");
        adv_post_timers_stop_timer_sig_mqtt();
    }

    if (gw_cfg_get_http_stat_use_http_stat())
    {
        LOG_INFO("Relaunch timer to send statistics");
        adv_post_timers_relaunch_timer_sig_send_statistics();
        p_adv_post_state->flag_need_to_send_statistics = true;
    }
    else
    {
        LOG_INFO("Stop timer to send statistics");
        adv_post_timers_stop_timer_sig_send_statistics();
        p_adv_post_state->flag_need_to_send_statistics = false;
    }

    adv_post_cfg_cache_mutex_unlock(&p_cfg_cache);

    LOG_INFO("Clear adv_table");
    adv_table_clear();
    adv_post_timers_start_timer_sig_do_async_comm();
}

static void
adv_post_handle_sig_gw_cfg_ready(adv_post_state_t* const p_adv_post_state)
{
    LOG_INFO("Got ADV_POST_SIG_GW_CFG_READY");
    ruuvi_send_nrf_settings_from_gw_cfg();
    if (gw_cfg_get_http_stat_use_http_stat())
    {
        adv_post_timers_postpone_sending_statistics();
    }
    adv_post_on_gw_cfg_change(p_adv_post_state);
    if (gw_cfg_get_mqtt_use_mqtt_over_ssl_or_wss() && (gw_cfg_get_http_use_http_ruuvi() || gw_cfg_get_http_use_http()))
    {
        http_server_mutex_activate();
    }
    else
    {
        http_server_mutex_deactivate();
    }
}

static void
adv_post_handle_sig_gw_cfg_changed_ruuvi(adv_post_state_t* const p_adv_post_state)
{
    LOG_INFO("Got ADV_POST_SIG_GW_CFG_CHANGED_RUUVI");
    ruuvi_send_nrf_settings_from_gw_cfg();
    adv_post_on_gw_cfg_change(p_adv_post_state);
    if (gw_cfg_get_mqtt_use_mqtt_over_ssl_or_wss() && (gw_cfg_get_http_use_http_ruuvi() || gw_cfg_get_http_use_http()))
    {
        http_server_mutex_activate();
    }
    else
    {
        http_server_mutex_deactivate();
    }
}

static void
adv_post_handle_sig_ble_scan_changed(ATTR_UNUSED adv_post_state_t* const p_adv_post_state) // NOSONAR
{
    LOG_INFO("Got ADV_POST_SIG_BLE_SCAN_CHANGED");
    LOG_INFO("Clear adv_table");
    adv_table_clear();
}

static void
adv_post_handle_sig_cfg_mode_activated(ATTR_UNUSED adv_post_state_t* const p_adv_post_state) // NOSONAR
{
    LOG_INFO("Got ADV_POST_SIG_CFG_MODE_ACTIVATED");
    LOG_INFO("Stop network watchdog timer");
    adv_post_timers_stop_timer_sig_network_watchdog();
    adv_post_cfg_cache_t* p_cfg_cache = adv_post_cfg_cache_mutex_lock();
    if (NULL != p_cfg_cache->p_arr_of_scan_filter_mac)
    {
        p_cfg_cache->scan_filter_length = 0;
        os_free(p_cfg_cache->p_arr_of_scan_filter_mac);
        p_cfg_cache->p_arr_of_scan_filter_mac = NULL;
    }
    adv_post_cfg_cache_mutex_unlock(&p_cfg_cache);

    LOG_INFO("Clear adv_table");
    adv_table_clear();
}

static void
adv_post_handle_sig_cfg_mode_deactivated(adv_post_state_t* const p_adv_post_state)
{
    LOG_INFO("Got ADV_POST_SIG_CFG_MODE_DEACTIVATED");
    LOG_INFO("Start network watchdog timer");
    adv_post_timers_start_timer_sig_network_watchdog();
    ruuvi_send_nrf_settings_from_gw_cfg();
    adv_post_on_gw_cfg_change(p_adv_post_state);
}

static void
adv_post_handle_sig_green_led_turn_on(ATTR_UNUSED adv_post_state_t* const p_adv_post_state) // NOSONAR
{
    adv_post_on_green_led_update(ADV_POST_GREEN_LED_CMD_ON);
}

static void
adv_post_handle_sig_green_led_turn_off(ATTR_UNUSED adv_post_state_t* const p_adv_post_state) // NOSONAR
{
    adv_post_on_green_led_update(ADV_POST_GREEN_LED_CMD_OFF);
}

static void
adv_post_handle_sig_green_led_update(ATTR_UNUSED adv_post_state_t* const p_adv_post_state) // NOSONAR
{
    adv_post_on_green_led_update(ADV_POST_GREEN_LED_CMD_UPDATE);
}

static void
adv_post_handle_sig_recv_adv_timeout(ATTR_UNUSED adv_post_state_t* const p_adv_post_state) // NOSONAR
{
    event_mgr_notify(EVENT_MGR_EV_RECV_ADV_TIMEOUT);
}

bool
adv_post_handle_sig(const adv_post_sig_e adv_post_sig, adv_post_state_t* const p_adv_post_state)
{
    static const adv_post_sig_handler_t g_adv_post_sig_handlers[ADV_POST_SIG_LAST + 1] = {
        [OS_SIGNAL_NUM_NONE]                 = NULL,
        [ADV_POST_SIG_STOP]                  = &adv_post_handle_sig_stop,
        [ADV_POST_SIG_NETWORK_DISCONNECTED]  = &adv_post_handle_sig_network_disconnected,
        [ADV_POST_SIG_NETWORK_CONNECTED]     = &adv_post_handle_sig_network_connected,
        [ADV_POST_SIG_TIME_SYNCHRONIZED]     = &adv_post_handle_sig_time_synchronized,
        [ADV_POST_SIG_RETRANSMIT]            = &adv_post_handle_sig_retransmit,
        [ADV_POST_SIG_RETRANSMIT2]           = &adv_post_handle_sig_retransmit2,
        [ADV_POST_SIG_RETRANSMIT_MQTT]       = &adv_post_handle_sig_retransmit_mqtt,
        [ADV_POST_SIG_SEND_STATISTICS]       = &adv_post_handle_sig_send_statistics,
        [ADV_POST_SIG_DO_ASYNC_COMM]         = &adv_post_do_async_comm,
        [ADV_POST_SIG_RELAYING_MODE_CHANGED] = &adv_post_handle_sig_relaying_mode_changed,
        [ADV_POST_SIG_NETWORK_WATCHDOG]      = &adv_post_handle_sig_network_watchdog,
        [ADV_POST_SIG_TASK_WATCHDOG_FEED]    = &adv_post_handle_sig_task_watchdog_feed,
        [ADV_POST_SIG_GW_CFG_READY]          = &adv_post_handle_sig_gw_cfg_ready,
        [ADV_POST_SIG_GW_CFG_CHANGED_RUUVI]  = &adv_post_handle_sig_gw_cfg_changed_ruuvi,
        [ADV_POST_SIG_BLE_SCAN_CHANGED]      = &adv_post_handle_sig_ble_scan_changed,
        [ADV_POST_SIG_CFG_MODE_ACTIVATED]    = &adv_post_handle_sig_cfg_mode_activated,
        [ADV_POST_SIG_CFG_MODE_DEACTIVATED]  = &adv_post_handle_sig_cfg_mode_deactivated,
        [ADV_POST_SIG_GREEN_LED_TURN_ON]     = &adv_post_handle_sig_green_led_turn_on,
        [ADV_POST_SIG_GREEN_LED_TURN_OFF]    = &adv_post_handle_sig_green_led_turn_off,
        [ADV_POST_SIG_GREEN_LED_UPDATE]      = &adv_post_handle_sig_green_led_update,
        [ADV_POST_SIG_RECV_ADV_TIMEOUT]      = &adv_post_handle_sig_recv_adv_timeout,
    };

    assert(adv_post_sig < OS_ARRAY_SIZE(g_adv_post_sig_handlers));
    adv_post_sig_handler_t p_sig_handler = g_adv_post_sig_handlers[adv_post_sig];
    assert(NULL != p_sig_handler);

    p_sig_handler(p_adv_post_state);

    return p_adv_post_state->flag_stop;
}

void
adv_post_signals_init(void)
{
    g_p_adv_post_sig = os_signal_create_static(&g_adv_post_sig_mem);
    for (uint32_t i = ADV_POST_SIG_FIRST; i <= ADV_POST_SIG_LAST; ++i)
    {
        os_signal_add(g_p_adv_post_sig, adv_post_conv_to_sig_num((adv_post_sig_e)i));
    }
}

void
adv_post_signals_deinit(void)
{
    os_signal_unregister_cur_thread(g_p_adv_post_sig);
    os_signal_delete(&g_p_adv_post_sig);
}
