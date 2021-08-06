/**
 * @file json_helper.c
 * @author TheSomeMan
 * @date 2021-07-31
 * @copyright Ruuvi Innovations Ltd, license BSD-3-Clause.
 */

#include "json_helper.h"
#include <stdbool.h>
#include <string.h>
#include <ctype.h>
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
            const char *const p_end = strpbrk(p_begin, "\\\"");
            if (NULL == p_end)
            {
                return false;
            }
            if ('"' == *p_end)
            {
                *p_p_end = p_end;
                break;
            }
            if ('\0' == p_end[1])
            {
                return false;
            }
            p_begin = &p_end[2];
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
        const size_t len = p_end - p_val;
        if ((0 == isdigit(*p_val)) && (0 != strncmp("null", p_val, len)) && (0 != strncmp("true", p_val, len))
            && (0 != strncmp("false", p_val, len)))
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
        p_token = strpbrk(p_token, "\\\"");
        if (NULL == p_token)
        {
            return str_buf_init_null();
        }
        if ('\\' == *p_token)
        {
            if ('\0' == p_token[1])
            {
                return str_buf_init_null();
            }
            p_token += 2;
            continue;
        }
        if ((0 == strncmp(&p_token[1], p_key, key_len)) && ('"' == p_token[1 + key_len]))
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
