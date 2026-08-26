/**
 * @file test_ruuvi_nvs.cpp
 * @author TheSomeMan
 * @date 2026-05-24
 * @copyright Ruuvi Innovations Ltd, license BSD-3-Clause.
 */

#include <cstring>
#include <vector>
#include "gtest/gtest.h"
#include "freertos/FreeRTOS.h"
#include "ruuvi_nvs.h"
#include "esp_log_wrapper.hpp"
#include "os_task.h"
#include "nvs.h"

using namespace std;

/*** Google-test class implementation
 * *********************************************************************************/

class TestRuuviNvs;
static TestRuuviNvs* g_pTestClass;

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

class TestRuuviNvs : public ::testing::Test
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

        this->m_nvs_flash_init_partition_result   = ESP_OK;
        this->m_nvs_flash_deinit_partition_result = ESP_OK;
        this->m_nvs_flash_erase_result            = ESP_OK;
        this->m_nvs_flash_erase_partition_result  = ESP_OK;

        this->m_nvs_open_from_partition_call_cnt = 0;
        this->m_nvs_open_from_partition_results.clear();

        this->m_nvs_commit_result  = ESP_OK;
        this->m_nvs_close_call_cnt = 0;
    }

    void
    TearDown() override
    {
        g_pTestClass = nullptr;
        esp_log_wrapper_deinit();
    }

public:
    TestRuuviNvs();

    ~TestRuuviNvs() override;

    MemAllocTrace m_mem_alloc_trace;
    uint32_t      m_malloc_cnt {};
    uint32_t      m_malloc_fail_on_cnt {};

    // Mock control for nvs_flash_init_partition
    esp_err_t m_nvs_flash_init_partition_result {};

    // Mock control for nvs_flash_deinit_partition
    esp_err_t m_nvs_flash_deinit_partition_result {};

    // Mock control for nvs_flash_erase
    esp_err_t m_nvs_flash_erase_result {};

    // Mock control for nvs_flash_erase_partition
    esp_err_t m_nvs_flash_erase_partition_result {};

    // Mock control for nvs_open_from_partition — per-call results
    uint32_t               m_nvs_open_from_partition_call_cnt {};
    std::vector<esp_err_t> m_nvs_open_from_partition_results;

    // Mock control for nvs_commit
    esp_err_t m_nvs_commit_result {};

    // Tracking counters
    uint32_t m_nvs_close_call_cnt {};
};

TestRuuviNvs::TestRuuviNvs()
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

esp_err_t
nvs_flash_init_partition(const char* partition_label)
{
    (void)partition_label;
    return g_pTestClass->m_nvs_flash_init_partition_result;
}

esp_err_t
nvs_flash_deinit_partition(const char* partition_label)
{
    (void)partition_label;
    return g_pTestClass->m_nvs_flash_deinit_partition_result;
}

esp_err_t
nvs_flash_erase(void)
{
    return g_pTestClass->m_nvs_flash_erase_result;
}

esp_err_t
nvs_flash_erase_partition(const char* part_name)
{
    (void)part_name;
    return g_pTestClass->m_nvs_flash_erase_partition_result;
}

esp_err_t
nvs_open_from_partition(const char* part_name, const char* name, nvs_open_mode_t open_mode, nvs_handle_t* out_handle)
{
    (void)part_name;
    (void)name;
    (void)open_mode;

    const uint32_t idx = g_pTestClass->m_nvs_open_from_partition_call_cnt++;
    esp_err_t      result;
    if (idx < g_pTestClass->m_nvs_open_from_partition_results.size())
    {
        result = g_pTestClass->m_nvs_open_from_partition_results[idx];
    }
    else
    {
        result = ESP_OK;
    }
    if (ESP_OK == result)
    {
        *out_handle = MOCK_NVS_VALID_HANDLE;
    }
    return result;
}

esp_err_t
nvs_commit(nvs_handle_t handle)
{
    (void)handle;
    return g_pTestClass->m_nvs_commit_result;
}

void
nvs_close(nvs_handle_t handle)
{
    assert(MOCK_NVS_VALID_HANDLE == handle);
    g_pTestClass->m_nvs_close_call_cnt++;
}

} // extern "C"

TestRuuviNvs::~TestRuuviNvs() = default;

/*** Unit-Tests
 * *******************************************************************************************************/

// ============================================================================================
// ruuvi_nvs_init
// ============================================================================================

TEST_F(TestRuuviNvs, test_ruuvi_nvs_init_ok) // NOLINT
{
    const bool result = ruuvi_nvs_init();

    ASSERT_TRUE(result);
    ASSERT_EQ("I ruuvi_nvs: NVS init partition: nvs\n", esp_log_wrapper_get_logs());
    ASSERT_TRUE(this->m_mem_alloc_trace.is_empty());
}

TEST_F(TestRuuviNvs, test_ruuvi_nvs_init_fail) // NOLINT
{
    this->m_nvs_flash_init_partition_result = ESP_ERR_NVS_NO_FREE_PAGES;

    const bool result = ruuvi_nvs_init();

    ASSERT_FALSE(result);
    // ESP_ERR_NVS_NO_FREE_PAGES = 0x110d = 4365
    ASSERT_EQ(
        "I ruuvi_nvs: NVS init partition: nvs\n"
        "E ruuvi_nvs: nvs_flash_init_partition failed for partition nvs, err=4365 (Error 0x110d(4365))\n",
        esp_log_wrapper_get_logs());
    ASSERT_TRUE(this->m_mem_alloc_trace.is_empty());
}

// ============================================================================================
// ruuvi_nvs_init_gw_cfg_storage
// ============================================================================================

TEST_F(TestRuuviNvs, test_ruuvi_nvs_init_gw_cfg_storage_ok) // NOLINT
{
    const bool result = ruuvi_nvs_init_gw_cfg_storage();

    ASSERT_TRUE(result);
    ASSERT_EQ("I ruuvi_nvs: NVS init partition: gw_cfg_def\n", esp_log_wrapper_get_logs());
    ASSERT_TRUE(this->m_mem_alloc_trace.is_empty());
}

TEST_F(TestRuuviNvs, test_ruuvi_nvs_init_gw_cfg_storage_fail) // NOLINT
{
    this->m_nvs_flash_init_partition_result = ESP_ERR_NVS_NO_FREE_PAGES;

    const bool result = ruuvi_nvs_init_gw_cfg_storage();

    ASSERT_FALSE(result);
    ASSERT_EQ(
        "I ruuvi_nvs: NVS init partition: gw_cfg_def\n"
        "E ruuvi_nvs: nvs_flash_init_partition failed for partition gw_cfg_def, err=4365 (Error 0x110d(4365))\n",
        esp_log_wrapper_get_logs());
    ASSERT_TRUE(this->m_mem_alloc_trace.is_empty());
}

// ============================================================================================
// ruuvi_nvs_deinit
// ============================================================================================

TEST_F(TestRuuviNvs, test_ruuvi_nvs_deinit_ok) // NOLINT
{
    const bool result = ruuvi_nvs_deinit();

    ASSERT_TRUE(result);
    ASSERT_EQ("I ruuvi_nvs: NVS deinit partition: nvs\n", esp_log_wrapper_get_logs());
    ASSERT_TRUE(this->m_mem_alloc_trace.is_empty());
}

TEST_F(TestRuuviNvs, test_ruuvi_nvs_deinit_fail) // NOLINT
{
    this->m_nvs_flash_deinit_partition_result = ESP_ERR_NVS_NOT_INITIALIZED;

    const bool result = ruuvi_nvs_deinit();

    ASSERT_FALSE(result);
    // ESP_ERR_NVS_NOT_INITIALIZED = 0x1101 = 4353
    ASSERT_EQ(
        "I ruuvi_nvs: NVS deinit partition: nvs\n"
        "E ruuvi_nvs: nvs_flash_deinit_partition failed for partition nvs, err=4353 (Error 0x1101(4353))\n",
        esp_log_wrapper_get_logs());
    ASSERT_TRUE(this->m_mem_alloc_trace.is_empty());
}

// ============================================================================================
// ruuvi_nvs_deinit_gw_cfg_storage
// ============================================================================================

TEST_F(TestRuuviNvs, test_ruuvi_nvs_deinit_gw_cfg_storage_ok) // NOLINT
{
    const bool result = ruuvi_nvs_deinit_gw_cfg_storage();

    ASSERT_TRUE(result);
    ASSERT_EQ("I ruuvi_nvs: NVS deinit partition: gw_cfg_def\n", esp_log_wrapper_get_logs());
    ASSERT_TRUE(this->m_mem_alloc_trace.is_empty());
}

TEST_F(TestRuuviNvs, test_ruuvi_nvs_deinit_gw_cfg_storage_fail) // NOLINT
{
    this->m_nvs_flash_deinit_partition_result = ESP_ERR_NVS_NOT_INITIALIZED;

    const bool result = ruuvi_nvs_deinit_gw_cfg_storage();

    ASSERT_FALSE(result);
    ASSERT_EQ(
        "I ruuvi_nvs: NVS deinit partition: gw_cfg_def\n"
        "E ruuvi_nvs: nvs_flash_deinit_partition failed for partition gw_cfg_def, err=4353 (Error 0x1101(4353))\n",
        esp_log_wrapper_get_logs());
    ASSERT_TRUE(this->m_mem_alloc_trace.is_empty());
}

// ============================================================================================
// ruuvi_nvs_erase
// ============================================================================================

TEST_F(TestRuuviNvs, test_ruuvi_nvs_erase_ok) // NOLINT
{
    ruuvi_nvs_erase();

    ASSERT_EQ("I ruuvi_nvs: Erase NVS\n", esp_log_wrapper_get_logs());
    ASSERT_TRUE(this->m_mem_alloc_trace.is_empty());
}

TEST_F(TestRuuviNvs, test_ruuvi_nvs_erase_fail) // NOLINT
{
    this->m_nvs_flash_erase_result = ESP_ERR_NVS_NOT_INITIALIZED;

    ruuvi_nvs_erase();

    ASSERT_EQ(
        "I ruuvi_nvs: Erase NVS\n"
        "E ruuvi_nvs: nvs_flash_erase failed, err=4353 (Error 0x1101(4353))\n",
        esp_log_wrapper_get_logs());
    ASSERT_TRUE(this->m_mem_alloc_trace.is_empty());
}

// ============================================================================================
// ruuvi_nvs_erase_gw_cfg_storage
// ============================================================================================

TEST_F(TestRuuviNvs, test_ruuvi_nvs_erase_gw_cfg_storage_ok) // NOLINT
{
    ruuvi_nvs_erase_gw_cfg_storage();

    ASSERT_EQ("I ruuvi_nvs: Erase NVS GW_CFG_STORAGE\n", esp_log_wrapper_get_logs());
    ASSERT_TRUE(this->m_mem_alloc_trace.is_empty());
}

TEST_F(TestRuuviNvs, test_ruuvi_nvs_erase_gw_cfg_storage_fail) // NOLINT
{
    this->m_nvs_flash_erase_partition_result = ESP_ERR_NVS_NOT_INITIALIZED;

    ruuvi_nvs_erase_gw_cfg_storage();

    ASSERT_EQ(
        "I ruuvi_nvs: Erase NVS GW_CFG_STORAGE\n"
        "E ruuvi_nvs: nvs_flash_erase failed, err=4353 (Error 0x1101(4353))\n",
        esp_log_wrapper_get_logs());
    ASSERT_TRUE(this->m_mem_alloc_trace.is_empty());
}

// ============================================================================================
// ruuvi_nvs_open (via ruuvi_nvs_open_partition with NVS_DEFAULT_PART_NAME = "nvs")
// ============================================================================================

TEST_F(TestRuuviNvs, test_ruuvi_nvs_open_ok) // NOLINT
{
    this->m_nvs_open_from_partition_results = { ESP_OK };

    nvs_handle_t handle = 0;
    const bool   result = ruuvi_nvs_open(NVS_READONLY, &handle);

    ASSERT_TRUE(result);
    ASSERT_EQ(MOCK_NVS_VALID_HANDLE, handle);
    ASSERT_EQ(1U, this->m_nvs_open_from_partition_call_cnt);
    ASSERT_EQ(0U, this->m_nvs_close_call_cnt);
    ASSERT_EQ("", esp_log_wrapper_get_logs());
    ASSERT_TRUE(this->m_mem_alloc_trace.is_empty());
}

TEST_F(TestRuuviNvs, test_ruuvi_nvs_open_not_initialized) // NOLINT
{
    this->m_nvs_open_from_partition_results = { ESP_ERR_NVS_NOT_INITIALIZED };

    nvs_handle_t handle = 0;
    const bool   result = ruuvi_nvs_open(NVS_READONLY, &handle);

    ASSERT_FALSE(result);
    ASSERT_EQ(1U, this->m_nvs_open_from_partition_call_cnt);
    ASSERT_EQ(0U, this->m_nvs_close_call_cnt);
    ASSERT_EQ(
        "W ruuvi_nvs: NVS partition nvs, namespace 'ruuvi_gateway': StorageState is INVALID, need to erase NVS\n",
        esp_log_wrapper_get_logs());
    ASSERT_TRUE(this->m_mem_alloc_trace.is_empty());
}

TEST_F(TestRuuviNvs, test_ruuvi_nvs_open_not_found_then_create_ok_then_reopen_ok) // NOLINT
{
    // 1st call: NOT_FOUND, 2nd call (READWRITE create): OK, 3rd call (re-open): OK
    this->m_nvs_open_from_partition_results = { ESP_ERR_NVS_NOT_FOUND, ESP_OK, ESP_OK };

    nvs_handle_t handle = 0;
    const bool   result = ruuvi_nvs_open(NVS_READONLY, &handle);

    ASSERT_TRUE(result);
    ASSERT_EQ(MOCK_NVS_VALID_HANDLE, handle);
    ASSERT_EQ(3U, this->m_nvs_open_from_partition_call_cnt);
    ASSERT_EQ(1U, this->m_nvs_close_call_cnt);
    ASSERT_EQ(
        "W ruuvi_nvs: NVS partition nvs, namespace 'ruuvi_gateway' doesn't exist and mode is NVS_READONLY, "
        "try to create it\n"
        "I ruuvi_nvs: ### NVS partition nvs, namespace 'ruuvi_gateway' created successfully\n",
        esp_log_wrapper_get_logs());
    ASSERT_TRUE(this->m_mem_alloc_trace.is_empty());
}

TEST_F(TestRuuviNvs, test_ruuvi_nvs_open_not_found_then_create_fail) // NOLINT
{
    // 1st call: NOT_FOUND, 2nd call (READWRITE create): fail
    this->m_nvs_open_from_partition_results = { ESP_ERR_NVS_NOT_FOUND, ESP_ERR_NVS_INVALID_NAME };

    nvs_handle_t handle = 0;
    const bool   result = ruuvi_nvs_open(NVS_READONLY, &handle);

    ASSERT_FALSE(result);
    ASSERT_EQ(2U, this->m_nvs_open_from_partition_call_cnt);
    ASSERT_EQ(0U, this->m_nvs_close_call_cnt);
    // ESP_ERR_NVS_INVALID_NAME = 0x1106 = 4358
    ASSERT_EQ(
        "W ruuvi_nvs: NVS partition nvs, namespace 'ruuvi_gateway' doesn't exist and mode is NVS_READONLY, "
        "try to create it\n"
        "E ruuvi_nvs: Can't open NVS for writing, err=4358 (Error 0x1106(4358))\n",
        esp_log_wrapper_get_logs());
    ASSERT_TRUE(this->m_mem_alloc_trace.is_empty());
}

TEST_F(TestRuuviNvs, test_ruuvi_nvs_open_not_found_then_create_ok_commit_fail) // NOLINT
{
    // 1st call: NOT_FOUND, 2nd call (READWRITE create): OK, but commit fails
    this->m_nvs_open_from_partition_results = { ESP_ERR_NVS_NOT_FOUND, ESP_OK };
    this->m_nvs_commit_result               = ESP_ERR_NVS_INVALID_STATE;

    nvs_handle_t handle = 0;
    const bool   result = ruuvi_nvs_open(NVS_READONLY, &handle);

    ASSERT_FALSE(result);
    ASSERT_EQ(2U, this->m_nvs_open_from_partition_call_cnt);
    ASSERT_EQ(1U, this->m_nvs_close_call_cnt);
    // ESP_ERR_NVS_INVALID_STATE = 0x110b = 4363
    ASSERT_EQ(
        "W ruuvi_nvs: NVS partition nvs, namespace 'ruuvi_gateway' doesn't exist and mode is NVS_READONLY, "
        "try to create it\n"
        "E ruuvi_nvs: nvs_commit failed, err=4363 (Error 0x110b(4363))\n",
        esp_log_wrapper_get_logs());
    ASSERT_TRUE(this->m_mem_alloc_trace.is_empty());
}

TEST_F(TestRuuviNvs, test_ruuvi_nvs_open_not_found_then_create_ok_commit_ok_reopen_fail) // NOLINT
{
    // 1st call: NOT_FOUND, 2nd call (READWRITE create): OK, commit OK, 3rd call (re-open): fail
    this->m_nvs_open_from_partition_results = { ESP_ERR_NVS_NOT_FOUND, ESP_OK, ESP_ERR_NVS_INVALID_STATE };

    nvs_handle_t handle = 0;
    const bool   result = ruuvi_nvs_open(NVS_READONLY, &handle);

    ASSERT_FALSE(result);
    ASSERT_EQ(3U, this->m_nvs_open_from_partition_call_cnt);
    ASSERT_EQ(1U, this->m_nvs_close_call_cnt);
    // ESP_ERR_NVS_INVALID_STATE = 0x110b = 4363
    ASSERT_EQ(
        "W ruuvi_nvs: NVS partition nvs, namespace 'ruuvi_gateway' doesn't exist and mode is NVS_READONLY, "
        "try to create it\n"
        "I ruuvi_nvs: ### NVS partition nvs, namespace 'ruuvi_gateway' created successfully\n"
        "E ruuvi_nvs: Can't open NVS partition nvs, namespace: 'ruuvi_gateway', err=4363 (Error 0x110b(4363))\n",
        esp_log_wrapper_get_logs());
    ASSERT_TRUE(this->m_mem_alloc_trace.is_empty());
}

TEST_F(TestRuuviNvs, test_ruuvi_nvs_open_other_error) // NOLINT
{
    // Some other error (not NOT_INITIALIZED, not NOT_FOUND)
    this->m_nvs_open_from_partition_results = { ESP_ERR_NVS_INVALID_HANDLE };

    nvs_handle_t handle = 0;
    const bool   result = ruuvi_nvs_open(NVS_READONLY, &handle);

    ASSERT_FALSE(result);
    ASSERT_EQ(1U, this->m_nvs_open_from_partition_call_cnt);
    ASSERT_EQ(0U, this->m_nvs_close_call_cnt);
    // ESP_ERR_NVS_INVALID_HANDLE = 0x1107 = 4359
    ASSERT_EQ(
        "E ruuvi_nvs: Can't open NVS partition nvs, namespace: 'ruuvi_gateway', err=4359 (Error 0x1107(4359))\n",
        esp_log_wrapper_get_logs());
    ASSERT_TRUE(this->m_mem_alloc_trace.is_empty());
}

// ============================================================================================
// ruuvi_nvs_open_gw_cfg_storage (via ruuvi_nvs_open_partition with "gw_cfg_def")
// ============================================================================================

TEST_F(TestRuuviNvs, test_ruuvi_nvs_open_gw_cfg_storage_ok) // NOLINT
{
    this->m_nvs_open_from_partition_results = { ESP_OK };

    nvs_handle_t handle = 0;
    const bool   result = ruuvi_nvs_open_gw_cfg_storage(NVS_READWRITE, &handle);

    ASSERT_TRUE(result);
    ASSERT_EQ(MOCK_NVS_VALID_HANDLE, handle);
    ASSERT_EQ(1U, this->m_nvs_open_from_partition_call_cnt);
    ASSERT_EQ(0U, this->m_nvs_close_call_cnt);
    ASSERT_EQ("", esp_log_wrapper_get_logs());
    ASSERT_TRUE(this->m_mem_alloc_trace.is_empty());
}

TEST_F(TestRuuviNvs, test_ruuvi_nvs_open_gw_cfg_storage_not_initialized) // NOLINT
{
    this->m_nvs_open_from_partition_results = { ESP_ERR_NVS_NOT_INITIALIZED };

    nvs_handle_t handle = 0;
    const bool   result = ruuvi_nvs_open_gw_cfg_storage(NVS_READWRITE, &handle);

    ASSERT_FALSE(result);
    ASSERT_EQ(1U, this->m_nvs_open_from_partition_call_cnt);
    ASSERT_EQ(0U, this->m_nvs_close_call_cnt);
    ASSERT_EQ(
        "W ruuvi_nvs: NVS partition gw_cfg_def, namespace 'ruuvi_gateway': StorageState is INVALID, "
        "need to erase NVS\n",
        esp_log_wrapper_get_logs());
    ASSERT_TRUE(this->m_mem_alloc_trace.is_empty());
}

TEST_F(TestRuuviNvs, test_ruuvi_nvs_open_gw_cfg_storage_not_found_then_create_ok_reopen_ok) // NOLINT
{
    this->m_nvs_open_from_partition_results = { ESP_ERR_NVS_NOT_FOUND, ESP_OK, ESP_OK };

    nvs_handle_t handle = 0;
    const bool   result = ruuvi_nvs_open_gw_cfg_storage(NVS_READONLY, &handle);

    ASSERT_TRUE(result);
    ASSERT_EQ(MOCK_NVS_VALID_HANDLE, handle);
    ASSERT_EQ(3U, this->m_nvs_open_from_partition_call_cnt);
    ASSERT_EQ(1U, this->m_nvs_close_call_cnt);
    ASSERT_EQ(
        "W ruuvi_nvs: NVS partition gw_cfg_def, namespace 'ruuvi_gateway' doesn't exist and mode is NVS_READONLY, "
        "try to create it\n"
        "I ruuvi_nvs: ### NVS partition gw_cfg_def, namespace 'ruuvi_gateway' created successfully\n",
        esp_log_wrapper_get_logs());
    ASSERT_TRUE(this->m_mem_alloc_trace.is_empty());
}

TEST_F(TestRuuviNvs, test_ruuvi_nvs_open_gw_cfg_storage_other_error) // NOLINT
{
    this->m_nvs_open_from_partition_results = { ESP_ERR_NVS_INVALID_HANDLE };

    nvs_handle_t handle = 0;
    const bool   result = ruuvi_nvs_open_gw_cfg_storage(NVS_READWRITE, &handle);

    ASSERT_FALSE(result);
    ASSERT_EQ(1U, this->m_nvs_open_from_partition_call_cnt);
    ASSERT_EQ(0U, this->m_nvs_close_call_cnt);
    ASSERT_EQ(
        "E ruuvi_nvs: Can't open NVS partition gw_cfg_def, namespace: 'ruuvi_gateway', "
        "err=4359 (Error 0x1107(4359))\n",
        esp_log_wrapper_get_logs());
    ASSERT_TRUE(this->m_mem_alloc_trace.is_empty());
}

TEST_F(TestRuuviNvs, test_ruuvi_nvs_open_gw_cfg_storage_not_found_then_create_fail) // NOLINT
{
    this->m_nvs_open_from_partition_results = { ESP_ERR_NVS_NOT_FOUND, ESP_ERR_NVS_PART_NOT_FOUND };

    nvs_handle_t handle = 0;
    const bool   result = ruuvi_nvs_open_gw_cfg_storage(NVS_READONLY, &handle);

    ASSERT_FALSE(result);
    ASSERT_EQ(2U, this->m_nvs_open_from_partition_call_cnt);
    ASSERT_EQ(0U, this->m_nvs_close_call_cnt);
    // ESP_ERR_NVS_PART_NOT_FOUND = 0x110f = 4367
    ASSERT_EQ(
        "W ruuvi_nvs: NVS partition gw_cfg_def, namespace 'ruuvi_gateway' doesn't exist and mode is NVS_READONLY, "
        "try to create it\n"
        "E ruuvi_nvs: Can't open NVS for writing, err=4367 (Error 0x110f(4367))\n",
        esp_log_wrapper_get_logs());
    ASSERT_TRUE(this->m_mem_alloc_trace.is_empty());
}

TEST_F(TestRuuviNvs, test_ruuvi_nvs_open_gw_cfg_storage_not_found_commit_fail) // NOLINT
{
    this->m_nvs_open_from_partition_results = { ESP_ERR_NVS_NOT_FOUND, ESP_OK };
    this->m_nvs_commit_result               = ESP_ERR_NVS_INVALID_STATE;

    nvs_handle_t handle = 0;
    const bool   result = ruuvi_nvs_open_gw_cfg_storage(NVS_READONLY, &handle);

    ASSERT_FALSE(result);
    ASSERT_EQ(2U, this->m_nvs_open_from_partition_call_cnt);
    ASSERT_EQ(1U, this->m_nvs_close_call_cnt);
    ASSERT_EQ(
        "W ruuvi_nvs: NVS partition gw_cfg_def, namespace 'ruuvi_gateway' doesn't exist and mode is NVS_READONLY, "
        "try to create it\n"
        "E ruuvi_nvs: nvs_commit failed, err=4363 (Error 0x110b(4363))\n",
        esp_log_wrapper_get_logs());
    ASSERT_TRUE(this->m_mem_alloc_trace.is_empty());
}

TEST_F(TestRuuviNvs, test_ruuvi_nvs_open_gw_cfg_storage_not_found_create_ok_reopen_fail) // NOLINT
{
    this->m_nvs_open_from_partition_results = { ESP_ERR_NVS_NOT_FOUND, ESP_OK, ESP_ERR_NVS_INVALID_STATE };

    nvs_handle_t handle = 0;
    const bool   result = ruuvi_nvs_open_gw_cfg_storage(NVS_READONLY, &handle);

    ASSERT_FALSE(result);
    ASSERT_EQ(3U, this->m_nvs_open_from_partition_call_cnt);
    ASSERT_EQ(1U, this->m_nvs_close_call_cnt);
    ASSERT_EQ(
        "W ruuvi_nvs: NVS partition gw_cfg_def, namespace 'ruuvi_gateway' doesn't exist and mode is NVS_READONLY, "
        "try to create it\n"
        "I ruuvi_nvs: ### NVS partition gw_cfg_def, namespace 'ruuvi_gateway' created successfully\n"
        "E ruuvi_nvs: Can't open NVS partition gw_cfg_def, namespace: 'ruuvi_gateway', "
        "err=4363 (Error 0x110b(4363))\n",
        esp_log_wrapper_get_logs());
    ASSERT_TRUE(this->m_mem_alloc_trace.is_empty());
}
