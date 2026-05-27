/**
 * @file gw_cfg_json_generate_internal.h
 * @author TheSomeMan
 * @date 2026-05-26
 * @copyright Ruuvi Innovations Ltd, license BSD-3-Clause.
 */

#ifndef RUUVI_GATEWAY_ESP_GW_CFG_JSON_GENERATE_INTERNAL_H
#define RUUVI_GATEWAY_ESP_GW_CFG_JSON_GENERATE_INTERNAL_H

#include <stdbool.h>
#include "cJSON.h"
#include "cjson_wrap.h"

#ifdef __cplusplus
extern "C" {
#endif

bool
gw_cfg_json_add_bool(cJSON* const p_json_root, const char* const p_item_name, const bool val);

bool
gw_cfg_json_add_string(cJSON* const p_json_root, const char* const p_item_name, const char* p_val);

bool
gw_cfg_json_add_number(cJSON* const p_json_root, const char* const p_item_name, const cjson_number_t val);

#ifdef __cplusplus
}
#endif

#endif // RUUVI_GATEWAY_ESP_GW_CFG_JSON_GENERATE_INTERNAL_H
