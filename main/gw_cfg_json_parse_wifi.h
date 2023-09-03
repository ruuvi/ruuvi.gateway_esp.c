/**
 * @file gw_cfg_json_parse_wifi.h
 * @author TheSomeMan
 * @date 2021-09-29
 * @copyright Ruuvi Innovations Ltd, license BSD-3-Clause.
 */

#ifndef RUUVI_GATEWAY_ESP_GW_CFG_JSON_PARSE_WIFI_H
#define RUUVI_GATEWAY_ESP_GW_CFG_JSON_PARSE_WIFI_H

#include "cJSON.h"
#include "gw_cfg.h"

#ifdef __cplusplus
extern "C" {
#endif

void
gw_cfg_json_parse_wifi_sta_config(const cJSON* const p_cjson, wifi_sta_config_t* const p_sta_cfg);

void
gw_cfg_json_parse_wifi_sta_settings(const cJSON* const p_cjson, wifi_settings_sta_t* const p_sta_settings);

void
gw_cfg_json_parse_wifi_ap_config(const cJSON* const p_cjson, wifi_ap_config_t* const p_ap_cfg);

void
gw_cfg_json_parse_wifi_ap_settings(const cJSON* const p_cjson, wifi_settings_ap_t* const p_ap_settings);

#ifdef __cplusplus
}
#endif

#endif // RUUVI_GATEWAY_ESP_GW_CFG_JSON_PARSE_WIFI_H
