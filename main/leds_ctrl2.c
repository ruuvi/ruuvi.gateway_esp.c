/**
 * @file leds_ctrl2.c
 * @author TheSomeMan
 * @date 2022-10-30
 * @copyright Ruuvi Innovations Ltd, license BSD-3-Clause.
 */

#include "leds_ctrl2.h"
#define LOG_LOCAL_LEVEL LOG_LEVEL_INFO
#include "log.h"

#if LOG_LOCAL_LEVEL >= LOG_LEVEL_DEBUG
static const char* TAG = "leds_ctrl2";
#endif

typedef struct leds_ctrl2_state_t
{
    leds_ctrl_params_t params;
    bool               flag_wifi_ap_active;
    bool               flag_network_connected;
    bool               flag_http_conn_status[LEDS_CTRL_MAX_NUM_HTTP_CONN];
    bool               flag_mqtt_conn_status[LEDS_CTRL_MAX_NUM_MQTT_CONN];
    bool               flag_http_poll_status;
    bool               flag_recv_adv_status;
} leds_ctrl2_state_t;

static leds_ctrl2_state_t g_leds_ctrl2;

void
leds_ctrl2_init(void)
{
    leds_ctrl2_state_t* const p_state = &g_leds_ctrl2;

    p_state->params = (leds_ctrl_params_t) {
        .flag_use_mqtt        = 0,
        .http_targets_bitmask = 0,
    };
    p_state->flag_wifi_ap_active    = false;
    p_state->flag_network_connected = true;
    p_state->flag_recv_adv_status   = true;
    for (int32_t i = 0; i < LEDS_CTRL_MAX_NUM_HTTP_CONN; ++i)
    {
        p_state->flag_http_conn_status[i] = true;
    }
    for (int32_t i = 0; i < LEDS_CTRL_MAX_NUM_MQTT_CONN; ++i)
    {
        p_state->flag_mqtt_conn_status[i] = true;
    }
    p_state->flag_http_poll_status = true;
}

void
leds_ctrl2_deinit(void)
{
    leds_ctrl2_state_t* const p_state = &g_leds_ctrl2;

    p_state->params = (leds_ctrl_params_t) {
        .flag_use_mqtt        = 0,
        .http_targets_bitmask = 0,
    };
    p_state->flag_wifi_ap_active    = false;
    p_state->flag_network_connected = false;
    p_state->flag_recv_adv_status   = false;
    for (int32_t i = 0; i < LEDS_CTRL_MAX_NUM_HTTP_CONN; ++i)
    {
        p_state->flag_http_conn_status[i] = false;
    }
    for (int32_t i = 0; i < LEDS_CTRL_MAX_NUM_MQTT_CONN; ++i)
    {
        p_state->flag_mqtt_conn_status[i] = false;
    }
    p_state->flag_http_poll_status = false;
}

void
leds_ctrl2_configure(const leds_ctrl_params_t params)
{
    leds_ctrl2_state_t* const p_state = &g_leds_ctrl2;

    p_state->params = params;
}

#if LOG_LOCAL_LEVEL >= LOG_LEVEL_DEBUG
static const char*
leds_ctrl2_event_to_str(const leds_ctrl2_event_e event)
{
    const char* p_desc = "Unknown";
    switch (event)
    {
        case LEDS_CTRL2_EVENT_WIFI_AP_STARTED:
            p_desc = "EVENT_WIFI_AP_STARTED";
            break;
        case LEDS_CTRL2_EVENT_WIFI_AP_STOPPED:
            p_desc = "EVENT_WIFI_AP_STOPPED";
            break;
        case LEDS_CTRL2_EVENT_NETWORK_CONNECTED:
            p_desc = "EVENT_NETWORK_CONNECTED";
            break;
        case LEDS_CTRL2_EVENT_NETWORK_DISCONNECTED:
            p_desc = "EVENT_NETWORK_DISCONNECTED";
            break;
        case LEDS_CTRL2_EVENT_MQTT1_CONNECTED:
            p_desc = "EVENT_MQTT1_CONNECTED";
            break;
        case LEDS_CTRL2_EVENT_MQTT1_DISCONNECTED:
            p_desc = "EVENT_MQTT1_DISCONNECTED";
            break;
        case LEDS_CTRL2_EVENT_HTTP1_DATA_SENT_SUCCESSFULLY:
            p_desc = "EVENT_HTTP1_DATA_SENT_SUCCESSFULLY";
            break;
        case LEDS_CTRL2_EVENT_HTTP1_DATA_SENT_FAIL:
            p_desc = "EVENT_HTTP1_DATA_SENT_FAIL";
            break;
        case LEDS_CTRL2_EVENT_HTTP2_DATA_SENT_SUCCESSFULLY:
            p_desc = "EVENT_HTTP2_DATA_SENT_SUCCESSFULLY";
            break;
        case LEDS_CTRL2_EVENT_HTTP2_DATA_SENT_FAIL:
            p_desc = "EVENT_HTTP2_DATA_SENT_FAIL";
            break;
        case LEDS_CTRL2_EVENT_HTTP_POLL_OK:
            p_desc = "EVENT_HTTP_POLL_OK";
            break;
        case LEDS_CTRL2_EVENT_HTTP_POLL_TIMEOUT:
            p_desc = "EVENT_HTTP_POLL_TIMEOUT";
            break;
        case LEDS_CTRL2_EVENT_RECV_ADV:
            p_desc = "EVENT_RECV_ADV";
            break;
        case LEDS_CTRL2_EVENT_RECV_ADV_TIMEOUT:
            p_desc = "EVENT_RECV_ADV_TIMEOUT";
            break;
    }
    return p_desc;
}
#endif

static void
leds_ctrl2_on_event(leds_ctrl2_state_t* const p_state, const leds_ctrl2_event_e event)
{
    LOG_DBG("Event: %s", leds_ctrl2_event_to_str(event));
    switch (event)
    {
        case LEDS_CTRL2_EVENT_WIFI_AP_STARTED:
            p_state->flag_wifi_ap_active = true;
            break;
        case LEDS_CTRL2_EVENT_WIFI_AP_STOPPED:
            p_state->flag_wifi_ap_active = false;
            break;
        case LEDS_CTRL2_EVENT_NETWORK_CONNECTED:
            p_state->flag_network_connected = true;
            break;
        case LEDS_CTRL2_EVENT_NETWORK_DISCONNECTED:
            p_state->flag_network_connected = false;
            break;
        case LEDS_CTRL2_EVENT_MQTT1_CONNECTED:
            p_state->flag_mqtt_conn_status[0] = true;
            break;
        case LEDS_CTRL2_EVENT_MQTT1_DISCONNECTED:
            p_state->flag_mqtt_conn_status[0] = false;
            break;
        case LEDS_CTRL2_EVENT_HTTP1_DATA_SENT_SUCCESSFULLY:
            p_state->flag_http_conn_status[0] = true;
            break;
        case LEDS_CTRL2_EVENT_HTTP1_DATA_SENT_FAIL:
            p_state->flag_http_conn_status[0] = false;
            break;
        case LEDS_CTRL2_EVENT_HTTP2_DATA_SENT_SUCCESSFULLY:
            p_state->flag_http_conn_status[1] = true;
            break;
        case LEDS_CTRL2_EVENT_HTTP2_DATA_SENT_FAIL:
            p_state->flag_http_conn_status[1] = false;
            break;
        case LEDS_CTRL2_EVENT_HTTP_POLL_OK:
            p_state->flag_http_poll_status = true;
            break;
        case LEDS_CTRL2_EVENT_HTTP_POLL_TIMEOUT:
            p_state->flag_http_poll_status = false;
            break;
        case LEDS_CTRL2_EVENT_RECV_ADV:
            p_state->flag_recv_adv_status = true;
            break;
        case LEDS_CTRL2_EVENT_RECV_ADV_TIMEOUT:
            p_state->flag_recv_adv_status = false;
            break;
    }
}

void
leds_ctrl2_handle_event(const leds_ctrl2_event_e event)
{
    leds_ctrl2_state_t* const p_state = &g_leds_ctrl2;
    leds_ctrl2_on_event(p_state, event);
}

static uint32_t
leds_ctrl2_get_num_configured_targets(const leds_ctrl2_state_t* const p_state)
{
    uint32_t cnt = 0;
    for (uint32_t i = 0; i < LEDS_CTRL_MAX_NUM_HTTP_CONN; ++i)
    {
        if (0 != (p_state->params.http_targets_bitmask & (1U << i)))
        {
            cnt += 1;
        }
    }
    if (p_state->params.flag_use_mqtt)
    {
        cnt += 1;
    }
    return cnt;
}

leds_blinking_mode_t
leds_ctrl2_get_new_blinking_sequence(void)
{
    const leds_ctrl2_state_t* const p_state = &g_leds_ctrl2;

    LOG_DBG(
        "Get new blinking sequence in state: ap_active=%d, network=%d, http1=%d, http2=%d, mqtt=%d, poll=%d",
        p_state->flag_wifi_ap_active,
        p_state->flag_network_connected,
        p_state->flag_http_conn_status[0],
        p_state->flag_http_conn_status[1],
        p_state->flag_mqtt_conn_status[0],
        p_state->flag_http_poll_status);

    if (p_state->flag_wifi_ap_active)
    {
        return (leds_blinking_mode_t) { .p_sequence = LEDS_BLINKING_MODE_WIFI_HOTSPOT_ACTIVE };
    }
    if (!p_state->flag_network_connected)
    {
        return (leds_blinking_mode_t) { .p_sequence = LEDS_BLINKING_MODE_NETWORK_PROBLEM };
    }
    if (!p_state->flag_recv_adv_status)
    {
        return (leds_blinking_mode_t) { .p_sequence = LEDS_BLINKING_MODE_NO_ADVS };
    }
    const uint32_t num_configured_targets = leds_ctrl2_get_num_configured_targets(p_state);
    if (0 == num_configured_targets)
    {
        if (!p_state->flag_http_poll_status)
        {
            return (leds_blinking_mode_t) { .p_sequence = LEDS_BLINKING_MODE_NOT_CONNECTED_TO_ANY_TARGET };
        }
        return (leds_blinking_mode_t) { .p_sequence = LEDS_BLINKING_MODE_CONNECTED_TO_ALL_TARGETS };
    }
    uint32_t num_active_targets = 0;

    for (int32_t i = 0; i < LEDS_CTRL_MAX_NUM_HTTP_CONN; ++i)
    {
        if (0 != (p_state->params.http_targets_bitmask & (1U << (uint32_t)i)))
        {
            if (p_state->flag_http_conn_status[i])
            {
                num_active_targets += 1;
            }
        }
    }
    if (p_state->params.flag_use_mqtt && p_state->flag_mqtt_conn_status[0])
    {
        num_active_targets += 1;
    }
    if (0 == num_active_targets)
    {
        return (leds_blinking_mode_t) { .p_sequence = LEDS_BLINKING_MODE_NOT_CONNECTED_TO_ANY_TARGET };
    }
    if (num_configured_targets != num_active_targets)
    {
        return (leds_blinking_mode_t) { .p_sequence = LEDS_BLINKING_MODE_CONNECTED_TO_SOME_TARGETS };
    }
    return (leds_blinking_mode_t) { .p_sequence = LEDS_BLINKING_MODE_CONNECTED_TO_ALL_TARGETS };
}
