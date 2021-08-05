/**
 * @file json_helper.c
 * @author TheSomeMan
 * @date 2021-07-31
 * @copyright Ruuvi Innovations Ltd, license BSD-3-Clause.
 */

#include "json_helper.h"
#include <stdbool.h>
#include <string.h>
#include "str_buf.h"
#include "esp_type_wrapper.h"

static bool
json_helper_find_token_start_and_end(const char *const p_val, const char **p_p_begin, const char **p_p_end)
{
    *p_p_end = NULL;
    if ('"' == *p_val)
    {
        const char *p_begin = p_val + 1;
        *p_p_begin          = p_begin;
        for (;;)
        {
            const char *const p_end = strchr(p_begin, '"');
            if (NULL == p_end)
            {
                return false;
            }
            const char *const p_char_before_quote = p_end - 1;
            if ('\\' != *p_char_before_quote)
            {
                *p_p_end = p_end;
                break;
            }
            p_begin = p_end + 1;
        }
    }
    else
    {
        *p_p_begin              = p_val;
        const char *const p_end = strpbrk(p_val, " \t,\n}");
        if (NULL == p_end)
        {
            return false;
        }
        *p_p_end = p_end;
    }
    return true;
}

str_buf_t
json_helper_get_by_key(const char *const p_json, const char *const p_key)
{
    const size_t key_len = strlen(p_key);
    const char * p_token = p_json;
    for (;;)
    {
        p_token = strchr(p_token, '\"');
        if (NULL == p_token)
        {
            return str_buf_init_null();
        }
        if ((0 == strncmp(p_token + 1, p_key, key_len)) && ('"' == p_token[1 + key_len]))
        {
            p_token += 1 + key_len + 1;
            break;
        }
        p_token += 1;
    }
    while (' ' == *p_token)
    {
        p_token += 1;
    }
    if (':' != *p_token)
    {
        return str_buf_init_null();
    }
    const char *p_val = p_token + 1;
    while (' ' == *p_val)
    {
        p_val += 1;
    }
    const char *p_begin = NULL;
    const char *p_end   = NULL;
    if (!json_helper_find_token_start_and_end(p_val, &p_begin, &p_end))
    {
        return str_buf_init_null();
    }
    return str_buf_printf_with_alloc("%.*s", (printf_int_t)(p_end - p_begin), p_begin);
}
