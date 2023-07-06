/**
 * @file sen5x_wrap.h
 * @author TheSomeMan
 * @date 2023-07-03
 * @copyright Ruuvi Innovations Ltd, license BSD-3-Clause.
 */

#ifndef RUUVI_GATEWAY_ESP_SEN5X_WRAP_H
#define RUUVI_GATEWAY_ESP_SEN5X_WRAP_H

#include <stdbool.h>
#include <stdint.h>
#include "ruuvi_endpoint_6.h"

#ifdef __cplusplus
extern "C" {
#endif

#define SEN5X_WRAP_SERIAL_NUMBER_SIZE (32U)
#define SEN5X_WRAP_PRODUCT_NAME_SIZE  (32U)

#define SEN5X_INVALID_RAW_VALUE_PM          (0xFFFFU)
#define SEN5X_INVALID_RAW_VALUE_HUMIDITY    (0x7FFFU)
#define SEN5X_INVALID_RAW_VALUE_TEMPERATURE (0x7FFFU)
#define SEN5X_INVALID_RAW_VALUE_VOC         (0x7FFFU)
#define SEN5X_INVALID_RAW_VALUE_NOX         (0x7FFFU)

typedef struct sen5x_wrap_serial_number_t
{
    uint8_t serial_number[SEN5X_WRAP_SERIAL_NUMBER_SIZE];
} sen5x_wrap_serial_number_t;

typedef struct sen5x_wrap_product_name_t
{
    uint8_t product_name[SEN5X_WRAP_PRODUCT_NAME_SIZE];
} sen5x_wrap_product_name_t;

typedef struct sen5x_wrap_version_t
{
    uint8_t firmware_major;
    uint8_t firmware_minor;
    bool    firmware_debug;
    uint8_t hardware_major;
    uint8_t hardware_minor;
    uint8_t protocol_major;
    uint8_t protocol_minor;
} sen5x_wrap_version_t;

typedef struct sen5x_wrap_measurement_t
{
    uint16_t mass_concentration_pm1p0;
    uint16_t mass_concentration_pm2p5;
    uint16_t mass_concentration_pm4p0;
    uint16_t mass_concentration_pm10p0;
    int16_t  ambient_humidity;
    int16_t  ambient_temperature;
    int16_t  voc_index;
    int16_t  nox_index;
} sen5x_wrap_measurement_t;

bool
sen5x_wrap_start_measurement(void);

bool
sen5x_wrap_read_measured_values(sen5x_wrap_measurement_t* const p_measurement);

bool
sen5x_wrap_check(void);

re_float
sen5x_wrap_conv_raw_to_float_pm(const uint16_t raw_pm);

re_float
sen5x_wrap_conv_raw_to_float_humidity(const int16_t raw_humidity);

re_float
sen5x_wrap_conv_raw_to_float_temperature(const int16_t temperature);

re_float
sen5x_wrap_conv_raw_to_float_voc_index(const int16_t raw_voc_index);

re_float
sen5x_wrap_conv_raw_to_float_nox_index(const int16_t raw_nox_index);

#ifdef __cplusplus
}
#endif

#endif // RUUVI_GATEWAY_ESP_SEN5X_WRAP_H
