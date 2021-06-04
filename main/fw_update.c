/**
 * @file fw_update.c
 * @author TheSomeMan
 * @date 2021-05-30
 * @copyright Ruuvi Innovations Ltd, license BSD-3-Clause.
 */

#include "fw_update.h"
#include <string.h>
#include "cJSON.h"
#include "http_server.h"

#define LOG_LOCAL_LEVEL LOG_LEVEL_DEBUG
#include "log.h"

#define GW_GWUI_PARTITION_2 GW_GWUI_PARTITION "_2"
#define GW_NRF_PARTITION_2  GW_NRF_PARTITION "_2"

static const char TAG[] = "fw_update";

static ruuvi_flash_info_t g_ruuvi_flash_info;

static const esp_partition_t *
find_data_fat_partition_by_name(const char *const p_partition_name)
{
    esp_partition_iterator_t iter = esp_partition_find(
        ESP_PARTITION_TYPE_DATA,
        ESP_PARTITION_SUBTYPE_DATA_FAT,
        p_partition_name);
    if (NULL == iter)
    {
        LOG_ERR("Can't find partition: %s", p_partition_name);
        return NULL;
    }
    const esp_partition_t *p_partition = esp_partition_get(iter);
    esp_partition_iterator_release(iter);
    return p_partition;
}

static bool
fw_update_read_flash_info_internal(ruuvi_flash_info_t *const p_flash_info)
{
    p_flash_info->is_valid = false;

    p_flash_info->p_app_desc = esp_ota_get_app_description();
    LOG_INFO("Project name     : %s", p_flash_info->p_app_desc->project_name);
    LOG_INFO("Firmware version : %s", p_flash_info->p_app_desc->version);

    p_flash_info->p_boot_partition = esp_ota_get_boot_partition();
    if (NULL == p_flash_info->p_boot_partition)
    {
        LOG_ERR("There is no boot partition info");
        return false;
    }
    p_flash_info->p_running_partition = esp_ota_get_running_partition();
    if (NULL == p_flash_info->p_running_partition)
    {
        LOG_ERR("There is no running partition info");
        return false;
    }
    LOG_INFO("Currently running partition: %s", p_flash_info->p_running_partition->label);
    p_flash_info->p_next_update_partition = esp_ota_get_next_update_partition(p_flash_info->p_running_partition);
    if (NULL == p_flash_info->p_next_update_partition)
    {
        LOG_ERR("Can't find next partition for updating");
        return false;
    }
    LOG_INFO(
        "Next update partition: %s: address 0x%08x, size 0x%x",
        p_flash_info->p_next_update_partition->label,
        p_flash_info->p_next_update_partition->address,
        p_flash_info->p_next_update_partition->size);
    p_flash_info->is_ota0_active = (0 == strcmp("ota_0", p_flash_info->p_running_partition->label)) ? true : false;
    const char *const p_gwui_parition_name    = p_flash_info->is_ota0_active ? GW_GWUI_PARTITION_2 : GW_GWUI_PARTITION;
    p_flash_info->p_next_fatfs_gwui_partition = find_data_fat_partition_by_name(p_gwui_parition_name);
    if (NULL == p_flash_info->p_next_fatfs_gwui_partition)
    {
        return false;
    }
    LOG_INFO(
        "Next fatfs_gwui partition: %s: address 0x%08x, size 0x%x",
        p_flash_info->p_next_fatfs_gwui_partition->label,
        p_flash_info->p_next_fatfs_gwui_partition->address,
        p_flash_info->p_next_fatfs_gwui_partition->size);

    const char *const p_fatfs_nrf52_partition_name = p_flash_info->is_ota0_active ? GW_NRF_PARTITION_2
                                                                                  : GW_NRF_PARTITION;
    p_flash_info->p_next_fatfs_nrf52_partition     = find_data_fat_partition_by_name(p_fatfs_nrf52_partition_name);
    if (NULL == p_flash_info->p_next_fatfs_nrf52_partition)
    {
        return false;
    }
    LOG_INFO(
        "Next fatfs_nrf52 partition: %s: address 0x%08x, size 0x%x",
        p_flash_info->p_next_fatfs_nrf52_partition->label,
        p_flash_info->p_next_fatfs_nrf52_partition->address,
        p_flash_info->p_next_fatfs_nrf52_partition->size);

    p_flash_info->is_valid = true;
    return true;
}

bool
fw_update_read_flash_info(void)
{
    ruuvi_flash_info_t *p_flash_info = &g_ruuvi_flash_info;
    fw_update_read_flash_info_internal(p_flash_info);
    return p_flash_info->is_valid;
}

const char *
fw_update_get_current_fatfs_nrf52_partition_name(void)
{
    ruuvi_flash_info_t *p_flash_info = &g_ruuvi_flash_info;
    return p_flash_info->is_ota0_active ? GW_NRF_PARTITION : GW_NRF_PARTITION_2;
}

const char *
fw_update_get_current_fatfs_gwui_partition_name(void)
{
    ruuvi_flash_info_t *p_flash_info = &g_ruuvi_flash_info;
    return p_flash_info->is_ota0_active ? GW_GWUI_PARTITION : GW_GWUI_PARTITION_2;
}
