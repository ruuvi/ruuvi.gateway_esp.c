/**
 * @file adv_post_statistics.h
 * @author TheSomeMan
 * @date 2023-09-04
 * @copyright Ruuvi Innovations Ltd, license BSD-3-Clause.
 */

#ifndef RUUVI_GATEWAY_ESP_ADV_POST_STATISTICS_H
#define RUUVI_GATEWAY_ESP_ADV_POST_STATISTICS_H

#include <stdbool.h>
#include "http_json.h"
#include "os_str.h"

#ifdef __cplusplus
extern "C" {
#endif

void
adv_post_statistics_init(void);

bool
adv_post_statistics_do_send(void);

http_json_statistics_info_t*
adv_post_statistics_info_generate(const str_buf_t* const p_reset_info);

#ifdef __cplusplus
}
#endif

#endif // RUUVI_GATEWAY_ESP_ADV_POST_STATISTICS_H
