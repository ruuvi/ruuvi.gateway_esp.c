/**
 * @file test_app_strtol.cpp
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

class TestAppStrtol : public ::testing::Test
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
    TestAppStrtol();

    ~TestAppStrtol() override;
};

TestAppStrtol::TestAppStrtol()
    : Test()
{
}

TestAppStrtol::~TestAppStrtol() = default;

/*** Unit-Tests
 * *******************************************************************************************************/

TEST_F(TestAppStrtol, test_strtol_cptr_123) // NOLINT
{
    const char *  val_str = "123";
    const int32_t result  = app_strtol_cptr(val_str, nullptr, 0);
    ASSERT_EQ(123, result);
}

TEST_F(TestAppStrtol, test_strtol_cptr_minus_123) // NOLINT
{
    const char *  val_str = "-123";
    const int32_t result  = app_strtol_cptr(val_str, nullptr, 0);
    ASSERT_EQ(-123, result);
}

TEST_F(TestAppStrtol, test_strtol_cptr_0x123) // NOLINT
{
    const char *  val_str = "0x123";
    const int32_t result  = app_strtol_cptr(val_str, nullptr, 0);
    ASSERT_EQ(0x123, result);
}

TEST_F(TestAppStrtol, test_strtol_cptr_end) // NOLINT
{
    const char *  val_str = "123abc";
    const char *  end     = nullptr;
    const int32_t result  = app_strtol_cptr(val_str, &end, 0);
    ASSERT_EQ(123, result);
    ASSERT_EQ(&val_str[3], end);
    ASSERT_EQ(string("abc"), string(end));
}

TEST_F(TestAppStrtol, test_strtol_cptr_overflow_32bit) // NOLINT
{
    const char *  val_str = "0x80000001";
    const char *  end     = nullptr;
    const int32_t result  = app_strtol_cptr(val_str, &end, 0);
    ASSERT_EQ(INT32_MAX, result);
    ASSERT_EQ(&val_str[10], end);
    ASSERT_EQ(string(""), string(end));
}

TEST_F(TestAppStrtol, test_strtol_cptr_underflow_32bit) // NOLINT
{
    const char *  val_str = "-0x80000001";
    const char *  end     = nullptr;
    const int32_t result  = app_strtol_cptr(val_str, &end, 0);
    ASSERT_EQ(INT32_MIN, result);
    ASSERT_EQ(&val_str[11], end);
    ASSERT_EQ(string(""), string(end));
}

TEST_F(TestAppStrtol, test_strtol_cptr_overflow_64bit) // NOLINT
{
    const char *  val_str = "0x8000000000000001";
    const char *  end     = nullptr;
    const int32_t result  = app_strtol_cptr(val_str, &end, 0);
    ASSERT_EQ(INT32_MAX, result);
    ASSERT_EQ(&val_str[18], end);
    ASSERT_EQ(string(""), string(end));
}

TEST_F(TestAppStrtol, test_strtol_cptr_undeflow_64bit) // NOLINT
{
    const char *  val_str = "-0x8000000000000001";
    const char *  end     = nullptr;
    const int32_t result  = app_strtol_cptr(val_str, &end, 0);
    ASSERT_EQ(INT32_MIN, result);
    ASSERT_EQ(&val_str[19], end);
    ASSERT_EQ(string(""), string(end));
}

TEST_F(TestAppStrtol, test_strtol_123) // NOLINT
{
    char val_str[80];
    snprintf(val_str, sizeof(val_str), "123");
    const int32_t result = app_strtol(val_str, nullptr, 0);
    ASSERT_EQ(123, result);
}

TEST_F(TestAppStrtol, test_strtol_minus_123) // NOLINT
{
    char val_str[80];
    snprintf(val_str, sizeof(val_str), "-123");
    const int32_t result = app_strtol(val_str, nullptr, 0);
    ASSERT_EQ(-123, result);
}

TEST_F(TestAppStrtol, test_strtol_0x123) // NOLINT
{
    char val_str[80];
    snprintf(val_str, sizeof(val_str), "0x123");
    const int32_t result = app_strtol(val_str, nullptr, 0);
    ASSERT_EQ(0x123, result);
}

TEST_F(TestAppStrtol, test_strtol_end) // NOLINT
{
    char val_str[80];
    snprintf(val_str, sizeof(val_str), "123abc");
    char *        end    = nullptr;
    const int32_t result = app_strtol(val_str, &end, 0);
    ASSERT_EQ(123, result);
    ASSERT_EQ(&val_str[3], end);
    ASSERT_EQ(string("abc"), string(end));
}

TEST_F(TestAppStrtol, test_strtol_overflow_32bit) // NOLINT
{
    char val_str[80];
    snprintf(val_str, sizeof(val_str), "0x80000001");
    char *        end    = nullptr;
    const int32_t result = app_strtol(val_str, &end, 0);
    ASSERT_EQ(INT32_MAX, result);
    ASSERT_EQ(&val_str[10], end);
    ASSERT_EQ(string(""), string(end));
}

TEST_F(TestAppStrtol, test_strtol_underflow_32bit) // NOLINT
{
    char val_str[80];
    snprintf(val_str, sizeof(val_str), "-0x80000001");
    char *        end    = nullptr;
    const int32_t result = app_strtol(val_str, &end, 0);
    ASSERT_EQ(INT32_MIN, result);
    ASSERT_EQ(&val_str[11], end);
    ASSERT_EQ(string(""), string(end));
}

TEST_F(TestAppStrtol, test_strtol_overflow_64bit) // NOLINT
{
    char val_str[80];
    snprintf(val_str, sizeof(val_str), "0x8000000000000001");
    char *        end    = nullptr;
    const int32_t result = app_strtol(val_str, &end, 0);
    ASSERT_EQ(INT32_MAX, result);
    ASSERT_EQ(&val_str[18], end);
    ASSERT_EQ(string(""), string(end));
}

TEST_F(TestAppStrtol, test_strtol_undeflow_64bit) // NOLINT
{
    char val_str[80];
    snprintf(val_str, sizeof(val_str), "-0x8000000000000001");
    char *        end    = nullptr;
    const int32_t result = app_strtol(val_str, &end, 0);
    ASSERT_EQ(INT32_MIN, result);
    ASSERT_EQ(&val_str[19], end);
    ASSERT_EQ(string(""), string(end));
}
