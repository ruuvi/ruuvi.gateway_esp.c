/**
 * @file test_json.cpp
 * @author TheSomeMan
 * @date 2020-08-23
 * @copyright Ruuvi Innovations Ltd, license BSD-3-Clause.
 */

#include "gtest/gtest.h"
#include "json_helper.h"
#include <string>

using namespace std;

/*** Google-test class implementation *********************************************************************************/

class TestJsonHelper : public ::testing::Test
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
    TestJsonHelper();

    ~TestJsonHelper() override;
};

TestJsonHelper::TestJsonHelper()
    : Test()
{
}

TestJsonHelper::~TestJsonHelper()
{
}

static string
json_helper_get_by_key_wrapper(const char *const p_json, const char *const p_key)
{
    str_buf_t str_buf = json_helper_get_by_key(p_json, p_key);
    if (nullptr == str_buf.buf)
    {
        return string {};
    }
    string val(str_buf.buf);
    str_buf_free_buf(&str_buf);
    return val;
}

/*** Unit-Tests *******************************************************************************************************/

TEST_F(TestJsonHelper, test_json_get_by_key_with_eol) // NOLINT
{
    const char *const p_json
        = "{\n"
          "\"node_id\": \"MDc6UmVsZWFzZTQ2NDE0Nzcy\",\n"
          "\"tag_name\": \"v1.6.0\",\n"
          "\"created_at\": \"2021-07-19T11:38:50Z\",\n"
          "\"published_at\": \"2021-07-19T14:08:10Z\"\n"
          "}";
    ASSERT_EQ(string("MDc6UmVsZWFzZTQ2NDE0Nzcy"), json_helper_get_by_key_wrapper(p_json, "node_id"));
    ASSERT_EQ(string("v1.6.0"), json_helper_get_by_key_wrapper(p_json, "tag_name"));
    ASSERT_EQ(string("2021-07-19T11:38:50Z"), json_helper_get_by_key_wrapper(p_json, "created_at"));
    ASSERT_EQ(string("2021-07-19T14:08:10Z"), json_helper_get_by_key_wrapper(p_json, "published_at"));
}

TEST_F(TestJsonHelper, test_json_get_by_key_without_eol) // NOLINT
{
    const char *const p_json
        = "{"
          "\"node_id\": \"MDc6UmVsZWFzZTQ2NDE0Nzcy\","
          "\"tag_name\": \"v1.6.0\","
          "\"created_at\": \"2021-07-19T11:38:50Z\","
          "\"published_at\": \"2021-07-19T14:08:10Z\""
          "}";
    ASSERT_EQ(string("MDc6UmVsZWFzZTQ2NDE0Nzcy"), json_helper_get_by_key_wrapper(p_json, "node_id"));
    ASSERT_EQ(string("v1.6.0"), json_helper_get_by_key_wrapper(p_json, "tag_name"));
    ASSERT_EQ(string("2021-07-19T11:38:50Z"), json_helper_get_by_key_wrapper(p_json, "created_at"));
    ASSERT_EQ(string("2021-07-19T14:08:10Z"), json_helper_get_by_key_wrapper(p_json, "published_at"));
}

TEST_F(TestJsonHelper, test_json_get_by_key_with_eol_unquoted_numbers) // NOLINT
{
    const char *const p_json
        = "{\n"
          "\"node_id\": 1,\n"
          "\"tag_name\": 1.5,\n"
          "\"created_at\": true,\n"
          "\"published_at\": false\n"
          "}";
    ASSERT_EQ(string("1"), json_helper_get_by_key_wrapper(p_json, "node_id"));
    ASSERT_EQ(string("1.5"), json_helper_get_by_key_wrapper(p_json, "tag_name"));
    ASSERT_EQ(string("true"), json_helper_get_by_key_wrapper(p_json, "created_at"));
    ASSERT_EQ(string("false"), json_helper_get_by_key_wrapper(p_json, "published_at"));
}

TEST_F(TestJsonHelper, test_json_get_by_key_without_eol_unquoted_numbers) // NOLINT
{
    const char *const p_json
        = "{"
          "\"node_id\": 1,"
          "\"tag_name\": 1.5,"
          "\"created_at\": true,"
          "\"published_at\": false"
          "}";
    ASSERT_EQ(string("1"), json_helper_get_by_key_wrapper(p_json, "node_id"));
    ASSERT_EQ(string("1.5"), json_helper_get_by_key_wrapper(p_json, "tag_name"));
    ASSERT_EQ(string("true"), json_helper_get_by_key_wrapper(p_json, "created_at"));
    ASSERT_EQ(string("false"), json_helper_get_by_key_wrapper(p_json, "published_at"));
}

TEST_F(TestJsonHelper, test_json_get_by_key_error) // NOLINT
{
    ASSERT_EQ(string("ABC"), json_helper_get_by_key_wrapper(R"({"node_id": "ABC"})", "node_id"));
    ASSERT_EQ(string(""), json_helper_get_by_key_wrapper(R"({"node_id": "ABC})", "node_id"));
    ASSERT_EQ(string(""), json_helper_get_by_key_wrapper(R"({"node_id": "ABC\"})", "node_id"));
    ASSERT_EQ(string(""), json_helper_get_by_key_wrapper(R"({"node_id": \"ABC"})", "node_id"));
    ASSERT_EQ(string(""), json_helper_get_by_key_wrapper(R"({"node_id": ABC"})", "node_id"));
    ASSERT_EQ(string(""), json_helper_get_by_key_wrapper(R"({"node_id": ABC})", "node_id"));
    ASSERT_EQ(string(""), json_helper_get_by_key_wrapper(R"({"node_id": ""})", "node_id"));
    ASSERT_EQ(string(""), json_helper_get_by_key_wrapper(R"({"node_id": \"})", "node_id"));
    ASSERT_EQ(string(""), json_helper_get_by_key_wrapper(R"({"node_id": \"\"})", "node_id"));
    ASSERT_EQ(string(""), json_helper_get_by_key_wrapper(R"({"node_id: "ABC"})", "node_id"));
    ASSERT_EQ(string(""), json_helper_get_by_key_wrapper(R"({"node_id\": "ABC"})", "node_id"));
    ASSERT_EQ(string(""), json_helper_get_by_key_wrapper(R"({node_id": "ABC"})", "node_id"));
    ASSERT_EQ(string(""), json_helper_get_by_key_wrapper(R"({\"node_id": "ABC"})", "node_id"));
}

TEST_F(TestJsonHelper, test_json_get_by_key_null) // NOLINT
{
    ASSERT_EQ(string("null"), json_helper_get_by_key_wrapper(R"({"node_id": "null"})", "node_id"));
    ASSERT_EQ(string("null"), json_helper_get_by_key_wrapper(R"({"node_id": null})", "node_id"));
}

TEST_F(TestJsonHelper, test_json_get_by_key_true) // NOLINT
{
    ASSERT_EQ(string("true"), json_helper_get_by_key_wrapper(R"({"node_id": "true"})", "node_id"));
    ASSERT_EQ(string("true"), json_helper_get_by_key_wrapper(R"({"node_id": true})", "node_id"));
}

TEST_F(TestJsonHelper, test_json_get_by_key_false) // NOLINT
{
    ASSERT_EQ(string("false"), json_helper_get_by_key_wrapper(R"({"node_id": "false"})", "node_id"));
    ASSERT_EQ(string("false"), json_helper_get_by_key_wrapper(R"({"node_id": false})", "node_id"));
}
