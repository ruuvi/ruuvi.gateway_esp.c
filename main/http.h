/**
 * @file http.h
 * @author Jukka Saari
 * @date 2019-11-27
 * @copyright Ruuvi Innovations Ltd, license BSD-3-Clause.
 */

#ifndef RUUVI_HTTP_H
#define RUUVI_HTTP_H

#include <stdbool.h>
#include "esp_http_client.h"
#include "adv_table.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*http_download_cb_on_data_t)(
    const uint8_t *const p_buf,
    const size_t         buf_size,
    const size_t         offset,
    const size_t         content_length,
    void *               p_user_data);

bool
http_send(const char *const p_msg);

void
http_send_advs(const adv_report_table_t *const p_reports, const uint32_t nonce);

bool
http_download(const char *const p_url, http_download_cb_on_data_t cb_on_data, void *const p_user_data);

#ifdef __cplusplus
}
#endif

#endif // RUUVI_HTTP_H
