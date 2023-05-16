/**
 * @file partition_table.h
 * @author TheSomeMan
 * @date 2023-05-11
 * @copyright Ruuvi Innovations Ltd, license BSD-3-Clause.
 */

#ifndef RUUVI_GATEWAY_ESP_PARTITION_TABLE_H
#define RUUVI_GATEWAY_ESP_PARTITION_TABLE_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

void
partition_table_update_init_mutex(void);

/**
 * @brief Check that the partition table in the flash memory is the latest version, and write the latest version if it
 * is not.
 * @return true if the partition table in the flash memory has been updated and a reboot is required.
 */
bool
partition_table_check_and_update(void);

#ifdef __cplusplus
}
#endif

#endif // RUUVI_GATEWAY_ESP_PARTITION_TABLE_H
