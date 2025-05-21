/**
 * @file adv_post.c
 * @author Oleg Protasevich
 * @date 2020-09-04
 * @copyright Ruuvi Innovations Ltd, license BSD-3-Clause.
 */

#include "adv_post.h"
#include <string.h>
#include <time.h>
#include "esp_task_wdt.h"
#include "os_task.h"
#include "mqtt.h"
#include "types_def.h"
#include "ruuvi_endpoint_ca_uart.h"
#include "ruuvi_gateway.h"
#include "time_task.h"
#include "metrics.h"
#include "mac_addr.h"
#include "ruuvi_device_id.h"
#include "event_mgr.h"
#include "gw_cfg.h"
#include "adv_post_signals.h"
#include "adv_post_timers.h"
#include "adv_post_task.h"
#include "adv_post_cfg_cache.h"
#include "adv_post_green_led.h"
#include "adv_post_async_comm.h"
#include "adv_post_statistics.h"
#include "adv_post_nrf52.h"
#include "api.h"
#include "terminal.h"

#define LOG_LOCAL_LEVEL LOG_LEVEL_INFO
#include "log.h"
static const char* TAG = "ADV_POST";

#define NRF52_COMM_TASK_PRIORITY (9)

static void
adv_post_nrf52_cb_on_recv_adv(void* p_arg);

static void
adv_post_nrf52_cb_on_recv_ack(void* arg);

static void
adv_post_nrf52_cb_on_recv_device_id(void* arg);

static void
adv_post_nrf52_cb_on_recv_get_all(void* arg);

static adv_callbacks_fn_t adv_callback_func_tbl = {
    .AdvAckCallback    = adv_post_nrf52_cb_on_recv_ack,
    .AdvReportCallback = adv_post_nrf52_cb_on_recv_adv,
    .AdvIdCallback     = adv_post_nrf52_cb_on_recv_device_id,
    .AdvGetAllCallback = adv_post_nrf52_cb_on_recv_get_all,
};

static uint32_t g_adv_post_advs_cnt;

static void
adv_post_advs_cnt_inc(void)
{
    g_adv_post_advs_cnt += 1;
}

uint32_t
adv_post_advs_cnt_get_and_clear(void)
{
    const uint32_t cnt  = g_adv_post_advs_cnt;
    g_adv_post_advs_cnt = 0;
    return cnt;
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

static bool
adv_put_to_table(const adv_report_t* const p_adv)
{
    metrics_received_advs_increment(p_adv->secondary_phy);
    return adv_table_put(p_adv);
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
    if ((RE_CA_UART_ADV_RPRT != p_msg->cmd) && (RE_CA_UART_ADV_RPRT2 != p_msg->cmd))
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
    p_adv->primary_phy     = p_report->primary_phy;
    p_adv->secondary_phy   = p_report->secondary_phy;
    p_adv->ch_index        = p_report->ch_index;
    p_adv->is_coded_phy    = p_report->is_coded_phy;
    p_adv->tx_power        = p_report->tx_power;
    p_adv->data_len        = p_report->adv_len;
    memcpy(p_adv->data_buf, p_report->adv, p_report->adv_len);

    return true;
}

static void
adv_post_nrf52_cb_on_recv_ack(void* arg)
{
    LOG_DBG("%s", __func__);
    const re_ca_uart_payload_t* const p_uart_payload = arg;

    adv_post_nrf52_on_async_ack(p_uart_payload->params.ack.cmd, p_uart_payload->params.ack.ack_state);
}

static void
adv_post_nrf52_cb_on_recv_device_id(void* p_arg)
{
    LOG_DBG("%s", __func__);
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

static const char*
ble_phy_to_str(const re_ca_uart_ble_phy_e phy)
{
    switch (phy)
    {
        case RE_CA_UART_BLE_PHY_1MBPS:
            return "1M";
        case RE_CA_UART_BLE_PHY_2MBPS:
            return "2M";
        case RE_CA_UART_BLE_PHY_CODED:
            return "Coded";
        case RE_CA_UART_BLE_PHY_NOT_SET:
            return "NA";
        default:
            break;
    }
    return "UNK";
}

static const char*
ble_phy_info_str(
    const re_ca_uart_ble_phy_e primary_phy,
    const re_ca_uart_ble_phy_e secondary_phy,
    const bool                 is_coded_phy)
{
    if (is_coded_phy)
    {
        if ((RE_CA_UART_BLE_PHY_CODED == primary_phy) && (RE_CA_UART_BLE_PHY_CODED == secondary_phy))
        {
            return "Coded";
        }
    }
    else
    {
        if ((RE_CA_UART_BLE_PHY_1MBPS == primary_phy) && (RE_CA_UART_BLE_PHY_NOT_SET == secondary_phy))
        {
            return "1M";
        }
        if ((RE_CA_UART_BLE_PHY_1MBPS == primary_phy) && (RE_CA_UART_BLE_PHY_2MBPS == secondary_phy))
        {
            return "2M";
        }
    }
    return NULL;
}

typedef struct ble_phy_agg_info_str_t
{
    char buf[32];
} ble_phy_agg_info_str_t;

static ble_phy_agg_info_str_t
ble_phy_agg_info_str(
    const re_ca_uart_ble_phy_e primary_phy,
    const re_ca_uart_ble_phy_e secondary_phy,
    const bool                 is_coded_phy)
{
    ble_phy_agg_info_str_t phy_agg_info   = { 0 };
    const char* const      p_phy_info_str = ble_phy_info_str(primary_phy, secondary_phy, is_coded_phy);
    if (NULL != p_phy_info_str)
    {
        snprintf(phy_agg_info.buf, sizeof(phy_agg_info.buf), "%s", p_phy_info_str);
    }
    else
    {
        snprintf(
            phy_agg_info.buf,
            sizeof(phy_agg_info.buf),
            "%s/%s(is_coded=%d)",
            ble_phy_to_str(primary_phy),
            ble_phy_to_str(secondary_phy),
            is_coded_phy);
    }
    return phy_agg_info;
}

typedef struct ble_phy_tx_power_str_t
{
    char buf[8];
} ble_phy_tx_power_str_t;

static ble_phy_tx_power_str_t
ble_tx_power_to_str(const int8_t tx_power)
{
    ble_phy_tx_power_str_t tx_power_str = { 0 };
    if (RE_CA_UART_BLE_GAP_POWER_LEVEL_INVALID == tx_power)
    {
        snprintf(tx_power_str.buf, sizeof(tx_power_str.buf), "NA");
    }
    else
    {
        snprintf(tx_power_str.buf, sizeof(tx_power_str.buf), "%d", tx_power);
    }
    return tx_power_str;
}

#define BLE_MSG_CHUNK_LEN_OFFSET                                             (0U)
#define BLE_MSG_CHUNK_TYPE_OFFSET                                            (1U)
#define BLE_MSG_CHUNK_TYPE_MANUFACTURED_SPECIFIC_DATA                        (0xFFU)
#define BLE_MSG_CHUNK_DATA_OFFSET                                            (2U)
#define BLE_MSG_CHUNK_MANUFACTURED_SPECIFIC_DATA_MIN_LEN                     (3U)
#define BLE_MSG_CHUNK_MANUFACTURED_SPECIFIC_DATA_MANUFACTURER_ID_LOW_OFFSET  (2U)
#define BLE_MSG_CHUNK_MANUFACTURED_SPECIFIC_DATA_MANUFACTURER_ID_HIGH_OFFSET (3U)
#define NUM_BITS_PER_BYTE                                                    (8U)

static uint16_t
get_ble_manuf_id(const adv_report_t* const p_adv)
{
    if (p_adv->data_len < BLE_MSG_CHUNK_DATA_OFFSET)
    {
        return 0;
    }
    const uint8_t* p_data = p_adv->data_buf;
    while (((ptrdiff_t)p_adv->data_len - (p_data - p_adv->data_buf)) >= BLE_MSG_CHUNK_DATA_OFFSET)
    {
        const uint8_t len  = p_data[BLE_MSG_CHUNK_LEN_OFFSET];
        const uint8_t type = p_data[BLE_MSG_CHUNK_TYPE_OFFSET];
        if (BLE_MSG_CHUNK_TYPE_MANUFACTURED_SPECIFIC_DATA != type)
        {
            p_data += len + 1;
            continue;
        }
        if (len < BLE_MSG_CHUNK_MANUFACTURED_SPECIFIC_DATA_MIN_LEN)
        {
            break;
        }
        return (uint16_t)((uint16_t)p_data[BLE_MSG_CHUNK_MANUFACTURED_SPECIFIC_DATA_MANUFACTURER_ID_HIGH_OFFSET] << NUM_BITS_PER_BYTE)
               | (uint16_t)p_data[BLE_MSG_CHUNK_MANUFACTURED_SPECIFIC_DATA_MANUFACTURER_ID_LOW_OFFSET];
    }
    return 0;
}

#define ADV_POST_MAX_NUM_SCAN_FILTERS_FOR_LOGGING (3U)

static void
adv_post_log_adv_report(
    const adv_report_t* const         p_adv,
    const adv_post_cfg_cache_t* const p_cfg_cache,
    const time_t                      timestamp)
{
#if 1 && LOG_LOCAL_LEVEL < LOG_LEVEL_VERBOSE
    bool flag_log_single_allowed_mac = false;
    if (p_cfg_cache->scan_filter_allow_listed && (p_cfg_cache->scan_filter_length > 0)
        && (p_cfg_cache->scan_filter_length <= ADV_POST_MAX_NUM_SCAN_FILTERS_FOR_LOGGING))
    {
        for (uint32_t i = 0; i < p_cfg_cache->scan_filter_length; ++i)
        {
            if (0 == memcmp(&p_cfg_cache->p_arr_of_scan_filter_mac[i].mac, p_adv->tag_mac.mac, MAC_ADDRESS_NUM_BYTES))
            {
                flag_log_single_allowed_mac = true;
                break;
            }
        }
    }

    if (flag_log_single_allowed_mac)
    {
        LOG_DUMP_INFO(
            p_adv->data_buf,
            p_adv->data_len,
            "Recv Adv: MAC=%s, ID=0x%04x, time=%lu, RSSI=%d, PHY=%s, Chan=%d, tx_power=%s",
            mac_address_to_str(&p_adv->tag_mac).str_buf,
            get_ble_manuf_id(p_adv),
            (printf_ulong_t)timestamp,
            p_adv->rssi,
            ble_phy_agg_info_str(p_adv->primary_phy, p_adv->secondary_phy, p_adv->is_coded_phy).buf,
            p_adv->ch_index,
            ble_tx_power_to_str(p_adv->tx_power).buf);
    }
#else
    LOG_DUMP_VERBOSE(
        p_adv->data_buf,
        p_adv->data_len,
        "Recv Adv: MAC=%s, ID=0x%04x, time=%lu, RSSI=%d, PHY=%s, Chan=%d, tx_power=%s",
        mac_address_to_str(&p_adv->tag_mac).str_buf,
        get_ble_manuf_id(p_adv),
        (printf_ulong_t)timestamp,
        p_adv->rssi,
        ble_phy_agg_info_str(p_adv->primary_phy, p_adv->secondary_phy, p_adv->is_coded_phy).buf,
        p_adv->ch_index,
        ble_tx_power_to_str(p_adv->tx_power).buf);
#endif
}

static void
adv_post_nrf52_cb_on_recv_adv(void* p_arg)
{
    LOG_DBG("%s", __func__);
    if (!adv_post_nrf52_is_configured())
    {
        LOG_DBG("Drop adv - nRF52 is not configured");
        return;
    }
    if (!gw_cfg_is_initialized())
    {
        LOG_DBG("Drop adv - gw_cfg is not ready yet");
        return;
    }
    adv_post_advs_cnt_inc();

    adv_post_cfg_cache_t* p_cfg_cache = adv_post_cfg_cache_mutex_try_lock();
    if (NULL == p_cfg_cache)
    {
        LOG_DBG("Drop adv - gateway in the process of reconfiguration");
        return;
    }

    const bool   flag_ntp_use              = p_cfg_cache->flag_use_ntp;
    const bool   flag_time_is_synchronized = time_is_synchronized();
    const time_t timestamp_if_synchronized = flag_time_is_synchronized ? time(NULL) : 0;
    const time_t timestamp = flag_ntp_use ? timestamp_if_synchronized : (time_t)metrics_received_advs_get();

    adv_report_t adv_report = { 0 };
    if (!parse_adv_report_from_uart((re_ca_uart_payload_t*)p_arg, timestamp, &adv_report))
    {
        LOG_WARN("Drop adv - parsing failed");
        adv_post_cfg_cache_mutex_unlock(&p_cfg_cache);
        return;
    }

    if (adv_post_check_if_mac_filtered_out(p_cfg_cache, &adv_report.tag_mac))
    {
        LOG_DBG("Drop adv - MAC is filtered out");
        adv_post_cfg_cache_mutex_unlock(&p_cfg_cache);
        return;
    }

    adv_post_timers_relaunch_timer_sig_recv_adv_timeout();

    adv_post_log_adv_report(&adv_report, p_cfg_cache, timestamp);

    if (flag_ntp_use && (!flag_time_is_synchronized))
    {
        LOG_DBG("Drop adv - time has not yet synchronized");
        adv_post_cfg_cache_mutex_unlock(&p_cfg_cache);
        return;
    }

    if (!adv_put_to_table(&adv_report))
    {
        LOG_DBG(
            "Drop adv because the same data is already in the buffer: MAC=%s, ID=0x%02x%02x",
            mac_address_to_str(&adv_report.tag_mac).str_buf,
            adv_report.data_buf[6],
            adv_report.data_buf[5]);
    }
    else
    {
        event_mgr_notify(EVENT_MGR_EV_RECV_ADV);
    }

    adv_post_cfg_cache_mutex_unlock(&p_cfg_cache);
}

static void
adv_post_nrf52_cb_on_recv_get_all(void* arg)
{
    (void)arg;
    LOG_DBG("%s", __func__);
    adv_post_green_led_async_disable();
    if (gw_cfg_is_initialized())
    {
        LOG_WARN("Got configuration request from NRF52");
        LOG_INFO("Notify: EVENT_MGR_EV_NRF52_REBOOTED");
    }
    else
    {
        LOG_INFO("Got initial configuration request from NRF52");
    }
    event_mgr_notify(EVENT_MGR_EV_NRF52_REBOOTED);
}

void
adv_post_init(void)
{
    terminal_open(NULL, true, NRF52_COMM_TASK_PRIORITY);
    api_process(true);
    adv_post_async_comm_init();
    adv_post_statistics_init();
    adv_table_init();

    const uint32_t           stack_size    = (1024U * 6U);
    const os_task_priority_t task_priority = 5;
    if (!os_task_create_finite_without_param(&adv_post_task, "adv_post_task", stack_size, task_priority))
    {
        LOG_ERR("Can't create thread");
    }
    while (!adv_post_signals_is_thread_registered())
    {
        vTaskDelay(1);
    }

    api_callbacks_reg((void*)&adv_callback_func_tbl);

    LOG_INFO("Wait while nRF52 is in hw_reset state");
    while (adv_post_nrf52_is_in_hw_reset_state())
    {
        vTaskDelay(1);
    }
    LOG_INFO("nRF52 started");
    vTaskDelay(pdMS_TO_TICKS(50));
}
