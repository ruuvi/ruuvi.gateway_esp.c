/**
 * @file gw_cfg_json.c
 * @author TheSomeMan
 * @date 2021-09-29
 * @copyright Ruuvi Innovations Ltd, license BSD-3-Clause.
 */

#include "gw_cfg_json.h"
#include <string.h>
#include "gw_cfg_default.h"

#define LOG_LOCAL_LEVEL LOG_LEVEL_INFO
#include "log.h"

static const char TAG[] = "gw_cfg";

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
    if (!gw_cfg_json_add_bool(p_json_root, "use_eth", p_cfg->eth.use_eth))
    {
        return false;
    }
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
gw_cfg_json_add_items_mqtt(cJSON *const p_json_root, const ruuvi_gateway_config_t *const p_cfg)
{
    if (!gw_cfg_json_add_bool(p_json_root, "use_mqtt", p_cfg->mqtt.use_mqtt))
    {
        return false;
    }
    if (!gw_cfg_json_add_bool(p_json_root, "mqtt_use_default_prefix", p_cfg->mqtt.mqtt_use_default_prefix))
    {
        return false;
    }
    if (!gw_cfg_json_add_string(p_json_root, "mqtt_transport", p_cfg->mqtt.mqtt_transport))
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
    if (!gw_cfg_json_add_string(p_json_root, "mqtt_client_id", p_cfg->mqtt.mqtt_client_id))
    {
        return false;
    }
    if (!gw_cfg_json_add_string(p_json_root, "mqtt_user", p_cfg->mqtt.mqtt_user))
    {
        return false;
    }
    if (!gw_cfg_json_add_string(p_json_root, "mqtt_pass", p_cfg->mqtt.mqtt_pass))
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
    if (!gw_cfg_json_add_string(p_json_root, "http_pass", p_cfg->http.http_pass))
    {
        return false;
    }
    return true;
}

static bool
gw_cfg_json_add_items_lan_auth(cJSON *p_json_root, const ruuvi_gateway_config_t *p_cfg)
{
    if (!gw_cfg_json_add_string(p_json_root, "lan_auth_type", p_cfg->lan_auth.lan_auth_type))
    {
        return false;
    }
    if (!gw_cfg_json_add_string(p_json_root, "lan_auth_user", p_cfg->lan_auth.lan_auth_user))
    {
        return false;
    }
    if (!gw_cfg_json_add_string(p_json_root, "lan_auth_pass", p_cfg->lan_auth.lan_auth_pass))
    {
        return false;
    }
    return true;
}

static bool
gw_cfg_json_add_items_auto_update(cJSON *p_json_root, const ruuvi_gateway_config_t *p_cfg)
{
    const char *p_auto_update_cycle_str = AUTO_UPDATE_CYCLE_TYPE_STR_MANUAL;
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
    if (!gw_cfg_json_add_string(p_json_root, "auto_update_cycle", p_auto_update_cycle_str))
    {
        return false;
    }
    if (!gw_cfg_json_add_number(
            p_json_root,
            "auto_update_weekdays_bitmask",
            p_cfg->auto_update.auto_update_weekdays_bitmask))
    {
        return false;
    }
    if (!gw_cfg_json_add_number(p_json_root, "auto_update_interval_from", p_cfg->auto_update.auto_update_interval_from))
    {
        return false;
    }
    if (!gw_cfg_json_add_number(p_json_root, "auto_update_interval_to", p_cfg->auto_update.auto_update_interval_to))
    {
        return false;
    }
    if (!gw_cfg_json_add_number(
            p_json_root,
            "auto_update_tz_offset_hours",
            p_cfg->auto_update.auto_update_tz_offset_hours))
    {
        return false;
    }
    return true;
}

static bool
gw_cfg_json_add_items_filter(cJSON *p_json_root, const ruuvi_gateway_config_t *p_cfg)
{
    if (!gw_cfg_json_add_number(p_json_root, "company_id", p_cfg->filter.company_id))
    {
        return false;
    }
    if (!gw_cfg_json_add_bool(p_json_root, "company_use_filtering", p_cfg->filter.company_use_filtering))
    {
        return false;
    }
    return true;
}

static bool
gw_cfg_json_add_items_scan(cJSON *p_json_root, const ruuvi_gateway_config_t *p_cfg)
{
    if (!gw_cfg_json_add_bool(p_json_root, "scan_coded_phy", p_cfg->scan.scan_coded_phy))
    {
        return false;
    }
    if (!gw_cfg_json_add_bool(p_json_root, "scan_1mbit_phy", p_cfg->scan.scan_1mbit_phy))
    {
        return false;
    }
    if (!gw_cfg_json_add_bool(p_json_root, "scan_extended_payload", p_cfg->scan.scan_extended_payload))
    {
        return false;
    }
    if (!gw_cfg_json_add_bool(p_json_root, "scan_channel_37", p_cfg->scan.scan_channel_37))
    {
        return false;
    }
    if (!gw_cfg_json_add_bool(p_json_root, "scan_channel_38", p_cfg->scan.scan_channel_38))
    {
        return false;
    }
    if (!gw_cfg_json_add_bool(p_json_root, "scan_channel_39", p_cfg->scan.scan_channel_39))
    {
        return false;
    }
    return true;
}

static bool
gw_cfg_json_add_items(cJSON *p_json_root, const ruuvi_gateway_config_t *p_cfg)
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
    if (!gw_cfg_json_add_items_lan_auth(p_json_root, p_cfg))
    {
        return false;
    }
    if (!gw_cfg_json_add_items_auto_update(p_json_root, p_cfg))
    {
        return false;
    }
    if (!gw_cfg_json_add_items_filter(p_json_root, p_cfg))
    {
        return false;
    }
    if (!gw_cfg_json_add_items_scan(p_json_root, p_cfg))
    {
        return false;
    }
    if (!gw_cfg_json_add_string(p_json_root, "coordinates", p_cfg->coordinates.buf))
    {
        return false;
    }
    return true;
}

bool
gw_cfg_json_generate(const ruuvi_gateway_config_t *const p_gw_cfg, cjson_wrap_str_t *const p_json_str)
{
    p_json_str->p_str = NULL;

    cJSON *p_json_root = cJSON_CreateObject();
    if (NULL == p_json_root)
    {
        LOG_ERR("Can't create json object");
        return false;
    }
    if (!gw_cfg_json_add_items(p_json_root, p_gw_cfg))
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

static void
gw_cfg_json_parse_eth(const cJSON *const p_json_root, ruuvi_gw_cfg_eth_t *const p_gw_cfg_eth)
{
    if (!json_wrap_get_bool_val(p_json_root, "use_eth", &p_gw_cfg_eth->use_eth))
    {
        LOG_WARN("Can't find key '%s' in config-json", "use_eth");
    }
    if (!json_wrap_get_bool_val(p_json_root, "eth_dhcp", &p_gw_cfg_eth->eth_dhcp))
    {
        LOG_WARN("Can't find key '%s' in config-json", "eth_dhcp");
    }
    if (!json_wrap_copy_string_val(
            p_json_root,
            "eth_static_ip",
            &p_gw_cfg_eth->eth_static_ip[0],
            sizeof(p_gw_cfg_eth->eth_static_ip)))
    {
        LOG_WARN("Can't find key '%s' in config-json", "eth_static_ip");
    }
    if (!json_wrap_copy_string_val(
            p_json_root,
            "eth_netmask",
            &p_gw_cfg_eth->eth_netmask[0],
            sizeof(p_gw_cfg_eth->eth_netmask)))
    {
        LOG_WARN("Can't find key '%s' in config-json", "eth_netmask");
    }
    if (!json_wrap_copy_string_val(p_json_root, "eth_gw", &p_gw_cfg_eth->eth_gw[0], sizeof(p_gw_cfg_eth->eth_gw)))
    {
        LOG_WARN("Can't find key '%s' in config-json", "eth_gw");
    }
    if (!json_wrap_copy_string_val(p_json_root, "eth_dns1", &p_gw_cfg_eth->eth_dns1[0], sizeof(p_gw_cfg_eth->eth_dns1)))
    {
        LOG_WARN("Can't find key '%s' in config-json", "eth_dns1");
    }
    if (!json_wrap_copy_string_val(p_json_root, "eth_dns2", &p_gw_cfg_eth->eth_dns2[0], sizeof(p_gw_cfg_eth->eth_dns2)))
    {
        LOG_WARN("Can't find key '%s' in config-json", "eth_dns2");
    }
}

static void
gw_cfg_json_parse_mqtt(const cJSON *const p_json_root, ruuvi_gw_cfg_mqtt_t *const p_gw_cfg_mqtt)
{
    if (!json_wrap_get_bool_val(p_json_root, "use_mqtt", &p_gw_cfg_mqtt->use_mqtt))
    {
        LOG_WARN("Can't find key '%s' in config-json", "use_mqtt");
    }
    if (!json_wrap_get_bool_val(p_json_root, "mqtt_use_default_prefix", &p_gw_cfg_mqtt->mqtt_use_default_prefix))
    {
        LOG_WARN("Can't find key '%s' in config-json", "mqtt_use_default_prefix");
    }
    if (!json_wrap_copy_string_val(
            p_json_root,
            "mqtt_transport",
            &p_gw_cfg_mqtt->mqtt_transport[0],
            sizeof(p_gw_cfg_mqtt->mqtt_transport)))
    {
        LOG_WARN("Can't find key '%s' in config-json", "mqtt_transport");
    }
    if (!json_wrap_copy_string_val(
            p_json_root,
            "mqtt_server",
            &p_gw_cfg_mqtt->mqtt_server[0],
            sizeof(p_gw_cfg_mqtt->mqtt_server)))
    {
        LOG_WARN("Can't find key '%s' in config-json", "mqtt_server");
    }
    if (!json_wrap_get_uint16_val(p_json_root, "mqtt_port", &p_gw_cfg_mqtt->mqtt_port))
    {
        LOG_WARN("Can't find key '%s' in config-json", "mqtt_port");
    }
    if (!json_wrap_copy_string_val(
            p_json_root,
            "mqtt_prefix",
            &p_gw_cfg_mqtt->mqtt_prefix[0],
            sizeof(p_gw_cfg_mqtt->mqtt_prefix)))
    {
        LOG_WARN("Can't find key '%s' in config-json", "mqtt_prefix");
    }
    if (!json_wrap_copy_string_val(
            p_json_root,
            "mqtt_client_id",
            &p_gw_cfg_mqtt->mqtt_client_id[0],
            sizeof(p_gw_cfg_mqtt->mqtt_client_id)))
    {
        LOG_WARN("Can't find key '%s' in config-json", "mqtt_client_id");
    }
    if (!json_wrap_copy_string_val(
            p_json_root,
            "mqtt_user",
            &p_gw_cfg_mqtt->mqtt_user[0],
            sizeof(p_gw_cfg_mqtt->mqtt_user)))
    {
        LOG_WARN("Can't find key '%s' in config-json", "mqtt_user");
    }
    if (!json_wrap_copy_string_val(
            p_json_root,
            "mqtt_pass",
            &p_gw_cfg_mqtt->mqtt_pass[0],
            sizeof(p_gw_cfg_mqtt->mqtt_pass)))
    {
        LOG_WARN("Can't find key '%s' in config-json", "mqtt_pass");
    }
}

static void
gw_cfg_json_parse_http(const cJSON *const p_json_root, ruuvi_gw_cfg_http_t *const p_gw_cfg_http)
{
    if (!json_wrap_get_bool_val(p_json_root, "use_http", &p_gw_cfg_http->use_http))
    {
        LOG_WARN("Can't find key '%s' in config-json", "use_http");
    }
    if (!json_wrap_copy_string_val(
            p_json_root,
            "http_url",
            &p_gw_cfg_http->http_url[0],
            sizeof(p_gw_cfg_http->http_url)))
    {
        LOG_WARN("Can't find key '%s' in config-json", "http_url");
    }
    if (!json_wrap_copy_string_val(
            p_json_root,
            "http_user",
            &p_gw_cfg_http->http_user[0],
            sizeof(p_gw_cfg_http->http_user)))
    {
        LOG_WARN("Can't find key '%s' in config-json", "http_user");
    }
    if (!json_wrap_copy_string_val(
            p_json_root,
            "http_pass",
            &p_gw_cfg_http->http_pass[0],
            sizeof(p_gw_cfg_http->http_pass)))
    {
        LOG_WARN("Can't find key '%s' in config-json", "http_pass");
    }
}

static void
gw_cfg_json_parse_lan_auth(const cJSON *const p_json_root, ruuvi_gw_cfg_lan_auth_t *const p_gw_cfg_lan_auth)
{
    if (!json_wrap_copy_string_val(
            p_json_root,
            "lan_auth_type",
            &p_gw_cfg_lan_auth->lan_auth_type[0],
            sizeof(p_gw_cfg_lan_auth->lan_auth_type)))
    {
        LOG_WARN("Can't find key '%s' in config-json", "lan_auth_type");
    }
    if (!json_wrap_copy_string_val(
            p_json_root,
            "lan_auth_user",
            &p_gw_cfg_lan_auth->lan_auth_user[0],
            sizeof(p_gw_cfg_lan_auth->lan_auth_user)))
    {
        LOG_WARN("Can't find key '%s' in config-json", "lan_auth_user");
    }
    if (!json_wrap_copy_string_val(
            p_json_root,
            "lan_auth_pass",
            &p_gw_cfg_lan_auth->lan_auth_pass[0],
            sizeof(p_gw_cfg_lan_auth->lan_auth_pass)))
    {
        LOG_WARN("Can't find key '%s' in config-json", "lan_auth_pass");
    }
}

static void
gw_cfg_json_parse_auto_update(const cJSON *const p_json_root, ruuvi_gw_cfg_auto_update_t *const p_gw_cfg_auto_update)
{
    char auto_update_cycle_str[AUTO_UPDATE_CYCLE_TYPE_STR_MAX_LEN];
    if (!json_wrap_copy_string_val(
            p_json_root,
            "auto_update_cycle",
            &auto_update_cycle_str[0],
            sizeof(auto_update_cycle_str)))
    {
        LOG_WARN("Can't find key '%s' in config-json", "auto_update_cycle");
    }
    p_gw_cfg_auto_update->auto_update_cycle = AUTO_UPDATE_CYCLE_TYPE_REGULAR;
    if (0 == strcmp(AUTO_UPDATE_CYCLE_TYPE_STR_REGULAR, auto_update_cycle_str))
    {
        p_gw_cfg_auto_update->auto_update_cycle = AUTO_UPDATE_CYCLE_TYPE_REGULAR;
    }
    else if (0 == strcmp(AUTO_UPDATE_CYCLE_TYPE_STR_BETA_TESTER, auto_update_cycle_str))
    {
        p_gw_cfg_auto_update->auto_update_cycle = AUTO_UPDATE_CYCLE_TYPE_BETA_TESTER;
    }
    else if (0 == strcmp(AUTO_UPDATE_CYCLE_TYPE_STR_MANUAL, auto_update_cycle_str))
    {
        p_gw_cfg_auto_update->auto_update_cycle = AUTO_UPDATE_CYCLE_TYPE_MANUAL;
    }

    if (!json_wrap_get_uint8_val(
            p_json_root,
            "auto_update_weekdays_bitmask",
            &p_gw_cfg_auto_update->auto_update_weekdays_bitmask))
    {
        LOG_WARN("Can't find key '%s' in config-json", "auto_update_weekdays_bitmask");
    }
    if (!json_wrap_get_uint8_val(
            p_json_root,
            "auto_update_interval_from",
            &p_gw_cfg_auto_update->auto_update_interval_from))
    {
        LOG_WARN("Can't find key '%s' in config-json", "auto_update_interval_from");
    }
    if (!json_wrap_get_uint8_val(
            p_json_root,
            "auto_update_interval_to",
            &p_gw_cfg_auto_update->auto_update_interval_to))
    {
        LOG_WARN("Can't find key '%s' in config-json", "auto_update_interval_to");
    }
    if (!json_wrap_get_int8_val(
            p_json_root,
            "auto_update_tz_offset_hours",
            &p_gw_cfg_auto_update->auto_update_tz_offset_hours))
    {
        LOG_WARN("Can't find key '%s' in config-json", "auto_update_tz_offset_hours");
    }
}

static void
gw_cfg_json_parse_filter(const cJSON *const p_json_root, ruuvi_gw_cfg_filter_t *const p_gw_cfg_filter)
{
    if (!json_wrap_get_uint16_val(p_json_root, "company_id", &p_gw_cfg_filter->company_id))
    {
        LOG_WARN("Can't find key '%s' in config-json", "company_id");
    }
    if (!json_wrap_get_bool_val(p_json_root, "company_use_filtering", &p_gw_cfg_filter->company_use_filtering))
    {
        LOG_WARN("Can't find key '%s' in config-json", "company_use_filtering");
    }
}

static void
gw_cfg_json_parse_scan(const cJSON *const p_json_root, ruuvi_gw_cfg_scan_t *const p_gw_cfg_scan)
{
    if (!json_wrap_get_bool_val(p_json_root, "scan_coded_phy", &p_gw_cfg_scan->scan_coded_phy))
    {
        LOG_WARN("Can't find key '%s' in config-json", "scan_coded_phy");
    }
    if (!json_wrap_get_bool_val(p_json_root, "scan_1mbit_phy", &p_gw_cfg_scan->scan_1mbit_phy))
    {
        LOG_WARN("Can't find key '%s' in config-json", "scan_1mbit_phy");
    }
    if (!json_wrap_get_bool_val(p_json_root, "scan_extended_payload", &p_gw_cfg_scan->scan_extended_payload))
    {
        LOG_WARN("Can't find key '%s' in config-json", "scan_extended_payload");
    }
    if (!json_wrap_get_bool_val(p_json_root, "scan_channel_37", &p_gw_cfg_scan->scan_channel_37))
    {
        LOG_WARN("Can't find key '%s' in config-json", "scan_channel_37");
    }
    if (!json_wrap_get_bool_val(p_json_root, "scan_channel_38", &p_gw_cfg_scan->scan_channel_38))
    {
        LOG_WARN("Can't find key '%s' in config-json", "scan_channel_38");
    }
    if (!json_wrap_get_bool_val(p_json_root, "scan_channel_39", &p_gw_cfg_scan->scan_channel_39))
    {
        LOG_WARN("Can't find key '%s' in config-json", "scan_channel_39");
    }
}

static void
gw_cfg_json_parse_cjson(const cJSON *const p_json_root, ruuvi_gateway_config_t *const p_gw_cfg)
{
    gw_cfg_json_parse_eth(p_json_root, &p_gw_cfg->eth);
    gw_cfg_json_parse_mqtt(p_json_root, &p_gw_cfg->mqtt);
    gw_cfg_json_parse_http(p_json_root, &p_gw_cfg->http);
    gw_cfg_json_parse_lan_auth(p_json_root, &p_gw_cfg->lan_auth);
    gw_cfg_json_parse_auto_update(p_json_root, &p_gw_cfg->auto_update);
    gw_cfg_json_parse_filter(p_json_root, &p_gw_cfg->filter);
    gw_cfg_json_parse_scan(p_json_root, &p_gw_cfg->scan);
    if (!json_wrap_copy_string_val(
            p_json_root,
            "coordinates",
            &p_gw_cfg->coordinates.buf[0],
            sizeof(p_gw_cfg->coordinates.buf)))
    {
        LOG_WARN("Can't find key '%s' in config-json", "coordinates");
    }
}

bool
gw_cfg_json_parse(const char *const p_json_str, ruuvi_gateway_config_t *const p_gw_cfg)
{
    *p_gw_cfg = g_gateway_config_default;

    if ('\0' == p_json_str[0])
    {
        return true;
    }

    cJSON *p_json_root = cJSON_Parse(p_json_str);
    if (NULL == p_json_root)
    {
        return false;
    }

    gw_cfg_json_parse_cjson(p_json_root, p_gw_cfg);

    cJSON_Delete(p_json_root);
    return true;
}