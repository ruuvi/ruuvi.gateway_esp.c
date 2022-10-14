/**
 * @file gw_cfg_blob.c
 * @author TheSomeMan
 * @date 2021-09-29
 * @copyright Ruuvi Innovations Ltd, license BSD-3-Clause.
 */

#include "gw_cfg_blob.h"
#include "gw_cfg_default.h"
#include "gw_cfg.h"
#include <stdio.h>
#include <string.h>

void
gw_cfg_blob_convert(gw_cfg_t* const p_cfg_dst, const ruuvi_gateway_config_blob_t* const p_cfg_src)
{
    gw_cfg_default_get(p_cfg_dst);

    if (RUUVI_GATEWAY_CONFIG_BLOB_HEADER != p_cfg_src->header)
    {
        return;
    }
    if (RUUVI_GATEWAY_CONFIG_BLOB_FMT_VERSION != p_cfg_src->fmt_version)
    {
        return;
    }

    gw_cfg_eth_t* const p_eth_cfg_dst = &p_cfg_dst->eth_cfg;
    p_eth_cfg_dst->use_eth            = p_cfg_src->eth.use_eth;
    p_eth_cfg_dst->eth_dhcp           = p_cfg_src->eth.eth_dhcp;
    snprintf(
        p_eth_cfg_dst->eth_static_ip.buf,
        sizeof(p_eth_cfg_dst->eth_static_ip.buf),
        "%s",
        p_cfg_src->eth.eth_static_ip);
    snprintf(p_eth_cfg_dst->eth_netmask.buf, sizeof(p_eth_cfg_dst->eth_netmask.buf), "%s", p_cfg_src->eth.eth_netmask);
    snprintf(p_eth_cfg_dst->eth_gw.buf, sizeof(p_eth_cfg_dst->eth_gw.buf), "%s", p_cfg_src->eth.eth_gw);
    snprintf(p_eth_cfg_dst->eth_dns1.buf, sizeof(p_eth_cfg_dst->eth_dns1.buf), "%s", p_cfg_src->eth.eth_dns1);
    snprintf(p_eth_cfg_dst->eth_dns2.buf, sizeof(p_eth_cfg_dst->eth_dns2.buf), "%s", p_cfg_src->eth.eth_dns2);

    gw_cfg_ruuvi_t* const p_ruufi_cfg_dst = &p_cfg_dst->ruuvi_cfg;
    p_ruufi_cfg_dst->mqtt.use_mqtt        = p_cfg_src->mqtt.use_mqtt;
    snprintf(
        p_ruufi_cfg_dst->mqtt.mqtt_server.buf,
        sizeof(p_ruufi_cfg_dst->mqtt.mqtt_server.buf),
        "%s",
        p_cfg_src->mqtt.mqtt_server);
    p_ruufi_cfg_dst->mqtt.mqtt_port = p_cfg_src->mqtt.mqtt_port;
    snprintf(
        p_ruufi_cfg_dst->mqtt.mqtt_prefix.buf,
        sizeof(p_ruufi_cfg_dst->mqtt.mqtt_prefix.buf),
        "%s",
        p_cfg_src->mqtt.mqtt_use_default_prefix ? gw_cfg_default_get_mqtt()->mqtt_prefix.buf
                                                : p_cfg_src->mqtt.mqtt_prefix);
    if ('\0' != p_cfg_src->mqtt.mqtt_client_id[0])
    {
        snprintf(
            p_ruufi_cfg_dst->mqtt.mqtt_client_id.buf,
            sizeof(p_ruufi_cfg_dst->mqtt.mqtt_client_id.buf),
            "%s",
            p_cfg_src->mqtt.mqtt_client_id);
    }
    else
    {
        snprintf(
            p_ruufi_cfg_dst->mqtt.mqtt_client_id.buf,
            sizeof(p_ruufi_cfg_dst->mqtt.mqtt_client_id.buf),
            "%s",
            gw_cfg_default_get_mqtt()->mqtt_client_id.buf);
    }
    snprintf(
        p_ruufi_cfg_dst->mqtt.mqtt_user.buf,
        sizeof(p_ruufi_cfg_dst->mqtt.mqtt_user),
        "%s",
        p_cfg_src->mqtt.mqtt_user);
    snprintf(
        p_ruufi_cfg_dst->mqtt.mqtt_pass.buf,
        sizeof(p_ruufi_cfg_dst->mqtt.mqtt_pass),
        "%s",
        p_cfg_src->mqtt.mqtt_pass);

    p_ruufi_cfg_dst->http.use_http = p_cfg_src->http.use_http;
    snprintf(
        p_ruufi_cfg_dst->http.http_url.buf,
        sizeof(p_ruufi_cfg_dst->http.http_url.buf),
        "%s",
        p_cfg_src->http.http_url);
    snprintf(
        p_ruufi_cfg_dst->http.http_user.buf,
        sizeof(p_ruufi_cfg_dst->http.http_user.buf),
        "%s",
        p_cfg_src->http.http_user);
    snprintf(
        p_ruufi_cfg_dst->http.http_pass.buf,
        sizeof(p_ruufi_cfg_dst->http.http_pass.buf),
        "%s",
        p_cfg_src->http.http_pass);

    p_ruufi_cfg_dst->lan_auth.lan_auth_type = HTTP_SERVER_AUTH_TYPE_DENY;
    if (0 == strcmp(RUUVI_GW_CFG_BLOB_AUTH_TYPE_STR_ALLOW, p_cfg_src->lan_auth.lan_auth_type))
    {
        p_ruufi_cfg_dst->lan_auth.lan_auth_type = HTTP_SERVER_AUTH_TYPE_ALLOW;
    }
    else if (0 == strcmp(RUUVI_GW_CFG_BLOB_AUTH_TYPE_STR_BASIC, p_cfg_src->lan_auth.lan_auth_type))
    {
        p_ruufi_cfg_dst->lan_auth.lan_auth_type = HTTP_SERVER_AUTH_TYPE_BASIC;
    }
    else if (0 == strcmp(RUUVI_GW_CFG_BLOB_AUTH_TYPE_STR_DIGEST, p_cfg_src->lan_auth.lan_auth_type))
    {
        p_ruufi_cfg_dst->lan_auth.lan_auth_type = HTTP_SERVER_AUTH_TYPE_DIGEST;
    }
    else if (0 == strcmp(RUUVI_GW_CFG_BLOB_AUTH_TYPE_STR_RUUVI, p_cfg_src->lan_auth.lan_auth_type))
    {
        const ruuvi_gw_cfg_lan_auth_t* const p_default_lan_auth = gw_cfg_default_get_lan_auth();
        if ((0 == strcmp(p_cfg_src->lan_auth.lan_auth_user, p_default_lan_auth->lan_auth_user.buf))
            && (0 == strcmp(p_cfg_src->lan_auth.lan_auth_pass, p_default_lan_auth->lan_auth_pass.buf)))
        {
            p_ruufi_cfg_dst->lan_auth.lan_auth_type = HTTP_SERVER_AUTH_TYPE_DEFAULT;
        }
        else
        {
            p_ruufi_cfg_dst->lan_auth.lan_auth_type = HTTP_SERVER_AUTH_TYPE_RUUVI;
        }
    }
    else if (0 == strcmp(RUUVI_GW_CFG_BLOB_AUTH_TYPE_STR_DENY, p_cfg_src->lan_auth.lan_auth_type))
    {
        p_ruufi_cfg_dst->lan_auth.lan_auth_type = HTTP_SERVER_AUTH_TYPE_DENY;
    }
    else
    {
        p_ruufi_cfg_dst->lan_auth.lan_auth_type = HTTP_SERVER_AUTH_TYPE_DENY;
    }
    snprintf(
        p_ruufi_cfg_dst->lan_auth.lan_auth_user.buf,
        sizeof(p_ruufi_cfg_dst->lan_auth.lan_auth_user.buf),
        "%s",
        p_cfg_src->lan_auth.lan_auth_user);
    snprintf(
        p_ruufi_cfg_dst->lan_auth.lan_auth_pass.buf,
        sizeof(p_ruufi_cfg_dst->lan_auth.lan_auth_pass.buf),
        "%s",
        p_cfg_src->lan_auth.lan_auth_pass);

    switch (p_cfg_src->auto_update.auto_update_cycle)
    {
        case RUUVI_GW_CFG_BLOB_AUTO_UPDATE_CYCLE_TYPE_REGULAR:
            p_ruufi_cfg_dst->auto_update.auto_update_cycle = AUTO_UPDATE_CYCLE_TYPE_REGULAR;
            break;
        case RUUVI_GW_CFG_BLOB_AUTO_UPDATE_CYCLE_TYPE_BETA_TESTER:
            p_ruufi_cfg_dst->auto_update.auto_update_cycle = AUTO_UPDATE_CYCLE_TYPE_BETA_TESTER;
            break;
        case RUUVI_GW_CFG_BLOB_AUTO_UPDATE_CYCLE_TYPE_MANUAL:
            p_ruufi_cfg_dst->auto_update.auto_update_cycle = AUTO_UPDATE_CYCLE_TYPE_MANUAL;
            break;
    }
    p_ruufi_cfg_dst->auto_update.auto_update_weekdays_bitmask = p_cfg_src->auto_update.auto_update_weekdays_bitmask;
    p_ruufi_cfg_dst->auto_update.auto_update_interval_from    = p_cfg_src->auto_update.auto_update_interval_from;
    p_ruufi_cfg_dst->auto_update.auto_update_interval_to      = p_cfg_src->auto_update.auto_update_interval_to;
    p_ruufi_cfg_dst->auto_update.auto_update_tz_offset_hours  = p_cfg_src->auto_update.auto_update_tz_offset_hours;

    p_ruufi_cfg_dst->filter.company_id            = p_cfg_src->filter.company_id;
    p_ruufi_cfg_dst->filter.company_use_filtering = p_cfg_src->filter.company_filter;

    p_ruufi_cfg_dst->scan.scan_coded_phy        = p_cfg_src->scan.scan_coded_phy;
    p_ruufi_cfg_dst->scan.scan_1mbit_phy        = p_cfg_src->scan.scan_1mbit_phy;
    p_ruufi_cfg_dst->scan.scan_extended_payload = p_cfg_src->scan.scan_extended_payload;
    p_ruufi_cfg_dst->scan.scan_channel_37       = p_cfg_src->scan.scan_channel_37;
    p_ruufi_cfg_dst->scan.scan_channel_38       = p_cfg_src->scan.scan_channel_38;
    p_ruufi_cfg_dst->scan.scan_channel_39       = p_cfg_src->scan.scan_channel_39;

    snprintf(p_ruufi_cfg_dst->coordinates.buf, sizeof(p_ruufi_cfg_dst->coordinates.buf), "%s", p_cfg_src->coordinates);
}
