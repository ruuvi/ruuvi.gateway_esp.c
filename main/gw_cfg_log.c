/**
 * @file gw_cfg_log.c
 * @author TheSomeMan
 * @date 2022-04-12
 * @copyright Ruuvi Innovations Ltd, license BSD-3-Clause.
 */

#include "gw_cfg_log.h"
#include "esp_netif.h"
#include "wifi_manager_defs.h"

#if defined(RUUVI_TESTS_HTTP_SERVER_CB)
#define LOG_LOCAL_LEVEL LOG_LEVEL_DEBUG
#else
#define LOG_LOCAL_LEVEL LOG_LEVEL_INFO
#endif
#include "log.h"

#if (LOG_LOCAL_LEVEL >= LOG_LEVEL_DEBUG) && !RUUVI_TESTS
#warning Debug log level prints out the passwords as a "plaintext".
#endif

static const char TAG[] = "gw_cfg";

static const char*
gw_cfg_log_get_sta_password_for_logging(const wifi_sta_config_t* const p_wifi_cfg_sta)
{
    return ((LOG_LOCAL_LEVEL >= LOG_LEVEL_DEBUG) ? (const char*)p_wifi_cfg_sta->password : "********");
}

static const char*
gw_cfg_log_wifi_auth_mode_to_str(const wifi_auth_mode_t auth_mode)
{
    switch (auth_mode)
    {
        case WIFI_AUTH_OPEN:
            return "OPEN";
        case WIFI_AUTH_WEP:
            return "WEP";
        case WIFI_AUTH_WPA_PSK:
            return "WPA_PSK";
        case WIFI_AUTH_WPA2_PSK:
            return "WPA2_PSK";
        case WIFI_AUTH_WPA_WPA2_PSK:
            return "WPA_WPA2_PSK";
        case WIFI_AUTH_WPA2_ENTERPRISE:
            return "WPA2_ENTERPRISE";
        case WIFI_AUTH_WPA3_PSK:
            return "WPA3_PSK";
        case WIFI_AUTH_WPA2_WPA3_PSK:
            return "WPA2_WPA3_PSK";
        case WIFI_AUTH_MAX:
            return "Unknown";
    }
    assert(0);
    return "Unknown";
}

static const char*
gw_cfg_log_wifi_bandwidth_to_str(const wifi_bandwidth_t bandwidth)
{
    switch (bandwidth)
    {
        case WIFI_BW_HT20:
            return "20MHz";
        case WIFI_BW_HT40:
            return "40MHz";
    }
    assert(0);
    return "Unknown";
}

static void
gw_cfg_log_wifi_config_ap(const wifi_ap_config_t* const p_wifi_cfg_ap)
{
    LOG_INFO("config: wifi_ap_config: SSID: %s", p_wifi_cfg_ap->ssid);
    LOG_INFO("config: wifi_ap_config: password: %s", p_wifi_cfg_ap->password);
    LOG_INFO("config: wifi_ap_config: ssid_len: %u", (printf_uint_t)p_wifi_cfg_ap->ssid_len);
    LOG_INFO("config: wifi_ap_config: channel: %u", (printf_uint_t)p_wifi_cfg_ap->channel);
    LOG_INFO("config: wifi_ap_config: auth_mode: %s", gw_cfg_log_wifi_auth_mode_to_str(p_wifi_cfg_ap->authmode));
    LOG_INFO("config: wifi_ap_config: ssid_hidden: %u", (printf_uint_t)p_wifi_cfg_ap->ssid_hidden);
    LOG_INFO("config: wifi_ap_config: max_connections: %u", (printf_uint_t)p_wifi_cfg_ap->max_connection);
    LOG_INFO("config: wifi_ap_config: beacon_interval: %u", (printf_uint_t)p_wifi_cfg_ap->beacon_interval);
}

static void
gw_cfg_log_wifi_settings_ap(const wifi_settings_ap_t* const p_settings_ap)
{
    LOG_INFO("config: wifi_ap_settings: bandwidth: %s", gw_cfg_log_wifi_bandwidth_to_str(p_settings_ap->ap_bandwidth));
    LOG_INFO("config: wifi_ap_settings: IP: %s", p_settings_ap->ap_ip.buf);
    LOG_INFO("config: wifi_ap_settings: GW: %s", p_settings_ap->ap_gw.buf);
    LOG_INFO("config: wifi_ap_settings: Netmask: %s", p_settings_ap->ap_netmask.buf);
}

static void
gw_cfg_log_wifi_config_sta(const wifi_sta_config_t* const p_wifi_cfg_sta)
{
    LOG_INFO(
        "config: wifi_sta_config: SSID:'%s', password:'%s'",
        p_wifi_cfg_sta->ssid,
        gw_cfg_log_get_sta_password_for_logging(p_wifi_cfg_sta));

    LOG_INFO(
        "config: wifi_sta_config: scan_method: %s",
        (WIFI_FAST_SCAN == p_wifi_cfg_sta->scan_method) ? "Fast" : "All");
    if (p_wifi_cfg_sta->bssid_set)
    {
        LOG_INFO(
            "config: wifi_sta_config: Use BSSID: %02x:%02x:%02x:%02x:%02x:%02x",
            p_wifi_cfg_sta->bssid[0],
            p_wifi_cfg_sta->bssid[1],
            p_wifi_cfg_sta->bssid[2],
            p_wifi_cfg_sta->bssid[3],
            p_wifi_cfg_sta->bssid[4],
            p_wifi_cfg_sta->bssid[5]);
    }
    else
    {
        LOG_INFO("config: wifi_sta_config: Use BSSID: %s", "false");
    }
    LOG_INFO("config: wifi_sta_config: Channel: %u", (printf_uint_t)p_wifi_cfg_sta->channel);
    LOG_INFO("config: wifi_sta_config: Listen interval: %u", (printf_uint_t)p_wifi_cfg_sta->listen_interval);
    LOG_INFO(
        "config: wifi_sta_config: Sort method: %s",
        (WIFI_CONNECT_AP_BY_SIGNAL == p_wifi_cfg_sta->sort_method) ? "by RSSI" : "by security mode");
    LOG_INFO("config: wifi_sta_config: Fast scan: min RSSI: %d", (printf_int_t)p_wifi_cfg_sta->threshold.rssi);
    LOG_INFO(
        "config: wifi_sta_config: Fast scan: weakest auth mode: %d",
        (printf_int_t)p_wifi_cfg_sta->threshold.authmode);
    LOG_INFO(
        "config: wifi_sta_config: Protected Management Frame: Capable: %s",
        p_wifi_cfg_sta->pmf_cfg.capable ? "true" : "false");
    LOG_INFO(
        "config: wifi_sta_config: Protected Management Frame: Required: %s",
        p_wifi_cfg_sta->pmf_cfg.required ? "true" : "false");
}

static const char*
gw_cfg_log_wifi_ps_type_to_str(const wifi_ps_type_t ps_type)
{
    switch (ps_type)
    {
        case WIFI_PS_NONE:
            return "NONE";
        case WIFI_PS_MIN_MODEM:
            return "MIN_MODEM";
        case WIFI_PS_MAX_MODEM:
            return "MAX_MODEM";
    }
    assert(0);
    return "Unknown";
}

static void
gw_cfg_log_wifi_settings_sta(const wifi_settings_sta_t* const p_settings_sta)
{
    LOG_INFO(
        "config: wifi_sta_settings: Power save: %s",
        gw_cfg_log_wifi_ps_type_to_str(p_settings_sta->sta_power_save));
    LOG_INFO("config: wifi_sta_settings: Use Static IP: %s", p_settings_sta->sta_static_ip ? "yes" : "no");
    if (p_settings_sta->sta_static_ip)
    {
        wifiman_ip4_addr_str_t ip_str      = { 0 };
        wifiman_ip4_addr_str_t gw_str      = { 0 };
        wifiman_ip4_addr_str_t netmask_str = { 0 };
        esp_ip4addr_ntoa(&p_settings_sta->sta_static_ip_config.ip, ip_str.buf, sizeof(ip_str.buf));
        esp_ip4addr_ntoa(&p_settings_sta->sta_static_ip_config.gw, gw_str.buf, sizeof(gw_str.buf));
        esp_ip4addr_ntoa(&p_settings_sta->sta_static_ip_config.netmask, netmask_str.buf, sizeof(netmask_str.buf));
        LOG_INFO(
            "config: wifi_sta_settings: Static IP: IP: %s , GW: %s , Mask: %s",
            ip_str.buf,
            gw_str.buf,
            netmask_str.buf);
    }
}

void
gw_cfg_log_wifi_cfg_ap(const wifiman_config_ap_t* const p_wifi_cfg_ap, const char* const p_title)
{
    if (NULL != p_title)
    {
        LOG_INFO("%s", p_title);
    }
    gw_cfg_log_wifi_config_ap(&p_wifi_cfg_ap->wifi_config_ap);
    gw_cfg_log_wifi_settings_ap(&p_wifi_cfg_ap->wifi_settings_ap);
}

void
gw_cfg_log_wifi_cfg_sta(const wifiman_config_sta_t* const p_wifi_cfg_sta, const char* const p_title)
{
    if (NULL != p_title)
    {
        LOG_INFO("%s", p_title);
    }
    gw_cfg_log_wifi_config_sta(&p_wifi_cfg_sta->wifi_config_sta);
    gw_cfg_log_wifi_settings_sta(&p_wifi_cfg_sta->wifi_settings_sta);
}

void
gw_cfg_log_device_info(const gw_cfg_device_info_t* const p_dev_info, const char* const p_title)
{
    if (NULL != p_title)
    {
        LOG_INFO("%s", p_title);
    }
    LOG_INFO("config: device_info: WiFi AP SSID / Hostname: %s", p_dev_info->wifi_ap_hostname.ssid_buf);
    LOG_INFO("config: device_info: ESP32 fw ver: %s", p_dev_info->esp32_fw_ver.buf);
    LOG_INFO("config: device_info: ESP32 WiFi MAC ADDR: %s", p_dev_info->esp32_mac_addr_wifi.str_buf);
    LOG_INFO("config: device_info: ESP32 Eth MAC ADDR: %s", p_dev_info->esp32_mac_addr_eth.str_buf);
    LOG_INFO("config: device_info: NRF52 fw ver: %s", p_dev_info->nrf52_fw_ver.buf);
    LOG_INFO("config: device_info: NRF52 MAC ADDR: %s", p_dev_info->nrf52_mac_addr.str_buf);
    LOG_INFO("config: device_info: NRF52 DEVICE ID: %s", p_dev_info->nrf52_device_id.str_buf);
}

void
gw_cfg_log_eth_cfg(const gw_cfg_eth_t* const p_gw_cfg_eth, const char* const p_title)
{
    if (NULL != p_title)
    {
        LOG_INFO("%s", p_title);
    }
    LOG_INFO("config: Use eth: %s", p_gw_cfg_eth->use_eth ? "yes" : "no");
    LOG_INFO("config: eth: use DHCP: %s", p_gw_cfg_eth->eth_dhcp ? "yes" : "no");
    LOG_INFO("config: eth: static IP: %s", p_gw_cfg_eth->eth_static_ip.buf);
    LOG_INFO("config: eth: netmask: %s", p_gw_cfg_eth->eth_netmask.buf);
    LOG_INFO("config: eth: GW: %s", p_gw_cfg_eth->eth_gw.buf);
    LOG_INFO("config: eth: DNS1: %s", p_gw_cfg_eth->eth_dns1.buf);
    LOG_INFO("config: eth: DNS2: %s", p_gw_cfg_eth->eth_dns2.buf);
}

static void
gw_cfg_log_ruuvi_cfg_remote(const ruuvi_gw_cfg_remote_t* const p_remote)
{
    LOG_INFO("config: use remote cfg: %d", p_remote->use_remote_cfg);
    LOG_INFO("config: remote cfg: URL: %s", p_remote->url.buf);
    switch (p_remote->auth_type)
    {
        case GW_CFG_REMOTE_AUTH_TYPE_NO:
            LOG_INFO("config: remote cfg: auth_type: %s", GW_CFG_REMOTE_AUTH_TYPE_STR_NO);
            break;
        case GW_CFG_REMOTE_AUTH_TYPE_BASIC:
            LOG_INFO("config: remote cfg: auth_type: %s", GW_CFG_REMOTE_AUTH_TYPE_STR_BASIC);
            LOG_INFO("config: remote cfg: auth user: %s", p_remote->auth.auth_basic.user.buf);
#if LOG_LOCAL_LEVEL >= LOG_LEVEL_DEBUG
            LOG_DBG("config: remote cfg: auth pass: %s", p_remote->auth.auth_basic.password.buf);
#else
            LOG_INFO("config: remote cfg: auth pass: %s", "********");
#endif
            break;
        case GW_CFG_REMOTE_AUTH_TYPE_BEARER:
            LOG_INFO("config: remote cfg: auth_type: %s", GW_CFG_REMOTE_AUTH_TYPE_STR_BEARER);
#if LOG_LOCAL_LEVEL >= LOG_LEVEL_DEBUG
            LOG_DBG("config: remote cfg: auth bearer token: %s", p_remote->auth.auth_bearer.token.buf);
#else
            LOG_INFO("config: remote cfg: auth bearer token: %s", "********");
#endif
            break;
    }
    LOG_INFO("config: remote cfg: refresh_interval_minutes: %u", (printf_uint_t)p_remote->refresh_interval_minutes);
}

static void
gw_cfg_log_ruuvi_cfg_http(const ruuvi_gw_cfg_http_t* const p_http)
{
    LOG_INFO("config: use http: %d", p_http->use_http);
    LOG_INFO("config: http url: %s", p_http->http_url.buf);
    LOG_INFO("config: http user: %s", p_http->http_user.buf);
#if LOG_LOCAL_LEVEL >= LOG_LEVEL_DEBUG
    LOG_DBG("config: http pass: %s", p_http->http_pass.buf);
#else
    LOG_INFO("config: http pass: %s", "********");
#endif
}

static void
gw_cfg_log_ruuvi_cfg_http_stat(const ruuvi_gw_cfg_http_stat_t* const p_http_stat)
{
    LOG_INFO("config: use http_stat: %d", p_http_stat->use_http_stat);
    LOG_INFO("config: http_stat url: %s", p_http_stat->http_stat_url.buf);
    LOG_INFO("config: http_stat user: %s", p_http_stat->http_stat_user.buf);
#if LOG_LOCAL_LEVEL >= LOG_LEVEL_DEBUG
    LOG_DBG("config: http_stat pass: %s", p_http_stat->http_stat_pass.buf);
#else
    LOG_INFO("config: http_stat pass: %s", "********");
#endif
}

static void
gw_cfg_log_ruuvi_cfg_mqtt(const ruuvi_gw_cfg_mqtt_t* const p_mqtt)
{
    LOG_INFO("config: use mqtt: %d", p_mqtt->use_mqtt);
    LOG_INFO("config: mqtt disable retained messages: %d", p_mqtt->mqtt_disable_retained_messages);
    LOG_INFO("config: mqtt transport: %s", p_mqtt->mqtt_transport.buf);
    LOG_INFO("config: mqtt server: %s", p_mqtt->mqtt_server.buf);
    LOG_INFO("config: mqtt port: %u", p_mqtt->mqtt_port);
    LOG_INFO("config: mqtt prefix: %s", p_mqtt->mqtt_prefix.buf);
    LOG_INFO("config: mqtt client id: %s", p_mqtt->mqtt_client_id.buf);
    LOG_INFO("config: mqtt user: %s", p_mqtt->mqtt_user.buf);
#if LOG_LOCAL_LEVEL >= LOG_LEVEL_DEBUG
    LOG_DBG("config: mqtt password: %s", p_mqtt->mqtt_pass.buf);
#else
    LOG_INFO("config: mqtt password: %s", "********");
#endif
}

static void
gw_cfg_log_ruuvi_cfg_lan_auth(const ruuvi_gw_cfg_lan_auth_t* const p_lan_auth)
{
    LOG_INFO("config: LAN auth type: %s", gw_cfg_auth_type_to_str(p_lan_auth));
    LOG_INFO("config: LAN auth user: %s", p_lan_auth->lan_auth_user.buf);
#if LOG_LOCAL_LEVEL >= LOG_LEVEL_DEBUG
    LOG_DBG("config: LAN auth pass: %s", p_lan_auth->lan_auth_pass.buf);
#else
    LOG_INFO("config: LAN auth pass: %s", "********");
#endif
#if LOG_LOCAL_LEVEL >= LOG_LEVEL_DEBUG
    LOG_DBG("config: LAN auth API key: %s", p_lan_auth->lan_auth_api_key.buf);
#else
    LOG_INFO("config: LAN auth API key: %s", "********");
#endif
}

static void
gw_cfg_log_ruuvi_cfg_auto_update(const ruuvi_gw_cfg_auto_update_t* const p_auto_update)
{
    switch (p_auto_update->auto_update_cycle)
    {
        case AUTO_UPDATE_CYCLE_TYPE_REGULAR:
            LOG_INFO("config: Auto update cycle: %s", AUTO_UPDATE_CYCLE_TYPE_STR_REGULAR);
            break;
        case AUTO_UPDATE_CYCLE_TYPE_BETA_TESTER:
            LOG_INFO("config: Auto update cycle: %s", AUTO_UPDATE_CYCLE_TYPE_STR_BETA_TESTER);
            break;
        case AUTO_UPDATE_CYCLE_TYPE_MANUAL:
            LOG_INFO("config: Auto update cycle: %s", AUTO_UPDATE_CYCLE_TYPE_STR_MANUAL);
            break;
        default:
            LOG_INFO("config: Auto update cycle: Unknown (%d)", p_auto_update->auto_update_cycle);
            break;
    }
    LOG_INFO("config: Auto update weekdays_bitmask: 0x%02x", p_auto_update->auto_update_weekdays_bitmask);
    LOG_INFO(
        "config: Auto update interval: %02u:00..%02u:00",
        p_auto_update->auto_update_interval_from,
        p_auto_update->auto_update_interval_to);
    LOG_INFO(
        "config: Auto update TZ: UTC%s%d",
        ((p_auto_update->auto_update_tz_offset_hours < 0) ? "" : "+"),
        (printf_int_t)p_auto_update->auto_update_tz_offset_hours);
}

static void
gw_cfg_log_ruuvi_cfg_ntp(const ruuvi_gw_cfg_ntp_t* const p_ntp)
{
    LOG_INFO("config: NTP: Use: %s", p_ntp->ntp_use ? "yes" : "no");
    LOG_INFO("config: NTP: Use DHCP: %s", p_ntp->ntp_use_dhcp ? "yes" : "no");
    LOG_INFO("config: NTP: Server1: %s", p_ntp->ntp_server1.buf);
    LOG_INFO("config: NTP: Server2: %s", p_ntp->ntp_server2.buf);
    LOG_INFO("config: NTP: Server3: %s", p_ntp->ntp_server3.buf);
    LOG_INFO("config: NTP: Server4: %s", p_ntp->ntp_server4.buf);
}

static void
gw_cfg_log_ruuvi_cfg_filter(const ruuvi_gw_cfg_filter_t* const p_filter)
{
    LOG_INFO("config: use company id filter: %d", p_filter->company_use_filtering);
    LOG_INFO("config: company id: 0x%04x", p_filter->company_id);
}

static void
gw_cfg_log_ruuvi_cfg_scan(const ruuvi_gw_cfg_scan_t* const p_scan)
{
    LOG_INFO("config: use scan coded phy: %d", p_scan->scan_coded_phy);
    LOG_INFO("config: use scan 1mbit/phy: %d", p_scan->scan_1mbit_phy);
    LOG_INFO("config: use scan extended payload: %d", p_scan->scan_extended_payload);
    LOG_INFO("config: use scan channel 37: %d", p_scan->scan_channel_37);
    LOG_INFO("config: use scan channel 38: %d", p_scan->scan_channel_38);
    LOG_INFO("config: use scan channel 39: %d", p_scan->scan_channel_39);
}

void
gw_cfg_log_ruuvi_cfg(const gw_cfg_ruuvi_t* const p_gw_cfg_ruuvi, const char* const p_title)
{
    if (NULL != p_title)
    {
        LOG_INFO("%s", p_title);
    }

    gw_cfg_log_ruuvi_cfg_remote(&p_gw_cfg_ruuvi->remote);
    gw_cfg_log_ruuvi_cfg_http(&p_gw_cfg_ruuvi->http);
    gw_cfg_log_ruuvi_cfg_http_stat(&p_gw_cfg_ruuvi->http_stat);
    gw_cfg_log_ruuvi_cfg_mqtt(&p_gw_cfg_ruuvi->mqtt);
    gw_cfg_log_ruuvi_cfg_lan_auth(&p_gw_cfg_ruuvi->lan_auth);
    gw_cfg_log_ruuvi_cfg_auto_update(&p_gw_cfg_ruuvi->auto_update);
    gw_cfg_log_ruuvi_cfg_ntp(&p_gw_cfg_ruuvi->ntp);
    gw_cfg_log_ruuvi_cfg_filter(&p_gw_cfg_ruuvi->filter);
    gw_cfg_log_ruuvi_cfg_scan(&p_gw_cfg_ruuvi->scan);

    LOG_INFO("config: coordinates: %s", p_gw_cfg_ruuvi->coordinates.buf);
}

void
gw_cfg_log(const gw_cfg_t* const p_gw_cfg, const char* const p_title, const bool flag_log_device_info)
{
    LOG_INFO("%s:", p_title);
    if (flag_log_device_info)
    {
        gw_cfg_log_device_info(&p_gw_cfg->device_info, NULL);
    }
    gw_cfg_log_eth_cfg(&p_gw_cfg->eth_cfg, NULL);
    gw_cfg_log_ruuvi_cfg(&p_gw_cfg->ruuvi_cfg, NULL);
    gw_cfg_log_wifi_cfg_ap(&p_gw_cfg->wifi_cfg.ap, NULL);
    gw_cfg_log_wifi_cfg_sta(&p_gw_cfg->wifi_cfg.sta, NULL);
}
