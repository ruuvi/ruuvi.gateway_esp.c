/**
 * @file test_cjson_wrap.cpp
 * @author TheSomeMan
 * @date 2020-10-23
 * @copyright Ruuvi Innovations Ltd, license BSD-3-Clause.
 */

#include "cjson_wrap.h"
#include "gtest/gtest.h"
#include <string>
#include "os_malloc.h"

using namespace std;

/*** Google-test class implementation
 * *********************************************************************************/

class TestCJsonWrap;
static TestCJsonWrap *g_pTestClass;

class MemAllocTrace
{
    vector<void *> allocated_mem;

    std::vector<void *>::iterator
    find(void *p_mem)
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
    add(void *p_mem)
    {
        auto iter = find(p_mem);
        assert(iter == this->allocated_mem.end()); // p_mem was found in the list of allocated memory blocks
        this->allocated_mem.push_back(p_mem);
    }
    void
    remove(void *p_mem)
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

class TestCJsonWrap : public ::testing::Test
{
private:
protected:
    void
    SetUp() override
    {
        cjson_wrap_init();
        g_pTestClass = this;
    }

    void
    TearDown() override
    {
        g_pTestClass = nullptr;
    }

public:
    TestCJsonWrap();

    ~TestCJsonWrap() override;

    MemAllocTrace m_mem_alloc_trace;
    uint32_t      m_malloc_cnt;
    uint32_t      m_malloc_fail_on_cnt;
};

TestCJsonWrap::TestCJsonWrap()
    : m_malloc_cnt(0)
    , m_malloc_fail_on_cnt(0)
    , Test()
{
}

TestCJsonWrap::~TestCJsonWrap() = default;

extern "C" {

void *
os_malloc(const size_t size)
{
    if (++g_pTestClass->m_malloc_cnt == g_pTestClass->m_malloc_fail_on_cnt)
    {
        return nullptr;
    }
    void *p_mem = malloc(size);
    assert(nullptr != p_mem);
    g_pTestClass->m_mem_alloc_trace.add(p_mem);
    return p_mem;
}

void
os_free_internal(void *p_mem)
{
    g_pTestClass->m_mem_alloc_trace.remove(p_mem);
    free(p_mem);
}

void *
os_calloc(const size_t nmemb, const size_t size)
{
    if (++g_pTestClass->m_malloc_cnt == g_pTestClass->m_malloc_fail_on_cnt)
    {
        return nullptr;
    }
    void *p_mem = calloc(nmemb, size);
    assert(nullptr != p_mem);
    g_pTestClass->m_mem_alloc_trace.add(p_mem);
    return p_mem;
}

} // extern "C"

/*** Unit-Tests
 * *******************************************************************************************************/

TEST_F(TestCJsonWrap, test_free_null_json_str) // NOLINT
{
    cjson_wrap_str_t json_str = cjson_wrap_str_null();
    cjson_wrap_free_json_str(&json_str);
}

TEST_F(TestCJsonWrap, test_add_timestamp_1) // NOLINT
{
    cJSON *p_root = cJSON_CreateObject();
    ASSERT_NE(nullptr, p_root);
    ASSERT_TRUE(cjson_wrap_add_timestamp(p_root, "timestamp", 1));
    char *p_json_str = cJSON_PrintUnformatted(p_root);
    ASSERT_NE(nullptr, p_json_str);
    ASSERT_EQ(string("{\"timestamp\":\"1\"}"), string(p_json_str));
    cJSON_Delete(p_root);
    os_free(p_json_str);
    ASSERT_TRUE(this->m_mem_alloc_trace.is_empty());
}

TEST_F(TestCJsonWrap, test_add_timestamp_12345678) // NOLINT
{
    cJSON *p_root = cJSON_CreateObject();
    ASSERT_NE(nullptr, p_root);
    ASSERT_TRUE(cjson_wrap_add_timestamp(p_root, "timestamp", 12345678));
    char *p_json_str = cJSON_PrintUnformatted(p_root);
    ASSERT_NE(nullptr, p_json_str);
    ASSERT_EQ(string("{\"timestamp\":\"12345678\"}"), string(p_json_str));
    cJSON_Delete(p_root);
    os_free(p_json_str);
    ASSERT_TRUE(this->m_mem_alloc_trace.is_empty());
}

TEST_F(TestCJsonWrap, test_add_timestamp_0x7FFFFFFF) // NOLINT
{
    cJSON *p_root = cJSON_CreateObject();
    ASSERT_NE(nullptr, p_root);
    ASSERT_TRUE(cjson_wrap_add_timestamp(p_root, "timestamp", 0x7FFFFFFF));
    char *p_json_str = cJSON_PrintUnformatted(p_root);
    ASSERT_NE(nullptr, p_json_str);
    ASSERT_EQ(string("{\"timestamp\":\"2147483647\"}"), string(p_json_str));
    cJSON_Delete(p_root);
    os_free(p_json_str);
    ASSERT_TRUE(this->m_mem_alloc_trace.is_empty());
}

TEST_F(TestCJsonWrap, test_delete) // NOLINT
{
    cJSON *p_root = cJSON_CreateObject();
    ASSERT_NE(nullptr, p_root);
    cjson_wrap_delete(&p_root);
    ASSERT_EQ(nullptr, p_root);
    ASSERT_TRUE(this->m_mem_alloc_trace.is_empty());
}

TEST_F(TestCJsonWrap, test_print) // NOLINT
{
    cJSON *p_root = cJSON_CreateObject();
    ASSERT_NE(nullptr, p_root);
    ASSERT_TRUE(cjson_wrap_add_timestamp(p_root, "timestamp", 1));
    cjson_wrap_str_t p_json_str = cjson_wrap_print(p_root);
    ASSERT_NE(nullptr, p_json_str.p_str);
    ASSERT_EQ(string("{\n\t\"timestamp\":\t\"1\"\n}"), string(p_json_str.p_str));
    cJSON_Delete(p_root);
    cjson_wrap_free_json_str(&p_json_str);
    ASSERT_EQ(nullptr, p_json_str.p_str);
    ASSERT_TRUE(this->m_mem_alloc_trace.is_empty());
}

TEST_F(TestCJsonWrap, test_print_and_delete) // NOLINT
{
    cJSON *p_root = cJSON_CreateObject();
    ASSERT_NE(nullptr, p_root);
    ASSERT_TRUE(cjson_wrap_add_timestamp(p_root, "timestamp", 1));
    cjson_wrap_str_t p_json_str = cjson_wrap_print_and_delete(&p_root);
    ASSERT_NE(nullptr, p_json_str.p_str);
    ASSERT_EQ(nullptr, p_root);
    ASSERT_EQ(string("{\n\t\"timestamp\":\t\"1\"\n}"), string(p_json_str.p_str));
    cjson_wrap_free_json_str(&p_json_str);
    ASSERT_EQ(nullptr, p_json_str.p_str);
    ASSERT_TRUE(this->m_mem_alloc_trace.is_empty());
}

TEST_F(TestCJsonWrap, test_copy_string_val) // NOLINT
{
    cJSON *p_root = cJSON_CreateObject();
    ASSERT_NE(nullptr, p_root);
    cJSON_AddStringToObject(p_root, "attr", "value123");
    std::array<char, 80> buf {};
    ASSERT_TRUE(json_wrap_copy_string_val(p_root, "attr", buf.data(), buf.size()));
    ASSERT_EQ(string("value123"), string(buf.data()));
    cJSON_Delete(p_root);
    ASSERT_TRUE(this->m_mem_alloc_trace.is_empty());
}

TEST_F(TestCJsonWrap, test_copy_string_val_with_buf_overflow) // NOLINT
{
    cJSON *p_root = cJSON_CreateObject();
    ASSERT_NE(nullptr, p_root);
    cJSON_AddStringToObject(p_root, "attr", "value123");
    std::array<char, 4> buf {};
    ASSERT_TRUE(json_wrap_copy_string_val(p_root, "attr", buf.data(), buf.size()));
    ASSERT_EQ(string("val"), string(buf.data()));
    cJSON_Delete(p_root);
    ASSERT_TRUE(this->m_mem_alloc_trace.is_empty());
}

TEST_F(TestCJsonWrap, test_copy_string_val_not_exist) // NOLINT
{
    cJSON *p_root = cJSON_CreateObject();
    ASSERT_NE(nullptr, p_root);
    cJSON_AddStringToObject(p_root, "attr", "value123");
    std::array<char, 80> buf {};
    ASSERT_FALSE(json_wrap_copy_string_val(p_root, "attr2", buf.data(), buf.size()));
    cJSON_Delete(p_root);
    ASSERT_TRUE(this->m_mem_alloc_trace.is_empty());
}

TEST_F(TestCJsonWrap, test_copy_string_val_wrong_type) // NOLINT
{
    cJSON *p_root = cJSON_CreateObject();
    ASSERT_NE(nullptr, p_root);
    cJSON_AddBoolToObject(p_root, "attr", 1);
    std::array<char, 80> buf {};
    ASSERT_FALSE(json_wrap_copy_string_val(p_root, "attr", buf.data(), buf.size()));
    cJSON_Delete(p_root);
    ASSERT_TRUE(this->m_mem_alloc_trace.is_empty());
}

TEST_F(TestCJsonWrap, test_get_bool_val_true) // NOLINT
{
    cJSON *p_root = cJSON_CreateObject();
    ASSERT_NE(nullptr, p_root);
    cJSON_AddBoolToObject(p_root, "attr", (cJSON_bool) true);
    bool val = false;
    ASSERT_TRUE(json_wrap_get_bool_val(p_root, "attr", &val));
    ASSERT_TRUE(val);
    cJSON_Delete(p_root);
    ASSERT_TRUE(this->m_mem_alloc_trace.is_empty());
}

TEST_F(TestCJsonWrap, test_get_bool_val_false) // NOLINT
{
    cJSON *p_root = cJSON_CreateObject();
    ASSERT_NE(nullptr, p_root);
    cJSON_AddBoolToObject(p_root, "attr", (cJSON_bool) false);
    bool val = false;
    ASSERT_TRUE(json_wrap_get_bool_val(p_root, "attr", &val));
    ASSERT_FALSE(val);
    cJSON_Delete(p_root);
    ASSERT_TRUE(this->m_mem_alloc_trace.is_empty());
}

TEST_F(TestCJsonWrap, test_get_bool_val_not_exist) // NOLINT
{
    cJSON *p_root = cJSON_CreateObject();
    ASSERT_NE(nullptr, p_root);
    cJSON_AddBoolToObject(p_root, "attr", (cJSON_bool) false);
    bool val = false;
    ASSERT_FALSE(json_wrap_get_bool_val(p_root, "attr2", &val));
    cJSON_Delete(p_root);
    ASSERT_TRUE(this->m_mem_alloc_trace.is_empty());
}

TEST_F(TestCJsonWrap, test_get_bool_val_wrong_type) // NOLINT
{
    cJSON *p_root = cJSON_CreateObject();
    ASSERT_NE(nullptr, p_root);
    cJSON_AddNumberToObject(p_root, "attr", 123.0);
    bool val = false;
    ASSERT_FALSE(json_wrap_get_bool_val(p_root, "attr", &val));
    cJSON_Delete(p_root);
    ASSERT_TRUE(this->m_mem_alloc_trace.is_empty());
}

TEST_F(TestCJsonWrap, test_get_uint16_val_0) // NOLINT
{
    cJSON *p_root = cJSON_CreateObject();
    ASSERT_NE(nullptr, p_root);
    cJSON_AddNumberToObject(p_root, "attr", 0.0);
    uint16_t val = 0;
    ASSERT_TRUE(json_wrap_get_uint16_val(p_root, "attr", &val));
    ASSERT_EQ(0, val);
    cJSON_Delete(p_root);
    ASSERT_TRUE(this->m_mem_alloc_trace.is_empty());
}

TEST_F(TestCJsonWrap, test_get_uint16_val_1) // NOLINT
{
    cJSON *p_root = cJSON_CreateObject();
    ASSERT_NE(nullptr, p_root);
    cJSON_AddNumberToObject(p_root, "attr", 1.0);
    uint16_t val = 0;
    ASSERT_TRUE(json_wrap_get_uint16_val(p_root, "attr", &val));
    ASSERT_EQ(1, val);
    cJSON_Delete(p_root);
    ASSERT_TRUE(this->m_mem_alloc_trace.is_empty());
}

TEST_F(TestCJsonWrap, test_get_uint16_val_65535) // NOLINT
{
    cJSON *p_root = cJSON_CreateObject();
    ASSERT_NE(nullptr, p_root);
    cJSON_AddNumberToObject(p_root, "attr", 65535.0);
    uint16_t val = 0;
    ASSERT_TRUE(json_wrap_get_uint16_val(p_root, "attr", &val));
    ASSERT_EQ(65535, val);
    cJSON_Delete(p_root);
    ASSERT_TRUE(this->m_mem_alloc_trace.is_empty());
}

TEST_F(TestCJsonWrap, test_get_uint16_val_minus_1) // NOLINT
{
    cJSON *p_root = cJSON_CreateObject();
    ASSERT_NE(nullptr, p_root);
    cJSON_AddNumberToObject(p_root, "attr", -1.0);
    uint16_t val = 0;
    ASSERT_FALSE(json_wrap_get_uint16_val(p_root, "attr", &val));
    cJSON_Delete(p_root);
    ASSERT_TRUE(this->m_mem_alloc_trace.is_empty());
}

TEST_F(TestCJsonWrap, test_get_uint16_val_65536) // NOLINT
{
    cJSON *p_root = cJSON_CreateObject();
    ASSERT_NE(nullptr, p_root);
    cJSON_AddNumberToObject(p_root, "attr", 65536.0);
    uint16_t val = 0;
    ASSERT_FALSE(json_wrap_get_uint16_val(p_root, "attr", &val));
    cJSON_Delete(p_root);
    ASSERT_TRUE(this->m_mem_alloc_trace.is_empty());
}

TEST_F(TestCJsonWrap, test_get_uint16_val_wrong_type) // NOLINT
{
    cJSON *p_root = cJSON_CreateObject();
    ASSERT_NE(nullptr, p_root);
    cJSON_AddStringToObject(p_root, "attr", "1");
    uint16_t val = 0;
    ASSERT_FALSE(json_wrap_get_uint16_val(p_root, "attr", &val));
    cJSON_Delete(p_root);
    ASSERT_TRUE(this->m_mem_alloc_trace.is_empty());
}

TEST_F(TestCJsonWrap, test_get_uint16_val_not_found) // NOLINT
{
    cJSON *p_root = cJSON_CreateObject();
    ASSERT_NE(nullptr, p_root);
    cJSON_AddNumberToObject(p_root, "attr", 0);
    uint16_t val = 0;
    ASSERT_FALSE(json_wrap_get_uint16_val(p_root, "attr2", &val));
    cJSON_Delete(p_root);
    ASSERT_TRUE(this->m_mem_alloc_trace.is_empty());
}
