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
#include "os_malloc.h"

static uint8_t g_hmac_sha256_key[HMAC_SHA256_MAX_KEY_SIZE];
static size_t  g_hmac_sha256_key_size;

bool
hmac_sha256_set_key_bin(const uint8_t* const p_key, const size_t key_size)
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
hmac_sha256_set_key_str(const char* const p_key)
{
    return hmac_sha256_set_key_bin((const uint8_t*)p_key, strlen(p_key));
}

bool
hmac_sha256_calc(const uint8_t* const p_msg_buf, const size_t msg_len, hmac_sha256_t* const p_hmac_sha256)
{
    const mbedtls_md_info_t* p_md_info = mbedtls_md_info_from_type(MBEDTLS_MD_SHA256);
    if (0
        != mbedtls_md_hmac(
            p_md_info,
            (const unsigned char*)g_hmac_sha256_key,
            g_hmac_sha256_key_size,
            p_msg_buf,
            msg_len,
            &p_hmac_sha256->buf[0]))
    {
        return false;
    }
    return true;
}

str_buf_t
hmac_sha256_calc_str(const char* const p_msg)
{
    const size_t hmac_str_buf_size = HMAC_SHA256_SIZE * 2 + 1;
    char* const  p_hmac_str_buf    = os_malloc(hmac_str_buf_size);
    if (NULL == p_hmac_str_buf)
    {
        return str_buf_init_null();
    }
    str_buf_t     hmac_str_buf = str_buf_init(p_hmac_str_buf, hmac_str_buf_size);
    hmac_sha256_t hmac_sha256  = { 0 };
    const size_t  msg_len      = strlen(p_msg);
    if (!hmac_sha256_calc((const uint8_t*)p_msg, msg_len, &hmac_sha256))
    {
        str_buf_free_buf(&hmac_str_buf);
        return str_buf_init_null();
    }
    for (uint32_t i = 0; i < HMAC_SHA256_SIZE; ++i)
    {
        str_buf_printf(&hmac_str_buf, "%02x", hmac_sha256.buf[i]);
    }
    return hmac_str_buf;
}

bool
hmac_sha256_is_str_valid(const str_buf_t* const p_hmac_sha256_str)
{
    if (NULL == p_hmac_sha256_str->buf)
    {
        return false;
    }
    if ('\0' == p_hmac_sha256_str->buf[0])
    {
        return false;
    }
    return true;
}
