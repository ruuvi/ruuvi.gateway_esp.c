/**
 * @file leds_ctrl.c
 * @author TheSomeMan
 * @date 2022-10-30
 * @copyright Ruuvi Innovations Ltd, license BSD-3-Clause.
 */

#include "leds_ctrl.h"
#include <stdbool.h>
#include <assert.h>
#include "os_task.h"
#include "leds_ctrl2.h"
#define LOG_LOCAL_LEVEL LOG_LEVEL_INFO
#include "log.h"

#if LOG_LOCAL_LEVEL >= LOG_LEVEL_DEBUG
static const char* TAG = "leds_ctrl";
#endif

typedef enum leds_ctrl_state_e
{
    LEDS_CTRL_STATE_INITIAL,
    LEDS_CTRL_STATE_AFTER_REBOOT,      // Blinking RED "R-" with period 50ms
    LEDS_CTRL_STATE_CHECKING_NRF52_FW, // Turn off RED
    LEDS_CTRL_STATE_FLASHING_NRF52_FW, // Blinking RED "R---------"
    LEDS_CTRL_STATE_NRF52_FAILURE,     // Solid RED
    LEDS_CTRL_STATE_CFG_ERASING,       // Turn off RED
    LEDS_CTRL_STATE_CFG_ERASED,        // Blinking RED "RR--"
    LEDS_CTRL_STATE_WAITING_CFG_READY, // GREEN LED is ON and it is controlled by nRF52
    LEDS_CTRL_STATE_SUBSTATE,          // Depends on sub-state
    LEDS_CTRL_STATE_BEFORE_REBOOT,     // Turn off RED and GREEN
} leds_ctrl_state_e;

typedef struct leds_ctrl_state_t
{
    os_task_handle_t      task_handle;
    leds_ctrl_state_e     leds_sm_state;
    bool                  flag_configure_button_pressed;
    bool                  flag_submachine_configured;
    leds_ctrl_callbacks_t callbacks;
} leds_ctrl_state_t;

static leds_ctrl_state_t g_leds_ctrl_state;

static void
leds_ctrl_do_actions(void);

void
leds_ctrl_init(const bool flag_configure_button_pressed, const leds_ctrl_callbacks_t callbacks)
{
    leds_ctrl_state_t* const p_leds_ctrl_state       = &g_leds_ctrl_state;
    p_leds_ctrl_state->leds_sm_state                 = LEDS_CTRL_STATE_INITIAL;
    p_leds_ctrl_state->task_handle                   = os_task_get_cur_task_handle();
    p_leds_ctrl_state->flag_configure_button_pressed = flag_configure_button_pressed;
    p_leds_ctrl_state->flag_submachine_configured    = false;
    p_leds_ctrl_state->callbacks                     = callbacks;

    leds_ctrl2_init();

    leds_ctrl_do_actions();
}

void
leds_ctrl_deinit(void)
{
    leds_ctrl_state_t* const p_leds_ctrl_state    = &g_leds_ctrl_state;
    p_leds_ctrl_state->leds_sm_state              = LEDS_CTRL_STATE_INITIAL;
    p_leds_ctrl_state->task_handle                = NULL;
    p_leds_ctrl_state->flag_submachine_configured = false;
    p_leds_ctrl_state->callbacks                  = (leds_ctrl_callbacks_t) {
                         .cb_on_enter_state_after_reboot  = NULL,
                         .cb_on_exit_state_after_reboot   = NULL,
                         .cb_on_enter_state_before_reboot = NULL,
    };

    leds_ctrl2_deinit();
}

void
leds_ctrl_configure_sub_machine(const leds_ctrl_params_t params)
{
    leds_ctrl_state_t* const p_leds_ctrl_state = &g_leds_ctrl_state;
    leds_ctrl2_configure(params);
    p_leds_ctrl_state->flag_submachine_configured = true;
}

static bool
leds_ctrl_check_cur_thread(void)
{
    const leds_ctrl_state_t* const p_leds_ctrl_state = &g_leds_ctrl_state;
    return os_task_get_cur_task_handle() == p_leds_ctrl_state->task_handle;
}

#if LOG_LOCAL_LEVEL >= LOG_LEVEL_DEBUG
static const char*
leds_ctrl_state_to_str(leds_ctrl_state_e leds_ctrl_sm_state)
{
    const char* p_desc = "Unknown";
    switch (leds_ctrl_sm_state)
    {
        case LEDS_CTRL_STATE_INITIAL:
            p_desc = "INITIAL";
            break;
        case LEDS_CTRL_STATE_AFTER_REBOOT:
            p_desc = "AFTER_REBOOT";
            break;
        case LEDS_CTRL_STATE_CHECKING_NRF52_FW:
            p_desc = "CHECKING_NRF52_FW";
            break;
        case LEDS_CTRL_STATE_FLASHING_NRF52_FW:
            p_desc = "FLASHING_NRF52_FW";
            break;
        case LEDS_CTRL_STATE_CFG_ERASING:
            p_desc = "CFG_ERASING";
            break;
        case LEDS_CTRL_STATE_CFG_ERASED:
            p_desc = "CFG_ERASED";
            break;
        case LEDS_CTRL_STATE_WAITING_CFG_READY:
            p_desc = "WAITING_CFG_READY";
            break;
        case LEDS_CTRL_STATE_SUBSTATE:
            p_desc = "SUBSTATE";
            break;
        case LEDS_CTRL_STATE_BEFORE_REBOOT:
            p_desc = "BEFORE_REBOOT";
            break;
    }
    return p_desc;
}
#endif

const char*
leds_ctrl_event_to_str(const leds_ctrl_event_e leds_ctrl_event)
{
    const char* p_desc = "Event:Unknown";
    switch (leds_ctrl_event)
    {
        case LEDS_CTRL_EVENT_REBOOT:
            p_desc = "EVENT_REBOOT";
            break;
        case LEDS_CTRL_EVENT_CFG_ERASED:
            p_desc = "EVENT_CFG_ERASED";
            break;
        case LEDS_CTRL_EVENT_NRF52_FW_CHECK:
            p_desc = "NRF52_FW_CHECK";
            break;
        case LEDS_CTRL_EVENT_NRF52_FW_UPDATING:
            p_desc = "NRF52_FW_UPDATING";
            break;
        case LEDS_CTRL_EVENT_NRF52_READY:
            p_desc = "NRF52_FW_CHECK_COMPLETED";
            break;
        case LEDS_CTRL_EVENT_NRF52_FAILURE:
            p_desc = "NRF52_FAILURE";
            break;
        case LEDS_CTRL_EVENT_CFG_READY:
            p_desc = "EVENT_CFG_READY";
            break;
    }
    return p_desc;
}

static leds_ctrl_state_e
leds_ctrl_do_actions_step(const leds_ctrl_state_t* const p_leds_ctrl_state)
{
    LOG_DBG("Do actions in state: %s", leds_ctrl_state_to_str(p_leds_ctrl_state->leds_sm_state));
    leds_ctrl_state_e new_leds_ctrl_sm_state = p_leds_ctrl_state->leds_sm_state;
    switch (p_leds_ctrl_state->leds_sm_state)
    {
        case LEDS_CTRL_STATE_INITIAL:
            new_leds_ctrl_sm_state = p_leds_ctrl_state->flag_configure_button_pressed ? LEDS_CTRL_STATE_CFG_ERASING
                                                                                      : LEDS_CTRL_STATE_AFTER_REBOOT;
            break;
        case LEDS_CTRL_STATE_AFTER_REBOOT:
            break;
        case LEDS_CTRL_STATE_CHECKING_NRF52_FW:
            break;
        case LEDS_CTRL_STATE_FLASHING_NRF52_FW:
            break;
        case LEDS_CTRL_STATE_NRF52_FAILURE:
            break;
        case LEDS_CTRL_STATE_CFG_ERASING:
            break;
        case LEDS_CTRL_STATE_CFG_ERASED:
            break;
        case LEDS_CTRL_STATE_WAITING_CFG_READY:
            break;
        case LEDS_CTRL_STATE_SUBSTATE:
            break;
        case LEDS_CTRL_STATE_BEFORE_REBOOT:
            break;
    }
    return new_leds_ctrl_sm_state;
}

static void
leds_ctrl_transition(leds_ctrl_state_t* const p_leds_ctrl_state, const leds_ctrl_state_e new_led_ctrl_sm_state)
{
    LOG_DBG(
        "Transition: %s -> %s",
        leds_ctrl_state_to_str(p_leds_ctrl_state->leds_sm_state),
        leds_ctrl_state_to_str(new_led_ctrl_sm_state));
    if (LEDS_CTRL_STATE_AFTER_REBOOT == new_led_ctrl_sm_state)
    {
        if (NULL != p_leds_ctrl_state->callbacks.cb_on_enter_state_after_reboot)
        {
            p_leds_ctrl_state->callbacks.cb_on_enter_state_after_reboot();
        }
    }
    else if (LEDS_CTRL_STATE_BEFORE_REBOOT == new_led_ctrl_sm_state)
    {
        if (NULL != p_leds_ctrl_state->callbacks.cb_on_enter_state_before_reboot)
        {
            p_leds_ctrl_state->callbacks.cb_on_enter_state_before_reboot();
        }
    }
    else
    {
        // MISRA C:2012, 15.7 - All if...else if constructs shall be terminated with an else statement
    }
    if (LEDS_CTRL_STATE_AFTER_REBOOT == p_leds_ctrl_state->leds_sm_state)
    {
        if (NULL != p_leds_ctrl_state->callbacks.cb_on_exit_state_after_reboot)
        {
            p_leds_ctrl_state->callbacks.cb_on_exit_state_after_reboot();
        }
    }
    p_leds_ctrl_state->leds_sm_state = new_led_ctrl_sm_state;
}

static void
leds_ctrl_do_actions(void)
{
    leds_ctrl_state_t* const p_leds_ctrl_state = &g_leds_ctrl_state;
    while (1)
    {
        const leds_ctrl_state_e new_leds_ctrl_state = leds_ctrl_do_actions_step(p_leds_ctrl_state);
        if (new_leds_ctrl_state == p_leds_ctrl_state->leds_sm_state)
        {
            break;
        }
        leds_ctrl_transition(p_leds_ctrl_state, new_leds_ctrl_state);
    }
}

static leds_ctrl_state_e
leds_ctrl_handle_event_in_state_after_reboot(
    const leds_ctrl_state_t* const p_leds_ctrl_state,
    const leds_ctrl_event_e        leds_ctrl_event)
{
    switch (leds_ctrl_event)
    {
        case LEDS_CTRL_EVENT_REBOOT:
            return LEDS_CTRL_STATE_BEFORE_REBOOT;

        case LEDS_CTRL_EVENT_CFG_ERASED:
            assert(0);
            break;

        case LEDS_CTRL_EVENT_NRF52_FW_CHECK:
            return LEDS_CTRL_STATE_CHECKING_NRF52_FW;

        case LEDS_CTRL_EVENT_NRF52_FW_UPDATING:
            assert(0);
            break;
        case LEDS_CTRL_EVENT_NRF52_READY:
            assert(0);
            break;
        case LEDS_CTRL_EVENT_NRF52_FAILURE:
            assert(0);
            break;
        case LEDS_CTRL_EVENT_CFG_READY:
            assert(0);
            break;
    }
    return p_leds_ctrl_state->leds_sm_state;
}

static leds_ctrl_state_e
leds_ctrl_handle_event_in_state_cfg_erasing(
    const leds_ctrl_state_t* const p_leds_ctrl_state,
    const leds_ctrl_event_e        leds_ctrl_event)
{
    switch (leds_ctrl_event)
    {
        case LEDS_CTRL_EVENT_REBOOT:
            return LEDS_CTRL_STATE_BEFORE_REBOOT;

        case LEDS_CTRL_EVENT_CFG_ERASED:
            return LEDS_CTRL_STATE_CFG_ERASED;

        case LEDS_CTRL_EVENT_NRF52_FW_CHECK:
            assert(0);
            break;
        case LEDS_CTRL_EVENT_NRF52_FW_UPDATING:
            assert(0);
            break;
        case LEDS_CTRL_EVENT_NRF52_READY:
            assert(0);
            break;
        case LEDS_CTRL_EVENT_NRF52_FAILURE:
            assert(0);
            break;
        case LEDS_CTRL_EVENT_CFG_READY:
            assert(0);
            break;
    }
    return p_leds_ctrl_state->leds_sm_state;
}

static leds_ctrl_state_e
leds_ctrl_handle_event_in_state_checking_nrf52_fw(
    const leds_ctrl_state_t* const p_leds_ctrl_state,
    const leds_ctrl_event_e        leds_ctrl_event)
{
    switch (leds_ctrl_event)
    {
        case LEDS_CTRL_EVENT_REBOOT:
            return LEDS_CTRL_STATE_BEFORE_REBOOT;

        case LEDS_CTRL_EVENT_CFG_ERASED:
            assert(0);
            break;

        case LEDS_CTRL_EVENT_NRF52_FW_CHECK:
            assert(0);
            break;

        case LEDS_CTRL_EVENT_NRF52_FW_UPDATING:
            return LEDS_CTRL_STATE_FLASHING_NRF52_FW;

        case LEDS_CTRL_EVENT_NRF52_READY:
            return LEDS_CTRL_STATE_WAITING_CFG_READY;

        case LEDS_CTRL_EVENT_NRF52_FAILURE:
            return LEDS_CTRL_STATE_NRF52_FAILURE;

        case LEDS_CTRL_EVENT_CFG_READY:
            assert(0);
            break;
    }
    return p_leds_ctrl_state->leds_sm_state;
}

static leds_ctrl_state_e
leds_ctrl_handle_event_in_state_flashing_nrf52_fw(
    const leds_ctrl_state_t* const p_leds_ctrl_state,
    const leds_ctrl_event_e        leds_ctrl_event)
{
    switch (leds_ctrl_event)
    {
        case LEDS_CTRL_EVENT_REBOOT:
            return LEDS_CTRL_STATE_BEFORE_REBOOT;

        case LEDS_CTRL_EVENT_CFG_ERASED:
            assert(0);
            break;

        case LEDS_CTRL_EVENT_NRF52_FW_CHECK:
            assert(0);
            break;

        case LEDS_CTRL_EVENT_NRF52_FW_UPDATING:
            assert(0);
            break;

        case LEDS_CTRL_EVENT_NRF52_READY:
            assert(0);
            break;

        case LEDS_CTRL_EVENT_NRF52_FAILURE:
            return LEDS_CTRL_STATE_NRF52_FAILURE;

        case LEDS_CTRL_EVENT_CFG_READY:
            break;
    }
    return p_leds_ctrl_state->leds_sm_state;
}

static leds_ctrl_state_e
leds_ctrl_handle_event_in_state_waiting_cfg_ready(
    const leds_ctrl_state_t* const p_leds_ctrl_state,
    const leds_ctrl_event_e        leds_ctrl_event)
{
    switch (leds_ctrl_event)
    {
        case LEDS_CTRL_EVENT_REBOOT:
            return LEDS_CTRL_STATE_BEFORE_REBOOT;

        case LEDS_CTRL_EVENT_CFG_ERASED:
            assert(0);
            break;
        case LEDS_CTRL_EVENT_NRF52_FW_CHECK:
            assert(0);
            break;
        case LEDS_CTRL_EVENT_NRF52_FW_UPDATING:
            assert(0);
            break;
        case LEDS_CTRL_EVENT_NRF52_READY:
            assert(0);
            break;
        case LEDS_CTRL_EVENT_NRF52_FAILURE:
            assert(0);
            break;

        case LEDS_CTRL_EVENT_CFG_READY:
            return LEDS_CTRL_STATE_SUBSTATE;
    }
    return p_leds_ctrl_state->leds_sm_state;
}

static leds_ctrl_state_e
leds_ctrl_handle_event_in_state_substate(
    const leds_ctrl_state_t* const p_leds_ctrl_state,
    const leds_ctrl_event_e        leds_ctrl_event)
{
    switch (leds_ctrl_event)
    {
        case LEDS_CTRL_EVENT_REBOOT:
            return LEDS_CTRL_STATE_BEFORE_REBOOT;
        case LEDS_CTRL_EVENT_CFG_ERASED:
            assert(0);
            break;
        case LEDS_CTRL_EVENT_NRF52_FW_CHECK:
            assert(0);
            break;
        case LEDS_CTRL_EVENT_NRF52_FW_UPDATING:
            return LEDS_CTRL_STATE_FLASHING_NRF52_FW;
        case LEDS_CTRL_EVENT_NRF52_READY:
            break;
        case LEDS_CTRL_EVENT_NRF52_FAILURE:
            break;
        case LEDS_CTRL_EVENT_CFG_READY:
            assert(0);
    }
    return p_leds_ctrl_state->leds_sm_state;
}

void
leds_ctrl_handle_event(const leds_ctrl_event_e leds_ctrl_event)
{
    assert(leds_ctrl_check_cur_thread());
    leds_ctrl_state_t* const p_leds_ctrl_state = &g_leds_ctrl_state;
    LOG_DBG(
        "Event: %s, in state %s",
        leds_ctrl_event_to_str(leds_ctrl_event),
        leds_ctrl_state_to_str(p_leds_ctrl_state->leds_sm_state));

    leds_ctrl_state_e new_sm_state = p_leds_ctrl_state->leds_sm_state;
    if (LEDS_CTRL_EVENT_REBOOT == leds_ctrl_event)
    {
        new_sm_state = LEDS_CTRL_STATE_BEFORE_REBOOT;
    }
    else
    {
        switch (p_leds_ctrl_state->leds_sm_state)
        {
            case LEDS_CTRL_STATE_INITIAL:
                break;
            case LEDS_CTRL_STATE_AFTER_REBOOT:
                new_sm_state = leds_ctrl_handle_event_in_state_after_reboot(p_leds_ctrl_state, leds_ctrl_event);
                break;
            case LEDS_CTRL_STATE_CHECKING_NRF52_FW:
                new_sm_state = leds_ctrl_handle_event_in_state_checking_nrf52_fw(p_leds_ctrl_state, leds_ctrl_event);
                break;
            case LEDS_CTRL_STATE_FLASHING_NRF52_FW:
                new_sm_state = leds_ctrl_handle_event_in_state_flashing_nrf52_fw(p_leds_ctrl_state, leds_ctrl_event);
                break;
            case LEDS_CTRL_STATE_NRF52_FAILURE:
                break;
            case LEDS_CTRL_STATE_CFG_ERASING:
                new_sm_state = leds_ctrl_handle_event_in_state_cfg_erasing(p_leds_ctrl_state, leds_ctrl_event);
                break;
            case LEDS_CTRL_STATE_CFG_ERASED:
                break;
            case LEDS_CTRL_STATE_WAITING_CFG_READY:
                new_sm_state = leds_ctrl_handle_event_in_state_waiting_cfg_ready(p_leds_ctrl_state, leds_ctrl_event);
                break;
            case LEDS_CTRL_STATE_SUBSTATE:
                assert(p_leds_ctrl_state->flag_submachine_configured);
                new_sm_state = leds_ctrl_handle_event_in_state_substate(p_leds_ctrl_state, leds_ctrl_event);
                break;
            case LEDS_CTRL_STATE_BEFORE_REBOOT:
                break;
        }
    }
    if (new_sm_state != p_leds_ctrl_state->leds_sm_state)
    {
        leds_ctrl_transition(p_leds_ctrl_state, new_sm_state);
        leds_ctrl_do_actions();
    }
}

leds_blinking_mode_t
leds_ctrl_get_new_blinking_sequence(void)
{
    assert(leds_ctrl_check_cur_thread());
    LOG_DBG("Get new blinking sequence in state: %s", leds_ctrl_state_to_str(g_leds_ctrl_state.leds_sm_state));
    switch (g_leds_ctrl_state.leds_sm_state)
    {
        case LEDS_CTRL_STATE_INITIAL:
            assert(0);
            break;
        case LEDS_CTRL_STATE_AFTER_REBOOT:
            return (leds_blinking_mode_t) { .p_sequence = LEDS_BLINKING_AFTER_REBOOT };
        case LEDS_CTRL_STATE_CHECKING_NRF52_FW:
            return (leds_blinking_mode_t) { .p_sequence = LEDS_BLINKING_WHILE_CHECKING_NRF52_FW };
        case LEDS_CTRL_STATE_FLASHING_NRF52_FW:
            return (leds_blinking_mode_t) { .p_sequence = LEDS_BLINKING_WHILE_FLASHING_NRF52_FW };
        case LEDS_CTRL_STATE_NRF52_FAILURE:
            return (leds_blinking_mode_t) { .p_sequence = LEDS_BLINKING_NRF52_FAILURE };
        case LEDS_CTRL_STATE_CFG_ERASING:
            return (leds_blinking_mode_t) { .p_sequence = LEDS_BLINKING_ON_CFG_ERASING };
        case LEDS_CTRL_STATE_CFG_ERASED:
            return (leds_blinking_mode_t) { .p_sequence = LEDS_BLINKING_ON_CFG_ERASED };
        case LEDS_CTRL_STATE_WAITING_CFG_READY:
            return (leds_blinking_mode_t) { .p_sequence = LEDS_BLINKING_CONTROLLED_BY_NRF52 };
        case LEDS_CTRL_STATE_SUBSTATE:
            return leds_ctrl2_get_new_blinking_sequence();
        case LEDS_CTRL_STATE_BEFORE_REBOOT:
            return (leds_blinking_mode_t) { .p_sequence = LEDS_BLINKING_MODE_BEFORE_REBOOT };
    }
    assert(0);
    return (leds_blinking_mode_t) { .p_sequence = NULL };
}

bool
leds_ctrl_is_in_substate(void)
{
    assert(leds_ctrl_check_cur_thread());
    const leds_ctrl_state_t* const p_leds_ctrl_state = &g_leds_ctrl_state;
    if (LEDS_CTRL_STATE_SUBSTATE == p_leds_ctrl_state->leds_sm_state)
    {
        return true;
    }
    return false;
}
