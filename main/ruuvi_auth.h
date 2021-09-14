/**
 * @file ruuvi_auth.h
 * @author TheSomeMan
 * @date 2021-09-14
 * @copyright Ruuvi Innovations Ltd, license BSD-3-Clause.
 */

#ifndef RUUVI_GATEWAY_ESP_RUUVI_AUTH_H
#define RUUVI_GATEWAY_ESP_RUUVI_AUTH_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

bool
ruuvi_auth_set_from_config(void);

#ifdef __cplusplus
}
#endif

#endif // RUUVI_GATEWAY_ESP_RUUVI_AUTH_H
