/**
 * @file gw_cfg_json.c
 * @author TheSomeMan
 * @date 2021-09-29
 * @copyright Ruuvi Innovations Ltd, license BSD-3-Clause.
 */

#include "gw_cfg_json_parse.h"
#include "gw_cfg_log.h"
#include "gw_cfg_json_parse_internal.h"
#include "gw_cfg_json_parse_device_info.h"
#include "gw_cfg_json_parse_eth.h"
#include "gw_cfg_json_parse_remote.h"
#include "gw_cfg_json_parse_http.h"
#include "gw_cfg_json_parse_http_stat.h"
#include "gw_cfg_json_parse_mqtt.h"
#include "gw_cfg_json_parse_lan_auth.h"
#include "gw_cfg_json_parse_auto_update.h"
#include "gw_cfg_json_parse_ntp.h"
#include "gw_cfg_json_parse_filter.h"
#include "gw_cfg_json_parse_scan.h"
#include "gw_cfg_json_parse_scan_filter.h"
#include "gw_cfg_json_parse_fw_update.h"
#include "gw_cfg_json_parse_wifi.h"

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

static void
gw_cfg_json_parse_cjson_ruuvi_cfg(const cJSON* const p_json_root, gw_cfg_ruuvi_t* const p_ruuvi_cfg)
{
    gw_cfg_json_parse_remote(p_json_root, &p_ruuvi_cfg->remote);
    gw_cfg_json_parse_http(p_json_root, &p_ruuvi_cfg->http);
    gw_cfg_json_parse_http_stat(p_json_root, &p_ruuvi_cfg->http_stat);
    gw_cfg_json_parse_mqtt(p_json_root, &p_ruuvi_cfg->mqtt);
    gw_cfg_json_parse_lan_auth(p_json_root, &p_ruuvi_cfg->lan_auth);
    gw_cfg_json_parse_auto_update(p_json_root, &p_ruuvi_cfg->auto_update);
    gw_cfg_json_parse_ntp(p_json_root, &p_ruuvi_cfg->ntp);
    gw_cfg_json_parse_filter(p_json_root, &p_ruuvi_cfg->filter);
    gw_cfg_json_parse_scan(p_json_root, &p_ruuvi_cfg->scan);
    gw_cfg_json_parse_scan_filter(p_json_root, &p_ruuvi_cfg->scan_filter);
    if (!gw_cfg_json_copy_string_val(
            p_json_root,
            "coordinates",
            &p_ruuvi_cfg->coordinates.buf[0],
            sizeof(p_ruuvi_cfg->coordinates.buf)))
    {
        LOG_WARN("Can't find key '%s' in config-json", "coordinates");
    }
    gw_cfg_json_parse_fw_update(p_json_root, &p_ruuvi_cfg->fw_update);
}

void
gw_cfg_json_parse_cjson(
    const cJSON* const          p_json_root,
    const char* const           p_log_title,
    gw_cfg_device_info_t* const p_dev_info,
    gw_cfg_ruuvi_t* const       p_ruuvi_cfg,
    gw_cfg_eth_t* const         p_eth_cfg,
    wifiman_config_ap_t* const  p_wifi_cfg_ap,
    wifiman_config_sta_t* const p_wifi_cfg_sta)
{
    if (NULL != p_log_title)
    {
        LOG_INFO("%s", p_log_title);
    }
    if (NULL != p_dev_info)
    {
        gw_cfg_json_parse_device_info(p_json_root, p_dev_info);
        if (NULL != p_log_title)
        {
            gw_cfg_log_device_info(p_dev_info, NULL);
        }
    }
    if (NULL != p_ruuvi_cfg)
    {
        gw_cfg_json_parse_cjson_ruuvi_cfg(p_json_root, p_ruuvi_cfg);
        if (NULL != p_log_title)
        {
            gw_cfg_log_ruuvi_cfg(p_ruuvi_cfg, NULL);
        }
    }
    if (NULL != p_eth_cfg)
    {
        gw_cfg_json_parse_eth(p_json_root, p_eth_cfg);
        if (NULL != p_log_title)
        {
            gw_cfg_log_eth_cfg(p_eth_cfg, NULL);
        }
    }

    if (NULL != p_wifi_cfg_ap)
    {
        const cJSON* const p_json_wifi_ap_cfg = cJSON_GetObjectItem(p_json_root, "wifi_ap_config");
        if (NULL == p_json_wifi_ap_cfg)
        {
            LOG_WARN("Can't find key '%s' in config-json", "wifi_ap_config");
        }
        else
        {
            gw_cfg_json_parse_wifi_ap_config(p_json_wifi_ap_cfg, &p_wifi_cfg_ap->wifi_config_ap);
            gw_cfg_json_parse_wifi_ap_settings(p_json_wifi_ap_cfg, &p_wifi_cfg_ap->wifi_settings_ap);
        }

        if (NULL != p_log_title)
        {
            gw_cfg_log_wifi_cfg_ap(p_wifi_cfg_ap, NULL);
        }
    }

    if (NULL != p_wifi_cfg_sta)
    {
        const cJSON* const p_json_wifi_sta_cfg = cJSON_GetObjectItem(p_json_root, "wifi_sta_config");
        if (NULL == p_json_wifi_sta_cfg)
        {
            LOG_WARN("Can't find key '%s' in config-json", "wifi_sta_config");
        }
        else
        {
            gw_cfg_json_parse_wifi_sta_config(p_json_wifi_sta_cfg, &p_wifi_cfg_sta->wifi_config_sta);
            gw_cfg_json_parse_wifi_sta_settings(p_json_wifi_sta_cfg, &p_wifi_cfg_sta->wifi_settings_sta);
        }

        if (NULL != p_log_title)
        {
            gw_cfg_log_wifi_cfg_sta(p_wifi_cfg_sta, NULL);
        }
    }
}

void
gw_cfg_json_parse_cjson_ruuvi(
    const cJSON* const    p_json_root,
    const char* const     p_log_title,
    gw_cfg_ruuvi_t* const p_ruuvi_cfg)
{
    gw_cfg_json_parse_cjson(p_json_root, p_log_title, NULL, p_ruuvi_cfg, NULL, NULL, NULL);
}

void
gw_cfg_json_parse_cjson_eth(
    const cJSON* const  p_json_root,
    const char* const   p_log_title,
    gw_cfg_eth_t* const p_eth_cfg)
{
    gw_cfg_json_parse_cjson(p_json_root, p_log_title, NULL, NULL, p_eth_cfg, NULL, NULL);
}

void
gw_cfg_json_parse_cjson_wifi_ap(
    const cJSON* const         p_json_root,
    const char* const          p_log_title,
    gw_cfg_eth_t* const        p_eth_cfg,
    wifiman_config_ap_t* const p_wifi_cfg_ap)
{
    gw_cfg_json_parse_cjson(p_json_root, p_log_title, NULL, NULL, p_eth_cfg, p_wifi_cfg_ap, NULL);
}

void
gw_cfg_json_parse_cjson_wifi_sta(
    const cJSON* const          p_json_root,
    const char* const           p_log_title,
    wifiman_config_sta_t* const p_wifi_cfg_sta)
{
    gw_cfg_json_parse_cjson(p_json_root, p_log_title, NULL, NULL, NULL, NULL, p_wifi_cfg_sta);
}

bool
gw_cfg_json_parse(
    const char* const p_json_name,
    const char* const p_log_title,
    const char* const p_json_str,
    gw_cfg_t* const   p_gw_cfg)
{
    if ('\0' == p_json_str[0])
    {
        LOG_WARN("%s is empty", p_json_name);
        return false;
    }

    cJSON* p_json_root = cJSON_Parse(p_json_str);
    if (NULL == p_json_root)
    {
        LOG_ERR("Failed to parse %s: %s", p_json_name, p_json_str);
        return false;
    }

    gw_cfg_json_parse_cjson(
        p_json_root,
        p_log_title,
        NULL,
        &p_gw_cfg->ruuvi_cfg,
        &p_gw_cfg->eth_cfg,
        &p_gw_cfg->wifi_cfg.ap,
        &p_gw_cfg->wifi_cfg.sta);

    cJSON_Delete(p_json_root);
    return true;
}
