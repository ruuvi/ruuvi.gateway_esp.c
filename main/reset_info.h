/**
 * @file reset_info.h
 * @author TheSomeMan
 * @date 2022-11-18
 * @copyright Ruuvi Innovations Ltd, license BSD-3-Clause.
 */

#ifndef RUUVI_GATEWAY_ESP_RESET_INFO_H
#define RUUVI_GATEWAY_ESP_RESET_INFO_H

#include <stdint.h>
#include "str_buf.h"

#ifdef __cplusplus
extern "C" {
#endif

void
reset_info_init(void);

void
reset_info_set_sw(const char* const p_msg);

str_buf_t
reset_info_get(void);

uint32_t
reset_info_get_cnt(void);

void
reset_info_clear_extra_info(void);

#ifdef __cplusplus
}
#endif

#endif // RUUVI_GATEWAY_ESP_RESET_INFO_H
