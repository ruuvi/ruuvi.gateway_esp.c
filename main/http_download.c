/**
 * @file http_download.c
 * @author TheSomeMan
 * @date 2020-10-26
 * @copyright Ruuvi Innovations Ltd, license BSD-3-Clause.
 */

#include "http_download.h"
#include <string.h>
#include <inttypes.h>
#include <esp_task_wdt.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "str_buf.h"
#include "os_malloc.h"
#include "ruuvi_gateway.h"
#include "http_server_cb.h"
#include "gw_status.h"
#include "os_str.h"
#include "http_server_resp.h"
#if (!RUUVI_TESTS_HTTP_SERVER_CB)
#include "tls_shared_buf.h"
#endif

#if RUUVI_TESTS_HTTP_SERVER_CB
#define LOG_LOCAL_LEVEL LOG_LEVEL_DEBUG
#else
#define LOG_LOCAL_LEVEL LOG_LEVEL_INFO
#endif
#include "log.h"
static const char TAG[] = "http_download";

#define HTTP_BUFFER_SIZE    (2048)
#define HTTP_BUFFER_SIZE_TX (1024)

#define HTTP_MIN_DOMAIN_NAME_LEN (3U)

#define BASE_10 (10U)

#define HTTP_RANGE_HEADER_MAX_LITERAL "bytes=4294967295-4294967295"
#define HTTP_RANGE_HEADER_BUF_SIZE    (sizeof(HTTP_RANGE_HEADER_MAX_LITERAL))

typedef struct http_download_range_header_t
{
    char buf[HTTP_RANGE_HEADER_BUF_SIZE];
} http_download_range_header_t;

typedef struct http_download_cb_info_t
{
    esp_http_client_handle_t     http_handle;
    uint32_t                     content_length;
    bool                         flag_feed_task_watchdog;
    http_download_range_header_t range_header;
    uint32_t                     range_start;
    uint32_t                     offset;
    http_download_cb_on_data_t   cb_on_data;
    void*                        p_user_data;
} http_download_cb_info_t;

typedef struct http_download_range_info_t
{
    bool     is_range_used;
    uint32_t range_start_u32;
    uint32_t range_end_u32;
} http_download_range_info_t;

bool
http_download_is_url_valid(const char* const p_url)
{
    const char url_http_prefix[] = "http://";
    if (0 == strncmp(p_url, url_http_prefix, strlen(url_http_prefix)))
    {
        const char* p_host = &p_url[strlen(url_http_prefix)];
        if (strlen(p_host) < HTTP_MIN_DOMAIN_NAME_LEN)
        {
            return false;
        }
        if (NULL == strchr(p_host, '.'))
        {
            return false;
        }
        return true;
    }
    const char url_https_prefix[] = "https://";
    if (0 == strncmp(p_url, url_https_prefix, strlen(url_https_prefix)))
    {
        const char* p_host = &p_url[strlen(url_https_prefix)];
        if (strlen(p_host) < HTTP_MIN_DOMAIN_NAME_LEN)
        {
            return false;
        }
        if (NULL == strchr(p_host, '.'))
        {
            return false;
        }
        return true;
    }
    return false;
}

#if (!RUUVI_TESTS_HTTP_SERVER_CB)
static esp_err_t
http_download_event_handler(esp_http_client_event_t* p_evt)
{
    http_download_cb_info_t* const p_cb_info = p_evt->user_data;
    switch (p_evt->event_id)
    {
        case HTTP_EVENT_ERROR:
            LOG_ERR("HTTP_EVENT_ERROR");
            http_feed_task_watchdog_if_needed(p_cb_info->flag_feed_task_watchdog);
            break;

        case HTTP_EVENT_ON_CONNECTED:
            LOG_INFO("HTTP_EVENT_ON_CONNECTED");
            http_feed_task_watchdog_if_needed(p_cb_info->flag_feed_task_watchdog);
            p_cb_info->offset = 0;
            break;

        case HTTP_EVENT_HEADER_SENT:
            LOG_INFO("HTTP_EVENT_HEADER_SENT");
            http_feed_task_watchdog_if_needed(p_cb_info->flag_feed_task_watchdog);
            break;

        case HTTP_EVENT_ON_HEADER:
            LOG_INFO("HTTP_EVENT_ON_HEADER, key=%s, value=%s", p_evt->header_key, p_evt->header_value);
            http_feed_task_watchdog_if_needed(p_cb_info->flag_feed_task_watchdog);
            if (0 == strcasecmp(p_evt->header_key, "Content-Length"))
            {
                p_cb_info->offset         = 0;
                p_cb_info->content_length = os_str_to_uint32_cptr(p_evt->header_value, NULL, BASE_10);
            }
            break;

        case HTTP_EVENT_ON_DATA:
            LOG_DBG("HTTP_EVENT_ON_DATA, len=%d", p_evt->data_len);
            LOG_DUMP_VERBOSE(p_evt->data, p_evt->data_len, "<--:");
            http_feed_task_watchdog_if_needed(p_cb_info->flag_feed_task_watchdog);
            const uint32_t offset = p_cb_info->offset;
            p_cb_info->offset += p_evt->data_len;
            if (NULL != p_cb_info->cb_on_data)
            {
                if (!p_cb_info->cb_on_data(
                        p_evt->data,
                        p_evt->data_len,
                        offset,
                        p_cb_info->content_length,
                        (http_resp_code_e)esp_http_client_get_status_code(p_cb_info->http_handle),
                        p_cb_info->range_start,
                        p_cb_info->p_user_data))
                {
                    LOG_ERR("HTTP_EVENT_ON_DATA: cb_on_data failed");
                    return ESP_FAIL;
                }
            }
            break;

        case HTTP_EVENT_ON_FINISH:
            LOG_INFO("HTTP_EVENT_ON_FINISH");
            http_feed_task_watchdog_if_needed(p_cb_info->flag_feed_task_watchdog);
            break;

        case HTTP_EVENT_DISCONNECTED:
            LOG_INFO("HTTP_EVENT_DISCONNECTED");
            http_feed_task_watchdog_if_needed(p_cb_info->flag_feed_task_watchdog);
            break;

        default:
            break;
    }
    return ESP_OK;
}

static http_server_resp_t
http_download_by_handle(
    esp_http_client_handle_t p_http_handle,
    const bool               flag_feed_task_watchdog,
    const TimeUnitsSeconds_t timeout_seconds)
{
    esp_err_t err = esp_http_client_set_header(p_http_handle, "Accept", "text/html,application/octet-stream,*/*");
    if (ESP_OK != err)
    {
        LOG_ERR("%s failed", "esp_http_client_set_header");
        return http_server_resp_500();
    }
    err = esp_http_client_set_header(p_http_handle, "User-Agent", "RuuviGateway");
    if (ESP_OK != err)
    {
        LOG_ERR("%s failed", "esp_http_client_set_header");
        return http_server_resp_500();
    }

    LOG_DBG("esp_http_client_perform");
    err = esp_http_client_perform(p_http_handle);
    if (ESP_OK == err)
    {
        LOG_DBG(
            "HTTP GET Status = %d, content_length = %d",
            esp_http_client_get_status_code(p_http_handle),
            esp_http_client_get_content_length(p_http_handle));
        return http_server_resp_err(esp_http_client_get_status_code(p_http_handle));
    }
    if (ESP_ERR_HTTP_EAGAIN != err)
    {
        LOG_ERR_ESP(
            err,
            "esp_http_client_perform failed, HTTP resp code %d (p_http_handle=%ld)",
            (printf_int_t)esp_http_client_get_status_code(p_http_handle),
            (printf_long_t)p_http_handle);
        return http_server_resp_502();
    }

    LOG_DBG("http_wait_until_async_req_completed");
    http_server_resp_t resp = http_wait_until_async_req_completed(
        p_http_handle,
        NULL,
        flag_feed_task_watchdog,
        timeout_seconds);
    LOG_DBG("http_wait_until_async_req_completed: finished, http_resp_code=%d", resp.http_resp_code);

#if LOG_LOCAL_LEVEL >= LOG_LEVEL_DEBUG
    const bool flag_is_in_memory = (HTTP_CONTENT_LOCATION_FLASH_MEM == resp.content_location)
                                   || (HTTP_CONTENT_LOCATION_STATIC_MEM == resp.content_location)
                                   || (HTTP_CONTENT_LOCATION_HEAP == resp.content_location);
    const char* p_json = (flag_is_in_memory && (NULL != resp.select_location.memory.p_buf))
                             ? (const char*)resp.select_location.memory.p_buf
                             : NULL;
    LOG_DBG("Resp: resp_code=%d, content: %s", resp.http_resp_code, (NULL != p_json) ? p_json : "<NULL>");
#endif

    return resp;
}

static esp_http_client_config_t*
http_download_create_config(
    const http_download_param_t* const          p_param,
    const esp_http_client_method_t              http_method,
    const ruuvi_gw_cfg_http_auth_basic_t* const p_auth_basic,
    http_event_handle_cb const                  p_event_handler,
    void* const                                 p_cb_info,
    const esp_transport_ssl_buf_cfg_t* const    p_tls_buf_info)
{
    esp_http_client_config_t* p_http_config = os_calloc(1, sizeof(*p_http_config));
    if (NULL == p_http_config)
    {
        LOG_ERR("Can't allocate memory for http_config");
        return false;
    }
    p_http_config->url  = p_param->p_url;
    p_http_config->host = NULL;
    p_http_config->port = 0;

    p_http_config->username  = (NULL != p_auth_basic) ? p_auth_basic->user.buf : NULL;
    p_http_config->password  = (NULL != p_auth_basic) ? p_auth_basic->password.buf : NULL;
    p_http_config->auth_type = (NULL != p_auth_basic) ? HTTP_AUTH_TYPE_BASIC : HTTP_AUTH_TYPE_NONE;

    p_http_config->path                        = NULL;
    p_http_config->query                       = NULL;
    p_http_config->cert_pem                    = p_param->p_server_cert;
    p_http_config->client_cert_pem             = p_param->p_client_cert;
    p_http_config->client_key_pem              = p_param->p_client_key;
    p_http_config->user_agent                  = NULL;
    p_http_config->method                      = http_method;
    const int32_t timeout_ms                   = (int32_t)p_param->timeout_seconds * (int32_t)TIME_UNITS_MS_PER_SECOND;
    p_http_config->timeout_ms                  = timeout_ms;
    p_http_config->disable_auto_redirect       = false;
    p_http_config->max_redirection_count       = 0;
    p_http_config->max_authorization_retries   = 0;
    p_http_config->event_handler               = p_event_handler;
    p_http_config->transport_type              = HTTP_TRANSPORT_UNKNOWN;
    p_http_config->buffer_size                 = HTTP_BUFFER_SIZE;
    p_http_config->buffer_size_tx              = HTTP_BUFFER_SIZE_TX;
    p_http_config->user_data                   = p_cb_info;
    p_http_config->is_async                    = true;
    p_http_config->use_global_ca_store         = false;
    p_http_config->skip_cert_common_name_check = false;
    p_http_config->keep_alive_enable           = false;
    p_http_config->keep_alive_idle             = 0;
    p_http_config->keep_alive_interval         = 0;
    p_http_config->keep_alive_count            = 0;
    p_http_config->ssl_buf_cfg                 = *p_tls_buf_info;
    return p_http_config;
}

static esp_http_client_handle_t
http_client_init(
    const http_download_param_with_auth_t* const p_param,
    const esp_http_client_config_t* const        p_http_config)
{
    esp_http_client_handle_t p_http_handle = esp_http_client_init(p_http_config);
    if (NULL == p_http_handle)
    {
        LOG_ERR("Can't init http client");
        return NULL;
    }

    if (!http_handle_add_authorization_if_needed(p_http_handle, p_param->auth_type, p_param->p_http_auth))
    {
        esp_http_client_cleanup(p_http_handle);
        return NULL;
    }

    if (NULL != p_param->p_extra_header_item)
    {
        LOG_INFO(
            "http_check: Add HTTP header: %s: %s",
            p_param->p_extra_header_item->p_key,
            p_param->p_extra_header_item->p_value);
        const esp_err_t err = esp_http_client_set_header(
            p_http_handle,
            p_param->p_extra_header_item->p_key,
            p_param->p_extra_header_item->p_value);
        if (ESP_OK != err)
        {
            LOG_ERR(
                "esp_http_client_set_header failed: key=%s, val=%s",
                p_param->p_extra_header_item->p_key,
                p_param->p_extra_header_item->p_value);
            esp_http_client_cleanup(p_http_handle);
            return NULL;
        }
    }
    return p_http_handle;
}

static bool
http_download_check_if_range_used(
    const size_t                      range_start,
    const size_t                      range_end,
    http_download_range_info_t* const p_range_info)
{
    if ((0 == range_start) && (0 == range_end))
    {
        p_range_info->is_range_used   = false;
        p_range_info->range_start_u32 = 0;
        p_range_info->range_end_u32   = 0;
        return true;
    }
    // The max size of a firmware update cannot exceed UINT32_MAX.
    if ((range_start > UINT32_MAX) || (range_end > UINT32_MAX) || ((0 != range_end) && (range_start > range_end)))
    {
        LOG_ERR("HTTP download/check: Invalid range: start=%zu, end=%zu", range_start, range_end);
        return false;
    }
    LOG_INFO("HTTP download/check: Range: start=%zu, end=%zu", range_start, range_end);
    p_range_info->is_range_used   = true;
    p_range_info->range_start_u32 = (uint32_t)range_start;
    p_range_info->range_end_u32   = (uint32_t)range_end;
    return true;
}

bool
http_download_set_range_header_if_needed(
    http_download_cb_info_t* const          p_cb_info,
    const http_download_range_info_t* const p_range_info)
{
    if (p_range_info->is_range_used)
    {
        if (0 == p_range_info->range_end_u32)
        {
            snprintf(
                p_cb_info->range_header.buf,
                sizeof(p_cb_info->range_header.buf),
                "bytes=%" PRIu32 "-",
                p_range_info->range_start_u32);
        }
        else
        {
            snprintf(
                p_cb_info->range_header.buf,
                sizeof(p_cb_info->range_header.buf),
                "bytes=%" PRIu32 "-%" PRIu32,
                p_range_info->range_start_u32,
                p_range_info->range_end_u32);
        }
        const esp_err_t err = esp_http_client_set_header(p_cb_info->http_handle, "Range", p_cb_info->range_header.buf);
        if (ESP_OK != err)
        {
            LOG_ERR("%s failed", "esp_http_client_set_header");
            return false;
        }
    }
    return true;
}

static http_server_resp_t
http_download_or_check_stage_5(
    esp_http_client_handle_t const http_handle,
    const bool                     flag_feed_task_watchdog,
    const TimeUnitsSeconds_t       timeout_seconds)
{
    LOG_DBG("Call http_download_by_handle");
    const http_server_resp_t resp = http_download_by_handle(http_handle, flag_feed_task_watchdog, timeout_seconds);

#if LOG_LOCAL_LEVEL >= LOG_LEVEL_DEBUG
    const bool flag_is_in_memory = (HTTP_CONTENT_LOCATION_FLASH_MEM == resp.content_location)
                                   || (HTTP_CONTENT_LOCATION_STATIC_MEM == resp.content_location)
                                   || (HTTP_CONTENT_LOCATION_HEAP == resp.content_location);
    const char* p_json = (flag_is_in_memory && (NULL != resp.select_location.memory.p_buf))
                             ? (const char*)resp.select_location.memory.p_buf
                             : NULL;
    LOG_DBG("Resp: resp_code=%d, content: %s", resp.http_resp_code, (NULL != p_json) ? p_json : "<NULL>");
#endif

    return resp;
}

static http_server_resp_t
http_download_or_check_stage_4(
    const http_download_param_with_auth_t* const p_param,
    http_download_cb_info_t* const               p_cb_info,
    const esp_http_client_config_t* const        p_http_config,
    const http_download_range_info_t* const      p_range_info)
{
    p_cb_info->http_handle = http_client_init(p_param, p_http_config);
    if (NULL == p_cb_info->http_handle)
    {
        LOG_ERR("Can't init http client");
        return http_server_resp_500();
    }
    if (!http_download_set_range_header_if_needed(p_cb_info, p_range_info))
    {
        return http_server_resp_500();
    }

    const http_server_resp_t resp = http_download_or_check_stage_5(
        p_cb_info->http_handle,
        p_param->base.flag_feed_task_watchdog,
        p_param->base.timeout_seconds);

    LOG_DBG("Call esp_http_client_cleanup");
    const esp_err_t err = esp_http_client_cleanup(p_cb_info->http_handle);
    if (ESP_OK != err)
    {
        LOG_ERR_ESP(err, "esp_http_client_cleanup failed");
    }
    p_cb_info->http_handle = NULL;

    return resp;
}

static http_server_resp_t
http_download_or_check_stage_3(
    const esp_http_client_method_t               http_method,
    const http_download_param_with_auth_t* const p_param,
    const ruuvi_gw_cfg_http_auth_basic_t* const  p_auth_basic,
    http_download_cb_info_t* const               p_cb_info,
    const http_download_range_info_t* const      p_range_info,
    const esp_transport_ssl_buf_cfg_t* const     p_tls_buf_info)
{
    esp_http_client_config_t* p_http_config = http_download_create_config(
        &p_param->base,
        http_method,
        p_auth_basic,
        &http_download_event_handler,
        p_cb_info,
        p_tls_buf_info);
    if (NULL == p_http_config)
    {
        LOG_ERR("Can't allocate memory for http_config");
        return http_server_resp_500();
    }

    const http_server_resp_t resp = http_download_or_check_stage_4(p_param, p_cb_info, p_http_config, p_range_info);

    os_free(p_http_config);

    return resp;
}

static http_server_resp_t
http_download_or_check_stage_2(
    const esp_http_client_method_t               http_method,
    const http_download_param_with_auth_t* const p_param,
    http_download_cb_on_data_t const             p_cb_on_data,
    void* const                                  p_user_data,
    const ruuvi_gw_cfg_http_auth_basic_t* const  p_auth_basic,
    const http_download_range_info_t* const      p_range_info,
    const esp_transport_ssl_buf_cfg_t* const     p_tls_buf_info)
{
    http_download_cb_info_t* p_cb_info = os_calloc(1, sizeof(*p_cb_info));
    if (NULL == p_cb_info)
    {
        LOG_ERR("Can't allocate memory");
        return http_server_resp_500();
    }

    p_cb_info->cb_on_data              = p_cb_on_data;
    p_cb_info->p_user_data             = p_user_data;
    p_cb_info->http_handle             = NULL;
    p_cb_info->content_length          = 0;
    p_cb_info->range_start             = p_range_info->is_range_used ? p_range_info->range_start_u32 : 0;
    p_cb_info->offset                  = 0;
    p_cb_info->flag_feed_task_watchdog = p_param->base.flag_feed_task_watchdog;

    const http_server_resp_t resp
        = http_download_or_check_stage_3(http_method, p_param, p_auth_basic, p_cb_info, p_range_info, p_tls_buf_info);

    os_free(p_cb_info);

    return resp;
}

static http_server_resp_t
http_download_or_check_stage_1(
    const esp_http_client_method_t               http_method,
    const http_download_param_with_auth_t* const p_param,
    http_download_cb_on_data_t const             p_cb_on_data,
    void* const                                  p_user_data,
    const ruuvi_gw_cfg_http_auth_basic_t* const  p_auth_basic,
    const http_download_range_info_t* const      p_range_info,
    const esp_transport_ssl_buf_cfg_t* const     p_tls_buf_info)
{
    http_async_info_t* const p_http_async_info = http_get_async_info();

    LOG_DBG("os_sema_wait_immediate: p_http_async_sema");
    if (!os_sema_wait_immediate(p_http_async_info->p_http_async_sema))
    {
        /* Because the amount of memory is limited and may not be sufficient for two simultaneous queries,
         * this function should only be called from a single thread. */
        LOG_ERR("This function is called twice from two different threads, which can lead to memory shortages");
        assert(0);
        return http_server_resp_500();
    }
    p_http_async_info->p_task = xTaskGetCurrentTaskHandle();

    const http_server_resp_t resp = http_download_or_check_stage_2(
        http_method,
        p_param,
        p_cb_on_data,
        p_user_data,
        p_auth_basic,
        p_range_info,
        p_tls_buf_info);

    os_sema_signal(p_http_async_info->p_http_async_sema);

    return resp;
}

static esp_transport_ssl_buf_cfg_t
http_download_get_tls_shared_buf_info(
    const bool                        flag_use_big_tls_buf,
    tls_shared_buf_https_download_t** p_p_buf_big,
    tls_shared_buf_https_post_t**     p_p_buf_small)
{
    *p_p_buf_big   = NULL;
    *p_p_buf_small = NULL;

    esp_transport_ssl_buf_cfg_t tls_shared_buf_cfg = { 0 };
    if (flag_use_big_tls_buf)
    {
        if (gw_status_is_relaying_via_http_enabled() || gw_status_is_relaying_via_mqtt_enabled())
        {
            LOG_ERR(
                "Relaying via HTTP/MQTT is enabled: %d, %d",
                gw_status_is_relaying_via_http_enabled(),
                gw_status_is_relaying_via_mqtt_enabled());
            assert(0);
        }
        tls_shared_buf_https_download_t* p_buf_big = tls_shared_buf_get_https_download();
        tls_shared_buf_cfg.p_ssl_in_buf            = p_buf_big->in_buf;
        tls_shared_buf_cfg.p_ssl_out_buf           = p_buf_big->out_buf;
        tls_shared_buf_cfg.ssl_in_buf_len          = MBEDTLS_SSL_IN_BUFFER_LEN;
        tls_shared_buf_cfg.ssl_out_buf_len         = MBEDTLS_SSL_OUT_BUFFER_LEN;
        tls_shared_buf_cfg.ssl_in_content_len      = MBEDTLS_SSL_IN_CONTENT_LEN;
        tls_shared_buf_cfg.ssl_out_content_len     = MBEDTLS_SSL_OUT_CONTENT_LEN;
        *p_p_buf_big                               = p_buf_big;
    }
    else
    {
        if (gw_status_is_relaying_via_http_enabled())
        {
            LOG_ERR("Relaying via HTTP is enabled");
            assert(0);
        }
        tls_shared_buf_https_post_t* p_buf_small = tls_shared_buf_get_https_post();
        tls_shared_buf_cfg.p_ssl_in_buf          = p_buf_small->in_buf;
        tls_shared_buf_cfg.ssl_in_buf_len        = MBEDTLS_SSL_IN_BUFFER_LEN_CALC(RUUVI_HTTPS_POST_TLS_IN_CONTENT_LEN);
        tls_shared_buf_cfg.ssl_in_content_len    = RUUVI_HTTPS_POST_TLS_IN_CONTENT_LEN;
        tls_shared_buf_cfg.p_ssl_out_buf         = p_buf_small->out_buf;
        tls_shared_buf_cfg.ssl_out_buf_len     = MBEDTLS_SSL_OUT_BUFFER_LEN_CALC(RUUVI_HTTPS_POST_TLS_OUT_CONTENT_LEN);
        tls_shared_buf_cfg.ssl_out_content_len = RUUVI_HTTPS_POST_TLS_OUT_CONTENT_LEN;
        *p_p_buf_small                         = p_buf_small;
    }
    return tls_shared_buf_cfg;
}

static http_server_resp_t
http_download_or_check(
    const esp_http_client_method_t               http_method,
    const http_download_param_with_auth_t* const p_param,
    http_download_cb_on_data_t const             p_cb_on_data,
    void* const                                  p_user_data,
    const bool                                   flag_use_big_tls_buf)
{
    LOG_INFO("HTTP download/check: Method=%s, URL: '%s'", http_client_method_to_str(http_method), p_param->base.p_url);
    http_download_range_info_t range_info = {
        .is_range_used   = false,
        .range_start_u32 = 0,
        .range_end_u32   = 0,
    };
    if (!http_download_check_if_range_used(p_param->base.range_start, p_param->base.range_end, &range_info))
    {
        return http_server_resp_400();
    }

    if (!http_download_is_url_valid(p_param->base.p_url))
    {
        LOG_ERR("HTTP download/check: Invalid URL: '%s'", p_param->base.p_url);
        return http_server_resp_400();
    }

    if ((GW_CFG_HTTP_AUTH_TYPE_NONE != p_param->auth_type) && (NULL == p_param->p_http_auth))
    {
        LOG_ERR("HTTP download/check: Auth type is not NONE, but p_http_auth is NULL");
        return http_server_resp_400();
    }
    if (!gw_status_is_network_connected())
    {
        LOG_ERR("HTTP download/check: No network connection");
        return http_server_resp_503();
    }

    const ruuvi_gw_cfg_http_auth_basic_t* p_auth_basic = NULL;
    if (GW_CFG_HTTP_AUTH_TYPE_BASIC == p_param->auth_type)
    {
        LOG_INFO("HTTP download/check: Auth: Basic, Username: %s", p_param->p_http_auth->auth_basic.user.buf);
        LOG_INFO(
            "HTTP download/check: Auth: Basic, Password: %s",
            (LOG_LOCAL_LEVEL >= LOG_LEVEL_DEBUG) ? p_param->p_http_auth->auth_basic.password.buf : "******");
        p_auth_basic = &p_param->p_http_auth->auth_basic;
    }

    const bool flag_wait_relaying_completed = true;

    if (flag_use_big_tls_buf && gw_cfg_get_mqtt_use_mqtt_over_ssl_or_wss())
    {
        LOG_DBG("suspend_relaying and wait");
        gw_status_suspend_relaying(flag_wait_relaying_completed);
        LOG_DBG("suspend_relaying: finished");
    }
    else
    {
        LOG_DBG("suspend_http_relaying and wait");
        gw_status_suspend_http_relaying(flag_wait_relaying_completed);
        LOG_DBG("suspend_relaying_and_wait: finished");
    }

    tls_shared_buf_https_download_t*  p_buf_big    = NULL;
    tls_shared_buf_https_post_t*      p_buf_small  = NULL;
    const esp_transport_ssl_buf_cfg_t tls_buf_info = http_download_get_tls_shared_buf_info(
        flag_use_big_tls_buf,
        &p_buf_big,
        &p_buf_small);

    const http_server_resp_t resp = http_download_or_check_stage_1(
        http_method,
        p_param,
        p_cb_on_data,
        p_user_data,
        p_auth_basic,
        &range_info,
        &tls_buf_info);

    if (flag_use_big_tls_buf)
    {
        tls_shared_buf_unlock_https_download(&p_buf_big);
    }
    else
    {
        tls_shared_buf_unlock_https_post(&p_buf_small);
    }

    if (flag_use_big_tls_buf && gw_cfg_get_mqtt_use_mqtt_over_ssl_or_wss())
    {
        LOG_DBG("resume_relaying and wait");
        gw_status_resume_relaying(flag_wait_relaying_completed);
        LOG_DBG("resume_relaying: finished");
    }
    else
    {
        LOG_DBG("resume_http_relaying and wait");
        gw_status_resume_http_relaying(flag_wait_relaying_completed);
        LOG_DBG("resume_http_relaying: finished");
    }

    return resp;
}

http_server_resp_t
http_download_with_auth(
    const http_download_param_with_auth_t* const p_param,
    http_download_cb_on_data_t const             p_cb_on_data,
    void* const                                  p_user_data,
    const bool                                   flag_use_big_tls_buf)
{
    return http_download_or_check(HTTP_METHOD_GET, p_param, p_cb_on_data, p_user_data, flag_use_big_tls_buf);
}

bool
http_check_with_auth(const http_download_param_with_auth_t* const p_param, http_resp_code_e* const p_http_resp_code)
{
    const bool flag_use_big_tls_buf = false;

    http_server_resp_t resp = http_download_or_check(HTTP_METHOD_HEAD, p_param, NULL, NULL, flag_use_big_tls_buf);

    *p_http_resp_code = resp.http_resp_code;
    if ((HTTP_CONTENT_LOCATION_HEAP == resp.content_location) && (NULL != resp.select_location.memory.p_buf))
    {
        os_free(resp.select_location.memory.p_buf);
    }
    return (HTTP_RESP_CODE_200 == resp.http_resp_code) ? true : false;
}

#endif

bool
http_download(
    const http_download_param_with_auth_t* const p_param,
    http_download_cb_on_data_t const             p_cb_on_data,
    void* const                                  p_user_data,
    bool* const                                  p_flag_allow_retry)
{
    const bool flag_use_big_tls_buf = true;

    http_server_resp_t resp = http_download_with_auth(p_param, p_cb_on_data, p_user_data, flag_use_big_tls_buf);

    if ((HTTP_CONTENT_LOCATION_HEAP == resp.content_location) && (NULL != resp.select_location.memory.p_buf))
    {
        os_free(resp.select_location.memory.p_buf);
    }
    if (NULL != p_flag_allow_retry)
    {
        *p_flag_allow_retry = (HTTP_RESP_CODE_502 == resp.http_resp_code) ? true : false;
    }
    const bool is_range_used = (0 != p_param->base.range_start) || (0 != p_param->base.range_end);

    return ((HTTP_RESP_CODE_200 == resp.http_resp_code)
            || (is_range_used && (HTTP_RESP_CODE_206 == resp.http_resp_code)))
               ? true
               : false;
}

static bool
cb_on_http_download_json_data(
    const uint8_t* const   p_buf,
    const size_t           buf_size,
    const size_t           offset,
    const size_t           content_length,
    const http_resp_code_e http_resp_code,
    const size_t           range_start,
    void*                  p_user_data)
{
    (void)range_start;
    LOG_INFO("%s: buf_size=%lu", __func__, (printf_ulong_t)buf_size);

    http_server_download_info_t* const p_info = p_user_data;
    if (p_info->is_error)
    {
        LOG_ERR("Error occurred while downloading");
        return false;
    }
    if ((HTTP_RESP_CODE_301 == http_resp_code) || (HTTP_RESP_CODE_302 == http_resp_code))
    {
        LOG_INFO("Got HTTP error %d: Redirect to another location", (printf_int_t)http_resp_code);
        return true;
    }
    p_info->http_resp_code = http_resp_code;

    if (NULL == p_info->p_json_buf)
    {
        if (0 == content_length)
        {
            p_info->json_buf_size = buf_size + 1;
        }
        else
        {
            p_info->json_buf_size = content_length + 1;
        }
        p_info->p_json_buf = os_calloc(1, p_info->json_buf_size);
        if (NULL == p_info->p_json_buf)
        {
            p_info->is_error = true;
            LOG_ERR("Can't allocate %lu bytes for json file", (printf_ulong_t)p_info->json_buf_size);
            return false;
        }
    }
    else
    {
        if (0 == content_length)
        {
            p_info->json_buf_size += buf_size;
            LOG_INFO("Reallocate %lu bytes", (printf_ulong_t)p_info->json_buf_size);
            if (!os_realloc_safe_and_clean((void**)&p_info->p_json_buf, p_info->json_buf_size))
            {
                p_info->is_error = true;
                LOG_ERR("Can't allocate %lu bytes for json file", (printf_ulong_t)p_info->json_buf_size);
                return false;
            }
        }
    }
    if ((offset >= p_info->json_buf_size) || ((offset + buf_size) >= p_info->json_buf_size))
    {
        p_info->is_error = true;
        if (NULL != p_info->p_json_buf)
        {
            os_free(p_info->p_json_buf);
            p_info->p_json_buf = NULL;
        }
        LOG_ERR(
            "Buffer overflow while downloading json file: json_buf_size=%lu, offset=%lu, len=%lu",
            (printf_ulong_t)p_info->json_buf_size,
            (printf_ulong_t)offset,
            (printf_ulong_t)buf_size);
        return false;
    }
    memcpy(&p_info->p_json_buf[offset], p_buf, buf_size);
    p_info->p_json_buf[offset + buf_size] = '\0';
    return true;
}

http_server_download_info_t
http_download_json(const http_download_param_with_auth_t* const p_params)
{
    http_server_download_info_t info = {
        .is_error       = false,
        .http_resp_code = HTTP_RESP_CODE_200,
        .p_json_buf     = NULL,
        .json_buf_size  = 0,
    };
    const TickType_t download_started_at_tick = xTaskGetTickCount();

    const bool         flag_use_big_tls_buf = false;
    http_server_resp_t resp                 = http_download_with_auth(
        p_params,
        &cb_on_http_download_json_data,
        &info,
        flag_use_big_tls_buf);
    if (HTTP_RESP_CODE_200 != resp.http_resp_code)
    {
        info.is_error       = true;
        info.http_resp_code = resp.http_resp_code;

        const bool flag_is_in_memory = (HTTP_CONTENT_LOCATION_FLASH_MEM == resp.content_location)
                                       || (HTTP_CONTENT_LOCATION_STATIC_MEM == resp.content_location)
                                       || (HTTP_CONTENT_LOCATION_HEAP == resp.content_location);
        const char* p_json = (flag_is_in_memory && (NULL != resp.select_location.memory.p_buf))
                                 ? (const char*)resp.select_location.memory.p_buf
                                 : NULL;
        if (NULL != p_json)
        {
            if (NULL != info.p_json_buf)
            {
                os_free(info.p_json_buf);
                info.p_json_buf = NULL;
            }
            LOG_ERR(
                "http_download failed for URL: %s, resp_code=%d, content: %s",
                p_params->base.p_url,
                resp.http_resp_code,
                p_json);
            str_buf_t str_buf = str_buf_printf_with_alloc("%s", p_json);
            info.p_json_buf   = str_buf.buf;
        }
        else
        {
            LOG_ERR(
                "http_download failed for URL: %s, resp_code=%d, content: %s",
                p_params->base.p_url,
                resp.http_resp_code,
                (NULL != info.p_json_buf) ? info.p_json_buf : "<NULL>");
        }
    }
    else if (HTTP_RESP_CODE_200 != info.http_resp_code)
    {
        if (NULL == info.p_json_buf)
        {
            LOG_ERR("http_download failed, HTTP resp code %d", (printf_int_t)info.http_resp_code);
        }
        else
        {
            LOG_ERR("http_download failed, HTTP resp code %d: %s", (printf_int_t)info.http_resp_code, info.p_json_buf);
        }
        info.is_error = true;
    }
    else if (NULL == info.p_json_buf)
    {
        LOG_ERR("http_download returned NULL buffer");
        info.is_error = true;
    }
    else
    {
        // MISRA C:2012, 15.7 - All if...else if constructs shall be terminated with an else statement
    }
    if (info.is_error && (HTTP_RESP_CODE_200 == info.http_resp_code))
    {
        info.http_resp_code = HTTP_RESP_CODE_400;
    }
    const TickType_t download_completed_within_ticks = xTaskGetTickCount() - download_started_at_tick;
    LOG_INFO("%s: completed within %u ticks", __func__, (printf_uint_t)download_completed_within_ticks);
    return info;
}

http_server_download_info_t
http_download_firmware_update_info(const char* const p_url, const bool flag_free_memory)
{
    const http_download_param_with_auth_t params = {
        .base = {
            .p_url                   = p_url,
            .timeout_seconds         = HTTP_DOWNLOAD_FW_RELEASE_INFO_TIMEOUT_SECONDS,
            .flag_feed_task_watchdog = true,
            .flag_free_memory        = flag_free_memory,
            .p_server_cert           = NULL,
            .p_client_cert           = NULL,
            .p_client_key            = NULL,
        },
        .auth_type = GW_CFG_HTTP_AUTH_TYPE_NONE,
        .p_http_auth = NULL,
        .p_extra_header_item = NULL,
    };
    return http_download_json(&params);
}

http_resp_code_e
http_check(const http_download_param_with_auth_t* const p_params)
{
    const TickType_t download_started_at_tick = xTaskGetTickCount();
    http_resp_code_e http_resp_code           = HTTP_RESP_CODE_200;

    if (!http_check_with_auth(p_params, &http_resp_code))
    {
        LOG_ERR("http_check failed for URL: %s", p_params->base.p_url);
    }
    const TickType_t download_completed_within_ticks = xTaskGetTickCount() - download_started_at_tick;
    LOG_INFO("%s: completed within %u ticks", __func__, (printf_uint_t)download_completed_within_ticks);
    return http_resp_code;
}
