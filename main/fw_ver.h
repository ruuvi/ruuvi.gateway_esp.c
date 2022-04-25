/**
 * @file fw_ver.h
 * @author TheSomeMan
 * @date 2022-01-14
 * @copyright Ruuvi Innovations Ltd, license BSD-3-Clause.
 */

#ifndef RUUVI_GATEWAY_ESP_FW_VER_H
#define RUUVI_GATEWAY_ESP_FW_VER_H

#ifdef __cplusplus
extern "C" {
#endif

#define RUUVI_ESP32_FW_VER_SIZE (32U)

typedef struct ruuvi_esp32_fw_ver_str_t
{
    char buf[RUUVI_ESP32_FW_VER_SIZE];
} ruuvi_esp32_fw_ver_str_t;

#ifdef __cplusplus
}
#endif

#endif // RUUVI_GATEWAY_ESP_FW_VER_H
