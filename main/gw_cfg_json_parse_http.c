/**
 * @file gw_cfg_json_parse_http.c
 * @author TheSomeMan
 * @date 2021-09-29
 * @copyright Ruuvi Innovations Ltd, license BSD-3-Clause.
 */

#include "gw_cfg_json_parse_http.h"
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

static gw_cfg_http_data_format_e
gw_cfg_json_parse_http_data_format(const cJSON* const p_json_root)
{
    char data_format_str[GW_CFG_HTTP_DATA_FORMAT_STR_SIZE];
    if (!gw_cfg_json_copy_string_val(p_json_root, "http_data_format", &data_format_str[0], sizeof(data_format_str)))
    {
        LOG_WARN("Can't find key '%s' in config-json", "http_data_format");
        return GW_CFG_HTTP_DATA_FORMAT_RUUVI;
    }
    if (0 == strcmp(GW_CFG_HTTP_DATA_FORMAT_STR_RUUVI, data_format_str))
    {
        return GW_CFG_HTTP_DATA_FORMAT_RUUVI;
    }
    if (0 == strcmp(GW_CFG_HTTP_DATA_FORMAT_STR_RAW_AND_DECODED, data_format_str))
    {
        return GW_CFG_HTTP_DATA_FORMAT_RUUVI_RAW_AND_DECODED;
    }
    if (0 == strcmp(GW_CFG_HTTP_DATA_FORMAT_STR_DECODED, data_format_str))
    {
        return GW_CFG_HTTP_DATA_FORMAT_RUUVI_DECODED;
    }
    LOG_WARN("Unknown http_data_format='%s', use 'ruuvi'", data_format_str);
    return GW_CFG_HTTP_DATA_FORMAT_RUUVI;
}

static gw_cfg_http_auth_type_e
gw_cfg_json_parse_http_auth_type(const cJSON* const p_json_root)
{
    char auth_type_str[GW_CFG_HTTP_AUTH_TYPE_STR_SIZE];
    if (!gw_cfg_json_copy_string_val(p_json_root, "http_auth", &auth_type_str[0], sizeof(auth_type_str)))
    {
        LOG_WARN("Can't find key '%s' in config-json", "http_auth");
        return GW_CFG_HTTP_AUTH_TYPE_NONE;
    }
    if (0 == strcmp(GW_CFG_HTTP_AUTH_TYPE_STR_NONE, auth_type_str))
    {
        return GW_CFG_HTTP_AUTH_TYPE_NONE;
    }
    if (0 == strcmp(GW_CFG_HTTP_AUTH_TYPE_STR_BASIC, auth_type_str))
    {
        return GW_CFG_HTTP_AUTH_TYPE_BASIC;
    }
    if (0 == strcmp(GW_CFG_HTTP_AUTH_TYPE_STR_BEARER, auth_type_str))
    {
        return GW_CFG_HTTP_AUTH_TYPE_BEARER;
    }
    if (0 == strcmp(GW_CFG_HTTP_AUTH_TYPE_STR_TOKEN, auth_type_str))
    {
        return GW_CFG_HTTP_AUTH_TYPE_TOKEN;
    }
    if (0 == strcmp(GW_CFG_HTTP_AUTH_TYPE_STR_APIKEY, auth_type_str))
    {
        return GW_CFG_HTTP_AUTH_TYPE_APIKEY;
    }
    LOG_WARN("Unknown http_auth='%s', use 'ruuvi'", auth_type_str);
    return GW_CFG_HTTP_AUTH_TYPE_NONE;
}

static void
gw_cfg_json_parse_http_auth_basic(const cJSON* const p_json_root, ruuvi_gw_cfg_http_t* const p_gw_cfg_http)
{
    if (!gw_cfg_json_copy_string_val(
            p_json_root,
            "http_user",
            &p_gw_cfg_http->auth.auth_basic.user.buf[0],
            sizeof(p_gw_cfg_http->auth.auth_basic.user.buf)))
    {
        LOG_WARN("Can't find key '%s' in config-json", "http_user");
    }
    if (!gw_cfg_json_copy_string_val(
            p_json_root,
            "http_pass",
            &p_gw_cfg_http->auth.auth_basic.password.buf[0],
            sizeof(p_gw_cfg_http->auth.auth_basic.password.buf)))
    {
        LOG_INFO("Can't find key '%s' in config-json, leave the previous value unchanged", "http_pass");
    }
}

static void
gw_cfg_json_parse_http_auth_bearer(const cJSON* const p_json_root, ruuvi_gw_cfg_http_t* const p_gw_cfg_http)
{
    if (!gw_cfg_json_copy_string_val(
            p_json_root,
            "http_bearer_token",
            &p_gw_cfg_http->auth.auth_bearer.token.buf[0],
            sizeof(p_gw_cfg_http->auth.auth_bearer.token.buf)))
    {
        LOG_WARN("Can't find key '%s' in config-json", "http_bearer_token");
    }
}

static void
gw_cfg_json_parse_http_auth_token(const cJSON* const p_json_root, ruuvi_gw_cfg_http_t* const p_gw_cfg_http)
{
    if (!gw_cfg_json_copy_string_val(
            p_json_root,
            "http_api_key",
            &p_gw_cfg_http->auth.auth_token.token.buf[0],
            sizeof(p_gw_cfg_http->auth.auth_token.token.buf)))
    {
        LOG_WARN("Can't find key '%s' in config-json", "http_api_key");
    }
}

static void
gw_cfg_json_parse_http_auth_apikey(const cJSON* const p_json_root, ruuvi_gw_cfg_http_t* const p_gw_cfg_http)
{
    if (!gw_cfg_json_copy_string_val(
            p_json_root,
            "http_api_key",
            &p_gw_cfg_http->auth.auth_apikey.api_key.buf[0],
            sizeof(p_gw_cfg_http->auth.auth_apikey.api_key.buf)))
    {
        LOG_WARN("Can't find key '%s' in config-json", "http_api_key");
    }
}

static void
gw_cfg_json_parse_http_ssl_certs(const cJSON* const p_json_root, ruuvi_gw_cfg_http_t* const p_gw_cfg_http)
{
    if (!gw_cfg_json_get_bool_val(p_json_root, "http_use_ssl_client_cert", &p_gw_cfg_http->http_use_ssl_client_cert))
    {
        LOG_WARN("Can't find key '%s' in config-json", "http_use_ssl_client_cert");
    }
    if (!gw_cfg_json_get_bool_val(p_json_root, "http_use_ssl_server_cert", &p_gw_cfg_http->http_use_ssl_server_cert))
    {
        LOG_WARN("Can't find key '%s' in config-json", "http_use_ssl_server_cert");
    }
}

void
gw_cfg_json_parse_http(const cJSON* const p_json_root, ruuvi_gw_cfg_http_t* const p_gw_cfg_http)
{
    if (!gw_cfg_json_get_bool_val(p_json_root, "use_http_ruuvi", &p_gw_cfg_http->use_http_ruuvi))
    {
        LOG_WARN("Can't find key '%s' in config-json", "use_http_ruuvi");
    }
    if (!gw_cfg_json_get_bool_val(p_json_root, "use_http", &p_gw_cfg_http->use_http))
    {
        LOG_WARN("Can't find key '%s' in config-json", "use_http");
    }

    if (p_gw_cfg_http->use_http)
    {
        p_gw_cfg_http->data_format = gw_cfg_json_parse_http_data_format(p_json_root);
        p_gw_cfg_http->auth_type   = gw_cfg_json_parse_http_auth_type(p_json_root);

        if (!gw_cfg_json_copy_string_val(
                p_json_root,
                "http_url",
                &p_gw_cfg_http->http_url.buf[0],
                sizeof(p_gw_cfg_http->http_url.buf)))
        {
            LOG_WARN("Can't find key '%s' in config-json", "http_url");
        }
        switch (p_gw_cfg_http->auth_type)
        {
            case GW_CFG_HTTP_AUTH_TYPE_NONE:
                break;
            case GW_CFG_HTTP_AUTH_TYPE_BASIC:
                gw_cfg_json_parse_http_auth_basic(p_json_root, p_gw_cfg_http);
                break;
            case GW_CFG_HTTP_AUTH_TYPE_BEARER:
                gw_cfg_json_parse_http_auth_bearer(p_json_root, p_gw_cfg_http);
                break;
            case GW_CFG_HTTP_AUTH_TYPE_TOKEN:
                gw_cfg_json_parse_http_auth_token(p_json_root, p_gw_cfg_http);
                break;
            case GW_CFG_HTTP_AUTH_TYPE_APIKEY:
                gw_cfg_json_parse_http_auth_apikey(p_json_root, p_gw_cfg_http);
                break;
        }
        if ((GW_CFG_HTTP_DATA_FORMAT_RUUVI == p_gw_cfg_http->data_format)
            && (GW_CFG_HTTP_AUTH_TYPE_NONE == p_gw_cfg_http->auth_type)
            && (0 == strcmp(RUUVI_GATEWAY_HTTP_DEFAULT_URL, p_gw_cfg_http->http_url.buf)))
        {
            // 'use_http_ruuvi' was added in v1.14, so we need to patch configuration
            // to ensure compatibility between configuration versions when upgrading firmware to a new version
            // or rolling back to an old one
            p_gw_cfg_http->use_http_ruuvi = true;
            p_gw_cfg_http->use_http       = false;
        }
        if (p_gw_cfg_http->use_http)
        {
            if (!gw_cfg_json_get_uint32_val(p_json_root, "http_period", &p_gw_cfg_http->http_period))
            {
                LOG_WARN("Can't find key '%s' in config-json", "http_period");
            }
            gw_cfg_json_parse_http_ssl_certs(p_json_root, p_gw_cfg_http);
        }
    }
}
