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

// clang-format off
#define RUUVI_GATEWAY_DEFAULT_CONFIGURATION \
    { \
        .header = RUUVI_GATEWAY_CONFIG_HEADER, \
        .fmt_version = RUUVI_GATEWAY_CONFIG_FMT_VERSION, \
        .eth = { \
            .use_eth = false, \
            .eth_dhcp = true, \
            .eth_static_ip = { 0 }, \
            .eth_netmask = { 0 }, \
            .eth_gw = { 0 }, \
            .eth_dns1 = { 0 }, \
            .eth_dns2 = { 0 }, \
        }, \
        .mqtt = { \
            .use_mqtt = false, \
            .mqtt_server = { 0 }, \
            .mqtt_port = 0, \
            .mqtt_prefix = { 0 }, \
            .mqtt_client_id = { 0 }, \
            .mqtt_user = { 0 }, \
            .mqtt_pass = { 0 }, \
        }, \
        .http = { \
            .use_http = true, \
            .http_url = { "https://network.ruuvi.com/record" }, \
            .http_user = { 0 }, \
            .http_pass = { 0 }, \
        }, \
        .lan_auth = { \
            .lan_auth_type = { HTTP_SERVER_AUTH_TYPE_STR_RUUVI }, \
            .lan_auth_user = { RUUVI_GATEWAY_AUTH_DEFAULT_USER }, \
            .lan_auth_pass = { RUUVI_GATEWAY_AUTH_DEFAULT_PASS_USE_DEVICE_ID }, \
        }, \
        .auto_update = { \
            .auto_update_cycle = AUTO_UPDATE_CYCLE_TYPE_REGULAR, \
            .auto_update_weekdays_bitmask = 0x7F, \
            .auto_update_interval_from = 0, \
            .auto_update_interval_to = 24, \
            .auto_update_tz_offset_hours = 3, \
        }, \
        .filter = { \
            .company_id = RUUVI_COMPANY_ID, \
            .company_filter = true, \
        }, \
        .scan = { \
            .scan_coded_phy = false, \
            .scan_1mbit_phy = true, \
            .scan_extended_payload = true, \
            .scan_channel_37 = true, \
            .scan_channel_38 = true, \
            .scan_channel_39 = true, \
        }, \
        .coordinates = { 0 }, \
    }
// clang-format on

extern const ruuvi_gateway_config_t g_gateway_config_default;

#ifdef __cplusplus
}
#endif

#endif // RUUVI_GATEWAY_ESP_GW_CFG_DEFAULT_H
