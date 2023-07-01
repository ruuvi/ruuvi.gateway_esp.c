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

typedef bool (*http_download_cb_on_data_t)(
    const uint8_t* const   p_buf,
    const size_t           buf_size,
    const size_t           offset,
    const size_t           content_length,
    const http_resp_code_e resp_code,
    void*                  p_user_data);

typedef struct http_download_param_t
{
    const char*                p_url;
    TimeUnitsSeconds_t         timeout_seconds;
    http_download_cb_on_data_t p_cb_on_data;
    void*                      p_user_data;
    bool                       flag_feed_task_watchdog;
    bool                       flag_free_memory;
    const char*                p_server_cert;
    const char*                p_client_cert;
    const char*                p_client_key;
} http_download_param_t;

typedef struct http_check_param_t
{
    const char*        p_url;
    TimeUnitsSeconds_t timeout_seconds;
    bool               flag_feed_task_watchdog;
    bool               flag_free_memory;
    const char*        p_server_cert;
    const char*        p_client_cert;
    const char*        p_client_key;
} http_check_param_t;

bool
http_send_advs(
    const adv_report_table_t* const  p_reports,
    const uint32_t                   nonce,
    const bool                       flag_use_timestamps,
    const bool                       flag_post_to_ruuvi,
    const ruuvi_gw_cfg_http_t* const p_cfg_http,
    void* const                      p_user_data);

http_server_resp_t
http_check_post_advs(
    const char* const             p_url,
    const gw_cfg_http_auth_type_e auth_type,
    const char* const             p_user,
    const char* const             p_pass,
    const TimeUnitsSeconds_t      timeout_seconds,
    const bool                    use_ssl_client_cert,
    const bool                    use_ssl_server_cert);

http_server_resp_t
http_check_post_stat(
    const char* const        p_url,
    const char* const        p_user,
    const char* const        p_pass,
    const TimeUnitsSeconds_t timeout_seconds,
    const bool               use_ssl_client_cert,
    const bool               use_ssl_server_cert);

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

bool
http_download(const http_download_param_t* const p_param);

bool
http_download_with_auth(
    const http_download_param_t* const    p_param,
    const gw_cfg_http_auth_type_e         auth_type,
    const ruuvi_gw_cfg_http_auth_t* const p_http_auth,
    const http_header_item_t* const       p_extra_header_item);

bool
http_check_with_auth(
    const http_check_param_t* const       p_param,
    const gw_cfg_http_auth_type_e         auth_type,
    const ruuvi_gw_cfg_http_auth_t* const p_http_auth,
    const http_header_item_t* const       p_extra_header_item,
    http_resp_code_e* const               p_http_resp_code);

void
http_abort_any_req_during_processing(void);

#ifdef __cplusplus
}
#endif

#endif // RUUVI_HTTP_H
