/**
 * @file test_http_check_post_advs.cpp
 * @author TheSomeMan
 * @date 2026-05-23
 * @copyright Ruuvi Innovations Ltd, license BSD-3-Clause.
 */

#include "http.h"
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

class TestHttpCheckPostAdvs;
static TestHttpCheckPostAdvs* g_pTestClass;

extern "C" {

} // extern "C"

/*** Test class *********************************************************************************/

class TestHttpCheckPostAdvs : public ::testing::Test
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
        this->m_flag_gateway_restart_low_memory       = false;

        this->m_mock_async_resp_set = false;
        memset(&this->m_mock_async_resp, 0, sizeof(this->m_mock_async_resp));

        esp_log_wrapper_clear();
        this->m_malloc_cnt = 0;

        this->m_alloc_free_call_count       = 0;
        this->m_flag_alloc_counting_enabled = true;

        this->m_mock_gw_status_relaying_via_http    = false;
        this->m_mock_esp_http_client_handle         = reinterpret_cast<esp_http_client_handle_t>(0x12345678);
        this->m_mock_esp_http_client_init_result    = this->m_mock_esp_http_client_handle;
        this->m_mock_http_send_async_result         = true;
        this->m_mock_http_wait_resp_code            = HTTP_RESP_CODE_200;
        this->m_mock_http_wait_resp_body            = "{}";
        this->m_mock_ssl_client_cert                = "";
        this->m_mock_ssl_client_key                 = "";
        this->m_mock_ssl_server_cert                = "";
        this->m_mock_http_client_config_init_result = true;
        this->m_mock_http_handle_add_auth_result    = true;
        this->m_mock_json_stream_gen                = reinterpret_cast<json_stream_gen_t*>(0xDEADBEEF);

        this->m_mock_http_handle_add_auth_called = false;
        this->m_captured_auth_type               = GW_CFG_HTTP_AUTH_TYPE_NONE;
        memset(&this->m_captured_auth, 0, sizeof(this->m_captured_auth));

        this->m_mock_http_client_config_init_called = false;
        this->m_captured_server_cert.clear();
        this->m_captured_client_cert.clear();
        this->m_captured_client_key.clear();
        this->m_captured_extra_http_path.clear();
        this->m_captured_extra_http_query.clear();
        this->m_captured_extra_http_headers.clear();
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
    TestHttpCheckPostAdvs();
    ~TestHttpCheckPostAdvs() override;

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
    bool                     m_flag_gateway_restart_low_memory;
    bool                     m_mock_http_client_config_init_result;
    bool                     m_mock_http_handle_add_auth_result;
    json_stream_gen_t*       m_mock_json_stream_gen;

    bool                     m_mock_http_handle_add_auth_called;
    gw_cfg_http_auth_type_e  m_captured_auth_type;
    ruuvi_gw_cfg_http_auth_t m_captured_auth;

    bool   m_mock_http_client_config_init_called;
    string m_captured_server_cert;
    string m_captured_client_cert;
    string m_captured_client_key;
    string m_captured_extra_http_path;
    string m_captured_extra_http_query;
    string m_captured_extra_http_headers;
};

TestHttpCheckPostAdvs::TestHttpCheckPostAdvs()
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
    , m_flag_gateway_restart_low_memory(false)
    , m_mock_http_client_config_init_result(true)
    , m_mock_http_handle_add_auth_result(true)
    , m_mock_json_stream_gen(nullptr)
    , m_mock_http_handle_add_auth_called(false)
    , m_captured_auth_type(GW_CFG_HTTP_AUTH_TYPE_NONE)
    , m_captured_auth()
    , m_mock_http_client_config_init_called(false)
    , Test()
{
}

TestHttpCheckPostAdvs::~TestHttpCheckPostAdvs() = default;

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

const mac_address_str_t*
gw_cfg_get_nrf52_mac_addr(void)
{
    static const mac_address_str_t mac_addr = { "11:22:33:44:55:66" };
    return &mac_addr;
}

str_buf_t
gw_cfg_get_coordinates_str_buf(void)
{
    return str_buf_printf_with_alloc("%s", "");
}

bool
gw_cfg_get_ntp_use(void)
{
    return true;
}

str_buf_t
gw_cfg_storage_read_file_as_string(const char* const p_file_name)
{
    if (nullptr != g_pTestClass)
    {
        if (0 == strcmp(p_file_name, GW_CFG_STORAGE_SSL_HTTP_CLI_CERT) && !g_pTestClass->m_mock_ssl_client_cert.empty())
        {
            return str_buf_printf_with_alloc("%s", g_pTestClass->m_mock_ssl_client_cert.c_str());
        }
        if (0 == strcmp(p_file_name, GW_CFG_STORAGE_SSL_HTTP_CLI_KEY) && !g_pTestClass->m_mock_ssl_client_key.empty())
        {
            return str_buf_printf_with_alloc("%s", g_pTestClass->m_mock_ssl_client_key.c_str());
        }
        if (0 == strcmp(p_file_name, GW_CFG_STORAGE_SSL_HTTP_SRV_CERT) && !g_pTestClass->m_mock_ssl_server_cert.empty())
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
    if (nullptr != g_pTestClass)
    {
        g_pTestClass->m_mock_http_async_info_free_data_called += 1;
    }
}

void
gateway_restart_low_memory(void)
{
    if (nullptr != g_pTestClass)
    {
        g_pTestClass->m_flag_gateway_restart_low_memory = true;
    }
}

bool
ruuvi_gw_mark_app_valid_cancel_rollback(void)
{
    return true;
}

void
adv_post_set_adv_post_http_action(const bool flag_post_to_ruuvi)
{
}

json_stream_gen_t*
http_json_create_stream_gen_advs(
    const adv_report_table_t* const                        p_reports,
    const http_json_create_stream_gen_advs_params_t* const p_params)
{
    if (nullptr != g_pTestClass)
    {
        return g_pTestClass->m_mock_json_stream_gen;
    }
    return nullptr;
}

bool
http_client_config_init(
    http_client_config_t* const                   p_http_client_config,
    const http_client_config_init_params_t* const p_params,
    void* const                                   p_user_data)
{
    if (nullptr == g_pTestClass)
    {
        return false;
    }
    g_pTestClass->m_mock_http_client_config_init_called = true;
    g_pTestClass->m_captured_server_cert        = (nullptr != p_params->p_server_cert) ? p_params->p_server_cert : "";
    g_pTestClass->m_captured_client_cert        = (nullptr != p_params->p_client_cert) ? p_params->p_client_cert : "";
    g_pTestClass->m_captured_client_key         = (nullptr != p_params->p_client_key) ? p_params->p_client_key : "";
    g_pTestClass->m_captured_extra_http_path    = (nullptr != p_params->p_filename_extra_http_path)
                                                      ? p_params->p_filename_extra_http_path
                                                      : "";
    g_pTestClass->m_captured_extra_http_query   = (nullptr != p_params->p_filename_extra_http_query)
                                                      ? p_params->p_filename_extra_http_query
                                                      : "";
    g_pTestClass->m_captured_extra_http_headers = (nullptr != p_params->p_filename_extra_http_headers)
                                                      ? p_params->p_filename_extra_http_headers
                                                      : "";
    // Store cert pointers in config so http_async_info_free_data can free them later,
    // as the real implementation does.
    p_http_client_config->esp_http_client_config.cert_pem        = p_params->p_server_cert;
    p_http_client_config->esp_http_client_config.client_cert_pem = p_params->p_client_cert;
    p_http_client_config->esp_http_client_config.client_key_pem  = p_params->p_client_key;
    return g_pTestClass->m_mock_http_client_config_init_result;
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

bool
http_handle_add_authorization_if_needed(
    esp_http_client_handle_t              http_handle,
    const gw_cfg_http_auth_type_e         auth_type,
    const ruuvi_gw_cfg_http_auth_t* const p_http_auth)
{
    if (nullptr != g_pTestClass)
    {
        g_pTestClass->m_mock_http_handle_add_auth_called = true;
        g_pTestClass->m_captured_auth_type               = auth_type;
        if (nullptr != p_http_auth)
        {
            g_pTestClass->m_captured_auth = *p_http_auth;
        }
        return g_pTestClass->m_mock_http_handle_add_auth_result;
    }
    return false;
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
hmac_sha256_calc_for_json_gen_http_ruuvi(json_stream_gen_t* const p_gen, hmac_sha256_t* const p_hmac_sha256)
{
    if (NULL != p_hmac_sha256)
    {
        memset(p_hmac_sha256, 0, sizeof(*p_hmac_sha256));
    }
    return true;
}

bool
hmac_sha256_calc_for_http_custom(const char* const p_str, hmac_sha256_t* const p_hmac_sha256)
{
    if (NULL != p_hmac_sha256)
    {
        memset(p_hmac_sha256, 0, sizeof(*p_hmac_sha256));
    }
    return true;
}

bool
hmac_sha256_calc_for_json_gen_http_custom(json_stream_gen_t* const p_gen, hmac_sha256_t* const p_hmac_sha256)
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

} // extern "C"

/*** Tests **************************************************************************************/

TEST_F(TestHttpCheckPostAdvs, test_url_null)
{
    const http_check_params_t params = {
        .p_url                  = NULL,
        .auth_type              = GW_CFG_HTTP_AUTH_TYPE_NONE,
        .p_user                 = NULL,
        .p_pass                 = NULL,
        .use_ssl_client_cert    = false,
        .use_ssl_server_cert    = false,
        .use_extra_http_path    = false,
        .use_extra_http_query   = false,
        .use_extra_http_headers = false,
    };
    http_server_resp_t resp = http_check_post_advs(&params, 10);
    ASSERT_EQ(HTTP_RESP_CODE_400, resp.http_resp_code);
    http_server_resp_free(&resp);
    esp_log_wrapper_clear();
    ASSERT_EQ(0, this->m_alloc_free_call_count);
}

TEST_F(TestHttpCheckPostAdvs, test_url_too_long)
{
    // Create a URL that exceeds GW_CFG_MAX_HTTP_URL_LEN
    string                    long_url(sizeof(ruuvi_gw_cfg_http_url_t), 'x');
    const http_check_params_t params = {
        .p_url                  = long_url.c_str(),
        .auth_type              = GW_CFG_HTTP_AUTH_TYPE_NONE,
        .p_user                 = NULL,
        .p_pass                 = NULL,
        .use_ssl_client_cert    = false,
        .use_ssl_server_cert    = false,
        .use_extra_http_path    = false,
        .use_extra_http_query   = false,
        .use_extra_http_headers = false,
    };
    http_server_resp_t resp = http_check_post_advs(&params, 10);
    ASSERT_EQ(HTTP_RESP_CODE_400, resp.http_resp_code);
    http_server_resp_free(&resp);
    esp_log_wrapper_clear();
    ASSERT_EQ(0, this->m_alloc_free_call_count);
}

TEST_F(TestHttpCheckPostAdvs, test_auth_none_ok)
{
    const http_check_params_t params = {
        .p_url                  = "https://myserver.com/api",
        .auth_type              = GW_CFG_HTTP_AUTH_TYPE_NONE,
        .p_user                 = NULL,
        .p_pass                 = NULL,
        .use_ssl_client_cert    = false,
        .use_ssl_server_cert    = false,
        .use_extra_http_path    = false,
        .use_extra_http_query   = false,
        .use_extra_http_headers = false,
    };
    this->m_mock_http_wait_resp_code = HTTP_RESP_CODE_200;
    this->m_mock_http_wait_resp_body = "{\"status\":200}";
    http_server_resp_t resp          = http_check_post_advs(&params, 10);
    ASSERT_EQ(HTTP_RESP_CODE_200, resp.http_resp_code);
    ASSERT_TRUE(this->m_mock_http_handle_add_auth_called);
    ASSERT_EQ(GW_CFG_HTTP_AUTH_TYPE_NONE, this->m_captured_auth_type);
    ASSERT_TRUE(this->m_mock_http_client_config_init_called);
    ASSERT_STREQ("", this->m_captured_client_cert.c_str());
    ASSERT_STREQ("", this->m_captured_client_key.c_str());
    ASSERT_STREQ("", this->m_captured_server_cert.c_str());
    ASSERT_STREQ("", this->m_captured_extra_http_path.c_str());
    ASSERT_STREQ("", this->m_captured_extra_http_query.c_str());
    ASSERT_STREQ("", this->m_captured_extra_http_headers.c_str());
    http_server_resp_free(&resp);
    esp_log_wrapper_clear();
    ASSERT_EQ(0, this->m_alloc_free_call_count);
}

TEST_F(TestHttpCheckPostAdvs, test_auth_basic_ok)
{
    const http_check_params_t params = {
        .p_url                  = "https://myserver.com/api",
        .auth_type              = GW_CFG_HTTP_AUTH_TYPE_BASIC,
        .p_user                 = "user1",
        .p_pass                 = "pass1",
        .use_ssl_client_cert    = false,
        .use_ssl_server_cert    = false,
        .use_extra_http_path    = false,
        .use_extra_http_query   = false,
        .use_extra_http_headers = false,
    };
    this->m_mock_http_wait_resp_code = HTTP_RESP_CODE_200;
    this->m_mock_http_wait_resp_body = "{}";
    http_server_resp_t resp          = http_check_post_advs(&params, 10);
    ASSERT_EQ(HTTP_RESP_CODE_200, resp.http_resp_code);
    ASSERT_TRUE(this->m_mock_http_handle_add_auth_called);
    ASSERT_EQ(GW_CFG_HTTP_AUTH_TYPE_BASIC, this->m_captured_auth_type);
    ASSERT_STREQ("user1", this->m_captured_auth.auth_basic.user.buf);
    ASSERT_STREQ("pass1", this->m_captured_auth.auth_basic.password.buf);
    http_server_resp_free(&resp);
    esp_log_wrapper_clear();
    ASSERT_EQ(0, this->m_alloc_free_call_count);
}

TEST_F(TestHttpCheckPostAdvs, test_auth_basic_null_user)
{
    const http_check_params_t params = {
        .p_url                  = "https://myserver.com/api",
        .auth_type              = GW_CFG_HTTP_AUTH_TYPE_BASIC,
        .p_user                 = NULL,
        .p_pass                 = "pass1",
        .use_ssl_client_cert    = false,
        .use_ssl_server_cert    = false,
        .use_extra_http_path    = false,
        .use_extra_http_query   = false,
        .use_extra_http_headers = false,
    };
    http_server_resp_t resp = http_check_post_advs(&params, 10);
    ASSERT_EQ(HTTP_RESP_CODE_400, resp.http_resp_code);
    http_server_resp_free(&resp);
    esp_log_wrapper_clear();
    ASSERT_EQ(0, this->m_alloc_free_call_count);
}

TEST_F(TestHttpCheckPostAdvs, test_auth_basic_null_pass)
{
    const http_check_params_t params = {
        .p_url                  = "https://myserver.com/api",
        .auth_type              = GW_CFG_HTTP_AUTH_TYPE_BASIC,
        .p_user                 = "user1",
        .p_pass                 = NULL,
        .use_ssl_client_cert    = false,
        .use_ssl_server_cert    = false,
        .use_extra_http_path    = false,
        .use_extra_http_query   = false,
        .use_extra_http_headers = false,
    };
    http_server_resp_t resp = http_check_post_advs(&params, 10);
    ASSERT_EQ(HTTP_RESP_CODE_400, resp.http_resp_code);
    http_server_resp_free(&resp);
    esp_log_wrapper_clear();
    ASSERT_EQ(0, this->m_alloc_free_call_count);
}

TEST_F(TestHttpCheckPostAdvs, test_auth_basic_user_too_long)
{
    string                    long_user(sizeof(ruuvi_gw_cfg_http_user_t), 'u');
    const http_check_params_t params = {
        .p_url                  = "https://myserver.com/api",
        .auth_type              = GW_CFG_HTTP_AUTH_TYPE_BASIC,
        .p_user                 = long_user.c_str(),
        .p_pass                 = "pass1",
        .use_ssl_client_cert    = false,
        .use_ssl_server_cert    = false,
        .use_extra_http_path    = false,
        .use_extra_http_query   = false,
        .use_extra_http_headers = false,
    };
    http_server_resp_t resp = http_check_post_advs(&params, 10);
    ASSERT_EQ(HTTP_RESP_CODE_400, resp.http_resp_code);
    http_server_resp_free(&resp);
    esp_log_wrapper_clear();
    ASSERT_EQ(0, this->m_alloc_free_call_count);
}

TEST_F(TestHttpCheckPostAdvs, test_auth_basic_pass_too_long)
{
    string                    long_pass(sizeof(ruuvi_gw_cfg_http_password_t), 'p');
    const http_check_params_t params = {
        .p_url                  = "https://myserver.com/api",
        .auth_type              = GW_CFG_HTTP_AUTH_TYPE_BASIC,
        .p_user                 = "user1",
        .p_pass                 = long_pass.c_str(),
        .use_ssl_client_cert    = false,
        .use_ssl_server_cert    = false,
        .use_extra_http_path    = false,
        .use_extra_http_query   = false,
        .use_extra_http_headers = false,
    };
    http_server_resp_t resp = http_check_post_advs(&params, 10);
    ASSERT_EQ(HTTP_RESP_CODE_400, resp.http_resp_code);
    http_server_resp_free(&resp);
    esp_log_wrapper_clear();
    ASSERT_EQ(0, this->m_alloc_free_call_count);
}

TEST_F(TestHttpCheckPostAdvs, test_auth_bearer_ok)
{
    const http_check_params_t params = {
        .p_url                  = "https://myserver.com/api",
        .auth_type              = GW_CFG_HTTP_AUTH_TYPE_BEARER,
        .p_user                 = NULL,
        .p_pass                 = "my_bearer_token",
        .use_ssl_client_cert    = false,
        .use_ssl_server_cert    = false,
        .use_extra_http_path    = false,
        .use_extra_http_query   = false,
        .use_extra_http_headers = false,
    };
    this->m_mock_http_wait_resp_code = HTTP_RESP_CODE_200;
    this->m_mock_http_wait_resp_body = "{}";
    http_server_resp_t resp          = http_check_post_advs(&params, 10);
    ASSERT_EQ(HTTP_RESP_CODE_200, resp.http_resp_code);
    ASSERT_TRUE(this->m_mock_http_handle_add_auth_called);
    ASSERT_EQ(GW_CFG_HTTP_AUTH_TYPE_BEARER, this->m_captured_auth_type);
    ASSERT_STREQ("my_bearer_token", this->m_captured_auth.auth_bearer.token.buf);
    http_server_resp_free(&resp);
    esp_log_wrapper_clear();
    ASSERT_EQ(0, this->m_alloc_free_call_count);
}

TEST_F(TestHttpCheckPostAdvs, test_auth_bearer_null_token)
{
    const http_check_params_t params = {
        .p_url                  = "https://myserver.com/api",
        .auth_type              = GW_CFG_HTTP_AUTH_TYPE_BEARER,
        .p_user                 = NULL,
        .p_pass                 = NULL,
        .use_ssl_client_cert    = false,
        .use_ssl_server_cert    = false,
        .use_extra_http_path    = false,
        .use_extra_http_query   = false,
        .use_extra_http_headers = false,
    };
    http_server_resp_t resp = http_check_post_advs(&params, 10);
    ASSERT_EQ(HTTP_RESP_CODE_400, resp.http_resp_code);
    http_server_resp_free(&resp);
    esp_log_wrapper_clear();
    ASSERT_EQ(0, this->m_alloc_free_call_count);
}

TEST_F(TestHttpCheckPostAdvs, test_auth_bearer_token_too_long)
{
    string                    long_token(sizeof(ruuvi_gw_cfg_http_bearer_token_t), 't');
    const http_check_params_t params = {
        .p_url                  = "https://myserver.com/api",
        .auth_type              = GW_CFG_HTTP_AUTH_TYPE_BEARER,
        .p_user                 = NULL,
        .p_pass                 = long_token.c_str(),
        .use_ssl_client_cert    = false,
        .use_ssl_server_cert    = false,
        .use_extra_http_path    = false,
        .use_extra_http_query   = false,
        .use_extra_http_headers = false,
    };
    http_server_resp_t resp = http_check_post_advs(&params, 10);
    ASSERT_EQ(HTTP_RESP_CODE_400, resp.http_resp_code);
    http_server_resp_free(&resp);
    esp_log_wrapper_clear();
    ASSERT_EQ(0, this->m_alloc_free_call_count);
}

TEST_F(TestHttpCheckPostAdvs, test_auth_token_ok)
{
    const http_check_params_t params = {
        .p_url                  = "https://myserver.com/api",
        .auth_type              = GW_CFG_HTTP_AUTH_TYPE_TOKEN,
        .p_user                 = NULL,
        .p_pass                 = "my_token",
        .use_ssl_client_cert    = false,
        .use_ssl_server_cert    = false,
        .use_extra_http_path    = false,
        .use_extra_http_query   = false,
        .use_extra_http_headers = false,
    };
    this->m_mock_http_wait_resp_code = HTTP_RESP_CODE_200;
    this->m_mock_http_wait_resp_body = "{}";
    http_server_resp_t resp          = http_check_post_advs(&params, 10);
    ASSERT_EQ(HTTP_RESP_CODE_200, resp.http_resp_code);
    ASSERT_TRUE(this->m_mock_http_handle_add_auth_called);
    ASSERT_EQ(GW_CFG_HTTP_AUTH_TYPE_TOKEN, this->m_captured_auth_type);
    ASSERT_STREQ("my_token", this->m_captured_auth.auth_token.token.buf);
    http_server_resp_free(&resp);
    esp_log_wrapper_clear();
    ASSERT_EQ(0, this->m_alloc_free_call_count);
}

TEST_F(TestHttpCheckPostAdvs, test_auth_token_null)
{
    const http_check_params_t params = {
        .p_url                  = "https://myserver.com/api",
        .auth_type              = GW_CFG_HTTP_AUTH_TYPE_TOKEN,
        .p_user                 = NULL,
        .p_pass                 = NULL,
        .use_ssl_client_cert    = false,
        .use_ssl_server_cert    = false,
        .use_extra_http_path    = false,
        .use_extra_http_query   = false,
        .use_extra_http_headers = false,
    };
    http_server_resp_t resp = http_check_post_advs(&params, 10);
    ASSERT_EQ(HTTP_RESP_CODE_400, resp.http_resp_code);
    http_server_resp_free(&resp);
    esp_log_wrapper_clear();
    ASSERT_EQ(0, this->m_alloc_free_call_count);
}

TEST_F(TestHttpCheckPostAdvs, test_auth_token_too_long)
{
    string                    long_token(sizeof(ruuvi_gw_cfg_http_token_t), 't');
    const http_check_params_t params = {
        .p_url                  = "https://myserver.com/api",
        .auth_type              = GW_CFG_HTTP_AUTH_TYPE_TOKEN,
        .p_user                 = NULL,
        .p_pass                 = long_token.c_str(),
        .use_ssl_client_cert    = false,
        .use_ssl_server_cert    = false,
        .use_extra_http_path    = false,
        .use_extra_http_query   = false,
        .use_extra_http_headers = false,
    };
    http_server_resp_t resp = http_check_post_advs(&params, 10);
    ASSERT_EQ(HTTP_RESP_CODE_400, resp.http_resp_code);
    http_server_resp_free(&resp);
    esp_log_wrapper_clear();
    ASSERT_EQ(0, this->m_alloc_free_call_count);
}

TEST_F(TestHttpCheckPostAdvs, test_auth_apikey_ok)
{
    const http_check_params_t params = {
        .p_url                  = "https://myserver.com/api",
        .auth_type              = GW_CFG_HTTP_AUTH_TYPE_APIKEY,
        .p_user                 = NULL,
        .p_pass                 = "my_api_key",
        .use_ssl_client_cert    = false,
        .use_ssl_server_cert    = false,
        .use_extra_http_path    = false,
        .use_extra_http_query   = false,
        .use_extra_http_headers = false,
    };
    this->m_mock_http_wait_resp_code = HTTP_RESP_CODE_200;
    this->m_mock_http_wait_resp_body = "{}";
    http_server_resp_t resp          = http_check_post_advs(&params, 10);
    ASSERT_EQ(HTTP_RESP_CODE_200, resp.http_resp_code);
    ASSERT_TRUE(this->m_mock_http_handle_add_auth_called);
    ASSERT_EQ(GW_CFG_HTTP_AUTH_TYPE_APIKEY, this->m_captured_auth_type);
    ASSERT_STREQ("my_api_key", this->m_captured_auth.auth_apikey.api_key.buf);
    http_server_resp_free(&resp);
    esp_log_wrapper_clear();
    ASSERT_EQ(0, this->m_alloc_free_call_count);
}

TEST_F(TestHttpCheckPostAdvs, test_auth_apikey_null)
{
    const http_check_params_t params = {
        .p_url                  = "https://myserver.com/api",
        .auth_type              = GW_CFG_HTTP_AUTH_TYPE_APIKEY,
        .p_user                 = NULL,
        .p_pass                 = NULL,
        .use_ssl_client_cert    = false,
        .use_ssl_server_cert    = false,
        .use_extra_http_path    = false,
        .use_extra_http_query   = false,
        .use_extra_http_headers = false,
    };
    http_server_resp_t resp = http_check_post_advs(&params, 10);
    ASSERT_EQ(HTTP_RESP_CODE_400, resp.http_resp_code);
    http_server_resp_free(&resp);
    esp_log_wrapper_clear();
    ASSERT_EQ(0, this->m_alloc_free_call_count);
}

TEST_F(TestHttpCheckPostAdvs, test_auth_apikey_too_long)
{
    string                    long_key(sizeof(ruuvi_gw_cfg_http_apikey_t), 'k');
    const http_check_params_t params = {
        .p_url                  = "https://myserver.com/api",
        .auth_type              = GW_CFG_HTTP_AUTH_TYPE_APIKEY,
        .p_user                 = NULL,
        .p_pass                 = long_key.c_str(),
        .use_ssl_client_cert    = false,
        .use_ssl_server_cert    = false,
        .use_extra_http_path    = false,
        .use_extra_http_query   = false,
        .use_extra_http_headers = false,
    };
    http_server_resp_t resp = http_check_post_advs(&params, 10);
    ASSERT_EQ(HTTP_RESP_CODE_400, resp.http_resp_code);
    http_server_resp_free(&resp);
    esp_log_wrapper_clear();
    ASSERT_EQ(0, this->m_alloc_free_call_count);
}

TEST_F(TestHttpCheckPostAdvs, test_post_to_ruuvi_default_url)
{
    const http_check_params_t params = {
        .p_url                  = RUUVI_GATEWAY_HTTP_DEFAULT_URL,
        .auth_type              = GW_CFG_HTTP_AUTH_TYPE_NONE,
        .p_user                 = NULL,
        .p_pass                 = NULL,
        .use_ssl_client_cert    = false,
        .use_ssl_server_cert    = false,
        .use_extra_http_path    = false,
        .use_extra_http_query   = false,
        .use_extra_http_headers = false,
    };
    this->m_mock_http_wait_resp_code = HTTP_RESP_CODE_200;
    this->m_mock_http_wait_resp_body = "{}";
    http_server_resp_t resp          = http_check_post_advs(&params, 10);
    ASSERT_EQ(HTTP_RESP_CODE_200, resp.http_resp_code);
    ASSERT_FALSE(this->m_mock_http_handle_add_auth_called);
    http_server_resp_free(&resp);
    esp_log_wrapper_clear();
    ASSERT_EQ(0, this->m_alloc_free_call_count);
}

TEST_F(TestHttpCheckPostAdvs, test_http_client_init_fail)
{
    const http_check_params_t params = {
        .p_url                  = "https://myserver.com/api",
        .auth_type              = GW_CFG_HTTP_AUTH_TYPE_NONE,
        .p_user                 = NULL,
        .p_pass                 = NULL,
        .use_ssl_client_cert    = false,
        .use_ssl_server_cert    = false,
        .use_extra_http_path    = false,
        .use_extra_http_query   = false,
        .use_extra_http_headers = false,
    };
    this->m_mock_esp_http_client_init_result = nullptr;
    http_server_resp_t resp                  = http_check_post_advs(&params, 10);
    ASSERT_EQ(HTTP_RESP_CODE_500, resp.http_resp_code);
    http_server_resp_free(&resp);
    esp_log_wrapper_clear();
    ASSERT_EQ(0, this->m_alloc_free_call_count);
}

TEST_F(TestHttpCheckPostAdvs, test_http_send_async_fail)
{
    const http_check_params_t params = {
        .p_url                  = "https://myserver.com/api",
        .auth_type              = GW_CFG_HTTP_AUTH_TYPE_NONE,
        .p_user                 = NULL,
        .p_pass                 = NULL,
        .use_ssl_client_cert    = false,
        .use_ssl_server_cert    = false,
        .use_extra_http_path    = false,
        .use_extra_http_query   = false,
        .use_extra_http_headers = false,
    };
    this->m_mock_http_send_async_result = false;
    http_server_resp_t resp             = http_check_post_advs(&params, 10);
    ASSERT_EQ(HTTP_RESP_CODE_500, resp.http_resp_code);
    http_server_resp_free(&resp);
    esp_log_wrapper_clear();
    ASSERT_EQ(0, this->m_alloc_free_call_count);
}

TEST_F(TestHttpCheckPostAdvs, test_ssl_client_cert_ok)
{
    const http_check_params_t params = {
        .p_url                  = "https://myserver.com/api",
        .auth_type              = GW_CFG_HTTP_AUTH_TYPE_NONE,
        .p_user                 = NULL,
        .p_pass                 = NULL,
        .use_ssl_client_cert    = true,
        .use_ssl_server_cert    = false,
        .use_extra_http_path    = false,
        .use_extra_http_query   = false,
        .use_extra_http_headers = false,
    };
    this->m_mock_ssl_client_cert     = "-----BEGIN CERTIFICATE-----\nfake_cert\n-----END CERTIFICATE-----";
    this->m_mock_ssl_client_key      = "-----BEGIN PRIVATE KEY-----\nfake_key\n-----END PRIVATE KEY-----";
    this->m_mock_http_wait_resp_code = HTTP_RESP_CODE_200;
    this->m_mock_http_wait_resp_body = "{}";
    http_server_resp_t resp          = http_check_post_advs(&params, 10);
    ASSERT_EQ(HTTP_RESP_CODE_200, resp.http_resp_code);
    ASSERT_TRUE(this->m_mock_http_client_config_init_called);
    ASSERT_STREQ(
        "-----BEGIN CERTIFICATE-----\nfake_cert\n-----END CERTIFICATE-----",
        this->m_captured_client_cert.c_str());
    ASSERT_STREQ(
        "-----BEGIN PRIVATE KEY-----\nfake_key\n-----END PRIVATE KEY-----",
        this->m_captured_client_key.c_str());
    ASSERT_STREQ("", this->m_captured_server_cert.c_str());
    http_server_resp_free(&resp);
    esp_log_wrapper_clear();
    ASSERT_EQ(0, this->m_alloc_free_call_count);
}

TEST_F(TestHttpCheckPostAdvs, test_ssl_client_cert_missing)
{
    const http_check_params_t params = {
        .p_url                  = "https://myserver.com/api",
        .auth_type              = GW_CFG_HTTP_AUTH_TYPE_NONE,
        .p_user                 = NULL,
        .p_pass                 = NULL,
        .use_ssl_client_cert    = true,
        .use_ssl_server_cert    = false,
        .use_extra_http_path    = false,
        .use_extra_http_query   = false,
        .use_extra_http_headers = false,
    };
    this->m_mock_ssl_client_cert = "";
    this->m_mock_ssl_client_key  = "";
    http_server_resp_t resp      = http_check_post_advs(&params, 10);
    ASSERT_EQ(HTTP_RESP_CODE_500, resp.http_resp_code);
    http_server_resp_free(&resp);
    esp_log_wrapper_clear();
    ASSERT_EQ(0, this->m_alloc_free_call_count);
}

TEST_F(TestHttpCheckPostAdvs, test_ssl_client_key_missing)
{
    const http_check_params_t params = {
        .p_url                  = "https://myserver.com/api",
        .auth_type              = GW_CFG_HTTP_AUTH_TYPE_NONE,
        .p_user                 = NULL,
        .p_pass                 = NULL,
        .use_ssl_client_cert    = true,
        .use_ssl_server_cert    = false,
        .use_extra_http_path    = false,
        .use_extra_http_query   = false,
        .use_extra_http_headers = false,
    };
    this->m_mock_ssl_client_cert = "fake_cert";
    this->m_mock_ssl_client_key  = ""; // key missing
    http_server_resp_t resp      = http_check_post_advs(&params, 10);
    ASSERT_EQ(HTTP_RESP_CODE_500, resp.http_resp_code);
    http_server_resp_free(&resp);
    esp_log_wrapper_clear();
    ASSERT_EQ(0, this->m_alloc_free_call_count);
}

TEST_F(TestHttpCheckPostAdvs, test_ssl_server_cert_ok)
{
    const http_check_params_t params = {
        .p_url                  = "https://myserver.com/api",
        .auth_type              = GW_CFG_HTTP_AUTH_TYPE_NONE,
        .p_user                 = NULL,
        .p_pass                 = NULL,
        .use_ssl_client_cert    = false,
        .use_ssl_server_cert    = true,
        .use_extra_http_path    = false,
        .use_extra_http_query   = false,
        .use_extra_http_headers = false,
    };
    this->m_mock_ssl_server_cert     = "-----BEGIN CERTIFICATE-----\nfake_server_cert\n-----END CERTIFICATE-----";
    this->m_mock_http_wait_resp_code = HTTP_RESP_CODE_200;
    this->m_mock_http_wait_resp_body = "{}";
    http_server_resp_t resp          = http_check_post_advs(&params, 10);
    ASSERT_EQ(HTTP_RESP_CODE_200, resp.http_resp_code);
    ASSERT_TRUE(this->m_mock_http_client_config_init_called);
    ASSERT_STREQ(
        "-----BEGIN CERTIFICATE-----\nfake_server_cert\n-----END CERTIFICATE-----",
        this->m_captured_server_cert.c_str());
    ASSERT_STREQ("", this->m_captured_client_cert.c_str());
    ASSERT_STREQ("", this->m_captured_client_key.c_str());
    http_server_resp_free(&resp);
    esp_log_wrapper_clear();
    ASSERT_EQ(0, this->m_alloc_free_call_count);
}

TEST_F(TestHttpCheckPostAdvs, test_ssl_server_cert_missing)
{
    const http_check_params_t params = {
        .p_url                  = "https://myserver.com/api",
        .auth_type              = GW_CFG_HTTP_AUTH_TYPE_NONE,
        .p_user                 = NULL,
        .p_pass                 = NULL,
        .use_ssl_client_cert    = false,
        .use_ssl_server_cert    = true,
        .use_extra_http_path    = false,
        .use_extra_http_query   = false,
        .use_extra_http_headers = false,
    };
    this->m_mock_ssl_server_cert = ""; // server cert missing
    http_server_resp_t resp      = http_check_post_advs(&params, 10);
    ASSERT_EQ(HTTP_RESP_CODE_500, resp.http_resp_code);
    http_server_resp_free(&resp);
    esp_log_wrapper_clear();
    ASSERT_EQ(0, this->m_alloc_free_call_count);
}

TEST_F(TestHttpCheckPostAdvs, test_extra_http_flags)
{
    const http_check_params_t params = {
        .p_url                  = "https://myserver.com",
        .auth_type              = GW_CFG_HTTP_AUTH_TYPE_NONE,
        .p_user                 = NULL,
        .p_pass                 = NULL,
        .use_ssl_client_cert    = false,
        .use_ssl_server_cert    = false,
        .use_extra_http_path    = true,
        .use_extra_http_query   = true,
        .use_extra_http_headers = true,
    };
    this->m_mock_http_wait_resp_code = HTTP_RESP_CODE_200;
    this->m_mock_http_wait_resp_body = "{}";
    http_server_resp_t resp          = http_check_post_advs(&params, 10);
    ASSERT_EQ(HTTP_RESP_CODE_200, resp.http_resp_code);
    ASSERT_TRUE(this->m_mock_http_client_config_init_called);
    ASSERT_STREQ(GW_CFG_STORAGE_HTTP_PATH, this->m_captured_extra_http_path.c_str());
    ASSERT_STREQ(GW_CFG_STORAGE_HTTP_QUERY, this->m_captured_extra_http_query.c_str());
    ASSERT_STREQ(GW_CFG_STORAGE_HTTP_HEADERS, this->m_captured_extra_http_headers.c_str());
    http_server_resp_free(&resp);
    esp_log_wrapper_clear();
    ASSERT_EQ(0, this->m_alloc_free_call_count);
}

TEST_F(TestHttpCheckPostAdvs, test_extra_http_path_only)
{
    const http_check_params_t params = {
        .p_url                  = "https://myserver.com",
        .auth_type              = GW_CFG_HTTP_AUTH_TYPE_NONE,
        .p_user                 = NULL,
        .p_pass                 = NULL,
        .use_ssl_client_cert    = false,
        .use_ssl_server_cert    = false,
        .use_extra_http_path    = true,
        .use_extra_http_query   = false,
        .use_extra_http_headers = false,
    };
    this->m_mock_http_wait_resp_code = HTTP_RESP_CODE_200;
    this->m_mock_http_wait_resp_body = "{}";
    http_server_resp_t resp          = http_check_post_advs(&params, 10);
    ASSERT_EQ(HTTP_RESP_CODE_200, resp.http_resp_code);
    ASSERT_TRUE(this->m_mock_http_client_config_init_called);
    ASSERT_STREQ(GW_CFG_STORAGE_HTTP_PATH, this->m_captured_extra_http_path.c_str());
    ASSERT_STREQ("", this->m_captured_extra_http_query.c_str());
    ASSERT_STREQ("", this->m_captured_extra_http_headers.c_str());
    http_server_resp_free(&resp);
    esp_log_wrapper_clear();
    ASSERT_EQ(0, this->m_alloc_free_call_count);
}

TEST_F(TestHttpCheckPostAdvs, test_extra_http_query_only)
{
    const http_check_params_t params = {
        .p_url                  = "https://myserver.com",
        .auth_type              = GW_CFG_HTTP_AUTH_TYPE_NONE,
        .p_user                 = NULL,
        .p_pass                 = NULL,
        .use_ssl_client_cert    = false,
        .use_ssl_server_cert    = false,
        .use_extra_http_path    = false,
        .use_extra_http_query   = true,
        .use_extra_http_headers = false,
    };
    this->m_mock_http_wait_resp_code = HTTP_RESP_CODE_200;
    this->m_mock_http_wait_resp_body = "{}";
    http_server_resp_t resp          = http_check_post_advs(&params, 10);
    ASSERT_EQ(HTTP_RESP_CODE_200, resp.http_resp_code);
    ASSERT_TRUE(this->m_mock_http_client_config_init_called);
    ASSERT_STREQ("", this->m_captured_extra_http_path.c_str());
    ASSERT_STREQ(GW_CFG_STORAGE_HTTP_QUERY, this->m_captured_extra_http_query.c_str());
    ASSERT_STREQ("", this->m_captured_extra_http_headers.c_str());
    http_server_resp_free(&resp);
    esp_log_wrapper_clear();
    ASSERT_EQ(0, this->m_alloc_free_call_count);
}

TEST_F(TestHttpCheckPostAdvs, test_extra_http_headers_only)
{
    const http_check_params_t params = {
        .p_url                  = "https://myserver.com",
        .auth_type              = GW_CFG_HTTP_AUTH_TYPE_NONE,
        .p_user                 = NULL,
        .p_pass                 = NULL,
        .use_ssl_client_cert    = false,
        .use_ssl_server_cert    = false,
        .use_extra_http_path    = false,
        .use_extra_http_query   = false,
        .use_extra_http_headers = true,
    };
    this->m_mock_http_wait_resp_code = HTTP_RESP_CODE_200;
    this->m_mock_http_wait_resp_body = "{}";
    http_server_resp_t resp          = http_check_post_advs(&params, 10);
    ASSERT_EQ(HTTP_RESP_CODE_200, resp.http_resp_code);
    ASSERT_TRUE(this->m_mock_http_client_config_init_called);
    ASSERT_STREQ("", this->m_captured_extra_http_path.c_str());
    ASSERT_STREQ("", this->m_captured_extra_http_query.c_str());
    ASSERT_STREQ(GW_CFG_STORAGE_HTTP_HEADERS, this->m_captured_extra_http_headers.c_str());
    http_server_resp_free(&resp);
    esp_log_wrapper_clear();
    ASSERT_EQ(0, this->m_alloc_free_call_count);
}

TEST_F(TestHttpCheckPostAdvs, test_resp_429_treated_as_200)
{
    const http_check_params_t params = {
        .p_url                  = "https://myserver.com/api",
        .auth_type              = GW_CFG_HTTP_AUTH_TYPE_NONE,
        .p_user                 = NULL,
        .p_pass                 = NULL,
        .use_ssl_client_cert    = false,
        .use_ssl_server_cert    = false,
        .use_extra_http_path    = false,
        .use_extra_http_query   = false,
        .use_extra_http_headers = false,
    };
    this->m_mock_http_wait_resp_code = HTTP_RESP_CODE_429;
    this->m_mock_http_wait_resp_body = "{\"error\":\"too many requests\"}";
    http_server_resp_t resp          = http_check_post_advs(&params, 10);
    // http_post_helper converts 429 -> 200
    ASSERT_EQ(HTTP_RESP_CODE_200, resp.http_resp_code);
    http_server_resp_free(&resp);
    esp_log_wrapper_clear();
    ASSERT_EQ(0, this->m_alloc_free_call_count);
}

TEST_F(TestHttpCheckPostAdvs, test_resp_500)
{
    const http_check_params_t params = {
        .p_url                  = "https://myserver.com/api",
        .auth_type              = GW_CFG_HTTP_AUTH_TYPE_NONE,
        .p_user                 = NULL,
        .p_pass                 = NULL,
        .use_ssl_client_cert    = false,
        .use_ssl_server_cert    = false,
        .use_extra_http_path    = false,
        .use_extra_http_query   = false,
        .use_extra_http_headers = false,
    };
    this->m_mock_http_wait_resp_code = HTTP_RESP_CODE_500;
    this->m_mock_http_wait_resp_body = "Internal Server Error";
    http_server_resp_t resp          = http_check_post_advs(&params, 10);
    ASSERT_EQ(HTTP_RESP_CODE_500, resp.http_resp_code);
    http_server_resp_free(&resp);
    esp_log_wrapper_clear();
    ASSERT_EQ(0, this->m_alloc_free_call_count);
}

TEST_F(TestHttpCheckPostAdvs, test_ssl_client_and_server_cert_ok)
{
    const http_check_params_t params = {
        .p_url                  = "https://myserver.com/api",
        .auth_type              = GW_CFG_HTTP_AUTH_TYPE_NONE,
        .p_user                 = NULL,
        .p_pass                 = NULL,
        .use_ssl_client_cert    = true,
        .use_ssl_server_cert    = true,
        .use_extra_http_path    = false,
        .use_extra_http_query   = false,
        .use_extra_http_headers = false,
    };
    this->m_mock_ssl_client_cert     = "fake_client_cert";
    this->m_mock_ssl_client_key      = "fake_client_key";
    this->m_mock_ssl_server_cert     = "fake_server_cert";
    this->m_mock_http_wait_resp_code = HTTP_RESP_CODE_200;
    this->m_mock_http_wait_resp_body = "{}";
    http_server_resp_t resp          = http_check_post_advs(&params, 10);
    ASSERT_EQ(HTTP_RESP_CODE_200, resp.http_resp_code);
    ASSERT_TRUE(this->m_mock_http_client_config_init_called);
    ASSERT_STREQ("fake_client_cert", this->m_captured_client_cert.c_str());
    ASSERT_STREQ("fake_client_key", this->m_captured_client_key.c_str());
    ASSERT_STREQ("fake_server_cert", this->m_captured_server_cert.c_str());
    http_server_resp_free(&resp);
    esp_log_wrapper_clear();
    ASSERT_EQ(0, this->m_alloc_free_call_count);
}

TEST_F(TestHttpCheckPostAdvs, test_auth_basic_with_ssl_client_cert)
{
    const http_check_params_t params = {
        .p_url                  = "https://myserver.com/api",
        .auth_type              = GW_CFG_HTTP_AUTH_TYPE_BASIC,
        .p_user                 = "admin",
        .p_pass                 = "secret",
        .use_ssl_client_cert    = true,
        .use_ssl_server_cert    = false,
        .use_extra_http_path    = false,
        .use_extra_http_query   = false,
        .use_extra_http_headers = false,
    };
    this->m_mock_ssl_client_cert     = "fake_cert";
    this->m_mock_ssl_client_key      = "fake_key";
    this->m_mock_http_wait_resp_code = HTTP_RESP_CODE_200;
    this->m_mock_http_wait_resp_body = "{}";
    http_server_resp_t resp          = http_check_post_advs(&params, 10);
    ASSERT_EQ(HTTP_RESP_CODE_200, resp.http_resp_code);
    ASSERT_TRUE(this->m_mock_http_handle_add_auth_called);
    ASSERT_EQ(GW_CFG_HTTP_AUTH_TYPE_BASIC, this->m_captured_auth_type);
    ASSERT_STREQ("admin", this->m_captured_auth.auth_basic.user.buf);
    ASSERT_STREQ("secret", this->m_captured_auth.auth_basic.password.buf);
    ASSERT_TRUE(this->m_mock_http_client_config_init_called);
    ASSERT_STREQ("fake_cert", this->m_captured_client_cert.c_str());
    ASSERT_STREQ("fake_key", this->m_captured_client_key.c_str());
    ASSERT_STREQ("", this->m_captured_server_cert.c_str());
    http_server_resp_free(&resp);
    esp_log_wrapper_clear();
    ASSERT_EQ(0, this->m_alloc_free_call_count);
}

TEST_F(TestHttpCheckPostAdvs, test_post_to_ruuvi_url_with_ssl_cert_flags_fails_when_certs_missing)
{
    // When posting to ruuvi default URL with SSL cert flags set but no certs available,
    // the function should fail because http_client_config_init_for_http_target tries
    // to read certs and they are not found
    const http_check_params_t params = {
        .p_url                  = RUUVI_GATEWAY_HTTP_DEFAULT_URL,
        .auth_type              = GW_CFG_HTTP_AUTH_TYPE_NONE,
        .p_user                 = NULL,
        .p_pass                 = NULL,
        .use_ssl_client_cert    = true,
        .use_ssl_server_cert    = true,
        .use_extra_http_path    = true,
        .use_extra_http_query   = true,
        .use_extra_http_headers = true,
    };
    // No SSL certs set -> reads fail -> returns 500
    http_server_resp_t resp = http_check_post_advs(&params, 10);
    ASSERT_EQ(HTTP_RESP_CODE_500, resp.http_resp_code);
    http_server_resp_free(&resp);
    esp_log_wrapper_clear();
    ASSERT_EQ(0, this->m_alloc_free_call_count);
}

TEST_F(TestHttpCheckPostAdvs, test_post_to_ruuvi_url_without_ssl_flags)
{
    // Post to ruuvi default URL without SSL cert flags - should succeed
    const http_check_params_t params = {
        .p_url                  = RUUVI_GATEWAY_HTTP_DEFAULT_URL,
        .auth_type              = GW_CFG_HTTP_AUTH_TYPE_NONE,
        .p_user                 = NULL,
        .p_pass                 = NULL,
        .use_ssl_client_cert    = false,
        .use_ssl_server_cert    = false,
        .use_extra_http_path    = true,
        .use_extra_http_query   = true,
        .use_extra_http_headers = true,
    };
    this->m_mock_http_wait_resp_code = HTTP_RESP_CODE_200;
    this->m_mock_http_wait_resp_body = "{}";
    http_server_resp_t resp          = http_check_post_advs(&params, 10);
    ASSERT_EQ(HTTP_RESP_CODE_200, resp.http_resp_code);
    ASSERT_TRUE(this->m_mock_http_client_config_init_called);
    ASSERT_STREQ(GW_CFG_STORAGE_HTTP_PATH, this->m_captured_extra_http_path.c_str());
    ASSERT_STREQ(GW_CFG_STORAGE_HTTP_QUERY, this->m_captured_extra_http_query.c_str());
    ASSERT_STREQ(GW_CFG_STORAGE_HTTP_HEADERS, this->m_captured_extra_http_headers.c_str());
    http_server_resp_free(&resp);
    esp_log_wrapper_clear();
    ASSERT_EQ(0, this->m_alloc_free_call_count);
}

TEST_F(TestHttpCheckPostAdvs, test_json_stream_gen_fail)
{
    const http_check_params_t params = {
        .p_url                  = "https://myserver.com/api",
        .auth_type              = GW_CFG_HTTP_AUTH_TYPE_NONE,
        .p_user                 = NULL,
        .p_pass                 = NULL,
        .use_ssl_client_cert    = false,
        .use_ssl_server_cert    = false,
        .use_extra_http_path    = false,
        .use_extra_http_query   = false,
        .use_extra_http_headers = false,
    };
    this->m_mock_json_stream_gen = nullptr; // simulate OOM in json_stream_gen
    http_server_resp_t resp      = http_check_post_advs(&params, 10);
    ASSERT_EQ(HTTP_RESP_CODE_500, resp.http_resp_code);
    ASSERT_TRUE(this->m_flag_gateway_restart_low_memory);
    http_server_resp_free(&resp);
    esp_log_wrapper_clear();
    ASSERT_EQ(0, this->m_alloc_free_call_count);
}

TEST_F(TestHttpCheckPostAdvs, test_calloc_fail_for_cfg_http)
{
    // Test that allocation failure for ruuvi_gw_cfg_http_t returns 500
    const http_check_params_t params = {
        .p_url                  = "https://myserver.com/api",
        .auth_type              = GW_CFG_HTTP_AUTH_TYPE_NONE,
        .p_user                 = NULL,
        .p_pass                 = NULL,
        .use_ssl_client_cert    = false,
        .use_ssl_server_cert    = false,
        .use_extra_http_path    = false,
        .use_extra_http_query   = false,
        .use_extra_http_headers = false,
    };
    // First allocation in http_check_post_advs_internal2 is os_calloc for ruuvi_gw_cfg_http_t.
    // We set fail on a very early allocation to force this to fail.
    this->m_malloc_fail_on_cnt = 1;
    http_server_resp_t resp    = http_check_post_advs(&params, 10);
    ASSERT_EQ(HTTP_RESP_CODE_500, resp.http_resp_code);
    http_server_resp_free(&resp);
    esp_log_wrapper_clear();
    ASSERT_EQ(0, this->m_alloc_free_call_count);
}

TEST_F(TestHttpCheckPostAdvs, test_http_handle_add_auth_fail)
{
    const http_check_params_t params = {
        .p_url                  = "https://myserver.com/api",
        .auth_type              = GW_CFG_HTTP_AUTH_TYPE_BASIC,
        .p_user                 = "user1",
        .p_pass                 = "pass1",
        .use_ssl_client_cert    = false,
        .use_ssl_server_cert    = false,
        .use_extra_http_path    = false,
        .use_extra_http_query   = false,
        .use_extra_http_headers = false,
    };
    this->m_mock_http_handle_add_auth_result = false;
    http_server_resp_t resp                  = http_check_post_advs(&params, 10);
    ASSERT_EQ(HTTP_RESP_CODE_500, resp.http_resp_code);
    ASSERT_TRUE(this->m_mock_http_handle_add_auth_called);
    ASSERT_EQ(GW_CFG_HTTP_AUTH_TYPE_BASIC, this->m_captured_auth_type);
    ASSERT_STREQ("user1", this->m_captured_auth.auth_basic.user.buf);
    ASSERT_STREQ("pass1", this->m_captured_auth.auth_basic.password.buf);
    http_server_resp_free(&resp);
    esp_log_wrapper_clear();
    ASSERT_EQ(0, this->m_alloc_free_call_count);
}
