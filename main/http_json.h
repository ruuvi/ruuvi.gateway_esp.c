/**
 * @file http_json.h
 * @author TheSomeMan
 * @date 2021-02-03
 * @copyright Ruuvi Innovations Ltd, license BSD-3-Clause.
 */

#ifndef RUUVI_GATEWAY_HTTP_JSON_H
#define RUUVI_GATEWAY_HTTP_JSON_H

#include <stdbool.h>
#include <time.h>
#include "adv_table.h"
#include "cjson_wrap.h"

#ifdef __cplusplus
extern "C" {
#endif

bool
http_create_json_str(
    const adv_report_table_t *const p_reports,
    const time_t                    timestamp,
    const mac_address_str_t *const  p_mac_addr,
    const char *const               p_coordinates_str,
    const bool                      flag_use_nonce,
    const uint32_t                  nonce,
    cjson_wrap_str_t *const         p_json_str);

bool
http_create_status_online_json_str(
    const time_t                   timestamp,
    const mac_address_str_t *const p_mac_addr,
    const char *const              p_coordinates_str,
    const uint32_t                 nonce,
    cjson_wrap_str_t *const        p_json_str);

#ifdef __cplusplus
}
#endif

#endif // RUUVI_GATEWAY_HTTP_JSON_H
