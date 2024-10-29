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

static void
gw_cfg_blob_convert_eth(gw_cfg_eth_t* const p_cfg_dst_eth, const ruuvi_gw_cfg_blob_eth_t* const p_cfg_src_eth)
{
    p_cfg_dst_eth->use_eth  = p_cfg_src_eth->use_eth;
    p_cfg_dst_eth->eth_dhcp = p_cfg_src_eth->eth_dhcp;
    (void)snprintf(
        p_cfg_dst_eth->eth_static_ip.buf,
        sizeof(p_cfg_dst_eth->eth_static_ip.buf),
        "%s",
        p_cfg_src_eth->eth_static_ip);
    (void)snprintf(
        p_cfg_dst_eth->eth_netmask.buf,
        sizeof(p_cfg_dst_eth->eth_netmask.buf),
        "%s",
        p_cfg_src_eth->eth_netmask);
    (void)snprintf(p_cfg_dst_eth->eth_gw.buf, sizeof(p_cfg_dst_eth->eth_gw.buf), "%s", p_cfg_src_eth->eth_gw);
    (void)snprintf(p_cfg_dst_eth->eth_dns1.buf, sizeof(p_cfg_dst_eth->eth_dns1.buf), "%s", p_cfg_src_eth->eth_dns1);
    (void)snprintf(p_cfg_dst_eth->eth_dns2.buf, sizeof(p_cfg_dst_eth->eth_dns2.buf), "%s", p_cfg_src_eth->eth_dns2);
}

static void
gw_cfg_blob_convert_mqtt(
    ruuvi_gw_cfg_mqtt_t* const            p_cfg_dst_mqtt,
    const ruuvi_gw_cfg_blob_mqtt_t* const p_cfg_src_mqtt)
{
    p_cfg_dst_mqtt->use_mqtt = p_cfg_src_mqtt->use_mqtt;
    (void)snprintf(
        p_cfg_dst_mqtt->mqtt_server.buf,
        sizeof(p_cfg_dst_mqtt->mqtt_server.buf),
        "%s",
        p_cfg_src_mqtt->mqtt_server);
    p_cfg_dst_mqtt->mqtt_port = p_cfg_src_mqtt->mqtt_port;
    (void)snprintf(
        p_cfg_dst_mqtt->mqtt_prefix.buf,
        sizeof(p_cfg_dst_mqtt->mqtt_prefix.buf),
        "%s",
        p_cfg_src_mqtt->mqtt_use_default_prefix ? gw_cfg_default_get_mqtt()->mqtt_prefix.buf
                                                : p_cfg_src_mqtt->mqtt_prefix);
    if ('\0' != p_cfg_src_mqtt->mqtt_client_id[0])
    {
        (void)snprintf(
            p_cfg_dst_mqtt->mqtt_client_id.buf,
            sizeof(p_cfg_dst_mqtt->mqtt_client_id.buf),
            "%s",
            p_cfg_src_mqtt->mqtt_client_id);
    }
    else
    {
        (void)snprintf(
            p_cfg_dst_mqtt->mqtt_client_id.buf,
            sizeof(p_cfg_dst_mqtt->mqtt_client_id.buf),
            "%s",
            gw_cfg_default_get_mqtt()->mqtt_client_id.buf);
    }
    (void)snprintf(p_cfg_dst_mqtt->mqtt_user.buf, sizeof(p_cfg_dst_mqtt->mqtt_user), "%s", p_cfg_src_mqtt->mqtt_user);
    (void)snprintf(p_cfg_dst_mqtt->mqtt_pass.buf, sizeof(p_cfg_dst_mqtt->mqtt_pass), "%s", p_cfg_src_mqtt->mqtt_pass);
}

static void
gw_cfg_blob_convert_http(
    ruuvi_gw_cfg_http_t* const            p_cfg_dst_http,
    const ruuvi_gw_cfg_blob_http_t* const p_cfg_src_http)
{
    p_cfg_dst_http->data_format = GW_CFG_HTTP_DATA_FORMAT_RUUVI;

    if (!p_cfg_src_http->use_http)
    {
        p_cfg_dst_http->use_http_ruuvi = false;
        p_cfg_dst_http->use_http       = false;
    }
    else
    {
        if ((0 == strcmp(RUUVI_GATEWAY_HTTP_DEFAULT_URL, p_cfg_src_http->http_url))
            && ('\0' == p_cfg_src_http->http_user[0]))
        {
            p_cfg_dst_http->use_http_ruuvi = true;
            p_cfg_dst_http->use_http       = false;
        }
        else
        {
            p_cfg_dst_http->use_http_ruuvi = false;
            p_cfg_dst_http->use_http       = true;
            (void)snprintf(
                p_cfg_dst_http->http_url.buf,
                sizeof(p_cfg_dst_http->http_url.buf),
                "%s",
                p_cfg_src_http->http_url);
            if ('\0' == p_cfg_src_http->http_user[0])
            {
                p_cfg_dst_http->auth_type = GW_CFG_HTTP_AUTH_TYPE_NONE;
            }
            else
            {
                p_cfg_dst_http->auth_type = GW_CFG_HTTP_AUTH_TYPE_BASIC;
                (void)snprintf(
                    p_cfg_dst_http->auth.auth_basic.user.buf,
                    sizeof(p_cfg_dst_http->auth.auth_basic.user.buf),
                    "%s",
                    p_cfg_src_http->http_user);
                (void)snprintf(
                    p_cfg_dst_http->auth.auth_basic.password.buf,
                    sizeof(p_cfg_dst_http->auth.auth_basic.password.buf),
                    "%s",
                    p_cfg_src_http->http_pass);
            }
        }
    }
}

static void
gw_cfg_blob_convert_lan_auth(
    ruuvi_gw_cfg_lan_auth_t* const            p_cfg_dst_lan_auth,
    const ruuvi_gw_cfg_blob_lan_auth_t* const p_cfg_src_lan_auth)
{
    p_cfg_dst_lan_auth->lan_auth_type = HTTP_SERVER_AUTH_TYPE_DENY;
    if (0 == strcmp(RUUVI_GW_CFG_BLOB_AUTH_TYPE_STR_ALLOW, p_cfg_src_lan_auth->lan_auth_type))
    {
        p_cfg_dst_lan_auth->lan_auth_type = HTTP_SERVER_AUTH_TYPE_ALLOW;
    }
    else if (0 == strcmp(RUUVI_GW_CFG_BLOB_AUTH_TYPE_STR_BASIC, p_cfg_src_lan_auth->lan_auth_type))
    {
        p_cfg_dst_lan_auth->lan_auth_type = HTTP_SERVER_AUTH_TYPE_BASIC;
    }
    else if (0 == strcmp(RUUVI_GW_CFG_BLOB_AUTH_TYPE_STR_DIGEST, p_cfg_src_lan_auth->lan_auth_type))
    {
        p_cfg_dst_lan_auth->lan_auth_type = HTTP_SERVER_AUTH_TYPE_DIGEST;
    }
    else if (0 == strcmp(RUUVI_GW_CFG_BLOB_AUTH_TYPE_STR_RUUVI, p_cfg_src_lan_auth->lan_auth_type))
    {
        const ruuvi_gw_cfg_lan_auth_t* const p_default_lan_auth = gw_cfg_default_get_lan_auth();
        if ((0 == strcmp(p_cfg_src_lan_auth->lan_auth_user, p_default_lan_auth->lan_auth_user.buf))
            && (0 == strcmp(p_cfg_src_lan_auth->lan_auth_pass, p_default_lan_auth->lan_auth_pass.buf)))
        {
            p_cfg_dst_lan_auth->lan_auth_type = HTTP_SERVER_AUTH_TYPE_DEFAULT;
        }
        else
        {
            p_cfg_dst_lan_auth->lan_auth_type = HTTP_SERVER_AUTH_TYPE_RUUVI;
        }
    }
    else if (0 == strcmp(RUUVI_GW_CFG_BLOB_AUTH_TYPE_STR_DENY, p_cfg_src_lan_auth->lan_auth_type))
    {
        p_cfg_dst_lan_auth->lan_auth_type = HTTP_SERVER_AUTH_TYPE_DENY;
    }
    else
    {
        p_cfg_dst_lan_auth->lan_auth_type = HTTP_SERVER_AUTH_TYPE_DENY;
    }
    (void)snprintf(
        p_cfg_dst_lan_auth->lan_auth_user.buf,
        sizeof(p_cfg_dst_lan_auth->lan_auth_user.buf),
        "%s",
        p_cfg_src_lan_auth->lan_auth_user);
    (void)snprintf(
        p_cfg_dst_lan_auth->lan_auth_pass.buf,
        sizeof(p_cfg_dst_lan_auth->lan_auth_pass.buf),
        "%s",
        p_cfg_src_lan_auth->lan_auth_pass);
}

static void
gw_cfg_blob_convert_auto_update(
    ruuvi_gw_cfg_auto_update_t* const            p_cfg_dst_auto_update,
    const ruuvi_gw_cfg_blob_auto_update_t* const p_cfg_src_auto_update)
{
    switch (p_cfg_src_auto_update->auto_update_cycle)
    {
        case RUUVI_GW_CFG_BLOB_AUTO_UPDATE_CYCLE_TYPE_REGULAR:
            p_cfg_dst_auto_update->auto_update_cycle = AUTO_UPDATE_CYCLE_TYPE_REGULAR;
            break;
        case RUUVI_GW_CFG_BLOB_AUTO_UPDATE_CYCLE_TYPE_BETA_TESTER:
            p_cfg_dst_auto_update->auto_update_cycle = AUTO_UPDATE_CYCLE_TYPE_BETA_TESTER;
            break;
        case RUUVI_GW_CFG_BLOB_AUTO_UPDATE_CYCLE_TYPE_MANUAL:
            p_cfg_dst_auto_update->auto_update_cycle = AUTO_UPDATE_CYCLE_TYPE_MANUAL;
            break;
    }
    p_cfg_dst_auto_update->auto_update_weekdays_bitmask = p_cfg_src_auto_update->auto_update_weekdays_bitmask;
    p_cfg_dst_auto_update->auto_update_interval_from    = p_cfg_src_auto_update->auto_update_interval_from;
    p_cfg_dst_auto_update->auto_update_interval_to      = p_cfg_src_auto_update->auto_update_interval_to;
    p_cfg_dst_auto_update->auto_update_tz_offset_hours  = p_cfg_src_auto_update->auto_update_tz_offset_hours;
}

static void
gw_cfg_blob_convert_filter(
    ruuvi_gw_cfg_filter_t* const            p_cfg_dst_filter,
    const ruuvi_gw_cfg_blob_filter_t* const p_cfg_src_filter)
{
    p_cfg_dst_filter->company_id            = p_cfg_src_filter->company_id;
    p_cfg_dst_filter->company_use_filtering = p_cfg_src_filter->company_filter;
}

static void
gw_cfg_blob_convert_scan(
    ruuvi_gw_cfg_scan_t* const            p_cfg_dst_scan,
    const ruuvi_gw_cfg_blob_scan_t* const p_cfg_src_scan)
{
    p_cfg_dst_scan->scan_coded_phy  = p_cfg_src_scan->scan_coded_phy;
    p_cfg_dst_scan->scan_1mbit_phy  = p_cfg_src_scan->scan_1mbit_phy;
    p_cfg_dst_scan->scan_2mbit_phy  = p_cfg_src_scan->scan_2mbit_phy;
    p_cfg_dst_scan->scan_channel_37 = p_cfg_src_scan->scan_channel_37;
    p_cfg_dst_scan->scan_channel_38 = p_cfg_src_scan->scan_channel_38;
    p_cfg_dst_scan->scan_channel_39 = p_cfg_src_scan->scan_channel_39;
    p_cfg_dst_scan->scan_default    = false;
}

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

    gw_cfg_blob_convert_eth(&p_cfg_dst->eth_cfg, &p_cfg_src->eth);
    gw_cfg_blob_convert_mqtt(&p_cfg_dst->ruuvi_cfg.mqtt, &p_cfg_src->mqtt);
    gw_cfg_blob_convert_http(&p_cfg_dst->ruuvi_cfg.http, &p_cfg_src->http);
    gw_cfg_blob_convert_lan_auth(&p_cfg_dst->ruuvi_cfg.lan_auth, &p_cfg_src->lan_auth);
    gw_cfg_blob_convert_auto_update(&p_cfg_dst->ruuvi_cfg.auto_update, &p_cfg_src->auto_update);
    gw_cfg_blob_convert_filter(&p_cfg_dst->ruuvi_cfg.filter, &p_cfg_src->filter);
    gw_cfg_blob_convert_scan(&p_cfg_dst->ruuvi_cfg.scan, &p_cfg_src->scan);

    (void)snprintf(
        p_cfg_dst->ruuvi_cfg.coordinates.buf,
        sizeof(p_cfg_dst->ruuvi_cfg.coordinates.buf),
        "%s",
        p_cfg_src->coordinates);
}
