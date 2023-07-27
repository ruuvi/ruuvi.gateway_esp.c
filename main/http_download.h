/**
 * @file http_download.h
 * @author TheSomeMan
 * @date 2020-10-26
 * @copyright Ruuvi Innovations Ltd, license BSD-3-Clause.
 */

#ifndef RUUVI_GATEWAY_ESP_HTTP_DOWNLOAD_H
#define RUUVI_GATEWAY_ESP_HTTP_DOWNLOAD_H

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include "time_units.h"
#include "wifi_manager_defs.h"
#include "gw_cfg.h"
#include "http.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct http_server_download_info_t
{
    bool             is_error;
    http_resp_code_e http_resp_code;
    char*            p_json_buf;
    size_t           json_buf_size;
} http_server_download_info_t;

typedef bool (*http_download_cb_on_data_t)(
    const uint8_t* const   p_buf,
    const size_t           buf_size,
    const size_t           offset,
    const size_t           content_length,
    const http_resp_code_e resp_code,
    void*                  p_user_data);

bool
http_download_with_auth(
    const http_download_param_with_auth_t* const p_param,
    http_download_cb_on_data_t const             p_cb_on_data,
    void* const                                  p_user_data);

bool
http_check_with_auth(const http_download_param_with_auth_t* const p_param, http_resp_code_e* const p_http_resp_code);

bool
http_download(
    const http_download_param_with_auth_t* const p_param,
    http_download_cb_on_data_t const             p_cb_on_data,
    void* const                                  p_user_data);

http_server_download_info_t
http_download_json(const http_download_param_with_auth_t* const p_params);

http_server_download_info_t
http_download_latest_release_info(const bool flag_free_memory);

http_resp_code_e
http_check(const http_download_param_with_auth_t* const p_param);

#ifdef __cplusplus
}
#endif

#endif // RUUVI_GATEWAY_ESP_HTTP_DOWNLOAD_H
