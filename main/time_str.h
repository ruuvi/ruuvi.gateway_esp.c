/**
 * @file time_str.h
 * @author TheSomeMan
 * @date 2021-07-31
 * @copyright Ruuvi Innovations Ltd, license BSD-3-Clause.
 */

#ifndef RUUVI_GATEWAY_ESP_TIME_STR_H
#define RUUVI_GATEWAY_ESP_TIME_STR_H

#include <stdbool.h>
#include <stdint.h>
#include <time.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Parse a string with timestamp in ISO 8601 standard and return 'struct tm' in UTC.
 * @note ISO 6801 format: YYYY-MM-DDThh:mm:ss.SSSZ
 *      The UTC offset is appended to the time in the same way that 'Z' was above,
 *      in the form ±[hh]:[mm], ±[hh][mm], or ±[hh].
 *      examples:
 *      - 2019-09-26T07:58:30.996+0200
 *      - 2019-09-26T07:58:30-04:00
 *      - 2019-09-26T07:58:30-0400
 *      - 2019-09-26T07:58:30-04
 *      - 2019-09-07T15:50:30+00
 *      - 2019-09-26T07:58:30Z
 *      - 20190907155030.996
 * @param p_time_str - ptr to a string with timestamp.
 * @param[out] p_tm_time - ptr to output variable of type 'struct tm'.
 * @param[out] p_ms - ptr to optional output variable with milli-seconds.
 * @return true if the parsing was successful.
 */
bool
time_str_conv_to_tm(const char* const p_time_str, struct tm* const p_tm_time, uint16_t* const p_ms);

/**
 * @brief Parse a string with timestamp in ISO 8601 standard and return unix-time (time_t).
 * @param p_time_str - ptr to a string with timestamp.
 * @return unix-time or 0 if the parsing was unsuccessful.
 */
time_t
time_str_conv_to_unix_time(const char* const p_time_str);

#ifdef __cplusplus
}
#endif

#endif // RUUVI_GATEWAY_ESP_TIME_STR_H
