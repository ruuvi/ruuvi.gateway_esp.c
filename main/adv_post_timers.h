/**
 * @file adv_post_timers.h
 * @author TheSomeMan
 * @date 2023-09-04
 * @copyright Ruuvi Innovations Ltd, license BSD-3-Clause.
 */

#ifndef RUUVI_GATEWAY_ESP_ADV_POST_TIMERS_H
#define RUUVI_GATEWAY_ESP_ADV_POST_TIMERS_H

#include "os_timer_sig.h"

#ifdef __cplusplus
extern "C" {
#endif

void
adv_post_create_timers(void);

void
adv_post_delete_timers(void);

os_timer_sig_periodic_t*
adv_post_timers_get_timer_sig_green_led_update(void);

void
adv_post_timers_relaunch_timer_sig_retransmit_to_http_ruuvi(void);

void
adv1_post_timer_stop(void);

void
adv_post_timers_relaunch_timer_sig_retransmit_to_http_custom(void);

void
adv2_post_timer_stop(void);

void
adv_post_timers_restart_timer_sig_mqtt(const os_delta_ticks_t delay_ticks);

void
adv_post_timers_stop_timer_sig_mqtt(void);

void
adv_post_timers_stop_timer_sig_send_statistics(void);

void
adv_post_timers_relaunch_timer_sig_send_statistics(void);

void
adv_post_timers_postpone_sending_statistics(void);

void
adv_post_timers_start_timer_sig_do_async_comm(void);

void
adv_post_timers_stop_timer_sig_do_async_comm(void);

void
adv_post_timers_start_timer_sig_watchdog_feed(void);

void
adv_post_timers_start_timer_sig_network_watchdog(void);

void
adv_post_timers_start_timer_sig_recv_adv_timeout(void);

void
adv_post_timers_relaunch_timer_sig_recv_adv_timeout(void);

void
adv1_post_timer_set_default_period_by_server_resp(const uint32_t period_ms);

void
adv2_post_timer_set_default_period_by_server_resp(const uint32_t period_ms);

void
adv1_post_timer_relaunch_with_default_period(void);

void
adv1_post_timer_relaunch_with_increased_period(void);

void
adv2_post_timer_relaunch_with_default_period(void);

void
adv2_post_timer_relaunch_with_increased_period(void);

void
adv2_post_timer_set_default_period(const uint32_t period_ms);

#ifdef __cplusplus
}
#endif

#endif // RUUVI_GATEWAY_ESP_ADV_POST_TIMERS_H
