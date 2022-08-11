/**
 * @file gw_cfg_json_generate.c
 * @author TheSomeMan
 * @date 2022-05-18
 * @copyright Ruuvi Innovations Ltd, license BSD-3-Clause.
 */

#include "gw_cfg_json_generate.h"

#if !defined(RUUVI_TESTS_HTTP_SERVER_CB)
#define RUUVI_TESTS_HTTP_SERVER_CB 0
#endif

#if !defined(RUUVI_TESTS_JSON_RUUVI)
#define RUUVI_TESTS_JSON_RUUVI 0
#endif

#if RUUVI_TESTS_HTTP_SERVER_CB || RUUVI_TESTS_JSON_RUUVI
#define LOG_LOCAL_LEVEL LOG_LEVEL_DEBUG
#else
#define LOG_LOCAL_LEVEL LOG_LEVEL_INFO
#endif
#include "log.h"

#if (LOG_LOCAL_LEVEL >= LOG_LEVEL_DEBUG) && !RUUVI_TESTS
#warning Debug log level prints out the passwords as a "plaintext".
#endif

static const char TAG[] = "gw_cfg";

static bool
gw_cfg_json_add_bool(cJSON *const p_json_root, const char *const p_item_name, const bool val)
{
    if (NULL == cJSON_AddBoolToObject(p_json_root, p_item_name, val))
    {
        LOG_ERR("Can't add json item: %s", p_item_name);
        return false;
    }
    return true;
}

static bool
gw_cfg_json_add_string(cJSON *const p_json_root, const char *const p_item_name, const char *p_val)
{
    if (NULL == cJSON_AddStringToObject(p_json_root, p_item_name, p_val))
    {
        LOG_ERR("Can't add json item: %s", p_item_name);
        return false;
    }
    return true;
}

static bool
gw_cfg_json_add_number(cJSON *const p_json_root, const char *const p_item_name, const cjson_number_t val)
{
    if (NULL == cJSON_AddNumberToObject(p_json_root, p_item_name, val))
    {
        LOG_ERR("Can't add json item: %s", p_item_name);
        return false;
    }
    return true;
}

static bool
gw_cfg_json_add_items_device_info(cJSON *const p_json_root, const gw_cfg_device_info_t *const p_dev_info)
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
gw_cfg_json_add_items_wifi_sta_config(
    cJSON *const                  p_json_root,
    const wifiman_config_t *const p_wifi_cfg,
    const bool                    flag_hide_passwords)
{
    cJSON *const p_cjson = cJSON_AddObjectToObject(p_json_root, "wifi_sta_config");
    if (NULL == p_cjson)
    {
        LOG_ERR("Can't add json item: %s", "wifi_sta_config");
        return false;
    }
    const wifi_sta_config_t *const p_wifi_cfg_sta = &p_wifi_cfg->sta.wifi_config_sta;
    if (!gw_cfg_json_add_string(p_cjson, "ssid", (char *)p_wifi_cfg_sta->ssid))
    {
        return false;
    }
    if (!flag_hide_passwords)
    {
        if (!gw_cfg_json_add_string(p_cjson, "password", (char *)p_wifi_cfg_sta->password))
        {
            return false;
        }
    }
    return true;
}

static bool
gw_cfg_json_add_items_wifi_ap_config(
    cJSON *const                  p_json_root,
    const wifiman_config_t *const p_wifi_cfg,
    const bool                    flag_hide_passwords)
{
    cJSON *const p_cjson = cJSON_AddObjectToObject(p_json_root, "wifi_ap_config");
    if (NULL == p_cjson)
    {
        LOG_ERR("Can't add json item: %s", "wifi_ap_config");
        return false;
    }
    const wifi_ap_config_t *const p_wifi_cfg_ap = &p_wifi_cfg->ap.wifi_config_ap;
    if (!flag_hide_passwords)
    {
        if (!gw_cfg_json_add_string(p_cjson, "password", (char *)p_wifi_cfg_ap->password))
        {
            return false;
        }
    }
    if (!gw_cfg_json_add_number(p_cjson, "channel", (0 != p_wifi_cfg_ap->channel) ? p_wifi_cfg_ap->channel : 1))
    {
        return false;
    }
    return true;
}

static bool
gw_cfg_json_add_items_eth(cJSON *const p_json_root, const gw_cfg_eth_t *const p_cfg_eth)
{
    if (!gw_cfg_json_add_bool(p_json_root, "use_eth", p_cfg_eth->use_eth))
    {
        return false;
    }
    if (!gw_cfg_json_add_bool(p_json_root, "eth_dhcp", p_cfg_eth->eth_dhcp))
    {
        return false;
    }
    if (!gw_cfg_json_add_string(p_json_root, "eth_static_ip", p_cfg_eth->eth_static_ip.buf))
    {
        return false;
    }
    if (!gw_cfg_json_add_string(p_json_root, "eth_netmask", p_cfg_eth->eth_netmask.buf))
    {
        return false;
    }
    if (!gw_cfg_json_add_string(p_json_root, "eth_gw", p_cfg_eth->eth_gw.buf))
    {
        return false;
    }
    if (!gw_cfg_json_add_string(p_json_root, "eth_dns1", p_cfg_eth->eth_dns1.buf))
    {
        return false;
    }
    if (!gw_cfg_json_add_string(p_json_root, "eth_dns2", p_cfg_eth->eth_dns2.buf))
    {
        return false;
    }
    return true;
}

static bool
gw_cfg_json_add_items_remote_auth_basic(
    cJSON *const                       p_json_root,
    const ruuvi_gw_cfg_remote_t *const p_cfg_remote,
    const bool                         flag_hide_passwords)
{
    if (!gw_cfg_json_add_string(p_json_root, "remote_cfg_auth_type", GW_CFG_REMOTE_AUTH_TYPE_STR_BASIC))
    {
        return false;
    }
    if (!gw_cfg_json_add_string(p_json_root, "remote_cfg_auth_basic_user", p_cfg_remote->auth.auth_basic.user.buf))
    {
        return false;
    }
    if (!flag_hide_passwords)
    {
        if (!gw_cfg_json_add_string(
                p_json_root,
                "remote_cfg_auth_basic_pass",
                p_cfg_remote->auth.auth_basic.password.buf))
        {
            return false;
        }
    }
    return true;
}

static bool
gw_cfg_json_add_items_remote_auth_bearer(
    cJSON *const                       p_json_root,
    const ruuvi_gw_cfg_remote_t *const p_cfg_remote,
    const bool                         flag_hide_passwords)
{
    if (!gw_cfg_json_add_string(p_json_root, "remote_cfg_auth_type", GW_CFG_REMOTE_AUTH_TYPE_STR_BEARER))
    {
        return false;
    }
    if ((!flag_hide_passwords)
        && (!gw_cfg_json_add_string(
            p_json_root,
            "remote_cfg_auth_bearer_token",
            p_cfg_remote->auth.auth_bearer.token.buf)))
    {
        return false;
    }
    return true;
}

static bool
gw_cfg_json_add_items_remote(
    cJSON *const                       p_json_root,
    const ruuvi_gw_cfg_remote_t *const p_cfg_remote,
    const bool                         flag_hide_passwords)
{
    if (!gw_cfg_json_add_bool(p_json_root, "remote_cfg_use", p_cfg_remote->use_remote_cfg))
    {
        return false;
    }
    if (!gw_cfg_json_add_string(p_json_root, "remote_cfg_url", p_cfg_remote->url.buf))
    {
        return false;
    }
    switch (p_cfg_remote->auth_type)
    {
        case GW_CFG_REMOTE_AUTH_TYPE_NO:
            if (!gw_cfg_json_add_string(p_json_root, "remote_cfg_auth_type", GW_CFG_REMOTE_AUTH_TYPE_STR_NO))
            {
                return false;
            }
            break;
        case GW_CFG_REMOTE_AUTH_TYPE_BASIC:
            if (!gw_cfg_json_add_items_remote_auth_basic(p_json_root, p_cfg_remote, flag_hide_passwords))
            {
                return false;
            }
            break;
        case GW_CFG_REMOTE_AUTH_TYPE_BEARER:
            if (!gw_cfg_json_add_items_remote_auth_bearer(p_json_root, p_cfg_remote, flag_hide_passwords))
            {
                return false;
            }
            break;
    }
    if (!gw_cfg_json_add_number(
            p_json_root,
            "remote_cfg_refresh_interval_minutes",
            p_cfg_remote->refresh_interval_minutes))
    {
        return false;
    }
    return true;
}

static bool
gw_cfg_json_add_items_http(
    cJSON *const                     p_json_root,
    const ruuvi_gw_cfg_http_t *const p_cfg_http,
    const bool                       flag_hide_passwords)
{
    if (!gw_cfg_json_add_bool(p_json_root, "use_http", p_cfg_http->use_http))
    {
        return false;
    }
    if (!gw_cfg_json_add_string(p_json_root, "http_url", p_cfg_http->http_url.buf))
    {
        return false;
    }
    if (!gw_cfg_json_add_string(p_json_root, "http_user", p_cfg_http->http_user.buf))
    {
        return false;
    }
    if ((!flag_hide_passwords) && (!gw_cfg_json_add_string(p_json_root, "http_pass", p_cfg_http->http_pass.buf)))
    {
        return false;
    }
    return true;
}

static bool
gw_cfg_json_add_items_http_stat(
    cJSON *const                          p_json_root,
    const ruuvi_gw_cfg_http_stat_t *const p_cfg_http_stat,
    const bool                            flag_hide_passwords)
{
    if (!gw_cfg_json_add_bool(p_json_root, "use_http_stat", p_cfg_http_stat->use_http_stat))
    {
        return false;
    }
    if (!gw_cfg_json_add_string(p_json_root, "http_stat_url", p_cfg_http_stat->http_stat_url.buf))
    {
        return false;
    }
    if (!gw_cfg_json_add_string(p_json_root, "http_stat_user", p_cfg_http_stat->http_stat_user.buf))
    {
        return false;
    }
    if ((!flag_hide_passwords)
        && (!gw_cfg_json_add_string(p_json_root, "http_stat_pass", p_cfg_http_stat->http_stat_pass.buf)))
    {
        return false;
    }
    return true;
}

static bool
gw_cfg_json_add_items_mqtt(
    cJSON *const                     p_json_root,
    const ruuvi_gw_cfg_mqtt_t *const p_cfg_mqtt,
    const bool                       flag_hide_passwords)
{
    if (!gw_cfg_json_add_bool(p_json_root, "use_mqtt", p_cfg_mqtt->use_mqtt))
    {
        return false;
    }
    if (!gw_cfg_json_add_string(p_json_root, "mqtt_transport", p_cfg_mqtt->mqtt_transport.buf))
    {
        return false;
    }
    if (!gw_cfg_json_add_string(p_json_root, "mqtt_server", p_cfg_mqtt->mqtt_server.buf))
    {
        return false;
    }
    if (!gw_cfg_json_add_number(p_json_root, "mqtt_port", p_cfg_mqtt->mqtt_port))
    {
        return false;
    }
    if (!gw_cfg_json_add_string(p_json_root, "mqtt_prefix", p_cfg_mqtt->mqtt_prefix.buf))
    {
        return false;
    }
    if (!gw_cfg_json_add_string(p_json_root, "mqtt_client_id", p_cfg_mqtt->mqtt_client_id.buf))
    {
        return false;
    }
    if (!gw_cfg_json_add_string(p_json_root, "mqtt_user", p_cfg_mqtt->mqtt_user.buf))
    {
        return false;
    }
    if ((!flag_hide_passwords) && (!gw_cfg_json_add_string(p_json_root, "mqtt_pass", p_cfg_mqtt->mqtt_pass.buf)))
    {
        return false;
    }
    return true;
}

static bool
gw_cfg_json_add_items_lan_auth(
    cJSON *const                         p_json_root,
    const ruuvi_gw_cfg_lan_auth_t *const p_cfg_lan_auth,
    const bool                           flag_hide_passwords)
{
    const char *const p_lan_auth_type_str = gw_cfg_auth_type_to_str(p_cfg_lan_auth);
    if (!gw_cfg_json_add_string(p_json_root, "lan_auth_type", p_lan_auth_type_str))
    {
        return false;
    }
    if (!gw_cfg_json_add_string(p_json_root, "lan_auth_user", p_cfg_lan_auth->lan_auth_user.buf))
    {
        return false;
    }
    if (!flag_hide_passwords)
    {
        switch (p_cfg_lan_auth->lan_auth_type)
        {
            case HTTP_SERVER_AUTH_TYPE_BASIC:
            case HTTP_SERVER_AUTH_TYPE_DIGEST:
            case HTTP_SERVER_AUTH_TYPE_RUUVI:
                if (!gw_cfg_json_add_string(p_json_root, "lan_auth_pass", p_cfg_lan_auth->lan_auth_pass.buf))
                {
                    return false;
                }
                break;
            default:
                break;
        }
    }
    if (flag_hide_passwords)
    {
        if (!gw_cfg_json_add_bool(
                p_json_root,
                "lan_auth_api_key_use",
                ('\0' != p_cfg_lan_auth->lan_auth_api_key.buf[0]) ? true : false))
        {
            return false;
        }
    }
    else
    {
        if (!gw_cfg_json_add_string(p_json_root, "lan_auth_api_key", p_cfg_lan_auth->lan_auth_api_key.buf))
        {
            return false;
        }
    }
    return true;
}

static bool
gw_cfg_json_add_items_auto_update(cJSON *const p_json_root, const ruuvi_gw_cfg_auto_update_t *const p_cfg_auto_update)
{
    const char *p_auto_update_cycle_str = AUTO_UPDATE_CYCLE_TYPE_STR_REGULAR;
    switch (p_cfg_auto_update->auto_update_cycle)
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
            p_cfg_auto_update->auto_update_weekdays_bitmask))
    {
        return false;
    }
    if (!gw_cfg_json_add_number(p_json_root, "auto_update_interval_from", p_cfg_auto_update->auto_update_interval_from))
    {
        return false;
    }
    if (!gw_cfg_json_add_number(p_json_root, "auto_update_interval_to", p_cfg_auto_update->auto_update_interval_to))
    {
        return false;
    }
    if (!gw_cfg_json_add_number(
            p_json_root,
            "auto_update_tz_offset_hours",
            p_cfg_auto_update->auto_update_tz_offset_hours))
    {
        return false;
    }
    return true;
}

static bool
gw_cfg_json_add_items_ntp(cJSON *const p_json_root, const ruuvi_gw_cfg_ntp_t *const p_cfg_ntp)
{
    if (!gw_cfg_json_add_bool(p_json_root, "ntp_use", p_cfg_ntp->ntp_use))
    {
        return false;
    }
    if (!gw_cfg_json_add_bool(p_json_root, "ntp_use_dhcp", p_cfg_ntp->ntp_use_dhcp))
    {
        return false;
    }
    if (!gw_cfg_json_add_string(p_json_root, "ntp_server1", p_cfg_ntp->ntp_server1.buf))
    {
        return false;
    }
    if (!gw_cfg_json_add_string(p_json_root, "ntp_server2", p_cfg_ntp->ntp_server2.buf))
    {
        return false;
    }
    if (!gw_cfg_json_add_string(p_json_root, "ntp_server3", p_cfg_ntp->ntp_server3.buf))
    {
        return false;
    }
    if (!gw_cfg_json_add_string(p_json_root, "ntp_server4", p_cfg_ntp->ntp_server4.buf))
    {
        return false;
    }
    return true;
}

static bool
gw_cfg_json_add_items_filter(cJSON *const p_json_root, const ruuvi_gw_cfg_filter_t *const p_cfg_filter)
{
    if (!gw_cfg_json_add_number(p_json_root, "company_id", p_cfg_filter->company_id))
    {
        return false;
    }
    if (!gw_cfg_json_add_bool(p_json_root, "company_use_filtering", p_cfg_filter->company_use_filtering))
    {
        return false;
    }
    return true;
}

static bool
gw_cfg_json_add_items_scan(cJSON *const p_json_root, const ruuvi_gw_cfg_scan_t *const p_cfg_scan)
{
    if (!gw_cfg_json_add_bool(p_json_root, "scan_coded_phy", p_cfg_scan->scan_coded_phy))
    {
        return false;
    }
    if (!gw_cfg_json_add_bool(p_json_root, "scan_1mbit_phy", p_cfg_scan->scan_1mbit_phy))
    {
        return false;
    }
    if (!gw_cfg_json_add_bool(p_json_root, "scan_extended_payload", p_cfg_scan->scan_extended_payload))
    {
        return false;
    }
    if (!gw_cfg_json_add_bool(p_json_root, "scan_channel_37", p_cfg_scan->scan_channel_37))
    {
        return false;
    }
    if (!gw_cfg_json_add_bool(p_json_root, "scan_channel_38", p_cfg_scan->scan_channel_38))
    {
        return false;
    }
    if (!gw_cfg_json_add_bool(p_json_root, "scan_channel_39", p_cfg_scan->scan_channel_39))
    {
        return false;
    }
    return true;
}

static bool
gw_cfg_json_add_items(cJSON *const p_json_root, const gw_cfg_t *p_cfg, const bool flag_hide_passwords)
{
    if (!gw_cfg_json_add_items_device_info(p_json_root, &p_cfg->device_info))
    {
        return false;
    }
    if (!gw_cfg_json_add_items_wifi_sta_config(p_json_root, &p_cfg->wifi_cfg, flag_hide_passwords))
    {
        return false;
    }
    if (!gw_cfg_json_add_items_wifi_ap_config(p_json_root, &p_cfg->wifi_cfg, flag_hide_passwords))
    {
        return false;
    }
    if (!gw_cfg_json_add_items_eth(p_json_root, &p_cfg->eth_cfg))
    {
        return false;
    }
    if (!gw_cfg_json_add_items_remote(p_json_root, &p_cfg->ruuvi_cfg.remote, flag_hide_passwords))
    {
        return false;
    }
    if (!gw_cfg_json_add_items_http(p_json_root, &p_cfg->ruuvi_cfg.http, flag_hide_passwords))
    {
        return false;
    }
    if (!gw_cfg_json_add_items_http_stat(p_json_root, &p_cfg->ruuvi_cfg.http_stat, flag_hide_passwords))
    {
        return false;
    }
    if (!gw_cfg_json_add_items_mqtt(p_json_root, &p_cfg->ruuvi_cfg.mqtt, flag_hide_passwords))
    {
        return false;
    }
    if (!gw_cfg_json_add_items_lan_auth(p_json_root, &p_cfg->ruuvi_cfg.lan_auth, flag_hide_passwords))
    {
        return false;
    }
    if (!gw_cfg_json_add_items_auto_update(p_json_root, &p_cfg->ruuvi_cfg.auto_update))
    {
        return false;
    }
    if (!gw_cfg_json_add_items_ntp(p_json_root, &p_cfg->ruuvi_cfg.ntp))
    {
        return false;
    }
    if (!gw_cfg_json_add_items_filter(p_json_root, &p_cfg->ruuvi_cfg.filter))
    {
        return false;
    }
    if (!gw_cfg_json_add_items_scan(p_json_root, &p_cfg->ruuvi_cfg.scan))
    {
        return false;
    }
    if (!gw_cfg_json_add_string(p_json_root, "coordinates", p_cfg->ruuvi_cfg.coordinates.buf))
    {
        return false;
    }
    return true;
}

static bool
gw_cfg_json_generate(const gw_cfg_t *const p_gw_cfg, cjson_wrap_str_t *const p_json_str, const bool flag_hide_passwords)
{
    p_json_str->p_str = NULL;

    cJSON *p_json_root = cJSON_CreateObject();
    if (NULL == p_json_root)
    {
        LOG_ERR("Can't create json object");
        return false;
    }
    if (!gw_cfg_json_add_items(p_json_root, p_gw_cfg, flag_hide_passwords))
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

bool
gw_cfg_json_generate_full(const gw_cfg_t *const p_gw_cfg, cjson_wrap_str_t *const p_json_str)
{
    return gw_cfg_json_generate(p_gw_cfg, p_json_str, false);
}

bool
gw_cfg_json_generate_without_passwords(const gw_cfg_t *const p_gw_cfg, cjson_wrap_str_t *const p_json_str)
{
    return gw_cfg_json_generate(p_gw_cfg, p_json_str, true);
}
