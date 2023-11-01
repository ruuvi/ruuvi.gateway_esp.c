/**
 * @file test_adv_mqtt_events.cpp
 * @author TheSomeMan
 * @date 2023-10-30
 * @copyright Ruuvi Innovations Ltd, license BSD-3-Clause.
 */

#include "adv_mqtt_events.h"
#include "adv_mqtt_signals.h"
#include "gtest/gtest.h"
#include <string>
#include <algorithm>
#include "event_mgr.h"

using namespace std;

class TestAdvMqttEvents;

static TestAdvMqttEvents* g_pTestClass;

typedef struct
{
    event_mgr_ev_e event;
    adv_mqtt_sig_e signal;
} EventSignalPair;

/*** Google-test class implementation
 * *********************************************************************************/

class TestAdvMqttEvents : public ::testing::Test
{
private:
protected:
    void
    SetUp() override
    {
        g_pTestClass = this;
    }

    void
    TearDown() override
    {
        g_pTestClass = nullptr;
    }

public:
    TestAdvMqttEvents();

    ~TestAdvMqttEvents() override;

    int          m_signal   = 0;
    os_signal_t* m_p_signal = reinterpret_cast<os_signal_t*>(&m_signal);

    std::vector<EventSignalPair> m_subscribedEvents;
};

TestAdvMqttEvents::TestAdvMqttEvents()
    : Test()
{
}

TestAdvMqttEvents::~TestAdvMqttEvents() = default;

extern "C" {

void
event_mgr_subscribe_sig_static(
    event_mgr_ev_info_static_t* const p_ev_info_mem,
    const event_mgr_ev_e              event,
    os_signal_t* const                p_signal,
    const os_signal_num_e             sig_num)
{
    assert(nullptr != p_ev_info_mem);
    assert(p_signal == g_pTestClass->m_p_signal);
    g_pTestClass->m_subscribedEvents.emplace_back(EventSignalPair { event, (adv_mqtt_sig_e)sig_num });
}

void
event_mgr_unsubscribe_sig_static(event_mgr_ev_info_static_t* const p_ev_info_mem, const event_mgr_ev_e event)
{
    assert(nullptr != p_ev_info_mem);

    auto it = std::remove_if(
        g_pTestClass->m_subscribedEvents.begin(),
        g_pTestClass->m_subscribedEvents.end(),
        [event](const EventSignalPair& pair) { return pair.event == event; });

    g_pTestClass->m_subscribedEvents.erase(it, g_pTestClass->m_subscribedEvents.end());
}

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

} // extern "C"

/*** Unit-Tests
 * *******************************************************************************************************/

TEST_F(TestAdvMqttEvents, test_1) // NOLINT
{
    adv_mqtt_subscribe_events();
    ASSERT_EQ(this->m_subscribedEvents.size(), 7);
    EXPECT_EQ(this->m_subscribedEvents[0].event, EVENT_MGR_EV_RECV_ADV);
    EXPECT_EQ(this->m_subscribedEvents[1].event, EVENT_MGR_EV_MQTT_CONNECTED);
    EXPECT_EQ(this->m_subscribedEvents[2].event, EVENT_MGR_EV_GW_CFG_READY);
    EXPECT_EQ(this->m_subscribedEvents[3].event, EVENT_MGR_EV_GW_CFG_CHANGED_RUUVI);
    EXPECT_EQ(this->m_subscribedEvents[4].event, EVENT_MGR_EV_RELAYING_MODE_CHANGED);
    EXPECT_EQ(this->m_subscribedEvents[5].event, EVENT_MGR_EV_CFG_MODE_ACTIVATED);
    EXPECT_EQ(this->m_subscribedEvents[6].event, EVENT_MGR_EV_CFG_MODE_DEACTIVATED);

    adv_mqtt_unsubscribe_events();
    ASSERT_EQ(this->m_subscribedEvents.size(), 0);
}
