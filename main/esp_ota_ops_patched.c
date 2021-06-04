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

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include "esp_err.h"
#include "esp_partition.h"
#include "esp_spi_flash.h"
#include "esp_image_format.h"
#include "esp_secure_boot.h"
#include "esp_flash_encrypt.h"
#include "esp_spi_flash.h"
#include "sdkconfig.h"

#include "esp_ota_ops.h"
#include "sys/queue.h"
#include "esp32/rom/crc.h"
#include "esp_log.h"
#include "esp_flash_partitions.h"
#include "bootloader_common.h"
#include "sys/param.h"
#include "esp_system.h"
#include "esp_efuse.h"

#define SUB_TYPE_ID(i) (i & 0x0F)

typedef struct ota_ops_entry_
{
    uint32_t               handle;
    const esp_partition_t *part;
    uint32_t               erased_size;
    uint32_t               wrote_size;
    uint8_t                partial_bytes;
    uint8_t                partial_data[16];
    LIST_ENTRY(ota_ops_entry_) entries;
} ota_ops_entry_t;

static LIST_HEAD(ota_ops_entries_head, ota_ops_entry_)
    s_ota_ops_entries_head = LIST_HEAD_INITIALIZER(s_ota_ops_entries_head);

static uint32_t s_ota_ops_last_handle = 0;

const static char *TAG = "esp_ota_ops";

/* Return true if this is an OTA app partition */
static bool
is_ota_partition(const esp_partition_t *p)
{
    return (
        p != NULL && p->type == ESP_PARTITION_TYPE_APP && p->subtype >= ESP_PARTITION_SUBTYPE_APP_OTA_0
        && p->subtype < ESP_PARTITION_SUBTYPE_APP_OTA_MAX);
}

esp_err_t
esp_ota_begin_patched(const esp_partition_t *partition, esp_ota_handle_t *out_handle)
{
    ota_ops_entry_t *new_entry;
    esp_err_t        ret = ESP_OK;

    if ((partition == NULL) || (out_handle == NULL))
    {
        return ESP_ERR_INVALID_ARG;
    }

    partition = esp_partition_verify(partition);
    if (partition == NULL)
    {
        return ESP_ERR_NOT_FOUND;
    }

    if (!is_ota_partition(partition))
    {
        return ESP_ERR_INVALID_ARG;
    }

    const esp_partition_t *running_partition = esp_ota_get_running_partition();
    if (partition == running_partition)
    {
        return ESP_ERR_OTA_PARTITION_CONFLICT;
    }

#ifdef CONFIG_BOOTLOADER_APP_ROLLBACK_ENABLE
    esp_ota_img_states_t ota_state_running_part;
    if (esp_ota_get_state_partition(running_partition, &ota_state_running_part) == ESP_OK)
    {
        if (ota_state_running_part == ESP_OTA_IMG_PENDING_VERIFY)
        {
            ESP_LOGE(TAG, "Running app has not confirmed state (ESP_OTA_IMG_PENDING_VERIFY)");
            return ESP_ERR_OTA_ROLLBACK_INVALID_STATE;
        }
    }
#endif

    extern esp_err_t erase_partition_with_sleep(const esp_partition_t *const p_partition);

    ret = erase_partition_with_sleep(partition);

    if (ret != ESP_OK)
    {
        return ret;
    }

    new_entry = (ota_ops_entry_t *)calloc(sizeof(ota_ops_entry_t), 1);
    if (new_entry == NULL)
    {
        return ESP_ERR_NO_MEM;
    }

    LIST_INSERT_HEAD(&s_ota_ops_entries_head, new_entry, entries);

    new_entry->erased_size = partition->size;

    new_entry->part   = partition;
    new_entry->handle = ++s_ota_ops_last_handle;
    *out_handle       = new_entry->handle;
    return ESP_OK;
}

esp_err_t
esp_ota_write_patched(esp_ota_handle_t handle, const void *data, size_t size)
{
    const uint8_t *  data_bytes = (const uint8_t *)data;
    esp_err_t        ret;
    ota_ops_entry_t *it;

    if (data == NULL)
    {
        ESP_LOGE(TAG, "write data is invalid");
        return ESP_ERR_INVALID_ARG;
    }

    // find ota handle in linked list
    for (it = LIST_FIRST(&s_ota_ops_entries_head); it != NULL; it = LIST_NEXT(it, entries))
    {
        if (it->handle == handle)
        {
            // must erase the partition before writing to it
            assert(it->erased_size > 0 && "must erase the partition before writing to it");
            if (it->wrote_size == 0 && it->partial_bytes == 0 && size > 0 && data_bytes[0] != ESP_IMAGE_HEADER_MAGIC)
            {
                ESP_LOGE(TAG, "OTA image has invalid magic byte (expected 0xE9, saw 0x%02x)", data_bytes[0]);
                return ESP_ERR_OTA_VALIDATE_FAILED;
            }

            if (esp_flash_encryption_enabled())
            {
                /* Can only write 16 byte blocks to flash, so need to cache anything else */
                size_t copy_len;

                /* check if we have partially written data from earlier */
                if (it->partial_bytes != 0)
                {
                    copy_len = MIN(16 - it->partial_bytes, size);
                    memcpy(it->partial_data + it->partial_bytes, data_bytes, copy_len);
                    it->partial_bytes += copy_len;
                    if (it->partial_bytes != 16)
                    {
                        return ESP_OK; /* nothing to write yet, just filling buffer */
                    }
                    /* write 16 byte to partition */
                    ret = esp_partition_write(it->part, it->wrote_size, it->partial_data, 16);
                    if (ret != ESP_OK)
                    {
                        return ret;
                    }
                    it->partial_bytes = 0;
                    memset(it->partial_data, 0xFF, 16);
                    it->wrote_size += 16;
                    data_bytes += copy_len;
                    size -= copy_len;
                }

                /* check if we need to save trailing data that we're about to write */
                it->partial_bytes = size % 16;
                if (it->partial_bytes != 0)
                {
                    size -= it->partial_bytes;
                    memcpy(it->partial_data, data_bytes + size, it->partial_bytes);
                }
            }

            ret = esp_partition_write(it->part, it->wrote_size, data_bytes, size);
            if (ret == ESP_OK)
            {
                it->wrote_size += size;
            }
            return ret;
        }
    }

    // if go to here ,means don't find the handle
    ESP_LOGE(TAG, "not found the handle");
    return ESP_ERR_INVALID_ARG;
}

esp_err_t
esp_ota_end_patched(esp_ota_handle_t handle)
{
    ota_ops_entry_t *it;
    esp_err_t        ret = ESP_OK;

    for (it = LIST_FIRST(&s_ota_ops_entries_head); it != NULL; it = LIST_NEXT(it, entries))
    {
        if (it->handle == handle)
        {
            break;
        }
    }

    if (it == NULL)
    {
        return ESP_ERR_NOT_FOUND;
    }

    /* 'it' holds the ota_ops_entry_t for 'handle' */

    // esp_ota_end() is only valid if some data was written to this handle
    if ((it->erased_size == 0) || (it->wrote_size == 0))
    {
        ret = ESP_ERR_INVALID_ARG;
        goto cleanup;
    }

    if (it->partial_bytes > 0)
    {
        /* Write out last 16 bytes, if necessary */
        ret = esp_partition_write(it->part, it->wrote_size, it->partial_data, 16);
        if (ret != ESP_OK)
        {
            ret = ESP_ERR_INVALID_STATE;
            goto cleanup;
        }
        it->wrote_size += 16;
        it->partial_bytes = 0;
    }

    esp_image_metadata_t      data;
    const esp_partition_pos_t part_pos = {
        .offset = it->part->address,
        .size   = it->part->size,
    };

    if (esp_image_verify(ESP_IMAGE_VERIFY, &part_pos, &data) != ESP_OK)
    {
        ret = ESP_ERR_OTA_VALIDATE_FAILED;
        goto cleanup;
    }

cleanup:
    LIST_REMOVE(it, entries);
    free(it);
    return ret;
}
