/**
 * @file settings.h
 * @author TheSomeMan
 * @date 2020-10-25
 * @copyright Ruuvi Innovations Ltd, license BSD-3-Clause.
 */

#ifndef RUUVI_GATEWAY_ESP_SETTINGS_H
#define RUUVI_GATEWAY_ESP_SETTINGS_H

#include <stdint.h>
#include <stdbool.h>
#include "gw_cfg.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum force_start_wifi_hotspot_e
{
    FORCE_START_WIFI_HOTSPOT_DISABLED = 0,
    FORCE_START_WIFI_HOTSPOT_ONCE     = 1,
} force_start_wifi_hotspot_e;

typedef enum settings_in_nvs_status_e
{
    SETTINGS_IN_NVS_STATUS_OK,
    SETTINGS_IN_NVS_STATUS_NOT_EXIST,
    SETTINGS_IN_NVS_STATUS_EMPTY,
} settings_in_nvs_status_e;

bool
settings_check_in_flash(void);

void
settings_save_to_flash(const gw_cfg_t* const p_gw_cfg);

const gw_cfg_t*
settings_get_from_flash(const bool flag_allow_cfg_updating, settings_in_nvs_status_e* const p_status);

mac_address_bin_t
settings_read_mac_addr(void);

void
settings_write_mac_addr(const mac_address_bin_t* const p_mac_addr);

void
settings_update_mac_addr(const mac_address_bin_t mac_addr);

bool
settings_read_flag_rebooting_after_auto_update(void);

void
settings_write_flag_rebooting_after_auto_update(const bool flag_rebooting_after_auto_update);

force_start_wifi_hotspot_e
settings_read_flag_force_start_wifi_hotspot(void);

void
settings_write_flag_force_start_wifi_hotspot(const force_start_wifi_hotspot_e force_start_wifi_hotspot);

#ifdef __cplusplus
}
#endif

#endif // RUUVI_GATEWAY_ESP_SETTINGS_H
