/**
 * @file gw_cfg.h
 * @author TheSomeMan
 * @date 2020-10-31
 * @copyright Ruuvi Innovations Ltd, license BSD-3-Clause.
 */

#include "gw_cfg.h"
#include "gw_cfg_default.h"
#include <string.h>
#include <stdio.h>
#include "os_mutex_recursive.h"

// Warning: Debug log level prints out the passwords as a "plaintext" so accidents won't happen.
#define LOG_LOCAL_LEVEL LOG_LEVEL_INFO
#include "log.h"

mac_address_bin_t g_gw_mac_eth      = { 0 };
mac_address_str_t g_gw_mac_eth_str  = { 0 };
mac_address_bin_t g_gw_mac_wifi     = { 0 };
mac_address_str_t g_gw_mac_wifi_str = { 0 };
mac_address_bin_t g_gw_mac_sta      = { 0 };
mac_address_str_t g_gw_mac_sta_str  = { 0 };
wifi_ssid_t       g_gw_wifi_ssid    = {
    .ssid_buf = DEFAULT_AP_SSID, // RuuviGatewayXXXX where XXXX - last 4 digits of the MAC-address
};

static ruuvi_gateway_config_t      g_gateway_config = { 0 };
static os_mutex_recursive_t        g_gw_cfg_mutex;
static os_mutex_recursive_static_t g_gw_cfg_mutex_mem;

static const char TAG[] = "gw_cfg";

void
gw_cfg_init(void)
{
    g_gw_cfg_mutex = os_mutex_recursive_create_static(&g_gw_cfg_mutex_mem);
    os_mutex_recursive_lock(g_gw_cfg_mutex);
    g_gateway_config = g_gateway_config_default;
    memset(&g_gw_mac_eth, 0, sizeof(g_gw_mac_eth));
    memset(&g_gw_mac_eth_str, 0, sizeof(g_gw_mac_eth_str));
    memset(&g_gw_mac_wifi, 0, sizeof(g_gw_mac_wifi));
    memset(&g_gw_mac_wifi_str, 0, sizeof(g_gw_mac_wifi_str));
    memset(&g_gw_mac_sta, 0, sizeof(g_gw_mac_sta));
    memset(&g_gw_mac_sta_str, 0, sizeof(g_gw_mac_sta_str));
    memset(&g_gw_wifi_ssid, 0, sizeof(g_gw_wifi_ssid));
    snprintf(&g_gw_wifi_ssid.ssid_buf[0], sizeof(g_gw_wifi_ssid.ssid_buf), "%s", DEFAULT_AP_SSID);
    os_mutex_recursive_unlock(g_gw_cfg_mutex);
}

ruuvi_gateway_config_t *
gw_cfg_lock_rw(void)
{
    os_mutex_recursive_lock(g_gw_cfg_mutex);
    return &g_gateway_config;
}

void
gw_cfg_unlock_rw(ruuvi_gateway_config_t **const p_p_gw_cfg)
{
    *p_p_gw_cfg = NULL;
    os_mutex_recursive_unlock(g_gw_cfg_mutex);
}

const ruuvi_gateway_config_t *
gw_cfg_lock_ro(void)
{
    os_mutex_recursive_lock(g_gw_cfg_mutex);
    return &g_gateway_config;
}

void
gw_cfg_unlock_ro(const ruuvi_gateway_config_t **const p_p_gw_cfg)
{
    *p_p_gw_cfg = NULL;
    os_mutex_recursive_unlock(g_gw_cfg_mutex);
}

void
gw_cfg_print_to_log(const ruuvi_gateway_config_t *const p_config)
{
    LOG_INFO("Gateway SETTINGS:");
    LOG_INFO("config: use eth: %d", p_config->eth.use_eth);
    LOG_INFO("config: use eth dhcp: %d", p_config->eth.eth_dhcp);
    LOG_INFO("config: eth static ip: %s", p_config->eth.eth_static_ip.buf);
    LOG_INFO("config: eth netmask: %s", p_config->eth.eth_netmask.buf);
    LOG_INFO("config: eth gw: %s", p_config->eth.eth_gw.buf);
    LOG_INFO("config: eth dns1: %s", p_config->eth.eth_dns1.buf);
    LOG_INFO("config: eth dns2: %s", p_config->eth.eth_dns2.buf);
    LOG_INFO("config: use mqtt: %d", p_config->mqtt.use_mqtt);
    LOG_INFO("config: mqtt transport: %s", p_config->mqtt.mqtt_transport.buf);
    LOG_INFO("config: mqtt server: %s", p_config->mqtt.mqtt_server.buf);
    LOG_INFO("config: mqtt port: %u", p_config->mqtt.mqtt_port);
    LOG_INFO("config: mqtt use default prefix: %d", p_config->mqtt.mqtt_use_default_prefix);
    LOG_INFO("config: mqtt prefix: %s", p_config->mqtt.mqtt_prefix.buf);
    LOG_INFO("config: mqtt client id: %s", p_config->mqtt.mqtt_client_id.buf);
    LOG_INFO("config: mqtt user: %s", p_config->mqtt.mqtt_user.buf);
    LOG_INFO("config: mqtt password: %s", "********");
    LOG_DBG("config: mqtt password: %s", p_config->mqtt.mqtt_pass.buf);
    LOG_INFO("config: use http: %d", p_config->http.use_http);
    LOG_INFO("config: http url: %s", p_config->http.http_url.buf);
    LOG_INFO("config: http user: %s", p_config->http.http_user.buf);
    LOG_INFO("config: http pass: %s", "********");
    LOG_DBG("config: http pass: %s", p_config->http.http_pass.buf);
    LOG_INFO("config: use http_stat: %d", p_config->http_stat.use_http_stat);
    LOG_INFO("config: http_stat url: %s", p_config->http_stat.http_stat_url.buf);
    LOG_INFO("config: http_stat user: %s", p_config->http_stat.http_stat_user.buf);
    LOG_DBG("config: http_stat pass: %s", p_config->http_stat.http_stat_pass.buf);
    LOG_INFO("config: http_stat pass: %s", "********");
    LOG_INFO("config: LAN auth type: %s", p_config->lan_auth.lan_auth_type);
    LOG_INFO("config: LAN auth user: %s", p_config->lan_auth.lan_auth_user);
    LOG_INFO("config: LAN auth pass: %s", "********");

    switch (p_config->auto_update.auto_update_cycle)
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
            LOG_INFO(
                "config: Auto update cycle: %s (%d)",
                AUTO_UPDATE_CYCLE_TYPE_STR_MANUAL,
                p_config->auto_update.auto_update_cycle);
            break;
    }
    LOG_INFO("config: Auto update weekdays_bitmask: 0x%02x", p_config->auto_update.auto_update_weekdays_bitmask);
    LOG_INFO(
        "config: Auto update interval: %02u:00..%02u:00",
        p_config->auto_update.auto_update_interval_from,
        p_config->auto_update.auto_update_interval_to);
    LOG_INFO(
        "config: Auto update TZ: UTC%s%d",
        ((p_config->auto_update.auto_update_tz_offset_hours < 0) ? "" : "+"),
        (printf_int_t)p_config->auto_update.auto_update_tz_offset_hours);

    LOG_INFO("config: coordinates: %s", p_config->coordinates.buf);
    LOG_INFO("config: use company id filter: %d", p_config->filter.company_use_filtering);
    LOG_INFO("config: company id: 0x%04x", p_config->filter.company_id);
    LOG_INFO("config: use scan coded phy: %d", p_config->scan.scan_coded_phy);
    LOG_INFO("config: use scan 1mbit/phy: %d", p_config->scan.scan_1mbit_phy);
    LOG_INFO("config: use scan extended payload: %d", p_config->scan.scan_extended_payload);
    LOG_INFO("config: use scan channel 37: %d", p_config->scan.scan_channel_37);
    LOG_INFO("config: use scan channel 38: %d", p_config->scan.scan_channel_38);
    LOG_INFO("config: use scan channel 39: %d", p_config->scan.scan_channel_39);
}

void
gw_cfg_update(const ruuvi_gateway_config_t *const p_gw_cfg_new, const bool flag_network_cfg)
{
    ruuvi_gateway_config_t *p_gw_cfg = gw_cfg_lock_rw();
    if (flag_network_cfg)
    {
        p_gw_cfg->eth = p_gw_cfg_new->eth;
    }
    else
    {
        const ruuvi_gw_cfg_eth_t eth_saved = p_gw_cfg->eth;
        *p_gw_cfg                          = *p_gw_cfg_new;
        p_gw_cfg->eth                      = eth_saved;
    }
    gw_cfg_unlock_rw(&p_gw_cfg);
}

void
gw_cfg_get_copy(ruuvi_gateway_config_t *const p_gw_cfg_dst)
{
    const ruuvi_gateway_config_t *p_gw_cfg = gw_cfg_lock_ro();
    *p_gw_cfg_dst                          = *p_gw_cfg;
    gw_cfg_unlock_ro(&p_gw_cfg);
}

bool
gw_cfg_get_eth_use_eth(void)
{
    const ruuvi_gateway_config_t *p_gw_cfg = gw_cfg_lock_ro();
    const bool                    use_eth  = p_gw_cfg->eth.use_eth;
    gw_cfg_unlock_ro(&p_gw_cfg);
    return use_eth;
}

bool
gw_cfg_get_eth_use_dhcp(void)
{
    const ruuvi_gateway_config_t *p_gw_cfg = gw_cfg_lock_ro();
    const bool                    use_dhcp = p_gw_cfg->eth.eth_dhcp;
    gw_cfg_unlock_ro(&p_gw_cfg);
    return use_dhcp;
}

bool
gw_cfg_get_mqtt_use_mqtt(void)
{
    const ruuvi_gateway_config_t *p_gw_cfg = gw_cfg_lock_ro();
    const bool                    use_mqtt = p_gw_cfg->mqtt.use_mqtt;
    gw_cfg_unlock_ro(&p_gw_cfg);
    return use_mqtt;
}

bool
gw_cfg_get_http_use_http(void)
{
    const ruuvi_gateway_config_t *p_gw_cfg = gw_cfg_lock_ro();
    const bool                    use_http = p_gw_cfg->http.use_http;
    gw_cfg_unlock_ro(&p_gw_cfg);
    return use_http;
}

bool
gw_cfg_get_http_stat_use_http_stat(void)
{
    const ruuvi_gateway_config_t *p_gw_cfg = gw_cfg_lock_ro();
    const bool                    use_http = p_gw_cfg->http_stat.use_http_stat;
    gw_cfg_unlock_ro(&p_gw_cfg);
    return use_http;
}

ruuvi_gw_cfg_mqtt_prefix_t
gw_cfg_get_mqtt_prefix(void)
{
    const ruuvi_gateway_config_t *   p_gw_cfg    = gw_cfg_lock_ro();
    const ruuvi_gw_cfg_mqtt_prefix_t mqtt_prefix = p_gw_cfg->mqtt.mqtt_prefix;
    gw_cfg_unlock_ro(&p_gw_cfg);
    return mqtt_prefix;
}

auto_update_cycle_type_e
gw_cfg_get_auto_update_cycle(void)
{
    const ruuvi_gateway_config_t * p_gw_cfg          = gw_cfg_lock_ro();
    const auto_update_cycle_type_e auto_update_cycle = p_gw_cfg->auto_update.auto_update_cycle;
    gw_cfg_unlock_ro(&p_gw_cfg);
    return auto_update_cycle;
}

ruuvi_gw_cfg_lan_auth_t
gw_cfg_get_lan_auth(void)
{
    const ruuvi_gateway_config_t *p_gw_cfg = gw_cfg_lock_ro();
    const ruuvi_gw_cfg_lan_auth_t lan_auth = p_gw_cfg->lan_auth;
    gw_cfg_unlock_ro(&p_gw_cfg);
    return lan_auth;
}

ruuvi_gw_cfg_coordinates_t
gw_cfg_get_coordinates(void)
{
    const ruuvi_gateway_config_t *   p_gw_cfg    = gw_cfg_lock_ro();
    const ruuvi_gw_cfg_coordinates_t coordinates = p_gw_cfg->coordinates;
    gw_cfg_unlock_ro(&p_gw_cfg);
    return coordinates;
}
