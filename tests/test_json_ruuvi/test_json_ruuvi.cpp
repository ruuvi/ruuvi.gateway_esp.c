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
#include "os_mutex.h"
#include "os_task.h"
#include "lwip/ip4_addr.h"
#include "event_mgr.h"

using namespace std;

/*** Google-test class implementation
 * *********************************************************************************/

class TestJsonRuuvi;
static TestJsonRuuvi* g_pTestClass;

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
        gw_cfg_init(nullptr);

        esp_log_wrapper_clear();

        this->m_malloc_cnt         = 0;
        this->m_malloc_fail_on_cnt = 0;
    }

    void
    TearDown() override
    {
        gw_cfg_deinit();
        gw_cfg_default_deinit();
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
    (void)ph_mutex;
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

char*
esp_ip4addr_ntoa(const esp_ip4_addr_t* addr, char* buf, int buflen)
{
    return ip4addr_ntoa_r((ip4_addr_t*)addr, buf, buflen);
}

uint32_t
esp_ip4addr_aton(const char* addr)
{
    return ipaddr_addr(addr);
}

void
wifi_manager_cb_save_wifi_config_sta(const wifiman_config_sta_t* const p_cfg_sta)
{
}

void
event_mgr_notify(const event_mgr_ev_e event)
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
    const string http_body = string(
        "{\n"
        "\t\"use_eth\":\ttrue,\n"
        "\t\"eth_dhcp\":\ttrue\n"
        "}");

    gw_cfg_t gw_cfg = { 0 };
    gw_cfg_default_get(&gw_cfg);
    bool flag_network_cfg = false;
    ASSERT_TRUE(json_ruuvi_parse_http_body(http_body.c_str(), &gw_cfg, &flag_network_cfg));
    ASSERT_TRUE(flag_network_cfg);
    ASSERT_TRUE(gw_cfg.eth_cfg.use_eth);
    ASSERT_TRUE(gw_cfg.eth_cfg.eth_dhcp);
    ASSERT_EQ(string(""), gw_cfg.eth_cfg.eth_static_ip.buf);
    ASSERT_EQ(string(""), gw_cfg.eth_cfg.eth_netmask.buf);
    ASSERT_EQ(string(""), gw_cfg.eth_cfg.eth_gw.buf);
    ASSERT_EQ(string(""), gw_cfg.eth_cfg.eth_dns1.buf);
    ASSERT_EQ(string(""), gw_cfg.eth_cfg.eth_dns2.buf);

    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, "Gateway SETTINGS (via HTTP):");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "use_eth: 1");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "eth_dhcp: 1");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, "config: Use eth: yes");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, "config: eth: use DHCP: yes");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, "config: eth: static IP: ");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, "config: eth: netmask: ");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, "config: eth: GW: ");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, "config: eth: DNS1: ");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, "config: eth: DNS2: ");
    ASSERT_TRUE(esp_log_wrapper_is_empty());
}

TEST_F(TestJsonRuuvi, json_ruuvi_parse_network_cfg_eth_static) // NOLINT
{
    const string http_body = string(
        "{\n"
        "\t\"use_eth\":\ttrue,\n"
        "\t\"eth_dhcp\":\tfalse,\n"
        "\t\"eth_static_ip\":\t\"192.168.1.1\",\n"
        "\t\"eth_netmask\":\t\"255.255.255.0\",\n"
        "\t\"eth_gw\":\t\"192.168.0.1\",\n"
        "\t\"eth_dns1\":\t\"8.8.8.8\",\n"
        "\t\"eth_dns2\":\t\"4.4.4.4\"\n"
        "}");

    gw_cfg_t gw_cfg = { 0 };
    gw_cfg_default_get(&gw_cfg);
    bool flag_network_cfg = false;
    ASSERT_TRUE(json_ruuvi_parse_http_body(http_body.c_str(), &gw_cfg, &flag_network_cfg));
    ASSERT_TRUE(flag_network_cfg);
    ASSERT_TRUE(gw_cfg.eth_cfg.use_eth);
    ASSERT_FALSE(gw_cfg.eth_cfg.eth_dhcp);
    ASSERT_EQ(string("192.168.1.1"), gw_cfg.eth_cfg.eth_static_ip.buf);
    ASSERT_EQ(string("255.255.255.0"), gw_cfg.eth_cfg.eth_netmask.buf);
    ASSERT_EQ(string("192.168.0.1"), gw_cfg.eth_cfg.eth_gw.buf);
    ASSERT_EQ(string("8.8.8.8"), gw_cfg.eth_cfg.eth_dns1.buf);
    ASSERT_EQ(string("4.4.4.4"), gw_cfg.eth_cfg.eth_dns2.buf);

    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, "Gateway SETTINGS (via HTTP):");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "use_eth: 1");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "eth_dhcp: 0");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "eth_static_ip: 192.168.1.1");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "eth_netmask: 255.255.255.0");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "eth_gw: 192.168.0.1");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "eth_dns1: 8.8.8.8");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "eth_dns2: 4.4.4.4");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, "config: Use eth: yes");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, "config: eth: use DHCP: no");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, "config: eth: static IP: 192.168.1.1");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, "config: eth: netmask: 255.255.255.0");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, "config: eth: GW: 192.168.0.1");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, "config: eth: DNS1: 8.8.8.8");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, "config: eth: DNS2: 4.4.4.4");
    ASSERT_TRUE(esp_log_wrapper_is_empty());
}

TEST_F(TestJsonRuuvi, json_ruuvi_parse_network_cfg_wifi) // NOLINT
{
    const string http_body = string(
        "{\n"
        "\t\"use_eth\":\tfalse,\n"
        "\t\"wifi_ap_config\":\t{\n"
        "\t\t\"channel\":\t3\n"
        "\t}\n"
        "}");

    gw_cfg_t gw_cfg = { 0 };
    gw_cfg_default_get(&gw_cfg);
    bool flag_network_cfg = false;
    ASSERT_TRUE(json_ruuvi_parse_http_body(http_body.c_str(), &gw_cfg, &flag_network_cfg));
    ASSERT_TRUE(flag_network_cfg);
    ASSERT_FALSE(gw_cfg.eth_cfg.use_eth);

    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, "Gateway SETTINGS (via HTTP):");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "use_eth: 0");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, "config: Use eth: no");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, "config: eth: use DHCP: yes");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, "config: eth: static IP: ");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, "config: eth: netmask: ");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, "config: eth: GW: ");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, "config: eth: DNS1: ");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, "config: eth: DNS2: ");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_WARN, "Can't find key 'wifi_ap_config/password' in config-json");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "channel: 3");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, "config: wifi_ap_config: SSID: ");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, "config: wifi_ap_config: password: ");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, "config: wifi_ap_config: ssid_len: 0");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, "config: wifi_ap_config: channel: 3");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, "config: wifi_ap_config: auth_mode: OPEN");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, "config: wifi_ap_config: ssid_hidden: 0");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, "config: wifi_ap_config: max_connections: 4");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, "config: wifi_ap_config: beacon_interval: 100");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, "config: wifi_ap_settings: bandwidth: 20MHz");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, "config: wifi_ap_settings: IP: 10.10.0.1");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, "config: wifi_ap_settings: GW: 10.10.0.1");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, "config: wifi_ap_settings: Netmask: 255.255.255.0");
    ASSERT_TRUE(esp_log_wrapper_is_empty());
}

TEST_F(TestJsonRuuvi, json_ruuvi_parse) // NOLINT
{
    const string http_body = string(
        "{\n"
        "\t\"remote_cfg_use\":\tfalse,\n"
        "\t\"remote_cfg_url\":\t\"\",\n"
        "\t\"remote_cfg_auth_type\":\t\"no\",\n"
        "\t\"remote_cfg_refresh_interval_minutes\":\t0,\n"

        "\t\"use_http_ruuvi\":\tfalse,\n"
        "\t\"use_http\":\ttrue,\n"
        "\t\"http_url\":\t\"https://api.ruuvi.com:456/api\",\n"
        "\t\"http_data_format\":\t\"ruuvi\",\n"
        "\t\"http_auth\":\t\"basic\",\n"
        "\t\"http_user\":\t\"user567\",\n"
        "\t\"http_pass\":\t\"pass567\",\n"

        "\t\"use_http_stat\":\ttrue,\n"
        "\t\"http_stat_url\":\t\"https://api.ruuvi.com:456/status\",\n"
        "\t\"http_stat_user\":\t\"user678\",\n"
        "\t\"http_stat_pass\":\t\"pass678\",\n"

        "\t\"use_mqtt\":\ttrue,\n"
        "\t\"mqtt_disable_retained_messages\":\tfalse,\n"
        "\t\"mqtt_transport\":\t\"TCP\",\n"
        "\t\"mqtt_server\":\t\"mqtt.server.org\",\n"
        "\t\"mqtt_prefix\":\t\"prefix\",\n"
        "\t\"mqtt_client_id\":\t\"AA:BB:CC:DD:EE:FF\",\n"
        "\t\"mqtt_port\":\t1234,\n"
        "\t\"mqtt_user\":\t\"user123\",\n"
        "\t\"mqtt_pass\":\t\"pass123\",\n"

        "\t\"lan_auth_type\":\t\"lan_auth_ruuvi\",\n"
        "\t\"lan_auth_user\":\t\"user1\",\n"
        "\t\"lan_auth_pass\":\t\"qwe\",\n"
        "\t\"lan_auth_api_key\":\t\"6kl/fd/c+3qvWm3Mhmwgh3BWNp+HDRQiLp/X0PuwG8Q=\",\n"
        "\t\"lan_auth_api_key_rw\":\t\"KAv9oAT0c1XzbCF9N/Bnj2mgVR7R4QbBn/L3Wq5/zuI=\",\n"

        "\t\"auto_update_cycle\":\t\"regular\",\n"
        "\t\"auto_update_weekdays_bitmask\":\t127,\n"
        "\t\"auto_update_interval_from\":\t0,\n"
        "\t\"auto_update_interval_to\":\t24,\n"
        "\t\"auto_update_tz_offset_hours\":\t3,\n"

        "\t\"ntp_use\":\ttrue,\n"
        "\t\"ntp_use_dhcp\":\tfalse,\n"
        "\t\"ntp_server1\":\t\"time1.server.com\",\n"
        "\t\"ntp_server2\":\t\"time2.server.com\",\n"
        "\t\"ntp_server3\":\t\"time3.server.com\",\n"
        "\t\"ntp_server4\":\t\"time4.server.com\",\n"

        "\t\"company_use_filtering\":\ttrue,\n"
        "\t\"company_id\":\t888,\n"

        "\t\"coordinates\":\t\"coord:123,456\",\n"

        "\t\"scan_coded_phy\":\ttrue,\n"
        "\t\"scan_1mbit_phy\":\ttrue,\n"
        "\t\"scan_extended_payload\":\ttrue,\n"
        "\t\"scan_channel_37\":\ttrue,\n"
        "\t\"scan_channel_38\":\ttrue,\n"
        "\t\"scan_channel_39\":\ttrue\n"
        "}");

    gw_cfg_t gw_cfg = { 0 };
    gw_cfg_default_get(&gw_cfg);
    bool flag_network_cfg = false;
    ASSERT_TRUE(json_ruuvi_parse_http_body(http_body.c_str(), &gw_cfg, &flag_network_cfg));
    ASSERT_FALSE(flag_network_cfg);
    ASSERT_FALSE(gw_cfg.ruuvi_cfg.remote.use_remote_cfg);
    ASSERT_EQ(string(""), string(gw_cfg.ruuvi_cfg.remote.url.buf));
    ASSERT_EQ(GW_CFG_HTTP_AUTH_TYPE_NONE, gw_cfg.ruuvi_cfg.remote.auth_type);
    ASSERT_EQ(0, gw_cfg.ruuvi_cfg.remote.refresh_interval_minutes);
    ASSERT_TRUE(gw_cfg.ruuvi_cfg.mqtt.use_mqtt);
    ASSERT_FALSE(gw_cfg.ruuvi_cfg.mqtt.mqtt_disable_retained_messages);
    ASSERT_EQ(string("TCP"), gw_cfg.ruuvi_cfg.mqtt.mqtt_transport.buf);
    ASSERT_EQ(string("mqtt.server.org"), gw_cfg.ruuvi_cfg.mqtt.mqtt_server.buf);
    ASSERT_EQ(string("prefix"), gw_cfg.ruuvi_cfg.mqtt.mqtt_prefix.buf);
    ASSERT_EQ(string("AA:BB:CC:DD:EE:FF"), gw_cfg.ruuvi_cfg.mqtt.mqtt_client_id.buf);
    ASSERT_EQ(1234, gw_cfg.ruuvi_cfg.mqtt.mqtt_port);
    ASSERT_EQ(string("user123"), gw_cfg.ruuvi_cfg.mqtt.mqtt_user.buf);
    ASSERT_EQ(string("pass123"), gw_cfg.ruuvi_cfg.mqtt.mqtt_pass.buf);
    ASSERT_FALSE(gw_cfg.ruuvi_cfg.http.use_http_ruuvi);
    ASSERT_TRUE(gw_cfg.ruuvi_cfg.http.use_http);
    ASSERT_EQ(string("https://api.ruuvi.com:456/api"), gw_cfg.ruuvi_cfg.http.http_url.buf);
    ASSERT_EQ(GW_CFG_HTTP_DATA_FORMAT_RUUVI, gw_cfg.ruuvi_cfg.http.data_format);
    ASSERT_EQ(GW_CFG_HTTP_AUTH_TYPE_BASIC, gw_cfg.ruuvi_cfg.http.auth_type);
    ASSERT_EQ(string("user567"), gw_cfg.ruuvi_cfg.http.auth.auth_basic.user.buf);
    ASSERT_EQ(string("pass567"), gw_cfg.ruuvi_cfg.http.auth.auth_basic.password.buf);
    ASSERT_TRUE(gw_cfg.ruuvi_cfg.http_stat.use_http_stat);
    ASSERT_EQ(string("https://api.ruuvi.com:456/status"), gw_cfg.ruuvi_cfg.http_stat.http_stat_url.buf);
    ASSERT_EQ(string("user678"), gw_cfg.ruuvi_cfg.http_stat.http_stat_user.buf);
    ASSERT_EQ(string("pass678"), gw_cfg.ruuvi_cfg.http_stat.http_stat_pass.buf);
    ASSERT_EQ(HTTP_SERVER_AUTH_TYPE_RUUVI, gw_cfg.ruuvi_cfg.lan_auth.lan_auth_type);
    ASSERT_EQ(string("user1"), gw_cfg.ruuvi_cfg.lan_auth.lan_auth_user.buf);
    ASSERT_EQ(string("qwe"), gw_cfg.ruuvi_cfg.lan_auth.lan_auth_pass.buf);
    ASSERT_EQ(string("6kl/fd/c+3qvWm3Mhmwgh3BWNp+HDRQiLp/X0PuwG8Q="), gw_cfg.ruuvi_cfg.lan_auth.lan_auth_api_key.buf);
    ASSERT_EQ(true, gw_cfg.ruuvi_cfg.ntp.ntp_use);
    ASSERT_EQ(false, gw_cfg.ruuvi_cfg.ntp.ntp_use_dhcp);
    ASSERT_EQ(string("time1.server.com"), string(gw_cfg.ruuvi_cfg.ntp.ntp_server1.buf));
    ASSERT_EQ(string("time2.server.com"), string(gw_cfg.ruuvi_cfg.ntp.ntp_server2.buf));
    ASSERT_EQ(string("time3.server.com"), string(gw_cfg.ruuvi_cfg.ntp.ntp_server3.buf));
    ASSERT_EQ(string("time4.server.com"), string(gw_cfg.ruuvi_cfg.ntp.ntp_server4.buf));
    ASSERT_TRUE(gw_cfg.ruuvi_cfg.filter.company_use_filtering);
    ASSERT_EQ(888, gw_cfg.ruuvi_cfg.filter.company_id);
    ASSERT_EQ(string("coord:123,456"), gw_cfg.ruuvi_cfg.coordinates.buf);
    ASSERT_EQ(true, gw_cfg.ruuvi_cfg.scan.scan_coded_phy);
    ASSERT_EQ(true, gw_cfg.ruuvi_cfg.scan.scan_1mbit_phy);
    ASSERT_EQ(true, gw_cfg.ruuvi_cfg.scan.scan_extended_payload);
    ASSERT_EQ(true, gw_cfg.ruuvi_cfg.scan.scan_channel_37);
    ASSERT_EQ(true, gw_cfg.ruuvi_cfg.scan.scan_channel_38);
    ASSERT_EQ(true, gw_cfg.ruuvi_cfg.scan.scan_channel_39);

    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, "Gateway SETTINGS (via HTTP):");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "remote_cfg_use: 0");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "remote_cfg_url: ");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "remote_cfg_auth_type: no");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "remote_cfg_refresh_interval_minutes: 0");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "use_http_ruuvi: 0");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "use_http: 1");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "http_data_format: ruuvi");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "http_auth: basic");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "http_url: https://api.ruuvi.com:456/api");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "http_user: user567");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "http_pass: pass567");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "use_http_stat: 1");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "http_stat_url: https://api.ruuvi.com:456/status");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "http_stat_user: user678");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "http_stat_pass: pass678");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "use_mqtt: 1");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "mqtt_disable_retained_messages: 0");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "mqtt_transport: TCP");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "mqtt_server: mqtt.server.org");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "mqtt_port: 1234");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "mqtt_prefix: prefix");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "mqtt_client_id: AA:BB:CC:DD:EE:FF");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "mqtt_user: user123");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "mqtt_pass: pass123");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "lan_auth_type: lan_auth_ruuvi");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "lan_auth_user: user1");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "lan_auth_pass: qwe");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "lan_auth_api_key: 6kl/fd/c+3qvWm3Mhmwgh3BWNp+HDRQiLp/X0PuwG8Q=");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "lan_auth_api_key_rw: KAv9oAT0c1XzbCF9N/Bnj2mgVR7R4QbBn/L3Wq5/zuI=");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "auto_update_cycle: regular");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "auto_update_weekdays_bitmask: 127");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "auto_update_interval_from: 0");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "auto_update_interval_to: 24");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "auto_update_tz_offset_hours: 3");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "ntp_use: 1");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "ntp_use_dhcp: 0");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "ntp_server1: time1.server.com");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "ntp_server2: time2.server.com");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "ntp_server3: time3.server.com");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "ntp_server4: time4.server.com");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "company_id: 888");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "company_use_filtering: 1");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "scan_coded_phy: 1");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "scan_1mbit_phy: 1");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "scan_extended_payload: 1");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "scan_channel_37: 1");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "scan_channel_38: 1");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "scan_channel_39: 1");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "coordinates: coord:123,456");

    esp_log_wrapper_clear();
    ASSERT_TRUE(esp_log_wrapper_is_empty());
}

TEST_F(TestJsonRuuvi, json_ruuvi_parse_without_passwords) // NOLINT
{
    const string http_body = string(
        "{\n"
        "\t\"remote_cfg_use\":\tfalse,\n"
        "\t\"remote_cfg_url\":\t\"\",\n"
        "\t\"remote_cfg_auth_type\":\t\"basic\",\n"
        "\t\"remote_cfg_auth_basic_user\":\t\"user1\",\n"
        "\t\"remote_cfg_refresh_interval_minutes\":\t0,\n"

        "\t\"use_http_ruuvi\":\tfalse,\n"
        "\t\"use_http\":\ttrue,\n"
        "\t\"http_url\":\t\"https://api.ruuvi.com:456/api\",\n"
        "\t\"http_data_format\":\t\"ruuvi\",\n"
        "\t\"http_auth\":\t\"basic\",\n"
        "\t\"http_user\":\t\"user567\",\n"

        "\t\"use_http_stat\":\ttrue,\n"
        "\t\"http_stat_url\":\t\"https://api.ruuvi.com:456/status\",\n"
        "\t\"http_stat_user\":\t\"user678\",\n"

        "\t\"use_mqtt\":\ttrue,\n"
        "\t\"mqtt_disable_retained_messages\":\tfalse,\n"
        "\t\"mqtt_transport\":\t\"TCP\",\n"
        "\t\"mqtt_server\":\t\"mqtt.server.org\",\n"
        "\t\"mqtt_prefix\":\t\"prefix\",\n"
        "\t\"mqtt_client_id\":\t\"AA:BB:CC:DD:EE:FF\",\n"
        "\t\"mqtt_port\":\t1234,\n"
        "\t\"mqtt_user\":\t\"user123\",\n"

        "\t\"lan_auth_type\":\t\"lan_auth_ruuvi\",\n"
        "\t\"lan_auth_user\":\t\"user1\",\n"

        "\t\"auto_update_cycle\":\t\"beta\",\n"
        "\t\"auto_update_weekdays_bitmask\":\t126,\n"
        "\t\"auto_update_interval_from\":\t1,\n"
        "\t\"auto_update_interval_to\":\t23,\n"
        "\t\"auto_update_tz_offset_hours\":\t7,\n"

        "\t\"ntp_use\":\ttrue,\n"
        "\t\"ntp_use_dhcp\":\tfalse,\n"
        "\t\"ntp_server1\":\t\"time1.server.com\",\n"
        "\t\"ntp_server2\":\t\"time2.server.com\",\n"
        "\t\"ntp_server3\":\t\"time3.server.com\",\n"
        "\t\"ntp_server4\":\t\"time4.server.com\",\n"

        "\t\"company_use_filtering\":\ttrue,\n"
        "\t\"company_id\":\t888,\n"

        "\t\"coordinates\":\t\"coord:123,456\",\n"

        "\t\"scan_coded_phy\":\ttrue,\n"
        "\t\"scan_1mbit_phy\":\ttrue,\n"
        "\t\"scan_extended_payload\":\ttrue,\n"
        "\t\"scan_channel_37\":\ttrue,\n"
        "\t\"scan_channel_38\":\ttrue,\n"
        "\t\"scan_channel_39\":\ttrue\n"
        "}");

    gw_cfg_t gw_cfg = { 0 };
    gw_cfg_default_get(&gw_cfg);
    bool flag_network_cfg = false;
    (void)snprintf(
        gw_cfg.ruuvi_cfg.http.auth.auth_basic.password.buf,
        sizeof(gw_cfg.ruuvi_cfg.http.auth.auth_basic.password.buf),
        "prev_http_pass");
    (void)snprintf(
        gw_cfg.ruuvi_cfg.http_stat.http_stat_pass.buf,
        sizeof(gw_cfg.ruuvi_cfg.http_stat.http_stat_pass.buf),
        "prev_http_stat_pass");
    (void)snprintf(gw_cfg.ruuvi_cfg.mqtt.mqtt_pass.buf, sizeof(gw_cfg.ruuvi_cfg.mqtt.mqtt_pass.buf), "prev_mqtt_pass");
    gw_cfg.ruuvi_cfg.remote.auth_type = GW_CFG_HTTP_AUTH_TYPE_BASIC;
    (void)snprintf(
        gw_cfg.ruuvi_cfg.remote.auth.auth_basic.password.buf,
        sizeof(gw_cfg.ruuvi_cfg.remote.auth.auth_basic.password.buf),
        "prev_remote_cfg_pass");
    (void)snprintf(
        gw_cfg.ruuvi_cfg.lan_auth.lan_auth_pass.buf,
        sizeof(gw_cfg.ruuvi_cfg.lan_auth.lan_auth_pass.buf),
        "prev_lan_auth_pass");
    (void)snprintf(
        gw_cfg.ruuvi_cfg.lan_auth.lan_auth_api_key.buf,
        sizeof(gw_cfg.ruuvi_cfg.lan_auth.lan_auth_api_key.buf),
        "prev_lan_auth_api_key");
    (void)snprintf(
        gw_cfg.ruuvi_cfg.lan_auth.lan_auth_api_key_rw.buf,
        sizeof(gw_cfg.ruuvi_cfg.lan_auth.lan_auth_api_key_rw.buf),
        "prev_lan_auth_api_key_rw");

    ASSERT_TRUE(json_ruuvi_parse_http_body(http_body.c_str(), &gw_cfg, &flag_network_cfg));

    ASSERT_FALSE(flag_network_cfg);
    ASSERT_FALSE(gw_cfg.ruuvi_cfg.remote.use_remote_cfg);
    ASSERT_EQ(string(""), string(gw_cfg.ruuvi_cfg.remote.url.buf));
    ASSERT_EQ(GW_CFG_HTTP_AUTH_TYPE_BASIC, gw_cfg.ruuvi_cfg.remote.auth_type);
    ASSERT_EQ(string("user1"), gw_cfg.ruuvi_cfg.remote.auth.auth_basic.user.buf);
    ASSERT_EQ(string("prev_remote_cfg_pass"), gw_cfg.ruuvi_cfg.remote.auth.auth_basic.password.buf);
    ASSERT_EQ(0, gw_cfg.ruuvi_cfg.remote.refresh_interval_minutes);
    ASSERT_TRUE(gw_cfg.ruuvi_cfg.mqtt.use_mqtt);
    ASSERT_FALSE(gw_cfg.ruuvi_cfg.mqtt.mqtt_disable_retained_messages);
    ASSERT_EQ(string("TCP"), gw_cfg.ruuvi_cfg.mqtt.mqtt_transport.buf);
    ASSERT_EQ(string("mqtt.server.org"), gw_cfg.ruuvi_cfg.mqtt.mqtt_server.buf);
    ASSERT_EQ(string("prefix"), gw_cfg.ruuvi_cfg.mqtt.mqtt_prefix.buf);
    ASSERT_EQ(string("AA:BB:CC:DD:EE:FF"), gw_cfg.ruuvi_cfg.mqtt.mqtt_client_id.buf);
    ASSERT_EQ(1234, gw_cfg.ruuvi_cfg.mqtt.mqtt_port);
    ASSERT_EQ(string("user123"), gw_cfg.ruuvi_cfg.mqtt.mqtt_user.buf);
    ASSERT_EQ(string("prev_mqtt_pass"), gw_cfg.ruuvi_cfg.mqtt.mqtt_pass.buf);
    ASSERT_FALSE(gw_cfg.ruuvi_cfg.http.use_http_ruuvi);
    ASSERT_TRUE(gw_cfg.ruuvi_cfg.http.use_http);
    ASSERT_EQ(string("https://api.ruuvi.com:456/api"), gw_cfg.ruuvi_cfg.http.http_url.buf);
    ASSERT_EQ(GW_CFG_HTTP_DATA_FORMAT_RUUVI, gw_cfg.ruuvi_cfg.http.data_format);
    ASSERT_EQ(GW_CFG_HTTP_AUTH_TYPE_BASIC, gw_cfg.ruuvi_cfg.http.auth_type);
    ASSERT_EQ(string("user567"), gw_cfg.ruuvi_cfg.http.auth.auth_basic.user.buf);
    ASSERT_EQ(string("prev_http_pass"), gw_cfg.ruuvi_cfg.http.auth.auth_basic.password.buf);
    ASSERT_TRUE(gw_cfg.ruuvi_cfg.http_stat.use_http_stat);
    ASSERT_EQ(string("https://api.ruuvi.com:456/status"), gw_cfg.ruuvi_cfg.http_stat.http_stat_url.buf);
    ASSERT_EQ(string("user678"), gw_cfg.ruuvi_cfg.http_stat.http_stat_user.buf);
    ASSERT_EQ(string("prev_http_stat_pass"), gw_cfg.ruuvi_cfg.http_stat.http_stat_pass.buf);
    ASSERT_EQ(HTTP_SERVER_AUTH_TYPE_RUUVI, gw_cfg.ruuvi_cfg.lan_auth.lan_auth_type);
    ASSERT_EQ(string("user1"), gw_cfg.ruuvi_cfg.lan_auth.lan_auth_user.buf);
    ASSERT_EQ(string("prev_lan_auth_pass"), gw_cfg.ruuvi_cfg.lan_auth.lan_auth_pass.buf);
    ASSERT_EQ(string("prev_lan_auth_api_key"), gw_cfg.ruuvi_cfg.lan_auth.lan_auth_api_key.buf);
    ASSERT_EQ(string("prev_lan_auth_api_key_rw"), gw_cfg.ruuvi_cfg.lan_auth.lan_auth_api_key_rw.buf);
    ASSERT_EQ(true, gw_cfg.ruuvi_cfg.ntp.ntp_use);
    ASSERT_EQ(false, gw_cfg.ruuvi_cfg.ntp.ntp_use_dhcp);
    ASSERT_EQ(string("time1.server.com"), string(gw_cfg.ruuvi_cfg.ntp.ntp_server1.buf));
    ASSERT_EQ(string("time2.server.com"), string(gw_cfg.ruuvi_cfg.ntp.ntp_server2.buf));
    ASSERT_EQ(string("time3.server.com"), string(gw_cfg.ruuvi_cfg.ntp.ntp_server3.buf));
    ASSERT_EQ(string("time4.server.com"), string(gw_cfg.ruuvi_cfg.ntp.ntp_server4.buf));
    ASSERT_TRUE(gw_cfg.ruuvi_cfg.filter.company_use_filtering);
    ASSERT_EQ(888, gw_cfg.ruuvi_cfg.filter.company_id);
    ASSERT_EQ(string("coord:123,456"), gw_cfg.ruuvi_cfg.coordinates.buf);
    ASSERT_EQ(true, gw_cfg.ruuvi_cfg.scan.scan_coded_phy);
    ASSERT_EQ(true, gw_cfg.ruuvi_cfg.scan.scan_1mbit_phy);
    ASSERT_EQ(true, gw_cfg.ruuvi_cfg.scan.scan_extended_payload);
    ASSERT_EQ(true, gw_cfg.ruuvi_cfg.scan.scan_channel_37);
    ASSERT_EQ(true, gw_cfg.ruuvi_cfg.scan.scan_channel_38);
    ASSERT_EQ(true, gw_cfg.ruuvi_cfg.scan.scan_channel_39);

    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, "Gateway SETTINGS (via HTTP):");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "remote_cfg_use: 0");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "remote_cfg_url: ");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "remote_cfg_auth_type: basic");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "remote_cfg_auth_basic_user: user1");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "remote_cfg_auth_basic_pass: not found");
    TEST_CHECK_LOG_RECORD_GW_CFG(
        ESP_LOG_INFO,
        "Can't find key 'remote_cfg_auth_basic_pass' in config-json, leave the previous value unchanged");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "remote_cfg_refresh_interval_minutes: 0");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "use_http_ruuvi: 0");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "use_http: 1");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "http_data_format: ruuvi");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "http_auth: basic");
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
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "use_mqtt: 1");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "mqtt_disable_retained_messages: 0");
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
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "lan_auth_type: lan_auth_ruuvi");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "lan_auth_user: user1");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "lan_auth_pass: not found");
    TEST_CHECK_LOG_RECORD_GW_CFG(
        ESP_LOG_INFO,
        "Can't find key 'lan_auth_pass' in config-json, leave the previous value unchanged");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "lan_auth_api_key: not found");
    TEST_CHECK_LOG_RECORD_GW_CFG(
        ESP_LOG_INFO,
        "Can't find key 'lan_auth_api_key' in config-json, leave the previous value unchanged");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "lan_auth_api_key_rw: not found");
    TEST_CHECK_LOG_RECORD_GW_CFG(
        ESP_LOG_INFO,
        "Can't find key 'lan_auth_api_key_rw' in config-json, leave the previous value unchanged");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "auto_update_cycle: beta");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "auto_update_weekdays_bitmask: 126");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "auto_update_interval_from: 1");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "auto_update_interval_to: 23");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "auto_update_tz_offset_hours: 7");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "ntp_use: 1");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "ntp_use_dhcp: 0");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "ntp_server1: time1.server.com");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "ntp_server2: time2.server.com");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "ntp_server3: time3.server.com");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "ntp_server4: time4.server.com");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "company_id: 888");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "company_use_filtering: 1");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "scan_coded_phy: 1");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "scan_1mbit_phy: 1");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "scan_extended_payload: 1");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "scan_channel_37: 1");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "scan_channel_38: 1");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "scan_channel_39: 1");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "coordinates: coord:123,456");
    esp_log_wrapper_clear();
    ASSERT_TRUE(esp_log_wrapper_is_empty());
}

TEST_F(TestJsonRuuvi, json_ruuvi_parse_without_passwords_and_remote_cfg_auth_changed) // NOLINT
{
    const string http_body = string(
        "{\n"
        "\t\"remote_cfg_use\":\tfalse,\n"
        "\t\"remote_cfg_url\":\t\"\",\n"
        "\t\"remote_cfg_auth_type\":\t\"basic\",\n"
        "\t\"remote_cfg_auth_basic_user\":\t\"user1\",\n"
        "\t\"remote_cfg_refresh_interval_minutes\":\t0,\n"

        "\t\"use_http_ruuvi\":\tfalse,\n"
        "\t\"use_http\":\tfalse,\n"
        "\t\"http_url\":\t\"https://api.ruuvi.com:456/api\",\n"
        "\t\"http_data_format\":\t\"ruuvi\",\n"
        "\t\"http_auth\":\t\"none\",\n"

        "\t\"use_http_stat\":\ttrue,\n"
        "\t\"http_stat_url\":\t\"https://api.ruuvi.com:456/status\",\n"
        "\t\"http_stat_user\":\t\"user678\",\n"

        "\t\"use_mqtt\":\ttrue,\n"
        "\t\"mqtt_disable_retained_messages\":\tfalse,\n"
        "\t\"mqtt_transport\":\t\"TCP\",\n"
        "\t\"mqtt_server\":\t\"mqtt.server.org\",\n"
        "\t\"mqtt_prefix\":\t\"prefix\",\n"
        "\t\"mqtt_client_id\":\t\"AA:BB:CC:DD:EE:FF\",\n"
        "\t\"mqtt_port\":\t1234,\n"
        "\t\"mqtt_user\":\t\"user123\",\n"

        "\t\"lan_auth_type\":\t\"lan_auth_ruuvi\",\n"
        "\t\"lan_auth_user\":\t\"user1\",\n"

        "\t\"auto_update_cycle\":\t\"beta\",\n"
        "\t\"auto_update_weekdays_bitmask\":\t126,\n"
        "\t\"auto_update_interval_from\":\t1,\n"
        "\t\"auto_update_interval_to\":\t23,\n"
        "\t\"auto_update_tz_offset_hours\":\t7,\n"

        "\t\"ntp_use\":\ttrue,\n"
        "\t\"ntp_use_dhcp\":\tfalse,\n"
        "\t\"ntp_server1\":\t\"time1.server.com\",\n"
        "\t\"ntp_server2\":\t\"time2.server.com\",\n"
        "\t\"ntp_server3\":\t\"time3.server.com\",\n"
        "\t\"ntp_server4\":\t\"time4.server.com\",\n"

        "\t\"company_use_filtering\":\ttrue,\n"
        "\t\"company_id\":\t888,\n"

        "\t\"coordinates\":\t\"coord:123,456\",\n"

        "\t\"scan_coded_phy\":\ttrue,\n"
        "\t\"scan_1mbit_phy\":\ttrue,\n"
        "\t\"scan_extended_payload\":\ttrue,\n"
        "\t\"scan_channel_37\":\ttrue,\n"
        "\t\"scan_channel_38\":\ttrue,\n"
        "\t\"scan_channel_39\":\ttrue\n"
        "}");

    gw_cfg_t gw_cfg = { 0 };
    gw_cfg_default_get(&gw_cfg);
    bool flag_network_cfg = false;
    (void)snprintf(
        gw_cfg.ruuvi_cfg.http.auth.auth_basic.password.buf,
        sizeof(gw_cfg.ruuvi_cfg.http.auth.auth_basic.password.buf),
        "prev_http_pass");
    (void)snprintf(
        gw_cfg.ruuvi_cfg.http_stat.http_stat_pass.buf,
        sizeof(gw_cfg.ruuvi_cfg.http_stat.http_stat_pass.buf),
        "prev_http_stat_pass");
    (void)snprintf(gw_cfg.ruuvi_cfg.mqtt.mqtt_pass.buf, sizeof(gw_cfg.ruuvi_cfg.mqtt.mqtt_pass.buf), "prev_mqtt_pass");
    gw_cfg.ruuvi_cfg.remote.auth_type = GW_CFG_HTTP_AUTH_TYPE_BEARER;
    (void)snprintf(
        gw_cfg.ruuvi_cfg.remote.auth.auth_bearer.token.buf,
        sizeof(gw_cfg.ruuvi_cfg.remote.auth.auth_bearer.token.buf),
        "prev_remote_cfg_bearer_token");
    (void)snprintf(
        gw_cfg.ruuvi_cfg.lan_auth.lan_auth_pass.buf,
        sizeof(gw_cfg.ruuvi_cfg.lan_auth.lan_auth_pass.buf),
        "prev_lan_auth_pass");
    (void)snprintf(
        gw_cfg.ruuvi_cfg.lan_auth.lan_auth_api_key.buf,
        sizeof(gw_cfg.ruuvi_cfg.lan_auth.lan_auth_api_key.buf),
        "prev_lan_auth_api_key");
    (void)snprintf(
        gw_cfg.ruuvi_cfg.lan_auth.lan_auth_api_key_rw.buf,
        sizeof(gw_cfg.ruuvi_cfg.lan_auth.lan_auth_api_key_rw.buf),
        "prev_lan_auth_api_key_rw");

    ASSERT_TRUE(json_ruuvi_parse_http_body(http_body.c_str(), &gw_cfg, &flag_network_cfg));

    ASSERT_FALSE(flag_network_cfg);
    ASSERT_FALSE(gw_cfg.ruuvi_cfg.remote.use_remote_cfg);
    ASSERT_EQ(string(""), string(gw_cfg.ruuvi_cfg.remote.url.buf));
    ASSERT_EQ(GW_CFG_HTTP_AUTH_TYPE_BASIC, gw_cfg.ruuvi_cfg.remote.auth_type);
    ASSERT_EQ(string("user1"), gw_cfg.ruuvi_cfg.remote.auth.auth_basic.user.buf);
    ASSERT_EQ(string(""), gw_cfg.ruuvi_cfg.remote.auth.auth_basic.password.buf);
    ASSERT_EQ(0, gw_cfg.ruuvi_cfg.remote.refresh_interval_minutes);
    ASSERT_TRUE(gw_cfg.ruuvi_cfg.mqtt.use_mqtt);
    ASSERT_FALSE(gw_cfg.ruuvi_cfg.mqtt.mqtt_disable_retained_messages);
    ASSERT_EQ(string("TCP"), gw_cfg.ruuvi_cfg.mqtt.mqtt_transport.buf);
    ASSERT_EQ(string("mqtt.server.org"), gw_cfg.ruuvi_cfg.mqtt.mqtt_server.buf);
    ASSERT_EQ(string("prefix"), gw_cfg.ruuvi_cfg.mqtt.mqtt_prefix.buf);
    ASSERT_EQ(string("AA:BB:CC:DD:EE:FF"), gw_cfg.ruuvi_cfg.mqtt.mqtt_client_id.buf);
    ASSERT_EQ(1234, gw_cfg.ruuvi_cfg.mqtt.mqtt_port);
    ASSERT_EQ(string("user123"), gw_cfg.ruuvi_cfg.mqtt.mqtt_user.buf);
    ASSERT_EQ(string("prev_mqtt_pass"), gw_cfg.ruuvi_cfg.mqtt.mqtt_pass.buf);

    ASSERT_FALSE(gw_cfg.ruuvi_cfg.http.use_http_ruuvi);
    ASSERT_FALSE(gw_cfg.ruuvi_cfg.http.use_http);
    ASSERT_EQ(string("https://api.ruuvi.com:456/api"), gw_cfg.ruuvi_cfg.http.http_url.buf);
    ASSERT_EQ(GW_CFG_HTTP_DATA_FORMAT_RUUVI, gw_cfg.ruuvi_cfg.http.data_format);
    ASSERT_EQ(GW_CFG_HTTP_AUTH_TYPE_NONE, gw_cfg.ruuvi_cfg.http.auth_type);

    ASSERT_TRUE(gw_cfg.ruuvi_cfg.http_stat.use_http_stat);
    ASSERT_EQ(string("https://api.ruuvi.com:456/status"), gw_cfg.ruuvi_cfg.http_stat.http_stat_url.buf);
    ASSERT_EQ(string("user678"), gw_cfg.ruuvi_cfg.http_stat.http_stat_user.buf);
    ASSERT_EQ(string("prev_http_stat_pass"), gw_cfg.ruuvi_cfg.http_stat.http_stat_pass.buf);
    ASSERT_EQ(HTTP_SERVER_AUTH_TYPE_RUUVI, gw_cfg.ruuvi_cfg.lan_auth.lan_auth_type);
    ASSERT_EQ(string("user1"), gw_cfg.ruuvi_cfg.lan_auth.lan_auth_user.buf);
    ASSERT_EQ(string("prev_lan_auth_pass"), gw_cfg.ruuvi_cfg.lan_auth.lan_auth_pass.buf);
    ASSERT_EQ(string("prev_lan_auth_api_key"), gw_cfg.ruuvi_cfg.lan_auth.lan_auth_api_key.buf);
    ASSERT_EQ(string("prev_lan_auth_api_key_rw"), gw_cfg.ruuvi_cfg.lan_auth.lan_auth_api_key_rw.buf);
    ASSERT_EQ(true, gw_cfg.ruuvi_cfg.ntp.ntp_use);
    ASSERT_EQ(false, gw_cfg.ruuvi_cfg.ntp.ntp_use_dhcp);
    ASSERT_EQ(string("time1.server.com"), string(gw_cfg.ruuvi_cfg.ntp.ntp_server1.buf));
    ASSERT_EQ(string("time2.server.com"), string(gw_cfg.ruuvi_cfg.ntp.ntp_server2.buf));
    ASSERT_EQ(string("time3.server.com"), string(gw_cfg.ruuvi_cfg.ntp.ntp_server3.buf));
    ASSERT_EQ(string("time4.server.com"), string(gw_cfg.ruuvi_cfg.ntp.ntp_server4.buf));
    ASSERT_TRUE(gw_cfg.ruuvi_cfg.filter.company_use_filtering);
    ASSERT_EQ(888, gw_cfg.ruuvi_cfg.filter.company_id);
    ASSERT_EQ(string("coord:123,456"), gw_cfg.ruuvi_cfg.coordinates.buf);
    ASSERT_EQ(true, gw_cfg.ruuvi_cfg.scan.scan_coded_phy);
    ASSERT_EQ(true, gw_cfg.ruuvi_cfg.scan.scan_1mbit_phy);
    ASSERT_EQ(true, gw_cfg.ruuvi_cfg.scan.scan_extended_payload);
    ASSERT_EQ(true, gw_cfg.ruuvi_cfg.scan.scan_channel_37);
    ASSERT_EQ(true, gw_cfg.ruuvi_cfg.scan.scan_channel_38);
    ASSERT_EQ(true, gw_cfg.ruuvi_cfg.scan.scan_channel_39);

    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, "Gateway SETTINGS (via HTTP):");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "remote_cfg_use: 0");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "remote_cfg_url: ");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "remote_cfg_auth_type: basic");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, "Key 'remote_cfg_auth_type' was changed, clear saved auth info");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "remote_cfg_auth_basic_user: user1");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "remote_cfg_auth_basic_pass: not found");
    TEST_CHECK_LOG_RECORD_GW_CFG(
        ESP_LOG_INFO,
        "Can't find key 'remote_cfg_auth_basic_pass' in config-json, leave the previous value unchanged");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "remote_cfg_refresh_interval_minutes: 0");

    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "use_http_ruuvi: 0");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "use_http: 0");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "http_data_format: ruuvi");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "http_auth: none");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "http_url: https://api.ruuvi.com:456/api");

    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "use_http_stat: 1");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "http_stat_url: https://api.ruuvi.com:456/status");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "http_stat_user: user678");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "http_stat_pass: not found");
    TEST_CHECK_LOG_RECORD_GW_CFG(
        ESP_LOG_INFO,
        "Can't find key 'http_stat_pass' in config-json, leave the previous value unchanged");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "use_mqtt: 1");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "mqtt_disable_retained_messages: 0");
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
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "lan_auth_type: lan_auth_ruuvi");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "lan_auth_user: user1");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "lan_auth_pass: not found");
    TEST_CHECK_LOG_RECORD_GW_CFG(
        ESP_LOG_INFO,
        "Can't find key 'lan_auth_pass' in config-json, leave the previous value unchanged");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "lan_auth_api_key: not found");
    TEST_CHECK_LOG_RECORD_GW_CFG(
        ESP_LOG_INFO,
        "Can't find key 'lan_auth_api_key' in config-json, leave the previous value unchanged");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "lan_auth_api_key_rw: not found");
    TEST_CHECK_LOG_RECORD_GW_CFG(
        ESP_LOG_INFO,
        "Can't find key 'lan_auth_api_key_rw' in config-json, leave the previous value unchanged");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "auto_update_cycle: beta");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "auto_update_weekdays_bitmask: 126");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "auto_update_interval_from: 1");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "auto_update_interval_to: 23");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "auto_update_tz_offset_hours: 7");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "ntp_use: 1");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "ntp_use_dhcp: 0");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "ntp_server1: time1.server.com");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "ntp_server2: time2.server.com");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "ntp_server3: time3.server.com");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "ntp_server4: time4.server.com");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "company_id: 888");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "company_use_filtering: 1");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "scan_coded_phy: 1");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "scan_1mbit_phy: 1");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "scan_extended_payload: 1");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "scan_channel_37: 1");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "scan_channel_38: 1");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "scan_channel_39: 1");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "coordinates: coord:123,456");
    esp_log_wrapper_clear();
    ASSERT_TRUE(esp_log_wrapper_is_empty());
}

TEST_F(TestJsonRuuvi, json_ruuvi_parse_mqtt_empty_prefix_and_client_id) // NOLINT
{
    const string http_body = string(
        "{\n"
        "\t\"remote_cfg_use\":\tfalse,\n"
        "\t\"remote_cfg_url\":\t\"\",\n"
        "\t\"remote_cfg_auth_type\":\t\"no\",\n"
        "\t\"remote_cfg_refresh_interval_minutes\":\t0,\n"

        "\t\"use_http_ruuvi\":\tfalse,\n"
        "\t\"use_http\":\tfalse,\n"
        "\t\"http_url\":\t\"https://api.ruuvi.com:456/api\",\n"
        "\t\"http_data_format\":\t\"ruuvi\",\n"
        "\t\"http_auth\":\t\"none\",\n"

        "\t\"use_http_stat\":\ttrue,\n"
        "\t\"http_stat_url\":\t\"https://api.ruuvi.com:456/status\",\n"
        "\t\"http_stat_user\":\t\"user678\",\n"
        "\t\"http_stat_pass\":\t\"pass678\",\n"

        "\t\"use_mqtt\":\ttrue,\n"
        "\t\"mqtt_disable_retained_messages\":\tfalse,\n"
        "\t\"mqtt_transport\":\t\"TCP\",\n"
        "\t\"mqtt_server\":\t\"mqtt.server.org\",\n"
        "\t\"mqtt_prefix\":\t\"\",\n"
        "\t\"mqtt_client_id\":\t\"\",\n"
        "\t\"mqtt_port\":\t8883,\n"
        "\t\"mqtt_user\":\t\"user123\",\n"

        "\t\"lan_auth_type\":\t\"lan_auth_ruuvi\",\n"
        "\t\"lan_auth_user\":\t\"user1\",\n"
        "\t\"lan_auth_pass\":\t\"qwe\",\n"
        "\t\"lan_auth_api_key\":\t\"\",\n"
        "\t\"lan_auth_api_key_rw\":\t\"\",\n"

        "\t\"auto_update_cycle\":\t\"beta\",\n"
        "\t\"auto_update_weekdays_bitmask\":\t126,\n"
        "\t\"auto_update_interval_from\":\t1,\n"
        "\t\"auto_update_interval_to\":\t23,\n"
        "\t\"auto_update_tz_offset_hours\":\t7,\n"

        "\t\"ntp_use\":\ttrue,\n"
        "\t\"ntp_use_dhcp\":\tfalse,\n"
        "\t\"ntp_server1\":\t\"time1.server.com\",\n"
        "\t\"ntp_server2\":\t\"time2.server.com\",\n"
        "\t\"ntp_server3\":\t\"time3.server.com\",\n"
        "\t\"ntp_server4\":\t\"time4.server.com\",\n"

        "\t\"company_use_filtering\":\ttrue,\n"
        "\t\"company_id\":\t888,\n"

        "\t\"coordinates\":\t\"coord:123,456\",\n"

        "\t\"scan_coded_phy\":\ttrue,\n"
        "\t\"scan_1mbit_phy\":\ttrue,\n"
        "\t\"scan_extended_payload\":\ttrue,\n"
        "\t\"scan_channel_37\":\ttrue,\n"
        "\t\"scan_channel_38\":\ttrue,\n"
        "\t\"scan_channel_39\":\ttrue\n"
        "}");

    gw_cfg_t gw_cfg = { 0 };
    gw_cfg_default_get(&gw_cfg);
    bool flag_network_cfg = false;
    ASSERT_TRUE(json_ruuvi_parse_http_body(http_body.c_str(), &gw_cfg, &flag_network_cfg));
    ASSERT_FALSE(flag_network_cfg);
    ASSERT_FALSE(gw_cfg.ruuvi_cfg.remote.use_remote_cfg);
    ASSERT_EQ(string(""), string(gw_cfg.ruuvi_cfg.remote.url.buf));
    ASSERT_EQ(GW_CFG_HTTP_AUTH_TYPE_NONE, gw_cfg.ruuvi_cfg.remote.auth_type);
    ASSERT_EQ(0, gw_cfg.ruuvi_cfg.remote.refresh_interval_minutes);
    ASSERT_TRUE(gw_cfg.ruuvi_cfg.mqtt.use_mqtt);
    ASSERT_FALSE(gw_cfg.ruuvi_cfg.mqtt.mqtt_disable_retained_messages);
    ASSERT_EQ(string("TCP"), gw_cfg.ruuvi_cfg.mqtt.mqtt_transport.buf);
    ASSERT_EQ(string("mqtt.server.org"), gw_cfg.ruuvi_cfg.mqtt.mqtt_server.buf);
    ASSERT_EQ(string("ruuvi/AA:BB:CC:DD:EE:FF/"), gw_cfg.ruuvi_cfg.mqtt.mqtt_prefix.buf);
    ASSERT_EQ(string("AA:BB:CC:DD:EE:FF"), gw_cfg.ruuvi_cfg.mqtt.mqtt_client_id.buf);
    ASSERT_EQ(8883, gw_cfg.ruuvi_cfg.mqtt.mqtt_port);
    ASSERT_EQ(string("user123"), gw_cfg.ruuvi_cfg.mqtt.mqtt_user.buf);
    ASSERT_EQ(string(""), gw_cfg.ruuvi_cfg.mqtt.mqtt_pass.buf);
    ASSERT_FALSE(gw_cfg.ruuvi_cfg.http.use_http_ruuvi);
    ASSERT_FALSE(gw_cfg.ruuvi_cfg.http.use_http);
    ASSERT_EQ(string("https://api.ruuvi.com:456/api"), gw_cfg.ruuvi_cfg.http.http_url.buf);
    ASSERT_EQ(GW_CFG_HTTP_DATA_FORMAT_RUUVI, gw_cfg.ruuvi_cfg.http.data_format);
    ASSERT_EQ(GW_CFG_HTTP_AUTH_TYPE_NONE, gw_cfg.ruuvi_cfg.http.auth_type);
    ASSERT_TRUE(gw_cfg.ruuvi_cfg.http_stat.use_http_stat);
    ASSERT_EQ(string("https://api.ruuvi.com:456/status"), gw_cfg.ruuvi_cfg.http_stat.http_stat_url.buf);
    ASSERT_EQ(string("user678"), gw_cfg.ruuvi_cfg.http_stat.http_stat_user.buf);
    ASSERT_EQ(string("pass678"), gw_cfg.ruuvi_cfg.http_stat.http_stat_pass.buf);
    ASSERT_EQ(HTTP_SERVER_AUTH_TYPE_RUUVI, gw_cfg.ruuvi_cfg.lan_auth.lan_auth_type);
    ASSERT_EQ(string("user1"), gw_cfg.ruuvi_cfg.lan_auth.lan_auth_user.buf);
    ASSERT_EQ(string("qwe"), gw_cfg.ruuvi_cfg.lan_auth.lan_auth_pass.buf);
    ASSERT_EQ(string(""), gw_cfg.ruuvi_cfg.lan_auth.lan_auth_api_key.buf);
    ASSERT_EQ(string(""), gw_cfg.ruuvi_cfg.lan_auth.lan_auth_api_key_rw.buf);
    ASSERT_EQ(true, gw_cfg.ruuvi_cfg.ntp.ntp_use);
    ASSERT_EQ(false, gw_cfg.ruuvi_cfg.ntp.ntp_use_dhcp);
    ASSERT_EQ(string("time1.server.com"), string(gw_cfg.ruuvi_cfg.ntp.ntp_server1.buf));
    ASSERT_EQ(string("time2.server.com"), string(gw_cfg.ruuvi_cfg.ntp.ntp_server2.buf));
    ASSERT_EQ(string("time3.server.com"), string(gw_cfg.ruuvi_cfg.ntp.ntp_server3.buf));
    ASSERT_EQ(string("time4.server.com"), string(gw_cfg.ruuvi_cfg.ntp.ntp_server4.buf));
    ASSERT_TRUE(gw_cfg.ruuvi_cfg.filter.company_use_filtering);
    ASSERT_EQ(888, gw_cfg.ruuvi_cfg.filter.company_id);
    ASSERT_EQ(string("coord:123,456"), gw_cfg.ruuvi_cfg.coordinates.buf);
    ASSERT_EQ(true, gw_cfg.ruuvi_cfg.scan.scan_coded_phy);
    ASSERT_EQ(true, gw_cfg.ruuvi_cfg.scan.scan_1mbit_phy);
    ASSERT_EQ(true, gw_cfg.ruuvi_cfg.scan.scan_extended_payload);
    ASSERT_EQ(true, gw_cfg.ruuvi_cfg.scan.scan_channel_37);
    ASSERT_EQ(true, gw_cfg.ruuvi_cfg.scan.scan_channel_38);
    ASSERT_EQ(true, gw_cfg.ruuvi_cfg.scan.scan_channel_39);

    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, "Gateway SETTINGS (via HTTP):");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "remote_cfg_use: 0");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "remote_cfg_url: ");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "remote_cfg_auth_type: no");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "remote_cfg_refresh_interval_minutes: 0");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "use_http_ruuvi: 0");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "use_http: 0");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "http_data_format: ruuvi");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "http_auth: none");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "http_url: https://api.ruuvi.com:456/api");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "use_http_stat: 1");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "http_stat_url: https://api.ruuvi.com:456/status");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "http_stat_user: user678");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "http_stat_pass: pass678");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "use_mqtt: 1");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "mqtt_disable_retained_messages: 0");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "mqtt_transport: TCP");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "mqtt_server: mqtt.server.org");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "mqtt_port: 8883");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "mqtt_prefix: ");
    TEST_CHECK_LOG_RECORD_GW_CFG(
        ESP_LOG_WARN,
        "Key 'mqtt_prefix' is empty in config-json, use default value: ruuvi/AA:BB:CC:DD:EE:FF/");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "mqtt_client_id: ");
    TEST_CHECK_LOG_RECORD_GW_CFG(
        ESP_LOG_WARN,
        "Key 'mqtt_client_id' is empty in config-json, use default value: AA:BB:CC:DD:EE:FF");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "mqtt_user: user123");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "mqtt_pass: not found");
    TEST_CHECK_LOG_RECORD_GW_CFG(
        ESP_LOG_INFO,
        "Can't find key 'mqtt_pass' in config-json, leave the previous value unchanged");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "lan_auth_type: lan_auth_ruuvi");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "lan_auth_user: user1");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "lan_auth_pass: qwe");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "lan_auth_api_key: ");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "lan_auth_api_key_rw: ");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "auto_update_cycle: beta");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "auto_update_weekdays_bitmask: 126");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "auto_update_interval_from: 1");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "auto_update_interval_to: 23");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "auto_update_tz_offset_hours: 7");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "ntp_use: 1");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "ntp_use_dhcp: 0");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "ntp_server1: time1.server.com");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "ntp_server2: time2.server.com");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "ntp_server3: time3.server.com");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "ntp_server4: time4.server.com");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "company_id: 888");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "company_use_filtering: 1");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "scan_coded_phy: 1");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "scan_1mbit_phy: 1");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "scan_extended_payload: 1");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "scan_channel_37: 1");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "scan_channel_38: 1");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "scan_channel_39: 1");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "coordinates: coord:123,456");
    esp_log_wrapper_clear();
    ASSERT_TRUE(esp_log_wrapper_is_empty());
}

TEST_F(TestJsonRuuvi, json_ruuvi_parse_mqtt_no_prefix_and_client_id) // NOLINT
{
    const string http_body = string(
        "{\n"
        "\t\"remote_cfg_use\":\tfalse,\n"
        "\t\"remote_cfg_url\":\t\"\",\n"
        "\t\"remote_cfg_auth_type\":\t\"no\",\n"
        "\t\"remote_cfg_refresh_interval_minutes\":\t0,\n"

        "\t\"use_http_ruuvi\":\tfalse,\n"
        "\t\"use_http\":\tfalse,\n"
        "\t\"http_url\":\t\"https://api.ruuvi.com:456/api\",\n"
        "\t\"http_data_format\":\t\"ruuvi\",\n"
        "\t\"http_auth\":\t\"none\",\n"

        "\t\"use_http_stat\":\ttrue,\n"
        "\t\"http_stat_url\":\t\"https://api.ruuvi.com:456/status\",\n"
        "\t\"http_stat_user\":\t\"user678\",\n"
        "\t\"http_stat_pass\":\t\"pass678\",\n"

        "\t\"use_mqtt\":\ttrue,\n"
        "\t\"mqtt_disable_retained_messages\":\tfalse,\n"
        "\t\"mqtt_transport\":\t\"TCP\",\n"
        "\t\"mqtt_server\":\t\"mqtt.server.org\",\n"
        "\t\"mqtt_port\":\t8883,\n"
        "\t\"mqtt_user\":\t\"user123\",\n"

        "\t\"lan_auth_type\":\t\"lan_auth_ruuvi\",\n"
        "\t\"lan_auth_user\":\t\"user1\",\n"
        "\t\"lan_auth_pass\":\t\"qwe\",\n"
        "\t\"lan_auth_api_key\":\t\"\",\n"
        "\t\"lan_auth_api_key_rw\":\t\"\",\n"

        "\t\"auto_update_cycle\":\t\"beta\",\n"
        "\t\"auto_update_weekdays_bitmask\":\t126,\n"
        "\t\"auto_update_interval_from\":\t1,\n"
        "\t\"auto_update_interval_to\":\t23,\n"
        "\t\"auto_update_tz_offset_hours\":\t7,\n"

        "\t\"ntp_use\":\ttrue,\n"
        "\t\"ntp_use_dhcp\":\tfalse,\n"
        "\t\"ntp_server1\":\t\"time1.server.com\",\n"
        "\t\"ntp_server2\":\t\"time2.server.com\",\n"
        "\t\"ntp_server3\":\t\"time3.server.com\",\n"
        "\t\"ntp_server4\":\t\"time4.server.com\",\n"

        "\t\"company_use_filtering\":\ttrue,\n"
        "\t\"company_id\":\t888,\n"

        "\t\"coordinates\":\t\"coord:123,456\",\n"

        "\t\"scan_coded_phy\":\ttrue,\n"
        "\t\"scan_1mbit_phy\":\ttrue,\n"
        "\t\"scan_extended_payload\":\ttrue,\n"
        "\t\"scan_channel_37\":\ttrue,\n"
        "\t\"scan_channel_38\":\ttrue,\n"
        "\t\"scan_channel_39\":\ttrue\n"
        "}");

    gw_cfg_t gw_cfg = { 0 };
    gw_cfg_default_get(&gw_cfg);
    bool flag_network_cfg = false;
    ASSERT_TRUE(json_ruuvi_parse_http_body(http_body.c_str(), &gw_cfg, &flag_network_cfg));
    ASSERT_FALSE(flag_network_cfg);
    ASSERT_FALSE(gw_cfg.ruuvi_cfg.remote.use_remote_cfg);
    ASSERT_EQ(string(""), string(gw_cfg.ruuvi_cfg.remote.url.buf));
    ASSERT_EQ(GW_CFG_HTTP_AUTH_TYPE_NONE, gw_cfg.ruuvi_cfg.remote.auth_type);
    ASSERT_EQ(0, gw_cfg.ruuvi_cfg.remote.refresh_interval_minutes);
    ASSERT_TRUE(gw_cfg.ruuvi_cfg.mqtt.use_mqtt);
    ASSERT_FALSE(gw_cfg.ruuvi_cfg.mqtt.mqtt_disable_retained_messages);
    ASSERT_EQ(string("TCP"), gw_cfg.ruuvi_cfg.mqtt.mqtt_transport.buf);
    ASSERT_EQ(string("mqtt.server.org"), gw_cfg.ruuvi_cfg.mqtt.mqtt_server.buf);
    ASSERT_EQ(string("ruuvi/AA:BB:CC:DD:EE:FF/"), gw_cfg.ruuvi_cfg.mqtt.mqtt_prefix.buf);
    ASSERT_EQ(string("AA:BB:CC:DD:EE:FF"), gw_cfg.ruuvi_cfg.mqtt.mqtt_client_id.buf);
    ASSERT_EQ(8883, gw_cfg.ruuvi_cfg.mqtt.mqtt_port);
    ASSERT_EQ(string("user123"), gw_cfg.ruuvi_cfg.mqtt.mqtt_user.buf);
    ASSERT_EQ(string(""), gw_cfg.ruuvi_cfg.mqtt.mqtt_pass.buf);
    ASSERT_FALSE(gw_cfg.ruuvi_cfg.http.use_http_ruuvi);
    ASSERT_FALSE(gw_cfg.ruuvi_cfg.http.use_http);
    ASSERT_EQ(string("https://api.ruuvi.com:456/api"), gw_cfg.ruuvi_cfg.http.http_url.buf);
    ASSERT_EQ(GW_CFG_HTTP_DATA_FORMAT_RUUVI, gw_cfg.ruuvi_cfg.http.data_format);
    ASSERT_EQ(GW_CFG_HTTP_AUTH_TYPE_NONE, gw_cfg.ruuvi_cfg.http.auth_type);
    ASSERT_TRUE(gw_cfg.ruuvi_cfg.http_stat.use_http_stat);
    ASSERT_EQ(string("https://api.ruuvi.com:456/status"), gw_cfg.ruuvi_cfg.http_stat.http_stat_url.buf);
    ASSERT_EQ(string("user678"), gw_cfg.ruuvi_cfg.http_stat.http_stat_user.buf);
    ASSERT_EQ(string("pass678"), gw_cfg.ruuvi_cfg.http_stat.http_stat_pass.buf);
    ASSERT_EQ(HTTP_SERVER_AUTH_TYPE_RUUVI, gw_cfg.ruuvi_cfg.lan_auth.lan_auth_type);
    ASSERT_EQ(string("user1"), gw_cfg.ruuvi_cfg.lan_auth.lan_auth_user.buf);
    ASSERT_EQ(string("qwe"), gw_cfg.ruuvi_cfg.lan_auth.lan_auth_pass.buf);
    ASSERT_EQ(string(""), gw_cfg.ruuvi_cfg.lan_auth.lan_auth_api_key.buf);
    ASSERT_EQ(string(""), gw_cfg.ruuvi_cfg.lan_auth.lan_auth_api_key_rw.buf);
    ASSERT_EQ(true, gw_cfg.ruuvi_cfg.ntp.ntp_use);
    ASSERT_EQ(false, gw_cfg.ruuvi_cfg.ntp.ntp_use_dhcp);
    ASSERT_EQ(string("time1.server.com"), string(gw_cfg.ruuvi_cfg.ntp.ntp_server1.buf));
    ASSERT_EQ(string("time2.server.com"), string(gw_cfg.ruuvi_cfg.ntp.ntp_server2.buf));
    ASSERT_EQ(string("time3.server.com"), string(gw_cfg.ruuvi_cfg.ntp.ntp_server3.buf));
    ASSERT_EQ(string("time4.server.com"), string(gw_cfg.ruuvi_cfg.ntp.ntp_server4.buf));
    ASSERT_TRUE(gw_cfg.ruuvi_cfg.filter.company_use_filtering);
    ASSERT_EQ(888, gw_cfg.ruuvi_cfg.filter.company_id);
    ASSERT_EQ(string("coord:123,456"), gw_cfg.ruuvi_cfg.coordinates.buf);
    ASSERT_EQ(true, gw_cfg.ruuvi_cfg.scan.scan_coded_phy);
    ASSERT_EQ(true, gw_cfg.ruuvi_cfg.scan.scan_1mbit_phy);
    ASSERT_EQ(true, gw_cfg.ruuvi_cfg.scan.scan_extended_payload);
    ASSERT_EQ(true, gw_cfg.ruuvi_cfg.scan.scan_channel_37);
    ASSERT_EQ(true, gw_cfg.ruuvi_cfg.scan.scan_channel_38);
    ASSERT_EQ(true, gw_cfg.ruuvi_cfg.scan.scan_channel_39);

    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, "Gateway SETTINGS (via HTTP):");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "remote_cfg_use: 0");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "remote_cfg_url: ");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "remote_cfg_auth_type: no");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "remote_cfg_refresh_interval_minutes: 0");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "use_http_ruuvi: 0");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "use_http: 0");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "http_data_format: ruuvi");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "http_auth: none");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "http_url: https://api.ruuvi.com:456/api");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "use_http_stat: 1");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "http_stat_url: https://api.ruuvi.com:456/status");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "http_stat_user: user678");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "http_stat_pass: pass678");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "use_mqtt: 1");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "mqtt_disable_retained_messages: 0");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "mqtt_transport: TCP");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "mqtt_server: mqtt.server.org");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "mqtt_port: 8883");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "mqtt_prefix: not found");
    TEST_CHECK_LOG_RECORD_GW_CFG(
        ESP_LOG_WARN,
        "Can't find key 'mqtt_prefix' in config-json, use default value: ruuvi/AA:BB:CC:DD:EE:FF/");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "mqtt_client_id: not found");
    TEST_CHECK_LOG_RECORD_GW_CFG(
        ESP_LOG_WARN,
        "Can't find key 'mqtt_client_id' in config-json, use default value: AA:BB:CC:DD:EE:FF");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "mqtt_user: user123");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "mqtt_pass: not found");
    TEST_CHECK_LOG_RECORD_GW_CFG(
        ESP_LOG_INFO,
        "Can't find key 'mqtt_pass' in config-json, leave the previous value unchanged");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "lan_auth_type: lan_auth_ruuvi");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "lan_auth_user: user1");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "lan_auth_pass: qwe");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "lan_auth_api_key: ");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "lan_auth_api_key_rw: ");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "auto_update_cycle: beta");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "auto_update_weekdays_bitmask: 126");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "auto_update_interval_from: 1");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "auto_update_interval_to: 23");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "auto_update_tz_offset_hours: 7");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "ntp_use: 1");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "ntp_use_dhcp: 0");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "ntp_server1: time1.server.com");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "ntp_server2: time2.server.com");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "ntp_server3: time3.server.com");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "ntp_server4: time4.server.com");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "company_id: 888");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "company_use_filtering: 1");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "scan_coded_phy: 1");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "scan_1mbit_phy: 1");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "scan_extended_payload: 1");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "scan_channel_37: 1");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "scan_channel_38: 1");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "scan_channel_39: 1");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "coordinates: coord:123,456");
    esp_log_wrapper_clear();
    ASSERT_TRUE(esp_log_wrapper_is_empty());
}

TEST_F(TestJsonRuuvi, json_ruuvi_parse_mqtt_ssl) // NOLINT
{
    const string http_body = string(
        "{\n"
        "\t\"remote_cfg_use\":\tfalse,\n"
        "\t\"remote_cfg_url\":\t\"\",\n"
        "\t\"remote_cfg_auth_type\":\t\"no\",\n"
        "\t\"remote_cfg_refresh_interval_minutes\":\t0,\n"

        "\t\"use_http_ruuvi\":\tfalse,\n"
        "\t\"use_http\":\tfalse,\n"
        "\t\"http_url\":\t\"https://api.ruuvi.com:456/api\",\n"
        "\t\"http_data_format\":\t\"ruuvi\",\n"
        "\t\"http_auth\":\t\"none\",\n"

        "\t\"use_http_stat\":\ttrue,\n"
        "\t\"http_stat_url\":\t\"https://api.ruuvi.com:456/status\",\n"
        "\t\"http_stat_user\":\t\"user678\",\n"
        "\t\"http_stat_pass\":\t\"pass678\",\n"

        "\t\"use_mqtt\":\ttrue,\n"
        "\t\"mqtt_disable_retained_messages\":\tfalse,\n"
        "\t\"mqtt_transport\":\t\"SSL\",\n"
        "\t\"mqtt_server\":\t\"mqtt.server.org\",\n"
        "\t\"mqtt_prefix\":\t\"prefix\",\n"
        "\t\"mqtt_client_id\":\t\"AA:BB:CC:DD:EE:FF\",\n"
        "\t\"mqtt_port\":\t8883,\n"
        "\t\"mqtt_user\":\t\"user123\",\n"

        "\t\"lan_auth_type\":\t\"lan_auth_ruuvi\",\n"
        "\t\"lan_auth_user\":\t\"user1\",\n"
        "\t\"lan_auth_pass\":\t\"qwe\",\n"
        "\t\"lan_auth_api_key\":\t\"\",\n"
        "\t\"lan_auth_api_key_rw\":\t\"\",\n"

        "\t\"auto_update_cycle\":\t\"beta\",\n"
        "\t\"auto_update_weekdays_bitmask\":\t126,\n"
        "\t\"auto_update_interval_from\":\t1,\n"
        "\t\"auto_update_interval_to\":\t23,\n"
        "\t\"auto_update_tz_offset_hours\":\t7,\n"

        "\t\"ntp_use\":\ttrue,\n"
        "\t\"ntp_use_dhcp\":\tfalse,\n"
        "\t\"ntp_server1\":\t\"time1.server.com\",\n"
        "\t\"ntp_server2\":\t\"time2.server.com\",\n"
        "\t\"ntp_server3\":\t\"time3.server.com\",\n"
        "\t\"ntp_server4\":\t\"time4.server.com\",\n"

        "\t\"company_use_filtering\":\ttrue,\n"
        "\t\"company_id\":\t888,\n"

        "\t\"coordinates\":\t\"coord:123,456\",\n"

        "\t\"scan_coded_phy\":\ttrue,\n"
        "\t\"scan_1mbit_phy\":\ttrue,\n"
        "\t\"scan_extended_payload\":\ttrue,\n"
        "\t\"scan_channel_37\":\ttrue,\n"
        "\t\"scan_channel_38\":\ttrue,\n"
        "\t\"scan_channel_39\":\ttrue\n"
        "}");

    gw_cfg_t gw_cfg = { 0 };
    gw_cfg_default_get(&gw_cfg);
    bool flag_network_cfg = false;
    ASSERT_TRUE(json_ruuvi_parse_http_body(http_body.c_str(), &gw_cfg, &flag_network_cfg));
    ASSERT_FALSE(flag_network_cfg);
    ASSERT_FALSE(gw_cfg.ruuvi_cfg.remote.use_remote_cfg);
    ASSERT_EQ(string(""), string(gw_cfg.ruuvi_cfg.remote.url.buf));
    ASSERT_EQ(GW_CFG_HTTP_AUTH_TYPE_NONE, gw_cfg.ruuvi_cfg.remote.auth_type);
    ASSERT_EQ(0, gw_cfg.ruuvi_cfg.remote.refresh_interval_minutes);
    ASSERT_TRUE(gw_cfg.ruuvi_cfg.mqtt.use_mqtt);
    ASSERT_FALSE(gw_cfg.ruuvi_cfg.mqtt.mqtt_disable_retained_messages);
    ASSERT_EQ(string("SSL"), gw_cfg.ruuvi_cfg.mqtt.mqtt_transport.buf);
    ASSERT_EQ(string("mqtt.server.org"), gw_cfg.ruuvi_cfg.mqtt.mqtt_server.buf);
    ASSERT_EQ(string("prefix"), gw_cfg.ruuvi_cfg.mqtt.mqtt_prefix.buf);
    ASSERT_EQ(string("AA:BB:CC:DD:EE:FF"), gw_cfg.ruuvi_cfg.mqtt.mqtt_client_id.buf);
    ASSERT_EQ(8883, gw_cfg.ruuvi_cfg.mqtt.mqtt_port);
    ASSERT_EQ(string("user123"), gw_cfg.ruuvi_cfg.mqtt.mqtt_user.buf);
    ASSERT_EQ(string(""), gw_cfg.ruuvi_cfg.mqtt.mqtt_pass.buf);
    ASSERT_FALSE(gw_cfg.ruuvi_cfg.http.use_http_ruuvi);
    ASSERT_FALSE(gw_cfg.ruuvi_cfg.http.use_http);
    ASSERT_EQ(string("https://api.ruuvi.com:456/api"), gw_cfg.ruuvi_cfg.http.http_url.buf);
    ASSERT_EQ(GW_CFG_HTTP_DATA_FORMAT_RUUVI, gw_cfg.ruuvi_cfg.http.data_format);
    ASSERT_EQ(GW_CFG_HTTP_AUTH_TYPE_NONE, gw_cfg.ruuvi_cfg.http.auth_type);
    ASSERT_TRUE(gw_cfg.ruuvi_cfg.http_stat.use_http_stat);
    ASSERT_EQ(string("https://api.ruuvi.com:456/status"), gw_cfg.ruuvi_cfg.http_stat.http_stat_url.buf);
    ASSERT_EQ(string("user678"), gw_cfg.ruuvi_cfg.http_stat.http_stat_user.buf);
    ASSERT_EQ(string("pass678"), gw_cfg.ruuvi_cfg.http_stat.http_stat_pass.buf);
    ASSERT_EQ(HTTP_SERVER_AUTH_TYPE_RUUVI, gw_cfg.ruuvi_cfg.lan_auth.lan_auth_type);
    ASSERT_EQ(string("user1"), gw_cfg.ruuvi_cfg.lan_auth.lan_auth_user.buf);
    ASSERT_EQ(string("qwe"), gw_cfg.ruuvi_cfg.lan_auth.lan_auth_pass.buf);
    ASSERT_EQ(string(""), gw_cfg.ruuvi_cfg.lan_auth.lan_auth_api_key.buf);
    ASSERT_EQ(string(""), gw_cfg.ruuvi_cfg.lan_auth.lan_auth_api_key_rw.buf);
    ASSERT_EQ(true, gw_cfg.ruuvi_cfg.ntp.ntp_use);
    ASSERT_EQ(false, gw_cfg.ruuvi_cfg.ntp.ntp_use_dhcp);
    ASSERT_EQ(string("time1.server.com"), string(gw_cfg.ruuvi_cfg.ntp.ntp_server1.buf));
    ASSERT_EQ(string("time2.server.com"), string(gw_cfg.ruuvi_cfg.ntp.ntp_server2.buf));
    ASSERT_EQ(string("time3.server.com"), string(gw_cfg.ruuvi_cfg.ntp.ntp_server3.buf));
    ASSERT_EQ(string("time4.server.com"), string(gw_cfg.ruuvi_cfg.ntp.ntp_server4.buf));
    ASSERT_TRUE(gw_cfg.ruuvi_cfg.filter.company_use_filtering);
    ASSERT_EQ(888, gw_cfg.ruuvi_cfg.filter.company_id);
    ASSERT_EQ(string("coord:123,456"), gw_cfg.ruuvi_cfg.coordinates.buf);
    ASSERT_EQ(true, gw_cfg.ruuvi_cfg.scan.scan_coded_phy);
    ASSERT_EQ(true, gw_cfg.ruuvi_cfg.scan.scan_1mbit_phy);
    ASSERT_EQ(true, gw_cfg.ruuvi_cfg.scan.scan_extended_payload);
    ASSERT_EQ(true, gw_cfg.ruuvi_cfg.scan.scan_channel_37);
    ASSERT_EQ(true, gw_cfg.ruuvi_cfg.scan.scan_channel_38);
    ASSERT_EQ(true, gw_cfg.ruuvi_cfg.scan.scan_channel_39);

    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, "Gateway SETTINGS (via HTTP):");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "remote_cfg_use: 0");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "remote_cfg_url: ");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "remote_cfg_auth_type: no");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "remote_cfg_refresh_interval_minutes: 0");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "use_http_ruuvi: 0");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "use_http: 0");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "http_data_format: ruuvi");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "http_auth: none");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "http_url: https://api.ruuvi.com:456/api");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "use_http_stat: 1");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "http_stat_url: https://api.ruuvi.com:456/status");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "http_stat_user: user678");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "http_stat_pass: pass678");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "use_mqtt: 1");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "mqtt_disable_retained_messages: 0");
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
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "lan_auth_type: lan_auth_ruuvi");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "lan_auth_user: user1");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "lan_auth_pass: qwe");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "lan_auth_api_key: ");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "lan_auth_api_key_rw: ");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "auto_update_cycle: beta");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "auto_update_weekdays_bitmask: 126");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "auto_update_interval_from: 1");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "auto_update_interval_to: 23");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "auto_update_tz_offset_hours: 7");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "ntp_use: 1");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "ntp_use_dhcp: 0");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "ntp_server1: time1.server.com");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "ntp_server2: time2.server.com");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "ntp_server3: time3.server.com");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "ntp_server4: time4.server.com");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "company_id: 888");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "company_use_filtering: 1");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "scan_coded_phy: 1");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "scan_1mbit_phy: 1");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "scan_extended_payload: 1");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "scan_channel_37: 1");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "scan_channel_38: 1");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "scan_channel_39: 1");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "coordinates: coord:123,456");
    esp_log_wrapper_clear();
    ASSERT_TRUE(esp_log_wrapper_is_empty());
}

TEST_F(TestJsonRuuvi, json_ruuvi_parse_mqtt_websocket) // NOLINT
{
    const string http_body = string(
        "{\n"
        "\t\"remote_cfg_use\":\tfalse,\n"
        "\t\"remote_cfg_url\":\t\"\",\n"
        "\t\"remote_cfg_auth_type\":\t\"no\",\n"
        "\t\"remote_cfg_refresh_interval_minutes\":\t0,\n"

        "\t\"use_http_ruuvi\":\tfalse,\n"
        "\t\"use_http\":\tfalse,\n"
        "\t\"http_url\":\t\"https://api.ruuvi.com:456/api\",\n"
        "\t\"http_data_format\":\t\"ruuvi\",\n"
        "\t\"http_auth\":\t\"none\",\n"

        "\t\"use_http_stat\":\ttrue,\n"
        "\t\"http_stat_url\":\t\"https://api.ruuvi.com:456/status\",\n"
        "\t\"http_stat_user\":\t\"user678\",\n"
        "\t\"http_stat_pass\":\t\"pass678\",\n"

        "\t\"use_mqtt\":\ttrue,\n"
        "\t\"mqtt_disable_retained_messages\":\tfalse,\n"
        "\t\"mqtt_transport\":\t\"WS\",\n"
        "\t\"mqtt_server\":\t\"mqtt.server.org\",\n"
        "\t\"mqtt_prefix\":\t\"prefix\",\n"
        "\t\"mqtt_client_id\":\t\"AA:BB:CC:DD:EE:FF\",\n"
        "\t\"mqtt_port\":\t8080,\n"
        "\t\"mqtt_user\":\t\"user123\",\n"

        "\t\"lan_auth_type\":\t\"lan_auth_ruuvi\",\n"
        "\t\"lan_auth_user\":\t\"user1\",\n"
        "\t\"lan_auth_pass\":\t\"qwe\",\n"
        "\t\"lan_auth_api_key\":\t\"\",\n"
        "\t\"lan_auth_api_key_rw\":\t\"\",\n"

        "\t\"auto_update_cycle\":\t\"beta\",\n"
        "\t\"auto_update_weekdays_bitmask\":\t126,\n"
        "\t\"auto_update_interval_from\":\t1,\n"
        "\t\"auto_update_interval_to\":\t23,\n"
        "\t\"auto_update_tz_offset_hours\":\t7,\n"

        "\t\"ntp_use\":\ttrue,\n"
        "\t\"ntp_use_dhcp\":\tfalse,\n"
        "\t\"ntp_server1\":\t\"time1.server.com\",\n"
        "\t\"ntp_server2\":\t\"time2.server.com\",\n"
        "\t\"ntp_server3\":\t\"time3.server.com\",\n"
        "\t\"ntp_server4\":\t\"time4.server.com\",\n"

        "\t\"company_use_filtering\":\ttrue,\n"
        "\t\"company_id\":\t888,\n"

        "\t\"coordinates\":\t\"coord:123,456\",\n"

        "\t\"scan_coded_phy\":\ttrue,\n"
        "\t\"scan_1mbit_phy\":\ttrue,\n"
        "\t\"scan_extended_payload\":\ttrue,\n"
        "\t\"scan_channel_37\":\ttrue,\n"
        "\t\"scan_channel_38\":\ttrue,\n"
        "\t\"scan_channel_39\":\ttrue\n"
        "}");

    gw_cfg_t gw_cfg = { 0 };
    gw_cfg_default_get(&gw_cfg);
    bool flag_network_cfg = false;
    ASSERT_TRUE(json_ruuvi_parse_http_body(http_body.c_str(), &gw_cfg, &flag_network_cfg));
    ASSERT_FALSE(flag_network_cfg);
    ASSERT_FALSE(gw_cfg.ruuvi_cfg.remote.use_remote_cfg);
    ASSERT_EQ(string(""), string(gw_cfg.ruuvi_cfg.remote.url.buf));
    ASSERT_EQ(GW_CFG_HTTP_AUTH_TYPE_NONE, gw_cfg.ruuvi_cfg.remote.auth_type);
    ASSERT_EQ(0, gw_cfg.ruuvi_cfg.remote.refresh_interval_minutes);
    ASSERT_TRUE(gw_cfg.ruuvi_cfg.mqtt.use_mqtt);
    ASSERT_FALSE(gw_cfg.ruuvi_cfg.mqtt.mqtt_disable_retained_messages);
    ASSERT_EQ(string("WS"), gw_cfg.ruuvi_cfg.mqtt.mqtt_transport.buf);
    ASSERT_EQ(string("mqtt.server.org"), gw_cfg.ruuvi_cfg.mqtt.mqtt_server.buf);
    ASSERT_EQ(string("prefix"), gw_cfg.ruuvi_cfg.mqtt.mqtt_prefix.buf);
    ASSERT_EQ(string("AA:BB:CC:DD:EE:FF"), gw_cfg.ruuvi_cfg.mqtt.mqtt_client_id.buf);
    ASSERT_EQ(8080, gw_cfg.ruuvi_cfg.mqtt.mqtt_port);
    ASSERT_EQ(string("user123"), gw_cfg.ruuvi_cfg.mqtt.mqtt_user.buf);
    ASSERT_EQ(string(""), gw_cfg.ruuvi_cfg.mqtt.mqtt_pass.buf);
    ASSERT_FALSE(gw_cfg.ruuvi_cfg.http.use_http_ruuvi);
    ASSERT_FALSE(gw_cfg.ruuvi_cfg.http.use_http);
    ASSERT_EQ(string("https://api.ruuvi.com:456/api"), gw_cfg.ruuvi_cfg.http.http_url.buf);
    ASSERT_EQ(GW_CFG_HTTP_DATA_FORMAT_RUUVI, gw_cfg.ruuvi_cfg.http.data_format);
    ASSERT_EQ(GW_CFG_HTTP_AUTH_TYPE_NONE, gw_cfg.ruuvi_cfg.http.auth_type);
    ASSERT_TRUE(gw_cfg.ruuvi_cfg.http_stat.use_http_stat);
    ASSERT_EQ(string("https://api.ruuvi.com:456/status"), gw_cfg.ruuvi_cfg.http_stat.http_stat_url.buf);
    ASSERT_EQ(string("user678"), gw_cfg.ruuvi_cfg.http_stat.http_stat_user.buf);
    ASSERT_EQ(string("pass678"), gw_cfg.ruuvi_cfg.http_stat.http_stat_pass.buf);
    ASSERT_EQ(HTTP_SERVER_AUTH_TYPE_RUUVI, gw_cfg.ruuvi_cfg.lan_auth.lan_auth_type);
    ASSERT_EQ(string("user1"), gw_cfg.ruuvi_cfg.lan_auth.lan_auth_user.buf);
    ASSERT_EQ(string("qwe"), gw_cfg.ruuvi_cfg.lan_auth.lan_auth_pass.buf);
    ASSERT_EQ(string(""), gw_cfg.ruuvi_cfg.lan_auth.lan_auth_api_key.buf);
    ASSERT_EQ(string(""), gw_cfg.ruuvi_cfg.lan_auth.lan_auth_api_key_rw.buf);
    ASSERT_EQ(true, gw_cfg.ruuvi_cfg.ntp.ntp_use);
    ASSERT_EQ(false, gw_cfg.ruuvi_cfg.ntp.ntp_use_dhcp);
    ASSERT_EQ(string("time1.server.com"), string(gw_cfg.ruuvi_cfg.ntp.ntp_server1.buf));
    ASSERT_EQ(string("time2.server.com"), string(gw_cfg.ruuvi_cfg.ntp.ntp_server2.buf));
    ASSERT_EQ(string("time3.server.com"), string(gw_cfg.ruuvi_cfg.ntp.ntp_server3.buf));
    ASSERT_EQ(string("time4.server.com"), string(gw_cfg.ruuvi_cfg.ntp.ntp_server4.buf));
    ASSERT_TRUE(gw_cfg.ruuvi_cfg.filter.company_use_filtering);
    ASSERT_EQ(888, gw_cfg.ruuvi_cfg.filter.company_id);
    ASSERT_EQ(string("coord:123,456"), gw_cfg.ruuvi_cfg.coordinates.buf);
    ASSERT_EQ(true, gw_cfg.ruuvi_cfg.scan.scan_coded_phy);
    ASSERT_EQ(true, gw_cfg.ruuvi_cfg.scan.scan_1mbit_phy);
    ASSERT_EQ(true, gw_cfg.ruuvi_cfg.scan.scan_extended_payload);
    ASSERT_EQ(true, gw_cfg.ruuvi_cfg.scan.scan_channel_37);
    ASSERT_EQ(true, gw_cfg.ruuvi_cfg.scan.scan_channel_38);
    ASSERT_EQ(true, gw_cfg.ruuvi_cfg.scan.scan_channel_39);

    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, "Gateway SETTINGS (via HTTP):");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "remote_cfg_use: 0");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "remote_cfg_url: ");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "remote_cfg_auth_type: no");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "remote_cfg_refresh_interval_minutes: 0");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "use_http_ruuvi: 0");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "use_http: 0");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "http_data_format: ruuvi");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "http_auth: none");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "http_url: https://api.ruuvi.com:456/api");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "use_http_stat: 1");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "http_stat_url: https://api.ruuvi.com:456/status");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "http_stat_user: user678");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "http_stat_pass: pass678");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "use_mqtt: 1");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "mqtt_disable_retained_messages: 0");
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
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "lan_auth_type: lan_auth_ruuvi");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "lan_auth_user: user1");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "lan_auth_pass: qwe");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "lan_auth_api_key: ");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "lan_auth_api_key_rw: ");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "auto_update_cycle: beta");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "auto_update_weekdays_bitmask: 126");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "auto_update_interval_from: 1");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "auto_update_interval_to: 23");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "auto_update_tz_offset_hours: 7");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "ntp_use: 1");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "ntp_use_dhcp: 0");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "ntp_server1: time1.server.com");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "ntp_server2: time2.server.com");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "ntp_server3: time3.server.com");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "ntp_server4: time4.server.com");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "company_id: 888");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "company_use_filtering: 1");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "scan_coded_phy: 1");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "scan_1mbit_phy: 1");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "scan_extended_payload: 1");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "scan_channel_37: 1");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "scan_channel_38: 1");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "scan_channel_39: 1");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "coordinates: coord:123,456");
    esp_log_wrapper_clear();
    ASSERT_TRUE(esp_log_wrapper_is_empty());
}

TEST_F(TestJsonRuuvi, json_ruuvi_parse_mqtt_secure_websocket) // NOLINT
{
    const string http_body = string(
        "{\n"
        "\t\"remote_cfg_use\":\tfalse,\n"
        "\t\"remote_cfg_url\":\t\"\",\n"
        "\t\"remote_cfg_auth_type\":\t\"no\",\n"
        "\t\"remote_cfg_refresh_interval_minutes\":\t0,\n"

        "\t\"use_http_ruuvi\":\tfalse,\n"
        "\t\"use_http\":\tfalse,\n"
        "\t\"http_url\":\t\"https://api.ruuvi.com:456/api\",\n"
        "\t\"http_data_format\":\t\"ruuvi\",\n"
        "\t\"http_auth\":\t\"none\",\n"

        "\t\"use_http_stat\":\ttrue,\n"
        "\t\"http_stat_url\":\t\"https://api.ruuvi.com:456/status\",\n"
        "\t\"http_stat_user\":\t\"user678\",\n"
        "\t\"http_stat_pass\":\t\"pass678\",\n"

        "\t\"use_mqtt\":\ttrue,\n"
        "\t\"mqtt_disable_retained_messages\":\tfalse,\n"
        "\t\"mqtt_transport\":\t\"WSS\",\n"
        "\t\"mqtt_server\":\t\"mqtt.server.org\",\n"
        "\t\"mqtt_prefix\":\t\"prefix\",\n"
        "\t\"mqtt_client_id\":\t\"AA:BB:CC:DD:EE:FF\",\n"
        "\t\"mqtt_port\":\t8081,\n"
        "\t\"mqtt_user\":\t\"user123\",\n"

        "\t\"lan_auth_type\":\t\"lan_auth_ruuvi\",\n"
        "\t\"lan_auth_user\":\t\"user1\",\n"
        "\t\"lan_auth_pass\":\t\"qwe\",\n"
        "\t\"lan_auth_api_key\":\t\"\",\n"
        "\t\"lan_auth_api_key_rw\":\t\"\",\n"

        "\t\"auto_update_cycle\":\t\"beta\",\n"
        "\t\"auto_update_weekdays_bitmask\":\t126,\n"
        "\t\"auto_update_interval_from\":\t1,\n"
        "\t\"auto_update_interval_to\":\t23,\n"
        "\t\"auto_update_tz_offset_hours\":\t7,\n"

        "\t\"ntp_use\":\ttrue,\n"
        "\t\"ntp_use_dhcp\":\tfalse,\n"
        "\t\"ntp_server1\":\t\"time1.server.com\",\n"
        "\t\"ntp_server2\":\t\"time2.server.com\",\n"
        "\t\"ntp_server3\":\t\"time3.server.com\",\n"
        "\t\"ntp_server4\":\t\"time4.server.com\",\n"

        "\t\"company_use_filtering\":\ttrue,\n"
        "\t\"company_id\":\t888,\n"

        "\t\"coordinates\":\t\"coord:123,456\",\n"

        "\t\"scan_coded_phy\":\ttrue,\n"
        "\t\"scan_1mbit_phy\":\ttrue,\n"
        "\t\"scan_extended_payload\":\ttrue,\n"
        "\t\"scan_channel_37\":\ttrue,\n"
        "\t\"scan_channel_38\":\ttrue,\n"
        "\t\"scan_channel_39\":\ttrue\n"
        "}");

    gw_cfg_t gw_cfg = { 0 };
    gw_cfg_default_get(&gw_cfg);
    bool flag_network_cfg = false;
    ASSERT_TRUE(json_ruuvi_parse_http_body(http_body.c_str(), &gw_cfg, &flag_network_cfg));
    ASSERT_FALSE(flag_network_cfg);
    ASSERT_FALSE(gw_cfg.ruuvi_cfg.remote.use_remote_cfg);
    ASSERT_EQ(string(""), string(gw_cfg.ruuvi_cfg.remote.url.buf));
    ASSERT_EQ(GW_CFG_HTTP_AUTH_TYPE_NONE, gw_cfg.ruuvi_cfg.remote.auth_type);
    ASSERT_EQ(0, gw_cfg.ruuvi_cfg.remote.refresh_interval_minutes);
    ASSERT_TRUE(gw_cfg.ruuvi_cfg.mqtt.use_mqtt);
    ASSERT_FALSE(gw_cfg.ruuvi_cfg.mqtt.mqtt_disable_retained_messages);
    ASSERT_EQ(string("WSS"), gw_cfg.ruuvi_cfg.mqtt.mqtt_transport.buf);
    ASSERT_EQ(string("mqtt.server.org"), gw_cfg.ruuvi_cfg.mqtt.mqtt_server.buf);
    ASSERT_EQ(string("prefix"), gw_cfg.ruuvi_cfg.mqtt.mqtt_prefix.buf);
    ASSERT_EQ(string("AA:BB:CC:DD:EE:FF"), gw_cfg.ruuvi_cfg.mqtt.mqtt_client_id.buf);
    ASSERT_EQ(8081, gw_cfg.ruuvi_cfg.mqtt.mqtt_port);
    ASSERT_EQ(string("user123"), gw_cfg.ruuvi_cfg.mqtt.mqtt_user.buf);
    ASSERT_EQ(string(""), gw_cfg.ruuvi_cfg.mqtt.mqtt_pass.buf);
    ASSERT_FALSE(gw_cfg.ruuvi_cfg.http.use_http_ruuvi);
    ASSERT_FALSE(gw_cfg.ruuvi_cfg.http.use_http);
    ASSERT_EQ(string("https://api.ruuvi.com:456/api"), gw_cfg.ruuvi_cfg.http.http_url.buf);
    ASSERT_EQ(GW_CFG_HTTP_DATA_FORMAT_RUUVI, gw_cfg.ruuvi_cfg.http.data_format);
    ASSERT_EQ(GW_CFG_HTTP_AUTH_TYPE_NONE, gw_cfg.ruuvi_cfg.http.auth_type);
    ASSERT_TRUE(gw_cfg.ruuvi_cfg.http_stat.use_http_stat);
    ASSERT_EQ(string("https://api.ruuvi.com:456/status"), gw_cfg.ruuvi_cfg.http_stat.http_stat_url.buf);
    ASSERT_EQ(string("user678"), gw_cfg.ruuvi_cfg.http_stat.http_stat_user.buf);
    ASSERT_EQ(string("pass678"), gw_cfg.ruuvi_cfg.http_stat.http_stat_pass.buf);
    ASSERT_EQ(HTTP_SERVER_AUTH_TYPE_RUUVI, gw_cfg.ruuvi_cfg.lan_auth.lan_auth_type);
    ASSERT_EQ(string("user1"), gw_cfg.ruuvi_cfg.lan_auth.lan_auth_user.buf);
    ASSERT_EQ(string("qwe"), gw_cfg.ruuvi_cfg.lan_auth.lan_auth_pass.buf);
    ASSERT_EQ(string(""), gw_cfg.ruuvi_cfg.lan_auth.lan_auth_api_key.buf);
    ASSERT_EQ(string(""), gw_cfg.ruuvi_cfg.lan_auth.lan_auth_api_key_rw.buf);
    ASSERT_EQ(true, gw_cfg.ruuvi_cfg.ntp.ntp_use);
    ASSERT_EQ(false, gw_cfg.ruuvi_cfg.ntp.ntp_use_dhcp);
    ASSERT_EQ(string("time1.server.com"), string(gw_cfg.ruuvi_cfg.ntp.ntp_server1.buf));
    ASSERT_EQ(string("time2.server.com"), string(gw_cfg.ruuvi_cfg.ntp.ntp_server2.buf));
    ASSERT_EQ(string("time3.server.com"), string(gw_cfg.ruuvi_cfg.ntp.ntp_server3.buf));
    ASSERT_EQ(string("time4.server.com"), string(gw_cfg.ruuvi_cfg.ntp.ntp_server4.buf));
    ASSERT_TRUE(gw_cfg.ruuvi_cfg.filter.company_use_filtering);
    ASSERT_EQ(888, gw_cfg.ruuvi_cfg.filter.company_id);
    ASSERT_EQ(string("coord:123,456"), gw_cfg.ruuvi_cfg.coordinates.buf);
    ASSERT_EQ(true, gw_cfg.ruuvi_cfg.scan.scan_coded_phy);
    ASSERT_EQ(true, gw_cfg.ruuvi_cfg.scan.scan_1mbit_phy);
    ASSERT_EQ(true, gw_cfg.ruuvi_cfg.scan.scan_extended_payload);
    ASSERT_EQ(true, gw_cfg.ruuvi_cfg.scan.scan_channel_37);
    ASSERT_EQ(true, gw_cfg.ruuvi_cfg.scan.scan_channel_38);
    ASSERT_EQ(true, gw_cfg.ruuvi_cfg.scan.scan_channel_39);

    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, "Gateway SETTINGS (via HTTP):");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "remote_cfg_use: 0");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "remote_cfg_url: ");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "remote_cfg_auth_type: no");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "remote_cfg_refresh_interval_minutes: 0");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "use_http_ruuvi: 0");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "use_http: 0");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "http_data_format: ruuvi");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "http_auth: none");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "http_url: https://api.ruuvi.com:456/api");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "use_http_stat: 1");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "http_stat_url: https://api.ruuvi.com:456/status");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "http_stat_user: user678");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "http_stat_pass: pass678");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "use_mqtt: 1");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "mqtt_disable_retained_messages: 0");
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
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "lan_auth_type: lan_auth_ruuvi");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "lan_auth_user: user1");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "lan_auth_pass: qwe");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "lan_auth_api_key: ");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "lan_auth_api_key_rw: ");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "auto_update_cycle: beta");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "auto_update_weekdays_bitmask: 126");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "auto_update_interval_from: 1");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "auto_update_interval_to: 23");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "auto_update_tz_offset_hours: 7");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "ntp_use: 1");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "ntp_use_dhcp: 0");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "ntp_server1: time1.server.com");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "ntp_server2: time2.server.com");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "ntp_server3: time3.server.com");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "ntp_server4: time4.server.com");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "company_id: 888");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "company_use_filtering: 1");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "scan_coded_phy: 1");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "scan_1mbit_phy: 1");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "scan_extended_payload: 1");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "scan_channel_37: 1");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "scan_channel_38: 1");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "scan_channel_39: 1");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "coordinates: coord:123,456");
    esp_log_wrapper_clear();
    ASSERT_TRUE(esp_log_wrapper_is_empty());
}

TEST_F(TestJsonRuuvi, json_ruuvi_parse_ntp_disabled_with_full_config) // NOLINT
{
    const string http_body = string(
        "{\n"
        "\t\"remote_cfg_use\":\tfalse,\n"
        "\t\"remote_cfg_url\":\t\"\",\n"
        "\t\"remote_cfg_auth_type\":\t\"no\",\n"
        "\t\"remote_cfg_refresh_interval_minutes\":\t0,\n"

        "\t\"use_http_ruuvi\":\tfalse,\n"
        "\t\"use_http\":\tfalse,\n"
        "\t\"http_url\":\t\"https://api.ruuvi.com:456/api\",\n"
        "\t\"http_data_format\":\t\"ruuvi\",\n"
        "\t\"http_auth\":\t\"none\",\n"

        "\t\"use_http_stat\":\ttrue,\n"
        "\t\"http_stat_url\":\t\"https://api.ruuvi.com:456/status\",\n"
        "\t\"http_stat_user\":\t\"user678\",\n"
        "\t\"http_stat_pass\":\t\"pass678\",\n"

        "\t\"use_mqtt\":\ttrue,\n"
        "\t\"mqtt_disable_retained_messages\":\tfalse,\n"
        "\t\"mqtt_transport\":\t\"TCP\",\n"
        "\t\"mqtt_server\":\t\"mqtt.server.org\",\n"
        "\t\"mqtt_prefix\":\t\"prefix\",\n"
        "\t\"mqtt_client_id\":\t\"AA:BB:CC:DD:EE:FF\",\n"
        "\t\"mqtt_port\":\t1883,\n"
        "\t\"mqtt_user\":\t\"user123\",\n"

        "\t\"lan_auth_type\":\t\"lan_auth_ruuvi\",\n"
        "\t\"lan_auth_user\":\t\"user1\",\n"
        "\t\"lan_auth_pass\":\t\"qwe\",\n"
        "\t\"lan_auth_api_key\":\t\"\",\n"
        "\t\"lan_auth_api_key_rw\":\t\"\",\n"

        "\t\"auto_update_cycle\":\t\"beta\",\n"
        "\t\"auto_update_weekdays_bitmask\":\t126,\n"
        "\t\"auto_update_interval_from\":\t1,\n"
        "\t\"auto_update_interval_to\":\t23,\n"
        "\t\"auto_update_tz_offset_hours\":\t7,\n"

        "\t\"ntp_use\":\tfalse,\n"
        "\t\"ntp_use_dhcp\":\tfalse,\n"
        "\t\"ntp_server1\":\t\"time1.server.com\",\n"
        "\t\"ntp_server2\":\t\"time2.server.com\",\n"
        "\t\"ntp_server3\":\t\"time3.server.com\",\n"
        "\t\"ntp_server4\":\t\"time4.server.com\",\n"

        "\t\"company_use_filtering\":\ttrue,\n"
        "\t\"company_id\":\t888,\n"

        "\t\"coordinates\":\t\"coord:123,456\",\n"

        "\t\"scan_coded_phy\":\ttrue,\n"
        "\t\"scan_1mbit_phy\":\ttrue,\n"
        "\t\"scan_extended_payload\":\ttrue,\n"
        "\t\"scan_channel_37\":\ttrue,\n"
        "\t\"scan_channel_38\":\ttrue,\n"
        "\t\"scan_channel_39\":\ttrue\n"
        "}");

    gw_cfg_t gw_cfg = { 0 };
    gw_cfg_default_get(&gw_cfg);
    bool flag_network_cfg = false;

    ASSERT_TRUE(json_ruuvi_parse_http_body(http_body.c_str(), &gw_cfg, &flag_network_cfg));

    ASSERT_FALSE(flag_network_cfg);
    ASSERT_FALSE(gw_cfg.ruuvi_cfg.remote.use_remote_cfg);
    ASSERT_EQ(string(""), string(gw_cfg.ruuvi_cfg.remote.url.buf));
    ASSERT_EQ(GW_CFG_HTTP_AUTH_TYPE_NONE, gw_cfg.ruuvi_cfg.remote.auth_type);
    ASSERT_EQ(0, gw_cfg.ruuvi_cfg.remote.refresh_interval_minutes);
    ASSERT_TRUE(gw_cfg.ruuvi_cfg.mqtt.use_mqtt);
    ASSERT_FALSE(gw_cfg.ruuvi_cfg.mqtt.mqtt_disable_retained_messages);
    ASSERT_EQ(string("TCP"), gw_cfg.ruuvi_cfg.mqtt.mqtt_transport.buf);
    ASSERT_EQ(string("mqtt.server.org"), gw_cfg.ruuvi_cfg.mqtt.mqtt_server.buf);
    ASSERT_EQ(string("prefix"), gw_cfg.ruuvi_cfg.mqtt.mqtt_prefix.buf);
    ASSERT_EQ(string("AA:BB:CC:DD:EE:FF"), gw_cfg.ruuvi_cfg.mqtt.mqtt_client_id.buf);
    ASSERT_EQ(1883, gw_cfg.ruuvi_cfg.mqtt.mqtt_port);
    ASSERT_EQ(string("user123"), gw_cfg.ruuvi_cfg.mqtt.mqtt_user.buf);
    ASSERT_EQ(string(""), gw_cfg.ruuvi_cfg.mqtt.mqtt_pass.buf);
    ASSERT_FALSE(gw_cfg.ruuvi_cfg.http.use_http_ruuvi);
    ASSERT_FALSE(gw_cfg.ruuvi_cfg.http.use_http);
    ASSERT_EQ(string("https://api.ruuvi.com:456/api"), gw_cfg.ruuvi_cfg.http.http_url.buf);
    ASSERT_EQ(GW_CFG_HTTP_DATA_FORMAT_RUUVI, gw_cfg.ruuvi_cfg.http.data_format);
    ASSERT_EQ(GW_CFG_HTTP_AUTH_TYPE_NONE, gw_cfg.ruuvi_cfg.http.auth_type);
    ASSERT_TRUE(gw_cfg.ruuvi_cfg.http_stat.use_http_stat);
    ASSERT_EQ(string("https://api.ruuvi.com:456/status"), gw_cfg.ruuvi_cfg.http_stat.http_stat_url.buf);
    ASSERT_EQ(string("user678"), gw_cfg.ruuvi_cfg.http_stat.http_stat_user.buf);
    ASSERT_EQ(string("pass678"), gw_cfg.ruuvi_cfg.http_stat.http_stat_pass.buf);
    ASSERT_EQ(HTTP_SERVER_AUTH_TYPE_RUUVI, gw_cfg.ruuvi_cfg.lan_auth.lan_auth_type);
    ASSERT_EQ(string("user1"), gw_cfg.ruuvi_cfg.lan_auth.lan_auth_user.buf);
    ASSERT_EQ(string("qwe"), gw_cfg.ruuvi_cfg.lan_auth.lan_auth_pass.buf);
    ASSERT_EQ(string(""), gw_cfg.ruuvi_cfg.lan_auth.lan_auth_api_key.buf);
    ASSERT_EQ(string(""), gw_cfg.ruuvi_cfg.lan_auth.lan_auth_api_key_rw.buf);
    ASSERT_EQ(false, gw_cfg.ruuvi_cfg.ntp.ntp_use);
    ASSERT_EQ(false, gw_cfg.ruuvi_cfg.ntp.ntp_use_dhcp);
    ASSERT_EQ(string("time.google.com"), string(gw_cfg.ruuvi_cfg.ntp.ntp_server1.buf));
    ASSERT_EQ(string("time.cloudflare.com"), string(gw_cfg.ruuvi_cfg.ntp.ntp_server2.buf));
    ASSERT_EQ(string("time.nist.gov"), string(gw_cfg.ruuvi_cfg.ntp.ntp_server3.buf));
    ASSERT_EQ(string("pool.ntp.org"), string(gw_cfg.ruuvi_cfg.ntp.ntp_server4.buf));
    ASSERT_TRUE(gw_cfg.ruuvi_cfg.filter.company_use_filtering);
    ASSERT_EQ(888, gw_cfg.ruuvi_cfg.filter.company_id);
    ASSERT_EQ(string("coord:123,456"), gw_cfg.ruuvi_cfg.coordinates.buf);
    ASSERT_EQ(true, gw_cfg.ruuvi_cfg.scan.scan_coded_phy);
    ASSERT_EQ(true, gw_cfg.ruuvi_cfg.scan.scan_1mbit_phy);
    ASSERT_EQ(true, gw_cfg.ruuvi_cfg.scan.scan_extended_payload);
    ASSERT_EQ(true, gw_cfg.ruuvi_cfg.scan.scan_channel_37);
    ASSERT_EQ(true, gw_cfg.ruuvi_cfg.scan.scan_channel_38);
    ASSERT_EQ(true, gw_cfg.ruuvi_cfg.scan.scan_channel_39);

    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, "Gateway SETTINGS (via HTTP):");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "remote_cfg_use: 0");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "remote_cfg_url: ");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "remote_cfg_auth_type: no");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "remote_cfg_refresh_interval_minutes: 0");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "use_http_ruuvi: 0");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "use_http: 0");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "http_data_format: ruuvi");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "http_auth: none");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "http_url: https://api.ruuvi.com:456/api");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "use_http_stat: 1");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "http_stat_url: https://api.ruuvi.com:456/status");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "http_stat_user: user678");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "http_stat_pass: pass678");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "use_mqtt: 1");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "mqtt_disable_retained_messages: 0");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "mqtt_transport: TCP");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "mqtt_server: mqtt.server.org");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "mqtt_port: 1883");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "mqtt_prefix: prefix");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "mqtt_client_id: AA:BB:CC:DD:EE:FF");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "mqtt_user: user123");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "mqtt_pass: not found");
    TEST_CHECK_LOG_RECORD_GW_CFG(
        ESP_LOG_INFO,
        "Can't find key 'mqtt_pass' in config-json, leave the previous value unchanged");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "lan_auth_type: lan_auth_ruuvi");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "lan_auth_user: user1");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "lan_auth_pass: qwe");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "lan_auth_api_key: ");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "lan_auth_api_key_rw: ");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "auto_update_cycle: beta");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "auto_update_weekdays_bitmask: 126");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "auto_update_interval_from: 1");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "auto_update_interval_to: 23");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "auto_update_tz_offset_hours: 7");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "ntp_use: 0");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "company_id: 888");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "company_use_filtering: 1");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "scan_coded_phy: 1");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "scan_1mbit_phy: 1");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "scan_extended_payload: 1");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "scan_channel_37: 1");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "scan_channel_38: 1");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "scan_channel_39: 1");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "coordinates: coord:123,456");
    esp_log_wrapper_clear();
    ASSERT_TRUE(esp_log_wrapper_is_empty());
}

TEST_F(TestJsonRuuvi, json_ruuvi_parse_ntp_disabled_with_min_config) // NOLINT
{
    const string http_body = string(
        "{\n"
        "\t\"remote_cfg_use\":\tfalse,\n"
        "\t\"remote_cfg_url\":\t\"\",\n"
        "\t\"remote_cfg_auth_type\":\t\"no\",\n"
        "\t\"remote_cfg_refresh_interval_minutes\":\t0,\n"

        "\t\"use_http_ruuvi\":\tfalse,\n"
        "\t\"use_http\":\tfalse,\n"
        "\t\"http_url\":\t\"https://api.ruuvi.com:456/api\",\n"
        "\t\"http_data_format\":\t\"ruuvi\",\n"
        "\t\"http_auth\":\t\"none\",\n"

        "\t\"use_http_stat\":\ttrue,\n"
        "\t\"http_stat_url\":\t\"https://api.ruuvi.com:456/status\",\n"
        "\t\"http_stat_user\":\t\"user678\",\n"
        "\t\"http_stat_pass\":\t\"pass678\",\n"

        "\t\"use_mqtt\":\ttrue,\n"
        "\t\"mqtt_disable_retained_messages\":\tfalse,\n"
        "\t\"mqtt_transport\":\t\"TCP\",\n"
        "\t\"mqtt_server\":\t\"mqtt.server.org\",\n"
        "\t\"mqtt_prefix\":\t\"prefix\",\n"
        "\t\"mqtt_client_id\":\t\"AA:BB:CC:DD:EE:FF\",\n"
        "\t\"mqtt_port\":\t1883,\n"
        "\t\"mqtt_user\":\t\"user123\",\n"

        "\t\"lan_auth_type\":\t\"lan_auth_ruuvi\",\n"
        "\t\"lan_auth_user\":\t\"user1\",\n"
        "\t\"lan_auth_pass\":\t\"qwe\",\n"
        "\t\"lan_auth_api_key\":\t\"\",\n"
        "\t\"lan_auth_api_key_rw\":\t\"\",\n"

        "\t\"auto_update_cycle\":\t\"beta\",\n"
        "\t\"auto_update_weekdays_bitmask\":\t126,\n"
        "\t\"auto_update_interval_from\":\t1,\n"
        "\t\"auto_update_interval_to\":\t23,\n"
        "\t\"auto_update_tz_offset_hours\":\t7,\n"

        "\t\"ntp_use\":\tfalse,\n"

        "\t\"company_use_filtering\":\ttrue,\n"
        "\t\"company_id\":\t888,\n"

        "\t\"coordinates\":\t\"coord:123,456\",\n"

        "\t\"scan_coded_phy\":\ttrue,\n"
        "\t\"scan_1mbit_phy\":\ttrue,\n"
        "\t\"scan_extended_payload\":\ttrue,\n"
        "\t\"scan_channel_37\":\ttrue,\n"
        "\t\"scan_channel_38\":\ttrue,\n"
        "\t\"scan_channel_39\":\ttrue\n"
        "}");

    gw_cfg_t gw_cfg = { 0 };
    gw_cfg_default_get(&gw_cfg);
    bool flag_network_cfg = false;

    ASSERT_TRUE(json_ruuvi_parse_http_body(http_body.c_str(), &gw_cfg, &flag_network_cfg));

    ASSERT_FALSE(flag_network_cfg);
    ASSERT_FALSE(gw_cfg.ruuvi_cfg.remote.use_remote_cfg);
    ASSERT_EQ(string(""), string(gw_cfg.ruuvi_cfg.remote.url.buf));
    ASSERT_EQ(GW_CFG_HTTP_AUTH_TYPE_NONE, gw_cfg.ruuvi_cfg.remote.auth_type);
    ASSERT_EQ(0, gw_cfg.ruuvi_cfg.remote.refresh_interval_minutes);
    ASSERT_TRUE(gw_cfg.ruuvi_cfg.mqtt.use_mqtt);
    ASSERT_FALSE(gw_cfg.ruuvi_cfg.mqtt.mqtt_disable_retained_messages);
    ASSERT_EQ(string("TCP"), gw_cfg.ruuvi_cfg.mqtt.mqtt_transport.buf);
    ASSERT_EQ(string("mqtt.server.org"), gw_cfg.ruuvi_cfg.mqtt.mqtt_server.buf);
    ASSERT_EQ(string("prefix"), gw_cfg.ruuvi_cfg.mqtt.mqtt_prefix.buf);
    ASSERT_EQ(string("AA:BB:CC:DD:EE:FF"), gw_cfg.ruuvi_cfg.mqtt.mqtt_client_id.buf);
    ASSERT_EQ(1883, gw_cfg.ruuvi_cfg.mqtt.mqtt_port);
    ASSERT_EQ(string("user123"), gw_cfg.ruuvi_cfg.mqtt.mqtt_user.buf);
    ASSERT_EQ(string(""), gw_cfg.ruuvi_cfg.mqtt.mqtt_pass.buf);
    ASSERT_FALSE(gw_cfg.ruuvi_cfg.http.use_http_ruuvi);
    ASSERT_FALSE(gw_cfg.ruuvi_cfg.http.use_http);
    ASSERT_EQ(string("https://api.ruuvi.com:456/api"), gw_cfg.ruuvi_cfg.http.http_url.buf);
    ASSERT_EQ(GW_CFG_HTTP_DATA_FORMAT_RUUVI, gw_cfg.ruuvi_cfg.http.data_format);
    ASSERT_EQ(GW_CFG_HTTP_AUTH_TYPE_NONE, gw_cfg.ruuvi_cfg.http.auth_type);
    ASSERT_TRUE(gw_cfg.ruuvi_cfg.http_stat.use_http_stat);
    ASSERT_EQ(string("https://api.ruuvi.com:456/status"), gw_cfg.ruuvi_cfg.http_stat.http_stat_url.buf);
    ASSERT_EQ(string("user678"), gw_cfg.ruuvi_cfg.http_stat.http_stat_user.buf);
    ASSERT_EQ(string("pass678"), gw_cfg.ruuvi_cfg.http_stat.http_stat_pass.buf);
    ASSERT_EQ(HTTP_SERVER_AUTH_TYPE_RUUVI, gw_cfg.ruuvi_cfg.lan_auth.lan_auth_type);
    ASSERT_EQ(string("user1"), gw_cfg.ruuvi_cfg.lan_auth.lan_auth_user.buf);
    ASSERT_EQ(string("qwe"), gw_cfg.ruuvi_cfg.lan_auth.lan_auth_pass.buf);
    ASSERT_EQ(string(""), gw_cfg.ruuvi_cfg.lan_auth.lan_auth_api_key.buf);
    ASSERT_EQ(string(""), gw_cfg.ruuvi_cfg.lan_auth.lan_auth_api_key_rw.buf);
    ASSERT_EQ(false, gw_cfg.ruuvi_cfg.ntp.ntp_use);
    ASSERT_EQ(false, gw_cfg.ruuvi_cfg.ntp.ntp_use_dhcp);
    ASSERT_EQ(string("time.google.com"), string(gw_cfg.ruuvi_cfg.ntp.ntp_server1.buf));
    ASSERT_EQ(string("time.cloudflare.com"), string(gw_cfg.ruuvi_cfg.ntp.ntp_server2.buf));
    ASSERT_EQ(string("time.nist.gov"), string(gw_cfg.ruuvi_cfg.ntp.ntp_server3.buf));
    ASSERT_EQ(string("pool.ntp.org"), string(gw_cfg.ruuvi_cfg.ntp.ntp_server4.buf));
    ASSERT_TRUE(gw_cfg.ruuvi_cfg.filter.company_use_filtering);
    ASSERT_EQ(888, gw_cfg.ruuvi_cfg.filter.company_id);
    ASSERT_EQ(string("coord:123,456"), gw_cfg.ruuvi_cfg.coordinates.buf);
    ASSERT_EQ(true, gw_cfg.ruuvi_cfg.scan.scan_coded_phy);
    ASSERT_EQ(true, gw_cfg.ruuvi_cfg.scan.scan_1mbit_phy);
    ASSERT_EQ(true, gw_cfg.ruuvi_cfg.scan.scan_extended_payload);
    ASSERT_EQ(true, gw_cfg.ruuvi_cfg.scan.scan_channel_37);
    ASSERT_EQ(true, gw_cfg.ruuvi_cfg.scan.scan_channel_38);
    ASSERT_EQ(true, gw_cfg.ruuvi_cfg.scan.scan_channel_39);

    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, "Gateway SETTINGS (via HTTP):");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "remote_cfg_use: 0");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "remote_cfg_url: ");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "remote_cfg_auth_type: no");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "remote_cfg_refresh_interval_minutes: 0");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "use_http_ruuvi: 0");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "use_http: 0");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "http_data_format: ruuvi");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "http_auth: none");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "http_url: https://api.ruuvi.com:456/api");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "use_http_stat: 1");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "http_stat_url: https://api.ruuvi.com:456/status");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "http_stat_user: user678");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "http_stat_pass: pass678");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "use_mqtt: 1");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "mqtt_disable_retained_messages: 0");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "mqtt_transport: TCP");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "mqtt_server: mqtt.server.org");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "mqtt_port: 1883");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "mqtt_prefix: prefix");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "mqtt_client_id: AA:BB:CC:DD:EE:FF");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "mqtt_user: user123");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "mqtt_pass: not found");
    TEST_CHECK_LOG_RECORD_GW_CFG(
        ESP_LOG_INFO,
        "Can't find key 'mqtt_pass' in config-json, leave the previous value unchanged");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "lan_auth_type: lan_auth_ruuvi");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "lan_auth_user: user1");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "lan_auth_pass: qwe");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "lan_auth_api_key: ");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "lan_auth_api_key_rw: ");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "auto_update_cycle: beta");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "auto_update_weekdays_bitmask: 126");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "auto_update_interval_from: 1");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "auto_update_interval_to: 23");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "auto_update_tz_offset_hours: 7");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "ntp_use: 0");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "company_id: 888");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "company_use_filtering: 1");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "scan_coded_phy: 1");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "scan_1mbit_phy: 1");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "scan_extended_payload: 1");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "scan_channel_37: 1");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "scan_channel_38: 1");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "scan_channel_39: 1");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "coordinates: coord:123,456");
    esp_log_wrapper_clear();
    ASSERT_TRUE(esp_log_wrapper_is_empty());
}

TEST_F(TestJsonRuuvi, json_ruuvi_parse_ntp_enabled_via_dhcp) // NOLINT
{
    const string http_body = string(
        "{\n"
        "\t\"remote_cfg_use\":\tfalse,\n"
        "\t\"remote_cfg_url\":\t\"\",\n"
        "\t\"remote_cfg_auth_type\":\t\"no\",\n"
        "\t\"remote_cfg_refresh_interval_minutes\":\t0,\n"

        "\t\"use_http_ruuvi\":\tfalse,\n"
        "\t\"use_http\":\tfalse,\n"
        "\t\"http_url\":\t\"https://api.ruuvi.com:456/api\",\n"
        "\t\"http_data_format\":\t\"ruuvi\",\n"
        "\t\"http_auth\":\t\"none\",\n"

        "\t\"use_http_stat\":\ttrue,\n"
        "\t\"http_stat_url\":\t\"https://api.ruuvi.com:456/status\",\n"
        "\t\"http_stat_user\":\t\"user678\",\n"
        "\t\"http_stat_pass\":\t\"pass678\",\n"

        "\t\"use_mqtt\":\ttrue,\n"
        "\t\"mqtt_disable_retained_messages\":\tfalse,\n"
        "\t\"mqtt_transport\":\t\"TCP\",\n"
        "\t\"mqtt_server\":\t\"mqtt.server.org\",\n"
        "\t\"mqtt_prefix\":\t\"prefix\",\n"
        "\t\"mqtt_client_id\":\t\"AA:BB:CC:DD:EE:FF\",\n"
        "\t\"mqtt_port\":\t1883,\n"
        "\t\"mqtt_user\":\t\"user123\",\n"

        "\t\"lan_auth_type\":\t\"lan_auth_ruuvi\",\n"
        "\t\"lan_auth_user\":\t\"user1\",\n"
        "\t\"lan_auth_pass\":\t\"qwe\",\n"
        "\t\"lan_auth_api_key\":\t\"\",\n"
        "\t\"lan_auth_api_key_rw\":\t\"\",\n"

        "\t\"auto_update_cycle\":\t\"beta\",\n"
        "\t\"auto_update_weekdays_bitmask\":\t126,\n"
        "\t\"auto_update_interval_from\":\t1,\n"
        "\t\"auto_update_interval_to\":\t23,\n"
        "\t\"auto_update_tz_offset_hours\":\t7,\n"

        "\t\"ntp_use\":\ttrue,\n"
        "\t\"ntp_use_dhcp\":\ttrue,\n"

        "\t\"company_use_filtering\":\ttrue,\n"
        "\t\"company_id\":\t888,\n"

        "\t\"coordinates\":\t\"coord:123,456\",\n"

        "\t\"scan_coded_phy\":\ttrue,\n"
        "\t\"scan_1mbit_phy\":\ttrue,\n"
        "\t\"scan_extended_payload\":\ttrue,\n"
        "\t\"scan_channel_37\":\ttrue,\n"
        "\t\"scan_channel_38\":\ttrue,\n"
        "\t\"scan_channel_39\":\ttrue\n"
        "}");

    gw_cfg_t gw_cfg = { 0 };
    gw_cfg_default_get(&gw_cfg);
    bool flag_network_cfg = false;

    ASSERT_TRUE(json_ruuvi_parse_http_body(http_body.c_str(), &gw_cfg, &flag_network_cfg));

    ASSERT_FALSE(flag_network_cfg);
    ASSERT_FALSE(gw_cfg.ruuvi_cfg.remote.use_remote_cfg);
    ASSERT_EQ(string(""), string(gw_cfg.ruuvi_cfg.remote.url.buf));
    ASSERT_EQ(GW_CFG_HTTP_AUTH_TYPE_NONE, gw_cfg.ruuvi_cfg.remote.auth_type);
    ASSERT_EQ(0, gw_cfg.ruuvi_cfg.remote.refresh_interval_minutes);
    ASSERT_TRUE(gw_cfg.ruuvi_cfg.mqtt.use_mqtt);
    ASSERT_FALSE(gw_cfg.ruuvi_cfg.mqtt.mqtt_disable_retained_messages);
    ASSERT_EQ(string("TCP"), gw_cfg.ruuvi_cfg.mqtt.mqtt_transport.buf);
    ASSERT_EQ(string("mqtt.server.org"), gw_cfg.ruuvi_cfg.mqtt.mqtt_server.buf);
    ASSERT_EQ(string("prefix"), gw_cfg.ruuvi_cfg.mqtt.mqtt_prefix.buf);
    ASSERT_EQ(string("AA:BB:CC:DD:EE:FF"), gw_cfg.ruuvi_cfg.mqtt.mqtt_client_id.buf);
    ASSERT_EQ(1883, gw_cfg.ruuvi_cfg.mqtt.mqtt_port);
    ASSERT_EQ(string("user123"), gw_cfg.ruuvi_cfg.mqtt.mqtt_user.buf);
    ASSERT_EQ(string(""), gw_cfg.ruuvi_cfg.mqtt.mqtt_pass.buf);
    ASSERT_FALSE(gw_cfg.ruuvi_cfg.http.use_http_ruuvi);
    ASSERT_FALSE(gw_cfg.ruuvi_cfg.http.use_http);
    ASSERT_EQ(string("https://api.ruuvi.com:456/api"), gw_cfg.ruuvi_cfg.http.http_url.buf);
    ASSERT_EQ(GW_CFG_HTTP_DATA_FORMAT_RUUVI, gw_cfg.ruuvi_cfg.http.data_format);
    ASSERT_EQ(GW_CFG_HTTP_AUTH_TYPE_NONE, gw_cfg.ruuvi_cfg.http.auth_type);
    ASSERT_TRUE(gw_cfg.ruuvi_cfg.http_stat.use_http_stat);
    ASSERT_EQ(string("https://api.ruuvi.com:456/status"), gw_cfg.ruuvi_cfg.http_stat.http_stat_url.buf);
    ASSERT_EQ(string("user678"), gw_cfg.ruuvi_cfg.http_stat.http_stat_user.buf);
    ASSERT_EQ(string("pass678"), gw_cfg.ruuvi_cfg.http_stat.http_stat_pass.buf);
    ASSERT_EQ(HTTP_SERVER_AUTH_TYPE_RUUVI, gw_cfg.ruuvi_cfg.lan_auth.lan_auth_type);
    ASSERT_EQ(string("user1"), gw_cfg.ruuvi_cfg.lan_auth.lan_auth_user.buf);
    ASSERT_EQ(string("qwe"), gw_cfg.ruuvi_cfg.lan_auth.lan_auth_pass.buf);
    ASSERT_EQ(string(""), gw_cfg.ruuvi_cfg.lan_auth.lan_auth_api_key.buf);
    ASSERT_EQ(string(""), gw_cfg.ruuvi_cfg.lan_auth.lan_auth_api_key_rw.buf);
    ASSERT_EQ(true, gw_cfg.ruuvi_cfg.ntp.ntp_use);
    ASSERT_EQ(true, gw_cfg.ruuvi_cfg.ntp.ntp_use_dhcp);
    ASSERT_EQ(string("time.google.com"), string(gw_cfg.ruuvi_cfg.ntp.ntp_server1.buf));
    ASSERT_EQ(string("time.cloudflare.com"), string(gw_cfg.ruuvi_cfg.ntp.ntp_server2.buf));
    ASSERT_EQ(string("time.nist.gov"), string(gw_cfg.ruuvi_cfg.ntp.ntp_server3.buf));
    ASSERT_EQ(string("pool.ntp.org"), string(gw_cfg.ruuvi_cfg.ntp.ntp_server4.buf));
    ASSERT_TRUE(gw_cfg.ruuvi_cfg.filter.company_use_filtering);
    ASSERT_EQ(888, gw_cfg.ruuvi_cfg.filter.company_id);
    ASSERT_EQ(string("coord:123,456"), gw_cfg.ruuvi_cfg.coordinates.buf);
    ASSERT_EQ(true, gw_cfg.ruuvi_cfg.scan.scan_coded_phy);
    ASSERT_EQ(true, gw_cfg.ruuvi_cfg.scan.scan_1mbit_phy);
    ASSERT_EQ(true, gw_cfg.ruuvi_cfg.scan.scan_extended_payload);
    ASSERT_EQ(true, gw_cfg.ruuvi_cfg.scan.scan_channel_37);
    ASSERT_EQ(true, gw_cfg.ruuvi_cfg.scan.scan_channel_38);
    ASSERT_EQ(true, gw_cfg.ruuvi_cfg.scan.scan_channel_39);

    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, "Gateway SETTINGS (via HTTP):");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "remote_cfg_use: 0");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "remote_cfg_url: ");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "remote_cfg_auth_type: no");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "remote_cfg_refresh_interval_minutes: 0");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "use_http_ruuvi: 0");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "use_http: 0");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "http_data_format: ruuvi");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "http_auth: none");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "http_url: https://api.ruuvi.com:456/api");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "use_http_stat: 1");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "http_stat_url: https://api.ruuvi.com:456/status");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "http_stat_user: user678");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "http_stat_pass: pass678");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "use_mqtt: 1");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "mqtt_disable_retained_messages: 0");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "mqtt_transport: TCP");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "mqtt_server: mqtt.server.org");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "mqtt_port: 1883");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "mqtt_prefix: prefix");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "mqtt_client_id: AA:BB:CC:DD:EE:FF");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "mqtt_user: user123");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "mqtt_pass: not found");
    TEST_CHECK_LOG_RECORD_GW_CFG(
        ESP_LOG_INFO,
        "Can't find key 'mqtt_pass' in config-json, leave the previous value unchanged");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "lan_auth_type: lan_auth_ruuvi");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "lan_auth_user: user1");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "lan_auth_pass: qwe");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "lan_auth_api_key: ");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "lan_auth_api_key_rw: ");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "auto_update_cycle: beta");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "auto_update_weekdays_bitmask: 126");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "auto_update_interval_from: 1");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "auto_update_interval_to: 23");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "auto_update_tz_offset_hours: 7");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "ntp_use: 1");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "ntp_use_dhcp: 1");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "company_id: 888");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "company_use_filtering: 1");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "scan_coded_phy: 1");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "scan_1mbit_phy: 1");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "scan_extended_payload: 1");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "scan_channel_37: 1");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "scan_channel_38: 1");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "scan_channel_39: 1");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "coordinates: coord:123,456");
    esp_log_wrapper_clear();
    ASSERT_TRUE(esp_log_wrapper_is_empty());
}

TEST_F(TestJsonRuuvi, json_ruuvi_parse_ntp_enabled_custom) // NOLINT
{
    const string http_body = string(
        "{\n"
        "\t\"remote_cfg_use\":\tfalse,\n"
        "\t\"remote_cfg_url\":\t\"\",\n"
        "\t\"remote_cfg_auth_type\":\t\"no\",\n"
        "\t\"remote_cfg_refresh_interval_minutes\":\t0,\n"

        "\t\"use_http_ruuvi\":\tfalse,\n"
        "\t\"use_http\":\tfalse,\n"
        "\t\"http_url\":\t\"https://api.ruuvi.com:456/api\",\n"
        "\t\"http_data_format\":\t\"ruuvi\",\n"
        "\t\"http_auth\":\t\"none\",\n"

        "\t\"use_http_stat\":\ttrue,\n"
        "\t\"http_stat_url\":\t\"https://api.ruuvi.com:456/status\",\n"
        "\t\"http_stat_user\":\t\"user678\",\n"
        "\t\"http_stat_pass\":\t\"pass678\",\n"

        "\t\"use_mqtt\":\ttrue,\n"
        "\t\"mqtt_disable_retained_messages\":\tfalse,\n"
        "\t\"mqtt_transport\":\t\"TCP\",\n"
        "\t\"mqtt_server\":\t\"mqtt.server.org\",\n"
        "\t\"mqtt_prefix\":\t\"prefix\",\n"
        "\t\"mqtt_client_id\":\t\"AA:BB:CC:DD:EE:FF\",\n"
        "\t\"mqtt_port\":\t1883,\n"
        "\t\"mqtt_user\":\t\"user123\",\n"

        "\t\"lan_auth_type\":\t\"lan_auth_ruuvi\",\n"
        "\t\"lan_auth_user\":\t\"user1\",\n"
        "\t\"lan_auth_pass\":\t\"qwe\",\n"
        "\t\"lan_auth_api_key\":\t\"\",\n"
        "\t\"lan_auth_api_key_rw\":\t\"\",\n"

        "\t\"auto_update_cycle\":\t\"beta\",\n"
        "\t\"auto_update_weekdays_bitmask\":\t126,\n"
        "\t\"auto_update_interval_from\":\t1,\n"
        "\t\"auto_update_interval_to\":\t23,\n"
        "\t\"auto_update_tz_offset_hours\":\t7,\n"

        "\t\"ntp_use\":\ttrue,\n"
        "\t\"ntp_use_dhcp\":\tfalse,\n"
        "\t\"ntp_server1\":\t\"time1.server.com\",\n"
        "\t\"ntp_server2\":\t\"time2.server.com\",\n"

        "\t\"company_use_filtering\":\ttrue,\n"
        "\t\"company_id\":\t888,\n"

        "\t\"coordinates\":\t\"coord:123,456\",\n"

        "\t\"scan_coded_phy\":\ttrue,\n"
        "\t\"scan_1mbit_phy\":\ttrue,\n"
        "\t\"scan_extended_payload\":\ttrue,\n"
        "\t\"scan_channel_37\":\ttrue,\n"
        "\t\"scan_channel_38\":\ttrue,\n"
        "\t\"scan_channel_39\":\ttrue\n"
        "}");

    gw_cfg_t gw_cfg = { 0 };
    gw_cfg_default_get(&gw_cfg);
    bool flag_network_cfg = false;

    ASSERT_TRUE(json_ruuvi_parse_http_body(http_body.c_str(), &gw_cfg, &flag_network_cfg));

    ASSERT_FALSE(flag_network_cfg);
    ASSERT_FALSE(gw_cfg.ruuvi_cfg.remote.use_remote_cfg);
    ASSERT_EQ(string(""), string(gw_cfg.ruuvi_cfg.remote.url.buf));
    ASSERT_EQ(GW_CFG_HTTP_AUTH_TYPE_NONE, gw_cfg.ruuvi_cfg.remote.auth_type);
    ASSERT_EQ(0, gw_cfg.ruuvi_cfg.remote.refresh_interval_minutes);
    ASSERT_TRUE(gw_cfg.ruuvi_cfg.mqtt.use_mqtt);
    ASSERT_FALSE(gw_cfg.ruuvi_cfg.mqtt.mqtt_disable_retained_messages);
    ASSERT_EQ(string("TCP"), gw_cfg.ruuvi_cfg.mqtt.mqtt_transport.buf);
    ASSERT_EQ(string("mqtt.server.org"), gw_cfg.ruuvi_cfg.mqtt.mqtt_server.buf);
    ASSERT_EQ(string("prefix"), gw_cfg.ruuvi_cfg.mqtt.mqtt_prefix.buf);
    ASSERT_EQ(string("AA:BB:CC:DD:EE:FF"), gw_cfg.ruuvi_cfg.mqtt.mqtt_client_id.buf);
    ASSERT_EQ(1883, gw_cfg.ruuvi_cfg.mqtt.mqtt_port);
    ASSERT_EQ(string("user123"), gw_cfg.ruuvi_cfg.mqtt.mqtt_user.buf);
    ASSERT_EQ(string(""), gw_cfg.ruuvi_cfg.mqtt.mqtt_pass.buf);
    ASSERT_FALSE(gw_cfg.ruuvi_cfg.http.use_http_ruuvi);
    ASSERT_FALSE(gw_cfg.ruuvi_cfg.http.use_http);
    ASSERT_EQ(string("https://api.ruuvi.com:456/api"), gw_cfg.ruuvi_cfg.http.http_url.buf);
    ASSERT_EQ(GW_CFG_HTTP_DATA_FORMAT_RUUVI, gw_cfg.ruuvi_cfg.http.data_format);
    ASSERT_EQ(GW_CFG_HTTP_AUTH_TYPE_NONE, gw_cfg.ruuvi_cfg.http.auth_type);
    ASSERT_TRUE(gw_cfg.ruuvi_cfg.http_stat.use_http_stat);
    ASSERT_EQ(string("https://api.ruuvi.com:456/status"), gw_cfg.ruuvi_cfg.http_stat.http_stat_url.buf);
    ASSERT_EQ(string("user678"), gw_cfg.ruuvi_cfg.http_stat.http_stat_user.buf);
    ASSERT_EQ(string("pass678"), gw_cfg.ruuvi_cfg.http_stat.http_stat_pass.buf);
    ASSERT_EQ(HTTP_SERVER_AUTH_TYPE_RUUVI, gw_cfg.ruuvi_cfg.lan_auth.lan_auth_type);
    ASSERT_EQ(string("user1"), gw_cfg.ruuvi_cfg.lan_auth.lan_auth_user.buf);
    ASSERT_EQ(string("qwe"), gw_cfg.ruuvi_cfg.lan_auth.lan_auth_pass.buf);
    ASSERT_EQ(string(""), gw_cfg.ruuvi_cfg.lan_auth.lan_auth_api_key.buf);
    ASSERT_EQ(string(""), gw_cfg.ruuvi_cfg.lan_auth.lan_auth_api_key_rw.buf);
    ASSERT_EQ(true, gw_cfg.ruuvi_cfg.ntp.ntp_use);
    ASSERT_EQ(false, gw_cfg.ruuvi_cfg.ntp.ntp_use_dhcp);
    ASSERT_EQ(string("time1.server.com"), string(gw_cfg.ruuvi_cfg.ntp.ntp_server1.buf));
    ASSERT_EQ(string("time2.server.com"), string(gw_cfg.ruuvi_cfg.ntp.ntp_server2.buf));
    ASSERT_EQ(string("time.nist.gov"), string(gw_cfg.ruuvi_cfg.ntp.ntp_server3.buf));
    ASSERT_EQ(string("pool.ntp.org"), string(gw_cfg.ruuvi_cfg.ntp.ntp_server4.buf));
    ASSERT_TRUE(gw_cfg.ruuvi_cfg.filter.company_use_filtering);
    ASSERT_EQ(888, gw_cfg.ruuvi_cfg.filter.company_id);
    ASSERT_EQ(string("coord:123,456"), gw_cfg.ruuvi_cfg.coordinates.buf);
    ASSERT_EQ(true, gw_cfg.ruuvi_cfg.scan.scan_coded_phy);
    ASSERT_EQ(true, gw_cfg.ruuvi_cfg.scan.scan_1mbit_phy);
    ASSERT_EQ(true, gw_cfg.ruuvi_cfg.scan.scan_extended_payload);
    ASSERT_EQ(true, gw_cfg.ruuvi_cfg.scan.scan_channel_37);
    ASSERT_EQ(true, gw_cfg.ruuvi_cfg.scan.scan_channel_38);
    ASSERT_EQ(true, gw_cfg.ruuvi_cfg.scan.scan_channel_39);

    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, "Gateway SETTINGS (via HTTP):");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "remote_cfg_use: 0");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "remote_cfg_url: ");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "remote_cfg_auth_type: no");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "remote_cfg_refresh_interval_minutes: 0");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "use_http_ruuvi: 0");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "use_http: 0");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "http_data_format: ruuvi");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "http_auth: none");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "http_url: https://api.ruuvi.com:456/api");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "use_http_stat: 1");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "http_stat_url: https://api.ruuvi.com:456/status");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "http_stat_user: user678");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "http_stat_pass: pass678");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "use_mqtt: 1");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "mqtt_disable_retained_messages: 0");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "mqtt_transport: TCP");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "mqtt_server: mqtt.server.org");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "mqtt_port: 1883");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "mqtt_prefix: prefix");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "mqtt_client_id: AA:BB:CC:DD:EE:FF");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "mqtt_user: user123");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "mqtt_pass: not found");
    TEST_CHECK_LOG_RECORD_GW_CFG(
        ESP_LOG_INFO,
        "Can't find key 'mqtt_pass' in config-json, leave the previous value unchanged");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "lan_auth_type: lan_auth_ruuvi");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "lan_auth_user: user1");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "lan_auth_pass: qwe");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "lan_auth_api_key: ");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "lan_auth_api_key_rw: ");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "auto_update_cycle: beta");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "auto_update_weekdays_bitmask: 126");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "auto_update_interval_from: 1");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "auto_update_interval_to: 23");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "auto_update_tz_offset_hours: 7");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "ntp_use: 1");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "ntp_use_dhcp: 0");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "ntp_server1: time1.server.com");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "ntp_server2: time2.server.com");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "ntp_server3: not found");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_WARN, "Can't find key 'ntp_server3' in config-json");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "ntp_server4: not found");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_WARN, "Can't find key 'ntp_server4' in config-json");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "company_id: 888");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "company_use_filtering: 1");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "scan_coded_phy: 1");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "scan_1mbit_phy: 1");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "scan_extended_payload: 1");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "scan_channel_37: 1");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "scan_channel_38: 1");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "scan_channel_39: 1");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "coordinates: coord:123,456");
    esp_log_wrapper_clear();
    ASSERT_TRUE(esp_log_wrapper_is_empty());
}

TEST_F(TestJsonRuuvi, json_ruuvi_parse_ntp_enabled_default) // NOLINT
{
    const string http_body = string(
        "{\n"
        "\t\"remote_cfg_use\":\tfalse,\n"
        "\t\"remote_cfg_url\":\t\"\",\n"
        "\t\"remote_cfg_auth_type\":\t\"no\",\n"
        "\t\"remote_cfg_refresh_interval_minutes\":\t0,\n"

        "\t\"use_http_ruuvi\":\tfalse,\n"
        "\t\"use_http\":\tfalse,\n"
        "\t\"http_url\":\t\"https://api.ruuvi.com:456/api\",\n"
        "\t\"http_data_format\":\t\"ruuvi\",\n"
        "\t\"http_auth\":\t\"none\",\n"

        "\t\"use_http_stat\":\ttrue,\n"
        "\t\"http_stat_url\":\t\"https://api.ruuvi.com:456/status\",\n"
        "\t\"http_stat_user\":\t\"user678\",\n"
        "\t\"http_stat_pass\":\t\"pass678\",\n"

        "\t\"use_mqtt\":\ttrue,\n"
        "\t\"mqtt_disable_retained_messages\":\tfalse,\n"
        "\t\"mqtt_transport\":\t\"TCP\",\n"
        "\t\"mqtt_server\":\t\"mqtt.server.org\",\n"
        "\t\"mqtt_prefix\":\t\"prefix\",\n"
        "\t\"mqtt_client_id\":\t\"AA:BB:CC:DD:EE:FF\",\n"
        "\t\"mqtt_port\":\t1883,\n"
        "\t\"mqtt_user\":\t\"user123\",\n"

        "\t\"lan_auth_type\":\t\"lan_auth_ruuvi\",\n"
        "\t\"lan_auth_user\":\t\"user1\",\n"
        "\t\"lan_auth_pass\":\t\"qwe\",\n"
        "\t\"lan_auth_api_key\":\t\"\",\n"
        "\t\"lan_auth_api_key_rw\":\t\"\",\n"

        "\t\"auto_update_cycle\":\t\"beta\",\n"
        "\t\"auto_update_weekdays_bitmask\":\t126,\n"
        "\t\"auto_update_interval_from\":\t1,\n"
        "\t\"auto_update_interval_to\":\t23,\n"
        "\t\"auto_update_tz_offset_hours\":\t7,\n"

        "\t\"ntp_use\":\ttrue,\n"
        "\t\"ntp_use_dhcp\":\tfalse,\n"
        "\t\"ntp_server1\":\t\"time.google.com\",\n"
        "\t\"ntp_server2\":\t\"time.cloudflare.com\",\n"
        "\t\"ntp_server3\":\t\"time.nist.gov\",\n"
        "\t\"ntp_server4\":\t\"pool.ntp.org\",\n"

        "\t\"company_use_filtering\":\ttrue,\n"
        "\t\"company_id\":\t888,\n"

        "\t\"coordinates\":\t\"coord:123,456\",\n"

        "\t\"scan_coded_phy\":\ttrue,\n"
        "\t\"scan_1mbit_phy\":\ttrue,\n"
        "\t\"scan_extended_payload\":\ttrue,\n"
        "\t\"scan_channel_37\":\ttrue,\n"
        "\t\"scan_channel_38\":\ttrue,\n"
        "\t\"scan_channel_39\":\ttrue\n"
        "}");

    gw_cfg_t gw_cfg = { 0 };
    gw_cfg_default_get(&gw_cfg);
    bool flag_network_cfg = false;

    ASSERT_TRUE(json_ruuvi_parse_http_body(http_body.c_str(), &gw_cfg, &flag_network_cfg));

    ASSERT_FALSE(flag_network_cfg);
    ASSERT_FALSE(gw_cfg.ruuvi_cfg.remote.use_remote_cfg);
    ASSERT_EQ(string(""), string(gw_cfg.ruuvi_cfg.remote.url.buf));
    ASSERT_EQ(GW_CFG_HTTP_AUTH_TYPE_NONE, gw_cfg.ruuvi_cfg.remote.auth_type);
    ASSERT_EQ(0, gw_cfg.ruuvi_cfg.remote.refresh_interval_minutes);
    ASSERT_TRUE(gw_cfg.ruuvi_cfg.mqtt.use_mqtt);
    ASSERT_FALSE(gw_cfg.ruuvi_cfg.mqtt.mqtt_disable_retained_messages);
    ASSERT_EQ(string("TCP"), gw_cfg.ruuvi_cfg.mqtt.mqtt_transport.buf);
    ASSERT_EQ(string("mqtt.server.org"), gw_cfg.ruuvi_cfg.mqtt.mqtt_server.buf);
    ASSERT_EQ(string("prefix"), gw_cfg.ruuvi_cfg.mqtt.mqtt_prefix.buf);
    ASSERT_EQ(string("AA:BB:CC:DD:EE:FF"), gw_cfg.ruuvi_cfg.mqtt.mqtt_client_id.buf);
    ASSERT_EQ(1883, gw_cfg.ruuvi_cfg.mqtt.mqtt_port);
    ASSERT_EQ(string("user123"), gw_cfg.ruuvi_cfg.mqtt.mqtt_user.buf);
    ASSERT_EQ(string(""), gw_cfg.ruuvi_cfg.mqtt.mqtt_pass.buf);
    ASSERT_FALSE(gw_cfg.ruuvi_cfg.http.use_http_ruuvi);
    ASSERT_FALSE(gw_cfg.ruuvi_cfg.http.use_http);
    ASSERT_EQ(string("https://api.ruuvi.com:456/api"), gw_cfg.ruuvi_cfg.http.http_url.buf);
    ASSERT_EQ(GW_CFG_HTTP_DATA_FORMAT_RUUVI, gw_cfg.ruuvi_cfg.http.data_format);
    ASSERT_EQ(GW_CFG_HTTP_AUTH_TYPE_NONE, gw_cfg.ruuvi_cfg.http.auth_type);
    ASSERT_TRUE(gw_cfg.ruuvi_cfg.http_stat.use_http_stat);
    ASSERT_EQ(string("https://api.ruuvi.com:456/status"), gw_cfg.ruuvi_cfg.http_stat.http_stat_url.buf);
    ASSERT_EQ(string("user678"), gw_cfg.ruuvi_cfg.http_stat.http_stat_user.buf);
    ASSERT_EQ(string("pass678"), gw_cfg.ruuvi_cfg.http_stat.http_stat_pass.buf);
    ASSERT_EQ(HTTP_SERVER_AUTH_TYPE_RUUVI, gw_cfg.ruuvi_cfg.lan_auth.lan_auth_type);
    ASSERT_EQ(string("user1"), gw_cfg.ruuvi_cfg.lan_auth.lan_auth_user.buf);
    ASSERT_EQ(string("qwe"), gw_cfg.ruuvi_cfg.lan_auth.lan_auth_pass.buf);
    ASSERT_EQ(string(""), gw_cfg.ruuvi_cfg.lan_auth.lan_auth_api_key.buf);
    ASSERT_EQ(string(""), gw_cfg.ruuvi_cfg.lan_auth.lan_auth_api_key_rw.buf);
    ASSERT_EQ(true, gw_cfg.ruuvi_cfg.ntp.ntp_use);
    ASSERT_EQ(false, gw_cfg.ruuvi_cfg.ntp.ntp_use_dhcp);
    ASSERT_EQ(string("time.google.com"), string(gw_cfg.ruuvi_cfg.ntp.ntp_server1.buf));
    ASSERT_EQ(string("time.cloudflare.com"), string(gw_cfg.ruuvi_cfg.ntp.ntp_server2.buf));
    ASSERT_EQ(string("time.nist.gov"), string(gw_cfg.ruuvi_cfg.ntp.ntp_server3.buf));
    ASSERT_EQ(string("pool.ntp.org"), string(gw_cfg.ruuvi_cfg.ntp.ntp_server4.buf));
    ASSERT_TRUE(gw_cfg.ruuvi_cfg.filter.company_use_filtering);
    ASSERT_EQ(888, gw_cfg.ruuvi_cfg.filter.company_id);
    ASSERT_EQ(string("coord:123,456"), gw_cfg.ruuvi_cfg.coordinates.buf);
    ASSERT_EQ(true, gw_cfg.ruuvi_cfg.scan.scan_coded_phy);
    ASSERT_EQ(true, gw_cfg.ruuvi_cfg.scan.scan_1mbit_phy);
    ASSERT_EQ(true, gw_cfg.ruuvi_cfg.scan.scan_extended_payload);
    ASSERT_EQ(true, gw_cfg.ruuvi_cfg.scan.scan_channel_37);
    ASSERT_EQ(true, gw_cfg.ruuvi_cfg.scan.scan_channel_38);
    ASSERT_EQ(true, gw_cfg.ruuvi_cfg.scan.scan_channel_39);

    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, "Gateway SETTINGS (via HTTP):");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "remote_cfg_use: 0");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "remote_cfg_url: ");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "remote_cfg_auth_type: no");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "remote_cfg_refresh_interval_minutes: 0");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "use_http_ruuvi: 0");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "use_http: 0");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "http_data_format: ruuvi");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "http_auth: none");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "http_url: https://api.ruuvi.com:456/api");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "use_http_stat: 1");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "http_stat_url: https://api.ruuvi.com:456/status");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "http_stat_user: user678");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "http_stat_pass: pass678");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "use_mqtt: 1");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "mqtt_disable_retained_messages: 0");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "mqtt_transport: TCP");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "mqtt_server: mqtt.server.org");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "mqtt_port: 1883");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "mqtt_prefix: prefix");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "mqtt_client_id: AA:BB:CC:DD:EE:FF");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "mqtt_user: user123");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "mqtt_pass: not found");
    TEST_CHECK_LOG_RECORD_GW_CFG(
        ESP_LOG_INFO,
        "Can't find key 'mqtt_pass' in config-json, leave the previous value unchanged");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "lan_auth_type: lan_auth_ruuvi");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "lan_auth_user: user1");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "lan_auth_pass: qwe");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "lan_auth_api_key: ");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "lan_auth_api_key_rw: ");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "auto_update_cycle: beta");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "auto_update_weekdays_bitmask: 126");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "auto_update_interval_from: 1");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "auto_update_interval_to: 23");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "auto_update_tz_offset_hours: 7");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "ntp_use: 1");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "ntp_use_dhcp: 0");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "ntp_server1: time.google.com");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "ntp_server2: time.cloudflare.com");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "ntp_server3: time.nist.gov");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "ntp_server4: pool.ntp.org");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "company_id: 888");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "company_use_filtering: 1");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "scan_coded_phy: 1");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "scan_1mbit_phy: 1");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "scan_extended_payload: 1");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "scan_channel_37: 1");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "scan_channel_38: 1");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "scan_channel_39: 1");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "coordinates: coord:123,456");
    esp_log_wrapper_clear();
    ASSERT_TRUE(esp_log_wrapper_is_empty());
}

TEST_F(TestJsonRuuvi, json_ruuvi_parse_http_body) // NOLINT
{
    gw_cfg_t gw_cfg = { 0 };
    gw_cfg_default_get(&gw_cfg);
    cJSON_Hooks hooks = {
        .malloc_fn = &os_malloc,
        .free_fn   = &os_free_internal,
    };
    g_pTestClass->m_malloc_fail_on_cnt = 0;
    cJSON_InitHooks(&hooks);
    bool flag_network_cfg = false;
    ASSERT_TRUE(json_ruuvi_parse_http_body(
        "{"
        "\t\"remote_cfg_use\":\tfalse,\n"
        "\t\"remote_cfg_url\":\t\"\",\n"
        "\t\"remote_cfg_auth_type\":\t\"no\",\n"
        "\t\"remote_cfg_refresh_interval_minutes\":\t0,\n"

        "\t\"use_http_ruuvi\":\tfalse,\n"
        "\t\"use_http\":\tfalse,\n"
        "\t\"http_url\":\t\"https://api.ruuvi.com:456/api\",\n"
        "\t\"http_data_format\":\t\"ruuvi\",\n"
        "\t\"http_auth\":\t\"none\",\n"

        "\"use_http_stat\":true,"
        "\"http_stat_url\":\"" RUUVI_GATEWAY_HTTP_STATUS_URL
        "\","
        "\"http_stat_user\":\"\","
        "\"http_stat_pass\":\"\","

        "\"use_mqtt\":true,"
        "\"mqtt_disable_retained_messages\":false,"
        "\"mqtt_transport\":\"TCP\","
        "\"mqtt_server\":\"test.mosquitto.org\","
        "\"mqtt_port\":1883,"
        "\"mqtt_prefix\":\"ruuvi/30:AE:A4:02:84:A4\","
        "\"mqtt_client_id\":\"30:AE:A4:02:84:A4\","
        "\"mqtt_user\":\"\","
        "\"mqtt_pass\":\"\","

        "\"lan_auth_type\":\"lan_auth_ruuvi\","
        "\"lan_auth_user\":\"user1\","
        "\"lan_auth_pass\":\"qwe\","
        "\"lan_auth_api_key\":\"\","
        "\"lan_auth_api_key_rw\":\"\","
        "\"auto_update_cycle\":\"regular\","
        "\"auto_update_weekdays_bitmask\":127,"
        "\"auto_update_interval_from\":0,"
        "\"auto_update_interval_to\":24,"
        "\"auto_update_tz_offset_hours\":-3,"
        "\"ntp_use\":true,"
        "\"ntp_use_dhcp\":false,"
        "\"ntp_server1\":\"time1.server.com\","
        "\"ntp_server2\":\"time2.server.com\","
        "\"ntp_server3\":\"time3.server.com\","
        "\"ntp_server4\":\"time4.server.com\","
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
    ASSERT_TRUE(gw_cfg.ruuvi_cfg.mqtt.use_mqtt);
    ASSERT_FALSE(gw_cfg.ruuvi_cfg.mqtt.mqtt_disable_retained_messages);
    ASSERT_FALSE(gw_cfg.ruuvi_cfg.remote.use_remote_cfg);
    ASSERT_EQ(string(""), string(gw_cfg.ruuvi_cfg.remote.url.buf));
    ASSERT_EQ(GW_CFG_HTTP_AUTH_TYPE_NONE, gw_cfg.ruuvi_cfg.remote.auth_type);
    ASSERT_EQ(0, gw_cfg.ruuvi_cfg.remote.refresh_interval_minutes);
    ASSERT_EQ(string("TCP"), gw_cfg.ruuvi_cfg.mqtt.mqtt_transport.buf);
    ASSERT_EQ(string("test.mosquitto.org"), gw_cfg.ruuvi_cfg.mqtt.mqtt_server.buf);
    ASSERT_EQ(string("ruuvi/30:AE:A4:02:84:A4"), gw_cfg.ruuvi_cfg.mqtt.mqtt_prefix.buf);
    ASSERT_EQ(string("30:AE:A4:02:84:A4"), gw_cfg.ruuvi_cfg.mqtt.mqtt_client_id.buf);
    ASSERT_EQ(1883, gw_cfg.ruuvi_cfg.mqtt.mqtt_port);
    ASSERT_EQ(string(""), gw_cfg.ruuvi_cfg.mqtt.mqtt_user.buf);
    ASSERT_EQ(string(""), gw_cfg.ruuvi_cfg.mqtt.mqtt_pass.buf);
    ASSERT_FALSE(gw_cfg.ruuvi_cfg.http.use_http_ruuvi);
    ASSERT_FALSE(gw_cfg.ruuvi_cfg.http.use_http);
    ASSERT_EQ(string("https://api.ruuvi.com:456/api"), gw_cfg.ruuvi_cfg.http.http_url.buf);
    ASSERT_EQ(GW_CFG_HTTP_DATA_FORMAT_RUUVI, gw_cfg.ruuvi_cfg.http.data_format);
    ASSERT_EQ(GW_CFG_HTTP_AUTH_TYPE_NONE, gw_cfg.ruuvi_cfg.http.auth_type);
    ASSERT_TRUE(gw_cfg.ruuvi_cfg.http_stat.use_http_stat);
    ASSERT_EQ(string(RUUVI_GATEWAY_HTTP_STATUS_URL), gw_cfg.ruuvi_cfg.http_stat.http_stat_url.buf);
    ASSERT_EQ(string(""), gw_cfg.ruuvi_cfg.http_stat.http_stat_user.buf);
    ASSERT_EQ(string(""), gw_cfg.ruuvi_cfg.http_stat.http_stat_pass.buf);
    ASSERT_EQ(HTTP_SERVER_AUTH_TYPE_RUUVI, gw_cfg.ruuvi_cfg.lan_auth.lan_auth_type);
    ASSERT_EQ(string("user1"), gw_cfg.ruuvi_cfg.lan_auth.lan_auth_user.buf);
    ASSERT_EQ(string("qwe"), gw_cfg.ruuvi_cfg.lan_auth.lan_auth_pass.buf);
    ASSERT_EQ(string(""), gw_cfg.ruuvi_cfg.lan_auth.lan_auth_api_key.buf);
    ASSERT_EQ(string(""), gw_cfg.ruuvi_cfg.lan_auth.lan_auth_api_key_rw.buf);
    ASSERT_EQ(true, gw_cfg.ruuvi_cfg.ntp.ntp_use);
    ASSERT_EQ(false, gw_cfg.ruuvi_cfg.ntp.ntp_use_dhcp);
    ASSERT_EQ(string("time1.server.com"), string(gw_cfg.ruuvi_cfg.ntp.ntp_server1.buf));
    ASSERT_EQ(string("time2.server.com"), string(gw_cfg.ruuvi_cfg.ntp.ntp_server2.buf));
    ASSERT_EQ(string("time3.server.com"), string(gw_cfg.ruuvi_cfg.ntp.ntp_server3.buf));
    ASSERT_EQ(string("time4.server.com"), string(gw_cfg.ruuvi_cfg.ntp.ntp_server4.buf));
    ASSERT_TRUE(gw_cfg.ruuvi_cfg.filter.company_use_filtering);
    ASSERT_EQ(RUUVI_COMPANY_ID, gw_cfg.ruuvi_cfg.filter.company_id);
    ASSERT_EQ(string(""), gw_cfg.ruuvi_cfg.coordinates.buf);

    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, "Gateway SETTINGS (via HTTP):");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "remote_cfg_use: 0");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "remote_cfg_url: ");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "remote_cfg_auth_type: no");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "remote_cfg_refresh_interval_minutes: 0");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "use_http_ruuvi: 0");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "use_http: 0");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "http_data_format: ruuvi");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "http_auth: none");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "http_url: https://api.ruuvi.com:456/api");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "use_http_stat: 1");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "http_stat_url: " RUUVI_GATEWAY_HTTP_STATUS_URL);
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "http_stat_user: ");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "http_stat_pass: ");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "use_mqtt: 1");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "mqtt_disable_retained_messages: 0");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "mqtt_transport: TCP");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "mqtt_server: test.mosquitto.org");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "mqtt_port: 1883");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "mqtt_prefix: ruuvi/30:AE:A4:02:84:A4");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "mqtt_client_id: 30:AE:A4:02:84:A4");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "mqtt_user: ");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "mqtt_pass: ");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "lan_auth_type: lan_auth_ruuvi");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "lan_auth_user: user1");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "lan_auth_pass: qwe");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "lan_auth_api_key: ");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "lan_auth_api_key_rw: ");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "auto_update_cycle: regular");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "auto_update_weekdays_bitmask: 127");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "auto_update_interval_from: 0");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "auto_update_interval_to: 24");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "auto_update_tz_offset_hours: -3");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "ntp_use: 1");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "ntp_use_dhcp: 0");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "ntp_server1: time1.server.com");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "ntp_server2: time2.server.com");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "ntp_server3: time3.server.com");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "ntp_server4: time4.server.com");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "company_id: not found or invalid");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_WARN, "Can't find key 'company_id' in config-json");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "company_use_filtering: 1");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "scan_coded_phy: 1");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "scan_1mbit_phy: 1");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "scan_extended_payload: 1");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "scan_channel_37: 1");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "scan_channel_38: 1");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "scan_channel_39: 1");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "coordinates: not found");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_WARN, "Can't find key 'coordinates' in config-json");
    esp_log_wrapper_clear();
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_TRUE(this->m_mem_alloc_trace.is_empty());
}

TEST_F(TestJsonRuuvi, json_ruuvi_parse_http_body_malloc_failed) // NOLINT
{
    gw_cfg_t gw_cfg = { 0 };
    gw_cfg_default_get(&gw_cfg);
    cJSON_Hooks hooks = {
        .malloc_fn = &os_malloc,
        .free_fn   = &os_free_internal,
    };
    g_pTestClass->m_malloc_fail_on_cnt = 1;
    cJSON_InitHooks(&hooks);
    bool flag_network_cfg = false;
    ASSERT_FALSE(json_ruuvi_parse_http_body(
        "{"
        "\"use_mqtt\":true,"
        "\"mqtt_disable_retained_messages\":false,"
        "\"mqtt_transport\":\"TCP\","
        "\"mqtt_server\":\"test.mosquitto.org\","
        "\"mqtt_port\":1883,"
        "\"mqtt_prefix\":\"ruuvi/30:AE:A4:02:84:A4\","
        "\"mqtt_client_id\":\"30:AE:A4:02:84:A4\","
        "\"mqtt_user\":\"\","
        "\"mqtt_pass\":\"\","
        "\"use_http_ruuvi\":false,"
        "\"use_http\":false,"
        "\"http_data_format\":\"ruuvi\","
        "\"http_auth\":\"none\","
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
