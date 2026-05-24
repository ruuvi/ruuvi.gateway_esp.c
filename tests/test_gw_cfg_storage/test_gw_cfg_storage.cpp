/**
 * @file test_gw_cfg_storage.cpp
 * @author TheSomeMan
 * @date 2026-05-24
 * @copyright Ruuvi Innovations Ltd, license BSD-3-Clause.
 */

#include <cstring>
#include <map>
#include <string>
#include <vector>
#include "gtest/gtest.h"
#include "freertos/FreeRTOS.h"
#include "gw_cfg_storage.h"
#include "esp_log_wrapper.hpp"
#include "os_task.h"
#include "nvs.h"

using namespace std;

/*** Google-test class implementation
 * *********************************************************************************/

class TestGwCfgStorage;
static TestGwCfgStorage* g_pTestClass;

static const nvs_handle_t MOCK_NVS_VALID_HANDLE = 0xABCDEF42U;

extern "C" {

const char*
os_task_get_name(void)
{
    static const char g_task_name[] = "main";
    return const_cast<char*>(g_task_name);
}

os_task_priority_t
os_task_get_priority(void)
{
    return 0;
}

} // extern "C"

class MemAllocTrace
{
    vector<void*> allocated_mem;

    std::vector<void*>::iterator
    find(void* ptr)
    {
        for (auto iter = this->allocated_mem.begin(); iter != this->allocated_mem.end(); ++iter)
        {
            if (*iter == ptr)
            {
                return iter;
            }
        }
        return this->allocated_mem.end();
    }

public:
    void
    add(void* ptr)
    {
        auto iter = find(ptr);
        assert(iter == this->allocated_mem.end()); // ptr was found in the list of allocated memory blocks
        this->allocated_mem.push_back(ptr);
    }

    void
    remove(void* ptr)
    {
        auto iter = find(ptr);
        assert(iter != this->allocated_mem.end()); // ptr was not found in the list of allocated memory blocks
        this->allocated_mem.erase(iter);
    }

    bool
    is_empty()
    {
        return this->allocated_mem.empty();
    }

    void
    clear()
    {
        this->allocated_mem.clear();
    }
};

class TestGwCfgStorage : public ::testing::Test
{
private:
protected:
    void
    SetUp() override
    {
        esp_log_wrapper_init();
        g_pTestClass = this;

        this->m_mem_alloc_trace.clear();
        this->m_malloc_cnt         = 0;
        this->m_malloc_fail_on_cnt = 0;

        this->m_nvs_storage.clear();
        this->m_nvs_open_fail             = false;
        this->m_nvs_get_str_call_cnt      = 0;
        this->m_nvs_get_str_fail_on_cnt   = 0;
        this->m_nvs_get_blob_call_cnt     = 0;
        this->m_nvs_get_blob_fail_on_cnt  = 0;
        this->m_nvs_set_str_fail          = false;
        this->m_nvs_set_blob_fail         = false;
        this->m_nvs_erase_key_fail        = false;
        this->m_nvs_close_call_cnt        = 0;
        this->m_ruuvi_nvs_deinit_call_cnt = 0;
        this->m_ruuvi_nvs_erase_call_cnt  = 0;
        this->m_ruuvi_nvs_init_call_cnt   = 0;
    }

    void
    TearDown() override
    {
        g_pTestClass = nullptr;
        esp_log_wrapper_deinit();
    }

public:
    TestGwCfgStorage();

    ~TestGwCfgStorage() override;

    MemAllocTrace m_mem_alloc_trace;
    uint32_t      m_malloc_cnt {};
    uint32_t      m_malloc_fail_on_cnt {};

    // Mock NVS in-memory storage (key -> data)
    std::map<std::string, std::string> m_nvs_storage;

    // Mock control flags
    bool     m_nvs_open_fail {};
    uint32_t m_nvs_get_str_call_cnt {};
    uint32_t m_nvs_get_str_fail_on_cnt {};
    uint32_t m_nvs_get_blob_call_cnt {};
    uint32_t m_nvs_get_blob_fail_on_cnt {};
    bool     m_nvs_set_str_fail {};
    bool     m_nvs_set_blob_fail {};
    bool     m_nvs_erase_key_fail {};

    // Tracking counters
    uint32_t m_nvs_close_call_cnt {};
    uint32_t m_ruuvi_nvs_deinit_call_cnt {};
    uint32_t m_ruuvi_nvs_erase_call_cnt {};
    uint32_t m_ruuvi_nvs_init_call_cnt {};
};

TestGwCfgStorage::TestGwCfgStorage()
    : Test()
{
}

extern "C" {

void*
os_malloc(const size_t size)
{
    if (++g_pTestClass->m_malloc_cnt == g_pTestClass->m_malloc_fail_on_cnt)
    {
        return nullptr;
    }
    void* ptr = malloc(size);
    assert(nullptr != ptr);
    g_pTestClass->m_mem_alloc_trace.add(ptr);
    return ptr;
}

void
os_free_internal(void* ptr)
{
    g_pTestClass->m_mem_alloc_trace.remove(ptr);
    free(ptr);
}

void*
os_calloc(const size_t nmemb, const size_t size)
{
    if (++g_pTestClass->m_malloc_cnt == g_pTestClass->m_malloc_fail_on_cnt)
    {
        return nullptr;
    }
    void* ptr = calloc(nmemb, size);
    assert(nullptr != ptr);
    g_pTestClass->m_mem_alloc_trace.add(ptr);
    return ptr;
}

const char*
wrap_esp_err_to_name_r(const esp_err_t code, char* const p_buf, const size_t buf_len)
{
    (void)snprintf(p_buf, buf_len, "Error 0x%x(%d)", code, code);
    return p_buf;
}

bool
ruuvi_nvs_init_gw_cfg_storage(void)
{
    g_pTestClass->m_ruuvi_nvs_init_call_cnt++;
    return true;
}

bool
ruuvi_nvs_deinit_gw_cfg_storage(void)
{
    g_pTestClass->m_ruuvi_nvs_deinit_call_cnt++;
    return true;
}

void
ruuvi_nvs_erase_gw_cfg_storage(void)
{
    g_pTestClass->m_ruuvi_nvs_erase_call_cnt++;
    g_pTestClass->m_nvs_storage.clear();
}

bool
ruuvi_nvs_open_gw_cfg_storage(nvs_open_mode_t open_mode, nvs_handle_t* p_handle)
{
    (void)open_mode;
    if (g_pTestClass->m_nvs_open_fail)
    {
        return false;
    }
    *p_handle = MOCK_NVS_VALID_HANDLE;
    return true;
}

void
nvs_close(nvs_handle_t handle)
{
    assert(MOCK_NVS_VALID_HANDLE == handle);
    g_pTestClass->m_nvs_close_call_cnt++;
}

esp_err_t
nvs_get_str(nvs_handle_t handle, const char* key, char* out_value, size_t* length)
{
    (void)handle;
    g_pTestClass->m_nvs_get_str_call_cnt++;
    if ((g_pTestClass->m_nvs_get_str_fail_on_cnt != 0)
        && (g_pTestClass->m_nvs_get_str_call_cnt == g_pTestClass->m_nvs_get_str_fail_on_cnt))
    {
        return ESP_ERR_NVS_NOT_FOUND;
    }
    const auto it = g_pTestClass->m_nvs_storage.find(key);
    if (it == g_pTestClass->m_nvs_storage.end())
    {
        return ESP_ERR_NVS_NOT_FOUND;
    }
    const std::string& val = it->second;
    *length                = val.size() + 1U; // includes null terminator
    if (nullptr != out_value)
    {
        memcpy(out_value, val.c_str(), val.size() + 1U);
    }
    return ESP_OK;
}

esp_err_t
nvs_set_str(nvs_handle_t handle, const char* key, const char* value)
{
    (void)handle;
    if (g_pTestClass->m_nvs_set_str_fail)
    {
        return ESP_ERR_NVS_NOT_ENOUGH_SPACE;
    }
    g_pTestClass->m_nvs_storage[key] = std::string(value);
    return ESP_OK;
}

esp_err_t
nvs_get_blob(nvs_handle_t handle, const char* key, void* out_value, size_t* length)
{
    (void)handle;
    g_pTestClass->m_nvs_get_blob_call_cnt++;
    if ((g_pTestClass->m_nvs_get_blob_fail_on_cnt != 0)
        && (g_pTestClass->m_nvs_get_blob_call_cnt == g_pTestClass->m_nvs_get_blob_fail_on_cnt))
    {
        return ESP_ERR_NVS_NOT_FOUND;
    }
    const auto it = g_pTestClass->m_nvs_storage.find(key);
    if (it == g_pTestClass->m_nvs_storage.end())
    {
        return ESP_ERR_NVS_NOT_FOUND;
    }
    const std::string& val = it->second;
    *length                = val.size(); // size without null terminator for blobs
    if (nullptr != out_value)
    {
        memcpy(out_value, val.data(), val.size());
    }
    return ESP_OK;
}

esp_err_t
nvs_set_blob(nvs_handle_t handle, const char* key, const void* value, size_t length)
{
    (void)handle;
    if (g_pTestClass->m_nvs_set_blob_fail)
    {
        return ESP_ERR_NVS_NOT_ENOUGH_SPACE;
    }
    g_pTestClass->m_nvs_storage[key] = std::string(static_cast<const char*>(value), length);
    return ESP_OK;
}

esp_err_t
nvs_erase_key(nvs_handle_t handle, const char* key)
{
    (void)handle;
    if (g_pTestClass->m_nvs_erase_key_fail)
    {
        return ESP_ERR_NVS_NOT_FOUND;
    }
    g_pTestClass->m_nvs_storage.erase(key);
    return ESP_OK;
}

} // extern "C"

TestGwCfgStorage::~TestGwCfgStorage() = default;

/*** Unit-Tests
 * *******************************************************************************************************/

// ============================================================================================
// gw_cfg_storage_check
// ============================================================================================

TEST_F(TestGwCfgStorage, test_gw_cfg_storage_check_success) // NOLINT
{
    const bool result = gw_cfg_storage_check();

    ASSERT_TRUE(result);
    ASSERT_EQ(1U, this->m_nvs_close_call_cnt);
    ASSERT_EQ("", esp_log_wrapper_get_logs());
    ASSERT_TRUE(this->m_mem_alloc_trace.is_empty());
}

TEST_F(TestGwCfgStorage, test_gw_cfg_storage_check_fail_nvs_open) // NOLINT
{
    this->m_nvs_open_fail = true;

    const bool result = gw_cfg_storage_check();

    ASSERT_FALSE(result);
    ASSERT_EQ(0U, this->m_nvs_close_call_cnt);
    ASSERT_EQ("E gw_cfg: ruuvi_nvs_open_gw_cfg_storage failed\n", esp_log_wrapper_get_logs());
    ASSERT_TRUE(this->m_mem_alloc_trace.is_empty());
}

// ============================================================================================
// gw_cfg_storage_check_file
// ============================================================================================

TEST_F(TestGwCfgStorage, test_gw_cfg_storage_check_file_string_not_found) // NOLINT
{
    size_t     file_size = 99U;
    const bool result    = gw_cfg_storage_check_file("test_file", false, &file_size);

    ASSERT_FALSE(result);
    ASSERT_EQ(99U, file_size); // should not have been modified
    ASSERT_EQ(1U, this->m_nvs_close_call_cnt);
    ASSERT_EQ(
        "I gw_cfg: Check file 'test_file' (string) in NVS\n"
        "I gw_cfg: Can't find config key 'test_file' in flash\n",
        esp_log_wrapper_get_logs());
    ASSERT_TRUE(this->m_mem_alloc_trace.is_empty());
}

TEST_F(TestGwCfgStorage, test_gw_cfg_storage_check_file_string_found) // NOLINT
{
    this->m_nvs_storage["test_file"] = "hello";

    size_t     file_size = 0U;
    const bool result    = gw_cfg_storage_check_file("test_file", false, &file_size);

    ASSERT_TRUE(result);
    ASSERT_EQ(6U, file_size); // "hello" + null terminator
    ASSERT_EQ(1U, this->m_nvs_close_call_cnt);
    ASSERT_EQ(
        "I gw_cfg: Check file 'test_file' (string) in NVS\n"
        "I gw_cfg: File 'test_file' (string) exists in NVS, size 6 bytes\n",
        esp_log_wrapper_get_logs());
    ASSERT_TRUE(this->m_mem_alloc_trace.is_empty());
}

TEST_F(TestGwCfgStorage, test_gw_cfg_storage_check_file_string_found_null_size_ptr) // NOLINT
{
    this->m_nvs_storage["test_file"] = "hello";

    const bool result = gw_cfg_storage_check_file("test_file", false, nullptr);

    ASSERT_TRUE(result);
    ASSERT_EQ(1U, this->m_nvs_close_call_cnt);
    ASSERT_EQ(
        "I gw_cfg: Check file 'test_file' (string) in NVS\n"
        "I gw_cfg: File 'test_file' (string) exists in NVS, size 6 bytes\n",
        esp_log_wrapper_get_logs());
    ASSERT_TRUE(this->m_mem_alloc_trace.is_empty());
}

TEST_F(TestGwCfgStorage, test_gw_cfg_storage_check_file_blob_not_found) // NOLINT
{
    size_t     file_size = 99U;
    const bool result    = gw_cfg_storage_check_file("test_blob", true, &file_size);

    ASSERT_FALSE(result);
    ASSERT_EQ(99U, file_size); // should not have been modified
    ASSERT_EQ(1U, this->m_nvs_close_call_cnt);
    ASSERT_EQ(
        "I gw_cfg: Check file 'test_blob' (blob) in NVS\n"
        "I gw_cfg: Can't find config key 'test_blob' in flash\n",
        esp_log_wrapper_get_logs());
    ASSERT_TRUE(this->m_mem_alloc_trace.is_empty());
}

TEST_F(TestGwCfgStorage, test_gw_cfg_storage_check_file_blob_found) // NOLINT
{
    this->m_nvs_storage["test_blob"] = "world";

    size_t     file_size = 0U;
    const bool result    = gw_cfg_storage_check_file("test_blob", true, &file_size);

    ASSERT_TRUE(result);
    ASSERT_EQ(5U, file_size); // "world" without null terminator (blob)
    ASSERT_EQ(1U, this->m_nvs_close_call_cnt);
    ASSERT_EQ(
        "I gw_cfg: Check file 'test_blob' (blob) in NVS\n"
        "I gw_cfg: File 'test_blob' (blob) exists in NVS, size 5 bytes\n",
        esp_log_wrapper_get_logs());
    ASSERT_TRUE(this->m_mem_alloc_trace.is_empty());
}

TEST_F(TestGwCfgStorage, test_gw_cfg_storage_check_file_fail_nvs_open) // NOLINT
{
    this->m_nvs_open_fail = true;

    size_t     file_size = 99U;
    const bool result    = gw_cfg_storage_check_file("test_file", false, &file_size);

    ASSERT_FALSE(result);
    ASSERT_EQ(99U, file_size); // should not be modified
    ASSERT_EQ(0U, this->m_nvs_close_call_cnt);
    ASSERT_EQ(
        "I gw_cfg: Check file 'test_file' (string) in NVS\n"
        "E gw_cfg: ruuvi_nvs_open_gw_cfg_storage failed\n",
        esp_log_wrapper_get_logs());
    ASSERT_TRUE(this->m_mem_alloc_trace.is_empty());
}

// ============================================================================================
// gw_cfg_storage_read_file_as_string
// ============================================================================================

TEST_F(TestGwCfgStorage, test_gw_cfg_storage_read_file_as_string_success) // NOLINT
{
    this->m_nvs_storage["cfg_file"] = "config_content";

    str_buf_t result = gw_cfg_storage_read_file_as_string("cfg_file");

    ASSERT_NE(nullptr, result.buf);
    ASSERT_EQ(string("config_content"), string(result.buf));
    ASSERT_EQ(1U, this->m_nvs_close_call_cnt);
    ASSERT_EQ(
        "I gw_cfg: Read file 'cfg_file' as string from NVS\n"
        "I gw_cfg: File 'cfg_file' (string) was successfully read from NVS\n",
        esp_log_wrapper_get_logs());

    str_buf_free_buf(&result);
    ASSERT_TRUE(this->m_mem_alloc_trace.is_empty());
}

TEST_F(TestGwCfgStorage, test_gw_cfg_storage_read_file_as_string_fail_nvs_open) // NOLINT
{
    this->m_nvs_open_fail = true;

    str_buf_t result = gw_cfg_storage_read_file_as_string("cfg_file");

    ASSERT_EQ(nullptr, result.buf);
    ASSERT_EQ(0U, this->m_nvs_close_call_cnt);
    ASSERT_EQ(
        "I gw_cfg: Read file 'cfg_file' as string from NVS\n"
        "E gw_cfg: ruuvi_nvs_open_gw_cfg_storage failed\n",
        esp_log_wrapper_get_logs());
    ASSERT_TRUE(this->m_mem_alloc_trace.is_empty());
}

TEST_F(TestGwCfgStorage, test_gw_cfg_storage_read_file_as_string_not_found) // NOLINT
{
    str_buf_t result = gw_cfg_storage_read_file_as_string("cfg_file");

    ASSERT_EQ(nullptr, result.buf);
    ASSERT_EQ(1U, this->m_nvs_close_call_cnt);
    ASSERT_EQ(
        "I gw_cfg: Read file 'cfg_file' as string from NVS\n"
        "E gw_cfg: Can't find string key 'cfg_file' in flash, err=4354 (Error 0x1102(4354))\n"
        "E gw_cfg: Failed to read file 'cfg_file' (string) from NVS\n",
        esp_log_wrapper_get_logs());
    ASSERT_TRUE(this->m_mem_alloc_trace.is_empty());
}

TEST_F(TestGwCfgStorage, test_gw_cfg_storage_read_file_as_string_malloc_fail) // NOLINT
{
    this->m_nvs_storage["my_key"] = "hello";
    this->m_malloc_fail_on_cnt    = 1; // fail first malloc (file content allocation)

    str_buf_t result = gw_cfg_storage_read_file_as_string("my_key");

    ASSERT_EQ(nullptr, result.buf);
    ASSERT_EQ(1U, this->m_nvs_close_call_cnt);
    ASSERT_EQ(
        "I gw_cfg: Read file 'my_key' as string from NVS\n"
        "E gw_cfg: Can't allocate 6 bytes for file\n"
        "E gw_cfg: Failed to read file 'my_key' (string) from NVS\n",
        esp_log_wrapper_get_logs());
    ASSERT_TRUE(this->m_mem_alloc_trace.is_empty());
}

TEST_F(TestGwCfgStorage, test_gw_cfg_storage_read_file_as_string_second_read_fail) // NOLINT
{
    this->m_nvs_storage["my_key"]   = "hello";
    this->m_nvs_get_str_fail_on_cnt = 2; // succeed on size probe, fail on actual read

    str_buf_t result = gw_cfg_storage_read_file_as_string("my_key");

    ASSERT_EQ(nullptr, result.buf);
    ASSERT_EQ(1U, this->m_nvs_close_call_cnt);
    ASSERT_EQ(
        "I gw_cfg: Read file 'my_key' as string from NVS\n"
        "E gw_cfg: Can't read string from NVS by key 'my_key', err=4354 (Error 0x1102(4354))\n"
        "E gw_cfg: Failed to read file 'my_key' (string) from NVS\n",
        esp_log_wrapper_get_logs());
    ASSERT_TRUE(this->m_mem_alloc_trace.is_empty());
}

// ============================================================================================
// gw_cfg_storage_read_file_as_blob
// ============================================================================================

TEST_F(TestGwCfgStorage, test_gw_cfg_storage_read_file_as_blob_success) // NOLINT
{
    this->m_nvs_storage["blob_file"] = "binary_data";

    str_buf_t result = gw_cfg_storage_read_file_as_blob("blob_file");

    ASSERT_NE(nullptr, result.buf);
    ASSERT_EQ(0, memcmp("binary_data", result.buf, 11));
    ASSERT_EQ(12U, result.size); // 11 data bytes + 1 null terminator
    ASSERT_EQ(1U, this->m_nvs_close_call_cnt);
    ASSERT_EQ(
        "I gw_cfg: Read file 'blob_file' as blob from NVS\n"
        "I gw_cfg: File 'blob_file' (blob) was successfully read from NVS\n",
        esp_log_wrapper_get_logs());

    str_buf_free_buf(&result);
    ASSERT_TRUE(this->m_mem_alloc_trace.is_empty());
}

TEST_F(TestGwCfgStorage, test_gw_cfg_storage_read_file_as_blob_fail_nvs_open) // NOLINT
{
    this->m_nvs_open_fail = true;

    str_buf_t result = gw_cfg_storage_read_file_as_blob("blob_file");

    ASSERT_EQ(nullptr, result.buf);
    ASSERT_EQ(0U, this->m_nvs_close_call_cnt);
    ASSERT_EQ(
        "I gw_cfg: Read file 'blob_file' as blob from NVS\n"
        "E gw_cfg: ruuvi_nvs_open_gw_cfg_storage failed\n",
        esp_log_wrapper_get_logs());
    ASSERT_TRUE(this->m_mem_alloc_trace.is_empty());
}

TEST_F(TestGwCfgStorage, test_gw_cfg_storage_read_file_as_blob_not_found) // NOLINT
{
    str_buf_t result = gw_cfg_storage_read_file_as_blob("blob_file");

    ASSERT_EQ(nullptr, result.buf);
    ASSERT_EQ(1U, this->m_nvs_close_call_cnt);
    ASSERT_EQ(
        "I gw_cfg: Read file 'blob_file' as blob from NVS\n"
        "E gw_cfg: Can't find blob key 'blob_file' in flash, err=4354 (Error 0x1102(4354))\n"
        "E gw_cfg: Failed to read file 'blob_file' (blob) from NVS\n",
        esp_log_wrapper_get_logs());
    ASSERT_TRUE(this->m_mem_alloc_trace.is_empty());
}

TEST_F(TestGwCfgStorage, test_gw_cfg_storage_read_file_as_blob_malloc_fail) // NOLINT
{
    this->m_nvs_storage["my_blob"] = "hi";
    this->m_malloc_fail_on_cnt     = 1; // fail first malloc (file content allocation)

    str_buf_t result = gw_cfg_storage_read_file_as_blob("my_blob");

    ASSERT_EQ(nullptr, result.buf);
    ASSERT_EQ(1U, this->m_nvs_close_call_cnt);
    ASSERT_EQ(
        "I gw_cfg: Read file 'my_blob' as blob from NVS\n"
        "E gw_cfg: Can't allocate 2 bytes for file\n"
        "E gw_cfg: Failed to read file 'my_blob' (blob) from NVS\n",
        esp_log_wrapper_get_logs());
    ASSERT_TRUE(this->m_mem_alloc_trace.is_empty());
}

TEST_F(TestGwCfgStorage, test_gw_cfg_storage_read_file_as_blob_second_read_fail) // NOLINT
{
    this->m_nvs_storage["my_blob"]   = "hello";
    this->m_nvs_get_blob_fail_on_cnt = 2; // succeed on size probe, fail on actual read

    str_buf_t result = gw_cfg_storage_read_file_as_blob("my_blob");

    ASSERT_EQ(nullptr, result.buf);
    ASSERT_EQ(1U, this->m_nvs_close_call_cnt);
    ASSERT_EQ(
        "I gw_cfg: Read file 'my_blob' as blob from NVS\n"
        "E gw_cfg: Can't read blob from NVS by key 'my_blob', err=4354 (Error 0x1102(4354))\n"
        "E gw_cfg: Failed to read file 'my_blob' (blob) from NVS\n",
        esp_log_wrapper_get_logs());
    ASSERT_TRUE(this->m_mem_alloc_trace.is_empty());
}

// ============================================================================================
// gw_cfg_storage_write_file_as_string
// ============================================================================================

TEST_F(TestGwCfgStorage, test_gw_cfg_storage_write_file_as_string_success) // NOLINT
{
    const bool result = gw_cfg_storage_write_file_as_string("my_cfg", "cfg_data");

    ASSERT_TRUE(result);
    ASSERT_EQ(string("cfg_data"), this->m_nvs_storage["my_cfg"]);
    ASSERT_EQ(1U, this->m_nvs_close_call_cnt);
    ASSERT_EQ(
        "I gw_cfg: Write file 'my_cfg' to NVS as string (length: 8)\n"
        "I gw_cfg: File 'my_cfg' was successfully written to NVS as string\n",
        esp_log_wrapper_get_logs());
    ASSERT_TRUE(this->m_mem_alloc_trace.is_empty());
}

TEST_F(TestGwCfgStorage, test_gw_cfg_storage_write_file_as_string_fail_nvs_open) // NOLINT
{
    this->m_nvs_open_fail = true;

    const bool result = gw_cfg_storage_write_file_as_string("my_cfg", "cfg_data");

    ASSERT_FALSE(result);
    ASSERT_TRUE(this->m_nvs_storage.empty());
    ASSERT_EQ(0U, this->m_nvs_close_call_cnt);
    ASSERT_EQ(
        "I gw_cfg: Write file 'my_cfg' to NVS as string (length: 8)\n"
        "E gw_cfg: ruuvi_nvs_open_gw_cfg_storage failed\n",
        esp_log_wrapper_get_logs());
    ASSERT_TRUE(this->m_mem_alloc_trace.is_empty());
}

TEST_F(TestGwCfgStorage, test_gw_cfg_storage_write_file_as_string_too_big) // NOLINT
{
    // NVS_CHUNK_MAX_SIZE = 32 * 125 = 4000; (len+1) > 4000 means len >= 4000
    const std::string too_big_content(4000, 'x');

    const bool result = gw_cfg_storage_write_file_as_string("my_cfg", too_big_content.c_str());

    ASSERT_FALSE(result);
    ASSERT_TRUE(this->m_nvs_storage.empty());
    ASSERT_EQ(1U, this->m_nvs_close_call_cnt);
    ASSERT_EQ(
        "I gw_cfg: Write file 'my_cfg' to NVS as string (length: 4000)\n"
        "E gw_cfg: File 'my_cfg' is too big to write as string to NVS (length=4000, max=3999)\n",
        esp_log_wrapper_get_logs());
    ASSERT_TRUE(this->m_mem_alloc_trace.is_empty());
}

TEST_F(TestGwCfgStorage, test_gw_cfg_storage_write_file_as_string_exactly_max_size) // NOLINT
{
    // Exactly at max: len = 3999, len+1 = 4000 = NVS_CHUNK_MAX_SIZE (not >), should succeed
    const std::string max_content(3999, 'x');

    const bool result = gw_cfg_storage_write_file_as_string("my_cfg", max_content.c_str());

    ASSERT_TRUE(result);
    ASSERT_EQ(max_content, this->m_nvs_storage["my_cfg"]);
    ASSERT_EQ(1U, this->m_nvs_close_call_cnt);
    ASSERT_EQ(
        "I gw_cfg: Write file 'my_cfg' to NVS as string (length: 3999)\n"
        "I gw_cfg: File 'my_cfg' was successfully written to NVS as string\n",
        esp_log_wrapper_get_logs());
    ASSERT_TRUE(this->m_mem_alloc_trace.is_empty());
}

TEST_F(TestGwCfgStorage, test_gw_cfg_storage_write_file_as_string_fail_set) // NOLINT
{
    this->m_nvs_set_str_fail = true;

    const bool result = gw_cfg_storage_write_file_as_string("my_cfg", "cfg_data");

    ASSERT_FALSE(result);
    ASSERT_TRUE(this->m_nvs_storage.empty());
    ASSERT_EQ(1U, this->m_nvs_close_call_cnt);
    ASSERT_EQ(
        "I gw_cfg: Write file 'my_cfg' to NVS as string (length: 8)\n"
        "E gw_cfg: Failed to write file 'my_cfg' to NVS as string, err=4357 (Error 0x1105(4357))\n",
        esp_log_wrapper_get_logs());
    ASSERT_TRUE(this->m_mem_alloc_trace.is_empty());
}

// ============================================================================================
// gw_cfg_storage_write_file_as_blob
// ============================================================================================

TEST_F(TestGwCfgStorage, test_gw_cfg_storage_write_file_as_blob_success) // NOLINT
{
    const uint8_t content[] = { 0x01, 0x02, 0x03, 0x04 };

    const bool result = gw_cfg_storage_write_file_as_blob("my_blob", content, sizeof(content));

    ASSERT_TRUE(result);
    const std::string expected(reinterpret_cast<const char*>(content), sizeof(content));
    ASSERT_EQ(expected, this->m_nvs_storage["my_blob"]);
    ASSERT_EQ(1U, this->m_nvs_close_call_cnt);
    ASSERT_EQ(
        "I gw_cfg: Write file 'my_blob' to NVS as blob (length: 4)\n"
        "I gw_cfg: File 'my_blob' was successfully written to NVS as blob\n",
        esp_log_wrapper_get_logs());
    ASSERT_TRUE(this->m_mem_alloc_trace.is_empty());
}

TEST_F(TestGwCfgStorage, test_gw_cfg_storage_write_file_as_blob_fail_nvs_open) // NOLINT
{
    this->m_nvs_open_fail   = true;
    const uint8_t content[] = { 0xAA, 0xBB };

    const bool result = gw_cfg_storage_write_file_as_blob("my_blob", content, sizeof(content));

    ASSERT_FALSE(result);
    ASSERT_TRUE(this->m_nvs_storage.empty());
    ASSERT_EQ(0U, this->m_nvs_close_call_cnt);
    ASSERT_EQ(
        "I gw_cfg: Write file 'my_blob' to NVS as blob (length: 2)\n"
        "E gw_cfg: ruuvi_nvs_open_gw_cfg_storage failed\n",
        esp_log_wrapper_get_logs());
    ASSERT_TRUE(this->m_mem_alloc_trace.is_empty());
}

TEST_F(TestGwCfgStorage, test_gw_cfg_storage_write_file_as_blob_too_big) // NOLINT
{
    // GW_CFG_STORAGE_MAX_BLOB_SIZE = 8192; len > 8192 is too big
    const std::vector<uint8_t> too_big_content(8193, 0xAA);

    const bool result = gw_cfg_storage_write_file_as_blob("my_blob", too_big_content.data(), too_big_content.size());

    ASSERT_FALSE(result);
    ASSERT_TRUE(this->m_nvs_storage.empty());
    ASSERT_EQ(1U, this->m_nvs_close_call_cnt);
    ASSERT_EQ(
        "I gw_cfg: Write file 'my_blob' to NVS as blob (length: 8193)\n"
        "E gw_cfg: File 'my_blob' is too big to write as blob to NVS (length=8193, max=8192)\n",
        esp_log_wrapper_get_logs());
    ASSERT_TRUE(this->m_mem_alloc_trace.is_empty());
}

TEST_F(TestGwCfgStorage, test_gw_cfg_storage_write_file_as_blob_exactly_max_size) // NOLINT
{
    // Exactly at max: len = 8192, should succeed
    const std::vector<uint8_t> max_content(8192, 0xBB);

    const bool result = gw_cfg_storage_write_file_as_blob("my_blob", max_content.data(), max_content.size());

    ASSERT_TRUE(result);
    ASSERT_EQ(1U, this->m_nvs_close_call_cnt);
    ASSERT_EQ(
        "I gw_cfg: Write file 'my_blob' to NVS as blob (length: 8192)\n"
        "I gw_cfg: File 'my_blob' was successfully written to NVS as blob\n",
        esp_log_wrapper_get_logs());
    ASSERT_TRUE(this->m_mem_alloc_trace.is_empty());
}

TEST_F(TestGwCfgStorage, test_gw_cfg_storage_write_file_as_blob_fail_set) // NOLINT
{
    this->m_nvs_set_blob_fail = true;
    const uint8_t content[]   = { 0x11, 0x22, 0x33 };

    const bool result = gw_cfg_storage_write_file_as_blob("my_blob", content, sizeof(content));

    ASSERT_FALSE(result);
    ASSERT_TRUE(this->m_nvs_storage.empty());
    ASSERT_EQ(1U, this->m_nvs_close_call_cnt);
    ASSERT_EQ(
        "I gw_cfg: Write file 'my_blob' to NVS as blob (length: 3)\n"
        "E gw_cfg: Failed to write file 'my_blob' to NVS as blob, err=4357 (Error 0x1105(4357))\n",
        esp_log_wrapper_get_logs());
    ASSERT_TRUE(this->m_mem_alloc_trace.is_empty());
}

// ============================================================================================
// gw_cfg_storage_delete_file
// ============================================================================================

TEST_F(TestGwCfgStorage, test_gw_cfg_storage_delete_file_success) // NOLINT
{
    this->m_nvs_storage["to_delete"] = "some_value";

    const bool result = gw_cfg_storage_delete_file("to_delete");

    ASSERT_TRUE(result);
    ASSERT_EQ(0U, this->m_nvs_storage.count("to_delete"));
    ASSERT_EQ(1U, this->m_nvs_close_call_cnt);
    ASSERT_EQ(
        "I gw_cfg: Delete file 'to_delete' from NVS\n"
        "I gw_cfg: File 'to_delete' was successfully deleted from NVS\n",
        esp_log_wrapper_get_logs());
    ASSERT_TRUE(this->m_mem_alloc_trace.is_empty());
}

TEST_F(TestGwCfgStorage, test_gw_cfg_storage_delete_file_fail_nvs_open) // NOLINT
{
    this->m_nvs_open_fail            = true;
    this->m_nvs_storage["to_delete"] = "some_value";

    const bool result = gw_cfg_storage_delete_file("to_delete");

    ASSERT_FALSE(result);
    ASSERT_EQ(1U, this->m_nvs_storage.count("to_delete")); // entry still there
    ASSERT_EQ(0U, this->m_nvs_close_call_cnt);
    ASSERT_EQ(
        "I gw_cfg: Delete file 'to_delete' from NVS\n"
        "E gw_cfg: ruuvi_nvs_open_gw_cfg_storage failed\n",
        esp_log_wrapper_get_logs());
    ASSERT_TRUE(this->m_mem_alloc_trace.is_empty());
}

TEST_F(TestGwCfgStorage, test_gw_cfg_storage_delete_file_fail_erase) // NOLINT
{
    this->m_nvs_erase_key_fail       = true;
    this->m_nvs_storage["to_delete"] = "some_value";

    const bool result = gw_cfg_storage_delete_file("to_delete");

    ASSERT_FALSE(result);
    ASSERT_EQ(1U, this->m_nvs_storage.count("to_delete")); // entry still there
    ASSERT_EQ(1U, this->m_nvs_close_call_cnt);
    ASSERT_EQ(
        "I gw_cfg: Delete file 'to_delete' from NVS\n"
        "E gw_cfg: Failed to delete file 'to_delete' in NVS, err=4354 (Error 0x1102(4354))\n",
        esp_log_wrapper_get_logs());
    ASSERT_TRUE(this->m_mem_alloc_trace.is_empty());
}

// ============================================================================================
// gw_cfg_storage_deinit_erase_init
// ============================================================================================

TEST_F(TestGwCfgStorage, test_gw_cfg_storage_deinit_erase_init_without_default) // NOLINT
{
    // No gw_cfg_default in NVS, another key is present
    this->m_nvs_storage["other_key"] = "other_value";

    gw_cfg_storage_deinit_erase_init();

    // deinit/erase/init must have been called in order
    ASSERT_EQ(1U, this->m_ruuvi_nvs_deinit_call_cnt);
    ASSERT_EQ(1U, this->m_ruuvi_nvs_erase_call_cnt);
    ASSERT_EQ(1U, this->m_ruuvi_nvs_init_call_cnt);
    // Storage was erased
    ASSERT_EQ(0U, this->m_nvs_storage.count("other_key"));
    // No default config written back (it did not exist)
    ASSERT_EQ(0U, this->m_nvs_storage.count(GW_CFG_STORAGE_GW_CFG_DEFAULT));
    ASSERT_TRUE(this->m_mem_alloc_trace.is_empty());
    ASSERT_EQ(
        "I gw_cfg: Read file 'gw_cfg_default' as string from NVS\n"
        "E gw_cfg: Can't find string key 'gw_cfg_default' in flash, err=4354 (Error 0x1102(4354))\n"
        "E gw_cfg: Failed to read file 'gw_cfg_default' (string) from NVS\n",
        esp_log_wrapper_get_logs());
}

TEST_F(TestGwCfgStorage, test_gw_cfg_storage_deinit_erase_init_with_default) // NOLINT
{
    const std::string default_cfg                      = "{\"key\":\"value\"}";
    this->m_nvs_storage[GW_CFG_STORAGE_GW_CFG_DEFAULT] = default_cfg;
    this->m_nvs_storage["other_key"]                   = "other_value";

    gw_cfg_storage_deinit_erase_init();

    // deinit/erase/init must have been called
    ASSERT_EQ(1U, this->m_ruuvi_nvs_deinit_call_cnt);
    ASSERT_EQ(1U, this->m_ruuvi_nvs_erase_call_cnt);
    ASSERT_EQ(1U, this->m_ruuvi_nvs_init_call_cnt);
    // Storage was erased then default cfg was written back
    ASSERT_EQ(0U, this->m_nvs_storage.count("other_key"));
    ASSERT_EQ(1U, this->m_nvs_storage.count(GW_CFG_STORAGE_GW_CFG_DEFAULT));
    ASSERT_EQ(default_cfg, this->m_nvs_storage[GW_CFG_STORAGE_GW_CFG_DEFAULT]);
    // No memory leaks
    ASSERT_TRUE(this->m_mem_alloc_trace.is_empty());
    ASSERT_EQ(
        "I gw_cfg: Read file 'gw_cfg_default' as string from NVS\n"
        "I gw_cfg: File 'gw_cfg_default' (string) was successfully read from NVS\n"
        "I gw_cfg: Write file 'gw_cfg_default' to NVS as string (length: 15)\n"
        "I gw_cfg: File 'gw_cfg_default' was successfully written to NVS as string\n",
        esp_log_wrapper_get_logs());
}
