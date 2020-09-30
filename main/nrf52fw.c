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
    char *end = (char *)p_digit_str_end;

    const unsigned long val = strtoul(p_digit_str, &end, 10);
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
    if (val > 255)
    {
        return false;
    }
    *p_digit = (uint8_t)val;
    return true;
}

STATIC bool
nrf52fw_parse_version(const char *p_version_str, NRF52Fw_Version_t *p_version)
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
    {
        uint8_t digit_val = 0;
        if (!nrf52fw_parse_version_digit(p_token_begin, p_token_end, &digit_val))
        {
            return false;
        }
        version |= (uint32_t)digit_val << 24U;
    }

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
    {
        uint8_t digit_val = 0;
        if (!nrf52fw_parse_version_digit(p_token_begin, p_token_end, &digit_val))
        {
            return false;
        }
        version |= (uint32_t)digit_val << 16U;
    }

    p_token_begin = p_token_end + 1;
    if ('\0' == *p_token_begin)
    {
        return false;
    }
    {
        uint8_t digit_val = 0;
        if (!nrf52fw_parse_version_digit(p_token_begin, NULL, &digit_val))
        {
            return false;
        }
        version |= (uint32_t)digit_val << 8U;
    }
    if (NULL != p_version)
    {
        p_version->version = version;
    }
    return true;
}

STATIC bool
nrf52fw_parse_version_line(const char *p_version_line, NRF52Fw_Version_t *p_version)
{
    const char * version_prefix     = "# v";
    const size_t version_prefix_len = strlen(version_prefix);
    if (0 != strncmp(p_version_line, version_prefix, version_prefix_len))
    {
        return false;
    }
    return nrf52fw_parse_version(&p_version_line[version_prefix_len], p_version);
}

STATIC void
nrf52fw_line_rstrip(char *p_line_buf, const size_t line_buf_size)
{
    p_line_buf[line_buf_size - 1] = '\0';
    size_t len                    = strlen(p_line_buf);
    while (len > 0)
    {
        len -= 1;
        const char last_ch = p_line_buf[len];
        if (('\n' != last_ch) && ('\r' != last_ch) && (' ' != last_ch) && ('\t' != last_ch))
        {
            break;
        }
        p_line_buf[len] = '\0';
    }
}

STATIC bool
nrf52fw_parse_segment_info_line(const char *p_version_line, NRF52Fw_Segment_t *p_segment)
{
    const char *        p_token_start = p_version_line;
    char *              end           = NULL;
    const unsigned long segment_addr  = strtoul(p_token_start, &end, 16);
    if ((' ' != *end) && ('\t' != *end))
    {
        return false;
    }
    while ((' ' == *end) || ('\t' == *end))
    {
        end += 1;
    }
    p_token_start                   = end;
    end                             = NULL;
    const unsigned long segment_len = strtoul(p_token_start, &end, 0);
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
    p_token_start                   = end;
    end                             = NULL;
    const unsigned long segment_crc = strtoul(p_token_start, &end, 16);
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
        (int)segment_file_name_len,
        p_segment_file_name_begin);
    p_segment->crc = segment_crc;
    return true;
}

STATIC int
nrf52fw_parse_info_file(FILE *p_fd, NRF52Fw_Info_t *p_info)
{
    p_info->fw_ver.version = 0;
    p_info->num_segments   = 0;

    char line_buf[80];
    int  line_num = 0;
    while (fgets(line_buf, sizeof(line_buf), p_fd) != NULL)
    {
        line_num += 1;
        nrf52fw_line_rstrip(line_buf, sizeof(line_buf));
        if (1 == line_num)
        {
            if (!nrf52fw_parse_version_line(line_buf, &p_info->fw_ver))
            {
                return line_num;
            }
        }
        else
        {
            if (p_info->num_segments >= sizeof(p_info->segments) / sizeof(p_info->segments[0]))
            {
                return line_num;
            }
            if (!nrf52fw_parse_segment_info_line(line_buf, &p_info->segments[p_info->num_segments]))
            {
                return line_num;
            }
            p_info->num_segments += 1;
        }
    }
    return 0;
}

STATIC bool
nrf52fw_read_info_txt(FlashFatFs_t *p_ffs, const char *p_path_info_txt, NRF52Fw_Info_t *p_info)
{
    memset(p_info, 0, sizeof(*p_info));
    FILE *p_fd = flashfatfs_fopen(p_ffs, p_path_info_txt);
    if (NULL == p_fd)
    {
        ESP_LOGE(TAG, "%s: Can't open: %s", __func__, p_path_info_txt);
        return false;
    }

    const int res = nrf52fw_parse_info_file(p_fd, p_info);
    fclose(p_fd);
    if (0 != res)
    {
        ESP_LOGE(TAG, "%s: Failed to parse '%s' at line %d", __func__, p_path_info_txt, res);
        return false;
    }
    return true;
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
nrf52fw_read_current_fw_version(uint32_t *p_fw_ver)
{
    if (!nrf52swd_read_mem(NRF52FW_IUCR_FW_VER, 1, p_fw_ver))
    {
        return false;
    }
    return true;
}

STATIC bool
nrf52fw_write_current_fw_version(const uint32_t fw_ver)
{
    if (!nrf52swd_write_mem(NRF52FW_IUCR_FW_VER, 1, &fw_ver))
    {
        return false;
    }
    return true;
}

#if RUUVI_TESTS_NRF52FW

static bool g_nrf52fw_flag_simulate_file_read_error;

STATIC void
nrf52fw_simulate_file_read_error(const bool flag_error)
{
    g_nrf52fw_flag_simulate_file_read_error = flag_error;
}
#endif

STATIC int
nrf52fw_file_read(const int fd, void *p_buf, const size_t buf_size)
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
    const int         fd,
    NRF52Fw_TmpBuf_t *p_tmp_buf,
    const uint32_t    segment_addr,
    const size_t      segment_len)
{
    uint32_t offset = 0;
    for (;;)
    {
        const uint32_t addr = segment_addr + offset;
        int            len  = nrf52fw_file_read(fd, p_tmp_buf->buf_wr, sizeof(p_tmp_buf->buf_wr));
        if (len < 0)
        {
            ESP_LOGE(TAG, "%s: %s failed", __func__, "nrf52fw_file_read");
            return false;
        }
        if (0 == len)
        {
            break;
        }
        if (0 != (len % sizeof(uint32_t)))
        {
            ESP_LOGE(TAG, "%s: bad len %d", __func__, len);
            return false;
        }
        offset += len;
        if (offset > segment_len)
        {
            ESP_LOGE(TAG, "%s: offset %u greater than segment len %u", __func__, offset, segment_len);
            return false;
        }
        ESP_LOGI(TAG, "Writing 0x%08x...", addr);
        if (!nrf52swd_write_mem(addr, len / sizeof(uint32_t), p_tmp_buf->buf_wr))
        {
            ESP_LOGE(TAG, "%s: %s failed", __func__, "nrf52swd_write_mem");
            return false;
        }
#if NRF52FW_ENABLE_FLASH_VERIFICATION
        if (!nrf52swd_read_mem(addr, len / sizeof(uint32_t), p_tmp_buf->buf_rd))
        {
            ESP_LOGE(TAG, "%s: %s failed", __func__, "nrf52swd_read_mem");
            return false;
        }
        if (0 != memcmp(p_tmp_buf->buf_wr, p_tmp_buf->buf_rd, len))
        {
            ESP_LOGE(TAG, "%s: %s failed", __func__, "verify");
            return false;
        }
#endif
    }
    return true;
}

STATIC bool
nrf52fw_flash_write_segment_from_file(
    FlashFatFs_t *    p_ffs,
    const char *      p_path,
    NRF52Fw_TmpBuf_t *p_tmp_buf,
    const uint32_t    segment_addr,
    const size_t      segment_len)
{
    const int fd = flashfatfs_open(p_ffs, p_path);
    if (fd < 0)
    {
        ESP_LOGE(TAG, "%s: Can't open '%s'", __func__, p_path);
        return false;
    }

    const bool res = nrf52fw_flash_write_segment(fd, p_tmp_buf, segment_addr, segment_len);
    close(fd);

    if (!res)
    {
        ESP_LOGE(TAG, "%s: Failed to write segment 0x%08x from '%s'", __func__, segment_addr, p_path);
        return false;
    }
    return true;
}

STATIC bool
nrf52fw_erase_flash(void)
{
    ESP_LOGI(TAG, "Erasing flash memory...");
    if (!nrf52swd_erase_all())
    {
        ESP_LOGE(TAG, "%s: %s failed", __func__, "nrf52swd_erase_all");
        return false;
    }
    return true;
}

STATIC bool
nrf52fw_flash_write_firmware(FlashFatFs_t *p_ffs, NRF52Fw_TmpBuf_t *p_tmp_buf, const NRF52Fw_Info_t *p_fw_info)
{
    if (!nrf52fw_erase_flash())
    {
        ESP_LOGE(TAG, "%s: %s failed", __func__, "nrf52fw_erase_flash");
        return false;
    }

    ESP_LOGI(TAG, "Flash %u segments", p_fw_info->num_segments);
    for (unsigned i = 0; i < p_fw_info->num_segments; ++i)
    {
        const NRF52Fw_Segment_t *p_segment_info = &p_fw_info->segments[i];
        ESP_LOGI(
            TAG,
            "Flash segment %u: 0x%08x size=%u from %s",
            i,
            p_segment_info->address,
            p_segment_info->size,
            p_segment_info->file_name);
        if (!nrf52fw_flash_write_segment_from_file(
                p_ffs,
                p_segment_info->file_name,
                p_tmp_buf,
                p_segment_info->address,
                p_segment_info->size))
        {
            ESP_LOGE(
                TAG,
                "%s: Failed to write segment %u: 0x%08x from %s",
                __func__,
                i,
                p_segment_info->address,
                p_segment_info->file_name);
            return false;
        }
    }
    if (!nrf52fw_write_current_fw_version(p_fw_info->fw_ver.version))
    {
        ESP_LOGE(TAG, "%s: Failed to write firmware version", __func__);
        return false;
    }
    return true;
}

STATIC bool
nrf52fw_calc_segment_crc(const int fd, NRF52Fw_TmpBuf_t *p_tmp_buf, const size_t segment_len, uint32_t *p_crc)
{
    uint32_t offset     = 0;
    uint32_t actual_crc = 0U;
    for (;;)
    {
        int len = nrf52fw_file_read(fd, p_tmp_buf->buf_wr, sizeof(p_tmp_buf->buf_wr));
        if (len < 0)
        {
            ESP_LOGE(TAG, "%s: %s failed", __func__, "nrf52fw_file_read");
            return false;
        }
        if (0 == len)
        {
            break;
        }
        if (0 != (len % sizeof(uint32_t)))
        {
            ESP_LOGE(TAG, "%s: bad len %d", __func__, len);
            return false;
        }
        offset += len;
        if (offset > segment_len)
        {
            ESP_LOGE(TAG, "%s: offset %u greater than segment len %u", __func__, offset, segment_len);
            return false;
        }
        actual_crc = crc32_le(actual_crc, (void *)p_tmp_buf->buf_wr, len);
    }
    *p_crc = actual_crc;
    return true;
}

STATIC bool
nrf52fw_check_firmware(FlashFatFs_t *p_ffs, NRF52Fw_TmpBuf_t *p_tmp_buf, const NRF52Fw_Info_t *p_fw_info)
{
    for (unsigned i = 0; i < p_fw_info->num_segments; ++i)
    {
        const NRF52Fw_Segment_t *p_segment_info = &p_fw_info->segments[i];

        const int fd = flashfatfs_open(p_ffs, p_segment_info->file_name);
        if (fd < 0)
        {
            ESP_LOGE(TAG, "%s: Can't open '%s'", __func__, p_segment_info->file_name);
            return false;
        }

        uint32_t   crc = 0;
        const bool res = nrf52fw_calc_segment_crc(fd, p_tmp_buf, p_segment_info->size, &crc);
        close(fd);

        if (!res)
        {
            ESP_LOGE(TAG, "%s: %s failed", __func__, "nrf52fw_calc_segment_crc");
            return false;
        }

        if (p_segment_info->crc != crc)
        {
            ESP_LOGE(TAG, "%s: %s failed", __func__, "nrf52fw_calc_segment_crc");
            ESP_LOGE(
                TAG,
                "%s: Segment %u: 0x%08x: expected CRC: 0x%08x, actual CRC: 0x%08x",
                __func__,
                i,
                p_segment_info->address,
                p_segment_info->crc,
                crc);
            return false;
        }
    }
    return true;
}

STATIC bool
nrf52fw_update_firmware_if_necessary_step3(FlashFatFs_t *p_ffs, NRF52Fw_TmpBuf_t *p_tmp_buf)
{
    NRF52Fw_Info_t fw_info = { 0 };
    if (!nrf52fw_read_info_txt(p_ffs, "info.txt", &fw_info))
    {
        ESP_LOGE(TAG, "%s: %s failed", __func__, "nrf52fw_read_info_txt");
        return false;
    }
    ESP_LOGI(
        TAG,
        "Firmware on FatFS: v%u.%u.%u",
        (unsigned)((fw_info.fw_ver.version >> 24U) & 0xFFU),
        (unsigned)((fw_info.fw_ver.version >> 16U) & 0xFFU),
        (unsigned)((fw_info.fw_ver.version >> 8U) & 0xFFU));

    uint32_t cur_fw_ver = 0;
    if (!nrf52fw_read_current_fw_version(&cur_fw_ver))
    {
        ESP_LOGE(TAG, "%s: %s failed", __func__, "nrf52fw_read_current_fw_version");
        return false;
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
        return true;
    }
    ESP_LOGI(TAG, "Need to update firmware on nRF52");
    if (!nrf52fw_check_firmware(p_ffs, p_tmp_buf, &fw_info))
    {
        ESP_LOGE(TAG, "%s: %s failed", __func__, "nrf52fw_check_firmware");
        return false;
    }
    if (!nrf52fw_flash_write_firmware(p_ffs, p_tmp_buf, &fw_info))
    {
        ESP_LOGE(TAG, "%s: %s failed", __func__, "nrf52fw_flash_write_firmware");
        return false;
    }
    return true;
}

STATIC bool
nrf52fw_update_firmware_if_necessary_step2(FlashFatFs_t *p_ffs)
{
    NRF52Fw_TmpBuf_t *p_tmp_buf = malloc(sizeof(*p_tmp_buf));
    if (NULL == p_tmp_buf)
    {
        ESP_LOGE(TAG, "%s: %s failed", __func__, "malloc");
        return false;
    }

    const bool res = nrf52fw_update_firmware_if_necessary_step3(p_ffs, p_tmp_buf);

    free(p_tmp_buf);
    return res;
}

STATIC bool
nrf52fw_update_firmware_if_necessary_step1(void)
{
    const int     max_num_files = 1;
    FlashFatFs_t *p_ffs         = flashfatfs_mount("/fs_nrf52", GW_NRF_PARTITION, max_num_files);
    if (NULL == p_ffs)
    {
        ESP_LOGE(TAG, "%s: %s failed", __func__, "flashfatfs_mount");
        return false;
    }

    const bool res = nrf52fw_update_firmware_if_necessary_step2(p_ffs);

    flashfatfs_unmount(p_ffs);
    return res;
}

STATIC bool
nrf52fw_update_firmware_if_necessary_step0(void)
{
    if (!nrf52fw_init_swd())
    {
        ESP_LOGE(TAG, "%s: %s failed", __func__, "nrf52fw_init_swd");
        return false;
    }

    bool res = nrf52fw_update_firmware_if_necessary_step1();

    if (res)
    {
        if (!nrf52swd_debug_run())
        {
            res = false;
            ESP_LOGE(TAG, "%s: %s failed", __func__, "nrf52swd_debug_run");
        }
    }
    nrf52fw_deinit_swd();
    return res;
}

STATIC void
nrf52fw_hw_reset_nrf52(const bool flag_reset)
{
    ESP_LOGI(TAG, "Hardware reset nRF52: %s", flag_reset ? "true" : "false");
    nrf52swd_reset(flag_reset);
}

bool
nrf52fw_update_firmware_if_necessary(void)
{
    nrf52fw_hw_reset_nrf52(true);
    vTaskDelay(100);
    nrf52fw_hw_reset_nrf52(false);

    const int res = nrf52fw_update_firmware_if_necessary_step0();

    nrf52fw_hw_reset_nrf52(true);
    vTaskDelay(100);
    nrf52fw_hw_reset_nrf52(false);

    return res;
}
