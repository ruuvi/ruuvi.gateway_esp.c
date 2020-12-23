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

#define LOG_LOCAL_LEVEL LOG_LEVEL_DEBUG
#include "log.h"

#define TM_YEAR_BASE (1900)
#define TM_YEAR_MIN  (2020)

#define RUUVI_STACK_SIZE_TIME_TASK (3U * 1024U)

#define TIME_TASK_BUF_SIZE_TIME_STR (20)

typedef enum time_task_sig_e
{
    TIME_TASK_SIG_WIFI_CONNECTED    = OS_SIGNAL_NUM_0,
    TIME_TASK_SIG_WIFI_DISCONNECTED = OS_SIGNAL_NUM_1,
    TIME_TASK_SIG_ETH_CONNECTED     = OS_SIGNAL_NUM_2,
    TIME_TASK_SIG_ETH_DISCONNECTED  = OS_SIGNAL_NUM_3,
} time_task_sig_e;

#define TIME_TASK_SIG_FIRST (TIME_TASK_SIG_WIFI_CONNECTED)
#define TIME_TASK_SIG_LAST  (TIME_TASK_SIG_ETH_DISCONNECTED)

static os_signal_static_t g_time_task_signal_mem;
static os_signal_t *      gp_time_task_signal;

static os_task_stack_type_t g_time_task_stack_mem[RUUVI_STACK_SIZE_TIME_TASK];
static os_task_static_t     g_time_task_mem;

static os_task_handle_t gp_time_task;
static time_t           g_time_min_valid;

static bool g_time_is_synchronized;

static const char TAG[] = "TIME";

static time_t
time_task_get_min_valid_time(void)
{
    struct tm tm_2020_01_01 = {
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
    return mktime(&tm_2020_01_01);
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
    }
}

static void
time_task_handle_sig(const time_task_sig_e time_task_sig)
{
    switch (time_task_sig)
    {
        case TIME_TASK_SIG_WIFI_CONNECTED:
        case TIME_TASK_SIG_ETH_CONNECTED:
            LOG_INFO("Activate SNTP time synchronization");
            g_time_is_synchronized = false;
            sntp_init();
            break;
        case TIME_TASK_SIG_WIFI_DISCONNECTED:
        case TIME_TASK_SIG_ETH_DISCONNECTED:
            LOG_INFO("Deactivate SNTP time synchronization");
            sntp_stop();
            break;
        default:
            LOG_ERR("Unhanded sig: %d", (int)time_task_sig);
            assert(0);
            break;
    }
}

ATTR_NORETURN
static void
time_task_thread(void)
{
    LOG_INFO("time_task started");
    os_signal_register_cur_thread(gp_time_task_signal);
    for (;;)
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
            time_task_handle_sig(time_task_sig);
        }
    }
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

bool
time_task_init(void)
{
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

    event_mgr_subscribe_sig(
        EVENT_MGR_EV_WIFI_CONNECTED,
        gp_time_task_signal,
        time_task_conv_to_sig_num(TIME_TASK_SIG_WIFI_CONNECTED));
    event_mgr_subscribe_sig(
        EVENT_MGR_EV_WIFI_DISCONNECTED,
        gp_time_task_signal,
        time_task_conv_to_sig_num(TIME_TASK_SIG_WIFI_DISCONNECTED));
    event_mgr_subscribe_sig(
        EVENT_MGR_EV_ETH_CONNECTED,
        gp_time_task_signal,
        time_task_conv_to_sig_num(TIME_TASK_SIG_ETH_CONNECTED));
    event_mgr_subscribe_sig(
        EVENT_MGR_EV_ETH_DISCONNECTED,
        gp_time_task_signal,
        time_task_conv_to_sig_num(TIME_TASK_SIG_ETH_DISCONNECTED));

    sntp_setoperatingmode(SNTP_OPMODE_POLL);
    sntp_set_sync_mode(SNTP_SYNC_MODE_SMOOTH);
    u8_t server_idx = 0;
    sntp_setservername(server_idx, "time.google.com");
    server_idx += 1;
    sntp_setservername(server_idx, "time.cloudflare.com");
    server_idx += 1;
    sntp_setservername(server_idx, "time.nist.gov");
    server_idx += 1;
    sntp_setservername(server_idx, "pool.ntp.org");
    sntp_set_time_sync_notification_cb(time_task_cb_notification_on_sync);

    g_time_min_valid = time_task_get_min_valid_time();

    const UBaseType_t task_priority = 1;
    if (!os_task_create_static_without_param(
            &time_task_thread,
            "time_task",
            g_time_task_stack_mem,
            RUUVI_STACK_SIZE_TIME_TASK,
            task_priority,
            &g_time_task_mem,
            &gp_time_task))
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
