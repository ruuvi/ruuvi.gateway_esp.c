/**
 * @file http_post_helper.h
 * @author TheSomeMan
 * @date 2026-05-20
 * @copyright Ruuvi Innovations Ltd, license BSD-3-Clause.
 */

#ifndef RUUVI_GATEWAY_ESP_HTTP_POST_HELPER_H
#define RUUVI_GATEWAY_ESP_HTTP_POST_HELPER_H
#include "time_units.h"
#include "http.h"
#include "wifi_manager_defs.h"

#ifdef __cplusplus
extern "C" {
#endif

http_server_resp_t
http_post_helper_wait_until_async_req_completed_and_gen_resp(
    http_async_info_t* const p_http_async_info,
    const TimeUnitsSeconds_t timeout_seconds);

#ifdef __cplusplus
}
#endif

#endif // RUUVI_GATEWAY_ESP_HTTP_POST_HELPER_H
