/**
 * @file gpio_switch_ctrl.h
 * @author TheSomeMan
 * @date 2021-04-03
 * @copyright Ruuvi Innovations Ltd, license BSD-3-Clause.
 */

#ifndef RUUVI_GATEWAY_GPIO_SWITCH_CTRL_H
#define RUUVI_GATEWAY_GPIO_SWITCH_CTRL_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

void
gpio_switch_ctrl_init(void);

void
gpio_switch_ctrl_activate(void);

void
gpio_switch_ctrl_deactivate(void);

#ifdef __cplusplus
}
#endif

#endif // RUUVI_GATEWAY_GPIO_SWITCH_CTRL_H
