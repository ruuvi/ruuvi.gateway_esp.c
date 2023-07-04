/**
 * @file scd4x_wrap.c
 * @author TheSomeMan
 * @date 2023-07-03
 * @copyright Ruuvi Innovations Ltd, license BSD-3-Clause.
 */

#include "scd4x_wrap.h"
#include <math.h>
#include "scd4x_i2c.h"
#include "sensirion_common.h"
#define LOG_LOCAL_LEVEL LOG_LEVEL_INFO
#include "log.h"

#define SCD4X_WRAP_NUM_RETRIES (3)

#define SCD4X_SCALE_FACTOR_CO2         (1)
#define SCD4X_SCALE_FACTOR_TEMPERATURE (1000)
#define SCD4X_SCALE_FACTOR_HUMIDITY    (1000)

static const char* TAG = "SCD4X";

static bool
scd4x_wrap_get_serial_number(scd4x_wrap_serial_num_t* const p_serial_num)
{
    bool flag_success = false;
    for (uint32_t i = 0; i < SCD4X_WRAP_NUM_RETRIES; ++i)
    {
        const int16_t error = scd4x_get_serial_number(
            &p_serial_num->serial_0,
            &p_serial_num->serial_1,
            &p_serial_num->serial_2);
        if (NO_ERROR == error)
        {
            flag_success = true;
            break;
        }
        LOG_ERR("%s[retry=%d]: err=%d", "scd4x_get_serial_number", i, error);
    }
    return flag_success;
}

static bool
scd4x_wrap_wake_up(void)
{
    bool flag_success = false;
    for (uint32_t i = 0; i < SCD4X_WRAP_NUM_RETRIES; ++i)
    {
        const int16_t error = scd4x_wake_up();
        if (NO_ERROR == error)
        {
            flag_success = true;
            break;
        }
        LOG_ERR("%s[retry=%d]: err=%d", "scd4x_wake_up", i, error);
    }
    return flag_success;
}

static bool
scd4x_wrap_stop_periodic_measurement(void)
{
    bool flag_success = false;
    for (uint32_t i = 0; i < SCD4X_WRAP_NUM_RETRIES; ++i)
    {
        const int16_t error = scd4x_stop_periodic_measurement();
        if (NO_ERROR == error)
        {
            flag_success = true;
            break;
        }
        LOG_ERR("%s[retry=%d]: err=%d", "scd4x_stop_periodic_measurement", i, error);
    }
    return flag_success;
}

static bool
scd4x_wrap_reinit(void)
{
    bool flag_success = false;
    for (uint32_t i = 0; i < SCD4X_WRAP_NUM_RETRIES; ++i)
    {
        const int16_t error = scd4x_reinit();
        if (NO_ERROR == error)
        {
            flag_success = true;
            break;
        }
        LOG_ERR("%s[retry=%d]: err=%d", "scd4x_reinit", i, error);
    }
    return flag_success;
}

bool
scd4x_wrap_start_periodic_measurement(void)
{
    bool flag_success = false;
    for (uint32_t i = 0; i < SCD4X_WRAP_NUM_RETRIES; ++i)
    {
        const int16_t error = scd4x_start_periodic_measurement();
        if (NO_ERROR == error)
        {
            flag_success = true;
            break;
        }
        LOG_ERR("%s[retry=%d]: err=%d", "scd4x_start_periodic_measurement", i, error);
    }
    return flag_success;
}

static bool
scd4x_wrap_get_data_ready_flag(bool* const p_data_ready_flag)
{
    bool flag_success = false;
    for (uint32_t i = 0; i < SCD4X_WRAP_NUM_RETRIES; ++i)
    {
        const int16_t error = scd4x_get_data_ready_flag(p_data_ready_flag);
        if (NO_ERROR == error)
        {
            flag_success = true;
            break;
        }
        LOG_ERR("%s[retry=%d]: err=%d", "scd4x_get_data_ready_flag", i, error);
    }
    return flag_success;
}

bool
scd4x_wrap_read_measurement(scd4x_wrap_measurement_t* const p_measurement)
{
    bool data_ready_flag = false;
    if (!scd4x_wrap_get_data_ready_flag(&data_ready_flag))
    {
        return false;
    }
    if (!data_ready_flag)
    {
        return false;
    }
    bool flag_success = false;
    for (uint32_t i = 0; i < SCD4X_WRAP_NUM_RETRIES; ++i)
    {
        const int16_t error = scd4x_read_measurement(
            &p_measurement->co2,
            &p_measurement->temperature_m_deg_c,
            &p_measurement->humidity_m_percent_rh);
        if (NO_ERROR == error)
        {
            flag_success = true;
            break;
        }
        LOG_ERR("%s[retry=%d]: err=%d", "scd4x_read_measurement", i, error);
    }
    if (flag_success)
    {
        LOG_DBG(
            "CO2: %u, Temperature: %d m°C, Humidity: %d mRH",
            p_measurement->co2,
            p_measurement->temperature_m_deg_c,
            p_measurement->humidity_m_percent_rh);
    }
    return flag_success;
}

bool
scd4x_wrap_check(void)
{
    if (!scd4x_wrap_wake_up())
    {
        LOG_ERR("%s failed", "scd4x_wrap_wake_up");
        return false;
    }
    if (!scd4x_wrap_stop_periodic_measurement())
    {
        LOG_ERR("%s failed", "scd4x_wrap_stop_periodic_measurement");
        return false;
    }
    if (!scd4x_wrap_reinit())
    {
        LOG_ERR("%s failed", "scd4x_wrap_reinit");
        return false;
    }

    scd4x_wrap_serial_num_t serial_num = { 0 };
    if (!scd4x_wrap_get_serial_number(&serial_num))
    {
        LOG_ERR("%s failed", "scd4x_wrap_get_serial_number");
        return false;
    }

    LOG_INFO("SCD4X: serial: 0x%04x%04x%04x", serial_num.serial_0, serial_num.serial_1, serial_num.serial_2);

    return true;
}

re_float
scd4x_wrap_conv_raw_to_float_co2(const scd4x_wrap_raw_co2_t raw_co2)
{
    const re_float result = (float)raw_co2 / SCD4X_SCALE_FACTOR_CO2;
    if ((result < RE_6_CO2_MIN) || (result > RE_6_CO2_MAX))
    {
        return NAN;
    }
    return result;
}

re_float
scd4x_wrap_conv_raw_to_float_temperature(const scd4x_wrap_raw_temperature_m_deg_c_t temperature_m_deg_c)
{
    const re_float result = (float)temperature_m_deg_c / SCD4X_SCALE_FACTOR_TEMPERATURE;
    if ((result < RE_6_TEMPERATURE_MIN) || (result > RE_6_TEMPERATURE_MAX))
    {
        return NAN;
    }
    return result;
}

re_float
scd4x_wrap_conv_raw_to_float_humidity(const scd4x_wrap_raw_humidity_m_percent_rh_t humidity_m_percent_rh)
{
    const re_float result = (float)humidity_m_percent_rh / SCD4X_SCALE_FACTOR_HUMIDITY;
    if ((result < RE_6_HUMIDITY_MIN) || (result > RE_6_HUMIDITY_MAX))
    {
        return NAN;
    }
    return result;
}
