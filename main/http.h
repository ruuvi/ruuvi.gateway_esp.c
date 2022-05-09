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

#ifdef __cplusplus
extern "C" {
#endif

#define HTTP_DOWNLOAD_TIMEOUT_SECONDS (25)

typedef struct http_header_item_t
{
    const char *p_key;
    const char *p_value;
} http_header_item_t;

typedef bool (*http_download_cb_on_data_t)(
    const uint8_t *const   p_buf,
    const size_t           buf_size,
    const size_t           offset,
    const size_t           content_length,
    const http_resp_code_e resp_code,
    void *                 p_user_data);

bool
http_send_advs(const adv_report_table_t *const p_reports, const uint32_t nonce, const bool flag_use_timestamps);

bool
http_send_statistics(const http_json_statistics_info_t *const p_stat_info, const adv_report_table_t *const p_reports);

bool
http_async_poll(void);

bool
http_download(
    const char *const          p_url,
    const TimeUnitsSeconds_t   timeout_seconds,
    http_download_cb_on_data_t p_cb_on_data,
    void *const                p_user_data,
    const bool                 flag_feed_task_watchdog);

bool
http_download_with_auth(
    const char *const                     p_url,
    const TimeUnitsSeconds_t              timeout_seconds,
    const gw_cfg_remote_auth_type_e       gw_cfg_http_auth_type,
    const ruuvi_gw_cfg_http_auth_t *const p_http_auth,
    const http_header_item_t *const       p_extra_header_item,
    http_download_cb_on_data_t            p_cb_on_data,
    void *const                           p_user_data,
    const bool                            flag_feed_task_watchdog);

#ifdef __cplusplus
}
#endif

#endif // RUUVI_HTTP_H
