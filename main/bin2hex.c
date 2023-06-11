/**
 * @file bin2hex.c
 * @author TheSomeMan
 * @date 2020-08-27
 * @copyright Ruuvi Innovations Ltd, license BSD-3-Clause.
 */

#include "bin2hex.h"
#include <string.h>
#include <stdlib.h>
#include "str_buf.h"
#include "os_malloc.h"

#define BIN2HEX_BASE_16 (16U)

BIN2HEX_STATIC
void
bin2hex(str_buf_t* p_str_buf, const uint8_t* const p_bin_buf, const size_t bin_buf_len)
{
    const size_t len_of_hex_digit_with_separator = 3;
    str_buf_printf(p_str_buf, "%s", "");
    for (size_t i = 0; i < bin_buf_len; ++i)
    {
        if ((0 != p_str_buf->size)
            && ((str_buf_get_len(p_str_buf) + len_of_hex_digit_with_separator) > p_str_buf->size))
        {
            break;
        }
        str_buf_printf(p_str_buf, "%02X", p_bin_buf[i]);
    }
}

char*
bin2hex_with_malloc(const uint8_t* const p_bin_buf, const size_t bin_buf_len)
{
    str_buf_t str_buf = STR_BUF_INIT_NULL();
    bin2hex(&str_buf, p_bin_buf, bin_buf_len);
    const size_t str_buf_size = str_buf_get_len(&str_buf) + 1;
    char*        p_str_buf    = os_malloc(str_buf_size);
    if (NULL == p_str_buf)
    {
        return NULL;
    }
    str_buf = str_buf_init(p_str_buf, str_buf_size);
    bin2hex(&str_buf, p_bin_buf, bin_buf_len);
    return p_str_buf;
}

uint8_t*
hex2bin_with_malloc(const char* const p_hex_str, size_t* const p_length)
{
    const size_t hex_str_len = strlen(p_hex_str);
    if (0 != (hex_str_len % 2))
    {
        *p_length = 0;
        return NULL;
    }
    const size_t bin_len = hex_str_len / 2;
    *p_length            = bin_len;
    uint8_t* p_buf       = os_malloc(bin_len);
    if (NULL == p_buf)
    {
        return NULL;
    }

    for (size_t i = 0, j = 0; i < hex_str_len; i += 2, ++j)
    {
        char byte_str_buf[3] = { p_hex_str[i], p_hex_str[i + 1], '\0' };

        char* end = NULL;
        p_buf[j]  = (uint8_t)strtol(byte_str_buf, &end, BIN2HEX_BASE_16);
        if ('\0' != *end)
        {
            os_free(p_buf);
            return NULL;
        }
    }

    return p_buf;
}