/**
 * @file test_http_check_post_stat.cpp
 * @author TheSomeMan
 * @date 2026-05-25
 * @copyright Ruuvi Innovations Ltd, license BSD-3-Clause.
 */

#include "http.h"
#include "http_client_config.h"
#include "http_stream_reader_nvs.h"
#include "http_post_event_handler.h"
#include <cstring>
#include <cstdarg>
#include <cstdio>
#include <string>
#include <utility>
#include <vector>
#include "gtest/gtest.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log_wrapper.hpp"
#include "gw_mac.h"
#include "gw_cfg.h"
#include "gw_cfg_default.h"
#include "gw_cfg_storage.h"
#include "str_buf.h"
#include "os_malloc.h"
#include "os_mutex_recursive.h"
#include "os_mutex.h"
#include "os_task.h"

using namespace std;

/*** Google-test class implementation
 * *********************************************************************************/

class TestHttpCheckPostStat;
static TestHttpCheckPostStat* g_pTestClass;

extern "C" {

} // extern "C"

/*** Test class *********************************************************************************/

class TestHttpCheckPostStat : public ::testing::Test
{
private:
protected:
    void
    SetUp() override
    {
        os_malloc_trace_init();
        esp_log_wrapper_init();
        g_pTestClass = this;

        this->m_malloc_cnt                            = 0;
        this->m_malloc_fail_on_cnt                    = 0;
        this->m_flag_settings_saved_to_flash          = false;
        this->m_mock_esp_http_client_cleanup_called   = 0;
        this->m_mock_http_async_info_free_data_called = 0;

        this->m_mock_async_resp_set = false;
        memset(&this->m_mock_async_resp, 0, sizeof(this->m_mock_async_resp));

        esp_log_wrapper_clear();
        this->m_malloc_cnt = 0;

        this->m_alloc_free_call_count       = 0;
        this->m_flag_alloc_counting_enabled = true;

        this->m_mock_gw_status_relaying_via_http = false;
        this->m_mock_esp_http_client_handle      = reinterpret_cast<esp_http_client_handle_t>(0x12345678);
        this->m_mock_esp_http_client_init_result = this->m_mock_esp_http_client_handle;
        this->m_mock_http_send_async_result      = true;
        this->m_mock_http_wait_resp_code         = HTTP_RESP_CODE_200;
        this->m_mock_http_wait_resp_body         = "{}";
        this->m_mock_ssl_client_cert             = "";
        this->m_mock_ssl_client_key              = "";
        this->m_mock_ssl_server_cert             = "";
        this->m_mock_esp_http_client_config_set_from_url_result = true;
        this->m_mock_adv_post_statistics_info_generate_result   = true;
        this->m_mock_http_json_create_status_str_result         = true;
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
    TestHttpCheckPostStat();
    ~TestHttpCheckPostStat() override;

    uint32_t m_malloc_cnt;
    uint32_t m_malloc_fail_on_cnt;
    bool     m_flag_settings_saved_to_flash;

    bool m_flag_alloc_counting_enabled;
    int  m_alloc_free_call_count;

    bool                     m_mock_gw_status_relaying_via_http;
    esp_http_client_handle_t m_mock_esp_http_client_handle;
    esp_http_client_handle_t m_mock_esp_http_client_init_result;
    bool                     m_mock_http_send_async_result;
    http_resp_code_e         m_mock_http_wait_resp_code;
    string                   m_mock_http_wait_resp_body;
    string                   m_mock_ssl_client_cert;
    string                   m_mock_ssl_client_key;
    string                   m_mock_ssl_server_cert;
    http_server_resp_t       m_mock_async_resp;
    bool                     m_mock_async_resp_set;
    int32_t                  m_mock_esp_http_client_cleanup_called;
    int32_t                  m_mock_http_async_info_free_data_called;
    bool                     m_mock_esp_http_client_config_set_from_url_result;
    bool                     m_mock_adv_post_statistics_info_generate_result;
    bool                     m_mock_http_json_create_status_str_result;

    string m_freed_server_cert;
    string m_freed_client_cert;
    string m_freed_client_key;
};

TestHttpCheckPostStat::TestHttpCheckPostStat()
    : m_malloc_cnt(0)
    , m_malloc_fail_on_cnt(0)
    , m_flag_settings_saved_to_flash(false)
    , m_flag_alloc_counting_enabled(false)
    , m_alloc_free_call_count(0)
    , m_mock_gw_status_relaying_via_http(false)
    , m_mock_esp_http_client_handle(nullptr)
    , m_mock_esp_http_client_init_result(nullptr)
    , m_mock_async_resp()
    , m_mock_async_resp_set(false)
    , m_mock_http_send_async_result(true)
    , m_mock_http_wait_resp_code(HTTP_RESP_CODE_200)
    , m_mock_esp_http_client_cleanup_called(0)
    , m_mock_http_async_info_free_data_called(0)
    , m_mock_esp_http_client_config_set_from_url_result(true)
    , m_mock_adv_post_statistics_info_generate_result(true)
    , m_mock_http_json_create_status_str_result(true)
    , Test()
{
}

TestHttpCheckPostStat::~TestHttpCheckPostStat() = default;

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

uint32_t
esp_random(void)
{
    return 0x12345678;
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

os_mutex_recursive_t
os_mutex_recursive_create_static(os_mutex_recursive_static_t* const p_mutex_static)
{
    return (os_mutex_recursive_t)p_mutex_static;
}

void
os_mutex_recursive_delete(os_mutex_recursive_t* const ph_mutex)
{
}

void
os_mutex_recursive_lock(os_mutex_recursive_t const h_mutex)
{
}

void
os_mutex_recursive_unlock(os_mutex_recursive_t const h_mutex)
{
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
os_mutex_try_lock(os_mutex_t const h_mutex)
{
    return true;
}

os_sema_t
os_sema_create_static(os_sema_static_t* const p_sema_static)
{
    return reinterpret_cast<os_sema_t>(p_sema_static);
}

void
os_sema_delete(os_sema_t* const ph_sema)
{
    *ph_sema = nullptr;
}

bool
os_sema_wait_immediate(os_sema_t const h_sema)
{
    return true;
}

void
os_sema_signal(os_sema_t const h_sema)
{
}

TaskHandle_t
xTaskGetCurrentTaskHandle(void)
{
    return nullptr;
}

int
mbedtls_ctr_drbg_random(void* p_rng, unsigned char* output, size_t output_len)
{
    memset(output, 0, output_len);
    return 0;
}

esp_http_client_handle_t
esp_http_client_init(const esp_http_client_config_t* config)
{
    if (nullptr != g_pTestClass)
    {
        return g_pTestClass->m_mock_esp_http_client_init_result;
    }
    return nullptr;
}

esp_err_t
esp_http_client_cleanup(esp_http_client_handle_t client)
{
    (void)client;
    if (nullptr != g_pTestClass)
    {
        g_pTestClass->m_mock_esp_http_client_cleanup_called += 1;
    }
    return ESP_OK;
}

bool
esp_http_client_config_set_from_url(esp_http_client_config_t* const p_cfg, char* const url)
{
    if (nullptr != g_pTestClass)
    {
        return g_pTestClass->m_mock_esp_http_client_config_set_from_url_result;
    }
    return false;
}

esp_err_t
http_post_event_handler(esp_http_client_event_t* p_evt)
{
    return ESP_OK;
}

ssize_t
http_stream_reader_nvs(const http_stream_reader_cmd_e cmd, const http_stream_reader_arg_t arg, void* const p_ctx)
{
    return -1;
}

const mac_address_str_t*
gw_cfg_get_nrf52_mac_addr(void)
{
    static const mac_address_str_t mac_addr = { "11:22:33:44:55:66" };
    return &mac_addr;
}

str_buf_t
gw_cfg_storage_read_file_as_string(const char* const p_file_name)
{
    if (nullptr != g_pTestClass)
    {
        if (0 == strcmp(p_file_name, GW_CFG_STORAGE_SSL_STAT_CLI_CERT) && !g_pTestClass->m_mock_ssl_client_cert.empty())
        {
            return str_buf_printf_with_alloc("%s", g_pTestClass->m_mock_ssl_client_cert.c_str());
        }
        if (0 == strcmp(p_file_name, GW_CFG_STORAGE_SSL_STAT_CLI_KEY) && !g_pTestClass->m_mock_ssl_client_key.empty())
        {
            return str_buf_printf_with_alloc("%s", g_pTestClass->m_mock_ssl_client_key.c_str());
        }
        if (0 == strcmp(p_file_name, GW_CFG_STORAGE_SSL_STAT_SRV_CERT) && !g_pTestClass->m_mock_ssl_server_cert.empty())
        {
            return str_buf_printf_with_alloc("%s", g_pTestClass->m_mock_ssl_server_cert.c_str());
        }
    }
    return str_buf_init_null();
}

bool
http_send_async(http_async_info_t* const p_http_async_info)
{
    if (nullptr != g_pTestClass)
    {
        return g_pTestClass->m_mock_http_send_async_result;
    }
    return false;
}

void
http_async_info_free_data(http_async_info_t* const p_http_async_info)
{
    if (nullptr != g_pTestClass)
    {
        g_pTestClass->m_freed_server_cert = (NULL
                                             != p_http_async_info->http_client_config.esp_http_client_config.cert_pem)
                                                ? p_http_async_info->http_client_config.esp_http_client_config.cert_pem
                                                : "";
        g_pTestClass->m_freed_client_cert
            = (NULL != p_http_async_info->http_client_config.esp_http_client_config.client_cert_pem)
                  ? p_http_async_info->http_client_config.esp_http_client_config.client_cert_pem
                  : "";
        g_pTestClass->m_freed_client_key
            = (NULL != p_http_async_info->http_client_config.esp_http_client_config.client_key_pem)
                  ? p_http_async_info->http_client_config.esp_http_client_config.client_key_pem
                  : "";
    }
    if (NULL != p_http_async_info->http_client_config.esp_http_client_config.cert_pem)
    {
        os_free(p_http_async_info->http_client_config.esp_http_client_config.cert_pem);
        p_http_async_info->http_client_config.esp_http_client_config.cert_pem = NULL;
    }
    if (NULL != p_http_async_info->http_client_config.esp_http_client_config.client_cert_pem)
    {
        os_free(p_http_async_info->http_client_config.esp_http_client_config.client_cert_pem);
        p_http_async_info->http_client_config.esp_http_client_config.client_cert_pem = NULL;
    }
    if (NULL != p_http_async_info->http_client_config.esp_http_client_config.client_key_pem)
    {
        os_free(p_http_async_info->http_client_config.esp_http_client_config.client_key_pem);
        p_http_async_info->http_client_config.esp_http_client_config.client_key_pem = NULL;
    }
    if (!p_http_async_info->use_json_stream_gen)
    {
        if (NULL != p_http_async_info->select.cjson_str.p_str)
        {
            os_free(p_http_async_info->select.cjson_str.p_str);
            p_http_async_info->select.cjson_str.p_str = NULL;
        }
    }
    if (nullptr != g_pTestClass)
    {
        g_pTestClass->m_mock_http_async_info_free_data_called += 1;
    }
}

http_server_resp_t
http_server_resp_err(const http_resp_code_e http_resp_code)
{
    return (http_server_resp_t) {
        .http_resp_code       = http_resp_code,
        .content_location     = HTTP_CONTENT_LOCATION_NO_CONTENT,
        .flag_no_cache        = true,
        .flag_add_header_date = true,
        .content_type         = HTTP_CONTENT_TYPE_TEXT_HTML,
        .p_content_type_param = NULL,
        .content_len          = 0,
        .content_encoding     = HTTP_CONTENT_ENCODING_NONE,
    };
}

http_server_resp_t
http_server_resp_500(void)
{
    return http_server_resp_err(HTTP_RESP_CODE_500);
}

const uint8_t*
http_server_resp_get_content_ptr_if_in_memory(const http_server_resp_t* const p_resp, size_t* const p_len)
{
    assert(NULL != p_resp);
    assert(NULL != p_len);
    const uint8_t* p_buf = NULL;
    *p_len               = 0;
    switch (p_resp->content_location)
    {
        case HTTP_CONTENT_LOCATION_FLASH_MEM:
            p_buf  = p_resp->select_location.flash.p_buf;
            *p_len = p_resp->content_len;
            break;
        case HTTP_CONTENT_LOCATION_STATIC_MEM:
            p_buf  = p_resp->select_location.static_mem.p_buf;
            *p_len = p_resp->content_len;
            break;
        case HTTP_CONTENT_LOCATION_HEAP:
            p_buf  = (const uint8_t*)p_resp->select_location.heap.p_buf;
            *p_len = p_resp->content_len;
            break;
        default:
            break;
    }
    return p_buf;
}

http_server_resp_t
http_server_cb_gen_resp(const http_resp_code_e resp_code, const char* const p_fmt, ...)
{
    va_list args;
    va_start(args, p_fmt);
    char tmp_buf[1024];
    vsnprintf(tmp_buf, sizeof(tmp_buf), p_fmt, args);
    va_end(args);

    const size_t len   = strlen(tmp_buf);
    char* const  p_buf = static_cast<char*>(os_malloc(len + 1));
    if (nullptr != p_buf)
    {
        memcpy(p_buf, tmp_buf, len + 1);
    }

    const http_server_resp_t resp = {
        .http_resp_code       = resp_code,
        .content_location     = (nullptr != p_buf) ? HTTP_CONTENT_LOCATION_HEAP : HTTP_CONTENT_LOCATION_NO_CONTENT,
        .flag_no_cache        = true,
        .flag_add_header_date = true,
        .content_type         = HTTP_CONTENT_TYPE_APPLICATION_JSON,
        .p_content_type_param = NULL,
        .content_len          = (nullptr != p_buf) ? len : 0,
        .content_encoding     = HTTP_CONTENT_ENCODING_NONE,
        .select_location = {
            .heap = {
                .p_buf = reinterpret_cast<uint8_t*>(p_buf),
            },
        },
    };
    return resp;
}

http_server_resp_t
http_wait_until_async_req_completed(
    esp_http_client_handle_t   p_http_handle,
    http_resp_cb_info_t* const p_cb_info,
    const bool                 flag_feed_task_watchdog,
    const TimeUnitsSeconds_t   timeout_seconds)
{
    (void)p_http_handle;
    (void)p_cb_info;
    (void)flag_feed_task_watchdog;
    (void)timeout_seconds;
    if ((nullptr != g_pTestClass) && g_pTestClass->m_mock_async_resp_set)
    {
        return g_pTestClass->m_mock_async_resp;
    }
    if (nullptr != g_pTestClass)
    {
        // Build response with body content on heap
        const char* const p_body   = g_pTestClass->m_mock_http_wait_resp_body.c_str();
        const size_t      body_len = strlen(p_body);
        char* const       p_buf    = static_cast<char*>(os_malloc(body_len + 1));
        if (nullptr != p_buf)
        {
            memcpy(p_buf, p_body, body_len + 1);
        }
        const http_server_resp_t resp = {
            .http_resp_code       = g_pTestClass->m_mock_http_wait_resp_code,
            .content_location     = HTTP_CONTENT_LOCATION_HEAP,
            .flag_no_cache        = true,
            .flag_add_header_date = true,
            .content_type         = HTTP_CONTENT_TYPE_APPLICATION_JSON,
            .p_content_type_param = NULL,
            .content_len          = body_len,
            .content_encoding     = HTTP_CONTENT_ENCODING_NONE,
            .select_location = {
                .heap = {
                    .p_buf = reinterpret_cast<uint8_t*>(p_buf),
                },
            },
        };
        return resp;
    }
    const http_server_resp_t resp = {
        .http_resp_code       = HTTP_RESP_CODE_200,
        .content_location     = HTTP_CONTENT_LOCATION_NO_CONTENT,
        .flag_no_cache        = true,
        .flag_add_header_date = true,
        .content_type         = HTTP_CONTENT_TYPE_TEXT_HTML,
        .p_content_type_param = NULL,
        .content_len          = 0,
        .content_encoding     = HTTP_CONTENT_ENCODING_NONE,
    };
    return resp;
}

void
http_server_resp_free(http_server_resp_t* const p_resp)
{
    if (NULL == p_resp)
    {
        return;
    }
    if (HTTP_CONTENT_LOCATION_HEAP == p_resp->content_location)
    {
        if (NULL != p_resp->select_location.heap.p_buf)
        {
            os_free(p_resp->select_location.heap.p_buf);
            p_resp->select_location.heap.p_buf = NULL;
        }
    }
    p_resp->content_location = HTTP_CONTENT_LOCATION_NO_CONTENT;
    p_resp->content_len      = 0;
}

bool
hmac_sha256_calc_for_stats(const char* const p_str, hmac_sha256_t* const p_hmac_sha256)
{
    if (NULL != p_hmac_sha256)
    {
        memset(p_hmac_sha256, 0, sizeof(*p_hmac_sha256));
    }
    return true;
}

bool
gw_status_is_relaying_via_http_enabled(void)
{
    if (nullptr != g_pTestClass)
    {
        return g_pTestClass->m_mock_gw_status_relaying_via_http;
    }
    return false;
}

static http_json_statistics_info_t g_mock_stat_info;

const http_json_statistics_info_t*
adv_post_statistics_info_generate(const str_buf_t* const p_reset_info)
{
    if (nullptr != g_pTestClass)
    {
        if (!g_pTestClass->m_mock_adv_post_statistics_info_generate_result)
        {
            return NULL;
        }
    }
    http_json_statistics_info_t* p_stat_info = static_cast<http_json_statistics_info_t*>(
        os_malloc(sizeof(http_json_statistics_info_t)));
    if (NULL == p_stat_info)
    {
        return NULL;
    }
    memset(p_stat_info, 0, sizeof(*p_stat_info));
    p_stat_info->p_reset_info = "test_reset_info";
    return p_stat_info;
}

bool
http_json_create_status_str(
    const http_json_statistics_info_t* const p_stat_info,
    const adv_report_table_t* const          p_reports,
    cjson_wrap_str_t* const                  p_json_str)
{
    if (nullptr != g_pTestClass)
    {
        if (!g_pTestClass->m_mock_http_json_create_status_str_result)
        {
            return false;
        }
    }
    const char*  mock_json = "{\"status\":\"ok\"}";
    const size_t len       = strlen(mock_json);
    char*        p_buf     = static_cast<char*>(os_malloc(len + 1));
    if (NULL == p_buf)
    {
        return false;
    }
    memcpy(p_buf, mock_json, len + 1);
    p_json_str->p_str = p_buf;
    return true;
}

static http_async_info_t g_test_http_async_info;

http_async_info_t*
http_get_async_info(void)
{
    http_async_info_t* p = &g_test_http_async_info;
    if (NULL == p->p_http_async_sema)
    {
        p->p_http_async_sema = os_sema_create_static(&p->http_async_sema_mem);
        os_sema_signal(p->p_http_async_sema);
    }
    return p;
}

} // extern "C"

/*** Tests **************************************************************************************/

TEST_F(TestHttpCheckPostStat, test_url_null) // NOLINT
{
    const http_check_params_t params = {
        .p_url               = NULL,
        .auth_type           = GW_CFG_HTTP_AUTH_TYPE_NONE,
        .p_user              = NULL,
        .p_pass              = NULL,
        .use_ssl_client_cert = false,
        .use_ssl_server_cert = false,
    };
    http_server_resp_t resp = http_check_post_stat(&params, 10);
    ASSERT_EQ(HTTP_RESP_CODE_400, resp.http_resp_code);
    http_server_resp_free(&resp);
    ASSERT_EQ("", esp_log_wrapper_get_logs());
    ASSERT_EQ(0, this->m_alloc_free_call_count);
}

TEST_F(TestHttpCheckPostStat, test_url_too_long) // NOLINT
{
    // Create a URL that exceeds sizeof(ruuvi_gw_cfg_http_url_t)
    string                    long_url(sizeof(ruuvi_gw_cfg_http_url_t), 'x');
    const http_check_params_t params = {
        .p_url               = long_url.c_str(),
        .auth_type           = GW_CFG_HTTP_AUTH_TYPE_NONE,
        .p_user              = NULL,
        .p_pass              = NULL,
        .use_ssl_client_cert = false,
        .use_ssl_server_cert = false,
    };
    http_server_resp_t resp = http_check_post_stat(&params, 10);
    ASSERT_EQ(HTTP_RESP_CODE_400, resp.http_resp_code);
    http_server_resp_free(&resp);
    ASSERT_EQ("", esp_log_wrapper_get_logs());
    ASSERT_EQ(0, this->m_alloc_free_call_count);
}

TEST_F(TestHttpCheckPostStat, test_auth_none_ok) // NOLINT
{
    const http_check_params_t params = {
        .p_url               = "https://myserver.com/stat",
        .auth_type           = GW_CFG_HTTP_AUTH_TYPE_NONE,
        .p_user              = NULL,
        .p_pass              = NULL,
        .use_ssl_client_cert = false,
        .use_ssl_server_cert = false,
    };
    this->m_mock_http_wait_resp_code = HTTP_RESP_CODE_200;
    this->m_mock_http_wait_resp_body = "{\"status\":200}";
    http_server_resp_t resp          = http_check_post_stat(&params, 10);
    ASSERT_EQ(HTTP_RESP_CODE_200, resp.http_resp_code);
    const esp_http_client_config_t& cfg = g_test_http_async_info.http_client_config.esp_http_client_config;
    ASSERT_EQ(nullptr, cfg.client_cert_pem);
    ASSERT_EQ(nullptr, cfg.client_key_pem);
    ASSERT_EQ(nullptr, cfg.cert_pem);
    http_server_resp_free(&resp);
    ASSERT_EQ("", esp_log_wrapper_get_logs());
    ASSERT_EQ(0, this->m_alloc_free_call_count);
}

TEST_F(TestHttpCheckPostStat, test_auth_basic_ok) // NOLINT
{
    const http_check_params_t params = {
        .p_url               = "https://myserver.com/stat",
        .auth_type           = GW_CFG_HTTP_AUTH_TYPE_BASIC,
        .p_user              = "user1",
        .p_pass              = "pass1",
        .use_ssl_client_cert = false,
        .use_ssl_server_cert = false,
    };
    this->m_mock_http_wait_resp_code = HTTP_RESP_CODE_200;
    this->m_mock_http_wait_resp_body = "{}";
    http_server_resp_t resp          = http_check_post_stat(&params, 10);
    ASSERT_EQ(HTTP_RESP_CODE_200, resp.http_resp_code);
    http_server_resp_free(&resp);
    ASSERT_EQ("", esp_log_wrapper_get_logs());
    ASSERT_EQ(0, this->m_alloc_free_call_count);
}

TEST_F(TestHttpCheckPostStat, test_auth_basic_null_user) // NOLINT
{
    const http_check_params_t params = {
        .p_url               = "https://myserver.com/stat",
        .auth_type           = GW_CFG_HTTP_AUTH_TYPE_BASIC,
        .p_user              = NULL,
        .p_pass              = "pass1",
        .use_ssl_client_cert = false,
        .use_ssl_server_cert = false,
    };
    http_server_resp_t resp = http_check_post_stat(&params, 10);
    ASSERT_EQ(HTTP_RESP_CODE_400, resp.http_resp_code);
    http_server_resp_free(&resp);
    ASSERT_EQ("", esp_log_wrapper_get_logs());
    ASSERT_EQ(0, this->m_alloc_free_call_count);
}

TEST_F(TestHttpCheckPostStat, test_auth_basic_null_pass) // NOLINT
{
    const http_check_params_t params = {
        .p_url               = "https://myserver.com/stat",
        .auth_type           = GW_CFG_HTTP_AUTH_TYPE_BASIC,
        .p_user              = "user1",
        .p_pass              = NULL,
        .use_ssl_client_cert = false,
        .use_ssl_server_cert = false,
    };
    http_server_resp_t resp = http_check_post_stat(&params, 10);
    ASSERT_EQ(HTTP_RESP_CODE_400, resp.http_resp_code);
    http_server_resp_free(&resp);
    ASSERT_EQ("", esp_log_wrapper_get_logs());
    ASSERT_EQ(0, this->m_alloc_free_call_count);
}

TEST_F(TestHttpCheckPostStat, test_auth_basic_user_too_long) // NOLINT
{
    string                    long_user(sizeof(ruuvi_gw_cfg_http_user_t), 'u');
    const http_check_params_t params = {
        .p_url               = "https://myserver.com/stat",
        .auth_type           = GW_CFG_HTTP_AUTH_TYPE_BASIC,
        .p_user              = long_user.c_str(),
        .p_pass              = "pass1",
        .use_ssl_client_cert = false,
        .use_ssl_server_cert = false,
    };
    http_server_resp_t resp = http_check_post_stat(&params, 10);
    ASSERT_EQ(HTTP_RESP_CODE_400, resp.http_resp_code);
    http_server_resp_free(&resp);
    ASSERT_EQ("", esp_log_wrapper_get_logs());
    ASSERT_EQ(0, this->m_alloc_free_call_count);
}

TEST_F(TestHttpCheckPostStat, test_auth_basic_pass_too_long) // NOLINT
{
    string                    long_pass(sizeof(ruuvi_gw_cfg_http_password_t), 'p');
    const http_check_params_t params = {
        .p_url               = "https://myserver.com/stat",
        .auth_type           = GW_CFG_HTTP_AUTH_TYPE_BASIC,
        .p_user              = "user1",
        .p_pass              = long_pass.c_str(),
        .use_ssl_client_cert = false,
        .use_ssl_server_cert = false,
    };
    http_server_resp_t resp = http_check_post_stat(&params, 10);
    ASSERT_EQ(HTTP_RESP_CODE_400, resp.http_resp_code);
    http_server_resp_free(&resp);
    ASSERT_EQ("", esp_log_wrapper_get_logs());
    ASSERT_EQ(0, this->m_alloc_free_call_count);
}

TEST_F(TestHttpCheckPostStat, test_auth_bearer_ok) // NOLINT
{
    const http_check_params_t params = {
        .p_url               = "https://myserver.com/stat",
        .auth_type           = GW_CFG_HTTP_AUTH_TYPE_BEARER,
        .p_user              = NULL,
        .p_pass              = "my_bearer_token",
        .use_ssl_client_cert = false,
        .use_ssl_server_cert = false,
    };
    this->m_mock_http_wait_resp_code = HTTP_RESP_CODE_200;
    this->m_mock_http_wait_resp_body = "{}";
    http_server_resp_t resp          = http_check_post_stat(&params, 10);
    ASSERT_EQ(HTTP_RESP_CODE_200, resp.http_resp_code);
    http_server_resp_free(&resp);
    ASSERT_EQ("", esp_log_wrapper_get_logs());
    ASSERT_EQ(0, this->m_alloc_free_call_count);
}

TEST_F(TestHttpCheckPostStat, test_auth_bearer_null_token) // NOLINT
{
    const http_check_params_t params = {
        .p_url               = "https://myserver.com/stat",
        .auth_type           = GW_CFG_HTTP_AUTH_TYPE_BEARER,
        .p_user              = NULL,
        .p_pass              = NULL,
        .use_ssl_client_cert = false,
        .use_ssl_server_cert = false,
    };
    http_server_resp_t resp = http_check_post_stat(&params, 10);
    ASSERT_EQ(HTTP_RESP_CODE_400, resp.http_resp_code);
    http_server_resp_free(&resp);
    ASSERT_EQ("", esp_log_wrapper_get_logs());
    ASSERT_EQ(0, this->m_alloc_free_call_count);
}

TEST_F(TestHttpCheckPostStat, test_auth_bearer_token_too_long) // NOLINT
{
    string                    long_token(sizeof(ruuvi_gw_cfg_http_password_t), 't');
    const http_check_params_t params = {
        .p_url               = "https://myserver.com/stat",
        .auth_type           = GW_CFG_HTTP_AUTH_TYPE_BEARER,
        .p_user              = NULL,
        .p_pass              = long_token.c_str(),
        .use_ssl_client_cert = false,
        .use_ssl_server_cert = false,
    };
    http_server_resp_t resp = http_check_post_stat(&params, 10);
    ASSERT_EQ(HTTP_RESP_CODE_400, resp.http_resp_code);
    http_server_resp_free(&resp);
    ASSERT_EQ("", esp_log_wrapper_get_logs());
    ASSERT_EQ(0, this->m_alloc_free_call_count);
}

TEST_F(TestHttpCheckPostStat, test_auth_token_ok) // NOLINT
{
    const http_check_params_t params = {
        .p_url               = "https://myserver.com/stat",
        .auth_type           = GW_CFG_HTTP_AUTH_TYPE_TOKEN,
        .p_user              = NULL,
        .p_pass              = "my_token",
        .use_ssl_client_cert = false,
        .use_ssl_server_cert = false,
    };
    this->m_mock_http_wait_resp_code = HTTP_RESP_CODE_200;
    this->m_mock_http_wait_resp_body = "{}";
    http_server_resp_t resp          = http_check_post_stat(&params, 10);
    ASSERT_EQ(HTTP_RESP_CODE_200, resp.http_resp_code);
    http_server_resp_free(&resp);
    ASSERT_EQ("", esp_log_wrapper_get_logs());
    ASSERT_EQ(0, this->m_alloc_free_call_count);
}

TEST_F(TestHttpCheckPostStat, test_auth_token_null) // NOLINT
{
    const http_check_params_t params = {
        .p_url               = "https://myserver.com/stat",
        .auth_type           = GW_CFG_HTTP_AUTH_TYPE_TOKEN,
        .p_user              = NULL,
        .p_pass              = NULL,
        .use_ssl_client_cert = false,
        .use_ssl_server_cert = false,
    };
    http_server_resp_t resp = http_check_post_stat(&params, 10);
    ASSERT_EQ(HTTP_RESP_CODE_400, resp.http_resp_code);
    http_server_resp_free(&resp);
    ASSERT_EQ("", esp_log_wrapper_get_logs());
    ASSERT_EQ(0, this->m_alloc_free_call_count);
}

TEST_F(TestHttpCheckPostStat, test_auth_apikey_ok) // NOLINT
{
    const http_check_params_t params = {
        .p_url               = "https://myserver.com/stat",
        .auth_type           = GW_CFG_HTTP_AUTH_TYPE_APIKEY,
        .p_user              = NULL,
        .p_pass              = "my_api_key",
        .use_ssl_client_cert = false,
        .use_ssl_server_cert = false,
    };
    this->m_mock_http_wait_resp_code = HTTP_RESP_CODE_200;
    this->m_mock_http_wait_resp_body = "{}";
    http_server_resp_t resp          = http_check_post_stat(&params, 10);
    ASSERT_EQ(HTTP_RESP_CODE_200, resp.http_resp_code);
    http_server_resp_free(&resp);
    ASSERT_EQ("", esp_log_wrapper_get_logs());
    ASSERT_EQ(0, this->m_alloc_free_call_count);
}

TEST_F(TestHttpCheckPostStat, test_auth_apikey_null) // NOLINT
{
    const http_check_params_t params = {
        .p_url               = "https://myserver.com/stat",
        .auth_type           = GW_CFG_HTTP_AUTH_TYPE_APIKEY,
        .p_user              = NULL,
        .p_pass              = NULL,
        .use_ssl_client_cert = false,
        .use_ssl_server_cert = false,
    };
    http_server_resp_t resp = http_check_post_stat(&params, 10);
    ASSERT_EQ(HTTP_RESP_CODE_400, resp.http_resp_code);
    http_server_resp_free(&resp);
    ASSERT_EQ("", esp_log_wrapper_get_logs());
    ASSERT_EQ(0, this->m_alloc_free_call_count);
}

TEST_F(TestHttpCheckPostStat, test_http_client_init_fail) // NOLINT
{
    const http_check_params_t params = {
        .p_url               = "https://myserver.com/stat",
        .auth_type           = GW_CFG_HTTP_AUTH_TYPE_NONE,
        .p_user              = NULL,
        .p_pass              = NULL,
        .use_ssl_client_cert = false,
        .use_ssl_server_cert = false,
    };
    this->m_mock_esp_http_client_init_result = nullptr;
    http_server_resp_t resp                  = http_check_post_stat(&params, 10);
    ASSERT_EQ(HTTP_RESP_CODE_500, resp.http_resp_code);
    http_server_resp_free(&resp);
    ASSERT_EQ(
        "E http: HTTP POST to Base URL=https://myserver.com/stat: Can't init http client\n"
        "E http: http_post_stat failed\n",
        esp_log_wrapper_get_logs());
    ASSERT_EQ(0, this->m_alloc_free_call_count);
}

TEST_F(TestHttpCheckPostStat, test_http_send_async_fail) // NOLINT
{
    const http_check_params_t params = {
        .p_url               = "https://myserver.com/stat",
        .auth_type           = GW_CFG_HTTP_AUTH_TYPE_NONE,
        .p_user              = NULL,
        .p_pass              = NULL,
        .use_ssl_client_cert = false,
        .use_ssl_server_cert = false,
    };
    this->m_mock_http_send_async_result = false;
    http_server_resp_t resp             = http_check_post_stat(&params, 10);
    ASSERT_EQ(HTTP_RESP_CODE_500, resp.http_resp_code);
    http_server_resp_free(&resp);
    ASSERT_EQ("E http: http_post_stat failed\n", esp_log_wrapper_get_logs());
    ASSERT_EQ(0, this->m_alloc_free_call_count);
}

TEST_F(TestHttpCheckPostStat, test_ssl_client_cert_ok) // NOLINT
{
    const http_check_params_t params = {
        .p_url               = "https://myserver.com/stat",
        .auth_type           = GW_CFG_HTTP_AUTH_TYPE_NONE,
        .p_user              = NULL,
        .p_pass              = NULL,
        .use_ssl_client_cert = true,
        .use_ssl_server_cert = false,
    };
    this->m_mock_ssl_client_cert     = "-----BEGIN CERTIFICATE-----\nfake_cert\n-----END CERTIFICATE-----";
    this->m_mock_ssl_client_key      = "-----BEGIN PRIVATE KEY-----\nfake_key\n-----END PRIVATE KEY-----";
    this->m_mock_http_wait_resp_code = HTTP_RESP_CODE_200;
    this->m_mock_http_wait_resp_body = "{}";
    http_server_resp_t resp          = http_check_post_stat(&params, 10);
    ASSERT_EQ(HTTP_RESP_CODE_200, resp.http_resp_code);
    ASSERT_STREQ(
        "-----BEGIN CERTIFICATE-----\nfake_cert\n-----END CERTIFICATE-----",
        this->m_freed_client_cert.c_str());
    ASSERT_STREQ("-----BEGIN PRIVATE KEY-----\nfake_key\n-----END PRIVATE KEY-----", this->m_freed_client_key.c_str());
    ASSERT_STREQ("", this->m_freed_server_cert.c_str());
    http_server_resp_free(&resp);
    ASSERT_EQ("", esp_log_wrapper_get_logs());
    ASSERT_EQ(0, this->m_alloc_free_call_count);
}

TEST_F(TestHttpCheckPostStat, test_ssl_server_cert_ok) // NOLINT
{
    const http_check_params_t params = {
        .p_url               = "https://myserver.com/stat",
        .auth_type           = GW_CFG_HTTP_AUTH_TYPE_NONE,
        .p_user              = NULL,
        .p_pass              = NULL,
        .use_ssl_client_cert = false,
        .use_ssl_server_cert = true,
    };
    this->m_mock_ssl_server_cert     = "-----BEGIN CERTIFICATE-----\nfake_server_cert\n-----END CERTIFICATE-----";
    this->m_mock_http_wait_resp_code = HTTP_RESP_CODE_200;
    this->m_mock_http_wait_resp_body = "{}";
    http_server_resp_t resp          = http_check_post_stat(&params, 10);
    ASSERT_EQ(HTTP_RESP_CODE_200, resp.http_resp_code);
    ASSERT_STREQ(
        "-----BEGIN CERTIFICATE-----\nfake_server_cert\n-----END CERTIFICATE-----",
        this->m_freed_server_cert.c_str());
    ASSERT_STREQ("", this->m_freed_client_cert.c_str());
    ASSERT_STREQ("", this->m_freed_client_key.c_str());
    http_server_resp_free(&resp);
    ASSERT_EQ("", esp_log_wrapper_get_logs());
    ASSERT_EQ(0, this->m_alloc_free_call_count);
}

TEST_F(TestHttpCheckPostStat, test_ssl_client_and_server_cert_ok) // NOLINT
{
    const http_check_params_t params = {
        .p_url               = "https://myserver.com/stat",
        .auth_type           = GW_CFG_HTTP_AUTH_TYPE_NONE,
        .p_user              = NULL,
        .p_pass              = NULL,
        .use_ssl_client_cert = true,
        .use_ssl_server_cert = true,
    };
    this->m_mock_ssl_client_cert     = "fake_client_cert";
    this->m_mock_ssl_client_key      = "fake_client_key";
    this->m_mock_ssl_server_cert     = "fake_server_cert";
    this->m_mock_http_wait_resp_code = HTTP_RESP_CODE_200;
    this->m_mock_http_wait_resp_body = "{}";
    http_server_resp_t resp          = http_check_post_stat(&params, 10);
    ASSERT_EQ(HTTP_RESP_CODE_200, resp.http_resp_code);
    ASSERT_STREQ("fake_client_cert", this->m_freed_client_cert.c_str());
    ASSERT_STREQ("fake_client_key", this->m_freed_client_key.c_str());
    ASSERT_STREQ("fake_server_cert", this->m_freed_server_cert.c_str());
    http_server_resp_free(&resp);
    ASSERT_EQ("", esp_log_wrapper_get_logs());
    ASSERT_EQ(0, this->m_alloc_free_call_count);
}

TEST_F(TestHttpCheckPostStat, test_auth_basic_with_ssl_client_cert) // NOLINT
{
    const http_check_params_t params = {
        .p_url               = "https://myserver.com/stat",
        .auth_type           = GW_CFG_HTTP_AUTH_TYPE_BASIC,
        .p_user              = "admin",
        .p_pass              = "secret",
        .use_ssl_client_cert = true,
        .use_ssl_server_cert = false,
    };
    this->m_mock_ssl_client_cert     = "fake_cert";
    this->m_mock_ssl_client_key      = "fake_key";
    this->m_mock_http_wait_resp_code = HTTP_RESP_CODE_200;
    this->m_mock_http_wait_resp_body = "{}";
    http_server_resp_t resp          = http_check_post_stat(&params, 10);
    ASSERT_EQ(HTTP_RESP_CODE_200, resp.http_resp_code);
    ASSERT_STREQ("fake_cert", this->m_freed_client_cert.c_str());
    ASSERT_STREQ("fake_key", this->m_freed_client_key.c_str());
    ASSERT_STREQ("", this->m_freed_server_cert.c_str());
    http_server_resp_free(&resp);
    ASSERT_EQ("", esp_log_wrapper_get_logs());
    ASSERT_EQ(0, this->m_alloc_free_call_count);
}

TEST_F(TestHttpCheckPostStat, test_ssl_client_cert_missing) // NOLINT
{
    // Unlike advs, stat uses http_client_config_init directly which doesn't reject NULL certs.
    // The request proceeds with NULL cert pointers — no resource leak should occur.
    const http_check_params_t params = {
        .p_url               = "https://myserver.com/stat",
        .auth_type           = GW_CFG_HTTP_AUTH_TYPE_NONE,
        .p_user              = NULL,
        .p_pass              = NULL,
        .use_ssl_client_cert = true,
        .use_ssl_server_cert = false,
    };
    this->m_mock_ssl_client_cert     = ""; // cert not available
    this->m_mock_ssl_client_key      = ""; // key not available
    this->m_mock_http_wait_resp_code = HTTP_RESP_CODE_200;
    this->m_mock_http_wait_resp_body = "{}";
    http_server_resp_t resp          = http_check_post_stat(&params, 10);
    ASSERT_EQ(HTTP_RESP_CODE_200, resp.http_resp_code);
    ASSERT_STREQ("", this->m_freed_client_cert.c_str());
    ASSERT_STREQ("", this->m_freed_client_key.c_str());
    ASSERT_STREQ("", this->m_freed_server_cert.c_str());
    http_server_resp_free(&resp);
    ASSERT_EQ("", esp_log_wrapper_get_logs());
    ASSERT_EQ(0, this->m_alloc_free_call_count);
}

TEST_F(TestHttpCheckPostStat, test_ssl_server_cert_missing) // NOLINT
{
    // Server cert requested but not available — stat proceeds without it.
    const http_check_params_t params = {
        .p_url               = "https://myserver.com/stat",
        .auth_type           = GW_CFG_HTTP_AUTH_TYPE_NONE,
        .p_user              = NULL,
        .p_pass              = NULL,
        .use_ssl_client_cert = false,
        .use_ssl_server_cert = true,
    };
    this->m_mock_ssl_server_cert     = ""; // cert not available
    this->m_mock_http_wait_resp_code = HTTP_RESP_CODE_200;
    this->m_mock_http_wait_resp_body = "{}";
    http_server_resp_t resp          = http_check_post_stat(&params, 10);
    ASSERT_EQ(HTTP_RESP_CODE_200, resp.http_resp_code);
    ASSERT_STREQ("", this->m_freed_server_cert.c_str());
    ASSERT_STREQ("", this->m_freed_client_cert.c_str());
    ASSERT_STREQ("", this->m_freed_client_key.c_str());
    http_server_resp_free(&resp);
    ASSERT_EQ("", esp_log_wrapper_get_logs());
    ASSERT_EQ(0, this->m_alloc_free_call_count);
}

TEST_F(TestHttpCheckPostStat, test_ssl_client_cert_present_key_missing) // NOLINT
{
    // Client cert present but key missing — stat proceeds (no validation).
    // Verifies the cert allocation is properly freed even when key is NULL.
    const http_check_params_t params = {
        .p_url               = "https://myserver.com/stat",
        .auth_type           = GW_CFG_HTTP_AUTH_TYPE_NONE,
        .p_user              = NULL,
        .p_pass              = NULL,
        .use_ssl_client_cert = true,
        .use_ssl_server_cert = false,
    };
    this->m_mock_ssl_client_cert     = "fake_cert";
    this->m_mock_ssl_client_key      = ""; // key missing
    this->m_mock_http_wait_resp_code = HTTP_RESP_CODE_200;
    this->m_mock_http_wait_resp_body = "{}";
    http_server_resp_t resp          = http_check_post_stat(&params, 10);
    ASSERT_EQ(HTTP_RESP_CODE_200, resp.http_resp_code);
    ASSERT_STREQ("fake_cert", this->m_freed_client_cert.c_str());
    ASSERT_STREQ("", this->m_freed_client_key.c_str());
    http_server_resp_free(&resp);
    ASSERT_EQ("", esp_log_wrapper_get_logs());
    ASSERT_EQ(0, this->m_alloc_free_call_count);
}

TEST_F(TestHttpCheckPostStat, test_http_client_init_fail_with_ssl_certs) // NOLINT
{
    // When esp_http_client_init fails, http_async_info_free_data must free
    // the cjson_str AND all allocated SSL cert buffers to avoid leaks.
    const http_check_params_t params = {
        .p_url               = "https://myserver.com/stat",
        .auth_type           = GW_CFG_HTTP_AUTH_TYPE_NONE,
        .p_user              = NULL,
        .p_pass              = NULL,
        .use_ssl_client_cert = true,
        .use_ssl_server_cert = true,
    };
    this->m_mock_ssl_client_cert             = "fake_client_cert";
    this->m_mock_ssl_client_key              = "fake_client_key";
    this->m_mock_ssl_server_cert             = "fake_server_cert";
    this->m_mock_esp_http_client_init_result = nullptr; // fail
    http_server_resp_t resp                  = http_check_post_stat(&params, 10);
    ASSERT_EQ(HTTP_RESP_CODE_500, resp.http_resp_code);
    // Verify certs were freed by http_async_info_free_data
    ASSERT_STREQ("fake_client_cert", this->m_freed_client_cert.c_str());
    ASSERT_STREQ("fake_client_key", this->m_freed_client_key.c_str());
    ASSERT_STREQ("fake_server_cert", this->m_freed_server_cert.c_str());
    http_server_resp_free(&resp);
    ASSERT_EQ(
        "E http: HTTP POST to Base URL=https://myserver.com/stat: Can't init http client\n"
        "E http: http_post_stat failed\n",
        esp_log_wrapper_get_logs());
    ASSERT_EQ(0, this->m_alloc_free_call_count);
}

TEST_F(TestHttpCheckPostStat, test_http_send_async_fail_with_ssl_certs) // NOLINT
{
    // When http_send_async fails, cleanup must free the cjson_str AND SSL cert buffers.
    const http_check_params_t params = {
        .p_url               = "https://myserver.com/stat",
        .auth_type           = GW_CFG_HTTP_AUTH_TYPE_NONE,
        .p_user              = NULL,
        .p_pass              = NULL,
        .use_ssl_client_cert = true,
        .use_ssl_server_cert = true,
    };
    this->m_mock_ssl_client_cert        = "fake_client_cert";
    this->m_mock_ssl_client_key         = "fake_client_key";
    this->m_mock_ssl_server_cert        = "fake_server_cert";
    this->m_mock_http_send_async_result = false;
    http_server_resp_t resp             = http_check_post_stat(&params, 10);
    ASSERT_EQ(HTTP_RESP_CODE_500, resp.http_resp_code);
    ASSERT_STREQ("fake_client_cert", this->m_freed_client_cert.c_str());
    ASSERT_STREQ("fake_client_key", this->m_freed_client_key.c_str());
    ASSERT_STREQ("fake_server_cert", this->m_freed_server_cert.c_str());
    http_server_resp_free(&resp);
    ASSERT_EQ("E http: http_post_stat failed\n", esp_log_wrapper_get_logs());
    ASSERT_EQ(0, this->m_alloc_free_call_count);
}

TEST_F(TestHttpCheckPostStat, test_http_json_create_status_str_fail_with_ssl_certs) // NOLINT
{
    // When http_json_create_status_str fails, the function returns false immediately
    // BEFORE allocating SSL certs. Verify no resource leak (the SSL certs are allocated
    // AFTER http_json_create_status_str, so they don't exist at this point).
    const http_check_params_t params = {
        .p_url               = "https://myserver.com/stat",
        .auth_type           = GW_CFG_HTTP_AUTH_TYPE_NONE,
        .p_user              = NULL,
        .p_pass              = NULL,
        .use_ssl_client_cert = true,
        .use_ssl_server_cert = true,
    };
    this->m_mock_ssl_client_cert                    = "fake_client_cert";
    this->m_mock_ssl_client_key                     = "fake_client_key";
    this->m_mock_ssl_server_cert                    = "fake_server_cert";
    this->m_mock_http_json_create_status_str_result = false;
    http_server_resp_t resp                         = http_check_post_stat(&params, 10);
    ASSERT_EQ(HTTP_RESP_CODE_500, resp.http_resp_code);
    http_server_resp_free(&resp);
    ASSERT_EQ(
        "E http: Not enough memory to generate status json\n"
        "E http: http_post_stat failed\n",
        esp_log_wrapper_get_logs());
    ASSERT_EQ(0, this->m_alloc_free_call_count);
}

TEST_F(TestHttpCheckPostStat, test_resp_429_treated_as_200) // NOLINT
{
    const http_check_params_t params = {
        .p_url               = "https://myserver.com/stat",
        .auth_type           = GW_CFG_HTTP_AUTH_TYPE_NONE,
        .p_user              = NULL,
        .p_pass              = NULL,
        .use_ssl_client_cert = false,
        .use_ssl_server_cert = false,
    };
    this->m_mock_http_wait_resp_code = HTTP_RESP_CODE_429;
    this->m_mock_http_wait_resp_body = "{\"error\":\"too many requests\"}";
    http_server_resp_t resp          = http_check_post_stat(&params, 10);
    // http_post_helper converts 429 -> 200
    ASSERT_EQ(HTTP_RESP_CODE_200, resp.http_resp_code);
    http_server_resp_free(&resp);
    ASSERT_EQ("", esp_log_wrapper_get_logs());
    ASSERT_EQ(0, this->m_alloc_free_call_count);
}

TEST_F(TestHttpCheckPostStat, test_resp_500) // NOLINT
{
    const http_check_params_t params = {
        .p_url               = "https://myserver.com/stat",
        .auth_type           = GW_CFG_HTTP_AUTH_TYPE_NONE,
        .p_user              = NULL,
        .p_pass              = NULL,
        .use_ssl_client_cert = false,
        .use_ssl_server_cert = false,
    };
    this->m_mock_http_wait_resp_code = HTTP_RESP_CODE_500;
    this->m_mock_http_wait_resp_body = "Internal Server Error";
    http_server_resp_t resp          = http_check_post_stat(&params, 10);
    ASSERT_EQ(HTTP_RESP_CODE_500, resp.http_resp_code);
    http_server_resp_free(&resp);
    ASSERT_EQ("", esp_log_wrapper_get_logs());
    ASSERT_EQ(0, this->m_alloc_free_call_count);
}

TEST_F(TestHttpCheckPostStat, test_malloc_fail_for_cfg_http_stat) // NOLINT
{
    const http_check_params_t params = {
        .p_url               = "https://myserver.com/stat",
        .auth_type           = GW_CFG_HTTP_AUTH_TYPE_NONE,
        .p_user              = NULL,
        .p_pass              = NULL,
        .use_ssl_client_cert = false,
        .use_ssl_server_cert = false,
    };
    // First allocation in http_check_post_stat_internal2 is os_malloc for ruuvi_gw_cfg_http_stat_t.
    this->m_malloc_fail_on_cnt = 1;
    http_server_resp_t resp    = http_check_post_stat(&params, 10);
    ASSERT_EQ(HTTP_RESP_CODE_500, resp.http_resp_code);
    http_server_resp_free(&resp);
    ASSERT_EQ("E http: Can't allocate memory for ruuvi_gw_cfg_http_t\n", esp_log_wrapper_get_logs());
    ASSERT_EQ(0, this->m_alloc_free_call_count);
}

TEST_F(TestHttpCheckPostStat, test_adv_post_statistics_info_generate_fail) // NOLINT
{
    const http_check_params_t params = {
        .p_url               = "https://myserver.com/stat",
        .auth_type           = GW_CFG_HTTP_AUTH_TYPE_NONE,
        .p_user              = NULL,
        .p_pass              = NULL,
        .use_ssl_client_cert = false,
        .use_ssl_server_cert = false,
    };
    this->m_mock_adv_post_statistics_info_generate_result = false;
    http_server_resp_t resp                               = http_check_post_stat(&params, 10);
    ASSERT_EQ(HTTP_RESP_CODE_500, resp.http_resp_code);
    http_server_resp_free(&resp);
    ASSERT_EQ("E http: Can't allocate memory\n", esp_log_wrapper_get_logs());
    ASSERT_EQ(0, this->m_alloc_free_call_count);
}

TEST_F(TestHttpCheckPostStat, test_http_json_create_status_str_fail) // NOLINT
{
    const http_check_params_t params = {
        .p_url               = "https://myserver.com/stat",
        .auth_type           = GW_CFG_HTTP_AUTH_TYPE_NONE,
        .p_user              = NULL,
        .p_pass              = NULL,
        .use_ssl_client_cert = false,
        .use_ssl_server_cert = false,
    };
    this->m_mock_http_json_create_status_str_result = false;
    http_server_resp_t resp                         = http_check_post_stat(&params, 10);
    ASSERT_EQ(HTTP_RESP_CODE_500, resp.http_resp_code);
    http_server_resp_free(&resp);
    ASSERT_EQ(
        "E http: Not enough memory to generate status json\n"
        "E http: http_post_stat failed\n",
        esp_log_wrapper_get_logs());
    ASSERT_EQ(0, this->m_alloc_free_call_count);
}

TEST_F(TestHttpCheckPostStat, test_http_client_config_init_url_parse_fail) // NOLINT
{
    const http_check_params_t params = {
        .p_url               = "https://myserver.com/stat",
        .auth_type           = GW_CFG_HTTP_AUTH_TYPE_NONE,
        .p_user              = NULL,
        .p_pass              = NULL,
        .use_ssl_client_cert = false,
        .use_ssl_server_cert = false,
    };
    // Make esp_http_client_config_set_from_url fail, which causes http_client_config_init to return false
    this->m_mock_esp_http_client_config_set_from_url_result = false;
    http_server_resp_t resp                                 = http_check_post_stat(&params, 10);
    ASSERT_EQ(HTTP_RESP_CODE_500, resp.http_resp_code);
    http_server_resp_free(&resp);
    ASSERT_EQ(
        "E http: esp_http_client_config_set_from_url failed for Base URL: https://myserver.com/stat\n"
        "E http: http_post_stat failed\n",
        esp_log_wrapper_get_logs());
    ASSERT_EQ(0, this->m_alloc_free_call_count);
}

TEST_F(TestHttpCheckPostStat, test_http_client_config_init_url_parse_fail_with_ssl_certs) // NOLINT
{
    const http_check_params_t params = {
        .p_url               = "https://myserver.com/stat",
        .auth_type           = GW_CFG_HTTP_AUTH_TYPE_NONE,
        .p_user              = NULL,
        .p_pass              = NULL,
        .use_ssl_client_cert = true,
        .use_ssl_server_cert = true,
    };
    this->m_mock_ssl_client_cert = "fake_client_cert";
    this->m_mock_ssl_client_key  = "fake_client_key";
    this->m_mock_ssl_server_cert = "fake_server_cert";
    // Make esp_http_client_config_set_from_url fail inside http_client_config_init
    this->m_mock_esp_http_client_config_set_from_url_result = false;
    http_server_resp_t resp                                 = http_check_post_stat(&params, 10);
    ASSERT_EQ(HTTP_RESP_CODE_500, resp.http_resp_code);
    http_server_resp_free(&resp);
    ASSERT_EQ(
        "E http: esp_http_client_config_set_from_url failed for Base URL: https://myserver.com/stat\n"
        "E http: http_post_stat failed\n",
        esp_log_wrapper_get_logs());
    ASSERT_EQ(0, this->m_alloc_free_call_count);
}
