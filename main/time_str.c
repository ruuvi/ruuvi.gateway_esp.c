/**
 * @file time_str.c
 * @author TheSomeMan
 * @date 2021-07-31
 * @copyright Ruuvi Innovations Ltd, license BSD-3-Clause.
 */

#include "time_str.h"
#include <string.h>
#include <ctype.h>
#include "os_str.h"
#include "os_mkgmtime.h"

#define BASE_10 (10)

static uint32_t
time_str_conv_to_uint32_cptr(const char *const p_str, const char **const pp_end, const os_str2num_base_t base)
{
    if ((NULL != pp_end) && (NULL != *pp_end))
    {
        char         tmp_buf[32];
        const size_t max_len = *pp_end - p_str;
        if (max_len >= sizeof(tmp_buf))
        {
            return UINT32_MAX;
        }
        memcpy(tmp_buf, p_str, max_len);
        tmp_buf[max_len]             = '\0';
        const char *   p_tmp_buf_end = NULL;
        const uint32_t result        = os_str_to_uint32_cptr(tmp_buf, &p_tmp_buf_end, base);
        const size_t   end_offset    = p_tmp_buf_end - &tmp_buf[0];
        *pp_end                      = &p_str[end_offset];
        return result;
    }
    return os_str_to_uint32_cptr(p_str, pp_end, base);
}

static bool
parse_val_of_tm(
    const char **const p_p_cur,
    const char *const  p_end1,
    const char *const  p_end2,
    const uint32_t     min_val,
    const uint32_t     max_val,
    const char         delimiter,
    bool *const        p_res,
    int *const         p_val)
{
    const char *   p_end = (p_end1 < p_end2) ? p_end1 : p_end2;
    const uint32_t val   = time_str_conv_to_uint32_cptr(*p_p_cur, &p_end, BASE_10);
    *p_res = false;
    if ((val < min_val) || (val > max_val))
    {
        return true;
    }
    *p_val = (int)val;
    if ('\0' == *p_end)
    {
        *p_p_cur = p_end;
        *p_res = true;
        return true;
    }
    if ('\0' != delimiter)
    {
        if (0 != isdigit(*p_end))
        {
            *p_p_cur = p_end;
        }
        else if (delimiter == *p_end)
        {
            *p_p_cur = p_end + 1;
        }
        else
        {
            return true;
        }
    }
    else
    {
        *p_p_cur = p_end;
    }
    return false;
}

static bool
parse_ms(const char **const p_p_cur, const char *p_end, uint16_t *const p_ms)
{
    const char* const p_cur = *p_p_cur + 1;
    const uint32_t fract_sec = time_str_conv_to_uint32_cptr(p_cur, &p_end, BASE_10);
    const size_t   len       = p_end - p_cur;
    uint16_t       ms        = 0;
    switch (len)
    {
        case 0:
            ms = 0;
            break;
        case 1:
            ms = (uint16_t)(fract_sec * 100U);
            break;
        case 2:
            ms = (uint16_t)(fract_sec * 10U);
            break;
        case 3:
            ms = (uint16_t)(fract_sec);
            break;
        case 4:
            ms = (uint16_t)(fract_sec / 10U);
            break;
        case 5:
            ms = (uint16_t)(fract_sec / 100U);
            break;
        case 6:
            ms = (uint16_t)(fract_sec / 1000U);
            break;
        default:
            return false;
    }
    if (NULL != p_ms)
    {
        *p_ms = ms;
    }
    *p_p_cur = p_end;
    return true;
}

static bool
parse_tz(const char **const p_p_cur, const char * const p_end, int32_t *const p_tz_offset_seconds)
{
    const char* p_cur = *p_p_cur;
    *p_tz_offset_seconds = 0;

    bool flag_tz_offset_positive = false;
    if ('+' == *p_cur)
    {
        flag_tz_offset_positive = true;
        p_cur += 1;
    }
    else if ('-' == *p_cur)
    {
        flag_tz_offset_positive = false;
        p_cur += 1;
    }
    else
    {
        return false;
    }

    bool res = false;
    int tz_hours = 0;
    int tz_minutes = 0;
    if (parse_val_of_tm(&p_cur, &p_cur[2], p_end, 0, 23, ':', &res, &tz_hours))
    {
        if (!res)
        {
            return false;
        }
    }

    if ('\0' != *p_cur)
    {
        if (!parse_val_of_tm(&p_cur, &p_cur[2], p_end, 0, 59, '\0', &res, &tz_minutes))
        {
            return false;
        }
        if (!res)
        {
            return false;
        }
    }
    if (flag_tz_offset_positive && (tz_hours > 14))
    {
        return false;
    }
    if (!flag_tz_offset_positive && (tz_hours > 12))
    {
        return false;
    }
    if (0 != (tz_minutes % 15))
    {
        return false;
    }
    *p_tz_offset_seconds = (flag_tz_offset_positive ? 1 : -1) * (tz_hours * 60 + tz_minutes) * 60;
    return true;
}

bool
time_str_conv_to_tm(const char *const p_time_str, struct tm *const p_tm_time, uint16_t *const p_ms)
{
    p_tm_time->tm_year  = 0;
    p_tm_time->tm_mon   = 0;
    p_tm_time->tm_mday  = 1;
    p_tm_time->tm_hour  = 0;
    p_tm_time->tm_min   = 0;
    p_tm_time->tm_sec   = 0;
    p_tm_time->tm_isdst = 0;
    p_tm_time->tm_wday  = 0;
    if (NULL != p_ms)
    {
        *p_ms = 0;
    }

    const size_t time_str_len = strlen(p_time_str);
    const char * p_cur        = p_time_str;

    bool res = false;
    if (parse_val_of_tm(&p_cur, &p_cur[4], &p_time_str[time_str_len], 1970U, 2106U, '-', &res, &p_tm_time->tm_year))
    {
        if (res)
        {
            p_tm_time->tm_year -= 1900;
        }
        return res;
    }
    p_tm_time->tm_year -= 1900;

    if (parse_val_of_tm(&p_cur, &p_cur[2], &p_time_str[time_str_len], 1, 12, '-', &res, &p_tm_time->tm_mon))
    {
        if (res)
        {
            p_tm_time->tm_mon -= 1;
        }
        return res;
    }
    p_tm_time->tm_mon -= 1;

    if (parse_val_of_tm(&p_cur, &p_cur[2], &p_time_str[time_str_len], 1, 31, 'T', &res, &p_tm_time->tm_mday))
    {
        return res;
    }

    if (parse_val_of_tm(&p_cur, &p_cur[2], &p_time_str[time_str_len], 0, 23, ':', &res, &p_tm_time->tm_hour))
    {
        return res;
    }

    if (parse_val_of_tm(&p_cur, &p_cur[2], &p_time_str[time_str_len], 0, 59, ':', &res, &p_tm_time->tm_min))
    {
        return res;
    }

    // 60 because of leap seconds: https://en.wikipedia.org/wiki/Leap_second
    if (parse_val_of_tm(&p_cur, &p_cur[2], &p_time_str[time_str_len], 0, 60, '\0', &res, &p_tm_time->tm_sec))
    {
        return res;
    }

    if ('.' == *p_cur)
    {
        if (!parse_ms(&p_cur, &p_time_str[time_str_len], p_ms))
        {
            return false;
        }
    }
    if (('\0' == *p_cur) || (0 == strcmp("Z", p_cur)))
    {
        return true;
    }

    int32_t tz_offset_seconds = 0;
    if (!parse_tz(&p_cur, &p_time_str[time_str_len], &tz_offset_seconds))
    {
        return false;
    }

    if (0 != tz_offset_seconds)
    {
        time_t unix_time = os_mkgmtime(p_tm_time);
        unix_time -= (time_t)tz_offset_seconds;
        gmtime_r(&unix_time, p_tm_time);
        p_tm_time->tm_isdst = 0;
    }
    return true;
}

time_t
time_str_conv_to_unix_time(const char *const p_time_str)
{
    struct tm tm_time = { 0 };
    if (!time_str_conv_to_tm(p_time_str, &tm_time, NULL))
    {
        return (time_t)0;
    }
    return os_mkgmtime(&tm_time);
}
