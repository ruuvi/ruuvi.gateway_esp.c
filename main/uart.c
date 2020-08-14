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
#include "ruuvi_gateway.h"
#include "uart.h"
#include "http.h"
#include "ruuvi_board_gwesp.h"
#include "ruuvi_endpoints.h"
#include "ruuvi_endpoint_ca_uart.h"

//#define LOG_LOCAL_LEVEL ESP_LOG_DEBUG

#define UART_RX_BUF_SIZE 1024
#define BUF_MAX          1000

static const char *TAG = "uart";

RingbufHandle_t uart_temp_buf;
portMUX_TYPE    adv_table_mux = portMUX_INITIALIZER_UNLOCKED;

struct adv_report_table adv_reports;
struct adv_report_table adv_reports_buf;

/**
 * @brief Print given binary into hex string.
 *
 * Example {0xA0, 0xBB, 0x31} -> "A0BB31"
 *
 * @param[out] hexstr String to print into. Must be at least 2 * binlen + 1 bytes long.
 * @param[in]  bin Binary to print from.
 * @param[in]  binlen Size of binary in bytes.
 */
static void
bin2hex(char *const hexstr, const size_t hexstr_size, const uint8_t *const bin, size_t binlen)
{
    size_t ii = 0;
    for (ii = 0; ii < binlen; ii++)
    {
        if ((2 * ii + 3) > hexstr_size)
        {
            break;
        }
        sprintf(hexstr + (2 * ii), "%02X", bin[ii]);
    }

    hexstr[2 * ii] = 0;
}

static esp_err_t
adv_put_to_table(const adv_report_t *const p_adv)
{
    portENTER_CRITICAL(&adv_table_mux);
    gw_metrics.received_advertisements++;
    bool      found = false;
    esp_err_t ret   = ESP_OK;

    // Check if we already have advertisement with this MAC
    for (int i = 0; i < adv_reports.num_of_advs; i++)
    {
        const mac_address_bin_t *p_mac = &adv_reports.table[i].tag_mac;

        if (memcmp(&p_adv->tag_mac, p_mac, sizeof(*p_mac)) == 0)
        {
            // Yes, update data.
            found                = true;
            adv_reports.table[i] = *p_adv;
        }
    }

    // not found from the table, insert if not full
    if (!found)
    {
        if (adv_reports.num_of_advs < MAX_ADVS_TABLE)
        {
            adv_reports.table[adv_reports.num_of_advs++] = *p_adv;
        }
        else
        {
            ret = ESP_ERR_NO_MEM;
        }
    }

    portEXIT_CRITICAL(&adv_table_mux);
    return ret;
}

int
uart_send_data(const char *logName, const char *data)
{
    const int len = strlen(data);

    const int txBytes = uart_write_bytes(UART_NUM_1, data, len);
    ESP_LOGI(logName, "Wrote to uart %d bytes:", txBytes);
    if (0 != txBytes)
    {
        ESP_LOG_BUFFER_HEXDUMP(logName, data, txBytes, ESP_LOG_INFO);
    }
    return txBytes;
}

void
uart_send_nrf_command(nrf_command_t command, void *arg)
{
    // data[0] = STX;
    switch (command)
    {
#if 0
            char data[10] = { 0 };

        case SET_FILTER:
        {
            uint16_t company_id = * (uint16_t *) arg;
            data[1] = NRF_CMD1_LEN;
            data[2] = NRF_CMD1;
            data[3] = company_id & 0xFFU;
            data[4] = (uint16_t)(company_id >> 8U) & 0xFFU;
            data[5] = ETX;
            uart_send_data (TAG, data);
            break;
        }

        case CLEAR_FILTER:
        {
            data[1] = NRF_CMD2_LEN;
            data[2] = NRF_CMD2;
            data[3] = ETX;
            uart_send_data (TAG, data);
            break;
        }

#endif
        default:
            break;
    }
}

static bool
is_hexstr(char *str)
{
    for (int i = 0; i < strlen(str); i++)
    {
        if (isxdigit(str[i]) == 0)
        {
            return ESP_ERR_INVALID_ARG;
        }
    }

    return ESP_OK;
}

static int
is_adv_report_valid(adv_report_t *adv)
{
    esp_err_t err = ESP_OK;

    if (is_hexstr(adv->data) != ESP_OK)
    {
        err = ESP_ERR_INVALID_ARG;
    }

    return err;
}

static int
parse_adv_report_from_uart(const re_ca_uart_payload_t *const msg, adv_report_t *adv)
{
    esp_err_t err = ESP_OK;

    if (NULL == msg)
    {
        err = ESP_ERR_INVALID_ARG;
    }
    else if (NULL == adv)
    {
        err = ESP_ERR_INVALID_ARG;
    }
    else if (msg->cmd != RE_CA_UART_ADV_RPRT)
    {
        err = ESP_ERR_INVALID_ARG;
    }
    else
    {
        const re_ca_uart_ble_adv_t *const report = &(msg->params.adv);
        time_t                            now    = 0;
        time(&now);
        adv->rssi      = report->rssi_db;
        adv->timestamp = now;
        mac_address_bin_init(&adv->tag_mac, report->mac);

        if (is_adv_report_valid(adv))
        {
            err = ESP_ERR_INVALID_ARG;
        }

        bin2hex(adv->data, sizeof(adv->data), report->adv, report->adv_len);
    }

    return err;
}

/**
 * @brief Search for STX in data. Move First found STX to index 0.
 *
 * Example {A B C D STX D B A, 8} -> {STX D B A X X X X, 4}
 *
 * @param[in,out] data Input: buffer to search for STX.
 *                     Output: buffer moved to start with STX.
 * @param[in,out] data_len Input: Length of data in buffer.
 *                         Output: Length of buffer after move
 * @retval true Buffer starts with STX
 * @retval false STX not found, data not modified.
 */
static bool
search_stx(uint8_t *const data, int32_t *const data_len)
{
    int32_t start = 0;

    for (; (start < *data_len) && (data[start] != RE_CA_UART_STX); start++)
        ;

    // If data has to be moved.
    if (start && (start < *data_len))
    {
        // Get number of bytes to move.
        int32_t moved_bytes = *data_len - start;
        // Move data to beginning of the message
        memmove(data, data + start, moved_bytes);
        // Clear out moved data
        memset(data + moved_bytes, 0, *data_len - moved_bytes);
        // Return new length
        *data_len = moved_bytes;
    }

    return data[0] == RE_CA_UART_STX;
}

static void
rx_task(void *arg)
{
    static const char *RX_TASK_TAG = "RX_TASK";
    esp_log_level_set(RX_TASK_TAG, ESP_LOG_DEBUG);
    uint8_t current_message[RE_CA_UART_TX_MAX_LEN]; //!< Store for incoming message.
    uint8_t next_message[RE_CA_UART_TX_MAX_LEN];    //!< Store for leftover bytes after message.
    int32_t current_index = 0;
    int32_t next_index    = 0; //!< Index where we are in message.
    uart_temp_buf         = xRingbufferCreate(BUF_MAX, RINGBUF_TYPE_NOSPLIT);

    if (uart_temp_buf == NULL)
    {
        ESP_LOGE(RX_TASK_TAG, "Failed to create temp ring buffer");
        ESP_ERROR_CHECK(1); // XXX crash here
    }

    while (1)
    {
        // Read incoming bytes.
        int32_t rxBytes = uart_read_bytes(
            UART_NUM_1,
            next_message + next_index,
            RE_CA_UART_TX_MAX_LEN - next_index,
            1000 / portTICK_RATE_MS);
        rxBytes += next_index;
        ESP_LOGD(RX_TASK_TAG, "Parsing %d bytes", rxBytes);
        ESP_LOG_BUFFER_HEXDUMP(TAG, next_message, rxBytes, ESP_LOG_DEBUG);

        // Handle incoming data. - refactor into function
        while (rxBytes > 0)
        {
            // Can we fit all the new data to our message buffer?
            if (RE_CA_UART_TX_MAX_LEN >= (rxBytes + current_index))
            {
                memcpy(current_message + current_index, next_message, rxBytes);
                current_index += rxBytes;
                next_index = 0;
                rxBytes    = 0;
            }
            // If not, how about some of it?
            else if (RE_CA_UART_TX_MAX_LEN > current_index)
            {
                int32_t fill_bytes = RE_CA_UART_TX_MAX_LEN - current_index;
                memcpy(current_message + current_index, next_message, fill_bytes);
                current_index += fill_bytes;
                memmove(next_message, next_message + rxBytes - fill_bytes, fill_bytes);
                next_index = fill_bytes;
                rxBytes -= fill_bytes;
            }
            else
            {
                ESP_LOGE(RX_TASK_TAG, "UART Parser fatal error");
                ESP_ERROR_CHECK(1); // XXX crash here
            }

            ESP_LOGD(RX_TASK_TAG, "Having %d bytes", current_index);
            ESP_LOG_BUFFER_HEXDUMP(TAG, current_message, current_index, ESP_LOG_DEBUG);

            // If current message starts with STX - refactor into function
            if (search_stx(current_message, &current_index))
            {
                re_ca_uart_payload_t msg = { 0 };
                ESP_LOGD(RX_TASK_TAG, "Got %d bytes", current_index);
                ESP_LOG_BUFFER_HEXDUMP(TAG, current_message, current_index, ESP_LOG_DEBUG);

                // If we have at least a message header.
                if (RE_CA_UART_HEADER_SIZE < current_index)
                {
                    // If message ends with ETX
                    size_t etx_index = current_message[RE_CA_UART_LEN_INDEX] + RE_CA_UART_HEADER_SIZE;
                    ESP_LOGD(
                        RX_TASK_TAG,
                        "Expecting %02X at ETX, got %02X",
                        RE_CA_UART_ETX,
                        current_message[etx_index]);

                    if (RE_CA_UART_ETX == current_message[etx_index])
                    {
                        re_status_t err_code = RE_SUCCESS;
                        err_code             = re_ca_uart_decode(current_message, &msg);

                        // if decode successful clear out old bytes - refactor into function
                        if (RE_SUCCESS == err_code)
                        {
                            ESP_LOGD(RX_TASK_TAG, "Remaining %d bytes", current_index);
                            adv_report_t adv_report;

                            // Refactor into function
                            if (parse_adv_report_from_uart(&msg, &adv_report) == ESP_OK)
                            {
                                int ret = adv_put_to_table(&adv_report);

                                if (ret == ESP_ERR_NO_MEM)
                                {
                                    ESP_LOGW(TAG, "Adv report table full, adv dropped");
                                }
                            }
                        }
                        else
                        {
                            //
                            ESP_LOGD(RX_TASK_TAG, "Decoding error");
                            ESP_LOG_BUFFER_HEXDUMP(TAG, current_message, current_index, ESP_LOG_DEBUG);
                        }

                        memset(current_message, 0, etx_index);

                        if (!search_stx(current_message, &current_index))
                        {
                            memset(current_message, 0, current_index);
                            current_index = 0;
                        }
                    }
                    // If message does not end with ETX, it's rubbish, go to next STX
                    else
                    {
                        current_message[0] = 0;

                        if (!search_stx(current_message, &current_index))
                        {
                            memset(current_message, 0, current_index);
                            current_index = 0;
                        }
                    }
                }
            }
            // If message does contain STX, it's rubbish, delete.
            else
            {
                memset(current_message, 0, current_index);
                current_index = 0;
            }
        }
    }
}

_Noreturn static void
adv_post_task(void *arg)
{
    esp_log_level_set(TAG, ESP_LOG_INFO);
    ESP_LOGI(TAG, "%s", __func__);
    bool flagConnected = false;

    while (1)
    {
        adv_report_t *adv = 0;
        ESP_LOGI(TAG, "advertisements in table:");

        for (int i = 0; i < adv_reports.num_of_advs; i++)
        {
            adv                             = &adv_reports.table[i];
            const mac_address_str_t mac_str = mac_address_to_str(&adv->tag_mac);
            ESP_LOGI(
                TAG,
                "i: %d, tag: %s, rssi: %d, data: %s, timestamp: %ld",
                i,
                mac_str.str_buf,
                adv->rssi,
                adv->data,
                adv->timestamp);
        }

        // for thread safety copy the advertisements to a separate buffer for posting
        portENTER_CRITICAL(&adv_table_mux);
        adv_reports_buf         = adv_reports;
        adv_reports.num_of_advs = 0; // clear the table
        portEXIT_CRITICAL(&adv_table_mux);
        EventBits_t status = xEventGroupGetBits(status_bits);

        if (!flagConnected)
        {
            if ((status & WIFI_CONNECTED_BIT) || (status & ETH_CONNECTED_BIT))
            {
                if (g_gateway_config.use_http)
                {
                    flagConnected = true;
                    char json_str[64];
                    snprintf(
                        json_str,
                        sizeof(json_str),
                        "{\"status\": \"online\", \"gw_mac\": \"%s\"}",
                        gw_mac_sta.str_buf);
                    ESP_LOGI(TAG, "HTTP POST: %s", json_str);
                    http_send(json_str);
                }
                else if (g_gateway_config.use_mqtt)
                {
                    flagConnected = true;
                }
            }
        }
        else
        {
            if (!((status & WIFI_CONNECTED_BIT) || (status & ETH_CONNECTED_BIT)))
            {
                flagConnected = false;
            }
        }

        if (adv_reports_buf.num_of_advs)
        {
            if (flagConnected)
            {
                if (g_gateway_config.use_http)
                {
                    http_send_advs(&adv_reports_buf);
                }
                if (g_gateway_config.use_mqtt && (xEventGroupGetBits(status_bits) & MQTT_CONNECTED_BIT))
                {
                    mqtt_publish_table(&adv_reports_buf);
                }
            }
            else
            {
                ESP_LOGW(TAG, "Can't send, no network connection");
            }
        }
        vTaskDelay(pdMS_TO_TICKS(ADV_POST_INTERVAL));
    }
}

/**
 * @brief Convert definition in ruuvi.boards.c to baudrate understood by ESP32
 *
 * @param[in] baud Baudrate configured to ruuvi.boards.c, unsigned int with not
 *                 explicit size.
 * @return Associated baudrate, 115200 by default.
 */
static inline uint32_t
rb_to_esp_baud(const unsigned int baud)
{
    uint32_t baudrate = 9600U;

    switch (baud)
    {
        case RB_UART_BAUDRATE_115200:
            baudrate = 115200;
            break;

        case RB_UART_BAUDRATE_9600:
        default:
            baudrate = 9600U;
            break;
    }

    return baudrate;
}

void
uart_init(void)
{
    const uart_config_t uart_config
        = { .data_bits = UART_DATA_8_BITS, //!< Only supported option my nRF52811
            .stop_bits = UART_STOP_BITS_1, //!< Only supported option by nRF52811
            .baud_rate = rb_to_esp_baud(RB_UART_BAUDRATE),
            .parity    = RB_PARITY_ENABLED ? UART_PARITY_ODD : UART_PARITY_DISABLE, // XXX
            .flow_ctrl = RB_HWFC_ENABLED ? UART_HW_FLOWCTRL_CTS_RTS : UART_HW_FLOWCTRL_DISABLE };
    uart_param_config(UART_NUM_1, &uart_config);
    // XXX Flow control pins not set by board definition.
    uart_set_pin(UART_NUM_1, TXD_PIN, RXD_PIN, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
    // We won't use a buffer for sending data.
    uart_driver_install(UART_NUM_1, UART_RX_BUF_SIZE * 2, 0, 0, NULL, 0);
    adv_reports.num_of_advs = 0;
    xTaskCreate(rx_task, "uart_rx_task", 1024 * 6, NULL, 1, NULL);
    xTaskCreate(adv_post_task, "adv_post_task", 1024 * 4, NULL, 1, NULL);
}
