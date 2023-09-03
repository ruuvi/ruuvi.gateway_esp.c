/**
 * @file gw_cfg_json_parse_http_stat.c
 * @author TheSomeMan
 * @date 2021-09-29
 * @copyright Ruuvi Innovations Ltd, license BSD-3-Clause.
 */

#include "gw_cfg_json_parse_http_stat.h"
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

void
gw_cfg_json_parse_http_stat(const cJSON* const p_json_root, ruuvi_gw_cfg_http_stat_t* const p_gw_cfg_http_stat)
{
    if (!gw_cfg_json_get_bool_val(p_json_root, "use_http_stat", &p_gw_cfg_http_stat->use_http_stat))
    {
        LOG_WARN("Can't find key '%s' in config-json", "use_http_stat");
    }
    if (!gw_cfg_json_copy_string_val(
            p_json_root,
            "http_stat_url",
            &p_gw_cfg_http_stat->http_stat_url.buf[0],
            sizeof(p_gw_cfg_http_stat->http_stat_url.buf)))
    {
        LOG_WARN("Can't find key '%s' in config-json", "http_stat_url");
    }
    if (!gw_cfg_json_copy_string_val(
            p_json_root,
            "http_stat_user",
            &p_gw_cfg_http_stat->http_stat_user.buf[0],
            sizeof(p_gw_cfg_http_stat->http_stat_user.buf)))
    {
        LOG_WARN("Can't find key '%s' in config-json", "http_stat_user");
    }
    if (!gw_cfg_json_copy_string_val(
            p_json_root,
            "http_stat_pass",
            &p_gw_cfg_http_stat->http_stat_pass.buf[0],
            sizeof(p_gw_cfg_http_stat->http_stat_pass.buf)))
    {
        LOG_INFO("Can't find key '%s' in config-json, leave the previous value unchanged", "http_stat_pass");
    }
    if (p_gw_cfg_http_stat->use_http_stat)
    {
        if (!gw_cfg_json_get_bool_val(
                p_json_root,
                "http_stat_use_ssl_client_cert",
                &p_gw_cfg_http_stat->http_stat_use_ssl_client_cert))
        {
            LOG_WARN("Can't find key '%s' in config-json", "http_stat_use_ssl_client_cert");
        }
        if (!gw_cfg_json_get_bool_val(
                p_json_root,
                "http_stat_use_ssl_server_cert",
                &p_gw_cfg_http_stat->http_stat_use_ssl_server_cert))
        {
            LOG_WARN("Can't find key '%s' in config-json", "http_stat_use_ssl_server_cert");
        }
    }
}
