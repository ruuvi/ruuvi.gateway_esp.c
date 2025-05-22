/**
 * @file http_json.h
 * @author TheSomeMan
 * @date 2021-02-03
 * @copyright Ruuvi Innovations Ltd, license BSD-3-Clause.
 */

#ifndef RUUVI_GATEWAY_HTTP_JSON_H
#define RUUVI_GATEWAY_HTTP_JSON_H

#include <stdbool.h>
#include <time.h>
#include "adv_table.h"
#include "cjson_wrap.h"
#include "fw_update.h"
#include "nrf52fw.h"

#ifdef __cplusplus
extern "C" {
#endif

#define HTTP_JSON_STATISTICS_RESET_REASON_MAX_LEN (24)

typedef struct http_json_statistics_reset_reason_buf_t
{
    char buf[HTTP_JSON_STATISTICS_RESET_REASON_MAX_LEN];
} http_json_statistics_reset_reason_buf_t;

typedef struct http_json_statistics_info_t
{
    mac_address_str_t                       nrf52_mac_addr;
    ruuvi_esp32_fw_ver_str_t                esp_fw;
    ruuvi_nrf52_fw_ver_str_t                nrf_fw;
    uint32_t                                uptime;
    uint32_t                                nonce;
    bool                                    nrf_status;
    bool                                    is_connected_to_wifi;
    uint32_t                                network_disconnect_cnt;
    uint32_t                                nrf_self_reboot_cnt;
    uint32_t                                nrf_ext_hw_reset_cnt;
    uint64_t                                nrf_lost_ack_cnt;
    http_json_statistics_reset_reason_buf_t reset_reason;
    uint32_t                                reset_cnt;
    const char*                             p_reset_info;
} http_json_statistics_info_t;

typedef struct http_json_header_info_t
{
    const bool                     flag_use_timestamps;
    const time_t                   timestamp;
    const mac_address_str_t* const p_mac_addr;
    const char* const              p_coordinates_str;
    const bool                     flag_use_nonce;
    const uint32_t                 nonce;
} http_json_header_info_t;

bool
http_json_create_status_str(
    const http_json_statistics_info_t* const p_stat_info,
    const adv_report_table_t* const          p_reports,
    cjson_wrap_str_t* const                  p_json_str);

typedef struct http_json_create_stream_gen_advs_params_t
{
    const bool                              flag_raw_data;
    const bool                              flag_decode;
    const bool                              flag_use_timestamps;
    const time_t                            cur_time;
    const bool                              flag_use_nonce;
    const uint32_t                          nonce;
    const mac_address_str_t* const          p_mac_addr;
    const ruuvi_gw_cfg_coordinates_t* const p_coordinates;
} http_json_create_stream_gen_advs_params_t;

json_stream_gen_t*
http_json_create_stream_gen_advs(
    const adv_report_table_t* const                        p_reports,
    const http_json_create_stream_gen_advs_params_t* const p_params);

#ifdef __cplusplus
}
#endif

#endif // RUUVI_GATEWAY_HTTP_JSON_H
