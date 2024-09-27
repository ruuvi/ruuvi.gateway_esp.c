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

#ifdef __cplusplus
extern "C" {
#endif

void
metrics_init(void);

void
metrics_deinit(void);

void
metrics_received_advs_increment(const bool is_ext_adv);

uint64_t
metrics_received_advs_get(void);

char*
metrics_generate(void);

#ifdef __cplusplus
}
#endif

#endif // RUUVI_GATEWAY_ESP_METRICS_H
