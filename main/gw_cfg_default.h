/**
 * @file gw_cfg_default.h
 * @author TheSomeMan
 * @date 2021-08-29
 * @copyright Ruuvi Innovations Ltd, license BSD-3-Clause.
 */

#ifndef RUUVI_GATEWAY_ESP_GW_CFG_DEFAULT_H
#define RUUVI_GATEWAY_ESP_GW_CFG_DEFAULT_H

#include "gw_cfg.h"
#include "http_server_auth_type.h"

#ifdef __cplusplus
extern "C" {
#endif

#define RUUVI_GATEWAY_AUTH_DEFAULT_USER               "Admin"
#define RUUVI_GATEWAY_AUTH_DEFAULT_PASS_USE_DEVICE_ID "\xff"

const ruuvi_gateway_config_t *
gw_cfg_default_get_ptr(void);

void
gw_cfg_default_get(ruuvi_gateway_config_t *const p_gw_cfg);

ruuvi_gw_cfg_eth_t
gw_cfg_default_get_eth(void);

#ifdef __cplusplus
}
#endif

#endif // RUUVI_GATEWAY_ESP_GW_CFG_DEFAULT_H
