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
    wifi_ssid_t            wifi_ap_ssid;
} gw_cfg_default_t;

typedef struct gw_cfg_default_init_param_t
{
    const wifi_ssid_t              wifi_ap_ssid;
    const nrf52_device_id_t        device_id;
    const ruuvi_esp32_fw_ver_str_t esp32_fw_ver;
    const ruuvi_nrf52_fw_ver_str_t nrf52_fw_ver;
    const mac_address_bin_t        nrf52_mac_addr;
    const mac_address_bin_t        esp32_mac_addr_wifi;
    const mac_address_bin_t        esp32_mac_addr_eth;
} gw_cfg_default_init_param_t;

void
gw_cfg_default_init(
    const gw_cfg_default_init_param_t *const p_init_param,
    bool (*p_cb_gw_cfg_default_json_read)(gw_cfg_default_t *const p_gw_cfg_default));

void
gw_cfg_default_get(ruuvi_gateway_config_t *const p_gw_cfg);

const ruuvi_gw_cfg_mqtt_t *
gw_cfg_default_get_mqtt(void);

const ruuvi_gw_cfg_lan_auth_t *
gw_cfg_default_get_lan_auth(void);

ruuvi_gw_cfg_eth_t
gw_cfg_default_get_eth(void);

const wifi_sta_config_t *
gw_cfg_default_get_wifi_sta_config_ptr(void);

const wifi_ssid_t *
gw_cfg_default_get_wifi_ap_ssid(void);

#ifdef __cplusplus
}
#endif

#endif // RUUVI_GATEWAY_ESP_GW_CFG_DEFAULT_H
