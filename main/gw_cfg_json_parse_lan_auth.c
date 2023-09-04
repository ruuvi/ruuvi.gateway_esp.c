/**
 * @file gw_cfg_json_parse_lan_auth.c
 * @author TheSomeMan
 * @date 2021-09-29
 * @copyright Ruuvi Innovations Ltd, license BSD-3-Clause.
 */

#include "gw_cfg_json_parse_lan_auth.h"
#include <string.h>
#include "gw_cfg_json_parse_internal.h"
#include "gw_cfg_default.h"

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
gw_cfg_json_parse_lan_auth_user_password(const cJSON* const p_cjson, ruuvi_gw_cfg_lan_auth_t* const p_cfg)
{
    if (!gw_cfg_json_copy_string_val(
            p_cjson,
            "lan_auth_user",
            &p_cfg->lan_auth_user.buf[0],
            sizeof(p_cfg->lan_auth_user.buf)))
    {
        LOG_WARN("Can't find key '%s' in config-json", "lan_auth_user");
    }
    if (!gw_cfg_json_copy_string_val(
            p_cjson,
            "lan_auth_pass",
            &p_cfg->lan_auth_pass.buf[0],
            sizeof(p_cfg->lan_auth_pass.buf)))
    {
        LOG_INFO("Can't find key '%s' in config-json, leave the previous value unchanged", "lan_auth_pass");
    }
}

void
gw_cfg_json_parse_lan_auth(const cJSON* const p_cjson, ruuvi_gw_cfg_lan_auth_t* const p_cfg)
{
    http_server_auth_type_str_t lan_auth_type_str = { 0 };
    if (!gw_cfg_json_copy_string_val(p_cjson, "lan_auth_type", lan_auth_type_str.buf, sizeof(lan_auth_type_str.buf)))
    {
        LOG_INFO("Can't find key '%s' in config-json, leave the previous value unchanged", "lan_auth_type");
    }
    else
    {
        const ruuvi_gw_cfg_lan_auth_t* const p_default_lan_auth = gw_cfg_default_get_lan_auth();
        p_cfg->lan_auth_type                                    = http_server_auth_type_from_str(lan_auth_type_str.buf);
        switch (p_cfg->lan_auth_type)
        {
            case HTTP_SERVER_AUTH_TYPE_BASIC:
            case HTTP_SERVER_AUTH_TYPE_DIGEST:
            case HTTP_SERVER_AUTH_TYPE_RUUVI:
                gw_cfg_json_parse_lan_auth_user_password(p_cjson, p_cfg);
                break;

            case HTTP_SERVER_AUTH_TYPE_DEFAULT:
                p_cfg->lan_auth_user = p_default_lan_auth->lan_auth_user;
                p_cfg->lan_auth_pass = p_default_lan_auth->lan_auth_pass;
                break;

            case HTTP_SERVER_AUTH_TYPE_ALLOW:
            case HTTP_SERVER_AUTH_TYPE_DENY:
            case HTTP_SERVER_AUTH_TYPE_BEARER:
                p_cfg->lan_auth_user.buf[0] = '\0';
                p_cfg->lan_auth_pass.buf[0] = '\0';
                break;
        }
        if ((HTTP_SERVER_AUTH_TYPE_RUUVI == p_cfg->lan_auth_type)
            && (0 == strcmp(p_default_lan_auth->lan_auth_user.buf, p_cfg->lan_auth_user.buf))
            && (0 == strcmp(p_default_lan_auth->lan_auth_pass.buf, p_cfg->lan_auth_pass.buf)))
        {
            p_cfg->lan_auth_type = HTTP_SERVER_AUTH_TYPE_DEFAULT;
        }
    }

    if (!gw_cfg_json_copy_string_val(
            p_cjson,
            "lan_auth_api_key",
            &p_cfg->lan_auth_api_key.buf[0],
            sizeof(p_cfg->lan_auth_api_key)))
    {
        LOG_INFO("Can't find key '%s' in config-json, leave the previous value unchanged", "lan_auth_api_key");
    }

    if (!gw_cfg_json_copy_string_val(
            p_cjson,
            "lan_auth_api_key_rw",
            &p_cfg->lan_auth_api_key_rw.buf[0],
            sizeof(p_cfg->lan_auth_api_key_rw)))
    {
        LOG_INFO("Can't find key '%s' in config-json, leave the previous value unchanged", "lan_auth_api_key_rw");
    }
}
