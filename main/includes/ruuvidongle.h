#pragma once

#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include <stdint.h>
#include <stdbool.h>

#define RUUVI_COMPANY_ID 0x0499
#define RUUVIDONGLE_NVS_CONFIGURATION_KEY "ruuvi_config"
#define RUUVIDONGLE_NVS_BOOT_KEY "ruuvi_boot"

#define MAC_LEN 12
#define ADV_DATA_MAX_LEN 64

#define MAX_CONFIG_STR_LEN  64
#define MAX_HTTP_URL_LEN    256
#define MAX_HTTP_USER_LEN   51
#define MAX_HTTP_PASS_LEN   51
#define MAX_MQTT_SERVER_LEN 256
#define MAX_MQTT_PREFIX_LEN 51
#define MAX_MQTT_USER_LEN   51
#define MAX_MQTT_PASS_LEN   51
#define IP_STR_LEN 17

#define ADV_POST_INTERVAL 10000
#define MAX_ADVS_TABLE 100

#define WIFI_CONNECTED_BIT (1U << 0U)
#define MQTT_CONNECTED_BIT (1U << 1U)
#define RESET_BUTTON_BIT (1U << 2U)
#define ETH_DISCONNECTED_BIT (1U << 3U)
#define ETH_CONNECTED_BIT (1U << 4U)

typedef struct adv_report
{
    char tag_mac[MAC_LEN + 1];
    time_t timestamp;
    int rssi;
    char data[ADV_DATA_MAX_LEN + 1];
} adv_report_t;

struct adv_report_table
{
    int num_of_advs;
    adv_report_t table[MAX_ADVS_TABLE];
};

#define RUUVI_DONGLE_CONFIG_HEADER          (0xAABBU)
#define RUUVI_DONGLE_CONFIG_FMT_VERSION     (0x0003U)

struct dongle_config
{
    uint16_t header;
    uint16_t fmt_version;
    bool eth_dhcp;
    bool use_mqtt;
    bool use_http;
    char eth_static_ip[IP_STR_LEN];
    char eth_netmask[IP_STR_LEN];
    char eth_gw[IP_STR_LEN];
    char eth_dns1[IP_STR_LEN];
    char eth_dns2[IP_STR_LEN];
    char mqtt_server[MAX_MQTT_SERVER_LEN];
    uint32_t mqtt_port;
    char mqtt_prefix[MAX_MQTT_PREFIX_LEN];
    char mqtt_user[MAX_MQTT_USER_LEN];
    char mqtt_pass[MAX_MQTT_PASS_LEN];
    char http_url[MAX_HTTP_URL_LEN];
    char http_user[MAX_HTTP_USER_LEN];
    char http_pass[MAX_HTTP_PASS_LEN];
    uint16_t company_id;
    bool company_filter;
    char coordinates[MAX_CONFIG_STR_LEN];
};

#define RUUVIDONGLE_DEFAULT_CONFIGURATION   {   \
    .header = RUUVI_DONGLE_CONFIG_HEADER, \
    .fmt_version = RUUVI_DONGLE_CONFIG_FMT_VERSION, \
    .eth_dhcp = true,               \
    .use_http = false,              \
    .use_mqtt = false,              \
    .eth_static_ip = { 0 },         \
    .eth_netmask = { 0 },           \
    .eth_gw = { 0 },                \
    .eth_dns1 = { 0 },              \
    .eth_dns2 = { 0 },              \
    .mqtt_server = { 0 },           \
    .mqtt_port = 0,                 \
    .mqtt_prefix = { 0 },           \
    .mqtt_user = { 0 },             \
    .mqtt_pass = { 0 },             \
    .http_url = { 0 },              \
    .http_user = { 0 },             \
    .http_pass = { 0 },             \
    .company_filter = true,         \
    .company_id = RUUVI_COMPANY_ID, \
    .coordinates = { 0 }            \
}

typedef enum nrf_command_t
{
    SET_FILTER,
    CLEAR_FILTER
} nrf_command_t;

extern struct dongle_config m_dongle_config;
extern EventGroupHandle_t status_bits;
extern char gw_mac[MAC_LEN + 1];

char * ruuvi_get_conf_json();
void settings_get_from_flash (struct dongle_config * dongle_config);
void settings_print (struct dongle_config * config);
int settings_save_to_flash (struct dongle_config * config);
void ruuvi_send_nrf_settings (struct dongle_config * config);
void ethernet_connection_ok_cb();
void ethernet_connection_down_cb();
void ethernet_link_up_cb();
void ethernet_link_down_cb();
void start_services();

