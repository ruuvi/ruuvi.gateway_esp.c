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
adv_post_set_default_period(const uint32_t period_ms);

bool
adv_post_set_hmac_sha256_key(const char* const p_key_str);

void
adv1_post_timer_restart_with_default_period(void);

void
adv1_post_timer_restart_with_increased_period(void);

void
adv1_post_timer_restart_with_short_period(void);

void
adv2_post_timer_restart_with_default_period(void);

void
adv2_post_timer_restart_with_increased_period(void);

void
adv2_post_timer_restart_with_short_period(const uint32_t default_interval_ms);

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
