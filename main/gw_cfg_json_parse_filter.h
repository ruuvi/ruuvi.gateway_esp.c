/**
 * @file gw_cfg_json_parse_filter.h
 * @author TheSomeMan
 * @date 2021-09-29
 * @copyright Ruuvi Innovations Ltd, license BSD-3-Clause.
 */

#ifndef RUUVI_GATEWAY_ESP_GW_CFG_JSON_PARSE_FILTER_H
#define RUUVI_GATEWAY_ESP_GW_CFG_JSON_PARSE_FILTER_H

#include "cJSON.h"
#include "gw_cfg.h"

#ifdef __cplusplus
extern "C" {
#endif

void
gw_cfg_json_parse_filter(const cJSON* const p_json_root, ruuvi_gw_cfg_filter_t* const p_gw_cfg_filter);

#ifdef __cplusplus
}
#endif

#endif // RUUVI_GATEWAY_ESP_GW_CFG_JSON_PARSE_FILTER_H
