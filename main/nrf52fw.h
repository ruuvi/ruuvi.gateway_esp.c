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
#include "nrf52_fw_ver.h"
#include "nrf52fw_info_txt.h"

#if !defined(RUUVI_TESTS_NRF52FW)
#define RUUVI_TESTS_NRF52FW (0)
#endif

#if RUUVI_TESTS_NRF52FW
#include <stdio.h>
#include "flashfatfs.h"
#endif

#if RUUVI_TESTS_NRF52FW
#define NRF52FW_STATIC
#else
#define NRF52FW_STATIC static
#endif

#define NRF52FW_ENABLE_FLASH_VERIFICATION 1

#ifdef __cplusplus
extern "C" {
#endif

#define NRF52FW_FICR_BASE_ADDR (0x10000000U)
#define NRF52FW_UICR_BASE_ADDR (NRF52FW_FICR_BASE_ADDR + 0x1000U)
#define NRF52FW_UICR_FW_VER    (NRF52FW_UICR_BASE_ADDR + 0x080U)
#define NRF52FW_FICR_INFO_PART (NRF52FW_FICR_BASE_ADDR + 0x100U)

typedef struct nrf52fw_tmp_buf_t
{
#define NRF52FW_TMP_BUF_SIZE (256U)
    uint32_t buf_wr[NRF52FW_TMP_BUF_SIZE / sizeof(uint32_t)];
    uint32_t buf_rd[NRF52FW_TMP_BUF_SIZE / sizeof(uint32_t)];
} nrf52fw_tmp_buf_t;

typedef void (*nrf52fw_cb_progress)(const size_t num_bytes_flashed, const size_t total_size, void* const p_param);

typedef void (*nrf52fw_cb_before_updating)(void);
typedef void (*nrf52fw_cb_after_updating)(const bool flag_success);

typedef struct nrf52fw_progress_info_t
{
    size_t              accum_num_bytes_flashed;
    const size_t        total_size;
    nrf52fw_cb_progress cb_progress;
    void* const         p_param_cb_progress;
} nrf52fw_progress_info_t;

void
nrf52fw_set_manual_reset_mode(const bool flag_manual_reset_mode);

bool
nrf52fw_get_manual_reset_mode(void);

void
nrf52fw_hw_reset_nrf52(const bool flag_reset);

bool
nrf52fw_update_fw_if_necessary(
    const char* const           p_fatfs_nrf52_partition_name,
    nrf52fw_cb_progress         cb_progress,
    void* const                 p_param_cb_progress,
    nrf52fw_cb_before_updating  cb_before_updating,
    nrf52fw_cb_after_updating   cb_after_updating,
    ruuvi_nrf52_fw_ver_t* const p_nrf52_fw_ver,
    const bool                  flag_run_fw_after_update);

bool
nrf52fw_software_reset(void);

#if RUUVI_TESTS_NRF52FW

/**
 * @brief Read current firmware version from nRF52
 * @param[out] p_fw_ver - ptr to output variable
 * @return true if successful
 */
NRF52FW_STATIC
bool
nrf52fw_read_current_fw_ver(ruuvi_nrf52_fw_ver_t* const p_fw_ver);

/**
 * @brief Write current firmware version to nRF52
 * @param device_info - firmware version
 * @return true if successful
 */
NRF52FW_STATIC
bool
nrf52fw_write_current_fw_ver(const uint32_t device_info);

/**
 * @brief Set error generation mode on subsequent calls to nrf52fw_file_read
 * @param flag_error - enable error generation mode if true
 */
NRF52FW_STATIC
void
nrf52fw_simulate_file_read_error(const bool flag_error);

/**
 * @brief Write a segment of flash memory to nRF52 from an opened file
 * @param fd - descriptor of an opened file, @ref file_descriptor_t
 * @param p_tmp_buf - ptr to temporary buffer, @ref nrf52fw_tmp_buf_t
 * @param segment_addr - address of segment
 * @param segment_len - segment length
 * @param p_progress_info - ptr to nrf52fw_progress_info_t
 * @return true if successful
 */
NRF52FW_STATIC
bool
nrf52fw_flash_write_segment(
    const file_descriptor_t        fd,
    nrf52fw_tmp_buf_t*             p_tmp_buf,
    const uint32_t                 segment_addr,
    const size_t                   segment_len,
    nrf52fw_progress_info_t* const p_progress_info);

/**
 * @brief Write a segment of flash memory to nRF52 from file
 * @param p_ffs - ptr to FlashFatFs descriptor, @ref flash_fat_fs_t
 * @param p_path - ptr to a string with file path
 * @param p_tmp_buf - ptr to temporary buffer, @ref nrf52fw_tmp_buf_t
 * @param segment_addr - address of segment
 * @param segment_len - segment length
 * @param p_progress_info - ptr to nrf52fw_progress_info_t
 * @return true if successful
 */
NRF52FW_STATIC
bool
nrf52fw_write_segment_from_file(
    const flash_fat_fs_t*          p_ffs,
    const char*                    p_path,
    nrf52fw_tmp_buf_t*             p_tmp_buf,
    const uint32_t                 segment_addr,
    const size_t                   segment_len,
    nrf52fw_progress_info_t* const p_progress_info);

/**
 * @brief Write firmware segments of flash memory to nRF52 from files
 * @param p_ffs - ptr to FlashFatFs descriptor, @ref flash_fat_fs_t
 * @param p_tmp_buf - ptr to temporary buffer, @ref nrf52fw_tmp_buf_t
 * @param p_fw_info - ptr to firmware segments description info, @ref nrf52fw_info_t
 * @param cb_progress - callback function to track progress
 * @param p_param_cb_progress - ptr to parameter for cb_progress
 * @return true if successful
 */
NRF52FW_STATIC
bool
nrf52fw_flash_write_firmware(
    const flash_fat_fs_t* p_ffs,
    nrf52fw_tmp_buf_t*    p_tmp_buf,
    const nrf52fw_info_t* p_fw_info,
    nrf52fw_cb_progress   cb_progress,
    void* const           p_param_cb_progress);

/**
 * @brief Read file and calculate CRC for firmware segment
 * @param fd - descriptor of an opened file, @ref file_descriptor_t
 * @param p_tmp_buf - ptr to temporary buffer, @ref nrf52fw_tmp_buf_t
 * @param segment_len - length of segment
 * @param[out] p_crc - ptr to output variable with CRC
 * @return true if successful
 */
NRF52FW_STATIC
bool
nrf52fw_calc_segment_crc(
    const file_descriptor_t fd,
    nrf52fw_tmp_buf_t*      p_tmp_buf,
    const size_t            segment_len,
    uint32_t*               p_crc);

/**
 * @brief Check CRC for all firmware segments
 * @param p_ffs - ptr to FlashFatFs descriptor, @ref flash_fat_fs_t
 * @param p_tmp_buf - ptr to temporary buffer, @ref nrf52fw_tmp_buf_t
 * @param p_fw_info - ptr to firmware segments description info, @ref nrf52fw_info_t
 * @return true if successful
 */
NRF52FW_STATIC
bool
nrf52fw_check_firmware(const flash_fat_fs_t* p_ffs, nrf52fw_tmp_buf_t* p_tmp_buf, const nrf52fw_info_t* p_fw_info);

#endif // RUUVI_TESTS_NRF52FW

#ifdef __cplusplus
}
#endif

#endif // RUUVI_GATEWAY_ESP_NRF52FW_H
