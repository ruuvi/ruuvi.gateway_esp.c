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
#include "time_units.h"

#if !defined(RUUVI_TESTS_LEDS)
#define RUUVI_TESTS_LEDS (0)
#endif

#if RUUVI_TESTS_LEDS
#define LEDS_STATIC
#else
#define LEDS_STATIC static
#endif

#ifdef __cplusplus
extern "C" {
#endif

void
leds_init(void);

void
leds_off(void);

void
leds_indication_on_configure_button_press(void);

void
leds_indication_on_hotspot_activation(void);

void
leds_indication_network_no_connection(void);

void
leds_indication_on_network_ok(void);

void
leds_indication_on_nrf52_fw_updating(void);

#if RUUVI_TESTS_LEDS

LEDS_STATIC
void
leds_on(void);

LEDS_STATIC
void
leds_start_blink(const TimeUnitsMilliSeconds_t interval_ms, const uint32_t duty_cycle_percent);

#endif // RUUVI_TESTS_LEDS

#ifdef __cplusplus
}
#endif

#endif // RUUVI_LEDS_H
