/**
 * @file app_malloc.h
 * @author TheSomeMan
 * @date 2020-10-01
 * @copyright Ruuvi Innovations Ltd, license BSD-3-Clause.
 */

#include "app_malloc.h"
#include <stdlib.h>

void *
app_malloc(const size_t size)
{
    return malloc(size);
}

void
app_free(void *ptr)
{
    free(ptr);
}

void
app_free_pptr(void **p_ptr)
{
    free(*p_ptr);
    *p_ptr = NULL;
}

void
app_free_const_pptr(const void **p_ptr)
{
    free((void *)*p_ptr);
    *p_ptr = NULL;
}

void *
app_calloc(const size_t nmemb, const size_t size)
{
    return calloc(nmemb, size);
}
