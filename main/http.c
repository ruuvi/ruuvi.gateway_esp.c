/**
 * @file http.c
 * @author Jukka Saari
 * @date 2019-11-27
 * @copyright Ruuvi Innovations Ltd, license BSD-3-Clause.
 */

#include "http.h"
#include <string.h>
#include <time.h>
#include <esp_task_wdt.h>
#include "cJSON.h"
#include "cjson_wrap.h"
#include "esp_http_client.h"
#include "ruuvi_gateway.h"
#include "http_json.h"
#include "leds.h"
#include "os_str.h"
#include "hmac_sha256.h"
#include "adv_post.h"
#include "fw_update.h"

#define LOG_LOCAL_LEVEL LOG_LEVEL_INFO
#include "log.h"

#define BASE_10 (10U)

#define HTTP_DOWNLOAD_TIMEOUT_SECONDS (25)

typedef struct http_download_cb_info_t
{
    http_download_cb_on_data_t cb_on_data;
    void *const                p_user_data;
    esp_http_client_handle_t   http_handle;
    uint32_t                   content_length;
    uint32_t                   offset;
    bool                       flag_feed_task_watchdog;
} http_download_cb_info_t;

static const char TAG[] = "http";

static esp_err_t
http_post_event_handler(esp_http_client_event_t *p_evt)
{
    switch (p_evt->event_id)
    {
        case HTTP_EVENT_ERROR:
            LOG_ERR("HTTP_EVENT_ERROR");
            break;

        case HTTP_EVENT_ON_CONNECTED:
            LOG_DBG("HTTP_EVENT_ON_CONNECTED");
            break;

        case HTTP_EVENT_HEADER_SENT:
            LOG_DBG("HTTP_EVENT_HEADER_SENT");
            break;

        case HTTP_EVENT_ON_HEADER:
            LOG_DBG("HTTP_EVENT_ON_HEADER, key=%s, value=%s", p_evt->header_key, p_evt->header_value);
            if (0 == strcasecmp("Ruuvi-HMAC-KEY", p_evt->header_key))
            {
                if (!hmac_sha256_set_key_str(p_evt->header_value))
                {
                    LOG_ERR("Failed to update Ruuvi-HMAC-KEY");
                }
            }
            else if (0 == strcasecmp("X-Ruuvi-Gateway-Rate", p_evt->header_key))
            {
                uint32_t period_seconds = os_str_to_uint32_cptr(p_evt->header_value, NULL, 10U);
                if ((0 == period_seconds) || (period_seconds > (60U * 60U)))
                {
                    LOG_WARN("X-Ruuvi-Gateway-Rate: Got incorrect value: %s", p_evt->header_value);
                    period_seconds = ADV_POST_DEFAULT_INTERVAL_SECONDS;
                }
                adv_post_set_period(period_seconds * 1000U);
            }
            else
            {
                // MISRA C:2012, 15.7 - All if...else if constructs shall be terminated with an else statement
            }
            break;

        case HTTP_EVENT_ON_DATA:
            LOG_ERR("HTTP_EVENT_ON_DATA, len=%d: %.*s", p_evt->data_len, p_evt->data_len, (char *)p_evt->data);
            break;

        case HTTP_EVENT_ON_FINISH:
            LOG_DBG("HTTP_EVENT_ON_FINISH");
            break;

        case HTTP_EVENT_DISCONNECTED:
            LOG_DBG("HTTP_EVENT_DISCONNECTED");
            break;

        default:
            break;
    }
    return ESP_OK;
}

bool
http_send(const char *const p_msg)
{
    const ruuvi_gateway_config_t * p_gw_cfg    = gw_cfg_lock_ro();
    const esp_http_client_config_t http_config = {
        .url                   = p_gw_cfg->http.http_url,
        .host                  = NULL,
        .port                  = 0,
        .username              = p_gw_cfg->http.http_user,
        .password              = p_gw_cfg->http.http_pass,
        .auth_type             = ('\0' != p_gw_cfg->http.http_user[0]) ? HTTP_AUTH_TYPE_BASIC : HTTP_AUTH_TYPE_NONE,
        .path                  = NULL,
        .query                 = NULL,
        .cert_pem              = NULL,
        .client_cert_pem       = NULL,
        .client_key_pem        = NULL,
        .method                = HTTP_METHOD_POST,
        .timeout_ms            = 0,
        .disable_auto_redirect = false,
        .max_redirection_count = 0,
        .event_handler         = &http_post_event_handler,
        .transport_type        = HTTP_TRANSPORT_UNKNOWN,
        .buffer_size           = 0,
        .buffer_size_tx        = 0,
        .user_data             = NULL,
        .is_async              = false,
        .use_global_ca_store   = false,
        .skip_cert_common_name_check = false,
    };
    gw_cfg_unlock_ro(&p_gw_cfg);

    LOG_INFO("HTTP POST to URL=%s, DATA:\n%s", http_config.url, p_msg);

    esp_http_client_handle_t http_handle = esp_http_client_init(&http_config);
    if (NULL == http_handle)
    {
        LOG_ERR("HTTP POST to URL=%s: Can't init http client", http_config.url);
        return false;
    }

    esp_http_client_set_post_field(http_handle, p_msg, (int)strlen(p_msg));
    esp_http_client_set_header(http_handle, "Content-Type", "application/json");

    const hmac_sha256_str_t hmac_sha256_str = hmac_sha256_calc_str(p_msg);
    if (hmac_sha256_is_str_valid(&hmac_sha256_str))
    {
        esp_http_client_set_header(http_handle, "Ruuvi-HMAC-SHA256", hmac_sha256_str.buf);
    }

    bool result = true;

    esp_err_t err = esp_http_client_perform(http_handle);
    if (ESP_OK == err)
    {
        const int http_status = esp_http_client_get_status_code(http_handle);
        if (HTTP_RESP_CODE_200 == http_status)
        {
            LOG_INFO("HTTP POST to URL=%s: STATUS=%d", http_config.url, http_status);
        }
        else
        {
            LOG_ERR("HTTP POST to URL=%s: STATUS=%d", http_config.url, http_status);
            result = false;
        }
    }
    else
    {
        LOG_ERR_ESP(err, "HTTP POST to URL=%s: request failed", http_config.url);
        result = false;
    }

    err = esp_http_client_cleanup(http_handle);
    if (ESP_OK != err)
    {
        LOG_ERR_ESP(err, "HTTP POST to URL=%s: esp_http_client_cleanup failed", http_config.url);
    }
    return result;
}

bool
http_send_advs(const adv_report_table_t *const p_reports, const uint32_t nonce)
{
    const ruuvi_gw_cfg_coordinates_t coordinates = gw_cfg_get_coordinates();
    cjson_wrap_str_t                 json_str    = cjson_wrap_str_null();
    if (!http_create_json_str(p_reports, time(NULL), &g_gw_mac_sta_str, coordinates.buf, true, nonce, &json_str))
    {
        LOG_ERR("Not enough memory to generate json");
        return false;
    }
    bool is_post_successful = false;
    if (http_send(json_str.p_str))
    {
        is_post_successful = true;
        leds_indication_on_network_ok();
        if (!fw_update_mark_app_valid_cancel_rollback())
        {
            LOG_ERR("%s failed", "fw_update_mark_app_valid_cancel_rollback");
        }
    }
    else
    {
        leds_indication_network_no_connection();
    }
    cjson_wrap_free_json_str(&json_str);
    return is_post_successful;
}

static void
http_download_feed_task_watchdog_if_needed(const http_download_cb_info_t *const p_cb_info)
{
    if (p_cb_info->flag_feed_task_watchdog)
    {
        const esp_err_t err = esp_task_wdt_reset();
        if (ESP_OK != err)
        {
            LOG_ERR_ESP(err, "%s failed", "esp_task_wdt_reset");
        }
    }
}

static esp_err_t
http_download_event_handler(esp_http_client_event_t *p_evt)
{
    http_download_cb_info_t *const p_cb_info = p_evt->user_data;
    switch (p_evt->event_id)
    {
        case HTTP_EVENT_ERROR:
            LOG_ERR("HTTP_EVENT_ERROR");
            http_download_feed_task_watchdog_if_needed(p_cb_info);
            break;

        case HTTP_EVENT_ON_CONNECTED:
            LOG_INFO("HTTP_EVENT_ON_CONNECTED");
            http_download_feed_task_watchdog_if_needed(p_cb_info);
            p_cb_info->offset = 0;
            break;

        case HTTP_EVENT_HEADER_SENT:
            LOG_INFO("HTTP_EVENT_HEADER_SENT");
            http_download_feed_task_watchdog_if_needed(p_cb_info);
            break;

        case HTTP_EVENT_ON_HEADER:
            LOG_INFO("HTTP_EVENT_ON_HEADER, key=%s, value=%s", p_evt->header_key, p_evt->header_value);
            http_download_feed_task_watchdog_if_needed(p_cb_info);
            if (0 == strcasecmp(p_evt->header_key, "Content-Length"))
            {
                p_cb_info->offset         = 0;
                p_cb_info->content_length = os_str_to_uint32_cptr(p_evt->header_value, NULL, BASE_10);
            }
            break;

        case HTTP_EVENT_ON_DATA:
            LOG_DBG("HTTP_EVENT_ON_DATA, len=%d", p_evt->data_len);
            LOG_DUMP_VERBOSE(p_evt->data, p_evt->data_len, "<--:");
            http_download_feed_task_watchdog_if_needed(p_cb_info);
            if (!p_cb_info->cb_on_data(
                    p_evt->data,
                    p_evt->data_len,
                    p_cb_info->offset,
                    p_cb_info->content_length,
                    (http_resp_code_e)esp_http_client_get_status_code(p_cb_info->http_handle),
                    p_cb_info->p_user_data))
            {
                return ESP_FAIL;
            }
            p_cb_info->offset += p_evt->data_len;
            break;

        case HTTP_EVENT_ON_FINISH:
            LOG_INFO("HTTP_EVENT_ON_FINISH");
            http_download_feed_task_watchdog_if_needed(p_cb_info);
            break;

        case HTTP_EVENT_DISCONNECTED:
            LOG_INFO("HTTP_EVENT_DISCONNECTED");
            http_download_feed_task_watchdog_if_needed(p_cb_info);
            break;

        default:
            break;
    }
    return ESP_OK;
}

static bool
http_download_firmware_via_handle(esp_http_client_handle_t http_handle)
{
    esp_err_t err = esp_http_client_set_header(http_handle, "Accept", "text/html,application/octet-stream,*/*");
    if (ESP_OK != err)
    {
        LOG_ERR("%s failed", "esp_http_client_set_header");
        return false;
    }
    err = esp_http_client_set_header(http_handle, "User-Agent", "RuuviGateway");
    if (ESP_OK != err)
    {
        LOG_ERR("%s failed", "esp_http_client_set_header");
        return false;
    }

    err = esp_http_client_perform(http_handle);
    if (ESP_OK != err)
    {
        LOG_ERR_ESP(
            err,
            "esp_http_client_perform failed, HTTP resp code %d (http_handle=%ld)",
            (printf_int_t)esp_http_client_get_status_code(http_handle),
            (printf_long_t)http_handle);
        return false;
    }
    LOG_DBG(
        "HTTP GET Status = %d, content_length = %d",
        esp_http_client_get_status_code(http_handle),
        esp_http_client_get_content_length(http_handle));
    return true;
}

bool
http_download(
    const char *const          p_url,
    http_download_cb_on_data_t cb_on_data,
    void *const                p_user_data,
    const bool                 flag_feed_task_watchdog)
{
    http_download_cb_info_t cb_info = {
        .cb_on_data              = cb_on_data,
        .p_user_data             = p_user_data,
        .http_handle             = NULL,
        .content_length          = 0,
        .offset                  = 0,
        .flag_feed_task_watchdog = flag_feed_task_watchdog,
    };
    const esp_http_client_config_t http_config = {
        .url                         = p_url,
        .host                        = NULL,
        .port                        = 0,
        .username                    = NULL,
        .password                    = NULL,
        .auth_type                   = HTTP_AUTH_TYPE_NONE,
        .path                        = NULL,
        .query                       = NULL,
        .cert_pem                    = NULL,
        .client_cert_pem             = NULL,
        .client_key_pem              = NULL,
        .method                      = HTTP_METHOD_GET,
        .timeout_ms                  = HTTP_DOWNLOAD_TIMEOUT_SECONDS * 1000,
        .disable_auto_redirect       = false,
        .max_redirection_count       = 0,
        .event_handler               = &http_download_event_handler,
        .transport_type              = HTTP_TRANSPORT_UNKNOWN,
        .buffer_size                 = 2048,
        .buffer_size_tx              = 1024,
        .user_data                   = &cb_info,
        .is_async                    = false,
        .use_global_ca_store         = false,
        .skip_cert_common_name_check = false,
    };
    LOG_INFO("http_download");
    cb_info.http_handle = esp_http_client_init(&http_config);
    if (NULL == cb_info.http_handle)
    {
        LOG_ERR("Can't init http client");
        return false;
    }

    const bool result = http_download_firmware_via_handle(cb_info.http_handle);

    const esp_err_t err = esp_http_client_cleanup(cb_info.http_handle);
    if (ESP_OK != err)
    {
        LOG_ERR_ESP(err, "esp_http_client_cleanup failed");
    }
    return result;
}
