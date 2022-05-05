/**
 * @file event_mgr.h
 * @author TheSomeMan
 * @date 2020-12-01
 * @copyright Ruuvi Innovations Ltd, license BSD-3-Clause.
 */

#ifndef RUUVI_GATEWAY_ESP_EVENT_MGR_H
#define RUUVI_GATEWAY_ESP_EVENT_MGR_H

#include <stdbool.h>
#include "os_signal.h"
#include "attribs.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum event_mgr_ev_e
{
    EVENT_MGR_EV_NONE = 0,
    EVENT_MGR_EV_WIFI_CONNECTED,
    EVENT_MGR_EV_WIFI_DISCONNECTED,
    EVENT_MGR_EV_ETH_CONNECTED,
    EVENT_MGR_EV_ETH_DISCONNECTED,
    EVENT_MGR_EV_TIME_SYNCHRONIZED,
    EVENT_MGR_EV_GW_CFG_READY,
    EVENT_MGR_EV_GW_CFG_CHANGED_RUUVI,
    EVENT_MGR_EV_GW_CFG_CHANGED_ETH,
    EVENT_MGR_EV_GW_CFG_CHANGED_WIFI,
    EVENT_MGR_EV_GW_CFG_CHANGED_RUUVI_NTP_USE,
    EVENT_MGR_EV_GW_CFG_CHANGED_RUUVI_NTP_USE_DHCP,
    EVENT_MGR_EV_LAST,
} event_mgr_ev_e;

typedef struct event_mgr_t event_mgr_t;

typedef struct event_mgr_ev_info_static_t
{
    void *          stub1;
    void *          stub2;
    void *          stub3;
    os_signal_num_e stub4;
    bool            stub5;
} event_mgr_ev_info_static_t;

bool
event_mgr_init(void);

ATTR_UNUSED
void
event_mgr_deinit(void);

bool
event_mgr_subscribe_sig(const event_mgr_ev_e event, os_signal_t *const p_signal, const os_signal_num_e sig_num);

void
event_mgr_subscribe_sig_static(
    event_mgr_ev_info_static_t *const p_ev_info_mem,
    const event_mgr_ev_e              event,
    os_signal_t *const                p_signal,
    const os_signal_num_e             sig_num);

void
event_mgr_notify(const event_mgr_ev_e event);

#ifdef __cplusplus
}
#endif

#endif // RUUVI_GATEWAY_ESP_EVENT_MGR_H
