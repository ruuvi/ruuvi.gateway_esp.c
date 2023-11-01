/**
 * @file test_adv_mqtt_task.cpp
 * @author TheSomeMan
 * @date 2023-10-30
 * @copyright Ruuvi Innovations Ltd, license BSD-3-Clause.
 */

#include "adv_mqtt_task.h"
#include "gtest/gtest.h"
#include <string>
#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "os_signal.h"
#include "adv_mqtt_signals.h"

using namespace std;

class TestAdvMqttTask;

static TestAdvMqttTask* g_pTestClass;

/*** Google-test class implementation
 * *********************************************************************************/

enum event_history_e
{
    EVENT_HISTORY_SIG_STOP                  = ADV_MQTT_SIG_STOP,
    EVENT_HISTORY_SIG_ON_RECV_ADV           = ADV_MQTT_SIG_ON_RECV_ADV,
    EVENT_HISTORY_SIG_MQTT_CONNECTED        = ADV_MQTT_SIG_MQTT_CONNECTED,
    EVENT_HISTORY_SIG_TASK_WATCHDOG_FEED    = ADV_MQTT_SIG_TASK_WATCHDOG_FEED,
    EVENT_HISTORY_SIG_GW_CFG_READY          = ADV_MQTT_SIG_GW_CFG_READY,
    EVENT_HISTORY_SIG_GW_CFG_CHANGED_RUUVI  = ADV_MQTT_SIG_GW_CFG_CHANGED_RUUVI,
    EVENT_HISTORY_SIG_RELAYING_MODE_CHANGED = ADV_MQTT_SIG_RELAYING_MODE_CHANGED,
    EVENT_HISTORY_SIG_CFG_MODE_DEACTIVATED  = ADV_MQTT_SIG_CFG_MODE_DEACTIVATED,

    EVENT_HISTORY_ADV_MQTT_SIGNALS_INIT,
    EVENT_HISTORY_ADV_MQTT_SIGNALS_DEINIT,
    EVENT_HISTORY_ADV_MQTT_SUBSCRIBE_EVENTS,
    EVENT_HISTORY_ADV_MQTT_UNSUBSCRIBE_EVENTS,
    EVENT_HISTORY_ADV_MQTT_CREATE_TIMERS,
    EVENT_HISTORY_ADV_MQTT_DELETE_TIMERS,
    EVENT_HISTORY_OS_SIGNAL_REGISTER_CUR_THREAD,
    EVENT_HISTORY_ESP_TASK_WDT_ADD,
    EVENT_HISTORY_ESP_TASK_WDT_DELETE,
    EVENT_HISTORY_START_TIMER_SIG_WATCHDOG_FEED,
    EVENT_HISTORY_ADV_MQTT_SEND_SIG_STOP,
};

class TestAdvMqttTask : public ::testing::Test
{
private:
protected:
    void
    SetUp() override
    {
        g_pTestClass                  = this;
        this->m_os_signal_wait_events = nullptr;
        this->m_events_history.clear();
        this->m_os_signal_register_cur_thread_res  = true;
        this->m_os_signal_wait_with_timeout_failed = false;
    }

    void
    TearDown() override
    {
        if (nullptr != this->m_os_signal_wait_events)
        {
            delete this->m_os_signal_wait_events;
            this->m_os_signal_wait_events = nullptr;
        }
        g_pTestClass = nullptr;
    }

public:
    TestAdvMqttTask();

    ~TestAdvMqttTask() override;

    std::vector<os_signal_sig_mask_t>* m_os_signal_wait_events {};
    std::vector<event_history_e>       m_events_history {};
    bool                               m_os_signal_register_cur_thread_res { false };
    bool                               m_os_signal_wait_with_timeout_failed { false };
};

TestAdvMqttTask::TestAdvMqttTask()
    : Test()
{
}

TestAdvMqttTask::~TestAdvMqttTask() = default;

extern "C" {

TaskHandle_t
xTaskGetCurrentTaskHandle(void)
{
    return nullptr;
}

esp_err_t
esp_task_wdt_add(TaskHandle_t handle)
{
    g_pTestClass->m_events_history.push_back(EVENT_HISTORY_ESP_TASK_WDT_ADD);
    return ESP_OK;
}

esp_err_t
esp_task_wdt_delete(TaskHandle_t handle)
{
    g_pTestClass->m_events_history.push_back(EVENT_HISTORY_ESP_TASK_WDT_DELETE);
    return ESP_OK;
}

bool
os_signal_register_cur_thread(os_signal_t* const p_signal)
{
    g_pTestClass->m_events_history.push_back(EVENT_HISTORY_OS_SIGNAL_REGISTER_CUR_THREAD);
    return g_pTestClass->m_os_signal_register_cur_thread_res;
}

bool
os_signal_wait_with_timeout(
    os_signal_t* const        p_signal,
    const os_delta_ticks_t    timeout_ticks,
    os_signal_events_t* const p_sig_events)
{
    assert(OS_DELTA_TICKS_INFINITE == timeout_ticks);
    assert(nullptr != g_pTestClass->m_os_signal_wait_events);
    const bool res                                     = !g_pTestClass->m_os_signal_wait_with_timeout_failed;
    g_pTestClass->m_os_signal_wait_with_timeout_failed = false;
    if (res)
    {
        assert(g_pTestClass->m_os_signal_wait_events->size() > 0);
        *p_sig_events = {
            .sig_mask = g_pTestClass->m_os_signal_wait_events->front(),
            .last_ofs = 0,
        };
        g_pTestClass->m_os_signal_wait_events->erase(g_pTestClass->m_os_signal_wait_events->begin());
    }
    else
    {
        *p_sig_events = {
            .sig_mask = 0,
            .last_ofs = 0,
        };
    }
    return res;
}

os_signal_num_e
os_signal_num_get_next(os_signal_events_t* const p_sig_events)
{
    if (0 == p_sig_events->sig_mask)
    {
        return OS_SIGNAL_NUM_NONE;
    }
    const uint32_t max_bit_idx = OS_SIGNAL_NUM_30 - OS_SIGNAL_NUM_0;
    for (uint32_t i = p_sig_events->last_ofs; i <= max_bit_idx; ++i)
    {
        const uint32_t mask = 1U << i;
        if (p_sig_events->sig_mask & mask)
        {
            p_sig_events->sig_mask ^= mask;
            p_sig_events->last_ofs = i;
            return (os_signal_num_e)(OS_SIGNAL_NUM_0 + i);
        }
    }
    return OS_SIGNAL_NUM_NONE;
}

bool
adv_mqtt_signals_is_thread_registered(void)
{
    return true;
}

void
adv_mqtt_signals_init(void)
{
    g_pTestClass->m_events_history.push_back(EVENT_HISTORY_ADV_MQTT_SIGNALS_INIT);
}

void
adv_mqtt_signals_deinit(void)
{
    g_pTestClass->m_events_history.push_back(EVENT_HISTORY_ADV_MQTT_SIGNALS_DEINIT);
}

os_signal_t*
adv_mqtt_signals_get(void)
{
    return nullptr;
}

adv_mqtt_sig_e
adv_mqtt_conv_from_sig_num(const os_signal_num_e sig_num)
{
    return (adv_mqtt_sig_e)sig_num;
}

void
adv_mqtt_signals_send_sig(const adv_mqtt_sig_e adv_mqtt_sig)
{
    g_pTestClass->m_events_history.push_back(EVENT_HISTORY_ADV_MQTT_SEND_SIG_STOP);
}

void
adv_mqtt_subscribe_events(void)
{
    g_pTestClass->m_events_history.push_back(EVENT_HISTORY_ADV_MQTT_SUBSCRIBE_EVENTS);
}

void
adv_mqtt_unsubscribe_events(void)
{
    g_pTestClass->m_events_history.push_back(EVENT_HISTORY_ADV_MQTT_UNSUBSCRIBE_EVENTS);
}

void
adv_mqtt_create_timers(void)
{
    g_pTestClass->m_events_history.push_back(EVENT_HISTORY_ADV_MQTT_CREATE_TIMERS);
}

void
adv_mqtt_delete_timers(void)
{
    g_pTestClass->m_events_history.push_back(EVENT_HISTORY_ADV_MQTT_DELETE_TIMERS);
}

bool
adv_mqtt_handle_sig(const adv_mqtt_sig_e adv_mqtt_sig, adv_mqtt_state_t* const p_adv_mqtt_state)
{
    assert(adv_mqtt_task_is_initialized());
    g_pTestClass->m_events_history.push_back((event_history_e)adv_mqtt_sig);
    if (adv_mqtt_sig == ADV_MQTT_SIG_STOP)
    {
        p_adv_mqtt_state->flag_stop = true;
    }
    return p_adv_mqtt_state->flag_stop;
}

void
adv_mqtt_timers_start_timer_sig_watchdog_feed(void)
{
    g_pTestClass->m_events_history.push_back(EVENT_HISTORY_START_TIMER_SIG_WATCHDOG_FEED);
}

} // extern "C"

/*** Unit-Tests
 * *******************************************************************************************************/

TEST_F(TestAdvMqttTask, test_sig_stop) // NOLINT
{
    this->m_os_signal_wait_events = new std::vector<os_signal_sig_mask_t>(
        { 1U << (ADV_MQTT_SIG_STOP - OS_SIGNAL_NUM_0) });

    adv_mqtt_task();

    ASSERT_EQ(0, this->m_os_signal_wait_events->size());

    const std::vector<event_history_e> expected_history = {
        EVENT_HISTORY_ADV_MQTT_SIGNALS_INIT,
        EVENT_HISTORY_ADV_MQTT_SUBSCRIBE_EVENTS,
        EVENT_HISTORY_ADV_MQTT_CREATE_TIMERS,
        EVENT_HISTORY_OS_SIGNAL_REGISTER_CUR_THREAD,
        EVENT_HISTORY_ESP_TASK_WDT_ADD,
        EVENT_HISTORY_START_TIMER_SIG_WATCHDOG_FEED,

        EVENT_HISTORY_SIG_STOP,

        EVENT_HISTORY_ESP_TASK_WDT_DELETE,
        EVENT_HISTORY_ADV_MQTT_DELETE_TIMERS,
        EVENT_HISTORY_ADV_MQTT_UNSUBSCRIBE_EVENTS,
        EVENT_HISTORY_ADV_MQTT_SIGNALS_DEINIT,
    };
    ASSERT_EQ(expected_history, this->m_events_history);
}

TEST_F(TestAdvMqttTask, test_1) // NOLINT
{
    this->m_os_signal_wait_events = new std::vector<os_signal_sig_mask_t>({
        (1U << (ADV_MQTT_SIG_ON_RECV_ADV - OS_SIGNAL_NUM_0))
            | (1U << (ADV_MQTT_SIG_TASK_WATCHDOG_FEED - OS_SIGNAL_NUM_0)),
        (1U << (ADV_MQTT_SIG_MQTT_CONNECTED - OS_SIGNAL_NUM_0)),
        (1U << (ADV_MQTT_SIG_GW_CFG_READY - OS_SIGNAL_NUM_0)),
        (1U << (ADV_MQTT_SIG_GW_CFG_CHANGED_RUUVI - OS_SIGNAL_NUM_0)),
        (1U << (ADV_MQTT_SIG_RELAYING_MODE_CHANGED - OS_SIGNAL_NUM_0)),
        (1U << (ADV_MQTT_SIG_CFG_MODE_DEACTIVATED - OS_SIGNAL_NUM_0)),
        1U << (ADV_MQTT_SIG_STOP - OS_SIGNAL_NUM_0),
    });

    adv_mqtt_task();

    ASSERT_EQ(0, this->m_os_signal_wait_events->size());

    const std::vector<event_history_e> expected_history = {
        EVENT_HISTORY_ADV_MQTT_SIGNALS_INIT,
        EVENT_HISTORY_ADV_MQTT_SUBSCRIBE_EVENTS,
        EVENT_HISTORY_ADV_MQTT_CREATE_TIMERS,
        EVENT_HISTORY_OS_SIGNAL_REGISTER_CUR_THREAD,
        EVENT_HISTORY_ESP_TASK_WDT_ADD,
        EVENT_HISTORY_START_TIMER_SIG_WATCHDOG_FEED,

        EVENT_HISTORY_SIG_ON_RECV_ADV,
        EVENT_HISTORY_SIG_TASK_WATCHDOG_FEED,
        EVENT_HISTORY_SIG_MQTT_CONNECTED,
        EVENT_HISTORY_SIG_GW_CFG_READY,
        EVENT_HISTORY_SIG_GW_CFG_CHANGED_RUUVI,
        EVENT_HISTORY_SIG_RELAYING_MODE_CHANGED,
        EVENT_HISTORY_SIG_CFG_MODE_DEACTIVATED,
        EVENT_HISTORY_SIG_STOP,

        EVENT_HISTORY_ESP_TASK_WDT_DELETE,
        EVENT_HISTORY_ADV_MQTT_DELETE_TIMERS,
        EVENT_HISTORY_ADV_MQTT_UNSUBSCRIBE_EVENTS,
        EVENT_HISTORY_ADV_MQTT_SIGNALS_DEINIT,
    };
    ASSERT_EQ(expected_history, this->m_events_history);
}

TEST_F(TestAdvMqttTask, test_os_signal_register_cur_thread_failed) // NOLINT
{
    this->m_os_signal_register_cur_thread_res = false;

    this->m_os_signal_wait_events = new std::vector<os_signal_sig_mask_t>();

    adv_mqtt_task();

    ASSERT_EQ(0, this->m_os_signal_wait_events->size());

    const std::vector<event_history_e> expected_history = {
        EVENT_HISTORY_ADV_MQTT_SIGNALS_INIT,   EVENT_HISTORY_ADV_MQTT_SUBSCRIBE_EVENTS,
        EVENT_HISTORY_ADV_MQTT_CREATE_TIMERS,  EVENT_HISTORY_OS_SIGNAL_REGISTER_CUR_THREAD,

        EVENT_HISTORY_ADV_MQTT_DELETE_TIMERS,  EVENT_HISTORY_ADV_MQTT_UNSUBSCRIBE_EVENTS,
        EVENT_HISTORY_ADV_MQTT_SIGNALS_DEINIT,
    };
    ASSERT_EQ(expected_history, this->m_events_history);
}

TEST_F(TestAdvMqttTask, test_os_signal_wait_with_timeout_failed) // NOLINT
{
    this->m_os_signal_wait_with_timeout_failed = true;

    this->m_os_signal_wait_events = new std::vector<os_signal_sig_mask_t>(
        { 1U << (ADV_MQTT_SIG_STOP - OS_SIGNAL_NUM_0) });

    adv_mqtt_task();

    ASSERT_EQ(0, this->m_os_signal_wait_events->size());

    const std::vector<event_history_e> expected_history = {
        EVENT_HISTORY_ADV_MQTT_SIGNALS_INIT,
        EVENT_HISTORY_ADV_MQTT_SUBSCRIBE_EVENTS,
        EVENT_HISTORY_ADV_MQTT_CREATE_TIMERS,
        EVENT_HISTORY_OS_SIGNAL_REGISTER_CUR_THREAD,
        EVENT_HISTORY_ESP_TASK_WDT_ADD,
        EVENT_HISTORY_START_TIMER_SIG_WATCHDOG_FEED,

        EVENT_HISTORY_SIG_STOP,

        EVENT_HISTORY_ESP_TASK_WDT_DELETE,
        EVENT_HISTORY_ADV_MQTT_DELETE_TIMERS,
        EVENT_HISTORY_ADV_MQTT_UNSUBSCRIBE_EVENTS,
        EVENT_HISTORY_ADV_MQTT_SIGNALS_DEINIT,
    };
    ASSERT_EQ(expected_history, this->m_events_history);
}

TEST_F(TestAdvMqttTask, test_adv_mqtt_stop) // NOLINT
{
    adv_mqtt_task_stop();

    const std::vector<event_history_e> expected_history = {
        EVENT_HISTORY_ADV_MQTT_SEND_SIG_STOP,
    };
    ASSERT_EQ(expected_history, this->m_events_history);
}
