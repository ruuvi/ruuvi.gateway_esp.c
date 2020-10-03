/**
 * @file app_wrappers.h
 * @author TheSomeMan
 * @date 2020-10-03
 * @copyright Ruuvi Innovations Ltd, license BSD-3-Clause.
 */

#ifndef RUUVI_GATEWAY_ESP_APP_WRAPPERS_H
#define RUUVI_GATEWAY_ESP_APP_WRAPPERS_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef int str2num_base_t;

typedef int app_print_precision_t;

/**
 * @brief This is a wrapper for stdlib strtoul which implements const-correctness and fixes portability issues (MISRA)
 * @param p_str - ptr to a const string
 * @param pp_end - if pp_end is not NULL, it stores the address of the first invalid character in *pp_end
 * @param base - should be in range 2..36 or 0 for auto detection
 * @return the result of the conversion
 */
uint32_t
app_strtoul_cptr(const char *__restrict p_str, const char **__restrict pp_end, const str2num_base_t base);

/**
 * @brief This is a wrapper for stdlib strtoul which implements const-correctness and fixes portability issues (MISRA)
 * @param p_str - ptr to a string
 * @param pp_end - if pp_end is not NULL, it stores the address of the first invalid character in *pp_end
 * @param base - should be in range 2..36 or 0 for auto detection
 * @return the result of the conversion
 */
uint32_t
app_strtoul(char *__restrict p_str, char **__restrict pp_end, const str2num_base_t base);

/**
 * @brief This is a wrapper for stdlib strtol which implements const-correctness and fixes portability issues (MISRA)
 * @param p_str - ptr to a const string
 * @param pp_end - if pp_end is not NULL, it stores the address of the first invalid character in *pp_end
 * @param base - should be in range 2..36 or 0 for auto detection
 * @return the result of the conversion
 */
int32_t
app_strtol_cptr(const char *__restrict p_str, const char **__restrict pp_end, const str2num_base_t base);

/**
 * @brief This is a wrapper for stdlib strtul which implements const-correctness and fixes portability issues (MISRA)
 * @param p_str - ptr to a string
 * @param pp_end - if pp_end is not NULL, it stores the address of the first invalid character in *pp_end
 * @param base - should be in range 2..36 or 0 for auto detection
 * @return the result of the conversion
 */
int32_t
app_strtol(char *__restrict p_str, char **__restrict pp_end, const str2num_base_t base);

#ifdef __cplusplus
}
#endif

#endif // RUUVI_GATEWAY_ESP_APP_WRAPPERS_H
