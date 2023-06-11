/**
 * @file http_server_cb.h
 * @author TheSomeMan
 * @date 2020-10-26
 * @copyright Ruuvi Innovations Ltd, license BSD-3-Clause.
 */

#ifndef RUUVI_GATEWAY_ESP_HTTP_SERVER_CB_H
#define RUUVI_GATEWAY_ESP_HTTP_SERVER_CB_H

#include "wifi_manager_defs.h"
#include "http_download.h"

#if !defined(RUUVI_TESTS_HTTP_SERVER_CB)
#define RUUVI_TESTS_HTTP_SERVER_CB (0)
#endif

#if RUUVI_TESTS_HTTP_SERVER_CB
#include <time.h>
#endif

#if RUUVI_TESTS_HTTP_SERVER_CB
#define HTTP_SERVER_CB_STATIC
#else
#define HTTP_SERVER_CB_STATIC static
#endif

#ifdef __cplusplus
extern "C" {
#endif

bool
http_server_cb_init(const char* const p_fatfs_gwui_partition_name);

void
http_server_cb_deinit(void);

time_t
http_server_get_cur_time(void);

http_resp_code_e
http_server_gw_cfg_download_and_parse(
    const ruuvi_gw_cfg_remote_t* const p_remote_cfg,
    const bool                         flag_free_memory,
    gw_cfg_t**                         p_p_gw_cfg_tmp,
    str_buf_t* const                   p_err_msg);

http_resp_code_e
http_server_gw_cfg_download_and_update(
    bool* const      p_flag_reboot_needed,
    const bool       flag_free_memory,
    str_buf_t* const p_err_msg);

void
http_server_cb_on_user_req(const http_server_user_req_code_e req_code);

http_server_resp_t
http_server_cb_on_get(
    const char* const               p_path,
    const char* const               p_uri_params,
    const bool                      flag_access_from_lan,
    const http_server_resp_t* const p_resp_auth);

http_server_resp_t
http_server_cb_on_post(
    const char* const p_file_name,
    const char* const p_uri_params,
    const char* const p_body,
    const bool        flag_access_from_lan);

http_server_resp_t
http_server_cb_on_delete(
    const char* const               p_path,
    const char* const               p_uri_params,
    const bool                      flag_access_from_lan,
    const http_server_resp_t* const p_resp_auth);

void
http_server_cb_prohibit_cfg_updating(void);

void
http_server_cb_allow_cfg_updating(void);

ATTR_PRINTF(2, 3)
ATTR_NONNULL(2)
http_server_resp_t
http_server_cb_gen_resp(const http_resp_code_e resp_code, const char* const p_fmt, ...);

str_buf_t
http_server_get_from_params_with_decoding(const char* const p_params, const char* const p_key);

str_buf_t
http_server_get_from_params(const char* const p_params, const char* const p_key);

#if RUUVI_TESTS_HTTP_SERVER_CB

time_t
http_server_get_cur_time(void);

http_server_resp_t
http_server_resp_json_ruuvi(void);

http_server_resp_t
http_server_resp_json(const char* p_file_name, const bool flag_access_from_lan);

http_server_resp_t
http_server_resp_metrics(void);

http_content_type_e
http_get_content_type_by_ext(const char* p_file_ext);

http_server_resp_t
http_server_resp_file(const char* file_path, const http_resp_code_e http_resp_code);

HTTP_SERVER_CB_STATIC
http_server_resp_t
http_server_cb_on_post_ruuvi(const char* p_body, const bool flag_access_from_lan);

#endif

#ifdef __cplusplus
}
#endif

#endif // RUUVI_GATEWAY_ESP_HTTP_SERVER_CB_H
