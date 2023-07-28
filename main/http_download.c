/**
 * @file http_download.c
 * @author TheSomeMan
 * @date 2020-10-26
 * @copyright Ruuvi Innovations Ltd, license BSD-3-Clause.
 */

#include "http_download.h"
#include <string.h>
#include <esp_task_wdt.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "str_buf.h"
#include "os_malloc.h"
#include "json_helper.h"
#include "time_str.h"
#include "ruuvi_gateway.h"
#include "http_server_cb.h"
#include "gw_status.h"
#include "os_str.h"
#include "http_server_resp.h"

#if RUUVI_TESTS_HTTP_SERVER_CB
#define LOG_LOCAL_LEVEL LOG_LEVEL_DEBUG
#else
#define LOG_LOCAL_LEVEL LOG_LEVEL_INFO
#endif
#include "log.h"
static const char TAG[] = "http_download";

#define HTTP_BUFFER_SIZE    (2048)
#define HTTP_BUFFER_SIZE_TX (1024)

#define BASE_10 (10U)

typedef struct http_download_cb_info_t
{
    esp_http_client_handle_t   http_handle;
    uint32_t                   content_length;
    bool                       flag_feed_task_watchdog;
    uint32_t                   offset;
    http_download_cb_on_data_t cb_on_data;
    void*                      p_user_data;
} http_download_cb_info_t;

static void
suspend_relaying_and_wait(const bool flag_force_free_memory)
{
    const bool flag_wait_until_relaying_stopped = true;
    if (flag_force_free_memory || gw_cfg_get_mqtt_use_mqtt_over_ssl_or_wss())
    {
        gw_status_suspend_relaying(flag_wait_until_relaying_stopped);
    }
    else
    {
        gw_status_suspend_http_relaying(flag_wait_until_relaying_stopped);
    }
}

static void
resume_relaying_and_wait(const bool flag_force_free_memory)
{
    const bool flag_wait_until_relaying_resumed = true;
    if (flag_force_free_memory || gw_cfg_get_mqtt_use_mqtt_over_ssl_or_wss())
    {
        gw_status_resume_relaying(flag_wait_until_relaying_resumed);
    }
    else
    {
        gw_status_resume_http_relaying(flag_wait_until_relaying_resumed);
    }
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
            if (NULL != p_cb_info->cb_on_data)
            {
                if (!p_cb_info->cb_on_data(
                        p_evt->data,
                        p_evt->data_len,
                        p_cb_info->offset,
                        p_cb_info->content_length,
                        (http_resp_code_e)esp_http_client_get_status_code(p_cb_info->http_handle),
                        p_cb_info->p_user_data))
                {
                    LOG_ERR("HTTP_EVENT_ON_DATA: cb_on_data failed");
                    return ESP_FAIL;
                }
            }
            p_cb_info->offset += p_evt->data_len;
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
    LOG_DBG("http_wait_until_async_req_completed: finished");

    return resp;
}

static esp_http_client_config_t*
http_download_create_config(
    const http_download_param_t* const          p_param,
    const esp_http_client_method_t              http_method,
    const ruuvi_gw_cfg_http_auth_basic_t* const p_auth_basic,
    http_event_handle_cb const                  p_event_handler,
    void* const                                 p_cb_info)
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
    return p_http_config;
}

static esp_http_client_handle_t
http_client_init(const http_download_param_with_auth_t* const p_param, esp_http_client_config_t* const p_http_config)
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

static const char*
http_client_method_to_str(const esp_http_client_method_t http_method)
{
    static const char* const g_http_method_to_str[HTTP_METHOD_MAX] = {
        [HTTP_METHOD_GET]         = "GET",
        [HTTP_METHOD_POST]        = "POST",
        [HTTP_METHOD_PUT]         = "PUT",
        [HTTP_METHOD_PATCH]       = "PATCH",
        [HTTP_METHOD_DELETE]      = "DELETE",
        [HTTP_METHOD_HEAD]        = "HEAD",
        [HTTP_METHOD_NOTIFY]      = "NOTIFY",
        [HTTP_METHOD_SUBSCRIBE]   = "SUBSCRIBE",
        [HTTP_METHOD_UNSUBSCRIBE] = "UNSUBSCRIBE",
        [HTTP_METHOD_OPTIONS]     = "OPTIONS",
        [HTTP_METHOD_COPY]        = "COPY",
        [HTTP_METHOD_MOVE]        = "MOVE",
        [HTTP_METHOD_LOCK]        = "LOCK",
        [HTTP_METHOD_UNLOCK]      = "UNLOCK",
        [HTTP_METHOD_PROPFIND]    = "PROPFIND",
        [HTTP_METHOD_PROPPATCH]   = "PROPPATCH",
        [HTTP_METHOD_MKCOL]       = "MKCOL",
    };
    if (http_method >= HTTP_METHOD_MAX)
    {
        return "UNK";
    }
    const char* const p_method_str = g_http_method_to_str[http_method];
    if (NULL == p_method_str)
    {
        return "UNK";
    }
    return p_method_str;
}

bool
http_download_or_check(
    const esp_http_client_method_t               http_method,
    const http_download_param_with_auth_t* const p_param,
    http_download_cb_on_data_t const             p_cb_on_data,
    void* const                                  p_user_data,
    http_resp_code_e* const                      p_http_resp_code)
{
    LOG_INFO("HTTP download/check: Method=%s, URL: '%s'", http_client_method_to_str(http_method), p_param->base.p_url);

    if ((GW_CFG_HTTP_AUTH_TYPE_NONE != p_param->auth_type) && (NULL == p_param->p_http_auth))
    {
        LOG_ERR("HTTP download/check: Auth type is not NONE, but p_http_auth is NULL");
        *p_http_resp_code = HTTP_RESP_CODE_400;
        return false;
    }
    if (!gw_status_is_network_connected())
    {
        LOG_ERR("HTTP download/check: No network connection");
        *p_http_resp_code = HTTP_RESP_CODE_503;
        return false;
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

    http_download_cb_info_t* p_cb_info = os_calloc(1, sizeof(*p_cb_info));
    if (NULL == p_cb_info)
    {
        LOG_ERR("Can't allocate memory");
        *p_http_resp_code = HTTP_RESP_CODE_500;
        return false;
    }
    p_cb_info->cb_on_data              = p_cb_on_data;
    p_cb_info->p_user_data             = p_user_data;
    p_cb_info->http_handle             = NULL;
    p_cb_info->content_length          = 0;
    p_cb_info->offset                  = 0;
    p_cb_info->flag_feed_task_watchdog = p_param->base.flag_feed_task_watchdog;

    esp_http_client_config_t* p_http_config = http_download_create_config(
        &p_param->base,
        http_method,
        p_auth_basic,
        &http_download_event_handler,
        p_cb_info);
    if (NULL == p_http_config)
    {
        LOG_ERR("Can't allocate memory for http_config");
        os_free(p_cb_info);
        *p_http_resp_code = HTTP_RESP_CODE_500;
        return false;
    }

    LOG_DBG("suspend_relaying_and_wait");
    suspend_relaying_and_wait(p_param->base.flag_free_memory);
    LOG_DBG("suspend_relaying_and_wait: finished");

    p_cb_info->http_handle = http_client_init(p_param, p_http_config);
    if (NULL == p_cb_info->http_handle)
    {
        os_free(p_http_config);
        os_free(p_cb_info);
        resume_relaying_and_wait(p_param->base.flag_free_memory);
        *p_http_resp_code = HTTP_RESP_CODE_500;
        return false;
    }

    LOG_DBG("Call http_download_by_handle");
    http_server_resp_t resp = http_download_by_handle(
        p_cb_info->http_handle,
        p_param->base.flag_feed_task_watchdog,
        p_param->base.timeout_seconds);

    LOG_DBG("Call esp_http_client_cleanup");
    const esp_err_t err = esp_http_client_cleanup(p_cb_info->http_handle);
    if (ESP_OK != err)
    {
        LOG_ERR_ESP(err, "esp_http_client_cleanup failed");
    }

    os_free(p_http_config);
    os_free(p_cb_info);
    resume_relaying_and_wait(p_param->base.flag_free_memory);

    if ((HTTP_CONTENT_LOCATION_HEAP == resp.content_location) && (NULL != resp.select_location.memory.p_buf))
    {
        os_free(resp.select_location.memory.p_buf);
    }

    *p_http_resp_code = resp.http_resp_code;
    return (HTTP_RESP_CODE_200 == resp.http_resp_code) ? true : false;
}

bool
http_download_with_auth(
    const http_download_param_with_auth_t* const p_param,
    http_download_cb_on_data_t const             p_cb_on_data,
    void* const                                  p_user_data)
{
    http_resp_code_e http_resp_code = HTTP_RESP_CODE_200;

    return http_download_or_check(HTTP_METHOD_GET, p_param, p_cb_on_data, p_user_data, &http_resp_code);
}

bool
http_check_with_auth(const http_download_param_with_auth_t* const p_param, http_resp_code_e* const p_http_resp_code)
{
    return http_download_or_check(HTTP_METHOD_HEAD, p_param, NULL, NULL, p_http_resp_code);
}

#endif

bool
http_download(
    const http_download_param_with_auth_t* const p_param,
    http_download_cb_on_data_t const             p_cb_on_data,
    void* const                                  p_user_data)
{
    return http_download_with_auth(p_param, p_cb_on_data, p_user_data);
}

static bool
cb_on_http_download_json_data(
    const uint8_t* const   p_buf,
    const size_t           buf_size,
    const size_t           offset,
    const size_t           content_length,
    const http_resp_code_e http_resp_code,
    void*                  p_user_data)
{
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
    if (HTTP_RESP_CODE_200 != http_resp_code)
    {
        LOG_ERR("Got HTTP error %d: %.*s", (printf_int_t)http_resp_code, (printf_int_t)buf_size, (const char*)p_buf);
        p_info->is_error = true;
        if (NULL != p_info->p_json_buf)
        {
            os_free(p_info->p_json_buf);
            p_info->p_json_buf = NULL;
        }
        return false;
    }

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
    if (!http_download_with_auth(p_params, &cb_on_http_download_json_data, &info))
    {
        LOG_ERR("http_download failed for URL: %s", p_params->base.p_url);
        info.is_error = true;
    }
    if (HTTP_RESP_CODE_200 != info.http_resp_code)
    {
        if (NULL == info.p_json_buf)
        {
            LOG_ERR("http_download failed, HTTP resp code %d", (printf_int_t)info.http_resp_code);
        }
        else
        {
            LOG_ERR("http_download failed, HTTP resp code %d: %s", (printf_int_t)info.http_resp_code, info.p_json_buf);
            os_free(info.p_json_buf);
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
http_download_latest_release_info(const bool flag_free_memory)
{
    const http_download_param_with_auth_t params = {
        .base = {
            .p_url                   = "https://api.github.com/repos/ruuvi/ruuvi.gateway_esp.c/releases/latest",
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
