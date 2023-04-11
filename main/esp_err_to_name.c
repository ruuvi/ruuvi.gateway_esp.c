/**
 * @file esp_err_to_name.c
 * @author TheSomeMan
 * @date 2023-03-07
 * @copyright Ruuvi Innovations Ltd, license BSD-3-Clause.
 */

#include "wrap_esp_err_to_name_r.h"

const char*
__wrap_esp_err_to_name_r(esp_err_t code, char* buf, size_t buflen)
{
    return wrap_esp_err_to_name_r(code, buf, buflen);
}
