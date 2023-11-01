/**
 * @file adv_post_signals.h
 * @author TheSomeMan
 * @date 2023-09-04
 * @copyright Ruuvi Innovations Ltd, license BSD-3-Clause.
 */

#ifndef RUUVI_GATEWAY_ESP_ADV_POST_SIGNALS_H
#define RUUVI_GATEWAY_ESP_ADV_POST_SIGNALS_H

#include "os_signal.h"
#include "adv_post_internal.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum adv_post_sig_e
{
    ADV_POST_SIG_STOP                  = OS_SIGNAL_NUM_0,
    ADV_POST_SIG_NETWORK_DISCONNECTED  = OS_SIGNAL_NUM_1,
    ADV_POST_SIG_NETWORK_CONNECTED     = OS_SIGNAL_NUM_2,
    ADV_POST_SIG_TIME_SYNCHRONIZED     = OS_SIGNAL_NUM_3,
    ADV_POST_SIG_RETRANSMIT            = OS_SIGNAL_NUM_4,
    ADV_POST_SIG_RETRANSMIT2           = OS_SIGNAL_NUM_5,
    ADV_POST_SIG_RETRANSMIT_MQTT       = OS_SIGNAL_NUM_6,
    ADV_POST_SIG_SEND_STATISTICS       = OS_SIGNAL_NUM_7,
    ADV_POST_SIG_DO_ASYNC_COMM         = OS_SIGNAL_NUM_8,
    ADV_POST_SIG_RELAYING_MODE_CHANGED = OS_SIGNAL_NUM_9,
    ADV_POST_SIG_NETWORK_WATCHDOG      = OS_SIGNAL_NUM_10,
    ADV_POST_SIG_TASK_WATCHDOG_FEED    = OS_SIGNAL_NUM_11,
    ADV_POST_SIG_GW_CFG_READY          = OS_SIGNAL_NUM_12,
    ADV_POST_SIG_GW_CFG_CHANGED_RUUVI  = OS_SIGNAL_NUM_13,
    ADV_POST_SIG_BLE_SCAN_CHANGED      = OS_SIGNAL_NUM_14,
    ADV_POST_SIG_CFG_MODE_ACTIVATED    = OS_SIGNAL_NUM_15,
    ADV_POST_SIG_CFG_MODE_DEACTIVATED  = OS_SIGNAL_NUM_16,
    ADV_POST_SIG_GREEN_LED_TURN_ON     = OS_SIGNAL_NUM_17,
    ADV_POST_SIG_GREEN_LED_TURN_OFF    = OS_SIGNAL_NUM_18,
    ADV_POST_SIG_GREEN_LED_UPDATE      = OS_SIGNAL_NUM_19,
    ADV_POST_SIG_RECV_ADV_TIMEOUT      = OS_SIGNAL_NUM_20,
} adv_post_sig_e;

#define ADV_POST_SIG_FIRST (ADV_POST_SIG_STOP)
#define ADV_POST_SIG_LAST  (ADV_POST_SIG_RECV_ADV_TIMEOUT)

ATTR_PURE
os_signal_num_e
adv_post_conv_to_sig_num(const adv_post_sig_e sig);

adv_post_sig_e
adv_post_conv_from_sig_num(const os_signal_num_e sig_num);

os_signal_t*
adv_post_signals_get(void);

void
adv_post_signals_init(void);

void
adv_post_signals_deinit(void);

void
adv_post_signals_send_sig(const adv_post_sig_e adv_post_sig);

bool
adv_post_signals_is_thread_registered(void);

bool
adv_post_handle_sig(const adv_post_sig_e adv_post_sig, adv_post_state_t* const p_adv_post_state);

#ifdef __cplusplus
}
#endif

#endif // RUUVI_GATEWAY_ESP_ADV_POST_SIGNALS_H
