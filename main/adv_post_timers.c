/**
 * @file adv_post_timers.c
 * @author TheSomeMan
 * @date 2023-09-04
 * @copyright Ruuvi Innovations Ltd, license BSD-3-Clause.
 */

#include "adv_post_timers.h"
#include "os_timer_sig.h"
#include "adv_post_signals.h"
#include "adv_post_internal.h"
#include "ruuvi_gateway.h"
#include "time_units.h"
#include "network_timeout.h"
#if defined(RUUVI_TESTS) && RUUVI_TESTS
#define LOG_LOCAL_DISABLED 1
#define LOG_LOCAL_LEVEL    LOG_LEVEL_NONE
#else
#define LOG_LOCAL_LEVEL LOG_LEVEL_INFO
#endif
#include "log.h"
static const char* TAG = "ADV_POST_TIMERS";

typedef struct adv_post_timer_t
{
    uint32_t num;
    uint32_t default_interval_ms;
} adv_post_timer_t;

typedef struct timer_sig_periodic_desc_t
{
    const char*            p_timer_name;
    adv_post_sig_e         sig;
    const os_delta_ticks_t period_ticks;
} timer_sig_periodic_desc_t;

typedef struct timer_sig_one_shot_desc_t
{
    const char*            p_timer_name;
    adv_post_sig_e         sig;
    const os_delta_ticks_t period_ticks;
} timer_sig_one_shot_desc_t;

typedef enum adv_post_timer_sig_periodic_e
{
    ADV_POST_PERIODIC_TIMER_SIG_RETRANSMIT,
    ADV_POST_PERIODIC_TIMER_SIG_RETRANSMIT2,
    ADV_POST_PERIODIC_TIMER_SIG_SEND_STATISTICS,
    ADV_POST_PERIODIC_TIMER_SIG_MQTT,
    ADV_POST_PERIODIC_TIMER_SIG_NETWORK_WATCHDOG,
    ADV_POST_PERIODIC_TIMER_SIG_WATCHDOG_FEED,
    ADV_POST_PERIODIC_TIMER_SIG_GREEN_LED_UPDATE,
    ADV_POST_PERIODIC_TIMER_SIG_NUM,
} adv_post_timer_sig_periodic_e;

typedef enum adv_post_timer_sig_one_shot_e
{
    ADV_POST_ONE_SHOT_TIMER_SIG_DO_ASYNC_COMM,
    ADV_POST_ONE_SHOT_TIMER_SIG_RECV_ADV_TIMEOUT,
    ADV_POST_ONE_SHOT_TIMER_SIG_NUM,
} adv_post_timer_sig_one_shot_e;

os_timer_sig_periodic_static_t g_adv_post_periodic_timer_sig_mem[ADV_POST_PERIODIC_TIMER_SIG_NUM];
os_timer_sig_periodic_t*       g_p_adv_post_periodic_timer_sig[ADV_POST_PERIODIC_TIMER_SIG_NUM];

os_timer_sig_one_shot_static_t g_adv_post_one_shot_timer_sig_mem[ADV_POST_ONE_SHOT_TIMER_SIG_NUM];
os_timer_sig_one_shot_t*       g_p_adv_post_one_shot_timer_sig[ADV_POST_ONE_SHOT_TIMER_SIG_NUM];

static const timer_sig_periodic_desc_t g_adv_post_periodic_timer_sig[ADV_POST_PERIODIC_TIMER_SIG_NUM] = {
    [ADV_POST_PERIODIC_TIMER_SIG_RETRANSMIT] = {
        .p_timer_name = "adv_post_retransmit",
        .sig = ADV_POST_SIG_RETRANSMIT,
        .period_ticks = pdMS_TO_TICKS(ADV_POST_DEFAULT_INTERVAL_SECONDS * TIME_UNITS_MS_PER_SECOND),
    },
    [ADV_POST_PERIODIC_TIMER_SIG_RETRANSMIT2] = {
        .p_timer_name = "adv_post_retransmit2",
        .sig = ADV_POST_SIG_RETRANSMIT2,
        .period_ticks = pdMS_TO_TICKS(ADV_POST_DEFAULT_INTERVAL_SECONDS * TIME_UNITS_MS_PER_SECOND),
    },
    [ADV_POST_PERIODIC_TIMER_SIG_MQTT] = {
        .p_timer_name = "adv_post_send_mqtt",
        .sig = ADV_POST_SIG_RETRANSMIT_MQTT,
        .period_ticks = 0,
    },
    [ADV_POST_PERIODIC_TIMER_SIG_SEND_STATISTICS] = {
        .p_timer_name = "adv_post_send_stat",
        .sig = ADV_POST_SIG_SEND_STATISTICS,
        .period_ticks = pdMS_TO_TICKS(ADV_POST_STATISTICS_INTERVAL_SECONDS) * TIME_UNITS_MS_PER_SECOND,
    },
    [ADV_POST_PERIODIC_TIMER_SIG_NETWORK_WATCHDOG] = {
        .p_timer_name = "adv_post:netwdog",
        .sig = ADV_POST_SIG_NETWORK_WATCHDOG,
        .period_ticks = pdMS_TO_TICKS(RUUVI_NETWORK_WATCHDOG_PERIOD_SECONDS * TIME_UNITS_MS_PER_SECOND),
    },
    [ADV_POST_PERIODIC_TIMER_SIG_WATCHDOG_FEED] = {
        .p_timer_name = "adv_post:wdog",
        .sig = ADV_POST_SIG_TASK_WATCHDOG_FEED,
        .period_ticks = ADV_POST_TASK_WATCHDOG_FEEDING_PERIOD_TICKS,
    },
    [ADV_POST_PERIODIC_TIMER_SIG_GREEN_LED_UPDATE] = {
        .p_timer_name = "adv_post:led",
        .sig = ADV_POST_SIG_GREEN_LED_UPDATE,
        .period_ticks = ADV_POST_TASK_LED_UPDATE_PERIOD_TICKS,
    },
};

static const timer_sig_one_shot_desc_t g_adv_post_one_shot_timer_sig[ADV_POST_ONE_SHOT_TIMER_SIG_NUM] = {
    [ADV_POST_ONE_SHOT_TIMER_SIG_DO_ASYNC_COMM] = {
        .p_timer_name = "adv_post_async",
        .sig = ADV_POST_SIG_DO_ASYNC_COMM,
        .period_ticks = pdMS_TO_TICKS(ADV_POST_DO_ASYNC_COMM_INTERVAL_MS),
    },
    [ADV_POST_ONE_SHOT_TIMER_SIG_RECV_ADV_TIMEOUT] = {
        .p_timer_name = "adv_post:timeout",
        .sig = ADV_POST_SIG_RECV_ADV_TIMEOUT,
        .period_ticks = ADV_POST_TASK_RECV_ADV_TIMEOUT_TICKS,
    },
};

static adv_post_timer_t g_adv_post_timers[2] = {
    {
        .num                 = 1,
        .default_interval_ms = ADV_POST_DEFAULT_INTERVAL_SECONDS * TIME_UNITS_MS_PER_SECOND,
    },
    {
        .num                 = 2,
        .default_interval_ms = ADV_POST_DEFAULT_INTERVAL_SECONDS * TIME_UNITS_MS_PER_SECOND,
    },
};

void
adv_post_create_timers(void)
{
    for (uint32_t i = 0; i < ADV_POST_PERIODIC_TIMER_SIG_NUM; ++i)
    {
        const timer_sig_periodic_desc_t* const p_timer_sig_desc = &g_adv_post_periodic_timer_sig[i];

        g_p_adv_post_periodic_timer_sig[i] = os_timer_sig_periodic_create_static(
            &g_adv_post_periodic_timer_sig_mem[i],
            p_timer_sig_desc->p_timer_name,
            adv_post_signals_get(),
            adv_post_conv_to_sig_num(p_timer_sig_desc->sig),
            p_timer_sig_desc->period_ticks);
    }
    for (uint32_t i = 0; i < ADV_POST_ONE_SHOT_TIMER_SIG_NUM; ++i)
    {
        const timer_sig_one_shot_desc_t* const p_timer_sig_desc = &g_adv_post_one_shot_timer_sig[i];

        g_p_adv_post_one_shot_timer_sig[i] = os_timer_sig_one_shot_create_static(
            &g_adv_post_one_shot_timer_sig_mem[i],
            p_timer_sig_desc->p_timer_name,
            adv_post_signals_get(),
            adv_post_conv_to_sig_num(p_timer_sig_desc->sig),
            p_timer_sig_desc->period_ticks);
    }
}

void
adv_post_delete_timers(void)
{
    for (uint32_t i = 0; i < ADV_POST_PERIODIC_TIMER_SIG_NUM; ++i)
    {
        os_timer_sig_periodic_stop(g_p_adv_post_periodic_timer_sig[i]);
        os_timer_sig_periodic_delete(&g_p_adv_post_periodic_timer_sig[i]);
    }
    for (uint32_t i = 0; i < ADV_POST_ONE_SHOT_TIMER_SIG_NUM; ++i)
    {
        os_timer_sig_one_shot_stop(g_p_adv_post_one_shot_timer_sig[i]);
        os_timer_sig_one_shot_delete(&g_p_adv_post_one_shot_timer_sig[i]);
    }
}

os_timer_sig_periodic_t*
adv_post_timers_get_timer_sig_green_led_update(void)
{
    return g_p_adv_post_periodic_timer_sig[ADV_POST_PERIODIC_TIMER_SIG_GREEN_LED_UPDATE];
}

void
adv_post_timers_relaunch_timer_sig_retransmit_to_http_ruuvi(void)
{
    os_timer_sig_periodic_t* const p_timer_sig
        = g_p_adv_post_periodic_timer_sig[ADV_POST_PERIODIC_TIMER_SIG_RETRANSMIT];
    LOG_INFO("%s", __func__);
    os_timer_sig_periodic_relaunch(p_timer_sig, true);
}

void
adv_post_timers_relaunch_timer_sig_retransmit_to_http_custom(void)
{
    os_timer_sig_periodic_t* const p_timer_sig
        = g_p_adv_post_periodic_timer_sig[ADV_POST_PERIODIC_TIMER_SIG_RETRANSMIT2];
    LOG_INFO("%s", __func__);
    os_timer_sig_periodic_relaunch(p_timer_sig, true);
}

void
adv1_post_timer_stop(void)
{
    os_timer_sig_periodic_t* const p_timer_sig
        = g_p_adv_post_periodic_timer_sig[ADV_POST_PERIODIC_TIMER_SIG_RETRANSMIT];
    LOG_INFO("%s", __func__);
    os_timer_sig_periodic_stop(p_timer_sig);
}

void
adv2_post_timer_stop(void)
{
    os_timer_sig_periodic_t* const p_timer_sig
        = g_p_adv_post_periodic_timer_sig[ADV_POST_PERIODIC_TIMER_SIG_RETRANSMIT2];
    LOG_INFO("%s", __func__);
    os_timer_sig_periodic_stop(p_timer_sig);
}

void
adv_post_timers_restart_timer_sig_mqtt(const os_delta_ticks_t delay_ticks)
{
    os_timer_sig_periodic_t* const p_timer_sig = g_p_adv_post_periodic_timer_sig[ADV_POST_PERIODIC_TIMER_SIG_MQTT];
    os_timer_sig_periodic_restart_with_period(p_timer_sig, delay_ticks, false);
}

void
adv_post_timers_stop_timer_sig_mqtt(void)
{
    os_timer_sig_periodic_t* const p_timer_sig = g_p_adv_post_periodic_timer_sig[ADV_POST_PERIODIC_TIMER_SIG_MQTT];
    os_timer_sig_periodic_stop(p_timer_sig);
}

void
adv_post_timers_stop_timer_sig_send_statistics(void)
{
    os_timer_sig_periodic_t* const p_timer_sig
        = g_p_adv_post_periodic_timer_sig[ADV_POST_PERIODIC_TIMER_SIG_SEND_STATISTICS];
    LOG_INFO("%s", __func__);
    os_timer_sig_periodic_stop(p_timer_sig);
}

void
adv_post_timers_relaunch_timer_sig_send_statistics(void)
{
    os_timer_sig_periodic_t* const p_timer_sig
        = g_p_adv_post_periodic_timer_sig[ADV_POST_PERIODIC_TIMER_SIG_SEND_STATISTICS];
    LOG_INFO("%s", __func__);
    os_timer_sig_periodic_relaunch(p_timer_sig, true);
}

void
adv_post_timers_postpone_sending_statistics(void)
{
    LOG_INFO("%s", __func__);
    os_timer_sig_periodic_update_timestamp_when_timer_was_triggered(
        g_p_adv_post_periodic_timer_sig[ADV_POST_PERIODIC_TIMER_SIG_SEND_STATISTICS],
        xTaskGetTickCount()
            - pdMS_TO_TICKS(
                (ADV_POST_STATISTICS_INTERVAL_SECONDS - ADV_POST_INITIAL_DELAY_IN_SENDING_STATISTICS_SECONDS)
                * TIME_UNITS_MS_PER_SECOND));
}

void
adv_post_timers_start_timer_sig_do_async_comm(void)
{
    os_timer_sig_one_shot_start(g_p_adv_post_one_shot_timer_sig[ADV_POST_ONE_SHOT_TIMER_SIG_DO_ASYNC_COMM]);
}

void
adv_post_timers_stop_timer_sig_do_async_comm(void)
{
    os_timer_sig_one_shot_stop(g_p_adv_post_one_shot_timer_sig[ADV_POST_ONE_SHOT_TIMER_SIG_DO_ASYNC_COMM]);
}

void
adv_post_timers_start_timer_sig_watchdog_feed(void)
{
    os_timer_sig_periodic_t* const p_timer_sig
        = g_p_adv_post_periodic_timer_sig[ADV_POST_PERIODIC_TIMER_SIG_WATCHDOG_FEED];
    os_timer_sig_periodic_start(p_timer_sig);
}

void
adv_post_timers_start_timer_sig_network_watchdog(void)
{
    network_timeout_update_timestamp();
    os_timer_sig_periodic_t* const p_timer_sig
        = g_p_adv_post_periodic_timer_sig[ADV_POST_PERIODIC_TIMER_SIG_NETWORK_WATCHDOG];
    os_timer_sig_periodic_start(p_timer_sig);
}

void
adv_post_timers_stop_timer_sig_network_watchdog(void)
{
    os_timer_sig_periodic_t* const p_timer_sig
        = g_p_adv_post_periodic_timer_sig[ADV_POST_PERIODIC_TIMER_SIG_NETWORK_WATCHDOG];
    os_timer_sig_periodic_stop(p_timer_sig);
}

void
adv_post_timers_start_timer_sig_recv_adv_timeout(void)
{
    os_timer_sig_one_shot_t* const p_timer_sig
        = g_p_adv_post_one_shot_timer_sig[ADV_POST_ONE_SHOT_TIMER_SIG_RECV_ADV_TIMEOUT];
    os_timer_sig_one_shot_start(p_timer_sig);
}

void
adv_post_timers_relaunch_timer_sig_recv_adv_timeout(void)
{
    os_timer_sig_one_shot_relaunch(g_p_adv_post_one_shot_timer_sig[ADV_POST_ONE_SHOT_TIMER_SIG_RECV_ADV_TIMEOUT], true);
}

static void
adv_post_timer_restart_from_current_moment(
    const uint32_t                 advs_num,
    os_timer_sig_periodic_t* const p_timer_sig,
    const uint32_t                 period_ms)
{
    const os_delta_ticks_t period_ticks = pdMS_TO_TICKS(period_ms);
    LOG_INFO(
        "advs%u: restart timer from current moment with period %u ms",
        (printf_uint_t)advs_num,
        (printf_uint_t)period_ms);
    os_timer_sig_periodic_set_period(p_timer_sig, period_ticks);
    os_timer_sig_periodic_relaunch(p_timer_sig, true);
}

static void
adv_post_timer_relaunch_if_period_changed(
    const uint32_t                 advs_num,
    os_timer_sig_periodic_t* const p_timer_sig,
    const uint32_t                 period_ms)
{
    LOG_DBG("%s: period_ms=%d", __func__, period_ms);
    const os_delta_ticks_t period_ticks = pdMS_TO_TICKS(period_ms);
    if ((!os_timer_sig_periodic_is_active(p_timer_sig))
        || (os_timer_sig_periodic_get_period(p_timer_sig) != period_ticks))
    {
        LOG_INFO("advs%u: restart timer with period %u ms", (printf_uint_t)advs_num, (printf_uint_t)period_ms);
        os_timer_sig_periodic_set_period(p_timer_sig, period_ticks);
        os_timer_sig_periodic_relaunch(p_timer_sig, false);
    }
    else
    {
        LOG_INFO(
            "advs%u: no need to restart timer - already started with period %u ms",
            (printf_uint_t)advs_num,
            (printf_uint_t)period_ms);
    }
}

void
adv1_post_timer_restart_from_current_moment(void)
{
    const adv_post_timer_t* const p_adv_post_timer = &g_adv_post_timers[0];
    adv_post_timer_restart_from_current_moment(
        p_adv_post_timer->num,
        g_p_adv_post_periodic_timer_sig[ADV_POST_PERIODIC_TIMER_SIG_RETRANSMIT],
        p_adv_post_timer->default_interval_ms);
}

void
adv1_post_timer_relaunch_with_default_period(void)
{
    const adv_post_timer_t* const p_adv_post_timer = &g_adv_post_timers[0];
    LOG_INFO("%s: interval_ms=%d", __func__, p_adv_post_timer->default_interval_ms);
    adv_post_timer_relaunch_if_period_changed(
        p_adv_post_timer->num,
        g_p_adv_post_periodic_timer_sig[ADV_POST_PERIODIC_TIMER_SIG_RETRANSMIT],
        p_adv_post_timer->default_interval_ms);
}

void
adv1_post_timer_relaunch_with_increased_period(void)
{
    const adv_post_timer_t* const p_adv_post_timer = &g_adv_post_timers[0];
    LOG_INFO("%s: interval_ms=%d", __func__, ADV_POST_DELAY_BEFORE_RETRYING_POST_AFTER_ERROR_MS);
    adv_post_timer_relaunch_if_period_changed(
        p_adv_post_timer->num,
        g_p_adv_post_periodic_timer_sig[ADV_POST_PERIODIC_TIMER_SIG_RETRANSMIT],
        ADV_POST_DELAY_BEFORE_RETRYING_POST_AFTER_ERROR_MS);
}

void
adv2_post_timer_restart_from_current_moment(void)
{
    const adv_post_timer_t* const p_adv_post_timer = &g_adv_post_timers[1];
    adv_post_timer_restart_from_current_moment(
        p_adv_post_timer->num,
        g_p_adv_post_periodic_timer_sig[ADV_POST_PERIODIC_TIMER_SIG_RETRANSMIT2],
        p_adv_post_timer->default_interval_ms);
}

void
adv2_post_timer_relaunch_with_default_period(void)
{
    const adv_post_timer_t* const p_adv_post_timer = &g_adv_post_timers[1];
    LOG_INFO("%s: interval_ms=%d", __func__, p_adv_post_timer->default_interval_ms);
    adv_post_timer_relaunch_if_period_changed(
        p_adv_post_timer->num,
        g_p_adv_post_periodic_timer_sig[ADV_POST_PERIODIC_TIMER_SIG_RETRANSMIT2],
        p_adv_post_timer->default_interval_ms);
}

void
adv2_post_timer_relaunch_with_increased_period(void)
{
    const adv_post_timer_t* const p_adv_post_timer = &g_adv_post_timers[1];
    LOG_INFO("%s: interval_ms=%d", __func__, ADV_POST_DELAY_BEFORE_RETRYING_POST_AFTER_ERROR_MS);
    adv_post_timer_relaunch_if_period_changed(
        p_adv_post_timer->num,
        g_p_adv_post_periodic_timer_sig[ADV_POST_PERIODIC_TIMER_SIG_RETRANSMIT2],
        ADV_POST_DELAY_BEFORE_RETRYING_POST_AFTER_ERROR_MS);
}

void
adv2_post_timer_set_default_period(const uint32_t period_ms)
{
    LOG_INFO("%s: Set default period: %d ms", __func__, period_ms);
    adv_post_timer_t* const p_adv_post_timer = &g_adv_post_timers[1];
    p_adv_post_timer->default_interval_ms    = period_ms;
}

static void
adv_post_timer_set_default_period_by_server_resp(adv_post_timer_t* const p_adv_post_timer, const uint32_t period_ms)
{
    LOG_DBG("%s", __func__);
    if (period_ms != p_adv_post_timer->default_interval_ms)
    {
        LOG_INFO(
            "X-Ruuvi-Gateway-Rate: adv%d: Change period from %u ms to %u ms",
            p_adv_post_timer->num,
            (printf_uint_t)p_adv_post_timer->default_interval_ms,
            (printf_uint_t)period_ms);
        p_adv_post_timer->default_interval_ms = period_ms;
    }
}

void
adv1_post_timer_set_default_period_by_server_resp(const uint32_t period_ms)
{
    LOG_DBG("%s", __func__);
    adv_post_timer_set_default_period_by_server_resp(&g_adv_post_timers[0], period_ms);
}

void
adv2_post_timer_set_default_period_by_server_resp(const uint32_t period_ms)
{
    LOG_DBG("%s", __func__);
    adv_post_timer_set_default_period_by_server_resp(&g_adv_post_timers[1], period_ms);
}
