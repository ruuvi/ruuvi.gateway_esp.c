/**
 * @copyright Ruuvi Innovations Ltd, license BSD-3-Clause.
 */

#include "esp_ota_helpers.h"
#include "esp_ota_ops_patched.h"
#include "esp_ota_ops.h"
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <bootloader_flash.h>
#include <mbedtls/sha256.h>
#include "esp_err.h"
#include "esp_partition.h"
#include "esp_image_format.h"
#include "esp_secure_boot.h"
#include "esp_flash_encrypt.h"
#include "sdkconfig.h"
#include "esp_flash_partitions.h"
#include "bootloader_common.h"
#include "sys/param.h"
#include "os_malloc.h"

#define LOG_LOCAL_LEVEL 3
#include "log.h"

const static char* TAG = "esp_ota_ops";

esp_err_t
esp_ota_helper_erase_partition_with_sleep(
    const esp_partition_t* const   p_partition,
    uint32_t const                 delay_ticks,
    const esp_ota_erase_callback_t callback,
    void* const                    p_user_data)
{
    assert(p_partition != NULL);
    if ((p_partition->size % SPI_FLASH_SEC_SIZE) != 0)
    {
        return ESP_ERR_INVALID_SIZE;
    }
    size_t offset = 0;
    while (offset < p_partition->size)
    {
        const uint32_t  sector_address = p_partition->address + offset;
        const esp_err_t err = esp_flash_erase_region(p_partition->flash_chip, sector_address, SPI_FLASH_SEC_SIZE);
        if (ESP_OK != err)
        {
            LOG_ERR("Failed to erase sector at 0x%08x, error %d", (unsigned)(p_partition->address + offset), err);
            return err;
        }
        if (NULL != callback)
        {
            callback(offset, p_partition->size, p_user_data);
        }
        vTaskDelay(delay_ticks);
        offset += SPI_FLASH_SEC_SIZE;
    }
    return ESP_OK;
}

esp_err_t
esp_ota_helper_safe_erase_app_partition(
    const esp_partition_t* const p_partition,
    uint32_t                     delay_ticks,
    esp_ota_erase_callback_t     callback,
    void* const                  p_user_data)
{
    if (NULL == p_partition)
    {
        return ESP_ERR_INVALID_ARG;
    }

    const esp_partition_t* const p_partition_verified = esp_partition_verify(p_partition);
    if (NULL == p_partition_verified)
    {
        return ESP_ERR_NOT_FOUND;
    }

    if (!esp_ota_is_ota_partition(p_partition_verified))
    {
        return ESP_ERR_INVALID_ARG;
    }

    const esp_partition_t* p_running_partition = esp_ota_get_running_partition();
    if (p_partition_verified == p_running_partition)
    {
        return ESP_ERR_OTA_PARTITION_CONFLICT;
    }

#ifdef CONFIG_BOOTLOADER_APP_ROLLBACK_ENABLE
    esp_ota_img_states_t ota_state_running_part = ESP_OTA_IMG_UNDEFINED;
    if ((ESP_OK == esp_ota_get_state_partition(p_running_partition, &ota_state_running_part))
        && (ESP_OTA_IMG_PENDING_VERIFY == ota_state_running_part))
    {
        LOG_ERR("Running app has not confirmed state (ESP_OTA_IMG_PENDING_VERIFY)");
        return ESP_ERR_OTA_ROLLBACK_INVALID_STATE;
    }
#endif

    const esp_err_t ret = esp_ota_helper_erase_partition_with_sleep(
        p_partition_verified,
        delay_ticks,
        callback,
        p_user_data);
    if (ESP_OK != ret)
    {
        return ret;
    }
    return ESP_OK;
}

static bool
esp_ota_helper_calc_sha256(const uint8_t* const p_data, const size_t data_len, uint8_t* const p_out_sha256)
{
    mbedtls_sha256_context* p_sha256_ctx = os_calloc(1, sizeof(*p_sha256_ctx));
    if (NULL == p_sha256_ctx)
    {
        return false;
    }
    mbedtls_sha256_init(p_sha256_ctx);
    if (mbedtls_sha256_starts(p_sha256_ctx, 0) < 0)
    {
        LOG_ERR("%s failed", "mbedtls_sha256_starts");
        mbedtls_sha256_free(p_sha256_ctx);
        os_free(p_sha256_ctx);
        return false;
    }
    if (mbedtls_sha256_update(p_sha256_ctx, p_data, data_len) < 0)
    {
        LOG_ERR("%s failed", "mbedtls_sha256_update");
        mbedtls_sha256_free(p_sha256_ctx);
        os_free(p_sha256_ctx);
        return false;
    }
    if (mbedtls_sha256_finish(p_sha256_ctx, p_out_sha256) < 0)
    {
        LOG_ERR("%s failed", "mbedtls_sha256_finish");
        mbedtls_sha256_free(p_sha256_ctx);
        os_free(p_sha256_ctx);
        return false;
    }
    mbedtls_sha256_free(p_sha256_ctx);
    os_free(p_sha256_ctx);
    return true;
}

bool
esp_ota_helper_calc_digest_for_partition(
    const esp_partition_t* const   p_partition,
    esp_ota_sha256_digest_t* const p_digest)
{
    const void* p_data = bootloader_mmap(p_partition->address, p_partition->size);
    if (NULL == p_data)
    {
        LOG_ERR(
            "bootloader_mmap failed for partition '%s' at address 0x%08x",
            p_partition->label,
            p_partition->address);
        return false;
    }
    if (!esp_ota_helper_calc_sha256(p_data, p_partition->size, &p_digest->digest[0]))
    {
        LOG_ERR("calc_sha256 failed for partition '%s' at address 0x%08x", p_partition->label, p_partition->address);
        bootloader_munmap(p_data);
        return false;
    }
    bootloader_munmap(p_data);
    return true;
}

bool
esp_ota_helper_calc_pub_key_digest_for_signature(
    const ets_secure_boot_signature_t* const p_sig_block,
    esp_ota_sha256_digest_t* const           p_pub_key_digest)
{
    if (!esp_ota_helper_calc_sha256(
            (const uint8_t*)&p_sig_block->block[0].key,
            sizeof(p_sig_block->block[0].key),
            &p_pub_key_digest->digest[0]))
    {
        LOG_ERR("%s failed", "calc_sha256");
        return false;
    }
    return true;
}

bool
esp_ota_helper_calc_pub_key_digest_for_signature_in_flash(
    const uint32_t                 signature_addr,
    esp_ota_sha256_digest_t* const p_pub_key_digest)
{
    const ets_secure_boot_signature_t* const p_sig_block = bootloader_mmap(
        signature_addr,
        sizeof(ets_secure_boot_signature_t));
    LOG_DUMP_DBG(
        (void*)&p_sig_block->block[0].key,
        sizeof(p_sig_block->block[0].key),
        "Public key (offset %u, size %u)",
        offsetof(ets_secure_boot_signature_t, block[0].key),
        sizeof(p_sig_block->block[0].key));
    const bool is_success = esp_ota_helper_calc_pub_key_digest_for_signature(p_sig_block, p_pub_key_digest);
    if (!is_success)
    {
        LOG_ERR("esp_ota_calc_pub_key_digest failed for address 0x%08x", signature_addr);
    }
    bootloader_munmap(p_sig_block);
    return is_success;
}

bool
esp_ota_helper_calc_pub_key_digest_for_app_image(
    const esp_image_metadata_t* const p_metadata,
    esp_ota_sha256_digest_t* const    p_pub_key_digest)
{
    if ((NULL == p_metadata) || (NULL == p_pub_key_digest))
    {
        LOG_ERR("Invalid argument");
        return false;
    }
    LOG_INFO(
        "Calc pub key digest for app image: addr=0x%08x, size: %u (0x%08x)",
        p_metadata->start_addr,
        p_metadata->image_len,
        p_metadata->image_len);
    LOG_INFO(
        "Image header: Magic: 0x%02x, Segment count: %u, SPI mode: 0x%02x, SPI size: 0x%02x, Entry addr: %08x",
        p_metadata->image.magic,
        p_metadata->image.segment_count,
        p_metadata->image.spi_mode,
        p_metadata->image.spi_size,
        p_metadata->image.entry_addr);
    uint32_t signature_offset = p_metadata->image_len;
#if CONFIG_SECURE_SIGNED_APPS_RSA_SCHEME
    signature_offset -= sizeof(ets_secure_boot_signature_t);
#endif
    LOG_DBG("signature_offset=0x%08x", signature_offset);
    if (!esp_ota_helper_calc_pub_key_digest_for_signature_in_flash(
            p_metadata->start_addr + signature_offset,
            p_pub_key_digest))
    {
        return false;
    }
    LOG_DUMP_DBG(
        p_pub_key_digest->digest,
        sizeof(p_pub_key_digest->digest),
        "Public key digest for partition at address 0x%08x, size 0x%08x",
        p_metadata->start_addr,
        signature_offset);
    return true;
}

bool
esp_ota_helper_data_partition_verify(
    const esp_ota_sha256_digest_t* const     p_digest,
    const ets_secure_boot_signature_t* const p_sig_block)
{
    esp_ota_sha256_digest_t verified_digest = { 0 };

    // If Secure Boot is not enabled or the pub key digest has not been written to eFuse yet,
    // then esp_secure_boot_verify_rsa_signature_block() will verify the signature using the pub key
    // embedded into sig_block and return ESP_OK.
    const esp_err_t err = esp_secure_boot_verify_rsa_signature_block(
        p_sig_block,
        &p_digest->digest[0],
        &verified_digest.digest[0]);
    if (ESP_OK != err)
    {
        LOG_ERR("esp_secure_boot_verify_rsa_signature_block failed");
        return false;
    }

    return true;
}
