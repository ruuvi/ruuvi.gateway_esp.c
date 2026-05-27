/**
 * @file gw_cfg_json_generate_remote.h
 * @author TheSomeMan
 * @date 2026-05-26
 * @copyright Ruuvi Innovations Ltd, license BSD-3-Clause.
 */

#ifndef RUUVI_GATEWAY_ESP_GW_CFG_JSON_GENERATE_REMOTE_H
#define RUUVI_GATEWAY_ESP_GW_CFG_JSON_GENERATE_REMOTE_H

#include <stdbool.h>
#include "cJSON.h"
#include "gw_cfg.h"

#ifdef __cplusplus
extern "C" {
#endif

bool
gw_cfg_json_add_items_remote(
    cJSON* const                       p_json_root,
    const ruuvi_gw_cfg_remote_t* const p_cfg_remote,
    const bool                         flag_hide_passwords);

#ifdef __cplusplus
}
#endif

#endif // RUUVI_GATEWAY_ESP_GW_CFG_JSON_GENERATE_REMOTE_H
