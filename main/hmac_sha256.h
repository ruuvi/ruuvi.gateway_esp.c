/**
 * @file hmac_sha256.h
 * @author TheSomeMan
 * @date 2021-07-04
 * @copyright Ruuvi Innovations Ltd, license BSD-3-Clause.
 */

#ifndef RUUVI_GATEWAY_HMAC_SHA256_H
#define RUUVI_GATEWAY_HMAC_SHA256_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "str_buf.h"

#ifdef __cplusplus
extern "C" {
#endif

#define HMAC_SHA256_SIZE (32U)

#define HMAC_SHA256_MAX_KEY_SIZE (64U)

typedef struct hmac_sha256_t
{
    uint8_t buf[HMAC_SHA256_SIZE];
} hmac_sha256_t;

/**
 * @brief Set the secret key for HTTP Ruuvi target.
 * @param p_key - ptr to the string with the secret key
 * @return true if successful, false if the length of the secret key exceeds HMAC_SHA256_MAX_KEY_SIZE
 */
bool
hmac_sha256_set_key_for_http_ruuvi(const char* const p_key);

/**
 * @brief Set the secret key for HTTP custom target.
 * @param p_key - ptr to the string with the secret key
 * @return true if successful, false if the length of the secret key exceeds HMAC_SHA256_MAX_KEY_SIZE
 */
bool
hmac_sha256_set_key_for_http_custom(const char* const p_key);

/**
 * @brief Set the secret key for HTTP stats.
 * @param p_key - ptr to the string with the secret key
 * @return true if successful, false if the length of the secret key exceeds HMAC_SHA256_MAX_KEY_SIZE
 */
bool
hmac_sha256_set_key_for_stats(const char* const p_key);

/**
 * @brief Compute HMAC_SHA256 for the message using the stored secret key for the Ruuvi target and return the result as
 * a binary buffer.
 * @param p_str - ptr to the buffer with a message
 * @param[out] p_hmac_sha256 - ptr to output binary buffer
 * @return true if successful, false - otherwise
 */
bool
hmac_sha256_calc_for_http_ruuvi(const char* const p_str, hmac_sha256_t* const p_hmac_sha256);

/**
 * @brief Compute HMAC_SHA256 for the message using the stored secret key for the custom target and return the result as
 * a binary buffer.
 * @param p_str - ptr to the buffer with a message
 * @param[out] p_hmac_sha256 - ptr to output binary buffer
 * @return true if successful, false - otherwise
 */
bool
hmac_sha256_calc_for_http_custom(const char* const p_str, hmac_sha256_t* const p_hmac_sha256);

/**
 * @brief Compute HMAC_SHA256 for the message using the stored secret key for the stats and return the result as a
 * binary buffer.
 * @param p_str - ptr to the buffer with a message
 * @param msg_len - length of the message
 * @param[out] p_hmac_sha256 - ptr to output binary buffer
 * @return true if successful, false - otherwise
 */
bool
hmac_sha256_calc_for_stats(const char* const p_str, hmac_sha256_t* const p_hmac_sha256);

/**
 * @brief Converts hmac_sha256_t to string in str_buf_t and allocates memory for the string.
 * @param p_hmac_sha256 - ptr to hmac_sha256_t
 * @return 64-byte hex-encoded string in str_buf_t (the allocated memory must be freed by the caller).
 */
str_buf_t
hmac_sha256_to_str_buf(const hmac_sha256_t* const p_hmac_sha256);

/**
 * @brief Check if hmac_sha256_str_t is valid
 * @param p_hmac_sha256_str - ptr to hmac_sha256_str_t
 * @return true if p_hmac_sha256_str contains hex-encoded HMAC_SHA256, false if p_hmac_sha256_str is empty.
 */
bool
hmac_sha256_is_str_valid(const str_buf_t* const p_hmac_sha256_str);

#ifdef __cplusplus
}
#endif

#endif // RUUVI_GATEWAY_HMAC_SHA256_H
