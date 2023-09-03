/**
 * @file gw_cfg_json_parse_http_stat.h
 * @author TheSomeMan
 * @date 2021-09-29
 * @copyright Ruuvi Innovations Ltd, license BSD-3-Clause.
 */

#ifndef RUUVI_GATEWAY_ESP_GW_CFG_JSON_PARSE_HTTP_STAT_H
#define RUUVI_GATEWAY_ESP_GW_CFG_JSON_PARSE_HTTP_STAT_H

#include "cJSON.h"
#include "gw_cfg.h"

#ifdef __cplusplus
extern "C" {
#endif

void
gw_cfg_json_parse_http_stat(const cJSON* const p_json_root, ruuvi_gw_cfg_http_stat_t* const p_gw_cfg_http_stat);

#ifdef __cplusplus
}
#endif

#endif // RUUVI_GATEWAY_ESP_GW_CFG_JSON_PARSE_HTTP_STAT_H
