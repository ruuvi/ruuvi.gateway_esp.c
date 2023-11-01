/**
 * @file adv_mqtt.c
 * @author TheSomeMan
 * @date 2023-10-27
 * @copyright Ruuvi Innovations Ltd, license BSD-3-Clause.
 */

#ifndef RUUVI_GATEWAY_ESP_ADV_MQTT_H
#define RUUVI_GATEWAY_ESP_ADV_MQTT_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

void
adv_mqtt_init(void);

bool
adv_mqtt_is_initialized(void);

void
adv_mqtt_stop(void);

#ifdef __cplusplus
}
#endif

#endif // RUUVI_GATEWAY_ESP_ADV_MQTT_H
