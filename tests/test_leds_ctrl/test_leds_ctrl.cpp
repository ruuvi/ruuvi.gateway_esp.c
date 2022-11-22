/**
 * @file test_leds_ctrl.cpp
 * @author TheSomeMan
 * @date 2022-10-30
 * @copyright Ruuvi Innovations Ltd, license BSD-3-Clause.
 */

#include "leds_ctrl.h"
#include "leds_ctrl2.h"
#include "gtest/gtest.h"
#include <string>
#include "os_task.h"

using namespace std;

/*** Google-test class implementation
 * *********************************************************************************/

class TestLedsCtrl;
static TestLedsCtrl* g_pTestClass;

const leds_ctrl_callbacks_t g_cb_null = {
    .cb_on_enter_state_after_reboot  = nullptr,
    .cb_on_exit_state_after_reboot   = nullptr,
    .cb_on_enter_state_before_reboot = nullptr,
};

class TestLedsCtrl : public ::testing::Test
{
private:
protected:
    void
    SetUp() override
    {
    }

    void
    TearDown() override
    {
        g_pTestClass = nullptr;
        leds_ctrl_deinit();
    }

public:
    TestLedsCtrl();

    ~TestLedsCtrl() override;
};

TestLedsCtrl::TestLedsCtrl()
    : Test()
{
}

TestLedsCtrl::~TestLedsCtrl() = default;

extern "C" {

os_task_handle_t
os_task_get_cur_task_handle(void)
{
    static int x = 0;
    return reinterpret_cast<os_task_handle_t>(&x);
}

} // extern "C"

/*** Unit-Tests
 * *******************************************************************************************************/

TEST_F(TestLedsCtrl, test_erase_cfg_on_boot) // NOLINT
{
    leds_ctrl_init(true, g_cb_null);

    const leds_ctrl_params_t params = {
        .flag_polling_mode = false,
        .num_http_targets  = 1,
        .num_mqtt_targets  = 0,
    };
    leds_ctrl_configure_sub_machine(params);

    ASSERT_EQ("-", string(leds_ctrl_get_new_blinking_sequence().p_sequence));

    leds_ctrl_handle_event(LEDS_CTRL_EVENT_CFG_ERASED);
    ASSERT_EQ("RR--RR--", string(leds_ctrl_get_new_blinking_sequence().p_sequence));

    leds_ctrl_handle_event(LEDS_CTRL_EVENT_REBOOT);
    ASSERT_EQ("-", string(leds_ctrl_get_new_blinking_sequence().p_sequence));
}

TEST_F(TestLedsCtrl, test_nrf52_fw_updating) // NOLINT
{
    leds_ctrl_init(false, g_cb_null);

    const leds_ctrl_params_t params = {
        .flag_polling_mode = false,
        .num_http_targets  = 1,
        .num_mqtt_targets  = 0,
    };
    leds_ctrl_configure_sub_machine(params);

    ASSERT_EQ("R-R-R-R-", string(leds_ctrl_get_new_blinking_sequence().p_sequence));

    leds_ctrl_handle_event(LEDS_CTRL_EVENT_NRF52_FW_CHECK);
    ASSERT_EQ("-", string(leds_ctrl_get_new_blinking_sequence().p_sequence));

    leds_ctrl_handle_event(LEDS_CTRL_EVENT_NRF52_FW_UPDATING);
    ASSERT_EQ("R---------", string(leds_ctrl_get_new_blinking_sequence().p_sequence));

    leds_ctrl_handle_event(LEDS_CTRL_EVENT_REBOOT);
    ASSERT_EQ("-", string(leds_ctrl_get_new_blinking_sequence().p_sequence));
}

TEST_F(TestLedsCtrl, test_nrf52_failure_while_fw_updating) // NOLINT
{
    leds_ctrl_init(false, g_cb_null);

    const leds_ctrl_params_t params = {
        .flag_polling_mode = false,
        .num_http_targets  = 1,
        .num_mqtt_targets  = 0,
    };
    leds_ctrl_configure_sub_machine(params);

    ASSERT_EQ("R-R-R-R-", string(leds_ctrl_get_new_blinking_sequence().p_sequence));

    leds_ctrl_handle_event(LEDS_CTRL_EVENT_NRF52_FW_CHECK);
    ASSERT_EQ("-", string(leds_ctrl_get_new_blinking_sequence().p_sequence));

    leds_ctrl_handle_event(LEDS_CTRL_EVENT_NRF52_FW_UPDATING);
    ASSERT_EQ("R---------", string(leds_ctrl_get_new_blinking_sequence().p_sequence));

    leds_ctrl_handle_event(LEDS_CTRL_EVENT_NRF52_FAILURE);
    ASSERT_EQ("R", string(leds_ctrl_get_new_blinking_sequence().p_sequence));

    leds_ctrl_handle_event(LEDS_CTRL_EVENT_REBOOT);
    ASSERT_EQ("-", string(leds_ctrl_get_new_blinking_sequence().p_sequence));
}

TEST_F(TestLedsCtrl, test_nrf52_failure_while_fw_checking) // NOLINT
{
    leds_ctrl_init(false, g_cb_null);

    const leds_ctrl_params_t params = {
        .flag_polling_mode = false,
        .num_http_targets  = 1,
        .num_mqtt_targets  = 0,
    };
    leds_ctrl_configure_sub_machine(params);

    ASSERT_EQ("R-R-R-R-", string(leds_ctrl_get_new_blinking_sequence().p_sequence));

    leds_ctrl_handle_event(LEDS_CTRL_EVENT_NRF52_FW_CHECK);
    ASSERT_EQ("-", string(leds_ctrl_get_new_blinking_sequence().p_sequence));

    leds_ctrl_handle_event(LEDS_CTRL_EVENT_NRF52_FAILURE);
    ASSERT_EQ("R", string(leds_ctrl_get_new_blinking_sequence().p_sequence));

    leds_ctrl_handle_event(LEDS_CTRL_EVENT_REBOOT);
    ASSERT_EQ("-", string(leds_ctrl_get_new_blinking_sequence().p_sequence));
}

TEST_F(TestLedsCtrl, test_regular_boot) // NOLINT
{
    leds_ctrl_init(false, g_cb_null);

    const leds_ctrl_params_t params = {
        .flag_polling_mode = false,
        .num_http_targets  = 1,
        .num_mqtt_targets  = 0,
    };
    leds_ctrl_configure_sub_machine(params);

    ASSERT_EQ("R-R-R-R-", string(leds_ctrl_get_new_blinking_sequence().p_sequence));

    leds_ctrl_handle_event(LEDS_CTRL_EVENT_NRF52_FW_CHECK);
    ASSERT_EQ("-", string(leds_ctrl_get_new_blinking_sequence().p_sequence));

    leds_ctrl_handle_event(LEDS_CTRL_EVENT_NRF52_READY);
    ASSERT_EQ("-", string(leds_ctrl_get_new_blinking_sequence().p_sequence));

    leds_ctrl_handle_event(LEDS_CTRL_EVENT_CFG_READY);
    ASSERT_EQ("G", string(leds_ctrl_get_new_blinking_sequence().p_sequence));

    leds_ctrl2_handle_event(LEDS_CTRL2_EVENT_NETWORK_CONNECTED);
    ASSERT_EQ("G", string(leds_ctrl2_get_new_blinking_sequence().p_sequence));

    leds_ctrl2_handle_event(LEDS_CTRL2_EVENT_HTTP1_DATA_SENT_SUCCESSFULLY);
    ASSERT_EQ("G", string(leds_ctrl2_get_new_blinking_sequence().p_sequence));

    leds_ctrl2_handle_event(LEDS_CTRL2_EVENT_RECV_ADV);
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

    leds_ctrl_handle_event(LEDS_CTRL_EVENT_REBOOT);
    ASSERT_EQ("-", string(leds_ctrl_get_new_blinking_sequence().p_sequence));
}
