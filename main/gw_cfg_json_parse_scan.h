/**
 * @file gw_cfg_json_parse_scan.h
 * @author TheSomeMan
 * @date 2021-09-29
 * @copyright Ruuvi Innovations Ltd, license BSD-3-Clause.
 */

#ifndef RUUVI_GATEWAY_ESP_GW_CFG_JSON_PARSE_SCAN_H
#define RUUVI_GATEWAY_ESP_GW_CFG_JSON_PARSE_SCAN_H

#include "cJSON.h"
#include "gw_cfg.h"

#ifdef __cplusplus
extern "C" {
#endif

void
gw_cfg_json_parse_scan(const cJSON* const p_json_root, ruuvi_gw_cfg_scan_t* const p_gw_cfg_scan);

#ifdef __cplusplus
}
#endif

#endif // RUUVI_GATEWAY_ESP_GW_CFG_JSON_PARSE_SCAN_H
