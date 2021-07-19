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
#include "log.h"

#define RUUVI_GATEWAY_NVS_NAMESPACE         "ruuvi_gateway"
#define RUUVI_GATEWAY_NVS_CONFIGURATION_KEY "ruuvi_config"
#define RUUVI_GATEWAY_NVS_MAC_ADDR_KEY      "ruuvi_mac_addr"
#define RUUVI_GATEWAY_NVS_BOOT_KEY          "ruuvi_boot"

static const char TAG[] = "settings";

static bool
settings_nvs_open(nvs_open_mode_t open_mode, nvs_handle_t *p_handle)
{
    const char *    nvs_name = RUUVI_GATEWAY_NVS_NAMESPACE;
    const esp_err_t err      = nvs_open(nvs_name, open_mode, p_handle);
    if (ESP_OK != err)
    {
        LOG_ERR_ESP(err, "Can't open '%s' NVS namespace", nvs_name);
        return false;
    }
    return true;
}

static bool
settings_nvs_set_blob(nvs_handle_t handle, const char *key, const void *value, size_t length)
{
    const esp_err_t esp_err = nvs_set_blob(handle, key, value, length);
    if (ESP_OK != esp_err)
    {
        LOG_ERR_ESP(esp_err, "Can't save config to NVS");
        return false;
    }
    return true;
}

void
settings_clear_in_flash(void)
{
    LOG_DBG(".");
    nvs_handle handle = 0;
    if (!settings_nvs_open(NVS_READWRITE, &handle))
    {
        return;
    }
    (void)settings_nvs_set_blob(
        handle,
        RUUVI_GATEWAY_NVS_CONFIGURATION_KEY,
        &g_gateway_config_default,
        sizeof(g_gateway_config_default));
    nvs_close(handle);
}

void
settings_save_to_flash(const ruuvi_gateway_config_t *p_config)
{
    LOG_DBG(".");
    nvs_handle handle = 0;
    if (!settings_nvs_open(NVS_READWRITE, &handle))
    {
        return;
    }
    (void)settings_nvs_set_blob(handle, RUUVI_GATEWAY_NVS_CONFIGURATION_KEY, p_config, sizeof(*p_config));
    nvs_close(handle);
}

static bool
settings_get_gw_cfg_from_nvs(nvs_handle handle, ruuvi_gateway_config_t *p_gw_cfg)
{
    size_t    sz      = 0;
    esp_err_t esp_err = nvs_get_blob(handle, RUUVI_GATEWAY_NVS_CONFIGURATION_KEY, NULL, &sz);
    if (ESP_OK != esp_err)
    {
        LOG_WARN_ESP(esp_err, "Can't read config from flash");
        return false;
    }

    if (sizeof(*p_gw_cfg) != sz)
    {
        LOG_WARN("Size of config in flash differs");
        return false;
    }

    esp_err = nvs_get_blob(handle, RUUVI_GATEWAY_NVS_CONFIGURATION_KEY, p_gw_cfg, &sz);
    if (ESP_OK != esp_err)
    {
        LOG_WARN_ESP(esp_err, "Can't read config from flash");
        return false;
    }

    if (RUUVI_GATEWAY_CONFIG_HEADER != p_gw_cfg->header)
    {
        LOG_WARN("Incorrect config header (0x%02X)", p_gw_cfg->header);
        return false;
    }
    if (RUUVI_GATEWAY_CONFIG_FMT_VERSION != p_gw_cfg->fmt_version)
    {
        LOG_WARN(
            "Incorrect config fmt version (exp 0x%02x, act 0x%02x)",
            RUUVI_GATEWAY_CONFIG_FMT_VERSION,
            p_gw_cfg->fmt_version);
        return false;
    }
    return true;
}

void
settings_get_from_flash(ruuvi_gateway_config_t *p_gateway_config)
{
    nvs_handle handle = 0;
    if (!settings_nvs_open(NVS_READONLY, &handle))
    {
        LOG_WARN("Using default config:");
        *p_gateway_config = g_gateway_config_default;
    }
    else
    {
        if (!settings_get_gw_cfg_from_nvs(handle, p_gateway_config))
        {
            LOG_INFO("Using default config:");
            *p_gateway_config = g_gateway_config_default;
        }
        else
        {
            LOG_INFO("Configuration from flash:");
        }
        nvs_close(handle);
    }
    gw_cfg_print_to_log(p_gateway_config);
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
        if (!settings_nvs_set_blob(handle, RUUVI_GATEWAY_NVS_MAC_ADDR_KEY, p_mac_addr, sizeof(*p_mac_addr)))
        {
            LOG_ERR("%s failed", "settings_nvs_set_blob");
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
