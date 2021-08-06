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

    const size_t time_str_len = strlen(p_time_str);
    const char * p_cur        = p_time_str;

    {
        const char *   p_end = (&p_cur[4] < &p_time_str[time_str_len]) ? &p_cur[4] : &p_time_str[time_str_len];
        const uint32_t year  = time_str_conv_to_uint32_cptr(p_cur, &p_end, 10);
        if ((year < 1900) || (year > 2100))
        {
            return false;
        }
        p_tm_time->tm_year = (int)(year - 1900);
        if ('\0' == *p_end)
        {
            return true;
        }
        if (0 != isdigit(*p_end))
        {
            p_cur = p_end;
        }
        else if ('-' == *p_end)
        {
            p_cur = p_end + 1;
        }
        else
        {
            return false;
        }
    }

    {
        const char *   p_end = (&p_cur[2] < &p_time_str[time_str_len]) ? &p_cur[2] : &p_time_str[time_str_len];
        const uint32_t month = time_str_conv_to_uint32_cptr(p_cur, &p_end, 10);
        if ((month < 1) || (month > 12))
        {
            return false;
        }
        p_tm_time->tm_mon = (int)(month - 1);
        if ('\0' == *p_end)
        {
            return true;
        }
        if (0 != isdigit(*p_end))
        {
            p_cur = p_end;
        }
        else if ('-' == *p_end)
        {
            p_cur = p_end + 1;
        }
        else
        {
            return false;
        }
    }

    {
        const char *   p_end = (&p_cur[2] < &p_time_str[time_str_len]) ? &p_cur[2] : &p_time_str[time_str_len];
        const uint32_t day   = time_str_conv_to_uint32_cptr(p_cur, &p_end, 10);
        if ((day < 1) || (day > 31))
        {
            return false;
        }
        p_tm_time->tm_mday = (int)day;
        if ('\0' == *p_end)
        {
            return true;
        }
        if (0 != isdigit(*p_end))
        {
            p_cur = p_end;
        }
        else if ('T' == *p_end)
        {
            p_cur = p_end + 1;
        }
        else
        {
            return false;
        }
    }

    {
        const char *   p_end = (&p_cur[2] < &p_time_str[time_str_len]) ? &p_cur[2] : &p_time_str[time_str_len];
        const uint32_t hour  = time_str_conv_to_uint32_cptr(p_cur, &p_end, 10);
        if (hour > 23)
        {
            return false;
        }
        p_tm_time->tm_hour = (int)hour;
        if ('\0' == *p_end)
        {
            return true;
        }
        if (0 != isdigit(*p_end))
        {
            p_cur = p_end;
        }
        else if (':' == *p_end)
        {
            p_cur = p_end + 1;
        }
        else
        {
            return false;
        }
    }

    {
        const char *   p_end  = (&p_cur[2] < &p_time_str[time_str_len]) ? &p_cur[2] : &p_time_str[time_str_len];
        const uint32_t minute = time_str_conv_to_uint32_cptr(p_cur, &p_end, 10);
        if (minute > 59)
        {
            return false;
        }
        p_tm_time->tm_min = (int)minute;
        if ('\0' == *p_end)
        {
            return true;
        }
        if (0 != isdigit(*p_end))
        {
            p_cur = p_end;
        }
        else if (':' == *p_end)
        {
            p_cur = p_end + 1;
        }
        else
        {
            return false;
        }
    }

    {
        const char *   p_end  = (&p_cur[2] < &p_time_str[time_str_len]) ? &p_cur[2] : &p_time_str[time_str_len];
        const uint32_t second = time_str_conv_to_uint32_cptr(p_cur, &p_end, 10);

        if (second > 60) // 60 because of leap seconds: https://en.wikipedia.org/wiki/Leap_second
        {
            return false;
        }
        p_tm_time->tm_sec = (int)second;
        p_cur             = p_end;
    }
    if ('.' == *p_cur)
    {
        p_cur += 1;
        const char *   p_end     = &p_time_str[time_str_len];
        const uint32_t fract_sec = time_str_conv_to_uint32_cptr(p_cur, &p_end, 10);
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
        p_cur = p_end;
    }
    if (('\0' == *p_cur) || (0 == strcmp("Z", p_cur)))
    {
        return true;
    }
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

    uint8_t tz_hours = 0;
    uint8_t tz_min   = 0;
    {
        const char *   p_end        = (&p_cur[2] < &p_time_str[time_str_len]) ? &p_cur[2] : &p_time_str[time_str_len];
        const uint32_t tz_hours_tmp = time_str_conv_to_uint32_cptr(p_cur, &p_end, 10);

        if (flag_tz_offset_positive && (tz_hours_tmp > 14))
        {
            return false;
        }
        if (!flag_tz_offset_positive && (tz_hours_tmp > 12))
        {
            return false;
        }
        tz_hours = (uint8_t)tz_hours_tmp;
        if ('\0' != *p_end)
        {
            if (':' == *p_end)
            {
                p_end += 1;
            }
            else if (0 == isdigit(*p_end))
            {
                return false;
            }
            else
            {
                // MISRA C:2012, 15.7 - All if...else if constructs shall be terminated with an else statement
            }
        }
        p_cur = p_end;
    }

    if ('\0' != *p_cur)
    {
        const char *   p_end      = &p_time_str[time_str_len];
        const uint32_t tz_min_tmp = time_str_conv_to_uint32_cptr(p_cur, &p_end, 10);

        if (tz_min_tmp >= 60)
        {
            return false;
        }
        if ('\0' != *p_end)
        {
            return false;
        }
        tz_min = tz_min_tmp;
    }
    if ((0 != tz_hours) || (0 != tz_min))
    {
        time_t unix_time = os_mkgmtime(p_tm_time);
        if (flag_tz_offset_positive)
        {
            unix_time -= (time_t)((uint32_t)((uint16_t)tz_hours * 60U + tz_min) * 60U);
        }
        else
        {
            unix_time += (time_t)((uint32_t)((uint16_t)tz_hours * 60U + tz_min) * 60U);
        }
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
