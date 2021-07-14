/**
 * @file adv_post.c
 * @author Oleg Protasevich
 * @date 2020-09-04
 * @copyright Ruuvi Innovations Ltd, license BSD-3-Clause.
 */

#include "adv_post.h"
#include "bin2hex.h"
#include "cJSON.h"
#include "esp_err.h"
#include "os_task.h"
#include "freertos/FreeRTOS.h"
#include "http.h"
#include "mqtt.h"
#include "ruuvi_boards.h"
#include "api.h"
#include "types_def.h"
#include "ruuvi_endpoint_ca_uart.h"
#include "ruuvi_endpoints.h"
#include "ruuvi_gateway.h"
#include <ctype.h>
#include <string.h>
#include <time.h>
#include "time_task.h"
#include "attribs.h"
#include "metrics.h"
#include "os_malloc.h"
#include "esp_type_wrapper.h"
#include "wifi_manager.h"
#include "mac_addr.h"
#include "http_json.h"
#include "ruuvi_device_id.h"

#define LOG_LOCAL_LEVEL LOG_LEVEL_INFO
#include "log.h"

static void
adv_post_send_report(void *arg);

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

static uint32_t g_adv_post_nonce;

static uint32_t g_adv_post_interval = ADV_POST_DEFAULT_INTERVAL;

/** @brief serialise up to U64 into given buffer, MSB first. */
static inline void
u64_to_array(const uint64_t u64, uint8_t *const array, uint8_t bytes)
{
    const uint8_t offset = bytes - 1;

    while (bytes--)
    {
        array[offset - bytes] = (u64 >> (8U * bytes)) & 0xFFU;
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
parse_adv_report_from_uart(const re_ca_uart_payload_t *const p_msg, adv_report_t *const p_adv)
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
    mac_address_bin_init(&p_adv->tag_mac, p_report->mac);
    p_adv->timestamp = time(NULL);
    p_adv->rssi      = p_report->rssi_db;
    p_adv->data_len  = p_report->adv_len;
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
adv_post_send_report(void *arg)
{
    adv_report_t adv_report = { 0 };

    if (!parse_adv_report_from_uart((re_ca_uart_payload_t *)arg, &adv_report))
    {
        return;
    }
    const esp_err_t ret = adv_put_to_table(&adv_report);
    if (ESP_ERR_NO_MEM == ret)
    {
        LOG_WARN("Adv report table full, adv dropped");
    }
}

static void
adv_post_send_get_all(void *arg)
{
    (void)arg;
    ruuvi_send_nrf_settings(&g_gateway_config);
}

static void
adv_post_log(const adv_report_table_t *p_reports)
{
    LOG_INFO("Advertisements in table: %u", (printf_uint_t)p_reports->num_of_advs);
    for (num_of_advs_t i = 0; i < p_reports->num_of_advs; ++i)
    {
        const adv_report_t *p_adv = &p_reports->table[i];

        const mac_address_str_t mac_str = mac_address_to_str(&p_adv->tag_mac);
        LOG_DUMP_INFO(
            p_adv->data_buf,
            p_adv->data_len,
            "i: %d, tag: %s, rssi: %d, timestamp: %ld",
            i,
            mac_str.str_buf,
            p_adv->rssi,
            p_adv->timestamp);
    }
}

static bool
adv_post_check_is_connected(const uint32_t nonce)
{
    const EventBits_t status = xEventGroupGetBits(status_bits);
    if (0 == (status & (WIFI_CONNECTED_BIT | ETH_CONNECTED_BIT)))
    {
        return false;
    }
    if (g_gateway_config.http.use_http)
    {
        cjson_wrap_str_t json_str = cjson_wrap_str_null();
        if (!http_create_status_online_json_str(
                time(NULL),
                &g_gw_mac_sta_str,
                g_gateway_config.coordinates,
                nonce,
                &json_str))
        {
            LOG_ERR("Not enough memory to generate json");
        }
        else
        {
            LOG_INFO("HTTP POST: %s", json_str.p_str);
            http_send(json_str.p_str);
            cjson_wrap_free_json_str(&json_str);
        }
    }
    return true;
}

static bool
adv_post_check_is_disconnected(void)
{
    bool flag_connected = true;

    const EventBits_t status = xEventGroupGetBits(status_bits);
    if (0 == (status & (WIFI_CONNECTED_BIT | ETH_CONNECTED_BIT)))
    {
        flag_connected = false;
    }
    return flag_connected;
}

static void
adv_post_retransmit_advs(const adv_report_table_t *p_reports, const bool flag_connected)
{
    if (0 == p_reports->num_of_advs)
    {
        return;
    }
    if (!flag_connected)
    {
        LOG_WARN("Can't send, no network connection");
        return;
    }
    if (!time_is_valid(p_reports->table[0].timestamp))
    {
        LOG_WARN("Can't send, time have not synchronized yet");
        return;
    }

    if (g_gateway_config.http.use_http)
    {
        if (!wifi_manager_is_connected_to_wifi_or_ethernet())
        {
            LOG_WARN("Can't send, no network connection");
            return;
        }
        http_send_advs(p_reports, g_adv_post_nonce);
        g_adv_post_nonce += 1;
    }
    if (g_gateway_config.mqtt.use_mqtt)
    {
        if (0 == (xEventGroupGetBits(status_bits) & MQTT_CONNECTED_BIT))
        {
            LOG_WARN("Can't send, MQTT is not connected yet");
            return;
        }
        mqtt_publish_table(p_reports);
    }
}

ATTR_NORETURN
static void
adv_post_task(void)
{
    static adv_report_table_t g_adv_reports_buf;

    esp_log_level_set(TAG, ESP_LOG_INFO);
    LOG_INFO("%s", __func__);
    bool flag_connected = false;

    while (1)
    {
        // for thread safety copy the advertisements to a separate buffer for posting
        adv_table_read_retransmission_list_and_clear(&g_adv_reports_buf);

        adv_post_log(&g_adv_reports_buf);

        if (!flag_connected)
        {
            flag_connected = adv_post_check_is_connected(g_adv_post_nonce);
            g_adv_post_nonce += 1;
        }
        else
        {
            flag_connected = adv_post_check_is_disconnected();
        }

        if (0 != g_adv_reports_buf.num_of_advs)
        {
            if (flag_connected)
            {
                adv_post_retransmit_advs(&g_adv_reports_buf, flag_connected);
            }
            else
            {
                LOG_WARN("Can't send, no network connection");
            }
        }

        vTaskDelay(pdMS_TO_TICKS(g_adv_post_interval));
    }
}

void
adv_post_init(void)
{
    g_adv_post_nonce = esp_random();
    adv_table_init();
    api_callbacks_reg((void *)&adv_callback_func_tbl);
    const uint32_t   stack_size = 1024U * 4U;
    os_task_handle_t h_task     = NULL;
    if (!os_task_create_without_param(&adv_post_task, "adv_post_task", stack_size, 1, &h_task))
    {
        LOG_ERR("Can't create thread");
    }
}

void
adv_post_set_period(const uint32_t period_ms)
{
    if (period_ms != g_adv_post_interval)
    {
        LOG_INFO("Change period from %u ms to %u ms", (printf_uint_t)g_adv_post_interval, (printf_uint_t)period_ms);
        g_adv_post_interval = period_ms;
    }
}
