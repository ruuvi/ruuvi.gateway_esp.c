/**
 * @file i2c_task.h
 * @author TheSomeMan
 * @date 2023-07-03
 * @copyright Ruuvi Innovations Ltd, license BSD-3-Clause.
 */

#ifndef RUUVI_GATEWAY_ESP_I2C_TASK_H
#define RUUVI_GATEWAY_ESP_I2C_TASK_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

bool
i2c_task_init(void);

#ifdef __cplusplus
}
#endif

#endif // RUUVI_GATEWAY_ESP_I2C_TASK_H
