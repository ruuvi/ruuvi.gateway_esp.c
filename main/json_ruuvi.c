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
#include "gw_cfg_json.h"

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
json_ruuvi_parse_main_cfg(const cJSON *p_json_root, ruuvi_gateway_config_t *const p_gw_cfg)
{
    if (!json_ruuvi_set_default_values_for_main_config(p_gw_cfg))
    {
        return false;
    }
    gw_cfg_json_parse_cjson(p_json_root, true, p_gw_cfg, NULL);
    return true;
}

JSON_RUUVI_STATIC
bool
json_ruuvi_parse(const cJSON *p_json_root, ruuvi_gateway_config_t *const p_gw_cfg, bool *const p_flag_network_cfg)
{
    bool use_eth = false;
    LOG_DBG("Got SETTINGS:");
    if (json_wrap_get_bool_val(p_json_root, "use_eth", &use_eth))
    {
        LOG_INFO("Got SETTINGS:");
        *p_flag_network_cfg = true;
        LOG_INFO("%s: %d", "use_eth", use_eth);

        p_gw_cfg->eth = gw_cfg_default_get_eth();
        if (use_eth)
        {
            gw_cfg_json_parse_eth(p_json_root, &p_gw_cfg->eth);
            LOG_INFO("%s: %d", "eth_dhcp", p_gw_cfg->eth.eth_dhcp);
            if (!p_gw_cfg->eth.eth_dhcp)
            {
                LOG_INFO("%s: %s", "eth_static_ip", p_gw_cfg->eth.eth_static_ip.buf);
                LOG_INFO("%s: %s", "eth_netmask", p_gw_cfg->eth.eth_netmask.buf);
                LOG_INFO("%s: %s", "eth_gw", p_gw_cfg->eth.eth_gw.buf);
                LOG_INFO("%s: %s", "eth_dns1", p_gw_cfg->eth.eth_dns1.buf);
                LOG_INFO("%s: %s", "eth_dns2", p_gw_cfg->eth.eth_dns2.buf);
            }
        }
        return true;
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
