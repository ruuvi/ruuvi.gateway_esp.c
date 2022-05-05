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
#include "time_task.h"
#include "event_mgr.h"

#define LOG_LOCAL_LEVEL LOG_LEVEL_DEBUG
#include "log.h"

static const char TAG[] = "ruuvi_gateway";

#define MAIN_TASK_LOG_HEAP_USAGE_PERIOD_SECONDS (10U)

#define MAIN_TASK_TIMEOUT_AFTER_MANUAL_HOTSPOT_ACTIVATION_SEC (60)

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
    MAIN_TASK_SIG_TASK_WATCHDOG_FEED                  = OS_SIGNAL_NUM_9,
    MAIN_TASK_SIG_TASK_RECONNECT_NETWORK              = OS_SIGNAL_NUM_10,
} main_task_sig_e;

#define MAIN_TASK_SIG_FIRST (MAIN_TASK_SIG_LOG_HEAP_USAGE)
#define MAIN_TASK_SIG_LAST  (MAIN_TASK_SIG_TASK_RECONNECT_NETWORK)

static os_signal_t *                  g_p_signal_main_task;
static os_signal_static_t             g_signal_main_task_mem;
static os_timer_sig_periodic_t *      g_p_timer_sig_log_heap_usage;
static os_timer_sig_periodic_static_t g_timer_sig_log_heap_usage;
static os_timer_sig_one_shot_t *      g_p_timer_sig_check_for_fw_updates;
static os_timer_sig_one_shot_static_t g_timer_sig_check_for_fw_updates_mem;
static os_timer_sig_one_shot_t *      g_p_timer_sig_deactivate_wifi_ap;
static os_timer_sig_one_shot_static_t g_p_timer_sig_deactivate_wifi_ap_mem;
static os_timer_sig_periodic_t *      g_p_timer_sig_check_for_remote_cfg;
static os_timer_sig_periodic_static_t g_timer_sig_check_for_remote_cfg_mem;
static os_timer_sig_periodic_t *      g_p_timer_sig_task_watchdog_feed;
static os_timer_sig_periodic_static_t g_timer_sig_task_watchdog_feed_mem;
static event_mgr_ev_info_static_t     g_main_loop_ev_info_mem_wifi_connected;
static event_mgr_ev_info_static_t     g_main_loop_ev_info_mem_eth_connected;

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

static const char *
get_wday_if_set_in_bitmask(const auto_update_weekdays_bitmask_t auto_update_weekdays_bitmask, const os_time_wday_e wday)
{
    if (0 != (auto_update_weekdays_bitmask & (1U << (uint32_t)wday)))
    {
        return os_time_wday_name_mid(wday);
    }
    return "";
}

static bool
check_if_checking_for_fw_updates_allowed2(const ruuvi_gw_cfg_auto_update_t *const p_cfg_auto_update)
{
    const time_t unix_time = os_time_get();
    time_t       cur_time  = (time_t)(
        unix_time
        + ((int32_t)p_cfg_auto_update->auto_update_tz_offset_hours
           * (TIME_UNITS_MINUTES_PER_HOUR * TIME_UNITS_SECONDS_PER_MINUTE)));
    struct tm tm_time = { 0 };
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
    const gw_cfg_t *p_gw_cfg = gw_cfg_lock_ro();

    const bool res = check_if_checking_for_fw_updates_allowed2(&p_gw_cfg->ruuvi_cfg.auto_update);

    gw_cfg_unlock_ro(&p_gw_cfg);
    return res;
}

static void
main_task_handle_sig_log_heap_usage(void)
{
    const uint32_t free_heap = esp_get_free_heap_size();
    LOG_INFO("free heap: %lu", (printf_ulong_t)free_heap);
    if (free_heap < (RUUVI_FREE_HEAP_LIM_KIB * RUUVI_NUM_BYTES_IN_1KB))
    {
        // TODO: in ESP-IDF v4.x there is API heap_caps_register_failed_alloc_callback,
        //       which allows to catch 'no memory' event and reboot.
        LOG_ERR(
            "Only %uKiB of free memory left - probably due to a memory leak. Reboot the Gateway.",
            (printf_uint_t)(free_heap / RUUVI_NUM_BYTES_IN_1KB));
        esp_restart();
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
    wifi_manager_stop_ap();
    if (gw_cfg_get_eth_use_eth() || (!wifi_manager_is_sta_configured()))
    {
        ethernet_start(gw_cfg_get_wifi_ap_ssid()->ssid_buf);
    }
    else
    {
        wifi_manager_connect_async();
    }

    main_task_configure_periodic_remote_cfg_check();
    leds_indication_network_no_connection();
}

static void
main_task_handle_sig_check_for_remote_cfg(void)
{
    LOG_INFO("Check for remote_cfg: activate");
    http_server_user_req(HTTP_SERVER_USER_REQ_CODE_DOWNLOAD_GW_CFG);
}

static void
main_task_handle_sig_network_connected(void)
{
    LOG_INFO("Handle event: NETWORK_CONNECTED");
    gw_cfg_remote_refresh_interval_minutes_t remote_cfg_refresh_interval_minutes = 0;
    const bool  flag_use_remote_cfg = gw_cfg_get_remote_cfg_use(&remote_cfg_refresh_interval_minutes);
    static bool g_flag_initial_request_for_remote_cfg_performed = false;
    if (flag_use_remote_cfg && !g_flag_initial_request_for_remote_cfg_performed)
    {
        g_flag_initial_request_for_remote_cfg_performed = true;
        LOG_INFO("Activate checking for remote cfg");
        os_signal_send(g_p_signal_main_task, main_task_conv_to_sig_num(MAIN_TASK_SIG_CHECK_FOR_REMOTE_CFG));
    }
}

static void
main_task_handle_sig_task_watchdog_feed(void)
{
    const esp_err_t err = esp_task_wdt_reset();
    if (ESP_OK != err)
    {
        LOG_ERR_ESP(err, "%s failed", "esp_task_wdt_reset");
    }
}

static void
main_task_handle_sig_task_network_reconnect(void)
{
    LOG_INFO("Perform network reconnect");
    if (gw_cfg_get_eth_use_eth())
    {
        ethernet_stop();
        ethernet_start(gw_cfg_get_wifi_ap_ssid()->ssid_buf);
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
            restart_services_internal();
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
        case MAIN_TASK_SIG_TASK_WATCHDOG_FEED:
            main_task_handle_sig_task_watchdog_feed();
            break;
        case MAIN_TASK_SIG_TASK_RECONNECT_NETWORK:
            main_task_handle_sig_task_network_reconnect();
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
        LOG_INFO("Reading of the configuration from the remote server is not active");
        os_timer_sig_periodic_stop(g_p_timer_sig_check_for_remote_cfg);
    }
}

ATTR_NORETURN
void
main_loop(void)
{
    LOG_INFO("Main loop started");
    main_wdt_add_and_start();

    os_timer_sig_periodic_start(g_p_timer_sig_log_heap_usage);

    main_task_configure_periodic_remote_cfg_check();

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
    os_signal_add(g_p_signal_main_task, main_task_conv_to_sig_num(MAIN_TASK_SIG_TASK_WATCHDOG_FEED));
    os_signal_add(g_p_signal_main_task, main_task_conv_to_sig_num(MAIN_TASK_SIG_TASK_RECONNECT_NETWORK));
}

void
main_task_init_timers(void)
{
    g_p_timer_sig_log_heap_usage = os_timer_sig_periodic_create_static(
        &g_timer_sig_log_heap_usage,
        "log_heap_usage",
        g_p_signal_main_task,
        main_task_conv_to_sig_num(MAIN_TASK_SIG_LOG_HEAP_USAGE),
        pdMS_TO_TICKS(MAIN_TASK_LOG_HEAP_USAGE_PERIOD_SECONDS * TIME_UNITS_MS_PER_SECOND));
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
        pdMS_TO_TICKS(MAIN_TASK_TIMEOUT_AFTER_MANUAL_HOTSPOT_ACTIVATION_SEC * TIME_UNITS_MS_PER_SECOND));

    g_p_timer_sig_check_for_remote_cfg = os_timer_sig_periodic_create_static(
        &g_timer_sig_check_for_remote_cfg_mem,
        "remote_cfg",
        g_p_signal_main_task,
        main_task_conv_to_sig_num(MAIN_TASK_SIG_CHECK_FOR_REMOTE_CFG),
        pdMS_TO_TICKS(60U * TIME_UNITS_MINUTES_PER_HOUR * TIME_UNITS_MS_PER_SECOND));

    g_p_timer_sig_task_watchdog_feed = os_timer_sig_periodic_create_static(
        &g_timer_sig_task_watchdog_feed_mem,
        "main_wgod",
        g_p_signal_main_task,
        main_task_conv_to_sig_num(MAIN_TASK_SIG_TASK_WATCHDOG_FEED),
        pdMS_TO_TICKS(CONFIG_ESP_TASK_WDT_TIMEOUT_S * TIME_UNITS_MS_PER_SECOND / 3U));

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
    os_signal_send(g_p_signal_main_task, main_task_conv_to_sig_num(MAIN_TASK_SIG_TASK_RECONNECT_NETWORK));
}

void
main_task_send_sig_mqtt_publish_connect(void)
{
    os_signal_send(g_p_signal_main_task, main_task_conv_to_sig_num(MAIN_TASK_SIG_MQTT_PUBLISH_CONNECT));
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
main_task_stop_timer_check_for_remote_cfg(void)
{
    LOG_INFO("Stop timer: Check for remote cfg");
    os_timer_sig_periodic_stop(g_p_timer_sig_check_for_remote_cfg);
}