/**
 * @file adv_mqtt_cfg_cache.c
 * @author TheSomeMan
 * @date 2023-10-27
 * @copyright Ruuvi Innovations Ltd, license BSD-3-Clause.
 */

#include "adv_mqtt_cfg_cache.h"
#include <stddef.h>
#include "os_mutex.h"

static adv_mqtt_cfg_cache_t g_adv_mqtt_cfg_cache;
static os_mutex_t           g_p_adv_mqtt_cfg_cache_access_mutex;
static os_mutex_static_t    g_adv_mqtt_cfg_cache_access_mutex_mem;

void
adv_mqtt_cfg_cache_init(void)
{
    if (NULL == g_p_adv_mqtt_cfg_cache_access_mutex)
    {
        g_p_adv_mqtt_cfg_cache_access_mutex = os_mutex_create_static(&g_adv_mqtt_cfg_cache_access_mutex_mem);
    }
}

void
adv_mqtt_cfg_cache_deinit(void)
{
    if (NULL != g_p_adv_mqtt_cfg_cache_access_mutex)
    {
        os_mutex_delete(&g_p_adv_mqtt_cfg_cache_access_mutex);
    }
}

adv_mqtt_cfg_cache_t*
adv_mqtt_cfg_cache_mutex_lock(void)
{
    if (NULL == g_p_adv_mqtt_cfg_cache_access_mutex)
    {
        adv_mqtt_cfg_cache_init();
    }
    os_mutex_lock(g_p_adv_mqtt_cfg_cache_access_mutex);
    return &g_adv_mqtt_cfg_cache;
}

void
adv_mqtt_cfg_cache_mutex_unlock(adv_mqtt_cfg_cache_t** p_p_cfg_cache)
{
    *p_p_cfg_cache = NULL;
    os_mutex_unlock(g_p_adv_mqtt_cfg_cache_access_mutex);
}
