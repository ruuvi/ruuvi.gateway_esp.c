/**
 * @file metrics.h
 * @author TheSomeMan
 * @date 2021-01-20
 * @copyright Ruuvi Innovations Ltd, license BSD-3-Clause.
 */

#ifndef RUUVI_GATEWAY_ESP_METRICS_H
#define RUUVI_GATEWAY_ESP_METRICS_H

#include <stdint.h>
#include <stdbool.h>
#include "ruuvi_endpoint_ca_uart.h"

#ifdef __cplusplus
extern "C" {
#endif

void
metrics_init(void);

void
metrics_deinit(void);

void
metrics_received_advs_increment(const re_ca_uart_ble_phy_e secondary_phy);

uint64_t
metrics_received_advs_get(void);

uint32_t
metrics_nrf_self_reboot_cnt_get(void);

void
metrics_nrf_self_reboot_cnt_inc(void);

uint32_t
metrics_nrf_ext_hw_reset_cnt_get(void);

void
metrics_nrf_ext_hw_reset_cnt_inc(void);

uint64_t
metrics_nrf_lost_ack_cnt_get(void);

void
metrics_nrf_lost_ack_cnt_inc(void);

char*
metrics_generate(void);

#ifdef __cplusplus
}
#endif

#endif // RUUVI_GATEWAY_ESP_METRICS_H
