/**
 * @file gw_cfg_json_parse_internal.h
 * @author TheSomeMan
 * @date 2021-09-29
 * @copyright Ruuvi Innovations Ltd, license BSD-3-Clause.
 */

#ifndef RUUVI_GATEWAY_ESP_GW_CFG_JSON_PARSE_INTERNAL_H
#define RUUVI_GATEWAY_ESP_GW_CFG_JSON_PARSE_INTERNAL_H

#include <stdbool.h>
#include <stdint.h>
#include "cJSON.h"

#ifdef __cplusplus
extern "C" {
#endif

bool
gw_cfg_json_copy_string_val(
    const cJSON* const p_json_root,
    const char* const  p_attr_name,
    char* const        p_buf,
    const size_t       buf_len);

bool
gw_cfg_json_get_bool_val(const cJSON* p_json_root, const char* p_attr_name, bool* p_val);

bool
gw_cfg_json_get_uint32_val(const cJSON* const p_json_root, const char* const p_attr_name, uint32_t* const p_val);

bool
gw_cfg_json_get_uint16_val(const cJSON* const p_json_root, const char* const p_attr_name, uint16_t* const p_val);

bool
gw_cfg_json_get_uint8_val(const cJSON* const p_json_root, const char* const p_attr_name, uint8_t* const p_val);

bool
gw_cfg_json_get_int8_val(const cJSON* p_json_root, const char* p_attr_name, int8_t* p_val);

#ifdef __cplusplus
}
#endif

#endif // RUUVI_GATEWAY_ESP_GW_CFG_JSON_PARSE_INTERNAL_H
