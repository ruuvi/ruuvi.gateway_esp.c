#include "esp_http_client.h"
#include <string.h>
#include "esp_log.h"
#include "ruuvidongle.h"
#include "cJSON.h"
#include <time.h>

#define LOG_LOCAL_LEVEL ESP_LOG_DEBUG

static const char TAG[] = "http";

static esp_err_t http_event_handler(esp_http_client_event_t* evt)
{
	switch(evt->event_id) {
		case HTTP_EVENT_ERROR:
			ESP_LOGE(TAG, "HTTP_EVENT_ERROR");
			break;
		case HTTP_EVENT_ON_CONNECTED:
			ESP_LOGD(TAG, "HTTP_EVENT_ON_CONNECTED");
			break;
		case HTTP_EVENT_HEADER_SENT:
			ESP_LOGD(TAG, "HTTP_EVENT_HEADER_SENT");
			break;
		case HTTP_EVENT_ON_HEADER:
			//ESP_LOGD(TAG, "HTTP_EVENT_ON_HEADER");
			//printf("%.*s", evt->data_len, (char*)evt->data);
			break;
		case HTTP_EVENT_ON_DATA:
			ESP_LOGD(TAG, "HTTP_EVENT_ON_DATA, len=%d", evt->data_len);
			if (!esp_http_client_is_chunked_response(evt->client)) {
				printf("%.*s", evt->data_len, (char*)evt->data);
			}
			break;
		case HTTP_EVENT_ON_FINISH:
			ESP_LOGD(TAG, "HTTP_EVENT_ON_FINISH");
			break;
		case HTTP_EVENT_DISCONNECTED:
			ESP_LOGD(TAG, "HTTP_EVENT_DISCONNECTED");
			break;
	}
	return ESP_OK;
}

static void http_send(char *msg) {

	esp_err_t err;
	esp_http_client_handle_t http_handle;
	esp_http_client_config_t http_config = {
		.url = m_dongle_config.http_url,
		.method = HTTP_METHOD_POST,
		.event_handler = http_event_handler,
	};
	http_handle = esp_http_client_init(&http_config);
	if (http_handle == NULL) {
		ESP_LOGE(TAG, "Can't init http client");
		return;
	}

	esp_http_client_set_post_field(http_handle, msg, strlen(msg));

	err = esp_http_client_perform(http_handle);
	if (err == ESP_OK) {
		ESP_LOGD(TAG, "HTTP POST Status = %d, content_length = %d",
				esp_http_client_get_status_code(http_handle),
				esp_http_client_get_content_length(http_handle));
	} else {
		ESP_LOGE(TAG, "HTTP POST request failed: %s", esp_err_to_name(err));
	}

	err = esp_http_client_cleanup(http_handle);
}

 void http_send_advs(struct adv_report_table *reports)
{
	cJSON* tags = 0;
	time_t now;
	time(&now);
	adv_report_t* adv;

	cJSON* root = cJSON_CreateObject();
	if (root) {
		cJSON* gw = cJSON_AddObjectToObject(root, "gwmac");
		if (gw) {
			cJSON_AddStringToObject(gw, "coordinates", m_dongle_config.coordinates);
			cJSON_AddNumberToObject(gw, "timestamp", now);
			tags = cJSON_AddObjectToObject(gw, "tags");
		} else {
			ESP_LOGE(TAG, "%s: can't create json", __func__);
		}
	} else {
		ESP_LOGE(TAG, "%s: can't create json", __func__);
	}

	if (tags) {
		for (int i=0; i<reports->num_of_advs; i++) {
			adv = &reports->table[i];
			cJSON* tag = cJSON_CreateObject();
			cJSON_AddNumberToObject(tag, "rssi", adv->rssi);
			cJSON_AddNumberToObject(tag, "timestamp", adv->timestamp);
			cJSON_AddStringToObject(tag, "data", adv->data);

			cJSON_AddItemToObject(tags, adv->tag_mac, tag);
		}
	} else {
		ESP_LOGE(TAG, "%s: can't create json", __func__);
	}

	char* json_str = cJSON_Print(root);
	ESP_LOGI(TAG, "HTTP POST: %s", json_str);
	cJSON_Delete(root);

	http_send(json_str);

	free(json_str);
}
