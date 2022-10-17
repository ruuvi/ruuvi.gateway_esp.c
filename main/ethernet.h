/**
 * @file ethernet.h
 * @author Jukka Saari
 * @date 2020-01-27
 * @copyright Ruuvi Innovations Ltd, license BSD-3-Clause.
 */

#ifndef RUUVI_ETHERNET_H
#define RUUVI_ETHERNET_H

#include <stdbool.h>
#include "esp_netif.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*ethernet_cb_link_up_t)(void);
typedef void (*ethernet_cb_link_down_t)(void);
typedef void (*ethernet_cb_connection_ok_t)(const esp_netif_ip_info_t* p_ip_info);

bool
ethernet_init(
    ethernet_cb_link_up_t       ethernet_link_up_cb,
    ethernet_cb_link_down_t     ethernet_link_down_cb,
    ethernet_cb_connection_ok_t ethernet_connection_ok_cb);

void
ethernet_deinit(void);

void
ethernet_start(const char* const hostname);

void
ethernet_stop(void);

void
ethernet_update_ip(void);

#ifdef __cplusplus
}
#endif

#endif // RUUVI_ETHERNET_H
