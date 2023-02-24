/**
 * @file ruuvi_gateway_main.c
 * @author Jukka Saari
 * @date 2019-11-27
 * @copyright Ruuvi Innovations Ltd, license BSD-3-Clause.
 */

#include "ruuvi_gateway.h"
#include <esp_task_wdt.h>
#include <driver/gpio.h>
#include <esp_system.h>
#include <esp_wifi.h>
#include <esp_netif_net_stack.h>
#include "mbedtls/ctr_drbg.h"
#include "mbedtls/entropy.h"
#include "freertos/FreeRTOS.h"
#include "lwip/dhcp.h"
#include "lwip/sockets.h"
#include "adv_post.h"
#include "api.h"
#include "cJSON.h"
#include "ethernet.h"
#include "os_task.h"
#include "os_malloc.h"
#include "gpio.h"
#include "leds.h"
#include "mqtt.h"
#include "ruuvi_nvs.h"
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
#include "gw_cfg_default_json.h"
#include "gw_cfg_log.h"
#include "str_buf.h"
#include "json_ruuvi.h"
#include "gw_mac.h"
#include "gw_status.h"
#include "reset_info.h"

#define LOG_LOCAL_LEVEL LOG_LEVEL_INFO
#include "log.h"

static const char TAG[] = "ruuvi_gateway";

#define RUUVI_GATEWAY_WIFI_AP_PREFIX  "Configure Ruuvi Gateway "
#define RUUVI_GATEWAY_HOSTNAME_PREFIX "RuuviGateway"

#define MAC_ADDRESS_IDX_OF_LAST_BYTE        (MAC_ADDRESS_NUM_BYTES - 1U)
#define MAC_ADDRESS_IDX_OF_PENULTIMATE_BYTE (MAC_ADDRESS_NUM_BYTES - 2U)

#define WIFI_COUNTRY_DEFAULT_FIRST_CHANNEL (1U)
#define WIFI_COUNTRY_DEFAULT_NUM_CHANNELS  (13U)
#define WIFI_MAX_TX_POWER_DEFAULT          (9)

#define NRF52_COMM_TASK_PRIORITY (7)

#define RUUVI_GATEWAY_DELAY_AFTER_NRF52_UPDATING_SECONDS (5)

uint32_t volatile g_network_disconnect_cnt;

static mbedtls_entropy_context  g_entropy;
static mbedtls_ctr_drbg_context g_ctr_drbg;

static void
ruuvi_cb_on_change_cfg(const gw_cfg_t* const p_gw_cfg);

static void
configure_wifi_country_and_max_tx_power(void);

static inline uint8_t
conv_bool_to_u8(const bool x)
{
    return x ? (uint8_t)RE_CA_BOOL_ENABLE : (uint8_t)RE_CA_BOOL_DISABLE;
}

void
ruuvi_send_nrf_settings(void)
{
    const gw_cfg_t*                    p_gw_cfg   = gw_cfg_lock_ro();
    const ruuvi_gw_cfg_filter_t* const p_filter   = &p_gw_cfg->ruuvi_cfg.filter;
    const ruuvi_gw_cfg_scan_t* const   p_scan     = &p_gw_cfg->ruuvi_cfg.scan;
    const uint16_t                     company_id = p_filter->company_id;
    LOG_INFO(
        "### "
        "sending settings to NRF: use filter: %d, "
        "company id: 0x%04x,"
        "use scan coded phy: %d,"
        "use scan 1mbit/phy: %d,"
        "use scan extended payload: %d,"
        "use scan channel 37: %d,"
        "use scan channel 38: %d,"
        "use scan channel 39: %d",
        p_filter->company_use_filtering,
        company_id,
        p_scan->scan_coded_phy,
        p_scan->scan_1mbit_phy,
        p_scan->scan_extended_payload,
        p_scan->scan_channel_37,
        p_scan->scan_channel_38,
        p_scan->scan_channel_39);

    api_send_all(
        RE_CA_UART_SET_ALL,
        company_id,
        conv_bool_to_u8(p_filter->company_use_filtering),
        conv_bool_to_u8(p_scan->scan_coded_phy),
        conv_bool_to_u8(p_scan->scan_extended_payload),
        conv_bool_to_u8(p_scan->scan_1mbit_phy),
        conv_bool_to_u8(p_scan->scan_channel_37),
        conv_bool_to_u8(p_scan->scan_channel_38),
        conv_bool_to_u8(p_scan->scan_channel_39));
    gw_cfg_unlock_ro(&p_gw_cfg);
}

static mac_address_bin_t
gateway_read_mac_addr(const esp_mac_type_t mac_type)
{
    mac_address_bin_t mac_addr = { 0 };
    const esp_err_t   err      = esp_read_mac(mac_addr.mac, mac_type);
    if (err != ESP_OK)
    {
        LOG_ERR_ESP(err, "Can't get mac address for mac_type %d", (printf_int_t)mac_type);
    }
    return mac_addr;
}

static wifiman_wifi_ssid_t
generate_wifi_ap_ssid(const mac_address_bin_t mac_addr)
{
    wifiman_wifi_ssid_t wifi_ap_ssid   = { 0 };
    bool                flag_mac_valid = false;
    for (uint32_t i = 0; i < sizeof(mac_addr.mac); ++i)
    {
        if (0 != mac_addr.mac[i])
        {
            flag_mac_valid = true;
            break;
        }
    }

    if (!flag_mac_valid)
    {
        sniprintf(wifi_ap_ssid.ssid_buf, sizeof(wifi_ap_ssid.ssid_buf), "%sXXXX", RUUVI_GATEWAY_WIFI_AP_PREFIX);
    }
    else
    {
        sniprintf(
            wifi_ap_ssid.ssid_buf,
            sizeof(wifi_ap_ssid.ssid_buf),
            "%s%02X%02X",
            RUUVI_GATEWAY_WIFI_AP_PREFIX,
            mac_addr.mac[MAC_ADDRESS_IDX_OF_PENULTIMATE_BYTE],
            mac_addr.mac[MAC_ADDRESS_IDX_OF_LAST_BYTE]);
    }
    return wifi_ap_ssid;
}

static wifiman_hostname_t
generate_hostname(const mac_address_bin_t mac_addr)
{
    wifiman_hostname_t hostname       = { 0 };
    bool               flag_mac_valid = false;
    for (uint32_t i = 0; i < sizeof(mac_addr.mac); ++i)
    {
        if (0 != mac_addr.mac[i])
        {
            flag_mac_valid = true;
            break;
        }
    }

    if (!flag_mac_valid)
    {
        sniprintf(hostname.hostname_buf, sizeof(hostname.hostname_buf), "%sXXXX", RUUVI_GATEWAY_HOSTNAME_PREFIX);
    }
    else
    {
        sniprintf(
            hostname.hostname_buf,
            sizeof(hostname.hostname_buf),
            "%s%02X%02X",
            RUUVI_GATEWAY_HOSTNAME_PREFIX,
            mac_addr.mac[MAC_ADDRESS_IDX_OF_PENULTIMATE_BYTE],
            mac_addr.mac[MAC_ADDRESS_IDX_OF_LAST_BYTE]);
    }
    return hostname;
}

static void
ruuvi_init_gw_cfg(
    const ruuvi_nrf52_fw_ver_t* const p_nrf52_fw_ver,
    const nrf52_device_info_t* const  p_nrf52_device_info)
{
    const nrf52_device_id_t nrf52_device_id = (NULL != p_nrf52_device_info) ? p_nrf52_device_info->nrf52_device_id
                                                                            : (nrf52_device_id_t) { 0 };
    const mac_address_bin_t nrf52_mac_addr  = (NULL != p_nrf52_device_info) ? p_nrf52_device_info->nrf52_mac_addr
                                                                            : settings_read_mac_addr();

    const gw_cfg_default_init_param_t gw_cfg_default_init_param = {
        .wifi_ap_ssid        = generate_wifi_ap_ssid(nrf52_mac_addr),
        .hostname            = generate_hostname(nrf52_mac_addr),
        .device_id           = nrf52_device_id,
        .esp32_fw_ver        = fw_update_get_cur_version(),
        .nrf52_fw_ver        = nrf52_fw_ver_get_str(p_nrf52_fw_ver),
        .nrf52_mac_addr      = nrf52_mac_addr,
        .esp32_mac_addr_wifi = gateway_read_mac_addr(ESP_MAC_WIFI_STA),
        .esp32_mac_addr_eth  = gateway_read_mac_addr(ESP_MAC_ETH),
    };
    gw_cfg_default_init(&gw_cfg_default_init_param, &gw_cfg_default_json_read);
    if (NULL != p_nrf52_fw_ver)
    {
        gw_cfg_default_log();
    }

    const gw_cfg_device_info_t dev_info = gw_cfg_default_device_info();

    LOG_INFO(
        "### Init gw_cfg: device_id=%s, MAC=%s, nRF52_fw_ver=%s, WiFi_AP=%s",
        dev_info.nrf52_device_id.str_buf,
        dev_info.nrf52_mac_addr.str_buf,
        dev_info.nrf52_fw_ver.buf,
        dev_info.wifi_ap.ssid_buf);

    gw_cfg_init((NULL != p_nrf52_fw_ver) ? &ruuvi_cb_on_change_cfg : NULL);

    const bool               flag_allow_cfg_updating = (NULL != p_nrf52_fw_ver) ? true : false;
    settings_in_nvs_status_e settings_status         = SETTINGS_IN_NVS_STATUS_NOT_EXIST;

    const gw_cfg_t* p_gw_cfg_tmp = settings_get_from_flash(flag_allow_cfg_updating, &settings_status);
    if (NULL == p_gw_cfg_tmp)
    {
        LOG_ERR("Can't get settings from flash");
        return;
    }
    if (NULL != p_nrf52_fw_ver)
    {
        gw_cfg_log(p_gw_cfg_tmp, "Gateway SETTINGS (from flash)", false);
        switch (settings_status)
        {
            case SETTINGS_IN_NVS_STATUS_OK:
                LOG_INFO("##### Config exists in NVS");
                break;
            case SETTINGS_IN_NVS_STATUS_NOT_EXIST:
                LOG_INFO("##### Config not exists in NVS");
                break;
            case SETTINGS_IN_NVS_STATUS_EMPTY:
                LOG_INFO("##### Config is empty in NVS");
                break;
        }
        (void)gw_cfg_update((SETTINGS_IN_NVS_STATUS_OK == settings_status) ? p_gw_cfg_tmp : NULL);
    }
    os_free(p_gw_cfg_tmp);
}

static void
ruuvi_deinit_gw_cfg(void)
{
    LOG_INFO("### Deinit gw_cfg");
    gw_cfg_deinit();
    gw_cfg_default_deinit();
}

static void
ethernet_link_up_cb(void)
{
    LOG_INFO("Ethernet link up");
    gw_status_set_eth_link_up();
}

static void
ethernet_link_down_cb(void)
{
    LOG_INFO("Ethernet lost connection");
    g_network_disconnect_cnt += 1;
    wifi_manager_update_network_connection_info(UPDATE_LOST_CONNECTION, NULL, NULL, NULL);
    gw_status_clear_eth_connected();
    event_mgr_notify(EVENT_MGR_EV_ETH_DISCONNECTED);
}

static void
ethernet_connection_ok_cb(const esp_netif_ip_info_t* p_ip_info)
{
    LOG_INFO("### Ethernet connected");
    if (gw_cfg_is_empty())
    {
        LOG_INFO(
            "### The Ethernet cable was connected, but Gateway has not configured yet - set default configuration");
        main_task_send_sig_set_default_config();
        if (wifi_manager_is_ap_active())
        {
            LOG_INFO("### Force stop Wi-Fi AP after connecting Ethernet cable");
            wifi_manager_stop_ap();
        }
    }
    const struct dhcp* p_dhcp  = NULL;
    esp_ip4_addr_t     dhcp_ip = { 0 };
    if (gw_cfg_get_eth_use_dhcp())
    {
        esp_netif_t* const p_netif_eth = esp_netif_get_handle_from_ifkey("ETH_DEF");
        p_dhcp                         = netif_dhcp_data((struct netif*)esp_netif_get_netif_impl(p_netif_eth));
    }
    if (NULL != p_dhcp)
    {
        dhcp_ip.addr = p_dhcp->server_ip_addr.u_addr.ip4.addr;
    }
    wifi_manager_update_network_connection_info(
        UPDATE_CONNECTION_OK,
        NULL,
        p_ip_info,
        (NULL != p_dhcp) ? &dhcp_ip : NULL);
    gw_status_set_eth_connected();
    event_mgr_notify(EVENT_MGR_EV_ETH_CONNECTED);
}

void
wifi_connection_ok_cb(void* p_param)
{
    (void)p_param;
    LOG_INFO("Wifi connected");
    gw_status_set_wifi_connected();
    event_mgr_notify(EVENT_MGR_EV_WIFI_CONNECTED);
}

static void
cb_on_connect_eth_cmd(void)
{
    LOG_INFO("callback: on_connect_eth_cmd");
    ethernet_start();
}

static void
cb_on_disconnect_eth_cmd(void)
{
    LOG_INFO("callback: on_disconnect_eth_cmd");
    wifi_manager_update_network_connection_info(UPDATE_USER_DISCONNECT, NULL, NULL, NULL);
    ethernet_stop();
}

static void
cb_on_disconnect_sta_cmd(void)
{
    LOG_INFO("callback: on_disconnect_sta_cmd");
    gw_status_clear_wifi_connected();
}

static void
cb_on_ap_started(void)
{
    LOG_INFO("### callback: on_ap_started");
    event_mgr_notify(EVENT_MGR_EV_WIFI_AP_STARTED);
    main_task_stop_timer_check_for_remote_cfg();
    main_task_start_timer_hotspot_deactivation();
    const bool flag_wait_until_relaying_stopped = false;
    gw_status_suspend_relaying(flag_wait_until_relaying_stopped);
}

static void
cb_on_ap_stopped(void)
{
    LOG_INFO("### callback: on_ap_stopped");
    event_mgr_notify(EVENT_MGR_EV_WIFI_AP_STOPPED);
    main_task_stop_timer_hotspot_deactivation();
    if (gw_cfg_get_eth_use_eth() || (!wifi_manager_is_sta_configured()))
    {
        ethernet_start();
    }
    else
    {
        wifi_manager_connect_async();
    }
    main_task_send_sig_restart_services();

    const bool flag_wait_until_relaying_resumed = false;
    gw_status_resume_relaying(flag_wait_until_relaying_resumed);
}

static void
cb_on_ap_sta_connected(void)
{
    LOG_INFO("### callback: on_ap_sta_connected");
    event_mgr_notify(EVENT_MGR_EV_WIFI_AP_STA_CONNECTED);
    main_task_stop_timer_hotspot_deactivation();
    ethernet_stop();
}

static void
cb_on_ap_sta_ip_assigned(void)
{
    LOG_INFO("callback: on_ap_sta_ip_assigned");
    if (main_task_is_active_timer_hotspot_deactivation())
    {
        main_task_start_timer_hotspot_deactivation();
    }
}

static void
cb_on_ap_sta_disconnected(void)
{
    LOG_INFO("### callback: on_ap_sta_disconnected");
    event_mgr_notify(EVENT_MGR_EV_WIFI_AP_STA_DISCONNECTED);
    main_task_start_timer_hotspot_deactivation();
}

static void
cb_save_wifi_config(const wifiman_config_sta_t* const p_wifi_cfg_sta)
{
    gw_cfg_update_wifi_sta_config(p_wifi_cfg_sta);
}

static void
cb_on_request_status_json(void)
{
    LOG_INFO("callback: cb_on_request_status_json");
    if (main_task_is_active_timer_hotspot_deactivation())
    {
        main_task_stop_timer_hotspot_deactivation();
        main_task_start_timer_hotspot_deactivation();
    }
}

void
wifi_disconnect_cb(void* p_param)
{
    (void)p_param;
    LOG_WARN("Wifi disconnected");
    g_network_disconnect_cnt += 1;
    gw_status_clear_wifi_connected();
    event_mgr_notify(EVENT_MGR_EV_WIFI_DISCONNECTED);
}

static bool
wifi_init(
    const bool                    flag_connect_sta,
    const wifiman_config_t* const p_wifi_cfg,
    const char* const             p_fatfs_gwui_partition_name)
{
    static bool g_flag_wifi_initialized = false;
    if (g_flag_wifi_initialized)
    {
        wifi_manager_reconfigure(flag_connect_sta, p_wifi_cfg);

        return true;
    }
    g_flag_wifi_initialized = true;

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
        .cb_on_ap_started          = &cb_on_ap_started,
        .cb_on_ap_stopped          = &cb_on_ap_stopped,
        .cb_on_ap_sta_connected    = &cb_on_ap_sta_connected,
        .cb_on_ap_sta_ip_assigned  = &cb_on_ap_sta_ip_assigned,
        .cb_on_ap_sta_disconnected = &cb_on_ap_sta_disconnected,
        .cb_save_wifi_config_sta   = &cb_save_wifi_config,
        .cb_on_request_status_json = &cb_on_request_status_json,
    };
    wifi_manager_start(
        flag_connect_sta,
        p_wifi_cfg,
        &wifi_antenna_config,
        &wifi_callbacks,
        &mbedtls_ctr_drbg_random,
        &g_ctr_drbg);
    wifi_manager_set_callback(EVENT_STA_GOT_IP, &wifi_connection_ok_cb);
    wifi_manager_set_callback(EVENT_STA_DISCONNECTED, &wifi_disconnect_cb);
    configure_wifi_country_and_max_tx_power();
    return true;
}

static void
cb_before_nrf52_fw_updating(void)
{
    leds_notify_nrf52_fw_updating();
    fw_update_set_stage_nrf52_updating();

    http_server_cb_prohibit_cfg_updating();
    // Here we do not yet know the value of nRF52 DeviceID, so we cannot use it as the default password.
    if (!http_server_set_auth(HTTP_SERVER_AUTH_TYPE_ALLOW, NULL, NULL, NULL, NULL))
    {
        LOG_ERR("%s failed", "http_server_set_auth");
    }

    const wifiman_wifi_ssid_t* const p_wifi_ap_ssid = gw_cfg_get_wifi_ap_ssid();
    LOG_INFO("Read saved WiFi SSID: %s", p_wifi_ap_ssid->ssid_buf);
    const wifiman_hostname_t* const p_hostname = gw_cfg_get_hostname();
    LOG_INFO("Read saved hostname: %s", p_hostname->hostname_buf);

    const wifiman_config_t* const p_wifi_cfg             = wifi_manager_default_config_init(p_wifi_ap_ssid, p_hostname);
    const bool                    flag_connect_sta_false = false;
    if (!wifi_init(flag_connect_sta_false, p_wifi_cfg, fw_update_get_current_fatfs_gwui_partition_name()))
    {
        LOG_ERR("%s failed", "wifi_init");
        return;
    }
    const bool flag_block_req_from_lan = true;
    wifi_manager_start_ap(flag_block_req_from_lan);
}

void
sleep_with_task_watchdog_feeding(const int32_t delay_seconds)
{
    for (int32_t i = 0; i < delay_seconds; ++i)
    {
        esp_task_wdt_reset();
        vTaskDelay(pdMS_TO_TICKS(1 * 1000));
    }
}

static void
cb_after_nrf52_fw_updating(const bool flag_success)
{
    if (flag_success)
    {
        fw_update_set_extra_info_for_status_json_update_successful();
        LOG_INFO("nRF52 firmware has been successfully updated");
        sleep_with_task_watchdog_feeding(RUUVI_GATEWAY_DELAY_AFTER_NRF52_UPDATING_SECONDS);
        fw_update_set_extra_info_for_status_json_update_reset();
    }
    else
    {
        LOG_INFO("Failed to update nRF52 firmware");
        fw_update_set_extra_info_for_status_json_update_failed_nrf52("Failed to update nRF52 firmware");
        sleep_with_task_watchdog_feeding(RUUVI_GATEWAY_DELAY_AFTER_NRF52_UPDATING_SECONDS);
    }
    wifi_manager_stop_ap();
    http_server_cb_allow_cfg_updating();
}

static void
handle_reset_button_is_pressed_during_boot(void)
{
    LOG_INFO("Reset button is pressed during boot - erase settings in flash");

    const mac_address_bin_t mac_addr = settings_read_mac_addr();

    ruuvi_nvs_deinit();
    ruuvi_nvs_erase();
    ruuvi_nvs_init();

    const mac_address_bin_t mac_addr_zero = { 0 };
    if (0 != memcmp(&mac_addr_zero, &mac_addr, sizeof(mac_addr)))
    {
        LOG_INFO("Restore previous MAC addr: %s", mac_address_to_str(&mac_addr).str_buf);
        settings_write_mac_addr(&mac_addr);
    }

    settings_write_flag_rebooting_after_auto_update(false);
    settings_write_flag_force_start_wifi_hotspot(FORCE_START_WIFI_HOTSPOT_ONCE);

    LOG_INFO("Wait until the CONFIGURE button is released");
    leds_notify_cfg_erased();
    while (0 == gpio_get_level(RB_BUTTON_RESET_PIN))
    {
        vTaskDelay(1);
    }
    gateway_restart("The CONFIGURE button has been released - restart system");
}

static void
read_wifi_country_and_print_to_log(const char* const p_msg_prefix)
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
read_wifi_max_tx_power_and_print_to_log(const char* const p_msg_prefix)
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

static void
configure_mbedtls_rng(void)
{
    mbedtls_entropy_init(&g_entropy);
    mbedtls_ctr_drbg_init(&g_ctr_drbg);

    LOG_INFO("Seeding the random number generator...");

    const nrf52_device_id_str_t* const p_nrf52_device_id_str = gw_cfg_get_nrf52_device_id();
    str_buf_t                          custom_seed           = str_buf_printf_with_alloc(
        "%s:%lu:%lu",
        p_nrf52_device_id_str->str_buf,
        (printf_ulong_t)time(NULL),
        (printf_ulong_t)xTaskGetTickCount());

    if (0
        != mbedtls_ctr_drbg_seed(
            &g_ctr_drbg,
            mbedtls_entropy_func,
            &g_entropy,
            (const unsigned char*)custom_seed.buf,
            strlen(custom_seed.buf)))
    {
        LOG_ERR("%s: failed", "mbedtls_ctr_drbg_seed");
    }
}

static bool
network_subsystem_init(
    const force_start_wifi_hotspot_e force_start_wifi_hotspot,
    const wifiman_config_t* const    p_wifi_cfg)
{
    configure_mbedtls_rng();

    const bool is_wifi_sta_configured = ('\0' != p_wifi_cfg->sta.wifi_config_sta.ssid[0]) ? true : false;

    const bool flag_connect_sta = (!gw_cfg_get_eth_use_eth())
                                  && (FORCE_START_WIFI_HOTSPOT_DISABLED == force_start_wifi_hotspot)
                                  && is_wifi_sta_configured;
    if (!wifi_init(flag_connect_sta, p_wifi_cfg, fw_update_get_current_fatfs_gwui_partition_name()))
    {
        LOG_ERR("%s failed", "wifi_init");
        return false;
    }
    ethernet_init(&ethernet_link_up_cb, &ethernet_link_down_cb, &ethernet_connection_ok_cb);

    if (((!gw_cfg_is_empty()) || flag_connect_sta) && (FORCE_START_WIFI_HOTSPOT_DISABLED == force_start_wifi_hotspot))
    {
        if (gw_cfg_get_eth_use_eth() || (!is_wifi_sta_configured))
        {
            ethernet_start();
            if (!gw_status_is_eth_link_up())
            {
                LOG_INFO("### Force start WiFi hotspot (there is no Ethernet connection)");
                const bool flag_block_req_from_lan = true;
                wifi_manager_start_ap(flag_block_req_from_lan);
            }
        }
        else
        {
            LOG_INFO("Gateway already configured to use WiFi connection, so Ethernet is not needed");
        }
    }
    else
    {
        if (gw_cfg_is_empty() && (FORCE_START_WIFI_HOTSPOT_ONCE == force_start_wifi_hotspot))
        {
            gw_status_set_first_boot_after_cfg_erase();
        }
        else
        {
            gw_status_clear_first_boot_after_cfg_erase();
            if (!flag_connect_sta)
            {
                LOG_INFO("Start Ethernet (gateway has not configured yet)");
                ethernet_start();
            }
        }
        if (FORCE_START_WIFI_HOTSPOT_ONCE == force_start_wifi_hotspot)
        {
            settings_write_flag_force_start_wifi_hotspot(FORCE_START_WIFI_HOTSPOT_DISABLED);
        }
        if (gw_cfg_is_empty())
        {
            LOG_INFO("Force start WiFi hotspot (gateway has not configured yet)");
        }
        else
        {
            LOG_INFO("Force start WiFi hotspot");
        }
        wifi_manager_start_ap(true);
    }
    return true;
}

static void
ruuvi_cb_on_change_cfg(const gw_cfg_t* const p_gw_cfg)
{
    LOG_INFO("%s: settings_save_to_flash", __func__);
    settings_save_to_flash(p_gw_cfg);

    const ruuvi_gw_cfg_lan_auth_t* const p_lan_auth = (NULL != p_gw_cfg) ? &p_gw_cfg->ruuvi_cfg.lan_auth
                                                                         : gw_cfg_default_get_lan_auth();
    LOG_INFO("%s: http_server_set_auth: %s", __func__, http_server_auth_type_to_str(p_lan_auth->lan_auth_type));

    if (!http_server_set_auth(
            p_lan_auth->lan_auth_type,
            &p_lan_auth->lan_auth_user,
            &p_lan_auth->lan_auth_pass,
            &p_lan_auth->lan_auth_api_key,
            &p_lan_auth->lan_auth_api_key_rw))
    {
        LOG_ERR("%s failed", "http_server_set_auth");
    }
}

static bool
main_task_init(void)
{
    esp_log_level_set(TAG, ESP_LOG_INFO);
    reset_info_init();
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

    if (!gw_status_init())
    {
        LOG_ERR("%s failed", "gw_status_init");
        return false;
    }

    gpio_init();
    const bool is_configure_button_pressed = (0 == gpio_get_level(RB_BUTTON_RESET_PIN)) ? true : false;
    nrf52fw_hw_reset_nrf52(true);
    leds_init(is_configure_button_pressed);

    if (!reset_task_init())
    {
        LOG_ERR("Can't create thread");
        return false;
    }

    ruuvi_nvs_init();

    LOG_INFO(
        "Checking the state of CONFIGURE button during startup: is_pressed: %s",
        is_configure_button_pressed ? "yes" : "no");
    if (is_configure_button_pressed)
    {
        handle_reset_button_is_pressed_during_boot();
        return false;
    }

    if (!settings_check_in_flash())
    {
        LOG_ERR("There is no configuration in flash, try to erase flash and use default configuration");
        ruuvi_nvs_deinit();
        ruuvi_nvs_erase();
        ruuvi_nvs_init();
    }

    ruuvi_init_gw_cfg(NULL, NULL);

    main_task_init_timers();

    leds_notify_nrf52_fw_check();
    vTaskDelay(pdMS_TO_TICKS(750)); // give time for leds_task to turn off the red LED
    ruuvi_nrf52_fw_ver_t nrf52_fw_ver = { 0 };
    if (!nrf52fw_update_fw_if_necessary(
            fw_update_get_current_fatfs_nrf52_partition_name(),
            &fw_update_nrf52fw_cb_progress,
            NULL,
            &cb_before_nrf52_fw_updating,
            &cb_after_nrf52_fw_updating,
            &nrf52_fw_ver))
    {
        LOG_ERR("%s failed", "nrf52fw_update_fw_if_necessary");
        if (esp_ota_check_rollback_is_possible())
        {
            LOG_ERR("Firmware rollback is possible, so try to do it");
            return false;
        }
        LOG_ERR("Firmware rollback is not possible, try to send HTTP statistics");
        gw_status_clear_nrf_status();
        leds_notify_nrf52_failure();
    }
    else
    {
        gw_status_set_nrf_status();
        leds_notify_nrf52_ready();
    }

    adv_post_init();
    terminal_open(NULL, true, NRF52_COMM_TASK_PRIORITY);
    api_process(true);
    const nrf52_device_info_t nrf52_device_info = ruuvi_device_id_request_and_wait();

    ruuvi_deinit_gw_cfg();
    ruuvi_init_gw_cfg(&nrf52_fw_ver, &nrf52_device_info);
    LOG_INFO("### Config is empty: %s", gw_cfg_is_empty() ? "true" : "false");

    hmac_sha256_set_key_str(gw_cfg_get_nrf52_device_id()->str_buf); // set default encryption key

    main_task_subscribe_events();

    time_task_init();

    const force_start_wifi_hotspot_e force_start_wifi_hotspot = settings_read_flag_force_start_wifi_hotspot();
    const wifiman_config_t           wifi_cfg                 = gw_cfg_get_wifi_cfg();
    if (!network_subsystem_init(force_start_wifi_hotspot, &wifi_cfg))
    {
        LOG_ERR("%s failed", "network_subsystem_init");
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
        reset_info_set_sw("Rollback firmware");
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
