/**
 * @file adv_mqtt_internal.h
 * @author TheSomeMan
 * @date 2023-10-27
 * @copyright Ruuvi Innovations Ltd, license BSD-3-Clause.
 */

#ifndef RUUVI_GATEWAY_ESP_ADV_MQTT_INTERNAL_H
#define RUUVI_GATEWAY_ESP_ADV_MQTT_INTERNAL_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define ADV_MQTT_TASK_WATCHDOG_FEEDING_PERIOD_TICKS  pdMS_TO_TICKS(1000)
#define ADV_MQTT_TASK_RETRY_SENDING_ADVS_AFTER_TICKS pdMS_TO_TICKS(50)

typedef struct adv_mqtt_state_t
{
    bool flag_stop;
} adv_mqtt_state_t;

#ifdef __cplusplus
}
#endif

#endif // RUUVI_GATEWAY_ESP_ADV_MQTT_INTERNAL_H
