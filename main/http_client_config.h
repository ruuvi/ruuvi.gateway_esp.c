/**
 * @file http_client_config.h
 * @author TheSomeMan
 * @date 2026-05-24
 * @copyright Ruuvi Innovations Ltd, license BSD-3-Clause.
 */

#ifndef RUUVI_GATEWAY_ESP_HTTP_CLIENT_CONFIG_H
#define RUUVI_GATEWAY_ESP_HTTP_CLIENT_CONFIG_H

#include <stdbool.h>
#include "esp_http_client.h"
#include "gw_cfg.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct http_client_config_t
{
    esp_http_client_config_t     esp_http_client_config;
    ruuvi_gw_cfg_http_url_t      http_url_copy;
    ruuvi_gw_cfg_http_user_t     http_user;
    ruuvi_gw_cfg_http_password_t http_pass;
} http_client_config_t;

typedef struct http_client_config_init_params_t
{
    const ruuvi_gw_cfg_http_url_t* const      p_url;
    const char* const                         p_filename_extra_http_path;
    const char* const                         p_filename_extra_http_query;
    const char* const                         p_filename_extra_http_headers;
    const ruuvi_gw_cfg_http_user_t* const     p_user;
    const ruuvi_gw_cfg_http_password_t* const p_password;
    const char* const                         p_server_cert;
    const char* const                         p_client_cert;
    const char* const                         p_client_key;
    esp_transport_ssl_buf_cfg_t               ssl_buf_cfg; /*!< SSL pre-allocated buffer configuration */
} http_client_config_init_params_t;

bool
http_client_config_init(
    http_client_config_t* const                   p_http_client_config,
    const http_client_config_init_params_t* const p_params,
    void* const                                   p_user_data);

#ifdef __cplusplus
}
#endif

#endif // RUUVI_GATEWAY_ESP_HTTP_CLIENT_CONFIG_H
