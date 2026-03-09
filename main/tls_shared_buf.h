/**
 * @copyright Ruuvi Innovations Ltd, license BSD-3-Clause.
 * @brief API for managing TLS shared buffers.
 *
 * Ruuvi Gateway manages memory by sharing TLS buffers between different tasks.
 * Two configurations are supported:
 *
 * 1. Concurrent HTTPS POST and MQTT (over TLS or Secure WebSocket):
 *    - Each uses a reduced-size buffer to save RAM.
 *    - HTTPS POST: 8192 bytes (in), 4096 bytes (out).
 *    - MQTT: 8192 bytes (in), 4096 bytes (out).
 *
 * 2. Single HTTPS Download (e.g., for firmware updates):
 *    - Uses a full-size TLS buffer.
 *    - 16384 bytes (in), 4096 bytes (out).
 *
 * The memory for (1) is shared with the memory for (2). Therefore, HTTPS POST/MQTT
 * and HTTPS Download MUST NOT be used simultaneously.
 *
 * A mutex-based protection mechanism ensures exclusive access. Attempting to
 * acquire a buffer while it's already in use by a conflicting task will result
 * in a software assertion and system reset.
 *
 * Before starting an HTTPS download, @c gw_status_suspend_relaying must be called
 * to suspend HTTPS POST and MQTT tasks.
 */

#ifndef RUUVI_GATEWAY_ESP_TLS_SHARED_BUF_H
#define RUUVI_GATEWAY_ESP_TLS_SHARED_BUF_H

#include <stdint.h>
#include "mbedtls/ssl_misc.h"
#include "ruuvi_gateway.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct tls_shared_buf_https_post_t
{
    uint8_t in_buf[MBEDTLS_SSL_IN_BUFFER_LEN_CALC(RUUVI_HTTPS_POST_TLS_IN_CONTENT_LEN)];
    uint8_t out_buf[MBEDTLS_SSL_OUT_BUFFER_LEN_CALC(RUUVI_HTTPS_POST_TLS_OUT_CONTENT_LEN)];
} tls_shared_buf_https_post_t;

typedef struct tls_shared_buf_mqtts_t
{
    uint8_t in_buf[MBEDTLS_SSL_IN_BUFFER_LEN_CALC(RUUVI_MQTT_TLS_IN_CONTENT_LEN)];
    uint8_t out_buf[MBEDTLS_SSL_OUT_BUFFER_LEN_CALC(RUUVI_MQTT_TLS_OUT_CONTENT_LEN)];
} tls_shared_buf_mqtts_t;

typedef struct tls_shared_buf_https_download_t
{
    uint8_t in_buf[MBEDTLS_SSL_IN_BUFFER_LEN_CALC(MBEDTLS_SSL_IN_CONTENT_LEN)];
    uint8_t out_buf[MBEDTLS_SSL_OUT_BUFFER_LEN_CALC(MBEDTLS_SSL_OUT_CONTENT_LEN)];
} tls_shared_buf_https_download_t;

/**
 * @brief Initialize the TLS shared buffer module.
 *
 * This function creates the necessary mutexes for managing access to the shared buffers.
 * It must be called before any other functions in this module.
 */
void
tls_shared_buf_init(void);

/**
 * @brief Get access to the HTTPS POST TLS shared buffer.
 *
 * This function locks the HTTPS POST buffer. If the buffer is already in use
 * (either by another HTTPS POST or by an HTTPS Download), it triggers an assertion.
 *
 * @return A pointer to the HTTPS POST shared buffer.
 */
tls_shared_buf_https_post_t*
tls_shared_buf_get_https_post(void);

/**
 * @brief Unlock and release the HTTPS POST TLS shared buffer.
 *
 * @param[in,out] p_p_buf Pointer to the buffer pointer. It will be set to NULL after unlocking.
 */
void
tls_shared_buf_unlock_https_post(tls_shared_buf_https_post_t** p_p_buf);

/**
 * @brief Get access to the MQTT TLS shared buffer.
 *
 * This function locks the MQTT buffer. If the buffer is already in use
 * (either by another MQTT task or by an HTTPS Download), it triggers an assertion.
 *
 * @return A pointer to the MQTT shared buffer.
 */
tls_shared_buf_mqtts_t*
tls_shared_buf_get_mqtts(void);

/**
 * @brief Unlock and release the MQTT TLS shared buffer.
 *
 * @param[in,out] p_p_buf Pointer to the buffer pointer. It will be set to NULL after unlocking.
 */
void
tls_shared_buf_unlock_mqtts(tls_shared_buf_mqtts_t** p_p_buf);

/**
 * @brief Get access to the HTTPS Download TLS shared buffer.
 *
 * This function locks both the HTTPS POST and MQTT buffers to ensure exclusive access
 * to the full-size buffer. If either is already in use, it triggers an assertion.
 *
 * @return A pointer to the HTTPS Download shared buffer.
 */
tls_shared_buf_https_download_t*
tls_shared_buf_get_https_download(void);

/**
 * @brief Unlock and release the HTTPS Download TLS shared buffer.
 *
 * Releases both mutexes held for the download buffer.
 *
 * @param[in,out] p_p_buf Pointer to the buffer pointer. It will be set to NULL after unlocking.
 */
void
tls_shared_buf_unlock_https_download(tls_shared_buf_https_download_t** p_p_buf);

#ifdef __cplusplus
}
#endif

#endif // RUUVI_GATEWAY_ESP_TLS_SHARED_BUF_H
