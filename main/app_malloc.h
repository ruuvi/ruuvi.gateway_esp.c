/**
 * @file app_malloc.h
 * @author TheSomeMan
 * @date 2020-10-01
 * @copyright Ruuvi Innovations Ltd, license BSD-3-Clause.
 */

#ifndef RUUVI_GATEWAY_ESP_APP_MALLOC_H
#define RUUVI_GATEWAY_ESP_APP_MALLOC_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

void *
app_malloc(const size_t size);

void
app_free(void *ptr);

void
app_free_pptr(void **p_ptr);

void
app_free_const_pptr(const void **p_ptr);

void *
app_calloc(const size_t nmemb, const size_t size);

#ifdef __cplusplus
}
#endif

#endif // RUUVI_GATEWAY_ESP_APP_MALLOC_H
