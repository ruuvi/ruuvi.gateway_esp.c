/**
 * @file adv_post_green_led.c
 * @author TheSomeMan
 * @date 2023-09-04
 * @copyright Ruuvi Innovations Ltd, license BSD-3-Clause.
 */

#include "adv_post_green_led.h"
#include "os_timer_sig.h"
#include "adv_post_internal.h"
#include "api.h"
#if defined(RUUVI_TESTS) && RUUVI_TESTS
#define LOG_LOCAL_DISABLED 1
#define LOG_LOCAL_LEVEL    LOG_LEVEL_NONE
#else
#define LOG_LOCAL_LEVEL LOG_LEVEL_INFO
#endif
#include "log.h"
#include "adv_post_timers.h"
static const char* TAG = "ADV_POST_TASK";

static bool g_adv_post_green_led_state = false;

void
adv_post_green_led_init(void)
{
    g_adv_post_green_led_state = false;
}

void
adv_post_on_green_led_update(const adv_post_green_len_cmd_e cmd)
{
    switch (cmd)
    {
        case ADV_POST_GREEN_LED_CMD_UPDATE:
            break;
        case ADV_POST_GREEN_LED_CMD_OFF:
            g_adv_post_green_led_state = false;
            break;
        case ADV_POST_GREEN_LED_CMD_ON:
            g_adv_post_green_led_state = true;
            break;
    }
    os_timer_sig_periodic_t* const p_timer_sig = adv_post_timers_get_timer_sig_green_led_update();

    if (!os_timer_sig_periodic_is_active(p_timer_sig))
    {
        LOG_INFO("GREEN LED: Activate periodic updating");
        os_timer_sig_periodic_start(p_timer_sig);
    }

    if (api_send_led_ctrl(g_adv_post_green_led_state ? ADV_POST_GREEN_LED_ON_INTERVAL_MS : 0) < 0)
    {
        LOG_ERR("%s failed", "api_send_led_ctrl");
    }
}
