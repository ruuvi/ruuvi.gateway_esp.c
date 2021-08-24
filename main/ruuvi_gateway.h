/**
 * @file ruuvi_gateway.h
 * @author Jukka Saari
 * @date 2019-11-27
 * @copyright Ruuvi Innovations Ltd, license BSD-3-Clause.
 */

#ifndef RUUVI_GATEWAY_H
#define RUUVI_GATEWAY_H

#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include <stdbool.h>
#include <stdint.h>
#include "mac_addr.h"
#include "cjson_wrap.h"
#include "settings.h"
#include "http_server.h"

#ifdef __cplusplus
extern "C" {
#endif

#define ADV_POST_DEFAULT_INTERVAL_SECONDS 10

#define HTTP_SERVER_USER_REQ_CODE_DOWNLOAD_LATEST_RELEASE_INFO (HTTP_SERVER_USER_REQ_CODE_1)

#define FW_UPDATING_REGULAR_CYCLE_DELAY_SECONDS                      (14U * 24U * 60U * 60U)
#define FW_UPDATING_DELAY_BETWEEN_SUCCESSFUL_FW_UPDATE_CHECK_SECONDS (12 * 60 * 60)

#define WIFI_CONNECTED_BIT (1U << 0U)
#define MQTT_CONNECTED_BIT (1U << 1U)
#define ETH_CONNECTED_BIT  (1U << 4U)

typedef enum nrf_command_e
{
    NRF_COMMAND_SET_FILTER   = 0,
    NRF_COMMAND_CLEAR_FILTER = 1,
} nrf_command_e;

extern EventGroupHandle_t status_bits;

bool
settings_check_in_flash(void);

bool
settings_clear_in_flash(void);

void
ruuvi_send_nrf_settings(const ruuvi_gateway_config_t *p_config);

void
start_services(void);

void
main_task_start_timer_after_hotspot_activation();

void
main_task_stop_timer_after_hotspot_activation();

#ifdef __cplusplus
}
#endif

#endif // RUUVI_GATEWAY_H
