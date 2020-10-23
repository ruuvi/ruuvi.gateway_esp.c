/**
 * @file time.c
 * @author Jukka Saari
 * @date 2019-11-27
 * @copyright Ruuvi Innovations Ltd, license BSD-3-Clause.
 */

#include "esp_log.h"
#include "esp_sntp.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/task.h"
#include "lwip/err.h"
#include "sntp.h"
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <sys/time.h>
#include <time.h>
#include "attribs.h"
#include "time_units.h"

#define TIME_SYNC_BIT (1 << 0)

#define TM_YEAR_BASE (1900)
#define TM_YEAR_MIN  (2020)

static TaskHandle_t task_time        = NULL;
EventGroupHandle_t  time_event_group = NULL;
EventBits_t         uxBits;
static const char   TAG[] = "time";

static time_t
wait_for_sntp(void)
{
    // wait for time to be set
    time_t    now         = 0;
    struct tm timeinfo    = { 0 };
    int       retry       = 0;
    const int retry_count = 20;

    while (timeinfo.tm_year < (TM_YEAR_MIN - TM_YEAR_BASE))
    {
        retry += 1;
        if (retry >= retry_count)
        {
            break;
        }
        ESP_LOGI(TAG, "Waiting for system time to be set... (%d/%d)", retry, retry_count);
        const uint32_t delay_ms = 3000;
        vTaskDelay(pdMS_TO_TICKS(delay_ms));
        time(&now);
        localtime_r(&now, &timeinfo);
    }

    if (timeinfo.tm_year > (TM_YEAR_MIN - TM_YEAR_BASE))
    {
        return now;
    }
    else
    {
        return 0;
    }
}

static void
sync_sntp(void)
{
    ESP_LOGI(TAG, "Synchronizing SNTP");
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
    time_t now = wait_for_sntp();
    sntp_stop();

    if (now)
    {
        char      strftime_buf[64];
        struct tm timeinfo = { 0 };
        localtime_r(&now, &timeinfo);
        strftime(strftime_buf, sizeof(strftime_buf), "%c", &timeinfo);
        ESP_LOGI(TAG, "SNTP Synchronized: %s", strftime_buf);
    }
    else
    {
        ESP_LOGW(TAG, "SNTP not synchronized");
    }
}

void
time_sync(void)
{
    xEventGroupSetBits(time_event_group, TIME_SYNC_BIT);
}

ATTR_NORETURN
static void
time_task(void *param)
{
    (void)param;
    if (!time_event_group)
    {
        time_event_group = xEventGroupCreate();
    }

    for (;;)
    {
        /**
         EventBits_t xEventGroupWaitBits(
                                    const EventGroupHandle_t xEventGroup,
                                    const EventBits_t uxBitsToWaitFor,
                                    const BaseType_t xClearOnExit,
                                    const BaseType_t xWaitForAllBits,
                                    TickType_t xTicksToWait );
         */
        // Wait for trigger command or 24 hours to synch time.
        xEventGroupWaitBits(
            time_event_group,
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
    const uint32_t stack_size = 3 * 1024;
    xTaskCreate(&time_task, "time_task", stack_size, NULL, 1, &task_time);
}

void
time_stop(void)
{
    sntp_stop();

    if (task_time)
    {
        vTaskDelete(task_time);
        vEventGroupDelete(time_event_group);
        time_event_group = NULL;
    }
}