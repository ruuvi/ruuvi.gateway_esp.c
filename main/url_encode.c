/**
 * @file url_encode.c
 * @author TheSomeMan
 * @date 2022-12-22
 * @copyright Ruuvi Innovations Ltd, license BSD-3-Clause.
 */

#include "url_encode.h"
#include <stdint.h>
#include <string.h>
#include "str_buf.h"
#include "os_str.h"
#include "esp_type_wrapper.h"

//#define LOG_LOCAL_LEVEL LOG_LEVEL_DEBUG
//#include "log.h"
//
// static const char TAG[] = "url_encode";

static const uint8_t g_rfc3986_table[256 / 8] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x60, 0xff, 0x03, 0xfe, 0xff, 0xff, 0x87, 0xfe, 0xff, 0xff, 0x47,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
};

static inline bool
rfc3986_is_in_range(const char val)
{
    const uint8_t uch     = (uint8_t)val;
    const uint8_t index   = (uint8_t)(uch / 8);
    const uint8_t bit_ofs = (uint8_t)(uch % 8);
    if (0 != (g_rfc3986_table[index] & (1U << bit_ofs)))
    {
        return true;
    }
    return false;
}

bool
url_n_encode_to_str_buf(const char* const p_src, const size_t len, str_buf_t* const p_dst)
{
    str_buf_printf(p_dst, "%s", "");
    for (const char* p_cur = p_src; ('\0' != *p_cur) && ((ptrdiff_t)(p_cur - p_src) < len); ++p_cur)
    {
        if (rfc3986_is_in_range(*p_cur))
        {
            str_buf_printf(p_dst, "%c", *p_cur);
        }
        else
        {
            const uint8_t uch = (uint8_t)*p_cur;
            str_buf_printf(p_dst, "%%%02X", (printf_int_t)uch);
        }
    }
    if (str_buf_is_overflow(p_dst))
    {
        return false;
    }
    return true;
}

bool
url_encode_to_str_buf(const char* const p_src, str_buf_t* const p_dst)
{
    return url_n_encode_to_str_buf(p_src, strlen(p_src), p_dst);
}

str_buf_t
url_n_encode_with_alloc(const char* const p_src, const size_t len)
{
    str_buf_t str_buf = STR_BUF_INIT_NULL();
    (void)url_n_encode_to_str_buf(p_src, len, &str_buf);
    if (!str_buf_init_with_alloc(&str_buf))
    {
        return str_buf_init_null();
    }
    (void)url_n_encode_to_str_buf(p_src, len, &str_buf);
    return str_buf;
}

str_buf_t
url_encode_with_alloc(const char* const p_src)
{
    return url_n_encode_with_alloc(p_src, strlen(p_src));
}

bool
url_n_decode_to_str_buf(const char* const p_src, const size_t len, str_buf_t* const p_dst)
{
    const os_str2num_base_t url_encoded_base = 16;
    str_buf_printf(p_dst, "%s", "");
    for (const char* p_cur = p_src; ('\0' != *p_cur) && ((ptrdiff_t)(p_cur - p_src) < len);)
    {
        if ('%' == *p_cur)
        {
            p_cur += 1;
            if ('\0' == p_cur[0])
            {
                return false;
            }
            if ('\0' == p_cur[1])
            {
                return false;
            }
            char        tmp_buf[3] = { p_cur[0], p_cur[1], '\0' };
            const char* p_end      = NULL;
            uint32_t    ch_val     = os_str_to_uint32_cptr(tmp_buf, &p_end, url_encoded_base);
            if ('\0' != *p_end)
            {
                return false;
            }
            str_buf_printf(p_dst, "%c", (char)ch_val);
            p_cur += 2;
        }
        else
        {
            str_buf_printf(p_dst, "%c", *p_cur);
            p_cur += 1;
        }
    }
    if (str_buf_is_overflow(p_dst))
    {
        return false;
    }
    return true;
}

bool
url_decode_to_str_buf(const char* const p_src, str_buf_t* const p_dst)
{
    return url_n_decode_to_str_buf(p_src, strlen(p_src), p_dst);
}

str_buf_t
url_n_decode_with_alloc(const char* const p_src, const size_t len)
{
    str_buf_t str_buf = STR_BUF_INIT_NULL();
    if (!url_n_decode_to_str_buf(p_src, len, &str_buf))
    {
        return str_buf_init_null();
    }
    if (!str_buf_init_with_alloc(&str_buf))
    {
        return str_buf_init_null();
    }
    if (!url_n_decode_to_str_buf(p_src, len, &str_buf))
    {
        return str_buf_init_null();
    }
    return str_buf;
}

str_buf_t
url_decode_with_alloc(const char* const p_src)
{
    return url_n_decode_with_alloc(p_src, strlen(p_src));
}
