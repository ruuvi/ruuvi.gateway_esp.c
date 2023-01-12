/**
 * @file adv_post.h
 * @author Oleg Protasevich
 * @date 2020-09-11
 * @copyright Ruuvi Innovations Ltd, license BSD-3-Clause.
 */

#ifndef RUUVI_ADV_POST_H
#define RUUVI_ADV_POST_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void
adv_post_init(void);

bool
adv_post_is_initialized(void);

void
adv_post_set_period(const uint32_t period_ms);

void
adv_post_stop(void);

void
adv_post_last_successful_network_comm_timestamp_update(void);

#ifdef __cplusplus
}
#endif

#endif // RUUVI_ADV_POST_H
