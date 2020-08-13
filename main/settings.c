#include "ruuvi_gateway.h"
#include "cJSON.h"
#include "esp_log.h"
#include "nvs.h"
#include "nvs_flash.h"
#include "string.h"

static const char  TAG[]                      = "settings";
static const char *ruuvi_dongle_nvs_namespace = "ruuvidongle";

static const ruuvi_gateway_config_t default_config = RUUVIDONGLE_DEFAULT_CONFIGURATION;

int
settings_clear_in_flash(void)
{
    ESP_LOGD(TAG, "%s", __func__);
    nvs_handle handle  = 0;
    esp_err_t  esp_err = 0;
    esp_err_t  ret     = nvs_open(ruuvi_dongle_nvs_namespace, NVS_READWRITE, &handle);
    if (ret == ESP_OK)
    {
        esp_err = nvs_set_blob(handle, RUUVIDONGLE_NVS_CONFIGURATION_KEY, &default_config, sizeof(default_config));
        if (esp_err != ESP_OK)
        {
            ESP_LOGE(TAG, "Can't save config to NVS, err: %02x", esp_err);
        }
        nvs_close(handle);
    }
    else
    {
        ESP_LOGE(TAG, "Can't open '%s' nvs namespace, err: %02x", RUUVIDONGLE_NVS_CONFIGURATION_KEY, ret);
    }
    return 0;
}

int
settings_save_to_flash(ruuvi_gateway_config_t *config)
{
    ESP_LOGD(TAG, "%s", __func__);
    nvs_handle handle;
    esp_err_t  esp_err;
    esp_err_t  ret = nvs_open(ruuvi_dongle_nvs_namespace, NVS_READWRITE, &handle);

    if (ret == ESP_OK)
    {
        esp_err = nvs_set_blob(handle, RUUVIDONGLE_NVS_CONFIGURATION_KEY, config, sizeof(ruuvi_gateway_config_t));

        if (esp_err != ESP_OK)
        {
            ESP_LOGE(TAG, "Can't save config to NVS, err: %02x", esp_err);
        }

        nvs_close(handle);
    }
    else
    {
        ESP_LOGE(TAG, "Can't open ruuvidongle nvs namespace, err: %02x", ret);
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
}

static bool
settings_get_from_nvs_handle(nvs_handle handle, ruuvi_gateway_config_t *dongle_config)
{
    size_t sz = 0;
    {
        const esp_err_t esp_err = nvs_get_blob(handle, RUUVIDONGLE_NVS_CONFIGURATION_KEY, NULL, &sz);
        if (ESP_OK != esp_err)
        {
            ESP_LOGW(TAG, "Can't read config from flash");
            return false;
        }
    }

    if (sizeof(*dongle_config) != sz)
    {
        ESP_LOGW(TAG, "Size of config in flash differs");
        return false;
    }

    const esp_err_t esp_err = nvs_get_blob(handle, RUUVIDONGLE_NVS_CONFIGURATION_KEY, dongle_config, &sz);
    if (ESP_OK != esp_err)
    {
        ESP_LOGW(TAG, "Can't read config from flash");
        return false;
    }

    if (RUUVI_DONGLE_CONFIG_HEADER != dongle_config->header)
    {
        ESP_LOGW(TAG, "Incorrect config header (0x%02X)", dongle_config->header);
        return false;
    }
    if (RUUVI_DONGLE_CONFIG_FMT_VERSION != dongle_config->fmt_version)
    {
        ESP_LOGW(
            TAG,
            "Incorrect config fmt version (exp 0x%02x, act 0x%02x)",
            RUUVI_DONGLE_CONFIG_FMT_VERSION,
            dongle_config->fmt_version);
        return false;
    }
    return true;
}

void
settings_get_from_flash(ruuvi_gateway_config_t *dongle_config)
{
    nvs_handle      handle = 0;
    const esp_err_t ret    = nvs_open(ruuvi_dongle_nvs_namespace, NVS_READONLY, &handle);
    if (ESP_OK != ret)
    {
        ESP_LOGE(TAG, "Can't open '%s' namespace in NVS, err: 0x%02x", ruuvi_dongle_nvs_namespace, ret);
        ESP_LOGI(TAG, "Using default config:");
        *dongle_config = default_config;
    }
    else
    {
        if (!settings_get_from_nvs_handle(handle, dongle_config))
        {
            ESP_LOGI(TAG, "Using default config:");
            *dongle_config = default_config;
        }
        else
        {
            ESP_LOGI(TAG, "Configuration from flash:");
        }
        nvs_close(handle);
    }
    settings_print(dongle_config);
}

char *
ruuvi_get_conf_json()
{
    char *                 buf  = 0;
    ruuvi_gateway_config_t c    = m_dongle_config;
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
        // cJSON_AddStringToObject(root, "mqtt_pass", c.mqtt_pass);  //don't send to browser because security
        cJSON_AddStringToObject(root, "coordinates", c.coordinates);
        cJSON_AddBoolToObject(root, "use_filtering", c.company_filter);
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