/**
 * @file sen5x_wrap.c
 * @author TheSomeMan
 * @date 2023-07-03
 * @copyright Ruuvi Innovations Ltd, license BSD-3-Clause.
 */

#include "sen5x_wrap.h"
#include <string.h>
#include "sen5x_i2c.h"
#include "sensirion_common.h"
#define LOG_LOCAL_LEVEL LOG_LEVEL_INFO
#include "log.h"

#define SEN5X_WRAP_NUM_RETRIES  (3U)

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
        const int16_t error = sen5x_get_product_name(p_product_name->product_name, sizeof(p_product_name->product_name));
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

    sen5x_wrap_serial_number_t serial_num = {0};
    if (!sen5x_wrap_get_serial_number(&serial_num))
    {
        LOG_ERR("%s failed", "sen5x_wrap_get_serial_number");
        return false;
    }
    LOG_INFO("SEN5X: Serial number: %s", serial_num.serial_number);

    sen5x_wrap_product_name_t product_name = {0};
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

    sen5x_wrap_version_t version = {0};
    if (!sen5x_wrap_get_version(&version))
    {
        LOG_ERR("%s failed", "sen5x_wrap_get_version");
        return false;
    }

    LOG_INFO("SEN5X: Firmware: %u.%u, Hardware: %u.%u", version.firmware_major,
             version.firmware_minor, version.hardware_major, version.hardware_minor);

    return true;
}
