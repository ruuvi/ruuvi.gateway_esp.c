/**
 * @file hmac_sha256.c
 * @author TheSomeMan
 * @date 2021-07-04
 * @copyright Ruuvi Innovations Ltd, license BSD-3-Clause.
 */

#include "hmac_sha256.h"
#include <string.h>
#include <stdio.h>
#include "mbedtls/md.h"

#define HMAC_SHA256_MAX_KEY_SIZE (64U)

static char g_hmac_sha256_key[HMAC_SHA256_MAX_KEY_SIZE];

bool
hmac_sha256_set_key(const char *const p_key)
{
    if (strlen(p_key) >= HMAC_SHA256_MAX_KEY_SIZE)
    {
        return false;
    }
    if (0 == strcmp(g_hmac_sha256_key, p_key))
    {
        return true;
    }
    strcpy(g_hmac_sha256_key, p_key);
    return true;
}

hmac_sha256_str_t
hmac_sha256_calc(const char *const p_msg)
{
    hmac_sha256_t     hmac     = { 0 };
    hmac_sha256_str_t hmac_str = { 0 };

    const mbedtls_md_info_t *p_md_info = mbedtls_md_info_from_type(MBEDTLS_MD_SHA256);
    if (0
        != mbedtls_md_hmac(
            p_md_info,
            (const unsigned char *)g_hmac_sha256_key,
            strlen(g_hmac_sha256_key),
            (const unsigned char *)p_msg,
            strlen(p_msg),
            &hmac.buf[0]))
    {
        hmac_str.buf[0] = '\0';
    }
    else
    {
        for (uint32_t i = 0; i < HMAC_SHA256_SIZE; ++i)
        {
            char *const  p_str   = &hmac_str.buf[i * 2];
            const size_t rem_len = (ptrdiff_t)(&hmac_str.buf[sizeof(hmac_str.buf) - 1] - p_str) + 1U;
            snprintf(p_str, rem_len, "%02x", hmac.buf[i]);
        }
    }
    return hmac_str;
}
