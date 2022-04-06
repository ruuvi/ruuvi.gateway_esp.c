/**
 * @file gw_cfg_json.h
 * @author TheSomeMan
 * @date 2021-09-29
 * @copyright Ruuvi Innovations Ltd, license BSD-3-Clause.
 */

#ifndef RUUVI_GATEWAY_ESP_GW_CFG_JSON_H
#define RUUVI_GATEWAY_ESP_GW_CFG_JSON_H

#include <stdbool.h>
#include "cjson_wrap.h"
#include "gw_cfg.h"

#ifdef __cplusplus
extern "C" {
#endif

bool
gw_cfg_json_generate(const ruuvi_gateway_config_t *const p_gw_cfg, cjson_wrap_str_t *const p_json_str);

bool
gw_cfg_json_parse(const char *const p_json_str, ruuvi_gateway_config_t *const p_gw_cfg, bool *const p_flag_modified);

void
gw_cfg_json_parse_cjson(
    const cJSON *const                p_json_root,
    ruuvi_gateway_config_t *const     p_gw_cfg,
    ruuvi_gw_cfg_device_info_t *const p_dev_info);

#ifdef __cplusplus
}
#endif

#endif // RUUVI_GATEWAY_ESP_GW_CFG_JSON_H
