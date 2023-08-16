/**
 * @file cjson_wrap.c
 * @author TheSomeMan
 * @date 2020-10-23
 * @copyright Ruuvi Innovations Ltd, license BSD-3-Clause.
 */

#include "cjson_wrap.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "os_malloc.h"
#include "esp_type_wrapper.h"

ATTR_MALLOC
ATTR_MALLOC_SIZE(1)
static void*
cjson_wrap_malloc(const size_t size)
{
    return os_malloc(size);
}

static void
cjson_wrap_free(void* ptr)
{
    os_free(ptr);
}

void
cjson_wrap_init(void)
{
    cJSON_Hooks hooks = {
        .malloc_fn = &cjson_wrap_malloc,
        .free_fn   = &cjson_wrap_free,
    };
    cJSON_InitHooks(&hooks);
}

bool
cjson_wrap_add_timestamp(cJSON* const p_object, const char* const p_name, const time_t timestamp)
{
    char timestamp_str[32];
    snprintf(timestamp_str, sizeof(timestamp_str), "%ld", timestamp);
    if (NULL == cJSON_AddStringToObject(p_object, p_name, timestamp_str))
    {
        return false;
    }
    return true;
}

bool
cjson_wrap_add_uint32(cJSON* const p_object, const char* const p_name, const uint32_t val)
{
    char val_str[32];
    snprintf(val_str, sizeof(val_str), "%lu", (printf_ulong_t)val);
    if (NULL == cJSON_AddStringToObject(p_object, p_name, val_str))
    {
        return false;
    }
    return true;
}

cjson_wrap_str_t
cjson_wrap_print(const cJSON* p_object)
{
    const cjson_wrap_str_t json_str = {
        .p_str = cJSON_Print(p_object),
    };
    return json_str;
}

void
cjson_wrap_delete(cJSON** pp_object)
{
    cJSON_Delete(*pp_object);
    *pp_object = NULL;
}

cjson_wrap_str_t
cjson_wrap_print_and_delete(cJSON** pp_object)
{
    const cjson_wrap_str_t json_str = {
        .p_str = cJSON_Print(*pp_object),
    };
    cJSON_Delete(*pp_object);
    *pp_object = NULL;
    return json_str;
}

void
cjson_wrap_free_json_str(cjson_wrap_str_t* p_json_str)
{
    if (NULL != p_json_str->p_mem)
    {
        cJSON_free(p_json_str->p_mem);
    }
    p_json_str->p_str = NULL;
}

bool
json_wrap_copy_string_val(const cJSON* p_json_root, const char* p_attr_name, char* buf, const size_t buf_len)
{
    cJSON* const p_json_attr = cJSON_GetObjectItem(p_json_root, p_attr_name);
    if (NULL == p_json_attr)
    {
        return false;
    }
    const char* p_str = cJSON_GetStringValue(p_json_attr);
    if (NULL == p_str)
    {
        return false;
    }
    snprintf(buf, buf_len, "%s", p_str);
    return true;
}

const char*
json_wrap_get_string_val(const cJSON* p_json_root, const char* p_attr_name)
{
    cJSON* const p_json_attr = cJSON_GetObjectItem(p_json_root, p_attr_name);
    if (NULL == p_json_attr)
    {
        return NULL;
    }
    return cJSON_GetStringValue(p_json_attr);
}

bool
json_wrap_get_bool_val(const cJSON* p_json_root, const char* p_attr_name, bool* p_val)
{
    const cJSON* p_json_attr = cJSON_GetObjectItem(p_json_root, p_attr_name);
    if (NULL == p_json_attr)
    {
        return false;
    }
    if (!(bool)cJSON_IsBool(p_json_attr))
    {
        return false;
    }
    *p_val = cJSON_IsTrue(p_json_attr);
    return true;
}

bool
json_wrap_get_uint16_val(const cJSON* p_json_root, const char* p_attr_name, uint16_t* p_val)
{
    const cJSON* p_json_attr = cJSON_GetObjectItem(p_json_root, p_attr_name);
    if (NULL == p_json_attr)
    {
        return false;
    }
    if ((bool)cJSON_IsNumber(p_json_attr))
    {
        if (!((p_json_attr->valueint >= 0) && (p_json_attr->valueint <= UINT16_MAX)))
        {
            return false;
        }
        *p_val = (uint16_t)p_json_attr->valueint;
    }
    else
    {
        const char* const p_prefix_hex   = "0x";
        const size_t      prefix_hex_len = strlen(p_prefix_hex);
        if (0 != strncmp(p_json_attr->valuestring, p_prefix_hex, prefix_hex_len))
        {
            return false;
        }
        const uint32_t val = strtoul(p_json_attr->valuestring, NULL, 16);
        if (!(val <= UINT16_MAX))
        {
            return false;
        }
        *p_val = (uint16_t)val;
    }
    return true;
}

bool
json_wrap_get_uint8_val(const cJSON* p_json_root, const char* p_attr_name, uint8_t* p_val)
{
    const cJSON* p_json_attr = cJSON_GetObjectItem(p_json_root, p_attr_name);
    if (NULL == p_json_attr)
    {
        return false;
    }
    if ((bool)cJSON_IsNumber(p_json_attr))
    {
        if (!((p_json_attr->valueint >= 0) && (p_json_attr->valueint <= UINT8_MAX)))
        {
            return false;
        }
        *p_val = (uint8_t)p_json_attr->valueint;
    }
    else
    {
        const char* const p_prefix_hex   = "0x";
        const size_t      prefix_hex_len = strlen(p_prefix_hex);
        if (0 != strncmp(p_json_attr->valuestring, p_prefix_hex, prefix_hex_len))
        {
            return false;
        }
        const uint32_t val = strtoul(p_json_attr->valuestring, NULL, 16);
        if (!(val <= UINT8_MAX))
        {
            return false;
        }
        *p_val = (uint8_t)val;
    }
    return true;
}

bool
json_wrap_get_int8_val(const cJSON* p_json_root, const char* p_attr_name, int8_t* p_val)
{
    const cJSON* p_json_attr = cJSON_GetObjectItem(p_json_root, p_attr_name);
    if (NULL == p_json_attr)
    {
        return false;
    }
    if ((bool)cJSON_IsNumber(p_json_attr))
    {
        if (p_json_attr->valueint < INT8_MIN)
        {
            return false;
        }
        if (p_json_attr->valueint > INT8_MAX)
        {
            return false;
        }
        *p_val = (int8_t)p_json_attr->valueint;
    }
    else
    {
        const char* const p_prefix_hex   = "0x";
        const size_t      prefix_hex_len = strlen(p_prefix_hex);
        if (0 != strncmp(p_json_attr->valuestring, p_prefix_hex, prefix_hex_len))
        {
            return false;
        }
        const int32_t val = strtol(p_json_attr->valuestring, NULL, 16);
        if (!((val >= 0) && (val <= INT8_MAX)))
        {
            return false;
        }
        *p_val = (int8_t)val;
    }
    return true;
}
