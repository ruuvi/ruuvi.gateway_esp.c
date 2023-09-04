/**
 * @file gw_cfg_json_parse_ntp.c
 * @author TheSomeMan
 * @date 2021-09-29
 * @copyright Ruuvi Innovations Ltd, license BSD-3-Clause.
 */

#include "gw_cfg_json_parse_ntp.h"
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
gw_cfg_json_parse_ntp_server(
    const cJSON* const                        p_cjson,
    const char* const                         p_srv_name,
    ruuvi_gw_cfg_ntp_server_addr_str_t* const p_addr)
{
    if (!gw_cfg_json_copy_string_val(p_cjson, p_srv_name, &p_addr->buf[0], sizeof(p_addr->buf)))
    {
        LOG_WARN("Can't find key '%s' in config-json", p_srv_name);
    }
}

static void
gw_cfg_json_parse_ntp_servers(const cJSON* const p_cjson, ruuvi_gw_cfg_ntp_t* const p_ntp)
{
    gw_cfg_json_parse_ntp_server(p_cjson, "ntp_server1", &p_ntp->ntp_server1);
    gw_cfg_json_parse_ntp_server(p_cjson, "ntp_server2", &p_ntp->ntp_server2);
    gw_cfg_json_parse_ntp_server(p_cjson, "ntp_server3", &p_ntp->ntp_server3);
    gw_cfg_json_parse_ntp_server(p_cjson, "ntp_server4", &p_ntp->ntp_server4);
}

void
gw_cfg_json_parse_ntp(const cJSON* const p_cjson, ruuvi_gw_cfg_ntp_t* const p_ntp)
{
    if (!gw_cfg_json_get_bool_val(p_cjson, "ntp_use", &p_ntp->ntp_use))
    {
        LOG_WARN("Can't find key '%s' in config-json", "ntp_use");
    }
    if (p_ntp->ntp_use)
    {
        if (!gw_cfg_json_get_bool_val(p_cjson, "ntp_use_dhcp", &p_ntp->ntp_use_dhcp))
        {
            LOG_WARN("Can't find key '%s' in config-json", "ntp_use_dhcp");
        }
        if (!p_ntp->ntp_use_dhcp)
        {
            gw_cfg_json_parse_ntp_servers(p_cjson, p_ntp);
        }
    }
    else
    {
        p_ntp->ntp_use_dhcp = false;
    }
}
