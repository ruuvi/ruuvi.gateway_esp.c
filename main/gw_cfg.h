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
#include "fw_ver.h"
#include "nrf52_fw_ver.h"
#include "ruuvi_device_id.h"

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
#define MAX_MQTT_PREFIX_LEN    257
#define MAX_MQTT_USER_LEN      129
#define MAX_MQTT_PASS_LEN      257
#define MAX_MQTT_CLIENT_ID_LEN 51
#define IP_STR_LEN             17

#define RUUVI_COMPANY_ID 0x0499

typedef struct ruuvi_gw_cfg_device_info_t
{
    ruuvi_esp32_fw_ver_str_t esp32_fw_ver;
    ruuvi_nrf52_fw_ver_str_t nrf52_fw_ver;
    mac_address_str_t        nrf52_mac_addr;
    nrf52_device_id_str_t    nrf52_device_id;
    mac_address_str_t        esp32_mac_addr_wifi;
    mac_address_str_t        esp32_mac_addr_eth;
} ruuvi_gw_cfg_device_info_t;

typedef struct ruuvi_gw_cfg_ip_addr_str_t
{
    char buf[IP_STR_LEN];
} ruuvi_gw_cfg_ip_addr_str_t;

typedef struct ruuvi_gw_cfg_eth_t
{
    bool                       use_eth;
    bool                       eth_dhcp;
    ruuvi_gw_cfg_ip_addr_str_t eth_static_ip;
    ruuvi_gw_cfg_ip_addr_str_t eth_netmask;
    ruuvi_gw_cfg_ip_addr_str_t eth_gw;
    ruuvi_gw_cfg_ip_addr_str_t eth_dns1;
    ruuvi_gw_cfg_ip_addr_str_t eth_dns2;
} ruuvi_gw_cfg_eth_t;

#define MQTT_TRANSPORT_TCP "TCP"
#define MQTT_TRANSPORT_SSL "SSL"
#define MQTT_TRANSPORT_WS  "WS"
#define MQTT_TRANSPORT_WSS "WSS"

typedef struct ruuvi_gw_cfg_mqtt_transport_t
{
    char buf[MAX_MQTT_TRANSPORT_LEN];
} ruuvi_gw_cfg_mqtt_transport_t;

typedef struct ruuvi_gw_cfg_mqtt_server_t
{
    char buf[MAX_MQTT_SERVER_LEN];
} ruuvi_gw_cfg_mqtt_server_t;

typedef struct ruuvi_gw_cfg_mqtt_prefix_t
{
    char buf[MAX_MQTT_PREFIX_LEN];
} ruuvi_gw_cfg_mqtt_prefix_t;

typedef struct ruuvi_gw_cfg_mqtt_client_id_t
{
    char buf[MAX_MQTT_CLIENT_ID_LEN];
} ruuvi_gw_cfg_mqtt_client_id_t;

typedef struct ruuvi_gw_cfg_mqtt_user_t
{
    char buf[MAX_MQTT_USER_LEN];
} ruuvi_gw_cfg_mqtt_user_t;

typedef struct ruuvi_gw_cfg_mqtt_password_t
{
    char buf[MAX_MQTT_PASS_LEN];
} ruuvi_gw_cfg_mqtt_password_t;

typedef struct ruuvi_gw_cfg_mqtt_t
{
    bool                          use_mqtt;
    ruuvi_gw_cfg_mqtt_transport_t mqtt_transport;
    ruuvi_gw_cfg_mqtt_server_t    mqtt_server;
    uint16_t                      mqtt_port;
    ruuvi_gw_cfg_mqtt_prefix_t    mqtt_prefix;
    ruuvi_gw_cfg_mqtt_client_id_t mqtt_client_id;
    ruuvi_gw_cfg_mqtt_user_t      mqtt_user;
    ruuvi_gw_cfg_mqtt_password_t  mqtt_pass;
} ruuvi_gw_cfg_mqtt_t;

typedef struct ruuvi_gw_cfg_http_url_t
{
    char buf[MAX_HTTP_URL_LEN];
} ruuvi_gw_cfg_http_url_t;

typedef struct ruuvi_gw_cfg_http_user_t
{
    char buf[MAX_HTTP_USER_LEN];
} ruuvi_gw_cfg_http_user_t;

typedef struct ruuvi_gw_cfg_http_password_t
{
    char buf[MAX_HTTP_PASS_LEN];
} ruuvi_gw_cfg_http_password_t;

typedef struct ruuvi_gw_cfg_http_t
{
    bool                         use_http;
    ruuvi_gw_cfg_http_url_t      http_url;
    ruuvi_gw_cfg_http_user_t     http_user;
    ruuvi_gw_cfg_http_password_t http_pass;
} ruuvi_gw_cfg_http_t;

typedef struct ruuvi_gw_cfg_http_stat_t
{
    bool                         use_http_stat;
    ruuvi_gw_cfg_http_url_t      http_stat_url;
    ruuvi_gw_cfg_http_user_t     http_stat_user;
    ruuvi_gw_cfg_http_password_t http_stat_pass;
} ruuvi_gw_cfg_http_stat_t;

typedef struct ruuvi_gw_cfg_lan_auth_t
{
    http_server_auth_type_e    lan_auth_type;
    http_server_auth_user_t    lan_auth_user;
    http_server_auth_pass_t    lan_auth_pass;
    http_server_auth_api_key_t lan_auth_api_key;
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
    ruuvi_gw_cfg_device_info_t device_info;
    ruuvi_gw_cfg_eth_t         eth;
    ruuvi_gw_cfg_mqtt_t        mqtt;
    ruuvi_gw_cfg_http_t        http;
    ruuvi_gw_cfg_http_stat_t   http_stat;
    ruuvi_gw_cfg_lan_auth_t    lan_auth;
    ruuvi_gw_cfg_auto_update_t auto_update;
    ruuvi_gw_cfg_filter_t      filter;
    ruuvi_gw_cfg_scan_t        scan;
    ruuvi_gw_cfg_coordinates_t coordinates;
} ruuvi_gateway_config_t;

void
gw_cfg_init(void);

void
gw_cfg_deinit(void);

bool
gw_cfg_is_initialized(void);

ruuvi_gateway_config_t *
gw_cfg_lock_rw(void);

void
gw_cfg_unlock_rw(ruuvi_gateway_config_t **const p_p_gw_cfg);

const ruuvi_gateway_config_t *
gw_cfg_lock_ro(void);

void
gw_cfg_unlock_ro(const ruuvi_gateway_config_t **const p_p_gw_cfg);

void
gw_cfg_print_to_log(
    const ruuvi_gateway_config_t *const p_config,
    const char *const                   p_title,
    const bool                          flag_print_device_info);

void
gw_cfg_update(const ruuvi_gateway_config_t *const p_gw_cfg_new, const bool flag_network_cfg);

void
gw_cfg_get_copy(ruuvi_gateway_config_t *const p_gw_cfg);

bool
gw_cfg_get_eth_use_eth(void);

bool
gw_cfg_get_eth_use_dhcp(void);

bool
gw_cfg_get_mqtt_use_mqtt(void);

bool
gw_cfg_get_http_use_http(void);

bool
gw_cfg_get_http_stat_use_http_stat(void);

ruuvi_gw_cfg_mqtt_prefix_t
gw_cfg_get_mqtt_prefix(void);

auto_update_cycle_type_e
gw_cfg_get_auto_update_cycle(void);

ruuvi_gw_cfg_lan_auth_t
gw_cfg_get_lan_auth(void);

ruuvi_gw_cfg_coordinates_t
gw_cfg_get_coordinates(void);

void
gw_cfg_set_default_lan_auth(void);

const ruuvi_esp32_fw_ver_str_t *
gw_cfg_get_esp32_fw_ver(void);

const ruuvi_nrf52_fw_ver_str_t *
gw_cfg_get_nrf52_fw_ver(void);

const nrf52_device_id_str_t *
gw_cfg_get_nrf52_device_id(void);

const mac_address_str_t *
gw_cfg_get_nrf52_mac_addr(void);

const mac_address_str_t *
gw_cfg_get_esp32_mac_addr_wifi(void);

const mac_address_str_t *
gw_cfg_get_esp32_mac_addr_eth(void);

const wifi_ssid_t *
gw_cfg_get_wifi_ap_ssid(void);

const char *
gw_cfg_auth_type_to_str(const ruuvi_gw_cfg_lan_auth_t *const p_lan_auth);

#ifdef __cplusplus
}
#endif

#endif // RUUVI_GW_CFG_H
