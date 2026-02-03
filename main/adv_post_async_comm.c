/**
 * @file adv_post_async_comm.c
 * @author TheSomeMan
 * @date 2023-09-04
 * @copyright Ruuvi Innovations Ltd, license BSD-3-Clause.
 */

#include "adv_post_async_comm.h"
#include <esp_system.h>
#include <esp_attr.h>
#include "os_malloc.h"
#include "os_timer_sig.h"
#include "time_task.h"
#include "ruuvi_gateway.h"
#include "gw_status.h"
#include "adv_table.h"
#include "http.h"
#include "mqtt.h"
#include "leds.h"
#include "adv_post.h"
#include "adv_post_statistics.h"
#include "adv_post_timers.h"
#include "adv_post_signals.h"
#include "reset_task.h"
#if defined(RUUVI_TESTS) && RUUVI_TESTS
#define LOG_LOCAL_DISABLED 1
#define LOG_LOCAL_LEVEL    LOG_LEVEL_NONE
#else
#define LOG_LOCAL_LEVEL LOG_LEVEL_INFO
#endif
#include "log.h"
static const char TAG[] = "ADV_POST_ASYNC_COMM";

static uint32_t IRAM_ATTR g_adv_post_nonce;
static adv_post_action_e  g_adv_post_action = ADV_POST_ACTION_NONE;

static const adv_report_table_t* IRAM_ATTR g_p_adv_post_reports_mqtt;
static uint32_t IRAM_ATTR                  g_adv_post_reports_mqtt_idx;
static time_t IRAM_ATTR                    g_adv_post_reports_mqtt_timestamp;

void
adv_post_async_comm_init(void)
{
    g_adv_post_nonce                  = esp_random();
    g_adv_post_action                 = ADV_POST_ACTION_NONE;
    g_p_adv_post_reports_mqtt         = NULL;
    g_adv_post_reports_mqtt_idx       = 0;
    g_adv_post_reports_mqtt_timestamp = 0;
}

static void
adv_post_log(const adv_report_table_t* p_reports, const bool flag_use_timestamps, const char* const p_target_name)
{
    (void)flag_use_timestamps;
    LOG_INFO("Advertisements in table for target=%s (num=%u):", p_target_name, (printf_uint_t)p_reports->num_of_advs);
#if LOG_LOCAL_LEVEL >= LOG_LEVEL_INFO
    for (num_of_advs_t i = 0; i < p_reports->num_of_advs; ++i)
    {
        const adv_report_t* p_adv = &p_reports->table[i];

        const mac_address_str_t mac_str = mac_address_to_str(&p_adv->tag_mac);
        LOG_DUMP_INFO(
            p_adv->data_buf,
            p_adv->data_len,
            "i: %d, tag: %s, rssi: %d, %s: %ld",
            i,
            mac_str.str_buf,
            p_adv->rssi,
            flag_use_timestamps ? "timestamp" : "counter",
            p_adv->timestamp);
        vTaskDelay(pdMS_TO_TICKS(5));
    }
#endif
}

static bool
adv_post_retransmit_advs(
    const adv_report_table_t* const p_reports,
    const bool                      flag_use_timestamps,
    const bool                      flag_post_to_ruuvi)
{
    const ruuvi_gw_cfg_http_t* p_cfg_http = gw_cfg_get_http_copy();
    if (NULL == p_cfg_http)
    {
        LOG_ERR("%s failed", "gw_cfg_get_http_copy");
        return false;
    }
    const bool res
        = http_post_advs(p_reports, g_adv_post_nonce, flag_use_timestamps, flag_post_to_ruuvi, p_cfg_http, NULL);
    os_free(p_cfg_http);
    if (!res)
    {
        return false;
    }
    return true;
}

static bool
adv_post_do_retransmission(const bool flag_use_timestamps, const adv_post_action_e adv_post_action)
{
    LOG_DBG("Allocate %u bytes for adv reports buffer", (printf_uint_t)sizeof(adv_report_table_t));
    adv_report_table_t* p_adv_reports_buf = os_calloc(1, sizeof(*p_adv_reports_buf));
    if (NULL == p_adv_reports_buf)
    {
        LOG_ERR("Can't allocate %u bytes of memory", (printf_uint_t)sizeof(*p_adv_reports_buf));
        gateway_restart_low_memory();
        return false;
    }

    bool res = false;

    // for thread safety copy the advertisements to a separate buffer for posting
    switch (adv_post_action)
    {
        case ADV_POST_ACTION_NONE:
            LOG_ERR("Incorrect adv_post_action: %s", "ADV_POST_ACTION_NONE");
            assert(0);
            break;
        case ADV_POST_ACTION_POST_ADVS_TO_RUUVI:
            adv_table_read_retransmission_list1_and_clear(p_adv_reports_buf);
            adv_post_log(p_adv_reports_buf, flag_use_timestamps, "HTTP(Ruuvi)");
            res = adv_post_retransmit_advs(p_adv_reports_buf, flag_use_timestamps, true);
            os_free(p_adv_reports_buf);
            break;
        case ADV_POST_ACTION_POST_ADVS_TO_CUSTOM:
            adv_table_read_retransmission_list2_and_clear(p_adv_reports_buf);
            adv_post_log(p_adv_reports_buf, flag_use_timestamps, "HTTP(Custom)");
            res = adv_post_retransmit_advs(p_adv_reports_buf, flag_use_timestamps, false);
            os_free(p_adv_reports_buf);
            break;
        case ADV_POST_ACTION_POST_STATS:
            LOG_ERR("Incorrect adv_post_action: %s", "ADV_POST_ACTION_POST_STATS");
            assert(0);
            break;
        case ADV_POST_ACTION_POST_ADVS_TO_MQTT:
            adv_table_read_retransmission_list3_and_clear(p_adv_reports_buf);
            adv_post_log(p_adv_reports_buf, flag_use_timestamps, "MQTT");
            g_p_adv_post_reports_mqtt         = p_adv_reports_buf;
            g_adv_post_reports_mqtt_idx       = 0;
            g_adv_post_reports_mqtt_timestamp = (gw_cfg_get_ntp_use() ? time(NULL) : 0);
            res                               = true;
            break;
    }
    return res;
}

static void
adv_post_do_async_comm_send_advs1(adv_post_state_t* const p_adv_post_state)
{
    if (!p_adv_post_state->flag_network_connected)
    {
        LOG_DBG("Can't send advs1, no network connection");
        return;
    }
    if (p_adv_post_state->flag_use_timestamps && (!time_is_synchronized()))
    {
        LOG_DBG("Can't send advs1, the time is not yet synchronized");
        return;
    }
    LOG_DBG("http_server_mutex_try_lock");
    if (!http_server_mutex_try_lock())
    {
        LOG_DBG("Wait until incoming HTTP connection is handled, postpone sending advs1");
        return;
    }
    g_adv_post_action = ADV_POST_ACTION_POST_ADVS_TO_RUUVI;
    if (!adv_post_do_retransmission(p_adv_post_state->flag_use_timestamps, ADV_POST_ACTION_POST_ADVS_TO_RUUVI))
    {
        g_adv_post_action = ADV_POST_ACTION_NONE;
        leds_notify_http1_data_sent_fail();
        p_adv_post_state->flag_need_to_send_advs1 = false;
        LOG_DBG("http_server_mutex_unlock");
        http_server_mutex_unlock();
        return;
    }
    g_adv_post_nonce += 1;
    p_adv_post_state->flag_async_comm_in_progress = true;
    p_adv_post_state->flag_need_to_send_advs1     = false;
}

static void
adv_post_do_async_comm_send_advs2(adv_post_state_t* const p_adv_post_state)
{
    if (!p_adv_post_state->flag_network_connected)
    {
        LOG_DBG("Can't send advs2, no network connection");
        return;
    }
    if (p_adv_post_state->flag_use_timestamps && (!time_is_synchronized()))
    {
        LOG_DBG("Can't send advs2, the time is not yet synchronized");
        return;
    }
    LOG_DBG("http_server_mutex_try_lock");
    if (!http_server_mutex_try_lock())
    {
        LOG_DBG("Wait until incoming HTTP connection is handled, postpone sending advs2");
        return;
    }
    g_adv_post_action = ADV_POST_ACTION_POST_ADVS_TO_CUSTOM;
    if (!adv_post_do_retransmission(p_adv_post_state->flag_use_timestamps, ADV_POST_ACTION_POST_ADVS_TO_CUSTOM))
    {
        g_adv_post_action = ADV_POST_ACTION_NONE;
        leds_notify_http2_data_sent_fail();
        p_adv_post_state->flag_need_to_send_advs2 = false;
        LOG_DBG("http_server_mutex_unlock");
        http_server_mutex_unlock();
        return;
    }
    g_adv_post_nonce += 1;
    p_adv_post_state->flag_async_comm_in_progress = true;
    p_adv_post_state->flag_need_to_send_advs2     = false;
}

static void
adv_post_do_async_comm_send_statistics(adv_post_state_t* const p_adv_post_state)
{
    if (!gw_cfg_get_http_stat_use_http_stat())
    {
        LOG_WARN("Can't send statistics, it was disabled in gw_cfg");
        p_adv_post_state->flag_need_to_send_statistics = false;
        return;
    }
    if (!p_adv_post_state->flag_network_connected)
    {
        LOG_DBG("Can't send statistics, no network connection");
        return;
    }
    if (p_adv_post_state->flag_use_timestamps && (!time_is_synchronized()))
    {
        LOG_DBG("Can't send statistics, the time is not yet synchronized");
        return;
    }
    LOG_DBG("http_server_mutex_try_lock");
    if (!http_server_mutex_try_lock())
    {
        LOG_DBG("Wait until incoming HTTP connection is handled, postpone sending statistics");
        return;
    }
    g_adv_post_action = ADV_POST_ACTION_POST_STATS;
    if (!adv_post_statistics_do_send())
    {
        LOG_ERR("Failed to send statistics");
        g_adv_post_action                              = ADV_POST_ACTION_NONE;
        p_adv_post_state->flag_need_to_send_statistics = false;
        LOG_DBG("http_server_mutex_unlock");
        http_server_mutex_unlock();
        return;
    }
    p_adv_post_state->flag_need_to_send_statistics = false;
    p_adv_post_state->flag_async_comm_in_progress  = true;
}

static void
adv_post_do_async_comm_start_sending_periodic_mqtt(adv_post_state_t* const p_adv_post_state)
{
    if (!gw_cfg_get_mqtt_use_mqtt())
    {
        LOG_DBG("Can't send advs via MQTT, it was disabled in gw_cfg");
        p_adv_post_state->flag_need_to_send_mqtt_periodic = false;
        return;
    }
    if (!p_adv_post_state->flag_network_connected)
    {
        LOG_DBG("Can't send advs via MQTT, no network connection");
        p_adv_post_state->flag_need_to_send_mqtt_periodic = false;
        return;
    }
    if (!gw_status_is_mqtt_connected())
    {
        LOG_DBG("Can't send advs via MQTT, MQTT is not connected");
        p_adv_post_state->flag_need_to_send_mqtt_periodic = false;
        return;
    }
    if (p_adv_post_state->flag_use_timestamps && (!time_is_synchronized()))
    {
        LOG_DBG("Can't send advs via MQTT, the time is not yet synchronized");
        return;
    }
    g_adv_post_action = ADV_POST_ACTION_POST_ADVS_TO_MQTT;

    if (!adv_post_do_retransmission(p_adv_post_state->flag_use_timestamps, ADV_POST_ACTION_POST_ADVS_TO_MQTT))
    {
        g_adv_post_action                                 = ADV_POST_ACTION_NONE;
        p_adv_post_state->flag_need_to_send_mqtt_periodic = false;
        main_task_send_sig_restart_services();
        return;
    }

    p_adv_post_state->flag_need_to_send_mqtt_periodic = false;
    p_adv_post_state->flag_async_comm_in_progress     = true;
    adv_post_signals_send_sig(ADV_POST_SIG_DO_ASYNC_COMM);
}

static bool
adv_post_do_async_comm_in_progress_mqtt(void)
{
    assert(NULL != g_p_adv_post_reports_mqtt);

    const adv_report_t* const p_adv_report = &g_p_adv_post_reports_mqtt->table[g_adv_post_reports_mqtt_idx];

    if (!mqtt_publish_adv(p_adv_report, gw_cfg_get_ntp_use(), g_adv_post_reports_mqtt_timestamp))
    {
        LOG_ERR("%s failed", "mqtt_publish_adv");
        os_free(g_p_adv_post_reports_mqtt);
        g_p_adv_post_reports_mqtt   = NULL;
        g_adv_post_reports_mqtt_idx = 0;
        return true;
    }

    g_adv_post_reports_mqtt_idx += 1;
    if (g_adv_post_reports_mqtt_idx >= g_p_adv_post_reports_mqtt->num_of_advs)
    {
        os_free(g_p_adv_post_reports_mqtt);
        g_p_adv_post_reports_mqtt   = NULL;
        g_adv_post_reports_mqtt_idx = 0;
        return true;
    }
    return false;
}

static bool
adv_post_do_async_comm_in_progress(adv_post_state_t* const p_adv_post_state)
{
    if (ADV_POST_ACTION_POST_ADVS_TO_MQTT != g_adv_post_action)
    {
        if (!http_async_poll())
        {
            LOG_DBG("os_timer_sig_one_shot_start: g_p_adv_post_timer_sig_do_async_comm");
            adv_post_timers_start_timer_sig_do_async_comm();
            return false;
        }
        switch (g_adv_post_action)
        {
            case ADV_POST_ACTION_POST_ADVS_TO_RUUVI:
                p_adv_post_state->flag_need_to_send_advs1 = false;
                g_adv_post_action                         = ADV_POST_ACTION_NONE;
                break;
            case ADV_POST_ACTION_POST_ADVS_TO_CUSTOM:
                p_adv_post_state->flag_need_to_send_advs2 = false;
                g_adv_post_action                         = ADV_POST_ACTION_NONE;
                break;
            case ADV_POST_ACTION_POST_STATS:
                p_adv_post_state->flag_need_to_send_statistics = false;
                g_adv_post_action                              = ADV_POST_ACTION_NONE;
                break;
            default:
                break;
        }

        LOG_DBG("http_server_mutex_unlock");
        http_server_mutex_unlock();

        ruuvi_log_heap_usage();
    }
    else
    {
        if (!adv_post_do_async_comm_in_progress_mqtt())
        {
            LOG_DBG("os_timer_sig_one_shot_start: g_p_adv_post_timer_sig_do_async_comm");
            adv_post_timers_start_timer_sig_do_async_comm();
            return false;
        }
    }
    return true;
}

void
adv_post_do_async_comm(adv_post_state_t* const p_adv_post_state)
{
    LOG_DBG("flag_async_comm_in_progress=%d", p_adv_post_state->flag_async_comm_in_progress);
    if (p_adv_post_state->flag_async_comm_in_progress)
    {
        if (!adv_post_do_async_comm_in_progress(p_adv_post_state))
        {
            return;
        }
        p_adv_post_state->flag_async_comm_in_progress = false;
    }
    if (!p_adv_post_state->flag_relaying_enabled)
    {
        return;
    }
    if (p_adv_post_state->flag_need_to_send_advs1)
    {
        adv_post_do_async_comm_send_advs1(p_adv_post_state);
        adv_post_timers_start_timer_sig_do_async_comm();
        return;
    }
    if (p_adv_post_state->flag_need_to_send_advs2)
    {
        adv_post_do_async_comm_send_advs2(p_adv_post_state);
        adv_post_timers_start_timer_sig_do_async_comm();
        return;
    }
    if (p_adv_post_state->flag_need_to_send_statistics)
    {
        adv_post_do_async_comm_send_statistics(p_adv_post_state);
        adv_post_timers_start_timer_sig_do_async_comm();
        return;
    }
    if (p_adv_post_state->flag_need_to_send_mqtt_periodic)
    {
        adv_post_do_async_comm_start_sending_periodic_mqtt(p_adv_post_state);
        adv_post_timers_start_timer_sig_do_async_comm();
        return;
    }
}

void
adv_post_set_default_period(const uint32_t period_ms)
{
    switch (g_adv_post_action)
    {
        case ADV_POST_ACTION_NONE:
            break;
        case ADV_POST_ACTION_POST_ADVS_TO_RUUVI:
            adv1_post_timer_set_default_period_by_server_resp(period_ms);
            break;
        case ADV_POST_ACTION_POST_ADVS_TO_CUSTOM:
            adv2_post_timer_set_default_period_by_server_resp(period_ms);
            break;
        case ADV_POST_ACTION_POST_STATS:
            ATTR_FALLTHROUGH;
        case ADV_POST_ACTION_POST_ADVS_TO_MQTT:
            break;
    }
}

bool
adv_post_set_hmac_sha256_key(const char* const p_key_str)
{
    switch (g_adv_post_action)
    {
        case ADV_POST_ACTION_NONE:
            break;
        case ADV_POST_ACTION_POST_ADVS_TO_RUUVI:
            LOG_INFO("Ruuvi-HMAC-KEY: Server updated HMAC_SHA256 key for Ruuvi target");
            return hmac_sha256_set_key_for_http_ruuvi(p_key_str);
        case ADV_POST_ACTION_POST_ADVS_TO_CUSTOM:
            LOG_INFO("Ruuvi-HMAC-KEY: Server updated HMAC_SHA256 key for custom target");
            return hmac_sha256_set_key_for_http_custom(p_key_str);
        case ADV_POST_ACTION_POST_STATS:
            LOG_INFO("Ruuvi-HMAC-KEY: Server updated HMAC_SHA256 key for stats");
            return hmac_sha256_set_key_for_stats(p_key_str);
        case ADV_POST_ACTION_POST_ADVS_TO_MQTT:
            break;
    }
    return false;
}

void
adv_post_set_adv_post_http_action(const bool flag_post_to_ruuvi)
{
    if (flag_post_to_ruuvi)
    {
        g_adv_post_action = ADV_POST_ACTION_POST_ADVS_TO_RUUVI;
    }
    else
    {
        g_adv_post_action = ADV_POST_ACTION_POST_ADVS_TO_CUSTOM;
    }
}

adv_post_action_e
adv_post_get_adv_post_action(void)
{
    return g_adv_post_action;
}
