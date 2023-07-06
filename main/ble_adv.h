/**
 * @file ble_adv.h
 * @author TheSomeMan
 * @date 2023-07-04
 * @copyright Ruuvi Innovations Ltd, license BSD-3-Clause.
 */

#ifndef RUUVI_GATEWAY_ESP_BLE_ADV_H
#define RUUVI_GATEWAY_ESP_BLE_ADV_H

#include <stdbool.h>
#include <stdint.h>
#include "ruuvi_endpoint_ca_uart.h"

#ifdef __cplusplus
extern "C" {
#endif

bool
ble_adv_init(void);

void
ble_adv_send(uint8_t buf[RE_CA_UART_ADV_BYTES]);

void
ble_adv_send_device_name(void);

#ifdef __cplusplus
}
#endif

#endif // RUUVI_GATEWAY_ESP_BLE_ADV_H
