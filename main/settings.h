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
#include "cjson_wrap.h"
#include "gw_cfg.h"

#ifdef __cplusplus
extern "C" {
#endif

void
settings_save_to_flash(const ruuvi_gateway_config_t *p_config);

void
settings_get_from_flash(ruuvi_gateway_config_t *p_gateway_config);

mac_address_bin_t
settings_read_mac_addr(void);

void
settings_write_mac_addr(const mac_address_bin_t *const p_mac_addr);

void
settings_update_mac_addr(const mac_address_bin_t *const p_mac_addr);

#ifdef __cplusplus
}
#endif

#endif // RUUVI_GATEWAY_ESP_SETTINGS_H
