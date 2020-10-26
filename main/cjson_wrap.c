/**
 * @file cjson_wrap.c
 * @author TheSomeMan
 * @date 2020-10-23
 * @copyright Ruuvi Innovations Ltd, license BSD-3-Clause.
 */

#include "cjson_wrap.h"
#include <stdio.h>
#include <stdlib.h>

void
cjson_wrap_add_timestamp(cJSON *const p_object, const char *const p_name, const time_t timestamp)
{
    char timestamp_str[32];
    snprintf(timestamp_str, sizeof(timestamp_str), "%ld", timestamp);
    cJSON_AddStringToObject(p_object, p_name, timestamp_str);
}

cjson_wrap_str_t
cjson_wrap_print(const cJSON *p_object)
{
    const cjson_wrap_str_t json_str = {
        .p_str = cJSON_Print(p_object),
    };
    return json_str;
}

cjson_wrap_str_t
cjson_wrap_print_and_delete(cJSON **pp_object)
{
    const cjson_wrap_str_t json_str = {
        .p_str = cJSON_Print(*pp_object),
    };
    cJSON_Delete(*pp_object);
    *pp_object = NULL;
    return json_str;
}

void
cjson_wrap_free_json_str(cjson_wrap_str_t *p_json_str)
{
    free((void *)p_json_str->p_str);
    p_json_str->p_str = NULL;
}
