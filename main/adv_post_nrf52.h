/**
 * @file adv_post_nrf52.h
 * @author TheSomeMan
 * @date 2025-05-09
 * @copyright Ruuvi Innovations Ltd, license BSD-3-Clause.
 */

#ifndef RUUVI_GATEWAY_ESP_ADV_POST_NRF52_H
#define RUUVI_GATEWAY_ESP_ADV_POST_NRF52_H

#include <stdint.h>
#include <stdbool.h>
#include "gw_cfg.h"
#include "ruuvi_endpoint_ca_uart.h"

#ifdef __cplusplus
extern "C" {
#endif

void
adv_post_nrf52_init(void);

bool
adv_post_nrf52_is_in_hw_reset_state(void);

bool
adv_post_nrf52_is_configured(void);

void
adv_post_nrf52_send_led_ctrl(const uint16_t time_interval_ms);

void
adv_post_nrf52_on_sig_nrf52_rebooted(void);

void
adv_post_nrf52_on_sig_nrf52_configured(void);

void
adv_post_nrf52_on_sig_nrf52_cfg_update(void);

void
adv_post_nrf52_on_sig_nrf52_hw_reset_off(void);

void
adv_post_nrf52_on_sig_nrf52_cfg_req_timeout(void);

void
adv_post_nrf52_on_async_ack(const re_ca_uart_cmd_t cmd, const re_ca_uart_ble_bool_t ack_state);

void
adv_post_nrf52_on_sync_ack_led_ctrl(void);

void
adv_post_nrf52_on_sync_ack_cfg(void);

void
adv_post_nrf52_on_sync_ack_timeout(void);

void
adv_post_nrf52_cfg_update(const ruuvi_gw_cfg_scan_t* const p_scan, const ruuvi_gw_cfg_filter_t* const p_filter);

#ifdef __cplusplus
}
#endif

#endif // RUUVI_GATEWAY_ESP_ADV_POST_NRF52_H
