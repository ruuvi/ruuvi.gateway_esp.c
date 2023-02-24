/**
 * @file reset_reason.h
 * @author TheSomeMan
 * @date 2022-11-18
 * @copyright Ruuvi Innovations Ltd, license BSD-3-Clause.
 */

#ifndef RUUVI_GATEWAY_ESP_RESET_REASON_H
#define RUUVI_GATEWAY_ESP_RESET_REASON_H

#include <esp_system.h>

#ifdef __cplusplus
extern "C" {
#endif

const char*
reset_reason_to_str(const esp_reset_reason_t reset_reason);

#ifdef __cplusplus
}
#endif

#endif // RUUVI_GATEWAY_ESP_RESET_REASON_H
