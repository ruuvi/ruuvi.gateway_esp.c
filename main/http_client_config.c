/**
 * @file http_client_config.c
 * @author TheSomeMan
 * @date 2026-05-24
 * @copyright Ruuvi Innovations Ltd, license BSD-3-Clause.
 */

#include "http_client_config.h"
#include "gw_cfg_default.h"
#include "http_stream_reader_nvs.h"
#include "http_post_event_handler.h"

#define LOG_LOCAL_LEVEL LOG_LEVEL_INFO
#include "log.h"
static const char TAG[] = "http";

#if (LOG_LOCAL_LEVEL >= LOG_LEVEL_DEBUG)
#warning Debug log level prints out the passwords as a "plaintext".
#endif

typedef struct stream_reader_info_t
{
    http_stream_reader_t const p_cb;
    void* const                p_cb_param;
    size_t const               ctx_size;
} stream_reader_info_t;

static stream_reader_info_t
stream_reader_info_init(const char* const p_filename)
{
    return (stream_reader_info_t) {
        .p_cb       = (NULL != p_filename) ? &http_stream_reader_nvs : NULL,
        .p_cb_param = (NULL != p_filename) ? (void*)p_filename : NULL, // NOSONAR
        .ctx_size   = (NULL != p_filename) ? sizeof(http_stream_reader_nvs_ctx_t) : 0,
    };
}

static void
http_client_config_init_log(
    const http_client_config_init_params_t* const p_params,
    const http_client_config_t* const             p_http_client_config)
{
    (void)p_params;
    (void)p_http_client_config;
#if LOG_LOCAL_LEVEL >= LOG_LEVEL_DEBUG
    LOG_DBG(
        "filename_extra_http_path=%s",
        (NULL != p_params->p_filename_extra_http_path) ? p_params->p_filename_extra_http_path : "<NULL>");
    LOG_DBG(
        "filename_extra_http_query=%s",
        (NULL != p_params->p_filename_extra_http_query) ? p_params->p_filename_extra_http_query : "<NULL>");
    LOG_DBG(
        "p_filename_extra_http_headers=%s",
        (NULL != p_params->p_filename_extra_http_headers) ? p_params->p_filename_extra_http_headers : "<NULL>");
    const esp_http_client_config_t* const p_http_config = &p_http_client_config->esp_http_client_config;
    LOG_DBG(
        "path=%s%s",
        (NULL != p_http_config->path) ? "/" : "",
        (NULL != p_http_config->path) ? p_http_config->path : "<NULL>");
    LOG_DBG("query=%s", (NULL != p_http_config->query) ? p_http_config->query : "<NULL>");
    LOG_DBG(
        "URL: http%s://%s:%u/%s%s%s",
        (HTTP_TRANSPORT_OVER_SSL == p_http_config->transport_type) ? "s" : "",
        (NULL != p_http_config->host) ? p_http_config->host : "<NULL>",
        p_http_config->port,
        (NULL != p_http_config->path)
            ? p_http_config->path
            : ((NULL != p_http_config->cb_path_stream_reader) ? "[path from stream reader]" : ""),
        ((NULL != p_http_config->query) || (NULL != p_http_config->cb_query_stream_reader)) ? "?" : "",
        (NULL != p_http_config->query)
            ? p_http_config->query
            : ((NULL != p_http_config->cb_query_stream_reader) ? "[query from stream reader]" : ""));
    LOG_DBG("user=%s", p_http_client_config->http_user.buf);
    LOG_DBG("pass=%s", p_http_client_config->http_pass.buf);
#endif
}

bool
http_client_config_init(
    http_client_config_t* const                   p_http_client_config,
    const http_client_config_init_params_t* const p_params,
    void* const                                   p_user_data)
{
    LOG_DBG("p_user_data=%p", p_user_data);
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

    esp_http_client_auth_type_t const auth_type = ('\0' != p_http_client_config->http_user.buf[0])
                                                      ? HTTP_AUTH_TYPE_BASIC
                                                      : HTTP_AUTH_TYPE_NONE;

    const stream_reader_info_t stream_reader_path    = stream_reader_info_init(p_params->p_filename_extra_http_path);
    const stream_reader_info_t stream_reader_query   = stream_reader_info_init(p_params->p_filename_extra_http_query);
    const stream_reader_info_t stream_reader_headers = stream_reader_info_init(p_params->p_filename_extra_http_headers);

    p_http_client_config->esp_http_client_config = (esp_http_client_config_t) {
        // clang-format off
        .url = NULL,
        .host = NULL,
        .port = 0,
        .username = &p_http_client_config->http_user.buf[0],
        .password = &p_http_client_config->http_pass.buf[0],
        .auth_type = auth_type,
        .path = NULL,
        .cb_path_stream_reader = stream_reader_path.p_cb,
        .cb_path_stream_reader_param = stream_reader_path.p_cb_param,
        .cb_path_stream_reader_ctx_size = stream_reader_path.ctx_size,
        .query = NULL,
        .cb_query_stream_reader = stream_reader_query.p_cb,
        .cb_query_stream_reader_param = stream_reader_query.p_cb_param,
        .cb_query_stream_reader_ctx_size = stream_reader_query.ctx_size,
        .cb_extra_headers_stream_reader = stream_reader_headers.p_cb,
        .cb_extra_headers_stream_reader_param = stream_reader_headers.p_cb_param,
        .cb_extra_headers_stream_reader_ctx_size = stream_reader_headers.ctx_size,
        .cert_pem = p_params->p_server_cert,
        .client_cert_pem = p_params->p_client_cert,
        .client_key_pem = p_params->p_client_key,
        .user_agent = NULL,
        .method = HTTP_METHOD_POST,
        .timeout_ms = 0,
        .disable_auto_redirect = false,
        .max_redirection_count = 0,
        .max_authorization_retries = 0,
        .event_handler = &http_post_event_handler,
        .transport_type = HTTP_TRANSPORT_UNKNOWN,
        .buffer_size = 0,
        .buffer_size_tx = 0,
        .user_data = p_user_data,
        .is_async = true,
        .use_global_ca_store = false,
        .skip_cert_common_name_check = false,
        .keep_alive_enable = false,
        .keep_alive_idle = 0,
        .keep_alive_interval = 0,
        .keep_alive_count = 0,
        .ssl_buf_cfg = p_params->ssl_buf_cfg,
        // clang-format on
    };

    if (NULL == p_params->p_url)
    {
        snprintf(
            p_http_client_config->http_url_copy.buf,
            sizeof(p_http_client_config->http_url_copy.buf),
            "%s",
            RUUVI_GATEWAY_HTTP_DEFAULT_URL);
    }
    else
    {
        p_http_client_config->http_url_copy = *p_params->p_url;
    }

    LOG_DBG("Base URL=%s", p_http_client_config->http_url_copy.buf);

    esp_http_client_config_t* const p_cli_cfg = &p_http_client_config->esp_http_client_config;
    if (!esp_http_client_config_set_from_url(p_cli_cfg, p_http_client_config->http_url_copy.buf))
    {
        LOG_ERR("esp_http_client_config_set_from_url failed for Base URL: %s", p_http_client_config->http_url_copy.buf);
        return false;
    }

    http_client_config_init_log(p_params, p_http_client_config);

    if ((NULL != p_cli_cfg->path) && (NULL != p_cli_cfg->cb_path_stream_reader))
    {
        LOG_ERR("HTTP client config error: both path and cb_path_stream_reader are set");
        return false;
    }
    if ((NULL != p_cli_cfg->query) && (NULL != p_cli_cfg->cb_query_stream_reader))
    {
        LOG_ERR("HTTP client config error: both query and cb_query_stream_reader are set");
        return false;
    }
    return true;
}
