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

#define GW_CFG_MAX_NUM_SENSORS (100U)

#define GW_CFG_MAX_HTTP_BEARER_TOKEN_LEN 256
#define GW_CFG_MAX_HTTP_TOKEN_LEN        256
#define GW_CFG_MAX_HTTP_APIKEY_LEN       256
#define GW_CFG_MAX_HTTP_URL_LEN          256
#define GW_CFG_MAX_HTTP_USER_LEN         51
#define GW_CFG_MAX_HTTP_PASS_LEN         51
#define GW_CFG_MAX_MQTT_TRANSPORT_LEN    8
#define GW_CFG_MAX_MQTT_SERVER_LEN       256
#define GW_CFG_MAX_MQTT_PREFIX_LEN       257
#define GW_CFG_MAX_MQTT_USER_LEN         129
#define GW_CFG_MAX_MQTT_PASS_LEN         257
#define GW_CFG_MAX_MQTT_CLIENT_ID_LEN    51
#define GW_CFG_MAX_NTP_SERVER_LEN        32
#define GW_CFG_MAX_COORDINATES_STR_LEN   64
#define GW_CFG_MAX_IP_ADDR_STR_LEN       17

#define RUUVI_COMPANY_ID 0x0499

typedef struct gw_cfg_device_info_t
{
    wifiman_wifi_ssid_t      wifi_ap;
    wifiman_hostname_t       hostname;
    ruuvi_esp32_fw_ver_str_t esp32_fw_ver;
    ruuvi_nrf52_fw_ver_str_t nrf52_fw_ver;
    mac_address_str_t        nrf52_mac_addr;
    nrf52_device_id_str_t    nrf52_device_id;
    mac_address_str_t        esp32_mac_addr_wifi;
    mac_address_str_t        esp32_mac_addr_eth;
} gw_cfg_device_info_t;

typedef struct ruuvi_gw_cfg_ip_addr_str_t
{
    char buf[GW_CFG_MAX_IP_ADDR_STR_LEN];
} ruuvi_gw_cfg_ip_addr_str_t;

typedef struct gw_cfg_eth_t
{
    bool                       use_eth;
    bool                       eth_dhcp;
    ruuvi_gw_cfg_ip_addr_str_t eth_static_ip;
    ruuvi_gw_cfg_ip_addr_str_t eth_netmask;
    ruuvi_gw_cfg_ip_addr_str_t eth_gw;
    ruuvi_gw_cfg_ip_addr_str_t eth_dns1;
    ruuvi_gw_cfg_ip_addr_str_t eth_dns2;
} gw_cfg_eth_t;

#define MQTT_TRANSPORT_TCP "TCP"
#define MQTT_TRANSPORT_SSL "SSL"
#define MQTT_TRANSPORT_WS  "WS"
#define MQTT_TRANSPORT_WSS "WSS"

typedef struct ruuvi_gw_cfg_mqtt_transport_t
{
    char buf[GW_CFG_MAX_MQTT_TRANSPORT_LEN];
} ruuvi_gw_cfg_mqtt_transport_t;

#define GW_CFG_MQTT_DATA_FORMAT_STR_RUUVI_RAW       "ruuvi_raw"
#define GW_CFG_MQTT_DATA_FORMAT_STR_RAW_AND_DECODED "ruuvi_raw_and_decoded"
#define GW_CFG_MQTT_DATA_FORMAT_STR_DECODED         "ruuvi_decoded"

#define GW_CFG_MQTT_DATA_FORMAT_STR_SIZE sizeof(GW_CFG_MQTT_DATA_FORMAT_STR_RAW_AND_DECODED)

typedef enum gw_cfg_mqtt_data_format_e
{
    GW_CFG_MQTT_DATA_FORMAT_RUUVI_RAW = 0,
    GW_CFG_MQTT_DATA_FORMAT_RUUVI_RAW_AND_DECODED,
    GW_CFG_MQTT_DATA_FORMAT_RUUVI_DECODED,
} gw_cfg_mqtt_data_format_e;

typedef struct ruuvi_gw_cfg_mqtt_server_t
{
    char buf[GW_CFG_MAX_MQTT_SERVER_LEN];
} ruuvi_gw_cfg_mqtt_server_t;

typedef struct ruuvi_gw_cfg_mqtt_prefix_t
{
    char buf[GW_CFG_MAX_MQTT_PREFIX_LEN];
} ruuvi_gw_cfg_mqtt_prefix_t;

typedef struct ruuvi_gw_cfg_mqtt_client_id_t
{
    char buf[GW_CFG_MAX_MQTT_CLIENT_ID_LEN];
} ruuvi_gw_cfg_mqtt_client_id_t;

typedef struct ruuvi_gw_cfg_mqtt_user_t
{
    char buf[GW_CFG_MAX_MQTT_USER_LEN];
} ruuvi_gw_cfg_mqtt_user_t;

typedef struct ruuvi_gw_cfg_mqtt_password_t
{
    char buf[GW_CFG_MAX_MQTT_PASS_LEN];
} ruuvi_gw_cfg_mqtt_password_t;

typedef struct ruuvi_gw_cfg_mqtt_t
{
    bool                          use_mqtt;
    bool                          mqtt_disable_retained_messages;
    bool                          use_ssl_client_cert;
    bool                          use_ssl_server_cert;
    ruuvi_gw_cfg_mqtt_transport_t mqtt_transport;
    gw_cfg_mqtt_data_format_e     mqtt_data_format;
    ruuvi_gw_cfg_mqtt_server_t    mqtt_server;
    uint16_t                      mqtt_port;
    uint32_t                      mqtt_sending_interval;
    ruuvi_gw_cfg_mqtt_prefix_t    mqtt_prefix;
    ruuvi_gw_cfg_mqtt_client_id_t mqtt_client_id;
    ruuvi_gw_cfg_mqtt_user_t      mqtt_user;
    ruuvi_gw_cfg_mqtt_password_t  mqtt_pass;
} ruuvi_gw_cfg_mqtt_t;

typedef struct ruuvi_gw_cfg_http_url_t
{
    char buf[GW_CFG_MAX_HTTP_URL_LEN];
} ruuvi_gw_cfg_http_url_t;

typedef struct ruuvi_gw_cfg_http_user_t
{
    char buf[GW_CFG_MAX_HTTP_USER_LEN];
} ruuvi_gw_cfg_http_user_t;

typedef struct ruuvi_gw_cfg_http_password_t
{
    char buf[GW_CFG_MAX_HTTP_PASS_LEN];
} ruuvi_gw_cfg_http_password_t;

typedef struct ruuvi_gw_cfg_http_bearer_token_t
{
    char buf[GW_CFG_MAX_HTTP_BEARER_TOKEN_LEN];
} ruuvi_gw_cfg_http_bearer_token_t;

typedef struct ruuvi_gw_cfg_http_token_t
{
    char buf[GW_CFG_MAX_HTTP_TOKEN_LEN];
} ruuvi_gw_cfg_http_token_t;

typedef struct ruuvi_gw_cfg_http_apikey_t
{
    char buf[GW_CFG_MAX_HTTP_APIKEY_LEN];
} ruuvi_gw_cfg_http_apikey_t;

#define GW_CFG_HTTP_AUTH_TYPE_STR_SIZE 8

#define GW_CFG_HTTP_AUTH_TYPE_STR_NO     "no" /* deprecated */
#define GW_CFG_HTTP_AUTH_TYPE_STR_NONE   "none"
#define GW_CFG_HTTP_AUTH_TYPE_STR_BASIC  "basic"
#define GW_CFG_HTTP_AUTH_TYPE_STR_BEARER "bearer"
#define GW_CFG_HTTP_AUTH_TYPE_STR_TOKEN  "token"
#define GW_CFG_HTTP_AUTH_TYPE_STR_APIKEY "api_key"

typedef enum gw_cfg_http_auth_type_e
{
    GW_CFG_HTTP_AUTH_TYPE_NONE   = 0,
    GW_CFG_HTTP_AUTH_TYPE_BASIC  = 1,
    GW_CFG_HTTP_AUTH_TYPE_BEARER = 2,
    GW_CFG_HTTP_AUTH_TYPE_TOKEN  = 3,
    GW_CFG_HTTP_AUTH_TYPE_APIKEY = 4,
} gw_cfg_http_auth_type_e;

typedef struct ruuvi_gw_cfg_http_auth_basic_t
{
    ruuvi_gw_cfg_http_user_t     user;
    ruuvi_gw_cfg_http_password_t password;
} ruuvi_gw_cfg_http_auth_basic_t;

typedef union ruuvi_gw_cfg_http_auth_t
{
    ruuvi_gw_cfg_http_auth_basic_t auth_basic;
    struct
    {
        ruuvi_gw_cfg_http_bearer_token_t token;
    } auth_bearer;
    struct
    {
        ruuvi_gw_cfg_http_token_t token;
    } auth_token;
    struct
    {
        ruuvi_gw_cfg_http_apikey_t api_key;
    } auth_apikey;
} ruuvi_gw_cfg_http_auth_t;

typedef uint16_t gw_cfg_remote_refresh_interval_minutes_t;

typedef struct ruuvi_gw_cfg_remote_t
{
    bool                                     use_remote_cfg;
    bool                                     use_ssl_client_cert;
    bool                                     use_ssl_server_cert;
    ruuvi_gw_cfg_http_url_t                  url;
    gw_cfg_http_auth_type_e                  auth_type;
    ruuvi_gw_cfg_http_auth_t                 auth;
    gw_cfg_remote_refresh_interval_minutes_t refresh_interval_minutes;
} ruuvi_gw_cfg_remote_t;

#define GW_CFG_HTTP_DATA_FORMAT_STR_RUUVI           "ruuvi"
#define GW_CFG_HTTP_DATA_FORMAT_STR_RAW_AND_DECODED "ruuvi_raw_and_decoded"
#define GW_CFG_HTTP_DATA_FORMAT_STR_DECODED         "ruuvi_decoded"

#define GW_CFG_HTTP_DATA_FORMAT_STR_SIZE sizeof(GW_CFG_HTTP_DATA_FORMAT_STR_RAW_AND_DECODED)

typedef enum gw_cfg_http_data_format_e
{
    GW_CFG_HTTP_DATA_FORMAT_RUUVI = 0,
    GW_CFG_HTTP_DATA_FORMAT_RUUVI_RAW_AND_DECODED,
    GW_CFG_HTTP_DATA_FORMAT_RUUVI_DECODED,
} gw_cfg_http_data_format_e;

typedef struct ruuvi_gw_cfg_http_t
{
    bool                      use_http_ruuvi;
    bool                      use_http;
    bool                      http_use_ssl_client_cert;
    bool                      http_use_ssl_server_cert;
    ruuvi_gw_cfg_http_url_t   http_url;
    uint32_t                  http_period;
    gw_cfg_http_data_format_e data_format;
    gw_cfg_http_auth_type_e   auth_type;
    ruuvi_gw_cfg_http_auth_t  auth;
} ruuvi_gw_cfg_http_t;

typedef struct ruuvi_gw_cfg_http_stat_t
{
    bool                         use_http_stat;
    bool                         http_stat_use_ssl_client_cert;
    bool                         http_stat_use_ssl_server_cert;
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
    http_server_auth_api_key_t lan_auth_api_key_rw;
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

typedef struct ruuvi_gw_cfg_ntp_server_addr_str_t
{
    char buf[GW_CFG_MAX_NTP_SERVER_LEN];
} ruuvi_gw_cfg_ntp_server_addr_str_t;

typedef struct ruuvi_gw_cfg_ntp_t
{
    bool                               ntp_use;
    bool                               ntp_use_dhcp;
    ruuvi_gw_cfg_ntp_server_addr_str_t ntp_server1;
    ruuvi_gw_cfg_ntp_server_addr_str_t ntp_server2;
    ruuvi_gw_cfg_ntp_server_addr_str_t ntp_server3;
    ruuvi_gw_cfg_ntp_server_addr_str_t ntp_server4;
} ruuvi_gw_cfg_ntp_t;

typedef struct ruuvi_gw_cfg_filter_t
{
    uint16_t company_id;
    bool     company_use_filtering;
} ruuvi_gw_cfg_filter_t;

typedef struct ruuvi_gw_cfg_scan_t
{
    bool scan_coded_phy;
    bool scan_1mbit_phy;
    bool scan_2mbit_phy;
    bool scan_channel_37;
    bool scan_channel_38;
    bool scan_channel_39;
    bool scan_default;
} ruuvi_gw_cfg_scan_t;

typedef struct ruuvi_gw_cfg_scan_filter_t
{
    bool              scan_filter_allow_listed;
    uint32_t          scan_filter_length;
    mac_address_bin_t scan_filter_list[GW_CFG_MAX_NUM_SENSORS];
} ruuvi_gw_cfg_scan_filter_t;

typedef struct ruuvi_gw_cfg_coordinates_t
{
    char buf[GW_CFG_MAX_COORDINATES_STR_LEN];
} ruuvi_gw_cfg_coordinates_t;

typedef struct ruuvi_gw_cfg_fw_update_t
{
    char fw_update_url[GW_CFG_MAX_HTTP_URL_LEN];
} ruuvi_gw_cfg_fw_update_t;

typedef struct ruuvi_gw_cfg_t
{
    ruuvi_gw_cfg_remote_t      remote;
    ruuvi_gw_cfg_http_t        http;
    ruuvi_gw_cfg_http_stat_t   http_stat;
    ruuvi_gw_cfg_mqtt_t        mqtt;
    ruuvi_gw_cfg_lan_auth_t    lan_auth;
    ruuvi_gw_cfg_auto_update_t auto_update;
    ruuvi_gw_cfg_ntp_t         ntp;
    ruuvi_gw_cfg_filter_t      filter;
    ruuvi_gw_cfg_scan_t        scan;
    ruuvi_gw_cfg_scan_filter_t scan_filter;
    ruuvi_gw_cfg_coordinates_t coordinates;
    ruuvi_gw_cfg_fw_update_t   fw_update;
} gw_cfg_ruuvi_t;

typedef struct gw_cfg_t
{
    gw_cfg_device_info_t device_info;
    gw_cfg_ruuvi_t       ruuvi_cfg;
    gw_cfg_eth_t         eth_cfg;
    wifiman_config_t     wifi_cfg;
} gw_cfg_t;

typedef struct gw_cfg_update_status_t
{
    bool flag_ruuvi_cfg_modified;
    bool flag_eth_cfg_modified;
    bool flag_wifi_ap_cfg_modified;
    bool flag_wifi_sta_cfg_modified;
} gw_cfg_update_status_t;

typedef void (*gw_cfg_cb_on_change_cfg)(const gw_cfg_t* const p_gw_cfg);

void
gw_cfg_init(gw_cfg_cb_on_change_cfg p_cb_on_change_cfg);

void
gw_cfg_deinit(void);

bool
gw_cfg_is_initialized(void);

const gw_cfg_t*
gw_cfg_lock_ro(void);

void
gw_cfg_unlock_ro(const gw_cfg_t** const p_p_gw_cfg);

bool
gw_cfg_is_empty(void);

void
gw_cfg_update_eth_cfg(const gw_cfg_eth_t* const p_gw_cfg_eth_new);

void
gw_cfg_update_ruuvi_cfg(const gw_cfg_ruuvi_t* const p_gw_cfg_ruuvi_new);

void
gw_cfg_update_fw_update_url(const char* const p_fw_update_url);

void
gw_cfg_update_wifi_ap_config(const wifiman_config_ap_t* const p_wifi_ap_cfg);

void
gw_cfg_update_wifi_sta_config(const wifiman_config_sta_t* const p_wifi_sta_cfg);

gw_cfg_update_status_t
gw_cfg_update(const gw_cfg_t* const p_gw_cfg);

void
gw_cfg_get_copy(gw_cfg_t* const p_gw_cfg);

bool
gw_cfg_get_remote_cfg_use(gw_cfg_remote_refresh_interval_minutes_t* const p_interval_minutes);

const ruuvi_gw_cfg_remote_t*
gw_cfg_get_remote_cfg_copy(void);

bool
gw_cfg_get_eth_use_eth(void);

void
gw_cfg_set_eth_use_eth(const bool flag_use_eth);

bool
gw_cfg_get_eth_use_dhcp(void);

bool
gw_cfg_get_mqtt_use_mqtt(void);

uint32_t
gw_cfg_get_mqtt_sending_interval(void);

bool
gw_cfg_get_mqtt_use_mqtt_over_ssl_or_wss(void);

bool
gw_cfg_get_http_use_http_ruuvi(void);

bool
gw_cfg_get_http_use_http(void);

uint32_t
gw_cfg_get_http_period(void);

ruuvi_gw_cfg_http_t*
gw_cfg_get_http_copy(void);

str_buf_t
gw_cfg_get_http_password_copy(void);

bool
gw_cfg_get_http_stat_use_http_stat(void);

ruuvi_gw_cfg_http_stat_t*
gw_cfg_get_http_stat_copy(void);

str_buf_t
gw_cfg_get_http_stat_password_copy(void);

ruuvi_gw_cfg_mqtt_t*
gw_cfg_get_mqtt_copy(void);

str_buf_t
gw_cfg_get_mqtt_password_copy(void);

auto_update_cycle_type_e
gw_cfg_get_auto_update_cycle(void);

bool
gw_cfg_get_ntp_use(void);

ruuvi_gw_cfg_coordinates_t
gw_cfg_get_coordinates(void);

const ruuvi_esp32_fw_ver_str_t*
gw_cfg_get_esp32_fw_ver(void);

const ruuvi_nrf52_fw_ver_str_t*
gw_cfg_get_nrf52_fw_ver(void);

const nrf52_device_id_str_t*
gw_cfg_get_nrf52_device_id(void);

const mac_address_str_t*
gw_cfg_get_nrf52_mac_addr(void);

const mac_address_str_t*
gw_cfg_get_esp32_mac_addr_wifi(void);

const mac_address_str_t*
gw_cfg_get_esp32_mac_addr_eth(void);

const wifiman_wifi_ssid_t*
gw_cfg_get_wifi_ap_ssid(void);

const wifiman_hostname_t*
gw_cfg_get_hostname(void);

const char*
gw_cfg_auth_type_to_str(const ruuvi_gw_cfg_lan_auth_t* const p_lan_auth);

wifiman_config_t
gw_cfg_get_wifi_cfg(void);

bool
gw_cfg_is_wifi_sta_configured(void);

str_buf_t
gw_cfg_get_fw_update_url(void);

bool
gw_cfg_ruuvi_cmp(const gw_cfg_ruuvi_t* const p_cfg_ruuvi1, const gw_cfg_ruuvi_t* const p_cfg_ruuvi2);

#ifdef __cplusplus
}
#endif

#endif // RUUVI_GW_CFG_H
