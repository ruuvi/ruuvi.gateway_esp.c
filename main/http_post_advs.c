/**
 * @file http_post_advs.c
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
#include "gw_cfg_default.h"
#include "reset_task.h"
#include "ruuvi_gateway.h"
#include "adv_post_async_comm.h"

#define LOG_LOCAL_LEVEL LOG_LEVEL_INFO
#include "log.h"
static const char TAG[] = "http";
#if (LOG_LOCAL_LEVEL >= LOG_LEVEL_DEBUG)
#warning Debug log level prints out the passwords as a "plaintext".
#endif

typedef struct http_send_advs_internal_params_t
{
    const uint32_t nonce;
    const bool     flag_use_timestamps;
    const bool     flag_post_to_ruuvi;
    const bool     use_ssl_client_cert;
    const bool     use_ssl_server_cert;
} http_send_advs_internal_params_t;

static bool
http_init_client_config_for_http_target(
    http_client_config_t* const                   p_http_client_config,
    const ruuvi_gw_cfg_http_t* const              p_cfg_http,
    const http_send_advs_internal_params_t* const p_params,
    void* const                                   p_user_data)
{
    const ruuvi_gw_cfg_http_user_t*     p_http_user = NULL;
    const ruuvi_gw_cfg_http_password_t* p_http_pass = NULL;
    if ((!p_params->flag_post_to_ruuvi) && (GW_CFG_HTTP_AUTH_TYPE_BASIC == p_cfg_http->auth_type))
    {
        p_http_user = &p_cfg_http->auth.auth_basic.user;
        p_http_pass = &p_cfg_http->auth.auth_basic.password;
    }

    ruuvi_gw_cfg_http_url_t* p_http_url = os_malloc(sizeof(*p_http_url));
    if (NULL == p_http_url)
    {
        LOG_ERR("Can't allocate memory");
        return false;
    }
    (void)snprintf(
        p_http_url->buf,
        sizeof(p_http_url->buf),
        "%s",
        p_params->flag_post_to_ruuvi ? RUUVI_GATEWAY_HTTP_DEFAULT_URL : p_cfg_http->http_url.buf);

    str_buf_t str_buf_server_cert_http = str_buf_init_null();
    str_buf_t str_buf_client_cert      = str_buf_init_null();
    str_buf_t str_buf_client_key       = str_buf_init_null();
    if (p_params->use_ssl_client_cert)
    {
        str_buf_client_cert = gw_cfg_storage_read_file(GW_CFG_STORAGE_SSL_HTTP_CLI_CERT);
        str_buf_client_key  = gw_cfg_storage_read_file(GW_CFG_STORAGE_SSL_HTTP_CLI_KEY);
    }
    if (p_params->use_ssl_server_cert)
    {
        str_buf_server_cert_http = gw_cfg_storage_read_file(GW_CFG_STORAGE_SSL_HTTP_SRV_CERT);
    }
    const http_init_client_config_params_t http_cli_cfg_params = {
        .p_url               = p_http_url,
        .p_user              = p_http_user,
        .p_password          = p_http_pass,
        .p_server_cert       = str_buf_server_cert_http.buf,
        .p_client_cert       = str_buf_client_cert.buf,
        .p_client_key        = str_buf_client_key.buf,
        .ssl_in_content_len  = RUUVI_POST_ADVS_TLS_IN_CONTENT_LEN,
        .ssl_out_content_len = RUUVI_POST_ADVS_TLS_OUT_CONTENT_LEN,
    };

    http_init_client_config(p_http_client_config, &http_cli_cfg_params, p_user_data);

    os_free(p_http_url);

    return true;
}

#if LOG_LOCAL_LEVEL >= LOG_LEVEL_DEBUG
static void
http_send_advs_log_auth_type(const ruuvi_gw_cfg_http_t* const p_cfg_http)
{
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
}
#endif

static bool
http_send_advs_internal(
    http_async_info_t* const                      p_http_async_info,
    const adv_report_table_t* const               p_reports,
    const ruuvi_gw_cfg_http_t* const              p_cfg_http,
    const http_send_advs_internal_params_t* const p_params,
    void* const                                   p_user_data)
{
    static uint32_t g_http_post_advs_malloc_fail_cnt = 0;

    p_http_async_info->recipient = p_params->flag_post_to_ruuvi ? HTTP_POST_RECIPIENT_ADVS1 : HTTP_POST_RECIPIENT_ADVS2;

    bool flag_raw_data = true;
    bool flag_decode   = false;
    if (!p_params->flag_post_to_ruuvi)
    {
        switch (p_cfg_http->data_format)
        {
            case GW_CFG_HTTP_DATA_FORMAT_RUUVI:
                flag_raw_data = true;
                flag_decode   = false;
                break;
            case GW_CFG_HTTP_DATA_FORMAT_RUUVI_RAW_AND_DECODED:
                flag_raw_data = true;
                flag_decode   = true;
                break;
            case GW_CFG_HTTP_DATA_FORMAT_RUUVI_DECODED:
                flag_raw_data = false;
                flag_decode   = true;
                break;
        }
    }

    p_http_async_info->use_json_stream_gen = true;
    const gw_cfg_t* p_gw_cfg               = gw_cfg_lock_ro();

    const http_json_create_stream_gen_advs_params_t params = {
        .flag_raw_data       = flag_raw_data,
        .flag_decode         = flag_decode,
        .flag_use_timestamps = p_params->flag_use_timestamps,
        .cur_time            = time(NULL),
        .flag_use_nonce      = true,
        .nonce               = p_params->nonce,
        .p_mac_addr          = gw_cfg_get_nrf52_mac_addr(),
        .p_coordinates       = &p_gw_cfg->ruuvi_cfg.coordinates,
    };
    p_http_async_info->select.p_gen = http_json_create_stream_gen_advs(p_reports, &params);
    gw_cfg_unlock_ro(&p_gw_cfg);
    if (NULL == p_http_async_info->select.p_gen)
    {
        LOG_ERR("Not enough memory to create http_json_create_stream_gen_advs");
        g_http_post_advs_malloc_fail_cnt += 1;
        if (g_http_post_advs_malloc_fail_cnt > RUUVI_MAX_LOW_HEAP_MEM_CNT)
        {
            gateway_restart("Low memory");
        }
        return false;
    }
    g_http_post_advs_malloc_fail_cnt = 0;

#if LOG_LOCAL_LEVEL >= LOG_LEVEL_DEBUG
    http_send_advs_log_auth_type(p_cfg_http);
#endif

    http_client_config_t* const p_http_cli_cfg = &p_http_async_info->http_client_config;
    if (!http_init_client_config_for_http_target(p_http_cli_cfg, p_cfg_http, p_params, p_user_data))
    {
        http_async_info_free_data(p_http_async_info);
        return false;
    }

    p_http_async_info->p_http_client_handle = esp_http_client_init(&p_http_cli_cfg->esp_http_client_config);
    if (NULL == p_http_async_info->p_http_client_handle)
    {
        LOG_ERR("HTTP POST to URL=%s: Can't init http client", p_http_cli_cfg->http_url.buf);
        http_async_info_free_data(p_http_async_info);
        return false;
    }

    if (!p_params->flag_post_to_ruuvi)
    {
        if (!http_handle_add_authorization_if_needed( // NOSONAR
                p_http_async_info->p_http_client_handle,
                p_cfg_http->auth_type,
                &p_cfg_http->auth))
        {
            http_async_info_free_data(p_http_async_info);
            return false;
        }
    }

    if (p_params->flag_post_to_ruuvi)
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

    adv_post_set_adv_post_http_action(p_params->flag_post_to_ruuvi);

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
http_post_advs(
    const adv_report_table_t* const  p_reports,
    const uint32_t                   nonce,
    const bool                       flag_use_timestamps,
    const bool                       flag_post_to_ruuvi,
    const ruuvi_gw_cfg_http_t* const p_cfg_http,
    void* const                      p_user_data)
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

    const bool use_ssl_client_cert = (!flag_post_to_ruuvi) && p_cfg_http->http_use_ssl_client_cert;
    const bool use_ssl_server_cert = (!flag_post_to_ruuvi) && p_cfg_http->http_use_ssl_server_cert;

    const http_send_advs_internal_params_t params = {
        .nonce               = nonce,
        .flag_use_timestamps = flag_use_timestamps,
        .flag_post_to_ruuvi  = flag_post_to_ruuvi,
        .use_ssl_client_cert = use_ssl_client_cert,
        .use_ssl_server_cert = use_ssl_server_cert,
    };

    if (!http_send_advs_internal(p_http_async_info, p_reports, p_cfg_http, &params, p_user_data))
    {
        LOG_DBG("os_sema_signal: p_http_async_sema");
        os_sema_signal(p_http_async_info->p_http_async_sema);
        return false;
    }
    return true;
}

static bool
http_check_post_advs_prep_auth_basic(
    ruuvi_gw_cfg_http_t* const p_cfg_http,
    const char* const          p_user,
    const char* const          p_pass)
{
    if ((NULL == p_user) || (NULL == p_pass))
    {
        return false;
    }
    if ((strlen(p_user) >= sizeof(p_cfg_http->auth.auth_basic.user.buf))
        || (strlen(p_pass) >= sizeof(p_cfg_http->auth.auth_basic.password.buf)))
    {
        return false;
    }
    (void)snprintf(p_cfg_http->auth.auth_basic.user.buf, sizeof(p_cfg_http->auth.auth_basic.user.buf), "%s", p_user);
    (void)snprintf(
        p_cfg_http->auth.auth_basic.password.buf,
        sizeof(p_cfg_http->auth.auth_basic.password.buf),
        "%s",
        p_pass);
    return true;
}

static bool
http_check_post_advs_prep_auth_bearer(ruuvi_gw_cfg_http_t* const p_cfg_http, const char* const p_token)
{
    if (NULL == p_token)
    {
        return false;
    }
    if (strlen(p_token) >= sizeof(p_cfg_http->auth.auth_bearer.token.buf))
    {
        return false;
    }
    ruuvi_gw_cfg_http_bearer_token_t* const p_bearer_token = &p_cfg_http->auth.auth_bearer.token;
    (void)snprintf(p_bearer_token->buf, sizeof(p_bearer_token->buf), "%s", p_token);
    return true;
}

static bool
http_check_post_advs_prep_auth_token(ruuvi_gw_cfg_http_t* const p_cfg_http, const char* const p_token)
{
    if (NULL == p_token)
    {
        return false;
    }
    if (strlen(p_token) >= sizeof(p_cfg_http->auth.auth_token.token.buf))
    {
        return false;
    }
    (void)snprintf(p_cfg_http->auth.auth_token.token.buf, sizeof(p_cfg_http->auth.auth_token.token.buf), "%s", p_token);
    return true;
}

static http_server_resp_t
http_check_post_advs_internal3(
    http_async_info_t* const         p_http_async_info,
    ruuvi_gw_cfg_http_t* const       p_cfg_http,
    const http_check_params_t* const p_params,
    const TimeUnitsSeconds_t         timeout_seconds)
{
    if (NULL == p_params->p_url)
    {
        return http_server_resp_err(HTTP_RESP_CODE_400);
    }
    if (strlen(p_params->p_url) >= sizeof(p_cfg_http->http_url.buf))
    {
        return http_server_resp_err(HTTP_RESP_CODE_400);
    }
    switch (p_params->auth_type)
    {
        case GW_CFG_HTTP_AUTH_TYPE_NONE:
            break;
        case GW_CFG_HTTP_AUTH_TYPE_BASIC:
            if (!http_check_post_advs_prep_auth_basic(p_cfg_http, p_params->p_user, p_params->p_pass))
            {
                return http_server_resp_err(HTTP_RESP_CODE_400);
            }
            break;
        case GW_CFG_HTTP_AUTH_TYPE_BEARER:
            if (!http_check_post_advs_prep_auth_bearer(p_cfg_http, p_params->p_pass))
            {
                return http_server_resp_err(HTTP_RESP_CODE_400);
            }
            break;
        case GW_CFG_HTTP_AUTH_TYPE_TOKEN:
            if (!http_check_post_advs_prep_auth_token(p_cfg_http, p_params->p_pass))
            {
                return http_server_resp_err(HTTP_RESP_CODE_400);
            }
            break;
    }

    (void)snprintf(p_cfg_http->http_url.buf, sizeof(p_cfg_http->http_url), "%s", p_params->p_url);
    p_cfg_http->auth_type = p_params->auth_type;

    const http_send_advs_internal_params_t params = {
        .nonce               = esp_random(),
        .flag_use_timestamps = gw_cfg_get_ntp_use(),
        .flag_post_to_ruuvi  = false,
        .use_ssl_client_cert = p_params->use_ssl_client_cert,
        .use_ssl_server_cert = p_params->use_ssl_server_cert,
    };

    if (!http_send_advs_internal(p_http_async_info, NULL, p_cfg_http, &params, &p_http_async_info->http_resp_cb_info))
    {
        LOG_ERR("http_post_advs failed");
        return http_server_resp_500();
    }

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
http_check_post_advs_internal2(
    http_async_info_t* const         p_http_async_info,
    const http_check_params_t* const p_params,
    const TimeUnitsSeconds_t         timeout_seconds)
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
        p_params,
        timeout_seconds);

    os_free(p_cfg_http);

    return resp;
}

http_server_resp_t
http_check_post_advs(const http_check_params_t* const p_params, const TimeUnitsSeconds_t timeout_seconds)
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

    const http_server_resp_t resp = http_check_post_advs_internal2(p_http_async_info, p_params, timeout_seconds);

    LOG_DBG("os_sema_signal: p_http_async_sema");
    os_sema_signal(p_http_async_info->p_http_async_sema);

    return resp;
}
