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
#include <mbedtls/sha256.h>
#include "esp_err.h"
#include "esp_partition.h"
#include "esp_image_format.h"
#include "sdkconfig.h"
#include "esp_flash_partitions.h"
#include "sys/param.h"

#define LOG_LOCAL_LEVEL 4
#include "log.h"
#include "esp_log.h"

const static char* TAG = "esp_ota_ops";

esp_err_t
esp_ota_helper_erase_partition_with_sleep(
    const esp_partition_t* const   p_partition,
    uint32_t const                 delay_ticks,
    const esp_ota_erase_callback_t callback)
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
            ESP_LOGE(TAG, "Failed to erase sector at 0x%08x, error %d", (unsigned)(p_partition->address + offset), err);
            return err;
        }
        if (NULL != callback)
        {
            callback(offset, p_partition->size);
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
    esp_ota_erase_callback_t     callback)
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
        ESP_LOGE(TAG, "Running app has not confirmed state (ESP_OTA_IMG_PENDING_VERIFY)");
        return ESP_ERR_OTA_ROLLBACK_INVALID_STATE;
    }
#endif

    const esp_err_t ret = esp_ota_helper_erase_partition_with_sleep(p_partition_verified, delay_ticks, callback);
    if (ESP_OK != ret)
    {
        return ret;
    }
    return ESP_OK;
}
