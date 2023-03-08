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
    void* const                p_user_data;
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

typedef struct http_async_info_t
{
    os_sema_t                p_http_async_sema;
    os_sema_static_t         http_async_sema_mem;
    http_client_config_t     http_client_config;
    esp_http_client_handle_t p_http_client_handle;
    cjson_wrap_str_t         cjson_str;
    bool                     flag_sending_advs;
    os_task_handle_t         p_task;
    http_post_cb_info_t      http_post_cb_info;
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
        if (!hmac_sha256_set_key_str(p_evt->header_value))
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
        adv_post_set_period(period_seconds * TIME_UNITS_MS_PER_SECOND);
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
    void* const                               p_user_data)
{
    LOG_DBG("p_user_data=%p", p_user_data);
    p_http_client_config->http_url  = *p_url;
    p_http_client_config->http_user = *p_user;
    p_http_client_config->http_pass = *p_password;
    LOG_DBG("URL=%s", p_http_client_config->http_url.buf);
    LOG_DBG("user=%s", p_http_client_config->http_user.buf);
    LOG_DBG("pass=%s", p_http_client_config->http_pass.buf);

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
    return http_server_resp_json_in_heap(http_resp_code, p_cb_info->p_buf);
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
            return http_server_resp_err(HTTP_RESP_CODE_502);
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
    return http_server_resp_err(HTTP_RESP_CODE_502);
}

static bool
http_send_async(http_async_info_t* const p_http_async_info)
{
    const esp_http_client_config_t* const p_http_config = &p_http_async_info->http_client_config.esp_http_client_config;

    const char* const p_msg = p_http_async_info->cjson_str.p_str;
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
        LOG_DBG("esp_http_client_cleanup");
        esp_http_client_cleanup(p_http_async_info->p_http_client_handle);
        p_http_async_info->p_http_client_handle = NULL;
        return false;
    }
    return true;
}

bool
http_send_advs_internal(
    http_async_info_t* const         p_http_async_info,
    const adv_report_table_t* const  p_reports,
    const uint32_t                   nonce,
    const bool                       flag_use_timestamps,
    const ruuvi_gw_cfg_http_t* const p_cfg_http,
    void* const                      p_user_data)
{
    const ruuvi_gw_cfg_coordinates_t coordinates = gw_cfg_get_coordinates();

    p_http_async_info->flag_sending_advs = true;
    p_http_async_info->cjson_str         = cjson_wrap_str_null();
    if (!http_json_create_records_str(
            p_reports,
            (http_json_header_info_t) {
                .flag_use_timestamps = flag_use_timestamps,
                .timestamp           = time(NULL),
                .p_mac_addr          = gw_cfg_get_nrf52_mac_addr(),
                .p_coordinates_str   = coordinates.buf,
                .flag_use_nonce      = true,
                .nonce               = nonce,
            },
            &p_http_async_info->cjson_str))
    {
        LOG_ERR("Not enough memory to generate json");
        return false;
    }

    LOG_DBG(
        "http_init_client_config: URL=%s, user=%s, pass=%s",
        p_cfg_http->http_url.buf,
        p_cfg_http->http_user.buf,
        p_cfg_http->http_pass.buf);
    http_init_client_config(
        &p_http_async_info->http_client_config,
        &p_cfg_http->http_url,
        &p_cfg_http->http_user,
        &p_cfg_http->http_pass,
        p_user_data);

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
        LOG_DBG("esp_http_client_cleanup");
        esp_http_client_cleanup(p_http_async_info->p_http_client_handle);
        p_http_async_info->p_http_client_handle = NULL;
        cjson_wrap_free_json_str(&p_http_async_info->cjson_str);
        return false;
    }
    return true;
}

bool
http_send_advs(
    const adv_report_table_t* const  p_reports,
    const uint32_t                   nonce,
    const bool                       flag_use_timestamps,
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

    if (!http_send_advs_internal(p_http_async_info, p_reports, nonce, flag_use_timestamps, p_cfg_http, p_user_data))
    {
        LOG_DBG("os_sema_signal: p_http_async_sema");
        os_sema_signal(p_http_async_info->p_http_async_sema);
        return false;
    }
    return true;
}

static http_server_resp_t
http_check_post_advs_internal3(
    http_async_info_t* const   p_http_async_info,
    ruuvi_gw_cfg_http_t* const p_cfg_http,
    const char* const          p_url,
    const char* const          p_user,
    const char* const          p_pass,
    const TimeUnitsSeconds_t   timeout_seconds)
{
    if ((strlen(p_url) >= sizeof(p_cfg_http->http_url.buf)) || (strlen(p_user) >= sizeof(p_cfg_http->http_user.buf))
        || (strlen(p_pass) >= sizeof(p_cfg_http->http_pass.buf)))
    {
        return http_server_resp_err(HTTP_RESP_CODE_400);
    }
    (void)snprintf(p_cfg_http->http_url.buf, sizeof(p_cfg_http->http_url), "%s", p_url);
    (void)snprintf(p_cfg_http->http_user.buf, sizeof(p_cfg_http->http_user), "%s", p_user);
    (void)snprintf(p_cfg_http->http_pass.buf, sizeof(p_cfg_http->http_pass), "%s", p_pass);

    if (!http_send_advs_internal(
            p_http_async_info,
            NULL,
            esp_random(),
            gw_cfg_get_ntp_use(),
            p_cfg_http,
            &p_http_async_info->http_post_cb_info))
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

    const http_server_resp_t resp = http_server_cb_gen_resp(http_resp_code, "%s", "");
    if ((HTTP_CONTENT_LOCATION_HEAP == server_resp.content_location)
        && (NULL != server_resp.select_location.memory.p_buf))
    {
        os_free(server_resp.select_location.memory.p_buf);
    }

    LOG_DBG("esp_http_client_cleanup");
    esp_http_client_cleanup(p_http_async_info->p_http_client_handle);
    p_http_async_info->p_http_client_handle = NULL;
    cjson_wrap_free_json_str(&p_http_async_info->cjson_str);

    return resp;
}

static http_server_resp_t
http_check_post_advs_internal2(
    http_async_info_t* const p_http_async_info,
    const char* const        p_url,
    const char* const        p_user,
    const char* const        p_pass,
    const TimeUnitsSeconds_t timeout_seconds)
{
    ruuvi_gw_cfg_http_t* p_cfg_http = os_malloc(sizeof(*p_cfg_http));
    if (NULL == p_cfg_http)
    {
        LOG_ERR("Can't allocate memory for ruuvi_gw_cfg_http_t");
        return http_server_resp_500();
    }

    const http_server_resp_t resp
        = http_check_post_advs_internal3(p_http_async_info, p_cfg_http, p_url, p_user, p_pass, timeout_seconds);

    os_free(p_cfg_http);

    return resp;
}

http_server_resp_t
http_check_post_advs(
    const char* const        p_url,
    const char* const        p_user,
    const char* const        p_pass,
    const TimeUnitsSeconds_t timeout_seconds)
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
        p_user,
        p_pass,
        timeout_seconds);

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
    void* const                              p_user_data)
{
    p_http_async_info->flag_sending_advs = false;
    p_http_async_info->cjson_str         = cjson_wrap_str_null();
    if (!http_json_create_status_str(p_stat_info, p_reports, &p_http_async_info->cjson_str))
    {
        LOG_ERR("Not enough memory to generate status json");
        return false;
    }

    p_http_async_info->http_client_config.http_url  = p_cfg_http_stat->http_stat_url;
    p_http_async_info->http_client_config.http_user = p_cfg_http_stat->http_stat_user;
    p_http_async_info->http_client_config.http_pass = p_cfg_http_stat->http_stat_pass;

    http_init_client_config(
        &p_http_async_info->http_client_config,
        &p_cfg_http_stat->http_stat_url,
        &p_cfg_http_stat->http_stat_user,
        &p_cfg_http_stat->http_stat_pass,
        p_user_data);

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
        LOG_DBG("esp_http_client_cleanup");
        esp_http_client_cleanup(p_http_async_info->p_http_client_handle);
        p_http_async_info->p_http_client_handle = NULL;
        cjson_wrap_free_json_str(&p_http_async_info->cjson_str);
        return false;
    }
    return true;
}

bool
http_send_statistics(
    const http_json_statistics_info_t* const p_stat_info,
    const adv_report_table_t* const          p_reports,
    const ruuvi_gw_cfg_http_stat_t* const    p_cfg_http_stat,
    void* const                              p_user_data)
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

    if (!http_send_statistics_internal(p_http_async_info, p_stat_info, p_reports, p_cfg_http_stat, p_user_data))
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
    const TimeUnitsSeconds_t        timeout_seconds)
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

    const http_json_statistics_info_t stat_info = adv_post_generate_statistics_info(NULL);

    if (!http_send_statistics_internal(
            p_http_async_info,
            &stat_info,
            NULL,
            p_cfg_http_stat,
            &p_http_async_info->http_post_cb_info))
    {
        LOG_ERR("http_send_statistics failed");
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

    const http_server_resp_t resp = http_server_cb_gen_resp(http_resp_code, "%s", "");
    if ((HTTP_CONTENT_LOCATION_HEAP == server_resp.content_location)
        && (NULL != server_resp.select_location.memory.p_buf))
    {
        os_free(server_resp.select_location.memory.p_buf);
    }

    LOG_DBG("esp_http_client_cleanup");
    esp_http_client_cleanup(p_http_async_info->p_http_client_handle);
    p_http_async_info->p_http_client_handle = NULL;
    cjson_wrap_free_json_str(&p_http_async_info->cjson_str);

    return resp;
}

static http_server_resp_t
http_check_post_stat_internal2(
    http_async_info_t* const p_http_async_info,
    const char* const        p_url,
    const char* const        p_user,
    const char* const        p_pass,
    const TimeUnitsSeconds_t timeout_seconds)
{
    ruuvi_gw_cfg_http_stat_t* p_cfg_http_stat = os_malloc(sizeof(*p_cfg_http_stat));
    if (NULL == p_cfg_http_stat)
    {
        LOG_ERR("Can't allocate memory for ruuvi_gw_cfg_http_t");
        return http_server_resp_500();
    }

    const http_server_resp_t resp
        = http_check_post_stat_internal3(p_http_async_info, p_cfg_http_stat, p_url, p_user, p_pass, timeout_seconds);

    os_free(p_cfg_http_stat);

    return resp;
}

http_server_resp_t
http_check_post_stat(
    const char* const        p_url,
    const char* const        p_user,
    const char* const        p_pass,
    const TimeUnitsSeconds_t timeout_seconds)
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
        timeout_seconds);

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

    const bool flag_is_mqtt_connected  = gw_status_is_mqtt_connected();
    const bool flag_is_mqtt_error      = gw_status_is_mqtt_error();
    const bool flag_is_mqtt_auth_error = gw_status_is_mqtt_auth_error();

    LOG_DBG("mqtt_app_stop");
    mqtt_app_stop();

    http_resp_code_e http_resp_code = HTTP_RESP_CODE_200;
    if ((!flag_is_mqtt_connected) || flag_is_mqtt_error)
    {
        if (flag_is_mqtt_auth_error)
        {
            http_resp_code = HTTP_RESP_CODE_401;
        }
        else
        {
            http_resp_code = HTTP_RESP_CODE_404;
        }
    }

    return http_server_cb_gen_resp(http_resp_code, "%s", "");
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
            if (p_http_async_info->flag_sending_advs)
            {
                adv_post_last_successful_network_comm_timestamp_update();
            }
            else
            {
                reset_info_clear_extra_info();
            }
            flag_success = true;
        }
        else
        {
            LOG_ERR(
                "### HTTP POST to URL=%s: STATUS=%d",
                p_http_async_info->http_client_config.esp_http_client_config.url,
                http_status);
            if ((HTTP_RESP_CODE_429 == http_status) && (p_http_async_info->flag_sending_advs))
            {
                adv_post_last_successful_network_comm_timestamp_update();
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
    esp_http_client_cleanup(p_http_async_info->p_http_client_handle);
    p_http_async_info->p_http_client_handle = NULL;

    cjson_wrap_free_json_str(&p_http_async_info->cjson_str);
    p_http_async_info->cjson_str = cjson_wrap_str_null();

    if (p_http_async_info->flag_sending_advs)
    {
        if (flag_success)
        {
            if (!fw_update_mark_app_valid_cancel_rollback())
            {
                LOG_ERR("%s failed", "fw_update_mark_app_valid_cancel_rollback");
            }
            leds_notify_http1_data_sent_successfully();
        }
        else
        {
            leds_notify_http1_data_sent_fail();
        }
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

static bool
http_download_by_handle(
    esp_http_client_handle_t http_handle,
    const bool               flag_feed_task_watchdog,
    const TimeUnitsSeconds_t timeout_seconds,
    http_resp_code_e* const  p_http_resp_code)
{
    esp_err_t err = esp_http_client_set_header(http_handle, "Accept", "text/html,application/octet-stream,*/*");
    if (ESP_OK != err)
    {
        LOG_ERR("%s failed", "esp_http_client_set_header");
        *p_http_resp_code = HTTP_RESP_CODE_500;
        return false;
    }
    err = esp_http_client_set_header(http_handle, "User-Agent", "RuuviGateway");
    if (ESP_OK != err)
    {
        LOG_ERR("%s failed", "esp_http_client_set_header");
        *p_http_resp_code = HTTP_RESP_CODE_500;
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
        *p_http_resp_code = esp_http_client_get_status_code(http_handle);
        return true;
    }
    if (ESP_ERR_HTTP_EAGAIN != err)
    {
        LOG_ERR_ESP(
            err,
            "esp_http_client_perform failed, HTTP resp code %d (http_handle=%ld)",
            (printf_int_t)esp_http_client_get_status_code(http_handle),
            (printf_long_t)http_handle);
        *p_http_resp_code = HTTP_RESP_CODE_502;
        return false;
    }

    const http_server_resp_t resp = http_wait_until_async_req_completed(
        http_handle,
        NULL,
        flag_feed_task_watchdog,
        timeout_seconds);

    *p_http_resp_code = resp.http_resp_code;
    if (HTTP_RESP_CODE_200 == resp.http_resp_code)
    {
        return true;
    }
    return false;
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

bool
http_download_with_auth(
    const http_download_param_t           param,
    const gw_cfg_remote_auth_type_e       gw_cfg_http_auth_type,
    const ruuvi_gw_cfg_http_auth_t* const p_http_auth,
    const http_header_item_t* const       p_extra_header_item)
{
    http_download_cb_info_t cb_info = {
        .cb_on_data              = param.p_cb_on_data,
        .p_user_data             = param.p_user_data,
        .http_handle             = NULL,
        .content_length          = 0,
        .offset                  = 0,
        .flag_feed_task_watchdog = param.flag_feed_task_watchdog,
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
        .url  = param.p_url,
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
        .timeout_ms                  = (int)(param.timeout_seconds * TIME_UNITS_MS_PER_SECOND),
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
    LOG_INFO("http_download: URL: %s", param.p_url);
    if (HTTP_AUTH_TYPE_BASIC == http_client_auth_type)
    {
        LOG_INFO("http_download: Auth: Basic, Username: %s", p_http_auth->auth_basic.user.buf);
        LOG_DBG("http_download: Auth: Basic, Password: %s", p_http_auth->auth_basic.password.buf);
    }
    if (!gw_status_is_network_connected())
    {
        LOG_ERR("HTTP download failed - no network connection");
        return false;
    }

    suspend_relaying_and_wait(param.flag_free_memory);

    cb_info.http_handle = esp_http_client_init(&http_config);
    if (NULL == cb_info.http_handle)
    {
        LOG_ERR("Can't init http client");
        resume_relaying_and_wait(param.flag_free_memory);
        return false;
    }

    if (GW_CFG_REMOTE_AUTH_TYPE_BEARER == gw_cfg_http_auth_type)
    {
        str_buf_t str_buf = str_buf_printf_with_alloc("Bearer %s", p_http_auth->auth_bearer.token.buf);
        if (NULL == str_buf.buf)
        {
            LOG_ERR("Can't allocate memory for bearer token");
            resume_relaying_and_wait(param.flag_free_memory);
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
    http_resp_code_e http_resp_code = HTTP_RESP_CODE_200;

    const bool result = http_download_by_handle(
        cb_info.http_handle,
        param.flag_feed_task_watchdog,
        param.timeout_seconds,
        &http_resp_code);

    LOG_DBG("esp_http_client_cleanup");
    const esp_err_t err = esp_http_client_cleanup(cb_info.http_handle);
    if (ESP_OK != err)
    {
        LOG_ERR_ESP(err, "esp_http_client_cleanup failed");
    }

    resume_relaying_and_wait(param.flag_free_memory);

    return result;
}

bool
http_check_with_auth(
    const http_check_param_t              param,
    const gw_cfg_remote_auth_type_e       gw_cfg_http_auth_type,
    const ruuvi_gw_cfg_http_auth_t* const p_http_auth,
    const http_header_item_t* const       p_extra_header_item,
    http_resp_code_e* const               p_http_resp_code)
{
    http_check_cb_info_t cb_info = {
        .http_handle             = NULL,
        .content_length          = 0,
        .flag_feed_task_watchdog = param.flag_feed_task_watchdog,
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
        .url  = param.p_url,
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
        .method                      = HTTP_METHOD_HEAD,
        .timeout_ms                  = (int)(param.timeout_seconds * TIME_UNITS_MS_PER_SECOND),
        .disable_auto_redirect       = false,
        .max_redirection_count       = 0,
        .max_authorization_retries   = 0,
        .event_handler               = &http_check_event_handler,
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
    LOG_INFO("http_check: URL: %s", param.p_url);
    if (HTTP_AUTH_TYPE_BASIC == http_client_auth_type)
    {
        LOG_INFO("http_check: Auth: Basic, Username: %s", p_http_auth->auth_basic.user.buf);
    }
    if (!gw_status_is_network_connected())
    {
        LOG_ERR("HTTP check failed - no network connection");
        return false;
    }

    suspend_relaying_and_wait(param.flag_free_memory);

    cb_info.http_handle = esp_http_client_init(&http_config);
    if (NULL == cb_info.http_handle)
    {
        LOG_ERR("Can't init http client");
        resume_relaying_and_wait(param.flag_free_memory);
        return false;
    }

    if (GW_CFG_REMOTE_AUTH_TYPE_BEARER == gw_cfg_http_auth_type)
    {
        str_buf_t str_buf = str_buf_printf_with_alloc("Bearer %s", p_http_auth->auth_bearer.token.buf);
        if (NULL == str_buf.buf)
        {
            LOG_ERR("Can't allocate memory for bearer token");
            resume_relaying_and_wait(param.flag_free_memory);
            return false;
        }
        LOG_INFO(
            "http_check: Add HTTP header: %s: %s",
            "Authorization",
            (LOG_LOCAL_LEVEL >= LOG_LEVEL_DEBUG) ? str_buf.buf : "********");
        esp_http_client_set_header(cb_info.http_handle, "Authorization", str_buf.buf);
        str_buf_free_buf(&str_buf);
    }
    if (NULL != p_extra_header_item)
    {
        LOG_INFO("http_check: Add HTTP header: %s: %s", p_extra_header_item->p_key, p_extra_header_item->p_value);
        esp_http_client_set_header(cb_info.http_handle, p_extra_header_item->p_key, p_extra_header_item->p_value);
    }

    LOG_DBG("http_check_by_handle");
    const bool result = http_download_by_handle(
        cb_info.http_handle,
        param.flag_feed_task_watchdog,
        param.timeout_seconds,
        p_http_resp_code);

    LOG_DBG("esp_http_client_cleanup");
    const esp_err_t err = esp_http_client_cleanup(cb_info.http_handle);
    if (ESP_OK != err)
    {
        LOG_ERR_ESP(err, "esp_http_client_cleanup failed");
    }

    resume_relaying_and_wait(param.flag_free_memory);

    return result;
}

bool
http_download(const http_download_param_t param)
{
    return http_download_with_auth(param, GW_CFG_REMOTE_AUTH_TYPE_NO, NULL, NULL);
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
        LOG_INFO(
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
        cjson_wrap_free_json_str(&p_http_async_info->cjson_str);
    }
    LOG_DBG("os_sema_signal: p_http_async_sema");
    os_sema_signal(p_http_async_info->p_http_async_sema);
}
