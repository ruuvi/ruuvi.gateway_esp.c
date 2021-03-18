/**
 * @file gw_cfg.h
 * @author TheSomeMan
 * @date 2020-10-31
 * @copyright Ruuvi Innovations Ltd, license BSD-3-Clause.
 */

#include "gw_cfg.h"
#include <stdio.h>

#define LOG_LOCAL_LEVEL LOG_LEVEL_DEBUG
#include "log.h"

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
            .mqtt_user = { 0 }, \
            .mqtt_pass = { 0 },                 \
        }, \
        .http = { \
            .use_http = true, \
            .http_url = { "https://network.ruuvi.com/record" }, \
            .http_user = { 0 }, \
            .http_pass = { 0 }, \
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

ruuvi_gateway_config_t       g_gateway_config         = RUUVI_GATEWAY_DEFAULT_CONFIGURATION;
const ruuvi_gateway_config_t g_gateway_config_default = RUUVI_GATEWAY_DEFAULT_CONFIGURATION;
mac_address_bin_t            g_gw_mac_sta             = { 0 };
mac_address_str_t            g_gw_mac_sta_str         = { 0 };

static const char TAG[] = "gw_cfg";

void
gw_cfg_print_to_log(const ruuvi_gateway_config_t *p_config)
{
    LOG_INFO("Gateway SETTINGS:");
    LOG_INFO("config: use eth: %d", p_config->eth.use_eth);
    LOG_INFO("config: use eth dhcp: %d", p_config->eth.eth_dhcp);
    LOG_INFO("config: eth static ip: %s", p_config->eth.eth_static_ip);
    LOG_INFO("config: eth netmask: %s", p_config->eth.eth_netmask);
    LOG_INFO("config: eth gw: %s", p_config->eth.eth_gw);
    LOG_INFO("config: eth dns1: %s", p_config->eth.eth_dns1);
    LOG_INFO("config: eth dns2: %s", p_config->eth.eth_dns2);
    LOG_INFO("config: use mqtt: %d", p_config->mqtt.use_mqtt);
    LOG_INFO("config: mqtt server: %s", p_config->mqtt.mqtt_server);
    LOG_INFO("config: mqtt port: %u", p_config->mqtt.mqtt_port);
    LOG_INFO("config: mqtt prefix: %s", p_config->mqtt.mqtt_prefix);
    LOG_INFO("config: mqtt user: %s", p_config->mqtt.mqtt_user);
    LOG_INFO("config: mqtt password: %s", "********");
    LOG_INFO("config: use http: %d", p_config->http.use_http);
    LOG_INFO("config: http url: %s", p_config->http.http_url);
    LOG_INFO("config: http user: %s", p_config->http.http_user);
    LOG_INFO("config: http pass: %s", "********");
    LOG_INFO("config: coordinates: %s", p_config->coordinates);
    LOG_INFO("config: use company id filter: %d", p_config->filter.company_filter);
    LOG_INFO("config: company id: 0x%04x", p_config->filter.company_id);
    LOG_INFO("config: use scan coded phy: %d", p_config->scan.scan_coded_phy);
    LOG_INFO("config: use scan 1mbit/phy: %d", p_config->scan.scan_1mbit_phy);
    LOG_INFO("config: use scan extended payload: %d", p_config->scan.scan_extended_payload);
    LOG_INFO("config: use scan channel 37: %d", p_config->scan.scan_channel_37);
    LOG_INFO("config: use scan channel 38: %d", p_config->scan.scan_channel_38);
    LOG_INFO("config: use scan channel 39: %d", p_config->scan.scan_channel_39);
}

static bool
gw_cfg_json_add_bool(cJSON *p_json_root, const char *p_item_name, const bool val)
{
    if (NULL == cJSON_AddBoolToObject(p_json_root, p_item_name, val))
    {
        LOG_ERR("Can't add json item: %s", p_item_name);
        return false;
    }
    return true;
}

static bool
gw_cfg_json_add_string(cJSON *p_json_root, const char *p_item_name, const char *p_val)
{
    if (NULL == cJSON_AddStringToObject(p_json_root, p_item_name, p_val))
    {
        LOG_ERR("Can't add json item: %s", p_item_name);
        return false;
    }
    return true;
}

static bool
gw_cfg_json_add_number(cJSON *p_json_root, const char *p_item_name, const cjson_number_t val)
{
    if (NULL == cJSON_AddNumberToObject(p_json_root, p_item_name, val))
    {
        LOG_ERR("Can't add json item: %s", p_item_name);
        return false;
    }
    return true;
}

static bool
gw_cfg_json_add_items_eth(cJSON *p_json_root, const ruuvi_gateway_config_t *p_cfg)
{
    if (!gw_cfg_json_add_bool(p_json_root, "eth_dhcp", p_cfg->eth.eth_dhcp))
    {
        return false;
    }
    if (!gw_cfg_json_add_string(p_json_root, "eth_static_ip", p_cfg->eth.eth_static_ip))
    {
        return false;
    }
    if (!gw_cfg_json_add_string(p_json_root, "eth_netmask", p_cfg->eth.eth_netmask))
    {
        return false;
    }
    if (!gw_cfg_json_add_string(p_json_root, "eth_gw", p_cfg->eth.eth_gw))
    {
        return false;
    }
    if (!gw_cfg_json_add_string(p_json_root, "eth_dns1", p_cfg->eth.eth_dns1))
    {
        return false;
    }
    if (!gw_cfg_json_add_string(p_json_root, "eth_dns2", p_cfg->eth.eth_dns2))
    {
        return false;
    }
    return true;
}

static bool
gw_cfg_json_add_items_http(cJSON *p_json_root, const ruuvi_gateway_config_t *p_cfg)
{
    if (!gw_cfg_json_add_bool(p_json_root, "use_http", p_cfg->http.use_http))
    {
        return false;
    }
    if (!gw_cfg_json_add_string(p_json_root, "http_url", p_cfg->http.http_url))
    {
        return false;
    }
    if (!gw_cfg_json_add_string(p_json_root, "http_user", p_cfg->http.http_user))
    {
        return false;
    }
    return true;
}

static bool
gw_cfg_json_add_items_mqtt(cJSON *p_json_root, const ruuvi_gateway_config_t *p_cfg)
{
    if (!gw_cfg_json_add_bool(p_json_root, "use_mqtt", p_cfg->mqtt.use_mqtt))
    {
        return false;
    }
    if (!gw_cfg_json_add_string(p_json_root, "mqtt_server", p_cfg->mqtt.mqtt_server))
    {
        return false;
    }
    if (!gw_cfg_json_add_number(p_json_root, "mqtt_port", p_cfg->mqtt.mqtt_port))
    {
        return false;
    }
    if (!gw_cfg_json_add_string(p_json_root, "mqtt_prefix", p_cfg->mqtt.mqtt_prefix))
    {
        return false;
    }
    if (!gw_cfg_json_add_string(p_json_root, "mqtt_user", p_cfg->mqtt.mqtt_user))
    {
        return false;
    }
#if 0
    // Don't send to browser because of security
    if (!gw_cfg_json_add_string(p_json_root, "mqtt_pass", p_cfg->mqtt_pass))
    {
        return false;
    }
#endif
    return true;
}

static bool
gw_cfg_json_add_items_filter(cJSON *p_json_root, const ruuvi_gateway_config_t *p_cfg)
{
    if (!gw_cfg_json_add_bool(p_json_root, "use_filtering", p_cfg->filter.company_filter))
    {
        return false;
    }
    char company_id[10];
    snprintf(company_id, sizeof(company_id), "0x%04x", p_cfg->filter.company_id);
    if (!gw_cfg_json_add_string(p_json_root, "company_id", company_id))
    {
        return false;
    }
    return true;
}

static bool
gw_cfg_json_add_items_scan(cJSON *p_json_root, const ruuvi_gateway_config_t *p_cfg)
{
    if (!gw_cfg_json_add_bool(p_json_root, "use_coded_phy", p_cfg->scan.scan_coded_phy))
    {
        return false;
    }
    if (!gw_cfg_json_add_bool(p_json_root, "use_1mbit_phy", p_cfg->scan.scan_1mbit_phy))
    {
        return false;
    }
    if (!gw_cfg_json_add_bool(p_json_root, "use_extended_payload", p_cfg->scan.scan_extended_payload))
    {
        return false;
    }
    if (!gw_cfg_json_add_bool(p_json_root, "use_channel_37", p_cfg->scan.scan_channel_37))
    {
        return false;
    }
    if (!gw_cfg_json_add_bool(p_json_root, "use_channel_38", p_cfg->scan.scan_channel_38))
    {
        return false;
    }
    if (!gw_cfg_json_add_bool(p_json_root, "use_channel_39", p_cfg->scan.scan_channel_39))
    {
        return false;
    }
    return true;
}

static bool
gw_cfg_json_add_items(cJSON *p_json_root, const ruuvi_gateway_config_t *p_cfg, const mac_address_str_t *p_mac_sta)
{
    if (!gw_cfg_json_add_items_eth(p_json_root, p_cfg))
    {
        return false;
    }
    if (!gw_cfg_json_add_items_http(p_json_root, p_cfg))
    {
        return false;
    }
    if (!gw_cfg_json_add_items_mqtt(p_json_root, p_cfg))
    {
        return false;
    }
    if (!gw_cfg_json_add_string(p_json_root, "gw_mac", p_mac_sta->str_buf))
    {
        return false;
    }
    if (!gw_cfg_json_add_items_filter(p_json_root, p_cfg))
    {
        return false;
    }
    if (!gw_cfg_json_add_string(p_json_root, "coordinates", p_cfg->coordinates))
    {
        return false;
    }
    if (!gw_cfg_json_add_items_scan(p_json_root, p_cfg))
    {
        return false;
    }
    return true;
}

bool
gw_cfg_generate_json_str(cjson_wrap_str_t *p_json_str)
{
    const ruuvi_gateway_config_t *p_cfg     = &g_gateway_config;
    const mac_address_str_t *     p_mac_sta = &g_gw_mac_sta_str;

    p_json_str->p_str = NULL;

    cJSON *p_json_root = cJSON_CreateObject();
    if (NULL == p_json_root)
    {
        LOG_ERR("Can't create json object");
        return false;
    }
    if (!gw_cfg_json_add_items(p_json_root, p_cfg, p_mac_sta))
    {
        cjson_wrap_delete(&p_json_root);
        return false;
    }

    *p_json_str = cjson_wrap_print_and_delete(&p_json_root);
    if (NULL == p_json_str->p_str)
    {
        LOG_ERR("Can't create json string");
        return false;
    }
    return true;
}
