#ifndef _LEDS_H_DEF_
#define _LEDS_H_DEF_

#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"

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

#endif
