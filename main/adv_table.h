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

#if !defined(RUUVI_TESTS_ADV_TABLE)
#define RUUVI_TESTS_ADV_TABLE 0
#endif

#if RUUVI_TESTS_ADV_TABLE
#define ADV_TABLE_STATIC
#else
#define ADV_TABLE_STATIC static
#endif

#define ADV_DATA_MAX_LEN (32)
#define MAX_ADVS_TABLE   (100U)

typedef int8_t  wifi_rssi_t;
typedef uint8_t ble_date_len_t;

typedef struct adv_report_t
{
    time_t            timestamp;
    mac_address_bin_t tag_mac;
    wifi_rssi_t       rssi;
    ble_date_len_t    data_len;
    uint8_t           data_buf[ADV_DATA_MAX_LEN];
} adv_report_t;

typedef uint32_t num_of_advs_t;

typedef struct adv_report_table_t
{
    num_of_advs_t num_of_advs;
    adv_report_t  table[MAX_ADVS_TABLE];
} adv_report_table_t;

void
adv_table_init(void);

void
adv_table_deinit(void);

bool
adv_table_put(const adv_report_t *const p_adv);

void
adv_table_read_and_clear(adv_report_table_t *const p_reports);

#if RUUVI_TESTS_ADV_TABLE

ADV_TABLE_STATIC
uint32_t
adv_report_calc_hash(const mac_address_bin_t *const p_mac);

#endif /* RUUVI_TESTS_ADV_TABLE */

#ifdef __cplusplus
}
#endif

#endif // RUUVI_GATEWAY_ESP_ADV_TABLE_H
