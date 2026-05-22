/**
 * @file test_esp_http_client.cpp
 * @author TheSomeMan
 * @date 2026-04-11
 * @copyright Ruuvi Innovations Ltd, license BSD-3-Clause.
 */

#include "esp_http_client.h"
#include "gtest/gtest.h"
#include <string>
#include <cstdlib>
#include <cstdarg>
#include <cstring>
#include <cstdio>

#include "esp_log_wrapper.hpp"
#include "esp_transport_internal.h"
#include "tcp_transport_stub.h"
#include "TQueue.hpp"
#include "http_event.hpp"
#include "esp_http_client_stream.h"

using namespace std;

#define ESP_HTTP_CLIENT_DEFAULT_HTTP_USER_AGENT "Ruuvi Gateway HTTP Client/1.0"
#define ESP_HTTP_CLIENT_DEFAULT_HTTP_PROTOCOL   "HTTP/1.1"

class TcpWriteRecord
{
public:
    std::vector<char> m_data;
    bool              m_is_buf_null;
    int               m_len;
    int               m_timeout_ms;

public:
    TcpWriteRecord(const char* buffer, int len, int timeout_ms);

    string
    getDataStr() const;
};

TcpWriteRecord::TcpWriteRecord(const char* buffer, int len, int timeout_ms)
{
    if (buffer != nullptr && len > 0)
    {
        this->m_data.assign(buffer, buffer + len);
    }
    this->m_is_buf_null = (buffer == nullptr);
    this->m_len         = len;
    this->m_timeout_ms  = timeout_ms;
}

string
TcpWriteRecord::getDataStr() const
{
    return string(m_data.begin(), m_data.end());
}

class TcpReadRecord
{
public:
    std::vector<char> m_data;
    esp_err_t         m_res;
    size_t            m_offset;

public:
    TcpReadRecord(const string& resp);
};

TcpReadRecord::TcpReadRecord(const string& resp)
{
    this->m_data.assign(resp.begin(), resp.end());
    this->m_res    = ESP_OK;
    this->m_offset = 0;
}

/*** Google-test class implementation
 * *********************************************************************************/

class TestEspHttpClient;
static TestEspHttpClient* g_pTestClass;

class TestEspHttpClient : public ::testing::Test
{
private:
protected:
    void
    SetUp() override
    {
        this->m_alloc_free_call_count = 0;
        this->m_malloc_fail_on_cnt    = 0;
        this->m_malloc_cnt            = 0;
        esp_log_wrapper_init();
        esp_log_wrapper_clear();
        this->m_alloc_free_call_count       = 0;
        this->m_flag_alloc_counting_enabled = true;
        g_pTestClass                        = this;
    }

    void
    TearDown() override
    {
        this->m_flag_alloc_counting_enabled = false;
        this->m_alloc_free_call_count       = 0;
        g_pTestClass                        = nullptr;
        esp_log_wrapper_deinit();
    }

public:
    TestEspHttpClient();

    ~TestEspHttpClient() override;

    bool                                   m_flag_alloc_counting_enabled;
    int                                    m_alloc_free_call_count;
    uint32_t                               m_malloc_cnt;
    uint32_t                               m_malloc_fail_on_cnt;
    std::queue<int>                        m_tcp_connect_ret_code;
    std::queue<int>                        m_tcp_connect_async_ret_code;
    std::queue<int>                        m_tcp_write_ret_code;
    std::queue<TcpWriteRecord>             m_tcp_write_queue;
    std::queue<TcpReadRecord>              m_tcp_read_queue;
    std::queue<std::shared_ptr<HttpEvent>> m_http_event_queue;
    string                                 m_custom_stream_reader_path;
    string                                 m_custom_stream_reader_query;
    string                                 m_custom_stream_reader_header_val;
    string                                 m_custom_stream_reader_headers;

    string
    addHttpReqHeader(
        const esp_http_client_config_t& config,
        const string&                   extra_header1 = "",
        const string&                   content       = "",
        const string&                   extra_header2 = "");

    void
    addHttpRespHeader(
        const HttpStatus_Code httpStatusCode,
        const size_t          resp_content_data_len = 0,
        const string&         extra_header          = "");

    string
    addHttpRespHeaderAndData(
        const HttpStatus_Code httpStatusCode,
        const string&         resp_content_data,
        const string&         extra_header = "");
};

TestEspHttpClient::TestEspHttpClient()
    : m_flag_alloc_counting_enabled(false)
    , m_alloc_free_call_count(0)
    , m_malloc_cnt(0)
    , m_malloc_fail_on_cnt(0)
    , m_tcp_connect_ret_code {}
    , m_tcp_connect_async_ret_code {}
    , m_tcp_write_ret_code {}
    , m_tcp_write_queue {}
    , m_tcp_read_queue {}
    , m_http_event_queue {}
    , m_custom_stream_reader_path {}
    , m_custom_stream_reader_query {}
    , m_custom_stream_reader_header_val {}
    , m_custom_stream_reader_headers {}
    , Test()
{
    std::srand(0);
}

TestEspHttpClient::~TestEspHttpClient() = default;

string
TestEspHttpClient::addHttpReqHeader(
    const esp_http_client_config_t& config,
    const string&                   extra_header1,
    const string&                   content,
    const string&                   extra_header2)
{
    string host;
    string path = "/";
    if (config.url)
    {
        string url(config.url);

        // Remove protocol (http:// or https://)
        size_t protocol_end = url.find("://");
        if (protocol_end != string::npos)
        {
            url = url.substr(protocol_end + 3);
        }

        // Extract host and path
        size_t path_start = url.find('/');
        if (path_start != string::npos)
        {
            host = url.substr(0, path_start);
            path = url.substr(path_start);

            // Remove fragment (part after #) if present
            size_t fragment_pos = path.find('#');
            if (fragment_pos != string::npos)
            {
                path = path.substr(0, fragment_pos);
            }
        }
        else
        {
            host = url;
        }
    }
    else
    {
        if (!config.host)
        {
            throw std::invalid_argument("Host is not set");
        }
        host = string(config.host);
        if (config.path)
        {
            path = string(config.path);
        }
        if (config.query)
        {
            path += "?" + string(config.query);
        }
    }
    const string req_header = "GET " + path + " " + ESP_HTTP_CLIENT_DEFAULT_HTTP_PROTOCOL + "\r\n"
                              + "User-Agent: " + ESP_HTTP_CLIENT_DEFAULT_HTTP_USER_AGENT + "\r\n" + "Host: " + host
                              + "\r\n" + extra_header1 + "Content-Length: " + to_string(content.length()) + "\r\n"
                              + extra_header2 + "\r\n";
    return req_header;
}

void
TestEspHttpClient::addHttpRespHeader(
    const HttpStatus_Code httpStatusCode,
    const size_t          resp_content_data_len,
    const string&         extra_header)
{
    string httpStatusStr = "";
    switch (httpStatusCode)
    {
        case HttpStatus_Ok:
            httpStatusStr = "OK";
            break;
        case HttpStatus_Unauthorized:
            httpStatusStr = "Unauthorized";
            break;
        default:
            assert(0);
            break;
    }
    const auto resp_header = string("HTTP/1.1 ") + to_string(httpStatusCode) + " " + httpStatusStr + "\r\n"
                             + extra_header + "Content-Length: " + std::to_string(resp_content_data_len) + "\r\n"
                             + "\r\n";
    this->m_tcp_read_queue.push(TcpReadRecord(resp_header));
}

string
TestEspHttpClient::addHttpRespHeaderAndData(
    const HttpStatus_Code httpStatusCode,
    const string&         resp_content_data,
    const string&         extra_header)
{
    this->addHttpRespHeader(httpStatusCode, resp_content_data.length(), extra_header);
    this->m_tcp_read_queue.push(TcpReadRecord(resp_content_data));
    return resp_content_data;
}

extern "C" {

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

void*
__wrap_realloc(void* ptr, size_t size)
{
    extern void* __real_realloc(void* _ptr, size_t _size) __THROW __wur;

    if (nullptr == ptr)
    {
        // realloc(NULL, size) acts like malloc
        return __wrap_malloc(size);
    }
    if (0 == size)
    {
        // realloc(ptr, 0) acts like free
        __wrap_free(ptr);
        return nullptr;
    }
    // __real_realloc may internally free the old block (via glibc's free, not counted)
    // and allocate a new block (via glibc's malloc, not counted).
    // Since the old block was counted as +1 (allocated via __wrap_malloc/calloc),
    // we need to account for losing it (-1) and gaining a new one (+1). Net: 0 change.
    // But if ptr was never counted (allocated before counting), this won't affect count.
    // The simplest correct approach: treat as free(old) + malloc(new).
    void* new_ptr = __real_realloc(ptr, size);
    // No counting adjustment needed: the alloc count for the old block
    // was already tracked at initial allocation time, and realloc doesn't change
    // the number of live allocations (still 1 block for this pointer).
    return new_ptr;
}

char*
__wrap_strdup(const char* s)
{
    if (nullptr == s)
    {
        return nullptr;
    }
    const size_t len = strlen(s) + 1;
    char*        dup = static_cast<char*>(__wrap_malloc(len));
    if (nullptr != dup)
    {
        memcpy(dup, s, len);
    }
    return dup;
}

int
__wrap_asprintf(char** strp, const char* fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    const int len = vsnprintf(nullptr, 0, fmt, args);
    va_end(args);
    if (len < 0)
    {
        *strp = nullptr;
        return -1;
    }
    *strp = static_cast<char*>(__wrap_malloc(len + 1));
    if (nullptr == *strp)
    {
        return -1;
    }
    va_start(args, fmt);
    vsnprintf(*strp, len + 1, fmt, args);
    va_end(args);
    return len;
}

int
__wrap_vasprintf(char** strp, const char* fmt, va_list ap)
{
    va_list ap2;
    va_copy(ap2, ap);
    const int len = vsnprintf(nullptr, 0, fmt, ap);
    va_end(ap);
    if (len < 0)
    {
        *strp = nullptr;
        return -1;
    }
    *strp = static_cast<char*>(__wrap_malloc(len + 1));
    if (nullptr == *strp)
    {
        va_end(ap2);
        return -1;
    }
    vsnprintf(*strp, len + 1, fmt, ap2);
    va_end(ap2);
    return len;
}

uint32_t
esp_random(void)
{
    return std::rand();
}

esp_transport_sync_connect_result_e
tcp_connect(esp_transport_handle_t t, const char* host, int port, int timeout_ms)
{
    if (g_pTestClass->m_tcp_connect_ret_code.empty())
    {
        return ESP_TRANSPORT_SYNC_CONNECT_RESULT_ERROR;
    }
    const int ret = g_pTestClass->m_tcp_connect_ret_code.front();
    g_pTestClass->m_tcp_connect_ret_code.pop();
    return static_cast<esp_transport_sync_connect_result_e>(ret);
}

esp_transport_async_connect_result_e
tcp_connect_async(esp_transport_handle_t t, const char* host, int port, int timeout_ms)
{
    if (g_pTestClass->m_tcp_connect_async_ret_code.empty())
    {
        return ESP_TRANSPORT_ASYNC_CONNECT_RESULT_ERROR;
    }
    const int ret = g_pTestClass->m_tcp_connect_async_ret_code.front();
    g_pTestClass->m_tcp_connect_async_ret_code.pop();
    return static_cast<esp_transport_async_connect_result_e>(ret);
}

int
tcp_read(esp_transport_handle_t t, char* buffer, int len, int timeout_ms)
{
    if (g_pTestClass->m_tcp_read_queue.empty())
    {
        return ESP_FAIL;
    }
    TcpReadRecord tcp_read_record = g_pTestClass->m_tcp_read_queue.front();
    if (tcp_read_record.m_res != ESP_OK)
    {
        g_pTestClass->m_tcp_read_queue.pop();
        return tcp_read_record.m_res;
    }
    const size_t rem_len  = tcp_read_record.m_data.size() - tcp_read_record.m_offset;
    const size_t read_len = len < rem_len ? len : rem_len;
    memcpy(buffer, tcp_read_record.m_data.data() + tcp_read_record.m_offset, read_len);
    tcp_read_record.m_offset += read_len;
    if (tcp_read_record.m_offset == tcp_read_record.m_data.size())
    {
        g_pTestClass->m_tcp_read_queue.pop();
    }
    return read_len;
}

int
tcp_write(esp_transport_handle_t t, const char* buffer, int len, int timeout_ms)
{
    if (0 == g_pTestClass->m_tcp_write_ret_code.size())
    {
        g_pTestClass->m_tcp_write_queue.push(TcpWriteRecord(buffer, len, timeout_ms));
        return len;
    }
    const int ret = g_pTestClass->m_tcp_write_ret_code.front();
    g_pTestClass->m_tcp_write_ret_code.pop();
    if (ret > len)
    {
        assert(ret <= len);
    }
    if (ret > 0)
    {
        g_pTestClass->m_tcp_write_queue.push(TcpWriteRecord(buffer, ret, timeout_ms));
    }
    return ret;
}

static esp_err_t
event_handler(esp_http_client_event_t* p_evt)
{
    switch (p_evt->event_id)
    {
        case HTTP_EVENT_ERROR:
            g_pTestClass->m_http_event_queue.push(std::make_shared<HttpEventError>());
            break;

        case HTTP_EVENT_ON_CONNECTED:
            g_pTestClass->m_http_event_queue.push(std::make_shared<HttpEventOnConnected>());
            break;

        case HTTP_EVENT_HEADERS_SENT:
            g_pTestClass->m_http_event_queue.push(std::make_shared<HttpEventHeadersSent>());
            break;

        case HTTP_EVENT_ON_HEADER:
            g_pTestClass->m_http_event_queue.push(
                std::make_shared<HttpEventOnHeader>(p_evt->header_key, p_evt->header_value));
            break;

        case HTTP_EVENT_ON_DATA:
            g_pTestClass->m_http_event_queue.push(std::make_shared<HttpEventOnData>(p_evt->data, p_evt->data_len));
            break;

        case HTTP_EVENT_ON_FINISH:
            g_pTestClass->m_http_event_queue.push(std::make_shared<HttpEventOnFinish>());
            break;

        case HTTP_EVENT_DISCONNECTED:
            g_pTestClass->m_http_event_queue.push(std::make_shared<HttpEventDisconnected>());
            break;

        default:
            assert(0);
            break;
    }
    return ESP_OK;
}

ssize_t
cb_custom_stream_reader(const http_stream_reader_cmd_e cmd, const http_stream_reader_arg_t arg, void* const p_ctx)
{
    http_stream_reader_string_ctx_t* const p_context = static_cast<http_stream_reader_string_ctx_t*>(p_ctx);
    switch (cmd)
    {
        case HTTP_STREAM_READER_CMD_OPEN:
            if (strcmp(static_cast<const char*>(arg.open.p_param), "path") == 0)
            {
                p_context->p_str = g_pTestClass->m_custom_stream_reader_path.c_str();
            }
            else if (strcmp(static_cast<const char*>(arg.open.p_param), "query") == 0)
            {
                p_context->p_str = g_pTestClass->m_custom_stream_reader_query.c_str();
            }
            else if (strcmp(static_cast<const char*>(arg.open.p_param), "header") == 0)
            {
                p_context->p_str = g_pTestClass->m_custom_stream_reader_header_val.c_str();
            }
            else if (strcmp(static_cast<const char*>(arg.open.p_param), "headers") == 0)
            {
                if (g_pTestClass->m_custom_stream_reader_headers.length() < 2)
                {
                    assert(g_pTestClass->m_custom_stream_reader_headers.length() >= 2);
                }
                const string last2 = g_pTestClass->m_custom_stream_reader_headers.substr(
                    g_pTestClass->m_custom_stream_reader_headers.size() - 2,
                    2);
                if (last2 != "\r\n")
                {
                    assert(last2 == "\r\n");
                }
                p_context->p_str = g_pTestClass->m_custom_stream_reader_headers.c_str();
            }
            else
            {
                assert(0);
            }
            p_context->data_offset = 0;
            return 0;
        case HTTP_STREAM_READER_CMD_READ:
        {
            const size_t len = strlen(&p_context->p_str[p_context->data_offset]);
            if (len >= arg.read.buf_len)
            {
                memcpy(arg.read.p_buf, &p_context->p_str[p_context->data_offset], arg.read.buf_len - 1);
                p_context->data_offset += (arg.read.buf_len - 1);
                arg.read.p_buf[arg.read.buf_len - 1] = '\0';
                return static_cast<ssize_t>(arg.read.buf_len - 1);
            }
            strcpy(arg.read.p_buf, &p_context->p_str[p_context->data_offset]);
            p_context->data_offset += len;
            return static_cast<ssize_t>(len);
        }
        case HTTP_STREAM_READER_CMD_CLOSE:
            p_context->p_str       = nullptr;
            p_context->data_offset = 0;
            return 0;
    }
    assert(0);
    return -1;
}

} // extern "C"

template<typename Expected>
std::shared_ptr<Expected>
assertHttpEventCast(const std::shared_ptr<HttpEvent>& event)
{
    const esp_http_client_event_id_t expected_id = Expected::EVENT_ID;
    EXPECT_EQ(expected_id, event->getEventId());
    if (expected_id != event->getEventId())
    {
        return nullptr;
    }
    return std::dynamic_pointer_cast<Expected>(event);
}

#define TEST_TCP_WRITE_RECORD(request_header_) \
    do \
    { \
        ASSERT_FALSE(g_pTestClass->m_tcp_write_queue.empty()); \
        const TcpWriteRecord tcp_write_record_ = g_pTestClass->m_tcp_write_queue.front(); \
        g_pTestClass->m_tcp_write_queue.pop(); \
        ASSERT_EQ(tcp_write_record_.m_data.size(), tcp_write_record_.m_len); \
        ASSERT_EQ(tcp_write_record_.getDataStr(), request_header_); \
        ASSERT_EQ(tcp_write_record_.m_len, (request_header_).length()); \
        ASSERT_FALSE(tcp_write_record_.m_is_buf_null); \
        ASSERT_EQ(tcp_write_record_.m_timeout_ms, 15000); \
    } while (0)

#define TEST_HTTP_EVENT_ON_CONNECTED() \
    do \
    { \
        auto event = this->m_http_event_queue.front(); \
        this->m_http_event_queue.pop(); \
        ASSERT_NE(nullptr, assertHttpEventCast<HttpEventOnConnected>(event)); \
    } while (0)

#define TEST_HTTP_EVENT_HEADERS_SENT() \
    do \
    { \
        auto event = this->m_http_event_queue.front(); \
        this->m_http_event_queue.pop(); \
        ASSERT_NE(nullptr, assertHttpEventCast<HttpEventHeadersSent>(event)); \
    } while (0)

#define TEST_HTTP_EVENT_ON_HEADER(key_, value_) \
    do \
    { \
        auto event = this->m_http_event_queue.front(); \
        this->m_http_event_queue.pop(); \
        auto casted = assertHttpEventCast<HttpEventOnHeader>(event); \
        ASSERT_NE(nullptr, casted); \
        ASSERT_EQ(HttpEventOnHeader(key_, value_), *casted); \
    } while (0)

#define TEST_HTTP_EVENT_ON_DATA(ptr_, len_) \
    do \
    { \
        auto event = this->m_http_event_queue.front(); \
        this->m_http_event_queue.pop(); \
        auto casted = assertHttpEventCast<HttpEventOnData>(event); \
        ASSERT_NE(nullptr, casted); \
        ASSERT_EQ(HttpEventOnData(ptr_, len_), *casted); \
    } while (0)

#define TEST_HTTP_EVENT_ON_FINISH() \
    do \
    { \
        auto event = this->m_http_event_queue.front(); \
        this->m_http_event_queue.pop(); \
        ASSERT_NE(nullptr, assertHttpEventCast<HttpEventOnFinish>(event)); \
    } while (0)

/*** Unit-Tests
 * *******************************************************************************************************/

TEST_F(TestEspHttpClient, test_http_get_by_host_and_default_path) // NOLINT
{
    esp_http_client_config_t config = {};
    config.event_handler            = &event_handler;
    config.host                     = "myhost.com";

    esp_http_client_handle_t client = esp_http_client_init(&config);
    ASSERT_NE(nullptr, client);

    this->m_tcp_connect_ret_code.push(ESP_OK);
    const string req_header        = this->addHttpReqHeader(config);
    const string resp_content_data = this->addHttpRespHeaderAndData(HttpStatus_Ok, R"({})");

    ASSERT_EQ(ESP_OK, esp_http_client_perform(client));

    TEST_HTTP_EVENT_ON_CONNECTED();
    TEST_HTTP_EVENT_HEADERS_SENT();
    TEST_HTTP_EVENT_ON_HEADER("Content-Length", to_string(resp_content_data.length()));
    TEST_HTTP_EVENT_ON_DATA(resp_content_data.c_str(), resp_content_data.length());
    TEST_HTTP_EVENT_ON_FINISH();
    ASSERT_TRUE(this->m_http_event_queue.empty());

    ASSERT_EQ(
        "GET / HTTP/1.1\r\n"
        "User-Agent: Ruuvi Gateway HTTP Client/1.0\r\n"
        "Host: myhost.com\r\n"
        "Content-Length: 0\r\n"
        "\r\n",
        req_header);
    TEST_TCP_WRITE_RECORD(req_header);
    ASSERT_TRUE(this->m_tcp_write_queue.empty());

    esp_http_client_cleanup(client);
    this->m_flag_alloc_counting_enabled = false;
    esp_log_wrapper_clear();
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    this->m_flag_alloc_counting_enabled = true;
    ASSERT_EQ(0, this->m_alloc_free_call_count);
}

TEST_F(TestEspHttpClient, test_config_set_from_url_null_cfg) // NOLINT
{
    char url[] = "http://myhost.com/path";
    ASSERT_FALSE(esp_http_client_config_set_from_url(nullptr, url));
}

TEST_F(TestEspHttpClient, test_config_set_from_url_null_url) // NOLINT
{
    esp_http_client_config_t config = {};
    ASSERT_FALSE(esp_http_client_config_set_from_url(&config, nullptr));
}

TEST_F(TestEspHttpClient, test_config_set_from_url_host_already_set) // NOLINT
{
    char                     url[]  = "http://myhost.com/path";
    esp_http_client_config_t config = {};
    config.host                     = "existing_host";
    ASSERT_FALSE(esp_http_client_config_set_from_url(&config, url));
}

TEST_F(TestEspHttpClient, test_config_set_from_url_http) // NOLINT
{
    char                     url[]  = "http://myhost.com/api/v1?key=val";
    esp_http_client_config_t config = {};
    ASSERT_TRUE(esp_http_client_config_set_from_url(&config, url));
    ASSERT_STREQ("myhost.com", config.host);
    ASSERT_EQ(80, config.port);
    ASSERT_EQ(HTTP_TRANSPORT_OVER_TCP, config.transport_type);
    ASSERT_STREQ("api/v1", config.path);
    ASSERT_STREQ("key=val", config.query);
    ASSERT_EQ(nullptr, config.username);
    ASSERT_EQ(nullptr, config.password);
}

TEST_F(TestEspHttpClient, test_config_set_from_url_https) // NOLINT
{
    char                     url[]  = "https://secure.example.com/data";
    esp_http_client_config_t config = {};
    ASSERT_TRUE(esp_http_client_config_set_from_url(&config, url));
    ASSERT_STREQ("secure.example.com", config.host);
    ASSERT_EQ(443, config.port);
    ASSERT_EQ(HTTP_TRANSPORT_OVER_SSL, config.transport_type);
    ASSERT_STREQ("data", config.path);
    ASSERT_EQ(nullptr, config.query);
}

TEST_F(TestEspHttpClient, test_config_set_from_url_with_port) // NOLINT
{
    char                     url[]  = "http://myhost.com:8080/api";
    esp_http_client_config_t config = {};
    ASSERT_TRUE(esp_http_client_config_set_from_url(&config, url));
    ASSERT_STREQ("myhost.com", config.host);
    ASSERT_EQ(8080, config.port);
    ASSERT_STREQ("api", config.path);
}

TEST_F(TestEspHttpClient, test_config_set_from_url_with_userinfo) // NOLINT
{
    char                     url[]  = "http://user:pass@myhost.com/path";
    esp_http_client_config_t config = {};
    ASSERT_TRUE(esp_http_client_config_set_from_url(&config, url));
    ASSERT_STREQ("myhost.com", config.host);
    ASSERT_STREQ("user", config.username);
    ASSERT_STREQ("pass", config.password);
    ASSERT_STREQ("path", config.path);
}

TEST_F(TestEspHttpClient, test_config_set_from_url_with_username_only) // NOLINT
{
    char                     url[]  = "http://user@myhost.com/path";
    esp_http_client_config_t config = {};
    ASSERT_TRUE(esp_http_client_config_set_from_url(&config, url));
    ASSERT_STREQ("myhost.com", config.host);
    ASSERT_STREQ("user", config.username);
    ASSERT_EQ(nullptr, config.password);
}

TEST_F(TestEspHttpClient, test_config_set_from_url_no_path) // NOLINT
{
    char                     url[]  = "http://myhost.com";
    esp_http_client_config_t config = {};
    ASSERT_TRUE(esp_http_client_config_set_from_url(&config, url));
    ASSERT_STREQ("myhost.com", config.host);
    ASSERT_EQ(80, config.port);
    ASSERT_EQ(nullptr, config.path);
    ASSERT_EQ(nullptr, config.query);
}

TEST_F(TestEspHttpClient, test_config_set_from_url_root_path) // NOLINT
{
    char                     url[]  = "http://myhost.com/";
    esp_http_client_config_t config = {};
    ASSERT_TRUE(esp_http_client_config_set_from_url(&config, url));
    ASSERT_STREQ("myhost.com", config.host);
    ASSERT_STREQ("", config.path);
}

TEST_F(TestEspHttpClient, test_config_set_from_url_unsupported_schema) // NOLINT
{
    char                     url[]  = "ftp://myhost.com/path";
    esp_http_client_config_t config = {};
    ASSERT_FALSE(esp_http_client_config_set_from_url(&config, url));
}

TEST_F(TestEspHttpClient, test_config_set_from_url_invalid_url) // NOLINT
{
    char                     url[]  = "not a url at all";
    esp_http_client_config_t config = {};
    ASSERT_FALSE(esp_http_client_config_set_from_url(&config, url));
}

TEST_F(TestEspHttpClient, test_config_set_from_url_hostname_too_long) // NOLINT
{
    // Build a URL with hostname longer than ESP_HTTP_CLIENT_MAX_HOSTNAME_LEN (253)
    string long_host;
    for (int i = 0; i < 4; i++)
    {
        if (i > 0)
            long_host += ".";
        long_host += string(63, 'a');
    }
    // long_host is 63*4 + 3 = 255 chars, which is > 253
    string url_str = "http://" + long_host + "/path";
    // Need mutable buffer
    std::vector<char> url_buf(url_str.begin(), url_str.end());
    url_buf.push_back('\0');

    esp_http_client_config_t config = {};
    ASSERT_FALSE(esp_http_client_config_set_from_url(&config, url_buf.data()));
}

TEST_F(TestEspHttpClient, test_config_set_from_url_then_init_and_perform) // NOLINT
{
    char                     url[]  = "http://myhost.com/api/v1?cmd=test";
    esp_http_client_config_t config = {};
    config.event_handler            = &event_handler;
    ASSERT_TRUE(esp_http_client_config_set_from_url(&config, url));

    esp_http_client_handle_t client = esp_http_client_init(&config);
    ASSERT_NE(nullptr, client);

    this->m_tcp_connect_ret_code.push(ESP_OK);
    const string resp_content_data = this->addHttpRespHeaderAndData(HttpStatus_Ok, R"({})");

    ASSERT_EQ(ESP_OK, esp_http_client_perform(client));

    TEST_HTTP_EVENT_ON_CONNECTED();
    TEST_HTTP_EVENT_HEADERS_SENT();
    TEST_HTTP_EVENT_ON_HEADER("Content-Length", to_string(resp_content_data.length()));
    TEST_HTTP_EVENT_ON_DATA(resp_content_data.c_str(), resp_content_data.length());
    TEST_HTTP_EVENT_ON_FINISH();
    ASSERT_TRUE(this->m_http_event_queue.empty());

    const string req_header
        = "GET /api/v1?cmd=test HTTP/1.1\r\n"
          "User-Agent: Ruuvi Gateway HTTP Client/1.0\r\n"
          "Host: myhost.com\r\n"
          "Content-Length: 0\r\n"
          "\r\n";
    TEST_TCP_WRITE_RECORD(req_header);
    ASSERT_TRUE(this->m_tcp_write_queue.empty());

    esp_http_client_cleanup(client);
    this->m_flag_alloc_counting_enabled = false;
    esp_log_wrapper_clear();
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    this->m_flag_alloc_counting_enabled = true;
    ASSERT_EQ(0, this->m_alloc_free_call_count);
}

TEST_F(TestEspHttpClient, test_config_set_from_url_https_then_init_and_perform) // NOLINT
{
    char                     url[]  = "https://secure.example.com:8443/data/endpoint";
    esp_http_client_config_t config = {};
    config.event_handler            = &event_handler;
    ASSERT_TRUE(esp_http_client_config_set_from_url(&config, url));

    ASSERT_EQ(8443, config.port);
    ASSERT_EQ(HTTP_TRANSPORT_OVER_SSL, config.transport_type);

    esp_http_client_handle_t client = esp_http_client_init(&config);
    ASSERT_NE(nullptr, client);

    this->m_tcp_connect_ret_code.push(ESP_OK);
    const string resp_content_data = this->addHttpRespHeaderAndData(HttpStatus_Ok, R"({})");

    ASSERT_EQ(ESP_OK, esp_http_client_perform(client));

    TEST_HTTP_EVENT_ON_CONNECTED();
    TEST_HTTP_EVENT_HEADERS_SENT();
    TEST_HTTP_EVENT_ON_HEADER("Content-Length", to_string(resp_content_data.length()));
    TEST_HTTP_EVENT_ON_DATA(resp_content_data.c_str(), resp_content_data.length());
    TEST_HTTP_EVENT_ON_FINISH();
    ASSERT_TRUE(this->m_http_event_queue.empty());

    const string req_header
        = "GET /data/endpoint HTTP/1.1\r\n"
          "User-Agent: Ruuvi Gateway HTTP Client/1.0\r\n"
          "Host: secure.example.com:8443\r\n"
          "Content-Length: 0\r\n"
          "\r\n";
    TEST_TCP_WRITE_RECORD(req_header);
    ASSERT_TRUE(this->m_tcp_write_queue.empty());

    esp_http_client_cleanup(client);
    this->m_flag_alloc_counting_enabled = false;
    esp_log_wrapper_clear();
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    this->m_flag_alloc_counting_enabled = true;
    ASSERT_EQ(0, this->m_alloc_free_call_count);
}

TEST_F(TestEspHttpClient, test_http_get_by_host_and_path_without_leading_slash) // NOLINT
{
    esp_http_client_config_t config = {};
    config.event_handler            = &event_handler;
    config.host                     = "myhost.com";
    config.path                     = "api/v2";

    esp_http_client_handle_t client = esp_http_client_init(&config);
    ASSERT_NE(nullptr, client);

    this->m_tcp_connect_ret_code.push(ESP_OK);
    const string resp_content_data = this->addHttpRespHeaderAndData(HttpStatus_Ok, R"({})");

    ASSERT_EQ(ESP_OK, esp_http_client_perform(client));

    TEST_HTTP_EVENT_ON_CONNECTED();
    TEST_HTTP_EVENT_HEADERS_SENT();
    TEST_HTTP_EVENT_ON_HEADER("Content-Length", to_string(resp_content_data.length()));
    TEST_HTTP_EVENT_ON_DATA(resp_content_data.c_str(), resp_content_data.length());
    TEST_HTTP_EVENT_ON_FINISH();
    ASSERT_TRUE(this->m_http_event_queue.empty());

    const string req_header
        = "GET /api/v2 HTTP/1.1\r\n"
          "User-Agent: Ruuvi Gateway HTTP Client/1.0\r\n"
          "Host: myhost.com\r\n"
          "Content-Length: 0\r\n"
          "\r\n";
    TEST_TCP_WRITE_RECORD(req_header);
    ASSERT_TRUE(this->m_tcp_write_queue.empty());

    esp_http_client_cleanup(client);
    this->m_flag_alloc_counting_enabled = false;
    esp_log_wrapper_clear();
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    this->m_flag_alloc_counting_enabled = true;
    ASSERT_EQ(0, this->m_alloc_free_call_count);
}

TEST_F(TestEspHttpClient, test_http_get_by_host_and_default_path_async) // NOLINT
{
    esp_http_client_config_t config = {};
    config.event_handler            = &event_handler;
    config.host                     = "myhost.com";
    config.is_async                 = true;

    esp_http_client_handle_t client = esp_http_client_init(&config);
    ASSERT_NE(nullptr, client);

    this->m_tcp_connect_async_ret_code.push(ESP_TRANSPORT_ASYNC_CONNECT_RESULT_IN_PROGRESS);
    this->m_tcp_connect_async_ret_code.push(ESP_TRANSPORT_ASYNC_CONNECT_RESULT_CONNECTED);
    const string req_header        = this->addHttpReqHeader(config);
    const string resp_content_data = this->addHttpRespHeaderAndData(HttpStatus_Ok, R"({})");

    ASSERT_EQ(ESP_ERR_HTTP_EAGAIN, esp_http_client_perform(client));
    ASSERT_EQ(ESP_OK, esp_http_client_perform(client));

    TEST_HTTP_EVENT_ON_CONNECTED();
    TEST_HTTP_EVENT_HEADERS_SENT();
    TEST_HTTP_EVENT_ON_HEADER("Content-Length", to_string(resp_content_data.length()));
    TEST_HTTP_EVENT_ON_DATA(resp_content_data.c_str(), resp_content_data.length());
    TEST_HTTP_EVENT_ON_FINISH();
    ASSERT_TRUE(this->m_http_event_queue.empty());

    ASSERT_EQ(
        "GET / HTTP/1.1\r\n"
        "User-Agent: Ruuvi Gateway HTTP Client/1.0\r\n"
        "Host: myhost.com\r\n"
        "Content-Length: 0\r\n"
        "\r\n",
        req_header);
    TEST_TCP_WRITE_RECORD(req_header);
    ASSERT_TRUE(this->m_tcp_write_queue.empty());

    esp_http_client_cleanup(client);
    this->m_flag_alloc_counting_enabled = false;
    esp_log_wrapper_clear();
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    this->m_flag_alloc_counting_enabled = true;
    ASSERT_EQ(0, this->m_alloc_free_call_count);
}

TEST_F(TestEspHttpClient, test_http_get_by_host_with_max_hostname_len) // NOLINT
{
    esp_http_client_config_t config = {};
    config.event_handler            = &event_handler;
    static const char hostname[]
        = "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcde."
          "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcde."
          "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcde."
          "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abc";
    static_assert(sizeof(hostname) == 254, "sizeof(hostname) == 254");
    config.host = hostname;

    esp_http_client_handle_t client = esp_http_client_init(&config);
    ASSERT_NE(nullptr, client);

    this->m_tcp_connect_ret_code.push(ESP_OK);
    const string req_header        = this->addHttpReqHeader(config);
    const string resp_content_data = this->addHttpRespHeaderAndData(HttpStatus_Ok, R"({})");

    ASSERT_EQ(ESP_OK, esp_http_client_perform(client));

    TEST_HTTP_EVENT_ON_CONNECTED();
    TEST_HTTP_EVENT_HEADERS_SENT();
    TEST_HTTP_EVENT_ON_HEADER("Content-Length", to_string(resp_content_data.length()));
    TEST_HTTP_EVENT_ON_DATA(resp_content_data.c_str(), resp_content_data.length());
    TEST_HTTP_EVENT_ON_FINISH();
    ASSERT_TRUE(this->m_http_event_queue.empty());

    ASSERT_EQ(
        "GET / HTTP/1.1\r\n"
        "User-Agent: Ruuvi Gateway HTTP Client/1.0\r\n"
        "Host: "
        "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcde."
        "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcde."
        "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcde."
        "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abc"
        "\r\n"
        "Content-Length: 0\r\n"
        "\r\n",
        req_header);
    TEST_TCP_WRITE_RECORD(req_header);
    ASSERT_TRUE(this->m_tcp_write_queue.empty());

    esp_http_client_cleanup(client);
    this->m_flag_alloc_counting_enabled = false;
    esp_log_wrapper_clear();
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    this->m_flag_alloc_counting_enabled = true;
    ASSERT_EQ(0, this->m_alloc_free_call_count);
}

TEST_F(TestEspHttpClient, test_http_get_by_host_with_hostname_len_overflow) // NOLINT
{
    esp_http_client_config_t config = {};
    config.event_handler            = &event_handler;
    static const char hostname[]
        = "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcde."
          "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcde."
          "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcde."
          "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcd";
    static_assert(sizeof(hostname) == 255, "sizeof(hostname) == 255");
    config.host = hostname;

    ASSERT_EQ(nullptr, esp_http_client_init(&config));
}

TEST_F(TestEspHttpClient, test_http_get_by_host_and_path) // NOLINT
{
    esp_http_client_config_t config = {};
    config.event_handler            = &event_handler;
    config.host                     = "myhost.com";
    config.path                     = "/api?cmd1=qwe&cmd2=asd#zzz";

    esp_http_client_handle_t client = esp_http_client_init(&config);
    ASSERT_NE(nullptr, client);

    this->m_tcp_connect_ret_code.push(ESP_OK);
    const string req_header        = this->addHttpReqHeader(config);
    const string resp_content_data = this->addHttpRespHeaderAndData(HttpStatus_Ok, R"({})");

    ASSERT_EQ(ESP_OK, esp_http_client_perform(client));

    TEST_HTTP_EVENT_ON_CONNECTED();
    TEST_HTTP_EVENT_HEADERS_SENT();
    TEST_HTTP_EVENT_ON_HEADER("Content-Length", to_string(resp_content_data.length()));
    TEST_HTTP_EVENT_ON_DATA(resp_content_data.c_str(), resp_content_data.length());
    TEST_HTTP_EVENT_ON_FINISH();
    ASSERT_TRUE(this->m_http_event_queue.empty());

    ASSERT_EQ(
        "GET /api?cmd1=qwe&cmd2=asd#zzz HTTP/1.1\r\n"
        "User-Agent: Ruuvi Gateway HTTP Client/1.0\r\n"
        "Host: myhost.com\r\n"
        "Content-Length: 0\r\n"
        "\r\n",
        req_header);
    TEST_TCP_WRITE_RECORD(req_header);
    ASSERT_TRUE(this->m_tcp_write_queue.empty());

    esp_http_client_cleanup(client);
    this->m_flag_alloc_counting_enabled = false;
    esp_log_wrapper_clear();
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    this->m_flag_alloc_counting_enabled = true;
    ASSERT_EQ(0, this->m_alloc_free_call_count);
}

TEST_F(TestEspHttpClient, test_http_get_by_host_and_path_with_extra_header_fields_from_stream_reader) // NOLINT
{
    this->m_custom_stream_reader_header_val = "Value0";
    this->m_custom_stream_reader_headers    = "X-Header1: Value1\r\nX-Header2: Value2\r\n";

    esp_http_client_config_t config                = {};
    config.event_handler                           = &event_handler;
    config.host                                    = "myhost.com";
    config.path                                    = "/api?cmd1=qwe&cmd2=asd#zzz";
    config.cb_extra_headers_stream_reader          = &cb_custom_stream_reader;
    config.cb_extra_headers_stream_reader_param    = const_cast<void*>(static_cast<const void*>("headers"));
    config.cb_extra_headers_stream_reader_ctx_size = sizeof(http_stream_reader_string_ctx_t);

    esp_http_client_handle_t client = esp_http_client_init(&config);
    ASSERT_NE(nullptr, client);

    http_stream_reader_string_ctx_t context = {};
    http_stream_reader_desc_t       desc    = {
                 .p_cb    = &cb_custom_stream_reader,
                 .p_param = const_cast<void*>(static_cast<const void*>("header")),
                 .p_ctx   = &context,
    };

    esp_http_client_set_header_from_stream(client, "X-Header0", &desc);

    this->m_tcp_connect_ret_code.push(ESP_OK);
    const string req_header
        = "GET /api?cmd1=qwe&cmd2=asd#zzz HTTP/1.1\r\n"
          "User-Agent: Ruuvi Gateway HTTP Client/1.0\r\n"
          "Host: myhost.com\r\n"
          "X-Header0: Value0\r\n"
          "Content-Length: 0\r\n"
          "X-Header1: Value1\r\n"
          "X-Header2: Value2\r\n"
          "\r\n";
    const string resp_content_data = this->addHttpRespHeaderAndData(HttpStatus_Ok, R"({})");

    ASSERT_EQ(ESP_OK, esp_http_client_perform(client));

    TEST_HTTP_EVENT_ON_CONNECTED();
    TEST_HTTP_EVENT_HEADERS_SENT();
    TEST_HTTP_EVENT_ON_HEADER("Content-Length", to_string(resp_content_data.length()));
    TEST_HTTP_EVENT_ON_DATA(resp_content_data.c_str(), resp_content_data.length());
    TEST_HTTP_EVENT_ON_FINISH();
    ASSERT_TRUE(this->m_http_event_queue.empty());

    TEST_TCP_WRITE_RECORD(req_header);
    ASSERT_TRUE(this->m_tcp_write_queue.empty());

    esp_http_client_cleanup(client);
    this->m_flag_alloc_counting_enabled = false;
    esp_log_wrapper_clear();
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    this->m_flag_alloc_counting_enabled = true;
    ASSERT_EQ(0, this->m_alloc_free_call_count);
}

TEST_F(TestEspHttpClient, test_http_get_by_host_and_path_with_custom_path_reader) // NOLINT
{
    this->m_custom_stream_reader_path = "api2?cmd2=QWE&cmd3=ASD#zzz";

    esp_http_client_config_t config       = {};
    config.event_handler                  = &event_handler;
    config.host                           = "myhost.com";
    config.path                           = nullptr;
    config.cb_path_stream_reader          = &cb_custom_stream_reader;
    config.cb_path_stream_reader_param    = const_cast<void*>(static_cast<const void*>("path"));
    config.cb_path_stream_reader_ctx_size = sizeof(http_stream_reader_string_ctx_t);

    esp_http_client_handle_t client = esp_http_client_init(&config);
    ASSERT_NE(nullptr, client);

    this->m_tcp_connect_ret_code.push(ESP_OK);
    const string req_header
        = "GET /api2?cmd2=QWE&cmd3=ASD#zzz HTTP/1.1\r\n"
          "User-Agent: Ruuvi Gateway HTTP Client/1.0\r\n"
          "Host: myhost.com\r\n"
          "Content-Length: 0\r\n"
          "\r\n";

    const string resp_content_data = this->addHttpRespHeaderAndData(HttpStatus_Ok, R"({})");

    ASSERT_EQ(ESP_OK, esp_http_client_perform(client));

    TEST_HTTP_EVENT_ON_CONNECTED();
    TEST_HTTP_EVENT_HEADERS_SENT();
    TEST_HTTP_EVENT_ON_HEADER("Content-Length", to_string(resp_content_data.length()));
    TEST_HTTP_EVENT_ON_DATA(resp_content_data.c_str(), resp_content_data.length());
    TEST_HTTP_EVENT_ON_FINISH();
    ASSERT_TRUE(this->m_http_event_queue.empty());

    TEST_TCP_WRITE_RECORD(req_header);
    ASSERT_TRUE(this->m_tcp_write_queue.empty());

    esp_http_client_cleanup(client);
    this->m_flag_alloc_counting_enabled = false;
    esp_log_wrapper_clear();
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    this->m_flag_alloc_counting_enabled = true;
    ASSERT_EQ(0, this->m_alloc_free_call_count);
}

TEST_F(TestEspHttpClient, test_http_get_by_host_and_path_and_query) // NOLINT
{
    esp_http_client_config_t config = {};
    config.event_handler            = &event_handler;
    config.host                     = "myhost.com";
    config.path                     = "/api";
    config.query                    = "cmd1=qwe&cmd2=asd#zzz";

    esp_http_client_handle_t client = esp_http_client_init(&config);
    ASSERT_NE(nullptr, client);

    this->m_tcp_connect_ret_code.push(ESP_OK);
    const string req_header        = this->addHttpReqHeader(config);
    const string resp_content_data = this->addHttpRespHeaderAndData(HttpStatus_Ok, R"({})");

    ASSERT_EQ(ESP_OK, esp_http_client_perform(client));

    TEST_HTTP_EVENT_ON_CONNECTED();
    TEST_HTTP_EVENT_HEADERS_SENT();
    TEST_HTTP_EVENT_ON_HEADER("Content-Length", to_string(resp_content_data.length()));
    TEST_HTTP_EVENT_ON_DATA(resp_content_data.c_str(), resp_content_data.length());
    TEST_HTTP_EVENT_ON_FINISH();
    ASSERT_TRUE(this->m_http_event_queue.empty());

    ASSERT_EQ(
        "GET /api?cmd1=qwe&cmd2=asd#zzz HTTP/1.1\r\n"
        "User-Agent: Ruuvi Gateway HTTP Client/1.0\r\n"
        "Host: myhost.com\r\n"
        "Content-Length: 0\r\n"
        "\r\n",
        req_header);
    TEST_TCP_WRITE_RECORD(req_header);
    ASSERT_TRUE(this->m_tcp_write_queue.empty());

    esp_http_client_cleanup(client);
    this->m_flag_alloc_counting_enabled = false;
    esp_log_wrapper_clear();
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    this->m_flag_alloc_counting_enabled = true;
    ASSERT_EQ(0, this->m_alloc_free_call_count);
}

TEST_F(TestEspHttpClient, test_http_get_by_host_and_path_and_query_with_custom_readers) // NOLINT
{
    this->m_custom_stream_reader_path  = "api3";
    this->m_custom_stream_reader_query = "cmd3=ZXC&cmd4=fgh";

    esp_http_client_config_t config        = {};
    config.event_handler                   = &event_handler;
    config.host                            = "myhost.com";
    config.path                            = nullptr;
    config.cb_path_stream_reader           = &cb_custom_stream_reader;
    config.cb_path_stream_reader_param     = const_cast<void*>(static_cast<const void*>("path"));
    config.cb_path_stream_reader_ctx_size  = sizeof(http_stream_reader_string_ctx_t);
    config.query                           = nullptr;
    config.cb_query_stream_reader          = &cb_custom_stream_reader;
    config.cb_query_stream_reader_param    = const_cast<void*>(static_cast<const void*>("query"));
    config.cb_query_stream_reader_ctx_size = sizeof(http_stream_reader_string_ctx_t);

    esp_http_client_handle_t client = esp_http_client_init(&config);
    ASSERT_NE(nullptr, client);

    this->m_tcp_connect_ret_code.push(ESP_OK);
    const string req_header
        = "GET /api3?cmd3=ZXC&cmd4=fgh HTTP/1.1\r\n"
          "User-Agent: Ruuvi Gateway HTTP Client/1.0\r\n"
          "Host: myhost.com\r\n"
          "Content-Length: 0\r\n"
          "\r\n";

    const string resp_content_data = this->addHttpRespHeaderAndData(HttpStatus_Ok, R"({})");

    ASSERT_EQ(ESP_OK, esp_http_client_perform(client));

    TEST_HTTP_EVENT_ON_CONNECTED();
    TEST_HTTP_EVENT_HEADERS_SENT();
    TEST_HTTP_EVENT_ON_HEADER("Content-Length", to_string(resp_content_data.length()));
    TEST_HTTP_EVENT_ON_DATA(resp_content_data.c_str(), resp_content_data.length());
    TEST_HTTP_EVENT_ON_FINISH();
    ASSERT_TRUE(this->m_http_event_queue.empty());

    TEST_TCP_WRITE_RECORD(req_header);
    ASSERT_TRUE(this->m_tcp_write_queue.empty());

    esp_http_client_cleanup(client);
    this->m_flag_alloc_counting_enabled = false;
    esp_log_wrapper_clear();
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    this->m_flag_alloc_counting_enabled = true;
    ASSERT_EQ(0, this->m_alloc_free_call_count);
}

TEST_F(TestEspHttpClient, test_http_get_by_url_with_default_path) // NOLINT
{
    esp_http_client_config_t config = {};
    config.event_handler            = &event_handler;
    config.url                      = "http://myhost.com";

    esp_http_client_handle_t client = esp_http_client_init(&config);
    ASSERT_NE(nullptr, client);

    this->m_tcp_connect_ret_code.push(ESP_OK);
    const string req_header        = this->addHttpReqHeader(config);
    const string resp_content_data = this->addHttpRespHeaderAndData(HttpStatus_Ok, R"({})");

    ASSERT_EQ(ESP_OK, esp_http_client_perform(client));

    TEST_HTTP_EVENT_ON_CONNECTED();
    TEST_HTTP_EVENT_HEADERS_SENT();
    TEST_HTTP_EVENT_ON_HEADER("Content-Length", to_string(resp_content_data.length()));
    TEST_HTTP_EVENT_ON_DATA(resp_content_data.c_str(), resp_content_data.length());
    TEST_HTTP_EVENT_ON_FINISH();
    ASSERT_TRUE(this->m_http_event_queue.empty());

    ASSERT_EQ(
        "GET / HTTP/1.1\r\n"
        "User-Agent: Ruuvi Gateway HTTP Client/1.0\r\n"
        "Host: myhost.com\r\n"
        "Content-Length: 0\r\n"
        "\r\n",
        req_header);
    TEST_TCP_WRITE_RECORD(req_header);
    ASSERT_TRUE(this->m_tcp_write_queue.empty());

    esp_http_client_cleanup(client);
    this->m_flag_alloc_counting_enabled = false;
    esp_log_wrapper_clear();
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    this->m_flag_alloc_counting_enabled = true;
    ASSERT_EQ(0, this->m_alloc_free_call_count);
}

TEST_F(TestEspHttpClient, test_http_get_by_url_with_max_hostname_len) // NOLINT
{
    esp_http_client_config_t config = {};
    config.event_handler            = &event_handler;
    static const char hostname[]
        = "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcde."
          "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcde."
          "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcde."
          "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abc";
    static_assert(sizeof(hostname) == 254, "sizeof(hostname) == 254");
    config.host = hostname;

    esp_http_client_handle_t client = esp_http_client_init(&config);
    ASSERT_NE(nullptr, client);

    this->m_tcp_connect_ret_code.push(ESP_OK);
    const string req_header        = this->addHttpReqHeader(config);
    const string resp_content_data = this->addHttpRespHeaderAndData(HttpStatus_Ok, R"({})");

    ASSERT_EQ(ESP_OK, esp_http_client_perform(client));

    TEST_HTTP_EVENT_ON_CONNECTED();
    TEST_HTTP_EVENT_HEADERS_SENT();
    TEST_HTTP_EVENT_ON_HEADER("Content-Length", to_string(resp_content_data.length()));
    TEST_HTTP_EVENT_ON_DATA(resp_content_data.c_str(), resp_content_data.length());
    TEST_HTTP_EVENT_ON_FINISH();
    ASSERT_TRUE(this->m_http_event_queue.empty());

    ASSERT_EQ(
        "GET / HTTP/1.1\r\n"
        "User-Agent: Ruuvi Gateway HTTP Client/1.0\r\n"
        "Host: "
        "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcde."
        "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcde."
        "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcde."
        "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abc"
        "\r\n"
        "Content-Length: 0\r\n"
        "\r\n",
        req_header);
    TEST_TCP_WRITE_RECORD(req_header);
    ASSERT_TRUE(this->m_tcp_write_queue.empty());

    esp_http_client_cleanup(client);
    this->m_flag_alloc_counting_enabled = false;
    esp_log_wrapper_clear();
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    this->m_flag_alloc_counting_enabled = true;
    ASSERT_EQ(0, this->m_alloc_free_call_count);
}

TEST_F(TestEspHttpClient, test_http_get_by_url_with_hostname_len_overflow) // NOLINT
{
    esp_http_client_config_t config = {};
    config.event_handler            = &event_handler;
    static const char hostname[]
        = "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcde."
          "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcde."
          "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcde."
          "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcd";
    static_assert(sizeof(hostname) == 255, "sizeof(hostname) == 255");
    config.host = hostname;

    ASSERT_EQ(nullptr, esp_http_client_init(&config));
}

TEST_F(TestEspHttpClient, test_http_get_by_url_with_path) // NOLINT
{
    esp_http_client_config_t config = {};
    config.event_handler            = &event_handler;
    config.url                      = "http://myhost.com/api?cmd1=qwe&cmd2=asd#zzz";

    esp_http_client_handle_t client = esp_http_client_init(&config);
    ASSERT_NE(nullptr, client);

    this->m_tcp_connect_ret_code.push(ESP_OK);
    const string resp_content_data = this->addHttpRespHeaderAndData(HttpStatus_Ok, R"({})");

    ASSERT_EQ(ESP_OK, esp_http_client_perform(client));

    TEST_HTTP_EVENT_ON_CONNECTED();
    TEST_HTTP_EVENT_HEADERS_SENT();
    TEST_HTTP_EVENT_ON_HEADER("Content-Length", to_string(resp_content_data.length()));
    TEST_HTTP_EVENT_ON_DATA(resp_content_data.c_str(), resp_content_data.length());
    TEST_HTTP_EVENT_ON_FINISH();
    ASSERT_TRUE(this->m_http_event_queue.empty());

    const string req_header
        = "GET /api?cmd1=qwe&cmd2=asd HTTP/1.1\r\n"
          "User-Agent: Ruuvi Gateway HTTP Client/1.0\r\n"
          "Host: myhost.com\r\n"
          "Content-Length: 0\r\n"
          "\r\n";
    TEST_TCP_WRITE_RECORD(req_header);
    ASSERT_TRUE(this->m_tcp_write_queue.empty());

    esp_http_client_cleanup(client);
    this->m_flag_alloc_counting_enabled = false;
    esp_log_wrapper_clear();
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    this->m_flag_alloc_counting_enabled = true;
    ASSERT_EQ(0, this->m_alloc_free_call_count);
}

TEST_F(TestEspHttpClient, test_http_get_by_url_and_headers) // NOLINT
{
    esp_http_client_config_t config                = {};
    config.event_handler                           = &event_handler;
    config.url                                     = "http://myhost.com/api?cmd1=qwe&cmd2=asd#zzz";
    config.cb_extra_headers_stream_reader          = &cb_custom_stream_reader;
    config.cb_extra_headers_stream_reader_param    = const_cast<void*>(static_cast<const void*>("headers"));
    config.cb_extra_headers_stream_reader_ctx_size = sizeof(http_stream_reader_string_ctx_t);
    this->m_custom_stream_reader_headers           = "X-Header1: val123\r\n";

    esp_http_client_handle_t client = esp_http_client_init(&config);
    ASSERT_NE(nullptr, client);

    this->m_tcp_connect_ret_code.push(ESP_OK);
    const string resp_content_data = this->addHttpRespHeaderAndData(HttpStatus_Ok, R"({})");

    ASSERT_EQ(ESP_OK, esp_http_client_perform(client));

    TEST_HTTP_EVENT_ON_CONNECTED();
    TEST_HTTP_EVENT_HEADERS_SENT();
    TEST_HTTP_EVENT_ON_HEADER("Content-Length", to_string(resp_content_data.length()));
    TEST_HTTP_EVENT_ON_DATA(resp_content_data.c_str(), resp_content_data.length());
    TEST_HTTP_EVENT_ON_FINISH();
    ASSERT_TRUE(this->m_http_event_queue.empty());

    const string req_header
        = "GET /api?cmd1=qwe&cmd2=asd HTTP/1.1\r\n"
          "User-Agent: Ruuvi Gateway HTTP Client/1.0\r\n"
          "Host: myhost.com\r\n"
          "Content-Length: 0\r\n"
          "X-Header1: val123\r\n"
          "\r\n";
    TEST_TCP_WRITE_RECORD(req_header);
    ASSERT_TRUE(this->m_tcp_write_queue.empty());

    esp_http_client_cleanup(client);
    this->m_flag_alloc_counting_enabled = false;
    esp_log_wrapper_clear();
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    this->m_flag_alloc_counting_enabled = true;
    ASSERT_EQ(0, this->m_alloc_free_call_count);
}

TEST_F(TestEspHttpClient, test_http_get_with_basic_auth) // NOLINT
{
    esp_http_client_config_t config = {};
    config.event_handler            = &event_handler;
    config.host                     = "myhost.com";
    config.path                     = "/";
    config.username                 = "user";
    config.password                 = "password";
    config.auth_type                = HTTP_AUTH_TYPE_BASIC;

    esp_http_client_handle_t client = esp_http_client_init(&config);
    ASSERT_NE(nullptr, client);

    char* value = nullptr;
    ASSERT_EQ(ESP_OK, esp_http_client_get_username(client, &value));
    ASSERT_NE(nullptr, value);
    ASSERT_EQ(string("user"), string(value));

    value = nullptr;
    ASSERT_EQ(ESP_OK, esp_http_client_get_password(client, &value));
    ASSERT_NE(nullptr, value);
    ASSERT_EQ(string("password"), string(value));

    this->m_tcp_connect_ret_code.push(ESP_OK);

    const string req_header        = this->addHttpReqHeader(config, "Authorization: Basic dXNlcjpwYXNzd29yZA==\r\n");
    const string resp_content_data = this->addHttpRespHeaderAndData(HttpStatus_Ok, R"({})");

    ASSERT_EQ(ESP_OK, esp_http_client_perform(client));

    TEST_HTTP_EVENT_ON_CONNECTED();
    TEST_HTTP_EVENT_HEADERS_SENT();
    TEST_HTTP_EVENT_ON_HEADER("Content-Length", to_string(resp_content_data.length()));
    TEST_HTTP_EVENT_ON_DATA(resp_content_data.c_str(), resp_content_data.length());
    TEST_HTTP_EVENT_ON_FINISH();
    ASSERT_TRUE(this->m_http_event_queue.empty());

    TEST_TCP_WRITE_RECORD(req_header);
    ASSERT_TRUE(this->m_tcp_write_queue.empty());

    esp_http_client_cleanup(client);
    this->m_flag_alloc_counting_enabled = false;
    esp_log_wrapper_clear();
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    this->m_flag_alloc_counting_enabled = true;
    ASSERT_EQ(0, this->m_alloc_free_call_count);
}

TEST_F(TestEspHttpClient, test_http_get_with_digest_auth) // NOLINT
{
    esp_http_client_config_t config = {};
    config.event_handler            = &event_handler;
    config.host                     = "myhost.com";
    config.path                     = "/auth";
    config.username                 = "user";
    config.password                 = "password";
    config.auth_type                = HTTP_AUTH_TYPE_DIGEST;
    char*                    value  = nullptr;
    esp_http_client_handle_t client = esp_http_client_init(&config);
    ASSERT_NE(nullptr, client);

    this->m_tcp_connect_ret_code.push(ESP_OK);

    const string req_header  = this->addHttpReqHeader(config);
    const string req_header2 = this->addHttpReqHeader(
        config,
        "",
        "",
        "Authorization: "
        "Digest username=\"user\", "
        "realm=\"myrealm\", "
        "nonce=\"1234567890\", "
        "uri=\"/auth\", "
        "algorithm=\"MD5\", "
        "response=\"8d037eeaef1be4e17e4f4986f093bada\", "
        "qop=auth, "
        "nc=00000001, "
        "cnonce=\"7222815480849057907\", "
        "opaque=\"abcdefg\""
        "\r\n");

    const string resp_content_data = this->addHttpRespHeaderAndData(
        HttpStatus_Unauthorized,
        R"({})",
        "WWW-Authenticate: "
        R"(Digest realm="myrealm", nonce="1234567890", opaque="abcdefg", algorithm=MD5, qop="auth")"
        "\r\n");

    const string resp_content_data2 = this->addHttpRespHeaderAndData(HttpStatus_Ok, R"({})");

    ASSERT_EQ(ESP_OK, esp_http_client_perform(client));

    TEST_HTTP_EVENT_ON_CONNECTED();
    TEST_HTTP_EVENT_HEADERS_SENT();
    TEST_HTTP_EVENT_ON_HEADER(
        "WWW-Authenticate",
        R"(Digest realm="myrealm", nonce="1234567890", opaque="abcdefg", algorithm=MD5, qop="auth")");
    TEST_HTTP_EVENT_ON_HEADER("Content-Length", to_string(resp_content_data.length()));
    TEST_HTTP_EVENT_ON_DATA(resp_content_data.c_str(), resp_content_data.length());
    TEST_HTTP_EVENT_ON_FINISH();
    TEST_HTTP_EVENT_HEADERS_SENT();
    TEST_HTTP_EVENT_ON_HEADER("Content-Length", to_string(resp_content_data.length()));
    TEST_HTTP_EVENT_ON_DATA(resp_content_data.c_str(), resp_content_data.length());
    TEST_HTTP_EVENT_ON_FINISH();
    ASSERT_TRUE(this->m_http_event_queue.empty());

    TEST_TCP_WRITE_RECORD(req_header);
    TEST_TCP_WRITE_RECORD(req_header2);
    ASSERT_TRUE(this->m_tcp_write_queue.empty());

    esp_http_client_cleanup(client);
    this->m_flag_alloc_counting_enabled = false;
    esp_log_wrapper_clear();
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    this->m_flag_alloc_counting_enabled = true;
    ASSERT_EQ(0, this->m_alloc_free_call_count);
}

TEST_F(TestEspHttpClient, test_http_get_with_long_url_split_req_header) // NOLINT
{
    esp_http_client_config_t config = {};
    config.event_handler            = &event_handler;

    static const char url[]
        = "https://a31415926535897-ats.iot.us-west-2.amazonaws.com:8443"
          "/topics/gateway/environment/data/sensors/v1/room101/temperature?"
          "X-Amz-Algorithm=AWS4-HMAC-SHA256&"
          "X-Amz-Credential=ASIAUTORANDOMID12345%2F20260419%2Fus-west-2%2Fiotdata%2Faws4_request&"
          "X-Amz-Date=20260419T010434Z&"
          "X-Amz-Expires=3600&"
          "X-Amz-SignedHeaders=content-type%3Bhost%3Bx-amz-client-id%3Bx-amz-date%3Bx-amz-meta-gateway-id%3Bx-amz-meta-"
          "location&"
          "X-Amz-Signature=b5e6f8d9a0c1b2c3d4e5f6g7h8i9j0k1l2m3n4o5p6q7r8s9t0u1v2w3x4y5z6a7"
          "#zzz";
    config.url = url;

    esp_http_client_handle_t client = esp_http_client_init(&config);
    ASSERT_NE(nullptr, client);

    this->m_tcp_connect_ret_code.push(ESP_OK);

    const string req_header
        = "GET "
          "/topics/gateway/environment/data/sensors/v1/room101/temperature?"
          "X-Amz-Algorithm=AWS4-HMAC-SHA256&"
          "X-Amz-Credential=ASIAUTORANDOMID12345%2F20260419%2Fus-west-2%2Fiotdata%2Faws4_request&"
          "X-Amz-Date=20260419T010434Z&"
          "X-Amz-Expires=3600&"
          "X-Amz-SignedHeaders=content-type%3Bhost%3Bx-amz-client-id%3Bx-amz-date%3Bx-amz-meta-gateway-id%3Bx-amz-meta-"
          "location&"
          "X-Amz-Signature=b5e6f8d9a0c1b2c3d4e5f6g7h8i9j0k1l2m3n4o5p6q7r8s9t0u1v2w3x4y5z6a7"
          " " ESP_HTTP_CLIENT_DEFAULT_HTTP_PROTOCOL
          "\r\n"
          "User-Agent: " ESP_HTTP_CLIENT_DEFAULT_HTTP_USER_AGENT
          "\r\n"
          "Host: a31415926535897-ats.";
    const string req_header2
        = "iot.us-west-2.amazonaws.com:8443"
          "\r\n"
          "Content-Length: 0"
          "\r\n"
          "\r\n";

    const string resp_content_data = this->addHttpRespHeaderAndData(HttpStatus_Ok, R"({})");

    ASSERT_EQ(ESP_OK, esp_http_client_perform(client));

    TEST_HTTP_EVENT_ON_CONNECTED();
    TEST_HTTP_EVENT_HEADERS_SENT();
    TEST_HTTP_EVENT_ON_HEADER("Content-Length", to_string(resp_content_data.length()));
    TEST_HTTP_EVENT_ON_DATA(resp_content_data.c_str(), resp_content_data.length());
    TEST_HTTP_EVENT_ON_FINISH();
    ASSERT_TRUE(this->m_http_event_queue.empty());

    TEST_TCP_WRITE_RECORD(req_header);
    TEST_TCP_WRITE_RECORD(req_header2);
    ASSERT_TRUE(this->m_tcp_write_queue.empty());

    esp_http_client_cleanup(client);
    this->m_flag_alloc_counting_enabled = false;
    esp_log_wrapper_clear();
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    this->m_flag_alloc_counting_enabled = true;
    ASSERT_EQ(0, this->m_alloc_free_call_count);
}

TEST_F(TestEspHttpClient, test_http_get_with_long_path_split_req_header) // NOLINT
{
    esp_http_client_config_t config = {};
    config.event_handler            = &event_handler;

    config.host = "a31415926535897-ats.iot.us-west-2.amazonaws.com";
    config.path
        = "/topics/gateway/environment/data/sensors/v1/room101/temperature?"
          "X-Amz-Algorithm=AWS4-HMAC-SHA256&"
          "X-Amz-Credential=ASIAUTORANDOMID12345%2F20260419%2Fus-west-2%2Fiotdata%2Faws4_request&"
          "X-Amz-Date=20260419T010434Z&"
          "X-Amz-Expires=3600&"
          "X-Amz-SignedHeaders=content-type%3Bhost%3Bx-amz-client-id%3Bx-amz-date%3Bx-amz-meta-gateway-id%3Bx-amz-meta-"
          "location&"
          "X-Amz-Signature=b5e6f8d9a0c1b2c3d4e5f6g7h8i9j0k1l2m3n4o5p6q7r8s9t0u1v2w3x4y5z6a7";

    esp_http_client_handle_t client = esp_http_client_init(&config);
    ASSERT_NE(nullptr, client);

    this->m_tcp_connect_ret_code.push(ESP_OK);

    const string req_header
        = "GET "
          "/topics/gateway/environment/data/sensors/v1/room101/temperature?"
          "X-Amz-Algorithm=AWS4-HMAC-SHA256&"
          "X-Amz-Credential=ASIAUTORANDOMID12345%2F20260419%2Fus-west-2%2Fiotdata%2Faws4_request&"
          "X-Amz-Date=20260419T010434Z&"
          "X-Amz-Expires=3600&"
          "X-Amz-SignedHeaders=content-type%3Bhost%3Bx-amz-client-id%3Bx-amz-date%3Bx-amz-meta-gateway-id%3Bx-amz-meta-"
          "location&"
          "X-Amz-Signature=b5e6f8d9a0c1b2c3d4e5f6g7h8i9j0k1l2m3n4o5p6q7r8s9t0u1v2w3x4y5z6a7"
          " " ESP_HTTP_CLIENT_DEFAULT_HTTP_PROTOCOL
          "\r\n"
          "User-Agent: " ESP_HTTP_CLIENT_DEFAULT_HTTP_USER_AGENT
          "\r\n"
          "Host: a31415926535897-ats.";
    const string req_header2
        = "iot.us-west-2.amazonaws.com"
          "\r\n"
          "Content-Length: 0"
          "\r\n"
          "\r\n";

    const string resp_content_data = this->addHttpRespHeaderAndData(HttpStatus_Ok, R"({})");

    ASSERT_EQ(ESP_OK, esp_http_client_perform(client));

    TEST_HTTP_EVENT_ON_CONNECTED();
    TEST_HTTP_EVENT_HEADERS_SENT();
    TEST_HTTP_EVENT_ON_HEADER("Content-Length", to_string(resp_content_data.length()));
    TEST_HTTP_EVENT_ON_DATA(resp_content_data.c_str(), resp_content_data.length());
    TEST_HTTP_EVENT_ON_FINISH();
    ASSERT_TRUE(this->m_http_event_queue.empty());

    TEST_TCP_WRITE_RECORD(req_header);
    TEST_TCP_WRITE_RECORD(req_header2);
    ASSERT_TRUE(this->m_tcp_write_queue.empty());

    esp_http_client_cleanup(client);
    this->m_flag_alloc_counting_enabled = false;
    esp_log_wrapper_clear();
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    this->m_flag_alloc_counting_enabled = true;
    ASSERT_EQ(0, this->m_alloc_free_call_count);
}

TEST_F(TestEspHttpClient, test_http_async_get_with_long_path_split_req_header) // NOLINT
{
    esp_http_client_config_t config = {};
    config.event_handler            = &event_handler;
    config.is_async                 = true;

    config.host = "a31415926535897-ats.iot.us-west-2.amazonaws.com";
    config.path
        = "/topics/gateway/environment/data/sensors/v1/room101/temperature?"
          "X-Amz-Algorithm=AWS4-HMAC-SHA256&"
          "X-Amz-Credential=ASIAUTORANDOMID12345%2F20260419%2Fus-west-2%2Fiotdata%2Faws4_request&"
          "X-Amz-Date=20260419T010434Z&"
          "X-Amz-Expires=3600&"
          "X-Amz-SignedHeaders=content-type%3Bhost%3Bx-amz-client-id%3Bx-amz-date%3Bx-amz-meta-gateway-id%3Bx-amz-meta-"
          "location&"
          "X-Amz-Signature=b5e6f8d9a0c1b2c3d4e5f6g7h8i9j0k1l2m3n4o5p6q7r8s9t0u1v2w3x4y5z6a7";

    esp_http_client_handle_t client = esp_http_client_init(&config);
    ASSERT_NE(nullptr, client);

    this->m_tcp_connect_async_ret_code.push(ESP_ERR_HTTP_EAGAIN);
    this->m_tcp_connect_async_ret_code.push(ESP_OK);

    const string req_header
        = "GET "
          "/topics/gateway/environment/data/sensors/v1/room101/temperature?"
          "X-Amz-Algorithm=AWS4-HMAC-SHA256";
    const string req_header2
        = "&"
          "X-Amz-Credential=ASIAUTORANDOMID12345%2F20260419%2Fus-west-2%2Fiotdata%2Faws4_request&"
          "X-Amz-Date=20";
    const string req_header3
        = "260419T010434Z&"
          "X-Amz-Expires=3600&"
          "X-Amz-SignedHeaders=content-type%3Bhost%3Bx-amz-client-id%3Bx-amz-";
    const string req_header4
        = "date%3Bx-amz-meta-gateway-id%3Bx-amz-meta-"
          "location&"
          "X-Amz-Signature=b5e6f8d9a0c1b2c3d4e5f6g7h8i9j0k1l";
    const string req_header5
        = "2m3n4o5p6q7r8s9t0u1v2w3x4y5z6a7"
          " " ESP_HTTP_CLIENT_DEFAULT_HTTP_PROTOCOL
          "\r\n"
          "User-Agent: " ESP_HTTP_CLIENT_DEFAULT_HTTP_USER_AGENT
          "\r\n"
          "Host: a31415926";
    const string req_header6 = "535897-ats.";
    const string req_header7
        = "iot.us-west-2.amazonaws.com"
          "\r\n"
          "Content-Length: 0"
          "\r\n"
          "\r\n";

    const string resp_content_data = this->addHttpRespHeaderAndData(HttpStatus_Ok, R"({})");

    this->m_tcp_write_ret_code.push(0);
    this->m_tcp_write_ret_code.push(100);
    this->m_tcp_write_ret_code.push(0);
    this->m_tcp_write_ret_code.push(100);
    this->m_tcp_write_ret_code.push(0);
    this->m_tcp_write_ret_code.push(100);
    this->m_tcp_write_ret_code.push(0);
    this->m_tcp_write_ret_code.push(100);
    this->m_tcp_write_ret_code.push(0);
    this->m_tcp_write_ret_code.push(0);
    this->m_tcp_write_ret_code.push(100);
    this->m_tcp_write_ret_code.push(11);
    this->m_tcp_write_ret_code.push(50);

    ASSERT_EQ(ESP_ERR_HTTP_EAGAIN, esp_http_client_perform(client));
    ASSERT_EQ(ESP_ERR_HTTP_EAGAIN, esp_http_client_perform(client));
    ASSERT_EQ(ESP_ERR_HTTP_EAGAIN, esp_http_client_perform(client));
    ASSERT_EQ(ESP_ERR_HTTP_EAGAIN, esp_http_client_perform(client));
    ASSERT_EQ(ESP_ERR_HTTP_EAGAIN, esp_http_client_perform(client));
    ASSERT_EQ(ESP_ERR_HTTP_EAGAIN, esp_http_client_perform(client));
    ASSERT_EQ(ESP_ERR_HTTP_EAGAIN, esp_http_client_perform(client));
    ASSERT_EQ(ESP_ERR_HTTP_EAGAIN, esp_http_client_perform(client));
    ASSERT_EQ(ESP_ERR_HTTP_EAGAIN, esp_http_client_perform(client));
    ASSERT_EQ(ESP_ERR_HTTP_EAGAIN, esp_http_client_perform(client));
    ASSERT_EQ(ESP_ERR_HTTP_EAGAIN, esp_http_client_perform(client));
    ASSERT_EQ(ESP_OK, esp_http_client_perform(client));

    ASSERT_EQ(0, this->m_tcp_write_ret_code.size());

    TEST_HTTP_EVENT_ON_CONNECTED();
    TEST_HTTP_EVENT_HEADERS_SENT();
    TEST_HTTP_EVENT_ON_HEADER("Content-Length", to_string(resp_content_data.length()));
    TEST_HTTP_EVENT_ON_DATA(resp_content_data.c_str(), resp_content_data.length());
    TEST_HTTP_EVENT_ON_FINISH();
    ASSERT_TRUE(this->m_http_event_queue.empty());

    TEST_TCP_WRITE_RECORD(req_header);
    TEST_TCP_WRITE_RECORD(req_header2);
    TEST_TCP_WRITE_RECORD(req_header3);
    TEST_TCP_WRITE_RECORD(req_header4);
    TEST_TCP_WRITE_RECORD(req_header5);
    TEST_TCP_WRITE_RECORD(req_header6);
    TEST_TCP_WRITE_RECORD(req_header7);
    ASSERT_TRUE(this->m_tcp_write_queue.empty());

    esp_http_client_cleanup(client);
    this->m_flag_alloc_counting_enabled = false;
    esp_log_wrapper_clear();
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    this->m_flag_alloc_counting_enabled = true;
    ASSERT_EQ(0, this->m_alloc_free_call_count);
}

TEST_F(TestEspHttpClient, test_http_get_with_long_path_and_query_split_req_header) // NOLINT
{
    esp_http_client_config_t config = {};
    config.event_handler            = &event_handler;

    config.host = "a31415926535897-ats.iot.us-west-2.amazonaws.com";
    config.path
        = "/topics/gateway/environment/data/sensors/v1/room101/very_long_path"
          "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef"
          "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef"
          "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef"
          "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef"
          "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef"
          "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef"
          "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef"
          "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef";
    config.query
        = "X-Amz-Algorithm=AWS4-HMAC-SHA256&"
          "X-Amz-Credential=ASIAUTORANDOMID12345%2F20260419%2Fus-west-2%2Fiotdata%2Faws4_request&"
          "X-Amz-Date=20260419T010434Z&"
          "X-Amz-Expires=3600&"
          "X-Amz-SignedHeaders=content-type%3Bhost%3Bx-amz-client-id%3Bx-amz-date%3Bx-amz-meta-gateway-id%3Bx-amz-meta-"
          "location&"
          "X-Amz-Signature=b5e6f8d9a0c1b2c3d4e5f6g7h8i9j0k1l2m3n4o5p6q7r8s9t0u1v2w3x4y5z6a7";

    esp_http_client_handle_t client = esp_http_client_init(&config);
    ASSERT_NE(nullptr, client);

    this->m_tcp_connect_ret_code.push(ESP_OK);

    const string req_header
        = "GET "
          "/topics/gateway/environment/data/sensors/v1/room101/very_long_path"
          "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef"
          "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef"
          "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef"
          "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef"
          "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef"
          "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef"
          "0123456789abcdef0123456789abcdef0123456789abcdef012345678";
    const string req_header2
        = "9abcdef"
          "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef"
          "?"
          "X-Amz-Algorithm=AWS4-HMAC-SHA256&"
          "X-Amz-Credential=ASIAUTORANDOMID12345%2F20260419%2Fus-west-2%2Fiotdata%2Faws4_request&"
          "X-Amz-Date=20260419T010434Z&"
          "X-Amz-Expires=3600&"
          "X-Amz-SignedHeaders=content-type%3Bhost%3Bx-amz-client-id%3Bx-amz-date%3Bx-amz-meta-gateway-id%3Bx-amz-meta-"
          "location&"
          "X-Amz-Signature=b5e6f8d9a0c1b2c3d4e5f6g7h8i9j0k1l2m3n4o5p6q7r8s9t0u1v2w3x4y5z6a7"
          " " ESP_HTTP_CLIENT_DEFAULT_HTTP_PROTOCOL
          "\r\n"
          "User-Agent: " ESP_HTTP_CLIENT_DEFAULT_HTTP_USER_AGENT
          "\r\n"
          "Host: a31415926535897-";
    const string req_header3
        = "ats.iot.us-west-2.amazonaws.com"
          "\r\n"
          "Content-Length: 0"
          "\r\n"
          "\r\n";

    const string resp_content_data = this->addHttpRespHeaderAndData(HttpStatus_Ok, R"({})");

    ASSERT_EQ(ESP_OK, esp_http_client_perform(client));

    TEST_HTTP_EVENT_ON_CONNECTED();
    TEST_HTTP_EVENT_HEADERS_SENT();
    TEST_HTTP_EVENT_ON_HEADER("Content-Length", to_string(resp_content_data.length()));
    TEST_HTTP_EVENT_ON_DATA(resp_content_data.c_str(), resp_content_data.length());
    TEST_HTTP_EVENT_ON_FINISH();
    ASSERT_TRUE(this->m_http_event_queue.empty());

    TEST_TCP_WRITE_RECORD(req_header);
    TEST_TCP_WRITE_RECORD(req_header2);
    TEST_TCP_WRITE_RECORD(req_header3);
    ASSERT_TRUE(this->m_tcp_write_queue.empty());

    esp_http_client_cleanup(client);
    this->m_flag_alloc_counting_enabled = false;
    esp_log_wrapper_clear();
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    this->m_flag_alloc_counting_enabled = true;
    ASSERT_EQ(0, this->m_alloc_free_call_count);
}

TEST_F(TestEspHttpClient, test_http_get_with_long_path_and_query_using_custom_readers) // NOLINT
{
    this->m_custom_stream_reader_path
        = "topics/gateway/environment/data/sensors/v1/room101/very_long_path"
          "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef"
          "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef"
          "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef"
          "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef"
          "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef"
          "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef"
          "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef"
          "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef";
    this->m_custom_stream_reader_query
        = "X-Amz-Algorithm=AWS4-HMAC-SHA256&"
          "X-Amz-Credential=ASIAUTORANDOMID12345%2F20260419%2Fus-west-2%2Fiotdata%2Faws4_request&"
          "X-Amz-Date=20260419T010434Z&"
          "X-Amz-Expires=3600&"
          "X-Amz-SignedHeaders=content-type%3Bhost%3Bx-amz-client-id%3Bx-amz-date%3Bx-amz-meta-gateway-id%3Bx-amz-meta-"
          "location&"
          "X-Amz-Signature=b5e6f8d9a0c1b2c3d4e5f6g7h8i9j0k1l2m3n4o5p6q7r8s9t0u1v2w3x4y5z6a7"
          "3pTLuMCYEeLAVt3g+xXXVQuPcc3w9i9HOb0OZxC6rP684ZWOClhU34evfC1Gm8dRXDV0m1vYJssN"
          "H/7aw1bdtGECWvP9Omk8EQqouumRalKNpQ0YFat0fDE3oEa/vsCnrXZ2QUu2KkUVuCWoJf93etsmsGEH"
          "pV0LbusqY0328pdXUlGHqZ6irrTAYj20UOVGocT8A+HsGj285dYAcyP8YAuc133VKItFlzlmKz2gUJ31"
          "1EeDL11j57ZvG8B0gbuUgDhwaMnw5TMNvfIzUoEnhdruGFyq+LDk6zvw5sOcq3+w3FZDfz3UrmJ8wVDR"
          "6wn4gxBGoBB79fz8u72W7RP0lW3bDuhkS9UP0t5gPbeWnkPS5/P4q5gzXMOD1bX2rCMCKgmqzl+mArl3"
          "T+79h68hLDo93LgzvFIKWDxmJdUCzGAqDL7uXyJrS8wlp7Mf9n5NZHfEZaFk5I/TxgJHHd6JML7KgTen"
          "rANPiikpXEzK/bBvPso9z0hJY6vfK62KviwJobnhkbgrztcjQcz+EwhhFROTTEc72J5l5iuedI7pnIg6";

    esp_http_client_config_t config = {};
    config.event_handler            = &event_handler;

    config.host                            = "a31415926535897-ats.iot.us-west-2.amazonaws.com";
    config.path                            = nullptr;
    config.cb_path_stream_reader           = &cb_custom_stream_reader;
    config.cb_path_stream_reader_param     = const_cast<void*>(static_cast<const void*>("path"));
    config.cb_path_stream_reader_ctx_size  = sizeof(http_stream_reader_string_ctx_t);
    config.query                           = nullptr;
    config.cb_query_stream_reader          = &cb_custom_stream_reader;
    config.cb_query_stream_reader_param    = const_cast<void*>(static_cast<const void*>("query"));
    config.cb_query_stream_reader_ctx_size = sizeof(http_stream_reader_string_ctx_t);

    esp_http_client_handle_t client = esp_http_client_init(&config);
    ASSERT_NE(nullptr, client);

    this->m_tcp_connect_ret_code.push(ESP_OK);

    const string req_header
        = "GET "
          "/topics/gateway/environment/data/sensors/v1/room101/very_long_path"
          "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef"
          "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef"
          "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef"
          "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef"
          "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef"
          "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef"
          "0123456789abcdef0123456789abcdef0123456789abcdef012345678";
    const string req_header2
        = "9abcdef"
          "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef"
          "?"
          "X-Amz-Algorithm=AWS4-HMAC-SHA256&"
          "X-Amz-Credential=ASIAUTORANDOMID12345%2F20260419%2Fus-west-2%2Fiotdata%2Faws4_request&"
          "X-Amz-Date=20260419T010434Z&"
          "X-Amz-Expires=3600&"
          "X-Amz-SignedHeaders=content-type%3Bhost%3Bx-amz-client-id%3Bx-amz-date%3Bx-amz-meta-gateway-id%3Bx-amz-meta-"
          "location&"
          "X-Amz-Signature=b5e6f8d9a0c1b2c3d4e5f6g7h8i9j0k1l2m3n4o5p6q7r8s9t0u1v2w3x4y5z6a7"
          "3pTLuMCYEeLAVt3g+xXXVQuPcc3w9i9HOb0OZxC6rP684ZWOClhU34evfC1Gm8dRXDV0m1vYJssN";
    const string req_header3
        = "H/7aw1bdtGECWvP9Omk8EQqouumRalKNpQ0YFat0fDE3oEa/vsCnrXZ2QUu2KkUVuCWoJf93etsmsGEH"
          "pV0LbusqY0328pdXUlGHqZ6irrTAYj20UOVGocT8A+HsGj285dYAcyP8YAuc133VKItFlzlmKz2gUJ31"
          "1EeDL11j57ZvG8B0gbuUgDhwaMnw5TMNvfIzUoEnhdruGFyq+LDk6zvw5sOcq3+w3FZDfz3UrmJ8wVDR"
          "6wn4gxBGoBB79fz8u72W7RP0lW3bDuhkS9UP0t5gPbeWnkPS5/P4q5gzXMOD1bX2rCMCKgmqzl+mArl3"
          "T+79h68hLDo93LgzvFIKWDxmJdUCzGAqDL7uXyJrS8wlp7Mf9n5NZHfEZaFk5I/TxgJHHd6JML7KgTen"
          "rANPiikpXEzK/bBvPso9z0hJY6vfK62KviwJobnhkbgrztcjQcz+EwhhFROTTEc72J5l5iuedI7pnIg6"
          " "
          "HTTP/1.1"
          "\r\n"
          "User-Agent: "
          "Ruuvi Ga";
    const string req_header4
        = "teway HTTP Client/1.0"
          "\r\n"
          "Host: a31415926535897-ats.iot.us-west-2.amazonaws.com"
          "\r\n"
          "Content-Length: 0"
          "\r\n"
          "\r\n";

    const string resp_content_data = this->addHttpRespHeaderAndData(HttpStatus_Ok, R"({})");

    ASSERT_EQ(ESP_OK, esp_http_client_perform(client));

    TEST_HTTP_EVENT_ON_CONNECTED();
    TEST_HTTP_EVENT_HEADERS_SENT();
    TEST_HTTP_EVENT_ON_HEADER("Content-Length", to_string(resp_content_data.length()));
    TEST_HTTP_EVENT_ON_DATA(resp_content_data.c_str(), resp_content_data.length());
    TEST_HTTP_EVENT_ON_FINISH();
    ASSERT_TRUE(this->m_http_event_queue.empty());

    TEST_TCP_WRITE_RECORD(req_header);
    TEST_TCP_WRITE_RECORD(req_header2);
    TEST_TCP_WRITE_RECORD(req_header3);
    TEST_TCP_WRITE_RECORD(req_header4);
    ASSERT_TRUE(this->m_tcp_write_queue.empty());

    esp_http_client_cleanup(client);
    this->m_flag_alloc_counting_enabled = false;
    esp_log_wrapper_clear();
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    this->m_flag_alloc_counting_enabled = true;
    ASSERT_EQ(0, this->m_alloc_free_call_count);
}

TEST_F(TestEspHttpClient, test_http_get_split_http_method) // NOLINT
{
    esp_http_client_config_t config = {};
    config.event_handler            = &event_handler;
    config.buffer_size_tx           = 2;

    config.host = "a.b.c";
    config.path = "/";

    esp_http_client_handle_t client = esp_http_client_init(&config);
    ASSERT_NE(nullptr, client);

    this->m_tcp_connect_ret_code.push(ESP_OK);

    const string req_header = "GET / " ESP_HTTP_CLIENT_DEFAULT_HTTP_PROTOCOL
                              "\r\n"
                              "User-Agent: " ESP_HTTP_CLIENT_DEFAULT_HTTP_USER_AGENT
                              "\r\n"
                              "Host: a.b.c\r\n"
                              "Content-Length: 0\r\n"
                              "\r\n";

    const string resp_content_data = this->addHttpRespHeaderAndData(HttpStatus_Ok, R"({})");

    ASSERT_EQ(ESP_OK, esp_http_client_perform(client));

    TEST_HTTP_EVENT_ON_CONNECTED();
    TEST_HTTP_EVENT_HEADERS_SENT();
    TEST_HTTP_EVENT_ON_HEADER("Content-Length", to_string(resp_content_data.length()));
    TEST_HTTP_EVENT_ON_DATA(resp_content_data.c_str(), resp_content_data.length());
    TEST_HTTP_EVENT_ON_FINISH();
    ASSERT_TRUE(this->m_http_event_queue.empty());

    for (size_t i = 0; i < req_header.length(); i += 1)
    {
        const string req_header_part = req_header.substr(i, 1);
        TEST_TCP_WRITE_RECORD(req_header_part);
    }
    ASSERT_TRUE(this->m_tcp_write_queue.empty());

    esp_http_client_cleanup(client);
    this->m_flag_alloc_counting_enabled = false;
    esp_log_wrapper_clear();
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    this->m_flag_alloc_counting_enabled = true;
    ASSERT_EQ(0, this->m_alloc_free_call_count);
}

TEST_F(
    TestEspHttpClient,
    test_http_get_by_host_with_long_path_and_long_extra_header_fields_from_stream_reader) // NOLINT
{
    this->m_custom_stream_reader_path
        = "topics/gateway/environment/data/sensors/v1/room101/very_long_path"
          "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef"
          "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef"
          "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef"
          "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef"
          "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef"
          "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef"
          "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef"
          "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef";
    this->m_custom_stream_reader_query
        = "X-Amz-Algorithm=AWS4-HMAC-SHA256&"
          "X-Amz-Credential=ASIAUTORANDOMID12345%2F20260419%2Fus-west-2%2Fiotdata%2Faws4_request&"
          "X-Amz-Date=20260419T010434Z&"
          "X-Amz-Expires=3600&"
          "X-Amz-SignedHeaders=content-type%3Bhost%3Bx-amz-client-id%3Bx-amz-date%3Bx-amz-meta-gateway-id%3Bx-amz-meta-"
          "location&"
          "X-Amz-Signature=b5e6f8d9a0c1b2c3d4e5f6g7h8i9j0k1l2m3n4o5p6q7r8s9t0u1v2w3x4y5z6a7"
          "3pTLuMCYEeLAVt3g+xXXVQuPcc3w9i9HOb0OZxC6rP684ZWOClhU34evfC1Gm8dRXDV0m1vYJssN"
          "H/7aw1bdtGECWvP9Omk8EQqouumRalKNpQ0YFat0fDE3oEa/vsCnrXZ2QUu2KkUVuCWoJf93etsmsGEH"
          "pV0LbusqY0328pdXUlGHqZ6irrTAYj20UOVGocT8A+HsGj285dYAcyP8YAuc133VKItFlzlmKz2gUJ31"
          "1EeDL11j57ZvG8B0gbuUgDhwaMnw5TMNvfIzUoEnhdruGFyq+LDk6zvw5sOcq3+w3FZDfz3UrmJ8wVDR"
          "6wn4gxBGoBB79fz8u72W7RP0lW3bDuhkS9UP0t5gPbeWnkPS5/P4q5gzXMOD1bX2rCMCKgmqzl+mArl3"
          "T+79h68hLDo93LgzvFIKWDxmJdUCzGAqDL7uXyJrS8wlp7Mf9n5NZHfEZaFk5I/TxgJHHd6JML7KgTen"
          "rANPiikpXEzK/bBvPso9z0hJY6vfK62KviwJobnhkbgrztcjQcz+EwhhFROTTEc72J5l5iuedI7pnIg6";
    this->m_custom_stream_reader_header_val
        = "O/ASxxgjZf6HaiAF3nVQGukgaNG9AXwqUCAYK4DQHXNPlrzEhbCQGonRKrOnA47lw7RyltYelh6HlcVx"
          "B6paiDk0ob4UN9JoCC3odhsJfY0ZuPAvbkNW8f+HAI3ukaedDOQAt5ygz2AJ21olR+mha0aVx9CdxEWl"
          "5H5Cs0gNaBRwc+aMb3ztjNKQZmsmpJo8X4emQ26NGeOqv270hJptIIePvJGWlvq2hqLadWOtRTtcmTn+"
          "aw29PZekG8awBgWeNhu6Wo4fQphi9ZXfnVdeqAjzbMuDXMujmT2IPrZfTzbBA99W1qFM0TGJ6NQ5uFPQ"
          "j4xHC6YqxLFWcp3PPMWp46IUmhO+d9PNP+3Ds3tP0KeVX27MuKymi5s34zEgmphF7oHHV3ddPjmol/eG"
          "deCxAxyqG384IFRUdSxG7/ys92xWZ6x2ACLxxmh2UwXN3WHc1tDdp4hgnaEt76T+4hMaYIJS5qPtRlOy"
          "3G4alEa71aH27mxC+NGmvczmyZPwhDRQPNcqtLxUDyrzfuwJRD7O5oJPC6o6lOg5fc9QBNOIuK89OYeG";
    this->m_custom_stream_reader_headers
        = "X-Header1: "
          "mfVNuugx3aL6Ep550Uq5tqrx4NvNLKvxN1WSUIipcijb3ulRYa0b89f8LYItlflqG82oPl4xC1Ijre6w"
          "bvFprGTNFSzgWaT/JirudGDgE2MGkHFwsQI2dSPQyAfSlN0JHDRrHeq0rdz0/rtOg1dJ+yZp2yuf24pH"
          "3LamX1/dCpyGGxyuDcb4d3G7lRwPmuCHvzLJJlNDvH7+Svxxo4AQw3GNWeH3Tc9jk7NJducnHdFVvRDl"
          "bSUVI6UZ8fkNfEDnoboJnsUQbyV2Oqp8VPYWC6RTVvQJsagQj3avbMsFBFv0R2av6Nxwkus2us5X/Zop"
          "PabVabSYrXvrfbJaZ4GOhlC0yR4U0DJ8nVpgkaK4h1PR6+8gJqtiHJjiCJvCe9ikOearqRg2AbrIX9ye"
          "lxuVlJU8iQQN/YMKX7l3fRR74WfPjZbufH6geWY2qVQJwAtaRkUfMqIa+hUbT+UHoo4lHyPyH6rbFz9H"
          "o5BBuSgDxt/O/byFsz60MutgY79qfZF3uyC/iZcACe7HBBv+HN0xXtkSWOWP4RWuslUHqv+fkj5eQZyK"
          "\r\n"
          "X-Header2: "
          "3PB4LOIKbLfhpGApKFhnuk/OfJUDrw09fHEbbFOht0pVkAWF0eLPEmNjfgMz+RiBn5B5d3m/nG5XXhhE"
          "+5Z7VRuwXukrVrhMSpoxplPVYpl/q+dv5SoEb0OEB/1e7oD8t64v00+itkySZoPMG8xgD5igUd/RVlnf"
          "YSfH/YwkgO1LFq4lKhbsKnESslLxVj9sR59ayCmRP9ygjbCo7sU1urFSKTLcVtjgKN17GZhSzoOFoc60"
          "NDEPhXci/ElDMGVdIciABaZ6pxi87Rl4CdlStJFQ+9C6T12h7hvDmq7UWZmhO9UUqepX6TwAR/5UWvmm"
          "3LamX1/dCpyGGxyuDcb4d3G7lRwPmuCHvzLJJlNDvH7+Svxxo4AQw3GNWeH3Tc9jk7NJducnHdFVvRDl"
          "bSUVI6UZ8fkNfEDnoboJnsUQbyV2Oqp8VPYWC6RTVvQJsagQj3avbMsFBFv0R2av6Nxwkus2us5X/Zop"
          "PabVabSYrXvrfbJaZ4GOhlC0yR4U0DJ8nVpgkaK4h1PR6+8gJqtiHJjiCJvCe9ikOearqRg2AbrIX9ye"
          "wgiMgrsWdKPBbOFF9dIR12xuU8z01dnSd5JSgDUs+ksioBgHqwv2hP2jN/B0"
          "\r\n";

    esp_http_client_config_t config                = {};
    config.event_handler                           = &event_handler;
    config.host                                    = "a31415926535897-ats.iot.us-west-2.amazonaws.com";
    config.path                                    = nullptr;
    config.cb_path_stream_reader                   = &cb_custom_stream_reader;
    config.cb_path_stream_reader_param             = const_cast<void*>(static_cast<const void*>("path"));
    config.cb_path_stream_reader_ctx_size          = sizeof(http_stream_reader_string_ctx_t);
    config.query                                   = nullptr;
    config.cb_query_stream_reader                  = &cb_custom_stream_reader;
    config.cb_query_stream_reader_param            = const_cast<void*>(static_cast<const void*>("query"));
    config.cb_query_stream_reader_ctx_size         = sizeof(http_stream_reader_string_ctx_t);
    config.cb_extra_headers_stream_reader          = &cb_custom_stream_reader;
    config.cb_extra_headers_stream_reader_param    = const_cast<void*>(static_cast<const void*>("headers"));
    config.cb_extra_headers_stream_reader_ctx_size = sizeof(http_stream_reader_string_ctx_t);

    esp_http_client_handle_t client = esp_http_client_init(&config);
    ASSERT_NE(nullptr, client);

    http_stream_reader_string_ctx_t context = {};
    http_stream_reader_desc_t       desc    = {
                 .p_cb    = &cb_custom_stream_reader,
                 .p_param = const_cast<void*>(static_cast<const void*>("header")),
                 .p_ctx   = &context,
    };

    esp_http_client_set_header_from_stream(client, "X-Header0", &desc);

    this->m_tcp_connect_ret_code.push(ESP_OK);
    const string req_headers[] = {
        "GET "
        "/topics/gateway/environment/data/sensors/v1/room101/very_long_path"
        "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef"
        "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef"
        "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef"
        "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef"
        "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef"
        "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef"
        "0123456789abcdef0123456789abcdef0123456789abcdef012345678",

        "9abcdef"
        "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef"
        "?"
        "X-Amz-Algorithm=AWS4-HMAC-SHA256&"
        "X-Amz-Credential=ASIAUTORANDOMID12345%2F20260419%2Fus-west-2%2Fiotdata%2Faws4_request&"
        "X-Amz-Date=20260419T010434Z&"
        "X-Amz-Expires=3600&"
        "X-Amz-SignedHeaders=content-type%3Bhost%3Bx-amz-client-id%3Bx-amz-date%3Bx-amz-meta-gateway-id%3Bx-amz-meta-"
        "location&"
        "X-Amz-Signature=b5e6f8d9a0c1b2c3d4e5f6g7h8i9j0k1l2m3n4o5p6q7r8s9t0u1v2w3x4y5z6a7"
        "3pTLuMCYEeLAVt3g+xXXVQuPcc3w9i9HOb0OZxC6rP684ZWOClhU34evfC1Gm8dRXDV0m1vYJssN",

        "H/7aw1bdtGECWvP9Omk8EQqouumRalKNpQ0YFat0fDE3oEa/vsCnrXZ2QUu2KkUVuCWoJf93etsmsGEH"
        "pV0LbusqY0328pdXUlGHqZ6irrTAYj20UOVGocT8A+HsGj285dYAcyP8YAuc133VKItFlzlmKz2gUJ31"
        "1EeDL11j57ZvG8B0gbuUgDhwaMnw5TMNvfIzUoEnhdruGFyq+LDk6zvw5sOcq3+w3FZDfz3UrmJ8wVDR"
        "6wn4gxBGoBB79fz8u72W7RP0lW3bDuhkS9UP0t5gPbeWnkPS5/P4q5gzXMOD1bX2rCMCKgmqzl+mArl3"
        "T+79h68hLDo93LgzvFIKWDxmJdUCzGAqDL7uXyJrS8wlp7Mf9n5NZHfEZaFk5I/TxgJHHd6JML7KgTen"
        "rANPiikpXEzK/bBvPso9z0hJY6vfK62KviwJobnhkbgrztcjQcz+EwhhFROTTEc72J5l5iuedI7pnIg6"
        " "
        "HTTP/1.1"
        "\r\n"
        "User-Agent: "
        "Ruuvi Ga",

        "teway HTTP Client/1.0"
        "\r\n"
        "Host: a31415926535897-ats.iot.us-west-2.amazonaws.com"
        "\r\n"
        "X-Header0: "
        "O/ASxxgjZf6HaiAF3nVQGukgaNG9AXwqUCAYK4DQHXNPlrzEhbCQGonRKrOnA47lw7RyltYelh6HlcVx"
        "B6paiDk0ob4UN9JoCC3odhsJfY0ZuPAvbkNW8f+HAI3ukaedDOQAt5ygz2AJ21olR+mha0aVx9CdxEWl"
        "5H5Cs0gNaBRwc+aMb3ztjNKQZmsmpJo8X4emQ26NGeOqv270hJptIIePvJGWlvq2hqLadWOtRTtcmTn+"
        "aw29PZekG8awBgWeNhu6Wo4fQphi9ZXfnVdeqAjzbMuDXMujmT2IPrZfTzbBA99W1qFM0TGJ6NQ5uFPQ"
        "j4xHC6YqxLFWcp3PPMWp46IUmhO+d9PNP+3Ds3tP0KeVX27MuKymi5s34zEgmphF7oHHV3ddPjmol/eG"
        "deCxAxyqG384IFRUdSxG7/",

        "ys92xWZ6x2ACLxxmh2UwXN3WHc1tDdp4hgnaEt76T+4hMaYIJS5qPtRlOy"
        "3G4alEa71aH27mxC+NGmvczmyZPwhDRQPNcqtLxUDyrzfuwJRD7O5oJPC6o6lOg5fc9QBNOIuK89OYeG"
        "\r\n"
        "Content-Length: 0\r\n"
        "X-Header1: "
        "mfVNuugx3aL6Ep550Uq5tqrx4NvNLKvxN1WSUIipcijb3ulRYa0b89f8LYItlflqG82oPl4xC1Ijre6w"
        "bvFprGTNFSzgWaT/JirudGDgE2MGkHFwsQI2dSPQyAfSlN0JHDRrHeq0rdz0/rtOg1dJ+yZp2yuf24pH"
        "3LamX1/dCpyGGxyuDcb4d3G7lRwPmuCHvzLJJlNDvH7+Svxxo4AQw3GNWeH3Tc9jk7NJducnHdFVvRDl"
        "bSUVI6UZ8fkNfEDnoboJnsUQbyV2Oqp8VPYWC6RTVvQJsagQj3avbMsFBFv0R2av6Nxwkus2us5X/Zop"
        "PabVabSYrXvrfbJaZ4GOh",

        "lC0yR4U0DJ8nVpgkaK4h1PR6+8gJqtiHJjiCJvCe9ikOearqRg2AbrIX9ye"
        "lxuVlJU8iQQN/YMKX7l3fRR74WfPjZbufH6geWY2qVQJwAtaRkUfMqIa+hUbT+UHoo4lHyPyH6rbFz9H"
        "o5BBuSgDxt/O/byFsz60MutgY79qfZF3uyC/iZcACe7HBBv+HN0xXtkSWOWP4RWuslUHqv+fkj5eQZyK"
        "\r\n"
        "X-Header2: "
        "3PB4LOIKbLfhpGApKFhnuk/OfJUDrw09fHEbbFOht0pVkAWF0eLPEmNjfgMz+RiBn5B5d3m/nG5XXhhE"
        "+5Z7VRuwXukrVrhMSpoxplPVYpl/q+dv5SoEb0OEB/1e7oD8t64v00+itkySZoPMG8xgD5igUd/RVlnf"
        "YSfH/YwkgO1LFq4lKhbsKnESslLxVj9sR59ayCmRP9ygjbCo7sU1urFSKTLcVtjgKN17GZhSzoOFoc60"
        "NDEPhXci/ElDMGVdIciABaZ6pxi87Rl4CdlStJF",

        "Q+9C6T12h7hvDmq7UWZmhO9UUqepX6TwAR/5UWvmm"
        "3LamX1/dCpyGGxyuDcb4d3G7lRwPmuCHvzLJJlNDvH7+Svxxo4AQw3GNWeH3Tc9jk7NJducnHdFVvRDl"
        "bSUVI6UZ8fkNfEDnoboJnsUQbyV2Oqp8VPYWC6RTVvQJsagQj3avbMsFBFv0R2av6Nxwkus2us5X/Zop"
        "PabVabSYrXvrfbJaZ4GOhlC0yR4U0DJ8nVpgkaK4h1PR6+8gJqtiHJjiCJvCe9ikOearqRg2AbrIX9ye"
        "wgiMgrsWdKPBbOFF9dIR12xuU8z01dnSd5JSgDUs+ksioBgHqwv2hP2jN/B0"
        "\r\n"
        "\r\n",
    };
    const string resp_content_data = this->addHttpRespHeaderAndData(HttpStatus_Ok, R"({})");

    ASSERT_EQ(ESP_OK, esp_http_client_perform(client));

    TEST_HTTP_EVENT_ON_CONNECTED();
    TEST_HTTP_EVENT_HEADERS_SENT();
    TEST_HTTP_EVENT_ON_HEADER("Content-Length", to_string(resp_content_data.length()));
    TEST_HTTP_EVENT_ON_DATA(resp_content_data.c_str(), resp_content_data.length());
    TEST_HTTP_EVENT_ON_FINISH();
    ASSERT_TRUE(this->m_http_event_queue.empty());

    for (const string& req_header : req_headers)
    {
        TEST_TCP_WRITE_RECORD(req_header);
    }
    ASSERT_TRUE(this->m_tcp_write_queue.empty());

    esp_http_client_cleanup(client);
    this->m_flag_alloc_counting_enabled = false;
    esp_log_wrapper_clear();
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    this->m_flag_alloc_counting_enabled = true;
    ASSERT_EQ(0, this->m_alloc_free_call_count);
}
