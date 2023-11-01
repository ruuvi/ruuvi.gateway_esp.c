/**
 * @file adv_mqtt_events.c
 * @author TheSomeMan
 * @date 2023-10-27
 * @copyright Ruuvi Innovations Ltd, license BSD-3-Clause.
 */

#include "adv_mqtt_events.h"
#include "event_mgr.h"
#include "adv_mqtt_signals.h"

typedef struct adv_mqtt_event_to_sig_t
{
    event_mgr_ev_e ev;
    adv_mqtt_sig_e sig;
} adv_mqtt_event_to_sig_t;

enum
{
    ADV_MQTT_EVENTS_SIZE = 7U,
};

static const adv_mqtt_event_to_sig_t g_adv_mqtt_ev_to_sig[ADV_MQTT_EVENTS_SIZE] = {
    { EVENT_MGR_EV_RECV_ADV, ADV_MQTT_SIG_ON_RECV_ADV },
    { EVENT_MGR_EV_MQTT_CONNECTED, ADV_MQTT_SIG_MQTT_CONNECTED },
    { EVENT_MGR_EV_GW_CFG_READY, ADV_MQTT_SIG_GW_CFG_READY },
    { EVENT_MGR_EV_GW_CFG_CHANGED_RUUVI, ADV_MQTT_SIG_GW_CFG_CHANGED_RUUVI },
    { EVENT_MGR_EV_RELAYING_MODE_CHANGED, ADV_MQTT_SIG_RELAYING_MODE_CHANGED },
    { EVENT_MGR_EV_CFG_MODE_ACTIVATED, ADV_MQTT_SIG_CFG_MODE_ACTIVATED },
    { EVENT_MGR_EV_CFG_MODE_DEACTIVATED, ADV_MQTT_SIG_CFG_MODE_DEACTIVATED },
};

static event_mgr_ev_info_static_t g_adv_mqtt_ev_info_mem[OS_ARRAY_SIZE(g_adv_mqtt_ev_to_sig)];

void
adv_mqtt_subscribe_events(void)
{
    for (uint32_t i = 0; i < OS_ARRAY_SIZE(g_adv_mqtt_ev_to_sig); ++i)
    {
        const adv_mqtt_event_to_sig_t* const p_ev_to_sig = &g_adv_mqtt_ev_to_sig[i];
        event_mgr_subscribe_sig_static(
            &g_adv_mqtt_ev_info_mem[i],
            p_ev_to_sig->ev,
            adv_mqtt_signals_get(),
            adv_mqtt_conv_to_sig_num(p_ev_to_sig->sig));
    }
}

void
adv_mqtt_unsubscribe_events(void)
{
    for (uint32_t i = 0; i < OS_ARRAY_SIZE(g_adv_mqtt_ev_to_sig); ++i)
    {
        const adv_mqtt_event_to_sig_t* const p_ev_to_sig = &g_adv_mqtt_ev_to_sig[i];
        event_mgr_unsubscribe_sig_static(&g_adv_mqtt_ev_info_mem[i], p_ev_to_sig->ev);
    }
}
