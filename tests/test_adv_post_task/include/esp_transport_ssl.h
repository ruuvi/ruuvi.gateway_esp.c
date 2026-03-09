/*
 * SPDX-FileCopyrightText: 2015-2022 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef _ESP_TRANSPORT_SSL_H_
#define _ESP_TRANSPORT_SSL_H_

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Configuration for pre-allocated TLS buffers.
 *
 * This structure defines the parameters for pre-allocated incoming and outgoing TLS buffers.
 * It is used to provide memory for the TLS layer to avoid dynamic allocations or to use specific memory areas.
 *
 * @note If pre-allocated buffers are not used, then pointers must be set to NULL and sizes must be set to 0.
 *
 * Buffer size configuration depends on the macro @c CONFIG_MBEDTLS_SSL_VARIABLE_BUFFER_LENGTH.
 * - When CONFIG_MBEDTLS_SSL_VARIABLE_BUFFER_LENGTH is not defined, the buffer sizes are fixed.
 *   - 'p_ssl_in_buf' must be set to a valid buffer address of @c MBEDTLS_SSL_IN_BUFFER_LEN size.
 *   - 'ssl_in_buf_len' must be set to @c MBEDTLS_SSL_IN_BUFFER_LEN
 *   - 'ssl_in_content_len' must be set to @c MBEDTLS_SSL_IN_CONTENT_LEN
 *   - 'p_ssl_out_buf' must be set to a valid buffer address of @c MBEDTLS_SSL_OUT_BUFFER_LEN size.
 *   - 'ssl_out_buf_len' must be set to @c MBEDTLS_SSL_OUT_BUFFER_LEN
 *   - 'ssl_out_content_len' must be set to @c MBEDTLS_SSL_OUT_CONTENT_LEN
 * - When CONFIG_MBEDTLS_SSL_VARIABLE_BUFFER_LENGTH is defined, the buffer sizes are variable and can be set
 * dynamically.
 *   - 'p_ssl_in_buf' must be set to a valid buffer address of @c MBEDTLS_SSL_IN_BUFFER_LEN_CALC(ssl_in_content_len)
 * size.
 *   - 'ssl_in_buf_len' must be set to @c MBEDTLS_SSL_IN_BUFFER_LEN_CALC(ssl_in_content_len)
 *   - 'ssl_in_content_len' can be set to any user selected value.
 *   - 'p_ssl_out_buf' must be set to a valid buffer address of @c MBEDTLS_SSL_OUT_BUFFER_LEN_CALC(ssl_out_content_len)
 * size.
 *   - 'ssl_out_buf_len' must be set to @c MBEDTLS_SSL_OUT_BUFFER_LEN_CALC(ssl_out_content_len)
 *   - 'ssl_out_content_len' must be set to any user selected value.
 */
typedef struct esp_transport_ssl_buf_cfg_t
{
    uint8_t* p_ssl_in_buf;        /*!< Pointer to pre-allocated buffer for incoming TLS data. */
    size_t   ssl_in_buf_len;      /*!< Size of the pre-allocated buffer for incoming TLS data. */
    size_t   ssl_in_content_len;  /*!< Maximum incoming fragment length in bytes. */
    uint8_t* p_ssl_out_buf;       /*!< Pointer to pre-allocated buffer for outgoing TLS data. */
    size_t   ssl_out_buf_len;     /*!< Size of the pre-allocated buffer for outgoing TLS data. */
    size_t   ssl_out_content_len; /*!< Maximum outgoing fragment length in bytes. */
} esp_transport_ssl_buf_cfg_t;

/**
 * @brief      Clear all saved TLS session tickets.
 */
void
esp_transport_ssl_clear_saved_session_tickets(void);

#ifdef __cplusplus
}
#endif
#endif /* _ESP_TRANSPORT_SSL_H_ */
