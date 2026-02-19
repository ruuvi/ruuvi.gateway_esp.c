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
#include "os_sema.h"
#include "os_task.h"
#include "adv_table.h"
#include "wifi_manager_defs.h"
#include "http_json.h"
#include "time_units.h"
#include "gw_cfg.h"
#include "hmac_sha256.h"

#ifdef __cplusplus
extern "C" {
#endif

#define HTTP_DOWNLOAD_TIMEOUT_SECONDS                 (25)
#define HTTP_DOWNLOAD_FW_RELEASE_INFO_TIMEOUT_SECONDS (30)
#define HTTP_DOWNLOAD_FW_BINARIES_TIMEOUT_SECONDS     (5 * TIME_UNITS_SECONDS_PER_MINUTE)
#define HTTP_DOWNLOAD_CHECK_MQTT_TIMEOUT_SECONDS      (30)

typedef struct http_client_config_t
{
    esp_http_client_config_t     esp_http_client_config;
    ruuvi_gw_cfg_http_url_t      http_url;
    ruuvi_gw_cfg_http_user_t     http_user;
    ruuvi_gw_cfg_http_password_t http_pass;
} http_client_config_t;

typedef enum http_post_recipient_e
{
    HTTP_POST_RECIPIENT_STATS,
    HTTP_POST_RECIPIENT_ADVS1,
    HTTP_POST_RECIPIENT_ADVS2,
} http_post_recipient_e;

typedef struct http_resp_cb_info_t
{
    uint32_t content_length;
    uint32_t offset;
    char*    p_buf;
} http_resp_cb_info_t;

typedef struct http_async_info_t
{
    os_sema_t                p_http_async_sema;
    os_sema_static_t         http_async_sema_mem;
    http_client_config_t     http_client_config;
    esp_http_client_handle_t p_http_client_handle;
    bool                     use_json_stream_gen;
    union
    {
        cjson_wrap_str_t   cjson_str;
        json_stream_gen_t* p_gen;
    } select;
    hmac_sha256_t         hmac_sha256;
    http_post_recipient_e recipient;
    os_task_handle_t      p_task;
    http_resp_cb_info_t   http_resp_cb_info;
#if !defined(CONFIG_MBEDTLS_SSL_VARIABLE_BUFFER_LENGTH)
    uint8_t* in_buf;
    uint8_t* out_buf;
#endif
} http_async_info_t;

typedef struct http_header_item_t
{
    const char* p_key;
    const char* p_value;
} http_header_item_t;

typedef struct http_check_params_t
{
    const char* const             p_url;
    const gw_cfg_http_auth_type_e auth_type;
    const char* const             p_user;
    const char* const             p_pass;
    const bool                    use_ssl_client_cert;
    const bool                    use_ssl_server_cert;
} http_check_params_t;

typedef struct http_init_client_config_params_t
{
    const ruuvi_gw_cfg_http_url_t* const      p_url;
    const ruuvi_gw_cfg_http_user_t* const     p_user;
    const ruuvi_gw_cfg_http_password_t* const p_password;
    const char* const                         p_server_cert;
    const char* const                         p_client_cert;
    const char* const                         p_client_key;
    uint8_t* const                            p_ssl_in_buf; //!< Pre-allocated buffer for incoming data. It can be NULL.
    uint8_t* const p_ssl_out_buf;                           //!< Pre-allocated buffer for outgoing data. It can be NULL.
#if defined(CONFIG_MBEDTLS_SSL_VARIABLE_BUFFER_LENGTH)
    const size_t ssl_in_content_len;
    const size_t ssl_out_content_len;
#endif
} http_init_client_config_params_t;

bool
http_post_advs(
    const adv_report_table_t* const  p_reports,
    const uint32_t                   nonce,
    const bool                       flag_use_timestamps,
    const bool                       flag_post_to_ruuvi,
    const ruuvi_gw_cfg_http_t* const p_cfg_http,
    void* const                      p_user_data);

http_server_resp_t
http_check_post_advs(const http_check_params_t* const p_params, const TimeUnitsSeconds_t timeout_seconds);

bool
http_post_stat(
    const http_json_statistics_info_t* const p_stat_info,
    const adv_report_table_t* const          p_reports,
    const ruuvi_gw_cfg_http_stat_t* const    p_cfg_http_stat,
    void* const                              p_user_data,
    const bool                               use_ssl_client_cert,
    const bool                               use_ssl_server_cert);

http_server_resp_t
http_check_post_stat(const http_check_params_t* const p_params, const TimeUnitsSeconds_t timeout_seconds);

http_server_resp_t
http_check_mqtt(const ruuvi_gw_cfg_mqtt_t* const p_mqtt_cfg, const TimeUnitsSeconds_t timeout_seconds);

http_async_info_t*
http_get_async_info(void);

void
http_async_info_free_data(http_async_info_t* const p_http_async_info);

bool
http_async_poll(void);

void
http_abort_any_req_during_processing(void);

void
http_feed_task_watchdog_if_needed(const bool flag_feed_task_watchdog);

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

void
http_init_client_config(
    http_client_config_t* const                   p_http_client_config,
    const http_init_client_config_params_t* const p_params,
    void* const                                   p_user_data);

bool
http_send_async(http_async_info_t* const p_http_async_info);

#ifdef __cplusplus
}
#endif

#endif // RUUVI_HTTP_H
