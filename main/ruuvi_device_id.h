/**
 * @file ruuvi_device_id.h
 * @author TheSomeMan
 * @date 2021-07-08
 * @copyright Ruuvi Innovations Ltd, license BSD-3-Clause.
 */

#ifndef RUUVI_GATEWAY_ESP_DEVICE_ID_H
#define RUUVI_GATEWAY_ESP_DEVICE_ID_H

#include <stdint.h>
#include <stdbool.h>
#include "mac_addr.h"

#ifdef __cplusplus
extern "C" {
#endif

#define NRF52_DEVICE_ID_SIZE (8U)

typedef struct nrf52_device_id_t
{
    uint8_t id[NRF52_DEVICE_ID_SIZE];
} nrf52_device_id_t;

typedef struct nrf52_device_id_str_t
{
    char str_buf[(NRF52_DEVICE_ID_SIZE * 2) + (NRF52_DEVICE_ID_SIZE - 1) + 1]; // format: XX:XX:XX:XX:XX:XX:XX:XX
} nrf52_device_id_str_t;

typedef struct nrf52_device_info_t
{
    nrf52_device_id_t nrf52_device_id;
    mac_address_bin_t nrf52_mac_addr;
} nrf52_device_info_t;

void
ruuvi_device_id_init(void);

void
ruuvi_device_id_deinit(void);

void
ruuvi_device_id_set(const nrf52_device_id_t *const p_nrf52_device_id, const mac_address_bin_t *const p_nrf52_mac_addr);

nrf52_device_info_t
ruuvi_device_id_request_and_wait(void);

#ifdef __cplusplus
}
#endif

#endif // RUUVI_GATEWAY_ESP_DEVICE_ID_H
