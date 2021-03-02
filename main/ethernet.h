/**
 * @file ethernet.h
 * @author Jukka Saari
 * @date 2020-01-27
 * @copyright Ruuvi Innovations Ltd, license BSD-3-Clause.
 */

#ifndef RUUVI_ETHERNET_H
#define RUUVI_ETHERNET_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

bool
ethernet_init(void);

void
ethernet_update_ip(void);

#ifdef __cplusplus
}
#endif

#endif // RUUVI_ETHERNET_H
