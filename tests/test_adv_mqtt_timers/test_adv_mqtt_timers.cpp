/**
 * @file test_adv_mqtt_timers.cpp
 * @author TheSomeMan
 * @date 2023-10-30
 * @copyright Ruuvi Innovations Ltd, license BSD-3-Clause.
 */

#include "adv_mqtt_timers.h"
#include "gtest/gtest.h"
#include "adv_mqtt_signals.h"
#include <string>
#include <unordered_map>

using namespace std;

class TestAdvMqttTimers;

static TestAdvMqttTimers* g_pTestClass;

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

class TestAdvMqttTimers : public ::testing::Test
{
private:
protected:
    void
    SetUp() override
    {
        g_pTestClass = this;
        adv_mqtt_create_timers();
    }

    void
    TearDown() override
    {
        adv_mqtt_delete_timers();
        assert(0 == this->m_timerMapPeriodic.size());
        assert(0 == this->m_timerMapOneShot.size());
        this->m_timerMapPeriodic.clear();
        this->m_timerMapOneShot.clear();
        g_pTestClass = nullptr;
    }

public:
    TestAdvMqttTimers();

    ~TestAdvMqttTimers() override;

    TimerData&
    findTimerSigPeriodicByName(const string& name);
    TimerData&
    findTimerSigOneShotByName(const string& name);

    int          m_signal   = 0;
    os_signal_t* m_p_signal = reinterpret_cast<os_signal_t*>(&m_signal);
    std::unordered_map<os_timer_sig_periodic_static_t*, TimerData> m_timerMapPeriodic;
    std::unordered_map<os_timer_sig_one_shot_static_t*, TimerData> m_timerMapOneShot;
};

TestAdvMqttTimers::TestAdvMqttTimers()
    : Test()
{
}

TestAdvMqttTimers::~TestAdvMqttTimers() = default;

TimerData&
TestAdvMqttTimers::findTimerSigPeriodicByName(const string& name)
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
TestAdvMqttTimers::findTimerSigOneShotByName(const string& name)
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
adv_mqtt_conv_to_sig_num(const adv_mqtt_sig_e sig)
{
    return (os_signal_num_e)sig;
}

os_signal_t*
adv_mqtt_signals_get(void)
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

} // extern "C"

/*** Unit-Tests
 * *******************************************************************************************************/

TEST_F(TestAdvMqttTimers, test_adv_mqtt_timer_sig_retry_sending_advs) // NOLINT
{
    TimerData& timerData = this->findTimerSigOneShotByName("adv_mqtt:retry");
    ASSERT_FALSE(timerData.is_active);
    adv_mqtt_timers_start_timer_sig_retry_sending_advs();
    ASSERT_TRUE(timerData.is_active);
    ASSERT_FALSE(timerData.flag_timer_triggered);
    ASSERT_EQ(50, timerData.period_ticks);

    timerData.flag_timer_triggered = false;
}

TEST_F(TestAdvMqttTimers, test_timer_watchdog_feed) // NOLINT
{
    TimerData& timerData = this->findTimerSigPeriodicByName("adv_mqtt:wdog");
    ASSERT_FALSE(timerData.is_active);
    adv_mqtt_timers_start_timer_sig_watchdog_feed();
    ASSERT_TRUE(timerData.is_active);
    ASSERT_FALSE(timerData.flag_timer_triggered);
    ASSERT_EQ(1000, timerData.period_ticks);
}
