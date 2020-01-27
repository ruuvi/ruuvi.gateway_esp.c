#include "ruuvidongle.h"
#include "cJSON.h"
#include "esp_log.h"
#include "nvs.h"
#include "nvs_flash.h"
#include "string.h"

static const char TAG[] = "settings";
static const char* ruuvi_dongle_nvs_namespace = "ruuvidongle";

int settings_save_to_flash(struct dongle_config *config)
{
	ESP_LOGD(TAG, "%s", __func__);

	nvs_handle handle;
	esp_err_t esp_err;

	esp_err_t ret = nvs_open(ruuvi_dongle_nvs_namespace, NVS_READWRITE, &handle);
	if (ret == ESP_OK) {
		esp_err = nvs_set_blob(handle, RUUVIDONGLE_NVS_CONFIGURATION_KEY, config, sizeof(struct dongle_config));
		if (esp_err != ESP_OK) {
			ESP_LOGE(TAG, "Can't save config to NVS, err: %02x", esp_err);
		}

		nvs_close(handle);
	} else {
		ESP_LOGE(TAG, "Can't open ruuvidongle nvs namespace, err: %02x", ret);
	}

	return 0;
}

void settings_print(struct dongle_config *config)
{
	ESP_LOGI(TAG, "config: use eth dhcp: %d", config->eth_dhcp);
	ESP_LOGI(TAG, "config: eth static ip: %s", config->eth_static_ip);
	ESP_LOGI(TAG, "config: eth netmask: %s", config->eth_netmask);
	ESP_LOGI(TAG, "config: eth gw: %s", config->eth_gw);
	ESP_LOGI(TAG, "config: use http: %d", config->use_http);
	ESP_LOGI(TAG, "config: use mqtt: %d", config->use_mqtt);
	ESP_LOGI(TAG, "config: mqtt server: %s", config->mqtt_server);
	ESP_LOGI(TAG, "config: mqtt port: %d", config->mqtt_port);
	ESP_LOGI(TAG, "config: mqtt prefix: %s", config->mqtt_prefix);
	ESP_LOGI(TAG, "config: mqtt user: %s", config->mqtt_user);
	ESP_LOGI(TAG, "config: mqtt password: %s", "********");
	ESP_LOGI(TAG, "config: http url: %s", config->http_url);
	ESP_LOGI(TAG, "config: coordinates: %s", config->coordinates);
	ESP_LOGI(TAG, "config: use company id filter: %d", config->company_filter);
	ESP_LOGI(TAG, "config: company id: 0x%04x", config->company_id);
}

bool settings_get_from_flash(struct dongle_config *dongle_config)
{
	nvs_handle handle;
	esp_err_t esp_err;
	esp_err_t ret = nvs_open(ruuvi_dongle_nvs_namespace, NVS_READONLY, &handle);
	if (ret == ESP_OK) {
		/* allocate buffer */
		size_t sz = sizeof(struct dongle_config);
		uint8_t *buff = (uint8_t*)malloc(sizeof(uint8_t) * sz);
		memset(buff, 0x00, sz);

		//sz = sizeof(m_dongle_config);
		esp_err = nvs_get_blob(handle, RUUVIDONGLE_NVS_CONFIGURATION_KEY, buff, &sz);
		if (esp_err == ESP_OK) {
			memcpy(dongle_config, buff, sz);
			ESP_LOGI(TAG, "Configuration from flash:");
		} else {
			struct dongle_config default_config = RUUVIDONGLE_DEFAULT_CONFIGURATION;
			*dongle_config = default_config;
			ESP_LOGW(TAG, "Can't read config from flash, using default");
		}
		nvs_close(handle);
		free(buff);
	} else {
		struct dongle_config default_config = RUUVIDONGLE_DEFAULT_CONFIGURATION;
		*dongle_config = default_config;
		ESP_LOGE(TAG, "%s: can't open nvs namespace, err: 0x%02x", __func__, ret);
	}

	ESP_LOGI(TAG, "Settings read from flash:");
	settings_print(dongle_config);

	return ret;
}

char* ruuvi_get_conf_json()
{
	char* buf = 0;
	struct dongle_config c = m_dongle_config;

	cJSON* root = cJSON_CreateObject();
	if (root) {
		cJSON_AddBoolToObject(root, "eth_dhcp", c.eth_dhcp);
		cJSON_AddStringToObject(root, "eth_static_ip", c.eth_static_ip);
		cJSON_AddStringToObject(root, "eth_netmask", c.eth_netmask);
		cJSON_AddStringToObject(root, "eth_gw", c.eth_gw);
		cJSON_AddBoolToObject(root, "use_http", c.use_http);
		cJSON_AddStringToObject(root, "http_url", c.http_url);
		cJSON_AddBoolToObject(root, "use_mqtt", c.use_mqtt);
		cJSON_AddStringToObject(root, "mqtt_server", c.mqtt_server);
		cJSON_AddNumberToObject(root, "mqtt_port", c.mqtt_port);
		cJSON_AddStringToObject(root, "mqtt_prefix", c.mqtt_prefix);
		cJSON_AddStringToObject(root, "mqtt_user", c.mqtt_user);
		//cJSON_AddStringToObject(root, "mqtt_pass", c.mqtt_pass);  //don't send to browser because security
		cJSON_AddStringToObject(root, "coordinates", c.coordinates);
		cJSON_AddBoolToObject(root, "use_filtering", c.company_filter);

		buf = cJSON_Print(root);
		if (!buf) {
			ESP_LOGE(TAG, "Can't create config json");
		}
		cJSON_Delete(root);
	} else {
		ESP_LOGE(TAG, "Can't create json object");
	}

	return buf;
}