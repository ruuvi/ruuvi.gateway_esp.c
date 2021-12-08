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
#include "esp_wifi.h"
#include "ethernet.h"
#include "os_task.h"
#include "os_malloc.h"
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
#include "hmac_sha256.h"
#include "ruuvi_device_id.h"
#include "gw_cfg.h"
#include "gw_cfg_default.h"
#include "gw_cfg_json.h"
#include "ruuvi_auth.h"
#include "lwip/dhcp.h"
#include "lwip/sockets.h"
#include "str_buf.h"
#include "wifiman_md5.h"
#include "json_ruuvi.h"

#define LOG_LOCAL_LEVEL LOG_LEVEL_DEBUG
#include "log.h"

static const char TAG[] = "ruuvi_gateway";

#define MAC_ADDRESS_IDX_OF_LAST_BYTE        (MAC_ADDRESS_NUM_BYTES - 1U)
#define MAC_ADDRESS_IDX_OF_PENULTIMATE_BYTE (MAC_ADDRESS_NUM_BYTES - 2U)

#define RUUVI_GET_NRF52_ID_NUM_RETRIES (3U)
#define RUUVI_GET_NRF52_ID_DELAY_MS    (1000U)
#define RUUVI_GET_NRF52_ID_STEP_MS     (100U)

#define WIFI_COUNTRY_DEFAULT_FIRST_CHANNEL (1U)
#define WIFI_COUNTRY_DEFAULT_NUM_CHANNELS  (13U)
#define WIFI_MAX_TX_POWER_DEFAULT          (9)

EventGroupHandle_t status_bits;
uint32_t volatile g_network_disconnect_cnt;

static inline uint8_t
conv_bool_to_u8(const bool x)
{
    return x ? (uint8_t)RE_CA_BOOL_ENABLE : (uint8_t)RE_CA_BOOL_DISABLE;
}

void
ruuvi_send_nrf_settings(void)
{
    const ruuvi_gateway_config_t *     p_gw_cfg = gw_cfg_lock_ro();
    const ruuvi_gw_cfg_filter_t *const p_filter = &p_gw_cfg->filter;
    const ruuvi_gw_cfg_scan_t *const   p_scan   = &p_gw_cfg->scan;
    LOG_INFO(
        "sending settings to NRF: use filter: %d, "
        "company id: 0x%04x,"
        "use scan coded phy: %d,"
        "use scan 1mbit/phy: %d,"
        "use scan extended payload: %d,"
        "use scan channel 37: %d,"
        "use scan channel 38: %d,"
        "use scan channel 39: %d",
        p_filter->company_use_filtering,
        p_filter->company_id,
        p_scan->scan_coded_phy,
        p_scan->scan_1mbit_phy,
        p_scan->scan_extended_payload,
        p_scan->scan_channel_37,
        p_scan->scan_channel_38,
        p_scan->scan_channel_39);

    api_send_all(
        RE_CA_UART_SET_ALL,
        p_filter->company_id,
        conv_bool_to_u8(p_filter->company_use_filtering),
        conv_bool_to_u8(p_scan->scan_coded_phy),
        conv_bool_to_u8(p_scan->scan_extended_payload),
        conv_bool_to_u8(p_scan->scan_1mbit_phy),
        conv_bool_to_u8(p_scan->scan_channel_37),
        conv_bool_to_u8(p_scan->scan_channel_38),
        conv_bool_to_u8(p_scan->scan_channel_39));
    gw_cfg_unlock_ro(&p_gw_cfg);
}

void
ruuvi_send_nrf_get_id(void)
{
    api_send_get_device_id(RE_CA_UART_GET_DEVICE_ID);
}

static void
gateway_read_mac_addr(
    const esp_mac_type_t     mac_type,
    mac_address_bin_t *const p_mac_addr_bin,
    mac_address_str_t *const p_mac_addr_str)
{
    const esp_err_t err = esp_read_mac(p_mac_addr_bin->mac, mac_type);
    if (err != ESP_OK)
    {
        LOG_ERR_ESP(err, "Can't get mac address for mac_type %d", (printf_int_t)mac_type);
        p_mac_addr_str->str_buf[0] = '\0';
    }
    else
    {
        *p_mac_addr_str = mac_address_to_str(p_mac_addr_bin);
    }
}

static void
set_gw_mac_sta(const mac_address_bin_t *const p_src, mac_address_str_t *const p_dst, wifi_ssid_t *const p_gw_wifi_ssid)
{
    *p_dst              = mac_address_to_str(p_src);
    bool flag_mac_valid = false;
    for (uint32_t i = 0; i < sizeof(p_src->mac); ++i)
    {
        if (0 != p_src->mac[i])
        {
            flag_mac_valid = true;
            break;
        }
    }
    if (!flag_mac_valid)
    {
        sniprintf(p_gw_wifi_ssid->ssid_buf, sizeof(p_gw_wifi_ssid->ssid_buf), "%sXXXX", DEFAULT_AP_SSID);
    }
    else
    {
        sniprintf(
            p_gw_wifi_ssid->ssid_buf,
            sizeof(p_gw_wifi_ssid->ssid_buf),
            "%s%02X%02X",
            DEFAULT_AP_SSID,
            p_src->mac[MAC_ADDRESS_IDX_OF_PENULTIMATE_BYTE],
            p_src->mac[MAC_ADDRESS_IDX_OF_LAST_BYTE]);
    }
}

static void
ethernet_link_up_cb(void)
{
    LOG_INFO("Ethernet connection established");
}

static void
ethernet_link_down_cb(void)
{
    LOG_INFO("Ethernet lost connection");
    g_network_disconnect_cnt += 1;
    wifi_manager_update_network_connection_info(UPDATE_LOST_CONNECTION, NULL, NULL, NULL);
    xEventGroupClearBits(status_bits, ETH_CONNECTED_BIT);
    leds_indication_network_no_connection();
    event_mgr_notify(EVENT_MGR_EV_ETH_DISCONNECTED);
}

static void
ethernet_connection_ok_cb(const tcpip_adapter_ip_info_t *p_ip_info)
{
    LOG_INFO("Ethernet connected");
    leds_indication_on_network_ok();
    if (!gw_cfg_get_eth_use_eth())
    {
        LOG_INFO("The Ethernet cable was connected, but the Ethernet was not configured");
        LOG_INFO("Set the default configuration with Ethernet and DHCP enabled");
        ruuvi_gateway_config_t *p_gw_cfg = gw_cfg_lock_rw();
        gw_cfg_default_get(p_gw_cfg);
        p_gw_cfg->eth.use_eth  = true;
        p_gw_cfg->eth.eth_dhcp = true;
        gw_cfg_print_to_log(p_gw_cfg);

        cjson_wrap_str_t cjson_str = { 0 };
        if (!gw_cfg_json_generate(p_gw_cfg, &cjson_str))
        {
            LOG_ERR("%s failed", "gw_cfg_json_generate");
        }
        else
        {
            settings_save_to_flash(cjson_str.p_str);
        }
        cjson_wrap_free_json_str(&cjson_str);
        gw_cfg_unlock_rw(&p_gw_cfg);

        if (!ruuvi_auth_set_from_config())
        {
            LOG_ERR("%s failed", "gw_cfg_set_auth_from_config");
        }
    }
    const ip4_addr_t *p_dhcp_ip = NULL;
    if (gw_cfg_get_eth_use_dhcp())
    {
        struct netif *p_netif = NULL;
        esp_err_t     err     = tcpip_adapter_get_netif(TCPIP_ADAPTER_IF_ETH, (void **)&p_netif);
        if (ESP_OK != err)
        {
            LOG_ERR_ESP(err, "%s failed", "tcpip_adapter_get_netif");
        }
        else
        {
            const struct dhcp *const p_dhcp = netif_dhcp_data(p_netif);
            if (NULL != p_dhcp)
            {
                p_dhcp_ip = &p_dhcp->server_ip_addr.u_addr.ip4;
            }
        }
    }
    wifi_manager_update_network_connection_info(UPDATE_CONNECTION_OK, NULL, p_ip_info, p_dhcp_ip);
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

static void
cb_on_connect_eth_cmd(void)
{
    LOG_INFO("callback: on_connect_eth_cmd");
    ethernet_start(g_gw_wifi_ssid.ssid_buf);
}

static void
cb_on_disconnect_eth_cmd(void)
{
    LOG_INFO("callback: on_disconnect_eth_cmd");
    wifi_manager_update_network_connection_info(UPDATE_USER_DISCONNECT, NULL, NULL, NULL);
    xEventGroupClearBits(status_bits, ETH_CONNECTED_BIT);
    ethernet_stop();
}

static void
cb_on_disconnect_sta_cmd(void)
{
    LOG_INFO("callback: on_disconnect_sta_cmd");
    xEventGroupClearBits(status_bits, WIFI_CONNECTED_BIT);
}

static void
cb_on_ap_sta_connected(void)
{
    LOG_INFO("callback: on_ap_sta_connected");
    main_task_stop_timer_after_hotspot_activation();
}

static void
cb_on_ap_sta_disconnected(void)
{
    LOG_INFO("callback: on_ap_sta_disconnected");
    if (!wifi_manager_is_connected_to_wifi_or_ethernet())
    {
        if (gw_cfg_get_eth_use_eth() || (!wifi_manager_is_sta_configured()))
        {
            ethernet_start(g_gw_wifi_ssid.ssid_buf);
        }
        main_task_start_timer_after_hotspot_activation();
    }
    adv_post_enable_retransmission();
}

void
wifi_disconnect_cb(void *p_param)
{
    (void)p_param;
    LOG_WARN("Wifi disconnected");
    g_network_disconnect_cnt += 1;
    xEventGroupClearBits(status_bits, WIFI_CONNECTED_BIT);
    leds_indication_network_no_connection();
    event_mgr_notify(EVENT_MGR_EV_WIFI_DISCONNECTED);
}

void
start_services(void)
{
    LOG_INFO("Start services");
    if (gw_cfg_get_mqtt_use_mqtt())
    {
        mqtt_app_start();
    }
}

void
restart_services(void)
{
    main_task_send_sig_restart_services();

    if (AUTO_UPDATE_CYCLE_TYPE_MANUAL != gw_cfg_get_auto_update_cycle())
    {
        const os_delta_ticks_t delay_ticks = pdMS_TO_TICKS(RUUVI_CHECK_FOR_FW_UPDATES_DELAY_BEFORE_RETRY_SECONDS)
                                             * 1000;
        LOG_INFO(
            "Restarting services: Restart firmware auto-updating, run next check after %lu seconds",
            (printf_ulong_t)RUUVI_CHECK_FOR_FW_UPDATES_DELAY_AFTER_REBOOT_SECONDS);
        main_task_timer_sig_check_for_fw_updates_restart(delay_ticks);
    }
    else
    {
        LOG_INFO("Restarting services: Stop firmware auto-updating");
        main_task_timer_sig_check_for_fw_updates_stop();
    }
}

static bool
wifi_init(
    const bool               flag_use_eth,
    const bool               flag_start_ap_only,
    const wifi_ssid_t *const p_gw_wifi_ssid,
    const char *const        p_fatfs_gwui_partition_name)
{
    static const wifi_manager_antenna_config_t wifi_antenna_config = {
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
    const wifi_manager_callbacks_t wifi_callbacks = {
        .cb_on_http_user_req       = &http_server_cb_on_user_req,
        .cb_on_http_get            = &http_server_cb_on_get,
        .cb_on_http_post           = &http_server_cb_on_post,
        .cb_on_http_delete         = &http_server_cb_on_delete,
        .cb_on_connect_eth_cmd     = &cb_on_connect_eth_cmd,
        .cb_on_disconnect_eth_cmd  = &cb_on_disconnect_eth_cmd,
        .cb_on_disconnect_sta_cmd  = &cb_on_disconnect_sta_cmd,
        .cb_on_ap_sta_connected    = &cb_on_ap_sta_connected,
        .cb_on_ap_sta_disconnected = &cb_on_ap_sta_disconnected,
    };
    wifi_manager_start(!flag_use_eth, flag_start_ap_only, p_gw_wifi_ssid, &wifi_antenna_config, &wifi_callbacks);
    wifi_manager_set_callback(EVENT_STA_GOT_IP, &wifi_connection_ok_cb);
    wifi_manager_set_callback(EVENT_STA_DISCONNECTED, &wifi_disconnect_cb);
    return true;
}

static void
request_and_wait_nrf52_id(void)
{
    const uint32_t delay_ms = RUUVI_GET_NRF52_ID_DELAY_MS;
    const uint32_t step_ms  = RUUVI_GET_NRF52_ID_STEP_MS;
    for (uint32_t i = 0; i < RUUVI_GET_NRF52_ID_NUM_RETRIES; ++i)
    {
        if (ruuvi_device_id_is_set())
        {
            break;
        }
        LOG_INFO("Request nRF52 ID");
        ruuvi_send_nrf_get_id();

        for (uint32_t j = 0; j < (delay_ms / step_ms); ++j)
        {
            vTaskDelay(step_ms / portTICK_PERIOD_MS);
            if (ruuvi_device_id_is_set())
            {
                break;
            }
        }
    }
    if (!ruuvi_device_id_is_set())
    {
        LOG_ERR("Failed to read nRF52 DEVICE ID");
    }
    else
    {
        const nrf52_device_id_str_t nrf52_device_id_str = ruuvi_device_id_get_str();
        LOG_INFO("nRF52 DEVICE ID : %s", nrf52_device_id_str.str_buf);

        const mac_address_str_t nrf52_mac_addr_str = ruuvi_device_id_get_nrf52_mac_address_str();
        LOG_INFO("nRF52 ADDR      : %s", nrf52_mac_addr_str.str_buf);

        hmac_sha256_set_key_str(nrf52_device_id_str.str_buf); // set default encryption key
    }
}

static void
cb_before_nrf52_fw_updating(void)
{
    fw_update_set_stage_nrf52_updating();

    // Here we do not yet know the value of nRF52 DeviceID, so we cannot use it as the default password.
    http_server_cb_prohibit_cfg_updating();
    if (!http_server_set_auth(HTTP_SERVER_AUTH_TYPE_STR_ALLOW, "", ""))
    {
        LOG_ERR("%s failed", "http_server_set_auth");
    }

    if (!wifi_init(false, true, &g_gw_wifi_ssid, fw_update_get_current_fatfs_gwui_partition_name()))
    {
        LOG_ERR("%s failed", "wifi_init");
        return;
    }
}

static void
cb_after_nrf52_fw_updating(void)
{
    fw_update_set_extra_info_for_status_json_update_successful();
    vTaskDelay(pdMS_TO_TICKS(5 * 1000));
    LOG_INFO("nRF52 firmware has been successfully updated - restart system");
    esp_restart();
}

static void
ruuvi_nvs_flash_init(void)
{
    LOG_INFO("Init NVS");
    const esp_err_t err = nvs_flash_init();
    if (ESP_OK != err)
    {
        LOG_ERR_ESP(err, "nvs_flash_init failed");
    }
}

static void
ruuvi_nvs_flash_deinit(void)
{
    LOG_INFO("Deinit NVS");
    const esp_err_t err = nvs_flash_deinit();
    if (ESP_OK != err)
    {
        LOG_ERR_ESP(err, "nvs_flash_deinit failed");
    }
}

static void
ruuvi_nvs_flash_erase(void)
{
    LOG_INFO("Erase NVS");
    const esp_err_t err = nvs_flash_erase();
    if (ESP_OK != err)
    {
        LOG_ERR_ESP(err, "nvs_flash_erase failed");
    }
}

ATTR_NORETURN
static void
handle_reset_button_is_pressed_during_boot(void)
{
    LOG_INFO("Reset button is pressed during boot - clear settings in flash");
    nrf52fw_hw_reset_nrf52(true);

    ruuvi_nvs_flash_erase();
    ruuvi_nvs_flash_init();

    LOG_INFO("Writing the default wifi-manager configuration to NVS");
    if (!wifi_manager_clear_sta_config(&g_gw_wifi_ssid))
    {
        LOG_ERR("Failed to clear the wifi-manager settings in NVS");
    }

    LOG_INFO("Writing the default gateway configuration to NVS");
    if (!settings_clear_in_flash())
    {
        LOG_ERR("Failed to clear the gateway settings in NVS");
    }
    settings_write_flag_rebooting_after_auto_update(false);
    settings_write_flag_force_start_wifi_hotspot(true);

    LOG_INFO("Wait until the CONFIGURE button is released");
    leds_indication_on_configure_button_press();
    while (0 == gpio_get_level(RB_BUTTON_RESET_PIN))
    {
        vTaskDelay(1);
    }
    LOG_INFO("The CONFIGURE button has been released - restart system");
    esp_restart();
}

static void
read_wifi_country_and_print_to_log(const char *const p_msg_prefix)
{
    wifi_country_t  country = { 0 };
    const esp_err_t err     = esp_wifi_get_country(&country);
    if (ESP_OK != err)
    {
        LOG_ERR_ESP(err, "%s failed", "esp_wifi_get_country");
    }
    else
    {
        LOG_INFO(
            "%s: CC=%.3s, max_tx_power=%d dBm, schan=%u, nchan=%u, policy=%s",
            p_msg_prefix,
            country.cc,
            (printf_int_t)country.max_tx_power,
            (printf_uint_t)country.schan,
            (printf_uint_t)country.nchan,
            country.policy ? "MANUAL" : "AUTO");
    }
}

static void
set_wifi_country(void)
{
    wifi_country_t country = {
        .cc           = "FI",                               /**< country code string */
        .schan        = WIFI_COUNTRY_DEFAULT_FIRST_CHANNEL, /**< start channel */
        .nchan        = WIFI_COUNTRY_DEFAULT_NUM_CHANNELS,  /**< total channel number */
        .max_tx_power = WIFI_MAX_TX_POWER_DEFAULT, /**< This field is used for getting WiFi maximum transmitting power,
                                            call esp_wifi_set_max_tx_power to set the maximum transmitting power. */
        .policy = WIFI_COUNTRY_POLICY_AUTO,        /**< country policy */
    };
    const esp_err_t err = esp_wifi_set_country(&country);
    if (ESP_OK != err)
    {
        LOG_ERR_ESP(err, "%s failed", "esp_wifi_get_country");
    }
}

static void
read_wifi_max_tx_power_and_print_to_log(const char *const p_msg_prefix)
{
    int8_t          power = 0;
    const esp_err_t err   = esp_wifi_get_max_tx_power(&power);
    if (ESP_OK != err)
    {
        LOG_ERR_ESP(err, "%s failed", "esp_wifi_get_max_tx_power");
    }
    else
    {
        LOG_INFO("%s: %d (%d dBm)", p_msg_prefix, (printf_int_t)power, (printf_int_t)(power / 4));
    }
}

static void
set_wifi_max_tx_power(void)
{
    const esp_err_t err = esp_wifi_set_max_tx_power(9 /*dbB*/ * 4);
    if (ESP_OK != err)
    {
        LOG_ERR_ESP(err, "%s failed", "esp_wifi_set_max_tx_power");
    }
}

static void
configure_wifi_country_and_max_tx_power(void)
{
    read_wifi_country_and_print_to_log("WiFi country after wifi_init");
    set_wifi_country();
    read_wifi_country_and_print_to_log("WiFi country after esp_wifi_set_country");

    read_wifi_max_tx_power_and_print_to_log("Max WiFi TX power after wifi_init");
    set_wifi_max_tx_power();
    read_wifi_max_tx_power_and_print_to_log("Max WiFi TX power after esp_wifi_set_max_tx_power");
}

static bool
network_subsystem_init(void)
{
    if (!wifi_init(
            gw_cfg_get_eth_use_eth(),
            settings_read_flag_force_start_wifi_hotspot(),
            &g_gw_wifi_ssid,
            fw_update_get_current_fatfs_gwui_partition_name()))
    {
        LOG_ERR("%s failed", "wifi_init");
        return false;
    }
    configure_wifi_country_and_max_tx_power();
    ethernet_init(&ethernet_link_up_cb, &ethernet_link_down_cb, &ethernet_connection_ok_cb);

    if (settings_read_flag_force_start_wifi_hotspot())
    {
        LOG_INFO("Force start WiFi hotspot");
        settings_write_flag_force_start_wifi_hotspot(false);
        if (!wifi_manager_is_ap_active())
        {
            wifi_manager_start_ap();
        }
        else
        {
            LOG_INFO("WiFi hotspot is already active");
        }
        main_task_start_timer_after_hotspot_activation();
    }
    else
    {
        if (gw_cfg_get_eth_use_eth() || (!wifi_manager_is_sta_configured()))
        {
            ethernet_start(g_gw_wifi_ssid.ssid_buf);
        }
        else
        {
            LOG_INFO("Gateway already configured to use WiFi connection, so Ethernet is not needed");
        }
    }

    if (wifi_manager_is_ap_active())
    {
        leds_indication_on_hotspot_activation();
    }
    else
    {
        leds_indication_network_no_connection();
    }
    return true;
}

static const char *
generate_default_lan_auth_password(void)
{
    const nrf52_device_id_str_t device_id = ruuvi_device_id_get_str();

    str_buf_t str_buf = str_buf_printf_with_alloc(
        "%s:%s:%s",
        RUUVI_GATEWAY_AUTH_DEFAULT_USER,
        g_gw_wifi_ssid.ssid_buf,
        device_id.str_buf);
    if (0 == str_buf_get_len(&str_buf))
    {
        return NULL;
    }

    const wifiman_md5_digest_hex_str_t password_md5 = wifiman_md5_calc_hex_str(str_buf.buf, str_buf_get_len(&str_buf));

    str_buf_free_buf(&str_buf);
    const size_t password_md5_len   = strlen(password_md5.buf);
    char *const  p_password_md5_str = os_malloc(password_md5_len + 1);
    if (NULL == p_password_md5_str)
    {
        return NULL;
    }
    snprintf(p_password_md5_str, password_md5_len + 1, "%s", password_md5.buf);
    return p_password_md5_str;
}

static bool
main_task_init(void)
{
    esp_log_level_set(TAG, ESP_LOG_DEBUG);
    cjson_wrap_init();

    if (!main_loop_init())
    {
        LOG_ERR("%s failed", "main_loop_init");
        return false;
    }

    if (!fw_update_read_flash_info())
    {
        LOG_ERR("%s failed", "fw_update_read_flash_info");
        return false;
    }

    if (!event_mgr_init())
    {
        LOG_ERR("%s failed", "event_mgr_init");
        return false;
    }

    status_bits = xEventGroupCreate();
    if (NULL == status_bits)
    {
        LOG_ERR("Can't create event group");
        return false;
    }

    gpio_init();
    leds_init();

    if (0 == gpio_get_level(RB_BUTTON_RESET_PIN))
    {
        handle_reset_button_is_pressed_during_boot();
    }

    ruuvi_nvs_flash_init();

    if ((!wifi_manager_check_sta_config()) || (!settings_check_in_flash()))
    {
        ruuvi_nvs_flash_deinit();
        ruuvi_nvs_flash_erase();
        ruuvi_nvs_flash_init();
    }

    gateway_read_mac_addr(ESP_MAC_WIFI_STA, &g_gw_mac_wifi, &g_gw_mac_wifi_str);
    gateway_read_mac_addr(ESP_MAC_ETH, &g_gw_mac_eth, &g_gw_mac_eth_str);

    mac_address_bin_t nrf52_mac_addr = settings_read_mac_addr();
    set_gw_mac_sta(&nrf52_mac_addr, &g_gw_mac_sta_str, &g_gw_wifi_ssid);
    LOG_INFO("Read saved Mac address: %s", g_gw_mac_sta_str.str_buf);
    LOG_INFO("Read saved WiFi SSID / Hostname: %s", g_gw_wifi_ssid.ssid_buf);

    leds_indication_on_nrf52_fw_updating();
    nrf52fw_update_fw_if_necessary(
        fw_update_get_current_fatfs_nrf52_partition_name(),
        &fw_update_nrf52fw_cb_progress,
        NULL,
        &cb_before_nrf52_fw_updating,
        &cb_after_nrf52_fw_updating);
    leds_indication_network_no_connection();

    adv_post_init();
    terminal_open(NULL, true);
    api_process(true);
    request_and_wait_nrf52_id();

    nrf52_mac_addr = ruuvi_device_id_get_nrf52_mac_address();
    set_gw_mac_sta(&nrf52_mac_addr, &g_gw_mac_sta_str, &g_gw_wifi_ssid);
    LOG_INFO("Mac address: %s", g_gw_mac_sta_str.str_buf);
    LOG_INFO("WiFi SSID / Hostname: %s", g_gw_wifi_ssid.ssid_buf);

    settings_update_mac_addr(&nrf52_mac_addr);

    if (!gw_cfg_default_set_lan_auth_password(generate_default_lan_auth_password()))
    {
        LOG_ERR("%s failed", "generate_default_lan_auth_password");
        return false;
    }
    gw_cfg_init();
    settings_get_from_flash();

    time_task_init();
    ruuvi_send_nrf_settings();
    ruuvi_auth_set_from_config();

    if (!network_subsystem_init())
    {
        LOG_ERR("%s failed", "network_subsystem_init");
        return false;
    }

    if (!reset_task_init())
    {
        LOG_ERR("Can't create thread");
        return false;
    }

    return true;
}

ATTR_NORETURN
void
app_main(void)
{
    if (!main_task_init())
    {
        LOG_ERR("main_task_init failed - try to rollback firmware");
        const esp_err_t err = esp_ota_mark_app_invalid_rollback_and_reboot();
        if (0 != err)
        {
            LOG_ERR_ESP(err, "%s failed", "esp_ota_mark_app_invalid_rollback_and_reboot");
        }
        for (;;)
        {
            vTaskDelay(1);
        }
    }
    else
    {
        if (settings_read_flag_rebooting_after_auto_update())
        {
            // If rebooting after auto firmware update, fw_update_mark_app_valid_cancel_rollback should be called by
            // http_send_advs or mqtt_event_handler after successful network communication with the server.
            settings_write_flag_rebooting_after_auto_update(false);
        }
        else
        {
            if (!fw_update_mark_app_valid_cancel_rollback())
            {
                LOG_ERR("%s failed", "fw_update_mark_app_valid_cancel_rollback");
            }
        }
        main_loop();
    }
}

/*
 * This function is called by task_wdt_isr function (ISR for when TWDT times out).
 * Note: It has the same limitations as the interrupt function.
 *       Do not use ESP_LOGI functions inside.
 */
void
esp_task_wdt_isr_user_handler(void)
{
    esp_restart();
}
