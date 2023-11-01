/**
 * @file mqtt.h
 * @author Jukka Saari
 * @date 2019-11-27
 * @copyright Ruuvi Innovations Ltd, license BSD-3-Clause.
 */

#ifndef RUUVI_MQTT_H
#define RUUVI_MQTT_H

#include "adv_table.h"
#include "gw_cfg.h"
#include "str_buf.h"

#ifdef __cplusplus
extern "C" {
#endif

void
mqtt_app_start(const ruuvi_gw_cfg_mqtt_t* const p_mqtt);

void
mqtt_app_start_with_gw_cfg(void);

void
mqtt_app_stop(void);

bool
mqtt_app_is_working(void);

bool
mqtt_is_buffer_available_for_publish(void);

bool
mqtt_publish_adv(const adv_report_t* const p_adv, const bool flag_use_timestamps, const time_t timestamp);

void
mqtt_publish_connect(void);

str_buf_t
mqtt_app_get_error_message(void);

#ifdef __cplusplus
}
#endif

#endif // RUUVI_MQTT_H
