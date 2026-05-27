/**
 * @file test_http_stream_reader_nvs.cpp
 * @author TheSomeMan
 * @date 2026-05-25
 * @copyright Ruuvi Innovations Ltd, license BSD-3-Clause.
 */

#include <cstring>
#include <string>
#include <vector>
#include "gtest/gtest.h"
#include "freertos/FreeRTOS.h"
#include "http_stream_reader_nvs.h"
#include "esp_log_wrapper.hpp"
#include "os_task.h"
#include "nvs.h"

using namespace std;

/*** Google-test class implementation
 * *********************************************************************************/

class TestHttpStreamReaderNvs;
static TestHttpStreamReaderNvs* g_pTestClass;

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
        assert(iter == this->allocated_mem.end());
        this->allocated_mem.push_back(ptr);
    }

    void
    remove(void* ptr)
    {
        auto iter = find(ptr);
        assert(iter != this->allocated_mem.end());
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

class TestHttpStreamReaderNvs : public ::testing::Test
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

        this->m_ruuvi_nvs_open_gw_cfg_storage_fail = false;
        this->m_nvs_close_call_cnt                 = 0;

        this->m_nvs_blob_data.clear();
        this->m_nvs_get_blob_result         = ESP_OK;
        this->m_nvs_get_blob_partial_result = ESP_OK;

        this->m_nvs_get_blob_partial_return_wrong_size = false;
    }

    void
    TearDown() override
    {
        g_pTestClass = nullptr;
        esp_log_wrapper_deinit();
    }

public:
    TestHttpStreamReaderNvs();

    ~TestHttpStreamReaderNvs() override;

    MemAllocTrace m_mem_alloc_trace;
    uint32_t      m_malloc_cnt {};
    uint32_t      m_malloc_fail_on_cnt {};

    // Mock control for ruuvi_nvs_open_gw_cfg_storage
    bool m_ruuvi_nvs_open_gw_cfg_storage_fail {};

    // Tracking counters
    uint32_t m_nvs_close_call_cnt {};

    // Mock NVS blob data (simulates file content)
    std::string m_nvs_blob_data;

    // Mock control for nvs_get_blob (size probe in OPEN)
    esp_err_t m_nvs_get_blob_result {};

    // Mock control for nvs_get_blob_partial (data read in READ)
    esp_err_t m_nvs_get_blob_partial_result {};

    // If true, nvs_get_blob_partial will return a different size than requested
    bool m_nvs_get_blob_partial_return_wrong_size {};
};

TestHttpStreamReaderNvs::TestHttpStreamReaderNvs()
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
ruuvi_nvs_open_gw_cfg_storage(nvs_open_mode_t open_mode, nvs_handle_t* p_handle)
{
    (void)open_mode;
    if (g_pTestClass->m_ruuvi_nvs_open_gw_cfg_storage_fail)
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
nvs_get_blob(nvs_handle_t handle, const char* key, void* out_value, size_t* length)
{
    (void)handle;
    (void)key;
    if (g_pTestClass->m_nvs_get_blob_result != ESP_OK)
    {
        return g_pTestClass->m_nvs_get_blob_result;
    }
    *length = g_pTestClass->m_nvs_blob_data.size();
    if (nullptr != out_value)
    {
        memcpy(out_value, g_pTestClass->m_nvs_blob_data.data(), g_pTestClass->m_nvs_blob_data.size());
    }
    return ESP_OK;
}

esp_err_t
nvs_get_blob_partial(nvs_handle_t c_handle, const char* key, void* out_value, size_t* length, size_t offset)
{
    (void)c_handle;
    (void)key;
    if (g_pTestClass->m_nvs_get_blob_partial_result != ESP_OK)
    {
        return g_pTestClass->m_nvs_get_blob_partial_result;
    }
    const std::string& data      = g_pTestClass->m_nvs_blob_data;
    size_t             requested = *length;
    size_t             available = (offset < data.size()) ? (data.size() - offset) : 0;
    size_t             to_copy   = (requested < available) ? requested : available;

    if (g_pTestClass->m_nvs_get_blob_partial_return_wrong_size)
    {
        // Return fewer bytes than requested to trigger the size mismatch path
        to_copy = (to_copy > 0) ? (to_copy - 1) : 0;
    }

    if (nullptr != out_value)
    {
        memcpy(out_value, data.data() + offset, to_copy);
    }
    *length = to_copy;
    return ESP_OK;
}

} // extern "C"

TestHttpStreamReaderNvs::~TestHttpStreamReaderNvs() = default;

/*** Unit-Tests
 * *******************************************************************************************************/

// ============================================================================================
// Helper function to construct http_stream_reader_arg_t for OPEN
// ============================================================================================
static http_stream_reader_arg_t
make_open_arg(const char* p_filename)
{
    http_stream_reader_arg_t arg = {};
    arg.open.p_param             = const_cast<char*>(p_filename);
    return arg;
}

// ============================================================================================
// Helper function to construct http_stream_reader_arg_t for READ
// ============================================================================================
static http_stream_reader_arg_t
make_read_arg(char* p_buf, size_t buf_len)
{
    http_stream_reader_arg_t arg = {};
    // We need to use memcpy to set the const fields in the union
    struct
    {
        char* const  p_buf;
        const size_t buf_len;
    } read_arg = { p_buf, buf_len };
    memcpy(&arg.read, &read_arg, sizeof(read_arg));
    return arg;
}

// ============================================================================================
// Helper function to construct http_stream_reader_arg_t for CLOSE
// ============================================================================================
static http_stream_reader_arg_t
make_close_arg(void)
{
    http_stream_reader_arg_t arg = {};
    return arg;
}

// ============================================================================================
// HTTP_STREAM_READER_CMD_OPEN
// ============================================================================================

TEST_F(TestHttpStreamReaderNvs, test_open_success) // NOLINT
{
    this->m_nvs_blob_data = "hello world";

    http_stream_reader_nvs_ctx_t ctx = {};
    const ssize_t result = http_stream_reader_nvs(HTTP_STREAM_READER_CMD_OPEN, make_open_arg("test_file"), &ctx);

    ASSERT_EQ(0, result);
    ASSERT_STREQ("test_file", ctx.p_filename);
    ASSERT_EQ(MOCK_NVS_VALID_HANDLE, ctx.handle);
    ASSERT_EQ(11U, ctx.file_size);
    ASSERT_EQ(0U, ctx.data_offset);

    // Close to avoid handle leak
    const ssize_t close_result = http_stream_reader_nvs(HTTP_STREAM_READER_CMD_CLOSE, make_close_arg(), &ctx);
    ASSERT_EQ(0, close_result);
    ASSERT_EQ(1U, this->m_nvs_close_call_cnt);
    // Fetch and check logs after all test actions to catch any unexpected or missing log messages
    ASSERT_EQ("", esp_log_wrapper_get_logs());
    // Check for resource leaks — must be the very last check to ensure all memory was freed on every code path
    ASSERT_TRUE(this->m_mem_alloc_trace.is_empty());
}

TEST_F(TestHttpStreamReaderNvs, test_open_fail_nvs_open) // NOLINT
{
    this->m_ruuvi_nvs_open_gw_cfg_storage_fail = true;

    http_stream_reader_nvs_ctx_t ctx = {};
    const ssize_t result = http_stream_reader_nvs(HTTP_STREAM_READER_CMD_OPEN, make_open_arg("test_file"), &ctx);

    ASSERT_EQ(-1, result);
    ASSERT_EQ(0U, this->m_nvs_close_call_cnt); // No handle was acquired, so no close
    // Fetch and check logs after all test actions to catch any unexpected or missing log messages
    ASSERT_EQ("E http: ruuvi_nvs_open_gw_cfg_storage failed\n", esp_log_wrapper_get_logs());
    // Check for resource leaks — must be the very last check to ensure all memory was freed on every code path
    ASSERT_TRUE(this->m_mem_alloc_trace.is_empty());
}

TEST_F(TestHttpStreamReaderNvs, test_open_fail_nvs_get_blob) // NOLINT
{
    this->m_nvs_get_blob_result = ESP_ERR_NVS_NOT_FOUND;

    http_stream_reader_nvs_ctx_t ctx = {};
    const ssize_t result = http_stream_reader_nvs(HTTP_STREAM_READER_CMD_OPEN, make_open_arg("missing_key"), &ctx);

    ASSERT_EQ(-1, result);
    ASSERT_EQ(1U, this->m_nvs_close_call_cnt); // NVS handle must be closed on error
    // Fetch and check logs after all test actions to catch any unexpected or missing log messages
    // ESP_ERR_NVS_NOT_FOUND = 0x1102 = 4354
    ASSERT_EQ(
        "E http: Can't find key 'missing_key' in NVS, err=4354 (Error 0x1102(4354))\n",
        esp_log_wrapper_get_logs());
    // Check for resource leaks — must be the very last check to ensure all memory was freed on every code path
    ASSERT_TRUE(this->m_mem_alloc_trace.is_empty());
}

TEST_F(TestHttpStreamReaderNvs, test_open_fail_empty_file) // NOLINT
{
    this->m_nvs_blob_data = ""; // empty file, size == 0

    http_stream_reader_nvs_ctx_t ctx = {};
    const ssize_t result = http_stream_reader_nvs(HTTP_STREAM_READER_CMD_OPEN, make_open_arg("empty_file"), &ctx);

    ASSERT_EQ(-1, result);
    ASSERT_EQ(1U, this->m_nvs_close_call_cnt); // NVS handle must be closed on error
    // Fetch and check logs after all test actions to catch any unexpected or missing log messages
    ASSERT_EQ("E http: File 'empty_file' is empty\n", esp_log_wrapper_get_logs());
    // Check for resource leaks — must be the very last check to ensure all memory was freed on every code path
    ASSERT_TRUE(this->m_mem_alloc_trace.is_empty());
}

// ============================================================================================
// HTTP_STREAM_READER_CMD_READ
// ============================================================================================

TEST_F(TestHttpStreamReaderNvs, test_read_full_file_in_one_call) // NOLINT
{
    this->m_nvs_blob_data = "hello";

    http_stream_reader_nvs_ctx_t ctx = {};
    // Open first
    ssize_t result = http_stream_reader_nvs(HTTP_STREAM_READER_CMD_OPEN, make_open_arg("test_file"), &ctx);
    ASSERT_EQ(0, result);
    (void)esp_log_wrapper_get_logs(); // clear logs

    // Read entire file — buffer is larger than file content
    char buf[64] = {};
    result       = http_stream_reader_nvs(HTTP_STREAM_READER_CMD_READ, make_read_arg(buf, sizeof(buf)), &ctx);

    ASSERT_EQ(5, result);
    ASSERT_STREQ("hello", buf);
    ASSERT_EQ(5U, ctx.data_offset);

    // Close to avoid handle leak
    result = http_stream_reader_nvs(HTTP_STREAM_READER_CMD_CLOSE, make_close_arg(), &ctx);
    ASSERT_EQ(0, result);
    ASSERT_EQ(1U, this->m_nvs_close_call_cnt);
    // Fetch and check logs after all test actions to catch any unexpected or missing log messages
    ASSERT_EQ("", esp_log_wrapper_get_logs());
    // Check for resource leaks — must be the very last check to ensure all memory was freed on every code path
    ASSERT_TRUE(this->m_mem_alloc_trace.is_empty());
}

TEST_F(TestHttpStreamReaderNvs, test_read_in_multiple_chunks) // NOLINT
{
    this->m_nvs_blob_data = "ABCDEFGHIJ"; // 10 bytes

    http_stream_reader_nvs_ctx_t ctx = {};
    ssize_t result = http_stream_reader_nvs(HTTP_STREAM_READER_CMD_OPEN, make_open_arg("test_file"), &ctx);
    ASSERT_EQ(0, result);
    ASSERT_EQ(10U, ctx.file_size);
    (void)esp_log_wrapper_get_logs();

    // Read with buf_len=5 → bytes_to_read = min(5-1, 10-0) = min(4, 10) = 4
    char buf[5] = {};
    result      = http_stream_reader_nvs(HTTP_STREAM_READER_CMD_READ, make_read_arg(buf, sizeof(buf)), &ctx);
    ASSERT_EQ(4, result);
    ASSERT_STREQ("ABCD", buf);
    ASSERT_EQ(4U, ctx.data_offset);
    (void)esp_log_wrapper_get_logs();

    // Next read: rem_len=6, bytes_to_read = min(4, 6) = 4
    memset(buf, 0, sizeof(buf));
    result = http_stream_reader_nvs(HTTP_STREAM_READER_CMD_READ, make_read_arg(buf, sizeof(buf)), &ctx);
    ASSERT_EQ(4, result);
    ASSERT_STREQ("EFGH", buf);
    ASSERT_EQ(8U, ctx.data_offset);
    (void)esp_log_wrapper_get_logs();

    // Final read: rem_len=2, bytes_to_read = min(4, 2) = 2
    memset(buf, 0, sizeof(buf));
    result = http_stream_reader_nvs(HTTP_STREAM_READER_CMD_READ, make_read_arg(buf, sizeof(buf)), &ctx);
    ASSERT_EQ(2, result);
    ASSERT_STREQ("IJ", buf);
    ASSERT_EQ(10U, ctx.data_offset); // offset == file_size
    (void)esp_log_wrapper_get_logs();

    // Close to avoid handle leak
    result = http_stream_reader_nvs(HTTP_STREAM_READER_CMD_CLOSE, make_close_arg(), &ctx);
    ASSERT_EQ(0, result);
    ASSERT_EQ(1U, this->m_nvs_close_call_cnt);
    // Fetch and check logs after all test actions to catch any unexpected or missing log messages
    ASSERT_EQ("", esp_log_wrapper_get_logs());
    // Check for resource leaks — must be the very last check to ensure all memory was freed on every code path
    ASSERT_TRUE(this->m_mem_alloc_trace.is_empty());
}

TEST_F(TestHttpStreamReaderNvs, test_read_fail_nvs_get_blob_partial) // NOLINT
{
    this->m_nvs_blob_data = "data";

    http_stream_reader_nvs_ctx_t ctx = {};
    ssize_t result = http_stream_reader_nvs(HTTP_STREAM_READER_CMD_OPEN, make_open_arg("test_file"), &ctx);
    ASSERT_EQ(0, result);
    (void)esp_log_wrapper_get_logs();

    // Make nvs_get_blob_partial fail
    this->m_nvs_get_blob_partial_result = ESP_ERR_NVS_INVALID_HANDLE;

    char buf[64] = {};
    result       = http_stream_reader_nvs(HTTP_STREAM_READER_CMD_READ, make_read_arg(buf, sizeof(buf)), &ctx);

    ASSERT_EQ(-1, result);
    // ESP_ERR_NVS_INVALID_HANDLE = 0x1107 = 4359
    ASSERT_EQ(
        "E http: Can't read file from NVS by key 'test_file', err=4359 (Error 0x1107(4359))\n",
        esp_log_wrapper_get_logs());

    // Close to avoid handle leak (handle is still open after READ error)
    result = http_stream_reader_nvs(HTTP_STREAM_READER_CMD_CLOSE, make_close_arg(), &ctx);
    ASSERT_EQ(0, result);
    ASSERT_EQ(1U, this->m_nvs_close_call_cnt);
    // Fetch and check logs after all test actions to catch any unexpected or missing log messages
    ASSERT_EQ("", esp_log_wrapper_get_logs());
    // Check for resource leaks — must be the very last check to ensure all memory was freed on every code path
    ASSERT_TRUE(this->m_mem_alloc_trace.is_empty());
}

TEST_F(TestHttpStreamReaderNvs, test_read_fail_size_mismatch) // NOLINT
{
    this->m_nvs_blob_data = "hello world";

    http_stream_reader_nvs_ctx_t ctx = {};
    ssize_t result = http_stream_reader_nvs(HTTP_STREAM_READER_CMD_OPEN, make_open_arg("test_file"), &ctx);
    ASSERT_EQ(0, result);
    (void)esp_log_wrapper_get_logs();

    // Make nvs_get_blob_partial return wrong size
    this->m_nvs_get_blob_partial_return_wrong_size = true;

    char buf[64] = {};
    result       = http_stream_reader_nvs(HTTP_STREAM_READER_CMD_READ, make_read_arg(buf, sizeof(buf)), &ctx);

    ASSERT_EQ(-1, result);
    ASSERT_EQ(
        "E http: Read unexpected number of bytes from NVS file 'test_file': expected 11, got 10\n",
        esp_log_wrapper_get_logs());

    // Close to avoid handle leak (handle is still open after READ error)
    result = http_stream_reader_nvs(HTTP_STREAM_READER_CMD_CLOSE, make_close_arg(), &ctx);
    ASSERT_EQ(0, result);
    ASSERT_EQ(1U, this->m_nvs_close_call_cnt);
    // Fetch and check logs after all test actions to catch any unexpected or missing log messages
    ASSERT_EQ("", esp_log_wrapper_get_logs());
    // Check for resource leaks — must be the very last check to ensure all memory was freed on every code path
    ASSERT_TRUE(this->m_mem_alloc_trace.is_empty());
}

// ============================================================================================
// HTTP_STREAM_READER_CMD_CLOSE
// ============================================================================================

TEST_F(TestHttpStreamReaderNvs, test_close) // NOLINT
{
    this->m_nvs_blob_data = "hello";

    http_stream_reader_nvs_ctx_t ctx = {};
    ssize_t result = http_stream_reader_nvs(HTTP_STREAM_READER_CMD_OPEN, make_open_arg("test_file"), &ctx);
    ASSERT_EQ(0, result);
    (void)esp_log_wrapper_get_logs();

    // Read some data to advance offset
    char buf[64] = {};
    result       = http_stream_reader_nvs(HTTP_STREAM_READER_CMD_READ, make_read_arg(buf, sizeof(buf)), &ctx);
    ASSERT_EQ(5, result);
    ASSERT_EQ(5U, ctx.data_offset);
    (void)esp_log_wrapper_get_logs();

    // Close
    result = http_stream_reader_nvs(HTTP_STREAM_READER_CMD_CLOSE, make_close_arg(), &ctx);

    ASSERT_EQ(0, result);
    ASSERT_EQ(0U, ctx.handle);
    ASSERT_EQ(0U, ctx.data_offset);
    ASSERT_EQ(1U, this->m_nvs_close_call_cnt);
    // Fetch and check logs after all test actions to catch any unexpected or missing log messages
    ASSERT_EQ("", esp_log_wrapper_get_logs());
    // Check for resource leaks — must be the very last check to ensure all memory was freed on every code path
    ASSERT_TRUE(this->m_mem_alloc_trace.is_empty());
}

// ============================================================================================
// Full open-read-close cycle
// ============================================================================================

TEST_F(TestHttpStreamReaderNvs, test_full_open_read_close_cycle) // NOLINT
{
    this->m_nvs_blob_data = "test content 123";

    http_stream_reader_nvs_ctx_t ctx = {};

    // Open
    ssize_t result = http_stream_reader_nvs(HTTP_STREAM_READER_CMD_OPEN, make_open_arg("cfg_file"), &ctx);
    ASSERT_EQ(0, result);
    ASSERT_EQ(16U, ctx.file_size);
    (void)esp_log_wrapper_get_logs();

    // Read all data
    char buf[64] = {};
    result       = http_stream_reader_nvs(HTTP_STREAM_READER_CMD_READ, make_read_arg(buf, sizeof(buf)), &ctx);
    ASSERT_EQ(16, result);
    ASSERT_STREQ("test content 123", buf);
    ASSERT_EQ(16U, ctx.data_offset);
    (void)esp_log_wrapper_get_logs();

    // Close
    result = http_stream_reader_nvs(HTTP_STREAM_READER_CMD_CLOSE, make_close_arg(), &ctx);
    ASSERT_EQ(0, result);
    ASSERT_EQ(0U, ctx.handle);
    ASSERT_EQ(0U, ctx.data_offset);
    ASSERT_EQ(1U, this->m_nvs_close_call_cnt);
    // Fetch and check logs after all test actions to catch any unexpected or missing log messages
    ASSERT_EQ("", esp_log_wrapper_get_logs());
    // Check for resource leaks — must be the very last check to ensure all memory was freed on every code path
    ASSERT_TRUE(this->m_mem_alloc_trace.is_empty());
}

TEST_F(TestHttpStreamReaderNvs, test_read_exact_buffer_boundary) // NOLINT
{
    // file_size=3, buf_len=4 → bytes_to_read = min(4-1, 3-0) = min(3, 3) = 3 (reads entire file)
    this->m_nvs_blob_data = "ABC";

    http_stream_reader_nvs_ctx_t ctx = {};
    ssize_t result                   = http_stream_reader_nvs(HTTP_STREAM_READER_CMD_OPEN, make_open_arg("file"), &ctx);
    ASSERT_EQ(0, result);
    ASSERT_EQ(3U, ctx.file_size);
    (void)esp_log_wrapper_get_logs();

    char buf[4] = {};
    result      = http_stream_reader_nvs(HTTP_STREAM_READER_CMD_READ, make_read_arg(buf, sizeof(buf)), &ctx);
    ASSERT_EQ(3, result);
    ASSERT_STREQ("ABC", buf);
    ASSERT_EQ(3U, ctx.data_offset); // offset == file_size → finished
    (void)esp_log_wrapper_get_logs();

    // Close to avoid handle leak
    result = http_stream_reader_nvs(HTTP_STREAM_READER_CMD_CLOSE, make_close_arg(), &ctx);
    ASSERT_EQ(0, result);
    ASSERT_EQ(1U, this->m_nvs_close_call_cnt);
    // Fetch and check logs after all test actions to catch any unexpected or missing log messages
    ASSERT_EQ("", esp_log_wrapper_get_logs());
    // Check for resource leaks — must be the very last check to ensure all memory was freed on every code path
    ASSERT_TRUE(this->m_mem_alloc_trace.is_empty());
}

TEST_F(TestHttpStreamReaderNvs, test_open_fail_nvs_get_blob_other_error) // NOLINT
{
    // Test with a different NVS error code
    this->m_nvs_get_blob_result = ESP_ERR_NVS_INVALID_HANDLE;

    http_stream_reader_nvs_ctx_t ctx = {};
    const ssize_t result             = http_stream_reader_nvs(HTTP_STREAM_READER_CMD_OPEN, make_open_arg("file"), &ctx);

    ASSERT_EQ(-1, result);
    ASSERT_EQ(1U, this->m_nvs_close_call_cnt); // NVS handle must be closed on error
    // Fetch and check logs after all test actions to catch any unexpected or missing log messages
    // ESP_ERR_NVS_INVALID_HANDLE = 0x1107 = 4359
    ASSERT_EQ("E http: Can't find key 'file' in NVS, err=4359 (Error 0x1107(4359))\n", esp_log_wrapper_get_logs());
    // Check for resource leaks — must be the very last check to ensure all memory was freed on every code path
    ASSERT_TRUE(this->m_mem_alloc_trace.is_empty());
}
