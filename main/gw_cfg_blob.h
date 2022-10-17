/**
 * @file gw_cfg_blob.h
 * @author TheSomeMan
 * @date 2021-09-29
 * @copyright Ruuvi Innovations Ltd, license BSD-3-Clause.
 */

#ifndef RUUVI_GW_CFG_BLOB_H
#define RUUVI_GW_CFG_BLOB_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define RUUVI_GW_CFG_BLOB_MAX_CONFIG_STR_LEN     64
#define RUUVI_GW_CFG_BLOB_MAX_HTTP_URL_LEN       256
#define RUUVI_GW_CFG_BLOB_MAX_HTTP_USER_LEN      51
#define RUUVI_GW_CFG_BLOB_MAX_HTTP_PASS_LEN      51
#define RUUVI_GW_CFG_BLOB_MAX_MQTT_SERVER_LEN    256
#define RUUVI_GW_CFG_BLOB_MAX_MQTT_PREFIX_LEN    51
#define RUUVI_GW_CFG_BLOB_MAX_MQTT_USER_LEN      51
#define RUUVI_GW_CFG_BLOB_MAX_MQTT_PASS_LEN      51
#define RUUVI_GW_CFG_BLOB_MAX_MQTT_CLIENT_ID_LEN 51
#define RUUVI_GW_CFG_BLOB_MAX_LAN_AUTH_TYPE_LEN  16
#define RUUVI_GW_CFG_BLOB_MAX_LAN_AUTH_USER_LEN  51
#define RUUVI_GW_CFG_BLOB_MAX_LAN_AUTH_PASS_LEN  51
#define RUUVI_GW_CFG_BLOB_IP_STR_LEN             17

#define RUUVI_GATEWAY_CONFIG_BLOB_HEADER      (0xAABBU)
#define RUUVI_GATEWAY_CONFIG_BLOB_FMT_VERSION (0x0008U)

#define RUUVI_GW_CFG_BLOB_AUTH_TYPE_STR_ALLOW  "lan_auth_allow"
#define RUUVI_GW_CFG_BLOB_AUTH_TYPE_STR_BASIC  "lan_auth_basic"
#define RUUVI_GW_CFG_BLOB_AUTH_TYPE_STR_DIGEST "lan_auth_digest"
#define RUUVI_GW_CFG_BLOB_AUTH_TYPE_STR_RUUVI  "lan_auth_ruuvi"
#define RUUVI_GW_CFG_BLOB_AUTH_TYPE_STR_DENY   "lan_auth_deny"

typedef struct ruuvi_gw_cfg_blob_eth_t
{
    bool use_eth;
    bool eth_dhcp;
    char eth_static_ip[RUUVI_GW_CFG_BLOB_IP_STR_LEN];
    char eth_netmask[RUUVI_GW_CFG_BLOB_IP_STR_LEN];
    char eth_gw[RUUVI_GW_CFG_BLOB_IP_STR_LEN];
    char eth_dns1[RUUVI_GW_CFG_BLOB_IP_STR_LEN];
    char eth_dns2[RUUVI_GW_CFG_BLOB_IP_STR_LEN];
} ruuvi_gw_cfg_blob_eth_t;

typedef struct ruuvi_gw_cfg_blob_mqtt_t
{
    bool     use_mqtt;
    bool     mqtt_use_default_prefix;
    char     mqtt_server[RUUVI_GW_CFG_BLOB_MAX_MQTT_SERVER_LEN];
    uint16_t mqtt_port;
    char     mqtt_prefix[RUUVI_GW_CFG_BLOB_MAX_MQTT_PREFIX_LEN];
    char     mqtt_client_id[RUUVI_GW_CFG_BLOB_MAX_MQTT_CLIENT_ID_LEN];
    char     mqtt_user[RUUVI_GW_CFG_BLOB_MAX_MQTT_USER_LEN];
    char     mqtt_pass[RUUVI_GW_CFG_BLOB_MAX_MQTT_PASS_LEN];
} ruuvi_gw_cfg_blob_mqtt_t;

typedef struct ruuvi_gw_cfg_blob_http_t
{
    bool use_http;
    char http_url[RUUVI_GW_CFG_BLOB_MAX_HTTP_URL_LEN];
    char http_user[RUUVI_GW_CFG_BLOB_MAX_HTTP_USER_LEN];
    char http_pass[RUUVI_GW_CFG_BLOB_MAX_HTTP_PASS_LEN];
} ruuvi_gw_cfg_blob_http_t;

typedef struct ruuvi_gw_cfg_blob_lan_auth_t
{
    char lan_auth_type[RUUVI_GW_CFG_BLOB_MAX_LAN_AUTH_TYPE_LEN];
    char lan_auth_user[RUUVI_GW_CFG_BLOB_MAX_LAN_AUTH_USER_LEN];
    char lan_auth_pass[RUUVI_GW_CFG_BLOB_MAX_LAN_AUTH_PASS_LEN];
} ruuvi_gw_cfg_blob_lan_auth_t;

typedef enum ruuvi_gw_cfg_blob_auto_update_cycle_type_e
{
    RUUVI_GW_CFG_BLOB_AUTO_UPDATE_CYCLE_TYPE_REGULAR     = 0,
    RUUVI_GW_CFG_BLOB_AUTO_UPDATE_CYCLE_TYPE_BETA_TESTER = 1,
    RUUVI_GW_CFG_BLOB_AUTO_UPDATE_CYCLE_TYPE_MANUAL      = 2,
} ruuvi_gw_cfg_blob_auto_update_cycle_type_e;

typedef uint8_t ruuvi_gw_cfg_blob_auto_update_weekdays_bitmask_t;
typedef uint8_t ruuvi_gw_cfg_blob_auto_update_interval_hours_t;
typedef int8_t  ruuvi_gw_cfg_blob_auto_update_tz_offset_hours_t;

typedef struct ruuvi_gw_cfg_blob_auto_update_t
{
    ruuvi_gw_cfg_blob_auto_update_cycle_type_e       auto_update_cycle;
    ruuvi_gw_cfg_blob_auto_update_weekdays_bitmask_t auto_update_weekdays_bitmask;
    ruuvi_gw_cfg_blob_auto_update_interval_hours_t   auto_update_interval_from;
    ruuvi_gw_cfg_blob_auto_update_interval_hours_t   auto_update_interval_to;
    ruuvi_gw_cfg_blob_auto_update_tz_offset_hours_t  auto_update_tz_offset_hours;
} ruuvi_gw_cfg_blob_auto_update_t;

typedef struct ruuvi_gw_cfg_blob_filter_t
{
    uint16_t company_id;
    bool     company_filter;
} ruuvi_gw_cfg_blob_filter_t;

typedef struct ruuvi_gw_cfg_blob_scan_t
{
    bool scan_coded_phy;
    bool scan_1mbit_phy;
    bool scan_extended_payload;
    bool scan_channel_37;
    bool scan_channel_38;
    bool scan_channel_39;
} ruuvi_gw_cfg_blob_scan_t;

typedef struct ruuvi_gateway_config_blob_t
{
    uint16_t                        header;
    uint16_t                        fmt_version;
    ruuvi_gw_cfg_blob_eth_t         eth;
    ruuvi_gw_cfg_blob_mqtt_t        mqtt;
    ruuvi_gw_cfg_blob_http_t        http;
    ruuvi_gw_cfg_blob_lan_auth_t    lan_auth;
    ruuvi_gw_cfg_blob_auto_update_t auto_update;
    ruuvi_gw_cfg_blob_filter_t      filter;
    ruuvi_gw_cfg_blob_scan_t        scan;
    char                            coordinates[RUUVI_GW_CFG_BLOB_MAX_CONFIG_STR_LEN];
} ruuvi_gateway_config_blob_t;

typedef struct gw_cfg_t gw_cfg_t;

void
gw_cfg_blob_convert(gw_cfg_t* const p_cfg_dst, const ruuvi_gateway_config_blob_t* const p_cfg_src);

#ifdef __cplusplus
}
#endif

#endif // RUUVI_GW_CFG_BLOB_H
