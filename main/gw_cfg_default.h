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
#define RUUVI_GATEWAY_FW_UPDATE_URL    "https://network.ruuvi.com/firmwareupdate"

#define RUUVI_GATEWAY_AUTH_DEFAULT_USER "Admin"

typedef bool (*gw_cfg_default_json_read_callback_t)(gw_cfg_t* const p_gw_cfg_default);

typedef struct gw_cfg_default_init_param_t
{
    wifiman_wifi_ssid_t      wifi_ap_ssid;
    wifiman_hostname_t       hostname;
    nrf52_device_id_t        device_id;
    ruuvi_esp32_fw_ver_str_t esp32_fw_ver;
    ruuvi_nrf52_fw_ver_str_t nrf52_fw_ver;
    mac_address_bin_t        nrf52_mac_addr;
    mac_address_bin_t        esp32_mac_addr_wifi;
    mac_address_bin_t        esp32_mac_addr_eth;
} gw_cfg_default_init_param_t;

bool
gw_cfg_default_init(
    const gw_cfg_default_init_param_t* const p_init_param,
    gw_cfg_default_json_read_callback_t      p_cb_gw_cfg_default_json_read);

void
gw_cfg_default_deinit(void);

void
gw_cfg_default_log(void);

void
gw_cfg_default_get(gw_cfg_t* const p_gw_cfg);

const gw_cfg_device_info_t*
gw_cfg_default_device_info(void);

const ruuvi_gw_cfg_mqtt_t*
gw_cfg_default_get_mqtt(void);

const ruuvi_gw_cfg_ntp_t*
gw_cfg_default_get_ntp(void);

const ruuvi_gw_cfg_filter_t*
gw_cfg_default_get_filter(void);

const ruuvi_gw_cfg_lan_auth_t*
gw_cfg_default_get_lan_auth(void);

const gw_cfg_eth_t*
gw_cfg_default_get_eth(void);

const wifiman_config_ap_t*
gw_cfg_default_get_wifi_config_ap_ptr(void);

const wifiman_config_sta_t*
gw_cfg_default_get_wifi_config_sta_ptr(void);

const wifiman_wifi_ssid_t*
gw_cfg_default_get_wifi_ap_ssid(void);

const wifiman_hostname_t*
gw_cfg_default_get_hostname(void);

#ifdef __cplusplus
}
#endif

#endif // RUUVI_GATEWAY_ESP_GW_CFG_DEFAULT_H
