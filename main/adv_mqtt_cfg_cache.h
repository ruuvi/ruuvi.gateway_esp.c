/**
 * @file adv_mqtt_cfg_cache.h
 * @author TheSomeMan
 * @date 2023-10-27
 * @copyright Ruuvi Innovations Ltd, license BSD-3-Clause.
 */

#ifndef RUUVI_GATEWAY_ESP_ADV_MQTT_CFG_CACHE_H
#define RUUVI_GATEWAY_ESP_ADV_MQTT_CFG_CACHE_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct adv_mqtt_cfg_cache_t
{
    bool flag_use_ntp;
    bool flag_mqtt_instant_mode_active;
} adv_mqtt_cfg_cache_t;

void
adv_mqtt_cfg_cache_init(void);

void
adv_mqtt_cfg_cache_deinit(void);

adv_mqtt_cfg_cache_t*
adv_mqtt_cfg_cache_mutex_lock(void);

void
adv_mqtt_cfg_cache_mutex_unlock(adv_mqtt_cfg_cache_t** p_p_cfg_cache);

#ifdef __cplusplus
}
#endif

#endif // RUUVI_GATEWAY_ESP_ADV_MQTT_CFG_CACHE_H
