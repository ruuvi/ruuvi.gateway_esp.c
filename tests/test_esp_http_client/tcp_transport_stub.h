/**
 * @file tcp_transport_stub.c
 * @author TheSomeMan
 * @date 2026-04-11
 * @copyright Ruuvi Innovations Ltd, license BSD-3-Clause.
 */

#ifndef RUUVI_GATEWAY_ESP_HTTP_CLIENT_TESTS_TCP_TRANSPORT_STUB_H
#define RUUVI_GATEWAY_ESP_HTTP_CLIENT_TESTS_TCP_TRANSPORT_STUB_H

#include "esp_transport.h"

#ifdef __cplusplus
extern "C" {
#endif

extern int
tcp_connect(esp_transport_handle_t t, const char* host, int port, int timeout_ms);
extern int
tcp_connect_async(esp_transport_handle_t t, const char* host, int port, int timeout_ms);

extern int
tcp_read(esp_transport_handle_t t, char* buffer, int len, int timeout_ms);
extern int
tcp_write(esp_transport_handle_t t, const char* buffer, int len, int timeout_ms);

#ifdef __cplusplus
}
#endif

#endif // RUUVI_GATEWAY_ESP_HTTP_CLIENT_TESTS_TCP_TRANSPORT_STUB_H