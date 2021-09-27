/**
 * @file mqtt.h
 * @author Jukka Saari
 * @date 2019-11-27
 * @copyright Ruuvi Innovations Ltd, license BSD-3-Clause.
 */

#ifndef RUUVI_MQTT_H
#define RUUVI_MQTT_H

#include "adv_table.h"

#ifdef __cplusplus
extern "C" {
#endif

void
mqtt_app_start(void);

void
mqtt_app_stop(void);

bool
mqtt_publish_adv(const adv_report_t *const p_adv);

#ifdef __cplusplus
}
#endif

#endif // RUUVI_MQTT_H
