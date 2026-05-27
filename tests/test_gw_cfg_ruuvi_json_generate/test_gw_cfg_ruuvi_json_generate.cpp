/**
 * @file test_gw_cfg_ruuvi_json_generate.cpp
 * @author TheSomeMan
 * @date 2021-10-06
 * @copyright Ruuvi Innovations Ltd, license BSD-3-Clause.
 */

#include "json_ruuvi.h"
#include <cstring>
#include <iterator>
#include "gtest/gtest.h"
#include "gw_mac.h"
#include "gw_cfg.h"
#include "gw_cfg_default.h"
#include "gw_cfg_ruuvi_json.h"
#include "gw_cfg_json_generate.h"
#include "esp_log_wrapper.hpp"
#include "os_mutex_recursive.h"
#include "os_mutex.h"
#include "os_task.h"
#include "lwip/ip4_addr.h"
#include "event_mgr.h"
#include "gw_cfg_storage.h"

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
    bool          m_flag_storage_ready { false };
    bool          m_flag_storage_http_cli_cert { false };
    bool          m_flag_storage_http_cli_key { false };
    bool          m_flag_storage_stat_cli_cert { false };
    bool          m_flag_storage_stat_cli_key { false };
    bool          m_flag_storage_mqtt_cli_cert { false };
    bool          m_flag_storage_mqtt_cli_key { false };
    bool          m_flag_storage_remote_cfg_cli_cert { false };
    bool          m_flag_storage_remote_cfg_cli_key { false };

    bool m_flag_storage_http_srv_cert { false };
    bool m_flag_storage_stat_srv_cert { false };
    bool m_flag_storage_mqtt_srv_cert { false };
    bool m_flag_storage_remote_cfg_srv_cert { false };

    bool m_flag_storage_http_path { false };
    bool m_flag_storage_http_query { false };
    bool m_flag_storage_http_headers { false };
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

bool
gw_cfg_storage_check(void)
{
    return g_pTestClass->m_flag_storage_ready;
}

bool
gw_cfg_storage_check_file(const char* const p_file_name, const bool is_blob, size_t* const p_file_size)
{
    if (nullptr != p_file_size)
    {
        *p_file_size = 0;
    }
    if (!is_blob)
    {
        if (0 == strcmp(GW_CFG_STORAGE_SSL_HTTP_CLI_CERT, p_file_name))
        {
            return g_pTestClass->m_flag_storage_ready && g_pTestClass->m_flag_storage_http_cli_cert;
        }
        if (0 == strcmp(GW_CFG_STORAGE_SSL_HTTP_CLI_KEY, p_file_name))
        {
            return g_pTestClass->m_flag_storage_ready && g_pTestClass->m_flag_storage_http_cli_key;
        }
        if (0 == strcmp(GW_CFG_STORAGE_SSL_STAT_CLI_CERT, p_file_name))
        {
            return g_pTestClass->m_flag_storage_ready && g_pTestClass->m_flag_storage_stat_cli_cert;
        }
        if (0 == strcmp(GW_CFG_STORAGE_SSL_STAT_CLI_KEY, p_file_name))
        {
            return g_pTestClass->m_flag_storage_ready && g_pTestClass->m_flag_storage_stat_cli_key;
        }
        if (0 == strcmp(GW_CFG_STORAGE_SSL_MQTT_CLI_CERT, p_file_name))
        {
            return g_pTestClass->m_flag_storage_ready && g_pTestClass->m_flag_storage_mqtt_cli_cert;
        }
        if (0 == strcmp(GW_CFG_STORAGE_SSL_MQTT_CLI_KEY, p_file_name))
        {
            return g_pTestClass->m_flag_storage_ready && g_pTestClass->m_flag_storage_mqtt_cli_key;
        }
        if (0 == strcmp(GW_CFG_STORAGE_SSL_REMOTE_CFG_CLI_CERT, p_file_name))
        {
            return g_pTestClass->m_flag_storage_ready && g_pTestClass->m_flag_storage_remote_cfg_cli_cert;
        }
        if (0 == strcmp(GW_CFG_STORAGE_SSL_REMOTE_CFG_CLI_KEY, p_file_name))
        {
            return g_pTestClass->m_flag_storage_ready && g_pTestClass->m_flag_storage_remote_cfg_cli_key;
        }

        if (0 == strcmp(GW_CFG_STORAGE_SSL_HTTP_SRV_CERT, p_file_name))
        {
            return g_pTestClass->m_flag_storage_ready && g_pTestClass->m_flag_storage_http_srv_cert;
        }
        if (0 == strcmp(GW_CFG_STORAGE_SSL_STAT_SRV_CERT, p_file_name))
        {
            return g_pTestClass->m_flag_storage_ready && g_pTestClass->m_flag_storage_stat_srv_cert;
        }
        if (0 == strcmp(GW_CFG_STORAGE_SSL_MQTT_SRV_CERT, p_file_name))
        {
            return g_pTestClass->m_flag_storage_ready && g_pTestClass->m_flag_storage_mqtt_srv_cert;
        }
        if (0 == strcmp(GW_CFG_STORAGE_SSL_REMOTE_CFG_SRV_CERT, p_file_name))
        {
            return g_pTestClass->m_flag_storage_ready && g_pTestClass->m_flag_storage_remote_cfg_srv_cert;
        }
    }
    else
    {
        if (0 == strcmp(GW_CFG_STORAGE_HTTP_PATH, p_file_name))
        {
            return g_pTestClass->m_flag_storage_ready && g_pTestClass->m_flag_storage_http_path;
        }
        if (0 == strcmp(GW_CFG_STORAGE_HTTP_QUERY, p_file_name))
        {
            return g_pTestClass->m_flag_storage_ready && g_pTestClass->m_flag_storage_http_query;
        }
        if (0 == strcmp(GW_CFG_STORAGE_HTTP_HEADERS, p_file_name))
        {
            return g_pTestClass->m_flag_storage_ready && g_pTestClass->m_flag_storage_http_headers;
        }
    }

    return false;
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
               "\t\"storage\":\t{\n"
               "\t\t\"storage_ready\":\tfalse\n"
               "\t},\n"
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
               "\t\"remote_cfg_use_ssl_client_cert\":\tfalse,\n"
               "\t\"remote_cfg_use_ssl_server_cert\":\tfalse,\n"
               "\t\"remote_cfg_refresh_interval_minutes\":\t0,\n"
               "\t\"use_http_ruuvi\":\ttrue,\n"
               "\t\"use_http\":\ttrue,\n"
               "\t\"http_url\":\t\"" RUUVI_GATEWAY_HTTP_DEFAULT_URL "\",\n"
               "\t\"http_data_format\":\t\"ruuvi\",\n"
               "\t\"http_auth\":\t\"none\",\n"
               "\t\"use_http_stat\":\ttrue,\n"
               "\t\"http_stat_url\":\t\"" RUUVI_GATEWAY_HTTP_STATUS_URL "\",\n"
               "\t\"http_stat_user\":\t\"\",\n"
               "\t\"http_stat_use_ssl_client_cert\":\tfalse,\n"
               "\t\"http_stat_use_ssl_server_cert\":\tfalse,\n"
               "\t\"use_mqtt\":\tfalse,\n"
               "\t\"mqtt_disable_retained_messages\":\tfalse,\n"
               "\t\"mqtt_transport\":\t\"TCP\",\n"
               "\t\"mqtt_data_format\":\t\"ruuvi_raw\",\n"
               "\t\"mqtt_server\":\t\"test.mosquitto.org\",\n"
               "\t\"mqtt_port\":\t1883,\n"
               "\t\"mqtt_sending_interval\":\t0,\n"
               "\t\"mqtt_prefix\":\t\"ruuvi/AA:BB:CC:DD:EE:FF/\",\n"
               "\t\"mqtt_client_id\":\t\"AA:BB:CC:DD:EE:FF\",\n"
               "\t\"mqtt_user\":\t\"\",\n"
               "\t\"mqtt_use_ssl_client_cert\":\tfalse,\n"
               "\t\"mqtt_use_ssl_server_cert\":\tfalse,\n"
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
               "\t\"ntp_server3\":\t\"pool.ntp.org\",\n"
               "\t\"ntp_server4\":\t\"time.ruuvi.com\",\n"
               "\t\"company_id\":\t1177,\n"
               "\t\"company_use_filtering\":\ttrue,\n"
               "\t\"scan_coded_phy\":\tfalse,\n"
               "\t\"scan_1mbit_phy\":\ttrue,\n"
               "\t\"scan_2mbit_phy\":\ttrue,\n"
               "\t\"scan_channel_37\":\ttrue,\n"
               "\t\"scan_channel_38\":\ttrue,\n"
               "\t\"scan_channel_39\":\ttrue,\n"
               "\t\"scan_default\":\ttrue,\n"
               "\t\"scan_filter_allow_listed\":\tfalse,\n"
               "\t\"scan_filter_list\":\t[],\n"
               "\t\"coordinates\":\t\"\",\n"
               "\t\"fw_update_url\":\t\"https://network.ruuvi.com/firmwareupdate\"\n"
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
               "\t\"storage\":\t{\n"
               "\t\t\"storage_ready\":\tfalse\n"
               "\t},\n"
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
               "\t\"remote_cfg_use_ssl_client_cert\":\tfalse,\n"
               "\t\"remote_cfg_use_ssl_server_cert\":\tfalse,\n"
               "\t\"remote_cfg_refresh_interval_minutes\":\t0,\n"
               "\t\"use_http_ruuvi\":\ttrue,\n"
               "\t\"use_http\":\ttrue,\n"
               "\t\"http_url\":\t\"" RUUVI_GATEWAY_HTTP_DEFAULT_URL "\",\n"
               "\t\"http_data_format\":\t\"ruuvi\",\n"
               "\t\"http_auth\":\t\"none\",\n"
               "\t\"use_http_stat\":\ttrue,\n"
               "\t\"http_stat_url\":\t\"" RUUVI_GATEWAY_HTTP_STATUS_URL "\",\n"
               "\t\"http_stat_user\":\t\"\",\n"
               "\t\"http_stat_use_ssl_client_cert\":\tfalse,\n"
               "\t\"http_stat_use_ssl_server_cert\":\tfalse,\n"
               "\t\"use_mqtt\":\tfalse,\n"
               "\t\"mqtt_disable_retained_messages\":\tfalse,\n"
               "\t\"mqtt_transport\":\t\"TCP\",\n"
               "\t\"mqtt_data_format\":\t\"ruuvi_raw\",\n"
               "\t\"mqtt_server\":\t\"test.mosquitto.org\",\n"
               "\t\"mqtt_port\":\t1883,\n"
               "\t\"mqtt_sending_interval\":\t0,\n"
               "\t\"mqtt_prefix\":\t\"ruuvi/AA:BB:CC:DD:EE:FF/\",\n"
               "\t\"mqtt_client_id\":\t\"AA:BB:CC:DD:EE:FF\",\n"
               "\t\"mqtt_user\":\t\"\",\n"
               "\t\"mqtt_use_ssl_client_cert\":\tfalse,\n"
               "\t\"mqtt_use_ssl_server_cert\":\tfalse,\n"
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
               "\t\"ntp_server3\":\t\"pool.ntp.org\",\n"
               "\t\"ntp_server4\":\t\"time.ruuvi.com\",\n"
               "\t\"company_id\":\t1177,\n"
               "\t\"company_use_filtering\":\ttrue,\n"
               "\t\"scan_coded_phy\":\tfalse,\n"
               "\t\"scan_1mbit_phy\":\ttrue,\n"
               "\t\"scan_2mbit_phy\":\ttrue,\n"
               "\t\"scan_channel_37\":\ttrue,\n"
               "\t\"scan_channel_38\":\ttrue,\n"
               "\t\"scan_channel_39\":\ttrue,\n"
               "\t\"scan_default\":\ttrue,\n"
               "\t\"scan_filter_allow_listed\":\tfalse,\n"
               "\t\"scan_filter_list\":\t[],\n"
               "\t\"coordinates\":\t\"\",\n"
               "\t\"fw_update_url\":\t\"https://network.ruuvi.com/firmwareupdate\"\n"
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
               "\t\"storage\":\t{\n"
               "\t\t\"storage_ready\":\tfalse\n"
               "\t},\n"
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
               "\t\"remote_cfg_use_ssl_client_cert\":\tfalse,\n"
               "\t\"remote_cfg_use_ssl_server_cert\":\tfalse,\n"
               "\t\"remote_cfg_refresh_interval_minutes\":\t0,\n"
               "\t\"use_http_ruuvi\":\ttrue,\n"
               "\t\"use_http\":\ttrue,\n"
               "\t\"http_url\":\t\"" RUUVI_GATEWAY_HTTP_DEFAULT_URL "\",\n"
               "\t\"http_data_format\":\t\"ruuvi\",\n"
               "\t\"http_auth\":\t\"none\",\n"
               "\t\"use_http_stat\":\ttrue,\n"
               "\t\"http_stat_url\":\t\"" RUUVI_GATEWAY_HTTP_STATUS_URL "\",\n"
               "\t\"http_stat_user\":\t\"\",\n"
               "\t\"http_stat_use_ssl_client_cert\":\tfalse,\n"
               "\t\"http_stat_use_ssl_server_cert\":\tfalse,\n"
               "\t\"use_mqtt\":\tfalse,\n"
               "\t\"mqtt_disable_retained_messages\":\tfalse,\n"
               "\t\"mqtt_transport\":\t\"TCP\",\n"
               "\t\"mqtt_data_format\":\t\"ruuvi_raw\",\n"
               "\t\"mqtt_server\":\t\"test.mosquitto.org\",\n"
               "\t\"mqtt_port\":\t1883,\n"
               "\t\"mqtt_sending_interval\":\t0,\n"
               "\t\"mqtt_prefix\":\t\"ruuvi/AA:BB:CC:DD:EE:FF/\",\n"
               "\t\"mqtt_client_id\":\t\"AA:BB:CC:DD:EE:FF\",\n"
               "\t\"mqtt_user\":\t\"\",\n"
               "\t\"mqtt_use_ssl_client_cert\":\tfalse,\n"
               "\t\"mqtt_use_ssl_server_cert\":\tfalse,\n"
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
               "\t\"ntp_server3\":\t\"pool.ntp.org\",\n"
               "\t\"ntp_server4\":\t\"time.ruuvi.com\",\n"
               "\t\"company_id\":\t1177,\n"
               "\t\"company_use_filtering\":\ttrue,\n"
               "\t\"scan_coded_phy\":\tfalse,\n"
               "\t\"scan_1mbit_phy\":\ttrue,\n"
               "\t\"scan_2mbit_phy\":\ttrue,\n"
               "\t\"scan_channel_37\":\ttrue,\n"
               "\t\"scan_channel_38\":\ttrue,\n"
               "\t\"scan_channel_39\":\ttrue,\n"
               "\t\"scan_default\":\ttrue,\n"
               "\t\"scan_filter_allow_listed\":\tfalse,\n"
               "\t\"scan_filter_list\":\t[],\n"
               "\t\"coordinates\":\t\"\",\n"
               "\t\"fw_update_url\":\t\"https://network.ruuvi.com/firmwareupdate\"\n"
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
               "\t\"storage\":\t{\n"
               "\t\t\"storage_ready\":\tfalse\n"
               "\t},\n"
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
               "\t\"remote_cfg_use_ssl_client_cert\":\tfalse,\n"
               "\t\"remote_cfg_use_ssl_server_cert\":\tfalse,\n"
               "\t\"remote_cfg_refresh_interval_minutes\":\t0,\n"
               "\t\"use_http_ruuvi\":\ttrue,\n"
               "\t\"use_http\":\ttrue,\n"
               "\t\"http_url\":\t\"" RUUVI_GATEWAY_HTTP_DEFAULT_URL "\",\n"
               "\t\"http_data_format\":\t\"ruuvi\",\n"
               "\t\"http_auth\":\t\"none\",\n"
               "\t\"use_http_stat\":\ttrue,\n"
               "\t\"http_stat_url\":\t\"" RUUVI_GATEWAY_HTTP_STATUS_URL "\",\n"
               "\t\"http_stat_user\":\t\"\",\n"
               "\t\"http_stat_use_ssl_client_cert\":\tfalse,\n"
               "\t\"http_stat_use_ssl_server_cert\":\tfalse,\n"
               "\t\"use_mqtt\":\tfalse,\n"
               "\t\"mqtt_disable_retained_messages\":\tfalse,\n"
               "\t\"mqtt_transport\":\t\"TCP\",\n"
               "\t\"mqtt_data_format\":\t\"ruuvi_raw\",\n"
               "\t\"mqtt_server\":\t\"test.mosquitto.org\",\n"
               "\t\"mqtt_port\":\t1883,\n"
               "\t\"mqtt_sending_interval\":\t0,\n"
               "\t\"mqtt_prefix\":\t\"ruuvi/AA:BB:CC:DD:EE:FF/\",\n"
               "\t\"mqtt_client_id\":\t\"AA:BB:CC:DD:EE:FF\",\n"
               "\t\"mqtt_user\":\t\"\",\n"
               "\t\"mqtt_use_ssl_client_cert\":\tfalse,\n"
               "\t\"mqtt_use_ssl_server_cert\":\tfalse,\n"
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
               "\t\"ntp_server3\":\t\"pool.ntp.org\",\n"
               "\t\"ntp_server4\":\t\"time.ruuvi.com\",\n"
               "\t\"company_id\":\t1177,\n"
               "\t\"company_use_filtering\":\ttrue,\n"
               "\t\"scan_coded_phy\":\tfalse,\n"
               "\t\"scan_1mbit_phy\":\ttrue,\n"
               "\t\"scan_2mbit_phy\":\ttrue,\n"
               "\t\"scan_channel_37\":\ttrue,\n"
               "\t\"scan_channel_38\":\ttrue,\n"
               "\t\"scan_channel_39\":\ttrue,\n"
               "\t\"scan_default\":\ttrue,\n"
               "\t\"scan_filter_allow_listed\":\tfalse,\n"
               "\t\"scan_filter_list\":\t[],\n"
               "\t\"coordinates\":\t\"\",\n"
               "\t\"fw_update_url\":\t\"https://network.ruuvi.com/firmwareupdate\"\n"
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
               "\t\"storage\":\t{\n"
               "\t\t\"storage_ready\":\tfalse\n"
               "\t},\n"
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
               "\t\"remote_cfg_use_ssl_client_cert\":\tfalse,\n"
               "\t\"remote_cfg_use_ssl_server_cert\":\tfalse,\n"
               "\t\"remote_cfg_refresh_interval_minutes\":\t0,\n"
               "\t\"use_http_ruuvi\":\ttrue,\n"
               "\t\"use_http\":\ttrue,\n"
               "\t\"http_url\":\t\"" RUUVI_GATEWAY_HTTP_DEFAULT_URL "\",\n"
               "\t\"http_data_format\":\t\"ruuvi\",\n"
               "\t\"http_auth\":\t\"none\",\n"
               "\t\"use_http_stat\":\ttrue,\n"
               "\t\"http_stat_url\":\t\"" RUUVI_GATEWAY_HTTP_STATUS_URL "\",\n"
               "\t\"http_stat_user\":\t\"\",\n"
               "\t\"http_stat_use_ssl_client_cert\":\tfalse,\n"
               "\t\"http_stat_use_ssl_server_cert\":\tfalse,\n"
               "\t\"use_mqtt\":\tfalse,\n"
               "\t\"mqtt_disable_retained_messages\":\tfalse,\n"
               "\t\"mqtt_transport\":\t\"TCP\",\n"
               "\t\"mqtt_data_format\":\t\"ruuvi_raw\",\n"
               "\t\"mqtt_server\":\t\"test.mosquitto.org\",\n"
               "\t\"mqtt_port\":\t1883,\n"
               "\t\"mqtt_sending_interval\":\t0,\n"
               "\t\"mqtt_prefix\":\t\"ruuvi/AA:BB:CC:DD:EE:FF/\",\n"
               "\t\"mqtt_client_id\":\t\"AA:BB:CC:DD:EE:FF\",\n"
               "\t\"mqtt_user\":\t\"\",\n"
               "\t\"mqtt_use_ssl_client_cert\":\tfalse,\n"
               "\t\"mqtt_use_ssl_server_cert\":\tfalse,\n"
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
               "\t\"ntp_server3\":\t\"pool.ntp.org\",\n"
               "\t\"ntp_server4\":\t\"time.ruuvi.com\",\n"
               "\t\"company_id\":\t1177,\n"
               "\t\"company_use_filtering\":\ttrue,\n"
               "\t\"scan_coded_phy\":\tfalse,\n"
               "\t\"scan_1mbit_phy\":\ttrue,\n"
               "\t\"scan_2mbit_phy\":\ttrue,\n"
               "\t\"scan_channel_37\":\ttrue,\n"
               "\t\"scan_channel_38\":\ttrue,\n"
               "\t\"scan_channel_39\":\ttrue,\n"
               "\t\"scan_default\":\ttrue,\n"
               "\t\"scan_filter_allow_listed\":\tfalse,\n"
               "\t\"scan_filter_list\":\t[],\n"
               "\t\"coordinates\":\t\"\",\n"
               "\t\"fw_update_url\":\t\"https://network.ruuvi.com/firmwareupdate\"\n"
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
               "\t\"storage\":\t{\n"
               "\t\t\"storage_ready\":\tfalse\n"
               "\t},\n"
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
               "\t\"remote_cfg_use_ssl_client_cert\":\tfalse,\n"
               "\t\"remote_cfg_use_ssl_server_cert\":\tfalse,\n"
               "\t\"remote_cfg_refresh_interval_minutes\":\t0,\n"
               "\t\"use_http_ruuvi\":\ttrue,\n"
               "\t\"use_http\":\ttrue,\n"
               "\t\"http_url\":\t\"" RUUVI_GATEWAY_HTTP_DEFAULT_URL "\",\n"
               "\t\"http_data_format\":\t\"ruuvi\",\n"
               "\t\"http_auth\":\t\"none\",\n"
               "\t\"use_http_stat\":\ttrue,\n"
               "\t\"http_stat_url\":\t\"" RUUVI_GATEWAY_HTTP_STATUS_URL "\",\n"
               "\t\"http_stat_user\":\t\"\",\n"
               "\t\"http_stat_use_ssl_client_cert\":\tfalse,\n"
               "\t\"http_stat_use_ssl_server_cert\":\tfalse,\n"
               "\t\"use_mqtt\":\tfalse,\n"
               "\t\"mqtt_disable_retained_messages\":\tfalse,\n"
               "\t\"mqtt_transport\":\t\"TCP\",\n"
               "\t\"mqtt_data_format\":\t\"ruuvi_raw\",\n"
               "\t\"mqtt_server\":\t\"test.mosquitto.org\",\n"
               "\t\"mqtt_port\":\t1883,\n"
               "\t\"mqtt_sending_interval\":\t0,\n"
               "\t\"mqtt_prefix\":\t\"ruuvi/AA:BB:CC:DD:EE:FF/\",\n"
               "\t\"mqtt_client_id\":\t\"AA:BB:CC:DD:EE:FF\",\n"
               "\t\"mqtt_user\":\t\"\",\n"
               "\t\"mqtt_use_ssl_client_cert\":\tfalse,\n"
               "\t\"mqtt_use_ssl_server_cert\":\tfalse,\n"
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
               "\t\"ntp_server3\":\t\"pool.ntp.org\",\n"
               "\t\"ntp_server4\":\t\"time.ruuvi.com\",\n"
               "\t\"company_id\":\t1177,\n"
               "\t\"company_use_filtering\":\ttrue,\n"
               "\t\"scan_coded_phy\":\tfalse,\n"
               "\t\"scan_1mbit_phy\":\ttrue,\n"
               "\t\"scan_2mbit_phy\":\ttrue,\n"
               "\t\"scan_channel_37\":\ttrue,\n"
               "\t\"scan_channel_38\":\ttrue,\n"
               "\t\"scan_channel_39\":\ttrue,\n"
               "\t\"scan_default\":\ttrue,\n"
               "\t\"scan_filter_allow_listed\":\tfalse,\n"
               "\t\"scan_filter_list\":\t[],\n"
               "\t\"coordinates\":\t\"\",\n"
               "\t\"fw_update_url\":\t\"https://network.ruuvi.com/firmwareupdate\"\n"
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
               "\t\"storage\":\t{\n"
               "\t\t\"storage_ready\":\tfalse\n"
               "\t},\n"
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
               "\t\"remote_cfg_use_ssl_client_cert\":\tfalse,\n"
               "\t\"remote_cfg_use_ssl_server_cert\":\tfalse,\n"
               "\t\"remote_cfg_refresh_interval_minutes\":\t0,\n"
               "\t\"use_http_ruuvi\":\ttrue,\n"
               "\t\"use_http\":\ttrue,\n"
               "\t\"http_url\":\t\"" RUUVI_GATEWAY_HTTP_DEFAULT_URL "\",\n"
               "\t\"http_data_format\":\t\"ruuvi\",\n"
               "\t\"http_auth\":\t\"none\",\n"
               "\t\"use_http_stat\":\ttrue,\n"
               "\t\"http_stat_url\":\t\"" RUUVI_GATEWAY_HTTP_STATUS_URL "\",\n"
               "\t\"http_stat_user\":\t\"\",\n"
               "\t\"http_stat_use_ssl_client_cert\":\tfalse,\n"
               "\t\"http_stat_use_ssl_server_cert\":\tfalse,\n"
               "\t\"use_mqtt\":\tfalse,\n"
               "\t\"mqtt_disable_retained_messages\":\tfalse,\n"
               "\t\"mqtt_transport\":\t\"TCP\",\n"
               "\t\"mqtt_data_format\":\t\"ruuvi_raw\",\n"
               "\t\"mqtt_server\":\t\"test.mosquitto.org\",\n"
               "\t\"mqtt_port\":\t1883,\n"
               "\t\"mqtt_sending_interval\":\t0,\n"
               "\t\"mqtt_prefix\":\t\"ruuvi/AA:BB:CC:DD:EE:FF/\",\n"
               "\t\"mqtt_client_id\":\t\"AA:BB:CC:DD:EE:FF\",\n"
               "\t\"mqtt_user\":\t\"\",\n"
               "\t\"mqtt_use_ssl_client_cert\":\tfalse,\n"
               "\t\"mqtt_use_ssl_server_cert\":\tfalse,\n"
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
               "\t\"ntp_server3\":\t\"pool.ntp.org\",\n"
               "\t\"ntp_server4\":\t\"time.ruuvi.com\",\n"
               "\t\"company_id\":\t1177,\n"
               "\t\"company_use_filtering\":\ttrue,\n"
               "\t\"scan_coded_phy\":\tfalse,\n"
               "\t\"scan_1mbit_phy\":\ttrue,\n"
               "\t\"scan_2mbit_phy\":\ttrue,\n"
               "\t\"scan_channel_37\":\ttrue,\n"
               "\t\"scan_channel_38\":\ttrue,\n"
               "\t\"scan_channel_39\":\ttrue,\n"
               "\t\"scan_default\":\ttrue,\n"
               "\t\"scan_filter_allow_listed\":\tfalse,\n"
               "\t\"scan_filter_list\":\t[],\n"
               "\t\"coordinates\":\t\"\",\n"
               "\t\"fw_update_url\":\t\"https://network.ruuvi.com/firmwareupdate\"\n"
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
               "\t\"storage\":\t{\n"
               "\t\t\"storage_ready\":\tfalse\n"
               "\t},\n"
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
               "\t\"remote_cfg_use_ssl_client_cert\":\tfalse,\n"
               "\t\"remote_cfg_use_ssl_server_cert\":\tfalse,\n"
               "\t\"remote_cfg_refresh_interval_minutes\":\t0,\n"
               "\t\"use_http_ruuvi\":\ttrue,\n"
               "\t\"use_http\":\ttrue,\n"
               "\t\"http_url\":\t\"" RUUVI_GATEWAY_HTTP_DEFAULT_URL "\",\n"
               "\t\"http_data_format\":\t\"ruuvi\",\n"
               "\t\"http_auth\":\t\"none\",\n"
               "\t\"use_http_stat\":\ttrue,\n"
               "\t\"http_stat_url\":\t\"" RUUVI_GATEWAY_HTTP_STATUS_URL "\",\n"
               "\t\"http_stat_user\":\t\"\",\n"
               "\t\"http_stat_use_ssl_client_cert\":\tfalse,\n"
               "\t\"http_stat_use_ssl_server_cert\":\tfalse,\n"
               "\t\"use_mqtt\":\tfalse,\n"
               "\t\"mqtt_disable_retained_messages\":\tfalse,\n"
               "\t\"mqtt_transport\":\t\"TCP\",\n"
               "\t\"mqtt_data_format\":\t\"ruuvi_raw\",\n"
               "\t\"mqtt_server\":\t\"test.mosquitto.org\",\n"
               "\t\"mqtt_port\":\t1883,\n"
               "\t\"mqtt_sending_interval\":\t0,\n"
               "\t\"mqtt_prefix\":\t\"ruuvi/AA:BB:CC:DD:EE:FF/\",\n"
               "\t\"mqtt_client_id\":\t\"AA:BB:CC:DD:EE:FF\",\n"
               "\t\"mqtt_user\":\t\"\",\n"
               "\t\"mqtt_use_ssl_client_cert\":\tfalse,\n"
               "\t\"mqtt_use_ssl_server_cert\":\tfalse,\n"
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
               "\t\"ntp_server3\":\t\"pool.ntp.org\",\n"
               "\t\"ntp_server4\":\t\"time.ruuvi.com\",\n"
               "\t\"company_id\":\t1177,\n"
               "\t\"company_use_filtering\":\ttrue,\n"
               "\t\"scan_coded_phy\":\tfalse,\n"
               "\t\"scan_1mbit_phy\":\ttrue,\n"
               "\t\"scan_2mbit_phy\":\ttrue,\n"
               "\t\"scan_channel_37\":\ttrue,\n"
               "\t\"scan_channel_38\":\ttrue,\n"
               "\t\"scan_channel_39\":\ttrue,\n"
               "\t\"scan_default\":\ttrue,\n"
               "\t\"scan_filter_allow_listed\":\tfalse,\n"
               "\t\"scan_filter_list\":\t[],\n"
               "\t\"coordinates\":\t\"\",\n"
               "\t\"fw_update_url\":\t\"https://network.ruuvi.com/firmwareupdate\"\n"
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
               "\t\"storage\":\t{\n"
               "\t\t\"storage_ready\":\tfalse\n"
               "\t},\n"
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
               "\t\"remote_cfg_use_ssl_client_cert\":\tfalse,\n"
               "\t\"remote_cfg_use_ssl_server_cert\":\tfalse,\n"
               "\t\"remote_cfg_refresh_interval_minutes\":\t0,\n"
               "\t\"use_http_ruuvi\":\ttrue,\n"
               "\t\"use_http\":\ttrue,\n"
               "\t\"http_url\":\t\"" RUUVI_GATEWAY_HTTP_DEFAULT_URL "\",\n"
               "\t\"http_data_format\":\t\"ruuvi\",\n"
               "\t\"http_auth\":\t\"none\",\n"
               "\t\"use_http_stat\":\ttrue,\n"
               "\t\"http_stat_url\":\t\"" RUUVI_GATEWAY_HTTP_STATUS_URL "\",\n"
               "\t\"http_stat_user\":\t\"\",\n"
               "\t\"http_stat_use_ssl_client_cert\":\tfalse,\n"
               "\t\"http_stat_use_ssl_server_cert\":\tfalse,\n"
               "\t\"use_mqtt\":\tfalse,\n"
               "\t\"mqtt_disable_retained_messages\":\tfalse,\n"
               "\t\"mqtt_transport\":\t\"TCP\",\n"
               "\t\"mqtt_data_format\":\t\"ruuvi_raw\",\n"
               "\t\"mqtt_server\":\t\"test.mosquitto.org\",\n"
               "\t\"mqtt_port\":\t1883,\n"
               "\t\"mqtt_sending_interval\":\t0,\n"
               "\t\"mqtt_prefix\":\t\"ruuvi/AA:BB:CC:DD:EE:FF/\",\n"
               "\t\"mqtt_client_id\":\t\"AA:BB:CC:DD:EE:FF\",\n"
               "\t\"mqtt_user\":\t\"\",\n"
               "\t\"mqtt_use_ssl_client_cert\":\tfalse,\n"
               "\t\"mqtt_use_ssl_server_cert\":\tfalse,\n"
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
               "\t\"ntp_server3\":\t\"pool.ntp.org\",\n"
               "\t\"ntp_server4\":\t\"time.ruuvi.com\",\n"
               "\t\"company_id\":\t1177,\n"
               "\t\"company_use_filtering\":\ttrue,\n"
               "\t\"scan_coded_phy\":\tfalse,\n"
               "\t\"scan_1mbit_phy\":\ttrue,\n"
               "\t\"scan_2mbit_phy\":\ttrue,\n"
               "\t\"scan_channel_37\":\ttrue,\n"
               "\t\"scan_channel_38\":\ttrue,\n"
               "\t\"scan_channel_39\":\ttrue,\n"
               "\t\"scan_default\":\ttrue,\n"
               "\t\"scan_filter_allow_listed\":\tfalse,\n"
               "\t\"scan_filter_list\":\t[],\n"
               "\t\"coordinates\":\t\"\",\n"
               "\t\"fw_update_url\":\t\"https://network.ruuvi.com/firmwareupdate\"\n"
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
                .http_use_ssl_client_cert = false,
                .http_use_ssl_server_cert = false,
                .http_use_extra_http_path = false,
                .http_use_extra_http_query = false,
                .http_use_extra_http_headers = false,
                .http_url = { "https://my_server1.com" },
                .http_period = 15,
                .data_format = GW_CFG_HTTP_DATA_FORMAT_RUUVI_RAW_AND_DECODED,
                .auth_type = GW_CFG_HTTP_AUTH_TYPE_BASIC,
                .auth = {
                    .auth_basic = {
                        .user = {"h_user1"},
                        .password = {"h_user1"},
                    },
                },
            },
            .http_stat = {
                .use_http_stat = true,
                .http_stat_use_ssl_client_cert = false,
                .http_stat_use_ssl_server_cert = false,
                .http_stat_url = { "https://my_server2.com/status" },
                .http_stat_user = { "h_user2" },
                .http_stat_pass = { "h_pass2" },
            },
            .mqtt = {
                .use_mqtt = true,
                .mqtt_disable_retained_messages = false,
                .use_ssl_client_cert = false,
                .use_ssl_server_cert = false,
                .mqtt_transport = { MQTT_TRANSPORT_SSL },
                .mqtt_server = { "ssl.test.mosquitto.org" },
                .mqtt_port = 1234,
                .mqtt_prefix = { "my_prefix" },
                .mqtt_client_id = {"my_client_id"},
                .mqtt_user = {"m_user1"},
                .mqtt_pass = {"m_pass1"},
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
                .scan_2mbit_phy = false,
                .scan_channel_37 = false,
                .scan_channel_38 = false,
                .scan_channel_39 = false,
                .scan_default = false,
            },
            .scan_filter = {
                .scan_filter_allow_listed = false,
                .scan_filter_length = 0,
                .scan_filter_list = {0},
            },
            .coordinates = { { 'q', 'w', 'e', '\0' } },
            .fw_update = {
                .fw_update_url = { "https://myserver1.com/fw_update_info.json" },
            },
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
               "\t\"storage\":\t{\n"
               "\t\t\"storage_ready\":\tfalse\n"
               "\t},\n"
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
               "\t\"remote_cfg_use_ssl_client_cert\":\tfalse,\n"
               "\t\"remote_cfg_use_ssl_server_cert\":\tfalse,\n"
               "\t\"remote_cfg_refresh_interval_minutes\":\t0,\n"
               "\t\"use_http_ruuvi\":\tfalse,\n"
               "\t\"use_http\":\ttrue,\n"
               "\t\"http_use_ssl_client_cert\":\tfalse,\n"
               "\t\"http_use_ssl_server_cert\":\tfalse,\n"
               "\t\"http_use_extra_http_path\":\tfalse,\n"
               "\t\"http_use_extra_http_query\":\tfalse,\n"
               "\t\"http_use_extra_http_headers\":\tfalse,\n"
               "\t\"http_url\":\t\"https://my_server1.com\",\n"
               "\t\"http_period\":\t15,\n"
               "\t\"http_data_format\":\t\"ruuvi_raw_and_decoded\",\n"
               "\t\"http_auth\":\t\"basic\",\n"
               "\t\"http_user\":\t\"h_user1\",\n"
               "\t\"use_http_stat\":\ttrue,\n"
               "\t\"http_stat_url\":\t\"https://my_server2.com/status\",\n"
               "\t\"http_stat_user\":\t\"h_user2\",\n"
               "\t\"http_stat_use_ssl_client_cert\":\tfalse,\n"
               "\t\"http_stat_use_ssl_server_cert\":\tfalse,\n"
               "\t\"use_mqtt\":\ttrue,\n"
               "\t\"mqtt_disable_retained_messages\":\tfalse,\n"
               "\t\"mqtt_transport\":\t\"SSL\",\n"
               "\t\"mqtt_data_format\":\t\"ruuvi_raw\",\n"
               "\t\"mqtt_server\":\t\"ssl.test.mosquitto.org\",\n"
               "\t\"mqtt_port\":\t1234,\n"
               "\t\"mqtt_sending_interval\":\t0,\n"
               "\t\"mqtt_prefix\":\t\"my_prefix\",\n"
               "\t\"mqtt_client_id\":\t\"my_client_id\",\n"
               "\t\"mqtt_user\":\t\"m_user1\",\n"
               "\t\"mqtt_use_ssl_client_cert\":\tfalse,\n"
               "\t\"mqtt_use_ssl_server_cert\":\tfalse,\n"
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
               "\t\"scan_2mbit_phy\":\tfalse,\n"
               "\t\"scan_channel_37\":\tfalse,\n"
               "\t\"scan_channel_38\":\tfalse,\n"
               "\t\"scan_channel_39\":\tfalse,\n"
               "\t\"scan_default\":\tfalse,\n"
               "\t\"scan_filter_allow_listed\":\tfalse,\n"
               "\t\"scan_filter_list\":\t[],\n"
               "\t\"coordinates\":\t\"qwe\",\n"
               "\t\"fw_update_url\":\t\"https://myserver1.com/fw_update_info.json\"\n"
               "}"),
        string(json_str.p_str));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    cjson_wrap_free_json_str(&json_str);
}

TEST_F(TestGwCfgRuuviJsonGenerate, gw_cfg_default_ruuvi_json_generate_malloc_failures) // NOLINT
{
    static const string expected_json
        = "{\n"
          "\t\"fw_ver\":\t\"v1.3.3\",\n"
          "\t\"nrf52_fw_ver\":\t\"v0.7.1\",\n"
          "\t\"gw_mac\":\t\"AA:BB:CC:DD:EE:FF\",\n"
          "\t\"storage\":\t{\n"
          "\t\t\"storage_ready\":\tfalse\n"
          "\t},\n"
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
          "\t\"remote_cfg_use_ssl_client_cert\":\tfalse,\n"
          "\t\"remote_cfg_use_ssl_server_cert\":\tfalse,\n"
          "\t\"remote_cfg_refresh_interval_minutes\":\t0,\n"
          "\t\"use_http_ruuvi\":\ttrue,\n"
          "\t\"use_http\":\ttrue,\n"
          "\t\"http_url\":\t\"https://network.ruuvi.com/record\",\n"
          "\t\"http_data_format\":\t\"ruuvi\",\n"
          "\t\"http_auth\":\t\"none\",\n"
          "\t\"use_http_stat\":\ttrue,\n"
          "\t\"http_stat_url\":\t\"https://network.ruuvi.com/status\",\n"
          "\t\"http_stat_user\":\t\"\",\n"
          "\t\"http_stat_use_ssl_client_cert\":\tfalse,\n"
          "\t\"http_stat_use_ssl_server_cert\":\tfalse,\n"
          "\t\"use_mqtt\":\tfalse,\n"
          "\t\"mqtt_disable_retained_messages\":\tfalse,\n"
          "\t\"mqtt_transport\":\t\"TCP\",\n"
          "\t\"mqtt_data_format\":\t\"ruuvi_raw\",\n"
          "\t\"mqtt_server\":\t\"test.mosquitto.org\",\n"
          "\t\"mqtt_port\":\t1883,\n"
          "\t\"mqtt_sending_interval\":\t0,\n"
          "\t\"mqtt_prefix\":\t\"ruuvi/AA:BB:CC:DD:EE:FF/\",\n"
          "\t\"mqtt_client_id\":\t\"AA:BB:CC:DD:EE:FF\",\n"
          "\t\"mqtt_user\":\t\"\",\n"
          "\t\"mqtt_use_ssl_client_cert\":\tfalse,\n"
          "\t\"mqtt_use_ssl_server_cert\":\tfalse,\n"
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
          "\t\"ntp_server3\":\t\"pool.ntp.org\",\n"
          "\t\"ntp_server4\":\t\"time.ruuvi.com\",\n"
          "\t\"company_id\":\t1177,\n"
          "\t\"company_use_filtering\":\ttrue,\n"
          "\t\"scan_coded_phy\":\tfalse,\n"
          "\t\"scan_1mbit_phy\":\ttrue,\n"
          "\t\"scan_2mbit_phy\":\ttrue,\n"
          "\t\"scan_channel_37\":\ttrue,\n"
          "\t\"scan_channel_38\":\ttrue,\n"
          "\t\"scan_channel_39\":\ttrue,\n"
          "\t\"scan_default\":\ttrue,\n"
          "\t\"scan_filter_allow_listed\":\tfalse,\n"
          "\t\"scan_filter_list\":\t[],\n"
          "\t\"coordinates\":\t\"\",\n"
          "\t\"fw_update_url\":\t\"https://network.ruuvi.com/firmwareupdate\"\n"
          "}";
    static const string expected_logs[] = { "E gw_cfg: Can't create json object\n",
                                            "E gw_cfg: Can't add json item: fw_ver\n",
                                            "E gw_cfg: Can't add json item: fw_ver\n",
                                            "E gw_cfg: Can't add json item: fw_ver\n",
                                            "E gw_cfg: Can't add json item: nrf52_fw_ver\n",
                                            "E gw_cfg: Can't add json item: nrf52_fw_ver\n",
                                            "E gw_cfg: Can't add json item: nrf52_fw_ver\n",
                                            "E gw_cfg: Can't add json item: gw_mac\n",
                                            "E gw_cfg: Can't add json item: gw_mac\n",
                                            "E gw_cfg: Can't add json item: gw_mac\n",
                                            "E gw_cfg: Can't add json item: storage\n",
                                            "E gw_cfg: Can't add json item: storage\n",
                                            "E gw_cfg: Can't add json item: storage_ready\n",
                                            "E gw_cfg: Can't add json item: storage_ready\n",
                                            "E gw_cfg: Can't add json item: wifi_sta_config\n",
                                            "E gw_cfg: Can't add json item: wifi_sta_config\n",
                                            "E gw_cfg: Can't add json item: ssid\n",
                                            "E gw_cfg: Can't add json item: ssid\n",
                                            "E gw_cfg: Can't add json item: ssid\n",
                                            "E gw_cfg: Can't add json item: wifi_ap_config\n",
                                            "E gw_cfg: Can't add json item: wifi_ap_config\n",
                                            "E gw_cfg: Can't add json item: channel\n",
                                            "E gw_cfg: Can't add json item: channel\n",
                                            "E gw_cfg: Can't add json item: use_eth\n",
                                            "E gw_cfg: Can't add json item: use_eth\n",
                                            "E gw_cfg: Can't add json item: eth_dhcp\n",
                                            "E gw_cfg: Can't add json item: eth_dhcp\n",
                                            "E gw_cfg: Can't add json item: eth_static_ip\n",
                                            "E gw_cfg: Can't add json item: eth_static_ip\n",
                                            "E gw_cfg: Can't add json item: eth_static_ip\n",
                                            "E gw_cfg: Can't add json item: eth_netmask\n",
                                            "E gw_cfg: Can't add json item: eth_netmask\n",
                                            "E gw_cfg: Can't add json item: eth_netmask\n",
                                            "E gw_cfg: Can't add json item: eth_gw\n",
                                            "E gw_cfg: Can't add json item: eth_gw\n",
                                            "E gw_cfg: Can't add json item: eth_gw\n",
                                            "E gw_cfg: Can't add json item: eth_dns1\n",
                                            "E gw_cfg: Can't add json item: eth_dns1\n",
                                            "E gw_cfg: Can't add json item: eth_dns1\n",
                                            "E gw_cfg: Can't add json item: eth_dns2\n",
                                            "E gw_cfg: Can't add json item: eth_dns2\n",
                                            "E gw_cfg: Can't add json item: eth_dns2\n",
                                            "E gw_cfg: Can't add json item: remote_cfg_use\n",
                                            "E gw_cfg: Can't add json item: remote_cfg_use\n",
                                            "E gw_cfg: Can't add json item: remote_cfg_url\n",
                                            "E gw_cfg: Can't add json item: remote_cfg_url\n",
                                            "E gw_cfg: Can't add json item: remote_cfg_url\n",
                                            "E gw_cfg: Can't add json item: remote_cfg_auth_type\n",
                                            "E gw_cfg: Can't add json item: remote_cfg_auth_type\n",
                                            "E gw_cfg: Can't add json item: remote_cfg_auth_type\n",
                                            "E gw_cfg: Can't add json item: remote_cfg_use_ssl_client_cert\n",
                                            "E gw_cfg: Can't add json item: remote_cfg_use_ssl_client_cert\n",
                                            "E gw_cfg: Can't add json item: remote_cfg_use_ssl_server_cert\n",
                                            "E gw_cfg: Can't add json item: remote_cfg_use_ssl_server_cert\n",
                                            "E gw_cfg: Can't add json item: remote_cfg_refresh_interval_minutes\n",
                                            "E gw_cfg: Can't add json item: remote_cfg_refresh_interval_minutes\n",
                                            "E gw_cfg: Can't add json item: use_http_ruuvi\n",
                                            "E gw_cfg: Can't add json item: use_http_ruuvi\n",
                                            "E gw_cfg: Can't add json item: use_http\n",
                                            "E gw_cfg: Can't add json item: use_http\n",
                                            "E gw_cfg: Can't add json item: http_url\n",
                                            "E gw_cfg: Can't add json item: http_url\n",
                                            "E gw_cfg: Can't add json item: http_url\n",
                                            "E gw_cfg: Can't add json item: http_data_format\n",
                                            "E gw_cfg: Can't add json item: http_data_format\n",
                                            "E gw_cfg: Can't add json item: http_data_format\n",
                                            "E gw_cfg: Can't add json item: http_auth\n",
                                            "E gw_cfg: Can't add json item: http_auth\n",
                                            "E gw_cfg: Can't add json item: http_auth\n",
                                            "E gw_cfg: Can't add json item: use_http_stat\n",
                                            "E gw_cfg: Can't add json item: use_http_stat\n",
                                            "E gw_cfg: Can't add json item: http_stat_url\n",
                                            "E gw_cfg: Can't add json item: http_stat_url\n",
                                            "E gw_cfg: Can't add json item: http_stat_url\n",
                                            "E gw_cfg: Can't add json item: http_stat_user\n",
                                            "E gw_cfg: Can't add json item: http_stat_user\n",
                                            "E gw_cfg: Can't add json item: http_stat_user\n",
                                            "E gw_cfg: Can't add json item: http_stat_use_ssl_client_cert\n",
                                            "E gw_cfg: Can't add json item: http_stat_use_ssl_client_cert\n",
                                            "E gw_cfg: Can't add json item: http_stat_use_ssl_server_cert\n",
                                            "E gw_cfg: Can't add json item: http_stat_use_ssl_server_cert\n",
                                            "E gw_cfg: Can't add json item: use_mqtt\n",
                                            "E gw_cfg: Can't add json item: use_mqtt\n",
                                            "E gw_cfg: Can't add json item: mqtt_disable_retained_messages\n",
                                            "E gw_cfg: Can't add json item: mqtt_disable_retained_messages\n",
                                            "E gw_cfg: Can't add json item: mqtt_transport\n",
                                            "E gw_cfg: Can't add json item: mqtt_transport\n",
                                            "E gw_cfg: Can't add json item: mqtt_transport\n",
                                            "E gw_cfg: Can't add json item: mqtt_data_format\n",
                                            "E gw_cfg: Can't add json item: mqtt_data_format\n",
                                            "E gw_cfg: Can't add json item: mqtt_data_format\n",
                                            "E gw_cfg: Can't add json item: mqtt_server\n",
                                            "E gw_cfg: Can't add json item: mqtt_server\n",
                                            "E gw_cfg: Can't add json item: mqtt_server\n",
                                            "E gw_cfg: Can't add json item: mqtt_port\n",
                                            "E gw_cfg: Can't add json item: mqtt_port\n",
                                            "E gw_cfg: Can't add json item: mqtt_sending_interval\n",
                                            "E gw_cfg: Can't add json item: mqtt_sending_interval\n",
                                            "E gw_cfg: Can't add json item: mqtt_prefix\n",
                                            "E gw_cfg: Can't add json item: mqtt_prefix\n",
                                            "E gw_cfg: Can't add json item: mqtt_prefix\n",
                                            "E gw_cfg: Can't add json item: mqtt_client_id\n",
                                            "E gw_cfg: Can't add json item: mqtt_client_id\n",
                                            "E gw_cfg: Can't add json item: mqtt_client_id\n",
                                            "E gw_cfg: Can't add json item: mqtt_user\n",
                                            "E gw_cfg: Can't add json item: mqtt_user\n",
                                            "E gw_cfg: Can't add json item: mqtt_user\n",
                                            "E gw_cfg: Can't add json item: mqtt_use_ssl_client_cert\n",
                                            "E gw_cfg: Can't add json item: mqtt_use_ssl_client_cert\n",
                                            "E gw_cfg: Can't add json item: mqtt_use_ssl_server_cert\n",
                                            "E gw_cfg: Can't add json item: mqtt_use_ssl_server_cert\n",
                                            "E gw_cfg: Can't add json item: lan_auth_type\n",
                                            "E gw_cfg: Can't add json item: lan_auth_type\n",
                                            "E gw_cfg: Can't add json item: lan_auth_type\n",
                                            "E gw_cfg: Can't add json item: lan_auth_user\n",
                                            "E gw_cfg: Can't add json item: lan_auth_user\n",
                                            "E gw_cfg: Can't add json item: lan_auth_user\n",
                                            "E gw_cfg: Can't add json item: lan_auth_api_key_use\n",
                                            "E gw_cfg: Can't add json item: lan_auth_api_key_use\n",
                                            "E gw_cfg: Can't add json item: lan_auth_api_key_rw_use\n",
                                            "E gw_cfg: Can't add json item: lan_auth_api_key_rw_use\n",
                                            "E gw_cfg: Can't add json item: auto_update_cycle\n",
                                            "E gw_cfg: Can't add json item: auto_update_cycle\n",
                                            "E gw_cfg: Can't add json item: auto_update_cycle\n",
                                            "E gw_cfg: Can't add json item: auto_update_weekdays_bitmask\n",
                                            "E gw_cfg: Can't add json item: auto_update_weekdays_bitmask\n",
                                            "E gw_cfg: Can't add json item: auto_update_interval_from\n",
                                            "E gw_cfg: Can't add json item: auto_update_interval_from\n",
                                            "E gw_cfg: Can't add json item: auto_update_interval_to\n",
                                            "E gw_cfg: Can't add json item: auto_update_interval_to\n",
                                            "E gw_cfg: Can't add json item: auto_update_tz_offset_hours\n",
                                            "E gw_cfg: Can't add json item: auto_update_tz_offset_hours\n",
                                            "E gw_cfg: Can't add json item: ntp_use\n",
                                            "E gw_cfg: Can't add json item: ntp_use\n",
                                            "E gw_cfg: Can't add json item: ntp_use_dhcp\n",
                                            "E gw_cfg: Can't add json item: ntp_use_dhcp\n",
                                            "E gw_cfg: Can't add json item: ntp_server1\n",
                                            "E gw_cfg: Can't add json item: ntp_server1\n",
                                            "E gw_cfg: Can't add json item: ntp_server1\n",
                                            "E gw_cfg: Can't add json item: ntp_server2\n",
                                            "E gw_cfg: Can't add json item: ntp_server2\n",
                                            "E gw_cfg: Can't add json item: ntp_server2\n",
                                            "E gw_cfg: Can't add json item: ntp_server3\n",
                                            "E gw_cfg: Can't add json item: ntp_server3\n",
                                            "E gw_cfg: Can't add json item: ntp_server3\n",
                                            "E gw_cfg: Can't add json item: ntp_server4\n",
                                            "E gw_cfg: Can't add json item: ntp_server4\n",
                                            "E gw_cfg: Can't add json item: ntp_server4\n",
                                            "E gw_cfg: Can't add json item: company_id\n",
                                            "E gw_cfg: Can't add json item: company_id\n",
                                            "E gw_cfg: Can't add json item: company_use_filtering\n",
                                            "E gw_cfg: Can't add json item: company_use_filtering\n",
                                            "E gw_cfg: Can't add json item: scan_coded_phy\n",
                                            "E gw_cfg: Can't add json item: scan_coded_phy\n",
                                            "E gw_cfg: Can't add json item: scan_1mbit_phy\n",
                                            "E gw_cfg: Can't add json item: scan_1mbit_phy\n",
                                            "E gw_cfg: Can't add json item: scan_2mbit_phy\n",
                                            "E gw_cfg: Can't add json item: scan_2mbit_phy\n",
                                            "E gw_cfg: Can't add json item: scan_channel_37\n",
                                            "E gw_cfg: Can't add json item: scan_channel_37\n",
                                            "E gw_cfg: Can't add json item: scan_channel_38\n",
                                            "E gw_cfg: Can't add json item: scan_channel_38\n",
                                            "E gw_cfg: Can't add json item: scan_channel_39\n",
                                            "E gw_cfg: Can't add json item: scan_channel_39\n",
                                            "E gw_cfg: Can't add json item: scan_default\n",
                                            "E gw_cfg: Can't add json item: scan_default\n",
                                            "E gw_cfg: Can't add json item: scan_filter_allow_listed\n",
                                            "E gw_cfg: Can't add json item: scan_filter_allow_listed\n",
                                            "E gw_cfg: Can't add json item: scan_filter_list\n",
                                            "E gw_cfg: Can't add json item: scan_filter_list\n",
                                            "E gw_cfg: Can't add json item: coordinates\n",
                                            "E gw_cfg: Can't add json item: coordinates\n",
                                            "E gw_cfg: Can't add json item: coordinates\n",
                                            "E gw_cfg: Can't add json item: fw_update_url\n",
                                            "E gw_cfg: Can't add json item: fw_update_url\n",
                                            "E gw_cfg: Can't add json item: fw_update_url\n",
                                            "E gw_cfg: Can't create json string\n",
                                            "E gw_cfg: Can't create json string\n",
                                            "E gw_cfg: Can't create json string\n",
                                            "E gw_cfg: Can't create json string\n",
                                            "E gw_cfg: Can't create json string\n",
                                            "" };

    cJSON_Hooks hooks = {
        .malloc_fn = &os_malloc,
        .free_fn   = &os_free_internal,
    };
    cJSON_InitHooks(&hooks);

    for (int32_t i = 0; i < std::size(expected_logs); ++i)
    {
        SCOPED_TRACE(::testing::Message() << "Simulate memory allocation failure on step: " << i);
        this->m_malloc_cnt         = 0;
        this->m_malloc_fail_on_cnt = i + 1;

        const gw_cfg_t   gw_cfg   = get_gateway_config_default();
        cjson_wrap_str_t json_str = cjson_wrap_str_null();

        if (expected_logs[i] != "")
        {
            ASSERT_FALSE(gw_cfg_ruuvi_json_generate(&gw_cfg, &json_str));
            ASSERT_EQ(nullptr, json_str.p_str);
            ASSERT_EQ(expected_logs[i], esp_log_wrapper_get_logs());
        }
        else
        {
            ASSERT_TRUE(gw_cfg_ruuvi_json_generate(&gw_cfg, &json_str));
            ASSERT_NE(nullptr, json_str.p_str);
            ASSERT_EQ(expected_json, string(json_str.p_str));
            cjson_wrap_free_json_str(&json_str);
        }
        ASSERT_TRUE(g_pTestClass->m_mem_alloc_trace.is_empty());
    }
}

TEST_F(TestGwCfgRuuviJsonGenerate, gw_cfg_non_default_ruuvi_json_generate_malloc_failures) // NOLINT
{
    static const string expected_json
        = "{\n"
          "\t\"fw_ver\":\t\"v1.3.3\",\n"
          "\t\"nrf52_fw_ver\":\t\"v0.7.1\",\n"
          "\t\"gw_mac\":\t\"AA:BB:CC:DD:EE:FF\",\n"
          "\t\"storage\":\t{\n"
          "\t\t\"storage_ready\":\tfalse\n"
          "\t},\n"
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
          "\t\"remote_cfg_use\":\ttrue,\n"
          "\t\"remote_cfg_url\":\t\"https://myserver2.com/api2\",\n"
          "\t\"remote_cfg_auth_type\":\t\"basic\",\n"
          "\t\"remote_cfg_auth_basic_user\":\t\"username1\",\n"
          "\t\"remote_cfg_use_ssl_client_cert\":\ttrue,\n"
          "\t\"remote_cfg_use_ssl_server_cert\":\ttrue,\n"
          "\t\"remote_cfg_refresh_interval_minutes\":\t0,\n"
          "\t\"use_http_ruuvi\":\ttrue,\n"
          "\t\"use_http\":\ttrue,\n"
          "\t\"http_use_ssl_client_cert\":\ttrue,\n"
          "\t\"http_use_ssl_server_cert\":\ttrue,\n"
          "\t\"http_use_extra_http_path\":\ttrue,\n"
          "\t\"http_use_extra_http_query\":\ttrue,\n"
          "\t\"http_use_extra_http_headers\":\ttrue,\n"
          "\t\"http_url\":\t\"https://myserver.com\",\n"
          "\t\"http_period\":\t10,\n"
          "\t\"http_data_format\":\t\"ruuvi\",\n"
          "\t\"http_auth\":\t\"bearer\",\n"
          "\t\"use_http_stat\":\ttrue,\n"
          "\t\"http_stat_url\":\t\"https://myserver3.com/stat\",\n"
          "\t\"http_stat_user\":\t\"username2\",\n"
          "\t\"http_stat_use_ssl_client_cert\":\ttrue,\n"
          "\t\"http_stat_use_ssl_server_cert\":\ttrue,\n"
          "\t\"use_mqtt\":\ttrue,\n"
          "\t\"mqtt_disable_retained_messages\":\tfalse,\n"
          "\t\"mqtt_transport\":\t\"WSS\",\n"
          "\t\"mqtt_data_format\":\t\"ruuvi_raw_and_decoded\",\n"
          "\t\"mqtt_server\":\t\"myserver4.com\",\n"
          "\t\"mqtt_port\":\t1234,\n"
          "\t\"mqtt_sending_interval\":\t60,\n"
          "\t\"mqtt_prefix\":\t\"mqtt_prefix1\",\n"
          "\t\"mqtt_client_id\":\t\"mqtt_client_id1\",\n"
          "\t\"mqtt_user\":\t\"mqtt_user1\",\n"
          "\t\"mqtt_use_ssl_client_cert\":\ttrue,\n"
          "\t\"mqtt_use_ssl_server_cert\":\ttrue,\n"
          "\t\"lan_auth_type\":\t\"lan_auth_basic\",\n"
          "\t\"lan_auth_user\":\t\"lan_user1\",\n"
          "\t\"lan_auth_api_key_use\":\ttrue,\n"
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
          "\t\"ntp_server3\":\t\"pool.ntp.org\",\n"
          "\t\"ntp_server4\":\t\"time.ruuvi.com\",\n"
          "\t\"company_id\":\t1177,\n"
          "\t\"company_use_filtering\":\ttrue,\n"
          "\t\"scan_coded_phy\":\tfalse,\n"
          "\t\"scan_1mbit_phy\":\ttrue,\n"
          "\t\"scan_2mbit_phy\":\ttrue,\n"
          "\t\"scan_channel_37\":\ttrue,\n"
          "\t\"scan_channel_38\":\ttrue,\n"
          "\t\"scan_channel_39\":\ttrue,\n"
          "\t\"scan_default\":\ttrue,\n"
          "\t\"scan_filter_allow_listed\":\ttrue,\n"
          "\t\"scan_filter_list\":\t[\"11:22:33:44:55:01\", \"11:22:33:44:55:02\"],\n"
          "\t\"coordinates\":\t\"coord1,coord2\",\n"
          "\t\"fw_update_url\":\t\"https://fwupdate.server1.com\"\n"
          "}";
    static const string expected_logs[] = {
        "E gw_cfg: Can't create json object\n",
        "E gw_cfg: Can't add json item: fw_ver\n",
        "E gw_cfg: Can't add json item: fw_ver\n",
        "E gw_cfg: Can't add json item: fw_ver\n",
        "E gw_cfg: Can't add json item: nrf52_fw_ver\n",
        "E gw_cfg: Can't add json item: nrf52_fw_ver\n",
        "E gw_cfg: Can't add json item: nrf52_fw_ver\n",
        "E gw_cfg: Can't add json item: gw_mac\n",
        "E gw_cfg: Can't add json item: gw_mac\n",
        "E gw_cfg: Can't add json item: gw_mac\n",
        "E gw_cfg: Can't add json item: storage\n",
        "E gw_cfg: Can't add json item: storage\n",
        "E gw_cfg: Can't add json item: storage_ready\n",
        "E gw_cfg: Can't add json item: storage_ready\n",
        "E gw_cfg: Can't add json item: wifi_sta_config\n",
        "E gw_cfg: Can't add json item: wifi_sta_config\n",
        "E gw_cfg: Can't add json item: ssid\n",
        "E gw_cfg: Can't add json item: ssid\n",
        "E gw_cfg: Can't add json item: ssid\n",
        "E gw_cfg: Can't add json item: wifi_ap_config\n",
        "E gw_cfg: Can't add json item: wifi_ap_config\n",
        "E gw_cfg: Can't add json item: channel\n",
        "E gw_cfg: Can't add json item: channel\n",
        "E gw_cfg: Can't add json item: use_eth\n",
        "E gw_cfg: Can't add json item: use_eth\n",
        "E gw_cfg: Can't add json item: eth_dhcp\n",
        "E gw_cfg: Can't add json item: eth_dhcp\n",
        "E gw_cfg: Can't add json item: eth_static_ip\n",
        "E gw_cfg: Can't add json item: eth_static_ip\n",
        "E gw_cfg: Can't add json item: eth_static_ip\n",
        "E gw_cfg: Can't add json item: eth_netmask\n",
        "E gw_cfg: Can't add json item: eth_netmask\n",
        "E gw_cfg: Can't add json item: eth_netmask\n",
        "E gw_cfg: Can't add json item: eth_gw\n",
        "E gw_cfg: Can't add json item: eth_gw\n",
        "E gw_cfg: Can't add json item: eth_gw\n",
        "E gw_cfg: Can't add json item: eth_dns1\n",
        "E gw_cfg: Can't add json item: eth_dns1\n",
        "E gw_cfg: Can't add json item: eth_dns1\n",
        "E gw_cfg: Can't add json item: eth_dns2\n",
        "E gw_cfg: Can't add json item: eth_dns2\n",
        "E gw_cfg: Can't add json item: eth_dns2\n",
        "E gw_cfg: Can't add json item: remote_cfg_use\n",
        "E gw_cfg: Can't add json item: remote_cfg_use\n",
        "E gw_cfg: Can't add json item: remote_cfg_url\n",
        "E gw_cfg: Can't add json item: remote_cfg_url\n",
        "E gw_cfg: Can't add json item: remote_cfg_url\n",
        "E gw_cfg: Can't add json item: remote_cfg_auth_type\n",
        "E gw_cfg: Can't add json item: remote_cfg_auth_type\n",
        "E gw_cfg: Can't add json item: remote_cfg_auth_type\n",
        "E gw_cfg: Can't add json item: remote_cfg_auth_basic_user\n",
        "E gw_cfg: Can't add json item: remote_cfg_auth_basic_user\n",
        "E gw_cfg: Can't add json item: remote_cfg_auth_basic_user\n",
        "E gw_cfg: Can't add json item: remote_cfg_use_ssl_client_cert\n",
        "E gw_cfg: Can't add json item: remote_cfg_use_ssl_client_cert\n",
        "E gw_cfg: Can't add json item: remote_cfg_use_ssl_server_cert\n",
        "E gw_cfg: Can't add json item: remote_cfg_use_ssl_server_cert\n",
        "E gw_cfg: Can't add json item: remote_cfg_refresh_interval_minutes\n",
        "E gw_cfg: Can't add json item: remote_cfg_refresh_interval_minutes\n",
        "E gw_cfg: Can't add json item: use_http_ruuvi\n",
        "E gw_cfg: Can't add json item: use_http_ruuvi\n",
        "E gw_cfg: Can't add json item: use_http\n",
        "E gw_cfg: Can't add json item: use_http\n",
        "E gw_cfg: Can't add json item: http_use_ssl_client_cert\n",
        "E gw_cfg: Can't add json item: http_use_ssl_client_cert\n",
        "E gw_cfg: Can't add json item: http_use_ssl_server_cert\n",
        "E gw_cfg: Can't add json item: http_use_ssl_server_cert\n",
        "E gw_cfg: Can't add json item: http_use_extra_http_path\n",
        "E gw_cfg: Can't add json item: http_use_extra_http_path\n",
        "E gw_cfg: Can't add json item: http_use_extra_http_query\n",
        "E gw_cfg: Can't add json item: http_use_extra_http_query\n",
        "E gw_cfg: Can't add json item: http_use_extra_http_headers\n",
        "E gw_cfg: Can't add json item: http_use_extra_http_headers\n",
        "E gw_cfg: Can't add json item: http_url\n",
        "E gw_cfg: Can't add json item: http_url\n",
        "E gw_cfg: Can't add json item: http_url\n",
        "E gw_cfg: Can't add json item: http_period\n",
        "E gw_cfg: Can't add json item: http_period\n",
        "E gw_cfg: Can't add json item: http_data_format\n",
        "E gw_cfg: Can't add json item: http_data_format\n",
        "E gw_cfg: Can't add json item: http_data_format\n",
        "E gw_cfg: Can't add json item: http_auth\n",
        "E gw_cfg: Can't add json item: http_auth\n",
        "E gw_cfg: Can't add json item: http_auth\n",
        "E gw_cfg: Can't add json item: use_http_stat\n",
        "E gw_cfg: Can't add json item: use_http_stat\n",
        "E gw_cfg: Can't add json item: http_stat_url\n",
        "E gw_cfg: Can't add json item: http_stat_url\n",
        "E gw_cfg: Can't add json item: http_stat_url\n",
        "E gw_cfg: Can't add json item: http_stat_user\n",
        "E gw_cfg: Can't add json item: http_stat_user\n",
        "E gw_cfg: Can't add json item: http_stat_user\n",
        "E gw_cfg: Can't add json item: http_stat_use_ssl_client_cert\n",
        "E gw_cfg: Can't add json item: http_stat_use_ssl_client_cert\n",
        "E gw_cfg: Can't add json item: http_stat_use_ssl_server_cert\n",
        "E gw_cfg: Can't add json item: http_stat_use_ssl_server_cert\n",
        "E gw_cfg: Can't add json item: use_mqtt\n",
        "E gw_cfg: Can't add json item: use_mqtt\n",
        "E gw_cfg: Can't add json item: mqtt_disable_retained_messages\n",
        "E gw_cfg: Can't add json item: mqtt_disable_retained_messages\n",
        "E gw_cfg: Can't add json item: mqtt_transport\n",
        "E gw_cfg: Can't add json item: mqtt_transport\n",
        "E gw_cfg: Can't add json item: mqtt_transport\n",
        "E gw_cfg: Can't add json item: mqtt_data_format\n",
        "E gw_cfg: Can't add json item: mqtt_data_format\n",
        "E gw_cfg: Can't add json item: mqtt_data_format\n",
        "E gw_cfg: Can't add json item: mqtt_server\n",
        "E gw_cfg: Can't add json item: mqtt_server\n",
        "E gw_cfg: Can't add json item: mqtt_server\n",
        "E gw_cfg: Can't add json item: mqtt_port\n",
        "E gw_cfg: Can't add json item: mqtt_port\n",
        "E gw_cfg: Can't add json item: mqtt_sending_interval\n",
        "E gw_cfg: Can't add json item: mqtt_sending_interval\n",
        "E gw_cfg: Can't add json item: mqtt_prefix\n",
        "E gw_cfg: Can't add json item: mqtt_prefix\n",
        "E gw_cfg: Can't add json item: mqtt_prefix\n",
        "E gw_cfg: Can't add json item: mqtt_client_id\n",
        "E gw_cfg: Can't add json item: mqtt_client_id\n",
        "E gw_cfg: Can't add json item: mqtt_client_id\n",
        "E gw_cfg: Can't add json item: mqtt_user\n",
        "E gw_cfg: Can't add json item: mqtt_user\n",
        "E gw_cfg: Can't add json item: mqtt_user\n",
        "E gw_cfg: Can't add json item: mqtt_use_ssl_client_cert\n",
        "E gw_cfg: Can't add json item: mqtt_use_ssl_client_cert\n",
        "E gw_cfg: Can't add json item: mqtt_use_ssl_server_cert\n",
        "E gw_cfg: Can't add json item: mqtt_use_ssl_server_cert\n",
        "E gw_cfg: Can't add json item: lan_auth_type\n",
        "E gw_cfg: Can't add json item: lan_auth_type\n",
        "E gw_cfg: Can't add json item: lan_auth_type\n",
        "E gw_cfg: Can't add json item: lan_auth_user\n",
        "E gw_cfg: Can't add json item: lan_auth_user\n",
        "E gw_cfg: Can't add json item: lan_auth_user\n",
        "E gw_cfg: Can't add json item: lan_auth_api_key_use\n",
        "E gw_cfg: Can't add json item: lan_auth_api_key_use\n",
        "E gw_cfg: Can't add json item: lan_auth_api_key_rw_use\n",
        "E gw_cfg: Can't add json item: lan_auth_api_key_rw_use\n",
        "E gw_cfg: Can't add json item: auto_update_cycle\n",
        "E gw_cfg: Can't add json item: auto_update_cycle\n",
        "E gw_cfg: Can't add json item: auto_update_cycle\n",
        "E gw_cfg: Can't add json item: auto_update_weekdays_bitmask\n",
        "E gw_cfg: Can't add json item: auto_update_weekdays_bitmask\n",
        "E gw_cfg: Can't add json item: auto_update_interval_from\n",
        "E gw_cfg: Can't add json item: auto_update_interval_from\n",
        "E gw_cfg: Can't add json item: auto_update_interval_to\n",
        "E gw_cfg: Can't add json item: auto_update_interval_to\n",
        "E gw_cfg: Can't add json item: auto_update_tz_offset_hours\n",
        "E gw_cfg: Can't add json item: auto_update_tz_offset_hours\n",
        "E gw_cfg: Can't add json item: ntp_use\n",
        "E gw_cfg: Can't add json item: ntp_use\n",
        "E gw_cfg: Can't add json item: ntp_use_dhcp\n",
        "E gw_cfg: Can't add json item: ntp_use_dhcp\n",
        "E gw_cfg: Can't add json item: ntp_server1\n",
        "E gw_cfg: Can't add json item: ntp_server1\n",
        "E gw_cfg: Can't add json item: ntp_server1\n",
        "E gw_cfg: Can't add json item: ntp_server2\n",
        "E gw_cfg: Can't add json item: ntp_server2\n",
        "E gw_cfg: Can't add json item: ntp_server2\n",
        "E gw_cfg: Can't add json item: ntp_server3\n",
        "E gw_cfg: Can't add json item: ntp_server3\n",
        "E gw_cfg: Can't add json item: ntp_server3\n",
        "E gw_cfg: Can't add json item: ntp_server4\n",
        "E gw_cfg: Can't add json item: ntp_server4\n",
        "E gw_cfg: Can't add json item: ntp_server4\n",
        "E gw_cfg: Can't add json item: company_id\n",
        "E gw_cfg: Can't add json item: company_id\n",
        "E gw_cfg: Can't add json item: company_use_filtering\n",
        "E gw_cfg: Can't add json item: company_use_filtering\n",
        "E gw_cfg: Can't add json item: scan_coded_phy\n",
        "E gw_cfg: Can't add json item: scan_coded_phy\n",
        "E gw_cfg: Can't add json item: scan_1mbit_phy\n",
        "E gw_cfg: Can't add json item: scan_1mbit_phy\n",
        "E gw_cfg: Can't add json item: scan_2mbit_phy\n",
        "E gw_cfg: Can't add json item: scan_2mbit_phy\n",
        "E gw_cfg: Can't add json item: scan_channel_37\n",
        "E gw_cfg: Can't add json item: scan_channel_37\n",
        "E gw_cfg: Can't add json item: scan_channel_38\n",
        "E gw_cfg: Can't add json item: scan_channel_38\n",
        "E gw_cfg: Can't add json item: scan_channel_39\n",
        "E gw_cfg: Can't add json item: scan_channel_39\n",
        "E gw_cfg: Can't add json item: scan_default\n",
        "E gw_cfg: Can't add json item: scan_default\n",
        "E gw_cfg: Can't add json item: scan_filter_allow_listed\n",
        "E gw_cfg: Can't add json item: scan_filter_allow_listed\n",
        "E gw_cfg: Can't add json item: scan_filter_list\n",
        "E gw_cfg: Can't add json item: scan_filter_list\n",
        "E gw_cfg: Can't add MAC addr to scan_filter_list array: 11:22:33:44:55:01\n",
        "E gw_cfg: Can't add MAC addr to scan_filter_list array: 11:22:33:44:55:01\n",
        "E gw_cfg: Can't add MAC addr to scan_filter_list array: 11:22:33:44:55:02\n",
        "E gw_cfg: Can't add MAC addr to scan_filter_list array: 11:22:33:44:55:02\n",
        "E gw_cfg: Can't add json item: coordinates\n",
        "E gw_cfg: Can't add json item: coordinates\n",
        "E gw_cfg: Can't add json item: coordinates\n",
        "E gw_cfg: Can't add json item: fw_update_url\n",
        "E gw_cfg: Can't add json item: fw_update_url\n",
        "E gw_cfg: Can't add json item: fw_update_url\n",
        "E gw_cfg: Can't create json string\n",
        "E gw_cfg: Can't create json string\n",
        "E gw_cfg: Can't create json string\n",
        "E gw_cfg: Can't create json string\n",
        "E gw_cfg: Can't create json string\n",
        "E gw_cfg: Can't create json string\n",
        ""
    };

    cJSON_Hooks hooks = {
        .malloc_fn = &os_malloc,
        .free_fn   = &os_free_internal,
    };
    cJSON_InitHooks(&hooks);

    gw_cfg_t gw_cfg                                   = get_gateway_config_default();
    gw_cfg.ruuvi_cfg.http.use_http                    = true;
    gw_cfg.ruuvi_cfg.http.http_use_ssl_client_cert    = true;
    gw_cfg.ruuvi_cfg.http.http_use_ssl_server_cert    = true;
    gw_cfg.ruuvi_cfg.http.http_use_extra_http_path    = true;
    gw_cfg.ruuvi_cfg.http.http_use_extra_http_query   = true;
    gw_cfg.ruuvi_cfg.http.http_use_extra_http_headers = true;
    gw_cfg.ruuvi_cfg.http.http_url                    = (ruuvi_gw_cfg_http_url_t) { .buf = "https://myserver.com" };
    gw_cfg.ruuvi_cfg.http.auth_type                   = GW_CFG_HTTP_AUTH_TYPE_BEARER;
    gw_cfg.ruuvi_cfg.http.auth.auth_bearer.token      = (ruuvi_gw_cfg_http_bearer_token_t) { .buf = "my_token" };

    gw_cfg.ruuvi_cfg.remote.use_remote_cfg       = true;
    gw_cfg.ruuvi_cfg.remote.use_ssl_client_cert  = true;
    gw_cfg.ruuvi_cfg.remote.use_ssl_server_cert  = true;
    gw_cfg.ruuvi_cfg.remote.url                  = (ruuvi_gw_cfg_http_url_t) { .buf = "https://myserver2.com/api2" };
    gw_cfg.ruuvi_cfg.remote.auth_type            = GW_CFG_HTTP_AUTH_TYPE_BASIC;
    gw_cfg.ruuvi_cfg.remote.auth.auth_basic.user = (ruuvi_gw_cfg_http_user_t) { .buf = "username1" };
    gw_cfg.ruuvi_cfg.remote.auth.auth_basic.password = (ruuvi_gw_cfg_http_password_t) { .buf = "password1" };

    gw_cfg.ruuvi_cfg.http_stat.use_http_stat                 = true;
    gw_cfg.ruuvi_cfg.http_stat.http_stat_use_ssl_client_cert = true;
    gw_cfg.ruuvi_cfg.http_stat.http_stat_use_ssl_server_cert = true;
    gw_cfg.ruuvi_cfg.http_stat.http_stat_url  = (ruuvi_gw_cfg_http_url_t) { .buf = "https://myserver3.com/stat" };
    gw_cfg.ruuvi_cfg.http_stat.http_stat_user = (ruuvi_gw_cfg_http_user_t) { .buf = "username2" };
    gw_cfg.ruuvi_cfg.http_stat.http_stat_pass = (ruuvi_gw_cfg_http_password_t) { .buf = "password2" };

    gw_cfg.ruuvi_cfg.mqtt.use_mqtt              = true;
    gw_cfg.ruuvi_cfg.mqtt.use_ssl_client_cert   = true;
    gw_cfg.ruuvi_cfg.mqtt.use_ssl_server_cert   = true;
    gw_cfg.ruuvi_cfg.mqtt.mqtt_transport        = (ruuvi_gw_cfg_mqtt_transport_t) { .buf = "WSS" };
    gw_cfg.ruuvi_cfg.mqtt.mqtt_data_format      = GW_CFG_MQTT_DATA_FORMAT_RUUVI_RAW_AND_DECODED;
    gw_cfg.ruuvi_cfg.mqtt.mqtt_server           = (ruuvi_gw_cfg_mqtt_server_t) { .buf = "myserver4.com" };
    gw_cfg.ruuvi_cfg.mqtt.mqtt_port             = 1234;
    gw_cfg.ruuvi_cfg.mqtt.mqtt_sending_interval = 60;
    gw_cfg.ruuvi_cfg.mqtt.mqtt_prefix           = (ruuvi_gw_cfg_mqtt_prefix_t) { .buf = "mqtt_prefix1" };
    gw_cfg.ruuvi_cfg.mqtt.mqtt_client_id        = (ruuvi_gw_cfg_mqtt_client_id_t) { .buf = "mqtt_client_id1" };
    gw_cfg.ruuvi_cfg.mqtt.mqtt_user             = (ruuvi_gw_cfg_mqtt_user_t) { .buf = "mqtt_user1" };
    gw_cfg.ruuvi_cfg.mqtt.mqtt_pass             = (ruuvi_gw_cfg_mqtt_password_t) { .buf = "mqtt_password1" };

    gw_cfg.ruuvi_cfg.lan_auth.lan_auth_type       = HTTP_SERVER_AUTH_TYPE_BASIC;
    gw_cfg.ruuvi_cfg.lan_auth.lan_auth_user       = (http_server_auth_user_t) { .buf = "lan_user1" };
    gw_cfg.ruuvi_cfg.lan_auth.lan_auth_pass       = (http_server_auth_pass_t) { .buf = "lan_pass1" };
    gw_cfg.ruuvi_cfg.lan_auth.lan_auth_api_key    = (http_server_auth_api_key_t) { .buf = "lan_api_key1" };
    gw_cfg.ruuvi_cfg.lan_auth.lan_auth_api_key_rw = (http_server_auth_api_key_t) { .buf = "lan_api_key2" };

    gw_cfg.ruuvi_cfg.scan_filter.scan_filter_allow_listed = true;
    gw_cfg.ruuvi_cfg.scan_filter.scan_filter_length       = 2;
    gw_cfg.ruuvi_cfg.scan_filter.scan_filter_list[0]      = (mac_address_bin_t) {
             .mac = { 0x11, 0x22, 0x33, 0x44, 0x55, 0x01 }
    };
    gw_cfg.ruuvi_cfg.scan_filter.scan_filter_list[1] = (mac_address_bin_t) {
        .mac = { 0x11, 0x22, 0x33, 0x44, 0x55, 0x02 }
    };

    gw_cfg.ruuvi_cfg.coordinates = (ruuvi_gw_cfg_coordinates_t) { .buf = "coord1,coord2" };

    gw_cfg.ruuvi_cfg.fw_update = (ruuvi_gw_cfg_fw_update_t) { .fw_update_url = "https://fwupdate.server1.com" };

    for (int32_t i = 0; i < std::size(expected_logs); ++i)
    {
        SCOPED_TRACE(::testing::Message() << "Simulate memory allocation failure on step: " << i);
        this->m_malloc_cnt         = 0;
        this->m_malloc_fail_on_cnt = i + 1;

        cjson_wrap_str_t json_str = cjson_wrap_str_null();

        if (expected_logs[i] != "")
        {
            ASSERT_FALSE(gw_cfg_ruuvi_json_generate(&gw_cfg, &json_str));
            ASSERT_EQ(nullptr, json_str.p_str);
            ASSERT_EQ(expected_logs[i], esp_log_wrapper_get_logs());
        }
        else
        {
            ASSERT_TRUE(gw_cfg_ruuvi_json_generate(&gw_cfg, &json_str));
            ASSERT_NE(nullptr, json_str.p_str);
            ASSERT_EQ(expected_json, string(json_str.p_str));
            cjson_wrap_free_json_str(&json_str);
        }
        ASSERT_TRUE(g_pTestClass->m_mem_alloc_trace.is_empty());
    }
}

TEST_F(TestGwCfgRuuviJsonGenerate, gw_cfg_json_generate_for_saving_default) // NOLINT
{
    const gw_cfg_t   gw_cfg   = get_gateway_config_default();
    cjson_wrap_str_t json_str = cjson_wrap_str_null();

    ASSERT_TRUE(gw_cfg_json_generate_for_saving(&gw_cfg, &json_str));
    ASSERT_NE(nullptr, json_str.p_str);
    // flag_hide_passwords=false: wifi_sta password shown (empty string)
    ASSERT_NE(nullptr, strstr(json_str.p_str, "\"password\":\t\"\""));
    // mqtt_pass shown
    ASSERT_NE(nullptr, strstr(json_str.p_str, "\"mqtt_pass\":\t\"\""));
    // http_stat_pass shown
    ASSERT_NE(nullptr, strstr(json_str.p_str, "\"http_stat_pass\":\t\"\""));
    // lan_auth_api_key as string (not _use bool)
    ASSERT_NE(nullptr, strstr(json_str.p_str, "\"lan_auth_api_key\":\t\"\""));
    ASSERT_NE(nullptr, strstr(json_str.p_str, "\"lan_auth_api_key_rw\":\t\"\""));
    // No device_info (fw_ver, nrf52_fw_ver, gw_mac, storage) when saving
    ASSERT_EQ(nullptr, strstr(json_str.p_str, "\"fw_ver\""));
    ASSERT_EQ(nullptr, strstr(json_str.p_str, "\"storage\""));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    cjson_wrap_free_json_str(&json_str);
}

TEST_F(TestGwCfgRuuviJsonGenerate, gw_cfg_json_generate_for_saving_with_passwords) // NOLINT
{
    gw_cfg_t         gw_cfg   = get_gateway_config_default();
    cjson_wrap_str_t json_str = cjson_wrap_str_null();

    // Set LAN auth to RUUVI type with password
    gw_cfg.ruuvi_cfg.lan_auth.lan_auth_type = HTTP_SERVER_AUTH_TYPE_RUUVI;
    snprintf(
        gw_cfg.ruuvi_cfg.lan_auth.lan_auth_pass.buf,
        sizeof(gw_cfg.ruuvi_cfg.lan_auth.lan_auth_pass.buf),
        "mypass");
    snprintf(
        gw_cfg.ruuvi_cfg.lan_auth.lan_auth_api_key.buf,
        sizeof(gw_cfg.ruuvi_cfg.lan_auth.lan_auth_api_key.buf),
        "my_api_key_123");
    snprintf(
        gw_cfg.ruuvi_cfg.lan_auth.lan_auth_api_key_rw.buf,
        sizeof(gw_cfg.ruuvi_cfg.lan_auth.lan_auth_api_key_rw.buf),
        "my_api_key_rw_456");
    // Set MQTT password
    snprintf(gw_cfg.ruuvi_cfg.mqtt.mqtt_pass.buf, sizeof(gw_cfg.ruuvi_cfg.mqtt.mqtt_pass.buf), "mqtt_secret");
    // Set HTTP stat password
    snprintf(
        gw_cfg.ruuvi_cfg.http_stat.http_stat_pass.buf,
        sizeof(gw_cfg.ruuvi_cfg.http_stat.http_stat_pass.buf),
        "stat_secret");
    // Set WiFi STA password
    snprintf(
        (char*)gw_cfg.wifi_cfg.sta.wifi_config_sta.password,
        sizeof(gw_cfg.wifi_cfg.sta.wifi_config_sta.password),
        "wifi_pass");
    // Set WiFi AP password
    snprintf(
        (char*)gw_cfg.wifi_cfg.ap.wifi_config_ap.password,
        sizeof(gw_cfg.wifi_cfg.ap.wifi_config_ap.password),
        "ap_pass");

    ASSERT_TRUE(gw_cfg_json_generate_for_saving(&gw_cfg, &json_str));
    ASSERT_NE(nullptr, json_str.p_str);
    // Passwords visible
    ASSERT_NE(nullptr, strstr(json_str.p_str, "\"lan_auth_pass\":\t\"mypass\""));
    ASSERT_NE(nullptr, strstr(json_str.p_str, "\"lan_auth_api_key\":\t\"my_api_key_123\""));
    ASSERT_NE(nullptr, strstr(json_str.p_str, "\"lan_auth_api_key_rw\":\t\"my_api_key_rw_456\""));
    ASSERT_NE(nullptr, strstr(json_str.p_str, "\"mqtt_pass\":\t\"mqtt_secret\""));
    ASSERT_NE(nullptr, strstr(json_str.p_str, "\"http_stat_pass\":\t\"stat_secret\""));
    ASSERT_NE(nullptr, strstr(json_str.p_str, "\"wifi_pass\""));
    ASSERT_NE(nullptr, strstr(json_str.p_str, "\"ap_pass\""));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    cjson_wrap_free_json_str(&json_str);
}

TEST_F(TestGwCfgRuuviJsonGenerate, gw_cfg_json_generate_remote_auth_basic_hide_passwords) // NOLINT
{
    gw_cfg_t         gw_cfg   = get_gateway_config_default();
    cjson_wrap_str_t json_str = cjson_wrap_str_null();

    gw_cfg.ruuvi_cfg.remote.auth_type = GW_CFG_HTTP_AUTH_TYPE_BASIC;
    snprintf(
        gw_cfg.ruuvi_cfg.remote.auth.auth_basic.user.buf,
        sizeof(gw_cfg.ruuvi_cfg.remote.auth.auth_basic.user.buf),
        "r_user1");
    snprintf(
        gw_cfg.ruuvi_cfg.remote.auth.auth_basic.password.buf,
        sizeof(gw_cfg.ruuvi_cfg.remote.auth.auth_basic.password.buf),
        "r_pass1");

    ASSERT_TRUE(gw_cfg_ruuvi_json_generate(&gw_cfg, &json_str));
    ASSERT_NE(nullptr, json_str.p_str);
    ASSERT_NE(nullptr, strstr(json_str.p_str, "\"remote_cfg_auth_type\":\t\"basic\""));
    ASSERT_NE(nullptr, strstr(json_str.p_str, "\"remote_cfg_auth_basic_user\":\t\"r_user1\""));
    // Password hidden
    ASSERT_EQ(nullptr, strstr(json_str.p_str, "remote_cfg_auth_basic_pass"));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    cjson_wrap_free_json_str(&json_str);
}

TEST_F(TestGwCfgRuuviJsonGenerate, gw_cfg_json_generate_remote_auth_basic_for_saving) // NOLINT
{
    gw_cfg_t         gw_cfg   = get_gateway_config_default();
    cjson_wrap_str_t json_str = cjson_wrap_str_null();

    gw_cfg.ruuvi_cfg.remote.auth_type = GW_CFG_HTTP_AUTH_TYPE_BASIC;
    snprintf(
        gw_cfg.ruuvi_cfg.remote.auth.auth_basic.user.buf,
        sizeof(gw_cfg.ruuvi_cfg.remote.auth.auth_basic.user.buf),
        "r_user2");
    snprintf(
        gw_cfg.ruuvi_cfg.remote.auth.auth_basic.password.buf,
        sizeof(gw_cfg.ruuvi_cfg.remote.auth.auth_basic.password.buf),
        "r_pass2");

    ASSERT_TRUE(gw_cfg_json_generate_for_saving(&gw_cfg, &json_str));
    ASSERT_NE(nullptr, json_str.p_str);
    ASSERT_NE(nullptr, strstr(json_str.p_str, "\"remote_cfg_auth_type\":\t\"basic\""));
    ASSERT_NE(nullptr, strstr(json_str.p_str, "\"remote_cfg_auth_basic_user\":\t\"r_user2\""));
    ASSERT_NE(nullptr, strstr(json_str.p_str, "\"remote_cfg_auth_basic_pass\":\t\"r_pass2\""));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    cjson_wrap_free_json_str(&json_str);
}

TEST_F(TestGwCfgRuuviJsonGenerate, gw_cfg_json_generate_remote_auth_bearer_hide_passwords) // NOLINT
{
    gw_cfg_t         gw_cfg   = get_gateway_config_default();
    cjson_wrap_str_t json_str = cjson_wrap_str_null();

    gw_cfg.ruuvi_cfg.remote.auth_type = GW_CFG_HTTP_AUTH_TYPE_BEARER;
    snprintf(
        gw_cfg.ruuvi_cfg.remote.auth.auth_bearer.token.buf,
        sizeof(gw_cfg.ruuvi_cfg.remote.auth.auth_bearer.token.buf),
        "bearer_tok1");

    ASSERT_TRUE(gw_cfg_ruuvi_json_generate(&gw_cfg, &json_str));
    ASSERT_NE(nullptr, json_str.p_str);
    ASSERT_NE(nullptr, strstr(json_str.p_str, "\"remote_cfg_auth_type\":\t\"bearer\""));
    // Token hidden
    ASSERT_EQ(nullptr, strstr(json_str.p_str, "remote_cfg_auth_bearer_token"));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    cjson_wrap_free_json_str(&json_str);
}

TEST_F(TestGwCfgRuuviJsonGenerate, gw_cfg_json_generate_remote_auth_bearer_for_saving) // NOLINT
{
    gw_cfg_t         gw_cfg   = get_gateway_config_default();
    cjson_wrap_str_t json_str = cjson_wrap_str_null();

    gw_cfg.ruuvi_cfg.remote.auth_type = GW_CFG_HTTP_AUTH_TYPE_BEARER;
    snprintf(
        gw_cfg.ruuvi_cfg.remote.auth.auth_bearer.token.buf,
        sizeof(gw_cfg.ruuvi_cfg.remote.auth.auth_bearer.token.buf),
        "bearer_tok2");

    ASSERT_TRUE(gw_cfg_json_generate_for_saving(&gw_cfg, &json_str));
    ASSERT_NE(nullptr, json_str.p_str);
    ASSERT_NE(nullptr, strstr(json_str.p_str, "\"remote_cfg_auth_type\":\t\"bearer\""));
    ASSERT_NE(nullptr, strstr(json_str.p_str, "\"remote_cfg_auth_bearer_token\":\t\"bearer_tok2\""));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    cjson_wrap_free_json_str(&json_str);
}

TEST_F(TestGwCfgRuuviJsonGenerate, gw_cfg_json_generate_remote_auth_token_hide_passwords) // NOLINT
{
    gw_cfg_t         gw_cfg   = get_gateway_config_default();
    cjson_wrap_str_t json_str = cjson_wrap_str_null();

    gw_cfg.ruuvi_cfg.remote.auth_type = GW_CFG_HTTP_AUTH_TYPE_TOKEN;
    snprintf(
        gw_cfg.ruuvi_cfg.remote.auth.auth_token.token.buf,
        sizeof(gw_cfg.ruuvi_cfg.remote.auth.auth_token.token.buf),
        "my_token1");

    ASSERT_TRUE(gw_cfg_ruuvi_json_generate(&gw_cfg, &json_str));
    ASSERT_NE(nullptr, json_str.p_str);
    ASSERT_NE(nullptr, strstr(json_str.p_str, "\"remote_cfg_auth_type\":\t\"token\""));
    ASSERT_EQ(nullptr, strstr(json_str.p_str, "remote_cfg_auth_token\""));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    cjson_wrap_free_json_str(&json_str);
}

TEST_F(TestGwCfgRuuviJsonGenerate, gw_cfg_json_generate_remote_auth_token_for_saving) // NOLINT
{
    gw_cfg_t         gw_cfg   = get_gateway_config_default();
    cjson_wrap_str_t json_str = cjson_wrap_str_null();

    gw_cfg.ruuvi_cfg.remote.auth_type = GW_CFG_HTTP_AUTH_TYPE_TOKEN;
    snprintf(
        gw_cfg.ruuvi_cfg.remote.auth.auth_token.token.buf,
        sizeof(gw_cfg.ruuvi_cfg.remote.auth.auth_token.token.buf),
        "my_token2");

    ASSERT_TRUE(gw_cfg_json_generate_for_saving(&gw_cfg, &json_str));
    ASSERT_NE(nullptr, json_str.p_str);
    ASSERT_NE(nullptr, strstr(json_str.p_str, "\"remote_cfg_auth_type\":\t\"token\""));
    ASSERT_NE(nullptr, strstr(json_str.p_str, "\"remote_cfg_auth_token\":\t\"my_token2\""));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    cjson_wrap_free_json_str(&json_str);
}

TEST_F(TestGwCfgRuuviJsonGenerate, gw_cfg_json_generate_remote_auth_apikey_hide_passwords) // NOLINT
{
    gw_cfg_t         gw_cfg   = get_gateway_config_default();
    cjson_wrap_str_t json_str = cjson_wrap_str_null();

    gw_cfg.ruuvi_cfg.remote.auth_type = GW_CFG_HTTP_AUTH_TYPE_APIKEY;
    snprintf(
        gw_cfg.ruuvi_cfg.remote.auth.auth_apikey.api_key.buf,
        sizeof(gw_cfg.ruuvi_cfg.remote.auth.auth_apikey.api_key.buf),
        "my_apikey1");

    ASSERT_TRUE(gw_cfg_ruuvi_json_generate(&gw_cfg, &json_str));
    ASSERT_NE(nullptr, json_str.p_str);
    ASSERT_NE(nullptr, strstr(json_str.p_str, "\"remote_cfg_auth_type\":\t\"api_key\""));
    ASSERT_EQ(nullptr, strstr(json_str.p_str, "remote_cfg_auth_apikey\""));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    cjson_wrap_free_json_str(&json_str);
}

TEST_F(TestGwCfgRuuviJsonGenerate, gw_cfg_json_generate_remote_auth_apikey_for_saving) // NOLINT
{
    gw_cfg_t         gw_cfg   = get_gateway_config_default();
    cjson_wrap_str_t json_str = cjson_wrap_str_null();

    gw_cfg.ruuvi_cfg.remote.auth_type = GW_CFG_HTTP_AUTH_TYPE_APIKEY;
    snprintf(
        gw_cfg.ruuvi_cfg.remote.auth.auth_apikey.api_key.buf,
        sizeof(gw_cfg.ruuvi_cfg.remote.auth.auth_apikey.api_key.buf),
        "my_apikey2");

    ASSERT_TRUE(gw_cfg_json_generate_for_saving(&gw_cfg, &json_str));
    ASSERT_NE(nullptr, json_str.p_str);
    ASSERT_NE(nullptr, strstr(json_str.p_str, "\"remote_cfg_auth_type\":\t\"api_key\""));
    ASSERT_NE(nullptr, strstr(json_str.p_str, "\"remote_cfg_auth_apikey\":\t\"my_apikey2\""));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    cjson_wrap_free_json_str(&json_str);
}

TEST_F(TestGwCfgRuuviJsonGenerate, gw_cfg_json_generate_http_bearer_auth) // NOLINT
{
    gw_cfg_t         gw_cfg   = get_gateway_config_default();
    cjson_wrap_str_t json_str = cjson_wrap_str_null();

    gw_cfg.ruuvi_cfg.http.use_http_ruuvi = false;
    gw_cfg.ruuvi_cfg.http.use_http       = true;
    gw_cfg.ruuvi_cfg.http.auth_type      = GW_CFG_HTTP_AUTH_TYPE_BEARER;
    gw_cfg.ruuvi_cfg.http.data_format    = GW_CFG_HTTP_DATA_FORMAT_RUUVI;
    snprintf(gw_cfg.ruuvi_cfg.http.http_url.buf, sizeof(gw_cfg.ruuvi_cfg.http.http_url.buf), "https://custom.url");
    snprintf(
        gw_cfg.ruuvi_cfg.http.auth.auth_bearer.token.buf,
        sizeof(gw_cfg.ruuvi_cfg.http.auth.auth_bearer.token.buf),
        "http_bearer_tok");

    ASSERT_TRUE(gw_cfg_ruuvi_json_generate(&gw_cfg, &json_str));
    ASSERT_NE(nullptr, json_str.p_str);
    ASSERT_NE(nullptr, strstr(json_str.p_str, "\"http_auth\":\t\"bearer\""));
    // Token hidden with flag_hide_passwords=true
    ASSERT_EQ(nullptr, strstr(json_str.p_str, "http_bearer_token"));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    cjson_wrap_free_json_str(&json_str);
}

TEST_F(TestGwCfgRuuviJsonGenerate, gw_cfg_json_generate_http_bearer_auth_for_saving) // NOLINT
{
    gw_cfg_t         gw_cfg   = get_gateway_config_default();
    cjson_wrap_str_t json_str = cjson_wrap_str_null();

    gw_cfg.ruuvi_cfg.http.use_http_ruuvi = false;
    gw_cfg.ruuvi_cfg.http.use_http       = true;
    gw_cfg.ruuvi_cfg.http.auth_type      = GW_CFG_HTTP_AUTH_TYPE_BEARER;
    gw_cfg.ruuvi_cfg.http.data_format    = GW_CFG_HTTP_DATA_FORMAT_RUUVI;
    snprintf(gw_cfg.ruuvi_cfg.http.http_url.buf, sizeof(gw_cfg.ruuvi_cfg.http.http_url.buf), "https://custom.url");
    snprintf(
        gw_cfg.ruuvi_cfg.http.auth.auth_bearer.token.buf,
        sizeof(gw_cfg.ruuvi_cfg.http.auth.auth_bearer.token.buf),
        "http_bearer_tok");

    ASSERT_TRUE(gw_cfg_json_generate_for_saving(&gw_cfg, &json_str));
    ASSERT_NE(nullptr, json_str.p_str);
    ASSERT_NE(nullptr, strstr(json_str.p_str, "\"http_auth\":\t\"bearer\""));
    ASSERT_NE(nullptr, strstr(json_str.p_str, "\"http_bearer_token\":\t\"http_bearer_tok\""));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    cjson_wrap_free_json_str(&json_str);
}

TEST_F(TestGwCfgRuuviJsonGenerate, gw_cfg_json_generate_http_token_auth) // NOLINT
{
    gw_cfg_t         gw_cfg   = get_gateway_config_default();
    cjson_wrap_str_t json_str = cjson_wrap_str_null();

    gw_cfg.ruuvi_cfg.http.use_http_ruuvi = false;
    gw_cfg.ruuvi_cfg.http.use_http       = true;
    gw_cfg.ruuvi_cfg.http.auth_type      = GW_CFG_HTTP_AUTH_TYPE_TOKEN;
    gw_cfg.ruuvi_cfg.http.data_format    = GW_CFG_HTTP_DATA_FORMAT_RUUVI;
    snprintf(gw_cfg.ruuvi_cfg.http.http_url.buf, sizeof(gw_cfg.ruuvi_cfg.http.http_url.buf), "https://custom.url");
    snprintf(
        gw_cfg.ruuvi_cfg.http.auth.auth_token.token.buf,
        sizeof(gw_cfg.ruuvi_cfg.http.auth.auth_token.token.buf),
        "http_tok");

    ASSERT_TRUE(gw_cfg_ruuvi_json_generate(&gw_cfg, &json_str));
    ASSERT_NE(nullptr, json_str.p_str);
    ASSERT_NE(nullptr, strstr(json_str.p_str, "\"http_auth\":\t\"token\""));
    // Token hidden
    ASSERT_EQ(nullptr, strstr(json_str.p_str, "\"http_api_key\""));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    cjson_wrap_free_json_str(&json_str);
}

TEST_F(TestGwCfgRuuviJsonGenerate, gw_cfg_json_generate_http_token_auth_for_saving) // NOLINT
{
    gw_cfg_t         gw_cfg   = get_gateway_config_default();
    cjson_wrap_str_t json_str = cjson_wrap_str_null();

    gw_cfg.ruuvi_cfg.http.use_http_ruuvi = false;
    gw_cfg.ruuvi_cfg.http.use_http       = true;
    gw_cfg.ruuvi_cfg.http.auth_type      = GW_CFG_HTTP_AUTH_TYPE_TOKEN;
    gw_cfg.ruuvi_cfg.http.data_format    = GW_CFG_HTTP_DATA_FORMAT_RUUVI;
    snprintf(gw_cfg.ruuvi_cfg.http.http_url.buf, sizeof(gw_cfg.ruuvi_cfg.http.http_url.buf), "https://custom.url");
    snprintf(
        gw_cfg.ruuvi_cfg.http.auth.auth_token.token.buf,
        sizeof(gw_cfg.ruuvi_cfg.http.auth.auth_token.token.buf),
        "http_tok_val");

    ASSERT_TRUE(gw_cfg_json_generate_for_saving(&gw_cfg, &json_str));
    ASSERT_NE(nullptr, json_str.p_str);
    ASSERT_NE(nullptr, strstr(json_str.p_str, "\"http_auth\":\t\"token\""));
    ASSERT_NE(nullptr, strstr(json_str.p_str, "\"http_api_key\":\t\"http_tok_val\""));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    cjson_wrap_free_json_str(&json_str);
}

TEST_F(TestGwCfgRuuviJsonGenerate, gw_cfg_json_generate_http_apikey_auth) // NOLINT
{
    gw_cfg_t         gw_cfg   = get_gateway_config_default();
    cjson_wrap_str_t json_str = cjson_wrap_str_null();

    gw_cfg.ruuvi_cfg.http.use_http_ruuvi = false;
    gw_cfg.ruuvi_cfg.http.use_http       = true;
    gw_cfg.ruuvi_cfg.http.auth_type      = GW_CFG_HTTP_AUTH_TYPE_APIKEY;
    gw_cfg.ruuvi_cfg.http.data_format    = GW_CFG_HTTP_DATA_FORMAT_RUUVI;
    snprintf(gw_cfg.ruuvi_cfg.http.http_url.buf, sizeof(gw_cfg.ruuvi_cfg.http.http_url.buf), "https://custom.url");
    snprintf(
        gw_cfg.ruuvi_cfg.http.auth.auth_apikey.api_key.buf,
        sizeof(gw_cfg.ruuvi_cfg.http.auth.auth_apikey.api_key.buf),
        "http_apikey_val");

    ASSERT_TRUE(gw_cfg_ruuvi_json_generate(&gw_cfg, &json_str));
    ASSERT_NE(nullptr, json_str.p_str);
    ASSERT_NE(nullptr, strstr(json_str.p_str, "\"http_auth\":\t\"api_key\""));
    // API key hidden
    ASSERT_EQ(nullptr, strstr(json_str.p_str, "\"http_api_key\""));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    cjson_wrap_free_json_str(&json_str);
}

TEST_F(TestGwCfgRuuviJsonGenerate, gw_cfg_json_generate_http_apikey_auth_for_saving) // NOLINT
{
    gw_cfg_t         gw_cfg   = get_gateway_config_default();
    cjson_wrap_str_t json_str = cjson_wrap_str_null();

    gw_cfg.ruuvi_cfg.http.use_http_ruuvi = false;
    gw_cfg.ruuvi_cfg.http.use_http       = true;
    gw_cfg.ruuvi_cfg.http.auth_type      = GW_CFG_HTTP_AUTH_TYPE_APIKEY;
    gw_cfg.ruuvi_cfg.http.data_format    = GW_CFG_HTTP_DATA_FORMAT_RUUVI;
    snprintf(gw_cfg.ruuvi_cfg.http.http_url.buf, sizeof(gw_cfg.ruuvi_cfg.http.http_url.buf), "https://custom.url");
    snprintf(
        gw_cfg.ruuvi_cfg.http.auth.auth_apikey.api_key.buf,
        sizeof(gw_cfg.ruuvi_cfg.http.auth.auth_apikey.api_key.buf),
        "http_apikey_val2");

    ASSERT_TRUE(gw_cfg_json_generate_for_saving(&gw_cfg, &json_str));
    ASSERT_NE(nullptr, json_str.p_str);
    ASSERT_NE(nullptr, strstr(json_str.p_str, "\"http_auth\":\t\"api_key\""));
    ASSERT_NE(nullptr, strstr(json_str.p_str, "\"http_api_key\":\t\"http_apikey_val2\""));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    cjson_wrap_free_json_str(&json_str);
}

TEST_F(TestGwCfgRuuviJsonGenerate, gw_cfg_json_generate_http_data_format_decoded) // NOLINT
{
    gw_cfg_t         gw_cfg   = get_gateway_config_default();
    cjson_wrap_str_t json_str = cjson_wrap_str_null();

    gw_cfg.ruuvi_cfg.http.use_http_ruuvi = false;
    gw_cfg.ruuvi_cfg.http.use_http       = true;
    gw_cfg.ruuvi_cfg.http.auth_type      = GW_CFG_HTTP_AUTH_TYPE_NONE;
    gw_cfg.ruuvi_cfg.http.data_format    = GW_CFG_HTTP_DATA_FORMAT_RUUVI_DECODED;
    snprintf(
        gw_cfg.ruuvi_cfg.http.http_url.buf,
        sizeof(gw_cfg.ruuvi_cfg.http.http_url.buf),
        "https://custom.decoded.url");

    ASSERT_TRUE(gw_cfg_ruuvi_json_generate(&gw_cfg, &json_str));
    ASSERT_NE(nullptr, json_str.p_str);
    ASSERT_NE(nullptr, strstr(json_str.p_str, "\"http_data_format\":\t\"ruuvi_decoded\""));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    cjson_wrap_free_json_str(&json_str);
}

TEST_F(TestGwCfgRuuviJsonGenerate, gw_cfg_json_generate_http_use_http_ruuvi_true_use_http_false) // NOLINT
{
    gw_cfg_t         gw_cfg   = get_gateway_config_default();
    cjson_wrap_str_t json_str = cjson_wrap_str_null();

    // This combination falls back to default HTTP config
    gw_cfg.ruuvi_cfg.http.use_http_ruuvi = true;
    gw_cfg.ruuvi_cfg.http.use_http       = false;

    ASSERT_TRUE(gw_cfg_ruuvi_json_generate(&gw_cfg, &json_str));
    ASSERT_NE(nullptr, json_str.p_str);
    // Should be patched to default: use_http_ruuvi=true, use_http=true, default URL
    ASSERT_NE(nullptr, strstr(json_str.p_str, "\"use_http_ruuvi\":\ttrue"));
    ASSERT_NE(nullptr, strstr(json_str.p_str, "\"use_http\":\ttrue"));
    ASSERT_NE(nullptr, strstr(json_str.p_str, RUUVI_GATEWAY_HTTP_DEFAULT_URL));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    cjson_wrap_free_json_str(&json_str);
}

TEST_F(TestGwCfgRuuviJsonGenerate, gw_cfg_json_generate_http_custom_not_used) // NOLINT
{
    gw_cfg_t         gw_cfg   = get_gateway_config_default();
    cjson_wrap_str_t json_str = cjson_wrap_str_null();

    gw_cfg.ruuvi_cfg.http.use_http_ruuvi = false;
    gw_cfg.ruuvi_cfg.http.use_http       = false;
    snprintf(gw_cfg.ruuvi_cfg.http.http_url.buf, sizeof(gw_cfg.ruuvi_cfg.http.http_url.buf), "https://unused.url");

    ASSERT_TRUE(gw_cfg_ruuvi_json_generate(&gw_cfg, &json_str));
    ASSERT_NE(nullptr, json_str.p_str);
    ASSERT_NE(nullptr, strstr(json_str.p_str, "\"use_http_ruuvi\":\tfalse"));
    ASSERT_NE(nullptr, strstr(json_str.p_str, "\"use_http\":\tfalse"));
    // No http_url, http_period, etc. when use_http=false
    ASSERT_EQ(nullptr, strstr(json_str.p_str, "\"http_url\""));
    ASSERT_EQ(nullptr, strstr(json_str.p_str, "\"http_period\""));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    cjson_wrap_free_json_str(&json_str);
}

TEST_F(TestGwCfgRuuviJsonGenerate, gw_cfg_json_generate_mqtt_data_format_raw_and_decoded) // NOLINT
{
    gw_cfg_t         gw_cfg   = get_gateway_config_default();
    cjson_wrap_str_t json_str = cjson_wrap_str_null();

    gw_cfg.ruuvi_cfg.mqtt.mqtt_data_format = GW_CFG_MQTT_DATA_FORMAT_RUUVI_RAW_AND_DECODED;

    ASSERT_TRUE(gw_cfg_ruuvi_json_generate(&gw_cfg, &json_str));
    ASSERT_NE(nullptr, json_str.p_str);
    ASSERT_NE(nullptr, strstr(json_str.p_str, "\"mqtt_data_format\":\t\"ruuvi_raw_and_decoded\""));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    cjson_wrap_free_json_str(&json_str);
}

TEST_F(TestGwCfgRuuviJsonGenerate, gw_cfg_json_generate_mqtt_data_format_decoded) // NOLINT
{
    gw_cfg_t         gw_cfg   = get_gateway_config_default();
    cjson_wrap_str_t json_str = cjson_wrap_str_null();

    gw_cfg.ruuvi_cfg.mqtt.mqtt_data_format = GW_CFG_MQTT_DATA_FORMAT_RUUVI_DECODED;

    ASSERT_TRUE(gw_cfg_ruuvi_json_generate(&gw_cfg, &json_str));
    ASSERT_NE(nullptr, json_str.p_str);
    ASSERT_NE(nullptr, strstr(json_str.p_str, "\"mqtt_data_format\":\t\"ruuvi_decoded\""));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    cjson_wrap_free_json_str(&json_str);
}

TEST_F(TestGwCfgRuuviJsonGenerate, gw_cfg_json_generate_storage_ready_with_files) // NOLINT
{
    const gw_cfg_t   gw_cfg   = get_gateway_config_default();
    cjson_wrap_str_t json_str = cjson_wrap_str_null();

    this->m_flag_storage_ready         = true;
    this->m_flag_storage_http_cli_cert = true;
    this->m_flag_storage_http_cli_key  = true;
    this->m_flag_storage_http_srv_cert = true;

    ASSERT_TRUE(gw_cfg_ruuvi_json_generate(&gw_cfg, &json_str));
    ASSERT_NE(nullptr, json_str.p_str);
    ASSERT_NE(nullptr, strstr(json_str.p_str, "\"storage_ready\":\ttrue"));
    ASSERT_NE(nullptr, strstr(json_str.p_str, "\"http_cli_cert\":\ttrue"));
    ASSERT_NE(nullptr, strstr(json_str.p_str, "\"http_cli_key\":\ttrue"));
    ASSERT_NE(nullptr, strstr(json_str.p_str, "\"http_srv_cert\":\ttrue"));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    cjson_wrap_free_json_str(&json_str);
}

TEST_F(TestGwCfgRuuviJsonGenerate, gw_cfg_json_generate_scan_filter_with_macs) // NOLINT
{
    gw_cfg_t         gw_cfg   = get_gateway_config_default();
    cjson_wrap_str_t json_str = cjson_wrap_str_null();

    gw_cfg.ruuvi_cfg.scan_filter.scan_filter_allow_listed = true;
    gw_cfg.ruuvi_cfg.scan_filter.scan_filter_length       = 2;
    gw_cfg.ruuvi_cfg.scan_filter.scan_filter_list[0]      = { { 0x11, 0x22, 0x33, 0x44, 0x55, 0x66 } };
    gw_cfg.ruuvi_cfg.scan_filter.scan_filter_list[1]      = { { 0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF } };

    ASSERT_TRUE(gw_cfg_ruuvi_json_generate(&gw_cfg, &json_str));
    ASSERT_NE(nullptr, json_str.p_str);
    ASSERT_NE(nullptr, strstr(json_str.p_str, "\"scan_filter_allow_listed\":\ttrue"));
    ASSERT_NE(nullptr, strstr(json_str.p_str, "11:22:33:44:55:66"));
    ASSERT_NE(nullptr, strstr(json_str.p_str, "AA:BB:CC:DD:EE:FF"));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    cjson_wrap_free_json_str(&json_str);
}

TEST_F(TestGwCfgRuuviJsonGenerate, gw_cfg_json_generate_wifi_ap_channel_nonzero) // NOLINT
{
    gw_cfg_t         gw_cfg   = get_gateway_config_default();
    cjson_wrap_str_t json_str = cjson_wrap_str_null();

    gw_cfg.wifi_cfg.ap.wifi_config_ap.channel = 6;

    ASSERT_TRUE(gw_cfg_ruuvi_json_generate(&gw_cfg, &json_str));
    ASSERT_NE(nullptr, json_str.p_str);
    ASSERT_NE(nullptr, strstr(json_str.p_str, "\"channel\":\t6"));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    cjson_wrap_free_json_str(&json_str);
}

TEST_F(TestGwCfgRuuviJsonGenerate, gw_cfg_json_generate_http_basic_auth_for_saving) // NOLINT
{
    gw_cfg_t         gw_cfg   = get_gateway_config_default();
    cjson_wrap_str_t json_str = cjson_wrap_str_null();

    gw_cfg.ruuvi_cfg.http.use_http_ruuvi = false;
    gw_cfg.ruuvi_cfg.http.use_http       = true;
    gw_cfg.ruuvi_cfg.http.auth_type      = GW_CFG_HTTP_AUTH_TYPE_BASIC;
    gw_cfg.ruuvi_cfg.http.data_format    = GW_CFG_HTTP_DATA_FORMAT_RUUVI;
    snprintf(gw_cfg.ruuvi_cfg.http.http_url.buf, sizeof(gw_cfg.ruuvi_cfg.http.http_url.buf), "https://custom.url");
    snprintf(
        gw_cfg.ruuvi_cfg.http.auth.auth_basic.user.buf,
        sizeof(gw_cfg.ruuvi_cfg.http.auth.auth_basic.user.buf),
        "h_user");
    snprintf(
        gw_cfg.ruuvi_cfg.http.auth.auth_basic.password.buf,
        sizeof(gw_cfg.ruuvi_cfg.http.auth.auth_basic.password.buf),
        "h_pass");

    ASSERT_TRUE(gw_cfg_json_generate_for_saving(&gw_cfg, &json_str));
    ASSERT_NE(nullptr, json_str.p_str);
    ASSERT_NE(nullptr, strstr(json_str.p_str, "\"http_auth\":\t\"basic\""));
    ASSERT_NE(nullptr, strstr(json_str.p_str, "\"http_user\":\t\"h_user\""));
    ASSERT_NE(nullptr, strstr(json_str.p_str, "\"http_pass\":\t\"h_pass\""));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    cjson_wrap_free_json_str(&json_str);
}

TEST_F(TestGwCfgRuuviJsonGenerate, gw_cfg_json_generate_lan_auth_basic_type_for_saving) // NOLINT
{
    gw_cfg_t         gw_cfg   = get_gateway_config_default();
    cjson_wrap_str_t json_str = cjson_wrap_str_null();

    gw_cfg.ruuvi_cfg.lan_auth.lan_auth_type = HTTP_SERVER_AUTH_TYPE_BASIC;
    snprintf(
        gw_cfg.ruuvi_cfg.lan_auth.lan_auth_pass.buf,
        sizeof(gw_cfg.ruuvi_cfg.lan_auth.lan_auth_pass.buf),
        "basic_pw");

    ASSERT_TRUE(gw_cfg_json_generate_for_saving(&gw_cfg, &json_str));
    ASSERT_NE(nullptr, json_str.p_str);
    ASSERT_NE(nullptr, strstr(json_str.p_str, "\"lan_auth_type\":\t\"lan_auth_basic\""));
    ASSERT_NE(nullptr, strstr(json_str.p_str, "\"lan_auth_pass\":\t\"basic_pw\""));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    cjson_wrap_free_json_str(&json_str);
}

TEST_F(TestGwCfgRuuviJsonGenerate, gw_cfg_json_generate_lan_auth_digest_for_saving) // NOLINT
{
    gw_cfg_t         gw_cfg   = get_gateway_config_default();
    cjson_wrap_str_t json_str = cjson_wrap_str_null();

    gw_cfg.ruuvi_cfg.lan_auth.lan_auth_type = HTTP_SERVER_AUTH_TYPE_DIGEST;
    snprintf(
        gw_cfg.ruuvi_cfg.lan_auth.lan_auth_pass.buf,
        sizeof(gw_cfg.ruuvi_cfg.lan_auth.lan_auth_pass.buf),
        "digest_pw");

    ASSERT_TRUE(gw_cfg_json_generate_for_saving(&gw_cfg, &json_str));
    ASSERT_NE(nullptr, json_str.p_str);
    ASSERT_NE(nullptr, strstr(json_str.p_str, "\"lan_auth_type\":\t\"lan_auth_digest\""));
    ASSERT_NE(nullptr, strstr(json_str.p_str, "\"lan_auth_pass\":\t\"digest_pw\""));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    cjson_wrap_free_json_str(&json_str);
}

TEST_F(TestGwCfgRuuviJsonGenerate, gw_cfg_json_generate_lan_auth_deny_for_saving) // NOLINT
{
    gw_cfg_t         gw_cfg   = get_gateway_config_default();
    cjson_wrap_str_t json_str = cjson_wrap_str_null();

    gw_cfg.ruuvi_cfg.lan_auth.lan_auth_type = HTTP_SERVER_AUTH_TYPE_DENY;

    ASSERT_TRUE(gw_cfg_json_generate_for_saving(&gw_cfg, &json_str));
    ASSERT_NE(nullptr, json_str.p_str);
    ASSERT_NE(nullptr, strstr(json_str.p_str, "\"lan_auth_type\":\t\"lan_auth_deny\""));
    // For DENY type, lan_auth_pass is NOT added
    ASSERT_EQ(nullptr, strstr(json_str.p_str, "lan_auth_pass"));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    cjson_wrap_free_json_str(&json_str);
}

// Additional comprehensive tests for uncovered code paths

TEST_F(TestGwCfgRuuviJsonGenerate, gw_cfg_json_generate_for_saving_wifi_passwords_visible) // NOLINT
{
    gw_cfg_t         gw_cfg   = get_gateway_config_default();
    cjson_wrap_str_t json_str = cjson_wrap_str_null();

    snprintf(
        (char*)gw_cfg.wifi_cfg.sta.wifi_config_sta.ssid,
        sizeof(gw_cfg.wifi_cfg.sta.wifi_config_sta.ssid),
        "my_wifi");
    snprintf(
        (char*)gw_cfg.wifi_cfg.sta.wifi_config_sta.password,
        sizeof(gw_cfg.wifi_cfg.sta.wifi_config_sta.password),
        "wifi_pass1");
    snprintf(
        (char*)gw_cfg.wifi_cfg.ap.wifi_config_ap.password,
        sizeof(gw_cfg.wifi_cfg.ap.wifi_config_ap.password),
        "ap_pass1");

    ASSERT_TRUE(gw_cfg_json_generate_for_saving(&gw_cfg, &json_str));
    ASSERT_NE(nullptr, json_str.p_str);
    ASSERT_NE(nullptr, strstr(json_str.p_str, "\"ssid\":\t\"my_wifi\""));
    ASSERT_NE(nullptr, strstr(json_str.p_str, "\"password\":\t\"wifi_pass1\""));
    ASSERT_NE(nullptr, strstr(json_str.p_str, "\"password\":\t\"ap_pass1\""));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    cjson_wrap_free_json_str(&json_str);
}

TEST_F(TestGwCfgRuuviJsonGenerate, gw_cfg_json_generate_for_saving_http_bearer_auth) // NOLINT
{
    gw_cfg_t         gw_cfg   = get_gateway_config_default();
    cjson_wrap_str_t json_str = cjson_wrap_str_null();

    gw_cfg.ruuvi_cfg.http.use_http_ruuvi = false;
    gw_cfg.ruuvi_cfg.http.use_http       = true;
    gw_cfg.ruuvi_cfg.http.auth_type      = GW_CFG_HTTP_AUTH_TYPE_BEARER;
    gw_cfg.ruuvi_cfg.http.data_format    = GW_CFG_HTTP_DATA_FORMAT_RUUVI;
    snprintf(gw_cfg.ruuvi_cfg.http.http_url.buf, sizeof(gw_cfg.ruuvi_cfg.http.http_url.buf), "https://custom.url");
    snprintf(
        gw_cfg.ruuvi_cfg.http.auth.auth_bearer.token.buf,
        sizeof(gw_cfg.ruuvi_cfg.http.auth.auth_bearer.token.buf),
        "bearer_tok_save");

    ASSERT_TRUE(gw_cfg_json_generate_for_saving(&gw_cfg, &json_str));
    ASSERT_NE(nullptr, json_str.p_str);
    ASSERT_NE(nullptr, strstr(json_str.p_str, "\"http_auth\":\t\"bearer\""));
    ASSERT_NE(nullptr, strstr(json_str.p_str, "\"http_bearer_token\":\t\"bearer_tok_save\""));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    cjson_wrap_free_json_str(&json_str);
}

TEST_F(TestGwCfgRuuviJsonGenerate, gw_cfg_json_generate_for_saving_http_token_auth) // NOLINT
{
    gw_cfg_t         gw_cfg   = get_gateway_config_default();
    cjson_wrap_str_t json_str = cjson_wrap_str_null();

    gw_cfg.ruuvi_cfg.http.use_http_ruuvi = false;
    gw_cfg.ruuvi_cfg.http.use_http       = true;
    gw_cfg.ruuvi_cfg.http.auth_type      = GW_CFG_HTTP_AUTH_TYPE_TOKEN;
    gw_cfg.ruuvi_cfg.http.data_format    = GW_CFG_HTTP_DATA_FORMAT_RUUVI;
    snprintf(gw_cfg.ruuvi_cfg.http.http_url.buf, sizeof(gw_cfg.ruuvi_cfg.http.http_url.buf), "https://custom.url");
    snprintf(
        gw_cfg.ruuvi_cfg.http.auth.auth_token.token.buf,
        sizeof(gw_cfg.ruuvi_cfg.http.auth.auth_token.token.buf),
        "token_val_save");

    ASSERT_TRUE(gw_cfg_json_generate_for_saving(&gw_cfg, &json_str));
    ASSERT_NE(nullptr, json_str.p_str);
    ASSERT_NE(nullptr, strstr(json_str.p_str, "\"http_auth\":\t\"token\""));
    ASSERT_NE(nullptr, strstr(json_str.p_str, "\"http_api_key\":\t\"token_val_save\""));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    cjson_wrap_free_json_str(&json_str);
}

TEST_F(TestGwCfgRuuviJsonGenerate, gw_cfg_json_generate_for_saving_http_stat_pass_visible) // NOLINT
{
    gw_cfg_t         gw_cfg   = get_gateway_config_default();
    cjson_wrap_str_t json_str = cjson_wrap_str_null();

    snprintf(
        gw_cfg.ruuvi_cfg.http_stat.http_stat_pass.buf,
        sizeof(gw_cfg.ruuvi_cfg.http_stat.http_stat_pass.buf),
        "stat_pass_save");

    ASSERT_TRUE(gw_cfg_json_generate_for_saving(&gw_cfg, &json_str));
    ASSERT_NE(nullptr, json_str.p_str);
    ASSERT_NE(nullptr, strstr(json_str.p_str, "\"http_stat_pass\":\t\"stat_pass_save\""));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    cjson_wrap_free_json_str(&json_str);
}

TEST_F(TestGwCfgRuuviJsonGenerate, gw_cfg_json_generate_for_saving_mqtt_pass_visible) // NOLINT
{
    gw_cfg_t         gw_cfg   = get_gateway_config_default();
    cjson_wrap_str_t json_str = cjson_wrap_str_null();

    snprintf(gw_cfg.ruuvi_cfg.mqtt.mqtt_pass.buf, sizeof(gw_cfg.ruuvi_cfg.mqtt.mqtt_pass.buf), "mqtt_pass_save");

    ASSERT_TRUE(gw_cfg_json_generate_for_saving(&gw_cfg, &json_str));
    ASSERT_NE(nullptr, json_str.p_str);
    ASSERT_NE(nullptr, strstr(json_str.p_str, "\"mqtt_pass\":\t\"mqtt_pass_save\""));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    cjson_wrap_free_json_str(&json_str);
}

TEST_F(TestGwCfgRuuviJsonGenerate, gw_cfg_json_generate_for_saving_lan_auth_ruuvi_pass) // NOLINT
{
    gw_cfg_t         gw_cfg   = get_gateway_config_default();
    cjson_wrap_str_t json_str = cjson_wrap_str_null();

    gw_cfg.ruuvi_cfg.lan_auth.lan_auth_type = HTTP_SERVER_AUTH_TYPE_RUUVI;
    snprintf(
        gw_cfg.ruuvi_cfg.lan_auth.lan_auth_pass.buf,
        sizeof(gw_cfg.ruuvi_cfg.lan_auth.lan_auth_pass.buf),
        "ruuvi_pw");

    ASSERT_TRUE(gw_cfg_json_generate_for_saving(&gw_cfg, &json_str));
    ASSERT_NE(nullptr, json_str.p_str);
    ASSERT_NE(nullptr, strstr(json_str.p_str, "\"lan_auth_type\":\t\"lan_auth_ruuvi\""));
    ASSERT_NE(nullptr, strstr(json_str.p_str, "\"lan_auth_pass\":\t\"ruuvi_pw\""));
    // for_saving shows api_key directly, not _use booleans
    ASSERT_NE(nullptr, strstr(json_str.p_str, "\"lan_auth_api_key\""));
    ASSERT_NE(nullptr, strstr(json_str.p_str, "\"lan_auth_api_key_rw\""));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    cjson_wrap_free_json_str(&json_str);
}

TEST_F(TestGwCfgRuuviJsonGenerate, gw_cfg_json_generate_for_saving_remote_auth_basic) // NOLINT
{
    gw_cfg_t         gw_cfg   = get_gateway_config_default();
    cjson_wrap_str_t json_str = cjson_wrap_str_null();

    gw_cfg.ruuvi_cfg.remote.use_remote_cfg = true;
    gw_cfg.ruuvi_cfg.remote.auth_type      = GW_CFG_HTTP_AUTH_TYPE_BASIC;
    snprintf(gw_cfg.ruuvi_cfg.remote.url.buf, sizeof(gw_cfg.ruuvi_cfg.remote.url.buf), "https://remote.url");
    snprintf(
        gw_cfg.ruuvi_cfg.remote.auth.auth_basic.user.buf,
        sizeof(gw_cfg.ruuvi_cfg.remote.auth.auth_basic.user.buf),
        "r_user");
    snprintf(
        gw_cfg.ruuvi_cfg.remote.auth.auth_basic.password.buf,
        sizeof(gw_cfg.ruuvi_cfg.remote.auth.auth_basic.password.buf),
        "r_pass");

    ASSERT_TRUE(gw_cfg_json_generate_for_saving(&gw_cfg, &json_str));
    ASSERT_NE(nullptr, json_str.p_str);
    ASSERT_NE(nullptr, strstr(json_str.p_str, "\"remote_cfg_auth_type\":\t\"basic\""));
    ASSERT_NE(nullptr, strstr(json_str.p_str, "\"remote_cfg_auth_basic_user\":\t\"r_user\""));
    ASSERT_NE(nullptr, strstr(json_str.p_str, "\"remote_cfg_auth_basic_pass\":\t\"r_pass\""));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    cjson_wrap_free_json_str(&json_str);
}

TEST_F(TestGwCfgRuuviJsonGenerate, gw_cfg_json_generate_for_saving_remote_auth_bearer) // NOLINT
{
    gw_cfg_t         gw_cfg   = get_gateway_config_default();
    cjson_wrap_str_t json_str = cjson_wrap_str_null();

    gw_cfg.ruuvi_cfg.remote.use_remote_cfg = true;
    gw_cfg.ruuvi_cfg.remote.auth_type      = GW_CFG_HTTP_AUTH_TYPE_BEARER;
    snprintf(gw_cfg.ruuvi_cfg.remote.url.buf, sizeof(gw_cfg.ruuvi_cfg.remote.url.buf), "https://remote.url");
    snprintf(
        gw_cfg.ruuvi_cfg.remote.auth.auth_bearer.token.buf,
        sizeof(gw_cfg.ruuvi_cfg.remote.auth.auth_bearer.token.buf),
        "r_bearer_tok");

    ASSERT_TRUE(gw_cfg_json_generate_for_saving(&gw_cfg, &json_str));
    ASSERT_NE(nullptr, json_str.p_str);
    ASSERT_NE(nullptr, strstr(json_str.p_str, "\"remote_cfg_auth_type\":\t\"bearer\""));
    ASSERT_NE(nullptr, strstr(json_str.p_str, "\"remote_cfg_auth_bearer_token\":\t\"r_bearer_tok\""));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    cjson_wrap_free_json_str(&json_str);
}

TEST_F(TestGwCfgRuuviJsonGenerate, gw_cfg_json_generate_for_saving_remote_auth_token) // NOLINT
{
    gw_cfg_t         gw_cfg   = get_gateway_config_default();
    cjson_wrap_str_t json_str = cjson_wrap_str_null();

    gw_cfg.ruuvi_cfg.remote.use_remote_cfg = true;
    gw_cfg.ruuvi_cfg.remote.auth_type      = GW_CFG_HTTP_AUTH_TYPE_TOKEN;
    snprintf(gw_cfg.ruuvi_cfg.remote.url.buf, sizeof(gw_cfg.ruuvi_cfg.remote.url.buf), "https://remote.url");
    snprintf(
        gw_cfg.ruuvi_cfg.remote.auth.auth_token.token.buf,
        sizeof(gw_cfg.ruuvi_cfg.remote.auth.auth_token.token.buf),
        "r_token_val");

    ASSERT_TRUE(gw_cfg_json_generate_for_saving(&gw_cfg, &json_str));
    ASSERT_NE(nullptr, json_str.p_str);
    ASSERT_NE(nullptr, strstr(json_str.p_str, "\"remote_cfg_auth_type\":\t\"token\""));
    ASSERT_NE(nullptr, strstr(json_str.p_str, "\"remote_cfg_auth_token\":\t\"r_token_val\""));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    cjson_wrap_free_json_str(&json_str);
}

TEST_F(TestGwCfgRuuviJsonGenerate, gw_cfg_json_generate_for_saving_remote_auth_apikey) // NOLINT
{
    gw_cfg_t         gw_cfg   = get_gateway_config_default();
    cjson_wrap_str_t json_str = cjson_wrap_str_null();

    gw_cfg.ruuvi_cfg.remote.use_remote_cfg = true;
    gw_cfg.ruuvi_cfg.remote.auth_type      = GW_CFG_HTTP_AUTH_TYPE_APIKEY;
    snprintf(gw_cfg.ruuvi_cfg.remote.url.buf, sizeof(gw_cfg.ruuvi_cfg.remote.url.buf), "https://remote.url");
    snprintf(
        gw_cfg.ruuvi_cfg.remote.auth.auth_apikey.api_key.buf,
        sizeof(gw_cfg.ruuvi_cfg.remote.auth.auth_apikey.api_key.buf),
        "r_apikey_val");

    ASSERT_TRUE(gw_cfg_json_generate_for_saving(&gw_cfg, &json_str));
    ASSERT_NE(nullptr, json_str.p_str);
    ASSERT_NE(nullptr, strstr(json_str.p_str, "\"remote_cfg_auth_type\":\t\"api_key\""));
    ASSERT_NE(nullptr, strstr(json_str.p_str, "\"remote_cfg_auth_apikey\":\t\"r_apikey_val\""));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    cjson_wrap_free_json_str(&json_str);
}

TEST_F(TestGwCfgRuuviJsonGenerate, gw_cfg_json_generate_remote_auth_basic_hidden) // NOLINT
{
    gw_cfg_t         gw_cfg   = get_gateway_config_default();
    cjson_wrap_str_t json_str = cjson_wrap_str_null();

    gw_cfg.ruuvi_cfg.remote.use_remote_cfg = true;
    gw_cfg.ruuvi_cfg.remote.auth_type      = GW_CFG_HTTP_AUTH_TYPE_BASIC;
    snprintf(gw_cfg.ruuvi_cfg.remote.url.buf, sizeof(gw_cfg.ruuvi_cfg.remote.url.buf), "https://remote.url");
    snprintf(
        gw_cfg.ruuvi_cfg.remote.auth.auth_basic.user.buf,
        sizeof(gw_cfg.ruuvi_cfg.remote.auth.auth_basic.user.buf),
        "r_user");
    snprintf(
        gw_cfg.ruuvi_cfg.remote.auth.auth_basic.password.buf,
        sizeof(gw_cfg.ruuvi_cfg.remote.auth.auth_basic.password.buf),
        "r_pass");

    // gw_cfg_ruuvi_json_generate uses UI client mode (passwords hidden)
    ASSERT_TRUE(gw_cfg_ruuvi_json_generate(&gw_cfg, &json_str));
    ASSERT_NE(nullptr, json_str.p_str);
    ASSERT_NE(nullptr, strstr(json_str.p_str, "\"remote_cfg_auth_type\":\t\"basic\""));
    ASSERT_NE(nullptr, strstr(json_str.p_str, "\"remote_cfg_auth_basic_user\":\t\"r_user\""));
    // Password hidden in UI client mode
    ASSERT_EQ(nullptr, strstr(json_str.p_str, "remote_cfg_auth_basic_pass"));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    cjson_wrap_free_json_str(&json_str);
}

TEST_F(TestGwCfgRuuviJsonGenerate, gw_cfg_json_generate_remote_auth_bearer_hidden) // NOLINT
{
    gw_cfg_t         gw_cfg   = get_gateway_config_default();
    cjson_wrap_str_t json_str = cjson_wrap_str_null();

    gw_cfg.ruuvi_cfg.remote.use_remote_cfg = true;
    gw_cfg.ruuvi_cfg.remote.auth_type      = GW_CFG_HTTP_AUTH_TYPE_BEARER;
    snprintf(gw_cfg.ruuvi_cfg.remote.url.buf, sizeof(gw_cfg.ruuvi_cfg.remote.url.buf), "https://remote.url");
    snprintf(
        gw_cfg.ruuvi_cfg.remote.auth.auth_bearer.token.buf,
        sizeof(gw_cfg.ruuvi_cfg.remote.auth.auth_bearer.token.buf),
        "r_bearer_tok");

    ASSERT_TRUE(gw_cfg_ruuvi_json_generate(&gw_cfg, &json_str));
    ASSERT_NE(nullptr, json_str.p_str);
    ASSERT_NE(nullptr, strstr(json_str.p_str, "\"remote_cfg_auth_type\":\t\"bearer\""));
    ASSERT_EQ(nullptr, strstr(json_str.p_str, "remote_cfg_auth_bearer_token"));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    cjson_wrap_free_json_str(&json_str);
}

TEST_F(TestGwCfgRuuviJsonGenerate, gw_cfg_json_generate_remote_auth_token_hidden) // NOLINT
{
    gw_cfg_t         gw_cfg   = get_gateway_config_default();
    cjson_wrap_str_t json_str = cjson_wrap_str_null();

    gw_cfg.ruuvi_cfg.remote.use_remote_cfg = true;
    gw_cfg.ruuvi_cfg.remote.auth_type      = GW_CFG_HTTP_AUTH_TYPE_TOKEN;
    snprintf(gw_cfg.ruuvi_cfg.remote.url.buf, sizeof(gw_cfg.ruuvi_cfg.remote.url.buf), "https://remote.url");
    snprintf(
        gw_cfg.ruuvi_cfg.remote.auth.auth_token.token.buf,
        sizeof(gw_cfg.ruuvi_cfg.remote.auth.auth_token.token.buf),
        "r_token_val");

    ASSERT_TRUE(gw_cfg_ruuvi_json_generate(&gw_cfg, &json_str));
    ASSERT_NE(nullptr, json_str.p_str);
    ASSERT_NE(nullptr, strstr(json_str.p_str, "\"remote_cfg_auth_type\":\t\"token\""));
    ASSERT_EQ(nullptr, strstr(json_str.p_str, "remote_cfg_auth_token"));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    cjson_wrap_free_json_str(&json_str);
}

TEST_F(TestGwCfgRuuviJsonGenerate, gw_cfg_json_generate_remote_auth_apikey_hidden) // NOLINT
{
    gw_cfg_t         gw_cfg   = get_gateway_config_default();
    cjson_wrap_str_t json_str = cjson_wrap_str_null();

    gw_cfg.ruuvi_cfg.remote.use_remote_cfg = true;
    gw_cfg.ruuvi_cfg.remote.auth_type      = GW_CFG_HTTP_AUTH_TYPE_APIKEY;
    snprintf(gw_cfg.ruuvi_cfg.remote.url.buf, sizeof(gw_cfg.ruuvi_cfg.remote.url.buf), "https://remote.url");
    snprintf(
        gw_cfg.ruuvi_cfg.remote.auth.auth_apikey.api_key.buf,
        sizeof(gw_cfg.ruuvi_cfg.remote.auth.auth_apikey.api_key.buf),
        "r_apikey_val");

    ASSERT_TRUE(gw_cfg_ruuvi_json_generate(&gw_cfg, &json_str));
    ASSERT_NE(nullptr, json_str.p_str);
    ASSERT_NE(nullptr, strstr(json_str.p_str, "\"remote_cfg_auth_type\":\t\"api_key\""));
    ASSERT_EQ(nullptr, strstr(json_str.p_str, "remote_cfg_auth_apikey"));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    cjson_wrap_free_json_str(&json_str);
}

TEST_F(TestGwCfgRuuviJsonGenerate, gw_cfg_json_generate_for_saving_http_decoded_format) // NOLINT
{
    gw_cfg_t         gw_cfg   = get_gateway_config_default();
    cjson_wrap_str_t json_str = cjson_wrap_str_null();

    gw_cfg.ruuvi_cfg.http.use_http_ruuvi = false;
    gw_cfg.ruuvi_cfg.http.use_http       = true;
    gw_cfg.ruuvi_cfg.http.auth_type      = GW_CFG_HTTP_AUTH_TYPE_NONE;
    gw_cfg.ruuvi_cfg.http.data_format    = GW_CFG_HTTP_DATA_FORMAT_RUUVI_DECODED;
    snprintf(gw_cfg.ruuvi_cfg.http.http_url.buf, sizeof(gw_cfg.ruuvi_cfg.http.http_url.buf), "https://custom.url");

    ASSERT_TRUE(gw_cfg_json_generate_for_saving(&gw_cfg, &json_str));
    ASSERT_NE(nullptr, json_str.p_str);
    ASSERT_NE(nullptr, strstr(json_str.p_str, "\"http_data_format\":\t\"ruuvi_decoded\""));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    cjson_wrap_free_json_str(&json_str);
}

TEST_F(TestGwCfgRuuviJsonGenerate, gw_cfg_json_generate_for_saving_mqtt_raw_and_decoded) // NOLINT
{
    gw_cfg_t         gw_cfg   = get_gateway_config_default();
    cjson_wrap_str_t json_str = cjson_wrap_str_null();

    gw_cfg.ruuvi_cfg.mqtt.mqtt_data_format = GW_CFG_MQTT_DATA_FORMAT_RUUVI_RAW_AND_DECODED;

    ASSERT_TRUE(gw_cfg_json_generate_for_saving(&gw_cfg, &json_str));
    ASSERT_NE(nullptr, json_str.p_str);
    ASSERT_NE(nullptr, strstr(json_str.p_str, "\"mqtt_data_format\":\t\"ruuvi_raw_and_decoded\""));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    cjson_wrap_free_json_str(&json_str);
}

TEST_F(TestGwCfgRuuviJsonGenerate, gw_cfg_json_generate_for_saving_mqtt_decoded) // NOLINT
{
    gw_cfg_t         gw_cfg   = get_gateway_config_default();
    cjson_wrap_str_t json_str = cjson_wrap_str_null();

    gw_cfg.ruuvi_cfg.mqtt.mqtt_data_format = GW_CFG_MQTT_DATA_FORMAT_RUUVI_DECODED;

    ASSERT_TRUE(gw_cfg_json_generate_for_saving(&gw_cfg, &json_str));
    ASSERT_NE(nullptr, json_str.p_str);
    ASSERT_NE(nullptr, strstr(json_str.p_str, "\"mqtt_data_format\":\t\"ruuvi_decoded\""));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    cjson_wrap_free_json_str(&json_str);
}

TEST_F(TestGwCfgRuuviJsonGenerate, gw_cfg_json_generate_http_use_default_url_with_use_http_true) // NOLINT
{
    gw_cfg_t         gw_cfg   = get_gateway_config_default();
    cjson_wrap_str_t json_str = cjson_wrap_str_null();

    // use_http=true, data_format=RUUVI, auth=NONE, http_url=default → falls back to default
    gw_cfg.ruuvi_cfg.http.use_http_ruuvi = false;
    gw_cfg.ruuvi_cfg.http.use_http       = true;
    gw_cfg.ruuvi_cfg.http.auth_type      = GW_CFG_HTTP_AUTH_TYPE_NONE;
    gw_cfg.ruuvi_cfg.http.data_format    = GW_CFG_HTTP_DATA_FORMAT_RUUVI;
    snprintf(
        gw_cfg.ruuvi_cfg.http.http_url.buf,
        sizeof(gw_cfg.ruuvi_cfg.http.http_url.buf),
        "%s",
        RUUVI_GATEWAY_HTTP_DEFAULT_URL);

    ASSERT_TRUE(gw_cfg_json_generate_for_saving(&gw_cfg, &json_str));
    ASSERT_NE(nullptr, json_str.p_str);
    // Should be patched to default: use_http_ruuvi=true, use_http=true
    ASSERT_NE(nullptr, strstr(json_str.p_str, "\"use_http_ruuvi\":\ttrue"));
    ASSERT_NE(nullptr, strstr(json_str.p_str, "\"use_http\":\ttrue"));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    cjson_wrap_free_json_str(&json_str);
}

TEST_F(TestGwCfgRuuviJsonGenerate, gw_cfg_json_generate_for_saving_http_custom_all_ssl_and_extra) // NOLINT
{
    gw_cfg_t         gw_cfg   = get_gateway_config_default();
    cjson_wrap_str_t json_str = cjson_wrap_str_null();

    gw_cfg.ruuvi_cfg.http.use_http_ruuvi              = false;
    gw_cfg.ruuvi_cfg.http.use_http                    = true;
    gw_cfg.ruuvi_cfg.http.http_use_ssl_client_cert    = true;
    gw_cfg.ruuvi_cfg.http.http_use_ssl_server_cert    = true;
    gw_cfg.ruuvi_cfg.http.http_use_extra_http_path    = true;
    gw_cfg.ruuvi_cfg.http.http_use_extra_http_query   = true;
    gw_cfg.ruuvi_cfg.http.http_use_extra_http_headers = true;
    gw_cfg.ruuvi_cfg.http.auth_type                   = GW_CFG_HTTP_AUTH_TYPE_NONE;
    gw_cfg.ruuvi_cfg.http.data_format                 = GW_CFG_HTTP_DATA_FORMAT_RUUVI_RAW_AND_DECODED;
    gw_cfg.ruuvi_cfg.http.http_period                 = 30;
    snprintf(gw_cfg.ruuvi_cfg.http.http_url.buf, sizeof(gw_cfg.ruuvi_cfg.http.http_url.buf), "https://custom.url");

    ASSERT_TRUE(gw_cfg_json_generate_for_saving(&gw_cfg, &json_str));
    ASSERT_NE(nullptr, json_str.p_str);
    ASSERT_NE(nullptr, strstr(json_str.p_str, "\"http_use_ssl_client_cert\":\ttrue"));
    ASSERT_NE(nullptr, strstr(json_str.p_str, "\"http_use_ssl_server_cert\":\ttrue"));
    ASSERT_NE(nullptr, strstr(json_str.p_str, "\"http_use_extra_http_path\":\ttrue"));
    ASSERT_NE(nullptr, strstr(json_str.p_str, "\"http_use_extra_http_query\":\ttrue"));
    ASSERT_NE(nullptr, strstr(json_str.p_str, "\"http_use_extra_http_headers\":\ttrue"));
    ASSERT_NE(nullptr, strstr(json_str.p_str, "\"http_period\":\t30"));
    ASSERT_NE(nullptr, strstr(json_str.p_str, "\"http_data_format\":\t\"ruuvi_raw_and_decoded\""));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    cjson_wrap_free_json_str(&json_str);
}

TEST_F(TestGwCfgRuuviJsonGenerate, gw_cfg_json_generate_for_saving_lan_auth_allow_no_pass) // NOLINT
{
    gw_cfg_t         gw_cfg   = get_gateway_config_default();
    cjson_wrap_str_t json_str = cjson_wrap_str_null();

    gw_cfg.ruuvi_cfg.lan_auth.lan_auth_type = HTTP_SERVER_AUTH_TYPE_ALLOW;
    snprintf(
        gw_cfg.ruuvi_cfg.lan_auth.lan_auth_pass.buf,
        sizeof(gw_cfg.ruuvi_cfg.lan_auth.lan_auth_pass.buf),
        "some_pass");

    ASSERT_TRUE(gw_cfg_json_generate_for_saving(&gw_cfg, &json_str));
    ASSERT_NE(nullptr, json_str.p_str);
    ASSERT_NE(nullptr, strstr(json_str.p_str, "\"lan_auth_type\":\t\"lan_auth_allow\""));
    // ALLOW type does not save the password
    ASSERT_EQ(nullptr, strstr(json_str.p_str, "lan_auth_pass"));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    cjson_wrap_free_json_str(&json_str);
}
