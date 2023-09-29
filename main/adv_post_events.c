/**
 * @file adv_post_events.c
 * @author TheSomeMan
 * @date 2023-09-04
 * @copyright Ruuvi Innovations Ltd, license BSD-3-Clause.
 */

#include "adv_post_events.h"
#include "event_mgr.h"
#include "adv_post_signals.h"

typedef struct event_mgr_ev_to_sig_t
{
    event_mgr_ev_e ev;
    adv_post_sig_e sig;
} event_mgr_ev_to_sig_t;

enum
{
    ADV_POST_EVENTS_SIZE = 11U,
};

static const event_mgr_ev_to_sig_t g_adv_post_ev_to_sig[ADV_POST_EVENTS_SIZE] = {
    { EVENT_MGR_EV_RECV_ADV, ADV_POST_SIG_ON_RECV_ADV },
    { EVENT_MGR_EV_WIFI_DISCONNECTED, ADV_POST_SIG_NETWORK_DISCONNECTED },
    { EVENT_MGR_EV_ETH_DISCONNECTED, ADV_POST_SIG_NETWORK_DISCONNECTED },
    { EVENT_MGR_EV_WIFI_CONNECTED, ADV_POST_SIG_NETWORK_CONNECTED },
    { EVENT_MGR_EV_ETH_CONNECTED, ADV_POST_SIG_NETWORK_CONNECTED },
    { EVENT_MGR_EV_TIME_SYNCHRONIZED, ADV_POST_SIG_TIME_SYNCHRONIZED },
    { EVENT_MGR_EV_GW_CFG_READY, ADV_POST_SIG_GW_CFG_READY },
    { EVENT_MGR_EV_GW_CFG_CHANGED_RUUVI, ADV_POST_SIG_GW_CFG_CHANGED_RUUVI },
    { EVENT_MGR_EV_RELAYING_MODE_CHANGED, ADV_POST_SIG_RELAYING_MODE_CHANGED },
    { EVENT_MGR_EV_GREEN_LED_TURN_ON, ADV_POST_SIG_GREEN_LED_TURN_ON },
    { EVENT_MGR_EV_GREEN_LED_TURN_OFF, ADV_POST_SIG_GREEN_LED_TURN_OFF },
};

static event_mgr_ev_info_static_t g_adv_post_ev_info_mem[OS_ARRAY_SIZE(g_adv_post_ev_to_sig)];

void
adv_post_subscribe_events(void)
{
    for (uint32_t i = 0; i < OS_ARRAY_SIZE(g_adv_post_ev_to_sig); ++i)
    {
        const event_mgr_ev_to_sig_t* const p_ev_to_sig = &g_adv_post_ev_to_sig[i];
        event_mgr_subscribe_sig_static(
            &g_adv_post_ev_info_mem[i],
            p_ev_to_sig->ev,
            adv_post_signals_get(),
            adv_post_conv_to_sig_num(p_ev_to_sig->sig));
    }
}

void
adv_post_unsubscribe_events(void)
{
    for (uint32_t i = 0; i < OS_ARRAY_SIZE(g_adv_post_ev_to_sig); ++i)
    {
        const event_mgr_ev_to_sig_t* const p_ev_to_sig = &g_adv_post_ev_to_sig[i];
        event_mgr_unsubscribe_sig_static(&g_adv_post_ev_info_mem[i], p_ev_to_sig->ev);
    }
}
