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
    ASSERT_EQ(0, http_header_count(this->m_header));
    char buffer[64];
    memset(buffer, 'a', sizeof(buffer));
    buffer[sizeof(buffer) - 1] = '\0';
    int buffer_len             = sizeof(buffer);
    ASSERT_EQ(0, http_header_generate_string(this->m_header, 0, buffer, &buffer_len));
    assert(buffer_len < sizeof(buffer));
    ASSERT_EQ(0, buffer_len);
    ASSERT_EQ(string(""), string(buffer));
}

TEST_F(TestHttpHeader, test_one_item) // NOLINT
{
    char buffer[64];

    ASSERT_EQ(0, http_header_count(this->m_header));
    http_header_set(this->m_header, "key1", "value1");
    ASSERT_EQ(1, http_header_count(this->m_header));

    memset(buffer, 'a', sizeof(buffer));
    buffer[sizeof(buffer) - 1] = '\0';
    int buffer_len             = sizeof(buffer);
    ASSERT_EQ(1, http_header_generate_string(this->m_header, 0, buffer, &buffer_len));
    const string expected_header = "key1: value1\r\n\r\n";
    ASSERT_EQ(expected_header, string(buffer));
    ASSERT_EQ(expected_header.length(), buffer_len);

    memset(buffer, 'a', sizeof(buffer));
    buffer[sizeof(buffer) - 1] = '\0';
    buffer_len                 = sizeof(buffer);
    ASSERT_EQ(0, http_header_generate_string(this->m_header, 1, buffer, &buffer_len));
    ASSERT_EQ(0, buffer_len);
}

TEST_F(TestHttpHeader, test_one_item_buffer_overflow) // NOLINT
{
    char buffer[16];

    ASSERT_EQ(0, http_header_count(this->m_header));
    http_header_set(this->m_header, "key1", "value1");
    ASSERT_EQ(1, http_header_count(this->m_header));

    memset(buffer, 'a', sizeof(buffer));
    buffer[sizeof(buffer) - 1] = '\0';
    int buffer_len             = sizeof(buffer);
    ASSERT_EQ(0, http_header_generate_string(this->m_header, 0, buffer, &buffer_len));
    ASSERT_EQ("", string(buffer));
    ASSERT_EQ(0, buffer_len);
}

TEST_F(TestHttpHeader, test_two_items) // NOLINT
{
    char buffer[64];

    ASSERT_EQ(0, http_header_count(this->m_header));
    ASSERT_EQ(ESP_OK, http_header_set(this->m_header, "key1", "value1"));
    ASSERT_EQ(ESP_OK, http_header_set(this->m_header, "key2", "value2"));
    ASSERT_EQ(2, http_header_count(this->m_header));

    memset(buffer, 'a', sizeof(buffer));
    buffer[sizeof(buffer) - 1] = '\0';
    int buffer_len             = sizeof(buffer);
    ASSERT_EQ(2, http_header_generate_string(this->m_header, 0, buffer, &buffer_len));
    const string expected_header = "key1: value1\r\nkey2: value2\r\n\r\n";
    ASSERT_EQ(expected_header, string(buffer));
    ASSERT_EQ(expected_header.length(), buffer_len);

    memset(buffer, 'a', sizeof(buffer));
    buffer[sizeof(buffer) - 1] = '\0';
    buffer_len                 = sizeof(buffer);
    ASSERT_EQ(0, http_header_generate_string(this->m_header, 2, buffer, &buffer_len));
    ASSERT_EQ(0, buffer_len);
}

TEST_F(TestHttpHeader, test_two_items_split) // NOLINT
{
    char buffer[17];

    ASSERT_EQ(0, http_header_count(this->m_header));
    ASSERT_EQ(ESP_OK, http_header_set(this->m_header, "key1", "value1"));
    ASSERT_EQ(ESP_OK, http_header_set(this->m_header, "key2", "value2"));
    ASSERT_EQ(2, http_header_count(this->m_header));

    memset(buffer, 'a', sizeof(buffer));
    buffer[sizeof(buffer) - 1] = '\0';
    int buffer_len             = sizeof(buffer);
    ASSERT_EQ(1, http_header_generate_string(this->m_header, 0, buffer, &buffer_len));
    const string expected_header1 = "key1: value1\r\n";
    ASSERT_EQ(expected_header1, string(buffer));
    ASSERT_EQ(expected_header1.length(), buffer_len);

    memset(buffer, 'a', sizeof(buffer));
    buffer[sizeof(buffer) - 1] = '\0';
    buffer_len                 = sizeof(buffer);
    ASSERT_EQ(2, http_header_generate_string(this->m_header, 1, buffer, &buffer_len));
    const string expected_header2 = "key2: value2\r\n\r\n";
    ASSERT_EQ(expected_header2, string(buffer));
    ASSERT_EQ(expected_header2.length(), buffer_len);

    memset(buffer, 'a', sizeof(buffer));
    buffer[sizeof(buffer) - 1] = '\0';
    buffer_len                 = sizeof(buffer);
    ASSERT_EQ(0, http_header_generate_string(this->m_header, 2, buffer, &buffer_len));
    ASSERT_EQ(0, buffer_len);
}

TEST_F(TestHttpHeader, test_two_items_buffer_overflow) // NOLINT
{
    char buffer[16];

    ASSERT_EQ(0, http_header_count(this->m_header));
    ASSERT_EQ(ESP_OK, http_header_set(this->m_header, "key1", "val_a"));
    ASSERT_EQ(ESP_OK, http_header_set(this->m_header, "key2", "value2"));
    ASSERT_EQ(2, http_header_count(this->m_header));

    memset(buffer, 'a', sizeof(buffer));
    buffer[sizeof(buffer) - 1] = '\0';
    int buffer_len             = sizeof(buffer);
    ASSERT_EQ(1, http_header_generate_string(this->m_header, 0, buffer, &buffer_len));
    const string expected_header1 = "key1: val_a\r\n";
    ASSERT_EQ(expected_header1, string(buffer));
    ASSERT_EQ(expected_header1.length(), buffer_len);

    memset(buffer, 'a', sizeof(buffer));
    buffer[sizeof(buffer) - 1] = '\0';
    buffer_len                 = sizeof(buffer);
    ASSERT_EQ(1, http_header_generate_string(this->m_header, 1, buffer, &buffer_len));
    ASSERT_EQ("", string(buffer));
    ASSERT_EQ(0, buffer_len);
}
