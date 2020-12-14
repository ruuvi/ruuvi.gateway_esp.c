/**
 * @file flashfatfs.c
 * @author TheSomeMan
 * @date 2020-09-14
 * @copyright Ruuvi Innovations Ltd, license BSD-3-Clause.
 */

#include "flashfatfs.h"
#define LOG_LOCAL_LEVEL LOG_LEVEL_DEBUG
#include "log.h"
#include <string.h>
#include <stdlib.h>
#include <fcntl.h>
#include "esp_err.h"
#include "esp_vfs_fat.h"
#include "wear_levelling.h"
#include "os_malloc.h"

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

const flash_fat_fs_t *
flashfatfs_mount(const char *mount_point, const char *partition_label, const flash_fat_fs_num_files_t max_files)
{
    const char *mount_point_prefix = "";
#if RUUVI_TESTS_NRF52FW || RUUVI_TESTS_FLASHFATFS
    mount_point_prefix = (mount_point[0] == '/') ? "." : "";
#endif
    size_t mount_point_buf_size = strlen(mount_point_prefix) + strlen(mount_point) + 1;

    LOG_DBG("Mount partition '%s' to the mount point %s", partition_label, mount_point);
    flash_fat_fs_t *p_obj = os_calloc(1, sizeof(*p_obj) + mount_point_buf_size);
    if (NULL == p_obj)
    {
        LOG_ERR("Can't allocate memory");
        return NULL;
    }
    p_obj->mount_point = (void *)(p_obj + 1);
    snprintf(p_obj->mount_point, mount_point_buf_size, "%s%s", mount_point_prefix, mount_point);

    esp_vfs_fat_sdmmc_mount_config_t mount_config = {
        .format_if_mount_failed = false,
        .max_files              = max_files,
        .allocation_unit_size   = 512U,
    };
    const esp_err_t err = esp_vfs_fat_spiflash_mount(mount_point, partition_label, &mount_config, &p_obj->wl_handle);
    if (ESP_OK != err)
    {
        LOG_ERR_ESP(err, "%s failed", "esp_vfs_fat_spiflash_mount");
        os_free(p_obj);
        return NULL;
    }
    LOG_INFO("Partition '%s' mounted successfully to %s", partition_label, mount_point);
    return p_obj;
}

bool
flashfatfs_unmount(const flash_fat_fs_t **pp_ffs)
{
    const flash_fat_fs_t *p_ffs = *pp_ffs;
    LOG_INFO("Unmount %s", p_ffs->mount_point);
    const esp_err_t err = esp_vfs_fat_spiflash_unmount(p_ffs->mount_point, p_ffs->wl_handle);
    os_free(p_ffs);
    *pp_ffs = NULL;
    if (ESP_OK != err)
    {
        LOG_ERR_ESP(err, "%s failed", "esp_vfs_fat_spiflash_unmount");
        return false;
    }
    return true;
}

FLASHFATFS_CB_STATIC
flashfatfs_path_t
flashfatfs_get_full_path(const flash_fat_fs_t *p_ffs, const char *file_path)
{
    const char *      mount_point = (NULL != p_ffs) ? p_ffs->mount_point : "";
    flashfatfs_path_t tmp_path    = { '\0' };
    snprintf(tmp_path.buf, sizeof(tmp_path.buf), "%s/%s", mount_point, file_path);
    return tmp_path;
}

file_descriptor_t
flashfatfs_open(const flash_fat_fs_t *p_ffs, const char *file_path)
{
    if (NULL == p_ffs)
    {
        LOG_ERR("p_ffs is NULL");
        return FILE_DESCRIPTOR_INVALID;
    }
    const flashfatfs_path_t tmp_path = flashfatfs_get_full_path(p_ffs, file_path);
    file_descriptor_t       fd       = open(tmp_path.buf, O_RDONLY);
    if (fd < 0)
    {
        LOG_ERR("Can't open: %s", tmp_path.buf);
    }
    return fd;
}

FILE *
flashfatfs_fopen(const flash_fat_fs_t *p_ffs, const char *file_path, const bool flag_use_binary_mode)
{
    const flashfatfs_path_t tmp_path = flashfatfs_get_full_path(p_ffs, file_path);
    const char *            mode     = flag_use_binary_mode ? "rb" : "r";

    FILE *fd = fopen(tmp_path.buf, mode);
    if (NULL == fd)
    {
        LOG_ERR("Can't open: %s", tmp_path.buf);
    }
    return fd;
}

bool
flashfatfs_stat(const flash_fat_fs_t *p_ffs, const char *file_path, struct stat *p_st)
{
    const flashfatfs_path_t tmp_path = flashfatfs_get_full_path(p_ffs, file_path);
    if (stat(tmp_path.buf, p_st) < 0)
    {
        return false;
    }
    return true;
}

bool
flashfatfs_get_file_size(const flash_fat_fs_t *p_ffs, const char *file_path, size_t *p_size)
{
    struct stat st = { 0 };
    *p_size        = 0;
    if (!flashfatfs_stat(p_ffs, file_path, &st))
    {
        return false;
    }
    *p_size = (size_t)st.st_size;
    return true;
}
