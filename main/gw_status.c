/**
 * @file gw_status.h
 * @author TheSomeMan
 * @date 2022-05-05
 * @copyright Ruuvi Innovations Ltd, license BSD-3-Clause.
 */

#include "gw_status.h"
#include <esp_task_wdt.h>
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "os_mutex.h"
#include "event_mgr.h"
#include "time_units.h"

#define LOG_LOCAL_LEVEL LOG_LEVEL_INFO
#include "log.h"

#define WIFI_CONNECTED_BIT    (1U << 0U)
#define ETH_CONNECTED_BIT     (1U << 1U)
#define ETH_LINK_UP_BIT       (1U << 2U)
#define MQTT_STARTED_BIT      (1U << 3U)
#define MQTT_CONNECTED_BIT    (1U << 4U)
#define MQTT_RELAYING_CMD_BIT (1U << 5U)
#define HTTP_RELAYING_CMD_BIT (1U << 6U)
#define NRF_STATUS_BIT        (1U << 7U)
#define MQTT_ERROR_BIT        (1U << 8U)
#define MQTT_AUTH_FAIL_BIT    (1U << 9U)

#define TIMEOUT_WAITING_UNTIL_RELAYING_STOPPED_SECONDS (15)

typedef struct gw_status_relaying_t
{
    os_mutex_t        mutex_cnt_relaying_suspended;
    os_mutex_static_t mutex_mem_cnt_relaying_suspended;
    int32_t           cnt_relaying_via_http_suspended;
    int32_t           cnt_relaying_via_mqtt_suspended;
} gw_status_relaying_t;

static const char TAG[] = "gw_status";

static EventGroupHandle_t   g_p_ev_grp_status_bits;
static gw_status_relaying_t g_gw_status_relaying;

bool
gw_status_init(void)
{
    g_p_ev_grp_status_bits = xEventGroupCreate();
    if (NULL == g_p_ev_grp_status_bits)
    {
        LOG_ERR("%s failed", "xEventGroupCreate");
        return false;
    }
    gw_status_relaying_t* const p_relaying      = &g_gw_status_relaying;
    p_relaying->cnt_relaying_via_http_suspended = 0;
    p_relaying->cnt_relaying_via_mqtt_suspended = 0;
    p_relaying->mutex_cnt_relaying_suspended    = os_mutex_create_static(&p_relaying->mutex_mem_cnt_relaying_suspended);
    return true;
}

void
gw_status_set_wifi_connected(void)
{
    xEventGroupSetBits(g_p_ev_grp_status_bits, WIFI_CONNECTED_BIT);
}

void
gw_status_clear_wifi_connected(void)
{
    xEventGroupClearBits(g_p_ev_grp_status_bits, WIFI_CONNECTED_BIT);
}

void
gw_status_set_eth_connected(void)
{
    xEventGroupSetBits(g_p_ev_grp_status_bits, ETH_CONNECTED_BIT);
}

void
gw_status_clear_eth_connected(void)
{
    xEventGroupClearBits(g_p_ev_grp_status_bits, ETH_CONNECTED_BIT | ETH_LINK_UP_BIT);
}

void
gw_status_set_eth_link_up(void)
{
    xEventGroupSetBits(g_p_ev_grp_status_bits, ETH_LINK_UP_BIT);
}

bool
gw_status_is_eth_link_up(void)
{
    if (0 != (xEventGroupGetBits(g_p_ev_grp_status_bits) & ETH_LINK_UP_BIT))
    {
        return true;
    }
    return false;
}

bool
gw_status_is_network_connected(void)
{
    if (0 != (xEventGroupGetBits(g_p_ev_grp_status_bits) & (WIFI_CONNECTED_BIT | ETH_CONNECTED_BIT)))
    {
        return true;
    }
    return false;
}

void
gw_status_set_mqtt_started(void)
{
    LOG_INFO("MQTT started");
    xEventGroupSetBits(g_p_ev_grp_status_bits, MQTT_STARTED_BIT);
}

void
gw_status_clear_mqtt_started(void)
{
    LOG_INFO("MQTT stopped");
    xEventGroupClearBits(g_p_ev_grp_status_bits, MQTT_STARTED_BIT);
}

bool
gw_status_is_mqtt_started(void)
{
    if (0 != (xEventGroupGetBits(g_p_ev_grp_status_bits) & MQTT_STARTED_BIT))
    {
        return true;
    }
    return false;
}

static void
gw_status_set_mqtt_relaying_cmd(void)
{
    LOG_INFO("Set MQTT_RELAYING_CMD_BIT");
    xEventGroupSetBits(g_p_ev_grp_status_bits, MQTT_RELAYING_CMD_BIT);
}

void
gw_status_clear_mqtt_relaying_cmd(void)
{
    LOG_INFO("Clear MQTT_RELAYING_CMD_BIT");
    xEventGroupClearBits(g_p_ev_grp_status_bits, MQTT_RELAYING_CMD_BIT);
}

bool
gw_status_is_mqtt_relaying_cmd_handled(void)
{
    if (0 == (xEventGroupGetBits(g_p_ev_grp_status_bits) & MQTT_RELAYING_CMD_BIT))
    {
        return true;
    }
    return false;
}

void
gw_status_set_mqtt_connected(void)
{
    LOG_INFO("MQTT connected");
    xEventGroupClearBits(g_p_ev_grp_status_bits, MQTT_ERROR_BIT | MQTT_AUTH_FAIL_BIT);
    xEventGroupSetBits(g_p_ev_grp_status_bits, MQTT_CONNECTED_BIT);
}

void
gw_status_clear_mqtt_connected(void)
{
    LOG_INFO("MQTT disconnected");
    xEventGroupClearBits(g_p_ev_grp_status_bits, MQTT_CONNECTED_BIT);
}

void
gw_status_set_mqtt_error(const bool flag_auth_failed)
{
    LOG_INFO("MQTT connection error: flag_auth_failed=%d", (printf_int_t)flag_auth_failed);
    xEventGroupClearBits(g_p_ev_grp_status_bits, MQTT_CONNECTED_BIT | MQTT_ERROR_BIT | MQTT_AUTH_FAIL_BIT);
    xEventGroupSetBits(g_p_ev_grp_status_bits, MQTT_ERROR_BIT | (flag_auth_failed ? MQTT_AUTH_FAIL_BIT : 0));
}

void
gw_status_clear_mqtt_connected_and_error(void)
{
    xEventGroupClearBits(g_p_ev_grp_status_bits, MQTT_CONNECTED_BIT | MQTT_ERROR_BIT | MQTT_AUTH_FAIL_BIT);
}

bool
gw_status_is_mqtt_connected(void)
{
    if (0 != (xEventGroupGetBits(g_p_ev_grp_status_bits) & MQTT_CONNECTED_BIT))
    {
        return true;
    }
    return false;
}

bool
gw_status_is_mqtt_error(void)
{
    if (0 != (xEventGroupGetBits(g_p_ev_grp_status_bits) & MQTT_ERROR_BIT))
    {
        return true;
    }
    return false;
}

bool
gw_status_is_mqtt_auth_error(void)
{
    if (0 != (xEventGroupGetBits(g_p_ev_grp_status_bits) & MQTT_AUTH_FAIL_BIT))
    {
        return true;
    }
    return false;
}

bool
gw_status_is_mqtt_connected_or_error(void)
{
    if (0 != (xEventGroupGetBits(g_p_ev_grp_status_bits) & (MQTT_CONNECTED_BIT | MQTT_ERROR_BIT)))
    {
        return true;
    }
    return false;
}

static void
gw_status_set_http_relaying_cmd(void)
{
    LOG_INFO("Set HTTP_RELAYING_CMD_BIT");
    xEventGroupSetBits(g_p_ev_grp_status_bits, HTTP_RELAYING_CMD_BIT);
}

void
gw_status_clear_http_relaying_cmd(void)
{
    LOG_INFO("Clear HTTP_RELAYING_CMD_BIT");
    xEventGroupClearBits(g_p_ev_grp_status_bits, HTTP_RELAYING_CMD_BIT);
}

static bool
gw_status_is_http_relaying_cmd_handled(void)
{
    if (0 == (xEventGroupGetBits(g_p_ev_grp_status_bits) & HTTP_RELAYING_CMD_BIT))
    {
        return true;
    }
    return false;
}

static void
gw_status_wait_after_relaying_mode_changed(
    const bool flag_notify_change_for_http,
    const bool flag_notify_change_for_mqtt)
{
    const TickType_t tick_start = xTaskGetTickCount();
    if (flag_notify_change_for_mqtt)
    {
        while (!gw_status_is_mqtt_relaying_cmd_handled())
        {
            if ((xTaskGetTickCount() - tick_start)
                >= pdMS_TO_TICKS(TIMEOUT_WAITING_UNTIL_RELAYING_STOPPED_SECONDS * TIME_UNITS_MS_PER_SECOND))
            {
                LOG_ERR("Timeout waiting until MQTT relaying command is handled");
                break;
            }
            esp_task_wdt_reset();
            vTaskDelay(pdMS_TO_TICKS(50));
        }
        LOG_INFO("MQTT relaying command handled");
    }
    if (flag_notify_change_for_http)
    {
        while (!gw_status_is_http_relaying_cmd_handled())
        {
            if ((xTaskGetTickCount() - tick_start)
                >= pdMS_TO_TICKS(TIMEOUT_WAITING_UNTIL_RELAYING_STOPPED_SECONDS * TIME_UNITS_MS_PER_SECOND))
            {
                LOG_ERR("Timeout waiting until HTTP relaying command is handled");
                break;
            }
            esp_task_wdt_reset();
            vTaskDelay(pdMS_TO_TICKS(50));
        }
        LOG_INFO("HTTP relaying command handled");
    }
}

static void
gw_status_notify_relaying_mode_changed_and_wait(
    const bool flag_notify_change_for_http,
    const bool flag_notify_change_for_mqtt,
    const bool flag_wait)
{
    if (!flag_notify_change_for_http && (!flag_notify_change_for_mqtt))
    {
        return;
    }
    if (flag_notify_change_for_http)
    {
        gw_status_set_http_relaying_cmd();
    }
    if (flag_notify_change_for_mqtt)
    {
        gw_status_set_mqtt_relaying_cmd();
    }

    event_mgr_notify(EVENT_MGR_EV_RELAYING_MODE_CHANGED);

    if (flag_wait)
    {
        gw_status_wait_after_relaying_mode_changed(flag_notify_change_for_http, flag_notify_change_for_mqtt);
    }
}

void
gw_status_suspend_relaying(const bool flag_wait_until_relaying_stopped)
{
    LOG_INFO("SUSPEND RELAYING");
    gw_status_relaying_t* const p_relaying = &g_gw_status_relaying;
    os_mutex_lock(p_relaying->mutex_cnt_relaying_suspended);
    const bool flag_notify_change_for_http = (0 == p_relaying->cnt_relaying_via_http_suspended) ? true : false;
    const bool flag_notify_change_for_mqtt = (0 == p_relaying->cnt_relaying_via_mqtt_suspended) ? true : false;
    p_relaying->cnt_relaying_via_http_suspended += 1;
    p_relaying->cnt_relaying_via_mqtt_suspended += 1;
    os_mutex_unlock(p_relaying->mutex_cnt_relaying_suspended);

    gw_status_notify_relaying_mode_changed_and_wait(
        flag_notify_change_for_http,
        flag_notify_change_for_mqtt,
        flag_wait_until_relaying_stopped);
}

void
gw_status_resume_relaying(const bool flag_wait)
{
    LOG_INFO("RESUME RELAYING");
    gw_status_relaying_t* const p_relaying = &g_gw_status_relaying;
    os_mutex_lock(p_relaying->mutex_cnt_relaying_suspended);
    bool flag_notify_change_for_http = false;
    bool flag_notify_change_for_mqtt = false;
    if (0 == p_relaying->cnt_relaying_via_http_suspended)
    {
        LOG_WARN("Attempt to resume relaying via HTTP, but it is already active");
    }
    else
    {
        p_relaying->cnt_relaying_via_http_suspended -= 1;
        if (0 == p_relaying->cnt_relaying_via_http_suspended)
        {
            flag_notify_change_for_http = true;
        }
    }
    if (0 == p_relaying->cnt_relaying_via_mqtt_suspended)
    {
        LOG_WARN("Attempt to resume relaying via MQTT, but it is already active");
    }
    else
    {
        p_relaying->cnt_relaying_via_mqtt_suspended -= 1;
        if (0 == p_relaying->cnt_relaying_via_mqtt_suspended)
        {
            flag_notify_change_for_mqtt = true;
        }
    }
    os_mutex_unlock(p_relaying->mutex_cnt_relaying_suspended);

    gw_status_notify_relaying_mode_changed_and_wait(
        flag_notify_change_for_http,
        flag_notify_change_for_mqtt,
        flag_wait);
}

void
gw_status_suspend_http_relaying(const bool flag_wait_until_relaying_stopped)
{
    LOG_INFO("SUSPEND HTTP RELAYING");
    gw_status_relaying_t* const p_relaying = &g_gw_status_relaying;
    os_mutex_lock(p_relaying->mutex_cnt_relaying_suspended);
    const bool flag_notify_change_for_http = (0 == p_relaying->cnt_relaying_via_http_suspended) ? true : false;
    p_relaying->cnt_relaying_via_http_suspended += 1;
    os_mutex_unlock(p_relaying->mutex_cnt_relaying_suspended);

    gw_status_notify_relaying_mode_changed_and_wait(
        flag_notify_change_for_http,
        false,
        flag_wait_until_relaying_stopped);
}

void
gw_status_resume_http_relaying(const bool flag_wait)
{
    LOG_INFO("RESUME HTTP RELAYING");
    gw_status_relaying_t* const p_relaying = &g_gw_status_relaying;
    os_mutex_lock(p_relaying->mutex_cnt_relaying_suspended);
    bool flag_notify_change_for_http = false;
    if (0 == p_relaying->cnt_relaying_via_http_suspended)
    {
        LOG_WARN("Attempt to resume relaying via HTTP, but it is already active");
    }
    else
    {
        p_relaying->cnt_relaying_via_http_suspended -= 1;
        if (0 == p_relaying->cnt_relaying_via_http_suspended)
        {
            flag_notify_change_for_http = true;
        }
    }
    os_mutex_unlock(p_relaying->mutex_cnt_relaying_suspended);

    gw_status_notify_relaying_mode_changed_and_wait(flag_notify_change_for_http, false, flag_wait);
}

bool
gw_status_is_relaying_via_http_enabled(void)
{
    gw_status_relaying_t* const p_relaying = &g_gw_status_relaying;
    os_mutex_lock(p_relaying->mutex_cnt_relaying_suspended);
    const bool flag_relaying_enabled = (0 == p_relaying->cnt_relaying_via_http_suspended) ? true : false;
    os_mutex_unlock(p_relaying->mutex_cnt_relaying_suspended);
    return flag_relaying_enabled;
}

bool
gw_status_is_relaying_via_mqtt_enabled(void)
{
    gw_status_relaying_t* const p_relaying = &g_gw_status_relaying;
    os_mutex_lock(p_relaying->mutex_cnt_relaying_suspended);
    const bool flag_relaying_enabled = (0 == p_relaying->cnt_relaying_via_mqtt_suspended) ? true : false;
    os_mutex_unlock(p_relaying->mutex_cnt_relaying_suspended);
    return flag_relaying_enabled;
}

void
gw_status_set_nrf_status(void)
{
    xEventGroupSetBits(g_p_ev_grp_status_bits, NRF_STATUS_BIT);
}

void
gw_status_clear_nrf_status(void)
{
    xEventGroupClearBits(g_p_ev_grp_status_bits, NRF_STATUS_BIT);
}

bool
gw_status_get_nrf_status(void)
{
    if (0 != (xEventGroupGetBits(g_p_ev_grp_status_bits) & NRF_STATUS_BIT))
    {
        return true;
    }
    return false;
}
