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

#define MAX_CONFIG_STR_LEN  64
#define MAX_HTTP_URL_LEN    256
#define MAX_HTTP_USER_LEN   51
#define MAX_HTTP_PASS_LEN   51
#define MAX_MQTT_SERVER_LEN 256
#define MAX_MQTT_PREFIX_LEN 51
#define MAX_MQTT_USER_LEN   51
#define MAX_MQTT_PASS_LEN   51
#define IP_STR_LEN          17

#define RUUVI_COMPANY_ID 0x0499

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

extern ruuvi_gateway_config_t       g_gateway_config;
extern const ruuvi_gateway_config_t g_gateway_config_default;
extern mac_address_str_t            gw_mac_sta;

void
gw_cfg_print_to_log(const ruuvi_gateway_config_t *p_config);

bool
gw_cfg_generate_json_str(cjson_wrap_str_t *p_json_str);

#ifdef __cplusplus
}
#endif

#endif // RUUVI_GW_CFG_H
