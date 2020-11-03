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
#include "attribs.h"
#include "log.h"

static void
adv_post_send_report(void *arg);

static void
adv_post_send_ack(void *arg);

static void
adv_post_send_device_id(void *arg);

portMUX_TYPE adv_table_mux = portMUX_INITIALIZER_UNLOCKED;

adv_report_table_t adv_reports;
adv_report_table_t adv_reports_buf;

static const char *TAG = "ADV_POST_TASK";

adv_callbacks_fn_t adv_callback_func_tbl = {
    .AdvAckCallback    = adv_post_send_ack,
    .AdvReportCallback = adv_post_send_report,
    .AdvIdCallback     = adv_post_send_device_id,
};

static esp_err_t
adv_put_to_table(const adv_report_t *const p_adv)
{
    portENTER_CRITICAL(&adv_table_mux);
    gw_metrics.received_advertisements += 1;
    bool      found = false;
    esp_err_t ret   = ESP_OK;

    // Check if we already have advertisement with this MAC
    for (num_of_advs_t i = 0; i < adv_reports.num_of_advs; ++i)
    {
        const mac_address_bin_t *p_mac = &adv_reports.table[i].tag_mac;

        if (0 == memcmp(&p_adv->tag_mac, p_mac, sizeof(*p_mac)))
        {
            // Yes, update data.
            found                = true;
            adv_reports.table[i] = *p_adv;
        }
    }

    // not found from the table, insert if not full
    if (!found)
    {
        if (adv_reports.num_of_advs < MAX_ADVS_TABLE)
        {
            adv_reports.table[adv_reports.num_of_advs] = *p_adv;
            adv_reports.num_of_advs += 1;
        }
        else
        {
            ret = ESP_ERR_NO_MEM;
        }
    }

    portEXIT_CRITICAL(&adv_table_mux);
    return ret;
}

static bool
is_hexstr(const char *str)
{
    const size_t len = strlen(str);
    for (size_t i = 0; i < len; ++i)
    {
        const int_fast32_t ch_val = (int_fast32_t)(uint8_t)str[i];
        if (0 == isxdigit(ch_val))
        {
            return false;
        }
    }
    return true;
}

static bool
is_adv_report_valid(const adv_report_t *adv)
{
    if (!is_hexstr(adv->data))
    {
        return false;
    }
    return true;
}

static bool
parse_adv_report_from_uart(const re_ca_uart_payload_t *const msg, adv_report_t *adv)
{
    if (NULL == msg)
    {
        return false;
    }
    if (NULL == adv)
    {
        return false;
    }
    if (RE_CA_UART_ADV_RPRT != msg->cmd)
    {
        return false;
    }
    const re_ca_uart_ble_adv_t *const report = &(msg->params.adv);
    time_t                            now    = 0;
    time(&now);
    adv->rssi      = (wifi_rssi_t)report->rssi_db;
    adv->timestamp = now;
    mac_address_bin_init(&adv->tag_mac, report->mac);
    bin2hex(adv->data, sizeof(adv->data), report->adv, report->adv_len);

    if (!is_adv_report_valid(adv))
    {
        return false;
    }

    return true;
}

static void
adv_post_send_ack(void *arg)
{
    (void)arg;
    // Do something
}

static void
adv_post_send_device_id(void *arg)
{
    (void)arg;
    // Do something
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
        ESP_LOGW(TAG, "Adv report table full, adv dropped");
    }
}

static void
adv_post_log(const adv_report_table_t *p_reports)
{
    ESP_LOGI(TAG, "Advertisements in table:");
    for (num_of_advs_t i = 0; i < p_reports->num_of_advs; ++i)
    {
        const adv_report_t *p_adv = &p_reports->table[i];

        const mac_address_str_t mac_str = mac_address_to_str(&p_adv->tag_mac);
        ESP_LOGI(
            TAG,
            "i: %d, tag: %s, rssi: %d, data: %s, timestamp: %ld",
            i,
            mac_str.str_buf,
            p_adv->rssi,
            p_adv->data,
            p_adv->timestamp);
    }
}

static bool
adv_post_check_is_connected(void)
{
    bool flag_connected = false;

    const EventBits_t status = xEventGroupGetBits(status_bits);
    if (0 != (status & (WIFI_CONNECTED_BIT | ETH_CONNECTED_BIT)))
    {
        if (g_gateway_config.http.use_http)
        {
            flag_connected = true;
            char json_str[64];
            snprintf(json_str, sizeof(json_str), "{\"status\": \"online\", \"gw_mac\": \"%s\"}", gw_mac_sta.str_buf);
            ESP_LOGI(TAG, "HTTP POST: %s", json_str);
            http_send(json_str);
        }
        else if (g_gateway_config.mqtt.use_mqtt)
        {
            flag_connected = true;
        }
        else
        {
            // MISRA C:2012, 15.7 - All if...else if constructs shall be terminated with an else statement
        }
    }
    return flag_connected;
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
adv_post_retransmit_advs(const adv_report_table_t *p_reports)
{
    if (g_gateway_config.http.use_http)
    {
        http_send_advs(p_reports);
    }
    if (g_gateway_config.mqtt.use_mqtt && (0 != (xEventGroupGetBits(status_bits) & MQTT_CONNECTED_BIT)))
    {
        mqtt_publish_table(p_reports);
    }
}

ATTR_NORETURN
static void
adv_post_task(ATTR_UNUSED void *arg)
{
    esp_log_level_set(TAG, ESP_LOG_INFO);
    ESP_LOGI(TAG, "%s", __func__);
    bool flag_connected = false;

    while (1)
    {
        // for thread safety copy the advertisements to a separate buffer for posting
        portENTER_CRITICAL(&adv_table_mux);
        adv_reports_buf         = adv_reports;
        adv_reports.num_of_advs = 0; // clear the table
        portEXIT_CRITICAL(&adv_table_mux);

        adv_post_log(&adv_reports_buf);

        if (!flag_connected)
        {
            flag_connected = adv_post_check_is_connected();
        }
        else
        {
            flag_connected = adv_post_check_is_disconnected();
        }

        if (0 != adv_reports_buf.num_of_advs)
        {
            if (flag_connected)
            {
                adv_post_retransmit_advs(&adv_reports_buf);
            }
            else
            {
                ESP_LOGW(TAG, "Can't send, no network connection");
            }
        }

        vTaskDelay(pdMS_TO_TICKS(ADV_POST_INTERVAL));
    }
}

void
adv_post_init(void)
{
    adv_reports.num_of_advs = 0;
    api_callbacks_reg((void *)&adv_callback_func_tbl);
    const uint32_t stack_size = 1024U * 4U;
    if (!os_task_create(&adv_post_task, "adv_post_task", stack_size, NULL, 1, NULL))
    {
        LOG_ERR("Can't create thread");
    }
}
