/**
 * @file gw_cfg_json_parse_fw_update.c
 * @author TheSomeMan
 * @date 2021-09-29
 * @copyright Ruuvi Innovations Ltd, license BSD-3-Clause.
 */

#include "gw_cfg_json_parse_fw_update.h"
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
gw_cfg_json_parse_fw_update(const cJSON* const p_cjson, ruuvi_gw_cfg_fw_update_t* const p_gw_cfg_fw_update)
{
    if (!gw_cfg_json_copy_string_val(
            p_cjson,
            "fw_update_url",
            p_gw_cfg_fw_update->fw_update_url,
            sizeof(p_gw_cfg_fw_update->fw_update_url)))
    {
        LOG_WARN("Can't find key '%s' in config-json", "fw_update_url");
    }
}
