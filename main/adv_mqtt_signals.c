/**
 * @file adv_mqtt_signals.c
 * @author TheSomeMan
 * @date 2023-10-27
 * @copyright Ruuvi Innovations Ltd, license BSD-3-Clause.
 */

#include "adv_mqtt_signals.h"
#include <esp_task_wdt.h>
#include "os_malloc.h"
#include "event_mgr.h"
#include "adv_table.h"
#include "gw_status.h"
#include "time_task.h"
#include "metrics.h"
#include "ruuvi_gateway.h"
#include "mqtt.h"
#include "reset_task.h"
#include "adv_mqtt_internal.h"
#include "adv_mqtt_timers.h"
#include "adv_mqtt_cfg_cache.h"
#include "network_timeout.h"
#if defined(RUUVI_TESTS) && RUUVI_TESTS
#define LOG_LOCAL_DISABLED 1
#define LOG_LOCAL_LEVEL    LOG_LEVEL_NONE
#else
#define LOG_LOCAL_LEVEL LOG_LEVEL_INFO
#endif
#include "log.h"
static const char* TAG = "ADV_MQTT_TASK";

typedef void (*adv_mqtt_sig_handler_t)(adv_mqtt_state_t* const p_adv_mqtt_state);

static os_signal_t*       g_p_adv_mqtt_sig;
static os_signal_static_t g_adv_mqtt_sig_mem;

ATTR_PURE
os_signal_num_e
adv_mqtt_conv_to_sig_num(const adv_mqtt_sig_e sig)
{
    return (os_signal_num_e)sig;
}

adv_mqtt_sig_e
adv_mqtt_conv_from_sig_num(const os_signal_num_e sig_num)
{
    assert(((os_signal_num_e)ADV_MQTT_SIG_FIRST <= sig_num) && (sig_num <= (os_signal_num_e)ADV_MQTT_SIG_LAST));
    return (adv_mqtt_sig_e)sig_num;
}

os_signal_t*
adv_mqtt_signals_get(void)
{
    return g_p_adv_mqtt_sig;
}

void
adv_mqtt_signals_send_sig(const adv_mqtt_sig_e adv_mqtt_sig)
{
    if (!os_signal_send(g_p_adv_mqtt_sig, adv_mqtt_conv_to_sig_num(adv_mqtt_sig)))
    {
        LOG_ERR("%s failed, sig=%d", "os_signal_send", adv_mqtt_sig);
    }
}

bool
adv_mqtt_signals_is_thread_registered(void)
{
    return os_signal_is_any_thread_registered(g_p_adv_mqtt_sig);
}

static void
adv_mqtt_handle_sig_stop(adv_mqtt_state_t* p_adv_mqtt_state)
{
    LOG_INFO("Got ADV_MQTT_SIG_STOP");
    p_adv_mqtt_state->flag_stop = true;
}

static void
adv_mqtt_handle_sig_recv_adv(ATTR_UNUSED adv_mqtt_state_t* const p_adv_mqtt_state) // NOSONAR
{
    LOG_DBG("Got ADV_MQTT_SIG_RECV_ADV");

    if ((!gw_status_is_mqtt_connected()) || (!gw_status_is_relaying_via_mqtt_enabled()))
    {
        return;
    }
    adv_mqtt_cfg_cache_t* p_cfg_cache = adv_mqtt_cfg_cache_mutex_lock();
    if (!p_cfg_cache->flag_mqtt_instant_mode_active)
    {
        adv_mqtt_cfg_cache_mutex_unlock(&p_cfg_cache);
        return;
    }

    if (!mqtt_is_buffer_available_for_publish())
    {
        LOG_DBG("MQTT buffer is full - postpone sending advs to MQTT");
        adv_mqtt_timers_start_timer_sig_retry_sending_advs();
        adv_mqtt_cfg_cache_mutex_unlock(&p_cfg_cache);
        return;
    }

    adv_report_t adv_report = { 0 };
    if (adv_table_read_retransmission_list3_head(&adv_report))
    {
        const time_t timestamp_if_synchronized = time_is_synchronized() ? time(NULL) : 0;
        const time_t timestamp                 = p_cfg_cache->flag_use_ntp ? timestamp_if_synchronized
                                                                           : (time_t)metrics_received_advs_get();
        if (mqtt_publish_adv(&adv_report, p_cfg_cache->flag_use_ntp, timestamp))
        {
            network_timeout_update_timestamp();
        }
        else
        {
            LOG_ERR("%s failed", "mqtt_publish_adv");
        }
    }
    if (!adv_table_read_retransmission_list3_is_empty())
    {
        os_signal_send(g_p_adv_mqtt_sig, adv_mqtt_conv_to_sig_num(ADV_MQTT_SIG_ON_RECV_ADV));
    }
    adv_mqtt_cfg_cache_mutex_unlock(&p_cfg_cache);
}

static void
adv_mqtt_handle_sig_mqtt_connected(ATTR_UNUSED adv_mqtt_state_t* const p_adv_mqtt_state)
{
    mqtt_publish_connect();
}

static void
adv_mqtt_handle_sig_task_watchdog_feed(ATTR_UNUSED adv_mqtt_state_t* const p_adv_mqtt_state) // NOSONAR
{
    LOG_DBG("Feed watchdog");
    const esp_err_t err = esp_task_wdt_reset();
    if (ESP_OK != err)
    {
        LOG_ERR_ESP(err, "%s failed", "esp_task_wdt_reset");
    }
}

static void
adv_mqtt_on_gw_cfg_change(adv_mqtt_state_t* const p_adv_mqtt_state)
{
    (void)p_adv_mqtt_state;
    adv_mqtt_cfg_cache_t* p_cfg_cache = adv_mqtt_cfg_cache_mutex_lock();

    p_cfg_cache->flag_use_ntp                  = gw_cfg_get_ntp_use();
    p_cfg_cache->flag_mqtt_instant_mode_active = gw_cfg_get_mqtt_use_mqtt()
                                                 && (0 == gw_cfg_get_mqtt_sending_interval());

    adv_mqtt_cfg_cache_mutex_unlock(&p_cfg_cache);
}

static void
adv_mqtt_handle_sig_gw_cfg_ready(adv_mqtt_state_t* const p_adv_mqtt_state)
{
    LOG_INFO("Got ADV_MQTT_SIG_GW_CFG_READY");
    adv_mqtt_on_gw_cfg_change(p_adv_mqtt_state);
}

static void
adv_mqtt_handle_sig_gw_cfg_changed_ruuvi(adv_mqtt_state_t* const p_adv_mqtt_state)
{
    LOG_INFO("Got ADV_MQTT_SIG_GW_CFG_CHANGED_RUUVI");
    adv_mqtt_on_gw_cfg_change(p_adv_mqtt_state);
}

static void
adv_mqtt_handle_sig_relaying_mode_changed(ATTR_UNUSED adv_mqtt_state_t* const p_adv_mqtt_state)
{
    LOG_INFO("Got ADV_MQTT_SIG_RELAYING_MODE_CHANGED");
}

static void
adv_mqtt_handle_sig_cfg_mode_activated(ATTR_UNUSED adv_mqtt_state_t* const p_adv_mqtt_state)
{
    LOG_INFO("Got ADV_MQTT_SIG_CFG_MODE_ACTIVATED");
}

static void
adv_mqtt_handle_sig_cfg_mode_deactivated(adv_mqtt_state_t* const p_adv_mqtt_state)
{
    LOG_INFO("Got ADV_MQTT_SIG_CFG_MODE_DEACTIVATED");
    adv_mqtt_on_gw_cfg_change(p_adv_mqtt_state);
}

bool
adv_mqtt_handle_sig(const adv_mqtt_sig_e adv_mqtt_sig, adv_mqtt_state_t* const p_adv_mqtt_state)
{
    static const adv_mqtt_sig_handler_t g_adv_mqtt_sig_handlers[ADV_MQTT_SIG_LAST + 1] = {
        [OS_SIGNAL_NUM_NONE]                 = NULL,
        [ADV_MQTT_SIG_STOP]                  = &adv_mqtt_handle_sig_stop,
        [ADV_MQTT_SIG_ON_RECV_ADV]           = &adv_mqtt_handle_sig_recv_adv,
        [ADV_MQTT_SIG_MQTT_CONNECTED]        = &adv_mqtt_handle_sig_mqtt_connected,
        [ADV_MQTT_SIG_TASK_WATCHDOG_FEED]    = &adv_mqtt_handle_sig_task_watchdog_feed,
        [ADV_MQTT_SIG_GW_CFG_READY]          = &adv_mqtt_handle_sig_gw_cfg_ready,
        [ADV_MQTT_SIG_GW_CFG_CHANGED_RUUVI]  = &adv_mqtt_handle_sig_gw_cfg_changed_ruuvi,
        [ADV_MQTT_SIG_RELAYING_MODE_CHANGED] = &adv_mqtt_handle_sig_relaying_mode_changed,
        [ADV_MQTT_SIG_CFG_MODE_ACTIVATED]    = &adv_mqtt_handle_sig_cfg_mode_activated,
        [ADV_MQTT_SIG_CFG_MODE_DEACTIVATED]  = &adv_mqtt_handle_sig_cfg_mode_deactivated,
    };

    assert(adv_mqtt_sig < OS_ARRAY_SIZE(g_adv_mqtt_sig_handlers));
    adv_mqtt_sig_handler_t p_sig_handler = g_adv_mqtt_sig_handlers[adv_mqtt_sig];
    assert(NULL != p_sig_handler);

    p_sig_handler(p_adv_mqtt_state);

    return p_adv_mqtt_state->flag_stop;
}

void
adv_mqtt_signals_init(void)
{
    g_p_adv_mqtt_sig = os_signal_create_static(&g_adv_mqtt_sig_mem);
    for (uint32_t i = ADV_MQTT_SIG_FIRST; i <= ADV_MQTT_SIG_LAST; ++i)
    {
        os_signal_add(g_p_adv_mqtt_sig, adv_mqtt_conv_to_sig_num((adv_mqtt_sig_e)i));
    }
}

void
adv_mqtt_signals_deinit(void)
{
    os_signal_unregister_cur_thread(g_p_adv_mqtt_sig);
    os_signal_delete(&g_p_adv_mqtt_sig);
}
