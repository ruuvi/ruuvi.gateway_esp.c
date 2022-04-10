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
gw_cfg_json_add_items_device_info(cJSON *p_json_root, const ruuvi_gw_cfg_device_info_t *const p_dev_info)
{
    if (!gw_cfg_json_add_string(p_json_root, "fw_ver", p_dev_info->esp32_fw_ver.buf))
    {
        return false;
    }
    if (!gw_cfg_json_add_string(p_json_root, "nrf52_fw_ver", p_dev_info->nrf52_fw_ver.buf))
    {
        return false;
    }
    if (!gw_cfg_json_add_string(p_json_root, "gw_mac", p_dev_info->nrf52_mac_addr.str_buf))
    {
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
    if (!gw_cfg_json_add_string(p_json_root, "eth_static_ip", p_cfg->eth.eth_static_ip.buf))
    {
        return false;
    }
    if (!gw_cfg_json_add_string(p_json_root, "eth_netmask", p_cfg->eth.eth_netmask.buf))
    {
        return false;
    }
    if (!gw_cfg_json_add_string(p_json_root, "eth_gw", p_cfg->eth.eth_gw.buf))
    {
        return false;
    }
    if (!gw_cfg_json_add_string(p_json_root, "eth_dns1", p_cfg->eth.eth_dns1.buf))
    {
        return false;
    }
    if (!gw_cfg_json_add_string(p_json_root, "eth_dns2", p_cfg->eth.eth_dns2.buf))
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
    if (!gw_cfg_json_add_string(p_json_root, "mqtt_transport", p_cfg->mqtt.mqtt_transport.buf))
    {
        return false;
    }
    if (!gw_cfg_json_add_string(p_json_root, "mqtt_server", p_cfg->mqtt.mqtt_server.buf))
    {
        return false;
    }
    if (!gw_cfg_json_add_number(p_json_root, "mqtt_port", p_cfg->mqtt.mqtt_port))
    {
        return false;
    }
    if (!gw_cfg_json_add_string(p_json_root, "mqtt_prefix", p_cfg->mqtt.mqtt_prefix.buf))
    {
        return false;
    }
    if (!gw_cfg_json_add_string(p_json_root, "mqtt_client_id", p_cfg->mqtt.mqtt_client_id.buf))
    {
        return false;
    }
    if (!gw_cfg_json_add_string(p_json_root, "mqtt_user", p_cfg->mqtt.mqtt_user.buf))
    {
        return false;
    }
    if (!gw_cfg_json_add_string(p_json_root, "mqtt_pass", p_cfg->mqtt.mqtt_pass.buf))
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
    if (!gw_cfg_json_add_string(p_json_root, "http_url", p_cfg->http.http_url.buf))
    {
        return false;
    }
    if (!gw_cfg_json_add_string(p_json_root, "http_user", p_cfg->http.http_user.buf))
    {
        return false;
    }
    if (!gw_cfg_json_add_string(p_json_root, "http_pass", p_cfg->http.http_pass.buf))
    {
        return false;
    }
    return true;
}

static bool
gw_cfg_json_add_items_http_stat(cJSON *p_json_root, const ruuvi_gateway_config_t *p_cfg)
{
    if (!gw_cfg_json_add_bool(p_json_root, "use_http_stat", p_cfg->http_stat.use_http_stat))
    {
        return false;
    }
    if (!gw_cfg_json_add_string(p_json_root, "http_stat_url", p_cfg->http_stat.http_stat_url.buf))
    {
        return false;
    }
    if (!gw_cfg_json_add_string(p_json_root, "http_stat_user", p_cfg->http_stat.http_stat_user.buf))
    {
        return false;
    }
    if (!gw_cfg_json_add_string(p_json_root, "http_stat_pass", p_cfg->http_stat.http_stat_pass.buf))
    {
        return false;
    }
    return true;
}

static bool
gw_cfg_json_add_items_lan_auth(cJSON *p_json_root, const ruuvi_gateway_config_t *p_cfg)
{
    const char *const p_lan_auth_type_str = gw_cfg_auth_type_to_str(&p_cfg->lan_auth);
    if (!gw_cfg_json_add_string(p_json_root, "lan_auth_type", p_lan_auth_type_str))
    {
        return false;
    }
    if (!gw_cfg_json_add_string(p_json_root, "lan_auth_user", p_cfg->lan_auth.lan_auth_user.buf))
    {
        return false;
    }
    switch (p_cfg->lan_auth.lan_auth_type)
    {
        case HTTP_SERVER_AUTH_TYPE_BASIC:
        case HTTP_SERVER_AUTH_TYPE_DIGEST:
        case HTTP_SERVER_AUTH_TYPE_RUUVI:
            if (!gw_cfg_json_add_string(p_json_root, "lan_auth_pass", p_cfg->lan_auth.lan_auth_pass.buf))
            {
                return false;
            }
            break;
        default:
            break;
    }
    if (!gw_cfg_json_add_string(p_json_root, "lan_auth_api_key", p_cfg->lan_auth.lan_auth_api_key.buf))
    {
        return false;
    }
    return true;
}

static bool
gw_cfg_json_add_items_auto_update(cJSON *p_json_root, const ruuvi_gateway_config_t *p_cfg)
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
    if (!gw_cfg_json_add_items_device_info(p_json_root, &p_cfg->device_info))
    {
        return false;
    }
    if (!gw_cfg_json_add_items_eth(p_json_root, p_cfg))
    {
        return false;
    }
    if (!gw_cfg_json_add_items_http(p_json_root, p_cfg))
    {
        return false;
    }
    if (!gw_cfg_json_add_items_http_stat(p_json_root, p_cfg))
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
gw_cfg_json_parse_device_info(const cJSON *const p_json_root, ruuvi_gw_cfg_device_info_t *const p_gw_cfg_dev_info)
{
    memset(p_gw_cfg_dev_info, 0, sizeof(*p_gw_cfg_dev_info));

    json_wrap_copy_string_val(
        p_json_root,
        "fw_ver",
        &p_gw_cfg_dev_info->esp32_fw_ver.buf[0],
        sizeof(p_gw_cfg_dev_info->esp32_fw_ver.buf));
    json_wrap_copy_string_val(
        p_json_root,
        "nrf52_fw_ver",
        &p_gw_cfg_dev_info->nrf52_fw_ver.buf[0],
        sizeof(p_gw_cfg_dev_info->nrf52_fw_ver.buf));
    json_wrap_copy_string_val(
        p_json_root,
        "gw_mac",
        &p_gw_cfg_dev_info->nrf52_mac_addr.str_buf[0],
        sizeof(p_gw_cfg_dev_info->nrf52_mac_addr.str_buf));
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
            &p_gw_cfg_eth->eth_static_ip.buf[0],
            sizeof(p_gw_cfg_eth->eth_static_ip.buf)))
    {
        LOG_WARN("Can't find key '%s' in config-json", "eth_static_ip");
    }
    if (!json_wrap_copy_string_val(
            p_json_root,
            "eth_netmask",
            &p_gw_cfg_eth->eth_netmask.buf[0],
            sizeof(p_gw_cfg_eth->eth_netmask.buf)))
    {
        LOG_WARN("Can't find key '%s' in config-json", "eth_netmask");
    }
    if (!json_wrap_copy_string_val(
            p_json_root,
            "eth_gw",
            &p_gw_cfg_eth->eth_gw.buf[0],
            sizeof(p_gw_cfg_eth->eth_gw.buf)))
    {
        LOG_WARN("Can't find key '%s' in config-json", "eth_gw");
    }
    if (!json_wrap_copy_string_val(
            p_json_root,
            "eth_dns1",
            &p_gw_cfg_eth->eth_dns1.buf[0],
            sizeof(p_gw_cfg_eth->eth_dns1.buf)))
    {
        LOG_WARN("Can't find key '%s' in config-json", "eth_dns1");
    }
    if (!json_wrap_copy_string_val(
            p_json_root,
            "eth_dns2",
            &p_gw_cfg_eth->eth_dns2.buf[0],
            sizeof(p_gw_cfg_eth->eth_dns2.buf)))
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
    if (!json_wrap_copy_string_val(
            p_json_root,
            "mqtt_transport",
            &p_gw_cfg_mqtt->mqtt_transport.buf[0],
            sizeof(p_gw_cfg_mqtt->mqtt_transport.buf)))
    {
        LOG_WARN("Can't find key '%s' in config-json", "mqtt_transport");
    }
    if (!json_wrap_copy_string_val(
            p_json_root,
            "mqtt_server",
            &p_gw_cfg_mqtt->mqtt_server.buf[0],
            sizeof(p_gw_cfg_mqtt->mqtt_server.buf)))
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
            &p_gw_cfg_mqtt->mqtt_prefix.buf[0],
            sizeof(p_gw_cfg_mqtt->mqtt_prefix.buf)))
    {
        LOG_WARN("Can't find key '%s' in config-json", "mqtt_prefix");
    }
    if (!json_wrap_copy_string_val(
            p_json_root,
            "mqtt_client_id",
            &p_gw_cfg_mqtt->mqtt_client_id.buf[0],
            sizeof(p_gw_cfg_mqtt->mqtt_client_id.buf)))
    {
        LOG_WARN("Can't find key '%s' in config-json", "mqtt_client_id");
    }
    if (!json_wrap_copy_string_val(
            p_json_root,
            "mqtt_user",
            &p_gw_cfg_mqtt->mqtt_user.buf[0],
            sizeof(p_gw_cfg_mqtt->mqtt_user.buf)))
    {
        LOG_WARN("Can't find key '%s' in config-json", "mqtt_user");
    }
    if (!json_wrap_copy_string_val(
            p_json_root,
            "mqtt_pass",
            &p_gw_cfg_mqtt->mqtt_pass.buf[0],
            sizeof(p_gw_cfg_mqtt->mqtt_pass.buf)))
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
            &p_gw_cfg_http->http_url.buf[0],
            sizeof(p_gw_cfg_http->http_url.buf)))
    {
        LOG_WARN("Can't find key '%s' in config-json", "http_url");
    }
    if (!json_wrap_copy_string_val(
            p_json_root,
            "http_user",
            &p_gw_cfg_http->http_user.buf[0],
            sizeof(p_gw_cfg_http->http_user.buf)))
    {
        LOG_WARN("Can't find key '%s' in config-json", "http_user");
    }
    if (!json_wrap_copy_string_val(
            p_json_root,
            "http_pass",
            &p_gw_cfg_http->http_pass.buf[0],
            sizeof(p_gw_cfg_http->http_pass.buf)))
    {
        LOG_WARN("Can't find key '%s' in config-json", "http_pass");
    }
}

static void
gw_cfg_json_parse_http_stat(const cJSON *const p_json_root, ruuvi_gw_cfg_http_stat_t *const p_gw_cfg_http_stat)
{
    if (!json_wrap_get_bool_val(p_json_root, "use_http_stat", &p_gw_cfg_http_stat->use_http_stat))
    {
        LOG_WARN("Can't find key '%s' in config-json", "use_http_stat");
    }
    if (!json_wrap_copy_string_val(
            p_json_root,
            "http_stat_url",
            &p_gw_cfg_http_stat->http_stat_url.buf[0],
            sizeof(p_gw_cfg_http_stat->http_stat_url.buf)))
    {
        LOG_WARN("Can't find key '%s' in config-json", "http_stat_url");
    }
    if (!json_wrap_copy_string_val(
            p_json_root,
            "http_stat_user",
            &p_gw_cfg_http_stat->http_stat_user.buf[0],
            sizeof(p_gw_cfg_http_stat->http_stat_user.buf)))
    {
        LOG_WARN("Can't find key '%s' in config-json", "http_stat_user");
    }
    if (!json_wrap_copy_string_val(
            p_json_root,
            "http_stat_pass",
            &p_gw_cfg_http_stat->http_stat_pass.buf[0],
            sizeof(p_gw_cfg_http_stat->http_stat_pass.buf)))
    {
        LOG_WARN("Can't find key '%s' in config-json", "http_stat_pass");
    }
}

static void
gw_cfg_json_parse_lan_auth(const cJSON *const p_json_root, ruuvi_gw_cfg_lan_auth_t *const p_gw_cfg_lan_auth)
{
    http_server_auth_type_str_t lan_auth_type_str     = { 0 };
    bool                        flag_use_default_auth = false;
    if (!json_wrap_copy_string_val(p_json_root, "lan_auth_type", lan_auth_type_str.buf, sizeof(lan_auth_type_str.buf)))
    {
        LOG_WARN("Can't find key '%s' in config-json", "lan_auth_type");
    }
    else
    {
        p_gw_cfg_lan_auth->lan_auth_type = http_server_auth_type_from_str(
            lan_auth_type_str.buf,
            &flag_use_default_auth);
    }

    if (flag_use_default_auth)
    {
        p_gw_cfg_lan_auth->lan_auth_user = gw_cfg_default_get_lan_auth()->lan_auth_user;
        p_gw_cfg_lan_auth->lan_auth_pass = gw_cfg_default_get_lan_auth()->lan_auth_pass;
    }
    else
    {
        switch (p_gw_cfg_lan_auth->lan_auth_type)
        {
            case HTTP_SERVER_AUTH_TYPE_BASIC:
            case HTTP_SERVER_AUTH_TYPE_DIGEST:
            case HTTP_SERVER_AUTH_TYPE_RUUVI:
                if (!json_wrap_copy_string_val(
                        p_json_root,
                        "lan_auth_user",
                        &p_gw_cfg_lan_auth->lan_auth_user.buf[0],
                        sizeof(p_gw_cfg_lan_auth->lan_auth_user.buf)))
                {
                    LOG_WARN("Can't find key '%s' in config-json", "lan_auth_user");
                }
                if (!json_wrap_copy_string_val(
                        p_json_root,
                        "lan_auth_pass",
                        &p_gw_cfg_lan_auth->lan_auth_pass.buf[0],
                        sizeof(p_gw_cfg_lan_auth->lan_auth_pass.buf)))
                {
                    LOG_WARN("Can't find key '%s' in config-json", "lan_auth_pass");
                }
                break;

            case HTTP_SERVER_AUTH_TYPE_ALLOW:
            case HTTP_SERVER_AUTH_TYPE_DENY:
                p_gw_cfg_lan_auth->lan_auth_user.buf[0] = '\0';
                p_gw_cfg_lan_auth->lan_auth_pass.buf[0] = '\0';
                break;
        }
    }
    if (!json_wrap_copy_string_val(
            p_json_root,
            "lan_auth_api_key",
            &p_gw_cfg_lan_auth->lan_auth_api_key.buf[0],
            sizeof(p_gw_cfg_lan_auth->lan_auth_api_key)))
    {
        LOG_WARN("Can't find key '%s' in config-json", "lan_auth_api_key");
    }
}

static void
gw_cfg_json_parse_auto_update(const cJSON *const p_json_root, ruuvi_gw_cfg_auto_update_t *const p_gw_cfg_auto_update)
{
    p_gw_cfg_auto_update->auto_update_cycle = AUTO_UPDATE_CYCLE_TYPE_REGULAR;
    char auto_update_cycle_str[AUTO_UPDATE_CYCLE_TYPE_STR_MAX_LEN];
    if (!json_wrap_copy_string_val(
            p_json_root,
            "auto_update_cycle",
            &auto_update_cycle_str[0],
            sizeof(auto_update_cycle_str)))
    {
        LOG_WARN("Can't find key '%s' in config-json", "auto_update_cycle");
    }
    else
    {
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
        else
        {
            LOG_WARN("Unknown auto_update_cycle='%s', use REGULAR", auto_update_cycle_str);
        }
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
        LOG_WARN(
            "Can't find key '%s' in config-json, use default value: 0x%04x",
            "company_id",
            p_gw_cfg_filter->company_id);
    }
    if (!json_wrap_get_bool_val(p_json_root, "company_use_filtering", &p_gw_cfg_filter->company_use_filtering))
    {
        LOG_WARN(
            "Can't find key '%s' in config-json, use default value: '%s'",
            "company_use_filtering",
            p_gw_cfg_filter->company_use_filtering ? "true" : "false");
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

void
gw_cfg_json_parse_cjson(
    const cJSON *const                p_json_root,
    ruuvi_gateway_config_t *const     p_gw_cfg,
    ruuvi_gw_cfg_device_info_t *const p_dev_info)
{
    if (NULL != p_dev_info)
    {
        gw_cfg_json_parse_device_info(p_json_root, p_dev_info);
    }

    gw_cfg_json_parse_eth(p_json_root, &p_gw_cfg->eth);
    gw_cfg_json_parse_mqtt(p_json_root, &p_gw_cfg->mqtt);
    gw_cfg_json_parse_http(p_json_root, &p_gw_cfg->http);
    gw_cfg_json_parse_http_stat(p_json_root, &p_gw_cfg->http_stat);
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

static bool
gw_cfg_json_compare_device_info(
    const ruuvi_gw_cfg_device_info_t *const p_val1,
    const ruuvi_gw_cfg_device_info_t *const p_val2)
{
    if (0 != strcmp(p_val1->esp32_fw_ver.buf, p_val2->esp32_fw_ver.buf))
    {
        return false;
    }
    if (0 != strcmp(p_val1->nrf52_fw_ver.buf, p_val2->nrf52_fw_ver.buf))
    {
        return false;
    }
    if (0 != strcmp(p_val1->nrf52_mac_addr.str_buf, p_val2->nrf52_mac_addr.str_buf))
    {
        return false;
    }
    return true;
}

bool
gw_cfg_json_parse(const char *const p_json_str, ruuvi_gateway_config_t *const p_gw_cfg, bool *const p_flag_modified)
{
    *p_flag_modified = false;

    gw_cfg_default_get(p_gw_cfg);

    if ('\0' == p_json_str[0])
    {
        return true;
    }

    cJSON *p_json_root = cJSON_Parse(p_json_str);
    if (NULL == p_json_root)
    {
        return false;
    }

    ruuvi_gw_cfg_device_info_t dev_info = { 0 };
    gw_cfg_json_parse_cjson(p_json_root, p_gw_cfg, &dev_info);

    if (!gw_cfg_json_compare_device_info(&dev_info, &p_gw_cfg->device_info))
    {
        LOG_INFO("gw_cfg: device_info differs:");
        LOG_INFO(
            "gw_cfg: esp32_fw_ver: cur=%s, prev=%s",
            p_gw_cfg->device_info.esp32_fw_ver.buf,
            dev_info.esp32_fw_ver.buf);
        LOG_INFO(
            "gw_cfg: nrf52_fw_ver: cur=%s, prev=%s",
            p_gw_cfg->device_info.nrf52_fw_ver.buf,
            dev_info.nrf52_fw_ver.buf);
        LOG_INFO(
            "gw_cfg: nrf52_mac_addr: cur=%s, prev=%s",
            p_gw_cfg->device_info.nrf52_mac_addr.str_buf,
            dev_info.nrf52_mac_addr.str_buf);
        *p_flag_modified = true;
    }

    cJSON_Delete(p_json_root);
    return true;
}
