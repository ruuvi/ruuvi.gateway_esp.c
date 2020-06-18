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

#define MAX_CONFIG_STR_LEN 64
#define MAX_HTTPURL_LEN 512
#define MAX_MQTTSERVER_LEN 256
#define MAX_MQTTPREFIX_LEN 64
#define MAX_MQTTUSER_LEN 64
#define MAX_MQTTPASS_LEN 64
#define IP_STR_LEN 17

#define ADV_POST_INTERVAL 10000
#define MAX_ADVS_TABLE 100

#define WIFI_CONNECTED_BIT (1 << 0)
#define MQTT_CONNECTED_BIT (1 << 1)
#define RESET_BUTTON_BIT (1 << 2)
#define ETH_DISCONNECTED_BIT (1 << 3)
#define ETH_CONNECTED_BIT (1 << 4)

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
struct dongle_config
{
    bool eth_dhcp;
    char eth_static_ip[IP_STR_LEN];
    char eth_netmask[IP_STR_LEN];
    char eth_gw[IP_STR_LEN];
    char eth_dns1[IP_STR_LEN];
    char eth_dns2[IP_STR_LEN];
    bool use_mqtt;
    bool use_http;
    char mqtt_server[256];
    uint32_t mqtt_port;
    char mqtt_prefix[256];
    char mqtt_user[64];
    char mqtt_pass[64];
    char http_url[512];
    uint16_t company_id;
    bool company_filter;
    char coordinates[MAX_CONFIG_STR_LEN];
};

#define RUUVIDONGLE_DEFAULT_CONFIGURATION   {   \
    .eth_dhcp = true,               \
    .eth_static_ip = { 0 },         \
    .eth_netmask = { 0 },           \
    .eth_gw = { 0 },                \
    .eth_dns1 = { 0 },              \
    .eth_dns2 = { 0 },              \
    .use_http = false,              \
    .use_mqtt = false,              \
    .http_url = { 0 },              \
    .mqtt_server = { 0 },           \
    .mqtt_port = 0,                 \
    .mqtt_prefix = { 0 },           \
    .mqtt_user = { 0 },             \
    .mqtt_pass = { 0 },             \
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
bool settings_get_from_flash (struct dongle_config * dongle_config);
void settings_print (struct dongle_config * config);
int settings_save_to_flash (struct dongle_config * config);
void ruuvi_send_nrf_settings (struct dongle_config * config);
void ethernet_connection_ok_cb();
void ethernet_connection_down_cb();
void ethernet_link_up_cb();
void ethernet_link_down_cb();
void start_services();

