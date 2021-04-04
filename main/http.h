/**
 * @file http.h
 * @author Jukka Saari
 * @date 2019-11-27
 * @copyright Ruuvi Innovations Ltd, license BSD-3-Clause.
 */

#ifndef RUUVI_HTTP_H
#define RUUVI_HTTP_H

#include <stdbool.h>
#include "adv_table.h"

#ifdef __cplusplus
extern "C" {
#endif

bool
http_send(const char *const p_msg);

void
http_send_advs(const adv_report_table_t *const p_reports);

#ifdef __cplusplus
}
#endif

#endif // RUUVI_HTTP_H
