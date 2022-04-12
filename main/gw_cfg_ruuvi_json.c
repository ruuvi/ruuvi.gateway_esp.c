/**
 * @file gw_cfg_ruuvi_json.c
 * @author TheSomeMan
 * @date 2021-09-29
 * @copyright Ruuvi Innovations Ltd, license BSD-3-Clause.
 */

#include "gw_cfg_ruuvi_json.h"
#include <stdio.h>
#include <string.h>
#include "gw_cfg.h"
#include "gw_cfg_default.h"
#include "json_ruuvi.h"
#include "gw_mac.h"
#include "gw_cfg_json.h"

#define LOG_LOCAL_LEVEL LOG_LEVEL_INFO
#include "log.h"

bool
gw_cfg_ruuvi_json_generate(const ruuvi_gateway_config_t *const p_cfg, cjson_wrap_str_t *const p_json_str)
{
    return gw_cfg_json_generate_without_passwords(p_cfg, p_json_str);
}
