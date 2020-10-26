/**
 * @file flashfatfs.h
 * @author TheSomeMan
 * @date 2020-09-14
 * @copyright Ruuvi Innovations Ltd, license BSD-3-Clause.
 */

#ifndef RUUVI_GATEWAY_ESP_FLASH_FATFS_H
#define RUUVI_GATEWAY_ESP_FLASH_FATFS_H

#include <stdbool.h>
#include <stdio.h>
//#include "esp_vfs_fat.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct flash_fat_fs_t flash_fat_fs_t;

typedef int flash_fat_fs_num_files_t;

typedef int file_descriptor_t;

flash_fat_fs_t *
flashfatfs_mount(const char *mount_point, const char *partition_label, const flash_fat_fs_num_files_t max_files);

bool
flashfatfs_unmount(flash_fat_fs_t *p_ffs);

file_descriptor_t
flashfatfs_open(flash_fat_fs_t *p_ffs, const char *file_path);

FILE *
flashfatfs_fopen(flash_fat_fs_t *p_ffs, const char *file_path, const bool flag_use_binary_mode);

#ifdef __cplusplus
}
#endif

#endif // RUUVI_GATEWAY_ESP_FLASH_FATFS_H
