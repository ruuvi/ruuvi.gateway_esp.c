/**
 * @file test_adv_post_task.cpp
 * @author TheSomeMan
 * @date 2023-09-27
 * @copyright Ruuvi Innovations Ltd, license BSD-3-Clause.
 */

#include "adv_post_task.h"
#include "gtest/gtest.h"
#include <string>
#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "os_signal.h"
#include "adv_post_signals.h"

using namespace std;

class TestAdvPostTask;

static TestAdvPostTask* g_pTestClass;

/*** Google-test class implementation
 * *********************************************************************************/

enum event_history_e
{
    EVENT_HISTORY_SIG_STOP                  = ADV_POST_SIG_STOP,
    EVENT_HISTORY_SIG_NETWORK_DISCONNECTED  = ADV_POST_SIG_NETWORK_DISCONNECTED,
    EVENT_HISTORY_SIG_NETWORK_CONNECTED     = ADV_POST_SIG_NETWORK_CONNECTED,
    EVENT_HISTORY_SIG_TIME_SYNCHRONIZED     = ADV_POST_SIG_TIME_SYNCHRONIZED,
    EVENT_HISTORY_SIG_RETRANSMIT            = ADV_POST_SIG_RETRANSMIT,
    EVENT_HISTORY_SIG_RETRANSMIT2           = ADV_POST_SIG_RETRANSMIT2,
    EVENT_HISTORY_SIG_RETRANSMIT_MQTT       = ADV_POST_SIG_RETRANSMIT_MQTT,
    EVENT_HISTORY_SIG_SEND_STATISTICS       = ADV_POST_SIG_SEND_STATISTICS,
    EVENT_HISTORY_SIG_DO_ASYNC_COMM         = ADV_POST_SIG_DO_ASYNC_COMM,
    EVENT_HISTORY_SIG_RELAYING_MODE_CHANGED = ADV_POST_SIG_RELAYING_MODE_CHANGED,
    EVENT_HISTORY_SIG_NETWORK_WATCHDOG      = ADV_POST_SIG_NETWORK_WATCHDOG,
    EVENT_HISTORY_SIG_TASK_WATCHDOG_FEED    = ADV_POST_SIG_TASK_WATCHDOG_FEED,
    EVENT_HISTORY_SIG_GW_CFG_READY          = ADV_POST_SIG_GW_CFG_READY,
    EVENT_HISTORY_SIG_GW_CFG_CHANGED_RUUVI  = ADV_POST_SIG_GW_CFG_CHANGED_RUUVI,
    EVENT_HISTORY_SIG_BLE_SCAN_CHANGED      = ADV_POST_SIG_BLE_SCAN_CHANGED,
    EVENT_HISTORY_SIG_ACTIVATE_CFG_MODE     = ADV_POST_SIG_CFG_MODE_ACTIVATED,
    EVENT_HISTORY_SIG_DEACTIVATE_CFG_MODE   = ADV_POST_SIG_CFG_MODE_DEACTIVATED,
    EVENT_HISTORY_SIG_GREEN_LED_TURN_ON     = ADV_POST_SIG_GREEN_LED_TURN_ON,
    EVENT_HISTORY_SIG_GREEN_LED_TURN_OFF    = ADV_POST_SIG_GREEN_LED_TURN_OFF,
    EVENT_HISTORY_SIG_GREEN_LED_UPDATE      = ADV_POST_SIG_GREEN_LED_UPDATE,
    EVENT_HISTORY_SIG_RECV_ADV_TIMEOUT      = ADV_POST_SIG_RECV_ADV_TIMEOUT,

    EVENT_HISTORY_ADV_POST_SIGNALS_INIT,
    EVENT_HISTORY_ADV_POST_SIGNALS_DEINIT,
    EVENT_HISTORY_ADV_POST_SUBSCRIBE_EVENTS,
    EVENT_HISTORY_ADV_POST_UNSUBSCRIBE_EVENTS,
    EVENT_HISTORY_ADV_POST_CREATE_TIMERS,
    EVENT_HISTORY_ADV_POST_DELETE_TIMERS,
    EVENT_HISTORY_OS_SIGNAL_REGISTER_CUR_THREAD,
    EVENT_HISTORY_START_TIMER_SIG_NETWORK_WATCHDOG,
    EVENT_HISTORY_START_TIMER_SIG_RECV_ADV_TIMEOUT,
    EVENT_HISTORY_ESP_TASK_WDT_ADD,
    EVENT_HISTORY_ESP_TASK_WDT_DELETE,
    EVENT_HISTORY_START_TIMER_SIG_WATCHDOG_FEED,
    EVENT_HISTORY_HTTP_ABORT_ANY_REQ_DURING_PROCESSING,
    EVENT_HISTORY_ADV_POST_SEND_SIG_STOP,
};

class TestAdvPostTask : public ::testing::Test
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
    TestAdvPostTask();

    ~TestAdvPostTask() override;

    std::vector<os_signal_sig_mask_t>* m_os_signal_wait_events {};
    std::vector<event_history_e>       m_events_history {};
    bool                               m_os_signal_register_cur_thread_res { false };
    bool                               m_os_signal_wait_with_timeout_failed { false };
};

TestAdvPostTask::TestAdvPostTask()
    : Test()
{
}

TestAdvPostTask::~TestAdvPostTask() = default;

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
adv_post_signals_is_thread_registered(void)
{
    return true;
}

void
adv_post_signals_init(void)
{
    g_pTestClass->m_events_history.push_back(EVENT_HISTORY_ADV_POST_SIGNALS_INIT);
}

void
adv_post_signals_deinit(void)
{
    g_pTestClass->m_events_history.push_back(EVENT_HISTORY_ADV_POST_SIGNALS_DEINIT);
}

os_signal_t*
adv_post_signals_get(void)
{
    return nullptr;
}

adv_post_sig_e
adv_post_conv_from_sig_num(const os_signal_num_e sig_num)
{
    return (adv_post_sig_e)sig_num;
}

void
adv_post_signals_send_sig(const adv_post_sig_e adv_post_sig)
{
    g_pTestClass->m_events_history.push_back(EVENT_HISTORY_ADV_POST_SEND_SIG_STOP);
}

void
adv_post_subscribe_events(void)
{
    g_pTestClass->m_events_history.push_back(EVENT_HISTORY_ADV_POST_SUBSCRIBE_EVENTS);
}

void
adv_post_unsubscribe_events(void)
{
    g_pTestClass->m_events_history.push_back(EVENT_HISTORY_ADV_POST_UNSUBSCRIBE_EVENTS);
}

void
adv_post_create_timers(void)
{
    g_pTestClass->m_events_history.push_back(EVENT_HISTORY_ADV_POST_CREATE_TIMERS);
}

void
adv_post_delete_timers(void)
{
    g_pTestClass->m_events_history.push_back(EVENT_HISTORY_ADV_POST_DELETE_TIMERS);
}

void
adv_post_timers_start_timer_sig_network_watchdog(void)
{
    g_pTestClass->m_events_history.push_back(EVENT_HISTORY_START_TIMER_SIG_NETWORK_WATCHDOG);
}

void
adv_post_timers_start_timer_sig_recv_adv_timeout(void)
{
    g_pTestClass->m_events_history.push_back(EVENT_HISTORY_START_TIMER_SIG_RECV_ADV_TIMEOUT);
}

bool
adv_post_handle_sig(const adv_post_sig_e adv_post_sig, adv_post_state_t* const p_adv_post_state)
{
    assert(adv_post_task_is_initialized());
    g_pTestClass->m_events_history.push_back((event_history_e)adv_post_sig);
    if (adv_post_sig == ADV_POST_SIG_STOP)
    {
        p_adv_post_state->flag_stop = true;
    }
    return p_adv_post_state->flag_stop;
}

void
adv_post_timers_start_timer_sig_watchdog_feed(void)
{
    g_pTestClass->m_events_history.push_back(EVENT_HISTORY_START_TIMER_SIG_WATCHDOG_FEED);
}

void
http_abort_any_req_during_processing(void)
{
    g_pTestClass->m_events_history.push_back(EVENT_HISTORY_HTTP_ABORT_ANY_REQ_DURING_PROCESSING);
}

} // extern "C"

/*** Unit-Tests
 * *******************************************************************************************************/

TEST_F(TestAdvPostTask, test_sig_stop) // NOLINT
{
    this->m_os_signal_wait_events = new std::vector<os_signal_sig_mask_t>(
        { 1U << (ADV_POST_SIG_STOP - OS_SIGNAL_NUM_0) });

    adv_post_task();

    ASSERT_EQ(0, this->m_os_signal_wait_events->size());

    const std::vector<event_history_e> expected_history = {
        EVENT_HISTORY_ADV_POST_SIGNALS_INIT,
        EVENT_HISTORY_ADV_POST_SUBSCRIBE_EVENTS,
        EVENT_HISTORY_ADV_POST_CREATE_TIMERS,
        EVENT_HISTORY_OS_SIGNAL_REGISTER_CUR_THREAD,
        EVENT_HISTORY_START_TIMER_SIG_NETWORK_WATCHDOG,
        EVENT_HISTORY_START_TIMER_SIG_RECV_ADV_TIMEOUT,
        EVENT_HISTORY_ESP_TASK_WDT_ADD,
        EVENT_HISTORY_START_TIMER_SIG_WATCHDOG_FEED,

        EVENT_HISTORY_SIG_STOP,

        EVENT_HISTORY_ESP_TASK_WDT_DELETE,
        EVENT_HISTORY_ADV_POST_DELETE_TIMERS,
        EVENT_HISTORY_ADV_POST_UNSUBSCRIBE_EVENTS,
        EVENT_HISTORY_HTTP_ABORT_ANY_REQ_DURING_PROCESSING,
        EVENT_HISTORY_ADV_POST_SIGNALS_DEINIT,
    };
    ASSERT_EQ(expected_history, this->m_events_history);
}

TEST_F(TestAdvPostTask, test_1) // NOLINT
{
    this->m_os_signal_wait_events = new std::vector<os_signal_sig_mask_t>({
        (1U << (ADV_POST_SIG_RETRANSMIT - OS_SIGNAL_NUM_0)) | (1U << (ADV_POST_SIG_RETRANSMIT2 - OS_SIGNAL_NUM_0))
            | (1U << (ADV_POST_SIG_RETRANSMIT_MQTT - OS_SIGNAL_NUM_0))
            | (1U << (ADV_POST_SIG_SEND_STATISTICS - OS_SIGNAL_NUM_0)),
        (1U << (ADV_POST_SIG_DO_ASYNC_COMM - OS_SIGNAL_NUM_0)),
        (1U << (ADV_POST_SIG_DO_ASYNC_COMM - OS_SIGNAL_NUM_0)),
        1U << (ADV_POST_SIG_STOP - OS_SIGNAL_NUM_0),
    });

    adv_post_task();

    ASSERT_EQ(0, this->m_os_signal_wait_events->size());

    const std::vector<event_history_e> expected_history = {
        EVENT_HISTORY_ADV_POST_SIGNALS_INIT,
        EVENT_HISTORY_ADV_POST_SUBSCRIBE_EVENTS,
        EVENT_HISTORY_ADV_POST_CREATE_TIMERS,
        EVENT_HISTORY_OS_SIGNAL_REGISTER_CUR_THREAD,
        EVENT_HISTORY_START_TIMER_SIG_NETWORK_WATCHDOG,
        EVENT_HISTORY_START_TIMER_SIG_RECV_ADV_TIMEOUT,
        EVENT_HISTORY_ESP_TASK_WDT_ADD,
        EVENT_HISTORY_START_TIMER_SIG_WATCHDOG_FEED,

        EVENT_HISTORY_SIG_RETRANSMIT,
        EVENT_HISTORY_SIG_RETRANSMIT2,
        EVENT_HISTORY_SIG_RETRANSMIT_MQTT,
        EVENT_HISTORY_SIG_SEND_STATISTICS,
        EVENT_HISTORY_SIG_DO_ASYNC_COMM,
        EVENT_HISTORY_SIG_DO_ASYNC_COMM,
        EVENT_HISTORY_SIG_STOP,

        EVENT_HISTORY_ESP_TASK_WDT_DELETE,
        EVENT_HISTORY_ADV_POST_DELETE_TIMERS,
        EVENT_HISTORY_ADV_POST_UNSUBSCRIBE_EVENTS,
        EVENT_HISTORY_HTTP_ABORT_ANY_REQ_DURING_PROCESSING,
        EVENT_HISTORY_ADV_POST_SIGNALS_DEINIT,
    };
    ASSERT_EQ(expected_history, this->m_events_history);
}

TEST_F(TestAdvPostTask, test_os_signal_register_cur_thread_failed) // NOLINT
{
    this->m_os_signal_register_cur_thread_res = false;

    this->m_os_signal_wait_events = new std::vector<os_signal_sig_mask_t>();

    adv_post_task();

    ASSERT_EQ(0, this->m_os_signal_wait_events->size());

    const std::vector<event_history_e> expected_history = {
        EVENT_HISTORY_ADV_POST_SIGNALS_INIT,   EVENT_HISTORY_ADV_POST_SUBSCRIBE_EVENTS,
        EVENT_HISTORY_ADV_POST_CREATE_TIMERS,  EVENT_HISTORY_OS_SIGNAL_REGISTER_CUR_THREAD,

        EVENT_HISTORY_ADV_POST_DELETE_TIMERS,  EVENT_HISTORY_ADV_POST_UNSUBSCRIBE_EVENTS,
        EVENT_HISTORY_ADV_POST_SIGNALS_DEINIT,
    };
    ASSERT_EQ(expected_history, this->m_events_history);
}

TEST_F(TestAdvPostTask, test_os_signal_wait_with_timeout_failed) // NOLINT
{
    this->m_os_signal_wait_with_timeout_failed = true;

    this->m_os_signal_wait_events = new std::vector<os_signal_sig_mask_t>(
        { 1U << (ADV_POST_SIG_STOP - OS_SIGNAL_NUM_0) });

    adv_post_task();

    ASSERT_EQ(0, this->m_os_signal_wait_events->size());

    const std::vector<event_history_e> expected_history = {
        EVENT_HISTORY_ADV_POST_SIGNALS_INIT,
        EVENT_HISTORY_ADV_POST_SUBSCRIBE_EVENTS,
        EVENT_HISTORY_ADV_POST_CREATE_TIMERS,
        EVENT_HISTORY_OS_SIGNAL_REGISTER_CUR_THREAD,
        EVENT_HISTORY_START_TIMER_SIG_NETWORK_WATCHDOG,
        EVENT_HISTORY_START_TIMER_SIG_RECV_ADV_TIMEOUT,
        EVENT_HISTORY_ESP_TASK_WDT_ADD,
        EVENT_HISTORY_START_TIMER_SIG_WATCHDOG_FEED,

        EVENT_HISTORY_SIG_STOP,

        EVENT_HISTORY_ESP_TASK_WDT_DELETE,
        EVENT_HISTORY_ADV_POST_DELETE_TIMERS,
        EVENT_HISTORY_ADV_POST_UNSUBSCRIBE_EVENTS,
        EVENT_HISTORY_HTTP_ABORT_ANY_REQ_DURING_PROCESSING,
        EVENT_HISTORY_ADV_POST_SIGNALS_DEINIT,
    };
    ASSERT_EQ(expected_history, this->m_events_history);
}

TEST_F(TestAdvPostTask, test_adv_post_stop) // NOLINT
{
    adv_post_task_stop();

    const std::vector<event_history_e> expected_history = {
        EVENT_HISTORY_ADV_POST_SEND_SIG_STOP,
    };
    ASSERT_EQ(expected_history, this->m_events_history);
}
