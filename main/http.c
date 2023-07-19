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
#include "str_buf.h"
#include "gw_status.h"
#include "reset_info.h"
#include "os_sema.h"
#include "os_malloc.h"
#include "http_server_resp.h"
#include "mqtt.h"
#include "http_server_cb.h"
#include "esp_tls.h"
#include "snprintf_with_esp_err_desc.h"
#include "gw_cfg_default.h"
#include "gw_cfg_storage.h"

#define LOG_LOCAL_LEVEL LOG_LEVEL_INFO
#include "log.h"

#if (LOG_LOCAL_LEVEL >= LOG_LEVEL_DEBUG)
#warning Debug log level prints out the passwords as a "plaintext".
#endif

#define BASE_10 (10U)

typedef int esp_http_client_len_t;
typedef int esp_http_client_http_status_code_t;

typedef struct http_download_cb_info_t
{
    http_download_cb_on_data_t cb_on_data;
    void*                      p_user_data;
    esp_http_client_handle_t   http_handle;
    uint32_t                   content_length;
    uint32_t                   offset;
    bool                       flag_feed_task_watchdog;
} http_download_cb_info_t;

typedef struct http_check_cb_info_t
{
    esp_http_client_handle_t http_handle;
    uint32_t                 content_length;
    bool                     flag_feed_task_watchdog;
} http_check_cb_info_t;

typedef struct http_client_config_t
{
    esp_http_client_config_t     esp_http_client_config;
    ruuvi_gw_cfg_http_url_t      http_url;
    ruuvi_gw_cfg_http_user_t     http_user;
    ruuvi_gw_cfg_http_password_t http_pass;
} http_client_config_t;

typedef struct http_post_cb_info_t
{
    uint32_t content_length;
    uint32_t offset;
    char*    p_buf;
} http_post_cb_info_t;

typedef enum http_post_recipient_e
{
    HTTP_POST_RECIPIENT_STATS,
    HTTP_POST_RECIPIENT_ADVS1,
    HTTP_POST_RECIPIENT_ADVS2,
} http_post_recipient_e;

typedef struct http_async_info_t
{
    os_sema_t                p_http_async_sema;
    os_sema_static_t         http_async_sema_mem;
    http_client_config_t     http_client_config;
    esp_http_client_handle_t p_http_client_handle;
    bool                     use_json_stream_gen;
    union
    {
        cjson_wrap_str_t   cjson_str;
        json_stream_gen_t* p_gen;
    } select;
    hmac_sha256_t         hmac_sha256;
    http_post_recipient_e recipient;
    os_task_handle_t      p_task;
    http_post_cb_info_t   http_post_cb_info;
} http_async_info_t;

static const char TAG[] = "http";

static http_async_info_t g_http_async_info;

static http_async_info_t*
get_http_async_info(void)
{
    http_async_info_t* p_http_async_info = &g_http_async_info;
    if (NULL == p_http_async_info->p_http_async_sema)
    {
        p_http_async_info->p_http_async_sema = os_sema_create_static(&p_http_async_info->http_async_sema_mem);
        os_sema_signal(p_http_async_info->p_http_async_sema);
    }
    return p_http_async_info;
}

static void
http_post_event_handler_on_header(const esp_http_client_event_t* const p_evt)
{
    LOG_DBG(
        "HTTP_EVENT_ON_HEADER, key=%s, value=%s, user_data=%p",
        p_evt->header_key,
        p_evt->header_value,
        p_evt->user_data);
    if (0 == strcasecmp("Ruuvi-HMAC-KEY", p_evt->header_key))
    {
        if (!adv_post_set_hmac_sha256_key(p_evt->header_value))
        {
            LOG_ERR("Failed to update Ruuvi-HMAC-KEY");
        }
    }
    else if (0 == strcasecmp("X-Ruuvi-Gateway-Rate", p_evt->header_key))
    {
        uint32_t period_seconds = os_str_to_uint32_cptr(p_evt->header_value, NULL, BASE_10);
        if ((0 == period_seconds) || (period_seconds > (TIME_UNITS_MINUTES_PER_HOUR * TIME_UNITS_SECONDS_PER_MINUTE)))
        {
            LOG_WARN("X-Ruuvi-Gateway-Rate: Got incorrect value: %s", p_evt->header_value);
            period_seconds = ADV_POST_DEFAULT_INTERVAL_SECONDS;
        }
        adv_post_set_default_period(period_seconds * TIME_UNITS_MS_PER_SECOND);
    }
    else if ((0 == strcasecmp("Content-Length", p_evt->header_key)) && (NULL != p_evt->user_data))
    {
        http_post_cb_info_t* const p_cb_info = p_evt->user_data;
        p_cb_info->content_length            = os_str_to_uint32_cptr(p_evt->header_value, NULL, BASE_10);
        p_cb_info->offset                    = 0;
        p_cb_info->p_buf                     = os_malloc(p_cb_info->content_length + 1);
        if (NULL == p_cb_info->p_buf)
        {
            LOG_ERR("Can't allocate %lu bytes for HTTP response", (printf_ulong_t)p_cb_info->content_length);
        }
        else
        {
            p_cb_info->p_buf[0] = '\0';
        }
    }
    else
    {
        // MISRA C:2012, 15.7 - All if...else if constructs shall be terminated with an else statement
    }
}

static void
http_post_event_handler_on_data(const esp_http_client_event_t* const p_evt)
{
    LOG_ERR("HTTP_EVENT_ON_DATA, len=%d: %.*s", p_evt->data_len, p_evt->data_len, (char*)p_evt->data);
    if (NULL != p_evt->user_data)
    {
        http_post_cb_info_t* const p_cb_info = p_evt->user_data;
        if (NULL != p_cb_info->p_buf)
        {
            const uint32_t remaining_buf = p_cb_info->content_length - p_cb_info->offset;
            const uint32_t len           = (p_evt->data_len <= remaining_buf) ? p_evt->data_len : remaining_buf;
            memcpy(&p_cb_info->p_buf[p_cb_info->offset], p_evt->data, len);
            p_cb_info->offset += len;
            p_cb_info->p_buf[p_cb_info->offset] = '\0';
        }
    }
}

static esp_err_t
http_post_event_handler(esp_http_client_event_t* p_evt)
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
            http_post_event_handler_on_header(p_evt);
            break;

        case HTTP_EVENT_ON_DATA:
            http_post_event_handler_on_data(p_evt);
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
    http_client_config_t* const               p_http_client_config,
    const ruuvi_gw_cfg_http_url_t* const      p_url,
    const ruuvi_gw_cfg_http_user_t* const     p_user,
    const ruuvi_gw_cfg_http_password_t* const p_password,
    const char* const                         p_server_cert,
    const char* const                         p_client_cert,
    const char* const                         p_client_key,
    void* const                               p_user_data)
{
    LOG_DBG("p_user_data=%p", p_user_data);
    p_http_client_config->http_url = *p_url;
    if (NULL != p_user)
    {
        p_http_client_config->http_user = *p_user;
    }
    else
    {
        p_http_client_config->http_user.buf[0] = '\0';
    }
    if (NULL != p_password)
    {
        p_http_client_config->http_pass = *p_password;
    }
    else
    {
        p_http_client_config->http_pass.buf[0] = '\0';
    }
    LOG_DBG("URL=%s", p_http_client_config->http_url.buf);
    LOG_DBG("user=%s", p_http_client_config->http_user.buf);
    LOG_DBG("pass=%s", p_http_client_config->http_pass.buf);

    p_http_client_config->esp_http_client_config = (esp_http_client_config_t) {
        .url       = &p_http_client_config->http_url.buf[0],
        .host      = NULL,
        .port      = 0,
        .username  = &p_http_client_config->http_user.buf[0],
        .password  = &p_http_client_config->http_pass.buf[0],
        .auth_type = ('\0' != p_http_client_config->http_user.buf[0]) ? HTTP_AUTH_TYPE_BASIC : HTTP_AUTH_TYPE_NONE,
        .path      = NULL,
        .query     = NULL,
        .cert_pem  = p_server_cert,
        .client_cert_pem             = p_client_cert,
        .client_key_pem              = p_client_key,
        .user_agent                  = NULL,
        .method                      = HTTP_METHOD_POST,
        .timeout_ms                  = 0,
        .disable_auto_redirect       = false,
        .max_redirection_count       = 0,
        .max_authorization_retries   = 0,
        .event_handler               = &http_post_event_handler,
        .transport_type              = HTTP_TRANSPORT_UNKNOWN,
        .buffer_size                 = 0,
        .buffer_size_tx              = 0,
        .user_data                   = p_user_data,
        .is_async                    = true,
        .use_global_ca_store         = false,
        .skip_cert_common_name_check = false,
        .keep_alive_enable           = false,
        .keep_alive_idle             = 0,
        .keep_alive_interval         = 0,
        .keep_alive_count            = 0,
    };
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

static void
escape_message_from_server(const char* const p_json, str_buf_t* const p_str_buf)
{
    for (const char* p_cur = p_json; '\0' != *p_cur; ++p_cur)
    {
        if ('"' == *p_cur)
        {
            str_buf_printf(p_str_buf, "\\%c", *p_cur);
        }
        else if ('\r' == *p_cur)
        {
            str_buf_printf(p_str_buf, "\\r");
        }
        else if ('\n' == *p_cur)
        {
            str_buf_printf(p_str_buf, "\\n");
        }
        else
        {
            str_buf_printf(p_str_buf, "%c", *p_cur);
        }
    }
}

static http_server_resp_t
http_wait_until_async_req_completed_handle_http_resp(
    esp_http_client_handle_t   p_http_handle,
    http_post_cb_info_t* const p_cb_info)
{
    const http_resp_code_e http_resp_code = esp_http_client_get_status_code(p_http_handle);
    LOG_DBG(
        "HTTP GET Status = %d, content_length = %d",
        http_resp_code,
        esp_http_client_get_content_length(p_http_handle));
    if (HTTP_RESP_CODE_200 == http_resp_code)
    {
        if ((NULL != p_cb_info) && (NULL != p_cb_info->p_buf))
        {
            os_free(p_cb_info->p_buf);
            p_cb_info->p_buf = NULL;
        }
        return http_server_resp_200_json("{}");
    }
    LOG_DBG("p_cb_info=%p", p_cb_info);
    if (NULL != p_cb_info)
    {
        LOG_DBG("p_cb_info->p_buf=%p", p_cb_info->p_buf);
    }
    if ((NULL == p_cb_info) || (NULL == p_cb_info->p_buf))
    {
        return http_server_resp_err(http_resp_code);
    }
    LOG_DBG("HTTP response: %.*s", p_cb_info->offset, p_cb_info->p_buf);

    str_buf_t str_buf = STR_BUF_INIT_NULL();
    escape_message_from_server(p_cb_info->p_buf, &str_buf);
    if (str_buf_init_with_alloc(&str_buf))
    {
        escape_message_from_server(p_cb_info->p_buf, &str_buf);
    }
    os_free(p_cb_info->p_buf);
    p_cb_info->p_buf = NULL;

    if (NULL == str_buf.buf)
    {
        LOG_ERR("Can't allocate memory for escapes message from the server");
        return http_server_resp_500();
    }
    const str_buf_t http_resp_buf = str_buf_printf_with_alloc(
        "HTTP response status: %u, Message from the server: %s",
        http_resp_code,
        str_buf.buf);
    str_buf_free_buf(&str_buf);

    if (NULL == http_resp_buf.buf)
    {
        LOG_ERR("Can't allocate memory for http_resp");
        return http_server_resp_500();
    }

    return http_server_resp_json_in_heap(http_resp_code, http_resp_buf.buf);
}

static http_server_resp_t
http_wait_until_async_req_completed(
    esp_http_client_handle_t   p_http_handle,
    http_post_cb_info_t* const p_cb_info,
    const bool                 flag_feed_task_watchdog,
    const TimeUnitsSeconds_t   timeout_seconds)
{
    const TickType_t tick_start = xTaskGetTickCount();
    while ((xTaskGetTickCount() - tick_start) < pdMS_TO_TICKS(timeout_seconds * TIME_UNITS_MS_PER_SECOND))
    {
        LOG_DBG("esp_http_client_perform");
        esp_err_t err = esp_http_client_perform(p_http_handle);
        if (ESP_OK == err)
        {
            return http_wait_until_async_req_completed_handle_http_resp(p_http_handle, p_cb_info);
        }
        if (ESP_ERR_HTTP_EAGAIN != err)
        {
            LOG_ERR_ESP(err, "%s failed", "esp_http_client_perform");
            if ((NULL != p_cb_info) && (NULL != p_cb_info->p_buf))
            {
                os_free(p_cb_info->p_buf);
                p_cb_info->p_buf = NULL;
            }
            if (ESP_ERR_ESP_TLS_CANNOT_RESOLVE_HOSTNAME == err)
            {
                LOG_ERR("%s failed, err=%d (%s)", "esp_http_client_perform", err, "CANNOT_RESOLVE_HOSTNAME");
                const str_buf_t str_buf = str_buf_printf_with_alloc("Failed to resolve hostname");
                return http_server_resp_502_json_in_heap(str_buf.buf);
            }
            LOG_ERR_ESP(err, "%s failed", "esp_http_client_perform");
            str_buf_t       err_desc = esp_err_to_name_with_alloc_str_buf(err);
            const str_buf_t str_buf  = str_buf_printf_with_alloc(
                "Network error when communicating with the server, err=%d, description=%s",
                err,
                (NULL != err_desc.buf) ? err_desc.buf : "");
            str_buf_free_buf(&err_desc);
            return http_server_resp_502_json_in_heap(str_buf.buf);
        }
        LOG_DBG("esp_http_client_perform: ESP_ERR_HTTP_EAGAIN");
        vTaskDelay(pdMS_TO_TICKS(50));
        http_download_feed_task_watchdog_if_needed(flag_feed_task_watchdog);
    }
    LOG_ERR("timeout (%u seconds)", (printf_uint_t)timeout_seconds);
    if ((NULL != p_cb_info) && (NULL != p_cb_info->p_buf))
    {
        os_free(p_cb_info->p_buf);
        p_cb_info->p_buf = NULL;
    }
    const str_buf_t str_buf = str_buf_printf_with_alloc("Timeout waiting for a response from the server");
    return http_server_resp_502_json_in_heap(str_buf.buf);
}

static bool
cb_on_post_get_chunk(void* p_user_data, void** p_p_buf, size_t* p_len)
{
    json_stream_gen_t* p_gen   = p_user_data;
    const char* const  p_chunk = json_stream_gen_get_next_chunk(p_gen);
    if (NULL == p_chunk)
    {
        return false;
    }
    *p_p_buf = (void*)p_chunk;
    *p_len   = strlen(p_chunk);
    if (0 != *p_len)
    {
        LOG_INFO("HTTP POST DATA:\n%s", p_chunk);
    }
    return true;
}

static bool
http_send_async(http_async_info_t* const p_http_async_info)
{
    const esp_http_client_config_t* const p_http_config = &p_http_async_info->http_client_config.esp_http_client_config;

    LOG_INFO("### HTTP POST to URL=%s", p_http_config->url);

    if (p_http_async_info->use_json_stream_gen)
    {
        const json_stream_gen_size_t json_len = json_stream_gen_calc_size(p_http_async_info->select.p_gen);
        const esp_err_t              err      = esp_http_client_set_cb_on_post_get_chunk(
            p_http_async_info->p_http_client_handle,
            (int)json_len,
            &cb_on_post_get_chunk,
            (void*)p_http_async_info->select.p_gen);
        if (0 != err)
        {
            LOG_ERR_ESP(err, "%s failed", "esp_http_client_set_cb_on_post_get_chunk");
            return false;
        }
    }
    else
    {
        const char* const p_msg = p_http_async_info->select.cjson_str.p_str;
        LOG_INFO("HTTP POST DATA:\n%s", p_msg);
        const esp_err_t err = esp_http_client_set_post_field(
            p_http_async_info->p_http_client_handle,
            p_msg,
            (esp_http_client_len_t)strlen(p_msg));
        if (0 != err)
        {
            LOG_ERR_ESP(err, "%s failed", "esp_http_client_set_post_field");
            return false;
        }
    }

    esp_http_client_set_header(p_http_async_info->p_http_client_handle, "Content-Type", "application/json");

    str_buf_t hmac_sha256_str = hmac_sha256_to_str_buf(&p_http_async_info->hmac_sha256);
    if (hmac_sha256_is_str_valid(&hmac_sha256_str))
    {
        esp_http_client_set_header(p_http_async_info->p_http_client_handle, "Ruuvi-HMAC-SHA256", hmac_sha256_str.buf);
    }
    str_buf_free_buf(&hmac_sha256_str);

    LOG_DBG("esp_http_client_perform");
    const esp_err_t err = esp_http_client_perform(p_http_async_info->p_http_client_handle);
    if (ESP_ERR_HTTP_EAGAIN != err)
    {
        LOG_ERR_ESP(err, "### HTTP POST to URL=%s: request failed", p_http_config->url);
        LOG_DBG("esp_http_client_cleanup");
        if (NULL != p_http_async_info->http_client_config.esp_http_client_config.cert_pem)
        {
            os_free(p_http_async_info->http_client_config.esp_http_client_config.cert_pem);
        }
        if (NULL != p_http_async_info->http_client_config.esp_http_client_config.client_cert_pem)
        {
            os_free(p_http_async_info->http_client_config.esp_http_client_config.client_cert_pem);
        }
        if (NULL != p_http_async_info->http_client_config.esp_http_client_config.client_key_pem)
        {
            os_free(p_http_async_info->http_client_config.esp_http_client_config.client_key_pem);
        }
        esp_http_client_cleanup(p_http_async_info->p_http_client_handle);
        p_http_async_info->p_http_client_handle = NULL;
        return false;
    }
    return true;
}

static void
http_async_info_free_data(http_async_info_t* const p_http_async_info)
{
    if (p_http_async_info->use_json_stream_gen)
    {
        if (NULL != p_http_async_info->select.p_gen)
        {
            json_stream_gen_delete(&p_http_async_info->select.p_gen);
        }
    }
    else
    {
        cjson_wrap_free_json_str(&p_http_async_info->select.cjson_str);
        p_http_async_info->select.cjson_str = cjson_wrap_str_null();
    }
}

bool
http_send_advs_internal(
    http_async_info_t* const         p_http_async_info,
    const adv_report_table_t* const  p_reports,
    const uint32_t                   nonce,
    const bool                       flag_use_timestamps,
    const bool                       flag_post_to_ruuvi,
    const ruuvi_gw_cfg_http_t* const p_cfg_http,
    void* const                      p_user_data,
    const bool                       use_ssl_client_cert,
    const bool                       use_ssl_server_cert)
{
    p_http_async_info->recipient = flag_post_to_ruuvi ? HTTP_POST_RECIPIENT_ADVS1 : HTTP_POST_RECIPIENT_ADVS2;

    const bool flag_decode    = false;
    const bool flag_use_nonce = true;

    p_http_async_info->use_json_stream_gen = true;
    const gw_cfg_t* p_gw_cfg               = gw_cfg_lock_ro();
    p_http_async_info->select.p_gen        = http_json_create_stream_gen_advs(
        p_reports,
        flag_decode,
        flag_use_timestamps,
        time(NULL),
        flag_use_nonce,
        nonce,
        gw_cfg_get_nrf52_mac_addr(),
        &p_gw_cfg->ruuvi_cfg.coordinates);
    gw_cfg_unlock_ro(&p_gw_cfg);
    if (NULL == p_http_async_info->select.p_gen)
    {
        LOG_ERR("Not enough memory to create http_json_create_stream_gen_advs");
        return false;
    }

#if LOG_LOCAL_LEVEL >= LOG_LEVEL_DEBUG
    switch (p_cfg_http->auth_type)
    {
        case GW_CFG_HTTP_AUTH_TYPE_NONE:
            LOG_DBG("http_init_client_config: URL=%s, auth_type=%s", p_cfg_http->http_url.buf, "None");
            break;
        case GW_CFG_HTTP_AUTH_TYPE_BASIC:
            LOG_DBG("http_init_client_config: URL=%s, auth_type=%s", p_cfg_http->http_url.buf, "Basic");
            LOG_DBG(
                "http_init_client_config: user=%s, pass=%s",
                p_cfg_http->auth.auth_basic.user.buf,
                p_cfg_http->auth.auth_basic.password.buf);
            break;
        case GW_CFG_HTTP_AUTH_TYPE_BEARER:
            LOG_DBG("http_init_client_config: URL=%s, auth_type=%s", p_cfg_http->http_url.buf, "Bearer");
            LOG_DBG("http_init_client_config: Bearer token: %s", p_cfg_http->auth.auth_bearer.token.buf);
            break;
        case GW_CFG_HTTP_AUTH_TYPE_TOKEN:
            LOG_DBG("http_init_client_config: URL=%s, auth_type=%s", p_cfg_http->http_url.buf, "Token");
            LOG_DBG("http_init_client_config: Token: %s", p_cfg_http->auth.auth_token.token.buf);
            break;
    }
#endif

    const ruuvi_gw_cfg_http_user_t*     p_http_user = NULL;
    const ruuvi_gw_cfg_http_password_t* p_http_pass = NULL;
    if (!flag_post_to_ruuvi && (GW_CFG_HTTP_AUTH_TYPE_BASIC == p_cfg_http->auth_type))
    {
        p_http_user = &p_cfg_http->auth.auth_basic.user;
        p_http_pass = &p_cfg_http->auth.auth_basic.password;
    }

    ruuvi_gw_cfg_http_url_t* p_http_url = os_malloc(sizeof(*p_http_url));
    if (NULL == p_http_url)
    {
        LOG_ERR("Can't allocate memory");
        http_async_info_free_data(p_http_async_info);
        return false;
    }
    (void)snprintf(
        p_http_url->buf,
        sizeof(p_http_url->buf),
        "%s",
        flag_post_to_ruuvi ? RUUVI_GATEWAY_HTTP_DEFAULT_URL : p_cfg_http->http_url.buf);

    str_buf_t str_buf_server_cert_http = str_buf_init_null();
    str_buf_t str_buf_client_cert      = str_buf_init_null();
    str_buf_t str_buf_client_key       = str_buf_init_null();
    if (use_ssl_client_cert)
    {
        str_buf_client_cert = gw_cfg_storage_read_file(GW_CFG_STORAGE_SSL_HTTP_CLI_CERT);
        str_buf_client_key  = gw_cfg_storage_read_file(GW_CFG_STORAGE_SSL_HTTP_CLI_KEY);
    }
    if (use_ssl_server_cert)
    {
        str_buf_server_cert_http = gw_cfg_storage_read_file(GW_CFG_STORAGE_SSL_HTTP_SRV_CERT);
    }

    http_init_client_config(
        &p_http_async_info->http_client_config,
        p_http_url,
        p_http_user,
        p_http_pass,
        str_buf_server_cert_http.buf,
        str_buf_client_cert.buf,
        str_buf_client_key.buf,
        p_user_data);

    os_free(p_http_url);

    p_http_async_info->p_http_client_handle = esp_http_client_init(
        &p_http_async_info->http_client_config.esp_http_client_config);
    if (NULL == p_http_async_info->p_http_client_handle)
    {
        LOG_ERR("HTTP POST to URL=%s: Can't init http client", p_http_async_info->http_client_config.http_url.buf);
        http_async_info_free_data(p_http_async_info);
        return false;
    }

    if (!flag_post_to_ruuvi && (GW_CFG_HTTP_AUTH_TYPE_BEARER == p_cfg_http->auth_type))
    {
        str_buf_t str_buf = str_buf_printf_with_alloc("Bearer %s", p_cfg_http->auth.auth_bearer.token.buf);
        if (NULL == str_buf.buf)
        {
            LOG_ERR("Can't allocate memory for bearer token");
            http_async_info_free_data(p_http_async_info);
            return false;
        }
        esp_http_client_set_header(p_http_async_info->p_http_client_handle, "Authorization", str_buf.buf);
        str_buf_free_buf(&str_buf);
    }
    else if (!flag_post_to_ruuvi && (GW_CFG_HTTP_AUTH_TYPE_TOKEN == p_cfg_http->auth_type))
    {
        str_buf_t str_buf = str_buf_printf_with_alloc("Token %s", p_cfg_http->auth.auth_token.token.buf);
        if (NULL == str_buf.buf)
        {
            LOG_ERR("Can't allocate memory for bearer token");
            http_async_info_free_data(p_http_async_info);
            return false;
        }
        esp_http_client_set_header(p_http_async_info->p_http_client_handle, "Authorization", str_buf.buf);
        str_buf_free_buf(&str_buf);
    }
    if (flag_post_to_ruuvi)
    {
        (void)hmac_sha256_calc_for_json_gen_http_ruuvi(
            p_http_async_info->select.p_gen,
            &p_http_async_info->hmac_sha256);
    }
    else
    {
        (void)hmac_sha256_calc_for_json_gen_http_custom(
            p_http_async_info->select.p_gen,
            &p_http_async_info->hmac_sha256);
    }

    if (!http_send_async(p_http_async_info))
    {
        LOG_DBG("esp_http_client_cleanup");
        if (NULL != p_http_async_info->http_client_config.esp_http_client_config.cert_pem)
        {
            os_free(p_http_async_info->http_client_config.esp_http_client_config.cert_pem);
        }
        if (NULL != p_http_async_info->http_client_config.esp_http_client_config.client_cert_pem)
        {
            os_free(p_http_async_info->http_client_config.esp_http_client_config.client_cert_pem);
        }
        if (NULL != p_http_async_info->http_client_config.esp_http_client_config.client_key_pem)
        {
            os_free(p_http_async_info->http_client_config.esp_http_client_config.client_key_pem);
        }
        esp_http_client_cleanup(p_http_async_info->p_http_client_handle);
        p_http_async_info->p_http_client_handle = NULL;
        http_async_info_free_data(p_http_async_info);
        return false;
    }
    return true;
}

bool
http_send_advs(
    const adv_report_table_t* const  p_reports,
    const uint32_t                   nonce,
    const bool                       flag_use_timestamps,
    const bool                       flag_post_to_ruuvi,
    const ruuvi_gw_cfg_http_t* const p_cfg_http,
    void* const                      p_user_data)
{
    http_async_info_t* p_http_async_info = get_http_async_info();
    LOG_DBG("os_sema_wait_immediate: p_http_async_sema");
    if (!os_sema_wait_immediate(p_http_async_info->p_http_async_sema))
    {
        /* Because the amount of memory is limited and may not be sufficient for two simultaneous queries,
         * this function should only be called from a single thread. */
        LOG_ERR("This function is called twice from two different threads, which can lead to memory shortages");
        assert(0);
    }
    p_http_async_info->p_task = xTaskGetCurrentTaskHandle();

    const bool use_ssl_client_cert = (!flag_post_to_ruuvi) && p_cfg_http->http_use_ssl_client_cert;
    const bool use_ssl_server_cert = (!flag_post_to_ruuvi) && p_cfg_http->http_use_ssl_server_cert;
    if (!http_send_advs_internal(
            p_http_async_info,
            p_reports,
            nonce,
            flag_use_timestamps,
            flag_post_to_ruuvi,
            p_cfg_http,
            p_user_data,
            use_ssl_client_cert,
            use_ssl_server_cert))
    {
        LOG_DBG("os_sema_signal: p_http_async_sema");
        os_sema_signal(p_http_async_info->p_http_async_sema);
        return false;
    }
    return true;
}

static http_server_resp_t
http_check_post_advs_internal3(
    http_async_info_t* const      p_http_async_info,
    ruuvi_gw_cfg_http_t* const    p_cfg_http,
    const char* const             p_url,
    const gw_cfg_http_auth_type_e auth_type,
    const char* const             p_user,
    const char* const             p_pass,
    const TimeUnitsSeconds_t      timeout_seconds,
    const bool                    use_ssl_client_cert,
    const bool                    use_ssl_server_cert)
{
    if (strlen(p_url) >= sizeof(p_cfg_http->http_url.buf))
    {
        return http_server_resp_err(HTTP_RESP_CODE_400);
    }
    switch (auth_type)
    {
        case GW_CFG_HTTP_AUTH_TYPE_NONE:
            break;
        case GW_CFG_HTTP_AUTH_TYPE_BASIC:
            if ((strlen(p_user) >= sizeof(p_cfg_http->auth.auth_basic.user.buf))
                || (strlen(p_pass) >= sizeof(p_cfg_http->auth.auth_basic.password.buf)))
            {
                return http_server_resp_err(HTTP_RESP_CODE_400);
            }
            (void)snprintf(
                p_cfg_http->auth.auth_basic.user.buf,
                sizeof(p_cfg_http->auth.auth_basic.user.buf),
                "%s",
                p_user);
            (void)snprintf(
                p_cfg_http->auth.auth_basic.password.buf,
                sizeof(p_cfg_http->auth.auth_basic.password.buf),
                "%s",
                p_pass);

            break;
        case GW_CFG_HTTP_AUTH_TYPE_BEARER:
            if ((strlen(p_pass) >= sizeof(p_cfg_http->auth.auth_bearer.token.buf)))
            {
                return http_server_resp_err(HTTP_RESP_CODE_400);
            }
            (void)snprintf(
                p_cfg_http->auth.auth_bearer.token.buf,
                sizeof(p_cfg_http->auth.auth_bearer.token.buf),
                "%s",
                p_pass);
            break;
        case GW_CFG_HTTP_AUTH_TYPE_TOKEN:
            if ((strlen(p_pass) >= sizeof(p_cfg_http->auth.auth_token.token.buf)))
            {
                return http_server_resp_err(HTTP_RESP_CODE_400);
            }
            (void)snprintf(
                p_cfg_http->auth.auth_token.token.buf,
                sizeof(p_cfg_http->auth.auth_token.token.buf),
                "%s",
                p_pass);
            break;
    }

    (void)snprintf(p_cfg_http->http_url.buf, sizeof(p_cfg_http->http_url), "%s", p_url);
    p_cfg_http->auth_type = auth_type;

    if (!http_send_advs_internal(
            p_http_async_info,
            NULL,
            esp_random(),
            gw_cfg_get_ntp_use(),
            false,
            p_cfg_http,
            &p_http_async_info->http_post_cb_info,
            use_ssl_client_cert,
            use_ssl_server_cert))
    {
        LOG_ERR("http_send_advs failed");
        return http_server_resp_500();
    }

    const bool         flag_feed_task_watchdog = true;
    http_server_resp_t server_resp             = http_wait_until_async_req_completed(
        p_http_async_info->p_http_client_handle,
        &p_http_async_info->http_post_cb_info,
        flag_feed_task_watchdog,
        timeout_seconds);

    http_resp_code_e http_resp_code = server_resp.http_resp_code;
    if (HTTP_RESP_CODE_429 == http_resp_code)
    {
        // Return OK if we got error "Too Many Requests"
        http_resp_code = HTTP_RESP_CODE_200;
    }

    const bool flag_is_in_memory = (HTTP_CONTENT_LOCATION_FLASH_MEM == server_resp.content_location)
                                   || (HTTP_CONTENT_LOCATION_STATIC_MEM == server_resp.content_location)
                                   || (HTTP_CONTENT_LOCATION_HEAP == server_resp.content_location);
    const char* const p_json = (flag_is_in_memory && (NULL != server_resp.select_location.memory.p_buf))
                                   ? (const char*)server_resp.select_location.memory.p_buf
                                   : NULL;

    const http_server_resp_t resp = http_server_cb_gen_resp(http_resp_code, "%s", (NULL != p_json) ? p_json : "");

    if ((HTTP_CONTENT_LOCATION_HEAP == server_resp.content_location)
        && (NULL != server_resp.select_location.memory.p_buf))
    {
        os_free(server_resp.select_location.memory.p_buf);
    }

    LOG_DBG("esp_http_client_cleanup");
    if (NULL != p_http_async_info->http_client_config.esp_http_client_config.cert_pem)
    {
        os_free(p_http_async_info->http_client_config.esp_http_client_config.cert_pem);
    }
    if (NULL != p_http_async_info->http_client_config.esp_http_client_config.client_cert_pem)
    {
        os_free(p_http_async_info->http_client_config.esp_http_client_config.client_cert_pem);
    }
    if (NULL != p_http_async_info->http_client_config.esp_http_client_config.client_key_pem)
    {
        os_free(p_http_async_info->http_client_config.esp_http_client_config.client_key_pem);
    }
    esp_http_client_cleanup(p_http_async_info->p_http_client_handle);
    p_http_async_info->p_http_client_handle = NULL;
    http_async_info_free_data(p_http_async_info);

    return resp;
}

static http_server_resp_t
http_check_post_advs_internal2(
    http_async_info_t* const      p_http_async_info,
    const char* const             p_url,
    const gw_cfg_http_auth_type_e auth_type,
    const char* const             p_user,
    const char* const             p_pass,
    const TimeUnitsSeconds_t      timeout_seconds,
    const bool                    use_ssl_client_cert,
    const bool                    use_ssl_server_cert)
{
    ruuvi_gw_cfg_http_t* p_cfg_http = os_malloc(sizeof(*p_cfg_http));
    if (NULL == p_cfg_http)
    {
        LOG_ERR("Can't allocate memory for ruuvi_gw_cfg_http_t");
        return http_server_resp_500();
    }

    const http_server_resp_t resp = http_check_post_advs_internal3(
        p_http_async_info,
        p_cfg_http,
        p_url,
        auth_type,
        p_user,
        p_pass,
        timeout_seconds,
        use_ssl_client_cert,
        use_ssl_server_cert);

    os_free(p_cfg_http);

    return resp;
}

http_server_resp_t
http_check_post_advs(
    const char* const             p_url,
    const gw_cfg_http_auth_type_e auth_type,
    const char* const             p_user,
    const char* const             p_pass,
    const TimeUnitsSeconds_t      timeout_seconds,
    const bool                    use_ssl_client_cert,
    const bool                    use_ssl_server_cert)
{
    http_async_info_t* const p_http_async_info = get_http_async_info();
    LOG_DBG("os_sema_wait_immediate: p_http_async_sema");
    if (!os_sema_wait_immediate(p_http_async_info->p_http_async_sema))
    {
        /* Because the amount of memory is limited and may not be sufficient for two simultaneous queries,
         * this function should only be called from a single thread. */
        LOG_ERR("This function is called twice from two different threads, which can lead to memory shortages");
        assert(0);
    }
    p_http_async_info->p_task = xTaskGetCurrentTaskHandle();

    const http_server_resp_t resp = http_check_post_advs_internal2(
        p_http_async_info,
        p_url,
        auth_type,
        p_user,
        p_pass,
        timeout_seconds,
        use_ssl_client_cert,
        use_ssl_server_cert);

    LOG_DBG("os_sema_signal: p_http_async_sema");
    os_sema_signal(p_http_async_info->p_http_async_sema);

    return resp;
}

static bool
http_send_statistics_internal(
    http_async_info_t* const                 p_http_async_info,
    const http_json_statistics_info_t* const p_stat_info,
    const adv_report_table_t* const          p_reports,
    const ruuvi_gw_cfg_http_stat_t* const    p_cfg_http_stat,
    void* const                              p_user_data,
    const bool                               use_ssl_client_cert,
    const bool                               use_ssl_server_cert)
{
    p_http_async_info->recipient           = HTTP_POST_RECIPIENT_STATS;
    p_http_async_info->use_json_stream_gen = false;
    p_http_async_info->select.cjson_str    = cjson_wrap_str_null();
    if (!http_json_create_status_str(p_stat_info, p_reports, &p_http_async_info->select.cjson_str))
    {
        LOG_ERR("Not enough memory to generate status json");
        return false;
    }

    p_http_async_info->http_client_config.http_url  = p_cfg_http_stat->http_stat_url;
    p_http_async_info->http_client_config.http_user = p_cfg_http_stat->http_stat_user;
    p_http_async_info->http_client_config.http_pass = p_cfg_http_stat->http_stat_pass;

    str_buf_t str_buf_server_cert_stat = str_buf_init_null();
    str_buf_t str_buf_client_cert      = str_buf_init_null();
    str_buf_t str_buf_client_key       = str_buf_init_null();
    if (use_ssl_client_cert)
    {
        str_buf_client_cert = gw_cfg_storage_read_file(GW_CFG_STORAGE_SSL_STAT_CLI_CERT);
        str_buf_client_key  = gw_cfg_storage_read_file(GW_CFG_STORAGE_SSL_STAT_CLI_KEY);
    }
    if (use_ssl_server_cert)
    {
        str_buf_server_cert_stat = gw_cfg_storage_read_file(GW_CFG_STORAGE_SSL_STAT_SRV_CERT);
    }

    http_init_client_config(
        &p_http_async_info->http_client_config,
        &p_cfg_http_stat->http_stat_url,
        &p_cfg_http_stat->http_stat_user,
        &p_cfg_http_stat->http_stat_pass,
        str_buf_server_cert_stat.buf,
        str_buf_client_cert.buf,
        str_buf_client_key.buf,
        p_user_data);

    p_http_async_info->p_http_client_handle = esp_http_client_init(
        &p_http_async_info->http_client_config.esp_http_client_config);
    if (NULL == p_http_async_info->p_http_client_handle)
    {
        LOG_ERR("HTTP POST to URL=%s: Can't init http client", p_http_async_info->http_client_config.http_url.buf);
        cjson_wrap_free_json_str(&p_http_async_info->select.cjson_str);
        return false;
    }

    (void)hmac_sha256_calc_for_stats(p_http_async_info->select.cjson_str.p_str, &p_http_async_info->hmac_sha256);

    if (!http_send_async(p_http_async_info))
    {
        LOG_DBG("esp_http_client_cleanup");
        if (NULL != p_http_async_info->http_client_config.esp_http_client_config.cert_pem)
        {
            os_free(p_http_async_info->http_client_config.esp_http_client_config.cert_pem);
        }
        if (NULL != p_http_async_info->http_client_config.esp_http_client_config.client_cert_pem)
        {
            os_free(p_http_async_info->http_client_config.esp_http_client_config.client_cert_pem);
        }
        if (NULL != p_http_async_info->http_client_config.esp_http_client_config.client_key_pem)
        {
            os_free(p_http_async_info->http_client_config.esp_http_client_config.client_key_pem);
        }
        esp_http_client_cleanup(p_http_async_info->p_http_client_handle);
        p_http_async_info->p_http_client_handle = NULL;
        cjson_wrap_free_json_str(&p_http_async_info->select.cjson_str);
        return false;
    }
    return true;
}

bool
http_send_statistics(
    const http_json_statistics_info_t* const p_stat_info,
    const adv_report_table_t* const          p_reports,
    const ruuvi_gw_cfg_http_stat_t* const    p_cfg_http_stat,
    void* const                              p_user_data,
    const bool                               use_ssl_client_cert,
    const bool                               use_ssl_server_cert)
{
    http_async_info_t* p_http_async_info = get_http_async_info();
    LOG_DBG("os_sema_wait_immediate: p_http_async_sema");
    if (!os_sema_wait_immediate(p_http_async_info->p_http_async_sema))
    {
        /* Because the amount of memory is limited and may not be sufficient for two simultaneous queries,
         * this function should only be called from a single thread. */
        LOG_ERR("This function is called twice from two different threads, which can lead to memory shortages");
        assert(0);
    }
    p_http_async_info->p_task = xTaskGetCurrentTaskHandle();

    if (!http_send_statistics_internal(
            p_http_async_info,
            p_stat_info,
            p_reports,
            p_cfg_http_stat,
            p_user_data,
            use_ssl_client_cert,
            use_ssl_server_cert))
    {
        LOG_DBG("os_sema_signal: p_http_async_sema");
        os_sema_signal(p_http_async_info->p_http_async_sema);
        return false;
    }
    return true;
}

static http_server_resp_t
http_check_post_stat_internal3(
    http_async_info_t* const        p_http_async_info,
    ruuvi_gw_cfg_http_stat_t* const p_cfg_http_stat,
    const char* const               p_url,
    const char* const               p_user,
    const char* const               p_pass,
    const TimeUnitsSeconds_t        timeout_seconds,
    const bool                      use_ssl_client_cert,
    const bool                      use_ssl_server_cert)
{

    if ((strlen(p_url) >= sizeof(p_cfg_http_stat->http_stat_url.buf))
        || (strlen(p_user) >= sizeof(p_cfg_http_stat->http_stat_user.buf))
        || (strlen(p_pass) >= sizeof(p_cfg_http_stat->http_stat_pass.buf)))
    {
        return http_server_resp_err(HTTP_RESP_CODE_400);
    }
    (void)snprintf(p_cfg_http_stat->http_stat_url.buf, sizeof(p_cfg_http_stat->http_stat_url), "%s", p_url);
    (void)snprintf(p_cfg_http_stat->http_stat_user.buf, sizeof(p_cfg_http_stat->http_stat_user), "%s", p_user);
    (void)snprintf(p_cfg_http_stat->http_stat_pass.buf, sizeof(p_cfg_http_stat->http_stat_pass), "%s", p_pass);

    const http_json_statistics_info_t* p_stat_info = adv_post_generate_statistics_info(NULL);
    if (NULL == p_stat_info)
    {
        LOG_ERR("Can't allocate memory");
        return http_server_resp_500();
    }

    if (!http_send_statistics_internal(
            p_http_async_info,
            p_stat_info,
            NULL,
            p_cfg_http_stat,
            &p_http_async_info->http_post_cb_info,
            use_ssl_client_cert,
            use_ssl_server_cert))
    {
        os_free(p_stat_info);
        LOG_ERR("http_send_statistics failed");
        return http_server_resp_500();
    }
    os_free(p_stat_info);

    const bool         flag_feed_task_watchdog = true;
    http_server_resp_t server_resp             = http_wait_until_async_req_completed(
        p_http_async_info->p_http_client_handle,
        &p_http_async_info->http_post_cb_info,
        flag_feed_task_watchdog,
        timeout_seconds);

    http_resp_code_e http_resp_code = server_resp.http_resp_code;
    if (HTTP_RESP_CODE_429 == http_resp_code)
    {
        // Return OK if we got error "Too Many Requests"
        http_resp_code = HTTP_RESP_CODE_200;
    }

    const bool flag_is_in_memory = (HTTP_CONTENT_LOCATION_FLASH_MEM == server_resp.content_location)
                                   || (HTTP_CONTENT_LOCATION_STATIC_MEM == server_resp.content_location)
                                   || (HTTP_CONTENT_LOCATION_HEAP == server_resp.content_location);
    const char* const p_json = (flag_is_in_memory && (NULL != server_resp.select_location.memory.p_buf))
                                   ? (const char*)server_resp.select_location.memory.p_buf
                                   : NULL;

    const http_server_resp_t resp = http_server_cb_gen_resp(http_resp_code, "%s", (NULL != p_json) ? p_json : "");

    if ((HTTP_CONTENT_LOCATION_HEAP == server_resp.content_location)
        && (NULL != server_resp.select_location.memory.p_buf))
    {
        os_free(server_resp.select_location.memory.p_buf);
    }

    LOG_DBG("esp_http_client_cleanup");
    if (NULL != p_http_async_info->http_client_config.esp_http_client_config.cert_pem)
    {
        os_free(p_http_async_info->http_client_config.esp_http_client_config.cert_pem);
    }
    if (NULL != p_http_async_info->http_client_config.esp_http_client_config.client_cert_pem)
    {
        os_free(p_http_async_info->http_client_config.esp_http_client_config.client_cert_pem);
    }
    if (NULL != p_http_async_info->http_client_config.esp_http_client_config.client_key_pem)
    {
        os_free(p_http_async_info->http_client_config.esp_http_client_config.client_key_pem);
    }
    esp_http_client_cleanup(p_http_async_info->p_http_client_handle);
    p_http_async_info->p_http_client_handle = NULL;
    cjson_wrap_free_json_str(&p_http_async_info->select.cjson_str);

    return resp;
}

static http_server_resp_t
http_check_post_stat_internal2(
    http_async_info_t* const p_http_async_info,
    const char* const        p_url,
    const char* const        p_user,
    const char* const        p_pass,
    const TimeUnitsSeconds_t timeout_seconds,
    const bool               use_ssl_client_cert,
    const bool               use_ssl_server_cert)
{
    ruuvi_gw_cfg_http_stat_t* p_cfg_http_stat = os_malloc(sizeof(*p_cfg_http_stat));
    if (NULL == p_cfg_http_stat)
    {
        LOG_ERR("Can't allocate memory for ruuvi_gw_cfg_http_t");
        return http_server_resp_500();
    }

    const http_server_resp_t resp = http_check_post_stat_internal3(
        p_http_async_info,
        p_cfg_http_stat,
        p_url,
        p_user,
        p_pass,
        timeout_seconds,
        use_ssl_client_cert,
        use_ssl_server_cert);

    os_free(p_cfg_http_stat);

    return resp;
}

http_server_resp_t
http_check_post_stat(
    const char* const        p_url,
    const char* const        p_user,
    const char* const        p_pass,
    const TimeUnitsSeconds_t timeout_seconds,
    const bool               use_ssl_client_cert,
    const bool               use_ssl_server_cert)
{
    http_async_info_t* const p_http_async_info = get_http_async_info();
    LOG_DBG("os_sema_wait_immediate: p_http_async_sema");
    if (!os_sema_wait_immediate(p_http_async_info->p_http_async_sema))
    {
        /* Because the amount of memory is limited and may not be sufficient for two simultaneous queries,
         * this function should only be called from a single thread. */
        LOG_ERR("This function is called twice from two different threads, which can lead to memory shortages");
        assert(0);
    }
    p_http_async_info->p_task = xTaskGetCurrentTaskHandle();

    const http_server_resp_t resp = http_check_post_stat_internal2(
        p_http_async_info,
        p_url,
        p_user,
        p_pass,
        timeout_seconds,
        use_ssl_client_cert,
        use_ssl_server_cert);

    LOG_DBG("os_sema_signal: p_http_async_sema");
    os_sema_signal(p_http_async_info->p_http_async_sema);

    return resp;
}

static http_server_resp_t
http_check_mqtt_internal(const ruuvi_gw_cfg_mqtt_t* const p_mqtt_cfg, const TimeUnitsSeconds_t timeout_seconds)
{
    LOG_DBG("mqtt_app_start");
    mqtt_app_start(p_mqtt_cfg);

    const TickType_t tick_start = xTaskGetTickCount();
    LOG_DBG("Wait until MQTT connected for %u seconds", (printf_int_t)timeout_seconds);
    while (!gw_status_is_mqtt_connected_or_error())
    {
        if ((xTaskGetTickCount() - tick_start) >= pdMS_TO_TICKS(timeout_seconds * TIME_UNITS_MS_PER_SECOND))
        {
            LOG_ERR("Timeout waiting until MQTT connected");
            break;
        }
        esp_task_wdt_reset();
        vTaskDelay(pdMS_TO_TICKS(50));
    }

    const bool         flag_is_mqtt_connected = gw_status_is_mqtt_connected();
    const mqtt_error_e mqtt_error             = gw_status_get_mqtt_error();
    str_buf_t          mqtt_err_msg           = mqtt_app_get_error_message();

    LOG_DBG("mqtt_app_stop");
    LOG_DBG("mqtt_err_msg: %s", mqtt_err_msg.buf ? mqtt_err_msg.buf : ":NULL:");
    mqtt_app_stop();

    http_resp_code_e http_resp_code = HTTP_RESP_CODE_200;
    str_buf_t        err_desc       = STR_BUF_INIT_NULL();
    if (!flag_is_mqtt_connected)
    {
        switch (mqtt_error)
        {
            case MQTT_ERROR_NONE:
                http_resp_code = HTTP_RESP_CODE_502; // Not Found
                err_desc       = str_buf_printf_with_alloc("Timeout waiting for a response from the server");
                break;
            case MQTT_ERROR_DNS:
                http_resp_code = HTTP_RESP_CODE_502; // Not Found
                err_desc       = str_buf_printf_with_alloc("Failed to resolve hostname");
                break;
            case MQTT_ERROR_AUTH:
                http_resp_code = HTTP_RESP_CODE_401; // Unauthorized
                err_desc       = str_buf_printf_with_alloc(
                    "Access with the specified credentials is prohibited (%s)",
                    (NULL != mqtt_err_msg.buf) ? mqtt_err_msg.buf : "");
                break;
            case MQTT_ERROR_CONNECT:
                http_resp_code = HTTP_RESP_CODE_502; // Not Found
                err_desc       = str_buf_printf_with_alloc(
                    "Failed to connect (%s)",
                    (NULL != mqtt_err_msg.buf) ? mqtt_err_msg.buf : "");
                break;
        }
    }
    str_buf_free_buf(&mqtt_err_msg);

    const http_server_resp_t resp = http_server_cb_gen_resp(
        http_resp_code,
        "%s",
        (NULL != err_desc.buf) ? err_desc.buf : "");
    str_buf_free_buf(&err_desc);
    return resp;
}

http_server_resp_t
http_check_mqtt(const ruuvi_gw_cfg_mqtt_t* const p_mqtt_cfg, const TimeUnitsSeconds_t timeout_seconds)
{
    http_async_info_t* const p_http_async_info = get_http_async_info();
    LOG_DBG("os_sema_wait_immediate: p_http_async_sema");
    if (!os_sema_wait_immediate(p_http_async_info->p_http_async_sema))
    {
        /* Because the amount of memory is limited and may not be sufficient for two simultaneous queries,
         * this function should only be called from a single thread. */
        LOG_ERR("This function is called twice from two different threads, which can lead to memory shortages");
        assert(0);
    }
    p_http_async_info->p_task = xTaskGetCurrentTaskHandle();

    const http_server_resp_t resp = http_check_mqtt_internal(p_mqtt_cfg, timeout_seconds);

    LOG_DBG("os_sema_signal: p_http_async_sema");
    os_sema_signal(p_http_async_info->p_http_async_sema);

    return resp;
}

bool
http_async_poll(void)
{
    http_async_info_t* p_http_async_info = get_http_async_info();

    if (p_http_async_info->p_task != os_task_get_cur_task_handle())
    {
        LOG_ERR(
            "%s is called from thread %s, but it's allowed to call only from thread %s",
            __func__,
            os_task_get_name(),
            pcTaskGetTaskName(p_http_async_info->p_task));
        assert(p_http_async_info->p_task == os_task_get_cur_task_handle());
    }

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
            switch (p_http_async_info->recipient)
            {
                case HTTP_POST_RECIPIENT_STATS:
                    reset_info_clear_extra_info();
                    break;
                case HTTP_POST_RECIPIENT_ADVS1:
                case HTTP_POST_RECIPIENT_ADVS2:
                    adv_post_last_successful_network_comm_timestamp_update();
                    break;
            }
            flag_success = true;
        }
        else
        {
            LOG_ERR(
                "### HTTP POST to URL=%s: STATUS=%d",
                p_http_async_info->http_client_config.esp_http_client_config.url,
                http_status);
            if (HTTP_RESP_CODE_429 == http_status)
            {
                switch (p_http_async_info->recipient)
                {
                    case HTTP_POST_RECIPIENT_STATS:
                        break;
                    case HTTP_POST_RECIPIENT_ADVS1:
                    case HTTP_POST_RECIPIENT_ADVS2:
                        adv_post_last_successful_network_comm_timestamp_update();
                        break;
                }
            }
        }
    }
    else
    {
        LOG_ERR_ESP(
            err,
            "### HTTP POST to URL=%s: failed",
            p_http_async_info->http_client_config.esp_http_client_config.url);
    }

    LOG_DBG("esp_http_client_cleanup");
    if (NULL != p_http_async_info->http_client_config.esp_http_client_config.cert_pem)
    {
        os_free(p_http_async_info->http_client_config.esp_http_client_config.cert_pem);
    }
    if (NULL != p_http_async_info->http_client_config.esp_http_client_config.client_cert_pem)
    {
        os_free(p_http_async_info->http_client_config.esp_http_client_config.client_cert_pem);
    }
    if (NULL != p_http_async_info->http_client_config.esp_http_client_config.client_key_pem)
    {
        os_free(p_http_async_info->http_client_config.esp_http_client_config.client_key_pem);
    }
    esp_http_client_cleanup(p_http_async_info->p_http_client_handle);
    p_http_async_info->p_http_client_handle = NULL;

    http_async_info_free_data(p_http_async_info);

    if (flag_success && (HTTP_POST_RECIPIENT_STATS != p_http_async_info->recipient))
    {
        if (!fw_update_mark_app_valid_cancel_rollback())
        {
            LOG_ERR("%s failed", "fw_update_mark_app_valid_cancel_rollback");
        }
    }
    switch (p_http_async_info->recipient)
    {
        case HTTP_POST_RECIPIENT_STATS:
            break;
        case HTTP_POST_RECIPIENT_ADVS1:
            if (flag_success)
            {
                leds_notify_http1_data_sent_successfully();
                adv1_post_timer_restart_with_default_period();
            }
            else
            {
                leds_notify_http1_data_sent_fail();
                adv1_post_timer_restart_with_increased_period();
            }
            break;
        case HTTP_POST_RECIPIENT_ADVS2:
            if (flag_success)
            {
                leds_notify_http2_data_sent_successfully();
                adv2_post_timer_restart_with_default_period();
            }
            else
            {
                leds_notify_http2_data_sent_fail();
                adv2_post_timer_restart_with_increased_period();
            }
            break;
    }
    LOG_DBG("os_sema_signal: p_http_async_sema");
    os_sema_signal(p_http_async_info->p_http_async_sema);
    return true;
}

static esp_err_t
http_download_event_handler(esp_http_client_event_t* p_evt)
{
    http_download_cb_info_t* const p_cb_info = p_evt->user_data;
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
                LOG_ERR("HTTP_EVENT_ON_DATA: cb_on_data failed");
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

static esp_err_t
http_check_event_handler(esp_http_client_event_t* p_evt)
{
    http_check_cb_info_t* const p_cb_info = p_evt->user_data;
    switch (p_evt->event_id)
    {
        case HTTP_EVENT_ERROR:
            LOG_ERR("HTTP_EVENT_ERROR");
            http_download_feed_task_watchdog_if_needed(p_cb_info->flag_feed_task_watchdog);
            break;

        case HTTP_EVENT_ON_CONNECTED:
            LOG_INFO("HTTP_EVENT_ON_CONNECTED");
            http_download_feed_task_watchdog_if_needed(p_cb_info->flag_feed_task_watchdog);
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
                p_cb_info->content_length = os_str_to_uint32_cptr(p_evt->header_value, NULL, BASE_10);
            }
            break;

        case HTTP_EVENT_ON_DATA:
            LOG_DBG("HTTP_EVENT_ON_DATA, len=%d", p_evt->data_len);
            LOG_DUMP_VERBOSE(p_evt->data, p_evt->data_len, "<--:");
            http_download_feed_task_watchdog_if_needed(p_cb_info->flag_feed_task_watchdog);
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

static http_server_resp_t
http_download_by_handle(
    esp_http_client_handle_t http_handle,
    const bool               flag_feed_task_watchdog,
    const TimeUnitsSeconds_t timeout_seconds)
{
    esp_err_t err = esp_http_client_set_header(http_handle, "Accept", "text/html,application/octet-stream,*/*");
    if (ESP_OK != err)
    {
        LOG_ERR("%s failed", "esp_http_client_set_header");
        return http_server_resp_500();
    }
    err = esp_http_client_set_header(http_handle, "User-Agent", "RuuviGateway");
    if (ESP_OK != err)
    {
        LOG_ERR("%s failed", "esp_http_client_set_header");
        return http_server_resp_500();
    }

    LOG_DBG("esp_http_client_perform");
    err = esp_http_client_perform(http_handle);
    if (ESP_OK == err)
    {
        LOG_DBG(
            "HTTP GET Status = %d, content_length = %d",
            esp_http_client_get_status_code(http_handle),
            esp_http_client_get_content_length(http_handle));
        return http_server_resp_err(esp_http_client_get_status_code(http_handle));
    }
    if (ESP_ERR_HTTP_EAGAIN != err)
    {
        LOG_ERR_ESP(
            err,
            "esp_http_client_perform failed, HTTP resp code %d (http_handle=%ld)",
            (printf_int_t)esp_http_client_get_status_code(http_handle),
            (printf_long_t)http_handle);
        return http_server_resp_502();
    }

    LOG_DBG("http_wait_until_async_req_completed");
    http_server_resp_t resp = http_wait_until_async_req_completed(
        http_handle,
        NULL,
        flag_feed_task_watchdog,
        timeout_seconds);
    LOG_DBG("http_wait_until_async_req_completed: finished");

    return resp;
}

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

static esp_http_client_config_t*
http_download_create_config(
    const char* const                     p_url,
    const TimeUnitsSeconds_t              timeout_seconds,
    const esp_http_client_method_t        http_method,
    const esp_http_client_auth_type_t     http_client_auth_type,
    const ruuvi_gw_cfg_http_auth_t* const p_http_auth,
    http_event_handle_cb                  p_event_handler,
    void* const                           p_cb_info,
    const char* const                     p_server_cert,
    const char* const                     p_client_cert,
    const char* const                     p_client_key)
{
    esp_http_client_config_t* p_http_config = os_calloc(1, sizeof(*p_http_config));
    if (NULL == p_http_config)
    {
        LOG_ERR("Can't allocate memory for http_config");
        return false;
    }
    p_http_config->url  = p_url;
    p_http_config->host = NULL;
    p_http_config->port = 0;

    p_http_config->username = (HTTP_AUTH_TYPE_BASIC == http_client_auth_type) ? p_http_auth->auth_basic.user.buf : NULL;
    p_http_config->password = (HTTP_AUTH_TYPE_BASIC == http_client_auth_type) ? p_http_auth->auth_basic.password.buf
                                                                              : NULL;
    p_http_config->auth_type = http_client_auth_type;

    p_http_config->path                        = NULL;
    p_http_config->query                       = NULL;
    p_http_config->cert_pem                    = p_server_cert;
    p_http_config->client_cert_pem             = p_client_cert;
    p_http_config->client_key_pem              = p_client_key;
    p_http_config->user_agent                  = NULL;
    p_http_config->method                      = http_method;
    p_http_config->timeout_ms                  = (int)(timeout_seconds * TIME_UNITS_MS_PER_SECOND);
    p_http_config->disable_auto_redirect       = false;
    p_http_config->max_redirection_count       = 0;
    p_http_config->max_authorization_retries   = 0;
    p_http_config->event_handler               = p_event_handler;
    p_http_config->transport_type              = HTTP_TRANSPORT_UNKNOWN;
    p_http_config->buffer_size                 = 2048;
    p_http_config->buffer_size_tx              = 1024;
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

bool
http_download_with_auth(
    const http_download_param_t* const    p_param,
    const gw_cfg_http_auth_type_e         auth_type,
    const ruuvi_gw_cfg_http_auth_t* const p_http_auth,
    const http_header_item_t* const       p_extra_header_item)
{
    LOG_INFO("http_download: URL: %s", p_param->p_url);

    if ((GW_CFG_HTTP_AUTH_TYPE_NONE != auth_type) && (NULL == p_http_auth))
    {
        LOG_ERR("Auth type is not NONE, but p_http_auth is NULL");
        return false;
    }
    if (!gw_status_is_network_connected())
    {
        LOG_ERR("HTTP download failed - no network connection");
        return false;
    }

    const esp_http_client_auth_type_t http_client_auth_type = (GW_CFG_HTTP_AUTH_TYPE_BASIC == auth_type)
                                                                  ? HTTP_AUTH_TYPE_BASIC
                                                                  : HTTP_AUTH_TYPE_NONE;

    if (HTTP_AUTH_TYPE_BASIC == http_client_auth_type)
    {
        LOG_INFO("http_download: Auth: Basic, Username: %s", p_http_auth->auth_basic.user.buf);
        LOG_DBG("http_download: Auth: Basic, Password: %s", p_http_auth->auth_basic.password.buf);
    }

    http_download_cb_info_t* p_cb_info = os_calloc(1, sizeof(*p_cb_info));
    if (NULL == p_cb_info)
    {
        LOG_ERR("Can't allocate memory");
        return false;
    }
    p_cb_info->cb_on_data              = p_param->p_cb_on_data;
    p_cb_info->p_user_data             = p_param->p_user_data;
    p_cb_info->http_handle             = NULL;
    p_cb_info->content_length          = 0;
    p_cb_info->offset                  = 0;
    p_cb_info->flag_feed_task_watchdog = p_param->flag_feed_task_watchdog;

    esp_http_client_config_t* p_http_config = http_download_create_config(
        p_param->p_url,
        p_param->timeout_seconds,
        HTTP_METHOD_GET,
        http_client_auth_type,
        p_http_auth,
        &http_download_event_handler,
        p_cb_info,
        p_param->p_server_cert,
        p_param->p_client_cert,
        p_param->p_client_key);
    if (NULL == p_http_config)
    {
        LOG_ERR("Can't allocate memory for http_config");
        os_free(p_cb_info);
        return false;
    }

    LOG_DBG("suspend_relaying_and_wait");
    suspend_relaying_and_wait(p_param->flag_free_memory);
    LOG_DBG("suspend_relaying_and_wait: finished");

    p_cb_info->http_handle = esp_http_client_init(p_http_config);
    if (NULL == p_cb_info->http_handle)
    {
        LOG_ERR("Can't init http client");
        os_free(p_http_config);
        os_free(p_cb_info);
        resume_relaying_and_wait(p_param->flag_free_memory);
        return false;
    }

    if (GW_CFG_HTTP_AUTH_TYPE_BEARER == auth_type)
    {
        str_buf_t str_buf = str_buf_printf_with_alloc("Bearer %s", p_http_auth->auth_bearer.token.buf);
        if (NULL == str_buf.buf)
        {
            LOG_ERR("Can't allocate memory for bearer token");
            os_free(p_http_config);
            os_free(p_cb_info);
            resume_relaying_and_wait(p_param->flag_free_memory);
            return false;
        }
        LOG_INFO(
            "http_download: Add HTTP header: %s: %s",
            "Authorization",
            (LOG_LOCAL_LEVEL >= LOG_LEVEL_DEBUG) ? str_buf.buf : "********");
        esp_http_client_set_header(p_cb_info->http_handle, "Authorization", str_buf.buf);
        str_buf_free_buf(&str_buf);
    }
    if (GW_CFG_HTTP_AUTH_TYPE_TOKEN == auth_type)
    {
        str_buf_t str_buf = str_buf_printf_with_alloc("Token %s", p_http_auth->auth_token.token.buf);
        if (NULL == str_buf.buf)
        {
            LOG_ERR("Can't allocate memory for bearer token");
            os_free(p_http_config);
            os_free(p_cb_info);
            resume_relaying_and_wait(p_param->flag_free_memory);
            return false;
        }
        LOG_INFO(
            "http_download: Add HTTP header: %s: %s",
            "Authorization",
            (LOG_LOCAL_LEVEL >= LOG_LEVEL_DEBUG) ? str_buf.buf : "********");
        esp_http_client_set_header(p_cb_info->http_handle, "Authorization", str_buf.buf);
        str_buf_free_buf(&str_buf);
    }
    if (NULL != p_extra_header_item)
    {
        LOG_INFO("http_download: Add HTTP header: %s: %s", p_extra_header_item->p_key, p_extra_header_item->p_value);
        esp_http_client_set_header(p_cb_info->http_handle, p_extra_header_item->p_key, p_extra_header_item->p_value);
    }

    LOG_DBG("http_download_by_handle");
    http_server_resp_t resp = http_download_by_handle(
        p_cb_info->http_handle,
        p_param->flag_feed_task_watchdog,
        p_param->timeout_seconds);

    LOG_DBG("esp_http_client_cleanup");
    const esp_err_t err = esp_http_client_cleanup(p_cb_info->http_handle);
    if (ESP_OK != err)
    {
        LOG_ERR_ESP(err, "esp_http_client_cleanup failed");
    }

    os_free(p_http_config);
    os_free(p_cb_info);
    resume_relaying_and_wait(p_param->flag_free_memory);

    if ((HTTP_CONTENT_LOCATION_HEAP == resp.content_location) && (NULL != resp.select_location.memory.p_buf))
    {
        os_free(resp.select_location.memory.p_buf);
    }

    if (HTTP_RESP_CODE_200 == resp.http_resp_code)
    {
        return true;
    }
    return false;
}

bool
http_check_with_auth(
    const http_check_param_t* const       p_param,
    const gw_cfg_http_auth_type_e         auth_type,
    const ruuvi_gw_cfg_http_auth_t* const p_http_auth,
    const http_header_item_t* const       p_extra_header_item,
    http_resp_code_e* const               p_http_resp_code)
{
    LOG_INFO("http_check: URL: %s", p_param->p_url);

    if ((GW_CFG_HTTP_AUTH_TYPE_NONE != auth_type) && (NULL == p_http_auth))
    {
        LOG_ERR("Auth type is not NONE, but p_http_auth is NULL");
        return false;
    }
    if (!gw_status_is_network_connected())
    {
        LOG_ERR("HTTP check failed - no network connection");
        return false;
    }

    http_download_cb_info_t* p_cb_info = os_calloc(1, sizeof(*p_cb_info));
    if (NULL == p_cb_info)
    {
        LOG_ERR("Can't allocate memory");
        return false;
    }
    p_cb_info->cb_on_data              = NULL;
    p_cb_info->p_user_data             = NULL;
    p_cb_info->http_handle             = NULL;
    p_cb_info->content_length          = 0;
    p_cb_info->offset                  = 0;
    p_cb_info->flag_feed_task_watchdog = p_param->flag_feed_task_watchdog;

    const esp_http_client_auth_type_t http_client_auth_type = (GW_CFG_HTTP_AUTH_TYPE_BASIC == auth_type)
                                                                  ? HTTP_AUTH_TYPE_BASIC
                                                                  : HTTP_AUTH_TYPE_NONE;

    esp_http_client_config_t* p_http_config = http_download_create_config(
        p_param->p_url,
        p_param->timeout_seconds,
        HTTP_METHOD_HEAD,
        http_client_auth_type,
        p_http_auth,
        &http_check_event_handler,
        p_cb_info,
        p_param->p_server_cert,
        p_param->p_client_cert,
        p_param->p_client_key);
    if (NULL == p_http_config)
    {
        LOG_ERR("Can't allocate memory for http_config");
        os_free(p_cb_info);
        return false;
    }
    if (HTTP_AUTH_TYPE_BASIC == http_client_auth_type)
    {
        LOG_INFO("http_check: Auth: Basic, Username: %s", p_http_auth->auth_basic.user.buf);
    }

    suspend_relaying_and_wait(p_param->flag_free_memory);

    p_cb_info->http_handle = esp_http_client_init(p_http_config);
    if (NULL == p_cb_info->http_handle)
    {
        LOG_ERR("Can't init http client");
        os_free(p_http_config);
        os_free(p_cb_info);
        resume_relaying_and_wait(p_param->flag_free_memory);
        return false;
    }

    if (GW_CFG_HTTP_AUTH_TYPE_BEARER == auth_type)
    {
        str_buf_t str_buf = str_buf_printf_with_alloc("Bearer %s", p_http_auth->auth_bearer.token.buf);
        if (NULL == str_buf.buf)
        {
            LOG_ERR("Can't allocate memory for bearer token");
            os_free(p_http_config);
            os_free(p_cb_info);
            resume_relaying_and_wait(p_param->flag_free_memory);
            return false;
        }
        LOG_INFO(
            "http_check: Add HTTP header: %s: %s",
            "Authorization",
            (LOG_LOCAL_LEVEL >= LOG_LEVEL_DEBUG) ? str_buf.buf : "********");
        esp_http_client_set_header(p_cb_info->http_handle, "Authorization", str_buf.buf);
        str_buf_free_buf(&str_buf);
    }
    if (GW_CFG_HTTP_AUTH_TYPE_TOKEN == auth_type)
    {
        str_buf_t str_buf = str_buf_printf_with_alloc("Token %s", p_http_auth->auth_token.token.buf);
        if (NULL == str_buf.buf)
        {
            LOG_ERR("Can't allocate memory for Token");
            os_free(p_http_config);
            os_free(p_cb_info);
            resume_relaying_and_wait(p_param->flag_free_memory);
            return false;
        }
        LOG_INFO(
            "http_check: Add HTTP header: %s: %s",
            "Authorization",
            (LOG_LOCAL_LEVEL >= LOG_LEVEL_DEBUG) ? str_buf.buf : "********");
        esp_http_client_set_header(p_cb_info->http_handle, "Authorization", str_buf.buf);
        str_buf_free_buf(&str_buf);
    }
    if (NULL != p_extra_header_item)
    {
        LOG_INFO("http_check: Add HTTP header: %s: %s", p_extra_header_item->p_key, p_extra_header_item->p_value);
        esp_http_client_set_header(p_cb_info->http_handle, p_extra_header_item->p_key, p_extra_header_item->p_value);
    }

    LOG_DBG("http_check_by_handle");
    http_server_resp_t resp = http_download_by_handle(
        p_cb_info->http_handle,
        p_param->flag_feed_task_watchdog,
        p_param->timeout_seconds);

    LOG_DBG("esp_http_client_cleanup");
    const esp_err_t err = esp_http_client_cleanup(p_cb_info->http_handle);
    if (ESP_OK != err)
    {
        LOG_ERR_ESP(err, "esp_http_client_cleanup failed");
    }

    os_free(p_http_config);
    os_free(p_cb_info);
    resume_relaying_and_wait(p_param->flag_free_memory);

    if ((HTTP_CONTENT_LOCATION_HEAP == resp.content_location) && (NULL != resp.select_location.memory.p_buf))
    {
        os_free(resp.select_location.memory.p_buf);
    }

    if (HTTP_RESP_CODE_200 == resp.http_resp_code)
    {
        return true;
    }
    return false;
}

bool
http_download(const http_download_param_t* const p_param)
{
    return http_download_with_auth(p_param, GW_CFG_HTTP_AUTH_TYPE_NONE, NULL, NULL);
}

static const char*
http_method_to_str(http_async_info_t* p_http_async_info)
{
    const esp_http_client_method_t method = p_http_async_info->http_client_config.esp_http_client_config.method;
    switch (method)
    {
        case HTTP_METHOD_GET:
            return "GET";
        case HTTP_METHOD_POST:
            return "POST";
        case HTTP_METHOD_DELETE:
            return "DELETE";
        case HTTP_METHOD_HEAD:
            return "HEAD";
        default:
            break;
    }
    return "UNKNOWN";
}

void
http_abort_any_req_during_processing(void)
{
    http_async_info_t* p_http_async_info = get_http_async_info();
    LOG_DBG("os_sema_wait_immediate: p_http_async_sema");
    if (!os_sema_wait_immediate(p_http_async_info->p_http_async_sema))
    {
        LOG_WARN(
            "### Abort HTTP %s to URL=%s",
            http_method_to_str(p_http_async_info),
            p_http_async_info->http_client_config.esp_http_client_config.url);

        if (p_http_async_info->p_task != os_task_get_cur_task_handle())
        {
            LOG_ERR(
                "%s is called from thread %s, but it's allowed to call only from thread %s",
                __func__,
                os_task_get_name(),
                pcTaskGetTaskName(p_http_async_info->p_task));
            assert(p_http_async_info->p_task == os_task_get_cur_task_handle());
        }

        LOG_DBG("esp_http_client_cleanup");
        if (NULL != p_http_async_info->http_client_config.esp_http_client_config.cert_pem)
        {
            os_free(p_http_async_info->http_client_config.esp_http_client_config.cert_pem);
        }
        if (NULL != p_http_async_info->http_client_config.esp_http_client_config.client_cert_pem)
        {
            os_free(p_http_async_info->http_client_config.esp_http_client_config.client_cert_pem);
        }
        if (NULL != p_http_async_info->http_client_config.esp_http_client_config.client_key_pem)
        {
            os_free(p_http_async_info->http_client_config.esp_http_client_config.client_key_pem);
        }
        esp_http_client_cleanup(p_http_async_info->p_http_client_handle);
        p_http_async_info->p_http_client_handle = NULL;
        http_async_info_free_data(p_http_async_info);
    }
    LOG_DBG("os_sema_signal: p_http_async_sema");
    os_sema_signal(p_http_async_info->p_http_async_sema);
}
