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

#ifdef __cplusplus
extern "C" {
#endif

#define SEN5X_WRAP_SERIAL_NUMBER_SIZE (32U)
#define SEN5X_WRAP_PRODUCT_NAME_SIZE  (32U)

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

#ifdef __cplusplus
}
#endif

#endif // RUUVI_GATEWAY_ESP_SEN5X_WRAP_H
