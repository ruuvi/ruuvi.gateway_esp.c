/**
 * @file reset_task.h
 * @author TheSomeMan
 * @date 2021-03-31
 * @copyright Ruuvi Innovations Ltd, license BSD-3-Clause.
 */

#ifndef RUUVI_GATEWAY_ESP_RESET_TASK_H
#define RUUVI_GATEWAY_ESP_RESET_TASK_H

#include <stdbool.h>
#include <stdint.h>
#include "attribs.h"

#ifdef __cplusplus
extern "C" {
#endif

extern volatile uint32_t g_cnt_cfg_button_pressed;
extern volatile uint32_t g_uptime_counter;

bool
reset_task_init(void);

void
reset_task_reboot_after_timeout(void);

void
reset_task_notify_configure_button_event(void);

void
gateway_restart(const char* const p_msg);

void
gateway_restart_low_memory(void);

#ifdef __cplusplus
}
#endif

#endif // RUUVI_GATEWAY_ESP_RESET_TASK_H
