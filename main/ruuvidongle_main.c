#include "freertos/FreeRTOS.h"
#include "freertos/FreeRTOSConfig.h"
#include "freertos/projdefs.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "driver/gpio.h"
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
#include "ethernet.h"
#include "ruuvi_board_gwesp.h"

static const char TAG[] = "ruuvidongle";

EventGroupHandle_t status_bits;

char gw_mac[MAC_LEN + 1] = { 0 };

struct dongle_config m_dongle_config = RUUVIDONGLE_DEFAULT_CONFIGURATION;
extern wifi_config_t * wifi_manager_config_sta;

void ruuvi_send_nrf_settings (struct dongle_config * config)
{
    ESP_LOGI (TAG, "sending settings to NRF: use filter: %d, company id: 0x%04x",
              config->company_filter, config->company_id);

    if (config->company_filter)
    {
        uart_send_nrf_command (SET_FILTER, &config->company_id);
    }
    else
    {
        uart_send_nrf_command (CLEAR_FILTER, 0);
    }
}

void monitoring_task (void * pvParameter)
{
    for (;;)
    {
        ESP_LOGI (TAG, "free heap: %d", esp_get_free_heap_size());
        vTaskDelay (pdMS_TO_TICKS (10000));
    }
}

void get_mac_address (esp_mac_type_t type)
{
    uint8_t mac[6];
    esp_err_t err = esp_read_mac (mac, type);

    if (err != ESP_OK)
    {
        ESP_LOGE (TAG, "Can't get mac address, err: %d", err);
        return;
    }

    for (int i = 0; i < 6; i++)
    {
        sprintf (gw_mac + (i * 2), "%02x", mac[i]);
    }

    gw_mac[12] = 0; //null terminator
    ESP_LOGI (TAG, "Mac address: %s", gw_mac);
}

/* brief this is an exemple of a callback that you can setup in your own app to get notified of wifi manager event */
void wifi_connection_ok_cb (void * pvParameter)
{
    ESP_LOGI (TAG, "Wifi connected");
    xEventGroupSetBits (status_bits, WIFI_CONNECTED_BIT);
    leds_stop_blink();
    leds_on();
    get_mac_address (ESP_MAC_WIFI_STA);
    start_services();
}

void ethernet_link_up_cb()
{
}

void ethernet_link_down_cb()
{
    ESP_LOGI (TAG, "Ethernet lost connection");
    xEventGroupClearBits (status_bits, ETH_CONNECTED_BIT);
    leds_stop_blink();
    leds_start_blink (LEDS_SLOW_BLINK);
}

void ethernet_connection_ok_cb()
{
    ESP_LOGI (TAG, "Ethernet connected");
    wifi_manager_stop();
    get_mac_address (ESP_MAC_ETH);
    leds_stop_blink();
    leds_on();
    xEventGroupSetBits (status_bits, ETH_CONNECTED_BIT);
    start_services();
}

void wifi_disconnect_cb (void * pvParameter)
{
    ESP_LOGW (TAG, "Wifi disconnected");
    xEventGroupClearBits (status_bits, WIFI_CONNECTED_BIT);
    leds_stop_blink();
    leds_start_blink (LEDS_SLOW_BLINK);
}

void start_services()
{
    time_sync();

    if (m_dongle_config.use_mqtt)
    {
        mqtt_app_start();
    }
}

void reset_task (void * arg)
{
    ESP_LOGI (TAG, "reset task started");
    EventBits_t bits;

    while (1)
    {
        bits = xEventGroupWaitBits (status_bits, RESET_BUTTON_BIT, pdTRUE, pdFALSE,
                                    pdMS_TO_TICKS (1000));

        if (bits & RESET_BUTTON_BIT)
        {
            ESP_LOGI (TAG, "Reset activated");
            // restart the Gateway,
            // on boot it will check if RB_BUTTON_RESET_PIN is pressed and erase the settings in flash.
            esp_restart();
        }
    }
}

void wifi_init()
{
    wifi_manager_start();
    wifi_manager_set_callback (EVENT_STA_GOT_IP, &wifi_connection_ok_cb);
    wifi_manager_set_callback (EVENT_STA_DISCONNECTED, &wifi_disconnect_cb);
}

void app_main (void)
{
    esp_log_level_set (TAG, ESP_LOG_DEBUG);
    status_bits = xEventGroupCreate();

    if (status_bits == NULL)
    {
        ESP_LOGE (TAG, "Can't create event group");
    }

    nvs_flash_init();
    gpio_init();
    leds_init();

    if (0 == gpio_get_level (RB_BUTTON_RESET_PIN))
    {
        ESP_LOGI (TAG, "Reset button is pressed during boot - clear settings in flash");
        wifi_manager_clear_sta_config();
        settings_clear_in_flash();
        ESP_LOGI (TAG, "Wait until the reset button is released");
        leds_start_blink (LEDS_MEDIUM_BLINK);
        while (0 == gpio_get_level (RB_BUTTON_RESET_PIN))
        {
            vTaskDelay(1);
        }
        ESP_LOGI (TAG, "Reset activated");
        esp_restart();
    }

    settings_get_from_flash (&m_dongle_config);
    uart_init();
    time_init();
    leds_start_blink (LEDS_FAST_BLINK);
    ruuvi_send_nrf_settings (&m_dongle_config);
    wifi_init();
    ethernet_init();
    xTaskCreate (monitoring_task, "monitoring_task", 2048, NULL, 1, NULL);
    xTaskCreate (reset_task, "reset_task", 1024 * 2, NULL, 1, NULL);
    ESP_LOGI (TAG, "Main started");
}
