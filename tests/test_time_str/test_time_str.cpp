/**
 * @file test_time_str.cpp
 * @author TheSomeMan
 * @date 2021-08-01
 * @copyright Ruuvi Innovations Ltd, license BSD-3-Clause.
 */

#include "time_str.h"
#include "gtest/gtest.h"
#include <string>

using namespace std;

/*** Google-test class implementation
 * *********************************************************************************/

class TestTimeStr : public ::testing::Test
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
    TestTimeStr();

    ~TestTimeStr() override;
};

TestTimeStr::TestTimeStr()
    : Test()
{
}

TestTimeStr::~TestTimeStr() = default;

/*** Unit-Tests
 * *******************************************************************************************************/

TEST_F(TestTimeStr, time_str_conv_to_tm_YYYY_MM_DDThh_mm_ss_mmm_plus_00_00) // NOLINT
{
    struct tm tm_time = {};
    uint16_t  ms      = 0;
    ASSERT_TRUE(time_str_conv_to_tm("2021-07-19T01:28:10.096+00:00", &tm_time, &ms));
    ASSERT_EQ(2021 - 1900, tm_time.tm_year);
    ASSERT_EQ(7 - 1, tm_time.tm_mon);
    ASSERT_EQ(19, tm_time.tm_mday);
    ASSERT_EQ(1, tm_time.tm_hour);
    ASSERT_EQ(28, tm_time.tm_min);
    ASSERT_EQ(10, tm_time.tm_sec);
    ASSERT_EQ(96, ms);
    ASSERT_TRUE(time_str_conv_to_tm("2021-07-19T14:08:10.096+02:00", &tm_time, nullptr));
}

TEST_F(TestTimeStr, time_str_conv_to_tm_YYYY_MM_DDThh_mm_ss_mmm_plus_02_30) // NOLINT
{
    struct tm tm_time = {};
    uint16_t  ms      = 0;
    ASSERT_TRUE(time_str_conv_to_tm("2021-07-19T01:28:10.096+02:30", &tm_time, &ms));
    ASSERT_EQ(2021 - 1900, tm_time.tm_year);
    ASSERT_EQ(7 - 1, tm_time.tm_mon);
    ASSERT_EQ(19 - 1, tm_time.tm_mday);
    ASSERT_EQ(24 + 1 - 3, tm_time.tm_hour);
    ASSERT_EQ(60 + 28 - 30, tm_time.tm_min);
    ASSERT_EQ(10, tm_time.tm_sec);
    ASSERT_EQ(96, ms);
    ASSERT_TRUE(time_str_conv_to_tm("2021-07-19T14:08:10.096+02:30", &tm_time, nullptr));
}

TEST_F(TestTimeStr, time_str_conv_to_tm_YYYY_MM_DDThh_mm_ss_mmm_minux_03_30) // NOLINT
{
    struct tm tm_time = {};
    uint16_t  ms      = 0;
    ASSERT_TRUE(time_str_conv_to_tm("2021-07-18T23:35:10.096-03:30", &tm_time, &ms));
    ASSERT_EQ(2021 - 1900, tm_time.tm_year);
    ASSERT_EQ(7 - 1, tm_time.tm_mon);
    ASSERT_EQ(18 + 1, tm_time.tm_mday);
    ASSERT_EQ(23 + 4 - 24, tm_time.tm_hour);
    ASSERT_EQ(35 + 30 - 60, tm_time.tm_min);
    ASSERT_EQ(10, tm_time.tm_sec);
    ASSERT_EQ(96, ms);
    ASSERT_TRUE(time_str_conv_to_tm("2021-07-18T23:35:10.096-03:30", &tm_time, nullptr));
}

TEST_F(TestTimeStr, time_str_conv_to_tm_YYYY_MM_DDThh_mm_ssZ) // NOLINT
{
    struct tm tm_time = {};
    uint16_t  ms      = 0;
    ASSERT_TRUE(time_str_conv_to_tm("2021-07-19T01:28:10Z", &tm_time, &ms));
    ASSERT_EQ(2021 - 1900, tm_time.tm_year);
    ASSERT_EQ(7 - 1, tm_time.tm_mon);
    ASSERT_EQ(19, tm_time.tm_mday);
    ASSERT_EQ(1, tm_time.tm_hour);
    ASSERT_EQ(28, tm_time.tm_min);
    ASSERT_EQ(10, tm_time.tm_sec);
    ASSERT_EQ(0, ms);
    ASSERT_TRUE(time_str_conv_to_tm("2021-07-19T14:08:10Z", &tm_time, nullptr));
}

TEST_F(TestTimeStr, time_str_conv_to_tm_YYYY_MM_DDThh_mm_ss) // NOLINT
{
    struct tm tm_time = {};
    uint16_t  ms      = 0;
    ASSERT_TRUE(time_str_conv_to_tm("2021-07-19T01:28:10", &tm_time, &ms));
    ASSERT_EQ(2021 - 1900, tm_time.tm_year);
    ASSERT_EQ(7 - 1, tm_time.tm_mon);
    ASSERT_EQ(19, tm_time.tm_mday);
    ASSERT_EQ(1, tm_time.tm_hour);
    ASSERT_EQ(28, tm_time.tm_min);
    ASSERT_EQ(10, tm_time.tm_sec);
    ASSERT_EQ(0, ms);
    ASSERT_TRUE(time_str_conv_to_tm("2021-07-19T14:08:10", &tm_time, nullptr));
}

TEST_F(TestTimeStr, time_str_conv_to_tm_YYYY_MM_DDThh_mm) // NOLINT
{
    struct tm tm_time = {};
    uint16_t  ms      = 0;
    ASSERT_TRUE(time_str_conv_to_tm("2021-07-19T01:28", &tm_time, &ms));
    ASSERT_EQ(2021 - 1900, tm_time.tm_year);
    ASSERT_EQ(7 - 1, tm_time.tm_mon);
    ASSERT_EQ(19, tm_time.tm_mday);
    ASSERT_EQ(1, tm_time.tm_hour);
    ASSERT_EQ(28, tm_time.tm_min);
    ASSERT_EQ(0, tm_time.tm_sec);
    ASSERT_EQ(0, ms);
    ASSERT_TRUE(time_str_conv_to_tm("2021-07-19T14:08", &tm_time, nullptr));
}

TEST_F(TestTimeStr, time_str_conv_to_tm_YYYY_MM_DDThh) // NOLINT
{
    struct tm tm_time = {};
    uint16_t  ms      = 0;
    ASSERT_TRUE(time_str_conv_to_tm("2021-07-19T01", &tm_time, &ms));
    ASSERT_EQ(2021 - 1900, tm_time.tm_year);
    ASSERT_EQ(7 - 1, tm_time.tm_mon);
    ASSERT_EQ(19, tm_time.tm_mday);
    ASSERT_EQ(1, tm_time.tm_hour);
    ASSERT_EQ(0, tm_time.tm_min);
    ASSERT_EQ(0, tm_time.tm_sec);
    ASSERT_EQ(0, ms);
    ASSERT_TRUE(time_str_conv_to_tm("2021-07-19T14", &tm_time, nullptr));
}

TEST_F(TestTimeStr, time_str_conv_to_tm_YYYY_MM_DD) // NOLINT
{
    struct tm tm_time = {};
    uint16_t  ms      = 0;
    ASSERT_TRUE(time_str_conv_to_tm("2021-07-19", &tm_time, &ms));
    ASSERT_EQ(2021 - 1900, tm_time.tm_year);
    ASSERT_EQ(7 - 1, tm_time.tm_mon);
    ASSERT_EQ(19, tm_time.tm_mday);
    ASSERT_EQ(0, tm_time.tm_hour);
    ASSERT_EQ(0, tm_time.tm_min);
    ASSERT_EQ(0, tm_time.tm_sec);
    ASSERT_EQ(0, ms);
    ASSERT_TRUE(time_str_conv_to_tm("2021-07-19", &tm_time, nullptr));
}

TEST_F(TestTimeStr, time_str_conv_to_tm_YYYY_MM) // NOLINT
{
    struct tm tm_time = {};
    uint16_t  ms      = 0;
    ASSERT_TRUE(time_str_conv_to_tm("2021-07", &tm_time, &ms));
    ASSERT_EQ(2021 - 1900, tm_time.tm_year);
    ASSERT_EQ(7 - 1, tm_time.tm_mon);
    ASSERT_EQ(1, tm_time.tm_mday);
    ASSERT_EQ(0, tm_time.tm_hour);
    ASSERT_EQ(0, tm_time.tm_min);
    ASSERT_EQ(0, tm_time.tm_sec);
    ASSERT_EQ(0, ms);
    ASSERT_TRUE(time_str_conv_to_tm("2021-07", &tm_time, nullptr));
}

TEST_F(TestTimeStr, time_str_conv_to_tm_YYYY) // NOLINT
{
    struct tm tm_time = {};
    uint16_t  ms      = 0;
    ASSERT_TRUE(time_str_conv_to_tm("2021", &tm_time, &ms));
    ASSERT_EQ(2021 - 1900, tm_time.tm_year);
    ASSERT_EQ(0, tm_time.tm_mon);
    ASSERT_EQ(1, tm_time.tm_mday);
    ASSERT_EQ(0, tm_time.tm_hour);
    ASSERT_EQ(0, tm_time.tm_min);
    ASSERT_EQ(0, tm_time.tm_sec);
    ASSERT_EQ(0, ms);
    ASSERT_TRUE(time_str_conv_to_tm("2021", &tm_time, nullptr));
}

TEST_F(TestTimeStr, time_str_conv_to_tm_YYYYMMDDhhmmss) // NOLINT
{
    struct tm tm_time = {};
    uint16_t  ms      = 0;
    ASSERT_TRUE(time_str_conv_to_tm("20210719012810", &tm_time, &ms));
    ASSERT_EQ(2021 - 1900, tm_time.tm_year);
    ASSERT_EQ(7 - 1, tm_time.tm_mon);
    ASSERT_EQ(19, tm_time.tm_mday);
    ASSERT_EQ(1, tm_time.tm_hour);
    ASSERT_EQ(28, tm_time.tm_min);
    ASSERT_EQ(10, tm_time.tm_sec);
    ASSERT_EQ(0, ms);
    ASSERT_TRUE(time_str_conv_to_tm("20210719012810", &tm_time, nullptr));
}

TEST_F(TestTimeStr, time_str_conv_to_tm_YYYYMMDDhhmmss_plus_0230) // NOLINT
{
    struct tm tm_time = {};
    uint16_t  ms      = 0;
    ASSERT_TRUE(time_str_conv_to_tm("20210719012810+0230", &tm_time, &ms));
    ASSERT_EQ(2021 - 1900, tm_time.tm_year);
    ASSERT_EQ(7 - 1, tm_time.tm_mon);
    ASSERT_EQ(18, tm_time.tm_mday);
    ASSERT_EQ(22, tm_time.tm_hour);
    ASSERT_EQ(58, tm_time.tm_min);
    ASSERT_EQ(10, tm_time.tm_sec);
    ASSERT_EQ(0, ms);
    ASSERT_TRUE(time_str_conv_to_tm("20210719012810+0230", &tm_time, nullptr));
}

TEST_F(TestTimeStr, time_str_conv_YYYYMMDDhhmmss_plus_02) // NOLINT
{
    struct tm tm_time = {};
    uint16_t  ms      = 0;
    ASSERT_TRUE(time_str_conv_to_tm("20210719012810+02", &tm_time, &ms));
    ASSERT_EQ(2021 - 1900, tm_time.tm_year);
    ASSERT_EQ(7 - 1, tm_time.tm_mon);
    ASSERT_EQ(18, tm_time.tm_mday);
    ASSERT_EQ(23, tm_time.tm_hour);
    ASSERT_EQ(28, tm_time.tm_min);
    ASSERT_EQ(10, tm_time.tm_sec);
    ASSERT_EQ(0, ms);
    ASSERT_TRUE(time_str_conv_to_tm("20210719012810+02", &tm_time, nullptr));
    ASSERT_EQ(1626650890, time_str_conv_to_unix_time("20210719012810+02"));
}
