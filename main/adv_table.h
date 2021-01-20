/**
 * @file adv_table.h
 * @author TheSomeMan
 * @date 2021-01-19
 * @copyright Ruuvi Innovations Ltd, license BSD-3-Clause.
 */

#ifndef RUUVI_GATEWAY_ESP_ADV_TABLE_H
#define RUUVI_GATEWAY_ESP_ADV_TABLE_H

#include <stdint.h>
#include <stdbool.h>
#include <time.h>
#include "mac_addr.h"

#ifdef __cplusplus
extern "C" {
#endif

#define ADV_DATA_MAX_LEN (64)
#define MAX_ADVS_TABLE   (100U)

typedef int32_t wifi_rssi_t;

typedef struct adv_report_t
{
    mac_address_bin_t tag_mac;
    time_t            timestamp;
    wifi_rssi_t       rssi;
    char              data[ADV_DATA_MAX_LEN + 1];
} adv_report_t;

typedef uint32_t num_of_advs_t;

typedef struct adv_report_table_t
{
    num_of_advs_t num_of_advs;
    adv_report_t  table[MAX_ADVS_TABLE];
} adv_report_table_t;

void
adv_table_init(void);

bool
adv_table_put(const adv_report_t *const p_adv);

void
adv_table_read_and_clear(adv_report_table_t *const p_reports);

#ifdef __cplusplus
}
#endif

#endif // RUUVI_GATEWAY_ESP_ADV_TABLE_H
