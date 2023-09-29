/**
 * @file adv_post_task.h
 * @author TheSomeMan
 * @date 2023-09-04
 * @copyright Ruuvi Innovations Ltd, license BSD-3-Clause.
 */

#ifndef RUUVI_GATEWAY_ESP_ADV_POST_TASK_H
#define RUUVI_GATEWAY_ESP_ADV_POST_TASK_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

void
adv_post_task(void);

bool
adv_post_task_is_initialized(void);

void
adv_post_task_stop(void);

#ifdef __cplusplus
}
#endif

#endif // RUUVI_GATEWAY_ESP_ADV_POST_TASK_H
