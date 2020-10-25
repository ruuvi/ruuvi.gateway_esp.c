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

#ifdef __cplusplus
extern "C" {
#endif

#define RUUVI_COMPANY_ID                    0x0499
#define RUUVI_GATEWAY_NVS_CONFIGURATION_KEY "ruuvi_config"
#define RUUVI_GATEWAY_NVS_BOOT_KEY          "ruuvi_boot"

#define ADV_DATA_MAX_LEN 64

#define MAX_CONFIG_STR_LEN  64
#define MAX_HTTP_URL_LEN    256
#define MAX_HTTP_USER_LEN   51
#define MAX_HTTP_PASS_LEN   51
#define MAX_MQTT_SERVER_LEN 256
#define MAX_MQTT_PREFIX_LEN 51
#define MAX_MQTT_USER_LEN   51
#define MAX_MQTT_PASS_LEN   51
#define IP_STR_LEN          17

#define ADV_POST_INTERVAL 10000
#define MAX_ADVS_TABLE    (100U)

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

#define RUUVI_GATEWAY_CONFIG_HEADER      (0xAABBU)
#define RUUVI_GATEWAY_CONFIG_FMT_VERSION (0x0004U)

typedef struct ruuvi_gateway_config_t
{
    uint16_t header;
    uint16_t fmt_version;
    bool     eth_dhcp;
    bool     use_mqtt;
    bool     use_http;
    char     eth_static_ip[IP_STR_LEN];
    char     eth_netmask[IP_STR_LEN];
    char     eth_gw[IP_STR_LEN];
    char     eth_dns1[IP_STR_LEN];
    char     eth_dns2[IP_STR_LEN];
    char     mqtt_server[MAX_MQTT_SERVER_LEN];
    uint16_t mqtt_port;
    char     mqtt_prefix[MAX_MQTT_PREFIX_LEN];
    char     mqtt_user[MAX_MQTT_USER_LEN];
    char     mqtt_pass[MAX_MQTT_PASS_LEN];
    char     http_url[MAX_HTTP_URL_LEN];
    char     http_user[MAX_HTTP_USER_LEN];
    char     http_pass[MAX_HTTP_PASS_LEN];
    uint16_t company_id;
    bool     company_filter;
    bool     scan_coded_phy;
    bool     scan_1mbit_phy;
    bool     scan_extended_payload;
    bool     scan_channel_37;
    bool     scan_channel_38;
    bool     scan_channel_39;
    char     coordinates[MAX_CONFIG_STR_LEN];
} ruuvi_gateway_config_t;

// clang-format off
#define RUUVI_GATEWAY_DEFAULT_CONFIGURATION \
    { \
        .header = RUUVI_GATEWAY_CONFIG_HEADER, \
        .fmt_version = RUUVI_GATEWAY_CONFIG_FMT_VERSION, \
        .eth_dhcp = true, \
        .use_http = true, \
        .use_mqtt = false, \
        .eth_static_ip = { 0 }, \
        .eth_netmask = { 0 }, \
        .eth_gw = { 0 }, \
        .eth_dns1 = { 0 }, \
        .eth_dns2 = { 0 }, \
        .mqtt_server = { 0 }, \
        .mqtt_port = 0, \
        .mqtt_prefix = { 0 }, \
        .mqtt_user = { 0 }, \
        .mqtt_pass = { 0 }, \
        .http_url = { "https://network.ruuvi.com:443/gwapi/v1" }, \
        .http_user = { 0 }, \
        .http_pass = { 0 }, \
        .company_filter = true, \
        .company_id = RUUVI_COMPANY_ID, \
        .scan_coded_phy = false, \
        .scan_1mbit_phy = false, \
        .scan_extended_payload = false, \
        .scan_channel_37 = false, \
        .scan_channel_38 = false, \
        .scan_channel_39 = false, \
        .coordinates = { 0 }, \
    }
// clang-format on

typedef enum nrf_command_e
{
    NRF_COMMAND_SET_FILTER   = 0,
    NRF_COMMAND_CLEAR_FILTER = 1,
} nrf_command_e;

extern ruuvi_gateway_config_t g_gateway_config;
extern EventGroupHandle_t     status_bits;
extern mac_address_str_t      gw_mac_sta;
extern gw_metrics_t           gw_metrics;

void
settings_clear_in_flash(void);

void
settings_save_to_flash(const ruuvi_gateway_config_t *p_config);

void
settings_print(const ruuvi_gateway_config_t *p_config);

void
settings_get_from_flash(ruuvi_gateway_config_t *p_gateway_config);

bool
ruuvi_get_conf_json_str(cjson_wrap_str_t *p_json_str);

__attribute__((__deprecated__)) //
char *
ruuvi_get_conf_json(void);

char *
ruuvi_get_metrics(void);

void
ruuvi_send_nrf_settings(ruuvi_gateway_config_t *config);

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
