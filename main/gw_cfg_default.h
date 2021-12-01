/**
 * @file gw_cfg_default.h
 * @author TheSomeMan
 * @date 2021-08-29
 * @copyright Ruuvi Innovations Ltd, license BSD-3-Clause.
 */

#ifndef RUUVI_GATEWAY_ESP_GW_CFG_DEFAULT_H
#define RUUVI_GATEWAY_ESP_GW_CFG_DEFAULT_H

#include "gw_cfg.h"
#include <stdbool.h>
#include "http_server_auth_type.h"

#ifdef __cplusplus
extern "C" {
#endif

#define RUUVI_GATEWAY_HTTP_DEFAULT_URL "https://network.ruuvi.com/record"
#define RUUVI_GATEWAY_HTTP_STATUS_URL  "https://network.ruuvi.com/status"

#define RUUVI_GATEWAY_AUTH_DEFAULT_USER "Admin"

bool
gw_cfg_default_set_lan_auth_password(const char *const p_password_md5);

const char *
gw_cfg_default_get_lan_auth_password(void);

void
gw_cfg_default_get(ruuvi_gateway_config_t *const p_gw_cfg);

ruuvi_gw_cfg_lan_auth_t
gw_cfg_default_get_lan_auth(void);

ruuvi_gw_cfg_eth_t
gw_cfg_default_get_eth(void);

#ifdef __cplusplus
}
#endif

#endif // RUUVI_GATEWAY_ESP_GW_CFG_DEFAULT_H
