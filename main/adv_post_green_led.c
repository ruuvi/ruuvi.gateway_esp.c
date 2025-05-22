/**
 * @file adv_post_green_led.c
 * @author TheSomeMan
 * @date 2023-09-04
 * @copyright Ruuvi Innovations Ltd, license BSD-3-Clause.
 */

#include "adv_post_green_led.h"
#include "os_timer_sig.h"
#include "os_mutex.h"
#include "os_task.h"
#include "adv_post_internal.h"
#include "adv_post_nrf52.h"
#if defined(RUUVI_TESTS) && RUUVI_TESTS
#define LOG_LOCAL_DISABLED 1
#define LOG_LOCAL_LEVEL    LOG_LEVEL_NONE
#else
#define LOG_LOCAL_LEVEL LOG_LEVEL_INFO
#endif
#include "log.h"
#include "adv_post_timers.h"
static const char* TAG = "ADV_POST_GREEN_LED";

static bool             g_adv_post_green_led_state;
static os_task_handle_t g_adv_post_task_handle;

static bool              g_adv_post_green_led_disabled;
static os_mutex_static_t g_adv_post_green_led_disabled_mutex_mem;
static os_mutex_t        g_adv_post_green_led_disabled_mutex;

void
adv_post_green_led_init(void)
{
    g_adv_post_green_led_disabled_mutex = os_mutex_create_static(&g_adv_post_green_led_disabled_mutex_mem);
    g_adv_post_task_handle              = os_task_get_cur_task_handle();
    g_adv_post_green_led_state          = false;
    g_adv_post_green_led_disabled       = false;
}

void
adv_post_green_led_async_disable(void)
{
    os_mutex_lock(g_adv_post_green_led_disabled_mutex);
    g_adv_post_green_led_disabled = true;
    LOG_WARN("GREEN LED: Disable Green LED updating");
    os_mutex_unlock(g_adv_post_green_led_disabled_mutex);
}

void
adv_post_green_led_enable(void)
{
    assert(g_adv_post_task_handle == os_task_get_cur_task_handle());
    os_mutex_lock(g_adv_post_green_led_disabled_mutex);
    LOG_WARN("GREEN LED: Enable Green LED updating");
    g_adv_post_green_led_disabled = false;
    os_mutex_unlock(g_adv_post_green_led_disabled_mutex);
    os_timer_sig_periodic_t* const p_timer_sig = adv_post_timers_get_timer_sig_green_led_update();
    if (!os_timer_sig_periodic_is_active(p_timer_sig))
    {
        LOG_INFO("GREEN LED: Activate periodic updating");
        os_timer_sig_periodic_start(p_timer_sig);
    }
}

static bool
adv_post_green_led_is_disabled(void)
{
    os_mutex_lock(g_adv_post_green_led_disabled_mutex);
    const bool flag_green_led_disabled = g_adv_post_green_led_disabled;
    os_mutex_unlock(g_adv_post_green_led_disabled_mutex);
    return flag_green_led_disabled;
}

void
adv_post_on_green_led_update(const adv_post_green_led_cmd_e cmd)
{
    assert(g_adv_post_task_handle == os_task_get_cur_task_handle());
    switch (cmd)
    {
        case ADV_POST_GREEN_LED_CMD_UPDATE:
            LOG_DBG("%s: ADV_POST_GREEN_LED_CMD_UPDATE: state=%d", __func__, g_adv_post_green_led_state);
            break;
        case ADV_POST_GREEN_LED_CMD_OFF:
            LOG_DBG("%s: ADV_POST_GREEN_LED_CMD_OFF", __func__);
            g_adv_post_green_led_state = false;
            break;
        case ADV_POST_GREEN_LED_CMD_ON:
            LOG_DBG("%s: ADV_POST_GREEN_LED_CMD_ON", __func__);
            g_adv_post_green_led_state = true;
            break;
    }
    os_timer_sig_periodic_t* const p_timer_sig = adv_post_timers_get_timer_sig_green_led_update();

    if (adv_post_green_led_is_disabled())
    {
        LOG_WARN("%s: GREEN LED: Updating is disabled", __func__);
        if (os_timer_sig_periodic_is_active(p_timer_sig))
        {
            LOG_INFO("GREEN LED: Stop periodic updating");
            os_timer_sig_periodic_stop(p_timer_sig);
        }
    }
    else
    {
        if (!os_timer_sig_periodic_is_active(p_timer_sig))
        {
            LOG_INFO("GREEN LED: Activate periodic updating");
            os_timer_sig_periodic_start(p_timer_sig);
        }
        else
        {
            if (ADV_POST_GREEN_LED_CMD_UPDATE != cmd)
            {
                LOG_DBG("GREEN LED: Restart periodic updating timer");
                os_timer_sig_periodic_relaunch(p_timer_sig, true);
            }
        }
        adv_post_nrf52_send_led_ctrl(g_adv_post_green_led_state ? ADV_POST_GREEN_LED_ON_INTERVAL_MS : 0);
    }
}
