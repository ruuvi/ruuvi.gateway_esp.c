/**
 * @file test_bin2hex.cpp
 * @author TheSomeMan
 * @date 2020-08-27
 * @copyright Ruuvi Innovations Ltd, license BSD-3-Clause.
 */

#include "gtest/gtest.h"
#include "bin2hex.h"
#include <string>

using namespace std;

/*** Google-test class implementation *********************************************************************************/

class TestBin2Hex : public ::testing::Test
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
    TestBin2Hex();

    ~TestBin2Hex() override;
};

TestBin2Hex::TestBin2Hex()
    : Test()
{
}

TestBin2Hex::~TestBin2Hex() = default;

/*** Unit-Tests *******************************************************************************************************/

TEST_F(TestBin2Hex, test_1) // NOLINT
{
    char          hex_str_buf[10];
    const uint8_t bin_buf[1] = { 0x01 };

    bin2hex(hex_str_buf, sizeof(hex_str_buf), bin_buf, sizeof(bin_buf));
    ASSERT_EQ(string("01"), string(hex_str_buf));
}

TEST_F(TestBin2Hex, test_2) // NOLINT
{
    char          hex_str_buf[10];
    const uint8_t bin_buf[2] = { 0x01, 0xAA };

    bin2hex(hex_str_buf, sizeof(hex_str_buf), bin_buf, sizeof(bin_buf));
    ASSERT_EQ(string("01AA"), string(hex_str_buf));
}

TEST_F(TestBin2Hex, test_5_with_overflow_size_9) // NOLINT
{
    char          hex_str_buf[9];
    const uint8_t bin_buf[5] = { 0x01, 0x02, 0x03, 0x04, 0x05 };

    bin2hex(hex_str_buf, sizeof(hex_str_buf), bin_buf, sizeof(bin_buf));
    ASSERT_EQ(string("01020304"), string(hex_str_buf));
}

TEST_F(TestBin2Hex, test_5_with_overflow_size_10) // NOLINT
{
    char          hex_str_buf[10];
    const uint8_t bin_buf[5] = { 0x01, 0x02, 0x03, 0x04, 0x05 };

    bin2hex(hex_str_buf, sizeof(hex_str_buf), bin_buf, sizeof(bin_buf));
    ASSERT_EQ(string("01020304"), string(hex_str_buf));
}

TEST_F(TestBin2Hex, test_5_without_overflow_size_11) // NOLINT
{
    char          hex_str_buf[11];
    const uint8_t bin_buf[5] = { 0x01, 0x02, 0x03, 0x04, 0x05 };

    bin2hex(hex_str_buf, sizeof(hex_str_buf), bin_buf, sizeof(bin_buf));
    ASSERT_EQ(string("0102030405"), string(hex_str_buf));
}
