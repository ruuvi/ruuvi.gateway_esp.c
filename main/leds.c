/**
 * @file leds.c
 * @author Jukka Saari
 * @date 2019-11-27
 * @copyright Ruuvi Innovations Ltd, license BSD-3-Clause.
 */

#include "leds.h"
#include <stdbool.h>
#include <assert.h>
#include "driver/ledc.h"
#include "driver/gpio.h"
#include <esp_task_wdt.h>
#include "os_task.h"
#include "os_signal.h"
#include "os_timer_sig.h"
#include "ruuvi_board_gwesp.h"
#include "gpio_switch_ctrl.h"
#include "esp_type_wrapper.h"
#include "event_mgr.h"
#include "leds_ctrl.h"
#include "leds_ctrl2.h"
#include "leds_blinking.h"
#include "nrf52fw.h"
#include "gw_cfg.h"

#define LOG_LOCAL_LEVEL LOG_LEVEL_INFO
#include "log.h"

static const char* TAG = "LEDS";

#define LED_PIN (RB_ESP32_GPIO_MUX_LED)

#define LEDC_HS_TIMER       LEDC_TIMER_1
#define LEDC_HS_MODE        LEDC_HIGH_SPEED_MODE
#define LEDC_HS_CH0_GPIO    LED_PIN
#define LEDC_HS_CH0_CHANNEL LEDC_CHANNEL_0

#define LEDC_FREQ_PWM_HZ    (5000)
#define LEDC_TEST_DUTY_OFF  (1024 /* 0% */)
#define LEDC_TEST_DUTY_ON   (1023 - 256 /* 256 / 1024 = 25% */)
#define LEDC_TEST_FADE_TIME (25)

#define LEDS_TASK_UPDATE_PERIOD_JUST_AFTER_REBOOT_MS (25U)
#define LEDS_TASK_UPDATE_PERIOD_MS                   (100U)
#define LEDS_TASK_WATCHDOG_PERIOD_MS                 (1000U)

#define LEDS_TASK_PRIORITY (8)

typedef enum leds_task_sig_e
{
    LEDS_TASK_SIG_UPDATE                             = OS_SIGNAL_NUM_0,
    LEDS_TASK_SIG_ON_EV_REBOOT                       = OS_SIGNAL_NUM_1,
    LEDS_TASK_SIG_ON_EV_NRF52_FW_CHECK               = OS_SIGNAL_NUM_2,
    LEDS_TASK_SIG_ON_EV_NRF52_FW_UPDATING            = OS_SIGNAL_NUM_3,
    LEDS_TASK_SIG_ON_EV_NRF52_READY                  = OS_SIGNAL_NUM_4,
    LEDS_TASK_SIG_ON_EV_NRF52_FAILURE                = OS_SIGNAL_NUM_5,
    LEDS_TASK_SIG_ON_EV_NRF52_REBOOTED               = OS_SIGNAL_NUM_6,
    LEDS_TASK_SIG_ON_EV_NRF52_CONFIGURED             = OS_SIGNAL_NUM_7,
    LEDS_TASK_SIG_ON_EV_CFG_READY                    = OS_SIGNAL_NUM_8,
    LEDS_TASK_SIG_ON_EV_CFG_CHANGED_RUUVI            = OS_SIGNAL_NUM_9,
    LEDS_TASK_SIG_ON_EV_CFG_ERASED                   = OS_SIGNAL_NUM_10,
    LEDS_TASK_SIG_ON_EV_WIFI_AP_STARTED              = OS_SIGNAL_NUM_11,
    LEDS_TASK_SIG_ON_EV_WIFI_AP_STOPPED              = OS_SIGNAL_NUM_12,
    LEDS_TASK_SIG_ON_EV_WPS_ACTIVATED                = OS_SIGNAL_NUM_13,
    LEDS_TASK_SIG_ON_EV_WPS_DEACTIVATED              = OS_SIGNAL_NUM_14,
    LEDS_TASK_SIG_ON_EV_NETWORK_CONNECTED            = OS_SIGNAL_NUM_15,
    LEDS_TASK_SIG_ON_EV_NETWORK_DISCONNECTED         = OS_SIGNAL_NUM_16,
    LEDS_TASK_SIG_ON_EV_MQTT1_CONNECTED              = OS_SIGNAL_NUM_17,
    LEDS_TASK_SIG_ON_EV_MQTT1_DISCONNECTED           = OS_SIGNAL_NUM_18,
    LEDS_TASK_SIG_ON_EV_HTTP1_DATA_SENT_SUCCESSFULLY = OS_SIGNAL_NUM_19,
    LEDS_TASK_SIG_ON_EV_HTTP1_DATA_SENT_FAIL         = OS_SIGNAL_NUM_20,
    LEDS_TASK_SIG_ON_EV_HTTP2_DATA_SENT_SUCCESSFULLY = OS_SIGNAL_NUM_21,
    LEDS_TASK_SIG_ON_EV_HTTP2_DATA_SENT_FAIL         = OS_SIGNAL_NUM_22,
    LEDS_TASK_SIG_ON_EV_HTTP_POLL_OK                 = OS_SIGNAL_NUM_23,
    LEDS_TASK_SIG_ON_EV_HTTP_POLL_TIMEOUT            = OS_SIGNAL_NUM_24,
    LEDS_TASK_SIG_ON_EV_RECV_ADV                     = OS_SIGNAL_NUM_25,
    LEDS_TASK_SIG_ON_EV_RECV_ADV_TIMEOUT             = OS_SIGNAL_NUM_26,
    LEDS_TASK_SIG_TASK_WATCHDOG_FEED                 = OS_SIGNAL_NUM_27,
} leds_task_sig_e;

#define LEDS_TASK_SIG_FIRST (LEDS_TASK_SIG_UPDATE)
#define LEDS_TASK_SIG_LAST  (LEDS_TASK_SIG_TASK_WATCHDOG_FEED)

typedef void (*leds_on_sig_handler_t)(void);

static ledc_channel_config_t ledc_channel[1] = {
    {
        .gpio_num   = LEDC_HS_CH0_GPIO,
        .speed_mode = LEDC_HS_MODE,
        .channel    = LEDC_HS_CH0_CHANNEL,
        .intr_type  = LEDC_INTR_DISABLE,
        .timer_sel  = LEDC_HS_TIMER,
        .duty       = 0,
        .hpoint     = 0,
    },
};

static os_signal_t*                   g_p_leds_signal;
static os_signal_static_t             g_leds_signal_mem;
static os_timer_sig_periodic_t*       g_p_leds_timer_sig_update;
static os_timer_sig_periodic_static_t g_leds_timer_sig_update_mem;
static os_timer_sig_periodic_t*       g_p_leds_timer_sig_watchdog_feed;
static os_timer_sig_periodic_static_t g_leds_timer_sig_watchdog_feed_mem;
static leds_color_e                   g_leds_state;
static bool                           g_green_led_state;
static os_mutex_t                     g_green_led_state_mutex;
static os_mutex_static_t              g_green_led_state_mutex_mem;

static event_mgr_ev_info_static_t g_leds_ev_info_mem_on_gw_cfg_ready;
static event_mgr_ev_info_static_t g_leds_ev_info_mem_on_gw_cfg_changed_ruuvi;
static event_mgr_ev_info_static_t g_leds_ev_info_mem_on_wifi_ap_started;
static event_mgr_ev_info_static_t g_leds_ev_info_mem_on_wifi_ap_stopped;
static event_mgr_ev_info_static_t g_leds_ev_info_mem_on_wps_activated;
static event_mgr_ev_info_static_t g_leds_ev_info_mem_on_wps_deactivated;
static event_mgr_ev_info_static_t g_leds_ev_info_mem_on_reboot;
static event_mgr_ev_info_static_t g_leds_ev_info_mem_on_wifi_connected;
static event_mgr_ev_info_static_t g_leds_ev_info_mem_on_wifi_disconnected;
static event_mgr_ev_info_static_t g_leds_ev_info_mem_on_eth_connected;
static event_mgr_ev_info_static_t g_leds_ev_info_mem_on_eth_disconnected;
static event_mgr_ev_info_static_t g_leds_ev_info_mem_on_recv_adv;
static event_mgr_ev_info_static_t g_leds_ev_info_mem_on_recv_adv_timeout;
static event_mgr_ev_info_static_t g_leds_ev_info_mem_on_nrf52_rebooted;
static event_mgr_ev_info_static_t g_leds_ev_info_mem_on_nrf52_configured;

ATTR_PURE
static os_signal_num_e
leds_task_conv_to_sig_num(const leds_task_sig_e sig)
{
    return (os_signal_num_e)sig;
}

static leds_task_sig_e
leds_task_conv_from_sig_num(const os_signal_num_e sig_num)
{
    assert(((os_signal_num_e)LEDS_TASK_SIG_FIRST <= sig_num) && (sig_num <= (os_signal_num_e)LEDS_TASK_SIG_LAST));
    return (leds_task_sig_e)sig_num;
}

static void
leds_task_watchdog_feed(void)
{
    LOG_DBG("Feed watchdog");
    const esp_err_t err = esp_task_wdt_reset();
    if (ESP_OK != err)
    {
        LOG_ERR_ESP(err, "%s failed", "esp_task_wdt_reset");
    }
}

static void
leds_set_green_led_state(const bool is_green_led_on)
{
    os_mutex_lock(g_green_led_state_mutex);
    g_green_led_state = is_green_led_on;
    os_mutex_unlock(g_green_led_state_mutex);
    event_mgr_notify(EVENT_MGR_EV_GREEN_LED_STATE_CHANGED);
}

bool
leds_get_green_led_state(void)
{
    os_mutex_lock(g_green_led_state_mutex);
    const bool is_green_led_on = g_green_led_state;
    os_mutex_unlock(g_green_led_state_mutex);
    return is_green_led_on;
}

static void
leds_turn_on_red(void)
{
    LOG_DBG("R_LED: ON");
    gpio_switch_ctrl_activate();
    ledc_set_fade_with_time(
        ledc_channel[0].speed_mode,
        ledc_channel[0].channel,
        LEDC_TEST_DUTY_ON,
        LEDC_TEST_FADE_TIME);
    ledc_fade_start(ledc_channel[0].speed_mode, ledc_channel[0].channel, LEDC_FADE_WAIT_DONE);
}

static void
leds_turn_off_red(void)
{
    LOG_DBG("R_LED: OFF");
    ledc_set_fade_with_time(
        ledc_channel[0].speed_mode,
        ledc_channel[0].channel,
        LEDC_TEST_DUTY_OFF,
        LEDC_TEST_FADE_TIME);
    ledc_fade_start(ledc_channel[0].speed_mode, ledc_channel[0].channel, LEDC_FADE_WAIT_DONE);
    gpio_switch_ctrl_deactivate();
}

static void
leds_turn_on_green(void)
{
    LOG_DBG("G_LED: ON");
    leds_set_green_led_state(true);
}

static void
leds_turn_off_green(void)
{
    LOG_DBG("G_LED: OFF");
    leds_set_green_led_state(false);
}

const char*
leds_color_to_str(const leds_color_e color)
{
    const char* p_color_str = "Unknown";
    switch (color)
    {
        case LEDS_COLOR_OFF:
            p_color_str = "OFF";
            break;
        case LEDS_COLOR_RED:
            p_color_str = "RED";
            break;
        case LEDS_COLOR_GREEN:
            p_color_str = "GREEN";
            break;
    }
    return p_color_str;
}

static void
leds_transition_from_off(const leds_color_e color)
{
    switch (color)
    {
        case LEDS_COLOR_OFF:
            assert(0);
            break;
        case LEDS_COLOR_RED:
            leds_turn_on_red();
            break;
        case LEDS_COLOR_GREEN:
            leds_turn_on_green();
            break;
    }
}

static void
leds_transition_from_red(const leds_color_e color)
{
    switch (color)
    {
        case LEDS_COLOR_OFF:
            leds_turn_off_red();
            break;
        case LEDS_COLOR_RED:
            assert(0);
            break;
        case LEDS_COLOR_GREEN:
            leds_turn_off_red();
            leds_turn_on_green();
            break;
    }
}

static void
leds_transition_from_green(const leds_color_e color)
{
    switch (color)
    {
        case LEDS_COLOR_OFF:
            leds_turn_off_green();
            break;
        case LEDS_COLOR_RED:
            leds_turn_off_green();
            leds_turn_on_red();
            break;
        case LEDS_COLOR_GREEN:
            assert(0);
            break;
    }
}

static void
leds_task_handle_sig_update(void)
{
    const leds_color_e prev_color = g_leds_state;
    const leds_color_e new_color  = leds_blinking_get_next();

    if (prev_color != new_color)
    {
        LOG_DBG("Transition: %s -> %s", leds_color_to_str(g_leds_state), leds_color_to_str(new_color));
        g_leds_state = new_color;
        switch (prev_color)
        {
            case LEDS_COLOR_OFF:
                leds_transition_from_off(new_color);
                break;
            case LEDS_COLOR_RED:
                leds_transition_from_red(new_color);
                break;
            case LEDS_COLOR_GREEN:
                leds_transition_from_green(new_color);
                break;
        }
    }
    if (leds_blinking_is_sequence_finished())
    {
        leds_blinking_set_new_sequence(leds_ctrl_get_new_blinking_sequence());
    }
}

static void
leds_task_handle_sig_on_ev_cfg_ready(void)
{
    LOG_INFO("LEDS_TASK_SIG_ON_EV_CFG_READY");
    const uint32_t flag_use_http_0 = gw_cfg_get_http_use_http_ruuvi() ? 1 : 0;
    const uint32_t flag_use_http_1 = gw_cfg_get_http_use_http() ? 1 : 0;
    leds_ctrl_configure_sub_machine((leds_ctrl_params_t) {
        .flag_use_mqtt        = gw_cfg_get_mqtt_use_mqtt(),
        .http_targets_bitmask = (flag_use_http_0 << 0U) | (flag_use_http_1 << 1U),
    });
    leds_ctrl_handle_event(LEDS_CTRL_EVENT_CFG_READY);
}

static void
leds_task_handle_sig_on_ev_cfg_changed_ruuvi(void)
{
    LOG_INFO("LEDS_TASK_SIG_ON_EV_CFG_CHANGED_RUUVI");
    const uint32_t flag_use_http_0 = gw_cfg_get_http_use_http_ruuvi() ? 1 : 0;
    const uint32_t flag_use_http_1 = gw_cfg_get_http_use_http() ? 1 : 0;
    leds_ctrl_configure_sub_machine((leds_ctrl_params_t) {
        .flag_use_mqtt        = gw_cfg_get_mqtt_use_mqtt(),
        .http_targets_bitmask = (flag_use_http_0 << 0U) | (flag_use_http_1 << 1U),
    });
}

static void
leds_task_handle_sig_on_ev_wifi_ap_started(void)
{
    const bool flag_is_in_substate = leds_ctrl_is_in_substate();
    LOG_INFO("LEDS_TASK_SIG_ON_EV_WIFI_AP_STARTED (ready=%d)", flag_is_in_substate);
    if (flag_is_in_substate)
    {
        leds_ctrl2_handle_event(LEDS_CTRL2_EVENT_WIFI_AP_STARTED);
    }
}

static void
leds_task_handle_sig_on_ev_wifi_ap_stopped(void)
{
    const bool flag_is_in_substate = leds_ctrl_is_in_substate();
    LOG_INFO("LEDS_TASK_SIG_ON_EV_WIFI_AP_STOPPED (ready=%d)", flag_is_in_substate);
    if (flag_is_in_substate)
    {
        leds_ctrl2_handle_event(LEDS_CTRL2_EVENT_WIFI_AP_STOPPED);
    }
}

static void
leds_task_handle_sig_on_ev_wps_activated(void)
{
    const bool flag_is_in_substate = leds_ctrl_is_in_substate();
    LOG_INFO("LEDS_TASK_SIG_ON_EV_WPS_ACTIVATED (ready=%d)", flag_is_in_substate);
    if (flag_is_in_substate)
    {
        leds_ctrl2_handle_event(LEDS_CTRL2_EVENT_WPS_ACTIVATED);
    }
}

static void
leds_task_handle_sig_on_ev_wps_deactivated(void)
{
    const bool flag_is_in_substate = leds_ctrl_is_in_substate();
    LOG_INFO("LEDS_TASK_SIG_ON_EV_WPS_DEACTIVATED (ready=%d)", flag_is_in_substate);
    if (flag_is_in_substate)
    {
        leds_ctrl2_handle_event(LEDS_CTRL2_EVENT_WPS_DEACTIVATED);
    }
}

static void
leds_task_handle_sig_on_ev_network_connected(void)
{
    const bool flag_is_in_substate = leds_ctrl_is_in_substate();
    LOG_INFO("LEDS_TASK_SIG_ON_EV_NETWORK_CONNECTED (ready=%d)", flag_is_in_substate);
    if (flag_is_in_substate)
    {
        leds_ctrl2_handle_event(LEDS_CTRL2_EVENT_NETWORK_CONNECTED);
    }
}

static void
leds_task_handle_sig_on_ev_network_disconnected(void)
{
    const bool flag_is_in_substate = leds_ctrl_is_in_substate();
    LOG_INFO("LEDS_TASK_SIG_ON_EV_NETWORK_DISCONNECTED (ready=%d)", flag_is_in_substate);
    if (flag_is_in_substate)
    {
        leds_ctrl2_handle_event(LEDS_CTRL2_EVENT_NETWORK_DISCONNECTED);
    }
}

static void
leds_task_handle_sig_on_ev_mqtt1_connected(void)
{
    const bool flag_is_in_substate = leds_ctrl_is_in_substate();
    LOG_INFO("LEDS_TASK_SIG_ON_EV_MQTT1_CONNECTED (ready=%d)", flag_is_in_substate);
    if (flag_is_in_substate)
    {
        leds_ctrl2_handle_event(LEDS_CTRL2_EVENT_MQTT1_CONNECTED);
    }
}

static void
leds_task_handle_sig_on_ev_mqtt1_disconnected(void)
{
    const bool flag_is_in_substate = leds_ctrl_is_in_substate();
    LOG_INFO("LEDS_TASK_SIG_ON_EV_MQTT1_DISCONNECTED (ready=%d)", flag_is_in_substate);
    if (flag_is_in_substate)
    {
        leds_ctrl2_handle_event(LEDS_CTRL2_EVENT_MQTT1_DISCONNECTED);
    }
}

static void
leds_task_handle_sig_on_ev_http1_data_sent_successfully(void)
{
    const bool flag_is_in_substate = leds_ctrl_is_in_substate();
    LOG_INFO("LEDS_TASK_SIG_ON_EV_HTTP1_DATA_SENT_SUCCESSFULLY (ready=%d)", flag_is_in_substate);
    if (flag_is_in_substate)
    {
        leds_ctrl2_handle_event(LEDS_CTRL2_EVENT_HTTP1_DATA_SENT_SUCCESSFULLY);
    }
}

static void
leds_task_handle_sig_on_ev_http1_data_sent_fail(void)
{
    const bool flag_is_in_substate = leds_ctrl_is_in_substate();
    LOG_WARN("LEDS_TASK_SIG_ON_EV_HTTP1_DATA_SENT_FAIL (ready=%d)", flag_is_in_substate);
    if (flag_is_in_substate)
    {
        leds_ctrl2_handle_event(LEDS_CTRL2_EVENT_HTTP1_DATA_SENT_FAIL);
    }
}

static void
leds_task_handle_sig_on_ev_http2_data_sent_successfully(void)
{
    const bool flag_is_in_substate = leds_ctrl_is_in_substate();
    LOG_INFO("LEDS_TASK_SIG_ON_EV_HTTP2_DATA_SENT_SUCCESSFULLY (ready=%d)", flag_is_in_substate);
    if (flag_is_in_substate)
    {
        leds_ctrl2_handle_event(LEDS_CTRL2_EVENT_HTTP2_DATA_SENT_SUCCESSFULLY);
    }
}

static void
leds_task_handle_sig_on_ev_http2_data_sent_fail(void)
{
    const bool flag_is_in_substate = leds_ctrl_is_in_substate();
    LOG_WARN("LEDS_TASK_SIG_ON_EV_HTTP2_DATA_SENT_FAIL (ready=%d)", flag_is_in_substate);
    if (flag_is_in_substate)
    {
        leds_ctrl2_handle_event(LEDS_CTRL2_EVENT_HTTP2_DATA_SENT_FAIL);
    }
}

static void
leds_task_handle_sig_on_ev_http_poll_ok(void)
{
    const bool flag_is_in_substate = leds_ctrl_is_in_substate();
    LOG_INFO("LEDS_TASK_SIG_ON_EV_HTTP_POLL_OK (ready=%d)", flag_is_in_substate);
    if (flag_is_in_substate)
    {
        leds_ctrl2_handle_event(LEDS_CTRL2_EVENT_HTTP_POLL_OK);
    }
}

static void
leds_task_handle_sig_on_ev_http_poll_timeout(void)
{
    const bool flag_is_in_substate = leds_ctrl_is_in_substate();
    LOG_INFO("LEDS_TASK_SIG_ON_EV_HTTP_POLL_TIMEOUT (ready=%d)", flag_is_in_substate);
    if (flag_is_in_substate)
    {
        leds_ctrl2_handle_event(LEDS_CTRL2_EVENT_HTTP_POLL_TIMEOUT);
    }
}

static void
leds_task_handle_sig_on_ev_recv_adv(void)
{
    const bool flag_is_in_substate = leds_ctrl_is_in_substate();
    LOG_DBG("LEDS_TASK_SIG_ON_EV_RECV_ADV (ready=%d)", flag_is_in_substate);
    if (flag_is_in_substate)
    {
        leds_ctrl2_handle_event(LEDS_CTRL2_EVENT_RECV_ADV);
    }
}

static void
leds_task_handle_sig_on_ev_recv_adv_timeout(void)
{
    const bool flag_is_in_substate = leds_ctrl_is_in_substate();
    LOG_WARN("LEDS_TASK_SIG_ON_EV_RECV_ADV_TIMEOUT (ready=%d)", flag_is_in_substate);
    if (flag_is_in_substate)
    {
        leds_ctrl2_handle_event(LEDS_CTRL2_EVENT_RECV_ADV_TIMEOUT);
    }
}

static void
leds_task_handle_sig_on_ev_reboot(void)
{
    LOG_INFO("LEDS_TASK_SIG_ON_EV_REBOOT");
    leds_ctrl_handle_event(LEDS_CTRL_EVENT_REBOOT);
}

static void
leds_task_handle_sig_on_ev_nrf52_fw_check(void)
{
    LOG_INFO("LEDS_TASK_SIG_ON_EV_NRF52_FW_CHECK");
    leds_ctrl_handle_event(LEDS_CTRL_EVENT_NRF52_FW_CHECK);
}

static void
leds_task_handle_sig_on_ev_nrf52_fw_updating(void)
{
    LOG_INFO("LEDS_TASK_SIG_ON_EV_NRF52_FW_UPDATING");
    leds_ctrl_handle_event(LEDS_CTRL_EVENT_NRF52_FW_UPDATING);
}

static void
leds_task_handle_sig_on_ev_nrf52_ready(void)
{
    LOG_INFO("LEDS_TASK_SIG_ON_EV_NRF52_READY");
    leds_ctrl_handle_event(LEDS_CTRL_EVENT_NRF52_READY);
}

static void
leds_task_handle_sig_on_ev_nrf52_failure(void)
{
    LOG_WARN("LEDS_TASK_SIG_ON_EV_NRF52_FAILURE");
    leds_ctrl_handle_event(LEDS_CTRL_EVENT_NRF52_FAILURE);
}

static void
leds_task_handle_sig_on_ev_nrf52_rebooted(void)
{
    const bool flag_gw_cfg_initialized = gw_cfg_is_initialized();
    LOG_INFO("LEDS_TASK_SIG_ON_EV_NRF52_REBOOTED: gw_cfg_initialized=%u", flag_gw_cfg_initialized);
    if (flag_gw_cfg_initialized)
    {
        leds_ctrl_handle_event(LEDS_CTRL_EVENT_NRF52_REBOOTED);
    }
}

static void
leds_task_handle_sig_on_ev_nrf52_configured(void)
{
    LOG_INFO("LEDS_TASK_SIG_ON_EV_NRF52_CONFIGURED");
    leds_ctrl_handle_event(LEDS_CTRL_EVENT_NRF52_CONFIGURED);
}

static void
leds_task_handle_sig_on_ev_cfg_erased(void)
{
    LOG_DBG("LEDS_TASK_SIG_ON_EV_CFG_ERASED");
    leds_ctrl_handle_event(LEDS_CTRL_EVENT_CFG_ERASED);
}

static void
leds_task_handle_sig(const leds_task_sig_e leds_task_sig)
{
    static const leds_on_sig_handler_t g_sig_handlers[LEDS_TASK_SIG_LAST + 1] = {
        [OS_SIGNAL_NUM_NONE]                               = NULL,
        [LEDS_TASK_SIG_UPDATE]                             = &leds_task_handle_sig_update,
        [LEDS_TASK_SIG_ON_EV_REBOOT]                       = &leds_task_handle_sig_on_ev_reboot,
        [LEDS_TASK_SIG_ON_EV_NRF52_FW_CHECK]               = &leds_task_handle_sig_on_ev_nrf52_fw_check,
        [LEDS_TASK_SIG_ON_EV_NRF52_FW_UPDATING]            = &leds_task_handle_sig_on_ev_nrf52_fw_updating,
        [LEDS_TASK_SIG_ON_EV_NRF52_READY]                  = &leds_task_handle_sig_on_ev_nrf52_ready,
        [LEDS_TASK_SIG_ON_EV_NRF52_FAILURE]                = &leds_task_handle_sig_on_ev_nrf52_failure,
        [LEDS_TASK_SIG_ON_EV_NRF52_REBOOTED]               = &leds_task_handle_sig_on_ev_nrf52_rebooted,
        [LEDS_TASK_SIG_ON_EV_NRF52_CONFIGURED]             = &leds_task_handle_sig_on_ev_nrf52_configured,
        [LEDS_TASK_SIG_ON_EV_CFG_READY]                    = &leds_task_handle_sig_on_ev_cfg_ready,
        [LEDS_TASK_SIG_ON_EV_CFG_CHANGED_RUUVI]            = &leds_task_handle_sig_on_ev_cfg_changed_ruuvi,
        [LEDS_TASK_SIG_ON_EV_CFG_ERASED]                   = &leds_task_handle_sig_on_ev_cfg_erased,
        [LEDS_TASK_SIG_ON_EV_WIFI_AP_STARTED]              = &leds_task_handle_sig_on_ev_wifi_ap_started,
        [LEDS_TASK_SIG_ON_EV_WIFI_AP_STOPPED]              = &leds_task_handle_sig_on_ev_wifi_ap_stopped,
        [LEDS_TASK_SIG_ON_EV_WPS_ACTIVATED]                = &leds_task_handle_sig_on_ev_wps_activated,
        [LEDS_TASK_SIG_ON_EV_WPS_DEACTIVATED]              = &leds_task_handle_sig_on_ev_wps_deactivated,
        [LEDS_TASK_SIG_ON_EV_NETWORK_CONNECTED]            = &leds_task_handle_sig_on_ev_network_connected,
        [LEDS_TASK_SIG_ON_EV_NETWORK_DISCONNECTED]         = &leds_task_handle_sig_on_ev_network_disconnected,
        [LEDS_TASK_SIG_ON_EV_MQTT1_CONNECTED]              = &leds_task_handle_sig_on_ev_mqtt1_connected,
        [LEDS_TASK_SIG_ON_EV_MQTT1_DISCONNECTED]           = &leds_task_handle_sig_on_ev_mqtt1_disconnected,
        [LEDS_TASK_SIG_ON_EV_HTTP1_DATA_SENT_SUCCESSFULLY] = &leds_task_handle_sig_on_ev_http1_data_sent_successfully,
        [LEDS_TASK_SIG_ON_EV_HTTP1_DATA_SENT_FAIL]         = &leds_task_handle_sig_on_ev_http1_data_sent_fail,
        [LEDS_TASK_SIG_ON_EV_HTTP2_DATA_SENT_SUCCESSFULLY] = &leds_task_handle_sig_on_ev_http2_data_sent_successfully,
        [LEDS_TASK_SIG_ON_EV_HTTP2_DATA_SENT_FAIL]         = &leds_task_handle_sig_on_ev_http2_data_sent_fail,
        [LEDS_TASK_SIG_ON_EV_HTTP_POLL_OK]                 = &leds_task_handle_sig_on_ev_http_poll_ok,
        [LEDS_TASK_SIG_ON_EV_HTTP_POLL_TIMEOUT]            = &leds_task_handle_sig_on_ev_http_poll_timeout,
        [LEDS_TASK_SIG_ON_EV_RECV_ADV]                     = &leds_task_handle_sig_on_ev_recv_adv,
        [LEDS_TASK_SIG_ON_EV_RECV_ADV_TIMEOUT]             = &leds_task_handle_sig_on_ev_recv_adv_timeout,
        [LEDS_TASK_SIG_TASK_WATCHDOG_FEED]                 = &leds_task_watchdog_feed,
    };
    if (leds_task_sig >= (sizeof(g_sig_handlers) / sizeof(g_sig_handlers[0])))
    {
        LOG_ERR("Unhanded sig: %d", (int)leds_task_sig);
        assert(0);
        return;
    }
    const leds_on_sig_handler_t p_sig_handler = g_sig_handlers[leds_task_sig];
    if (NULL == p_sig_handler)
    {
        LOG_ERR("Uninitialized sig handler: %d", (int)leds_task_sig);
        assert(0);
        return;
    }
    p_sig_handler();
}

static void
leds_wdt_add_and_start(void)
{
    LOG_INFO("TaskWatchdog: Register current thread");
    const esp_err_t err = esp_task_wdt_add(xTaskGetCurrentTaskHandle());
    if (ESP_OK != err)
    {
        LOG_ERR_ESP(err, "%s failed", "esp_task_wdt_add");
    }
    LOG_INFO("TaskWatchdog: Start timer");
    os_timer_sig_periodic_start(g_p_leds_timer_sig_watchdog_feed);
}

static void
leds_cb_on_enter_state_after_reboot(void)
{
    LOG_DBG("on_enter_state_after_reboot");
    LOG_DBG("Start SIG_UPDATE timer with period %ums", LEDS_TASK_UPDATE_PERIOD_JUST_AFTER_REBOOT_MS);
    os_timer_sig_periodic_restart_with_period(
        g_p_leds_timer_sig_update,
        pdMS_TO_TICKS(LEDS_TASK_UPDATE_PERIOD_JUST_AFTER_REBOOT_MS),
        false);
}

static void
leds_cb_on_exit_state_after_reboot(void)
{
    LOG_DBG("on_exit_state_after_reboot");
    LOG_DBG("Switch SIG_UPDATE timer to period %ums", LEDS_TASK_UPDATE_PERIOD_MS);
    os_timer_sig_periodic_restart_with_period(
        g_p_leds_timer_sig_update,
        pdMS_TO_TICKS(LEDS_TASK_UPDATE_PERIOD_MS),
        false);
}

static void
leds_cb_on_enter_state_before_reboot(void)
{
    LOG_DBG("on_enter_state_before_reboot");
    LOG_DBG("Stop SIG_UPDATE timer");
    os_timer_sig_periodic_stop(g_p_leds_timer_sig_update);
    leds_turn_off_red();
    nrf52fw_hw_reset_nrf52(true);
}

ATTR_NORETURN
static void
leds_task(const void* const p_param)
{
    const bool flag_configure_button_pressed = (bool)(intptr_t)p_param;
    LOG_INFO("%s started (flag_configure_button_pressed=%d)", __func__, flag_configure_button_pressed);

    leds_ctrl_init(
        flag_configure_button_pressed,
        (leds_ctrl_callbacks_t) {
            .cb_on_enter_state_after_reboot  = &leds_cb_on_enter_state_after_reboot,
            .cb_on_exit_state_after_reboot   = &leds_cb_on_exit_state_after_reboot,
            .cb_on_enter_state_before_reboot = &leds_cb_on_enter_state_before_reboot,
        });
    leds_blinking_init(leds_ctrl_get_new_blinking_sequence());

    os_signal_register_cur_thread(g_p_leds_signal);

    os_timer_sig_periodic_start(g_p_leds_timer_sig_update);

    leds_wdt_add_and_start();

    for (;;)
    {
        os_signal_events_t sig_events = { 0 };
        os_signal_wait(g_p_leds_signal, &sig_events);
        for (;;)
        {
            const os_signal_num_e sig_num = os_signal_num_get_next(&sig_events);
            if (OS_SIGNAL_NUM_NONE == sig_num)
            {
                break;
            }
            const leds_task_sig_e time_task_sig = leds_task_conv_from_sig_num(sig_num);
            leds_task_handle_sig(time_task_sig);
        }
    }
}

static void
leds_register_signals(void)
{
    os_signal_add(g_p_leds_signal, leds_task_conv_to_sig_num(LEDS_TASK_SIG_UPDATE));
    os_signal_add(g_p_leds_signal, leds_task_conv_to_sig_num(LEDS_TASK_SIG_ON_EV_REBOOT));
    os_signal_add(g_p_leds_signal, leds_task_conv_to_sig_num(LEDS_TASK_SIG_ON_EV_NRF52_FW_CHECK));
    os_signal_add(g_p_leds_signal, leds_task_conv_to_sig_num(LEDS_TASK_SIG_ON_EV_NRF52_FW_UPDATING));
    os_signal_add(g_p_leds_signal, leds_task_conv_to_sig_num(LEDS_TASK_SIG_ON_EV_NRF52_READY));
    os_signal_add(g_p_leds_signal, leds_task_conv_to_sig_num(LEDS_TASK_SIG_ON_EV_NRF52_FAILURE));
    os_signal_add(g_p_leds_signal, leds_task_conv_to_sig_num(LEDS_TASK_SIG_ON_EV_NRF52_REBOOTED));
    os_signal_add(g_p_leds_signal, leds_task_conv_to_sig_num(LEDS_TASK_SIG_ON_EV_NRF52_CONFIGURED));
    os_signal_add(g_p_leds_signal, leds_task_conv_to_sig_num(LEDS_TASK_SIG_ON_EV_CFG_READY));
    os_signal_add(g_p_leds_signal, leds_task_conv_to_sig_num(LEDS_TASK_SIG_ON_EV_CFG_CHANGED_RUUVI));
    os_signal_add(g_p_leds_signal, leds_task_conv_to_sig_num(LEDS_TASK_SIG_ON_EV_CFG_ERASED));
    os_signal_add(g_p_leds_signal, leds_task_conv_to_sig_num(LEDS_TASK_SIG_ON_EV_WIFI_AP_STARTED));
    os_signal_add(g_p_leds_signal, leds_task_conv_to_sig_num(LEDS_TASK_SIG_ON_EV_WIFI_AP_STOPPED));
    os_signal_add(g_p_leds_signal, leds_task_conv_to_sig_num(LEDS_TASK_SIG_ON_EV_WPS_ACTIVATED));
    os_signal_add(g_p_leds_signal, leds_task_conv_to_sig_num(LEDS_TASK_SIG_ON_EV_WPS_DEACTIVATED));
    os_signal_add(g_p_leds_signal, leds_task_conv_to_sig_num(LEDS_TASK_SIG_ON_EV_NETWORK_CONNECTED));
    os_signal_add(g_p_leds_signal, leds_task_conv_to_sig_num(LEDS_TASK_SIG_ON_EV_NETWORK_DISCONNECTED));
    os_signal_add(g_p_leds_signal, leds_task_conv_to_sig_num(LEDS_TASK_SIG_ON_EV_MQTT1_CONNECTED));
    os_signal_add(g_p_leds_signal, leds_task_conv_to_sig_num(LEDS_TASK_SIG_ON_EV_MQTT1_DISCONNECTED));
    os_signal_add(g_p_leds_signal, leds_task_conv_to_sig_num(LEDS_TASK_SIG_ON_EV_HTTP1_DATA_SENT_SUCCESSFULLY));
    os_signal_add(g_p_leds_signal, leds_task_conv_to_sig_num(LEDS_TASK_SIG_ON_EV_HTTP1_DATA_SENT_FAIL));
    os_signal_add(g_p_leds_signal, leds_task_conv_to_sig_num(LEDS_TASK_SIG_ON_EV_HTTP2_DATA_SENT_SUCCESSFULLY));
    os_signal_add(g_p_leds_signal, leds_task_conv_to_sig_num(LEDS_TASK_SIG_ON_EV_HTTP2_DATA_SENT_FAIL));
    os_signal_add(g_p_leds_signal, leds_task_conv_to_sig_num(LEDS_TASK_SIG_ON_EV_HTTP_POLL_OK));
    os_signal_add(g_p_leds_signal, leds_task_conv_to_sig_num(LEDS_TASK_SIG_ON_EV_HTTP_POLL_TIMEOUT));
    os_signal_add(g_p_leds_signal, leds_task_conv_to_sig_num(LEDS_TASK_SIG_ON_EV_RECV_ADV));
    os_signal_add(g_p_leds_signal, leds_task_conv_to_sig_num(LEDS_TASK_SIG_ON_EV_RECV_ADV_TIMEOUT));
    os_signal_add(g_p_leds_signal, leds_task_conv_to_sig_num(LEDS_TASK_SIG_TASK_WATCHDOG_FEED));
}

static void
leds_subscribe_events(void)
{
    event_mgr_subscribe_sig_static(
        &g_leds_ev_info_mem_on_gw_cfg_ready,
        EVENT_MGR_EV_GW_CFG_READY,
        g_p_leds_signal,
        leds_task_conv_to_sig_num(LEDS_TASK_SIG_ON_EV_CFG_READY));
    event_mgr_subscribe_sig_static(
        &g_leds_ev_info_mem_on_gw_cfg_changed_ruuvi,
        EVENT_MGR_EV_GW_CFG_CHANGED_RUUVI,
        g_p_leds_signal,
        leds_task_conv_to_sig_num(LEDS_TASK_SIG_ON_EV_CFG_CHANGED_RUUVI));
    event_mgr_subscribe_sig_static(
        &g_leds_ev_info_mem_on_wifi_ap_started,
        EVENT_MGR_EV_WIFI_AP_STARTED,
        g_p_leds_signal,
        leds_task_conv_to_sig_num(LEDS_TASK_SIG_ON_EV_WIFI_AP_STARTED));
    event_mgr_subscribe_sig_static(
        &g_leds_ev_info_mem_on_wifi_ap_stopped,
        EVENT_MGR_EV_WIFI_AP_STOPPED,
        g_p_leds_signal,
        leds_task_conv_to_sig_num(LEDS_TASK_SIG_ON_EV_WIFI_AP_STOPPED));
    event_mgr_subscribe_sig_static(
        &g_leds_ev_info_mem_on_wps_activated,
        EVENT_MGR_EV_WPS_ACTIVATED,
        g_p_leds_signal,
        leds_task_conv_to_sig_num(LEDS_TASK_SIG_ON_EV_WPS_ACTIVATED));
    event_mgr_subscribe_sig_static(
        &g_leds_ev_info_mem_on_wps_deactivated,
        EVENT_MGR_EV_WPS_DEACTIVATED,
        g_p_leds_signal,
        leds_task_conv_to_sig_num(LEDS_TASK_SIG_ON_EV_WPS_DEACTIVATED));
    event_mgr_subscribe_sig_static(
        &g_leds_ev_info_mem_on_reboot,
        EVENT_MGR_EV_REBOOT,
        g_p_leds_signal,
        leds_task_conv_to_sig_num(LEDS_TASK_SIG_ON_EV_REBOOT));
    event_mgr_subscribe_sig_static(
        &g_leds_ev_info_mem_on_wifi_connected,
        EVENT_MGR_EV_WIFI_CONNECTED,
        g_p_leds_signal,
        leds_task_conv_to_sig_num(LEDS_TASK_SIG_ON_EV_NETWORK_CONNECTED));
    event_mgr_subscribe_sig_static(
        &g_leds_ev_info_mem_on_wifi_disconnected,
        EVENT_MGR_EV_WIFI_DISCONNECTED,
        g_p_leds_signal,
        leds_task_conv_to_sig_num(LEDS_TASK_SIG_ON_EV_NETWORK_DISCONNECTED));
    event_mgr_subscribe_sig_static(
        &g_leds_ev_info_mem_on_eth_connected,
        EVENT_MGR_EV_ETH_CONNECTED,
        g_p_leds_signal,
        leds_task_conv_to_sig_num(LEDS_TASK_SIG_ON_EV_NETWORK_CONNECTED));
    event_mgr_subscribe_sig_static(
        &g_leds_ev_info_mem_on_eth_disconnected,
        EVENT_MGR_EV_ETH_DISCONNECTED,
        g_p_leds_signal,
        leds_task_conv_to_sig_num(LEDS_TASK_SIG_ON_EV_NETWORK_DISCONNECTED));
    event_mgr_subscribe_sig_static(
        &g_leds_ev_info_mem_on_recv_adv,
        EVENT_MGR_EV_RECV_ADV,
        g_p_leds_signal,
        leds_task_conv_to_sig_num(LEDS_TASK_SIG_ON_EV_RECV_ADV));
    event_mgr_subscribe_sig_static(
        &g_leds_ev_info_mem_on_recv_adv_timeout,
        EVENT_MGR_EV_RECV_ADV_TIMEOUT,
        g_p_leds_signal,
        leds_task_conv_to_sig_num(LEDS_TASK_SIG_ON_EV_RECV_ADV_TIMEOUT));
    event_mgr_subscribe_sig_static(
        &g_leds_ev_info_mem_on_nrf52_rebooted,
        EVENT_MGR_EV_NRF52_REBOOTED,
        g_p_leds_signal,
        leds_task_conv_to_sig_num(LEDS_TASK_SIG_ON_EV_NRF52_REBOOTED));
    event_mgr_subscribe_sig_static(
        &g_leds_ev_info_mem_on_nrf52_configured,
        EVENT_MGR_EV_NRF52_CONFIGURED,
        g_p_leds_signal,
        leds_task_conv_to_sig_num(LEDS_TASK_SIG_ON_EV_NRF52_CONFIGURED));
}

static void
leds_create_timers(void)
{
    g_p_leds_timer_sig_update = os_timer_sig_periodic_create_static(
        &g_leds_timer_sig_update_mem,
        "leds:update",
        g_p_leds_signal,
        leds_task_conv_to_sig_num(LEDS_TASK_SIG_UPDATE),
        pdMS_TO_TICKS(LEDS_TASK_UPDATE_PERIOD_MS));

    LOG_INFO("TaskWatchdog: leds_task: Create timer");
    g_p_leds_timer_sig_watchdog_feed = os_timer_sig_periodic_create_static(
        &g_leds_timer_sig_watchdog_feed_mem,
        "leds:wdog",
        g_p_leds_signal,
        leds_task_conv_to_sig_num(LEDS_TASK_SIG_TASK_WATCHDOG_FEED),
        pdMS_TO_TICKS(LEDS_TASK_WATCHDOG_PERIOD_MS));
}

bool
leds_init(const bool flag_configure_button_pressed)
{
    LOG_INFO("%s", __func__);

    g_leds_state = LEDS_COLOR_OFF;

    g_green_led_state       = false;
    g_green_led_state_mutex = os_mutex_create_static(&g_green_led_state_mutex_mem);

    g_p_leds_signal = os_signal_create_static(&g_leds_signal_mem);
    leds_register_signals();
    leds_subscribe_events();
    leds_create_timers();

    ledc_timer_config_t ledc_timer = {
        .duty_resolution = LEDC_TIMER_10_BIT, // resolution of PWM duty
        .freq_hz         = LEDC_FREQ_PWM_HZ,  // frequency of PWM signal
        .speed_mode      = LEDC_HS_MODE,      // timer mode
        .timer_num       = LEDC_HS_TIMER,     // timer index
        .clk_cfg         = LEDC_AUTO_CLK,     // Auto select the source clock
    };

    ledc_timer_config(&ledc_timer);
    ledc_channel_config(&ledc_channel[0]);
    ledc_fade_func_install(0);
    ledc_stop(ledc_channel[0].speed_mode, ledc_channel[0].channel, LEDC_TEST_DUTY_OFF);

    const uint32_t stack_size = 3U * 1024U;

    os_task_handle_t h_task = NULL;
    if (!os_task_create_with_const_param(
            &leds_task,
            "leds_task",
            stack_size,
            (const void*)(intptr_t)flag_configure_button_pressed,
            LEDS_TASK_PRIORITY,
            &h_task))
    {
        LOG_ERR("Can't create thread");
        return false;
    }
    while (!os_signal_is_any_thread_registered(g_p_leds_signal))
    {
        vTaskDelay(1);
    }
    return true;
}

void
leds_notify_nrf52_ready(void)
{
    LOG_INFO("Notify: nRF52 ready");
    if (!os_signal_send(g_p_leds_signal, leds_task_conv_to_sig_num(LEDS_TASK_SIG_ON_EV_NRF52_READY)))
    {
        LOG_ERR("%s failed", "os_signal_send");
    }
}

void
leds_notify_nrf52_failure(void)
{
    LOG_INFO("Notify: nRF52 failure");
    if (!os_signal_send(g_p_leds_signal, leds_task_conv_to_sig_num(LEDS_TASK_SIG_ON_EV_NRF52_FAILURE)))
    {
        LOG_ERR("%s failed", "os_signal_send");
    }
}

void
leds_notify_nrf52_fw_check(void)
{
    LOG_INFO("Notify: nRF52 fw_check");
    if (!os_signal_send(g_p_leds_signal, leds_task_conv_to_sig_num(LEDS_TASK_SIG_ON_EV_NRF52_FW_CHECK)))
    {
        LOG_ERR("%s failed", "os_signal_send");
    }
}

void
leds_notify_nrf52_fw_updating(void)
{
    LOG_INFO("Notify: nRF52 fw_updating");
    if (!os_signal_send(g_p_leds_signal, leds_task_conv_to_sig_num(LEDS_TASK_SIG_ON_EV_NRF52_FW_UPDATING)))
    {
        LOG_ERR("%s failed", "os_signal_send");
    }
}

void
leds_notify_cfg_erased(void)
{
    LOG_INFO("Notify: Cfg erased");
    if (!os_signal_send(g_p_leds_signal, leds_task_conv_to_sig_num(LEDS_TASK_SIG_ON_EV_CFG_ERASED)))
    {
        LOG_ERR("%s failed", "os_signal_send");
    }
}

void
leds_notify_mqtt1_connected(void)
{
    LOG_INFO("Notify: MQTT(1) connected");
    if (!os_signal_send(g_p_leds_signal, leds_task_conv_to_sig_num(LEDS_TASK_SIG_ON_EV_MQTT1_CONNECTED)))
    {
        LOG_ERR("%s failed", "os_signal_send");
    }
}

void
leds_notify_mqtt1_disconnected(void)
{
    LOG_INFO("Notify: MQTT(1) disconnected");
    if (!os_signal_send(g_p_leds_signal, leds_task_conv_to_sig_num(LEDS_TASK_SIG_ON_EV_MQTT1_DISCONNECTED)))
    {
        LOG_ERR("%s failed", "os_signal_send");
    }
}

void
leds_notify_http1_data_sent_successfully(void)
{
    LOG_INFO("Notify: HTTP(1) data sent successfully");
    if (!os_signal_send(g_p_leds_signal, leds_task_conv_to_sig_num(LEDS_TASK_SIG_ON_EV_HTTP1_DATA_SENT_SUCCESSFULLY)))
    {
        LOG_ERR("%s failed", "os_signal_send");
    }
}

void
leds_notify_http1_data_sent_fail(void)
{
    LOG_INFO("Notify: HTTP(1) data sent fail");
    if (!os_signal_send(g_p_leds_signal, leds_task_conv_to_sig_num(LEDS_TASK_SIG_ON_EV_HTTP1_DATA_SENT_FAIL)))
    {
        LOG_ERR("%s failed", "os_signal_send");
    }
}

void
leds_notify_http2_data_sent_successfully(void)
{
    LOG_INFO("Notify: HTTP(2) data sent successfully");
    if (!os_signal_send(g_p_leds_signal, leds_task_conv_to_sig_num(LEDS_TASK_SIG_ON_EV_HTTP2_DATA_SENT_SUCCESSFULLY)))
    {
        LOG_ERR("%s failed", "os_signal_send");
    }
}

void
leds_notify_http2_data_sent_fail(void)
{
    LOG_INFO("Notify: HTTP(2) data sent fail");
    if (!os_signal_send(g_p_leds_signal, leds_task_conv_to_sig_num(LEDS_TASK_SIG_ON_EV_HTTP2_DATA_SENT_FAIL)))
    {
        LOG_ERR("%s failed", "os_signal_send");
    }
}

void
leds_notify_http_poll_ok(void)
{
    LOG_INFO("Notify: HTTP_Poll ok");
    if (!os_signal_send(g_p_leds_signal, leds_task_conv_to_sig_num(LEDS_TASK_SIG_ON_EV_HTTP_POLL_OK)))
    {
        LOG_ERR("%s failed", "os_signal_send");
    }
}

void
leds_notify_http_poll_timeout(void)
{
    LOG_INFO("Notify: HTTP_Poll timeout");
    if (!os_signal_send(g_p_leds_signal, leds_task_conv_to_sig_num(LEDS_TASK_SIG_ON_EV_HTTP_POLL_TIMEOUT)))
    {
        LOG_ERR("%s failed", "os_signal_send");
    }
}

void
leds_simulate_ev_network_disconnected(void)
{
    os_signal_send(g_p_leds_signal, leds_task_conv_to_sig_num(LEDS_TASK_SIG_ON_EV_NETWORK_DISCONNECTED));
}
