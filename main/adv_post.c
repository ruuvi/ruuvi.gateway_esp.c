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
#include "cJSON.h"
#include "mqtt.h"
#include "api.h"
#include "types_def.h"
#include "ruuvi_endpoint_ca_uart.h"
#include "ruuvi_gateway.h"
#include "time_task.h"
#include "metrics.h"
#include "mac_addr.h"
#include "ruuvi_device_id.h"
#include "event_mgr.h"
#include "gw_status.h"
#include "gw_cfg.h"
#include "adv_post_signals.h"
#include "adv_post_timers.h"
#include "adv_post_task.h"
#include "adv_post_cfg_cache.h"
#include "adv_post_green_led.h"
#include "adv_post_async_comm.h"
#include "adv_post_statistics.h"

#define LOG_LOCAL_LEVEL LOG_LEVEL_INFO
#include "log.h"
static const char* TAG = "ADV_POST_TASK";

static void
adv_post_send_report(void* p_arg);

static void
adv_post_send_ack(void* arg);

static void
adv_post_cb_on_recv_device_id(void* arg);

static void
adv_post_send_get_all(void* arg);

static adv_callbacks_fn_t adv_callback_func_tbl = {
    .AdvAckCallback    = adv_post_send_ack,
    .AdvReportCallback = adv_post_send_report,
    .AdvIdCallback     = adv_post_cb_on_recv_device_id,
    .AdvGetAllCallback = adv_post_send_get_all,
};

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
    metrics_received_advs_increment();
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
        LOG_DBG("Drop adv - gw_cfg is not ready yet");
        return;
    }

    adv_post_cfg_cache_t* p_cfg_cache = adv_post_cfg_cache_mutex_try_lock();
    if (NULL == p_cfg_cache)
    {
        LOG_DBG("Drop adv - gateway in the process of reconfiguration");
        return;
    }

    const bool   flag_ntp_use              = p_cfg_cache->flag_use_ntp;
    const time_t timestamp_if_synchronized = time_is_synchronized() ? time(NULL) : 0;
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
        adv_post_cfg_cache_mutex_unlock(&p_cfg_cache);
        return;
    }

    adv_post_timers_relaunch_timer_sig_recv_adv_timeout();

    LOG_DUMP_VERBOSE(
        adv_report.data_buf,
        adv_report.data_len,
        "Recv Adv: MAC=%s, ID=0x%02x%02x, time=%lu",
        mac_address_to_str(&adv_report.tag_mac).str_buf,
        adv_report.data_buf[6],
        adv_report.data_buf[5],
        (printf_ulong_t)timestamp);

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
adv_post_send_get_all(void* arg)
{
    (void)arg;
    LOG_INFO("Got configuration request from NRF52");
    adv_post_on_green_led_update(ADV_POST_GREEN_LED_CMD_UPDATE);
    ruuvi_send_nrf_settings_from_gw_cfg();
}

void
adv_post_init(void)
{
    adv_post_green_led_init();
    adv_post_async_comm_init();
    adv_post_statistics_init();
    adv_table_init();

    const uint32_t           stack_size    = (1024U * 4U) + 512U;
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
}
