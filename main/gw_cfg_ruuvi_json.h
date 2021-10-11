/**
 * @file gw_cfg_ruuvi_json.h
 * @author TheSomeMan
 * @date 2021-09-29
 * @copyright Ruuvi Innovations Ltd, license BSD-3-Clause.
 */

#ifndef RUUVI_GATEWAY_ESP_GW_CFG_RUUVI_JSON_H
#define RUUVI_GATEWAY_ESP_GW_CFG_RUUVI_JSON_H

#include <stdbool.h>
#include "cjson_wrap.h"
#include "gw_cfg.h"

#ifdef __cplusplus
extern "C" {
#endif

bool
gw_cfg_ruuvi_json_generate(
    const ruuvi_gateway_config_t *const p_cfg,
    const mac_address_str_t *const      p_mac_sta,
    const char *const                   p_fw_ver,
    const char *const                   p_nrf52_fw_ver,
    cjson_wrap_str_t *const             p_json_str);

#ifdef __cplusplus
}
#endif

#endif // RUUVI_GATEWAY_ESP_GW_CFG_RUUVI_JSON_H
