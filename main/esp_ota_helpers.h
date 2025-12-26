/**
 * @copyright Ruuvi Innovations Ltd, license BSD-3-Clause.
 */

#ifndef RUUVI_GATEWAY_ESP_ESP_OTA_HELPERS_H
#define RUUVI_GATEWAY_ESP_ESP_OTA_HELPERS_H

#include <stdint.h>
#include <stdbool.h>
#include <esp_err.h>
#include <esp_partition.h>
#include <esp_secure_boot.h>

#ifdef __cplusplus
extern "C" {
#endif

#define ESP_OTA_SHA256_DIGEST_SIZE (32U)

typedef struct esp_ota_sha256_digest_t
{
    uint8_t digest[ESP_OTA_SHA256_DIGEST_SIZE];
} esp_ota_sha256_digest_t;

typedef void (*esp_ota_erase_callback_t)(const uint32_t offset, const uint32_t partition_size, void* const p_user_data);

/**
 * @brief This function erases the given partition with delay between each sector erase.
 * @param p_partition Pointer to partition to erase.
 * @param delay_ticks Number of ticks to delay between erasing each sector.
 * @param callback Optional callback to be called after each sector is erased.
 * @param p_user_data Optional user data pointer passed to the callback.
 * @return ESP_OK on success, error code otherwise.
 */
esp_err_t
esp_ota_helper_erase_partition_with_sleep(
    const esp_partition_t* const   p_partition,
    uint32_t const                 delay_ticks,
    const esp_ota_erase_callback_t callback,
    void* const                    p_user_data);

/**
 * @brief This function erases the given partition safely by checking that the partition is valid inactive OTA
 * partition.
 * @note This function is just an extract of code from esp_ota_begin_patched function.
 * @param p_partition Pointer to partition to erase.
 * @param delay_ticks Number of ticks to delay between erasing each sector.
 * @param callback Optional callback to be called after each sector is erased.
 * @param p_user_data Optional user data pointer passed to the callback.
 * @return ESP_OK on success, error code otherwise.
 */
esp_err_t
esp_ota_helper_safe_erase_app_partition(
    const esp_partition_t* const   p_partition,
    const uint32_t                 delay_ticks,
    const esp_ota_erase_callback_t callback,
    void* const                    p_user_data);

/**
 * @brief This function calculates the public key digest from secure boot signature.
 * @param p_sig_block Pointer to signature block.
 * @param[OUT] p_pub_key_digest Pointer to public key digest structure to fill.
 * @return true on success, false otherwise.
 */
bool
esp_ota_helper_calc_pub_key_digest_for_signature(
    const ets_secure_boot_signature_t* const p_sig_block,
    esp_ota_sha256_digest_t* const           p_pub_key_digest);

/**
 * @brief This function calculates the public key digest from signature.
 * @param signature_addr Address of the signature block in flash.
 * @param[OUT] p_pub_key_digest Pointer to public key digest structure to fill.
 * @return true on success, false otherwise.
 */
bool
esp_ota_helper_calc_pub_key_digest_for_signature_in_flash(
    const uint32_t                 signature_addr,
    esp_ota_sha256_digest_t* const p_pub_key_digest);

/**
 * @brief This function calculates the public key digest from image metadata.
 * @param p_metadata Pointer to image metadata.
 * @param[OUT] p_pub_key_digest Pointer to public key digest structure to fill.
 * @return true on success, false otherwise.
 */
bool
esp_ota_helper_calc_pub_key_digest_for_app_image(
    const esp_image_metadata_t* const p_metadata,
    esp_ota_sha256_digest_t* const    p_pub_key_digest);

#ifdef __cplusplus
}
#endif

#endif // RUUVI_GATEWAY_ESP_ESP_OTA_HELPERS_H
