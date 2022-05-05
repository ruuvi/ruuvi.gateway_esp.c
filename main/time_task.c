/**
 * @file time.c
 * @author Jukka Saari, TheSomeMan
 * @date 2019-11-27
 * @copyright Ruuvi Innovations Ltd, license BSD-3-Clause.
 */

#include "time_task.h"
#include <time.h>
#include <assert.h>
#include "os_task.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "esp_sntp.h"
#include "attribs.h"
#include "time_units.h"
#include "os_time.h"
#include "os_signal.h"
#include "esp_type_wrapper.h"
#include "event_mgr.h"
#include "os_mkgmtime.h"
#include "gw_cfg.h"
#include "ruuvi_gateway.h"

#define LOG_LOCAL_LEVEL LOG_LEVEL_DEBUG
#include "log.h"

#define TM_YEAR_BASE (1900)
#define TM_YEAR_MIN  (2021)

#define RUUVI_STACK_SIZE_TIME_TASK (3U * 1024U)

#define TIME_TASK_BUF_SIZE_TIME_STR (20)

#define TIME_TASK_NUM_OF_TIME_SERVERS (4)

#define TIME_TASK_NAME "time_task"

typedef enum time_task_sig_e
{
    TIME_TASK_SIG_WIFI_CONNECTED       = OS_SIGNAL_NUM_0,
    TIME_TASK_SIG_WIFI_DISCONNECTED    = OS_SIGNAL_NUM_1,
    TIME_TASK_SIG_ETH_CONNECTED        = OS_SIGNAL_NUM_2,
    TIME_TASK_SIG_ETH_DISCONNECTED     = OS_SIGNAL_NUM_3,
    TIME_TASK_SIG_GW_CFG_CHANGED_RUUVI = OS_SIGNAL_NUM_4,
    TIME_TASK_SIG_STOP                 = OS_SIGNAL_NUM_5,
} time_task_sig_e;

#define TIME_TASK_SIG_FIRST (TIME_TASK_SIG_WIFI_CONNECTED)
#define TIME_TASK_SIG_LAST  (TIME_TASK_SIG_STOP)

static os_signal_static_t g_time_task_signal_mem;
static os_signal_t *      gp_time_task_signal;

static os_task_stack_type_t g_time_task_stack_mem[RUUVI_STACK_SIZE_TIME_TASK];
static os_task_static_t     g_time_task_mem;

static event_mgr_ev_info_static_t g_time_task_ev_info_mem_wifi_connected;
static event_mgr_ev_info_static_t g_time_task_ev_info_mem_wifi_disconnected;
static event_mgr_ev_info_static_t g_time_task_ev_info_mem_eth_connected;
static event_mgr_ev_info_static_t g_time_task_ev_info_mem_eth_disconnected;
static event_mgr_ev_info_static_t g_time_task_ev_info_mem_gw_cfg_changed_ruuvi;

static time_t g_time_min_valid;

static bool g_time_is_synchronized;

static bool                               g_time_task_ntp_use;
static bool                               g_time_task_ntp_use_dhcp;
static ruuvi_gw_cfg_ntp_server_addr_str_t g_arr_of_time_servers[TIME_TASK_NUM_OF_TIME_SERVERS];

static const char TAG[] = "TIME";

static void
time_task_configure_ntp_sources(void);

static time_t
time_task_get_min_valid_time(void)
{
    struct tm tm_2021_01_01 = {
        .tm_sec   = 0,
        .tm_min   = 0,
        .tm_hour  = 0,
        .tm_mday  = 1,
        .tm_mon   = 0,
        .tm_year  = TM_YEAR_MIN - TM_YEAR_BASE,
        .tm_wday  = 0,
        .tm_yday  = 0,
        .tm_isdst = -1,
    };

    return os_mkgmtime(&tm_2021_01_01);
}

bool
time_is_valid(const time_t timestamp)
{
    if (timestamp < g_time_min_valid)
    {
        return false;
    }
    return true;
}

bool
time_is_synchronized(void)
{
    return g_time_is_synchronized;
}

ATTR_PURE
static os_signal_num_e
time_task_conv_to_sig_num(const time_task_sig_e sig)
{
    return (os_signal_num_e)sig;
}

static time_task_sig_e
time_task_conv_from_sig_num(const os_signal_num_e sig_num)
{
    assert(((os_signal_num_e)TIME_TASK_SIG_FIRST <= sig_num) && (sig_num <= (os_signal_num_e)TIME_TASK_SIG_LAST));
    return (time_task_sig_e)sig_num;
}

static void
time_task_cb_notification_on_sync(struct timeval *p_tv)
{
    struct tm tm_time = { 0 };
    gmtime_r(&p_tv->tv_sec, &tm_time);
    char buf_time_str[TIME_TASK_BUF_SIZE_TIME_STR];
    strftime(buf_time_str, sizeof(buf_time_str), "%Y-%m-%d %H:%M:%S", &tm_time);
    if (p_tv->tv_sec < g_time_min_valid)
    {
        LOG_WARN(
            "Time has been synchronized but timestamp is bad: %s.%03u",
            buf_time_str,
            (printf_uint_t)(p_tv->tv_usec / 1000));
        g_time_is_synchronized = false;
    }
    else
    {
        LOG_INFO("Time has been synchronized: %s.%03u", buf_time_str, (printf_uint_t)(p_tv->tv_usec / 1000));
        g_time_is_synchronized = true;
        if (SNTP_SYNC_MODE_IMMED == sntp_get_sync_mode())
        {
            LOG_INFO("Switch time sync mode to SMOOTH");
            sntp_set_sync_mode(SNTP_SYNC_MODE_SMOOTH);
        }
        event_mgr_notify(EVENT_MGR_EV_TIME_SYNCHRONIZED);
    }
}

static void
time_task_sntp_start(void)
{
    LOG_INFO("Activate SNTP time synchronization");
    g_time_is_synchronized = false;
    sntp_init();
}

static void
time_task_sntp_stop(void)
{
    LOG_INFO("Deactivate SNTP time synchronization");
    g_time_is_synchronized = false;
    sntp_stop();
}

static void
time_task_on_cfg_changed(void)
{
    time_task_sntp_stop();
    LOG_INFO("Reconfigure SNTP");
    const bool flag_prev_ntp_use      = g_time_task_ntp_use;
    const bool flag_prev_ntp_use_dhcp = g_time_task_ntp_use_dhcp;
    time_task_configure_ntp_sources();
    if ((flag_prev_ntp_use != g_time_task_ntp_use) || (flag_prev_ntp_use_dhcp != g_time_task_ntp_use_dhcp))
    {
        main_task_send_sig_reconnect_network();
    }
    if (g_time_task_ntp_use)
    {
        time_task_sntp_start();
    }
}

static bool
time_task_handle_sig(const time_task_sig_e time_task_sig)
{
    bool flag_stop_task = false;
    switch (time_task_sig)
    {
        case TIME_TASK_SIG_WIFI_CONNECTED:
        case TIME_TASK_SIG_ETH_CONNECTED:
            if (g_time_task_ntp_use)
            {
                time_task_sntp_start();
            }
            break;
        case TIME_TASK_SIG_WIFI_DISCONNECTED:
        case TIME_TASK_SIG_ETH_DISCONNECTED:
            time_task_sntp_stop();
            break;
        case TIME_TASK_SIG_GW_CFG_CHANGED_RUUVI:
            LOG_INFO("Got TIME_TASK_SIG_GW_CFG_CHANGED_RUUVI");
            time_task_on_cfg_changed();
            break;
        case TIME_TASK_SIG_STOP:
            LOG_INFO("Stop time_task");
            flag_stop_task = true;
            break;
        default:
            LOG_ERR("Unhanded sig: %d", (int)time_task_sig);
            assert(0);
            break;
    }
    return flag_stop_task;
}

static void
time_task_destroy_resources(void)
{
    if (NULL != gp_time_task_signal)
    {
        os_signal_delete(&gp_time_task_signal);
    }
    g_time_min_valid = 0;
}

static void
time_task_thread(void)
{
    LOG_INFO("time_task started");
    os_signal_register_cur_thread(gp_time_task_signal);
    bool flag_stop_task = false;
    while (!flag_stop_task)
    {
        os_signal_events_t sig_events = { 0 };
        os_signal_wait(gp_time_task_signal, &sig_events);
        for (;;)
        {
            const os_signal_num_e sig_num = os_signal_num_get_next(&sig_events);
            if (OS_SIGNAL_NUM_NONE == sig_num)
            {
                break;
            }
            const time_task_sig_e time_task_sig = time_task_conv_from_sig_num(sig_num);
            flag_stop_task                      = time_task_handle_sig(time_task_sig);
        }
    }
    os_signal_unregister_cur_thread(gp_time_task_signal);
}

static bool
time_task_configure_signals(void)
{
    for (uint32_t sig_num = TIME_TASK_SIG_FIRST; sig_num <= TIME_TASK_SIG_LAST; ++sig_num)
    {
        if (!os_signal_add(gp_time_task_signal, (os_signal_num_e)sig_num))
        {
            return false;
        }
    }
    return true;
}

static void
time_task_sntp_add_ntp_server(const uint32_t server_idx, const ruuvi_gw_cfg_ntp_server_addr_str_t *const p_ntp_srv_addr)
{
    if ((NULL != p_ntp_srv_addr) && ('\0' != p_ntp_srv_addr->buf[0]))
    {
        LOG_INFO("Add time server %u: %s", (printf_uint_t)server_idx, p_ntp_srv_addr->buf);
        sntp_setservername((u8_t)server_idx, p_ntp_srv_addr->buf);
    }
    else
    {
        LOG_INFO("Add time server %u: NULL", (printf_uint_t)server_idx);
        sntp_setserver((u8_t)server_idx, NULL);
    }
}

static void
time_task_add_ntp_server(
    ruuvi_gw_cfg_ntp_server_addr_str_t *const       p_arr_of_time_servers,
    const ruuvi_gw_cfg_ntp_server_addr_str_t *const p_ntp_srv_addr,
    uint32_t *const                                 p_srv_idx)
{
    if ('\0' != p_ntp_srv_addr->buf[0])
    {
        p_arr_of_time_servers[*p_srv_idx] = *p_ntp_srv_addr;
        *p_srv_idx += 1;
    }
}

static void
time_task_save_settings(void)
{
    const gw_cfg_t *p_gw_cfg = gw_cfg_lock_ro();

    g_time_task_ntp_use      = p_gw_cfg->ruuvi_cfg.ntp.ntp_use;
    g_time_task_ntp_use_dhcp = p_gw_cfg->ruuvi_cfg.ntp.ntp_use_dhcp;

    for (uint32_t i = 0; i < TIME_TASK_NUM_OF_TIME_SERVERS; ++i)
    {
        g_arr_of_time_servers[i].buf[0] = '\0';
    }
    uint32_t server_idx = 0;
    time_task_add_ntp_server(g_arr_of_time_servers, &p_gw_cfg->ruuvi_cfg.ntp.ntp_server1, &server_idx);
    time_task_add_ntp_server(g_arr_of_time_servers, &p_gw_cfg->ruuvi_cfg.ntp.ntp_server2, &server_idx);
    time_task_add_ntp_server(g_arr_of_time_servers, &p_gw_cfg->ruuvi_cfg.ntp.ntp_server3, &server_idx);
    time_task_add_ntp_server(g_arr_of_time_servers, &p_gw_cfg->ruuvi_cfg.ntp.ntp_server4, &server_idx);

    gw_cfg_unlock_ro(&p_gw_cfg);
}

static void
time_task_configure_ntp_sources(void)
{
    time_task_save_settings();

    for (int32_t i = 0; i < SNTP_MAX_SERVERS; ++i)
    {
        sntp_setserver(i, NULL);
    }

    if (g_time_task_ntp_use)
    {
        if (g_time_task_ntp_use_dhcp)
        {
            LOG_INFO("Configure SNTP to use DHCP");
            sntp_servermode_dhcp(1);
        }
        else
        {
            LOG_INFO("Configure SNTP to not use DHCP");
            sntp_servermode_dhcp(0);
        }
    }

    if (g_time_task_ntp_use && !g_time_task_ntp_use_dhcp)
    {
        for (uint32_t i = 0; i < TIME_TASK_NUM_OF_TIME_SERVERS; ++i)
        {
            time_task_sntp_add_ntp_server(i, &g_arr_of_time_servers[i]);
        }
    }
}

bool
time_task_init(void)
{
    time_task_save_settings();

    if (NULL != gp_time_task_signal)
    {
        LOG_ERR("time_task was already initialized");
        return false;
    }

    gp_time_task_signal = os_signal_create_static(&g_time_task_signal_mem);
    if (!time_task_configure_signals())
    {
        LOG_ERR("%s failed", "time_task_configure_signals");
        time_task_destroy_resources();
        return false;
    }

    event_mgr_subscribe_sig_static(
        &g_time_task_ev_info_mem_wifi_connected,
        EVENT_MGR_EV_WIFI_CONNECTED,
        gp_time_task_signal,
        time_task_conv_to_sig_num(TIME_TASK_SIG_WIFI_CONNECTED));
    event_mgr_subscribe_sig_static(
        &g_time_task_ev_info_mem_wifi_disconnected,
        EVENT_MGR_EV_WIFI_DISCONNECTED,
        gp_time_task_signal,
        time_task_conv_to_sig_num(TIME_TASK_SIG_WIFI_DISCONNECTED));
    event_mgr_subscribe_sig_static(
        &g_time_task_ev_info_mem_eth_connected,
        EVENT_MGR_EV_ETH_CONNECTED,
        gp_time_task_signal,
        time_task_conv_to_sig_num(TIME_TASK_SIG_ETH_CONNECTED));
    event_mgr_subscribe_sig_static(
        &g_time_task_ev_info_mem_eth_disconnected,
        EVENT_MGR_EV_ETH_DISCONNECTED,
        gp_time_task_signal,
        time_task_conv_to_sig_num(TIME_TASK_SIG_ETH_DISCONNECTED));
    event_mgr_subscribe_sig_static(
        &g_time_task_ev_info_mem_gw_cfg_changed_ruuvi,
        EVENT_MGR_EV_GW_CFG_CHANGED_RUUVI,
        gp_time_task_signal,
        time_task_conv_to_sig_num(TIME_TASK_SIG_GW_CFG_CHANGED_RUUVI));

    sntp_setoperatingmode(SNTP_OPMODE_POLL);
    LOG_INFO("Set time sync mode to IMMED");
    sntp_set_sync_mode(SNTP_SYNC_MODE_IMMED);

    time_task_configure_ntp_sources();

    sntp_set_time_sync_notification_cb(time_task_cb_notification_on_sync);

    g_time_min_valid = time_task_get_min_valid_time();

    const UBaseType_t task_priority = 1;
    if (!os_task_create_static_finite_without_param(
            &time_task_thread,
            "time_task",
            g_time_task_stack_mem,
            RUUVI_STACK_SIZE_TIME_TASK,
            task_priority,
            &g_time_task_mem))
    {
        LOG_ERR("Can't create thread");
        time_task_destroy_resources();
        return false;
    }
    while (!os_signal_is_any_thread_registered(gp_time_task_signal))
    {
        os_task_delay(1);
    }
    return true;
}

#if RUUVI_TESTS_TIME_TASK
bool
time_task_stop(void)
{
    if (!os_signal_send(gp_time_task_signal, time_task_conv_to_sig_num(TIME_TASK_SIG_STOP)))
    {
        LOG_ERR("Can't send signal to stop thread");
        return false;
    }
    bool             result = false;
    const TickType_t t0     = xTaskGetTickCount();
    for (;;)
    {
        TaskHandle_t p_task = xTaskGetHandle(TIME_TASK_NAME);
        if (NULL == p_task)
        {
            result = true;
            time_task_destroy_resources();
            break;
        }
        if ((xTaskGetTickCount() - t0) > pdMS_TO_TICKS(1000))
        {
            break;
        }
        vTaskDelay(1);
    }
    return result;
}
#endif
