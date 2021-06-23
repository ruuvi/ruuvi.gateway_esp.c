/**
 * @file adv_post.h
 * @author Oleg Protasevich
 * @date 2020-09-11
 * @copyright Ruuvi Innovations Ltd, license BSD-3-Clause.
 */

#ifndef RUUVI_ADV_POST_H
#define RUUVI_ADV_POST_H

#include "ruuvi_gateway.h"
#include <stdbool.h>
#include "gw_cfg.h"

#ifdef __cplusplus
extern "C" {
#endif

void
adv_post_init(void);

bool
is_nrf52_id_received(void);

mac_address_bin_t
nrf52_get_mac_address(void);

mac_address_str_t
nrf52_get_mac_address_str(void);

nrf52_device_id_t
nrf52_get_device_id(void);

nrf52_device_id_str_t
nrf52_get_device_id_str(void);

#ifdef __cplusplus
}
#endif

#endif // RUUVI_ADV_POST_H
