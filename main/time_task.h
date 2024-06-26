/**
 * @file time_task.h
 * @author Jukka Saari, TheSomeMan
 * @date 2019-11-27
 * @copyright Ruuvi Innovations Ltd, license BSD-3-Clause.
 */

#ifndef RUUVI_TIMETASK_H
#define RUUVI_TIMETASK_H

#include <stdbool.h>
#include <time.h>

#if !defined(RUUVI_TESTS_TIME_TASK)
#define RUUVI_TESTS_TIME_TASK (0)
#endif

#if RUUVI_TESTS_TIME_TASK
#define TIME_TASK_STATIC
#else
#define TIME_TASK_STATIC static
#endif

#ifdef __cplusplus
extern "C" {
#endif

bool
time_task_init(void);

#if RUUVI_TESTS_TIME_TASK
bool
time_task_stop(void);
#endif

bool
time_is_valid(const time_t timestamp);

bool
time_is_synchronized(void);

#ifdef __cplusplus
}
#endif

#endif // RUUVI_TIMETASK_H
