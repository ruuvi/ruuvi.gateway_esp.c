/**
 * @file cjson_wrap.h
 * @author TheSomeMan
 * @date 2020-10-23
 * @copyright Ruuvi Innovations Ltd, license BSD-3-Clause.
 */

#ifndef RUUVI_CJSON_WRAP_H
#define RUUVI_CJSON_WRAP_H

#include <time.h>
#include "cJSON.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct cjson_wrap_str_t
{
    const char *p_str;
} cjson_wrap_str_t;

void
cjson_wrap_add_timestamp(cJSON *const p_object, const char *const p_name, const time_t timestamp);

cjson_wrap_str_t
cjson_wrap_print(const cJSON *p_item);

cjson_wrap_str_t
cjson_wrap_print_and_delete(cJSON **pp_object);

void
cjson_wrap_free_json_str(cjson_wrap_str_t *p_json_str);

#ifdef __cplusplus
}
#endif

#endif // RUUVI_CJSON_WRAP_H
