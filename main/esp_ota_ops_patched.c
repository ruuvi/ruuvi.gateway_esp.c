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
#include <inttypes.h>
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
#include "log.h"

#define ESP_OTA_FLASH_ENCRYPTION_MIN_CHUNK_SIZE (16U)
#define ESP_OTA_FLASH_ENCRYPTION_FILL           (0xFFU)

#define ESP_OTA_NUM_OTA_SLOTS (2U)

#define SUB_TYPE_ID(i) (i & 0x0F)

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
static bool
is_ota_partition(const esp_partition_t* p)
{
    return (
        (NULL != p) && (ESP_PARTITION_TYPE_APP == p->type) && (p->subtype >= ESP_PARTITION_SUBTYPE_APP_OTA_0)
        && (p->subtype < ESP_PARTITION_SUBTYPE_APP_OTA_MAX));
}

bool
esp_ota_is_ota_partition(const esp_partition_t* p)
{
    return is_ota_partition(p);
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

static esp_err_t
image_validate(const esp_partition_t* partition, esp_image_load_mode_t load_mode)
{
    esp_image_metadata_t      data;
    const esp_partition_pos_t part_pos = {
        .offset = partition->address,
        .size   = partition->size,
    };

    if (esp_image_verify(load_mode, &part_pos, &data) != ESP_OK)
    {
        return ESP_ERR_OTA_VALIDATE_FAILED;
    }

    return ESP_OK;
}

static esp_ota_img_states_t
set_new_state_otadata(void)
{
#ifdef CONFIG_BOOTLOADER_APP_ROLLBACK_ENABLE
    ESP_LOGD(TAG, "Monitoring the first boot of the app is enabled.");
    return ESP_OTA_IMG_NEW;
#else
    return ESP_OTA_IMG_UNDEFINED;
#endif
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

    if (!is_ota_partition(p_partition_verified))
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
    if ((ESP_OK == esp_ota_get_state_partition_patched(p_running_partition, &ota_state_running_part))
        && (ESP_OTA_IMG_PENDING_VERIFY == ota_state_running_part))
    {
        LOG_ERR("Running app has not confirmed state (ESP_OTA_IMG_PENDING_VERIFY)");
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
        LOG_ERR("OTA image has invalid magic byte (expected 0xE9, saw 0x%02x)", p_data_bytes[0]);
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
        LOG_ERR("write p_data is invalid");
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
    LOG_ERR("not found the handle");
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
                    LOG_ERR(
                        "Image verify failed for partition at address 0x%08x, size 0x%08x",
                        part_pos.offset,
                        part_pos.size);
                    ret = ESP_ERR_OTA_VALIDATE_FAILED;
                }
                else
                {
                    if (!esp_ota_helper_calc_pub_key_digest_for_app_image(p_metadata, p_pub_key_digest))
                    {
                        LOG_ERR(
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

static esp_err_t
rewrite_ota_seq(
    esp_ota_select_entry_t* two_otadata,
    uint32_t                seq,
    uint8_t                 sec_id,
    const esp_partition_t*  ota_data_partition)
{
    if (two_otadata == NULL || sec_id > 1)
    {
        return ESP_ERR_INVALID_ARG;
    }

    two_otadata[sec_id].ota_seq = seq;
    two_otadata[sec_id].crc     = bootloader_common_ota_select_crc(&two_otadata[sec_id]);
    esp_err_t ret = esp_partition_erase_range(ota_data_partition, sec_id * SPI_FLASH_SEC_SIZE, SPI_FLASH_SEC_SIZE);
    if (ret != ESP_OK)
    {
        return ret;
    }
    else
    {
        return esp_partition_write(
            ota_data_partition,
            SPI_FLASH_SEC_SIZE * sec_id,
            &two_otadata[sec_id],
            sizeof(esp_ota_select_entry_t));
    }
}

static uint8_t
get_ota_partition_count(void)
{
    uint8_t ota_app_count = 0;
    while (esp_partition_find_first(ESP_PARTITION_TYPE_APP, ESP_PARTITION_SUBTYPE_APP_OTA_MIN + ota_app_count, NULL)
           != NULL)
    {
        const uint32_t max_ota_app_count = ESP_PARTITION_SUBTYPE_APP_OTA_MAX - ESP_PARTITION_SUBTYPE_APP_OTA_MIN;
        assert(ota_app_count < max_ota_app_count && "must erase the partition before writing to it");
        ota_app_count++;
    }
    return ota_app_count;
}

static esp_err_t
esp_rewrite_ota_data(esp_partition_subtype_t subtype)
{
    esp_ota_select_entry_t otadata[2];
    const esp_partition_t* otadata_partition = read_otadata(otadata);
    if (otadata_partition == NULL)
    {
        return ESP_ERR_NOT_FOUND;
    }

    int ota_app_count = get_ota_partition_count();
    if (SUB_TYPE_ID(subtype) >= ota_app_count)
    {
        return ESP_ERR_INVALID_ARG;
    }

    // esp32_idf use two sector for store information about which partition is running
    // it defined the two sector as ota data partition,two structure esp_ota_select_entry_t is saved in the two sector
    // named data in first sector as otadata[0], second sector data as otadata[1]
    // e.g.
    // if otadata[0].ota_seq == otadata[1].ota_seq == 0xFFFFFFFF,means ota info partition is in init status
    // so it will boot factory application(if there is),if there's no factory application,it will boot ota[0]
    // application if otadata[0].ota_seq != 0 and otadata[1].ota_seq != 0,it will choose a max seq ,and get value of
    // max_seq%max_ota_app_number and boot a subtype (mask 0x0F) value is (max_seq - 1)%max_ota_app_number,so if want
    // switch to run ota[x],can use next formulas. for example, if otadata[0].ota_seq = 4, otadata[1].ota_seq = 5, and
    // there are 8 ota application, current running is (5-1)%8 = 4,running ota[4],so if we want to switch to run ota[7],
    // we should add otadata[0].ota_seq (is 4) to 4 ,(8-1)%8=7,then it will boot ota[7]
    // if      A=(B - C)%D
    // then    B=(A + C)%D + D*n ,n= (0,1,2...)
    // so current ota app sub type id is x , dest bin subtype is y,total ota app count is n
    // seq will add (x + n*1 + 1 - seq)%n

    int active_otadata = bootloader_common_get_active_otadata(otadata);
    if (active_otadata != -1)
    {
        uint32_t seq = otadata[active_otadata].ota_seq;
        uint32_t i   = 0;
        while (seq > (SUB_TYPE_ID(subtype) + 1) % ota_app_count + i * ota_app_count)
        {
            i++;
        }
        int next_otadata                = (~active_otadata) & 1; // if 0 -> will be next 1. and if 1 -> will be next 0.
        otadata[next_otadata].ota_state = set_new_state_otadata();
        return rewrite_ota_seq(
            otadata,
            (SUB_TYPE_ID(subtype) + 1) % ota_app_count + i * ota_app_count,
            next_otadata,
            otadata_partition);
    }
    else
    {
        /* Both OTA slots are invalid, probably because unformatted... */
        int next_otadata                = 0;
        otadata[next_otadata].ota_state = set_new_state_otadata();
        return rewrite_ota_seq(otadata, SUB_TYPE_ID(subtype) + 1, next_otadata, otadata_partition);
    }
}

esp_err_t
esp_ota_set_boot_partition_patched(const esp_partition_t* partition)
{
    if (partition == NULL)
    {
        return ESP_ERR_INVALID_ARG;
    }

    if (image_validate(partition, ESP_IMAGE_VERIFY) != ESP_OK)
    {
        return ESP_ERR_OTA_VALIDATE_FAILED;
    }

    // if set boot partition to factory bin ,just format ota info partition
    if (partition->type == ESP_PARTITION_TYPE_APP)
    {
        if (partition->subtype == ESP_PARTITION_SUBTYPE_APP_FACTORY)
        {
            const esp_partition_t* find_partition = esp_partition_find_first(
                ESP_PARTITION_TYPE_DATA,
                ESP_PARTITION_SUBTYPE_DATA_OTA,
                NULL);
            if (find_partition != NULL)
            {
                return esp_partition_erase_range(find_partition, 0, find_partition->size);
            }
            else
            {
                return ESP_ERR_NOT_FOUND;
            }
        }
        else
        {
#ifdef CONFIG_BOOTLOADER_APP_ANTI_ROLLBACK
            esp_app_desc_t partition_app_desc;
            esp_err_t      err = esp_ota_get_partition_description(partition, &partition_app_desc);
            if (err != ESP_OK)
            {
                return err;
            }

            if (esp_efuse_check_secure_version(partition_app_desc.secure_version) == false)
            {
                ESP_LOGE(
                    TAG,
                    "This a new partition can not be booted due to a secure version is lower than stored in efuse. "
                    "Partition will be erased.");
                esp_err_t err = esp_partition_erase_range(partition, 0, partition->size);
                if (err != ESP_OK)
                {
                    return err;
                }
                return ESP_ERR_OTA_SMALL_SEC_VER;
            }
#endif
            return esp_rewrite_ota_data(partition->subtype);
        }
    }
    else
    {
        return ESP_ERR_INVALID_ARG;
    }
}

static const esp_partition_t*
read_two_ota_data(esp_ota_select_entry_t* const p_two_ota_data)
{
    const esp_partition_t* const p_ota_data_partition = esp_partition_find_first(
        ESP_PARTITION_TYPE_DATA,
        ESP_PARTITION_SUBTYPE_DATA_OTA,
        NULL);

    if (p_ota_data_partition == NULL)
    {
        LOG_ERR("Partition with OTA data not found");
        return NULL;
    }

    spi_flash_mmap_handle_t ota_data_map = 0;
    const void*             p_result     = NULL;

    const esp_err_t err = esp_partition_mmap(
        p_ota_data_partition,
        0,
        p_ota_data_partition->size,
        SPI_FLASH_MMAP_DATA,
        &p_result,
        &ota_data_map);
    if (ESP_OK != err)
    {
        LOG_ERR("mmap OTA-data failed. Err=%" PRId32, err);
        return NULL;
    }
    if (NULL == p_result)
    {
        LOG_ERR("mmap OTA-data returned NULL pointer");
        spi_flash_munmap(ota_data_map);
        return NULL;
    }
    memcpy(&p_two_ota_data[0], p_result, sizeof(esp_ota_select_entry_t));
    memcpy(&p_two_ota_data[1], (const uint8_t*)p_result + SPI_FLASH_SEC_SIZE, sizeof(esp_ota_select_entry_t));

    spi_flash_munmap(ota_data_map);
    return p_ota_data_partition;
}

static int32_t
esp_ota_calc_slot_from_seq(const uint32_t ota_seq, const uint8_t ota_app_count)
{
    if (0U == ota_app_count)
    {
        return -1;
    }
    if ((UINT32_MAX == ota_seq) || (0 == ota_seq))
    {
        return -1;
    }
    return (int32_t)((ota_seq - 1U) % (uint32_t)ota_app_count);
}

esp_err_t
esp_ota_get_state_partition_patched(const esp_partition_t* const p_partition, esp_ota_img_states_t* const p_ota_state)
{
    if ((NULL == p_partition) || (NULL == p_ota_state))
    {
        return ESP_ERR_INVALID_ARG;
    }

    if (!is_ota_partition(p_partition))
    {
        return ESP_ERR_NOT_SUPPORTED;
    }

    const uint8_t ota_app_count = get_ota_partition_count();
    if (0 == ota_app_count)
    {
        LOG_ERR("There is no OTA app partition");
        return ESP_ERR_NOT_FOUND;
    }
    LOG_INFO("### Found %" PRIu8 " OTA app partitions", ota_app_count);

    esp_ota_select_entry_t ota_data[ESP_OTA_NUM_OTA_SLOTS];
    if (NULL == read_two_ota_data(ota_data))
    {
        LOG_ERR("Failed to read OTA data");
        return ESP_ERR_NOT_FOUND;
    }

    LOG_DUMP_INFO(
        (const uint8_t*)&ota_data[0],
        sizeof(esp_ota_select_entry_t),
        "### otadata[0]: ota_slot %" PRIi32,
        esp_ota_calc_slot_from_seq(ota_data[0].ota_seq, ota_app_count));
    LOG_DUMP_INFO(
        (const uint8_t*)&ota_data[1],
        sizeof(esp_ota_select_entry_t),
        "### otadata[1]: ota_slot %" PRIi32,
        esp_ota_calc_slot_from_seq(ota_data[1].ota_seq, ota_app_count));

    const int32_t req_ota_slot = (int32_t)(p_partition->subtype - ESP_PARTITION_SUBTYPE_APP_OTA_MIN);
    LOG_INFO("### Requested OTA slot %" PRIi32, req_ota_slot);
    bool is_slot_found = false;
    for (uint32_t i = 0; i < ESP_OTA_NUM_OTA_SLOTS; ++i)
    {
        const int32_t ota_slot = esp_ota_calc_slot_from_seq(ota_data[i].ota_seq, ota_app_count);
        if (ota_slot == req_ota_slot)
        {
            // Write ota_state before checking CRC,
            // so that the caller will be able to log the OTA state even if CRC check fails.
            *p_ota_state = ota_data[i].ota_state;

            is_slot_found = true;

            const uint32_t actual_crc = bootloader_common_ota_select_crc(&ota_data[i]);
            if (ota_data[i].crc != actual_crc)
            {
                LOG_ERR(
                    "[idx=%" PRIu32 "] CRC mismatch for OTA slot %" PRIi32 ": ota_seq=0x%08" PRIx32
                    ", ota_state=0x%08" PRIx32 ", crc=0x%08" PRIx32 ", actual crc=0x%08" PRIx32,
                    i,
                    req_ota_slot,
                    ota_data[i].ota_seq,
                    ota_data[i].ota_state,
                    ota_data[i].crc,
                    actual_crc);
                continue;
            }
            return ESP_OK;
        }
    }
    if (is_slot_found)
    {
        LOG_ERR("There is no OTA slot %" PRIi32 " with valid CRC", req_ota_slot);
        return ESP_ERR_INVALID_CRC;
    }
    LOG_ERR("OTA slot %" PRIi32 " not found", req_ota_slot);
    return ESP_ERR_NOT_FOUND;
}
