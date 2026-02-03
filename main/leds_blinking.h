/**
 * @file leds_blinking.h
 * @author TheSomeMan
 * @date 2022-10-30
 * @copyright Ruuvi Innovations Ltd, license BSD-3-Clause.
 */

#ifndef RUUVI_GATEWAY_ESP_LEDS_BLINKING_H
#define RUUVI_GATEWAY_ESP_LEDS_BLINKING_H

#include <stdbool.h>
#include <esp_attr.h>
#include "leds_ctrl_defs.h"

#ifdef __cplusplus
extern "C" {
#endif

#if defined(RUUVI_TESTS) && RUUVI_TESTS
extern leds_blinking_mode_t g_leds_blinking_mode;
extern int32_t IRAM_ATTR    g_leds_blinking_sequence_idx;
#endif

void
leds_blinking_init(const leds_blinking_mode_t blinking_mode);

void
leds_blinking_deinit(void);

void
leds_blinking_set_new_sequence(const leds_blinking_mode_t blinking_mode);

leds_color_e
leds_blinking_get_next(void);

bool
leds_blinking_is_sequence_finished(void);

#ifdef __cplusplus
}
#endif

#endif // RUUVI_GATEWAY_ESP_LEDS_BLINKING_H
