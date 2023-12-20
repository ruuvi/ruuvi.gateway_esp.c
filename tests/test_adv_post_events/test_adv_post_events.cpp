/**
 * @file test_adv_post_events.cpp
 * @author TheSomeMan
 * @date 2023-09-17
 * @copyright Ruuvi Innovations Ltd, license BSD-3-Clause.
 */

#include "adv_post_events.h"
#include "adv_post_signals.h"
#include "gtest/gtest.h"
#include <string>
#include <algorithm>
#include "event_mgr.h"

using namespace std;

class TestAdvPostEvents;

static TestAdvPostEvents* g_pTestClass;

typedef struct
{
    event_mgr_ev_e event;
    adv_post_sig_e signal;
} EventSignalPair;

/*** Google-test class implementation
 * *********************************************************************************/

class TestAdvPostEvents : public ::testing::Test
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
    TestAdvPostEvents();

    ~TestAdvPostEvents() override;

    int          m_signal   = 0;
    os_signal_t* m_p_signal = reinterpret_cast<os_signal_t*>(&m_signal);

    std::vector<EventSignalPair> m_subscribedEvents;
};

TestAdvPostEvents::TestAdvPostEvents()
    : Test()
{
}

TestAdvPostEvents::~TestAdvPostEvents() = default;

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
    g_pTestClass->m_subscribedEvents.emplace_back(EventSignalPair { event, (adv_post_sig_e)sig_num });
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
adv_post_conv_to_sig_num(const adv_post_sig_e sig)
{
    return (os_signal_num_e)sig;
}

os_signal_t*
adv_post_signals_get(void)
{
    return g_pTestClass->m_p_signal;
}

} // extern "C"

/*** Unit-Tests
 * *******************************************************************************************************/

TEST_F(TestAdvPostEvents, test_1) // NOLINT
{
    adv_post_subscribe_events();
    ASSERT_EQ(this->m_subscribedEvents.size(), 12);
    EXPECT_EQ(this->m_subscribedEvents[0].event, EVENT_MGR_EV_WIFI_DISCONNECTED);
    EXPECT_EQ(this->m_subscribedEvents[1].event, EVENT_MGR_EV_ETH_DISCONNECTED);
    EXPECT_EQ(this->m_subscribedEvents[2].event, EVENT_MGR_EV_WIFI_CONNECTED);
    EXPECT_EQ(this->m_subscribedEvents[3].event, EVENT_MGR_EV_ETH_CONNECTED);
    EXPECT_EQ(this->m_subscribedEvents[4].event, EVENT_MGR_EV_TIME_SYNCHRONIZED);
    EXPECT_EQ(this->m_subscribedEvents[5].event, EVENT_MGR_EV_GW_CFG_READY);
    EXPECT_EQ(this->m_subscribedEvents[6].event, EVENT_MGR_EV_GW_CFG_CHANGED_RUUVI);
    EXPECT_EQ(this->m_subscribedEvents[7].event, EVENT_MGR_EV_RELAYING_MODE_CHANGED);
    EXPECT_EQ(this->m_subscribedEvents[8].event, EVENT_MGR_EV_GREEN_LED_STATE_CHANGED);
    EXPECT_EQ(this->m_subscribedEvents[9].event, EVENT_MGR_EV_CFG_MODE_ACTIVATED);
    EXPECT_EQ(this->m_subscribedEvents[10].event, EVENT_MGR_EV_CFG_MODE_DEACTIVATED);
    EXPECT_EQ(this->m_subscribedEvents[11].event, EVENT_MGR_EV_CFG_BLE_SCAN_CHANGED);

    adv_post_unsubscribe_events();
    ASSERT_EQ(this->m_subscribedEvents.size(), 0);
}
