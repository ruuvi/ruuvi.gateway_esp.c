/**
 * @file gw_cfg_json_parse_eth.h
 * @author TheSomeMan
 * @date 2021-09-29
 * @copyright Ruuvi Innovations Ltd, license BSD-3-Clause.
 */

#ifndef RUUVI_GATEWAY_ESP_GW_CFG_JSON_PARSE_ETH_H
#define RUUVI_GATEWAY_ESP_GW_CFG_JSON_PARSE_ETH_H

#include "cJSON.h"
#include "gw_cfg.h"

#ifdef __cplusplus
extern "C" {
#endif

void
gw_cfg_json_parse_eth(const cJSON* const p_json_root, gw_cfg_eth_t* const p_gw_cfg_eth);

#ifdef __cplusplus
}
#endif

#endif // RUUVI_GATEWAY_ESP_GW_CFG_JSON_PARSE_ETH_H
