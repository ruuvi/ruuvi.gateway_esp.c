/**
 * @file settings.c
 * @author Jukka Saari
 * @date 2019-11-27
 * @copyright Ruuvi Innovations Ltd, license BSD-3-Clause.
 */

#include "cJSON.h"
#include "esp_log.h"
#include "nvs.h"
#include "nvs_flash.h"
#include "ruuvi_gateway.h"
#include "string.h"

static const char TAG[] = "settings";

static const char g_ruuvi_gateway_nvs_namespace[] = "ruuvi_gateway";

static const ruuvi_gateway_config_t default_config = RUUVI_GATEWAY_DEFAULT_CONFIGURATION;

int
settings_clear_in_flash(void)
{
    ESP_LOGD(TAG, "%s", __func__);
    nvs_handle handle  = 0;
    esp_err_t  esp_err = 0;
    esp_err_t  ret     = nvs_open(g_ruuvi_gateway_nvs_namespace, NVS_READWRITE, &handle);
    if (ret == ESP_OK)
    {
        esp_err = nvs_set_blob(handle, RUUVI_GATEWAY_NVS_CONFIGURATION_KEY, &default_config, sizeof(default_config));
        if (esp_err != ESP_OK)
        {
            ESP_LOGE(TAG, "Can't save config to NVS, err: %02x", esp_err);
        }
        nvs_close(handle);
    }
    else
    {
        ESP_LOGE(TAG, "Can't open '%s' nvs namespace, err: %02x", RUUVI_GATEWAY_NVS_CONFIGURATION_KEY, ret);
    }
    return 0;
}

int
settings_save_to_flash(ruuvi_gateway_config_t *config)
{
    ESP_LOGD(TAG, "%s", __func__);
    nvs_handle handle;
    esp_err_t  esp_err;
    esp_err_t  ret = nvs_open(g_ruuvi_gateway_nvs_namespace, NVS_READWRITE, &handle);

    if (ret == ESP_OK)
    {
        esp_err = nvs_set_blob(handle, RUUVI_GATEWAY_NVS_CONFIGURATION_KEY, config, sizeof(ruuvi_gateway_config_t));

        if (esp_err != ESP_OK)
        {
            ESP_LOGE(TAG, "Can't save config to NVS, err: %02x", esp_err);
        }

        nvs_close(handle);
    }
    else
    {
        ESP_LOGE(TAG, "Can't open %s nvs namespace, err: %02x", g_ruuvi_gateway_nvs_namespace, ret);
    }

    return 0;
}

void
settings_print(ruuvi_gateway_config_t *config)
{
    ESP_LOGI(TAG, "config: use eth dhcp: %d", config->eth_dhcp);
    ESP_LOGI(TAG, "config: eth static ip: %s", config->eth_static_ip);
    ESP_LOGI(TAG, "config: eth netmask: %s", config->eth_netmask);
    ESP_LOGI(TAG, "config: eth gw: %s", config->eth_gw);
    ESP_LOGI(TAG, "config: eth dns1: %s", config->eth_dns1);
    ESP_LOGI(TAG, "config: eth dns2: %s", config->eth_dns2);
    ESP_LOGI(TAG, "config: use mqtt: %d", config->use_mqtt);
    ESP_LOGI(TAG, "config: mqtt server: %s", config->mqtt_server);
    ESP_LOGI(TAG, "config: mqtt port: %u", config->mqtt_port);
    ESP_LOGI(TAG, "config: mqtt prefix: %s", config->mqtt_prefix);
    ESP_LOGI(TAG, "config: mqtt user: %s", config->mqtt_user);
    ESP_LOGI(TAG, "config: mqtt password: %s", "********");
    ESP_LOGI(TAG, "config: use http: %d", config->use_http);
    ESP_LOGI(TAG, "config: http url: %s", config->http_url);
    ESP_LOGI(TAG, "config: http user: %s", config->http_user);
    ESP_LOGI(TAG, "config: http pass: %s", "********");
    ESP_LOGI(TAG, "config: coordinates: %s", config->coordinates);
    ESP_LOGI(TAG, "config: use company id filter: %d", config->company_filter);
    ESP_LOGI(TAG, "config: company id: 0x%04x", config->company_id);
    ESP_LOGI(TAG, "config: use scan coded phy: %d", config->scan_coded_phy);
    ESP_LOGI(TAG, "config: use scan 1mbit/phy: %d", config->scan_1mbit_phy);
    ESP_LOGI(TAG, "config: use scan extended payload: %d", config->scan_extended_payload);
    ESP_LOGI(TAG, "config: use scan channel 37: %d", config->scan_channel_37);
    ESP_LOGI(TAG, "config: use scan channel 38: %d", config->scan_channel_38);
    ESP_LOGI(TAG, "config: use scan channel 39: %d", config->scan_channel_39);
}

static bool
settings_get_from_nvs_handle(nvs_handle handle, ruuvi_gateway_config_t *p_gateway_config)
{
    size_t sz = 0;
    {
        const esp_err_t esp_err = nvs_get_blob(handle, RUUVI_GATEWAY_NVS_CONFIGURATION_KEY, NULL, &sz);
        if (ESP_OK != esp_err)
        {
            ESP_LOGW(TAG, "Can't read config from flash");
            return false;
        }
    }

    if (sizeof(*p_gateway_config) != sz)
    {
        ESP_LOGW(TAG, "Size of config in flash differs");
        return false;
    }

    const esp_err_t esp_err = nvs_get_blob(handle, RUUVI_GATEWAY_NVS_CONFIGURATION_KEY, p_gateway_config, &sz);
    if (ESP_OK != esp_err)
    {
        ESP_LOGW(TAG, "Can't read config from flash");
        return false;
    }

    if (RUUVI_GATEWAY_CONFIG_HEADER != p_gateway_config->header)
    {
        ESP_LOGW(TAG, "Incorrect config header (0x%02X)", p_gateway_config->header);
        return false;
    }
    if (RUUVI_GATEWAY_CONFIG_FMT_VERSION != p_gateway_config->fmt_version)
    {
        ESP_LOGW(
            TAG,
            "Incorrect config fmt version (exp 0x%02x, act 0x%02x)",
            RUUVI_GATEWAY_CONFIG_FMT_VERSION,
            p_gateway_config->fmt_version);
        return false;
    }
    return true;
}

void
settings_get_from_flash(ruuvi_gateway_config_t *p_gateway_config)
{
    nvs_handle      handle = 0;
    const esp_err_t ret    = nvs_open(g_ruuvi_gateway_nvs_namespace, NVS_READONLY, &handle);
    if (ESP_OK != ret)
    {
        ESP_LOGE(TAG, "Can't open '%s' namespace in NVS, err: 0x%02x", g_ruuvi_gateway_nvs_namespace, ret);
        ESP_LOGI(TAG, "Using default config:");
        *p_gateway_config = default_config;
    }
    else
    {
        if (!settings_get_from_nvs_handle(handle, p_gateway_config))
        {
            ESP_LOGI(TAG, "Using default config:");
            *p_gateway_config = default_config;
        }
        else
        {
            ESP_LOGI(TAG, "Configuration from flash:");
        }
        nvs_close(handle);
    }
    settings_print(p_gateway_config);
}

char *
ruuvi_get_conf_json()
{
    char *                 buf  = 0;
    ruuvi_gateway_config_t c    = g_gateway_config;
    cJSON *                root = cJSON_CreateObject();

    if (root)
    {
        cJSON_AddBoolToObject(root, "eth_dhcp", c.eth_dhcp);
        cJSON_AddStringToObject(root, "eth_static_ip", c.eth_static_ip);
        cJSON_AddStringToObject(root, "eth_netmask", c.eth_netmask);
        cJSON_AddStringToObject(root, "eth_gw", c.eth_gw);
        cJSON_AddStringToObject(root, "eth_dns1", c.eth_dns1);
        cJSON_AddStringToObject(root, "eth_dns2", c.eth_dns2);
        cJSON_AddBoolToObject(root, "use_http", c.use_http);
        cJSON_AddStringToObject(root, "http_url", c.http_url);
        cJSON_AddStringToObject(root, "http_user", c.http_user);
        cJSON_AddBoolToObject(root, "use_mqtt", c.use_mqtt);
        cJSON_AddStringToObject(root, "mqtt_server", c.mqtt_server);
        cJSON_AddNumberToObject(root, "mqtt_port", c.mqtt_port);
        cJSON_AddStringToObject(root, "mqtt_prefix", c.mqtt_prefix);
        cJSON_AddStringToObject(root, "mqtt_user", c.mqtt_user);
        // cJSON_AddStringToObject(root, "mqtt_pass", c.mqtt_pass);  //don't send to
        // browser because security
        cJSON_AddStringToObject(root, "coordinates", c.coordinates);
        cJSON_AddBoolToObject(root, "use_filtering", c.company_filter);
        cJSON_AddBoolToObject(root, "use_coded_phy", c.scan_coded_phy);
        cJSON_AddBoolToObject(root, "use_1mbit_phy", c.scan_1mbit_phy);
        cJSON_AddBoolToObject(root, "use_extended_payload", c.scan_extended_payload);
        cJSON_AddBoolToObject(root, "use_channel_37", c.scan_channel_37);
        cJSON_AddBoolToObject(root, "use_channel_38", c.scan_channel_38);
        cJSON_AddBoolToObject(root, "use_channel_39", c.scan_channel_39);
        cJSON_AddStringToObject(root, "gw_mac", gw_mac_sta.str_buf);
        char company_id[10];
        snprintf(company_id, sizeof(company_id), "0x%04x", c.company_id);
        cJSON_AddStringToObject(root, "company_id", company_id);
        buf = cJSON_Print(root);

        if (!buf)
        {
            ESP_LOGE(TAG, "Can't create config json");
        }

        cJSON_Delete(root);
    }
    else
    {
        ESP_LOGE(TAG, "Can't create json object");
    }

    return buf;
}
