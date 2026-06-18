// Copyright 2015-2017 Espressif Systems (Shanghai) PTE LTD
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
// Local overlay of ESP-IDF v4.2.2 components/fatfs/vfs/vfs_fat_spiflash.c
//
// FIX (Ruuvi): The upstream `fail:` paths of both `esp_vfs_fat_spiflash_mount`
// and `esp_vfs_fat_rawflash_mount` skip the FatFs deregistration step.
// After `f_mount(fs, drv, 1)` has been called, the global `FatFs[vol]` slot
// is set to `fs` and a FreeRTOS sync object is allocated for it (when
// FF_FS_REENTRANT is enabled). The upstream cleanup then frees the
// containing fat_ctx via esp_vfs_fat_unregister_path() but never calls
// `f_mount(NULL, drv, 0)`, leaving `FatFs[vol]` pointing at freed memory
// (and leaking the sync object / wear-levelling handle).
// Any subsequent `f_mount(...)` on the same volume — e.g. retrying as
// raw FAT after a wear-levelled mount failure — will then dereference
// the dangling pointer and assert in vQueueDelete(NULL).
//
// This overlay tracks whether `f_mount` succeeded enough to register
// the volume and, if so, performs a proper `f_mount(NULL, drv, 0)` and
// `wl_unmount()` before releasing the VFS path and the disk-io slot.

#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "esp_log.h"
#include "esp_vfs.h"
#include "esp_vfs_fat.h"
#include "vfs_fat_internal.h"
#include "diskio_impl.h"

#include "diskio_rawflash.h"

#include "wear_levelling.h"
#include "diskio_wl.h"

static const char *TAG = "vfs_fat_spiflash";
esp_err_t esp_vfs_fat_spiflash_mount(const char* base_path,
    const char* partition_label,
    const esp_vfs_fat_mount_config_t* mount_config,
    wl_handle_t* wl_handle)
{
    esp_err_t result = ESP_OK;
    const size_t workbuf_size = 4096;
    void *workbuf = NULL;
    bool wl_mounted = false;
    bool diskio_registered = false;
    bool vfs_registered = false;
    bool fatfs_registered = false; /* f_mount(fs, drv, 1) installed fs into FatFs[vol] */

    esp_partition_subtype_t subtype = partition_label ?
            ESP_PARTITION_SUBTYPE_ANY : ESP_PARTITION_SUBTYPE_DATA_FAT;
    const esp_partition_t *data_partition = esp_partition_find_first(ESP_PARTITION_TYPE_DATA,
                                                subtype, partition_label);
    if (data_partition == NULL) {
        ESP_LOGE(TAG, "Failed to find FATFS partition (type='data', subtype='fat', partition_label='%s'). Check the partition table.", partition_label);
        return ESP_ERR_NOT_FOUND;
    }

    result = wl_mount(data_partition, wl_handle);
    if (result != ESP_OK) {
        ESP_LOGE(TAG, "failed to mount wear levelling layer. result = %i", result);
        return result;
    }
    wl_mounted = true;
    // connect driver to FATFS
    BYTE pdrv = 0xFF;
    if (ff_diskio_get_drive(&pdrv) != ESP_OK) {
        ESP_LOGD(TAG, "the maximum count of volumes is already mounted");
        result = ESP_ERR_NO_MEM;
        goto fail;
    }
    ESP_LOGD(TAG, "using pdrv=%i", pdrv);
    char drv[3] = {(char)('0' + pdrv), ':', 0};

    result = ff_diskio_register_wl_partition(pdrv, *wl_handle);
    if (result != ESP_OK) {
        ESP_LOGE(TAG, "ff_diskio_register_wl_partition failed pdrv=%i, error - 0x(%x)", pdrv, result);
        goto fail;
    }
    diskio_registered = true;

    FATFS *fs = NULL;
    result = esp_vfs_fat_register(base_path, drv, mount_config->max_files, &fs);
    if (result == ESP_ERR_INVALID_STATE) {
        // it's okay, already registered with VFS
        result = ESP_OK;
    } else if (result != ESP_OK) {
        ESP_LOGD(TAG, "esp_vfs_fat_register failed 0x(%x)", result);
        goto fail;
    } else {
        vfs_registered = true;
    }

    // Try to mount partition
    FRESULT fresult = f_mount(fs, drv, 1);
    /* f_mount() installs `fs` into FatFs[vol] and allocates a sync
     * object *before* it tries to read the volume, so we must
     * deregister it on failure regardless of fresult.
     */
    fatfs_registered = true;
    if (fresult != FR_OK) {
        ESP_LOGW(TAG, "f_mount failed (%d)", fresult);
        if (!((fresult == FR_NO_FILESYSTEM || fresult == FR_INT_ERR)
              && mount_config->format_if_mount_failed)) {
            result = ESP_FAIL;
            goto fail;
        }
        workbuf = ff_memalloc(workbuf_size);
        if (workbuf == NULL) {
            result = ESP_ERR_NO_MEM;
            goto fail;
        }
        size_t alloc_unit_size = esp_vfs_fat_get_allocation_unit_size(
                CONFIG_WL_SECTOR_SIZE,
                mount_config->allocation_unit_size);
        ESP_LOGI(TAG, "Formatting FATFS partition, allocation unit size=%d", alloc_unit_size);
        fresult = f_mkfs(drv, FM_ANY | FM_SFD, alloc_unit_size, workbuf, workbuf_size);
        if (fresult != FR_OK) {
            result = ESP_FAIL;
            ESP_LOGE(TAG, "f_mkfs failed (%d)", fresult);
            goto fail;
        }
        free(workbuf);
        workbuf = NULL;
        ESP_LOGI(TAG, "Mounting again");
        fresult = f_mount(fs, drv, 0);
        if (fresult != FR_OK) {
            result = ESP_FAIL;
            ESP_LOGE(TAG, "f_mount failed after formatting (%d)", fresult);
            goto fail;
        }
    }
    return ESP_OK;

fail:
    free(workbuf);
    /* Order matters here: f_mount(NULL, ...) must be called *before*
     * esp_vfs_fat_unregister_path() because the latter frees the
     * fat_ctx that owns the FATFS struct pointed to by FatFs[vol].
     */
    if (fatfs_registered) {
        char drv_cleanup[3] = {(char)('0' + pdrv), ':', 0};
        f_mount(NULL, drv_cleanup, 0);
    }
    if (vfs_registered) {
        esp_vfs_fat_unregister_path(base_path);
    }
    if (diskio_registered) {
        ff_diskio_clear_pdrv_wl(*wl_handle);
        ff_diskio_unregister(pdrv);
    }
    if (wl_mounted) {
        wl_unmount(*wl_handle);
        *wl_handle = WL_INVALID_HANDLE;
    }
    return result;
}

esp_err_t esp_vfs_fat_spiflash_unmount(const char *base_path, wl_handle_t wl_handle)
{
    BYTE pdrv = ff_diskio_get_pdrv_wl(wl_handle);
    if (pdrv == 0xff) {
        return ESP_ERR_INVALID_STATE;
    }
    char drv[3] = {(char)('0' + pdrv), ':', 0};

    f_mount(0, drv, 0);
    ff_diskio_unregister(pdrv);
    ff_diskio_clear_pdrv_wl(wl_handle);
    // release partition driver
    esp_err_t err_drv = wl_unmount(wl_handle);
    esp_err_t err = esp_vfs_fat_unregister_path(base_path);
    if (err == ESP_OK) err = err_drv;
    return err;
}

esp_err_t esp_vfs_fat_rawflash_mount(const char* base_path,
    const char* partition_label,
    const esp_vfs_fat_mount_config_t* mount_config)
{
    esp_err_t result = ESP_OK;
    bool diskio_registered = false;
    bool vfs_registered = false;
    bool fatfs_registered = false;

    const esp_partition_t *data_partition = esp_partition_find_first(ESP_PARTITION_TYPE_DATA,
            ESP_PARTITION_SUBTYPE_DATA_FAT, partition_label);
    if (data_partition == NULL) {
        ESP_LOGE(TAG, "Failed to find FATFS partition (type='data', subtype='fat', partition_label='%s'). Check the partition table.", partition_label);
        return ESP_ERR_NOT_FOUND;
    }

    // connect driver to FATFS
    BYTE pdrv = 0xFF;
    if (ff_diskio_get_drive(&pdrv) != ESP_OK) {
        ESP_LOGD(TAG, "the maximum count of volumes is already mounted");
        return ESP_ERR_NO_MEM;
    }
    ESP_LOGD(TAG, "using pdrv=%i", pdrv);
    char drv[3] = {(char)('0' + pdrv), ':', 0};

    result = ff_diskio_register_raw_partition(pdrv, data_partition);
    if (result != ESP_OK) {
        ESP_LOGE(TAG, "ff_diskio_register_raw_partition failed pdrv=%i, error - 0x(%x)", pdrv, result);
        goto fail;
    }
    diskio_registered = true;

    FATFS *fs = NULL;
    result = esp_vfs_fat_register(base_path, drv, mount_config->max_files, &fs);
    if (result == ESP_ERR_INVALID_STATE) {
        // it's okay, already registered with VFS
        result = ESP_OK;
    } else if (result != ESP_OK) {
        ESP_LOGD(TAG, "esp_vfs_fat_register failed 0x(%x)", result);
        goto fail;
    } else {
        vfs_registered = true;
    }

    // Try to mount partition
    FRESULT fresult = f_mount(fs, drv, 1);
    fatfs_registered = true; /* see comment in spiflash_mount above */
    if (fresult != FR_OK) {
        ESP_LOGW(TAG, "f_mount failed (%d)", fresult);
        result = ESP_FAIL;
        goto fail;
    }
    return ESP_OK;

fail:
    if (fatfs_registered) {
        char drv_cleanup[3] = {(char)('0' + pdrv), ':', 0};
        f_mount(NULL, drv_cleanup, 0);
    }
    if (vfs_registered) {
        esp_vfs_fat_unregister_path(base_path);
    }
    if (diskio_registered) {
        ff_diskio_unregister(pdrv);
    }
    return result;
}


esp_err_t esp_vfs_fat_rawflash_unmount(const char *base_path, const char* partition_label)
{
    const esp_partition_t *data_partition = esp_partition_find_first(ESP_PARTITION_TYPE_DATA,
            ESP_PARTITION_SUBTYPE_DATA_FAT, partition_label);

    if (data_partition == NULL) {
        ESP_LOGE(TAG, "Failed to find FATFS partition (type='data', subtype='fat', partition_label='%s'). Check the partition table.", partition_label);
        return ESP_ERR_NOT_FOUND;
    }

    BYTE pdrv = ff_diskio_get_pdrv_raw(data_partition);
    if (pdrv == 0xff) {
        return ESP_ERR_INVALID_STATE;
    }
    char drv[3] = {(char)('0' + pdrv), ':', 0};

    f_mount(0, drv, 0);
    ff_diskio_unregister(pdrv);
    esp_err_t err = esp_vfs_fat_unregister_path(base_path);
    return err;
}

