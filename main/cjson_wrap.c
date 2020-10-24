/**
 * @file cjson_wrap.c
 * @author TheSomeMan
 * @date 2020-10-23
 * @copyright Ruuvi Innovations Ltd, license BSD-3-Clause.
 */

#include "cjson_wrap.h"
#include <stdio.h>

void
cjson_wrap_add_timestamp(cJSON *const object, const char *const name, const time_t timestamp)
{
    char timestamp_str[32];
    snprintf(timestamp_str, sizeof(timestamp_str), "%ld", (long)timestamp);
    cJSON_AddStringToObject(object, name, timestamp_str);
}
