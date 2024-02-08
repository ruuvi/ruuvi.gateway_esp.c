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
 * @brief      Clear all saved TLS session tickets.
 */
void
esp_transport_ssl_clear_saved_session_tickets(void);

#ifdef __cplusplus
}
#endif
#endif /* _ESP_TRANSPORT_SSL_H_ */
