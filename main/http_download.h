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

http_server_download_info_t
http_download_json(
    const char* const                     p_url,
    const TimeUnitsSeconds_t              timeout_seconds,
    const gw_cfg_remote_auth_type_e       auth_type,
    const ruuvi_gw_cfg_http_auth_t* const p_http_auth,
    const http_header_item_t* const       p_extra_header_item,
    const bool                            flag_free_memory);

#ifdef __cplusplus
}
#endif

#endif // RUUVI_GATEWAY_ESP_HTTP_DOWNLOAD_H
