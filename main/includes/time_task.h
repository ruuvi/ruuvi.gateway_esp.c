/**
 * @file time_task.h
 * @author Jukka Saari
 * @date 2019-11-27
 * @copyright Ruuvi Innovations Ltd, license BSD-3-Clause.
 */

#ifndef RUUVI_TIMETASK_H
#define RUUVI_TIMETASK_H

#include <stdbool.h>
#include <time.h>

#ifdef __cplusplus
extern "C" {
#endif

void
time_task_sync_time(void);

void
time_task_init(void);

void
time_task_stop(void);

bool
time_is_valid(const time_t timestamp);

#ifdef __cplusplus
}
#endif

#endif // RUUVI_TIMETASK_H
