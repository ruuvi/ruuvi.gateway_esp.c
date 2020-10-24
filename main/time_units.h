/**
 * @file time_units.h
 * @author TheSomeMan
 * @date 2020-10-23
 * @copyright Ruuvi Innovations Ltd, license BSD-3-Clause.
 */

#ifndef RUUVI_TIME_UNITS_H
#define RUUVI_TIME_UNITS_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define TIME_UNITS_MS_PER_SECOND (1000U)
#define TIME_UNITS_US_PER_MS     (1000U)

#define TIME_UNITS_HOURS_PER_DAY      (24U)
#define TIME_UNITS_MINUTES_PER_HOUR   (60U)
#define TIME_UNITS_SECONDS_PER_MINUTE (60U)

typedef uint32_t TimeUnitsSeconds_t;
typedef uint32_t TimeUnitsMilliSeconds_t;
typedef uint64_t TimeUnitsMicroSeconds_t;

static inline TimeUnitsMilliSeconds_t
time_units_conv_seconds_to_ms(const TimeUnitsSeconds_t num_seconds)
{
    return num_seconds * TIME_UNITS_MS_PER_SECOND;
}

static inline TimeUnitsMicroSeconds_t
time_units_conv_ms_to_us(const TimeUnitsMilliSeconds_t num_ms)
{
    return (TimeUnitsMicroSeconds_t)num_ms * TIME_UNITS_US_PER_MS;
}

#ifdef __cplusplus
}
#endif

#endif // RUUVI_TIME_UNITS_H
