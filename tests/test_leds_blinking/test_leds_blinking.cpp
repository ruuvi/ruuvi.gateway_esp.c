/**
 * @file test_leds_blinking.cpp
 * @author TheSomeMan
 * @date 2022-10-30
 * @copyright Ruuvi Innovations Ltd, license BSD-3-Clause.
 */

#include "leds_blinking.h"
#include "gtest/gtest.h"
#include <string>
#include "os_task.h"

using namespace std;

/*** Google-test class implementation
 * *********************************************************************************/

class TestLedsBlinking;
static TestLedsBlinking* g_pTestClass;

class TestLedsBlinking : public ::testing::Test
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
        leds_blinking_deinit();
    }

public:
    TestLedsBlinking();

    ~TestLedsBlinking() override;
};

TestLedsBlinking::TestLedsBlinking()
    : Test()
{
}

TestLedsBlinking::~TestLedsBlinking() = default;

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

TEST_F(TestLedsBlinking, test_1) // NOLINT
{
    leds_blinking_init((leds_blinking_mode_t) { .p_sequence = "RG-" });

    ASSERT_EQ(LEDS_COLOR_RED, leds_blinking_get_next());
    ASSERT_FALSE(leds_blinking_is_sequence_finished());
    ASSERT_EQ(LEDS_COLOR_GREEN, leds_blinking_get_next());
    ASSERT_FALSE(leds_blinking_is_sequence_finished());
    ASSERT_EQ(LEDS_COLOR_OFF, leds_blinking_get_next());
    ASSERT_TRUE(leds_blinking_is_sequence_finished());
    ASSERT_EQ(LEDS_COLOR_RED, leds_blinking_get_next());
    ASSERT_FALSE(leds_blinking_is_sequence_finished());
    ASSERT_EQ(LEDS_COLOR_GREEN, leds_blinking_get_next());
    ASSERT_FALSE(leds_blinking_is_sequence_finished());
    ASSERT_EQ(LEDS_COLOR_OFF, leds_blinking_get_next());
    ASSERT_TRUE(leds_blinking_is_sequence_finished());

    leds_blinking_set_new_sequence((leds_blinking_mode_t) { .p_sequence = "RRGG--" });
    ASSERT_EQ(LEDS_COLOR_RED, leds_blinking_get_next());
    ASSERT_FALSE(leds_blinking_is_sequence_finished());
    ASSERT_EQ(LEDS_COLOR_RED, leds_blinking_get_next());
    ASSERT_FALSE(leds_blinking_is_sequence_finished());
    ASSERT_EQ(LEDS_COLOR_GREEN, leds_blinking_get_next());
    ASSERT_FALSE(leds_blinking_is_sequence_finished());
    ASSERT_EQ(LEDS_COLOR_GREEN, leds_blinking_get_next());
    ASSERT_FALSE(leds_blinking_is_sequence_finished());
    ASSERT_EQ(LEDS_COLOR_OFF, leds_blinking_get_next());
    ASSERT_FALSE(leds_blinking_is_sequence_finished());
    ASSERT_EQ(LEDS_COLOR_OFF, leds_blinking_get_next());
    ASSERT_TRUE(leds_blinking_is_sequence_finished());
}
