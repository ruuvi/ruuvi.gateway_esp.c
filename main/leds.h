/**
 * @file leds.h
 * @author Jukka Saari
 * @date 2019-11-27
 * @copyright Ruuvi Innovations Ltd, license BSD-3-Clause.
 */

#ifndef RUUVI_LEDS_H
#define RUUVI_LEDS_H

#include "time_units.h"
#include <stdbool.h>

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

bool
leds_init(const bool flag_configure_button_pressed);

void
leds_notify_nrf52_ready(void);

void
leds_notify_nrf52_failure(void);

void
leds_notify_nrf52_fw_check(void);

void
leds_notify_nrf52_fw_updating(void);

void
leds_notify_cfg_erased(void);

void
leds_notify_mqtt1_connected(void);

void
leds_notify_mqtt1_disconnected(void);

void
leds_notify_http1_data_sent_successfully(void);

void
leds_notify_http1_data_sent_fail(void);

void
leds_notify_http2_data_sent_successfully(void);

void
leds_notify_http2_data_sent_fail(void);

void
leds_notify_http_poll_ok(void);

void
leds_notify_http_poll_timeout(void);

bool
leds_get_green_led_state(void);

#if RUUVI_TESTS_LEDS

LEDS_STATIC

#endif // RUUVI_TESTS_LEDS

#ifdef __cplusplus
}
#endif

#endif // RUUVI_LEDS_H
