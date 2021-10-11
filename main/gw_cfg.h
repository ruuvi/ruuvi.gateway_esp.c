/**
 * @file gw_cfg.h
 * @author TheSomeMan
 * @date 2020-10-31
 * @copyright Ruuvi Innovations Ltd, license BSD-3-Clause.
 */

#ifndef RUUVI_GW_CFG_H
#define RUUVI_GW_CFG_H

#include <stdint.h>
#include <stdbool.h>
#include "mac_addr.h"
#include "wifi_manager_defs.h"

#if !defined(RUUVI_TESTS_GW_CFG)
#define RUUVI_TESTS_GW_CFG (0)
#endif

#if RUUVI_TESTS_GW_CFG
#define GW_CFG_STATIC
#else
#define GW_CFG_STATIC static
#endif

#ifdef __cplusplus
extern "C" {
#endif

#define MAX_CONFIG_STR_LEN     64
#define MAX_HTTP_URL_LEN       256
#define MAX_HTTP_USER_LEN      51
#define MAX_HTTP_PASS_LEN      51
#define MAX_MQTT_TRANSPORT_LEN 8
#define MAX_MQTT_SERVER_LEN    256
#define MAX_MQTT_PREFIX_LEN    51
#define MAX_MQTT_USER_LEN      51
#define MAX_MQTT_PASS_LEN      51
#define MAX_MQTT_CLIENT_ID_LEN 51
#define MAX_LAN_AUTH_TYPE_LEN  16
#define MAX_LAN_AUTH_USER_LEN  51
#define MAX_LAN_AUTH_PASS_LEN  51
#define IP_STR_LEN             17

#define RUUVI_COMPANY_ID 0x0499

typedef struct ruuvi_gw_cfg_eth_t
{
    bool use_eth;
    bool eth_dhcp;
    char eth_static_ip[IP_STR_LEN];
    char eth_netmask[IP_STR_LEN];
    char eth_gw[IP_STR_LEN];
    char eth_dns1[IP_STR_LEN];
    char eth_dns2[IP_STR_LEN];
} ruuvi_gw_cfg_eth_t;

#define MQTT_TRANSPORT_TCP "TCP"
#define MQTT_TRANSPORT_SSL "SSL"
#define MQTT_TRANSPORT_WS  "WS"
#define MQTT_TRANSPORT_WSS "WSS"

typedef struct ruuvi_gw_cfg_mqtt_t
{
    bool     use_mqtt;
    bool     mqtt_use_default_prefix;
    char     mqtt_transport[MAX_MQTT_TRANSPORT_LEN];
    char     mqtt_server[MAX_MQTT_SERVER_LEN];
    uint16_t mqtt_port;
    char     mqtt_prefix[MAX_MQTT_PREFIX_LEN];
    char     mqtt_client_id[MAX_MQTT_CLIENT_ID_LEN];
    char     mqtt_user[MAX_MQTT_USER_LEN];
    char     mqtt_pass[MAX_MQTT_PASS_LEN];
} ruuvi_gw_cfg_mqtt_t;

typedef struct ruuvi_gw_cfg_http_t
{
    bool use_http;
    char http_url[MAX_HTTP_URL_LEN];
    char http_user[MAX_HTTP_USER_LEN];
    char http_pass[MAX_HTTP_PASS_LEN];
} ruuvi_gw_cfg_http_t;

typedef struct ruuvi_gw_cfg_lan_auth_t
{
    char lan_auth_type[MAX_LAN_AUTH_TYPE_LEN];
    char lan_auth_user[MAX_LAN_AUTH_USER_LEN];
    char lan_auth_pass[MAX_LAN_AUTH_PASS_LEN];
} ruuvi_gw_cfg_lan_auth_t;

#define AUTO_UPDATE_CYCLE_TYPE_STR_REGULAR     "regular"
#define AUTO_UPDATE_CYCLE_TYPE_STR_BETA_TESTER "beta"
#define AUTO_UPDATE_CYCLE_TYPE_STR_MANUAL      "manual"
#define AUTO_UPDATE_CYCLE_TYPE_STR_MAX_LEN     (20U)

typedef enum auto_update_cycle_type_e
{
    AUTO_UPDATE_CYCLE_TYPE_REGULAR     = 0,
    AUTO_UPDATE_CYCLE_TYPE_BETA_TESTER = 1,
    AUTO_UPDATE_CYCLE_TYPE_MANUAL      = 2,
} auto_update_cycle_type_e;

typedef uint8_t auto_update_weekdays_bitmask_t;
typedef uint8_t auto_update_interval_hours_t;
typedef int8_t  auto_update_tz_offset_hours_t;

typedef struct ruuvi_gw_cfg_auto_update_t
{
    auto_update_cycle_type_e       auto_update_cycle;
    auto_update_weekdays_bitmask_t auto_update_weekdays_bitmask;
    auto_update_interval_hours_t   auto_update_interval_from;
    auto_update_interval_hours_t   auto_update_interval_to;
    auto_update_tz_offset_hours_t  auto_update_tz_offset_hours;
} ruuvi_gw_cfg_auto_update_t;

typedef struct ruuvi_gw_cfg_filter_t
{
    uint16_t company_id;
    bool     company_use_filtering;
} ruuvi_gw_cfg_filter_t;

typedef struct ruuvi_gw_cfg_scan_t
{
    bool scan_coded_phy;
    bool scan_1mbit_phy;
    bool scan_extended_payload;
    bool scan_channel_37;
    bool scan_channel_38;
    bool scan_channel_39;
} ruuvi_gw_cfg_scan_t;

typedef struct ruuvi_gw_cfg_coordinates_t
{
    char buf[MAX_CONFIG_STR_LEN];
} ruuvi_gw_cfg_coordinates_t;

typedef struct ruuvi_gateway_config_t
{
    ruuvi_gw_cfg_eth_t         eth;
    ruuvi_gw_cfg_mqtt_t        mqtt;
    ruuvi_gw_cfg_http_t        http;
    ruuvi_gw_cfg_lan_auth_t    lan_auth;
    ruuvi_gw_cfg_auto_update_t auto_update;
    ruuvi_gw_cfg_filter_t      filter;
    ruuvi_gw_cfg_scan_t        scan;
    ruuvi_gw_cfg_coordinates_t coordinates;
} ruuvi_gateway_config_t;

extern mac_address_bin_t g_gw_mac_eth;
extern mac_address_str_t g_gw_mac_eth_str;
extern mac_address_bin_t g_gw_mac_wifi;
extern mac_address_str_t g_gw_mac_wifi_str;
extern mac_address_str_t g_gw_mac_sta_str;
extern wifi_ssid_t       g_gw_wifi_ssid;

void
gw_cfg_init(void);

ruuvi_gateway_config_t *
gw_cfg_lock_rw(void);

void
gw_cfg_unlock_rw(ruuvi_gateway_config_t **const p_p_gw_cfg);

const ruuvi_gateway_config_t *
gw_cfg_lock_ro(void);

void
gw_cfg_unlock_ro(const ruuvi_gateway_config_t **const p_p_gw_cfg);

void
gw_cfg_print_to_log(const ruuvi_gateway_config_t *const p_config);

void
gw_cfg_update(const ruuvi_gateway_config_t *const p_gw_cfg_new, const bool flag_network_cfg);

void
gw_cfg_get_copy(ruuvi_gateway_config_t *const p_gw_cfg);

bool
gw_cfg_get_eth_use_eth(void);

bool
gw_cfg_get_mqtt_use_mqtt(void);

bool
gw_cfg_get_mqtt_use_http(void);

auto_update_cycle_type_e
gw_cfg_get_auto_update_cycle(void);

ruuvi_gw_cfg_lan_auth_t
gw_cfg_get_lan_auth(void);

ruuvi_gw_cfg_coordinates_t
gw_cfg_get_coordinates(void);

#ifdef __cplusplus
}
#endif

#endif // RUUVI_GW_CFG_H
