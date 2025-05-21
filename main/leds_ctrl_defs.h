/**
 * @file leds_ctrl_defs.h
 * @author TheSomeMan
 * @date 2022-10-30
 * @copyright Ruuvi Innovations Ltd, license BSD-3-Clause.
 */

#ifndef RUUVI_GATEWAY_LEDS_CTRL_DEFS_H
#define RUUVI_GATEWAY_LEDS_CTRL_DEFS_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define LEDS_CTRL_MAX_NUM_HTTP_CONN (2)
#define LEDS_CTRL_MAX_NUM_MQTT_CONN (1)

#define LEDS_BLINKING_AFTER_REBOOT "R-R-R-R-" /* LED blinking, the step is 25ms */

/* LED blinking, the step is 100ms: */
#define LEDS_BLINKING_WHILE_CHECKING_NRF52_FW "-"
#define LEDS_BLINKING_CONTROLLED_BY_NRF52     "-"
#define LEDS_BLINKING_WHILE_FLASHING_NRF52_FW "R---------"
#define LEDS_BLINKING_NRF52_FAILURE           "R"
#define LEDS_BLINKING_NRF52_REBOOTED          "-------R-R"
#define LEDS_BLINKING_ON_CFG_ERASING          "-"
#define LEDS_BLINKING_ON_CFG_ERASED           "RR--RR--"
#define LEDS_BLINKING_MODE_BEFORE_REBOOT      "-"

/* LED blinking, the step is 100ms: */
#define LEDS_BLINKING_MODE_WIFI_HOTSPOT_ACTIVE         "RRRRRRRRRRGGGGGGGGGG"
#define LEDS_BLINKING_MODE_WIFI_HOTSPOT_ACTIVE_AND_WPS "RRRRRRRRGRGGGGGGGGGG"
#define LEDS_BLINKING_MODE_NETWORK_PROBLEM             "R-R-R-R-R-"
#define LEDS_BLINKING_MODE_NOT_CONNECTED_TO_ANY_TARGET "RRRRR-----"
#define LEDS_BLINKING_MODE_NO_ADVS                     "G-G-G-G-G-"
#define LEDS_BLINKING_MODE_CONNECTED_TO_ALL_TARGETS    "GGGGGGGGGG"
#define LEDS_BLINKING_MODE_CONNECTED_TO_SOME_TARGETS   "GGGGGGGGG-"

typedef enum leds_color_e
{
    LEDS_COLOR_OFF,
    LEDS_COLOR_RED,
    LEDS_COLOR_GREEN,
} leds_color_e;

typedef struct leds_ctrl_params_t
{
    bool     flag_use_mqtt;
    uint32_t http_targets_bitmask;
} leds_ctrl_params_t;

typedef void (*leds_ctrl_callback_on_enter_state_after_reboot_t)(void);
typedef void (*leds_ctrl_callback_on_exit_state_after_reboot_t)(void);
typedef void (*leds_ctrl_callback_on_enter_state_before_reboot_t)(void);

typedef struct leds_ctrl_callbacks_t
{
    leds_ctrl_callback_on_enter_state_after_reboot_t  cb_on_enter_state_after_reboot;
    leds_ctrl_callback_on_exit_state_after_reboot_t   cb_on_exit_state_after_reboot;
    leds_ctrl_callback_on_enter_state_before_reboot_t cb_on_enter_state_before_reboot;
} leds_ctrl_callbacks_t;

typedef struct leds_blinking_mode_t
{
    const char* p_sequence;
} leds_blinking_mode_t;

#ifdef __cplusplus
}
#endif

#endif // RUUVI_GATEWAY_LEDS_CTRL_DEFS_H
