/**
 * @file json_helper.h
 * @author TheSomeMan
 * @date 2021-07-31
 * @copyright Ruuvi Innovations Ltd, license BSD-3-Clause.
 */

#ifndef RUUVI_GATEWAY_ESP_JSON_HELPER_H
#define RUUVI_GATEWAY_ESP_JSON_HELPER_H

#include "str_buf.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Find a value by the key.
 * @param p_json - ptr to c-string containing json.
 * @param p_key - ptr to a key
 * @return str_buf_t containing the value corresponding to the key or STR_BUF_INIT_NULL if json parsing failed.
 */
str_buf_t
json_helper_get_by_key(const char* const p_json, const char* const p_key);

#ifdef __cplusplus
}
#endif

#endif // RUUVI_GATEWAY_ESP_JSON_HELPER_H
