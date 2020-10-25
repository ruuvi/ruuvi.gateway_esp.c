/**
 * @file settings.c
 * @author Jukka Saari
 * @date 2019-11-27
 * @copyright Ruuvi Innovations Ltd, license BSD-3-Clause.
 */

#include "settings.h"
#include "cJSON.h"
#include "nvs.h"
#include "nvs_flash.h"
#include "ruuvi_gateway.h"
#include "cjson_wrap.h"
#include "log.h"

static const char TAG[] = "settings";

static const char g_ruuvi_gateway_nvs_namespace[] = "ruuvi_gateway";

static const ruuvi_gateway_config_t default_config = RUUVI_GATEWAY_DEFAULT_CONFIGURATION;

static bool
settings_nvs_open(nvs_open_mode_t open_mode, nvs_handle_t *p_handle)
{
    const char *    nvs_name = g_ruuvi_gateway_nvs_namespace;
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
    (void)settings_nvs_set_blob(handle, RUUVI_GATEWAY_NVS_CONFIGURATION_KEY, &default_config, sizeof(default_config));
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

void
settings_print(const ruuvi_gateway_config_t *p_config)
{
    LOG_INFO("config: use eth dhcp: %d", p_config->eth_dhcp);
    LOG_INFO("config: eth static ip: %s", p_config->eth_static_ip);
    LOG_INFO("config: eth netmask: %s", p_config->eth_netmask);
    LOG_INFO("config: eth gw: %s", p_config->eth_gw);
    LOG_INFO("config: eth dns1: %s", p_config->eth_dns1);
    LOG_INFO("config: eth dns2: %s", p_config->eth_dns2);
    LOG_INFO("config: use mqtt: %d", p_config->use_mqtt);
    LOG_INFO("config: mqtt server: %s", p_config->mqtt_server);
    LOG_INFO("config: mqtt port: %u", p_config->mqtt_port);
    LOG_INFO("config: mqtt prefix: %s", p_config->mqtt_prefix);
    LOG_INFO("config: mqtt user: %s", p_config->mqtt_user);
    LOG_INFO("config: mqtt password: %s", "********");
    LOG_INFO("config: use http: %d", p_config->use_http);
    LOG_INFO("config: http url: %s", p_config->http_url);
    LOG_INFO("config: http user: %s", p_config->http_user);
    LOG_INFO("config: http pass: %s", "********");
    LOG_INFO("config: coordinates: %s", p_config->coordinates);
    LOG_INFO("config: use company id filter: %d", p_config->company_filter);
    LOG_INFO("config: company id: 0x%04x", p_config->company_id);
    LOG_INFO("config: use scan coded phy: %d", p_config->scan_coded_phy);
    LOG_INFO("config: use scan 1mbit/phy: %d", p_config->scan_1mbit_phy);
    LOG_INFO("config: use scan extended payload: %d", p_config->scan_extended_payload);
    LOG_INFO("config: use scan channel 37: %d", p_config->scan_channel_37);
    LOG_INFO("config: use scan channel 38: %d", p_config->scan_channel_38);
    LOG_INFO("config: use scan channel 39: %d", p_config->scan_channel_39);
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
        LOG_INFO("Using default config:");
        *p_gateway_config = default_config;
    }
    else
    {
        if (!settings_get_gw_cfg_from_nvs(handle, p_gateway_config))
        {
            LOG_INFO("Using default config:");
            *p_gateway_config = default_config;
        }
        else
        {
            LOG_INFO("Configuration from flash:");
        }
        nvs_close(handle);
    }
    settings_print(p_gateway_config);
}

bool
ruuvi_get_conf_json_str(cjson_wrap_str_t *p_json_str)
{
    const ruuvi_gateway_config_t *p_cfg = &g_gateway_config;

    p_json_str->p_str = NULL;

    cJSON *p_json_root = cJSON_CreateObject();
    if (NULL == p_json_root)
    {
        LOG_ERR("Can't create json object");
        return false;
    }
    cJSON_AddBoolToObject(p_json_root, "eth_dhcp", p_cfg->eth_dhcp);
    cJSON_AddStringToObject(p_json_root, "eth_static_ip", p_cfg->eth_static_ip);
    cJSON_AddStringToObject(p_json_root, "eth_netmask", p_cfg->eth_netmask);
    cJSON_AddStringToObject(p_json_root, "eth_gw", p_cfg->eth_gw);
    cJSON_AddStringToObject(p_json_root, "eth_dns1", p_cfg->eth_dns1);
    cJSON_AddStringToObject(p_json_root, "eth_dns2", p_cfg->eth_dns2);
    cJSON_AddBoolToObject(p_json_root, "use_http", p_cfg->use_http);
    cJSON_AddStringToObject(p_json_root, "http_url", p_cfg->http_url);
    cJSON_AddStringToObject(p_json_root, "http_user", p_cfg->http_user);
    cJSON_AddBoolToObject(p_json_root, "use_mqtt", p_cfg->use_mqtt);
    cJSON_AddStringToObject(p_json_root, "mqtt_server", p_cfg->mqtt_server);
    cJSON_AddNumberToObject(p_json_root, "mqtt_port", p_cfg->mqtt_port);
    cJSON_AddStringToObject(p_json_root, "mqtt_prefix", p_cfg->mqtt_prefix);
    cJSON_AddStringToObject(p_json_root, "mqtt_user", p_cfg->mqtt_user);
    // cJSON_AddStringToObject(root, "mqtt_pass", p_cfg->mqtt_pass);  //don't send to
    // browser because security
    cJSON_AddStringToObject(p_json_root, "coordinates", p_cfg->coordinates);
    cJSON_AddBoolToObject(p_json_root, "use_filtering", p_cfg->company_filter);
    cJSON_AddBoolToObject(p_json_root, "use_coded_phy", p_cfg->scan_coded_phy);
    cJSON_AddBoolToObject(p_json_root, "use_1mbit_phy", p_cfg->scan_1mbit_phy);
    cJSON_AddBoolToObject(p_json_root, "use_extended_payload", p_cfg->scan_extended_payload);
    cJSON_AddBoolToObject(p_json_root, "use_channel_37", p_cfg->scan_channel_37);
    cJSON_AddBoolToObject(p_json_root, "use_channel_38", p_cfg->scan_channel_38);
    cJSON_AddBoolToObject(p_json_root, "use_channel_39", p_cfg->scan_channel_39);
    cJSON_AddStringToObject(p_json_root, "gw_mac", gw_mac_sta.str_buf);
    char company_id[10];
    snprintf(company_id, sizeof(company_id), "0x%04x", p_cfg->company_id);
    cJSON_AddStringToObject(p_json_root, "company_id", company_id);

    *p_json_str = cjson_wrap_print_and_delete(&p_json_root);
    if (NULL == p_json_str->p_str)
    {
        LOG_ERR("Can't create config json");
        return false;
    }
    return true;
}

char *
ruuvi_get_conf_json(void)
{
    cjson_wrap_str_t json_str = {
        .p_str = NULL,
    };
    if (!ruuvi_get_conf_json_str(&json_str))
    {
        return NULL;
    }
    return (char *)json_str.p_str;
}
