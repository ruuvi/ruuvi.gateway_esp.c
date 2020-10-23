/**
 * @file test_time_units.cpp
 * @author TheSomeMan
 * @date 2020-10-23
 * @copyright Ruuvi Innovations Ltd, license BSD-3-Clause.
 */

#include "time_units.h"
#include "gtest/gtest.h"
#include <string>

using namespace std;

/*** Google-test class implementation
 * *********************************************************************************/

class TestTimeUnits : public ::testing::Test
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
    }

public:
    TestTimeUnits();

    ~TestTimeUnits() override;
};

TestTimeUnits::TestTimeUnits()
    : Test()
{
}

TestTimeUnits::~TestTimeUnits() = default;

/*** Unit-Tests
 * *******************************************************************************************************/

TEST_F(TestTimeUnits, test_time_units_conv_seconds_to_ms_0) // NOLINT
{
    ASSERT_EQ(0, time_units_conv_seconds_to_ms(0));
}

TEST_F(TestTimeUnits, test_time_units_conv_seconds_to_ms_1) // NOLINT
{
    ASSERT_EQ(1000, time_units_conv_seconds_to_ms(1));
}

TEST_F(TestTimeUnits, test_time_units_conv_seconds_to_ms_max) // NOLINT
{
    ASSERT_EQ(4294967000, time_units_conv_seconds_to_ms(0xFFFFFFFFU / 1000U));
}

TEST_F(TestTimeUnits, test_time_units_conv_seconds_to_ms_max_minus_one) // NOLINT
{
    ASSERT_EQ(4294966000, time_units_conv_seconds_to_ms(0xFFFFFFFFU / 1000U - 1));
}

TEST_F(TestTimeUnits, test_time_units_conv_seconds_to_ms_overflow32) // NOLINT
{
    ASSERT_LT(time_units_conv_seconds_to_ms(0xFFFFFFFFU / 1000U + 1), 1000);
}

TEST_F(TestTimeUnits, test_time_units_conv_ms_to_us_0) // NOLINT
{
    ASSERT_EQ(0, time_units_conv_ms_to_us(0));
}

TEST_F(TestTimeUnits, test_time_units_conv_ms_to_us_1) // NOLINT
{
    ASSERT_EQ(1000, time_units_conv_ms_to_us(1));
}

TEST_F(TestTimeUnits, test_time_units_conv_ms_to_us_0xFFFFFFFFU) // NOLINT
{
    ASSERT_EQ(4294967000, time_units_conv_ms_to_us(0xFFFFFFFFU / 1000U));
}

TEST_F(TestTimeUnits, test_time_units_conv_ms_to_us_overflow32) // NOLINT
{
    ASSERT_EQ(4294968000, time_units_conv_ms_to_us(0xFFFFFFFFU / 1000U + 1));
}

TEST_F(TestTimeUnits, test_time_units_conv_ms_to_us_max) // NOLINT
{
    ASSERT_EQ(0xFFFFFFFFLLU * 1000U, time_units_conv_ms_to_us(0xFFFFFFFFU));
}
