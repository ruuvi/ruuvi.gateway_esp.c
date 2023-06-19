/**
 * @file test_gw_cfg_ruuvi_json_generate.cpp
 * @author TheSomeMan
 * @date 2021-10-06
 * @copyright Ruuvi Innovations Ltd, license BSD-3-Clause.
 */

#include "json_ruuvi.h"
#include <cstring>
#include "gtest/gtest.h"
#include "gw_mac.h"
#include "gw_cfg.h"
#include "gw_cfg_default.h"
#include "gw_cfg_ruuvi_json.h"
#include "esp_log_wrapper.hpp"
#include "os_mutex_recursive.h"
#include "os_mutex.h"
#include "os_task.h"
#include "lwip/ip4_addr.h"
#include "event_mgr.h"

using namespace std;

/*** Google-test class implementation
 * *********************************************************************************/

class TestGwCfgRuuviJsonGenerate;
static TestGwCfgRuuviJsonGenerate* g_pTestClass;

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

class TestGwCfgRuuviJsonGenerate : public ::testing::Test
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

        const gw_cfg_default_init_param_t init_params = {
            .wifi_ap_ssid        = { "my_ssid1" },
            .device_id           = { 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88 },
            .esp32_fw_ver        = { "v1.3.3" },
            .nrf52_fw_ver        = { "v0.7.1" },
            .nrf52_mac_addr      = { 0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF },
            .esp32_mac_addr_wifi = { 0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0x11 },
            .esp32_mac_addr_eth  = { 0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0x22 },
        };
        gw_cfg_default_init(&init_params, nullptr);
        gw_cfg_init(nullptr);
        esp_log_wrapper_clear();
        this->m_malloc_cnt = 0;
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
    TestGwCfgRuuviJsonGenerate();

    ~TestGwCfgRuuviJsonGenerate() override;

    MemAllocTrace m_mem_alloc_trace;
    uint32_t      m_malloc_cnt {};
    uint32_t      m_malloc_fail_on_cnt {};
};

TestGwCfgRuuviJsonGenerate::TestGwCfgRuuviJsonGenerate()
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

os_mutex_recursive_t
os_mutex_recursive_create_static(os_mutex_recursive_static_t* const p_mutex_static)
{
    return reinterpret_cast<os_mutex_recursive_t>(p_mutex_static);
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

TestGwCfgRuuviJsonGenerate::~TestGwCfgRuuviJsonGenerate() = default;

#define TEST_CHECK_LOG_RECORD(level_, msg_) ESP_LOG_WRAPPER_TEST_CHECK_LOG_RECORD("gw_cfg", level_, msg_)

static gw_cfg_t
get_gateway_config_default()
{
    gw_cfg_t gw_cfg {};
    gw_cfg_default_get(&gw_cfg);
    return gw_cfg;
}

/*** Unit-Tests
 * *******************************************************************************************************/

TEST_F(TestGwCfgRuuviJsonGenerate, gw_cfg_ruuvi_json_generate_default) // NOLINT
{
    const gw_cfg_t   gw_cfg   = get_gateway_config_default();
    cjson_wrap_str_t json_str = cjson_wrap_str_null();

    ASSERT_TRUE(gw_cfg_ruuvi_json_generate(&gw_cfg, &json_str));
    ASSERT_NE(nullptr, json_str.p_str);
    ASSERT_EQ(
        string("{\n"
               "\t\"fw_ver\":\t\"v1.3.3\",\n"
               "\t\"nrf52_fw_ver\":\t\"v0.7.1\",\n"
               "\t\"gw_mac\":\t\"AA:BB:CC:DD:EE:FF\",\n"
               "\t\"wifi_sta_config\":\t{\n"
               "\t\t\"ssid\":\t\"\"\n"
               "\t},\n"
               "\t\"wifi_ap_config\":\t{\n"
               "\t\t\"channel\":\t1\n"
               "\t},\n"
               "\t\"use_eth\":\ttrue,\n"
               "\t\"eth_dhcp\":\ttrue,\n"
               "\t\"eth_static_ip\":\t\"\",\n"
               "\t\"eth_netmask\":\t\"\",\n"
               "\t\"eth_gw\":\t\"\",\n"
               "\t\"eth_dns1\":\t\"\",\n"
               "\t\"eth_dns2\":\t\"\",\n"
               "\t\"remote_cfg_use\":\tfalse,\n"
               "\t\"remote_cfg_url\":\t\"\",\n"
               "\t\"remote_cfg_auth_type\":\t\"none\",\n"
               "\t\"remote_cfg_refresh_interval_minutes\":\t0,\n"
               "\t\"use_http_ruuvi\":\ttrue,\n"
               "\t\"use_http\":\ttrue,\n"
               "\t\"http_url\":\t\"" RUUVI_GATEWAY_HTTP_DEFAULT_URL "\",\n"
               "\t\"http_data_format\":\t\"ruuvi\",\n"
               "\t\"http_auth\":\t\"none\",\n"
               "\t\"use_http_stat\":\ttrue,\n"
               "\t\"http_stat_url\":\t\"" RUUVI_GATEWAY_HTTP_STATUS_URL "\",\n"
               "\t\"http_stat_user\":\t\"\",\n"
               "\t\"use_mqtt\":\tfalse,\n"
               "\t\"mqtt_disable_retained_messages\":\tfalse,\n"
               "\t\"mqtt_transport\":\t\"TCP\",\n"
               "\t\"mqtt_server\":\t\"test.mosquitto.org\",\n"
               "\t\"mqtt_port\":\t1883,\n"
               "\t\"mqtt_prefix\":\t\"ruuvi/AA:BB:CC:DD:EE:FF/\",\n"
               "\t\"mqtt_client_id\":\t\"AA:BB:CC:DD:EE:FF\",\n"
               "\t\"mqtt_user\":\t\"\",\n"
               "\t\"lan_auth_type\":\t\"lan_auth_default\",\n"
               "\t\"lan_auth_user\":\t\"Admin\",\n"
               "\t\"lan_auth_api_key_use\":\tfalse,\n"
               "\t\"lan_auth_api_key_rw_use\":\tfalse,\n"
               "\t\"auto_update_cycle\":\t\"regular\",\n"
               "\t\"auto_update_weekdays_bitmask\":\t127,\n"
               "\t\"auto_update_interval_from\":\t0,\n"
               "\t\"auto_update_interval_to\":\t24,\n"
               "\t\"auto_update_tz_offset_hours\":\t3,\n"
               "\t\"ntp_use\":\ttrue,\n"
               "\t\"ntp_use_dhcp\":\tfalse,\n"
               "\t\"ntp_server1\":\t\"time.google.com\",\n"
               "\t\"ntp_server2\":\t\"time.cloudflare.com\",\n"
               "\t\"ntp_server3\":\t\"time.nist.gov\",\n"
               "\t\"ntp_server4\":\t\"pool.ntp.org\",\n"
               "\t\"company_id\":\t1177,\n"
               "\t\"company_use_filtering\":\ttrue,\n"
               "\t\"scan_coded_phy\":\tfalse,\n"
               "\t\"scan_1mbit_phy\":\ttrue,\n"
               "\t\"scan_extended_payload\":\ttrue,\n"
               "\t\"scan_channel_37\":\ttrue,\n"
               "\t\"scan_channel_38\":\ttrue,\n"
               "\t\"scan_channel_39\":\ttrue,\n"
               "\t\"scan_filter_allow_listed\":\tfalse,\n"
               "\t\"scan_filter_list\":\t[],\n"
               "\t\"coordinates\":\t\"\"\n"
               "}"),
        string(json_str.p_str));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    cjson_wrap_free_json_str(&json_str);
}

TEST_F(TestGwCfgRuuviJsonGenerate, gw_cfg_ruuvi_json_generate_default_diff_lan_auth_type) // NOLINT
{
    gw_cfg_t         gw_cfg   = get_gateway_config_default();
    cjson_wrap_str_t json_str = cjson_wrap_str_null();

    gw_cfg.ruuvi_cfg.lan_auth.lan_auth_type = HTTP_SERVER_AUTH_TYPE_ALLOW;
    ASSERT_TRUE(gw_cfg_ruuvi_json_generate(&gw_cfg, &json_str));
    ASSERT_NE(nullptr, json_str.p_str);
    ASSERT_EQ(
        string("{\n"
               "\t\"fw_ver\":\t\"v1.3.3\",\n"
               "\t\"nrf52_fw_ver\":\t\"v0.7.1\",\n"
               "\t\"gw_mac\":\t\"AA:BB:CC:DD:EE:FF\",\n"
               "\t\"wifi_sta_config\":\t{\n"
               "\t\t\"ssid\":\t\"\"\n"
               "\t},\n"
               "\t\"wifi_ap_config\":\t{\n"
               "\t\t\"channel\":\t1\n"
               "\t},\n"
               "\t\"use_eth\":\ttrue,\n"
               "\t\"eth_dhcp\":\ttrue,\n"
               "\t\"eth_static_ip\":\t\"\",\n"
               "\t\"eth_netmask\":\t\"\",\n"
               "\t\"eth_gw\":\t\"\",\n"
               "\t\"eth_dns1\":\t\"\",\n"
               "\t\"eth_dns2\":\t\"\",\n"
               "\t\"remote_cfg_use\":\tfalse,\n"
               "\t\"remote_cfg_url\":\t\"\",\n"
               "\t\"remote_cfg_auth_type\":\t\"none\",\n"
               "\t\"remote_cfg_refresh_interval_minutes\":\t0,\n"
               "\t\"use_http_ruuvi\":\ttrue,\n"
               "\t\"use_http\":\ttrue,\n"
               "\t\"http_url\":\t\"" RUUVI_GATEWAY_HTTP_DEFAULT_URL "\",\n"
               "\t\"http_data_format\":\t\"ruuvi\",\n"
               "\t\"http_auth\":\t\"none\",\n"
               "\t\"use_http_stat\":\ttrue,\n"
               "\t\"http_stat_url\":\t\"" RUUVI_GATEWAY_HTTP_STATUS_URL "\",\n"
               "\t\"http_stat_user\":\t\"\",\n"
               "\t\"use_mqtt\":\tfalse,\n"
               "\t\"mqtt_disable_retained_messages\":\tfalse,\n"
               "\t\"mqtt_transport\":\t\"TCP\",\n"
               "\t\"mqtt_server\":\t\"test.mosquitto.org\",\n"
               "\t\"mqtt_port\":\t1883,\n"
               "\t\"mqtt_prefix\":\t\"ruuvi/AA:BB:CC:DD:EE:FF/\",\n"
               "\t\"mqtt_client_id\":\t\"AA:BB:CC:DD:EE:FF\",\n"
               "\t\"mqtt_user\":\t\"\",\n"
               "\t\"lan_auth_type\":\t\"lan_auth_allow\",\n"
               "\t\"lan_auth_user\":\t\"Admin\",\n"
               "\t\"lan_auth_api_key_use\":\tfalse,\n"
               "\t\"lan_auth_api_key_rw_use\":\tfalse,\n"
               "\t\"auto_update_cycle\":\t\"regular\",\n"
               "\t\"auto_update_weekdays_bitmask\":\t127,\n"
               "\t\"auto_update_interval_from\":\t0,\n"
               "\t\"auto_update_interval_to\":\t24,\n"
               "\t\"auto_update_tz_offset_hours\":\t3,\n"
               "\t\"ntp_use\":\ttrue,\n"
               "\t\"ntp_use_dhcp\":\tfalse,\n"
               "\t\"ntp_server1\":\t\"time.google.com\",\n"
               "\t\"ntp_server2\":\t\"time.cloudflare.com\",\n"
               "\t\"ntp_server3\":\t\"time.nist.gov\",\n"
               "\t\"ntp_server4\":\t\"pool.ntp.org\",\n"
               "\t\"company_id\":\t1177,\n"
               "\t\"company_use_filtering\":\ttrue,\n"
               "\t\"scan_coded_phy\":\tfalse,\n"
               "\t\"scan_1mbit_phy\":\ttrue,\n"
               "\t\"scan_extended_payload\":\ttrue,\n"
               "\t\"scan_channel_37\":\ttrue,\n"
               "\t\"scan_channel_38\":\ttrue,\n"
               "\t\"scan_channel_39\":\ttrue,\n"
               "\t\"scan_filter_allow_listed\":\tfalse,\n"
               "\t\"scan_filter_list\":\t[],\n"
               "\t\"coordinates\":\t\"\"\n"
               "}"),
        string(json_str.p_str));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    cjson_wrap_free_json_str(&json_str);
}

TEST_F(TestGwCfgRuuviJsonGenerate, gw_cfg_ruuvi_json_generate_default_diff_lan_auth_user) // NOLINT
{
    gw_cfg_t         gw_cfg   = get_gateway_config_default();
    cjson_wrap_str_t json_str = cjson_wrap_str_null();

    gw_cfg.ruuvi_cfg.lan_auth.lan_auth_type = HTTP_SERVER_AUTH_TYPE_RUUVI;
    snprintf(gw_cfg.ruuvi_cfg.lan_auth.lan_auth_user.buf, sizeof(gw_cfg.ruuvi_cfg.lan_auth.lan_auth_user.buf), "user2");
    snprintf(gw_cfg.ruuvi_cfg.lan_auth.lan_auth_pass.buf, sizeof(gw_cfg.ruuvi_cfg.lan_auth.lan_auth_pass.buf), "pass2");
    ASSERT_TRUE(gw_cfg_ruuvi_json_generate(&gw_cfg, &json_str));
    ASSERT_NE(nullptr, json_str.p_str);
    ASSERT_EQ(
        string("{\n"
               "\t\"fw_ver\":\t\"v1.3.3\",\n"
               "\t\"nrf52_fw_ver\":\t\"v0.7.1\",\n"
               "\t\"gw_mac\":\t\"AA:BB:CC:DD:EE:FF\",\n"
               "\t\"wifi_sta_config\":\t{\n"
               "\t\t\"ssid\":\t\"\"\n"
               "\t},\n"
               "\t\"wifi_ap_config\":\t{\n"
               "\t\t\"channel\":\t1\n"
               "\t},\n"
               "\t\"use_eth\":\ttrue,\n"
               "\t\"eth_dhcp\":\ttrue,\n"
               "\t\"eth_static_ip\":\t\"\",\n"
               "\t\"eth_netmask\":\t\"\",\n"
               "\t\"eth_gw\":\t\"\",\n"
               "\t\"eth_dns1\":\t\"\",\n"
               "\t\"eth_dns2\":\t\"\",\n"
               "\t\"remote_cfg_use\":\tfalse,\n"
               "\t\"remote_cfg_url\":\t\"\",\n"
               "\t\"remote_cfg_auth_type\":\t\"none\",\n"
               "\t\"remote_cfg_refresh_interval_minutes\":\t0,\n"
               "\t\"use_http_ruuvi\":\ttrue,\n"
               "\t\"use_http\":\ttrue,\n"
               "\t\"http_url\":\t\"" RUUVI_GATEWAY_HTTP_DEFAULT_URL "\",\n"
               "\t\"http_data_format\":\t\"ruuvi\",\n"
               "\t\"http_auth\":\t\"none\",\n"
               "\t\"use_http_stat\":\ttrue,\n"
               "\t\"http_stat_url\":\t\"" RUUVI_GATEWAY_HTTP_STATUS_URL "\",\n"
               "\t\"http_stat_user\":\t\"\",\n"
               "\t\"use_mqtt\":\tfalse,\n"
               "\t\"mqtt_disable_retained_messages\":\tfalse,\n"
               "\t\"mqtt_transport\":\t\"TCP\",\n"
               "\t\"mqtt_server\":\t\"test.mosquitto.org\",\n"
               "\t\"mqtt_port\":\t1883,\n"
               "\t\"mqtt_prefix\":\t\"ruuvi/AA:BB:CC:DD:EE:FF/\",\n"
               "\t\"mqtt_client_id\":\t\"AA:BB:CC:DD:EE:FF\",\n"
               "\t\"mqtt_user\":\t\"\",\n"
               "\t\"lan_auth_type\":\t\"lan_auth_ruuvi\",\n"
               "\t\"lan_auth_user\":\t\"user2\",\n"
               "\t\"lan_auth_api_key_use\":\tfalse,\n"
               "\t\"lan_auth_api_key_rw_use\":\tfalse,\n"
               "\t\"auto_update_cycle\":\t\"regular\",\n"
               "\t\"auto_update_weekdays_bitmask\":\t127,\n"
               "\t\"auto_update_interval_from\":\t0,\n"
               "\t\"auto_update_interval_to\":\t24,\n"
               "\t\"auto_update_tz_offset_hours\":\t3,\n"
               "\t\"ntp_use\":\ttrue,\n"
               "\t\"ntp_use_dhcp\":\tfalse,\n"
               "\t\"ntp_server1\":\t\"time.google.com\",\n"
               "\t\"ntp_server2\":\t\"time.cloudflare.com\",\n"
               "\t\"ntp_server3\":\t\"time.nist.gov\",\n"
               "\t\"ntp_server4\":\t\"pool.ntp.org\",\n"
               "\t\"company_id\":\t1177,\n"
               "\t\"company_use_filtering\":\ttrue,\n"
               "\t\"scan_coded_phy\":\tfalse,\n"
               "\t\"scan_1mbit_phy\":\ttrue,\n"
               "\t\"scan_extended_payload\":\ttrue,\n"
               "\t\"scan_channel_37\":\ttrue,\n"
               "\t\"scan_channel_38\":\ttrue,\n"
               "\t\"scan_channel_39\":\ttrue,\n"
               "\t\"scan_filter_allow_listed\":\tfalse,\n"
               "\t\"scan_filter_list\":\t[],\n"
               "\t\"coordinates\":\t\"\"\n"
               "}"),
        string(json_str.p_str));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    cjson_wrap_free_json_str(&json_str);
}

TEST_F(TestGwCfgRuuviJsonGenerate, gw_cfg_ruuvi_json_generate_default_diff_lan_auth_pass) // NOLINT
{
    gw_cfg_t         gw_cfg   = get_gateway_config_default();
    cjson_wrap_str_t json_str = cjson_wrap_str_null();

    gw_cfg.ruuvi_cfg.lan_auth.lan_auth_type = HTTP_SERVER_AUTH_TYPE_RUUVI;
    snprintf(gw_cfg.ruuvi_cfg.lan_auth.lan_auth_pass.buf, sizeof(gw_cfg.ruuvi_cfg.lan_auth.lan_auth_pass.buf), "qwe");

    ASSERT_TRUE(gw_cfg_ruuvi_json_generate(&gw_cfg, &json_str));
    ASSERT_NE(nullptr, json_str.p_str);
    ASSERT_EQ(
        string("{\n"
               "\t\"fw_ver\":\t\"v1.3.3\",\n"
               "\t\"nrf52_fw_ver\":\t\"v0.7.1\",\n"
               "\t\"gw_mac\":\t\"AA:BB:CC:DD:EE:FF\",\n"
               "\t\"wifi_sta_config\":\t{\n"
               "\t\t\"ssid\":\t\"\"\n"
               "\t},\n"
               "\t\"wifi_ap_config\":\t{\n"
               "\t\t\"channel\":\t1\n"
               "\t},\n"
               "\t\"use_eth\":\ttrue,\n"
               "\t\"eth_dhcp\":\ttrue,\n"
               "\t\"eth_static_ip\":\t\"\",\n"
               "\t\"eth_netmask\":\t\"\",\n"
               "\t\"eth_gw\":\t\"\",\n"
               "\t\"eth_dns1\":\t\"\",\n"
               "\t\"eth_dns2\":\t\"\",\n"
               "\t\"remote_cfg_use\":\tfalse,\n"
               "\t\"remote_cfg_url\":\t\"\",\n"
               "\t\"remote_cfg_auth_type\":\t\"none\",\n"
               "\t\"remote_cfg_refresh_interval_minutes\":\t0,\n"
               "\t\"use_http_ruuvi\":\ttrue,\n"
               "\t\"use_http\":\ttrue,\n"
               "\t\"http_url\":\t\"" RUUVI_GATEWAY_HTTP_DEFAULT_URL "\",\n"
               "\t\"http_data_format\":\t\"ruuvi\",\n"
               "\t\"http_auth\":\t\"none\",\n"
               "\t\"use_http_stat\":\ttrue,\n"
               "\t\"http_stat_url\":\t\"" RUUVI_GATEWAY_HTTP_STATUS_URL "\",\n"
               "\t\"http_stat_user\":\t\"\",\n"
               "\t\"use_mqtt\":\tfalse,\n"
               "\t\"mqtt_disable_retained_messages\":\tfalse,\n"
               "\t\"mqtt_transport\":\t\"TCP\",\n"
               "\t\"mqtt_server\":\t\"test.mosquitto.org\",\n"
               "\t\"mqtt_port\":\t1883,\n"
               "\t\"mqtt_prefix\":\t\"ruuvi/AA:BB:CC:DD:EE:FF/\",\n"
               "\t\"mqtt_client_id\":\t\"AA:BB:CC:DD:EE:FF\",\n"
               "\t\"mqtt_user\":\t\"\",\n"
               "\t\"lan_auth_type\":\t\"lan_auth_ruuvi\",\n"
               "\t\"lan_auth_user\":\t\"Admin\",\n"
               "\t\"lan_auth_api_key_use\":\tfalse,\n"
               "\t\"lan_auth_api_key_rw_use\":\tfalse,\n"
               "\t\"auto_update_cycle\":\t\"regular\",\n"
               "\t\"auto_update_weekdays_bitmask\":\t127,\n"
               "\t\"auto_update_interval_from\":\t0,\n"
               "\t\"auto_update_interval_to\":\t24,\n"
               "\t\"auto_update_tz_offset_hours\":\t3,\n"
               "\t\"ntp_use\":\ttrue,\n"
               "\t\"ntp_use_dhcp\":\tfalse,\n"
               "\t\"ntp_server1\":\t\"time.google.com\",\n"
               "\t\"ntp_server2\":\t\"time.cloudflare.com\",\n"
               "\t\"ntp_server3\":\t\"time.nist.gov\",\n"
               "\t\"ntp_server4\":\t\"pool.ntp.org\",\n"
               "\t\"company_id\":\t1177,\n"
               "\t\"company_use_filtering\":\ttrue,\n"
               "\t\"scan_coded_phy\":\tfalse,\n"
               "\t\"scan_1mbit_phy\":\ttrue,\n"
               "\t\"scan_extended_payload\":\ttrue,\n"
               "\t\"scan_channel_37\":\ttrue,\n"
               "\t\"scan_channel_38\":\ttrue,\n"
               "\t\"scan_channel_39\":\ttrue,\n"
               "\t\"scan_filter_allow_listed\":\tfalse,\n"
               "\t\"scan_filter_list\":\t[],\n"
               "\t\"coordinates\":\t\"\"\n"
               "}"),
        string(json_str.p_str));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    cjson_wrap_free_json_str(&json_str);
}

TEST_F(TestGwCfgRuuviJsonGenerate, gw_cfg_ruuvi_json_generate_default_diff_lan_auth_api_key_rw) // NOLINT
{
    gw_cfg_t         gw_cfg   = get_gateway_config_default();
    cjson_wrap_str_t json_str = cjson_wrap_str_null();

    snprintf(
        gw_cfg.ruuvi_cfg.lan_auth.lan_auth_api_key_rw.buf,
        sizeof(gw_cfg.ruuvi_cfg.lan_auth.lan_auth_api_key_rw.buf),
        "6kl/fd/c+3qvWm3Mhmwgh3BWNp+HDRQiLp/X0PuwG8Q=");

    ASSERT_TRUE(gw_cfg_ruuvi_json_generate(&gw_cfg, &json_str));
    ASSERT_NE(nullptr, json_str.p_str);
    ASSERT_EQ(
        string("{\n"
               "\t\"fw_ver\":\t\"v1.3.3\",\n"
               "\t\"nrf52_fw_ver\":\t\"v0.7.1\",\n"
               "\t\"gw_mac\":\t\"AA:BB:CC:DD:EE:FF\",\n"
               "\t\"wifi_sta_config\":\t{\n"
               "\t\t\"ssid\":\t\"\"\n"
               "\t},\n"
               "\t\"wifi_ap_config\":\t{\n"
               "\t\t\"channel\":\t1\n"
               "\t},\n"
               "\t\"use_eth\":\ttrue,\n"
               "\t\"eth_dhcp\":\ttrue,\n"
               "\t\"eth_static_ip\":\t\"\",\n"
               "\t\"eth_netmask\":\t\"\",\n"
               "\t\"eth_gw\":\t\"\",\n"
               "\t\"eth_dns1\":\t\"\",\n"
               "\t\"eth_dns2\":\t\"\",\n"
               "\t\"remote_cfg_use\":\tfalse,\n"
               "\t\"remote_cfg_url\":\t\"\",\n"
               "\t\"remote_cfg_auth_type\":\t\"none\",\n"
               "\t\"remote_cfg_refresh_interval_minutes\":\t0,\n"
               "\t\"use_http_ruuvi\":\ttrue,\n"
               "\t\"use_http\":\ttrue,\n"
               "\t\"http_url\":\t\"" RUUVI_GATEWAY_HTTP_DEFAULT_URL "\",\n"
               "\t\"http_data_format\":\t\"ruuvi\",\n"
               "\t\"http_auth\":\t\"none\",\n"
               "\t\"use_http_stat\":\ttrue,\n"
               "\t\"http_stat_url\":\t\"" RUUVI_GATEWAY_HTTP_STATUS_URL "\",\n"
               "\t\"http_stat_user\":\t\"\",\n"
               "\t\"use_mqtt\":\tfalse,\n"
               "\t\"mqtt_disable_retained_messages\":\tfalse,\n"
               "\t\"mqtt_transport\":\t\"TCP\",\n"
               "\t\"mqtt_server\":\t\"test.mosquitto.org\",\n"
               "\t\"mqtt_port\":\t1883,\n"
               "\t\"mqtt_prefix\":\t\"ruuvi/AA:BB:CC:DD:EE:FF/\",\n"
               "\t\"mqtt_client_id\":\t\"AA:BB:CC:DD:EE:FF\",\n"
               "\t\"mqtt_user\":\t\"\",\n"
               "\t\"lan_auth_type\":\t\"lan_auth_default\",\n"
               "\t\"lan_auth_user\":\t\"Admin\",\n"
               "\t\"lan_auth_api_key_use\":\tfalse,\n"
               "\t\"lan_auth_api_key_rw_use\":\ttrue,\n"
               "\t\"auto_update_cycle\":\t\"regular\",\n"
               "\t\"auto_update_weekdays_bitmask\":\t127,\n"
               "\t\"auto_update_interval_from\":\t0,\n"
               "\t\"auto_update_interval_to\":\t24,\n"
               "\t\"auto_update_tz_offset_hours\":\t3,\n"
               "\t\"ntp_use\":\ttrue,\n"
               "\t\"ntp_use_dhcp\":\tfalse,\n"
               "\t\"ntp_server1\":\t\"time.google.com\",\n"
               "\t\"ntp_server2\":\t\"time.cloudflare.com\",\n"
               "\t\"ntp_server3\":\t\"time.nist.gov\",\n"
               "\t\"ntp_server4\":\t\"pool.ntp.org\",\n"
               "\t\"company_id\":\t1177,\n"
               "\t\"company_use_filtering\":\ttrue,\n"
               "\t\"scan_coded_phy\":\tfalse,\n"
               "\t\"scan_1mbit_phy\":\ttrue,\n"
               "\t\"scan_extended_payload\":\ttrue,\n"
               "\t\"scan_channel_37\":\ttrue,\n"
               "\t\"scan_channel_38\":\ttrue,\n"
               "\t\"scan_channel_39\":\ttrue,\n"
               "\t\"scan_filter_allow_listed\":\tfalse,\n"
               "\t\"scan_filter_list\":\t[],\n"
               "\t\"coordinates\":\t\"\"\n"
               "}"),
        string(json_str.p_str));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    cjson_wrap_free_json_str(&json_str);
}

TEST_F(TestGwCfgRuuviJsonGenerate, gw_cfg_ruuvi_json_generate_default_diff_lan_auth_api_key) // NOLINT
{
    gw_cfg_t         gw_cfg   = get_gateway_config_default();
    cjson_wrap_str_t json_str = cjson_wrap_str_null();

    snprintf(
        gw_cfg.ruuvi_cfg.lan_auth.lan_auth_api_key.buf,
        sizeof(gw_cfg.ruuvi_cfg.lan_auth.lan_auth_api_key.buf),
        "6kl/fd/c+3qvWm3Mhmwgh3BWNp+HDRQiLp/X0PuwG8Q=");

    ASSERT_TRUE(gw_cfg_ruuvi_json_generate(&gw_cfg, &json_str));
    ASSERT_NE(nullptr, json_str.p_str);
    ASSERT_EQ(
        string("{\n"
               "\t\"fw_ver\":\t\"v1.3.3\",\n"
               "\t\"nrf52_fw_ver\":\t\"v0.7.1\",\n"
               "\t\"gw_mac\":\t\"AA:BB:CC:DD:EE:FF\",\n"
               "\t\"wifi_sta_config\":\t{\n"
               "\t\t\"ssid\":\t\"\"\n"
               "\t},\n"
               "\t\"wifi_ap_config\":\t{\n"
               "\t\t\"channel\":\t1\n"
               "\t},\n"
               "\t\"use_eth\":\ttrue,\n"
               "\t\"eth_dhcp\":\ttrue,\n"
               "\t\"eth_static_ip\":\t\"\",\n"
               "\t\"eth_netmask\":\t\"\",\n"
               "\t\"eth_gw\":\t\"\",\n"
               "\t\"eth_dns1\":\t\"\",\n"
               "\t\"eth_dns2\":\t\"\",\n"
               "\t\"remote_cfg_use\":\tfalse,\n"
               "\t\"remote_cfg_url\":\t\"\",\n"
               "\t\"remote_cfg_auth_type\":\t\"none\",\n"
               "\t\"remote_cfg_refresh_interval_minutes\":\t0,\n"
               "\t\"use_http_ruuvi\":\ttrue,\n"
               "\t\"use_http\":\ttrue,\n"
               "\t\"http_url\":\t\"" RUUVI_GATEWAY_HTTP_DEFAULT_URL "\",\n"
               "\t\"http_data_format\":\t\"ruuvi\",\n"
               "\t\"http_auth\":\t\"none\",\n"
               "\t\"use_http_stat\":\ttrue,\n"
               "\t\"http_stat_url\":\t\"" RUUVI_GATEWAY_HTTP_STATUS_URL "\",\n"
               "\t\"http_stat_user\":\t\"\",\n"
               "\t\"use_mqtt\":\tfalse,\n"
               "\t\"mqtt_disable_retained_messages\":\tfalse,\n"
               "\t\"mqtt_transport\":\t\"TCP\",\n"
               "\t\"mqtt_server\":\t\"test.mosquitto.org\",\n"
               "\t\"mqtt_port\":\t1883,\n"
               "\t\"mqtt_prefix\":\t\"ruuvi/AA:BB:CC:DD:EE:FF/\",\n"
               "\t\"mqtt_client_id\":\t\"AA:BB:CC:DD:EE:FF\",\n"
               "\t\"mqtt_user\":\t\"\",\n"
               "\t\"lan_auth_type\":\t\"lan_auth_default\",\n"
               "\t\"lan_auth_user\":\t\"Admin\",\n"
               "\t\"lan_auth_api_key_use\":\ttrue,\n"
               "\t\"lan_auth_api_key_rw_use\":\tfalse,\n"
               "\t\"auto_update_cycle\":\t\"regular\",\n"
               "\t\"auto_update_weekdays_bitmask\":\t127,\n"
               "\t\"auto_update_interval_from\":\t0,\n"
               "\t\"auto_update_interval_to\":\t24,\n"
               "\t\"auto_update_tz_offset_hours\":\t3,\n"
               "\t\"ntp_use\":\ttrue,\n"
               "\t\"ntp_use_dhcp\":\tfalse,\n"
               "\t\"ntp_server1\":\t\"time.google.com\",\n"
               "\t\"ntp_server2\":\t\"time.cloudflare.com\",\n"
               "\t\"ntp_server3\":\t\"time.nist.gov\",\n"
               "\t\"ntp_server4\":\t\"pool.ntp.org\",\n"
               "\t\"company_id\":\t1177,\n"
               "\t\"company_use_filtering\":\ttrue,\n"
               "\t\"scan_coded_phy\":\tfalse,\n"
               "\t\"scan_1mbit_phy\":\ttrue,\n"
               "\t\"scan_extended_payload\":\ttrue,\n"
               "\t\"scan_channel_37\":\ttrue,\n"
               "\t\"scan_channel_38\":\ttrue,\n"
               "\t\"scan_channel_39\":\ttrue,\n"
               "\t\"scan_filter_allow_listed\":\tfalse,\n"
               "\t\"scan_filter_list\":\t[],\n"
               "\t\"coordinates\":\t\"\"\n"
               "}"),
        string(json_str.p_str));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    cjson_wrap_free_json_str(&json_str);
}

TEST_F(TestGwCfgRuuviJsonGenerate, gw_cfg_ruuvi_json_generate_default_auto_update_cycle_beta) // NOLINT
{
    gw_cfg_t         gw_cfg   = get_gateway_config_default();
    cjson_wrap_str_t json_str = cjson_wrap_str_null();

    gw_cfg.ruuvi_cfg.auto_update.auto_update_cycle = AUTO_UPDATE_CYCLE_TYPE_BETA_TESTER;
    ASSERT_TRUE(gw_cfg_ruuvi_json_generate(&gw_cfg, &json_str));
    ASSERT_NE(nullptr, json_str.p_str);
    ASSERT_EQ(
        string("{\n"
               "\t\"fw_ver\":\t\"v1.3.3\",\n"
               "\t\"nrf52_fw_ver\":\t\"v0.7.1\",\n"
               "\t\"gw_mac\":\t\"AA:BB:CC:DD:EE:FF\",\n"
               "\t\"wifi_sta_config\":\t{\n"
               "\t\t\"ssid\":\t\"\"\n"
               "\t},\n"
               "\t\"wifi_ap_config\":\t{\n"
               "\t\t\"channel\":\t1\n"
               "\t},\n"
               "\t\"use_eth\":\ttrue,\n"
               "\t\"eth_dhcp\":\ttrue,\n"
               "\t\"eth_static_ip\":\t\"\",\n"
               "\t\"eth_netmask\":\t\"\",\n"
               "\t\"eth_gw\":\t\"\",\n"
               "\t\"eth_dns1\":\t\"\",\n"
               "\t\"eth_dns2\":\t\"\",\n"
               "\t\"remote_cfg_use\":\tfalse,\n"
               "\t\"remote_cfg_url\":\t\"\",\n"
               "\t\"remote_cfg_auth_type\":\t\"none\",\n"
               "\t\"remote_cfg_refresh_interval_minutes\":\t0,\n"
               "\t\"use_http_ruuvi\":\ttrue,\n"
               "\t\"use_http\":\ttrue,\n"
               "\t\"http_url\":\t\"" RUUVI_GATEWAY_HTTP_DEFAULT_URL "\",\n"
               "\t\"http_data_format\":\t\"ruuvi\",\n"
               "\t\"http_auth\":\t\"none\",\n"
               "\t\"use_http_stat\":\ttrue,\n"
               "\t\"http_stat_url\":\t\"" RUUVI_GATEWAY_HTTP_STATUS_URL "\",\n"
               "\t\"http_stat_user\":\t\"\",\n"
               "\t\"use_mqtt\":\tfalse,\n"
               "\t\"mqtt_disable_retained_messages\":\tfalse,\n"
               "\t\"mqtt_transport\":\t\"TCP\",\n"
               "\t\"mqtt_server\":\t\"test.mosquitto.org\",\n"
               "\t\"mqtt_port\":\t1883,\n"
               "\t\"mqtt_prefix\":\t\"ruuvi/AA:BB:CC:DD:EE:FF/\",\n"
               "\t\"mqtt_client_id\":\t\"AA:BB:CC:DD:EE:FF\",\n"
               "\t\"mqtt_user\":\t\"\",\n"
               "\t\"lan_auth_type\":\t\"lan_auth_default\",\n"
               "\t\"lan_auth_user\":\t\"Admin\",\n"
               "\t\"lan_auth_api_key_use\":\tfalse,\n"
               "\t\"lan_auth_api_key_rw_use\":\tfalse,\n"
               "\t\"auto_update_cycle\":\t\"beta\",\n"
               "\t\"auto_update_weekdays_bitmask\":\t127,\n"
               "\t\"auto_update_interval_from\":\t0,\n"
               "\t\"auto_update_interval_to\":\t24,\n"
               "\t\"auto_update_tz_offset_hours\":\t3,\n"
               "\t\"ntp_use\":\ttrue,\n"
               "\t\"ntp_use_dhcp\":\tfalse,\n"
               "\t\"ntp_server1\":\t\"time.google.com\",\n"
               "\t\"ntp_server2\":\t\"time.cloudflare.com\",\n"
               "\t\"ntp_server3\":\t\"time.nist.gov\",\n"
               "\t\"ntp_server4\":\t\"pool.ntp.org\",\n"
               "\t\"company_id\":\t1177,\n"
               "\t\"company_use_filtering\":\ttrue,\n"
               "\t\"scan_coded_phy\":\tfalse,\n"
               "\t\"scan_1mbit_phy\":\ttrue,\n"
               "\t\"scan_extended_payload\":\ttrue,\n"
               "\t\"scan_channel_37\":\ttrue,\n"
               "\t\"scan_channel_38\":\ttrue,\n"
               "\t\"scan_channel_39\":\ttrue,\n"
               "\t\"scan_filter_allow_listed\":\tfalse,\n"
               "\t\"scan_filter_list\":\t[],\n"
               "\t\"coordinates\":\t\"\"\n"
               "}"),
        string(json_str.p_str));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    cjson_wrap_free_json_str(&json_str);
}

TEST_F(TestGwCfgRuuviJsonGenerate, gw_cfg_ruuvi_json_generate_default_auto_update_cycle_manual) // NOLINT
{
    gw_cfg_t         gw_cfg   = get_gateway_config_default();
    cjson_wrap_str_t json_str = cjson_wrap_str_null();

    gw_cfg.ruuvi_cfg.auto_update.auto_update_cycle = AUTO_UPDATE_CYCLE_TYPE_MANUAL;
    ASSERT_TRUE(gw_cfg_ruuvi_json_generate(&gw_cfg, &json_str));
    ASSERT_NE(nullptr, json_str.p_str);
    ASSERT_EQ(
        string("{\n"
               "\t\"fw_ver\":\t\"v1.3.3\",\n"
               "\t\"nrf52_fw_ver\":\t\"v0.7.1\",\n"
               "\t\"gw_mac\":\t\"AA:BB:CC:DD:EE:FF\",\n"
               "\t\"wifi_sta_config\":\t{\n"
               "\t\t\"ssid\":\t\"\"\n"
               "\t},\n"
               "\t\"wifi_ap_config\":\t{\n"
               "\t\t\"channel\":\t1\n"
               "\t},\n"
               "\t\"use_eth\":\ttrue,\n"
               "\t\"eth_dhcp\":\ttrue,\n"
               "\t\"eth_static_ip\":\t\"\",\n"
               "\t\"eth_netmask\":\t\"\",\n"
               "\t\"eth_gw\":\t\"\",\n"
               "\t\"eth_dns1\":\t\"\",\n"
               "\t\"eth_dns2\":\t\"\",\n"
               "\t\"remote_cfg_use\":\tfalse,\n"
               "\t\"remote_cfg_url\":\t\"\",\n"
               "\t\"remote_cfg_auth_type\":\t\"none\",\n"
               "\t\"remote_cfg_refresh_interval_minutes\":\t0,\n"
               "\t\"use_http_ruuvi\":\ttrue,\n"
               "\t\"use_http\":\ttrue,\n"
               "\t\"http_url\":\t\"" RUUVI_GATEWAY_HTTP_DEFAULT_URL "\",\n"
               "\t\"http_data_format\":\t\"ruuvi\",\n"
               "\t\"http_auth\":\t\"none\",\n"
               "\t\"use_http_stat\":\ttrue,\n"
               "\t\"http_stat_url\":\t\"" RUUVI_GATEWAY_HTTP_STATUS_URL "\",\n"
               "\t\"http_stat_user\":\t\"\",\n"
               "\t\"use_mqtt\":\tfalse,\n"
               "\t\"mqtt_disable_retained_messages\":\tfalse,\n"
               "\t\"mqtt_transport\":\t\"TCP\",\n"
               "\t\"mqtt_server\":\t\"test.mosquitto.org\",\n"
               "\t\"mqtt_port\":\t1883,\n"
               "\t\"mqtt_prefix\":\t\"ruuvi/AA:BB:CC:DD:EE:FF/\",\n"
               "\t\"mqtt_client_id\":\t\"AA:BB:CC:DD:EE:FF\",\n"
               "\t\"mqtt_user\":\t\"\",\n"
               "\t\"lan_auth_type\":\t\"lan_auth_default\",\n"
               "\t\"lan_auth_user\":\t\"Admin\",\n"
               "\t\"lan_auth_api_key_use\":\tfalse,\n"
               "\t\"lan_auth_api_key_rw_use\":\tfalse,\n"
               "\t\"auto_update_cycle\":\t\"manual\",\n"
               "\t\"auto_update_weekdays_bitmask\":\t127,\n"
               "\t\"auto_update_interval_from\":\t0,\n"
               "\t\"auto_update_interval_to\":\t24,\n"
               "\t\"auto_update_tz_offset_hours\":\t3,\n"
               "\t\"ntp_use\":\ttrue,\n"
               "\t\"ntp_use_dhcp\":\tfalse,\n"
               "\t\"ntp_server1\":\t\"time.google.com\",\n"
               "\t\"ntp_server2\":\t\"time.cloudflare.com\",\n"
               "\t\"ntp_server3\":\t\"time.nist.gov\",\n"
               "\t\"ntp_server4\":\t\"pool.ntp.org\",\n"
               "\t\"company_id\":\t1177,\n"
               "\t\"company_use_filtering\":\ttrue,\n"
               "\t\"scan_coded_phy\":\tfalse,\n"
               "\t\"scan_1mbit_phy\":\ttrue,\n"
               "\t\"scan_extended_payload\":\ttrue,\n"
               "\t\"scan_channel_37\":\ttrue,\n"
               "\t\"scan_channel_38\":\ttrue,\n"
               "\t\"scan_channel_39\":\ttrue,\n"
               "\t\"scan_filter_allow_listed\":\tfalse,\n"
               "\t\"scan_filter_list\":\t[],\n"
               "\t\"coordinates\":\t\"\"\n"
               "}"),
        string(json_str.p_str));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    cjson_wrap_free_json_str(&json_str);
}

TEST_F(TestGwCfgRuuviJsonGenerate, gw_cfg_ruuvi_json_generate_default_auto_update_cycle_unknown) // NOLINT
{
    gw_cfg_t         gw_cfg   = get_gateway_config_default();
    cjson_wrap_str_t json_str = cjson_wrap_str_null();

    gw_cfg.ruuvi_cfg.auto_update.auto_update_cycle = (auto_update_cycle_type_e)-1;
    ASSERT_TRUE(gw_cfg_ruuvi_json_generate(&gw_cfg, &json_str));
    ASSERT_NE(nullptr, json_str.p_str);
    ASSERT_EQ(
        string("{\n"
               "\t\"fw_ver\":\t\"v1.3.3\",\n"
               "\t\"nrf52_fw_ver\":\t\"v0.7.1\",\n"
               "\t\"gw_mac\":\t\"AA:BB:CC:DD:EE:FF\",\n"
               "\t\"wifi_sta_config\":\t{\n"
               "\t\t\"ssid\":\t\"\"\n"
               "\t},\n"
               "\t\"wifi_ap_config\":\t{\n"
               "\t\t\"channel\":\t1\n"
               "\t},\n"
               "\t\"use_eth\":\ttrue,\n"
               "\t\"eth_dhcp\":\ttrue,\n"
               "\t\"eth_static_ip\":\t\"\",\n"
               "\t\"eth_netmask\":\t\"\",\n"
               "\t\"eth_gw\":\t\"\",\n"
               "\t\"eth_dns1\":\t\"\",\n"
               "\t\"eth_dns2\":\t\"\",\n"
               "\t\"remote_cfg_use\":\tfalse,\n"
               "\t\"remote_cfg_url\":\t\"\",\n"
               "\t\"remote_cfg_auth_type\":\t\"none\",\n"
               "\t\"remote_cfg_refresh_interval_minutes\":\t0,\n"
               "\t\"use_http_ruuvi\":\ttrue,\n"
               "\t\"use_http\":\ttrue,\n"
               "\t\"http_url\":\t\"" RUUVI_GATEWAY_HTTP_DEFAULT_URL "\",\n"
               "\t\"http_data_format\":\t\"ruuvi\",\n"
               "\t\"http_auth\":\t\"none\",\n"
               "\t\"use_http_stat\":\ttrue,\n"
               "\t\"http_stat_url\":\t\"" RUUVI_GATEWAY_HTTP_STATUS_URL "\",\n"
               "\t\"http_stat_user\":\t\"\",\n"
               "\t\"use_mqtt\":\tfalse,\n"
               "\t\"mqtt_disable_retained_messages\":\tfalse,\n"
               "\t\"mqtt_transport\":\t\"TCP\",\n"
               "\t\"mqtt_server\":\t\"test.mosquitto.org\",\n"
               "\t\"mqtt_port\":\t1883,\n"
               "\t\"mqtt_prefix\":\t\"ruuvi/AA:BB:CC:DD:EE:FF/\",\n"
               "\t\"mqtt_client_id\":\t\"AA:BB:CC:DD:EE:FF\",\n"
               "\t\"mqtt_user\":\t\"\",\n"
               "\t\"lan_auth_type\":\t\"lan_auth_default\",\n"
               "\t\"lan_auth_user\":\t\"Admin\",\n"
               "\t\"lan_auth_api_key_use\":\tfalse,\n"
               "\t\"lan_auth_api_key_rw_use\":\tfalse,\n"
               "\t\"auto_update_cycle\":\t\"regular\",\n"
               "\t\"auto_update_weekdays_bitmask\":\t127,\n"
               "\t\"auto_update_interval_from\":\t0,\n"
               "\t\"auto_update_interval_to\":\t24,\n"
               "\t\"auto_update_tz_offset_hours\":\t3,\n"
               "\t\"ntp_use\":\ttrue,\n"
               "\t\"ntp_use_dhcp\":\tfalse,\n"
               "\t\"ntp_server1\":\t\"time.google.com\",\n"
               "\t\"ntp_server2\":\t\"time.cloudflare.com\",\n"
               "\t\"ntp_server3\":\t\"time.nist.gov\",\n"
               "\t\"ntp_server4\":\t\"pool.ntp.org\",\n"
               "\t\"company_id\":\t1177,\n"
               "\t\"company_use_filtering\":\ttrue,\n"
               "\t\"scan_coded_phy\":\tfalse,\n"
               "\t\"scan_1mbit_phy\":\ttrue,\n"
               "\t\"scan_extended_payload\":\ttrue,\n"
               "\t\"scan_channel_37\":\ttrue,\n"
               "\t\"scan_channel_38\":\ttrue,\n"
               "\t\"scan_channel_39\":\ttrue,\n"
               "\t\"scan_filter_allow_listed\":\tfalse,\n"
               "\t\"scan_filter_list\":\t[],\n"
               "\t\"coordinates\":\t\"\"\n"
               "}"),
        string(json_str.p_str));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    cjson_wrap_free_json_str(&json_str);
}

TEST_F(TestGwCfgRuuviJsonGenerate, gw_cfg_ruuvi_json_generate_non_default) // NOLINT
{
    const gw_cfg_t gw_cfg = {
        .device_info = {
            .esp32_fw_ver = { "v1.3.3", },
            .nrf52_fw_ver = { "v0.7.1", },
            .nrf52_mac_addr = { "AA:BB:CC:DD:EE:FF" },
            .nrf52_device_id = { "11:22:33:44:55:66:77:88" },
        },
        .ruuvi_cfg = {
            .http = {
                .use_http_ruuvi = false,
                .use_http = true,
                .http_url = { "https://my_server1.com" },
                .data_format = GW_CFG_HTTP_DATA_FORMAT_RUUVI,
                .auth_type = GW_CFG_HTTP_AUTH_TYPE_BASIC,
                .auth = {
                    .auth_basic = {
                        .user = {"h_user1"},
                        .password = {"h_user1"},
                    },
                },
            },
            .http_stat = {
                true,
                "https://my_server2.com/status",
                "h_user2",
                "h_pass2",
            },
            .mqtt = {
                true,
                false,
                MQTT_TRANSPORT_SSL,
                "ssl.test.mosquitto.org",
                1234,
                "my_prefix",
                "my_client_id",
                "m_user1",
                "m_pass1",
            },
            .lan_auth = {
                HTTP_SERVER_AUTH_TYPE_DIGEST,
                "l_user1",
                "l_pass1",
                "wH3F9SIiAA3rhG32aJki2Z7ekdFc0vtxuDhxl39zFvw=",
            },
            .auto_update = {
                .auto_update_cycle = AUTO_UPDATE_CYCLE_TYPE_BETA_TESTER,
                .auto_update_weekdays_bitmask = 0x3F,
                .auto_update_interval_from = 2,
                .auto_update_interval_to = 22,
                .auto_update_tz_offset_hours = 5,
            },
            .ntp = {
                .ntp_use = false,
                .ntp_use_dhcp = true,
                .ntp_server1 = { "time1.server.com" },
                .ntp_server2 = { "time2.server.com" },
                .ntp_server3 = { "time3.server.com" },
                .ntp_server4 = { "time4.server.com" },
            },
            .filter = {
                .company_id = RUUVI_COMPANY_ID + 1,
                .company_use_filtering = false,
            },
            .scan = {
                .scan_coded_phy = true,
                .scan_1mbit_phy = false,
                .scan_extended_payload = false,
                .scan_channel_37 = false,
                .scan_channel_38 = false,
                .scan_channel_39 = false,
            },
            .scan_filter = {
                .scan_filter_allow_listed = false,
                .scan_filter_length = 0,
                .scan_filter_list = {0},
            },
            .coordinates = { { 'q', 'w', 'e', '\0' } },
        },
        .eth_cfg = {
            true,
            false,
            "192.168.1.10",
            "255.255.255.0",
            "192.168.1.1",
            "8.8.8.8",
            "4.4.4.4",
        },
    };
    cjson_wrap_str_t json_str = cjson_wrap_str_null();

    ASSERT_TRUE(gw_cfg_ruuvi_json_generate(&gw_cfg, &json_str));
    ASSERT_NE(nullptr, json_str.p_str);
    ASSERT_EQ(
        string("{\n"
               "\t\"fw_ver\":\t\"v1.3.3\",\n"
               "\t\"nrf52_fw_ver\":\t\"v0.7.1\",\n"
               "\t\"gw_mac\":\t\"AA:BB:CC:DD:EE:FF\",\n"
               "\t\"wifi_sta_config\":\t{\n"
               "\t\t\"ssid\":\t\"\"\n"
               "\t},\n"
               "\t\"wifi_ap_config\":\t{\n"
               "\t\t\"channel\":\t1\n"
               "\t},\n"
               "\t\"use_eth\":\ttrue,\n"
               "\t\"eth_dhcp\":\tfalse,\n"
               "\t\"eth_static_ip\":\t\"192.168.1.10\",\n"
               "\t\"eth_netmask\":\t\"255.255.255.0\",\n"
               "\t\"eth_gw\":\t\"192.168.1.1\",\n"
               "\t\"eth_dns1\":\t\"8.8.8.8\",\n"
               "\t\"eth_dns2\":\t\"4.4.4.4\",\n"
               "\t\"remote_cfg_use\":\tfalse,\n"
               "\t\"remote_cfg_url\":\t\"\",\n"
               "\t\"remote_cfg_auth_type\":\t\"none\",\n"
               "\t\"remote_cfg_refresh_interval_minutes\":\t0,\n"
               "\t\"use_http_ruuvi\":\tfalse,\n"
               "\t\"use_http\":\ttrue,\n"
               "\t\"http_url\":\t\"https://my_server1.com\",\n"
               "\t\"http_data_format\":\t\"ruuvi\",\n"
               "\t\"http_auth\":\t\"basic\",\n"
               "\t\"http_user\":\t\"h_user1\",\n"
               "\t\"use_http_stat\":\ttrue,\n"
               "\t\"http_stat_url\":\t\"https://my_server2.com/status\",\n"
               "\t\"http_stat_user\":\t\"h_user2\",\n"
               "\t\"use_mqtt\":\ttrue,\n"
               "\t\"mqtt_disable_retained_messages\":\tfalse,\n"
               "\t\"mqtt_transport\":\t\"SSL\",\n"
               "\t\"mqtt_server\":\t\"ssl.test.mosquitto.org\",\n"
               "\t\"mqtt_port\":\t1234,\n"
               "\t\"mqtt_prefix\":\t\"my_prefix\",\n"
               "\t\"mqtt_client_id\":\t\"my_client_id\",\n"
               "\t\"mqtt_user\":\t\"m_user1\",\n"
               "\t\"lan_auth_type\":\t\"lan_auth_digest\",\n"
               "\t\"lan_auth_user\":\t\"l_user1\",\n"
               "\t\"lan_auth_api_key_use\":\ttrue,\n"
               "\t\"lan_auth_api_key_rw_use\":\tfalse,\n"
               "\t\"auto_update_cycle\":\t\"beta\",\n"
               "\t\"auto_update_weekdays_bitmask\":\t63,\n"
               "\t\"auto_update_interval_from\":\t2,\n"
               "\t\"auto_update_interval_to\":\t22,\n"
               "\t\"auto_update_tz_offset_hours\":\t5,\n"
               "\t\"ntp_use\":\tfalse,\n"
               "\t\"ntp_use_dhcp\":\ttrue,\n"
               "\t\"ntp_server1\":\t\"time1.server.com\",\n"
               "\t\"ntp_server2\":\t\"time2.server.com\",\n"
               "\t\"ntp_server3\":\t\"time3.server.com\",\n"
               "\t\"ntp_server4\":\t\"time4.server.com\",\n"
               "\t\"company_id\":\t1178,\n"
               "\t\"company_use_filtering\":\tfalse,\n"
               "\t\"scan_coded_phy\":\ttrue,\n"
               "\t\"scan_1mbit_phy\":\tfalse,\n"
               "\t\"scan_extended_payload\":\tfalse,\n"
               "\t\"scan_channel_37\":\tfalse,\n"
               "\t\"scan_channel_38\":\tfalse,\n"
               "\t\"scan_channel_39\":\tfalse,\n"
               "\t\"scan_filter_allow_listed\":\tfalse,\n"
               "\t\"scan_filter_list\":\t[],\n"
               "\t\"coordinates\":\t\"qwe\"\n"
               "}"),
        string(json_str.p_str));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    cjson_wrap_free_json_str(&json_str);
}

TEST_F(TestGwCfgRuuviJsonGenerate, gw_cfg_ruuvi_json_generate_malloc_failed_on_json_creation) // NOLINT
{
    const gw_cfg_t   gw_cfg   = get_gateway_config_default();
    cjson_wrap_str_t json_str = cjson_wrap_str_null();

    cJSON_Hooks hooks = {
        .malloc_fn = &os_malloc,
        .free_fn   = &os_free_internal,
    };
    cJSON_InitHooks(&hooks);
    this->m_malloc_fail_on_cnt = 1;

    ASSERT_FALSE(gw_cfg_ruuvi_json_generate(&gw_cfg, &json_str));
    ASSERT_EQ(nullptr, json_str.p_str);
    TEST_CHECK_LOG_RECORD(ESP_LOG_ERROR, string("Can't create json object"));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_TRUE(g_pTestClass->m_mem_alloc_trace.is_empty());
}

TEST_F(TestGwCfgRuuviJsonGenerate, gw_cfg_ruuvi_json_generate_malloc_failed_on_fw_ver) // NOLINT
{
    const gw_cfg_t   gw_cfg   = get_gateway_config_default();
    cjson_wrap_str_t json_str = cjson_wrap_str_null();

    cJSON_Hooks hooks = {
        .malloc_fn = &os_malloc,
        .free_fn   = &os_free_internal,
    };
    cJSON_InitHooks(&hooks);
    this->m_malloc_fail_on_cnt = 2;

    ASSERT_FALSE(gw_cfg_ruuvi_json_generate(&gw_cfg, &json_str));
    ASSERT_EQ(nullptr, json_str.p_str);
    TEST_CHECK_LOG_RECORD(ESP_LOG_ERROR, string("Can't add json item: fw_ver"));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_TRUE(g_pTestClass->m_mem_alloc_trace.is_empty());
}

TEST_F(TestGwCfgRuuviJsonGenerate, gw_cfg_ruuvi_json_generate_malloc_failed_on_fw_ver_2) // NOLINT
{
    const gw_cfg_t   gw_cfg   = get_gateway_config_default();
    cjson_wrap_str_t json_str = cjson_wrap_str_null();

    cJSON_Hooks hooks = {
        .malloc_fn = &os_malloc,
        .free_fn   = &os_free_internal,
    };
    cJSON_InitHooks(&hooks);
    this->m_malloc_fail_on_cnt = 3;

    ASSERT_FALSE(gw_cfg_ruuvi_json_generate(&gw_cfg, &json_str));
    ASSERT_EQ(nullptr, json_str.p_str);
    TEST_CHECK_LOG_RECORD(ESP_LOG_ERROR, string("Can't add json item: fw_ver"));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_TRUE(g_pTestClass->m_mem_alloc_trace.is_empty());
}

TEST_F(TestGwCfgRuuviJsonGenerate, gw_cfg_ruuvi_json_generate_malloc_failed_on_fw_ver_3) // NOLINT
{
    const gw_cfg_t   gw_cfg   = get_gateway_config_default();
    cjson_wrap_str_t json_str = cjson_wrap_str_null();

    cJSON_Hooks hooks = {
        .malloc_fn = &os_malloc,
        .free_fn   = &os_free_internal,
    };
    cJSON_InitHooks(&hooks);
    this->m_malloc_fail_on_cnt = 4;

    ASSERT_FALSE(gw_cfg_ruuvi_json_generate(&gw_cfg, &json_str));
    ASSERT_EQ(nullptr, json_str.p_str);
    TEST_CHECK_LOG_RECORD(ESP_LOG_ERROR, string("Can't add json item: fw_ver"));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_TRUE(g_pTestClass->m_mem_alloc_trace.is_empty());
}

TEST_F(TestGwCfgRuuviJsonGenerate, gw_cfg_ruuvi_json_generate_malloc_failed_on_nrf52_fw_ver) // NOLINT
{
    const gw_cfg_t   gw_cfg   = get_gateway_config_default();
    cjson_wrap_str_t json_str = cjson_wrap_str_null();

    cJSON_Hooks hooks = {
        .malloc_fn = &os_malloc,
        .free_fn   = &os_free_internal,
    };
    cJSON_InitHooks(&hooks);
    this->m_malloc_fail_on_cnt = 5;

    ASSERT_FALSE(gw_cfg_ruuvi_json_generate(&gw_cfg, &json_str));
    ASSERT_EQ(nullptr, json_str.p_str);
    TEST_CHECK_LOG_RECORD(ESP_LOG_ERROR, string("Can't add json item: nrf52_fw_ver"));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_TRUE(g_pTestClass->m_mem_alloc_trace.is_empty());
}

TEST_F(TestGwCfgRuuviJsonGenerate, gw_cfg_ruuvi_json_generate_malloc_failed_on_nrf52_fw_ver_2) // NOLINT
{
    const gw_cfg_t   gw_cfg   = get_gateway_config_default();
    cjson_wrap_str_t json_str = cjson_wrap_str_null();

    cJSON_Hooks hooks = {
        .malloc_fn = &os_malloc,
        .free_fn   = &os_free_internal,
    };
    cJSON_InitHooks(&hooks);
    this->m_malloc_fail_on_cnt = 6;

    ASSERT_FALSE(gw_cfg_ruuvi_json_generate(&gw_cfg, &json_str));
    ASSERT_EQ(nullptr, json_str.p_str);
    TEST_CHECK_LOG_RECORD(ESP_LOG_ERROR, string("Can't add json item: nrf52_fw_ver"));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_TRUE(g_pTestClass->m_mem_alloc_trace.is_empty());
}

TEST_F(TestGwCfgRuuviJsonGenerate, gw_cfg_ruuvi_json_generate_malloc_failed_on_nrf52_fw_ver_3) // NOLINT
{
    const gw_cfg_t   gw_cfg   = get_gateway_config_default();
    cjson_wrap_str_t json_str = cjson_wrap_str_null();

    cJSON_Hooks hooks = {
        .malloc_fn = &os_malloc,
        .free_fn   = &os_free_internal,
    };
    cJSON_InitHooks(&hooks);
    this->m_malloc_fail_on_cnt = 7;

    ASSERT_FALSE(gw_cfg_ruuvi_json_generate(&gw_cfg, &json_str));
    ASSERT_EQ(nullptr, json_str.p_str);
    TEST_CHECK_LOG_RECORD(ESP_LOG_ERROR, string("Can't add json item: nrf52_fw_ver"));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_TRUE(g_pTestClass->m_mem_alloc_trace.is_empty());
}

TEST_F(TestGwCfgRuuviJsonGenerate, gw_cfg_ruuvi_json_generate_malloc_failed_on_gw_mac) // NOLINT
{
    const gw_cfg_t   gw_cfg   = get_gateway_config_default();
    cjson_wrap_str_t json_str = cjson_wrap_str_null();

    cJSON_Hooks hooks = {
        .malloc_fn = &os_malloc,
        .free_fn   = &os_free_internal,
    };
    cJSON_InitHooks(&hooks);
    this->m_malloc_fail_on_cnt = 8;

    ASSERT_FALSE(gw_cfg_ruuvi_json_generate(&gw_cfg, &json_str));
    ASSERT_EQ(nullptr, json_str.p_str);
    TEST_CHECK_LOG_RECORD(ESP_LOG_ERROR, string("Can't add json item: gw_mac"));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_TRUE(g_pTestClass->m_mem_alloc_trace.is_empty());
}

TEST_F(TestGwCfgRuuviJsonGenerate, gw_cfg_ruuvi_json_generate_malloc_failed_on_nrf52_gw_mac_2) // NOLINT
{
    const gw_cfg_t   gw_cfg   = get_gateway_config_default();
    cjson_wrap_str_t json_str = cjson_wrap_str_null();

    cJSON_Hooks hooks = {
        .malloc_fn = &os_malloc,
        .free_fn   = &os_free_internal,
    };
    cJSON_InitHooks(&hooks);
    this->m_malloc_fail_on_cnt = 9;

    ASSERT_FALSE(gw_cfg_ruuvi_json_generate(&gw_cfg, &json_str));
    ASSERT_EQ(nullptr, json_str.p_str);
    TEST_CHECK_LOG_RECORD(ESP_LOG_ERROR, string("Can't add json item: gw_mac"));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_TRUE(g_pTestClass->m_mem_alloc_trace.is_empty());
}

TEST_F(TestGwCfgRuuviJsonGenerate, gw_cfg_ruuvi_json_generate_malloc_failed_on_nrf52_gw_mac_3) // NOLINT
{
    const gw_cfg_t   gw_cfg   = get_gateway_config_default();
    cjson_wrap_str_t json_str = cjson_wrap_str_null();

    cJSON_Hooks hooks = {
        .malloc_fn = &os_malloc,
        .free_fn   = &os_free_internal,
    };
    cJSON_InitHooks(&hooks);
    this->m_malloc_fail_on_cnt = 10;

    ASSERT_FALSE(gw_cfg_ruuvi_json_generate(&gw_cfg, &json_str));
    ASSERT_EQ(nullptr, json_str.p_str);
    TEST_CHECK_LOG_RECORD(ESP_LOG_ERROR, string("Can't add json item: gw_mac"));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_TRUE(g_pTestClass->m_mem_alloc_trace.is_empty());
}

TEST_F(TestGwCfgRuuviJsonGenerate, gw_cfg_ruuvi_json_generate_malloc_failed_on_wifi_sta_config) // NOLINT
{
    const gw_cfg_t   gw_cfg   = get_gateway_config_default();
    cjson_wrap_str_t json_str = cjson_wrap_str_null();

    cJSON_Hooks hooks = {
        .malloc_fn = &os_malloc,
        .free_fn   = &os_free_internal,
    };
    cJSON_InitHooks(&hooks);
    this->m_malloc_fail_on_cnt = 11;

    ASSERT_FALSE(gw_cfg_ruuvi_json_generate(&gw_cfg, &json_str));
    ASSERT_EQ(nullptr, json_str.p_str);
    TEST_CHECK_LOG_RECORD(ESP_LOG_ERROR, string("Can't add json item: wifi_sta_config"));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_TRUE(g_pTestClass->m_mem_alloc_trace.is_empty());
}

TEST_F(TestGwCfgRuuviJsonGenerate, gw_cfg_ruuvi_json_generate_malloc_failed_on_wifi_sta_config_2) // NOLINT
{
    const gw_cfg_t   gw_cfg   = get_gateway_config_default();
    cjson_wrap_str_t json_str = cjson_wrap_str_null();

    cJSON_Hooks hooks = {
        .malloc_fn = &os_malloc,
        .free_fn   = &os_free_internal,
    };
    cJSON_InitHooks(&hooks);
    this->m_malloc_fail_on_cnt = 12;

    ASSERT_FALSE(gw_cfg_ruuvi_json_generate(&gw_cfg, &json_str));
    ASSERT_EQ(nullptr, json_str.p_str);
    TEST_CHECK_LOG_RECORD(ESP_LOG_ERROR, string("Can't add json item: wifi_sta_config"));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_TRUE(g_pTestClass->m_mem_alloc_trace.is_empty());
}

TEST_F(TestGwCfgRuuviJsonGenerate, gw_cfg_ruuvi_json_generate_malloc_failed_on_wifi_sta_config_ssid_1) // NOLINT
{
    const gw_cfg_t   gw_cfg   = get_gateway_config_default();
    cjson_wrap_str_t json_str = cjson_wrap_str_null();

    cJSON_Hooks hooks = {
        .malloc_fn = &os_malloc,
        .free_fn   = &os_free_internal,
    };
    cJSON_InitHooks(&hooks);
    this->m_malloc_fail_on_cnt = 13;

    ASSERT_FALSE(gw_cfg_ruuvi_json_generate(&gw_cfg, &json_str));
    ASSERT_EQ(nullptr, json_str.p_str);
    TEST_CHECK_LOG_RECORD(ESP_LOG_ERROR, string("Can't add json item: ssid"));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_TRUE(g_pTestClass->m_mem_alloc_trace.is_empty());
}

TEST_F(TestGwCfgRuuviJsonGenerate, gw_cfg_ruuvi_json_generate_malloc_failed_on_wifi_sta_config_ssid_2) // NOLINT
{
    const gw_cfg_t   gw_cfg   = get_gateway_config_default();
    cjson_wrap_str_t json_str = cjson_wrap_str_null();

    cJSON_Hooks hooks = {
        .malloc_fn = &os_malloc,
        .free_fn   = &os_free_internal,
    };
    cJSON_InitHooks(&hooks);
    this->m_malloc_fail_on_cnt = 14;

    ASSERT_FALSE(gw_cfg_ruuvi_json_generate(&gw_cfg, &json_str));
    ASSERT_EQ(nullptr, json_str.p_str);
    TEST_CHECK_LOG_RECORD(ESP_LOG_ERROR, string("Can't add json item: ssid"));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_TRUE(g_pTestClass->m_mem_alloc_trace.is_empty());
}

TEST_F(TestGwCfgRuuviJsonGenerate, gw_cfg_ruuvi_json_generate_malloc_failed_on_wifi_sta_config_ssid_3) // NOLINT
{
    const gw_cfg_t   gw_cfg   = get_gateway_config_default();
    cjson_wrap_str_t json_str = cjson_wrap_str_null();

    cJSON_Hooks hooks = {
        .malloc_fn = &os_malloc,
        .free_fn   = &os_free_internal,
    };
    cJSON_InitHooks(&hooks);
    this->m_malloc_fail_on_cnt = 15;

    ASSERT_FALSE(gw_cfg_ruuvi_json_generate(&gw_cfg, &json_str));
    ASSERT_EQ(nullptr, json_str.p_str);
    TEST_CHECK_LOG_RECORD(ESP_LOG_ERROR, string("Can't add json item: ssid"));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_TRUE(g_pTestClass->m_mem_alloc_trace.is_empty());
}

TEST_F(TestGwCfgRuuviJsonGenerate, gw_cfg_ruuvi_json_generate_malloc_failed_on_wifi_ap_config_1) // NOLINT
{
    const gw_cfg_t   gw_cfg   = get_gateway_config_default();
    cjson_wrap_str_t json_str = cjson_wrap_str_null();

    cJSON_Hooks hooks = {
        .malloc_fn = &os_malloc,
        .free_fn   = &os_free_internal,
    };
    cJSON_InitHooks(&hooks);
    this->m_malloc_fail_on_cnt = 16;

    ASSERT_FALSE(gw_cfg_ruuvi_json_generate(&gw_cfg, &json_str));
    ASSERT_EQ(nullptr, json_str.p_str);
    TEST_CHECK_LOG_RECORD(ESP_LOG_ERROR, string("Can't add json item: wifi_ap_config"));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_TRUE(g_pTestClass->m_mem_alloc_trace.is_empty());
}

TEST_F(TestGwCfgRuuviJsonGenerate, gw_cfg_ruuvi_json_generate_malloc_failed_on_wifi_ap_config_2) // NOLINT
{
    const gw_cfg_t   gw_cfg   = get_gateway_config_default();
    cjson_wrap_str_t json_str = cjson_wrap_str_null();

    cJSON_Hooks hooks = {
        .malloc_fn = &os_malloc,
        .free_fn   = &os_free_internal,
    };
    cJSON_InitHooks(&hooks);
    this->m_malloc_fail_on_cnt = 17;

    ASSERT_FALSE(gw_cfg_ruuvi_json_generate(&gw_cfg, &json_str));
    ASSERT_EQ(nullptr, json_str.p_str);
    TEST_CHECK_LOG_RECORD(ESP_LOG_ERROR, string("Can't add json item: wifi_ap_config"));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_TRUE(g_pTestClass->m_mem_alloc_trace.is_empty());
}

TEST_F(TestGwCfgRuuviJsonGenerate, gw_cfg_ruuvi_json_generate_malloc_failed_on_wifi_ap_config_channel_1) // NOLINT
{
    const gw_cfg_t   gw_cfg   = get_gateway_config_default();
    cjson_wrap_str_t json_str = cjson_wrap_str_null();

    cJSON_Hooks hooks = {
        .malloc_fn = &os_malloc,
        .free_fn   = &os_free_internal,
    };
    cJSON_InitHooks(&hooks);
    this->m_malloc_fail_on_cnt = 18;

    ASSERT_FALSE(gw_cfg_ruuvi_json_generate(&gw_cfg, &json_str));
    ASSERT_EQ(nullptr, json_str.p_str);
    TEST_CHECK_LOG_RECORD(ESP_LOG_ERROR, string("Can't add json item: channel"));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_TRUE(g_pTestClass->m_mem_alloc_trace.is_empty());
}

TEST_F(TestGwCfgRuuviJsonGenerate, gw_cfg_ruuvi_json_generate_malloc_failed_on_wifi_ap_config_channel_2) // NOLINT
{
    const gw_cfg_t   gw_cfg   = get_gateway_config_default();
    cjson_wrap_str_t json_str = cjson_wrap_str_null();

    cJSON_Hooks hooks = {
        .malloc_fn = &os_malloc,
        .free_fn   = &os_free_internal,
    };
    cJSON_InitHooks(&hooks);
    this->m_malloc_fail_on_cnt = 19;

    ASSERT_FALSE(gw_cfg_ruuvi_json_generate(&gw_cfg, &json_str));
    ASSERT_EQ(nullptr, json_str.p_str);
    TEST_CHECK_LOG_RECORD(ESP_LOG_ERROR, string("Can't add json item: channel"));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_TRUE(g_pTestClass->m_mem_alloc_trace.is_empty());
}

TEST_F(TestGwCfgRuuviJsonGenerate, gw_cfg_ruuvi_json_generate_malloc_failed_on_use_eth) // NOLINT
{
    const gw_cfg_t   gw_cfg   = get_gateway_config_default();
    cjson_wrap_str_t json_str = cjson_wrap_str_null();

    cJSON_Hooks hooks = {
        .malloc_fn = &os_malloc,
        .free_fn   = &os_free_internal,
    };
    cJSON_InitHooks(&hooks);
    this->m_malloc_fail_on_cnt = 20;

    ASSERT_FALSE(gw_cfg_ruuvi_json_generate(&gw_cfg, &json_str));
    ASSERT_EQ(nullptr, json_str.p_str);
    TEST_CHECK_LOG_RECORD(ESP_LOG_ERROR, string("Can't add json item: use_eth"));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_TRUE(g_pTestClass->m_mem_alloc_trace.is_empty());
}

TEST_F(TestGwCfgRuuviJsonGenerate, gw_cfg_ruuvi_json_generate_malloc_failed_on_use_eth_2) // NOLINT
{
    const gw_cfg_t   gw_cfg   = get_gateway_config_default();
    cjson_wrap_str_t json_str = cjson_wrap_str_null();

    cJSON_Hooks hooks = {
        .malloc_fn = &os_malloc,
        .free_fn   = &os_free_internal,
    };
    cJSON_InitHooks(&hooks);
    this->m_malloc_fail_on_cnt = 21;

    ASSERT_FALSE(gw_cfg_ruuvi_json_generate(&gw_cfg, &json_str));
    ASSERT_EQ(nullptr, json_str.p_str);
    TEST_CHECK_LOG_RECORD(ESP_LOG_ERROR, string("Can't add json item: use_eth"));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_TRUE(g_pTestClass->m_mem_alloc_trace.is_empty());
}

TEST_F(TestGwCfgRuuviJsonGenerate, gw_cfg_ruuvi_json_generate_malloc_failed_on_eth_dhcp) // NOLINT
{
    const gw_cfg_t   gw_cfg   = get_gateway_config_default();
    cjson_wrap_str_t json_str = cjson_wrap_str_null();

    cJSON_Hooks hooks = {
        .malloc_fn = &os_malloc,
        .free_fn   = &os_free_internal,
    };
    cJSON_InitHooks(&hooks);
    this->m_malloc_fail_on_cnt = 22;

    ASSERT_FALSE(gw_cfg_ruuvi_json_generate(&gw_cfg, &json_str));
    ASSERT_EQ(nullptr, json_str.p_str);
    TEST_CHECK_LOG_RECORD(ESP_LOG_ERROR, string("Can't add json item: eth_dhcp"));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_TRUE(g_pTestClass->m_mem_alloc_trace.is_empty());
}

TEST_F(TestGwCfgRuuviJsonGenerate, gw_cfg_ruuvi_json_generate_malloc_failed_on_eth_dhcp_2) // NOLINT
{
    const gw_cfg_t   gw_cfg   = get_gateway_config_default();
    cjson_wrap_str_t json_str = cjson_wrap_str_null();

    cJSON_Hooks hooks = {
        .malloc_fn = &os_malloc,
        .free_fn   = &os_free_internal,
    };
    cJSON_InitHooks(&hooks);
    this->m_malloc_fail_on_cnt = 23;

    ASSERT_FALSE(gw_cfg_ruuvi_json_generate(&gw_cfg, &json_str));
    ASSERT_EQ(nullptr, json_str.p_str);
    TEST_CHECK_LOG_RECORD(ESP_LOG_ERROR, string("Can't add json item: eth_dhcp"));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_TRUE(g_pTestClass->m_mem_alloc_trace.is_empty());
}

TEST_F(TestGwCfgRuuviJsonGenerate, gw_cfg_ruuvi_json_generate_malloc_failed_on_eth_static_ip) // NOLINT
{
    const gw_cfg_t   gw_cfg   = get_gateway_config_default();
    cjson_wrap_str_t json_str = cjson_wrap_str_null();

    cJSON_Hooks hooks = {
        .malloc_fn = &os_malloc,
        .free_fn   = &os_free_internal,
    };
    cJSON_InitHooks(&hooks);
    this->m_malloc_fail_on_cnt = 24;

    ASSERT_FALSE(gw_cfg_ruuvi_json_generate(&gw_cfg, &json_str));
    ASSERT_EQ(nullptr, json_str.p_str);
    TEST_CHECK_LOG_RECORD(ESP_LOG_ERROR, string("Can't add json item: eth_static_ip"));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_TRUE(g_pTestClass->m_mem_alloc_trace.is_empty());
}

TEST_F(TestGwCfgRuuviJsonGenerate, gw_cfg_ruuvi_json_generate_malloc_failed_on_eth_static_ip_2) // NOLINT
{
    const gw_cfg_t   gw_cfg   = get_gateway_config_default();
    cjson_wrap_str_t json_str = cjson_wrap_str_null();

    cJSON_Hooks hooks = {
        .malloc_fn = &os_malloc,
        .free_fn   = &os_free_internal,
    };
    cJSON_InitHooks(&hooks);
    this->m_malloc_fail_on_cnt = 25;

    ASSERT_FALSE(gw_cfg_ruuvi_json_generate(&gw_cfg, &json_str));
    ASSERT_EQ(nullptr, json_str.p_str);
    TEST_CHECK_LOG_RECORD(ESP_LOG_ERROR, string("Can't add json item: eth_static_ip"));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_TRUE(g_pTestClass->m_mem_alloc_trace.is_empty());
}

TEST_F(TestGwCfgRuuviJsonGenerate, gw_cfg_ruuvi_json_generate_malloc_failed_on_eth_static_ip_3) // NOLINT
{
    const gw_cfg_t   gw_cfg   = get_gateway_config_default();
    cjson_wrap_str_t json_str = cjson_wrap_str_null();

    cJSON_Hooks hooks = {
        .malloc_fn = &os_malloc,
        .free_fn   = &os_free_internal,
    };
    cJSON_InitHooks(&hooks);
    this->m_malloc_fail_on_cnt = 26;

    ASSERT_FALSE(gw_cfg_ruuvi_json_generate(&gw_cfg, &json_str));
    ASSERT_EQ(nullptr, json_str.p_str);
    TEST_CHECK_LOG_RECORD(ESP_LOG_ERROR, string("Can't add json item: eth_static_ip"));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_TRUE(g_pTestClass->m_mem_alloc_trace.is_empty());
}

TEST_F(TestGwCfgRuuviJsonGenerate, gw_cfg_ruuvi_json_generate_malloc_failed_on_eth_netmask) // NOLINT
{
    const gw_cfg_t   gw_cfg   = get_gateway_config_default();
    cjson_wrap_str_t json_str = cjson_wrap_str_null();

    cJSON_Hooks hooks = {
        .malloc_fn = &os_malloc,
        .free_fn   = &os_free_internal,
    };
    cJSON_InitHooks(&hooks);
    this->m_malloc_fail_on_cnt = 27;

    ASSERT_FALSE(gw_cfg_ruuvi_json_generate(&gw_cfg, &json_str));
    ASSERT_EQ(nullptr, json_str.p_str);
    TEST_CHECK_LOG_RECORD(ESP_LOG_ERROR, string("Can't add json item: eth_netmask"));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_TRUE(g_pTestClass->m_mem_alloc_trace.is_empty());
}

TEST_F(TestGwCfgRuuviJsonGenerate, gw_cfg_ruuvi_json_generate_malloc_failed_on_eth_netmask_2) // NOLINT
{
    const gw_cfg_t   gw_cfg   = get_gateway_config_default();
    cjson_wrap_str_t json_str = cjson_wrap_str_null();

    cJSON_Hooks hooks = {
        .malloc_fn = &os_malloc,
        .free_fn   = &os_free_internal,
    };
    cJSON_InitHooks(&hooks);
    this->m_malloc_fail_on_cnt = 28;

    ASSERT_FALSE(gw_cfg_ruuvi_json_generate(&gw_cfg, &json_str));
    ASSERT_EQ(nullptr, json_str.p_str);
    TEST_CHECK_LOG_RECORD(ESP_LOG_ERROR, string("Can't add json item: eth_netmask"));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_TRUE(g_pTestClass->m_mem_alloc_trace.is_empty());
}

TEST_F(TestGwCfgRuuviJsonGenerate, gw_cfg_ruuvi_json_generate_malloc_failed_on_eth_netmask_3) // NOLINT
{
    const gw_cfg_t   gw_cfg   = get_gateway_config_default();
    cjson_wrap_str_t json_str = cjson_wrap_str_null();

    cJSON_Hooks hooks = {
        .malloc_fn = &os_malloc,
        .free_fn   = &os_free_internal,
    };
    cJSON_InitHooks(&hooks);
    this->m_malloc_fail_on_cnt = 29;

    ASSERT_FALSE(gw_cfg_ruuvi_json_generate(&gw_cfg, &json_str));
    ASSERT_EQ(nullptr, json_str.p_str);
    TEST_CHECK_LOG_RECORD(ESP_LOG_ERROR, string("Can't add json item: eth_netmask"));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_TRUE(g_pTestClass->m_mem_alloc_trace.is_empty());
}

TEST_F(TestGwCfgRuuviJsonGenerate, gw_cfg_ruuvi_json_generate_malloc_failed_on_eth_gw) // NOLINT
{
    const gw_cfg_t   gw_cfg   = get_gateway_config_default();
    cjson_wrap_str_t json_str = cjson_wrap_str_null();

    cJSON_Hooks hooks = {
        .malloc_fn = &os_malloc,
        .free_fn   = &os_free_internal,
    };
    cJSON_InitHooks(&hooks);
    this->m_malloc_fail_on_cnt = 30;

    ASSERT_FALSE(gw_cfg_ruuvi_json_generate(&gw_cfg, &json_str));
    ASSERT_EQ(nullptr, json_str.p_str);
    TEST_CHECK_LOG_RECORD(ESP_LOG_ERROR, string("Can't add json item: eth_gw"));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_TRUE(g_pTestClass->m_mem_alloc_trace.is_empty());
}

TEST_F(TestGwCfgRuuviJsonGenerate, gw_cfg_ruuvi_json_generate_malloc_failed_on_eth_gw_2) // NOLINT
{
    const gw_cfg_t   gw_cfg   = get_gateway_config_default();
    cjson_wrap_str_t json_str = cjson_wrap_str_null();

    cJSON_Hooks hooks = {
        .malloc_fn = &os_malloc,
        .free_fn   = &os_free_internal,
    };
    cJSON_InitHooks(&hooks);
    this->m_malloc_fail_on_cnt = 31;

    ASSERT_FALSE(gw_cfg_ruuvi_json_generate(&gw_cfg, &json_str));
    ASSERT_EQ(nullptr, json_str.p_str);
    TEST_CHECK_LOG_RECORD(ESP_LOG_ERROR, string("Can't add json item: eth_gw"));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_TRUE(g_pTestClass->m_mem_alloc_trace.is_empty());
}

TEST_F(TestGwCfgRuuviJsonGenerate, gw_cfg_ruuvi_json_generate_malloc_failed_on_eth_gw_3) // NOLINT
{
    const gw_cfg_t   gw_cfg   = get_gateway_config_default();
    cjson_wrap_str_t json_str = cjson_wrap_str_null();

    cJSON_Hooks hooks = {
        .malloc_fn = &os_malloc,
        .free_fn   = &os_free_internal,
    };
    cJSON_InitHooks(&hooks);
    this->m_malloc_fail_on_cnt = 32;

    ASSERT_FALSE(gw_cfg_ruuvi_json_generate(&gw_cfg, &json_str));
    ASSERT_EQ(nullptr, json_str.p_str);
    TEST_CHECK_LOG_RECORD(ESP_LOG_ERROR, string("Can't add json item: eth_gw"));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_TRUE(g_pTestClass->m_mem_alloc_trace.is_empty());
}

TEST_F(TestGwCfgRuuviJsonGenerate, gw_cfg_ruuvi_json_generate_malloc_failed_on_eth_dns1) // NOLINT
{
    const gw_cfg_t   gw_cfg   = get_gateway_config_default();
    cjson_wrap_str_t json_str = cjson_wrap_str_null();

    cJSON_Hooks hooks = {
        .malloc_fn = &os_malloc,
        .free_fn   = &os_free_internal,
    };
    cJSON_InitHooks(&hooks);
    this->m_malloc_fail_on_cnt = 33;

    ASSERT_FALSE(gw_cfg_ruuvi_json_generate(&gw_cfg, &json_str));
    ASSERT_EQ(nullptr, json_str.p_str);
    TEST_CHECK_LOG_RECORD(ESP_LOG_ERROR, string("Can't add json item: eth_dns1"));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_TRUE(g_pTestClass->m_mem_alloc_trace.is_empty());
}

TEST_F(TestGwCfgRuuviJsonGenerate, gw_cfg_ruuvi_json_generate_malloc_failed_on_eth_dns1_2) // NOLINT
{
    const gw_cfg_t   gw_cfg   = get_gateway_config_default();
    cjson_wrap_str_t json_str = cjson_wrap_str_null();

    cJSON_Hooks hooks = {
        .malloc_fn = &os_malloc,
        .free_fn   = &os_free_internal,
    };
    cJSON_InitHooks(&hooks);
    this->m_malloc_fail_on_cnt = 34;

    ASSERT_FALSE(gw_cfg_ruuvi_json_generate(&gw_cfg, &json_str));
    ASSERT_EQ(nullptr, json_str.p_str);
    TEST_CHECK_LOG_RECORD(ESP_LOG_ERROR, string("Can't add json item: eth_dns1"));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_TRUE(g_pTestClass->m_mem_alloc_trace.is_empty());
}

TEST_F(TestGwCfgRuuviJsonGenerate, gw_cfg_ruuvi_json_generate_malloc_failed_on_eth_dns1_3) // NOLINT
{
    const gw_cfg_t   gw_cfg   = get_gateway_config_default();
    cjson_wrap_str_t json_str = cjson_wrap_str_null();

    cJSON_Hooks hooks = {
        .malloc_fn = &os_malloc,
        .free_fn   = &os_free_internal,
    };
    cJSON_InitHooks(&hooks);
    this->m_malloc_fail_on_cnt = 35;

    ASSERT_FALSE(gw_cfg_ruuvi_json_generate(&gw_cfg, &json_str));
    ASSERT_EQ(nullptr, json_str.p_str);
    TEST_CHECK_LOG_RECORD(ESP_LOG_ERROR, string("Can't add json item: eth_dns1"));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_TRUE(g_pTestClass->m_mem_alloc_trace.is_empty());
}

TEST_F(TestGwCfgRuuviJsonGenerate, gw_cfg_ruuvi_json_generate_malloc_failed_on_eth_dns2) // NOLINT
{
    const gw_cfg_t   gw_cfg   = get_gateway_config_default();
    cjson_wrap_str_t json_str = cjson_wrap_str_null();

    cJSON_Hooks hooks = {
        .malloc_fn = &os_malloc,
        .free_fn   = &os_free_internal,
    };
    cJSON_InitHooks(&hooks);
    this->m_malloc_fail_on_cnt = 36;

    ASSERT_FALSE(gw_cfg_ruuvi_json_generate(&gw_cfg, &json_str));
    ASSERT_EQ(nullptr, json_str.p_str);
    TEST_CHECK_LOG_RECORD(ESP_LOG_ERROR, string("Can't add json item: eth_dns2"));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_TRUE(g_pTestClass->m_mem_alloc_trace.is_empty());
}

TEST_F(TestGwCfgRuuviJsonGenerate, gw_cfg_ruuvi_json_generate_malloc_failed_on_eth_dns2_1) // NOLINT
{
    const gw_cfg_t   gw_cfg   = get_gateway_config_default();
    cjson_wrap_str_t json_str = cjson_wrap_str_null();

    cJSON_Hooks hooks = {
        .malloc_fn = &os_malloc,
        .free_fn   = &os_free_internal,
    };
    cJSON_InitHooks(&hooks);
    this->m_malloc_fail_on_cnt = 37;

    ASSERT_FALSE(gw_cfg_ruuvi_json_generate(&gw_cfg, &json_str));
    ASSERT_EQ(nullptr, json_str.p_str);
    TEST_CHECK_LOG_RECORD(ESP_LOG_ERROR, string("Can't add json item: eth_dns2"));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_TRUE(g_pTestClass->m_mem_alloc_trace.is_empty());
}

TEST_F(TestGwCfgRuuviJsonGenerate, gw_cfg_ruuvi_json_generate_malloc_failed_on_eth_dns2_2) // NOLINT
{
    const gw_cfg_t   gw_cfg   = get_gateway_config_default();
    cjson_wrap_str_t json_str = cjson_wrap_str_null();

    cJSON_Hooks hooks = {
        .malloc_fn = &os_malloc,
        .free_fn   = &os_free_internal,
    };
    cJSON_InitHooks(&hooks);
    this->m_malloc_fail_on_cnt = 38;

    ASSERT_FALSE(gw_cfg_ruuvi_json_generate(&gw_cfg, &json_str));
    ASSERT_EQ(nullptr, json_str.p_str);
    TEST_CHECK_LOG_RECORD(ESP_LOG_ERROR, string("Can't add json item: eth_dns2"));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_TRUE(g_pTestClass->m_mem_alloc_trace.is_empty());
}

TEST_F(TestGwCfgRuuviJsonGenerate, gw_cfg_ruuvi_json_generate_malloc_failed_on_remote_cfg_use) // NOLINT
{
    const gw_cfg_t   gw_cfg   = get_gateway_config_default();
    cjson_wrap_str_t json_str = cjson_wrap_str_null();

    cJSON_Hooks hooks = {
        .malloc_fn = &os_malloc,
        .free_fn   = &os_free_internal,
    };
    cJSON_InitHooks(&hooks);
    this->m_malloc_fail_on_cnt = 39;

    ASSERT_FALSE(gw_cfg_ruuvi_json_generate(&gw_cfg, &json_str));
    ASSERT_EQ(nullptr, json_str.p_str);
    TEST_CHECK_LOG_RECORD(ESP_LOG_ERROR, string("Can't add json item: remote_cfg_use"));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_TRUE(g_pTestClass->m_mem_alloc_trace.is_empty());
}

TEST_F(TestGwCfgRuuviJsonGenerate, gw_cfg_ruuvi_json_generate_malloc_failed_on_remote_cfg_use_2) // NOLINT
{
    const gw_cfg_t   gw_cfg   = get_gateway_config_default();
    cjson_wrap_str_t json_str = cjson_wrap_str_null();

    cJSON_Hooks hooks = {
        .malloc_fn = &os_malloc,
        .free_fn   = &os_free_internal,
    };
    cJSON_InitHooks(&hooks);
    this->m_malloc_fail_on_cnt = 40;

    ASSERT_FALSE(gw_cfg_ruuvi_json_generate(&gw_cfg, &json_str));
    ASSERT_EQ(nullptr, json_str.p_str);
    TEST_CHECK_LOG_RECORD(ESP_LOG_ERROR, string("Can't add json item: remote_cfg_use"));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_TRUE(g_pTestClass->m_mem_alloc_trace.is_empty());
}

TEST_F(TestGwCfgRuuviJsonGenerate, gw_cfg_ruuvi_json_generate_malloc_failed_on_remote_cfg_url) // NOLINT
{
    const gw_cfg_t   gw_cfg   = get_gateway_config_default();
    cjson_wrap_str_t json_str = cjson_wrap_str_null();

    cJSON_Hooks hooks = {
        .malloc_fn = &os_malloc,
        .free_fn   = &os_free_internal,
    };
    cJSON_InitHooks(&hooks);
    this->m_malloc_fail_on_cnt = 41;

    ASSERT_FALSE(gw_cfg_ruuvi_json_generate(&gw_cfg, &json_str));
    ASSERT_EQ(nullptr, json_str.p_str);
    TEST_CHECK_LOG_RECORD(ESP_LOG_ERROR, string("Can't add json item: remote_cfg_url"));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_TRUE(g_pTestClass->m_mem_alloc_trace.is_empty());
}

TEST_F(TestGwCfgRuuviJsonGenerate, gw_cfg_ruuvi_json_generate_malloc_failed_on_remote_cfg_url_2) // NOLINT
{
    const gw_cfg_t   gw_cfg   = get_gateway_config_default();
    cjson_wrap_str_t json_str = cjson_wrap_str_null();

    cJSON_Hooks hooks = {
        .malloc_fn = &os_malloc,
        .free_fn   = &os_free_internal,
    };
    cJSON_InitHooks(&hooks);
    this->m_malloc_fail_on_cnt = 42;

    ASSERT_FALSE(gw_cfg_ruuvi_json_generate(&gw_cfg, &json_str));
    ASSERT_EQ(nullptr, json_str.p_str);
    TEST_CHECK_LOG_RECORD(ESP_LOG_ERROR, string("Can't add json item: remote_cfg_url"));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_TRUE(g_pTestClass->m_mem_alloc_trace.is_empty());
}

TEST_F(TestGwCfgRuuviJsonGenerate, gw_cfg_ruuvi_json_generate_malloc_failed_on_remote_cfg_url_3) // NOLINT
{
    const gw_cfg_t   gw_cfg   = get_gateway_config_default();
    cjson_wrap_str_t json_str = cjson_wrap_str_null();

    cJSON_Hooks hooks = {
        .malloc_fn = &os_malloc,
        .free_fn   = &os_free_internal,
    };
    cJSON_InitHooks(&hooks);
    this->m_malloc_fail_on_cnt = 43;

    ASSERT_FALSE(gw_cfg_ruuvi_json_generate(&gw_cfg, &json_str));
    ASSERT_EQ(nullptr, json_str.p_str);
    TEST_CHECK_LOG_RECORD(ESP_LOG_ERROR, string("Can't add json item: remote_cfg_url"));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_TRUE(g_pTestClass->m_mem_alloc_trace.is_empty());
}

TEST_F(TestGwCfgRuuviJsonGenerate, gw_cfg_ruuvi_json_generate_malloc_failed_on_remote_cfg_auth_type) // NOLINT
{
    const gw_cfg_t   gw_cfg   = get_gateway_config_default();
    cjson_wrap_str_t json_str = cjson_wrap_str_null();

    cJSON_Hooks hooks = {
        .malloc_fn = &os_malloc,
        .free_fn   = &os_free_internal,
    };
    cJSON_InitHooks(&hooks);
    this->m_malloc_fail_on_cnt = 44;

    ASSERT_FALSE(gw_cfg_ruuvi_json_generate(&gw_cfg, &json_str));
    ASSERT_EQ(nullptr, json_str.p_str);
    TEST_CHECK_LOG_RECORD(ESP_LOG_ERROR, string("Can't add json item: remote_cfg_auth_type"));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_TRUE(g_pTestClass->m_mem_alloc_trace.is_empty());
}

TEST_F(TestGwCfgRuuviJsonGenerate, gw_cfg_ruuvi_json_generate_malloc_failed_on_remote_cfg_auth_type_2) // NOLINT
{
    const gw_cfg_t   gw_cfg   = get_gateway_config_default();
    cjson_wrap_str_t json_str = cjson_wrap_str_null();

    cJSON_Hooks hooks = {
        .malloc_fn = &os_malloc,
        .free_fn   = &os_free_internal,
    };
    cJSON_InitHooks(&hooks);
    this->m_malloc_fail_on_cnt = 45;

    ASSERT_FALSE(gw_cfg_ruuvi_json_generate(&gw_cfg, &json_str));
    ASSERT_EQ(nullptr, json_str.p_str);
    TEST_CHECK_LOG_RECORD(ESP_LOG_ERROR, string("Can't add json item: remote_cfg_auth_type"));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_TRUE(g_pTestClass->m_mem_alloc_trace.is_empty());
}

TEST_F(TestGwCfgRuuviJsonGenerate, gw_cfg_ruuvi_json_generate_malloc_failed_on_remote_cfg_auth_type_3) // NOLINT
{
    const gw_cfg_t   gw_cfg   = get_gateway_config_default();
    cjson_wrap_str_t json_str = cjson_wrap_str_null();

    cJSON_Hooks hooks = {
        .malloc_fn = &os_malloc,
        .free_fn   = &os_free_internal,
    };
    cJSON_InitHooks(&hooks);
    this->m_malloc_fail_on_cnt = 46;

    ASSERT_FALSE(gw_cfg_ruuvi_json_generate(&gw_cfg, &json_str));
    ASSERT_EQ(nullptr, json_str.p_str);
    TEST_CHECK_LOG_RECORD(ESP_LOG_ERROR, string("Can't add json item: remote_cfg_auth_type"));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_TRUE(g_pTestClass->m_mem_alloc_trace.is_empty());
}

TEST_F(
    TestGwCfgRuuviJsonGenerate,
    gw_cfg_ruuvi_json_generate_malloc_failed_on_remote_cfg_refresh_interval_minutes) // NOLINT
{
    const gw_cfg_t   gw_cfg   = get_gateway_config_default();
    cjson_wrap_str_t json_str = cjson_wrap_str_null();

    cJSON_Hooks hooks = {
        .malloc_fn = &os_malloc,
        .free_fn   = &os_free_internal,
    };
    cJSON_InitHooks(&hooks);
    this->m_malloc_fail_on_cnt = 47;

    ASSERT_FALSE(gw_cfg_ruuvi_json_generate(&gw_cfg, &json_str));
    ASSERT_EQ(nullptr, json_str.p_str);
    TEST_CHECK_LOG_RECORD(ESP_LOG_ERROR, string("Can't add json item: remote_cfg_refresh_interval_minutes"));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_TRUE(g_pTestClass->m_mem_alloc_trace.is_empty());
}

TEST_F(
    TestGwCfgRuuviJsonGenerate,
    gw_cfg_ruuvi_json_generate_malloc_failed_on_remote_cfg_refresh_interval_minutes_2) // NOLINT
{
    const gw_cfg_t   gw_cfg   = get_gateway_config_default();
    cjson_wrap_str_t json_str = cjson_wrap_str_null();

    cJSON_Hooks hooks = {
        .malloc_fn = &os_malloc,
        .free_fn   = &os_free_internal,
    };
    cJSON_InitHooks(&hooks);
    this->m_malloc_fail_on_cnt = 48;

    ASSERT_FALSE(gw_cfg_ruuvi_json_generate(&gw_cfg, &json_str));
    ASSERT_EQ(nullptr, json_str.p_str);
    TEST_CHECK_LOG_RECORD(ESP_LOG_ERROR, string("Can't add json item: remote_cfg_refresh_interval_minutes"));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_TRUE(g_pTestClass->m_mem_alloc_trace.is_empty());
}

TEST_F(TestGwCfgRuuviJsonGenerate, gw_cfg_ruuvi_json_generate_malloc_failed_on_use_http_ruuvi) // NOLINT
{
    const gw_cfg_t   gw_cfg   = get_gateway_config_default();
    cjson_wrap_str_t json_str = cjson_wrap_str_null();

    cJSON_Hooks hooks = {
        .malloc_fn = &os_malloc,
        .free_fn   = &os_free_internal,
    };
    cJSON_InitHooks(&hooks);
    this->m_malloc_fail_on_cnt = 49;

    ASSERT_FALSE(gw_cfg_ruuvi_json_generate(&gw_cfg, &json_str));
    ASSERT_EQ(nullptr, json_str.p_str);
    TEST_CHECK_LOG_RECORD(ESP_LOG_ERROR, string("Can't add json item: use_http_ruuvi"));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_TRUE(g_pTestClass->m_mem_alloc_trace.is_empty());
}

TEST_F(TestGwCfgRuuviJsonGenerate, gw_cfg_ruuvi_json_generate_malloc_failed_on_use_http_ruuvi_2) // NOLINT
{
    const gw_cfg_t   gw_cfg   = get_gateway_config_default();
    cjson_wrap_str_t json_str = cjson_wrap_str_null();

    cJSON_Hooks hooks = {
        .malloc_fn = &os_malloc,
        .free_fn   = &os_free_internal,
    };
    cJSON_InitHooks(&hooks);
    this->m_malloc_fail_on_cnt = 50;

    ASSERT_FALSE(gw_cfg_ruuvi_json_generate(&gw_cfg, &json_str));
    ASSERT_EQ(nullptr, json_str.p_str);
    TEST_CHECK_LOG_RECORD(ESP_LOG_ERROR, string("Can't add json item: use_http_ruuvi"));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_TRUE(g_pTestClass->m_mem_alloc_trace.is_empty());
}

TEST_F(TestGwCfgRuuviJsonGenerate, gw_cfg_ruuvi_json_generate_malloc_failed_on_use_http) // NOLINT
{
    const gw_cfg_t   gw_cfg   = get_gateway_config_default();
    cjson_wrap_str_t json_str = cjson_wrap_str_null();

    cJSON_Hooks hooks = {
        .malloc_fn = &os_malloc,
        .free_fn   = &os_free_internal,
    };
    cJSON_InitHooks(&hooks);
    this->m_malloc_fail_on_cnt = 51;

    ASSERT_FALSE(gw_cfg_ruuvi_json_generate(&gw_cfg, &json_str));
    ASSERT_EQ(nullptr, json_str.p_str);
    TEST_CHECK_LOG_RECORD(ESP_LOG_ERROR, string("Can't add json item: use_http"));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_TRUE(g_pTestClass->m_mem_alloc_trace.is_empty());
}

TEST_F(TestGwCfgRuuviJsonGenerate, gw_cfg_ruuvi_json_generate_malloc_failed_on_use_http_2) // NOLINT
{
    const gw_cfg_t   gw_cfg   = get_gateway_config_default();
    cjson_wrap_str_t json_str = cjson_wrap_str_null();

    cJSON_Hooks hooks = {
        .malloc_fn = &os_malloc,
        .free_fn   = &os_free_internal,
    };
    cJSON_InitHooks(&hooks);
    this->m_malloc_fail_on_cnt = 52;

    ASSERT_FALSE(gw_cfg_ruuvi_json_generate(&gw_cfg, &json_str));
    ASSERT_EQ(nullptr, json_str.p_str);
    TEST_CHECK_LOG_RECORD(ESP_LOG_ERROR, string("Can't add json item: use_http"));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_TRUE(g_pTestClass->m_mem_alloc_trace.is_empty());
}

TEST_F(TestGwCfgRuuviJsonGenerate, gw_cfg_ruuvi_json_generate_malloc_failed_on_http_url) // NOLINT
{
    const gw_cfg_t   gw_cfg   = get_gateway_config_default();
    cjson_wrap_str_t json_str = cjson_wrap_str_null();

    cJSON_Hooks hooks = {
        .malloc_fn = &os_malloc,
        .free_fn   = &os_free_internal,
    };
    cJSON_InitHooks(&hooks);
    this->m_malloc_fail_on_cnt = 53;

    ASSERT_FALSE(gw_cfg_ruuvi_json_generate(&gw_cfg, &json_str));
    ASSERT_EQ(nullptr, json_str.p_str);
    TEST_CHECK_LOG_RECORD(ESP_LOG_ERROR, string("Can't add json item: http_url"));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_TRUE(g_pTestClass->m_mem_alloc_trace.is_empty());
}

TEST_F(TestGwCfgRuuviJsonGenerate, gw_cfg_ruuvi_json_generate_malloc_failed_on_http_url_2) // NOLINT
{
    const gw_cfg_t   gw_cfg   = get_gateway_config_default();
    cjson_wrap_str_t json_str = cjson_wrap_str_null();

    cJSON_Hooks hooks = {
        .malloc_fn = &os_malloc,
        .free_fn   = &os_free_internal,
    };
    cJSON_InitHooks(&hooks);
    this->m_malloc_fail_on_cnt = 54;

    ASSERT_FALSE(gw_cfg_ruuvi_json_generate(&gw_cfg, &json_str));
    ASSERT_EQ(nullptr, json_str.p_str);
    TEST_CHECK_LOG_RECORD(ESP_LOG_ERROR, string("Can't add json item: http_url"));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_TRUE(g_pTestClass->m_mem_alloc_trace.is_empty());
}

TEST_F(TestGwCfgRuuviJsonGenerate, gw_cfg_ruuvi_json_generate_malloc_failed_on_http_url_3) // NOLINT
{
    const gw_cfg_t   gw_cfg   = get_gateway_config_default();
    cjson_wrap_str_t json_str = cjson_wrap_str_null();

    cJSON_Hooks hooks = {
        .malloc_fn = &os_malloc,
        .free_fn   = &os_free_internal,
    };
    cJSON_InitHooks(&hooks);
    this->m_malloc_fail_on_cnt = 55;

    ASSERT_FALSE(gw_cfg_ruuvi_json_generate(&gw_cfg, &json_str));
    ASSERT_EQ(nullptr, json_str.p_str);
    TEST_CHECK_LOG_RECORD(ESP_LOG_ERROR, string("Can't add json item: http_url"));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_TRUE(g_pTestClass->m_mem_alloc_trace.is_empty());
}

TEST_F(TestGwCfgRuuviJsonGenerate, gw_cfg_ruuvi_json_generate_malloc_failed_on_http_data_format) // NOLINT
{
    const gw_cfg_t   gw_cfg   = get_gateway_config_default();
    cjson_wrap_str_t json_str = cjson_wrap_str_null();

    cJSON_Hooks hooks = {
        .malloc_fn = &os_malloc,
        .free_fn   = &os_free_internal,
    };
    cJSON_InitHooks(&hooks);
    this->m_malloc_fail_on_cnt = 56;

    ASSERT_FALSE(gw_cfg_ruuvi_json_generate(&gw_cfg, &json_str));
    ASSERT_EQ(nullptr, json_str.p_str);
    TEST_CHECK_LOG_RECORD(ESP_LOG_ERROR, string("Can't add json item: http_data_format"));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_TRUE(g_pTestClass->m_mem_alloc_trace.is_empty());
}

TEST_F(TestGwCfgRuuviJsonGenerate, gw_cfg_ruuvi_json_generate_malloc_failed_on_http_data_format_2) // NOLINT
{
    const gw_cfg_t   gw_cfg   = get_gateway_config_default();
    cjson_wrap_str_t json_str = cjson_wrap_str_null();

    cJSON_Hooks hooks = {
        .malloc_fn = &os_malloc,
        .free_fn   = &os_free_internal,
    };
    cJSON_InitHooks(&hooks);
    this->m_malloc_fail_on_cnt = 57;

    ASSERT_FALSE(gw_cfg_ruuvi_json_generate(&gw_cfg, &json_str));
    ASSERT_EQ(nullptr, json_str.p_str);
    TEST_CHECK_LOG_RECORD(ESP_LOG_ERROR, string("Can't add json item: http_data_format"));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_TRUE(g_pTestClass->m_mem_alloc_trace.is_empty());
}

TEST_F(TestGwCfgRuuviJsonGenerate, gw_cfg_ruuvi_json_generate_malloc_failed_on_http_data_format_3) // NOLINT
{
    const gw_cfg_t   gw_cfg   = get_gateway_config_default();
    cjson_wrap_str_t json_str = cjson_wrap_str_null();

    cJSON_Hooks hooks = {
        .malloc_fn = &os_malloc,
        .free_fn   = &os_free_internal,
    };
    cJSON_InitHooks(&hooks);
    this->m_malloc_fail_on_cnt = 58;

    ASSERT_FALSE(gw_cfg_ruuvi_json_generate(&gw_cfg, &json_str));
    ASSERT_EQ(nullptr, json_str.p_str);
    TEST_CHECK_LOG_RECORD(ESP_LOG_ERROR, string("Can't add json item: http_data_format"));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_TRUE(g_pTestClass->m_mem_alloc_trace.is_empty());
}

TEST_F(TestGwCfgRuuviJsonGenerate, gw_cfg_ruuvi_json_generate_malloc_failed_on_http_auth) // NOLINT
{
    const gw_cfg_t   gw_cfg   = get_gateway_config_default();
    cjson_wrap_str_t json_str = cjson_wrap_str_null();

    cJSON_Hooks hooks = {
        .malloc_fn = &os_malloc,
        .free_fn   = &os_free_internal,
    };
    cJSON_InitHooks(&hooks);
    this->m_malloc_fail_on_cnt = 59;

    ASSERT_FALSE(gw_cfg_ruuvi_json_generate(&gw_cfg, &json_str));
    ASSERT_EQ(nullptr, json_str.p_str);
    TEST_CHECK_LOG_RECORD(ESP_LOG_ERROR, string("Can't add json item: http_auth"));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_TRUE(g_pTestClass->m_mem_alloc_trace.is_empty());
}

TEST_F(TestGwCfgRuuviJsonGenerate, gw_cfg_ruuvi_json_generate_malloc_failed_on_http_auth_2) // NOLINT
{
    const gw_cfg_t   gw_cfg   = get_gateway_config_default();
    cjson_wrap_str_t json_str = cjson_wrap_str_null();

    cJSON_Hooks hooks = {
        .malloc_fn = &os_malloc,
        .free_fn   = &os_free_internal,
    };
    cJSON_InitHooks(&hooks);
    this->m_malloc_fail_on_cnt = 60;

    ASSERT_FALSE(gw_cfg_ruuvi_json_generate(&gw_cfg, &json_str));
    ASSERT_EQ(nullptr, json_str.p_str);
    TEST_CHECK_LOG_RECORD(ESP_LOG_ERROR, string("Can't add json item: http_auth"));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_TRUE(g_pTestClass->m_mem_alloc_trace.is_empty());
}

TEST_F(TestGwCfgRuuviJsonGenerate, gw_cfg_ruuvi_json_generate_malloc_failed_on_http_auth_3) // NOLINT
{
    const gw_cfg_t   gw_cfg   = get_gateway_config_default();
    cjson_wrap_str_t json_str = cjson_wrap_str_null();

    cJSON_Hooks hooks = {
        .malloc_fn = &os_malloc,
        .free_fn   = &os_free_internal,
    };
    cJSON_InitHooks(&hooks);
    this->m_malloc_fail_on_cnt = 61;

    ASSERT_FALSE(gw_cfg_ruuvi_json_generate(&gw_cfg, &json_str));
    ASSERT_EQ(nullptr, json_str.p_str);
    TEST_CHECK_LOG_RECORD(ESP_LOG_ERROR, string("Can't add json item: http_auth"));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_TRUE(g_pTestClass->m_mem_alloc_trace.is_empty());
}

TEST_F(TestGwCfgRuuviJsonGenerate, gw_cfg_ruuvi_json_generate_malloc_failed_on_use_http_stat) // NOLINT
{
    const gw_cfg_t   gw_cfg   = get_gateway_config_default();
    cjson_wrap_str_t json_str = cjson_wrap_str_null();

    cJSON_Hooks hooks = {
        .malloc_fn = &os_malloc,
        .free_fn   = &os_free_internal,
    };
    cJSON_InitHooks(&hooks);
    this->m_malloc_fail_on_cnt = 62;

    ASSERT_FALSE(gw_cfg_ruuvi_json_generate(&gw_cfg, &json_str));
    ASSERT_EQ(nullptr, json_str.p_str);
    TEST_CHECK_LOG_RECORD(ESP_LOG_ERROR, string("Can't add json item: use_http_stat"));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_TRUE(g_pTestClass->m_mem_alloc_trace.is_empty());
}

TEST_F(TestGwCfgRuuviJsonGenerate, gw_cfg_ruuvi_json_generate_malloc_failed_on_use_http_stat_2) // NOLINT
{
    const gw_cfg_t   gw_cfg   = get_gateway_config_default();
    cjson_wrap_str_t json_str = cjson_wrap_str_null();

    cJSON_Hooks hooks = {
        .malloc_fn = &os_malloc,
        .free_fn   = &os_free_internal,
    };
    cJSON_InitHooks(&hooks);
    this->m_malloc_fail_on_cnt = 63;

    ASSERT_FALSE(gw_cfg_ruuvi_json_generate(&gw_cfg, &json_str));
    ASSERT_EQ(nullptr, json_str.p_str);
    TEST_CHECK_LOG_RECORD(ESP_LOG_ERROR, string("Can't add json item: use_http_stat"));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_TRUE(g_pTestClass->m_mem_alloc_trace.is_empty());
}

TEST_F(TestGwCfgRuuviJsonGenerate, gw_cfg_ruuvi_json_generate_malloc_failed_on_http_stat_url) // NOLINT
{
    const gw_cfg_t   gw_cfg   = get_gateway_config_default();
    cjson_wrap_str_t json_str = cjson_wrap_str_null();

    cJSON_Hooks hooks = {
        .malloc_fn = &os_malloc,
        .free_fn   = &os_free_internal,
    };
    cJSON_InitHooks(&hooks);
    this->m_malloc_fail_on_cnt = 64;

    ASSERT_FALSE(gw_cfg_ruuvi_json_generate(&gw_cfg, &json_str));
    ASSERT_EQ(nullptr, json_str.p_str);
    TEST_CHECK_LOG_RECORD(ESP_LOG_ERROR, string("Can't add json item: http_stat_url"));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_TRUE(g_pTestClass->m_mem_alloc_trace.is_empty());
}

TEST_F(TestGwCfgRuuviJsonGenerate, gw_cfg_ruuvi_json_generate_malloc_failed_on_http_stat_url_2) // NOLINT
{
    const gw_cfg_t   gw_cfg   = get_gateway_config_default();
    cjson_wrap_str_t json_str = cjson_wrap_str_null();

    cJSON_Hooks hooks = {
        .malloc_fn = &os_malloc,
        .free_fn   = &os_free_internal,
    };
    cJSON_InitHooks(&hooks);
    this->m_malloc_fail_on_cnt = 65;

    ASSERT_FALSE(gw_cfg_ruuvi_json_generate(&gw_cfg, &json_str));
    ASSERT_EQ(nullptr, json_str.p_str);
    TEST_CHECK_LOG_RECORD(ESP_LOG_ERROR, string("Can't add json item: http_stat_url"));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_TRUE(g_pTestClass->m_mem_alloc_trace.is_empty());
}

TEST_F(TestGwCfgRuuviJsonGenerate, gw_cfg_ruuvi_json_generate_malloc_failed_on_http_stat_url_3) // NOLINT
{
    const gw_cfg_t   gw_cfg   = get_gateway_config_default();
    cjson_wrap_str_t json_str = cjson_wrap_str_null();

    cJSON_Hooks hooks = {
        .malloc_fn = &os_malloc,
        .free_fn   = &os_free_internal,
    };
    cJSON_InitHooks(&hooks);
    this->m_malloc_fail_on_cnt = 66;

    ASSERT_FALSE(gw_cfg_ruuvi_json_generate(&gw_cfg, &json_str));
    ASSERT_EQ(nullptr, json_str.p_str);
    TEST_CHECK_LOG_RECORD(ESP_LOG_ERROR, string("Can't add json item: http_stat_url"));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_TRUE(g_pTestClass->m_mem_alloc_trace.is_empty());
}

TEST_F(TestGwCfgRuuviJsonGenerate, gw_cfg_ruuvi_json_generate_malloc_failed_on_http_stat_user) // NOLINT
{
    const gw_cfg_t   gw_cfg   = get_gateway_config_default();
    cjson_wrap_str_t json_str = cjson_wrap_str_null();

    cJSON_Hooks hooks = {
        .malloc_fn = &os_malloc,
        .free_fn   = &os_free_internal,
    };
    cJSON_InitHooks(&hooks);
    this->m_malloc_fail_on_cnt = 67;

    ASSERT_FALSE(gw_cfg_ruuvi_json_generate(&gw_cfg, &json_str));
    ASSERT_EQ(nullptr, json_str.p_str);
    TEST_CHECK_LOG_RECORD(ESP_LOG_ERROR, string("Can't add json item: http_stat_user"));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_TRUE(g_pTestClass->m_mem_alloc_trace.is_empty());
}

TEST_F(TestGwCfgRuuviJsonGenerate, gw_cfg_ruuvi_json_generate_malloc_failed_on_http_stat_user_2) // NOLINT
{
    const gw_cfg_t   gw_cfg   = get_gateway_config_default();
    cjson_wrap_str_t json_str = cjson_wrap_str_null();

    cJSON_Hooks hooks = {
        .malloc_fn = &os_malloc,
        .free_fn   = &os_free_internal,
    };
    cJSON_InitHooks(&hooks);
    this->m_malloc_fail_on_cnt = 68;

    ASSERT_FALSE(gw_cfg_ruuvi_json_generate(&gw_cfg, &json_str));
    ASSERT_EQ(nullptr, json_str.p_str);
    TEST_CHECK_LOG_RECORD(ESP_LOG_ERROR, string("Can't add json item: http_stat_user"));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_TRUE(g_pTestClass->m_mem_alloc_trace.is_empty());
}

TEST_F(TestGwCfgRuuviJsonGenerate, gw_cfg_ruuvi_json_generate_malloc_failed_on_http_stat_user_3) // NOLINT
{
    const gw_cfg_t   gw_cfg   = get_gateway_config_default();
    cjson_wrap_str_t json_str = cjson_wrap_str_null();

    cJSON_Hooks hooks = {
        .malloc_fn = &os_malloc,
        .free_fn   = &os_free_internal,
    };
    cJSON_InitHooks(&hooks);
    this->m_malloc_fail_on_cnt = 69;

    ASSERT_FALSE(gw_cfg_ruuvi_json_generate(&gw_cfg, &json_str));
    ASSERT_EQ(nullptr, json_str.p_str);
    TEST_CHECK_LOG_RECORD(ESP_LOG_ERROR, string("Can't add json item: http_stat_user"));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_TRUE(g_pTestClass->m_mem_alloc_trace.is_empty());
}

TEST_F(TestGwCfgRuuviJsonGenerate, gw_cfg_ruuvi_json_generate_malloc_failed_on_use_mqtt) // NOLINT
{
    const gw_cfg_t   gw_cfg   = get_gateway_config_default();
    cjson_wrap_str_t json_str = cjson_wrap_str_null();

    cJSON_Hooks hooks = {
        .malloc_fn = &os_malloc,
        .free_fn   = &os_free_internal,
    };
    cJSON_InitHooks(&hooks);
    this->m_malloc_fail_on_cnt = 70;

    ASSERT_FALSE(gw_cfg_ruuvi_json_generate(&gw_cfg, &json_str));
    ASSERT_EQ(nullptr, json_str.p_str);
    TEST_CHECK_LOG_RECORD(ESP_LOG_ERROR, string("Can't add json item: use_mqtt"));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_TRUE(g_pTestClass->m_mem_alloc_trace.is_empty());
}

TEST_F(TestGwCfgRuuviJsonGenerate, gw_cfg_ruuvi_json_generate_malloc_failed_on_use_mqtt_2) // NOLINT
{
    const gw_cfg_t   gw_cfg   = get_gateway_config_default();
    cjson_wrap_str_t json_str = cjson_wrap_str_null();

    cJSON_Hooks hooks = {
        .malloc_fn = &os_malloc,
        .free_fn   = &os_free_internal,
    };
    cJSON_InitHooks(&hooks);
    this->m_malloc_fail_on_cnt = 71;

    ASSERT_FALSE(gw_cfg_ruuvi_json_generate(&gw_cfg, &json_str));
    ASSERT_EQ(nullptr, json_str.p_str);
    TEST_CHECK_LOG_RECORD(ESP_LOG_ERROR, string("Can't add json item: use_mqtt"));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_TRUE(g_pTestClass->m_mem_alloc_trace.is_empty());
}

TEST_F(TestGwCfgRuuviJsonGenerate, gw_cfg_ruuvi_json_generate_malloc_failed_on_mqtt_disable_retained_messages) // NOLINT
{
    const gw_cfg_t   gw_cfg   = get_gateway_config_default();
    cjson_wrap_str_t json_str = cjson_wrap_str_null();

    cJSON_Hooks hooks = {
        .malloc_fn = &os_malloc,
        .free_fn   = &os_free_internal,
    };
    cJSON_InitHooks(&hooks);
    this->m_malloc_fail_on_cnt = 72;

    ASSERT_FALSE(gw_cfg_ruuvi_json_generate(&gw_cfg, &json_str));
    ASSERT_EQ(nullptr, json_str.p_str);
    TEST_CHECK_LOG_RECORD(ESP_LOG_ERROR, string("Can't add json item: mqtt_disable_retained_messages"));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_TRUE(g_pTestClass->m_mem_alloc_trace.is_empty());
}

TEST_F(
    TestGwCfgRuuviJsonGenerate,
    gw_cfg_ruuvi_json_generate_malloc_failed_on_mqtt_disable_retained_messages_2) // NOLINT
{
    const gw_cfg_t   gw_cfg   = get_gateway_config_default();
    cjson_wrap_str_t json_str = cjson_wrap_str_null();

    cJSON_Hooks hooks = {
        .malloc_fn = &os_malloc,
        .free_fn   = &os_free_internal,
    };
    cJSON_InitHooks(&hooks);
    this->m_malloc_fail_on_cnt = 73;

    ASSERT_FALSE(gw_cfg_ruuvi_json_generate(&gw_cfg, &json_str));
    ASSERT_EQ(nullptr, json_str.p_str);
    TEST_CHECK_LOG_RECORD(ESP_LOG_ERROR, string("Can't add json item: mqtt_disable_retained_messages"));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_TRUE(g_pTestClass->m_mem_alloc_trace.is_empty());
}

TEST_F(TestGwCfgRuuviJsonGenerate, gw_cfg_ruuvi_json_generate_malloc_failed_on_mqtt_transport) // NOLINT
{
    const gw_cfg_t   gw_cfg   = get_gateway_config_default();
    cjson_wrap_str_t json_str = cjson_wrap_str_null();

    cJSON_Hooks hooks = {
        .malloc_fn = &os_malloc,
        .free_fn   = &os_free_internal,
    };
    cJSON_InitHooks(&hooks);
    this->m_malloc_fail_on_cnt = 74;

    ASSERT_FALSE(gw_cfg_ruuvi_json_generate(&gw_cfg, &json_str));
    ASSERT_EQ(nullptr, json_str.p_str);
    TEST_CHECK_LOG_RECORD(ESP_LOG_ERROR, string("Can't add json item: mqtt_transport"));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_TRUE(g_pTestClass->m_mem_alloc_trace.is_empty());
}

TEST_F(TestGwCfgRuuviJsonGenerate, gw_cfg_ruuvi_json_generate_malloc_failed_on_mqtt_transport_2) // NOLINT
{
    const gw_cfg_t   gw_cfg   = get_gateway_config_default();
    cjson_wrap_str_t json_str = cjson_wrap_str_null();

    cJSON_Hooks hooks = {
        .malloc_fn = &os_malloc,
        .free_fn   = &os_free_internal,
    };
    cJSON_InitHooks(&hooks);
    this->m_malloc_fail_on_cnt = 75;

    ASSERT_FALSE(gw_cfg_ruuvi_json_generate(&gw_cfg, &json_str));
    ASSERT_EQ(nullptr, json_str.p_str);
    TEST_CHECK_LOG_RECORD(ESP_LOG_ERROR, string("Can't add json item: mqtt_transport"));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_TRUE(g_pTestClass->m_mem_alloc_trace.is_empty());
}

TEST_F(TestGwCfgRuuviJsonGenerate, gw_cfg_ruuvi_json_generate_malloc_failed_on_mqtt_transport_3) // NOLINT
{
    const gw_cfg_t   gw_cfg   = get_gateway_config_default();
    cjson_wrap_str_t json_str = cjson_wrap_str_null();

    cJSON_Hooks hooks = {
        .malloc_fn = &os_malloc,
        .free_fn   = &os_free_internal,
    };
    cJSON_InitHooks(&hooks);
    this->m_malloc_fail_on_cnt = 76;

    ASSERT_FALSE(gw_cfg_ruuvi_json_generate(&gw_cfg, &json_str));
    ASSERT_EQ(nullptr, json_str.p_str);
    TEST_CHECK_LOG_RECORD(ESP_LOG_ERROR, string("Can't add json item: mqtt_transport"));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_TRUE(g_pTestClass->m_mem_alloc_trace.is_empty());
}

TEST_F(TestGwCfgRuuviJsonGenerate, gw_cfg_ruuvi_json_generate_malloc_failed_on_mqtt_server) // NOLINT
{
    const gw_cfg_t   gw_cfg   = get_gateway_config_default();
    cjson_wrap_str_t json_str = cjson_wrap_str_null();

    cJSON_Hooks hooks = {
        .malloc_fn = &os_malloc,
        .free_fn   = &os_free_internal,
    };
    cJSON_InitHooks(&hooks);
    this->m_malloc_fail_on_cnt = 77;

    ASSERT_FALSE(gw_cfg_ruuvi_json_generate(&gw_cfg, &json_str));
    ASSERT_EQ(nullptr, json_str.p_str);
    TEST_CHECK_LOG_RECORD(ESP_LOG_ERROR, string("Can't add json item: mqtt_server"));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_TRUE(g_pTestClass->m_mem_alloc_trace.is_empty());
}

TEST_F(TestGwCfgRuuviJsonGenerate, gw_cfg_ruuvi_json_generate_malloc_failed_on_mqtt_server_2) // NOLINT
{
    const gw_cfg_t   gw_cfg   = get_gateway_config_default();
    cjson_wrap_str_t json_str = cjson_wrap_str_null();

    cJSON_Hooks hooks = {
        .malloc_fn = &os_malloc,
        .free_fn   = &os_free_internal,
    };
    cJSON_InitHooks(&hooks);
    this->m_malloc_fail_on_cnt = 78;

    ASSERT_FALSE(gw_cfg_ruuvi_json_generate(&gw_cfg, &json_str));
    ASSERT_EQ(nullptr, json_str.p_str);
    TEST_CHECK_LOG_RECORD(ESP_LOG_ERROR, string("Can't add json item: mqtt_server"));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_TRUE(g_pTestClass->m_mem_alloc_trace.is_empty());
}

TEST_F(TestGwCfgRuuviJsonGenerate, gw_cfg_ruuvi_json_generate_malloc_failed_on_mqtt_server_3) // NOLINT
{
    const gw_cfg_t   gw_cfg   = get_gateway_config_default();
    cjson_wrap_str_t json_str = cjson_wrap_str_null();

    cJSON_Hooks hooks = {
        .malloc_fn = &os_malloc,
        .free_fn   = &os_free_internal,
    };
    cJSON_InitHooks(&hooks);
    this->m_malloc_fail_on_cnt = 79;

    ASSERT_FALSE(gw_cfg_ruuvi_json_generate(&gw_cfg, &json_str));
    ASSERT_EQ(nullptr, json_str.p_str);
    TEST_CHECK_LOG_RECORD(ESP_LOG_ERROR, string("Can't add json item: mqtt_server"));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_TRUE(g_pTestClass->m_mem_alloc_trace.is_empty());
}

TEST_F(TestGwCfgRuuviJsonGenerate, gw_cfg_ruuvi_json_generate_malloc_failed_on_mqtt_port) // NOLINT
{
    const gw_cfg_t   gw_cfg   = get_gateway_config_default();
    cjson_wrap_str_t json_str = cjson_wrap_str_null();

    cJSON_Hooks hooks = {
        .malloc_fn = &os_malloc,
        .free_fn   = &os_free_internal,
    };
    cJSON_InitHooks(&hooks);
    this->m_malloc_fail_on_cnt = 80;

    ASSERT_FALSE(gw_cfg_ruuvi_json_generate(&gw_cfg, &json_str));
    ASSERT_EQ(nullptr, json_str.p_str);
    TEST_CHECK_LOG_RECORD(ESP_LOG_ERROR, string("Can't add json item: mqtt_port"));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_TRUE(g_pTestClass->m_mem_alloc_trace.is_empty());
}

TEST_F(TestGwCfgRuuviJsonGenerate, gw_cfg_ruuvi_json_generate_malloc_failed_on_mqtt_port_2) // NOLINT
{
    const gw_cfg_t   gw_cfg   = get_gateway_config_default();
    cjson_wrap_str_t json_str = cjson_wrap_str_null();

    cJSON_Hooks hooks = {
        .malloc_fn = &os_malloc,
        .free_fn   = &os_free_internal,
    };
    cJSON_InitHooks(&hooks);
    this->m_malloc_fail_on_cnt = 81;

    ASSERT_FALSE(gw_cfg_ruuvi_json_generate(&gw_cfg, &json_str));
    ASSERT_EQ(nullptr, json_str.p_str);
    TEST_CHECK_LOG_RECORD(ESP_LOG_ERROR, string("Can't add json item: mqtt_port"));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_TRUE(g_pTestClass->m_mem_alloc_trace.is_empty());
}

TEST_F(TestGwCfgRuuviJsonGenerate, gw_cfg_ruuvi_json_generate_malloc_failed_on_mqtt_prefix) // NOLINT
{
    const gw_cfg_t   gw_cfg   = get_gateway_config_default();
    cjson_wrap_str_t json_str = cjson_wrap_str_null();

    cJSON_Hooks hooks = {
        .malloc_fn = &os_malloc,
        .free_fn   = &os_free_internal,
    };
    cJSON_InitHooks(&hooks);
    this->m_malloc_fail_on_cnt = 82;

    ASSERT_FALSE(gw_cfg_ruuvi_json_generate(&gw_cfg, &json_str));
    ASSERT_EQ(nullptr, json_str.p_str);
    TEST_CHECK_LOG_RECORD(ESP_LOG_ERROR, string("Can't add json item: mqtt_prefix"));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_TRUE(g_pTestClass->m_mem_alloc_trace.is_empty());
}

TEST_F(TestGwCfgRuuviJsonGenerate, gw_cfg_ruuvi_json_generate_malloc_failed_on_mqtt_prefix_2) // NOLINT
{
    const gw_cfg_t   gw_cfg   = get_gateway_config_default();
    cjson_wrap_str_t json_str = cjson_wrap_str_null();

    cJSON_Hooks hooks = {
        .malloc_fn = &os_malloc,
        .free_fn   = &os_free_internal,
    };
    cJSON_InitHooks(&hooks);
    this->m_malloc_fail_on_cnt = 83;

    ASSERT_FALSE(gw_cfg_ruuvi_json_generate(&gw_cfg, &json_str));
    ASSERT_EQ(nullptr, json_str.p_str);
    TEST_CHECK_LOG_RECORD(ESP_LOG_ERROR, string("Can't add json item: mqtt_prefix"));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_TRUE(g_pTestClass->m_mem_alloc_trace.is_empty());
}

TEST_F(TestGwCfgRuuviJsonGenerate, gw_cfg_ruuvi_json_generate_malloc_failed_on_mqtt_prefix_3) // NOLINT
{
    const gw_cfg_t   gw_cfg   = get_gateway_config_default();
    cjson_wrap_str_t json_str = cjson_wrap_str_null();

    cJSON_Hooks hooks = {
        .malloc_fn = &os_malloc,
        .free_fn   = &os_free_internal,
    };
    cJSON_InitHooks(&hooks);
    this->m_malloc_fail_on_cnt = 84;

    ASSERT_FALSE(gw_cfg_ruuvi_json_generate(&gw_cfg, &json_str));
    ASSERT_EQ(nullptr, json_str.p_str);
    TEST_CHECK_LOG_RECORD(ESP_LOG_ERROR, string("Can't add json item: mqtt_prefix"));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_TRUE(g_pTestClass->m_mem_alloc_trace.is_empty());
}

TEST_F(TestGwCfgRuuviJsonGenerate, gw_cfg_ruuvi_json_generate_malloc_failed_on_mqtt_client_id) // NOLINT
{
    const gw_cfg_t   gw_cfg   = get_gateway_config_default();
    cjson_wrap_str_t json_str = cjson_wrap_str_null();

    cJSON_Hooks hooks = {
        .malloc_fn = &os_malloc,
        .free_fn   = &os_free_internal,
    };
    cJSON_InitHooks(&hooks);
    this->m_malloc_fail_on_cnt = 85;

    ASSERT_FALSE(gw_cfg_ruuvi_json_generate(&gw_cfg, &json_str));
    ASSERT_EQ(nullptr, json_str.p_str);
    TEST_CHECK_LOG_RECORD(ESP_LOG_ERROR, string("Can't add json item: mqtt_client_id"));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_TRUE(g_pTestClass->m_mem_alloc_trace.is_empty());
}

TEST_F(TestGwCfgRuuviJsonGenerate, gw_cfg_ruuvi_json_generate_malloc_failed_on_mqtt_client_id_2) // NOLINT
{
    const gw_cfg_t   gw_cfg   = get_gateway_config_default();
    cjson_wrap_str_t json_str = cjson_wrap_str_null();

    cJSON_Hooks hooks = {
        .malloc_fn = &os_malloc,
        .free_fn   = &os_free_internal,
    };
    cJSON_InitHooks(&hooks);
    this->m_malloc_fail_on_cnt = 86;

    ASSERT_FALSE(gw_cfg_ruuvi_json_generate(&gw_cfg, &json_str));
    ASSERT_EQ(nullptr, json_str.p_str);
    TEST_CHECK_LOG_RECORD(ESP_LOG_ERROR, string("Can't add json item: mqtt_client_id"));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_TRUE(g_pTestClass->m_mem_alloc_trace.is_empty());
}

TEST_F(TestGwCfgRuuviJsonGenerate, gw_cfg_ruuvi_json_generate_malloc_failed_on_mqtt_client_id_3) // NOLINT
{
    const gw_cfg_t   gw_cfg   = get_gateway_config_default();
    cjson_wrap_str_t json_str = cjson_wrap_str_null();

    cJSON_Hooks hooks = {
        .malloc_fn = &os_malloc,
        .free_fn   = &os_free_internal,
    };
    cJSON_InitHooks(&hooks);
    this->m_malloc_fail_on_cnt = 87;

    ASSERT_FALSE(gw_cfg_ruuvi_json_generate(&gw_cfg, &json_str));
    ASSERT_EQ(nullptr, json_str.p_str);
    TEST_CHECK_LOG_RECORD(ESP_LOG_ERROR, string("Can't add json item: mqtt_client_id"));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_TRUE(g_pTestClass->m_mem_alloc_trace.is_empty());
}

TEST_F(TestGwCfgRuuviJsonGenerate, gw_cfg_ruuvi_json_generate_malloc_failed_on_mqtt_user) // NOLINT
{
    const gw_cfg_t   gw_cfg   = get_gateway_config_default();
    cjson_wrap_str_t json_str = cjson_wrap_str_null();

    cJSON_Hooks hooks = {
        .malloc_fn = &os_malloc,
        .free_fn   = &os_free_internal,
    };
    cJSON_InitHooks(&hooks);
    this->m_malloc_fail_on_cnt = 88;

    ASSERT_FALSE(gw_cfg_ruuvi_json_generate(&gw_cfg, &json_str));
    ASSERT_EQ(nullptr, json_str.p_str);
    TEST_CHECK_LOG_RECORD(ESP_LOG_ERROR, string("Can't add json item: mqtt_user"));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_TRUE(g_pTestClass->m_mem_alloc_trace.is_empty());
}

TEST_F(TestGwCfgRuuviJsonGenerate, gw_cfg_ruuvi_json_generate_malloc_failed_on_mqtt_user_2) // NOLINT
{
    const gw_cfg_t   gw_cfg   = get_gateway_config_default();
    cjson_wrap_str_t json_str = cjson_wrap_str_null();

    cJSON_Hooks hooks = {
        .malloc_fn = &os_malloc,
        .free_fn   = &os_free_internal,
    };
    cJSON_InitHooks(&hooks);
    this->m_malloc_fail_on_cnt = 89;

    ASSERT_FALSE(gw_cfg_ruuvi_json_generate(&gw_cfg, &json_str));
    ASSERT_EQ(nullptr, json_str.p_str);
    TEST_CHECK_LOG_RECORD(ESP_LOG_ERROR, string("Can't add json item: mqtt_user"));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_TRUE(g_pTestClass->m_mem_alloc_trace.is_empty());
}

TEST_F(TestGwCfgRuuviJsonGenerate, gw_cfg_ruuvi_json_generate_malloc_failed_on_mqtt_user_3) // NOLINT
{
    const gw_cfg_t   gw_cfg   = get_gateway_config_default();
    cjson_wrap_str_t json_str = cjson_wrap_str_null();

    cJSON_Hooks hooks = {
        .malloc_fn = &os_malloc,
        .free_fn   = &os_free_internal,
    };
    cJSON_InitHooks(&hooks);
    this->m_malloc_fail_on_cnt = 90;

    ASSERT_FALSE(gw_cfg_ruuvi_json_generate(&gw_cfg, &json_str));
    ASSERT_EQ(nullptr, json_str.p_str);
    TEST_CHECK_LOG_RECORD(ESP_LOG_ERROR, string("Can't add json item: mqtt_user"));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_TRUE(g_pTestClass->m_mem_alloc_trace.is_empty());
}

TEST_F(TestGwCfgRuuviJsonGenerate, gw_cfg_ruuvi_json_generate_malloc_failed_on_lan_auth_type) // NOLINT
{
    const gw_cfg_t   gw_cfg   = get_gateway_config_default();
    cjson_wrap_str_t json_str = cjson_wrap_str_null();

    cJSON_Hooks hooks = {
        .malloc_fn = &os_malloc,
        .free_fn   = &os_free_internal,
    };
    cJSON_InitHooks(&hooks);
    this->m_malloc_fail_on_cnt = 91;

    ASSERT_FALSE(gw_cfg_ruuvi_json_generate(&gw_cfg, &json_str));
    ASSERT_EQ(nullptr, json_str.p_str);
    TEST_CHECK_LOG_RECORD(ESP_LOG_ERROR, string("Can't add json item: lan_auth_type"));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_TRUE(g_pTestClass->m_mem_alloc_trace.is_empty());
}

TEST_F(TestGwCfgRuuviJsonGenerate, gw_cfg_ruuvi_json_generate_malloc_failed_on_lan_auth_type_2) // NOLINT
{
    const gw_cfg_t   gw_cfg   = get_gateway_config_default();
    cjson_wrap_str_t json_str = cjson_wrap_str_null();

    cJSON_Hooks hooks = {
        .malloc_fn = &os_malloc,
        .free_fn   = &os_free_internal,
    };
    cJSON_InitHooks(&hooks);
    this->m_malloc_fail_on_cnt = 92;

    ASSERT_FALSE(gw_cfg_ruuvi_json_generate(&gw_cfg, &json_str));
    ASSERT_EQ(nullptr, json_str.p_str);
    TEST_CHECK_LOG_RECORD(ESP_LOG_ERROR, string("Can't add json item: lan_auth_type"));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_TRUE(g_pTestClass->m_mem_alloc_trace.is_empty());
}

TEST_F(TestGwCfgRuuviJsonGenerate, gw_cfg_ruuvi_json_generate_malloc_failed_on_lan_auth_type_3) // NOLINT
{
    const gw_cfg_t   gw_cfg   = get_gateway_config_default();
    cjson_wrap_str_t json_str = cjson_wrap_str_null();

    cJSON_Hooks hooks = {
        .malloc_fn = &os_malloc,
        .free_fn   = &os_free_internal,
    };
    cJSON_InitHooks(&hooks);
    this->m_malloc_fail_on_cnt = 93;

    ASSERT_FALSE(gw_cfg_ruuvi_json_generate(&gw_cfg, &json_str));
    ASSERT_EQ(nullptr, json_str.p_str);
    TEST_CHECK_LOG_RECORD(ESP_LOG_ERROR, string("Can't add json item: lan_auth_type"));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_TRUE(g_pTestClass->m_mem_alloc_trace.is_empty());
}

TEST_F(TestGwCfgRuuviJsonGenerate, gw_cfg_ruuvi_json_generate_malloc_failed_on_lan_auth_user) // NOLINT
{
    const gw_cfg_t   gw_cfg   = get_gateway_config_default();
    cjson_wrap_str_t json_str = cjson_wrap_str_null();

    cJSON_Hooks hooks = {
        .malloc_fn = &os_malloc,
        .free_fn   = &os_free_internal,
    };
    cJSON_InitHooks(&hooks);
    this->m_malloc_fail_on_cnt = 94;

    ASSERT_FALSE(gw_cfg_ruuvi_json_generate(&gw_cfg, &json_str));
    ASSERT_EQ(nullptr, json_str.p_str);
    TEST_CHECK_LOG_RECORD(ESP_LOG_ERROR, string("Can't add json item: lan_auth_user"));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_TRUE(g_pTestClass->m_mem_alloc_trace.is_empty());
}

TEST_F(TestGwCfgRuuviJsonGenerate, gw_cfg_ruuvi_json_generate_malloc_failed_on_lan_auth_user_2) // NOLINT
{
    const gw_cfg_t   gw_cfg   = get_gateway_config_default();
    cjson_wrap_str_t json_str = cjson_wrap_str_null();

    cJSON_Hooks hooks = {
        .malloc_fn = &os_malloc,
        .free_fn   = &os_free_internal,
    };
    cJSON_InitHooks(&hooks);
    this->m_malloc_fail_on_cnt = 95;

    ASSERT_FALSE(gw_cfg_ruuvi_json_generate(&gw_cfg, &json_str));
    ASSERT_EQ(nullptr, json_str.p_str);
    TEST_CHECK_LOG_RECORD(ESP_LOG_ERROR, string("Can't add json item: lan_auth_user"));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_TRUE(g_pTestClass->m_mem_alloc_trace.is_empty());
}

TEST_F(TestGwCfgRuuviJsonGenerate, gw_cfg_ruuvi_json_generate_malloc_failed_on_lan_auth_user_3) // NOLINT
{
    const gw_cfg_t   gw_cfg   = get_gateway_config_default();
    cjson_wrap_str_t json_str = cjson_wrap_str_null();

    cJSON_Hooks hooks = {
        .malloc_fn = &os_malloc,
        .free_fn   = &os_free_internal,
    };
    cJSON_InitHooks(&hooks);
    this->m_malloc_fail_on_cnt = 96;

    ASSERT_FALSE(gw_cfg_ruuvi_json_generate(&gw_cfg, &json_str));
    ASSERT_EQ(nullptr, json_str.p_str);
    TEST_CHECK_LOG_RECORD(ESP_LOG_ERROR, string("Can't add json item: lan_auth_user"));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_TRUE(g_pTestClass->m_mem_alloc_trace.is_empty());
}

TEST_F(TestGwCfgRuuviJsonGenerate, gw_cfg_ruuvi_json_generate_malloc_failed_on_lan_auth_api_key_use) // NOLINT
{
    const gw_cfg_t   gw_cfg   = get_gateway_config_default();
    cjson_wrap_str_t json_str = cjson_wrap_str_null();

    cJSON_Hooks hooks = {
        .malloc_fn = &os_malloc,
        .free_fn   = &os_free_internal,
    };
    cJSON_InitHooks(&hooks);
    this->m_malloc_fail_on_cnt = 97;

    ASSERT_FALSE(gw_cfg_ruuvi_json_generate(&gw_cfg, &json_str));
    ASSERT_EQ(nullptr, json_str.p_str);
    TEST_CHECK_LOG_RECORD(ESP_LOG_ERROR, string("Can't add json item: lan_auth_api_key_use"));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_TRUE(g_pTestClass->m_mem_alloc_trace.is_empty());
}

TEST_F(TestGwCfgRuuviJsonGenerate, gw_cfg_ruuvi_json_generate_malloc_failed_on_lan_auth_api_key_use_2) // NOLINT
{
    const gw_cfg_t   gw_cfg   = get_gateway_config_default();
    cjson_wrap_str_t json_str = cjson_wrap_str_null();

    cJSON_Hooks hooks = {
        .malloc_fn = &os_malloc,
        .free_fn   = &os_free_internal,
    };
    cJSON_InitHooks(&hooks);
    this->m_malloc_fail_on_cnt = 98;

    ASSERT_FALSE(gw_cfg_ruuvi_json_generate(&gw_cfg, &json_str));
    ASSERT_EQ(nullptr, json_str.p_str);
    TEST_CHECK_LOG_RECORD(ESP_LOG_ERROR, string("Can't add json item: lan_auth_api_key_use"));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_TRUE(g_pTestClass->m_mem_alloc_trace.is_empty());
}

TEST_F(TestGwCfgRuuviJsonGenerate, gw_cfg_ruuvi_json_generate_malloc_failed_on_lan_auth_api_key_rw_use) // NOLINT
{
    const gw_cfg_t   gw_cfg   = get_gateway_config_default();
    cjson_wrap_str_t json_str = cjson_wrap_str_null();

    cJSON_Hooks hooks = {
        .malloc_fn = &os_malloc,
        .free_fn   = &os_free_internal,
    };
    cJSON_InitHooks(&hooks);
    this->m_malloc_fail_on_cnt = 99;

    ASSERT_FALSE(gw_cfg_ruuvi_json_generate(&gw_cfg, &json_str));
    ASSERT_EQ(nullptr, json_str.p_str);
    TEST_CHECK_LOG_RECORD(ESP_LOG_ERROR, string("Can't add json item: lan_auth_api_key_rw_use"));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_TRUE(g_pTestClass->m_mem_alloc_trace.is_empty());
}

TEST_F(TestGwCfgRuuviJsonGenerate, gw_cfg_ruuvi_json_generate_malloc_failed_on_lan_auth_api_key_rw_use_2) // NOLINT
{
    const gw_cfg_t   gw_cfg   = get_gateway_config_default();
    cjson_wrap_str_t json_str = cjson_wrap_str_null();

    cJSON_Hooks hooks = {
        .malloc_fn = &os_malloc,
        .free_fn   = &os_free_internal,
    };
    cJSON_InitHooks(&hooks);
    this->m_malloc_fail_on_cnt = 100;

    ASSERT_FALSE(gw_cfg_ruuvi_json_generate(&gw_cfg, &json_str));
    ASSERT_EQ(nullptr, json_str.p_str);
    TEST_CHECK_LOG_RECORD(ESP_LOG_ERROR, string("Can't add json item: lan_auth_api_key_rw_use"));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_TRUE(g_pTestClass->m_mem_alloc_trace.is_empty());
}

TEST_F(TestGwCfgRuuviJsonGenerate, gw_cfg_ruuvi_json_generate_malloc_failed_on_auto_update_cycle) // NOLINT
{
    const gw_cfg_t   gw_cfg   = get_gateway_config_default();
    cjson_wrap_str_t json_str = cjson_wrap_str_null();

    cJSON_Hooks hooks = {
        .malloc_fn = &os_malloc,
        .free_fn   = &os_free_internal,
    };
    cJSON_InitHooks(&hooks);
    this->m_malloc_fail_on_cnt = 101;

    ASSERT_FALSE(gw_cfg_ruuvi_json_generate(&gw_cfg, &json_str));
    ASSERT_EQ(nullptr, json_str.p_str);
    TEST_CHECK_LOG_RECORD(ESP_LOG_ERROR, string("Can't add json item: auto_update_cycle"));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_TRUE(g_pTestClass->m_mem_alloc_trace.is_empty());
}

TEST_F(TestGwCfgRuuviJsonGenerate, gw_cfg_ruuvi_json_generate_malloc_failed_on_auto_update_cycle_2) // NOLINT
{
    const gw_cfg_t   gw_cfg   = get_gateway_config_default();
    cjson_wrap_str_t json_str = cjson_wrap_str_null();

    cJSON_Hooks hooks = {
        .malloc_fn = &os_malloc,
        .free_fn   = &os_free_internal,
    };
    cJSON_InitHooks(&hooks);
    this->m_malloc_fail_on_cnt = 102;

    ASSERT_FALSE(gw_cfg_ruuvi_json_generate(&gw_cfg, &json_str));
    ASSERT_EQ(nullptr, json_str.p_str);
    TEST_CHECK_LOG_RECORD(ESP_LOG_ERROR, string("Can't add json item: auto_update_cycle"));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_TRUE(g_pTestClass->m_mem_alloc_trace.is_empty());
}

TEST_F(TestGwCfgRuuviJsonGenerate, gw_cfg_ruuvi_json_generate_malloc_failed_on_auto_update_cycle_3) // NOLINT
{
    const gw_cfg_t   gw_cfg   = get_gateway_config_default();
    cjson_wrap_str_t json_str = cjson_wrap_str_null();

    cJSON_Hooks hooks = {
        .malloc_fn = &os_malloc,
        .free_fn   = &os_free_internal,
    };
    cJSON_InitHooks(&hooks);
    this->m_malloc_fail_on_cnt = 103;

    ASSERT_FALSE(gw_cfg_ruuvi_json_generate(&gw_cfg, &json_str));
    ASSERT_EQ(nullptr, json_str.p_str);
    TEST_CHECK_LOG_RECORD(ESP_LOG_ERROR, string("Can't add json item: auto_update_cycle"));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_TRUE(g_pTestClass->m_mem_alloc_trace.is_empty());
}

TEST_F(TestGwCfgRuuviJsonGenerate, gw_cfg_ruuvi_json_generate_malloc_failed_on_auto_update_weekdays_bitmask) // NOLINT
{
    const gw_cfg_t   gw_cfg   = get_gateway_config_default();
    cjson_wrap_str_t json_str = cjson_wrap_str_null();

    cJSON_Hooks hooks = {
        .malloc_fn = &os_malloc,
        .free_fn   = &os_free_internal,
    };
    cJSON_InitHooks(&hooks);
    this->m_malloc_fail_on_cnt = 104;

    ASSERT_FALSE(gw_cfg_ruuvi_json_generate(&gw_cfg, &json_str));
    ASSERT_EQ(nullptr, json_str.p_str);
    TEST_CHECK_LOG_RECORD(ESP_LOG_ERROR, string("Can't add json item: auto_update_weekdays_bitmask"));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_TRUE(g_pTestClass->m_mem_alloc_trace.is_empty());
}

TEST_F(TestGwCfgRuuviJsonGenerate, gw_cfg_ruuvi_json_generate_malloc_failed_on_auto_update_weekdays_bitmask_2) // NOLINT
{
    const gw_cfg_t   gw_cfg   = get_gateway_config_default();
    cjson_wrap_str_t json_str = cjson_wrap_str_null();

    cJSON_Hooks hooks = {
        .malloc_fn = &os_malloc,
        .free_fn   = &os_free_internal,
    };
    cJSON_InitHooks(&hooks);
    this->m_malloc_fail_on_cnt = 105;

    ASSERT_FALSE(gw_cfg_ruuvi_json_generate(&gw_cfg, &json_str));
    ASSERT_EQ(nullptr, json_str.p_str);
    TEST_CHECK_LOG_RECORD(ESP_LOG_ERROR, string("Can't add json item: auto_update_weekdays_bitmask"));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_TRUE(g_pTestClass->m_mem_alloc_trace.is_empty());
}

TEST_F(TestGwCfgRuuviJsonGenerate, gw_cfg_ruuvi_json_generate_malloc_failed_on_auto_update_interval_from) // NOLINT
{
    const gw_cfg_t   gw_cfg   = get_gateway_config_default();
    cjson_wrap_str_t json_str = cjson_wrap_str_null();

    cJSON_Hooks hooks = {
        .malloc_fn = &os_malloc,
        .free_fn   = &os_free_internal,
    };
    cJSON_InitHooks(&hooks);
    this->m_malloc_fail_on_cnt = 106;

    ASSERT_FALSE(gw_cfg_ruuvi_json_generate(&gw_cfg, &json_str));
    ASSERT_EQ(nullptr, json_str.p_str);
    TEST_CHECK_LOG_RECORD(ESP_LOG_ERROR, string("Can't add json item: auto_update_interval_from"));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_TRUE(g_pTestClass->m_mem_alloc_trace.is_empty());
}

TEST_F(TestGwCfgRuuviJsonGenerate, gw_cfg_ruuvi_json_generate_malloc_failed_on_auto_update_interval_from_2) // NOLINT
{
    const gw_cfg_t   gw_cfg   = get_gateway_config_default();
    cjson_wrap_str_t json_str = cjson_wrap_str_null();

    cJSON_Hooks hooks = {
        .malloc_fn = &os_malloc,
        .free_fn   = &os_free_internal,
    };
    cJSON_InitHooks(&hooks);
    this->m_malloc_fail_on_cnt = 107;

    ASSERT_FALSE(gw_cfg_ruuvi_json_generate(&gw_cfg, &json_str));
    ASSERT_EQ(nullptr, json_str.p_str);
    TEST_CHECK_LOG_RECORD(ESP_LOG_ERROR, string("Can't add json item: auto_update_interval_from"));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_TRUE(g_pTestClass->m_mem_alloc_trace.is_empty());
}

TEST_F(TestGwCfgRuuviJsonGenerate, gw_cfg_ruuvi_json_generate_malloc_failed_on_auto_update_interval_to) // NOLINT
{
    const gw_cfg_t   gw_cfg   = get_gateway_config_default();
    cjson_wrap_str_t json_str = cjson_wrap_str_null();

    cJSON_Hooks hooks = {
        .malloc_fn = &os_malloc,
        .free_fn   = &os_free_internal,
    };
    cJSON_InitHooks(&hooks);
    this->m_malloc_fail_on_cnt = 108;

    ASSERT_FALSE(gw_cfg_ruuvi_json_generate(&gw_cfg, &json_str));
    ASSERT_EQ(nullptr, json_str.p_str);
    TEST_CHECK_LOG_RECORD(ESP_LOG_ERROR, string("Can't add json item: auto_update_interval_to"));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_TRUE(g_pTestClass->m_mem_alloc_trace.is_empty());
}

TEST_F(TestGwCfgRuuviJsonGenerate, gw_cfg_ruuvi_json_generate_malloc_failed_on_auto_update_interval_to_2) // NOLINT
{
    const gw_cfg_t   gw_cfg   = get_gateway_config_default();
    cjson_wrap_str_t json_str = cjson_wrap_str_null();

    cJSON_Hooks hooks = {
        .malloc_fn = &os_malloc,
        .free_fn   = &os_free_internal,
    };
    cJSON_InitHooks(&hooks);
    this->m_malloc_fail_on_cnt = 109;

    ASSERT_FALSE(gw_cfg_ruuvi_json_generate(&gw_cfg, &json_str));
    ASSERT_EQ(nullptr, json_str.p_str);
    TEST_CHECK_LOG_RECORD(ESP_LOG_ERROR, string("Can't add json item: auto_update_interval_to"));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_TRUE(g_pTestClass->m_mem_alloc_trace.is_empty());
}

TEST_F(TestGwCfgRuuviJsonGenerate, gw_cfg_ruuvi_json_generate_malloc_failed_on_auto_update_tz) // NOLINT
{
    const gw_cfg_t   gw_cfg   = get_gateway_config_default();
    cjson_wrap_str_t json_str = cjson_wrap_str_null();

    cJSON_Hooks hooks = {
        .malloc_fn = &os_malloc,
        .free_fn   = &os_free_internal,
    };
    cJSON_InitHooks(&hooks);
    this->m_malloc_fail_on_cnt = 110;

    ASSERT_FALSE(gw_cfg_ruuvi_json_generate(&gw_cfg, &json_str));
    ASSERT_EQ(nullptr, json_str.p_str);
    TEST_CHECK_LOG_RECORD(ESP_LOG_ERROR, string("Can't add json item: auto_update_tz_offset_hours"));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_TRUE(g_pTestClass->m_mem_alloc_trace.is_empty());
}

TEST_F(TestGwCfgRuuviJsonGenerate, gw_cfg_ruuvi_json_generate_malloc_failed_on_auto_update_tz_2) // NOLINT
{
    const gw_cfg_t   gw_cfg   = get_gateway_config_default();
    cjson_wrap_str_t json_str = cjson_wrap_str_null();

    cJSON_Hooks hooks = {
        .malloc_fn = &os_malloc,
        .free_fn   = &os_free_internal,
    };
    cJSON_InitHooks(&hooks);
    this->m_malloc_fail_on_cnt = 111;

    ASSERT_FALSE(gw_cfg_ruuvi_json_generate(&gw_cfg, &json_str));
    ASSERT_EQ(nullptr, json_str.p_str);
    TEST_CHECK_LOG_RECORD(ESP_LOG_ERROR, string("Can't add json item: auto_update_tz_offset_hours"));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_TRUE(g_pTestClass->m_mem_alloc_trace.is_empty());
}

TEST_F(TestGwCfgRuuviJsonGenerate, gw_cfg_ruuvi_json_generate_malloc_failed_on_ntp_use) // NOLINT
{
    const gw_cfg_t   gw_cfg   = get_gateway_config_default();
    cjson_wrap_str_t json_str = cjson_wrap_str_null();

    cJSON_Hooks hooks = {
        .malloc_fn = &os_malloc,
        .free_fn   = &os_free_internal,
    };
    cJSON_InitHooks(&hooks);
    this->m_malloc_fail_on_cnt = 112;

    ASSERT_FALSE(gw_cfg_ruuvi_json_generate(&gw_cfg, &json_str));
    ASSERT_EQ(nullptr, json_str.p_str);
    TEST_CHECK_LOG_RECORD(ESP_LOG_ERROR, string("Can't add json item: ntp_use"));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_TRUE(g_pTestClass->m_mem_alloc_trace.is_empty());
}

TEST_F(TestGwCfgRuuviJsonGenerate, gw_cfg_ruuvi_json_generate_malloc_failed_on_ntp_use_2) // NOLINT
{
    const gw_cfg_t   gw_cfg   = get_gateway_config_default();
    cjson_wrap_str_t json_str = cjson_wrap_str_null();

    cJSON_Hooks hooks = {
        .malloc_fn = &os_malloc,
        .free_fn   = &os_free_internal,
    };
    cJSON_InitHooks(&hooks);
    this->m_malloc_fail_on_cnt = 113;

    ASSERT_FALSE(gw_cfg_ruuvi_json_generate(&gw_cfg, &json_str));
    ASSERT_EQ(nullptr, json_str.p_str);
    TEST_CHECK_LOG_RECORD(ESP_LOG_ERROR, string("Can't add json item: ntp_use"));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_TRUE(g_pTestClass->m_mem_alloc_trace.is_empty());
}

TEST_F(TestGwCfgRuuviJsonGenerate, gw_cfg_ruuvi_json_generate_malloc_failed_on_ntp_use_dhcp) // NOLINT
{
    const gw_cfg_t   gw_cfg   = get_gateway_config_default();
    cjson_wrap_str_t json_str = cjson_wrap_str_null();

    cJSON_Hooks hooks = {
        .malloc_fn = &os_malloc,
        .free_fn   = &os_free_internal,
    };
    cJSON_InitHooks(&hooks);
    this->m_malloc_fail_on_cnt = 114;

    ASSERT_FALSE(gw_cfg_ruuvi_json_generate(&gw_cfg, &json_str));
    ASSERT_EQ(nullptr, json_str.p_str);
    TEST_CHECK_LOG_RECORD(ESP_LOG_ERROR, string("Can't add json item: ntp_use_dhcp"));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_TRUE(g_pTestClass->m_mem_alloc_trace.is_empty());
}

TEST_F(TestGwCfgRuuviJsonGenerate, gw_cfg_ruuvi_json_generate_malloc_failed_on_ntp_use_dhcp_2) // NOLINT
{
    const gw_cfg_t   gw_cfg   = get_gateway_config_default();
    cjson_wrap_str_t json_str = cjson_wrap_str_null();

    cJSON_Hooks hooks = {
        .malloc_fn = &os_malloc,
        .free_fn   = &os_free_internal,
    };
    cJSON_InitHooks(&hooks);
    this->m_malloc_fail_on_cnt = 115;

    ASSERT_FALSE(gw_cfg_ruuvi_json_generate(&gw_cfg, &json_str));
    ASSERT_EQ(nullptr, json_str.p_str);
    TEST_CHECK_LOG_RECORD(ESP_LOG_ERROR, string("Can't add json item: ntp_use_dhcp"));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_TRUE(g_pTestClass->m_mem_alloc_trace.is_empty());
}

TEST_F(TestGwCfgRuuviJsonGenerate, gw_cfg_ruuvi_json_generate_malloc_failed_on_ntp_server1) // NOLINT
{
    const gw_cfg_t   gw_cfg   = get_gateway_config_default();
    cjson_wrap_str_t json_str = cjson_wrap_str_null();

    cJSON_Hooks hooks = {
        .malloc_fn = &os_malloc,
        .free_fn   = &os_free_internal,
    };
    cJSON_InitHooks(&hooks);
    this->m_malloc_fail_on_cnt = 116;

    ASSERT_FALSE(gw_cfg_ruuvi_json_generate(&gw_cfg, &json_str));
    ASSERT_EQ(nullptr, json_str.p_str);
    TEST_CHECK_LOG_RECORD(ESP_LOG_ERROR, string("Can't add json item: ntp_server1"));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_TRUE(g_pTestClass->m_mem_alloc_trace.is_empty());
}

TEST_F(TestGwCfgRuuviJsonGenerate, gw_cfg_ruuvi_json_generate_malloc_failed_on_ntp_server1_2) // NOLINT
{
    const gw_cfg_t   gw_cfg   = get_gateway_config_default();
    cjson_wrap_str_t json_str = cjson_wrap_str_null();

    cJSON_Hooks hooks = {
        .malloc_fn = &os_malloc,
        .free_fn   = &os_free_internal,
    };
    cJSON_InitHooks(&hooks);
    this->m_malloc_fail_on_cnt = 117;

    ASSERT_FALSE(gw_cfg_ruuvi_json_generate(&gw_cfg, &json_str));
    ASSERT_EQ(nullptr, json_str.p_str);
    TEST_CHECK_LOG_RECORD(ESP_LOG_ERROR, string("Can't add json item: ntp_server1"));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_TRUE(g_pTestClass->m_mem_alloc_trace.is_empty());
}

TEST_F(TestGwCfgRuuviJsonGenerate, gw_cfg_ruuvi_json_generate_malloc_failed_on_ntp_server1_3) // NOLINT
{
    const gw_cfg_t   gw_cfg   = get_gateway_config_default();
    cjson_wrap_str_t json_str = cjson_wrap_str_null();

    cJSON_Hooks hooks = {
        .malloc_fn = &os_malloc,
        .free_fn   = &os_free_internal,
    };
    cJSON_InitHooks(&hooks);
    this->m_malloc_fail_on_cnt = 118;

    ASSERT_FALSE(gw_cfg_ruuvi_json_generate(&gw_cfg, &json_str));
    ASSERT_EQ(nullptr, json_str.p_str);
    TEST_CHECK_LOG_RECORD(ESP_LOG_ERROR, string("Can't add json item: ntp_server1"));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_TRUE(g_pTestClass->m_mem_alloc_trace.is_empty());
}

TEST_F(TestGwCfgRuuviJsonGenerate, gw_cfg_ruuvi_json_generate_malloc_failed_on_ntp_server2) // NOLINT
{
    const gw_cfg_t   gw_cfg   = get_gateway_config_default();
    cjson_wrap_str_t json_str = cjson_wrap_str_null();

    cJSON_Hooks hooks = {
        .malloc_fn = &os_malloc,
        .free_fn   = &os_free_internal,
    };
    cJSON_InitHooks(&hooks);
    this->m_malloc_fail_on_cnt = 119;

    ASSERT_FALSE(gw_cfg_ruuvi_json_generate(&gw_cfg, &json_str));
    ASSERT_EQ(nullptr, json_str.p_str);
    TEST_CHECK_LOG_RECORD(ESP_LOG_ERROR, string("Can't add json item: ntp_server2"));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_TRUE(g_pTestClass->m_mem_alloc_trace.is_empty());
}

TEST_F(TestGwCfgRuuviJsonGenerate, gw_cfg_ruuvi_json_generate_malloc_failed_on_ntp_server2_2) // NOLINT
{
    const gw_cfg_t   gw_cfg   = get_gateway_config_default();
    cjson_wrap_str_t json_str = cjson_wrap_str_null();

    cJSON_Hooks hooks = {
        .malloc_fn = &os_malloc,
        .free_fn   = &os_free_internal,
    };
    cJSON_InitHooks(&hooks);
    this->m_malloc_fail_on_cnt = 120;

    ASSERT_FALSE(gw_cfg_ruuvi_json_generate(&gw_cfg, &json_str));
    ASSERT_EQ(nullptr, json_str.p_str);
    TEST_CHECK_LOG_RECORD(ESP_LOG_ERROR, string("Can't add json item: ntp_server2"));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_TRUE(g_pTestClass->m_mem_alloc_trace.is_empty());
}

TEST_F(TestGwCfgRuuviJsonGenerate, gw_cfg_ruuvi_json_generate_malloc_failed_on_ntp_server2_3) // NOLINT
{
    const gw_cfg_t   gw_cfg   = get_gateway_config_default();
    cjson_wrap_str_t json_str = cjson_wrap_str_null();

    cJSON_Hooks hooks = {
        .malloc_fn = &os_malloc,
        .free_fn   = &os_free_internal,
    };
    cJSON_InitHooks(&hooks);
    this->m_malloc_fail_on_cnt = 121;

    ASSERT_FALSE(gw_cfg_ruuvi_json_generate(&gw_cfg, &json_str));
    ASSERT_EQ(nullptr, json_str.p_str);
    TEST_CHECK_LOG_RECORD(ESP_LOG_ERROR, string("Can't add json item: ntp_server2"));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_TRUE(g_pTestClass->m_mem_alloc_trace.is_empty());
}

TEST_F(TestGwCfgRuuviJsonGenerate, gw_cfg_ruuvi_json_generate_malloc_failed_on_ntp_server3) // NOLINT
{
    const gw_cfg_t   gw_cfg   = get_gateway_config_default();
    cjson_wrap_str_t json_str = cjson_wrap_str_null();

    cJSON_Hooks hooks = {
        .malloc_fn = &os_malloc,
        .free_fn   = &os_free_internal,
    };
    cJSON_InitHooks(&hooks);
    this->m_malloc_fail_on_cnt = 122;

    ASSERT_FALSE(gw_cfg_ruuvi_json_generate(&gw_cfg, &json_str));
    ASSERT_EQ(nullptr, json_str.p_str);
    TEST_CHECK_LOG_RECORD(ESP_LOG_ERROR, string("Can't add json item: ntp_server3"));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_TRUE(g_pTestClass->m_mem_alloc_trace.is_empty());
}

TEST_F(TestGwCfgRuuviJsonGenerate, gw_cfg_ruuvi_json_generate_malloc_failed_on_ntp_server3_2) // NOLINT
{
    const gw_cfg_t   gw_cfg   = get_gateway_config_default();
    cjson_wrap_str_t json_str = cjson_wrap_str_null();

    cJSON_Hooks hooks = {
        .malloc_fn = &os_malloc,
        .free_fn   = &os_free_internal,
    };
    cJSON_InitHooks(&hooks);
    this->m_malloc_fail_on_cnt = 123;

    ASSERT_FALSE(gw_cfg_ruuvi_json_generate(&gw_cfg, &json_str));
    ASSERT_EQ(nullptr, json_str.p_str);
    TEST_CHECK_LOG_RECORD(ESP_LOG_ERROR, string("Can't add json item: ntp_server3"));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_TRUE(g_pTestClass->m_mem_alloc_trace.is_empty());
}

TEST_F(TestGwCfgRuuviJsonGenerate, gw_cfg_ruuvi_json_generate_malloc_failed_on_ntp_server3_3) // NOLINT
{
    const gw_cfg_t   gw_cfg   = get_gateway_config_default();
    cjson_wrap_str_t json_str = cjson_wrap_str_null();

    cJSON_Hooks hooks = {
        .malloc_fn = &os_malloc,
        .free_fn   = &os_free_internal,
    };
    cJSON_InitHooks(&hooks);
    this->m_malloc_fail_on_cnt = 124;

    ASSERT_FALSE(gw_cfg_ruuvi_json_generate(&gw_cfg, &json_str));
    ASSERT_EQ(nullptr, json_str.p_str);
    TEST_CHECK_LOG_RECORD(ESP_LOG_ERROR, string("Can't add json item: ntp_server3"));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_TRUE(g_pTestClass->m_mem_alloc_trace.is_empty());
}

TEST_F(TestGwCfgRuuviJsonGenerate, gw_cfg_ruuvi_json_generate_malloc_failed_on_ntp_server4) // NOLINT
{
    const gw_cfg_t   gw_cfg   = get_gateway_config_default();
    cjson_wrap_str_t json_str = cjson_wrap_str_null();

    cJSON_Hooks hooks = {
        .malloc_fn = &os_malloc,
        .free_fn   = &os_free_internal,
    };
    cJSON_InitHooks(&hooks);
    this->m_malloc_fail_on_cnt = 125;

    ASSERT_FALSE(gw_cfg_ruuvi_json_generate(&gw_cfg, &json_str));
    ASSERT_EQ(nullptr, json_str.p_str);
    TEST_CHECK_LOG_RECORD(ESP_LOG_ERROR, string("Can't add json item: ntp_server4"));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_TRUE(g_pTestClass->m_mem_alloc_trace.is_empty());
}

TEST_F(TestGwCfgRuuviJsonGenerate, gw_cfg_ruuvi_json_generate_malloc_failed_on_ntp_server4_2) // NOLINT
{
    const gw_cfg_t   gw_cfg   = get_gateway_config_default();
    cjson_wrap_str_t json_str = cjson_wrap_str_null();

    cJSON_Hooks hooks = {
        .malloc_fn = &os_malloc,
        .free_fn   = &os_free_internal,
    };
    cJSON_InitHooks(&hooks);
    this->m_malloc_fail_on_cnt = 126;

    ASSERT_FALSE(gw_cfg_ruuvi_json_generate(&gw_cfg, &json_str));
    ASSERT_EQ(nullptr, json_str.p_str);
    TEST_CHECK_LOG_RECORD(ESP_LOG_ERROR, string("Can't add json item: ntp_server4"));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_TRUE(g_pTestClass->m_mem_alloc_trace.is_empty());
}

TEST_F(TestGwCfgRuuviJsonGenerate, gw_cfg_ruuvi_json_generate_malloc_failed_on_ntp_server4_3) // NOLINT
{
    const gw_cfg_t   gw_cfg   = get_gateway_config_default();
    cjson_wrap_str_t json_str = cjson_wrap_str_null();

    cJSON_Hooks hooks = {
        .malloc_fn = &os_malloc,
        .free_fn   = &os_free_internal,
    };
    cJSON_InitHooks(&hooks);
    this->m_malloc_fail_on_cnt = 127;

    ASSERT_FALSE(gw_cfg_ruuvi_json_generate(&gw_cfg, &json_str));
    ASSERT_EQ(nullptr, json_str.p_str);
    TEST_CHECK_LOG_RECORD(ESP_LOG_ERROR, string("Can't add json item: ntp_server4"));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_TRUE(g_pTestClass->m_mem_alloc_trace.is_empty());
}

TEST_F(TestGwCfgRuuviJsonGenerate, gw_cfg_ruuvi_json_generate_malloc_failed_on_company_id) // NOLINT
{
    const gw_cfg_t   gw_cfg   = get_gateway_config_default();
    cjson_wrap_str_t json_str = cjson_wrap_str_null();

    cJSON_Hooks hooks = {
        .malloc_fn = &os_malloc,
        .free_fn   = &os_free_internal,
    };
    cJSON_InitHooks(&hooks);
    this->m_malloc_fail_on_cnt = 128;

    ASSERT_FALSE(gw_cfg_ruuvi_json_generate(&gw_cfg, &json_str));
    ASSERT_EQ(nullptr, json_str.p_str);
    TEST_CHECK_LOG_RECORD(ESP_LOG_ERROR, string("Can't add json item: company_id"));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_TRUE(g_pTestClass->m_mem_alloc_trace.is_empty());
}

TEST_F(TestGwCfgRuuviJsonGenerate, gw_cfg_ruuvi_json_generate_malloc_failed_on_company_id_2) // NOLINT
{
    const gw_cfg_t   gw_cfg   = get_gateway_config_default();
    cjson_wrap_str_t json_str = cjson_wrap_str_null();

    cJSON_Hooks hooks = {
        .malloc_fn = &os_malloc,
        .free_fn   = &os_free_internal,
    };
    cJSON_InitHooks(&hooks);
    this->m_malloc_fail_on_cnt = 129;

    ASSERT_FALSE(gw_cfg_ruuvi_json_generate(&gw_cfg, &json_str));
    ASSERT_EQ(nullptr, json_str.p_str);
    TEST_CHECK_LOG_RECORD(ESP_LOG_ERROR, string("Can't add json item: company_id"));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_TRUE(g_pTestClass->m_mem_alloc_trace.is_empty());
}

TEST_F(TestGwCfgRuuviJsonGenerate, gw_cfg_ruuvi_json_generate_malloc_failed_on_company_use_filtering) // NOLINT
{
    const gw_cfg_t   gw_cfg   = get_gateway_config_default();
    cjson_wrap_str_t json_str = cjson_wrap_str_null();

    cJSON_Hooks hooks = {
        .malloc_fn = &os_malloc,
        .free_fn   = &os_free_internal,
    };
    cJSON_InitHooks(&hooks);
    this->m_malloc_fail_on_cnt = 130;

    ASSERT_FALSE(gw_cfg_ruuvi_json_generate(&gw_cfg, &json_str));
    ASSERT_EQ(nullptr, json_str.p_str);
    TEST_CHECK_LOG_RECORD(ESP_LOG_ERROR, string("Can't add json item: company_use_filtering"));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_TRUE(g_pTestClass->m_mem_alloc_trace.is_empty());
}

TEST_F(TestGwCfgRuuviJsonGenerate, gw_cfg_ruuvi_json_generate_malloc_failed_on_company_use_filtering_2) // NOLINT
{
    const gw_cfg_t   gw_cfg   = get_gateway_config_default();
    cjson_wrap_str_t json_str = cjson_wrap_str_null();

    cJSON_Hooks hooks = {
        .malloc_fn = &os_malloc,
        .free_fn   = &os_free_internal,
    };
    cJSON_InitHooks(&hooks);
    this->m_malloc_fail_on_cnt = 131;

    ASSERT_FALSE(gw_cfg_ruuvi_json_generate(&gw_cfg, &json_str));
    ASSERT_EQ(nullptr, json_str.p_str);
    TEST_CHECK_LOG_RECORD(ESP_LOG_ERROR, string("Can't add json item: company_use_filtering"));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_TRUE(g_pTestClass->m_mem_alloc_trace.is_empty());
}

TEST_F(TestGwCfgRuuviJsonGenerate, gw_cfg_ruuvi_json_generate_malloc_failed_on_use_coded_phy) // NOLINT
{
    const gw_cfg_t   gw_cfg   = get_gateway_config_default();
    cjson_wrap_str_t json_str = cjson_wrap_str_null();

    cJSON_Hooks hooks = {
        .malloc_fn = &os_malloc,
        .free_fn   = &os_free_internal,
    };
    cJSON_InitHooks(&hooks);
    this->m_malloc_fail_on_cnt = 132;

    ASSERT_FALSE(gw_cfg_ruuvi_json_generate(&gw_cfg, &json_str));
    ASSERT_EQ(nullptr, json_str.p_str);
    TEST_CHECK_LOG_RECORD(ESP_LOG_ERROR, string("Can't add json item: scan_coded_phy"));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_TRUE(g_pTestClass->m_mem_alloc_trace.is_empty());
}

TEST_F(TestGwCfgRuuviJsonGenerate, gw_cfg_ruuvi_json_generate_malloc_failed_on_use_coded_phy_2) // NOLINT
{
    const gw_cfg_t   gw_cfg   = get_gateway_config_default();
    cjson_wrap_str_t json_str = cjson_wrap_str_null();

    cJSON_Hooks hooks = {
        .malloc_fn = &os_malloc,
        .free_fn   = &os_free_internal,
    };
    cJSON_InitHooks(&hooks);
    this->m_malloc_fail_on_cnt = 133;

    ASSERT_FALSE(gw_cfg_ruuvi_json_generate(&gw_cfg, &json_str));
    ASSERT_EQ(nullptr, json_str.p_str);
    TEST_CHECK_LOG_RECORD(ESP_LOG_ERROR, string("Can't add json item: scan_coded_phy"));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_TRUE(g_pTestClass->m_mem_alloc_trace.is_empty());
}

TEST_F(TestGwCfgRuuviJsonGenerate, gw_cfg_ruuvi_json_generate_malloc_failed_on_use_1mbit_phy) // NOLINT
{
    const gw_cfg_t   gw_cfg   = get_gateway_config_default();
    cjson_wrap_str_t json_str = cjson_wrap_str_null();

    cJSON_Hooks hooks = {
        .malloc_fn = &os_malloc,
        .free_fn   = &os_free_internal,
    };
    cJSON_InitHooks(&hooks);
    this->m_malloc_fail_on_cnt = 134;

    ASSERT_FALSE(gw_cfg_ruuvi_json_generate(&gw_cfg, &json_str));
    ASSERT_EQ(nullptr, json_str.p_str);
    TEST_CHECK_LOG_RECORD(ESP_LOG_ERROR, string("Can't add json item: scan_1mbit_phy"));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_TRUE(g_pTestClass->m_mem_alloc_trace.is_empty());
}

TEST_F(TestGwCfgRuuviJsonGenerate, gw_cfg_ruuvi_json_generate_malloc_failed_on_use_1mbit_phy_2) // NOLINT
{
    const gw_cfg_t   gw_cfg   = get_gateway_config_default();
    cjson_wrap_str_t json_str = cjson_wrap_str_null();

    cJSON_Hooks hooks = {
        .malloc_fn = &os_malloc,
        .free_fn   = &os_free_internal,
    };
    cJSON_InitHooks(&hooks);
    this->m_malloc_fail_on_cnt = 135;

    ASSERT_FALSE(gw_cfg_ruuvi_json_generate(&gw_cfg, &json_str));
    ASSERT_EQ(nullptr, json_str.p_str);
    TEST_CHECK_LOG_RECORD(ESP_LOG_ERROR, string("Can't add json item: scan_1mbit_phy"));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_TRUE(g_pTestClass->m_mem_alloc_trace.is_empty());
}

TEST_F(TestGwCfgRuuviJsonGenerate, gw_cfg_ruuvi_json_generate_malloc_failed_on_use_extended_payload) // NOLINT
{
    const gw_cfg_t   gw_cfg   = get_gateway_config_default();
    cjson_wrap_str_t json_str = cjson_wrap_str_null();

    cJSON_Hooks hooks = {
        .malloc_fn = &os_malloc,
        .free_fn   = &os_free_internal,
    };
    cJSON_InitHooks(&hooks);
    this->m_malloc_fail_on_cnt = 136;

    ASSERT_FALSE(gw_cfg_ruuvi_json_generate(&gw_cfg, &json_str));
    ASSERT_EQ(nullptr, json_str.p_str);
    TEST_CHECK_LOG_RECORD(ESP_LOG_ERROR, string("Can't add json item: scan_extended_payload"));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_TRUE(g_pTestClass->m_mem_alloc_trace.is_empty());
}

TEST_F(TestGwCfgRuuviJsonGenerate, gw_cfg_ruuvi_json_generate_malloc_failed_on_use_extended_payload_2) // NOLINT
{
    const gw_cfg_t   gw_cfg   = get_gateway_config_default();
    cjson_wrap_str_t json_str = cjson_wrap_str_null();

    cJSON_Hooks hooks = {
        .malloc_fn = &os_malloc,
        .free_fn   = &os_free_internal,
    };
    cJSON_InitHooks(&hooks);
    this->m_malloc_fail_on_cnt = 137;

    ASSERT_FALSE(gw_cfg_ruuvi_json_generate(&gw_cfg, &json_str));
    ASSERT_EQ(nullptr, json_str.p_str);
    TEST_CHECK_LOG_RECORD(ESP_LOG_ERROR, string("Can't add json item: scan_extended_payload"));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_TRUE(g_pTestClass->m_mem_alloc_trace.is_empty());
}

TEST_F(TestGwCfgRuuviJsonGenerate, gw_cfg_ruuvi_json_generate_malloc_failed_on_use_channel_37) // NOLINT
{
    const gw_cfg_t   gw_cfg   = get_gateway_config_default();
    cjson_wrap_str_t json_str = cjson_wrap_str_null();

    cJSON_Hooks hooks = {
        .malloc_fn = &os_malloc,
        .free_fn   = &os_free_internal,
    };
    cJSON_InitHooks(&hooks);
    this->m_malloc_fail_on_cnt = 138;

    ASSERT_FALSE(gw_cfg_ruuvi_json_generate(&gw_cfg, &json_str));
    ASSERT_EQ(nullptr, json_str.p_str);
    TEST_CHECK_LOG_RECORD(ESP_LOG_ERROR, string("Can't add json item: scan_channel_37"));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_TRUE(g_pTestClass->m_mem_alloc_trace.is_empty());
}

TEST_F(TestGwCfgRuuviJsonGenerate, gw_cfg_ruuvi_json_generate_malloc_failed_on_use_channel_37_2) // NOLINT
{
    const gw_cfg_t   gw_cfg   = get_gateway_config_default();
    cjson_wrap_str_t json_str = cjson_wrap_str_null();

    cJSON_Hooks hooks = {
        .malloc_fn = &os_malloc,
        .free_fn   = &os_free_internal,
    };
    cJSON_InitHooks(&hooks);
    this->m_malloc_fail_on_cnt = 139;

    ASSERT_FALSE(gw_cfg_ruuvi_json_generate(&gw_cfg, &json_str));
    ASSERT_EQ(nullptr, json_str.p_str);
    TEST_CHECK_LOG_RECORD(ESP_LOG_ERROR, string("Can't add json item: scan_channel_37"));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_TRUE(g_pTestClass->m_mem_alloc_trace.is_empty());
}

TEST_F(TestGwCfgRuuviJsonGenerate, gw_cfg_ruuvi_json_generate_malloc_failed_on_use_channel_38) // NOLINT
{
    const gw_cfg_t   gw_cfg   = get_gateway_config_default();
    cjson_wrap_str_t json_str = cjson_wrap_str_null();

    cJSON_Hooks hooks = {
        .malloc_fn = &os_malloc,
        .free_fn   = &os_free_internal,
    };
    cJSON_InitHooks(&hooks);
    this->m_malloc_fail_on_cnt = 140;

    ASSERT_FALSE(gw_cfg_ruuvi_json_generate(&gw_cfg, &json_str));
    ASSERT_EQ(nullptr, json_str.p_str);
    TEST_CHECK_LOG_RECORD(ESP_LOG_ERROR, string("Can't add json item: scan_channel_38"));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_TRUE(g_pTestClass->m_mem_alloc_trace.is_empty());
}

TEST_F(TestGwCfgRuuviJsonGenerate, gw_cfg_ruuvi_json_generate_malloc_failed_on_use_channel_38_2) // NOLINT
{
    const gw_cfg_t   gw_cfg   = get_gateway_config_default();
    cjson_wrap_str_t json_str = cjson_wrap_str_null();

    cJSON_Hooks hooks = {
        .malloc_fn = &os_malloc,
        .free_fn   = &os_free_internal,
    };
    cJSON_InitHooks(&hooks);
    this->m_malloc_fail_on_cnt = 141;

    ASSERT_FALSE(gw_cfg_ruuvi_json_generate(&gw_cfg, &json_str));
    ASSERT_EQ(nullptr, json_str.p_str);
    TEST_CHECK_LOG_RECORD(ESP_LOG_ERROR, string("Can't add json item: scan_channel_38"));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_TRUE(g_pTestClass->m_mem_alloc_trace.is_empty());
}

TEST_F(TestGwCfgRuuviJsonGenerate, gw_cfg_ruuvi_json_generate_malloc_failed_on_use_channel_39) // NOLINT
{
    const gw_cfg_t   gw_cfg   = get_gateway_config_default();
    cjson_wrap_str_t json_str = cjson_wrap_str_null();

    cJSON_Hooks hooks = {
        .malloc_fn = &os_malloc,
        .free_fn   = &os_free_internal,
    };
    cJSON_InitHooks(&hooks);
    this->m_malloc_fail_on_cnt = 142;

    ASSERT_FALSE(gw_cfg_ruuvi_json_generate(&gw_cfg, &json_str));
    ASSERT_EQ(nullptr, json_str.p_str);
    TEST_CHECK_LOG_RECORD(ESP_LOG_ERROR, string("Can't add json item: scan_channel_39"));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_TRUE(g_pTestClass->m_mem_alloc_trace.is_empty());
}

TEST_F(TestGwCfgRuuviJsonGenerate, gw_cfg_ruuvi_json_generate_malloc_failed_on_use_channel_39_2) // NOLINT
{
    const gw_cfg_t   gw_cfg   = get_gateway_config_default();
    cjson_wrap_str_t json_str = cjson_wrap_str_null();

    cJSON_Hooks hooks = {
        .malloc_fn = &os_malloc,
        .free_fn   = &os_free_internal,
    };
    cJSON_InitHooks(&hooks);
    this->m_malloc_fail_on_cnt = 143;

    ASSERT_FALSE(gw_cfg_ruuvi_json_generate(&gw_cfg, &json_str));
    ASSERT_EQ(nullptr, json_str.p_str);
    TEST_CHECK_LOG_RECORD(ESP_LOG_ERROR, string("Can't add json item: scan_channel_39"));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_TRUE(g_pTestClass->m_mem_alloc_trace.is_empty());
}

TEST_F(TestGwCfgRuuviJsonGenerate, gw_cfg_ruuvi_json_generate_malloc_failed_on_scan_filter_allow_listed) // NOLINT
{
    const gw_cfg_t   gw_cfg   = get_gateway_config_default();
    cjson_wrap_str_t json_str = cjson_wrap_str_null();

    cJSON_Hooks hooks = {
        .malloc_fn = &os_malloc,
        .free_fn   = &os_free_internal,
    };
    cJSON_InitHooks(&hooks);
    this->m_malloc_fail_on_cnt = 144;

    ASSERT_FALSE(gw_cfg_ruuvi_json_generate(&gw_cfg, &json_str));
    ASSERT_EQ(nullptr, json_str.p_str);
    TEST_CHECK_LOG_RECORD(ESP_LOG_ERROR, string("Can't add json item: scan_filter_allow_listed"));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_TRUE(g_pTestClass->m_mem_alloc_trace.is_empty());
}

TEST_F(TestGwCfgRuuviJsonGenerate, gw_cfg_ruuvi_json_generate_malloc_failed_on_scan_filter_allow_listed_2) // NOLINT
{
    const gw_cfg_t   gw_cfg   = get_gateway_config_default();
    cjson_wrap_str_t json_str = cjson_wrap_str_null();

    cJSON_Hooks hooks = {
        .malloc_fn = &os_malloc,
        .free_fn   = &os_free_internal,
    };
    cJSON_InitHooks(&hooks);
    this->m_malloc_fail_on_cnt = 145;

    ASSERT_FALSE(gw_cfg_ruuvi_json_generate(&gw_cfg, &json_str));
    ASSERT_EQ(nullptr, json_str.p_str);
    TEST_CHECK_LOG_RECORD(ESP_LOG_ERROR, string("Can't add json item: scan_filter_allow_listed"));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_TRUE(g_pTestClass->m_mem_alloc_trace.is_empty());
}

TEST_F(TestGwCfgRuuviJsonGenerate, gw_cfg_ruuvi_json_generate_malloc_failed_on_scan_filter_list) // NOLINT
{
    const gw_cfg_t   gw_cfg   = get_gateway_config_default();
    cjson_wrap_str_t json_str = cjson_wrap_str_null();

    cJSON_Hooks hooks = {
        .malloc_fn = &os_malloc,
        .free_fn   = &os_free_internal,
    };
    cJSON_InitHooks(&hooks);
    this->m_malloc_fail_on_cnt = 146;

    ASSERT_FALSE(gw_cfg_ruuvi_json_generate(&gw_cfg, &json_str));
    ASSERT_EQ(nullptr, json_str.p_str);
    TEST_CHECK_LOG_RECORD(ESP_LOG_ERROR, string("Can't add json item: scan_filter_list"));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_TRUE(g_pTestClass->m_mem_alloc_trace.is_empty());
}

TEST_F(TestGwCfgRuuviJsonGenerate, gw_cfg_ruuvi_json_generate_malloc_failed_on_scan_filter_list_2) // NOLINT
{
    const gw_cfg_t   gw_cfg   = get_gateway_config_default();
    cjson_wrap_str_t json_str = cjson_wrap_str_null();

    cJSON_Hooks hooks = {
        .malloc_fn = &os_malloc,
        .free_fn   = &os_free_internal,
    };
    cJSON_InitHooks(&hooks);
    this->m_malloc_fail_on_cnt = 147;

    ASSERT_FALSE(gw_cfg_ruuvi_json_generate(&gw_cfg, &json_str));
    ASSERT_EQ(nullptr, json_str.p_str);
    TEST_CHECK_LOG_RECORD(ESP_LOG_ERROR, string("Can't add json item: scan_filter_list"));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_TRUE(g_pTestClass->m_mem_alloc_trace.is_empty());
}

TEST_F(TestGwCfgRuuviJsonGenerate, gw_cfg_ruuvi_json_generate_malloc_failed_on_coordinates) // NOLINT
{
    const gw_cfg_t   gw_cfg   = get_gateway_config_default();
    cjson_wrap_str_t json_str = cjson_wrap_str_null();

    cJSON_Hooks hooks = {
        .malloc_fn = &os_malloc,
        .free_fn   = &os_free_internal,
    };
    cJSON_InitHooks(&hooks);
    this->m_malloc_fail_on_cnt = 148;

    ASSERT_FALSE(gw_cfg_ruuvi_json_generate(&gw_cfg, &json_str));
    ASSERT_EQ(nullptr, json_str.p_str);
    TEST_CHECK_LOG_RECORD(ESP_LOG_ERROR, string("Can't add json item: coordinates"));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_TRUE(g_pTestClass->m_mem_alloc_trace.is_empty());
}

TEST_F(TestGwCfgRuuviJsonGenerate, gw_cfg_ruuvi_json_generate_malloc_failed_on_coordinates_2) // NOLINT
{
    const gw_cfg_t   gw_cfg   = get_gateway_config_default();
    cjson_wrap_str_t json_str = cjson_wrap_str_null();

    cJSON_Hooks hooks = {
        .malloc_fn = &os_malloc,
        .free_fn   = &os_free_internal,
    };
    cJSON_InitHooks(&hooks);
    this->m_malloc_fail_on_cnt = 149;

    ASSERT_FALSE(gw_cfg_ruuvi_json_generate(&gw_cfg, &json_str));
    ASSERT_EQ(nullptr, json_str.p_str);
    TEST_CHECK_LOG_RECORD(ESP_LOG_ERROR, string("Can't add json item: coordinates"));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_TRUE(g_pTestClass->m_mem_alloc_trace.is_empty());
}

TEST_F(TestGwCfgRuuviJsonGenerate, gw_cfg_ruuvi_json_generate_malloc_failed_on_coordinates_3) // NOLINT
{
    const gw_cfg_t   gw_cfg   = get_gateway_config_default();
    cjson_wrap_str_t json_str = cjson_wrap_str_null();

    cJSON_Hooks hooks = {
        .malloc_fn = &os_malloc,
        .free_fn   = &os_free_internal,
    };
    cJSON_InitHooks(&hooks);
    this->m_malloc_fail_on_cnt = 150;

    ASSERT_FALSE(gw_cfg_ruuvi_json_generate(&gw_cfg, &json_str));
    ASSERT_EQ(nullptr, json_str.p_str);
    TEST_CHECK_LOG_RECORD(ESP_LOG_ERROR, string("Can't add json item: coordinates"));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_TRUE(g_pTestClass->m_mem_alloc_trace.is_empty());
}

TEST_F(TestGwCfgRuuviJsonGenerate, gw_cfg_ruuvi_json_generate_malloc_failed_on_converting_to_json_string) // NOLINT
{
    const gw_cfg_t   gw_cfg   = get_gateway_config_default();
    cjson_wrap_str_t json_str = cjson_wrap_str_null();

    cJSON_Hooks hooks = {
        .malloc_fn = &os_malloc,
        .free_fn   = &os_free_internal,
    };
    cJSON_InitHooks(&hooks);
    this->m_malloc_fail_on_cnt = 151;

    ASSERT_FALSE(gw_cfg_ruuvi_json_generate(&gw_cfg, &json_str));
    ASSERT_EQ(nullptr, json_str.p_str);
    TEST_CHECK_LOG_RECORD(ESP_LOG_ERROR, string("Can't create json string"));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_TRUE(g_pTestClass->m_mem_alloc_trace.is_empty());
}
