/**
 * @file gw_cfg_json_generate_remote.c
 * @author TheSomeMan
 * @date 2026-05-26
 * @copyright Ruuvi Innovations Ltd, license BSD-3-Clause.
 */

#include "gw_cfg_json_generate_remote.h"
#include "gw_cfg_json_generate_internal.h"

static bool
gw_cfg_json_add_items_remote_auth_basic(
    cJSON* const                       p_json_root,
    const ruuvi_gw_cfg_remote_t* const p_cfg_remote,
    const bool                         flag_hide_passwords)
{
    if (!gw_cfg_json_add_string(p_json_root, "remote_cfg_auth_type", GW_CFG_HTTP_AUTH_TYPE_STR_BASIC))
    {
        return false;
    }
    if (!gw_cfg_json_add_string(p_json_root, "remote_cfg_auth_basic_user", p_cfg_remote->auth.auth_basic.user.buf))
    {
        return false;
    }
    if ((!flag_hide_passwords)
        && (!gw_cfg_json_add_string(
            p_json_root,
            "remote_cfg_auth_basic_pass",
            p_cfg_remote->auth.auth_basic.password.buf)))
    {
        return false;
    }
    return true;
}

static bool
gw_cfg_json_add_items_remote_auth_bearer(
    cJSON* const                       p_json_root,
    const ruuvi_gw_cfg_remote_t* const p_cfg_remote,
    const bool                         flag_hide_passwords)
{
    if (!gw_cfg_json_add_string(p_json_root, "remote_cfg_auth_type", GW_CFG_HTTP_AUTH_TYPE_STR_BEARER))
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
gw_cfg_json_add_items_remote_auth_token(
    cJSON* const                       p_json_root,
    const ruuvi_gw_cfg_remote_t* const p_cfg_remote,
    const bool                         flag_hide_passwords)
{
    if (!gw_cfg_json_add_string(p_json_root, "remote_cfg_auth_type", GW_CFG_HTTP_AUTH_TYPE_STR_TOKEN))
    {
        return false;
    }
    if ((!flag_hide_passwords)
        && (!gw_cfg_json_add_string(p_json_root, "remote_cfg_auth_token", p_cfg_remote->auth.auth_token.token.buf)))
    {
        return false;
    }
    return true;
}

static bool
gw_cfg_json_add_items_remote_auth_apikey(
    cJSON* const                       p_json_root,
    const ruuvi_gw_cfg_remote_t* const p_cfg_remote,
    const bool                         flag_hide_passwords)
{
    if (!gw_cfg_json_add_string(p_json_root, "remote_cfg_auth_type", GW_CFG_HTTP_AUTH_TYPE_STR_APIKEY))
    {
        return false;
    }
    if ((!flag_hide_passwords)
        && (!gw_cfg_json_add_string(p_json_root, "remote_cfg_auth_apikey", p_cfg_remote->auth.auth_apikey.api_key.buf)))
    {
        return false;
    }
    return true;
}

bool
gw_cfg_json_add_items_remote(
    cJSON* const                       p_json_root,
    const ruuvi_gw_cfg_remote_t* const p_cfg_remote,
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
        case GW_CFG_HTTP_AUTH_TYPE_NONE:
            if (!gw_cfg_json_add_string(p_json_root, "remote_cfg_auth_type", GW_CFG_HTTP_AUTH_TYPE_STR_NONE))
            {
                return false;
            }
            break;
        case GW_CFG_HTTP_AUTH_TYPE_BASIC:
            if (!gw_cfg_json_add_items_remote_auth_basic(p_json_root, p_cfg_remote, flag_hide_passwords))
            {
                return false;
            }
            break;
        case GW_CFG_HTTP_AUTH_TYPE_BEARER:
            if (!gw_cfg_json_add_items_remote_auth_bearer(p_json_root, p_cfg_remote, flag_hide_passwords))
            {
                return false;
            }
            break;
        case GW_CFG_HTTP_AUTH_TYPE_TOKEN:
            if (!gw_cfg_json_add_items_remote_auth_token(p_json_root, p_cfg_remote, flag_hide_passwords))
            {
                return false;
            }
            break;
        case GW_CFG_HTTP_AUTH_TYPE_APIKEY:
            if (!gw_cfg_json_add_items_remote_auth_apikey(p_json_root, p_cfg_remote, flag_hide_passwords))
            {
                return false;
            }
            break;
    }
    if (!gw_cfg_json_add_bool(p_json_root, "remote_cfg_use_ssl_client_cert", p_cfg_remote->use_ssl_client_cert))
    {
        return false;
    }
    if (!gw_cfg_json_add_bool(p_json_root, "remote_cfg_use_ssl_server_cert", p_cfg_remote->use_ssl_server_cert))
    {
        return false;
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
