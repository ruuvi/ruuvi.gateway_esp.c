/**
 * @file network_subsystem.c
 * @author TheSomeMan
 * @date 2023-04-07
 * @copyright Ruuvi Innovations Ltd, license BSD-3-Clause.
 */

#include "network_subsystem.h"
#include <esp_netif_net_stack.h>
#include <esp_wifi.h>
#include "driver/gpio.h"
#include "ruuvi_boards.h"
#include "lwip/dhcp.h"
#include "mbedtls/ctr_drbg.h"
#include "mbedtls/entropy.h"
#include "settings.h"
#include "wifi_manager_defs.h"
#include "wifi_manager.h"
#include "ethernet.h"
#include "str_buf.h"
#include "mqtt.h"
#include "http_server_cb.h"
#include "gw_status.h"
#include "event_mgr.h"
#include "ruuvi_gateway.h"

#define LOG_LOCAL_LEVEL LOG_LEVEL_INFO
#include "log.h"

#define WIFI_COUNTRY_DEFAULT_FIRST_CHANNEL (1U)
#define WIFI_COUNTRY_DEFAULT_NUM_CHANNELS  (13U)
#define WIFI_MAX_TX_POWER_DEFAULT          (9)

static const char TAG[] = "network";

static mbedtls_entropy_context  g_entropy;
static mbedtls_ctr_drbg_context g_ctr_drbg;

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
wifi_connection_ok_cb(void* p_param) // NOSONAR
{
    (void)p_param;
    LOG_INFO("### Wi-Fi connected");
    gw_status_set_wifi_connected();
    event_mgr_notify(EVENT_MGR_EV_WIFI_CONNECTED);

    if (gw_status_is_waiting_auto_cfg_by_wps())
    {
        LOG_INFO("### Wi-Fi is configured by WPS - use default configuration");
        if (wifi_manager_is_ap_active())
        {
            LOG_INFO("### Force stop Wi-Fi AP after configuring by WPS");
            wifi_manager_stop_ap();
        }
        LOG_INFO("### Send signal to deactivate configuration mode after configuring by WPS");
        main_task_send_sig_deactivate_cfg_mode();
    }
}

static void
cb_on_connect_eth_cmd(void)
{
    LOG_INFO("%s: ### Start Ethernet", __func__);
    ethernet_start();
}

static void
cb_on_disconnect_eth_cmd(void)
{
    LOG_INFO("callback: on_disconnect_eth_cmd");
    wifi_manager_update_network_connection_info(UPDATE_USER_DISCONNECT, NULL, NULL, NULL);
    ethernet_stop();
    main_task_stop_timer_activation_ethernet_after_timeout();
}

static void
cb_on_disconnect_sta_cmd(void)
{
    LOG_INFO("callback: on_disconnect_sta_cmd");
    gw_status_clear_wifi_connected();
}

static void
cb_on_wps_started(void)
{
    LOG_INFO("### callback: on_wps_started");
    event_mgr_notify(EVENT_MGR_EV_WPS_ACTIVATED);
}

static void
cb_on_wps_stopped(void)
{
    LOG_INFO("### callback: on_wps_stopped");
    event_mgr_notify(EVENT_MGR_EV_WPS_DEACTIVATED);
}

static void
cb_on_ap_started(void)
{
    LOG_INFO("### callback: on_ap_started");
    event_mgr_notify(EVENT_MGR_EV_WIFI_AP_STARTED);
}

static void
cb_on_ap_stopped(void)
{
    LOG_INFO("### callback: on_ap_stopped");
    event_mgr_notify(EVENT_MGR_EV_WIFI_AP_STOPPED);
}

static void
cb_on_ap_sta_connected(void)
{
    LOG_INFO("### callback: on_ap_sta_connected");
    event_mgr_notify(EVENT_MGR_EV_WIFI_AP_STA_CONNECTED);
    ethernet_stop();
    main_task_stop_timer_activation_ethernet_after_timeout();
    if (gw_status_is_waiting_auto_cfg_by_wps())
    {
        LOG_INFO("### Disable WPS");
        wifi_manager_disable_wps();
        gw_status_clear_waiting_auto_cfg_by_wps();
    }
}

static void
cb_on_ap_sta_ip_assigned(void)
{
    LOG_INFO("callback: on_ap_sta_ip_assigned");
}

static void
cb_on_ap_sta_disconnected(void)
{
    LOG_INFO("### callback: on_ap_sta_disconnected");
    event_mgr_notify(EVENT_MGR_EV_WIFI_AP_STA_DISCONNECTED);
}

static void
cb_save_wifi_config(const wifiman_config_sta_t* const p_wifi_cfg_sta)
{
    if ('\0' != p_wifi_cfg_sta->wifi_config_sta.ssid[0])
    {
        // When Wi-Fi credentials got from WPS, then disable Ethernet.
        gw_cfg_set_eth_use_eth(false);
    }
    gw_cfg_update_wifi_sta_config(p_wifi_cfg_sta);
}

static void
cb_on_request_status_json(void)
{
    LOG_INFO("callback: cb_on_request_status_json");
    if (timer_cfg_mode_deactivation_is_active())
    {
        timer_cfg_mode_deactivation_start();
    }
}

void
wifi_disconnect_cb(void* p_param) // NOSONAR
{
    (void)p_param;
    LOG_WARN("Wifi disconnected");
    g_network_disconnect_cnt += 1;
    gw_status_clear_wifi_connected();
    event_mgr_notify(EVENT_MGR_EV_WIFI_DISCONNECTED);
}

static void
configure_mbedtls_rng(const nrf52_device_id_str_t* const p_nrf52_device_id_str)
{
    mbedtls_entropy_init(&g_entropy);
    mbedtls_ctr_drbg_init(&g_ctr_drbg);

    LOG_INFO("Seeding the random number generator...");

    str_buf_t custom_seed = str_buf_printf_with_alloc(
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
network_subsystem_check_if_ethernet_needs_to_be_started(
    const force_start_wifi_hotspot_e force_start_wifi_hotspot,
    const bool                       flag_gw_cfg_use_eth,
    const bool                       is_wifi_sta_configured)
{
    bool flag_start_ethernet = false;
    if (FORCE_START_WIFI_HOTSPOT_DISABLED == force_start_wifi_hotspot)
    {
        if (flag_gw_cfg_use_eth)
        {
            LOG_INFO("Gateway is configured to use Ethernet");
            flag_start_ethernet = true;
        }
        else if (!is_wifi_sta_configured)
        {
            LOG_INFO("Gateway is not configured to use Ethernet and WiFi is not configured, so enable Ethernet");
            flag_start_ethernet = true;
        }
        else
        {
            LOG_INFO("Gateway is configured to use WiFi connection, so Ethernet is not needed");
        }
    }
    return flag_start_ethernet;
}

static bool
network_subsystem_check_if_wifi_ap_needs_to_be_started(
    const force_start_wifi_hotspot_e force_start_wifi_hotspot,
    const bool                       flag_gw_cfg_empty)
{
    bool flag_start_wifi_ap = false;
    if (FORCE_START_WIFI_HOTSPOT_DISABLED != force_start_wifi_hotspot)
    {
        LOG_INFO("%s: Wi-Fi hotspot is forced to start", __func__);
        flag_start_wifi_ap = true;
    }
    else
    {
        if (flag_gw_cfg_empty)
        {
            LOG_INFO("%s: gw_cfg is empty, so need to start Wi-Fi hotspot", __func__);
            flag_start_wifi_ap = true;
        }
    }
    return flag_start_wifi_ap;
}

bool
network_subsystem_init(const force_start_wifi_hotspot_e force_start_wifi_hotspot, const gw_cfg_t* const p_gw_cfg)
{
    const bool flag_gw_cfg_empty      = gw_cfg_is_empty();
    const bool flag_gw_cfg_use_eth    = gw_cfg_get_eth_use_eth();
    const bool is_wifi_sta_configured = gw_cfg_is_wifi_sta_configured();
    const bool flag_connect_sta       = (!flag_gw_cfg_use_eth)
                                  && (FORCE_START_WIFI_HOTSPOT_DISABLED == force_start_wifi_hotspot)
                                  && is_wifi_sta_configured;

    const wifiman_config_t* const p_wifi_cfg = &p_gw_cfg->wifi_cfg;

    const nrf52_device_id_str_t* const p_nrf52_device_id_str = gw_cfg_get_nrf52_device_id();
    configure_mbedtls_rng(p_nrf52_device_id_str);

    LOG_INFO("%s: flag gw_cfg_empty:              %d", __func__, flag_gw_cfg_empty);
    LOG_INFO("%s: flag gw_cfg: use_eth:           %d", __func__, flag_gw_cfg_use_eth);
    LOG_INFO("%s: Wi-Fi Sta SSID:                 \"%s\"", __func__, p_wifi_cfg->sta.wifi_config_sta.ssid);
    LOG_INFO("%s: is_wifi_sta_configured:         %d", __func__, is_wifi_sta_configured);
    LOG_INFO("%s: flag connect_sta:               %d", __func__, flag_connect_sta);
    LOG_INFO("%s: flag force_start_wifi_hotspot:  %d", __func__, force_start_wifi_hotspot);

    if (FORCE_START_WIFI_HOTSPOT_ONCE == force_start_wifi_hotspot)
    {
        settings_write_flag_force_start_wifi_hotspot(FORCE_START_WIFI_HOTSPOT_DISABLED);
    }
    if (flag_gw_cfg_empty && (FORCE_START_WIFI_HOTSPOT_DISABLED != force_start_wifi_hotspot))
    {
        LOG_INFO("%s: Set GW_STATUS: first_boot_after_cfg_erase", __func__);
        gw_status_set_first_boot_after_cfg_erase();
    }
    else
    {
        gw_status_clear_first_boot_after_cfg_erase();
    }

    const bool flag_start_ethernet = network_subsystem_check_if_ethernet_needs_to_be_started(
        force_start_wifi_hotspot,
        flag_gw_cfg_use_eth,
        is_wifi_sta_configured);

    bool flag_start_wifi_ap = network_subsystem_check_if_wifi_ap_needs_to_be_started(
        force_start_wifi_hotspot,
        flag_gw_cfg_empty);

    LOG_INFO("### %s: Init Wi-Fi subsystem (flag_connect_sta=%d)", __func__, flag_connect_sta);
    if (!wifi_init(flag_connect_sta, p_wifi_cfg, fw_update_get_current_fatfs_gwui_partition_name()))
    {
        LOG_ERR("%s failed", "wifi_init");
        return false;
    }
    LOG_INFO("%s: ### Init Ethernet subsystem", __func__);
    ethernet_init(&ethernet_link_up_cb, &ethernet_link_down_cb, &ethernet_connection_ok_cb);

    if (flag_start_ethernet)
    {
        LOG_INFO("%s: ### Start Ethernet", __func__);
        ethernet_start();
        vTaskDelay(pdMS_TO_TICKS(100));
        if (!gw_status_is_eth_link_up())
        {
            LOG_INFO("%s: ### Ethernet cable is not connected", __func__);
            gw_status_clear_eth_connected();
            event_mgr_notify(EVENT_MGR_EV_ETH_DISCONNECTED);
            if (flag_gw_cfg_empty)
            {
                LOG_INFO(
                    "%s: ### There is no Ethernet connection and Gateway has not configured, "
                    "so need to start Wi-Fi hotspot",
                    __func__);
                flag_start_wifi_ap = true;
            }
        }
    }
    else
    {
        if (gw_status_get_first_boot_after_cfg_erase())
        {
            LOG_INFO(
                "%s: ### Start Ethernet after %u seconds on first boot after erasing configuration",
                __func__,
                RUUVI_DELAY_BEFORE_ETHERNET_ACTIVATION_ON_FIRST_BOOT_SEC);
            main_task_start_timer_activation_ethernet_after_timeout();
        }
        else
        {
            LOG_INFO("%s: ### Do not start Ethernet", __func__);
        }
    }

    if (flag_start_wifi_ap)
    {
        LOG_INFO("%s: ### Start Wi-Fi AP", __func__);
        start_wifi_ap();
        timer_cfg_mode_deactivation_start_with_delay(RUUVI_CFG_MODE_DEACTIVATION_LONG_DELAY_SEC);
    }
    else
    {
        LOG_INFO("%s: ### Do not start Wi-Fi AP", __func__);
    }
    if (gw_cfg_is_empty())
    {
        LOG_INFO("%s: ### Enable Wi-Fi WPS (gw_cfg is empty)", __func__);
        gw_status_set_waiting_auto_cfg_by_wps();
        wifi_manager_enable_wps();
    }
    return true;
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
        .cc    = "FI",                               /**< country code string */
        .schan = WIFI_COUNTRY_DEFAULT_FIRST_CHANNEL, /**< start channel */
        .nchan = WIFI_COUNTRY_DEFAULT_NUM_CHANNELS,  /**< total channel number */
        .max_tx_power
        = WIFI_MAX_TX_POWER_DEFAULT,        /**< This field is used for getting WiFi maximum transmitting power,
                                              call esp_wifi_set_max_tx_power to set the maximum transmitting power. */
        .policy = WIFI_COUNTRY_POLICY_AUTO, /**< country policy */
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

bool
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
    static const wifi_manager_callbacks_t g_wifi_callbacks = {
        .cb_on_http_user_req       = &http_server_cb_on_user_req,
        .cb_on_http_get            = &http_server_cb_on_get,
        .cb_on_http_post           = &http_server_cb_on_post,
        .cb_on_http_delete         = &http_server_cb_on_delete,
        .cb_on_connect_eth_cmd     = &cb_on_connect_eth_cmd,
        .cb_on_disconnect_eth_cmd  = &cb_on_disconnect_eth_cmd,
        .cb_on_disconnect_sta_cmd  = &cb_on_disconnect_sta_cmd,
        .cb_on_wps_started         = &cb_on_wps_started,
        .cb_on_wps_stopped         = &cb_on_wps_stopped,
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
        &g_wifi_callbacks,
        &mbedtls_ctr_drbg_random,
        &g_ctr_drbg);
    wifi_manager_set_callback(EVENT_STA_GOT_IP, &wifi_connection_ok_cb);
    wifi_manager_set_callback(EVENT_STA_DISCONNECTED, &wifi_disconnect_cb);
    configure_wifi_country_and_max_tx_power();
    return true;
}
