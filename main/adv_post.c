#include "freertos/FreeRTOS.h"
#include "freertos/FreeRTOSConfig.h"
#include "freertos/portmacro.h"
#include "freertos/task.h"
#include "freertos/projdefs.h"
#include "esp_err.h"
#include "esp_log.h"
#include <string.h>
#include <ctype.h>
#include "cJSON.h"
#include <time.h>
#include "mqtt.h"
#include "ruuvidongle.h"
#include "adv_post.h"
#include "http.h"
#include "ruuvi_board_gwesp.h"
#include "ruuvi_endpoints.h"
#include "ruuvi_endpoint_ca_uart.h"

portMUX_TYPE adv_table_mux = portMUX_INITIALIZER_UNLOCKED;

struct adv_report_table adv_reports;
struct adv_report_table adv_reports_buf;

static const char * ADV_POST_TASK_TAG = "ADV_POST_TASK";

/**
 * @brief Print given binary into hex string.
 *
 * Example {0xA0, 0xBB, 0x31} -> "A0BB31"
 *
 * @param[out] hexstr String to print into. Must be at least 2 * binlen + 1 bytes long.
 * @param[in]  bin Binary to print from.
 * @param[in]  binlen Size of binary in bytes.
 */
static void bin2hex (char * const hexstr, const size_t hexstr_size,
                     const uint8_t * const bin, size_t binlen)
{
    size_t ii = 0;

    for (ii = 0; ii < binlen; ii++)
    {
        if ( (2 * ii + 3) > hexstr_size)
        {
            break;
        }

        sprintf (hexstr + (2 * ii), "%02X", bin[ii]);
    }

    hexstr[2 * ii] = 0;
}

static esp_err_t adv_put_to_table (const adv_report_t * const p_adv)
{
    portENTER_CRITICAL (&adv_table_mux);
    bool found = false;
    esp_err_t ret = ESP_OK;

    // Check if we already have advertisement with this MAC
    for (int i = 0; i < adv_reports.num_of_advs; i++)
    {
        char * mac = adv_reports.table[i].tag_mac;

        if (strcmp (p_adv->tag_mac, mac) == 0)
        {
            // Yes, update data.
            found = true;
            adv_reports.table[i] = *p_adv;
        }
    }

    //not found from the table, insert if not full
    if (!found)
    {
        if (adv_reports.num_of_advs < MAX_ADVS_TABLE)
        {
            adv_reports.table[adv_reports.num_of_advs++] = *p_adv;
        }
        else
        {
            ret = ESP_ERR_NO_MEM;
        }
    }

    portEXIT_CRITICAL (&adv_table_mux);
    return ret;
}

static bool is_hexstr (char * str)
{
    for (int i = 0; i < strlen (str); i++)
    {
        if (isxdigit (str[i]) == 0)
        {
            return ESP_ERR_INVALID_ARG;
        }
    }

    return ESP_OK;
}

static int is_adv_report_valid (adv_report_t * adv)
{
    esp_err_t err = ESP_OK;

    if (is_hexstr (adv->tag_mac) != ESP_OK)
    {
        err = ESP_ERR_INVALID_ARG;
    }

    if (is_hexstr (adv->data) != ESP_OK)
    {
        err = ESP_ERR_INVALID_ARG;
    }

    return err;
}

static int parse_adv_report_from_uart (const re_ca_uart_payload_t * const msg,
                                       adv_report_t * adv)
{
    esp_err_t err = ESP_OK;

    if (NULL == msg)
    {
        err = ESP_ERR_INVALID_ARG;
    }
    else if (NULL == adv)
    {
        err = ESP_ERR_INVALID_ARG;
    }
    else if (msg->cmd != RE_CA_UART_ADV_RPRT)
    {
        err = ESP_ERR_INVALID_ARG;
    }
    else
    {
        const re_ca_uart_ble_adv_t * const report = & (msg->params.adv);
        time_t now = 0;
        time (&now);
        adv->rssi = report->rssi_db;
        adv->timestamp = now;
        bin2hex (adv->tag_mac, sizeof (adv->tag_mac), report->mac, RE_CA_UART_MAC_BYTES);
        bin2hex (adv->data, sizeof (adv->data), report->adv, report->adv_len);

        if (is_adv_report_valid (adv))
        {
            err = ESP_ERR_INVALID_ARG;
        }


    }

    return err;
}

void adv_post_send (void * arg)
{
    adv_report_t adv_report;

    // Refactor into function
    if (parse_adv_report_from_uart ( (re_ca_uart_payload_t *) arg, &adv_report) == ESP_OK)
    {
        int ret = adv_put_to_table (&adv_report);

        if (ret == ESP_ERR_NO_MEM)
        {
            ESP_LOGW (ADV_POST_TASK_TAG, "Adv report table full, adv dropped");
        }
    }
}

static void adv_post_task (void * arg)
{
    esp_log_level_set (ADV_POST_TASK_TAG, ESP_LOG_INFO);
    ESP_LOGI (ADV_POST_TASK_TAG, "%s", __func__);

    while (1)
    {
        adv_report_t * adv = 0;
        ESP_LOGI (ADV_POST_TASK_TAG, "advertisements in table:");

        for (int i = 0; i < adv_reports.num_of_advs; i++)
        {
            adv = &adv_reports.table[i];
            ESP_LOGI (ADV_POST_TASK_TAG, "i: %d, tag: %s, rssi: %d, data: %s, timestamp: %ld", i,
                      adv->tag_mac,
                      adv->rssi, adv->data, adv->timestamp);
        }

        //for thread safety copy the advertisements to a separate buffer for posting
        portENTER_CRITICAL (&adv_table_mux);
        adv_reports_buf = adv_reports;
        adv_reports.num_of_advs = 0; //clear the table
        portEXIT_CRITICAL (&adv_table_mux);
        EventBits_t status = xEventGroupGetBits (status_bits);

        if (adv_reports_buf.num_of_advs)
        {
            if ( ( (status & WIFI_CONNECTED_BIT)
                    || (status & ETH_CONNECTED_BIT)))
            {
                if (m_dongle_config.use_http)
                {
                    http_send_advs (&adv_reports_buf);
                }

                if (m_dongle_config.use_mqtt && (xEventGroupGetBits (status_bits) & MQTT_CONNECTED_BIT))
                {
                    mqtt_publish_table (&adv_reports_buf);
                }
            }
            else
            {
                ESP_LOGW (ADV_POST_TASK_TAG, "Can't send, no network connection");
            }
        }

        vTaskDelay (pdMS_TO_TICKS (ADV_POST_INTERVAL));
    }
}

void adv_post_init (void)
{
    adv_reports.num_of_advs = 0;
    xTaskCreate (adv_post_task, "adv_post_task", 1024 * 4, NULL, 1, NULL);
}
