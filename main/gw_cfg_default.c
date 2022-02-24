/**
 * @file gw_cfg_default.c
 * @author TheSomeMan
 * @date 2021-08-29
 * @copyright Ruuvi Innovations Ltd, license BSD-3-Clause.
 */

#include "gw_cfg_default.h"
#include <stdio.h>
#include <string.h>
#include "os_malloc.h"
#include "wifiman_md5.h"

static ruuvi_gateway_config_t g_gateway_config_default = {
        .eth = {
            .use_eth = false,
            .eth_dhcp = true,
            .eth_static_ip = {{ "" }},
            .eth_netmask = {{ "" }},
            .eth_gw = {{ "" }},
            .eth_dns1 = {{ "" }},
            .eth_dns2 = {{ "" }},
        },
        .http = {
            .use_http = true,
            .http_url = { { RUUVI_GATEWAY_HTTP_DEFAULT_URL } },
            .http_user = {{ "" }},
            .http_pass = {{ "" }},
        },
        .http_stat = {
            .use_http_stat = true,
            .http_stat_url = {{ RUUVI_GATEWAY_HTTP_STATUS_URL }},
            .http_stat_user = {{ "" }},
            .http_stat_pass = {{ "" }},
        },
        .mqtt = {
            .use_mqtt = false,
            .mqtt_use_default_prefix = true,
            .mqtt_transport = {{ MQTT_TRANSPORT_TCP }},
            .mqtt_server = {{ "" }},
            .mqtt_port = 0,
            .mqtt_prefix = {{ "" }},
            .mqtt_client_id = {{ "" }},
            .mqtt_user = {{ "" }},
            .mqtt_pass = {{ "" }},
        },
        .lan_auth = {
            .lan_auth_type = { HTTP_SERVER_AUTH_TYPE_STR_RUUVI },
            .lan_auth_user = { RUUVI_GATEWAY_AUTH_DEFAULT_USER },
            .lan_auth_pass = { "" },
            .lan_auth_api_key = { "" },
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
        .coordinates = {{ "" }},
    };

static void
gw_cfg_generate_default_lan_auth_password(
    const wifi_ssid_t *const            p_gw_wifi_ssid,
    const nrf52_device_id_str_t *const  p_device_id,
    wifiman_md5_digest_hex_str_t *const p_lan_auth_default_password_md5)
{
    char tmp_buf[sizeof(RUUVI_GATEWAY_AUTH_DEFAULT_USER) + 1 + MAX_SSID_SIZE + 1 + sizeof(nrf52_device_id_str_t)];
    snprintf(
        tmp_buf,
        sizeof(tmp_buf),
        "%s:%s:%s",
        RUUVI_GATEWAY_AUTH_DEFAULT_USER,
        p_gw_wifi_ssid->ssid_buf,
        p_device_id->str_buf);

    *p_lan_auth_default_password_md5 = wifiman_md5_calc_hex_str(tmp_buf, strlen(tmp_buf));
}

void
gw_cfg_default_init(const wifi_ssid_t *const p_gw_wifi_ssid, const nrf52_device_id_str_t device_id_str)
{
    wifiman_md5_digest_hex_str_t lan_auth_default_password_md5 = { 0 };

    gw_cfg_generate_default_lan_auth_password(p_gw_wifi_ssid, &device_id_str, &lan_auth_default_password_md5);

    _Static_assert(
        sizeof(g_gateway_config_default.lan_auth.lan_auth_pass) >= sizeof(lan_auth_default_password_md5),
        "sizeof lan_auth_pass >= sizeof wifiman_md5_digest_hex_str_t");
    snprintf(
        g_gateway_config_default.lan_auth.lan_auth_pass,
        sizeof(g_gateway_config_default.lan_auth.lan_auth_pass),
        "%s",
        lan_auth_default_password_md5.buf);
}

void
gw_cfg_default_get(ruuvi_gateway_config_t *const p_gw_cfg)
{
    *p_gw_cfg = g_gateway_config_default;
}

const ruuvi_gw_cfg_lan_auth_t *
gw_cfg_default_get_lan_auth(void)
{
    return &g_gateway_config_default.lan_auth;
}

ruuvi_gw_cfg_eth_t
gw_cfg_default_get_eth(void)
{
    return g_gateway_config_default.eth;
}
