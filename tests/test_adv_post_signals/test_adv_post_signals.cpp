/**
 * @file test_adv_post_signals.cpp
 * @author TheSomeMan
 * @date 2023-09-27
 * @copyright Ruuvi Innovations Ltd, license BSD-3-Clause.
 */

#include "adv_post_signals.h"
#include "gtest/gtest.h"
#include <string>
#include "esp_err.h"
#include "adv_table.h"
#include "adv_post_cfg_cache.h"
#include "adv_post_green_led.h"
#include "event_mgr.h"
#include "gw_cfg_default.h"

using namespace std;

class TestAdvPostSignals;

static TestAdvPostSignals* g_pTestClass;

/*** Google-test class implementation
 * *********************************************************************************/

enum event_type_e
{
    EVENT_HISTORY_OS_SIGNAL_CREATE_STATIC,
    EVENT_HISTORY_OS_SIGNAL_ADD,
    EVENT_HISTORY_OS_SIGNAL_SEND,
    EVENT_HISTORY_OS_SIGNAL_UNREGISTER_CUR_THREAD,
    EVENT_HISTORY_OS_SIGNAL_DELETE,
    EVENT_HISTORY_RELAUNCH_TIMER_SIG_RETRANSMIT_TO_HTTP_RUUVI,
    EVENT_HISTORY_RELAUNCH_TIMER_SIG_RETRANSMIT_TO_HTTP_CUSTOM,
    EVENT_HISTORY_RELAUNCH_TIMER_SIG_SEND_STATISTICS,
    EVENT_HISTORY_ADV_TABLE_CLEAR,
    EVENT_HISTORY_NETWORK_TIMEOUT_UPDATE_TIMESTAMP,
    EVENT_HISTORY_DO_ASYNC_COMM,
    EVENT_HISTORY_STOP_TIMER_SIG_DO_ASYNC_COMM,
    EVENT_HISTORY_HTTP_ABORT_ANY_REQ_DURING_PROCESSING,
    EVENT_HISTORY_HTTP_SERVER_MUTEX_ACTIVATE,
    EVENT_HISTORY_HTTP_SERVER_MUTEX_DEACTIVATE,
    EVENT_HISTORY_HTTP_SERVER_MUTEX_UNLOCK,
    EVENT_HISTORY_LEDS_NOTIFY_HTTP1_DATA_SENT_FAIL,
    EVENT_HISTORY_LEDS_NOTIFY_HTTP2_DATA_SENT_FAIL,
    EVENT_HISTORY_GW_STATUS_CLEAR_HTTP_RELAYING_CMD,
    EVENT_HISTORY_GATEWAY_RESTART,
    EVENT_HISTORY_ESP_TASK_WDT_RESET,
    EVENT_HISTORY_RUUVI_SEND_NRF_SETTINGS_FROM_GW_CFG,
    EVENT_HISTORY_ADV1_POST_TIMER_RESTART_WITH_DEFAULT_PERIOD,
    EVENT_HISTORY_ADV2_POST_TIMER_RESTART_WITH_DEFAULT_PERIOD,
    EVENT_HISTORY_ADV2_POST_TIMERS_SET_DEFAULT_PERIOD,
    EVENT_HISTORY_STOP_TIMER_SIG_RETRANSMIT_TO_HTTP_RUUVI,
    EVENT_HISTORY_STOP_TIMER_SIG_RETRANSMIT_TO_HTTP_CUSTOM,
    EVENT_HISTORY_RESTART_TIMER_SIG_MQTT,
    EVENT_HISTORY_STOP_TIMER_SIG_MQTT,
    EVENT_HISTORY_STOP_TIMER_SIG_SEND_STATISTICS,
    EVENT_HISTORY_ADV_POST_CFG_CACHE_MUTEX_LOCK,
    EVENT_HISTORY_ADV_POST_CFG_CACHE_MUTEX_UNLOCK,
    EVENT_HISTORY_ADV_POST_ON_GREEN_LED_UPDATE,
    EVENT_HISTORY_EVENT_MGR_NOTIFY,
    EVENT_HISTORY_ADV_POST_TIMERS_POSTPONE_SENDING_STATISTICS,
};

typedef struct event_info_t
{
    event_type_e event_type;
    union
    {
        struct
        {
            os_signal_num_e sig_num;
        } os_signal_add;
        struct
        {
            os_signal_num_e sig_num;
        } os_signal_send;
        struct
        {
            adv_post_green_led_cmd_e cmd;
        } green_led_update;
        struct
        {
            event_mgr_ev_e event;
        } event_mgr_notify;
    };
} event_info_t;

class MemAllocTrace
{
    vector<void*> allocated_mem;

    std::vector<void*>::iterator
    find(void* p_mem)
    {
        for (auto iter = this->allocated_mem.begin(); iter != this->allocated_mem.end(); ++iter)
        {
            if (*iter == p_mem)
            {
                return iter;
            }
        }
        return this->allocated_mem.end();
    }

public:
    void
    add(void* p_mem)
    {
        auto iter = find(p_mem);
        assert(iter == this->allocated_mem.end()); // p_mem was found in the list of allocated memory blocks
        this->allocated_mem.push_back(p_mem);
    }
    void
    remove(void* p_mem)
    {
        auto iter = find(p_mem);
        assert(iter != this->allocated_mem.end()); // p_mem was not found in the list of allocated memory blocks
        this->allocated_mem.erase(iter);
    }
    bool
    is_empty()
    {
        return this->allocated_mem.empty();
    }
};

class TestAdvPostSignals : public ::testing::Test
{
private:
protected:
    void
    SetUp() override
    {
        g_pTestClass               = this;
        this->m_malloc_cnt         = 0;
        this->m_malloc_fail_on_cnt = 0;

        gw_cfg_default_init_param_t init_params = {
            .wifi_ap_ssid        = { "" },
            .hostname            = { "" },
            .device_id           = {},
            .esp32_fw_ver        = { "v1.14.0" },
            .nrf52_fw_ver        = { "v1.0.0" },
            .nrf52_mac_addr      = {},
            .esp32_mac_addr_wifi = {},
            .esp32_mac_addr_eth  = {},
        };
        gw_cfg_default_init(&init_params, nullptr);
        gw_cfg_default_get(&this->m_gw_cfg);

        this->m_malloc_cnt         = 0;
        this->m_malloc_fail_on_cnt = 0;
        this->m_events_history.clear();

        this->m_gw_cfg.ruuvi_cfg.ntp.ntp_use                = true;
        this->m_gw_cfg.ruuvi_cfg.http.use_http_ruuvi        = true;
        this->m_gw_cfg.ruuvi_cfg.http.use_http              = false;
        this->m_gw_cfg.ruuvi_cfg.http.http_period           = 10;
        this->m_gw_cfg.ruuvi_cfg.http_stat.use_http_stat    = true;
        this->m_gw_cfg.ruuvi_cfg.mqtt.use_mqtt              = false;
        this->m_gw_cfg.ruuvi_cfg.mqtt.mqtt_sending_interval = 0;
        snprintf(
            this->m_gw_cfg.ruuvi_cfg.mqtt.mqtt_transport.buf,
            sizeof(this->m_gw_cfg.ruuvi_cfg.mqtt.mqtt_transport.buf),
            "%s",
            MQTT_TRANSPORT_TCP);

        this->m_gw_status_is_mqtt_connected                    = false;
        this->m_gw_status_is_relaying_via_http_enabled         = true;
        this->m_time_is_synchronized                           = true;
        this->m_metrics_received_advs                          = 0;
        this->adv_table_read_retransmission_list3_head_res     = true;
        this->adv_table_read_retransmission_list3_is_empty_res = true;
        this->m_adv_report                                     = {};
        this->mqtt_publish_adv_res                             = true;
        this->m_network_timeout_check_res                      = false;
        this->m_adv_post_cfg_cache                             = {};
        this->m_gw_cfg                                         = {};
    }

    void
    TearDown() override
    {
        gw_cfg_default_deinit();
        g_pTestClass = nullptr;
    }

public:
    TestAdvPostSignals();

    ~TestAdvPostSignals() override;

    MemAllocTrace m_mem_alloc_trace;
    uint32_t      m_malloc_cnt {};
    uint32_t      m_malloc_fail_on_cnt {};

    wifiman_config_t m_wifiman_default_config {};
    gw_cfg_t         m_gw_cfg {};

    bool m_gw_status_is_mqtt_connected { false };
    bool m_gw_status_is_relaying_via_http_enabled { true };

    bool                      m_time_is_synchronized { true };
    uint64_t                  m_metrics_received_advs { 0 };
    bool                      adv_table_read_retransmission_list3_head_res { true };
    bool                      adv_table_read_retransmission_list3_is_empty_res { true };
    adv_report_t              m_adv_report {};
    bool                      mqtt_publish_adv_res { true };
    bool                      m_network_timeout_check_res { false };
    adv_post_cfg_cache_t      m_adv_post_cfg_cache {};
    std::vector<event_info_t> m_events_history {};
};

TestAdvPostSignals::TestAdvPostSignals()
    : Test()
{
}

TestAdvPostSignals::~TestAdvPostSignals() = default;

extern "C" {

void*
os_malloc(const size_t size)
{
    assert(nullptr != g_pTestClass);
    if (++g_pTestClass->m_malloc_cnt == g_pTestClass->m_malloc_fail_on_cnt)
    {
        return nullptr;
    }
    void* p_mem = malloc(size);
    assert(nullptr != p_mem);
    g_pTestClass->m_mem_alloc_trace.add(p_mem);
    return p_mem;
}

void
os_free_internal(void* p_mem)
{
    assert(nullptr != g_pTestClass);
    g_pTestClass->m_mem_alloc_trace.remove(p_mem);
    free(p_mem);
}

void*
os_calloc(const size_t nmemb, const size_t size)
{
    assert(nullptr != g_pTestClass);
    if (++g_pTestClass->m_malloc_cnt == g_pTestClass->m_malloc_fail_on_cnt)
    {
        return nullptr;
    }
    void* p_mem = calloc(nmemb, size);
    assert(nullptr != p_mem);
    g_pTestClass->m_mem_alloc_trace.add(p_mem);
    return p_mem;
}

os_signal_t*
os_signal_create_static(os_signal_static_t* const p_signal_mem)
{
    assert(nullptr != p_signal_mem);
    g_pTestClass->m_events_history.push_back({ .event_type = EVENT_HISTORY_OS_SIGNAL_CREATE_STATIC });
    return reinterpret_cast<os_signal_t*>(p_signal_mem);
}

bool
os_signal_add(os_signal_t* const p_signal, const os_signal_num_e sig_num)
{
    assert(nullptr != p_signal);
    g_pTestClass->m_events_history.push_back(
        { .event_type = EVENT_HISTORY_OS_SIGNAL_ADD, .os_signal_add = { .sig_num = sig_num } });
    return true;
}

void
os_signal_unregister_cur_thread(os_signal_t* const p_signal)
{
    assert(nullptr != p_signal);
    g_pTestClass->m_events_history.push_back({ .event_type = EVENT_HISTORY_OS_SIGNAL_UNREGISTER_CUR_THREAD });
}

void
os_signal_delete(os_signal_t** const pp_signal)
{
    assert(nullptr != pp_signal);
    assert(nullptr != *pp_signal);
    g_pTestClass->m_events_history.push_back({ .event_type = EVENT_HISTORY_OS_SIGNAL_DELETE });
    *pp_signal = nullptr;
}

bool
os_signal_send(os_signal_t* const p_signal, const os_signal_num_e sig_num)
{
    assert(nullptr != p_signal);
    g_pTestClass->m_events_history.push_back(
        { .event_type = EVENT_HISTORY_OS_SIGNAL_SEND, .os_signal_add = { .sig_num = sig_num } });
    return true;
}

bool
os_signal_is_any_thread_registered(os_signal_t* const p_signal)
{
    return true;
}

const wifiman_config_t*
wifiman_default_config_init(const wifiman_wifi_ssid_t* const p_wifi_ssid, const wifiman_hostinfo_t* const p_hostinfo)
{
    return &g_pTestClass->m_wifiman_default_config;
}

void
wifiman_default_config_set(const wifiman_config_t* const p_wifi_cfg)
{
    g_pTestClass->m_wifiman_default_config = *p_wifi_cfg;
}

void
adv_post_timers_relaunch_timer_sig_retransmit_to_http_ruuvi(void)
{
    g_pTestClass->m_events_history.push_back(
        { .event_type = EVENT_HISTORY_RELAUNCH_TIMER_SIG_RETRANSMIT_TO_HTTP_RUUVI });
}

void
adv_post_timers_relaunch_timer_sig_retransmit_to_http_custom(void)
{
    g_pTestClass->m_events_history.push_back(
        { .event_type = EVENT_HISTORY_RELAUNCH_TIMER_SIG_RETRANSMIT_TO_HTTP_CUSTOM });
}

void
adv_post_timers_relaunch_timer_sig_send_statistics(void)
{
    g_pTestClass->m_events_history.push_back({ .event_type = EVENT_HISTORY_RELAUNCH_TIMER_SIG_SEND_STATISTICS });
}

void
adv_table_clear(void)
{
    g_pTestClass->m_events_history.push_back({ .event_type = EVENT_HISTORY_ADV_TABLE_CLEAR });
}

void
gw_cfg_log(const gw_cfg_t* const p_gw_cfg, const char* const p_title, const bool flag_log_device_info)
{
}

const gw_cfg_t*
gw_cfg_lock_ro(void)
{
    return &g_pTestClass->m_gw_cfg;
}

void
gw_cfg_unlock_ro(const gw_cfg_t** const p_p_gw_cfg)
{
    *p_p_gw_cfg = nullptr;
}

bool
gw_cfg_get_ntp_use(void)
{
    const gw_cfg_t* p_gw_cfg = gw_cfg_lock_ro();
    const bool      ntp_use  = p_gw_cfg->ruuvi_cfg.ntp.ntp_use;
    gw_cfg_unlock_ro(&p_gw_cfg);

    return ntp_use;
}

bool
gw_cfg_get_http_use_http_ruuvi(void)
{
    const gw_cfg_t* p_gw_cfg       = gw_cfg_lock_ro();
    const bool      use_http_ruuvi = p_gw_cfg->ruuvi_cfg.http.use_http_ruuvi;
    gw_cfg_unlock_ro(&p_gw_cfg);

    return use_http_ruuvi;
}

bool
gw_cfg_get_http_use_http(void)
{
    const gw_cfg_t* p_gw_cfg = gw_cfg_lock_ro();
    const bool      use_http = p_gw_cfg->ruuvi_cfg.http.use_http;
    gw_cfg_unlock_ro(&p_gw_cfg);

    return use_http;
}

uint32_t
gw_cfg_get_http_period(void)
{
    const gw_cfg_t* p_gw_cfg    = gw_cfg_lock_ro();
    const uint32_t  http_period = p_gw_cfg->ruuvi_cfg.http.http_period;
    gw_cfg_unlock_ro(&p_gw_cfg);

    return http_period;
}

bool
gw_cfg_get_http_stat_use_http_stat(void)
{
    const gw_cfg_t* p_gw_cfg      = gw_cfg_lock_ro();
    const bool      use_http_stat = p_gw_cfg->ruuvi_cfg.http_stat.use_http_stat;
    gw_cfg_unlock_ro(&p_gw_cfg);

    return use_http_stat;
}

bool
gw_cfg_get_mqtt_use_mqtt(void)
{
    const gw_cfg_t* p_gw_cfg = gw_cfg_lock_ro();
    const bool      use_mqtt = p_gw_cfg->ruuvi_cfg.mqtt.use_mqtt;
    gw_cfg_unlock_ro(&p_gw_cfg);

    return use_mqtt;
}

uint32_t
gw_cfg_get_mqtt_sending_interval(void)
{
    const gw_cfg_t* p_gw_cfg              = gw_cfg_lock_ro();
    const uint32_t  mqtt_sending_interval = p_gw_cfg->ruuvi_cfg.mqtt.mqtt_sending_interval;
    gw_cfg_unlock_ro(&p_gw_cfg);

    return mqtt_sending_interval;
}

bool
gw_cfg_get_mqtt_use_mqtt_over_ssl_or_wss(void)
{
    bool            res      = false;
    const gw_cfg_t* p_gw_cfg = gw_cfg_lock_ro();

    if (p_gw_cfg->ruuvi_cfg.mqtt.use_mqtt)
    {
        if (0 == strcmp(MQTT_TRANSPORT_SSL, p_gw_cfg->ruuvi_cfg.mqtt.mqtt_transport.buf))
        {
            res = true;
        }
        else if (0 == strcmp(MQTT_TRANSPORT_WSS, p_gw_cfg->ruuvi_cfg.mqtt.mqtt_transport.buf))
        {
            res = true;
        }
    }

    gw_cfg_unlock_ro(&p_gw_cfg);
    return res;
}

// bool
// gw_status_is_mqtt_connected(void)
//{
//     return g_pTestClass->m_gw_status_is_mqtt_connected;
// }

bool
gw_status_is_relaying_via_http_enabled(void)
{
    return g_pTestClass->m_gw_status_is_relaying_via_http_enabled;
}

void
gw_status_clear_http_relaying_cmd(void)
{
    g_pTestClass->m_events_history.push_back({ .event_type = EVENT_HISTORY_GW_STATUS_CLEAR_HTTP_RELAYING_CMD });
}

bool
time_is_synchronized(void)
{
    return g_pTestClass->m_time_is_synchronized;
}

uint64_t
metrics_received_advs_get(void)
{
    return g_pTestClass->m_metrics_received_advs;
}

bool
adv_table_read_retransmission_list3_head(adv_report_t* const p_adv_report)
{
    *p_adv_report = g_pTestClass->m_adv_report;
    return g_pTestClass->adv_table_read_retransmission_list3_head_res;
}

bool
adv_table_read_retransmission_list3_is_empty(void)
{
    return g_pTestClass->adv_table_read_retransmission_list3_is_empty_res;
}

bool
mqtt_publish_adv(const adv_report_t* const p_adv, const bool flag_use_timestamps, const time_t timestamp)
{
    return g_pTestClass->mqtt_publish_adv_res;
}

void
network_timeout_update_timestamp(void)
{
    g_pTestClass->m_events_history.push_back({ .event_type = EVENT_HISTORY_NETWORK_TIMEOUT_UPDATE_TIMESTAMP });
}

void
adv_post_timers_stop_timer_sig_do_async_comm(void)
{
    g_pTestClass->m_events_history.push_back({ .event_type = EVENT_HISTORY_STOP_TIMER_SIG_DO_ASYNC_COMM });
}

void
http_abort_any_req_during_processing(void)
{
    g_pTestClass->m_events_history.push_back({ .event_type = EVENT_HISTORY_HTTP_ABORT_ANY_REQ_DURING_PROCESSING });
}

void
http_server_mutex_activate(void)
{
    g_pTestClass->m_events_history.push_back({ .event_type = EVENT_HISTORY_HTTP_SERVER_MUTEX_ACTIVATE });
}

void
http_server_mutex_deactivate(void)
{
    g_pTestClass->m_events_history.push_back({ .event_type = EVENT_HISTORY_HTTP_SERVER_MUTEX_DEACTIVATE });
}

void
http_server_mutex_unlock(void)
{
    g_pTestClass->m_events_history.push_back({ .event_type = EVENT_HISTORY_HTTP_SERVER_MUTEX_UNLOCK });
}

void
leds_notify_http1_data_sent_fail(void)
{
    g_pTestClass->m_events_history.push_back({ .event_type = EVENT_HISTORY_LEDS_NOTIFY_HTTP1_DATA_SENT_FAIL });
}

void
leds_notify_http2_data_sent_fail(void)
{
    g_pTestClass->m_events_history.push_back({ .event_type = EVENT_HISTORY_LEDS_NOTIFY_HTTP2_DATA_SENT_FAIL });
}

bool
network_timeout_check(void)
{
    return g_pTestClass->m_network_timeout_check_res;
}

void
gateway_restart(const char* const p_msg)
{
    g_pTestClass->m_events_history.push_back({ .event_type = EVENT_HISTORY_GATEWAY_RESTART });
}

esp_err_t
esp_task_wdt_reset(void)
{
    g_pTestClass->m_events_history.push_back({ .event_type = EVENT_HISTORY_ESP_TASK_WDT_RESET });
    return ESP_OK;
}

adv_post_cfg_cache_t*
adv_post_cfg_cache_mutex_lock(void)
{
    g_pTestClass->m_events_history.push_back({ .event_type = EVENT_HISTORY_ADV_POST_CFG_CACHE_MUTEX_LOCK });
    return &g_pTestClass->m_adv_post_cfg_cache;
}

void
adv_post_cfg_cache_mutex_unlock(adv_post_cfg_cache_t** p_p_cfg_cache)
{
    g_pTestClass->m_events_history.push_back({ .event_type = EVENT_HISTORY_ADV_POST_CFG_CACHE_MUTEX_UNLOCK });
    *p_p_cfg_cache = nullptr;
}

void
adv1_post_timer_relaunch_with_default_period(void)
{
    g_pTestClass->m_events_history.push_back(
        { .event_type = EVENT_HISTORY_ADV1_POST_TIMER_RESTART_WITH_DEFAULT_PERIOD });
}

void
adv2_post_timer_relaunch_with_default_period(void)
{
    g_pTestClass->m_events_history.push_back(
        { .event_type = EVENT_HISTORY_ADV2_POST_TIMER_RESTART_WITH_DEFAULT_PERIOD });
}

void
adv2_post_timer_set_default_period(const uint32_t period_ms)
{
    g_pTestClass->m_events_history.push_back({ .event_type = EVENT_HISTORY_ADV2_POST_TIMERS_SET_DEFAULT_PERIOD });
}

void
adv1_post_timer_stop(void)
{
    g_pTestClass->m_events_history.push_back({ .event_type = EVENT_HISTORY_STOP_TIMER_SIG_RETRANSMIT_TO_HTTP_RUUVI });
}

void
adv2_post_timer_stop(void)
{
    g_pTestClass->m_events_history.push_back({ .event_type = EVENT_HISTORY_STOP_TIMER_SIG_RETRANSMIT_TO_HTTP_CUSTOM });
}

void
adv_post_timers_restart_timer_sig_mqtt(const os_delta_ticks_t delay_ticks)
{
    g_pTestClass->m_events_history.push_back({ .event_type = EVENT_HISTORY_RESTART_TIMER_SIG_MQTT });
}

void
adv_post_timers_stop_timer_sig_mqtt(void)
{
    g_pTestClass->m_events_history.push_back({ .event_type = EVENT_HISTORY_STOP_TIMER_SIG_MQTT });
}

void
adv_post_timers_stop_timer_sig_send_statistics(void)
{
    g_pTestClass->m_events_history.push_back({ .event_type = EVENT_HISTORY_STOP_TIMER_SIG_SEND_STATISTICS });
}

void
ruuvi_send_nrf_settings_from_gw_cfg(void)
{
    g_pTestClass->m_events_history.push_back({ .event_type = EVENT_HISTORY_RUUVI_SEND_NRF_SETTINGS_FROM_GW_CFG });
}

void
adv_post_on_green_led_update(const adv_post_green_led_cmd_e cmd)
{
    g_pTestClass->m_events_history.push_back(
        { .event_type = EVENT_HISTORY_ADV_POST_ON_GREEN_LED_UPDATE, .green_led_update = { .cmd = cmd } });
}

void
event_mgr_notify(const event_mgr_ev_e event)
{
    g_pTestClass->m_events_history.push_back(
        { .event_type = EVENT_HISTORY_EVENT_MGR_NOTIFY, .event_mgr_notify = { .event = event } });
}

void
adv_post_do_async_comm(adv_post_state_t* const p_adv_post_state)
{
    g_pTestClass->m_events_history.push_back({ .event_type = EVENT_HISTORY_DO_ASYNC_COMM });
}

void
adv_post_timers_postpone_sending_statistics(void)
{
    g_pTestClass->m_events_history.push_back(
        { .event_type = EVENT_HISTORY_ADV_POST_TIMERS_POSTPONE_SENDING_STATISTICS });
}

} // extern "C"

/*** Unit-Tests
 * *******************************************************************************************************/

TEST_F(TestAdvPostSignals, test_adv_post_signals_init_deinit) // NOLINT
{
    adv_post_signals_init();

    adv_post_signals_deinit();

    const std::vector<event_info_t> expected_history = {
        { .event_type = EVENT_HISTORY_OS_SIGNAL_CREATE_STATIC },
        { .event_type    = EVENT_HISTORY_OS_SIGNAL_ADD,
          .os_signal_add = { .sig_num = adv_post_conv_to_sig_num(ADV_POST_SIG_STOP) } },
        { .event_type    = EVENT_HISTORY_OS_SIGNAL_ADD,
          .os_signal_add = { .sig_num = adv_post_conv_to_sig_num(ADV_POST_SIG_NETWORK_DISCONNECTED) } },
        { .event_type    = EVENT_HISTORY_OS_SIGNAL_ADD,
          .os_signal_add = { .sig_num = adv_post_conv_to_sig_num(ADV_POST_SIG_NETWORK_CONNECTED) } },
        { .event_type    = EVENT_HISTORY_OS_SIGNAL_ADD,
          .os_signal_add = { .sig_num = adv_post_conv_to_sig_num(ADV_POST_SIG_TIME_SYNCHRONIZED) } },
        { .event_type    = EVENT_HISTORY_OS_SIGNAL_ADD,
          .os_signal_add = { .sig_num = adv_post_conv_to_sig_num(ADV_POST_SIG_RETRANSMIT) } },
        { .event_type    = EVENT_HISTORY_OS_SIGNAL_ADD,
          .os_signal_add = { .sig_num = adv_post_conv_to_sig_num(ADV_POST_SIG_RETRANSMIT2) } },
        { .event_type    = EVENT_HISTORY_OS_SIGNAL_ADD,
          .os_signal_add = { .sig_num = adv_post_conv_to_sig_num(ADV_POST_SIG_RETRANSMIT_MQTT) } },
        { .event_type    = EVENT_HISTORY_OS_SIGNAL_ADD,
          .os_signal_add = { .sig_num = adv_post_conv_to_sig_num(ADV_POST_SIG_SEND_STATISTICS) } },
        { .event_type    = EVENT_HISTORY_OS_SIGNAL_ADD,
          .os_signal_add = { .sig_num = adv_post_conv_to_sig_num(ADV_POST_SIG_DO_ASYNC_COMM) } },
        { .event_type    = EVENT_HISTORY_OS_SIGNAL_ADD,
          .os_signal_add = { .sig_num = adv_post_conv_to_sig_num(ADV_POST_SIG_RELAYING_MODE_CHANGED) } },
        { .event_type    = EVENT_HISTORY_OS_SIGNAL_ADD,
          .os_signal_add = { .sig_num = adv_post_conv_to_sig_num(ADV_POST_SIG_NETWORK_WATCHDOG) } },
        { .event_type    = EVENT_HISTORY_OS_SIGNAL_ADD,
          .os_signal_add = { .sig_num = adv_post_conv_to_sig_num(ADV_POST_SIG_TASK_WATCHDOG_FEED) } },
        { .event_type    = EVENT_HISTORY_OS_SIGNAL_ADD,
          .os_signal_add = { .sig_num = adv_post_conv_to_sig_num(ADV_POST_SIG_GW_CFG_READY) } },
        { .event_type    = EVENT_HISTORY_OS_SIGNAL_ADD,
          .os_signal_add = { .sig_num = adv_post_conv_to_sig_num(ADV_POST_SIG_GW_CFG_CHANGED_RUUVI) } },
        { .event_type    = EVENT_HISTORY_OS_SIGNAL_ADD,
          .os_signal_add = { .sig_num = adv_post_conv_to_sig_num(ADV_POST_SIG_BLE_SCAN_CHANGED) } },
        { .event_type    = EVENT_HISTORY_OS_SIGNAL_ADD,
          .os_signal_add = { .sig_num = adv_post_conv_to_sig_num(ADV_POST_SIG_CFG_MODE_ACTIVATED) } },
        { .event_type    = EVENT_HISTORY_OS_SIGNAL_ADD,
          .os_signal_add = { .sig_num = adv_post_conv_to_sig_num(ADV_POST_SIG_CFG_MODE_DEACTIVATED) } },
        { .event_type    = EVENT_HISTORY_OS_SIGNAL_ADD,
          .os_signal_add = { .sig_num = adv_post_conv_to_sig_num(ADV_POST_SIG_GREEN_LED_TURN_ON) } },
        { .event_type    = EVENT_HISTORY_OS_SIGNAL_ADD,
          .os_signal_add = { .sig_num = adv_post_conv_to_sig_num(ADV_POST_SIG_GREEN_LED_TURN_OFF) } },
        { .event_type    = EVENT_HISTORY_OS_SIGNAL_ADD,
          .os_signal_add = { .sig_num = adv_post_conv_to_sig_num(ADV_POST_SIG_GREEN_LED_UPDATE) } },
        { .event_type    = EVENT_HISTORY_OS_SIGNAL_ADD,
          .os_signal_add = { .sig_num = adv_post_conv_to_sig_num(ADV_POST_SIG_RECV_ADV_TIMEOUT) } },
        { .event_type = EVENT_HISTORY_OS_SIGNAL_UNREGISTER_CUR_THREAD },
        { .event_type = EVENT_HISTORY_OS_SIGNAL_DELETE },
    };
    for (uint32_t i = 0; i < expected_history.size(); ++i)
    {
        ASSERT_EQ(expected_history[i].event_type, this->m_events_history[i].event_type) << "Failed on cycle: " << i;
        switch (expected_history[i].event_type)
        {
            case EVENT_HISTORY_OS_SIGNAL_ADD:
                ASSERT_EQ(expected_history[i].os_signal_add.sig_num, this->m_events_history[i].os_signal_add.sig_num)
                    << "Failed on cycle: " << i;
                break;
            case EVENT_HISTORY_OS_SIGNAL_SEND:
                ASSERT_EQ(expected_history[i].os_signal_send.sig_num, this->m_events_history[i].os_signal_send.sig_num)
                    << "Failed on cycle: " << i;
                break;
            default:
                break;
        }
    }
    ASSERT_EQ(expected_history.size(), this->m_events_history.size())
        << "Expected: " << expected_history.size() << " events, but got: " << this->m_events_history.size();

    ASSERT_TRUE(this->m_mem_alloc_trace.is_empty());
}

TEST_F(TestAdvPostSignals, test_adv_post_signals_get) // NOLINT
{
    adv_post_signals_init();

    ASSERT_NE(nullptr, adv_post_signals_get());

    ASSERT_TRUE(this->m_mem_alloc_trace.is_empty());
}

TEST_F(TestAdvPostSignals, test_adv_post_conv_to_sig_num) // NOLINT
{
    ASSERT_EQ(OS_SIGNAL_NUM_0, adv_post_conv_to_sig_num(ADV_POST_SIG_STOP));
    ASSERT_EQ(OS_SIGNAL_NUM_1, adv_post_conv_to_sig_num(ADV_POST_SIG_NETWORK_DISCONNECTED));
    ASSERT_EQ(OS_SIGNAL_NUM_2, adv_post_conv_to_sig_num(ADV_POST_SIG_NETWORK_CONNECTED));
    ASSERT_EQ(OS_SIGNAL_NUM_3, adv_post_conv_to_sig_num(ADV_POST_SIG_TIME_SYNCHRONIZED));
    ASSERT_EQ(OS_SIGNAL_NUM_4, adv_post_conv_to_sig_num(ADV_POST_SIG_RETRANSMIT));
    ASSERT_EQ(OS_SIGNAL_NUM_5, adv_post_conv_to_sig_num(ADV_POST_SIG_RETRANSMIT2));
    ASSERT_EQ(OS_SIGNAL_NUM_6, adv_post_conv_to_sig_num(ADV_POST_SIG_RETRANSMIT_MQTT));
    ASSERT_EQ(OS_SIGNAL_NUM_7, adv_post_conv_to_sig_num(ADV_POST_SIG_SEND_STATISTICS));
    ASSERT_EQ(OS_SIGNAL_NUM_8, adv_post_conv_to_sig_num(ADV_POST_SIG_DO_ASYNC_COMM));
    ASSERT_EQ(OS_SIGNAL_NUM_9, adv_post_conv_to_sig_num(ADV_POST_SIG_RELAYING_MODE_CHANGED));
    ASSERT_EQ(OS_SIGNAL_NUM_10, adv_post_conv_to_sig_num(ADV_POST_SIG_NETWORK_WATCHDOG));
    ASSERT_EQ(OS_SIGNAL_NUM_11, adv_post_conv_to_sig_num(ADV_POST_SIG_TASK_WATCHDOG_FEED));
    ASSERT_EQ(OS_SIGNAL_NUM_12, adv_post_conv_to_sig_num(ADV_POST_SIG_GW_CFG_READY));
    ASSERT_EQ(OS_SIGNAL_NUM_13, adv_post_conv_to_sig_num(ADV_POST_SIG_GW_CFG_CHANGED_RUUVI));
    ASSERT_EQ(OS_SIGNAL_NUM_14, adv_post_conv_to_sig_num(ADV_POST_SIG_BLE_SCAN_CHANGED));
    ASSERT_EQ(OS_SIGNAL_NUM_15, adv_post_conv_to_sig_num(ADV_POST_SIG_CFG_MODE_ACTIVATED));
    ASSERT_EQ(OS_SIGNAL_NUM_16, adv_post_conv_to_sig_num(ADV_POST_SIG_CFG_MODE_DEACTIVATED));
    ASSERT_EQ(OS_SIGNAL_NUM_17, adv_post_conv_to_sig_num(ADV_POST_SIG_GREEN_LED_TURN_ON));
    ASSERT_EQ(OS_SIGNAL_NUM_18, adv_post_conv_to_sig_num(ADV_POST_SIG_GREEN_LED_TURN_OFF));
    ASSERT_EQ(OS_SIGNAL_NUM_19, adv_post_conv_to_sig_num(ADV_POST_SIG_GREEN_LED_UPDATE));
    ASSERT_EQ(OS_SIGNAL_NUM_20, adv_post_conv_to_sig_num(ADV_POST_SIG_RECV_ADV_TIMEOUT));

    ASSERT_TRUE(this->m_mem_alloc_trace.is_empty());
}

TEST_F(TestAdvPostSignals, test_adv_post_conv_from_sig_num) // NOLINT
{
    ASSERT_EQ(ADV_POST_SIG_STOP, adv_post_conv_from_sig_num(OS_SIGNAL_NUM_0));
    ASSERT_EQ(ADV_POST_SIG_NETWORK_DISCONNECTED, adv_post_conv_from_sig_num(OS_SIGNAL_NUM_1));
    ASSERT_EQ(ADV_POST_SIG_NETWORK_CONNECTED, adv_post_conv_from_sig_num(OS_SIGNAL_NUM_2));
    ASSERT_EQ(ADV_POST_SIG_TIME_SYNCHRONIZED, adv_post_conv_from_sig_num(OS_SIGNAL_NUM_3));
    ASSERT_EQ(ADV_POST_SIG_RETRANSMIT, adv_post_conv_from_sig_num(OS_SIGNAL_NUM_4));
    ASSERT_EQ(ADV_POST_SIG_RETRANSMIT2, adv_post_conv_from_sig_num(OS_SIGNAL_NUM_5));
    ASSERT_EQ(ADV_POST_SIG_RETRANSMIT_MQTT, adv_post_conv_from_sig_num(OS_SIGNAL_NUM_6));
    ASSERT_EQ(ADV_POST_SIG_SEND_STATISTICS, adv_post_conv_from_sig_num(OS_SIGNAL_NUM_7));
    ASSERT_EQ(ADV_POST_SIG_DO_ASYNC_COMM, adv_post_conv_from_sig_num(OS_SIGNAL_NUM_8));
    ASSERT_EQ(ADV_POST_SIG_RELAYING_MODE_CHANGED, adv_post_conv_from_sig_num(OS_SIGNAL_NUM_9));
    ASSERT_EQ(ADV_POST_SIG_NETWORK_WATCHDOG, adv_post_conv_from_sig_num(OS_SIGNAL_NUM_10));
    ASSERT_EQ(ADV_POST_SIG_TASK_WATCHDOG_FEED, adv_post_conv_from_sig_num(OS_SIGNAL_NUM_11));
    ASSERT_EQ(ADV_POST_SIG_GW_CFG_READY, adv_post_conv_from_sig_num(OS_SIGNAL_NUM_12));
    ASSERT_EQ(ADV_POST_SIG_GW_CFG_CHANGED_RUUVI, adv_post_conv_from_sig_num(OS_SIGNAL_NUM_13));
    ASSERT_EQ(ADV_POST_SIG_BLE_SCAN_CHANGED, adv_post_conv_from_sig_num(OS_SIGNAL_NUM_14));
    ASSERT_EQ(ADV_POST_SIG_CFG_MODE_ACTIVATED, adv_post_conv_from_sig_num(OS_SIGNAL_NUM_15));
    ASSERT_EQ(ADV_POST_SIG_CFG_MODE_DEACTIVATED, adv_post_conv_from_sig_num(OS_SIGNAL_NUM_16));
    ASSERT_EQ(ADV_POST_SIG_GREEN_LED_TURN_ON, adv_post_conv_from_sig_num(OS_SIGNAL_NUM_17));
    ASSERT_EQ(ADV_POST_SIG_GREEN_LED_TURN_OFF, adv_post_conv_from_sig_num(OS_SIGNAL_NUM_18));
    ASSERT_EQ(ADV_POST_SIG_GREEN_LED_UPDATE, adv_post_conv_from_sig_num(OS_SIGNAL_NUM_19));
    ASSERT_EQ(ADV_POST_SIG_RECV_ADV_TIMEOUT, adv_post_conv_from_sig_num(OS_SIGNAL_NUM_20));

    ASSERT_TRUE(this->m_mem_alloc_trace.is_empty());
}

TEST_F(TestAdvPostSignals, test_adv_post_signals_send_sig) // NOLINT
{
    adv_post_signals_init();
    this->m_events_history.clear();

    adv_post_signals_send_sig(ADV_POST_SIG_RETRANSMIT);

    const std::vector<event_info_t> expected_history = {
        { .event_type     = EVENT_HISTORY_OS_SIGNAL_SEND,
          .os_signal_send = { .sig_num = adv_post_conv_to_sig_num(ADV_POST_SIG_RETRANSMIT) } },
    };
    for (uint32_t i = 0; i < expected_history.size(); ++i)
    {
        ASSERT_EQ(expected_history[i].event_type, this->m_events_history[i].event_type) << "Failed on cycle: " << i;
        switch (expected_history[i].event_type)
        {
            case EVENT_HISTORY_OS_SIGNAL_ADD:
                ASSERT_EQ(expected_history[i].os_signal_add.sig_num, this->m_events_history[i].os_signal_add.sig_num)
                    << "Failed on cycle: " << i;
                break;
            case EVENT_HISTORY_OS_SIGNAL_SEND:
                ASSERT_EQ(expected_history[i].os_signal_send.sig_num, this->m_events_history[i].os_signal_send.sig_num)
                    << "Failed on cycle: " << i;
                break;
            default:
                break;
        }
    }
    ASSERT_EQ(expected_history.size(), this->m_events_history.size())
        << "Expected: " << expected_history.size() << " events, but got: " << this->m_events_history.size();

    ASSERT_TRUE(this->m_mem_alloc_trace.is_empty());
}

TEST_F(TestAdvPostSignals, test_adv_post_signals_is_thread_registered) // NOLINT
{
    adv_post_signals_init();

    ASSERT_TRUE(adv_post_signals_is_thread_registered());

    ASSERT_TRUE(this->m_mem_alloc_trace.is_empty());
}

TEST_F(TestAdvPostSignals, test_adv_post_handle_sig_stop) // NOLINT
{
    adv_post_signals_init();
    this->m_events_history.clear();

    adv_post_state_t adv_post_state = {
        .flag_stop = false,
    };
    ASSERT_TRUE(adv_post_handle_sig(ADV_POST_SIG_STOP, &adv_post_state));
    ASSERT_TRUE(adv_post_state.flag_stop);

    ASSERT_TRUE(this->m_mem_alloc_trace.is_empty());
}

TEST_F(TestAdvPostSignals, test_adv_post_handle_sig_network_disconnected) // NOLINT
{
    adv_post_signals_init();
    this->m_events_history.clear();

    adv_post_state_t adv_post_state = {
        .flag_primary_time_sync_is_done  = false,
        .flag_network_connected          = true,
        .flag_async_comm_in_progress     = false,
        .flag_need_to_send_advs1         = false,
        .flag_need_to_send_advs2         = false,
        .flag_need_to_send_statistics    = false,
        .flag_need_to_send_mqtt_periodic = false,
        .flag_relaying_enabled           = true,
        .flag_use_timestamps             = true,
        .flag_stop                       = false,
    };
    ASSERT_FALSE(adv_post_handle_sig(ADV_POST_SIG_NETWORK_DISCONNECTED, &adv_post_state));
    ASSERT_FALSE(adv_post_state.flag_stop);
    ASSERT_FALSE(adv_post_state.flag_network_connected);

    ASSERT_TRUE(this->m_mem_alloc_trace.is_empty());
}

TEST_F(TestAdvPostSignals, test_adv_post_handle_sig_network_connected_no_pending_transmissions) // NOLINT
{
    adv_post_signals_init();
    this->m_events_history.clear();

    adv_post_state_t adv_post_state = {
        .flag_primary_time_sync_is_done  = false,
        .flag_network_connected          = false,
        .flag_async_comm_in_progress     = false,
        .flag_need_to_send_advs1         = false,
        .flag_need_to_send_advs2         = false,
        .flag_need_to_send_statistics    = false,
        .flag_need_to_send_mqtt_periodic = false,
        .flag_relaying_enabled           = true,
        .flag_use_timestamps             = true,
        .flag_stop                       = false,
    };
    ASSERT_FALSE(adv_post_handle_sig(ADV_POST_SIG_NETWORK_CONNECTED, &adv_post_state));
    ASSERT_FALSE(adv_post_state.flag_stop);
    ASSERT_TRUE(adv_post_state.flag_network_connected);

    ASSERT_EQ(0, this->m_events_history.size());

    ASSERT_TRUE(this->m_mem_alloc_trace.is_empty());
}

TEST_F(TestAdvPostSignals, test_adv_post_handle_sig_network_connected_with_pending_transmissions) // NOLINT
{
    adv_post_signals_init();
    this->m_events_history.clear();

    adv_post_state_t adv_post_state = {
        .flag_primary_time_sync_is_done  = false,
        .flag_network_connected          = false,
        .flag_async_comm_in_progress     = false,
        .flag_need_to_send_advs1         = true,
        .flag_need_to_send_advs2         = true,
        .flag_need_to_send_statistics    = true,
        .flag_need_to_send_mqtt_periodic = true,
        .flag_relaying_enabled           = true,
        .flag_use_timestamps             = true,
        .flag_stop                       = false,
    };
    ASSERT_FALSE(adv_post_handle_sig(ADV_POST_SIG_NETWORK_CONNECTED, &adv_post_state));
    ASSERT_FALSE(adv_post_state.flag_stop);
    ASSERT_TRUE(adv_post_state.flag_network_connected);

    const std::vector<event_info_t> expected_history = {
        { .event_type = EVENT_HISTORY_RELAUNCH_TIMER_SIG_RETRANSMIT_TO_HTTP_RUUVI },
        { .event_type = EVENT_HISTORY_RELAUNCH_TIMER_SIG_RETRANSMIT_TO_HTTP_CUSTOM },
        { .event_type = EVENT_HISTORY_RELAUNCH_TIMER_SIG_SEND_STATISTICS },
    };
    for (uint32_t i = 0; i < expected_history.size(); ++i)
    {
        ASSERT_EQ(expected_history[i].event_type, this->m_events_history[i].event_type) << "Failed on cycle: " << i;
    }
    ASSERT_EQ(expected_history.size(), this->m_events_history.size())
        << "Expected: " << expected_history.size() << " events, but got: " << this->m_events_history.size();

    ASSERT_TRUE(this->m_mem_alloc_trace.is_empty());
}

TEST_F(TestAdvPostSignals, test_adv_post_handle_sig_time_synchronized) // NOLINT
{
    adv_post_signals_init();
    this->m_events_history.clear();

    adv_post_state_t adv_post_state = {
        .flag_primary_time_sync_is_done  = false,
        .flag_network_connected          = false,
        .flag_async_comm_in_progress     = false,
        .flag_need_to_send_advs1         = false,
        .flag_need_to_send_advs2         = false,
        .flag_need_to_send_statistics    = false,
        .flag_need_to_send_mqtt_periodic = false,
        .flag_relaying_enabled           = true,
        .flag_use_timestamps             = true,
        .flag_stop                       = false,
    };

    m_gw_cfg.ruuvi_cfg.http_stat.use_http_stat = false;

    ASSERT_FALSE(adv_post_handle_sig(ADV_POST_SIG_TIME_SYNCHRONIZED, &adv_post_state));
    ASSERT_FALSE(adv_post_state.flag_stop);
    ASSERT_TRUE(adv_post_state.flag_primary_time_sync_is_done);

    ASSERT_EQ(1, this->m_events_history.size());
    ASSERT_EQ(EVENT_HISTORY_ADV_TABLE_CLEAR, this->m_events_history[0].event_type);

    this->m_events_history.clear();
    ASSERT_FALSE(adv_post_handle_sig(ADV_POST_SIG_TIME_SYNCHRONIZED, &adv_post_state));
    ASSERT_EQ(0, this->m_events_history.size());

    this->m_events_history.clear();
    adv_post_state.flag_primary_time_sync_is_done = false;
    m_gw_cfg.ruuvi_cfg.http_stat.use_http_stat    = true;
    ASSERT_FALSE(adv_post_handle_sig(ADV_POST_SIG_TIME_SYNCHRONIZED, &adv_post_state));
    ASSERT_TRUE(adv_post_state.flag_primary_time_sync_is_done);
    ASSERT_EQ(2, this->m_events_history.size());
    ASSERT_EQ(EVENT_HISTORY_ADV_TABLE_CLEAR, this->m_events_history[0].event_type);
    ASSERT_EQ(EVENT_HISTORY_ADV_POST_TIMERS_POSTPONE_SENDING_STATISTICS, this->m_events_history[1].event_type);

    this->m_events_history.clear();
    adv_post_state.flag_need_to_send_advs1 = 1;
    ASSERT_FALSE(adv_post_handle_sig(ADV_POST_SIG_TIME_SYNCHRONIZED, &adv_post_state));
    ASSERT_EQ(1, this->m_events_history.size());
    ASSERT_EQ(EVENT_HISTORY_RELAUNCH_TIMER_SIG_RETRANSMIT_TO_HTTP_RUUVI, this->m_events_history[0].event_type);
    adv_post_state.flag_need_to_send_advs1 = 0;

    this->m_events_history.clear();
    adv_post_state.flag_need_to_send_advs2 = 1;
    ASSERT_FALSE(adv_post_handle_sig(ADV_POST_SIG_TIME_SYNCHRONIZED, &adv_post_state));
    ASSERT_EQ(1, this->m_events_history.size());
    ASSERT_EQ(EVENT_HISTORY_RELAUNCH_TIMER_SIG_RETRANSMIT_TO_HTTP_CUSTOM, this->m_events_history[0].event_type);
    adv_post_state.flag_need_to_send_advs2 = 0;

    this->m_events_history.clear();
    adv_post_state.flag_need_to_send_statistics = 1;
    ASSERT_FALSE(adv_post_handle_sig(ADV_POST_SIG_TIME_SYNCHRONIZED, &adv_post_state));
    ASSERT_EQ(1, this->m_events_history.size());
    ASSERT_EQ(EVENT_HISTORY_RELAUNCH_TIMER_SIG_SEND_STATISTICS, this->m_events_history[0].event_type);
    adv_post_state.flag_need_to_send_statistics = 0;

    ASSERT_TRUE(this->m_mem_alloc_trace.is_empty());
}

TEST_F(TestAdvPostSignals, test_adv_post_handle_sig_retransmit) // NOLINT
{
    adv_post_signals_init();
    this->m_events_history.clear();

    adv_post_state_t adv_post_state = {
        .flag_primary_time_sync_is_done  = false,
        .flag_network_connected          = false,
        .flag_async_comm_in_progress     = false,
        .flag_need_to_send_advs1         = false,
        .flag_need_to_send_advs2         = false,
        .flag_need_to_send_statistics    = false,
        .flag_need_to_send_mqtt_periodic = false,
        .flag_relaying_enabled           = true,
        .flag_use_timestamps             = true,
        .flag_stop                       = false,
    };

    adv_post_state.flag_relaying_enabled         = false;
    this->m_gw_cfg.ruuvi_cfg.http.use_http_ruuvi = false;
    ASSERT_FALSE(adv_post_handle_sig(ADV_POST_SIG_RETRANSMIT, &adv_post_state));
    ASSERT_FALSE(adv_post_state.flag_stop);
    ASSERT_FALSE(adv_post_state.flag_need_to_send_advs1);
    ASSERT_EQ(0, this->m_events_history.size());

    adv_post_state.flag_relaying_enabled         = true;
    this->m_gw_cfg.ruuvi_cfg.http.use_http_ruuvi = false;
    ASSERT_FALSE(adv_post_handle_sig(ADV_POST_SIG_RETRANSMIT, &adv_post_state));
    ASSERT_FALSE(adv_post_state.flag_stop);
    ASSERT_FALSE(adv_post_state.flag_need_to_send_advs1);
    ASSERT_EQ(0, this->m_events_history.size());

    adv_post_state.flag_relaying_enabled         = false;
    this->m_gw_cfg.ruuvi_cfg.http.use_http_ruuvi = true;
    ASSERT_FALSE(adv_post_handle_sig(ADV_POST_SIG_RETRANSMIT, &adv_post_state));
    ASSERT_FALSE(adv_post_state.flag_stop);
    ASSERT_FALSE(adv_post_state.flag_need_to_send_advs1);
    ASSERT_EQ(0, this->m_events_history.size());

    adv_post_state.flag_relaying_enabled         = true;
    this->m_gw_cfg.ruuvi_cfg.http.use_http_ruuvi = true;
    ASSERT_FALSE(adv_post_handle_sig(ADV_POST_SIG_RETRANSMIT, &adv_post_state));
    ASSERT_FALSE(adv_post_state.flag_stop);
    ASSERT_TRUE(adv_post_state.flag_need_to_send_advs1);
    ASSERT_EQ(1, this->m_events_history.size());
    ASSERT_EQ(EVENT_HISTORY_OS_SIGNAL_SEND, this->m_events_history[0].event_type);
    ASSERT_EQ(adv_post_conv_to_sig_num(ADV_POST_SIG_DO_ASYNC_COMM), this->m_events_history[0].os_signal_send.sig_num);

    ASSERT_TRUE(this->m_mem_alloc_trace.is_empty());
}

TEST_F(TestAdvPostSignals, test_adv_post_handle_sig_retransmit2) // NOLINT
{
    adv_post_signals_init();
    this->m_events_history.clear();

    adv_post_state_t adv_post_state = {
        .flag_primary_time_sync_is_done  = false,
        .flag_network_connected          = false,
        .flag_async_comm_in_progress     = false,
        .flag_need_to_send_advs1         = false,
        .flag_need_to_send_advs2         = false,
        .flag_need_to_send_statistics    = false,
        .flag_need_to_send_mqtt_periodic = false,
        .flag_relaying_enabled           = true,
        .flag_use_timestamps             = true,
        .flag_stop                       = false,
    };

    adv_post_state.flag_relaying_enabled   = false;
    this->m_gw_cfg.ruuvi_cfg.http.use_http = false;
    ASSERT_FALSE(adv_post_handle_sig(ADV_POST_SIG_RETRANSMIT2, &adv_post_state));
    ASSERT_FALSE(adv_post_state.flag_stop);
    ASSERT_FALSE(adv_post_state.flag_need_to_send_advs2);
    ASSERT_EQ(0, this->m_events_history.size());

    adv_post_state.flag_relaying_enabled   = true;
    this->m_gw_cfg.ruuvi_cfg.http.use_http = false;
    ASSERT_FALSE(adv_post_handle_sig(ADV_POST_SIG_RETRANSMIT2, &adv_post_state));
    ASSERT_FALSE(adv_post_state.flag_stop);
    ASSERT_FALSE(adv_post_state.flag_need_to_send_advs2);
    ASSERT_EQ(0, this->m_events_history.size());

    adv_post_state.flag_relaying_enabled   = false;
    this->m_gw_cfg.ruuvi_cfg.http.use_http = true;
    ASSERT_FALSE(adv_post_handle_sig(ADV_POST_SIG_RETRANSMIT2, &adv_post_state));
    ASSERT_FALSE(adv_post_state.flag_stop);
    ASSERT_FALSE(adv_post_state.flag_need_to_send_advs2);
    ASSERT_EQ(0, this->m_events_history.size());

    adv_post_state.flag_relaying_enabled   = true;
    this->m_gw_cfg.ruuvi_cfg.http.use_http = true;
    ASSERT_FALSE(adv_post_handle_sig(ADV_POST_SIG_RETRANSMIT2, &adv_post_state));
    ASSERT_FALSE(adv_post_state.flag_stop);
    ASSERT_TRUE(adv_post_state.flag_need_to_send_advs2);
    ASSERT_EQ(1, this->m_events_history.size());
    ASSERT_EQ(EVENT_HISTORY_OS_SIGNAL_SEND, this->m_events_history[0].event_type);
    ASSERT_EQ(adv_post_conv_to_sig_num(ADV_POST_SIG_DO_ASYNC_COMM), this->m_events_history[0].os_signal_send.sig_num);

    ASSERT_TRUE(this->m_mem_alloc_trace.is_empty());
}

TEST_F(TestAdvPostSignals, test_adv_post_handle_sig_retransmit_mqtt) // NOLINT
{
    adv_post_signals_init();
    this->m_events_history.clear();

    adv_post_state_t adv_post_state = {
        .flag_primary_time_sync_is_done  = true,
        .flag_network_connected          = true,
        .flag_async_comm_in_progress     = false,
        .flag_need_to_send_advs1         = false,
        .flag_need_to_send_advs2         = false,
        .flag_need_to_send_statistics    = false,
        .flag_need_to_send_mqtt_periodic = false,
        .flag_relaying_enabled           = true,
        .flag_use_timestamps             = true,
        .flag_stop                       = false,
    };

    adv_post_state.flag_relaying_enabled   = false;
    this->m_gw_cfg.ruuvi_cfg.mqtt.use_mqtt = false;
    ASSERT_FALSE(adv_post_handle_sig(ADV_POST_SIG_RETRANSMIT_MQTT, &adv_post_state));
    ASSERT_FALSE(adv_post_state.flag_stop);
    ASSERT_FALSE(adv_post_state.flag_need_to_send_mqtt_periodic);
    ASSERT_EQ(0, this->m_events_history.size());

    adv_post_state.flag_relaying_enabled   = true;
    this->m_gw_cfg.ruuvi_cfg.mqtt.use_mqtt = false;
    ASSERT_FALSE(adv_post_handle_sig(ADV_POST_SIG_RETRANSMIT_MQTT, &adv_post_state));
    ASSERT_FALSE(adv_post_state.flag_stop);
    ASSERT_FALSE(adv_post_state.flag_need_to_send_mqtt_periodic);
    ASSERT_EQ(0, this->m_events_history.size());

    adv_post_state.flag_relaying_enabled   = false;
    this->m_gw_cfg.ruuvi_cfg.mqtt.use_mqtt = true;
    ASSERT_FALSE(adv_post_handle_sig(ADV_POST_SIG_RETRANSMIT_MQTT, &adv_post_state));
    ASSERT_FALSE(adv_post_state.flag_stop);
    ASSERT_FALSE(adv_post_state.flag_need_to_send_mqtt_periodic);
    ASSERT_EQ(0, this->m_events_history.size());

    adv_post_state.flag_relaying_enabled   = true;
    this->m_gw_cfg.ruuvi_cfg.mqtt.use_mqtt = true;
    ASSERT_FALSE(adv_post_handle_sig(ADV_POST_SIG_RETRANSMIT_MQTT, &adv_post_state));
    ASSERT_FALSE(adv_post_state.flag_stop);
    ASSERT_TRUE(adv_post_state.flag_need_to_send_mqtt_periodic);
    ASSERT_EQ(1, this->m_events_history.size());
    ASSERT_EQ(EVENT_HISTORY_OS_SIGNAL_SEND, this->m_events_history[0].event_type);
    ASSERT_EQ(adv_post_conv_to_sig_num(ADV_POST_SIG_DO_ASYNC_COMM), this->m_events_history[0].os_signal_send.sig_num);

    ASSERT_TRUE(this->m_mem_alloc_trace.is_empty());
}

TEST_F(TestAdvPostSignals, test_adv_post_handle_sig_send_statistics) // NOLINT
{
    adv_post_signals_init();
    this->m_events_history.clear();

    adv_post_state_t adv_post_state = {
        .flag_primary_time_sync_is_done  = true,
        .flag_network_connected          = true,
        .flag_async_comm_in_progress     = false,
        .flag_need_to_send_advs1         = false,
        .flag_need_to_send_advs2         = false,
        .flag_need_to_send_statistics    = false,
        .flag_need_to_send_mqtt_periodic = false,
        .flag_relaying_enabled           = true,
        .flag_use_timestamps             = true,
        .flag_stop                       = false,
    };

    adv_post_state.flag_relaying_enabled             = false;
    this->m_gw_cfg.ruuvi_cfg.http_stat.use_http_stat = false;
    ASSERT_FALSE(adv_post_handle_sig(ADV_POST_SIG_SEND_STATISTICS, &adv_post_state));
    ASSERT_FALSE(adv_post_state.flag_stop);
    ASSERT_FALSE(adv_post_state.flag_need_to_send_statistics);
    ASSERT_EQ(0, this->m_events_history.size());

    adv_post_state.flag_relaying_enabled             = true;
    this->m_gw_cfg.ruuvi_cfg.http_stat.use_http_stat = false;
    ASSERT_FALSE(adv_post_handle_sig(ADV_POST_SIG_SEND_STATISTICS, &adv_post_state));
    ASSERT_FALSE(adv_post_state.flag_stop);
    ASSERT_FALSE(adv_post_state.flag_need_to_send_statistics);
    ASSERT_EQ(0, this->m_events_history.size());

    adv_post_state.flag_relaying_enabled             = false;
    this->m_gw_cfg.ruuvi_cfg.http_stat.use_http_stat = true;
    ASSERT_FALSE(adv_post_handle_sig(ADV_POST_SIG_SEND_STATISTICS, &adv_post_state));
    ASSERT_FALSE(adv_post_state.flag_stop);
    ASSERT_FALSE(adv_post_state.flag_need_to_send_statistics);
    ASSERT_EQ(0, this->m_events_history.size());

    adv_post_state.flag_relaying_enabled             = true;
    this->m_gw_cfg.ruuvi_cfg.http_stat.use_http_stat = true;
    ASSERT_FALSE(adv_post_handle_sig(ADV_POST_SIG_SEND_STATISTICS, &adv_post_state));
    ASSERT_FALSE(adv_post_state.flag_stop);
    ASSERT_TRUE(adv_post_state.flag_need_to_send_statistics);
    ASSERT_EQ(1, this->m_events_history.size());
    ASSERT_EQ(EVENT_HISTORY_OS_SIGNAL_SEND, this->m_events_history[0].event_type);
    ASSERT_EQ(adv_post_conv_to_sig_num(ADV_POST_SIG_DO_ASYNC_COMM), this->m_events_history[0].os_signal_send.sig_num);

    ASSERT_TRUE(this->m_mem_alloc_trace.is_empty());
}

TEST_F(TestAdvPostSignals, test_adv_post_handle_sig_do_async_comm) // NOLINT
{
    adv_post_signals_init();
    this->m_events_history.clear();

    adv_post_state_t adv_post_state = {
        .flag_primary_time_sync_is_done  = true,
        .flag_network_connected          = true,
        .flag_async_comm_in_progress     = false,
        .flag_need_to_send_advs1         = false,
        .flag_need_to_send_advs2         = false,
        .flag_need_to_send_statistics    = false,
        .flag_need_to_send_mqtt_periodic = false,
        .flag_relaying_enabled           = true,
        .flag_use_timestamps             = true,
        .flag_stop                       = false,
    };

    ASSERT_FALSE(adv_post_handle_sig(ADV_POST_SIG_DO_ASYNC_COMM, &adv_post_state));
    ASSERT_FALSE(adv_post_state.flag_stop);
    ASSERT_FALSE(adv_post_state.flag_need_to_send_statistics);
    ASSERT_EQ(1, this->m_events_history.size());
    ASSERT_EQ(EVENT_HISTORY_DO_ASYNC_COMM, this->m_events_history[0].event_type);

    ASSERT_TRUE(this->m_mem_alloc_trace.is_empty());
}

TEST_F(TestAdvPostSignals, test_adv_post_handle_sig_relaying_mode_changed) // NOLINT
{
    adv_post_signals_init();
    this->m_events_history.clear();

    adv_post_state_t adv_post_state = {
        .flag_primary_time_sync_is_done  = true,
        .flag_network_connected          = true,
        .flag_async_comm_in_progress     = false,
        .flag_need_to_send_advs1         = false,
        .flag_need_to_send_advs2         = false,
        .flag_need_to_send_statistics    = false,
        .flag_need_to_send_mqtt_periodic = false,
        .flag_relaying_enabled           = true,
        .flag_use_timestamps             = true,
        .flag_stop                       = false,
    };

    {
        this->m_gw_status_is_relaying_via_http_enabled = false;
        adv_post_state.flag_async_comm_in_progress     = false;
        this->m_gw_cfg.ruuvi_cfg.http.use_http_ruuvi   = false;
        this->m_gw_cfg.ruuvi_cfg.http.use_http         = false;
        ASSERT_FALSE(adv_post_handle_sig(ADV_POST_SIG_RELAYING_MODE_CHANGED, &adv_post_state));
        ASSERT_FALSE(adv_post_state.flag_stop);
        ASSERT_FALSE(adv_post_state.flag_relaying_enabled);
        ASSERT_EQ(1, this->m_events_history.size());
        ASSERT_EQ(EVENT_HISTORY_GW_STATUS_CLEAR_HTTP_RELAYING_CMD, this->m_events_history[0].event_type);
        this->m_events_history.clear();
    }
    {
        this->m_gw_status_is_relaying_via_http_enabled = true;
        adv_post_state.flag_async_comm_in_progress     = false;
        this->m_gw_cfg.ruuvi_cfg.http.use_http_ruuvi   = false;
        this->m_gw_cfg.ruuvi_cfg.http.use_http         = false;
        ASSERT_FALSE(adv_post_handle_sig(ADV_POST_SIG_RELAYING_MODE_CHANGED, &adv_post_state));
        ASSERT_FALSE(adv_post_state.flag_stop);
        ASSERT_TRUE(adv_post_state.flag_relaying_enabled);
        ASSERT_EQ(1, this->m_events_history.size());
        ASSERT_EQ(EVENT_HISTORY_GW_STATUS_CLEAR_HTTP_RELAYING_CMD, this->m_events_history[0].event_type);
        this->m_events_history.clear();
    }
    {
        this->m_gw_status_is_relaying_via_http_enabled = false;
        adv_post_state.flag_async_comm_in_progress     = false;
        this->m_gw_cfg.ruuvi_cfg.http.use_http_ruuvi   = true;
        this->m_gw_cfg.ruuvi_cfg.http.use_http         = false;
        ASSERT_FALSE(adv_post_handle_sig(ADV_POST_SIG_RELAYING_MODE_CHANGED, &adv_post_state));
        ASSERT_FALSE(adv_post_state.flag_stop);
        ASSERT_FALSE(adv_post_state.flag_relaying_enabled);
        ASSERT_EQ(2, this->m_events_history.size());
        ASSERT_EQ(EVENT_HISTORY_LEDS_NOTIFY_HTTP1_DATA_SENT_FAIL, this->m_events_history[0].event_type);
        ASSERT_EQ(EVENT_HISTORY_GW_STATUS_CLEAR_HTTP_RELAYING_CMD, this->m_events_history[1].event_type);
        this->m_events_history.clear();
    }
    {
        this->m_gw_status_is_relaying_via_http_enabled = true;
        adv_post_state.flag_async_comm_in_progress     = false;
        this->m_gw_cfg.ruuvi_cfg.http.use_http_ruuvi   = true;
        this->m_gw_cfg.ruuvi_cfg.http.use_http         = false;
        ASSERT_FALSE(adv_post_handle_sig(ADV_POST_SIG_RELAYING_MODE_CHANGED, &adv_post_state));
        ASSERT_FALSE(adv_post_state.flag_stop);
        ASSERT_TRUE(adv_post_state.flag_relaying_enabled);
        ASSERT_EQ(1, this->m_events_history.size());
        ASSERT_EQ(EVENT_HISTORY_GW_STATUS_CLEAR_HTTP_RELAYING_CMD, this->m_events_history[0].event_type);
        this->m_events_history.clear();
    }
    {
        this->m_gw_status_is_relaying_via_http_enabled = false;
        adv_post_state.flag_async_comm_in_progress     = false;
        this->m_gw_cfg.ruuvi_cfg.http.use_http_ruuvi   = false;
        this->m_gw_cfg.ruuvi_cfg.http.use_http         = true;
        ASSERT_FALSE(adv_post_handle_sig(ADV_POST_SIG_RELAYING_MODE_CHANGED, &adv_post_state));
        ASSERT_FALSE(adv_post_state.flag_stop);
        ASSERT_FALSE(adv_post_state.flag_relaying_enabled);
        ASSERT_EQ(2, this->m_events_history.size());
        ASSERT_EQ(EVENT_HISTORY_LEDS_NOTIFY_HTTP2_DATA_SENT_FAIL, this->m_events_history[0].event_type);
        ASSERT_EQ(EVENT_HISTORY_GW_STATUS_CLEAR_HTTP_RELAYING_CMD, this->m_events_history[1].event_type);
        this->m_events_history.clear();
    }
    {
        this->m_gw_status_is_relaying_via_http_enabled = true;
        adv_post_state.flag_async_comm_in_progress     = false;
        this->m_gw_cfg.ruuvi_cfg.http.use_http_ruuvi   = false;
        this->m_gw_cfg.ruuvi_cfg.http.use_http         = true;
        ASSERT_FALSE(adv_post_handle_sig(ADV_POST_SIG_RELAYING_MODE_CHANGED, &adv_post_state));
        ASSERT_FALSE(adv_post_state.flag_stop);
        ASSERT_TRUE(adv_post_state.flag_relaying_enabled);
        ASSERT_EQ(1, this->m_events_history.size());
        ASSERT_EQ(EVENT_HISTORY_GW_STATUS_CLEAR_HTTP_RELAYING_CMD, this->m_events_history[0].event_type);
        this->m_events_history.clear();
    }
    {
        this->m_gw_status_is_relaying_via_http_enabled = false;
        adv_post_state.flag_async_comm_in_progress     = true;
        this->m_gw_cfg.ruuvi_cfg.http.use_http_ruuvi   = false;
        this->m_gw_cfg.ruuvi_cfg.http.use_http         = false;
        ASSERT_FALSE(adv_post_handle_sig(ADV_POST_SIG_RELAYING_MODE_CHANGED, &adv_post_state));
        ASSERT_FALSE(adv_post_state.flag_stop);
        ASSERT_FALSE(adv_post_state.flag_relaying_enabled);
        ASSERT_FALSE(adv_post_state.flag_async_comm_in_progress);
        ASSERT_EQ(4, this->m_events_history.size());
        ASSERT_EQ(EVENT_HISTORY_STOP_TIMER_SIG_DO_ASYNC_COMM, this->m_events_history[0].event_type);
        ASSERT_EQ(EVENT_HISTORY_HTTP_ABORT_ANY_REQ_DURING_PROCESSING, this->m_events_history[1].event_type);
        ASSERT_EQ(EVENT_HISTORY_HTTP_SERVER_MUTEX_UNLOCK, this->m_events_history[2].event_type);
        ASSERT_EQ(EVENT_HISTORY_GW_STATUS_CLEAR_HTTP_RELAYING_CMD, this->m_events_history[3].event_type);
        this->m_events_history.clear();
    }

    ASSERT_TRUE(this->m_mem_alloc_trace.is_empty());
}

TEST_F(TestAdvPostSignals, test_adv_post_handle_sig_network_watchdog) // NOLINT
{
    adv_post_signals_init();
    this->m_events_history.clear();

    adv_post_state_t adv_post_state = {
        .flag_primary_time_sync_is_done  = true,
        .flag_network_connected          = true,
        .flag_async_comm_in_progress     = false,
        .flag_need_to_send_advs1         = false,
        .flag_need_to_send_advs2         = false,
        .flag_need_to_send_statistics    = false,
        .flag_need_to_send_mqtt_periodic = false,
        .flag_relaying_enabled           = true,
        .flag_use_timestamps             = true,
        .flag_stop                       = false,
    };

    this->m_network_timeout_check_res = false;
    ASSERT_FALSE(adv_post_handle_sig(ADV_POST_SIG_NETWORK_WATCHDOG, &adv_post_state));
    ASSERT_FALSE(adv_post_state.flag_stop);
    ASSERT_EQ(0, this->m_events_history.size());

    this->m_network_timeout_check_res = true;
    ASSERT_FALSE(adv_post_handle_sig(ADV_POST_SIG_NETWORK_WATCHDOG, &adv_post_state));
    ASSERT_FALSE(adv_post_state.flag_stop);
    ASSERT_EQ(1, this->m_events_history.size());
    ASSERT_EQ(EVENT_HISTORY_GATEWAY_RESTART, this->m_events_history[0].event_type);

    ASSERT_TRUE(this->m_mem_alloc_trace.is_empty());
}

TEST_F(TestAdvPostSignals, test_adv_post_handle_sig_task_watchdog_feed) // NOLINT
{
    adv_post_signals_init();
    this->m_events_history.clear();

    adv_post_state_t adv_post_state = {
        .flag_primary_time_sync_is_done  = true,
        .flag_network_connected          = true,
        .flag_async_comm_in_progress     = false,
        .flag_need_to_send_advs1         = false,
        .flag_need_to_send_advs2         = false,
        .flag_need_to_send_statistics    = false,
        .flag_need_to_send_mqtt_periodic = false,
        .flag_relaying_enabled           = true,
        .flag_use_timestamps             = true,
        .flag_stop                       = false,
    };

    ASSERT_FALSE(adv_post_handle_sig(ADV_POST_SIG_TASK_WATCHDOG_FEED, &adv_post_state));
    ASSERT_FALSE(adv_post_state.flag_stop);
    ASSERT_EQ(1, this->m_events_history.size());
    ASSERT_EQ(EVENT_HISTORY_ESP_TASK_WDT_RESET, this->m_events_history[0].event_type);

    ASSERT_TRUE(this->m_mem_alloc_trace.is_empty());
}

TEST_F(TestAdvPostSignals, test_adv_post_handle_sig_gw_cfg_ready) // NOLINT
{
    adv_post_signals_init();
    this->m_events_history.clear();

    adv_post_state_t adv_post_state = {
        .flag_primary_time_sync_is_done  = true,
        .flag_network_connected          = true,
        .flag_async_comm_in_progress     = false,
        .flag_need_to_send_advs1         = false,
        .flag_need_to_send_advs2         = false,
        .flag_need_to_send_statistics    = false,
        .flag_need_to_send_mqtt_periodic = false,
        .flag_relaying_enabled           = true,
        .flag_use_timestamps             = true,
        .flag_stop                       = false,
    };

    {
        this->m_gw_cfg.ruuvi_cfg.http_stat.use_http_stat    = true;
        this->m_gw_cfg.ruuvi_cfg.ntp.ntp_use                = true;
        this->m_adv_post_cfg_cache.p_arr_of_scan_filter_mac = nullptr;
        this->m_gw_cfg.ruuvi_cfg.http.use_http_ruuvi        = true;
        this->m_gw_cfg.ruuvi_cfg.http.use_http              = false;
        this->m_gw_cfg.ruuvi_cfg.mqtt.mqtt_sending_interval = 0;
        this->m_gw_cfg.ruuvi_cfg.mqtt.use_mqtt              = false;
        ASSERT_FALSE(adv_post_handle_sig(ADV_POST_SIG_GW_CFG_READY, &adv_post_state));
        ASSERT_FALSE(adv_post_state.flag_stop);
        ASSERT_TRUE(adv_post_state.flag_use_timestamps);
        ASSERT_TRUE(this->m_adv_post_cfg_cache.flag_use_ntp);
        ASSERT_FALSE(adv_post_state.flag_need_to_send_advs1);
        ASSERT_FALSE(adv_post_state.flag_need_to_send_advs2);
        ASSERT_EQ(10, this->m_events_history.size());
        ASSERT_EQ(EVENT_HISTORY_RUUVI_SEND_NRF_SETTINGS_FROM_GW_CFG, this->m_events_history[0].event_type);
        ASSERT_EQ(EVENT_HISTORY_ADV_POST_CFG_CACHE_MUTEX_LOCK, this->m_events_history[2].event_type);
        ASSERT_EQ(EVENT_HISTORY_ADV1_POST_TIMER_RESTART_WITH_DEFAULT_PERIOD, this->m_events_history[3].event_type);
        ASSERT_EQ(EVENT_HISTORY_STOP_TIMER_SIG_RETRANSMIT_TO_HTTP_CUSTOM, this->m_events_history[4].event_type);
        ASSERT_EQ(EVENT_HISTORY_STOP_TIMER_SIG_MQTT, this->m_events_history[5].event_type);
        ASSERT_EQ(EVENT_HISTORY_RELAUNCH_TIMER_SIG_SEND_STATISTICS, this->m_events_history[6].event_type);
        ASSERT_EQ(EVENT_HISTORY_ADV_POST_CFG_CACHE_MUTEX_UNLOCK, this->m_events_history[7].event_type);
        ASSERT_EQ(EVENT_HISTORY_ADV_TABLE_CLEAR, this->m_events_history[8].event_type);
        ASSERT_EQ(EVENT_HISTORY_HTTP_SERVER_MUTEX_DEACTIVATE, this->m_events_history[9].event_type);
        this->m_events_history.clear();
    }

    {
        this->m_gw_cfg.ruuvi_cfg.http_stat.use_http_stat    = false;
        this->m_gw_cfg.ruuvi_cfg.ntp.ntp_use                = true;
        this->m_adv_post_cfg_cache.p_arr_of_scan_filter_mac = nullptr;
        this->m_gw_cfg.ruuvi_cfg.http.use_http_ruuvi        = true;
        this->m_gw_cfg.ruuvi_cfg.http.use_http              = false;
        this->m_gw_cfg.ruuvi_cfg.mqtt.mqtt_sending_interval = 0;
        this->m_gw_cfg.ruuvi_cfg.mqtt.use_mqtt              = false;
        ASSERT_FALSE(adv_post_handle_sig(ADV_POST_SIG_GW_CFG_READY, &adv_post_state));
        ASSERT_FALSE(adv_post_state.flag_stop);
        ASSERT_TRUE(adv_post_state.flag_use_timestamps);
        ASSERT_TRUE(this->m_adv_post_cfg_cache.flag_use_ntp);
        ASSERT_FALSE(adv_post_state.flag_need_to_send_advs1);
        ASSERT_FALSE(adv_post_state.flag_need_to_send_advs2);
        ASSERT_EQ(9, this->m_events_history.size());
        ASSERT_EQ(EVENT_HISTORY_RUUVI_SEND_NRF_SETTINGS_FROM_GW_CFG, this->m_events_history[0].event_type);
        ASSERT_EQ(EVENT_HISTORY_ADV_POST_CFG_CACHE_MUTEX_LOCK, this->m_events_history[1].event_type);
        ASSERT_EQ(EVENT_HISTORY_ADV1_POST_TIMER_RESTART_WITH_DEFAULT_PERIOD, this->m_events_history[2].event_type);
        ASSERT_EQ(EVENT_HISTORY_STOP_TIMER_SIG_RETRANSMIT_TO_HTTP_CUSTOM, this->m_events_history[3].event_type);
        ASSERT_EQ(EVENT_HISTORY_STOP_TIMER_SIG_MQTT, this->m_events_history[4].event_type);
        ASSERT_EQ(EVENT_HISTORY_STOP_TIMER_SIG_SEND_STATISTICS, this->m_events_history[5].event_type);
        ASSERT_EQ(EVENT_HISTORY_ADV_POST_CFG_CACHE_MUTEX_UNLOCK, this->m_events_history[6].event_type);
        ASSERT_EQ(EVENT_HISTORY_ADV_TABLE_CLEAR, this->m_events_history[7].event_type);
        ASSERT_EQ(EVENT_HISTORY_HTTP_SERVER_MUTEX_DEACTIVATE, this->m_events_history[8].event_type);
        this->m_events_history.clear();
    }
    {
        this->m_gw_cfg.ruuvi_cfg.http_stat.use_http_stat    = false;
        this->m_gw_cfg.ruuvi_cfg.ntp.ntp_use                = true;
        this->m_adv_post_cfg_cache.p_arr_of_scan_filter_mac = nullptr;
        this->m_gw_cfg.ruuvi_cfg.http.use_http_ruuvi        = false;
        this->m_gw_cfg.ruuvi_cfg.http.use_http              = false;
        this->m_gw_cfg.ruuvi_cfg.mqtt.mqtt_sending_interval = 0;
        this->m_gw_cfg.ruuvi_cfg.mqtt.use_mqtt              = true;
        snprintf(
            this->m_gw_cfg.ruuvi_cfg.mqtt.mqtt_transport.buf,
            sizeof(this->m_gw_cfg.ruuvi_cfg.mqtt.mqtt_transport.buf),
            "%s",
            MQTT_TRANSPORT_SSL);
        ASSERT_FALSE(adv_post_handle_sig(ADV_POST_SIG_GW_CFG_READY, &adv_post_state));
        ASSERT_FALSE(adv_post_state.flag_stop);
        ASSERT_TRUE(adv_post_state.flag_use_timestamps);
        ASSERT_TRUE(this->m_adv_post_cfg_cache.flag_use_ntp);
        ASSERT_FALSE(adv_post_state.flag_need_to_send_advs1);
        ASSERT_FALSE(adv_post_state.flag_need_to_send_advs2);
        ASSERT_EQ(9, this->m_events_history.size());
        ASSERT_EQ(EVENT_HISTORY_RUUVI_SEND_NRF_SETTINGS_FROM_GW_CFG, this->m_events_history[0].event_type);
        ASSERT_EQ(EVENT_HISTORY_ADV_POST_CFG_CACHE_MUTEX_LOCK, this->m_events_history[1].event_type);
        ASSERT_EQ(EVENT_HISTORY_STOP_TIMER_SIG_RETRANSMIT_TO_HTTP_RUUVI, this->m_events_history[2].event_type);
        ASSERT_EQ(EVENT_HISTORY_STOP_TIMER_SIG_RETRANSMIT_TO_HTTP_CUSTOM, this->m_events_history[3].event_type);
        ASSERT_EQ(EVENT_HISTORY_STOP_TIMER_SIG_MQTT, this->m_events_history[4].event_type);
        ASSERT_EQ(EVENT_HISTORY_STOP_TIMER_SIG_SEND_STATISTICS, this->m_events_history[5].event_type);
        ASSERT_EQ(EVENT_HISTORY_ADV_POST_CFG_CACHE_MUTEX_UNLOCK, this->m_events_history[6].event_type);
        ASSERT_EQ(EVENT_HISTORY_ADV_TABLE_CLEAR, this->m_events_history[7].event_type);
        ASSERT_EQ(EVENT_HISTORY_HTTP_SERVER_MUTEX_DEACTIVATE, this->m_events_history[8].event_type);
        this->m_events_history.clear();
    }
    {
        this->m_gw_cfg.ruuvi_cfg.http_stat.use_http_stat    = false;
        this->m_gw_cfg.ruuvi_cfg.ntp.ntp_use                = true;
        this->m_adv_post_cfg_cache.p_arr_of_scan_filter_mac = nullptr;
        this->m_gw_cfg.ruuvi_cfg.http.use_http_ruuvi        = true;
        this->m_gw_cfg.ruuvi_cfg.http.use_http              = false;
        this->m_gw_cfg.ruuvi_cfg.mqtt.mqtt_sending_interval = 0;
        this->m_gw_cfg.ruuvi_cfg.mqtt.use_mqtt              = true;
        snprintf(
            this->m_gw_cfg.ruuvi_cfg.mqtt.mqtt_transport.buf,
            sizeof(this->m_gw_cfg.ruuvi_cfg.mqtt.mqtt_transport.buf),
            "%s",
            MQTT_TRANSPORT_WSS);
        ASSERT_FALSE(adv_post_handle_sig(ADV_POST_SIG_GW_CFG_READY, &adv_post_state));
        ASSERT_FALSE(adv_post_state.flag_stop);
        ASSERT_TRUE(adv_post_state.flag_use_timestamps);
        ASSERT_TRUE(this->m_adv_post_cfg_cache.flag_use_ntp);
        ASSERT_FALSE(adv_post_state.flag_need_to_send_advs1);
        ASSERT_FALSE(adv_post_state.flag_need_to_send_advs2);
        ASSERT_EQ(9, this->m_events_history.size());
        ASSERT_EQ(EVENT_HISTORY_RUUVI_SEND_NRF_SETTINGS_FROM_GW_CFG, this->m_events_history[0].event_type);
        ASSERT_EQ(EVENT_HISTORY_ADV_POST_CFG_CACHE_MUTEX_LOCK, this->m_events_history[1].event_type);
        ASSERT_EQ(EVENT_HISTORY_ADV1_POST_TIMER_RESTART_WITH_DEFAULT_PERIOD, this->m_events_history[2].event_type);
        ASSERT_EQ(EVENT_HISTORY_STOP_TIMER_SIG_RETRANSMIT_TO_HTTP_CUSTOM, this->m_events_history[3].event_type);
        ASSERT_EQ(EVENT_HISTORY_STOP_TIMER_SIG_MQTT, this->m_events_history[4].event_type);
        ASSERT_EQ(EVENT_HISTORY_STOP_TIMER_SIG_SEND_STATISTICS, this->m_events_history[5].event_type);
        ASSERT_EQ(EVENT_HISTORY_ADV_POST_CFG_CACHE_MUTEX_UNLOCK, this->m_events_history[6].event_type);
        ASSERT_EQ(EVENT_HISTORY_ADV_TABLE_CLEAR, this->m_events_history[7].event_type);
        ASSERT_EQ(EVENT_HISTORY_HTTP_SERVER_MUTEX_ACTIVATE, this->m_events_history[8].event_type);
        this->m_events_history.clear();
    }
    {
        this->m_gw_cfg.ruuvi_cfg.http_stat.use_http_stat    = false;
        this->m_gw_cfg.ruuvi_cfg.ntp.ntp_use                = true;
        this->m_adv_post_cfg_cache.p_arr_of_scan_filter_mac = nullptr;
        this->m_gw_cfg.ruuvi_cfg.http.use_http_ruuvi        = false;
        this->m_gw_cfg.ruuvi_cfg.http.use_http              = true;
        this->m_gw_cfg.ruuvi_cfg.mqtt.mqtt_sending_interval = 0;
        this->m_gw_cfg.ruuvi_cfg.mqtt.use_mqtt              = true;
        snprintf(
            this->m_gw_cfg.ruuvi_cfg.mqtt.mqtt_transport.buf,
            sizeof(this->m_gw_cfg.ruuvi_cfg.mqtt.mqtt_transport.buf),
            "%s",
            MQTT_TRANSPORT_SSL);
        ASSERT_FALSE(adv_post_handle_sig(ADV_POST_SIG_GW_CFG_READY, &adv_post_state));
        ASSERT_FALSE(adv_post_state.flag_stop);
        ASSERT_TRUE(adv_post_state.flag_use_timestamps);
        ASSERT_TRUE(this->m_adv_post_cfg_cache.flag_use_ntp);
        ASSERT_FALSE(adv_post_state.flag_need_to_send_advs1);
        ASSERT_FALSE(adv_post_state.flag_need_to_send_advs2);
        ASSERT_EQ(10, this->m_events_history.size());
        ASSERT_EQ(EVENT_HISTORY_RUUVI_SEND_NRF_SETTINGS_FROM_GW_CFG, this->m_events_history[0].event_type);
        ASSERT_EQ(EVENT_HISTORY_ADV_POST_CFG_CACHE_MUTEX_LOCK, this->m_events_history[1].event_type);
        ASSERT_EQ(EVENT_HISTORY_STOP_TIMER_SIG_RETRANSMIT_TO_HTTP_RUUVI, this->m_events_history[2].event_type);
        ASSERT_EQ(EVENT_HISTORY_ADV2_POST_TIMERS_SET_DEFAULT_PERIOD, this->m_events_history[3].event_type);
        ASSERT_EQ(EVENT_HISTORY_ADV2_POST_TIMER_RESTART_WITH_DEFAULT_PERIOD, this->m_events_history[4].event_type);
        ASSERT_EQ(EVENT_HISTORY_STOP_TIMER_SIG_MQTT, this->m_events_history[5].event_type);
        ASSERT_EQ(EVENT_HISTORY_STOP_TIMER_SIG_SEND_STATISTICS, this->m_events_history[6].event_type);
        ASSERT_EQ(EVENT_HISTORY_ADV_POST_CFG_CACHE_MUTEX_UNLOCK, this->m_events_history[7].event_type);
        ASSERT_EQ(EVENT_HISTORY_ADV_TABLE_CLEAR, this->m_events_history[8].event_type);
        ASSERT_EQ(EVENT_HISTORY_HTTP_SERVER_MUTEX_ACTIVATE, this->m_events_history[9].event_type);
        this->m_events_history.clear();
    }
    {
        this->m_gw_cfg.ruuvi_cfg.http_stat.use_http_stat    = false;
        this->m_gw_cfg.ruuvi_cfg.ntp.ntp_use                = true;
        this->m_adv_post_cfg_cache.p_arr_of_scan_filter_mac = nullptr;
        this->m_gw_cfg.ruuvi_cfg.http.use_http_ruuvi        = true;
        this->m_gw_cfg.ruuvi_cfg.http.use_http              = true;
        this->m_gw_cfg.ruuvi_cfg.mqtt.mqtt_sending_interval = 0;
        this->m_gw_cfg.ruuvi_cfg.mqtt.use_mqtt              = true;
        snprintf(
            this->m_gw_cfg.ruuvi_cfg.mqtt.mqtt_transport.buf,
            sizeof(this->m_gw_cfg.ruuvi_cfg.mqtt.mqtt_transport.buf),
            "%s",
            MQTT_TRANSPORT_WSS);
        ASSERT_FALSE(adv_post_handle_sig(ADV_POST_SIG_GW_CFG_READY, &adv_post_state));
        ASSERT_FALSE(adv_post_state.flag_stop);
        ASSERT_TRUE(adv_post_state.flag_use_timestamps);
        ASSERT_TRUE(this->m_adv_post_cfg_cache.flag_use_ntp);
        ASSERT_FALSE(adv_post_state.flag_need_to_send_advs1);
        ASSERT_FALSE(adv_post_state.flag_need_to_send_advs2);
        ASSERT_EQ(10, this->m_events_history.size());
        ASSERT_EQ(EVENT_HISTORY_RUUVI_SEND_NRF_SETTINGS_FROM_GW_CFG, this->m_events_history[0].event_type);
        ASSERT_EQ(EVENT_HISTORY_ADV_POST_CFG_CACHE_MUTEX_LOCK, this->m_events_history[1].event_type);
        ASSERT_EQ(EVENT_HISTORY_ADV1_POST_TIMER_RESTART_WITH_DEFAULT_PERIOD, this->m_events_history[2].event_type);
        ASSERT_EQ(EVENT_HISTORY_ADV2_POST_TIMERS_SET_DEFAULT_PERIOD, this->m_events_history[3].event_type);
        ASSERT_EQ(EVENT_HISTORY_ADV2_POST_TIMER_RESTART_WITH_DEFAULT_PERIOD, this->m_events_history[4].event_type);
        ASSERT_EQ(EVENT_HISTORY_STOP_TIMER_SIG_MQTT, this->m_events_history[5].event_type);
        ASSERT_EQ(EVENT_HISTORY_STOP_TIMER_SIG_SEND_STATISTICS, this->m_events_history[6].event_type);
        ASSERT_EQ(EVENT_HISTORY_ADV_POST_CFG_CACHE_MUTEX_UNLOCK, this->m_events_history[7].event_type);
        ASSERT_EQ(EVENT_HISTORY_ADV_TABLE_CLEAR, this->m_events_history[8].event_type);
        ASSERT_EQ(EVENT_HISTORY_HTTP_SERVER_MUTEX_ACTIVATE, this->m_events_history[9].event_type);
        this->m_events_history.clear();
    }
    {
        this->m_gw_cfg.ruuvi_cfg.http_stat.use_http_stat    = false;
        this->m_gw_cfg.ruuvi_cfg.ntp.ntp_use                = true;
        this->m_adv_post_cfg_cache.p_arr_of_scan_filter_mac = nullptr;
        this->m_gw_cfg.ruuvi_cfg.http.use_http_ruuvi        = true;
        this->m_gw_cfg.ruuvi_cfg.http.use_http              = true;
        this->m_gw_cfg.ruuvi_cfg.mqtt.mqtt_sending_interval = 0;
        this->m_gw_cfg.ruuvi_cfg.mqtt.use_mqtt              = true;
        snprintf(
            this->m_gw_cfg.ruuvi_cfg.mqtt.mqtt_transport.buf,
            sizeof(this->m_gw_cfg.ruuvi_cfg.mqtt.mqtt_transport.buf),
            "%s",
            MQTT_TRANSPORT_TCP);
        ASSERT_FALSE(adv_post_handle_sig(ADV_POST_SIG_GW_CFG_READY, &adv_post_state));
        ASSERT_FALSE(adv_post_state.flag_stop);
        ASSERT_TRUE(adv_post_state.flag_use_timestamps);
        ASSERT_TRUE(this->m_adv_post_cfg_cache.flag_use_ntp);
        ASSERT_FALSE(adv_post_state.flag_need_to_send_advs1);
        ASSERT_FALSE(adv_post_state.flag_need_to_send_advs2);
        ASSERT_EQ(10, this->m_events_history.size());
        ASSERT_EQ(EVENT_HISTORY_RUUVI_SEND_NRF_SETTINGS_FROM_GW_CFG, this->m_events_history[0].event_type);
        ASSERT_EQ(EVENT_HISTORY_ADV_POST_CFG_CACHE_MUTEX_LOCK, this->m_events_history[1].event_type);
        ASSERT_EQ(EVENT_HISTORY_ADV1_POST_TIMER_RESTART_WITH_DEFAULT_PERIOD, this->m_events_history[2].event_type);
        ASSERT_EQ(EVENT_HISTORY_ADV2_POST_TIMERS_SET_DEFAULT_PERIOD, this->m_events_history[3].event_type);
        ASSERT_EQ(EVENT_HISTORY_ADV2_POST_TIMER_RESTART_WITH_DEFAULT_PERIOD, this->m_events_history[4].event_type);
        ASSERT_EQ(EVENT_HISTORY_STOP_TIMER_SIG_MQTT, this->m_events_history[5].event_type);
        ASSERT_EQ(EVENT_HISTORY_STOP_TIMER_SIG_SEND_STATISTICS, this->m_events_history[6].event_type);
        ASSERT_EQ(EVENT_HISTORY_ADV_POST_CFG_CACHE_MUTEX_UNLOCK, this->m_events_history[7].event_type);
        ASSERT_EQ(EVENT_HISTORY_ADV_TABLE_CLEAR, this->m_events_history[8].event_type);
        ASSERT_EQ(EVENT_HISTORY_HTTP_SERVER_MUTEX_DEACTIVATE, this->m_events_history[9].event_type);
        this->m_events_history.clear();
    }
    {
        this->m_gw_cfg.ruuvi_cfg.http_stat.use_http_stat    = false;
        this->m_gw_cfg.ruuvi_cfg.ntp.ntp_use                = true;
        this->m_adv_post_cfg_cache.p_arr_of_scan_filter_mac = nullptr;
        this->m_gw_cfg.ruuvi_cfg.http.use_http_ruuvi        = true;
        this->m_gw_cfg.ruuvi_cfg.http.use_http              = true;
        this->m_gw_cfg.ruuvi_cfg.mqtt.mqtt_sending_interval = 0;
        this->m_gw_cfg.ruuvi_cfg.mqtt.use_mqtt              = true;
        snprintf(
            this->m_gw_cfg.ruuvi_cfg.mqtt.mqtt_transport.buf,
            sizeof(this->m_gw_cfg.ruuvi_cfg.mqtt.mqtt_transport.buf),
            "%s",
            MQTT_TRANSPORT_WS);
        ASSERT_FALSE(adv_post_handle_sig(ADV_POST_SIG_GW_CFG_READY, &adv_post_state));
        ASSERT_FALSE(adv_post_state.flag_stop);
        ASSERT_TRUE(adv_post_state.flag_use_timestamps);
        ASSERT_TRUE(this->m_adv_post_cfg_cache.flag_use_ntp);
        ASSERT_FALSE(adv_post_state.flag_need_to_send_advs1);
        ASSERT_FALSE(adv_post_state.flag_need_to_send_advs2);
        ASSERT_EQ(10, this->m_events_history.size());
        ASSERT_EQ(EVENT_HISTORY_RUUVI_SEND_NRF_SETTINGS_FROM_GW_CFG, this->m_events_history[0].event_type);
        ASSERT_EQ(EVENT_HISTORY_ADV_POST_CFG_CACHE_MUTEX_LOCK, this->m_events_history[1].event_type);
        ASSERT_EQ(EVENT_HISTORY_ADV1_POST_TIMER_RESTART_WITH_DEFAULT_PERIOD, this->m_events_history[2].event_type);
        ASSERT_EQ(EVENT_HISTORY_ADV2_POST_TIMERS_SET_DEFAULT_PERIOD, this->m_events_history[3].event_type);
        ASSERT_EQ(EVENT_HISTORY_ADV2_POST_TIMER_RESTART_WITH_DEFAULT_PERIOD, this->m_events_history[4].event_type);
        ASSERT_EQ(EVENT_HISTORY_STOP_TIMER_SIG_MQTT, this->m_events_history[5].event_type);
        ASSERT_EQ(EVENT_HISTORY_STOP_TIMER_SIG_SEND_STATISTICS, this->m_events_history[6].event_type);
        ASSERT_EQ(EVENT_HISTORY_ADV_POST_CFG_CACHE_MUTEX_UNLOCK, this->m_events_history[7].event_type);
        ASSERT_EQ(EVENT_HISTORY_ADV_TABLE_CLEAR, this->m_events_history[8].event_type);
        ASSERT_EQ(EVENT_HISTORY_HTTP_SERVER_MUTEX_DEACTIVATE, this->m_events_history[9].event_type);
        this->m_events_history.clear();
    }

    ASSERT_TRUE(this->m_mem_alloc_trace.is_empty());
}

TEST_F(TestAdvPostSignals, test_adv_post_handle_sig_gw_cfg_changed_ruuvi) // NOLINT
{
    adv_post_signals_init();
    this->m_events_history.clear();

    adv_post_state_t adv_post_state = {
        .flag_primary_time_sync_is_done  = true,
        .flag_network_connected          = true,
        .flag_async_comm_in_progress     = false,
        .flag_need_to_send_advs1         = false,
        .flag_need_to_send_advs2         = false,
        .flag_need_to_send_statistics    = false,
        .flag_need_to_send_mqtt_periodic = false,
        .flag_relaying_enabled           = true,
        .flag_use_timestamps             = true,
        .flag_stop                       = false,
    };

    {
        this->m_gw_cfg.ruuvi_cfg.http_stat.use_http_stat    = true;
        this->m_gw_cfg.ruuvi_cfg.ntp.ntp_use                = true;
        this->m_adv_post_cfg_cache.p_arr_of_scan_filter_mac = nullptr;
        this->m_gw_cfg.ruuvi_cfg.http.use_http_ruuvi        = true;
        this->m_gw_cfg.ruuvi_cfg.http.use_http              = false;
        this->m_gw_cfg.ruuvi_cfg.mqtt.mqtt_sending_interval = 0;
        this->m_gw_cfg.ruuvi_cfg.mqtt.use_mqtt              = false;
        ASSERT_FALSE(adv_post_handle_sig(ADV_POST_SIG_GW_CFG_CHANGED_RUUVI, &adv_post_state));
        ASSERT_FALSE(adv_post_state.flag_stop);
        ASSERT_TRUE(adv_post_state.flag_use_timestamps);
        ASSERT_TRUE(this->m_adv_post_cfg_cache.flag_use_ntp);
        ASSERT_FALSE(adv_post_state.flag_need_to_send_advs1);
        ASSERT_FALSE(adv_post_state.flag_need_to_send_advs2);
        ASSERT_EQ(9, this->m_events_history.size());
        ASSERT_EQ(EVENT_HISTORY_RUUVI_SEND_NRF_SETTINGS_FROM_GW_CFG, this->m_events_history[0].event_type);
        ASSERT_EQ(EVENT_HISTORY_ADV_POST_CFG_CACHE_MUTEX_LOCK, this->m_events_history[1].event_type);
        ASSERT_EQ(EVENT_HISTORY_ADV1_POST_TIMER_RESTART_WITH_DEFAULT_PERIOD, this->m_events_history[2].event_type);
        ASSERT_EQ(EVENT_HISTORY_STOP_TIMER_SIG_RETRANSMIT_TO_HTTP_CUSTOM, this->m_events_history[3].event_type);
        ASSERT_EQ(EVENT_HISTORY_STOP_TIMER_SIG_MQTT, this->m_events_history[4].event_type);
        ASSERT_EQ(EVENT_HISTORY_RELAUNCH_TIMER_SIG_SEND_STATISTICS, this->m_events_history[5].event_type);
        ASSERT_EQ(EVENT_HISTORY_ADV_POST_CFG_CACHE_MUTEX_UNLOCK, this->m_events_history[6].event_type);
        ASSERT_EQ(EVENT_HISTORY_ADV_TABLE_CLEAR, this->m_events_history[7].event_type);
        ASSERT_EQ(EVENT_HISTORY_HTTP_SERVER_MUTEX_DEACTIVATE, this->m_events_history[8].event_type);
        this->m_events_history.clear();
    }

    {
        this->m_gw_cfg.ruuvi_cfg.http_stat.use_http_stat    = true;
        this->m_gw_cfg.ruuvi_cfg.ntp.ntp_use                = true;
        this->m_adv_post_cfg_cache.p_arr_of_scan_filter_mac = nullptr;
        this->m_gw_cfg.ruuvi_cfg.http.use_http_ruuvi        = false;
        this->m_gw_cfg.ruuvi_cfg.http.use_http              = false;
        this->m_gw_cfg.ruuvi_cfg.mqtt.mqtt_sending_interval = 0;
        this->m_gw_cfg.ruuvi_cfg.mqtt.use_mqtt              = false;
        ASSERT_FALSE(adv_post_handle_sig(ADV_POST_SIG_GW_CFG_CHANGED_RUUVI, &adv_post_state));
        ASSERT_FALSE(adv_post_state.flag_stop);
        ASSERT_TRUE(adv_post_state.flag_use_timestamps);
        ASSERT_TRUE(this->m_adv_post_cfg_cache.flag_use_ntp);
        ASSERT_FALSE(adv_post_state.flag_need_to_send_advs1);
        ASSERT_FALSE(adv_post_state.flag_need_to_send_advs2);
        ASSERT_EQ(9, this->m_events_history.size());
        ASSERT_EQ(EVENT_HISTORY_RUUVI_SEND_NRF_SETTINGS_FROM_GW_CFG, this->m_events_history[0].event_type);
        ASSERT_EQ(EVENT_HISTORY_ADV_POST_CFG_CACHE_MUTEX_LOCK, this->m_events_history[1].event_type);
        ASSERT_EQ(EVENT_HISTORY_STOP_TIMER_SIG_RETRANSMIT_TO_HTTP_RUUVI, this->m_events_history[2].event_type);
        ASSERT_EQ(EVENT_HISTORY_STOP_TIMER_SIG_RETRANSMIT_TO_HTTP_CUSTOM, this->m_events_history[3].event_type);
        ASSERT_EQ(EVENT_HISTORY_STOP_TIMER_SIG_MQTT, this->m_events_history[4].event_type);
        ASSERT_EQ(EVENT_HISTORY_RELAUNCH_TIMER_SIG_SEND_STATISTICS, this->m_events_history[5].event_type);
        ASSERT_EQ(EVENT_HISTORY_ADV_POST_CFG_CACHE_MUTEX_UNLOCK, this->m_events_history[6].event_type);
        ASSERT_EQ(EVENT_HISTORY_ADV_TABLE_CLEAR, this->m_events_history[7].event_type);
        ASSERT_EQ(EVENT_HISTORY_HTTP_SERVER_MUTEX_DEACTIVATE, this->m_events_history[8].event_type);
        this->m_events_history.clear();
    }

    {
        this->m_gw_cfg.ruuvi_cfg.http_stat.use_http_stat    = true;
        this->m_gw_cfg.ruuvi_cfg.ntp.ntp_use                = true;
        this->m_adv_post_cfg_cache.p_arr_of_scan_filter_mac = nullptr;
        this->m_gw_cfg.ruuvi_cfg.http.use_http_ruuvi        = false;
        this->m_gw_cfg.ruuvi_cfg.http.use_http              = true;
        this->m_gw_cfg.ruuvi_cfg.mqtt.mqtt_sending_interval = 0;
        this->m_gw_cfg.ruuvi_cfg.mqtt.use_mqtt              = false;
        ASSERT_FALSE(adv_post_handle_sig(ADV_POST_SIG_GW_CFG_CHANGED_RUUVI, &adv_post_state));
        ASSERT_FALSE(adv_post_state.flag_stop);
        ASSERT_TRUE(adv_post_state.flag_use_timestamps);
        ASSERT_TRUE(this->m_adv_post_cfg_cache.flag_use_ntp);
        ASSERT_FALSE(adv_post_state.flag_need_to_send_advs1);
        ASSERT_FALSE(adv_post_state.flag_need_to_send_advs2);
        ASSERT_EQ(10, this->m_events_history.size());
        ASSERT_EQ(EVENT_HISTORY_RUUVI_SEND_NRF_SETTINGS_FROM_GW_CFG, this->m_events_history[0].event_type);
        ASSERT_EQ(EVENT_HISTORY_ADV_POST_CFG_CACHE_MUTEX_LOCK, this->m_events_history[1].event_type);
        ASSERT_EQ(EVENT_HISTORY_STOP_TIMER_SIG_RETRANSMIT_TO_HTTP_RUUVI, this->m_events_history[2].event_type);
        ASSERT_EQ(EVENT_HISTORY_ADV2_POST_TIMERS_SET_DEFAULT_PERIOD, this->m_events_history[3].event_type);
        ASSERT_EQ(EVENT_HISTORY_ADV2_POST_TIMER_RESTART_WITH_DEFAULT_PERIOD, this->m_events_history[4].event_type);
        ASSERT_EQ(EVENT_HISTORY_STOP_TIMER_SIG_MQTT, this->m_events_history[5].event_type);
        ASSERT_EQ(EVENT_HISTORY_RELAUNCH_TIMER_SIG_SEND_STATISTICS, this->m_events_history[6].event_type);
        ASSERT_EQ(EVENT_HISTORY_ADV_POST_CFG_CACHE_MUTEX_UNLOCK, this->m_events_history[7].event_type);
        ASSERT_EQ(EVENT_HISTORY_ADV_TABLE_CLEAR, this->m_events_history[8].event_type);
        ASSERT_EQ(EVENT_HISTORY_HTTP_SERVER_MUTEX_DEACTIVATE, this->m_events_history[9].event_type);
        this->m_events_history.clear();
    }

    {
        this->m_gw_cfg.ruuvi_cfg.http_stat.use_http_stat    = false;
        this->m_gw_cfg.ruuvi_cfg.ntp.ntp_use                = true;
        this->m_adv_post_cfg_cache.p_arr_of_scan_filter_mac = nullptr;
        this->m_gw_cfg.ruuvi_cfg.http.use_http_ruuvi        = true;
        this->m_gw_cfg.ruuvi_cfg.http.use_http              = false;
        this->m_gw_cfg.ruuvi_cfg.mqtt.mqtt_sending_interval = 0;
        this->m_gw_cfg.ruuvi_cfg.mqtt.use_mqtt              = false;
        ASSERT_FALSE(adv_post_handle_sig(ADV_POST_SIG_GW_CFG_CHANGED_RUUVI, &adv_post_state));
        ASSERT_FALSE(adv_post_state.flag_stop);
        ASSERT_TRUE(adv_post_state.flag_use_timestamps);
        ASSERT_TRUE(this->m_adv_post_cfg_cache.flag_use_ntp);
        ASSERT_FALSE(adv_post_state.flag_need_to_send_advs1);
        ASSERT_FALSE(adv_post_state.flag_need_to_send_advs2);
        ASSERT_EQ(9, this->m_events_history.size());
        ASSERT_EQ(EVENT_HISTORY_RUUVI_SEND_NRF_SETTINGS_FROM_GW_CFG, this->m_events_history[0].event_type);
        ASSERT_EQ(EVENT_HISTORY_ADV_POST_CFG_CACHE_MUTEX_LOCK, this->m_events_history[1].event_type);
        ASSERT_EQ(EVENT_HISTORY_ADV1_POST_TIMER_RESTART_WITH_DEFAULT_PERIOD, this->m_events_history[2].event_type);
        ASSERT_EQ(EVENT_HISTORY_STOP_TIMER_SIG_RETRANSMIT_TO_HTTP_CUSTOM, this->m_events_history[3].event_type);
        ASSERT_EQ(EVENT_HISTORY_STOP_TIMER_SIG_MQTT, this->m_events_history[4].event_type);
        ASSERT_EQ(EVENT_HISTORY_STOP_TIMER_SIG_SEND_STATISTICS, this->m_events_history[5].event_type);
        ASSERT_EQ(EVENT_HISTORY_ADV_POST_CFG_CACHE_MUTEX_UNLOCK, this->m_events_history[6].event_type);
        ASSERT_EQ(EVENT_HISTORY_ADV_TABLE_CLEAR, this->m_events_history[7].event_type);
        ASSERT_EQ(EVENT_HISTORY_HTTP_SERVER_MUTEX_DEACTIVATE, this->m_events_history[8].event_type);
        this->m_events_history.clear();
    }
    {
        this->m_gw_cfg.ruuvi_cfg.http_stat.use_http_stat    = false;
        this->m_gw_cfg.ruuvi_cfg.ntp.ntp_use                = true;
        this->m_adv_post_cfg_cache.p_arr_of_scan_filter_mac = nullptr;
        this->m_gw_cfg.ruuvi_cfg.http.use_http_ruuvi        = false;
        this->m_gw_cfg.ruuvi_cfg.http.use_http              = false;
        this->m_gw_cfg.ruuvi_cfg.mqtt.mqtt_sending_interval = 0;
        this->m_gw_cfg.ruuvi_cfg.mqtt.use_mqtt              = true;
        snprintf(
            this->m_gw_cfg.ruuvi_cfg.mqtt.mqtt_transport.buf,
            sizeof(this->m_gw_cfg.ruuvi_cfg.mqtt.mqtt_transport.buf),
            "%s",
            MQTT_TRANSPORT_SSL);
        ASSERT_FALSE(adv_post_handle_sig(ADV_POST_SIG_GW_CFG_CHANGED_RUUVI, &adv_post_state));
        ASSERT_FALSE(adv_post_state.flag_stop);
        ASSERT_TRUE(adv_post_state.flag_use_timestamps);
        ASSERT_TRUE(this->m_adv_post_cfg_cache.flag_use_ntp);
        ASSERT_FALSE(adv_post_state.flag_need_to_send_advs1);
        ASSERT_FALSE(adv_post_state.flag_need_to_send_advs2);
        ASSERT_EQ(9, this->m_events_history.size());
        ASSERT_EQ(EVENT_HISTORY_RUUVI_SEND_NRF_SETTINGS_FROM_GW_CFG, this->m_events_history[0].event_type);
        ASSERT_EQ(EVENT_HISTORY_ADV_POST_CFG_CACHE_MUTEX_LOCK, this->m_events_history[1].event_type);
        ASSERT_EQ(EVENT_HISTORY_STOP_TIMER_SIG_RETRANSMIT_TO_HTTP_RUUVI, this->m_events_history[2].event_type);
        ASSERT_EQ(EVENT_HISTORY_STOP_TIMER_SIG_RETRANSMIT_TO_HTTP_CUSTOM, this->m_events_history[3].event_type);
        ASSERT_EQ(EVENT_HISTORY_STOP_TIMER_SIG_MQTT, this->m_events_history[4].event_type);
        ASSERT_EQ(EVENT_HISTORY_STOP_TIMER_SIG_SEND_STATISTICS, this->m_events_history[5].event_type);
        ASSERT_EQ(EVENT_HISTORY_ADV_POST_CFG_CACHE_MUTEX_UNLOCK, this->m_events_history[6].event_type);
        ASSERT_EQ(EVENT_HISTORY_ADV_TABLE_CLEAR, this->m_events_history[7].event_type);
        ASSERT_EQ(EVENT_HISTORY_HTTP_SERVER_MUTEX_DEACTIVATE, this->m_events_history[8].event_type);
        this->m_events_history.clear();
    }
    {
        this->m_gw_cfg.ruuvi_cfg.http_stat.use_http_stat    = false;
        this->m_gw_cfg.ruuvi_cfg.ntp.ntp_use                = true;
        this->m_adv_post_cfg_cache.p_arr_of_scan_filter_mac = nullptr;
        this->m_gw_cfg.ruuvi_cfg.http.use_http_ruuvi        = true;
        this->m_gw_cfg.ruuvi_cfg.http.use_http              = false;
        this->m_gw_cfg.ruuvi_cfg.mqtt.mqtt_sending_interval = 0;
        this->m_gw_cfg.ruuvi_cfg.mqtt.use_mqtt              = true;
        snprintf(
            this->m_gw_cfg.ruuvi_cfg.mqtt.mqtt_transport.buf,
            sizeof(this->m_gw_cfg.ruuvi_cfg.mqtt.mqtt_transport.buf),
            "%s",
            MQTT_TRANSPORT_WSS);
        ASSERT_FALSE(adv_post_handle_sig(ADV_POST_SIG_GW_CFG_CHANGED_RUUVI, &adv_post_state));
        ASSERT_FALSE(adv_post_state.flag_stop);
        ASSERT_TRUE(adv_post_state.flag_use_timestamps);
        ASSERT_TRUE(this->m_adv_post_cfg_cache.flag_use_ntp);
        ASSERT_FALSE(adv_post_state.flag_need_to_send_advs1);
        ASSERT_FALSE(adv_post_state.flag_need_to_send_advs2);
        ASSERT_EQ(9, this->m_events_history.size());
        ASSERT_EQ(EVENT_HISTORY_RUUVI_SEND_NRF_SETTINGS_FROM_GW_CFG, this->m_events_history[0].event_type);
        ASSERT_EQ(EVENT_HISTORY_ADV_POST_CFG_CACHE_MUTEX_LOCK, this->m_events_history[1].event_type);
        ASSERT_EQ(EVENT_HISTORY_ADV1_POST_TIMER_RESTART_WITH_DEFAULT_PERIOD, this->m_events_history[2].event_type);
        ASSERT_EQ(EVENT_HISTORY_STOP_TIMER_SIG_RETRANSMIT_TO_HTTP_CUSTOM, this->m_events_history[3].event_type);
        ASSERT_EQ(EVENT_HISTORY_STOP_TIMER_SIG_MQTT, this->m_events_history[4].event_type);
        ASSERT_EQ(EVENT_HISTORY_STOP_TIMER_SIG_SEND_STATISTICS, this->m_events_history[5].event_type);
        ASSERT_EQ(EVENT_HISTORY_ADV_POST_CFG_CACHE_MUTEX_UNLOCK, this->m_events_history[6].event_type);
        ASSERT_EQ(EVENT_HISTORY_ADV_TABLE_CLEAR, this->m_events_history[7].event_type);
        ASSERT_EQ(EVENT_HISTORY_HTTP_SERVER_MUTEX_ACTIVATE, this->m_events_history[8].event_type);
        this->m_events_history.clear();
    }
    {
        this->m_gw_cfg.ruuvi_cfg.http_stat.use_http_stat    = false;
        this->m_gw_cfg.ruuvi_cfg.ntp.ntp_use                = true;
        this->m_adv_post_cfg_cache.p_arr_of_scan_filter_mac = nullptr;
        this->m_gw_cfg.ruuvi_cfg.http.use_http_ruuvi        = false;
        this->m_gw_cfg.ruuvi_cfg.http.use_http              = true;
        this->m_gw_cfg.ruuvi_cfg.mqtt.mqtt_sending_interval = 0;
        this->m_gw_cfg.ruuvi_cfg.mqtt.use_mqtt              = true;
        snprintf(
            this->m_gw_cfg.ruuvi_cfg.mqtt.mqtt_transport.buf,
            sizeof(this->m_gw_cfg.ruuvi_cfg.mqtt.mqtt_transport.buf),
            "%s",
            MQTT_TRANSPORT_SSL);
        ASSERT_FALSE(adv_post_handle_sig(ADV_POST_SIG_GW_CFG_CHANGED_RUUVI, &adv_post_state));
        ASSERT_FALSE(adv_post_state.flag_stop);
        ASSERT_TRUE(adv_post_state.flag_use_timestamps);
        ASSERT_TRUE(this->m_adv_post_cfg_cache.flag_use_ntp);
        ASSERT_FALSE(adv_post_state.flag_need_to_send_advs1);
        ASSERT_FALSE(adv_post_state.flag_need_to_send_advs2);
        ASSERT_EQ(10, this->m_events_history.size());
        ASSERT_EQ(EVENT_HISTORY_RUUVI_SEND_NRF_SETTINGS_FROM_GW_CFG, this->m_events_history[0].event_type);
        ASSERT_EQ(EVENT_HISTORY_ADV_POST_CFG_CACHE_MUTEX_LOCK, this->m_events_history[1].event_type);
        ASSERT_EQ(EVENT_HISTORY_STOP_TIMER_SIG_RETRANSMIT_TO_HTTP_RUUVI, this->m_events_history[2].event_type);
        ASSERT_EQ(EVENT_HISTORY_ADV2_POST_TIMERS_SET_DEFAULT_PERIOD, this->m_events_history[3].event_type);
        ASSERT_EQ(EVENT_HISTORY_ADV2_POST_TIMER_RESTART_WITH_DEFAULT_PERIOD, this->m_events_history[4].event_type);
        ASSERT_EQ(EVENT_HISTORY_STOP_TIMER_SIG_MQTT, this->m_events_history[5].event_type);
        ASSERT_EQ(EVENT_HISTORY_STOP_TIMER_SIG_SEND_STATISTICS, this->m_events_history[6].event_type);
        ASSERT_EQ(EVENT_HISTORY_ADV_POST_CFG_CACHE_MUTEX_UNLOCK, this->m_events_history[7].event_type);
        ASSERT_EQ(EVENT_HISTORY_ADV_TABLE_CLEAR, this->m_events_history[8].event_type);
        ASSERT_EQ(EVENT_HISTORY_HTTP_SERVER_MUTEX_ACTIVATE, this->m_events_history[9].event_type);
        this->m_events_history.clear();
    }
    {
        this->m_gw_cfg.ruuvi_cfg.http_stat.use_http_stat    = false;
        this->m_gw_cfg.ruuvi_cfg.ntp.ntp_use                = true;
        this->m_adv_post_cfg_cache.p_arr_of_scan_filter_mac = nullptr;
        this->m_gw_cfg.ruuvi_cfg.http.use_http_ruuvi        = true;
        this->m_gw_cfg.ruuvi_cfg.http.use_http              = true;
        this->m_gw_cfg.ruuvi_cfg.mqtt.mqtt_sending_interval = 0;
        this->m_gw_cfg.ruuvi_cfg.mqtt.use_mqtt              = true;
        snprintf(
            this->m_gw_cfg.ruuvi_cfg.mqtt.mqtt_transport.buf,
            sizeof(this->m_gw_cfg.ruuvi_cfg.mqtt.mqtt_transport.buf),
            "%s",
            MQTT_TRANSPORT_WSS);
        ASSERT_FALSE(adv_post_handle_sig(ADV_POST_SIG_GW_CFG_CHANGED_RUUVI, &adv_post_state));
        ASSERT_FALSE(adv_post_state.flag_stop);
        ASSERT_TRUE(adv_post_state.flag_use_timestamps);
        ASSERT_TRUE(this->m_adv_post_cfg_cache.flag_use_ntp);
        ASSERT_FALSE(adv_post_state.flag_need_to_send_advs1);
        ASSERT_FALSE(adv_post_state.flag_need_to_send_advs2);
        ASSERT_EQ(10, this->m_events_history.size());
        ASSERT_EQ(EVENT_HISTORY_RUUVI_SEND_NRF_SETTINGS_FROM_GW_CFG, this->m_events_history[0].event_type);
        ASSERT_EQ(EVENT_HISTORY_ADV_POST_CFG_CACHE_MUTEX_LOCK, this->m_events_history[1].event_type);
        ASSERT_EQ(EVENT_HISTORY_ADV1_POST_TIMER_RESTART_WITH_DEFAULT_PERIOD, this->m_events_history[2].event_type);
        ASSERT_EQ(EVENT_HISTORY_ADV2_POST_TIMERS_SET_DEFAULT_PERIOD, this->m_events_history[3].event_type);
        ASSERT_EQ(EVENT_HISTORY_ADV2_POST_TIMER_RESTART_WITH_DEFAULT_PERIOD, this->m_events_history[4].event_type);
        ASSERT_EQ(EVENT_HISTORY_STOP_TIMER_SIG_MQTT, this->m_events_history[5].event_type);
        ASSERT_EQ(EVENT_HISTORY_STOP_TIMER_SIG_SEND_STATISTICS, this->m_events_history[6].event_type);
        ASSERT_EQ(EVENT_HISTORY_ADV_POST_CFG_CACHE_MUTEX_UNLOCK, this->m_events_history[7].event_type);
        ASSERT_EQ(EVENT_HISTORY_ADV_TABLE_CLEAR, this->m_events_history[8].event_type);
        ASSERT_EQ(EVENT_HISTORY_HTTP_SERVER_MUTEX_ACTIVATE, this->m_events_history[9].event_type);
        this->m_events_history.clear();
    }
    {
        this->m_gw_cfg.ruuvi_cfg.http_stat.use_http_stat    = false;
        this->m_gw_cfg.ruuvi_cfg.ntp.ntp_use                = true;
        this->m_adv_post_cfg_cache.p_arr_of_scan_filter_mac = nullptr;
        this->m_gw_cfg.ruuvi_cfg.http.use_http_ruuvi        = true;
        this->m_gw_cfg.ruuvi_cfg.http.use_http              = true;
        this->m_gw_cfg.ruuvi_cfg.mqtt.mqtt_sending_interval = 0;
        this->m_gw_cfg.ruuvi_cfg.mqtt.use_mqtt              = true;
        snprintf(
            this->m_gw_cfg.ruuvi_cfg.mqtt.mqtt_transport.buf,
            sizeof(this->m_gw_cfg.ruuvi_cfg.mqtt.mqtt_transport.buf),
            "%s",
            MQTT_TRANSPORT_TCP);
        ASSERT_FALSE(adv_post_handle_sig(ADV_POST_SIG_GW_CFG_CHANGED_RUUVI, &adv_post_state));
        ASSERT_FALSE(adv_post_state.flag_stop);
        ASSERT_TRUE(adv_post_state.flag_use_timestamps);
        ASSERT_TRUE(this->m_adv_post_cfg_cache.flag_use_ntp);
        ASSERT_FALSE(adv_post_state.flag_need_to_send_advs1);
        ASSERT_FALSE(adv_post_state.flag_need_to_send_advs2);
        ASSERT_EQ(10, this->m_events_history.size());
        ASSERT_EQ(EVENT_HISTORY_RUUVI_SEND_NRF_SETTINGS_FROM_GW_CFG, this->m_events_history[0].event_type);
        ASSERT_EQ(EVENT_HISTORY_ADV_POST_CFG_CACHE_MUTEX_LOCK, this->m_events_history[1].event_type);
        ASSERT_EQ(EVENT_HISTORY_ADV1_POST_TIMER_RESTART_WITH_DEFAULT_PERIOD, this->m_events_history[2].event_type);
        ASSERT_EQ(EVENT_HISTORY_ADV2_POST_TIMERS_SET_DEFAULT_PERIOD, this->m_events_history[3].event_type);
        ASSERT_EQ(EVENT_HISTORY_ADV2_POST_TIMER_RESTART_WITH_DEFAULT_PERIOD, this->m_events_history[4].event_type);
        ASSERT_EQ(EVENT_HISTORY_STOP_TIMER_SIG_MQTT, this->m_events_history[5].event_type);
        ASSERT_EQ(EVENT_HISTORY_STOP_TIMER_SIG_SEND_STATISTICS, this->m_events_history[6].event_type);
        ASSERT_EQ(EVENT_HISTORY_ADV_POST_CFG_CACHE_MUTEX_UNLOCK, this->m_events_history[7].event_type);
        ASSERT_EQ(EVENT_HISTORY_ADV_TABLE_CLEAR, this->m_events_history[8].event_type);
        ASSERT_EQ(EVENT_HISTORY_HTTP_SERVER_MUTEX_DEACTIVATE, this->m_events_history[9].event_type);
        this->m_events_history.clear();
    }
    {
        this->m_gw_cfg.ruuvi_cfg.http_stat.use_http_stat    = false;
        this->m_gw_cfg.ruuvi_cfg.ntp.ntp_use                = true;
        this->m_adv_post_cfg_cache.p_arr_of_scan_filter_mac = nullptr;
        this->m_gw_cfg.ruuvi_cfg.http.use_http_ruuvi        = true;
        this->m_gw_cfg.ruuvi_cfg.http.use_http              = true;
        this->m_gw_cfg.ruuvi_cfg.mqtt.mqtt_sending_interval = 0;
        this->m_gw_cfg.ruuvi_cfg.mqtt.use_mqtt              = true;
        snprintf(
            this->m_gw_cfg.ruuvi_cfg.mqtt.mqtt_transport.buf,
            sizeof(this->m_gw_cfg.ruuvi_cfg.mqtt.mqtt_transport.buf),
            "%s",
            MQTT_TRANSPORT_WS);
        ASSERT_FALSE(adv_post_handle_sig(ADV_POST_SIG_GW_CFG_CHANGED_RUUVI, &adv_post_state));
        ASSERT_FALSE(adv_post_state.flag_stop);
        ASSERT_TRUE(adv_post_state.flag_use_timestamps);
        ASSERT_TRUE(this->m_adv_post_cfg_cache.flag_use_ntp);
        ASSERT_FALSE(adv_post_state.flag_need_to_send_advs1);
        ASSERT_FALSE(adv_post_state.flag_need_to_send_advs2);
        ASSERT_EQ(10, this->m_events_history.size());
        ASSERT_EQ(EVENT_HISTORY_RUUVI_SEND_NRF_SETTINGS_FROM_GW_CFG, this->m_events_history[0].event_type);
        ASSERT_EQ(EVENT_HISTORY_ADV_POST_CFG_CACHE_MUTEX_LOCK, this->m_events_history[1].event_type);
        ASSERT_EQ(EVENT_HISTORY_ADV1_POST_TIMER_RESTART_WITH_DEFAULT_PERIOD, this->m_events_history[2].event_type);
        ASSERT_EQ(EVENT_HISTORY_ADV2_POST_TIMERS_SET_DEFAULT_PERIOD, this->m_events_history[3].event_type);
        ASSERT_EQ(EVENT_HISTORY_ADV2_POST_TIMER_RESTART_WITH_DEFAULT_PERIOD, this->m_events_history[4].event_type);
        ASSERT_EQ(EVENT_HISTORY_STOP_TIMER_SIG_MQTT, this->m_events_history[5].event_type);
        ASSERT_EQ(EVENT_HISTORY_STOP_TIMER_SIG_SEND_STATISTICS, this->m_events_history[6].event_type);
        ASSERT_EQ(EVENT_HISTORY_ADV_POST_CFG_CACHE_MUTEX_UNLOCK, this->m_events_history[7].event_type);
        ASSERT_EQ(EVENT_HISTORY_ADV_TABLE_CLEAR, this->m_events_history[8].event_type);
        ASSERT_EQ(EVENT_HISTORY_HTTP_SERVER_MUTEX_DEACTIVATE, this->m_events_history[9].event_type);
        this->m_events_history.clear();
    }
    {
        this->m_gw_cfg.ruuvi_cfg.http_stat.use_http_stat    = false;
        this->m_gw_cfg.ruuvi_cfg.ntp.ntp_use                = true;
        this->m_adv_post_cfg_cache.p_arr_of_scan_filter_mac = nullptr;
        this->m_gw_cfg.ruuvi_cfg.http.use_http_ruuvi        = true;
        this->m_gw_cfg.ruuvi_cfg.http.use_http              = false;
        this->m_gw_cfg.ruuvi_cfg.mqtt.mqtt_sending_interval = 10;
        this->m_gw_cfg.ruuvi_cfg.mqtt.use_mqtt              = true;
        snprintf(
            this->m_gw_cfg.ruuvi_cfg.mqtt.mqtt_transport.buf,
            sizeof(this->m_gw_cfg.ruuvi_cfg.mqtt.mqtt_transport.buf),
            "%s",
            MQTT_TRANSPORT_SSL);
        ASSERT_FALSE(adv_post_handle_sig(ADV_POST_SIG_GW_CFG_CHANGED_RUUVI, &adv_post_state));
        ASSERT_FALSE(adv_post_state.flag_stop);
        ASSERT_TRUE(adv_post_state.flag_use_timestamps);
        ASSERT_TRUE(this->m_adv_post_cfg_cache.flag_use_ntp);
        ASSERT_FALSE(adv_post_state.flag_need_to_send_advs1);
        ASSERT_FALSE(adv_post_state.flag_need_to_send_advs2);
        ASSERT_EQ(9, this->m_events_history.size());
        ASSERT_EQ(EVENT_HISTORY_RUUVI_SEND_NRF_SETTINGS_FROM_GW_CFG, this->m_events_history[0].event_type);
        ASSERT_EQ(EVENT_HISTORY_ADV_POST_CFG_CACHE_MUTEX_LOCK, this->m_events_history[1].event_type);
        ASSERT_EQ(EVENT_HISTORY_ADV1_POST_TIMER_RESTART_WITH_DEFAULT_PERIOD, this->m_events_history[2].event_type);
        ASSERT_EQ(EVENT_HISTORY_STOP_TIMER_SIG_RETRANSMIT_TO_HTTP_CUSTOM, this->m_events_history[3].event_type);
        ASSERT_EQ(EVENT_HISTORY_RESTART_TIMER_SIG_MQTT, this->m_events_history[4].event_type);
        ASSERT_EQ(EVENT_HISTORY_STOP_TIMER_SIG_SEND_STATISTICS, this->m_events_history[5].event_type);
        ASSERT_EQ(EVENT_HISTORY_ADV_POST_CFG_CACHE_MUTEX_UNLOCK, this->m_events_history[6].event_type);
        ASSERT_EQ(EVENT_HISTORY_ADV_TABLE_CLEAR, this->m_events_history[7].event_type);
        ASSERT_EQ(EVENT_HISTORY_HTTP_SERVER_MUTEX_ACTIVATE, this->m_events_history[8].event_type);
        this->m_events_history.clear();
    }

    {
        this->m_gw_cfg.ruuvi_cfg.scan_filter.scan_filter_allow_listed = false;
        this->m_gw_cfg.ruuvi_cfg.scan_filter.scan_filter_length       = 2;
        this->m_gw_cfg.ruuvi_cfg.scan_filter.scan_filter_list[0]      = { 0x11, 0x12, 0x13, 0x14, 0x15, 0x16 };
        this->m_gw_cfg.ruuvi_cfg.scan_filter.scan_filter_list[1]      = { 0x11, 0x12, 0x13, 0x14, 0x15, 0x17 };
        this->m_gw_cfg.ruuvi_cfg.http_stat.use_http_stat              = false;
        this->m_gw_cfg.ruuvi_cfg.ntp.ntp_use                          = true;
        this->m_adv_post_cfg_cache.p_arr_of_scan_filter_mac           = nullptr;
        this->m_gw_cfg.ruuvi_cfg.http.use_http_ruuvi                  = true;
        this->m_gw_cfg.ruuvi_cfg.http.use_http                        = false;
        this->m_gw_cfg.ruuvi_cfg.mqtt.mqtt_sending_interval           = 0;
        this->m_gw_cfg.ruuvi_cfg.mqtt.use_mqtt                        = false;
        ASSERT_FALSE(adv_post_handle_sig(ADV_POST_SIG_GW_CFG_CHANGED_RUUVI, &adv_post_state));
        ASSERT_FALSE(adv_post_state.flag_stop);
        ASSERT_TRUE(adv_post_state.flag_use_timestamps);
        ASSERT_TRUE(this->m_adv_post_cfg_cache.flag_use_ntp);
        ASSERT_FALSE(adv_post_state.flag_need_to_send_advs1);
        ASSERT_FALSE(adv_post_state.flag_need_to_send_advs2);
        ASSERT_EQ(9, this->m_events_history.size());
        ASSERT_EQ(EVENT_HISTORY_RUUVI_SEND_NRF_SETTINGS_FROM_GW_CFG, this->m_events_history[0].event_type);
        ASSERT_EQ(EVENT_HISTORY_ADV_POST_CFG_CACHE_MUTEX_LOCK, this->m_events_history[1].event_type);
        ASSERT_EQ(EVENT_HISTORY_ADV1_POST_TIMER_RESTART_WITH_DEFAULT_PERIOD, this->m_events_history[2].event_type);
        ASSERT_EQ(EVENT_HISTORY_STOP_TIMER_SIG_RETRANSMIT_TO_HTTP_CUSTOM, this->m_events_history[3].event_type);
        ASSERT_EQ(EVENT_HISTORY_STOP_TIMER_SIG_MQTT, this->m_events_history[4].event_type);
        ASSERT_EQ(EVENT_HISTORY_STOP_TIMER_SIG_SEND_STATISTICS, this->m_events_history[5].event_type);
        ASSERT_EQ(EVENT_HISTORY_ADV_POST_CFG_CACHE_MUTEX_UNLOCK, this->m_events_history[6].event_type);
        ASSERT_EQ(EVENT_HISTORY_ADV_TABLE_CLEAR, this->m_events_history[7].event_type);
        ASSERT_EQ(EVENT_HISTORY_HTTP_SERVER_MUTEX_DEACTIVATE, this->m_events_history[8].event_type);
        ASSERT_EQ(2, this->m_adv_post_cfg_cache.scan_filter_length);
        ASSERT_EQ(false, this->m_adv_post_cfg_cache.scan_filter_allow_listed);
        ASSERT_EQ(
            0,
            memcmp(
                this->m_adv_post_cfg_cache.p_arr_of_scan_filter_mac,
                this->m_gw_cfg.ruuvi_cfg.scan_filter.scan_filter_list,
                sizeof(mac_address_bin_t) * this->m_adv_post_cfg_cache.scan_filter_length));
        this->m_events_history.clear();
    }
    {
        this->m_gw_cfg.ruuvi_cfg.scan_filter.scan_filter_allow_listed = true;
        this->m_gw_cfg.ruuvi_cfg.scan_filter.scan_filter_length       = 1;
        this->m_gw_cfg.ruuvi_cfg.scan_filter.scan_filter_list[0]      = { 0x11, 0x12, 0x13, 0x14, 0x15, 0x18 };
        this->m_gw_cfg.ruuvi_cfg.http_stat.use_http_stat              = false;
        this->m_gw_cfg.ruuvi_cfg.ntp.ntp_use                          = true;
        this->m_gw_cfg.ruuvi_cfg.http.use_http_ruuvi                  = true;
        this->m_gw_cfg.ruuvi_cfg.http.use_http                        = false;
        this->m_gw_cfg.ruuvi_cfg.mqtt.mqtt_sending_interval           = 0;
        this->m_gw_cfg.ruuvi_cfg.mqtt.use_mqtt                        = false;
        ASSERT_FALSE(adv_post_handle_sig(ADV_POST_SIG_GW_CFG_CHANGED_RUUVI, &adv_post_state));
        ASSERT_FALSE(adv_post_state.flag_stop);
        ASSERT_TRUE(adv_post_state.flag_use_timestamps);
        ASSERT_TRUE(this->m_adv_post_cfg_cache.flag_use_ntp);
        ASSERT_FALSE(adv_post_state.flag_need_to_send_advs1);
        ASSERT_FALSE(adv_post_state.flag_need_to_send_advs2);
        ASSERT_EQ(9, this->m_events_history.size());
        ASSERT_EQ(EVENT_HISTORY_RUUVI_SEND_NRF_SETTINGS_FROM_GW_CFG, this->m_events_history[0].event_type);
        ASSERT_EQ(EVENT_HISTORY_ADV_POST_CFG_CACHE_MUTEX_LOCK, this->m_events_history[1].event_type);
        ASSERT_EQ(EVENT_HISTORY_ADV1_POST_TIMER_RESTART_WITH_DEFAULT_PERIOD, this->m_events_history[2].event_type);
        ASSERT_EQ(EVENT_HISTORY_STOP_TIMER_SIG_RETRANSMIT_TO_HTTP_CUSTOM, this->m_events_history[3].event_type);
        ASSERT_EQ(EVENT_HISTORY_STOP_TIMER_SIG_MQTT, this->m_events_history[4].event_type);
        ASSERT_EQ(EVENT_HISTORY_STOP_TIMER_SIG_SEND_STATISTICS, this->m_events_history[5].event_type);
        ASSERT_EQ(EVENT_HISTORY_ADV_POST_CFG_CACHE_MUTEX_UNLOCK, this->m_events_history[6].event_type);
        ASSERT_EQ(EVENT_HISTORY_ADV_TABLE_CLEAR, this->m_events_history[7].event_type);
        ASSERT_EQ(EVENT_HISTORY_HTTP_SERVER_MUTEX_DEACTIVATE, this->m_events_history[8].event_type);
        ASSERT_EQ(1, this->m_adv_post_cfg_cache.scan_filter_length);
        ASSERT_EQ(true, this->m_adv_post_cfg_cache.scan_filter_allow_listed);
        ASSERT_EQ(
            0,
            memcmp(
                this->m_adv_post_cfg_cache.p_arr_of_scan_filter_mac,
                this->m_gw_cfg.ruuvi_cfg.scan_filter.scan_filter_list,
                sizeof(mac_address_bin_t) * this->m_adv_post_cfg_cache.scan_filter_length));
        this->m_events_history.clear();
    }
    {
        this->m_gw_cfg.ruuvi_cfg.scan_filter.scan_filter_allow_listed = true;
        this->m_gw_cfg.ruuvi_cfg.scan_filter.scan_filter_length       = 0;
        this->m_gw_cfg.ruuvi_cfg.http_stat.use_http_stat              = false;
        this->m_gw_cfg.ruuvi_cfg.ntp.ntp_use                          = true;
        this->m_gw_cfg.ruuvi_cfg.http.use_http_ruuvi                  = true;
        this->m_gw_cfg.ruuvi_cfg.http.use_http                        = false;
        this->m_gw_cfg.ruuvi_cfg.mqtt.mqtt_sending_interval           = 0;
        this->m_gw_cfg.ruuvi_cfg.mqtt.use_mqtt                        = false;
        ASSERT_FALSE(adv_post_handle_sig(ADV_POST_SIG_GW_CFG_CHANGED_RUUVI, &adv_post_state));
        ASSERT_FALSE(adv_post_state.flag_stop);
        ASSERT_TRUE(adv_post_state.flag_use_timestamps);
        ASSERT_TRUE(this->m_adv_post_cfg_cache.flag_use_ntp);
        ASSERT_FALSE(adv_post_state.flag_need_to_send_advs1);
        ASSERT_FALSE(adv_post_state.flag_need_to_send_advs2);
        ASSERT_EQ(9, this->m_events_history.size());
        ASSERT_EQ(EVENT_HISTORY_RUUVI_SEND_NRF_SETTINGS_FROM_GW_CFG, this->m_events_history[0].event_type);
        ASSERT_EQ(EVENT_HISTORY_ADV_POST_CFG_CACHE_MUTEX_LOCK, this->m_events_history[1].event_type);
        ASSERT_EQ(EVENT_HISTORY_ADV1_POST_TIMER_RESTART_WITH_DEFAULT_PERIOD, this->m_events_history[2].event_type);
        ASSERT_EQ(EVENT_HISTORY_STOP_TIMER_SIG_RETRANSMIT_TO_HTTP_CUSTOM, this->m_events_history[3].event_type);
        ASSERT_EQ(EVENT_HISTORY_STOP_TIMER_SIG_MQTT, this->m_events_history[4].event_type);
        ASSERT_EQ(EVENT_HISTORY_STOP_TIMER_SIG_SEND_STATISTICS, this->m_events_history[5].event_type);
        ASSERT_EQ(EVENT_HISTORY_ADV_POST_CFG_CACHE_MUTEX_UNLOCK, this->m_events_history[6].event_type);
        ASSERT_EQ(EVENT_HISTORY_ADV_TABLE_CLEAR, this->m_events_history[7].event_type);
        ASSERT_EQ(EVENT_HISTORY_HTTP_SERVER_MUTEX_DEACTIVATE, this->m_events_history[8].event_type);
        ASSERT_EQ(0, this->m_adv_post_cfg_cache.scan_filter_length);
        ASSERT_EQ(true, this->m_adv_post_cfg_cache.scan_filter_allow_listed);
        ASSERT_EQ(nullptr, this->m_adv_post_cfg_cache.p_arr_of_scan_filter_mac);
        this->m_events_history.clear();
    }
    ASSERT_TRUE(this->m_mem_alloc_trace.is_empty());
    {
        this->m_malloc_cnt                                            = 0;
        this->m_malloc_fail_on_cnt                                    = 1;
        this->m_gw_cfg.ruuvi_cfg.scan_filter.scan_filter_allow_listed = true;
        this->m_gw_cfg.ruuvi_cfg.scan_filter.scan_filter_length       = 1;
        this->m_gw_cfg.ruuvi_cfg.scan_filter.scan_filter_list[0]      = { 0x11, 0x12, 0x13, 0x14, 0x15, 0x18 };
        this->m_gw_cfg.ruuvi_cfg.http_stat.use_http_stat              = false;
        this->m_gw_cfg.ruuvi_cfg.ntp.ntp_use                          = true;
        this->m_gw_cfg.ruuvi_cfg.http.use_http_ruuvi                  = true;
        this->m_gw_cfg.ruuvi_cfg.http.use_http                        = false;
        this->m_gw_cfg.ruuvi_cfg.mqtt.mqtt_sending_interval           = 0;
        this->m_gw_cfg.ruuvi_cfg.mqtt.use_mqtt                        = false;
        ASSERT_FALSE(adv_post_handle_sig(ADV_POST_SIG_GW_CFG_CHANGED_RUUVI, &adv_post_state));
        ASSERT_FALSE(adv_post_state.flag_stop);
        ASSERT_TRUE(adv_post_state.flag_use_timestamps);
        ASSERT_TRUE(this->m_adv_post_cfg_cache.flag_use_ntp);
        ASSERT_FALSE(adv_post_state.flag_need_to_send_advs1);
        ASSERT_FALSE(adv_post_state.flag_need_to_send_advs2);
        ASSERT_EQ(4, this->m_events_history.size());
        ASSERT_EQ(EVENT_HISTORY_RUUVI_SEND_NRF_SETTINGS_FROM_GW_CFG, this->m_events_history[0].event_type);
        ASSERT_EQ(EVENT_HISTORY_ADV_POST_CFG_CACHE_MUTEX_LOCK, this->m_events_history[1].event_type);
        ASSERT_EQ(EVENT_HISTORY_GATEWAY_RESTART, this->m_events_history[2].event_type);
        ASSERT_EQ(EVENT_HISTORY_HTTP_SERVER_MUTEX_DEACTIVATE, this->m_events_history[3].event_type);
        ASSERT_EQ(0, this->m_adv_post_cfg_cache.scan_filter_length);
        ASSERT_EQ(true, this->m_adv_post_cfg_cache.scan_filter_allow_listed);
        ASSERT_EQ(nullptr, this->m_adv_post_cfg_cache.p_arr_of_scan_filter_mac);
        this->m_events_history.clear();
    }

    ASSERT_TRUE(this->m_mem_alloc_trace.is_empty());
}

TEST_F(TestAdvPostSignals, test_adv_post_handle_sig_ble_scan_changed) // NOLINT
{
    adv_post_signals_init();
    this->m_events_history.clear();

    adv_post_state_t adv_post_state = {
        .flag_primary_time_sync_is_done  = true,
        .flag_network_connected          = true,
        .flag_async_comm_in_progress     = false,
        .flag_need_to_send_advs1         = false,
        .flag_need_to_send_advs2         = false,
        .flag_need_to_send_statistics    = false,
        .flag_need_to_send_mqtt_periodic = false,
        .flag_relaying_enabled           = true,
        .flag_use_timestamps             = true,
        .flag_stop                       = false,
    };

    {
        ASSERT_FALSE(adv_post_handle_sig(ADV_POST_SIG_BLE_SCAN_CHANGED, &adv_post_state));
        ASSERT_FALSE(adv_post_state.flag_stop);
        ASSERT_EQ(1, this->m_events_history.size());
        ASSERT_EQ(EVENT_HISTORY_ADV_TABLE_CLEAR, this->m_events_history[0].event_type);
        this->m_events_history.clear();
    }
    ASSERT_TRUE(this->m_mem_alloc_trace.is_empty());
}

TEST_F(TestAdvPostSignals, test_adv_post_handle_sig_activate_cfg_mode) // NOLINT
{
    adv_post_signals_init();
    this->m_events_history.clear();

    adv_post_state_t adv_post_state = {
        .flag_primary_time_sync_is_done  = true,
        .flag_network_connected          = true,
        .flag_async_comm_in_progress     = false,
        .flag_need_to_send_advs1         = false,
        .flag_need_to_send_advs2         = false,
        .flag_need_to_send_statistics    = false,
        .flag_need_to_send_mqtt_periodic = false,
        .flag_relaying_enabled           = true,
        .flag_use_timestamps             = true,
        .flag_stop                       = false,
    };

    {
        ASSERT_FALSE(adv_post_handle_sig(ADV_POST_SIG_CFG_MODE_ACTIVATED, &adv_post_state));
        ASSERT_FALSE(adv_post_state.flag_stop);
        ASSERT_EQ(3, this->m_events_history.size());
        ASSERT_EQ(EVENT_HISTORY_ADV_POST_CFG_CACHE_MUTEX_LOCK, this->m_events_history[0].event_type);
        ASSERT_EQ(EVENT_HISTORY_ADV_POST_CFG_CACHE_MUTEX_UNLOCK, this->m_events_history[1].event_type);
        ASSERT_EQ(EVENT_HISTORY_ADV_TABLE_CLEAR, this->m_events_history[2].event_type);
        this->m_events_history.clear();
    }
    {
        this->m_gw_cfg.ruuvi_cfg.scan_filter.scan_filter_allow_listed = false;
        this->m_gw_cfg.ruuvi_cfg.scan_filter.scan_filter_length       = 2;
        this->m_gw_cfg.ruuvi_cfg.scan_filter.scan_filter_list[0]      = { 0x11, 0x12, 0x13, 0x14, 0x15, 0x16 };
        this->m_gw_cfg.ruuvi_cfg.scan_filter.scan_filter_list[1]      = { 0x11, 0x12, 0x13, 0x14, 0x15, 0x17 };
        this->m_gw_cfg.ruuvi_cfg.http_stat.use_http_stat              = false;
        this->m_gw_cfg.ruuvi_cfg.ntp.ntp_use                          = true;
        this->m_adv_post_cfg_cache.p_arr_of_scan_filter_mac           = nullptr;
        this->m_gw_cfg.ruuvi_cfg.http.use_http_ruuvi                  = true;
        this->m_gw_cfg.ruuvi_cfg.http.use_http                        = false;
        this->m_gw_cfg.ruuvi_cfg.mqtt.mqtt_sending_interval           = 0;
        this->m_gw_cfg.ruuvi_cfg.mqtt.use_mqtt                        = false;
        ASSERT_FALSE(adv_post_handle_sig(ADV_POST_SIG_GW_CFG_CHANGED_RUUVI, &adv_post_state));
        ASSERT_FALSE(adv_post_state.flag_stop);
        ASSERT_TRUE(adv_post_state.flag_use_timestamps);
        ASSERT_TRUE(this->m_adv_post_cfg_cache.flag_use_ntp);
        ASSERT_FALSE(adv_post_state.flag_need_to_send_advs1);
        ASSERT_FALSE(adv_post_state.flag_need_to_send_advs2);
        ASSERT_EQ(9, this->m_events_history.size());
        ASSERT_EQ(EVENT_HISTORY_RUUVI_SEND_NRF_SETTINGS_FROM_GW_CFG, this->m_events_history[0].event_type);
        ASSERT_EQ(EVENT_HISTORY_ADV_POST_CFG_CACHE_MUTEX_LOCK, this->m_events_history[1].event_type);
        ASSERT_EQ(EVENT_HISTORY_ADV1_POST_TIMER_RESTART_WITH_DEFAULT_PERIOD, this->m_events_history[2].event_type);
        ASSERT_EQ(EVENT_HISTORY_STOP_TIMER_SIG_RETRANSMIT_TO_HTTP_CUSTOM, this->m_events_history[3].event_type);
        ASSERT_EQ(EVENT_HISTORY_STOP_TIMER_SIG_MQTT, this->m_events_history[4].event_type);
        ASSERT_EQ(EVENT_HISTORY_STOP_TIMER_SIG_SEND_STATISTICS, this->m_events_history[5].event_type);
        ASSERT_EQ(EVENT_HISTORY_ADV_POST_CFG_CACHE_MUTEX_UNLOCK, this->m_events_history[6].event_type);
        ASSERT_EQ(EVENT_HISTORY_ADV_TABLE_CLEAR, this->m_events_history[7].event_type);
        ASSERT_EQ(EVENT_HISTORY_HTTP_SERVER_MUTEX_DEACTIVATE, this->m_events_history[8].event_type);
        ASSERT_EQ(2, this->m_adv_post_cfg_cache.scan_filter_length);
        ASSERT_EQ(false, this->m_adv_post_cfg_cache.scan_filter_allow_listed);
        ASSERT_EQ(
            0,
            memcmp(
                this->m_adv_post_cfg_cache.p_arr_of_scan_filter_mac,
                this->m_gw_cfg.ruuvi_cfg.scan_filter.scan_filter_list,
                sizeof(mac_address_bin_t) * this->m_adv_post_cfg_cache.scan_filter_length));
        this->m_events_history.clear();
    }
    {
        ASSERT_FALSE(adv_post_handle_sig(ADV_POST_SIG_CFG_MODE_ACTIVATED, &adv_post_state));
        ASSERT_FALSE(adv_post_state.flag_stop);
        ASSERT_EQ(3, this->m_events_history.size());
        ASSERT_EQ(EVENT_HISTORY_ADV_POST_CFG_CACHE_MUTEX_LOCK, this->m_events_history[0].event_type);
        ASSERT_EQ(EVENT_HISTORY_ADV_POST_CFG_CACHE_MUTEX_UNLOCK, this->m_events_history[1].event_type);
        ASSERT_EQ(EVENT_HISTORY_ADV_TABLE_CLEAR, this->m_events_history[2].event_type);
        this->m_events_history.clear();
    }
    ASSERT_TRUE(this->m_mem_alloc_trace.is_empty());
}

TEST_F(TestAdvPostSignals, test_adv_post_handle_sig_deactivate_cfg_mode) // NOLINT
{
    adv_post_signals_init();
    this->m_events_history.clear();

    adv_post_state_t adv_post_state = {
        .flag_primary_time_sync_is_done  = true,
        .flag_network_connected          = true,
        .flag_async_comm_in_progress     = false,
        .flag_need_to_send_advs1         = false,
        .flag_need_to_send_advs2         = false,
        .flag_need_to_send_statistics    = false,
        .flag_need_to_send_mqtt_periodic = false,
        .flag_relaying_enabled           = true,
        .flag_use_timestamps             = true,
        .flag_stop                       = false,
    };

    {
        this->m_gw_cfg.ruuvi_cfg.http_stat.use_http_stat    = true;
        this->m_gw_cfg.ruuvi_cfg.ntp.ntp_use                = true;
        this->m_adv_post_cfg_cache.p_arr_of_scan_filter_mac = nullptr;
        this->m_gw_cfg.ruuvi_cfg.http.use_http_ruuvi        = true;
        this->m_gw_cfg.ruuvi_cfg.http.use_http              = false;
        this->m_gw_cfg.ruuvi_cfg.mqtt.mqtt_sending_interval = 0;
        this->m_gw_cfg.ruuvi_cfg.mqtt.use_mqtt              = false;
        ASSERT_FALSE(adv_post_handle_sig(ADV_POST_SIG_CFG_MODE_DEACTIVATED, &adv_post_state));
        ASSERT_FALSE(adv_post_state.flag_stop);
        ASSERT_TRUE(adv_post_state.flag_use_timestamps);
        ASSERT_TRUE(this->m_adv_post_cfg_cache.flag_use_ntp);
        ASSERT_FALSE(adv_post_state.flag_need_to_send_advs1);
        ASSERT_FALSE(adv_post_state.flag_need_to_send_advs2);
        ASSERT_EQ(8, this->m_events_history.size());
        ASSERT_EQ(EVENT_HISTORY_RUUVI_SEND_NRF_SETTINGS_FROM_GW_CFG, this->m_events_history[0].event_type);
        ASSERT_EQ(EVENT_HISTORY_ADV_POST_CFG_CACHE_MUTEX_LOCK, this->m_events_history[1].event_type);
        ASSERT_EQ(EVENT_HISTORY_ADV1_POST_TIMER_RESTART_WITH_DEFAULT_PERIOD, this->m_events_history[2].event_type);
        ASSERT_EQ(EVENT_HISTORY_STOP_TIMER_SIG_RETRANSMIT_TO_HTTP_CUSTOM, this->m_events_history[3].event_type);
        ASSERT_EQ(EVENT_HISTORY_STOP_TIMER_SIG_MQTT, this->m_events_history[4].event_type);
        ASSERT_EQ(EVENT_HISTORY_RELAUNCH_TIMER_SIG_SEND_STATISTICS, this->m_events_history[5].event_type);
        ASSERT_EQ(EVENT_HISTORY_ADV_POST_CFG_CACHE_MUTEX_UNLOCK, this->m_events_history[6].event_type);
        ASSERT_EQ(EVENT_HISTORY_ADV_TABLE_CLEAR, this->m_events_history[7].event_type);
        this->m_events_history.clear();
    }

    {
        this->m_gw_cfg.ruuvi_cfg.http_stat.use_http_stat    = false;
        this->m_gw_cfg.ruuvi_cfg.ntp.ntp_use                = true;
        this->m_adv_post_cfg_cache.p_arr_of_scan_filter_mac = nullptr;
        this->m_gw_cfg.ruuvi_cfg.http.use_http_ruuvi        = true;
        this->m_gw_cfg.ruuvi_cfg.http.use_http              = false;
        this->m_gw_cfg.ruuvi_cfg.mqtt.mqtt_sending_interval = 0;
        this->m_gw_cfg.ruuvi_cfg.mqtt.use_mqtt              = false;
        ASSERT_FALSE(adv_post_handle_sig(ADV_POST_SIG_CFG_MODE_DEACTIVATED, &adv_post_state));
        ASSERT_FALSE(adv_post_state.flag_stop);
        ASSERT_TRUE(adv_post_state.flag_use_timestamps);
        ASSERT_TRUE(this->m_adv_post_cfg_cache.flag_use_ntp);
        ASSERT_FALSE(adv_post_state.flag_need_to_send_advs1);
        ASSERT_FALSE(adv_post_state.flag_need_to_send_advs2);
        ASSERT_EQ(8, this->m_events_history.size());
        ASSERT_EQ(EVENT_HISTORY_RUUVI_SEND_NRF_SETTINGS_FROM_GW_CFG, this->m_events_history[0].event_type);
        ASSERT_EQ(EVENT_HISTORY_ADV_POST_CFG_CACHE_MUTEX_LOCK, this->m_events_history[1].event_type);
        ASSERT_EQ(EVENT_HISTORY_ADV1_POST_TIMER_RESTART_WITH_DEFAULT_PERIOD, this->m_events_history[2].event_type);
        ASSERT_EQ(EVENT_HISTORY_STOP_TIMER_SIG_RETRANSMIT_TO_HTTP_CUSTOM, this->m_events_history[3].event_type);
        ASSERT_EQ(EVENT_HISTORY_STOP_TIMER_SIG_MQTT, this->m_events_history[4].event_type);
        ASSERT_EQ(EVENT_HISTORY_STOP_TIMER_SIG_SEND_STATISTICS, this->m_events_history[5].event_type);
        ASSERT_EQ(EVENT_HISTORY_ADV_POST_CFG_CACHE_MUTEX_UNLOCK, this->m_events_history[6].event_type);
        ASSERT_EQ(EVENT_HISTORY_ADV_TABLE_CLEAR, this->m_events_history[7].event_type);
        this->m_events_history.clear();
    }
    {
        this->m_gw_cfg.ruuvi_cfg.http_stat.use_http_stat    = false;
        this->m_gw_cfg.ruuvi_cfg.ntp.ntp_use                = true;
        this->m_adv_post_cfg_cache.p_arr_of_scan_filter_mac = nullptr;
        this->m_gw_cfg.ruuvi_cfg.http.use_http_ruuvi        = true;
        this->m_gw_cfg.ruuvi_cfg.http.use_http              = false;
        this->m_gw_cfg.ruuvi_cfg.mqtt.mqtt_sending_interval = 0;
        this->m_gw_cfg.ruuvi_cfg.mqtt.use_mqtt              = true;
        snprintf(
            this->m_gw_cfg.ruuvi_cfg.mqtt.mqtt_transport.buf,
            sizeof(this->m_gw_cfg.ruuvi_cfg.mqtt.mqtt_transport.buf),
            "%s",
            MQTT_TRANSPORT_SSL);
        ASSERT_FALSE(adv_post_handle_sig(ADV_POST_SIG_CFG_MODE_DEACTIVATED, &adv_post_state));
        ASSERT_FALSE(adv_post_state.flag_stop);
        ASSERT_TRUE(adv_post_state.flag_use_timestamps);
        ASSERT_TRUE(this->m_adv_post_cfg_cache.flag_use_ntp);
        ASSERT_FALSE(adv_post_state.flag_need_to_send_advs1);
        ASSERT_FALSE(adv_post_state.flag_need_to_send_advs2);
        ASSERT_EQ(8, this->m_events_history.size());
        ASSERT_EQ(EVENT_HISTORY_RUUVI_SEND_NRF_SETTINGS_FROM_GW_CFG, this->m_events_history[0].event_type);
        ASSERT_EQ(EVENT_HISTORY_ADV_POST_CFG_CACHE_MUTEX_LOCK, this->m_events_history[1].event_type);
        ASSERT_EQ(EVENT_HISTORY_ADV1_POST_TIMER_RESTART_WITH_DEFAULT_PERIOD, this->m_events_history[2].event_type);
        ASSERT_EQ(EVENT_HISTORY_STOP_TIMER_SIG_RETRANSMIT_TO_HTTP_CUSTOM, this->m_events_history[3].event_type);
        ASSERT_EQ(EVENT_HISTORY_STOP_TIMER_SIG_MQTT, this->m_events_history[4].event_type);
        ASSERT_EQ(EVENT_HISTORY_STOP_TIMER_SIG_SEND_STATISTICS, this->m_events_history[5].event_type);
        ASSERT_EQ(EVENT_HISTORY_ADV_POST_CFG_CACHE_MUTEX_UNLOCK, this->m_events_history[6].event_type);
        ASSERT_EQ(EVENT_HISTORY_ADV_TABLE_CLEAR, this->m_events_history[7].event_type);
        this->m_events_history.clear();
    }

    {
        this->m_gw_cfg.ruuvi_cfg.scan_filter.scan_filter_allow_listed = false;
        this->m_gw_cfg.ruuvi_cfg.scan_filter.scan_filter_length       = 2;
        this->m_gw_cfg.ruuvi_cfg.scan_filter.scan_filter_list[0]      = { 0x11, 0x12, 0x13, 0x14, 0x15, 0x16 };
        this->m_gw_cfg.ruuvi_cfg.scan_filter.scan_filter_list[1]      = { 0x11, 0x12, 0x13, 0x14, 0x15, 0x17 };
        this->m_gw_cfg.ruuvi_cfg.http_stat.use_http_stat              = false;
        this->m_gw_cfg.ruuvi_cfg.ntp.ntp_use                          = true;
        this->m_adv_post_cfg_cache.p_arr_of_scan_filter_mac           = nullptr;
        this->m_gw_cfg.ruuvi_cfg.http.use_http_ruuvi                  = true;
        this->m_gw_cfg.ruuvi_cfg.http.use_http                        = false;
        this->m_gw_cfg.ruuvi_cfg.mqtt.mqtt_sending_interval           = 0;
        this->m_gw_cfg.ruuvi_cfg.mqtt.use_mqtt                        = false;
        ASSERT_FALSE(adv_post_handle_sig(ADV_POST_SIG_CFG_MODE_DEACTIVATED, &adv_post_state));
        ASSERT_FALSE(adv_post_state.flag_stop);
        ASSERT_TRUE(adv_post_state.flag_use_timestamps);
        ASSERT_TRUE(this->m_adv_post_cfg_cache.flag_use_ntp);
        ASSERT_FALSE(adv_post_state.flag_need_to_send_advs1);
        ASSERT_FALSE(adv_post_state.flag_need_to_send_advs2);
        ASSERT_EQ(8, this->m_events_history.size());
        ASSERT_EQ(EVENT_HISTORY_RUUVI_SEND_NRF_SETTINGS_FROM_GW_CFG, this->m_events_history[0].event_type);
        ASSERT_EQ(EVENT_HISTORY_ADV_POST_CFG_CACHE_MUTEX_LOCK, this->m_events_history[1].event_type);
        ASSERT_EQ(EVENT_HISTORY_ADV1_POST_TIMER_RESTART_WITH_DEFAULT_PERIOD, this->m_events_history[2].event_type);
        ASSERT_EQ(EVENT_HISTORY_STOP_TIMER_SIG_RETRANSMIT_TO_HTTP_CUSTOM, this->m_events_history[3].event_type);
        ASSERT_EQ(EVENT_HISTORY_STOP_TIMER_SIG_MQTT, this->m_events_history[4].event_type);
        ASSERT_EQ(EVENT_HISTORY_STOP_TIMER_SIG_SEND_STATISTICS, this->m_events_history[5].event_type);
        ASSERT_EQ(EVENT_HISTORY_ADV_POST_CFG_CACHE_MUTEX_UNLOCK, this->m_events_history[6].event_type);
        ASSERT_EQ(EVENT_HISTORY_ADV_TABLE_CLEAR, this->m_events_history[7].event_type);
        ASSERT_EQ(2, this->m_adv_post_cfg_cache.scan_filter_length);
        ASSERT_EQ(false, this->m_adv_post_cfg_cache.scan_filter_allow_listed);
        ASSERT_EQ(
            0,
            memcmp(
                this->m_adv_post_cfg_cache.p_arr_of_scan_filter_mac,
                this->m_gw_cfg.ruuvi_cfg.scan_filter.scan_filter_list,
                sizeof(mac_address_bin_t) * this->m_adv_post_cfg_cache.scan_filter_length));
        this->m_events_history.clear();
    }
    {
        this->m_gw_cfg.ruuvi_cfg.scan_filter.scan_filter_allow_listed = true;
        this->m_gw_cfg.ruuvi_cfg.scan_filter.scan_filter_length       = 1;
        this->m_gw_cfg.ruuvi_cfg.scan_filter.scan_filter_list[0]      = { 0x11, 0x12, 0x13, 0x14, 0x15, 0x18 };
        this->m_gw_cfg.ruuvi_cfg.http_stat.use_http_stat              = false;
        this->m_gw_cfg.ruuvi_cfg.ntp.ntp_use                          = true;
        this->m_gw_cfg.ruuvi_cfg.http.use_http_ruuvi                  = true;
        this->m_gw_cfg.ruuvi_cfg.http.use_http                        = false;
        this->m_gw_cfg.ruuvi_cfg.mqtt.mqtt_sending_interval           = 0;
        this->m_gw_cfg.ruuvi_cfg.mqtt.use_mqtt                        = false;
        ASSERT_FALSE(adv_post_handle_sig(ADV_POST_SIG_CFG_MODE_DEACTIVATED, &adv_post_state));
        ASSERT_FALSE(adv_post_state.flag_stop);
        ASSERT_TRUE(adv_post_state.flag_use_timestamps);
        ASSERT_TRUE(this->m_adv_post_cfg_cache.flag_use_ntp);
        ASSERT_FALSE(adv_post_state.flag_need_to_send_advs1);
        ASSERT_FALSE(adv_post_state.flag_need_to_send_advs2);
        ASSERT_EQ(8, this->m_events_history.size());
        ASSERT_EQ(EVENT_HISTORY_RUUVI_SEND_NRF_SETTINGS_FROM_GW_CFG, this->m_events_history[0].event_type);
        ASSERT_EQ(EVENT_HISTORY_ADV_POST_CFG_CACHE_MUTEX_LOCK, this->m_events_history[1].event_type);
        ASSERT_EQ(EVENT_HISTORY_ADV1_POST_TIMER_RESTART_WITH_DEFAULT_PERIOD, this->m_events_history[2].event_type);
        ASSERT_EQ(EVENT_HISTORY_STOP_TIMER_SIG_RETRANSMIT_TO_HTTP_CUSTOM, this->m_events_history[3].event_type);
        ASSERT_EQ(EVENT_HISTORY_STOP_TIMER_SIG_MQTT, this->m_events_history[4].event_type);
        ASSERT_EQ(EVENT_HISTORY_STOP_TIMER_SIG_SEND_STATISTICS, this->m_events_history[5].event_type);
        ASSERT_EQ(EVENT_HISTORY_ADV_POST_CFG_CACHE_MUTEX_UNLOCK, this->m_events_history[6].event_type);
        ASSERT_EQ(EVENT_HISTORY_ADV_TABLE_CLEAR, this->m_events_history[7].event_type);
        ASSERT_EQ(1, this->m_adv_post_cfg_cache.scan_filter_length);
        ASSERT_EQ(true, this->m_adv_post_cfg_cache.scan_filter_allow_listed);
        ASSERT_EQ(
            0,
            memcmp(
                this->m_adv_post_cfg_cache.p_arr_of_scan_filter_mac,
                this->m_gw_cfg.ruuvi_cfg.scan_filter.scan_filter_list,
                sizeof(mac_address_bin_t) * this->m_adv_post_cfg_cache.scan_filter_length));
        this->m_events_history.clear();
    }
    {
        this->m_gw_cfg.ruuvi_cfg.scan_filter.scan_filter_allow_listed = true;
        this->m_gw_cfg.ruuvi_cfg.scan_filter.scan_filter_length       = 0;
        this->m_gw_cfg.ruuvi_cfg.http_stat.use_http_stat              = false;
        this->m_gw_cfg.ruuvi_cfg.ntp.ntp_use                          = true;
        this->m_gw_cfg.ruuvi_cfg.http.use_http_ruuvi                  = true;
        this->m_gw_cfg.ruuvi_cfg.http.use_http                        = false;
        this->m_gw_cfg.ruuvi_cfg.mqtt.mqtt_sending_interval           = 0;
        this->m_gw_cfg.ruuvi_cfg.mqtt.use_mqtt                        = false;
        ASSERT_FALSE(adv_post_handle_sig(ADV_POST_SIG_CFG_MODE_DEACTIVATED, &adv_post_state));
        ASSERT_FALSE(adv_post_state.flag_stop);
        ASSERT_TRUE(adv_post_state.flag_use_timestamps);
        ASSERT_TRUE(this->m_adv_post_cfg_cache.flag_use_ntp);
        ASSERT_FALSE(adv_post_state.flag_need_to_send_advs1);
        ASSERT_FALSE(adv_post_state.flag_need_to_send_advs2);
        ASSERT_EQ(8, this->m_events_history.size());
        ASSERT_EQ(EVENT_HISTORY_RUUVI_SEND_NRF_SETTINGS_FROM_GW_CFG, this->m_events_history[0].event_type);
        ASSERT_EQ(EVENT_HISTORY_ADV_POST_CFG_CACHE_MUTEX_LOCK, this->m_events_history[1].event_type);
        ASSERT_EQ(EVENT_HISTORY_ADV1_POST_TIMER_RESTART_WITH_DEFAULT_PERIOD, this->m_events_history[2].event_type);
        ASSERT_EQ(EVENT_HISTORY_STOP_TIMER_SIG_RETRANSMIT_TO_HTTP_CUSTOM, this->m_events_history[3].event_type);
        ASSERT_EQ(EVENT_HISTORY_STOP_TIMER_SIG_MQTT, this->m_events_history[4].event_type);
        ASSERT_EQ(EVENT_HISTORY_STOP_TIMER_SIG_SEND_STATISTICS, this->m_events_history[5].event_type);
        ASSERT_EQ(EVENT_HISTORY_ADV_POST_CFG_CACHE_MUTEX_UNLOCK, this->m_events_history[6].event_type);
        ASSERT_EQ(EVENT_HISTORY_ADV_TABLE_CLEAR, this->m_events_history[7].event_type);
        ASSERT_EQ(0, this->m_adv_post_cfg_cache.scan_filter_length);
        ASSERT_EQ(true, this->m_adv_post_cfg_cache.scan_filter_allow_listed);
        ASSERT_EQ(nullptr, this->m_adv_post_cfg_cache.p_arr_of_scan_filter_mac);
        this->m_events_history.clear();
    }
    ASSERT_TRUE(this->m_mem_alloc_trace.is_empty());
    {
        this->m_malloc_cnt                                            = 0;
        this->m_malloc_fail_on_cnt                                    = 1;
        this->m_gw_cfg.ruuvi_cfg.scan_filter.scan_filter_allow_listed = true;
        this->m_gw_cfg.ruuvi_cfg.scan_filter.scan_filter_length       = 1;
        this->m_gw_cfg.ruuvi_cfg.scan_filter.scan_filter_list[0]      = { 0x11, 0x12, 0x13, 0x14, 0x15, 0x18 };
        this->m_gw_cfg.ruuvi_cfg.http_stat.use_http_stat              = false;
        this->m_gw_cfg.ruuvi_cfg.ntp.ntp_use                          = true;
        this->m_gw_cfg.ruuvi_cfg.http.use_http_ruuvi                  = true;
        this->m_gw_cfg.ruuvi_cfg.http.use_http                        = false;
        this->m_gw_cfg.ruuvi_cfg.mqtt.mqtt_sending_interval           = 0;
        this->m_gw_cfg.ruuvi_cfg.mqtt.use_mqtt                        = false;
        ASSERT_FALSE(adv_post_handle_sig(ADV_POST_SIG_CFG_MODE_DEACTIVATED, &adv_post_state));
        ASSERT_FALSE(adv_post_state.flag_stop);
        ASSERT_TRUE(adv_post_state.flag_use_timestamps);
        ASSERT_TRUE(this->m_adv_post_cfg_cache.flag_use_ntp);
        ASSERT_FALSE(adv_post_state.flag_need_to_send_advs1);
        ASSERT_FALSE(adv_post_state.flag_need_to_send_advs2);
        ASSERT_EQ(3, this->m_events_history.size());
        ASSERT_EQ(EVENT_HISTORY_RUUVI_SEND_NRF_SETTINGS_FROM_GW_CFG, this->m_events_history[0].event_type);
        ASSERT_EQ(EVENT_HISTORY_ADV_POST_CFG_CACHE_MUTEX_LOCK, this->m_events_history[1].event_type);
        ASSERT_EQ(EVENT_HISTORY_GATEWAY_RESTART, this->m_events_history[2].event_type);
        ASSERT_EQ(0, this->m_adv_post_cfg_cache.scan_filter_length);
        ASSERT_EQ(true, this->m_adv_post_cfg_cache.scan_filter_allow_listed);
        ASSERT_EQ(nullptr, this->m_adv_post_cfg_cache.p_arr_of_scan_filter_mac);
        this->m_events_history.clear();
    }
    ASSERT_TRUE(this->m_mem_alloc_trace.is_empty());
}

TEST_F(TestAdvPostSignals, test_adv_post_handle_sig_green_led_turn_on) // NOLINT
{
    adv_post_signals_init();
    this->m_events_history.clear();

    adv_post_state_t adv_post_state = {
        .flag_primary_time_sync_is_done  = true,
        .flag_network_connected          = true,
        .flag_async_comm_in_progress     = false,
        .flag_need_to_send_advs1         = false,
        .flag_need_to_send_advs2         = false,
        .flag_need_to_send_statistics    = false,
        .flag_need_to_send_mqtt_periodic = false,
        .flag_relaying_enabled           = true,
        .flag_use_timestamps             = true,
        .flag_stop                       = false,
    };

    {
        ASSERT_FALSE(adv_post_handle_sig(ADV_POST_SIG_GREEN_LED_TURN_ON, &adv_post_state));
        ASSERT_FALSE(adv_post_state.flag_stop);
        ASSERT_EQ(1, this->m_events_history.size());
        ASSERT_EQ(EVENT_HISTORY_ADV_POST_ON_GREEN_LED_UPDATE, this->m_events_history[0].event_type);
        ASSERT_EQ(ADV_POST_GREEN_LED_CMD_ON, this->m_events_history[0].green_led_update.cmd);
        this->m_events_history.clear();
    }
    ASSERT_TRUE(this->m_mem_alloc_trace.is_empty());
}

TEST_F(TestAdvPostSignals, test_adv_post_handle_sig_green_led_turn_off) // NOLINT
{
    adv_post_signals_init();
    this->m_events_history.clear();

    adv_post_state_t adv_post_state = {
        .flag_primary_time_sync_is_done  = true,
        .flag_network_connected          = true,
        .flag_async_comm_in_progress     = false,
        .flag_need_to_send_advs1         = false,
        .flag_need_to_send_advs2         = false,
        .flag_need_to_send_statistics    = false,
        .flag_need_to_send_mqtt_periodic = false,
        .flag_relaying_enabled           = true,
        .flag_use_timestamps             = true,
        .flag_stop                       = false,
    };

    {
        ASSERT_FALSE(adv_post_handle_sig(ADV_POST_SIG_GREEN_LED_TURN_OFF, &adv_post_state));
        ASSERT_FALSE(adv_post_state.flag_stop);
        ASSERT_EQ(1, this->m_events_history.size());
        ASSERT_EQ(EVENT_HISTORY_ADV_POST_ON_GREEN_LED_UPDATE, this->m_events_history[0].event_type);
        ASSERT_EQ(ADV_POST_GREEN_LED_CMD_OFF, this->m_events_history[0].green_led_update.cmd);
        this->m_events_history.clear();
    }
    ASSERT_TRUE(this->m_mem_alloc_trace.is_empty());
}

TEST_F(TestAdvPostSignals, test_adv_post_handle_sig_green_led_update) // NOLINT
{
    adv_post_signals_init();
    this->m_events_history.clear();

    adv_post_state_t adv_post_state = {
        .flag_primary_time_sync_is_done  = true,
        .flag_network_connected          = true,
        .flag_async_comm_in_progress     = false,
        .flag_need_to_send_advs1         = false,
        .flag_need_to_send_advs2         = false,
        .flag_need_to_send_statistics    = false,
        .flag_need_to_send_mqtt_periodic = false,
        .flag_relaying_enabled           = true,
        .flag_use_timestamps             = true,
        .flag_stop                       = false,
    };

    {
        ASSERT_FALSE(adv_post_handle_sig(ADV_POST_SIG_GREEN_LED_UPDATE, &adv_post_state));
        ASSERT_FALSE(adv_post_state.flag_stop);
        ASSERT_EQ(1, this->m_events_history.size());
        ASSERT_EQ(EVENT_HISTORY_ADV_POST_ON_GREEN_LED_UPDATE, this->m_events_history[0].event_type);
        ASSERT_EQ(ADV_POST_GREEN_LED_CMD_UPDATE, this->m_events_history[0].green_led_update.cmd);
        this->m_events_history.clear();
    }
    ASSERT_TRUE(this->m_mem_alloc_trace.is_empty());
}

TEST_F(TestAdvPostSignals, test_adv_post_handle_sig_recv_adv_timeout) // NOLINT
{
    adv_post_signals_init();
    this->m_events_history.clear();

    adv_post_state_t adv_post_state = {
        .flag_primary_time_sync_is_done  = true,
        .flag_network_connected          = true,
        .flag_async_comm_in_progress     = false,
        .flag_need_to_send_advs1         = false,
        .flag_need_to_send_advs2         = false,
        .flag_need_to_send_statistics    = false,
        .flag_need_to_send_mqtt_periodic = false,
        .flag_relaying_enabled           = true,
        .flag_use_timestamps             = true,
        .flag_stop                       = false,
    };

    {
        ASSERT_FALSE(adv_post_handle_sig(ADV_POST_SIG_RECV_ADV_TIMEOUT, &adv_post_state));
        ASSERT_FALSE(adv_post_state.flag_stop);
        ASSERT_EQ(1, this->m_events_history.size());
        ASSERT_EQ(EVENT_HISTORY_EVENT_MGR_NOTIFY, this->m_events_history[0].event_type);
        ASSERT_EQ(EVENT_MGR_EV_RECV_ADV_TIMEOUT, this->m_events_history[0].event_mgr_notify.event);
        this->m_events_history.clear();
    }
    ASSERT_TRUE(this->m_mem_alloc_trace.is_empty());
}
