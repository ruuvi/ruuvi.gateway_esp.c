/**
 * @file gw_cfg_default_json.h
 * @author TheSomeMan
 * @date 2022-02-28
 * @copyright Ruuvi Innovations Ltd, license BSD-3-Clause.
 */

#ifndef RUUVI_GATEWAY_ESP_GW_CFG_DEFAULT_JSON_H
#define RUUVI_GATEWAY_ESP_GW_CFG_DEFAULT_JSON_H

#include <stdbool.h>
#include "gw_cfg_default.h"

#ifdef __cplusplus
extern "C" {
#endif

bool
gw_cfg_default_json_read(gw_cfg_t *const p_gw_cfg_default);

#ifdef __cplusplus
}
#endif

#endif // RUUVI_GATEWAY_ESP_GW_CFG_DEFAULT_JSON_H
