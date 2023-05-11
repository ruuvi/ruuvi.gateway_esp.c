/**
 * @file gw_cfg_storage.c
 * @author TheSomeMan
 * @date 2023-05-06
 * @copyright Ruuvi Innovations Ltd, license BSD-3-Clause.
 */

#include "gw_cfg_storage.h"
#include <string.h>
#include <nvs_flash.h>
#include "gw_cfg.h"
#include "os_malloc.h"
#include "ruuvi_nvs.h"

#if defined(RUUVI_TESTS_GW_CFG_DEFAULT_JSON)
#define LOG_LOCAL_LEVEL LOG_LEVEL_DEBUG
#else
#define LOG_LOCAL_LEVEL LOG_LEVEL_INFO
#endif
#include "log.h"

#if (LOG_LOCAL_LEVEL >= LOG_LEVEL_DEBUG) && !RUUVI_TESTS
#warning Debug log level prints out the passwords as a "plaintext".
#endif

static const char TAG[] = "gw_cfg";

bool
gw_cfg_storage_check_file(const char* const p_file_name)
{
    LOG_INFO("Check file '%s' in NVS", p_file_name);
    nvs_handle handle = 0;
    if (!ruuvi_nvs_open_gw_cfg_storage(NVS_READONLY, &handle))
    {
        LOG_ERR("%s failed", "ruuvi_nvs_open_gw_cfg_storage");
        return false;
    }

    size_t    file_size = 0;
    esp_err_t esp_err   = nvs_get_str(handle, p_file_name, NULL, &file_size);
    if (ESP_OK != esp_err)
    {
        LOG_ERR_ESP(esp_err, "Can't find config key '%s' in flash", p_file_name);
        nvs_close(handle);
        return false;
    }

    nvs_close(handle);

    LOG_INFO("File '%s' exists in NVS", p_file_name);

    return true;
}

static str_buf_t
gw_cfg_storage_read_from_nvs(nvs_handle handle, const char* const p_nvs_key)
{
    size_t    file_size = 0;
    esp_err_t esp_err   = nvs_get_str(handle, p_nvs_key, NULL, &file_size);
    if (ESP_OK != esp_err)
    {
        LOG_ERR_ESP(esp_err, "Can't find config key '%s' in flash", p_nvs_key);
        return str_buf_init_null();
    }

    char* p_file_content = os_malloc(file_size);
    if (NULL == p_file_content)
    {
        LOG_ERR("Can't allocate %lu bytes for file", (printf_ulong_t)file_size);
        return str_buf_init_null();
    }

    esp_err = nvs_get_str(handle, p_nvs_key, p_file_content, &file_size);
    if (ESP_OK != esp_err)
    {
        LOG_ERR_ESP(esp_err, "Can't read file from flash by key '%s'", p_nvs_key);
        os_free(p_file_content);
        return str_buf_init_null();
    }
    return str_buf_init(p_file_content, file_size);
}

str_buf_t
gw_cfg_storage_read_file(const char* const p_file_name)
{
    LOG_INFO("Read file '%s' from NVS", p_file_name);
    nvs_handle handle = 0;
    if (!ruuvi_nvs_open_gw_cfg_storage(NVS_READONLY, &handle))
    {
        LOG_ERR("%s failed", "ruuvi_nvs_open_gw_cfg_storage");
        return str_buf_init_null();
    }

    str_buf_t str_buf = gw_cfg_storage_read_from_nvs(handle, p_file_name);

    nvs_close(handle);

    if (NULL == str_buf.buf)
    {
        LOG_ERR("Failed to read file '%s' from NVS", p_file_name);
        return str_buf_init_null();
    }
    LOG_INFO("File '%s' was successfully read from NVS", p_file_name);

    return str_buf;
}

bool
gw_cfg_storage_write_file(const char* const p_file_name, const char* const p_content)
{
    LOG_INFO("Write file '%s' to NVS", p_file_name);
    nvs_handle handle = 0;
    if (!ruuvi_nvs_open_gw_cfg_storage(NVS_READWRITE, &handle))
    {
        LOG_ERR("%s failed", "ruuvi_nvs_open_gw_cfg_storage");
        return false;
    }

    esp_err_t esp_err = nvs_set_str(handle, p_file_name, p_content);
    if (ESP_OK != esp_err)
    {
        LOG_ERR_ESP(esp_err, "Failed to write file '%s' to NVS", p_file_name);
    }
    else
    {
        LOG_INFO("File '%s' was successfully written NVS", p_file_name);
    }

    nvs_close(handle);

    return (ESP_OK == esp_err) ? true : false;
}

bool
gw_cfg_storage_delete_file(const char* const p_file_name)
{
    LOG_INFO("Delete file '%s' from NVS", p_file_name);
    nvs_handle handle = 0;
    if (!ruuvi_nvs_open_gw_cfg_storage(NVS_READWRITE, &handle))
    {
        LOG_ERR("%s failed", "ruuvi_nvs_open_gw_cfg_storage");
        return false;
    }

    esp_err_t esp_err = nvs_erase_key(handle, p_file_name);
    if (ESP_OK != esp_err)
    {
        LOG_ERR_ESP(esp_err, "Failed to delete file '%s' in NVS", p_file_name);
    }
    else
    {
        LOG_INFO("File '%s' was successfully deleted from NVS", p_file_name);
    }

    nvs_close(handle);

    return (ESP_OK == esp_err) ? true : false;
}

void
gw_cfg_storage_deinit_erase_init(void)
{
    str_buf_t str_buf_gw_cfg_def = gw_cfg_storage_read_file(GW_CFG_STORAGE_GW_CFG_DEFAULT);

    ruuvi_nvs_deinit_gw_cfg_storage();
    ruuvi_nvs_erase_gw_cfg_storage();
    ruuvi_nvs_init_gw_cfg_storage();
    if (NULL != str_buf_gw_cfg_def.buf)
    {
        gw_cfg_storage_write_file(GW_CFG_STORAGE_GW_CFG_DEFAULT, str_buf_gw_cfg_def.buf);
        str_buf_free_buf(&str_buf_gw_cfg_def);
    }
}
