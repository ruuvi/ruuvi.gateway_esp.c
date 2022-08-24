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
#include "esp_netif_net_stack.h"
#include "ethernet.h"
#include "mbedtls/ctr_drbg.h"
#include "mbedtls/entropy.h"
#include "os_task.h"
#include "os_malloc.h"
#include "freertos/FreeRTOS.h"
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
#include "gw_cfg_json_parse.h"
#include "gw_cfg_log.h"
#include "lwip/dhcp.h"
#include "lwip/sockets.h"
#include "str_buf.h"
#include "wifiman_md5.h"
#include "json_ruuvi.h"
#include "gw_mac.h"
#include "gw_status.h"

#define LOG_LOCAL_LEVEL LOG_LEVEL_DEBUG
#include "log.h"

static const char TAG[] = "ruuvi_gateway";

#define MAC_ADDRESS_IDX_OF_LAST_BYTE        (MAC_ADDRESS_NUM_BYTES - 1U)
#define MAC_ADDRESS_IDX_OF_PENULTIMATE_BYTE (MAC_ADDRESS_NUM_BYTES - 2U)

#define WIFI_COUNTRY_DEFAULT_FIRST_CHANNEL (1U)
#define WIFI_COUNTRY_DEFAULT_NUM_CHANNELS  (13U)
#define WIFI_MAX_TX_POWER_DEFAULT          (9)

uint32_t volatile g_network_disconnect_cnt;

static mbedtls_entropy_context  g_entropy;
static mbedtls_ctr_drbg_context g_ctr_drbg;

static inline uint8_t
conv_bool_to_u8(const bool x)
{
    return x ? (uint8_t)RE_CA_BOOL_ENABLE : (uint8_t)RE_CA_BOOL_DISABLE;
}

void
ruuvi_send_nrf_settings(void)
{
    const gw_cfg_t *                   p_gw_cfg = gw_cfg_lock_ro();
    const ruuvi_gw_cfg_filter_t *const p_filter = &p_gw_cfg->ruuvi_cfg.filter;
    const ruuvi_gw_cfg_scan_t *const   p_scan   = &p_gw_cfg->ruuvi_cfg.scan;
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
        sniprintf(wifi_ap_ssid.ssid_buf, sizeof(wifi_ap_ssid.ssid_buf), "%sXXXX", DEFAULT_AP_SSID);
    }
    else
    {
        sniprintf(
            wifi_ap_ssid.ssid_buf,
            sizeof(wifi_ap_ssid.ssid_buf),
            "%s%02X%02X",
            DEFAULT_AP_SSID,
            mac_addr.mac[MAC_ADDRESS_IDX_OF_PENULTIMATE_BYTE],
            mac_addr.mac[MAC_ADDRESS_IDX_OF_LAST_BYTE]);
    }
    return wifi_ap_ssid;
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
    gw_status_clear_eth_connected();
    leds_indication_network_no_connection();
    event_mgr_notify(EVENT_MGR_EV_ETH_DISCONNECTED);
}

static void
ethernet_connection_ok_cb(const esp_netif_ip_info_t *p_ip_info)
{
    LOG_INFO("Ethernet connected");
    if (!gw_cfg_get_eth_use_eth())
    {
        LOG_INFO("The Ethernet cable was connected, but the Ethernet was not configured");
        LOG_INFO("Set the default configuration with Ethernet and DHCP enabled");
        gw_cfg_t *p_gw_cfg = os_calloc(1, sizeof(*p_gw_cfg));
        if (NULL == p_gw_cfg)
        {
            LOG_ERR("Can't allocate memory for gw_cfg");
            return;
        }
        gw_cfg_default_get(p_gw_cfg);
        p_gw_cfg->eth_cfg.use_eth  = true;
        p_gw_cfg->eth_cfg.eth_dhcp = true;
        gw_cfg_log(p_gw_cfg, "Gateway SETTINGS", false);
        (void)gw_cfg_update(p_gw_cfg);
        os_free(p_gw_cfg);
    }
    const struct dhcp *p_dhcp  = NULL;
    esp_ip4_addr_t     dhcp_ip = { 0 };
    if (gw_cfg_get_eth_use_dhcp())
    {
        esp_netif_t *const p_netif_eth = esp_netif_get_handle_from_ifkey("ETH_DEF");
        p_dhcp                         = netif_dhcp_data((struct netif *)esp_netif_get_netif_impl(p_netif_eth));
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
    start_services();
    event_mgr_notify(EVENT_MGR_EV_ETH_CONNECTED);
}

void
wifi_connection_ok_cb(void *p_param)
{
    (void)p_param;
    LOG_INFO("Wifi connected");
    gw_status_set_wifi_connected();
    start_services();
    event_mgr_notify(EVENT_MGR_EV_WIFI_CONNECTED);
}

static void
cb_on_connect_eth_cmd(void)
{
    LOG_INFO("callback: on_connect_eth_cmd");
    ethernet_start(gw_cfg_get_wifi_ap_ssid()->ssid_buf);
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
            ethernet_start(gw_cfg_get_wifi_ap_ssid()->ssid_buf);
        }
        main_task_start_timer_after_hotspot_activation();
    }
    adv_post_enable_retransmission();
}

static void
cb_save_wifi_config(const wifiman_config_sta_t *const p_wifi_cfg_sta)
{
    gw_cfg_update_wifi_sta_config(p_wifi_cfg_sta);
}

void
wifi_disconnect_cb(void *p_param)
{
    (void)p_param;
    LOG_WARN("Wifi disconnected");
    g_network_disconnect_cnt += 1;
    gw_status_clear_wifi_connected();
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
    main_task_configure_periodic_remote_cfg_check();

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
    const bool                    flag_use_eth,
    const bool                    flag_start_ap_only,
    const wifiman_config_t *const p_wifi_cfg,
    const char *const             p_fatfs_gwui_partition_name)
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
        .cb_save_wifi_config_sta   = &cb_save_wifi_config,
    };
    wifi_manager_start(
        !flag_use_eth,
        flag_start_ap_only,
        p_wifi_cfg,
        &wifi_antenna_config,
        &wifi_callbacks,
        &mbedtls_ctr_drbg_random,
        &g_ctr_drbg);
    wifi_manager_set_callback(EVENT_STA_GOT_IP, &wifi_connection_ok_cb);
    wifi_manager_set_callback(EVENT_STA_DISCONNECTED, &wifi_disconnect_cb);
    return true;
}

static void
cb_before_nrf52_fw_updating(void)
{
    fw_update_set_stage_nrf52_updating();

    // Here we do not yet know the value of nRF52 DeviceID, so we cannot use it as the default password.
    http_server_cb_prohibit_cfg_updating();
    if (!http_server_set_auth(HTTP_SERVER_AUTH_TYPE_ALLOW, NULL, NULL, NULL))
    {
        LOG_ERR("%s failed", "http_server_set_auth");
    }

    const wifiman_wifi_ssid_t wifi_ap_ssid = generate_wifi_ap_ssid(settings_read_mac_addr());
    LOG_INFO("Read saved WiFi SSID / Hostname: %s", wifi_ap_ssid.ssid_buf);

    const wifiman_config_t *const p_wifi_cfg = wifi_manager_default_config_init(&wifi_ap_ssid);
    if (!wifi_init(false, true, p_wifi_cfg, fw_update_get_current_fatfs_gwui_partition_name()))
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

ATTR_NORETURN
static void
handle_reset_button_is_pressed_during_boot(void)
{
    LOG_INFO("Reset button is pressed during boot - erase settings in flash");
    nrf52fw_hw_reset_nrf52(true);

    ruuvi_nvs_erase();
    ruuvi_nvs_init();

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

static void
configure_mbedtls_rng(void)
{
    mbedtls_entropy_init(&g_entropy);
    mbedtls_ctr_drbg_init(&g_ctr_drbg);

    LOG_INFO("Seeding the random number generator...");

    const nrf52_device_id_str_t *const p_nrf52_device_id_str = gw_cfg_get_nrf52_device_id();
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
            (const unsigned char *)custom_seed.buf,
            strlen(custom_seed.buf)))
    {
        LOG_ERR("%s: failed", "mbedtls_ctr_drbg_seed");
    }
}

static bool
network_subsystem_init(void)
{
    configure_mbedtls_rng();

    const wifiman_config_t wifi_cfg = gw_cfg_get_wifi_cfg();

    if (!wifi_init(
            gw_cfg_get_eth_use_eth(),
            settings_read_flag_force_start_wifi_hotspot(),
            &wifi_cfg,
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
            ethernet_start(gw_cfg_get_wifi_ap_ssid()->ssid_buf);
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

static void
ruuvi_cb_on_change_cfg(const gw_cfg_t *const p_gw_cfg)
{
    LOG_INFO("%s: settings_save_to_flash", __func__);
    settings_save_to_flash(p_gw_cfg);

    const ruuvi_gw_cfg_lan_auth_t lan_auth = gw_cfg_get_lan_auth();
    LOG_INFO("%s: http_server_set_auth: %s", __func__, http_server_auth_type_to_str(lan_auth.lan_auth_type));
    if (!http_server_set_auth(
            lan_auth.lan_auth_type,
            &lan_auth.lan_auth_user,
            &lan_auth.lan_auth_pass,
            &lan_auth.lan_auth_api_key))
    {
        LOG_ERR("%s failed", "http_server_set_auth");
    }
}

static void
ruuvi_init_gw_cfg(
    const ruuvi_nrf52_fw_ver_t *const p_nrf52_fw_ver,
    const nrf52_device_info_t *const  p_nrf52_device_info)
{
    const gw_cfg_default_init_param_t gw_cfg_default_init_param = {
        .wifi_ap_ssid        = generate_wifi_ap_ssid(p_nrf52_device_info->nrf52_mac_addr),
        .device_id           = p_nrf52_device_info->nrf52_device_id,
        .esp32_fw_ver        = fw_update_get_cur_version(),
        .nrf52_fw_ver        = nrf52_fw_ver_get_str(p_nrf52_fw_ver),
        .nrf52_mac_addr      = p_nrf52_device_info->nrf52_mac_addr,
        .esp32_mac_addr_wifi = gateway_read_mac_addr(ESP_MAC_WIFI_STA),
        .esp32_mac_addr_eth  = gateway_read_mac_addr(ESP_MAC_ETH),
    };
    gw_cfg_default_init(&gw_cfg_default_init_param, &gw_cfg_default_json_read);
    gw_cfg_init(&ruuvi_cb_on_change_cfg);

    const gw_cfg_t *p_gw_cfg_tmp = settings_get_from_flash();
    if (NULL == p_gw_cfg_tmp)
    {
        LOG_ERR("Can't get settings from flash");
        return;
    }
    gw_cfg_log(p_gw_cfg_tmp, "Gateway SETTINGS (from flash)", false);
    (void)gw_cfg_update(p_gw_cfg_tmp);
    os_free(p_gw_cfg_tmp);
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

    if (!gw_status_init())
    {
        LOG_ERR("%s failed", "gw_status_init");
        return false;
    }

    gpio_init();
    leds_init();

    ruuvi_nvs_init();

    if (!settings_check_in_flash())
    {
        ruuvi_nvs_deinit();
        ruuvi_nvs_erase();
        ruuvi_nvs_init();
    }

    ruuvi_nrf52_fw_ver_t nrf52_fw_ver = { 0 };

    leds_indication_on_nrf52_fw_updating();
    if (!nrf52fw_update_fw_if_necessary(
            fw_update_get_current_fatfs_nrf52_partition_name(),
            &fw_update_nrf52fw_cb_progress,
            NULL,
            &cb_before_nrf52_fw_updating,
            &cb_after_nrf52_fw_updating,
            &nrf52_fw_ver))
    {
        LOG_ERR("%s failed", "nrf52fw_update_fw_if_necessary");
        return false;
    }
    leds_indication_network_no_connection();

    adv_post_init();
    terminal_open(NULL, true);
    api_process(true);
    const nrf52_device_info_t nrf52_device_info = ruuvi_device_id_request_and_wait();

    settings_update_mac_addr(nrf52_device_info.nrf52_mac_addr);

    ruuvi_init_gw_cfg(&nrf52_fw_ver, &nrf52_device_info);

    hmac_sha256_set_key_str(gw_cfg_get_nrf52_device_id()->str_buf); // set default encryption key

    if (0 == gpio_get_level(RB_BUTTON_RESET_PIN))
    {
        ruuvi_nvs_deinit();
        handle_reset_button_is_pressed_during_boot();
    }

    main_task_init_timers();

    time_task_init();

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
