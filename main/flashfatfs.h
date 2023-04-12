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
#include <sys/stat.h>
#include "esp_type_wrapper.h"
#include "str_buf.h"

#ifdef __cplusplus
extern "C" {
#endif

#if !defined(RUUVI_TESTS_FLASHFATFS)
#define RUUVI_TESTS_FLASHFATFS (0)
#endif

#if RUUVI_TESTS_FLASHFATFS
#define FLASHFATFS_CB_STATIC
#else
#define FLASHFATFS_CB_STATIC static
#endif

typedef struct flash_fat_fs_t flash_fat_fs_t;

typedef int flash_fat_fs_num_files_t;

const flash_fat_fs_t*
flashfatfs_mount(const char* mount_point, const char* partition_label, const flash_fat_fs_num_files_t max_files);

bool
flashfatfs_unmount(const flash_fat_fs_t** pp_ffs);

file_descriptor_t
flashfatfs_open(const flash_fat_fs_t* p_ffs, const char* file_path);

FILE*
flashfatfs_fopen(const flash_fat_fs_t* p_ffs, const char* file_path, const bool flag_use_binary_mode);

bool
flashfatfs_stat(const flash_fat_fs_t* p_ffs, const char* file_path, struct stat* p_st);

bool
flashfatfs_get_file_size(const flash_fat_fs_t* p_ffs, const char* file_path, size_t* p_size);

#if RUUVI_TESTS_FLASHFATFS

FLASHFATFS_CB_STATIC
str_buf_t
flashfatfs_get_full_path(const flash_fat_fs_t* p_ffs, const char* file_path);

#endif // RUUVI_TESTS_FLASHFATFS

#ifdef __cplusplus
}
#endif

#endif // RUUVI_GATEWAY_ESP_FLASH_FATFS_H
