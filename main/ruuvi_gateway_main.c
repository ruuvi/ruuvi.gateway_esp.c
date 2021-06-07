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
#include "reset_task.h"
#include "http_server.h"
#include "fw_update.h"

#define LOG_LOCAL_LEVEL LOG_LEVEL_DEBUG
#include "log.h"

static const char TAG[] = "ruuvi_gateway";

#define MAC_ADDRESS_IDX_OF_LAST_BYTE        (MAC_ADDRESS_NUM_BYTES - 1U)
#define MAC_ADDRESS_IDX_OF_PENULTIMATE_BYTE (MAC_ADDRESS_NUM_BYTES - 2U)

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

static void
get_gw_mac_sta(
    mac_address_bin_t *const p_gw_mac_sta,
    mac_address_str_t *const p_gw_mac_sta_str,
    wifi_ssid_t *const       p_gw_wifi_ssid)
{
    const esp_err_t err = esp_read_mac(p_gw_mac_sta->mac, ESP_MAC_WIFI_STA);
    if (err != ESP_OK)
    {
        LOG_ERR_ESP(err, "Can't get mac address");
        p_gw_mac_sta_str->str_buf[0] = '\0';
        sniprintf(p_gw_wifi_ssid->ssid_buf, sizeof(p_gw_wifi_ssid->ssid_buf), "%sXXXX", DEFAULT_AP_SSID);
        return;
    }
    *p_gw_mac_sta_str = mac_address_to_str(p_gw_mac_sta);
    sniprintf(
        p_gw_wifi_ssid->ssid_buf,
        sizeof(p_gw_wifi_ssid->ssid_buf),
        "%s%02X%02X",
        DEFAULT_AP_SSID,
        p_gw_mac_sta->mac[MAC_ADDRESS_IDX_OF_PENULTIMATE_BYTE],
        p_gw_mac_sta->mac[MAC_ADDRESS_IDX_OF_LAST_BYTE]);
}

static void
ethernet_link_up_cb(void)
{
    LOG_INFO("Ethernet connection established");
    leds_indication_on_network_ok();
}

static void
ethernet_link_down_cb(void)
{
    LOG_INFO("Ethernet lost connection");
    wifi_manager_update_network_connection_info(UPDATE_LOST_CONNECTION, NULL, NULL);
    xEventGroupClearBits(status_bits, ETH_CONNECTED_BIT);
    leds_indication_network_no_connection();
    event_mgr_notify(EVENT_MGR_EV_ETH_DISCONNECTED);
}

static void
ethernet_connection_ok_cb(const tcpip_adapter_ip_info_t *p_ip_info)
{
    LOG_INFO("Ethernet connected");
    wifi_manager_update_network_connection_info(UPDATE_CONNECTION_OK, NULL, p_ip_info);
    if (!g_gateway_config.eth.use_eth)
    {
        LOG_INFO("The Ethernet cable was connected, but the Ethernet was not configured");
        LOG_INFO("Set the default configuration with Ethernet and DHCP enabled");
        g_gateway_config              = g_gateway_config_default;
        g_gateway_config.eth.use_eth  = true;
        g_gateway_config.eth.eth_dhcp = true;
        gw_cfg_print_to_log(&g_gateway_config);
        settings_save_to_flash(&g_gateway_config);
        if (!http_server_set_auth(
                g_gateway_config.lan_auth.lan_auth_type,
                g_gateway_config.lan_auth.lan_auth_user,
                g_gateway_config.lan_auth.lan_auth_pass))
        {
            LOG_ERR("%s failed", "http_server_set_auth");
        }
        if (wifi_manager_is_ap_active())
        {
            LOG_INFO("Stop WiFi AP");
            wifi_manager_stop_ap();
        }
    }
    xEventGroupSetBits(status_bits, ETH_CONNECTED_BIT);
    start_services();
    event_mgr_notify(EVENT_MGR_EV_ETH_CONNECTED);
}

void
wifi_connection_ok_cb(void *p_param)
{
    (void)p_param;
    LOG_INFO("Wifi connected");
    xEventGroupSetBits(status_bits, WIFI_CONNECTED_BIT);
    start_services();
    event_mgr_notify(EVENT_MGR_EV_WIFI_CONNECTED);
    leds_indication_on_network_ok();
}

void
wifi_connection_cb_on_connect_eth_cmd(void)
{
    LOG_INFO("callback: on_connect_eth_cmd");
    ethernet_start(g_gw_wifi_ssid.ssid_buf);
}

void
wifi_connection_cb_on_disconnect_eth_cmd(void)
{
    LOG_INFO("callback: on_disconnect_eth_cmd");
    wifi_manager_update_network_connection_info(UPDATE_USER_DISCONNECT, NULL, NULL);
    xEventGroupClearBits(status_bits, ETH_CONNECTED_BIT);
    ethernet_stop();
}

void
wifi_connection_cb_on_disconnect_sta_cmd(void)
{
    LOG_INFO("callback: on_disconnect_sta_cmd");
    xEventGroupClearBits(status_bits, WIFI_CONNECTED_BIT);
}

void
wifi_connection_cb_on_ap_sta_connected(void)
{
    LOG_INFO("callback: on_ap_sta_connected");
    reset_task_stop_timer_after_hotspot_activation();
}

void
wifi_connection_cb_on_ap_sta_disconnected(void)
{
    LOG_INFO("callback: on_ap_sta_disconnected");
    if (!wifi_manager_is_connected_to_wifi_or_ethernet())
    {
        ethernet_start(g_gw_wifi_ssid.ssid_buf);
        reset_task_start_timer_after_hotspot_activation();
    }
}

void
wifi_disconnect_cb(void *p_param)
{
    (void)p_param;
    LOG_WARN("Wifi disconnected");
    xEventGroupClearBits(status_bits, WIFI_CONNECTED_BIT);
    leds_indication_network_no_connection();
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

static bool
wifi_init(
    const bool               flag_use_eth,
    const wifi_ssid_t *const p_gw_wifi_ssid,
    const char *const        p_fatfs_gwui_partition_name)
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
    if (!http_server_cb_init(p_fatfs_gwui_partition_name))
    {
        LOG_ERR("%s failed", "http_server_cb_init");
        return false;
    }
    wifi_manager_start(
        !flag_use_eth,
        p_gw_wifi_ssid,
        &wiFiAntConfig,
        &http_server_cb_on_get,
        &http_server_cb_on_post,
        &http_server_cb_on_delete,
        &wifi_connection_cb_on_connect_eth_cmd,
        &wifi_connection_cb_on_disconnect_eth_cmd,
        &wifi_connection_cb_on_disconnect_sta_cmd,
        &wifi_connection_cb_on_ap_sta_connected,
        &wifi_connection_cb_on_ap_sta_disconnected);
    wifi_manager_set_callback(EVENT_STA_GOT_IP, &wifi_connection_ok_cb);
    wifi_manager_set_callback(EVENT_STA_DISCONNECTED, &wifi_disconnect_cb);
    return true;
}

void
app_main(void)
{
    esp_log_level_set(TAG, ESP_LOG_DEBUG);
    cjson_wrap_init();

    fw_update_read_flash_info();

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

    get_gw_mac_sta(&g_gw_mac_sta, &g_gw_mac_sta_str, &g_gw_wifi_ssid);
    LOG_INFO("Mac address: %s", g_gw_mac_sta_str.str_buf);
    LOG_INFO("WiFi SSID / Hostname: %s", g_gw_wifi_ssid.ssid_buf);

    if (0 == gpio_get_level(RB_BUTTON_RESET_PIN))
    {
        LOG_INFO("Reset button is pressed during boot - clear settings in flash");
        nrf52fw_hw_reset_nrf52(true);
        if (!wifi_manager_clear_sta_config(&g_gw_wifi_ssid))
        {
            LOG_ERR("%s failed", "wifi_manager_clear_sta_config");
        }
        settings_clear_in_flash();
        LOG_INFO("Wait until the CONFIGURE button is released");
        leds_indication_on_configure_button_press();
        while (0 == gpio_get_level(RB_BUTTON_RESET_PIN))
        {
            vTaskDelay(1);
        }
        LOG_INFO("The CONFIGURE button has been released - restart system");
        esp_restart();
    }

    leds_indication_on_nrf52_fw_updating();
    nrf52fw_update_fw_if_necessary(fw_update_get_current_fatfs_nrf52_partition_name(), NULL, NULL);
    leds_off();

    settings_get_from_flash(&g_gateway_config);
    if (!http_server_set_auth(
            g_gateway_config.lan_auth.lan_auth_type,
            g_gateway_config.lan_auth.lan_auth_user,
            g_gateway_config.lan_auth.lan_auth_pass))
    {
        LOG_ERR("%s failed", "http_server_set_auth");
    }
    adv_post_init();
    terminal_open(NULL, true);
    api_process(true);
    time_task_init();
    ruuvi_send_nrf_settings(&g_gateway_config);

    if (!wifi_init(g_gateway_config.eth.use_eth, &g_gw_wifi_ssid, fw_update_get_current_fatfs_gwui_partition_name()))
    {
        LOG_ERR("%s failed", "wifi_init");
        return;
    }
    ethernet_init(&ethernet_link_up_cb, &ethernet_link_down_cb, &ethernet_connection_ok_cb);
    if (g_gateway_config.eth.use_eth || (!wifi_manager_is_sta_configured()))
    {
        ethernet_start(g_gw_wifi_ssid.ssid_buf);
    }
    else
    {
        LOG_INFO("Gateway already configured to use WiFi connection, so Ethernet is not needed");
    }

    if (wifi_manager_is_ap_active())
    {
        leds_indication_on_hotspot_activation();
    }
    else
    {
        leds_indication_network_no_connection();
    }

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

    if (!reset_task_init())
    {
        LOG_ERR("Can't create thread");
    }
    LOG_INFO("Main started");
}
