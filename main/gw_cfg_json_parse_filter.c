/**
 * @file gw_cfg_json_parse_filter.c
 * @author TheSomeMan
 * @date 2021-09-29
 * @copyright Ruuvi Innovations Ltd, license BSD-3-Clause.
 */

#include "gw_cfg_json_parse_filter.h"
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
gw_cfg_json_parse_filter(const cJSON* const p_json_root, ruuvi_gw_cfg_filter_t* const p_gw_cfg_filter)
{
    if (!gw_cfg_json_get_uint16_val(p_json_root, "company_id", &p_gw_cfg_filter->company_id))
    {
        LOG_WARN("Can't find key '%s' in config-json", "company_id");
    }
    if (!gw_cfg_json_get_bool_val(p_json_root, "company_use_filtering", &p_gw_cfg_filter->company_use_filtering))
    {
        LOG_WARN("Can't find key '%s' in config-json", "company_use_filtering");
    }
}
