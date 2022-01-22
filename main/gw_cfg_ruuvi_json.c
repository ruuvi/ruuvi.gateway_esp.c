/**
 * @file gw_cfg_ruuvi_json.c
 * @author TheSomeMan
 * @date 2021-09-29
 * @copyright Ruuvi Innovations Ltd, license BSD-3-Clause.
 */

#include "gw_cfg_ruuvi_json.h"
#include <stdio.h>
#include <string.h>
#include "gw_cfg.h"
#include "gw_cfg_default.h"
#include "json_ruuvi.h"
#include "gw_mac.h"

#define LOG_LOCAL_LEVEL LOG_LEVEL_INFO
#include "log.h"

static const char TAG[] = "gw_cfg";

static bool
gw_cfg_ruuvi_json_add_bool(cJSON *p_json_root, const char *p_item_name, const bool val)
{
    if (NULL == cJSON_AddBoolToObject(p_json_root, p_item_name, val))
    {
        LOG_ERR("Can't add json item: %s", p_item_name);
        return false;
    }
    return true;
}

static bool
gw_cfg_ruuvi_json_add_string(cJSON *p_json_root, const char *p_item_name, const char *p_val)
{
    if (NULL == cJSON_AddStringToObject(p_json_root, p_item_name, p_val))
    {
        LOG_ERR("Can't add json item: %s", p_item_name);
        return false;
    }
    return true;
}

static bool
gw_cfg_ruuvi_json_add_number(cJSON *p_json_root, const char *p_item_name, const cjson_number_t val)
{
    if (NULL == cJSON_AddNumberToObject(p_json_root, p_item_name, val))
    {
        LOG_ERR("Can't add json item: %s", p_item_name);
        return false;
    }
    return true;
}

static bool
gw_cfg_ruuvi_json_add_items_fw_version(cJSON *p_json_root, const char *const p_fw_ver, const char *const p_nrf52_fw_ver)
{
    if (!gw_cfg_ruuvi_json_add_string(p_json_root, "fw_ver", p_fw_ver))
    {
        return false;
    }
    if (!gw_cfg_ruuvi_json_add_string(p_json_root, "nrf52_fw_ver", p_nrf52_fw_ver))
    {
        return false;
    }
    return true;
}

static bool
gw_cfg_ruuvi_json_add_items_eth(cJSON *p_json_root, const ruuvi_gateway_config_t *p_cfg)
{
    if (!gw_cfg_ruuvi_json_add_bool(p_json_root, "use_eth", p_cfg->eth.use_eth))
    {
        return false;
    }
    if (!gw_cfg_ruuvi_json_add_bool(p_json_root, "eth_dhcp", p_cfg->eth.eth_dhcp))
    {
        return false;
    }
    if (!gw_cfg_ruuvi_json_add_string(p_json_root, "eth_static_ip", p_cfg->eth.eth_static_ip.buf))
    {
        return false;
    }
    if (!gw_cfg_ruuvi_json_add_string(p_json_root, "eth_netmask", p_cfg->eth.eth_netmask.buf))
    {
        return false;
    }
    if (!gw_cfg_ruuvi_json_add_string(p_json_root, "eth_gw", p_cfg->eth.eth_gw.buf))
    {
        return false;
    }
    if (!gw_cfg_ruuvi_json_add_string(p_json_root, "eth_dns1", p_cfg->eth.eth_dns1.buf))
    {
        return false;
    }
    if (!gw_cfg_ruuvi_json_add_string(p_json_root, "eth_dns2", p_cfg->eth.eth_dns2.buf))
    {
        return false;
    }
    return true;
}

static bool
gw_cfg_ruuvi_json_add_items_http(cJSON *p_json_root, const ruuvi_gateway_config_t *p_cfg)
{
    if (!gw_cfg_ruuvi_json_add_bool(p_json_root, "use_http", p_cfg->http.use_http))
    {
        return false;
    }
    if (!gw_cfg_ruuvi_json_add_string(p_json_root, "http_url", p_cfg->http.http_url.buf))
    {
        return false;
    }
    if (!gw_cfg_ruuvi_json_add_string(p_json_root, "http_user", p_cfg->http.http_user.buf))
    {
        return false;
    }
    return true;
}

static bool
gw_cfg_ruuvi_json_add_items_http_stat(cJSON *p_json_root, const ruuvi_gateway_config_t *p_cfg)
{
    if (!gw_cfg_ruuvi_json_add_bool(p_json_root, "use_http_stat", p_cfg->http_stat.use_http_stat))
    {
        return false;
    }
    if (!gw_cfg_ruuvi_json_add_string(p_json_root, "http_stat_url", p_cfg->http_stat.http_stat_url.buf))
    {
        return false;
    }
    if (!gw_cfg_ruuvi_json_add_string(p_json_root, "http_stat_user", p_cfg->http_stat.http_stat_user.buf))
    {
        return false;
    }
    return true;
}

static bool
gw_cfg_ruuvi_json_add_items_lan_auth(cJSON *p_json_root, const ruuvi_gateway_config_t *p_cfg)
{
    const char *const p_lan_auth_type
        = ((0 == strcmp(p_cfg->lan_auth.lan_auth_type, HTTP_SERVER_AUTH_TYPE_STR_RUUVI))
           && (0 == strcmp(p_cfg->lan_auth.lan_auth_user, RUUVI_GATEWAY_AUTH_DEFAULT_USER))
           && (0 == strcmp(p_cfg->lan_auth.lan_auth_pass, gw_cfg_default_get_lan_auth_password())))
              ? "lan_auth_default"
              : &p_cfg->lan_auth.lan_auth_type[0];
    if (!gw_cfg_ruuvi_json_add_string(p_json_root, "lan_auth_type", p_lan_auth_type))
    {
        return false;
    }
    if (!gw_cfg_ruuvi_json_add_string(p_json_root, "lan_auth_user", p_cfg->lan_auth.lan_auth_user))
    {
        return false;
    }
    if (!gw_cfg_ruuvi_json_add_string(p_json_root, "lan_auth_api_key", p_cfg->lan_auth.lan_auth_api_key))
    {
        return false;
    }
    return true;
}

static bool
gw_cfg_ruuvi_json_add_items_auto_update(cJSON *p_json_root, const ruuvi_gateway_config_t *p_cfg)
{
    const char *p_auto_update_cycle_str = AUTO_UPDATE_CYCLE_TYPE_STR_REGULAR;
    switch (p_cfg->auto_update.auto_update_cycle)
    {
        case AUTO_UPDATE_CYCLE_TYPE_REGULAR:
            p_auto_update_cycle_str = AUTO_UPDATE_CYCLE_TYPE_STR_REGULAR;
            break;
        case AUTO_UPDATE_CYCLE_TYPE_BETA_TESTER:
            p_auto_update_cycle_str = AUTO_UPDATE_CYCLE_TYPE_STR_BETA_TESTER;
            break;
        case AUTO_UPDATE_CYCLE_TYPE_MANUAL:
            p_auto_update_cycle_str = AUTO_UPDATE_CYCLE_TYPE_STR_MANUAL;
            break;
    }
    if (!gw_cfg_ruuvi_json_add_string(p_json_root, "auto_update_cycle", p_auto_update_cycle_str))
    {
        return false;
    }
    if (!gw_cfg_ruuvi_json_add_number(
            p_json_root,
            "auto_update_weekdays_bitmask",
            p_cfg->auto_update.auto_update_weekdays_bitmask))
    {
        return false;
    }
    if (!gw_cfg_ruuvi_json_add_number(
            p_json_root,
            "auto_update_interval_from",
            p_cfg->auto_update.auto_update_interval_from))
    {
        return false;
    }
    if (!gw_cfg_ruuvi_json_add_number(
            p_json_root,
            "auto_update_interval_to",
            p_cfg->auto_update.auto_update_interval_to))
    {
        return false;
    }
    if (!gw_cfg_ruuvi_json_add_number(
            p_json_root,
            "auto_update_tz_offset_hours",
            p_cfg->auto_update.auto_update_tz_offset_hours))
    {
        return false;
    }
    return true;
}

static bool
gw_cfg_ruuvi_json_add_items_mqtt(cJSON *const p_json_root, const ruuvi_gateway_config_t *const p_cfg)
{
    if (!gw_cfg_ruuvi_json_add_bool(p_json_root, "use_mqtt", p_cfg->mqtt.use_mqtt))
    {
        return false;
    }
    if (!gw_cfg_ruuvi_json_add_string(p_json_root, "mqtt_transport", p_cfg->mqtt.mqtt_transport.buf))
    {
        return false;
    }
    if (!gw_cfg_ruuvi_json_add_string(p_json_root, "mqtt_server", p_cfg->mqtt.mqtt_server.buf))
    {
        return false;
    }
    if (!gw_cfg_ruuvi_json_add_number(p_json_root, "mqtt_port", p_cfg->mqtt.mqtt_port))
    {
        return false;
    }
    ruuvi_gw_cfg_mqtt_prefix_t mqtt_prefix = { '\0' };
    if (p_cfg->mqtt.mqtt_use_default_prefix)
    {
        snprintf(mqtt_prefix.buf, sizeof(mqtt_prefix.buf), "ruuvi/%s/", g_gw_mac_sta_str.str_buf);
    }
    else
    {
        snprintf(mqtt_prefix.buf, sizeof(mqtt_prefix.buf), "%s", p_cfg->mqtt.mqtt_prefix.buf);
    }
    if (!gw_cfg_ruuvi_json_add_string(p_json_root, "mqtt_prefix", mqtt_prefix.buf))
    {
        return false;
    }
    if (!gw_cfg_ruuvi_json_add_string(p_json_root, "mqtt_client_id", p_cfg->mqtt.mqtt_client_id.buf))
    {
        return false;
    }
    if (!gw_cfg_ruuvi_json_add_string(p_json_root, "mqtt_user", p_cfg->mqtt.mqtt_user.buf))
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
gw_cfg_ruuvi_json_add_items_filter(cJSON *p_json_root, const ruuvi_gateway_config_t *p_cfg)
{
    if (!gw_cfg_ruuvi_json_add_bool(p_json_root, "use_filtering", p_cfg->filter.company_use_filtering))
    {
        return false;
    }
    char company_id[10];
    snprintf(company_id, sizeof(company_id), "0x%04x", p_cfg->filter.company_id);
    if (!gw_cfg_ruuvi_json_add_string(p_json_root, "company_id", company_id))
    {
        return false;
    }
    return true;
}

static bool
gw_cfg_ruuvi_json_add_items_scan(cJSON *p_json_root, const ruuvi_gateway_config_t *p_cfg)
{
    if (!gw_cfg_ruuvi_json_add_bool(p_json_root, "use_coded_phy", p_cfg->scan.scan_coded_phy))
    {
        return false;
    }
    if (!gw_cfg_ruuvi_json_add_bool(p_json_root, "use_1mbit_phy", p_cfg->scan.scan_1mbit_phy))
    {
        return false;
    }
    if (!gw_cfg_ruuvi_json_add_bool(p_json_root, "use_extended_payload", p_cfg->scan.scan_extended_payload))
    {
        return false;
    }
    if (!gw_cfg_ruuvi_json_add_bool(p_json_root, "use_channel_37", p_cfg->scan.scan_channel_37))
    {
        return false;
    }
    if (!gw_cfg_ruuvi_json_add_bool(p_json_root, "use_channel_38", p_cfg->scan.scan_channel_38))
    {
        return false;
    }
    if (!gw_cfg_ruuvi_json_add_bool(p_json_root, "use_channel_39", p_cfg->scan.scan_channel_39))
    {
        return false;
    }
    return true;
}

static bool
gw_cfg_ruuvi_json_add_items(
    cJSON *                       p_json_root,
    const ruuvi_gateway_config_t *p_cfg,
    const mac_address_str_t *     p_mac_sta,
    const char *const             p_fw_ver,
    const char *const             p_nrf52_fw_ver)
{
    if (!gw_cfg_ruuvi_json_add_items_fw_version(p_json_root, p_fw_ver, p_nrf52_fw_ver))
    {
        return false;
    }
    if (!gw_cfg_ruuvi_json_add_items_eth(p_json_root, p_cfg))
    {
        return false;
    }
    if (!gw_cfg_ruuvi_json_add_items_http(p_json_root, p_cfg))
    {
        return false;
    }
    if (!gw_cfg_ruuvi_json_add_items_http_stat(p_json_root, p_cfg))
    {
        return false;
    }
    if (!gw_cfg_ruuvi_json_add_items_mqtt(p_json_root, p_cfg))
    {
        return false;
    }
    if (!gw_cfg_ruuvi_json_add_items_lan_auth(p_json_root, p_cfg))
    {
        return false;
    }
    if (!gw_cfg_ruuvi_json_add_items_auto_update(p_json_root, p_cfg))
    {
        return false;
    }
    if (!gw_cfg_ruuvi_json_add_string(p_json_root, "gw_mac", p_mac_sta->str_buf))
    {
        return false;
    }
    if (!gw_cfg_ruuvi_json_add_items_filter(p_json_root, p_cfg))
    {
        return false;
    }
    if (!gw_cfg_ruuvi_json_add_string(p_json_root, "coordinates", p_cfg->coordinates.buf))
    {
        return false;
    }
    if (!gw_cfg_ruuvi_json_add_items_scan(p_json_root, p_cfg))
    {
        return false;
    }
    return true;
}

bool
gw_cfg_ruuvi_json_generate(
    const ruuvi_gateway_config_t *const p_cfg,
    const mac_address_str_t *const      p_mac_sta,
    const char *const                   p_fw_ver,
    const char *const                   p_nrf52_fw_ver,
    cjson_wrap_str_t *const             p_json_str)
{
    p_json_str->p_str = NULL;

    cJSON *p_json_root = cJSON_CreateObject();
    if (NULL == p_json_root)
    {
        LOG_ERR("Can't create json object");
        return false;
    }
    if (!gw_cfg_ruuvi_json_add_items(p_json_root, p_cfg, p_mac_sta, p_fw_ver, p_nrf52_fw_ver))
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
