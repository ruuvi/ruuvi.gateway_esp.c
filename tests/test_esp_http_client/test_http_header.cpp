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
#include "esp_http_client_stream.h"

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
        },
    };
    ASSERT_EQ(0, http_header_count(this->m_header));
    char buffer[64];
    memset(buffer, 'a', sizeof(buffer));
    buffer[sizeof(buffer) - 1]   = '\0';
    const size_t buffer_len      = sizeof(buffer);
    const string expected_header = "\r\n";

    http_stream_reader_string_ctx_t ctx { .p_str = nullptr, .data_offset = 0 };
    http_stream_last_call_t         last_call = { .cb_stream_reader = nullptr, .p_ctx = nullptr };
    size_t                          buf_ofs   = 0;
    ASSERT_TRUE(http_header_generate_string(&ctx, &last_call, this->m_header, &state, buffer, buffer_len, &buf_ofs));
    ASSERT_EQ(expected_header, string(buffer));
    ASSERT_EQ(expected_header.length(), buf_ofs);

    // Test writing '\0' to end of the buffer when buf_ofs is not zero
    memset(buffer, 'a', sizeof(buffer));
    buffer[sizeof(buffer) - 1] = '\0';
    buf_ofs                    = 2;
    ASSERT_TRUE(http_header_generate_string(&ctx, &last_call, this->m_header, &state, buffer, buffer_len, &buf_ofs));
    ASSERT_EQ(string("aa"), string(buffer));
    ASSERT_EQ(2, buf_ofs);

    // Test that buffer is not modified when buf_ofs is already at the end of the buffer
    memset(buffer, 'a', sizeof(buffer));
    buffer[sizeof(buffer) - 1] = '\0';
    buf_ofs                    = 2;
    ASSERT_TRUE(http_header_generate_string(&ctx, &last_call, this->m_header, &state, buffer, buf_ofs, &buf_ofs));
    ASSERT_EQ(string("aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"), string(buffer));
    ASSERT_EQ(2, buf_ofs);
}

TEST_F(TestHttpHeader, test_one_item) // NOLINT
{
    http_header_generate_state_t state = {
        .stage = HTTP_HEADER_GENERATE_STAGE_INIT,
        .item_idx = 0,
        .item_state = {
            .stage = HTTP_HEADER_GENERATE_ITEM_STAGE_INIT,
        },
    };
    char buffer[64];

    ASSERT_EQ(0, http_header_count(this->m_header));
    http_header_set(this->m_header, "key1", "value1");
    ASSERT_EQ(1, http_header_count(this->m_header));

    memset(buffer, 'a', sizeof(buffer));
    buffer[sizeof(buffer) - 1]   = '\0';
    const size_t buffer_len      = sizeof(buffer);
    const string expected_header = "key1: value1\r\n\r\n";

    http_stream_reader_string_ctx_t ctx { .p_str = nullptr, .data_offset = 0 };
    http_stream_last_call_t         last_call = { .cb_stream_reader = nullptr, .p_ctx = nullptr };
    size_t                          buf_ofs   = 0;
    ASSERT_TRUE(http_header_generate_string(&ctx, &last_call, this->m_header, &state, buffer, buffer_len, &buf_ofs));
    ASSERT_EQ(expected_header, string(buffer));
    ASSERT_EQ(expected_header.length(), buf_ofs);

    memset(buffer, 'a', sizeof(buffer));
    buffer[sizeof(buffer) - 1] = '\0';
    buf_ofs                    = 0;
    ASSERT_TRUE(http_header_generate_string(&ctx, &last_call, this->m_header, &state, buffer, buffer_len, &buf_ofs));
    ASSERT_EQ("", string(buffer));
    ASSERT_EQ(0, buf_ofs);
}

TEST_F(TestHttpHeader, test_one_item_empty_buffer) // NOLINT
{
    http_header_generate_state_t state = {
        .stage = HTTP_HEADER_GENERATE_STAGE_INIT,
        .item_idx = 0,
        .item_state = {
            .stage = HTTP_HEADER_GENERATE_ITEM_STAGE_INIT,
        },
    };
    char buffer[16];

    ASSERT_EQ(0, http_header_count(this->m_header));
    http_header_set(this->m_header, "key1", "val1");
    ASSERT_EQ(1, http_header_count(this->m_header));

    memset(buffer, 'a', sizeof(buffer));
    buffer[sizeof(buffer) - 1] = '\0';
    const size_t buffer_len    = sizeof(buffer);
    size_t       buf_ofs       = buffer_len;

    http_stream_reader_string_ctx_t ctx { .p_str = nullptr, .data_offset = 0 };
    http_stream_last_call_t         last_call = { .cb_stream_reader = nullptr, .p_ctx = nullptr };
    ASSERT_TRUE(http_header_generate_string(&ctx, &last_call, this->m_header, &state, buffer, buffer_len, &buf_ofs));
    ASSERT_EQ("aaaaaaaaaaaaaaa", string(buffer));
    ASSERT_EQ(buffer_len, buf_ofs);
}

TEST_F(TestHttpHeader, test_one_item_buffer_overflow) // NOLINT
{
    http_header_generate_state_t state = {
        .stage = HTTP_HEADER_GENERATE_STAGE_INIT,
        .item_idx = 0,
        .item_state = {
            .stage = HTTP_HEADER_GENERATE_ITEM_STAGE_INIT,
        },
    };
    char buffer[16];

    ASSERT_EQ(0, http_header_count(this->m_header));
    http_header_set(this->m_header, "key1", "value1");
    ASSERT_EQ(1, http_header_count(this->m_header));

    memset(buffer, 'a', sizeof(buffer));
    buffer[sizeof(buffer) - 1]    = '\0';
    const size_t buffer_len       = sizeof(buffer);
    const string expected_header1 = "key1: value1\r\n\r";

    http_stream_reader_string_ctx_t ctx { .p_str = nullptr, .data_offset = 0 };
    http_stream_last_call_t         last_call = { .cb_stream_reader = nullptr, .p_ctx = nullptr };
    size_t                          buf_ofs   = 0;
    ASSERT_TRUE(http_header_generate_string(&ctx, &last_call, this->m_header, &state, buffer, buffer_len, &buf_ofs));
    ASSERT_EQ(expected_header1, string(buffer));
    ASSERT_EQ(expected_header1.length(), buf_ofs);

    memset(buffer, 'a', sizeof(buffer));
    buffer[sizeof(buffer) - 1]    = '\0';
    const string expected_header2 = "\n";

    buf_ofs = 0;
    ASSERT_TRUE(http_header_generate_string(&ctx, &last_call, this->m_header, &state, buffer, buffer_len, &buf_ofs));
    ASSERT_EQ(expected_header2, string(buffer));
    ASSERT_EQ(expected_header2.length(), buf_ofs);

    memset(buffer, 'a', sizeof(buffer));
    buffer[sizeof(buffer) - 1] = '\0';

    buf_ofs = 0;
    ASSERT_TRUE(http_header_generate_string(&ctx, &last_call, this->m_header, &state, buffer, buffer_len, &buf_ofs));
    ASSERT_EQ("", string(buffer));
    ASSERT_EQ(0, buf_ofs);
}

TEST_F(TestHttpHeader, test_one_item_split_key) // NOLINT
{
    http_header_generate_state_t state = {
        .stage = HTTP_HEADER_GENERATE_STAGE_INIT,
        .item_idx = 0,
        .item_state = {
            .stage = HTTP_HEADER_GENERATE_ITEM_STAGE_INIT,
        },
    };
    char buffer[10];

    ASSERT_EQ(0, http_header_count(this->m_header));
    http_header_set(this->m_header, "key_abcdef", "v1");
    ASSERT_EQ(1, http_header_count(this->m_header));

    memset(buffer, 'a', sizeof(buffer));
    buffer[sizeof(buffer) - 1]    = '\0';
    const size_t buffer_len       = sizeof(buffer);
    const string expected_header1 = "key_abcde";

    http_stream_reader_string_ctx_t ctx { .p_str = nullptr, .data_offset = 0 };
    http_stream_last_call_t         last_call = { .cb_stream_reader = nullptr, .p_ctx = nullptr };
    size_t                          buf_ofs   = 0;
    ASSERT_TRUE(http_header_generate_string(&ctx, &last_call, this->m_header, &state, buffer, buffer_len, &buf_ofs));
    ASSERT_EQ(expected_header1, string(buffer));
    ASSERT_EQ(expected_header1.length(), buf_ofs);

    memset(buffer, 'a', sizeof(buffer));
    buffer[sizeof(buffer) - 1]    = '\0';
    const string expected_header2 = "f: v1\r\n\r\n";

    buf_ofs = 0;
    ASSERT_TRUE(http_header_generate_string(&ctx, &last_call, this->m_header, &state, buffer, buffer_len, &buf_ofs));
    ASSERT_EQ(expected_header2, string(buffer));
    ASSERT_EQ(expected_header2.length(), buf_ofs);

    memset(buffer, 'a', sizeof(buffer));
    buffer[sizeof(buffer) - 1] = '\0';
    buf_ofs                    = 0;
    ASSERT_TRUE(http_header_generate_string(&ctx, &last_call, this->m_header, &state, buffer, buffer_len, &buf_ofs));
    ASSERT_EQ("", string(buffer));
    ASSERT_EQ(0, buf_ofs);
}

TEST_F(TestHttpHeader, test_one_item_split_after_key) // NOLINT
{
    http_header_generate_state_t state = {
        .stage = HTTP_HEADER_GENERATE_STAGE_INIT,
        .item_idx = 0,
        .item_state = {
            .stage = HTTP_HEADER_GENERATE_ITEM_STAGE_INIT,
        },
    };
    char buffer[11];

    ASSERT_EQ(0, http_header_count(this->m_header));
    http_header_set(this->m_header, "key_abcdef", "v1");
    ASSERT_EQ(1, http_header_count(this->m_header));

    memset(buffer, 'a', sizeof(buffer));
    buffer[sizeof(buffer) - 1]    = '\0';
    const size_t buffer_len       = sizeof(buffer);
    const string expected_header1 = "key_abcdef";

    http_stream_reader_string_ctx_t ctx { .p_str = nullptr, .data_offset = 0 };
    http_stream_last_call_t         last_call = { .cb_stream_reader = nullptr, .p_ctx = nullptr };
    size_t                          buf_ofs   = 0;
    ASSERT_TRUE(http_header_generate_string(&ctx, &last_call, this->m_header, &state, buffer, buffer_len, &buf_ofs));
    ASSERT_EQ(expected_header1, string(buffer));
    ASSERT_EQ(expected_header1.length(), buf_ofs);

    memset(buffer, 'a', sizeof(buffer));
    buffer[sizeof(buffer) - 1]    = '\0';
    const string expected_header2 = ": v1\r\n\r\n";

    buf_ofs = 0;
    ASSERT_TRUE(http_header_generate_string(&ctx, &last_call, this->m_header, &state, buffer, buffer_len, &buf_ofs));
    ASSERT_EQ(expected_header2, string(buffer));
    ASSERT_EQ(expected_header2.length(), buf_ofs);

    memset(buffer, 'a', sizeof(buffer));
    buffer[sizeof(buffer) - 1] = '\0';
    buf_ofs                    = 0;
    ASSERT_TRUE(http_header_generate_string(&ctx, &last_call, this->m_header, &state, buffer, buffer_len, &buf_ofs));
    ASSERT_EQ("", string(buffer));
    ASSERT_EQ(0, buf_ofs);
}

TEST_F(TestHttpHeader, test_one_item_split_separator) // NOLINT
{
    http_header_generate_state_t state = {
        .stage = HTTP_HEADER_GENERATE_STAGE_INIT,
        .item_idx = 0,
        .item_state = {
            .stage = HTTP_HEADER_GENERATE_ITEM_STAGE_INIT,
        },
    };
    char buffer[12];

    ASSERT_EQ(0, http_header_count(this->m_header));
    http_header_set(this->m_header, "key_abcdef", "v1");
    ASSERT_EQ(1, http_header_count(this->m_header));

    memset(buffer, 'a', sizeof(buffer));
    buffer[sizeof(buffer) - 1]    = '\0';
    const size_t buffer_len       = sizeof(buffer);
    const string expected_header1 = "key_abcdef:";

    http_stream_reader_string_ctx_t ctx { .p_str = nullptr, .data_offset = 0 };
    http_stream_last_call_t         last_call = { .cb_stream_reader = nullptr, .p_ctx = nullptr };
    size_t                          buf_ofs   = 0;
    ASSERT_TRUE(http_header_generate_string(&ctx, &last_call, this->m_header, &state, buffer, buffer_len, &buf_ofs));
    ASSERT_EQ(expected_header1, string(buffer));
    ASSERT_EQ(expected_header1.length(), buf_ofs);

    memset(buffer, 'a', sizeof(buffer));
    buffer[sizeof(buffer) - 1]    = '\0';
    const string expected_header2 = " v1\r\n\r\n";

    buf_ofs = 0;
    ASSERT_TRUE(http_header_generate_string(&ctx, &last_call, this->m_header, &state, buffer, buffer_len, &buf_ofs));
    ASSERT_EQ(expected_header2, string(buffer));
    ASSERT_EQ(expected_header2.length(), buf_ofs);

    memset(buffer, 'a', sizeof(buffer));
    buffer[sizeof(buffer) - 1] = '\0';
    buf_ofs                    = 0;
    ASSERT_TRUE(http_header_generate_string(&ctx, &last_call, this->m_header, &state, buffer, buffer_len, &buf_ofs));
    ASSERT_EQ("", string(buffer));
    ASSERT_EQ(0, buf_ofs);
}

TEST_F(TestHttpHeader, test_one_item_split_after_separator) // NOLINT
{
    http_header_generate_state_t state = {
        .stage = HTTP_HEADER_GENERATE_STAGE_INIT,
        .item_idx = 0,
        .item_state = {
            .stage = HTTP_HEADER_GENERATE_ITEM_STAGE_INIT,
        },
    };
    char buffer[13];

    ASSERT_EQ(0, http_header_count(this->m_header));
    http_header_set(this->m_header, "key_abcdef", "v1");
    ASSERT_EQ(1, http_header_count(this->m_header));

    memset(buffer, 'a', sizeof(buffer));
    buffer[sizeof(buffer) - 1]    = '\0';
    const size_t buffer_len       = sizeof(buffer);
    const string expected_header1 = "key_abcdef: ";

    http_stream_reader_string_ctx_t ctx { .p_str = nullptr, .data_offset = 0 };
    http_stream_last_call_t         last_call = { .cb_stream_reader = nullptr, .p_ctx = nullptr };
    size_t                          buf_ofs   = 0;
    ASSERT_TRUE(http_header_generate_string(&ctx, &last_call, this->m_header, &state, buffer, buffer_len, &buf_ofs));
    ASSERT_EQ(expected_header1, string(buffer));
    ASSERT_EQ(expected_header1.length(), buf_ofs);

    memset(buffer, 'a', sizeof(buffer));
    buffer[sizeof(buffer) - 1]    = '\0';
    const string expected_header2 = "v1\r\n\r\n";

    buf_ofs = 0;
    ASSERT_TRUE(http_header_generate_string(&ctx, &last_call, this->m_header, &state, buffer, buffer_len, &buf_ofs));
    ASSERT_EQ(expected_header2, string(buffer));
    ASSERT_EQ(expected_header2.length(), buf_ofs);

    memset(buffer, 'a', sizeof(buffer));
    buffer[sizeof(buffer) - 1] = '\0';
    buf_ofs                    = 0;
    ASSERT_TRUE(http_header_generate_string(&ctx, &last_call, this->m_header, &state, buffer, buffer_len, &buf_ofs));
    ASSERT_EQ("", string(buffer));
    ASSERT_EQ(0, buf_ofs);
}

TEST_F(TestHttpHeader, test_one_item_split_val) // NOLINT
{
    http_header_generate_state_t state = {
        .stage = HTTP_HEADER_GENERATE_STAGE_INIT,
        .item_idx = 0,
        .item_state = {
            .stage = HTTP_HEADER_GENERATE_ITEM_STAGE_INIT,
        },
    };
    char buffer[14];

    ASSERT_EQ(0, http_header_count(this->m_header));
    http_header_set(this->m_header, "key_abcdef", "v1");
    ASSERT_EQ(1, http_header_count(this->m_header));

    memset(buffer, 'a', sizeof(buffer));
    buffer[sizeof(buffer) - 1]    = '\0';
    const size_t buffer_len       = sizeof(buffer);
    const string expected_header1 = "key_abcdef: v";

    http_stream_reader_string_ctx_t ctx { .p_str = nullptr, .data_offset = 0 };
    http_stream_last_call_t         last_call = { .cb_stream_reader = nullptr, .p_ctx = nullptr };
    size_t                          buf_ofs   = 0;
    ASSERT_TRUE(http_header_generate_string(&ctx, &last_call, this->m_header, &state, buffer, buffer_len, &buf_ofs));
    ASSERT_EQ(expected_header1, string(buffer));
    ASSERT_EQ(expected_header1.length(), buf_ofs);

    memset(buffer, 'a', sizeof(buffer));
    buffer[sizeof(buffer) - 1]    = '\0';
    const string expected_header2 = "1\r\n\r\n";

    buf_ofs = 0;
    ASSERT_TRUE(http_header_generate_string(&ctx, &last_call, this->m_header, &state, buffer, buffer_len, &buf_ofs));
    ASSERT_EQ(expected_header2, string(buffer));
    ASSERT_EQ(expected_header2.length(), buf_ofs);

    memset(buffer, 'a', sizeof(buffer));
    buffer[sizeof(buffer) - 1] = '\0';
    buf_ofs                    = 0;
    ASSERT_TRUE(http_header_generate_string(&ctx, &last_call, this->m_header, &state, buffer, buffer_len, &buf_ofs));
    ASSERT_EQ("", string(buffer));
    ASSERT_EQ(0, buf_ofs);
}

TEST_F(TestHttpHeader, test_one_item_split_after_val) // NOLINT
{
    http_header_generate_state_t state = {
        .stage = HTTP_HEADER_GENERATE_STAGE_INIT,
        .item_idx = 0,
        .item_state = {
            .stage = HTTP_HEADER_GENERATE_ITEM_STAGE_INIT,
        },
    };
    char buffer[15];

    ASSERT_EQ(0, http_header_count(this->m_header));
    http_header_set(this->m_header, "key_abcdef", "v1");
    ASSERT_EQ(1, http_header_count(this->m_header));

    memset(buffer, 'a', sizeof(buffer));
    buffer[sizeof(buffer) - 1]    = '\0';
    const size_t buffer_len       = sizeof(buffer);
    const string expected_header1 = "key_abcdef: v1";

    http_stream_reader_string_ctx_t ctx { .p_str = nullptr, .data_offset = 0 };
    http_stream_last_call_t         last_call = { .cb_stream_reader = nullptr, .p_ctx = nullptr };
    size_t                          buf_ofs   = 0;
    ASSERT_TRUE(http_header_generate_string(&ctx, &last_call, this->m_header, &state, buffer, buffer_len, &buf_ofs));
    ASSERT_EQ(expected_header1, string(buffer));
    ASSERT_EQ(expected_header1.length(), buf_ofs);

    memset(buffer, 'a', sizeof(buffer));
    buffer[sizeof(buffer) - 1]    = '\0';
    const string expected_header2 = "\r\n\r\n";

    buf_ofs = 0;
    ASSERT_TRUE(http_header_generate_string(&ctx, &last_call, this->m_header, &state, buffer, buffer_len, &buf_ofs));
    ASSERT_EQ(expected_header2, string(buffer));
    ASSERT_EQ(expected_header2.length(), buf_ofs);

    memset(buffer, 'a', sizeof(buffer));
    buffer[sizeof(buffer) - 1] = '\0';
    buf_ofs                    = 0;
    ASSERT_TRUE(http_header_generate_string(&ctx, &last_call, this->m_header, &state, buffer, buffer_len, &buf_ofs));
    ASSERT_EQ("", string(buffer));
    ASSERT_EQ(0, buf_ofs);
}

TEST_F(TestHttpHeader, test_one_item_split_eol) // NOLINT
{
    http_header_generate_state_t state = {
        .stage = HTTP_HEADER_GENERATE_STAGE_INIT,
        .item_idx = 0,
        .item_state = {
            .stage = HTTP_HEADER_GENERATE_ITEM_STAGE_INIT,
        },
    };
    char buffer[16];

    ASSERT_EQ(0, http_header_count(this->m_header));
    http_header_set(this->m_header, "key_abcdef", "v1");
    ASSERT_EQ(1, http_header_count(this->m_header));

    memset(buffer, 'a', sizeof(buffer));
    buffer[sizeof(buffer) - 1]    = '\0';
    const size_t buffer_len       = sizeof(buffer);
    const string expected_header1 = "key_abcdef: v1\r";

    http_stream_reader_string_ctx_t ctx { .p_str = nullptr, .data_offset = 0 };
    http_stream_last_call_t         last_call = { .cb_stream_reader = nullptr, .p_ctx = nullptr };
    size_t                          buf_ofs   = 0;
    ASSERT_TRUE(http_header_generate_string(&ctx, &last_call, this->m_header, &state, buffer, buffer_len, &buf_ofs));
    ASSERT_EQ(expected_header1, string(buffer));
    ASSERT_EQ(expected_header1.length(), buf_ofs);

    memset(buffer, 'a', sizeof(buffer));
    buffer[sizeof(buffer) - 1]    = '\0';
    const string expected_header2 = "\n\r\n";

    buf_ofs = 0;
    ASSERT_TRUE(http_header_generate_string(&ctx, &last_call, this->m_header, &state, buffer, buffer_len, &buf_ofs));
    ASSERT_EQ(expected_header2, string(buffer));
    ASSERT_EQ(expected_header2.length(), buf_ofs);

    memset(buffer, 'a', sizeof(buffer));
    buffer[sizeof(buffer) - 1] = '\0';
    buf_ofs                    = 0;
    ASSERT_TRUE(http_header_generate_string(&ctx, &last_call, this->m_header, &state, buffer, buffer_len, &buf_ofs));
    ASSERT_EQ("", string(buffer));
    ASSERT_EQ(0, buf_ofs);
}

TEST_F(TestHttpHeader, test_one_item_split_after_eol) // NOLINT
{
    http_header_generate_state_t state = {
        .stage = HTTP_HEADER_GENERATE_STAGE_INIT,
        .item_idx = 0,
        .item_state = {
            .stage = HTTP_HEADER_GENERATE_ITEM_STAGE_INIT,
        },
    };
    char buffer[17];

    ASSERT_EQ(0, http_header_count(this->m_header));
    http_header_set(this->m_header, "key_abcdef", "v1");
    ASSERT_EQ(1, http_header_count(this->m_header));

    memset(buffer, 'a', sizeof(buffer));
    buffer[sizeof(buffer) - 1]    = '\0';
    const size_t buffer_len       = sizeof(buffer);
    const string expected_header1 = "key_abcdef: v1\r\n";

    http_stream_reader_string_ctx_t ctx { .p_str = nullptr, .data_offset = 0 };
    http_stream_last_call_t         last_call = { .cb_stream_reader = nullptr, .p_ctx = nullptr };
    size_t                          buf_ofs   = 0;
    ASSERT_TRUE(http_header_generate_string(&ctx, &last_call, this->m_header, &state, buffer, buffer_len, &buf_ofs));
    ASSERT_EQ(expected_header1, string(buffer));
    ASSERT_EQ(expected_header1.length(), buf_ofs);

    memset(buffer, 'a', sizeof(buffer));
    buffer[sizeof(buffer) - 1]    = '\0';
    const string expected_header2 = "\r\n";

    buf_ofs = 0;
    ASSERT_TRUE(http_header_generate_string(&ctx, &last_call, this->m_header, &state, buffer, buffer_len, &buf_ofs));
    ASSERT_EQ(expected_header2, string(buffer));
    ASSERT_EQ(expected_header2.length(), buf_ofs);

    memset(buffer, 'a', sizeof(buffer));
    buffer[sizeof(buffer) - 1] = '\0';
    buf_ofs                    = 0;
    ASSERT_TRUE(http_header_generate_string(&ctx, &last_call, this->m_header, &state, buffer, buffer_len, &buf_ofs));
    ASSERT_EQ("", string(buffer));
    ASSERT_EQ(0, buf_ofs);
}

TEST_F(TestHttpHeader, test_one_item_split_final_eol) // NOLINT
{
    http_header_generate_state_t state = {
        .stage = HTTP_HEADER_GENERATE_STAGE_INIT,
        .item_idx = 0,
        .item_state = {
            .stage = HTTP_HEADER_GENERATE_ITEM_STAGE_INIT,
        },
    };
    char buffer[18];

    ASSERT_EQ(0, http_header_count(this->m_header));
    http_header_set(this->m_header, "key_abcdef", "v1");
    ASSERT_EQ(1, http_header_count(this->m_header));

    memset(buffer, 'a', sizeof(buffer));
    buffer[sizeof(buffer) - 1]    = '\0';
    const size_t buffer_len       = sizeof(buffer);
    const string expected_header1 = "key_abcdef: v1\r\n\r";

    http_stream_reader_string_ctx_t ctx { .p_str = nullptr, .data_offset = 0 };
    http_stream_last_call_t         last_call = { .cb_stream_reader = nullptr, .p_ctx = nullptr };
    size_t                          buf_ofs   = 0;
    ASSERT_TRUE(http_header_generate_string(&ctx, &last_call, this->m_header, &state, buffer, buffer_len, &buf_ofs));
    ASSERT_EQ(expected_header1, string(buffer));
    ASSERT_EQ(expected_header1.length(), buf_ofs);

    memset(buffer, 'a', sizeof(buffer));
    buffer[sizeof(buffer) - 1]    = '\0';
    const string expected_header2 = "\n";

    buf_ofs = 0;
    ASSERT_TRUE(http_header_generate_string(&ctx, &last_call, this->m_header, &state, buffer, buffer_len, &buf_ofs));
    ASSERT_EQ(expected_header2, string(buffer));
    ASSERT_EQ(expected_header2.length(), buf_ofs);

    memset(buffer, 'a', sizeof(buffer));
    buffer[sizeof(buffer) - 1] = '\0';
    buf_ofs                    = 0;
    ASSERT_TRUE(http_header_generate_string(&ctx, &last_call, this->m_header, &state, buffer, buffer_len, &buf_ofs));
    ASSERT_EQ("", string(buffer));
    ASSERT_EQ(0, buf_ofs);
}

TEST_F(TestHttpHeader, test_one_item_no_split_after_final_eol) // NOLINT
{
    http_header_generate_state_t state = {
        .stage = HTTP_HEADER_GENERATE_STAGE_INIT,
        .item_idx = 0,
        .item_state = {
            .stage = HTTP_HEADER_GENERATE_ITEM_STAGE_INIT,
        },
    };
    char buffer[19];

    ASSERT_EQ(0, http_header_count(this->m_header));
    http_header_set(this->m_header, "key_abcdef", "v1");
    ASSERT_EQ(1, http_header_count(this->m_header));

    memset(buffer, 'a', sizeof(buffer));
    buffer[sizeof(buffer) - 1]    = '\0';
    const size_t buffer_len       = sizeof(buffer);
    const string expected_header1 = "key_abcdef: v1\r\n\r\n";

    http_stream_reader_string_ctx_t ctx { .p_str = nullptr, .data_offset = 0 };
    http_stream_last_call_t         last_call = { .cb_stream_reader = nullptr, .p_ctx = nullptr };
    size_t                          buf_ofs   = 0;
    ASSERT_TRUE(http_header_generate_string(&ctx, &last_call, this->m_header, &state, buffer, buffer_len, &buf_ofs));
    ASSERT_EQ(expected_header1, string(buffer));
    ASSERT_EQ(expected_header1.length(), buf_ofs);

    memset(buffer, 'a', sizeof(buffer));
    buffer[sizeof(buffer) - 1] = '\0';
    buf_ofs                    = 0;
    ASSERT_TRUE(http_header_generate_string(&ctx, &last_call, this->m_header, &state, buffer, buffer_len, &buf_ofs));
    ASSERT_EQ("", string(buffer));
    ASSERT_EQ(0, buf_ofs);
}

TEST_F(TestHttpHeader, test_two_items) // NOLINT
{
    http_header_generate_state_t state = {
        .stage = HTTP_HEADER_GENERATE_STAGE_INIT,
        .item_idx = 0,
        .item_state = {
            .stage = HTTP_HEADER_GENERATE_ITEM_STAGE_INIT,
        },
    };
    char buffer[64];

    ASSERT_EQ(0, http_header_count(this->m_header));
    ASSERT_EQ(ESP_OK, http_header_set(this->m_header, "key1", "value1"));
    ASSERT_EQ(ESP_OK, http_header_set(this->m_header, "key2", "value2"));
    ASSERT_EQ(2, http_header_count(this->m_header));

    memset(buffer, 'a', sizeof(buffer));
    buffer[sizeof(buffer) - 1]   = '\0';
    const size_t buffer_len      = sizeof(buffer);
    const string expected_header = "key1: value1\r\nkey2: value2\r\n\r\n";

    http_stream_reader_string_ctx_t ctx { .p_str = nullptr, .data_offset = 0 };
    http_stream_last_call_t         last_call = { .cb_stream_reader = nullptr, .p_ctx = nullptr };
    size_t                          buf_ofs   = 0;
    ASSERT_TRUE(http_header_generate_string(&ctx, &last_call, this->m_header, &state, buffer, buffer_len, &buf_ofs));
    ASSERT_EQ(expected_header, string(buffer));
    ASSERT_EQ(expected_header.length(), buf_ofs);

    memset(buffer, 'a', sizeof(buffer));
    buffer[sizeof(buffer) - 1] = '\0';
    buf_ofs                    = 0;
    ASSERT_TRUE(http_header_generate_string(&ctx, &last_call, this->m_header, &state, buffer, buffer_len, &buf_ofs));
    ASSERT_EQ("", string(buffer));
    ASSERT_EQ(0, buf_ofs);
}

TEST_F(TestHttpHeader, test_two_items_split) // NOLINT
{
    http_header_generate_state_t state = {
        .item_idx = 0,
        .item_state = {
            .stage = HTTP_HEADER_GENERATE_ITEM_STAGE_INIT,
        },
    };
    char buffer[15];

    ASSERT_EQ(0, http_header_count(this->m_header));
    ASSERT_EQ(ESP_OK, http_header_set(this->m_header, "key1", "value1"));
    ASSERT_EQ(ESP_OK, http_header_set(this->m_header, "key2", "value2"));
    ASSERT_EQ(2, http_header_count(this->m_header));

    memset(buffer, 'a', sizeof(buffer));
    buffer[sizeof(buffer) - 1]    = '\0';
    const size_t buffer_len       = sizeof(buffer);
    const string expected_header1 = "key1: value1\r\n";

    http_stream_reader_string_ctx_t ctx { .p_str = nullptr, .data_offset = 0 };
    http_stream_last_call_t         last_call = { .cb_stream_reader = nullptr, .p_ctx = nullptr };
    size_t                          buf_ofs   = 0;
    ASSERT_TRUE(http_header_generate_string(&ctx, &last_call, this->m_header, &state, buffer, buffer_len, &buf_ofs));
    ASSERT_EQ(expected_header1, string(buffer));
    ASSERT_EQ(expected_header1.length(), buf_ofs);

    memset(buffer, 'a', sizeof(buffer));
    buffer[sizeof(buffer) - 1]    = '\0';
    const string expected_header2 = "key2: value2\r\n";

    buf_ofs = 0;
    ASSERT_TRUE(http_header_generate_string(&ctx, &last_call, this->m_header, &state, buffer, buffer_len, &buf_ofs));
    ASSERT_EQ(expected_header2, string(buffer));
    ASSERT_EQ(expected_header2.length(), buf_ofs);

    memset(buffer, 'a', sizeof(buffer));
    buffer[sizeof(buffer) - 1]    = '\0';
    const string expected_header3 = "\r\n";

    buf_ofs = 0;
    ASSERT_TRUE(http_header_generate_string(&ctx, &last_call, this->m_header, &state, buffer, buffer_len, &buf_ofs));
    ASSERT_EQ(expected_header3, string(buffer));
    ASSERT_EQ(expected_header3.length(), buf_ofs);

    memset(buffer, 'a', sizeof(buffer));
    buffer[sizeof(buffer) - 1] = '\0';
    buf_ofs                    = 0;
    ASSERT_TRUE(http_header_generate_string(&ctx, &last_call, this->m_header, &state, buffer, buffer_len, &buf_ofs));
    ASSERT_EQ("", string(buffer));
    ASSERT_EQ(0, buf_ofs);
}

TEST_F(TestHttpHeader, test_two_items_split_2) // NOLINT
{
    http_header_generate_state_t state = {
        .item_idx = 0,
        .item_state = {
            .stage = HTTP_HEADER_GENERATE_ITEM_STAGE_INIT,
        },
    };
    char buffer[14];

    ASSERT_EQ(0, http_header_count(this->m_header));
    ASSERT_EQ(ESP_OK, http_header_set(this->m_header, "key1", "value1"));
    ASSERT_EQ(ESP_OK, http_header_set(this->m_header, "key2", "value234"));
    ASSERT_EQ(2, http_header_count(this->m_header));

    memset(buffer, 'a', sizeof(buffer));
    buffer[sizeof(buffer) - 1]    = '\0';
    const size_t buffer_len       = sizeof(buffer);
    const string expected_header1 = "key1: value1\r";

    http_stream_reader_string_ctx_t ctx { .p_str = nullptr, .data_offset = 0 };
    http_stream_last_call_t         last_call = { .cb_stream_reader = nullptr, .p_ctx = nullptr };
    size_t                          buf_ofs   = 0;
    ASSERT_TRUE(http_header_generate_string(&ctx, &last_call, this->m_header, &state, buffer, buffer_len, &buf_ofs));
    ASSERT_EQ(expected_header1, string(buffer));
    ASSERT_EQ(expected_header1.length(), buf_ofs);

    memset(buffer, 'a', sizeof(buffer));
    buffer[sizeof(buffer) - 1]    = '\0';
    const string expected_header2 = "\nkey2: value2";

    buf_ofs = 0;
    ASSERT_TRUE(http_header_generate_string(&ctx, &last_call, this->m_header, &state, buffer, buffer_len, &buf_ofs));
    ASSERT_EQ(expected_header2, string(buffer));
    ASSERT_EQ(expected_header2.length(), buf_ofs);

    memset(buffer, 'a', sizeof(buffer));
    buffer[sizeof(buffer) - 1]    = '\0';
    const string expected_header3 = "34\r\n\r\n";

    buf_ofs = 0;
    ASSERT_TRUE(http_header_generate_string(&ctx, &last_call, this->m_header, &state, buffer, buffer_len, &buf_ofs));
    ASSERT_EQ(expected_header3, string(buffer));
    ASSERT_EQ(expected_header3.length(), buf_ofs);

    memset(buffer, 'a', sizeof(buffer));
    buffer[sizeof(buffer) - 1] = '\0';
    buf_ofs                    = 0;
    ASSERT_TRUE(http_header_generate_string(&ctx, &last_call, this->m_header, &state, buffer, buffer_len, &buf_ofs));
    ASSERT_EQ("", string(buffer));
    ASSERT_EQ(0, buf_ofs);
}
