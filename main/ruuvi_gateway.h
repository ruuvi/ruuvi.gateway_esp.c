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
#include "time_units.h"

#ifdef __cplusplus
extern "C" {
#endif

#define ADV_DATA_MAX_LEN (48)

#define RUUVI_BITS_PER_BYTE (8U)
#define RUUVI_BYTE_MASK     (0xFFU)

#define RUUVI_GATEWAY_ENABLE_MEM_FRAGMENTATION_TEST (0)

#define RUUVI_LARGEST_FREE_BLOCK_LIM_KIB (18U)
#define RUUVI_FREE_HEAP_LIM_KIB          (50U)
#define RUUVI_MAX_LOW_HEAP_MEM_CNT       (5)

#define ADV_POST_DEFAULT_INTERVAL_SECONDS (10)

#define ADV_POST_STATISTICS_INTERVAL_SECONDS (60 * 60)
#define ADV_POST_DO_ASYNC_COMM_INTERVAL_MS   (50)

#define HTTP_SERVER_USER_REQ_CODE_DOWNLOAD_LATEST_RELEASE_INFO (HTTP_SERVER_USER_REQ_CODE_1)
#define HTTP_SERVER_USER_REQ_CODE_DOWNLOAD_GW_CFG              (HTTP_SERVER_USER_REQ_CODE_2)

#define RUUVI_CHECK_FOR_FW_UPDATES_DELAY_AFTER_REBOOT_SECONDS  (40U * 60U)
#define RUUVI_CHECK_FOR_FW_UPDATES_DELAY_AFTER_SUCCESS_SECONDS (12U * 60U * 60U)
#define RUUVI_CHECK_FOR_FW_UPDATES_DELAY_BEFORE_RETRY_SECONDS  (40U * 60U)

#define RUUVI_NETWORK_WATCHDOG_TIMEOUT_SECONDS (60U * 60U)
#define RUUVI_NETWORK_WATCHDOG_PERIOD_SECONDS  (1U)

#define RUUVI_CFG_MODE_DEACTIVATION_DEFAULT_DELAY_SEC (60)
#define RUUVI_CFG_MODE_DEACTIVATION_SHORT_DELAY_SEC   (5)
#define RUUVI_CFG_MODE_DEACTIVATION_LONG_DELAY_SEC    (60 * 60)

#define RUUVI_DELAY_BEFORE_ETHERNET_ACTIVATION_ON_FIRST_BOOT_SEC (60)

#define RUUVI_POST_ADVS_TLS_IN_CONTENT_LEN  (8192)
#define RUUVI_POST_ADVS_TLS_OUT_CONTENT_LEN (4096)

#define RUUVI_POST_STAT_TLS_IN_CONTENT_LEN  (8192)
#define RUUVI_POST_STAT_TLS_OUT_CONTENT_LEN (4096)

#define RUUVI_MQTT_TLS_IN_CONTENT_LEN  (8192)
#define RUUVI_MQTT_TLS_OUT_CONTENT_LEN (4096)

extern volatile uint32_t g_network_disconnect_cnt;

void
timer_cfg_mode_deactivation_start_with_delay(const TimeUnitsSeconds_t delay_sec);

void
timer_cfg_mode_deactivation_start(void);

void
timer_cfg_mode_deactivation_stop(void);

bool
timer_cfg_mode_deactivation_is_active(void);

void
main_task_stop_timer_check_for_remote_cfg(void);

void
main_task_schedule_next_check_for_fw_updates(void);

void
main_task_schedule_retry_check_for_fw_updates(void);

void
main_task_send_sig_restart_services(void);

void
main_task_send_sig_activate_cfg_mode(void);

void
main_task_send_sig_deactivate_cfg_mode(void);

void
main_task_send_sig_reconnect_network(void);

void
main_task_send_sig_set_default_config(void);

void
main_task_send_sig_log_runtime_stat(void);

void
main_task_timer_sig_check_for_fw_updates_restart(const os_delta_ticks_t delay_ticks);

void
main_task_timer_sig_check_for_fw_updates_stop(void);

void
main_task_start_timer_activation_ethernet_after_timeout(void);

void
main_task_stop_timer_activation_ethernet_after_timeout(void);

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

void
http_server_mutex_activate(void);

void
http_server_mutex_deactivate(void);

bool
http_server_mutex_try_lock(void);

void
http_server_mutex_unlock(void);

void
start_wifi_ap(void);

void
start_wifi_ap_without_blocking_req_from_lan(void);

void
ruuvi_log_heap_usage(void);

#ifdef __cplusplus
}
#endif

#endif // RUUVI_GATEWAY_H
