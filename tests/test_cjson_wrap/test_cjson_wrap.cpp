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
    ASSERT_TRUE(cjson_wrap_add_timestamp(root, "timestamp", 1));
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
    ASSERT_TRUE(cjson_wrap_add_timestamp(root, "timestamp", 12345678));
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
    ASSERT_TRUE(cjson_wrap_add_timestamp(root, "timestamp", 0x7FFFFFFF));
    char *json_str = cJSON_PrintUnformatted(root);
    ASSERT_NE(nullptr, json_str);
    ASSERT_EQ(string("{\"timestamp\":\"2147483647\"}"), string(json_str));
    cJSON_Delete(root);
    free(json_str);
}

TEST_F(TestCJsonWrap, test_delete) // NOLINT
{
    cJSON *root = cJSON_CreateObject();
    ASSERT_NE(nullptr, root);
    cjson_wrap_delete(&root);
    ASSERT_EQ(nullptr, root);
}

TEST_F(TestCJsonWrap, test_print) // NOLINT
{
    cJSON *root = cJSON_CreateObject();
    ASSERT_NE(nullptr, root);
    ASSERT_TRUE(cjson_wrap_add_timestamp(root, "timestamp", 1));
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
    ASSERT_TRUE(cjson_wrap_add_timestamp(root, "timestamp", 1));
    cjson_wrap_str_t json_str = cjson_wrap_print_and_delete(&root);
    ASSERT_NE(nullptr, json_str.p_str);
    ASSERT_EQ(nullptr, root);
    ASSERT_EQ(string("{\n\t\"timestamp\":\t\"1\"\n}"), string(json_str.p_str));
    cjson_wrap_free_json_str(&json_str);
    ASSERT_EQ(nullptr, json_str.p_str);
}

TEST_F(TestCJsonWrap, test_copy_string_val) // NOLINT
{
    cJSON *root = cJSON_CreateObject();
    ASSERT_NE(nullptr, root);
    cJSON_AddStringToObject(root, "attr", "value123");
    char buf[80];
    ASSERT_TRUE(json_wrap_copy_string_val(root, "attr", buf, sizeof(buf)));
    ASSERT_EQ(string("value123"), string(buf));
    cJSON_Delete(root);
}

TEST_F(TestCJsonWrap, test_copy_string_val_with_buf_overflow) // NOLINT
{
    cJSON *root = cJSON_CreateObject();
    ASSERT_NE(nullptr, root);
    cJSON_AddStringToObject(root, "attr", "value123");
    char buf[4];
    ASSERT_TRUE(json_wrap_copy_string_val(root, "attr", buf, sizeof(buf)));
    ASSERT_EQ(string("val"), string(buf));
    cJSON_Delete(root);
}

TEST_F(TestCJsonWrap, test_copy_string_val_not_exist) // NOLINT
{
    cJSON *root = cJSON_CreateObject();
    ASSERT_NE(nullptr, root);
    cJSON_AddStringToObject(root, "attr", "value123");
    char buf[80];
    ASSERT_FALSE(json_wrap_copy_string_val(root, "attr2", buf, sizeof(buf)));
    cJSON_Delete(root);
}

TEST_F(TestCJsonWrap, test_copy_string_val_wrong_type) // NOLINT
{
    cJSON *root = cJSON_CreateObject();
    ASSERT_NE(nullptr, root);
    cJSON_AddBoolToObject(root, "attr", 1);
    char buf[80];
    ASSERT_FALSE(json_wrap_copy_string_val(root, "attr", buf, sizeof(buf)));
    cJSON_Delete(root);
}

TEST_F(TestCJsonWrap, test_get_bool_val_true) // NOLINT
{
    cJSON *root = cJSON_CreateObject();
    ASSERT_NE(nullptr, root);
    cJSON_AddBoolToObject(root, "attr", true);
    bool val = false;
    ASSERT_TRUE(json_wrap_get_bool_val(root, "attr", &val));
    ASSERT_TRUE(val);
    cJSON_Delete(root);
}

TEST_F(TestCJsonWrap, test_get_bool_val_false) // NOLINT
{
    cJSON *root = cJSON_CreateObject();
    ASSERT_NE(nullptr, root);
    cJSON_AddBoolToObject(root, "attr", false);
    bool val = false;
    ASSERT_TRUE(json_wrap_get_bool_val(root, "attr", &val));
    ASSERT_FALSE(val);
    cJSON_Delete(root);
}

TEST_F(TestCJsonWrap, test_get_bool_val_not_exist) // NOLINT
{
    cJSON *root = cJSON_CreateObject();
    ASSERT_NE(nullptr, root);
    cJSON_AddBoolToObject(root, "attr", false);
    bool val = false;
    ASSERT_FALSE(json_wrap_get_bool_val(root, "attr2", &val));
    cJSON_Delete(root);
}

TEST_F(TestCJsonWrap, test_get_bool_val_wrong_type) // NOLINT
{
    cJSON *root = cJSON_CreateObject();
    ASSERT_NE(nullptr, root);
    cJSON_AddNumberToObject(root, "attr", 123.0);
    bool val = false;
    ASSERT_FALSE(json_wrap_get_bool_val(root, "attr", &val));
    cJSON_Delete(root);
}

TEST_F(TestCJsonWrap, test_get_uint16_val_0) // NOLINT
{
    cJSON *root = cJSON_CreateObject();
    ASSERT_NE(nullptr, root);
    cJSON_AddNumberToObject(root, "attr", 0.0);
    uint16_t val = 0;
    ASSERT_TRUE(json_wrap_get_uint16_val(root, "attr", &val));
    ASSERT_EQ(0, val);
    cJSON_Delete(root);
}

TEST_F(TestCJsonWrap, test_get_uint16_val_1) // NOLINT
{
    cJSON *root = cJSON_CreateObject();
    ASSERT_NE(nullptr, root);
    cJSON_AddNumberToObject(root, "attr", 1.0);
    uint16_t val = 0;
    ASSERT_TRUE(json_wrap_get_uint16_val(root, "attr", &val));
    ASSERT_EQ(1, val);
    cJSON_Delete(root);
}

TEST_F(TestCJsonWrap, test_get_uint16_val_65535) // NOLINT
{
    cJSON *root = cJSON_CreateObject();
    ASSERT_NE(nullptr, root);
    cJSON_AddNumberToObject(root, "attr", 65535.0);
    uint16_t val = 0;
    ASSERT_TRUE(json_wrap_get_uint16_val(root, "attr", &val));
    ASSERT_EQ(65535, val);
    cJSON_Delete(root);
}

TEST_F(TestCJsonWrap, test_get_uint16_val_minus_1) // NOLINT
{
    cJSON *root = cJSON_CreateObject();
    ASSERT_NE(nullptr, root);
    cJSON_AddNumberToObject(root, "attr", -1.0);
    uint16_t val = 0;
    ASSERT_FALSE(json_wrap_get_uint16_val(root, "attr", &val));
    cJSON_Delete(root);
}

TEST_F(TestCJsonWrap, test_get_uint16_val_65536) // NOLINT
{
    cJSON *root = cJSON_CreateObject();
    ASSERT_NE(nullptr, root);
    cJSON_AddNumberToObject(root, "attr", 65536.0);
    uint16_t val = 0;
    ASSERT_FALSE(json_wrap_get_uint16_val(root, "attr", &val));
    cJSON_Delete(root);
}

TEST_F(TestCJsonWrap, test_get_uint16_val_wrong_type) // NOLINT
{
    cJSON *root = cJSON_CreateObject();
    ASSERT_NE(nullptr, root);
    cJSON_AddStringToObject(root, "attr", "1");
    uint16_t val = 0;
    ASSERT_FALSE(json_wrap_get_uint16_val(root, "attr", &val));
    cJSON_Delete(root);
}

TEST_F(TestCJsonWrap, test_get_uint16_val_not_found) // NOLINT
{
    cJSON *root = cJSON_CreateObject();
    ASSERT_NE(nullptr, root);
    cJSON_AddNumberToObject(root, "attr", 0);
    uint16_t val = 0;
    ASSERT_FALSE(json_wrap_get_uint16_val(root, "attr2", &val));
    cJSON_Delete(root);
}
