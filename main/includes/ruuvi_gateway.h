/**
 * @file ruuvi_gateway.h
 * @author Jukka Saari
 * @date 2019-11-27
 * @copyright Ruuvi Innovations Ltd, license BSD-3-Clause.
 */

#ifndef RUUVI_GATEWAY_H
#define RUUVI_GATEWAY_H

#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include <stdbool.h>
#include <stdint.h>
#include "../mac_addr.h"
#include "../cjson_wrap.h"
#include "../settings.h"

#ifdef __cplusplus
extern "C" {
#endif

#define ADV_POST_INTERVAL 10000
#define MAX_ADVS_TABLE    (100U)

#define ADV_DATA_MAX_LEN 64

#define WIFI_CONNECTED_BIT   (1U << 0U)
#define MQTT_CONNECTED_BIT   (1U << 1U)
#define RESET_BUTTON_BIT     (1U << 2U)
#define ETH_DISCONNECTED_BIT (1U << 3U)
#define ETH_CONNECTED_BIT    (1U << 4U)

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

typedef struct gw_metrics_t
{
    uint64_t received_advertisements;
} gw_metrics_t;

typedef enum nrf_command_e
{
    NRF_COMMAND_SET_FILTER   = 0,
    NRF_COMMAND_CLEAR_FILTER = 1,
} nrf_command_e;

extern EventGroupHandle_t status_bits;
extern gw_metrics_t       gw_metrics;

void
settings_clear_in_flash(void);

char *
ruuvi_get_metrics(void);

void
ruuvi_send_nrf_settings(const ruuvi_gateway_config_t *p_config);

void
ethernet_connection_ok_cb(void);

void
ethernet_link_up_cb(void);

void
ethernet_link_down_cb(void);

void
start_services(void);

#ifdef __cplusplus
}
#endif

#endif // RUUVI_GATEWAY_H
