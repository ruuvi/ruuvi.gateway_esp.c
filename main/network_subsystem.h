/**
 * @file network_subsystem.h
 * @author TheSomeMan
 * @date 2023-04-07
 * @copyright Ruuvi Innovations Ltd, license BSD-3-Clause.
 */

#ifndef RUUVI_GATEWAY_NETWORK_SUBSYSTEM_H
#define RUUVI_GATEWAY_NETWORK_SUBSYSTEM_H

#include <stdbool.h>
#include "settings.h"
#include "wifi_manager_defs.h"

#ifdef __cplusplus
extern "C" {
#endif

bool
network_subsystem_init(
    const force_start_wifi_hotspot_e force_start_wifi_hotspot,
    const wifiman_config_t* const    p_wifi_cfg);

bool
wifi_init(
    const bool                    flag_connect_sta,
    const wifiman_config_t* const p_wifi_cfg,
    const char* const             p_fatfs_gwui_partition_name);

#ifdef __cplusplus
}
#endif

#endif // RUUVI_GATEWAY_NETWORK_SUBSYSTEM_H
