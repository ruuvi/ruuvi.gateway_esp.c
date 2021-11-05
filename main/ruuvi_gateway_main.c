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
#include <esp_task_wdt.h>
#include "ethernet.h"
#include "os_task.h"
#include "os_timer_sig.h"
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
#include "os_time.h"
#include "ruuvi_auth.h"
#include "lwip/dhcp.h"
#include "lwip/sockets.h"

#define LOG_LOCAL_LEVEL LOG_LEVEL_DEBUG
#include "log.h"

static const char TAG[] = "ruuvi_gateway";

#define MAC_ADDRESS_IDX_OF_LAST_BYTE        (MAC_ADDRESS_NUM_BYTES - 1U)
#define MAC_ADDRESS_IDX_OF_PENULTIMATE_BYTE (MAC_ADDRESS_NUM_BYTES - 2U)

#define MAIN_TASK_LOG_HEAP_USAGE_PERIOD_SECONDS (10U)

#define MAIN_TASK_TIMEOUT_AFTER_MANUAL_HOTSPOT_ACTIVATION_SEC (60)

typedef enum main_task_sig_e
{
    MAIN_TASK_SIG_LOG_HEAP_USAGE                      = OS_SIGNAL_NUM_0,
    MAIN_TASK_SIG_CHECK_FOR_FW_UPDATES                = OS_SIGNAL_NUM_1,
    MAIN_TASK_SIG_SCHEDULE_NEXT_CHECK_FOR_FW_UPDATES  = OS_SIGNAL_NUM_2,
    MAIN_TASK_SIG_SCHEDULE_RETRY_CHECK_FOR_FW_UPDATES = OS_SIGNAL_NUM_3,
    MAIN_TASK_SIG_DEACTIVATE_WIFI_AP                  = OS_SIGNAL_NUM_4,
    MAIN_TASK_SIG_TASK_RESTART_SERVICES               = OS_SIGNAL_NUM_5,
    MAIN_TASK_SIG_TASK_WATCHDOG_FEED                  = OS_SIGNAL_NUM_6,
} main_task_sig_e;

#define MAIN_TASK_SIG_FIRST (MAIN_TASK_SIG_LOG_HEAP_USAGE)
#define MAIN_TASK_SIG_LAST  (MAIN_TASK_SIG_TASK_WATCHDOG_FEED)

EventGroupHandle_t status_bits;
uint32_t volatile g_network_disconnect_cnt;

static os_signal_t *                  g_p_signal_main_task;
static os_signal_static_t             g_signal_main_task_mem;
static os_timer_sig_periodic_t *      g_p_timer_sig_log_heap_usage;
static os_timer_sig_periodic_static_t g_timer_sig_log_heap_usage;
static os_timer_sig_one_shot_t *      g_p_timer_sig_check_for_fw_updates;
static os_timer_sig_one_shot_static_t g_timer_sig_check_for_fw_updates_mem;
static os_timer_sig_one_shot_t *      g_p_timer_sig_deactivate_wifi_ap;
static os_timer_sig_one_shot_static_t g_p_timer_sig_deactivate_wifi_ap_mem;
static os_timer_sig_periodic_t *      g_p_timer_sig_task_watchdog_feed;
static os_timer_sig_periodic_static_t g_timer_sig_task_watchdog_feed_mem;

ATTR_PURE
static os_signal_num_e
main_task_conv_to_sig_num(const main_task_sig_e sig)
{
    return (os_signal_num_e)sig;
}

static main_task_sig_e
main_task_conv_from_sig_num(const os_signal_num_e sig_num)
{
    assert(((os_signal_num_e)MAIN_TASK_SIG_FIRST <= sig_num) && (sig_num <= (os_signal_num_e)MAIN_TASK_SIG_LAST));
    return (main_task_sig_e)sig_num;
}

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
        *p_gw_cfg                        = g_gateway_config_default;
        p_gw_cfg->eth.use_eth            = true;
        p_gw_cfg->eth.eth_dhcp           = true;
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
    ip4_addr_t *p_dhcp_ip = NULL;
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
            struct dhcp *const p_dhcp = netif_dhcp_data(p_netif);
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
    os_signal_send(g_p_signal_main_task, main_task_conv_to_sig_num(MAIN_TASK_SIG_TASK_RESTART_SERVICES));
}

static void
restart_services_internal(void)
{
    LOG_INFO("Restart services");
    if (mqtt_app_is_working())
    {
        // mqtt_app_stop can take up to 4500 ms, so we need to feed the task watchdog here
        const esp_err_t err = esp_task_wdt_reset();
        if (ESP_OK != err)
        {
            LOG_ERR_ESP(err, "%s failed", "esp_task_wdt_reset");
        }
        mqtt_app_stop();
    }
    start_services();
}

static bool
wifi_init(
    const bool               flag_use_eth,
    const bool               flag_start_ap_only,
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
    wifi_manager_start(!flag_use_eth, flag_start_ap_only, p_gw_wifi_ssid, &wiFiAntConfig, &wifi_callbacks);
    wifi_manager_set_callback(EVENT_STA_GOT_IP, &wifi_connection_ok_cb);
    wifi_manager_set_callback(EVENT_STA_DISCONNECTED, &wifi_disconnect_cb);
    return true;
}

static void
request_and_wait_nrf52_id(void)
{
    const uint32_t delay_ms = 1000U;
    const uint32_t step_ms  = 100U;
    for (uint32_t i = 0; (i < 3U) && (!ruuvi_device_id_is_set()); ++i)
    {
        LOG_INFO("Request nRF52 ID");
        ruuvi_send_nrf_get_id();

        for (uint32_t j = 0; j < delay_ms / step_ms; ++j)
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

    const mac_address_bin_t nrf52_mac_addr = settings_read_mac_addr();
    set_gw_mac_sta(&nrf52_mac_addr, &g_gw_mac_sta_str, &g_gw_wifi_ssid);
    LOG_INFO("Mac address: %s", g_gw_mac_sta_str.str_buf);
    LOG_INFO("WiFi SSID / Hostname: %s", g_gw_wifi_ssid.ssid_buf);

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

static const char *
get_wday_if_set_in_bitmask(const auto_update_weekdays_bitmask_t auto_update_weekdays_bitmask, const os_time_wday_e wday)
{
    if (0 != (auto_update_weekdays_bitmask & (1U << (unsigned)wday)))
    {
        return os_time_wday_name_mid(wday);
    }
    return "";
}

static bool
check_if_checking_for_fw_updates_allowed2(const ruuvi_gw_cfg_auto_update_t *const p_cfg_auto_update)
{
    const time_t unix_time = os_time_get();
    time_t       cur_time  = unix_time + (time_t)((int32_t)p_cfg_auto_update->auto_update_tz_offset_hours * 60 * 60);
    struct tm    tm_time   = { 0 };
    gmtime_r(&cur_time, &tm_time);

    LOG_INFO(
        "Check for fw updates: configured weekdays: %s %s %s %s %s %s %s, current: %s",
        get_wday_if_set_in_bitmask(p_cfg_auto_update->auto_update_weekdays_bitmask, OS_TIME_WDAY_SUN),
        get_wday_if_set_in_bitmask(p_cfg_auto_update->auto_update_weekdays_bitmask, OS_TIME_WDAY_MON),
        get_wday_if_set_in_bitmask(p_cfg_auto_update->auto_update_weekdays_bitmask, OS_TIME_WDAY_TUE),
        get_wday_if_set_in_bitmask(p_cfg_auto_update->auto_update_weekdays_bitmask, OS_TIME_WDAY_WED),
        get_wday_if_set_in_bitmask(p_cfg_auto_update->auto_update_weekdays_bitmask, OS_TIME_WDAY_THU),
        get_wday_if_set_in_bitmask(p_cfg_auto_update->auto_update_weekdays_bitmask, OS_TIME_WDAY_FRI),
        get_wday_if_set_in_bitmask(p_cfg_auto_update->auto_update_weekdays_bitmask, OS_TIME_WDAY_SAT),
        os_time_wday_name_mid(os_time_get_tm_wday(&tm_time)));

    const uint32_t cur_day_bit_mask = 1U << (uint8_t)tm_time.tm_wday;
    if (0 == (p_cfg_auto_update->auto_update_weekdays_bitmask & cur_day_bit_mask))
    {
        LOG_INFO("Check for fw updates - skip (weekday does not match)");
        return false;
    }
    LOG_INFO(
        "Check for fw updates: configured range [%02u:00 .. %02u:00], current time: %02u:%02u)",
        (printf_uint_t)p_cfg_auto_update->auto_update_interval_from,
        (printf_uint_t)p_cfg_auto_update->auto_update_interval_to,
        (printf_uint_t)tm_time.tm_hour,
        (printf_uint_t)tm_time.tm_min);
    if (!((tm_time.tm_hour >= p_cfg_auto_update->auto_update_interval_from)
          && (tm_time.tm_hour < p_cfg_auto_update->auto_update_interval_to)))
    {
        LOG_INFO("Check for fw updates - skip (current time is out of range)");
        return false;
    }
    return true;
}

static bool
check_if_checking_for_fw_updates_allowed(void)
{
    if (!wifi_manager_is_connected_to_wifi_or_ethernet())
    {
        LOG_INFO("Check for fw updates - skip (not connected to WiFi or Ethernet)");
        return false;
    }
    if (!time_is_synchronized())
    {
        LOG_INFO("Check for fw updates - skip (time is not synchronized)");
        return false;
    }
    const ruuvi_gateway_config_t *p_gw_cfg = gw_cfg_lock_ro();

    const bool res = check_if_checking_for_fw_updates_allowed2(&p_gw_cfg->auto_update);

    gw_cfg_unlock_ro(&p_gw_cfg);
    return res;
}

static void
main_task_handle_sig(const main_task_sig_e main_task_sig)
{
    switch (main_task_sig)
    {
        case MAIN_TASK_SIG_LOG_HEAP_USAGE:
        {
            const uint32_t free_heap = esp_get_free_heap_size();
            LOG_INFO("free heap: %lu", (printf_ulong_t)free_heap);
            if (free_heap < (RUUVI_FREE_HEAP_LIM_KIB * 1024U))
            {
                // TODO: in ESP-IDF v4.x there is API heap_caps_register_failed_alloc_callback,
                //       which allows to catch 'no memory' event and reboot.
                LOG_ERR(
                    "Only %uKiB of free memory left - probably due to a memory leak. Reboot the Gateway.",
                    (printf_uint_t)(free_heap / 1024U));
                esp_restart();
            }
            break;
        }
        case MAIN_TASK_SIG_CHECK_FOR_FW_UPDATES:
        {
            if (check_if_checking_for_fw_updates_allowed())
            {
                LOG_INFO("Check for fw updates: activate");
                http_server_user_req(HTTP_SERVER_USER_REQ_CODE_DOWNLOAD_LATEST_RELEASE_INFO);
            }
            else
            {
                main_task_schedule_retry_check_for_fw_updates();
            }
            break;
        }
        case MAIN_TASK_SIG_SCHEDULE_NEXT_CHECK_FOR_FW_UPDATES:
        {
            const os_delta_ticks_t delay_ticks = pdMS_TO_TICKS(RUUVI_CHECK_FOR_FW_UPDATES_DELAY_AFTER_SUCCESS_SECONDS)
                                                 * 1000;
            LOG_INFO(
                "Schedule next check for fw updates (after successful release_info downloading) after %lu seconds (%lu "
                "ticks)",
                (printf_ulong_t)RUUVI_CHECK_FOR_FW_UPDATES_DELAY_AFTER_SUCCESS_SECONDS,
                (printf_ulong_t)delay_ticks);
            os_timer_sig_one_shot_restart(g_p_timer_sig_check_for_fw_updates, delay_ticks);
            break;
        }
        case MAIN_TASK_SIG_SCHEDULE_RETRY_CHECK_FOR_FW_UPDATES:
        {
            const os_delta_ticks_t delay_ticks = pdMS_TO_TICKS(RUUVI_CHECK_FOR_FW_UPDATES_DELAY_BEFORE_RETRY_SECONDS)
                                                 * 1000;
            LOG_INFO(
                "Schedule a recheck for fw updates after %lu seconds (%lu ticks)",
                (printf_ulong_t)RUUVI_CHECK_FOR_FW_UPDATES_DELAY_BEFORE_RETRY_SECONDS,
                (printf_ulong_t)delay_ticks);
            os_timer_sig_one_shot_restart(g_p_timer_sig_check_for_fw_updates, delay_ticks);
            break;
        }
        case MAIN_TASK_SIG_DEACTIVATE_WIFI_AP:
            LOG_INFO("MAIN_TASK_SIG_DEACTIVATE_WIFI_AP");
            wifi_manager_stop_ap();
            if (gw_cfg_get_eth_use_eth() || (!wifi_manager_is_sta_configured()))
            {
                ethernet_start(g_gw_wifi_ssid.ssid_buf);
            }
            else
            {
                wifi_manager_connect_async();
            }

            leds_indication_network_no_connection();
            break;
        case MAIN_TASK_SIG_TASK_RESTART_SERVICES:
            restart_services_internal();
            break;
        case MAIN_TASK_SIG_TASK_WATCHDOG_FEED:
        {
            const esp_err_t err = esp_task_wdt_reset();
            if (ESP_OK != err)
            {
                LOG_ERR_ESP(err, "%s failed", "esp_task_wdt_reset");
            }
            break;
        }
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

ATTR_NORETURN
static void
main_loop(void)
{
    LOG_INFO("Main loop started");
    for (;;)
    {
        os_signal_events_t sig_events = { 0 };
        if (!os_signal_wait_with_timeout(g_p_signal_main_task, OS_DELTA_TICKS_INFINITE, &sig_events))
        {
            continue;
        }
        for (;;)
        {
            const os_signal_num_e sig_num = os_signal_num_get_next(&sig_events);
            if (OS_SIGNAL_NUM_NONE == sig_num)
            {
                break;
            }
            const main_task_sig_e main_task_sig = main_task_conv_from_sig_num(sig_num);
            main_task_handle_sig(main_task_sig);
        }
    }
}

static void
configure_wifi_country_and_max_tx_power(void)
{
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
                "WiFi country after wifi_init: CC=%.3s, max_tx_power=%d dBm, schan=%u, nchan=%u, policy=%s",
                country.cc,
                (printf_int_t)country.max_tx_power,
                (printf_uint_t)country.schan,
                (printf_uint_t)country.nchan,
                country.policy ? "MANUAL" : "AUTO");
        }
    }
    {
        wifi_country_t country = {
            .cc           = "FI", /**< country code string */
            .schan        = 1,    /**< start channel */
            .nchan        = 13,   /**< total channel number */
            .max_tx_power = 9,    /**< This field is used for getting WiFi maximum transmitting power, call
                                     esp_wifi_set_max_tx_power to set the maximum transmitting power. */
            .policy = WIFI_COUNTRY_POLICY_AUTO, /**< country policy */
        };
        const esp_err_t err = esp_wifi_set_country(&country);
        if (ESP_OK != err)
        {
            LOG_ERR_ESP(err, "%s failed", "esp_wifi_get_country");
        }
    }
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
                "WiFi country after esp_wifi_set_country: CC=%.3s, max_tx_power=%d dBm, schan=%u, nchan=%u, policy=%s",
                country.cc,
                (printf_int_t)country.max_tx_power,
                (printf_uint_t)country.schan,
                (printf_uint_t)country.nchan,
                country.policy ? "MANUAL" : "AUTO");
        }
    }

    {
        int8_t          power = 0;
        const esp_err_t err   = esp_wifi_get_max_tx_power(&power);
        if (ESP_OK != err)
        {
            LOG_ERR_ESP(err, "%s failed", "esp_wifi_get_max_tx_power");
        }
        else
        {
            LOG_INFO("Max WiFi TX power after wifi_init: %d (%d dBm)", (printf_int_t)power, (printf_int_t)(power / 4));
        }
    }
    {
        const esp_err_t err = esp_wifi_set_max_tx_power(9 /*dbB*/ * 4);
        if (ESP_OK != err)
        {
            LOG_ERR_ESP(err, "%s failed", "esp_wifi_set_max_tx_power");
        }
    }
    {
        int8_t          power = 0;
        const esp_err_t err   = esp_wifi_get_max_tx_power(&power);
        if (ESP_OK != err)
        {
            LOG_ERR_ESP(err, "%s failed", "esp_wifi_get_max_tx_power");
        }
        else
        {
            LOG_INFO(
                "Max WiFi TX power after esp_wifi_set_max_tx_power: %d (%d dBm)",
                (printf_int_t)power,
                (printf_int_t)(power / 4));
        }
    }
}

static void
main_wdt_add_and_start(void)
{
    LOG_INFO("TaskWatchdog: Register current thread");
    const esp_err_t err = esp_task_wdt_add(xTaskGetCurrentTaskHandle());
    if (ESP_OK != err)
    {
        LOG_ERR_ESP(err, "%s failed", "esp_task_wdt_add");
    }
    LOG_INFO("TaskWatchdog: Start timer");
    os_timer_sig_periodic_start(g_p_timer_sig_task_watchdog_feed);
}

static bool
main_task_init(void)
{
    esp_log_level_set(TAG, ESP_LOG_DEBUG);
    cjson_wrap_init();
    gw_cfg_init();

    g_p_signal_main_task = os_signal_create_static(&g_signal_main_task_mem);
    os_signal_add(g_p_signal_main_task, main_task_conv_to_sig_num(MAIN_TASK_SIG_LOG_HEAP_USAGE));
    os_signal_add(g_p_signal_main_task, main_task_conv_to_sig_num(MAIN_TASK_SIG_CHECK_FOR_FW_UPDATES));
    os_signal_add(g_p_signal_main_task, main_task_conv_to_sig_num(MAIN_TASK_SIG_SCHEDULE_NEXT_CHECK_FOR_FW_UPDATES));
    os_signal_add(g_p_signal_main_task, main_task_conv_to_sig_num(MAIN_TASK_SIG_SCHEDULE_RETRY_CHECK_FOR_FW_UPDATES));
    os_signal_add(g_p_signal_main_task, main_task_conv_to_sig_num(MAIN_TASK_SIG_DEACTIVATE_WIFI_AP));
    os_signal_add(g_p_signal_main_task, main_task_conv_to_sig_num(MAIN_TASK_SIG_TASK_RESTART_SERVICES));
    os_signal_add(g_p_signal_main_task, main_task_conv_to_sig_num(MAIN_TASK_SIG_TASK_WATCHDOG_FEED));

    g_p_timer_sig_log_heap_usage = os_timer_sig_periodic_create_static(
        &g_timer_sig_log_heap_usage,
        "log_heap_usage",
        g_p_signal_main_task,
        main_task_conv_to_sig_num(MAIN_TASK_SIG_LOG_HEAP_USAGE),
        pdMS_TO_TICKS(MAIN_TASK_LOG_HEAP_USAGE_PERIOD_SECONDS * 1000U));
    g_p_timer_sig_check_for_fw_updates = os_timer_sig_one_shot_create_static(
        &g_timer_sig_check_for_fw_updates_mem,
        "check_fw_updates",
        g_p_signal_main_task,
        main_task_conv_to_sig_num(MAIN_TASK_SIG_CHECK_FOR_FW_UPDATES),
        pdMS_TO_TICKS(RUUVI_CHECK_FOR_FW_UPDATES_DELAY_AFTER_REBOOT_SECONDS) * 1000U);
    g_p_timer_sig_deactivate_wifi_ap = os_timer_sig_one_shot_create_static(
        &g_p_timer_sig_deactivate_wifi_ap_mem,
        "deactivate_ap",
        g_p_signal_main_task,
        main_task_conv_to_sig_num(MAIN_TASK_SIG_DEACTIVATE_WIFI_AP),
        pdMS_TO_TICKS(MAIN_TASK_TIMEOUT_AFTER_MANUAL_HOTSPOT_ACTIVATION_SEC * 1000));
    g_p_timer_sig_task_watchdog_feed = os_timer_sig_periodic_create_static(
        &g_timer_sig_task_watchdog_feed_mem,
        "main_wgod",
        g_p_signal_main_task,
        main_task_conv_to_sig_num(MAIN_TASK_SIG_TASK_WATCHDOG_FEED),
        pdMS_TO_TICKS(CONFIG_ESP_TASK_WDT_TIMEOUT_S * 1000U / 3U));

    if (!os_signal_register_cur_thread(g_p_signal_main_task))
    {
        LOG_ERR("%s failed", "os_signal_register_cur_thread");
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

    if (!wifi_manager_check_sta_config() || !settings_check_in_flash())
    {
        ruuvi_nvs_flash_deinit();
        ruuvi_nvs_flash_erase();
        ruuvi_nvs_flash_init();
    }

    gateway_read_mac_addr(ESP_MAC_WIFI_STA, &g_gw_mac_wifi, &g_gw_mac_wifi_str);
    gateway_read_mac_addr(ESP_MAC_ETH, &g_gw_mac_eth, &g_gw_mac_eth_str);

    settings_get_from_flash();

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

    const mac_address_bin_t nrf52_mac_addr = ruuvi_device_id_get_nrf52_mac_address();
    set_gw_mac_sta(&nrf52_mac_addr, &g_gw_mac_sta_str, &g_gw_wifi_ssid);
    LOG_INFO("Mac address: %s", g_gw_mac_sta_str.str_buf);
    LOG_INFO("WiFi SSID / Hostname: %s", g_gw_wifi_ssid.ssid_buf);

    settings_update_mac_addr(&nrf52_mac_addr);

    time_task_init();
    ruuvi_send_nrf_settings();

    ruuvi_auth_set_from_config();

    if (!wifi_init(
            gw_cfg_get_eth_use_eth(),
            settings_read_flag_force_start_wifi_hotspot(),
            &g_gw_wifi_ssid,
            fw_update_get_current_fatfs_gwui_partition_name()))
    {
        LOG_ERR("%s failed", "wifi_init");
        return false;
    }
    else
    {
        configure_wifi_country_and_max_tx_power();
    }
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

    if (!reset_task_init())
    {
        LOG_ERR("Can't create thread");
        return false;
    }

    os_timer_sig_periodic_start(g_p_timer_sig_log_heap_usage);
    if (AUTO_UPDATE_CYCLE_TYPE_MANUAL != gw_cfg_get_auto_update_cycle())
    {
        LOG_INFO(
            "Firmware auto-updating is active, run next check after %lu seconds",
            (printf_ulong_t)RUUVI_CHECK_FOR_FW_UPDATES_DELAY_AFTER_REBOOT_SECONDS);
        os_timer_sig_one_shot_start(g_p_timer_sig_check_for_fw_updates);
    }
    else
    {
        LOG_INFO("Firmware auto-updating is not active");
    }

    main_wdt_add_and_start();

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

void
main_task_start_timer_after_hotspot_activation(void)
{
    LOG_INFO("Start AP timer for %u seconds", MAIN_TASK_TIMEOUT_AFTER_MANUAL_HOTSPOT_ACTIVATION_SEC);
    os_timer_sig_one_shot_start(g_p_timer_sig_deactivate_wifi_ap);
}

void
main_task_stop_timer_after_hotspot_activation(void)
{
    LOG_INFO("Stop AP timer");
    os_timer_sig_one_shot_stop(g_p_timer_sig_deactivate_wifi_ap);
}

void
main_task_schedule_next_check_for_fw_updates(void)
{
    os_signal_send(g_p_signal_main_task, main_task_conv_to_sig_num(MAIN_TASK_SIG_SCHEDULE_NEXT_CHECK_FOR_FW_UPDATES));
}

void
main_task_schedule_retry_check_for_fw_updates(void)
{
    os_signal_send(g_p_signal_main_task, main_task_conv_to_sig_num(MAIN_TASK_SIG_SCHEDULE_RETRY_CHECK_FOR_FW_UPDATES));
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
