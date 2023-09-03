/**
 * @file gw_cfg_json_parse_device_info.c
 * @author TheSomeMan
 * @date 2021-09-29
 * @copyright Ruuvi Innovations Ltd, license BSD-3-Clause.
 */

#include "gw_cfg_json_parse_device_info.h"
#include <string.h>
#include "gw_cfg_json_parse_internal.h"

void
gw_cfg_json_parse_device_info(const cJSON* const p_json_root, gw_cfg_device_info_t* const p_gw_cfg_dev_info)
{
    memset(p_gw_cfg_dev_info, 0, sizeof(*p_gw_cfg_dev_info));

    gw_cfg_json_copy_string_val(
        p_json_root,
        "fw_ver",
        &p_gw_cfg_dev_info->esp32_fw_ver.buf[0],
        sizeof(p_gw_cfg_dev_info->esp32_fw_ver.buf));
    gw_cfg_json_copy_string_val(
        p_json_root,
        "nrf52_fw_ver",
        &p_gw_cfg_dev_info->nrf52_fw_ver.buf[0],
        sizeof(p_gw_cfg_dev_info->nrf52_fw_ver.buf));
    gw_cfg_json_copy_string_val(
        p_json_root,
        "gw_mac",
        &p_gw_cfg_dev_info->nrf52_mac_addr.str_buf[0],
        sizeof(p_gw_cfg_dev_info->nrf52_mac_addr.str_buf));
}
