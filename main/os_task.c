/**
 * @file os_task.c
 * @author TheSomeMan
 * @date 2020-11-03
 * @copyright Ruuvi Innovations Ltd, license BSD-3-Clause.
 */

#include "os_task.h"
#include "log.h"

static const char *TAG = "os_task";

bool
os_task_create(
    os_task_func_t           p_func,
    const char *             p_name,
    const uint32_t           stack_depth,
    void *                   p_param,
    const os_task_priority_t priority,
    os_task_handle_t *       ph_task)
{
    LOG_INFO("Start thread '%s' with priority %d, stack size %u bytes", p_name, priority, stack_depth);
    if (pdPASS != xTaskCreate(p_func, p_name, stack_depth, p_param, priority, ph_task))
    {
        LOG_ERR("Failed to start thread '%s'", p_name);
        return false;
    }
    return true;
}

const char *
os_task_get_name(void)
{
    const char *task_name = pcTaskGetTaskName(NULL);
    if (NULL == task_name)
    {
        task_name = "???";
    }
    return task_name;
}
