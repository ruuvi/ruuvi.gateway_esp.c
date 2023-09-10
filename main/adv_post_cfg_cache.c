/**
 * @file adv_post_cfg_cache.c
 * @author TheSomeMan
 * @date 2023-09-04
 * @copyright Ruuvi Innovations Ltd, license BSD-3-Clause.
 */

#include "adv_post_cfg_cache.h"
#include <stddef.h>
#include "os_mutex.h"

static adv_post_cfg_cache_t g_adv_post_cfg_cache;
static os_mutex_t           g_p_adv_post_cfg_cache_access_mutex;
static os_mutex_static_t    g_adv_post_cfg_cache_access_mutex_mem;

adv_post_cfg_cache_t*
adv_post_cfg_access_mutex_try_lock(void)
{
    if (NULL == g_p_adv_post_cfg_cache_access_mutex)
    {
        g_p_adv_post_cfg_cache_access_mutex = os_mutex_create_static(&g_adv_post_cfg_cache_access_mutex_mem);
    }
    if (!os_mutex_try_lock(g_p_adv_post_cfg_cache_access_mutex))
    {
        return NULL;
    }
    return &g_adv_post_cfg_cache;
}

adv_post_cfg_cache_t*
adv_post_cfg_access_mutex_lock(void)
{
    if (NULL == g_p_adv_post_cfg_cache_access_mutex)
    {
        g_p_adv_post_cfg_cache_access_mutex = os_mutex_create_static(&g_adv_post_cfg_cache_access_mutex_mem);
    }
    os_mutex_lock(g_p_adv_post_cfg_cache_access_mutex);
    return &g_adv_post_cfg_cache;
}

void
adv_post_cfg_access_mutex_unlock(adv_post_cfg_cache_t** p_p_cfg_cache)
{
    *p_p_cfg_cache = NULL;
    os_mutex_unlock(g_p_adv_post_cfg_cache_access_mutex);
}
