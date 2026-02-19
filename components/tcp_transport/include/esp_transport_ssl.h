/*
 * SPDX-FileCopyrightText: 2015-2022 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef _ESP_TRANSPORT_SSL_H_
#define _ESP_TRANSPORT_SSL_H_

#include <stdint.h>
#include "esp_transport.h"
#include "esp_tls.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @def ESP_TLS_MAX_NUM_SAVED_SESSIONS
 * @brief Maximum number of TLS session tickets that can be cached for reuse.
 *
 * This macro defines the capacity of the internal saved-session storage used by
 * the SSL/TLS transport layer (for example, for session resumption).
 *
 * @note Increasing this value may increase RAM usage.
 */
#define ESP_TLS_MAX_NUM_SAVED_SESSIONS (2)

/**
 * @brief       Create new SSL transport, the transport handle must be release esp_transport_destroy callback
 *
 * @return      the allocated esp_transport_handle_t, or NULL if the handle can not be allocated
 */
esp_transport_handle_t esp_transport_ssl_init(void);

/**
 * @brief      Set SSL certificate data (as PEM format).
 *             Note that, this function stores the pointer to data, rather than making a copy.
 *             So this data must remain valid until after the connection is cleaned up
 *
 * @param      t     ssl transport
 * @param[in]  data  The pem data
 * @param[in]  len   The length
 */
void esp_transport_ssl_set_cert_data(esp_transport_handle_t t, const char *data, int len);

/**
 * @brief      Set SSL certificate data (as DER format).
 *             Note that, this function stores the pointer to data, rather than making a copy.
 *             So this data must remain valid until after the connection is cleaned up
 *
 * @param      t     ssl transport
 * @param[in]  data  The der data
 * @param[in]  len   The length
 */
void esp_transport_ssl_set_cert_data_der(esp_transport_handle_t t, const char *data, int len);

/**
 * @brief      Enable the use of certification bundle for server verfication for
 *             an SSL connection.
 *             It must be first enabled in menuconfig.
 *
 * @param      t    ssl transport
 * @param[in]  crt_bundle_attach    Function pointer to esp_crt_bundle_attach
 */
void esp_transport_ssl_crt_bundle_attach(esp_transport_handle_t t, esp_err_t ((*crt_bundle_attach)(void *conf)));

/**
 * @brief      Enable global CA store for SSL connection
 *
 * @param      t    ssl transport
 */
void esp_transport_ssl_enable_global_ca_store(esp_transport_handle_t t);

/**
 * @brief      Set SSL client certificate data for mutual authentication (as PEM format).
 *             Note that, this function stores the pointer to data, rather than making a copy.
 *             So this data must remain valid until after the connection is cleaned up
 *
 * @param      t     ssl transport
 * @param[in]  data  The pem data
 * @param[in]  len   The length
 */
void esp_transport_ssl_set_client_cert_data(esp_transport_handle_t t, const char *data, int len);

/**
 * @brief      Set SSL client certificate data for mutual authentication (as DER format).
 *             Note that, this function stores the pointer to data, rather than making a copy.
 *             So this data must remain valid until after the connection is cleaned up
 *
 * @param      t     ssl transport
 * @param[in]  data  The der data
 * @param[in]  len   The length
 */
void esp_transport_ssl_set_client_cert_data_der(esp_transport_handle_t t, const char *data, int len);

/**
 * @brief      Set SSL client key data for mutual authentication (as PEM format).
 *             Note that, this function stores the pointer to data, rather than making a copy.
 *             So this data must remain valid until after the connection is cleaned up
 *
 * @param      t     ssl transport
 * @param[in]  data  The pem data
 * @param[in]  len   The length
 */
void esp_transport_ssl_set_client_key_data(esp_transport_handle_t t, const char *data, int len);

/**
 * @brief      Set SSL client key password if the key is password protected. The configured
 *             password is passed to the underlying TLS stack to decrypt the client key
 *
 * @param      t     ssl transport
 * @param[in]  password  Pointer to the password
 * @param[in]  password_len   Password length
 */
void esp_transport_ssl_set_client_key_password(esp_transport_handle_t t, const char *password, int password_len);

/**
 * @brief      Set SSL client key data for mutual authentication (as DER format).
 *             Note that, this function stores the pointer to data, rather than making a copy.
 *             So this data must remain valid until after the connection is cleaned up
 *
 * @param      t     ssl transport
 * @param[in]  data  The der data
 * @param[in]  len   The length
 */
void esp_transport_ssl_set_client_key_data_der(esp_transport_handle_t t, const char *data, int len);

/**
 * @brief      Set the list of supported application protocols to be used with ALPN.
 *             Note that, this function stores the pointer to data, rather than making a copy.
 *             So this data must remain valid until after the connection is cleaned up
 *
 * @param      t            ssl transport
 * @param[in]  alpn_porot   The list of ALPN protocols, the last entry must be NULL
 */
void esp_transport_ssl_set_alpn_protocol(esp_transport_handle_t t, const char **alpn_protos);

/**
 * @brief      Skip validation of certificate's common name field
 *
 * @note       Skipping CN validation is not recommended
 *
 * @param      t     ssl transport
 */
void esp_transport_ssl_skip_common_name_check(esp_transport_handle_t t);

/**
 * @brief      Set the server certificate's common name field
 *
 * @note
 *             If non-NULL, server certificate CN must match this name,
 *             If NULL, server certificate CN must match hostname.
 * @param      t             ssl transport
 *             common_name   A string containing the common name to be set
 */
void esp_transport_ssl_set_common_name(esp_transport_handle_t t, const char *common_name);

/**
 * @brief      Set the ssl context to use secure element (atecc608a) for client(device) private key and certificate
 *
 * @note       Recommended to be used with ESP32-WROOM-32SE (which has inbuilt ATECC608A a.k.a Secure Element)
 *
 * @param      t     ssl transport
 */
void esp_transport_ssl_use_secure_element(esp_transport_handle_t t);

/**
 * @brief      Set the ds_data handle in ssl context.(used for the digital signature operation)
 *
 * @param      t        ssl transport
 *             ds_data  the handle for ds data params
 */
void esp_transport_ssl_set_ds_data(esp_transport_handle_t t, void *ds_data);

/**
 * @brief      Set PSK key and hint for PSK server/client verification in esp-tls component.
 *             Important notes:
 *             - This function stores the pointer to data, rather than making a copy.
 *             So this data must remain valid until after the connection is cleaned up
 *             - ESP_TLS_PSK_VERIFICATION config option must be enabled in menuconfig
 *             - certificate verification takes priority so it must not be configured
 *             to enable PSK method.
 *
 * @param      t             ssl transport
 * @param[in]  psk_hint_key  psk key and hint structure defined in esp_tls.h
 */
void esp_transport_ssl_set_psk_key_hint(esp_transport_handle_t t, const psk_hint_key_t* psk_hint_key);

/**
 * @brief      Set keep-alive status in current ssl context
 *
 * @param[in]  t               ssl transport
 * @param[in]  keep_alive_cfg  The handle for keep-alive configuration
 */
void esp_transport_ssl_set_keep_alive(esp_transport_handle_t t, esp_transport_keep_alive_t *keep_alive_cfg);

/**
 * @brief      Set name of interface that socket can be binded on
 *             So the data can transport on this interface
 *
 * @param[in]  t        The transport handle
 * @param[in]  if_name  The interface name
 */
void esp_transport_ssl_set_interface_name(esp_transport_handle_t t, struct ifreq *if_name);

#if defined(CONFIG_MBEDTLS_SSL_VARIABLE_BUFFER_LENGTH)
 /**
 * @brief      Set buffer size for input and output buffer.
 *
 * @param[in]  t        The transport handle
 * @param[in]  ssl_in_content_len  Maximum incoming fragment length in bytes (default MBEDTLS_SSL_IN_CONTENT_LEN)
 * @param[in]  ssl_out_content_len  Maximum outgoing fragment length in bytes (default MBEDTLS_SSL_OUT_CONTENT_LEN)
 */
void esp_transport_ssl_set_buffer_size(esp_transport_handle_t t,
                                       const size_t ssl_in_content_len,
                                       const size_t ssl_out_content_len);
#endif

/**
 * @brief      Set pre-allocated buffers for input and output buffer.
 *
 * @param[in]  t        The transport handle
 * @param[in]  p_ssl_in_buf Pointer to per-allocated buffer for incoming data. It can be NULL.
 * @param[in]  p_ssl_out_buf Pointer to per-allocated buffer for outgoing data. It can be NULL.
 */
void esp_transport_ssl_set_buffer(esp_transport_handle_t t,
                                  uint8_t *const p_ssl_in_buf,
                                  uint8_t *const p_ssl_out_buf);

/**
 * @brief      Clear all saved TLS session tickets.
 */
void esp_transport_ssl_clear_saved_session_tickets(void);


/**
 * @brief Initialize storage for saved TLS session tickets.
 *
 * This function initializes the storage for saved session tickets, including
 * clearing any existing session data and initializing the session slots.
 *
 * @note This function should be called before using any other session-related
 *       functions to ensure proper initialization.
 */
void esp_transport_ssl_init_saved_tickets_storage(void);

/**
 * @brief Set (or clear) the hostname associated with a saved TLS session ticket slot.
 *
 * This function updates the hostname label used to match a cached session ticket
 * against a target server when attempting session resumption.
 *
 * Passing a non-NULL hostname assigns/overwrites the hostname for the given slot.
 * Passing @c NULL clears the hostname for the slot.
 *
 * @param[in] idx        Saved session slot index. Must be less than
 *                       @c ESP_TLS_MAX_NUM_SAVED_SESSIONS.
 * @param[in] p_hostname Pointer to the hostname to store, or @c NULL to clear it.
 *
 * @return @c true on success, @c false if @p idx is out of range.
 */
bool esp_transport_ssl_set_saved_ticket_hostname(
    const uint32_t idx,
    const mbedtls_ssl_hostname_t* const p_hostname);

#ifdef __cplusplus
}
#endif
#endif /* _ESP_TRANSPORT_SSL_H_ */
