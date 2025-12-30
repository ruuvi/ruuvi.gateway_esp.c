/**
 * @file nrf52fw.c
 * @author TheSomeMan
 * @date 2020-09-13
 * @copyright Ruuvi Innovations Ltd, license BSD-3-Clause.
 */

#include "nrf52fw.h"
#include <string.h>
#include <unistd.h>
#include "flashfatfs.h"
#include "nrf52swd.h"
#include "esp32/rom/crc.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <mbedtls/sha256.h>
#include "os_malloc.h"
#include "os_str.h"
#include "gpio_switch_ctrl.h"
#include "adv_post_green_led.h"
#include "nrf52fw_info_txt.h"

#define LOG_LOCAL_LEVEL LOG_LEVEL_INFO
#include "log.h"

#define NRF52FW_SLEEP_WHILE_FLASHING_MS (20U)

#define NRF52FW_ERASED_FLASH_DWORD_VAL (0xFFFFFFFFU)

#define NRF52FW_SHA256_DIGEST_SIZE (32U)

typedef struct nrf52fw_sha256_t
{
    uint8_t digest[NRF52FW_SHA256_DIGEST_SIZE];
} nrf52fw_sha256_t;

typedef struct nrf52fw_update_tmp_data_t
{
    nrf52fw_tmp_buf_t        tmp_buf;
    nrf52fw_info_t           fw_info;
    ruuvi_nrf52_fw_ver_str_t fatfs_nrf52_fw_ver;
    ruuvi_nrf52_fw_ver_t     cur_fw_ver;
    ruuvi_nrf52_fw_ver_str_t cur_nrf52_ver;
    ruuvi_nrf52_hw_rev_t     nrf52_hw_rev;
    nrf52fw_sha256_t         sha256_digest_fatfs;
    nrf52swd_sha256_t        sha256_digest_nrf52;
    nrf52swd_segment_t       sha256_stub_mem_segments[NRF52SWD_SHA256_MAX_SEGMENTS];
} nrf52fw_update_tmp_data_t;

static const char* TAG = "nRF52Fw";

#if !RUUVI_TESTS_NRF52FW
static portMUX_TYPE g_nrf52fw_manual_reset_mode_spinlock = portMUX_INITIALIZER_UNLOCKED;
#endif
static bool g_nrf52fw_flag_manual_reset_mode = false;

void
nrf52fw_set_manual_reset_mode(const bool flag_manual_reset_mode)
{
#if !RUUVI_TESTS_NRF52FW
    portENTER_CRITICAL(&g_nrf52fw_manual_reset_mode_spinlock);
#endif
    g_nrf52fw_flag_manual_reset_mode = flag_manual_reset_mode;
#if !RUUVI_TESTS_NRF52FW
    portEXIT_CRITICAL(&g_nrf52fw_manual_reset_mode_spinlock);
#endif
    if (flag_manual_reset_mode)
    {
        LOG_INFO("nRF52 manual reset mode: ON");
    }
    else
    {
        LOG_INFO("nRF52 manual reset mode: OFF");
    }
}

bool
nrf52fw_get_manual_reset_mode(void)
{
#if !RUUVI_TESTS_NRF52FW
    portENTER_CRITICAL(&g_nrf52fw_manual_reset_mode_spinlock);
#endif
    const bool flag_manual_reset_mode = g_nrf52fw_flag_manual_reset_mode;
#if !RUUVI_TESTS_NRF52FW
    portEXIT_CRITICAL(&g_nrf52fw_manual_reset_mode_spinlock);
#endif
    return flag_manual_reset_mode;
}

NRF52FW_STATIC
bool
nrf52fw_init_swd(void)
{
    LOG_INFO("Init SWD");
    if (!nrf52swd_init())
    {
        LOG_ERR("nrf52swd_init failed");
        return false;
    }
    if (!nrf52swd_check_id_code())
    {
        LOG_ERR("nrf52swd_check_id_code failed");
        return false;
    }
    if (!nrf52swd_debug_halt())
    {
        LOG_ERR("nrf52swd_debug_halt failed");
        return false;
    }
    if (!nrf52swd_debug_enable_reset_vector_catch())
    {
        LOG_ERR("nrf52swd_debug_enable_reset_vector_catch failed");
        return false;
    }
    if (!nrf52swd_debug_reset())
    {
        LOG_ERR("nrf52swd_debug_reset failed");
        return false;
    }
    return true;
}

NRF52FW_STATIC
void
nrf52fw_deinit_swd(void)
{
    LOG_INFO("Deinit SWD");
    nrf52swd_deinit();
}

NRF52FW_STATIC
bool
nrf52fw_read_current_fw_ver(ruuvi_nrf52_fw_ver_t* const p_fw_ver)
{
    return nrf52swd_read_mem(NRF52FW_UICR_FW_VER, 1, &p_fw_ver->version);
}

NRF52FW_STATIC
bool
nrf52fw_read_nrf52_ficr_hw_rev(ruuvi_nrf52_hw_rev_t* const p_hw_ver)
{
    return nrf52swd_read_mem(NRF52FW_FICR_INFO_PART, 1, &p_hw_ver->part_code);
}

NRF52FW_STATIC
bool
nrf52fw_write_current_fw_ver(const uint32_t fw_ver)
{
    return nrf52swd_write_flash(NRF52FW_UICR_FW_VER, 1, &fw_ver);
}

#if RUUVI_TESTS_NRF52FW

static bool g_nrf52fw_flag_simulate_file_read_error;

NRF52FW_STATIC
void
nrf52fw_simulate_file_read_error(const bool flag_error)
{
    g_nrf52fw_flag_simulate_file_read_error = flag_error;
}
#endif

NRF52FW_STATIC
int32_t
nrf52fw_file_read(const file_descriptor_t fd, void* p_buf, const size_t buf_size)
{
#if RUUVI_TESTS_NRF52FW
    if (g_nrf52fw_flag_simulate_file_read_error)
    {
        return -1;
    }
#endif
    return read(fd, p_buf, buf_size);
}

NRF52FW_STATIC
bool
nrf52fw_flash_write_block(
    nrf52fw_tmp_buf_t* p_tmp_buf,
    const int32_t      len,
    const uint32_t     segment_addr,
    const size_t       segment_len,
    uint32_t*          p_offset)
{
    const uint32_t addr = segment_addr + *p_offset;
    if (0 != (len % sizeof(uint32_t)))
    {
        LOG_ERR("bad len %d", len);
        return false;
    }
    *p_offset += len;
    if (*p_offset > segment_len)
    {
        LOG_ERR("offset %u greater than segment len %u", *p_offset, segment_len);
        return false;
    }
    LOG_INFO("Writing 0x%08x...", addr);
    if (!nrf52swd_write_flash(addr, len / sizeof(uint32_t), p_tmp_buf->buf_wr))
    {
        LOG_ERR("%s failed", "nrf52swd_write_flash");
        return false;
    }
#if NRF52FW_ENABLE_FLASH_VERIFICATION
    if (!nrf52swd_read_mem(addr, len / sizeof(uint32_t), p_tmp_buf->buf_rd))
    {
        LOG_ERR("%s failed", "nrf52swd_read_mem");
        return false;
    }
    if (0 != memcmp(p_tmp_buf->buf_wr, p_tmp_buf->buf_rd, len))
    {
        LOG_ERR("%s failed", "verify");
        return false;
    }
#endif
    return true;
}

NRF52FW_STATIC
bool
nrf52fw_flash_write_segment(
    const file_descriptor_t        fd,
    nrf52fw_tmp_buf_t*             p_tmp_buf,
    const uint32_t                 segment_addr,
    const size_t                   segment_len,
    nrf52fw_progress_info_t* const p_progress_info)
{
    uint32_t offset = 0;
    for (;;)
    {
        const int32_t len = nrf52fw_file_read(fd, p_tmp_buf->buf_wr, sizeof(p_tmp_buf->buf_wr));
        if (0 == len)
        {
            break;
        }
        if (len < 0)
        {
            LOG_ERR("%s failed", "nrf52fw_file_read");
            return false;
        }
        if (!nrf52fw_flash_write_block(p_tmp_buf, len, segment_addr, segment_len, &offset))
        {
            return false;
        }
        if (NULL != p_progress_info)
        {
            p_progress_info->accum_num_bytes_flashed += len;
            if (NULL != p_progress_info->cb_progress)
            {
                p_progress_info->cb_progress(
                    p_progress_info->accum_num_bytes_flashed,
                    p_progress_info->total_size,
                    p_progress_info->p_param_cb_progress);
            }
        }
        vTaskDelay(pdMS_TO_TICKS(NRF52FW_SLEEP_WHILE_FLASHING_MS));
    }
    return true;
}

NRF52FW_STATIC
bool
nrf52fw_write_segment_from_file(
    const flash_fat_fs_t*          p_ffs,
    const char*                    p_path,
    nrf52fw_tmp_buf_t*             p_tmp_buf,
    const uint32_t                 segment_addr,
    const size_t                   segment_len,
    nrf52fw_progress_info_t* const p_progress_info)
{
    const file_descriptor_t fd = flashfatfs_open(p_ffs, p_path);
    if (fd < 0)
    {
        LOG_ERR("Can't open '%s'", p_path);
        return false;
    }
    const bool res = nrf52fw_flash_write_segment(fd, p_tmp_buf, segment_addr, segment_len, p_progress_info);
    close(fd);
    if (!res)
    {
        LOG_ERR("Failed to write segment 0x%08x from '%s'", segment_addr, p_path);
        return false;
    }
    return true;
}

NRF52FW_STATIC
bool
nrf52fw_erase_flash(void)
{
    LOG_INFO("Erasing flash memory...");
    if (!nrf52swd_erase_all())
    {
        LOG_ERR("%s failed", "nrf52swd_erase_all");
        return false;
    }
    return true;
}

NRF52FW_STATIC
bool
nrf52fw_flash_write_firmware(
    const flash_fat_fs_t* p_ffs,
    nrf52fw_tmp_buf_t*    p_tmp_buf,
    const nrf52fw_info_t* p_fw_info,
    nrf52fw_cb_progress   cb_progress,
    void* const           p_param_cb_progress)
{
    if (!nrf52fw_erase_flash())
    {
        LOG_ERR("%s failed", "nrf52fw_erase_flash");
        return false;
    }
    LOG_INFO("Flash %u segments", p_fw_info->num_segments);
    size_t total_size = 0;
    for (uint32_t i = 0; i < p_fw_info->num_segments; ++i)
    {
        const nrf52fw_segment_t* const p_segment_info = &p_fw_info->segments[i];
        total_size += p_segment_info->size;
    }
    nrf52fw_progress_info_t progress_info = {
        .accum_num_bytes_flashed = 0,
        .total_size              = total_size,
        .cb_progress             = cb_progress,
        .p_param_cb_progress     = p_param_cb_progress,
    };
    for (uint32_t i = 0; i < p_fw_info->num_segments; ++i)
    {
        const nrf52fw_segment_t* p_segment_info = &p_fw_info->segments[i];
        LOG_INFO(
            "Flash segment %u: 0x%08x size=%u from %s",
            i,
            p_segment_info->address,
            p_segment_info->size,
            p_segment_info->file_name);
        if (!nrf52fw_write_segment_from_file(
                p_ffs,
                p_segment_info->file_name,
                p_tmp_buf,
                p_segment_info->address,
                p_segment_info->size,
                &progress_info))
        {
            LOG_ERR(
                "Failed to write segment %u: 0x%08x from %s",
                i,
                p_segment_info->address,
                p_segment_info->file_name);
            return false;
        }
    }
    /*
     * Starting from gateway_nrf v2.0.0 the firmware image.hex file contains the firmware version in the last segment,
     * but only if it's a released version.
     * So, we need to write the firmware version to the UICR register only if it was not written yet.
     */
    ruuvi_nrf52_fw_ver_t fw_ver = { 0 };
    if (!nrf52fw_read_current_fw_ver(&fw_ver))
    {
        LOG_ERR("%s failed", "nrf52fw_read_current_fw_ver");
        return false;
    }
    if (NRF52FW_ERASED_FLASH_DWORD_VAL == fw_ver.version)
    {
        if (!nrf52fw_write_current_fw_ver(p_fw_info->fw_ver.version))
        {
            LOG_ERR("Failed to write firmware version");
            return false;
        }
    }
    else
    {
        if (fw_ver.version != p_fw_info->fw_ver.version)
        {
            LOG_ERR(
                "Firmware version in the firmware image: 0x%08x, but expected version is 0x%08x",
                fw_ver.version,
                p_fw_info->fw_ver.version);
            return false;
        }
    }
    return true;
}

NRF52FW_STATIC
bool
nrf52fw_calc_segment_crc(
    const file_descriptor_t fd,
    nrf52fw_tmp_buf_t*      p_tmp_buf,
    const size_t            segment_len,
    uint32_t*               p_crc)
{
    uint32_t offset     = 0U;
    uint32_t actual_crc = 0U;
    for (;;)
    {
        const int32_t len = nrf52fw_file_read(fd, p_tmp_buf->buf_wr, sizeof(p_tmp_buf->buf_wr));
        if (len < 0)
        {
            LOG_ERR("%s failed", "nrf52fw_file_read");
            return false;
        }
        if (0 == len)
        {
            break;
        }
        if (0 != (len % sizeof(uint32_t)))
        {
            LOG_ERR("bad len %d", len);
            return false;
        }
        offset += len;
        if (offset > segment_len)
        {
            LOG_ERR("offset %u greater than segment len %u", offset, segment_len);
            return false;
        }
        actual_crc = crc32_le(actual_crc, (void*)p_tmp_buf->buf_wr, len);
    }
    *p_crc = actual_crc;
    return true;
}

NRF52FW_STATIC
bool
nrf52fw_check_fw_segment_crc(
    const flash_fat_fs_t*    p_ffs,
    nrf52fw_tmp_buf_t*       p_tmp_buf,
    const nrf52fw_segment_t* p_segment_info)
{
    const file_descriptor_t fd = flashfatfs_open(p_ffs, p_segment_info->file_name);
    if (fd < 0)
    {
        LOG_ERR("Can't open '%s'", p_segment_info->file_name);
        return false;
    }

    uint32_t   crc = 0;
    const bool res = nrf52fw_calc_segment_crc(fd, p_tmp_buf, p_segment_info->size, &crc);
    close(fd);

    if (!res)
    {
        LOG_ERR("%s failed", "nrf52fw_calc_segment_crc");
        return false;
    }
    if (p_segment_info->crc != crc)
    {
        LOG_ERR(
            "Segment: 0x%08x: expected CRC: 0x%08x, actual CRC: 0x%08x",
            p_segment_info->address,
            p_segment_info->crc,
            crc);
        return false;
    }
    return true;
}

NRF52FW_STATIC
bool
nrf52fw_check_firmware(const flash_fat_fs_t* p_ffs, nrf52fw_tmp_buf_t* p_tmp_buf, const nrf52fw_info_t* p_fw_info)
{
    for (uint32_t i = 0; i < p_fw_info->num_segments; ++i)
    {
        if (!nrf52fw_check_fw_segment_crc(p_ffs, p_tmp_buf, &p_fw_info->segments[i]))
        {
            return false;
        }
    }
    return true;
}

static bool
nrf52fw_update_sha256_for_fd(
    mbedtls_sha256_context*  p_sha256_ctx,
    const file_descriptor_t  fd,
    const uint32_t           buf_len,
    nrf52fw_tmp_buf_t* const p_tmp_buf)
{
    uint32_t rem_len = buf_len;
    while (rem_len > 0)
    {
        const uint32_t len_to_read = (rem_len > sizeof(p_tmp_buf->buf_wr)) ? sizeof(p_tmp_buf->buf_wr) : rem_len;
        const int32_t  len         = nrf52fw_file_read(fd, p_tmp_buf->buf_wr, len_to_read);
        if (len < 0)
        {
            LOG_ERR("%s failed", "nrf52fw_file_read");
            return false;
        }
        if (len != (int32_t)len_to_read)
        {
            LOG_ERR("read len %d not equal to expected len %u", len, len_to_read);
            return false;
        }
        rem_len -= (uint32_t)len;
        if (mbedtls_sha256_update(p_sha256_ctx, (const unsigned char*)p_tmp_buf->buf_wr, len) < 0)
        {
            LOG_ERR("%s failed", "mbedtls_sha256_update");
            return false;
        }
    }
    return true;
}

static bool
nrf52fw_update_sha256_for_segment(
    mbedtls_sha256_context*        p_sha256_ctx,
    const nrf52fw_segment_t* const p_segment_info,
    const flash_fat_fs_t*          p_ffs,
    nrf52fw_tmp_buf_t* const       p_tmp_buf)
{
    const file_descriptor_t fd = flashfatfs_open(p_ffs, p_segment_info->file_name);
    LOG_INFO(
        "Opened file '%s' for SHA256 calculation (size %u bytes)",
        p_segment_info->file_name,
        p_segment_info->size);
    if (fd < 0)
    {
        LOG_ERR("Can't open '%s'", p_segment_info->file_name);
        return false;
    }
    const bool res = nrf52fw_update_sha256_for_fd(p_sha256_ctx, fd, p_segment_info->size, p_tmp_buf);
    close(fd);
    return res;
}

static bool
nrf52fw_calc_sha256_internal(
    mbedtls_sha256_context*     p_sha256_ctx,
    const nrf52fw_info_t* const p_fw_info,
    nrf52fw_sha256_t* const     p_out_sha256,
    const flash_fat_fs_t*       p_ffs,
    nrf52fw_tmp_buf_t* const    p_tmp_buf)
{
    mbedtls_sha256_init(p_sha256_ctx);
    if (mbedtls_sha256_starts(p_sha256_ctx, 0) < 0)
    {
        LOG_ERR("%s failed", "mbedtls_sha256_starts");
        return false;
    }
    for (uint32_t i = 0; i < p_fw_info->num_segments; ++i)
    {
        LOG_DBG("Calculating SHA256 for segment %u...", i);
        if (!nrf52fw_update_sha256_for_segment(p_sha256_ctx, &p_fw_info->segments[i], p_ffs, p_tmp_buf))
        {
            LOG_ERR("%s failed for segment %u", "nrf52fw_update_sha256_for_segment", i);
            return false;
        }
    }
    if (mbedtls_sha256_finish(p_sha256_ctx, p_out_sha256->digest) < 0)
    {
        LOG_ERR("%s failed", "mbedtls_sha256_finish");
        return false;
    }
    return true;
}

static bool
nrf52fw_calc_sha256(
    const nrf52fw_info_t* const p_fw_info,
    nrf52fw_sha256_t* const     p_out_sha256,
    const flash_fat_fs_t*       p_ffs,
    nrf52fw_tmp_buf_t* const    p_tmp_buf)
{
    mbedtls_sha256_context* p_sha256_ctx = os_calloc(1, sizeof(*p_sha256_ctx));
    if (NULL == p_sha256_ctx)
    {
        LOG_ERR("%s failed", "os_malloc");
        return false;
    }

    const bool res = nrf52fw_calc_sha256_internal(p_sha256_ctx, p_fw_info, p_out_sha256, p_ffs, p_tmp_buf);

    mbedtls_sha256_free(p_sha256_ctx);
    os_free(p_sha256_ctx);

    return res;
}

static void
nrf52fw_fill_sha256_stub_mem_segments(nrf52fw_update_tmp_data_t* const p_tmp_data)
{
    for (uint32_t i = 0; i < p_tmp_data->fw_info.num_segments; ++i)
    {
        const nrf52fw_segment_t* const p_segment_info = &p_tmp_data->fw_info.segments[i];
        if ((NRF52FW_UICR_FW_VER == p_segment_info->address) && (sizeof(uint32_t) == p_segment_info->size))
        {
            /*
             * Skip UICR.FW_VER segment for SHA256 calculation, because it is included only in the released firmware.
             */
            continue;
        }
        p_tmp_data->sha256_stub_mem_segments[i].start_addr = p_segment_info->address;
        p_tmp_data->sha256_stub_mem_segments[i].size_bytes = p_segment_info->size;
    }
}

NRF52FW_STATIC
bool
nrf52fw_update_fw_step4(
    const flash_fat_fs_t* const      p_ffs,
    nrf52fw_update_tmp_data_t* const p_tmp_data,
    nrf52fw_cb_progress              cb_progress,
    void* const                      p_param_cb_progress,
    ruuvi_nrf52_fw_ver_t* const      p_nrf52_fw_ver)
{
    if (!nrf52fw_flash_write_firmware(
            p_ffs,
            &p_tmp_data->tmp_buf,
            &p_tmp_data->fw_info,
            cb_progress,
            p_param_cb_progress))
    {
        LOG_ERR("%s failed", "nrf52fw_flash_write_firmware");
        if (NULL != p_nrf52_fw_ver)
        {
            nrf52fw_read_current_fw_ver(p_nrf52_fw_ver);
        }
        return false;
    }
    if (!nrf52fw_read_current_fw_ver(&p_tmp_data->cur_fw_ver))
    {
        LOG_ERR("%s failed", "nrf52fw_read_current_fw_ver");
        if (NULL != p_nrf52_fw_ver)
        {
            *p_nrf52_fw_ver = p_tmp_data->cur_fw_ver;
        }
        return false;
    }
    return true;
}

NRF52FW_STATIC
bool
nrf52fw_update_fw_step3(
    const flash_fat_fs_t* const      p_ffs,
    nrf52fw_update_tmp_data_t* const p_tmp_data,
    nrf52fw_cb_progress              cb_progress,
    void* const                      p_param_cb_progress,
    nrf52fw_cb_before_updating       cb_before_updating,
    nrf52fw_cb_after_updating        cb_after_updating,
    ruuvi_nrf52_fw_ver_t* const      p_nrf52_fw_ver)
{
    if (!nrf52fw_info_txt_read(p_ffs, "info.txt", &p_tmp_data->fw_info))
    {
        LOG_ERR("%s failed", "nrf52fw_info_txt_read");
        return false;
    }
    if (!nrf52fw_read_nrf52_ficr_hw_rev(&p_tmp_data->nrf52_hw_rev))
    {
        LOG_ERR("%s failed", "nrf52fw_read_nrf52_ficr_hw_rev");
        return false;
    }
    LOG_INFO("nRF52 factory information: Part code: 0x%08x", p_tmp_data->nrf52_hw_rev.part_code);

    if (!nrf52fw_read_current_fw_ver(&p_tmp_data->cur_fw_ver))
    {
        LOG_ERR("%s failed", "nrf52fw_read_current_fw_ver");
        return false;
    }
    p_tmp_data->cur_nrf52_ver = nrf52_fw_ver_get_str(&p_tmp_data->cur_fw_ver);
    LOG_INFO("### Firmware on nRF52: %s", p_tmp_data->cur_nrf52_ver.buf);
    if (NULL != p_nrf52_fw_ver)
    {
        *p_nrf52_fw_ver = p_tmp_data->cur_fw_ver;
    }

    nrf52fw_fill_sha256_stub_mem_segments(p_tmp_data);

    if (!nrf52swd_calc_sha256_digest_on_nrf52(
            &p_tmp_data->sha256_stub_mem_segments[0],
            p_tmp_data->fw_info.num_segments,
            &p_tmp_data->sha256_digest_nrf52))
    {
        LOG_ERR("Failed to calculate SHA256 digest on nRF52");
        return false;
    }
    LOG_DUMP_INFO(
        p_tmp_data->sha256_digest_nrf52.digest,
        sizeof(p_tmp_data->sha256_digest_nrf52.digest),
        "SHA256 digest of nRF52 firmware on nRF52");

    p_tmp_data->fatfs_nrf52_fw_ver = nrf52_fw_ver_get_str(&p_tmp_data->fw_info.fw_ver);
    LOG_INFO("Firmware on FatFS: %s", p_tmp_data->fatfs_nrf52_fw_ver.buf);
    if (!nrf52fw_check_firmware(p_ffs, &p_tmp_data->tmp_buf, &p_tmp_data->fw_info))
    {
        LOG_ERR("%s failed", "nrf52fw_check_firmware");
        return false;
    }
    if (!nrf52fw_calc_sha256(&p_tmp_data->fw_info, &p_tmp_data->sha256_digest_fatfs, p_ffs, &p_tmp_data->tmp_buf))
    {
        LOG_ERR("%s failed", "nrf52fw_calc_sha256");
        return false;
    }
    LOG_DUMP_INFO(
        p_tmp_data->sha256_digest_fatfs.digest,
        sizeof(p_tmp_data->sha256_digest_fatfs.digest),
        "SHA256 digest of nRF52 firmware on FatFS");

    if ((p_tmp_data->cur_fw_ver.version == p_tmp_data->fw_info.fw_ver.version)
        && (0
            == memcmp(
                p_tmp_data->sha256_digest_nrf52.digest,
                p_tmp_data->sha256_digest_fatfs.digest,
                sizeof(p_tmp_data->sha256_digest_nrf52.digest))))
    {
        LOG_INFO("### Firmware updating is not needed");
        return true;
    }

    LOG_INFO("### Need to update firmware on nRF52");
    if (NULL != cb_before_updating)
    {
        cb_before_updating();
    }

    const bool res = nrf52fw_update_fw_step4(p_ffs, p_tmp_data, cb_progress, p_param_cb_progress, p_nrf52_fw_ver);
    if (NULL != cb_after_updating)
    {
        cb_after_updating(true);
    }
    if (!res)
    {
        return false;
    }
    if (NULL != p_nrf52_fw_ver)
    {
        *p_nrf52_fw_ver = p_tmp_data->cur_fw_ver;
    }
    p_tmp_data->cur_nrf52_ver = nrf52_fw_ver_get_str(&p_tmp_data->cur_fw_ver);
    LOG_INFO("Firmware on nRF52: %s", p_tmp_data->cur_nrf52_ver.buf);
    return true;
}

NRF52FW_STATIC
bool
nrf52fw_update_fw_step2(
    const flash_fat_fs_t* const p_ffs,
    nrf52fw_cb_progress         cb_progress,
    void* const                 p_param_cb_progress,
    nrf52fw_cb_before_updating  cb_before_updating,
    nrf52fw_cb_after_updating   cb_after_updating,
    ruuvi_nrf52_fw_ver_t* const p_nrf52_fw_ver)
{
    nrf52fw_update_tmp_data_t* p_tmp_data = os_calloc(1, sizeof(*p_tmp_data));
    if (NULL == p_tmp_data)
    {
        LOG_ERR("%s failed", "os_malloc");
        return false;
    }
    const bool result = nrf52fw_update_fw_step3(
        p_ffs,
        p_tmp_data,
        cb_progress,
        p_param_cb_progress,
        cb_before_updating,
        cb_after_updating,
        p_nrf52_fw_ver);
    os_free(p_tmp_data);
    return result;
}

NRF52FW_STATIC
bool
nrf52fw_update_fw_step1(
    const char* const           p_fatfs_nrf52_partition_name,
    nrf52fw_cb_progress         cb_progress,
    void* const                 p_param_cb_progress,
    nrf52fw_cb_before_updating  cb_before_updating,
    nrf52fw_cb_after_updating   cb_after_updating,
    ruuvi_nrf52_fw_ver_t* const p_nrf52_fw_ver)
{
    const flash_fat_fs_num_files_t max_num_files = 1;

    const flash_fat_fs_t* p_ffs = flashfatfs_mount("/fs_nrf52", p_fatfs_nrf52_partition_name, max_num_files);
    if (NULL == p_ffs)
    {
        LOG_ERR("%s failed", "flashfatfs_mount");
        return false;
    }
    const bool result = nrf52fw_update_fw_step2(
        p_ffs,
        cb_progress,
        p_param_cb_progress,
        cb_before_updating,
        cb_after_updating,
        p_nrf52_fw_ver);
    flashfatfs_unmount(&p_ffs);
    return result;
}

NRF52FW_STATIC
bool
nrf52fw_update_fw_step0(
    const char* const           p_fatfs_nrf52_partition_name,
    nrf52fw_cb_progress         cb_progress,
    void* const                 p_param_cb_progress,
    nrf52fw_cb_before_updating  cb_before_updating,
    nrf52fw_cb_after_updating   cb_after_updating,
    ruuvi_nrf52_fw_ver_t* const p_nrf52_fw_ver,
    const bool                  flag_run_fw_after_update)
{
    if (!nrf52fw_init_swd())
    {
        LOG_ERR("%s failed", "nrf52fw_init_swd");
        return false;
    }

    bool result = nrf52fw_update_fw_step1(
        p_fatfs_nrf52_partition_name,
        cb_progress,
        p_param_cb_progress,
        cb_before_updating,
        cb_after_updating,
        p_nrf52_fw_ver);

    if (result && flag_run_fw_after_update)
    {
        result = nrf52swd_debug_run();
        if (!result)
        {
            LOG_ERR("%s failed", "nrf52swd_debug_run");
        }
    }
    nrf52fw_deinit_swd();
    return result;
}

void
nrf52fw_hw_reset_nrf52(const bool flag_reset)
{
    LOG_INFO("Hardware reset nRF52: %s", flag_reset ? "true" : "false");
    nrf52swd_init_gpio_cfg_nreset();
    if (flag_reset)
    {
        gpio_switch_ctrl_activate();
    }
    nrf52swd_reset(flag_reset);
    if (!flag_reset)
    {
        gpio_switch_ctrl_deactivate();
    }
}

bool
nrf52fw_update_fw_if_necessary(
    const char* const           p_fatfs_nrf52_partition_name,
    nrf52fw_cb_progress         cb_progress,
    void* const                 p_param_cb_progress,
    nrf52fw_cb_before_updating  cb_before_updating,
    nrf52fw_cb_after_updating   cb_after_updating,
    ruuvi_nrf52_fw_ver_t* const p_nrf52_fw_ver,
    const bool                  flag_run_fw_after_update)
{
    adv_post_green_led_async_disable();
    nrf52fw_set_manual_reset_mode(true);

    const TickType_t ticks_in_reset_state = 100;
    nrf52fw_hw_reset_nrf52(true);
    vTaskDelay(ticks_in_reset_state);
    nrf52fw_hw_reset_nrf52(false);

    const bool res = nrf52fw_update_fw_step0(
        p_fatfs_nrf52_partition_name,
        cb_progress,
        p_param_cb_progress,
        cb_before_updating,
        cb_after_updating,
        p_nrf52_fw_ver,
        flag_run_fw_after_update);

    nrf52fw_hw_reset_nrf52(true);
    if (!res)
    {
        LOG_ERR("### nRF52 firmware update check failed");
        return false;
    }
    if (flag_run_fw_after_update)
    {
        vTaskDelay(ticks_in_reset_state);
        nrf52fw_hw_reset_nrf52(false);
        nrf52fw_set_manual_reset_mode(false);
    }

    return res;
}

bool
nrf52fw_software_reset(void)
{
    LOG_INFO("nrf52fw_software_reset");
    if (!nrf52fw_init_swd())
    {
        LOG_ERR("%s failed", "nrf52fw_init_swd");
        return false;
    }
    if (!nrf52swd_debug_run())
    {
        LOG_ERR("%s failed", "nrf52swd_debug_run");
        nrf52fw_deinit_swd();
        return false;
    }
    nrf52fw_deinit_swd();
    return true;
}
