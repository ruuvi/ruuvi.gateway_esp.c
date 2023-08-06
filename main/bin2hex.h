/**
 * @file bin2hex.h
 * @author TheSomeMan
 * @date 2020-08-27
 * @copyright Ruuvi Innovations Ltd, license BSD-3-Clause.
 */

#ifndef RUUVI_GATEWAY_ESP_BIN2HEX_H
#define RUUVI_GATEWAY_ESP_BIN2HEX_H

#include <stddef.h>
#include <stdint.h>
#include "str_buf.h"

#if !defined(RUUVI_TESTS_BIN2HEX)
#define RUUVI_TESTS_BIN2HEX 0
#endif

#if RUUVI_TESTS_BIN2HEX
#define BIN2HEX_STATIC
#else
#define BIN2HEX_STATIC static
#endif

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Allocate memory and print given binary buffer into hex string.
 *
 * Example {0xA0, 0xBB, 0x31} -> "A0BB31"
 *
 * @param[in]  p_bin_buf Pointer to the binary to print from.
 * @param[in]  bin_buf_len Size of the binary buffer in bytes.
 * @return pointer to the allocated string.
 */
char*
bin2hex_with_malloc(const uint8_t* const p_bin_buf, const size_t bin_buf_len);

#if RUUVI_TESTS_BIN2HEX

BIN2HEX_STATIC
void
bin2hex(str_buf_t* p_str_buf, const uint8_t* const p_bin_buf, const size_t bin_buf_len);

#endif // RUUVI_TESTS_BIN2HEX

#ifdef __cplusplus
}
#endif

#endif // RUUVI_GATEWAY_ESP_BIN2HEX_H
