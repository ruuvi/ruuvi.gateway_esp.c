/**
 * @file scd4x_wrap.c
 * @author TheSomeMan
 * @date 2023-07-03
 * @copyright Ruuvi Innovations Ltd, license BSD-3-Clause.
 */

#include "scd4x_wrap.h"
#include "scd4x_i2c.h"
#include "sensirion_common.h"
#define LOG_LOCAL_LEVEL LOG_LEVEL_INFO
#include "log.h"

#define SCD4X_WRAP_NUM_RETRIES (3)

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
            &p_measurement->temperature,
            &p_measurement->humidity);
        if (NO_ERROR == error)
        {
            flag_success = true;
            break;
        }
        LOG_ERR("%s[retry=%d]: err=%d", "scd4x_read_measurement", i, error);
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

    scd4x_wrap_serial_num_t serial_num = {0};
    if (!scd4x_wrap_get_serial_number(&serial_num))
    {
        LOG_ERR("%s failed", "scd4x_wrap_get_serial_number");
        return false;
    }

    LOG_INFO("SCD4X: serial: 0x%04x%04x%04x", serial_num.serial_0, serial_num.serial_1, serial_num.serial_2);

    return true;
}
