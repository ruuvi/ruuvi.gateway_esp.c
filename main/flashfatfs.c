/**
 * @file flashfatfs.c
 * @author TheSomeMan
 * @date 2020-09-14
 * @copyright Ruuvi Innovations Ltd, license BSD-3-Clause.
 */

#include "flashfatfs.h"
#define LOG_LOCAL_LEVEL LOG_LEVEL_INFO
#include "log.h"
#include <string.h>
#include <stdlib.h>
#include <fcntl.h>
#include "esp_err.h"
#include "esp_vfs_fat.h"
#include "os_malloc.h"
#include "str_buf.h"

#if !defined(RUUVI_TESTS_NRF52FW)
#define RUUVI_TESTS_NRF52FW (0)
#endif

#if !defined(RUUVI_TESTS_FLASHFATFS)
#define RUUVI_TESTS_FLASHFATFS (0)
#endif

static const char TAG[] = "FlashFatFS";

struct flash_fat_fs_t
{
    char* mount_point;
    char* partition_label;
};

const flash_fat_fs_t*
flashfatfs_mount(const char* mount_point, const char* partition_label, const flash_fat_fs_num_files_t max_files)
{
    const char* mount_point_prefix = "";
#if RUUVI_TESTS_NRF52FW || RUUVI_TESTS_FLASHFATFS
    mount_point_prefix = (mount_point[0] == '/') ? "." : "";
#endif
    size_t mount_point_buf_size     = strlen(mount_point_prefix) + strlen(mount_point) + 1;
    size_t partition_label_buf_size = strlen(partition_label) + 1;

    LOG_INFO("Mount partition '%s' to the mount point %s", partition_label, mount_point);
    flash_fat_fs_t* p_obj = os_calloc(1, sizeof(*p_obj) + mount_point_buf_size + partition_label_buf_size);
    if (NULL == p_obj)
    {
        LOG_ERR("Can't allocate memory");
        return NULL;
    }
    p_obj->mount_point = (void*)(p_obj + 1);
    snprintf(p_obj->mount_point, mount_point_buf_size, "%s%s", mount_point_prefix, mount_point);
    p_obj->partition_label = p_obj->mount_point + mount_point_buf_size;
    snprintf(p_obj->partition_label, partition_label_buf_size, "%s", partition_label);

    esp_vfs_fat_mount_config_t mount_config = {
        .format_if_mount_failed = false,
        .max_files              = max_files,
        .allocation_unit_size   = 512U,
    };
    const esp_err_t err = esp_vfs_fat_rawflash_mount(p_obj->mount_point, p_obj->partition_label, &mount_config);
    if (ESP_OK != err)
    {
        LOG_ERR_ESP(err, "%s failed", "esp_vfs_fat_rawflash_mount");
        os_free(p_obj);
        return NULL;
    }
    LOG_INFO("Partition '%s' mounted successfully to %s", partition_label, mount_point);
    return p_obj;
}

bool
flashfatfs_unmount(const flash_fat_fs_t** pp_ffs)
{
    const flash_fat_fs_t* p_ffs = *pp_ffs;
    LOG_INFO("Unmount %s", p_ffs->mount_point);
    const esp_err_t err = esp_vfs_fat_rawflash_unmount(p_ffs->mount_point, p_ffs->partition_label);
    os_free(p_ffs);
    *pp_ffs = NULL;
    if (ESP_OK != err)
    {
        LOG_ERR_ESP(err, "%s failed", "esp_vfs_fat_rawflash_unmount");
        return false;
    }
    return true;
}

FLASHFATFS_CB_STATIC
str_buf_t
flashfatfs_get_full_path(const flash_fat_fs_t* p_ffs, const char* file_path)
{
    const char* mount_point = (NULL != p_ffs) ? p_ffs->mount_point : "";
    return str_buf_printf_with_alloc("%s/%s", mount_point, file_path);
}

file_descriptor_t
flashfatfs_open(const flash_fat_fs_t* p_ffs, const char* file_path)
{
    if (NULL == p_ffs)
    {
        LOG_ERR("p_ffs is NULL");
        return FILE_DESCRIPTOR_INVALID;
    }
    str_buf_t tmp_path = flashfatfs_get_full_path(p_ffs, file_path);
    if (NULL == tmp_path.buf)
    {
        LOG_ERR("Can't allocate memory");
        return FILE_DESCRIPTOR_INVALID;
    }
    file_descriptor_t file_desc = open(tmp_path.buf, O_RDONLY);
    if (file_desc < 0)
    {
        LOG_ERR("Can't open: %s", tmp_path.buf);
    }
    str_buf_free_buf(&tmp_path);
    return file_desc;
}

FILE*
flashfatfs_fopen(const flash_fat_fs_t* p_ffs, const char* file_path, const bool flag_use_binary_mode)
{
    str_buf_t   tmp_path = flashfatfs_get_full_path(p_ffs, file_path);
    const char* mode     = flag_use_binary_mode ? "rb" : "r";
    if (NULL == tmp_path.buf)
    {
        LOG_ERR("Can't allocate memory");
        return (FILE*)NULL;
    }

    FILE* p_fd = fopen(tmp_path.buf, mode);
    if (NULL == p_fd)
    {
        LOG_ERR("Can't open: %s", tmp_path.buf);
    }
    str_buf_free_buf(&tmp_path);
    return p_fd;
}

bool
flashfatfs_stat(const flash_fat_fs_t* p_ffs, const char* file_path, struct stat* p_st)
{
    str_buf_t tmp_path = flashfatfs_get_full_path(p_ffs, file_path);
    if (NULL == tmp_path.buf)
    {
        LOG_ERR("Can't allocate memory");
        return false;
    }
    if (stat(tmp_path.buf, p_st) < 0)
    {
        str_buf_free_buf(&tmp_path);
        return false;
    }
    str_buf_free_buf(&tmp_path);
    return true;
}

bool
flashfatfs_get_file_size(const flash_fat_fs_t* p_ffs, const char* file_path, size_t* p_size)
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
