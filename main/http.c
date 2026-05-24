/**
 * @file http.c
 * @author Jukka Saari
 * @date 2019-11-27
 * @copyright Ruuvi Innovations Ltd, license BSD-3-Clause.
 */

#include "http.h"
#include <string.h>
#include <errno.h>
#include <esp_task_wdt.h>
#include "cjson_wrap.h"
#include "esp_http_client.h"
#include "mbedtls/ssl_misc.h"
#include "ruuvi_gateway.h"
#include "ruuvi_nvs.h"
#include "leds.h"
#include "os_str.h"
#include "hmac_sha256.h"
#include "adv_post_timers.h"
#include "str_buf.h"
#include "reset_info.h"
#include "os_sema.h"
#include "os_malloc.h"
#include "http_server_resp.h"
#include "snprintf_with_esp_err_desc.h"
#include "esp_tls_err.h"
#include "reset_task.h"
#include "network_timeout.h"
#include "tls_shared_buf.h"

#define LOG_LOCAL_LEVEL LOG_LEVEL_INFO
#include "log.h"
static const char TAG[] = "http";

#if (LOG_LOCAL_LEVEL >= LOG_LEVEL_DEBUG)
#warning Debug log level prints out the passwords as a "plaintext".
#endif

#define HTTP_POST_MAX_LEN_TO_PRINT_LOG (4U * 1024U)

typedef int esp_http_client_len_t;
typedef int esp_http_client_http_status_code_t;

static http_async_info_t g_http_async_info;

http_async_info_t*
http_get_async_info(void)
{
    http_async_info_t* p_http_async_info = &g_http_async_info;
    if (NULL == p_http_async_info->p_http_async_sema)
    {
        p_http_async_info->p_http_async_sema = os_sema_create_static(&p_http_async_info->http_async_sema_mem);
        os_sema_signal(p_http_async_info->p_http_async_sema);
    }
    return p_http_async_info;
}

static http_server_resp_t
http_wait_until_async_req_completed_handle_http_resp(
    esp_http_client_handle_t   p_http_handle,
    http_resp_cb_info_t* const p_cb_info)
{
    const http_resp_code_e http_resp_code = esp_http_client_get_status_code(p_http_handle);
    LOG_DBG(
        "HTTP GET Status = %d, content_length = %d",
        http_resp_code,
        esp_http_client_get_content_length(p_http_handle));
    if ((HTTP_RESP_CODE_200 <= http_resp_code) && (http_resp_code <= HTTP_RESP_CODE_299))
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

    const str_buf_t http_resp_buf = str_buf_printf_with_alloc(
        "HTTP response status: %u, Message from the server: %s",
        http_resp_code,
        p_cb_info->p_buf);
    os_free(p_cb_info->p_buf);
    p_cb_info->p_buf = NULL;

    if (NULL == http_resp_buf.buf)
    {
        LOG_ERR("Can't allocate memory for http_resp");
        return http_server_resp_500();
    }

    return http_server_resp_json_in_heap(http_resp_code, http_resp_buf.buf);
}

void
http_feed_task_watchdog_if_needed(const bool flag_feed_task_watchdog)
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

http_server_resp_t
http_wait_until_async_req_completed(
    esp_http_client_handle_t   p_http_handle,
    http_resp_cb_info_t* const p_cb_info,
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
            if ((NULL != p_cb_info) && (NULL != p_cb_info->p_buf))
            {
                os_free(p_cb_info->p_buf);
                p_cb_info->p_buf = NULL;
            }
            if (esp_tls_err_is_cannot_resolve_hostname(err))
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
        http_feed_task_watchdog_if_needed(flag_feed_task_watchdog);
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
cb_on_post_get_chunk(void* p_user_data, const void** p_p_buf, size_t* p_len)
{
    json_stream_gen_t* p_gen   = p_user_data;
    const char* const  p_chunk = json_stream_gen_get_next_chunk(p_gen);
    if (NULL == p_chunk)
    {
        return false;
    }
    *p_p_buf = p_chunk;
    *p_len   = strlen(p_chunk);
    if (0 != *p_len)
    {
        // Don't print logs on INFO level because it's called from the HTTP event handler
        LOG_DBG("HTTP POST DATA:\n%s", p_chunk);
    }
    return true;
}

static bool
http_send_async_from_json_stream_gen(http_async_info_t* const p_http_async_info)
{
    const json_stream_gen_size_t json_len = json_stream_gen_calc_size(p_http_async_info->select.p_gen);
    LOG_INFO("HTTP POST DATA len=%u:", (printf_int_t)json_len);
    while (true)
    {
        const char* const p_chunk = json_stream_gen_get_next_chunk(p_http_async_info->select.p_gen);
        if (NULL == p_chunk)
        {
            break;
        }
        if ('\0' == *p_chunk)
        {
            break;
        }
        if (json_len < HTTP_POST_MAX_LEN_TO_PRINT_LOG)
        {
            LOG_INFO("HTTP POST DATA:\n%s", p_chunk);
        }
        else
        {
            LOG_DBG("HTTP POST DATA:\n%s", p_chunk);
        }
        vTaskDelay(pdMS_TO_TICKS(5)); // A delay to avoid triggering watchdog
    }
    json_stream_gen_reset(p_http_async_info->select.p_gen);

    const esp_err_t err = esp_http_client_set_cb_on_post_get_chunk(
        p_http_async_info->p_http_client_handle,
        json_len,
        &cb_on_post_get_chunk,
        (void*)p_http_async_info->select.p_gen);
    if (0 != err)
    {
        LOG_ERR_ESP(err, "%s failed", "esp_http_client_set_cb_on_post_get_chunk");
        return false;
    }
    return true;
}

bool
http_send_async(http_async_info_t* const p_http_async_info)
{
    const esp_http_client_config_t* const p_http_config = &p_http_async_info->http_client_config.esp_http_client_config;

    LOG_INFO(
        "### HTTP POST to URL=http%s://%s:%u/%s%s%s",
        (HTTP_TRANSPORT_OVER_SSL == p_http_config->transport_type) ? "s" : "",
        p_http_config->host,
        p_http_config->port,
        (NULL != p_http_config->path)
            ? p_http_config->path
            : ((NULL != p_http_config->cb_path_stream_reader) ? "[path from stream reader]" : ""),
        ((NULL != p_http_config->query) || (NULL != p_http_config->cb_query_stream_reader)) ? "?" : "",
        (NULL != p_http_config->query)
            ? p_http_config->query
            : ((NULL != p_http_config->cb_query_stream_reader) ? "[query from stream reader]" : ""));

    ruuvi_log_heap_usage();

    if (p_http_async_info->use_json_stream_gen)
    {
        if (!http_send_async_from_json_stream_gen(p_http_async_info))
        {
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
        LOG_ERR_ESP(
            err,
            "### HTTP POST to URL=http%s://%s:%u/%s%s%s: request failed",
            (HTTP_TRANSPORT_OVER_SSL == p_http_config->transport_type) ? "s" : "",
            p_http_config->host,
            p_http_config->port,
            (NULL != p_http_config->path)
                ? p_http_config->path
                : ((NULL != p_http_config->cb_path_stream_reader) ? "[path from stream reader]" : ""),
            ((NULL != p_http_config->query) || (NULL != p_http_config->cb_query_stream_reader)) ? "?" : "",
            (NULL != p_http_config->query)
                ? p_http_config->query
                : ((NULL != p_http_config->cb_query_stream_reader) ? "[query from stream reader]" : ""));
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
        ruuvi_log_heap_usage();
        return false;
    }
    return true;
}

void
http_async_info_free_data(http_async_info_t* const p_http_async_info)
{
    if (NULL != p_http_async_info->p_tls_shared_buf)
    {
        LOG_DBG("%s: tls_shared_buf_unlock_https_post", __func__);
        tls_shared_buf_unlock_https_post(&p_http_async_info->p_tls_shared_buf);
    }
    if (NULL != p_http_async_info->http_client_config.esp_http_client_config.cert_pem)
    {
        os_free(p_http_async_info->http_client_config.esp_http_client_config.cert_pem);
        p_http_async_info->http_client_config.esp_http_client_config.cert_pem = NULL;
    }
    if (NULL != p_http_async_info->http_client_config.esp_http_client_config.client_cert_pem)
    {
        os_free(p_http_async_info->http_client_config.esp_http_client_config.client_cert_pem);
        p_http_async_info->http_client_config.esp_http_client_config.client_cert_pem = NULL;
    }
    if (NULL != p_http_async_info->http_client_config.esp_http_client_config.client_key_pem)
    {
        os_free(p_http_async_info->http_client_config.esp_http_client_config.client_key_pem);
        p_http_async_info->http_client_config.esp_http_client_config.client_key_pem = NULL;
    }
    if (p_http_async_info->use_json_stream_gen)
    {
        if (NULL != p_http_async_info->select.p_gen)
        {
            json_stream_gen_delete(&p_http_async_info->select.p_gen);
            p_http_async_info->select.p_gen = NULL;
        }
    }
    else
    {
        cjson_wrap_free_json_str(&p_http_async_info->select.cjson_str);
        p_http_async_info->select.cjson_str = cjson_wrap_str_null();
    }
}

bool
http_handle_add_authorization_if_needed(
    esp_http_client_handle_t              http_handle,
    const gw_cfg_http_auth_type_e         auth_type,
    const ruuvi_gw_cfg_http_auth_t* const p_http_auth)
{
    str_buf_t str_buf = str_buf_init_null();
    if (GW_CFG_HTTP_AUTH_TYPE_BEARER == auth_type)
    {
        str_buf = str_buf_printf_with_alloc("Bearer %s", p_http_auth->auth_bearer.token.buf);
        if (NULL == str_buf.buf)
        {
            LOG_ERR("Can't allocate memory for bearer token");
            return false;
        }
    }
    else if (GW_CFG_HTTP_AUTH_TYPE_TOKEN == auth_type)
    {
        str_buf = str_buf_printf_with_alloc("Token %s", p_http_auth->auth_token.token.buf);
        if (NULL == str_buf.buf)
        {
            return false;
        }
    }
    else if (GW_CFG_HTTP_AUTH_TYPE_APIKEY == auth_type)
    {
        str_buf = str_buf_printf_with_alloc("%s", p_http_auth->auth_apikey.api_key.buf);
        if (NULL == str_buf.buf)
        {
            return false;
        }
    }
    else
    {
        // MISRA C:2012, 15.7 - All if...else if constructs shall be terminated with an else statement
    }
    if (NULL != str_buf.buf)
    {
        LOG_INFO(
            "Add HTTP header: %s: %s",
            "Authorization",
            (LOG_LOCAL_LEVEL >= LOG_LEVEL_DEBUG) ? str_buf.buf : "********");
        esp_http_client_set_header(http_handle, "Authorization", str_buf.buf);
        str_buf_free_buf(&str_buf);
    }
    return true;
}

static void
http_async_poll_handle_resp_ok(
    const http_async_info_t* const           p_http_async_info,
    const esp_http_client_http_status_code_t http_status)
{
    const esp_http_client_config_t* const p_http_config = &p_http_async_info->http_client_config.esp_http_client_config;
    LOG_INFO(
        "### HTTP POST to URL=http%s://%s:%u/%s%s%s: STATUS=%d",
        (HTTP_TRANSPORT_OVER_SSL == p_http_config->transport_type) ? "s" : "",
        p_http_config->host,
        p_http_config->port,
        (NULL != p_http_config->path)
            ? p_http_config->path
            : ((NULL != p_http_config->cb_path_stream_reader) ? "[path from stream reader]" : ""),
        ((NULL != p_http_config->query) || (NULL != p_http_config->cb_query_stream_reader)) ? "?" : "",
        (NULL != p_http_config->query)
            ? p_http_config->query
            : ((NULL != p_http_config->cb_query_stream_reader) ? "[query from stream reader]" : ""),
        http_status);
    switch (p_http_async_info->recipient)
    {
        case HTTP_POST_RECIPIENT_STATS:
            reset_info_clear_extra_info();
            break;
        case HTTP_POST_RECIPIENT_ADVS1:
            ATTR_FALLTHROUGH;
        case HTTP_POST_RECIPIENT_ADVS2:
            network_timeout_update_timestamp();
            break;
    }
}

static void
http_async_poll_handle_resp_err(
    const http_async_info_t* const           p_http_async_info,
    const esp_http_client_http_status_code_t http_status)
{
    const esp_http_client_config_t* const p_http_config = &p_http_async_info->http_client_config.esp_http_client_config;
    LOG_ERR(
        "### HTTP POST to URL=http%s://%s:%u/%s%s%s: STATUS=%d",
        (HTTP_TRANSPORT_OVER_SSL == p_http_config->transport_type) ? "s" : "",
        p_http_config->host,
        p_http_config->port,
        (NULL != p_http_config->path)
            ? p_http_config->path
            : ((NULL != p_http_config->cb_path_stream_reader) ? "[path from stream reader]" : ""),
        ((NULL != p_http_config->query) || (NULL != p_http_config->cb_query_stream_reader)) ? "?" : "",
        (NULL != p_http_config->query)
            ? p_http_config->query
            : ((NULL != p_http_config->cb_query_stream_reader) ? "[query from stream reader]" : ""),
        http_status);
    if (HTTP_RESP_CODE_429 == http_status)
    {
        switch (p_http_async_info->recipient)
        {
            case HTTP_POST_RECIPIENT_STATS:
                break;
            case HTTP_POST_RECIPIENT_ADVS1:
            case HTTP_POST_RECIPIENT_ADVS2:
                network_timeout_update_timestamp();
                break;
        }
    }
}

static void
http_async_poll_do_actions_after_completion_advs1(const bool flag_success)
{
    if (flag_success)
    {
        leds_notify_http1_data_sent_successfully();
        adv1_post_timer_relaunch_with_default_period();
    }
    else
    {
        leds_notify_http1_data_sent_fail();
        adv1_post_timer_relaunch_with_increased_period();
    }
}

static void
http_async_poll_do_actions_after_completion_advs2(const bool flag_success)
{
    if (flag_success)
    {
        leds_notify_http2_data_sent_successfully();
        adv2_post_timer_relaunch_with_default_period();
    }
    else
    {
        leds_notify_http2_data_sent_fail();
        adv2_post_timer_relaunch_with_increased_period();
    }
}

static void
http_async_poll_do_actions_after_completion(const http_async_info_t* const p_http_async_info, const bool flag_success)
{
    switch (p_http_async_info->recipient)
    {
        case HTTP_POST_RECIPIENT_STATS:
            break;
        case HTTP_POST_RECIPIENT_ADVS1:
            http_async_poll_do_actions_after_completion_advs1(flag_success);
            break;
        case HTTP_POST_RECIPIENT_ADVS2:
            http_async_poll_do_actions_after_completion_advs2(flag_success);
            break;
    }
}

bool
http_async_poll(void)
{
    http_async_info_t* p_http_async_info = http_get_async_info();

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
        if ((HTTP_RESP_CODE_200 <= http_status) && (http_status <= HTTP_RESP_CODE_299))
        {
            http_async_poll_handle_resp_ok(p_http_async_info, http_status);
            flag_success = true;
        }
        else
        {
            http_async_poll_handle_resp_err(p_http_async_info, http_status);
        }
    }
    else
    {
        const esp_http_client_config_t* const p_http_config = &p_http_async_info->http_client_config
                                                                   .esp_http_client_config;
        LOG_ERR_ESP(
            err,
            "### HTTP POST to URL=http%s://%s:%u/%s%s%s: failed",
            (HTTP_TRANSPORT_OVER_SSL == p_http_config->transport_type) ? "s" : "",
            p_http_config->host,
            p_http_config->port,
            (NULL != p_http_config->path)
                ? p_http_config->path
                : ((NULL != p_http_config->cb_path_stream_reader) ? "[path from stream reader]" : ""),
            ((NULL != p_http_config->query) || (NULL != p_http_config->cb_query_stream_reader)) ? "?" : "",
            (NULL != p_http_config->query)
                ? p_http_config->query
                : ((NULL != p_http_config->cb_query_stream_reader) ? "[query from stream reader]" : ""));
        if (esp_tls_err_is_ssl_alloc_failed(err))
        {
            // In case if esp_http_client_perform fails with MBEDTLS_ERR_SSL_ALLOC_FAILED
            // this means that there is not enough memory to allocate buffers for TLS connection
            // and the handshake process has not been started.
            LOG_ERR("Failed to allocate buffers for TLS connection");
            gateway_restart_low_memory();
        }
    }

    LOG_DBG("esp_http_client_cleanup");
    esp_http_client_cleanup(p_http_async_info->p_http_client_handle);
    p_http_async_info->p_http_client_handle = NULL;

    http_async_info_free_data(p_http_async_info);

    if (flag_success && (HTTP_POST_RECIPIENT_STATS != p_http_async_info->recipient))
    {
        if (!ruuvi_gw_mark_app_valid_cancel_rollback()) // NOSONAR
        {
            LOG_ERR("%s failed", "fw_update_mark_app_valid_cancel_rollback");
        }
    }

    http_async_poll_do_actions_after_completion(p_http_async_info, flag_success);

    LOG_DBG("os_sema_signal: p_http_async_sema");
    os_sema_signal(p_http_async_info->p_http_async_sema);
    return true;
}

const char*
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

static const char*
http_method_to_str(const http_async_info_t* const p_http_async_info)
{
    return http_client_method_to_str(p_http_async_info->http_client_config.esp_http_client_config.method);
}

void
http_abort_any_req_during_processing(void)
{
    http_async_info_t* p_http_async_info = http_get_async_info();
    LOG_DBG("os_sema_wait_immediate: p_http_async_sema");
    if (!os_sema_wait_immediate(p_http_async_info->p_http_async_sema))
    {
        const esp_http_client_config_t* const p_http_config = &p_http_async_info->http_client_config
                                                                   .esp_http_client_config;
        LOG_WARN(
            "### Abort HTTP %s to URL=http%s://%s:%u/%s%s%s",
            http_method_to_str(p_http_async_info),
            (HTTP_TRANSPORT_OVER_SSL == p_http_config->transport_type) ? "s" : "",
            p_http_config->host,
            p_http_config->port,
            (NULL != p_http_config->path)
                ? p_http_config->path
                : ((NULL != p_http_config->cb_path_stream_reader) ? "[path from stream reader]" : ""),
            ((NULL != p_http_config->query) || (NULL != p_http_config->cb_query_stream_reader)) ? "?" : "",
            (NULL != p_http_config->query)
                ? p_http_config->query
                : ((NULL != p_http_config->cb_query_stream_reader) ? "[query from stream reader]" : ""));

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
        esp_http_client_cleanup(p_http_async_info->p_http_client_handle);
        p_http_async_info->p_http_client_handle = NULL;
        http_async_info_free_data(p_http_async_info);
    }
    LOG_DBG("os_sema_signal: p_http_async_sema");
    os_sema_signal(p_http_async_info->p_http_async_sema);
}
