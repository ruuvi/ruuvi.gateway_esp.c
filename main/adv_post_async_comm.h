/**
 * @file adv_post_async_comm.h
 * @author TheSomeMan
 * @date 2023-09-04
 * @copyright Ruuvi Innovations Ltd, license BSD-3-Clause.
 */

#ifndef RUUVI_GATEWAY_ESP_ADV_POST_ASYNC_COMM_H
#define RUUVI_GATEWAY_ESP_ADV_POST_ASYNC_COMM_H

#include <stdint.h>
#include "adv_post_internal.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum adv_post_action_e
{
    ADV_POST_ACTION_NONE = 0,
    ADV_POST_ACTION_POST_ADVS_TO_RUUVI,
    ADV_POST_ACTION_POST_ADVS_TO_CUSTOM,
    ADV_POST_ACTION_POST_STATS,
    ADV_POST_ACTION_POST_ADVS_TO_MQTT,
} adv_post_action_e;

void
adv_post_async_comm_init(void);

void
adv_post_do_async_comm(adv_post_state_t* const p_adv_post_state);

void
adv_post_set_default_period(const uint32_t period_ms);

bool
adv_post_set_hmac_sha256_key(const char* const p_key_str);

void
adv_post_set_adv_post_http_action(const bool flag_post_to_ruuvi);

adv_post_action_e
adv_post_get_adv_post_action(void);

#ifdef __cplusplus
}
#endif

#endif // RUUVI_GATEWAY_ESP_ADV_POST_ASYNC_COMM_H
