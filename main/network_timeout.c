/**
 * @file network_timeout.c
 * @author TheSomeMan
 * @date 2023-09-04
 * @copyright Ruuvi Innovations Ltd, license BSD-3-Clause.
 */

#include "network_timeout.h"
#include <stddef.h>
#include <freertos/FreeRTOS.h>
#include <freertos/timers.h>
#include "os_mutex.h"
#include "ruuvi_gateway.h"

static TickType_t        g_network_timeout_last_successful_timestamp;
static os_mutex_t        g_p_network_timeout_mutex;
static os_mutex_static_t g_network_timeout_mutex_mem;

static void
network_timeout_lock(void)
{
    if (NULL == g_p_network_timeout_mutex)
    {
        g_p_network_timeout_mutex = os_mutex_create_static(&g_network_timeout_mutex_mem);
    }
    os_mutex_lock(g_p_network_timeout_mutex);
}

static void
network_timeout_unlock(void)
{
    os_mutex_unlock(g_p_network_timeout_mutex);
}

void
network_timeout_update_timestamp(void)
{
    network_timeout_lock();
    g_network_timeout_last_successful_timestamp = xTaskGetTickCount();
    network_timeout_unlock();
}

bool
network_timeout_check(void)
{
    const TickType_t timeout_ticks = pdMS_TO_TICKS(RUUVI_NETWORK_WATCHDOG_TIMEOUT_SECONDS) * 1000;

    network_timeout_lock();
    const TickType_t delta_ticks = xTaskGetTickCount() - g_network_timeout_last_successful_timestamp;
    network_timeout_unlock();

    return (delta_ticks > timeout_ticks) ? true : false;
}
