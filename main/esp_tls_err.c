/**
 * @file esp_tls_err.c
 * @author TheSomeMan
 * @date 2023-07-26
 * @copyright Ruuvi Innovations Ltd, license BSD-3-Clause.
 */

#include "esp_tls_err.h"
#include "esp_tls.h"

bool
esp_tls_err_is_cannot_resolve_hostname(const esp_err_t err)
{
    if (ESP_ERR_ESP_TLS_CANNOT_RESOLVE_HOSTNAME == err)
    {
        return true;
    }
    return false;
}

bool
esp_tls_err_is_failed_connect_to_host(const esp_err_t err)
{
    if (ESP_ERR_ESP_TLS_FAILED_CONNECT_TO_HOST == err)
    {
        return true;
    }
    return false;
}

bool
esp_tls_err_is_ssl_alloc_failed(const esp_err_t err)
{
    if (MBEDTLS_ERR_SSL_ALLOC_FAILED == err)
    {
        return true;
    }
    return false;
}
