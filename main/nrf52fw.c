/**
 * @file nrf52fw.h
 * @author TheSomeMan
 * @date 2020-09-13
 * @copyright Ruuvi Innovations Ltd, license BSD-3-Clause.
 */

#include "nrf52fw.h"
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/stat.h>
#include "esp_log.h"
#include "flashfatfs.h"
#include "nrf52swd.h"
#include "esp32/rom/crc.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "app_malloc.h"
#if RUUVI_TESTS_NRF52FW
#include <stdio.h>
#endif

#if RUUVI_TESTS_NRF52FW
#define STATIC
#else
#define STATIC static
#endif

#define NRF52FW_IUCR_BASE_ADDR (0x10001000)
#define NRF52FW_IUCR_FW_VER    (NRF52FW_IUCR_BASE_ADDR + 0x080)

static const char *TAG = "nRF52Fw";

STATIC bool
nrf52fw_parse_version_digit(const char *p_digit_str, const char *p_digit_str_end, uint8_t *p_digit)
{
    bool  result = false;
    char *end    = (void *)p_digit_str_end;

    do
    {
        const uint32_t val = (uint32_t)strtoul(p_digit_str, &end, 10);
        if (NULL == p_digit_str_end)
        {
            if ('\0' != *end)
            {
                break;
            }
        }
        else
        {
            if (p_digit_str_end != end)
            {
                break;
            }
        }
        if (val > UINT8_MAX)
        {
            break;
        }
        *p_digit = (uint8_t)val;
        result   = true;
    } while (0);
    return result;
}

STATIC bool
nrf52fw_parse_digit_update_ver(
    const char *  p_token_begin,
    const char *  p_token_end,
    uint32_t *    p_version,
    const uint8_t byte_num)
{
    bool    result    = false;
    uint8_t digit_val = 0;
    if (nrf52fw_parse_version_digit(p_token_begin, p_token_end, &digit_val))
    {
        const uint32_t num_bits_per_byte = 8U;
        *p_version |= (uint32_t)digit_val << (num_bits_per_byte * byte_num);
        result = true;
    }
    return result;
}

STATIC bool
nrf52fw_parse_version(const char *p_version_str, NRF52Fw_Version_t *p_version)
{
    bool     result  = false;
    uint32_t version = 0;

    do
    {
        if (NULL == p_version_str)
        {
            break;
        }

        const char *p_token_begin = p_version_str;
        if ('\0' == *p_token_begin)
        {
            break;
        }
        const char *p_token_end = strchr(p_token_begin, '.');
        if (NULL == p_token_end)
        {
            break;
        }
        uint8_t byte_num = 3U;
        if (!nrf52fw_parse_digit_update_ver(p_token_begin, p_token_end, &version, byte_num))
        {
            break;
        }
        byte_num -= 1;

        p_token_begin = p_token_end + 1;
        if ('\0' == *p_token_begin)
        {
            break;
        }
        p_token_end = strchr(p_token_begin, '.');
        if (NULL == p_token_end)
        {
            break;
        }
        if (!nrf52fw_parse_digit_update_ver(p_token_begin, p_token_end, &version, byte_num))
        {
            break;
        }
        byte_num -= 1;

        p_token_begin = p_token_end + 1;
        if ('\0' == *p_token_begin)
        {
            break;
        }
        if (!nrf52fw_parse_digit_update_ver(p_token_begin, NULL, &version, byte_num))
        {
            break;
        }
        if (NULL != p_version)
        {
            p_version->version = version;
        }
        result = true;
    } while (0);
    return result;
}

STATIC bool
nrf52fw_parse_version_line(const char *p_version_line, NRF52Fw_Version_t *p_version)
{
    bool         result             = false;
    const char * version_prefix     = "# v";
    const size_t version_prefix_len = strlen(version_prefix);
    if (0 == strncmp(p_version_line, version_prefix, version_prefix_len))
    {
        result = nrf52fw_parse_version(&p_version_line[version_prefix_len], p_version);
    }
    return result;
}

STATIC void
nrf52fw_line_rstrip(char *p_line_buf, const size_t line_buf_size)
{
    p_line_buf[line_buf_size - 1] = '\0';
    size_t len                    = strlen(p_line_buf);
    bool   flag_stop              = false;
    while ((len > 0) && !flag_stop)
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

STATIC bool
nrf52fw_parse_segment_info_line(const char *p_version_line, NRF52Fw_Segment_t *p_segment)
{
    bool result = false;
    do
    {
        const char *   p_token_start = p_version_line;
        char *         end           = NULL;
        const uint32_t segment_addr  = (uint32_t)strtoul(p_token_start, &end, 16);
        if ((' ' != *end) && ('\t' != *end))
        {
            break;
        }
        while ((' ' == *end) || ('\t' == *end))
        {
            end += 1;
        }
        p_token_start              = end;
        end                        = NULL;
        const uint32_t segment_len = (uint32_t)strtoul(p_token_start, &end, 0);
        if ((' ' != *end) && ('\t' != *end))
        {
            break;
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
            break;
        }
        p_token_start              = end;
        end                        = NULL;
        const uint32_t segment_crc = (uint32_t)strtoul(p_token_start, &end, 16);
        if ('\0' != *end)
        {
            break;
        }
        p_segment->address = segment_addr;
        p_segment->size    = segment_len;
        snprintf(
            p_segment->file_name,
            sizeof(p_segment->file_name),
            "%.*s",
            (int)segment_file_name_len,
            p_segment_file_name_begin);
        p_segment->crc = segment_crc;
        result         = true;
    } while (0);
    return result;
}

STATIC int32_t
nrf52fw_parse_info_file(FILE *p_fd, NRF52Fw_Info_t *p_info)
{
    p_info->fw_ver.version = 0;
    p_info->num_segments   = 0;

    char    line_buf[80];
    int32_t err_line_num = 0;
    int32_t line_cnt     = 0;
    while ((0 == err_line_num) && (fgets(line_buf, sizeof(line_buf), p_fd) != NULL))
    {
        line_cnt += 1;
        nrf52fw_line_rstrip(line_buf, sizeof(line_buf));
        if (1 == line_cnt)
        {
            if (!nrf52fw_parse_version_line(line_buf, &p_info->fw_ver))
            {
                err_line_num = line_cnt;
            }
        }
        else
        {
            if (p_info->num_segments >= sizeof(p_info->segments) / sizeof(p_info->segments[0]))
            {
                err_line_num = line_cnt;
            }
            else if (!nrf52fw_parse_segment_info_line(line_buf, &p_info->segments[p_info->num_segments]))
            {
                err_line_num = line_cnt;
            }
            else
            {
                p_info->num_segments += 1;
            }
        }
    }
    return err_line_num;
}

STATIC bool
nrf52fw_read_info_txt(FlashFatFs_t *p_ffs, const char *p_path_info_txt, NRF52Fw_Info_t *p_info)
{
    bool result = false;
    memset(p_info, 0, sizeof(*p_info));
    const bool flag_use_binary_mode = false;
    FILE *     p_fd                 = flashfatfs_fopen(p_ffs, p_path_info_txt, flag_use_binary_mode);
    if (NULL == p_fd)
    {
        ESP_LOGE(TAG, "%s: Can't open: %s", __func__, p_path_info_txt);
    }
    else
    {
        const int32_t err_line_num = nrf52fw_parse_info_file(p_fd, p_info);
        fclose(p_fd);
        if (0 != err_line_num)
        {
            ESP_LOGE(TAG, "%s: Failed to parse '%s' at line %d", __func__, p_path_info_txt, err_line_num);
        }
        else
        {
            result = true;
        }
    }
    return result;
}

STATIC bool
nrf52fw_init_swd(void)
{
    ESP_LOGI(TAG, "Init SWD");
    if (!nrf52swd_init())
    {
        ESP_LOGE(TAG, "nrf52swd_init failed");
        return false;
    }
    if (!nrf52swd_check_id_code())
    {
        ESP_LOGE(TAG, "nrf52swd_check_id_code failed");
        return false;
    }
    if (!nrf52swd_debug_halt())
    {
        ESP_LOGE(TAG, "nrf52swd_debug_halt failed");
        return false;
    }
    return true;
}

STATIC void
nrf52fw_deinit_swd(void)
{
    ESP_LOGI(TAG, "Deinit SWD");
    nrf52swd_deinit();
}

STATIC bool
nrf52fw_read_current_fw_ver(uint32_t *p_fw_ver)
{
    return nrf52swd_read_mem(NRF52FW_IUCR_FW_VER, 1, p_fw_ver);
}

STATIC bool
nrf52fw_write_current_fw_ver(const uint32_t fw_ver)
{
    return nrf52swd_write_mem(NRF52FW_IUCR_FW_VER, 1, &fw_ver);
}

#if RUUVI_TESTS_NRF52FW

static bool g_nrf52fw_flag_simulate_file_read_error;

STATIC void
nrf52fw_simulate_file_read_error(const bool flag_error)
{
    g_nrf52fw_flag_simulate_file_read_error = flag_error;
}
#endif

STATIC int32_t
nrf52fw_file_read(const FileDescriptor_t fd, void *p_buf, const size_t buf_size)
{
#if RUUVI_TESTS_NRF52FW
    if (g_nrf52fw_flag_simulate_file_read_error)
    {
        return -1;
    }
#endif
    return read(fd, p_buf, buf_size);
}

STATIC bool
nrf52fw_flash_write_segment(
    const FileDescriptor_t fd,
    NRF52Fw_TmpBuf_t *     p_tmp_buf,
    const uint32_t         segment_addr,
    const size_t           segment_len)
{
    bool     result    = false;
    uint32_t offset    = 0;
    bool     flag_stop = false;
    while (!flag_stop)
    {
        const uint32_t addr = segment_addr + offset;
        int32_t        len  = nrf52fw_file_read(fd, p_tmp_buf->buf_wr, sizeof(p_tmp_buf->buf_wr));
        if (len < 0)
        {
            ESP_LOGE(TAG, "%s: %s failed", __func__, "nrf52fw_file_read");
            flag_stop = true;
        }
        else if (0 == len)
        {
            result    = true;
            flag_stop = true;
        }
        else if (0 != (len % sizeof(uint32_t)))
        {
            ESP_LOGE(TAG, "%s: bad len %d", __func__, len);
            flag_stop = true;
        }
        else
        {
            offset += len;
            if (offset > segment_len)
            {
                ESP_LOGE(TAG, "%s: offset %u greater than segment len %u", __func__, offset, segment_len);
                flag_stop = true;
            }
            if (!flag_stop)
            {
                ESP_LOGI(TAG, "Writing 0x%08x...", addr);
                if (!nrf52swd_write_mem(addr, len / sizeof(uint32_t), p_tmp_buf->buf_wr))
                {
                    ESP_LOGE(TAG, "%s: %s failed", __func__, "nrf52swd_write_mem");
                    flag_stop = true;
                }
            }
#if NRF52FW_ENABLE_FLASH_VERIFICATION
            if (!flag_stop)
            {
                if (!nrf52swd_read_mem(addr, len / sizeof(uint32_t), p_tmp_buf->buf_rd))
                {
                    ESP_LOGE(TAG, "%s: %s failed", __func__, "nrf52swd_read_mem");
                    flag_stop = true;
                }
            }
            if (!flag_stop)
            {
                if (0 != memcmp(p_tmp_buf->buf_wr, p_tmp_buf->buf_rd, len))
                {
                    ESP_LOGE(TAG, "%s: %s failed", __func__, "verify");
                    flag_stop = true;
                }
            }
#endif
        }
    }
    return result;
}

STATIC bool
nrf52fw_write_segment_from_file(
    FlashFatFs_t *    p_ffs,
    const char *      p_path,
    NRF52Fw_TmpBuf_t *p_tmp_buf,
    const uint32_t    segment_addr,
    const size_t      segment_len)
{
    bool                   result = false;
    const FileDescriptor_t fd     = flashfatfs_open(p_ffs, p_path);
    if (fd < 0)
    {
        ESP_LOGE(TAG, "%s: Can't open '%s'", __func__, p_path);
    }
    else
    {
        const bool res = nrf52fw_flash_write_segment(fd, p_tmp_buf, segment_addr, segment_len);
        close(fd);
        if (!res)
        {
            ESP_LOGE(TAG, "%s: Failed to write segment 0x%08x from '%s'", __func__, segment_addr, p_path);
        }
        else
        {
            result = true;
        }
    }
    return result;
}

STATIC bool
nrf52fw_erase_flash(void)
{
    bool result = false;
    ESP_LOGI(TAG, "Erasing flash memory...");
    if (!nrf52swd_erase_all())
    {
        ESP_LOGE(TAG, "%s: %s failed", __func__, "nrf52swd_erase_all");
    }
    else
    {
        result = true;
    }
    return result;
}

STATIC bool
nrf52fw_flash_write_firmware(FlashFatFs_t *p_ffs, NRF52Fw_TmpBuf_t *p_tmp_buf, const NRF52Fw_Info_t *p_fw_info)
{
    bool result = nrf52fw_erase_flash();
    if (!result)
    {
        ESP_LOGE(TAG, "%s: %s failed", __func__, "nrf52fw_erase_flash");
    }
    else
    {
        ESP_LOGI(TAG, "Flash %u segments", p_fw_info->num_segments);
        for (uint32_t i = 0; result && (i < p_fw_info->num_segments); ++i)
        {
            const NRF52Fw_Segment_t *p_segment_info = &p_fw_info->segments[i];
            ESP_LOGI(
                TAG,
                "Flash segment %u: 0x%08x size=%u from %s",
                i,
                p_segment_info->address,
                p_segment_info->size,
                p_segment_info->file_name);
            result = nrf52fw_write_segment_from_file(
                p_ffs,
                p_segment_info->file_name,
                p_tmp_buf,
                p_segment_info->address,
                p_segment_info->size);
            if (!result)
            {
                ESP_LOGE(
                    TAG,
                    "%s: Failed to write segment %u: 0x%08x from %s",
                    __func__,
                    i,
                    p_segment_info->address,
                    p_segment_info->file_name);
            }
        }
        if (result)
        {
            result = nrf52fw_write_current_fw_ver(p_fw_info->fw_ver.version);
            if (!result)
            {
                ESP_LOGE(TAG, "%s: Failed to write firmware version", __func__);
            }
        }
    }
    return result;
}

STATIC bool
nrf52fw_calc_segment_crc(
    const FileDescriptor_t fd,
    NRF52Fw_TmpBuf_t *     p_tmp_buf,
    const size_t           segment_len,
    uint32_t *             p_crc)
{
    bool     result     = false;
    uint32_t offset     = 0U;
    uint32_t actual_crc = 0U;
    bool     flag_stop  = false;
    while (!flag_stop)
    {
        const int32_t len = nrf52fw_file_read(fd, p_tmp_buf->buf_wr, sizeof(p_tmp_buf->buf_wr));
        if (len < 0)
        {
            ESP_LOGE(TAG, "%s: %s failed", __func__, "nrf52fw_file_read");
            flag_stop = true;
        }
        else if (0 == len)
        {
            result    = true;
            flag_stop = true;
        }
        else if (0 != (len % sizeof(uint32_t)))
        {
            ESP_LOGE(TAG, "%s: bad len %d", __func__, len);
            flag_stop = true;
        }
        else
        {
            offset += len;
            if (offset > segment_len)
            {
                ESP_LOGE(TAG, "%s: offset %u greater than segment len %u", __func__, offset, segment_len);
                flag_stop = true;
            }
            else
            {
                actual_crc = crc32_le(actual_crc, (void *)p_tmp_buf->buf_wr, len);
            }
        }
    }
    *p_crc = actual_crc;
    return result;
}

STATIC bool
nrf52fw_check_fw_segment_crc(FlashFatFs_t *p_ffs, NRF52Fw_TmpBuf_t *p_tmp_buf, const NRF52Fw_Segment_t *p_segment_info)
{
    bool                   result = false;
    const FileDescriptor_t fd     = flashfatfs_open(p_ffs, p_segment_info->file_name);
    if (fd < 0)
    {
        ESP_LOGE(TAG, "%s: Can't open '%s'", __func__, p_segment_info->file_name);
    }
    else
    {
        uint32_t   crc = 0;
        const bool res = nrf52fw_calc_segment_crc(fd, p_tmp_buf, p_segment_info->size, &crc);
        close(fd);

        if (!res)
        {
            ESP_LOGE(TAG, "%s: %s failed", __func__, "nrf52fw_calc_segment_crc");
        }
        else
        {
            if (p_segment_info->crc != crc)
            {
                ESP_LOGE(
                    TAG,
                    "%s: Segment: 0x%08x: expected CRC: 0x%08x, actual CRC: 0x%08x",
                    __func__,
                    p_segment_info->address,
                    p_segment_info->crc,
                    crc);
            }
            else
            {
                result = true;
            }
        }
    }
    return result;
}

STATIC bool
nrf52fw_check_firmware(FlashFatFs_t *p_ffs, NRF52Fw_TmpBuf_t *p_tmp_buf, const NRF52Fw_Info_t *p_fw_info)
{
    bool result = true;
    for (uint32_t i = 0; result && (i < p_fw_info->num_segments); ++i)
    {
        result = nrf52fw_check_fw_segment_crc(p_ffs, p_tmp_buf, &p_fw_info->segments[i]);
    }
    return result;
}

STATIC bool
nrf52fw_update_fw_step3(FlashFatFs_t *p_ffs, NRF52Fw_TmpBuf_t *p_tmp_buf)
{
    bool           result  = false;
    NRF52Fw_Info_t fw_info = { 0 };
    do
    {
        if (!nrf52fw_read_info_txt(p_ffs, "info.txt", &fw_info))
        {
            ESP_LOGE(TAG, "%s: %s failed", __func__, "nrf52fw_read_info_txt");
            break;
        }
        ESP_LOGI(
            TAG,
            "Firmware on FatFS: v%u.%u.%u",
            (unsigned)((fw_info.fw_ver.version >> 24U) & 0xFFU),
            (unsigned)((fw_info.fw_ver.version >> 16U) & 0xFFU),
            (unsigned)((fw_info.fw_ver.version >> 8U) & 0xFFU));

        uint32_t cur_fw_ver = 0;
        if (!nrf52fw_read_current_fw_ver(&cur_fw_ver))
        {
            ESP_LOGE(TAG, "%s: %s failed", __func__, "nrf52fw_read_current_fw_ver");
            break;
        }
        ESP_LOGI(
            TAG,
            "Firmware on nRF52: v%u.%u.%u",
            (unsigned)((cur_fw_ver >> 24U) & 0xFFU),
            (unsigned)((cur_fw_ver >> 16U) & 0xFFU),
            (unsigned)((cur_fw_ver >> 8U) & 0xFFU));
        if (cur_fw_ver == fw_info.fw_ver.version)
        {
            ESP_LOGI(TAG, "Firmware updating is not needed");
            result = true;
            break;
        }
        ESP_LOGI(TAG, "Need to update firmware on nRF52");
        if (!nrf52fw_check_firmware(p_ffs, p_tmp_buf, &fw_info))
        {
            ESP_LOGE(TAG, "%s: %s failed", __func__, "nrf52fw_check_firmware");
            break;
        }
        if (!nrf52fw_flash_write_firmware(p_ffs, p_tmp_buf, &fw_info))
        {
            ESP_LOGE(TAG, "%s: %s failed", __func__, "nrf52fw_flash_write_firmware");
            break;
        }
        result = true;
    } while (0);
    return result;
}

STATIC bool
nrf52fw_update_fw_step2(FlashFatFs_t *p_ffs)
{
    bool              result    = false;
    NRF52Fw_TmpBuf_t *p_tmp_buf = app_malloc(sizeof(*p_tmp_buf));
    if (NULL == p_tmp_buf)
    {
        ESP_LOGE(TAG, "%s: %s failed", __func__, "app_malloc");
    }
    else
    {
        result = nrf52fw_update_fw_step3(p_ffs, p_tmp_buf);
        app_free(p_tmp_buf);
    }
    return result;
}

STATIC bool
nrf52fw_update_fw_step1(void)
{
    bool                       result        = false;
    const FlashFatFsNumFiles_t max_num_files = 1;
    FlashFatFs_t *             p_ffs         = flashfatfs_mount("/fs_nrf52", GW_NRF_PARTITION, max_num_files);
    if (NULL == p_ffs)
    {
        ESP_LOGE(TAG, "%s: %s failed", __func__, "flashfatfs_mount");
    }
    else
    {
        result = nrf52fw_update_fw_step2(p_ffs);
        flashfatfs_unmount(p_ffs);
    }
    return result;
}

STATIC bool
nrf52fw_update_fw_step0(void)
{
    bool result = false;
    do
    {
        if (!nrf52fw_init_swd())
        {
            ESP_LOGE(TAG, "%s: %s failed", __func__, "nrf52fw_init_swd");
            break;
        }

        result = nrf52fw_update_fw_step1();

        if (result)
        {
            result = nrf52swd_debug_run();
            if (!result)
            {
                ESP_LOGE(TAG, "%s: %s failed", __func__, "nrf52swd_debug_run");
            }
        }
        nrf52fw_deinit_swd();
    } while (0);
    return result;
}

STATIC void
nrf52fw_hw_reset_nrf52(const bool flag_reset)
{
    ESP_LOGI(TAG, "Hardware reset nRF52: %s", flag_reset ? "true" : "false");
    nrf52swd_reset(flag_reset);
}

bool
nrf52fw_update_fw_if_necessary(void)
{
    const TickType_t ticks_in_reset_state = 100;
    nrf52fw_hw_reset_nrf52(true);
    vTaskDelay(ticks_in_reset_state);
    nrf52fw_hw_reset_nrf52(false);

    const bool res = nrf52fw_update_fw_step0();

    nrf52fw_hw_reset_nrf52(true);
    vTaskDelay(ticks_in_reset_state);
    nrf52fw_hw_reset_nrf52(false);

    return res;
}
