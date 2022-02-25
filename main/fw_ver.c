/**
 * @file fw_ver.c
 * @author TheSomeMan
 * @date 2022-01-14
 * @copyright Ruuvi Innovations Ltd, license BSD-3-Clause.
 */

#include "fw_ver.h"
#include <stdint.h>
#include "esp_app_format.h"

_Static_assert(sizeof(fw_ver_str_t) == sizeof(((esp_app_desc_t *)0)->version), "sizeof fw_ver_str_t");
