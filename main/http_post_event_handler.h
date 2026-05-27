/**
 * @file http_post_event_handler.h
 * @author TheSomeMan
 * @date 2026-05-24
 * @copyright Ruuvi Innovations Ltd, license BSD-3-Clause.
 */

#ifndef RUUVI_GATEWAY_ESP_HTTP_POST_EVENT_HANDLER_H
#define RUUVI_GATEWAY_ESP_HTTP_POST_EVENT_HANDLER_H

#include <esp_err.h>
#include "esp_http_client.h"

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t
http_post_event_handler(esp_http_client_event_t* p_evt);

#ifdef __cplusplus
}
#endif

#endif // RUUVI_GATEWAY_ESP_HTTP_POST_EVENT_HANDLER_H
