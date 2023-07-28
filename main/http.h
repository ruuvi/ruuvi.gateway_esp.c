/**
 * @file http.h
 * @author Jukka Saari
 * @date 2019-11-27
 * @copyright Ruuvi Innovations Ltd, license BSD-3-Clause.
 */

#ifndef RUUVI_HTTP_H
#define RUUVI_HTTP_H

#include <stdbool.h>
#include "esp_http_client.h"
#include "adv_table.h"
#include "wifi_manager_defs.h"
#include "http_json.h"
#include "time_units.h"
#include "gw_cfg.h"

#ifdef __cplusplus
extern "C" {
#endif

#define HTTP_DOWNLOAD_TIMEOUT_SECONDS                 (25)
#define HTTP_DOWNLOAD_FW_RELEASE_INFO_TIMEOUT_SECONDS (50)
#define HTTP_DOWNLOAD_FW_BINARIES_TIMEOUT_SECONDS     (5 * TIME_UNITS_SECONDS_PER_MINUTE)
#define HTTP_DOWNLOAD_CHECK_MQTT_TIMEOUT_SECONDS      (30)

typedef struct http_header_item_t
{
    const char* p_key;
    const char* p_value;
} http_header_item_t;

typedef struct http_download_param_t
{
    const char*        p_url;
    TimeUnitsSeconds_t timeout_seconds;
    bool               flag_feed_task_watchdog;
    bool               flag_free_memory;
    const char*        p_server_cert;
    const char*        p_client_cert;
    const char*        p_client_key;
} http_download_param_t;

typedef struct http_download_param_with_auth_t
{
    const http_download_param_t           base;
    const gw_cfg_http_auth_type_e         auth_type;
    const ruuvi_gw_cfg_http_auth_t* const p_http_auth;
    const http_header_item_t* const       p_extra_header_item;
} http_download_param_with_auth_t;

typedef struct http_check_params_t
{
    const char* const             p_url;
    const gw_cfg_http_auth_type_e auth_type;
    const char* const             p_user;
    const char* const             p_pass;
    const bool                    use_ssl_client_cert;
    const bool                    use_ssl_server_cert;
} http_check_params_t;

bool
http_send_advs(
    const adv_report_table_t* const  p_reports,
    const uint32_t                   nonce,
    const bool                       flag_use_timestamps,
    const bool                       flag_post_to_ruuvi,
    const ruuvi_gw_cfg_http_t* const p_cfg_http,
    void* const                      p_user_data);

http_server_resp_t
http_check_post_advs(const http_check_params_t* const p_params, const TimeUnitsSeconds_t timeout_seconds);

http_server_resp_t
http_check_post_stat(const http_check_params_t* const p_params, const TimeUnitsSeconds_t timeout_seconds);

http_server_resp_t
http_check_mqtt(const ruuvi_gw_cfg_mqtt_t* const p_mqtt_cfg, const TimeUnitsSeconds_t timeout_seconds);

bool
http_send_statistics(
    const http_json_statistics_info_t* const p_stat_info,
    const adv_report_table_t* const          p_reports,
    const ruuvi_gw_cfg_http_stat_t* const    p_cfg_http_stat,
    void* const                              p_user_data,
    const bool                               use_ssl_client_cert,
    const bool                               use_ssl_server_cert);

bool
http_async_poll(void);

void
http_abort_any_req_during_processing(void);

void
http_feed_task_watchdog_if_needed(const bool flag_feed_task_watchdog);

typedef struct http_resp_cb_info_t
{
    uint32_t content_length;
    uint32_t offset;
    char*    p_buf;
} http_resp_cb_info_t;

http_server_resp_t
http_wait_until_async_req_completed(
    esp_http_client_handle_t   p_http_handle,
    http_resp_cb_info_t* const p_cb_info,
    const bool                 flag_feed_task_watchdog,
    const TimeUnitsSeconds_t   timeout_seconds);

bool
http_handle_add_authorization_if_needed(
    esp_http_client_handle_t              http_handle,
    const gw_cfg_http_auth_type_e         auth_type,
    const ruuvi_gw_cfg_http_auth_t* const p_http_auth);

const char*
http_client_method_to_str(const esp_http_client_method_t http_method);

#ifdef __cplusplus
}
#endif

#endif // RUUVI_HTTP_H
