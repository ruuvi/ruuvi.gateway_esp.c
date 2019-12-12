#include "freertos/FreeRTOS.h"
#include "freertos/FreeRTOSConfig.h"
#include "freertos/portmacro.h"
#include "freertos/task.h"
#include "freertos/projdefs.h"
#include "esp_err.h"
#include "driver/uart.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include <string.h>
#include <ctype.h>
#include "cJSON.h"
#include <time.h>
#include "mqtt.h"
#include "ruuvidongle.h"
#include "uart.h"
#include "http.h"

#define LOG_LOCAL_LEVEL ESP_LOG_DEBUG

#define TXD_PIN (GPIO_NUM_4)
#define RXD_PIN (GPIO_NUM_5)
#define UART_RX_BUF_SIZE 1024
#define BUF_MAX 1000

#define STX 0x2
#define ETX 0x3
#define NRF_CMD1 0x1
#define NRF_CMD2 0x2
#define NRF_CMD1_LEN 3
#define NRF_CMD2_LEN 1

static const char *TAG = "uart";

RingbufHandle_t uart_temp_buf;
portMUX_TYPE adv_table_mux = portMUX_INITIALIZER_UNLOCKED;

struct adv_report_table adv_reports;
struct adv_report_table adv_reports_buf;

int adv_put_to_table(adv_report_t* new_adv)
{
	portENTER_CRITICAL(&adv_table_mux);
	bool found = false;
	esp_err_t ret = ESP_OK;

	for (int i=0; i<adv_reports.num_of_advs; i++) {
		char* mac = adv_reports.table[i].tag_mac;
		if (strcmp(new_adv->tag_mac, mac) == 0) {
			found = true;
			adv_reports.table[i] = *new_adv;
		}
	}

	//not found from the table, insert if not full
	if (!found) {
		if (adv_reports.num_of_advs < MAX_ADVS_TABLE) {
			adv_reports.table[adv_reports.num_of_advs++] = *new_adv;
		} else {
			ret = ESP_ERR_NO_MEM;
		}
	}
	portEXIT_CRITICAL(&adv_table_mux);

	return ret;
}

int uart_send_data(const char* logName, const char* data)
{
	char buf[100] = { 0 };
	const int len = strlen(data);

	for (int i = 0; i < len; i++) {
		sprintf(buf+(2*i), "%02x", data[i]);
	}

	const int txBytes = uart_write_bytes(UART_NUM_1, data, len);
	ESP_LOGI(logName, "Wrote to uart %d bytes, 0x%s", txBytes, buf);

	return txBytes;
}

void uart_send_nrf_command(nrf_command_t command, void* arg)
{
	char data[10] = { 0 };

	data[0] = STX;

	switch(command) {
		case SET_FILTER: {
			uint16_t company_id = *(uint16_t*)arg;
			data[1] = NRF_CMD1_LEN;
			data[2] = NRF_CMD1;
			memcpy(&data[3], &company_id, 2);
			data[5] = ETX;

			uart_send_data(TAG, data);
			break;
		}
		case CLEAR_FILTER: {
			data[1] = NRF_CMD2_LEN;
			data[2] = NRF_CMD2;
			data[3] = ETX;

			uart_send_data(TAG, data);
			break;
		}
		default:
		break;
	}
}

static bool is_hexstr(char* str) {
	for (int i=0; i<strlen(str); i++) {
		if (isxdigit(str[i]) == 0) {
			return ESP_ERR_INVALID_ARG;
		}
	}
	return ESP_OK;
}

static int is_adv_report_valid(adv_report_t* adv)
{
	esp_err_t err = ESP_OK;
	if (is_hexstr(adv->tag_mac) != ESP_OK) {
		err = ESP_ERR_INVALID_ARG;
	}

	if (is_hexstr(adv->data) != ESP_OK) {
		err = ESP_ERR_INVALID_ARG;
	}

	return err;
}

static int parse_adv_report_from_uart(char *msg_orig, int len, adv_report_t *adv) {
	ESP_LOGD(TAG, "%s: len: %d, data: %s", __func__, len, msg_orig);
	//TODO this assumes that data starts with valid message. It's possible that first there is junk bytes
	//data should be iterated with different start positions and try to find a message
	esp_err_t err = ESP_OK;

	const int tokens_num = 3;
	int rssi = -1;
	char *tokens[tokens_num+5];
	int i = 0;
	char *pch;
	char *data;
	char *tag_mac;

	char *msg = strdup(msg_orig);

	pch = strtok(msg, ",");
	while (pch != NULL && i < 4) {
		tokens[i++] = pch;
		pch = strtok(NULL, ",");
	}

	if (i != tokens_num) {
		err = ESP_ERR_INVALID_ARG;
		ESP_LOGW(TAG, "tokens: %d", i);
	}

	tag_mac = tokens[0];
	data = tokens[1];
	rssi = atoi(tokens[2]);
	time_t now = 0;
	time(&now);

	if (strlen(tag_mac) != MAC_LEN) {
		err = ESP_ERR_INVALID_SIZE;
		ESP_LOGW(TAG, "mac len wrong");
	}

	if (strlen(data) > ADV_DATA_MAX_LEN) {
		err = ESP_ERR_INVALID_SIZE;
		ESP_LOGW(TAG, "data len wrong");
	}

	if (rssi > 0) {
		err = ESP_ERR_INVALID_ARG;
		ESP_LOGW(TAG, "rssi wrong");
	}

	if (!err) {
		adv->rssi = rssi;
		strncpy(adv->tag_mac, tag_mac, MAC_LEN);
		adv->tag_mac[MAC_LEN] = 0;
		strncpy(adv->data, data, ADV_DATA_MAX_LEN);
		adv->data[ADV_DATA_MAX_LEN] = 0;
		adv->timestamp = now;

		if (is_adv_report_valid(adv)) {
			err = ESP_ERR_INVALID_ARG;
		}
	} else {
		//ESP_LOGW(TAG, "invalid adv:");
	}

	free(msg);

	return err;
}

static void rx_task(void *arg)
{
	static const char *RX_TASK_TAG = "RX_TASK";
	esp_log_level_set(RX_TASK_TAG, ESP_LOG_INFO);
	uint8_t* data = (uint8_t*) malloc(UART_RX_BUF_SIZE+1);

	uart_temp_buf = xRingbufferCreate(BUF_MAX, RINGBUF_TYPE_NOSPLIT);
	if (uart_temp_buf == NULL) {
		printf("Failed to create temp ring buffer");
	}

	while (1) {
		const int rxBytes = uart_read_bytes(UART_NUM_1, data, UART_RX_BUF_SIZE, 1000 / portTICK_RATE_MS);
		if (rxBytes > 0) {
			data[rxBytes] = 0;
			ESP_LOGV(RX_TASK_TAG, "%d bytes from uart: %s", rxBytes, data);
			//ESP_LOG_BUFFER_HEXDUMP(RX_TASK_TAG, data, rxBytes, ESP_LOG_INFO);

			size_t temp_size = 0;
			char* temp = 0;
			char new_temp[2*UART_RX_BUF_SIZE];
			temp = xRingbufferReceive(uart_temp_buf, &temp_size, pdMS_TO_TICKS(0));
			ESP_LOGV(TAG, "%zu bytes from temp buffer", temp_size);
			if (temp != NULL) {
				vRingbufferReturnItem(uart_temp_buf, temp);
				ESP_LOG_BUFFER_HEXDUMP(TAG, temp, temp_size, ESP_LOG_VERBOSE);
			}
			memcpy(new_temp, temp, temp_size);
			memcpy(new_temp + temp_size, data, rxBytes);

			char adv_data[UART_RX_BUF_SIZE];
			int i, j;
			uint end = 0;
			int dataBytes = temp_size + rxBytes;

			for (i = 0, j = 0; i < dataBytes; i++) {
				adv_data[j++] = new_temp[i];
				if (new_temp[i] == '\n') {
					adv_data[j-1] = 0;

					adv_report_t adv_report;
					if (parse_adv_report_from_uart(adv_data, j, &adv_report) == ESP_OK) {
						int ret = adv_put_to_table(&adv_report);
						if (ret == ESP_ERR_NO_MEM) {
							ESP_LOGW(TAG, "Adv report table full, adv dropped");
						}
					} else {
						ESP_LOGW(TAG, "invalid adv: %s", adv_data);
					}

					end = i+1;
					j = 0;
				}
				//TODO when adv_data fills with junk data, save end of it to temp buffer. This way we won't lose a message which is preceded with junk
				if (j >= UART_RX_BUF_SIZE) {
					j = 0;
					end = i+1;
					ESP_LOGW(TAG, "new_data buffer filled, reset new_data");

				}
			}

			if (dataBytes - end) {
				ESP_LOGD(TAG, "dataBytes: %d, end: %d, j: %d", dataBytes, end, j);
				ESP_LOG_BUFFER_HEXDUMP(TAG, adv_data, j, ESP_LOG_DEBUG);
				int ret = xRingbufferSend(uart_temp_buf, new_temp + end, dataBytes - end, 0);
				if (ret != pdTRUE) {
					ESP_LOGE(TAG, "%s: Can't push to uart_temp_buf", __func__);
				}
			}
		}
	}
	free(data);
}

static void adv_post_task(void *arg)
{
	esp_log_level_set(TAG, ESP_LOG_DEBUG);

	ESP_LOGI(TAG, "%s", __func__);
	while (1) {
		adv_report_t* adv = 0;
		ESP_LOGI(TAG, "advertisements in table:");
		for (int i = 0; i < adv_reports.num_of_advs; i++) {
			adv = &adv_reports.table[i];
			ESP_LOGD(TAG, "i: %d, tag: %s, rssi: %d, data: %s, timestamp: %ld", i, adv->tag_mac, adv->rssi, adv->data, adv->timestamp);
		}

		//for thread safety copy the advertisements to a separate buffer for posting
		portENTER_CRITICAL(&adv_table_mux);
		adv_reports_buf = adv_reports;
		adv_reports.num_of_advs = 0; //clear the table
		portEXIT_CRITICAL(&adv_table_mux);

		if (xEventGroupGetBits(status_bits) & WIFI_CONNECTED_BIT) {
			if (m_dongle_config.use_http) {
				http_send_advs(&adv_reports_buf);
			}
			if (m_dongle_config.use_mqtt && xEventGroupGetBits(status_bits) & MQTT_CONNECTED_BIT) {
				mqtt_publish_table(&adv_reports_buf);
			}
		} else {
			ESP_LOGW(TAG, "Can't send, wifi not connected");
		}

		vTaskDelay( pdMS_TO_TICKS(ADV_POST_INTERVAL) );
	}
}

void uart_init(void) {
	const uart_config_t uart_config = {
		.baud_rate = 115200,
		.data_bits = UART_DATA_8_BITS,
		.parity = UART_PARITY_DISABLE,
		.stop_bits = UART_STOP_BITS_1,
		.flow_ctrl = UART_HW_FLOWCTRL_DISABLE
	};
	uart_param_config(UART_NUM_1, &uart_config);
	uart_set_pin(UART_NUM_1, TXD_PIN, RXD_PIN, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
	// We won't use a buffer for sending data.
	uart_driver_install(UART_NUM_1, UART_RX_BUF_SIZE * 2, 0, 0, NULL, 0);

	adv_reports.num_of_advs = 0;

	xTaskCreate(rx_task, "uart_rx_task", 1024*6, NULL, 1, NULL);
	xTaskCreate(adv_post_task, "adv_post_task", 1024*4, NULL, 1, NULL);
}
