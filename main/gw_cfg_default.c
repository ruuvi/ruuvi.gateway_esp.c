/**
 * @file gw_cfg_default.c
 * @author TheSomeMan
 * @date 2021-08-29
 * @copyright Ruuvi Innovations Ltd, license BSD-3-Clause.
 */

#include "gw_cfg_default.h"

const ruuvi_gateway_config_t g_gateway_config_default = {
        .eth = {
            .use_eth = false,
            .eth_dhcp = true,
            .eth_static_ip = { 0 },
            .eth_netmask = { 0 },
            .eth_gw = { 0 },
            .eth_dns1 = { 0 },
            .eth_dns2 = { 0 },
        },
        .mqtt = {
            .use_mqtt = false,
            .mqtt_use_default_prefix = true,
            .mqtt_transport = { {MQTT_TRANSPORT_TCP} },
            .mqtt_server = { {0} },
            .mqtt_port = 0,
            .mqtt_prefix = { {0} },
            .mqtt_client_id = { {0} },
            .mqtt_user = { {0} },
            .mqtt_pass = { {0} },
        },
        .http = {
            .use_http = true,
            .http_url = { { RUUVI_GATEWAY_HTTP_DEFAULT_URL } },
            .http_user = {{ 0 } },
            .http_pass = {{ 0 } },
        },
        .http_stat = {
            .use_http_stat = true,
            .http_stat_url = {{ RUUVI_GATEWAY_HTTP_STATUS_URL }},
            .http_stat_user = {{ 0 }},
            .http_stat_pass = {{ 0 }},
        },
        .lan_auth = {
            .lan_auth_type = { HTTP_SERVER_AUTH_TYPE_STR_RUUVI },
            .lan_auth_user = { RUUVI_GATEWAY_AUTH_DEFAULT_USER },
            .lan_auth_pass = { RUUVI_GATEWAY_AUTH_DEFAULT_PASS_USE_DEVICE_ID },
        },
        .auto_update = {
            .auto_update_cycle = AUTO_UPDATE_CYCLE_TYPE_REGULAR,
            .auto_update_weekdays_bitmask = 0x7F,
            .auto_update_interval_from = 0,
            .auto_update_interval_to = 24,
            .auto_update_tz_offset_hours = 3,
        },
        .filter = {
            .company_id = RUUVI_COMPANY_ID,
            .company_use_filtering = true,
        },
        .scan = {
            .scan_coded_phy = false,
            .scan_1mbit_phy = true,
            .scan_extended_payload = true,
            .scan_channel_37 = true,
            .scan_channel_38 = true,
            .scan_channel_39 = true,
        },
        .coordinates = { { 0 } },
    };
