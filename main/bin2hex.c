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

BIN2HEX_STATIC
void
bin2hex(str_buf_t *p_str_buf, const uint8_t *const p_bin_buf, const size_t bin_buf_len)
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

char *
bin2hex_with_malloc(const uint8_t *const p_bin_buf, const size_t bin_buf_len)
{
    str_buf_t str_buf = STR_BUF_INIT_NULL();
    bin2hex(&str_buf, p_bin_buf, bin_buf_len);
    const size_t str_buf_size = str_buf_get_len(&str_buf) + 1;
    char *       p_str_buf    = os_malloc(str_buf_size);
    if (NULL == p_str_buf)
    {
        return NULL;
    }
    str_buf = str_buf_init(p_str_buf, str_buf_size);
    bin2hex(&str_buf, p_bin_buf, bin_buf_len);
    return p_str_buf;
}
