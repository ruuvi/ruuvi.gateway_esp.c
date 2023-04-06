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
#include "os_mutex.h"

#define LOG_LOCAL_LEVEL LOG_LEVEL_INFO
#include "log.h"

static const char TAG[] = "runtime_stat";

enum
{
    RUNTIME_STAT_MAX_NUM_TASKS = 24,
};

typedef struct RuntimeStatTaskInfo_t
{
    UBaseType_t task_number;
    uint32_t    runtime_counter;
} RuntimeStatTaskInfo_t;

typedef struct RuntimeStatAccumulatedTaskInfo_t
{
    char     task_name[configMAX_TASK_NAME_LEN];
    uint32_t min_free_stack_size;
} RuntimeStatAccumulatedTaskInfo_t;

typedef uint32_t RuntimeStatTaskInfoIdx_t;

static RuntimeStatAccumulatedTaskInfo_t g_runtime_stat_accumulated[RUNTIME_STAT_MAX_NUM_TASKS];
static os_mutex_t                       g_runtime_stat_accumulated_mutex;
static os_mutex_static_t                g_runtime_stat_accumulated_mutex_mem;

static void
runtime_stat_lock_accumulated_info(void)
{
    if (NULL == g_runtime_stat_accumulated_mutex)
    {
        g_runtime_stat_accumulated_mutex = os_mutex_create_static(&g_runtime_stat_accumulated_mutex_mem);
    }
    os_mutex_lock(g_runtime_stat_accumulated_mutex);
}

static void
runtime_stat_unlock_accumulated_info(void)
{
    os_mutex_unlock(g_runtime_stat_accumulated_mutex);
}

static void
runtime_stat_update_accumulated_info(const char* const p_task_name, const uint32_t free_stack_size)
{
    assert(strlen(p_task_name) < configMAX_TASK_NAME_LEN);
    runtime_stat_lock_accumulated_info();
    for (uint32_t i = 0; i < sizeof(g_runtime_stat_accumulated) / sizeof(g_runtime_stat_accumulated[0]); ++i)
    {
        RuntimeStatAccumulatedTaskInfo_t* const p_info = &g_runtime_stat_accumulated[i];
        if ('\0' == p_info->task_name[0])
        {
            LOG_DBG("Add task: %s", p_task_name);
            (void)snprintf(p_info->task_name, sizeof(p_info->task_name), "%s", p_task_name);
            p_info->min_free_stack_size = UINT32_MAX;
        }
        if (0 == strcmp(p_task_name, p_info->task_name))
        {
            if (p_info->min_free_stack_size > free_stack_size)
            {
                LOG_DBG(
                    "Update minimum free stack size for the task %s: %u",
                    p_task_name,
                    (printf_uint_t)free_stack_size);
                p_info->min_free_stack_size = free_stack_size;
            }
            break;
        }
    }
    runtime_stat_unlock_accumulated_info();
}

bool
runtime_stat_for_each_accumulated_info(
    bool (*p_cb)(const char* const p_task_name, const uint32_t min_free_stack_size, void* p_userdata),
    void* p_userdata)
{
    bool res = true;
    runtime_stat_lock_accumulated_info();
    for (uint32_t i = 0; i < sizeof(g_runtime_stat_accumulated) / sizeof(g_runtime_stat_accumulated[0]); ++i)
    {
        RuntimeStatAccumulatedTaskInfo_t* const p_info = &g_runtime_stat_accumulated[i];
        if ('\0' != p_info->task_name[0])
        {
            if (!p_cb(p_info->task_name, p_info->min_free_stack_size, p_userdata))
            {
                res = false;
                break;
            }
        }
    }
    runtime_stat_unlock_accumulated_info();
    return res;
}

static RuntimeStatTaskInfoIdx_t
runtime_stat_find_task_info(
    const RuntimeStatTaskInfo_t* const p_arr_of_tasks,
    const uint32_t                     max_num_tasks,
    const UBaseType_t                  task_num)
{
    for (RuntimeStatTaskInfoIdx_t i = 0; i < max_num_tasks; ++i)
    {
        const RuntimeStatTaskInfo_t* const p_task_info = &p_arr_of_tasks[i];
        if (task_num == p_task_info->task_number)
        {
            return i;
        }
    }
    return UINT32_MAX;
}

static RuntimeStatTaskInfoIdx_t
runtime_stat_add_task_info(
    RuntimeStatTaskInfo_t* const p_arr_of_tasks,
    const uint32_t               max_num_tasks,
    const UBaseType_t            task_num,
    const uint32_t               runtime_counter)
{
    for (RuntimeStatTaskInfoIdx_t i = 0; i < max_num_tasks; ++i)
    {
        RuntimeStatTaskInfo_t* const p_task_info = &p_arr_of_tasks[i];
        if (0 == p_task_info->task_number)
        {
            p_task_info->task_number     = task_num;
            p_task_info->runtime_counter = runtime_counter;
            return i;
        }
    }
    return UINT32_MAX;
}

static void
runtime_stat_remove_deleted_tasks_info(
    RuntimeStatTaskInfo_t* const p_arr_of_tasks,
    const uint32_t               max_num_tasks,
    const uint32_t               task_info_bit_mask)
{
    for (RuntimeStatTaskInfoIdx_t i = 0; i < max_num_tasks; ++i)
    {
        RuntimeStatTaskInfo_t* const p_task_info = &p_arr_of_tasks[i];
        if (0 == (task_info_bit_mask & (1U << i)))
        {
            p_task_info->task_number     = 0;
            p_task_info->runtime_counter = 0;
        }
    }
}

void
log_runtime_statistics(void)
{
    static RuntimeStatTaskInfo_t g_tasks_info[RUNTIME_STAT_MAX_NUM_TASKS] = { 0 };

    TaskStatus_t* p_arr_of_tasks = os_calloc(RUNTIME_STAT_MAX_NUM_TASKS, sizeof(*p_arr_of_tasks));
    if (NULL == p_arr_of_tasks)
    {
        LOG_ERR("Can't allocate memory");
        return;
    }

    static uint32_t prev_total_time = 0;
    uint32_t        total_time      = 0;

    const uint32_t num_tasks = uxTaskGetSystemState(p_arr_of_tasks, RUNTIME_STAT_MAX_NUM_TASKS, &total_time);

    const uint32_t total_time_delta = total_time - prev_total_time;
    prev_total_time                 = total_time;

    LOG_INFO(
        "======== totalTime %8u ==============================================================",
        (printf_uint_t)total_time_delta);
    LOG_INFO("|  #  |    Task name     | Pri | Stat|    cnt    |  %%  |        Stack       | Stack free |");
    LOG_INFO("|-----|------------------|-----|-----|-----------|-----|--------------------|------------|");

    if (num_tasks == 0)
    {
        LOG_ERR("The arr of tasks is too small");
        os_free(p_arr_of_tasks);
        return;
    }
    uint32_t task_info_bit_mask = 0;
    for (uint32_t task_idx = 0; task_idx < num_tasks; task_idx++)
    {
        TaskStatus_t* p_task_status = &p_arr_of_tasks[task_idx];

        RuntimeStatTaskInfoIdx_t task_info_idx = runtime_stat_find_task_info(
            g_tasks_info,
            sizeof(g_tasks_info) / sizeof(*g_tasks_info),
            p_task_status->xTaskNumber);

        RuntimeStatTaskInfo_t* const p_task_info = (task_info_idx < sizeof(g_tasks_info) / sizeof(*g_tasks_info))
                                                       ? &g_tasks_info[task_info_idx]
                                                       : NULL;

        const uint32_t runtime_counter_delta = p_task_status->ulRunTimeCounter
                                               - ((NULL != p_task_info) ? p_task_info->runtime_counter : 0);
        if (NULL != p_task_info)
        {
            p_task_info->runtime_counter = p_task_status->ulRunTimeCounter;
        }
        else
        {
            task_info_idx = runtime_stat_add_task_info(
                g_tasks_info,
                sizeof(g_tasks_info) / sizeof(*g_tasks_info),
                p_task_status->xTaskNumber,
                p_task_status->ulRunTimeCounter);
        }
        if (UINT32_MAX != task_info_idx)
        {
            task_info_bit_mask |= 1U << task_info_idx;
        }

        const UBaseType_t stack_size = uxTaskGetStackSize(p_task_status->xHandle);

        const uint32_t percentage_of_cpu_usage = (total_time_delta != 0)
                                                     ? (runtime_counter_delta * 100 + total_time_delta / 2)
                                                           / total_time_delta
                                                     : UINT32_MAX;

        const char* p_task_status_str = "";
        switch (p_task_status->eCurrentState)
        {
            case eReady:
                p_task_status_str = "Rdy";
                break;
            case eBlocked:
                p_task_status_str = "Blk";
                break;
            case eSuspended:
                p_task_status_str = "Sus";
                break;
            case eDeleted:
                p_task_status_str = "Del";
                break;
            default:
                p_task_status_str = "???";
                break;
        }

        const size_t task_name_len = strlen(p_task_status->pcTaskName);
        if (UINT32_MAX == percentage_of_cpu_usage)
        {
            LOG_INFO(
                "|%4u | %s%*s | %3u | %s |%10u |  -  |%5d of %5u (%2u%%)|    %5u   |",
                (printf_uint_t)p_task_status->xTaskNumber,
                p_task_status->pcTaskName,
                16 - task_name_len,
                "",
                (printf_uint_t)p_task_status->uxBasePriority,
                p_task_status_str,
                runtime_counter_delta,
                (printf_int_t)(stack_size - p_task_status->usStackHighWaterMark),
                (printf_uint_t)stack_size,
                (printf_uint_t)((stack_size - p_task_status->usStackHighWaterMark) * 100 / stack_size),
                (printf_uint_t)p_task_status->usStackHighWaterMark);
        }
        else if ((percentage_of_cpu_usage > 0UL) || (0 == runtime_counter_delta))
        {
            LOG_INFO(
                "|%4u | %s%*s | %3u | %s |%10u |%3u%% |%5d of %5u (%2u%%)|    %5u   |",
                (printf_uint_t)p_task_status->xTaskNumber,
                p_task_status->pcTaskName,
                16 - task_name_len,
                "",
                (printf_uint_t)p_task_status->uxBasePriority,
                p_task_status_str,
                (printf_uint_t)runtime_counter_delta,
                (printf_uint_t)percentage_of_cpu_usage,
                (printf_int_t)(stack_size - p_task_status->usStackHighWaterMark),
                (printf_uint_t)stack_size,
                (printf_uint_t)((stack_size - p_task_status->usStackHighWaterMark) * 100 / stack_size),
                (printf_uint_t)p_task_status->usStackHighWaterMark);
        }
        else
        {
            /* If the percentage is zero here then the task has consumed less than 1% of the total run time. */
            LOG_INFO(
                "|%4u | %s%*s | %3u | %s |%10u | <1%% |%5d of %5u (%2u%%)|    %5u   |",
                (printf_uint_t)p_task_status->xTaskNumber,
                p_task_status->pcTaskName,
                16 - task_name_len,
                "",
                (printf_uint_t)p_task_status->uxBasePriority,
                p_task_status_str,
                runtime_counter_delta,
                (printf_int_t)(stack_size - p_task_status->usStackHighWaterMark),
                (printf_uint_t)stack_size,
                (printf_uint_t)((stack_size - p_task_status->usStackHighWaterMark) * 100 / stack_size),
                (printf_uint_t)p_task_status->usStackHighWaterMark);
        }
        runtime_stat_update_accumulated_info(p_task_status->pcTaskName, p_task_status->usStackHighWaterMark);
    }
    runtime_stat_remove_deleted_tasks_info(
        g_tasks_info,
        sizeof(g_tasks_info) / sizeof(*g_tasks_info),
        task_info_bit_mask);
    LOG_INFO("==========================================================================================");
    os_free(p_arr_of_tasks);
}
