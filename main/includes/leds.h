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

#ifdef __cplusplus
extern "C" {
#endif

#define LEDS_ON_BIT    (1 << 0)
#define LEDS_OFF_BIT   (1 << 1)
#define LEDS_BLINK_BIT (1 << 2)

#define LEDS_SLOW_BLINK   1000
#define LEDS_MEDIUM_BLINK 500
#define LEDS_FAST_BLINK   200

extern EventGroupHandle_t led_bits;

void
leds_on();

void
leds_off();

void
leds_start_blink(uint32_t interval);

void
leds_stop_blink();

void
leds_init();

#ifdef __cplusplus
}
#endif

#endif // RUUVI_LEDS_H
