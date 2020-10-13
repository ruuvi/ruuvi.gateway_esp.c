/**
 * @file nrf52fw.h
 * @author TheSomeMan
 * @date 2020-09-13
 * @copyright Ruuvi Innovations Ltd, license BSD-3-Clause.
 */

#ifndef RUUVI_GATEWAY_ESP_NRF52FW_H
#define RUUVI_GATEWAY_ESP_NRF52FW_H

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

#if !defined(RUUVI_TESTS_NRF52FW)
#define RUUVI_TESTS_NRF52FW (0)
#endif

#if RUUVI_TESTS_NRF52FW
#include <stdio.h>
#include "flashfatfs.h"
#endif

#define NRF52FW_ENABLE_FLASH_VERIFICATION 1

#ifdef __cplusplus
extern "C" {
#endif

typedef struct NRF52Fw_Version_Tag
{
    uint32_t version;
} NRF52Fw_Version_t;

typedef struct NRF52Fw_Segment_Tag
{
    uint32_t address;
    uint32_t size;
    char     file_name[20];
    uint32_t crc;
} NRF52Fw_Segment_t;

typedef struct NRF52Fw_Info_Tag
{
    NRF52Fw_Version_t fw_ver;
    uint32_t          num_segments;
    NRF52Fw_Segment_t segments[5];
} NRF52Fw_Info_t;

typedef struct NRF52Fw_TmpBuf_Tag
{
#define NRF52FW_TMP_BUF_SIZE (512U)
    uint32_t buf_wr[NRF52FW_TMP_BUF_SIZE / sizeof(uint32_t)];
    uint32_t buf_rd[NRF52FW_TMP_BUF_SIZE / sizeof(uint32_t)];
} NRF52Fw_TmpBuf_t;

bool
nrf52fw_update_fw_if_necessary(void);

#if RUUVI_TESTS_NRF52FW

#define STATIC

/**
 * @brief Parse one-byte digit (in range 0..255)
 * @param p_digit_str - ptr to a string
 * @param p_digit_str_end - ptr to the end position in the string or NULL
 * @param[out] p_digit - ptr to output value
 * @return true if successful
 */
STATIC bool
nrf52fw_parse_version_digit(const char *p_digit_str, const char *p_digit_str_end, uint8_t *p_digit);

/**
 * @brief Parse one-byte digit (in range 0..255) and update corresponding byte in version.
 * @param p_token_begin - ptr to string
 * @param p_token_end - ptr to the end of the digit in string or NULL
 * @param[out] p_version - ptr to output variable version
 * @param byte_num - byte number in version that needs to be updated
 * @return true if successful
 */
STATIC bool
nrf52fw_parse_digit_update_ver(
    const char *  p_token_begin,
    const char *  p_token_end,
    uint32_t *    p_version,
    const uint8_t byte_num);

/**
 * @brief Parse a version string (like "1.2.3")
 * @param p_version_str - ptr to a string
 * @param[out] p_version - ptr to the output variable @ref NRF52Fw_Version_t
 * @return true if successful
 */
STATIC bool
nrf52fw_parse_version(const char *p_version_str, NRF52Fw_Version_t *p_version);

/**
 * @brief Parse a version line string (like "# v1.2.3")
 * @param p_version_str - ptr to a string
 * @param[out] p_version - ptr to the output variable @ref NRF52Fw_Version_t
 * @return true if successful
 */
STATIC bool
nrf52fw_parse_version_line(const char *p_version_line, NRF52Fw_Version_t *p_version);

/**
 * @brief Remove CR, LF, Tab, Space from the right end of the string, force put EOL and end of the buffer with the
 * string.
 * @param p_line_buf - ptr to a line-buffer with a string
 * @param line_buf_size - size of the line-buffer
 */
STATIC void
nrf52fw_line_rstrip(char *p_line_buf, const size_t line_buf_size);

/**
 * @brief Parse string with segment info and fill @ref NRF52Fw_Segment_t
 * @param p_version_line - ptr to a string in format: "hex-addr size file-name crc" ("0x00001000 151016 segment_2.bin
 * 0x0e326e66")
 * @param[out] p_segment - ptr to @ref NRF52Fw_Segment_t
 * @return true if successful
 */
STATIC bool
nrf52fw_parse_segment_info_line(const char *p_version_line, NRF52Fw_Segment_t *p_segment);

/**
 * @brief Read opened file and fill @ref NRF52Fw_Info_t
 * @param p_fd - ptr to opened FILE
 * @param[out] p_info - ptr to @ref NRF52Fw_Info_t
 * @return 0 if success, line number if parsing failed
 */
STATIC int
nrf52fw_parse_info_file(FILE *p_fd, NRF52Fw_Info_t *p_info);

/**
 * @brief Open file "info.txt" on FlashFatFs, parse it and fill @ref NRF52Fw_Info_t
 * @param p_ffs - ptr to @ref FlashFatFs_t
 * @param p_path_info_txt - ptr to path to "into.txt"
 * @param[out] p_info - ptr to output variable @ref NRF52Fw_Info_t
 * @return true if successful
 */
STATIC bool
nrf52fw_read_info_txt(FlashFatFs_t *p_ffs, const char *p_path_info_txt, NRF52Fw_Info_t *p_info);

STATIC bool
nrf52fw_read_current_fw_ver(uint32_t *p_fw_ver);

STATIC bool
nrf52fw_write_current_fw_ver(const uint32_t fw_ver);

STATIC void
nrf52fw_simulate_file_read_error(const bool flag_error);

STATIC bool
nrf52fw_flash_write_segment(
    const int         fd,
    NRF52Fw_TmpBuf_t *p_tmp_buf,
    const uint32_t    segment_addr,
    const size_t      segment_len);

STATIC bool
nrf52fw_write_segment_from_file(
    FlashFatFs_t *    p_ffs,
    const char *      p_path,
    NRF52Fw_TmpBuf_t *p_tmp_buf,
    const uint32_t    segment_addr,
    const size_t      segment_len);

STATIC bool
nrf52fw_flash_write_firmware(FlashFatFs_t *p_ffs, NRF52Fw_TmpBuf_t *p_tmp_buf, const NRF52Fw_Info_t *p_fw_info);

STATIC bool
nrf52fw_calc_segment_crc(const int fd, NRF52Fw_TmpBuf_t *p_tmp_buf, const size_t segment_len, uint32_t *p_crc);

STATIC bool
nrf52fw_check_firmware(FlashFatFs_t *p_ffs, NRF52Fw_TmpBuf_t *p_tmp_buf, const NRF52Fw_Info_t *p_fw_info);

#endif // RUUVI_TESTS_NRF52FW

#ifdef __cplusplus
}
#endif

#endif // RUUVI_GATEWAY_ESP_NRF52FW_H
