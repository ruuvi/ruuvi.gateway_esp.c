/**
 * @file esp_err_to_name.c
 * @author TheSomeMan
 * @date 2023-03-07
 * @copyright Ruuvi Innovations Ltd, license BSD-3-Clause.
 */

#include <esp_err.h>
#include "mbedtls/error.h"
#include "esp_tls.h"

const char *__wrap_esp_err_to_name_r(esp_err_t code, char *buf, size_t buflen)
{
    static const char* esp_unknown_msg = NULL;
    if (NULL == esp_unknown_msg)
    {
        const esp_err_t unknown_esp_err_code = 1;
        esp_unknown_msg = esp_err_to_name(unknown_esp_err_code);
    }
    const char* p_err_desc = esp_err_to_name(code);
    if (esp_unknown_msg != p_err_desc)
    {
        (void)snprintf(buf, buflen, "%s", p_err_desc);
        return buf;
    }

    if (strerror_r(code, buf, buflen) == 0) {
        return buf;
    }

    snprintf(buf, buflen, "%s 0x%x(%d)", esp_unknown_msg, code, code);

    return buf;
}
