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
#include "attribs.h"
#include "mac_addr.h"
#include "cjson_wrap.h"
#include "settings.h"
#include "http_server.h"
#include "os_wrapper_types.h"

#ifdef __cplusplus
extern "C" {
#endif

#define RUUVI_BITS_PER_BYTE (8U)
#define RUUVI_BYTE_MASK     (0xFFU)

#define RUUVI_FREE_HEAP_LIM_KIB (10U)

#define ADV_POST_DEFAULT_INTERVAL_SECONDS (10)

#define ADV_POST_STATISTICS_INTERVAL_SECONDS (60 * 60)
#define ADV_POST_DO_ASYNC_COMM_INTERVAL_MS   (100)

#define HTTP_SERVER_USER_REQ_CODE_DOWNLOAD_LATEST_RELEASE_INFO (HTTP_SERVER_USER_REQ_CODE_1)

#define FW_UPDATING_REGULAR_CYCLE_DELAY_SECONDS (14U * 24U * 60U * 60U)

#define RUUVI_CHECK_FOR_FW_UPDATES_DELAY_AFTER_REBOOT_SECONDS  (40U * 60U)
#define RUUVI_CHECK_FOR_FW_UPDATES_DELAY_AFTER_SUCCESS_SECONDS (12U * 60U * 60U)
#define RUUVI_CHECK_FOR_FW_UPDATES_DELAY_BEFORE_RETRY_SECONDS  (40U * 60U)

#define RUUVI_NETWORK_WATCHDOG_TIMEOUT_SECONDS (60U * 60U)
#define RUUVI_NETWORK_WATCHDOG_PERIOD_SECONDS  (1U)

#define WIFI_CONNECTED_BIT (1U << 0U)
#define MQTT_CONNECTED_BIT (1U << 1U)
#define ETH_CONNECTED_BIT  (1U << 4U)

typedef enum nrf_command_e
{
    NRF_COMMAND_SET_FILTER   = 0,
    NRF_COMMAND_CLEAR_FILTER = 1,
} nrf_command_e;

extern EventGroupHandle_t status_bits;
extern volatile uint32_t  g_network_disconnect_cnt;

void
ruuvi_send_nrf_settings(void);

void
start_services(void);

void
restart_services(void);

void
main_task_start_timer_after_hotspot_activation(void);

void
main_task_stop_timer_after_hotspot_activation(void);

void
main_task_schedule_next_check_for_fw_updates(void);

void
main_task_schedule_retry_check_for_fw_updates(void);

void
main_task_send_sig_restart_services(void);

void
main_task_send_sig_mqtt_publish_connect(void);

void
main_task_timer_sig_check_for_fw_updates_restart(const os_delta_ticks_t delay_ticks);

void
main_task_timer_sig_check_for_fw_updates_stop(void);

bool
main_loop_init(void);

ATTR_NORETURN
void
main_loop(void);

#ifdef __cplusplus
}
#endif

#endif // RUUVI_GATEWAY_H
