/**
 * @file test_adv_mqtt_signals.cpp
 * @author TheSomeMan
 * @date 2023-10-30
 * @copyright Ruuvi Innovations Ltd, license BSD-3-Clause.
 */

#include "adv_mqtt_signals.h"
#include "gtest/gtest.h"
#include <string>
#include "esp_err.h"
#include "adv_table.h"
#include "adv_mqtt_cfg_cache.h"
#include "event_mgr.h"
#include "gw_cfg_default.h"

using namespace std;

class TestAdvMqttSignals;

static TestAdvMqttSignals* g_pTestClass;

/*** Google-test class implementation
 * *********************************************************************************/

enum event_type_e
{
    EVENT_HISTORY_OS_SIGNAL_CREATE_STATIC,
    EVENT_HISTORY_OS_SIGNAL_ADD,
    EVENT_HISTORY_OS_SIGNAL_SEND,
    EVENT_HISTORY_OS_SIGNAL_UNREGISTER_CUR_THREAD,
    EVENT_HISTORY_OS_SIGNAL_DELETE,
    EVENT_HISTORY_ESP_TASK_WDT_RESET,
    EVENT_HISTORY_ADV_MQTT_CFG_CACHE_MUTEX_LOCK,
    EVENT_HISTORY_ADV_MQTT_CFG_CACHE_MUTEX_UNLOCK,
    EVENT_HISTORY_ADV_MQTT_TIMER_SIG_RETRY_SENDING_ADVS,
    EVENT_HISTORY_MQTT_PUBLISH_CONNECT,
    EVENT_HISTORY_MQTT_PUBLISH_ADV,
    EVENT_HISTORY_EVENT_MGR_NOTIFY,
    EVENT_HISTORY_NETWORK_TIMEOUT_UPDATE_TIMESTAMP,
    EVENT_HISTORY_GW_CFG_LOCK,
    EVENT_HISTORY_GW_CFG_UNLOCK,
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

class TestAdvMqttSignals : public ::testing::Test
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
        this->m_gw_status_is_relaying_via_mqtt_enabled         = true;
        this->m_time_is_synchronized                           = true;
        this->m_metrics_received_advs                          = 0;
        this->adv_table_read_retransmission_list3_head_res     = true;
        this->adv_table_read_retransmission_list3_is_empty_res = true;
        this->m_adv_report                                     = {};
        this->mqtt_is_buffer_available_for_publish_res         = true;
        this->mqtt_publish_adv_res                             = true;
        this->m_network_timeout_check_res                      = false;
        this->m_adv_mqtt_cfg_cache                             = {};
        this->m_gw_cfg                                         = {};
    }

    void
    TearDown() override
    {
        gw_cfg_default_deinit();
        g_pTestClass = nullptr;
    }

public:
    TestAdvMqttSignals();

    ~TestAdvMqttSignals() override;

    MemAllocTrace m_mem_alloc_trace;
    uint32_t      m_malloc_cnt {};
    uint32_t      m_malloc_fail_on_cnt {};

    wifiman_config_t m_wifiman_default_config {};
    gw_cfg_t         m_gw_cfg {};

    bool m_gw_status_is_mqtt_connected { false };
    bool m_gw_status_is_relaying_via_mqtt_enabled { true };

    bool                      m_time_is_synchronized { true };
    uint64_t                  m_metrics_received_advs { 0 };
    bool                      adv_table_read_retransmission_list3_head_res { true };
    bool                      adv_table_read_retransmission_list3_is_empty_res { true };
    adv_report_t              m_adv_report {};
    bool                      mqtt_publish_adv_res { true };
    bool                      mqtt_is_buffer_available_for_publish_res { true };
    bool                      m_network_timeout_check_res { false };
    adv_mqtt_cfg_cache_t      m_adv_mqtt_cfg_cache {};
    std::vector<event_info_t> m_events_history {};
};

TestAdvMqttSignals::TestAdvMqttSignals()
    : Test()
{
}

TestAdvMqttSignals::~TestAdvMqttSignals() = default;

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
gw_cfg_log(const gw_cfg_t* const p_gw_cfg, const char* const p_title, const bool flag_log_device_info)
{
}

const gw_cfg_t*
gw_cfg_lock_ro(void)
{
    g_pTestClass->m_events_history.push_back({ .event_type = EVENT_HISTORY_GW_CFG_LOCK });
    return &g_pTestClass->m_gw_cfg;
}

void
gw_cfg_unlock_ro(const gw_cfg_t** const p_p_gw_cfg)
{
    g_pTestClass->m_events_history.push_back({ .event_type = EVENT_HISTORY_GW_CFG_UNLOCK });
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

bool
gw_status_is_mqtt_connected(void)
{
    return g_pTestClass->m_gw_status_is_mqtt_connected;
}

bool
gw_status_is_relaying_via_mqtt_enabled(void)
{
    return g_pTestClass->m_gw_status_is_relaying_via_mqtt_enabled;
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

void
adv_mqtt_timers_start_timer_sig_retry_sending_advs(void)
{
    g_pTestClass->m_events_history.push_back({ .event_type = EVENT_HISTORY_ADV_MQTT_TIMER_SIG_RETRY_SENDING_ADVS });
}

bool
mqtt_is_buffer_available_for_publish(void)
{
    return g_pTestClass->mqtt_is_buffer_available_for_publish_res;
}

void
mqtt_publish_connect(void)
{
    g_pTestClass->m_events_history.push_back({ .event_type = EVENT_HISTORY_MQTT_PUBLISH_CONNECT });
}

bool
mqtt_publish_adv(const adv_report_t* const p_adv, const bool flag_use_timestamps, const time_t timestamp)
{
    g_pTestClass->m_events_history.push_back({ .event_type = EVENT_HISTORY_MQTT_PUBLISH_ADV });
    return g_pTestClass->mqtt_publish_adv_res;
}

void
network_timeout_update_timestamp(void)
{
    g_pTestClass->m_events_history.push_back({ .event_type = EVENT_HISTORY_NETWORK_TIMEOUT_UPDATE_TIMESTAMP });
}

bool
network_timeout_check(void)
{
    return g_pTestClass->m_network_timeout_check_res;
}

esp_err_t
esp_task_wdt_reset(void)
{
    g_pTestClass->m_events_history.push_back({ .event_type = EVENT_HISTORY_ESP_TASK_WDT_RESET });
    return ESP_OK;
}

adv_mqtt_cfg_cache_t*
adv_mqtt_cfg_cache_mutex_lock(void)
{
    g_pTestClass->m_events_history.push_back({ .event_type = EVENT_HISTORY_ADV_MQTT_CFG_CACHE_MUTEX_LOCK });
    return &g_pTestClass->m_adv_mqtt_cfg_cache;
}

void
adv_mqtt_cfg_cache_mutex_unlock(adv_mqtt_cfg_cache_t** p_p_cfg_cache)
{
    g_pTestClass->m_events_history.push_back({ .event_type = EVENT_HISTORY_ADV_MQTT_CFG_CACHE_MUTEX_UNLOCK });
    *p_p_cfg_cache = nullptr;
}

void
event_mgr_notify(const event_mgr_ev_e event)
{
    g_pTestClass->m_events_history.push_back(
        { .event_type = EVENT_HISTORY_EVENT_MGR_NOTIFY, .event_mgr_notify = { .event = event } });
}

} // extern "C"

/*** Unit-Tests
 * *******************************************************************************************************/

TEST_F(TestAdvMqttSignals, test_adv_mqtt_signals_init_deinit) // NOLINT
{
    adv_mqtt_signals_init();

    adv_mqtt_signals_deinit();

    const std::vector<event_info_t> expected_history = {
        { .event_type = EVENT_HISTORY_OS_SIGNAL_CREATE_STATIC },
        { .event_type    = EVENT_HISTORY_OS_SIGNAL_ADD,
          .os_signal_add = { .sig_num = adv_mqtt_conv_to_sig_num(ADV_MQTT_SIG_STOP) } },
        { .event_type    = EVENT_HISTORY_OS_SIGNAL_ADD,
          .os_signal_add = { .sig_num = adv_mqtt_conv_to_sig_num(ADV_MQTT_SIG_ON_RECV_ADV) } },
        { .event_type    = EVENT_HISTORY_OS_SIGNAL_ADD,
          .os_signal_add = { .sig_num = adv_mqtt_conv_to_sig_num(ADV_MQTT_SIG_MQTT_CONNECTED) } },
        { .event_type    = EVENT_HISTORY_OS_SIGNAL_ADD,
          .os_signal_add = { .sig_num = adv_mqtt_conv_to_sig_num(ADV_MQTT_SIG_TASK_WATCHDOG_FEED) } },
        { .event_type    = EVENT_HISTORY_OS_SIGNAL_ADD,
          .os_signal_add = { .sig_num = adv_mqtt_conv_to_sig_num(ADV_MQTT_SIG_GW_CFG_READY) } },
        { .event_type    = EVENT_HISTORY_OS_SIGNAL_ADD,
          .os_signal_add = { .sig_num = adv_mqtt_conv_to_sig_num(ADV_MQTT_SIG_GW_CFG_CHANGED_RUUVI) } },
        { .event_type    = EVENT_HISTORY_OS_SIGNAL_ADD,
          .os_signal_add = { .sig_num = adv_mqtt_conv_to_sig_num(ADV_MQTT_SIG_RELAYING_MODE_CHANGED) } },
        { .event_type    = EVENT_HISTORY_OS_SIGNAL_ADD,
          .os_signal_add = { .sig_num = adv_mqtt_conv_to_sig_num(ADV_MQTT_SIG_CFG_MODE_ACTIVATED) } },
        { .event_type    = EVENT_HISTORY_OS_SIGNAL_ADD,
          .os_signal_add = { .sig_num = adv_mqtt_conv_to_sig_num(ADV_MQTT_SIG_CFG_MODE_DEACTIVATED) } },
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

TEST_F(TestAdvMqttSignals, test_adv_mqtt_signals_get) // NOLINT
{
    adv_mqtt_signals_init();

    ASSERT_NE(nullptr, adv_mqtt_signals_get());

    ASSERT_TRUE(this->m_mem_alloc_trace.is_empty());
}

TEST_F(TestAdvMqttSignals, test_adv_mqtt_conv_to_sig_num) // NOLINT
{
    ASSERT_EQ(OS_SIGNAL_NUM_0, adv_mqtt_conv_to_sig_num(ADV_MQTT_SIG_STOP));
    ASSERT_EQ(OS_SIGNAL_NUM_1, adv_mqtt_conv_to_sig_num(ADV_MQTT_SIG_ON_RECV_ADV));
    ASSERT_EQ(OS_SIGNAL_NUM_2, adv_mqtt_conv_to_sig_num(ADV_MQTT_SIG_MQTT_CONNECTED));
    ASSERT_EQ(OS_SIGNAL_NUM_3, adv_mqtt_conv_to_sig_num(ADV_MQTT_SIG_TASK_WATCHDOG_FEED));
    ASSERT_EQ(OS_SIGNAL_NUM_4, adv_mqtt_conv_to_sig_num(ADV_MQTT_SIG_GW_CFG_READY));
    ASSERT_EQ(OS_SIGNAL_NUM_5, adv_mqtt_conv_to_sig_num(ADV_MQTT_SIG_GW_CFG_CHANGED_RUUVI));
    ASSERT_EQ(OS_SIGNAL_NUM_6, adv_mqtt_conv_to_sig_num(ADV_MQTT_SIG_RELAYING_MODE_CHANGED));
    ASSERT_EQ(OS_SIGNAL_NUM_7, adv_mqtt_conv_to_sig_num(ADV_MQTT_SIG_CFG_MODE_ACTIVATED));
    ASSERT_EQ(OS_SIGNAL_NUM_8, adv_mqtt_conv_to_sig_num(ADV_MQTT_SIG_CFG_MODE_DEACTIVATED));

    ASSERT_TRUE(this->m_mem_alloc_trace.is_empty());
}

TEST_F(TestAdvMqttSignals, test_adv_mqtt_conv_from_sig_num) // NOLINT
{
    ASSERT_EQ(ADV_MQTT_SIG_STOP, adv_mqtt_conv_from_sig_num(OS_SIGNAL_NUM_0));
    ASSERT_EQ(ADV_MQTT_SIG_ON_RECV_ADV, adv_mqtt_conv_from_sig_num(OS_SIGNAL_NUM_1));
    ASSERT_EQ(ADV_MQTT_SIG_MQTT_CONNECTED, adv_mqtt_conv_from_sig_num(OS_SIGNAL_NUM_2));
    ASSERT_EQ(ADV_MQTT_SIG_TASK_WATCHDOG_FEED, adv_mqtt_conv_from_sig_num(OS_SIGNAL_NUM_3));
    ASSERT_EQ(ADV_MQTT_SIG_GW_CFG_READY, adv_mqtt_conv_from_sig_num(OS_SIGNAL_NUM_4));
    ASSERT_EQ(ADV_MQTT_SIG_GW_CFG_CHANGED_RUUVI, adv_mqtt_conv_from_sig_num(OS_SIGNAL_NUM_5));
    ASSERT_EQ(ADV_MQTT_SIG_RELAYING_MODE_CHANGED, adv_mqtt_conv_from_sig_num(OS_SIGNAL_NUM_6));
    ASSERT_EQ(ADV_MQTT_SIG_CFG_MODE_ACTIVATED, adv_mqtt_conv_from_sig_num(OS_SIGNAL_NUM_7));
    ASSERT_EQ(ADV_MQTT_SIG_CFG_MODE_DEACTIVATED, adv_mqtt_conv_from_sig_num(OS_SIGNAL_NUM_8));

    ASSERT_TRUE(this->m_mem_alloc_trace.is_empty());
}

TEST_F(TestAdvMqttSignals, test_adv_mqtt_signals_send_sig) // NOLINT
{
    adv_mqtt_signals_init();
    this->m_events_history.clear();

    adv_mqtt_signals_send_sig(ADV_MQTT_SIG_ON_RECV_ADV);

    const std::vector<event_info_t> expected_history = {
        { .event_type     = EVENT_HISTORY_OS_SIGNAL_SEND,
          .os_signal_send = { .sig_num = adv_mqtt_conv_to_sig_num(ADV_MQTT_SIG_ON_RECV_ADV) } },
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

TEST_F(TestAdvMqttSignals, test_adv_mqtt_signals_is_thread_registered) // NOLINT
{
    adv_mqtt_signals_init();

    ASSERT_TRUE(adv_mqtt_signals_is_thread_registered());

    ASSERT_TRUE(this->m_mem_alloc_trace.is_empty());
}

TEST_F(TestAdvMqttSignals, test_adv_mqtt_handle_sig_stop) // NOLINT
{
    adv_mqtt_signals_init();
    this->m_events_history.clear();

    adv_mqtt_state_t adv_mqtt_state = {
        .flag_stop = false,
    };
    ASSERT_TRUE(adv_mqtt_handle_sig(ADV_MQTT_SIG_STOP, &adv_mqtt_state));
    ASSERT_TRUE(adv_mqtt_state.flag_stop);

    ASSERT_TRUE(this->m_mem_alloc_trace.is_empty());
}

TEST_F(TestAdvMqttSignals, test_adv_mqtt_handle_sig_gw_cfg_ready) // NOLINT
{
    adv_mqtt_signals_init();
    this->m_events_history.clear();

    adv_mqtt_state_t adv_mqtt_state = {
        .flag_stop = false,
    };

    {
        this->m_gw_cfg.ruuvi_cfg.ntp.ntp_use                = true;
        this->m_gw_cfg.ruuvi_cfg.mqtt.use_mqtt              = false;
        this->m_gw_cfg.ruuvi_cfg.mqtt.mqtt_sending_interval = 10;

        this->m_adv_mqtt_cfg_cache.flag_use_ntp                  = false;
        this->m_adv_mqtt_cfg_cache.flag_mqtt_instant_mode_active = true;

        ASSERT_FALSE(adv_mqtt_handle_sig(ADV_MQTT_SIG_GW_CFG_READY, &adv_mqtt_state));
        ASSERT_FALSE(adv_mqtt_state.flag_stop);
        ASSERT_EQ(6, this->m_events_history.size());
        ASSERT_EQ(EVENT_HISTORY_ADV_MQTT_CFG_CACHE_MUTEX_LOCK, this->m_events_history[0].event_type);
        ASSERT_EQ(EVENT_HISTORY_GW_CFG_LOCK, this->m_events_history[1].event_type);
        ASSERT_EQ(EVENT_HISTORY_GW_CFG_UNLOCK, this->m_events_history[2].event_type);
        ASSERT_EQ(EVENT_HISTORY_GW_CFG_LOCK, this->m_events_history[3].event_type);
        ASSERT_EQ(EVENT_HISTORY_GW_CFG_UNLOCK, this->m_events_history[4].event_type);
        ASSERT_EQ(EVENT_HISTORY_ADV_MQTT_CFG_CACHE_MUTEX_UNLOCK, this->m_events_history[5].event_type);
        this->m_events_history.clear();

        ASSERT_TRUE(this->m_adv_mqtt_cfg_cache.flag_use_ntp);
        ASSERT_FALSE(this->m_adv_mqtt_cfg_cache.flag_mqtt_instant_mode_active);
    }
    {
        this->m_gw_cfg.ruuvi_cfg.ntp.ntp_use                = true;
        this->m_gw_cfg.ruuvi_cfg.mqtt.use_mqtt              = true;
        this->m_gw_cfg.ruuvi_cfg.mqtt.mqtt_sending_interval = 10;

        this->m_adv_mqtt_cfg_cache.flag_use_ntp                  = false;
        this->m_adv_mqtt_cfg_cache.flag_mqtt_instant_mode_active = true;

        ASSERT_FALSE(adv_mqtt_handle_sig(ADV_MQTT_SIG_GW_CFG_READY, &adv_mqtt_state));
        ASSERT_FALSE(adv_mqtt_state.flag_stop);
        ASSERT_EQ(8, this->m_events_history.size());
        ASSERT_EQ(EVENT_HISTORY_ADV_MQTT_CFG_CACHE_MUTEX_LOCK, this->m_events_history[0].event_type);
        ASSERT_EQ(EVENT_HISTORY_GW_CFG_LOCK, this->m_events_history[1].event_type);
        ASSERT_EQ(EVENT_HISTORY_GW_CFG_UNLOCK, this->m_events_history[2].event_type);
        ASSERT_EQ(EVENT_HISTORY_GW_CFG_LOCK, this->m_events_history[3].event_type);
        ASSERT_EQ(EVENT_HISTORY_GW_CFG_UNLOCK, this->m_events_history[4].event_type);
        ASSERT_EQ(EVENT_HISTORY_GW_CFG_LOCK, this->m_events_history[5].event_type);
        ASSERT_EQ(EVENT_HISTORY_GW_CFG_UNLOCK, this->m_events_history[6].event_type);
        ASSERT_EQ(EVENT_HISTORY_ADV_MQTT_CFG_CACHE_MUTEX_UNLOCK, this->m_events_history[7].event_type);
        this->m_events_history.clear();

        ASSERT_TRUE(this->m_adv_mqtt_cfg_cache.flag_use_ntp);
        ASSERT_FALSE(this->m_adv_mqtt_cfg_cache.flag_mqtt_instant_mode_active);
    }
    {
        this->m_gw_cfg.ruuvi_cfg.ntp.ntp_use                = false;
        this->m_gw_cfg.ruuvi_cfg.mqtt.use_mqtt              = false;
        this->m_gw_cfg.ruuvi_cfg.mqtt.mqtt_sending_interval = 0;

        this->m_gw_status_is_mqtt_connected = false;

        this->m_adv_mqtt_cfg_cache.flag_use_ntp                  = true;
        this->m_adv_mqtt_cfg_cache.flag_mqtt_instant_mode_active = true;

        ASSERT_FALSE(adv_mqtt_handle_sig(ADV_MQTT_SIG_GW_CFG_READY, &adv_mqtt_state));
        ASSERT_FALSE(adv_mqtt_state.flag_stop);
        ASSERT_EQ(6, this->m_events_history.size());
        ASSERT_EQ(EVENT_HISTORY_ADV_MQTT_CFG_CACHE_MUTEX_LOCK, this->m_events_history[0].event_type);
        ASSERT_EQ(EVENT_HISTORY_GW_CFG_LOCK, this->m_events_history[1].event_type);
        ASSERT_EQ(EVENT_HISTORY_GW_CFG_UNLOCK, this->m_events_history[2].event_type);
        ASSERT_EQ(EVENT_HISTORY_GW_CFG_LOCK, this->m_events_history[3].event_type);
        ASSERT_EQ(EVENT_HISTORY_GW_CFG_UNLOCK, this->m_events_history[4].event_type);
        ASSERT_EQ(EVENT_HISTORY_ADV_MQTT_CFG_CACHE_MUTEX_UNLOCK, this->m_events_history[5].event_type);
        this->m_events_history.clear();

        ASSERT_FALSE(this->m_adv_mqtt_cfg_cache.flag_use_ntp);
        ASSERT_FALSE(this->m_adv_mqtt_cfg_cache.flag_mqtt_instant_mode_active);
    }
    {
        this->m_gw_cfg.ruuvi_cfg.ntp.ntp_use                = true;
        this->m_gw_cfg.ruuvi_cfg.mqtt.use_mqtt              = true;
        this->m_gw_cfg.ruuvi_cfg.mqtt.mqtt_sending_interval = 0;

        this->m_gw_status_is_mqtt_connected = false;

        this->m_adv_mqtt_cfg_cache.flag_use_ntp                  = false;
        this->m_adv_mqtt_cfg_cache.flag_mqtt_instant_mode_active = false;

        ASSERT_FALSE(adv_mqtt_handle_sig(ADV_MQTT_SIG_GW_CFG_READY, &adv_mqtt_state));
        ASSERT_FALSE(adv_mqtt_state.flag_stop);
        ASSERT_EQ(8, this->m_events_history.size());
        ASSERT_EQ(EVENT_HISTORY_ADV_MQTT_CFG_CACHE_MUTEX_LOCK, this->m_events_history[0].event_type);
        ASSERT_EQ(EVENT_HISTORY_GW_CFG_LOCK, this->m_events_history[1].event_type);
        ASSERT_EQ(EVENT_HISTORY_GW_CFG_UNLOCK, this->m_events_history[2].event_type);
        ASSERT_EQ(EVENT_HISTORY_GW_CFG_LOCK, this->m_events_history[3].event_type);
        ASSERT_EQ(EVENT_HISTORY_GW_CFG_UNLOCK, this->m_events_history[4].event_type);
        ASSERT_EQ(EVENT_HISTORY_GW_CFG_LOCK, this->m_events_history[5].event_type);
        ASSERT_EQ(EVENT_HISTORY_GW_CFG_UNLOCK, this->m_events_history[6].event_type);
        ASSERT_EQ(EVENT_HISTORY_ADV_MQTT_CFG_CACHE_MUTEX_UNLOCK, this->m_events_history[7].event_type);
        this->m_events_history.clear();

        ASSERT_TRUE(this->m_adv_mqtt_cfg_cache.flag_use_ntp);
        ASSERT_TRUE(this->m_adv_mqtt_cfg_cache.flag_mqtt_instant_mode_active);
    }
}

TEST_F(TestAdvMqttSignals, test_adv_mqtt_handle_sig_gw_cfg_changed_ruuvi) // NOLINT
{
    adv_mqtt_signals_init();
    this->m_events_history.clear();

    adv_mqtt_state_t adv_mqtt_state = {
        .flag_stop = false,
    };

    {
        this->m_gw_cfg.ruuvi_cfg.ntp.ntp_use                = true;
        this->m_gw_cfg.ruuvi_cfg.mqtt.use_mqtt              = false;
        this->m_gw_cfg.ruuvi_cfg.mqtt.mqtt_sending_interval = 10;

        this->m_adv_mqtt_cfg_cache.flag_use_ntp                  = false;
        this->m_adv_mqtt_cfg_cache.flag_mqtt_instant_mode_active = true;

        ASSERT_FALSE(adv_mqtt_handle_sig(ADV_MQTT_SIG_GW_CFG_CHANGED_RUUVI, &adv_mqtt_state));
        ASSERT_FALSE(adv_mqtt_state.flag_stop);
        ASSERT_EQ(6, this->m_events_history.size());
        ASSERT_EQ(EVENT_HISTORY_ADV_MQTT_CFG_CACHE_MUTEX_LOCK, this->m_events_history[0].event_type);
        ASSERT_EQ(EVENT_HISTORY_GW_CFG_LOCK, this->m_events_history[1].event_type);
        ASSERT_EQ(EVENT_HISTORY_GW_CFG_UNLOCK, this->m_events_history[2].event_type);
        ASSERT_EQ(EVENT_HISTORY_GW_CFG_LOCK, this->m_events_history[3].event_type);
        ASSERT_EQ(EVENT_HISTORY_GW_CFG_UNLOCK, this->m_events_history[4].event_type);
        ASSERT_EQ(EVENT_HISTORY_ADV_MQTT_CFG_CACHE_MUTEX_UNLOCK, this->m_events_history[5].event_type);
        this->m_events_history.clear();

        ASSERT_TRUE(this->m_adv_mqtt_cfg_cache.flag_use_ntp);
        ASSERT_FALSE(this->m_adv_mqtt_cfg_cache.flag_mqtt_instant_mode_active);
    }
    {
        this->m_gw_cfg.ruuvi_cfg.ntp.ntp_use                = true;
        this->m_gw_cfg.ruuvi_cfg.mqtt.use_mqtt              = true;
        this->m_gw_cfg.ruuvi_cfg.mqtt.mqtt_sending_interval = 10;

        this->m_adv_mqtt_cfg_cache.flag_use_ntp                  = false;
        this->m_adv_mqtt_cfg_cache.flag_mqtt_instant_mode_active = true;

        ASSERT_FALSE(adv_mqtt_handle_sig(ADV_MQTT_SIG_GW_CFG_CHANGED_RUUVI, &adv_mqtt_state));
        ASSERT_FALSE(adv_mqtt_state.flag_stop);
        ASSERT_EQ(8, this->m_events_history.size());
        ASSERT_EQ(EVENT_HISTORY_ADV_MQTT_CFG_CACHE_MUTEX_LOCK, this->m_events_history[0].event_type);
        ASSERT_EQ(EVENT_HISTORY_GW_CFG_LOCK, this->m_events_history[1].event_type);
        ASSERT_EQ(EVENT_HISTORY_GW_CFG_UNLOCK, this->m_events_history[2].event_type);
        ASSERT_EQ(EVENT_HISTORY_GW_CFG_LOCK, this->m_events_history[3].event_type);
        ASSERT_EQ(EVENT_HISTORY_GW_CFG_UNLOCK, this->m_events_history[4].event_type);
        ASSERT_EQ(EVENT_HISTORY_GW_CFG_LOCK, this->m_events_history[5].event_type);
        ASSERT_EQ(EVENT_HISTORY_GW_CFG_UNLOCK, this->m_events_history[6].event_type);
        ASSERT_EQ(EVENT_HISTORY_ADV_MQTT_CFG_CACHE_MUTEX_UNLOCK, this->m_events_history[7].event_type);
        this->m_events_history.clear();

        ASSERT_TRUE(this->m_adv_mqtt_cfg_cache.flag_use_ntp);
        ASSERT_FALSE(this->m_adv_mqtt_cfg_cache.flag_mqtt_instant_mode_active);
    }
    {
        this->m_gw_cfg.ruuvi_cfg.ntp.ntp_use                = false;
        this->m_gw_cfg.ruuvi_cfg.mqtt.use_mqtt              = false;
        this->m_gw_cfg.ruuvi_cfg.mqtt.mqtt_sending_interval = 0;

        this->m_gw_status_is_mqtt_connected = false;

        this->m_adv_mqtt_cfg_cache.flag_use_ntp                  = true;
        this->m_adv_mqtt_cfg_cache.flag_mqtt_instant_mode_active = true;

        ASSERT_FALSE(adv_mqtt_handle_sig(ADV_MQTT_SIG_GW_CFG_CHANGED_RUUVI, &adv_mqtt_state));
        ASSERT_FALSE(adv_mqtt_state.flag_stop);
        ASSERT_EQ(6, this->m_events_history.size());
        ASSERT_EQ(EVENT_HISTORY_ADV_MQTT_CFG_CACHE_MUTEX_LOCK, this->m_events_history[0].event_type);
        ASSERT_EQ(EVENT_HISTORY_GW_CFG_LOCK, this->m_events_history[1].event_type);
        ASSERT_EQ(EVENT_HISTORY_GW_CFG_UNLOCK, this->m_events_history[2].event_type);
        ASSERT_EQ(EVENT_HISTORY_GW_CFG_LOCK, this->m_events_history[3].event_type);
        ASSERT_EQ(EVENT_HISTORY_GW_CFG_UNLOCK, this->m_events_history[4].event_type);
        ASSERT_EQ(EVENT_HISTORY_ADV_MQTT_CFG_CACHE_MUTEX_UNLOCK, this->m_events_history[5].event_type);
        this->m_events_history.clear();

        ASSERT_FALSE(this->m_adv_mqtt_cfg_cache.flag_use_ntp);
        ASSERT_FALSE(this->m_adv_mqtt_cfg_cache.flag_mqtt_instant_mode_active);
    }
    {
        this->m_gw_cfg.ruuvi_cfg.ntp.ntp_use                = true;
        this->m_gw_cfg.ruuvi_cfg.mqtt.use_mqtt              = true;
        this->m_gw_cfg.ruuvi_cfg.mqtt.mqtt_sending_interval = 0;

        this->m_gw_status_is_mqtt_connected = false;

        this->m_adv_mqtt_cfg_cache.flag_use_ntp                  = false;
        this->m_adv_mqtt_cfg_cache.flag_mqtt_instant_mode_active = false;

        ASSERT_FALSE(adv_mqtt_handle_sig(ADV_MQTT_SIG_GW_CFG_CHANGED_RUUVI, &adv_mqtt_state));
        ASSERT_FALSE(adv_mqtt_state.flag_stop);
        ASSERT_EQ(8, this->m_events_history.size());
        ASSERT_EQ(EVENT_HISTORY_ADV_MQTT_CFG_CACHE_MUTEX_LOCK, this->m_events_history[0].event_type);
        ASSERT_EQ(EVENT_HISTORY_GW_CFG_LOCK, this->m_events_history[1].event_type);
        ASSERT_EQ(EVENT_HISTORY_GW_CFG_UNLOCK, this->m_events_history[2].event_type);
        ASSERT_EQ(EVENT_HISTORY_GW_CFG_LOCK, this->m_events_history[3].event_type);
        ASSERT_EQ(EVENT_HISTORY_GW_CFG_UNLOCK, this->m_events_history[4].event_type);
        ASSERT_EQ(EVENT_HISTORY_GW_CFG_LOCK, this->m_events_history[5].event_type);
        ASSERT_EQ(EVENT_HISTORY_GW_CFG_UNLOCK, this->m_events_history[6].event_type);
        ASSERT_EQ(EVENT_HISTORY_ADV_MQTT_CFG_CACHE_MUTEX_UNLOCK, this->m_events_history[7].event_type);
        this->m_events_history.clear();

        ASSERT_TRUE(this->m_adv_mqtt_cfg_cache.flag_use_ntp);
        ASSERT_TRUE(this->m_adv_mqtt_cfg_cache.flag_mqtt_instant_mode_active);
    }
}

TEST_F(TestAdvMqttSignals, test_adv_mqtt_handle_sig_cfg_mode_activated_deactivated) // NOLINT
{
    adv_mqtt_signals_init();
    this->m_events_history.clear();

    adv_mqtt_state_t adv_mqtt_state = {
        .flag_stop = false,
    };

    {
        this->m_gw_cfg.ruuvi_cfg.ntp.ntp_use                = true;
        this->m_gw_cfg.ruuvi_cfg.mqtt.use_mqtt              = false;
        this->m_gw_cfg.ruuvi_cfg.mqtt.mqtt_sending_interval = 10;

        this->m_adv_mqtt_cfg_cache.flag_use_ntp                  = false;
        this->m_adv_mqtt_cfg_cache.flag_mqtt_instant_mode_active = true;

        ASSERT_FALSE(adv_mqtt_handle_sig(ADV_MQTT_SIG_CFG_MODE_ACTIVATED, &adv_mqtt_state));
        ASSERT_FALSE(adv_mqtt_handle_sig(ADV_MQTT_SIG_CFG_MODE_DEACTIVATED, &adv_mqtt_state));
        ASSERT_FALSE(adv_mqtt_state.flag_stop);
        ASSERT_EQ(6, this->m_events_history.size());
        ASSERT_EQ(EVENT_HISTORY_ADV_MQTT_CFG_CACHE_MUTEX_LOCK, this->m_events_history[0].event_type);
        ASSERT_EQ(EVENT_HISTORY_GW_CFG_LOCK, this->m_events_history[1].event_type);
        ASSERT_EQ(EVENT_HISTORY_GW_CFG_UNLOCK, this->m_events_history[2].event_type);
        ASSERT_EQ(EVENT_HISTORY_GW_CFG_LOCK, this->m_events_history[3].event_type);
        ASSERT_EQ(EVENT_HISTORY_GW_CFG_UNLOCK, this->m_events_history[4].event_type);
        ASSERT_EQ(EVENT_HISTORY_ADV_MQTT_CFG_CACHE_MUTEX_UNLOCK, this->m_events_history[5].event_type);
        this->m_events_history.clear();

        ASSERT_TRUE(this->m_adv_mqtt_cfg_cache.flag_use_ntp);
        ASSERT_FALSE(this->m_adv_mqtt_cfg_cache.flag_mqtt_instant_mode_active);
    }
    {
        this->m_gw_cfg.ruuvi_cfg.ntp.ntp_use                = true;
        this->m_gw_cfg.ruuvi_cfg.mqtt.use_mqtt              = true;
        this->m_gw_cfg.ruuvi_cfg.mqtt.mqtt_sending_interval = 10;

        this->m_adv_mqtt_cfg_cache.flag_use_ntp                  = false;
        this->m_adv_mqtt_cfg_cache.flag_mqtt_instant_mode_active = true;

        ASSERT_FALSE(adv_mqtt_handle_sig(ADV_MQTT_SIG_CFG_MODE_ACTIVATED, &adv_mqtt_state));
        ASSERT_FALSE(adv_mqtt_handle_sig(ADV_MQTT_SIG_CFG_MODE_DEACTIVATED, &adv_mqtt_state));
        ASSERT_FALSE(adv_mqtt_state.flag_stop);
        ASSERT_EQ(8, this->m_events_history.size());
        ASSERT_EQ(EVENT_HISTORY_ADV_MQTT_CFG_CACHE_MUTEX_LOCK, this->m_events_history[0].event_type);
        ASSERT_EQ(EVENT_HISTORY_GW_CFG_LOCK, this->m_events_history[1].event_type);
        ASSERT_EQ(EVENT_HISTORY_GW_CFG_UNLOCK, this->m_events_history[2].event_type);
        ASSERT_EQ(EVENT_HISTORY_GW_CFG_LOCK, this->m_events_history[3].event_type);
        ASSERT_EQ(EVENT_HISTORY_GW_CFG_UNLOCK, this->m_events_history[4].event_type);
        ASSERT_EQ(EVENT_HISTORY_GW_CFG_LOCK, this->m_events_history[5].event_type);
        ASSERT_EQ(EVENT_HISTORY_GW_CFG_UNLOCK, this->m_events_history[6].event_type);
        ASSERT_EQ(EVENT_HISTORY_ADV_MQTT_CFG_CACHE_MUTEX_UNLOCK, this->m_events_history[7].event_type);
        this->m_events_history.clear();

        ASSERT_TRUE(this->m_adv_mqtt_cfg_cache.flag_use_ntp);
        ASSERT_FALSE(this->m_adv_mqtt_cfg_cache.flag_mqtt_instant_mode_active);
    }
    {
        this->m_gw_cfg.ruuvi_cfg.ntp.ntp_use                = false;
        this->m_gw_cfg.ruuvi_cfg.mqtt.use_mqtt              = false;
        this->m_gw_cfg.ruuvi_cfg.mqtt.mqtt_sending_interval = 0;

        this->m_gw_status_is_mqtt_connected = false;

        this->m_adv_mqtt_cfg_cache.flag_use_ntp                  = true;
        this->m_adv_mqtt_cfg_cache.flag_mqtt_instant_mode_active = true;

        ASSERT_FALSE(adv_mqtt_handle_sig(ADV_MQTT_SIG_CFG_MODE_ACTIVATED, &adv_mqtt_state));
        ASSERT_FALSE(adv_mqtt_handle_sig(ADV_MQTT_SIG_CFG_MODE_DEACTIVATED, &adv_mqtt_state));
        ASSERT_FALSE(adv_mqtt_state.flag_stop);
        ASSERT_EQ(6, this->m_events_history.size());
        ASSERT_EQ(EVENT_HISTORY_ADV_MQTT_CFG_CACHE_MUTEX_LOCK, this->m_events_history[0].event_type);
        ASSERT_EQ(EVENT_HISTORY_GW_CFG_LOCK, this->m_events_history[1].event_type);
        ASSERT_EQ(EVENT_HISTORY_GW_CFG_UNLOCK, this->m_events_history[2].event_type);
        ASSERT_EQ(EVENT_HISTORY_GW_CFG_LOCK, this->m_events_history[3].event_type);
        ASSERT_EQ(EVENT_HISTORY_GW_CFG_UNLOCK, this->m_events_history[4].event_type);
        ASSERT_EQ(EVENT_HISTORY_ADV_MQTT_CFG_CACHE_MUTEX_UNLOCK, this->m_events_history[5].event_type);
        this->m_events_history.clear();

        ASSERT_FALSE(this->m_adv_mqtt_cfg_cache.flag_use_ntp);
        ASSERT_FALSE(this->m_adv_mqtt_cfg_cache.flag_mqtt_instant_mode_active);
    }
    {
        this->m_gw_cfg.ruuvi_cfg.ntp.ntp_use                = true;
        this->m_gw_cfg.ruuvi_cfg.mqtt.use_mqtt              = true;
        this->m_gw_cfg.ruuvi_cfg.mqtt.mqtt_sending_interval = 0;

        this->m_gw_status_is_mqtt_connected = false;

        this->m_adv_mqtt_cfg_cache.flag_use_ntp                  = false;
        this->m_adv_mqtt_cfg_cache.flag_mqtt_instant_mode_active = false;

        ASSERT_FALSE(adv_mqtt_handle_sig(ADV_MQTT_SIG_CFG_MODE_ACTIVATED, &adv_mqtt_state));
        ASSERT_FALSE(adv_mqtt_handle_sig(ADV_MQTT_SIG_CFG_MODE_DEACTIVATED, &adv_mqtt_state));
        ASSERT_FALSE(adv_mqtt_state.flag_stop);
        ASSERT_EQ(8, this->m_events_history.size());
        ASSERT_EQ(EVENT_HISTORY_ADV_MQTT_CFG_CACHE_MUTEX_LOCK, this->m_events_history[0].event_type);
        ASSERT_EQ(EVENT_HISTORY_GW_CFG_LOCK, this->m_events_history[1].event_type);
        ASSERT_EQ(EVENT_HISTORY_GW_CFG_UNLOCK, this->m_events_history[2].event_type);
        ASSERT_EQ(EVENT_HISTORY_GW_CFG_LOCK, this->m_events_history[3].event_type);
        ASSERT_EQ(EVENT_HISTORY_GW_CFG_UNLOCK, this->m_events_history[4].event_type);
        ASSERT_EQ(EVENT_HISTORY_GW_CFG_LOCK, this->m_events_history[5].event_type);
        ASSERT_EQ(EVENT_HISTORY_GW_CFG_UNLOCK, this->m_events_history[6].event_type);
        ASSERT_EQ(EVENT_HISTORY_ADV_MQTT_CFG_CACHE_MUTEX_UNLOCK, this->m_events_history[7].event_type);
        this->m_events_history.clear();

        ASSERT_TRUE(this->m_adv_mqtt_cfg_cache.flag_use_ntp);
        ASSERT_TRUE(this->m_adv_mqtt_cfg_cache.flag_mqtt_instant_mode_active);
    }
}

TEST_F(TestAdvMqttSignals, test_adv_mqtt_handle_sig_on_recv_adv) // NOLINT
{
    adv_mqtt_signals_init();
    this->m_events_history.clear();

    adv_mqtt_state_t adv_mqtt_state = {
        .flag_stop = false,
    };

    this->m_gw_status_is_mqtt_connected            = false;
    this->m_gw_status_is_relaying_via_mqtt_enabled = false;

    this->m_adv_mqtt_cfg_cache.flag_use_ntp                  = true;
    this->m_adv_mqtt_cfg_cache.flag_mqtt_instant_mode_active = true;

    this->adv_table_read_retransmission_list3_head_res     = true;
    this->adv_table_read_retransmission_list3_is_empty_res = false;
    this->m_time_is_synchronized                           = true;
    this->m_metrics_received_advs                          = 123;

    ASSERT_FALSE(adv_mqtt_handle_sig(ADV_MQTT_SIG_ON_RECV_ADV, &adv_mqtt_state));
    ASSERT_FALSE(adv_mqtt_state.flag_stop);
    ASSERT_EQ(0, this->m_events_history.size());

    this->m_gw_status_is_mqtt_connected = true;

    ASSERT_FALSE(adv_mqtt_handle_sig(ADV_MQTT_SIG_ON_RECV_ADV, &adv_mqtt_state));
    ASSERT_FALSE(adv_mqtt_state.flag_stop);
    ASSERT_EQ(0, this->m_events_history.size());

    this->m_gw_status_is_mqtt_connected            = false;
    this->m_gw_status_is_relaying_via_mqtt_enabled = true;

    ASSERT_FALSE(adv_mqtt_handle_sig(ADV_MQTT_SIG_ON_RECV_ADV, &adv_mqtt_state));
    ASSERT_FALSE(adv_mqtt_state.flag_stop);
    ASSERT_EQ(0, this->m_events_history.size());

    this->m_gw_status_is_mqtt_connected = true;

    ASSERT_FALSE(adv_mqtt_handle_sig(ADV_MQTT_SIG_ON_RECV_ADV, &adv_mqtt_state));
    ASSERT_FALSE(adv_mqtt_state.flag_stop);
    ASSERT_EQ(5, this->m_events_history.size());
    ASSERT_EQ(EVENT_HISTORY_ADV_MQTT_CFG_CACHE_MUTEX_LOCK, this->m_events_history[0].event_type);
    ASSERT_EQ(EVENT_HISTORY_MQTT_PUBLISH_ADV, this->m_events_history[1].event_type);
    ASSERT_EQ(EVENT_HISTORY_NETWORK_TIMEOUT_UPDATE_TIMESTAMP, this->m_events_history[2].event_type);
    ASSERT_EQ(EVENT_HISTORY_OS_SIGNAL_SEND, this->m_events_history[3].event_type);
    ASSERT_EQ(ADV_MQTT_SIG_ON_RECV_ADV, this->m_events_history[3].os_signal_send.sig_num);
    ASSERT_EQ(EVENT_HISTORY_ADV_MQTT_CFG_CACHE_MUTEX_UNLOCK, this->m_events_history[4].event_type);
    this->m_events_history.clear();

    this->adv_table_read_retransmission_list3_is_empty_res = true;

    ASSERT_FALSE(adv_mqtt_handle_sig(ADV_MQTT_SIG_ON_RECV_ADV, &adv_mqtt_state));
    ASSERT_FALSE(adv_mqtt_state.flag_stop);
    ASSERT_EQ(4, this->m_events_history.size());
    ASSERT_EQ(EVENT_HISTORY_ADV_MQTT_CFG_CACHE_MUTEX_LOCK, this->m_events_history[0].event_type);
    ASSERT_EQ(EVENT_HISTORY_MQTT_PUBLISH_ADV, this->m_events_history[1].event_type);
    ASSERT_EQ(EVENT_HISTORY_NETWORK_TIMEOUT_UPDATE_TIMESTAMP, this->m_events_history[2].event_type);
    ASSERT_EQ(EVENT_HISTORY_ADV_MQTT_CFG_CACHE_MUTEX_UNLOCK, this->m_events_history[3].event_type);
    this->m_events_history.clear();

    this->m_time_is_synchronized = false;

    ASSERT_FALSE(adv_mqtt_handle_sig(ADV_MQTT_SIG_ON_RECV_ADV, &adv_mqtt_state));
    ASSERT_FALSE(adv_mqtt_state.flag_stop);
    ASSERT_EQ(4, this->m_events_history.size());
    ASSERT_EQ(EVENT_HISTORY_ADV_MQTT_CFG_CACHE_MUTEX_LOCK, this->m_events_history[0].event_type);
    ASSERT_EQ(EVENT_HISTORY_MQTT_PUBLISH_ADV, this->m_events_history[1].event_type);
    ASSERT_EQ(EVENT_HISTORY_NETWORK_TIMEOUT_UPDATE_TIMESTAMP, this->m_events_history[2].event_type);
    ASSERT_EQ(EVENT_HISTORY_ADV_MQTT_CFG_CACHE_MUTEX_UNLOCK, this->m_events_history[3].event_type);
    this->m_events_history.clear();

    this->m_adv_mqtt_cfg_cache.flag_use_ntp = false;

    ASSERT_FALSE(adv_mqtt_handle_sig(ADV_MQTT_SIG_ON_RECV_ADV, &adv_mqtt_state));
    ASSERT_FALSE(adv_mqtt_state.flag_stop);
    ASSERT_EQ(4, this->m_events_history.size());
    ASSERT_EQ(EVENT_HISTORY_ADV_MQTT_CFG_CACHE_MUTEX_LOCK, this->m_events_history[0].event_type);
    ASSERT_EQ(EVENT_HISTORY_MQTT_PUBLISH_ADV, this->m_events_history[1].event_type);
    ASSERT_EQ(EVENT_HISTORY_NETWORK_TIMEOUT_UPDATE_TIMESTAMP, this->m_events_history[2].event_type);
    ASSERT_EQ(EVENT_HISTORY_ADV_MQTT_CFG_CACHE_MUTEX_UNLOCK, this->m_events_history[3].event_type);
    this->m_events_history.clear();

    this->m_time_is_synchronized = true;

    ASSERT_FALSE(adv_mqtt_handle_sig(ADV_MQTT_SIG_ON_RECV_ADV, &adv_mqtt_state));
    ASSERT_FALSE(adv_mqtt_state.flag_stop);
    ASSERT_EQ(4, this->m_events_history.size());
    ASSERT_EQ(EVENT_HISTORY_ADV_MQTT_CFG_CACHE_MUTEX_LOCK, this->m_events_history[0].event_type);
    ASSERT_EQ(EVENT_HISTORY_MQTT_PUBLISH_ADV, this->m_events_history[1].event_type);
    ASSERT_EQ(EVENT_HISTORY_NETWORK_TIMEOUT_UPDATE_TIMESTAMP, this->m_events_history[2].event_type);
    ASSERT_EQ(EVENT_HISTORY_ADV_MQTT_CFG_CACHE_MUTEX_UNLOCK, this->m_events_history[3].event_type);
    this->m_events_history.clear();

    this->m_adv_mqtt_cfg_cache.flag_use_ntp = true;

    this->adv_table_read_retransmission_list3_head_res = false;

    ASSERT_FALSE(adv_mqtt_handle_sig(ADV_MQTT_SIG_ON_RECV_ADV, &adv_mqtt_state));
    ASSERT_FALSE(adv_mqtt_state.flag_stop);
    ASSERT_EQ(2, this->m_events_history.size());
    ASSERT_EQ(EVENT_HISTORY_ADV_MQTT_CFG_CACHE_MUTEX_LOCK, this->m_events_history[0].event_type);
    ASSERT_EQ(EVENT_HISTORY_ADV_MQTT_CFG_CACHE_MUTEX_UNLOCK, this->m_events_history[3].event_type);
    this->m_events_history.clear();

    this->adv_table_read_retransmission_list3_head_res = true;

    this->mqtt_publish_adv_res = false;

    ASSERT_FALSE(adv_mqtt_handle_sig(ADV_MQTT_SIG_ON_RECV_ADV, &adv_mqtt_state));
    ASSERT_FALSE(adv_mqtt_state.flag_stop);
    ASSERT_EQ(3, this->m_events_history.size());
    ASSERT_EQ(EVENT_HISTORY_ADV_MQTT_CFG_CACHE_MUTEX_LOCK, this->m_events_history[0].event_type);
    ASSERT_EQ(EVENT_HISTORY_MQTT_PUBLISH_ADV, this->m_events_history[1].event_type);
    ASSERT_EQ(EVENT_HISTORY_ADV_MQTT_CFG_CACHE_MUTEX_UNLOCK, this->m_events_history[2].event_type);
    this->m_events_history.clear();

    this->mqtt_publish_adv_res                     = true;
    this->mqtt_is_buffer_available_for_publish_res = false;

    ASSERT_FALSE(adv_mqtt_handle_sig(ADV_MQTT_SIG_ON_RECV_ADV, &adv_mqtt_state));
    ASSERT_FALSE(adv_mqtt_state.flag_stop);
    ASSERT_EQ(3, this->m_events_history.size());
    ASSERT_EQ(EVENT_HISTORY_ADV_MQTT_CFG_CACHE_MUTEX_LOCK, this->m_events_history[0].event_type);
    ASSERT_EQ(EVENT_HISTORY_ADV_MQTT_TIMER_SIG_RETRY_SENDING_ADVS, this->m_events_history[1].event_type);
    ASSERT_EQ(EVENT_HISTORY_ADV_MQTT_CFG_CACHE_MUTEX_UNLOCK, this->m_events_history[2].event_type);
    this->m_events_history.clear();

    this->mqtt_is_buffer_available_for_publish_res = true;

    this->m_adv_mqtt_cfg_cache.flag_mqtt_instant_mode_active = false;

    ASSERT_FALSE(adv_mqtt_handle_sig(ADV_MQTT_SIG_ON_RECV_ADV, &adv_mqtt_state));
    ASSERT_FALSE(adv_mqtt_state.flag_stop);
    ASSERT_EQ(2, this->m_events_history.size());
    ASSERT_EQ(EVENT_HISTORY_ADV_MQTT_CFG_CACHE_MUTEX_LOCK, this->m_events_history[0].event_type);
    ASSERT_EQ(EVENT_HISTORY_ADV_MQTT_CFG_CACHE_MUTEX_UNLOCK, this->m_events_history[1].event_type);
    this->m_events_history.clear();

    ASSERT_TRUE(this->m_mem_alloc_trace.is_empty());
}

TEST_F(TestAdvMqttSignals, test_adv_mqtt_handle_sig_mqtt_connected) // NOLINT
{
    adv_mqtt_signals_init();
    this->m_events_history.clear();

    adv_mqtt_state_t adv_mqtt_state = {
        .flag_stop = false,
    };
    ASSERT_FALSE(adv_mqtt_handle_sig(ADV_MQTT_SIG_MQTT_CONNECTED, &adv_mqtt_state));
    ASSERT_FALSE(adv_mqtt_state.flag_stop);
    ASSERT_EQ(1, this->m_events_history.size());
    ASSERT_EQ(EVENT_HISTORY_MQTT_PUBLISH_CONNECT, this->m_events_history[0].event_type);
    this->m_events_history.clear();

    ASSERT_TRUE(this->m_mem_alloc_trace.is_empty());
}

TEST_F(TestAdvMqttSignals, test_adv_mqtt_handle_sig_task_watchdog_feed) // NOLINT
{
    adv_mqtt_signals_init();
    this->m_events_history.clear();

    adv_mqtt_state_t adv_mqtt_state = {
        .flag_stop = false,
    };
    ASSERT_FALSE(adv_mqtt_handle_sig(ADV_MQTT_SIG_TASK_WATCHDOG_FEED, &adv_mqtt_state));
    ASSERT_FALSE(adv_mqtt_state.flag_stop);
    ASSERT_EQ(1, this->m_events_history.size());
    ASSERT_EQ(EVENT_HISTORY_ESP_TASK_WDT_RESET, this->m_events_history[0].event_type);
    this->m_events_history.clear();

    ASSERT_TRUE(this->m_mem_alloc_trace.is_empty());
}

TEST_F(TestAdvMqttSignals, test_adv_mqtt_handle_sig_relaying_mode_changed) // NOLINT
{
    adv_mqtt_signals_init();
    this->m_events_history.clear();

    adv_mqtt_state_t adv_mqtt_state = {
        .flag_stop = false,
    };
    ASSERT_FALSE(adv_mqtt_handle_sig(ADV_MQTT_SIG_RELAYING_MODE_CHANGED, &adv_mqtt_state));
    ASSERT_FALSE(adv_mqtt_state.flag_stop);
    ASSERT_EQ(0, this->m_events_history.size());
    this->m_events_history.clear();

    ASSERT_TRUE(this->m_mem_alloc_trace.is_empty());
}
