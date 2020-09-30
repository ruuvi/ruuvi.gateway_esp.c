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

typedef struct FlashFatFs_Tag FlashFatFs_t;

FlashFatFs_t *
flashfatfs_mount(const char *mount_point, const char *partition_label, const int max_files);

bool
flashfatfs_unmount(FlashFatFs_t *p_ffs);

int
flashfatfs_open(FlashFatFs_t *p_ffs, const char *file_path);

FILE*
flashfatfs_fopen(FlashFatFs_t *p_ffs, const char *file_path);

#ifdef __cplusplus
}
#endif

#endif // RUUVI_GATEWAY_ESP_FLASH_FATFS_H
