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

#include "esp_log_wrapper.hpp"
#include "esp_transport_internal.h"
#include "tcp_transport_stub.h"
#include "TQueue.hpp"
#include "http_event.hpp"

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
        g_pTestClass = this;
        esp_log_wrapper_init();
    }

    void
    TearDown() override
    {
        g_pTestClass = nullptr;
        esp_log_wrapper_deinit();
    }

public:
    TestEspHttpClient();

    ~TestEspHttpClient() override;

    std::queue<int>                        m_tcp_connect_ret_code;
    std::queue<int>                        m_tcp_connect_async_ret_code;
    std::queue<int>                        m_tcp_write_ret_code;
    std::queue<TcpWriteRecord>             m_tcp_write_queue;
    std::queue<TcpReadRecord>              m_tcp_read_queue;
    std::queue<std::shared_ptr<HttpEvent>> m_http_event_queue;

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
    : m_tcp_connect_ret_code {}
    , m_tcp_connect_async_ret_code {}
    , m_tcp_write_ret_code {}
    , m_tcp_write_queue {}
    , m_tcp_read_queue {}
    , m_http_event_queue {}
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
        "GET /api?cmd1=qwe&cmd2=asd HTTP/1.1\r\n"
        "User-Agent: Ruuvi Gateway HTTP Client/1.0\r\n"
        "Host: myhost.com\r\n"
        "Content-Length: 0\r\n"
        "\r\n",
        req_header);
    TEST_TCP_WRITE_RECORD(req_header);
    ASSERT_TRUE(this->m_tcp_write_queue.empty());

    esp_http_client_cleanup(client);
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
}
