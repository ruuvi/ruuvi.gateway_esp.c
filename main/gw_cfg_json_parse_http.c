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

static void
gw_cfg_json_log_missing_key(const char* const p_key, const bool flag_warn_if_missing)
{
    if (flag_warn_if_missing)
    {
        LOG_WARN("Can't find key '%s' in config-json", p_key);
    }
    else
    {
        LOG_DBG("Can't find key '%s' in config-json", p_key);
    }
}

static void
gw_cfg_json_log_missing_key_with_default(
    const char* const  p_key,
    const printf_int_t default_value,
    const bool         flag_warn_if_missing)
{
    if (flag_warn_if_missing)
    {
        LOG_WARN("Can't find key '%s' in config-json, use default value %d", p_key, default_value);
    }
    else
    {
        LOG_DBG("Can't find key '%s' in config-json, use default value %d", p_key, default_value);
    }
}

static void
gw_cfg_json_get_bool_or_log(
    const cJSON* const p_json_root,
    const char* const  p_key,
    bool* const        p_val,
    const bool         flag_warn_if_missing)
{
    if (!gw_cfg_json_get_bool_val(p_json_root, p_key, p_val))
    {
        gw_cfg_json_log_missing_key(p_key, flag_warn_if_missing);
    }
}

static void
gw_cfg_json_get_uint32_or_log(
    const cJSON* const p_json_root,
    const char* const  p_key,
    uint32_t* const    p_val,
    const bool         flag_warn_if_missing)
{
    if (!gw_cfg_json_get_uint32_val(p_json_root, p_key, p_val))
    {
        gw_cfg_json_log_missing_key(p_key, flag_warn_if_missing);
    }
}

static void
gw_cfg_json_copy_string_or_log(
    const cJSON* const p_json_root,
    const char* const  p_key,
    char* const        p_buf,
    const size_t       buf_size,
    const bool         flag_warn_if_missing)
{
    if (!gw_cfg_json_copy_string_val(p_json_root, p_key, p_buf, buf_size))
    {
        gw_cfg_json_log_missing_key(p_key, flag_warn_if_missing);
    }
}

static void
gw_cfg_json_parse_http_data_format(
    const cJSON* const               p_json_root,
    const bool                       flag_warn_if_missing,
    gw_cfg_http_data_format_e* const p_data_format)
{
    char data_format_str[GW_CFG_HTTP_DATA_FORMAT_STR_SIZE];
    if (!gw_cfg_json_copy_string_val(p_json_root, "http_data_format", &data_format_str[0], sizeof(data_format_str)))
    {
        gw_cfg_json_log_missing_key_with_default(
            "http_data_format",
            (printf_int_t)*p_data_format,
            flag_warn_if_missing);
        return;
    }
    if (0 == strcmp(GW_CFG_HTTP_DATA_FORMAT_STR_RUUVI, data_format_str))
    {
        *p_data_format = GW_CFG_HTTP_DATA_FORMAT_RUUVI;
        return;
    }
    if (0 == strcmp(GW_CFG_HTTP_DATA_FORMAT_STR_RAW_AND_DECODED, data_format_str))
    {
        *p_data_format = GW_CFG_HTTP_DATA_FORMAT_RUUVI_RAW_AND_DECODED;
        return;
    }
    if (0 == strcmp(GW_CFG_HTTP_DATA_FORMAT_STR_DECODED, data_format_str))
    {
        *p_data_format = GW_CFG_HTTP_DATA_FORMAT_RUUVI_DECODED;
        return;
    }
    LOG_WARN("Unknown http_data_format='%s', use 'ruuvi'", data_format_str);
    *p_data_format = GW_CFG_HTTP_DATA_FORMAT_RUUVI;
}

static void
gw_cfg_json_parse_http_auth_type(
    const cJSON* const             p_json_root,
    const bool                     flag_warn_if_missing,
    gw_cfg_http_auth_type_e* const p_auth_type)
{
    char auth_type_str[GW_CFG_HTTP_AUTH_TYPE_STR_SIZE];
    if (!gw_cfg_json_copy_string_val(p_json_root, "http_auth", &auth_type_str[0], sizeof(auth_type_str)))
    {
        gw_cfg_json_log_missing_key_with_default("http_auth", (printf_int_t)*p_auth_type, flag_warn_if_missing);
        return;
    }
    if (0 == strcmp(GW_CFG_HTTP_AUTH_TYPE_STR_NONE, auth_type_str))
    {
        *p_auth_type = GW_CFG_HTTP_AUTH_TYPE_NONE;
        return;
    }
    if (0 == strcmp(GW_CFG_HTTP_AUTH_TYPE_STR_BASIC, auth_type_str))
    {
        *p_auth_type = GW_CFG_HTTP_AUTH_TYPE_BASIC;
        return;
    }
    if (0 == strcmp(GW_CFG_HTTP_AUTH_TYPE_STR_BEARER, auth_type_str))
    {
        *p_auth_type = GW_CFG_HTTP_AUTH_TYPE_BEARER;
        return;
    }
    if (0 == strcmp(GW_CFG_HTTP_AUTH_TYPE_STR_TOKEN, auth_type_str))
    {
        *p_auth_type = GW_CFG_HTTP_AUTH_TYPE_TOKEN;
        return;
    }
    if (0 == strcmp(GW_CFG_HTTP_AUTH_TYPE_STR_APIKEY, auth_type_str))
    {
        *p_auth_type = GW_CFG_HTTP_AUTH_TYPE_APIKEY;
        return;
    }
    LOG_WARN("Unknown http_auth='%s', use 'none'", auth_type_str);
    *p_auth_type = GW_CFG_HTTP_AUTH_TYPE_NONE;
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
gw_cfg_json_parse_http_ssl_certs(
    const cJSON* const         p_json_root,
    ruuvi_gw_cfg_http_t* const p_gw_cfg_http,
    const bool                 flag_warn_if_missing)
{
    gw_cfg_json_get_bool_or_log(
        p_json_root,
        "http_use_ssl_client_cert",
        &p_gw_cfg_http->http_use_ssl_client_cert,
        flag_warn_if_missing);
    gw_cfg_json_get_bool_or_log(
        p_json_root,
        "http_use_ssl_server_cert",
        &p_gw_cfg_http->http_use_ssl_server_cert,
        flag_warn_if_missing);
}

static void
gw_cfg_json_parse_http_auth(const cJSON* const p_json_root, ruuvi_gw_cfg_http_t* const p_gw_cfg_http)
{
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
}

static void
gw_cfg_json_patch_http_legacy_pre_v1_14(ruuvi_gw_cfg_http_t* const p_gw_cfg_http)
{
    // 'use_http_ruuvi' was added in v1.14. To stay compatible with configs
    // produced by older firmware (where the field is absent and use_http==true
    // with the Ruuvi default URL meant "post to the Ruuvi cloud"), detect that
    // exact shape and rewrite it to the v1.14+ representation.
    if ((GW_CFG_HTTP_DATA_FORMAT_RUUVI == p_gw_cfg_http->data_format)
        && (GW_CFG_HTTP_AUTH_TYPE_NONE == p_gw_cfg_http->auth_type)
        && (0 == strcmp(RUUVI_GATEWAY_HTTP_DEFAULT_URL, p_gw_cfg_http->http_url.buf)))
    {
        p_gw_cfg_http->use_http_ruuvi = true;
        p_gw_cfg_http->use_http       = false;
    }
}

void
gw_cfg_json_parse_http(const cJSON* const p_json_root, ruuvi_gw_cfg_http_t* const p_gw_cfg_http)
{
    gw_cfg_json_get_bool_or_log(p_json_root, "use_http_ruuvi", &p_gw_cfg_http->use_http_ruuvi, true);
    gw_cfg_json_get_bool_or_log(p_json_root, "use_http", &p_gw_cfg_http->use_http, true);

    bool flag_warn_if_missing = p_gw_cfg_http->use_http;

    gw_cfg_json_parse_http_data_format(p_json_root, flag_warn_if_missing, &p_gw_cfg_http->data_format);
    gw_cfg_json_parse_http_auth_type(p_json_root, flag_warn_if_missing, &p_gw_cfg_http->auth_type);

    gw_cfg_json_copy_string_or_log(
        p_json_root,
        "http_url",
        &p_gw_cfg_http->http_url.buf[0],
        sizeof(p_gw_cfg_http->http_url.buf),
        flag_warn_if_missing);

    if (p_gw_cfg_http->use_http)
    {
        gw_cfg_json_parse_http_auth(p_json_root, p_gw_cfg_http);
        gw_cfg_json_patch_http_legacy_pre_v1_14(p_gw_cfg_http); // this function can disable 'use_http'
        flag_warn_if_missing = p_gw_cfg_http->use_http;
    }

    gw_cfg_json_get_uint32_or_log(p_json_root, "http_period", &p_gw_cfg_http->http_period, flag_warn_if_missing);
    gw_cfg_json_get_bool_or_log(
        p_json_root,
        "http_use_extra_http_path",
        &p_gw_cfg_http->http_use_extra_http_path,
        flag_warn_if_missing);
    gw_cfg_json_get_bool_or_log(
        p_json_root,
        "http_use_extra_http_query",
        &p_gw_cfg_http->http_use_extra_http_query,
        flag_warn_if_missing);
    gw_cfg_json_get_bool_or_log(
        p_json_root,
        "http_use_extra_http_headers",
        &p_gw_cfg_http->http_use_extra_http_headers,
        flag_warn_if_missing);

    gw_cfg_json_parse_http_ssl_certs(p_json_root, p_gw_cfg_http, flag_warn_if_missing);
}
