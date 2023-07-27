/**
 * @file partition_table.c
 * @author TheSomeMan
 * @date 2023-05-11
 * @copyright Ruuvi Innovations Ltd, license BSD-3-Clause.
 */

#include "partition_table.h"
#include <stdint.h>
#include <string.h>
#include <esp_spi_flash.h>
#include "os_malloc.h"
#include "os_mutex.h"

#define LOG_LOCAL_LEVEL LOG_LEVEL_INFO
#include "log.h"

#define PARTITION_TABLE_UPDATE_MAX_NUM_RETRIES (10U)

static const char TAG[] = "partition_table";

extern const uint8_t partition_table_start[] asm("_binary_partition_table_bin_start");
extern const uint8_t partition_table_end[] asm("_binary_partition_table_bin_end");

static os_mutex_static_t g_mutex_partition_table_mem;
static os_mutex_t        g_mutex_partition_table = NULL;

void
partition_table_update_init_mutex(void)
{
    if (NULL == g_mutex_partition_table)
    {
        g_mutex_partition_table = os_mutex_create_static(&g_mutex_partition_table_mem);
    }
}

static bool
partition_table_erase_write(const uint32_t partition_table_address, const size_t partition_table_size)
{
    esp_err_t err = spi_flash_erase_range(partition_table_address, SPI_FLASH_SEC_SIZE);
    if (ESP_OK != err)
    {
        LOG_ERR_ESP(err, "%s failed", "esp_flash_erase_region");
        return false;
    }
    err = spi_flash_write(partition_table_address, partition_table_start, partition_table_size);
    if (ESP_OK != err)
    {
        LOG_ERR_ESP(err, "%s failed", "esp_flash_read");
        return false;
    }
    return true;
}

static bool
partition_table_check_and_update_internal(
    const uint32_t partition_table_address,
    const size_t   partition_table_size,
    uint8_t* const p_buf)
{
    LOG_INFO("#### Check partition table");

    esp_err_t err = spi_flash_read(partition_table_address, p_buf, SPI_FLASH_SEC_SIZE);
    if (ESP_OK != err)
    {
        LOG_ERR_ESP(err, "%s failed", "esp_flash_read");
        return false;
    }
    if (0 != memcmp(partition_table_start, p_buf, partition_table_size))
    {
        LOG_WARN("Partition table do not match - need to update");

        bool flag_partition_table_updated = false;
        for (uint32_t i = 0; i < PARTITION_TABLE_UPDATE_MAX_NUM_RETRIES; ++i)
        {
            if (partition_table_erase_write(partition_table_address, partition_table_size))
            {
                flag_partition_table_updated = true;
                break;
            }
            vTaskDelay(pdMS_TO_TICKS(100));
        }

        if (flag_partition_table_updated)
        {
            LOG_INFO("#### Partition table has been successfully updated");
        }
        else
        {
            LOG_ERR("#### Failed to update partition table");
        }
        return true; // reboot required
    }
    LOG_INFO("#### Partition table has already been updated, no update is necessary");
    return false; // reboot not required
}

bool
partition_table_check_and_update(void)
{
    partition_table_update_init_mutex();

    const uint32_t partition_table_address = 0x8000;
    const size_t   partition_table_size    = partition_table_end - partition_table_start;
    if (partition_table_size > SPI_FLASH_SEC_SIZE)
    {
        LOG_ERR(
            "The partition table size (%u) is greater than the sector size (%u)",
            (printf_uint_t)partition_table_size,
            (printf_uint_t)SPI_FLASH_SEC_SIZE);
        return false;
    }

    os_mutex_lock(g_mutex_partition_table);

    uint8_t* p_buf = os_malloc(SPI_FLASH_SEC_SIZE);
    if (NULL == p_buf)
    {
        LOG_ERR("Can't allocate memory");
        os_mutex_unlock(g_mutex_partition_table);
        return false;
    }

    const bool flag_reboot_required = partition_table_check_and_update_internal(
        partition_table_address,
        partition_table_size,
        p_buf);

    os_free(p_buf);

    os_mutex_unlock(g_mutex_partition_table);

    return flag_reboot_required;
}
