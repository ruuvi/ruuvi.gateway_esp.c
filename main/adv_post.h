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
adv2_post_timer_restart_with_short_period(void);

void
adv_post_send_sig_ble_scan_changed(void);

void
adv_post_send_sig_activate_cfg_mode(void);

void
adv_post_send_sig_deactivate_cfg_mode(void);

void
adv_post_stop(void);

void
adv_post_last_successful_network_comm_timestamp_update(void);

bool
adv_post_stat(const ruuvi_gw_cfg_http_stat_t* const p_cfg_http_stat, void* const p_user_data);

http_json_statistics_info_t*
adv_post_generate_statistics_info(const str_buf_t* const p_reset_info);

#ifdef __cplusplus
}
#endif

#endif // RUUVI_ADV_POST_H
