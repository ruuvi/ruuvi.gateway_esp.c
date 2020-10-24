/**
 * @file test_mac_addr.cpp
 * @author TheSomeMan
 * @date 2020-10-23
 * @copyright Ruuvi Innovations Ltd, license BSD-3-Clause.
 */

#include "cjson_wrap.h"
#include "gtest/gtest.h"
#include <string>

using namespace std;

/*** Google-test class implementation
 * *********************************************************************************/

class TestCJsonWrap : public ::testing::Test
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
    TestCJsonWrap();

    ~TestCJsonWrap() override;
};

TestCJsonWrap::TestCJsonWrap()
    : Test()
{
}

TestCJsonWrap::~TestCJsonWrap() = default;

/*** Unit-Tests
 * *******************************************************************************************************/

TEST_F(TestCJsonWrap, test_1) // NOLINT
{
    cJSON *root = cJSON_CreateObject();
    ASSERT_NE(nullptr, root);
    cjson_wrap_add_timestamp(root, "timestamp", 1);
    char *json_str = cJSON_PrintUnformatted(root);
    ASSERT_NE(nullptr, json_str);
    ASSERT_EQ(string("{\"timestamp\":\"1\"}"), string(json_str));
    cJSON_Delete(root);
    free(json_str);
}

TEST_F(TestCJsonWrap, test_12345678) // NOLINT
{
    cJSON *root = cJSON_CreateObject();
    ASSERT_NE(nullptr, root);
    cjson_wrap_add_timestamp(root, "timestamp", 12345678);
    char *json_str = cJSON_PrintUnformatted(root);
    ASSERT_NE(nullptr, json_str);
    ASSERT_EQ(string("{\"timestamp\":\"12345678\"}"), string(json_str));
    cJSON_Delete(root);
    free(json_str);
}

TEST_F(TestCJsonWrap, test_0x7FFFFFFF) // NOLINT
{
    cJSON *root = cJSON_CreateObject();
    ASSERT_NE(nullptr, root);
    cjson_wrap_add_timestamp(root, "timestamp", 0x7FFFFFFF);
    char *json_str = cJSON_PrintUnformatted(root);
    ASSERT_NE(nullptr, json_str);
    ASSERT_EQ(string("{\"timestamp\":\"2147483647\"}"), string(json_str));
    cJSON_Delete(root);
    free(json_str);
}
