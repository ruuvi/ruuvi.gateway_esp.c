/**
 * @file url_encode.h
 * @author TheSomeMan
 * @date 2022-12-22
 * @copyright Ruuvi Innovations Ltd, license BSD-3-Clause.
 */

#ifndef RUUVI_GATEWAY_ESP_URL_ENCODE_H
#define RUUVI_GATEWAY_ESP_URL_ENCODE_H

#include <stdbool.h>
#include "str_buf.h"

#ifdef __cplusplus
extern "C" {
#endif

bool
url_encode_to_str_buf(const char* const p_src, str_buf_t* const p_dst);

str_buf_t
url_encode_with_alloc(const char* const p_src);

bool
url_decode_to_str_buf(const char* p_src, str_buf_t* const p_dst);

str_buf_t
url_decode_with_alloc(const char* p_src);

#ifdef __cplusplus
}
#endif

#endif // RUUVI_GATEWAY_ESP_URL_ENCODE_H
