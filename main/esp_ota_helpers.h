/**
 * @copyright Ruuvi Innovations Ltd, license BSD-3-Clause.
 */

#ifndef RUUVI_GATEWAY_ESP_ESP_OTA_HELPERS_H
#define RUUVI_GATEWAY_ESP_ESP_OTA_HELPERS_H

#include <stdint.h>
#include <stdbool.h>
#include <esp_err.h>
#include <esp_partition.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*esp_ota_erase_callback_t)(const uint32_t offset, const uint32_t partition_size);

/**
 * @brief This function erases the given partition with delay between each sector erase.
 * @param p_partition Pointer to partition to erase.
 * @param delay_ticks Number of ticks to delay between erasing each sector.
 * @param callback Optional callback to be called after each sector is erased.
 * @return ESP_OK on success, error code otherwise.
 */
esp_err_t
esp_ota_helper_erase_partition_with_sleep(
    const esp_partition_t* const   p_partition,
    uint32_t const                 delay_ticks,
    const esp_ota_erase_callback_t callback);

/**
 * @brief This function erases the given partition safely by checking that the partition is valid inactive OTA
 * partition.
 * @note This function is just an extract of code from esp_ota_begin_patched function.
 * @param p_partition Pointer to partition to erase.
 * @param delay_ticks Number of ticks to delay between erasing each sector.
 * @param callback Optional callback to be called after each sector is erased.
 * @return ESP_OK on success, error code otherwise.
 */
esp_err_t
esp_ota_helper_safe_erase_app_partition(
    const esp_partition_t* const   p_partition,
    const uint32_t                 delay_ticks,
    const esp_ota_erase_callback_t callback);

#ifdef __cplusplus
}
#endif

#endif // RUUVI_GATEWAY_ESP_ESP_OTA_HELPERS_H
