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
json_ruuvi_parse_http_body(const char* const p_body, gw_cfg_t* const p_gw_cfg, bool* const p_flag_network_cfg);

#ifdef __cplusplus
}
#endif

#endif // RUUVI_GATEWAY_JSON_RUUVI_H
