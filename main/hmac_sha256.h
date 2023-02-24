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

#ifdef __cplusplus
extern "C" {
#endif

#define HMAC_SHA256_SIZE (32U)

#define HMAC_SHA256_MAX_KEY_SIZE (64U)

typedef struct hmac_sha256_t
{
    uint8_t buf[HMAC_SHA256_SIZE];
} hmac_sha256_t;

typedef struct hmac_sha256_str_t
{
    char buf[(HMAC_SHA256_SIZE * 2) + 1];
} hmac_sha256_str_t;

/**
 * @brief Set the secret key.
 * @param p_key - ptr to the binary buffer with the secret key
 * @param key_size - size of the binary buffer with the secret key
 * @return true if successful, false if the length of the secret key exceeds HMAC_SHA256_MAX_KEY_SIZE
 */
bool
hmac_sha256_set_key_bin(const uint8_t* const p_key, const size_t key_size);

/**
 * @brief Set the secret key.
 * @param p_key - ptr to the string with the secret key
 * @return true if successful, false if the length of the secret key exceeds HMAC_SHA256_MAX_KEY_SIZE
 */
bool
hmac_sha256_set_key_str(const char* const p_key);

/**
 * @brief Compute HMAC_SHA256 for the message using the stored secret key and return the result as a binary buffer.
 * @param p_msg_buf - ptr to the buffer with a message
 * @param msg_len - length of the message
 * @param[out] p_hmac_sha256 - ptr to output binary buffer
 * @return true if successful, false - otherwise
 */
bool
hmac_sha256_calc(const uint8_t* const p_msg_buf, const size_t msg_len, hmac_sha256_t* const p_hmac_sha256);

/**
 * @brief Compute HMAC_SHA256 for the message using the stored secret key and return the result as a string.
 * @param p_msg - ptr to the message
 * @return 64-byte hex-encoded string hmac_sha256_str_t or empty string in case of error.
 */
hmac_sha256_str_t
hmac_sha256_calc_str(const char* const p_msg);

/**
 * @brief Check if hmac_sha256_str_t is valid
 * @param p_hmac_sha256_str - ptr to hmac_sha256_str_t
 * @return true if p_hmac_sha256_str contains hex-encoded HMAC_SHA256, false if p_hmac_sha256_str is empty.
 */
bool
hmac_sha256_is_str_valid(const hmac_sha256_str_t* const p_hmac_sha256_str);

#ifdef __cplusplus
}
#endif

#endif // RUUVI_GATEWAY_HMAC_SHA256_H
