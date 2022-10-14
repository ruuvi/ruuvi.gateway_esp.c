/**
 * @file gw_cfg_json_generate.h
 * @author TheSomeMan
 * @date 2022-05-18
 * @copyright Ruuvi Innovations Ltd, license BSD-3-Clause.
 */

#ifndef RUUVI_GATEWAY_GW_CFG_JSON_GENERATE_H
#define RUUVI_GATEWAY_GW_CFG_JSON_GENERATE_H

#include <stdbool.h>
#include "gw_cfg.h"
#include "cjson_wrap.h"

#ifdef __cplusplus
extern "C" {
#endif

bool
gw_cfg_json_generate_full(const gw_cfg_t* const p_gw_cfg, cjson_wrap_str_t* const p_json_str);

bool
gw_cfg_json_generate_without_passwords(const gw_cfg_t* const p_gw_cfg, cjson_wrap_str_t* const p_json_str);

#ifdef __cplusplus
}
#endif

#endif // RUUVI_GATEWAY_GW_CFG_JSON_GENERATE_H
