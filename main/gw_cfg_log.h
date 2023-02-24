/**
 * @file gw_cfg_log.h
 * @author TheSomeMan
 * @date 2022-04-12
 * @copyright Ruuvi Innovations Ltd, license BSD-3-Clause.
 */

#ifndef RUUVI_GATEWAY_ESP_GW_CFG_LOG_H
#define RUUVI_GATEWAY_ESP_GW_CFG_LOG_H

#include "gw_cfg.h"

#ifdef __cplusplus
extern "C" {
#endif

void
gw_cfg_log(const gw_cfg_t* const p_gw_cfg, const char* const p_title, const bool flag_log_device_info);

void
gw_cfg_log_device_info(const gw_cfg_device_info_t* const p_dev_info, const char* const p_title);

void
gw_cfg_log_eth_cfg(const gw_cfg_eth_t* const p_gw_cfg_eth, const char* const p_title);

void
gw_cfg_log_ruuvi_cfg(const gw_cfg_ruuvi_t* const p_gw_cfg_ruuvi, const char* const p_title);

void
gw_cfg_log_wifi_cfg_ap(const wifiman_config_ap_t* const p_wifi_cfg_ap, const char* const p_title);

void
gw_cfg_log_wifi_cfg_sta(const wifiman_config_sta_t* const p_wifi_cfg_sta, const char* const p_title);

#ifdef __cplusplus
}
#endif

#endif // RUUVI_GATEWAY_ESP_GW_CFG_LOG_H
