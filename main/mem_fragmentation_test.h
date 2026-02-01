/**
 * @author TheSomeMan
 * @date 2026-02-01
 * @copyright Ruuvi Innovations Ltd, license BSD-3-Clause.
 */

#ifndef RUUVI_GATEWAY_ESP_MEM_FRAGMENTATION_TEST_H
#define RUUVI_GATEWAY_ESP_MEM_FRAGMENTATION_TEST_H

#include "ruuvi_gateway.h"

#if !defined(RUUVI_GATEWAY_ENABLE_MEM_FRAGMENTATION_TEST)
#error "RUUVI_GATEWAY_ENABLE_MEM_FRAGMENTATION_TEST must be defined"
#endif

#ifdef __cplusplus
extern "C" {
#endif

#if RUUVI_GATEWAY_ENABLE_MEM_FRAGMENTATION_TEST
void
mem_fragmentation_test(void);
#endif // RUUVI_GATEWAY_ENABLE_MEM_FRAGMENTATION_TEST

#ifdef __cplusplus
}
#endif

#endif // RUUVI_GATEWAY_ESP_MEM_FRAGMENTATION_TEST_H
