/**
 * @file json_ruuvi.h
 * @author TheSomeMan
 * @date 2020-10-31
 * @copyright Ruuvi Innovations Ltd, license BSD-3-Clause.
 */

#include "json_ruuvi.h"
#include <string.h>
#include <stdio.h>
#include "cJSON.h"
#include "cjson_wrap.h"
#include "http_server_auth_type.h"
#include "gw_cfg_default.h"

#define LOG_LOCAL_LEVEL LOG_LEVEL_DEBUG
#include "log.h"

static const char TAG[] = "http_server";

JSON_RUUVI_STATIC
bool
json_ruuvi_copy_string_val(
    const cJSON *p_json_root,
    const char * p_attr_name,
    char *       buf,
    const size_t buf_len,
    bool         flag_log_err_if_not_found)
{
    if (!json_wrap_copy_string_val(p_json_root, p_attr_name, buf, buf_len))
    {
        if (flag_log_err_if_not_found)
        {
            LOG_ERR("%s not found", p_attr_name);
        }
        return false;
    }
    LOG_DBG("%s: %s", p_attr_name, buf);
    return true;
}

JSON_RUUVI_STATIC
bool
json_ruuvi_get_bool_val(const cJSON *p_json_root, const char *p_attr_name, bool *p_val, bool flag_log_err_if_not_found)
{
    if (!json_wrap_get_bool_val(p_json_root, p_attr_name, p_val))
    {
        if (flag_log_err_if_not_found)
        {
            LOG_ERR("%s not found", p_attr_name);
        }
        return false;
    }
    LOG_DBG("%s: %d", p_attr_name, *p_val);
    return true;
}

JSON_RUUVI_STATIC
bool
json_ruuvi_get_uint16_val(
    const cJSON *p_json_root,
    const char * p_attr_name,
    uint16_t *   p_val,
    bool         flag_log_err_if_not_found)
{
    if (!json_wrap_get_uint16_val(p_json_root, p_attr_name, p_val))
    {
        if (flag_log_err_if_not_found)
        {
            LOG_ERR("%s not found or invalid", p_attr_name);
        }
        return false;
    }
    LOG_DBG("%s: %u", p_attr_name, *p_val);
    return true;
}

JSON_RUUVI_STATIC
bool
json_ruuvi_get_uint8_val(
    const cJSON *p_json_root,
    const char * p_attr_name,
    uint8_t *    p_val,
    bool         flag_log_err_if_not_found)
{
    if (!json_wrap_get_uint8_val(p_json_root, p_attr_name, p_val))
    {
        if (flag_log_err_if_not_found)
        {
            LOG_ERR("%s not found or invalid", p_attr_name);
        }
        return false;
    }
    LOG_DBG("%s: %u", p_attr_name, *p_val);
    return true;
}

JSON_RUUVI_STATIC
bool
json_ruuvi_get_int8_val(
    const cJSON *p_json_root,
    const char * p_attr_name,
    int8_t *     p_val,
    bool         flag_log_err_if_not_found)
{
    if (!json_wrap_get_int8_val(p_json_root, p_attr_name, p_val))
    {
        if (flag_log_err_if_not_found)
        {
            LOG_ERR("%s not found or invalid", p_attr_name);
        }
        return false;
    }
    LOG_DBG("%s: %d", p_attr_name, (printf_int_t)*p_val);
    return true;
}

JSON_RUUVI_STATIC
bool
json_ruuvi_parse(const cJSON *p_json_root, ruuvi_gateway_config_t *const p_gw_cfg, bool *const p_flag_network_cfg)
{
    LOG_DBG("Got SETTINGS:");

    bool use_eth = false;
    if (json_wrap_get_bool_val(p_json_root, "use_eth", &use_eth))
    {
        LOG_DBG("%s: %d", "use_eth", use_eth);
        *p_flag_network_cfg = true;

        p_gw_cfg->eth         = g_gateway_config_default.eth;
        p_gw_cfg->eth.use_eth = use_eth;

        if (use_eth)
        {
            json_ruuvi_get_bool_val(p_json_root, "eth_dhcp", &p_gw_cfg->eth.eth_dhcp, true);
            if (!p_gw_cfg->eth.eth_dhcp)
            {
                json_ruuvi_copy_string_val(
                    p_json_root,
                    "eth_static_ip",
                    p_gw_cfg->eth.eth_static_ip,
                    sizeof(p_gw_cfg->eth.eth_static_ip),
                    true);
                json_ruuvi_copy_string_val(
                    p_json_root,
                    "eth_netmask",
                    p_gw_cfg->eth.eth_netmask,
                    sizeof(p_gw_cfg->eth.eth_netmask),
                    true);
                json_ruuvi_copy_string_val(
                    p_json_root,
                    "eth_gw",
                    p_gw_cfg->eth.eth_gw,
                    sizeof(p_gw_cfg->eth.eth_gw),
                    true);
                json_ruuvi_copy_string_val(
                    p_json_root,
                    "eth_dns1",
                    p_gw_cfg->eth.eth_dns1,
                    sizeof(p_gw_cfg->eth.eth_dns1),
                    true);
                json_ruuvi_copy_string_val(
                    p_json_root,
                    "eth_dns2",
                    p_gw_cfg->eth.eth_dns2,
                    sizeof(p_gw_cfg->eth.eth_dns2),
                    true);
            }
        }
    }
    else
    {
        *p_flag_network_cfg = false;

        const ruuvi_gw_cfg_eth_t      saved_eth_cfg  = p_gw_cfg->eth;
        const ruuvi_gw_cfg_lan_auth_t saved_lan_auth = p_gw_cfg->lan_auth;

        *p_gw_cfg          = g_gateway_config_default;
        p_gw_cfg->eth      = saved_eth_cfg;
        p_gw_cfg->lan_auth = saved_lan_auth;

        json_ruuvi_get_bool_val(p_json_root, "use_mqtt", &p_gw_cfg->mqtt.use_mqtt, true);
        json_ruuvi_copy_string_val(
            p_json_root,
            "mqtt_server",
            p_gw_cfg->mqtt.mqtt_server,
            sizeof(p_gw_cfg->mqtt.mqtt_server),
            true);
        json_ruuvi_copy_string_val(
            p_json_root,
            "mqtt_prefix",
            p_gw_cfg->mqtt.mqtt_prefix,
            sizeof(p_gw_cfg->mqtt.mqtt_prefix),
            true);
        json_ruuvi_copy_string_val(
            p_json_root,
            "mqtt_client_id",
            p_gw_cfg->mqtt.mqtt_client_id,
            sizeof(p_gw_cfg->mqtt.mqtt_client_id),
            true);
        json_ruuvi_get_uint16_val(p_json_root, "mqtt_port", &p_gw_cfg->mqtt.mqtt_port, true);
        json_ruuvi_copy_string_val(
            p_json_root,
            "mqtt_user",
            p_gw_cfg->mqtt.mqtt_user,
            sizeof(p_gw_cfg->mqtt.mqtt_user),
            true);
        if (!json_ruuvi_copy_string_val(
                p_json_root,
                "mqtt_pass",
                p_gw_cfg->mqtt.mqtt_pass,
                sizeof(p_gw_cfg->mqtt.mqtt_pass),
                false))
        {
            LOG_WARN("mqtt_pass not found or not changed");
        }

        json_ruuvi_get_bool_val(p_json_root, "use_http", &p_gw_cfg->http.use_http, true);
        json_ruuvi_copy_string_val(
            p_json_root,
            "http_url",
            p_gw_cfg->http.http_url,
            sizeof(p_gw_cfg->http.http_url),
            true);
        json_ruuvi_copy_string_val(
            p_json_root,
            "http_user",
            p_gw_cfg->http.http_user,
            sizeof(p_gw_cfg->http.http_user),
            true);
        json_ruuvi_copy_string_val(
            p_json_root,
            "http_pass",
            p_gw_cfg->http.http_pass,
            sizeof(p_gw_cfg->http.http_pass),
            true);

        if (!json_ruuvi_copy_string_val(
                p_json_root,
                "lan_auth_type",
                p_gw_cfg->lan_auth.lan_auth_type,
                sizeof(p_gw_cfg->lan_auth.lan_auth_type),
                true))
        {
            LOG_INFO("Use previous LAN auth settings");
        }
        else
        {
            json_ruuvi_copy_string_val(
                p_json_root,
                "lan_auth_user",
                p_gw_cfg->lan_auth.lan_auth_user,
                sizeof(p_gw_cfg->lan_auth.lan_auth_user),
                true);
            json_ruuvi_copy_string_val(
                p_json_root,
                "lan_auth_pass",
                p_gw_cfg->lan_auth.lan_auth_pass,
                sizeof(p_gw_cfg->lan_auth.lan_auth_pass),
                true);
        }

        char auto_update_cycle[AUTO_UPDATE_CYCLE_TYPE_STR_MAX_LEN];
        json_ruuvi_copy_string_val(
            p_json_root,
            "auto_update_cycle",
            auto_update_cycle,
            sizeof(auto_update_cycle),
            true);
        if (0 == strcmp(AUTO_UPDATE_CYCLE_TYPE_STR_REGULAR, auto_update_cycle))
        {
            p_gw_cfg->auto_update.auto_update_cycle = AUTO_UPDATE_CYCLE_TYPE_REGULAR;
        }
        else if (0 == strcmp(AUTO_UPDATE_CYCLE_TYPE_STR_BETA_TESTER, auto_update_cycle))
        {
            p_gw_cfg->auto_update.auto_update_cycle = AUTO_UPDATE_CYCLE_TYPE_BETA_TESTER;
        }
        else if (0 == strcmp(AUTO_UPDATE_CYCLE_TYPE_STR_MANUAL, auto_update_cycle))
        {
            p_gw_cfg->auto_update.auto_update_cycle = AUTO_UPDATE_CYCLE_TYPE_MANUAL;
        }
        else
        {
            p_gw_cfg->auto_update.auto_update_cycle = AUTO_UPDATE_CYCLE_TYPE_MANUAL;
        }
        json_ruuvi_get_uint8_val(
            p_json_root,
            "auto_update_weekdays_bitmask",
            &p_gw_cfg->auto_update.auto_update_weekdays_bitmask,
            true);
        json_ruuvi_get_uint8_val(
            p_json_root,
            "auto_update_interval_from",
            &p_gw_cfg->auto_update.auto_update_interval_from,
            true);
        json_ruuvi_get_uint8_val(
            p_json_root,
            "auto_update_interval_to",
            &p_gw_cfg->auto_update.auto_update_interval_to,
            true);
        json_ruuvi_get_int8_val(
            p_json_root,
            "auto_update_tz_offset_hours",
            &p_gw_cfg->auto_update.auto_update_tz_offset_hours,
            true);

        json_ruuvi_get_bool_val(p_json_root, "use_filtering", &p_gw_cfg->filter.company_filter, true);
        json_ruuvi_get_uint16_val(p_json_root, "company_id", &p_gw_cfg->filter.company_id, true);

        json_ruuvi_copy_string_val(
            p_json_root,
            "coordinates",
            p_gw_cfg->coordinates,
            sizeof(p_gw_cfg->coordinates),
            true);

        json_ruuvi_get_bool_val(p_json_root, "use_coded_phy", &p_gw_cfg->scan.scan_coded_phy, true);
        json_ruuvi_get_bool_val(p_json_root, "use_1mbit_phy", &p_gw_cfg->scan.scan_1mbit_phy, true);
        json_ruuvi_get_bool_val(p_json_root, "use_extended_payload", &p_gw_cfg->scan.scan_extended_payload, true);
        json_ruuvi_get_bool_val(p_json_root, "use_channel_37", &p_gw_cfg->scan.scan_channel_37, true);
        json_ruuvi_get_bool_val(p_json_root, "use_channel_38", &p_gw_cfg->scan.scan_channel_38, true);
        json_ruuvi_get_bool_val(p_json_root, "use_channel_39", &p_gw_cfg->scan.scan_channel_39, true);
    }
    return true;
}

bool
json_ruuvi_parse_http_body(const char *p_body, ruuvi_gateway_config_t *const p_gw_cfg, bool *const p_flag_network_cfg)
{
    cJSON *p_json_root = cJSON_Parse(p_body);
    if (NULL == p_json_root)
    {
        LOG_ERR("Failed to parse json or no memory");
        return false;
    }
    const bool ret = json_ruuvi_parse(p_json_root, p_gw_cfg, p_flag_network_cfg);
    cJSON_Delete(p_json_root);
    return ret;
}
