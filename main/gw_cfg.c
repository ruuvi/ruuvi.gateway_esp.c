/**
 * @file gw_cfg.h
 * @author TheSomeMan
 * @date 2020-10-31
 * @copyright Ruuvi Innovations Ltd, license BSD-3-Clause.
 */

#include "gw_cfg.h"
#include "gw_cfg_default.h"
#include <string.h>
#include "os_mutex_recursive.h"
#include "event_mgr.h"
#include "os_malloc.h"

_Static_assert(sizeof(GW_CFG_REMOTE_AUTH_TYPE_STR_NO) <= GW_CFG_REMOTE_AUTH_TYPE_STR_SIZE, "");
_Static_assert(sizeof(GW_CFG_REMOTE_AUTH_TYPE_STR_BASIC) <= GW_CFG_REMOTE_AUTH_TYPE_STR_SIZE, "");
_Static_assert(sizeof(GW_CFG_REMOTE_AUTH_TYPE_STR_BEARER) <= GW_CFG_REMOTE_AUTH_TYPE_STR_SIZE, "");

static gw_cfg_t                    g_gateway_config = { 0 };
static bool                        g_gw_cfg_ready   = false;
static os_mutex_recursive_t        g_gw_cfg_mutex;
static os_mutex_recursive_static_t g_gw_cfg_mutex_mem;
static gw_cfg_device_info_t *const g_gw_cfg_p_device_info = &g_gateway_config.device_info;
static gw_cfg_cb_on_change_cfg     g_p_gw_cfg_cb_on_change_cfg;

void
gw_cfg_init(gw_cfg_cb_on_change_cfg p_cb_on_change_cfg)
{
    g_gw_cfg_mutex = os_mutex_recursive_create_static(&g_gw_cfg_mutex_mem);
    os_mutex_recursive_lock(g_gw_cfg_mutex);
    g_gw_cfg_ready = false;
    gw_cfg_default_get(&g_gateway_config);
    g_p_gw_cfg_cb_on_change_cfg = p_cb_on_change_cfg;
    os_mutex_recursive_unlock(g_gw_cfg_mutex);
}

void
gw_cfg_deinit(void)
{
    os_mutex_recursive_delete(&g_gw_cfg_mutex);
    g_gw_cfg_mutex = NULL;
}

bool
gw_cfg_is_initialized(void)
{
    if (NULL != g_gw_cfg_mutex)
    {
        return true;
    }
    return false;
}

const gw_cfg_t *
gw_cfg_lock_ro(void)
{
    os_mutex_recursive_lock(g_gw_cfg_mutex);
    return &g_gateway_config;
}

void
gw_cfg_unlock_ro(const gw_cfg_t **const p_p_gw_cfg)
{
    *p_p_gw_cfg = NULL;
    os_mutex_recursive_unlock(g_gw_cfg_mutex);
}

static bool
ruuvi_gw_cfg_remote_cmp(const ruuvi_gw_cfg_remote_t *const p_remote1, const ruuvi_gw_cfg_remote_t *const p_remote2)
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
        case GW_CFG_REMOTE_AUTH_TYPE_NO:
            break;
        case GW_CFG_REMOTE_AUTH_TYPE_BASIC:
            if (0 != strcmp(p_remote1->auth.auth_basic.user.buf, p_remote2->auth.auth_basic.user.buf))
            {
                return false;
            }
            if (0 != strcmp(p_remote1->auth.auth_basic.password.buf, p_remote2->auth.auth_basic.password.buf))
            {
                return false;
            }
            break;
        case GW_CFG_REMOTE_AUTH_TYPE_BEARER:
            if (0 != strcmp(p_remote1->auth.auth_bearer.token.buf, p_remote2->auth.auth_bearer.token.buf))
            {
                return false;
            }
            break;
    }
    if (p_remote1->refresh_interval_minutes != p_remote2->refresh_interval_minutes)
    {
        return false;
    }
    return true;
}

static bool
ruuvi_gw_cfg_http_cmp(const ruuvi_gw_cfg_http_t *const p_http1, const ruuvi_gw_cfg_http_t *const p_http2)
{
    if (p_http1->use_http != p_http2->use_http)
    {
        return false;
    }
    if (0 != strcmp(p_http1->http_url.buf, p_http2->http_url.buf))
    {
        return false;
    }
    if (0 != strcmp(p_http1->http_user.buf, p_http2->http_user.buf))
    {
        return false;
    }
    if (0 != strcmp(p_http1->http_pass.buf, p_http2->http_pass.buf))
    {
        return false;
    }
    return true;
}

static bool
ruuvi_gw_cfg_http_stat_cmp(
    const ruuvi_gw_cfg_http_stat_t *const p_http_stat1,
    const ruuvi_gw_cfg_http_stat_t *const p_http_stat2)
{
    if (p_http_stat1->use_http_stat != p_http_stat2->use_http_stat)
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
ruuvi_gw_cfg_mqtt_cmp(const ruuvi_gw_cfg_mqtt_t *const p_mqtt1, const ruuvi_gw_cfg_mqtt_t *const p_mqtt2)
{
    if (p_mqtt1->use_mqtt != p_mqtt2->use_mqtt)
    {
        return false;
    }
    if (0 != strcmp(p_mqtt1->mqtt_transport.buf, p_mqtt2->mqtt_transport.buf))
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
    return true;
}

static bool
ruuvi_gw_cfg_lan_auth_cmp(
    const ruuvi_gw_cfg_lan_auth_t *const p_lan_auth1,
    const ruuvi_gw_cfg_lan_auth_t *const p_lan_auth2)
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
    return true;
}

static bool
ruuvi_gw_cfg_auto_update_cmp(
    const ruuvi_gw_cfg_auto_update_t *const p_auto_update1,
    const ruuvi_gw_cfg_auto_update_t *const p_auto_update2)
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
ruuvi_gw_cfg_ntp_cmp(const ruuvi_gw_cfg_ntp_t *const p_ntp1, const ruuvi_gw_cfg_ntp_t *const p_ntp2)
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
ruuvi_gw_cfg_filter_cmp(const ruuvi_gw_cfg_filter_t *const p_filter1, const ruuvi_gw_cfg_filter_t *const p_filter2)
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
ruuvi_gw_cfg_scan_cmp(const ruuvi_gw_cfg_scan_t *const p_scan1, const ruuvi_gw_cfg_scan_t *const p_scan2)
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
ruuvi_gw_cfg_coordinates_cmp(
    const ruuvi_gw_cfg_coordinates_t *const p_coordinates1,
    const ruuvi_gw_cfg_coordinates_t *const p_coordinates2)
{
    if (0 != strcmp(p_coordinates1->buf, p_coordinates2->buf))
    {
        return false;
    }
    return true;
}

static bool
gw_cfg_ruuvi_cmp(const gw_cfg_ruuvi_t *const p_cfg_ruuvi1, const gw_cfg_ruuvi_t *const p_cfg_ruuvi2)
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
    if (!ruuvi_gw_cfg_coordinates_cmp(&p_cfg_ruuvi1->coordinates, &p_cfg_ruuvi2->coordinates))
    {
        return false;
    }
    return true;
}

static bool
gw_cfg_eth_cmp(const gw_cfg_eth_t *const p_eth1, const gw_cfg_eth_t *const p_eth2)
{
    if (p_eth1->use_eth != p_eth2->use_eth)
    {
        return false;
    }
    if (p_eth1->eth_dhcp != p_eth2->eth_dhcp)
    {
        return false;
    }
    if (0 != strcmp(p_eth1->eth_static_ip.buf, p_eth2->eth_static_ip.buf))
    {
        return false;
    }
    if (0 != strcmp(p_eth1->eth_netmask.buf, p_eth2->eth_netmask.buf))
    {
        return false;
    }
    if (0 != strcmp(p_eth1->eth_gw.buf, p_eth2->eth_gw.buf))
    {
        return false;
    }
    if (0 != strcmp(p_eth1->eth_dns1.buf, p_eth2->eth_dns1.buf))
    {
        return false;
    }
    if (0 != strcmp(p_eth1->eth_dns2.buf, p_eth2->eth_dns2.buf))
    {
        return false;
    }
    return true;
}

static bool
wifiman_config_cmp_config_ap(const wifi_ap_config_t *const p_wifi1, const wifi_ap_config_t *const p_wifi2)
{
    if (0 != strcmp((const char *)p_wifi1->ssid, (const char *)p_wifi2->ssid))
    {
        return false;
    }
    if (0 != strcmp((const char *)p_wifi1->password, (const char *)p_wifi2->password))
    {
        return false;
    }
    if (p_wifi1->ssid_len != p_wifi2->ssid_len)
    {
        return false;
    }
    if (p_wifi1->channel != p_wifi2->channel)
    {
        return false;
    }
    if (p_wifi1->authmode != p_wifi2->authmode)
    {
        return false;
    }
    if (p_wifi1->ssid_hidden != p_wifi2->ssid_hidden)
    {
        return false;
    }
    if (p_wifi1->max_connection != p_wifi2->max_connection)
    {
        return false;
    }
    if (p_wifi1->beacon_interval != p_wifi2->beacon_interval)
    {
        return false;
    }
    return true;
}

static bool
wifiman_config_cmp_settings_ap(const wifi_settings_ap_t *const p_wifi1, const wifi_settings_ap_t *const p_wifi2)
{
    if (p_wifi1->ap_bandwidth != p_wifi2->ap_bandwidth)
    {
        return false;
    }
    if (0 != strcmp(p_wifi1->ap_ip.buf, p_wifi2->ap_ip.buf))
    {
        return false;
    }
    if (0 != strcmp(p_wifi1->ap_gw.buf, p_wifi2->ap_gw.buf))
    {
        return false;
    }
    if (0 != strcmp(p_wifi1->ap_netmask.buf, p_wifi2->ap_netmask.buf))
    {
        return false;
    }
    return true;
}

static bool
wifiman_config_cmp_sta_config(const wifi_sta_config_t *const p_wifi1, const wifi_sta_config_t *const p_wifi2)
{
    if (0 != strcmp((const char *)p_wifi1->ssid, (const char *)p_wifi2->ssid))
    {
        return false;
    }
    if (0 != strcmp((const char *)p_wifi1->password, (const char *)p_wifi2->password))
    {
        return false;
    }
    if (p_wifi1->scan_method != p_wifi2->scan_method)
    {
        return false;
    }
    if (p_wifi1->bssid_set != p_wifi2->bssid_set)
    {
        return false;
    }
    if (0 != memcmp(p_wifi1->bssid, p_wifi2->bssid, sizeof(p_wifi1->bssid)))
    {
        return false;
    }
    if (p_wifi1->channel != p_wifi2->channel)
    {
        return false;
    }
    if (p_wifi1->listen_interval != p_wifi2->listen_interval)
    {
        return false;
    }
    if (p_wifi1->sort_method != p_wifi2->sort_method)
    {
        return false;
    }
    if (p_wifi1->threshold.rssi != p_wifi2->threshold.rssi)
    {
        return false;
    }
    if (p_wifi1->threshold.authmode != p_wifi2->threshold.authmode)
    {
        return false;
    }
    if (p_wifi1->pmf_cfg.capable != p_wifi2->pmf_cfg.capable)
    {
        return false;
    }
    if (p_wifi1->pmf_cfg.required != p_wifi2->pmf_cfg.required)
    {
        return false;
    }
    return true;
}

static bool
wifiman_config_cmp_settings_sta(const wifi_settings_sta_t *const p_wifi1, const wifi_settings_sta_t *const p_wifi2)
{
    if (p_wifi1->sta_power_save != p_wifi2->sta_power_save)
    {
        return false;
    }
    if (p_wifi1->sta_static_ip != p_wifi2->sta_static_ip)
    {
        return false;
    }
    if (p_wifi1->sta_static_ip_config.ip.addr != p_wifi2->sta_static_ip_config.ip.addr)
    {
        return false;
    }
    if (p_wifi1->sta_static_ip_config.netmask.addr != p_wifi2->sta_static_ip_config.netmask.addr)
    {
        return false;
    }
    if (p_wifi1->sta_static_ip_config.gw.addr != p_wifi2->sta_static_ip_config.gw.addr)
    {
        return false;
    }
    return true;
}

static bool
wifiman_config_cmp(const wifiman_config_t *const p_wifi1, const wifiman_config_t *const p_wifi2)
{
    if (!wifiman_config_cmp_config_ap(&p_wifi1->wifi_config_ap, &p_wifi2->wifi_config_ap))
    {
        return false;
    }
    if (!wifiman_config_cmp_settings_ap(&p_wifi1->wifi_settings_ap, &p_wifi2->wifi_settings_ap))
    {
        return false;
    }
    if (!wifiman_config_cmp_sta_config(&p_wifi1->wifi_config_sta, &p_wifi2->wifi_config_sta))
    {
        return false;
    }
    if (!wifiman_config_cmp_settings_sta(&p_wifi1->wifi_settings_sta, &p_wifi2->wifi_settings_sta))
    {
        return false;
    }
    return true;
}

static gw_cfg_update_status_t
gw_cfg_set(
    const gw_cfg_ruuvi_t *const   p_gw_cfg_ruuvi,
    const gw_cfg_eth_t *const     p_gw_cfg_eth,
    const wifiman_config_t *const p_gw_cfg_wifi)
{
    gw_cfg_update_status_t update_status = {
        .flag_ruuvi_cfg_modified = false,
        .flag_eth_cfg_modified   = false,
        .flag_wifi_cfg_modified  = false,
    };

    os_mutex_recursive_lock(g_gw_cfg_mutex);
    gw_cfg_t *const p_gw_cfg_dst = &g_gateway_config;

    if (NULL != p_gw_cfg_ruuvi)
    {
        if (!gw_cfg_ruuvi_cmp(&p_gw_cfg_dst->ruuvi_cfg, p_gw_cfg_ruuvi))
        {
            if (g_gw_cfg_ready)
            {
                event_mgr_notify(EVENT_MGR_EV_GW_CFG_CHANGED_RUUVI);
                if (p_gw_cfg_dst->ruuvi_cfg.ntp.ntp_use != p_gw_cfg_ruuvi->ntp.ntp_use)
                {
                    event_mgr_notify(EVENT_MGR_EV_GW_CFG_CHANGED_RUUVI_NTP_USE);
                }
                else if (
                    p_gw_cfg_ruuvi->ntp.ntp_use
                    && (p_gw_cfg_dst->ruuvi_cfg.ntp.ntp_use_dhcp != p_gw_cfg_ruuvi->ntp.ntp_use_dhcp))
                {
                    event_mgr_notify(EVENT_MGR_EV_GW_CFG_CHANGED_RUUVI_NTP_USE_DHCP);
                }
            }
            update_status.flag_ruuvi_cfg_modified = true;
            p_gw_cfg_dst->ruuvi_cfg               = *p_gw_cfg_ruuvi;
        }
    }
    if (NULL != p_gw_cfg_eth)
    {
        if (!gw_cfg_eth_cmp(&p_gw_cfg_dst->eth_cfg, p_gw_cfg_eth))
        {
            if (g_gw_cfg_ready)
            {
                event_mgr_notify(EVENT_MGR_EV_GW_CFG_CHANGED_ETH);
            }
            update_status.flag_eth_cfg_modified = true;
            p_gw_cfg_dst->eth_cfg               = *p_gw_cfg_eth;
        }
    }
    if (NULL != p_gw_cfg_wifi)
    {
        if (!wifiman_config_cmp(&p_gw_cfg_dst->wifi_cfg, p_gw_cfg_wifi))
        {
            if (g_gw_cfg_ready)
            {
                event_mgr_notify(EVENT_MGR_EV_GW_CFG_CHANGED_WIFI);
            }
            update_status.flag_wifi_cfg_modified = true;
            p_gw_cfg_dst->wifi_cfg               = *p_gw_cfg_wifi;
        }
    }
    if (update_status.flag_ruuvi_cfg_modified || update_status.flag_eth_cfg_modified
        || update_status.flag_wifi_cfg_modified)
    {
        if (NULL != g_p_gw_cfg_cb_on_change_cfg)
        {
            g_p_gw_cfg_cb_on_change_cfg(p_gw_cfg_dst);
        }
    }
    if (!g_gw_cfg_ready)
    {
        g_gw_cfg_ready = true;
        event_mgr_notify(EVENT_MGR_EV_GW_CFG_READY);
    }

    os_mutex_recursive_unlock(g_gw_cfg_mutex);
    return update_status;
}

void
gw_cfg_update_eth_cfg(const gw_cfg_eth_t *const p_gw_cfg_eth_new)
{
    gw_cfg_set(NULL, p_gw_cfg_eth_new, NULL);
}

void
gw_cfg_update_ruuvi_cfg(const gw_cfg_ruuvi_t *const p_gw_cfg_ruuvi_new)
{
    gw_cfg_set(p_gw_cfg_ruuvi_new, NULL, NULL);
}

void
gw_cfg_update_wifi_config(const wifiman_config_t *const p_wifi_cfg)
{
    gw_cfg_set(NULL, NULL, p_wifi_cfg);
}

gw_cfg_update_status_t
gw_cfg_update(const gw_cfg_t *const p_gw_cfg)
{
    return gw_cfg_set(&p_gw_cfg->ruuvi_cfg, &p_gw_cfg->eth_cfg, &p_gw_cfg->wifi_cfg);
}

void
gw_cfg_get_copy(gw_cfg_t *const p_gw_cfg_dst)
{
    const gw_cfg_t *p_gw_cfg = gw_cfg_lock_ro();
    *p_gw_cfg_dst            = *p_gw_cfg;
    gw_cfg_unlock_ro(&p_gw_cfg);
}

bool
gw_cfg_get_remote_cfg_use(gw_cfg_remote_refresh_interval_minutes_t *const p_interval_minutes)
{
    const gw_cfg_t *p_gw_cfg = gw_cfg_lock_ro();
    if (NULL != p_interval_minutes)
    {
        *p_interval_minutes = p_gw_cfg->ruuvi_cfg.remote.refresh_interval_minutes;
    }
    const bool flag_use_remote_cfg = p_gw_cfg->ruuvi_cfg.remote.use_remote_cfg;
    gw_cfg_unlock_ro(&p_gw_cfg);
    return flag_use_remote_cfg;
}

const ruuvi_gw_cfg_remote_t *
gw_cfg_get_remote_cfg_copy(void)
{
    const gw_cfg_t *       p_gw_cfg = gw_cfg_lock_ro();
    ruuvi_gw_cfg_remote_t *p_remote = os_calloc(1, sizeof(*p_remote));
    if (NULL == p_remote)
    {
        return NULL;
    }
    *p_remote = p_gw_cfg->ruuvi_cfg.remote;
    gw_cfg_unlock_ro(&p_gw_cfg);
    return p_remote;
}

bool
gw_cfg_get_eth_use_eth(void)
{
    const gw_cfg_t *p_gw_cfg = gw_cfg_lock_ro();
    const bool      use_eth  = p_gw_cfg->eth_cfg.use_eth;
    gw_cfg_unlock_ro(&p_gw_cfg);
    return use_eth;
}

bool
gw_cfg_get_eth_use_dhcp(void)
{
    const gw_cfg_t *p_gw_cfg = gw_cfg_lock_ro();
    const bool      use_dhcp = p_gw_cfg->eth_cfg.eth_dhcp;
    gw_cfg_unlock_ro(&p_gw_cfg);
    return use_dhcp;
}

bool
gw_cfg_get_mqtt_use_mqtt(void)
{
    const gw_cfg_t *p_gw_cfg = gw_cfg_lock_ro();
    const bool      use_mqtt = p_gw_cfg->ruuvi_cfg.mqtt.use_mqtt;
    gw_cfg_unlock_ro(&p_gw_cfg);
    return use_mqtt;
}

bool
gw_cfg_get_http_use_http(void)
{
    const gw_cfg_t *p_gw_cfg = gw_cfg_lock_ro();
    const bool      use_http = p_gw_cfg->ruuvi_cfg.http.use_http;
    gw_cfg_unlock_ro(&p_gw_cfg);
    return use_http;
}

bool
gw_cfg_get_http_stat_use_http_stat(void)
{
    const gw_cfg_t *p_gw_cfg = gw_cfg_lock_ro();
    const bool      use_http = p_gw_cfg->ruuvi_cfg.http_stat.use_http_stat;
    gw_cfg_unlock_ro(&p_gw_cfg);
    return use_http;
}

ruuvi_gw_cfg_mqtt_prefix_t
gw_cfg_get_mqtt_prefix(void)
{
    const gw_cfg_t *                 p_gw_cfg    = gw_cfg_lock_ro();
    const ruuvi_gw_cfg_mqtt_prefix_t mqtt_prefix = p_gw_cfg->ruuvi_cfg.mqtt.mqtt_prefix;
    gw_cfg_unlock_ro(&p_gw_cfg);
    return mqtt_prefix;
}

auto_update_cycle_type_e
gw_cfg_get_auto_update_cycle(void)
{
    const gw_cfg_t *               p_gw_cfg          = gw_cfg_lock_ro();
    const auto_update_cycle_type_e auto_update_cycle = p_gw_cfg->ruuvi_cfg.auto_update.auto_update_cycle;
    gw_cfg_unlock_ro(&p_gw_cfg);
    return auto_update_cycle;
}

ruuvi_gw_cfg_lan_auth_t
gw_cfg_get_lan_auth(void)
{
    const gw_cfg_t *              p_gw_cfg = gw_cfg_lock_ro();
    const ruuvi_gw_cfg_lan_auth_t lan_auth = p_gw_cfg->ruuvi_cfg.lan_auth;
    gw_cfg_unlock_ro(&p_gw_cfg);
    return lan_auth;
}

bool
gw_cfg_get_ntp_use(void)
{
    const gw_cfg_t *p_gw_cfg = gw_cfg_lock_ro();
    const bool      ntp_use  = p_gw_cfg->ruuvi_cfg.ntp.ntp_use;
    gw_cfg_unlock_ro(&p_gw_cfg);
    return ntp_use;
}

ruuvi_gw_cfg_coordinates_t
gw_cfg_get_coordinates(void)
{
    const gw_cfg_t *                 p_gw_cfg    = gw_cfg_lock_ro();
    const ruuvi_gw_cfg_coordinates_t coordinates = p_gw_cfg->ruuvi_cfg.coordinates;
    gw_cfg_unlock_ro(&p_gw_cfg);
    return coordinates;
}

wifiman_config_t
gw_cfg_get_wifi_cfg(void)
{
    const gw_cfg_t *       p_gw_cfg = gw_cfg_lock_ro();
    const wifiman_config_t wifi_cfg = p_gw_cfg->wifi_cfg;
    gw_cfg_unlock_ro(&p_gw_cfg);
    return wifi_cfg;
}

const ruuvi_esp32_fw_ver_str_t *
gw_cfg_get_esp32_fw_ver(void)
{
    assert(NULL != g_gw_cfg_mutex);
    return &g_gw_cfg_p_device_info->esp32_fw_ver;
}

const ruuvi_nrf52_fw_ver_str_t *
gw_cfg_get_nrf52_fw_ver(void)
{
    assert(NULL != g_gw_cfg_mutex);
    return &g_gw_cfg_p_device_info->nrf52_fw_ver;
}

const nrf52_device_id_str_t *
gw_cfg_get_nrf52_device_id(void)
{
    assert(NULL != g_gw_cfg_mutex);
    return &g_gw_cfg_p_device_info->nrf52_device_id;
}

const mac_address_str_t *
gw_cfg_get_nrf52_mac_addr(void)
{
    assert(NULL != g_gw_cfg_mutex);
    return &g_gw_cfg_p_device_info->nrf52_mac_addr;
}

const mac_address_str_t *
gw_cfg_get_esp32_mac_addr_wifi(void)
{
    assert(NULL != g_gw_cfg_mutex);
    return &g_gw_cfg_p_device_info->esp32_mac_addr_wifi;
}

const mac_address_str_t *
gw_cfg_get_esp32_mac_addr_eth(void)
{
    assert(NULL != g_gw_cfg_mutex);
    return &g_gw_cfg_p_device_info->esp32_mac_addr_eth;
}

const wifiman_wifi_ssid_t *
gw_cfg_get_wifi_ap_ssid(void)
{
    return gw_cfg_default_get_wifi_ap_ssid();
}

const char *
gw_cfg_auth_type_to_str(const ruuvi_gw_cfg_lan_auth_t *const p_lan_auth)
{
    const ruuvi_gw_cfg_lan_auth_t *const p_default_lan_auth    = gw_cfg_default_get_lan_auth();
    bool                                 flag_use_default_auth = false;
    if ((HTTP_SERVER_AUTH_TYPE_RUUVI == p_lan_auth->lan_auth_type)
        && (0 == strcmp(p_lan_auth->lan_auth_user.buf, p_default_lan_auth->lan_auth_user.buf))
        && (0 == strcmp(p_lan_auth->lan_auth_pass.buf, p_default_lan_auth->lan_auth_pass.buf)))
    {
        flag_use_default_auth = true;
    }
    return http_server_auth_type_to_str(p_lan_auth->lan_auth_type, flag_use_default_auth);
}
