/**
 * @file gw_status.h
 * @author TheSomeMan
 * @date 2022-05-05
 * @copyright Ruuvi Innovations Ltd, license BSD-3-Clause.
 */

#ifndef RUUVI_GATEWAY_ESP_GW_STATUS_H
#define RUUVI_GATEWAY_ESP_GW_STATUS_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

bool
gw_status_init(void);

void
gw_status_set_wifi_connected(void);

void
gw_status_clear_wifi_connected(void);

void
gw_status_set_eth_connected(void);

void
gw_status_clear_eth_connected(void);

void
gw_status_set_eth_link_up(void);

bool
gw_status_is_eth_link_up(void);

bool
gw_status_is_network_connected(void);

void
gw_status_set_mqtt_connected(void);

void
gw_status_clear_mqtt_connected(void);

void
gw_status_set_mqtt_error(const bool flag_auth_failed);

void
gw_status_clear_mqtt_connected_and_error(void);

bool
gw_status_is_mqtt_connected(void);

bool
gw_status_is_mqtt_error(void);

bool
gw_status_is_mqtt_auth_error(void);

bool
gw_status_is_mqtt_connected_or_error(void);

void
gw_status_set_nrf_status(void);

void
gw_status_clear_nrf_status(void);

bool
gw_status_get_nrf_status(void);

void
gw_status_clear_mqtt_relaying_cmd(void);

void
gw_status_set_mqtt_started(void);

void
gw_status_clear_mqtt_started(void);

bool
gw_status_is_mqtt_started(void);

bool
gw_status_is_mqtt_relaying_cmd_handled(void);

void
gw_status_clear_http_relaying_cmd(void);

void
gw_status_resume_relaying(const bool flag_wait);

void
gw_status_suspend_relaying(const bool flag_wait_until_relaying_stopped);

void
gw_status_suspend_http_relaying(const bool flag_wait_until_relaying_stopped);

void
gw_status_resume_http_relaying(const bool flag_wait);

bool
gw_status_is_relaying_via_http_enabled(void);

bool
gw_status_is_relaying_via_mqtt_enabled(void);

void
gw_status_set_first_boot_after_cfg_erase(void);

void
gw_status_clear_first_boot_after_cfg_erase(void);

bool
gw_status_get_first_boot_after_cfg_erase(void);

#ifdef __cplusplus
}
#endif

#endif // RUUVI_GATEWAY_ESP_GW_STATUS_H
