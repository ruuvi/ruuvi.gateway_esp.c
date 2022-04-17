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

static gw_cfg_t                    g_gateway_config = { 0 };
static os_mutex_recursive_t        g_gw_cfg_mutex;
static os_mutex_recursive_static_t g_gw_cfg_mutex_mem;
static gw_cfg_device_info_t *const g_gw_cfg_p_device_info = &g_gateway_config.device_info;

void
gw_cfg_init(void)
{
    g_gw_cfg_mutex = os_mutex_recursive_create_static(&g_gw_cfg_mutex_mem);
    os_mutex_recursive_lock(g_gw_cfg_mutex);
    gw_cfg_default_get(&g_gateway_config);
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

gw_cfg_t *
gw_cfg_lock_rw(void)
{
    os_mutex_recursive_lock(g_gw_cfg_mutex);
    return &g_gateway_config;
}

void
gw_cfg_unlock_rw(gw_cfg_t **const p_p_gw_cfg)
{
    *p_p_gw_cfg = NULL;
    os_mutex_recursive_unlock(g_gw_cfg_mutex);
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

void
gw_cfg_update_eth_cfg(const gw_cfg_eth_t *const p_gw_cfg_eth_new)
{
    gw_cfg_t *p_gw_cfg = gw_cfg_lock_rw();
    p_gw_cfg->eth_cfg  = *p_gw_cfg_eth_new;
    gw_cfg_unlock_rw(&p_gw_cfg);
}

void
gw_cfg_update_ruuvi_cfg(const gw_cfg_ruuvi_t *const p_gw_cfg_ruuvi_new)
{
    gw_cfg_t *p_gw_cfg  = gw_cfg_lock_rw();
    p_gw_cfg->ruuvi_cfg = *p_gw_cfg_ruuvi_new;
    gw_cfg_unlock_rw(&p_gw_cfg);
}

void
gw_cfg_update_wifi_config(const wifiman_config_t *const p_wifi_cfg)
{
    gw_cfg_t *p_gw_cfg = gw_cfg_lock_rw();
    p_gw_cfg->wifi_cfg = *p_wifi_cfg;
    gw_cfg_unlock_rw(&p_gw_cfg);
}

void
gw_cfg_get_copy(gw_cfg_t *const p_gw_cfg_dst)
{
    const gw_cfg_t *p_gw_cfg = gw_cfg_lock_ro();
    *p_gw_cfg_dst            = *p_gw_cfg;
    gw_cfg_unlock_ro(&p_gw_cfg);
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
    gw_cfg_t *             p_gw_cfg = gw_cfg_lock_rw();
    const wifiman_config_t wifi_cfg = p_gw_cfg->wifi_cfg;
    gw_cfg_unlock_rw(&p_gw_cfg);
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
