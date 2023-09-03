/**
 * @file gw_cfg_json_parse_auto_update.c
 * @author TheSomeMan
 * @date 2021-09-29
 * @copyright Ruuvi Innovations Ltd, license BSD-3-Clause.
 */

#include "gw_cfg_json_parse_auto_update.h"
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

void
gw_cfg_json_parse_auto_update(const cJSON* const p_cjson, ruuvi_gw_cfg_auto_update_t* const p_cfg)
{
    char auto_update_cycle_str[AUTO_UPDATE_CYCLE_TYPE_STR_MAX_LEN];
    if (!gw_cfg_json_copy_string_val(
            p_cjson,
            "auto_update_cycle",
            &auto_update_cycle_str[0],
            sizeof(auto_update_cycle_str)))
    {
        LOG_WARN("Can't find key '%s' in config-json, leave the previous value unchanged", "auto_update_cycle");
    }
    else
    {
        if (0 == strcmp(AUTO_UPDATE_CYCLE_TYPE_STR_REGULAR, auto_update_cycle_str))
        {
            p_cfg->auto_update_cycle = AUTO_UPDATE_CYCLE_TYPE_REGULAR;
        }
        else if (0 == strcmp(AUTO_UPDATE_CYCLE_TYPE_STR_BETA_TESTER, auto_update_cycle_str))
        {
            p_cfg->auto_update_cycle = AUTO_UPDATE_CYCLE_TYPE_BETA_TESTER;
        }
        else if (0 == strcmp(AUTO_UPDATE_CYCLE_TYPE_STR_MANUAL, auto_update_cycle_str))
        {
            p_cfg->auto_update_cycle = AUTO_UPDATE_CYCLE_TYPE_MANUAL;
        }
        else
        {
            p_cfg->auto_update_cycle = AUTO_UPDATE_CYCLE_TYPE_REGULAR;
            LOG_WARN("Unknown auto_update_cycle='%s', use REGULAR", auto_update_cycle_str);
        }
    }

    if (!gw_cfg_json_get_uint8_val(p_cjson, "auto_update_weekdays_bitmask", &p_cfg->auto_update_weekdays_bitmask))
    {
        LOG_WARN("Can't find key '%s' in config-json", "auto_update_weekdays_bitmask");
    }
    if (!gw_cfg_json_get_uint8_val(p_cjson, "auto_update_interval_from", &p_cfg->auto_update_interval_from))
    {
        LOG_WARN("Can't find key '%s' in config-json", "auto_update_interval_from");
    }
    if (!gw_cfg_json_get_uint8_val(p_cjson, "auto_update_interval_to", &p_cfg->auto_update_interval_to))
    {
        LOG_WARN("Can't find key '%s' in config-json", "auto_update_interval_to");
    }
    if (!gw_cfg_json_get_int8_val(p_cjson, "auto_update_tz_offset_hours", &p_cfg->auto_update_tz_offset_hours))
    {
        LOG_WARN("Can't find key '%s' in config-json", "auto_update_tz_offset_hours");
    }
}
