/**
 * @file test_http_post_event_handler.cpp
 * @author TheSomeMan
 * @date 2026-05-26
 * @copyright Ruuvi Innovations Ltd, license BSD-3-Clause.
 */

#include "http_post_event_handler.h"
#include "http.h"
#include <cstring>
#include <string>
#include "gtest/gtest.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log_wrapper.hpp"
#include "os_malloc.h"
#include "os_mutex.h"
#include "os_task.h"
#include "ruuvi_gateway.h"

using namespace std;

/*** Google-test class implementation
 * *********************************************************************************/

class TestHttpPostEventHandler;
static TestHttpPostEventHandler* g_pTestClass;

/*** Test class *********************************************************************************/

class TestHttpPostEventHandler : public ::testing::Test
{
private:
protected:
    void
    SetUp() override
    {
        os_malloc_trace_init();
        esp_log_wrapper_init();
        g_pTestClass = this;

        this->m_malloc_cnt                  = 0;
        this->m_malloc_fail_on_cnt          = 0;
        this->m_alloc_free_call_count       = 0;
        this->m_flag_alloc_counting_enabled = true;

        this->m_hmac_key_set_called           = false;
        this->m_hmac_key_value                = "";
        this->m_mock_adv_post_set_hmac_result = true;
        this->m_default_period_set_called     = false;
        this->m_default_period_ms             = 0;

        esp_log_wrapper_clear();
    }

    void
    TearDown() override
    {
        this->m_flag_alloc_counting_enabled = false;
        this->m_alloc_free_call_count       = 0;
        g_pTestClass                        = nullptr;
        os_malloc_trace_deinit();
        esp_log_wrapper_deinit();
    }

public:
    TestHttpPostEventHandler();
    ~TestHttpPostEventHandler() override;

    uint32_t m_malloc_cnt;
    uint32_t m_malloc_fail_on_cnt;

    bool m_flag_alloc_counting_enabled;
    int  m_alloc_free_call_count;

    bool     m_hmac_key_set_called;
    string   m_hmac_key_value;
    bool     m_mock_adv_post_set_hmac_result;
    bool     m_default_period_set_called;
    uint32_t m_default_period_ms;
};

TestHttpPostEventHandler::TestHttpPostEventHandler()
    : m_malloc_cnt(0)
    , m_malloc_fail_on_cnt(0)
    , m_flag_alloc_counting_enabled(false)
    , m_alloc_free_call_count(0)
    , m_hmac_key_set_called(false)
    , m_mock_adv_post_set_hmac_result(true)
    , m_default_period_set_called(false)
    , m_default_period_ms(0)
    , Test()
{
}

TestHttpPostEventHandler::~TestHttpPostEventHandler() = default;

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

TickType_t
xTaskGetTickCount(void)
{
    return 0;
}

void*
__wrap_malloc(size_t size)
{
    extern void* __real_malloc(size_t _size) __THROW __attribute_malloc__ __attribute_alloc_size__((1)) __wur;
    extern void                                                           __real_free(void* _ptr) __THROW;

    void* ptr = __real_malloc(size);
    assert(nullptr != ptr);
    if (nullptr == ptr)
    {
        return nullptr;
    }
    if (g_pTestClass && g_pTestClass->m_malloc_fail_on_cnt > 0)
    {
        g_pTestClass->m_malloc_cnt += 1;
        if (g_pTestClass->m_malloc_cnt == g_pTestClass->m_malloc_fail_on_cnt)
        {
            __real_free(ptr);
            return nullptr;
        }
    }
    if (g_pTestClass && g_pTestClass->m_flag_alloc_counting_enabled)
    {
        g_pTestClass->m_alloc_free_call_count += 1;
    }
    return ptr;
}

void*
__wrap_calloc(size_t nmemb, size_t size)
{
    extern void*                     __real_calloc(size_t _nmemb, size_t _size)
        __THROW __attribute_malloc__ __attribute_alloc_size__((1, 2)) __wur;
    extern void                      __real_free(void* _ptr) __THROW;

    void* ptr = __real_calloc(nmemb, size);
    assert(nullptr != ptr);
    if (nullptr == ptr)
    {
        return nullptr;
    }
    if (g_pTestClass && g_pTestClass->m_malloc_fail_on_cnt > 0)
    {
        g_pTestClass->m_malloc_cnt += 1;
        if (g_pTestClass->m_malloc_cnt == g_pTestClass->m_malloc_fail_on_cnt)
        {
            __real_free(ptr);
            return nullptr;
        }
    }
    if (g_pTestClass && g_pTestClass->m_flag_alloc_counting_enabled)
    {
        g_pTestClass->m_alloc_free_call_count += 1;
    }
    return ptr;
}

void
__wrap_free(void* ptr)
{
    extern void __real_free(void* _ptr) __THROW;

    __real_free(ptr);
    if (nullptr == ptr)
    {
        return;
    }
    if (g_pTestClass && g_pTestClass->m_flag_alloc_counting_enabled)
    {
        g_pTestClass->m_alloc_free_call_count -= 1;
    }
}

TaskHandle_t
xTaskGetCurrentTaskHandle(void)
{
    return nullptr;
}

os_mutex_t
os_mutex_create_static(os_mutex_static_t* const p_mutex_static)
{
    return reinterpret_cast<os_mutex_t>(p_mutex_static);
}

void
os_mutex_delete(os_mutex_t* const ph_mutex)
{
    *ph_mutex = NULL;
}

void
os_mutex_lock(os_mutex_t const h_mutex)
{
    (void)h_mutex;
}

void
os_mutex_unlock(os_mutex_t const h_mutex)
{
    (void)h_mutex;
}

bool
adv_post_set_hmac_sha256_key(const char* const p_key_str)
{
    if (nullptr != g_pTestClass)
    {
        g_pTestClass->m_hmac_key_set_called = true;
        g_pTestClass->m_hmac_key_value      = (nullptr != p_key_str) ? p_key_str : "";
        return g_pTestClass->m_mock_adv_post_set_hmac_result;
    }
    return false;
}

void
adv_post_set_default_period(const uint32_t period_ms)
{
    if (nullptr != g_pTestClass)
    {
        g_pTestClass->m_default_period_set_called = true;
        g_pTestClass->m_default_period_ms         = period_ms;
    }
}

} // extern "C"

/*** Tests **************************************************************************************/

TEST_F(TestHttpPostEventHandler, test_event_handler_error) // NOLINT
{
    esp_http_client_event_t evt = {};
    evt.event_id                = HTTP_EVENT_ERROR;
    ASSERT_EQ(ESP_OK, http_post_event_handler(&evt));
    ASSERT_EQ("E http: HTTP_EVENT_ERROR\n", esp_log_wrapper_get_logs());
}

TEST_F(TestHttpPostEventHandler, test_event_handler_on_connected) // NOLINT
{
    esp_http_client_event_t evt = {};
    evt.event_id                = HTTP_EVENT_ON_CONNECTED;
    ASSERT_EQ(ESP_OK, http_post_event_handler(&evt));
    // LOG_DBG is not emitted at LOG_LOCAL_LEVEL = LOG_LEVEL_INFO
    ASSERT_EQ("", esp_log_wrapper_get_logs());
}

TEST_F(TestHttpPostEventHandler, test_event_handler_headers_sent) // NOLINT
{
    esp_http_client_event_t evt = {};
    evt.event_id                = HTTP_EVENT_HEADERS_SENT;
    ASSERT_EQ(ESP_OK, http_post_event_handler(&evt));
    ASSERT_EQ("", esp_log_wrapper_get_logs());
}

TEST_F(TestHttpPostEventHandler, test_event_handler_on_finish) // NOLINT
{
    esp_http_client_event_t evt = {};
    evt.event_id                = HTTP_EVENT_ON_FINISH;
    ASSERT_EQ(ESP_OK, http_post_event_handler(&evt));
    ASSERT_EQ("", esp_log_wrapper_get_logs());
}

TEST_F(TestHttpPostEventHandler, test_event_handler_disconnected) // NOLINT
{
    esp_http_client_event_t evt = {};
    evt.event_id                = HTTP_EVENT_DISCONNECTED;
    ASSERT_EQ(ESP_OK, http_post_event_handler(&evt));
    ASSERT_EQ("", esp_log_wrapper_get_logs());
}

TEST_F(TestHttpPostEventHandler, test_event_handler_default_event) // NOLINT
{
    esp_http_client_event_t evt = {};
    evt.event_id                = static_cast<esp_http_client_event_id_t>(999);
    ASSERT_EQ(ESP_OK, http_post_event_handler(&evt));
    ASSERT_EQ("", esp_log_wrapper_get_logs());
}

TEST_F(TestHttpPostEventHandler, test_event_handler_on_header_ruuvi_hmac_key_ok) // NOLINT
{
    esp_http_client_event_t evt = {};
    evt.event_id                = HTTP_EVENT_ON_HEADER;
    char header_key[]           = "Ruuvi-HMAC-KEY";
    char header_value[]         = "my_hmac_key_123";
    evt.header_key              = header_key;
    evt.header_value            = header_value;
    evt.user_data               = nullptr;
    ASSERT_EQ(ESP_OK, http_post_event_handler(&evt));
    ASSERT_TRUE(this->m_hmac_key_set_called);
    ASSERT_EQ("my_hmac_key_123", this->m_hmac_key_value);
    ASSERT_EQ("", esp_log_wrapper_get_logs());
}

TEST_F(TestHttpPostEventHandler, test_event_handler_on_header_ruuvi_hmac_key_case_insensitive) // NOLINT
{
    esp_http_client_event_t evt = {};
    evt.event_id                = HTTP_EVENT_ON_HEADER;
    char header_key[]           = "ruuvi-hmac-key";
    char header_value[]         = "another_key";
    evt.header_key              = header_key;
    evt.header_value            = header_value;
    evt.user_data               = nullptr;
    ASSERT_EQ(ESP_OK, http_post_event_handler(&evt));
    ASSERT_TRUE(this->m_hmac_key_set_called);
    ASSERT_EQ("another_key", this->m_hmac_key_value);
    ASSERT_EQ("", esp_log_wrapper_get_logs());
}

TEST_F(TestHttpPostEventHandler, test_event_handler_on_header_ruuvi_hmac_key_fail) // NOLINT
{
    esp_http_client_event_t evt           = {};
    evt.event_id                          = HTTP_EVENT_ON_HEADER;
    char header_key[]                     = "Ruuvi-HMAC-KEY";
    char header_value[]                   = "bad_key";
    evt.header_key                        = header_key;
    evt.header_value                      = header_value;
    evt.user_data                         = nullptr;
    this->m_mock_adv_post_set_hmac_result = false;
    ASSERT_EQ(ESP_OK, http_post_event_handler(&evt));
    ASSERT_TRUE(this->m_hmac_key_set_called);
    ASSERT_EQ("E http: Failed to update Ruuvi-HMAC-KEY\n", esp_log_wrapper_get_logs());
}

TEST_F(TestHttpPostEventHandler, test_event_handler_on_header_x_ruuvi_gateway_rate_valid) // NOLINT
{
    esp_http_client_event_t evt = {};
    evt.event_id                = HTTP_EVENT_ON_HEADER;
    char header_key[]           = "X-Ruuvi-Gateway-Rate";
    char header_value[]         = "30";
    evt.header_key              = header_key;
    evt.header_value            = header_value;
    evt.user_data               = nullptr;
    ASSERT_EQ(ESP_OK, http_post_event_handler(&evt));
    ASSERT_TRUE(this->m_default_period_set_called);
    ASSERT_EQ(30U * 1000U, this->m_default_period_ms);
    ASSERT_EQ("", esp_log_wrapper_get_logs());
}

TEST_F(TestHttpPostEventHandler, test_event_handler_on_header_x_ruuvi_gateway_rate_1_second) // NOLINT
{
    esp_http_client_event_t evt = {};
    evt.event_id                = HTTP_EVENT_ON_HEADER;
    char header_key[]           = "X-Ruuvi-Gateway-Rate";
    char header_value[]         = "1";
    evt.header_key              = header_key;
    evt.header_value            = header_value;
    evt.user_data               = nullptr;
    ASSERT_EQ(ESP_OK, http_post_event_handler(&evt));
    ASSERT_TRUE(this->m_default_period_set_called);
    ASSERT_EQ(1U * 1000U, this->m_default_period_ms);
    ASSERT_EQ("", esp_log_wrapper_get_logs());
}

TEST_F(TestHttpPostEventHandler, test_event_handler_on_header_x_ruuvi_gateway_rate_max_valid) // NOLINT
{
    // Max valid = 60 * 60 = 3600
    esp_http_client_event_t evt = {};
    evt.event_id                = HTTP_EVENT_ON_HEADER;
    char header_key[]           = "X-Ruuvi-Gateway-Rate";
    char header_value[]         = "3600";
    evt.header_key              = header_key;
    evt.header_value            = header_value;
    evt.user_data               = nullptr;
    ASSERT_EQ(ESP_OK, http_post_event_handler(&evt));
    ASSERT_TRUE(this->m_default_period_set_called);
    ASSERT_EQ(3600U * 1000U, this->m_default_period_ms);
    ASSERT_EQ("", esp_log_wrapper_get_logs());
}

TEST_F(TestHttpPostEventHandler, test_event_handler_on_header_x_ruuvi_gateway_rate_zero) // NOLINT
{
    // period_seconds == 0 -> fallback to default
    esp_http_client_event_t evt = {};
    evt.event_id                = HTTP_EVENT_ON_HEADER;
    char header_key[]           = "X-Ruuvi-Gateway-Rate";
    char header_value[]         = "0";
    evt.header_key              = header_key;
    evt.header_value            = header_value;
    evt.user_data               = nullptr;
    ASSERT_EQ(ESP_OK, http_post_event_handler(&evt));
    ASSERT_TRUE(this->m_default_period_set_called);
    ASSERT_EQ(static_cast<uint32_t>(ADV_POST_DEFAULT_INTERVAL_SECONDS) * 1000U, this->m_default_period_ms);
    ASSERT_EQ("W http: X-Ruuvi-Gateway-Rate: Got incorrect value: 0\n", esp_log_wrapper_get_logs());
}

TEST_F(TestHttpPostEventHandler, test_event_handler_on_header_x_ruuvi_gateway_rate_too_large) // NOLINT
{
    // period_seconds > 3600 -> fallback to default
    esp_http_client_event_t evt = {};
    evt.event_id                = HTTP_EVENT_ON_HEADER;
    char header_key[]           = "X-Ruuvi-Gateway-Rate";
    char header_value[]         = "3601";
    evt.header_key              = header_key;
    evt.header_value            = header_value;
    evt.user_data               = nullptr;
    ASSERT_EQ(ESP_OK, http_post_event_handler(&evt));
    ASSERT_TRUE(this->m_default_period_set_called);
    ASSERT_EQ(static_cast<uint32_t>(ADV_POST_DEFAULT_INTERVAL_SECONDS) * 1000U, this->m_default_period_ms);
    ASSERT_EQ("W http: X-Ruuvi-Gateway-Rate: Got incorrect value: 3601\n", esp_log_wrapper_get_logs());
}

TEST_F(TestHttpPostEventHandler, test_event_handler_on_header_x_ruuvi_gateway_rate_non_numeric) // NOLINT
{
    // Non-numeric string -> strtoul returns 0 -> fallback to default
    esp_http_client_event_t evt = {};
    evt.event_id                = HTTP_EVENT_ON_HEADER;
    char header_key[]           = "X-Ruuvi-Gateway-Rate";
    char header_value[]         = "abc";
    evt.header_key              = header_key;
    evt.header_value            = header_value;
    evt.user_data               = nullptr;
    ASSERT_EQ(ESP_OK, http_post_event_handler(&evt));
    ASSERT_TRUE(this->m_default_period_set_called);
    ASSERT_EQ(static_cast<uint32_t>(ADV_POST_DEFAULT_INTERVAL_SECONDS) * 1000U, this->m_default_period_ms);
    ASSERT_EQ("W http: X-Ruuvi-Gateway-Rate: Got incorrect value: abc\n", esp_log_wrapper_get_logs());
}

TEST_F(TestHttpPostEventHandler, test_event_handler_on_header_content_length_with_user_data) // NOLINT
{
    http_resp_cb_info_t     cb_info = {};
    esp_http_client_event_t evt     = {};
    evt.event_id                    = HTTP_EVENT_ON_HEADER;
    char header_key[]               = "Content-Length";
    char header_value[]             = "42";
    evt.header_key                  = header_key;
    evt.header_value                = header_value;
    evt.user_data                   = &cb_info;
    ASSERT_EQ(ESP_OK, http_post_event_handler(&evt));
    ASSERT_EQ(42U, cb_info.content_length);
    ASSERT_EQ(0U, cb_info.offset);
    ASSERT_NE(nullptr, cb_info.p_buf);
    ASSERT_EQ('\0', cb_info.p_buf[0]);
    os_free(cb_info.p_buf);
    ASSERT_EQ("", esp_log_wrapper_get_logs());
    ASSERT_EQ(0, this->m_alloc_free_call_count);
}

TEST_F(TestHttpPostEventHandler, test_event_handler_on_header_content_length_no_user_data) // NOLINT
{
    // Content-Length header with user_data == NULL -> falls to else branch
    esp_http_client_event_t evt = {};
    evt.event_id                = HTTP_EVENT_ON_HEADER;
    char header_key[]           = "Content-Length";
    char header_value[]         = "42";
    evt.header_key              = header_key;
    evt.header_value            = header_value;
    evt.user_data               = nullptr;
    ASSERT_EQ(ESP_OK, http_post_event_handler(&evt));
    ASSERT_EQ("", esp_log_wrapper_get_logs());
}

TEST_F(TestHttpPostEventHandler, test_event_handler_on_header_content_length_malloc_fail) // NOLINT
{
    http_resp_cb_info_t     cb_info = {};
    esp_http_client_event_t evt     = {};
    evt.event_id                    = HTTP_EVENT_ON_HEADER;
    char header_key[]               = "Content-Length";
    char header_value[]             = "100";
    evt.header_key                  = header_key;
    evt.header_value                = header_value;
    evt.user_data                   = &cb_info;
    this->m_malloc_fail_on_cnt      = 1;
    ASSERT_EQ(ESP_OK, http_post_event_handler(&evt));
    ASSERT_EQ(100U, cb_info.content_length);
    ASSERT_EQ(nullptr, cb_info.p_buf);
    ASSERT_EQ("E http: Can't allocate 100 bytes for HTTP response\n", esp_log_wrapper_get_logs());
    ASSERT_EQ(0, this->m_alloc_free_call_count);
}

TEST_F(TestHttpPostEventHandler, test_event_handler_on_header_unknown_header) // NOLINT
{
    // Unknown header -> falls to else (MISRA) branch
    esp_http_client_event_t evt = {};
    evt.event_id                = HTTP_EVENT_ON_HEADER;
    char header_key[]           = "X-Custom-Header";
    char header_value[]         = "some_value";
    evt.header_key              = header_key;
    evt.header_value            = header_value;
    evt.user_data               = nullptr;
    ASSERT_EQ(ESP_OK, http_post_event_handler(&evt));
    ASSERT_FALSE(this->m_hmac_key_set_called);
    ASSERT_FALSE(this->m_default_period_set_called);
    ASSERT_EQ("", esp_log_wrapper_get_logs());
}

TEST_F(TestHttpPostEventHandler, test_event_handler_on_data_with_buffer) // NOLINT
{
    // Simulate receiving data into a pre-allocated buffer
    http_resp_cb_info_t cb_info = {};
    cb_info.content_length      = 20;
    cb_info.offset              = 0;
    cb_info.p_buf               = static_cast<char*>(os_malloc(21));
    ASSERT_NE(nullptr, cb_info.p_buf);
    cb_info.p_buf[0] = '\0';

    esp_http_client_event_t evt = {};
    evt.event_id                = HTTP_EVENT_ON_DATA;
    char data[]                 = "Hello, World!";
    evt.data                    = data;
    evt.data_len                = static_cast<int>(strlen(data));
    evt.user_data               = &cb_info;
    ASSERT_EQ(ESP_OK, http_post_event_handler(&evt));
    ASSERT_EQ(static_cast<uint32_t>(strlen(data)), cb_info.offset);
    ASSERT_STREQ("Hello, World!", cb_info.p_buf);
    os_free(cb_info.p_buf);
    ASSERT_EQ("", esp_log_wrapper_get_logs());
    ASSERT_EQ(0, this->m_alloc_free_call_count);
}

TEST_F(TestHttpPostEventHandler, test_event_handler_on_data_multiple_chunks) // NOLINT
{
    http_resp_cb_info_t cb_info = {};
    cb_info.content_length      = 20;
    cb_info.offset              = 0;
    cb_info.p_buf               = static_cast<char*>(os_malloc(21));
    ASSERT_NE(nullptr, cb_info.p_buf);
    cb_info.p_buf[0] = '\0';

    esp_http_client_event_t evt = {};
    evt.event_id                = HTTP_EVENT_ON_DATA;
    evt.user_data               = &cb_info;

    // First chunk
    char data1[] = "Hello, ";
    evt.data     = data1;
    evt.data_len = static_cast<int>(strlen(data1));
    ASSERT_EQ(ESP_OK, http_post_event_handler(&evt));
    ASSERT_EQ(7U, cb_info.offset);

    // Second chunk
    char data2[] = "World!";
    evt.data     = data2;
    evt.data_len = static_cast<int>(strlen(data2));
    ASSERT_EQ(ESP_OK, http_post_event_handler(&evt));
    ASSERT_EQ(13U, cb_info.offset);
    ASSERT_STREQ("Hello, World!", cb_info.p_buf);

    os_free(cb_info.p_buf);
    ASSERT_EQ("", esp_log_wrapper_get_logs());
    ASSERT_EQ(0, this->m_alloc_free_call_count);
}

TEST_F(TestHttpPostEventHandler, test_event_handler_on_data_overflow_truncated) // NOLINT
{
    // Buffer is smaller than data -> data should be truncated
    http_resp_cb_info_t cb_info = {};
    cb_info.content_length      = 5;
    cb_info.offset              = 0;
    cb_info.p_buf               = static_cast<char*>(os_malloc(6));
    ASSERT_NE(nullptr, cb_info.p_buf);
    cb_info.p_buf[0] = '\0';

    esp_http_client_event_t evt = {};
    evt.event_id                = HTTP_EVENT_ON_DATA;
    char data[]                 = "Hello, World!";
    evt.data                    = data;
    evt.data_len                = static_cast<int>(strlen(data));
    evt.user_data               = &cb_info;
    ASSERT_EQ(ESP_OK, http_post_event_handler(&evt));
    ASSERT_EQ(5U, cb_info.offset);
    ASSERT_STREQ("Hello", cb_info.p_buf);

    os_free(cb_info.p_buf);
    ASSERT_EQ("", esp_log_wrapper_get_logs());
    ASSERT_EQ(0, this->m_alloc_free_call_count);
}

TEST_F(TestHttpPostEventHandler, test_event_handler_on_data_null_user_data) // NOLINT
{
    esp_http_client_event_t evt = {};
    evt.event_id                = HTTP_EVENT_ON_DATA;
    char data[]                 = "some data";
    evt.data                    = data;
    evt.data_len                = static_cast<int>(strlen(data));
    evt.user_data               = nullptr;
    ASSERT_EQ(ESP_OK, http_post_event_handler(&evt));
    ASSERT_EQ("", esp_log_wrapper_get_logs());
}

TEST_F(TestHttpPostEventHandler, test_event_handler_on_data_null_buffer) // NOLINT
{
    // user_data is set but p_buf is NULL (e.g. malloc failed earlier)
    http_resp_cb_info_t cb_info = {};
    cb_info.content_length      = 10;
    cb_info.offset              = 0;
    cb_info.p_buf               = nullptr;

    esp_http_client_event_t evt = {};
    evt.event_id                = HTTP_EVENT_ON_DATA;
    char data[]                 = "some data";
    evt.data                    = data;
    evt.data_len                = static_cast<int>(strlen(data));
    evt.user_data               = &cb_info;
    ASSERT_EQ(ESP_OK, http_post_event_handler(&evt));
    ASSERT_EQ(nullptr, cb_info.p_buf);
    ASSERT_EQ("", esp_log_wrapper_get_logs());
}

TEST_F(TestHttpPostEventHandler, test_event_handler_on_data_exact_fit) // NOLINT
{
    // Data exactly fills the buffer
    http_resp_cb_info_t cb_info = {};
    cb_info.content_length      = 5;
    cb_info.offset              = 0;
    cb_info.p_buf               = static_cast<char*>(os_malloc(6));
    ASSERT_NE(nullptr, cb_info.p_buf);
    cb_info.p_buf[0] = '\0';

    esp_http_client_event_t evt = {};
    evt.event_id                = HTTP_EVENT_ON_DATA;
    char data[]                 = "ABCDE";
    evt.data                    = data;
    evt.data_len                = 5;
    evt.user_data               = &cb_info;
    ASSERT_EQ(ESP_OK, http_post_event_handler(&evt));
    ASSERT_EQ(5U, cb_info.offset);
    ASSERT_STREQ("ABCDE", cb_info.p_buf);

    os_free(cb_info.p_buf);
    ASSERT_EQ("", esp_log_wrapper_get_logs());
    ASSERT_EQ(0, this->m_alloc_free_call_count);
}
