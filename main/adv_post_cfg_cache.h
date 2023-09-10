/**
 * @file adv_post_cfg_cache.h
 * @author TheSomeMan
 * @date 2023-09-04
 * @copyright Ruuvi Innovations Ltd, license BSD-3-Clause.
 */

#ifndef RUUVI_GATEWAY_ESP_ADV_POST_CFG_CACHE_H
#define RUUVI_GATEWAY_ESP_ADV_POST_CFG_CACHE_H

#include <stdbool.h>
#include <stdint.h>
#include "mac_addr.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct adv_post_cfg_cache_t
{
    bool               flag_use_ntp;
    bool               scan_filter_allow_listed;
    uint32_t           scan_filter_length;
    mac_address_bin_t* p_arr_of_scan_filter_mac;
} adv_post_cfg_cache_t;

adv_post_cfg_cache_t*
adv_post_cfg_access_mutex_try_lock(void);

adv_post_cfg_cache_t*
adv_post_cfg_access_mutex_lock(void);

void
adv_post_cfg_access_mutex_unlock(adv_post_cfg_cache_t** p_p_cfg_cache);

#ifdef __cplusplus
}
#endif

#endif // RUUVI_GATEWAY_ESP_ADV_POST_CFG_CACHE_H
