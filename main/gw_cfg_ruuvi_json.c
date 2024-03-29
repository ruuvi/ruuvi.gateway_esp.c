/**
 * @file gw_cfg_ruuvi_json.c
 * @author TheSomeMan
 * @date 2021-09-29
 * @copyright Ruuvi Innovations Ltd, license BSD-3-Clause.
 */

#include "gw_cfg_ruuvi_json.h"
#include "gw_cfg.h"
#include "gw_cfg_json_generate.h"

bool
gw_cfg_ruuvi_json_generate(const gw_cfg_t* const p_cfg, cjson_wrap_str_t* const p_json_str)
{
    return gw_cfg_json_generate_for_ui_client(p_cfg, p_json_str);
}
