/**
 * @file gw_cfg_storage.c
 * @author TheSomeMan
 * @date 2023-05-06
 * @copyright Ruuvi Innovations Ltd, license BSD-3-Clause.
 */

#include "gw_cfg_storage.h"
#include <string.h>
#include <nvs_flash.h>
#include "os_malloc.h"
#include "ruuvi_nvs.h"
#include "nvs.h"

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

#define GW_CFG_STORAGE_MAX_BLOB_SIZE (8192U)

bool
gw_cfg_storage_check(void)
{
    nvs_handle_t handle = 0;
    if (!ruuvi_nvs_open_gw_cfg_storage(NVS_READONLY, &handle))
    {
        LOG_ERR("%s failed", "ruuvi_nvs_open_gw_cfg_storage");
        return false;
    }
    nvs_close(handle);
    return true;
}

bool
gw_cfg_storage_check_file(const char* const p_file_name, const bool is_blob, size_t* const p_file_size)
{
    LOG_INFO("Check file '%s' (%s) in NVS", p_file_name, is_blob ? "blob" : "string");
    nvs_handle_t handle = 0;
    if (!ruuvi_nvs_open_gw_cfg_storage(NVS_READONLY, &handle))
    {
        LOG_ERR("%s failed", "ruuvi_nvs_open_gw_cfg_storage");
        return false;
    }

    size_t file_size = 0;

    const esp_err_t esp_err = is_blob ? nvs_get_blob(handle, p_file_name, NULL, &file_size)
                                      : nvs_get_str(handle, p_file_name, NULL, &file_size);
    if (ESP_OK != esp_err)
    {
        LOG_INFO("Can't find config key '%s' in flash", p_file_name);
        nvs_close(handle);
        return false;
    }

    nvs_close(handle);

    if (NULL != p_file_size)
    {
        *p_file_size = file_size;
    }

    LOG_INFO("File '%s' (%s) exists in NVS, size %zu bytes", p_file_name, is_blob ? "blob" : "string", file_size);

    return true;
}

static str_buf_t
gw_cfg_storage_read_string_from_nvs(nvs_handle_t const handle, const char* const p_nvs_key)
{
    size_t    file_size = 0;
    esp_err_t esp_err   = nvs_get_str(handle, p_nvs_key, NULL, &file_size);
    if (ESP_OK != esp_err)
    {
        LOG_ERR_ESP(esp_err, "Can't find string key '%s' in flash", p_nvs_key);
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
        LOG_ERR_ESP(esp_err, "Can't read string from NVS by key '%s'", p_nvs_key);
        os_free(p_file_content);
        return str_buf_init_null();
    }
    return str_buf_init(p_file_content, file_size);
}

static str_buf_t
gw_cfg_storage_read_blob_from_nvs(nvs_handle_t handle, const char* const p_nvs_key)
{
    size_t    file_size = 0;
    esp_err_t esp_err   = nvs_get_blob(handle, p_nvs_key, NULL, &file_size);
    if (ESP_OK != esp_err)
    {
        LOG_ERR_ESP(esp_err, "Can't find blob key '%s' in flash", p_nvs_key);
        return str_buf_init_null();
    }

    const size_t file_size_with_null_terminator = file_size + 1;
    char*        p_file_content                 = os_malloc(file_size_with_null_terminator);
    if (NULL == p_file_content)
    {
        LOG_ERR("Can't allocate %lu bytes for file", (printf_ulong_t)file_size);
        return str_buf_init_null();
    }
    p_file_content[file_size] = '\0'; // zero terminate string

    esp_err = nvs_get_blob(handle, p_nvs_key, p_file_content, &file_size);
    if (ESP_OK != esp_err)
    {
        LOG_ERR_ESP(esp_err, "Can't read blob from NVS by key '%s'", p_nvs_key);
        os_free(p_file_content);
        return str_buf_init_null();
    }
    return str_buf_init(p_file_content, file_size_with_null_terminator);
}

str_buf_t
gw_cfg_storage_read_file_as_string(const char* const p_file_name)
{
    LOG_INFO("Read file '%s' as string from NVS", p_file_name);
    nvs_handle_t handle = 0;
    if (!ruuvi_nvs_open_gw_cfg_storage(NVS_READONLY, &handle))
    {
        LOG_ERR("%s failed", "ruuvi_nvs_open_gw_cfg_storage");
        return str_buf_init_null();
    }

    str_buf_t str_buf = gw_cfg_storage_read_string_from_nvs(handle, p_file_name);

    nvs_close(handle);

    if (NULL == str_buf.buf)
    {
        LOG_ERR("Failed to read file '%s' (string) from NVS", p_file_name);
        return str_buf_init_null();
    }
    LOG_INFO("File '%s' (string) was successfully read from NVS", p_file_name);

    return str_buf;
}

str_buf_t
gw_cfg_storage_read_file_as_blob(const char* const p_file_name)
{
    LOG_INFO("Read file '%s' as blob from NVS", p_file_name);
    nvs_handle_t handle = 0;
    if (!ruuvi_nvs_open_gw_cfg_storage(NVS_READONLY, &handle))
    {
        LOG_ERR("%s failed", "ruuvi_nvs_open_gw_cfg_storage");
        return str_buf_init_null();
    }

    const str_buf_t str_buf = gw_cfg_storage_read_blob_from_nvs(handle, p_file_name);

    nvs_close(handle);

    if (NULL == str_buf.buf)
    {
        LOG_ERR("Failed to read file '%s' (blob) from NVS", p_file_name);
        return str_buf_init_null();
    }
    LOG_INFO("File '%s' (blob) was successfully read from NVS", p_file_name);
    LOG_DUMP_DBG(
        (const uint8_t*)str_buf.buf,
        strlen(str_buf.buf),
        "File '%s' (blob) was successfully read from NVS, size=%zu",
        p_file_name,
        str_buf.size - 1);

    return str_buf;
}

bool
gw_cfg_storage_write_file_as_string(const char* const p_file_name, const char* const p_content)
{
    const size_t len = strlen(p_content);
    LOG_INFO("Write file '%s' to NVS as string (length: %zu)", p_file_name, len);
    nvs_handle_t handle = 0;
    if (!ruuvi_nvs_open_gw_cfg_storage(NVS_READWRITE, &handle))
    {
        LOG_ERR("%s failed", "ruuvi_nvs_open_gw_cfg_storage");
        return false;
    }

    if ((len + 1) > NVS_CHUNK_MAX_SIZE)
    {
        LOG_ERR(
            "File '%s' is too big to write as string to NVS (length=%zu, max=%u)",
            p_file_name,
            len,
            NVS_CHUNK_MAX_SIZE - 1);
        nvs_close(handle);
        return false;
    }
    LOG_DUMP_DBG(
        (const uint8_t*)p_content,
        len,
        "Content to write to file '%s' as string (length: %zu)",
        p_file_name,
        len);

    const esp_err_t esp_err = nvs_set_str(handle, p_file_name, p_content);
    if (ESP_OK != esp_err)
    {
        LOG_ERR_ESP(esp_err, "Failed to write file '%s' to NVS as string", p_file_name);
    }
    else
    {
        LOG_INFO("File '%s' was successfully written to NVS as string", p_file_name);
    }

    nvs_close(handle);

    return (ESP_OK == esp_err) ? true : false;
}

bool
gw_cfg_storage_write_file_as_blob(const char* const p_file_name, const uint8_t* const p_content, const size_t len)
{
    LOG_INFO("Write file '%s' to NVS as blob (length: %zu)", p_file_name, len);
    nvs_handle_t handle = 0;
    if (!ruuvi_nvs_open_gw_cfg_storage(NVS_READWRITE, &handle))
    {
        LOG_ERR("%s failed", "ruuvi_nvs_open_gw_cfg_storage");
        return false;
    }

    if (len > GW_CFG_STORAGE_MAX_BLOB_SIZE)
    {
        LOG_ERR(
            "File '%s' is too big to write as blob to NVS (length=%zu, max=%u)",
            p_file_name,
            len,
            GW_CFG_STORAGE_MAX_BLOB_SIZE);
        nvs_close(handle);
        return false;
    }
    LOG_DUMP_DBG(p_content, len, "Content to write to file '%s' as blob (length: %zu)", p_file_name, len);
    const esp_err_t esp_err = nvs_set_blob(handle, p_file_name, p_content, len);
    if (ESP_OK != esp_err)
    {
        LOG_ERR_ESP(esp_err, "Failed to write file '%s' to NVS as blob", p_file_name);
    }
    else
    {
        LOG_INFO("File '%s' was successfully written to NVS as blob", p_file_name);
    }

    nvs_close(handle);

    return (ESP_OK == esp_err) ? true : false;
}

bool
gw_cfg_storage_delete_file(const char* const p_file_name)
{
    LOG_INFO("Delete file '%s' from NVS", p_file_name);
    nvs_handle_t handle = 0;
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
    str_buf_t str_buf_gw_cfg_def = gw_cfg_storage_read_file_as_string(GW_CFG_STORAGE_GW_CFG_DEFAULT);

    ruuvi_nvs_deinit_gw_cfg_storage();
    ruuvi_nvs_erase_gw_cfg_storage();
    ruuvi_nvs_init_gw_cfg_storage();
    if (NULL != str_buf_gw_cfg_def.buf)
    {
        gw_cfg_storage_write_file_as_string(GW_CFG_STORAGE_GW_CFG_DEFAULT, str_buf_gw_cfg_def.buf);
        str_buf_free_buf(&str_buf_gw_cfg_def);
    }
}
