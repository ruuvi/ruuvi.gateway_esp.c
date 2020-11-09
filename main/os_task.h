/**
 * @file os_task.h
 * @author TheSomeMan
 * @date 2020-11-03
 * @copyright Ruuvi Innovations Ltd, license BSD-3-Clause.
 */

#ifndef OS_TASK_H
#define OS_TASK_H

#include <stdbool.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "attribs.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef ATTR_NORETURN void (*os_task_func_t)(void *p_param);
typedef UBaseType_t  os_task_priority_t;
typedef TaskHandle_t os_task_handle_t;

/**
 * Create a new task thread.
 * @param p_func - pointer to the task function
 * @param p_name - pointer to the task name
 * @param stack_depth - the size of the task stack (in bytes)
 * @param p_param - pointer that will be passed as the parameter to the task function
 * @param priority - task priority
 * @param[out] ph_task - pointer to the variable to return task handle
 * @return true if successful
 */
bool
os_task_create(
    os_task_func_t           p_func,
    const char *             p_name,
    const uint32_t           stack_depth,
    void *                   p_param,
    const os_task_priority_t priority,
    os_task_handle_t *       ph_task);

/**
 * Get task name for the current thread.
 * @return pointer to the string with the current task name.
 */
const char *
os_task_get_name(void);

#ifdef __cplusplus
}
#endif

#endif // OS_TASK_H
