/**
 * @file validate_url.h
 * @author TheSomeMan
 * @date 2023-07-31
 * @copyright Ruuvi Innovations Ltd, license BSD-3-Clause.
 */

#ifndef VALIDATE_URL_H
#define VALIDATE_URL_H

#include "wifi_manager.h"

#ifdef __cplusplus
extern "C" {
#endif

http_server_resp_t
validate_url(const char* const p_url_params);

#ifdef __cplusplus
}
#endif

#endif // VALIDATE_URL_H
