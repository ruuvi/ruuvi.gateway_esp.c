/**
 * @file adv_mqtt_timers.h
 * @author TheSomeMan
 * @date 2023-10-27
 * @copyright Ruuvi Innovations Ltd, license BSD-3-Clause.
 */

#ifndef RUUVI_GATEWAY_ESP_ADV_MQTT_TIMERS_H
#define RUUVI_GATEWAY_ESP_ADV_MQTT_TIMERS_H

#include "os_timer_sig.h"

#ifdef __cplusplus
extern "C" {
#endif

void
adv_mqtt_create_timers(void);

void
adv_mqtt_delete_timers(void);

void
adv_mqtt_timers_start_timer_sig_watchdog_feed(void);

void
adv_mqtt_timers_start_timer_sig_retry_sending_advs(void);

#ifdef __cplusplus
}
#endif

#endif // RUUVI_GATEWAY_ESP_ADV_MQTT_TIMERS_H
