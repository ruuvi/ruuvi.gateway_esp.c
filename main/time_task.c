/**
 * @file time.c
 * @author Jukka Saari
 * @date 2019-11-27
 * @copyright Ruuvi Innovations Ltd, license BSD-3-Clause.
 */

#include "time_task.h"
#include "esp_sntp.h"
#include "esp_system.h"
#include "os_task.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "lwip/err.h"
#include "sntp.h"
#include <stdio.h>
#include <string.h>
#include <time.h>
#include "attribs.h"
#include "time_units.h"
#include "log.h"

#define TIME_SYNC_BIT (1U << 0U)

#define TM_YEAR_BASE (1900)
#define TM_YEAR_MIN  (2020)

static TaskHandle_t       gh_time_task        = NULL;
static EventGroupHandle_t gh_time_event_group = NULL;

static time_t             g_time_min;

static const char TAG[] = "time";

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
    if (timestamp < g_time_min)
    {
        return false;
    }
    return true;
}

/**
 * @brief Wait for time to be set.
 */
static void
wait_for_sntp(void)
{
    int32_t       retry       = 0;
    const int32_t retry_count = 20;

    while (!time_is_valid(time(NULL)))
    {
        retry += 1;
        if (retry >= retry_count)
        {
            break;
        }
        LOG_INFO("Waiting for system time to be set... (%d/%d)", retry, retry_count);
        const uint32_t delay_ms = 3000;
        vTaskDelay(pdMS_TO_TICKS(delay_ms));
    }
}

static void
sync_sntp(void)
{
    LOG_INFO("Synchronizing SNTP");
    sntp_setoperatingmode(SNTP_OPMODE_POLL);
    sntp_set_sync_mode(SNTP_SYNC_MODE_SMOOTH);
    u8_t server_idx = 0;
    sntp_setservername(server_idx, "time1.google.com");
    server_idx += 1;
    sntp_setservername(server_idx, "time2.google.com");
    server_idx += 1;
    sntp_setservername(server_idx, "time3.google.com");
    server_idx += 1;
    sntp_setservername(server_idx, "pool.ntp.org");
    sntp_init();
    wait_for_sntp();
    sntp_stop();

    const time_t now = time(NULL);
    if (!time_is_valid(now))
    {
        LOG_WARN("SNTP not synchronized");
        return;
    }

    char      strftime_buf[64];
    struct tm timeinfo = { 0 };
    localtime_r(&now, &timeinfo);
    strftime(strftime_buf, sizeof(strftime_buf), "%c", &timeinfo);
    LOG_INFO("SNTP Synchronized: %s", strftime_buf);
}

void
time_sync(void)
{
    xEventGroupSetBits(gh_time_event_group, TIME_SYNC_BIT);
}

ATTR_NORETURN
static void
time_task(ATTR_UNUSED void *p_param)
{
    if (NULL == gh_time_event_group)
    {
        gh_time_event_group = xEventGroupCreate();
    }

    for (;;)
    {
        // Wait for trigger command or 24 hours to synch time.
        xEventGroupWaitBits(
            gh_time_event_group,
            TIME_SYNC_BIT,
            pdTRUE,
            pdFALSE,
            pdMS_TO_TICKS(
                TIME_UNITS_HOURS_PER_DAY * TIME_UNITS_MINUTES_PER_HOUR * TIME_UNITS_SECONDS_PER_MINUTE
                * TIME_UNITS_MS_PER_SECOND));
        sync_sntp();
    }
}

void
time_init(void)
{
    g_time_min = time_task_get_min_valid_time();

    const uint32_t    stack_size    = 3U * 1024U;
    const UBaseType_t task_priority = 1;
    if (!os_task_create(&time_task, "time_task", stack_size, NULL, task_priority, &gh_time_task))
    {
        LOG_ERR("Can't create thread");
    }
}

void
time_stop(void)
{
    sntp_stop();

    if (NULL != gh_time_task)
    {
        vTaskDelete(gh_time_task);
        vEventGroupDelete(gh_time_event_group);
        gh_time_event_group = NULL;
    }
}
