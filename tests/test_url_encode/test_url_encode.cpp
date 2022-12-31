/**
 * @file test_url_encode.cpp
 * @author TheSomeMan
 * @date 2022-12-22
 * @copyright Ruuvi Innovations Ltd, license BSD-3-Clause.
 */

#include "url_encode.h"
#include "str_buf.h"
#include "gtest/gtest.h"
#include <string>
#include "os_malloc.h"

using namespace std;

/*** Google-test class implementation
 * *********************************************************************************/

class TestUrlEncode;
static TestUrlEncode* g_pTestClass;

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

class TestUrlEncode : public ::testing::Test
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
    TestUrlEncode();

    ~TestUrlEncode() override;

    MemAllocTrace m_mem_alloc_trace;
    uint32_t      m_malloc_cnt {};
    uint32_t      m_malloc_fail_on_cnt {};
    char*         m_p_str {};
};

TestUrlEncode::TestUrlEncode()
    : Test()
{
}

TestUrlEncode::~TestUrlEncode() = default;

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

TEST_F(TestUrlEncode, test_empty) // NOLINT
{
    const str_buf_t val_encoded = url_encode_with_alloc("");
    ASSERT_NE(nullptr, val_encoded.buf);
    ASSERT_FALSE(str_buf_is_overflow(&val_encoded));
    ASSERT_EQ(string(""), string(val_encoded.buf));

    const str_buf_t val_decoded = url_decode_with_alloc(val_encoded.buf);
    ASSERT_NE(nullptr, val_decoded.buf);
    ASSERT_FALSE(str_buf_is_overflow(&val_decoded));
    ASSERT_EQ(string(""), string(val_decoded.buf));
}

TEST_F(TestUrlEncode, test_ascii) // NOLINT
{
    const str_buf_t val_encoded = url_encode_with_alloc("abcABC");
    ASSERT_NE(nullptr, val_encoded.buf);
    ASSERT_FALSE(str_buf_is_overflow(&val_encoded));
    ASSERT_EQ(string("abcABC"), string(val_encoded.buf));

    const str_buf_t val_decoded = url_decode_with_alloc(val_encoded.buf);
    ASSERT_NE(nullptr, val_decoded.buf);
    ASSERT_FALSE(str_buf_is_overflow(&val_decoded));
    ASSERT_EQ(string("abcABC"), string(val_decoded.buf));
}

TEST_F(TestUrlEncode, test_url) // NOLINT
{
    const str_buf_t val_encoded = url_encode_with_alloc("https://test.com:8000/");
    ASSERT_NE(nullptr, val_encoded.buf);
    ASSERT_FALSE(str_buf_is_overflow(&val_encoded));
    ASSERT_EQ(string("https%3A%2F%2Ftest.com%3A8000%2F"), string(val_encoded.buf));

    const str_buf_t val_decoded = url_decode_with_alloc(val_encoded.buf);
    ASSERT_NE(nullptr, val_decoded.buf);
    ASSERT_FALSE(str_buf_is_overflow(&val_decoded));
    ASSERT_EQ(string("https://test.com:8000/"), string(val_decoded.buf));
}

TEST_F(TestUrlEncode, test_decode_fail_1) // NOLINT
{
    const string    val_encoded = string("%3");
    const str_buf_t val_decoded = url_decode_with_alloc(val_encoded.c_str());
    ASSERT_EQ(nullptr, val_decoded.buf);
}

TEST_F(TestUrlEncode, test_decode_fail_2) // NOLINT
{
    const string    val_encoded = string("%");
    const str_buf_t val_decoded = url_decode_with_alloc(val_encoded.c_str());
    ASSERT_EQ(nullptr, val_decoded.buf);
}

TEST_F(TestUrlEncode, test_decode_fail_3) // NOLINT
{
    const string    val_encoded = string("%3Q");
    const str_buf_t val_decoded = url_decode_with_alloc(val_encoded.c_str());
    ASSERT_EQ(nullptr, val_decoded.buf);
}
