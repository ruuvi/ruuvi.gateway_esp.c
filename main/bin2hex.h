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

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Print given binary into hex string.
 *
 * Example {0xA0, 0xBB, 0x31} -> "A0BB31"
 *
 * @param[out] p_hex_str Pointer to string to print into. Must be at least 2 *
 * bin_buf_len + 1 bytes long.
 * @param[in]  hex_str_size Size of hex_str in bytes.
 * @param[in]  p_bin_buf Pointer to the binary to print from.
 * @param[in]  bin_buf_len Size of the binary buffer in bytes.
 */
void
bin2hex(char *const p_hex_str, const size_t hex_str_size, const uint8_t *const p_bin_buf, const size_t bin_buf_len);

#ifdef __cplusplus
}
#endif

#endif // RUUVI_GATEWAY_ESP_BIN2HEX_H
