/**
 * @file adv_post.h
 * @author Oleg Protasevich
 * @date 2020-09-11
 * @copyright Ruuvi Innovations Ltd, license BSD-3-Clause.
 */

#ifndef RUUVI_ADV_POST_H
#define RUUVI_ADV_POST_H

#include <stdbool.h>
#include <stdint.h>
#include "gw_cfg.h"
#include "http_json.h"

#ifdef __cplusplus
extern "C" {
#endif

void
adv_post_init(void);

bool
adv_post_is_initialized(void);

void
adv_post_stop(void);

void
adv_post_signal_send_ble_scan_changed(void);

void
adv_post_signal_send_activate_cfg_mode(void);

void
adv_post_signal_send_deactivate_cfg_mode(void);

#ifdef __cplusplus
}
#endif

#endif // RUUVI_ADV_POST_H
