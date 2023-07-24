/**
 * @file leds_blinking.c
 * @author TheSomeMan
 * @date 2022-10-30
 * @copyright Ruuvi Innovations Ltd, license BSD-3-Clause.
 */

#include "leds_blinking.h"
#include <stdint.h>
#include <assert.h>
#include <string.h>
#include "os_task.h"
#if defined(RUUVI_TESTS_LEDS_BLINKING) && RUUVI_TESTS_LEDS_BLINKING
#define LOG_LOCAL_DISABLED
#define LOG_LOCAL_LEVEL LOG_LEVEL_NONE
#else
#define LOG_LOCAL_LEVEL LOG_LEVEL_INFO
#endif
#include "log.h"

#if LOG_LOCAL_LEVEL >= LOG_LEVEL_DEBUG
static const char* TAG = "leds_blinking";
#endif

static leds_blinking_mode_t g_leds_blinking_mode;
static int32_t              g_leds_blinking_sequence_idx;
static os_task_handle_t     g_leds_blinking_task_handle;

static const char TAG[] = "LEDS";

void
leds_blinking_init(const leds_blinking_mode_t blinking_mode)
{
    assert(NULL == g_leds_blinking_task_handle);
    g_leds_blinking_task_handle  = os_task_get_cur_task_handle();
    g_leds_blinking_mode         = blinking_mode;
    g_leds_blinking_sequence_idx = 0;
}

void
leds_blinking_deinit(void)
{
    assert(g_leds_blinking_task_handle == os_task_get_cur_task_handle());
    g_leds_blinking_task_handle     = NULL;
    g_leds_blinking_mode.p_sequence = NULL;
    g_leds_blinking_sequence_idx    = 0;
}

void
leds_blinking_set_new_sequence(const leds_blinking_mode_t blinking_mode)
{
    assert(g_leds_blinking_task_handle == os_task_get_cur_task_handle());
    assert(NULL != blinking_mode.p_sequence);
    LOG_DBG("Start new blinking sequence: %s", blinking_mode.p_sequence);
    if (0 != g_leds_blinking_sequence_idx)
    {
        // Blinking sequence can be changed only after completing of the previous one
        assert(0 == g_leds_blinking_sequence_idx);
    }
    if ((NULL == g_leds_blinking_mode.p_sequence)
        || (0 != strcmp(g_leds_blinking_mode.p_sequence, blinking_mode.p_sequence)))
    {
        LOG_INFO("LEDS: New blinking mode: %s", blinking_mode.p_sequence);
    }
    g_leds_blinking_mode = blinking_mode;
}

static leds_color_e
leds_blinking_get_next_internal(
    const leds_blinking_mode_t* const p_blinking_mode,
    int32_t* const                    p_leds_blinking_sequence_idx)
{
    int32_t blinking_sequence_idx = *p_leds_blinking_sequence_idx;
    assert(NULL != p_blinking_mode->p_sequence);
    const char cmd = p_blinking_mode->p_sequence[blinking_sequence_idx];
    blinking_sequence_idx += 1;
    if ('\0' == p_blinking_mode->p_sequence[blinking_sequence_idx])
    {
        blinking_sequence_idx = 0;
    }
    *p_leds_blinking_sequence_idx = blinking_sequence_idx;
    switch (cmd)
    {
        case 'R':
            return LEDS_COLOR_RED;
        case 'G':
            return LEDS_COLOR_GREEN;
        case '-':
            return LEDS_COLOR_OFF;
        default:
            break;
    }
    LOG_DBG("Unknown cmd='%c' (only 'R', 'G' or '-' are allowed)", cmd);
    assert(0);
    *p_leds_blinking_sequence_idx = 0;
    return LEDS_COLOR_OFF;
}

leds_color_e
leds_blinking_get_next(void)
{
    assert(g_leds_blinking_task_handle == os_task_get_cur_task_handle());
    return leds_blinking_get_next_internal(&g_leds_blinking_mode, &g_leds_blinking_sequence_idx);
}

bool
leds_blinking_is_sequence_finished(void)
{
    assert(g_leds_blinking_task_handle == os_task_get_cur_task_handle());
    if (0 == g_leds_blinking_sequence_idx)
    {
        return true;
    }
    return false;
}
