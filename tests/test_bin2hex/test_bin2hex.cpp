/**
 * @file test_bin2hex.cpp
 * @author TheSomeMan
 * @date 2020-08-27
 * @copyright Ruuvi Innovations Ltd, license BSD-3-Clause.
 */

#include "bin2hex.h"
#include "gtest/gtest.h"
#include <string>
#include "os_malloc.h"

using namespace std;

/*** Google-test class implementation
 * *********************************************************************************/

class TestBin2Hex;
static TestBin2Hex* g_pTestClass;

class MemAllocTrace
{
    vector<void*> allocated_mem;

    std::vector<void*>::iterator
    find(void* p_mem)
    {
        for (auto iter = this->allocated_mem.begin(); iter != this->allocated_mem.end(); ++iter)
        {
            if (*iter == p_mem)
            {
                return iter;
            }
        }
        return this->allocated_mem.end();
    }

public:
    void
    add(void* p_mem)
    {
        auto iter = find(p_mem);
        assert(iter == this->allocated_mem.end()); // p_mem was found in the list of allocated memory blocks
        this->allocated_mem.push_back(p_mem);
    }
    void
    remove(void* p_mem)
    {
        auto iter = find(p_mem);
        assert(iter != this->allocated_mem.end()); // p_mem was not found in the list of allocated memory blocks
        this->allocated_mem.erase(iter);
    }
    bool
    is_empty()
    {
        return this->allocated_mem.empty();
    }
};

class TestBin2Hex : public ::testing::Test
{
private:
protected:
    void
    SetUp() override
    {
        g_pTestClass               = this;
        this->m_malloc_cnt         = 0;
        this->m_malloc_fail_on_cnt = 0;
    }

    void
    TearDown() override
    {
        g_pTestClass = nullptr;
        if (nullptr != this->m_p_str)
        {
            os_free(this->m_p_str);
            this->m_p_str = nullptr;
        }
    }

public:
    TestBin2Hex();

    ~TestBin2Hex() override;

    MemAllocTrace m_mem_alloc_trace;
    uint32_t      m_malloc_cnt {};
    uint32_t      m_malloc_fail_on_cnt {};
    char*         m_p_str {};
};

TestBin2Hex::TestBin2Hex()
    : Test()
{
}

TestBin2Hex::~TestBin2Hex() = default;

extern "C" {

void*
os_malloc(const size_t size)
{
    assert(nullptr != g_pTestClass);
    if (++g_pTestClass->m_malloc_cnt == g_pTestClass->m_malloc_fail_on_cnt)
    {
        return nullptr;
    }
    void* p_mem = malloc(size);
    assert(nullptr != p_mem);
    g_pTestClass->m_mem_alloc_trace.add(p_mem);
    return p_mem;
}

void
os_free_internal(void* p_mem)
{
    assert(nullptr != g_pTestClass);
    g_pTestClass->m_mem_alloc_trace.remove(p_mem);
    free(p_mem);
}

void*
os_calloc(const size_t nmemb, const size_t size)
{
    assert(nullptr != g_pTestClass);
    if (++g_pTestClass->m_malloc_cnt == g_pTestClass->m_malloc_fail_on_cnt)
    {
        return nullptr;
    }
    void* p_mem = calloc(nmemb, size);
    assert(nullptr != p_mem);
    g_pTestClass->m_mem_alloc_trace.add(p_mem);
    return p_mem;
}

} // extern "C"

/*** Unit-Tests
 * *******************************************************************************************************/

TEST_F(TestBin2Hex, test_1) // NOLINT
{
    const size_t                       hex_str_buf_size = 10;
    std::array<char, hex_str_buf_size> hex_str_buf {};
    const std::array<uint8_t, 1>       bin_buf = { 0x01 };
    str_buf_t                          str_buf = STR_BUF_INIT(hex_str_buf.data(), hex_str_buf.size());

    bin2hex(&str_buf, bin_buf.data(), bin_buf.size());
    ASSERT_EQ(string("01"), string(hex_str_buf.data()));
}

TEST_F(TestBin2Hex, test_2) // NOLINT
{
    const size_t                       hex_str_buf_size = 10;
    std::array<char, hex_str_buf_size> hex_str_buf {};
    const std::array<uint8_t, 2>       bin_buf = { 0x01, 0xAA };
    str_buf_t                          str_buf = STR_BUF_INIT(hex_str_buf.data(), hex_str_buf.size());

    bin2hex(&str_buf, bin_buf.data(), bin_buf.size());
    ASSERT_EQ(string("01AA"), string(hex_str_buf.data()));
}

TEST_F(TestBin2Hex, test_empty_data) // NOLINT
{
    const size_t                       hex_str_buf_size = 10;
    std::array<char, hex_str_buf_size> hex_str_buf {};
    const std::array<uint8_t, 0>       bin_buf = {};
    memset(hex_str_buf.data(), 'a', hex_str_buf.size());
    str_buf_t str_buf = STR_BUF_INIT(hex_str_buf.data(), hex_str_buf.size());

    bin2hex(&str_buf, bin_buf.data(), bin_buf.size());
    ASSERT_EQ(string(""), string(hex_str_buf.data()));
}

TEST_F(TestBin2Hex, test_5_with_overflow_size_9) // NOLINT
{
    const size_t                       hex_str_buf_size = 9;
    std::array<char, hex_str_buf_size> hex_str_buf {};
    const std::array<uint8_t, 5>       bin_buf = { 0x01, 0x02, 0x03, 0x04, 0x05 };
    str_buf_t                          str_buf = STR_BUF_INIT(hex_str_buf.data(), hex_str_buf.size());

    bin2hex(&str_buf, bin_buf.data(), bin_buf.size());
    ASSERT_EQ(string("01020304"), string(hex_str_buf.data()));
}

TEST_F(TestBin2Hex, test_5_with_overflow_size_10) // NOLINT
{
    const size_t                       hex_str_buf_size = 10;
    std::array<char, hex_str_buf_size> hex_str_buf {};
    const std::array<uint8_t, 5>       bin_buf = { 0x01, 0x02, 0x03, 0x04, 0x05 };
    str_buf_t                          str_buf = STR_BUF_INIT(hex_str_buf.data(), hex_str_buf.size());

    bin2hex(&str_buf, bin_buf.data(), bin_buf.size());
    ASSERT_EQ(string("01020304"), string(hex_str_buf.data()));
}

TEST_F(TestBin2Hex, test_5_without_overflow_size_11) // NOLINT
{
    const size_t                       hex_str_buf_size = 11;
    std::array<char, hex_str_buf_size> hex_str_buf {};
    const std::array<uint8_t, 5>       bin_buf = { 0x01, 0x02, 0x03, 0x04, 0x05 };
    str_buf_t                          str_buf = STR_BUF_INIT(hex_str_buf.data(), hex_str_buf.size());

    bin2hex(&str_buf, bin_buf.data(), bin_buf.size());
    ASSERT_EQ(string("0102030405"), string(hex_str_buf.data()));
}

TEST_F(TestBin2Hex, test_bin2hex_with_malloc_success) // NOLINT
{
    const std::array<uint8_t, 5> bin_buf = { 0x01, 0x02, 0x03, 0x04, 0x05 };

    char* p_str = bin2hex_with_malloc(bin_buf.data(), bin_buf.size());
    ASSERT_EQ(string("0102030405"), string(p_str));
    os_free(p_str);
    ASSERT_TRUE(this->m_mem_alloc_trace.is_empty());
}

TEST_F(TestBin2Hex, test_bin2hex_with_malloc_fail) // NOLINT
{
    const std::array<uint8_t, 5> bin_buf = { 0x01, 0x02, 0x03, 0x04, 0x05 };

    this->m_malloc_fail_on_cnt = 1;

    char* p_str = bin2hex_with_malloc(bin_buf.data(), bin_buf.size());
    ASSERT_EQ(nullptr, p_str);
    ASSERT_TRUE(this->m_mem_alloc_trace.is_empty());
}

TEST_F(TestBin2Hex, test_hex2bin_with_malloc_success) // NOLINT
{
    const std::array<uint8_t, 5> bin_buf = { 0x01, 0xAC, 0x03, 0x04, 0xFF };

    size_t   length = 0;
    uint8_t* p_buf  = hex2bin_with_malloc("01AC0304FF", &length);
    ASSERT_EQ(5, length);
    for (size_t i = 0; i < length; i++)
    {
        ASSERT_EQ(bin_buf[i], p_buf[i]) << "Buffers differ at index " << i;
    }
    os_free(p_buf);
    ASSERT_TRUE(this->m_mem_alloc_trace.is_empty());
}

TEST_F(TestBin2Hex, test_hex2bin_with_malloc_fail_bad_length) // NOLINT
{
    size_t   length = 1;
    uint8_t* p_buf  = hex2bin_with_malloc("01A", &length);
    ASSERT_EQ(0, length);
    ASSERT_EQ(nullptr, p_buf);
    ASSERT_TRUE(this->m_mem_alloc_trace.is_empty());
}

TEST_F(TestBin2Hex, test_hex2bin_with_malloc_fail_bad_hex_string) // NOLINT
{
    size_t   length = 0;
    uint8_t* p_buf  = hex2bin_with_malloc("01XX", &length);
    ASSERT_EQ(2, length);
    ASSERT_EQ(nullptr, p_buf);
    ASSERT_TRUE(this->m_mem_alloc_trace.is_empty());
}

TEST_F(TestBin2Hex, test_hex2bin_with_malloc_fail_on_malloc) // NOLINT
{
    this->m_malloc_fail_on_cnt = 1;

    size_t   length = 0;
    uint8_t* p_buf  = hex2bin_with_malloc("0102", &length);
    ASSERT_EQ(2, length);
    ASSERT_EQ(nullptr, p_buf);
    ASSERT_TRUE(this->m_mem_alloc_trace.is_empty());
}
