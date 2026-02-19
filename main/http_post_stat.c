/**
 * @file http_post_stat.c
 * @author TheSomeMan
 * @date 2023-07-31
 * @copyright Ruuvi Innovations Ltd, license BSD-3-Clause.
 */

#include "http.h"
#include <string.h>
#include <esp_task_wdt.h>
#include "os_malloc.h"
#include "gw_status.h"
#include "http_server_cb.h"
#include "http_server_resp.h"
#include "adv_post.h"
#include "gw_cfg_storage.h"
#include "adv_post_statistics.h"
#include "ruuvi_gateway.h"

#define LOG_LOCAL_LEVEL LOG_LEVEL_INFO
#include "log.h"
static const char TAG[] = "http";
#if (LOG_LOCAL_LEVEL >= LOG_LEVEL_DEBUG)
#warning Debug log level prints out the passwords as a "plaintext".
#endif

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

    const http_init_client_config_params_t http_cli_cfg_params = {
        .p_url         = &p_cfg_http_stat->http_stat_url,
        .p_user        = &p_cfg_http_stat->http_stat_user,
        .p_password    = &p_cfg_http_stat->http_stat_pass,
        .p_server_cert = str_buf_server_cert_stat.buf,
        .p_client_cert = str_buf_client_cert.buf,
        .p_client_key  = str_buf_client_key.buf,
#if defined(CONFIG_MBEDTLS_SSL_VARIABLE_BUFFER_LENGTH)
        .p_ssl_in_buf  = NULL,
        .p_ssl_out_buf = NULL,
#else
        .p_ssl_in_buf  = p_http_async_info->in_buf,
        .p_ssl_out_buf = p_http_async_info->out_buf,
#endif
#if defined(CONFIG_MBEDTLS_SSL_VARIABLE_BUFFER_LENGTH)
        .ssl_in_content_len  = RUUVI_POST_STAT_TLS_IN_CONTENT_LEN,
        .ssl_out_content_len = RUUVI_POST_STAT_TLS_OUT_CONTENT_LEN,
#endif
    };

    http_init_client_config(&p_http_async_info->http_client_config, &http_cli_cfg_params, p_user_data);

    p_http_async_info->p_http_client_handle = esp_http_client_init(
        &p_http_async_info->http_client_config.esp_http_client_config);
    if (NULL == p_http_async_info->p_http_client_handle)
    {
        LOG_ERR("HTTP POST to URL=%s: Can't init http client", p_http_async_info->http_client_config.http_url.buf);
        http_async_info_free_data(p_http_async_info);
        return false;
    }

    (void)hmac_sha256_calc_for_stats(p_http_async_info->select.cjson_str.p_str, &p_http_async_info->hmac_sha256);

    if (!http_send_async(p_http_async_info))
    {
        LOG_DBG("esp_http_client_cleanup");
        esp_http_client_cleanup(p_http_async_info->p_http_client_handle);
        p_http_async_info->p_http_client_handle = NULL;
        http_async_info_free_data(p_http_async_info);
        return false;
    }
    return true;
}

bool
http_post_stat(
    const http_json_statistics_info_t* const p_stat_info,
    const adv_report_table_t* const          p_reports,
    const ruuvi_gw_cfg_http_stat_t* const    p_cfg_http_stat,
    void* const                              p_user_data,
    const bool                               use_ssl_client_cert,
    const bool                               use_ssl_server_cert)
{
    http_async_info_t* p_http_async_info = http_get_async_info();
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

static bool
http_post_stat_check_user(const char* const p_user)
{
    if (NULL == p_user)
    {
        return false;
    }
    if (strlen(p_user) >= GW_CFG_MAX_HTTP_USER_LEN)
    {
        return false;
    }
    return true;
}

static bool
http_post_stat_check_pass(const char* const p_pass)
{
    if (NULL == p_pass)
    {
        return false;
    }
    if (strlen(p_pass) >= GW_CFG_MAX_HTTP_PASS_LEN)
    {
        return false;
    }
    return true;
}

static http_server_resp_t
http_check_post_stat_internal3(
    http_async_info_t* const         p_http_async_info,
    ruuvi_gw_cfg_http_stat_t* const  p_cfg_http_stat,
    const http_check_params_t* const p_params,
    const TimeUnitsSeconds_t         timeout_seconds)
{
    if (NULL == p_params->p_url)
    {
        return http_server_resp_err(HTTP_RESP_CODE_400);
    }
    if (strlen(p_params->p_url) >= sizeof(p_cfg_http_stat->http_stat_url.buf))
    {
        return http_server_resp_err(HTTP_RESP_CODE_400);
    }
    (void)snprintf(p_cfg_http_stat->http_stat_url.buf, sizeof(p_cfg_http_stat->http_stat_url), "%s", p_params->p_url);
    ruuvi_gw_cfg_http_user_t* const     p_user = &p_cfg_http_stat->http_stat_user;
    ruuvi_gw_cfg_http_password_t* const p_pass = &p_cfg_http_stat->http_stat_pass;
    switch (p_params->auth_type)
    {
        case GW_CFG_HTTP_AUTH_TYPE_NONE:
            p_user->buf[0] = '\0';
            p_pass->buf[0] = '\0';
            break;
        case GW_CFG_HTTP_AUTH_TYPE_BASIC:
            if ((!http_post_stat_check_user(p_params->p_user)) || (!http_post_stat_check_pass(p_params->p_pass)))
            {
                return http_server_resp_err(HTTP_RESP_CODE_400);
            }
            (void)snprintf(p_user->buf, sizeof(p_user->buf), "%s", p_params->p_user);
            (void)snprintf(p_pass->buf, sizeof(p_pass->buf), "%s", p_params->p_pass);
            break;
        case GW_CFG_HTTP_AUTH_TYPE_BEARER:
            ATTR_FALLTHROUGH;
        case GW_CFG_HTTP_AUTH_TYPE_TOKEN:
            ATTR_FALLTHROUGH;
        case GW_CFG_HTTP_AUTH_TYPE_APIKEY:
            p_user->buf[0] = '\0';
            if (!http_post_stat_check_pass(p_params->p_pass))
            {
                return http_server_resp_err(HTTP_RESP_CODE_400);
            }
            (void)snprintf(p_pass->buf, sizeof(p_pass->buf), "%s", p_params->p_pass);
            break;
    }

    const http_json_statistics_info_t* p_stat_info = adv_post_statistics_info_generate(NULL);
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
            &p_http_async_info->http_resp_cb_info,
            p_params->use_ssl_client_cert,
            p_params->use_ssl_server_cert))
    {
        os_free(p_stat_info);
        LOG_ERR("http_post_stat failed");
        return http_server_resp_500();
    }
    os_free(p_stat_info);

    const bool         flag_feed_task_watchdog = true;
    http_server_resp_t server_resp             = http_wait_until_async_req_completed(
        p_http_async_info->p_http_client_handle,
        &p_http_async_info->http_resp_cb_info,
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
    esp_http_client_cleanup(p_http_async_info->p_http_client_handle);
    p_http_async_info->p_http_client_handle = NULL;
    http_async_info_free_data(p_http_async_info);

    return resp;
}

static http_server_resp_t
http_check_post_stat_internal2(
    http_async_info_t* const         p_http_async_info,
    const http_check_params_t* const p_params,
    const TimeUnitsSeconds_t         timeout_seconds)
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
        p_params,
        timeout_seconds);

    os_free(p_cfg_http_stat);

    return resp;
}

http_server_resp_t
http_check_post_stat(const http_check_params_t* const p_params, const TimeUnitsSeconds_t timeout_seconds)
{
    http_async_info_t* const p_http_async_info = http_get_async_info();
    LOG_DBG("os_sema_wait_immediate: p_http_async_sema");
    if (!os_sema_wait_immediate(p_http_async_info->p_http_async_sema))
    {
        /* Because the amount of memory is limited and may not be sufficient for two simultaneous queries,
         * this function should only be called from a single thread. */
        LOG_ERR("This function is called twice from two different threads, which can lead to memory shortages");
        assert(0);
    }
    p_http_async_info->p_task = xTaskGetCurrentTaskHandle();

    const http_server_resp_t resp = http_check_post_stat_internal2(p_http_async_info, p_params, timeout_seconds);

    LOG_DBG("os_sema_signal: p_http_async_sema");
    os_sema_signal(p_http_async_info->p_http_async_sema);

    return resp;
}
