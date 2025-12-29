/**
 * @file nrf52_fw_ver.h
 * @author TheSomeMan
 * @date 2022-04-05
 * @copyright Ruuvi Innovations Ltd, license BSD-3-Clause.
 */

#ifndef RUUVI_GATEWAY_ESP_NRF52_FW_VER_H
#define RUUVI_GATEWAY_ESP_NRF52_FW_VER_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define RUUVI_NRF52FW_FIRMWARE_VERSION_SIZE (32U)

typedef struct ruuvi_nrf52_fw_ver_str_t
{
    char buf[RUUVI_NRF52FW_FIRMWARE_VERSION_SIZE];
} ruuvi_nrf52_fw_ver_str_t;

typedef struct ruuvi_nrf52_fw_ver_t
{
    uint32_t version;
} ruuvi_nrf52_fw_ver_t;

typedef struct ruuvi_nrf52_hw_rev_t
{
    uint32_t part_code;
} ruuvi_nrf52_hw_rev_t;

ruuvi_nrf52_fw_ver_str_t
nrf52_fw_ver_get_str(const ruuvi_nrf52_fw_ver_t* const p_nrf52_fw_ver);

#ifdef __cplusplus
}
#endif

#endif // RUUVI_GATEWAY_ESP_NRF52_FW_VER_H
