/**
 * @file test_adv_post_timers.cpp
 * @author TheSomeMan
 * @date 2023-09-17
 * @copyright Ruuvi Innovations Ltd, license BSD-3-Clause.
 */

#include "adv_post_timers.h"
#include "gtest/gtest.h"
#include "adv_post_signals.h"
#include <string>
#include <unordered_map>

using namespace std;

class TestAdvPostTimers;

static TestAdvPostTimers* g_pTestClass;

struct TimerData
{
    const char*      p_timer_name;
    os_signal_t*     p_signal;
    os_signal_num_e  sig_num;
    os_delta_ticks_t period_ticks;
    TickType_t       timestamp_when_timer_was_triggered;
    TickType_t       timestamp_next_fire;
    bool             is_active;
    bool             flag_timer_triggered;
};

/*** Google-test class implementation
 * *********************************************************************************/

class TestAdvPostTimers : public ::testing::Test
{
private:
protected:
    void
    SetUp() override
    {
        g_pTestClass = this;
        adv_post_create_timers();
    }

    void
    TearDown() override
    {
        adv_post_delete_timers();
        assert(0 == this->m_timerMapPeriodic.size());
        assert(0 == this->m_timerMapOneShot.size());
        this->m_timerMapPeriodic.clear();
        this->m_timerMapOneShot.clear();
        g_pTestClass = nullptr;
    }

public:
    TestAdvPostTimers();

    ~TestAdvPostTimers() override;

    TimerData&
    findTimerSigPeriodicByName(const string& name);
    TimerData&
    findTimerSigOneShotByName(const string& name);

    int          m_signal   = 0;
    os_signal_t* m_p_signal = reinterpret_cast<os_signal_t*>(&m_signal);
    std::unordered_map<os_timer_sig_periodic_static_t*, TimerData> m_timerMapPeriodic;
    std::unordered_map<os_timer_sig_one_shot_static_t*, TimerData> m_timerMapOneShot;
};

TestAdvPostTimers::TestAdvPostTimers()
    : Test()
{
}

TestAdvPostTimers::~TestAdvPostTimers() = default;

TimerData&
TestAdvPostTimers::findTimerSigPeriodicByName(const string& name)
{
    for (auto& pair : g_pTestClass->m_timerMapPeriodic)
    {
        if (pair.second.p_timer_name == name)
        {
            return pair.second;
        }
    }
    throw std::runtime_error("Timer not found");
}

TimerData&
TestAdvPostTimers::findTimerSigOneShotByName(const string& name)
{
    for (auto& pair : g_pTestClass->m_timerMapOneShot)
    {
        if (pair.second.p_timer_name == name)
        {
            return pair.second;
        }
    }
    throw std::runtime_error("Timer not found");
}

extern "C" {

ATTR_PURE
os_signal_num_e
adv_post_conv_to_sig_num(const adv_post_sig_e sig)
{
    return (os_signal_num_e)sig;
}

os_signal_t*
adv_post_signals_get(void)
{
    return g_pTestClass->m_p_signal;
}

os_timer_sig_periodic_t*
os_timer_sig_periodic_create_static(
    os_timer_sig_periodic_static_t* const p_timer_sig_mem,
    const char* const                     p_timer_name,
    os_signal_t* const                    p_signal,
    const os_signal_num_e                 sig_num,
    const os_delta_ticks_t                period_ticks)
{
    auto& timerMap = g_pTestClass->m_timerMapPeriodic;
    auto  iter     = timerMap.find(p_timer_sig_mem);
    assert(iter == timerMap.end());

    timerMap[p_timer_sig_mem] = { p_timer_name, p_signal, sig_num, period_ticks, false };
    return reinterpret_cast<os_timer_sig_periodic_t*>(p_timer_sig_mem);
}

os_timer_sig_one_shot_t*
os_timer_sig_one_shot_create_static(
    os_timer_sig_one_shot_static_t* const p_timer_sig_mem,
    const char* const                     p_timer_name,
    os_signal_t* const                    p_signal,
    const os_signal_num_e                 sig_num,
    const os_delta_ticks_t                period_ticks)
{
    auto& timerMap = g_pTestClass->m_timerMapOneShot;
    auto  iter     = timerMap.find(p_timer_sig_mem);
    assert(iter == timerMap.end());
    timerMap[p_timer_sig_mem] = { p_timer_name, p_signal, sig_num, period_ticks, false };
    return reinterpret_cast<os_timer_sig_one_shot_t*>(p_timer_sig_mem);
}

void
os_timer_sig_periodic_start(os_timer_sig_periodic_t* const p_obj)
{
    auto& timerMap = g_pTestClass->m_timerMapPeriodic;
    auto  iter     = timerMap.find(reinterpret_cast<os_timer_sig_periodic_static_t*>(p_obj));
    assert(iter != timerMap.end());
    TimerData& timerData          = iter->second;
    timerData.timestamp_next_fire = xTaskGetTickCount() + timerData.period_ticks;
    timerData.is_active           = true;
}

void
os_timer_sig_one_shot_start(os_timer_sig_one_shot_t* const p_obj)
{
    auto& timerMap = g_pTestClass->m_timerMapOneShot;
    auto  iter     = timerMap.find(reinterpret_cast<os_timer_sig_one_shot_static_t*>(p_obj));
    assert(iter != timerMap.end());
    TimerData& timerData          = iter->second;
    timerData.timestamp_next_fire = xTaskGetTickCount() + timerData.period_ticks;
    timerData.is_active           = true;
}

void
os_timer_sig_periodic_stop(os_timer_sig_periodic_t* const p_obj)
{
    auto& timerMap = g_pTestClass->m_timerMapPeriodic;
    auto  iter     = timerMap.find(reinterpret_cast<os_timer_sig_periodic_static_t*>(p_obj));
    assert(iter != timerMap.end());
    TimerData& timerData = iter->second;
    timerData.is_active  = false;
}

void
os_timer_sig_one_shot_stop(os_timer_sig_one_shot_t* const p_obj)
{
    auto& timerMap = g_pTestClass->m_timerMapOneShot;
    auto  iter     = timerMap.find(reinterpret_cast<os_timer_sig_one_shot_static_t*>(p_obj));
    assert(iter != timerMap.end());
    TimerData& timerData = iter->second;
    timerData.is_active  = false;
}

void
os_timer_sig_periodic_delete(os_timer_sig_periodic_t** const pp_obj)
{
    auto& timerMap = g_pTestClass->m_timerMapPeriodic;
    auto  iter     = timerMap.find(reinterpret_cast<os_timer_sig_periodic_static_t*>(*pp_obj));
    assert(iter != timerMap.end());
    timerMap.erase(iter);
    *pp_obj = nullptr;
}

void
os_timer_sig_one_shot_delete(os_timer_sig_one_shot_t** const pp_obj)
{
    auto& timerMap = g_pTestClass->m_timerMapOneShot;
    auto  iter     = timerMap.find(reinterpret_cast<os_timer_sig_one_shot_static_t*>(*pp_obj));
    assert(iter != timerMap.end());
    timerMap.erase(iter);
    *pp_obj = nullptr;
}

void
os_timer_sig_periodic_relaunch(os_timer_sig_periodic_t* const p_obj, bool flag_restart_from_current_moment)
{
    auto& timerMap = g_pTestClass->m_timerMapPeriodic;
    auto  iter     = timerMap.find(reinterpret_cast<os_timer_sig_periodic_static_t*>(p_obj));
    assert(iter != timerMap.end());
    TimerData&       timerData = iter->second;
    const TickType_t cur_tick  = xTaskGetTickCount();
    if (flag_restart_from_current_moment)
    {
        timerData.timestamp_next_fire                = cur_tick + timerData.period_ticks;
        timerData.timestamp_when_timer_was_triggered = cur_tick;
    }
    else
    {
        const os_delta_ticks_t ticks_elapsed = cur_tick - timerData.timestamp_when_timer_was_triggered;
        if (ticks_elapsed >= timerData.period_ticks)
        {
            timerData.timestamp_next_fire                = cur_tick + timerData.period_ticks;
            timerData.timestamp_when_timer_was_triggered = cur_tick;
            timerData.flag_timer_triggered               = true;
        }
        else
        {
            timerData.timestamp_next_fire = timerData.timestamp_when_timer_was_triggered + timerData.period_ticks;
        }
    }
    timerData.is_active = true;
}

void
os_timer_sig_one_shot_relaunch(os_timer_sig_one_shot_t* const p_obj, const bool flag_restart_from_current_moment)
{
    auto& timerMap = g_pTestClass->m_timerMapOneShot;
    auto  iter     = timerMap.find(reinterpret_cast<os_timer_sig_one_shot_static_t*>(p_obj));
    assert(iter != timerMap.end());
    TimerData&       timerData = iter->second;
    const TickType_t cur_tick  = xTaskGetTickCount();
    if (flag_restart_from_current_moment)
    {
        timerData.timestamp_next_fire = cur_tick + timerData.period_ticks;
        timerData.is_active           = true;
    }
    else
    {
        const os_delta_ticks_t ticks_elapsed = cur_tick - timerData.timestamp_when_timer_was_triggered;
        if (ticks_elapsed >= timerData.period_ticks)
        {
            timerData.timestamp_when_timer_was_triggered = cur_tick;
            timerData.flag_timer_triggered               = true;
            timerData.is_active                          = false;
        }
        else
        {
            timerData.timestamp_next_fire = timerData.timestamp_when_timer_was_triggered + timerData.period_ticks;
            timerData.is_active           = true;
        }
    }
}

void
os_timer_sig_periodic_restart_with_period(
    os_timer_sig_periodic_t* const p_obj,
    const os_delta_ticks_t         delay_ticks,
    const bool                     flag_reset_active_timer)
{
    auto& timerMap = g_pTestClass->m_timerMapPeriodic;
    auto  iter     = timerMap.find(reinterpret_cast<os_timer_sig_periodic_static_t*>(p_obj));
    assert(iter != timerMap.end());
    TimerData& timerData   = iter->second;
    timerData.period_ticks = delay_ticks;
    timerData.is_active    = true;
}

os_delta_ticks_t
os_timer_sig_periodic_get_period(os_timer_sig_periodic_t* const p_obj)
{
    auto& timerMap = g_pTestClass->m_timerMapPeriodic;
    auto  iter     = timerMap.find(reinterpret_cast<os_timer_sig_periodic_static_t*>(p_obj));
    assert(iter != timerMap.end());
    TimerData& timerData = iter->second;
    return timerData.period_ticks;
}

void
os_timer_sig_periodic_set_period(os_timer_sig_periodic_t* const p_obj, const os_delta_ticks_t delay_ticks)
{
    auto& timerMap = g_pTestClass->m_timerMapPeriodic;
    auto  iter     = timerMap.find(reinterpret_cast<os_timer_sig_periodic_static_t*>(p_obj));
    assert(iter != timerMap.end());
    TimerData& timerData   = iter->second;
    timerData.period_ticks = delay_ticks;
}

TickType_t
xTaskGetTickCount(void)
{
    return 0;
}

void
os_timer_sig_periodic_update_timestamp_when_timer_was_triggered(
    os_timer_sig_periodic_t* const p_obj,
    const TickType_t               timestamp)
{
    auto& timerMap = g_pTestClass->m_timerMapPeriodic;
    auto  iter     = timerMap.find(reinterpret_cast<os_timer_sig_periodic_static_t*>(p_obj));
    assert(iter != timerMap.end());
    TimerData& timerData                         = iter->second;
    timerData.timestamp_when_timer_was_triggered = timestamp;
}

bool
os_timer_sig_periodic_is_active(os_timer_sig_periodic_t* const p_obj)
{
    auto& timerMap = g_pTestClass->m_timerMapPeriodic;
    auto  iter     = timerMap.find(reinterpret_cast<os_timer_sig_periodic_static_t*>(p_obj));
    assert(iter != timerMap.end());
    TimerData& timerData = iter->second;
    return timerData.is_active;
}

void
network_timeout_update_timestamp(void)
{
}

} // extern "C"

/*** Unit-Tests
 * *******************************************************************************************************/

TEST_F(TestAdvPostTimers, test_1) // NOLINT
{
    ASSERT_EQ(7, this->m_timerMapPeriodic.size());
    ASSERT_EQ(2, this->m_timerMapOneShot.size());

    os_timer_sig_periodic_t* p_timer_sig_greed_led_update = adv_post_timers_get_timer_sig_green_led_update();
    ASSERT_NE(nullptr, p_timer_sig_greed_led_update);
    auto& timerMap = g_pTestClass->m_timerMapPeriodic;
    auto  iter     = timerMap.find(reinterpret_cast<os_timer_sig_periodic_static_t*>(p_timer_sig_greed_led_update));
    assert(iter != timerMap.end());
    TimerData& timerData = iter->second;
    ASSERT_EQ(string("adv_post:led"), string(timerData.p_timer_name));
    ASSERT_EQ(ADV_POST_SIG_GREEN_LED_UPDATE, timerData.sig_num);
    ASSERT_EQ(1000, timerData.period_ticks);
}

TEST_F(TestAdvPostTimers, test_adv_post_timer_http_ruuvi) // NOLINT
{
    TimerData& timerData = this->findTimerSigPeriodicByName("adv_post_retransmit");
    ASSERT_FALSE(timerData.is_active);
    adv_post_timers_relaunch_timer_sig_retransmit_to_http_ruuvi();
    ASSERT_TRUE(timerData.is_active);
    ASSERT_FALSE(timerData.flag_timer_triggered);
    ASSERT_EQ(10 * 1000, timerData.period_ticks);

    adv_post_timers_relaunch_timer_sig_retransmit_to_http_ruuvi();
    ASSERT_TRUE(timerData.is_active);
    ASSERT_FALSE(timerData.flag_timer_triggered);
    ASSERT_EQ(10 * 1000, timerData.period_ticks);

    timerData.flag_timer_triggered = false;

    adv1_post_timer_stop();
    ASSERT_FALSE(timerData.is_active);
}

TEST_F(TestAdvPostTimers, test_adv_post_timer_http_custom) // NOLINT
{
    TimerData& timerData = this->findTimerSigPeriodicByName("adv_post_retransmit2");

    ASSERT_FALSE(timerData.is_active);
    adv_post_timers_relaunch_timer_sig_retransmit_to_http_custom();
    ASSERT_TRUE(timerData.is_active);
    ASSERT_FALSE(timerData.flag_timer_triggered);
    ASSERT_EQ(10 * 1000, timerData.period_ticks);

    adv_post_timers_relaunch_timer_sig_retransmit_to_http_custom();
    ASSERT_TRUE(timerData.is_active);
    ASSERT_FALSE(timerData.flag_timer_triggered);
    ASSERT_EQ(10 * 1000, timerData.period_ticks);

    adv2_post_timer_stop();
    ASSERT_FALSE(timerData.is_active);
}

TEST_F(TestAdvPostTimers, test_timer_mqtt) // NOLINT
{
    TimerData& timerData = this->findTimerSigPeriodicByName("adv_post_send_mqtt");

    ASSERT_FALSE(timerData.is_active);
    const os_delta_ticks_t delay_ticks = 3000;
    adv_post_timers_restart_timer_sig_mqtt(delay_ticks);
    ASSERT_TRUE(timerData.is_active);
    ASSERT_EQ(3000, timerData.period_ticks);

    adv_post_timers_stop_timer_sig_mqtt();
    ASSERT_FALSE(timerData.is_active);
}

TEST_F(TestAdvPostTimers, test_timer_sig_send_statistics) // NOLINT
{
    TimerData& timerData = this->findTimerSigPeriodicByName("adv_post_send_stat");

    adv_post_timers_postpone_sending_statistics();

    adv_post_timers_relaunch_timer_sig_send_statistics(false);
    ASSERT_TRUE(timerData.is_active);
    ASSERT_FALSE(timerData.flag_timer_triggered);
    ASSERT_EQ(60 * 60 * 1000, timerData.period_ticks);

    adv_post_timers_stop_timer_sig_send_statistics();
    ASSERT_FALSE(timerData.is_active);
    ASSERT_FALSE(timerData.flag_timer_triggered);

    adv_post_timers_relaunch_timer_sig_send_statistics(false);
    ASSERT_TRUE(timerData.is_active);
    ASSERT_FALSE(timerData.flag_timer_triggered);
    ASSERT_EQ(60 * 60 * 1000, timerData.period_ticks);

    adv_post_timers_relaunch_timer_sig_send_statistics(false);
    ASSERT_TRUE(timerData.is_active);
    ASSERT_FALSE(timerData.flag_timer_triggered);
    ASSERT_EQ(60 * 60 * 1000, timerData.period_ticks);

    adv_post_timers_relaunch_timer_sig_send_statistics(true);
    ASSERT_TRUE(timerData.is_active);
    ASSERT_FALSE(timerData.flag_timer_triggered);
    ASSERT_EQ(60 * 60 * 1000, timerData.period_ticks);

    adv_post_timers_stop_timer_sig_send_statistics();
    ASSERT_FALSE(timerData.is_active);
}

TEST_F(TestAdvPostTimers, test_timer_do_async_comm) // NOLINT
{
    TimerData& timerData = this->findTimerSigOneShotByName("adv_post_async");

    adv_post_timers_start_timer_sig_do_async_comm();
    ASSERT_TRUE(timerData.is_active);
    ASSERT_FALSE(timerData.flag_timer_triggered);
    ASSERT_EQ(50, timerData.period_ticks);

    adv_post_timers_stop_timer_sig_do_async_comm();
    ASSERT_FALSE(timerData.is_active);
}

TEST_F(TestAdvPostTimers, test_timer_watchdog_feed) // NOLINT
{
    TimerData& timerData = this->findTimerSigPeriodicByName("adv_post:wdog");
    ASSERT_FALSE(timerData.is_active);
    adv_post_timers_start_timer_sig_watchdog_feed();
    ASSERT_TRUE(timerData.is_active);
    ASSERT_FALSE(timerData.flag_timer_triggered);
    ASSERT_EQ(1000, timerData.period_ticks);
}

TEST_F(TestAdvPostTimers, test_timer_network_watchdog) // NOLINT
{
    TimerData& timerData = this->findTimerSigPeriodicByName("adv_post:netwdog");
    ASSERT_FALSE(timerData.is_active);
    adv_post_timers_start_timer_sig_network_watchdog();
    ASSERT_TRUE(timerData.is_active);
    ASSERT_FALSE(timerData.flag_timer_triggered);
    ASSERT_EQ(1000, timerData.period_ticks);
    adv_post_timers_stop_timer_sig_network_watchdog();
    ASSERT_FALSE(timerData.is_active);
}

TEST_F(TestAdvPostTimers, test_adv_post_timer_recv_adv_timeout) // NOLINT
{
    TimerData& timerData = this->findTimerSigOneShotByName("adv_post:timeout");
    ASSERT_FALSE(timerData.is_active);

    adv_post_timers_start_timer_sig_recv_adv_timeout();
    ASSERT_TRUE(timerData.is_active);
    ASSERT_FALSE(timerData.flag_timer_triggered);
    ASSERT_EQ(50 * 1000, timerData.period_ticks);

    adv_post_timers_relaunch_timer_sig_recv_adv_timeout();
    ASSERT_TRUE(timerData.is_active);
    ASSERT_FALSE(timerData.flag_timer_triggered);
    ASSERT_EQ(50 * 1000, timerData.period_ticks);
}

TEST_F(TestAdvPostTimers, test_adv_post_timer_http_ruuvi_reconfiguration) // NOLINT
{
    TimerData& timerData = this->findTimerSigPeriodicByName("adv_post_retransmit");
    ASSERT_FALSE(timerData.is_active);
    adv_post_timers_relaunch_timer_sig_retransmit_to_http_ruuvi();
    ASSERT_TRUE(timerData.is_active);
    ASSERT_FALSE(timerData.flag_timer_triggered);
    ASSERT_EQ(10 * 1000, timerData.period_ticks);

    adv_post_timers_relaunch_timer_sig_retransmit_to_http_ruuvi();
    ASSERT_TRUE(timerData.is_active);
    ASSERT_FALSE(timerData.flag_timer_triggered);
    ASSERT_EQ(10 * 1000, timerData.period_ticks);

    adv1_post_timer_relaunch_with_increased_period();
    ASSERT_TRUE(timerData.is_active);
    ASSERT_FALSE(timerData.flag_timer_triggered);
    ASSERT_EQ(67 * 1000, timerData.period_ticks);

    adv1_post_timer_stop();
    ASSERT_FALSE(timerData.is_active);

    adv1_post_timer_relaunch_with_increased_period();
    ASSERT_TRUE(timerData.is_active);
    ASSERT_FALSE(timerData.flag_timer_triggered);
    ASSERT_EQ(67 * 1000, timerData.period_ticks);

    adv1_post_timer_relaunch_with_increased_period();
    ASSERT_TRUE(timerData.is_active);
    ASSERT_FALSE(timerData.flag_timer_triggered);
    ASSERT_EQ(67 * 1000, timerData.period_ticks);

    adv1_post_timer_relaunch_with_default_period();
    ASSERT_TRUE(timerData.is_active);
    ASSERT_FALSE(timerData.flag_timer_triggered);
    ASSERT_EQ(10 * 1000, timerData.period_ticks);

    adv1_post_timer_set_default_period_by_server_resp(60 * 1000);
    ASSERT_TRUE(timerData.is_active);
    ASSERT_FALSE(timerData.flag_timer_triggered);
    ASSERT_EQ(10 * 1000, timerData.period_ticks);

    adv1_post_timer_set_default_period_by_server_resp(60 * 1000);
    ASSERT_TRUE(timerData.is_active);
    ASSERT_FALSE(timerData.flag_timer_triggered);
    ASSERT_EQ(10 * 1000, timerData.period_ticks);

    adv1_post_timer_relaunch_with_default_period();
    ASSERT_TRUE(timerData.is_active);
    ASSERT_FALSE(timerData.flag_timer_triggered);
    ASSERT_EQ(60 * 1000, timerData.period_ticks);

    adv1_post_timer_stop();
    ASSERT_FALSE(timerData.is_active);
}

TEST_F(TestAdvPostTimers, test_adv_post_timer_http_custom_reconfiguration) // NOLINT
{
    TimerData& timerData = this->findTimerSigPeriodicByName("adv_post_retransmit2");
    ASSERT_FALSE(timerData.is_active);
    adv_post_timers_relaunch_timer_sig_retransmit_to_http_custom();
    ASSERT_TRUE(timerData.is_active);
    ASSERT_FALSE(timerData.flag_timer_triggered);
    ASSERT_EQ(10 * 1000, timerData.period_ticks);

    adv_post_timers_relaunch_timer_sig_retransmit_to_http_custom();
    ASSERT_TRUE(timerData.is_active);
    ASSERT_FALSE(timerData.flag_timer_triggered);
    ASSERT_EQ(10 * 1000, timerData.period_ticks);

    adv2_post_timer_relaunch_with_increased_period();
    ASSERT_TRUE(timerData.is_active);
    ASSERT_FALSE(timerData.flag_timer_triggered);
    ASSERT_EQ(67 * 1000, timerData.period_ticks);

    adv2_post_timer_stop();
    ASSERT_FALSE(timerData.is_active);

    adv2_post_timer_relaunch_with_increased_period();
    ASSERT_TRUE(timerData.is_active);
    ASSERT_FALSE(timerData.flag_timer_triggered);
    ASSERT_EQ(67 * 1000, timerData.period_ticks);

    adv2_post_timer_relaunch_with_increased_period();
    ASSERT_TRUE(timerData.is_active);
    ASSERT_FALSE(timerData.flag_timer_triggered);
    ASSERT_EQ(67 * 1000, timerData.period_ticks);

    adv2_post_timer_set_default_period(25 * 1000);

    adv2_post_timer_relaunch_with_default_period();
    ASSERT_TRUE(timerData.is_active);
    ASSERT_FALSE(timerData.flag_timer_triggered);
    ASSERT_EQ(25 * 1000, timerData.period_ticks);

    adv2_post_timer_relaunch_with_default_period();
    ASSERT_TRUE(timerData.is_active);
    ASSERT_FALSE(timerData.flag_timer_triggered);
    ASSERT_EQ(25 * 1000, timerData.period_ticks);

    adv2_post_timer_set_default_period_by_server_resp(60 * 1000);
    ASSERT_TRUE(timerData.is_active);
    ASSERT_FALSE(timerData.flag_timer_triggered);
    ASSERT_EQ(25 * 1000, timerData.period_ticks);

    adv2_post_timer_set_default_period_by_server_resp(60 * 1000);
    ASSERT_TRUE(timerData.is_active);
    ASSERT_FALSE(timerData.flag_timer_triggered);
    ASSERT_EQ(25 * 1000, timerData.period_ticks);

    adv2_post_timer_relaunch_with_default_period();
    ASSERT_TRUE(timerData.is_active);
    ASSERT_FALSE(timerData.flag_timer_triggered);
    ASSERT_EQ(60 * 1000, timerData.period_ticks);

    adv2_post_timer_stop();
    ASSERT_FALSE(timerData.is_active);
}
