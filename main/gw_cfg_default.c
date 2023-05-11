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
#include "wifi_manager.h"
#include "str_buf.h"
#include "gw_cfg_log.h"

static const gw_cfg_eth_t g_gateway_config_default_eth = {
    .use_eth       = true,
    .eth_dhcp      = true,
    .eth_static_ip = { { "" } },
    .eth_netmask   = { { "" } },
    .eth_gw        = { { "" } },
    .eth_dns1      = { { "" } },
    .eth_dns2      = { { "" } },
};

static const gw_cfg_ruuvi_t g_gateway_config_default_ruuvi = {
        .remote = {
            .use_remote_cfg = false,
            .use_ssl_client_cert = false,
            .use_ssl_server_cert = false,
            .url = {{ "" }},
            .auth_type = GW_CFG_HTTP_AUTH_TYPE_NONE,
            .auth = {
                .auth_basic = {
                    .user = {{""}},
                    .password = {{""}},
                },
            },
            .refresh_interval_minutes = 0,
        },
        .http = {
            .use_http_ruuvi = true,
            .use_http = false,
            .http_use_ssl_client_cert = false,
            .http_use_ssl_server_cert = false,
            .http_url = { { RUUVI_GATEWAY_HTTP_DEFAULT_URL } },
            .data_format = GW_CFG_HTTP_DATA_FORMAT_RUUVI,
            .auth_type = GW_CFG_HTTP_AUTH_TYPE_NONE,
            .auth = {
                .auth_basic = {
                    .user = {{""}},
                    .password = {{""}},
                },
            },
        },
        .http_stat = {
            .use_http_stat = true,
            .http_stat_use_ssl_client_cert = false,
            .http_stat_use_ssl_server_cert = false,
            .http_stat_url = {{ RUUVI_GATEWAY_HTTP_STATUS_URL }},
            .http_stat_user = {{ "" }},
            .http_stat_pass = {{ "" }},
        },
        .mqtt = {
            .use_mqtt = false,
            .mqtt_disable_retained_messages = false,
            .use_ssl_client_cert = false,
            .use_ssl_server_cert = false,
            .mqtt_transport = {{ MQTT_TRANSPORT_TCP }},
            .mqtt_server = {{ "test.mosquitto.org" }},
            .mqtt_port = 1883,
            .mqtt_prefix = {{ "" }},
            .mqtt_client_id = {{ "" }},
            .mqtt_user = {{ "" }},
            .mqtt_pass = {{ "" }},
        },
        .lan_auth = {
            .lan_auth_type = HTTP_SERVER_AUTH_TYPE_DEFAULT,
            .lan_auth_user = { RUUVI_GATEWAY_AUTH_DEFAULT_USER },
            .lan_auth_pass = { "" },  // default password is set in gw_cfg_default_init
            .lan_auth_api_key = { "" },
            .lan_auth_api_key_rw = { "" },
        },
        .auto_update = {
            .auto_update_cycle = AUTO_UPDATE_CYCLE_TYPE_REGULAR,
            .auto_update_weekdays_bitmask = 0x7F,
            .auto_update_interval_from = 0,
            .auto_update_interval_to = 24,
            .auto_update_tz_offset_hours = 3,
        },
        .ntp = {
            .ntp_use = true,
            .ntp_use_dhcp = false,
            .ntp_server1 = { "time.google.com" },
            .ntp_server2 = { "time.cloudflare.com" },
            .ntp_server3 = { "time.nist.gov" },
            .ntp_server4 = { "pool.ntp.org" },
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

static gw_cfg_t g_gw_cfg_default;

static void
gw_cfg_default_generate_lan_auth_password(
    const wifiman_hostname_t* const     p_gw_hostname,
    const nrf52_device_id_str_t* const  p_device_id,
    wifiman_md5_digest_hex_str_t* const p_lan_auth_default_password_md5)
{
    str_buf_t tmp_str_buf = str_buf_printf_with_alloc(
        "%s:%s:%s",
        RUUVI_GATEWAY_AUTH_DEFAULT_USER,
        p_gw_hostname->buf,
        p_device_id->str_buf);

    p_lan_auth_default_password_md5->buf[0] = '\0';

    if (NULL != tmp_str_buf.buf)
    {
        *p_lan_auth_default_password_md5 = wifiman_md5_calc_hex_str(tmp_str_buf.buf, strlen(tmp_str_buf.buf));
        str_buf_free_buf(&tmp_str_buf);
    }
}

static nrf52_device_id_str_t
gw_cfg_default_nrf52_device_id_to_str(const nrf52_device_id_t* const p_dev_id)
{
    nrf52_device_id_str_t device_id_str = { 0 };
    str_buf_t             str_buf       = {
                          .buf  = device_id_str.str_buf,
                          .size = sizeof(device_id_str.str_buf),
                          .idx  = 0,
    };
    for (size_t i = 0; i < sizeof(p_dev_id->id); ++i)
    {
        if (0 != i)
        {
            str_buf_printf(&str_buf, ":");
        }
        str_buf_printf(&str_buf, "%02X", p_dev_id->id[i]);
    }
    return device_id_str;
}

void
gw_cfg_default_init(
    const gw_cfg_default_init_param_t* const p_init_param,
    gw_cfg_default_json_read_callback_t      p_cb_gw_cfg_default_json_read)
{
    memset(&g_gw_cfg_default, 0, sizeof(g_gw_cfg_default));

    g_gw_cfg_default.ruuvi_cfg = g_gateway_config_default_ruuvi;
    g_gw_cfg_default.eth_cfg   = g_gateway_config_default_eth;

    wifiman_hostinfo_t hostinfo = { 0 };
    (void)snprintf(hostinfo.hostname.buf, sizeof(hostinfo.hostname.buf), "%s", p_init_param->hostname.buf);
    (void)snprintf(hostinfo.fw_ver.buf, sizeof(hostinfo.fw_ver.buf), "%s", p_init_param->esp32_fw_ver.buf);
    (void)snprintf(hostinfo.nrf52_fw_ver.buf, sizeof(hostinfo.nrf52_fw_ver.buf), "%s", p_init_param->nrf52_fw_ver.buf);

    g_gw_cfg_default.wifi_cfg = *wifi_manager_default_config_init(&p_init_param->wifi_ap_ssid, &hostinfo);

    gw_cfg_device_info_t* const p_def_dev_info = &g_gw_cfg_default.device_info;
    p_def_dev_info->wifi_ap                    = p_init_param->wifi_ap_ssid;
    p_def_dev_info->hostname                   = p_init_param->hostname;
    p_def_dev_info->esp32_fw_ver               = p_init_param->esp32_fw_ver;
    p_def_dev_info->nrf52_fw_ver               = p_init_param->nrf52_fw_ver;
    p_def_dev_info->nrf52_device_id            = gw_cfg_default_nrf52_device_id_to_str(&p_init_param->device_id);
    p_def_dev_info->nrf52_mac_addr             = mac_address_to_str(&p_init_param->nrf52_mac_addr);
    p_def_dev_info->esp32_mac_addr_wifi        = mac_address_to_str(&p_init_param->esp32_mac_addr_wifi);
    p_def_dev_info->esp32_mac_addr_eth         = mac_address_to_str(&p_init_param->esp32_mac_addr_eth);

    if ((NULL != p_cb_gw_cfg_default_json_read) && p_cb_gw_cfg_default_json_read(&g_gw_cfg_default))
    {
        wifi_manager_set_default_config(&g_gw_cfg_default.wifi_cfg);
    }

    ruuvi_gw_cfg_mqtt_t* const p_mqtt = &g_gw_cfg_default.ruuvi_cfg.mqtt;
    (void)snprintf(
        p_mqtt->mqtt_prefix.buf,
        sizeof(p_mqtt->mqtt_prefix.buf),
        "ruuvi/%s/",
        p_def_dev_info->nrf52_mac_addr.str_buf);
    (void)snprintf(
        p_mqtt->mqtt_client_id.buf,
        sizeof(p_mqtt->mqtt_client_id.buf),
        "%s",
        p_def_dev_info->nrf52_mac_addr.str_buf);

    wifiman_md5_digest_hex_str_t lan_auth_default_password_md5 = { 0 };
    gw_cfg_default_generate_lan_auth_password(
        &p_init_param->hostname,
        &p_def_dev_info->nrf52_device_id,
        &lan_auth_default_password_md5);

    ruuvi_gw_cfg_lan_auth_t* const p_lan_auth = &g_gw_cfg_default.ruuvi_cfg.lan_auth;
    _Static_assert(
        sizeof(p_lan_auth->lan_auth_pass) >= sizeof(lan_auth_default_password_md5),
        "sizeof lan_auth_pass >= sizeof wifiman_md5_digest_hex_str_t");

    (void)snprintf(
        p_lan_auth->lan_auth_user.buf,
        sizeof(p_lan_auth->lan_auth_user.buf),
        "%s",
        RUUVI_GATEWAY_AUTH_DEFAULT_USER);
    (void)snprintf(
        p_lan_auth->lan_auth_pass.buf,
        sizeof(p_lan_auth->lan_auth_pass.buf),
        "%s",
        lan_auth_default_password_md5.buf);
}

void
gw_cfg_default_deinit(void)
{
    memset(&g_gw_cfg_default, 0, sizeof(g_gw_cfg_default));
}

void
gw_cfg_default_log(void)
{
    const bool flag_log_device_info = true;
    gw_cfg_log(&g_gw_cfg_default, "Gateway SETTINGS (default)", flag_log_device_info);
}

void
gw_cfg_default_get(gw_cfg_t* const p_gw_cfg)
{
    *p_gw_cfg = g_gw_cfg_default;
}

const gw_cfg_device_info_t*
gw_cfg_default_device_info(void)
{
    return &g_gw_cfg_default.device_info;
}

const ruuvi_gw_cfg_mqtt_t*
gw_cfg_default_get_mqtt(void)
{
    return &g_gw_cfg_default.ruuvi_cfg.mqtt;
}

const ruuvi_gw_cfg_ntp_t*
gw_cfg_default_get_ntp(void)
{
    return &g_gw_cfg_default.ruuvi_cfg.ntp;
}

const ruuvi_gw_cfg_filter_t*
gw_cfg_default_get_filter(void)
{
    return &g_gw_cfg_default.ruuvi_cfg.filter;
}

const ruuvi_gw_cfg_lan_auth_t*
gw_cfg_default_get_lan_auth(void)
{
    return &g_gw_cfg_default.ruuvi_cfg.lan_auth;
}

const gw_cfg_eth_t*
gw_cfg_default_get_eth(void)
{
    return &g_gw_cfg_default.eth_cfg;
}

const wifiman_config_ap_t*
gw_cfg_default_get_wifi_config_ap_ptr(void)
{
    return &g_gw_cfg_default.wifi_cfg.ap;
}

const wifiman_config_sta_t*
gw_cfg_default_get_wifi_config_sta_ptr(void)
{
    return &g_gw_cfg_default.wifi_cfg.sta;
}

const wifiman_wifi_ssid_t*
gw_cfg_default_get_wifi_ap_ssid(void)
{
    return &g_gw_cfg_default.device_info.wifi_ap;
}

const wifiman_hostname_t*
gw_cfg_default_get_hostname(void)
{
    return &g_gw_cfg_default.device_info.hostname;
}
