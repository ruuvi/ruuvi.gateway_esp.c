/**
 * @file gw_cfg_cmp.c
 * @author TheSomeMan
 * @date 2023-07-30
 * @copyright Ruuvi Innovations Ltd, license BSD-3-Clause.
 */

#include "gw_cfg.h"
#include <string.h>

static bool
ruuvi_gw_cfg_remote_cmp_auth_basic(
    const ruuvi_gw_cfg_remote_t* const p_remote1,
    const ruuvi_gw_cfg_remote_t* const p_remote2)
{
    if (0 != strcmp(p_remote1->auth.auth_basic.user.buf, p_remote2->auth.auth_basic.user.buf))
    {
        return false;
    }
    if (0 != strcmp(p_remote1->auth.auth_basic.password.buf, p_remote2->auth.auth_basic.password.buf))
    {
        return false;
    }
    return true;
}

static bool
ruuvi_gw_cfg_remote_cmp(const ruuvi_gw_cfg_remote_t* const p_remote1, const ruuvi_gw_cfg_remote_t* const p_remote2)
{
    if (p_remote1->use_remote_cfg != p_remote2->use_remote_cfg)
    {
        return false;
    }
    if (0 != strcmp(p_remote1->url.buf, p_remote2->url.buf))
    {
        return false;
    }
    if (p_remote1->auth_type != p_remote2->auth_type)
    {
        return false;
    }
    switch (p_remote1->auth_type)
    {
        case GW_CFG_HTTP_AUTH_TYPE_NONE:
            break;
        case GW_CFG_HTTP_AUTH_TYPE_BASIC:
            if (!ruuvi_gw_cfg_remote_cmp_auth_basic(p_remote1, p_remote2))
            {
                return false;
            }
            break;
        case GW_CFG_HTTP_AUTH_TYPE_BEARER:
            if (0 != strcmp(p_remote1->auth.auth_bearer.token.buf, p_remote2->auth.auth_bearer.token.buf))
            {
                return false;
            }
            break;
        case GW_CFG_HTTP_AUTH_TYPE_TOKEN:
            if (0 != strcmp(p_remote1->auth.auth_token.token.buf, p_remote2->auth.auth_token.token.buf))
            {
                return false;
            }
            break;
        case GW_CFG_HTTP_AUTH_TYPE_APIKEY:
            if (0 != strcmp(p_remote1->auth.auth_apikey.api_key.buf, p_remote2->auth.auth_apikey.api_key.buf))
            {
                return false;
            }
            break;
    }
    if (p_remote1->refresh_interval_minutes != p_remote2->refresh_interval_minutes)
    {
        return false;
    }
    if (p_remote1->use_ssl_client_cert != p_remote2->use_ssl_client_cert)
    {
        return false;
    }
    if (p_remote1->use_ssl_server_cert != p_remote2->use_ssl_server_cert)
    {
        return false;
    }
    return true;
}

static bool
ruuvi_gw_cfg_http_cmp_auth_basic(
    const ruuvi_gw_cfg_http_auth_basic_t* const p_auth_basic1,
    const ruuvi_gw_cfg_http_auth_basic_t* const p_auth_basic2)
{
    if (0 != strcmp(p_auth_basic1->user.buf, p_auth_basic2->user.buf))
    {
        return false;
    }
    if (0 != strcmp(p_auth_basic1->password.buf, p_auth_basic2->password.buf))
    {
        return false;
    }
    return true;
}

static bool
ruuvi_gw_cfg_http_cmp(const ruuvi_gw_cfg_http_t* const p_http1, const ruuvi_gw_cfg_http_t* const p_http2)
{
    if (p_http1->use_http_ruuvi != p_http2->use_http_ruuvi)
    {
        return false;
    }
    if (p_http1->use_http != p_http2->use_http)
    {
        return false;
    }
    if (p_http1->http_use_ssl_client_cert != p_http2->http_use_ssl_client_cert)
    {
        return false;
    }
    if (p_http1->http_use_ssl_server_cert != p_http2->http_use_ssl_server_cert)
    {
        return false;
    }
    if (0 != strcmp(p_http1->http_url.buf, p_http2->http_url.buf))
    {
        return false;
    }
    if (p_http1->http_period != p_http2->http_period)
    {
        return false;
    }
    if (p_http1->data_format != p_http2->data_format)
    {
        return false;
    }
    if (p_http1->auth_type != p_http2->auth_type)
    {
        return false;
    }
    switch (p_http1->auth_type)
    {
        case GW_CFG_HTTP_AUTH_TYPE_NONE:
            break;
        case GW_CFG_HTTP_AUTH_TYPE_BASIC:
            if (!ruuvi_gw_cfg_http_cmp_auth_basic(&p_http1->auth.auth_basic, &p_http2->auth.auth_basic))
            {
                return false;
            }
            break;
        case GW_CFG_HTTP_AUTH_TYPE_BEARER:
            if (0 != strcmp(p_http1->auth.auth_bearer.token.buf, p_http2->auth.auth_bearer.token.buf))
            {
                return false;
            }
            break;
        case GW_CFG_HTTP_AUTH_TYPE_TOKEN:
            if (0 != strcmp(p_http1->auth.auth_token.token.buf, p_http2->auth.auth_token.token.buf))
            {
                return false;
            }
            break;
        case GW_CFG_HTTP_AUTH_TYPE_APIKEY:
            if (0 != strcmp(p_http1->auth.auth_apikey.api_key.buf, p_http2->auth.auth_apikey.api_key.buf))
            {
                return false;
            }
            break;
    }
    return true;
}

static bool
ruuvi_gw_cfg_http_stat_cmp(
    const ruuvi_gw_cfg_http_stat_t* const p_http_stat1,
    const ruuvi_gw_cfg_http_stat_t* const p_http_stat2)
{
    if (p_http_stat1->use_http_stat != p_http_stat2->use_http_stat)
    {
        return false;
    }
    if (p_http_stat1->http_stat_use_ssl_client_cert != p_http_stat2->http_stat_use_ssl_client_cert)
    {
        return false;
    }
    if (p_http_stat1->http_stat_use_ssl_server_cert != p_http_stat2->http_stat_use_ssl_server_cert)
    {
        return false;
    }
    if (0 != strcmp(p_http_stat1->http_stat_url.buf, p_http_stat2->http_stat_url.buf))
    {
        return false;
    }
    if (0 != strcmp(p_http_stat1->http_stat_user.buf, p_http_stat2->http_stat_user.buf))
    {
        return false;
    }
    if (0 != strcmp(p_http_stat1->http_stat_pass.buf, p_http_stat2->http_stat_pass.buf))
    {
        return false;
    }
    return true;
}

static bool
ruuvi_gw_cfg_mqtt_cmp(const ruuvi_gw_cfg_mqtt_t* const p_mqtt1, const ruuvi_gw_cfg_mqtt_t* const p_mqtt2)
{
    if (p_mqtt1->use_mqtt != p_mqtt2->use_mqtt)
    {
        return false;
    }
    if (p_mqtt1->mqtt_disable_retained_messages != p_mqtt2->mqtt_disable_retained_messages)
    {
        return false;
    }
    if (0 != strcmp(p_mqtt1->mqtt_transport.buf, p_mqtt2->mqtt_transport.buf))
    {
        return false;
    }
    if (p_mqtt1->mqtt_data_format != p_mqtt2->mqtt_data_format)
    {
        return false;
    }
    if (0 != strcmp(p_mqtt1->mqtt_server.buf, p_mqtt2->mqtt_server.buf))
    {
        return false;
    }
    if (p_mqtt1->mqtt_port != p_mqtt2->mqtt_port)
    {
        return false;
    }
    if (p_mqtt1->mqtt_sending_interval != p_mqtt2->mqtt_sending_interval)
    {
        return false;
    }
    if (0 != strcmp(p_mqtt1->mqtt_prefix.buf, p_mqtt2->mqtt_prefix.buf))
    {
        return false;
    }
    if (0 != strcmp(p_mqtt1->mqtt_client_id.buf, p_mqtt2->mqtt_client_id.buf))
    {
        return false;
    }
    if (0 != strcmp(p_mqtt1->mqtt_user.buf, p_mqtt2->mqtt_user.buf))
    {
        return false;
    }
    if (0 != strcmp(p_mqtt1->mqtt_pass.buf, p_mqtt2->mqtt_pass.buf))
    {
        return false;
    }
    if (p_mqtt1->use_ssl_client_cert != p_mqtt2->use_ssl_client_cert)
    {
        return false;
    }
    if (p_mqtt1->use_ssl_server_cert != p_mqtt2->use_ssl_server_cert)
    {
        return false;
    }
    return true;
}

static bool
ruuvi_gw_cfg_lan_auth_cmp(
    const ruuvi_gw_cfg_lan_auth_t* const p_lan_auth1,
    const ruuvi_gw_cfg_lan_auth_t* const p_lan_auth2)
{
    if (p_lan_auth1->lan_auth_type != p_lan_auth2->lan_auth_type)
    {
        return false;
    }
    if (0 != strcmp(p_lan_auth1->lan_auth_user.buf, p_lan_auth2->lan_auth_user.buf))
    {
        return false;
    }
    if (0 != strcmp(p_lan_auth1->lan_auth_pass.buf, p_lan_auth2->lan_auth_pass.buf))
    {
        return false;
    }
    if (0 != strcmp(p_lan_auth1->lan_auth_api_key.buf, p_lan_auth2->lan_auth_api_key.buf))
    {
        return false;
    }
    if (0 != strcmp(p_lan_auth1->lan_auth_api_key_rw.buf, p_lan_auth2->lan_auth_api_key_rw.buf))
    {
        return false;
    }
    return true;
}

static bool
ruuvi_gw_cfg_auto_update_cmp(
    const ruuvi_gw_cfg_auto_update_t* const p_auto_update1,
    const ruuvi_gw_cfg_auto_update_t* const p_auto_update2)
{
    if (p_auto_update1->auto_update_cycle != p_auto_update2->auto_update_cycle)
    {
        return false;
    }
    if (p_auto_update1->auto_update_weekdays_bitmask != p_auto_update2->auto_update_weekdays_bitmask)
    {
        return false;
    }
    if (p_auto_update1->auto_update_interval_from != p_auto_update2->auto_update_interval_from)
    {
        return false;
    }
    if (p_auto_update1->auto_update_interval_to != p_auto_update2->auto_update_interval_to)
    {
        return false;
    }
    if (p_auto_update1->auto_update_tz_offset_hours != p_auto_update2->auto_update_tz_offset_hours)
    {
        return false;
    }
    return true;
}

static bool
ruuvi_gw_cfg_ntp_cmp(const ruuvi_gw_cfg_ntp_t* const p_ntp1, const ruuvi_gw_cfg_ntp_t* const p_ntp2)
{
    if (p_ntp1->ntp_use != p_ntp2->ntp_use)
    {
        return false;
    }
    if (p_ntp1->ntp_use_dhcp != p_ntp2->ntp_use_dhcp)
    {
        return false;
    }
    if (0 != strcmp(p_ntp1->ntp_server1.buf, p_ntp2->ntp_server1.buf))
    {
        return false;
    }
    if (0 != strcmp(p_ntp1->ntp_server2.buf, p_ntp2->ntp_server2.buf))
    {
        return false;
    }
    if (0 != strcmp(p_ntp1->ntp_server3.buf, p_ntp2->ntp_server3.buf))
    {
        return false;
    }
    if (0 != strcmp(p_ntp1->ntp_server4.buf, p_ntp2->ntp_server4.buf))
    {
        return false;
    }
    return true;
}

static bool
ruuvi_gw_cfg_filter_cmp(const ruuvi_gw_cfg_filter_t* const p_filter1, const ruuvi_gw_cfg_filter_t* const p_filter2)
{
    if (p_filter1->company_use_filtering != p_filter2->company_use_filtering)
    {
        return false;
    }
    if (p_filter1->company_id != p_filter2->company_id)
    {
        return false;
    }
    return true;
}

static bool
ruuvi_gw_cfg_scan_cmp(const ruuvi_gw_cfg_scan_t* const p_scan1, const ruuvi_gw_cfg_scan_t* const p_scan2)
{
    if (p_scan1->scan_coded_phy != p_scan2->scan_coded_phy)
    {
        return false;
    }
    if (p_scan1->scan_1mbit_phy != p_scan2->scan_1mbit_phy)
    {
        return false;
    }
    if (p_scan1->scan_extended_payload != p_scan2->scan_extended_payload)
    {
        return false;
    }
    if (p_scan1->scan_channel_37 != p_scan2->scan_channel_37)
    {
        return false;
    }
    if (p_scan1->scan_channel_38 != p_scan2->scan_channel_38)
    {
        return false;
    }
    if (p_scan1->scan_channel_39 != p_scan2->scan_channel_39)
    {
        return false;
    }
    return true;
}

static bool
ruuvi_gw_cfg_scan_filter_cmp(
    const ruuvi_gw_cfg_scan_filter_t* const p_scan_filter1,
    const ruuvi_gw_cfg_scan_filter_t* const p_scan_filter2)
{
    if (p_scan_filter1->scan_filter_allow_listed != p_scan_filter2->scan_filter_allow_listed)
    {
        return false;
    }
    if (p_scan_filter1->scan_filter_length != p_scan_filter2->scan_filter_length)
    {
        return false;
    }
    for (uint32_t i = 0; i < p_scan_filter1->scan_filter_length; ++i)
    {
        if (0
            != memcmp(
                &p_scan_filter1->scan_filter_list[i],
                &p_scan_filter2->scan_filter_list[i],
                sizeof(p_scan_filter1->scan_filter_list[i])))
        {
            return false;
        }
    }
    return true;
}

static bool
ruuvi_gw_cfg_coordinates_cmp(
    const ruuvi_gw_cfg_coordinates_t* const p_coordinates1,
    const ruuvi_gw_cfg_coordinates_t* const p_coordinates2)
{
    if (0 != strcmp(p_coordinates1->buf, p_coordinates2->buf))
    {
        return false;
    }
    return true;
}

static bool
ruuvi_gw_cfg_fw_update_cmp(
    const ruuvi_gw_cfg_fw_update_t* const p_fw_update1,
    const ruuvi_gw_cfg_fw_update_t* const p_fw_update2)
{
    if (0 != strcmp(p_fw_update1->fw_update_url, p_fw_update2->fw_update_url))
    {
        return false;
    }
    return true;
}

bool
gw_cfg_ruuvi_cmp(const gw_cfg_ruuvi_t* const p_cfg_ruuvi1, const gw_cfg_ruuvi_t* const p_cfg_ruuvi2)
{
    if (!ruuvi_gw_cfg_remote_cmp(&p_cfg_ruuvi1->remote, &p_cfg_ruuvi2->remote))
    {
        return false;
    }
    if (!ruuvi_gw_cfg_http_cmp(&p_cfg_ruuvi1->http, &p_cfg_ruuvi2->http))
    {
        return false;
    }
    if (!ruuvi_gw_cfg_http_stat_cmp(&p_cfg_ruuvi1->http_stat, &p_cfg_ruuvi2->http_stat))
    {
        return false;
    }
    if (!ruuvi_gw_cfg_mqtt_cmp(&p_cfg_ruuvi1->mqtt, &p_cfg_ruuvi2->mqtt))
    {
        return false;
    }
    if (!ruuvi_gw_cfg_lan_auth_cmp(&p_cfg_ruuvi1->lan_auth, &p_cfg_ruuvi2->lan_auth))
    {
        return false;
    }
    if (!ruuvi_gw_cfg_auto_update_cmp(&p_cfg_ruuvi1->auto_update, &p_cfg_ruuvi2->auto_update))
    {
        return false;
    }
    if (!ruuvi_gw_cfg_ntp_cmp(&p_cfg_ruuvi1->ntp, &p_cfg_ruuvi2->ntp))
    {
        return false;
    }
    if (!ruuvi_gw_cfg_filter_cmp(&p_cfg_ruuvi1->filter, &p_cfg_ruuvi2->filter))
    {
        return false;
    }
    if (!ruuvi_gw_cfg_scan_cmp(&p_cfg_ruuvi1->scan, &p_cfg_ruuvi2->scan))
    {
        return false;
    }
    if (!ruuvi_gw_cfg_scan_filter_cmp(&p_cfg_ruuvi1->scan_filter, &p_cfg_ruuvi2->scan_filter))
    {
        return false;
    }
    if (!ruuvi_gw_cfg_coordinates_cmp(&p_cfg_ruuvi1->coordinates, &p_cfg_ruuvi2->coordinates))
    {
        return false;
    }
    if (!ruuvi_gw_cfg_fw_update_cmp(&p_cfg_ruuvi1->fw_update, &p_cfg_ruuvi2->fw_update))
    {
        return false;
    }
    return true;
}
