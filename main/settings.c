/**
 * @file settings.c
 * @author Jukka Saari
 * @date 2019-11-27
 * @copyright Ruuvi Innovations Ltd, license BSD-3-Clause.
 */

#include "settings.h"
#include <string.h>
#include "cJSON.h"
#include "nvs.h"
#include "cjson_wrap.h"
#include "gw_cfg.h"
#include "gw_cfg_default.h"
#include "gw_cfg_blob.h"
#include "gw_cfg_json_parse.h"
#include "gw_cfg_json_generate.h"
#include "gw_cfg_log.h"
#include "os_malloc.h"
#include "wifi_manager.h"
#include "ruuvi_nvs.h"

#define LOG_LOCAL_LEVEL LOG_LEVEL_INFO
#include "log.h"

#if (LOG_LOCAL_LEVEL >= LOG_LEVEL_DEBUG) && !RUUVI_TESTS
#warning Debug log level prints out the passwords as a "plaintext".
#endif

#define RUUVI_GATEWAY_NVS_CFG_BLOB_KEY "ruuvi_config" /* deprecated */
#define RUUVI_GATEWAY_NVS_CFG_JSON_KEY "ruuvi_cfg_json"
#define RUUVI_GATEWAY_NVS_MAC_ADDR_KEY "ruuvi_mac_addr"

#define RUUVI_GATEWAY_NVS_FLAG_REBOOTING_AFTER_AUTO_UPDATE_KEY   "ruuvi_auto_udp"
#define RUUVI_GATEWAY_NVS_FLAG_REBOOTING_AFTER_AUTO_UPDATE_VALUE (0xAACC5533U)

#define RUUVI_GATEWAY_NVS_FLAG_FORCE_START_WIFI_HOTSPOT_KEY       "ruuvi_wifi_ap"
#define RUUVI_GATEWAY_NVS_FLAG_FORCE_START_WIFI_HOTSPOT_DISABLED  (0)
#define RUUVI_GATEWAY_NVS_FLAG_FORCE_START_WIFI_HOTSPOT_ONCE      (0xAACC5533U)
#define RUUVI_GATEWAY_NVS_FLAG_FORCE_START_WIFI_HOTSPOT_PERMANENT (0xBBDD6677U)

static const char TAG[] = "settings";

bool
settings_check_in_flash(void)
{
    nvs_handle handle = 0;
    if (!ruuvi_nvs_open(NVS_READONLY, &handle))
    {
        return false;
    }
    nvs_close(handle);
    return true;
}

static char *
settings_read_gw_cfg_json_from_nvs(nvs_handle handle)
{
    size_t    cfg_json_size = 0;
    esp_err_t esp_err       = nvs_get_str(handle, RUUVI_GATEWAY_NVS_CFG_JSON_KEY, NULL, &cfg_json_size);
    if (ESP_OK != esp_err)
    {
        LOG_ERR_ESP(esp_err, "Can't find config key '%s' in flash", RUUVI_GATEWAY_NVS_CFG_JSON_KEY);
        return NULL;
    }

    char *p_cfg_json = os_malloc(cfg_json_size);
    if (NULL == p_cfg_json)
    {
        LOG_ERR("Can't allocate %lu bytes for configuration", (printf_ulong_t)cfg_json_size);
        return NULL;
    }

    esp_err = nvs_get_str(handle, RUUVI_GATEWAY_NVS_CFG_JSON_KEY, p_cfg_json, &cfg_json_size);
    if (ESP_OK != esp_err)
    {
        LOG_ERR_ESP(esp_err, "Can't read config-json from flash by key '%s'", RUUVI_GATEWAY_NVS_CFG_JSON_KEY);
        os_free(p_cfg_json);
        return NULL;
    }
    return p_cfg_json;
}

static bool
settings_save_to_flash_cjson(const char *const p_json_str)
{
    LOG_DBG("Save config to NVS: %s", (NULL != p_json_str) ? p_json_str : "");

    nvs_handle handle = 0;
    if (!ruuvi_nvs_open(NVS_READWRITE, &handle))
    {
        LOG_ERR("Failed to open NVS for writing");
        return false;
    }

    char *p_gw_cfg_prev = settings_read_gw_cfg_json_from_nvs(handle);
    if (NULL == p_gw_cfg_prev)
    {
        LOG_WARN("%s failed", "settings_read_gw_cfg_json_from_nvs");
    }
    else
    {
        const bool flag_is_cfg_equal = (0 == strcmp(p_gw_cfg_prev, p_json_str)) ? true : false;
        os_free(p_gw_cfg_prev);
        if (flag_is_cfg_equal)
        {
            nvs_close(handle);
            LOG_INFO("### Save config to NVS: not needed (gw_cfg was not modified)");
            return true;
        }
    }

    esp_err_t err = nvs_set_str(handle, RUUVI_GATEWAY_NVS_CFG_JSON_KEY, (NULL != p_json_str) ? p_json_str : "");
    if (ESP_OK != err)
    {
        LOG_ERR_ESP(err, "%s failed", "nvs_set_str");
        nvs_close(handle);
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

    LOG_INFO("### Save config to NVS: successfully updated");

    return true;
}

void
settings_save_to_flash(const gw_cfg_t *const p_gw_cfg)
{
    cjson_wrap_str_t cjson_str = { 0 };
    if (!gw_cfg_json_generate_full(p_gw_cfg, &cjson_str))
    {
        LOG_ERR("%s failed", "gw_cfg_json_generate");
    }
    else
    {
        settings_save_to_flash_cjson(cjson_str.p_str);
    }
    cjson_wrap_free_json_str(&cjson_str);
}

static bool
settings_get_gw_cfg_from_nvs(nvs_handle handle, gw_cfg_t *const p_gw_cfg, bool *const p_flag_modified)
{
    gw_cfg_default_get(p_gw_cfg);

    char *p_cfg_json = settings_read_gw_cfg_json_from_nvs(handle);
    if (NULL == p_cfg_json)
    {
        LOG_ERR("%s failed", "settings_read_gw_cfg_json_from_nvs");
        return false;
    }

    gw_cfg_default_get(p_gw_cfg);

    if (!gw_cfg_json_parse("NVS", "Read config from NVS:", p_cfg_json, p_gw_cfg, p_flag_modified))
    {
        LOG_ERR("Failed to parse config-json or no memory");
        os_free(p_cfg_json);
        return false;
    }
    os_free(p_cfg_json);

    return true;
}

static void
settings_erase_gw_cfg_blob_if_exist(nvs_handle handle)
{
    size_t    sz      = 0;
    esp_err_t esp_err = nvs_get_blob(handle, RUUVI_GATEWAY_NVS_CFG_BLOB_KEY, NULL, &sz);
    if (ESP_OK != esp_err)
    {
        return;
    }
    LOG_INFO("Erase deprecated cfg BLOB");
    const esp_err_t err = nvs_erase_key(handle, RUUVI_GATEWAY_NVS_CFG_BLOB_KEY);
    if (ESP_OK != err)
    {
        LOG_ERR_ESP(err, "Failed to erase deprecated cfg BLOB");
    }
}

static bool
settings_get_gw_cfg_blob_from_nvs(nvs_handle handle, ruuvi_gateway_config_blob_t *const p_gw_cfg_blob)
{
    size_t    sz      = 0;
    esp_err_t esp_err = nvs_get_blob(handle, RUUVI_GATEWAY_NVS_CFG_BLOB_KEY, NULL, &sz);
    if (ESP_OK != esp_err)
    {
        LOG_ERR_ESP(esp_err, "Can't find config key '%s' in flash", RUUVI_GATEWAY_NVS_CFG_BLOB_KEY);
        return false;
    }

    if (sizeof(*p_gw_cfg_blob) != sz)
    {
        LOG_WARN("Size of config in flash differs");
        return false;
    }

    esp_err = nvs_get_blob(handle, RUUVI_GATEWAY_NVS_CFG_BLOB_KEY, p_gw_cfg_blob, &sz);
    if (ESP_OK != esp_err)
    {
        LOG_ERR_ESP(esp_err, "Can't find config key '%s' in flash", RUUVI_GATEWAY_NVS_CFG_BLOB_KEY);
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
settings_read_from_blob(nvs_handle handle, gw_cfg_t *const p_gw_cfg)
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
            LOG_INFO("Convert Cfg BLOB");
            gw_cfg_blob_convert(p_gw_cfg, p_gw_cfg_blob);
        }
        os_free(p_gw_cfg_blob);
    }
    return flag_use_default_config;
}

const gw_cfg_t *
settings_get_from_flash(bool *const p_flag_default_cfg_is_used)
{
    gw_cfg_t *p_gw_cfg_tmp = os_calloc(1, sizeof(*p_gw_cfg_tmp));
    if (NULL == p_gw_cfg_tmp)
    {
        LOG_ERR("Can't allocate memory for gw_cfg");
        return NULL;
    }
    bool       flag_use_default_config = false;
    bool       flag_modified           = false;
    nvs_handle handle                  = 0;
    if (!ruuvi_nvs_open(NVS_READWRITE, &handle))
    {
        flag_use_default_config = true;
    }
    else
    {
        if (!settings_get_gw_cfg_from_nvs(handle, p_gw_cfg_tmp, &flag_modified))
        {
            flag_use_default_config = settings_read_from_blob(handle, p_gw_cfg_tmp);
            if (!flag_use_default_config)
            {
                flag_modified = true;
            }
        }
        nvs_close(handle);
    }
    *p_flag_default_cfg_is_used = flag_use_default_config;
    if (flag_use_default_config)
    {
        LOG_WARN("Using default config");
        gw_cfg_default_get(p_gw_cfg_tmp);
    }

    const bool flag_wifi_cfg_blob_used = wifi_manager_cfg_blob_read(&p_gw_cfg_tmp->wifi_cfg);
    if (flag_wifi_cfg_blob_used)
    {
        gw_cfg_log_wifi_cfg_ap(&p_gw_cfg_tmp->wifi_cfg.ap, "Got wifi_cfg from NVS BLOB:");
        gw_cfg_log_wifi_cfg_sta(&p_gw_cfg_tmp->wifi_cfg.sta, "Got wifi_cfg from NVS BLOB:");
    }

    if (flag_modified || flag_wifi_cfg_blob_used)
    {
        LOG_INFO("Update config in flash");
        settings_save_to_flash(p_gw_cfg_tmp); // Update configuration in NVS before erasing BLOBs
    }
    settings_erase_gw_cfg_blob_if_exist(handle);
    if (flag_wifi_cfg_blob_used)
    {
        if (!wifi_manager_cfg_blob_mark_deprecated())
        {
            LOG_ERR("Failed to erase wifi_cfg_blob");
        }
    }

    return p_gw_cfg_tmp;
}

mac_address_bin_t
settings_read_mac_addr(void)
{
    mac_address_bin_t mac_addr = { 0 };
    nvs_handle        handle   = 0;
    if (!ruuvi_nvs_open(NVS_READONLY, &handle))
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
    if (!ruuvi_nvs_open(NVS_READWRITE, &handle))
    {
        LOG_ERR("%s failed", "ruuvi_nvs_open");
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
settings_update_mac_addr(const mac_address_bin_t new_mac_addr)
{
    const mac_address_bin_t saved_mac_addr = settings_read_mac_addr();
    if (0 != memcmp(&saved_mac_addr, &new_mac_addr, sizeof(new_mac_addr)))
    {
        const mac_address_str_t new_mac_addr_str = mac_address_to_str(&new_mac_addr);
        LOG_INFO("### Save new MAC-address: %s", new_mac_addr_str.str_buf);
        settings_write_mac_addr(&new_mac_addr);
    }
}

bool
settings_read_flag_rebooting_after_auto_update(void)
{
    uint32_t   flag_rebooting_after_auto_update = 0;
    nvs_handle handle                           = 0;
    if (!ruuvi_nvs_open(NVS_READONLY, &handle))
    {
        LOG_WARN("ruuvi_nvs_open failed, flag_rebooting_after_auto_update = false");
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
    LOG_INFO("### SETTINGS: Write flag_rebooting_after_auto_update: %d", flag_rebooting_after_auto_update);
    nvs_handle handle = 0;
    if (!ruuvi_nvs_open(NVS_READWRITE, &handle))
    {
        LOG_ERR("%s failed", "ruuvi_nvs_open");
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

force_start_wifi_hotspot_t
settings_read_flag_force_start_wifi_hotspot(void)
{
    uint32_t   flag_force_start_wifi_hotspot_val = 0;
    nvs_handle handle                            = 0;
    if (!ruuvi_nvs_open(NVS_READONLY, &handle))
    {
        LOG_WARN("ruuvi_nvs_open failed, flag_force_start_wifi_hotspot_val = false");
        return FORCE_START_WIFI_HOTSPOT_DISABLED;
    }
    size_t    sz      = sizeof(flag_force_start_wifi_hotspot_val);
    esp_err_t esp_err = nvs_get_blob(
        handle,
        RUUVI_GATEWAY_NVS_FLAG_FORCE_START_WIFI_HOTSPOT_KEY,
        &flag_force_start_wifi_hotspot_val,
        &sz);
    if (ESP_OK != esp_err)
    {
        LOG_WARN_ESP(esp_err, "Can't read '%s' from flash", RUUVI_GATEWAY_NVS_FLAG_FORCE_START_WIFI_HOTSPOT_KEY);
        nvs_close(handle);
        settings_write_flag_force_start_wifi_hotspot(FORCE_START_WIFI_HOTSPOT_DISABLED);
    }
    else
    {
        nvs_close(handle);
    }

    force_start_wifi_hotspot_t force_start_wifi_hotspot = FORCE_START_WIFI_HOTSPOT_DISABLED;
    switch (flag_force_start_wifi_hotspot_val)
    {
        case RUUVI_GATEWAY_NVS_FLAG_FORCE_START_WIFI_HOTSPOT_DISABLED:
            force_start_wifi_hotspot = FORCE_START_WIFI_HOTSPOT_DISABLED;
            break;
        case RUUVI_GATEWAY_NVS_FLAG_FORCE_START_WIFI_HOTSPOT_ONCE:
            force_start_wifi_hotspot = FORCE_START_WIFI_HOTSPOT_ONCE;
            break;
        case RUUVI_GATEWAY_NVS_FLAG_FORCE_START_WIFI_HOTSPOT_PERMANENT:
            force_start_wifi_hotspot = FORCE_START_WIFI_HOTSPOT_PERMANENT;
            break;
        default:
            force_start_wifi_hotspot = FORCE_START_WIFI_HOTSPOT_DISABLED;
            break;
    }
    return force_start_wifi_hotspot;
}

void
settings_write_flag_force_start_wifi_hotspot(const force_start_wifi_hotspot_t force_start_wifi_hotspot)
{
    nvs_handle handle = 0;
    LOG_INFO("### SETTINGS: Write flag_force_start_wifi_hotspot: %d", (printf_int_t)force_start_wifi_hotspot);
    if (!ruuvi_nvs_open(NVS_READWRITE, &handle))
    {
        LOG_ERR("%s failed", "ruuvi_nvs_open");
        return;
    }
    uint32_t flag_force_start_wifi_hotspot_val = RUUVI_GATEWAY_NVS_FLAG_FORCE_START_WIFI_HOTSPOT_DISABLED;
    switch (force_start_wifi_hotspot)
    {
        case FORCE_START_WIFI_HOTSPOT_DISABLED:
            flag_force_start_wifi_hotspot_val = RUUVI_GATEWAY_NVS_FLAG_FORCE_START_WIFI_HOTSPOT_DISABLED;
            break;
        case FORCE_START_WIFI_HOTSPOT_ONCE:
            flag_force_start_wifi_hotspot_val = RUUVI_GATEWAY_NVS_FLAG_FORCE_START_WIFI_HOTSPOT_ONCE;
            break;
        case FORCE_START_WIFI_HOTSPOT_PERMANENT:
            flag_force_start_wifi_hotspot_val = RUUVI_GATEWAY_NVS_FLAG_FORCE_START_WIFI_HOTSPOT_PERMANENT;
            break;
    }
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
