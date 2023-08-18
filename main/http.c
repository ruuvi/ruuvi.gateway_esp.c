/**
 * @file http.c
 * @author Jukka Saari
 * @date 2019-11-27
 * @copyright Ruuvi Innovations Ltd, license BSD-3-Clause.
 */

#include "http.h"
#include <string.h>
#include <esp_task_wdt.h>
#include "cJSON.h"
#include "cjson_wrap.h"
#include "esp_http_client.h"
#include "ruuvi_gateway.h"
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
#include "snprintf_with_esp_err_desc.h"
#include "gw_cfg_default.h"
#include "esp_tls_err.h"
#include "reset_task.h"

#define LOG_LOCAL_LEVEL LOG_LEVEL_INFO
#include "log.h"
static const char TAG[] = "http";

#if (LOG_LOCAL_LEVEL >= LOG_LEVEL_DEBUG)
#warning Debug log level prints out the passwords as a "plaintext".
#endif

#define BASE_10 (10U)

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
        http_resp_cb_info_t* const p_cb_info = p_evt->user_data;
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
        http_resp_cb_info_t* const p_cb_info = p_evt->user_data;
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
http_post_event_handler(esp_http_client_event_t* p_evt) // NOSONAR
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

void
http_init_client_config(
    http_client_config_t* const                   p_http_client_config,
    const http_init_client_config_params_t* const p_params,
    void* const                                   p_user_data)
{
    LOG_DBG("p_user_data=%p", p_user_data);
    p_http_client_config->http_url = *p_params->p_url;
    if (NULL != p_params->p_user)
    {
        p_http_client_config->http_user = *p_params->p_user;
    }
    else
    {
        p_http_client_config->http_user.buf[0] = '\0';
    }
    if (NULL != p_params->p_password)
    {
        p_http_client_config->http_pass = *p_params->p_password;
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
        .cert_pem  = p_params->p_server_cert,
        .client_cert_pem             = p_params->p_client_cert,
        .client_key_pem              = p_params->p_client_key,
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
    http_resp_cb_info_t* const p_cb_info)
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
            LOG_ERR_ESP(err, "%s failed", "esp_http_client_perform");
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
        LOG_INFO("HTTP POST DATA:\n%s", p_chunk);
    }
    return true;
}

bool
http_send_async(http_async_info_t* const p_http_async_info)
{
    const esp_http_client_config_t* const p_http_config = &p_http_async_info->http_client_config.esp_http_client_config;

    LOG_INFO("### HTTP POST to URL=%s", p_http_config->url);

    if (p_http_async_info->use_json_stream_gen)
    {
        const json_stream_gen_size_t json_len = json_stream_gen_calc_size(p_http_async_info->select.p_gen);
        LOG_INFO("HTTP POST DATA len=%u", (printf_int_t)json_len);
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

void
http_async_info_free_data(http_async_info_t* const p_http_async_info)
{
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
            ATTR_FALLTHROUGH;
        case HTTP_POST_RECIPIENT_ADVS2:
            adv_post_last_successful_network_comm_timestamp_update();
            break;
    }
}

static void
http_async_poll_handle_resp_err(
    const http_async_info_t* const           p_http_async_info,
    const esp_http_client_http_status_code_t http_status)
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

static void
http_async_poll_do_actions_after_completion_advs1(const bool flag_success)
{
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
}

static void
http_async_poll_do_actions_after_completion_advs2(const bool flag_success)
{
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
http_async_poll(uint32_t* const p_malloc_fail_cnt)
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
        if (HTTP_RESP_CODE_200 == http_status)
        {
            http_async_poll_handle_resp_ok(p_http_async_info, http_status);
            flag_success = true;
        }
        else
        {
            http_async_poll_handle_resp_err(p_http_async_info, http_status);
        }
        *p_malloc_fail_cnt = 0;
    }
    else
    {
        LOG_ERR_ESP(
            err,
            "### HTTP POST to URL=%s: failed",
            p_http_async_info->http_client_config.esp_http_client_config.url);
        // If there is not enough memory in the system, the HTTPS connection may fail
        // with the error 32784 (ESP_ERR_MBEDTLS_SSL_HANDSHAKE_FAILED)
        if (esp_tls_err_is_ssl_handshake_failed(err))
        {
            *p_malloc_fail_cnt += 1;
            if (*p_malloc_fail_cnt >= RUUVI_MAX_LOW_HEAP_MEM_CNT)
            {
                gateway_restart("Low memory");
            }
        }
        else
        {
            *p_malloc_fail_cnt = 0;
        }
    }

    LOG_DBG("esp_http_client_cleanup");
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
        esp_http_client_cleanup(p_http_async_info->p_http_client_handle);
        p_http_async_info->p_http_client_handle = NULL;
        http_async_info_free_data(p_http_async_info);
    }
    LOG_DBG("os_sema_signal: p_http_async_sema");
    os_sema_signal(p_http_async_info->p_http_async_sema);
}
