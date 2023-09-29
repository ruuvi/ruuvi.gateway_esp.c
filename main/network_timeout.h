/**
 * @file network_timeout.h
 * @author TheSomeMan
 * @date 2023-09-04
 * @copyright Ruuvi Innovations Ltd, license BSD-3-Clause.
 */

#ifndef RUUVI_GATEWAY_ESP_NETWORK_TIMEOUT_H
#define RUUVI_GATEWAY_ESP_NETWORK_TIMEOUT_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

void
network_timeout_update_timestamp(void);

bool
network_timeout_check(void);

#ifdef __cplusplus
}
#endif

#endif // RUUVI_GATEWAY_ESP_NETWORK_TIMEOUT_H
