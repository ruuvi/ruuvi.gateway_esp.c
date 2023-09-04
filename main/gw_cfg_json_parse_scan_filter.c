/**
 * @file gw_cfg_json_parse_scan_filter.c
 * @author TheSomeMan
 * @date 2021-09-29
 * @copyright Ruuvi Innovations Ltd, license BSD-3-Clause.
 */

#include "gw_cfg_json_parse_scan_filter.h"
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
gw_cfg_json_parse_scan_filter(const cJSON* const p_cjson, ruuvi_gw_cfg_scan_filter_t* const p_gw_cfg_scan_filter)
{
    if (!gw_cfg_json_get_bool_val(p_cjson, "scan_filter_allow_listed", &p_gw_cfg_scan_filter->scan_filter_allow_listed))
    {
        LOG_WARN("Can't find key '%s' in config-json", "scan_filter_allow_listed");
    }
    const cJSON* const p_json_scan_filter_list = cJSON_GetObjectItem(p_cjson, "scan_filter_list");
    if (NULL == p_json_scan_filter_list)
    {
        LOG_WARN("Can't find key '%s' in config-json", "scan_filter_list");
    }
    else
    {
        const int32_t scan_filter_length = cJSON_GetArraySize(p_json_scan_filter_list);
        uint32_t      arr_idx            = 0;
        for (int32_t i = 0; i < scan_filter_length; ++i)
        {
            cJSON* const      p_filter_item = cJSON_GetArrayItem(p_json_scan_filter_list, i);
            const char* const p_str         = cJSON_GetStringValue(p_filter_item);
            if (!mac_addr_from_str(p_str, &p_gw_cfg_scan_filter->scan_filter_list[arr_idx]))
            {
                LOG_ERR("Can't parse MAC address in scan_filter_list: %s", p_str);
            }
            arr_idx += 1;
        }
        p_gw_cfg_scan_filter->scan_filter_length = arr_idx;
    }
}
