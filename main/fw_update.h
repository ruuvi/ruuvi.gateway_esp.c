/**
 * @file fw_update.h
 * @author TheSomeMan
 * @date 2021-05-30
 * @copyright Ruuvi Innovations Ltd, license BSD-3-Clause.
 */

#ifndef RUUVI_GATEWAY_ESP_FW_UPDATE_H
#define RUUVI_GATEWAY_ESP_FW_UPDATE_H

#include <stdbool.h>
#include "esp_ota_ops.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct ruuvi_flash_info_t
{
    bool                   is_valid;
    bool                   is_ota0_active;
    const esp_app_desc_t * p_app_desc;
    const esp_partition_t *p_boot_partition;
    const esp_partition_t *p_running_partition;
    const esp_partition_t *p_next_update_partition;
    const esp_partition_t *p_next_fatfs_gwui_partition;
    const esp_partition_t *p_next_fatfs_nrf52_partition;
} ruuvi_flash_info_t;

bool
fw_update_read_flash_info(void);

const char *
fw_update_get_current_fatfs_nrf52_partition_name(void);

const char *
fw_update_get_current_fatfs_gwui_partition_name(void);

const char *
fw_update_get_cur_version(void);

#ifdef __cplusplus
}
#endif

#endif // RUUVI_GATEWAY_ESP_FW_UPDATE_H
