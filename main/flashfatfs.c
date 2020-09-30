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

static const char *TAG = "FlashFatFS";

struct FlashFatFs_Tag
{
    char *      mount_point;
    wl_handle_t wl_handle;
};

FlashFatFs_t *
flashfatfs_mount(const char *mount_point, const char *partition_label, const int max_files)
{
    const char *mount_point_prefix = "";
#if RUUVI_TESTS_NRF52FW
    mount_point_prefix = (mount_point[0] == '/') ? "." : "";
#endif
    size_t        mount_point_buf_size = strlen(mount_point_prefix) + strlen(mount_point) + 1;
    FlashFatFs_t *pObj                 = calloc(1, sizeof(*pObj) + mount_point_buf_size);
    if (NULL == pObj)
    {
        ESP_LOGE(TAG, "%s: Can't allocate memory", __func__);
        return NULL;
    }
    pObj->mount_point = (void *)(pObj + 1);
    snprintf(pObj->mount_point, mount_point_buf_size, "%s%s", mount_point_prefix, mount_point);

    esp_vfs_fat_sdmmc_mount_config_t mount_config = {
        .format_if_mount_failed = false,
        .max_files              = max_files,
    };
    {
        const esp_err_t err = esp_vfs_fat_spiflash_mount(mount_point, partition_label, &mount_config, &pObj->wl_handle);
        if (ESP_OK != err)
        {
            ESP_LOGE(TAG, "%s: %s failed, err=%d", __func__, "esp_vfs_fat_spiflash_mount", err);
            free(pObj);
            return NULL;
        }
    }
    ESP_LOGI(TAG, "Partition '%s' mounted successfully to %s", partition_label, mount_point);
    return pObj;
}

bool
flashfatfs_unmount(FlashFatFs_t *p_ffs)
{
    ESP_LOGI(TAG, "Unmount %s", p_ffs->mount_point);
    const esp_err_t err = esp_vfs_fat_spiflash_unmount(p_ffs->mount_point, p_ffs->wl_handle);
    free(p_ffs);
    if (ESP_OK != err)
    {
        ESP_LOGE(TAG, "%s: %s failed, err=%d", __func__, "esp_vfs_fat_spiflash_unmount", err);
        return false;
    }
    return true;
}

int
flashfatfs_open(FlashFatFs_t *p_ffs, const char *file_path)
{
    char tmp_path[80];
    snprintf(tmp_path, sizeof(tmp_path), "%s/%s", p_ffs->mount_point, file_path);
    int fd = open(tmp_path, O_RDONLY);
    if (fd < 0)
    {
        ESP_LOGE(TAG, "Can't open: %s", tmp_path);
        return -1;
    }
    return fd;
}

FILE *
flashfatfs_fopen(FlashFatFs_t *p_ffs, const char *file_path)
{
    char tmp_path[80];
    snprintf(tmp_path, sizeof(tmp_path), "%s/%s", p_ffs->mount_point, file_path);
    FILE *fd = fopen(tmp_path, "r");
    if (NULL == fd)
    {
        ESP_LOGE(TAG, "Can't open: %s", tmp_path);
        return NULL;
    }
    return fd;
}
