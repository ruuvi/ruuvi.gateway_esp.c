/**
 * @file flashfatfs.c
 * @author TheSomeMan
 * @date 2020-09-14
 * @copyright Ruuvi Innovations Ltd, license BSD-3-Clause.
 */

#include "flashfatfs.h"
#include <string.h>
#include <stdlib.h>
#include <fcntl.h>
#include "esp_err.h"
#include "esp_log.h"
#include "esp_vfs_fat.h"
#include "wear_levelling.h"
#include "app_malloc.h"

#if !defined(RUUVI_TESTS_NRF52FW)
#define RUUVI_TESTS_NRF52FW (0)
#endif

#if !defined(RUUVI_TESTS_FLASHFATFS)
#define RUUVI_TESTS_FLASHFATFS (0)
#endif

static const char *TAG = "FlashFatFS";

struct flash_fat_fs_t
{
    char *      mount_point;
    wl_handle_t wl_handle;
};

flash_fat_fs_t *
flashfatfs_mount(const char *mount_point, const char *partition_label, const flash_fat_fs_num_files_t max_files)
{
    const char *mount_point_prefix = "";
#if RUUVI_TESTS_NRF52FW || RUUVI_TESTS_FLASHFATFS
    mount_point_prefix = (mount_point[0] == '/') ? "." : "";
#endif
    size_t mount_point_buf_size = strlen(mount_point_prefix) + strlen(mount_point) + 1;

    flash_fat_fs_t *p_obj = app_calloc(1, sizeof(*p_obj) + mount_point_buf_size);
    if (NULL == p_obj)
    {
        ESP_LOGE(TAG, "%s: Can't allocate memory", __func__);
    }
    else
    {
        p_obj->mount_point = (void *)(p_obj + 1);
        snprintf(p_obj->mount_point, mount_point_buf_size, "%s%s", mount_point_prefix, mount_point);

        esp_vfs_fat_sdmmc_mount_config_t mount_config = {
            .format_if_mount_failed = false,
            .max_files              = max_files,
            .allocation_unit_size   = 512U,
        };
        const esp_err_t err = esp_vfs_fat_spiflash_mount(
            mount_point,
            partition_label,
            &mount_config,
            &p_obj->wl_handle);
        if (ESP_OK != err)
        {
            ESP_LOGE(TAG, "%s: %s failed, err=%d", __func__, "esp_vfs_fat_spiflash_mount", err);
            app_free_pptr((void **)&p_obj);
        }
        else
        {
            ESP_LOGI(TAG, "Partition '%s' mounted successfully to %s", partition_label, mount_point);
        }
    }
    return p_obj;
}

bool
flashfatfs_unmount(flash_fat_fs_t *p_ffs)
{
    bool result = false;
    ESP_LOGI(TAG, "Unmount %s", p_ffs->mount_point);
    const esp_err_t err = esp_vfs_fat_spiflash_unmount(p_ffs->mount_point, p_ffs->wl_handle);
    app_free_const_pptr((const void **)pp_ffs);
    *pp_ffs = NULL;
    if (ESP_OK != err)
    {
        ESP_LOGE(TAG, "%s: %s failed, err=%d", __func__, "esp_vfs_fat_spiflash_unmount", err);
    }
    else
    {
        result = true;
    }
    return result;
}

file_descriptor_t
flashfatfs_open(flash_fat_fs_t *p_ffs, const char *file_path)
{
    char tmp_path[80];
    snprintf(tmp_path, sizeof(tmp_path), "%s/%s", p_ffs->mount_point, file_path);
    file_descriptor_t fd = open(tmp_path, O_RDONLY);
    if (fd < 0)
    {
        ESP_LOGE(TAG, "Can't open: %s", tmp_path);
    }
    return fd;
}

FILE *
flashfatfs_fopen(flash_fat_fs_t *p_ffs, const char *file_path, const bool flag_use_binary_mode)
{
    char tmp_path[80];
    snprintf(tmp_path, sizeof(tmp_path), "%s/%s", p_ffs->mount_point, file_path);
    const char *mode = flag_use_binary_mode ? "rb" : "r";

    FILE *fd = fopen(tmp_path, mode);
    if (NULL == fd)
    {
        ESP_LOGE(TAG, "Can't open: %s", tmp_path);
    }
    return fd;
}
