/**
 * @file adv_post.h
 * @author Oleg Protasevich
 * @date 2020-09-11
 * @copyright Ruuvi Innovations Ltd, license BSD-3-Clause.
 */

#ifndef RUUVI_ADV_POST_H
#define RUUVI_ADV_POST_H

#include "ruuvi_gateway.h"
#include <stdbool.h>
#include "gw_cfg.h"

#ifdef __cplusplus
extern "C" {
#endif

void
adv_post_init(void);

void
adv_post_set_period(const uint32_t period_ms);

void
adv_post_stop(void);

#ifdef __cplusplus
}
#endif

#endif // RUUVI_ADV_POST_H
