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

#define LOG_LOCAL_LEVEL LOG_LEVEL_INFO
#include "log.h"

static const char TAG[] = "partition_table";

extern const uint8_t partition_table_start[] asm("_binary_partition_table_bin_start");
extern const uint8_t partition_table_end[] asm("_binary_partition_table_bin_end");

bool
partition_table_check_and_update(void)
{
    const uint32_t partition_table_address = 0x8000;
    const size_t   partition_table_size    = partition_table_end - partition_table_start;
    bool           flag_partition_updated  = false;
    LOG_INFO("#### Check partition table");
    if (partition_table_size > SPI_FLASH_SEC_SIZE)
    {
        LOG_ERR(
            "The partition table size (%u) is greater than the sector size (%u)",
            (printf_uint_t)partition_table_size,
            (printf_uint_t)SPI_FLASH_SEC_SIZE);
        return false;
    }
    uint8_t* p_buf = os_malloc(SPI_FLASH_SEC_SIZE);
    if (NULL == p_buf)
    {
        LOG_ERR("Can't allocate memory");
        return false;
    }

    esp_err_t err = spi_flash_read(partition_table_address, p_buf, SPI_FLASH_SEC_SIZE);
    if (ESP_OK != err)
    {
        LOG_ERR_ESP(err, "%s failed", "esp_flash_read");
        os_free(p_buf);
        return false;
    }
    if (0 != memcmp(partition_table_start, p_buf, partition_table_size))
    {
        LOG_WARN("Partition table do not match - need to update");
        err = spi_flash_erase_range(partition_table_address, SPI_FLASH_SEC_SIZE);
        if (ESP_OK != err)
        {
            LOG_ERR_ESP(err, "%s failed", "esp_flash_erase_region");
            os_free(p_buf);
            return false;
        }
        err = spi_flash_write(partition_table_address, partition_table_start, partition_table_size);
        if (ESP_OK != err)
        {
            LOG_ERR_ESP(err, "%s failed", "esp_flash_read");
            os_free(p_buf);
            return false;
        }
        LOG_INFO("#### Partition table has been successfully updated");
        // reboot required
        flag_partition_updated = true;
    }
    os_free(p_buf);
    return flag_partition_updated;
}
