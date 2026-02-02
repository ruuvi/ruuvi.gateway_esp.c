/**
 * @file adv_mqtt.c
 * @author TheSomeMan
 * @date 2023-10-27
 * @copyright Ruuvi Innovations Ltd, license BSD-3-Clause.
 */

#include "adv_mqtt.h"
#include <stdint.h>
#include <stdbool.h>
#include "os_task.h"
#include "adv_mqtt_task.h"
#include "adv_mqtt_signals.h"
#define LOG_LOCAL_LEVEL LOG_LEVEL_INFO
#include "log.h"
static const char TAG[] = "ADV_MQTT";

void
adv_mqtt_init(void)
{
    const uint32_t           stack_size    = (1024U * 4U);
    const os_task_priority_t task_priority = 6;
    if (!os_task_create_finite_without_param(&adv_mqtt_task, "adv_mqtt_task", stack_size, task_priority))
    {
        LOG_ERR("Can't create thread");
    }
    while (!adv_mqtt_is_initialized())
    {
        vTaskDelay(1);
    }
}

bool
adv_mqtt_is_initialized(void)
{
    return adv_mqtt_signals_is_thread_registered();
}

void
adv_mqtt_stop(void)
{
    LOG_INFO("adv_mqtt_stop");
    adv_mqtt_signals_send_sig(ADV_MQTT_SIG_STOP);
}
