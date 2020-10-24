/**
 * @file leds.h
 * @author Jukka Saari
 * @date 2019-11-27
 * @copyright Ruuvi Innovations Ltd, license BSD-3-Clause.
 */

#ifndef RUUVI_LEDS_H
#define RUUVI_LEDS_H

#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "../time_units.h"

#ifdef __cplusplus
extern "C" {
#endif

#define LEDS_ON_BIT    (1U << 0U)
#define LEDS_OFF_BIT   (1U << 1U)
#define LEDS_BLINK_BIT (1U << 2U)

#define LEDS_SLOW_BLINK   1000U
#define LEDS_MEDIUM_BLINK 500U
#define LEDS_FAST_BLINK   200U

extern EventGroupHandle_t led_bits;

void
leds_on(void);

void
leds_off(void);

void
leds_start_blink(const TimeUnitsMilliSeconds_t interval_ms);

void
leds_stop_blink(void);

void
leds_init(void);

#ifdef __cplusplus
}
#endif

#endif // RUUVI_LEDS_H
