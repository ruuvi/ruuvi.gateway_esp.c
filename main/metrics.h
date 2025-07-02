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

typedef enum metrics_malloc_cap_e
{
    METRICS_MALLOC_CAP_EXEC,
    METRICS_MALLOC_CAP_32BIT,
    METRICS_MALLOC_CAP_8BIT,
    METRICS_MALLOC_CAP_DMA,
    METRICS_MALLOC_CAP_PID2,
    METRICS_MALLOC_CAP_PID3,
    METRICS_MALLOC_CAP_PID4,
    METRICS_MALLOC_CAP_PID5,
    METRICS_MALLOC_CAP_PID6,
    METRICS_MALLOC_CAP_PID7,
    METRICS_MALLOC_CAP_SPIRAM,
    METRICS_MALLOC_CAP_INTERNAL,
    METRICS_MALLOC_CAP_DEFAULT,
    METRICS_MALLOC_CAP_IRAM_8BIT,
} metrics_malloc_cap_e;

uint32_t
metrics_get_total_free_bytes(const metrics_malloc_cap_e malloc_cap);

uint32_t
metrics_get_largest_free_block(const metrics_malloc_cap_e malloc_cap);

char*
metrics_generate(void);

#ifdef __cplusplus
}
#endif

#endif // RUUVI_GATEWAY_ESP_METRICS_H
