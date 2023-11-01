/**
 * @file adv_mqtt_signals.h
 * @author TheSomeMan
 * @date 2023-10-27
 * @copyright Ruuvi Innovations Ltd, license BSD-3-Clause.
 */

#ifndef RUUVI_GATEWAY_ESP_ADV_MQTT_SIGNALS_H
#define RUUVI_GATEWAY_ESP_ADV_MQTT_SIGNALS_H

#include "os_signal.h"
#include "adv_mqtt_internal.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum adv_mqtt_sig_e
{
    ADV_MQTT_SIG_STOP                  = OS_SIGNAL_NUM_0,
    ADV_MQTT_SIG_ON_RECV_ADV           = OS_SIGNAL_NUM_1,
    ADV_MQTT_SIG_MQTT_CONNECTED        = OS_SIGNAL_NUM_2,
    ADV_MQTT_SIG_TASK_WATCHDOG_FEED    = OS_SIGNAL_NUM_3,
    ADV_MQTT_SIG_GW_CFG_READY          = OS_SIGNAL_NUM_4,
    ADV_MQTT_SIG_GW_CFG_CHANGED_RUUVI  = OS_SIGNAL_NUM_5,
    ADV_MQTT_SIG_RELAYING_MODE_CHANGED = OS_SIGNAL_NUM_6,
    ADV_MQTT_SIG_CFG_MODE_ACTIVATED    = OS_SIGNAL_NUM_7,
    ADV_MQTT_SIG_CFG_MODE_DEACTIVATED  = OS_SIGNAL_NUM_8,
} adv_mqtt_sig_e;

#define ADV_MQTT_SIG_FIRST (ADV_MQTT_SIG_STOP)
#define ADV_MQTT_SIG_LAST  (ADV_MQTT_SIG_CFG_MODE_DEACTIVATED)

ATTR_PURE
os_signal_num_e
adv_mqtt_conv_to_sig_num(const adv_mqtt_sig_e sig);

adv_mqtt_sig_e
adv_mqtt_conv_from_sig_num(const os_signal_num_e sig_num);

os_signal_t*
adv_mqtt_signals_get(void);

void
adv_mqtt_signals_init(void);

void
adv_mqtt_signals_deinit(void);

void
adv_mqtt_signals_send_sig(const adv_mqtt_sig_e adv_mqtt_sig);

bool
adv_mqtt_signals_is_thread_registered(void);

bool
adv_mqtt_handle_sig(const adv_mqtt_sig_e adv_mqtt_sig, adv_mqtt_state_t* const p_adv_mqtt_state);

#ifdef __cplusplus
}
#endif

#endif // RUUVI_GATEWAY_ESP_ADV_MQTT_SIGNALS_H
