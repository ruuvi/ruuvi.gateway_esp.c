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
#include "freertos/FreeRTOS.h"
#include "lwip/sockets.h"
#include "adv_post.h"
#include "adv_mqtt.h"
#include "api.h"
#include "cJSON.h"
#include "os_task.h"
#include "os_malloc.h"
#include "gpio.h"
#include "leds.h"
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
#include "gw_mac.h"
#include "gw_status.h"
#include "reset_info.h"
#include "network_subsystem.h"
#include "gw_cfg_storage.h"

#define LOG_LOCAL_LEVEL LOG_LEVEL_INFO
#include "log.h"
#include "partition_table.h"

static const char TAG[] = "ruuvi_gateway";

#define RUUVI_GATEWAY_WIFI_AP_PREFIX  "Configure Ruuvi Gateway "
#define RUUVI_GATEWAY_HOSTNAME_PREFIX "RuuviGateway"

#define MAC_ADDRESS_IDX_OF_LAST_BYTE        (MAC_ADDRESS_NUM_BYTES - 1U)
#define MAC_ADDRESS_IDX_OF_PENULTIMATE_BYTE (MAC_ADDRESS_NUM_BYTES - 2U)

#define NRF52_COMM_TASK_PRIORITY (9)

#define RUUVI_GATEWAY_DELAY_AFTER_NRF52_UPDATING_SECONDS (5)

uint32_t volatile g_network_disconnect_cnt;

static os_mutex_t        g_http_server_mutex_incoming_connection;
static os_mutex_static_t g_http_server_mutex_incoming_connection_mem;

void
http_server_mutex_activate(void)
{
    http_server_use_mutex_for_incoming_connection_handling(g_http_server_mutex_incoming_connection);
}

void
http_server_mutex_deactivate(void)
{
    http_server_use_mutex_for_incoming_connection_handling(NULL);
}

bool
http_server_mutex_try_lock(void)
{
    return os_mutex_try_lock(g_http_server_mutex_incoming_connection);
}

void
http_server_mutex_unlock(void)
{
    os_mutex_unlock(g_http_server_mutex_incoming_connection);
}

static void
ruuvi_cb_on_change_cfg(const gw_cfg_t* const p_gw_cfg);

static inline uint8_t
conv_bool_to_u8(const bool x)
{
    return x ? (uint8_t)RE_CA_BOOL_ENABLE : (uint8_t)RE_CA_BOOL_DISABLE;
}

void
ruuvi_send_nrf_settings(const ruuvi_gw_cfg_scan_t* const p_scan, const ruuvi_gw_cfg_filter_t* const p_filter)
{
    LOG_INFO(
        "### sending settings to NRF: "
        "use filter: %d, "
        "company id: 0x%04x,"
        "use scan Coded PHY: %d,"
        "use scan 1Mbit PHY: %d,"
        "use scan 2Mbit PHY: %d,"
        "use scan channel 37: %d,"
        "use scan channel 38: %d,"
        "use scan channel 39: %d",
        p_filter->company_use_filtering,
        p_filter->company_id,
        p_scan->scan_coded_phy,
        p_scan->scan_1mbit_phy,
        p_scan->scan_2mbit_phy,
        p_scan->scan_channel_37,
        p_scan->scan_channel_38,
        p_scan->scan_channel_39);

    api_send_all(
        RE_CA_UART_SET_ALL,
        p_filter->company_id,
        conv_bool_to_u8(p_filter->company_use_filtering),
        conv_bool_to_u8(p_scan->scan_coded_phy),
        conv_bool_to_u8(p_scan->scan_2mbit_phy),
        conv_bool_to_u8(p_scan->scan_1mbit_phy),
        conv_bool_to_u8(p_scan->scan_channel_37),
        conv_bool_to_u8(p_scan->scan_channel_38),
        conv_bool_to_u8(p_scan->scan_channel_39),
        ADV_DATA_MAX_LEN);
}

void
ruuvi_send_nrf_settings_from_gw_cfg(void)
{
    const gw_cfg_t* p_gw_cfg = gw_cfg_lock_ro();
    ruuvi_send_nrf_settings(&p_gw_cfg->ruuvi_cfg.scan, &p_gw_cfg->ruuvi_cfg.filter);
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
        sniprintf(hostname.buf, sizeof(hostname.buf), "%sXXXX", RUUVI_GATEWAY_HOSTNAME_PREFIX);
    }
    else
    {
        sniprintf(
            hostname.buf,
            sizeof(hostname.buf),
            "%s%02X%02X",
            RUUVI_GATEWAY_HOSTNAME_PREFIX,
            mac_addr.mac[MAC_ADDRESS_IDX_OF_PENULTIMATE_BYTE],
            mac_addr.mac[MAC_ADDRESS_IDX_OF_LAST_BYTE]);
    }
    return hostname;
}

static const gw_cfg_default_init_param_t*
ruuvi_generate_init_params(
    const ruuvi_nrf52_fw_ver_t* const p_nrf52_fw_ver,
    const nrf52_device_info_t* const  p_nrf52_device_info)
{
    gw_cfg_default_init_param_t* const p_init_params = os_calloc(1, sizeof(*p_init_params));
    if (NULL == p_init_params)
    {
        return NULL;
    }
    const nrf52_device_id_t nrf52_device_id = (NULL != p_nrf52_device_info) ? p_nrf52_device_info->nrf52_device_id
                                                                            : (nrf52_device_id_t) { 0 };
    const mac_address_bin_t nrf52_mac_addr  = (NULL != p_nrf52_device_info) ? p_nrf52_device_info->nrf52_mac_addr
                                                                            : settings_read_mac_addr();

    p_init_params->wifi_ap_ssid        = generate_wifi_ap_ssid(nrf52_mac_addr);
    p_init_params->hostname            = generate_hostname(nrf52_mac_addr);
    p_init_params->device_id           = nrf52_device_id;
    p_init_params->esp32_fw_ver        = fw_update_get_cur_version();
    p_init_params->nrf52_fw_ver        = nrf52_fw_ver_get_str(p_nrf52_fw_ver);
    p_init_params->nrf52_mac_addr      = nrf52_mac_addr;
    p_init_params->esp32_mac_addr_wifi = gateway_read_mac_addr(ESP_MAC_WIFI_STA);
    p_init_params->esp32_mac_addr_eth  = gateway_read_mac_addr(ESP_MAC_ETH);
    return p_init_params;
}

static bool
ruuvi_init_gw_cfg(
    const ruuvi_nrf52_fw_ver_t* const p_nrf52_fw_ver,
    const nrf52_device_info_t* const  p_nrf52_device_info)
{
    const gw_cfg_default_init_param_t* p_gw_cfg_default_init_param = ruuvi_generate_init_params(
        p_nrf52_fw_ver,
        p_nrf52_device_info);
    if (NULL == p_gw_cfg_default_init_param)
    {
        return false;
    }

    if (!gw_cfg_default_init(p_gw_cfg_default_init_param, &gw_cfg_default_json_read))
    {
        os_free(p_gw_cfg_default_init_param);
        return false;
    }
    os_free(p_gw_cfg_default_init_param);
    if (NULL != p_nrf52_fw_ver)
    {
        gw_cfg_default_log();
    }

    const gw_cfg_device_info_t* const p_dev_info = gw_cfg_default_device_info();

    LOG_INFO(
        "### Init gw_cfg: device_id=%s, MAC=%s, nRF52_fw_ver=%s, WiFi_AP=%s",
        p_dev_info->nrf52_device_id.str_buf,
        p_dev_info->nrf52_mac_addr.str_buf,
        p_dev_info->nrf52_fw_ver.buf,
        p_dev_info->wifi_ap.ssid_buf);

    gw_cfg_init((NULL != p_nrf52_fw_ver) ? &ruuvi_cb_on_change_cfg : NULL);

    const bool               flag_allow_cfg_updating = (NULL != p_nrf52_fw_ver) ? true : false;
    settings_in_nvs_status_e settings_status         = SETTINGS_IN_NVS_STATUS_NOT_EXIST;

    const gw_cfg_t* p_gw_cfg_tmp = settings_get_from_flash(flag_allow_cfg_updating, &settings_status);
    if (NULL == p_gw_cfg_tmp)
    {
        LOG_ERR("Can't get settings from flash");
        return false;
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
    return true;
}

static void
ruuvi_deinit_gw_cfg(void)
{
    LOG_INFO("### Deinit gw_cfg");
    gw_cfg_deinit();
    gw_cfg_default_deinit();
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
    LOG_INFO("Read saved hostname: %s", p_hostname->buf);
    const ruuvi_esp32_fw_ver_str_t* const p_fw_ver       = gw_cfg_get_esp32_fw_ver();
    const ruuvi_nrf52_fw_ver_str_t* const p_nrf52_fw_ver = gw_cfg_get_nrf52_fw_ver();

    wifiman_hostinfo_t hostinfo = { 0 };
    (void)snprintf(hostinfo.hostname.buf, sizeof(hostinfo.hostname.buf), "%s", p_hostname->buf);
    (void)snprintf(hostinfo.fw_ver.buf, sizeof(hostinfo.fw_ver.buf), "%s", p_fw_ver->buf);
    (void)snprintf(hostinfo.nrf52_fw_ver.buf, sizeof(hostinfo.nrf52_fw_ver.buf), "%s", p_nrf52_fw_ver->buf);

    const wifiman_config_t* const p_wifi_cfg             = wifi_manager_default_config_init(p_wifi_ap_ssid, &hostinfo);
    const bool                    flag_connect_sta_false = false;
    if (!wifi_init(flag_connect_sta_false, p_wifi_cfg, fw_update_get_current_fatfs_gwui_partition_name()))
    {
        LOG_ERR("%s failed", "wifi_init");
        return;
    }
    start_wifi_ap();
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
    main_task_send_sig_deactivate_cfg_mode();
}

static void
handle_reset_button_is_pressed_during_boot(void)
{
    LOG_INFO("Reset button is pressed during boot - erase settings in flash");

    const mac_address_bin_t mac_addr = settings_read_mac_addr();

    ruuvi_nvs_deinit();
    ruuvi_nvs_erase();
    ruuvi_nvs_init();

    gw_cfg_storage_deinit_erase_init();

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
    if (gw_status_is_waiting_auto_cfg_by_wps())
    {
        main_task_send_sig_deactivate_cfg_mode();
    }
}

static bool
main_task_initial_initialization(void)
{
    reset_info_init();
    cjson_wrap_init();
    partition_table_update_init_mutex();

    g_http_server_mutex_incoming_connection = os_mutex_create_static(&g_http_server_mutex_incoming_connection_mem);

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

    return true;
}

static bool
main_task_init(void)
{
    esp_log_level_set(TAG, ESP_LOG_INFO);

    if (!main_task_initial_initialization())
    {
        return false;
    }

    const bool is_configure_button_pressed = (0 == gpio_get_level(RB_BUTTON_RESET_PIN)) ? true : false;
    nrf52fw_hw_reset_nrf52(true);
    leds_init(is_configure_button_pressed);

    if (!reset_task_init())
    {
        LOG_ERR("Can't create thread");
        return false;
    }

    ruuvi_nvs_init();
    ruuvi_nvs_init_gw_cfg_storage();

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

    if (!ruuvi_init_gw_cfg(NULL, NULL))
    {
        LOG_ERR("%s failed", "ruuvi_init_gw_cfg");
        return false;
    }

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

    adv_mqtt_init();
    adv_post_init();
    terminal_open(NULL, true, NRF52_COMM_TASK_PRIORITY);
    api_process(true);
    const nrf52_device_info_t nrf52_device_info = ruuvi_device_id_request_and_wait();

    ruuvi_deinit_gw_cfg();
    ruuvi_init_gw_cfg(&nrf52_fw_ver, &nrf52_device_info);
    LOG_INFO("### Config is empty: %s", gw_cfg_is_empty() ? "true" : "false");

    hmac_sha256_set_key_for_http_ruuvi(gw_cfg_get_nrf52_device_id()->str_buf);  // set default encryption key
    hmac_sha256_set_key_for_http_custom(gw_cfg_get_nrf52_device_id()->str_buf); // set default encryption key
    hmac_sha256_set_key_for_stats(gw_cfg_get_nrf52_device_id()->str_buf);       // set default encryption key

    main_task_subscribe_events();

    time_task_init();

    const force_start_wifi_hotspot_e force_start_wifi_hotspot = settings_read_flag_force_start_wifi_hotspot();

    const gw_cfg_t* p_gw_cfg = gw_cfg_lock_ro();
    if (!network_subsystem_init(force_start_wifi_hotspot, p_gw_cfg))
    {
        LOG_ERR("%s failed", "network_subsystem_init");
        gw_cfg_unlock_ro(&p_gw_cfg);
        return false;
    }
    gw_cfg_unlock_ro(&p_gw_cfg);

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
            // http_post_advs or mqtt_event_handler after successful network communication with the server.
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
