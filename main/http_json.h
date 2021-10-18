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
http_json_create_records_str(
    const adv_report_table_t *const p_reports,
    const time_t                    timestamp,
    const mac_address_str_t *const  p_mac_addr,
    const char *const               p_coordinates_str,
    const bool                      flag_use_nonce,
    const uint32_t                  nonce,
    cjson_wrap_str_t *const         p_json_str);

bool
http_json_create_status_str(
    const mac_address_str_t         nrf52_mac_addr,
    const char *                    p_esp_fw,
    const char *                    p_nrf_fw,
    const uint32_t                  uptime,
    const bool                      flag_is_wifi,
    const uint32_t                  network_disconnect_cnt,
    const adv_report_table_t *const p_reports,
    const uint32_t                  nonce,
    cjson_wrap_str_t *const         p_json_str);

#ifdef __cplusplus
}
#endif

#endif // RUUVI_GATEWAY_HTTP_JSON_H
