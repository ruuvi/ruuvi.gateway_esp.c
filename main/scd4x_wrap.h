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
#include "ruuvi_endpoint_6.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct scd4x_wrap_serial_num_t
{
    uint16_t serial_0;
    uint16_t serial_1;
    uint16_t serial_2;
} scd4x_wrap_serial_num_t;

typedef uint16_t scd4x_wrap_raw_co2_t;
typedef int32_t  scd4x_wrap_raw_temperature_m_deg_c_t;
typedef int32_t  scd4x_wrap_raw_humidity_m_percent_rh_t;

typedef struct scd4x_wrap_measurement_t
{
    scd4x_wrap_raw_co2_t                   co2;
    scd4x_wrap_raw_temperature_m_deg_c_t   temperature_m_deg_c;
    scd4x_wrap_raw_humidity_m_percent_rh_t humidity_m_percent_rh;
} scd4x_wrap_measurement_t;

bool
scd4x_wrap_start_periodic_measurement(void);

bool
scd4x_wrap_read_measurement(scd4x_wrap_measurement_t* const p_measurement);

bool
scd4x_wrap_check(void);

re_float
scd4x_wrap_conv_raw_to_float_co2(const scd4x_wrap_raw_co2_t raw_co2);

re_float
scd4x_wrap_conv_raw_to_float_temperature(const scd4x_wrap_raw_temperature_m_deg_c_t temperature_m_deg_c);

re_float
scd4x_wrap_conv_raw_to_float_humidity(const scd4x_wrap_raw_humidity_m_percent_rh_t humidity_m_percent_rh);

#ifdef __cplusplus
}
#endif

#endif // RUUVI_GATEWAY_ESP_SCD4X_WRAP_H
