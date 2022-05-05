/**
 * @file mqtt_json.h
 * @author TheSomeMan
 * @date 2021-02-04
 * @copyright Ruuvi Innovations Ltd, license BSD-3-Clause.
 */

#ifndef RUUVI_GATEWAY_ESP_MQTT_JSON_H
#define RUUVI_GATEWAY_ESP_MQTT_JSON_H

#include <stdbool.h>
#include "adv_table.h"
#include "cjson_wrap.h"

#ifdef __cplusplus
extern "C" {
#endif

bool
mqtt_create_json_str(
    const adv_report_t *const      p_adv,
    const bool                     flag_use_timestamps,
    const time_t                   timestamp,
    const mac_address_str_t *const p_mac_addr,
    const char *const              p_coordinates_str,
    cjson_wrap_str_t *const        p_json_str);

#ifdef __cplusplus
}
#endif

#endif // RUUVI_GATEWAY_ESP_MQTT_JSON_H
