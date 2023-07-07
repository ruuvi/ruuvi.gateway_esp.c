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
#include "gw_cfg.h"

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
#define MAX_ADVS_TABLE   (GW_CFG_MAX_NUM_SENSORS)

#define ADV_TABLE_RSSI_LOCAL_SENSOR (-1)

typedef int8_t   wifi_rssi_t;
typedef uint8_t  ble_data_len_t;
typedef uint32_t adv_counter_t;

typedef struct adv_report_t
{
    time_t            timestamp;
    adv_counter_t     samples_counter;
    mac_address_bin_t tag_mac;
    wifi_rssi_t       rssi;
    ble_data_len_t    data_len;
    uint8_t           data_buf[ADV_DATA_MAX_LEN];
} adv_report_t;

typedef uint32_t num_of_advs_t;

typedef struct adv_report_table_t
{
    num_of_advs_t num_of_advs;
    adv_report_t  table[MAX_ADVS_TABLE];
} adv_report_table_t;

void
adv_table_init(const mac_address_bin_t esp32_bluetooth_mac_addr);

void
adv_table_deinit(void);

bool
adv_table_put(const adv_report_t* const p_adv);

void
adv_table_read_retransmission_list1_and_clear(adv_report_table_t* const p_reports);

void
adv_table_read_retransmission_list2_and_clear(adv_report_table_t* const p_reports);

void
adv_table_history_read(
    adv_report_table_t* const p_reports,
    const time_t              cur_time,
    const bool                flag_use_timestamps,
    const uint32_t            filter,
    const bool                flag_use_filter);

void
adv_table_statistics_read(adv_report_table_t* const p_reports);

void
adv_table_clear(void);

#if RUUVI_TESTS_ADV_TABLE

ADV_TABLE_STATIC
uint32_t
adv_report_calc_hash(const mac_address_bin_t* const p_mac);

#endif /* RUUVI_TESTS_ADV_TABLE */

#ifdef __cplusplus
}
#endif

#endif // RUUVI_GATEWAY_ESP_ADV_TABLE_H
