/**
 * @file settings.c
 * @author Jukka Saari
 * @date 2019-11-27
 * @copyright Ruuvi Innovations Ltd, license BSD-3-Clause.
 */

#include "settings.h"
#include <stdio.h>
#include <string.h>
#include "cJSON.h"
#include "nvs.h"
#include "nvs_flash.h"
#include "ruuvi_gateway.h"
#include "cjson_wrap.h"
#include "gw_cfg.h"
#include "gw_cfg_default.h"
#include "gw_cfg_blob.h"
#include "gw_cfg_json.h"
#include "log.h"
#include "os_malloc.h"

#define RUUVI_GATEWAY_NVS_NAMESPACE    "ruuvi_gateway"
#define RUUVI_GATEWAY_NVS_CFG_BLOB_KEY "ruuvi_config" /* deprecated */
#define RUUVI_GATEWAY_NVS_CFG_JSON_KEY "ruuvi_cfg_json"
#define RUUVI_GATEWAY_NVS_MAC_ADDR_KEY "ruuvi_mac_addr"

#define RUUVI_GATEWAY_NVS_FLAG_REBOOTING_AFTER_AUTO_UPDATE_KEY   "ruuvi_auto_udp"
#define RUUVI_GATEWAY_NVS_FLAG_REBOOTING_AFTER_AUTO_UPDATE_VALUE (0xAACC5533U)

#define RUUVI_GATEWAY_NVS_FLAG_FORCE_START_WIFI_HOTSPOT_KEY   "ruuvi_wifi_ap"
#define RUUVI_GATEWAY_NVS_FLAG_FORCE_START_WIFI_HOTSPOT_VALUE (0xAACC5533U)

static const char TAG[] = "settings";

static bool g_flag_cfg_blob_used;

static bool
settings_nvs_open(nvs_open_mode_t open_mode, nvs_handle_t *p_handle)
{
    const char *nvs_name = RUUVI_GATEWAY_NVS_NAMESPACE;
    esp_err_t   err      = nvs_open(nvs_name, open_mode, p_handle);
    if (ESP_OK != err)
    {
        if (ESP_ERR_NVS_NOT_INITIALIZED == err)
        {
            LOG_WARN("NVS namespace '%s': StorageState is INVALID, need to erase NVS", nvs_name);
            return false;
        }
        else if (ESP_ERR_NVS_NOT_FOUND == err)
        {
            LOG_WARN("NVS namespace '%s' doesn't exist and mode is NVS_READONLY, try to create it", nvs_name);
            nvs_handle handle = 0;
            err               = nvs_open(nvs_name, NVS_READWRITE, &handle);
            if (ESP_OK != err)
            {
                LOG_ERR_ESP(err, "Can't open NVS for writing");
                return false;
            }
            err = nvs_set_str(handle, RUUVI_GATEWAY_NVS_CFG_JSON_KEY, "");
            if (ESP_OK != err)
            {
                LOG_ERR_ESP(err, "Can't save config to NVS");
                nvs_close(handle);
                return false;
            }
            nvs_close(handle);

            LOG_INFO("NVS namespace '%s' created successfully", nvs_name);
            err = nvs_open(nvs_name, open_mode, p_handle);
            if (ESP_OK != err)
            {
                LOG_ERR_ESP(err, "Can't open NVS namespace: '%s'", nvs_name);
                return false;
            }
        }
        else
        {
            LOG_ERR_ESP(err, "Can't open NVS namespace: '%s'", nvs_name);
            return false;
        }
    }
    return true;
}

bool
settings_check_in_flash(void)
{
    nvs_handle handle = 0;
    if (!settings_nvs_open(NVS_READONLY, &handle))
    {
        return false;
    }
    nvs_close(handle);
    return true;
}

bool
settings_save_to_flash(const char *const p_json_str)
{
    LOG_DBG(".");

    nvs_handle handle = 0;
    if (!settings_nvs_open(NVS_READWRITE, &handle))
    {
        LOG_ERR("Failed to open NVS for writing");
        return false;
    }

    if (g_flag_cfg_blob_used)
    {
        g_flag_cfg_blob_used = false;
        LOG_INFO("Erase deprecated cfg BLOB");
        const esp_err_t err = nvs_erase_key(handle, RUUVI_GATEWAY_NVS_CFG_BLOB_KEY);
        if (ESP_OK != err)
        {
            LOG_ERR_ESP(err, "Failed to erase deprecated cfg BLOB");
        }
    }

    const esp_err_t err = nvs_set_str(handle, RUUVI_GATEWAY_NVS_CFG_JSON_KEY, (NULL != p_json_str) ? p_json_str : "");
    nvs_close(handle);
    if (ESP_OK != err)
    {
        LOG_ERR_ESP(err, "Failed to save config to flash");
        return false;
    }
    return true;
}

bool
settings_clear_in_flash(void)
{
    LOG_DBG(".");
    return settings_save_to_flash("");
}

static bool
settings_get_gw_cfg_from_nvs(nvs_handle handle, ruuvi_gateway_config_t *const p_gw_cfg)
{
    gw_cfg_default_get(p_gw_cfg);

    size_t    cfg_json_size = 0;
    esp_err_t esp_err       = nvs_get_str(handle, RUUVI_GATEWAY_NVS_CFG_JSON_KEY, NULL, &cfg_json_size);
    if (ESP_OK != esp_err)
    {
        LOG_ERR_ESP(esp_err, "Can't find config key '%s' in flash", RUUVI_GATEWAY_NVS_CFG_JSON_KEY);
        return false;
    }

    char *p_cfg_json = os_malloc(cfg_json_size);
    if (NULL == p_cfg_json)
    {
        LOG_ERR("Can't allocate %lu bytes for configuration", (printf_ulong_t)cfg_json_size);
        return false;
    }

    esp_err = nvs_get_str(handle, RUUVI_GATEWAY_NVS_CFG_JSON_KEY, p_cfg_json, &cfg_json_size);
    if (ESP_OK != esp_err)
    {
        LOG_ERR_ESP(esp_err, "Can't read config-json from flash by key '%s'", RUUVI_GATEWAY_NVS_CFG_JSON_KEY);
        os_free(p_cfg_json);
        return false;
    }

    if (!gw_cfg_json_parse(p_cfg_json, p_gw_cfg))
    {
        LOG_ERR("Failed to parse config-json or no memory");
        os_free(p_cfg_json);
        return false;
    }
    os_free(p_cfg_json);

    return true;
}

static bool
settings_get_gw_cfg_blob_from_nvs(nvs_handle handle, ruuvi_gateway_config_blob_t *const p_gw_cfg_blob)
{
    size_t    sz      = 0;
    esp_err_t esp_err = nvs_get_blob(handle, RUUVI_GATEWAY_NVS_CFG_BLOB_KEY, NULL, &sz);
    if (ESP_OK != esp_err)
    {
        LOG_ERR_ESP(esp_err, "Can't read config from flash");
        return false;
    }

    g_flag_cfg_blob_used = true;

    if (sizeof(*p_gw_cfg_blob) != sz)
    {
        LOG_WARN("Size of config in flash differs");
        return false;
    }

    esp_err = nvs_get_blob(handle, RUUVI_GATEWAY_NVS_CFG_BLOB_KEY, p_gw_cfg_blob, &sz);
    if (ESP_OK != esp_err)
    {
        LOG_ERR_ESP(esp_err, "Can't read config from flash");
        return false;
    }

    if (RUUVI_GATEWAY_CONFIG_BLOB_HEADER != p_gw_cfg_blob->header)
    {
        LOG_WARN("Incorrect config header (0x%02X)", p_gw_cfg_blob->header);
        return false;
    }
    if (RUUVI_GATEWAY_CONFIG_BLOB_FMT_VERSION != p_gw_cfg_blob->fmt_version)
    {
        LOG_WARN(
            "Incorrect config fmt version (exp 0x%02x, act 0x%02x)",
            RUUVI_GATEWAY_CONFIG_BLOB_FMT_VERSION,
            p_gw_cfg_blob->fmt_version);
        return false;
    }
    return true;
}

static bool
settings_read_from_blob(nvs_handle handle, ruuvi_gateway_config_t *const p_gw_cfg)
{
    LOG_WARN("Try to read config from BLOB");
    bool                         flag_use_default_config = false;
    ruuvi_gateway_config_blob_t *p_gw_cfg_blob           = os_calloc(1, sizeof(*p_gw_cfg_blob));
    if (NULL == p_gw_cfg_blob)
    {
        flag_use_default_config = true;
    }
    else
    {
        if (!settings_get_gw_cfg_blob_from_nvs(handle, p_gw_cfg_blob))
        {
            flag_use_default_config = true;
        }
        else
        {
            gw_cfg_blob_convert(p_gw_cfg, p_gw_cfg_blob);
        }
        os_free(p_gw_cfg_blob);
    }
    return flag_use_default_config;
}

bool
settings_get_from_flash(void)
{
    ruuvi_gateway_config_t *p_gw_cfg                = gw_cfg_lock_rw();
    bool                    flag_use_default_config = false;
    nvs_handle              handle                  = 0;
    if (!settings_nvs_open(NVS_READWRITE, &handle))
    {
        flag_use_default_config = true;
    }
    else
    {
        if (!settings_get_gw_cfg_from_nvs(handle, p_gw_cfg))
        {
            flag_use_default_config = settings_read_from_blob(handle, p_gw_cfg);
        }
        nvs_close(handle);
    }
    if (g_flag_cfg_blob_used)
    {
        LOG_INFO("Convert Cfg BLOB to json");
        cjson_wrap_str_t cjson_str = { 0 };
        if (!gw_cfg_json_generate(p_gw_cfg, &cjson_str))
        {
            LOG_ERR("%s failed", "gw_cfg_json_generate");
        }
        else
        {
            settings_save_to_flash(cjson_str.p_str);
        }
        cjson_wrap_free_json_str(&cjson_str);
    }

    if (flag_use_default_config)
    {
        LOG_WARN("Using default config:");
        gw_cfg_default_get(p_gw_cfg);
    }
    else
    {
        LOG_INFO("Configuration from flash:");
    }

    gw_cfg_print_to_log(p_gw_cfg);
    gw_cfg_unlock_rw(&p_gw_cfg);
    return flag_use_default_config;
}

mac_address_bin_t
settings_read_mac_addr(void)
{
    mac_address_bin_t mac_addr = { 0 };
    nvs_handle        handle   = 0;
    if (!settings_nvs_open(NVS_READONLY, &handle))
    {
        LOG_WARN("Use empty mac_addr");
    }
    else
    {
        size_t    sz      = sizeof(mac_addr);
        esp_err_t esp_err = nvs_get_blob(handle, RUUVI_GATEWAY_NVS_MAC_ADDR_KEY, &mac_addr, &sz);
        if (ESP_OK != esp_err)
        {
            LOG_WARN_ESP(esp_err, "Can't read mac_addr from flash");
        }
        nvs_close(handle);
    }
    return mac_addr;
}

void
settings_write_mac_addr(const mac_address_bin_t *const p_mac_addr)
{
    nvs_handle handle = 0;
    if (!settings_nvs_open(NVS_READWRITE, &handle))
    {
        LOG_ERR("%s failed", "settings_nvs_open");
    }
    else
    {
        const esp_err_t esp_err = nvs_set_blob(handle, RUUVI_GATEWAY_NVS_MAC_ADDR_KEY, p_mac_addr, sizeof(*p_mac_addr));
        if (ESP_OK != esp_err)
        {
            LOG_ERR_ESP(esp_err, "%s failed", "nvs_set_blob");
        }
        nvs_close(handle);
    }
}

void
settings_update_mac_addr(const mac_address_bin_t *const p_mac_addr)
{
    const mac_address_bin_t mac_addr = settings_read_mac_addr();
    if (0 != memcmp(&mac_addr, p_mac_addr, sizeof(*p_mac_addr)))
    {
        const mac_address_str_t new_mac_addr_str = mac_address_to_str(p_mac_addr);
        LOG_INFO("Save new MAC-address: %s", new_mac_addr_str.str_buf);
        settings_write_mac_addr(p_mac_addr);
    }
}

bool
settings_read_flag_rebooting_after_auto_update(void)
{
    uint32_t   flag_rebooting_after_auto_update = 0;
    nvs_handle handle                           = 0;
    if (!settings_nvs_open(NVS_READONLY, &handle))
    {
        LOG_WARN("settings_nvs_open failed, flag_rebooting_after_auto_update = false");
        return false;
    }
    size_t    sz      = sizeof(flag_rebooting_after_auto_update);
    esp_err_t esp_err = nvs_get_blob(
        handle,
        RUUVI_GATEWAY_NVS_FLAG_REBOOTING_AFTER_AUTO_UPDATE_KEY,
        &flag_rebooting_after_auto_update,
        &sz);
    if (ESP_OK != esp_err)
    {
        LOG_WARN_ESP(esp_err, "Can't read '%s' from flash", RUUVI_GATEWAY_NVS_FLAG_REBOOTING_AFTER_AUTO_UPDATE_KEY);
        nvs_close(handle);
        settings_write_flag_rebooting_after_auto_update(false);
    }
    else
    {
        nvs_close(handle);
    }
    if (RUUVI_GATEWAY_NVS_FLAG_REBOOTING_AFTER_AUTO_UPDATE_VALUE == flag_rebooting_after_auto_update)
    {
        return true;
    }
    return false;
}

void
settings_write_flag_rebooting_after_auto_update(const bool flag_rebooting_after_auto_update)
{
    LOG_INFO("SETTINGS: Write flag_rebooting_after_auto_update: %d", flag_rebooting_after_auto_update);
    nvs_handle handle = 0;
    if (!settings_nvs_open(NVS_READWRITE, &handle))
    {
        LOG_ERR("%s failed", "settings_nvs_open");
        return;
    }
    const uint32_t flag_rebooting_after_auto_update_val = flag_rebooting_after_auto_update
                                                              ? RUUVI_GATEWAY_NVS_FLAG_REBOOTING_AFTER_AUTO_UPDATE_VALUE
                                                              : 0;
    const esp_err_t esp_err = nvs_set_blob(
        handle,
        RUUVI_GATEWAY_NVS_FLAG_REBOOTING_AFTER_AUTO_UPDATE_KEY,
        &flag_rebooting_after_auto_update_val,
        sizeof(flag_rebooting_after_auto_update_val));
    if (ESP_OK != esp_err)
    {
        LOG_ERR_ESP(esp_err, "%s failed", "nvs_set_blob");
    }
    nvs_close(handle);
}

bool
settings_read_flag_force_start_wifi_hotspot(void)
{
    uint32_t   flag_force_start_wifi_hotspot = 0;
    nvs_handle handle                        = 0;
    if (!settings_nvs_open(NVS_READONLY, &handle))
    {
        LOG_WARN("settings_nvs_open failed, flag_force_start_wifi_hotspot = false");
        return false;
    }
    size_t    sz      = sizeof(flag_force_start_wifi_hotspot);
    esp_err_t esp_err = nvs_get_blob(
        handle,
        RUUVI_GATEWAY_NVS_FLAG_FORCE_START_WIFI_HOTSPOT_KEY,
        &flag_force_start_wifi_hotspot,
        &sz);
    if (ESP_OK != esp_err)
    {
        LOG_WARN_ESP(esp_err, "Can't read '%s' from flash", RUUVI_GATEWAY_NVS_FLAG_FORCE_START_WIFI_HOTSPOT_KEY);
        nvs_close(handle);
        settings_write_flag_force_start_wifi_hotspot(false);
    }
    else
    {
        nvs_close(handle);
    }
    if (RUUVI_GATEWAY_NVS_FLAG_FORCE_START_WIFI_HOTSPOT_VALUE == flag_force_start_wifi_hotspot)
    {
        return true;
    }
    return false;
}

void
settings_write_flag_force_start_wifi_hotspot(const bool flag_force_start_wifi_hotspot)
{
    nvs_handle handle = 0;
    LOG_INFO("SETTINGS: Write flag_force_start_wifi_hotspot: %d", flag_force_start_wifi_hotspot);
    if (!settings_nvs_open(NVS_READWRITE, &handle))
    {
        LOG_ERR("%s failed", "settings_nvs_open");
        return;
    }
    const uint32_t flag_force_start_wifi_hotspot_val = flag_force_start_wifi_hotspot
                                                           ? RUUVI_GATEWAY_NVS_FLAG_FORCE_START_WIFI_HOTSPOT_VALUE
                                                           : 0;
    const esp_err_t esp_err = nvs_set_blob(
        handle,
        RUUVI_GATEWAY_NVS_FLAG_FORCE_START_WIFI_HOTSPOT_KEY,
        &flag_force_start_wifi_hotspot_val,
        sizeof(flag_force_start_wifi_hotspot_val));
    if (ESP_OK != esp_err)
    {
        LOG_ERR_ESP(esp_err, "%s failed", "nvs_set_blob");
    }
    nvs_close(handle);
}
