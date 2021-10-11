/**
 * @file json_ruuvi.h
 * @author TheSomeMan
 * @date 2020-10-31
 * @copyright Ruuvi Innovations Ltd, license BSD-3-Clause.
 */

#ifndef RUUVI_GATEWAY_JSON_RUUVI_H
#define RUUVI_GATEWAY_JSON_RUUVI_H

#include <stdbool.h>
#include "settings.h"
#include "cJSON.h"

#if !defined(RUUVI_TESTS_JSON_RUUVI)
#define RUUVI_TESTS_JSON_RUUVI (0)
#endif

#if RUUVI_TESTS_JSON_RUUVI
#define JSON_RUUVI_STATIC
#else
#define JSON_RUUVI_STATIC static
#endif

#ifdef __cplusplus
extern "C" {
#endif

bool
json_ruuvi_parse_http_body(const char *p_body, ruuvi_gateway_config_t *const p_gw_cfg, bool *const p_flag_network_cfg);

#if RUUVI_TESTS_JSON_RUUVI

bool
json_ruuvi_copy_string_val(
    const cJSON *p_json_root,
    const char * p_attr_name,
    char *       buf,
    const size_t buf_len,
    bool         flag_log_err_if_not_found);

bool
json_ruuvi_get_bool_val(const cJSON *p_json_root, const char *p_attr_name, bool *p_val, bool flag_log_err_if_not_found);

bool
json_ruuvi_get_uint16_val(
    const cJSON *p_json_root,
    const char * p_attr_name,
    uint16_t *   p_val,
    bool         flag_log_err_if_not_found);

bool
json_ruuvi_parse(const cJSON *p_json_root, ruuvi_gateway_config_t *const p_gw_cfg, bool *const p_flag_network_cfg);

#endif

#ifdef __cplusplus
}
#endif

#endif // RUUVI_GATEWAY_JSON_RUUVI_H
