/**
 * @file ruuvi_nvs.h
 * @author TheSomeMan
 * @date 2022-05-07
 * @copyright Ruuvi Innovations Ltd, license BSD-3-Clause.
 */

#ifndef RUUVI_NVS_H
#define RUUVI_NVS_H

#include <stdbool.h>
#include "nvs.h"

#ifdef __cplusplus
extern "C" {
#endif

bool
ruuvi_nvs_init(void);

bool
ruuvi_nvs_deinit(void);

void
ruuvi_nvs_erase(void);

bool
ruuvi_nvs_init_gw_cfg_default(void);

bool
ruuvi_nvs_deinit_gw_cfg_default(void);

bool
ruuvi_nvs_open(nvs_open_mode_t open_mode, nvs_handle_t* p_handle);

bool
ruuvi_nvs_open_gw_cfg_default(nvs_open_mode_t open_mode, nvs_handle_t* p_handle);

#ifdef __cplusplus
}
#endif

#endif // RUUVI_NVS_H
