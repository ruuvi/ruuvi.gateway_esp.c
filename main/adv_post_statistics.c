/**
 * @file adv_post_statistics.c
 * @author TheSomeMan
 * @date 2023-09-04
 * @copyright Ruuvi Innovations Ltd, license BSD-3-Clause.
 */

#include "adv_post_statistics.h"
#include <esp_attr.h>
#include "os_malloc.h"
#include "gw_cfg.h"
#include "runtime_stat.h"
#include "adv_table.h"
#include "reset_info.h"
#include "http_json.h"
#include "reset_reason.h"
#include "gw_status.h"
#include "wifi_manager.h"
#include "http.h"
#include "reset_task.h"
#include "ruuvi_gateway.h"
#include "metrics.h"
#if defined(RUUVI_TESTS) && RUUVI_TESTS
#define LOG_LOCAL_DISABLED 1
#define LOG_LOCAL_LEVEL    LOG_LEVEL_NONE
#else
#define LOG_LOCAL_LEVEL LOG_LEVEL_INFO
#endif
#include "log.h"
static const char TAG[] = "ADV_POST_STAT";

static uint32_t IRAM_ATTR g_adv_post_stat_nonce;

void
adv_post_statistics_init(void)
{
    g_adv_post_stat_nonce = esp_random();
}

http_json_statistics_info_t*
adv_post_statistics_info_generate(const str_buf_t* const p_reset_info)
{
    http_json_statistics_info_t* p_stat_info = os_calloc(1, sizeof(*p_stat_info));
    if (NULL == p_stat_info)
    {
        return NULL;
    }

    p_stat_info->nrf52_mac_addr              = *gw_cfg_get_nrf52_mac_addr();
    p_stat_info->esp_fw                      = *gw_cfg_get_esp32_fw_ver();
    p_stat_info->nrf_fw                      = *gw_cfg_get_nrf52_fw_ver();
    p_stat_info->uptime                      = g_uptime_counter;
    p_stat_info->nonce                       = g_adv_post_stat_nonce;
    p_stat_info->nrf_status                  = gw_status_get_nrf_status();
    p_stat_info->is_connected_to_wifi        = wifi_manager_is_connected_to_wifi();
    p_stat_info->network_disconnect_cnt      = g_network_disconnect_cnt;
    p_stat_info->nrf_self_reboot_cnt         = metrics_nrf_self_reboot_cnt_get();
    p_stat_info->nrf_ext_hw_reset_cnt        = metrics_nrf_ext_hw_reset_cnt_get();
    p_stat_info->nrf_lost_ack_cnt            = metrics_nrf_lost_ack_cnt_get();
    p_stat_info->total_free_bytes_internal   = metrics_get_total_free_bytes(METRICS_MALLOC_CAP_INTERNAL);
    p_stat_info->total_free_bytes_default    = metrics_get_total_free_bytes(METRICS_MALLOC_CAP_DEFAULT);
    p_stat_info->largest_free_block_internal = metrics_get_largest_free_block(METRICS_MALLOC_CAP_INTERNAL);
    p_stat_info->largest_free_block_default  = metrics_get_largest_free_block(METRICS_MALLOC_CAP_DEFAULT);
    (void)snprintf(
        p_stat_info->reset_reason.buf,
        sizeof(p_stat_info->reset_reason.buf),
        "%s",
        reset_reason_to_str(esp_reset_reason()));
    p_stat_info->reset_cnt    = reset_info_get_cnt();
    p_stat_info->p_reset_info = ((NULL != p_reset_info) && (NULL != p_reset_info->buf)) ? &p_reset_info->buf[0] : "";

    g_adv_post_stat_nonce += 1;

    return p_stat_info;
}

static bool
adv_post_stat(const ruuvi_gw_cfg_http_stat_t* const p_cfg_http_stat, void* const p_user_data)
{
    log_runtime_statistics();

    adv_report_table_t* p_reports = os_malloc(sizeof(*p_reports));
    if (NULL == p_reports)
    {
        LOG_ERR("Can't allocate memory for statistics");
        return false;
    }
    adv_table_statistics_read(p_reports);

    str_buf_t reset_info = reset_info_get();
    if (NULL == reset_info.buf)
    {
        LOG_ERR("Can't allocate memory");
        os_free(p_reports);
        return false;
    }
    const http_json_statistics_info_t* p_stat_info = adv_post_statistics_info_generate(&reset_info);
    if (NULL == p_stat_info)
    {
        LOG_ERR("Can't allocate memory");
        str_buf_free_buf(&reset_info);
        os_free(p_reports);
        return false;
    }

    const bool res = http_post_stat(
        p_stat_info,
        p_reports,
        p_cfg_http_stat,
        p_user_data,
        p_cfg_http_stat->http_stat_use_ssl_client_cert,
        p_cfg_http_stat->http_stat_use_ssl_server_cert);
    os_free(p_stat_info);
    str_buf_free_buf(&reset_info);
    os_free(p_reports);

    return res;
}

bool
adv_post_statistics_do_send(void)
{
    const ruuvi_gw_cfg_http_stat_t* p_cfg_http_stat = gw_cfg_get_http_stat_copy();
    if (NULL == p_cfg_http_stat)
    {
        LOG_ERR("%s failed", "gw_cfg_get_http_copy");
        return false;
    }
    const bool res = adv_post_stat(p_cfg_http_stat, NULL);
    os_free(p_cfg_http_stat);

    return res;
}
