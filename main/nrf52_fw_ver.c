/**
 * @file nrf52_fw_ver.c
 * @author TheSomeMan
 * @date 2022-04-05
 * @copyright Ruuvi Innovations Ltd, license BSD-3-Clause.
 */

#include "nrf52_fw_ver.h"
#include <stdio.h>
#include "esp_type_wrapper.h"

#define BIT_IDX_OF_BYTE_3 (3U * 8U)
#define BIT_IDX_OF_BYTE_2 (2U * 8U)
#define BIT_IDX_OF_BYTE_1 (1U * 8U)
#define BIT_IDX_OF_BYTE_0 (0U * 8U)
#define BYTE_MASK         (0xFFU)

#define NRF52_FW_VER_UNDEFINED (0xFFFFFFFFU)

ruuvi_nrf52_fw_ver_str_t
nrf52_fw_ver_get_str(const ruuvi_nrf52_fw_ver_t* const p_nrf52_fw_ver)
{
    ruuvi_nrf52_fw_ver_str_t nrf52_fw_ver_str = { 0 };
    const uint32_t           fw_ver = (NULL != p_nrf52_fw_ver) ? p_nrf52_fw_ver->version : NRF52_FW_VER_UNDEFINED;
    (void)snprintf(
        nrf52_fw_ver_str.buf,
        sizeof(nrf52_fw_ver_str.buf),
        "v%u.%u.%u",
        (printf_uint_t)((fw_ver >> BIT_IDX_OF_BYTE_3) & BYTE_MASK),
        (printf_uint_t)((fw_ver >> BIT_IDX_OF_BYTE_2) & BYTE_MASK),
        (printf_uint_t)((fw_ver >> BIT_IDX_OF_BYTE_1) & BYTE_MASK));
    return nrf52_fw_ver_str;
}
