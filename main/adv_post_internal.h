/**
 * @file adv_post_internal.h
 * @author TheSomeMan
 * @date 2023-09-04
 * @copyright Ruuvi Innovations Ltd, license BSD-3-Clause.
 */

#ifndef RUUVI_GATEWAY_ESP_ADV_POST_INTERNAL_H
#define RUUVI_GATEWAY_ESP_ADV_POST_INTERNAL_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define ADV_POST_TASK_WATCHDOG_FEEDING_PERIOD_TICKS pdMS_TO_TICKS(1000)
#define ADV_POST_TASK_LED_UPDATE_PERIOD_TICKS       pdMS_TO_TICKS(1000)
#define ADV_POST_TASK_RECV_ADV_TIMEOUT_TICKS        pdMS_TO_TICKS(10 * 1000)

#define ADV_POST_GREEN_LED_ON_INTERVAL_MS (1500)

#define ADV_POST_DELAY_BEFORE_RETRYING_POST_AFTER_ERROR_MS   (67 * 1000)
#define ADV_POST_DELAY_AFTER_CONFIGURATION_CHANGED_MS        (5 * 1000)
#define ADV_POST_INITIAL_DELAY_IN_SENDING_STATISTICS_SECONDS (60)

typedef struct adv_post_state_t
{
    bool flag_primary_time_sync_is_done;
    bool flag_network_connected;
    bool flag_async_comm_in_progress;
    bool flag_need_to_send_advs1;
    bool flag_need_to_send_advs2;
    bool flag_need_to_send_statistics;
    bool flag_need_to_send_mqtt_periodic;
    bool flag_relaying_enabled;
    bool flag_use_timestamps;
    bool flag_stop;
} adv_post_state_t;

#ifdef __cplusplus
}
#endif

#endif // RUUVI_GATEWAY_ESP_ADV_POST_INTERNAL_H
