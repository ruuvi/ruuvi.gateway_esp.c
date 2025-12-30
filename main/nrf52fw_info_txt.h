/**
 * @copyright Ruuvi Innovations Ltd, license BSD-3-Clause.
 */

#ifndef RUUVI_GATEWAY_ESP_NRF52FW_INFO_TXT_H
#define RUUVI_GATEWAY_ESP_NRF52FW_INFO_TXT_H

#include <stdbool.h>
#include "flashfatfs.h"
#include "nrf52_fw_ver.h"

#ifdef __cplusplus
extern "C" {
#endif

#if !defined(RUUVI_TESTS_NRF52FW)
#define RUUVI_TESTS_NRF52FW (0)
#endif

#if RUUVI_TESTS_NRF52FW
#define NRF52FW_STATIC
#else
#define NRF52FW_STATIC static
#endif

typedef struct nrf52fw_segment_t
{
    uint32_t address;
    uint32_t size;
    char     file_name[20];
    uint32_t crc;
} nrf52fw_segment_t;

typedef struct nrf52fw_info_t
{
    ruuvi_nrf52_fw_ver_t fw_ver;
    uint32_t             num_segments;
    nrf52fw_segment_t    segments[5];
} nrf52fw_info_t;

bool
nrf52fw_info_txt_read(const flash_fat_fs_t* p_ffs, const char* const p_path_info_txt, nrf52fw_info_t* const p_info);

#if RUUVI_TESTS_NRF52FW

/**
 * @brief Parse one-byte digit (in range 0..255)
 * @param p_digit_str - ptr to a string
 * @param p_digit_str_end - ptr to the end position in the string or NULL
 * @param[out] p_digit - ptr to output value
 * @return true if successful
 */
NRF52FW_STATIC
bool
nrf52fw_parse_version_digit(const char* p_digit_str, const char* p_digit_str_end, uint8_t* p_digit);

/**
 * @brief Parse one-byte digit (in range 0..255) and update corresponding byte in version.
 * @param p_token_begin - ptr to string
 * @param p_token_end - ptr to the end of the digit in string or NULL
 * @param[out] p_version - ptr to output variable version
 * @param byte_num - byte number in version that needs to be updated
 * @return true if successful
 */
NRF52FW_STATIC
bool
nrf52fw_parse_digit_update_ver(
    const char*   p_token_begin,
    const char*   p_token_end,
    uint32_t*     p_version,
    const uint8_t byte_num);

/**
 * @brief Parse a version string (like "1.2.3")
 * @param p_version_str - ptr to a string
 * @param[out] p_version - ptr to the output variable @ref ruuvi_nrf52_fw_ver_t
 * @return true if successful
 */
NRF52FW_STATIC
bool
nrf52fw_parse_version(const char* p_version_str, ruuvi_nrf52_fw_ver_t* p_version);

/**
 * @brief Parse a version line string (like "# v1.2.3")
 * @param p_version_str - ptr to a string
 * @param[out] p_version - ptr to the output variable @ref ruuvi_nrf52_fw_ver_t
 * @return true if successful
 */
NRF52FW_STATIC
bool
nrf52fw_parse_version_line(const char* p_version_line, ruuvi_nrf52_fw_ver_t* p_version);

/**
 * @brief Remove CR, LF, Tab, Space from the right end of the string, force put EOL and end of the buffer with the
 * string.
 * @param p_line_buf - ptr to a line-buffer with a string
 * @param line_buf_size - size of the line-buffer
 */
NRF52FW_STATIC
void
nrf52fw_line_rstrip(char* p_line_buf, const size_t line_buf_size);

/**
 * @brief Parse string with segment info and fill @ref nrf52fw_segment_t
 * @param p_version_line - ptr to a string in format: "hex-addr size file-name crc" ("0x00001000 151016 segment_2.bin
 * 0x0e326e66")
 * @param[out] p_segment - ptr to @ref nrf52fw_segment_t
 * @return true if successful
 */
NRF52FW_STATIC
bool
nrf52fw_parse_segment_info_line(const char* p_version_line, nrf52fw_segment_t* p_segment);

/**
 * @brief Read opened file and fill @ref nrf52fw_info_t
 * @param p_fd - ptr to opened FILE
 * @param[out] p_info - ptr to @ref nrf52fw_info_t
 * @return 0 if success, line number if parsing failed
 */
NRF52FW_STATIC
int
nrf52fw_parse_info_file(FILE* p_fd, nrf52fw_info_t* p_info);

/**
 * @brief Open file "info.txt" on FlashFatFs, parse it and fill @ref nrf52fw_info_t
 * @param p_ffs - ptr to @ref flash_fat_fs_t
 * @param p_path_info_txt - ptr to path to "into.txt"
 * @param[out] p_info - ptr to output variable @ref nrf52fw_info_t
 * @return true if successful
 */
NRF52FW_STATIC
bool
nrf52fw_info_txt_read(const flash_fat_fs_t* p_ffs, const char* p_path_info_txt, nrf52fw_info_t* p_info);

#endif // RUUVI_TESTS_NRF52FW

#ifdef __cplusplus
}
#endif

#endif // RUUVI_GATEWAY_ESP_NRF52FW_INFO_TXT_H
