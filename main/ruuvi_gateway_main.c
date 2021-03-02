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
#include "driver/gpio.h"
#include "esp_system.h"
#include "ethernet.h"
#include "os_task.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "gpio.h"
#include "leds.h"
#include "mqtt.h"
#include "nvs.h"
#include "nvs_flash.h"
#include "ruuvi_boards.h"
#include "terminal.h"
#include "time_task.h"
#include "wifi_manager.h"
#include "ruuvi_endpoint_ca_uart.h"
#include "nrf52fw.h"
#include "attribs.h"
#include "http_server_cb.h"
#include "event_mgr.h"
#include "cjson_wrap.h"

#define LOG_LOCAL_LEVEL LOG_LEVEL_DEBUG
#include "log.h"

static const char TAG[] = "ruuvi_gateway";

EventGroupHandle_t status_bits;

static inline uint8_t
conv_bool_to_u8(const bool x)
{
    return x ? (uint8_t)RE_CA_BOOL_ENABLE : (uint8_t)RE_CA_BOOL_DISABLE;
}

void
ruuvi_send_nrf_settings(const ruuvi_gateway_config_t *p_config)
{
    LOG_INFO(
        "sending settings to NRF: use filter: %d, "
        "company id: 0x%04x,"
        "use scan coded phy: %d,"
        "use scan 1mbit/phy: %d,"
        "use scan extended payload: %d,"
        "use scan channel 37: %d,"
        "use scan channel 38: %d,"
        "use scan channel 39: %d",
        p_config->filter.company_filter,
        p_config->filter.company_id,
        p_config->scan.scan_coded_phy,
        p_config->scan.scan_1mbit_phy,
        p_config->scan.scan_extended_payload,
        p_config->scan.scan_channel_37,
        p_config->scan.scan_channel_38,
        p_config->scan.scan_channel_39);

    api_send_all(
        RE_CA_UART_SET_ALL,
        p_config->filter.company_id,
        conv_bool_to_u8(p_config->filter.company_filter),
        conv_bool_to_u8(p_config->scan.scan_coded_phy),
        conv_bool_to_u8(p_config->scan.scan_extended_payload),
        conv_bool_to_u8(p_config->scan.scan_1mbit_phy),
        conv_bool_to_u8(p_config->scan.scan_channel_37),
        conv_bool_to_u8(p_config->scan.scan_channel_38),
        conv_bool_to_u8(p_config->scan.scan_channel_39));
}

void
ruuvi_send_nrf_get_id(void)
{
    api_send_get_device_id(RE_CA_UART_GET_DEVICE_ID);
}

ATTR_NORETURN
static void
monitoring_task(void)
{
    for (;;)
    {
        LOG_INFO("free heap: %u", esp_get_free_heap_size());
        vTaskDelay(pdMS_TO_TICKS(10000));
    }
}

mac_address_str_t
get_gw_mac_sta(void)
{
    mac_address_bin_t mac = { 0 };
    const esp_err_t   err = esp_read_mac(mac.mac, ESP_MAC_WIFI_STA);
    if (err != ESP_OK)
    {
        LOG_ERR_ESP(err, "Can't get mac address");
        mac_address_str_t mac_str = { 0 };
        return mac_str;
    }
    return mac_address_to_str(&mac);
}

void
wifi_connection_ok_cb(void *p_param)
{
    (void)p_param;
    LOG_INFO("Wifi connected");
    xEventGroupSetBits(status_bits, WIFI_CONNECTED_BIT);
    ethernet_deinit();
    leds_stop_blink();
    leds_on();
    start_services();
    event_mgr_notify(EVENT_MGR_EV_WIFI_CONNECTED);
}

void
ethernet_link_up_cb(void)
{
    LOG_INFO("Ethernet connection established");
}

void
ethernet_link_down_cb(void)
{
    LOG_INFO("Ethernet lost connection");
    xEventGroupClearBits(status_bits, ETH_CONNECTED_BIT);
    leds_stop_blink();
    leds_start_blink(LEDS_SLOW_BLINK);
    event_mgr_notify(EVENT_MGR_EV_ETH_DISCONNECTED);
}

void
ethernet_connection_ok_cb(void)
{
    LOG_INFO("Ethernet connected");
    if (wifi_manager_is_working())
    {
        wifi_manager_stop();
    }
    if (!g_gateway_config.eth.use_eth)
    {
        LOG_INFO("The Ethernet cable was connected, but the Ethernet was not configured");
        LOG_INFO("Set the default configuration with Ethernet and DHCP enabled");
        g_gateway_config              = g_gateway_config_default;
        g_gateway_config.eth.use_eth  = true;
        g_gateway_config.eth.eth_dhcp = true;
        gw_cfg_print_to_log(&g_gateway_config);
        settings_save_to_flash(&g_gateway_config);
    }
    leds_stop_blink();
    leds_on();
    xEventGroupSetBits(status_bits, ETH_CONNECTED_BIT);
    start_services();
    event_mgr_notify(EVENT_MGR_EV_ETH_CONNECTED);
}

void
wifi_disconnect_cb(void *p_param)
{
    (void)p_param;
    LOG_WARN("Wifi disconnected");
    xEventGroupClearBits(status_bits, WIFI_CONNECTED_BIT);
    leds_stop_blink();
    leds_start_blink(LEDS_SLOW_BLINK);
    event_mgr_notify(EVENT_MGR_EV_WIFI_DISCONNECTED);
}

void
start_services(void)
{
    if (g_gateway_config.mqtt.use_mqtt)
    {
        mqtt_app_start();
    }
}

ATTR_NORETURN
static void
reset_task(void)
{
    LOG_INFO("Reset task started");

    while (1)
    {
        const EventBits_t bits = xEventGroupWaitBits(
            status_bits,
            RESET_BUTTON_BIT,
            pdTRUE,
            pdFALSE,
            pdMS_TO_TICKS(1000));
        if (0 != (bits & RESET_BUTTON_BIT))
        {
            LOG_INFO("Reset activated");
            // restart the Gateway,
            // on boot it will check if RB_BUTTON_RESET_PIN is pressed and erase the
            // settings in flash.
            esp_restart();
        }
    }
}

static bool
wifi_init(void)
{
    static const WiFiAntConfig_t wiFiAntConfig = {
        .wifi_ant_gpio_config = {
            .gpio_cfg = {
                [0] = {
                    .gpio_select = 1,
                    .gpio_num = RB_GWBUS_LNA,
                },
                [1] = {
                    .gpio_select = 0,
                    .gpio_num = 0,
                },
                [2] = {
                    .gpio_select = 0,
                    .gpio_num = 0,
                },
                [3] = {
                    .gpio_select = 0,
                    .gpio_num = 0,
                },
            },
        },
        .wifi_ant_config = {
            .rx_ant_mode = WIFI_ANT_MODE_ANT1,
            .rx_ant_default = WIFI_ANT_ANT1,
            .tx_ant_mode = WIFI_ANT_MODE_ANT0,
            .enabled_ant0 = 0,
            .enabled_ant1 = 1,
        },
    };
    if (!http_server_cb_init())
    {
        LOG_ERR("%s failed", "http_server_cb_init");
        return false;
    }
    wifi_manager_start(&wiFiAntConfig, &http_server_cb_on_get, &http_server_cb_on_post, &http_server_cb_on_delete);
    wifi_manager_set_callback(EVENT_STA_GOT_IP, &wifi_connection_ok_cb);
    wifi_manager_set_callback(EVENT_STA_DISCONNECTED, &wifi_disconnect_cb);
    return true;
}

void
app_main(void)
{
    esp_log_level_set(TAG, ESP_LOG_DEBUG);
    cjson_wrap_init();

    if (!event_mgr_init())
    {
        LOG_ERR("%s failed", "event_mgr_init");
    }

    status_bits = xEventGroupCreate();

    if (NULL == status_bits)
    {
        LOG_ERR("Can't create event group");
    }

    nvs_flash_init();
    gpio_init();
    leds_init();

    if (0 == gpio_get_level(RB_BUTTON_RESET_PIN))
    {
        LOG_INFO("Reset button is pressed during boot - clear settings in flash");
        if (!wifi_manager_clear_sta_config())
        {
            LOG_ERR("%s failed", "wifi_manager_clear_sta_config");
        }
        settings_clear_in_flash();
        LOG_INFO("Wait until the reset button is released");
        leds_start_blink(LEDS_MEDIUM_BLINK);
        while (0 == gpio_get_level(RB_BUTTON_RESET_PIN))
        {
            vTaskDelay(1);
        }
        LOG_INFO("Reset activated");
        esp_restart();
    }

    nrf52fw_update_fw_if_necessary();

    settings_get_from_flash(&g_gateway_config);
    adv_post_init();
    terminal_open(NULL, true);
    api_process(1);
    time_task_init();
    leds_start_blink(LEDS_FAST_BLINK);
    ruuvi_send_nrf_settings(&g_gateway_config);
    gw_mac_sta = get_gw_mac_sta();
    LOG_INFO("Mac address: %s", gw_mac_sta.str_buf);
    if (!wifi_init())
    {
        LOG_ERR("%s failed", "wifi_init");
        return;
    }
    ethernet_init();
    const uint32_t   stack_size_for_monitoring_task = 2 * 1024;
    os_task_handle_t ph_task_monitoring             = NULL;
    if (!os_task_create_without_param(
            &monitoring_task,
            "monitoring_task",
            stack_size_for_monitoring_task,
            1,
            &ph_task_monitoring))
    {
        LOG_ERR("Can't create thread");
    }
    const uint32_t   stack_size_for_reset_task = 2 * 1024;
    os_task_handle_t ph_task_reset             = NULL;
    if (!os_task_create_without_param(&reset_task, "reset_task", stack_size_for_reset_task, 1, &ph_task_reset))
    {
        LOG_ERR("Can't create thread");
    }
    LOG_INFO("Main started");
}
