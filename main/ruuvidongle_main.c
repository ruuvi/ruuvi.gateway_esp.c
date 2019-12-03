#include "freertos/FreeRTOS.h"
#include "freertos/FreeRTOSConfig.h"
#include "freertos/projdefs.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "ruuvidongle.h"
#include "cJSON.h"
#include "esp_system.h"
#include "esp_log.h"
#include "string.h"
#include "wifi_manager.h"
#include "time_task.h"
#include "mqtt.h"
#include "nvs.h"
#include "nvs_flash.h"
#include "gpio.h"
#include "leds.h"
#include "uart.h"
#include "http.h"
#include "dns_server.h"
#include "http_server.h"

static const char TAG[] = "ruuvidongle";

EventGroupHandle_t status_bits;

char gw_mac[13] = { 0 };

struct dongle_config m_dongle_config = RUUVIDONGLE_DEFAULT_CONFIGURATION;
extern wifi_config_t* wifi_manager_config_sta;

void ruuvi_send_nrf_settings()
{
	ESP_LOGI(TAG, "sending settings to NRF: use filter: %d, company id: 0x%04x", m_dongle_config.company_filter, m_dongle_config.company_id);
	if (m_dongle_config.company_filter) {
		uart_send_nrf_command(SET_FILTER, &m_dongle_config.company_id);
	} else {
		uart_send_nrf_command(CLEAR_FILTER, 0);
	}
}

void monitoring_task(void *pvParameter)
{
	for(;;){
		ESP_LOGI(TAG, "free heap: %d",esp_get_free_heap_size());
		vTaskDelay( pdMS_TO_TICKS(10000) );
	}
}

void get_mac_address()
{
	uint8_t mac[6];
	esp_err_t err = esp_read_mac(mac, ESP_MAC_WIFI_STA);
	if (err != ESP_OK) {
		ESP_LOGE(TAG, "Can't get mac address, err: %d", err);
		return;
	}

	for (int i=0; i<6; i++) {
		sprintf(gw_mac+(i*2), "%02x", mac[i]);
	}
	gw_mac[12] = 0; //null terminator

	ESP_LOGI(TAG, "Mac address: %s", gw_mac);

}

/* brief this is an exemple of a callback that you can setup in your own app to get notified of wifi manager event */
void cb_connection_ok(void *pvParameter){
	ESP_LOGI(TAG, "Wifi connected");

	xEventGroupSetBits(status_bits, WIFI_CONNECTED_BIT);
	leds_stop_blink();
	leds_on();

	get_mac_address();

	time_sync();

	if (m_dongle_config.use_mqtt) {
		mqtt_app_start();
	}

	dns_server_stop();
	http_server_stop();
}

void cb_disconnect(void *pvParameter)
{
	ESP_LOGW(TAG, "Wifi disconnected");

	xEventGroupClearBits(status_bits, WIFI_CONNECTED_BIT);
	leds_stop_blink();
	leds_start_blink(LEDS_SLOW_BLINK);
}

esp_err_t reset_wifi_settings()
{
	if(wifi_manager_config_sta){
		memset(wifi_manager_config_sta, 0x00, sizeof(wifi_config_t));
	}

	/* save empty connection info in NVS memory */
	wifi_manager_save_sta_config();

	return ESP_OK;
}

void reset_task(void* arg)
{
	ESP_LOGI(TAG, "reset task started");
	EventBits_t bits;
	while (1) {
		bits = xEventGroupWaitBits(status_bits, RESET_BUTTON_BIT, pdTRUE, pdFALSE, pdMS_TO_TICKS(1000));
		if (bits & RESET_BUTTON_BIT) {
			ESP_LOGI(TAG, "Reset activated");

			reset_wifi_settings();  //erase wifi settings so after reboot ap will start

			esp_restart();
		}
	}
}

void app_main(void)
{
	status_bits = xEventGroupCreate();
	if (status_bits == NULL) {
		ESP_LOGE(TAG, "Can't create event group");
	}

	nvs_flash_init();
	settings_get_from_flash(&m_dongle_config);

	uart_init();
	gpio_init();
	leds_init();
	time_init();

	leds_start_blink(LEDS_FAST_BLINK);

	//esp_log_level_set(TAG, ESP_LOG_DEBUG);

	ruuvi_send_nrf_settings();

	wifi_manager_start();
	wifi_manager_set_callback(EVENT_STA_GOT_IP, &cb_connection_ok);
	wifi_manager_set_callback(EVENT_STA_DISCONNECTED, &cb_disconnect);

	xTaskCreate(monitoring_task, "monitoring_task", 2048, NULL, 1, NULL);
	xTaskCreate(reset_task, "reset_task", 1024*2, NULL, 5, NULL);

	ESP_LOGI(TAG, "Main started");
}
