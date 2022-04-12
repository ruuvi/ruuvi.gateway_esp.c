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
#include "os_mutex_recursive.h"
#include "esp_log_wrapper.hpp"
#include "gw_cfg_default.h"

using namespace std;

/*** Google-test class implementation
 * *********************************************************************************/

class TestJsonRuuvi;
static TestJsonRuuvi *g_pTestClass;

extern "C" {

const char *
os_task_get_name(void)
{
    static const char g_task_name[] = "main";
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

        const gw_cfg_default_init_param_t init_params = {
            .wifi_ap_ssid        = { "" },
            .device_id           = { 0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77 },
            .esp32_fw_ver        = { "v1.10.0" },
            .nrf52_fw_ver        = { "v0.7.1" },
            .nrf52_mac_addr      = { 0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF },
            .esp32_mac_addr_wifi = { 0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0x11 },
            .esp32_mac_addr_eth  = { 0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0x22 },
        };
        gw_cfg_default_init(&init_params, nullptr);
        gw_cfg_init();

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
os_malloc(const size_t size)
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
os_free_internal(void *ptr)
{
    g_pTestClass->m_mem_alloc_trace.remove(ptr);
    free(ptr);
}

void *
os_calloc(const size_t nmemb, const size_t size)
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

os_mutex_recursive_t
os_mutex_recursive_create_static(os_mutex_recursive_static_t *const p_mutex_static)
{
    return (os_mutex_recursive_t)p_mutex_static;
}

void
os_mutex_recursive_delete(os_mutex_recursive_t *const ph_mutex)
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

} // extern "C"

TestJsonRuuvi::~TestJsonRuuvi() = default;

#define TEST_CHECK_LOG_RECORD_HTTP_SERVER(level_, msg_) \
    ESP_LOG_WRAPPER_TEST_CHECK_LOG_RECORD("http_server", level_, msg_)
#define TEST_CHECK_LOG_RECORD_GW_CFG(level_, msg_) ESP_LOG_WRAPPER_TEST_CHECK_LOG_RECORD("gw_cfg", level_, msg_)

/*** Unit-Tests
 * *******************************************************************************************************/

TEST_F(TestJsonRuuvi, json_ruuvi_parse_network_cfg_eth_dhcp) // NOLINT
{
    cJSON *root = cJSON_CreateObject();
    ASSERT_NE(nullptr, root);
    cJSON_AddBoolToObject(root, "use_eth", true);
    cJSON_AddBoolToObject(root, "eth_dhcp", true);

    ruuvi_gateway_config_t gw_cfg           = { 0 };
    bool                   flag_network_cfg = false;
    ASSERT_TRUE(json_ruuvi_parse(root, &gw_cfg, &flag_network_cfg));
    cJSON_Delete(root);
    ASSERT_TRUE(flag_network_cfg);
    ASSERT_TRUE(gw_cfg.eth.use_eth);
    ASSERT_TRUE(gw_cfg.eth.eth_dhcp);
    ASSERT_EQ(string(""), gw_cfg.eth.eth_static_ip.buf);
    ASSERT_EQ(string(""), gw_cfg.eth.eth_netmask.buf);
    ASSERT_EQ(string(""), gw_cfg.eth.eth_gw.buf);
    ASSERT_EQ(string(""), gw_cfg.eth.eth_dns1.buf);
    ASSERT_EQ(string(""), gw_cfg.eth.eth_dns2.buf);

    TEST_CHECK_LOG_RECORD_HTTP_SERVER(ESP_LOG_DEBUG, "Got SETTINGS:");
    TEST_CHECK_LOG_RECORD_HTTP_SERVER(ESP_LOG_INFO, "Got SETTINGS:");
    TEST_CHECK_LOG_RECORD_HTTP_SERVER(ESP_LOG_INFO, "use_eth: 1");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "use_eth: 1");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "eth_dhcp: 1");
    TEST_CHECK_LOG_RECORD_HTTP_SERVER(ESP_LOG_INFO, "eth_dhcp: 1");
    ASSERT_TRUE(esp_log_wrapper_is_empty());
}

TEST_F(TestJsonRuuvi, json_ruuvi_parse_network_cfg_eth_static) // NOLINT
{
    cJSON *root = cJSON_CreateObject();
    ASSERT_NE(nullptr, root);
    cJSON_AddBoolToObject(root, "use_eth", true);
    cJSON_AddBoolToObject(root, "eth_dhcp", false);
    cJSON_AddStringToObject(root, "eth_static_ip", "192.168.1.1");
    cJSON_AddStringToObject(root, "eth_netmask", "255.255.255.0");
    cJSON_AddStringToObject(root, "eth_gw", "192.168.0.1");
    cJSON_AddStringToObject(root, "eth_dns1", "8.8.8.8");
    cJSON_AddStringToObject(root, "eth_dns2", "4.4.4.4");

    ruuvi_gateway_config_t gw_cfg           = { 0 };
    bool                   flag_network_cfg = false;
    ASSERT_TRUE(json_ruuvi_parse(root, &gw_cfg, &flag_network_cfg));
    cJSON_Delete(root);
    ASSERT_TRUE(flag_network_cfg);
    ASSERT_TRUE(gw_cfg.eth.use_eth);
    ASSERT_FALSE(gw_cfg.eth.eth_dhcp);
    ASSERT_EQ(string("192.168.1.1"), gw_cfg.eth.eth_static_ip.buf);
    ASSERT_EQ(string("255.255.255.0"), gw_cfg.eth.eth_netmask.buf);
    ASSERT_EQ(string("192.168.0.1"), gw_cfg.eth.eth_gw.buf);
    ASSERT_EQ(string("8.8.8.8"), gw_cfg.eth.eth_dns1.buf);
    ASSERT_EQ(string("4.4.4.4"), gw_cfg.eth.eth_dns2.buf);

    TEST_CHECK_LOG_RECORD_HTTP_SERVER(ESP_LOG_DEBUG, "Got SETTINGS:");
    TEST_CHECK_LOG_RECORD_HTTP_SERVER(ESP_LOG_INFO, "Got SETTINGS:");
    TEST_CHECK_LOG_RECORD_HTTP_SERVER(ESP_LOG_INFO, "use_eth: 1");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "use_eth: 1");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "eth_dhcp: 0");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "eth_static_ip: 192.168.1.1");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "eth_netmask: 255.255.255.0");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "eth_gw: 192.168.0.1");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "eth_dns1: 8.8.8.8");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "eth_dns2: 4.4.4.4");
    TEST_CHECK_LOG_RECORD_HTTP_SERVER(ESP_LOG_INFO, "eth_dhcp: 0");
    TEST_CHECK_LOG_RECORD_HTTP_SERVER(ESP_LOG_INFO, "eth_static_ip: 192.168.1.1");
    TEST_CHECK_LOG_RECORD_HTTP_SERVER(ESP_LOG_INFO, "eth_netmask: 255.255.255.0");
    TEST_CHECK_LOG_RECORD_HTTP_SERVER(ESP_LOG_INFO, "eth_gw: 192.168.0.1");
    TEST_CHECK_LOG_RECORD_HTTP_SERVER(ESP_LOG_INFO, "eth_dns1: 8.8.8.8");
    TEST_CHECK_LOG_RECORD_HTTP_SERVER(ESP_LOG_INFO, "eth_dns2: 4.4.4.4");
    ASSERT_TRUE(esp_log_wrapper_is_empty());
}

TEST_F(TestJsonRuuvi, json_ruuvi_parse_network_cfg_wifi) // NOLINT
{
    cJSON *root = cJSON_CreateObject();
    ASSERT_NE(nullptr, root);
    cJSON_AddBoolToObject(root, "use_eth", false);

    ruuvi_gateway_config_t gw_cfg           = { 0 };
    bool                   flag_network_cfg = false;
    ASSERT_TRUE(json_ruuvi_parse(root, &gw_cfg, &flag_network_cfg));
    cJSON_Delete(root);
    ASSERT_TRUE(flag_network_cfg);
    ASSERT_FALSE(gw_cfg.eth.use_eth);

    TEST_CHECK_LOG_RECORD_HTTP_SERVER(ESP_LOG_DEBUG, "Got SETTINGS:");
    TEST_CHECK_LOG_RECORD_HTTP_SERVER(ESP_LOG_INFO, "Got SETTINGS:");
    TEST_CHECK_LOG_RECORD_HTTP_SERVER(ESP_LOG_INFO, "use_eth: 0");
    ASSERT_TRUE(esp_log_wrapper_is_empty());
}

TEST_F(TestJsonRuuvi, json_ruuvi_parse) // NOLINT
{
    cJSON *root = cJSON_CreateObject();
    ASSERT_NE(nullptr, root);

    cJSON_AddBoolToObject(root, "use_mqtt", true);
    cJSON_AddStringToObject(root, "mqtt_transport", "TCP");
    cJSON_AddStringToObject(root, "mqtt_server", "mqtt.server.org");
    cJSON_AddStringToObject(root, "mqtt_prefix", "prefix");
    cJSON_AddStringToObject(root, "mqtt_client_id", "AA:BB:CC:DD:EE:FF");
    cJSON_AddNumberToObject(root, "mqtt_port", 1234);
    cJSON_AddStringToObject(root, "mqtt_user", "user123");
    cJSON_AddStringToObject(root, "mqtt_pass", "pass123");

    cJSON_AddBoolToObject(root, "use_http", false);
    cJSON_AddStringToObject(root, "http_url", "https://api.ruuvi.com:456/api");
    cJSON_AddStringToObject(root, "http_user", "user567");
    cJSON_AddStringToObject(root, "http_pass", "pass567");

    cJSON_AddBoolToObject(root, "use_http_stat", true);
    cJSON_AddStringToObject(root, "http_stat_url", "https://api.ruuvi.com:456/status");
    cJSON_AddStringToObject(root, "http_stat_user", "user678");
    cJSON_AddStringToObject(root, "http_stat_pass", "pass678");

    cJSON_AddStringToObject(root, "lan_auth_type", "lan_auth_ruuvi");
    cJSON_AddStringToObject(root, "lan_auth_user", "user1");
    cJSON_AddStringToObject(root, "lan_auth_pass", "qwe");
    cJSON_AddStringToObject(root, "lan_auth_api_key", "6kl/fd/c+3qvWm3Mhmwgh3BWNp+HDRQiLp/X0PuwG8Q=");

    cJSON_AddStringToObject(root, "auto_update_cycle", "regular");
    cJSON_AddNumberToObject(root, "auto_update_weekdays_bitmask", 127);
    cJSON_AddNumberToObject(root, "auto_update_interval_from", 0);
    cJSON_AddNumberToObject(root, "auto_update_interval_to", 24);
    cJSON_AddNumberToObject(root, "auto_update_tz_offset_hours", 3);

    cJSON_AddBoolToObject(root, "company_use_filtering", true);
    cJSON_AddNumberToObject(root, "company_id", 888);

    cJSON_AddStringToObject(root, "coordinates", "coord:123,456");

    cJSON_AddBoolToObject(root, "scan_coded_phy", true);
    cJSON_AddBoolToObject(root, "scan_1mbit_phy", true);
    cJSON_AddBoolToObject(root, "scan_extended_payload", true);
    cJSON_AddBoolToObject(root, "scan_channel_37", true);
    cJSON_AddBoolToObject(root, "scan_channel_38", true);
    cJSON_AddBoolToObject(root, "scan_channel_39", true);

    ruuvi_gateway_config_t gw_cfg           = { 0 };
    bool                   flag_network_cfg = false;
    ASSERT_TRUE(json_ruuvi_parse(root, &gw_cfg, &flag_network_cfg));
    cJSON_Delete(root);
    ASSERT_FALSE(flag_network_cfg);
    ASSERT_TRUE(gw_cfg.mqtt.use_mqtt);
    ASSERT_EQ(string("TCP"), gw_cfg.mqtt.mqtt_transport.buf);
    ASSERT_EQ(string("mqtt.server.org"), gw_cfg.mqtt.mqtt_server.buf);
    ASSERT_EQ(string("prefix"), gw_cfg.mqtt.mqtt_prefix.buf);
    ASSERT_EQ(string("AA:BB:CC:DD:EE:FF"), gw_cfg.mqtt.mqtt_client_id.buf);
    ASSERT_EQ(1234, gw_cfg.mqtt.mqtt_port);
    ASSERT_EQ(string("user123"), gw_cfg.mqtt.mqtt_user.buf);
    ASSERT_EQ(string("pass123"), gw_cfg.mqtt.mqtt_pass.buf);
    ASSERT_FALSE(gw_cfg.http.use_http);
    ASSERT_EQ(string("https://api.ruuvi.com:456/api"), gw_cfg.http.http_url.buf);
    ASSERT_EQ(string("user567"), gw_cfg.http.http_user.buf);
    ASSERT_EQ(string("pass567"), gw_cfg.http.http_pass.buf);
    ASSERT_TRUE(gw_cfg.http_stat.use_http_stat);
    ASSERT_EQ(string("https://api.ruuvi.com:456/status"), gw_cfg.http_stat.http_stat_url.buf);
    ASSERT_EQ(string("user678"), gw_cfg.http_stat.http_stat_user.buf);
    ASSERT_EQ(string("pass678"), gw_cfg.http_stat.http_stat_pass.buf);
    ASSERT_EQ(HTTP_SERVER_AUTH_TYPE_RUUVI, gw_cfg.lan_auth.lan_auth_type);
    ASSERT_EQ(string("user1"), gw_cfg.lan_auth.lan_auth_user.buf);
    ASSERT_EQ(string("qwe"), gw_cfg.lan_auth.lan_auth_pass.buf);
    ASSERT_EQ(string("6kl/fd/c+3qvWm3Mhmwgh3BWNp+HDRQiLp/X0PuwG8Q="), gw_cfg.lan_auth.lan_auth_api_key.buf);
    ASSERT_TRUE(gw_cfg.filter.company_use_filtering);
    ASSERT_EQ(888, gw_cfg.filter.company_id);
    ASSERT_EQ(string("coord:123,456"), gw_cfg.coordinates.buf);
    ASSERT_EQ(true, gw_cfg.scan.scan_coded_phy);
    ASSERT_EQ(true, gw_cfg.scan.scan_1mbit_phy);
    ASSERT_EQ(true, gw_cfg.scan.scan_extended_payload);
    ASSERT_EQ(true, gw_cfg.scan.scan_channel_37);
    ASSERT_EQ(true, gw_cfg.scan.scan_channel_38);
    ASSERT_EQ(true, gw_cfg.scan.scan_channel_39);

    TEST_CHECK_LOG_RECORD_HTTP_SERVER(ESP_LOG_DEBUG, "Got SETTINGS:");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "use_mqtt: 1");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "mqtt_transport: TCP");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "mqtt_server: mqtt.server.org");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "mqtt_port: 1234");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "mqtt_prefix: prefix");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "mqtt_client_id: AA:BB:CC:DD:EE:FF");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "mqtt_user: user123");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "mqtt_pass: pass123");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "use_http: 0");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "http_url: https://api.ruuvi.com:456/api");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "http_user: user567");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "http_pass: pass567");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "use_http_stat: 1");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "http_stat_url: https://api.ruuvi.com:456/status");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "http_stat_user: user678");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "http_stat_pass: pass678");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "lan_auth_type: lan_auth_ruuvi");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "lan_auth_user: user1");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "lan_auth_pass: qwe");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "lan_auth_api_key: 6kl/fd/c+3qvWm3Mhmwgh3BWNp+HDRQiLp/X0PuwG8Q=");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "auto_update_cycle: regular");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "auto_update_weekdays_bitmask: 127");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "auto_update_interval_from: 0");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "auto_update_interval_to: 24");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "auto_update_tz_offset_hours: 3");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "company_id: 888");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "company_use_filtering: 1");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "scan_coded_phy: 1");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "scan_1mbit_phy: 1");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "scan_extended_payload: 1");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "scan_channel_37: 1");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "scan_channel_38: 1");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "scan_channel_39: 1");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "coordinates: coord:123,456");
    ASSERT_TRUE(esp_log_wrapper_is_empty());
}

TEST_F(TestJsonRuuvi, json_ruuvi_parse_without_http_and_mqtt_pass) // NOLINT
{
    cJSON *root = cJSON_CreateObject();
    ASSERT_NE(nullptr, root);

    cJSON_AddBoolToObject(root, "use_mqtt", true);
    cJSON_AddStringToObject(root, "mqtt_transport", "TCP");
    cJSON_AddStringToObject(root, "mqtt_server", "mqtt.server.org");
    cJSON_AddStringToObject(root, "mqtt_prefix", "prefix");
    cJSON_AddStringToObject(root, "mqtt_client_id", "AA:BB:CC:DD:EE:FF");
    cJSON_AddNumberToObject(root, "mqtt_port", 1234);
    cJSON_AddStringToObject(root, "mqtt_user", "user123");

    cJSON_AddBoolToObject(root, "use_http", false);
    cJSON_AddStringToObject(root, "http_url", "https://api.ruuvi.com:456/api");
    cJSON_AddStringToObject(root, "http_user", "user567");

    cJSON_AddBoolToObject(root, "use_http_stat", true);
    cJSON_AddStringToObject(root, "http_stat_url", "https://api.ruuvi.com:456/status");
    cJSON_AddStringToObject(root, "http_stat_user", "user678");

    cJSON_AddStringToObject(root, "lan_auth_type", "lan_auth_ruuvi");
    cJSON_AddStringToObject(root, "lan_auth_user", "user1");
    cJSON_AddStringToObject(root, "lan_auth_pass", "qwe");
    cJSON_AddStringToObject(root, "lan_auth_api_key", "");

    cJSON_AddStringToObject(root, "auto_update_cycle", "beta");
    cJSON_AddNumberToObject(root, "auto_update_weekdays_bitmask", 126);
    cJSON_AddNumberToObject(root, "auto_update_interval_from", 1);
    cJSON_AddNumberToObject(root, "auto_update_interval_to", 23);
    cJSON_AddNumberToObject(root, "auto_update_tz_offset_hours", 7);

    cJSON_AddBoolToObject(root, "company_use_filtering", true);
    cJSON_AddNumberToObject(root, "company_id", 888);

    cJSON_AddStringToObject(root, "coordinates", "coord:123,456");

    cJSON_AddBoolToObject(root, "scan_coded_phy", true);
    cJSON_AddBoolToObject(root, "scan_1mbit_phy", true);
    cJSON_AddBoolToObject(root, "scan_extended_payload", true);
    cJSON_AddBoolToObject(root, "scan_channel_37", true);
    cJSON_AddBoolToObject(root, "scan_channel_38", true);
    cJSON_AddBoolToObject(root, "scan_channel_39", true);

    ruuvi_gateway_config_t gw_cfg           = { 0 };
    bool                   flag_network_cfg = false;
    snprintf(gw_cfg.http.http_pass.buf, sizeof(gw_cfg.http.http_pass.buf), "prev_http_pass");
    snprintf(gw_cfg.http_stat.http_stat_pass.buf, sizeof(gw_cfg.http_stat.http_stat_pass.buf), "prev_http_stat_pass");
    snprintf(gw_cfg.mqtt.mqtt_pass.buf, sizeof(gw_cfg.mqtt.mqtt_pass.buf), "prev_mqtt_pass");
    ASSERT_TRUE(json_ruuvi_parse(root, &gw_cfg, &flag_network_cfg));
    cJSON_Delete(root);
    ASSERT_FALSE(flag_network_cfg);
    ASSERT_TRUE(gw_cfg.mqtt.use_mqtt);
    ASSERT_EQ(string("TCP"), gw_cfg.mqtt.mqtt_transport.buf);
    ASSERT_EQ(string("mqtt.server.org"), gw_cfg.mqtt.mqtt_server.buf);
    ASSERT_EQ(string("prefix"), gw_cfg.mqtt.mqtt_prefix.buf);
    ASSERT_EQ(string("AA:BB:CC:DD:EE:FF"), gw_cfg.mqtt.mqtt_client_id.buf);
    ASSERT_EQ(1234, gw_cfg.mqtt.mqtt_port);
    ASSERT_EQ(string("user123"), gw_cfg.mqtt.mqtt_user.buf);
    ASSERT_EQ(string("prev_mqtt_pass"), gw_cfg.mqtt.mqtt_pass.buf);
    ASSERT_FALSE(gw_cfg.http.use_http);
    ASSERT_EQ(string("https://api.ruuvi.com:456/api"), gw_cfg.http.http_url.buf);
    ASSERT_EQ(string("user567"), gw_cfg.http.http_user.buf);
    ASSERT_EQ(string("prev_http_pass"), gw_cfg.http.http_pass.buf);
    ASSERT_TRUE(gw_cfg.http_stat.use_http_stat);
    ASSERT_EQ(string("https://api.ruuvi.com:456/status"), gw_cfg.http_stat.http_stat_url.buf);
    ASSERT_EQ(string("user678"), gw_cfg.http_stat.http_stat_user.buf);
    ASSERT_EQ(string("prev_http_stat_pass"), gw_cfg.http_stat.http_stat_pass.buf);
    ASSERT_EQ(HTTP_SERVER_AUTH_TYPE_RUUVI, gw_cfg.lan_auth.lan_auth_type);
    ASSERT_EQ(string("user1"), gw_cfg.lan_auth.lan_auth_user.buf);
    ASSERT_EQ(string("qwe"), gw_cfg.lan_auth.lan_auth_pass.buf);
    ASSERT_EQ(string(""), gw_cfg.lan_auth.lan_auth_api_key.buf);
    ASSERT_TRUE(gw_cfg.filter.company_use_filtering);
    ASSERT_EQ(888, gw_cfg.filter.company_id);
    ASSERT_EQ(string("coord:123,456"), gw_cfg.coordinates.buf);
    ASSERT_EQ(true, gw_cfg.scan.scan_coded_phy);
    ASSERT_EQ(true, gw_cfg.scan.scan_1mbit_phy);
    ASSERT_EQ(true, gw_cfg.scan.scan_extended_payload);
    ASSERT_EQ(true, gw_cfg.scan.scan_channel_37);
    ASSERT_EQ(true, gw_cfg.scan.scan_channel_38);
    ASSERT_EQ(true, gw_cfg.scan.scan_channel_39);

    TEST_CHECK_LOG_RECORD_HTTP_SERVER(ESP_LOG_DEBUG, "Got SETTINGS:");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "use_mqtt: 1");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "mqtt_transport: TCP");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "mqtt_server: mqtt.server.org");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "mqtt_port: 1234");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "mqtt_prefix: prefix");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "mqtt_client_id: AA:BB:CC:DD:EE:FF");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "mqtt_user: user123");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "mqtt_pass: not found");
    TEST_CHECK_LOG_RECORD_GW_CFG(
        ESP_LOG_INFO,
        "Can't find key 'mqtt_pass' in config-json, leave the previous value unchanged");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "use_http: 0");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "http_url: https://api.ruuvi.com:456/api");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "http_user: user567");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "http_pass: not found");
    TEST_CHECK_LOG_RECORD_GW_CFG(
        ESP_LOG_INFO,
        "Can't find key 'http_pass' in config-json, leave the previous value unchanged");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "use_http_stat: 1");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "http_stat_url: https://api.ruuvi.com:456/status");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "http_stat_user: user678");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "http_stat_pass: not found");
    TEST_CHECK_LOG_RECORD_GW_CFG(
        ESP_LOG_INFO,
        "Can't find key 'http_stat_pass' in config-json, leave the previous value unchanged");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "lan_auth_type: lan_auth_ruuvi");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "lan_auth_user: user1");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "lan_auth_pass: qwe");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "lan_auth_api_key: ");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "auto_update_cycle: beta");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "auto_update_weekdays_bitmask: 126");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "auto_update_interval_from: 1");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "auto_update_interval_to: 23");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "auto_update_tz_offset_hours: 7");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "company_id: 888");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "company_use_filtering: 1");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "scan_coded_phy: 1");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "scan_1mbit_phy: 1");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "scan_extended_payload: 1");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "scan_channel_37: 1");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "scan_channel_38: 1");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "scan_channel_39: 1");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "coordinates: coord:123,456");
    ASSERT_TRUE(esp_log_wrapper_is_empty());
}

TEST_F(TestJsonRuuvi, json_ruuvi_parse_mqtt_ssl) // NOLINT
{
    cJSON *root = cJSON_CreateObject();
    ASSERT_NE(nullptr, root);

    cJSON_AddBoolToObject(root, "use_mqtt", true);
    cJSON_AddStringToObject(root, "mqtt_transport", "SSL");
    cJSON_AddStringToObject(root, "mqtt_server", "mqtt.server.org");
    cJSON_AddStringToObject(root, "mqtt_prefix", "prefix");
    cJSON_AddStringToObject(root, "mqtt_client_id", "AA:BB:CC:DD:EE:FF");
    cJSON_AddNumberToObject(root, "mqtt_port", 8883);
    cJSON_AddStringToObject(root, "mqtt_user", "user123");

    cJSON_AddBoolToObject(root, "use_http", false);
    cJSON_AddStringToObject(root, "http_url", "https://api.ruuvi.com:456/api");
    cJSON_AddStringToObject(root, "http_user", "user567");
    cJSON_AddStringToObject(root, "http_pass", "pass567");

    cJSON_AddBoolToObject(root, "use_http_stat", true);
    cJSON_AddStringToObject(root, "http_stat_url", "https://api.ruuvi.com:456/status");
    cJSON_AddStringToObject(root, "http_stat_user", "user678");
    cJSON_AddStringToObject(root, "http_stat_pass", "pass678");

    cJSON_AddStringToObject(root, "lan_auth_type", "lan_auth_ruuvi");
    cJSON_AddStringToObject(root, "lan_auth_user", "user1");
    cJSON_AddStringToObject(root, "lan_auth_pass", "qwe");
    cJSON_AddStringToObject(root, "lan_auth_api_key", "");

    cJSON_AddStringToObject(root, "auto_update_cycle", "beta");
    cJSON_AddNumberToObject(root, "auto_update_weekdays_bitmask", 126);
    cJSON_AddNumberToObject(root, "auto_update_interval_from", 1);
    cJSON_AddNumberToObject(root, "auto_update_interval_to", 23);
    cJSON_AddNumberToObject(root, "auto_update_tz_offset_hours", 7);

    cJSON_AddBoolToObject(root, "company_use_filtering", true);
    cJSON_AddNumberToObject(root, "company_id", 888);

    cJSON_AddStringToObject(root, "coordinates", "coord:123,456");

    cJSON_AddBoolToObject(root, "scan_coded_phy", true);
    cJSON_AddBoolToObject(root, "scan_1mbit_phy", true);
    cJSON_AddBoolToObject(root, "scan_extended_payload", true);
    cJSON_AddBoolToObject(root, "scan_channel_37", true);
    cJSON_AddBoolToObject(root, "scan_channel_38", true);
    cJSON_AddBoolToObject(root, "scan_channel_39", true);

    ruuvi_gateway_config_t gw_cfg           = { 0 };
    bool                   flag_network_cfg = false;
    ASSERT_TRUE(json_ruuvi_parse(root, &gw_cfg, &flag_network_cfg));
    cJSON_Delete(root);
    ASSERT_FALSE(flag_network_cfg);
    ASSERT_TRUE(gw_cfg.mqtt.use_mqtt);
    ASSERT_EQ(string("SSL"), gw_cfg.mqtt.mqtt_transport.buf);
    ASSERT_EQ(string("mqtt.server.org"), gw_cfg.mqtt.mqtt_server.buf);
    ASSERT_EQ(string("prefix"), gw_cfg.mqtt.mqtt_prefix.buf);
    ASSERT_EQ(string("AA:BB:CC:DD:EE:FF"), gw_cfg.mqtt.mqtt_client_id.buf);
    ASSERT_EQ(8883, gw_cfg.mqtt.mqtt_port);
    ASSERT_EQ(string("user123"), gw_cfg.mqtt.mqtt_user.buf);
    ASSERT_EQ(string(""), gw_cfg.mqtt.mqtt_pass.buf);
    ASSERT_FALSE(gw_cfg.http.use_http);
    ASSERT_EQ(string("https://api.ruuvi.com:456/api"), gw_cfg.http.http_url.buf);
    ASSERT_EQ(string("user567"), gw_cfg.http.http_user.buf);
    ASSERT_EQ(string("pass567"), gw_cfg.http.http_pass.buf);
    ASSERT_TRUE(gw_cfg.http_stat.use_http_stat);
    ASSERT_EQ(string("https://api.ruuvi.com:456/status"), gw_cfg.http_stat.http_stat_url.buf);
    ASSERT_EQ(string("user678"), gw_cfg.http_stat.http_stat_user.buf);
    ASSERT_EQ(string("pass678"), gw_cfg.http_stat.http_stat_pass.buf);
    ASSERT_EQ(HTTP_SERVER_AUTH_TYPE_RUUVI, gw_cfg.lan_auth.lan_auth_type);
    ASSERT_EQ(string("user1"), gw_cfg.lan_auth.lan_auth_user.buf);
    ASSERT_EQ(string("qwe"), gw_cfg.lan_auth.lan_auth_pass.buf);
    ASSERT_EQ(string(""), gw_cfg.lan_auth.lan_auth_api_key.buf);
    ASSERT_TRUE(gw_cfg.filter.company_use_filtering);
    ASSERT_EQ(888, gw_cfg.filter.company_id);
    ASSERT_EQ(string("coord:123,456"), gw_cfg.coordinates.buf);
    ASSERT_EQ(true, gw_cfg.scan.scan_coded_phy);
    ASSERT_EQ(true, gw_cfg.scan.scan_1mbit_phy);
    ASSERT_EQ(true, gw_cfg.scan.scan_extended_payload);
    ASSERT_EQ(true, gw_cfg.scan.scan_channel_37);
    ASSERT_EQ(true, gw_cfg.scan.scan_channel_38);
    ASSERT_EQ(true, gw_cfg.scan.scan_channel_39);

    TEST_CHECK_LOG_RECORD_HTTP_SERVER(ESP_LOG_DEBUG, "Got SETTINGS:");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "use_mqtt: 1");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "mqtt_transport: SSL");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "mqtt_server: mqtt.server.org");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "mqtt_port: 8883");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "mqtt_prefix: prefix");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "mqtt_client_id: AA:BB:CC:DD:EE:FF");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "mqtt_user: user123");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "mqtt_pass: not found");
    TEST_CHECK_LOG_RECORD_GW_CFG(
        ESP_LOG_INFO,
        "Can't find key 'mqtt_pass' in config-json, leave the previous value unchanged");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "use_http: 0");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "http_url: https://api.ruuvi.com:456/api");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "http_user: user567");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "http_pass: pass567");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "use_http_stat: 1");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "http_stat_url: https://api.ruuvi.com:456/status");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "http_stat_user: user678");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "http_stat_pass: pass678");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "lan_auth_type: lan_auth_ruuvi");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "lan_auth_user: user1");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "lan_auth_pass: qwe");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "lan_auth_api_key: ");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "auto_update_cycle: beta");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "auto_update_weekdays_bitmask: 126");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "auto_update_interval_from: 1");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "auto_update_interval_to: 23");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "auto_update_tz_offset_hours: 7");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "company_id: 888");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "company_use_filtering: 1");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "scan_coded_phy: 1");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "scan_1mbit_phy: 1");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "scan_extended_payload: 1");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "scan_channel_37: 1");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "scan_channel_38: 1");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "scan_channel_39: 1");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "coordinates: coord:123,456");
    ASSERT_TRUE(esp_log_wrapper_is_empty());
}

TEST_F(TestJsonRuuvi, json_ruuvi_parse_mqtt_websocket) // NOLINT
{
    cJSON *root = cJSON_CreateObject();
    ASSERT_NE(nullptr, root);

    cJSON_AddBoolToObject(root, "use_mqtt", true);
    cJSON_AddStringToObject(root, "mqtt_transport", "WS");
    cJSON_AddStringToObject(root, "mqtt_server", "mqtt.server.org");
    cJSON_AddStringToObject(root, "mqtt_prefix", "prefix");
    cJSON_AddStringToObject(root, "mqtt_client_id", "AA:BB:CC:DD:EE:FF");
    cJSON_AddNumberToObject(root, "mqtt_port", 8080);
    cJSON_AddStringToObject(root, "mqtt_user", "user123");

    cJSON_AddBoolToObject(root, "use_http", false);
    cJSON_AddStringToObject(root, "http_url", "https://api.ruuvi.com:456/api");
    cJSON_AddStringToObject(root, "http_user", "user567");
    cJSON_AddStringToObject(root, "http_pass", "pass567");

    cJSON_AddBoolToObject(root, "use_http_stat", true);
    cJSON_AddStringToObject(root, "http_stat_url", "https://api.ruuvi.com:456/status");
    cJSON_AddStringToObject(root, "http_stat_user", "user678");
    cJSON_AddStringToObject(root, "http_stat_pass", "pass678");

    cJSON_AddStringToObject(root, "lan_auth_type", "lan_auth_ruuvi");
    cJSON_AddStringToObject(root, "lan_auth_user", "user1");
    cJSON_AddStringToObject(root, "lan_auth_pass", "qwe");
    cJSON_AddStringToObject(root, "lan_auth_api_key", "");

    cJSON_AddStringToObject(root, "auto_update_cycle", "beta");
    cJSON_AddNumberToObject(root, "auto_update_weekdays_bitmask", 126);
    cJSON_AddNumberToObject(root, "auto_update_interval_from", 1);
    cJSON_AddNumberToObject(root, "auto_update_interval_to", 23);
    cJSON_AddNumberToObject(root, "auto_update_tz_offset_hours", 7);

    cJSON_AddBoolToObject(root, "company_use_filtering", true);
    cJSON_AddNumberToObject(root, "company_id", 888);

    cJSON_AddStringToObject(root, "coordinates", "coord:123,456");

    cJSON_AddBoolToObject(root, "scan_coded_phy", true);
    cJSON_AddBoolToObject(root, "scan_1mbit_phy", true);
    cJSON_AddBoolToObject(root, "scan_extended_payload", true);
    cJSON_AddBoolToObject(root, "scan_channel_37", true);
    cJSON_AddBoolToObject(root, "scan_channel_38", true);
    cJSON_AddBoolToObject(root, "scan_channel_39", true);

    ruuvi_gateway_config_t gw_cfg           = { 0 };
    bool                   flag_network_cfg = false;
    ASSERT_TRUE(json_ruuvi_parse(root, &gw_cfg, &flag_network_cfg));
    cJSON_Delete(root);
    ASSERT_FALSE(flag_network_cfg);
    ASSERT_TRUE(gw_cfg.mqtt.use_mqtt);
    ASSERT_EQ(string("WS"), gw_cfg.mqtt.mqtt_transport.buf);
    ASSERT_EQ(string("mqtt.server.org"), gw_cfg.mqtt.mqtt_server.buf);
    ASSERT_EQ(string("prefix"), gw_cfg.mqtt.mqtt_prefix.buf);
    ASSERT_EQ(string("AA:BB:CC:DD:EE:FF"), gw_cfg.mqtt.mqtt_client_id.buf);
    ASSERT_EQ(8080, gw_cfg.mqtt.mqtt_port);
    ASSERT_EQ(string("user123"), gw_cfg.mqtt.mqtt_user.buf);
    ASSERT_EQ(string(""), gw_cfg.mqtt.mqtt_pass.buf);
    ASSERT_FALSE(gw_cfg.http.use_http);
    ASSERT_EQ(string("https://api.ruuvi.com:456/api"), gw_cfg.http.http_url.buf);
    ASSERT_EQ(string("user567"), gw_cfg.http.http_user.buf);
    ASSERT_EQ(string("pass567"), gw_cfg.http.http_pass.buf);
    ASSERT_TRUE(gw_cfg.http_stat.use_http_stat);
    ASSERT_EQ(string("https://api.ruuvi.com:456/status"), gw_cfg.http_stat.http_stat_url.buf);
    ASSERT_EQ(string("user678"), gw_cfg.http_stat.http_stat_user.buf);
    ASSERT_EQ(string("pass678"), gw_cfg.http_stat.http_stat_pass.buf);
    ASSERT_EQ(HTTP_SERVER_AUTH_TYPE_RUUVI, gw_cfg.lan_auth.lan_auth_type);
    ASSERT_EQ(string("user1"), gw_cfg.lan_auth.lan_auth_user.buf);
    ASSERT_EQ(string("qwe"), gw_cfg.lan_auth.lan_auth_pass.buf);
    ASSERT_EQ(string(""), gw_cfg.lan_auth.lan_auth_api_key.buf);
    ASSERT_TRUE(gw_cfg.filter.company_use_filtering);
    ASSERT_EQ(888, gw_cfg.filter.company_id);
    ASSERT_EQ(string("coord:123,456"), gw_cfg.coordinates.buf);
    ASSERT_EQ(true, gw_cfg.scan.scan_coded_phy);
    ASSERT_EQ(true, gw_cfg.scan.scan_1mbit_phy);
    ASSERT_EQ(true, gw_cfg.scan.scan_extended_payload);
    ASSERT_EQ(true, gw_cfg.scan.scan_channel_37);
    ASSERT_EQ(true, gw_cfg.scan.scan_channel_38);
    ASSERT_EQ(true, gw_cfg.scan.scan_channel_39);

    TEST_CHECK_LOG_RECORD_HTTP_SERVER(ESP_LOG_DEBUG, "Got SETTINGS:");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "use_mqtt: 1");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "mqtt_transport: WS");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "mqtt_server: mqtt.server.org");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "mqtt_port: 8080");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "mqtt_prefix: prefix");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "mqtt_client_id: AA:BB:CC:DD:EE:FF");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "mqtt_user: user123");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "mqtt_pass: not found");
    TEST_CHECK_LOG_RECORD_GW_CFG(
        ESP_LOG_INFO,
        "Can't find key 'mqtt_pass' in config-json, leave the previous value unchanged");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "use_http: 0");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "http_url: https://api.ruuvi.com:456/api");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "http_user: user567");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "http_pass: pass567");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "use_http_stat: 1");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "http_stat_url: https://api.ruuvi.com:456/status");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "http_stat_user: user678");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "http_stat_pass: pass678");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "lan_auth_type: lan_auth_ruuvi");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "lan_auth_user: user1");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "lan_auth_pass: qwe");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "lan_auth_api_key: ");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "auto_update_cycle: beta");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "auto_update_weekdays_bitmask: 126");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "auto_update_interval_from: 1");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "auto_update_interval_to: 23");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "auto_update_tz_offset_hours: 7");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "company_id: 888");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "company_use_filtering: 1");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "scan_coded_phy: 1");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "scan_1mbit_phy: 1");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "scan_extended_payload: 1");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "scan_channel_37: 1");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "scan_channel_38: 1");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "scan_channel_39: 1");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "coordinates: coord:123,456");
    ASSERT_TRUE(esp_log_wrapper_is_empty());
}

TEST_F(TestJsonRuuvi, json_ruuvi_parse_mqtt_secure_websocket) // NOLINT
{
    cJSON *root = cJSON_CreateObject();
    ASSERT_NE(nullptr, root);

    cJSON_AddBoolToObject(root, "use_mqtt", true);
    cJSON_AddStringToObject(root, "mqtt_transport", "WSS");
    cJSON_AddStringToObject(root, "mqtt_server", "mqtt.server.org");
    cJSON_AddStringToObject(root, "mqtt_prefix", "prefix");
    cJSON_AddStringToObject(root, "mqtt_client_id", "AA:BB:CC:DD:EE:FF");
    cJSON_AddNumberToObject(root, "mqtt_port", 8081);
    cJSON_AddStringToObject(root, "mqtt_user", "user123");

    cJSON_AddBoolToObject(root, "use_http", false);
    cJSON_AddStringToObject(root, "http_url", "https://api.ruuvi.com:456/api");
    cJSON_AddStringToObject(root, "http_user", "user567");
    cJSON_AddStringToObject(root, "http_pass", "pass567");

    cJSON_AddBoolToObject(root, "use_http_stat", true);
    cJSON_AddStringToObject(root, "http_stat_url", "https://api.ruuvi.com:456/status");
    cJSON_AddStringToObject(root, "http_stat_user", "user678");
    cJSON_AddStringToObject(root, "http_stat_pass", "pass678");

    cJSON_AddStringToObject(root, "lan_auth_type", "lan_auth_ruuvi");
    cJSON_AddStringToObject(root, "lan_auth_user", "user1");
    cJSON_AddStringToObject(root, "lan_auth_pass", "qwe");
    cJSON_AddStringToObject(root, "lan_auth_api_key", "");

    cJSON_AddStringToObject(root, "auto_update_cycle", "beta");
    cJSON_AddNumberToObject(root, "auto_update_weekdays_bitmask", 126);
    cJSON_AddNumberToObject(root, "auto_update_interval_from", 1);
    cJSON_AddNumberToObject(root, "auto_update_interval_to", 23);
    cJSON_AddNumberToObject(root, "auto_update_tz_offset_hours", 7);

    cJSON_AddBoolToObject(root, "company_use_filtering", true);
    cJSON_AddNumberToObject(root, "company_id", 888);

    cJSON_AddStringToObject(root, "coordinates", "coord:123,456");

    cJSON_AddBoolToObject(root, "scan_coded_phy", true);
    cJSON_AddBoolToObject(root, "scan_1mbit_phy", true);
    cJSON_AddBoolToObject(root, "scan_extended_payload", true);
    cJSON_AddBoolToObject(root, "scan_channel_37", true);
    cJSON_AddBoolToObject(root, "scan_channel_38", true);
    cJSON_AddBoolToObject(root, "scan_channel_39", true);

    ruuvi_gateway_config_t gw_cfg           = { 0 };
    bool                   flag_network_cfg = false;
    ASSERT_TRUE(json_ruuvi_parse(root, &gw_cfg, &flag_network_cfg));
    cJSON_Delete(root);
    ASSERT_FALSE(flag_network_cfg);
    ASSERT_TRUE(gw_cfg.mqtt.use_mqtt);
    ASSERT_EQ(string("WSS"), gw_cfg.mqtt.mqtt_transport.buf);
    ASSERT_EQ(string("mqtt.server.org"), gw_cfg.mqtt.mqtt_server.buf);
    ASSERT_EQ(string("prefix"), gw_cfg.mqtt.mqtt_prefix.buf);
    ASSERT_EQ(string("AA:BB:CC:DD:EE:FF"), gw_cfg.mqtt.mqtt_client_id.buf);
    ASSERT_EQ(8081, gw_cfg.mqtt.mqtt_port);
    ASSERT_EQ(string("user123"), gw_cfg.mqtt.mqtt_user.buf);
    ASSERT_EQ(string(""), gw_cfg.mqtt.mqtt_pass.buf);
    ASSERT_FALSE(gw_cfg.http.use_http);
    ASSERT_EQ(string("https://api.ruuvi.com:456/api"), gw_cfg.http.http_url.buf);
    ASSERT_EQ(string("user567"), gw_cfg.http.http_user.buf);
    ASSERT_EQ(string("pass567"), gw_cfg.http.http_pass.buf);
    ASSERT_TRUE(gw_cfg.http_stat.use_http_stat);
    ASSERT_EQ(string("https://api.ruuvi.com:456/status"), gw_cfg.http_stat.http_stat_url.buf);
    ASSERT_EQ(string("user678"), gw_cfg.http_stat.http_stat_user.buf);
    ASSERT_EQ(string("pass678"), gw_cfg.http_stat.http_stat_pass.buf);
    ASSERT_EQ(HTTP_SERVER_AUTH_TYPE_RUUVI, gw_cfg.lan_auth.lan_auth_type);
    ASSERT_EQ(string("user1"), gw_cfg.lan_auth.lan_auth_user.buf);
    ASSERT_EQ(string("qwe"), gw_cfg.lan_auth.lan_auth_pass.buf);
    ASSERT_EQ(string(""), gw_cfg.lan_auth.lan_auth_api_key.buf);
    ASSERT_TRUE(gw_cfg.filter.company_use_filtering);
    ASSERT_EQ(888, gw_cfg.filter.company_id);
    ASSERT_EQ(string("coord:123,456"), gw_cfg.coordinates.buf);
    ASSERT_EQ(true, gw_cfg.scan.scan_coded_phy);
    ASSERT_EQ(true, gw_cfg.scan.scan_1mbit_phy);
    ASSERT_EQ(true, gw_cfg.scan.scan_extended_payload);
    ASSERT_EQ(true, gw_cfg.scan.scan_channel_37);
    ASSERT_EQ(true, gw_cfg.scan.scan_channel_38);
    ASSERT_EQ(true, gw_cfg.scan.scan_channel_39);

    TEST_CHECK_LOG_RECORD_HTTP_SERVER(ESP_LOG_DEBUG, "Got SETTINGS:");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "use_mqtt: 1");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "mqtt_transport: WSS");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "mqtt_server: mqtt.server.org");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "mqtt_port: 8081");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "mqtt_prefix: prefix");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "mqtt_client_id: AA:BB:CC:DD:EE:FF");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "mqtt_user: user123");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "mqtt_pass: not found");
    TEST_CHECK_LOG_RECORD_GW_CFG(
        ESP_LOG_INFO,
        "Can't find key 'mqtt_pass' in config-json, leave the previous value unchanged");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "use_http: 0");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "http_url: https://api.ruuvi.com:456/api");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "http_user: user567");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "http_pass: pass567");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "use_http_stat: 1");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "http_stat_url: https://api.ruuvi.com:456/status");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "http_stat_user: user678");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "http_stat_pass: pass678");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "lan_auth_type: lan_auth_ruuvi");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "lan_auth_user: user1");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "lan_auth_pass: qwe");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "lan_auth_api_key: ");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "auto_update_cycle: beta");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "auto_update_weekdays_bitmask: 126");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "auto_update_interval_from: 1");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "auto_update_interval_to: 23");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "auto_update_tz_offset_hours: 7");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "company_id: 888");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "company_use_filtering: 1");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "scan_coded_phy: 1");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "scan_1mbit_phy: 1");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "scan_extended_payload: 1");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "scan_channel_37: 1");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "scan_channel_38: 1");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "scan_channel_39: 1");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "coordinates: coord:123,456");
    ASSERT_TRUE(esp_log_wrapper_is_empty());
}

TEST_F(TestJsonRuuvi, json_ruuvi_parse_http_body) // NOLINT
{
    ruuvi_gateway_config_t gw_cfg = { 0 };
    cJSON_Hooks            hooks  = {
        .malloc_fn = &os_malloc,
        .free_fn   = &os_free_internal,
    };
    g_pTestClass->m_malloc_fail_on_cnt = 0;
    cJSON_InitHooks(&hooks);
    bool flag_network_cfg = false;
    ASSERT_TRUE(
        json_ruuvi_parse_http_body(
            "{"
            "\"use_mqtt\":true,"
            "\"mqtt_transport\":\"TCP\","
            "\"mqtt_server\":\"test.mosquitto.org\","
            "\"mqtt_port\":1883,"
            "\"mqtt_prefix\":\"ruuvi/30:AE:A4:02:84:A4\","
            "\"mqtt_client_id\":\"30:AE:A4:02:84:A4\","
            "\"mqtt_user\":\"\","
            "\"mqtt_pass\":\"\","
            "\"use_http\":false,"
            "\"http_url\":\"" RUUVI_GATEWAY_HTTP_DEFAULT_URL "\","
            "\"http_user\":\"\","
            "\"http_pass\":\"\","
            "\"use_http_stat\":true,"
            "\"http_stat_url\":\"" RUUVI_GATEWAY_HTTP_STATUS_URL "\","
            "\"http_stat_user\":\"\","
            "\"http_stat_pass\":\"\","
            "\"lan_auth_type\":\"lan_auth_ruuvi\","
            "\"lan_auth_user\":\"user1\","
            "\"lan_auth_pass\":\"qwe\","
            "\"lan_auth_api_key\":\"\","
            "\"auto_update_cycle\":\"regular\","
            "\"auto_update_weekdays_bitmask\":127,"
            "\"auto_update_interval_from\":0,"
            "\"auto_update_interval_to\":24,"
            "\"auto_update_tz_offset_hours\":-3,"
            "\"company_use_filtering\":true,"
            "\"scan_coded_phy\":true,"
            "\"scan_1mbit_phy\":true,"
            "\"scan_extended_payload\":true,"
            "\"scan_channel_37\":true,"
            "\"scan_channel_38\":true,"
            "\"scan_channel_39\":true"
            "}",
            &gw_cfg,
            &flag_network_cfg));

    ASSERT_FALSE(flag_network_cfg);
    ASSERT_TRUE(gw_cfg.mqtt.use_mqtt);
    ASSERT_EQ(string("TCP"), gw_cfg.mqtt.mqtt_transport.buf);
    ASSERT_EQ(string("test.mosquitto.org"), gw_cfg.mqtt.mqtt_server.buf);
    ASSERT_EQ(string("ruuvi/30:AE:A4:02:84:A4"), gw_cfg.mqtt.mqtt_prefix.buf);
    ASSERT_EQ(string("30:AE:A4:02:84:A4"), gw_cfg.mqtt.mqtt_client_id.buf);
    ASSERT_EQ(1883, gw_cfg.mqtt.mqtt_port);
    ASSERT_EQ(string(""), gw_cfg.mqtt.mqtt_user.buf);
    ASSERT_EQ(string(""), gw_cfg.mqtt.mqtt_pass.buf);
    ASSERT_FALSE(gw_cfg.http.use_http);
    ASSERT_EQ(string(RUUVI_GATEWAY_HTTP_DEFAULT_URL), gw_cfg.http.http_url.buf);
    ASSERT_EQ(string(""), gw_cfg.http.http_user.buf);
    ASSERT_EQ(string(""), gw_cfg.http.http_pass.buf);
    ASSERT_TRUE(gw_cfg.http_stat.use_http_stat);
    ASSERT_EQ(string(RUUVI_GATEWAY_HTTP_STATUS_URL), gw_cfg.http_stat.http_stat_url.buf);
    ASSERT_EQ(string(""), gw_cfg.http_stat.http_stat_user.buf);
    ASSERT_EQ(string(""), gw_cfg.http_stat.http_stat_pass.buf);
    ASSERT_EQ(HTTP_SERVER_AUTH_TYPE_RUUVI, gw_cfg.lan_auth.lan_auth_type);
    ASSERT_EQ(string("user1"), gw_cfg.lan_auth.lan_auth_user.buf);
    ASSERT_EQ(string("qwe"), gw_cfg.lan_auth.lan_auth_pass.buf);
    ASSERT_EQ(string(""), gw_cfg.lan_auth.lan_auth_api_key.buf);
    ASSERT_TRUE(gw_cfg.filter.company_use_filtering);
    ASSERT_EQ(RUUVI_COMPANY_ID, gw_cfg.filter.company_id);
    ASSERT_EQ(string(""), gw_cfg.coordinates.buf);

    TEST_CHECK_LOG_RECORD_HTTP_SERVER(ESP_LOG_DEBUG, "Got SETTINGS:");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "use_mqtt: 1");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "mqtt_transport: TCP");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "mqtt_server: test.mosquitto.org");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "mqtt_port: 1883");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "mqtt_prefix: ruuvi/30:AE:A4:02:84:A4");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "mqtt_client_id: 30:AE:A4:02:84:A4");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "mqtt_user: ");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "mqtt_pass: ");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "use_http: 0");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "http_url: " RUUVI_GATEWAY_HTTP_DEFAULT_URL);
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "http_user: ");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "http_pass: ");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "use_http_stat: 1");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "http_stat_url: " RUUVI_GATEWAY_HTTP_STATUS_URL);
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "http_stat_user: ");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "http_stat_pass: ");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "lan_auth_type: lan_auth_ruuvi");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "lan_auth_user: user1");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "lan_auth_pass: qwe");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "lan_auth_api_key: ");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "auto_update_cycle: regular");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "auto_update_weekdays_bitmask: 127");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "auto_update_interval_from: 0");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "auto_update_interval_to: 24");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "auto_update_tz_offset_hours: -3");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "company_id: not found or invalid");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_WARN, "Can't find key 'company_id' in config-json, use default value: 0x0499");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "company_use_filtering: 1");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "scan_coded_phy: 1");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "scan_1mbit_phy: 1");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "scan_extended_payload: 1");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "scan_channel_37: 1");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "scan_channel_38: 1");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "scan_channel_39: 1");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "coordinates: not found");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_WARN, "Can't find key 'coordinates' in config-json");
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_TRUE(this->m_mem_alloc_trace.is_empty());
}

TEST_F(TestJsonRuuvi, json_ruuvi_parse_http_body_malloc_failed) // NOLINT
{
    ruuvi_gateway_config_t gw_cfg = { 0 };
    cJSON_Hooks            hooks  = {
        .malloc_fn = &os_malloc,
        .free_fn   = &os_free_internal,
    };
    g_pTestClass->m_malloc_fail_on_cnt = 1;
    cJSON_InitHooks(&hooks);
    bool flag_network_cfg = false;
    ASSERT_FALSE(json_ruuvi_parse_http_body(
        "{"
        "\"use_mqtt\":true,"
        "\"mqtt_transport\":\"TCP\","
        "\"mqtt_server\":\"test.mosquitto.org\","
        "\"mqtt_port\":1883,"
        "\"mqtt_prefix\":\"ruuvi/30:AE:A4:02:84:A4\","
        "\"mqtt_client_id\":\"30:AE:A4:02:84:A4\","
        "\"mqtt_user\":\"\","
        "\"mqtt_pass\":\"\","
        "\"use_http\":false,"
        "\"http_url\":\"" RUUVI_GATEWAY_HTTP_DEFAULT_URL
        "\","
        "\"http_user\":\"\","
        "\"http_pass\":\"\","
        "\"company_use_filtering\":true"
        "}",
        &gw_cfg,
        &flag_network_cfg));

    ASSERT_FALSE(flag_network_cfg);
    TEST_CHECK_LOG_RECORD_HTTP_SERVER(ESP_LOG_ERROR, "Failed to parse json or no memory");
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_TRUE(this->m_mem_alloc_trace.is_empty());
}
