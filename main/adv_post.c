/**
 * @file adv_post.c
 * @author Oleg Protasevich
 * @date 2020-09-04
 * @copyright Ruuvi Innovations Ltd, license BSD-3-Clause.
 */

#include "adv_post.h"
#include <string.h>
#include <time.h>
#include "freertos/FreeRTOS.h"
#include "esp_task_wdt.h"
#include "esp_err.h"
#include "esp_type_wrapper.h"
#include "os_task.h"
#include "os_malloc.h"
#include "os_mutex.h"
#include "os_signal.h"
#include "os_timer_sig.h"
#include "cJSON.h"
#include "http.h"
#include "mqtt.h"
#include "api.h"
#include "types_def.h"
#include "ruuvi_endpoint_ca_uart.h"
#include "ruuvi_gateway.h"
#include "time_task.h"
#include "attribs.h"
#include "metrics.h"
#include "wifi_manager.h"
#include "mac_addr.h"
#include "ruuvi_device_id.h"
#include "event_mgr.h"
#include "nrf52fw.h"
#include "reset_task.h"
#include "time_units.h"
#include "gw_status.h"

#define LOG_LOCAL_LEVEL LOG_LEVEL_INFO
#include "log.h"

typedef enum adv_post_sig_e
{
    ADV_POST_SIG_STOP                 = OS_SIGNAL_NUM_0,
    ADV_POST_SIG_NETWORK_DISCONNECTED = OS_SIGNAL_NUM_1,
    ADV_POST_SIG_NETWORK_CONNECTED    = OS_SIGNAL_NUM_2,
    ADV_POST_SIG_TIME_SYNCHRONIZED    = OS_SIGNAL_NUM_3,
    ADV_POST_SIG_RETRANSMIT           = OS_SIGNAL_NUM_4,
    ADV_POST_SIG_SEND_STATISTICS      = OS_SIGNAL_NUM_5,
    ADV_POST_SIG_DO_ASYNC_COMM        = OS_SIGNAL_NUM_6,
    ADV_POST_SIG_DISABLE              = OS_SIGNAL_NUM_7,
    ADV_POST_SIG_ENABLE               = OS_SIGNAL_NUM_8,
    ADV_POST_SIG_NETWORK_WATCHDOG     = OS_SIGNAL_NUM_9,
    ADV_POST_SIG_TASK_WATCHDOG_FEED   = OS_SIGNAL_NUM_10,
    ADV_POST_SIG_GW_CFG_READY         = OS_SIGNAL_NUM_11,
    ADV_POST_SIG_GW_CFG_CHANGED_RUUVI = OS_SIGNAL_NUM_12,
} adv_post_sig_e;

typedef struct adv_post_state_t
{
    bool flag_network_connected;
    bool flag_async_comm_in_progress;
    bool flag_need_to_send_advs;
    bool flag_need_to_send_statistics;
    bool flag_retransmission_disabled;
    bool flag_use_timestamps;
} adv_post_state_t;

#define ADV_POST_SIG_FIRST (ADV_POST_SIG_STOP)
#define ADV_POST_SIG_LAST  (ADV_POST_SIG_GW_CFG_CHANGED_RUUVI)

static void
adv_post_send_report(void *p_arg);

static void
adv_post_send_ack(void *arg);

static void
adv_post_cb_on_recv_device_id(void *arg);

static void
adv_post_send_get_all(void *arg);

static const char *TAG = "ADV_POST_TASK";

static adv_callbacks_fn_t adv_callback_func_tbl = {
    .AdvAckCallback    = adv_post_send_ack,
    .AdvReportCallback = adv_post_send_report,
    .AdvIdCallback     = adv_post_cb_on_recv_device_id,
    .AdvGetAllCallback = adv_post_send_get_all,
};

static uint32_t                       g_adv_post_nonce;
static os_signal_t *                  g_p_adv_post_sig;
static os_signal_static_t             g_adv_post_sig_mem;
static os_timer_sig_periodic_t *      g_p_adv_post_timer_sig_retransmit;
static os_timer_sig_periodic_static_t g_adv_post_timer_sig_retransmit_mem;
static os_timer_sig_periodic_t *      g_p_adv_post_timer_sig_send_statistics;
static os_timer_sig_periodic_static_t g_adv_post_timer_sig_send_statistics_mem;
static os_timer_sig_one_shot_t *      g_p_adv_post_timer_sig_do_async_comm;
static os_timer_sig_one_shot_static_t g_adv_post_timer_sig_do_async_comm_mem;
static os_timer_sig_periodic_t *      g_p_adv_post_timer_sig_watchdog_feed;
static os_timer_sig_periodic_static_t g_adv_post_timer_sig_watchdog_feed_mem;
static TickType_t                     g_adv_post_last_successful_network_comm_timestamp;
static os_mutex_t                     g_p_adv_port_last_successful_network_comm_mutex;
static os_mutex_static_t              g_adv_port_last_successful_network_comm_mutex_mem;
static os_timer_sig_periodic_t *      g_p_adv_post_timer_sig_network_watchdog;
static os_timer_sig_periodic_static_t g_adv_post_timer_sig_network_watchdog_mem;
static event_mgr_ev_info_static_t     g_adv_post_ev_info_mem_wifi_disconnected;
static event_mgr_ev_info_static_t     g_adv_post_ev_info_mem_eth_disconnected;
static event_mgr_ev_info_static_t     g_adv_post_ev_info_mem_wifi_connected;
static event_mgr_ev_info_static_t     g_adv_post_ev_info_mem_eth_connected;
static event_mgr_ev_info_static_t     g_adv_post_ev_info_mem_time_synchronized;
static event_mgr_ev_info_static_t     g_adv_post_ev_info_mem_cfg_ready;
static event_mgr_ev_info_static_t     g_adv_post_ev_info_mem_gw_cfg_ruuvi_changed;

static uint32_t g_adv_post_interval_ms = ADV_POST_DEFAULT_INTERVAL_SECONDS * TIME_UNITS_MS_PER_SECOND;

ATTR_PURE
static os_signal_num_e
adv_post_conv_to_sig_num(const adv_post_sig_e sig)
{
    return (os_signal_num_e)sig;
}

static adv_post_sig_e
adv_post_conv_from_sig_num(const os_signal_num_e sig_num)
{
    assert(((os_signal_num_e)ADV_POST_SIG_FIRST <= sig_num) && (sig_num <= (os_signal_num_e)ADV_POST_SIG_LAST));
    return (adv_post_sig_e)sig_num;
}

/** @brief serialise up to U64 into given buffer, MSB first. */
static inline void
u64_to_array(const uint64_t u64, uint8_t *const p_array, const uint8_t num_bytes)
{
    const uint8_t offset = num_bytes - 1;
    uint8_t       cnt    = num_bytes;
    while (0 != cnt)
    {
        cnt -= 1;
        p_array[offset - cnt] = (u64 >> (RUUVI_BITS_PER_BYTE * cnt)) & RUUVI_BYTE_MASK;
    }
}

static esp_err_t
adv_put_to_table(const adv_report_t *const p_adv)
{
    metrics_received_advs_increment();
    if (!adv_table_put(p_adv))
    {
        return ESP_ERR_NO_MEM;
    }
    return ESP_OK;
}

static bool
parse_adv_report_from_uart(const re_ca_uart_payload_t *const p_msg, const time_t timestamp, adv_report_t *const p_adv)
{
    if (NULL == p_msg)
    {
        return false;
    }
    if (NULL == p_adv)
    {
        return false;
    }
    if (RE_CA_UART_ADV_RPRT != p_msg->cmd)
    {
        return false;
    }
    const re_ca_uart_ble_adv_t *const p_report = &(p_msg->params.adv);
    if (p_report->adv_len > sizeof(p_adv->data_buf))
    {
        LOG_ERR(
            "Got advertisement with len=%u, max allowed len=%u",
            (unsigned)p_report->adv_len,
            (unsigned)sizeof(p_adv->data_buf));
        return false;
    }
    if (0 == p_report->adv_len)
    {
        LOG_WARN("Got advertisement with zero len");
        return false;
    }
    mac_address_bin_init(&p_adv->tag_mac, p_report->mac);
    p_adv->timestamp       = timestamp;
    p_adv->samples_counter = 0;
    p_adv->rssi            = p_report->rssi_db;
    p_adv->data_len        = p_report->adv_len;
    memcpy(p_adv->data_buf, p_report->adv, p_report->adv_len);

    return true;
}

static void
adv_post_send_ack(void *arg)
{
    (void)arg;
    // Do something
}

static void
adv_post_cb_on_recv_device_id(void *p_arg)
{
    const re_ca_uart_payload_t *const p_uart_payload = (re_ca_uart_payload_t *)p_arg;

    nrf52_device_id_t nrf52_device_id = { 0 };
    u64_to_array(p_uart_payload->params.device_id.id, &nrf52_device_id.id[0], sizeof(nrf52_device_id.id));

    mac_address_bin_t nrf52_mac_addr = { 0 };
    u64_to_array(p_uart_payload->params.device_id.addr, &nrf52_mac_addr.mac[0], sizeof(nrf52_mac_addr.mac));

    LOG_INFO("nRF52 DEVICE ID : 0x%016llx", p_uart_payload->params.device_id.id);
    LOG_INFO("nRF52 MAC ADDR  : 0x%016llx", p_uart_payload->params.device_id.addr);

    ruuvi_device_id_set(&nrf52_device_id, &nrf52_mac_addr);
}

static void
adv_post_send_report(void *p_arg)
{
    if (!gw_cfg_is_initialized())
    {
        LOG_WARN("Drop adv - gw_cfg is not ready yet");
        return;
    }

    if (!gw_status_is_network_connected())
    {
        static TickType_t g_last_tick_log_printed = 0;
        const TickType_t  cur_tick                = xTaskGetTickCount();
        if ((cur_tick - g_last_tick_log_printed) >= pdMS_TO_TICKS(60 * 1000))
        {
            LOG_WARN("Drop adv - no network connection");
            g_last_tick_log_printed = cur_tick;
        }
        return;
    }

    const bool flag_ntp_use = gw_cfg_get_ntp_use();
    if (flag_ntp_use && (!time_is_synchronized()))
    {
        LOG_WARN("Drop adv - time is not synchronized yet");
        return;
    }

    const time_t timestamp = flag_ntp_use ? time(NULL) : (time_t)metrics_received_advs_get();

    adv_report_t adv_report = { 0 };
    if (!parse_adv_report_from_uart((re_ca_uart_payload_t *)p_arg, timestamp, &adv_report))
    {
        LOG_WARN("Drop adv - parsing failed");
        return;
    }

    const esp_err_t ret = adv_put_to_table(&adv_report);
    if (ESP_ERR_NO_MEM == ret)
    {
        LOG_WARN("Drop adv - table full");
    }
    if (gw_cfg_get_mqtt_use_mqtt())
    {
        if (!gw_status_is_mqtt_connected())
        {
            LOG_WARN("Can't send adv, MQTT is not connected yet");
        }
        else
        {
            if (mqtt_publish_adv(&adv_report, flag_ntp_use, time(NULL)))
            {
                adv_post_last_successful_network_comm_timestamp_update();
            }
            else
            {
                LOG_ERR("%s failed", "mqtt_publish_adv");
            }
        }
    }
}

static void
adv_post_send_get_all(void *arg)
{
    (void)arg;
    LOG_INFO("Got configuration request from NRF52");
    ruuvi_send_nrf_settings();
}

static void
adv_post_log(const adv_report_table_t *p_reports, const bool flag_use_timestamps)
{
    LOG_INFO("Advertisements in table: %u", (printf_uint_t)p_reports->num_of_advs);
    for (num_of_advs_t i = 0; i < p_reports->num_of_advs; ++i)
    {
        const adv_report_t *p_adv = &p_reports->table[i];

        const mac_address_str_t mac_str = mac_address_to_str(&p_adv->tag_mac);
        if (flag_use_timestamps)
        {
            LOG_DUMP_INFO(
                p_adv->data_buf,
                p_adv->data_len,
                "i: %d, tag: %s, rssi: %d, timestamp: %ld",
                i,
                mac_str.str_buf,
                p_adv->rssi,
                p_adv->timestamp);
        }
        else
        {
            LOG_DUMP_INFO(
                p_adv->data_buf,
                p_adv->data_len,
                "i: %d, tag: %s, rssi: %d, counter: %ld",
                i,
                mac_str.str_buf,
                p_adv->rssi,
                p_adv->timestamp);
        }
    }
}

static bool
adv_post_retransmit_advs(
    const adv_report_table_t *p_reports,
    const bool                flag_network_connected,
    const bool                flag_use_timestamps)
{
    if (0 == p_reports->num_of_advs)
    {
        return false;
    }
    if (!flag_network_connected)
    {
        LOG_WARN("Can't send advs, no network connection");
        return false;
    }

    if (!http_send_advs(p_reports, g_adv_post_nonce, flag_use_timestamps))
    {
        return false;
    }
    os_timer_sig_one_shot_start(g_p_adv_post_timer_sig_do_async_comm);
    return true;
}

static bool
adv_post_do_retransmission(const bool flag_network_connected, const bool flag_use_timestamps)
{
    static adv_report_table_t g_adv_reports_buf;

    // for thread safety copy the advertisements to a separate buffer for posting
    adv_table_read_retransmission_list_and_clear(&g_adv_reports_buf);

    adv_post_log(&g_adv_reports_buf, flag_use_timestamps);

    return adv_post_retransmit_advs(&g_adv_reports_buf, flag_network_connected, flag_use_timestamps);
}

static bool
adv_post_do_send_statistics(void)
{
    adv_report_table_t *p_reports = os_malloc(sizeof(*p_reports));
    if (NULL == p_reports)
    {
        LOG_ERR("Can't allocate memory for statistics");
        return false;
    }
    adv_table_statistics_read(p_reports);

    const http_json_statistics_info_t stat_info = {
        .nrf52_mac_addr         = *gw_cfg_get_nrf52_mac_addr(),
        .esp_fw                 = *gw_cfg_get_esp32_fw_ver(),
        .nrf_fw                 = *gw_cfg_get_nrf52_fw_ver(),
        .uptime                 = g_uptime_counter,
        .nonce                  = g_adv_post_nonce,
        .is_connected_to_wifi   = wifi_manager_is_connected_to_wifi(),
        .network_disconnect_cnt = g_network_disconnect_cnt,
    };

    const bool res = http_send_statistics(&stat_info, p_reports);
    os_free(p_reports);
    return res;
}

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
    os_timer_sig_periodic_start(g_p_adv_post_timer_sig_watchdog_feed);
}

static void
adv_post_do_async_comm_send_advs(adv_post_state_t *const p_adv_post_state)
{
    if (!p_adv_post_state->flag_network_connected)
    {
        LOG_WARN("Can't send advs, no network connection");
    }
    else if (p_adv_post_state->flag_use_timestamps && (!time_is_synchronized()))
    {
        LOG_WARN("Can't send advs, the time is not yet synchronized");
    }
    else
    {
        p_adv_post_state->flag_need_to_send_advs = false;
        if (adv_post_do_retransmission(p_adv_post_state->flag_network_connected, p_adv_post_state->flag_use_timestamps))
        {
            p_adv_post_state->flag_async_comm_in_progress = true;
        }
        g_adv_post_nonce += 1;
    }
}

static void
adv_post_do_async_comm_send_statistics(adv_post_state_t *const p_adv_post_state)
{
    if (!gw_cfg_get_http_stat_use_http_stat())
    {
        LOG_INFO("Can't send statistics, it was disabled in gw_cfg");
        p_adv_post_state->flag_need_to_send_statistics = false;
    }
    else if (!p_adv_post_state->flag_network_connected)
    {
        LOG_WARN("Can't send statistics, no network connection");
    }
    else if (p_adv_post_state->flag_use_timestamps && (!time_is_synchronized()))
    {
        LOG_WARN("Can't send statistics, the time is not yet synchronized");
    }
    else
    {
        p_adv_post_state->flag_need_to_send_statistics = false;
        if (adv_post_do_send_statistics())
        {
            p_adv_post_state->flag_async_comm_in_progress = true;
            os_timer_sig_one_shot_start(g_p_adv_post_timer_sig_do_async_comm);
        }
        else
        {
            LOG_ERR("Failed to send statistics");
        }
        g_adv_post_nonce += 1;
    }
}

static void
adv_post_do_async_comm(adv_post_state_t *const p_adv_post_state)
{
    if (p_adv_post_state->flag_async_comm_in_progress)
    {
        if (http_async_poll())
        {
            p_adv_post_state->flag_async_comm_in_progress = false;
        }
        else
        {
            os_timer_sig_one_shot_start(g_p_adv_post_timer_sig_do_async_comm);
        }
    }
    if ((!p_adv_post_state->flag_async_comm_in_progress) && p_adv_post_state->flag_need_to_send_advs)
    {
        adv_post_do_async_comm_send_advs(p_adv_post_state);
    }
    if ((!p_adv_post_state->flag_async_comm_in_progress) && p_adv_post_state->flag_need_to_send_statistics)
    {
        adv_post_do_async_comm_send_statistics(p_adv_post_state);
    }
}

static void
adv_post_last_successful_network_comm_timestamp_lock(void)
{
    if (NULL == g_p_adv_port_last_successful_network_comm_mutex)
    {
        g_p_adv_port_last_successful_network_comm_mutex = os_mutex_create_static(
            &g_adv_port_last_successful_network_comm_mutex_mem);
    }
    os_mutex_lock(g_p_adv_port_last_successful_network_comm_mutex);
}

static void
adv_post_last_successful_network_comm_timestamp_unlock(void)
{
    os_mutex_unlock(g_p_adv_port_last_successful_network_comm_mutex);
}

void
adv_post_last_successful_network_comm_timestamp_update(void)
{
    adv_post_last_successful_network_comm_timestamp_lock();
    g_adv_post_last_successful_network_comm_timestamp = xTaskGetTickCount();
    adv_post_last_successful_network_comm_timestamp_unlock();
}

static bool
adv_post_last_successful_network_comm_timestamp_check_timeout(void)
{
    const TickType_t timeout_ticks = pdMS_TO_TICKS(RUUVI_NETWORK_WATCHDOG_TIMEOUT_SECONDS) * 1000;

    adv_post_last_successful_network_comm_timestamp_lock();
    const TickType_t delta_ticks = xTaskGetTickCount() - g_adv_post_last_successful_network_comm_timestamp;
    adv_post_last_successful_network_comm_timestamp_unlock();

    return (delta_ticks > timeout_ticks) ? true : false;
}

static void
adv_post_handle_sig_network_watchdog(void)
{
    if (adv_post_last_successful_network_comm_timestamp_check_timeout())
    {
        LOG_INFO(
            "No networking for %lu seconds - reboot the gateway",
            (printf_ulong_t)RUUVI_NETWORK_WATCHDOG_TIMEOUT_SECONDS);
        esp_restart();
    }
}

static void
adv_post_handle_sig_task_watchdog_feed(void)
{
    LOG_DBG("Feed watchdog");
    const esp_err_t err = esp_task_wdt_reset();
    if (ESP_OK != err)
    {
        LOG_ERR_ESP(err, "%s failed", "esp_task_wdt_reset");
    }
}

static void
adv_post_handle_sig_send_statistics(adv_post_state_t *const p_adv_post_state)
{
    if (!gw_cfg_get_http_stat_use_http_stat())
    {
        return;
    }
    p_adv_post_state->flag_need_to_send_statistics = true;
    if (!p_adv_post_state->flag_retransmission_disabled)
    {
        if (!p_adv_post_state->flag_network_connected)
        {
            LOG_WARN("Can't send statistics, no network connection");
            return;
        }
        if (p_adv_post_state->flag_use_timestamps && (!time_is_synchronized()))
        {
            LOG_WARN("Can't send statistics, the time is not yet synchronized");
            return;
        }
        LOG_INFO("%s: os_signal_send: ADV_POST_SIG_DO_ASYNC_COMM", __func__);
        os_signal_send(g_p_adv_post_sig, adv_post_conv_to_sig_num(ADV_POST_SIG_DO_ASYNC_COMM));
    }
}

static void
adv_post_timer_restart(void)
{
    os_timer_sig_periodic_restart(g_p_adv_post_timer_sig_retransmit, pdMS_TO_TICKS(g_adv_post_interval_ms));
}

static void
adv_post_handle_sig_time_synchronized(const adv_post_state_t *const p_adv_post_state)
{
    if (p_adv_post_state->flag_need_to_send_advs)
    {
        LOG_INFO("Time has been synchronized - force advs retransmission");
        os_timer_sig_periodic_simulate(g_p_adv_post_timer_sig_retransmit);
        adv_post_timer_restart();
    }
    if (p_adv_post_state->flag_need_to_send_statistics)
    {
        LOG_INFO("Time has been synchronized - force statistics retransmission");
        os_timer_sig_periodic_simulate(g_p_adv_post_timer_sig_send_statistics);
        os_timer_sig_periodic_restart(
            g_p_adv_post_timer_sig_send_statistics,
            pdMS_TO_TICKS(ADV_POST_STATISTICS_INTERVAL_SECONDS) * TIME_UNITS_MS_PER_SECOND);
    }
}

static void
adv_post_on_gw_cfg_change(adv_post_state_t *const p_adv_post_state)
{
    p_adv_post_state->flag_use_timestamps = gw_cfg_get_ntp_use();
    if (gw_cfg_get_http_use_http())
    {
        LOG_INFO("Start timer for advs retransmission");
        os_timer_sig_periodic_start(g_p_adv_post_timer_sig_retransmit);
    }
    else
    {
        LOG_INFO("Stop timer for advs retransmission");
        os_timer_sig_periodic_stop(g_p_adv_post_timer_sig_retransmit);
        p_adv_post_state->flag_need_to_send_advs = false;
    }
    if (gw_cfg_get_http_stat_use_http_stat())
    {
        LOG_INFO("Start timer to send statistics");
        os_timer_sig_periodic_start(g_p_adv_post_timer_sig_send_statistics);
    }
    else
    {
        LOG_INFO("Stop timer to send statistics");
        os_timer_sig_periodic_stop(g_p_adv_post_timer_sig_send_statistics);
        p_adv_post_state->flag_need_to_send_statistics = false;
    }
}

static bool
adv_post_handle_sig(const adv_post_sig_e adv_post_sig, adv_post_state_t *const p_adv_post_state)
{
    bool flag_stop = false;
    switch (adv_post_sig)
    {
        case ADV_POST_SIG_STOP:
            LOG_INFO("Got ADV_POST_SIG_STOP");
            flag_stop = true;
            break;
        case ADV_POST_SIG_NETWORK_DISCONNECTED:
            LOG_INFO("Handle event: NETWORK_DISCONNECTED");
            p_adv_post_state->flag_network_connected = false;
            break;
        case ADV_POST_SIG_NETWORK_CONNECTED:
            LOG_INFO("Handle event: NETWORK_CONNECTED");
            p_adv_post_state->flag_network_connected = true;
            break;
        case ADV_POST_SIG_TIME_SYNCHRONIZED:
            adv_post_handle_sig_time_synchronized(p_adv_post_state);
            break;
        case ADV_POST_SIG_RETRANSMIT:
            LOG_INFO("Got ADV_POST_SIG_RETRANSMIT");
            if ((!p_adv_post_state->flag_retransmission_disabled) && gw_cfg_get_http_use_http())
            {
                p_adv_post_state->flag_need_to_send_advs = true;
                os_signal_send(g_p_adv_post_sig, adv_post_conv_to_sig_num(ADV_POST_SIG_DO_ASYNC_COMM));
            }
            break;
        case ADV_POST_SIG_SEND_STATISTICS:
            LOG_INFO("Got ADV_POST_SIG_SEND_STATISTICS");
            adv_post_handle_sig_send_statistics(p_adv_post_state);
            break;
        case ADV_POST_SIG_DO_ASYNC_COMM:
            adv_post_do_async_comm(p_adv_post_state);
            break;
        case ADV_POST_SIG_DISABLE:
            p_adv_post_state->flag_retransmission_disabled = true;
            break;
        case ADV_POST_SIG_ENABLE:
            p_adv_post_state->flag_retransmission_disabled = false;
            break;
        case ADV_POST_SIG_NETWORK_WATCHDOG:
            adv_post_handle_sig_network_watchdog();
            break;
        case ADV_POST_SIG_TASK_WATCHDOG_FEED:
            adv_post_handle_sig_task_watchdog_feed();
            break;
        case ADV_POST_SIG_GW_CFG_READY:
            LOG_INFO("Got ADV_POST_SIG_GW_CFG_READY");
            ruuvi_send_nrf_settings();
            p_adv_post_state->flag_need_to_send_statistics = gw_cfg_get_http_stat_use_http_stat();
            adv_post_on_gw_cfg_change(p_adv_post_state);
            break;
        case ADV_POST_SIG_GW_CFG_CHANGED_RUUVI:
            LOG_INFO("Got ADV_POST_SIG_GW_CFG_CHANGED_RUUVI");
            ruuvi_send_nrf_settings();
            adv_post_on_gw_cfg_change(p_adv_post_state);
            break;
    }
    return flag_stop;
}

static void
adv_post_task(void)
{
    esp_log_level_set(TAG, ESP_LOG_INFO);

    if (!os_signal_register_cur_thread(g_p_adv_post_sig))
    {
        LOG_ERR("%s failed", "os_signal_register_cur_thread");
        return;
    }

    LOG_INFO("%s started", __func__);
    os_timer_sig_periodic_start(g_p_adv_post_timer_sig_network_watchdog);

    adv_post_wdt_add_and_start();

    adv_post_state_t adv_post_state = {
        .flag_network_connected       = false,
        .flag_async_comm_in_progress  = false,
        .flag_need_to_send_advs       = false,
        .flag_need_to_send_statistics = false,
        .flag_retransmission_disabled = false,
        .flag_use_timestamps          = false,
    };

    for (;;)
    {
        os_signal_events_t sig_events = { 0 };
        if (!os_signal_wait_with_timeout(g_p_adv_post_sig, OS_DELTA_TICKS_INFINITE, &sig_events))
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
    os_timer_sig_periodic_stop(g_p_adv_post_timer_sig_retransmit);
    os_timer_sig_periodic_delete(&g_p_adv_post_timer_sig_retransmit);
    os_timer_sig_periodic_stop(g_p_adv_post_timer_sig_send_statistics);
    os_timer_sig_periodic_delete(&g_p_adv_post_timer_sig_send_statistics);
    os_timer_sig_one_shot_stop(g_p_adv_post_timer_sig_do_async_comm);
    os_timer_sig_one_shot_delete(&g_p_adv_post_timer_sig_do_async_comm);
    os_timer_sig_periodic_stop(g_p_adv_post_timer_sig_network_watchdog);
    os_timer_sig_periodic_delete(&g_p_adv_post_timer_sig_network_watchdog);

    LOG_INFO("TaskWatchdog: Stop timer");
    os_timer_sig_periodic_stop(g_p_adv_post_timer_sig_watchdog_feed);
    LOG_INFO("TaskWatchdog: Delete timer");
    os_timer_sig_periodic_delete(&g_p_adv_post_timer_sig_watchdog_feed);

    os_signal_unregister_cur_thread(g_p_adv_post_sig);
    os_signal_delete(&g_p_adv_post_sig);
}

void
adv_post_init(void)
{
    g_p_adv_post_sig = os_signal_create_static(&g_adv_post_sig_mem);
    os_signal_add(g_p_adv_post_sig, adv_post_conv_to_sig_num(ADV_POST_SIG_STOP));
    os_signal_add(g_p_adv_post_sig, adv_post_conv_to_sig_num(ADV_POST_SIG_NETWORK_DISCONNECTED));
    os_signal_add(g_p_adv_post_sig, adv_post_conv_to_sig_num(ADV_POST_SIG_NETWORK_CONNECTED));
    os_signal_add(g_p_adv_post_sig, adv_post_conv_to_sig_num(ADV_POST_SIG_TIME_SYNCHRONIZED));
    os_signal_add(g_p_adv_post_sig, adv_post_conv_to_sig_num(ADV_POST_SIG_RETRANSMIT));
    os_signal_add(g_p_adv_post_sig, adv_post_conv_to_sig_num(ADV_POST_SIG_SEND_STATISTICS));
    os_signal_add(g_p_adv_post_sig, adv_post_conv_to_sig_num(ADV_POST_SIG_DO_ASYNC_COMM));
    os_signal_add(g_p_adv_post_sig, adv_post_conv_to_sig_num(ADV_POST_SIG_DISABLE));
    os_signal_add(g_p_adv_post_sig, adv_post_conv_to_sig_num(ADV_POST_SIG_ENABLE));
    os_signal_add(g_p_adv_post_sig, adv_post_conv_to_sig_num(ADV_POST_SIG_NETWORK_WATCHDOG));
    os_signal_add(g_p_adv_post_sig, adv_post_conv_to_sig_num(ADV_POST_SIG_TASK_WATCHDOG_FEED));
    os_signal_add(g_p_adv_post_sig, adv_post_conv_to_sig_num(ADV_POST_SIG_GW_CFG_READY));
    os_signal_add(g_p_adv_post_sig, adv_post_conv_to_sig_num(ADV_POST_SIG_GW_CFG_CHANGED_RUUVI));

    event_mgr_subscribe_sig_static(
        &g_adv_post_ev_info_mem_wifi_disconnected,
        EVENT_MGR_EV_WIFI_DISCONNECTED,
        g_p_adv_post_sig,
        adv_post_conv_to_sig_num(ADV_POST_SIG_NETWORK_DISCONNECTED));

    event_mgr_subscribe_sig_static(
        &g_adv_post_ev_info_mem_eth_disconnected,
        EVENT_MGR_EV_ETH_DISCONNECTED,
        g_p_adv_post_sig,
        adv_post_conv_to_sig_num(ADV_POST_SIG_NETWORK_DISCONNECTED));

    event_mgr_subscribe_sig_static(
        &g_adv_post_ev_info_mem_wifi_connected,
        EVENT_MGR_EV_WIFI_CONNECTED,
        g_p_adv_post_sig,
        adv_post_conv_to_sig_num(ADV_POST_SIG_NETWORK_CONNECTED));

    event_mgr_subscribe_sig_static(
        &g_adv_post_ev_info_mem_eth_connected,
        EVENT_MGR_EV_ETH_CONNECTED,
        g_p_adv_post_sig,
        adv_post_conv_to_sig_num(ADV_POST_SIG_NETWORK_CONNECTED));

    event_mgr_subscribe_sig_static(
        &g_adv_post_ev_info_mem_time_synchronized,
        EVENT_MGR_EV_TIME_SYNCHRONIZED,
        g_p_adv_post_sig,
        adv_post_conv_to_sig_num(ADV_POST_SIG_TIME_SYNCHRONIZED));

    event_mgr_subscribe_sig_static(
        &g_adv_post_ev_info_mem_cfg_ready,
        EVENT_MGR_EV_GW_CFG_READY,
        g_p_adv_post_sig,
        adv_post_conv_to_sig_num(ADV_POST_SIG_GW_CFG_READY));

    event_mgr_subscribe_sig_static(
        &g_adv_post_ev_info_mem_gw_cfg_ruuvi_changed,
        EVENT_MGR_EV_GW_CFG_CHANGED_RUUVI,
        g_p_adv_post_sig,
        adv_post_conv_to_sig_num(ADV_POST_SIG_GW_CFG_CHANGED_RUUVI));

    g_p_adv_post_timer_sig_retransmit = os_timer_sig_periodic_create_static(
        &g_adv_post_timer_sig_retransmit_mem,
        "adv_post_retransmit",
        g_p_adv_post_sig,
        adv_post_conv_to_sig_num(ADV_POST_SIG_RETRANSMIT),
        pdMS_TO_TICKS(ADV_POST_DEFAULT_INTERVAL_SECONDS * TIME_UNITS_MS_PER_SECOND));

    g_p_adv_post_timer_sig_send_statistics = os_timer_sig_periodic_create_static(
        &g_adv_post_timer_sig_send_statistics_mem,
        "adv_post_send_status",
        g_p_adv_post_sig,
        adv_post_conv_to_sig_num(ADV_POST_SIG_SEND_STATISTICS),
        pdMS_TO_TICKS(ADV_POST_STATISTICS_INTERVAL_SECONDS) * TIME_UNITS_MS_PER_SECOND);

    g_p_adv_post_timer_sig_do_async_comm = os_timer_sig_one_shot_create_static(
        &g_adv_post_timer_sig_do_async_comm_mem,
        "adv_post_async",
        g_p_adv_post_sig,
        adv_post_conv_to_sig_num(ADV_POST_SIG_DO_ASYNC_COMM),
        pdMS_TO_TICKS(ADV_POST_DO_ASYNC_COMM_INTERVAL_MS));

    g_p_adv_post_timer_sig_network_watchdog = os_timer_sig_periodic_create_static(
        &g_adv_post_timer_sig_network_watchdog_mem,
        "adv_post_watchdog",
        g_p_adv_post_sig,
        adv_post_conv_to_sig_num(ADV_POST_SIG_NETWORK_WATCHDOG),
        pdMS_TO_TICKS(RUUVI_NETWORK_WATCHDOG_PERIOD_SECONDS * TIME_UNITS_MS_PER_SECOND));

    LOG_INFO("TaskWatchdog: adv_post: Create timer");
    g_p_adv_post_timer_sig_watchdog_feed = os_timer_sig_periodic_create_static(
        &g_adv_post_timer_sig_watchdog_feed_mem,
        "adv_post:wdog",
        g_p_adv_post_sig,
        adv_post_conv_to_sig_num(ADV_POST_SIG_TASK_WATCHDOG_FEED),
        pdMS_TO_TICKS(CONFIG_ESP_TASK_WDT_TIMEOUT_S * TIME_UNITS_MS_PER_SECOND / 3U));

    g_adv_post_nonce = esp_random();
    adv_table_init();
    api_callbacks_reg((void *)&adv_callback_func_tbl);
    const uint32_t stack_size = 1024U * 4U;
    if (!os_task_create_finite_without_param(&adv_post_task, "adv_post_task", stack_size, 1))
    {
        LOG_ERR("Can't create thread");
    }
}

void
adv_post_set_period(const uint32_t period_ms)
{
    if (period_ms != g_adv_post_interval_ms)
    {
        LOG_INFO(
            "X-Ruuvi-Gateway-Rate: Change period from %u ms to %u ms",
            (printf_uint_t)g_adv_post_interval_ms,
            (printf_uint_t)period_ms);
        g_adv_post_interval_ms = period_ms;
        adv_post_timer_restart();
    }
}

void
adv_post_stop(void)
{
    LOG_INFO("adv_post_stop");
    if (!os_signal_send(g_p_adv_post_sig, adv_post_conv_to_sig_num(ADV_POST_SIG_STOP)))
    {
        LOG_ERR("%s failed", "os_signal_send");
    }
}

void
adv_post_disable_retransmission(void)
{
    LOG_INFO("adv_post_disable_retransmission");
    if (!os_signal_send(g_p_adv_post_sig, adv_post_conv_to_sig_num(ADV_POST_SIG_DISABLE)))
    {
        LOG_ERR("%s failed", "os_signal_send");
    }
}

void
adv_post_enable_retransmission(void)
{
    LOG_INFO("adv_post_enable_retransmission");
    if (!os_signal_send(g_p_adv_post_sig, adv_post_conv_to_sig_num(ADV_POST_SIG_ENABLE)))
    {
        LOG_ERR("%s failed", "os_signal_send");
    }
}
