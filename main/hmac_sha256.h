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

#ifdef __cplusplus
extern "C" {
#endif

#define HMAC_SHA256_SIZE (32U)

typedef struct hmac_sha256_t
{
    uint8_t buf[HMAC_SHA256_SIZE];
} hmac_sha256_t;

typedef struct hmac_sha256_str_t
{
    char buf[HMAC_SHA256_SIZE * 2 + 1];
} hmac_sha256_str_t;

bool
hmac_sha256_set_key(const char *const p_key);

hmac_sha256_str_t
hmac_sha256_calc(const char *const p_msg);

#ifdef __cplusplus
}
#endif

#endif // RUUVI_GATEWAY_HMAC_SHA256_H
