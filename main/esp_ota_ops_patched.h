/**
 * @file esp_opa_patched.h
 * @author TheSomeMan
 * @date 2021-05-30
 * @copyright Ruuvi Innovations Ltd, license BSD-3-Clause.
 */

#ifndef RUUVI_GATEWAY_ESP_ESP_OTA_PATCHED_H
#define RUUVI_GATEWAY_ESP_ESP_OTA_PATCHED_H

#include <stddef.h>
#include "esp_err.h"
#include "esp_partition.h"
#include "esp_ota_ops.h"

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t
esp_ota_begin_patched(const esp_partition_t *p_partition, esp_ota_handle_t *p_out_handle);

esp_err_t
esp_ota_write_patched(esp_ota_handle_t handle, const void *p_data, size_t size);

esp_err_t
esp_ota_end_patched(esp_ota_handle_t handle);

#ifdef __cplusplus
}
#endif

#endif // RUUVI_GATEWAY_ESP_ESP_OTA_PATCHED_H
