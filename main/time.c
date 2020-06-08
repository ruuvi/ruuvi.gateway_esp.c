#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <time.h>
#include <sys/time.h>
#include "esp_log.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "lwip/err.h"
#include "esp_sntp.h"
#include "sntp.h"

#define TIME_SYNC_BIT (1<<0)

static TaskHandle_t task_time = NULL;
EventGroupHandle_t time_event_group = NULL;
EventBits_t uxBits;
static const char TAG[] = "time";

static time_t wait_for_sntp (void)
{
    // wait for time to be set
    time_t now = 0;
    struct tm timeinfo = { 0 };
    int retry = 0;
    const int retry_count = 20;

    while (timeinfo.tm_year < (2016 - 1900) && ++retry < retry_count)
    {
        ESP_LOGI (TAG, "Waiting for system time to be set... (%d/%d)", retry, retry_count);
        vTaskDelay (3000 / portTICK_PERIOD_MS);
        time (&now);
        localtime_r (&now, &timeinfo);
    }

    if (timeinfo.tm_year > (2016 - 1900))
    {
        return now;
    }
    else
    {
        return 0;
    }
}

static void sync_sntp (void)
{
    ESP_LOGI (TAG, "Synchronizing SNTP");
    sntp_setoperatingmode (SNTP_OPMODE_POLL);
    sntp_set_sync_mode (SNTP_SYNC_MODE_SMOOTH);
    sntp_setservername (0, "time1.google.com");
    sntp_setservername (1, "time2.google.com");
    sntp_setservername (2, "time3.google.com");
    sntp_setservername (3, "pool.ntp.org");
    sntp_init();
    time_t now =  wait_for_sntp();
    sntp_stop();

    if (now)
    {
        char strftime_buf[64];
        struct tm timeinfo = { 0 };
        localtime_r (&now, &timeinfo);
        strftime (strftime_buf, sizeof (strftime_buf), "%c", &timeinfo);
        ESP_LOGI (TAG, "SNTP Synchronized: %s", strftime_buf);
    }
    else
    {
        ESP_LOGW (TAG, "SNTP not synchronized");
    }
}

void time_sync (void)
{
    xEventGroupSetBits (time_event_group, TIME_SYNC_BIT);
}

void time_task (void * param)
{
    if (!time_event_group) { time_event_group = xEventGroupCreate(); }

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
        xEventGroupWaitBits (time_event_group, TIME_SYNC_BIT, pdTRUE, pdFALSE,
                             (24 * 60 * 60 * 1000) / portTICK_PERIOD_MS);
        sync_sntp();
    }
}

void time_init()
{
    xTaskCreate (time_task, "time_task", 1024 * 2, NULL, 1, &task_time);
}

void time_stop()
{
    sntp_stop();

    if (task_time)
    {
        vTaskDelete (task_time);
        vEventGroupDelete (time_event_group);
        time_event_group = NULL;
    }
}