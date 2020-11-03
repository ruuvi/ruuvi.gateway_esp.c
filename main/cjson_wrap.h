/**
 * @file cjson_wrap.h
 * @author TheSomeMan
 * @date 2020-10-23
 * @copyright Ruuvi Innovations Ltd, license BSD-3-Clause.
 */

#ifndef RUUVI_CJSON_WRAP_H
#define RUUVI_CJSON_WRAP_H

#include <stdbool.h>
#include <stdint.h>
#include <time.h>
#include "cJSON.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef double cjson_number_t;

typedef struct cjson_wrap_str_t
{
    const char *p_str;
} cjson_wrap_str_t;

static inline cjson_wrap_str_t
cjson_wrap_str_null(void)
{
    const cjson_wrap_str_t json_str = {
        .p_str = NULL,
    };
    return json_str;
}

void
cjson_wrap_add_timestamp(cJSON *const p_object, const char *const p_name, const time_t timestamp);

cjson_wrap_str_t
cjson_wrap_print(const cJSON *p_item);

void
cjson_wrap_delete(cJSON **pp_object);

cjson_wrap_str_t
cjson_wrap_print_and_delete(cJSON **pp_object);

void
cjson_wrap_free_json_str(cjson_wrap_str_t *p_json_str);

bool
json_wrap_copy_string_val(const cJSON *p_json_root, const char *p_attr_name, char *buf, const size_t buf_len);

bool
json_wrap_get_bool_val(const cJSON *p_json_root, const char *p_attr_name, bool *p_val);

bool
json_wrap_get_uint16_val(const cJSON *p_json_root, const char *p_attr_name, uint16_t *p_val);

#ifdef __cplusplus
}
#endif

#endif // RUUVI_CJSON_WRAP_H
