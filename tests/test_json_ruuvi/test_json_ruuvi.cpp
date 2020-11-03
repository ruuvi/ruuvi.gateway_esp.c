/**
 * @file test_bin2hex.cpp
 * @author TheSomeMan
 * @date 2020-08-27
 * @copyright Ruuvi Innovations Ltd, license BSD-3-Clause.
 */

#include "json_ruuvi.h"
#include <cstring>
#include "gtest/gtest.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log_wrapper.hpp"

using namespace std;

/*** Google-test class implementation
 * *********************************************************************************/

class TestJsonRuuvi;
static TestJsonRuuvi *g_pTestClass;

extern "C" {

static const char g_task_name[] = "main";

char *
pcTaskGetName(TaskHandle_t xTaskToQuery)
{
    (void)xTaskToQuery;
    return const_cast<char *>(g_task_name);
}

} // extern "C"

class MemAllocTrace
{
    vector<void *> allocated_mem;

    std::vector<void *>::iterator
    find(void *ptr)
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
    add(void *ptr)
    {
        auto iter = find(ptr);
        assert(iter == this->allocated_mem.end()); // ptr was found in the list of allocated memory blocks
        this->allocated_mem.push_back(ptr);
    }
    void
    remove(void *ptr)
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
};

class TestJsonRuuvi : public ::testing::Test
{
private:
protected:
    void
    SetUp() override
    {
        esp_log_wrapper_init();
        g_pTestClass = this;

        this->m_malloc_cnt         = 0;
        this->m_malloc_fail_on_cnt = 0;
    }

    void
    TearDown() override
    {
        g_pTestClass = nullptr;
        esp_log_wrapper_deinit();
    }

public:
    TestJsonRuuvi();

    ~TestJsonRuuvi() override;

    MemAllocTrace m_mem_alloc_trace;
    uint32_t      m_malloc_cnt;
    uint32_t      m_malloc_fail_on_cnt;
};

TestJsonRuuvi::TestJsonRuuvi()
    : m_malloc_cnt(0)
    , m_malloc_fail_on_cnt(0)
    , Test()
{
}

extern "C" {

void *
app_malloc(const size_t size)
{
    if (++g_pTestClass->m_malloc_cnt == g_pTestClass->m_malloc_fail_on_cnt)
    {
        return nullptr;
    }
    void *ptr = malloc(size);
    assert(nullptr != ptr);
    g_pTestClass->m_mem_alloc_trace.add(ptr);
    return ptr;
}

void
app_free(void *ptr)
{
    g_pTestClass->m_mem_alloc_trace.remove(ptr);
    free(ptr);
}

void *
app_calloc(const size_t nmemb, const size_t size)
{
    if (++g_pTestClass->m_malloc_cnt == g_pTestClass->m_malloc_fail_on_cnt)
    {
        return nullptr;
    }
    void *ptr = calloc(nmemb, size);
    assert(nullptr != ptr);
    g_pTestClass->m_mem_alloc_trace.add(ptr);
    return ptr;
}

} // extern "C"

TestJsonRuuvi::~TestJsonRuuvi() = default;

#define TEST_CHECK_LOG_RECORD_EX(tag_, level_, msg_, flag_skip_file_info_) \
    do \
    { \
        ASSERT_FALSE(esp_log_wrapper_is_empty()); \
        const LogRecord log_record = esp_log_wrapper_pop(); \
        ASSERT_EQ(level_, log_record.level); \
        ASSERT_EQ(string(tag_), log_record.tag); \
        if (flag_skip_file_info_) \
        { \
            const char *p = strchr(log_record.message.c_str(), ' '); \
            assert(NULL != p); \
            p += 1; \
            p = strchr(p, ' '); \
            assert(NULL != p); \
            p += 1; \
            p = strchr(p, ' '); \
            assert(NULL != p); \
            p += 1; \
            ASSERT_EQ(string(msg_), p); \
        } \
        else \
        { \
            ASSERT_EQ(string(msg_), log_record.message); \
        } \
    } while (0)

#define TEST_CHECK_LOG_RECORD(level_, msg_)         TEST_CHECK_LOG_RECORD_EX("http_server", level_, msg_, false)
#define TEST_CHECK_LOG_RECORD_NO_FILE(level_, msg_) TEST_CHECK_LOG_RECORD_EX("http_server", level_, msg_, true)

/*** Unit-Tests
 * *******************************************************************************************************/

TEST_F(TestJsonRuuvi, copy_string_val_ok) // NOLINT
{
    cJSON *root = cJSON_CreateObject();
    ASSERT_NE(nullptr, root);
    cJSON_AddStringToObject(root, "attr", "value123");
    char buf[80];
    ASSERT_TRUE(json_ruuvi_copy_string_val(root, "attr", buf, sizeof(buf), false));
    ASSERT_EQ(string("value123"), string(buf));
    cJSON_Delete(root);
    TEST_CHECK_LOG_RECORD_NO_FILE(ESP_LOG_DEBUG, "attr: value123");
    ASSERT_TRUE(esp_log_wrapper_is_empty());
}

TEST_F(TestJsonRuuvi, copy_string_val_failed_without_log) // NOLINT
{
    cJSON *root = cJSON_CreateObject();
    ASSERT_NE(nullptr, root);
    cJSON_AddStringToObject(root, "attr", "value123");
    char buf[80];
    ASSERT_FALSE(json_ruuvi_copy_string_val(root, "attr2", buf, sizeof(buf), false));
    cJSON_Delete(root);
    ASSERT_TRUE(esp_log_wrapper_is_empty());
}

TEST_F(TestJsonRuuvi, copy_string_val_failed_log) // NOLINT
{
    cJSON *root = cJSON_CreateObject();
    ASSERT_NE(nullptr, root);
    cJSON_AddStringToObject(root, "attr", "value123");
    char buf[80];
    ASSERT_FALSE(json_ruuvi_copy_string_val(root, "attr2", buf, sizeof(buf), true));
    cJSON_Delete(root);
    TEST_CHECK_LOG_RECORD_NO_FILE(ESP_LOG_ERROR, "attr2 not found");
    ASSERT_TRUE(esp_log_wrapper_is_empty());
}

TEST_F(TestJsonRuuvi, get_bool_val_ok) // NOLINT
{
    cJSON *root = cJSON_CreateObject();
    ASSERT_NE(nullptr, root);
    cJSON_AddBoolToObject(root, "attr", true);
    bool val = false;
    ASSERT_TRUE(json_ruuvi_get_bool_val(root, "attr", &val, false));
    ASSERT_TRUE(val);
    cJSON_Delete(root);
    TEST_CHECK_LOG_RECORD_NO_FILE(ESP_LOG_DEBUG, "attr: 1");
    ASSERT_TRUE(esp_log_wrapper_is_empty());
}

TEST_F(TestJsonRuuvi, get_bool_val_failed_without_log) // NOLINT
{
    cJSON *root = cJSON_CreateObject();
    ASSERT_NE(nullptr, root);
    cJSON_AddBoolToObject(root, "attr", true);
    bool val = false;
    ASSERT_FALSE(json_ruuvi_get_bool_val(root, "attr2", &val, false));
    cJSON_Delete(root);
    ASSERT_TRUE(esp_log_wrapper_is_empty());
}

TEST_F(TestJsonRuuvi, get_bool_val_failed_log) // NOLINT
{
    cJSON *root = cJSON_CreateObject();
    ASSERT_NE(nullptr, root);
    cJSON_AddBoolToObject(root, "attr", true);
    bool val = false;
    ASSERT_FALSE(json_ruuvi_get_bool_val(root, "attr2", &val, true));
    cJSON_Delete(root);
    TEST_CHECK_LOG_RECORD_NO_FILE(ESP_LOG_ERROR, "attr2 not found");
    ASSERT_TRUE(esp_log_wrapper_is_empty());
}

TEST_F(TestJsonRuuvi, get_uint16_val_ok) // NOLINT
{
    cJSON *root = cJSON_CreateObject();
    ASSERT_NE(nullptr, root);
    cJSON_AddNumberToObject(root, "attr", 123.0);
    uint16_t val = 0;
    ASSERT_TRUE(json_ruuvi_get_uint16_val(root, "attr", &val, false));
    ASSERT_EQ(123, val);
    cJSON_Delete(root);
    TEST_CHECK_LOG_RECORD_NO_FILE(ESP_LOG_DEBUG, "attr: 123");
    ASSERT_TRUE(esp_log_wrapper_is_empty());
}

TEST_F(TestJsonRuuvi, get_uint16_val_failed_without_log) // NOLINT
{
    cJSON *root = cJSON_CreateObject();
    ASSERT_NE(nullptr, root);
    cJSON_AddNumberToObject(root, "attr", 123.0);
    uint16_t val = 0;
    ASSERT_FALSE(json_ruuvi_get_uint16_val(root, "attr2", &val, false));
    cJSON_Delete(root);
    ASSERT_TRUE(esp_log_wrapper_is_empty());
}

TEST_F(TestJsonRuuvi, get_uint16_val_failed_log) // NOLINT
{
    cJSON *root = cJSON_CreateObject();
    ASSERT_NE(nullptr, root);
    cJSON_AddNumberToObject(root, "attr", 123.0);
    uint16_t val = 0;
    ASSERT_FALSE(json_ruuvi_get_uint16_val(root, "attr2", &val, true));
    cJSON_Delete(root);
    TEST_CHECK_LOG_RECORD_NO_FILE(ESP_LOG_ERROR, "attr2 not found or invalid");
    ASSERT_TRUE(esp_log_wrapper_is_empty());
}

TEST_F(TestJsonRuuvi, json_ruuvi_parse) // NOLINT
{
    cJSON *root = cJSON_CreateObject();
    ASSERT_NE(nullptr, root);
    cJSON_AddBoolToObject(root, "eth_dhcp", true);
    cJSON_AddStringToObject(root, "eth_static_ip", "192.168.1.1");
    cJSON_AddStringToObject(root, "eth_netmask", "255.255.255.0");
    cJSON_AddStringToObject(root, "eth_gw", "192.168.0.1");
    cJSON_AddStringToObject(root, "eth_dns1", "8.8.8.8");
    cJSON_AddStringToObject(root, "eth_dns2", "4.4.4.4");

    cJSON_AddBoolToObject(root, "use_mqtt", true);
    cJSON_AddStringToObject(root, "mqtt_server", "mqtt.server.org");
    cJSON_AddStringToObject(root, "mqtt_prefix", "prefix");
    cJSON_AddNumberToObject(root, "mqtt_port", 1234);
    cJSON_AddStringToObject(root, "mqtt_user", "user123");
    cJSON_AddStringToObject(root, "mqtt_pass", "pass123");

    cJSON_AddBoolToObject(root, "use_http", false);
    cJSON_AddStringToObject(root, "http_url", "https://api.ruuvi.com:456/api");
    cJSON_AddStringToObject(root, "http_user", "user567");
    cJSON_AddStringToObject(root, "http_pass", "pass567");

    cJSON_AddBoolToObject(root, "use_filtering", true);
    cJSON_AddNumberToObject(root, "company_id", 888);

    cJSON_AddStringToObject(root, "coordinates", "coord:123,456");

    ruuvi_gateway_config_t gw_cfg = { 0 };
    ASSERT_TRUE(json_ruuvi_parse(root, &gw_cfg));
    cJSON_Delete(root);
    ASSERT_TRUE(gw_cfg.eth.eth_dhcp);
    ASSERT_EQ(string("192.168.1.1"), gw_cfg.eth.eth_static_ip);
    ASSERT_EQ(string("255.255.255.0"), gw_cfg.eth.eth_netmask);
    ASSERT_EQ(string("192.168.0.1"), gw_cfg.eth.eth_gw);
    ASSERT_EQ(string("8.8.8.8"), gw_cfg.eth.eth_dns1);
    ASSERT_EQ(string("4.4.4.4"), gw_cfg.eth.eth_dns2);
    ASSERT_TRUE(gw_cfg.mqtt.use_mqtt);
    ASSERT_EQ(string("mqtt.server.org"), gw_cfg.mqtt.mqtt_server);
    ASSERT_EQ(string("prefix"), gw_cfg.mqtt.mqtt_prefix);
    ASSERT_EQ(1234, gw_cfg.mqtt.mqtt_port);
    ASSERT_EQ(string("user123"), gw_cfg.mqtt.mqtt_user);
    ASSERT_EQ(string("pass123"), gw_cfg.mqtt.mqtt_pass);
    ASSERT_FALSE(gw_cfg.http.use_http);
    ASSERT_EQ(string("https://api.ruuvi.com:456/api"), gw_cfg.http.http_url);
    ASSERT_EQ(string("user567"), gw_cfg.http.http_user);
    ASSERT_EQ(string("pass567"), gw_cfg.http.http_pass);
    ASSERT_TRUE(gw_cfg.filter.company_filter);
    ASSERT_EQ(888, gw_cfg.filter.company_id);
    ASSERT_EQ(string("coord:123,456"), gw_cfg.coordinates);

    TEST_CHECK_LOG_RECORD_NO_FILE(ESP_LOG_DEBUG, "Got SETTINGS:");
    TEST_CHECK_LOG_RECORD_NO_FILE(ESP_LOG_DEBUG, "eth_dhcp: 1");
    TEST_CHECK_LOG_RECORD_NO_FILE(ESP_LOG_DEBUG, "eth_static_ip: 192.168.1.1");
    TEST_CHECK_LOG_RECORD_NO_FILE(ESP_LOG_DEBUG, "eth_netmask: 255.255.255.0");
    TEST_CHECK_LOG_RECORD_NO_FILE(ESP_LOG_DEBUG, "eth_gw: 192.168.0.1");
    TEST_CHECK_LOG_RECORD_NO_FILE(ESP_LOG_DEBUG, "eth_dns1: 8.8.8.8");
    TEST_CHECK_LOG_RECORD_NO_FILE(ESP_LOG_DEBUG, "eth_dns2: 4.4.4.4");
    TEST_CHECK_LOG_RECORD_NO_FILE(ESP_LOG_DEBUG, "use_mqtt: 1");
    TEST_CHECK_LOG_RECORD_NO_FILE(ESP_LOG_DEBUG, "mqtt_server: mqtt.server.org");
    TEST_CHECK_LOG_RECORD_NO_FILE(ESP_LOG_DEBUG, "mqtt_prefix: prefix");
    TEST_CHECK_LOG_RECORD_NO_FILE(ESP_LOG_DEBUG, "mqtt_port: 1234");
    TEST_CHECK_LOG_RECORD_NO_FILE(ESP_LOG_DEBUG, "mqtt_user: user123");
    TEST_CHECK_LOG_RECORD_NO_FILE(ESP_LOG_DEBUG, "mqtt_pass: pass123");
    TEST_CHECK_LOG_RECORD_NO_FILE(ESP_LOG_DEBUG, "use_http: 0");
    TEST_CHECK_LOG_RECORD_NO_FILE(ESP_LOG_DEBUG, "http_url: https://api.ruuvi.com:456/api");
    TEST_CHECK_LOG_RECORD_NO_FILE(ESP_LOG_DEBUG, "http_user: user567");
    TEST_CHECK_LOG_RECORD_NO_FILE(ESP_LOG_DEBUG, "http_pass: pass567");
    TEST_CHECK_LOG_RECORD_NO_FILE(ESP_LOG_DEBUG, "use_filtering: 1");
    TEST_CHECK_LOG_RECORD_NO_FILE(ESP_LOG_DEBUG, "company_id: 888");
    TEST_CHECK_LOG_RECORD_NO_FILE(ESP_LOG_DEBUG, "coordinates: coord:123,456");
    ASSERT_TRUE(esp_log_wrapper_is_empty());
}

TEST_F(TestJsonRuuvi, json_ruuvi_parse_http_body) // NOLINT
{
    ruuvi_gateway_config_t gw_cfg = { 0 };
    cJSON_Hooks            hooks  = {
        .malloc_fn = &app_malloc,
        .free_fn   = &app_free,
    };
    g_pTestClass->m_malloc_fail_on_cnt = 0;
    cJSON_InitHooks(&hooks);
    ASSERT_TRUE(json_ruuvi_parse_http_body(
        "{"
        "\"use_mqtt\":true,"
        "\"mqtt_server\":\"test.mosquitto.org\","
        "\"mqtt_port\":1883,"
        "\"mqtt_prefix\":\"ruuvi/30:AE:A4:02:84:A4\","
        "\"mqtt_user\":\"\","
        "\"mqtt_pass\":\"\","
        "\"use_http\":false,"
        "\"http_url\":\"https://network.ruuvi.com:443/gwapi/v1\","
        "\"http_user\":\"\","
        "\"http_pass\":\"\","
        "\"use_filtering\":true"
        "}",
        &gw_cfg));

    ASSERT_FALSE(gw_cfg.eth.eth_dhcp);
    ASSERT_EQ(string(""), gw_cfg.eth.eth_static_ip);
    ASSERT_EQ(string(""), gw_cfg.eth.eth_netmask);
    ASSERT_EQ(string(""), gw_cfg.eth.eth_gw);
    ASSERT_EQ(string(""), gw_cfg.eth.eth_dns1);
    ASSERT_EQ(string(""), gw_cfg.eth.eth_dns2);
    ASSERT_TRUE(gw_cfg.mqtt.use_mqtt);
    ASSERT_EQ(string("test.mosquitto.org"), gw_cfg.mqtt.mqtt_server);
    ASSERT_EQ(string("ruuvi/30:AE:A4:02:84:A4"), gw_cfg.mqtt.mqtt_prefix);
    ASSERT_EQ(1883, gw_cfg.mqtt.mqtt_port);
    ASSERT_EQ(string(""), gw_cfg.mqtt.mqtt_user);
    ASSERT_EQ(string(""), gw_cfg.mqtt.mqtt_pass);
    ASSERT_FALSE(gw_cfg.http.use_http);
    ASSERT_EQ(string("https://network.ruuvi.com:443/gwapi/v1"), gw_cfg.http.http_url);
    ASSERT_EQ(string(""), gw_cfg.http.http_user);
    ASSERT_EQ(string(""), gw_cfg.http.http_pass);
    ASSERT_TRUE(gw_cfg.filter.company_filter);
    ASSERT_EQ(0, gw_cfg.filter.company_id);
    ASSERT_EQ(string(""), gw_cfg.coordinates);

    TEST_CHECK_LOG_RECORD_NO_FILE(ESP_LOG_DEBUG, "Got SETTINGS:");
    TEST_CHECK_LOG_RECORD_NO_FILE(ESP_LOG_ERROR, "eth_dhcp not found");
    TEST_CHECK_LOG_RECORD_NO_FILE(ESP_LOG_ERROR, "eth_static_ip not found");
    TEST_CHECK_LOG_RECORD_NO_FILE(ESP_LOG_ERROR, "eth_netmask not found");
    TEST_CHECK_LOG_RECORD_NO_FILE(ESP_LOG_ERROR, "eth_gw not found");
    TEST_CHECK_LOG_RECORD_NO_FILE(ESP_LOG_ERROR, "eth_dns1 not found");
    TEST_CHECK_LOG_RECORD_NO_FILE(ESP_LOG_ERROR, "eth_dns2 not found");
    TEST_CHECK_LOG_RECORD_NO_FILE(ESP_LOG_DEBUG, "use_mqtt: 1");
    TEST_CHECK_LOG_RECORD_NO_FILE(ESP_LOG_DEBUG, "mqtt_server: test.mosquitto.org");
    TEST_CHECK_LOG_RECORD_NO_FILE(ESP_LOG_DEBUG, "mqtt_prefix: ruuvi/30:AE:A4:02:84:A4");
    TEST_CHECK_LOG_RECORD_NO_FILE(ESP_LOG_DEBUG, "mqtt_port: 1883");
    TEST_CHECK_LOG_RECORD_NO_FILE(ESP_LOG_DEBUG, "mqtt_user: ");
    TEST_CHECK_LOG_RECORD_NO_FILE(ESP_LOG_DEBUG, "mqtt_pass: ");
    TEST_CHECK_LOG_RECORD_NO_FILE(ESP_LOG_DEBUG, "use_http: 0");
    TEST_CHECK_LOG_RECORD_NO_FILE(ESP_LOG_DEBUG, "http_url: https://network.ruuvi.com:443/gwapi/v1");
    TEST_CHECK_LOG_RECORD_NO_FILE(ESP_LOG_DEBUG, "http_user: ");
    TEST_CHECK_LOG_RECORD_NO_FILE(ESP_LOG_DEBUG, "http_pass: ");
    TEST_CHECK_LOG_RECORD_NO_FILE(ESP_LOG_DEBUG, "use_filtering: 1");
    TEST_CHECK_LOG_RECORD_NO_FILE(ESP_LOG_ERROR, "company_id not found or invalid");
    TEST_CHECK_LOG_RECORD_NO_FILE(ESP_LOG_ERROR, "coordinates not found");
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_TRUE(this->m_mem_alloc_trace.is_empty());
}

TEST_F(TestJsonRuuvi, json_ruuvi_parse_http_body_malloc_failed) // NOLINT
{
    ruuvi_gateway_config_t gw_cfg = { 0 };
    cJSON_Hooks            hooks  = {
        .malloc_fn = &app_malloc,
        .free_fn   = &app_free,
    };
    g_pTestClass->m_malloc_fail_on_cnt = 1;
    cJSON_InitHooks(&hooks);
    ASSERT_FALSE(json_ruuvi_parse_http_body(
        "{"
        "\"use_mqtt\":true,"
        "\"mqtt_server\":\"test.mosquitto.org\","
        "\"mqtt_port\":1883,"
        "\"mqtt_prefix\":\"ruuvi/30:AE:A4:02:84:A4\","
        "\"mqtt_user\":\"\","
        "\"mqtt_pass\":\"\","
        "\"use_http\":false,"
        "\"http_url\":\"https://network.ruuvi.com:443/gwapi/v1\","
        "\"http_user\":\"\","
        "\"http_pass\":\"\","
        "\"use_filtering\":true"
        "}",
        &gw_cfg));

    TEST_CHECK_LOG_RECORD_NO_FILE(ESP_LOG_ERROR, "Failed to parse json or no memory");
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_TRUE(this->m_mem_alloc_trace.is_empty());
}
