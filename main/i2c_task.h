/**
 * @file i2c_task.h
 * @author TheSomeMan
 * @date 2023-07-03
 * @copyright Ruuvi Innovations Ltd, license BSD-3-Clause.
 */

#ifndef RUUVI_GATEWAY_ESP_I2C_TASK_H
#define RUUVI_GATEWAY_ESP_I2C_TASK_H

#include <stdbool.h>
#include <stdint.h>
#include "ruuvi_endpoint_ca_uart.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct i2c_task_ble_adv_packet_t
{
    uint8_t len;
    uint8_t buf[RE_CA_UART_ADV_BYTES];
} i2c_task_ble_adv_packet_t;

bool
i2c_task_init(void);

bool
i2c_task_get_ble_adv_packet(i2c_task_ble_adv_packet_t* const p_packet);

#ifdef __cplusplus
}
#endif

#endif // RUUVI_GATEWAY_ESP_I2C_TASK_H
