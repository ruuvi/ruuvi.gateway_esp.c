/**
 * @file adv_mqtt_events.h
 * @author TheSomeMan
 * @date 2023-10-27
 * @copyright Ruuvi Innovations Ltd, license BSD-3-Clause.
 */

#ifndef RUUVI_GATEWAY_ESP_ADV_MQTT_EVENTS_H
#define RUUVI_GATEWAY_ESP_ADV_MQTT_EVENTS_H

#ifdef __cplusplus
extern "C" {
#endif

void
adv_mqtt_subscribe_events(void);

void
adv_mqtt_unsubscribe_events(void);

#ifdef __cplusplus
}
#endif

#endif // RUUVI_GATEWAY_ESP_ADV_MQTT_EVENTS_H
