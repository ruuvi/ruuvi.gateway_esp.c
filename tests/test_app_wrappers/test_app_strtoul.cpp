/**
 * @file test_app_strtoul.cpp
 * @author TheSomeMan
 * @date 2020-10-03
 * @copyright Ruuvi Innovations Ltd, license BSD-3-Clause.
 */

#include "app_wrappers.h"
#include "gtest/gtest.h"
#include <string>

using namespace std;

/*** Google-test class implementation
 * *********************************************************************************/

class TestAppStrtoul : public ::testing::Test
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
    TestAppStrtoul();

    ~TestAppStrtoul() override;
};

TestAppStrtoul::TestAppStrtoul()
    : Test()
{
}

TestAppStrtoul::~TestAppStrtoul() = default;

/*** Unit-Tests
 * *******************************************************************************************************/

TEST_F(TestAppStrtoul, test_strtoul_cptr_123) // NOLINT
{
    const char* val_str = "123";
    const uint32_t result = app_strtoul_cptr(val_str, nullptr, 0);
    ASSERT_EQ(123, result);
}

TEST_F(TestAppStrtoul, test_strtoul_cptr_0x123) // NOLINT
{
    const char* val_str = "0x123";
    const uint32_t result = app_strtoul_cptr(val_str, nullptr, 0);
    ASSERT_EQ(0x123, result);
}

TEST_F(TestAppStrtoul, test_strtoul_cptr_end) // NOLINT
{
    const char* val_str = "123abc";
    const char* end = nullptr;
    const uint32_t result = app_strtoul_cptr(val_str, &end, 0);
    ASSERT_EQ(123, result);
    ASSERT_EQ(&val_str[3], end);
    ASSERT_EQ(string("abc"), string(end));
}

TEST_F(TestAppStrtoul, test_strtoul_cptr_overflow_32bit) // NOLINT
{
    const char* val_str = "0x123456789";
    const char* end = nullptr;
    const uint32_t result = app_strtoul_cptr(val_str, &end, 0);
    ASSERT_EQ(UINT32_MAX, result);
    ASSERT_EQ(&val_str[11], end);
    ASSERT_EQ(string(""), string(end));
}

TEST_F(TestAppStrtoul, test_strtoul_cptr_overflow_64bit) // NOLINT
{
    const char* val_str = "0x12345678123456789";
    const char* end = nullptr;
    const uint32_t result = app_strtoul_cptr(val_str, &end, 0);
    ASSERT_EQ(UINT32_MAX, result);
    ASSERT_EQ(&val_str[19], end);
    ASSERT_EQ(string(""), string(end));
}

TEST_F(TestAppStrtoul, test_strtoul_123) // NOLINT
{
    char val_str[80];
    snprintf(val_str, sizeof(val_str), "123");
    const uint32_t result = app_strtoul(val_str, nullptr, 0);
    ASSERT_EQ(123, result);
}

TEST_F(TestAppStrtoul, test_strtoul_0x123) // NOLINT
{
    char val_str[80];
    snprintf(val_str, sizeof(val_str), "0x123");
    const uint32_t result = app_strtoul(val_str, nullptr, 0);
    ASSERT_EQ(0x123, result);
}

TEST_F(TestAppStrtoul, test_strtoul_end) // NOLINT
{
    char val_str[80];
    snprintf(val_str, sizeof(val_str), "123abc");
    char* end = nullptr;
    const uint32_t result = app_strtoul(val_str, &end, 0);
    ASSERT_EQ(123, result);
    ASSERT_EQ(&val_str[3], end);
    ASSERT_EQ(string("abc"), string(end));
}

TEST_F(TestAppStrtoul, test_strtoul_overflow_32bit) // NOLINT
{
    char val_str[80];
    snprintf(val_str, sizeof(val_str), "0x123456789");
    char* end = nullptr;
    const uint32_t result = app_strtoul(val_str, &end, 0);
    ASSERT_EQ(UINT32_MAX, result);
    ASSERT_EQ(&val_str[11], end);
    ASSERT_EQ(string(""), string(end));
}

TEST_F(TestAppStrtoul, test_strtoul_overflow_64bit) // NOLINT
{
    char val_str[80];
    snprintf(val_str, sizeof(val_str), "0x12345678123456789");
    char* end = nullptr;
    const uint32_t result = app_strtoul(val_str, &end, 0);
    ASSERT_EQ(UINT32_MAX, result);
    ASSERT_EQ(&val_str[19], end);
    ASSERT_EQ(string(""), string(end));
}




