/**
 * @file gw_cfg_json_parse_fw_update.h
 * @author TheSomeMan
 * @date 2021-09-29
 * @copyright Ruuvi Innovations Ltd, license BSD-3-Clause.
 */

#ifndef RUUVI_GATEWAY_ESP_GW_CFG_JSON_PARSE_FW_UPDATE_H
#define RUUVI_GATEWAY_ESP_GW_CFG_JSON_PARSE_FW_UPDATE_H

#include "cJSON.h"
#include "gw_cfg.h"

#ifdef __cplusplus
extern "C" {
#endif

void
gw_cfg_json_parse_fw_update(const cJSON* const p_cjson, ruuvi_gw_cfg_fw_update_t* const p_gw_cfg_fw_update);

#ifdef __cplusplus
}
#endif

#endif // RUUVI_GATEWAY_ESP_GW_CFG_JSON_PARSE_FW_UPDATE_H
