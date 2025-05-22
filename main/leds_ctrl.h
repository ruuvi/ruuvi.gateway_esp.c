/**
 * @file leds_ctrl.h
 * @author TheSomeMan
 * @date 2022-10-30
 * @copyright Ruuvi Innovations Ltd, license BSD-3-Clause.
 */

#ifndef RUUVI_GATEWAY_LEDS_CTRL_H
#define RUUVI_GATEWAY_LEDS_CTRL_H

#include "leds_blinking.h"
#include "leds_ctrl_defs.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum leds_ctrl_event_e
{
    LEDS_CTRL_EVENT_REBOOT,
    LEDS_CTRL_EVENT_CFG_ERASED,
    LEDS_CTRL_EVENT_NRF52_FW_CHECK,
    LEDS_CTRL_EVENT_NRF52_FW_UPDATING,
    LEDS_CTRL_EVENT_NRF52_READY,
    LEDS_CTRL_EVENT_NRF52_FAILURE,
    LEDS_CTRL_EVENT_NRF52_REBOOTED,
    LEDS_CTRL_EVENT_NRF52_CONFIGURED,
    LEDS_CTRL_EVENT_CFG_READY,
} leds_ctrl_event_e;

typedef enum leds_ctrl_state_e
{
    LEDS_CTRL_STATE_INITIAL,
    LEDS_CTRL_STATE_AFTER_REBOOT,      // Blinking RED "R-" with period 50ms
    LEDS_CTRL_STATE_CHECKING_NRF52_FW, // Turn off RED
    LEDS_CTRL_STATE_FLASHING_NRF52_FW, // Blinking RED "R---------"
    LEDS_CTRL_STATE_NRF52_FAILURE,     // Solid RED
    LEDS_CTRL_STATE_NRF52_REBOOTED,    // Blinking RED "-------R-R"
    LEDS_CTRL_STATE_CFG_ERASING,       // Turn off RED
    LEDS_CTRL_STATE_CFG_ERASED,        // Blinking RED "RR--"
    LEDS_CTRL_STATE_WAITING_CFG_READY, // GREEN LED is ON and it is controlled by nRF52
    LEDS_CTRL_STATE_SUBSTATE,          // Depends on sub-state
    LEDS_CTRL_STATE_BEFORE_REBOOT,     // Turn off RED and GREEN
} leds_ctrl_state_e;

#if defined(RUUVI_TESTS_LEDS) && RUUVI_TESTS_LEDS
leds_ctrl_state_e
leds_ctrl_get_state(void);
#endif

void
leds_ctrl_init(const bool flag_configure_button_pressed, const leds_ctrl_callbacks_t callbacks);

void
leds_ctrl_deinit(void);

void
leds_ctrl_configure_sub_machine(const leds_ctrl_params_t params);

void
leds_ctrl_handle_event(const leds_ctrl_event_e leds_ctrl_event);

leds_blinking_mode_t
leds_ctrl_get_new_blinking_sequence(void);

bool
leds_ctrl_is_in_substate(void);

#ifdef __cplusplus
}
#endif

#endif // RUUVI_GATEWAY_LEDS_CTRL_H
