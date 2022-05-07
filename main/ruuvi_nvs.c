/**
 * @file ruuvi_nvs.c
 * @author TheSomeMan
 * @date 2022-05-07
 * @copyright Ruuvi Innovations Ltd, license BSD-3-Clause.
 */

#include "ruuvi_nvs.h"
#include "os_malloc.h"
#include "esp_type_wrapper.h"

#define LOG_LOCAL_LEVEL LOG_LEVEL_INFO
#include "log.h"

#if (LOG_LOCAL_LEVEL >= LOG_LEVEL_DEBUG) && !RUUVI_TESTS
#warning Debug log level prints out the passwords as a "plaintext".
#endif

#define RUUVI_GATEWAY_NVS_NAMESPACE "ruuvi_gateway"

static const char TAG[] = "ruuvi_nvs";

static bool
ruuvi_nvs_open_partition(const char *const p_partition_name, nvs_open_mode_t open_mode, nvs_handle_t *p_handle)
{
    const char *const p_nvs_name = RUUVI_GATEWAY_NVS_NAMESPACE;

    esp_err_t err = nvs_open_from_partition(p_partition_name, p_nvs_name, open_mode, p_handle);
    if (ESP_OK != err)
    {
        if (ESP_ERR_NVS_NOT_INITIALIZED == err)
        {
            LOG_WARN(
                "NVS partition %s, namespace '%s': StorageState is INVALID, need to erase NVS",
                p_partition_name,
                p_nvs_name);
            return false;
        }
        if (ESP_ERR_NVS_NOT_FOUND == err)
        {
            LOG_WARN(
                "NVS partition %s, namespace '%s' doesn't exist and mode is NVS_READONLY, try to create it",
                p_partition_name,
                p_nvs_name);

            nvs_handle handle = 0;

            err = nvs_open_from_partition(p_partition_name, p_nvs_name, NVS_READWRITE, &handle);
            if (ESP_OK != err)
            {
                LOG_ERR_ESP(err, "Can't open NVS for writing");
                return false;
            }
            err = nvs_commit(handle);
            if (ESP_OK != err)
            {
                LOG_ERR_ESP(err, "%s failed", "nvs_commit");
                nvs_close(handle);
                return false;
            }
            nvs_close(handle);

            LOG_INFO("NVS partition %s, namespace '%s' created successfully", p_partition_name, p_nvs_name);
            err = nvs_open_from_partition(p_partition_name, p_nvs_name, open_mode, p_handle);
            if (ESP_OK != err)
            {
                LOG_ERR_ESP(err, "Can't open NVS partition %s, namespace: '%s'", p_partition_name, p_nvs_name);
                return false;
            }
        }
        else
        {
            LOG_ERR_ESP(err, "Can't open NVS partition %s, namespace: '%s'", p_partition_name, p_nvs_name);
            return false;
        }
    }
    return true;
}

bool
ruuvi_nvs_open(nvs_open_mode_t open_mode, nvs_handle_t *p_handle)
{
    return ruuvi_nvs_open_partition(NVS_DEFAULT_PART_NAME, open_mode, p_handle);
}
