/**
 * @copyright Ruuvi Innovations Ltd, license BSD-3-Clause.
 */

#include "nrf52fw_info_txt.h"
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>
#include "flashfatfs.h"
#include "os_str.h"

#define LOG_LOCAL_LEVEL LOG_LEVEL_INFO
#include "log.h"

static const char TAG[] = "nRF52Fw";

NRF52FW_STATIC
bool
nrf52fw_parse_version_digit(const char* p_digit_str, const char* p_digit_str_end, uint8_t* p_digit)
{
    const char*    end = p_digit_str_end;
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
    const char*   p_token_begin,
    const char*   p_token_end,
    uint32_t*     p_version,
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
nrf52fw_parse_version(const char* p_version_str, ruuvi_nrf52_fw_ver_t* const p_version)
{
    uint32_t version = 0;

    if (NULL == p_version_str)
    {
        return false;
    }

    const char* p_token_begin = p_version_str;
    if ('\0' == *p_token_begin)
    {
        return false;
    }
    const char* p_token_end = strchr(p_token_begin, '.');
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
nrf52fw_parse_version_line(const char* p_version_line, ruuvi_nrf52_fw_ver_t* const p_version)
{
    const char*  version_prefix     = "# v";
    const size_t version_prefix_len = strlen(version_prefix);
    if (0 != strncmp(p_version_line, version_prefix, version_prefix_len))
    {
        return false;
    }
    return nrf52fw_parse_version(&p_version_line[version_prefix_len], p_version);
}

NRF52FW_STATIC
void
nrf52fw_line_rstrip(char* p_line_buf, const size_t line_buf_size)
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
nrf52fw_parse_segment_info_line(const char* p_version_line, nrf52fw_segment_t* p_segment)
{
    const char*    p_token_start = p_version_line;
    const char*    end           = NULL;
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
    const char* p_segment_file_name_begin = p_token_start;
    while ((' ' != *end) && ('\t' != *end))
    {
        end += 1;
    }
    const char*  p_segment_file_name_end = end;
    const size_t segment_file_name_len   = (size_t)(p_segment_file_name_end - p_segment_file_name_begin);
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
nrf52fw_parse_info_file(FILE* p_fd, nrf52fw_info_t* p_info)
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

bool
nrf52fw_info_txt_read(const flash_fat_fs_t* p_ffs, const char* const p_path_info_txt, nrf52fw_info_t* const p_info)
{
    memset(p_info, 0, sizeof(*p_info));
    const bool flag_use_binary_mode = false;

    FILE* p_fd = flashfatfs_fopen(p_ffs, p_path_info_txt, flag_use_binary_mode);
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
