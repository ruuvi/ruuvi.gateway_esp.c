/**
 * @file gw_cfg.c
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

_Static_assert(sizeof(GW_CFG_HTTP_AUTH_TYPE_STR_NO) <= GW_CFG_HTTP_AUTH_TYPE_STR_SIZE, "");
_Static_assert(sizeof(GW_CFG_HTTP_AUTH_TYPE_STR_NONE) <= GW_CFG_HTTP_AUTH_TYPE_STR_SIZE, "");
_Static_assert(sizeof(GW_CFG_HTTP_AUTH_TYPE_STR_BASIC) <= GW_CFG_HTTP_AUTH_TYPE_STR_SIZE, "");
_Static_assert(sizeof(GW_CFG_HTTP_AUTH_TYPE_STR_BEARER) <= GW_CFG_HTTP_AUTH_TYPE_STR_SIZE, "");
_Static_assert(sizeof(GW_CFG_HTTP_AUTH_TYPE_STR_TOKEN) <= GW_CFG_HTTP_AUTH_TYPE_STR_SIZE, "");

_Static_assert(sizeof(GW_CFG_HTTP_DATA_FORMAT_STR_RUUVI) <= GW_CFG_HTTP_DATA_FORMAT_STR_SIZE, "");

static gw_cfg_t                    g_gateway_config  = { 0 };
static bool                        g_gw_cfg_ready    = false;
static bool                        g_gw_cfg_is_empty = false;
static os_mutex_recursive_t        g_gw_cfg_mutex;
static os_mutex_recursive_static_t g_gw_cfg_mutex_mem;
static gw_cfg_device_info_t* const g_gw_cfg_p_device_info = &g_gateway_config.device_info;
static gw_cfg_cb_on_change_cfg     g_p_gw_cfg_cb_on_change_cfg;

void
gw_cfg_init(gw_cfg_cb_on_change_cfg p_cb_on_change_cfg)
{
    assert(NULL == g_gw_cfg_mutex);
    g_gw_cfg_mutex = os_mutex_recursive_create_static(&g_gw_cfg_mutex_mem);
    os_mutex_recursive_lock(g_gw_cfg_mutex);
    g_gw_cfg_ready    = false;
    g_gw_cfg_is_empty = true;
    gw_cfg_default_get(&g_gateway_config);
    g_p_gw_cfg_cb_on_change_cfg = p_cb_on_change_cfg;
    os_mutex_recursive_unlock(g_gw_cfg_mutex);
}

void
gw_cfg_deinit(void)
{
    assert(NULL != g_gw_cfg_mutex);
    os_mutex_recursive_lock(g_gw_cfg_mutex);
    g_p_gw_cfg_cb_on_change_cfg = NULL;
    g_gw_cfg_ready              = false;
    os_mutex_recursive_unlock(g_gw_cfg_mutex);
    os_mutex_recursive_delete(&g_gw_cfg_mutex);
    g_gw_cfg_mutex = NULL;
}

bool
gw_cfg_is_initialized(void)
{
    if (NULL != g_gw_cfg_mutex)
    {
        return g_gw_cfg_ready;
    }
    return false;
}

const gw_cfg_t*
gw_cfg_lock_ro(void)
{
    assert(NULL != g_gw_cfg_mutex);
    os_mutex_recursive_lock(g_gw_cfg_mutex);
    return &g_gateway_config;
}

void
gw_cfg_unlock_ro(const gw_cfg_t** const p_p_gw_cfg)
{
    assert(NULL != g_gw_cfg_mutex);
    *p_p_gw_cfg = NULL;
    os_mutex_recursive_unlock(g_gw_cfg_mutex);
}

bool
gw_cfg_is_empty(void)
{
    assert(NULL != g_gw_cfg_mutex);
    const gw_cfg_t* p_gw_cfg      = gw_cfg_lock_ro();
    const bool      flag_is_empty = g_gw_cfg_is_empty;
    gw_cfg_unlock_ro(&p_gw_cfg);
    return flag_is_empty;
}

static bool
gw_cfg_eth_cmp(const gw_cfg_eth_t* const p_eth1, const gw_cfg_eth_t* const p_eth2)
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
wifiman_config_cmp_config_ap(const wifi_ap_config_t* const p_wifi1, const wifi_ap_config_t* const p_wifi2)
{
    if (0 != strcmp((const char*)p_wifi1->ssid, (const char*)p_wifi2->ssid))
    {
        return false;
    }
    if (0 != strcmp((const char*)p_wifi1->password, (const char*)p_wifi2->password))
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
wifiman_config_cmp_settings_ap(const wifi_settings_ap_t* const p_wifi1, const wifi_settings_ap_t* const p_wifi2)
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
wifiman_config_cmp_sta_config(const wifi_sta_config_t* const p_wifi1, const wifi_sta_config_t* const p_wifi2)
{
    if (0 != strcmp((const char*)p_wifi1->ssid, (const char*)p_wifi2->ssid))
    {
        return false;
    }
    if (0 != strcmp((const char*)p_wifi1->password, (const char*)p_wifi2->password))
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
wifiman_config_cmp_settings_sta(const wifi_settings_sta_t* const p_wifi1, const wifi_settings_sta_t* const p_wifi2)
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
wifiman_config_ap_cmp(const wifiman_config_ap_t* const p_wifi1, const wifiman_config_ap_t* const p_wifi2)
{
    if (!wifiman_config_cmp_config_ap(&p_wifi1->wifi_config_ap, &p_wifi2->wifi_config_ap))
    {
        return false;
    }
    if (!wifiman_config_cmp_settings_ap(&p_wifi1->wifi_settings_ap, &p_wifi2->wifi_settings_ap))
    {
        return false;
    }
    return true;
}

static bool
wifiman_config_sta_cmp(const wifiman_config_sta_t* const p_wifi1, const wifiman_config_sta_t* const p_wifi2)
{
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

static void
gw_cfg_set_ruuvi(
    const gw_cfg_ruuvi_t* const p_gw_cfg_ruuvi,
    gw_cfg_ruuvi_t* const       p_gw_cfg_ruuvi_dst,
    bool* const                 p_ruuvi_cfg_modified)
{
    if (!gw_cfg_ruuvi_cmp(p_gw_cfg_ruuvi_dst, p_gw_cfg_ruuvi))
    {
        if (g_gw_cfg_ready)
        {
            event_mgr_notify(EVENT_MGR_EV_GW_CFG_CHANGED_RUUVI);
            if (p_gw_cfg_ruuvi_dst->ntp.ntp_use != p_gw_cfg_ruuvi->ntp.ntp_use)
            {
                event_mgr_notify(EVENT_MGR_EV_GW_CFG_CHANGED_RUUVI_NTP_USE);
            }
            else if (
                p_gw_cfg_ruuvi->ntp.ntp_use
                && (p_gw_cfg_ruuvi_dst->ntp.ntp_use_dhcp != p_gw_cfg_ruuvi->ntp.ntp_use_dhcp))
            {
                event_mgr_notify(EVENT_MGR_EV_GW_CFG_CHANGED_RUUVI_NTP_USE_DHCP);
            }
            else
            {
                // MISRA C:2012, 15.7 - All if...else if constructs shall be terminated with an else statement
            }
        }
        *p_ruuvi_cfg_modified = true;
        *p_gw_cfg_ruuvi_dst   = *p_gw_cfg_ruuvi;
    }
}

static void
gw_cfg_set_eth(
    const gw_cfg_eth_t* const p_gw_cfg_eth,
    gw_cfg_eth_t* const       p_gw_cfg_eth_dst,
    bool* const               p_eth_cfg_modified)
{
    if (!gw_cfg_eth_cmp(p_gw_cfg_eth_dst, p_gw_cfg_eth))
    {
        if (g_gw_cfg_ready)
        {
            event_mgr_notify(EVENT_MGR_EV_GW_CFG_CHANGED_ETH);
        }
        *p_eth_cfg_modified = true;
        *p_gw_cfg_eth_dst   = *p_gw_cfg_eth;
    }
}

static void
gw_cfg_set_wifi_ap(
    const wifiman_config_ap_t* const p_gw_cfg_wifi_ap,
    wifiman_config_ap_t* const       p_gw_cfg_wifi_ap_dst,
    bool* const                      p_wifi_cfg_modified)
{
    if (!wifiman_config_ap_cmp(p_gw_cfg_wifi_ap_dst, p_gw_cfg_wifi_ap))
    {
        if (g_gw_cfg_ready)
        {
            event_mgr_notify(EVENT_MGR_EV_GW_CFG_CHANGED_WIFI);
        }
        *p_wifi_cfg_modified  = true;
        *p_gw_cfg_wifi_ap_dst = *p_gw_cfg_wifi_ap;
    }
}

static void
gw_cfg_set_wifi_sta(
    const wifiman_config_sta_t* const p_gw_cfg_wifi_sta,
    wifiman_config_sta_t* const       p_gw_cfg_wifi_sta_dst,
    bool* const                       p_wifi_cfg_modified)
{
    if (!wifiman_config_sta_cmp(p_gw_cfg_wifi_sta_dst, p_gw_cfg_wifi_sta))
    {
        if (g_gw_cfg_ready)
        {
            event_mgr_notify(EVENT_MGR_EV_GW_CFG_CHANGED_WIFI);
        }
        *p_wifi_cfg_modified   = true;
        *p_gw_cfg_wifi_sta_dst = *p_gw_cfg_wifi_sta;
    }
}

static gw_cfg_update_status_t
gw_cfg_set(
    const gw_cfg_ruuvi_t* const       p_gw_cfg_ruuvi,
    const gw_cfg_eth_t* const         p_gw_cfg_eth,
    const wifiman_config_ap_t* const  p_gw_cfg_wifi_ap,
    const wifiman_config_sta_t* const p_gw_cfg_wifi_sta)
{
    gw_cfg_update_status_t update_status = {
        .flag_ruuvi_cfg_modified    = false,
        .flag_eth_cfg_modified      = false,
        .flag_wifi_ap_cfg_modified  = false,
        .flag_wifi_sta_cfg_modified = false,
    };

    assert(NULL != g_gw_cfg_mutex);
    os_mutex_recursive_lock(g_gw_cfg_mutex);
    gw_cfg_t* const p_gw_cfg_dst = &g_gateway_config;

    g_gw_cfg_is_empty = (NULL == p_gw_cfg_ruuvi) && (NULL == p_gw_cfg_eth) && (NULL == p_gw_cfg_wifi_ap)
                        && (NULL == p_gw_cfg_wifi_sta);

    if (NULL != p_gw_cfg_ruuvi)
    {
        gw_cfg_set_ruuvi(p_gw_cfg_ruuvi, &p_gw_cfg_dst->ruuvi_cfg, &update_status.flag_ruuvi_cfg_modified);
    }
    if (NULL != p_gw_cfg_eth)
    {
        gw_cfg_set_eth(p_gw_cfg_eth, &p_gw_cfg_dst->eth_cfg, &update_status.flag_eth_cfg_modified);
    }
    if (NULL != p_gw_cfg_wifi_ap)
    {
        gw_cfg_set_wifi_ap(p_gw_cfg_wifi_ap, &p_gw_cfg_dst->wifi_cfg.ap, &update_status.flag_wifi_ap_cfg_modified);
    }
    if (NULL != p_gw_cfg_wifi_sta)
    {
        gw_cfg_set_wifi_sta(p_gw_cfg_wifi_sta, &p_gw_cfg_dst->wifi_cfg.sta, &update_status.flag_wifi_sta_cfg_modified);
    }

    if (NULL != g_p_gw_cfg_cb_on_change_cfg)
    {
        // We don't need to check update_status before calling g_p_gw_cfg_cb_on_change_cfg
        // because this callback will compare and update the configuration in NVS if they don't match.
        // Moreover, in case if the configuration in NVS is absent and the default configuration is used,
        // then update_status can show that updating is not needed, but it is required.
        g_p_gw_cfg_cb_on_change_cfg(g_gw_cfg_is_empty ? NULL : p_gw_cfg_dst);
    }
    if (!g_gw_cfg_ready)
    {
        g_gw_cfg_ready = true;
        if (NULL != g_p_gw_cfg_cb_on_change_cfg)
        {
            event_mgr_notify(EVENT_MGR_EV_GW_CFG_READY);
        }
    }

    os_mutex_recursive_unlock(g_gw_cfg_mutex);
    return update_status;
}

void
gw_cfg_update_eth_cfg(const gw_cfg_eth_t* const p_gw_cfg_eth_new)
{
    gw_cfg_set(NULL, p_gw_cfg_eth_new, NULL, NULL);
}

void
gw_cfg_update_ruuvi_cfg(const gw_cfg_ruuvi_t* const p_gw_cfg_ruuvi_new)
{
    gw_cfg_set(p_gw_cfg_ruuvi_new, NULL, NULL, NULL);
}

void
gw_cfg_update_fw_update_url(const char* const p_fw_update_url)
{
    assert(NULL != g_gw_cfg_mutex);
    os_mutex_recursive_lock(g_gw_cfg_mutex);
    gw_cfg_t* const p_gw_cfg_dst = &g_gateway_config;

    (void)snprintf(
        p_gw_cfg_dst->ruuvi_cfg.fw_update.fw_update_url,
        sizeof(p_gw_cfg_dst->ruuvi_cfg.fw_update.fw_update_url),
        "%s",
        p_fw_update_url);

    if (NULL != g_p_gw_cfg_cb_on_change_cfg)
    {
        g_p_gw_cfg_cb_on_change_cfg(p_gw_cfg_dst);
    }

    os_mutex_recursive_unlock(g_gw_cfg_mutex);
}

void
gw_cfg_update_wifi_ap_config(const wifiman_config_ap_t* const p_wifi_ap_cfg)
{
    gw_cfg_set(NULL, NULL, p_wifi_ap_cfg, NULL);
}

void
gw_cfg_update_wifi_sta_config(const wifiman_config_sta_t* const p_wifi_sta_cfg)
{
    gw_cfg_set(NULL, NULL, NULL, p_wifi_sta_cfg);
}

gw_cfg_update_status_t
gw_cfg_update(const gw_cfg_t* const p_gw_cfg)
{
    if (NULL == p_gw_cfg)
    {
        return gw_cfg_set(NULL, NULL, NULL, NULL);
    }
    return gw_cfg_set(&p_gw_cfg->ruuvi_cfg, &p_gw_cfg->eth_cfg, &p_gw_cfg->wifi_cfg.ap, &p_gw_cfg->wifi_cfg.sta);
}

void
gw_cfg_get_copy(gw_cfg_t* const p_gw_cfg_dst)
{
    assert(NULL != g_gw_cfg_mutex);
    const gw_cfg_t* p_gw_cfg = gw_cfg_lock_ro();
    *p_gw_cfg_dst            = *p_gw_cfg;
    gw_cfg_unlock_ro(&p_gw_cfg);
}

bool
gw_cfg_get_remote_cfg_use(gw_cfg_remote_refresh_interval_minutes_t* const p_interval_minutes)
{
    assert(NULL != g_gw_cfg_mutex);
    const gw_cfg_t* p_gw_cfg = gw_cfg_lock_ro();
    if (NULL != p_interval_minutes)
    {
        *p_interval_minutes = p_gw_cfg->ruuvi_cfg.remote.refresh_interval_minutes;
    }
    const bool flag_use_remote_cfg = p_gw_cfg->ruuvi_cfg.remote.use_remote_cfg;
    gw_cfg_unlock_ro(&p_gw_cfg);
    return flag_use_remote_cfg;
}

const ruuvi_gw_cfg_remote_t*
gw_cfg_get_remote_cfg_copy(void)
{
    assert(NULL != g_gw_cfg_mutex);
    const gw_cfg_t*        p_gw_cfg = gw_cfg_lock_ro();
    ruuvi_gw_cfg_remote_t* p_remote = os_calloc(1, sizeof(*p_remote));
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
    assert(NULL != g_gw_cfg_mutex);
    const gw_cfg_t* p_gw_cfg = gw_cfg_lock_ro();
    const bool      use_eth  = p_gw_cfg->eth_cfg.use_eth;
    gw_cfg_unlock_ro(&p_gw_cfg);
    return use_eth;
}

bool
gw_cfg_get_eth_use_dhcp(void)
{
    assert(NULL != g_gw_cfg_mutex);
    const gw_cfg_t* p_gw_cfg = gw_cfg_lock_ro();
    const bool      use_dhcp = p_gw_cfg->eth_cfg.eth_dhcp;
    gw_cfg_unlock_ro(&p_gw_cfg);
    return use_dhcp;
}

bool
gw_cfg_get_mqtt_use_mqtt(void)
{
    assert(NULL != g_gw_cfg_mutex);
    const gw_cfg_t* p_gw_cfg = gw_cfg_lock_ro();
    const bool      use_mqtt = p_gw_cfg->ruuvi_cfg.mqtt.use_mqtt;
    gw_cfg_unlock_ro(&p_gw_cfg);
    return use_mqtt;
}

uint32_t
gw_cfg_get_mqtt_sending_interval(void)
{
    assert(NULL != g_gw_cfg_mutex);
    const gw_cfg_t* p_gw_cfg              = gw_cfg_lock_ro();
    const uint32_t  mqtt_sending_interval = p_gw_cfg->ruuvi_cfg.mqtt.mqtt_sending_interval;
    gw_cfg_unlock_ro(&p_gw_cfg);
    return mqtt_sending_interval;
}

bool
gw_cfg_get_mqtt_use_mqtt_over_ssl_or_wss(void)
{
    const gw_cfg_t*                     p_gw_cfg       = gw_cfg_lock_ro();
    const bool                          use_mqtt       = p_gw_cfg->ruuvi_cfg.mqtt.use_mqtt;
    const ruuvi_gw_cfg_mqtt_transport_t mqtt_transport = p_gw_cfg->ruuvi_cfg.mqtt.mqtt_transport;
    gw_cfg_unlock_ro(&p_gw_cfg);
    if (!use_mqtt)
    {
        return false;
    }
    if (0 == strcmp(mqtt_transport.buf, MQTT_TRANSPORT_SSL))
    {
        return true;
    }
    if (0 == strcmp(mqtt_transport.buf, MQTT_TRANSPORT_WSS))
    {
        return true;
    }
    return false;
}

bool
gw_cfg_get_http_use_http_ruuvi(void)
{
    assert(NULL != g_gw_cfg_mutex);
    const gw_cfg_t* p_gw_cfg = gw_cfg_lock_ro();
    const bool      use_http = p_gw_cfg->ruuvi_cfg.http.use_http_ruuvi;
    gw_cfg_unlock_ro(&p_gw_cfg);
    return use_http;
}

bool
gw_cfg_get_http_use_http(void)
{
    assert(NULL != g_gw_cfg_mutex);
    const gw_cfg_t* p_gw_cfg = gw_cfg_lock_ro();
    const bool      use_http = p_gw_cfg->ruuvi_cfg.http.use_http;
    gw_cfg_unlock_ro(&p_gw_cfg);
    return use_http;
}

uint32_t
gw_cfg_get_http_period(void)
{
    assert(NULL != g_gw_cfg_mutex);
    const gw_cfg_t* p_gw_cfg    = gw_cfg_lock_ro();
    const uint32_t  http_period = p_gw_cfg->ruuvi_cfg.http.http_period;
    gw_cfg_unlock_ro(&p_gw_cfg);
    return http_period;
}

ruuvi_gw_cfg_http_t*
gw_cfg_get_http_copy(void)
{
    assert(NULL != g_gw_cfg_mutex);
    ruuvi_gw_cfg_http_t* p_cfg_http = os_malloc(sizeof(*p_cfg_http));
    if (NULL == p_cfg_http)
    {
        return p_cfg_http;
    }

    const gw_cfg_t* p_gw_cfg = gw_cfg_lock_ro();
    *p_cfg_http              = p_gw_cfg->ruuvi_cfg.http;
    gw_cfg_unlock_ro(&p_gw_cfg);
    return p_cfg_http;
}

static str_buf_t
gw_cfg_get_http_password_copy_unsafe(const gw_cfg_t* const p_gw_cfg)
{
    switch (p_gw_cfg->ruuvi_cfg.http.auth_type)
    {
        case GW_CFG_HTTP_AUTH_TYPE_NONE:
            return str_buf_printf_with_alloc("%s", "");
        case GW_CFG_HTTP_AUTH_TYPE_BASIC:
            return str_buf_printf_with_alloc("%s", p_gw_cfg->ruuvi_cfg.http.auth.auth_basic.password.buf);
        case GW_CFG_HTTP_AUTH_TYPE_BEARER:
            return str_buf_printf_with_alloc("%s", p_gw_cfg->ruuvi_cfg.http.auth.auth_bearer.token.buf);
        case GW_CFG_HTTP_AUTH_TYPE_TOKEN:
            return str_buf_printf_with_alloc("%s", p_gw_cfg->ruuvi_cfg.http.auth.auth_token.token.buf);
    }
    assert(0);
    return str_buf_init_null();
}

str_buf_t
gw_cfg_get_http_password_copy(void)
{
    assert(NULL != g_gw_cfg_mutex);
    const gw_cfg_t* p_gw_cfg      = gw_cfg_lock_ro();
    str_buf_t       http_password = gw_cfg_get_http_password_copy_unsafe(p_gw_cfg);
    gw_cfg_unlock_ro(&p_gw_cfg);
    return http_password;
}

bool
gw_cfg_get_http_stat_use_http_stat(void)
{
    assert(NULL != g_gw_cfg_mutex);
    const gw_cfg_t* p_gw_cfg = gw_cfg_lock_ro();
    const bool      use_http = p_gw_cfg->ruuvi_cfg.http_stat.use_http_stat;
    gw_cfg_unlock_ro(&p_gw_cfg);
    return use_http;
}

ruuvi_gw_cfg_http_stat_t*
gw_cfg_get_http_stat_copy(void)
{
    assert(NULL != g_gw_cfg_mutex);
    ruuvi_gw_cfg_http_stat_t* p_cfg_http_stat = os_malloc(sizeof(*p_cfg_http_stat));
    if (NULL == p_cfg_http_stat)
    {
        return p_cfg_http_stat;
    }
    const gw_cfg_t* p_gw_cfg = gw_cfg_lock_ro();
    *p_cfg_http_stat         = p_gw_cfg->ruuvi_cfg.http_stat;
    gw_cfg_unlock_ro(&p_gw_cfg);
    return p_cfg_http_stat;
}

str_buf_t
gw_cfg_get_http_stat_password_copy(void)
{
    assert(NULL != g_gw_cfg_mutex);
    const gw_cfg_t* p_gw_cfg     = gw_cfg_lock_ro();
    str_buf_t http_stat_password = str_buf_printf_with_alloc("%s", p_gw_cfg->ruuvi_cfg.http_stat.http_stat_pass.buf);
    gw_cfg_unlock_ro(&p_gw_cfg);
    return http_stat_password;
}

ruuvi_gw_cfg_mqtt_t*
gw_cfg_get_mqtt_copy(void)
{
    assert(NULL != g_gw_cfg_mutex);
    ruuvi_gw_cfg_mqtt_t* p_cfg_mqtt = os_calloc(1, sizeof(*p_cfg_mqtt));
    if (NULL == p_cfg_mqtt)
    {
        return p_cfg_mqtt;
    }
    const gw_cfg_t* p_gw_cfg = gw_cfg_lock_ro();
    *p_cfg_mqtt              = p_gw_cfg->ruuvi_cfg.mqtt;
    gw_cfg_unlock_ro(&p_gw_cfg);
    return p_cfg_mqtt;
}

str_buf_t
gw_cfg_get_mqtt_password_copy(void)
{
    assert(NULL != g_gw_cfg_mutex);
    const gw_cfg_t* p_gw_cfg      = gw_cfg_lock_ro();
    str_buf_t       mqtt_password = str_buf_printf_with_alloc("%s", p_gw_cfg->ruuvi_cfg.mqtt.mqtt_pass.buf);
    gw_cfg_unlock_ro(&p_gw_cfg);
    return mqtt_password;
}

auto_update_cycle_type_e
gw_cfg_get_auto_update_cycle(void)
{
    assert(NULL != g_gw_cfg_mutex);
    const gw_cfg_t*                p_gw_cfg          = gw_cfg_lock_ro();
    const auto_update_cycle_type_e auto_update_cycle = p_gw_cfg->ruuvi_cfg.auto_update.auto_update_cycle;
    gw_cfg_unlock_ro(&p_gw_cfg);
    return auto_update_cycle;
}

bool
gw_cfg_get_ntp_use(void)
{
    assert(NULL != g_gw_cfg_mutex);
    const gw_cfg_t* p_gw_cfg = gw_cfg_lock_ro();
    const bool      ntp_use  = p_gw_cfg->ruuvi_cfg.ntp.ntp_use;
    gw_cfg_unlock_ro(&p_gw_cfg);
    return ntp_use;
}

ruuvi_gw_cfg_coordinates_t
gw_cfg_get_coordinates(void)
{
    assert(NULL != g_gw_cfg_mutex);
    const gw_cfg_t*                  p_gw_cfg    = gw_cfg_lock_ro();
    const ruuvi_gw_cfg_coordinates_t coordinates = p_gw_cfg->ruuvi_cfg.coordinates;
    gw_cfg_unlock_ro(&p_gw_cfg);
    return coordinates;
}

wifiman_config_t
gw_cfg_get_wifi_cfg(void)
{
    assert(NULL != g_gw_cfg_mutex);
    const gw_cfg_t*        p_gw_cfg = gw_cfg_lock_ro();
    const wifiman_config_t wifi_cfg = p_gw_cfg->wifi_cfg;
    gw_cfg_unlock_ro(&p_gw_cfg);
    return wifi_cfg;
}

const ruuvi_esp32_fw_ver_str_t*
gw_cfg_get_esp32_fw_ver(void)
{
    assert(NULL != g_gw_cfg_mutex);
    return &g_gw_cfg_p_device_info->esp32_fw_ver;
}

const ruuvi_nrf52_fw_ver_str_t*
gw_cfg_get_nrf52_fw_ver(void)
{
    assert(NULL != g_gw_cfg_mutex);
    return &g_gw_cfg_p_device_info->nrf52_fw_ver;
}

const nrf52_device_id_str_t*
gw_cfg_get_nrf52_device_id(void)
{
    assert(NULL != g_gw_cfg_mutex);
    return &g_gw_cfg_p_device_info->nrf52_device_id;
}

const mac_address_str_t*
gw_cfg_get_nrf52_mac_addr(void)
{
    assert(NULL != g_gw_cfg_mutex);
    return &g_gw_cfg_p_device_info->nrf52_mac_addr;
}

const mac_address_str_t*
gw_cfg_get_esp32_mac_addr_wifi(void)
{
    assert(NULL != g_gw_cfg_mutex);
    return &g_gw_cfg_p_device_info->esp32_mac_addr_wifi;
}

const mac_address_str_t*
gw_cfg_get_esp32_mac_addr_eth(void)
{
    assert(NULL != g_gw_cfg_mutex);
    return &g_gw_cfg_p_device_info->esp32_mac_addr_eth;
}

const wifiman_wifi_ssid_t*
gw_cfg_get_wifi_ap_ssid(void)
{
    return gw_cfg_default_get_wifi_ap_ssid();
}

const wifiman_hostname_t*
gw_cfg_get_hostname(void)
{
    return gw_cfg_default_get_hostname();
}

const char*
gw_cfg_auth_type_to_str(const ruuvi_gw_cfg_lan_auth_t* const p_lan_auth)
{
    const ruuvi_gw_cfg_lan_auth_t* const p_default_lan_auth = gw_cfg_default_get_lan_auth();
    http_server_auth_type_e              lan_auth_type      = p_lan_auth->lan_auth_type;
    if ((HTTP_SERVER_AUTH_TYPE_RUUVI == lan_auth_type)
        && (0 == strcmp(p_lan_auth->lan_auth_user.buf, p_default_lan_auth->lan_auth_user.buf))
        && (0 == strcmp(p_lan_auth->lan_auth_pass.buf, p_default_lan_auth->lan_auth_pass.buf)))
    {
        lan_auth_type = HTTP_SERVER_AUTH_TYPE_DEFAULT;
    }
    return http_server_auth_type_to_str(lan_auth_type);
}

str_buf_t
gw_cfg_get_fw_update_url(void)
{
    assert(NULL != g_gw_cfg_mutex);
    const gw_cfg_t*                       p_gw_cfg      = gw_cfg_lock_ro();
    const ruuvi_gw_cfg_fw_update_t* const p_fw_update   = &p_gw_cfg->ruuvi_cfg.fw_update;
    const str_buf_t                       fw_update_url = str_buf_printf_with_alloc("%s", p_fw_update->fw_update_url);
    gw_cfg_unlock_ro(&p_gw_cfg);
    return fw_update_url;
}
