/**
 * @file test_leds_ctrl.cpp
 * @author TheSomeMan
 * @date 2022-10-30
 * @copyright Ruuvi Innovations Ltd, license BSD-3-Clause.
 */

#include "leds_ctrl2.h"
#include "gtest/gtest.h"
#include <string>

using namespace std;

/*** Google-test class implementation
 * *********************************************************************************/

class TestLedsCtrl2;
static TestLedsCtrl2* g_pTestClass;

class TestLedsCtrl2 : public ::testing::Test
{
private:
protected:
    void
    SetUp() override
    {
        g_pTestClass = this;

        leds_ctrl2_init();
    }

    void
    TearDown() override
    {
        g_pTestClass = nullptr;
        leds_ctrl2_deinit();
    }

public:
    TestLedsCtrl2();

    ~TestLedsCtrl2() override;
};

TestLedsCtrl2::TestLedsCtrl2()
    : Test()
{
}

TestLedsCtrl2::~TestLedsCtrl2() = default;

extern "C" {

} // extern "C"

/*** Unit-Tests
 * *******************************************************************************************************/

TEST_F(TestLedsCtrl2, test_one_target) // NOLINT
{
    leds_ctrl2_configure((leds_ctrl_params_t) {
        .flag_polling_mode = false,
        .num_http_targets  = 1,
        .num_mqtt_targets  = 0,
    });

    ASSERT_EQ("G", string(leds_ctrl2_get_new_blinking_sequence().p_sequence));

    leds_ctrl2_handle_event(LEDS_CTRL2_EVENT_WIFI_AP_STARTED);
    ASSERT_EQ("RRRRRRRRRRGGGGGGGGGG", string(leds_ctrl2_get_new_blinking_sequence().p_sequence));

    leds_ctrl2_handle_event(LEDS_CTRL2_EVENT_NETWORK_CONNECTED);
    ASSERT_EQ("RRRRRRRRRRGGGGGGGGGG", string(leds_ctrl2_get_new_blinking_sequence().p_sequence));

    leds_ctrl2_handle_event(LEDS_CTRL2_EVENT_WIFI_AP_STOPPED);
    ASSERT_EQ("G", string(leds_ctrl2_get_new_blinking_sequence().p_sequence));

    leds_ctrl2_handle_event(LEDS_CTRL2_EVENT_NETWORK_DISCONNECTED);
    ASSERT_EQ("R-R-R-R-R-", string(leds_ctrl2_get_new_blinking_sequence().p_sequence));

    leds_ctrl2_handle_event(LEDS_CTRL2_EVENT_NETWORK_CONNECTED);
    ASSERT_EQ("G", string(leds_ctrl2_get_new_blinking_sequence().p_sequence));

    leds_ctrl2_handle_event(LEDS_CTRL2_EVENT_HTTP1_DATA_SENT_FAIL);
    ASSERT_EQ("RRRRR-----", string(leds_ctrl2_get_new_blinking_sequence().p_sequence));

    leds_ctrl2_handle_event(LEDS_CTRL2_EVENT_HTTP1_DATA_SENT_SUCCESSFULLY);
    ASSERT_EQ("G", string(leds_ctrl2_get_new_blinking_sequence().p_sequence));

    leds_ctrl2_handle_event(LEDS_CTRL2_EVENT_RECV_ADV_TIMEOUT);
    ASSERT_EQ("G-G-G-G-G-", string(leds_ctrl2_get_new_blinking_sequence().p_sequence));

    leds_ctrl2_handle_event(LEDS_CTRL2_EVENT_RECV_ADV);
    ASSERT_EQ("G", string(leds_ctrl2_get_new_blinking_sequence().p_sequence));
}

TEST_F(TestLedsCtrl2, test_two_targets) // NOLINT
{
    leds_ctrl2_configure((leds_ctrl_params_t) {
        .flag_polling_mode = false,
        .num_http_targets  = 1,
        .num_mqtt_targets  = 1,
    });

    ASSERT_EQ("G", string(leds_ctrl2_get_new_blinking_sequence().p_sequence));

    leds_ctrl2_handle_event(LEDS_CTRL2_EVENT_WIFI_AP_STARTED);
    ASSERT_EQ("RRRRRRRRRRGGGGGGGGGG", string(leds_ctrl2_get_new_blinking_sequence().p_sequence));

    leds_ctrl2_handle_event(LEDS_CTRL2_EVENT_NETWORK_CONNECTED);
    ASSERT_EQ("RRRRRRRRRRGGGGGGGGGG", string(leds_ctrl2_get_new_blinking_sequence().p_sequence));

    leds_ctrl2_handle_event(LEDS_CTRL2_EVENT_WIFI_AP_STOPPED);
    ASSERT_EQ("G", string(leds_ctrl2_get_new_blinking_sequence().p_sequence));

    leds_ctrl2_handle_event(LEDS_CTRL2_EVENT_NETWORK_DISCONNECTED);
    ASSERT_EQ("R-R-R-R-R-", string(leds_ctrl2_get_new_blinking_sequence().p_sequence));

    leds_ctrl2_handle_event(LEDS_CTRL2_EVENT_NETWORK_CONNECTED);
    ASSERT_EQ("G", string(leds_ctrl2_get_new_blinking_sequence().p_sequence));

    leds_ctrl2_handle_event(LEDS_CTRL2_EVENT_HTTP1_DATA_SENT_FAIL);
    ASSERT_EQ("GGGGGGGGG-", string(leds_ctrl2_get_new_blinking_sequence().p_sequence));

    leds_ctrl2_handle_event(LEDS_CTRL2_EVENT_MQTT1_DISCONNECTED);
    ASSERT_EQ("RRRRR-----", string(leds_ctrl2_get_new_blinking_sequence().p_sequence));

    leds_ctrl2_handle_event(LEDS_CTRL2_EVENT_HTTP1_DATA_SENT_SUCCESSFULLY);
    ASSERT_EQ("GGGGGGGGG-", string(leds_ctrl2_get_new_blinking_sequence().p_sequence));

    leds_ctrl2_handle_event(LEDS_CTRL2_EVENT_MQTT1_CONNECTED);
    ASSERT_EQ("G", string(leds_ctrl2_get_new_blinking_sequence().p_sequence));

    leds_ctrl2_handle_event(LEDS_CTRL2_EVENT_RECV_ADV_TIMEOUT);
    ASSERT_EQ("G-G-G-G-G-", string(leds_ctrl2_get_new_blinking_sequence().p_sequence));

    leds_ctrl2_handle_event(LEDS_CTRL2_EVENT_RECV_ADV);
    ASSERT_EQ("G", string(leds_ctrl2_get_new_blinking_sequence().p_sequence));
}
