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

typedef struct hmac_sha256_key_t
{
    uint8_t key[HMAC_SHA256_MAX_KEY_SIZE];
    size_t  key_size;
} hmac_sha256_key_t;

static hmac_sha256_key_t g_hmac_sha256_key_ruuvi;
static hmac_sha256_key_t g_hmac_sha256_key_custom;
static hmac_sha256_key_t g_hmac_sha256_key_stats;

static bool
hmac_sha256_set_key_bin(hmac_sha256_key_t* const p_key_storage, const uint8_t* const p_key, const size_t key_size)
{
    if (key_size >= HMAC_SHA256_MAX_KEY_SIZE)
    {
        return false;
    }
    if ((key_size == p_key_storage->key_size) && (0 == memcmp(p_key_storage->key, p_key, key_size)))
    {
        return true;
    }
    memcpy(p_key_storage->key, p_key, key_size);
    p_key_storage->key_size = key_size;
    return true;
}

bool
hmac_sha256_set_key_for_http_ruuvi(const char* const p_key)
{
    return hmac_sha256_set_key_bin(&g_hmac_sha256_key_ruuvi, (const uint8_t*)p_key, strlen(p_key));
}

bool
hmac_sha256_set_key_for_http_custom(const char* const p_key)
{
    return hmac_sha256_set_key_bin(&g_hmac_sha256_key_custom, (const uint8_t*)p_key, strlen(p_key));
}

bool
hmac_sha256_set_key_for_stats(const char* const p_key)
{
    return hmac_sha256_set_key_bin(&g_hmac_sha256_key_stats, (const uint8_t*)p_key, strlen(p_key));
}

static bool
hmac_sha256_calc(const char* const p_str, const hmac_sha256_key_t* const p_key, hmac_sha256_t* const p_hmac_sha256)
{
    const size_t             msg_len   = strlen(p_str);
    const mbedtls_md_info_t* p_md_info = mbedtls_md_info_from_type(MBEDTLS_MD_SHA256);
    if (0
        != mbedtls_md_hmac(
            p_md_info,
            (const unsigned char*)p_key->key,
            p_key->key_size,
            (const uint8_t*)p_str,
            msg_len,
            &p_hmac_sha256->buf[0]))
    {
        memset(p_hmac_sha256->buf, 0, sizeof(p_hmac_sha256->buf));
        return false;
    }
    return true;
}

bool
hmac_sha256_calc_for_http_ruuvi(const char* const p_str, hmac_sha256_t* const p_hmac_sha256)
{
    return hmac_sha256_calc(p_str, &g_hmac_sha256_key_ruuvi, p_hmac_sha256);
}

bool
hmac_sha256_calc_for_http_custom(const char* const p_str, hmac_sha256_t* const p_hmac_sha256)
{
    return hmac_sha256_calc(p_str, &g_hmac_sha256_key_custom, p_hmac_sha256);
}

bool
hmac_sha256_calc_for_stats(const char* const p_str, hmac_sha256_t* const p_hmac_sha256)
{
    return hmac_sha256_calc(p_str, &g_hmac_sha256_key_stats, p_hmac_sha256);
}

str_buf_t
hmac_sha256_to_str_buf(const hmac_sha256_t* const p_hmac_sha256)
{
    const size_t hmac_str_buf_size = HMAC_SHA256_SIZE * 2 + 1;
    char* const  p_hmac_str_buf    = os_malloc(hmac_str_buf_size);
    if (NULL == p_hmac_str_buf)
    {
        return str_buf_init_null();
    }
    str_buf_t hmac_str_buf = str_buf_init(p_hmac_str_buf, hmac_str_buf_size);
    for (uint32_t i = 0; i < HMAC_SHA256_SIZE; ++i)
    {
        str_buf_printf(&hmac_str_buf, "%02x", p_hmac_sha256->buf[i]);
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
