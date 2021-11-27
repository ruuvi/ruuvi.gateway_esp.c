/**
 * @file time_str.c
 * @author TheSomeMan
 * @date 2021-07-31
 * @copyright Ruuvi Innovations Ltd, license BSD-3-Clause.
 */

#include "time_str.h"
#include <string.h>
#include <ctype.h>
#include <assert.h>
#include "os_str.h"
#include "os_mkgmtime.h"
#include "time_units.h"

#define BASE_10               (10)
#define TIME_STR_TMP_BUF_SIZE (32)

#define FRACTIONAL_PART_NUM_DIGITS_0            (0U)
#define FRACTIONAL_PART_NUM_DIGITS_1            (1U)
#define FRACTIONAL_PART_NUM_DIGITS_1_MULTIPLIER (100U)
#define FRACTIONAL_PART_NUM_DIGITS_2            (2U)
#define FRACTIONAL_PART_NUM_DIGITS_2_MULTIPLIER (10U)
#define FRACTIONAL_PART_NUM_DIGITS_3            (3U)
#define FRACTIONAL_PART_NUM_DIGITS_3_MULTIPLIER (1U)
#define FRACTIONAL_PART_NUM_DIGITS_4            (4U)
#define FRACTIONAL_PART_NUM_DIGITS_4_DIVIDER    (10U)
#define FRACTIONAL_PART_NUM_DIGITS_5            (5U)
#define FRACTIONAL_PART_NUM_DIGITS_5_DIVIDER    (100U)
#define FRACTIONAL_PART_NUM_DIGITS_6            (6U)
#define FRACTIONAL_PART_NUM_DIGITS_6_DIVIDER    (1000U)

#define TIME_STR_YEAR_MIN      (1970)
#define TIME_STR_YEAR_MAX      (2106)
#define TIME_STR_YEAR_BASE     (1900)
#define TIME_STR_MONTH_MIN     (1)
#define TIME_STR_MONTH_MAX     (12)
#define TIME_STR_MONTH_DAY_MIN (1)
#define TIME_STR_MONTH_DAY_MAX (31)
#define TIME_STR_HOUR_MIN      (0)
#define TIME_STR_HOUR_MAX      (TIME_UNITS_HOURS_PER_DAY - 1)
#define TIME_STR_MINUTE_MIN    (0)
#define TIME_STR_MINUTE_MAX    (TIME_UNITS_MINUTES_PER_HOUR - 1)
#define TIME_STR_SECOND_MIN    (0)
// 60 because of leap seconds: https://en.wikipedia.org/wiki/Leap_second
#define TIME_STR_SECOND_MAX (TIME_UNITS_SECONDS_PER_MINUTE)

#define TIME_STR_YEAR_LEN      (4U)
#define TIME_STR_MONTH_LEN     (2U)
#define TIME_STR_MONTH_DAY_LEN (2U)
#define TIME_STR_HOUR_LEN      (2U)
#define TIME_STR_MINUTE_LEN    (2U)
#define TIME_STR_SECOND_LEN    (2U)

#define TIME_STR_TZ_HOURS_MAX_POSITIVE_OFFSET       (14U)
#define TIME_STR_TZ_HOURS_MAX_NEGATIVE_OFFSET       (12U)
#define TIME_STR_TZ_MINUTES_MUST_BE_MULTIPLE_OF_VAL (15U)

static uint32_t
time_str_conv_to_uint32_cptr(const char *const p_str, const char **const pp_end, const os_str2num_base_t base)
{
    if ((NULL != pp_end) && (NULL != *pp_end))
    {
        const size_t max_len = (size_t)(*pp_end - p_str);
        char         tmp_buf[TIME_STR_TMP_BUF_SIZE];
        if (max_len >= sizeof(tmp_buf))
        {
            return UINT32_MAX;
        }
        memcpy(tmp_buf, p_str, max_len);
        tmp_buf[max_len]             = '\0';
        const char *   p_tmp_buf_end = NULL;
        const uint32_t result        = os_str_to_uint32_cptr(tmp_buf, &p_tmp_buf_end, base);
        const size_t   end_offset    = (size_t)(p_tmp_buf_end - &tmp_buf[0]);
        *pp_end                      = &p_str[end_offset];
        return result;
    }
    return os_str_to_uint32_cptr(p_str, pp_end, base);
}

static bool
parse_val_of_tm(
    const char **const p_p_cur,
    const size_t       val_len,
    const char *const  p_end2,
    const uint32_t     min_val,
    const uint32_t     max_val,
    const char         delimiter,
    int32_t *const     p_val)
{
    assert(min_val <= INT32_MAX);
    assert(max_val <= INT32_MAX);
    const char *const p_end1 = &(*p_p_cur)[val_len];
    const char *      p_end  = (p_end1 < p_end2) ? p_end1 : p_end2;
    const uint32_t    val    = time_str_conv_to_uint32_cptr(*p_p_cur, &p_end, BASE_10);
    if ((val < min_val) || (val > max_val))
    {
        *p_val = INT32_MIN;
        return true;
    }
    *p_val = (int32_t)val;
    if ('\0' == *p_end)
    {
        *p_p_cur = p_end;
        return true;
    }
    if ('\0' != delimiter)
    {
        if (delimiter == *p_end)
        {
            *p_p_cur = p_end + 1;
            return false;
        }
        if (0 != isdigit(*p_end))
        {
            *p_p_cur = p_end;
            return false;
        }
        *p_val = INT32_MIN;
        return true;
    }
    *p_p_cur = p_end;
    return false;
}

static bool
parse_ms(const char **const p_p_cur, const char *p_end, uint16_t *const p_ms)
{
    const char *const p_cur     = *p_p_cur + 1;
    const uint32_t    fract_sec = time_str_conv_to_uint32_cptr(p_cur, &p_end, BASE_10);
    const size_t      len       = (size_t)(p_end - p_cur);
    uint16_t          ms        = 0;
    switch (len)
    {
        case FRACTIONAL_PART_NUM_DIGITS_0:
            ms = 0;
            break;
        case FRACTIONAL_PART_NUM_DIGITS_1:
            ms = (uint16_t)(fract_sec * FRACTIONAL_PART_NUM_DIGITS_1_MULTIPLIER);
            break;
        case FRACTIONAL_PART_NUM_DIGITS_2:
            ms = (uint16_t)(fract_sec * FRACTIONAL_PART_NUM_DIGITS_2_MULTIPLIER);
            break;
        case FRACTIONAL_PART_NUM_DIGITS_3:
            ms = (uint16_t)(fract_sec * FRACTIONAL_PART_NUM_DIGITS_3_MULTIPLIER);
            break;
        case FRACTIONAL_PART_NUM_DIGITS_4:
            ms = (uint16_t)(fract_sec / FRACTIONAL_PART_NUM_DIGITS_4_DIVIDER);
            break;
        case FRACTIONAL_PART_NUM_DIGITS_5:
            ms = (uint16_t)(fract_sec / FRACTIONAL_PART_NUM_DIGITS_5_DIVIDER);
            break;
        case FRACTIONAL_PART_NUM_DIGITS_6:
            ms = (uint16_t)(fract_sec / FRACTIONAL_PART_NUM_DIGITS_6_DIVIDER);
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
parse_tz(const char **const p_p_cur, const char *const p_end, int32_t *const p_tz_offset_seconds)
{
    const char *p_cur    = *p_p_cur;
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

    int32_t tz_hours   = 0;
    int32_t tz_minutes = 0;
    if (parse_val_of_tm(&p_cur, TIME_STR_HOUR_LEN, p_end, 0, TIME_UNITS_HOURS_PER_DAY - 1, ':', &tz_hours)
        && (INT32_MIN == tz_hours))
    {
        return false;
    }

    if ('\0' != *p_cur)
    {
        if (!parse_val_of_tm(&p_cur, TIME_STR_MINUTE_LEN, p_end, 0, TIME_UNITS_MINUTES_PER_HOUR - 1, '\0', &tz_minutes))
        {
            return false;
        }
        if (INT32_MIN == tz_minutes)
        {
            return false;
        }
    }
    if (flag_tz_offset_positive && (tz_hours > TIME_STR_TZ_HOURS_MAX_POSITIVE_OFFSET))
    {
        return false;
    }
    if ((!flag_tz_offset_positive) && (tz_hours > TIME_STR_TZ_HOURS_MAX_NEGATIVE_OFFSET))
    {
        return false;
    }
    if (0 != (tz_minutes % TIME_STR_TZ_MINUTES_MUST_BE_MULTIPLE_OF_VAL))
    {
        return false;
    }
    const int32_t tz_offset_seconds_abs
        = (((tz_hours * (int32_t)TIME_UNITS_MINUTES_PER_HOUR) + tz_minutes) * (int32_t)TIME_UNITS_SECONDS_PER_MINUTE);
    *p_tz_offset_seconds = flag_tz_offset_positive ? tz_offset_seconds_abs : -tz_offset_seconds_abs;
    return true;
}

static bool
time_str_parse_tm_year(
    const char **const p_p_cur,
    const char *const  p_end,
    struct tm *const   p_tm_time,
    bool *const        p_res)
{
    int32_t tm_year = 0;
    if (parse_val_of_tm(p_p_cur, TIME_STR_YEAR_LEN, p_end, TIME_STR_YEAR_MIN, TIME_STR_YEAR_MAX, '-', &tm_year))
    {
        if (INT32_MIN == tm_year)
        {
            *p_res = false;
        }
        else
        {
            p_tm_time->tm_year = tm_year - TIME_STR_YEAR_BASE;
            *p_res             = true;
        }
        return true;
    }
    p_tm_time->tm_year = tm_year - TIME_STR_YEAR_BASE;
    return false;
}

static bool
time_str_parse_tm_mon(
    const char **const p_p_cur,
    const char *const  p_end,
    struct tm *const   p_tm_time,
    bool *const        p_res)
{
    int32_t tm_month = 0;
    if (parse_val_of_tm(p_p_cur, TIME_STR_MONTH_LEN, p_end, TIME_STR_MONTH_MIN, TIME_STR_MONTH_MAX, '-', &tm_month))
    {
        if (INT32_MIN == tm_month)
        {
            *p_res = false;
        }
        else
        {
            p_tm_time->tm_mon = tm_month - 1;
            *p_res            = true;
        }
        return true;
    }
    p_tm_time->tm_mon = tm_month - 1;
    return false;
}

static bool
time_str_parse_tm_mday(
    const char **const p_p_cur,
    const char *const  p_end,
    struct tm *const   p_tm_time,
    bool *const        p_res)
{
    int32_t tm_mday = 0;
    if (parse_val_of_tm(
            p_p_cur,
            TIME_STR_MONTH_DAY_LEN,
            p_end,
            TIME_STR_MONTH_DAY_MIN,
            TIME_STR_MONTH_DAY_MAX,
            'T',
            &tm_mday))
    {
        if (INT32_MIN == tm_mday)
        {
            *p_res = false;
        }
        else
        {
            p_tm_time->tm_mday = tm_mday;
            *p_res             = true;
        }
        return true;
    }
    p_tm_time->tm_mday = tm_mday;
    return false;
}

static bool
time_str_parse_tm_hour(
    const char **const p_p_cur,
    const char *const  p_end,
    struct tm *const   p_tm_time,
    bool *const        p_res)
{
    int32_t tm_hour = 0;
    if (parse_val_of_tm(p_p_cur, TIME_STR_HOUR_LEN, p_end, TIME_STR_HOUR_MIN, TIME_STR_HOUR_MAX, ':', &tm_hour))
    {
        if (INT32_MIN == tm_hour)
        {
            *p_res = false;
        }
        else
        {
            p_tm_time->tm_hour = tm_hour;
            *p_res             = true;
        }
        return true;
    }
    p_tm_time->tm_hour = tm_hour;
    return false;
}

static bool
time_str_parse_tm_min(
    const char **const p_p_cur,
    const char *const  p_end,
    struct tm *const   p_tm_time,
    bool *const        p_res)
{
    int32_t tm_min = 0;
    if (parse_val_of_tm(p_p_cur, TIME_STR_MINUTE_LEN, p_end, TIME_STR_MINUTE_MIN, TIME_STR_MINUTE_MAX, ':', &tm_min))
    {
        if (INT32_MIN == tm_min)
        {
            *p_res = false;
        }
        else
        {
            p_tm_time->tm_min = tm_min;
            *p_res            = true;
        }
        return true;
    }
    p_tm_time->tm_min = tm_min;
    return false;
}

static bool
time_str_parse_tm_sec(
    const char **const p_p_cur,
    const char *const  p_end,
    struct tm *const   p_tm_time,
    bool *const        p_res)
{
    int32_t tm_sec = 0;
    if (parse_val_of_tm(p_p_cur, TIME_STR_SECOND_LEN, p_end, TIME_STR_SECOND_MIN, TIME_STR_SECOND_MAX, '\0', &tm_sec))
    {
        if (INT32_MIN == tm_sec)
        {
            *p_res = false;
        }
        else
        {
            p_tm_time->tm_sec = tm_sec;
            *p_res            = true;
        }
        return true;
    }
    p_tm_time->tm_sec = tm_sec;
    return false;
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

    const char *const p_end = &p_time_str[strlen(p_time_str)];
    const char *      p_cur = p_time_str;

    bool res = false;

    if (time_str_parse_tm_year(&p_cur, p_end, p_tm_time, &res))
    {
        return res;
    }
    if (time_str_parse_tm_mon(&p_cur, p_end, p_tm_time, &res))
    {
        return res;
    }
    if (time_str_parse_tm_mday(&p_cur, p_end, p_tm_time, &res))
    {
        return res;
    }
    if (time_str_parse_tm_hour(&p_cur, p_end, p_tm_time, &res))
    {
        return res;
    }
    if (time_str_parse_tm_min(&p_cur, p_end, p_tm_time, &res))
    {
        return res;
    }
    if (time_str_parse_tm_sec(&p_cur, p_end, p_tm_time, &res))
    {
        return res;
    }

    if (('.' == *p_cur) && (!parse_ms(&p_cur, p_end, p_ms)))
    {
        return false;
    }
    if (('\0' == *p_cur) || (0 == strcmp("Z", p_cur)))
    {
        return true;
    }

    int32_t tz_offset_seconds = 0;
    if (!parse_tz(&p_cur, p_end, &tz_offset_seconds))
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
