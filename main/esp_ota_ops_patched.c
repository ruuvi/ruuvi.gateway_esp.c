// Copyright 2015-2016 Espressif Systems (Shanghai) PTE LTD
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at

//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

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
#include <esp_attr.h>
#include "os_malloc.h"

#include "esp_err.h"
#include "esp_partition.h"
#include "esp_image_format.h"
#include "esp_secure_boot.h"
#include "esp_flash_encrypt.h"
#include "sdkconfig.h"

#include "sys/queue.h"
#include "esp_flash_partitions.h"
#include "bootloader_common.h"
#include "sys/param.h"

#define LOG_LOCAL_LEVEL 3
#include "esp_log.h"

#define ESP_OTA_FLASH_ENCRYPTION_MIN_CHUNK_SIZE (16U)
#define ESP_OTA_FLASH_ENCRYPTION_FILL           (0xFFU)

typedef struct ota_ops_entry_t
{
    uint32_t               handle;
    const esp_partition_t* part;
    uint32_t               erased_size;
    uint32_t               wrote_size;
    uint8_t                partial_bytes;
    uint8_t                partial_data[ESP_OTA_FLASH_ENCRYPTION_MIN_CHUNK_SIZE];
    LIST_ENTRY(ota_ops_entry_t) entries;
} ota_ops_entry_t;

typedef LIST_HEAD(ota_ops_entries_head, ota_ops_entry_t) ota_ops_entries_head_t;

static ota_ops_entries_head_t s_ota_ops_entries_head = LIST_HEAD_INITIALIZER(s_ota_ops_entries_head);

static uint32_t IRAM_ATTR s_ota_ops_last_handle = 0;

const static char TAG[] = "esp_ota_ops";

/* Return true if this is an OTA app partition */
bool
esp_ota_is_ota_partition(const esp_partition_t* p)
{
    return (
        (NULL != p) && (ESP_PARTITION_TYPE_APP == p->type) && (p->subtype >= ESP_PARTITION_SUBTYPE_APP_OTA_0)
        && (p->subtype < ESP_PARTITION_SUBTYPE_APP_OTA_MAX));
}

esp_err_t
esp_ota_begin_patched(
    const esp_partition_t* const   p_partition,
    esp_ota_handle_t* const        p_out_handle,
    uint32_t const                 delay_ticks,
    const esp_ota_erase_callback_t callback,
    void* const                    p_user_data)
{
    if ((NULL == p_partition) || (NULL == p_out_handle))
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

    const bool flag_erase_only_first_sector = false;

    const esp_err_t ret = esp_ota_helper_erase_partition_with_sleep(
        p_partition_verified,
        flag_erase_only_first_sector,
        delay_ticks,
        callback,
        p_user_data);
    if (ESP_OK != ret)
    {
        return ret;
    }

    ota_ops_entry_t* const p_new_entry = (ota_ops_entry_t*)os_calloc(sizeof(ota_ops_entry_t), 1);
    if (NULL == p_new_entry)
    {
        return ESP_ERR_NO_MEM;
    }

    LIST_INSERT_HEAD(&s_ota_ops_entries_head, p_new_entry, entries);

    p_new_entry->erased_size = p_partition_verified->size;

    p_new_entry->part   = p_partition_verified;
    p_new_entry->handle = ++s_ota_ops_last_handle;
    *p_out_handle       = p_new_entry->handle;
    return ESP_OK;
}

static esp_err_t
esp_ota_write_entry(ota_ops_entry_t* const p_it, const void* const p_data, const size_t size)
{
    const uint8_t* p_data_bytes        = (const uint8_t*)p_data;
    size_t         cnt_remaining_bytes = size;

    // must erase the partition before writing to p_it
    assert((p_it->erased_size > 0) && "must erase the partition before writing to p_it");
    if ((0 == p_it->wrote_size) && (0 == p_it->partial_bytes) && (cnt_remaining_bytes > 0)
        && (ESP_IMAGE_HEADER_MAGIC != p_data_bytes[0]))
    {
        ESP_LOGE(TAG, "OTA image has invalid magic byte (expected 0xE9, saw 0x%02x)", p_data_bytes[0]);
        return ESP_ERR_OTA_VALIDATE_FAILED;
    }

    if (esp_flash_encryption_enabled())
    {
        /* Can only write 16 byte blocks to flash, so need to cache anything else */

        /* check if we have partially written p_data from earlier */
        if (0 != p_it->partial_bytes)
        {
            const size_t copy_len = MIN(
                ESP_OTA_FLASH_ENCRYPTION_MIN_CHUNK_SIZE - p_it->partial_bytes,
                cnt_remaining_bytes);
            memcpy(p_it->partial_data + p_it->partial_bytes, p_data_bytes, copy_len);
            p_it->partial_bytes += copy_len;
            if (ESP_OTA_FLASH_ENCRYPTION_MIN_CHUNK_SIZE != p_it->partial_bytes)
            {
                return ESP_OK; /* nothing to write yet, just filling buffer */
            }
            /* write 16 byte to partition */
            const esp_err_t ret = esp_partition_write(
                p_it->part,
                p_it->wrote_size,
                p_it->partial_data,
                ESP_OTA_FLASH_ENCRYPTION_MIN_CHUNK_SIZE);
            if (ESP_OK != ret)
            {
                return ret;
            }
            p_it->partial_bytes = 0;
            memset(p_it->partial_data, ESP_OTA_FLASH_ENCRYPTION_FILL, ESP_OTA_FLASH_ENCRYPTION_MIN_CHUNK_SIZE);
            p_it->wrote_size += ESP_OTA_FLASH_ENCRYPTION_MIN_CHUNK_SIZE;
            p_data_bytes += copy_len;
            cnt_remaining_bytes -= copy_len;
        }

        /* check if we need to save trailing p_data that we're about to write */
        p_it->partial_bytes = cnt_remaining_bytes % ESP_OTA_FLASH_ENCRYPTION_MIN_CHUNK_SIZE;
        if (0 != p_it->partial_bytes)
        {
            cnt_remaining_bytes -= p_it->partial_bytes;
            memcpy(p_it->partial_data, p_data_bytes + cnt_remaining_bytes, p_it->partial_bytes);
        }
    }

    const esp_err_t ret = esp_partition_write(p_it->part, p_it->wrote_size, p_data_bytes, cnt_remaining_bytes);
    if (ESP_OK == ret)
    {
        p_it->wrote_size += cnt_remaining_bytes;
    }
    return ret;
}

esp_err_t
esp_ota_write_patched(const esp_ota_handle_t handle, const void* const p_data, const size_t size)
{
    if (NULL == p_data)
    {
        ESP_LOGE(TAG, "write p_data is invalid");
        return ESP_ERR_INVALID_ARG;
    }

    // find ota handle in linked list
    for (ota_ops_entry_t* p_it = LIST_FIRST(&s_ota_ops_entries_head); p_it != NULL; p_it = LIST_NEXT(p_it, entries))
    {
        if (p_it->handle == handle)
        {
            return esp_ota_write_entry(p_it, p_data, size);
        }
    }

    // if go to here ,means don't find the handle
    ESP_LOGE(TAG, "not found the handle");
    return ESP_ERR_INVALID_ARG;
}

esp_err_t
esp_ota_end_patched(const esp_ota_handle_t handle, esp_ota_sha256_digest_t* const p_pub_key_digest)
{
    ota_ops_entry_t* p_it = NULL;
    esp_err_t        ret  = ESP_OK;

    for (p_it = LIST_FIRST(&s_ota_ops_entries_head); p_it != NULL; p_it = LIST_NEXT(p_it, entries))
    {
        if (p_it->handle == handle)
        {
            break;
        }
    }

    if (NULL == p_it)
    {
        return ESP_ERR_NOT_FOUND;
    }

    /* 'p_it' holds the ota_ops_entry_t for 'handle' */

    // esp_ota_end() is only valid if some data was written to this handle
    if ((0 == p_it->erased_size) || (0 == p_it->wrote_size))
    {
        ret = ESP_ERR_INVALID_ARG;
    }
    else
    {
        if (p_it->partial_bytes > 0)
        {
            /* Write out last 16 bytes, if necessary */
            ret = esp_partition_write(
                p_it->part,
                p_it->wrote_size,
                p_it->partial_data,
                ESP_OTA_FLASH_ENCRYPTION_MIN_CHUNK_SIZE);
            if (ESP_OK != ret)
            {
                ret = ESP_ERR_INVALID_STATE;
            }
            else
            {
                p_it->wrote_size += ESP_OTA_FLASH_ENCRYPTION_MIN_CHUNK_SIZE;
                p_it->partial_bytes = 0;
            }
        }

        if (ESP_OK == ret)
        {
            const esp_partition_pos_t part_pos = {
                .offset = p_it->part->address,
                .size   = p_it->part->size,
            };

            esp_image_metadata_t* p_metadata = os_calloc(1, sizeof(*p_metadata));
            if (NULL == p_metadata)
            {
                ret = ESP_ERR_NO_MEM;
            }
            else
            {
                if (esp_image_verify(ESP_IMAGE_VERIFY, &part_pos, p_metadata) != ESP_OK)
                {
                    ESP_LOGE(
                        TAG,
                        "Image verify failed for partition at address 0x%08x, size 0x%08x",
                        part_pos.offset,
                        part_pos.size);
                    ret = ESP_ERR_OTA_VALIDATE_FAILED;
                }
                else
                {
                    if (!esp_ota_helper_calc_pub_key_digest_for_app_image(p_metadata, p_pub_key_digest))
                    {
                        ESP_LOGE(
                            TAG,
                            "Public key digest calculation failed for partition at address 0x%08x, size 0x%08x",
                            part_pos.offset,
                            part_pos.size);
                        ret = ESP_ERR_OTA_VALIDATE_FAILED;
                    }
                }
            }
            os_free(p_metadata);
        }
    }

    LIST_REMOVE(p_it, entries);
    os_free(p_it);
    return ret;
}

static uint8_t
get_ota_partition_count(void)
{
    uint16_t ota_app_count = 0;
    while (esp_partition_find_first(ESP_PARTITION_TYPE_APP, ESP_PARTITION_SUBTYPE_APP_OTA_MIN + ota_app_count, NULL)
           != NULL)
    {
        assert(ota_app_count < 16 && "must erase the partition before writing to it");
        ota_app_count++;
    }
    return ota_app_count;
}

// Read otadata partition and fill array from two otadata structures.
// Also return pointer to otadata info partition.
static const esp_partition_t*
read_otadata(esp_ota_select_entry_t* two_otadata)
{
    const esp_partition_t* otadata_partition = esp_partition_find_first(
        ESP_PARTITION_TYPE_DATA,
        ESP_PARTITION_SUBTYPE_DATA_OTA,
        NULL);

    if (otadata_partition == NULL)
    {
        ESP_LOGE(TAG, "not found otadata");
        return NULL;
    }

    spi_flash_mmap_handle_t ota_data_map;
    const void*             result = NULL;
    esp_err_t               err    = esp_partition_mmap(
        otadata_partition,
        0,
        otadata_partition->size,
        SPI_FLASH_MMAP_DATA,
        &result,
        &ota_data_map);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "mmap otadata filed. Err=0x%8x", err);
        return NULL;
    }
    else
    {
        memcpy(&two_otadata[0], result, sizeof(esp_ota_select_entry_t));
        memcpy(&two_otadata[1], result + SPI_FLASH_SEC_SIZE, sizeof(esp_ota_select_entry_t));
        spi_flash_munmap(ota_data_map);
    }
    return otadata_partition;
}

esp_err_t
esp_ota_get_state_partition_patched(const esp_partition_t* partition, esp_ota_img_states_t* ota_state)
{
    if (partition == NULL || ota_state == NULL)
    {
        return ESP_ERR_INVALID_ARG;
    }

    if (!esp_ota_is_ota_partition(partition))
    {
        return ESP_ERR_NOT_SUPPORTED;
    }

    esp_ota_select_entry_t otadata[2];
    int                    ota_app_count = get_ota_partition_count();
    if (read_otadata(otadata) == NULL || ota_app_count == 0)
    {
        return ESP_ERR_NOT_FOUND;
    }

    int  req_ota_slot = partition->subtype - ESP_PARTITION_SUBTYPE_APP_OTA_MIN;
    bool not_found    = true;
    for (int i = 0; i < 2; ++i)
    {
        int ota_slot = (otadata[i].ota_seq - 1) % ota_app_count;
        if (ota_slot == req_ota_slot && otadata[i].crc == bootloader_common_ota_select_crc(&otadata[i]))
        {
            *ota_state = otadata[i].ota_state;
            not_found  = false;
            break;
        }
    }

    if (not_found)
    {
        return ESP_ERR_NOT_FOUND;
    }

    return ESP_OK;
}
