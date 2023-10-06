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
#include "reset_task.h"
#include "time_units.h"
#include "gw_status.h"
#include "leds.h"
#include "reset_info.h"
#include "reset_reason.h"
#include "runtime_stat.h"
#include "hmac_sha256.h"

#define LOG_LOCAL_LEVEL LOG_LEVEL_INFO
#include "log.h"

#define ADV_POST_TASK_WATCHDOG_FEEDING_PERIOD_TICKS pdMS_TO_TICKS(1000)
#define ADV_POST_TASK_LED_UPDATE_PERIOD_TICKS       pdMS_TO_TICKS(1000)
#define ADV_POST_TASK_RECV_ADV_TIMEOUT_TICKS        pdMS_TO_TICKS(10 * 1000)

#define ADV_POST_GREEN_LED_ON_INTERVAL_MS (1500)

#define ADV_POST_DELAY_BEFORE_RETRYING_POST_AFTER_ERROR_MS   (67 * 1000)
#define ADV_POST_DELAY_AFTER_CONFIGURATION_CHANGED_MS        (5 * 1000)
#define ADV_POST_INITIAL_DELAY_IN_SENDING_STATISTICS_SECONDS (60)

typedef enum adv_post_sig_e
{
    ADV_POST_SIG_STOP                        = OS_SIGNAL_NUM_0,
    ADV_POST_SIG_NETWORK_DISCONNECTED        = OS_SIGNAL_NUM_1,
    ADV_POST_SIG_NETWORK_CONNECTED           = OS_SIGNAL_NUM_2,
    ADV_POST_SIG_TIME_SYNCHRONIZED           = OS_SIGNAL_NUM_3,
    ADV_POST_SIG_RETRANSMIT                  = OS_SIGNAL_NUM_4,
    ADV_POST_SIG_RETRANSMIT2                 = OS_SIGNAL_NUM_5,
    ADV_POST_SIG_ACTIVATE_SENDING_STATISTICS = OS_SIGNAL_NUM_6,
    ADV_POST_SIG_SEND_STATISTICS             = OS_SIGNAL_NUM_7,
    ADV_POST_SIG_DO_ASYNC_COMM               = OS_SIGNAL_NUM_8,
    ADV_POST_SIG_RELAYING_MODE_CHANGED       = OS_SIGNAL_NUM_9,
    ADV_POST_SIG_NETWORK_WATCHDOG            = OS_SIGNAL_NUM_10,
    ADV_POST_SIG_TASK_WATCHDOG_FEED          = OS_SIGNAL_NUM_11,
    ADV_POST_SIG_GW_CFG_READY                = OS_SIGNAL_NUM_12,
    ADV_POST_SIG_GW_CFG_CHANGED_RUUVI        = OS_SIGNAL_NUM_13,
    ADV_POST_SIG_BLE_SCAN_CHANGED            = OS_SIGNAL_NUM_14,
    ADV_POST_SIG_ACTIVATE_CFG_MODE           = OS_SIGNAL_NUM_15,
    ADV_POST_SIG_DEACTIVATE_CFG_MODE         = OS_SIGNAL_NUM_16,
    ADV_POST_SIG_GREEN_LED_TURN_ON           = OS_SIGNAL_NUM_17,
    ADV_POST_SIG_GREEN_LED_TURN_OFF          = OS_SIGNAL_NUM_18,
    ADV_POST_SIG_GREEN_LED_UPDATE            = OS_SIGNAL_NUM_19,
    ADV_POST_SIG_RECV_ADV_TIMEOUT            = OS_SIGNAL_NUM_20,
} adv_post_sig_e;

typedef struct adv_post_state_t
{
    bool flag_primary_time_sync_is_done;
    bool flag_network_connected;
    bool flag_async_comm_in_progress;
    bool flag_need_to_send_advs1;
    bool flag_need_to_send_advs2;
    bool flag_need_to_send_statistics;
    bool flag_relaying_enabled;
    bool flag_use_timestamps;
} adv_post_state_t;

typedef struct adv_post_timer_t
{
    uint32_t                       num;
    uint32_t                       default_interval_ms;
    uint32_t                       cur_interval_ms;
    os_timer_sig_periodic_t*       p_timer_sig;
    os_timer_sig_periodic_static_t timer_sig_mem;
} adv_post_timer_t;

typedef struct adv_post_cfg_cache_t
{
    bool flag_use_mqtt;
    bool flag_use_ntp;

    bool               scan_filter_allow_listed;
    uint32_t           scan_filter_length;
    mac_address_bin_t* p_arr_of_scan_filter_mac;
} adv_post_cfg_cache_t;

typedef enum adv_post_action_e
{
    ADV_POST_ACTION_NONE = 0,
    ADV_POST_ACTION_POST_ADVS_TO_RUUVI,
    ADV_POST_ACTION_POST_ADVS_TO_CUSTOM,
    ADV_POST_ACTION_POST_STATS,
} adv_post_action_e;

#define ADV_POST_SIG_FIRST (ADV_POST_SIG_STOP)
#define ADV_POST_SIG_LAST  (ADV_POST_SIG_RECV_ADV_TIMEOUT)

static void
adv_post_send_report(void* p_arg);

static void
adv_post_send_ack(void* arg);

static void
adv_post_cb_on_recv_device_id(void* arg);

static void
adv_post_send_get_all(void* arg);

static void
adv_post_unsubscribe_events(void);

static void
adv_post_delete_timers(void);

static const char* TAG = "ADV_POST_TASK";

static adv_callbacks_fn_t adv_callback_func_tbl = {
    .AdvAckCallback    = adv_post_send_ack,
    .AdvReportCallback = adv_post_send_report,
    .AdvIdCallback     = adv_post_cb_on_recv_device_id,
    .AdvGetAllCallback = adv_post_send_get_all,
};

static adv_post_cfg_cache_t g_adv_post_cfg_cache;
static os_mutex_t           g_p_adv_post_cfg_access_mutex;
static os_mutex_static_t    g_adv_post_cfg_access_mutex_mem;

static uint32_t                       g_adv_post_nonce;
static os_signal_t*                   g_p_adv_post_sig;
static os_signal_static_t             g_adv_post_sig_mem;
static os_timer_sig_one_shot_t*       g_p_adv_post_timer_sig_activate_sending_statistics;
static os_timer_sig_one_shot_static_t g_p_adv_post_timer_sig_activate_sending_statistics_mem;
static os_timer_sig_periodic_t*       g_p_adv_post_timer_sig_send_statistics;
static os_timer_sig_periodic_static_t g_adv_post_timer_sig_send_statistics_mem;
static os_timer_sig_one_shot_t*       g_p_adv_post_timer_sig_do_async_comm;
static os_timer_sig_one_shot_static_t g_adv_post_timer_sig_do_async_comm_mem;
static os_timer_sig_periodic_t*       g_p_adv_post_timer_sig_watchdog_feed;
static os_timer_sig_periodic_static_t g_adv_post_timer_sig_watchdog_feed_mem;
static os_timer_sig_periodic_t*       g_p_adv_post_timer_sig_green_led_update;
static os_timer_sig_periodic_static_t g_adv_post_timer_sig_green_led_update_mem;
static os_timer_sig_one_shot_t*       g_p_adv_post_timer_sig_recv_adv_timeout;
static os_timer_sig_one_shot_static_t g_adv_post_timer_sig_recv_adv_timeout_mem;
static TickType_t                     g_adv_post_last_successful_network_comm_timestamp;
static os_mutex_t                     g_p_adv_post_last_successful_network_comm_mutex;
static os_mutex_static_t              g_adv_post_last_successful_network_comm_mutex_mem;
static os_timer_sig_periodic_t*       g_p_adv_post_timer_sig_network_watchdog;
static os_timer_sig_periodic_static_t g_adv_post_timer_sig_network_watchdog_mem;
static event_mgr_ev_info_static_t     g_adv_post_ev_info_mem_wifi_disconnected;
static event_mgr_ev_info_static_t     g_adv_post_ev_info_mem_eth_disconnected;
static event_mgr_ev_info_static_t     g_adv_post_ev_info_mem_wifi_connected;
static event_mgr_ev_info_static_t     g_adv_post_ev_info_mem_eth_connected;
static event_mgr_ev_info_static_t     g_adv_post_ev_info_mem_time_synchronized;
static event_mgr_ev_info_static_t     g_adv_post_ev_info_mem_cfg_ready;
static event_mgr_ev_info_static_t     g_adv_post_ev_info_mem_gw_cfg_ruuvi_changed;
static event_mgr_ev_info_static_t     g_adv_post_ev_info_mem_relaying_mode_changed;
static event_mgr_ev_info_static_t     g_adv_post_ev_info_mem_green_led_turn_on;
static event_mgr_ev_info_static_t     g_adv_post_ev_info_mem_green_led_turn_off;

static bool g_adv_post_green_led_state = false;

static adv_post_action_e g_adv_post_action = ADV_POST_ACTION_NONE;

static adv_post_timer_t g_adv_post_timers[2] = {
    {
        .num                 = 1,
        .default_interval_ms = ADV_POST_DEFAULT_INTERVAL_SECONDS * TIME_UNITS_MS_PER_SECOND,
        .cur_interval_ms     = 0,
        .p_timer_sig         = NULL,
        .timer_sig_mem       = {},
    },
    {
        .num                 = 2,
        .default_interval_ms = ADV_POST_DEFAULT_INTERVAL_SECONDS * TIME_UNITS_MS_PER_SECOND,
        .cur_interval_ms     = 0,
        .p_timer_sig         = NULL,
        .timer_sig_mem       = {},
    },
};

static uint32_t g_adv_post_malloc_fail_cnt[2];

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
u64_to_array(const uint64_t u64, uint8_t* const p_array, const uint8_t num_bytes)
{
    const uint8_t offset = num_bytes - 1;
    uint8_t       cnt    = num_bytes;
    while (0 != cnt)
    {
        cnt -= 1;
        p_array[offset - cnt] = (u64 >> (RUUVI_BITS_PER_BYTE * cnt)) & RUUVI_BYTE_MASK;
    }
}

static adv_post_cfg_cache_t*
adv_post_cfg_access_mutex_try_lock(void)
{
    if (NULL == g_p_adv_post_cfg_access_mutex)
    {
        g_p_adv_post_cfg_access_mutex = os_mutex_create_static(&g_adv_post_cfg_access_mutex_mem);
    }
    if (!os_mutex_try_lock(g_p_adv_post_cfg_access_mutex))
    {
        return NULL;
    }
    return &g_adv_post_cfg_cache;
}

static adv_post_cfg_cache_t*
adv_post_cfg_access_mutex_lock(void)
{
    if (NULL == g_p_adv_post_cfg_access_mutex)
    {
        g_p_adv_post_cfg_access_mutex = os_mutex_create_static(&g_adv_post_cfg_access_mutex_mem);
    }
    os_mutex_lock(g_p_adv_post_cfg_access_mutex);
    return &g_adv_post_cfg_cache;
}

static void
adv_post_cfg_access_mutex_unlock(adv_post_cfg_cache_t** p_p_cfg_cache)
{
    *p_p_cfg_cache = NULL;
    os_mutex_unlock(g_p_adv_post_cfg_access_mutex);
}

static esp_err_t
adv_put_to_table(const adv_report_t* const p_adv)
{
    metrics_received_advs_increment();
    if (!adv_table_put(p_adv))
    {
        return ESP_ERR_NO_MEM;
    }
    return ESP_OK;
}

static bool
parse_adv_report_from_uart(const re_ca_uart_payload_t* const p_msg, const time_t timestamp, adv_report_t* const p_adv)
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
    const re_ca_uart_ble_adv_t* const p_report = &(p_msg->params.adv);
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
adv_post_send_ack(void* arg)
{
    (void)arg;
    // Do something
}

static void
adv_post_cb_on_recv_device_id(void* p_arg)
{
    const re_ca_uart_payload_t* const p_uart_payload = (re_ca_uart_payload_t*)p_arg;

    nrf52_device_id_t nrf52_device_id = { 0 };
    u64_to_array(p_uart_payload->params.device_id.id, &nrf52_device_id.id[0], sizeof(nrf52_device_id.id));

    mac_address_bin_t nrf52_mac_addr = { 0 };
    u64_to_array(p_uart_payload->params.device_id.addr, &nrf52_mac_addr.mac[0], sizeof(nrf52_mac_addr.mac));

    LOG_INFO("nRF52 DEVICE ID : 0x%016llx", p_uart_payload->params.device_id.id);
    LOG_INFO("nRF52 MAC ADDR  : 0x%016llx", p_uart_payload->params.device_id.addr);

    ruuvi_device_id_set(&nrf52_device_id, &nrf52_mac_addr);
}

static bool
adv_post_check_if_mac_filtered_out(
    const adv_post_cfg_cache_t* const p_cfg_cache,
    const mac_address_bin_t* const    p_mac_addr)
{
    if (0 == p_cfg_cache->scan_filter_length)
    {
        return false;
    }
    bool flag_is_filtered_out = false;
    if (p_cfg_cache->scan_filter_allow_listed)
    {
        for (uint32_t i = 0; i < p_cfg_cache->scan_filter_length; ++i)
        {
            if (0 == memcmp(p_mac_addr, &p_cfg_cache->p_arr_of_scan_filter_mac[i], sizeof(*p_mac_addr)))
            {
                return false;
            }
        }
        flag_is_filtered_out = true;
    }
    else
    {
        for (uint32_t i = 0; i < p_cfg_cache->scan_filter_length; ++i)
        {
            if (0 == memcmp(p_mac_addr, &p_cfg_cache->p_arr_of_scan_filter_mac[i], sizeof(*p_mac_addr)))
            {
                return true;
            }
        }
    }
    return flag_is_filtered_out;
}

static void
adv_post_send_report(void* p_arg)
{
    if (!gw_cfg_is_initialized())
    {
        LOG_WARN("Drop adv - gw_cfg is not ready yet");
        return;
    }

    adv_post_cfg_cache_t* p_cfg_cache = adv_post_cfg_access_mutex_try_lock();
    if (NULL == p_cfg_cache)
    {
        LOG_WARN("Drop adv - gateway in the process of reconfiguration");
        return;
    }

    const bool   flag_ntp_use              = p_cfg_cache->flag_use_ntp;
    const time_t timestamp_if_synchronized = time_is_synchronized() ? time(NULL) : 0;
    const time_t timestamp = flag_ntp_use ? timestamp_if_synchronized : (time_t)metrics_received_advs_get();

    adv_report_t adv_report = { 0 };
    if (!parse_adv_report_from_uart((re_ca_uart_payload_t*)p_arg, timestamp, &adv_report))
    {
        LOG_WARN("Drop adv - parsing failed");
        adv_post_cfg_access_mutex_unlock(&p_cfg_cache);
        return;
    }

    if (adv_post_check_if_mac_filtered_out(p_cfg_cache, &adv_report.tag_mac))
    {
        adv_post_cfg_access_mutex_unlock(&p_cfg_cache);
        return;
    }

    os_timer_sig_one_shot_stop(g_p_adv_post_timer_sig_recv_adv_timeout);
    os_timer_sig_one_shot_start(g_p_adv_post_timer_sig_recv_adv_timeout);
    event_mgr_notify(EVENT_MGR_EV_RECV_ADV);

    LOG_DUMP_VERBOSE(
        adv_report.data_buf,
        adv_report.data_len,
        "Recv Adv: MAC=%s, ID=0x%02x%02x, time=%lu",
        mac_address_to_str(&adv_report.tag_mac).str_buf,
        adv_report.data_buf[6],
        adv_report.data_buf[5],
        (printf_ulong_t)timestamp);

    const esp_err_t ret = adv_put_to_table(&adv_report);
    if (ESP_ERR_NO_MEM == ret)
    {
        LOG_WARN("Drop adv - table full");
    }
    if (p_cfg_cache->flag_use_mqtt && gw_status_is_mqtt_connected())
    {
        if (mqtt_publish_adv(&adv_report, flag_ntp_use, (flag_ntp_use ? time(NULL) : 0)))
        {
            adv_post_last_successful_network_comm_timestamp_update();
        }
        else
        {
            LOG_ERR("%s failed", "mqtt_publish_adv");
        }
    }
    adv_post_cfg_access_mutex_unlock(&p_cfg_cache);
}

static void
adv_post_on_green_led_update(void)
{
    if (!os_timer_sig_periodic_is_active(g_p_adv_post_timer_sig_green_led_update))
    {
        LOG_INFO("GREEN LED: Activate periodic updating");
        os_timer_sig_periodic_start(g_p_adv_post_timer_sig_green_led_update);
    }

    if (api_send_led_ctrl(g_adv_post_green_led_state ? ADV_POST_GREEN_LED_ON_INTERVAL_MS : 0) < 0)
    {
        LOG_ERR("%s failed", "api_send_led_ctrl");
    }
}

static void
adv_post_send_get_all(void* arg)
{
    (void)arg;
    LOG_INFO("Got configuration request from NRF52");
    adv_post_on_green_led_update();
    ruuvi_send_nrf_settings_from_gw_cfg();
}

static void
adv_post_log(const adv_report_table_t* p_reports, const bool flag_use_timestamps)
{
    LOG_INFO("Advertisements in table: %u", (printf_uint_t)p_reports->num_of_advs);
    for (num_of_advs_t i = 0; i < p_reports->num_of_advs; ++i)
    {
        const adv_report_t* p_adv = &p_reports->table[i];

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
    os_timer_sig_one_shot_start(g_p_adv_post_timer_sig_do_async_comm);
    return true;
}

static bool
adv_post_do_retransmission(const bool flag_use_timestamps, const bool flag_post_to_ruuvi)
{
    adv_report_table_t* p_adv_reports_buf = os_calloc(1, sizeof(*p_adv_reports_buf));
    if (NULL == p_adv_reports_buf)
    {
        LOG_ERR("Can't allocate memory");
        return false;
    }

    // for thread safety copy the advertisements to a separate buffer for posting
    if (flag_post_to_ruuvi)
    {
        adv_table_read_retransmission_list1_and_clear(p_adv_reports_buf);
    }
    else
    {
        adv_table_read_retransmission_list2_and_clear(p_adv_reports_buf);
    }

    adv_post_log(p_adv_reports_buf, flag_use_timestamps);

    const bool res = adv_post_retransmit_advs(p_adv_reports_buf, flag_use_timestamps, flag_post_to_ruuvi);

    os_free(p_adv_reports_buf);

    return res;
}

http_json_statistics_info_t*
adv_post_generate_statistics_info(const str_buf_t* const p_reset_info)
{
    http_json_statistics_info_t* p_stat_info = os_calloc(1, sizeof(*p_stat_info));
    if (NULL == p_stat_info)
    {
        return NULL;
    }

    p_stat_info->nrf52_mac_addr         = *gw_cfg_get_nrf52_mac_addr();
    p_stat_info->esp_fw                 = *gw_cfg_get_esp32_fw_ver();
    p_stat_info->nrf_fw                 = *gw_cfg_get_nrf52_fw_ver();
    p_stat_info->uptime                 = g_uptime_counter;
    p_stat_info->nonce                  = g_adv_post_nonce;
    p_stat_info->nrf_status             = gw_status_get_nrf_status();
    p_stat_info->is_connected_to_wifi   = wifi_manager_is_connected_to_wifi();
    p_stat_info->network_disconnect_cnt = g_network_disconnect_cnt;
    (void)sniprintf(
        p_stat_info->reset_reason.buf,
        sizeof(p_stat_info->reset_reason.buf),
        "%s",
        reset_reason_to_str(esp_reset_reason()));
    p_stat_info->reset_cnt    = reset_info_get_cnt();
    p_stat_info->p_reset_info = (NULL != p_reset_info) ? &p_reset_info->buf[0] : "";

    g_adv_post_nonce += 1;

    return p_stat_info;
}

static bool
adv_post_stat(const ruuvi_gw_cfg_http_stat_t* const p_cfg_http_stat, void* const p_user_data)
{
    log_runtime_statistics();

    adv_report_table_t* p_reports = os_malloc(sizeof(*p_reports));
    if (NULL == p_reports)
    {
        LOG_ERR("Can't allocate memory for statistics");
        return false;
    }
    adv_table_statistics_read(p_reports);

    str_buf_t                          reset_info  = reset_info_get();
    const http_json_statistics_info_t* p_stat_info = adv_post_generate_statistics_info(&reset_info);
    if (NULL == p_stat_info)
    {
        LOG_ERR("Can't allocate memory");
        str_buf_free_buf(&reset_info);
        os_free(p_reports);
        return false;
    }

    const bool res = http_post_stat(
        p_stat_info,
        p_reports,
        p_cfg_http_stat,
        p_user_data,
        p_cfg_http_stat->http_stat_use_ssl_client_cert,
        p_cfg_http_stat->http_stat_use_ssl_server_cert);
    os_free(p_stat_info);
    str_buf_free_buf(&reset_info);
    os_free(p_reports);

    return res;
}

static bool
adv_post_do_send_statistics(void)
{
    const ruuvi_gw_cfg_http_stat_t* p_cfg_http_stat = gw_cfg_get_http_stat_copy();
    if (NULL == p_cfg_http_stat)
    {
        LOG_ERR("%s failed", "gw_cfg_get_http_copy");
        return false;
    }
    const bool res = adv_post_stat(p_cfg_http_stat, NULL);
    os_free(p_cfg_http_stat);

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

static bool
adv_post_do_async_comm_send_advs(adv_post_state_t* const p_adv_post_state, const bool flag_post_to_ruuvi)
{
    if (!p_adv_post_state->flag_network_connected)
    {
        LOG_DBG("Can't send advs, no network connection");
        if (flag_post_to_ruuvi)
        {
            leds_notify_http1_data_sent_fail();
        }
        else
        {
            leds_notify_http2_data_sent_fail();
        }
        return false;
    }
    if (p_adv_post_state->flag_use_timestamps && (!time_is_synchronized()))
    {
        LOG_DBG("Can't send advs, the time is not yet synchronized");
        if (flag_post_to_ruuvi)
        {
            leds_notify_http1_data_sent_fail();
        }
        else
        {
            leds_notify_http2_data_sent_fail();
        }
        return false;
    }
    if (flag_post_to_ruuvi)
    {
        p_adv_post_state->flag_need_to_send_advs1 = false;
    }
    else
    {
        p_adv_post_state->flag_need_to_send_advs2 = false;
    }
    if (adv_post_do_retransmission(p_adv_post_state->flag_use_timestamps, flag_post_to_ruuvi))
    {
        p_adv_post_state->flag_async_comm_in_progress = true;
    }
    g_adv_post_nonce += 1;
    return p_adv_post_state->flag_async_comm_in_progress;
}

static bool
adv_post_do_async_comm_send_statistics(adv_post_state_t* const p_adv_post_state)
{
    if (!gw_cfg_get_http_stat_use_http_stat())
    {
        LOG_WARN("Can't send statistics, it was disabled in gw_cfg");
        p_adv_post_state->flag_need_to_send_statistics = false;
        return false;
    }
    if (!p_adv_post_state->flag_network_connected)
    {
        LOG_WARN("Can't send statistics, no network connection");
        return false;
    }
    if (p_adv_post_state->flag_use_timestamps && (!time_is_synchronized()))
    {
        LOG_WARN("Can't send statistics, the time is not yet synchronized");
        return false;
    }
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
    return p_adv_post_state->flag_async_comm_in_progress;
}

static void
adv_post_do_async_comm(adv_post_state_t* const p_adv_post_state)
{
    LOG_DBG("flag_async_comm_in_progress=%d", p_adv_post_state->flag_async_comm_in_progress);
    if (p_adv_post_state->flag_async_comm_in_progress)
    {
        if (http_async_poll(
                &g_adv_post_malloc_fail_cnt[ADV_POST_ACTION_POST_ADVS_TO_RUUVI == g_adv_post_action ? 0 : 1]))
        {
            p_adv_post_state->flag_async_comm_in_progress = false;
            LOG_DBG("http_server_mutex_unlock");
            http_server_mutex_unlock();

            const uint32_t free_heap = esp_get_free_heap_size();
            LOG_INFO("Cur free heap: %lu", (printf_ulong_t)free_heap);
        }
        else
        {
            LOG_DBG("os_timer_sig_one_shot_start: g_p_adv_post_timer_sig_do_async_comm");
            os_timer_sig_one_shot_start(g_p_adv_post_timer_sig_do_async_comm);
        }
    }
    if ((!p_adv_post_state->flag_async_comm_in_progress) && p_adv_post_state->flag_need_to_send_advs1
        && p_adv_post_state->flag_relaying_enabled)
    {
        LOG_DBG("http_server_mutex_try_lock");
        if (!http_server_mutex_try_lock())
        {
            LOG_DBG("http_server_mutex_try_lock failed, postpone sending advs1");
            os_timer_sig_one_shot_start(g_p_adv_post_timer_sig_do_async_comm);
            return;
        }
        g_adv_post_action             = ADV_POST_ACTION_POST_ADVS_TO_RUUVI;
        const bool flag_send_to_ruuvi = true;
        if (!adv_post_do_async_comm_send_advs(p_adv_post_state, flag_send_to_ruuvi))
        {
            g_adv_post_action = ADV_POST_ACTION_NONE;
            LOG_DBG("http_server_mutex_unlock");
            http_server_mutex_unlock();
        }
    }
    if ((!p_adv_post_state->flag_async_comm_in_progress) && p_adv_post_state->flag_need_to_send_advs2
        && p_adv_post_state->flag_relaying_enabled)
    {
        LOG_DBG("http_server_mutex_try_lock");
        if (!http_server_mutex_try_lock())
        {
            LOG_DBG("http_server_mutex_try_lock failed, postpone sending advs2");
            os_timer_sig_one_shot_start(g_p_adv_post_timer_sig_do_async_comm);
            return;
        }
        g_adv_post_action             = ADV_POST_ACTION_POST_ADVS_TO_CUSTOM;
        const bool flag_send_to_ruuvi = false;
        if (!adv_post_do_async_comm_send_advs(p_adv_post_state, flag_send_to_ruuvi))
        {
            g_adv_post_action = ADV_POST_ACTION_NONE;
            LOG_DBG("http_server_mutex_unlock");
            http_server_mutex_unlock();
        }
    }
    if ((!p_adv_post_state->flag_async_comm_in_progress) && p_adv_post_state->flag_need_to_send_statistics
        && p_adv_post_state->flag_relaying_enabled)
    {
        LOG_DBG("http_server_mutex_try_lock");
        if (!http_server_mutex_try_lock())
        {
            LOG_DBG("http_server_mutex_try_lock failed, postpone sending statistics");
            os_timer_sig_one_shot_start(g_p_adv_post_timer_sig_do_async_comm);
            return;
        }
        g_adv_post_action = ADV_POST_ACTION_POST_STATS;
        if (!adv_post_do_async_comm_send_statistics(p_adv_post_state))
        {
            g_adv_post_action = ADV_POST_ACTION_NONE;
            LOG_DBG("http_server_mutex_unlock");
            http_server_mutex_unlock();
        }
    }
}

static void
adv_post_last_successful_network_comm_timestamp_lock(void)
{
    if (NULL == g_p_adv_post_last_successful_network_comm_mutex)
    {
        g_p_adv_post_last_successful_network_comm_mutex = os_mutex_create_static(
            &g_adv_post_last_successful_network_comm_mutex_mem);
    }
    os_mutex_lock(g_p_adv_post_last_successful_network_comm_mutex);
}

static void
adv_post_last_successful_network_comm_timestamp_unlock(void)
{
    os_mutex_unlock(g_p_adv_post_last_successful_network_comm_mutex);
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
        gateway_restart("Network watchdog");
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
adv_post_restart_pending_retransmissions(const adv_post_state_t* const p_adv_post_state)
{
    if (p_adv_post_state->flag_need_to_send_advs1)
    {
        LOG_INFO("Force pending advs1 retransmission");
        os_timer_sig_periodic_t* const p_timer_sig = g_adv_post_timers[0].p_timer_sig;
        os_timer_sig_periodic_stop(p_timer_sig);
        os_timer_sig_periodic_start(p_timer_sig);
        os_timer_sig_periodic_simulate(p_timer_sig);
    }
    if (p_adv_post_state->flag_need_to_send_advs2)
    {
        LOG_INFO("Force pending advs2 retransmission");
        os_timer_sig_periodic_t* const p_timer_sig = g_adv_post_timers[1].p_timer_sig;
        os_timer_sig_periodic_stop(p_timer_sig);
        os_timer_sig_periodic_start(p_timer_sig);
        os_timer_sig_periodic_simulate(p_timer_sig);
    }
    if (p_adv_post_state->flag_need_to_send_statistics)
    {
        LOG_INFO("Force pending statistics retransmission");
        os_timer_sig_periodic_t* const p_timer_sig = g_p_adv_post_timer_sig_send_statistics;
        os_timer_sig_periodic_stop(p_timer_sig);
        os_timer_sig_periodic_start(p_timer_sig);
        os_timer_sig_periodic_simulate(p_timer_sig);
    }
}

static void
adv_post_disable_ble_filtering(void)
{
    adv_post_cfg_cache_t* p_cfg_cache = adv_post_cfg_access_mutex_lock();
    if (NULL != p_cfg_cache->p_arr_of_scan_filter_mac)
    {
        p_cfg_cache->scan_filter_length = 0;
        os_free(p_cfg_cache->p_arr_of_scan_filter_mac);
        p_cfg_cache->p_arr_of_scan_filter_mac = NULL;
    }
    adv_post_cfg_access_mutex_unlock(&p_cfg_cache);

    LOG_INFO("Clear adv_table");
    adv_table_clear();
}

static void
adv_post_on_gw_cfg_change(adv_post_state_t* const p_adv_post_state)
{
    p_adv_post_state->flag_use_timestamps = gw_cfg_get_ntp_use();

    adv_post_cfg_cache_t* p_cfg_cache = adv_post_cfg_access_mutex_lock();

    p_cfg_cache->flag_use_mqtt = gw_cfg_get_mqtt_use_mqtt();
    p_cfg_cache->flag_use_ntp  = p_adv_post_state->flag_use_timestamps;
    if (NULL != p_cfg_cache->p_arr_of_scan_filter_mac)
    {
        p_cfg_cache->scan_filter_length = 0;
        os_free(p_cfg_cache->p_arr_of_scan_filter_mac);
        p_cfg_cache->p_arr_of_scan_filter_mac = NULL;
    }

    const gw_cfg_t*                         p_gw_cfg      = gw_cfg_lock_ro();
    const ruuvi_gw_cfg_scan_filter_t* const p_scan_filter = &p_gw_cfg->ruuvi_cfg.scan_filter;

    p_cfg_cache->scan_filter_allow_listed = p_scan_filter->scan_filter_allow_listed;

    if (0 != p_scan_filter->scan_filter_length)
    {
        p_cfg_cache->p_arr_of_scan_filter_mac = os_calloc(
            p_scan_filter->scan_filter_length,
            sizeof(*p_cfg_cache->p_arr_of_scan_filter_mac));
        if (NULL == p_cfg_cache->p_arr_of_scan_filter_mac)
        {
            LOG_ERR("Can't allocate memory for scan_filter");
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

    gw_cfg_unlock_ro(&p_gw_cfg);

    if (gw_cfg_get_http_use_http_ruuvi())
    {
        LOG_INFO("Start timer for advs1 retransmission");
        adv1_post_timer_restart_with_short_period();
    }
    else
    {
        LOG_INFO("Stop timer for advs1 retransmission");
        os_timer_sig_periodic_stop(g_adv_post_timers[0].p_timer_sig);
        p_adv_post_state->flag_need_to_send_advs1 = false;
    }
    if (gw_cfg_get_http_use_http())
    {
        LOG_INFO("Start timer for advs2 retransmission");
        adv2_post_timer_restart_with_short_period();
    }
    else
    {
        LOG_INFO("Stop timer for advs2 retransmission");
        os_timer_sig_periodic_stop(g_adv_post_timers[1].p_timer_sig);
        p_adv_post_state->flag_need_to_send_advs2 = false;
    }
    if (gw_cfg_get_http_stat_use_http_stat())
    {
        LOG_INFO("Start timer to send statistics");
        os_timer_sig_periodic_start(g_p_adv_post_timer_sig_send_statistics);
        os_timer_sig_one_shot_restart(
            g_p_adv_post_timer_sig_activate_sending_statistics,
            pdMS_TO_TICKS(ADV_POST_DELAY_AFTER_CONFIGURATION_CHANGED_MS));
    }
    else
    {
        LOG_INFO("Stop timer to send statistics");
        os_timer_sig_periodic_stop(g_p_adv_post_timer_sig_send_statistics);
        os_timer_sig_one_shot_stop(g_p_adv_post_timer_sig_activate_sending_statistics);
        p_adv_post_state->flag_need_to_send_statistics = false;
    }
    adv_post_cfg_access_mutex_unlock(&p_cfg_cache);

    LOG_INFO("Clear adv_table");
    adv_table_clear();
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
        os_timer_sig_one_shot_stop(g_p_adv_post_timer_sig_do_async_comm);
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
adv_post_handle_sig_activate_sending_statistics(adv_post_state_t* const p_adv_post_state)
{
    LOG_INFO("Got ADV_POST_SIG_ACTIVATE_SENDING_STATISTICS");
    p_adv_post_state->flag_need_to_send_statistics = true;
    os_timer_sig_periodic_start(g_p_adv_post_timer_sig_send_statistics);
    os_timer_sig_periodic_simulate(g_p_adv_post_timer_sig_send_statistics);
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
adv_post_handle_sig_gw_cfg_ready(adv_post_state_t* const p_adv_post_state)
{
    LOG_INFO("Got ADV_POST_SIG_GW_CFG_READY");
    ruuvi_send_nrf_settings_from_gw_cfg();
    if (gw_cfg_get_http_stat_use_http_stat())
    {
        os_timer_sig_one_shot_start(g_p_adv_post_timer_sig_activate_sending_statistics);
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

static bool
adv_post_handle_sig(const adv_post_sig_e adv_post_sig, adv_post_state_t* const p_adv_post_state)
{
    bool flag_stop = false;
    switch (adv_post_sig)
    {
        case ADV_POST_SIG_STOP:
            LOG_INFO("Got ADV_POST_SIG_STOP");
            flag_stop = true;
            break;
        case ADV_POST_SIG_NETWORK_DISCONNECTED:
            adv_post_handle_sig_network_disconnected(p_adv_post_state);
            break;
        case ADV_POST_SIG_NETWORK_CONNECTED:
            adv_post_handle_sig_network_connected(p_adv_post_state);
            break;
        case ADV_POST_SIG_TIME_SYNCHRONIZED:
            adv_post_handle_sig_time_synchronized(p_adv_post_state);
            break;
        case ADV_POST_SIG_RETRANSMIT:
            adv_post_handle_sig_retransmit(p_adv_post_state);
            break;
        case ADV_POST_SIG_RETRANSMIT2:
            adv_post_handle_sig_retransmit2(p_adv_post_state);
            break;
        case ADV_POST_SIG_ACTIVATE_SENDING_STATISTICS:
            adv_post_handle_sig_activate_sending_statistics(p_adv_post_state);
            break;
        case ADV_POST_SIG_SEND_STATISTICS:
            adv_post_handle_sig_send_statistics(p_adv_post_state);
            break;
        case ADV_POST_SIG_DO_ASYNC_COMM:
            adv_post_do_async_comm(p_adv_post_state);
            break;
        case ADV_POST_SIG_RELAYING_MODE_CHANGED:
            adv_post_handle_sig_relaying_mode_changed(p_adv_post_state);
            break;
        case ADV_POST_SIG_NETWORK_WATCHDOG:
            adv_post_handle_sig_network_watchdog();
            break;
        case ADV_POST_SIG_TASK_WATCHDOG_FEED:
            adv_post_handle_sig_task_watchdog_feed();
            break;
        case ADV_POST_SIG_GW_CFG_READY:
            adv_post_handle_sig_gw_cfg_ready(p_adv_post_state);
            break;
        case ADV_POST_SIG_GW_CFG_CHANGED_RUUVI:
            adv_post_handle_sig_gw_cfg_changed_ruuvi(p_adv_post_state);
            break;
        case ADV_POST_SIG_BLE_SCAN_CHANGED:
            LOG_INFO("Got ADV_POST_SIG_BLE_SCAN_CHANGED");
            LOG_INFO("Clear adv_table");
            adv_table_clear();
            break;
        case ADV_POST_SIG_ACTIVATE_CFG_MODE:
            LOG_INFO("Got ADV_POST_SIG_ACTIVATE_CFG_MODE");
            adv_post_disable_ble_filtering();
            break;
        case ADV_POST_SIG_DEACTIVATE_CFG_MODE:
            LOG_INFO("Got ADV_POST_SIG_DEACTIVATE_CFG_MODE");
            ruuvi_send_nrf_settings_from_gw_cfg();
            adv_post_on_gw_cfg_change(p_adv_post_state);
            break;
        case ADV_POST_SIG_GREEN_LED_TURN_ON:
            g_adv_post_green_led_state = true;
            adv_post_on_green_led_update();
            break;
        case ADV_POST_SIG_GREEN_LED_TURN_OFF:
            g_adv_post_green_led_state = false;
            adv_post_on_green_led_update();
            break;
        case ADV_POST_SIG_GREEN_LED_UPDATE:
            adv_post_on_green_led_update();
            break;
        case ADV_POST_SIG_RECV_ADV_TIMEOUT:
            event_mgr_notify(EVENT_MGR_EV_RECV_ADV_TIMEOUT);
            break;
    }
    return flag_stop;
}

static void
adv_post_task(void)
{
    esp_log_level_set(TAG, LOG_LOCAL_LEVEL);

    if (!os_signal_register_cur_thread(g_p_adv_post_sig))
    {
        LOG_ERR("%s failed", "os_signal_register_cur_thread");
        return;
    }

    LOG_INFO("%s started", __func__);
    os_timer_sig_periodic_start(g_p_adv_post_timer_sig_network_watchdog);
    os_timer_sig_one_shot_start(g_p_adv_post_timer_sig_recv_adv_timeout);

    adv_post_wdt_add_and_start();

    adv_post_state_t adv_post_state = {
        .flag_primary_time_sync_is_done = false,
        .flag_network_connected         = false,
        .flag_async_comm_in_progress    = false,
        .flag_need_to_send_advs1        = false,
        .flag_need_to_send_advs2        = false,
        .flag_need_to_send_statistics   = false,
        .flag_relaying_enabled          = true,
        .flag_use_timestamps            = false,
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

    adv_post_unsubscribe_events();
    adv_post_delete_timers();

    http_abort_any_req_during_processing();

    os_signal_unregister_cur_thread(g_p_adv_post_sig);
    os_signal_delete(&g_p_adv_post_sig);
}

static void
adv_post_register_signals(void)
{
    os_signal_add(g_p_adv_post_sig, adv_post_conv_to_sig_num(ADV_POST_SIG_STOP));
    os_signal_add(g_p_adv_post_sig, adv_post_conv_to_sig_num(ADV_POST_SIG_NETWORK_DISCONNECTED));
    os_signal_add(g_p_adv_post_sig, adv_post_conv_to_sig_num(ADV_POST_SIG_NETWORK_CONNECTED));
    os_signal_add(g_p_adv_post_sig, adv_post_conv_to_sig_num(ADV_POST_SIG_TIME_SYNCHRONIZED));
    os_signal_add(g_p_adv_post_sig, adv_post_conv_to_sig_num(ADV_POST_SIG_RETRANSMIT));
    os_signal_add(g_p_adv_post_sig, adv_post_conv_to_sig_num(ADV_POST_SIG_RETRANSMIT2));
    os_signal_add(g_p_adv_post_sig, adv_post_conv_to_sig_num(ADV_POST_SIG_ACTIVATE_SENDING_STATISTICS));
    os_signal_add(g_p_adv_post_sig, adv_post_conv_to_sig_num(ADV_POST_SIG_SEND_STATISTICS));
    os_signal_add(g_p_adv_post_sig, adv_post_conv_to_sig_num(ADV_POST_SIG_DO_ASYNC_COMM));
    os_signal_add(g_p_adv_post_sig, adv_post_conv_to_sig_num(ADV_POST_SIG_RELAYING_MODE_CHANGED));
    os_signal_add(g_p_adv_post_sig, adv_post_conv_to_sig_num(ADV_POST_SIG_NETWORK_WATCHDOG));
    os_signal_add(g_p_adv_post_sig, adv_post_conv_to_sig_num(ADV_POST_SIG_TASK_WATCHDOG_FEED));
    os_signal_add(g_p_adv_post_sig, adv_post_conv_to_sig_num(ADV_POST_SIG_GW_CFG_READY));
    os_signal_add(g_p_adv_post_sig, adv_post_conv_to_sig_num(ADV_POST_SIG_GW_CFG_CHANGED_RUUVI));
    os_signal_add(g_p_adv_post_sig, adv_post_conv_to_sig_num(ADV_POST_SIG_BLE_SCAN_CHANGED));
    os_signal_add(g_p_adv_post_sig, adv_post_conv_to_sig_num(ADV_POST_SIG_ACTIVATE_CFG_MODE));
    os_signal_add(g_p_adv_post_sig, adv_post_conv_to_sig_num(ADV_POST_SIG_DEACTIVATE_CFG_MODE));
    os_signal_add(g_p_adv_post_sig, adv_post_conv_to_sig_num(ADV_POST_SIG_GREEN_LED_TURN_ON));
    os_signal_add(g_p_adv_post_sig, adv_post_conv_to_sig_num(ADV_POST_SIG_GREEN_LED_TURN_OFF));
    os_signal_add(g_p_adv_post_sig, adv_post_conv_to_sig_num(ADV_POST_SIG_GREEN_LED_UPDATE));
    os_signal_add(g_p_adv_post_sig, adv_post_conv_to_sig_num(ADV_POST_SIG_RECV_ADV_TIMEOUT));
}

static void
adv_post_subscribe_events(void)
{
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

    event_mgr_subscribe_sig_static(
        &g_adv_post_ev_info_mem_relaying_mode_changed,
        EVENT_MGR_EV_RELAYING_MODE_CHANGED,
        g_p_adv_post_sig,
        adv_post_conv_to_sig_num(ADV_POST_SIG_RELAYING_MODE_CHANGED));

    event_mgr_subscribe_sig_static(
        &g_adv_post_ev_info_mem_green_led_turn_on,
        EVENT_MGR_EV_GREEN_LED_TURN_ON,
        g_p_adv_post_sig,
        adv_post_conv_to_sig_num(ADV_POST_SIG_GREEN_LED_TURN_ON));
    event_mgr_subscribe_sig_static(
        &g_adv_post_ev_info_mem_green_led_turn_off,
        EVENT_MGR_EV_GREEN_LED_TURN_OFF,
        g_p_adv_post_sig,
        adv_post_conv_to_sig_num(ADV_POST_SIG_GREEN_LED_TURN_OFF));
}

static void
adv_post_unsubscribe_events(void)
{
    event_mgr_unsubscribe_sig_static(&g_adv_post_ev_info_mem_wifi_disconnected, EVENT_MGR_EV_WIFI_DISCONNECTED);
    event_mgr_unsubscribe_sig_static(&g_adv_post_ev_info_mem_eth_disconnected, EVENT_MGR_EV_ETH_DISCONNECTED);
    event_mgr_unsubscribe_sig_static(&g_adv_post_ev_info_mem_wifi_connected, EVENT_MGR_EV_WIFI_CONNECTED);
    event_mgr_unsubscribe_sig_static(&g_adv_post_ev_info_mem_eth_connected, EVENT_MGR_EV_ETH_CONNECTED);
    event_mgr_unsubscribe_sig_static(&g_adv_post_ev_info_mem_time_synchronized, EVENT_MGR_EV_TIME_SYNCHRONIZED);
    event_mgr_unsubscribe_sig_static(&g_adv_post_ev_info_mem_cfg_ready, EVENT_MGR_EV_GW_CFG_READY);
    event_mgr_unsubscribe_sig_static(&g_adv_post_ev_info_mem_gw_cfg_ruuvi_changed, EVENT_MGR_EV_GW_CFG_CHANGED_RUUVI);
    event_mgr_unsubscribe_sig_static(&g_adv_post_ev_info_mem_relaying_mode_changed, EVENT_MGR_EV_RELAYING_MODE_CHANGED);
    event_mgr_unsubscribe_sig_static(&g_adv_post_ev_info_mem_green_led_turn_on, EVENT_MGR_EV_GREEN_LED_TURN_ON);
    event_mgr_unsubscribe_sig_static(&g_adv_post_ev_info_mem_green_led_turn_off, EVENT_MGR_EV_GREEN_LED_TURN_OFF);
}

static void
adv_post_create_timers(void)
{
    g_adv_post_timers[0].p_timer_sig = os_timer_sig_periodic_create_static(
        &g_adv_post_timers[0].timer_sig_mem,
        "adv_post_retransmit",
        g_p_adv_post_sig,
        adv_post_conv_to_sig_num(ADV_POST_SIG_RETRANSMIT),
        pdMS_TO_TICKS(ADV_POST_DEFAULT_INTERVAL_SECONDS * TIME_UNITS_MS_PER_SECOND));

    g_adv_post_timers[1].p_timer_sig = os_timer_sig_periodic_create_static(
        &g_adv_post_timers[1].timer_sig_mem,
        "adv_post_retransmit2",
        g_p_adv_post_sig,
        adv_post_conv_to_sig_num(ADV_POST_SIG_RETRANSMIT2),
        pdMS_TO_TICKS(ADV_POST_DEFAULT_INTERVAL_SECONDS * TIME_UNITS_MS_PER_SECOND));

    g_p_adv_post_timer_sig_activate_sending_statistics = os_timer_sig_one_shot_create_static(
        &g_p_adv_post_timer_sig_activate_sending_statistics_mem,
        "adv_post_act_send_stat",
        g_p_adv_post_sig,
        adv_post_conv_to_sig_num(ADV_POST_SIG_ACTIVATE_SENDING_STATISTICS),
        pdMS_TO_TICKS(ADV_POST_INITIAL_DELAY_IN_SENDING_STATISTICS_SECONDS) * TIME_UNITS_MS_PER_SECOND);

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

    g_p_adv_post_timer_sig_watchdog_feed = os_timer_sig_periodic_create_static(
        &g_adv_post_timer_sig_watchdog_feed_mem,
        "adv_post:wdog",
        g_p_adv_post_sig,
        adv_post_conv_to_sig_num(ADV_POST_SIG_TASK_WATCHDOG_FEED),
        ADV_POST_TASK_WATCHDOG_FEEDING_PERIOD_TICKS);

    g_p_adv_post_timer_sig_green_led_update = os_timer_sig_periodic_create_static(
        &g_adv_post_timer_sig_green_led_update_mem,
        "adv_post:led",
        g_p_adv_post_sig,
        adv_post_conv_to_sig_num(ADV_POST_SIG_GREEN_LED_UPDATE),
        ADV_POST_TASK_LED_UPDATE_PERIOD_TICKS);

    g_p_adv_post_timer_sig_recv_adv_timeout = os_timer_sig_one_shot_create_static(
        &g_adv_post_timer_sig_recv_adv_timeout_mem,
        "adv_post:timeout",
        g_p_adv_post_sig,
        adv_post_conv_to_sig_num(ADV_POST_SIG_RECV_ADV_TIMEOUT),
        ADV_POST_TASK_RECV_ADV_TIMEOUT_TICKS);
}

static void
adv_post_delete_timers(void)
{
    os_timer_sig_periodic_stop(g_adv_post_timers[0].p_timer_sig);
    os_timer_sig_periodic_delete(&g_adv_post_timers[0].p_timer_sig);
    os_timer_sig_periodic_stop(g_adv_post_timers[1].p_timer_sig);
    os_timer_sig_periodic_delete(&g_adv_post_timers[1].p_timer_sig);
    os_timer_sig_one_shot_stop(g_p_adv_post_timer_sig_activate_sending_statistics);
    os_timer_sig_one_shot_delete(&g_p_adv_post_timer_sig_activate_sending_statistics);
    os_timer_sig_periodic_stop(g_p_adv_post_timer_sig_send_statistics);
    os_timer_sig_periodic_delete(&g_p_adv_post_timer_sig_send_statistics);
    os_timer_sig_one_shot_stop(g_p_adv_post_timer_sig_do_async_comm);
    os_timer_sig_one_shot_delete(&g_p_adv_post_timer_sig_do_async_comm);
    os_timer_sig_periodic_stop(g_p_adv_post_timer_sig_network_watchdog);
    os_timer_sig_periodic_delete(&g_p_adv_post_timer_sig_network_watchdog);
    os_timer_sig_periodic_stop(g_p_adv_post_timer_sig_watchdog_feed);
    os_timer_sig_periodic_delete(&g_p_adv_post_timer_sig_watchdog_feed);
    os_timer_sig_periodic_stop(g_p_adv_post_timer_sig_green_led_update);
    os_timer_sig_periodic_delete(&g_p_adv_post_timer_sig_green_led_update);
    os_timer_sig_one_shot_stop(g_p_adv_post_timer_sig_recv_adv_timeout);
    os_timer_sig_one_shot_delete(&g_p_adv_post_timer_sig_recv_adv_timeout);
}

void
adv_post_init(void)
{
    g_adv_post_green_led_state = false;

    g_p_adv_post_sig = os_signal_create_static(&g_adv_post_sig_mem);
    adv_post_register_signals();
    adv_post_subscribe_events();
    adv_post_create_timers();

    g_adv_post_nonce = esp_random();
    adv_table_init();
    api_callbacks_reg((void*)&adv_callback_func_tbl);
    const uint32_t           stack_size    = (1024U * 6U);
    const os_task_priority_t task_priority = 5;
    if (!os_task_create_finite_without_param(&adv_post_task, "adv_post_task", stack_size, task_priority))
    {
        LOG_ERR("Can't create thread");
    }
    while (!os_signal_is_any_thread_registered(g_p_adv_post_sig))
    {
        vTaskDelay(1);
    }
}

bool
adv_post_is_initialized(void)
{
    return os_signal_is_any_thread_registered(g_p_adv_post_sig);
}

static void
adv_post_set_default_period_for_timer(adv_post_timer_t* const p_adv_post_timer, const uint32_t period_ms)
{
    if (period_ms != p_adv_post_timer->default_interval_ms)
    {
        LOG_INFO(
            "X-Ruuvi-Gateway-Rate: adv%d: Change period from %u ms to %u ms",
            p_adv_post_timer->num,
            (printf_uint_t)p_adv_post_timer->default_interval_ms,
            (printf_uint_t)period_ms);
        p_adv_post_timer->default_interval_ms = period_ms;
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
            adv_post_set_default_period_for_timer(&g_adv_post_timers[0], period_ms);
            break;
        case ADV_POST_ACTION_POST_ADVS_TO_CUSTOM:
            adv_post_set_default_period_for_timer(&g_adv_post_timers[1], period_ms);
            break;
        case ADV_POST_ACTION_POST_STATS:
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
    }
    return false;
}

static void
adv_post_timer_restart_with_period(adv_post_timer_t* const p_adv_post_timer, const uint32_t period_ms)
{
    if (p_adv_post_timer->cur_interval_ms != period_ms)
    {
        p_adv_post_timer->cur_interval_ms = period_ms;
        LOG_INFO(
            "advs%u: restart timer with period %u ms",
            (printf_uint_t)p_adv_post_timer->num,
            (printf_uint_t)p_adv_post_timer->cur_interval_ms);
        os_timer_sig_periodic_restart(p_adv_post_timer->p_timer_sig, pdMS_TO_TICKS(p_adv_post_timer->cur_interval_ms));
    }
}

void
adv1_post_timer_restart_with_default_period(void)
{
    adv_post_timer_t* const p_adv_post_timer = &g_adv_post_timers[0];
    adv_post_timer_restart_with_period(p_adv_post_timer, p_adv_post_timer->default_interval_ms);
}

void
adv1_post_timer_restart_with_increased_period(void)
{
    adv_post_timer_t* const p_adv_post_timer = &g_adv_post_timers[0];
    adv_post_timer_restart_with_period(p_adv_post_timer, ADV_POST_DELAY_BEFORE_RETRYING_POST_AFTER_ERROR_MS);
}

void
adv1_post_timer_restart_with_short_period(void)
{
    adv_post_timer_t* const p_adv_post_timer = &g_adv_post_timers[0];
    adv_post_timer_restart_with_period(p_adv_post_timer, ADV_POST_DELAY_AFTER_CONFIGURATION_CHANGED_MS);
}

void
adv2_post_timer_restart_with_default_period(void)
{
    adv_post_timer_t* const p_adv_post_timer = &g_adv_post_timers[1];
    adv_post_timer_restart_with_period(p_adv_post_timer, p_adv_post_timer->default_interval_ms);
}

void
adv2_post_timer_restart_with_increased_period(void)
{
    adv_post_timer_t* const p_adv_post_timer = &g_adv_post_timers[1];
    adv_post_timer_restart_with_period(p_adv_post_timer, ADV_POST_DELAY_BEFORE_RETRYING_POST_AFTER_ERROR_MS);
}

void
adv2_post_timer_restart_with_short_period(void)
{
    adv_post_timer_t* const p_adv_post_timer = &g_adv_post_timers[1];
    adv_post_timer_restart_with_period(p_adv_post_timer, ADV_POST_DELAY_AFTER_CONFIGURATION_CHANGED_MS);
}

void
adv_post_send_sig_ble_scan_changed(void)
{
    os_signal_send(g_p_adv_post_sig, adv_post_conv_to_sig_num(ADV_POST_SIG_BLE_SCAN_CHANGED));
}

void
adv_post_send_sig_activate_cfg_mode(void)
{
    os_signal_send(g_p_adv_post_sig, adv_post_conv_to_sig_num(ADV_POST_SIG_ACTIVATE_CFG_MODE));
}

void
adv_post_send_sig_deactivate_cfg_mode(void)
{
    os_signal_send(g_p_adv_post_sig, adv_post_conv_to_sig_num(ADV_POST_SIG_DEACTIVATE_CFG_MODE));
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
