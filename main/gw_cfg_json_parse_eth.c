/**
 * @file gw_cfg_json_parse_eth.c
 * @author TheSomeMan
 * @date 2021-09-29
 * @copyright Ruuvi Innovations Ltd, license BSD-3-Clause.
 */

#include "gw_cfg_json_parse_eth.h"
#include "gw_cfg_json_parse_internal.h"

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
gw_cfg_json_parse_ip(
    const cJSON* const                p_root,
    const char* const                 p_attr_name,
    ruuvi_gw_cfg_ip_addr_str_t* const p_ip_addr)
{
    if (!gw_cfg_json_copy_string_val(p_root, p_attr_name, &p_ip_addr->buf[0], sizeof(p_ip_addr->buf)))
    {
        LOG_WARN("Can't find key '%s' in config-json", p_attr_name);
    }
}

static void
gw_cfg_json_parse_eth_dhcp(const cJSON* const p_root, gw_cfg_eth_t* const p_gw_cfg_eth)
{
    gw_cfg_json_parse_ip(p_root, "eth_static_ip", &p_gw_cfg_eth->eth_static_ip);
    gw_cfg_json_parse_ip(p_root, "eth_netmask", &p_gw_cfg_eth->eth_netmask);
    gw_cfg_json_parse_ip(p_root, "eth_gw", &p_gw_cfg_eth->eth_gw);
    gw_cfg_json_parse_ip(p_root, "eth_dns1", &p_gw_cfg_eth->eth_dns1);
    gw_cfg_json_parse_ip(p_root, "eth_dns2", &p_gw_cfg_eth->eth_dns2);
}

void
gw_cfg_json_parse_eth(const cJSON* const p_json_root, gw_cfg_eth_t* const p_gw_cfg_eth)
{
    if (!gw_cfg_json_get_bool_val(p_json_root, "use_eth", &p_gw_cfg_eth->use_eth))
    {
        LOG_WARN("Can't find key '%s' in config-json", "use_eth");
    }
    if (p_gw_cfg_eth->use_eth)
    {
        if (!gw_cfg_json_get_bool_val(p_json_root, "eth_dhcp", &p_gw_cfg_eth->eth_dhcp))
        {
            LOG_WARN("Can't find key '%s' in config-json", "eth_dhcp");
        }
        if (!p_gw_cfg_eth->eth_dhcp)
        {
            gw_cfg_json_parse_eth_dhcp(p_json_root, p_gw_cfg_eth);
        }
    }
}
