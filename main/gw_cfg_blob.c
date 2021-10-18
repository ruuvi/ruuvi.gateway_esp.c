/**
 * @file gw_cfg_blob.c
 * @author TheSomeMan
 * @date 2021-09-29
 * @copyright Ruuvi Innovations Ltd, license BSD-3-Clause.
 */

#include "gw_cfg_blob.h"
#include "gw_cfg_default.h"
#include <stdio.h>

void
gw_cfg_blob_convert(ruuvi_gateway_config_t *const p_cfg_dst, const ruuvi_gateway_config_blob_t *const p_cfg_src)
{
    *p_cfg_dst = g_gateway_config_default;

    if (RUUVI_GATEWAY_CONFIG_BLOB_HEADER != p_cfg_src->header)
    {
        return;
    }
    if (RUUVI_GATEWAY_CONFIG_BLOB_FMT_VERSION != p_cfg_src->fmt_version)
    {
        return;
    }

    p_cfg_dst->eth.use_eth  = p_cfg_src->eth.use_eth;
    p_cfg_dst->eth.eth_dhcp = p_cfg_src->eth.eth_dhcp;
    snprintf(p_cfg_dst->eth.eth_static_ip, sizeof(p_cfg_dst->eth.eth_static_ip), "%s", p_cfg_src->eth.eth_static_ip);
    snprintf(p_cfg_dst->eth.eth_netmask, sizeof(p_cfg_dst->eth.eth_netmask), "%s", p_cfg_src->eth.eth_netmask);
    snprintf(p_cfg_dst->eth.eth_gw, sizeof(p_cfg_dst->eth.eth_gw), "%s", p_cfg_src->eth.eth_gw);
    snprintf(p_cfg_dst->eth.eth_dns1, sizeof(p_cfg_dst->eth.eth_dns1), "%s", p_cfg_src->eth.eth_dns1);
    snprintf(p_cfg_dst->eth.eth_dns2, sizeof(p_cfg_dst->eth.eth_dns2), "%s", p_cfg_src->eth.eth_dns2);

    p_cfg_dst->mqtt.use_mqtt                = p_cfg_src->mqtt.use_mqtt;
    p_cfg_dst->mqtt.mqtt_use_default_prefix = p_cfg_src->mqtt.mqtt_use_default_prefix;
    snprintf(
        p_cfg_dst->mqtt.mqtt_server.buf,
        sizeof(p_cfg_dst->mqtt.mqtt_server.buf),
        "%s",
        p_cfg_src->mqtt.mqtt_server);
    p_cfg_dst->mqtt.mqtt_port = p_cfg_src->mqtt.mqtt_port;
    snprintf(
        p_cfg_dst->mqtt.mqtt_prefix.buf,
        sizeof(p_cfg_dst->mqtt.mqtt_prefix.buf),
        "%s",
        p_cfg_src->mqtt.mqtt_prefix);
    snprintf(
        p_cfg_dst->mqtt.mqtt_client_id.buf,
        sizeof(p_cfg_dst->mqtt.mqtt_client_id.buf),
        "%s",
        p_cfg_src->mqtt.mqtt_client_id);
    snprintf(p_cfg_dst->mqtt.mqtt_user.buf, sizeof(p_cfg_dst->mqtt.mqtt_user), "%s", p_cfg_src->mqtt.mqtt_user);
    snprintf(p_cfg_dst->mqtt.mqtt_pass.buf, sizeof(p_cfg_dst->mqtt.mqtt_pass), "%s", p_cfg_src->mqtt.mqtt_pass);

    p_cfg_dst->http.use_http = p_cfg_src->http.use_http;
    snprintf(p_cfg_dst->http.http_url.buf, sizeof(p_cfg_dst->http.http_url.buf), "%s", p_cfg_src->http.http_url);
    snprintf(p_cfg_dst->http.http_user.buf, sizeof(p_cfg_dst->http.http_user.buf), "%s", p_cfg_src->http.http_user);
    snprintf(p_cfg_dst->http.http_pass.buf, sizeof(p_cfg_dst->http.http_pass.buf), "%s", p_cfg_src->http.http_pass);

    snprintf(
        p_cfg_dst->lan_auth.lan_auth_type,
        sizeof(p_cfg_dst->lan_auth.lan_auth_type),
        "%s",
        p_cfg_src->lan_auth.lan_auth_type);
    snprintf(
        p_cfg_dst->lan_auth.lan_auth_user,
        sizeof(p_cfg_dst->lan_auth.lan_auth_user),
        "%s",
        p_cfg_src->lan_auth.lan_auth_user);
    snprintf(
        p_cfg_dst->lan_auth.lan_auth_pass,
        sizeof(p_cfg_dst->lan_auth.lan_auth_pass),
        "%s",
        p_cfg_src->lan_auth.lan_auth_pass);

    switch (p_cfg_src->auto_update.auto_update_cycle)
    {
        case RUUVI_GW_CFG_BLOB_AUTO_UPDATE_CYCLE_TYPE_REGULAR:
            p_cfg_dst->auto_update.auto_update_cycle = AUTO_UPDATE_CYCLE_TYPE_REGULAR;
            break;
        case RUUVI_GW_CFG_BLOB_AUTO_UPDATE_CYCLE_TYPE_BETA_TESTER:
            p_cfg_dst->auto_update.auto_update_cycle = AUTO_UPDATE_CYCLE_TYPE_BETA_TESTER;
            break;
        case RUUVI_GW_CFG_BLOB_AUTO_UPDATE_CYCLE_TYPE_MANUAL:
            p_cfg_dst->auto_update.auto_update_cycle = AUTO_UPDATE_CYCLE_TYPE_MANUAL;
            break;
    }
    p_cfg_dst->auto_update.auto_update_weekdays_bitmask = p_cfg_src->auto_update.auto_update_weekdays_bitmask;
    p_cfg_dst->auto_update.auto_update_interval_from    = p_cfg_src->auto_update.auto_update_interval_from;
    p_cfg_dst->auto_update.auto_update_interval_to      = p_cfg_src->auto_update.auto_update_interval_to;
    p_cfg_dst->auto_update.auto_update_tz_offset_hours  = p_cfg_src->auto_update.auto_update_tz_offset_hours;

    p_cfg_dst->filter.company_id            = p_cfg_src->filter.company_id;
    p_cfg_dst->filter.company_use_filtering = p_cfg_src->filter.company_filter;

    p_cfg_dst->scan.scan_coded_phy        = p_cfg_src->scan.scan_coded_phy;
    p_cfg_dst->scan.scan_1mbit_phy        = p_cfg_src->scan.scan_1mbit_phy;
    p_cfg_dst->scan.scan_extended_payload = p_cfg_src->scan.scan_extended_payload;
    p_cfg_dst->scan.scan_channel_37       = p_cfg_src->scan.scan_channel_37;
    p_cfg_dst->scan.scan_channel_38       = p_cfg_src->scan.scan_channel_38;
    p_cfg_dst->scan.scan_channel_39       = p_cfg_src->scan.scan_channel_39;

    snprintf(p_cfg_dst->coordinates.buf, sizeof(p_cfg_dst->coordinates.buf), "%s", p_cfg_src->coordinates);
}
