/**
 * @file adv_mqtt_timers.c
 * @author TheSomeMan
 * @date 2023-10-27
 * @copyright Ruuvi Innovations Ltd, license BSD-3-Clause.
 */

#include "adv_mqtt_timers.h"
#include "os_timer_sig.h"
#include "adv_mqtt_signals.h"
#include "adv_mqtt_internal.h"
#if defined(RUUVI_TESTS) && RUUVI_TESTS
#define LOG_LOCAL_DISABLED 1
#define LOG_LOCAL_LEVEL    LOG_LEVEL_NONE
#else
#define LOG_LOCAL_LEVEL LOG_LEVEL_INFO
#endif
//#include "log.h"
// static const char* TAG = "ADV_MQTT_TASK";

typedef struct timer_sig_periodic_desc_t
{
    const char*            p_timer_name;
    adv_mqtt_sig_e         sig;
    const os_delta_ticks_t period_ticks;
} timer_sig_periodic_desc_t;

typedef struct timer_sig_one_shot_desc_t
{
    const char*            p_timer_name;
    adv_mqtt_sig_e         sig;
    const os_delta_ticks_t period_ticks;
} timer_sig_one_shot_desc_t;

typedef enum adv_mqtt_timer_sig_periodic_e
{
    ADV_MQTT_PERIODIC_TIMER_SIG_WATCHDOG_FEED,
    ADV_MQTT_PERIODIC_TIMER_SIG_NUM,
} adv_mqtt_timer_sig_periodic_e;

typedef enum adv_mqtt_timer_sig_one_shot_e
{
    ADV_MQTT_ONE_SHOT_TIMER_SIG_RETRY_SENDING_ADVS,
    ADV_MQTT_ONE_SHOT_TIMER_SIG_NUM,
} adv_mqtt_timer_sig_one_shot_e;

os_timer_sig_periodic_static_t g_adv_mqtt_periodic_timer_sig_mem[ADV_MQTT_PERIODIC_TIMER_SIG_NUM];
os_timer_sig_periodic_t*       g_p_adv_mqtt_periodic_timer_sig[ADV_MQTT_PERIODIC_TIMER_SIG_NUM];

os_timer_sig_one_shot_static_t g_adv_mqtt_one_shot_timer_sig_mem[ADV_MQTT_ONE_SHOT_TIMER_SIG_NUM];
os_timer_sig_one_shot_t*       g_p_adv_mqtt_one_shot_timer_sig[ADV_MQTT_ONE_SHOT_TIMER_SIG_NUM];

static const timer_sig_periodic_desc_t g_adv_mqtt_periodic_timer_sig[ADV_MQTT_PERIODIC_TIMER_SIG_NUM] = {
    [ADV_MQTT_PERIODIC_TIMER_SIG_WATCHDOG_FEED] = {
        .p_timer_name = "adv_mqtt:wdog",
        .sig = ADV_MQTT_SIG_TASK_WATCHDOG_FEED,
        .period_ticks = ADV_MQTT_TASK_WATCHDOG_FEEDING_PERIOD_TICKS,
    },
};

static const timer_sig_one_shot_desc_t g_adv_mqtt_one_shot_timer_sig[ADV_MQTT_ONE_SHOT_TIMER_SIG_NUM] = {
    [ADV_MQTT_ONE_SHOT_TIMER_SIG_RETRY_SENDING_ADVS] = {
        .p_timer_name = "adv_mqtt:retry",
        .sig = ADV_MQTT_SIG_ON_RECV_ADV,
        .period_ticks = ADV_MQTT_TASK_RETRY_SENDING_ADVS_AFTER_TICKS,
    },
};

void
adv_mqtt_create_timers(void)
{
    for (uint32_t i = 0; i < ADV_MQTT_PERIODIC_TIMER_SIG_NUM; ++i)
    {
        const timer_sig_periodic_desc_t* const p_timer_sig_desc = &g_adv_mqtt_periodic_timer_sig[i];

        g_p_adv_mqtt_periodic_timer_sig[i] = os_timer_sig_periodic_create_static(
            &g_adv_mqtt_periodic_timer_sig_mem[i],
            p_timer_sig_desc->p_timer_name,
            adv_mqtt_signals_get(),
            adv_mqtt_conv_to_sig_num(p_timer_sig_desc->sig),
            p_timer_sig_desc->period_ticks);
    }
    for (uint32_t i = 0; i < ADV_MQTT_ONE_SHOT_TIMER_SIG_NUM; ++i)
    {
        const timer_sig_one_shot_desc_t* const p_timer_sig_desc = &g_adv_mqtt_one_shot_timer_sig[i];

        g_p_adv_mqtt_one_shot_timer_sig[i] = os_timer_sig_one_shot_create_static(
            &g_adv_mqtt_one_shot_timer_sig_mem[i],
            p_timer_sig_desc->p_timer_name,
            adv_mqtt_signals_get(),
            adv_mqtt_conv_to_sig_num(p_timer_sig_desc->sig),
            p_timer_sig_desc->period_ticks);
    }
}

void
adv_mqtt_delete_timers(void)
{
    for (uint32_t i = 0; i < ADV_MQTT_PERIODIC_TIMER_SIG_NUM; ++i)
    {
        os_timer_sig_periodic_stop(g_p_adv_mqtt_periodic_timer_sig[i]);
        os_timer_sig_periodic_delete(&g_p_adv_mqtt_periodic_timer_sig[i]);
    }
    for (uint32_t i = 0; i < ADV_MQTT_ONE_SHOT_TIMER_SIG_NUM; ++i)
    {
        os_timer_sig_one_shot_stop(g_p_adv_mqtt_one_shot_timer_sig[i]);
        os_timer_sig_one_shot_delete(&g_p_adv_mqtt_one_shot_timer_sig[i]);
    }
}

void
adv_mqtt_timers_start_timer_sig_watchdog_feed(void)
{
    os_timer_sig_periodic_t* const p_timer_sig
        = g_p_adv_mqtt_periodic_timer_sig[ADV_MQTT_PERIODIC_TIMER_SIG_WATCHDOG_FEED];
    os_timer_sig_periodic_start(p_timer_sig);
}

void
adv_mqtt_timers_start_timer_sig_retry_sending_advs(void)
{
    os_timer_sig_one_shot_t* const p_timer_sig
        = g_p_adv_mqtt_one_shot_timer_sig[ADV_MQTT_ONE_SHOT_TIMER_SIG_RETRY_SENDING_ADVS];
    os_timer_sig_one_shot_start(p_timer_sig);
}
