/**
 * @file esp_tls_err.h
 * @author TheSomeMan
 * @date 2023-07-26
 * @copyright Ruuvi Innovations Ltd, license BSD-3-Clause.
 */

#ifndef ESP_ESP_TLS_ERR_H
#define ESP_ESP_TLS_ERR_H

#include <stdbool.h>
#include <esp_err.h>

#ifdef __cplusplus
extern "C" {
#endif

bool
esp_tls_err_is_cannot_resolve_hostname(const esp_err_t err);

bool
esp_tls_err_is_failed_connect_to_host(const esp_err_t err);

bool
esp_tls_err_is_ssl_alloc_failed(const esp_err_t err);

bool
esp_tls_err_is_ssl_handshake_failed(const esp_err_t err);

#ifdef __cplusplus
}
#endif

#endif // ESP_ESP_TLS_ERR_H
