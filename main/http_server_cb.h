/**
 * @file http_server_cb.h
 * @author TheSomeMan
 * @date 2020-10-26
 * @copyright Ruuvi Innovations Ltd, license BSD-3-Clause.
 */

#ifndef RUUVI_GATEWAY_ESP_HTTP_SERVER_CB_H
#define RUUVI_GATEWAY_ESP_HTTP_SERVER_CB_H

#include "wifi_manager_defs.h"

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
http_server_cb_init(void);

void
http_server_cb_deinit(void);

http_server_resp_t
http_server_cb_on_get(const char *p_path, const http_server_resp_t *const p_resp_auth);

http_server_resp_t
http_server_cb_on_post(const char *p_file_name, const char *p_body);

http_server_resp_t
http_server_cb_on_delete(const char *p_path, const http_server_resp_t *const p_resp_auth);

#if RUUVI_TESTS_HTTP_SERVER_CB

time_t
http_server_get_cur_time(void);

http_server_resp_t
http_server_resp_json_ruuvi(void);

http_server_resp_t
http_server_resp_json(const char *p_file_name);

http_server_resp_t
http_server_resp_metrics(void);

http_content_type_e
http_get_content_type_by_ext(const char *p_file_ext);

http_server_resp_t
http_server_resp_file(const char *file_path, const http_resp_code_e http_resp_code);

http_server_resp_t
http_server_cb_on_post_ruuvi(const char *p_body);

#endif

#ifdef __cplusplus
}
#endif

#endif // RUUVI_GATEWAY_ESP_HTTP_SERVER_CB_H
