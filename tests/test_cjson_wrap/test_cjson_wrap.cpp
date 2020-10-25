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

TEST_F(TestCJsonWrap, test_add_timestamp_1) // NOLINT
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

TEST_F(TestCJsonWrap, test_add_timestamp_12345678) // NOLINT
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

TEST_F(TestCJsonWrap, test_add_timestamp_0x7FFFFFFF) // NOLINT
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

TEST_F(TestCJsonWrap, test_print) // NOLINT
{
    cJSON *root = cJSON_CreateObject();
    ASSERT_NE(nullptr, root);
    cjson_wrap_add_timestamp(root, "timestamp", 1);
    cjson_wrap_str_t json_str = cjson_wrap_print(root);
    ASSERT_NE(nullptr, json_str.p_str);
    ASSERT_EQ(string("{\n\t\"timestamp\":\t\"1\"\n}"), string(json_str.p_str));
    cJSON_Delete(root);
    cjson_wrap_free_json_str(&json_str);
    ASSERT_EQ(nullptr, json_str.p_str);
}

TEST_F(TestCJsonWrap, test_print_and_delete) // NOLINT
{
    cJSON *root = cJSON_CreateObject();
    ASSERT_NE(nullptr, root);
    cjson_wrap_add_timestamp(root, "timestamp", 1);
    cjson_wrap_str_t json_str = cjson_wrap_print_and_delete(&root);
    ASSERT_NE(nullptr, json_str.p_str);
    ASSERT_EQ(nullptr, root);
    ASSERT_EQ(string("{\n\t\"timestamp\":\t\"1\"\n}"), string(json_str.p_str));
    cjson_wrap_free_json_str(&json_str);
    ASSERT_EQ(nullptr, json_str.p_str);
}
