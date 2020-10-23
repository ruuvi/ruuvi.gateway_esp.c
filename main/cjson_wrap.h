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

void
cjson_wrap_add_timestamp(cJSON *const object, const char *const name, const time_t timestamp);

#ifdef __cplusplus
}
#endif

#endif // RUUVI_CJSON_WRAP_H
