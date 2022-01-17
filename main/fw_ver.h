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

#define FW_VER_SIZE (32U)

typedef struct fw_update_app_version_str_t
{
    char buf[FW_VER_SIZE];
} fw_ver_str_t;

#ifdef __cplusplus
}
#endif

#endif // RUUVI_GATEWAY_ESP_FW_VER_H
