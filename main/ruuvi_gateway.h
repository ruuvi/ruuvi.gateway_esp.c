/**
 * @file ruuvi_gateway.h
 * @author Jukka Saari
 * @date 2019-11-27
 * @copyright Ruuvi Innovations Ltd, license BSD-3-Clause.
 */

#ifndef RUUVI_GATEWAY_H
#define RUUVI_GATEWAY_H

#include <stdbool.h>
#include <stdint.h>
#include "settings.h"
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
#define HTTP_SERVER_USER_REQ_CODE_DOWNLOAD_GW_CFG              (HTTP_SERVER_USER_REQ_CODE_2)

#define FW_UPDATING_REGULAR_CYCLE_DELAY_SECONDS (14U * 24U * 60U * 60U)

#define RUUVI_CHECK_FOR_FW_UPDATES_DELAY_AFTER_REBOOT_SECONDS  (40U * 60U)
#define RUUVI_CHECK_FOR_FW_UPDATES_DELAY_AFTER_SUCCESS_SECONDS (12U * 60U * 60U)
#define RUUVI_CHECK_FOR_FW_UPDATES_DELAY_BEFORE_RETRY_SECONDS  (40U * 60U)

#define RUUVI_NETWORK_WATCHDOG_TIMEOUT_SECONDS (60U * 60U)
#define RUUVI_NETWORK_WATCHDOG_PERIOD_SECONDS  (1U)

extern volatile uint32_t g_network_disconnect_cnt;

void
ruuvi_send_nrf_settings(void);

void
main_task_start_timer_hotspot_deactivation(void);

void
main_task_stop_timer_hotspot_deactivation(void);

bool
main_task_is_active_timer_hotspot_deactivation(void);

void
main_task_stop_wifi_hotspot_after_short_delay(void);

void
main_task_stop_timer_check_for_remote_cfg(void);

void
main_task_schedule_next_check_for_fw_updates(void);

void
main_task_schedule_retry_check_for_fw_updates(void);

void
main_task_send_sig_restart_services(void);

void
main_task_send_sig_reconnect_network(void);

void
main_task_send_sig_set_default_config(void);

void
main_task_send_sig_mqtt_publish_connect(void);

void
main_task_timer_sig_check_for_fw_updates_restart(const os_delta_ticks_t delay_ticks);

void
main_task_timer_sig_check_for_fw_updates_stop(void);

void
main_task_on_get_history(void);

bool
main_loop_init(void);

void
main_task_init_timers(void);

void
main_task_subscribe_events(void);

void
main_task_configure_periodic_remote_cfg_check(void);

ATTR_NORETURN
void
main_loop(void);

void
sleep_with_task_watchdog_feeding(const int32_t delay_seconds);

#ifdef __cplusplus
}
#endif

#endif // RUUVI_GATEWAY_H
