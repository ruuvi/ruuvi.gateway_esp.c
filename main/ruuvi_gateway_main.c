/**
 * @file ruuvi_gateway_main.c
 * @author Jukka Saari
 * @date 2019-11-27
 * @copyright Ruuvi Innovations Ltd, license BSD-3-Clause.
 */

#include "ruuvi_gateway.h"
#include "adv_post.h"
#include "api.h"
#include "cJSON.h"
#include "dns_server.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_system.h"
#include "ethernet.h"
#include "freertos/FreeRTOS.h"
#include "freertos/FreeRTOSConfig.h"
#include "freertos/event_groups.h"
#include "freertos/projdefs.h"
#include "freertos/task.h"
#include "gpio.h"
#include "http.h"
#include "http_server.h"
#include "leds.h"
#include "mqtt.h"
#include "nvs.h"
#include "nvs_flash.h"
#include "ruuvi_boards.h"
#include "string.h"
#include "terminal.h"
#include "time_task.h"
#include "wifi_manager.h"
#include "ruuvi_endpoint_ca_uart.h"
#include "nrf52fw.h"

static const char TAG[] = "ruuvi_gateway";

EventGroupHandle_t status_bits;

mac_address_str_t gw_mac_sta = { 0 };

ruuvi_gateway_config_t g_gateway_config = RUUVI_GATEWAY_DEFAULT_CONFIGURATION;

extern wifi_config_t *wifi_manager_config_sta;

static inline uint8_t
conv_bool_to_u8(const bool x)
{
    return x ? (uint8_t)RE_CA_BOOL_ENABLE : (uint8_t)RE_CA_BOOL_DISABLE;
}

void
ruuvi_send_nrf_settings(ruuvi_gateway_config_t *config)
{
    ESP_LOGI(
        TAG,
        "sending settings to NRF: use filter: %d, "
        "company id: 0x%04x,"
        "use scan coded phy: %d,"
        "use scan 1mbit/phy: %d,"
        "use scan extended payload: %d,"
        "use scan channel 37: %d,"
        "use scan channel 38: %d,"
        "use scan channel 39: %d",
        config->company_filter,
        config->company_id,
        config->scan_coded_phy,
        config->scan_1mbit_phy,
        config->scan_extended_payload,
        config->scan_channel_37,
        config->scan_channel_38,
        config->scan_channel_39);

    api_send_all(
        RE_CA_UART_SET_ALL,
        config->company_id,
        conv_bool_to_u8(config->company_filter),
        conv_bool_to_u8(config->scan_coded_phy),
        conv_bool_to_u8(config->scan_extended_payload),
        conv_bool_to_u8(config->scan_1mbit_phy),
        conv_bool_to_u8(config->scan_channel_37),
        conv_bool_to_u8(config->scan_channel_38),
        conv_bool_to_u8(config->scan_channel_39));
}

void
ruuvi_send_nrf_get_id(void)
{
    api_send_get_device_id(RE_CA_UART_GET_DEVICE_ID);
}

void
monitoring_task(void *pvParameter)
{
    (void)pvParameter;
    for (;;)
    {
        ESP_LOGI(TAG, "free heap: %d", esp_get_free_heap_size());
        vTaskDelay(pdMS_TO_TICKS(10000));
    }
}

mac_address_str_t
get_gw_mac_sta(void)
{
    mac_address_bin_t mac = { 0 };
    esp_err_t         err = esp_read_mac(mac.mac, ESP_MAC_WIFI_STA);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Can't get mac address, err: %d", err);
        mac_address_str_t mac_str = { 0 };
        return mac_str;
    }
    return mac_address_to_str(&mac);
}

/* brief this is an exemple of a callback that you can setup in your own app to
 * get notified of wifi manager event */
void
wifi_connection_ok_cb(void *pvParameter)
{
    (void)pvParameter;
    ESP_LOGI(TAG, "Wifi connected");
    xEventGroupSetBits(status_bits, WIFI_CONNECTED_BIT);
    leds_stop_blink();
    leds_on();
    start_services();
}

void
ethernet_link_up_cb(void)
{
    ESP_LOGI(TAG, "Ethernet connection established");
}

void
ethernet_link_down_cb(void)
{
    ESP_LOGI(TAG, "Ethernet lost connection");
    xEventGroupClearBits(status_bits, ETH_CONNECTED_BIT);
    leds_stop_blink();
    leds_start_blink(LEDS_SLOW_BLINK);
}

void
ethernet_connection_ok_cb(void)
{
    ESP_LOGI(TAG, "Ethernet connected");
    wifi_manager_stop();
    leds_stop_blink();
    leds_on();
    xEventGroupSetBits(status_bits, ETH_CONNECTED_BIT);
    start_services();
}

void
wifi_disconnect_cb(void *pvParameter)
{
    (void)pvParameter;
    ESP_LOGW(TAG, "Wifi disconnected");
    xEventGroupClearBits(status_bits, WIFI_CONNECTED_BIT);
    leds_stop_blink();
    leds_start_blink(LEDS_SLOW_BLINK);
}

void
start_services(void)
{
    time_sync();

    if (g_gateway_config.use_mqtt)
    {
        mqtt_app_start();
    }
}

void
reset_task(void *arg)
{
    ESP_LOGI(TAG, "reset task started");
    EventBits_t bits;

    while (1)
    {
        bits = xEventGroupWaitBits(status_bits, RESET_BUTTON_BIT, pdTRUE, pdFALSE, pdMS_TO_TICKS(1000));

        if (bits & RESET_BUTTON_BIT)
        {
            ESP_LOGI(TAG, "Reset activated");
            // restart the Gateway,
            // on boot it will check if RB_BUTTON_RESET_PIN is pressed and erase the
            // settings in flash.
            esp_restart();
        }
    }
}

void
wifi_init(void)
{
    static const WiFiAntConfig_t wiFiAntConfig = {
      .wifiAntGpioConfig =
          {
              .gpio_cfg =
                  {
                          [0] =
                              {
                                  .gpio_select = 1, .gpio_num = RB_GWBUS_LNA,
                              },
                          [1] =
                              {
                                  .gpio_select = 0, .gpio_num = 0,
                              },
                          [2] =
                              {
                                  .gpio_select = 0, .gpio_num = 0,
                              },
                          [3] =
                              {
                                  .gpio_select = 0, .gpio_num = 0,
                              },
                  },
          },
      .wifiAntConfig = {.rx_ant_mode = WIFI_ANT_MODE_ANT1,
                        .rx_ant_default = WIFI_ANT_ANT1,
                        .tx_ant_mode = WIFI_ANT_MODE_ANT0,
                        .enabled_ant0 = 0,
                        .enabled_ant1 = 1},
  };
    wifi_manager_start(&wiFiAntConfig);
    wifi_manager_set_callback(EVENT_STA_GOT_IP, &wifi_connection_ok_cb);
    wifi_manager_set_callback(EVENT_STA_DISCONNECTED, &wifi_disconnect_cb);
}

void
app_main(void)
{
    esp_log_level_set(TAG, ESP_LOG_DEBUG);
    status_bits = xEventGroupCreate();

    if (status_bits == NULL)
    {
        ESP_LOGE(TAG, "Can't create event group");
    }

    nvs_flash_init();
    gpio_init();
    leds_init();

    if (0 == gpio_get_level(RB_BUTTON_RESET_PIN))
    {
        ESP_LOGI(TAG, "Reset button is pressed during boot - clear settings in flash");
        wifi_manager_clear_sta_config();
        settings_clear_in_flash();
        ESP_LOGI(TAG, "Wait until the reset button is released");
        leds_start_blink(LEDS_MEDIUM_BLINK);
        while (0 == gpio_get_level(RB_BUTTON_RESET_PIN))
        {
            vTaskDelay(1);
        }
        ESP_LOGI(TAG, "Reset activated");
        esp_restart();
    }

    nrf52fw_update_fw_if_necessary();

    settings_get_from_flash(&g_gateway_config);
    adv_post_init();
    terminal_open(NULL, true);
    api_process(1);
    time_init();
    leds_start_blink(LEDS_FAST_BLINK);
    ruuvi_send_nrf_settings(&g_gateway_config);
    gw_mac_sta = get_gw_mac_sta();
    ESP_LOGI(TAG, "Mac address: %s", gw_mac_sta.str_buf);
    wifi_init();
    ethernet_init();
    const uint32_t stack_size_for_monitoring_task = 2 * 1024;
    xTaskCreate(monitoring_task, "monitoring_task", stack_size_for_monitoring_task, NULL, 1, NULL);
    const uint32_t stack_size_for_reset_task = 2 * 1024;
    xTaskCreate(reset_task, "reset_task", stack_size_for_reset_task, NULL, 1, NULL);
    ESP_LOGI(TAG, "Main started");
}
