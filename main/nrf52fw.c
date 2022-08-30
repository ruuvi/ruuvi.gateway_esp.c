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
#include "os_malloc.h"
#include "os_str.h"
#include "gpio_switch_ctrl.h"

#define LOG_LOCAL_LEVEL LOG_LEVEL_DEBUG
#include "log.h"

#define NRF52FW_IUCR_BASE_ADDR (0x10001000)
#define NRF52FW_IUCR_FW_VER    (NRF52FW_IUCR_BASE_ADDR + 0x080)

#define NRF52FW_SLEEP_WHILE_FLASHING_MS (20U)

static const char *TAG = "nRF52Fw";

NRF52FW_STATIC
bool
nrf52fw_parse_version_digit(const char *p_digit_str, const char *p_digit_str_end, uint8_t *p_digit)
{
    const char *   end = p_digit_str_end;
    const uint32_t val = os_str_to_uint32_cptr(p_digit_str, &end, 10);
    if (NULL == p_digit_str_end)
    {
        if ('\0' != *end)
        {
            return false;
        }
    }
    else
    {
        if (p_digit_str_end != end)
        {
            return false;
        }
    }
    if (val > UINT8_MAX)
    {
        return false;
    }
    *p_digit = (uint8_t)val;
    return true;
}

NRF52FW_STATIC
bool
nrf52fw_parse_digit_update_ver(
    const char *  p_token_begin,
    const char *  p_token_end,
    uint32_t *    p_version,
    const uint8_t byte_num)
{
    uint8_t digit_val = 0;
    if (!nrf52fw_parse_version_digit(p_token_begin, p_token_end, &digit_val))
    {
        return false;
    }
    const uint32_t num_bits_per_byte = 8U;
    *p_version |= (uint32_t)digit_val << (num_bits_per_byte * byte_num);
    return true;
}

NRF52FW_STATIC
bool
nrf52fw_parse_version(const char *p_version_str, ruuvi_nrf52_fw_ver_t *const p_version)
{
    uint32_t version = 0;

    if (NULL == p_version_str)
    {
        return false;
    }

    const char *p_token_begin = p_version_str;
    if ('\0' == *p_token_begin)
    {
        return false;
    }
    const char *p_token_end = strchr(p_token_begin, '.');
    if (NULL == p_token_end)
    {
        return false;
    }
    uint8_t byte_num = 3U;
    if (!nrf52fw_parse_digit_update_ver(p_token_begin, p_token_end, &version, byte_num))
    {
        return false;
    }
    byte_num -= 1;

    p_token_begin = p_token_end + 1;
    if ('\0' == *p_token_begin)
    {
        return false;
    }
    p_token_end = strchr(p_token_begin, '.');
    if (NULL == p_token_end)
    {
        return false;
    }
    if (!nrf52fw_parse_digit_update_ver(p_token_begin, p_token_end, &version, byte_num))
    {
        return false;
    }
    byte_num -= 1;

    p_token_begin = p_token_end + 1;
    if ('\0' == *p_token_begin)
    {
        return false;
    }
    if (!nrf52fw_parse_digit_update_ver(p_token_begin, NULL, &version, byte_num))
    {
        return false;
    }
    if (NULL != p_version)
    {
        p_version->version = version;
    }
    return true;
}

NRF52FW_STATIC
bool
nrf52fw_parse_version_line(const char *p_version_line, ruuvi_nrf52_fw_ver_t *const p_version)
{
    const char * version_prefix     = "# v";
    const size_t version_prefix_len = strlen(version_prefix);
    if (0 != strncmp(p_version_line, version_prefix, version_prefix_len))
    {
        return false;
    }
    return nrf52fw_parse_version(&p_version_line[version_prefix_len], p_version);
}

NRF52FW_STATIC
void
nrf52fw_line_rstrip(char *p_line_buf, const size_t line_buf_size)
{
    p_line_buf[line_buf_size - 1] = '\0';
    size_t len                    = strlen(p_line_buf);
    bool   flag_stop              = false;
    while ((len > 0) && (!flag_stop))
    {
        len -= 1;
        switch (p_line_buf[len])
        {
            case '\n':
            case '\r':
            case ' ':
            case '\t':
                p_line_buf[len] = '\0';
                break;
            default:
                flag_stop = true;
                break;
        }
    }
}

NRF52FW_STATIC
bool
nrf52fw_parse_segment_info_line(const char *p_version_line, nrf52fw_segment_t *p_segment)
{
    const char *   p_token_start = p_version_line;
    const char *   end           = NULL;
    const uint32_t segment_addr  = os_str_to_uint32_cptr(p_token_start, &end, 16);
    if ((' ' != *end) && ('\t' != *end))
    {
        return false;
    }
    while ((' ' == *end) || ('\t' == *end))
    {
        end += 1;
    }
    p_token_start              = end;
    end                        = NULL;
    const uint32_t segment_len = os_str_to_uint32_cptr(p_token_start, &end, 0);
    if ((' ' != *end) && ('\t' != *end))
    {
        return false;
    }
    while ((' ' == *end) || ('\t' == *end))
    {
        end += 1;
    }
    p_token_start                         = end;
    const char *p_segment_file_name_begin = p_token_start;
    while ((' ' != *end) && ('\t' != *end))
    {
        end += 1;
    }
    const char * p_segment_file_name_end = end;
    const size_t segment_file_name_len   = (size_t)(ptrdiff_t)(p_segment_file_name_end - p_segment_file_name_begin);
    if (segment_file_name_len >= sizeof(p_segment->file_name))
    {
        return false;
    }
    p_token_start              = end;
    end                        = NULL;
    const uint32_t segment_crc = os_str_to_uint32_cptr(p_token_start, &end, 16);
    if ('\0' != *end)
    {
        return false;
    }
    p_segment->address = segment_addr;
    p_segment->size    = segment_len;
    snprintf(
        p_segment->file_name,
        sizeof(p_segment->file_name),
        "%.*s",
        (printf_precision_t)segment_file_name_len,
        p_segment_file_name_begin);
    p_segment->crc = segment_crc;
    return true;
}

NRF52FW_STATIC
int32_t
nrf52fw_parse_info_file(FILE *p_fd, nrf52fw_info_t *p_info)
{
    p_info->fw_ver.version = 0;
    p_info->num_segments   = 0;

    char    line_buf[80];
    int32_t err_line_num = 0;
    int32_t line_cnt     = 0;
    while (fgets(line_buf, sizeof(line_buf), p_fd) != NULL)
    {
        line_cnt += 1;
        nrf52fw_line_rstrip(line_buf, sizeof(line_buf));
        if (1 == line_cnt)
        {
            if (!nrf52fw_parse_version_line(line_buf, &p_info->fw_ver))
            {
                err_line_num = line_cnt;
                break;
            }
        }
        else
        {
            if (p_info->num_segments >= (sizeof(p_info->segments) / sizeof(p_info->segments[0])))
            {
                err_line_num = line_cnt;
                break;
            }
            if (!nrf52fw_parse_segment_info_line(line_buf, &p_info->segments[p_info->num_segments]))
            {
                err_line_num = line_cnt;
                break;
            }
            p_info->num_segments += 1;
        }
    }
    return err_line_num;
}

NRF52FW_STATIC
bool
nrf52fw_read_info_txt(const flash_fat_fs_t *p_ffs, const char *p_path_info_txt, nrf52fw_info_t *p_info)
{
    memset(p_info, 0, sizeof(*p_info));
    const bool flag_use_binary_mode = false;

    FILE *p_fd = flashfatfs_fopen(p_ffs, p_path_info_txt, flag_use_binary_mode);
    if (NULL == p_fd)
    {
        LOG_ERR("Can't open: %s", p_path_info_txt);
        return false;
    }
    const int32_t err_line_num = nrf52fw_parse_info_file(p_fd, p_info);
    fclose(p_fd);
    if (0 != err_line_num)
    {
        LOG_ERR("Failed to parse '%s' at line %d", p_path_info_txt, err_line_num);
        return false;
    }
    return true;
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
nrf52fw_read_current_fw_ver(ruuvi_nrf52_fw_ver_t *const p_fw_ver)
{
    return nrf52swd_read_mem(NRF52FW_IUCR_FW_VER, 1, &p_fw_ver->version);
}

NRF52FW_STATIC
bool
nrf52fw_write_current_fw_ver(const uint32_t fw_ver)
{
    return nrf52swd_write_mem(NRF52FW_IUCR_FW_VER, 1, &fw_ver);
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
nrf52fw_file_read(const file_descriptor_t fd, void *p_buf, const size_t buf_size)
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
    nrf52fw_tmp_buf_t *p_tmp_buf,
    const int32_t      len,
    const uint32_t     segment_addr,
    const size_t       segment_len,
    uint32_t *         p_offset)
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
    if (!nrf52swd_write_mem(addr, len / sizeof(uint32_t), p_tmp_buf->buf_wr))
    {
        LOG_ERR("%s failed", "nrf52swd_write_mem");
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
    nrf52fw_tmp_buf_t *            p_tmp_buf,
    const uint32_t                 segment_addr,
    const size_t                   segment_len,
    nrf52fw_progress_info_t *const p_progress_info)
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
    const flash_fat_fs_t *         p_ffs,
    const char *                   p_path,
    nrf52fw_tmp_buf_t *            p_tmp_buf,
    const uint32_t                 segment_addr,
    const size_t                   segment_len,
    nrf52fw_progress_info_t *const p_progress_info)
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
    const flash_fat_fs_t *p_ffs,
    nrf52fw_tmp_buf_t *   p_tmp_buf,
    const nrf52fw_info_t *p_fw_info,
    nrf52fw_cb_progress   cb_progress,
    void *const           p_param_cb_progress)
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
        const nrf52fw_segment_t *p_segment_info = &p_fw_info->segments[i];
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
        const nrf52fw_segment_t *p_segment_info = &p_fw_info->segments[i];
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
    if (!nrf52fw_write_current_fw_ver(p_fw_info->fw_ver.version))
    {
        LOG_ERR("Failed to write firmware version");
        return false;
    }
    return true;
}

NRF52FW_STATIC
bool
nrf52fw_calc_segment_crc(
    const file_descriptor_t fd,
    nrf52fw_tmp_buf_t *     p_tmp_buf,
    const size_t            segment_len,
    uint32_t *              p_crc)
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
        actual_crc = crc32_le(actual_crc, (void *)p_tmp_buf->buf_wr, len);
    }
    *p_crc = actual_crc;
    return true;
}

NRF52FW_STATIC
bool
nrf52fw_check_fw_segment_crc(
    const flash_fat_fs_t *   p_ffs,
    nrf52fw_tmp_buf_t *      p_tmp_buf,
    const nrf52fw_segment_t *p_segment_info)
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
nrf52fw_check_firmware(const flash_fat_fs_t *p_ffs, nrf52fw_tmp_buf_t *p_tmp_buf, const nrf52fw_info_t *p_fw_info)
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

NRF52FW_STATIC
bool
nrf52fw_update_fw_step3(
    const flash_fat_fs_t *      p_ffs,
    nrf52fw_tmp_buf_t *         p_tmp_buf,
    nrf52fw_cb_progress         cb_progress,
    void *const                 p_param_cb_progress,
    nrf52fw_cb_before_updating  cb_before_updating,
    nrf52fw_cb_after_updating   cb_after_updating,
    ruuvi_nrf52_fw_ver_t *const p_nrf52_fw_ver)
{
    nrf52fw_info_t fw_info = { 0 };
    if (!nrf52fw_read_info_txt(p_ffs, "info.txt", &fw_info))
    {
        LOG_ERR("%s failed", "nrf52fw_read_info_txt");
        return false;
    }
    const ruuvi_nrf52_fw_ver_str_t fatfs_nrf52_fw_ver = nrf52_fw_ver_get_str(&fw_info.fw_ver);
    LOG_INFO("Firmware on FatFS: %s", fatfs_nrf52_fw_ver.buf);

    ruuvi_nrf52_fw_ver_t cur_fw_ver = { 0 };
    if (!nrf52fw_read_current_fw_ver(&cur_fw_ver))
    {
        LOG_ERR("%s failed", "nrf52fw_read_current_fw_ver");
        return false;
    }
    if (NULL != p_nrf52_fw_ver)
    {
        *p_nrf52_fw_ver = cur_fw_ver;
    }
    ruuvi_nrf52_fw_ver_str_t cur_nrf52_ver = nrf52_fw_ver_get_str(&cur_fw_ver);
    LOG_INFO("### Firmware on nRF52: %s", cur_nrf52_ver.buf);

    if (cur_fw_ver.version == fw_info.fw_ver.version)
    {
        LOG_INFO("### Firmware updating is not needed");
        return true;
    }

    LOG_INFO("### Need to update firmware on nRF52");
    if (NULL != cb_before_updating)
    {
        cb_before_updating();
    }
    if (!nrf52fw_check_firmware(p_ffs, p_tmp_buf, &fw_info))
    {
        LOG_ERR("%s failed", "nrf52fw_check_firmware");
        return false;
    }
    if (!nrf52fw_flash_write_firmware(p_ffs, p_tmp_buf, &fw_info, cb_progress, p_param_cb_progress))
    {
        LOG_ERR("%s failed", "nrf52fw_flash_write_firmware");
        return false;
    }
    if (NULL != cb_after_updating)
    {
        cb_after_updating();
    }
    if (!nrf52fw_read_current_fw_ver(&cur_fw_ver))
    {
        LOG_ERR("%s failed", "nrf52fw_read_current_fw_ver");
        return false;
    }
    if (NULL != p_nrf52_fw_ver)
    {
        *p_nrf52_fw_ver = cur_fw_ver;
    }
    cur_nrf52_ver = nrf52_fw_ver_get_str(&cur_fw_ver);
    LOG_INFO("Firmware on nRF52: %s", cur_nrf52_ver.buf);
    return true;
}

NRF52FW_STATIC
bool
nrf52fw_update_fw_step2(
    const flash_fat_fs_t *      p_ffs,
    nrf52fw_cb_progress         cb_progress,
    void *const                 p_param_cb_progress,
    nrf52fw_cb_before_updating  cb_before_updating,
    nrf52fw_cb_after_updating   cb_after_updating,
    ruuvi_nrf52_fw_ver_t *const p_nrf52_fw_ver)
{
    nrf52fw_tmp_buf_t *p_tmp_buf = os_malloc(sizeof(*p_tmp_buf));
    if (NULL == p_tmp_buf)
    {
        LOG_ERR("%s failed", "os_malloc");
        return false;
    }
    const bool result = nrf52fw_update_fw_step3(
        p_ffs,
        p_tmp_buf,
        cb_progress,
        p_param_cb_progress,
        cb_before_updating,
        cb_after_updating,
        p_nrf52_fw_ver);
    os_free(p_tmp_buf);
    return result;
}

NRF52FW_STATIC
bool
nrf52fw_update_fw_step1(
    const char *const           p_fatfs_nrf52_partition_name,
    nrf52fw_cb_progress         cb_progress,
    void *const                 p_param_cb_progress,
    nrf52fw_cb_before_updating  cb_before_updating,
    nrf52fw_cb_after_updating   cb_after_updating,
    ruuvi_nrf52_fw_ver_t *const p_nrf52_fw_ver)
{
    const flash_fat_fs_num_files_t max_num_files = 1;

    const flash_fat_fs_t *p_ffs = flashfatfs_mount("/fs_nrf52", p_fatfs_nrf52_partition_name, max_num_files);
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
    const char *const           p_fatfs_nrf52_partition_name,
    nrf52fw_cb_progress         cb_progress,
    void *const                 p_param_cb_progress,
    nrf52fw_cb_before_updating  cb_before_updating,
    nrf52fw_cb_after_updating   cb_after_updating,
    ruuvi_nrf52_fw_ver_t *const p_nrf52_fw_ver)
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

    if (result)
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
    const char *const           p_fatfs_nrf52_partition_name,
    nrf52fw_cb_progress         cb_progress,
    void *const                 p_param_cb_progress,
    nrf52fw_cb_before_updating  cb_before_updating,
    nrf52fw_cb_after_updating   cb_after_updating,
    ruuvi_nrf52_fw_ver_t *const p_nrf52_fw_ver)
{
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
        p_nrf52_fw_ver);

    nrf52fw_hw_reset_nrf52(true);
    vTaskDelay(ticks_in_reset_state);
    nrf52fw_hw_reset_nrf52(false);

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
