/**
 * @file main_loop.c
 * @author TheSomeMan
 * @date 2021-11-29
 * @copyright Ruuvi Innovations Ltd, license BSD-3-Clause.
 */

#include "ruuvi_gateway.h"
#include "esp_task_wdt.h"
#include "os_signal.h"
#include "os_timer_sig.h"
#include "os_time.h"
#include "time_units.h"
#include "wifi_manager.h"
#include "ethernet.h"
#include "leds.h"
#include "mqtt.h"
#include "mdns.h"
#include "time_task.h"
#include "event_mgr.h"
#include "gw_status.h"
#include "os_malloc.h"
#include "gw_cfg.h"
#include "gw_cfg_default.h"
#include "gw_cfg_log.h"
#include "reset_task.h"
#include "runtime_stat.h"

#define LOG_LOCAL_LEVEL LOG_LEVEL_INFO
#include "log.h"

static const char TAG[] = "ruuvi_gateway";

#define MAIN_TASK_LOG_HEAP_STAT_PERIOD_MS              (100U)
#define MAIN_TASK_LOG_HEAP_USAGE_PERIOD_SECONDS        (10U)
#define MAIN_TASK_TIMEOUT_HOTSPOT_DEACTIVATION_SEC     (60)
#define MAIN_TASK_HOTSPOT_DEACTIVATION_SHORT_DELAY_SEC (5)
#define MAIN_TASK_CHECK_FOR_REMOTE_CFG_PERIOD_MS       (60U * TIME_UNITS_SECONDS_PER_MINUTE * TIME_UNITS_MS_PER_SECOND)
#define MAIN_TASK_GET_HISTORY_TIMEOUT_MS               (70U * TIME_UNITS_MS_PER_SECOND)
#define MAIN_TASK_LOG_RUNTIME_STAT_PERIOD_MS           (30 * TIME_UNITS_MS_PER_SECOND)
#define MAIN_TASK_WATCHDOG_FEED_PERIOD_MS              (1 * TIME_UNITS_MS_PER_SECOND)

#define RUUVI_NUM_BYTES_IN_1KB (1024U)

typedef enum main_task_sig_e
{
    MAIN_TASK_SIG_LOG_HEAP_USAGE                      = OS_SIGNAL_NUM_0,
    MAIN_TASK_SIG_CHECK_FOR_FW_UPDATES                = OS_SIGNAL_NUM_1,
    MAIN_TASK_SIG_SCHEDULE_NEXT_CHECK_FOR_FW_UPDATES  = OS_SIGNAL_NUM_2,
    MAIN_TASK_SIG_SCHEDULE_RETRY_CHECK_FOR_FW_UPDATES = OS_SIGNAL_NUM_3,
    MAIN_TASK_SIG_DEACTIVATE_WIFI_AP                  = OS_SIGNAL_NUM_4,
    MAIN_TASK_SIG_TASK_RESTART_SERVICES               = OS_SIGNAL_NUM_5,
    MAIN_TASK_SIG_MQTT_PUBLISH_CONNECT                = OS_SIGNAL_NUM_6,
    MAIN_TASK_SIG_CHECK_FOR_REMOTE_CFG                = OS_SIGNAL_NUM_7,
    MAIN_TASK_SIG_NETWORK_CONNECTED                   = OS_SIGNAL_NUM_8,
    MAIN_TASK_SIG_NETWORK_DISCONNECTED                = OS_SIGNAL_NUM_9,
    MAIN_TASK_SIG_RECONNECT_NETWORK                   = OS_SIGNAL_NUM_10,
    MAIN_TASK_SIG_SET_DEFAULT_CONFIG                  = OS_SIGNAL_NUM_11,
    MAIN_TASK_SIG_ON_GET_HISTORY                      = OS_SIGNAL_NUM_12,
    MAIN_TASK_SIG_ON_GET_HISTORY_TIMEOUT              = OS_SIGNAL_NUM_13,
    MAIN_TASK_SIG_RELAYING_MODE_CHANGED               = OS_SIGNAL_NUM_14,
    MAIN_TASK_SIG_LOG_RUNTIME_STAT                    = OS_SIGNAL_NUM_15,
    MAIN_TASK_SIG_TASK_WATCHDOG_FEED                  = OS_SIGNAL_NUM_16,
} main_task_sig_e;

#define MAIN_TASK_SIG_FIRST (MAIN_TASK_SIG_LOG_HEAP_USAGE)
#define MAIN_TASK_SIG_LAST  (MAIN_TASK_SIG_TASK_WATCHDOG_FEED)

static os_signal_t*                   g_p_signal_main_task;
static os_signal_static_t             g_signal_main_task_mem;
static os_timer_sig_periodic_t*       g_p_timer_sig_log_heap_usage;
static os_timer_sig_periodic_static_t g_timer_sig_log_heap_usage;
static os_timer_sig_periodic_t*       g_p_timer_sig_log_runtime_stat;
static os_timer_sig_periodic_static_t g_timer_sig_log_runtime_stat;
static os_timer_sig_one_shot_t*       g_p_timer_sig_check_for_fw_updates;
static os_timer_sig_one_shot_static_t g_timer_sig_check_for_fw_updates_mem;
static os_timer_sig_one_shot_t*       g_p_timer_sig_deactivate_wifi_ap;
static os_timer_sig_one_shot_static_t g_p_timer_sig_deactivate_wifi_ap_mem;
static os_timer_sig_periodic_t*       g_p_timer_sig_check_for_remote_cfg;
static os_timer_sig_periodic_static_t g_timer_sig_check_for_remote_cfg_mem;
static os_timer_sig_one_shot_t*       g_p_timer_sig_get_history_timeout;
static os_timer_sig_one_shot_static_t g_timer_sig_get_history_timeout_mem;
static os_timer_sig_periodic_t*       g_p_timer_sig_task_watchdog_feed;
static os_timer_sig_periodic_static_t g_timer_sig_task_watchdog_feed_mem;
static event_mgr_ev_info_static_t     g_main_loop_ev_info_mem_wifi_connected;
static event_mgr_ev_info_static_t     g_main_loop_ev_info_mem_eth_connected;
static event_mgr_ev_info_static_t     g_main_loop_ev_info_mem_wifi_disconnected;
static event_mgr_ev_info_static_t     g_main_loop_ev_info_mem_eth_disconnected;
static event_mgr_ev_info_static_t     g_main_loop_ev_info_mem_relaying_mode_changed;

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

static const char*
get_wday_if_set_in_bitmask(const auto_update_weekdays_bitmask_t auto_update_weekdays_bitmask, const os_time_wday_e wday)
{
    if (0 != (auto_update_weekdays_bitmask & (1U << (uint32_t)wday)))
    {
        return os_time_wday_name_mid(wday);
    }
    return "";
}

static bool
check_if_checking_for_fw_updates_allowed2(const ruuvi_gw_cfg_auto_update_t* const p_cfg_auto_update)
{
    const int32_t tz_offset_seconds = (int32_t)p_cfg_auto_update->auto_update_tz_offset_hours
                                      * (int32_t)(TIME_UNITS_MINUTES_PER_HOUR * TIME_UNITS_SECONDS_PER_MINUTE);

    const time_t unix_time = os_time_get();
    time_t       cur_time  = unix_time + tz_offset_seconds;
    struct tm    tm_time   = { 0 };
    gmtime_r(&cur_time, &tm_time);

    if (AUTO_UPDATE_CYCLE_TYPE_MANUAL == p_cfg_auto_update->auto_update_cycle)
    {
        LOG_INFO("Check for fw updates - skip (manual updating mode)");
        return false;
    }

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
    const gw_cfg_t* p_gw_cfg = gw_cfg_lock_ro();

    const bool res = check_if_checking_for_fw_updates_allowed2(&p_gw_cfg->ruuvi_cfg.auto_update);

    gw_cfg_unlock_ro(&p_gw_cfg);
    return res;
}

static void
main_task_handle_sig_log_heap_usage(void)
{
    static uint32_t g_heap_usage_stat_cnt      = 0;
    static uint32_t g_heap_usage_min_free_heap = 0xFFFFFFFFU;
    static uint32_t g_heap_limit_cnt           = 0;

    const uint32_t free_heap = esp_get_free_heap_size();

    g_heap_usage_stat_cnt += 1;
    if (g_heap_usage_stat_cnt
        < ((MAIN_TASK_LOG_HEAP_USAGE_PERIOD_SECONDS * TIME_UNITS_MS_PER_SECOND) / MAIN_TASK_LOG_HEAP_STAT_PERIOD_MS))
    {
        if (free_heap < g_heap_usage_min_free_heap)
        {
            g_heap_usage_min_free_heap = free_heap;
        }
    }
    else
    {
        LOG_INFO("free heap: %lu", (printf_ulong_t)g_heap_usage_min_free_heap);
        if (g_heap_usage_min_free_heap < (RUUVI_FREE_HEAP_LIM_KIB * RUUVI_NUM_BYTES_IN_1KB))
        {
            // TODO: in ESP-IDF v4.x there is API heap_caps_register_failed_alloc_callback,
            //       which allows to catch 'no memory' event and reboot.
            g_heap_limit_cnt += 1;
            if (g_heap_limit_cnt >= 3)
            {
                LOG_ERR(
                    "Only %uKiB of free memory left - probably due to a memory leak. Reboot the Gateway.",
                    (printf_uint_t)(g_heap_usage_min_free_heap / RUUVI_NUM_BYTES_IN_1KB));
                gateway_restart("Low memory");
            }
        }
        else
        {
            g_heap_limit_cnt = 0;
        }
        g_heap_usage_stat_cnt      = 0;
        g_heap_usage_min_free_heap = 0xFFFFFFFFU;
    }
}

static void
main_task_handle_sig_check_for_fw_updates(void)
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
}

static void
main_task_handle_sig_schedule_next_check_for_fw_updates(void)
{
    const os_delta_ticks_t delay_ticks = pdMS_TO_TICKS(RUUVI_CHECK_FOR_FW_UPDATES_DELAY_AFTER_SUCCESS_SECONDS)
                                         * TIME_UNITS_MS_PER_SECOND;
    LOG_INFO(
        "Schedule next check for fw updates (after successful release_info downloading) after %lu seconds (%lu "
        "ticks)",
        (printf_ulong_t)RUUVI_CHECK_FOR_FW_UPDATES_DELAY_AFTER_SUCCESS_SECONDS,
        (printf_ulong_t)delay_ticks);
    os_timer_sig_one_shot_restart(g_p_timer_sig_check_for_fw_updates, delay_ticks);
}

static void
main_task_handle_sig_schedule_retry_check_for_fw_updates(void)
{
    const os_delta_ticks_t delay_ticks = pdMS_TO_TICKS(RUUVI_CHECK_FOR_FW_UPDATES_DELAY_BEFORE_RETRY_SECONDS)
                                         * TIME_UNITS_MS_PER_SECOND;
    LOG_INFO(
        "Schedule a recheck for fw updates after %lu seconds (%lu ticks)",
        (printf_ulong_t)RUUVI_CHECK_FOR_FW_UPDATES_DELAY_BEFORE_RETRY_SECONDS,
        (printf_ulong_t)delay_ticks);
    os_timer_sig_one_shot_restart(g_p_timer_sig_check_for_fw_updates, delay_ticks);
}

static void
main_task_handle_sig_deactivate_wifi_ap(void)
{
    LOG_INFO("MAIN_TASK_SIG_DEACTIVATE_WIFI_AP");
    if (gw_status_get_first_boot_after_cfg_erase() && gw_cfg_is_empty())
    {
        LOG_INFO("Gateway has not configured yet, so don't stop Wi-Fi hotspot, start Ethernet instead");
        ethernet_start();
    }
    else
    {
        wifi_manager_stop_ap();
    }
}

static void
main_task_handle_sig_check_for_remote_cfg(void)
{
    LOG_INFO("Check for remote_cfg: activate");
    http_server_user_req(HTTP_SERVER_USER_REQ_CODE_DOWNLOAD_GW_CFG);
}

static void
start_mdns(void)
{
    esp_err_t err = mdns_init();
    if (ESP_OK != err)
    {
        LOG_ERR_ESP(err, "mdns_init failed");
        return;
    }

    const wifiman_hostname_t* const p_hostname = gw_cfg_get_hostname();

    err = mdns_hostname_set(p_hostname->hostname_buf);
    if (ESP_OK != err)
    {
        LOG_ERR_ESP(err, "mdns_hostname_set failed");
    }
    LOG_INFO("### Start mDNS: Hostname: \"%s\", Instance: \"%s\"", p_hostname->hostname_buf, p_hostname->hostname_buf);
    err = mdns_instance_name_set(p_hostname->hostname_buf);
    if (ESP_OK != err)
    {
        LOG_ERR_ESP(err, "mdns_instance_name_set failed");
    }
    err = mdns_service_add(NULL, "_http", "_tcp", 80, NULL, 0);
    if (ESP_OK != err)
    {
        LOG_ERR_ESP(err, "mdns_service_add failed");
    }
}

static void
stop_mdns(void)
{
    LOG_INFO("### Stop mDNS");
    mdns_free();
}

static void
main_task_handle_sig_network_connected(void)
{
    LOG_INFO("### Handle event: NETWORK_CONNECTED");

    const force_start_wifi_hotspot_e force_start_wifi_hotspot = settings_read_flag_force_start_wifi_hotspot();
    if (FORCE_START_WIFI_HOTSPOT_DISABLED != force_start_wifi_hotspot)
    {
        /* The Wi-Fi access point must be started each time it is rebooted after the configuration has been erased
         * until it is connected to the network. */
        settings_write_flag_force_start_wifi_hotspot(FORCE_START_WIFI_HOTSPOT_DISABLED);
    }

    start_mdns();

    gw_cfg_remote_refresh_interval_minutes_t remote_cfg_refresh_interval_minutes = 0;
    const bool  flag_use_remote_cfg = gw_cfg_get_remote_cfg_use(&remote_cfg_refresh_interval_minutes);
    static bool g_flag_initial_request_for_remote_cfg_performed = false;
    if (flag_use_remote_cfg && (!g_flag_initial_request_for_remote_cfg_performed) && (!wifi_manager_is_ap_active()))
    {
        g_flag_initial_request_for_remote_cfg_performed = true;
        LOG_INFO("Activate checking for remote cfg");
        os_signal_send(g_p_signal_main_task, main_task_conv_to_sig_num(MAIN_TASK_SIG_CHECK_FOR_REMOTE_CFG));
    }
}

static void
main_task_handle_sig_network_disconnected(void)
{
    LOG_INFO("### Handle event: NETWORK_DISCONNECTED");
    stop_mdns();
}

static void
main_task_handle_sig_task_watchdog_feed(void)
{
    LOG_DBG("Feed watchdog");
    const esp_err_t err = esp_task_wdt_reset();
    if (ESP_OK != err)
    {
        LOG_ERR_ESP(err, "%s failed", "esp_task_wdt_reset");
    }
}

static void
main_task_handle_sig_network_reconnect(void)
{
    LOG_INFO("Perform network reconnect");
    if (gw_cfg_get_eth_use_eth())
    {
        ethernet_stop();
        ethernet_start();
    }
    else
    {
        if (wifi_manager_is_connected_to_wifi())
        {
            wifi_manager_disconnect_wifi();
            wifi_manager_connect_async();
        }
    }
}

void
main_task_handle_sig_set_default_config(void)
{
    LOG_INFO("### Set default config");
    gw_cfg_t* p_gw_cfg = os_calloc(1, sizeof(*p_gw_cfg));
    if (NULL == p_gw_cfg)
    {
        LOG_ERR("Can't allocate memory for gw_cfg");
        return;
    }
    gw_cfg_default_get(p_gw_cfg);
    gw_cfg_log(p_gw_cfg, "Gateway SETTINGS", false);
    (void)gw_cfg_update(p_gw_cfg);
    os_free(p_gw_cfg);
}

static void
main_task_handle_sig_restart_services(void)
{
    LOG_INFO("Restart services");
    mqtt_app_stop();
    if (gw_cfg_get_mqtt_use_mqtt() && gw_status_is_relaying_via_mqtt_enabled())
    {
        mqtt_app_start_with_gw_cfg();
    }

    main_task_configure_periodic_remote_cfg_check();

    if (AUTO_UPDATE_CYCLE_TYPE_MANUAL != gw_cfg_get_auto_update_cycle())
    {
        const os_delta_ticks_t delay_ticks = pdMS_TO_TICKS(RUUVI_CHECK_FOR_FW_UPDATES_DELAY_BEFORE_RETRY_SECONDS)
                                             * TIME_UNITS_MS_PER_SECOND;
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

static void
main_task_handle_sig_relaying_mode_changed(void)
{
    LOG_INFO("Relaying mode changed");

    if (gw_cfg_get_mqtt_use_mqtt())
    {
        if (gw_status_is_relaying_via_mqtt_enabled())
        {
            if (!gw_status_is_mqtt_started())
            {
                mqtt_app_start_with_gw_cfg();
            }
        }
        else
        {
            mqtt_app_stop();
        }
    }
    else
    {
        mqtt_app_stop();
    }
    gw_status_clear_mqtt_relaying_cmd();
    main_task_send_sig_log_runtime_stat();
}

static void
main_task_handle_sig(const main_task_sig_e main_task_sig)
{
    switch (main_task_sig)
    {
        case MAIN_TASK_SIG_LOG_HEAP_USAGE:
            main_task_handle_sig_log_heap_usage();
            break;
        case MAIN_TASK_SIG_CHECK_FOR_FW_UPDATES:
            main_task_handle_sig_check_for_fw_updates();
            break;
        case MAIN_TASK_SIG_SCHEDULE_NEXT_CHECK_FOR_FW_UPDATES:
            main_task_handle_sig_schedule_next_check_for_fw_updates();
            break;
        case MAIN_TASK_SIG_SCHEDULE_RETRY_CHECK_FOR_FW_UPDATES:
            main_task_handle_sig_schedule_retry_check_for_fw_updates();
            break;
        case MAIN_TASK_SIG_DEACTIVATE_WIFI_AP:
            main_task_handle_sig_deactivate_wifi_ap();
            break;
        case MAIN_TASK_SIG_TASK_RESTART_SERVICES:
            main_task_handle_sig_restart_services();
            break;
        case MAIN_TASK_SIG_MQTT_PUBLISH_CONNECT:
            mqtt_publish_connect();
            break;
        case MAIN_TASK_SIG_CHECK_FOR_REMOTE_CFG:
            main_task_handle_sig_check_for_remote_cfg();
            break;
        case MAIN_TASK_SIG_NETWORK_CONNECTED:
            main_task_handle_sig_network_connected();
            break;
        case MAIN_TASK_SIG_NETWORK_DISCONNECTED:
            main_task_handle_sig_network_disconnected();
            break;
        case MAIN_TASK_SIG_RECONNECT_NETWORK:
            main_task_handle_sig_network_reconnect();
            break;
        case MAIN_TASK_SIG_RELAYING_MODE_CHANGED:
            main_task_handle_sig_relaying_mode_changed();
            break;
        case MAIN_TASK_SIG_SET_DEFAULT_CONFIG:
            main_task_handle_sig_set_default_config();
            break;
        case MAIN_TASK_SIG_ON_GET_HISTORY:
            LOG_INFO("MAIN_TASK_SIG_ON_GET_HISTORY");
            os_timer_sig_one_shot_stop(g_p_timer_sig_get_history_timeout);
            os_timer_sig_one_shot_start(g_p_timer_sig_get_history_timeout);
            leds_notify_http_poll_ok();
            break;
        case MAIN_TASK_SIG_ON_GET_HISTORY_TIMEOUT:
            LOG_INFO("MAIN_TASK_SIG_ON_GET_HISTORY_TIMEOUT");
            leds_notify_http_poll_timeout();
            break;
        case MAIN_TASK_SIG_LOG_RUNTIME_STAT:
            log_runtime_statistics();
            break;
        case MAIN_TASK_SIG_TASK_WATCHDOG_FEED:
            main_task_handle_sig_task_watchdog_feed();
            break;
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

void
main_task_configure_periodic_remote_cfg_check(void)
{
    gw_cfg_remote_refresh_interval_minutes_t remote_cfg_refresh_interval_minutes = 0;

    const bool flag_use_remote_cfg = gw_cfg_get_remote_cfg_use(&remote_cfg_refresh_interval_minutes);
    if (flag_use_remote_cfg)
    {
        if (0 != remote_cfg_refresh_interval_minutes)
        {
            LOG_INFO(
                "Reading of the configuration from the remote server is active, period: %u minutes",
                (printf_uint_t)remote_cfg_refresh_interval_minutes);
            os_timer_sig_periodic_restart(
                g_p_timer_sig_check_for_remote_cfg,
                pdMS_TO_TICKS(
                    remote_cfg_refresh_interval_minutes * TIME_UNITS_SECONDS_PER_MINUTE * TIME_UNITS_MS_PER_SECOND));
        }
        else
        {
            LOG_WARN("Reading of the configuration from the remote server is active, but period is not set");
            os_timer_sig_periodic_stop(g_p_timer_sig_check_for_remote_cfg);
        }
    }
    else
    {
        LOG_INFO("### Reading of the configuration from the remote server is not active");
        os_timer_sig_periodic_stop(g_p_timer_sig_check_for_remote_cfg);
    }
}

ATTR_NORETURN
void
main_loop(void)
{
    LOG_INFO("Main loop started");
    main_wdt_add_and_start();

    if (gw_cfg_get_mqtt_use_mqtt())
    {
        mqtt_app_start_with_gw_cfg();
    }

    os_timer_sig_periodic_start(g_p_timer_sig_log_heap_usage);
    os_timer_sig_periodic_start(g_p_timer_sig_log_runtime_stat);
    os_timer_sig_one_shot_start(g_p_timer_sig_get_history_timeout);

    main_task_configure_periodic_remote_cfg_check();

    if (AUTO_UPDATE_CYCLE_TYPE_MANUAL != gw_cfg_get_auto_update_cycle())
    {
        LOG_INFO(
            "### Firmware auto-updating is active, run next check after %lu seconds",
            (printf_ulong_t)RUUVI_CHECK_FOR_FW_UPDATES_DELAY_AFTER_REBOOT_SECONDS);
        os_timer_sig_one_shot_start(g_p_timer_sig_check_for_fw_updates);
    }
    else
    {
        LOG_INFO("Firmware auto-updating is not active");
    }

    main_task_send_sig_log_runtime_stat();

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
main_task_init_signals(void)
{
    g_p_signal_main_task = os_signal_create_static(&g_signal_main_task_mem);
    os_signal_add(g_p_signal_main_task, main_task_conv_to_sig_num(MAIN_TASK_SIG_LOG_HEAP_USAGE));
    os_signal_add(g_p_signal_main_task, main_task_conv_to_sig_num(MAIN_TASK_SIG_CHECK_FOR_FW_UPDATES));
    os_signal_add(g_p_signal_main_task, main_task_conv_to_sig_num(MAIN_TASK_SIG_SCHEDULE_NEXT_CHECK_FOR_FW_UPDATES));
    os_signal_add(g_p_signal_main_task, main_task_conv_to_sig_num(MAIN_TASK_SIG_SCHEDULE_RETRY_CHECK_FOR_FW_UPDATES));
    os_signal_add(g_p_signal_main_task, main_task_conv_to_sig_num(MAIN_TASK_SIG_DEACTIVATE_WIFI_AP));
    os_signal_add(g_p_signal_main_task, main_task_conv_to_sig_num(MAIN_TASK_SIG_TASK_RESTART_SERVICES));
    os_signal_add(g_p_signal_main_task, main_task_conv_to_sig_num(MAIN_TASK_SIG_MQTT_PUBLISH_CONNECT));
    os_signal_add(g_p_signal_main_task, main_task_conv_to_sig_num(MAIN_TASK_SIG_CHECK_FOR_REMOTE_CFG));
    os_signal_add(g_p_signal_main_task, main_task_conv_to_sig_num(MAIN_TASK_SIG_NETWORK_CONNECTED));
    os_signal_add(g_p_signal_main_task, main_task_conv_to_sig_num(MAIN_TASK_SIG_NETWORK_DISCONNECTED));
    os_signal_add(g_p_signal_main_task, main_task_conv_to_sig_num(MAIN_TASK_SIG_RECONNECT_NETWORK));
    os_signal_add(g_p_signal_main_task, main_task_conv_to_sig_num(MAIN_TASK_SIG_SET_DEFAULT_CONFIG));
    os_signal_add(g_p_signal_main_task, main_task_conv_to_sig_num(MAIN_TASK_SIG_ON_GET_HISTORY));
    os_signal_add(g_p_signal_main_task, main_task_conv_to_sig_num(MAIN_TASK_SIG_ON_GET_HISTORY_TIMEOUT));
    os_signal_add(g_p_signal_main_task, main_task_conv_to_sig_num(MAIN_TASK_SIG_RELAYING_MODE_CHANGED));
    os_signal_add(g_p_signal_main_task, main_task_conv_to_sig_num(MAIN_TASK_SIG_LOG_RUNTIME_STAT));
    os_signal_add(g_p_signal_main_task, main_task_conv_to_sig_num(MAIN_TASK_SIG_TASK_WATCHDOG_FEED));
}

void
main_task_init_timers(void)
{
    g_p_timer_sig_log_heap_usage = os_timer_sig_periodic_create_static(
        &g_timer_sig_log_heap_usage,
        "log_heap_usage",
        g_p_signal_main_task,
        main_task_conv_to_sig_num(MAIN_TASK_SIG_LOG_HEAP_USAGE),
        pdMS_TO_TICKS(MAIN_TASK_LOG_HEAP_STAT_PERIOD_MS));
    g_p_timer_sig_check_for_fw_updates = os_timer_sig_one_shot_create_static(
        &g_timer_sig_check_for_fw_updates_mem,
        "check_fw_updates",
        g_p_signal_main_task,
        main_task_conv_to_sig_num(MAIN_TASK_SIG_CHECK_FOR_FW_UPDATES),
        pdMS_TO_TICKS(RUUVI_CHECK_FOR_FW_UPDATES_DELAY_AFTER_REBOOT_SECONDS) * TIME_UNITS_MS_PER_SECOND);

    g_p_timer_sig_deactivate_wifi_ap = os_timer_sig_one_shot_create_static(
        &g_p_timer_sig_deactivate_wifi_ap_mem,
        "deactivate_ap",
        g_p_signal_main_task,
        main_task_conv_to_sig_num(MAIN_TASK_SIG_DEACTIVATE_WIFI_AP),
        pdMS_TO_TICKS(MAIN_TASK_TIMEOUT_HOTSPOT_DEACTIVATION_SEC * TIME_UNITS_MS_PER_SECOND));

    g_p_timer_sig_check_for_remote_cfg = os_timer_sig_periodic_create_static(
        &g_timer_sig_check_for_remote_cfg_mem,
        "remote_cfg",
        g_p_signal_main_task,
        main_task_conv_to_sig_num(MAIN_TASK_SIG_CHECK_FOR_REMOTE_CFG),
        pdMS_TO_TICKS(MAIN_TASK_CHECK_FOR_REMOTE_CFG_PERIOD_MS));

    g_p_timer_sig_get_history_timeout = os_timer_sig_one_shot_create_static(
        &g_timer_sig_get_history_timeout_mem,
        "main_hist",
        g_p_signal_main_task,
        main_task_conv_to_sig_num(MAIN_TASK_SIG_ON_GET_HISTORY_TIMEOUT),
        pdMS_TO_TICKS(MAIN_TASK_GET_HISTORY_TIMEOUT_MS));

    g_p_timer_sig_log_runtime_stat = os_timer_sig_periodic_create_static(
        &g_timer_sig_log_runtime_stat,
        "log_runtime_stat",
        g_p_signal_main_task,
        main_task_conv_to_sig_num(MAIN_TASK_SIG_LOG_RUNTIME_STAT),
        pdMS_TO_TICKS(MAIN_TASK_LOG_RUNTIME_STAT_PERIOD_MS));

    g_p_timer_sig_task_watchdog_feed = os_timer_sig_periodic_create_static(
        &g_timer_sig_task_watchdog_feed_mem,
        "main_wgod",
        g_p_signal_main_task,
        main_task_conv_to_sig_num(MAIN_TASK_SIG_TASK_WATCHDOG_FEED),
        pdMS_TO_TICKS(MAIN_TASK_WATCHDOG_FEED_PERIOD_MS));
}

void
main_task_subscribe_events(void)
{
    event_mgr_subscribe_sig_static(
        &g_main_loop_ev_info_mem_wifi_connected,
        EVENT_MGR_EV_WIFI_CONNECTED,
        g_p_signal_main_task,
        main_task_conv_to_sig_num(MAIN_TASK_SIG_NETWORK_CONNECTED));

    event_mgr_subscribe_sig_static(
        &g_main_loop_ev_info_mem_eth_connected,
        EVENT_MGR_EV_ETH_CONNECTED,
        g_p_signal_main_task,
        main_task_conv_to_sig_num(MAIN_TASK_SIG_NETWORK_CONNECTED));

    event_mgr_subscribe_sig_static(
        &g_main_loop_ev_info_mem_wifi_disconnected,
        EVENT_MGR_EV_WIFI_DISCONNECTED,
        g_p_signal_main_task,
        main_task_conv_to_sig_num(MAIN_TASK_SIG_NETWORK_DISCONNECTED));

    event_mgr_subscribe_sig_static(
        &g_main_loop_ev_info_mem_eth_disconnected,
        EVENT_MGR_EV_ETH_DISCONNECTED,
        g_p_signal_main_task,
        main_task_conv_to_sig_num(MAIN_TASK_SIG_NETWORK_DISCONNECTED));

    event_mgr_subscribe_sig_static(
        &g_main_loop_ev_info_mem_relaying_mode_changed,
        EVENT_MGR_EV_RELAYING_MODE_CHANGED,
        g_p_signal_main_task,
        main_task_conv_to_sig_num(MAIN_TASK_SIG_RELAYING_MODE_CHANGED));
}

bool
main_loop_init(void)
{
    main_task_init_signals();
    if (!os_signal_register_cur_thread(g_p_signal_main_task))
    {
        LOG_ERR("%s failed", "os_signal_register_cur_thread");
        return false;
    }
    return true;
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

void
main_task_send_sig_restart_services(void)
{
    os_signal_send(g_p_signal_main_task, main_task_conv_to_sig_num(MAIN_TASK_SIG_TASK_RESTART_SERVICES));
}

void
main_task_send_sig_reconnect_network(void)
{
    os_signal_send(g_p_signal_main_task, main_task_conv_to_sig_num(MAIN_TASK_SIG_RECONNECT_NETWORK));
}

void
main_task_send_sig_set_default_config(void)
{
    os_signal_send(g_p_signal_main_task, main_task_conv_to_sig_num(MAIN_TASK_SIG_SET_DEFAULT_CONFIG));
}

void
main_task_send_sig_mqtt_publish_connect(void)
{
    os_signal_send(g_p_signal_main_task, main_task_conv_to_sig_num(MAIN_TASK_SIG_MQTT_PUBLISH_CONNECT));
}

void
main_task_send_sig_log_runtime_stat(void)
{
    os_timer_sig_periodic_simulate(g_p_timer_sig_log_runtime_stat);
}

void
main_task_timer_sig_check_for_fw_updates_restart(const os_delta_ticks_t delay_ticks)
{
    os_timer_sig_one_shot_restart(g_p_timer_sig_check_for_fw_updates, delay_ticks);
}

void
main_task_timer_sig_check_for_fw_updates_stop(void)
{
    os_timer_sig_one_shot_stop(g_p_timer_sig_check_for_fw_updates);
}

void
main_task_start_timer_hotspot_deactivation(void)
{
    LOG_INFO("### Start Wi-Fi AP deactivation timer (%u seconds)", MAIN_TASK_TIMEOUT_HOTSPOT_DEACTIVATION_SEC);
    os_timer_sig_one_shot_stop(g_p_timer_sig_deactivate_wifi_ap);
    os_timer_sig_one_shot_restart(
        g_p_timer_sig_deactivate_wifi_ap,
        pdMS_TO_TICKS(MAIN_TASK_TIMEOUT_HOTSPOT_DEACTIVATION_SEC * TIME_UNITS_MS_PER_SECOND));
}

void
main_task_stop_timer_hotspot_deactivation(void)
{
    LOG_INFO("### Stop Wi-Fi AP deactivation timer");
    os_timer_sig_one_shot_stop(g_p_timer_sig_deactivate_wifi_ap);
}

bool
main_task_is_active_timer_hotspot_deactivation(void)
{
    return os_timer_sig_one_shot_is_active(g_p_timer_sig_deactivate_wifi_ap);
}

void
main_task_stop_wifi_hotspot_after_short_delay(void)
{
    LOG_INFO("### Force stop WiFi hotspot after %u seconds", MAIN_TASK_HOTSPOT_DEACTIVATION_SHORT_DELAY_SEC);
    os_timer_sig_one_shot_stop(g_p_timer_sig_deactivate_wifi_ap);
    os_timer_sig_one_shot_restart(
        g_p_timer_sig_deactivate_wifi_ap,
        pdMS_TO_TICKS(MAIN_TASK_HOTSPOT_DEACTIVATION_SHORT_DELAY_SEC * TIME_UNITS_MS_PER_SECOND));
}

void
main_task_stop_timer_check_for_remote_cfg(void)
{
    LOG_INFO("Stop timer: Check for remote cfg");
    os_timer_sig_periodic_stop(g_p_timer_sig_check_for_remote_cfg);
}

void
main_task_on_get_history(void)
{
    os_signal_send(g_p_signal_main_task, main_task_conv_to_sig_num(MAIN_TASK_SIG_ON_GET_HISTORY));
}
