/**
 * @file ethernet.h
 * @author Jukka Saari
 * @date 2020-01-27
 * @copyright Ruuvi Innovations Ltd, license BSD-3-Clause.
 */

#ifndef RUUVI_ETHERNET_H
#define RUUVI_ETHERNET_H

#include <stdbool.h>
#include "tcpip_adapter.h"

#ifdef __cplusplus
extern "C" {
#endif

bool
ethernet_init(
    void (*ethernet_link_up_cb)(void),
    void (*ethernet_link_down_cb)(void),
    void (*ethernet_connection_ok_cb)(const tcpip_adapter_ip_info_t *p_ip_info));

void
ethernet_deinit(void);

void
ethernet_start(void);

void
ethernet_stop(void);

void
ethernet_update_ip(void);

#ifdef __cplusplus
}
#endif

#endif // RUUVI_ETHERNET_H
