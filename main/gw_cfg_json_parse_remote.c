/**
 * @file gw_cfg_json_parse_remote.h
 * @author TheSomeMan
 * @date 2021-09-29
 * @copyright Ruuvi Innovations Ltd, license BSD-3-Clause.
 */

#include "gw_cfg_json_parse_remote.h"
#include <string.h>
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
gw_cfg_json_parse_remote_auth_type_basic(const cJSON* const p_json_root, ruuvi_gw_cfg_remote_t* const p_gw_cfg_remote)
{
    if (!gw_cfg_json_copy_string_val(
            p_json_root,
            "remote_cfg_auth_basic_user",
            &p_gw_cfg_remote->auth.auth_basic.user.buf[0],
            sizeof(p_gw_cfg_remote->auth.auth_basic.user.buf)))
    {
        LOG_WARN("Can't find key '%s' in config-json", "remote_cfg_auth_basic_user");
    }
    if (!gw_cfg_json_copy_string_val(
            p_json_root,
            "remote_cfg_auth_basic_pass",
            &p_gw_cfg_remote->auth.auth_basic.password.buf[0],
            sizeof(p_gw_cfg_remote->auth.auth_basic.password.buf)))
    {
        LOG_INFO(
            "Can't find key '%s' in config-json, leave the previous value unchanged",
            "remote_cfg_auth_basic_pass");
    }
}

static void
gw_cfg_json_parse_remote_auth_type_bearer(const cJSON* const p_json_root, ruuvi_gw_cfg_remote_t* const p_gw_cfg_remote)
{
    if (!gw_cfg_json_copy_string_val(
            p_json_root,
            "remote_cfg_auth_bearer_token",
            &p_gw_cfg_remote->auth.auth_bearer.token.buf[0],
            sizeof(p_gw_cfg_remote->auth.auth_bearer.token.buf)))
    {
        LOG_INFO(
            "Can't find key '%s' in config-json, leave the previous value unchanged",
            "remote_cfg_auth_bearer_token");
    }
}

void
gw_cfg_json_parse_remote(const cJSON* const p_json_root, ruuvi_gw_cfg_remote_t* const p_gw_cfg_remote)
{
    if (!gw_cfg_json_get_bool_val(p_json_root, "remote_cfg_use", &p_gw_cfg_remote->use_remote_cfg))
    {
        LOG_WARN("Can't find key '%s' in config-json", "remote_cfg_use");
    }
    if (!gw_cfg_json_copy_string_val(
            p_json_root,
            "remote_cfg_url",
            &p_gw_cfg_remote->url.buf[0],
            sizeof(p_gw_cfg_remote->url.buf)))
    {
        LOG_WARN("Can't find key '%s' in config-json", "remote_cfg_url");
    }

    char auth_type_str[GW_CFG_HTTP_AUTH_TYPE_STR_SIZE];
    if (!gw_cfg_json_copy_string_val(p_json_root, "remote_cfg_auth_type", &auth_type_str[0], sizeof(auth_type_str)))
    {
        LOG_WARN("Can't find key '%s' in config-json", "remote_cfg_auth_type");
    }
    else
    {
        gw_cfg_http_auth_type_e auth_type = GW_CFG_HTTP_AUTH_TYPE_NONE;
        if (0 == strcmp(GW_CFG_HTTP_AUTH_TYPE_STR_NONE, auth_type_str))
        {
            auth_type = GW_CFG_HTTP_AUTH_TYPE_NONE;
        }
        else if (0 == strcmp(GW_CFG_HTTP_AUTH_TYPE_STR_NO, auth_type_str))
        {
            auth_type = GW_CFG_HTTP_AUTH_TYPE_NONE;
        }
        else if (0 == strcmp(GW_CFG_HTTP_AUTH_TYPE_STR_BASIC, auth_type_str))
        {
            auth_type = GW_CFG_HTTP_AUTH_TYPE_BASIC;
        }
        else if (0 == strcmp(GW_CFG_HTTP_AUTH_TYPE_STR_BEARER, auth_type_str))
        {
            auth_type = GW_CFG_HTTP_AUTH_TYPE_BEARER;
        }
        else
        {
            LOG_WARN("Unknown remote_cfg_auth_type='%s', use NONE", auth_type_str);
            auth_type = GW_CFG_HTTP_AUTH_TYPE_NONE;
        }
        if (p_gw_cfg_remote->auth_type != auth_type)
        {
            memset(&p_gw_cfg_remote->auth, 0, sizeof(p_gw_cfg_remote->auth));
            p_gw_cfg_remote->auth_type = auth_type;
            LOG_INFO("Key 'remote_cfg_auth_type' was changed, clear saved auth info");
        }
        switch (auth_type)
        {
            case GW_CFG_HTTP_AUTH_TYPE_NONE:
                break;

            case GW_CFG_HTTP_AUTH_TYPE_BASIC:
                gw_cfg_json_parse_remote_auth_type_basic(p_json_root, p_gw_cfg_remote);
                break;

            case GW_CFG_HTTP_AUTH_TYPE_BEARER:
                gw_cfg_json_parse_remote_auth_type_bearer(p_json_root, p_gw_cfg_remote);
                break;

            case GW_CFG_HTTP_AUTH_TYPE_TOKEN:
                LOG_ERR("Unsupported auth_type=token for remote_cfg");
                break;
        }
    }
    if (!gw_cfg_json_get_bool_val(p_json_root, "remote_cfg_use_ssl_client_cert", &p_gw_cfg_remote->use_ssl_client_cert))
    {
        LOG_WARN("Can't find key '%s' in config-json", "remote_cfg_use_ssl_client_cert");
    }
    if (!gw_cfg_json_get_bool_val(p_json_root, "remote_cfg_use_ssl_server_cert", &p_gw_cfg_remote->use_ssl_server_cert))
    {
        LOG_WARN("Can't find key '%s' in config-json", "remote_cfg_use_ssl_server_cert");
    }
    if (!gw_cfg_json_get_uint16_val(
            p_json_root,
            "remote_cfg_refresh_interval_minutes",
            &p_gw_cfg_remote->refresh_interval_minutes))
    {
        LOG_WARN("Can't find key '%s' in config-json", "remote_cfg_refresh_interval_minutes");
    }
}
