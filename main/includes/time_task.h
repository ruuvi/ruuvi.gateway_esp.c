/**
 * @file time_task.h
 * @author Jukka Saari
 * @date 2019-11-27
 * @copyright Ruuvi Innovations Ltd, license BSD-3-Clause.
 */

#ifndef RUUVI_TIMETASK_H
#define RUUVI_TIMETASK_H

#ifdef __cplusplus
extern "C" {
#endif

void
time_sync(void);

void
time_init(void);

void
time_stop(void);

#ifdef __cplusplus
}
#endif

#endif // RUUVI_TIMETASK_H
