/**
 * @file adv_post_nrf52.c
 * @author TheSomeMan
 * @date 2025-05-09
 * @copyright Ruuvi Innovations Ltd, license BSD-3-Clause.
 */

#include "adv_post_nrf52.h"
#include <assert.h>
#include <stdbool.h>
#include <string.h>
#include "os_mutex.h"
#include "os_timer_sig.h"
#include "ruuvi_endpoint_ca_uart.h"
#include "api.h"
#include "ruuvi_gateway.h"
#include "adv_post_signals.h"
#include "adv_table.h"
#include "event_mgr.h"
#include "nrf52fw.h"
#include "metrics.h"

#define LOG_LOCAL_LEVEL LOG_LEVEL_INFO
#include "log.h"
static const char* TAG = "ADV_POST_NRF52";

#define ADV_POST_NRF52_RESET_TIMEOUT_MS   (100U)
#define ADV_POST_NRF52_CFG_REQ_TIMEOUT_MS (500U)
#define ADV_POST_NRF52_ACK_TIMEOUT_MS     (100U)
#define ADV_POST_NRF52_NO_ACK_MAX_CNT     (10)

typedef struct adv_post_nrf52_cfg_t
{
    ruuvi_gw_cfg_scan_t   scan;
    ruuvi_gw_cfg_filter_t filter;
    os_mutex_static_t     mutex_mem;
    os_mutex_t            mutex;
    bool                  flag_cfg_ready;
    bool                  flag_nrf52_configured;
    bool                  flag_nrf52_in_hw_reset_state;
} adv_post_nrf52_cfg_t;

static adv_post_nrf52_cfg_t g_adv_post_nrf52_cfg;

static os_task_handle_t g_adv_post_nrf52_task_handle;
// All functions that send commands to nRF52 must be called from the same thread.
// So, there is no need to use mutexes to protect the access to the following global variables.
static re_ca_uart_cmd_t g_adv_post_nrf52_last_cmd;
static bool             g_adv_post_nrf52_is_waiting_ack;
static bool             g_adv_post_nrf52_flag_cfg_required;
static uint16_t         g_adv_post_nrf52_ack_timeout_cnt;

#define ADV_POST_LED_CTRL_TIME_INTERVAL_INVALID (0xFFFFU)
static uint16_t g_adv_post_nrf52_led_ctrl_time_interval_ms;

static os_timer_sig_one_shot_static_t g_adv_post_timer_sig_nrf52_ack_timeout_mem;
static os_timer_sig_one_shot_t*       g_adv_post_timer_sig_nrf52_ack_timeout;

static os_timer_sig_one_shot_static_t g_adv_post_timer_sig_nrf52_reset_mem;
static os_timer_sig_one_shot_t*       g_adv_post_timer_sig_nrf52_reset;

static os_timer_sig_one_shot_static_t g_adv_post_timer_sig_nrf52_cfg_req_timeout_mem;
static os_timer_sig_one_shot_t*       g_adv_post_timer_sig_nrf52_cfg_req_timeout;

bool
adv_post_nrf52_is_in_hw_reset_state(void)
{
    os_mutex_lock(g_adv_post_nrf52_cfg.mutex);
    const bool flag_nrf52_in_hw_reset_state = g_adv_post_nrf52_cfg.flag_nrf52_in_hw_reset_state;
    os_mutex_unlock(g_adv_post_nrf52_cfg.mutex);
    return flag_nrf52_in_hw_reset_state;
}

static void
adv_post_nrf52_cfg_update_cached(const ruuvi_gw_cfg_scan_t* const p_scan, const ruuvi_gw_cfg_filter_t* const p_filter)
{
    assert(NULL != g_adv_post_nrf52_cfg.mutex);
    LOG_INFO("Update cached nRF52 settings");
    os_mutex_lock(g_adv_post_nrf52_cfg.mutex);
    g_adv_post_nrf52_cfg.scan                         = *p_scan;
    g_adv_post_nrf52_cfg.filter                       = *p_filter;
    g_adv_post_nrf52_cfg.flag_nrf52_configured        = false;
    g_adv_post_nrf52_cfg.flag_nrf52_in_hw_reset_state = false;
    g_adv_post_nrf52_cfg.flag_cfg_ready               = true;
    os_mutex_unlock(g_adv_post_nrf52_cfg.mutex);
}

static bool
adv_post_nrf52_cfg_is_ready(void)
{
    assert(NULL != g_adv_post_nrf52_cfg.mutex);
    os_mutex_lock(g_adv_post_nrf52_cfg.mutex);
    const bool flag_cfg_ready = g_adv_post_nrf52_cfg.flag_cfg_ready;
    os_mutex_unlock(g_adv_post_nrf52_cfg.mutex);
    return flag_cfg_ready;
}

static void
adv_post_nrf52_cfg_get_cached(ruuvi_gw_cfg_scan_t* const p_scan, ruuvi_gw_cfg_filter_t* const p_filter)
{
    assert(NULL != g_adv_post_nrf52_cfg.mutex);
    os_mutex_lock(g_adv_post_nrf52_cfg.mutex);
    *p_scan   = g_adv_post_nrf52_cfg.scan;
    *p_filter = g_adv_post_nrf52_cfg.filter;
    os_mutex_unlock(g_adv_post_nrf52_cfg.mutex);
}

static void
adv_post_nrf52_cfg_set_configured(void)
{
    assert(NULL != g_adv_post_nrf52_cfg.mutex);
    os_mutex_lock(g_adv_post_nrf52_cfg.mutex);
    g_adv_post_nrf52_cfg.flag_nrf52_configured = true;
    os_mutex_unlock(g_adv_post_nrf52_cfg.mutex);
}

static void
adv_post_nrf52_cfg_set_unconfigured(void)
{
    assert(NULL != g_adv_post_nrf52_cfg.mutex);
    os_mutex_lock(g_adv_post_nrf52_cfg.mutex);
    g_adv_post_nrf52_cfg.flag_nrf52_configured = false;
    os_mutex_unlock(g_adv_post_nrf52_cfg.mutex);
}

bool
adv_post_nrf52_is_configured(void)
{
    os_mutex_lock(g_adv_post_nrf52_cfg.mutex);
    const bool flag_nrf52_configured = g_adv_post_nrf52_cfg.flag_nrf52_configured;
    os_mutex_unlock(g_adv_post_nrf52_cfg.mutex);
    return flag_nrf52_configured;
}

static void
adv_post_nrf52_set_waiting_ack(const re_ca_uart_cmd_t cmd)
{
    LOG_DBG("cmd=%d, start timer nrf52_ack_timeout", cmd);
    os_timer_sig_one_shot_relaunch(g_adv_post_timer_sig_nrf52_ack_timeout, true);
    g_adv_post_nrf52_last_cmd       = cmd;
    g_adv_post_nrf52_is_waiting_ack = true;
}

static void
adv_post_nrf52_send_cmd_led_ctrl_internal(void)
{
    assert(!g_adv_post_nrf52_is_waiting_ack);
    assert(ADV_POST_LED_CTRL_TIME_INTERVAL_INVALID != g_adv_post_nrf52_led_ctrl_time_interval_ms);

    const uint16_t time_interval_ms = g_adv_post_nrf52_led_ctrl_time_interval_ms;
    adv_post_nrf52_set_waiting_ack(RE_CA_UART_LED_CTRL);
    if (api_send_led_ctrl(time_interval_ms) < 0)
    {
        LOG_ERR("%s failed", "api_send_led_ctrl");
        return;
    }
}

static inline uint8_t
conv_bool_to_u8(const bool x)
{
    return x ? (uint8_t)RE_CA_BOOL_ENABLE : (uint8_t)RE_CA_BOOL_DISABLE;
}

static void
adv_post_nrf52_send_cmd_cfg_internal(void)
{
    assert(!g_adv_post_nrf52_is_waiting_ack);
    assert(!g_adv_post_nrf52_cfg.flag_nrf52_configured);
    assert(g_adv_post_nrf52_flag_cfg_required);

    ruuvi_gw_cfg_scan_t   scan   = { 0 };
    ruuvi_gw_cfg_filter_t filter = { 0 };
    adv_post_nrf52_cfg_get_cached(&scan, &filter);

    LOG_INFO(
        "### sending settings to NRF: "
        "use filter: %d, "
        "company id: 0x%04x,"
        "use scan Coded PHY: %d,"
        "use scan 1Mbit PHY: %d,"
        "use scan 2Mbit PHY: %d,"
        "use scan channel 37: %d,"
        "use scan channel 38: %d,"
        "use scan channel 39: %d",
        filter.company_use_filtering,
        filter.company_id,
        scan.scan_coded_phy,
        scan.scan_1mbit_phy,
        scan.scan_2mbit_phy,
        scan.scan_channel_37,
        scan.scan_channel_38,
        scan.scan_channel_39);

    adv_post_nrf52_set_waiting_ack(RE_CA_UART_SET_ALL);
    const int8_t res = api_send_all(
        RE_CA_UART_SET_ALL,
        filter.company_id,
        conv_bool_to_u8(filter.company_use_filtering),
        conv_bool_to_u8(scan.scan_coded_phy),
        conv_bool_to_u8(scan.scan_2mbit_phy),
        conv_bool_to_u8(scan.scan_1mbit_phy),
        conv_bool_to_u8(scan.scan_channel_37),
        conv_bool_to_u8(scan.scan_channel_38),
        conv_bool_to_u8(scan.scan_channel_39),
        ADV_DATA_MAX_LEN);
    if (0 != res)
    {
        LOG_ERR("Failed to send settings to NRF");
        return;
    }
}

static void
adv_post_nrf52_reset(void)
{
    LOG_WARN("nRF52 hw_reset: ON");
    nrf52fw_hw_reset_nrf52(true);
    os_mutex_lock(g_adv_post_nrf52_cfg.mutex);
    g_adv_post_nrf52_cfg.flag_nrf52_in_hw_reset_state = true;
    g_adv_post_nrf52_led_ctrl_time_interval_ms        = ADV_POST_LED_CTRL_TIME_INTERVAL_INVALID;
    g_adv_post_nrf52_flag_cfg_required                = false;
    g_adv_post_nrf52_is_waiting_ack                   = false;
    os_mutex_unlock(g_adv_post_nrf52_cfg.mutex);
    LOG_INFO("Start timer nrf52_reset");
    os_timer_sig_one_shot_start(g_adv_post_timer_sig_nrf52_reset);
}

void
adv_post_nrf52_init(void)
{
    assert(NULL == g_adv_post_nrf52_task_handle);
    g_adv_post_nrf52_task_handle = os_task_get_cur_task_handle();

    g_adv_post_nrf52_cfg.mutex                 = os_mutex_create_static(&g_adv_post_nrf52_cfg.mutex_mem);
    g_adv_post_nrf52_cfg.flag_nrf52_configured = false;

    const gw_cfg_t* p_gw_cfg    = gw_cfg_lock_ro();
    g_adv_post_nrf52_cfg.scan   = p_gw_cfg->ruuvi_cfg.scan;
    g_adv_post_nrf52_cfg.filter = p_gw_cfg->ruuvi_cfg.filter;
    gw_cfg_unlock_ro(&p_gw_cfg);
    g_adv_post_nrf52_cfg.flag_cfg_ready               = false;
    g_adv_post_nrf52_cfg.flag_nrf52_in_hw_reset_state = false;

    g_adv_post_nrf52_last_cmd                  = (re_ca_uart_cmd_t)0;
    g_adv_post_nrf52_is_waiting_ack            = false;
    g_adv_post_nrf52_flag_cfg_required         = false;
    g_adv_post_nrf52_ack_timeout_cnt           = 0;
    g_adv_post_nrf52_led_ctrl_time_interval_ms = ADV_POST_LED_CTRL_TIME_INTERVAL_INVALID;

    g_adv_post_timer_sig_nrf52_ack_timeout = os_timer_sig_one_shot_create_static(
        &g_adv_post_timer_sig_nrf52_ack_timeout_mem,
        "nrf_ack_timeout",
        adv_post_signals_get(),
        adv_post_conv_to_sig_num(ADV_POST_SIG_NRF52_ACK_TIMEOUT),
        pdMS_TO_TICKS(ADV_POST_NRF52_ACK_TIMEOUT_MS));

    g_adv_post_timer_sig_nrf52_reset = os_timer_sig_one_shot_create_static(
        &g_adv_post_timer_sig_nrf52_reset_mem,
        "nrf_reset",
        adv_post_signals_get(),
        adv_post_conv_to_sig_num(ADV_POST_SIG_NRF52_HW_RESET_OFF),
        pdMS_TO_TICKS(ADV_POST_NRF52_RESET_TIMEOUT_MS));

    g_adv_post_timer_sig_nrf52_cfg_req_timeout = os_timer_sig_one_shot_create_static(
        &g_adv_post_timer_sig_nrf52_cfg_req_timeout_mem,
        "nrf_cfg_req",
        adv_post_signals_get(),
        adv_post_conv_to_sig_num(ADV_POST_SIG_NRF52_CFG_REQ_TIMEOUT),
        pdMS_TO_TICKS(ADV_POST_NRF52_CFG_REQ_TIMEOUT_MS));

    adv_post_nrf52_reset();
}

/**
 * @brief Cache nRF52 configuration and activate adv_post thread to pass the configuration to nRF52.
 * @param p_scan - Scan configuration
 * @param p_filter - Filter configuration
 */
void
adv_post_nrf52_cfg_update(const ruuvi_gw_cfg_scan_t* const p_scan, const ruuvi_gw_cfg_filter_t* const p_filter)
{
    assert(NULL != g_adv_post_nrf52_cfg.mutex);
    LOG_INFO("Set new nRF52 configuration");

    adv_post_nrf52_cfg_update_cached(p_scan, p_filter);

    LOG_INFO("Clear adv_table");
    adv_table_clear();
    adv_post_signals_send_sig_nrf52_cfg_update();
}

void
adv_post_nrf52_send_led_ctrl(const uint16_t time_interval_ms)
{
    assert(g_adv_post_nrf52_task_handle == os_task_get_cur_task_handle());
    g_adv_post_nrf52_led_ctrl_time_interval_ms = time_interval_ms;
    if (!g_adv_post_nrf52_is_waiting_ack)
    {
        LOG_DBG("nRF52 LED CTRL: %u ms", time_interval_ms);
        adv_post_nrf52_send_cmd_led_ctrl_internal();
    }
    else
    {
        LOG_WARN("nRF52 LED CTRL: %u ms, but waiting for ACK", time_interval_ms);
    }
}

void
adv_post_nrf52_on_sig_nrf52_rebooted(void)
{
    LOG_DBG("Stop timer nrf52_cfg_req_timeout");
    g_adv_post_nrf52_led_ctrl_time_interval_ms = ADV_POST_LED_CTRL_TIME_INTERVAL_INVALID;
    g_adv_post_nrf52_flag_cfg_required         = false;
    g_adv_post_nrf52_is_waiting_ack            = false;

    if (adv_post_nrf52_cfg_is_ready())
    {
        LOG_INFO("nRF52 Recv sig nrf52_rebooted and cfg is ready");
        if (!os_timer_sig_one_shot_is_active(g_adv_post_timer_sig_nrf52_cfg_req_timeout))
        {
            metrics_nrf_self_reboot_cnt_inc();
        }
        adv_post_signals_send_sig_nrf52_cfg_update();
    }
    else
    {
        LOG_WARN("nRF52 Recv sig nrf52_rebooted, but cfg is not ready");
    }
    os_timer_sig_one_shot_stop(g_adv_post_timer_sig_nrf52_cfg_req_timeout);
}

void
adv_post_nrf52_on_sig_nrf52_configured(void)
{
    LOG_INFO("nRF52 has been configured");
}

void
adv_post_nrf52_on_sig_nrf52_cfg_update(void)
{
    assert(g_adv_post_nrf52_task_handle == os_task_get_cur_task_handle());

    adv_post_nrf52_cfg_set_unconfigured();
    g_adv_post_nrf52_flag_cfg_required = true;
    if (!g_adv_post_nrf52_is_waiting_ack)
    {
        LOG_INFO("nRF52 Recv sig nrf52_cfg_update");
        adv_post_nrf52_send_cmd_cfg_internal();
    }
    else
    {
        LOG_WARN("nRF52 Recv sig nrf52_cfg_update, but waiting for ACK");
    }
}

void
adv_post_nrf52_on_sig_nrf52_hw_reset_off(void)
{
    assert(g_adv_post_nrf52_task_handle == os_task_get_cur_task_handle());

    LOG_INFO("nRF52: on_sig_nrf52_hw_reset_off");

    LOG_INFO("nRF52 hw_reset: OFF");
    nrf52fw_hw_reset_nrf52(false);

    os_mutex_lock(g_adv_post_nrf52_cfg.mutex);
    g_adv_post_nrf52_cfg.flag_nrf52_in_hw_reset_state = false;
    os_mutex_unlock(g_adv_post_nrf52_cfg.mutex);

    LOG_INFO("Start timer nrf52_cfg_req_timeout");
    os_timer_sig_one_shot_start(g_adv_post_timer_sig_nrf52_cfg_req_timeout);
}

void
adv_post_nrf52_on_sig_nrf52_cfg_req_timeout(void)
{
    assert(g_adv_post_nrf52_task_handle == os_task_get_cur_task_handle());
    LOG_INFO("nRF52: on_sig_nrf52_cfg_req_timeout");
    metrics_nrf_ext_hw_reset_cnt_inc();
    adv_post_nrf52_reset();
}

static void
adv_post_nrf52_on_async_ack_led_ctrl(const re_ca_uart_ble_bool_t ack_state)
{
    if (0 == ack_state.state)
    {
        LOG_DBG("Got ACK from nRF52 for cmd=%s, status=%s", "RE_CA_UART_LED_CTRL", "OK");
        adv_post_signals_send_sig(ADV_POST_SIG_NRF52_ACK_LED_CTRL);
    }
    else
    {
        LOG_ERR("Got ACK from nRF52 for cmd=%s, status=%s", "RE_CA_UART_LED_CTRL", "ERROR");
    }
}

static void
adv_post_nrf52_on_async_ack_set_all(const re_ca_uart_ble_bool_t ack_state)
{
    if (0 == ack_state.state)
    {
        LOG_INFO("Got ACK from nRF52 for cmd=%s, status=%s", "RE_CA_UART_SET_ALL", "OK");
        adv_post_signals_send_sig(ADV_POST_SIG_NRF52_ACK_CFG);
    }
    else
    {
        LOG_ERR("Got ACK from nRF52 for cmd=%s, status=%s", "RE_CA_UART_SET_ALL", "ERROR");
    }
}

void
adv_post_nrf52_on_async_ack(const re_ca_uart_cmd_t cmd, const re_ca_uart_ble_bool_t ack_state)
{
    // This function is called asynchronously from uart_rx_task.
    // Here we use only read-only access to the global variable g_adv_post_nrf52_last_cmd.
    const re_ca_uart_cmd_t last_cmd = g_adv_post_nrf52_last_cmd;
    g_adv_post_nrf52_last_cmd       = 0;
    if (last_cmd != cmd)
    {
        LOG_ERR("Got ACK from nRF52 for cmd=%d, status=%d, but expected cmd=%d", cmd, ack_state.state, last_cmd);
        return;
    }

    switch (cmd)
    {
        case RE_CA_UART_LED_CTRL:
            adv_post_nrf52_on_async_ack_led_ctrl(ack_state);
            break;
        case RE_CA_UART_SET_ALL:
            adv_post_nrf52_on_async_ack_set_all(ack_state);
            break;
        default:
            LOG_ERR("Got ACK from nRF52 for unknown cmd=%d, status=%d", cmd, ack_state.state);
            break;
    }
}

void
adv_post_nrf52_on_sync_ack_led_ctrl(void)
{
    LOG_DBG("nRF52: Sync handle ACK for LED_CTRL");
    g_adv_post_nrf52_led_ctrl_time_interval_ms = ADV_POST_LED_CTRL_TIME_INTERVAL_INVALID;
    g_adv_post_nrf52_ack_timeout_cnt           = 0;
    os_timer_sig_one_shot_stop(g_adv_post_timer_sig_nrf52_ack_timeout);
    g_adv_post_nrf52_is_waiting_ack = false;
    if (g_adv_post_nrf52_flag_cfg_required)
    {
        adv_post_nrf52_send_cmd_cfg_internal();
    }
}

void
adv_post_nrf52_on_sync_ack_cfg(void)
{
    LOG_INFO("nRF52: Sync handle ACK for CFG");
    g_adv_post_nrf52_ack_timeout_cnt = 0;
    os_timer_sig_one_shot_stop(g_adv_post_timer_sig_nrf52_ack_timeout);
    g_adv_post_nrf52_is_waiting_ack    = false;
    g_adv_post_nrf52_flag_cfg_required = false;
    adv_post_nrf52_cfg_set_configured();
    LOG_INFO("Clear adv_table");
    adv_table_clear();
    LOG_INFO("Notify: EVENT_MGR_EV_NRF52_CONFIGURED");
    event_mgr_notify(EVENT_MGR_EV_NRF52_CONFIGURED);
    if (ADV_POST_LED_CTRL_TIME_INTERVAL_INVALID != g_adv_post_nrf52_led_ctrl_time_interval_ms)
    {
        adv_post_nrf52_send_cmd_led_ctrl_internal();
    }
}

void
adv_post_nrf52_on_sync_ack_timeout(void)
{
    g_adv_post_nrf52_is_waiting_ack = false;
    metrics_nrf_lost_ack_cnt_inc();
    g_adv_post_nrf52_ack_timeout_cnt += 1;
    LOG_WARN("nRF52 ACK timeout, cnt=%u", g_adv_post_nrf52_ack_timeout_cnt);
    if (g_adv_post_nrf52_ack_timeout_cnt >= ADV_POST_NRF52_NO_ACK_MAX_CNT)
    {
        LOG_ERR("nRF52 ACK timeout counter exceeds the limit, reset nRF52");
        g_adv_post_nrf52_ack_timeout_cnt = 0;
        metrics_nrf_ext_hw_reset_cnt_inc();
        adv_post_nrf52_reset();
    }
    else
    {
        if (g_adv_post_nrf52_flag_cfg_required)
        {
            adv_post_nrf52_send_cmd_cfg_internal();
        }
        else if (ADV_POST_LED_CTRL_TIME_INTERVAL_INVALID != g_adv_post_nrf52_led_ctrl_time_interval_ms)
        {
            adv_post_nrf52_send_cmd_led_ctrl_internal();
        }
        else
        {
            // MISRA C:2012, 15.7 - All if...else if constructs shall be terminated with an else statement
        }
    }
}
