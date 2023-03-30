/**
 * @file runtime_stat.c
 * @author TheSomeMan
 * @date 2023-03-28
 * @copyright Ruuvi Innovations Ltd, license BSD-3-Clause.
 */

#include "runtime_stat.h"
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos_task_stack_size.h"
#include "os_malloc.h"

#define LOG_LOCAL_LEVEL LOG_LEVEL_INFO
#include "log.h"

static const char TAG[] = "runtime_stat";

extern UBaseType_t
uxTaskGetStackSize(TaskHandle_t xTask);

void
log_runtime_statistics(void)
{
    uint32_t      num_tasks      = 24;
    TaskStatus_t* p_arr_of_tasks = os_calloc(num_tasks, sizeof(*p_arr_of_tasks));
    if (NULL == p_arr_of_tasks)
    {
        LOG_ERR("Can't allocate memory");
        return;
    }

    uint32_t total_time = 0;

    num_tasks = uxTaskGetSystemState(p_arr_of_tasks, num_tasks, &total_time);

    LOG_INFO("======== totalTime %8u =================================================", (printf_uint_t)total_time);
    LOG_INFO("|  #  |    Task name     | Pri | Stat|    cnt    |  %%  |        Stack       |");

    if (num_tasks == 0)
    {
        LOG_ERR("The arr of tasks is too small");
        os_free(p_arr_of_tasks);
        return;
    }
    if (total_time == 0)
    {
        LOG_ERR("Total time is zero");
        os_free(p_arr_of_tasks);
        return;
    }
    for (uint32_t task_idx = 0; task_idx < num_tasks; task_idx++)
    {
        TaskStatus_t*     pTaskStatus = &p_arr_of_tasks[task_idx];
        const UBaseType_t stackSize   = uxTaskGetStackSize(pTaskStatus->xHandle);
        /* What percentage of the total run time has the task used?
         This will always be rounded down to the nearest integer.
         ulTotalRunTimeDiv100 has already been divided by 100. */
        const uint32_t ulStatsAsPercentage = (pTaskStatus->ulRunTimeCounter * 100 + total_time / 2) / total_time;

        const char* taskStatus = "";
        switch (pTaskStatus->eCurrentState)
        {
            case eReady:
                taskStatus = "Rdy";
                break;
            case eBlocked:
                taskStatus = "Blk";
                break;
            case eSuspended:
                taskStatus = "Sus";
                break;
            case eDeleted:
                taskStatus = "Del";
                break;
            default:
                taskStatus = "???";
                break;
        }

        const size_t taskNameLen = strlen(pTaskStatus->pcTaskName);
        if ((ulStatsAsPercentage > 0UL) || (0 == pTaskStatus->ulRunTimeCounter))
        {
            LOG_INFO(
                "|%4u | %s%*s | %3u | %s |%10u |%3u%% |%5d of %5u (%2u%%)|",
                (printf_uint_t)pTaskStatus->xTaskNumber,
                pTaskStatus->pcTaskName,
                16 - taskNameLen,
                "",
                (printf_uint_t)pTaskStatus->uxBasePriority,
                taskStatus,
                (printf_uint_t)pTaskStatus->ulRunTimeCounter,
                (printf_uint_t)ulStatsAsPercentage,
                (printf_int_t)(stackSize - pTaskStatus->usStackHighWaterMark),
                (printf_uint_t)stackSize,
                (printf_uint_t)((stackSize - pTaskStatus->usStackHighWaterMark) * 100 / stackSize));
        }
        else
        {
            /* If the percentage is zero here then the task has
             consumed less than 1% of the total run time. */
            LOG_INFO(
                "|%4u | %s%*s | %3u | %s |%10u | <1%% |%5d of %5u (%2u%%)|",
                (printf_uint_t)pTaskStatus->xTaskNumber,
                pTaskStatus->pcTaskName,
                16 - taskNameLen,
                "",
                (printf_uint_t)pTaskStatus->uxBasePriority,
                taskStatus,
                pTaskStatus->ulRunTimeCounter,
                (printf_int_t)(stackSize - pTaskStatus->usStackHighWaterMark),
                (printf_uint_t)stackSize,
                (printf_uint_t)((stackSize - pTaskStatus->usStackHighWaterMark) * 100 / stackSize));
        }
    }
    LOG_INFO("=============================================================================");
    os_free(p_arr_of_tasks);
}
