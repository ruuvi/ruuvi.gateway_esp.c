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
#include "os_malloc.h"

#if !defined(RUUVI_TESTS_HTTP_SERVER_CB)
#define RUUVI_TESTS_HTTP_SERVER_CB (0)
#endif

#if RUUVI_TESTS_HTTP_SERVER_CB || RUUVI_TESTS_JSON_RUUVI
#define LOG_LOCAL_LEVEL LOG_LEVEL_DEBUG
#else
// Warning: Debug log level prints out the passwords as a "plaintext" so accidents won't happen.
#define LOG_LOCAL_LEVEL LOG_LEVEL_INFO
#endif
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
json_ruuvi_parse_network_cfg(const cJSON *p_json_root, const bool use_eth, ruuvi_gateway_config_t *const p_gw_cfg)
{
    LOG_DBG("%s: %d", "use_eth", use_eth);

    p_gw_cfg->eth         = gw_cfg_default_get_eth();
    p_gw_cfg->eth.use_eth = use_eth;

    if (use_eth)
    {
        json_ruuvi_get_bool_val(p_json_root, "eth_dhcp", &p_gw_cfg->eth.eth_dhcp, true);
        if (!p_gw_cfg->eth.eth_dhcp)
        {
            json_ruuvi_copy_string_val(
                p_json_root,
                "eth_static_ip",
                p_gw_cfg->eth.eth_static_ip.buf,
                sizeof(p_gw_cfg->eth.eth_static_ip),
                true);
            json_ruuvi_copy_string_val(
                p_json_root,
                "eth_netmask",
                p_gw_cfg->eth.eth_netmask.buf,
                sizeof(p_gw_cfg->eth.eth_netmask),
                true);
            json_ruuvi_copy_string_val(
                p_json_root,
                "eth_gw",
                p_gw_cfg->eth.eth_gw.buf,
                sizeof(p_gw_cfg->eth.eth_gw),
                true);
            json_ruuvi_copy_string_val(
                p_json_root,
                "eth_dns1",
                p_gw_cfg->eth.eth_dns1.buf,
                sizeof(p_gw_cfg->eth.eth_dns1),
                true);
            json_ruuvi_copy_string_val(
                p_json_root,
                "eth_dns2",
                p_gw_cfg->eth.eth_dns2.buf,
                sizeof(p_gw_cfg->eth.eth_dns2),
                true);
        }
    }
    return true;
}

JSON_RUUVI_STATIC
bool
json_ruuvi_set_default_values_for_main_config(ruuvi_gateway_config_t *const p_gw_cfg)
{
    ruuvi_gateway_config_t *p_saved_cfg = os_malloc(sizeof(*p_saved_cfg));
    if (NULL == p_saved_cfg)
    {
        LOG_ERR("Can't allocate memory for gw_cfg copy");
        return false;
    }
    *p_saved_cfg = *p_gw_cfg;
    gw_cfg_default_get(p_gw_cfg);
    p_gw_cfg->eth                      = p_saved_cfg->eth;
    p_gw_cfg->lan_auth                 = p_saved_cfg->lan_auth;
    p_gw_cfg->mqtt.mqtt_pass           = p_saved_cfg->mqtt.mqtt_pass;
    p_gw_cfg->http.http_pass           = p_saved_cfg->http.http_pass;
    p_gw_cfg->http_stat.http_stat_pass = p_saved_cfg->http_stat.http_stat_pass;
    os_free(p_saved_cfg);
    return true;
}

JSON_RUUVI_STATIC
bool
json_ruuvi_parse_main_cfg_mqtt(const cJSON *p_json_root, ruuvi_gw_cfg_mqtt_t *const p_gw_cfg_mqtt)
{
    json_ruuvi_get_bool_val(p_json_root, "use_mqtt", &p_gw_cfg_mqtt->use_mqtt, true);
    p_gw_cfg_mqtt->mqtt_use_default_prefix = false;
    json_ruuvi_copy_string_val(
        p_json_root,
        "mqtt_transport",
        p_gw_cfg_mqtt->mqtt_transport.buf,
        sizeof(p_gw_cfg_mqtt->mqtt_transport),
        true);
    json_ruuvi_copy_string_val(
        p_json_root,
        "mqtt_server",
        p_gw_cfg_mqtt->mqtt_server.buf,
        sizeof(p_gw_cfg_mqtt->mqtt_server),
        true);
    json_ruuvi_copy_string_val(
        p_json_root,
        "mqtt_prefix",
        p_gw_cfg_mqtt->mqtt_prefix.buf,
        sizeof(p_gw_cfg_mqtt->mqtt_prefix),
        true);
    json_ruuvi_copy_string_val(
        p_json_root,
        "mqtt_client_id",
        p_gw_cfg_mqtt->mqtt_client_id.buf,
        sizeof(p_gw_cfg_mqtt->mqtt_client_id.buf),
        true);
    json_ruuvi_get_uint16_val(p_json_root, "mqtt_port", &p_gw_cfg_mqtt->mqtt_port, true);
    json_ruuvi_copy_string_val(
        p_json_root,
        "mqtt_user",
        p_gw_cfg_mqtt->mqtt_user.buf,
        sizeof(p_gw_cfg_mqtt->mqtt_user.buf),
        true);
    if (!json_ruuvi_copy_string_val(
            p_json_root,
            "mqtt_pass",
            p_gw_cfg_mqtt->mqtt_pass.buf,
            sizeof(p_gw_cfg_mqtt->mqtt_pass.buf),
            false))
    {
        LOG_INFO("mqtt_pass was not changed");
    }
    return true;
}

JSON_RUUVI_STATIC
bool
json_ruuvi_parse_main_cfg_http(const cJSON *p_json_root, ruuvi_gw_cfg_http_t *const p_gw_cfg_http)
{
    json_ruuvi_get_bool_val(p_json_root, "use_http", &p_gw_cfg_http->use_http, true);
    json_ruuvi_copy_string_val(
        p_json_root,
        "http_url",
        p_gw_cfg_http->http_url.buf,
        sizeof(p_gw_cfg_http->http_url.buf),
        true);
    json_ruuvi_copy_string_val(
        p_json_root,
        "http_user",
        p_gw_cfg_http->http_user.buf,
        sizeof(p_gw_cfg_http->http_user.buf),
        true);
    if (!json_ruuvi_copy_string_val(
            p_json_root,
            "http_pass",
            p_gw_cfg_http->http_pass.buf,
            sizeof(p_gw_cfg_http->http_pass.buf),
            false))
    {
        LOG_INFO("http_pass was not changed");
    }
    return true;
}

JSON_RUUVI_STATIC
bool
json_ruuvi_parse_main_cfg_http_stat(const cJSON *p_json_root, ruuvi_gw_cfg_http_stat_t *const p_gw_cfg_http_stat)
{
    json_ruuvi_get_bool_val(p_json_root, "use_http_stat", &p_gw_cfg_http_stat->use_http_stat, true);
    json_ruuvi_copy_string_val(
        p_json_root,
        "http_stat_url",
        p_gw_cfg_http_stat->http_stat_url.buf,
        sizeof(p_gw_cfg_http_stat->http_stat_url.buf),
        true);
    json_ruuvi_copy_string_val(
        p_json_root,
        "http_stat_user",
        p_gw_cfg_http_stat->http_stat_user.buf,
        sizeof(p_gw_cfg_http_stat->http_stat_user.buf),
        true);
    if (!json_ruuvi_copy_string_val(
            p_json_root,
            "http_stat_pass",
            p_gw_cfg_http_stat->http_stat_pass.buf,
            sizeof(p_gw_cfg_http_stat->http_stat_pass.buf),
            false))
    {
        LOG_INFO("http_stat_pass was not changed");
    }
    return true;
}

JSON_RUUVI_STATIC
bool
json_ruuvi_parse_main_cfg_lan_auth(const cJSON *p_json_root, ruuvi_gw_cfg_lan_auth_t *const p_gw_cfg_lan_auth)
{
    char lan_auth_type[MAX_LAN_AUTH_TYPE_LEN] = { '\0' };
    if (!json_ruuvi_copy_string_val(p_json_root, "lan_auth_type", lan_auth_type, sizeof(lan_auth_type), true))
    {
        LOG_INFO("Use previous LAN auth settings");
    }
    else
    {
        if ((0 == strcmp(HTTP_SERVER_AUTH_TYPE_STR_RUUVI, lan_auth_type))
            || (0 == strcmp(HTTP_SERVER_AUTH_TYPE_STR_DIGEST, lan_auth_type))
            || (0 == strcmp(HTTP_SERVER_AUTH_TYPE_STR_BASIC, lan_auth_type)))
        {
            snprintf(p_gw_cfg_lan_auth->lan_auth_type, sizeof(p_gw_cfg_lan_auth->lan_auth_type), "%s", lan_auth_type);
            json_ruuvi_copy_string_val(
                p_json_root,
                "lan_auth_user",
                p_gw_cfg_lan_auth->lan_auth_user,
                sizeof(p_gw_cfg_lan_auth->lan_auth_user),
                true);
            json_ruuvi_copy_string_val(
                p_json_root,
                "lan_auth_pass",
                p_gw_cfg_lan_auth->lan_auth_pass,
                sizeof(p_gw_cfg_lan_auth->lan_auth_pass),
                true);
        }
        else if (
            (0 == strcmp(HTTP_SERVER_AUTH_TYPE_STR_DENY, lan_auth_type))
            || (0 == strcmp(HTTP_SERVER_AUTH_TYPE_STR_ALLOW, lan_auth_type)))
        {
            snprintf(p_gw_cfg_lan_auth->lan_auth_type, sizeof(p_gw_cfg_lan_auth->lan_auth_type), "%s", lan_auth_type);
            p_gw_cfg_lan_auth->lan_auth_user[0] = '\0';
            p_gw_cfg_lan_auth->lan_auth_pass[0] = '\0';
        }
        else
        {
            snprintf(
                p_gw_cfg_lan_auth->lan_auth_type,
                sizeof(p_gw_cfg_lan_auth->lan_auth_type),
                "%s",
                HTTP_SERVER_AUTH_TYPE_STR_RUUVI);
            snprintf(
                p_gw_cfg_lan_auth->lan_auth_user,
                sizeof(p_gw_cfg_lan_auth->lan_auth_user),
                "%s",
                RUUVI_GATEWAY_AUTH_DEFAULT_USER);
            snprintf(
                p_gw_cfg_lan_auth->lan_auth_pass,
                sizeof(p_gw_cfg_lan_auth->lan_auth_pass),
                "%s",
                gw_cfg_default_get_lan_auth_password());
        }
    }
    return true;
}

JSON_RUUVI_STATIC
bool
json_ruuvi_parse_main_cfg_auto_update(const cJSON *p_json_root, ruuvi_gw_cfg_auto_update_t *const p_gw_cfg_auto_update)
{
    char auto_update_cycle[AUTO_UPDATE_CYCLE_TYPE_STR_MAX_LEN];
    json_ruuvi_copy_string_val(p_json_root, "auto_update_cycle", auto_update_cycle, sizeof(auto_update_cycle), true);
    if (0 == strcmp(AUTO_UPDATE_CYCLE_TYPE_STR_REGULAR, auto_update_cycle))
    {
        p_gw_cfg_auto_update->auto_update_cycle = AUTO_UPDATE_CYCLE_TYPE_REGULAR;
    }
    else if (0 == strcmp(AUTO_UPDATE_CYCLE_TYPE_STR_BETA_TESTER, auto_update_cycle))
    {
        p_gw_cfg_auto_update->auto_update_cycle = AUTO_UPDATE_CYCLE_TYPE_BETA_TESTER;
    }
    else if (0 == strcmp(AUTO_UPDATE_CYCLE_TYPE_STR_MANUAL, auto_update_cycle))
    {
        p_gw_cfg_auto_update->auto_update_cycle = AUTO_UPDATE_CYCLE_TYPE_MANUAL;
    }
    else
    {
        p_gw_cfg_auto_update->auto_update_cycle = AUTO_UPDATE_CYCLE_TYPE_REGULAR;
    }
    json_ruuvi_get_uint8_val(
        p_json_root,
        "auto_update_weekdays_bitmask",
        &p_gw_cfg_auto_update->auto_update_weekdays_bitmask,
        true);
    json_ruuvi_get_uint8_val(
        p_json_root,
        "auto_update_interval_from",
        &p_gw_cfg_auto_update->auto_update_interval_from,
        true);
    json_ruuvi_get_uint8_val(
        p_json_root,
        "auto_update_interval_to",
        &p_gw_cfg_auto_update->auto_update_interval_to,
        true);
    json_ruuvi_get_int8_val(
        p_json_root,
        "auto_update_tz_offset_hours",
        &p_gw_cfg_auto_update->auto_update_tz_offset_hours,
        true);
    return true;
}

JSON_RUUVI_STATIC
bool
json_ruuvi_parse_main_cfg_filter(const cJSON *p_json_root, ruuvi_gw_cfg_filter_t *const p_gw_cfg_filter)
{
    json_ruuvi_get_bool_val(p_json_root, "use_filtering", &p_gw_cfg_filter->company_use_filtering, true);
    json_ruuvi_get_uint16_val(p_json_root, "company_id", &p_gw_cfg_filter->company_id, true);
    return true;
}

JSON_RUUVI_STATIC
bool
json_ruuvi_parse_main_cfg_scan(const cJSON *p_json_root, ruuvi_gw_cfg_scan_t *const p_gw_cfg_scan)
{
    json_ruuvi_get_bool_val(p_json_root, "use_coded_phy", &p_gw_cfg_scan->scan_coded_phy, true);
    json_ruuvi_get_bool_val(p_json_root, "use_1mbit_phy", &p_gw_cfg_scan->scan_1mbit_phy, true);
    json_ruuvi_get_bool_val(p_json_root, "use_extended_payload", &p_gw_cfg_scan->scan_extended_payload, true);
    json_ruuvi_get_bool_val(p_json_root, "use_channel_37", &p_gw_cfg_scan->scan_channel_37, true);
    json_ruuvi_get_bool_val(p_json_root, "use_channel_38", &p_gw_cfg_scan->scan_channel_38, true);
    json_ruuvi_get_bool_val(p_json_root, "use_channel_39", &p_gw_cfg_scan->scan_channel_39, true);
    return true;
}

JSON_RUUVI_STATIC
bool
json_ruuvi_parse_main_cfg(const cJSON *p_json_root, ruuvi_gateway_config_t *const p_gw_cfg)
{
    if (!json_ruuvi_set_default_values_for_main_config(p_gw_cfg))
    {
        return false;
    }
    if (!json_ruuvi_parse_main_cfg_mqtt(p_json_root, &p_gw_cfg->mqtt))
    {
        return false;
    }
    if (!json_ruuvi_parse_main_cfg_http(p_json_root, &p_gw_cfg->http))
    {
        return false;
    }
    if (!json_ruuvi_parse_main_cfg_http_stat(p_json_root, &p_gw_cfg->http_stat))
    {
        return false;
    }
    if (!json_ruuvi_parse_main_cfg_lan_auth(p_json_root, &p_gw_cfg->lan_auth))
    {
        return false;
    }
    if (!json_ruuvi_parse_main_cfg_auto_update(p_json_root, &p_gw_cfg->auto_update))
    {
        return false;
    }
    if (!json_ruuvi_parse_main_cfg_filter(p_json_root, &p_gw_cfg->filter))
    {
        return false;
    }

    json_ruuvi_copy_string_val(
        p_json_root,
        "coordinates",
        p_gw_cfg->coordinates.buf,
        sizeof(p_gw_cfg->coordinates.buf),
        true);

    if (!json_ruuvi_parse_main_cfg_scan(p_json_root, &p_gw_cfg->scan))
    {
        return false;
    }
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
        *p_flag_network_cfg = true;
        return json_ruuvi_parse_network_cfg(p_json_root, use_eth, p_gw_cfg);
    }
    *p_flag_network_cfg = false;
    return json_ruuvi_parse_main_cfg(p_json_root, p_gw_cfg);
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
