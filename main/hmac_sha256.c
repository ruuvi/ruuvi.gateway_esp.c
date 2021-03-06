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

static uint8_t g_hmac_sha256_key[HMAC_SHA256_MAX_KEY_SIZE];
static size_t  g_hmac_sha256_key_size;

bool
hmac_sha256_set_key_bin(const uint8_t *const p_key, const size_t key_size)
{
    if (key_size >= HMAC_SHA256_MAX_KEY_SIZE)
    {
        return false;
    }
    if ((key_size == g_hmac_sha256_key_size) && (0 == memcmp(g_hmac_sha256_key, p_key, key_size)))
    {
        return true;
    }
    memcpy(g_hmac_sha256_key, p_key, key_size);
    g_hmac_sha256_key_size = key_size;
    return true;
}

bool
hmac_sha256_set_key_str(const char *const p_key)
{
    return hmac_sha256_set_key_bin((const uint8_t *)p_key, strlen(p_key));
}

bool
hmac_sha256_calc(const uint8_t *const p_msg_buf, const size_t msg_len, hmac_sha256_t *const p_hmac_sha256)
{
    const mbedtls_md_info_t *p_md_info = mbedtls_md_info_from_type(MBEDTLS_MD_SHA256);
    if (0
        != mbedtls_md_hmac(
            p_md_info,
            (const unsigned char *)g_hmac_sha256_key,
            g_hmac_sha256_key_size,
            (const unsigned char *)p_msg_buf,
            msg_len,
            &p_hmac_sha256->buf[0]))
    {
        return false;
    }
    return true;
}

hmac_sha256_str_t
hmac_sha256_calc_str(const char *const p_msg)
{
    hmac_sha256_str_t hmac_str    = { 0 };
    hmac_sha256_t     hmac_sha256 = { 0 };
    const size_t      msg_len     = strlen(p_msg);
    if (!hmac_sha256_calc((const uint8_t *)p_msg, msg_len, &hmac_sha256))
    {
        return hmac_str; // empty string
    }
    for (uint32_t i = 0; i < HMAC_SHA256_SIZE; ++i)
    {
        char *const  p_str   = &hmac_str.buf[i * 2];
        const size_t rem_len = (ptrdiff_t)(&hmac_str.buf[sizeof(hmac_str.buf) - 1] - p_str) + 1U;
        snprintf(p_str, rem_len, "%02x", hmac_sha256.buf[i]);
    }
    return hmac_str;
}

bool
hmac_sha256_is_str_valid(const hmac_sha256_str_t *const p_hmac_sha256_str)
{
    if ('\0' == p_hmac_sha256_str->buf[0])
    {
        return false;
    }
    return true;
}
