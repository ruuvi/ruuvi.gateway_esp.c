/**
 * @file gw_cfg_default_json.c
 * @author TheSomeMan
 * @date 2022-02-28
 * @copyright Ruuvi Innovations Ltd, license BSD-3-Clause.
 */

#include "gw_cfg_default_json.h"
#include <string.h>
#include "gw_cfg_default.h"
#include "gw_cfg.h"
#include "cJSON.h"
#include "os_malloc.h"
#include "gw_cfg_json_parse.h"
#include "gw_cfg_log.h"
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

static const char *
gw_cfg_default_json_read_from_nvs(nvs_handle handle, const char *const p_nvs_key)
{
    size_t    json_size = 0;
    esp_err_t esp_err   = nvs_get_str(handle, p_nvs_key, NULL, &json_size);
    if (ESP_OK != esp_err)
    {
        LOG_ERR_ESP(esp_err, "Can't find config key '%s' in flash", p_nvs_key);
        return NULL;
    }

    char *p_cfg_json = os_malloc(json_size);
    if (NULL == p_cfg_json)
    {
        LOG_ERR("Can't allocate %lu bytes for configuration", (printf_ulong_t)json_size);
        return NULL;
    }

    esp_err = nvs_get_str(handle, p_nvs_key, p_cfg_json, &json_size);
    if (ESP_OK != esp_err)
    {
        LOG_ERR_ESP(esp_err, "Can't read config-json from flash by key '%s'", p_nvs_key);
        os_free(p_cfg_json);
        return NULL;
    }
    return p_cfg_json;
}

bool
gw_cfg_default_json_read(gw_cfg_t *const p_gw_cfg_default)
{
    static const char *const p_nvs_key_gw_cfg_default = "gw_cfg_default";

    if (!ruuvi_nvs_init_gw_cfg_default())
    {
        LOG_ERR("%s failed", "ruuvi_nvs_init_gw_cfg_default");
        return false;
    }

    LOG_INFO("Read default gw_cfg from NVS by key '%s'", p_nvs_key_gw_cfg_default);
    nvs_handle handle = 0;
    if (!ruuvi_nvs_open_gw_cfg_default(NVS_READONLY, &handle))
    {
        LOG_ERR("%s failed", "ruuvi_nvs_open_gw_cfg_default");
        (void)ruuvi_nvs_deinit_gw_cfg_default();
        return false;
    }

    const char *p_cfg_json = gw_cfg_default_json_read_from_nvs(handle, p_nvs_key_gw_cfg_default);

    nvs_close(handle);
    (void)ruuvi_nvs_deinit_gw_cfg_default();

    if (NULL == p_cfg_json)
    {
        LOG_ERR("Failed to read default gw_cfg from NVS");
        return false;
    }
    LOG_INFO("Default gw_cfg was successfully read from NVS");

    const bool res = gw_cfg_json_parse(p_nvs_key_gw_cfg_default, NULL, p_cfg_json, p_gw_cfg_default, NULL);
    os_free(p_cfg_json);

    if (!res)
    {
        LOG_ERR("Failed to parse default gw_cfg from NVS");
        return false;
    }

    return true;
}
