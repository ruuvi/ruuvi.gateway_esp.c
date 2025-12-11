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
esp_ota_begin_patched(const esp_partition_t* const p_partition, esp_ota_handle_t* const p_out_handle);

esp_err_t
esp_ota_write_patched(const esp_ota_handle_t handle, const void* const p_data, const size_t size);

esp_err_t
esp_ota_end_patched(const esp_ota_handle_t handle);

/**
 * @brief This function erases the given partition safely by checking that the partition is valid inactive OTA
 * partition.
 * @note This function is just an extract of code from esp_ota_begin_patched function.
 * @param p_partition Pointer to partition to erase.
 * @return ESP_OK on success, error code otherwise.
 */
esp_err_t
esp_ota_safe_erase(const esp_partition_t* const p_partition);

#ifdef __cplusplus
}
#endif

#endif // RUUVI_GATEWAY_ESP_ESP_OTA_PATCHED_H
