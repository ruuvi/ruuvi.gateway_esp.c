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
#include "ruuvi_device_id.h"

#ifdef __cplusplus
extern "C" {
#endif

#define RUUVI_GATEWAY_HTTP_DEFAULT_URL "https://network.ruuvi.com/record"
#define RUUVI_GATEWAY_HTTP_STATUS_URL  "https://network.ruuvi.com/status"

#define RUUVI_GATEWAY_AUTH_DEFAULT_USER "Admin"

typedef struct gw_cfg_default_t
{
    ruuvi_gateway_config_t ruuvi_gw_cfg;
    wifi_sta_config_t      wifi_sta_config;
} gw_cfg_default_t;

void
gw_cfg_default_init(const wifi_ssid_t *const p_gw_wifi_ssid, const nrf52_device_id_str_t device_id_str);

void
gw_cfg_default_get(ruuvi_gateway_config_t *const p_gw_cfg);

gw_cfg_default_t *
gw_cfg_default_get_ptr(void);

const ruuvi_gw_cfg_lan_auth_t *
gw_cfg_default_get_lan_auth(void);

ruuvi_gw_cfg_eth_t
gw_cfg_default_get_eth(void);

const wifi_sta_config_t *
gw_cfg_default_get_wifi_sta_config_ptr(void);

#ifdef __cplusplus
}
#endif

#endif // RUUVI_GATEWAY_ESP_GW_CFG_DEFAULT_H
