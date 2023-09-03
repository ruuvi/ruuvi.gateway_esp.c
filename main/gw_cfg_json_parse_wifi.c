/**
 * @file gw_cfg_json_parse_wifi.c
 * @author TheSomeMan
 * @date 2021-09-29
 * @copyright Ruuvi Innovations Ltd, license BSD-3-Clause.
 */

#include "gw_cfg_json_parse_wifi.h"
#include "gw_cfg_json_parse_internal.h"
#include "cjson_wrap.h"

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

void
gw_cfg_json_parse_wifi_sta_config(const cJSON* const p_cjson, wifi_sta_config_t* const p_sta_cfg)
{
    if (!json_wrap_copy_string_val(p_cjson, "ssid", (char*)p_sta_cfg->ssid, sizeof(p_sta_cfg->ssid)))
    {
        LOG_WARN("Can't find key '%s' in config-json", "wifi_sta_config/ssid");
    }
    if (!json_wrap_copy_string_val(p_cjson, "password", (char*)p_sta_cfg->password, sizeof(p_sta_cfg->password)))
    {
        LOG_WARN("Can't find key '%s' in config-json", "wifi_sta_config/password");
    }
}

void
gw_cfg_json_parse_wifi_sta_settings(
    const cJSON* const         p_cjson,
    wifi_settings_sta_t* const p_sta_settings) // NOSONAR
{
    (void)p_cjson;
    (void)p_sta_settings;
    // Storing wifi_sta_settings settings in json is not currently supported.

    (void)p_sta_settings->sta_power_save;
    (void)p_sta_settings->sta_static_ip;
    (void)p_sta_settings->sta_static_ip_config;
}

void
gw_cfg_json_parse_wifi_ap_config(const cJSON* const p_cjson, wifi_ap_config_t* const p_ap_cfg)
{
    if (!json_wrap_copy_string_val(p_cjson, "password", (char*)p_ap_cfg->password, sizeof(p_ap_cfg->password)))
    {
        LOG_WARN("Can't find key '%s' in config-json", "wifi_ap_config/password");
    }
    if (!gw_cfg_json_get_uint8_val(p_cjson, "channel", &p_ap_cfg->channel))
    {
        LOG_WARN("Can't find key '%s' in config-json", "wifi_ap_config/channel");
    }
    if (0 == p_ap_cfg->channel)
    {
        p_ap_cfg->channel = 1;
        LOG_WARN(
            "Key '%s' in config-json is zero, use default value: %d",
            "wifi_ap_config/channel",
            (printf_int_t)p_ap_cfg->channel);
    }
}

void
gw_cfg_json_parse_wifi_ap_settings(const cJSON* const p_cjson, wifi_settings_ap_t* const p_ap_settings) // NOSONAR
{
    (void)p_cjson;
    (void)p_ap_settings;
    // Storing wifi_settings_ap in json is not currently supported.

    (void)p_ap_settings->ap_bandwidth;
    (void)p_ap_settings->ap_ip;
    (void)p_ap_settings->ap_gw;
    (void)p_ap_settings->ap_netmask;
}
