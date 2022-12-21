/**
 * @file gw_status.h
 * @author TheSomeMan
 * @date 2022-05-05
 * @copyright Ruuvi Innovations Ltd, license BSD-3-Clause.
 */

#include "gw_status.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "os_mutex.h"
#include "event_mgr.h"

#define LOG_LOCAL_LEVEL LOG_LEVEL_INFO
#include "log.h"

#define WIFI_CONNECTED_BIT (1U << 0U)
#define MQTT_CONNECTED_BIT (1U << 1U)
#define MQTT_STARTED_BIT   (1U << 2U)
#define ETH_CONNECTED_BIT  (1U << 4U)
#define ETH_LINK_UP_BIT    (1U << 5U)
#define NRF_STATUS_BIT     (1U << 7U)

typedef struct gw_status_relaying_t
{
    os_mutex_t        mutex_cnt_relaying_suspended;
    os_mutex_static_t mutex_mem_cnt_relaying_suspended;
    int32_t           cnt_relaying_via_http_suspended;
    int32_t           cnt_relaying_via_mqtt_suspended;
} gw_status_relaying_t;

static const char TAG[] = "gw_status";

static EventGroupHandle_t   g_p_ev_grp_status_bits;
static gw_status_relaying_t g_gw_status_relaying;

bool
gw_status_init(void)
{
    g_p_ev_grp_status_bits = xEventGroupCreate();
    if (NULL == g_p_ev_grp_status_bits)
    {
        LOG_ERR("%s failed", "xEventGroupCreate");
        return false;
    }
    gw_status_relaying_t* const p_relaying      = &g_gw_status_relaying;
    p_relaying->cnt_relaying_via_http_suspended = 0;
    p_relaying->cnt_relaying_via_mqtt_suspended = 0;
    p_relaying->mutex_cnt_relaying_suspended    = os_mutex_create_static(&p_relaying->mutex_mem_cnt_relaying_suspended);
    return true;
}

void
gw_status_set_wifi_connected(void)
{
    xEventGroupSetBits(g_p_ev_grp_status_bits, WIFI_CONNECTED_BIT);
}

void
gw_status_clear_wifi_connected(void)
{
    xEventGroupClearBits(g_p_ev_grp_status_bits, WIFI_CONNECTED_BIT);
}

void
gw_status_set_eth_connected(void)
{
    xEventGroupSetBits(g_p_ev_grp_status_bits, ETH_CONNECTED_BIT);
}

void
gw_status_clear_eth_connected(void)
{
    xEventGroupClearBits(g_p_ev_grp_status_bits, ETH_CONNECTED_BIT | ETH_LINK_UP_BIT);
}

void
gw_status_set_eth_link_up(void)
{
    xEventGroupSetBits(g_p_ev_grp_status_bits, ETH_LINK_UP_BIT);
}

bool
gw_status_is_eth_link_up(void)
{
    if (0 != (xEventGroupGetBits(g_p_ev_grp_status_bits) & ETH_LINK_UP_BIT))
    {
        return true;
    }
    return false;
}

bool
gw_status_is_network_connected(void)
{
    if (0 != (xEventGroupGetBits(g_p_ev_grp_status_bits) & (WIFI_CONNECTED_BIT | ETH_CONNECTED_BIT)))
    {
        return true;
    }
    return false;
}

void
gw_status_set_mqtt_connected(void)
{
    xEventGroupSetBits(g_p_ev_grp_status_bits, MQTT_CONNECTED_BIT);
}

void
gw_status_clear_mqtt_connected(void)
{
    xEventGroupClearBits(g_p_ev_grp_status_bits, MQTT_CONNECTED_BIT);
}

bool
gw_status_is_mqtt_connected(void)
{
    if (0 != (xEventGroupGetBits(g_p_ev_grp_status_bits) & MQTT_CONNECTED_BIT))
    {
        return true;
    }
    return false;
}

void
gw_status_set_nrf_status(void)
{
    xEventGroupSetBits(g_p_ev_grp_status_bits, NRF_STATUS_BIT);
}

void
gw_status_clear_nrf_status(void)
{
    xEventGroupClearBits(g_p_ev_grp_status_bits, NRF_STATUS_BIT);
}

bool
gw_status_get_nrf_status(void)
{
    if (0 != (xEventGroupGetBits(g_p_ev_grp_status_bits) & NRF_STATUS_BIT))
    {
        return true;
    }
    return false;
}

void
gw_status_set_mqtt_started(void)
{
    xEventGroupSetBits(g_p_ev_grp_status_bits, MQTT_STARTED_BIT);
}

void
gw_status_clear_mqtt_started(void)
{
    xEventGroupClearBits(g_p_ev_grp_status_bits, MQTT_STARTED_BIT);
}

bool
gw_status_is_mqtt_started(void)
{
    if (0 != (xEventGroupGetBits(g_p_ev_grp_status_bits) & MQTT_STARTED_BIT))
    {
        return true;
    }
    return false;
}

void
gw_status_suspend_relaying(void)
{
    LOG_INFO("SUSPEND RELAYING");
    gw_status_relaying_t* const p_relaying = &g_gw_status_relaying;
    os_mutex_lock(p_relaying->mutex_cnt_relaying_suspended);
    const bool flag_notify_change_for_http = (0 == p_relaying->cnt_relaying_via_http_suspended) ? true : false;
    const bool flag_notify_change_for_mqtt = (0 == p_relaying->cnt_relaying_via_mqtt_suspended) ? true : false;
    p_relaying->cnt_relaying_via_http_suspended += 1;
    p_relaying->cnt_relaying_via_mqtt_suspended += 1;
    os_mutex_unlock(p_relaying->mutex_cnt_relaying_suspended);
    if (flag_notify_change_for_http || flag_notify_change_for_mqtt)
    {
        event_mgr_notify(EVENT_MGR_EV_RELAYING_MODE_CHANGED);
    }
}

void
gw_status_resume_relaying(void)
{
    LOG_INFO("RESUME RELAYING");
    gw_status_relaying_t* const p_relaying = &g_gw_status_relaying;
    os_mutex_lock(p_relaying->mutex_cnt_relaying_suspended);
    bool flag_notify_change = false;
    if (0 == p_relaying->cnt_relaying_via_http_suspended)
    {
        LOG_WARN("Attempt to resume relaying via HTTP, but it is already active");
    }
    else
    {
        p_relaying->cnt_relaying_via_http_suspended -= 1;
        if (0 == p_relaying->cnt_relaying_via_http_suspended)
        {
            flag_notify_change = true;
        }
    }
    if (0 == p_relaying->cnt_relaying_via_mqtt_suspended)
    {
        LOG_WARN("Attempt to resume relaying via MQTT, but it is already active");
    }
    else
    {
        p_relaying->cnt_relaying_via_mqtt_suspended -= 1;
        if (0 == p_relaying->cnt_relaying_via_mqtt_suspended)
        {
            flag_notify_change = true;
        }
    }
    os_mutex_unlock(p_relaying->mutex_cnt_relaying_suspended);
    if (flag_notify_change)
    {
        event_mgr_notify(EVENT_MGR_EV_RELAYING_MODE_CHANGED);
    }
}

void
gw_status_suspend_http_relaying(void)
{
    LOG_INFO("SUSPEND HTTP RELAYING");
    gw_status_relaying_t* const p_relaying = &g_gw_status_relaying;
    os_mutex_lock(p_relaying->mutex_cnt_relaying_suspended);
    const bool flag_notify_change_for_http = (0 == p_relaying->cnt_relaying_via_http_suspended) ? true : false;
    p_relaying->cnt_relaying_via_http_suspended += 1;
    os_mutex_unlock(p_relaying->mutex_cnt_relaying_suspended);
    if (flag_notify_change_for_http)
    {
        event_mgr_notify(EVENT_MGR_EV_RELAYING_MODE_CHANGED);
    }
}

void
gw_status_resume_http_relaying(void)
{
    LOG_INFO("RESUME HTTP RELAYING");
    gw_status_relaying_t* const p_relaying = &g_gw_status_relaying;
    os_mutex_lock(p_relaying->mutex_cnt_relaying_suspended);
    bool flag_notify_change = false;
    if (0 == p_relaying->cnt_relaying_via_http_suspended)
    {
        LOG_WARN("Attempt to resume relaying via HTTP, but it is already active");
    }
    else
    {
        p_relaying->cnt_relaying_via_http_suspended -= 1;
        if (0 == p_relaying->cnt_relaying_via_http_suspended)
        {
            flag_notify_change = true;
        }
    }
    os_mutex_unlock(p_relaying->mutex_cnt_relaying_suspended);
    if (flag_notify_change)
    {
        event_mgr_notify(EVENT_MGR_EV_RELAYING_MODE_CHANGED);
    }
}

bool
gw_status_is_relaying_via_http_enabled(void)
{
    gw_status_relaying_t* const p_relaying = &g_gw_status_relaying;
    os_mutex_lock(p_relaying->mutex_cnt_relaying_suspended);
    const bool flag_relaying_enabled = (0 == p_relaying->cnt_relaying_via_http_suspended) ? true : false;
    os_mutex_unlock(p_relaying->mutex_cnt_relaying_suspended);
    return flag_relaying_enabled;
}

bool
gw_status_is_relaying_via_mqtt_enabled(void)
{
    gw_status_relaying_t* const p_relaying = &g_gw_status_relaying;
    os_mutex_lock(p_relaying->mutex_cnt_relaying_suspended);
    const bool flag_relaying_enabled = (0 == p_relaying->cnt_relaying_via_mqtt_suspended) ? true : false;
    os_mutex_unlock(p_relaying->mutex_cnt_relaying_suspended);
    return flag_relaying_enabled;
}
