/**
 * @file gw_cfg_json.h
 * @author TheSomeMan
 * @date 2021-09-29
 * @copyright Ruuvi Innovations Ltd, license BSD-3-Clause.
 */

#ifndef RUUVI_GATEWAY_ESP_GW_CFG_JSON_PARSE_H
#define RUUVI_GATEWAY_ESP_GW_CFG_JSON_PARSE_H

#include <stdbool.h>
#include "cjson_wrap.h"
#include "gw_cfg.h"

#if !defined(RUUVI_TESTS_GW_CFG_JSON)
#define RUUVI_TESTS_GW_CFG_JSON 0
#endif

#if RUUVI_TESTS_GW_CFG_JSON
#define GW_CFG_JSON_STATIC
#else
#define GW_CFG_JSON_STATIC static
#endif

#ifdef __cplusplus
extern "C" {
#endif

bool
gw_cfg_json_parse(
    const char* const p_json_name,
    const char* const p_log_title,
    const char* const p_json_str,
    gw_cfg_t* const   p_gw_cfg);

void
gw_cfg_json_parse_cjson(
    const cJSON* const          p_json_root,
    const char* const           p_log_title,
    gw_cfg_device_info_t* const p_dev_info,
    gw_cfg_ruuvi_t* const       p_ruuvi_cfg,
    gw_cfg_eth_t* const         p_eth_cfg,
    wifiman_config_ap_t* const  p_wifi_cfg_ap,
    wifiman_config_sta_t* const p_wifi_cfg_sta);

void
gw_cfg_json_parse_cjson_ruuvi(
    const cJSON* const    p_json_root,
    const char* const     p_log_title,
    gw_cfg_ruuvi_t* const p_ruuvi_cfg);

void
gw_cfg_json_parse_cjson_eth(
    const cJSON* const  p_json_root,
    const char* const   p_log_title,
    gw_cfg_eth_t* const p_eth_cfg);

void
gw_cfg_json_parse_cjson_wifi_ap(
    const cJSON* const         p_json_root,
    const char* const          p_log_title,
    gw_cfg_eth_t* const        p_eth_cfg,
    wifiman_config_ap_t* const p_wifi_cfg_ap);

void
gw_cfg_json_parse_cjson_wifi_sta(
    const cJSON* const          p_json_root,
    const char* const           p_log_title,
    wifiman_config_sta_t* const p_wifi_cfg_sta);

#ifdef __cplusplus
}
#endif

#endif // RUUVI_GATEWAY_ESP_GW_CFG_JSON_PARSE_H
