/**
 * @file test_adv_post_green_led.cpp
 * @author TheSomeMan
 * @date 2023-09-17
 * @copyright Ruuvi Innovations Ltd, license BSD-3-Clause.
 */

#include "adv_post_green_led.h"
#include "gtest/gtest.h"
#include <string>
#include "os_timer_sig.h"
#include "os_mutex.h"
#include "os_task.h"
#include "adv_post_internal.h"

using namespace std;

class TestAdvPostGreenLed;

static TestAdvPostGreenLed* g_pTestClass;

/*** Google-test class implementation
 * *********************************************************************************/

class TestAdvPostGreenLed : public ::testing::Test
{
private:
protected:
    void
    SetUp() override
    {
        g_pTestClass                      = this;
        this->m_is_timer_active           = false;
        this->m_led_ctrl_time_interval_ms = -1;
        this->m_api_send_led_ctrl_res     = 0;
        adv_post_green_led_init();
    }

    void
    TearDown() override
    {
        g_pTestClass = nullptr;
    }

public:
    TestAdvPostGreenLed();

    ~TestAdvPostGreenLed() override;

    int                      m_timer_stub;
    os_timer_sig_periodic_t* m_p_timer_sig_green_led_update = reinterpret_cast<os_timer_sig_periodic_t*>(&m_timer_stub);
    bool                     m_is_timer_active;
    int32_t                  m_led_ctrl_time_interval_ms;
    int8_t                   m_api_send_led_ctrl_res;
};

TestAdvPostGreenLed::TestAdvPostGreenLed()
    : Test()
{
}

TestAdvPostGreenLed::~TestAdvPostGreenLed() = default;

extern "C" {

os_timer_sig_periodic_t*
adv_post_timers_get_timer_sig_green_led_update(void)
{
    return g_pTestClass->m_p_timer_sig_green_led_update;
}

void
os_timer_sig_periodic_start(os_timer_sig_periodic_t* const p_obj)
{
    assert(p_obj == g_pTestClass->m_p_timer_sig_green_led_update);
    g_pTestClass->m_is_timer_active = true;
}

void
os_timer_sig_periodic_relaunch(os_timer_sig_periodic_t* const p_obj, bool flag_restart_from_current_moment)
{
    assert(p_obj == g_pTestClass->m_p_timer_sig_green_led_update);
    g_pTestClass->m_is_timer_active = true;
}

bool
os_timer_sig_periodic_is_active(os_timer_sig_periodic_t* const p_obj)
{
    assert(p_obj == g_pTestClass->m_p_timer_sig_green_led_update);
    return g_pTestClass->m_is_timer_active;
}

void
os_timer_sig_periodic_stop(os_timer_sig_periodic_t* const p_obj)
{
    assert(p_obj == g_pTestClass->m_p_timer_sig_green_led_update);
    g_pTestClass->m_is_timer_active = false;
}

os_mutex_t
os_mutex_create_static(os_mutex_static_t* const p_mutex_static)
{
    return reinterpret_cast<os_mutex_t>(p_mutex_static);
}

void
os_mutex_lock(os_mutex_t const h_mutex)
{
}

void
os_mutex_unlock(os_mutex_t const h_mutex)
{
}

os_task_handle_t
os_task_get_cur_task_handle(void)
{
    return nullptr;
}

void
adv_post_nrf52_send_led_ctrl(const uint16_t time_interval_ms)
{
    g_pTestClass->m_led_ctrl_time_interval_ms = time_interval_ms;
}

} // extern "C"

/*** Unit-Tests
 * *******************************************************************************************************/

TEST_F(TestAdvPostGreenLed, test_1) // NOLINT
{
    this->m_led_ctrl_time_interval_ms = -1;
    adv_post_on_green_led_update(ADV_POST_GREEN_LED_CMD_UPDATE);
    ASSERT_TRUE(this->m_is_timer_active);
    ASSERT_EQ(0, this->m_led_ctrl_time_interval_ms);

    this->m_led_ctrl_time_interval_ms = -1;
    adv_post_on_green_led_update(ADV_POST_GREEN_LED_CMD_ON);
    ASSERT_EQ(ADV_POST_GREEN_LED_ON_INTERVAL_MS, this->m_led_ctrl_time_interval_ms);

    this->m_led_ctrl_time_interval_ms = -1;
    adv_post_on_green_led_update(ADV_POST_GREEN_LED_CMD_UPDATE);
    ASSERT_EQ(ADV_POST_GREEN_LED_ON_INTERVAL_MS, this->m_led_ctrl_time_interval_ms);

    this->m_led_ctrl_time_interval_ms = -1;
    adv_post_on_green_led_update(ADV_POST_GREEN_LED_CMD_OFF);
    ASSERT_EQ(0, this->m_led_ctrl_time_interval_ms);

    this->m_led_ctrl_time_interval_ms = -1;
    adv_post_on_green_led_update(ADV_POST_GREEN_LED_CMD_UPDATE);
    ASSERT_EQ(0, this->m_led_ctrl_time_interval_ms);

    adv_post_on_green_led_update((adv_post_green_led_cmd_e)-1);
    ASSERT_EQ(0, this->m_led_ctrl_time_interval_ms);
}
