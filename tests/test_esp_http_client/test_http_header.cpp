/**
 * @file test_http_header.cpp
 * @author TheSomeMan
 * @date 2026-04-11
 * @copyright Ruuvi Innovations Ltd, license BSD-3-Clause.
 */

#include "http_header.h"
#include "gtest/gtest.h"
#include <string>
#include <cassert>
#include <complex>

using namespace std;

class TestHttpHeader : public ::testing::Test
{
private:
protected:
    void
    SetUp() override
    {
        this->m_header = http_header_init();
        assert(this->m_header != nullptr);
    }

    void
    TearDown() override
    {
        if (this->m_header)
        {
            const esp_err_t err = http_header_destroy(this->m_header);
            this->m_header      = nullptr;
            assert(err == ESP_OK);
        }
    }

public:
    TestHttpHeader();

    ~TestHttpHeader() override;

    http_header_handle_t m_header;
};

TestHttpHeader::TestHttpHeader()
    : Test()
    , m_header {}
{
}

TestHttpHeader::~TestHttpHeader() = default;

/*** Unit-Tests
 * *******************************************************************************************************/

TEST_F(TestHttpHeader, test_empty_list) // NOLINT
{
    http_header_generate_state_t state = {
        .stage = HTTP_HEADER_GENERATE_STAGE_INIT,
        .item_idx = 0,
        .item_state = {
            .stage = HTTP_HEADER_GENERATE_ITEM_STAGE_INIT,
            .offset = 0,
        },
    };
    ASSERT_EQ(0, http_header_count(this->m_header));
    char buffer[64];
    memset(buffer, 'a', sizeof(buffer));
    buffer[sizeof(buffer) - 1]   = '\0';
    int          buffer_len      = sizeof(buffer);
    const string expected_header = "\r\n";

    const size_t ret = http_header_generate_string(this->m_header, &state, buffer, buffer_len);
    ASSERT_EQ(expected_header, string(buffer));
    ASSERT_EQ(ret, expected_header.length());

    memset(buffer, 'a', sizeof(buffer));
    buffer[sizeof(buffer) - 1] = '\0';
    buffer_len                 = sizeof(buffer);
    ASSERT_EQ(0, http_header_generate_string(this->m_header, &state, buffer, buffer_len));
    ASSERT_EQ(string(""), string(buffer));
}

TEST_F(TestHttpHeader, test_one_item) // NOLINT
{
    http_header_generate_state_t state = {
        .stage = HTTP_HEADER_GENERATE_STAGE_INIT,
        .item_idx = 0,
        .item_state = {
            .stage = HTTP_HEADER_GENERATE_ITEM_STAGE_INIT,
            .offset = 0,
        },
    };
    char buffer[64];

    ASSERT_EQ(0, http_header_count(this->m_header));
    http_header_set(this->m_header, "key1", "value1");
    ASSERT_EQ(1, http_header_count(this->m_header));

    memset(buffer, 'a', sizeof(buffer));
    buffer[sizeof(buffer) - 1]   = '\0';
    int          buffer_len      = sizeof(buffer);
    const string expected_header = "key1: value1\r\n\r\n";

    const size_t ret = http_header_generate_string(this->m_header, &state, buffer, buffer_len);
    ASSERT_EQ(expected_header, string(buffer));
    ASSERT_EQ(ret, expected_header.length());

    memset(buffer, 'a', sizeof(buffer));
    buffer[sizeof(buffer) - 1] = '\0';
    buffer_len                 = sizeof(buffer);
    ASSERT_EQ(0, http_header_generate_string(this->m_header, &state, buffer, buffer_len));
    ASSERT_EQ("", string(buffer));
}

TEST_F(TestHttpHeader, test_one_item_empty_buffer) // NOLINT
{
    http_header_generate_state_t state = {
        .stage = HTTP_HEADER_GENERATE_STAGE_INIT,
        .item_idx = 0,
        .item_state = {
            .stage = HTTP_HEADER_GENERATE_ITEM_STAGE_INIT,
            .offset = 0,
        },
    };
    char buffer[16];

    ASSERT_EQ(0, http_header_count(this->m_header));
    http_header_set(this->m_header, "key1", "val1");
    ASSERT_EQ(1, http_header_count(this->m_header));

    memset(buffer, 'a', sizeof(buffer));
    buffer[sizeof(buffer) - 1] = '\0';
    int buffer_len             = 0;

    ASSERT_EQ(0, http_header_generate_string(this->m_header, &state, buffer, buffer_len));
    ASSERT_EQ("aaaaaaaaaaaaaaa", string(buffer));
}

TEST_F(TestHttpHeader, test_one_item_buffer_overflow) // NOLINT
{
    http_header_generate_state_t state = {
        .stage = HTTP_HEADER_GENERATE_STAGE_INIT,
        .item_idx = 0,
        .item_state = {
            .stage = HTTP_HEADER_GENERATE_ITEM_STAGE_INIT,
            .offset = 0,
        },
    };
    char buffer[16];

    ASSERT_EQ(0, http_header_count(this->m_header));
    http_header_set(this->m_header, "key1", "value1");
    ASSERT_EQ(1, http_header_count(this->m_header));

    memset(buffer, 'a', sizeof(buffer));
    buffer[sizeof(buffer) - 1]    = '\0';
    int          buffer_len       = sizeof(buffer);
    const string expected_header1 = "key1: value1\r\n\r";

    size_t ret = http_header_generate_string(this->m_header, &state, buffer, buffer_len);
    ASSERT_EQ(expected_header1, string(buffer));
    ASSERT_EQ(ret, expected_header1.length());

    memset(buffer, 'a', sizeof(buffer));
    buffer[sizeof(buffer) - 1]    = '\0';
    buffer_len                    = sizeof(buffer);
    const string expected_header2 = "\n";

    ret = http_header_generate_string(this->m_header, &state, buffer, buffer_len);
    ASSERT_EQ(expected_header2, string(buffer));
    ASSERT_EQ(ret, expected_header2.length());

    memset(buffer, 'a', sizeof(buffer));
    buffer[sizeof(buffer) - 1] = '\0';
    buffer_len                 = sizeof(buffer);
    ASSERT_EQ(0, http_header_generate_string(this->m_header, &state, buffer, buffer_len));
    ASSERT_EQ("", string(buffer));
}

TEST_F(TestHttpHeader, test_one_item_split_key) // NOLINT
{
    http_header_generate_state_t state = {
        .stage = HTTP_HEADER_GENERATE_STAGE_INIT,
        .item_idx = 0,
        .item_state = {
            .stage = HTTP_HEADER_GENERATE_ITEM_STAGE_INIT,
            .offset = 0,
        },
    };
    char buffer[10];

    ASSERT_EQ(0, http_header_count(this->m_header));
    http_header_set(this->m_header, "key_abcdef", "v1");
    ASSERT_EQ(1, http_header_count(this->m_header));

    memset(buffer, 'a', sizeof(buffer));
    buffer[sizeof(buffer) - 1]    = '\0';
    int          buffer_len       = sizeof(buffer);
    const string expected_header1 = "key_abcde";

    size_t ret = http_header_generate_string(this->m_header, &state, buffer, buffer_len);
    ASSERT_EQ(expected_header1, string(buffer));
    ASSERT_EQ(ret, expected_header1.length());

    memset(buffer, 'a', sizeof(buffer));
    buffer[sizeof(buffer) - 1]    = '\0';
    buffer_len                    = sizeof(buffer);
    const string expected_header2 = "f: v1\r\n\r\n";

    ret = http_header_generate_string(this->m_header, &state, buffer, buffer_len);
    ASSERT_EQ(expected_header2, string(buffer));
    ASSERT_EQ(ret, expected_header2.length());

    memset(buffer, 'a', sizeof(buffer));
    buffer[sizeof(buffer) - 1] = '\0';
    buffer_len                 = sizeof(buffer);
    ASSERT_EQ(0, http_header_generate_string(this->m_header, &state, buffer, buffer_len));
    ASSERT_EQ("", string(buffer));
}

TEST_F(TestHttpHeader, test_one_item_split_after_key) // NOLINT
{
    http_header_generate_state_t state = {
        .stage = HTTP_HEADER_GENERATE_STAGE_INIT,
        .item_idx = 0,
        .item_state = {
            .stage = HTTP_HEADER_GENERATE_ITEM_STAGE_INIT,
            .offset = 0,
        },
    };
    char buffer[11];

    ASSERT_EQ(0, http_header_count(this->m_header));
    http_header_set(this->m_header, "key_abcdef", "v1");
    ASSERT_EQ(1, http_header_count(this->m_header));

    memset(buffer, 'a', sizeof(buffer));
    buffer[sizeof(buffer) - 1]    = '\0';
    int          buffer_len       = sizeof(buffer);
    const string expected_header1 = "key_abcdef";

    size_t ret = http_header_generate_string(this->m_header, &state, buffer, buffer_len);
    ASSERT_EQ(expected_header1, string(buffer));
    ASSERT_EQ(ret, expected_header1.length());

    memset(buffer, 'a', sizeof(buffer));
    buffer[sizeof(buffer) - 1]    = '\0';
    buffer_len                    = sizeof(buffer);
    const string expected_header2 = ": v1\r\n\r\n";

    ret = http_header_generate_string(this->m_header, &state, buffer, buffer_len);
    ASSERT_EQ(expected_header2, string(buffer));
    ASSERT_EQ(ret, expected_header2.length());

    memset(buffer, 'a', sizeof(buffer));
    buffer[sizeof(buffer) - 1] = '\0';
    buffer_len                 = sizeof(buffer);
    ASSERT_EQ(0, http_header_generate_string(this->m_header, &state, buffer, buffer_len));
    ASSERT_EQ("", string(buffer));
}

TEST_F(TestHttpHeader, test_one_item_split_separator) // NOLINT
{
    http_header_generate_state_t state = {
        .stage = HTTP_HEADER_GENERATE_STAGE_INIT,
        .item_idx = 0,
        .item_state = {
            .stage = HTTP_HEADER_GENERATE_ITEM_STAGE_INIT,
            .offset = 0,
        },
    };
    char buffer[12];

    ASSERT_EQ(0, http_header_count(this->m_header));
    http_header_set(this->m_header, "key_abcdef", "v1");
    ASSERT_EQ(1, http_header_count(this->m_header));

    memset(buffer, 'a', sizeof(buffer));
    buffer[sizeof(buffer) - 1]    = '\0';
    int          buffer_len       = sizeof(buffer);
    const string expected_header1 = "key_abcdef:";

    size_t ret = http_header_generate_string(this->m_header, &state, buffer, buffer_len);
    ASSERT_EQ(expected_header1, string(buffer));
    ASSERT_EQ(ret, expected_header1.length());

    memset(buffer, 'a', sizeof(buffer));
    buffer[sizeof(buffer) - 1]    = '\0';
    buffer_len                    = sizeof(buffer);
    const string expected_header2 = " v1\r\n\r\n";

    ret = http_header_generate_string(this->m_header, &state, buffer, buffer_len);
    ASSERT_EQ(expected_header2, string(buffer));
    ASSERT_EQ(ret, expected_header2.length());

    memset(buffer, 'a', sizeof(buffer));
    buffer[sizeof(buffer) - 1] = '\0';
    buffer_len                 = sizeof(buffer);
    ASSERT_EQ(0, http_header_generate_string(this->m_header, &state, buffer, buffer_len));
    ASSERT_EQ("", string(buffer));
}

TEST_F(TestHttpHeader, test_one_item_split_after_separator) // NOLINT
{
    http_header_generate_state_t state = {
        .stage = HTTP_HEADER_GENERATE_STAGE_INIT,
        .item_idx = 0,
        .item_state = {
            .stage = HTTP_HEADER_GENERATE_ITEM_STAGE_INIT,
            .offset = 0,
        },
    };
    char buffer[13];

    ASSERT_EQ(0, http_header_count(this->m_header));
    http_header_set(this->m_header, "key_abcdef", "v1");
    ASSERT_EQ(1, http_header_count(this->m_header));

    memset(buffer, 'a', sizeof(buffer));
    buffer[sizeof(buffer) - 1]    = '\0';
    int          buffer_len       = sizeof(buffer);
    const string expected_header1 = "key_abcdef: ";

    size_t ret = http_header_generate_string(this->m_header, &state, buffer, buffer_len);
    ASSERT_EQ(expected_header1, string(buffer));
    ASSERT_EQ(ret, expected_header1.length());

    memset(buffer, 'a', sizeof(buffer));
    buffer[sizeof(buffer) - 1]    = '\0';
    buffer_len                    = sizeof(buffer);
    const string expected_header2 = "v1\r\n\r\n";

    ret = http_header_generate_string(this->m_header, &state, buffer, buffer_len);
    ASSERT_EQ(expected_header2, string(buffer));
    ASSERT_EQ(ret, expected_header2.length());

    memset(buffer, 'a', sizeof(buffer));
    buffer[sizeof(buffer) - 1] = '\0';
    buffer_len                 = sizeof(buffer);
    ASSERT_EQ(0, http_header_generate_string(this->m_header, &state, buffer, buffer_len));
    ASSERT_EQ("", string(buffer));
}

TEST_F(TestHttpHeader, test_one_item_split_val) // NOLINT
{
    http_header_generate_state_t state = {
        .stage = HTTP_HEADER_GENERATE_STAGE_INIT,
        .item_idx = 0,
        .item_state = {
            .stage = HTTP_HEADER_GENERATE_ITEM_STAGE_INIT,
            .offset = 0,
        },
    };
    char buffer[14];

    ASSERT_EQ(0, http_header_count(this->m_header));
    http_header_set(this->m_header, "key_abcdef", "v1");
    ASSERT_EQ(1, http_header_count(this->m_header));

    memset(buffer, 'a', sizeof(buffer));
    buffer[sizeof(buffer) - 1]    = '\0';
    int          buffer_len       = sizeof(buffer);
    const string expected_header1 = "key_abcdef: v";

    size_t ret = http_header_generate_string(this->m_header, &state, buffer, buffer_len);
    ASSERT_EQ(expected_header1, string(buffer));
    ASSERT_EQ(ret, expected_header1.length());

    memset(buffer, 'a', sizeof(buffer));
    buffer[sizeof(buffer) - 1]    = '\0';
    buffer_len                    = sizeof(buffer);
    const string expected_header2 = "1\r\n\r\n";

    ret = http_header_generate_string(this->m_header, &state, buffer, buffer_len);
    ASSERT_EQ(expected_header2, string(buffer));
    ASSERT_EQ(ret, expected_header2.length());

    memset(buffer, 'a', sizeof(buffer));
    buffer[sizeof(buffer) - 1] = '\0';
    buffer_len                 = sizeof(buffer);
    ASSERT_EQ(0, http_header_generate_string(this->m_header, &state, buffer, buffer_len));
    ASSERT_EQ("", string(buffer));
}

TEST_F(TestHttpHeader, test_one_item_split_after_val) // NOLINT
{
    http_header_generate_state_t state = {
        .stage = HTTP_HEADER_GENERATE_STAGE_INIT,
        .item_idx = 0,
        .item_state = {
            .stage = HTTP_HEADER_GENERATE_ITEM_STAGE_INIT,
            .offset = 0,
        },
    };
    char buffer[15];

    ASSERT_EQ(0, http_header_count(this->m_header));
    http_header_set(this->m_header, "key_abcdef", "v1");
    ASSERT_EQ(1, http_header_count(this->m_header));

    memset(buffer, 'a', sizeof(buffer));
    buffer[sizeof(buffer) - 1]    = '\0';
    int          buffer_len       = sizeof(buffer);
    const string expected_header1 = "key_abcdef: v1";

    size_t ret = http_header_generate_string(this->m_header, &state, buffer, buffer_len);
    ASSERT_EQ(expected_header1, string(buffer));
    ASSERT_EQ(ret, expected_header1.length());

    memset(buffer, 'a', sizeof(buffer));
    buffer[sizeof(buffer) - 1]    = '\0';
    buffer_len                    = sizeof(buffer);
    const string expected_header2 = "\r\n\r\n";

    ret = http_header_generate_string(this->m_header, &state, buffer, buffer_len);
    ASSERT_EQ(expected_header2, string(buffer));
    ASSERT_EQ(ret, expected_header2.length());

    memset(buffer, 'a', sizeof(buffer));
    buffer[sizeof(buffer) - 1] = '\0';
    buffer_len                 = sizeof(buffer);
    ASSERT_EQ(0, http_header_generate_string(this->m_header, &state, buffer, buffer_len));
    ASSERT_EQ("", string(buffer));
}

TEST_F(TestHttpHeader, test_one_item_split_eol) // NOLINT
{
    http_header_generate_state_t state = {
        .stage = HTTP_HEADER_GENERATE_STAGE_INIT,
        .item_idx = 0,
        .item_state = {
            .stage = HTTP_HEADER_GENERATE_ITEM_STAGE_INIT,
            .offset = 0,
        },
    };
    char buffer[16];

    ASSERT_EQ(0, http_header_count(this->m_header));
    http_header_set(this->m_header, "key_abcdef", "v1");
    ASSERT_EQ(1, http_header_count(this->m_header));

    memset(buffer, 'a', sizeof(buffer));
    buffer[sizeof(buffer) - 1]    = '\0';
    int          buffer_len       = sizeof(buffer);
    const string expected_header1 = "key_abcdef: v1\r";

    size_t ret = http_header_generate_string(this->m_header, &state, buffer, buffer_len);
    ASSERT_EQ(expected_header1, string(buffer));
    ASSERT_EQ(ret, expected_header1.length());

    memset(buffer, 'a', sizeof(buffer));
    buffer[sizeof(buffer) - 1]    = '\0';
    buffer_len                    = sizeof(buffer);
    const string expected_header2 = "\n\r\n";

    ret = http_header_generate_string(this->m_header, &state, buffer, buffer_len);
    ASSERT_EQ(expected_header2, string(buffer));
    ASSERT_EQ(ret, expected_header2.length());

    memset(buffer, 'a', sizeof(buffer));
    buffer[sizeof(buffer) - 1] = '\0';
    buffer_len                 = sizeof(buffer);
    ASSERT_EQ(0, http_header_generate_string(this->m_header, &state, buffer, buffer_len));
    ASSERT_EQ("", string(buffer));
}

TEST_F(TestHttpHeader, test_one_item_split_after_eol) // NOLINT
{
    http_header_generate_state_t state = {
        .stage = HTTP_HEADER_GENERATE_STAGE_INIT,
        .item_idx = 0,
        .item_state = {
            .stage = HTTP_HEADER_GENERATE_ITEM_STAGE_INIT,
            .offset = 0,
        },
    };
    char buffer[17];

    ASSERT_EQ(0, http_header_count(this->m_header));
    http_header_set(this->m_header, "key_abcdef", "v1");
    ASSERT_EQ(1, http_header_count(this->m_header));

    memset(buffer, 'a', sizeof(buffer));
    buffer[sizeof(buffer) - 1]    = '\0';
    int          buffer_len       = sizeof(buffer);
    const string expected_header1 = "key_abcdef: v1\r\n";

    size_t ret = http_header_generate_string(this->m_header, &state, buffer, buffer_len);
    ASSERT_EQ(expected_header1, string(buffer));
    ASSERT_EQ(ret, expected_header1.length());

    memset(buffer, 'a', sizeof(buffer));
    buffer[sizeof(buffer) - 1]    = '\0';
    buffer_len                    = sizeof(buffer);
    const string expected_header2 = "\r\n";

    ret = http_header_generate_string(this->m_header, &state, buffer, buffer_len);
    ASSERT_EQ(expected_header2, string(buffer));
    ASSERT_EQ(ret, expected_header2.length());

    memset(buffer, 'a', sizeof(buffer));
    buffer[sizeof(buffer) - 1] = '\0';
    buffer_len                 = sizeof(buffer);
    ASSERT_EQ(0, http_header_generate_string(this->m_header, &state, buffer, buffer_len));
    ASSERT_EQ("", string(buffer));
}

TEST_F(TestHttpHeader, test_one_item_split_final_eol) // NOLINT
{
    http_header_generate_state_t state = {
        .stage = HTTP_HEADER_GENERATE_STAGE_INIT,
        .item_idx = 0,
        .item_state = {
            .stage = HTTP_HEADER_GENERATE_ITEM_STAGE_INIT,
            .offset = 0,
        },
    };
    char buffer[18];

    ASSERT_EQ(0, http_header_count(this->m_header));
    http_header_set(this->m_header, "key_abcdef", "v1");
    ASSERT_EQ(1, http_header_count(this->m_header));

    memset(buffer, 'a', sizeof(buffer));
    buffer[sizeof(buffer) - 1]    = '\0';
    int          buffer_len       = sizeof(buffer);
    const string expected_header1 = "key_abcdef: v1\r\n\r";

    size_t ret = http_header_generate_string(this->m_header, &state, buffer, buffer_len);
    ASSERT_EQ(expected_header1, string(buffer));
    ASSERT_EQ(ret, expected_header1.length());

    memset(buffer, 'a', sizeof(buffer));
    buffer[sizeof(buffer) - 1]    = '\0';
    buffer_len                    = sizeof(buffer);
    const string expected_header2 = "\n";

    ret = http_header_generate_string(this->m_header, &state, buffer, buffer_len);
    ASSERT_EQ(expected_header2, string(buffer));
    ASSERT_EQ(ret, expected_header2.length());

    memset(buffer, 'a', sizeof(buffer));
    buffer[sizeof(buffer) - 1] = '\0';
    buffer_len                 = sizeof(buffer);
    ASSERT_EQ(0, http_header_generate_string(this->m_header, &state, buffer, buffer_len));
    ASSERT_EQ("", string(buffer));
}

TEST_F(TestHttpHeader, test_one_item_no_split_after_final_eol) // NOLINT
{
    http_header_generate_state_t state = {
        .stage = HTTP_HEADER_GENERATE_STAGE_INIT,
        .item_idx = 0,
        .item_state = {
            .stage = HTTP_HEADER_GENERATE_ITEM_STAGE_INIT,
            .offset = 0,
        },
    };
    char buffer[19];

    ASSERT_EQ(0, http_header_count(this->m_header));
    http_header_set(this->m_header, "key_abcdef", "v1");
    ASSERT_EQ(1, http_header_count(this->m_header));

    memset(buffer, 'a', sizeof(buffer));
    buffer[sizeof(buffer) - 1]    = '\0';
    int          buffer_len       = sizeof(buffer);
    const string expected_header1 = "key_abcdef: v1\r\n\r\n";

    size_t ret = http_header_generate_string(this->m_header, &state, buffer, buffer_len);
    ASSERT_EQ(expected_header1, string(buffer));
    ASSERT_EQ(ret, expected_header1.length());

    memset(buffer, 'a', sizeof(buffer));
    buffer[sizeof(buffer) - 1] = '\0';
    buffer_len                 = sizeof(buffer);
    ASSERT_EQ(0, http_header_generate_string(this->m_header, &state, buffer, buffer_len));
    ASSERT_EQ("", string(buffer));
}

TEST_F(TestHttpHeader, test_two_items) // NOLINT
{
    http_header_generate_state_t state = {
        .stage = HTTP_HEADER_GENERATE_STAGE_INIT,
        .item_idx = 0,
        .item_state = {
            .stage = HTTP_HEADER_GENERATE_ITEM_STAGE_INIT,
            .offset = 0,
        },
    };
    char buffer[64];

    ASSERT_EQ(0, http_header_count(this->m_header));
    ASSERT_EQ(ESP_OK, http_header_set(this->m_header, "key1", "value1"));
    ASSERT_EQ(ESP_OK, http_header_set(this->m_header, "key2", "value2"));
    ASSERT_EQ(2, http_header_count(this->m_header));

    memset(buffer, 'a', sizeof(buffer));
    buffer[sizeof(buffer) - 1]   = '\0';
    int          buffer_len      = sizeof(buffer);
    const string expected_header = "key1: value1\r\nkey2: value2\r\n\r\n";

    size_t ret = http_header_generate_string(this->m_header, &state, buffer, buffer_len);
    ASSERT_EQ(expected_header, string(buffer));
    ASSERT_EQ(expected_header.length(), ret);

    memset(buffer, 'a', sizeof(buffer));
    buffer[sizeof(buffer) - 1] = '\0';
    buffer_len                 = sizeof(buffer);
    ASSERT_EQ(0, http_header_generate_string(this->m_header, &state, buffer, buffer_len));
    ASSERT_EQ("", string(buffer));
}

TEST_F(TestHttpHeader, test_two_items_split) // NOLINT
{
    http_header_generate_state_t state = {
        .item_idx = 0,
        .item_state = {
            .stage = HTTP_HEADER_GENERATE_ITEM_STAGE_INIT,
            .offset = 0,
        },
    };
    char buffer[15];

    ASSERT_EQ(0, http_header_count(this->m_header));
    ASSERT_EQ(ESP_OK, http_header_set(this->m_header, "key1", "value1"));
    ASSERT_EQ(ESP_OK, http_header_set(this->m_header, "key2", "value2"));
    ASSERT_EQ(2, http_header_count(this->m_header));

    memset(buffer, 'a', sizeof(buffer));
    buffer[sizeof(buffer) - 1]    = '\0';
    int          buffer_len       = sizeof(buffer);
    const string expected_header1 = "key1: value1\r\n";

    size_t ret = http_header_generate_string(this->m_header, &state, buffer, buffer_len);
    ASSERT_EQ(expected_header1, string(buffer));
    ASSERT_EQ(expected_header1.length(), ret);

    memset(buffer, 'a', sizeof(buffer));
    buffer[sizeof(buffer) - 1]    = '\0';
    buffer_len                    = sizeof(buffer);
    const string expected_header2 = "key2: value2\r\n";

    ret = http_header_generate_string(this->m_header, &state, buffer, buffer_len);
    ASSERT_EQ(expected_header2, string(buffer));
    ASSERT_EQ(expected_header2.length(), ret);

    memset(buffer, 'a', sizeof(buffer));
    buffer[sizeof(buffer) - 1]    = '\0';
    buffer_len                    = sizeof(buffer);
    const string expected_header3 = "\r\n";

    ret = http_header_generate_string(this->m_header, &state, buffer, buffer_len);
    ASSERT_EQ(expected_header3, string(buffer));
    ASSERT_EQ(expected_header3.length(), ret);

    memset(buffer, 'a', sizeof(buffer));
    buffer[sizeof(buffer) - 1] = '\0';
    buffer_len                 = sizeof(buffer);
    ASSERT_EQ(0, http_header_generate_string(this->m_header, &state, buffer, buffer_len));
    ASSERT_EQ("", string(buffer));
}

TEST_F(TestHttpHeader, test_two_items_split_2) // NOLINT
{
    http_header_generate_state_t state = {
        .item_idx = 0,
        .item_state = {
            .stage = HTTP_HEADER_GENERATE_ITEM_STAGE_INIT,
            .offset = 0,
        },
    };
    char buffer[14];

    ASSERT_EQ(0, http_header_count(this->m_header));
    ASSERT_EQ(ESP_OK, http_header_set(this->m_header, "key1", "value1"));
    ASSERT_EQ(ESP_OK, http_header_set(this->m_header, "key2", "value234"));
    ASSERT_EQ(2, http_header_count(this->m_header));

    memset(buffer, 'a', sizeof(buffer));
    buffer[sizeof(buffer) - 1]    = '\0';
    int          buffer_len       = sizeof(buffer);
    const string expected_header1 = "key1: value1\r";

    size_t ret = http_header_generate_string(this->m_header, &state, buffer, buffer_len);
    ASSERT_EQ(expected_header1, string(buffer));
    ASSERT_EQ(expected_header1.length(), ret);

    memset(buffer, 'a', sizeof(buffer));
    buffer[sizeof(buffer) - 1]    = '\0';
    buffer_len                    = sizeof(buffer);
    const string expected_header2 = "\nkey2: value2";

    ret = http_header_generate_string(this->m_header, &state, buffer, buffer_len);
    ASSERT_EQ(expected_header2, string(buffer));
    ASSERT_EQ(expected_header2.length(), ret);

    memset(buffer, 'a', sizeof(buffer));
    buffer[sizeof(buffer) - 1]    = '\0';
    buffer_len                    = sizeof(buffer);
    const string expected_header3 = "34\r\n\r\n";

    ret = http_header_generate_string(this->m_header, &state, buffer, buffer_len);
    ASSERT_EQ(expected_header3, string(buffer));
    ASSERT_EQ(expected_header3.length(), ret);

    memset(buffer, 'a', sizeof(buffer));
    buffer[sizeof(buffer) - 1] = '\0';
    buffer_len                 = sizeof(buffer);
    ASSERT_EQ(0, http_header_generate_string(this->m_header, &state, buffer, buffer_len));
    ASSERT_EQ("", string(buffer));
}
