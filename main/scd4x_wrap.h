/**
 * @file scd4x_wrap.h
 * @author TheSomeMan
 * @date 2023-07-03
 * @copyright Ruuvi Innovations Ltd, license BSD-3-Clause.
 */

#ifndef RUUVI_GATEWAY_ESP_SCD4X_WRAP_H
#define RUUVI_GATEWAY_ESP_SCD4X_WRAP_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct scd4x_wrap_serial_num_t
{
    uint16_t serial_0;
    uint16_t serial_1;
    uint16_t serial_2;
} scd4x_wrap_serial_num_t;


typedef struct scd4x_wrap_measurement_t
{
    uint16_t co2;
    int32_t temperature;
    int32_t humidity;
} scd4x_wrap_measurement_t;

bool
scd4x_wrap_start_periodic_measurement(void);

bool
scd4x_wrap_read_measurement(scd4x_wrap_measurement_t* const p_measurement);

bool
scd4x_wrap_check(void);

#ifdef __cplusplus
}
#endif

#endif // RUUVI_GATEWAY_ESP_SCD4X_WRAP_H
