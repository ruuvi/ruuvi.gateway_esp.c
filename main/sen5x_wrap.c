/**
 * @file sen5x_wrap.c
 * @author TheSomeMan
 * @date 2023-07-03
 * @copyright Ruuvi Innovations Ltd, license BSD-3-Clause.
 */

#include "sen5x_wrap.h"
#include <string.h>
#include <math.h>
#include "sen5x_i2c.h"
#include "sensirion_common.h"
#include "ruuvi_endpoint_6.h"
#define LOG_LOCAL_LEVEL LOG_LEVEL_INFO
#include "log.h"

#define SEN5X_WRAP_NUM_RETRIES (3U)

#define SEN5X_INVALID_RAW_VALUE_PM          (0xFFFFU)
#define SEN5X_INVALID_RAW_VALUE_HUMIDITY    (0x7FFFU)
#define SEN5X_INVALID_RAW_VALUE_TEMPERATURE (0x7FFFU)
#define SEN5X_INVALID_RAW_VALUE_VOC         (0x7FFFU)
#define SEN5X_INVALID_RAW_VALUE_NOX         (0x7FFFU)

#define SEN5X_SCALE_FACTOR_PM          (10)
#define SEN5X_SCALE_FACTOR_HUMIDITY    (100)
#define SEN5X_SCALE_FACTOR_TEMPERATURE (200)
#define SEN5X_SCALE_FACTOR_VOC_INDEX   (10)
#define SEN5X_SCALE_FACTOR_NOX_INDEX   (10)

static const char* TAG = "SEN5X";

static bool g_sen5x_wrap_is_sen55 = false;

static bool
sen5x_wrap_device_reset(void)
{
    bool flag_success = false;
    for (uint32_t i = 0; i < SEN5X_WRAP_NUM_RETRIES; ++i)
    {
        const int16_t error = sen5x_device_reset();
        if (NO_ERROR == error)
        {
            flag_success = true;
            break;
        }
        LOG_ERR("%s[retry=%d]: err=%d", "sen5x_device_reset", i, error);
    }
    return flag_success;
}

static bool
sen5x_wrap_get_serial_number(sen5x_wrap_serial_number_t* const p_serial_num)
{
    bool flag_success = false;
    for (uint32_t i = 0; i < SEN5X_WRAP_NUM_RETRIES; ++i)
    {
        const int16_t error = sen5x_get_serial_number(p_serial_num->serial_number, sizeof(p_serial_num->serial_number));
        if (NO_ERROR == error)
        {
            flag_success = true;
            break;
        }
        LOG_ERR("%s[retry=%d]: err=%d", "sen5x_get_serial_number", i, error);
    }
    return flag_success;
}

static bool
sen5x_wrap_get_product_name(sen5x_wrap_product_name_t* const p_product_name)
{
    bool flag_success = false;
    for (uint32_t i = 0; i < SEN5X_WRAP_NUM_RETRIES; ++i)
    {
        const int16_t error = sen5x_get_product_name(
            p_product_name->product_name,
            sizeof(p_product_name->product_name));
        if (NO_ERROR == error)
        {
            flag_success = true;
            break;
        }
        LOG_ERR("%s[retry=%d]: err=%d", "sen5x_get_product_name", i, error);
    }
    return flag_success;
}

static bool
sen5x_wrap_get_version(sen5x_wrap_version_t* const p_ver)
{
    bool flag_success = false;
    for (uint32_t i = 0; i < SEN5X_WRAP_NUM_RETRIES; ++i)
    {
        const int16_t error = sen5x_get_version(
            &p_ver->firmware_major,
            &p_ver->firmware_minor,
            &p_ver->firmware_debug,
            &p_ver->hardware_major,
            &p_ver->hardware_minor,
            &p_ver->protocol_major,
            &p_ver->protocol_minor);
        if (NO_ERROR == error)
        {
            flag_success = true;
            break;
        }
        LOG_ERR("%s[retry=%d]: err=%d", "sen5x_get_version", i, error);
    }
    return flag_success;
}

bool
sen5x_wrap_start_measurement(void)
{
    bool flag_success = false;
    for (uint32_t i = 0; i < SEN5X_WRAP_NUM_RETRIES; ++i)
    {
        const int16_t error = sen5x_start_measurement();
        if (NO_ERROR == error)
        {
            flag_success = true;
            break;
        }
        LOG_ERR("%s[retry=%d]: err=%d", "sen5x_start_measurement", i, error);
    }
    return flag_success;
}

static bool
sen5x_wrap_read_data_ready(bool* const p_flag_data_ready)
{
    bool flag_success = false;
    for (uint32_t i = 0; i < SEN5X_WRAP_NUM_RETRIES; ++i)
    {
        const int16_t error = sen5x_read_data_ready(p_flag_data_ready);
        if (NO_ERROR == error)
        {
            flag_success = true;
            break;
        }
        LOG_ERR("%s[retry=%d]: err=%d", "sen5x_read_data_ready", i, error);
    }
    return flag_success;
}

bool
sen5x_wrap_read_measured_values(sen5x_wrap_measurement_t* const p_measurement)
{
    bool flag_data_ready = false;
    if (!sen5x_wrap_read_data_ready(&flag_data_ready))
    {
        return false;
    }
    if (!flag_data_ready)
    {
        return false;
    }

    bool flag_success = false;
    for (uint32_t i = 0; i < SEN5X_WRAP_NUM_RETRIES; ++i)
    {
        const int16_t error = sen5x_read_measured_values(
            &p_measurement->mass_concentration_pm1p0,
            &p_measurement->mass_concentration_pm2p5,
            &p_measurement->mass_concentration_pm4p0,
            &p_measurement->mass_concentration_pm10p0,
            &p_measurement->ambient_humidity,
            &p_measurement->ambient_temperature,
            &p_measurement->voc_index,
            &p_measurement->nox_index);
        if (NO_ERROR == error)
        {
            flag_success = true;
            break;
        }
        LOG_ERR("%s[retry=%d]: err=%d", "sen5x_read_measured_values", i, error);
    }
    if (flag_success)
    {
        LOG_DBG(
            "PM=%u,%u,%u,%u/%u µg/m³, H=%d/%u %%RH, T=%d/%u °C, VOC=%d/%u, NOX=%d/%u",
            p_measurement->mass_concentration_pm1p0,
            p_measurement->mass_concentration_pm2p5,
            p_measurement->mass_concentration_pm4p0,
            p_measurement->mass_concentration_pm10p0,
            SEN5X_SCALE_FACTOR_PM,
            p_measurement->ambient_humidity,
            SEN5X_SCALE_FACTOR_HUMIDITY,
            p_measurement->ambient_temperature,
            SEN5X_SCALE_FACTOR_TEMPERATURE,
            p_measurement->voc_index,
            SEN5X_SCALE_FACTOR_VOC_INDEX,
            p_measurement->nox_index,
            SEN5X_SCALE_FACTOR_NOX_INDEX);
    }
    return flag_success;
}

bool
sen5x_wrap_check(void)
{
    if (!sen5x_wrap_device_reset())
    {
        LOG_ERR("%s failed", "sen5x_wrap_device_reset");
        return false;
    }

    sen5x_wrap_serial_number_t serial_num = { 0 };
    if (!sen5x_wrap_get_serial_number(&serial_num))
    {
        LOG_ERR("%s failed", "sen5x_wrap_get_serial_number");
        return false;
    }
    LOG_INFO("SEN5X: Serial number: %s", serial_num.serial_number);

    sen5x_wrap_product_name_t product_name = { 0 };
    if (!sen5x_wrap_get_product_name(&product_name))
    {
        LOG_ERR("%s failed", "sen5x_wrap_get_product_name");
        return false;
    }
    LOG_INFO("SEN5X: Product name: %s", product_name.product_name);
    if (0 == strcmp("SEN55", (const char*)product_name.product_name))
    {
        g_sen5x_wrap_is_sen55 = true;
    }

    sen5x_wrap_version_t version = { 0 };
    if (!sen5x_wrap_get_version(&version))
    {
        LOG_ERR("%s failed", "sen5x_wrap_get_version");
        return false;
    }

    LOG_INFO(
        "SEN5X: Firmware: %u.%u, Hardware: %u.%u",
        version.firmware_major,
        version.firmware_minor,
        version.hardware_major,
        version.hardware_minor);

    return true;
}

re_float
sen5x_wrap_conv_raw_to_float_pm(const uint16_t raw_pm)
{
    if (SEN5X_INVALID_RAW_VALUE_PM == raw_pm)
    {
        return NAN;
    }
    const re_float result = (float)raw_pm / SEN5X_SCALE_FACTOR_PM;
    if ((result < RE_6_PM_MIN) || (result > RE_6_PM_MAX))
    {
        return NAN;
    }
    return result;
}

re_float
sen5x_wrap_conv_raw_to_float_humidity(const int16_t raw_humidity)
{
    if (SEN5X_INVALID_RAW_VALUE_HUMIDITY == raw_humidity)
    {
        return NAN;
    }
    const re_float result = (float)raw_humidity / SEN5X_SCALE_FACTOR_HUMIDITY;
    if ((result < RE_6_HUMIDITY_MIN) || (result > RE_6_HUMIDITY_MAX))
    {
        return NAN;
    }
    return result;
}

re_float
sen5x_wrap_conv_raw_to_float_temperature(const int16_t temperature)
{
    if (SEN5X_INVALID_RAW_VALUE_TEMPERATURE == temperature)
    {
        return NAN;
    }
    const re_float result = (float)temperature / SEN5X_SCALE_FACTOR_TEMPERATURE;
    if ((result < RE_6_TEMPERATURE_MIN) || (result > RE_6_TEMPERATURE_MAX))
    {
        return NAN;
    }
    return result;
}

re_float
sen5x_wrap_conv_raw_to_float_voc_index(const int16_t raw_voc_index)
{
    if (SEN5X_INVALID_RAW_VALUE_VOC == raw_voc_index)
    {
        return NAN;
    }
    const re_float result = (float)raw_voc_index / SEN5X_SCALE_FACTOR_VOC_INDEX;
    if ((result < RE_6_VOC_INDEX_MIN) || (result > RE_6_VOC_INDEX_MAX))
    {
        return NAN;
    }
    return result;
}

re_float
sen5x_wrap_conv_raw_to_float_nox_index(const int16_t raw_nox_index)
{
    if (SEN5X_INVALID_RAW_VALUE_NOX == raw_nox_index)
    {
        return NAN;
    }
    const re_float result = (float)raw_nox_index / SEN5X_SCALE_FACTOR_NOX_INDEX;
    if ((result < RE_6_NOX_INDEX_MIN) || (result > RE_6_NOX_INDEX_MAX))
    {
        return NAN;
    }
    return result;
}
