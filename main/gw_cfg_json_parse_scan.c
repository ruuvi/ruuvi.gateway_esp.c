/**
 * @file gw_cfg_json_parse_scan.c
 * @author TheSomeMan
 * @date 2021-09-29
 * @copyright Ruuvi Innovations Ltd, license BSD-3-Clause.
 */

#include "gw_cfg_json_parse_scan.h"
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

void
gw_cfg_json_parse_scan(const cJSON* const p_json_root, ruuvi_gw_cfg_scan_t* const p_gw_cfg_scan)
{
    bool flag_scan_default_exists = true;
    if (!gw_cfg_json_get_bool_val(p_json_root, "scan_default", &p_gw_cfg_scan->scan_default))
    {
        LOG_WARN("Can't find key '%s' in config-json", "scan_default");
        flag_scan_default_exists = false;
    }
    if (p_gw_cfg_scan->scan_default)
    {
        const ruuvi_gw_cfg_scan_t* const p_gw_cfg_default_scan = gw_cfg_default_get_scan();
        p_gw_cfg_scan->scan_coded_phy                          = p_gw_cfg_default_scan->scan_coded_phy;
        p_gw_cfg_scan->scan_1mbit_phy                          = p_gw_cfg_default_scan->scan_1mbit_phy;
        p_gw_cfg_scan->scan_2mbit_phy                          = p_gw_cfg_default_scan->scan_2mbit_phy;
        p_gw_cfg_scan->scan_channel_37                         = p_gw_cfg_default_scan->scan_channel_37;
        p_gw_cfg_scan->scan_channel_38                         = p_gw_cfg_default_scan->scan_channel_38;
        p_gw_cfg_scan->scan_channel_39                         = p_gw_cfg_default_scan->scan_channel_39;
        p_gw_cfg_scan->scan_default                            = p_gw_cfg_default_scan->scan_default;
    }
    else
    {
        if (!gw_cfg_json_get_bool_val(p_json_root, "scan_coded_phy", &p_gw_cfg_scan->scan_coded_phy))
        {
            LOG_WARN("Can't find key '%s' in config-json", "scan_coded_phy");
        }
        if (!gw_cfg_json_get_bool_val(p_json_root, "scan_1mbit_phy", &p_gw_cfg_scan->scan_1mbit_phy))
        {
            LOG_WARN("Can't find key '%s' in config-json", "scan_1mbit_phy");
        }

        // Parse deprecated key "scan_extended_payload" for backward compatibility,
        // this field was replaced with "scan_2mbit_phy" in v1.16.x
        bool flag_need_to_conv_conf = false;
        if (gw_cfg_json_get_bool_val(p_json_root, "scan_extended_payload", &p_gw_cfg_scan->scan_2mbit_phy))
        {
            LOG_WARN("Found deprecated key '%s' in config-json", "scan_extended_payload");
            flag_need_to_conv_conf = true;
        }

        if (!gw_cfg_json_get_bool_val(p_json_root, "scan_2mbit_phy", &p_gw_cfg_scan->scan_2mbit_phy))
        {
            LOG_WARN("Can't find key '%s' in config-json", "scan_2mbit_phy");
        }
        if (!gw_cfg_json_get_bool_val(p_json_root, "scan_channel_37", &p_gw_cfg_scan->scan_channel_37))
        {
            LOG_WARN("Can't find key '%s' in config-json", "scan_channel_37");
        }
        if (!gw_cfg_json_get_bool_val(p_json_root, "scan_channel_38", &p_gw_cfg_scan->scan_channel_38))
        {
            LOG_WARN("Can't find key '%s' in config-json", "scan_channel_38");
        }
        if (!gw_cfg_json_get_bool_val(p_json_root, "scan_channel_39", &p_gw_cfg_scan->scan_channel_39))
        {
            LOG_WARN("Can't find key '%s' in config-json", "scan_channel_39");
        }
        if (flag_need_to_conv_conf && !flag_scan_default_exists)
        {
            LOG_INFO("Convert deprecated key '%s' to '%s'", "scan_extended_payload", "scan_2mbit_phy");
            if ((!p_gw_cfg_scan->scan_coded_phy) && p_gw_cfg_scan->scan_1mbit_phy && p_gw_cfg_scan->scan_2mbit_phy
                && p_gw_cfg_scan->scan_channel_37 && p_gw_cfg_scan->scan_channel_38 && p_gw_cfg_scan->scan_channel_39)
            {
                LOG_INFO("Set scan_default=true");
                p_gw_cfg_scan->scan_default                            = true;
                const ruuvi_gw_cfg_scan_t* const p_gw_cfg_default_scan = gw_cfg_default_get_scan();
                p_gw_cfg_scan->scan_coded_phy                          = p_gw_cfg_default_scan->scan_coded_phy;
                p_gw_cfg_scan->scan_1mbit_phy                          = p_gw_cfg_default_scan->scan_1mbit_phy;
                p_gw_cfg_scan->scan_2mbit_phy                          = p_gw_cfg_default_scan->scan_2mbit_phy;
                p_gw_cfg_scan->scan_channel_37                         = p_gw_cfg_default_scan->scan_channel_37;
                p_gw_cfg_scan->scan_channel_38                         = p_gw_cfg_default_scan->scan_channel_38;
                p_gw_cfg_scan->scan_channel_39                         = p_gw_cfg_default_scan->scan_channel_39;
            }
            else
            {
                LOG_INFO("Set scan_default=false");
                p_gw_cfg_scan->scan_default = false;
            }
        }
    }
}
