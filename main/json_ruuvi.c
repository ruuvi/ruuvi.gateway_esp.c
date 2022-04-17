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
#include "gw_cfg_log.h"

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

bool
json_ruuvi_parse_http_body(const char *p_body, gw_cfg_t *const p_gw_cfg, bool *const p_flag_network_cfg)
{
    cJSON *p_json_root = cJSON_Parse(p_body);
    if (NULL == p_json_root)
    {
        LOG_ERR("Failed to parse json or no memory");
        return false;
    }

    bool use_eth        = false;
    *p_flag_network_cfg = json_wrap_get_bool_val(p_json_root, "use_eth", &use_eth);
    if (*p_flag_network_cfg)
    {
        p_gw_cfg->eth_cfg = gw_cfg_default_get_eth();
        gw_cfg_json_parse_cjson_eth(p_json_root, "Gateway SETTINGS (via HTTP)", &p_gw_cfg->eth_cfg);
    }
    else
    {
        gw_cfg_json_parse_cjson_ruuvi(p_json_root, "Gateway SETTINGS (via HTTP)", &p_gw_cfg->ruuvi_cfg);
    }

    cJSON_Delete(p_json_root);
    return true;
}
