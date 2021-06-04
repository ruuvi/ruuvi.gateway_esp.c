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
#include "cjson_wrap.h"
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

#define RUUVI_GATEWAY_CONFIG_HEADER      (0xAABBU)
#define RUUVI_GATEWAY_CONFIG_FMT_VERSION (0x0008U)

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

typedef struct ruuvi_gw_cfg_mqtt_t
{
    bool     use_mqtt;
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

typedef struct ruuvi_gw_cfg_filter_t
{
    uint16_t company_id;
    bool     company_filter;
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

typedef struct ruuvi_gateway_config_t
{
    uint16_t                header;
    uint16_t                fmt_version;
    ruuvi_gw_cfg_eth_t      eth;
    ruuvi_gw_cfg_mqtt_t     mqtt;
    ruuvi_gw_cfg_http_t     http;
    ruuvi_gw_cfg_lan_auth_t lan_auth;
    ruuvi_gw_cfg_filter_t   filter;
    ruuvi_gw_cfg_scan_t     scan;
    char                    coordinates[MAX_CONFIG_STR_LEN];
} ruuvi_gateway_config_t;

extern ruuvi_gateway_config_t       g_gateway_config;
extern const ruuvi_gateway_config_t g_gateway_config_default;
extern mac_address_bin_t            g_gw_mac_sta;
extern mac_address_str_t            g_gw_mac_sta_str;
extern wifi_ssid_t                  g_gw_wifi_ssid;

void
gw_cfg_print_to_log(const ruuvi_gateway_config_t *p_config);

bool
gw_cfg_generate_json_str(cjson_wrap_str_t *p_json_str);

#ifdef __cplusplus
}
#endif

#endif // RUUVI_GW_CFG_H
