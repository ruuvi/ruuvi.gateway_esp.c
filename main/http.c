/**
 * @file http.c
 * @author Jukka Saari
 * @date 2019-11-27
 * @copyright Ruuvi Innovations Ltd, license BSD-3-Clause.
 */

#include "http.h"
#include <string.h>
#include <time.h>
#include <errno.h>
#include <esp_task_wdt.h>
#include "cJSON.h"
#include "cjson_wrap.h"
#include "esp_http_client.h"
#include "ruuvi_gateway.h"
#include "http_json.h"
#include "leds.h"
#include "os_str.h"
#include "os_malloc.h"
#include "hmac_sha256.h"
#include "adv_post.h"
#include "fw_update.h"
#include "gw_mac.h"
#include "str_buf.h"
#include "gw_status.h"

#define LOG_LOCAL_LEVEL LOG_LEVEL_INFO
#include "log.h"

#define BASE_10 (10U)

typedef int esp_http_client_len_t;
typedef int esp_http_client_http_status_code_t;

typedef struct http_download_cb_info_t
{
    http_download_cb_on_data_t cb_on_data;
    void *const                p_user_data;
    esp_http_client_handle_t   http_handle;
    uint32_t                   content_length;
    uint32_t                   offset;
    bool                       flag_feed_task_watchdog;
} http_download_cb_info_t;

typedef struct http_client_config_t
{
    esp_http_client_config_t     esp_http_client_config;
    ruuvi_gw_cfg_http_url_t      http_url;
    ruuvi_gw_cfg_http_user_t     http_user;
    ruuvi_gw_cfg_http_password_t http_pass;
} http_client_config_t;

typedef struct http_async_info_t
{
    http_client_config_t     http_client_config;
    esp_http_client_handle_t p_http_client_handle;
    cjson_wrap_str_t         cjson_str;
    bool                     flag_sending_advs;
} http_async_info_t;

static const char TAG[] = "http";

static http_async_info_t g_http_async_info;

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
                uint32_t period_seconds = os_str_to_uint32_cptr(p_evt->header_value, NULL, BASE_10);
                if ((0 == period_seconds)
                    || (period_seconds > (TIME_UNITS_MINUTES_PER_HOUR * TIME_UNITS_SECONDS_PER_MINUTE)))
                {
                    LOG_WARN("X-Ruuvi-Gateway-Rate: Got incorrect value: %s", p_evt->header_value);
                    period_seconds = ADV_POST_DEFAULT_INTERVAL_SECONDS;
                }
                adv_post_set_period(period_seconds * TIME_UNITS_MS_PER_SECOND);
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

static void
http_init_client_config(
    http_client_config_t *const               p_http_client_config,
    const ruuvi_gw_cfg_http_url_t *const      p_url,
    const ruuvi_gw_cfg_http_user_t *const     p_user,
    const ruuvi_gw_cfg_http_password_t *const p_password)
{
    p_http_client_config->esp_http_client_config = (esp_http_client_config_t) {
        .url                         = &p_http_client_config->http_url.buf[0],
        .host                        = NULL,
        .port                        = 0,
        .username                    = &p_http_client_config->http_user.buf[0],
        .password                    = &p_http_client_config->http_pass.buf[0],
        .auth_type                   = ('\0' != p_user->buf[0]) ? HTTP_AUTH_TYPE_BASIC : HTTP_AUTH_TYPE_NONE,
        .path                        = NULL,
        .query                       = NULL,
        .cert_pem                    = NULL,
        .client_cert_pem             = NULL,
        .client_key_pem              = NULL,
        .method                      = HTTP_METHOD_POST,
        .timeout_ms                  = 0,
        .disable_auto_redirect       = false,
        .max_redirection_count       = 0,
        .event_handler               = &http_post_event_handler,
        .transport_type              = HTTP_TRANSPORT_UNKNOWN,
        .buffer_size                 = 0,
        .buffer_size_tx              = 0,
        .user_data                   = NULL,
        .is_async                    = true,
        .use_global_ca_store         = false,
        .skip_cert_common_name_check = false,
    };
    p_http_client_config->http_url  = *p_url;
    p_http_client_config->http_user = *p_user;
    p_http_client_config->http_pass = *p_password;
}

static void
http_init_client_config_for_http_record(http_client_config_t *const p_http_client_config)
{
    const gw_cfg_t *p_gw_cfg = gw_cfg_lock_ro();
    http_init_client_config(
        p_http_client_config,
        &p_gw_cfg->ruuvi_cfg.http.http_url,
        &p_gw_cfg->ruuvi_cfg.http.http_user,
        &p_gw_cfg->ruuvi_cfg.http.http_pass);
    gw_cfg_unlock_ro(&p_gw_cfg);
}

static void
http_init_client_config_for_http_status(http_client_config_t *const p_http_client_config)
{
    const gw_cfg_t *p_gw_cfg = gw_cfg_lock_ro();
    http_init_client_config(
        p_http_client_config,
        &p_gw_cfg->ruuvi_cfg.http_stat.http_stat_url,
        &p_gw_cfg->ruuvi_cfg.http_stat.http_stat_user,
        &p_gw_cfg->ruuvi_cfg.http_stat.http_stat_pass);
    gw_cfg_unlock_ro(&p_gw_cfg);
}

static bool
http_send_async(http_async_info_t *const p_http_async_info)
{
    const esp_http_client_config_t *const p_http_config = &p_http_async_info->http_client_config.esp_http_client_config;

    const char *const p_msg = p_http_async_info->cjson_str.p_str;
    LOG_INFO("### HTTP POST to URL=%s", p_http_config->url);
    LOG_INFO("HTTP POST DATA:\n%s", p_msg);

    esp_http_client_set_post_field(
        p_http_async_info->p_http_client_handle,
        p_msg,
        (esp_http_client_len_t)strlen(p_msg));
    esp_http_client_set_header(p_http_async_info->p_http_client_handle, "Content-Type", "application/json");

    const hmac_sha256_str_t hmac_sha256_str = hmac_sha256_calc_str(p_msg);
    if (hmac_sha256_is_str_valid(&hmac_sha256_str))
    {
        esp_http_client_set_header(p_http_async_info->p_http_client_handle, "Ruuvi-HMAC-SHA256", hmac_sha256_str.buf);
    }

    LOG_DBG("esp_http_client_perform");
    const esp_err_t err = esp_http_client_perform(p_http_async_info->p_http_client_handle);
    if (ESP_ERR_HTTP_EAGAIN != err)
    {
        LOG_ERR_ESP(err, "### HTTP POST to URL=%s: request failed", p_http_config->url);
        esp_http_client_cleanup(p_http_async_info->p_http_client_handle);
        p_http_async_info->p_http_client_handle = NULL;
        return false;
    }
    return true;
}

bool
http_send_advs(const adv_report_table_t *const p_reports, const uint32_t nonce, const bool flag_use_timestamps)
{
    const ruuvi_gw_cfg_coordinates_t coordinates       = gw_cfg_get_coordinates();
    http_async_info_t *              p_http_async_info = &g_http_async_info;

    p_http_async_info->flag_sending_advs = true;
    p_http_async_info->cjson_str         = cjson_wrap_str_null();
    if (!http_json_create_records_str(
            p_reports,
            flag_use_timestamps,
            time(NULL),
            gw_cfg_get_nrf52_mac_addr(),
            coordinates.buf,
            true,
            nonce,
            &p_http_async_info->cjson_str))
    {
        LOG_ERR("Not enough memory to generate json");
        return false;
    }

    http_init_client_config_for_http_record(&p_http_async_info->http_client_config);

    p_http_async_info->p_http_client_handle = esp_http_client_init(
        &p_http_async_info->http_client_config.esp_http_client_config);
    if (NULL == p_http_async_info->p_http_client_handle)
    {
        LOG_ERR("HTTP POST to URL=%s: Can't init http client", p_http_async_info->http_client_config.http_url.buf);
        cjson_wrap_free_json_str(&p_http_async_info->cjson_str);
        return false;
    }

    if (!http_send_async(p_http_async_info))
    {
        esp_http_client_cleanup(p_http_async_info->p_http_client_handle);
        p_http_async_info->p_http_client_handle = NULL;
        cjson_wrap_free_json_str(&p_http_async_info->cjson_str);
        return false;
    }
    return true;
}

bool
http_send_statistics(const http_json_statistics_info_t *const p_stat_info, const adv_report_table_t *const p_reports)
{
    http_async_info_t *p_http_async_info = &g_http_async_info;

    p_http_async_info->flag_sending_advs = false;
    p_http_async_info->cjson_str         = cjson_wrap_str_null();
    if (!http_json_create_status_str(p_stat_info, p_reports, &p_http_async_info->cjson_str))
    {
        LOG_ERR("Not enough memory to generate status json");
        return false;
    }

    http_init_client_config_for_http_status(&p_http_async_info->http_client_config);

    p_http_async_info->p_http_client_handle = esp_http_client_init(
        &p_http_async_info->http_client_config.esp_http_client_config);
    if (NULL == p_http_async_info->p_http_client_handle)
    {
        LOG_ERR("HTTP POST to URL=%s: Can't init http client", p_http_async_info->http_client_config.http_url.buf);
        cjson_wrap_free_json_str(&p_http_async_info->cjson_str);
        return false;
    }

    if (!http_send_async(p_http_async_info))
    {
        esp_http_client_cleanup(p_http_async_info->p_http_client_handle);
        p_http_async_info->p_http_client_handle = NULL;
        cjson_wrap_free_json_str(&p_http_async_info->cjson_str);
        return false;
    }
    return true;
}

bool
http_async_poll(void)
{
    http_async_info_t *p_http_async_info = &g_http_async_info;

    LOG_DBG("esp_http_client_perform");
    const esp_err_t err = esp_http_client_perform(p_http_async_info->p_http_client_handle);
    if (ESP_ERR_HTTP_EAGAIN == err)
    {
        LOG_DBG("esp_http_client_perform: ESP_ERR_HTTP_EAGAIN");
        return false;
    }

    bool flag_success = false;
    if (ESP_OK == err)
    {
        const esp_http_client_http_status_code_t http_status = esp_http_client_get_status_code(
            p_http_async_info->p_http_client_handle);
        if (HTTP_RESP_CODE_200 == http_status)
        {
            LOG_INFO(
                "### HTTP POST to URL=%s: STATUS=%d",
                p_http_async_info->http_client_config.esp_http_client_config.url,
                http_status);
            if (p_http_async_info->flag_sending_advs)
            {
                adv_post_last_successful_network_comm_timestamp_update();
            }
            flag_success = true;
        }
        else
        {
            LOG_ERR(
                "### HTTP POST to URL=%s: STATUS=%d",
                p_http_async_info->http_client_config.esp_http_client_config.url,
                http_status);
        }
    }
    else
    {
        LOG_ERR_ESP(
            err,
            "### HTTP POST to URL=%s: failed",
            p_http_async_info->http_client_config.esp_http_client_config.url);
    }

    esp_http_client_cleanup(p_http_async_info->p_http_client_handle);
    p_http_async_info->p_http_client_handle = NULL;

    cjson_wrap_free_json_str(&p_http_async_info->cjson_str);
    p_http_async_info->cjson_str = cjson_wrap_str_null();

    if (p_http_async_info->flag_sending_advs)
    {
        if (flag_success)
        {
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
    }
    return true;
}

static void
http_download_feed_task_watchdog_if_needed(const bool flag_feed_task_watchdog)
{
    if (flag_feed_task_watchdog)
    {
        LOG_DBG("Feed watchdog");
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
            http_download_feed_task_watchdog_if_needed(p_cb_info->flag_feed_task_watchdog);
            break;

        case HTTP_EVENT_ON_CONNECTED:
            LOG_INFO("HTTP_EVENT_ON_CONNECTED");
            http_download_feed_task_watchdog_if_needed(p_cb_info->flag_feed_task_watchdog);
            p_cb_info->offset = 0;
            break;

        case HTTP_EVENT_HEADER_SENT:
            LOG_INFO("HTTP_EVENT_HEADER_SENT");
            http_download_feed_task_watchdog_if_needed(p_cb_info->flag_feed_task_watchdog);
            break;

        case HTTP_EVENT_ON_HEADER:
            LOG_INFO("HTTP_EVENT_ON_HEADER, key=%s, value=%s", p_evt->header_key, p_evt->header_value);
            http_download_feed_task_watchdog_if_needed(p_cb_info->flag_feed_task_watchdog);
            if (0 == strcasecmp(p_evt->header_key, "Content-Length"))
            {
                p_cb_info->offset         = 0;
                p_cb_info->content_length = os_str_to_uint32_cptr(p_evt->header_value, NULL, BASE_10);
            }
            break;

        case HTTP_EVENT_ON_DATA:
            LOG_DBG("HTTP_EVENT_ON_DATA, len=%d", p_evt->data_len);
            LOG_DUMP_VERBOSE(p_evt->data, p_evt->data_len, "<--:");
            http_download_feed_task_watchdog_if_needed(p_cb_info->flag_feed_task_watchdog);
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
            http_download_feed_task_watchdog_if_needed(p_cb_info->flag_feed_task_watchdog);
            break;

        case HTTP_EVENT_DISCONNECTED:
            LOG_INFO("HTTP_EVENT_DISCONNECTED");
            http_download_feed_task_watchdog_if_needed(p_cb_info->flag_feed_task_watchdog);
            break;

        default:
            break;
    }
    return ESP_OK;
}

static bool
http_download_by_handle(esp_http_client_handle_t http_handle, const bool flag_feed_task_watchdog)
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

    LOG_DBG("esp_http_client_perform");
    err = esp_http_client_perform(http_handle);
    if (ESP_OK == err)
    {
        LOG_DBG(
            "HTTP GET Status = %d, content_length = %d",
            esp_http_client_get_status_code(http_handle),
            esp_http_client_get_content_length(http_handle));
        return true;
    }
    if (ESP_ERR_HTTP_EAGAIN != err)
    {
        LOG_ERR_ESP(
            err,
            "esp_http_client_perform failed, HTTP resp code %d (http_handle=%ld)",
            (printf_int_t)esp_http_client_get_status_code(http_handle),
            (printf_long_t)http_handle);
        return false;
    }

    while (true)
    {
        LOG_DBG("esp_http_client_perform");
        err = esp_http_client_perform(http_handle);
        if (ESP_OK == err)
        {
            LOG_DBG(
                "HTTP GET Status = %d, content_length = %d",
                esp_http_client_get_status_code(http_handle),
                esp_http_client_get_content_length(http_handle));
            return true;
        }
        if (ESP_ERR_HTTP_EAGAIN == err)
        {
            LOG_DBG("esp_http_client_perform: ESP_ERR_HTTP_EAGAIN");
            vTaskDelay(pdMS_TO_TICKS(50));
            http_download_feed_task_watchdog_if_needed(flag_feed_task_watchdog);
        }
        else
        {
            LOG_ERR_ESP(err, "%s failed", "esp_http_client_perform");
            return false;
        }
    }
}

bool
http_download_with_auth(
    const char *const                     p_url,
    const TimeUnitsSeconds_t              timeout_seconds,
    const gw_cfg_remote_auth_type_e       gw_cfg_http_auth_type,
    const ruuvi_gw_cfg_http_auth_t *const p_http_auth,
    const http_header_item_t *const       p_extra_header_item,
    http_download_cb_on_data_t            p_cb_on_data,
    void *const                           p_user_data,
    const bool                            flag_feed_task_watchdog)
{
    http_download_cb_info_t cb_info = {
        .cb_on_data              = p_cb_on_data,
        .p_user_data             = p_user_data,
        .http_handle             = NULL,
        .content_length          = 0,
        .offset                  = 0,
        .flag_feed_task_watchdog = flag_feed_task_watchdog,
    };

    if ((GW_CFG_REMOTE_AUTH_TYPE_NO != gw_cfg_http_auth_type) && (NULL == p_http_auth))
    {
        LOG_ERR("Auth type is not NONE, but p_http_auth is NULL");
        return false;
    }
    const esp_http_client_auth_type_t http_client_auth_type = (GW_CFG_REMOTE_AUTH_TYPE_BASIC == gw_cfg_http_auth_type)
                                                                  ? HTTP_AUTH_TYPE_BASIC
                                                                  : HTTP_AUTH_TYPE_NONE;
    const esp_http_client_config_t http_config = {
        .url  = p_url,
        .host = NULL,
        .port = 0,

        .username  = (HTTP_AUTH_TYPE_BASIC == http_client_auth_type) ? p_http_auth->auth_basic.user.buf : NULL,
        .password  = (HTTP_AUTH_TYPE_BASIC == http_client_auth_type) ? p_http_auth->auth_basic.password.buf : NULL,
        .auth_type = http_client_auth_type,

        .path                        = NULL,
        .query                       = NULL,
        .cert_pem                    = NULL,
        .client_cert_pem             = NULL,
        .client_key_pem              = NULL,
        .user_agent                  = NULL,
        .method                      = HTTP_METHOD_GET,
        .timeout_ms                  = (int)(timeout_seconds * 1000),
        .disable_auto_redirect       = false,
        .max_redirection_count       = 0,
        .max_authorization_retries   = 0,
        .event_handler               = &http_download_event_handler,
        .transport_type              = HTTP_TRANSPORT_UNKNOWN,
        .buffer_size                 = 2048,
        .buffer_size_tx              = 1024,
        .user_data                   = &cb_info,
        .is_async                    = true,
        .use_global_ca_store         = false,
        .skip_cert_common_name_check = false,
        .keep_alive_enable           = false,
        .keep_alive_idle             = 0,
        .keep_alive_interval         = 0,
        .keep_alive_count            = 0,
    };
    LOG_INFO("http_download: URL: %s", p_url);
    if (HTTP_AUTH_TYPE_BASIC == http_client_auth_type)
    {
        LOG_INFO("http_download: Auth: Basic, Username: %s", p_http_auth->auth_basic.user.buf);
    }
    if (!gw_status_is_network_connected())
    {
        LOG_ERR("HTTP download failed - no network connection");
        return false;
    }

    cb_info.http_handle = esp_http_client_init(&http_config);
    if (NULL == cb_info.http_handle)
    {
        LOG_ERR("Can't init http client");
        return false;
    }

    if (GW_CFG_REMOTE_AUTH_TYPE_BEARER == gw_cfg_http_auth_type)
    {
        str_buf_t str_buf = str_buf_printf_with_alloc("Bearer %s", p_http_auth->auth_bearer.token.buf);
        if (NULL == str_buf.buf)
        {
            LOG_ERR("Can't allocate memory for bearer token");
            return false;
        }
        LOG_INFO(
            "http_download: Add HTTP header: %s: %s",
            "Authorization",
            (LOG_LOCAL_LEVEL >= LOG_LEVEL_DEBUG) ? str_buf.buf : "********");
        esp_http_client_set_header(cb_info.http_handle, "Authorization", str_buf.buf);
        str_buf_free_buf(&str_buf);
    }
    if (NULL != p_extra_header_item)
    {
        LOG_INFO("http_download: Add HTTP header: %s: %s", p_extra_header_item->p_key, p_extra_header_item->p_value);
        esp_http_client_set_header(cb_info.http_handle, p_extra_header_item->p_key, p_extra_header_item->p_value);
    }

    LOG_DBG("http_download_by_handle");
    const bool result = http_download_by_handle(cb_info.http_handle, flag_feed_task_watchdog);

    LOG_DBG("esp_http_client_cleanup");
    const esp_err_t err = esp_http_client_cleanup(cb_info.http_handle);
    if (ESP_OK != err)
    {
        LOG_ERR_ESP(err, "esp_http_client_cleanup failed");
    }
    return result;
}

bool
http_download(
    const char *const          p_url,
    const TimeUnitsSeconds_t   timeout_seconds,
    http_download_cb_on_data_t p_cb_on_data,
    void *const                p_user_data,
    const bool                 flag_feed_task_watchdog)
{
    return http_download_with_auth(
        p_url,
        timeout_seconds,
        GW_CFG_REMOTE_AUTH_TYPE_NO,
        NULL,
        NULL,
        p_cb_on_data,
        p_user_data,
        flag_feed_task_watchdog);
}
