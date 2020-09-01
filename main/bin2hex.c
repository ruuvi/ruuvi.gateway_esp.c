/**
 * @file bin2hex.c
 * @author TheSomeMan
 * @date 2020-08-27
 * @copyright Ruuvi Innovations Ltd, license BSD-3-Clause.
 */

#include "bin2hex.h"
#include "str_buf.h"

void
bin2hex(char *const p_hex_str, const size_t hex_str_size, const uint8_t *const p_bin_buf, const size_t bin_buf_len)
{
    str_buf_t str_buf = STR_BUF_INIT(p_hex_str, hex_str_size);

    for (size_t i = 0; i < bin_buf_len; i++)
    {
        if (str_buf_get_len(&str_buf) + 3 > hex_str_size)
        {
            break;
        }
        str_buf_printf(&str_buf, "%02X", p_bin_buf[i]);
    }
}
