/**
 * @file reset_task.h
 * @author TheSomeMan
 * @date 2021-03-31
 * @copyright Ruuvi Innovations Ltd, license BSD-3-Clause.
 */

#ifndef RUUVI_GATEWAY_ESP_RESET_TASK_H
#define RUUVI_GATEWAY_ESP_RESET_TASK_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

bool
reset_task_init(void);

void
reset_task_start_timer_after_hotspot_activation(void);

void
reset_task_stop_timer_after_hotspot_activation(void);

void
reset_task_notify_configure_button_pressed(void);

void
reset_task_notify_configure_button_released(void);

void
reset_task_activate_after_delay(void);

#ifdef __cplusplus
}
#endif

#endif // RUUVI_GATEWAY_ESP_RESET_TASK_H
