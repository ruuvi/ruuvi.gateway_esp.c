/**
 * @file leds_ctrl2.h
 * @author TheSomeMan
 * @date 2022-10-30
 * @copyright Ruuvi Innovations Ltd, license BSD-3-Clause.
 */

#ifndef RUUVI_GATEWAY_LEDS_CTRL2_H
#define RUUVI_GATEWAY_LEDS_CTRL2_H

#include <stdint.h>
#include <stdbool.h>
#include "leds_ctrl_defs.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum leds_ctrl2_event_e
{
    LEDS_CTRL2_EVENT_WIFI_AP_STARTED,
    LEDS_CTRL2_EVENT_WIFI_AP_STOPPED,
    LEDS_CTRL2_EVENT_NETWORK_CONNECTED,
    LEDS_CTRL2_EVENT_NETWORK_DISCONNECTED,

    LEDS_CTRL2_EVENT_MQTT1_CONNECTED,
    LEDS_CTRL2_EVENT_MQTT1_DISCONNECTED,
    LEDS_CTRL2_EVENT_HTTP1_DATA_SENT_SUCCESSFULLY,
    LEDS_CTRL2_EVENT_HTTP1_DATA_SENT_FAIL,
    LEDS_CTRL2_EVENT_HTTP2_DATA_SENT_SUCCESSFULLY,
    LEDS_CTRL2_EVENT_HTTP2_DATA_SENT_FAIL,
    LEDS_CTRL2_EVENT_HTTP_POLL_OK,
    LEDS_CTRL2_EVENT_HTTP_POLL_TIMEOUT,
    LEDS_CTRL2_EVENT_RECV_ADV,
    LEDS_CTRL2_EVENT_RECV_ADV_TIMEOUT,
} leds_ctrl2_event_e;

void
leds_ctrl2_init(void);

void
leds_ctrl2_deinit(void);

void
leds_ctrl2_configure(const leds_ctrl_params_t params);

void
leds_ctrl2_handle_event(const leds_ctrl2_event_e event);

leds_blinking_mode_t
leds_ctrl2_get_new_blinking_sequence(void);

#ifdef __cplusplus
}
#endif

#endif // RUUVI_GATEWAY_LEDS_CTRL2_H
