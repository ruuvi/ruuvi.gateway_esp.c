/**
 * @file test_gw_cfg_json.cpp
 * @author TheSomeMan
 * @date 2021-10-08
 * @copyright Ruuvi Innovations Ltd, license BSD-3-Clause.
 */

#include "json_ruuvi.h"
#include <cstring>
#include "gtest/gtest.h"
#include "gw_cfg.h"
#include "gw_cfg_default.h"
#include "gw_cfg_json_parse.h"
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

class TestGwCfgJson;
static TestGwCfgJson* g_pTestClass;

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

class TestGwCfgJson : public ::testing::Test
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
        this->m_flag_storage_ready = false;

        const gw_cfg_default_init_param_t init_params = {
            .wifi_ap_ssid        = { "my_ssid1" },
            .hostname            = { "my_ssid1" },
            .device_id           = { 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88 },
            .esp32_fw_ver        = { "v1.10.0" },
            .nrf52_fw_ver        = { "v0.7.2" },
            .nrf52_mac_addr      = { 0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF },
            .esp32_mac_addr_wifi = { 0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0x11 },
            .esp32_mac_addr_eth  = { 0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0x22 },
        };
        gw_cfg_default_init(&init_params, nullptr);
        gw_cfg_default_log();
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
    TestGwCfgJson();

    ~TestGwCfgJson() override;

    MemAllocTrace m_mem_alloc_trace;
    uint32_t      m_malloc_cnt { 0 };
    uint32_t      m_malloc_fail_on_cnt { 0 };
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
};

TestGwCfgJson::TestGwCfgJson()
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
gw_cfg_storage_check_file(const char* const p_file_name)
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
    return false;
}

} // extern "C"

TestGwCfgJson::~TestGwCfgJson() = default;

#define TEST_CHECK_LOG_RECORD(level_, msg_) ESP_LOG_WRAPPER_TEST_CHECK_LOG_RECORD("gw_cfg", level_, msg_)

static gw_cfg_t
get_gateway_config_default()
{
    gw_cfg_t gw_cfg {};
    gw_cfg_default_get(&gw_cfg);
    return gw_cfg;
}

static gw_cfg_t
get_gateway_config_default_lan_auth_ruuvi()
{
    gw_cfg_t gw_cfg {};
    gw_cfg_default_get(&gw_cfg);
    gw_cfg.ruuvi_cfg.lan_auth.lan_auth_type = HTTP_SERVER_AUTH_TYPE_RUUVI;
    (void)snprintf(
        gw_cfg.ruuvi_cfg.lan_auth.lan_auth_pass.buf,
        sizeof(gw_cfg.ruuvi_cfg.lan_auth.lan_auth_pass.buf),
        "non_default_pass");
    return gw_cfg;
}

/*** Unit-Tests
 * *******************************************************************************************************/

TEST_F(TestGwCfgJson, copy_string_val_ok) // NOLINT
{
    cJSON* root = cJSON_CreateObject();
    ASSERT_NE(nullptr, root);
    cJSON_AddStringToObject(root, "attr", "value123");
    char buf[80];
    ASSERT_TRUE(gw_cfg_json_copy_string_val(root, "attr", buf, sizeof(buf)));
    ASSERT_EQ(string("value123"), string(buf));

    cJSON_Delete(root);
    //    TEST_CHECK_LOG_RECORD(ESP_LOG_DEBUG, "attr: value123");
    ASSERT_TRUE(esp_log_wrapper_is_empty());
}

TEST_F(TestGwCfgJson, copy_string_val_failed) // NOLINT
{
    cJSON* root = cJSON_CreateObject();
    ASSERT_NE(nullptr, root);
    cJSON_AddStringToObject(root, "attr", "value123");
    char buf[80];
    ASSERT_FALSE(gw_cfg_json_copy_string_val(root, "attr2", buf, sizeof(buf)));
    cJSON_Delete(root);
    //    TEST_CHECK_LOG_RECORD(ESP_LOG_DEBUG, "attr2: not found");
    ASSERT_TRUE(esp_log_wrapper_is_empty());
}

TEST_F(TestGwCfgJson, get_bool_val_ok) // NOLINT
{
    cJSON* root = cJSON_CreateObject();
    ASSERT_NE(nullptr, root);
    cJSON_AddBoolToObject(root, "attr", true);
    bool val = false;
    ASSERT_TRUE(gw_cfg_json_get_bool_val(root, "attr", &val));
    ASSERT_TRUE(val);
    cJSON_Delete(root);
    //    TEST_CHECK_LOG_RECORD(ESP_LOG_DEBUG, "attr: 1");
    ASSERT_TRUE(esp_log_wrapper_is_empty());
}

TEST_F(TestGwCfgJson, get_bool_val_failed) // NOLINT
{
    cJSON* root = cJSON_CreateObject();
    ASSERT_NE(nullptr, root);
    cJSON_AddBoolToObject(root, "attr", true);
    bool val = false;
    ASSERT_FALSE(gw_cfg_json_get_bool_val(root, "attr2", &val));
    cJSON_Delete(root);
    //    TEST_CHECK_LOG_RECORD(ESP_LOG_DEBUG, "attr2: not found");
    ASSERT_TRUE(esp_log_wrapper_is_empty());
}

TEST_F(TestGwCfgJson, get_uint16_val_ok) // NOLINT
{
    cJSON* root = cJSON_CreateObject();
    ASSERT_NE(nullptr, root);
    cJSON_AddNumberToObject(root, "attr", 123.0);
    uint16_t val = 0;
    ASSERT_TRUE(gw_cfg_json_get_uint16_val(root, "attr", &val));
    ASSERT_EQ(123, val);
    cJSON_Delete(root);
    //    TEST_CHECK_LOG_RECORD(ESP_LOG_DEBUG, "attr: 123");
    ASSERT_TRUE(esp_log_wrapper_is_empty());
}

TEST_F(TestGwCfgJson, get_uint16_val_failed) // NOLINT
{
    cJSON* root = cJSON_CreateObject();
    ASSERT_NE(nullptr, root);
    cJSON_AddNumberToObject(root, "attr", 123.0);
    uint16_t val = 0;
    ASSERT_FALSE(gw_cfg_json_get_uint16_val(root, "attr2", &val));
    cJSON_Delete(root);
    //    TEST_CHECK_LOG_RECORD(ESP_LOG_DEBUG, "attr2: not found or invalid");
    ASSERT_TRUE(esp_log_wrapper_is_empty());
}

TEST_F(TestGwCfgJson, gw_cfg_json_generate_for_ui_client) // NOLINT
{
    const gw_cfg_t   gw_cfg   = get_gateway_config_default();
    cjson_wrap_str_t json_str = cjson_wrap_str_null();

    ASSERT_TRUE(gw_cfg_json_generate_for_ui_client(&gw_cfg, &json_str));
    ASSERT_NE(nullptr, json_str.p_str);
    ASSERT_EQ(
        string("{\n"
               "\t\"fw_ver\":\t\"v1.10.0\",\n"
               "\t\"nrf52_fw_ver\":\t\"v0.7.2\",\n"
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
               "\t\"coordinates\":\t\"\",\n"
               "\t\"fw_update_url\":\t\"https://network.ruuvi.com/firmwareupdate\"\n"
               "}"),
        string(json_str.p_str));
    ASSERT_TRUE(esp_log_wrapper_is_empty());

    gw_cfg_t gw_cfg2 = get_gateway_config_default();
    ASSERT_TRUE(gw_cfg_json_parse("my.json", nullptr, json_str.p_str, &gw_cfg2));
    cjson_wrap_free_json_str(&json_str);

    ASSERT_TRUE(0 == memcmp(&gw_cfg, &gw_cfg2, sizeof(gw_cfg)));
}

TEST_F(TestGwCfgJson, gw_cfg_json_generate_default) // NOLINT
{
    const gw_cfg_t   gw_cfg   = get_gateway_config_default();
    cjson_wrap_str_t json_str = cjson_wrap_str_null();

    ASSERT_TRUE(gw_cfg_json_generate_for_saving(&gw_cfg, &json_str));
    ASSERT_NE(nullptr, json_str.p_str);
    ASSERT_EQ(
        string("{\n"
               "\t\"wifi_sta_config\":\t{\n"
               "\t\t\"ssid\":\t\"\",\n"
               "\t\t\"password\":\t\"\"\n"
               "\t},\n"
               "\t\"wifi_ap_config\":\t{\n"
               "\t\t\"password\":\t\"\",\n"
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
               "\t\"http_stat_pass\":\t\"\",\n"
               "\t\"http_stat_use_ssl_client_cert\":\tfalse,\n"
               "\t\"http_stat_use_ssl_server_cert\":\tfalse,\n"
               "\t\"use_mqtt\":\tfalse,\n"
               "\t\"mqtt_disable_retained_messages\":\tfalse,\n"
               "\t\"mqtt_transport\":\t\"TCP\",\n"
               "\t\"mqtt_server\":\t\"test.mosquitto.org\",\n"
               "\t\"mqtt_port\":\t1883,\n"
               "\t\"mqtt_sending_interval\":\t0,\n"
               "\t\"mqtt_prefix\":\t\"ruuvi/AA:BB:CC:DD:EE:FF/\",\n"
               "\t\"mqtt_client_id\":\t\"AA:BB:CC:DD:EE:FF\",\n"
               "\t\"mqtt_user\":\t\"\",\n"
               "\t\"mqtt_pass\":\t\"\",\n"
               "\t\"mqtt_use_ssl_client_cert\":\tfalse,\n"
               "\t\"mqtt_use_ssl_server_cert\":\tfalse,\n"
               "\t\"lan_auth_type\":\t\"lan_auth_default\",\n"
               "\t\"lan_auth_user\":\t\"Admin\",\n"
               "\t\"lan_auth_api_key\":\t\"\",\n"
               "\t\"lan_auth_api_key_rw\":\t\"\",\n"
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
               "\t\"coordinates\":\t\"\",\n"
               "\t\"fw_update_url\":\t\"https://network.ruuvi.com/firmwareupdate\"\n"
               "}"),
        string(json_str.p_str));
    ASSERT_TRUE(esp_log_wrapper_is_empty());

    gw_cfg_t gw_cfg2 = get_gateway_config_default();
    ASSERT_TRUE(gw_cfg_json_parse("my.json", nullptr, json_str.p_str, &gw_cfg2));
    cjson_wrap_free_json_str(&json_str);

    ASSERT_TRUE(0 == memcmp(&gw_cfg, &gw_cfg2, sizeof(gw_cfg)));
}

TEST_F(TestGwCfgJson, gw_cfg_json_parse_default_company_id_0x0500) // NOLINT
{
    gw_cfg_t gw_cfg                    = get_gateway_config_default();
    gw_cfg.ruuvi_cfg.filter.company_id = 0x0500;

    string json_content = string(
        "{\n"
        "\t\"use_eth\":\ttrue,\n"
        "\t\"eth_dhcp\":\ttrue,\n"
        "\t\"eth_static_ip\":\t\"\",\n"
        "\t\"eth_netmask\":\t\"\",\n"
        "\t\"eth_gw\":\t\"\",\n"
        "\t\"eth_dns1\":\t\"\",\n"
        "\t\"eth_dns2\":\t\"\",\n"
        "\t\"use_http\":\ttrue,\n"
        "\t\"http_url\":\t\"" RUUVI_GATEWAY_HTTP_DEFAULT_URL
        "\",\n"
        "\t\"http_user\":\t\"\",\n"
        "\t\"http_pass\":\t\"\",\n"
        "\t\"use_http_stat\":\ttrue,\n"
        "\t\"http_stat_url\":\t\"" RUUVI_GATEWAY_HTTP_STATUS_URL
        "\",\n"
        "\t\"http_stat_user\":\t\"\",\n"
        "\t\"http_stat_pass\":\t\"\",\n"
        "\t\"http_stat_use_ssl_client_cert\":\tfalse,\n"
        "\t\"http_stat_use_ssl_server_cert\":\tfalse,\n"
        "\t\"use_mqtt\":\tfalse,\n"
        "\t\"mqtt_disable_retained_messages\":\tfalse,\n"
        "\t\"mqtt_transport\":\t\"TCP\",\n"
        "\t\"mqtt_server\":\t\"test.mosquitto.org\",\n"
        "\t\"mqtt_port\":\t1883,\n"
        "\t\"mqtt_sending_interval\":\t0,\n"
        "\t\"mqtt_prefix\":\t\"ruuvi/AA:BB:CC:DD:EE:FF/\",\n"
        "\t\"mqtt_client_id\":\t\"AA:BB:CC:DD:EE:FF\",\n"
        "\t\"mqtt_user\":\t\"\",\n"
        "\t\"mqtt_pass\":\t\"\",\n"
        "\t\"mqtt_use_ssl_client_cert\":\tfalse,\n"
        "\t\"mqtt_use_ssl_server_cert\":\tfalse,\n"
        "\t\"lan_auth_type\":\t\"lan_auth_default\",\n"
        "\t\"lan_auth_user\":\t\"Admin\",\n"
        "\t\"lan_auth_api_key\":\t\"\",\n"
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
        "\t\"company_id\":\t\"0x0500\",\n"
        "\t\"company_use_filtering\":\ttrue,\n"
        "\t\"scan_coded_phy\":\tfalse,\n"
        "\t\"scan_1mbit_phy\":\ttrue,\n"
        "\t\"scan_extended_payload\":\ttrue,\n"
        "\t\"scan_channel_37\":\ttrue,\n"
        "\t\"scan_channel_38\":\ttrue,\n"
        "\t\"scan_channel_39\":\ttrue,\n"
        "\t\"scan_filter_allow_listed\":\tfalse,\n"
        "\t\"scan_filter_list\":\t[],\n"
        "\t\"coordinates\":\t\"\",\n"
        "\t\"fw_update_url\":\t\"https://network.ruuvi.com/firmwareupdate\"\n"
        "}");
    cjson_wrap_str_t json_str = cjson_wrap_str_null();
    json_str.p_str            = json_content.c_str();

    gw_cfg_t gw_cfg2 = get_gateway_config_default();
    ASSERT_TRUE(gw_cfg_json_parse("my.json", nullptr, json_str.p_str, &gw_cfg2));

    ASSERT_EQ(gw_cfg2.ruuvi_cfg.filter.company_id, gw_cfg.ruuvi_cfg.filter.company_id);
    ASSERT_TRUE(0 == memcmp(&gw_cfg, &gw_cfg2, sizeof(gw_cfg)));
}

TEST_F(TestGwCfgJson, gw_cfg_json_generate_eth_disabled) // NOLINT
{
    gw_cfg_t         gw_cfg   = get_gateway_config_default();
    cjson_wrap_str_t json_str = cjson_wrap_str_null();

    gw_cfg.eth_cfg.use_eth = false;

    ASSERT_TRUE(gw_cfg_json_generate_for_saving(&gw_cfg, &json_str));
    ASSERT_NE(nullptr, json_str.p_str);
    ASSERT_EQ(
        string("{\n"
               "\t\"wifi_sta_config\":\t{\n"
               "\t\t\"ssid\":\t\"\",\n"
               "\t\t\"password\":\t\"\"\n"
               "\t},\n"
               "\t\"wifi_ap_config\":\t{\n"
               "\t\t\"password\":\t\"\",\n"
               "\t\t\"channel\":\t1\n"
               "\t},\n"
               "\t\"use_eth\":\tfalse,\n"
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
               "\t\"http_stat_pass\":\t\"\",\n"
               "\t\"http_stat_use_ssl_client_cert\":\tfalse,\n"
               "\t\"http_stat_use_ssl_server_cert\":\tfalse,\n"
               "\t\"use_mqtt\":\tfalse,\n"
               "\t\"mqtt_disable_retained_messages\":\tfalse,\n"
               "\t\"mqtt_transport\":\t\"TCP\",\n"
               "\t\"mqtt_server\":\t\"test.mosquitto.org\",\n"
               "\t\"mqtt_port\":\t1883,\n"
               "\t\"mqtt_sending_interval\":\t0,\n"
               "\t\"mqtt_prefix\":\t\"ruuvi/AA:BB:CC:DD:EE:FF/\",\n"
               "\t\"mqtt_client_id\":\t\"AA:BB:CC:DD:EE:FF\",\n"
               "\t\"mqtt_user\":\t\"\",\n"
               "\t\"mqtt_pass\":\t\"\",\n"
               "\t\"mqtt_use_ssl_client_cert\":\tfalse,\n"
               "\t\"mqtt_use_ssl_server_cert\":\tfalse,\n"
               "\t\"lan_auth_type\":\t\"lan_auth_default\",\n"
               "\t\"lan_auth_user\":\t\"Admin\",\n"
               "\t\"lan_auth_api_key\":\t\"\",\n"
               "\t\"lan_auth_api_key_rw\":\t\"\",\n"
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
               "\t\"coordinates\":\t\"\",\n"
               "\t\"fw_update_url\":\t\"https://network.ruuvi.com/firmwareupdate\"\n"
               "}"),
        string(json_str.p_str));
    ASSERT_TRUE(esp_log_wrapper_is_empty());

    gw_cfg_t gw_cfg2 = get_gateway_config_default();
    ASSERT_TRUE(gw_cfg_json_parse("my.json", nullptr, json_str.p_str, &gw_cfg2));
    cjson_wrap_free_json_str(&json_str);

    ASSERT_TRUE(0 == memcmp(&gw_cfg, &gw_cfg2, sizeof(gw_cfg)));
}

TEST_F(TestGwCfgJson, gw_cfg_json_generate_eth_enabled_dhcp_enabled) // NOLINT
{
    gw_cfg_t         gw_cfg   = get_gateway_config_default();
    cjson_wrap_str_t json_str = cjson_wrap_str_null();

    gw_cfg.eth_cfg.use_eth  = true;
    gw_cfg.eth_cfg.eth_dhcp = true;

    ASSERT_TRUE(gw_cfg_json_generate_for_saving(&gw_cfg, &json_str));
    ASSERT_NE(nullptr, json_str.p_str);
    ASSERT_EQ(
        string("{\n"
               "\t\"wifi_sta_config\":\t{\n"
               "\t\t\"ssid\":\t\"\",\n"
               "\t\t\"password\":\t\"\"\n"
               "\t},\n"
               "\t\"wifi_ap_config\":\t{\n"
               "\t\t\"password\":\t\"\",\n"
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
               "\t\"http_stat_pass\":\t\"\",\n"
               "\t\"http_stat_use_ssl_client_cert\":\tfalse,\n"
               "\t\"http_stat_use_ssl_server_cert\":\tfalse,\n"
               "\t\"use_mqtt\":\tfalse,\n"
               "\t\"mqtt_disable_retained_messages\":\tfalse,\n"
               "\t\"mqtt_transport\":\t\"TCP\",\n"
               "\t\"mqtt_server\":\t\"test.mosquitto.org\",\n"
               "\t\"mqtt_port\":\t1883,\n"
               "\t\"mqtt_sending_interval\":\t0,\n"
               "\t\"mqtt_prefix\":\t\"ruuvi/AA:BB:CC:DD:EE:FF/\",\n"
               "\t\"mqtt_client_id\":\t\"AA:BB:CC:DD:EE:FF\",\n"
               "\t\"mqtt_user\":\t\"\",\n"
               "\t\"mqtt_pass\":\t\"\",\n"
               "\t\"mqtt_use_ssl_client_cert\":\tfalse,\n"
               "\t\"mqtt_use_ssl_server_cert\":\tfalse,\n"
               "\t\"lan_auth_type\":\t\"lan_auth_default\",\n"
               "\t\"lan_auth_user\":\t\"Admin\",\n"
               "\t\"lan_auth_api_key\":\t\"\",\n"
               "\t\"lan_auth_api_key_rw\":\t\"\",\n"
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
               "\t\"coordinates\":\t\"\",\n"
               "\t\"fw_update_url\":\t\"https://network.ruuvi.com/firmwareupdate\"\n"
               "}"),
        string(json_str.p_str));
    ASSERT_TRUE(esp_log_wrapper_is_empty());

    gw_cfg_t gw_cfg2 = get_gateway_config_default();
    ASSERT_TRUE(gw_cfg_json_parse("my.json", nullptr, json_str.p_str, &gw_cfg2));
    cjson_wrap_free_json_str(&json_str);

    ASSERT_TRUE(0 == memcmp(&gw_cfg, &gw_cfg2, sizeof(gw_cfg)));
}

TEST_F(TestGwCfgJson, gw_cfg_json_generate_eth_enabled_dhcp_disabled) // NOLINT
{
    gw_cfg_t         gw_cfg   = get_gateway_config_default();
    cjson_wrap_str_t json_str = cjson_wrap_str_null();

    gw_cfg.eth_cfg.use_eth  = true;
    gw_cfg.eth_cfg.eth_dhcp = false;
    snprintf(gw_cfg.eth_cfg.eth_static_ip.buf, sizeof(gw_cfg.eth_cfg.eth_static_ip.buf), "192.168.1.10");
    snprintf(gw_cfg.eth_cfg.eth_netmask.buf, sizeof(gw_cfg.eth_cfg.eth_netmask.buf), "255.255.255.0");
    snprintf(gw_cfg.eth_cfg.eth_gw.buf, sizeof(gw_cfg.eth_cfg.eth_gw.buf), "192.168.1.1");
    snprintf(gw_cfg.eth_cfg.eth_dns1.buf, sizeof(gw_cfg.eth_cfg.eth_dns1.buf), "8.8.8.8");
    snprintf(gw_cfg.eth_cfg.eth_dns2.buf, sizeof(gw_cfg.eth_cfg.eth_dns2.buf), "4.4.4.4");

    ASSERT_TRUE(gw_cfg_json_generate_for_saving(&gw_cfg, &json_str));
    ASSERT_NE(nullptr, json_str.p_str);
    ASSERT_EQ(
        string("{\n"
               "\t\"wifi_sta_config\":\t{\n"
               "\t\t\"ssid\":\t\"\",\n"
               "\t\t\"password\":\t\"\"\n"
               "\t},\n"
               "\t\"wifi_ap_config\":\t{\n"
               "\t\t\"password\":\t\"\",\n"
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
               "\t\"use_http_ruuvi\":\ttrue,\n"
               "\t\"use_http\":\ttrue,\n"
               "\t\"http_url\":\t\"" RUUVI_GATEWAY_HTTP_DEFAULT_URL "\",\n"
               "\t\"http_data_format\":\t\"ruuvi\",\n"
               "\t\"http_auth\":\t\"none\",\n"
               "\t\"use_http_stat\":\ttrue,\n"
               "\t\"http_stat_url\":\t\"" RUUVI_GATEWAY_HTTP_STATUS_URL "\",\n"
               "\t\"http_stat_user\":\t\"\",\n"
               "\t\"http_stat_pass\":\t\"\",\n"
               "\t\"http_stat_use_ssl_client_cert\":\tfalse,\n"
               "\t\"http_stat_use_ssl_server_cert\":\tfalse,\n"
               "\t\"use_mqtt\":\tfalse,\n"
               "\t\"mqtt_disable_retained_messages\":\tfalse,\n"
               "\t\"mqtt_transport\":\t\"TCP\",\n"
               "\t\"mqtt_server\":\t\"test.mosquitto.org\",\n"
               "\t\"mqtt_port\":\t1883,\n"
               "\t\"mqtt_sending_interval\":\t0,\n"
               "\t\"mqtt_prefix\":\t\"ruuvi/AA:BB:CC:DD:EE:FF/\",\n"
               "\t\"mqtt_client_id\":\t\"AA:BB:CC:DD:EE:FF\",\n"
               "\t\"mqtt_user\":\t\"\",\n"
               "\t\"mqtt_pass\":\t\"\",\n"
               "\t\"mqtt_use_ssl_client_cert\":\tfalse,\n"
               "\t\"mqtt_use_ssl_server_cert\":\tfalse,\n"
               "\t\"lan_auth_type\":\t\"lan_auth_default\",\n"
               "\t\"lan_auth_user\":\t\"Admin\",\n"
               "\t\"lan_auth_api_key\":\t\"\",\n"
               "\t\"lan_auth_api_key_rw\":\t\"\",\n"
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
               "\t\"coordinates\":\t\"\",\n"
               "\t\"fw_update_url\":\t\"https://network.ruuvi.com/firmwareupdate\"\n"
               "}"),
        string(json_str.p_str));
    ASSERT_TRUE(esp_log_wrapper_is_empty());

    gw_cfg_t gw_cfg2 = get_gateway_config_default();
    ASSERT_TRUE(gw_cfg_json_parse("my.json", nullptr, json_str.p_str, &gw_cfg2));
    cjson_wrap_free_json_str(&json_str);

    ASSERT_TRUE(0 == memcmp(&gw_cfg, &gw_cfg2, sizeof(gw_cfg)));
}

TEST_F(TestGwCfgJson, gw_cfg_json_generate_remote_cfg_enabled_auth_no) // NOLINT
{
    gw_cfg_t         gw_cfg   = get_gateway_config_default();
    cjson_wrap_str_t json_str = cjson_wrap_str_null();

    gw_cfg.ruuvi_cfg.remote.use_remote_cfg = true;
    snprintf(gw_cfg.ruuvi_cfg.remote.url.buf, sizeof(gw_cfg.ruuvi_cfg.remote.url.buf), "http://my_server1.com");
    gw_cfg.ruuvi_cfg.remote.auth_type                = GW_CFG_HTTP_AUTH_TYPE_NONE;
    gw_cfg.ruuvi_cfg.remote.refresh_interval_minutes = 10;

    ASSERT_TRUE(gw_cfg_json_generate_for_saving(&gw_cfg, &json_str));
    ASSERT_NE(nullptr, json_str.p_str);
    ASSERT_EQ(
        string("{\n"
               "\t\"wifi_sta_config\":\t{\n"
               "\t\t\"ssid\":\t\"\",\n"
               "\t\t\"password\":\t\"\"\n"
               "\t},\n"
               "\t\"wifi_ap_config\":\t{\n"
               "\t\t\"password\":\t\"\",\n"
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
               "\t\"remote_cfg_url\":\t\"http://my_server1.com\",\n"
               "\t\"remote_cfg_auth_type\":\t\"none\",\n"
               "\t\"remote_cfg_use_ssl_client_cert\":\tfalse,\n"
               "\t\"remote_cfg_use_ssl_server_cert\":\tfalse,\n"
               "\t\"remote_cfg_refresh_interval_minutes\":\t10,\n"
               "\t\"use_http_ruuvi\":\ttrue,\n"
               "\t\"use_http\":\ttrue,\n"
               "\t\"http_url\":\t\"" RUUVI_GATEWAY_HTTP_DEFAULT_URL "\",\n"
               "\t\"http_data_format\":\t\"ruuvi\",\n"
               "\t\"http_auth\":\t\"none\",\n"
               "\t\"use_http_stat\":\ttrue,\n"
               "\t\"http_stat_url\":\t\"" RUUVI_GATEWAY_HTTP_STATUS_URL "\",\n"
               "\t\"http_stat_user\":\t\"\",\n"
               "\t\"http_stat_pass\":\t\"\",\n"
               "\t\"http_stat_use_ssl_client_cert\":\tfalse,\n"
               "\t\"http_stat_use_ssl_server_cert\":\tfalse,\n"
               "\t\"use_mqtt\":\tfalse,\n"
               "\t\"mqtt_disable_retained_messages\":\tfalse,\n"
               "\t\"mqtt_transport\":\t\"TCP\",\n"
               "\t\"mqtt_server\":\t\"test.mosquitto.org\",\n"
               "\t\"mqtt_port\":\t1883,\n"
               "\t\"mqtt_sending_interval\":\t0,\n"
               "\t\"mqtt_prefix\":\t\"ruuvi/AA:BB:CC:DD:EE:FF/\",\n"
               "\t\"mqtt_client_id\":\t\"AA:BB:CC:DD:EE:FF\",\n"
               "\t\"mqtt_user\":\t\"\",\n"
               "\t\"mqtt_pass\":\t\"\",\n"
               "\t\"mqtt_use_ssl_client_cert\":\tfalse,\n"
               "\t\"mqtt_use_ssl_server_cert\":\tfalse,\n"
               "\t\"lan_auth_type\":\t\"lan_auth_default\",\n"
               "\t\"lan_auth_user\":\t\"Admin\",\n"
               "\t\"lan_auth_api_key\":\t\"\",\n"
               "\t\"lan_auth_api_key_rw\":\t\"\",\n"
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
               "\t\"coordinates\":\t\"\",\n"
               "\t\"fw_update_url\":\t\"https://network.ruuvi.com/firmwareupdate\"\n"
               "}"),
        string(json_str.p_str));
    ASSERT_TRUE(esp_log_wrapper_is_empty());

    gw_cfg_t gw_cfg2 = get_gateway_config_default();
    ASSERT_TRUE(gw_cfg_json_parse("my.json", nullptr, json_str.p_str, &gw_cfg2));
    cjson_wrap_free_json_str(&json_str);

    ASSERT_TRUE(0 == memcmp(&gw_cfg, &gw_cfg2, sizeof(gw_cfg)));
}

TEST_F(TestGwCfgJson, gw_cfg_json_generate_remote_cfg_enabled_auth_basic) // NOLINT
{
    gw_cfg_t         gw_cfg   = get_gateway_config_default();
    cjson_wrap_str_t json_str = cjson_wrap_str_null();

    {
        ruuvi_gw_cfg_remote_t* const p_remote = &gw_cfg.ruuvi_cfg.remote;
        p_remote->use_remote_cfg              = true;
        snprintf(p_remote->url.buf, sizeof(p_remote->url.buf), "https://my_server2.com");
        p_remote->auth_type = GW_CFG_HTTP_AUTH_TYPE_BASIC;
        snprintf(p_remote->auth.auth_basic.user.buf, sizeof(p_remote->auth.auth_basic.user.buf), "user1");
        snprintf(p_remote->auth.auth_basic.password.buf, sizeof(p_remote->auth.auth_basic.password.buf), "pass1");
        p_remote->refresh_interval_minutes = 20;
    }

    ASSERT_TRUE(gw_cfg_json_generate_for_saving(&gw_cfg, &json_str));
    ASSERT_NE(nullptr, json_str.p_str);
    ASSERT_EQ(
        string("{\n"
               "\t\"wifi_sta_config\":\t{\n"
               "\t\t\"ssid\":\t\"\",\n"
               "\t\t\"password\":\t\"\"\n"
               "\t},\n"
               "\t\"wifi_ap_config\":\t{\n"
               "\t\t\"password\":\t\"\",\n"
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
               "\t\"remote_cfg_url\":\t\"https://my_server2.com\",\n"
               "\t\"remote_cfg_auth_type\":\t\"basic\",\n"
               "\t\"remote_cfg_auth_basic_user\":\t\"user1\",\n"
               "\t\"remote_cfg_auth_basic_pass\":\t\"pass1\",\n"
               "\t\"remote_cfg_use_ssl_client_cert\":\tfalse,\n"
               "\t\"remote_cfg_use_ssl_server_cert\":\tfalse,\n"
               "\t\"remote_cfg_refresh_interval_minutes\":\t20,\n"
               "\t\"use_http_ruuvi\":\ttrue,\n"
               "\t\"use_http\":\ttrue,\n"
               "\t\"http_url\":\t\"" RUUVI_GATEWAY_HTTP_DEFAULT_URL "\",\n"
               "\t\"http_data_format\":\t\"ruuvi\",\n"
               "\t\"http_auth\":\t\"none\",\n"
               "\t\"use_http_stat\":\ttrue,\n"
               "\t\"http_stat_url\":\t\"" RUUVI_GATEWAY_HTTP_STATUS_URL "\",\n"
               "\t\"http_stat_user\":\t\"\",\n"
               "\t\"http_stat_pass\":\t\"\",\n"
               "\t\"http_stat_use_ssl_client_cert\":\tfalse,\n"
               "\t\"http_stat_use_ssl_server_cert\":\tfalse,\n"
               "\t\"use_mqtt\":\tfalse,\n"
               "\t\"mqtt_disable_retained_messages\":\tfalse,\n"
               "\t\"mqtt_transport\":\t\"TCP\",\n"
               "\t\"mqtt_server\":\t\"test.mosquitto.org\",\n"
               "\t\"mqtt_port\":\t1883,\n"
               "\t\"mqtt_sending_interval\":\t0,\n"
               "\t\"mqtt_prefix\":\t\"ruuvi/AA:BB:CC:DD:EE:FF/\",\n"
               "\t\"mqtt_client_id\":\t\"AA:BB:CC:DD:EE:FF\",\n"
               "\t\"mqtt_user\":\t\"\",\n"
               "\t\"mqtt_pass\":\t\"\",\n"
               "\t\"mqtt_use_ssl_client_cert\":\tfalse,\n"
               "\t\"mqtt_use_ssl_server_cert\":\tfalse,\n"
               "\t\"lan_auth_type\":\t\"lan_auth_default\",\n"
               "\t\"lan_auth_user\":\t\"Admin\",\n"
               "\t\"lan_auth_api_key\":\t\"\",\n"
               "\t\"lan_auth_api_key_rw\":\t\"\",\n"
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
               "\t\"coordinates\":\t\"\",\n"
               "\t\"fw_update_url\":\t\"https://network.ruuvi.com/firmwareupdate\"\n"
               "}"),
        string(json_str.p_str));
    ASSERT_TRUE(esp_log_wrapper_is_empty());

    gw_cfg_t gw_cfg2 = get_gateway_config_default();
    ASSERT_TRUE(gw_cfg_json_parse("my.json", nullptr, json_str.p_str, &gw_cfg2));
    cjson_wrap_free_json_str(&json_str);

    ASSERT_TRUE(0 == memcmp(&gw_cfg, &gw_cfg2, sizeof(gw_cfg)));
}

TEST_F(TestGwCfgJson, gw_cfg_json_generate_remote_cfg_enabled_auth_bearer) // NOLINT
{
    gw_cfg_t         gw_cfg   = get_gateway_config_default();
    cjson_wrap_str_t json_str = cjson_wrap_str_null();

    {
        ruuvi_gw_cfg_remote_t* const p_remote = &gw_cfg.ruuvi_cfg.remote;
        p_remote->use_remote_cfg              = true;
        snprintf(p_remote->url.buf, sizeof(p_remote->url.buf), "https://my_server2.com");
        p_remote->auth_type = GW_CFG_HTTP_AUTH_TYPE_BEARER;
        snprintf(p_remote->auth.auth_bearer.token.buf, sizeof(p_remote->auth.auth_bearer.token.buf), "token1");
        p_remote->refresh_interval_minutes = 30;
    }

    ASSERT_TRUE(gw_cfg_json_generate_for_saving(&gw_cfg, &json_str));
    ASSERT_NE(nullptr, json_str.p_str);
    ASSERT_EQ(
        string("{\n"
               "\t\"wifi_sta_config\":\t{\n"
               "\t\t\"ssid\":\t\"\",\n"
               "\t\t\"password\":\t\"\"\n"
               "\t},\n"
               "\t\"wifi_ap_config\":\t{\n"
               "\t\t\"password\":\t\"\",\n"
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
               "\t\"remote_cfg_url\":\t\"https://my_server2.com\",\n"
               "\t\"remote_cfg_auth_type\":\t\"bearer\",\n"
               "\t\"remote_cfg_auth_bearer_token\":\t\"token1\",\n"
               "\t\"remote_cfg_use_ssl_client_cert\":\tfalse,\n"
               "\t\"remote_cfg_use_ssl_server_cert\":\tfalse,\n"
               "\t\"remote_cfg_refresh_interval_minutes\":\t30,\n"
               "\t\"use_http_ruuvi\":\ttrue,\n"
               "\t\"use_http\":\ttrue,\n"
               "\t\"http_url\":\t\"" RUUVI_GATEWAY_HTTP_DEFAULT_URL "\",\n"
               "\t\"http_data_format\":\t\"ruuvi\",\n"
               "\t\"http_auth\":\t\"none\",\n"
               "\t\"use_http_stat\":\ttrue,\n"
               "\t\"http_stat_url\":\t\"" RUUVI_GATEWAY_HTTP_STATUS_URL "\",\n"
               "\t\"http_stat_user\":\t\"\",\n"
               "\t\"http_stat_pass\":\t\"\",\n"
               "\t\"http_stat_use_ssl_client_cert\":\tfalse,\n"
               "\t\"http_stat_use_ssl_server_cert\":\tfalse,\n"
               "\t\"use_mqtt\":\tfalse,\n"
               "\t\"mqtt_disable_retained_messages\":\tfalse,\n"
               "\t\"mqtt_transport\":\t\"TCP\",\n"
               "\t\"mqtt_server\":\t\"test.mosquitto.org\",\n"
               "\t\"mqtt_port\":\t1883,\n"
               "\t\"mqtt_sending_interval\":\t0,\n"
               "\t\"mqtt_prefix\":\t\"ruuvi/AA:BB:CC:DD:EE:FF/\",\n"
               "\t\"mqtt_client_id\":\t\"AA:BB:CC:DD:EE:FF\",\n"
               "\t\"mqtt_user\":\t\"\",\n"
               "\t\"mqtt_pass\":\t\"\",\n"
               "\t\"mqtt_use_ssl_client_cert\":\tfalse,\n"
               "\t\"mqtt_use_ssl_server_cert\":\tfalse,\n"
               "\t\"lan_auth_type\":\t\"lan_auth_default\",\n"
               "\t\"lan_auth_user\":\t\"Admin\",\n"
               "\t\"lan_auth_api_key\":\t\"\",\n"
               "\t\"lan_auth_api_key_rw\":\t\"\",\n"
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
               "\t\"coordinates\":\t\"\",\n"
               "\t\"fw_update_url\":\t\"https://network.ruuvi.com/firmwareupdate\"\n"
               "}"),
        string(json_str.p_str));
    ASSERT_TRUE(esp_log_wrapper_is_empty());

    gw_cfg_t gw_cfg2 = get_gateway_config_default();
    ASSERT_TRUE(gw_cfg_json_parse("my.json", nullptr, json_str.p_str, &gw_cfg2));
    cjson_wrap_free_json_str(&json_str);

    ASSERT_TRUE(0 == memcmp(&gw_cfg, &gw_cfg2, sizeof(gw_cfg)));
}

TEST_F(TestGwCfgJson, gw_cfg_json_generate_mqtt_disabled) // NOLINT
{
    gw_cfg_t         gw_cfg   = get_gateway_config_default();
    cjson_wrap_str_t json_str = cjson_wrap_str_null();

    gw_cfg.ruuvi_cfg.mqtt.use_mqtt = false;

    ASSERT_TRUE(gw_cfg_json_generate_for_saving(&gw_cfg, &json_str));
    ASSERT_NE(nullptr, json_str.p_str);
    ASSERT_EQ(
        string("{\n"
               "\t\"wifi_sta_config\":\t{\n"
               "\t\t\"ssid\":\t\"\",\n"
               "\t\t\"password\":\t\"\"\n"
               "\t},\n"
               "\t\"wifi_ap_config\":\t{\n"
               "\t\t\"password\":\t\"\",\n"
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
               "\t\"http_stat_pass\":\t\"\",\n"
               "\t\"http_stat_use_ssl_client_cert\":\tfalse,\n"
               "\t\"http_stat_use_ssl_server_cert\":\tfalse,\n"
               "\t\"use_mqtt\":\tfalse,\n"
               "\t\"mqtt_disable_retained_messages\":\tfalse,\n"
               "\t\"mqtt_transport\":\t\"TCP\",\n"
               "\t\"mqtt_server\":\t\"test.mosquitto.org\",\n"
               "\t\"mqtt_port\":\t1883,\n"
               "\t\"mqtt_sending_interval\":\t0,\n"
               "\t\"mqtt_prefix\":\t\"ruuvi/AA:BB:CC:DD:EE:FF/\",\n"
               "\t\"mqtt_client_id\":\t\"AA:BB:CC:DD:EE:FF\",\n"
               "\t\"mqtt_user\":\t\"\",\n"
               "\t\"mqtt_pass\":\t\"\",\n"
               "\t\"mqtt_use_ssl_client_cert\":\tfalse,\n"
               "\t\"mqtt_use_ssl_server_cert\":\tfalse,\n"
               "\t\"lan_auth_type\":\t\"lan_auth_default\",\n"
               "\t\"lan_auth_user\":\t\"Admin\",\n"
               "\t\"lan_auth_api_key\":\t\"\",\n"
               "\t\"lan_auth_api_key_rw\":\t\"\",\n"
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
               "\t\"coordinates\":\t\"\",\n"
               "\t\"fw_update_url\":\t\"https://network.ruuvi.com/firmwareupdate\"\n"
               "}"),
        string(json_str.p_str));
    ASSERT_TRUE(esp_log_wrapper_is_empty());

    gw_cfg_t gw_cfg2 = get_gateway_config_default();
    ASSERT_TRUE(gw_cfg_json_parse("my.json", nullptr, json_str.p_str, &gw_cfg2));
    cjson_wrap_free_json_str(&json_str);

    ASSERT_TRUE(0 == memcmp(&gw_cfg, &gw_cfg2, sizeof(gw_cfg)));
}

TEST_F(TestGwCfgJson, gw_cfg_json_generate_mqtt_enabled_TCP) // NOLINT
{
    gw_cfg_t         gw_cfg   = get_gateway_config_default();
    cjson_wrap_str_t json_str = cjson_wrap_str_null();

    gw_cfg.ruuvi_cfg.mqtt.use_mqtt                       = true;
    gw_cfg.ruuvi_cfg.mqtt.mqtt_disable_retained_messages = false;
    snprintf(
        gw_cfg.ruuvi_cfg.mqtt.mqtt_transport.buf,
        sizeof(gw_cfg.ruuvi_cfg.mqtt.mqtt_transport.buf),
        MQTT_TRANSPORT_TCP);
    snprintf(gw_cfg.ruuvi_cfg.mqtt.mqtt_server.buf, sizeof(gw_cfg.ruuvi_cfg.mqtt.mqtt_server.buf), "mqtt_server1.com");
    gw_cfg.ruuvi_cfg.mqtt.mqtt_port = 1339;
    snprintf(gw_cfg.ruuvi_cfg.mqtt.mqtt_prefix.buf, sizeof(gw_cfg.ruuvi_cfg.mqtt.mqtt_prefix.buf), "prefix1");
    snprintf(gw_cfg.ruuvi_cfg.mqtt.mqtt_client_id.buf, sizeof(gw_cfg.ruuvi_cfg.mqtt.mqtt_client_id.buf), "client123");
    snprintf(gw_cfg.ruuvi_cfg.mqtt.mqtt_user.buf, sizeof(gw_cfg.ruuvi_cfg.mqtt.mqtt_user.buf), "user1");
    snprintf(gw_cfg.ruuvi_cfg.mqtt.mqtt_pass.buf, sizeof(gw_cfg.ruuvi_cfg.mqtt.mqtt_pass.buf), "pass1");

    ASSERT_TRUE(gw_cfg_json_generate_for_saving(&gw_cfg, &json_str));
    ASSERT_NE(nullptr, json_str.p_str);
    ASSERT_EQ(
        string("{\n"
               "\t\"wifi_sta_config\":\t{\n"
               "\t\t\"ssid\":\t\"\",\n"
               "\t\t\"password\":\t\"\"\n"
               "\t},\n"
               "\t\"wifi_ap_config\":\t{\n"
               "\t\t\"password\":\t\"\",\n"
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
               "\t\"http_stat_pass\":\t\"\",\n"
               "\t\"http_stat_use_ssl_client_cert\":\tfalse,\n"
               "\t\"http_stat_use_ssl_server_cert\":\tfalse,\n"
               "\t\"use_mqtt\":\ttrue,\n"
               "\t\"mqtt_disable_retained_messages\":\tfalse,\n"
               "\t\"mqtt_transport\":\t\"" MQTT_TRANSPORT_TCP "\",\n"
               "\t\"mqtt_server\":\t\"mqtt_server1.com\",\n"
               "\t\"mqtt_port\":\t1339,\n"
               "\t\"mqtt_sending_interval\":\t0,\n"
               "\t\"mqtt_prefix\":\t\"prefix1\",\n"
               "\t\"mqtt_client_id\":\t\"client123\",\n"
               "\t\"mqtt_user\":\t\"user1\",\n"
               "\t\"mqtt_pass\":\t\"pass1\",\n"
               "\t\"mqtt_use_ssl_client_cert\":\tfalse,\n"
               "\t\"mqtt_use_ssl_server_cert\":\tfalse,\n"
               "\t\"lan_auth_type\":\t\"lan_auth_default\",\n"
               "\t\"lan_auth_user\":\t\"Admin\",\n"
               "\t\"lan_auth_api_key\":\t\"\",\n"
               "\t\"lan_auth_api_key_rw\":\t\"\",\n"
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
               "\t\"coordinates\":\t\"\",\n"
               "\t\"fw_update_url\":\t\"https://network.ruuvi.com/firmwareupdate\"\n"
               "}"),
        string(json_str.p_str));
    ASSERT_TRUE(esp_log_wrapper_is_empty());

    gw_cfg_t gw_cfg2 = get_gateway_config_default();
    ASSERT_TRUE(gw_cfg_json_parse("my.json", nullptr, json_str.p_str, &gw_cfg2));
    cjson_wrap_free_json_str(&json_str);

    ASSERT_TRUE(0 == memcmp(&gw_cfg, &gw_cfg2, sizeof(gw_cfg)));
}

TEST_F(TestGwCfgJson, gw_cfg_json_generate_mqtt_enabled_TCP_disable_retained_messages) // NOLINT
{
    gw_cfg_t         gw_cfg   = get_gateway_config_default();
    cjson_wrap_str_t json_str = cjson_wrap_str_null();

    gw_cfg.ruuvi_cfg.mqtt.use_mqtt                       = true;
    gw_cfg.ruuvi_cfg.mqtt.mqtt_disable_retained_messages = true;
    snprintf(
        gw_cfg.ruuvi_cfg.mqtt.mqtt_transport.buf,
        sizeof(gw_cfg.ruuvi_cfg.mqtt.mqtt_transport.buf),
        MQTT_TRANSPORT_TCP);
    snprintf(gw_cfg.ruuvi_cfg.mqtt.mqtt_server.buf, sizeof(gw_cfg.ruuvi_cfg.mqtt.mqtt_server.buf), "mqtt_server1.com");
    gw_cfg.ruuvi_cfg.mqtt.mqtt_port = 1339;
    snprintf(gw_cfg.ruuvi_cfg.mqtt.mqtt_prefix.buf, sizeof(gw_cfg.ruuvi_cfg.mqtt.mqtt_prefix.buf), "prefix1");
    snprintf(gw_cfg.ruuvi_cfg.mqtt.mqtt_client_id.buf, sizeof(gw_cfg.ruuvi_cfg.mqtt.mqtt_client_id.buf), "client123");
    snprintf(gw_cfg.ruuvi_cfg.mqtt.mqtt_user.buf, sizeof(gw_cfg.ruuvi_cfg.mqtt.mqtt_user.buf), "user1");
    snprintf(gw_cfg.ruuvi_cfg.mqtt.mqtt_pass.buf, sizeof(gw_cfg.ruuvi_cfg.mqtt.mqtt_pass.buf), "pass1");

    ASSERT_TRUE(gw_cfg_json_generate_for_saving(&gw_cfg, &json_str));
    ASSERT_NE(nullptr, json_str.p_str);
    ASSERT_EQ(
        string("{\n"
               "\t\"wifi_sta_config\":\t{\n"
               "\t\t\"ssid\":\t\"\",\n"
               "\t\t\"password\":\t\"\"\n"
               "\t},\n"
               "\t\"wifi_ap_config\":\t{\n"
               "\t\t\"password\":\t\"\",\n"
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
               "\t\"http_stat_pass\":\t\"\",\n"
               "\t\"http_stat_use_ssl_client_cert\":\tfalse,\n"
               "\t\"http_stat_use_ssl_server_cert\":\tfalse,\n"
               "\t\"use_mqtt\":\ttrue,\n"
               "\t\"mqtt_disable_retained_messages\":\ttrue,\n"
               "\t\"mqtt_transport\":\t\"" MQTT_TRANSPORT_TCP "\",\n"
               "\t\"mqtt_server\":\t\"mqtt_server1.com\",\n"
               "\t\"mqtt_port\":\t1339,\n"
               "\t\"mqtt_sending_interval\":\t0,\n"
               "\t\"mqtt_prefix\":\t\"prefix1\",\n"
               "\t\"mqtt_client_id\":\t\"client123\",\n"
               "\t\"mqtt_user\":\t\"user1\",\n"
               "\t\"mqtt_pass\":\t\"pass1\",\n"
               "\t\"mqtt_use_ssl_client_cert\":\tfalse,\n"
               "\t\"mqtt_use_ssl_server_cert\":\tfalse,\n"
               "\t\"lan_auth_type\":\t\"lan_auth_default\",\n"
               "\t\"lan_auth_user\":\t\"Admin\",\n"
               "\t\"lan_auth_api_key\":\t\"\",\n"
               "\t\"lan_auth_api_key_rw\":\t\"\",\n"
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
               "\t\"coordinates\":\t\"\",\n"
               "\t\"fw_update_url\":\t\"https://network.ruuvi.com/firmwareupdate\"\n"
               "}"),
        string(json_str.p_str));
    ASSERT_TRUE(esp_log_wrapper_is_empty());

    gw_cfg_t gw_cfg2 = get_gateway_config_default();
    ASSERT_TRUE(gw_cfg_json_parse("my.json", nullptr, json_str.p_str, &gw_cfg2));
    cjson_wrap_free_json_str(&json_str);

    ASSERT_TRUE(0 == memcmp(&gw_cfg, &gw_cfg2, sizeof(gw_cfg)));
}

TEST_F(TestGwCfgJson, gw_cfg_json_generate_mqtt_enabled_SSL) // NOLINT
{
    gw_cfg_t         gw_cfg   = get_gateway_config_default();
    cjson_wrap_str_t json_str = cjson_wrap_str_null();

    gw_cfg.ruuvi_cfg.mqtt.use_mqtt                       = true;
    gw_cfg.ruuvi_cfg.mqtt.mqtt_disable_retained_messages = false;
    snprintf(
        gw_cfg.ruuvi_cfg.mqtt.mqtt_transport.buf,
        sizeof(gw_cfg.ruuvi_cfg.mqtt.mqtt_transport.buf),
        MQTT_TRANSPORT_SSL);
    snprintf(gw_cfg.ruuvi_cfg.mqtt.mqtt_server.buf, sizeof(gw_cfg.ruuvi_cfg.mqtt.mqtt_server.buf), "mqtt_server2.com");
    gw_cfg.ruuvi_cfg.mqtt.mqtt_port = 1340;
    snprintf(gw_cfg.ruuvi_cfg.mqtt.mqtt_prefix.buf, sizeof(gw_cfg.ruuvi_cfg.mqtt.mqtt_prefix.buf), "prefix2");
    snprintf(gw_cfg.ruuvi_cfg.mqtt.mqtt_client_id.buf, sizeof(gw_cfg.ruuvi_cfg.mqtt.mqtt_client_id.buf), "client124");
    snprintf(gw_cfg.ruuvi_cfg.mqtt.mqtt_user.buf, sizeof(gw_cfg.ruuvi_cfg.mqtt.mqtt_user.buf), "user2");
    snprintf(gw_cfg.ruuvi_cfg.mqtt.mqtt_pass.buf, sizeof(gw_cfg.ruuvi_cfg.mqtt.mqtt_pass.buf), "pass2");

    ASSERT_TRUE(gw_cfg_json_generate_for_saving(&gw_cfg, &json_str));
    ASSERT_NE(nullptr, json_str.p_str);
    ASSERT_EQ(
        string("{\n"
               "\t\"wifi_sta_config\":\t{\n"
               "\t\t\"ssid\":\t\"\",\n"
               "\t\t\"password\":\t\"\"\n"
               "\t},\n"
               "\t\"wifi_ap_config\":\t{\n"
               "\t\t\"password\":\t\"\",\n"
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
               "\t\"http_stat_pass\":\t\"\",\n"
               "\t\"http_stat_use_ssl_client_cert\":\tfalse,\n"
               "\t\"http_stat_use_ssl_server_cert\":\tfalse,\n"
               "\t\"use_mqtt\":\ttrue,\n"
               "\t\"mqtt_disable_retained_messages\":\tfalse,\n"
               "\t\"mqtt_transport\":\t\"" MQTT_TRANSPORT_SSL "\",\n"
               "\t\"mqtt_server\":\t\"mqtt_server2.com\",\n"
               "\t\"mqtt_port\":\t1340,\n"
               "\t\"mqtt_sending_interval\":\t0,\n"
               "\t\"mqtt_prefix\":\t\"prefix2\",\n"
               "\t\"mqtt_client_id\":\t\"client124\",\n"
               "\t\"mqtt_user\":\t\"user2\",\n"
               "\t\"mqtt_pass\":\t\"pass2\",\n"
               "\t\"mqtt_use_ssl_client_cert\":\tfalse,\n"
               "\t\"mqtt_use_ssl_server_cert\":\tfalse,\n"
               "\t\"lan_auth_type\":\t\"lan_auth_default\",\n"
               "\t\"lan_auth_user\":\t\"Admin\",\n"
               "\t\"lan_auth_api_key\":\t\"\",\n"
               "\t\"lan_auth_api_key_rw\":\t\"\",\n"
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
               "\t\"coordinates\":\t\"\",\n"
               "\t\"fw_update_url\":\t\"https://network.ruuvi.com/firmwareupdate\"\n"
               "}"),
        string(json_str.p_str));
    ASSERT_TRUE(esp_log_wrapper_is_empty());

    gw_cfg_t gw_cfg2 = get_gateway_config_default();
    ASSERT_TRUE(gw_cfg_json_parse("my.json", nullptr, json_str.p_str, &gw_cfg2));
    cjson_wrap_free_json_str(&json_str);

    ASSERT_TRUE(0 == memcmp(&gw_cfg, &gw_cfg2, sizeof(gw_cfg)));
}

TEST_F(TestGwCfgJson, gw_cfg_json_generate_mqtt_enabled_WS) // NOLINT
{
    gw_cfg_t         gw_cfg   = get_gateway_config_default();
    cjson_wrap_str_t json_str = cjson_wrap_str_null();

    gw_cfg.ruuvi_cfg.mqtt.use_mqtt                       = true;
    gw_cfg.ruuvi_cfg.mqtt.mqtt_disable_retained_messages = false;
    snprintf(
        gw_cfg.ruuvi_cfg.mqtt.mqtt_transport.buf,
        sizeof(gw_cfg.ruuvi_cfg.mqtt.mqtt_transport.buf),
        MQTT_TRANSPORT_WS);
    snprintf(gw_cfg.ruuvi_cfg.mqtt.mqtt_server.buf, sizeof(gw_cfg.ruuvi_cfg.mqtt.mqtt_server.buf), "mqtt_server2.com");
    gw_cfg.ruuvi_cfg.mqtt.mqtt_port = 1340;
    snprintf(gw_cfg.ruuvi_cfg.mqtt.mqtt_prefix.buf, sizeof(gw_cfg.ruuvi_cfg.mqtt.mqtt_prefix.buf), "prefix2");
    snprintf(gw_cfg.ruuvi_cfg.mqtt.mqtt_client_id.buf, sizeof(gw_cfg.ruuvi_cfg.mqtt.mqtt_client_id.buf), "client124");
    snprintf(gw_cfg.ruuvi_cfg.mqtt.mqtt_user.buf, sizeof(gw_cfg.ruuvi_cfg.mqtt.mqtt_user.buf), "user2");
    snprintf(gw_cfg.ruuvi_cfg.mqtt.mqtt_pass.buf, sizeof(gw_cfg.ruuvi_cfg.mqtt.mqtt_pass.buf), "pass2");

    ASSERT_TRUE(gw_cfg_json_generate_for_saving(&gw_cfg, &json_str));
    ASSERT_NE(nullptr, json_str.p_str);
    ASSERT_EQ(
        string("{\n"
               "\t\"wifi_sta_config\":\t{\n"
               "\t\t\"ssid\":\t\"\",\n"
               "\t\t\"password\":\t\"\"\n"
               "\t},\n"
               "\t\"wifi_ap_config\":\t{\n"
               "\t\t\"password\":\t\"\",\n"
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
               "\t\"http_stat_pass\":\t\"\",\n"
               "\t\"http_stat_use_ssl_client_cert\":\tfalse,\n"
               "\t\"http_stat_use_ssl_server_cert\":\tfalse,\n"
               "\t\"use_mqtt\":\ttrue,\n"
               "\t\"mqtt_disable_retained_messages\":\tfalse,\n"
               "\t\"mqtt_transport\":\t\"" MQTT_TRANSPORT_WS "\",\n"
               "\t\"mqtt_server\":\t\"mqtt_server2.com\",\n"
               "\t\"mqtt_port\":\t1340,\n"
               "\t\"mqtt_sending_interval\":\t0,\n"
               "\t\"mqtt_prefix\":\t\"prefix2\",\n"
               "\t\"mqtt_client_id\":\t\"client124\",\n"
               "\t\"mqtt_user\":\t\"user2\",\n"
               "\t\"mqtt_pass\":\t\"pass2\",\n"
               "\t\"mqtt_use_ssl_client_cert\":\tfalse,\n"
               "\t\"mqtt_use_ssl_server_cert\":\tfalse,\n"
               "\t\"lan_auth_type\":\t\"lan_auth_default\",\n"
               "\t\"lan_auth_user\":\t\"Admin\",\n"
               "\t\"lan_auth_api_key\":\t\"\",\n"
               "\t\"lan_auth_api_key_rw\":\t\"\",\n"
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
               "\t\"coordinates\":\t\"\",\n"
               "\t\"fw_update_url\":\t\"https://network.ruuvi.com/firmwareupdate\"\n"
               "}"),
        string(json_str.p_str));
    ASSERT_TRUE(esp_log_wrapper_is_empty());

    gw_cfg_t gw_cfg2 = get_gateway_config_default();
    ASSERT_TRUE(gw_cfg_json_parse("my.json", nullptr, json_str.p_str, &gw_cfg2));
    cjson_wrap_free_json_str(&json_str);

    ASSERT_TRUE(0 == memcmp(&gw_cfg, &gw_cfg2, sizeof(gw_cfg)));
}

TEST_F(TestGwCfgJson, gw_cfg_json_generate_mqtt_enabled_WSS) // NOLINT
{
    gw_cfg_t         gw_cfg   = get_gateway_config_default();
    cjson_wrap_str_t json_str = cjson_wrap_str_null();

    gw_cfg.ruuvi_cfg.mqtt.use_mqtt                       = true;
    gw_cfg.ruuvi_cfg.mqtt.mqtt_disable_retained_messages = false;
    snprintf(
        gw_cfg.ruuvi_cfg.mqtt.mqtt_transport.buf,
        sizeof(gw_cfg.ruuvi_cfg.mqtt.mqtt_transport.buf),
        MQTT_TRANSPORT_WSS);
    snprintf(gw_cfg.ruuvi_cfg.mqtt.mqtt_server.buf, sizeof(gw_cfg.ruuvi_cfg.mqtt.mqtt_server.buf), "mqtt_server2.com");
    gw_cfg.ruuvi_cfg.mqtt.mqtt_port = 1340;
    snprintf(gw_cfg.ruuvi_cfg.mqtt.mqtt_prefix.buf, sizeof(gw_cfg.ruuvi_cfg.mqtt.mqtt_prefix.buf), "prefix2");
    snprintf(gw_cfg.ruuvi_cfg.mqtt.mqtt_client_id.buf, sizeof(gw_cfg.ruuvi_cfg.mqtt.mqtt_client_id.buf), "client124");
    snprintf(gw_cfg.ruuvi_cfg.mqtt.mqtt_user.buf, sizeof(gw_cfg.ruuvi_cfg.mqtt.mqtt_user.buf), "user2");
    snprintf(gw_cfg.ruuvi_cfg.mqtt.mqtt_pass.buf, sizeof(gw_cfg.ruuvi_cfg.mqtt.mqtt_pass.buf), "pass2");

    ASSERT_TRUE(gw_cfg_json_generate_for_saving(&gw_cfg, &json_str));
    ASSERT_NE(nullptr, json_str.p_str);
    ASSERT_EQ(
        string("{\n"
               "\t\"wifi_sta_config\":\t{\n"
               "\t\t\"ssid\":\t\"\",\n"
               "\t\t\"password\":\t\"\"\n"
               "\t},\n"
               "\t\"wifi_ap_config\":\t{\n"
               "\t\t\"password\":\t\"\",\n"
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
               "\t\"http_stat_pass\":\t\"\",\n"
               "\t\"http_stat_use_ssl_client_cert\":\tfalse,\n"
               "\t\"http_stat_use_ssl_server_cert\":\tfalse,\n"
               "\t\"use_mqtt\":\ttrue,\n"
               "\t\"mqtt_disable_retained_messages\":\tfalse,\n"
               "\t\"mqtt_transport\":\t\"" MQTT_TRANSPORT_WSS "\",\n"
               "\t\"mqtt_server\":\t\"mqtt_server2.com\",\n"
               "\t\"mqtt_port\":\t1340,\n"
               "\t\"mqtt_sending_interval\":\t0,\n"
               "\t\"mqtt_prefix\":\t\"prefix2\",\n"
               "\t\"mqtt_client_id\":\t\"client124\",\n"
               "\t\"mqtt_user\":\t\"user2\",\n"
               "\t\"mqtt_pass\":\t\"pass2\",\n"
               "\t\"mqtt_use_ssl_client_cert\":\tfalse,\n"
               "\t\"mqtt_use_ssl_server_cert\":\tfalse,\n"
               "\t\"lan_auth_type\":\t\"lan_auth_default\",\n"
               "\t\"lan_auth_user\":\t\"Admin\",\n"
               "\t\"lan_auth_api_key\":\t\"\",\n"
               "\t\"lan_auth_api_key_rw\":\t\"\",\n"
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
               "\t\"coordinates\":\t\"\",\n"
               "\t\"fw_update_url\":\t\"https://network.ruuvi.com/firmwareupdate\"\n"
               "}"),
        string(json_str.p_str));
    ASSERT_TRUE(esp_log_wrapper_is_empty());

    gw_cfg_t gw_cfg2 = get_gateway_config_default();
    ASSERT_TRUE(gw_cfg_json_parse("my.json", nullptr, json_str.p_str, &gw_cfg2));
    cjson_wrap_free_json_str(&json_str);

    ASSERT_TRUE(0 == memcmp(&gw_cfg, &gw_cfg2, sizeof(gw_cfg)));
}

TEST_F(TestGwCfgJson, gw_cfg_json_generate_custom_http_enabled) // NOLINT
{
    gw_cfg_t         gw_cfg   = get_gateway_config_default();
    cjson_wrap_str_t json_str = cjson_wrap_str_null();

    gw_cfg.ruuvi_cfg.http.use_http_ruuvi           = false;
    gw_cfg.ruuvi_cfg.http.use_http                 = true;
    gw_cfg.ruuvi_cfg.http.http_use_ssl_client_cert = false;
    gw_cfg.ruuvi_cfg.http.http_use_ssl_server_cert = true;
    snprintf(
        gw_cfg.ruuvi_cfg.http.http_url.buf,
        sizeof(gw_cfg.ruuvi_cfg.http.http_url.buf),
        "https://my_url1.com/record");
    gw_cfg.ruuvi_cfg.http.data_format = GW_CFG_HTTP_DATA_FORMAT_RUUVI;
    gw_cfg.ruuvi_cfg.http.auth_type   = GW_CFG_HTTP_AUTH_TYPE_NONE;

    ASSERT_TRUE(gw_cfg_json_generate_for_saving(&gw_cfg, &json_str));
    ASSERT_NE(nullptr, json_str.p_str);
    ASSERT_EQ(
        string("{\n"
               "\t\"wifi_sta_config\":\t{\n"
               "\t\t\"ssid\":\t\"\",\n"
               "\t\t\"password\":\t\"\"\n"
               "\t},\n"
               "\t\"wifi_ap_config\":\t{\n"
               "\t\t\"password\":\t\"\",\n"
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
               "\t\"use_http_ruuvi\":\tfalse,\n"
               "\t\"use_http\":\ttrue,\n"
               "\t\"http_use_ssl_client_cert\":\tfalse,\n"
               "\t\"http_use_ssl_server_cert\":\ttrue,\n"
               "\t\"http_url\":\t\"https://my_url1.com/record\",\n"
               "\t\"http_data_format\":\t\"ruuvi\",\n"
               "\t\"http_auth\":\t\"none\",\n"
               "\t\"use_http_stat\":\ttrue,\n"
               "\t\"http_stat_url\":\t\"" RUUVI_GATEWAY_HTTP_STATUS_URL "\",\n"
               "\t\"http_stat_user\":\t\"\",\n"
               "\t\"http_stat_pass\":\t\"\",\n"
               "\t\"http_stat_use_ssl_client_cert\":\tfalse,\n"
               "\t\"http_stat_use_ssl_server_cert\":\tfalse,\n"
               "\t\"use_mqtt\":\tfalse,\n"
               "\t\"mqtt_disable_retained_messages\":\tfalse,\n"
               "\t\"mqtt_transport\":\t\"TCP\",\n"
               "\t\"mqtt_server\":\t\"test.mosquitto.org\",\n"
               "\t\"mqtt_port\":\t1883,\n"
               "\t\"mqtt_sending_interval\":\t0,\n"
               "\t\"mqtt_prefix\":\t\"ruuvi/AA:BB:CC:DD:EE:FF/\",\n"
               "\t\"mqtt_client_id\":\t\"AA:BB:CC:DD:EE:FF\",\n"
               "\t\"mqtt_user\":\t\"\",\n"
               "\t\"mqtt_pass\":\t\"\",\n"
               "\t\"mqtt_use_ssl_client_cert\":\tfalse,\n"
               "\t\"mqtt_use_ssl_server_cert\":\tfalse,\n"
               "\t\"lan_auth_type\":\t\"lan_auth_default\",\n"
               "\t\"lan_auth_user\":\t\"Admin\",\n"
               "\t\"lan_auth_api_key\":\t\"\",\n"
               "\t\"lan_auth_api_key_rw\":\t\"\",\n"
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
               "\t\"coordinates\":\t\"\",\n"
               "\t\"fw_update_url\":\t\"https://network.ruuvi.com/firmwareupdate\"\n"
               "}"),
        string(json_str.p_str));
    ASSERT_TRUE(esp_log_wrapper_is_empty());

    gw_cfg_t gw_cfg2 = get_gateway_config_default();
    ASSERT_TRUE(gw_cfg_json_parse("my.json", nullptr, json_str.p_str, &gw_cfg2));
    cjson_wrap_free_json_str(&json_str);

    ASSERT_TRUE(0 == memcmp(&gw_cfg, &gw_cfg2, sizeof(gw_cfg)));
}

TEST_F(TestGwCfgJson, gw_cfg_json_generate_http_disabled) // NOLINT
{
    gw_cfg_t         gw_cfg   = get_gateway_config_default();
    cjson_wrap_str_t json_str = cjson_wrap_str_null();

    gw_cfg.ruuvi_cfg.http.use_http_ruuvi = false;
    gw_cfg.ruuvi_cfg.http.use_http       = false;
    ASSERT_TRUE(gw_cfg_json_generate_for_saving(&gw_cfg, &json_str));
    ASSERT_NE(nullptr, json_str.p_str);
    ASSERT_EQ(
        string("{\n"
               "\t\"wifi_sta_config\":\t{\n"
               "\t\t\"ssid\":\t\"\",\n"
               "\t\t\"password\":\t\"\"\n"
               "\t},\n"
               "\t\"wifi_ap_config\":\t{\n"
               "\t\t\"password\":\t\"\",\n"
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
               "\t\"use_http_ruuvi\":\tfalse,\n"
               "\t\"use_http\":\tfalse,\n"
               "\t\"use_http_stat\":\ttrue,\n"
               "\t\"http_stat_url\":\t\"" RUUVI_GATEWAY_HTTP_STATUS_URL "\",\n"
               "\t\"http_stat_user\":\t\"\",\n"
               "\t\"http_stat_pass\":\t\"\",\n"
               "\t\"http_stat_use_ssl_client_cert\":\tfalse,\n"
               "\t\"http_stat_use_ssl_server_cert\":\tfalse,\n"
               "\t\"use_mqtt\":\tfalse,\n"
               "\t\"mqtt_disable_retained_messages\":\tfalse,\n"
               "\t\"mqtt_transport\":\t\"TCP\",\n"
               "\t\"mqtt_server\":\t\"test.mosquitto.org\",\n"
               "\t\"mqtt_port\":\t1883,\n"
               "\t\"mqtt_sending_interval\":\t0,\n"
               "\t\"mqtt_prefix\":\t\"ruuvi/AA:BB:CC:DD:EE:FF/\",\n"
               "\t\"mqtt_client_id\":\t\"AA:BB:CC:DD:EE:FF\",\n"
               "\t\"mqtt_user\":\t\"\",\n"
               "\t\"mqtt_pass\":\t\"\",\n"
               "\t\"mqtt_use_ssl_client_cert\":\tfalse,\n"
               "\t\"mqtt_use_ssl_server_cert\":\tfalse,\n"
               "\t\"lan_auth_type\":\t\"lan_auth_default\",\n"
               "\t\"lan_auth_user\":\t\"Admin\",\n"
               "\t\"lan_auth_api_key\":\t\"\",\n"
               "\t\"lan_auth_api_key_rw\":\t\"\",\n"
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
               "\t\"coordinates\":\t\"\",\n"
               "\t\"fw_update_url\":\t\"https://network.ruuvi.com/firmwareupdate\"\n"
               "}"),
        string(json_str.p_str));
    ASSERT_TRUE(esp_log_wrapper_is_empty());

    gw_cfg_t gw_cfg2 = get_gateway_config_default();
    ASSERT_TRUE(gw_cfg_json_parse("my.json", nullptr, json_str.p_str, &gw_cfg2));
    cjson_wrap_free_json_str(&json_str);

    ASSERT_TRUE(0 == memcmp(&gw_cfg, &gw_cfg2, sizeof(gw_cfg)));
}

TEST_F(TestGwCfgJson, gw_cfg_json_generate_http_enabled_data_format_ruuvi) // NOLINT
{
    gw_cfg_t         gw_cfg   = get_gateway_config_default();
    cjson_wrap_str_t json_str = cjson_wrap_str_null();

    gw_cfg.ruuvi_cfg.http.use_http    = true;
    gw_cfg.ruuvi_cfg.http.data_format = GW_CFG_HTTP_DATA_FORMAT_RUUVI;
    snprintf(
        gw_cfg.ruuvi_cfg.http.http_url.buf,
        sizeof(gw_cfg.ruuvi_cfg.http.http_url.buf),
        "https://my_url1.com/status");
    ASSERT_TRUE(gw_cfg_json_generate_for_saving(&gw_cfg, &json_str));
    ASSERT_NE(nullptr, json_str.p_str);
    ASSERT_EQ(
        string("{\n"
               "\t\"wifi_sta_config\":\t{\n"
               "\t\t\"ssid\":\t\"\",\n"
               "\t\t\"password\":\t\"\"\n"
               "\t},\n"
               "\t\"wifi_ap_config\":\t{\n"
               "\t\t\"password\":\t\"\",\n"
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
               "\t\"http_use_ssl_client_cert\":\tfalse,\n"
               "\t\"http_use_ssl_server_cert\":\tfalse,\n"
               "\t\"http_url\":\t\""
               "https://my_url1.com/status"
               "\",\n"
               "\t\"http_data_format\":\t\"ruuvi\",\n"
               "\t\"http_auth\":\t\"none\",\n"
               "\t\"use_http_stat\":\ttrue,\n"
               "\t\"http_stat_url\":\t\"" RUUVI_GATEWAY_HTTP_STATUS_URL "\",\n"
               "\t\"http_stat_user\":\t\"\",\n"
               "\t\"http_stat_pass\":\t\"\",\n"
               "\t\"http_stat_use_ssl_client_cert\":\tfalse,\n"
               "\t\"http_stat_use_ssl_server_cert\":\tfalse,\n"
               "\t\"use_mqtt\":\tfalse,\n"
               "\t\"mqtt_disable_retained_messages\":\tfalse,\n"
               "\t\"mqtt_transport\":\t\"TCP\",\n"
               "\t\"mqtt_server\":\t\"test.mosquitto.org\",\n"
               "\t\"mqtt_port\":\t1883,\n"
               "\t\"mqtt_sending_interval\":\t0,\n"
               "\t\"mqtt_prefix\":\t\"ruuvi/AA:BB:CC:DD:EE:FF/\",\n"
               "\t\"mqtt_client_id\":\t\"AA:BB:CC:DD:EE:FF\",\n"
               "\t\"mqtt_user\":\t\"\",\n"
               "\t\"mqtt_pass\":\t\"\",\n"
               "\t\"mqtt_use_ssl_client_cert\":\tfalse,\n"
               "\t\"mqtt_use_ssl_server_cert\":\tfalse,\n"
               "\t\"lan_auth_type\":\t\"lan_auth_default\",\n"
               "\t\"lan_auth_user\":\t\"Admin\",\n"
               "\t\"lan_auth_api_key\":\t\"\",\n"
               "\t\"lan_auth_api_key_rw\":\t\"\",\n"
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
               "\t\"coordinates\":\t\"\",\n"
               "\t\"fw_update_url\":\t\"https://network.ruuvi.com/firmwareupdate\"\n"
               "}"),
        string(json_str.p_str));
    ASSERT_TRUE(esp_log_wrapper_is_empty());

    gw_cfg_t gw_cfg2 = get_gateway_config_default();
    ASSERT_TRUE(gw_cfg_json_parse("my.json", nullptr, json_str.p_str, &gw_cfg2));
    cjson_wrap_free_json_str(&json_str);

    ASSERT_TRUE(0 == memcmp(&gw_cfg, &gw_cfg2, sizeof(gw_cfg)));
}

TEST_F(TestGwCfgJson, gw_cfg_json_generate_http_enabled_data_format_ruuvi_raw_and_decoded) // NOLINT
{
    gw_cfg_t         gw_cfg   = get_gateway_config_default();
    cjson_wrap_str_t json_str = cjson_wrap_str_null();

    gw_cfg.ruuvi_cfg.http.use_http    = true;
    gw_cfg.ruuvi_cfg.http.data_format = GW_CFG_HTTP_DATA_FORMAT_RUUVI_RAW_AND_DECODED;
    snprintf(
        gw_cfg.ruuvi_cfg.http.http_url.buf,
        sizeof(gw_cfg.ruuvi_cfg.http.http_url.buf),
        "https://my_url1.com/status");
    ASSERT_TRUE(gw_cfg_json_generate_for_saving(&gw_cfg, &json_str));
    ASSERT_NE(nullptr, json_str.p_str);
    ASSERT_EQ(
        string("{\n"
               "\t\"wifi_sta_config\":\t{\n"
               "\t\t\"ssid\":\t\"\",\n"
               "\t\t\"password\":\t\"\"\n"
               "\t},\n"
               "\t\"wifi_ap_config\":\t{\n"
               "\t\t\"password\":\t\"\",\n"
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
               "\t\"http_use_ssl_client_cert\":\tfalse,\n"
               "\t\"http_use_ssl_server_cert\":\tfalse,\n"
               "\t\"http_url\":\t\""
               "https://my_url1.com/status"
               "\",\n"
               "\t\"http_data_format\":\t\"ruuvi_raw_and_decoded\",\n"
               "\t\"http_auth\":\t\"none\",\n"
               "\t\"use_http_stat\":\ttrue,\n"
               "\t\"http_stat_url\":\t\"" RUUVI_GATEWAY_HTTP_STATUS_URL "\",\n"
               "\t\"http_stat_user\":\t\"\",\n"
               "\t\"http_stat_pass\":\t\"\",\n"
               "\t\"http_stat_use_ssl_client_cert\":\tfalse,\n"
               "\t\"http_stat_use_ssl_server_cert\":\tfalse,\n"
               "\t\"use_mqtt\":\tfalse,\n"
               "\t\"mqtt_disable_retained_messages\":\tfalse,\n"
               "\t\"mqtt_transport\":\t\"TCP\",\n"
               "\t\"mqtt_server\":\t\"test.mosquitto.org\",\n"
               "\t\"mqtt_port\":\t1883,\n"
               "\t\"mqtt_sending_interval\":\t0,\n"
               "\t\"mqtt_prefix\":\t\"ruuvi/AA:BB:CC:DD:EE:FF/\",\n"
               "\t\"mqtt_client_id\":\t\"AA:BB:CC:DD:EE:FF\",\n"
               "\t\"mqtt_user\":\t\"\",\n"
               "\t\"mqtt_pass\":\t\"\",\n"
               "\t\"mqtt_use_ssl_client_cert\":\tfalse,\n"
               "\t\"mqtt_use_ssl_server_cert\":\tfalse,\n"
               "\t\"lan_auth_type\":\t\"lan_auth_default\",\n"
               "\t\"lan_auth_user\":\t\"Admin\",\n"
               "\t\"lan_auth_api_key\":\t\"\",\n"
               "\t\"lan_auth_api_key_rw\":\t\"\",\n"
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
               "\t\"coordinates\":\t\"\",\n"
               "\t\"fw_update_url\":\t\"https://network.ruuvi.com/firmwareupdate\"\n"
               "}"),
        string(json_str.p_str));
    ASSERT_TRUE(esp_log_wrapper_is_empty());

    gw_cfg_t gw_cfg2 = get_gateway_config_default();
    ASSERT_TRUE(gw_cfg_json_parse("my.json", nullptr, json_str.p_str, &gw_cfg2));
    cjson_wrap_free_json_str(&json_str);

    ASSERT_TRUE(0 == memcmp(&gw_cfg, &gw_cfg2, sizeof(gw_cfg)));
}

TEST_F(TestGwCfgJson, gw_cfg_json_generate_http_enabled_data_format_ruuvi_decoded) // NOLINT
{
    gw_cfg_t         gw_cfg   = get_gateway_config_default();
    cjson_wrap_str_t json_str = cjson_wrap_str_null();

    gw_cfg.ruuvi_cfg.http.use_http    = true;
    gw_cfg.ruuvi_cfg.http.data_format = GW_CFG_HTTP_DATA_FORMAT_RUUVI_DECODED;
    snprintf(
        gw_cfg.ruuvi_cfg.http.http_url.buf,
        sizeof(gw_cfg.ruuvi_cfg.http.http_url.buf),
        "https://my_url1.com/status");
    ASSERT_TRUE(gw_cfg_json_generate_for_saving(&gw_cfg, &json_str));
    ASSERT_NE(nullptr, json_str.p_str);
    ASSERT_EQ(
        string("{\n"
               "\t\"wifi_sta_config\":\t{\n"
               "\t\t\"ssid\":\t\"\",\n"
               "\t\t\"password\":\t\"\"\n"
               "\t},\n"
               "\t\"wifi_ap_config\":\t{\n"
               "\t\t\"password\":\t\"\",\n"
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
               "\t\"http_use_ssl_client_cert\":\tfalse,\n"
               "\t\"http_use_ssl_server_cert\":\tfalse,\n"
               "\t\"http_url\":\t\""
               "https://my_url1.com/status"
               "\",\n"
               "\t\"http_data_format\":\t\"ruuvi_decoded\",\n"
               "\t\"http_auth\":\t\"none\",\n"
               "\t\"use_http_stat\":\ttrue,\n"
               "\t\"http_stat_url\":\t\"" RUUVI_GATEWAY_HTTP_STATUS_URL "\",\n"
               "\t\"http_stat_user\":\t\"\",\n"
               "\t\"http_stat_pass\":\t\"\",\n"
               "\t\"http_stat_use_ssl_client_cert\":\tfalse,\n"
               "\t\"http_stat_use_ssl_server_cert\":\tfalse,\n"
               "\t\"use_mqtt\":\tfalse,\n"
               "\t\"mqtt_disable_retained_messages\":\tfalse,\n"
               "\t\"mqtt_transport\":\t\"TCP\",\n"
               "\t\"mqtt_server\":\t\"test.mosquitto.org\",\n"
               "\t\"mqtt_port\":\t1883,\n"
               "\t\"mqtt_sending_interval\":\t0,\n"
               "\t\"mqtt_prefix\":\t\"ruuvi/AA:BB:CC:DD:EE:FF/\",\n"
               "\t\"mqtt_client_id\":\t\"AA:BB:CC:DD:EE:FF\",\n"
               "\t\"mqtt_user\":\t\"\",\n"
               "\t\"mqtt_pass\":\t\"\",\n"
               "\t\"mqtt_use_ssl_client_cert\":\tfalse,\n"
               "\t\"mqtt_use_ssl_server_cert\":\tfalse,\n"
               "\t\"lan_auth_type\":\t\"lan_auth_default\",\n"
               "\t\"lan_auth_user\":\t\"Admin\",\n"
               "\t\"lan_auth_api_key\":\t\"\",\n"
               "\t\"lan_auth_api_key_rw\":\t\"\",\n"
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
               "\t\"coordinates\":\t\"\",\n"
               "\t\"fw_update_url\":\t\"https://network.ruuvi.com/firmwareupdate\"\n"
               "}"),
        string(json_str.p_str));
    ASSERT_TRUE(esp_log_wrapper_is_empty());

    gw_cfg_t gw_cfg2 = get_gateway_config_default();
    ASSERT_TRUE(gw_cfg_json_parse("my.json", nullptr, json_str.p_str, &gw_cfg2));
    cjson_wrap_free_json_str(&json_str);

    ASSERT_TRUE(0 == memcmp(&gw_cfg, &gw_cfg2, sizeof(gw_cfg)));
}

TEST_F(TestGwCfgJson, gw_cfg_json_generate_http_enabled_auth_none) // NOLINT
{
    gw_cfg_t         gw_cfg   = get_gateway_config_default();
    cjson_wrap_str_t json_str = cjson_wrap_str_null();

    gw_cfg.ruuvi_cfg.http.use_http  = true;
    gw_cfg.ruuvi_cfg.http.auth_type = GW_CFG_HTTP_AUTH_TYPE_NONE;
    snprintf(
        gw_cfg.ruuvi_cfg.http.http_url.buf,
        sizeof(gw_cfg.ruuvi_cfg.http.http_url.buf),
        "https://my_url1.com/status");
    ASSERT_TRUE(gw_cfg_json_generate_for_saving(&gw_cfg, &json_str));
    ASSERT_NE(nullptr, json_str.p_str);
    ASSERT_EQ(
        string("{\n"
               "\t\"wifi_sta_config\":\t{\n"
               "\t\t\"ssid\":\t\"\",\n"
               "\t\t\"password\":\t\"\"\n"
               "\t},\n"
               "\t\"wifi_ap_config\":\t{\n"
               "\t\t\"password\":\t\"\",\n"
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
               "\t\"http_use_ssl_client_cert\":\tfalse,\n"
               "\t\"http_use_ssl_server_cert\":\tfalse,\n"
               "\t\"http_url\":\t\""
               "https://my_url1.com/status"
               "\",\n"
               "\t\"http_data_format\":\t\"ruuvi\",\n"
               "\t\"http_auth\":\t\"none\",\n"
               "\t\"use_http_stat\":\ttrue,\n"
               "\t\"http_stat_url\":\t\"" RUUVI_GATEWAY_HTTP_STATUS_URL "\",\n"
               "\t\"http_stat_user\":\t\"\",\n"
               "\t\"http_stat_pass\":\t\"\",\n"
               "\t\"http_stat_use_ssl_client_cert\":\tfalse,\n"
               "\t\"http_stat_use_ssl_server_cert\":\tfalse,\n"
               "\t\"use_mqtt\":\tfalse,\n"
               "\t\"mqtt_disable_retained_messages\":\tfalse,\n"
               "\t\"mqtt_transport\":\t\"TCP\",\n"
               "\t\"mqtt_server\":\t\"test.mosquitto.org\",\n"
               "\t\"mqtt_port\":\t1883,\n"
               "\t\"mqtt_sending_interval\":\t0,\n"
               "\t\"mqtt_prefix\":\t\"ruuvi/AA:BB:CC:DD:EE:FF/\",\n"
               "\t\"mqtt_client_id\":\t\"AA:BB:CC:DD:EE:FF\",\n"
               "\t\"mqtt_user\":\t\"\",\n"
               "\t\"mqtt_pass\":\t\"\",\n"
               "\t\"mqtt_use_ssl_client_cert\":\tfalse,\n"
               "\t\"mqtt_use_ssl_server_cert\":\tfalse,\n"
               "\t\"lan_auth_type\":\t\"lan_auth_default\",\n"
               "\t\"lan_auth_user\":\t\"Admin\",\n"
               "\t\"lan_auth_api_key\":\t\"\",\n"
               "\t\"lan_auth_api_key_rw\":\t\"\",\n"
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
               "\t\"coordinates\":\t\"\",\n"
               "\t\"fw_update_url\":\t\"https://network.ruuvi.com/firmwareupdate\"\n"
               "}"),
        string(json_str.p_str));
    ASSERT_TRUE(esp_log_wrapper_is_empty());

    gw_cfg_t gw_cfg2 = get_gateway_config_default();
    ASSERT_TRUE(gw_cfg_json_parse("my.json", nullptr, json_str.p_str, &gw_cfg2));
    cjson_wrap_free_json_str(&json_str);

    ASSERT_TRUE(0 == memcmp(&gw_cfg, &gw_cfg2, sizeof(gw_cfg)));
}

TEST_F(TestGwCfgJson, gw_cfg_json_generate_http_enabled_auth_basic) // NOLINT
{
    gw_cfg_t         gw_cfg   = get_gateway_config_default();
    cjson_wrap_str_t json_str = cjson_wrap_str_null();

    gw_cfg.ruuvi_cfg.http.use_http  = true;
    gw_cfg.ruuvi_cfg.http.auth_type = GW_CFG_HTTP_AUTH_TYPE_BASIC;
    snprintf(
        gw_cfg.ruuvi_cfg.http.http_url.buf,
        sizeof(gw_cfg.ruuvi_cfg.http.http_url.buf),
        "https://my_url1.com/status");
    snprintf(
        gw_cfg.ruuvi_cfg.http.auth.auth_basic.user.buf,
        sizeof(gw_cfg.ruuvi_cfg.http.auth.auth_basic.user.buf),
        "user2");
    snprintf(
        gw_cfg.ruuvi_cfg.http.auth.auth_basic.password.buf,
        sizeof(gw_cfg.ruuvi_cfg.http.auth.auth_basic.password.buf),
        "pass2");
    ASSERT_TRUE(gw_cfg_json_generate_for_saving(&gw_cfg, &json_str));
    ASSERT_NE(nullptr, json_str.p_str);
    ASSERT_EQ(
        string("{\n"
               "\t\"wifi_sta_config\":\t{\n"
               "\t\t\"ssid\":\t\"\",\n"
               "\t\t\"password\":\t\"\"\n"
               "\t},\n"
               "\t\"wifi_ap_config\":\t{\n"
               "\t\t\"password\":\t\"\",\n"
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
               "\t\"http_use_ssl_client_cert\":\tfalse,\n"
               "\t\"http_use_ssl_server_cert\":\tfalse,\n"
               "\t\"http_url\":\t\""
               "https://my_url1.com/status"
               "\",\n"
               "\t\"http_data_format\":\t\"ruuvi\",\n"
               "\t\"http_auth\":\t\"basic\",\n"
               "\t\"http_user\":\t\"user2\",\n"
               "\t\"http_pass\":\t\"pass2\",\n"
               "\t\"use_http_stat\":\ttrue,\n"
               "\t\"http_stat_url\":\t\"" RUUVI_GATEWAY_HTTP_STATUS_URL "\",\n"
               "\t\"http_stat_user\":\t\"\",\n"
               "\t\"http_stat_pass\":\t\"\",\n"
               "\t\"http_stat_use_ssl_client_cert\":\tfalse,\n"
               "\t\"http_stat_use_ssl_server_cert\":\tfalse,\n"
               "\t\"use_mqtt\":\tfalse,\n"
               "\t\"mqtt_disable_retained_messages\":\tfalse,\n"
               "\t\"mqtt_transport\":\t\"TCP\",\n"
               "\t\"mqtt_server\":\t\"test.mosquitto.org\",\n"
               "\t\"mqtt_port\":\t1883,\n"
               "\t\"mqtt_sending_interval\":\t0,\n"
               "\t\"mqtt_prefix\":\t\"ruuvi/AA:BB:CC:DD:EE:FF/\",\n"
               "\t\"mqtt_client_id\":\t\"AA:BB:CC:DD:EE:FF\",\n"
               "\t\"mqtt_user\":\t\"\",\n"
               "\t\"mqtt_pass\":\t\"\",\n"
               "\t\"mqtt_use_ssl_client_cert\":\tfalse,\n"
               "\t\"mqtt_use_ssl_server_cert\":\tfalse,\n"
               "\t\"lan_auth_type\":\t\"lan_auth_default\",\n"
               "\t\"lan_auth_user\":\t\"Admin\",\n"
               "\t\"lan_auth_api_key\":\t\"\",\n"
               "\t\"lan_auth_api_key_rw\":\t\"\",\n"
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
               "\t\"coordinates\":\t\"\",\n"
               "\t\"fw_update_url\":\t\"https://network.ruuvi.com/firmwareupdate\"\n"
               "}"),
        string(json_str.p_str));
    ASSERT_TRUE(esp_log_wrapper_is_empty());

    gw_cfg_t gw_cfg2 = get_gateway_config_default();
    ASSERT_TRUE(gw_cfg_json_parse("my.json", nullptr, json_str.p_str, &gw_cfg2));
    cjson_wrap_free_json_str(&json_str);

    ASSERT_TRUE(0 == memcmp(&gw_cfg, &gw_cfg2, sizeof(gw_cfg)));
}

TEST_F(TestGwCfgJson, gw_cfg_json_generate_http_enabled_auth_bearer) // NOLINT
{
    gw_cfg_t         gw_cfg   = get_gateway_config_default();
    cjson_wrap_str_t json_str = cjson_wrap_str_null();

    gw_cfg.ruuvi_cfg.http.use_http  = true;
    gw_cfg.ruuvi_cfg.http.auth_type = GW_CFG_HTTP_AUTH_TYPE_BEARER;
    snprintf(
        gw_cfg.ruuvi_cfg.http.http_url.buf,
        sizeof(gw_cfg.ruuvi_cfg.http.http_url.buf),
        "https://my_url1.com/status");
    snprintf(
        gw_cfg.ruuvi_cfg.http.auth.auth_bearer.token.buf,
        sizeof(gw_cfg.ruuvi_cfg.http.auth.auth_bearer.token.buf),
        "token123");
    ASSERT_TRUE(gw_cfg_json_generate_for_saving(&gw_cfg, &json_str));
    ASSERT_NE(nullptr, json_str.p_str);
    ASSERT_EQ(
        string("{\n"
               "\t\"wifi_sta_config\":\t{\n"
               "\t\t\"ssid\":\t\"\",\n"
               "\t\t\"password\":\t\"\"\n"
               "\t},\n"
               "\t\"wifi_ap_config\":\t{\n"
               "\t\t\"password\":\t\"\",\n"
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
               "\t\"http_use_ssl_client_cert\":\tfalse,\n"
               "\t\"http_use_ssl_server_cert\":\tfalse,\n"
               "\t\"http_url\":\t\""
               "https://my_url1.com/status"
               "\",\n"
               "\t\"http_data_format\":\t\"ruuvi\",\n"
               "\t\"http_auth\":\t\"bearer\",\n"
               "\t\"http_bearer_token\":\t\"token123\",\n"
               "\t\"use_http_stat\":\ttrue,\n"
               "\t\"http_stat_url\":\t\"" RUUVI_GATEWAY_HTTP_STATUS_URL "\",\n"
               "\t\"http_stat_user\":\t\"\",\n"
               "\t\"http_stat_pass\":\t\"\",\n"
               "\t\"http_stat_use_ssl_client_cert\":\tfalse,\n"
               "\t\"http_stat_use_ssl_server_cert\":\tfalse,\n"
               "\t\"use_mqtt\":\tfalse,\n"
               "\t\"mqtt_disable_retained_messages\":\tfalse,\n"
               "\t\"mqtt_transport\":\t\"TCP\",\n"
               "\t\"mqtt_server\":\t\"test.mosquitto.org\",\n"
               "\t\"mqtt_port\":\t1883,\n"
               "\t\"mqtt_sending_interval\":\t0,\n"
               "\t\"mqtt_prefix\":\t\"ruuvi/AA:BB:CC:DD:EE:FF/\",\n"
               "\t\"mqtt_client_id\":\t\"AA:BB:CC:DD:EE:FF\",\n"
               "\t\"mqtt_user\":\t\"\",\n"
               "\t\"mqtt_pass\":\t\"\",\n"
               "\t\"mqtt_use_ssl_client_cert\":\tfalse,\n"
               "\t\"mqtt_use_ssl_server_cert\":\tfalse,\n"
               "\t\"lan_auth_type\":\t\"lan_auth_default\",\n"
               "\t\"lan_auth_user\":\t\"Admin\",\n"
               "\t\"lan_auth_api_key\":\t\"\",\n"
               "\t\"lan_auth_api_key_rw\":\t\"\",\n"
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
               "\t\"coordinates\":\t\"\",\n"
               "\t\"fw_update_url\":\t\"https://network.ruuvi.com/firmwareupdate\"\n"
               "}"),
        string(json_str.p_str));
    ASSERT_TRUE(esp_log_wrapper_is_empty());

    gw_cfg_t gw_cfg2 = get_gateway_config_default();
    ASSERT_TRUE(gw_cfg_json_parse("my.json", nullptr, json_str.p_str, &gw_cfg2));
    cjson_wrap_free_json_str(&json_str);

    ASSERT_TRUE(0 == memcmp(&gw_cfg, &gw_cfg2, sizeof(gw_cfg)));
}

TEST_F(TestGwCfgJson, gw_cfg_json_generate_http_enabled_auth_token) // NOLINT
{
    gw_cfg_t         gw_cfg   = get_gateway_config_default();
    cjson_wrap_str_t json_str = cjson_wrap_str_null();

    gw_cfg.ruuvi_cfg.http.use_http  = true;
    gw_cfg.ruuvi_cfg.http.auth_type = GW_CFG_HTTP_AUTH_TYPE_TOKEN;
    snprintf(
        gw_cfg.ruuvi_cfg.http.http_url.buf,
        sizeof(gw_cfg.ruuvi_cfg.http.http_url.buf),
        "https://my_url1.com/status");
    snprintf(
        gw_cfg.ruuvi_cfg.http.auth.auth_token.token.buf,
        sizeof(gw_cfg.ruuvi_cfg.http.auth.auth_token.token.buf),
        "token123");
    ASSERT_TRUE(gw_cfg_json_generate_for_saving(&gw_cfg, &json_str));
    ASSERT_NE(nullptr, json_str.p_str);
    ASSERT_EQ(
        string("{\n"
               "\t\"wifi_sta_config\":\t{\n"
               "\t\t\"ssid\":\t\"\",\n"
               "\t\t\"password\":\t\"\"\n"
               "\t},\n"
               "\t\"wifi_ap_config\":\t{\n"
               "\t\t\"password\":\t\"\",\n"
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
               "\t\"http_use_ssl_client_cert\":\tfalse,\n"
               "\t\"http_use_ssl_server_cert\":\tfalse,\n"
               "\t\"http_url\":\t\""
               "https://my_url1.com/status"
               "\",\n"
               "\t\"http_data_format\":\t\"ruuvi\",\n"
               "\t\"http_auth\":\t\"token\",\n"
               "\t\"http_api_key\":\t\"token123\",\n"
               "\t\"use_http_stat\":\ttrue,\n"
               "\t\"http_stat_url\":\t\"" RUUVI_GATEWAY_HTTP_STATUS_URL "\",\n"
               "\t\"http_stat_user\":\t\"\",\n"
               "\t\"http_stat_pass\":\t\"\",\n"
               "\t\"http_stat_use_ssl_client_cert\":\tfalse,\n"
               "\t\"http_stat_use_ssl_server_cert\":\tfalse,\n"
               "\t\"use_mqtt\":\tfalse,\n"
               "\t\"mqtt_disable_retained_messages\":\tfalse,\n"
               "\t\"mqtt_transport\":\t\"TCP\",\n"
               "\t\"mqtt_server\":\t\"test.mosquitto.org\",\n"
               "\t\"mqtt_port\":\t1883,\n"
               "\t\"mqtt_sending_interval\":\t0,\n"
               "\t\"mqtt_prefix\":\t\"ruuvi/AA:BB:CC:DD:EE:FF/\",\n"
               "\t\"mqtt_client_id\":\t\"AA:BB:CC:DD:EE:FF\",\n"
               "\t\"mqtt_user\":\t\"\",\n"
               "\t\"mqtt_pass\":\t\"\",\n"
               "\t\"mqtt_use_ssl_client_cert\":\tfalse,\n"
               "\t\"mqtt_use_ssl_server_cert\":\tfalse,\n"
               "\t\"lan_auth_type\":\t\"lan_auth_default\",\n"
               "\t\"lan_auth_user\":\t\"Admin\",\n"
               "\t\"lan_auth_api_key\":\t\"\",\n"
               "\t\"lan_auth_api_key_rw\":\t\"\",\n"
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
               "\t\"coordinates\":\t\"\",\n"
               "\t\"fw_update_url\":\t\"https://network.ruuvi.com/firmwareupdate\"\n"
               "}"),
        string(json_str.p_str));
    ASSERT_TRUE(esp_log_wrapper_is_empty());

    gw_cfg_t gw_cfg2 = get_gateway_config_default();
    ASSERT_TRUE(gw_cfg_json_parse("my.json", nullptr, json_str.p_str, &gw_cfg2));
    cjson_wrap_free_json_str(&json_str);

    ASSERT_TRUE(0 == memcmp(&gw_cfg, &gw_cfg2, sizeof(gw_cfg)));
}

TEST_F(TestGwCfgJson, gw_cfg_json_generate_http_enabled_equal_to_default) // NOLINT
{
    gw_cfg_t         gw_cfg   = get_gateway_config_default();
    cjson_wrap_str_t json_str = cjson_wrap_str_null();

    gw_cfg.ruuvi_cfg.http.use_http_ruuvi = false;
    gw_cfg.ruuvi_cfg.http.use_http       = true;
    gw_cfg.ruuvi_cfg.http.auth_type      = GW_CFG_HTTP_AUTH_TYPE_NONE;
    snprintf(
        gw_cfg.ruuvi_cfg.http.http_url.buf,
        sizeof(gw_cfg.ruuvi_cfg.http.http_url.buf),
        "https://network.ruuvi.com/record");
    ASSERT_TRUE(gw_cfg_json_generate_for_saving(&gw_cfg, &json_str));
    ASSERT_NE(nullptr, json_str.p_str);
    ASSERT_EQ(
        string("{\n"
               "\t\"wifi_sta_config\":\t{\n"
               "\t\t\"ssid\":\t\"\",\n"
               "\t\t\"password\":\t\"\"\n"
               "\t},\n"
               "\t\"wifi_ap_config\":\t{\n"
               "\t\t\"password\":\t\"\",\n"
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
               "\t\"http_url\":\t\""
               "https://network.ruuvi.com/record"
               "\",\n"
               "\t\"http_data_format\":\t\"ruuvi\",\n"
               "\t\"http_auth\":\t\"none\",\n"
               "\t\"use_http_stat\":\ttrue,\n"
               "\t\"http_stat_url\":\t\"" RUUVI_GATEWAY_HTTP_STATUS_URL "\",\n"
               "\t\"http_stat_user\":\t\"\",\n"
               "\t\"http_stat_pass\":\t\"\",\n"
               "\t\"http_stat_use_ssl_client_cert\":\tfalse,\n"
               "\t\"http_stat_use_ssl_server_cert\":\tfalse,\n"
               "\t\"use_mqtt\":\tfalse,\n"
               "\t\"mqtt_disable_retained_messages\":\tfalse,\n"
               "\t\"mqtt_transport\":\t\"TCP\",\n"
               "\t\"mqtt_server\":\t\"test.mosquitto.org\",\n"
               "\t\"mqtt_port\":\t1883,\n"
               "\t\"mqtt_sending_interval\":\t0,\n"
               "\t\"mqtt_prefix\":\t\"ruuvi/AA:BB:CC:DD:EE:FF/\",\n"
               "\t\"mqtt_client_id\":\t\"AA:BB:CC:DD:EE:FF\",\n"
               "\t\"mqtt_user\":\t\"\",\n"
               "\t\"mqtt_pass\":\t\"\",\n"
               "\t\"mqtt_use_ssl_client_cert\":\tfalse,\n"
               "\t\"mqtt_use_ssl_server_cert\":\tfalse,\n"
               "\t\"lan_auth_type\":\t\"lan_auth_default\",\n"
               "\t\"lan_auth_user\":\t\"Admin\",\n"
               "\t\"lan_auth_api_key\":\t\"\",\n"
               "\t\"lan_auth_api_key_rw\":\t\"\",\n"
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
               "\t\"coordinates\":\t\"\",\n"
               "\t\"fw_update_url\":\t\"https://network.ruuvi.com/firmwareupdate\"\n"
               "}"),
        string(json_str.p_str));
    ASSERT_TRUE(esp_log_wrapper_is_empty());

    gw_cfg_t gw_cfg2 = get_gateway_config_default();
    ASSERT_TRUE(gw_cfg_json_parse("my.json", nullptr, json_str.p_str, &gw_cfg2));
    cjson_wrap_free_json_str(&json_str);

    ASSERT_TRUE(gw_cfg2.ruuvi_cfg.http.use_http_ruuvi);
    ASSERT_FALSE(gw_cfg2.ruuvi_cfg.http.use_http);
}

TEST_F(TestGwCfgJson, gw_cfg_json_generate_http_enabled_equal_to_default_auth_diff) // NOLINT
{
    gw_cfg_t         gw_cfg   = get_gateway_config_default();
    cjson_wrap_str_t json_str = cjson_wrap_str_null();

    gw_cfg.ruuvi_cfg.http.use_http_ruuvi = false;
    gw_cfg.ruuvi_cfg.http.use_http       = true;
    gw_cfg.ruuvi_cfg.http.auth_type      = GW_CFG_HTTP_AUTH_TYPE_BASIC;
    snprintf(
        gw_cfg.ruuvi_cfg.http.http_url.buf,
        sizeof(gw_cfg.ruuvi_cfg.http.http_url.buf),
        "https://network.ruuvi.com/record");
    snprintf(
        gw_cfg.ruuvi_cfg.http.auth.auth_basic.user.buf,
        sizeof(gw_cfg.ruuvi_cfg.http.auth.auth_basic.user.buf),
        "user2");
    snprintf(
        gw_cfg.ruuvi_cfg.http.auth.auth_basic.password.buf,
        sizeof(gw_cfg.ruuvi_cfg.http.auth.auth_basic.password.buf),
        "pass2");
    ASSERT_TRUE(gw_cfg_json_generate_for_saving(&gw_cfg, &json_str));
    ASSERT_NE(nullptr, json_str.p_str);
    ASSERT_EQ(
        string("{\n"
               "\t\"wifi_sta_config\":\t{\n"
               "\t\t\"ssid\":\t\"\",\n"
               "\t\t\"password\":\t\"\"\n"
               "\t},\n"
               "\t\"wifi_ap_config\":\t{\n"
               "\t\t\"password\":\t\"\",\n"
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
               "\t\"use_http_ruuvi\":\tfalse,\n"
               "\t\"use_http\":\ttrue,\n"
               "\t\"http_use_ssl_client_cert\":\tfalse,\n"
               "\t\"http_use_ssl_server_cert\":\tfalse,\n"
               "\t\"http_url\":\t\""
               "https://network.ruuvi.com/record"
               "\",\n"
               "\t\"http_data_format\":\t\"ruuvi\",\n"
               "\t\"http_auth\":\t\"basic\",\n"
               "\t\"http_user\":\t\"user2\",\n"
               "\t\"http_pass\":\t\"pass2\",\n"
               "\t\"use_http_stat\":\ttrue,\n"
               "\t\"http_stat_url\":\t\"" RUUVI_GATEWAY_HTTP_STATUS_URL "\",\n"
               "\t\"http_stat_user\":\t\"\",\n"
               "\t\"http_stat_pass\":\t\"\",\n"
               "\t\"http_stat_use_ssl_client_cert\":\tfalse,\n"
               "\t\"http_stat_use_ssl_server_cert\":\tfalse,\n"
               "\t\"use_mqtt\":\tfalse,\n"
               "\t\"mqtt_disable_retained_messages\":\tfalse,\n"
               "\t\"mqtt_transport\":\t\"TCP\",\n"
               "\t\"mqtt_server\":\t\"test.mosquitto.org\",\n"
               "\t\"mqtt_port\":\t1883,\n"
               "\t\"mqtt_sending_interval\":\t0,\n"
               "\t\"mqtt_prefix\":\t\"ruuvi/AA:BB:CC:DD:EE:FF/\",\n"
               "\t\"mqtt_client_id\":\t\"AA:BB:CC:DD:EE:FF\",\n"
               "\t\"mqtt_user\":\t\"\",\n"
               "\t\"mqtt_pass\":\t\"\",\n"
               "\t\"mqtt_use_ssl_client_cert\":\tfalse,\n"
               "\t\"mqtt_use_ssl_server_cert\":\tfalse,\n"
               "\t\"lan_auth_type\":\t\"lan_auth_default\",\n"
               "\t\"lan_auth_user\":\t\"Admin\",\n"
               "\t\"lan_auth_api_key\":\t\"\",\n"
               "\t\"lan_auth_api_key_rw\":\t\"\",\n"
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
               "\t\"coordinates\":\t\"\",\n"
               "\t\"fw_update_url\":\t\"https://network.ruuvi.com/firmwareupdate\"\n"
               "}"),
        string(json_str.p_str));
    ASSERT_TRUE(esp_log_wrapper_is_empty());

    gw_cfg_t gw_cfg2 = get_gateway_config_default();
    ASSERT_TRUE(gw_cfg_json_parse("my.json", nullptr, json_str.p_str, &gw_cfg2));
    cjson_wrap_free_json_str(&json_str);

    ASSERT_TRUE(0 == memcmp(&gw_cfg, &gw_cfg2, sizeof(gw_cfg)));
}

TEST_F(TestGwCfgJson, gw_cfg_json_generate_http_stat_disabled) // NOLINT
{
    gw_cfg_t         gw_cfg   = get_gateway_config_default();
    cjson_wrap_str_t json_str = cjson_wrap_str_null();

    gw_cfg.ruuvi_cfg.http_stat.use_http_stat = false;
    ASSERT_TRUE(gw_cfg_json_generate_for_saving(&gw_cfg, &json_str));
    ASSERT_NE(nullptr, json_str.p_str);
    ASSERT_EQ(
        string("{\n"
               "\t\"wifi_sta_config\":\t{\n"
               "\t\t\"ssid\":\t\"\",\n"
               "\t\t\"password\":\t\"\"\n"
               "\t},\n"
               "\t\"wifi_ap_config\":\t{\n"
               "\t\t\"password\":\t\"\",\n"
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
               "\t\"use_http_stat\":\tfalse,\n"
               "\t\"http_stat_url\":\t\"" RUUVI_GATEWAY_HTTP_STATUS_URL "\",\n"
               "\t\"http_stat_user\":\t\"\",\n"
               "\t\"http_stat_pass\":\t\"\",\n"
               "\t\"http_stat_use_ssl_client_cert\":\tfalse,\n"
               "\t\"http_stat_use_ssl_server_cert\":\tfalse,\n"
               "\t\"use_mqtt\":\tfalse,\n"
               "\t\"mqtt_disable_retained_messages\":\tfalse,\n"
               "\t\"mqtt_transport\":\t\"TCP\",\n"
               "\t\"mqtt_server\":\t\"test.mosquitto.org\",\n"
               "\t\"mqtt_port\":\t1883,\n"
               "\t\"mqtt_sending_interval\":\t0,\n"
               "\t\"mqtt_prefix\":\t\"ruuvi/AA:BB:CC:DD:EE:FF/\",\n"
               "\t\"mqtt_client_id\":\t\"AA:BB:CC:DD:EE:FF\",\n"
               "\t\"mqtt_user\":\t\"\",\n"
               "\t\"mqtt_pass\":\t\"\",\n"
               "\t\"mqtt_use_ssl_client_cert\":\tfalse,\n"
               "\t\"mqtt_use_ssl_server_cert\":\tfalse,\n"
               "\t\"lan_auth_type\":\t\"lan_auth_default\",\n"
               "\t\"lan_auth_user\":\t\"Admin\",\n"
               "\t\"lan_auth_api_key\":\t\"\",\n"
               "\t\"lan_auth_api_key_rw\":\t\"\",\n"
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
               "\t\"coordinates\":\t\"\",\n"
               "\t\"fw_update_url\":\t\"https://network.ruuvi.com/firmwareupdate\"\n"
               "}"),
        string(json_str.p_str));
    ASSERT_TRUE(esp_log_wrapper_is_empty());

    gw_cfg_t gw_cfg2 = get_gateway_config_default();
    ASSERT_TRUE(gw_cfg_json_parse("my.json", nullptr, json_str.p_str, &gw_cfg2));
    cjson_wrap_free_json_str(&json_str);

    ASSERT_TRUE(0 == memcmp(&gw_cfg, &gw_cfg2, sizeof(gw_cfg)));
}

TEST_F(TestGwCfgJson, gw_cfg_json_generate_http_stat_enabled) // NOLINT
{
    gw_cfg_t         gw_cfg   = get_gateway_config_default();
    cjson_wrap_str_t json_str = cjson_wrap_str_null();

    gw_cfg.ruuvi_cfg.http_stat.use_http_stat = true;
    snprintf(
        gw_cfg.ruuvi_cfg.http_stat.http_stat_url.buf,
        sizeof(gw_cfg.ruuvi_cfg.http_stat.http_stat_url.buf),
        "https://my_stat_url1.com/status");
    snprintf(
        gw_cfg.ruuvi_cfg.http_stat.http_stat_user.buf,
        sizeof(gw_cfg.ruuvi_cfg.http_stat.http_stat_user.buf),
        "user1");
    snprintf(
        gw_cfg.ruuvi_cfg.http_stat.http_stat_pass.buf,
        sizeof(gw_cfg.ruuvi_cfg.http_stat.http_stat_pass.buf),
        "pass1");
    ASSERT_TRUE(gw_cfg_json_generate_for_saving(&gw_cfg, &json_str));
    ASSERT_NE(nullptr, json_str.p_str);
    ASSERT_EQ(
        string("{\n"
               "\t\"wifi_sta_config\":\t{\n"
               "\t\t\"ssid\":\t\"\",\n"
               "\t\t\"password\":\t\"\"\n"
               "\t},\n"
               "\t\"wifi_ap_config\":\t{\n"
               "\t\t\"password\":\t\"\",\n"
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
               "\t\"http_stat_url\":\t\"https://my_stat_url1.com/status\",\n"
               "\t\"http_stat_user\":\t\"user1\",\n"
               "\t\"http_stat_pass\":\t\"pass1\",\n"
               "\t\"http_stat_use_ssl_client_cert\":\tfalse,\n"
               "\t\"http_stat_use_ssl_server_cert\":\tfalse,\n"
               "\t\"use_mqtt\":\tfalse,\n"
               "\t\"mqtt_disable_retained_messages\":\tfalse,\n"
               "\t\"mqtt_transport\":\t\"TCP\",\n"
               "\t\"mqtt_server\":\t\"test.mosquitto.org\",\n"
               "\t\"mqtt_port\":\t1883,\n"
               "\t\"mqtt_sending_interval\":\t0,\n"
               "\t\"mqtt_prefix\":\t\"ruuvi/AA:BB:CC:DD:EE:FF/\",\n"
               "\t\"mqtt_client_id\":\t\"AA:BB:CC:DD:EE:FF\",\n"
               "\t\"mqtt_user\":\t\"\",\n"
               "\t\"mqtt_pass\":\t\"\",\n"
               "\t\"mqtt_use_ssl_client_cert\":\tfalse,\n"
               "\t\"mqtt_use_ssl_server_cert\":\tfalse,\n"
               "\t\"lan_auth_type\":\t\"lan_auth_default\",\n"
               "\t\"lan_auth_user\":\t\"Admin\",\n"
               "\t\"lan_auth_api_key\":\t\"\",\n"
               "\t\"lan_auth_api_key_rw\":\t\"\",\n"
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
               "\t\"coordinates\":\t\"\",\n"
               "\t\"fw_update_url\":\t\"https://network.ruuvi.com/firmwareupdate\"\n"
               "}"),
        string(json_str.p_str));
    ASSERT_TRUE(esp_log_wrapper_is_empty());

    gw_cfg_t gw_cfg2 = get_gateway_config_default();
    ASSERT_TRUE(gw_cfg_json_parse("my.json", nullptr, json_str.p_str, &gw_cfg2));
    cjson_wrap_free_json_str(&json_str);

    ASSERT_TRUE(0 == memcmp(&gw_cfg, &gw_cfg2, sizeof(gw_cfg)));
}

TEST_F(TestGwCfgJson, gw_cfg_json_generate_lan_auth_default) // NOLINT
{
    gw_cfg_t         gw_cfg   = get_gateway_config_default();
    cjson_wrap_str_t json_str = cjson_wrap_str_null();

    ASSERT_TRUE(gw_cfg_json_generate_for_saving(&gw_cfg, &json_str));
    ASSERT_NE(nullptr, json_str.p_str);
    ASSERT_EQ(
        string("{\n"
               "\t\"wifi_sta_config\":\t{\n"
               "\t\t\"ssid\":\t\"\",\n"
               "\t\t\"password\":\t\"\"\n"
               "\t},\n"
               "\t\"wifi_ap_config\":\t{\n"
               "\t\t\"password\":\t\"\",\n"
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
               "\t\"http_stat_pass\":\t\"\",\n"
               "\t\"http_stat_use_ssl_client_cert\":\tfalse,\n"
               "\t\"http_stat_use_ssl_server_cert\":\tfalse,\n"
               "\t\"use_mqtt\":\tfalse,\n"
               "\t\"mqtt_disable_retained_messages\":\tfalse,\n"
               "\t\"mqtt_transport\":\t\"TCP\",\n"
               "\t\"mqtt_server\":\t\"test.mosquitto.org\",\n"
               "\t\"mqtt_port\":\t1883,\n"
               "\t\"mqtt_sending_interval\":\t0,\n"
               "\t\"mqtt_prefix\":\t\"ruuvi/AA:BB:CC:DD:EE:FF/\",\n"
               "\t\"mqtt_client_id\":\t\"AA:BB:CC:DD:EE:FF\",\n"
               "\t\"mqtt_user\":\t\"\",\n"
               "\t\"mqtt_pass\":\t\"\",\n"
               "\t\"mqtt_use_ssl_client_cert\":\tfalse,\n"
               "\t\"mqtt_use_ssl_server_cert\":\tfalse,\n"
               "\t\"lan_auth_type\":\t\"lan_auth_default\",\n"
               "\t\"lan_auth_user\":\t\"Admin\",\n"
               "\t\"lan_auth_api_key\":\t\"\",\n"
               "\t\"lan_auth_api_key_rw\":\t\"\",\n"
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
               "\t\"coordinates\":\t\"\",\n"
               "\t\"fw_update_url\":\t\"https://network.ruuvi.com/firmwareupdate\"\n"
               "}"),
        string(json_str.p_str));
    ASSERT_TRUE(esp_log_wrapper_is_empty());

    gw_cfg_t gw_cfg2 = get_gateway_config_default();
    ASSERT_TRUE(gw_cfg_json_parse("my.json", nullptr, json_str.p_str, &gw_cfg2));
    cjson_wrap_free_json_str(&json_str);

    ASSERT_TRUE(0 == memcmp(&gw_cfg, &gw_cfg2, sizeof(gw_cfg)));
}

TEST_F(TestGwCfgJson, gw_cfg_json_generate_lan_auth_ruuvi) // NOLINT
{
    gw_cfg_t         gw_cfg   = get_gateway_config_default();
    cjson_wrap_str_t json_str = cjson_wrap_str_null();

    gw_cfg.ruuvi_cfg.lan_auth.lan_auth_type = HTTP_SERVER_AUTH_TYPE_RUUVI;
    snprintf(gw_cfg.ruuvi_cfg.lan_auth.lan_auth_user.buf, sizeof(gw_cfg.ruuvi_cfg.lan_auth.lan_auth_user.buf), "user1");
    snprintf(gw_cfg.ruuvi_cfg.lan_auth.lan_auth_pass.buf, sizeof(gw_cfg.ruuvi_cfg.lan_auth.lan_auth_pass.buf), "pass1");
    gw_cfg.ruuvi_cfg.lan_auth.lan_auth_api_key.buf[0]    = '\0';
    gw_cfg.ruuvi_cfg.lan_auth.lan_auth_api_key_rw.buf[0] = '\0';

    ASSERT_TRUE(gw_cfg_json_generate_for_saving(&gw_cfg, &json_str));
    ASSERT_NE(nullptr, json_str.p_str);
    ASSERT_EQ(
        string("{\n"
               "\t\"wifi_sta_config\":\t{\n"
               "\t\t\"ssid\":\t\"\",\n"
               "\t\t\"password\":\t\"\"\n"
               "\t},\n"
               "\t\"wifi_ap_config\":\t{\n"
               "\t\t\"password\":\t\"\",\n"
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
               "\t\"http_stat_pass\":\t\"\",\n"
               "\t\"http_stat_use_ssl_client_cert\":\tfalse,\n"
               "\t\"http_stat_use_ssl_server_cert\":\tfalse,\n"
               "\t\"use_mqtt\":\tfalse,\n"
               "\t\"mqtt_disable_retained_messages\":\tfalse,\n"
               "\t\"mqtt_transport\":\t\"TCP\",\n"
               "\t\"mqtt_server\":\t\"test.mosquitto.org\",\n"
               "\t\"mqtt_port\":\t1883,\n"
               "\t\"mqtt_sending_interval\":\t0,\n"
               "\t\"mqtt_prefix\":\t\"ruuvi/AA:BB:CC:DD:EE:FF/\",\n"
               "\t\"mqtt_client_id\":\t\"AA:BB:CC:DD:EE:FF\",\n"
               "\t\"mqtt_user\":\t\"\",\n"
               "\t\"mqtt_pass\":\t\"\",\n"
               "\t\"mqtt_use_ssl_client_cert\":\tfalse,\n"
               "\t\"mqtt_use_ssl_server_cert\":\tfalse,\n"
               "\t\"lan_auth_type\":\t\"lan_auth_ruuvi\",\n"
               "\t\"lan_auth_user\":\t\"user1\",\n"
               "\t\"lan_auth_pass\":\t\"pass1\",\n"
               "\t\"lan_auth_api_key\":\t\"\",\n"
               "\t\"lan_auth_api_key_rw\":\t\"\",\n"
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
               "\t\"coordinates\":\t\"\",\n"
               "\t\"fw_update_url\":\t\"https://network.ruuvi.com/firmwareupdate\"\n"
               "}"),
        string(json_str.p_str));
    ASSERT_TRUE(esp_log_wrapper_is_empty());

    gw_cfg_t gw_cfg2 = get_gateway_config_default();
    ASSERT_TRUE(gw_cfg_json_parse("my.json", nullptr, json_str.p_str, &gw_cfg2));
    cjson_wrap_free_json_str(&json_str);

    ASSERT_TRUE(0 == memcmp(&gw_cfg, &gw_cfg2, sizeof(gw_cfg)));
}

TEST_F(TestGwCfgJson, gw_cfg_json_generate_lan_auth_ruuvi_with_api_key) // NOLINT
{
    gw_cfg_t         gw_cfg   = get_gateway_config_default();
    cjson_wrap_str_t json_str = cjson_wrap_str_null();

    gw_cfg.ruuvi_cfg.lan_auth.lan_auth_type = HTTP_SERVER_AUTH_TYPE_RUUVI;
    snprintf(gw_cfg.ruuvi_cfg.lan_auth.lan_auth_user.buf, sizeof(gw_cfg.ruuvi_cfg.lan_auth.lan_auth_user.buf), "user1");
    snprintf(gw_cfg.ruuvi_cfg.lan_auth.lan_auth_pass.buf, sizeof(gw_cfg.ruuvi_cfg.lan_auth.lan_auth_pass.buf), "pass1");
    snprintf(
        gw_cfg.ruuvi_cfg.lan_auth.lan_auth_api_key.buf,
        sizeof(gw_cfg.ruuvi_cfg.lan_auth.lan_auth_api_key.buf),
        "wH3F9SIiAA3rhG32aJki2Z7ekdFc0vtxuDhxl39zFvw=");
    snprintf(
        gw_cfg.ruuvi_cfg.lan_auth.lan_auth_api_key_rw.buf,
        sizeof(gw_cfg.ruuvi_cfg.lan_auth.lan_auth_api_key_rw.buf),
        "KAv9oAT0c1XzbCF9N/Bnj2mgVR7R4QbBn/L3Wq5/zuI=");

    ASSERT_TRUE(gw_cfg_json_generate_for_saving(&gw_cfg, &json_str));
    ASSERT_NE(nullptr, json_str.p_str);
    ASSERT_EQ(
        string("{\n"
               "\t\"wifi_sta_config\":\t{\n"
               "\t\t\"ssid\":\t\"\",\n"
               "\t\t\"password\":\t\"\"\n"
               "\t},\n"
               "\t\"wifi_ap_config\":\t{\n"
               "\t\t\"password\":\t\"\",\n"
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
               "\t\"http_stat_pass\":\t\"\",\n"
               "\t\"http_stat_use_ssl_client_cert\":\tfalse,\n"
               "\t\"http_stat_use_ssl_server_cert\":\tfalse,\n"
               "\t\"use_mqtt\":\tfalse,\n"
               "\t\"mqtt_disable_retained_messages\":\tfalse,\n"
               "\t\"mqtt_transport\":\t\"TCP\",\n"
               "\t\"mqtt_server\":\t\"test.mosquitto.org\",\n"
               "\t\"mqtt_port\":\t1883,\n"
               "\t\"mqtt_sending_interval\":\t0,\n"
               "\t\"mqtt_prefix\":\t\"ruuvi/AA:BB:CC:DD:EE:FF/\",\n"
               "\t\"mqtt_client_id\":\t\"AA:BB:CC:DD:EE:FF\",\n"
               "\t\"mqtt_user\":\t\"\",\n"
               "\t\"mqtt_pass\":\t\"\",\n"
               "\t\"mqtt_use_ssl_client_cert\":\tfalse,\n"
               "\t\"mqtt_use_ssl_server_cert\":\tfalse,\n"
               "\t\"lan_auth_type\":\t\"lan_auth_ruuvi\",\n"
               "\t\"lan_auth_user\":\t\"user1\",\n"
               "\t\"lan_auth_pass\":\t\"pass1\",\n"
               "\t\"lan_auth_api_key\":\t\"wH3F9SIiAA3rhG32aJki2Z7ekdFc0vtxuDhxl39zFvw=\",\n"
               "\t\"lan_auth_api_key_rw\":\t\"KAv9oAT0c1XzbCF9N/Bnj2mgVR7R4QbBn/L3Wq5/zuI=\",\n"
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
               "\t\"coordinates\":\t\"\",\n"
               "\t\"fw_update_url\":\t\"https://network.ruuvi.com/firmwareupdate\"\n"
               "}"),
        string(json_str.p_str));
    ASSERT_TRUE(esp_log_wrapper_is_empty());

    gw_cfg_t gw_cfg2 = get_gateway_config_default();
    ASSERT_TRUE(gw_cfg_json_parse("my.json", nullptr, json_str.p_str, &gw_cfg2));
    cjson_wrap_free_json_str(&json_str);

    ASSERT_TRUE(0 == memcmp(&gw_cfg, &gw_cfg2, sizeof(gw_cfg)));
}

TEST_F(TestGwCfgJson, gw_cfg_json_generate_lan_auth_digest) // NOLINT
{
    gw_cfg_t         gw_cfg   = get_gateway_config_default();
    cjson_wrap_str_t json_str = cjson_wrap_str_null();

    gw_cfg.ruuvi_cfg.lan_auth.lan_auth_type = HTTP_SERVER_AUTH_TYPE_DIGEST;
    snprintf(gw_cfg.ruuvi_cfg.lan_auth.lan_auth_user.buf, sizeof(gw_cfg.ruuvi_cfg.lan_auth.lan_auth_user.buf), "user1");
    snprintf(gw_cfg.ruuvi_cfg.lan_auth.lan_auth_pass.buf, sizeof(gw_cfg.ruuvi_cfg.lan_auth.lan_auth_pass.buf), "pass1");
    gw_cfg.ruuvi_cfg.lan_auth.lan_auth_api_key.buf[0]    = '\0';
    gw_cfg.ruuvi_cfg.lan_auth.lan_auth_api_key_rw.buf[0] = '\0';

    ASSERT_TRUE(gw_cfg_json_generate_for_saving(&gw_cfg, &json_str));
    ASSERT_NE(nullptr, json_str.p_str);
    ASSERT_EQ(
        string("{\n"
               "\t\"wifi_sta_config\":\t{\n"
               "\t\t\"ssid\":\t\"\",\n"
               "\t\t\"password\":\t\"\"\n"
               "\t},\n"
               "\t\"wifi_ap_config\":\t{\n"
               "\t\t\"password\":\t\"\",\n"
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
               "\t\"http_stat_pass\":\t\"\",\n"
               "\t\"http_stat_use_ssl_client_cert\":\tfalse,\n"
               "\t\"http_stat_use_ssl_server_cert\":\tfalse,\n"
               "\t\"use_mqtt\":\tfalse,\n"
               "\t\"mqtt_disable_retained_messages\":\tfalse,\n"
               "\t\"mqtt_transport\":\t\"TCP\",\n"
               "\t\"mqtt_server\":\t\"test.mosquitto.org\",\n"
               "\t\"mqtt_port\":\t1883,\n"
               "\t\"mqtt_sending_interval\":\t0,\n"
               "\t\"mqtt_prefix\":\t\"ruuvi/AA:BB:CC:DD:EE:FF/\",\n"
               "\t\"mqtt_client_id\":\t\"AA:BB:CC:DD:EE:FF\",\n"
               "\t\"mqtt_user\":\t\"\",\n"
               "\t\"mqtt_pass\":\t\"\",\n"
               "\t\"mqtt_use_ssl_client_cert\":\tfalse,\n"
               "\t\"mqtt_use_ssl_server_cert\":\tfalse,\n"
               "\t\"lan_auth_type\":\t\"lan_auth_digest\",\n"
               "\t\"lan_auth_user\":\t\"user1\",\n"
               "\t\"lan_auth_pass\":\t\"pass1\",\n"
               "\t\"lan_auth_api_key\":\t\"\",\n"
               "\t\"lan_auth_api_key_rw\":\t\"\",\n"
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
               "\t\"coordinates\":\t\"\",\n"
               "\t\"fw_update_url\":\t\"https://network.ruuvi.com/firmwareupdate\"\n"
               "}"),
        string(json_str.p_str));
    ASSERT_TRUE(esp_log_wrapper_is_empty());

    gw_cfg_t gw_cfg2 = get_gateway_config_default();
    ASSERT_TRUE(gw_cfg_json_parse("my.json", nullptr, json_str.p_str, &gw_cfg2));
    cjson_wrap_free_json_str(&json_str);

    ASSERT_TRUE(0 == memcmp(&gw_cfg, &gw_cfg2, sizeof(gw_cfg)));
}

TEST_F(TestGwCfgJson, gw_cfg_json_generate_lan_auth_basic) // NOLINT
{
    gw_cfg_t         gw_cfg   = get_gateway_config_default();
    cjson_wrap_str_t json_str = cjson_wrap_str_null();

    gw_cfg.ruuvi_cfg.lan_auth.lan_auth_type = HTTP_SERVER_AUTH_TYPE_BASIC;
    snprintf(gw_cfg.ruuvi_cfg.lan_auth.lan_auth_user.buf, sizeof(gw_cfg.ruuvi_cfg.lan_auth.lan_auth_user.buf), "user1");
    snprintf(gw_cfg.ruuvi_cfg.lan_auth.lan_auth_pass.buf, sizeof(gw_cfg.ruuvi_cfg.lan_auth.lan_auth_pass.buf), "pass1");
    gw_cfg.ruuvi_cfg.lan_auth.lan_auth_api_key.buf[0]    = '\0';
    gw_cfg.ruuvi_cfg.lan_auth.lan_auth_api_key_rw.buf[0] = '\0';

    ASSERT_TRUE(gw_cfg_json_generate_for_saving(&gw_cfg, &json_str));
    ASSERT_NE(nullptr, json_str.p_str);
    ASSERT_EQ(
        string("{\n"
               "\t\"wifi_sta_config\":\t{\n"
               "\t\t\"ssid\":\t\"\",\n"
               "\t\t\"password\":\t\"\"\n"
               "\t},\n"
               "\t\"wifi_ap_config\":\t{\n"
               "\t\t\"password\":\t\"\",\n"
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
               "\t\"http_stat_pass\":\t\"\",\n"
               "\t\"http_stat_use_ssl_client_cert\":\tfalse,\n"
               "\t\"http_stat_use_ssl_server_cert\":\tfalse,\n"
               "\t\"use_mqtt\":\tfalse,\n"
               "\t\"mqtt_disable_retained_messages\":\tfalse,\n"
               "\t\"mqtt_transport\":\t\"TCP\",\n"
               "\t\"mqtt_server\":\t\"test.mosquitto.org\",\n"
               "\t\"mqtt_port\":\t1883,\n"
               "\t\"mqtt_sending_interval\":\t0,\n"
               "\t\"mqtt_prefix\":\t\"ruuvi/AA:BB:CC:DD:EE:FF/\",\n"
               "\t\"mqtt_client_id\":\t\"AA:BB:CC:DD:EE:FF\",\n"
               "\t\"mqtt_user\":\t\"\",\n"
               "\t\"mqtt_pass\":\t\"\",\n"
               "\t\"mqtt_use_ssl_client_cert\":\tfalse,\n"
               "\t\"mqtt_use_ssl_server_cert\":\tfalse,\n"
               "\t\"lan_auth_type\":\t\"lan_auth_basic\",\n"
               "\t\"lan_auth_user\":\t\"user1\",\n"
               "\t\"lan_auth_pass\":\t\"pass1\",\n"
               "\t\"lan_auth_api_key\":\t\"\",\n"
               "\t\"lan_auth_api_key_rw\":\t\"\",\n"
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
               "\t\"coordinates\":\t\"\",\n"
               "\t\"fw_update_url\":\t\"https://network.ruuvi.com/firmwareupdate\"\n"
               "}"),
        string(json_str.p_str));
    ASSERT_TRUE(esp_log_wrapper_is_empty());

    gw_cfg_t gw_cfg2 = get_gateway_config_default();
    ASSERT_TRUE(gw_cfg_json_parse("my.json", nullptr, json_str.p_str, &gw_cfg2));
    cjson_wrap_free_json_str(&json_str);

    ASSERT_TRUE(0 == memcmp(&gw_cfg, &gw_cfg2, sizeof(gw_cfg)));
}

TEST_F(TestGwCfgJson, gw_cfg_json_generate_lan_auth_allow) // NOLINT
{
    gw_cfg_t         gw_cfg   = get_gateway_config_default();
    cjson_wrap_str_t json_str = cjson_wrap_str_null();

    gw_cfg.ruuvi_cfg.lan_auth.lan_auth_type              = HTTP_SERVER_AUTH_TYPE_ALLOW;
    gw_cfg.ruuvi_cfg.lan_auth.lan_auth_user.buf[0]       = '\0';
    gw_cfg.ruuvi_cfg.lan_auth.lan_auth_pass.buf[0]       = '\0';
    gw_cfg.ruuvi_cfg.lan_auth.lan_auth_api_key.buf[0]    = '\0';
    gw_cfg.ruuvi_cfg.lan_auth.lan_auth_api_key_rw.buf[0] = '\0';

    ASSERT_TRUE(gw_cfg_json_generate_for_saving(&gw_cfg, &json_str));
    ASSERT_NE(nullptr, json_str.p_str);
    ASSERT_EQ(
        string("{\n"
               "\t\"wifi_sta_config\":\t{\n"
               "\t\t\"ssid\":\t\"\",\n"
               "\t\t\"password\":\t\"\"\n"
               "\t},\n"
               "\t\"wifi_ap_config\":\t{\n"
               "\t\t\"password\":\t\"\",\n"
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
               "\t\"http_stat_pass\":\t\"\",\n"
               "\t\"http_stat_use_ssl_client_cert\":\tfalse,\n"
               "\t\"http_stat_use_ssl_server_cert\":\tfalse,\n"
               "\t\"use_mqtt\":\tfalse,\n"
               "\t\"mqtt_disable_retained_messages\":\tfalse,\n"
               "\t\"mqtt_transport\":\t\"TCP\",\n"
               "\t\"mqtt_server\":\t\"test.mosquitto.org\",\n"
               "\t\"mqtt_port\":\t1883,\n"
               "\t\"mqtt_sending_interval\":\t0,\n"
               "\t\"mqtt_prefix\":\t\"ruuvi/AA:BB:CC:DD:EE:FF/\",\n"
               "\t\"mqtt_client_id\":\t\"AA:BB:CC:DD:EE:FF\",\n"
               "\t\"mqtt_user\":\t\"\",\n"
               "\t\"mqtt_pass\":\t\"\",\n"
               "\t\"mqtt_use_ssl_client_cert\":\tfalse,\n"
               "\t\"mqtt_use_ssl_server_cert\":\tfalse,\n"
               "\t\"lan_auth_type\":\t\"lan_auth_allow\",\n"
               "\t\"lan_auth_user\":\t\"\",\n"
               "\t\"lan_auth_api_key\":\t\"\",\n"
               "\t\"lan_auth_api_key_rw\":\t\"\",\n"
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
               "\t\"coordinates\":\t\"\",\n"
               "\t\"fw_update_url\":\t\"https://network.ruuvi.com/firmwareupdate\"\n"
               "}"),
        string(json_str.p_str));
    ASSERT_TRUE(esp_log_wrapper_is_empty());

    gw_cfg_t gw_cfg2 = get_gateway_config_default();
    ASSERT_TRUE(gw_cfg_json_parse("my.json", nullptr, json_str.p_str, &gw_cfg2));
    cjson_wrap_free_json_str(&json_str);

    ASSERT_TRUE(0 == memcmp(&gw_cfg, &gw_cfg2, sizeof(gw_cfg)));
}

TEST_F(TestGwCfgJson, gw_cfg_json_generate_lan_auth_deny) // NOLINT
{
    gw_cfg_t         gw_cfg   = get_gateway_config_default();
    cjson_wrap_str_t json_str = cjson_wrap_str_null();

    gw_cfg.ruuvi_cfg.lan_auth.lan_auth_type              = HTTP_SERVER_AUTH_TYPE_DENY;
    gw_cfg.ruuvi_cfg.lan_auth.lan_auth_user.buf[0]       = '\0';
    gw_cfg.ruuvi_cfg.lan_auth.lan_auth_pass.buf[0]       = '\0';
    gw_cfg.ruuvi_cfg.lan_auth.lan_auth_api_key.buf[0]    = '\0';
    gw_cfg.ruuvi_cfg.lan_auth.lan_auth_api_key_rw.buf[0] = '\0';

    ASSERT_TRUE(gw_cfg_json_generate_for_saving(&gw_cfg, &json_str));
    ASSERT_NE(nullptr, json_str.p_str);
    ASSERT_EQ(
        string("{\n"
               "\t\"wifi_sta_config\":\t{\n"
               "\t\t\"ssid\":\t\"\",\n"
               "\t\t\"password\":\t\"\"\n"
               "\t},\n"
               "\t\"wifi_ap_config\":\t{\n"
               "\t\t\"password\":\t\"\",\n"
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
               "\t\"http_stat_pass\":\t\"\",\n"
               "\t\"http_stat_use_ssl_client_cert\":\tfalse,\n"
               "\t\"http_stat_use_ssl_server_cert\":\tfalse,\n"
               "\t\"use_mqtt\":\tfalse,\n"
               "\t\"mqtt_disable_retained_messages\":\tfalse,\n"
               "\t\"mqtt_transport\":\t\"TCP\",\n"
               "\t\"mqtt_server\":\t\"test.mosquitto.org\",\n"
               "\t\"mqtt_port\":\t1883,\n"
               "\t\"mqtt_sending_interval\":\t0,\n"
               "\t\"mqtt_prefix\":\t\"ruuvi/AA:BB:CC:DD:EE:FF/\",\n"
               "\t\"mqtt_client_id\":\t\"AA:BB:CC:DD:EE:FF\",\n"
               "\t\"mqtt_user\":\t\"\",\n"
               "\t\"mqtt_pass\":\t\"\",\n"
               "\t\"mqtt_use_ssl_client_cert\":\tfalse,\n"
               "\t\"mqtt_use_ssl_server_cert\":\tfalse,\n"
               "\t\"lan_auth_type\":\t\"lan_auth_deny\",\n"
               "\t\"lan_auth_user\":\t\"\",\n"
               "\t\"lan_auth_api_key\":\t\"\",\n"
               "\t\"lan_auth_api_key_rw\":\t\"\",\n"
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
               "\t\"coordinates\":\t\"\",\n"
               "\t\"fw_update_url\":\t\"https://network.ruuvi.com/firmwareupdate\"\n"
               "}"),
        string(json_str.p_str));
    ASSERT_TRUE(esp_log_wrapper_is_empty());

    gw_cfg_t gw_cfg2 = get_gateway_config_default();
    ASSERT_TRUE(gw_cfg_json_parse("my.json", nullptr, json_str.p_str, &gw_cfg2));
    cjson_wrap_free_json_str(&json_str);

    ASSERT_TRUE(0 == memcmp(&gw_cfg, &gw_cfg2, sizeof(gw_cfg)));
}

TEST_F(TestGwCfgJson, gw_cfg_json_generate_auto_update_beta_tester) // NOLINT
{
    gw_cfg_t         gw_cfg   = get_gateway_config_default();
    cjson_wrap_str_t json_str = cjson_wrap_str_null();

    gw_cfg.ruuvi_cfg.auto_update.auto_update_cycle            = AUTO_UPDATE_CYCLE_TYPE_BETA_TESTER;
    gw_cfg.ruuvi_cfg.auto_update.auto_update_weekdays_bitmask = 126;
    gw_cfg.ruuvi_cfg.auto_update.auto_update_interval_from    = 1;
    gw_cfg.ruuvi_cfg.auto_update.auto_update_interval_to      = 23;
    gw_cfg.ruuvi_cfg.auto_update.auto_update_tz_offset_hours  = 4;
    ASSERT_TRUE(gw_cfg_json_generate_for_saving(&gw_cfg, &json_str));
    ASSERT_NE(nullptr, json_str.p_str);
    ASSERT_EQ(
        string("{\n"
               "\t\"wifi_sta_config\":\t{\n"
               "\t\t\"ssid\":\t\"\",\n"
               "\t\t\"password\":\t\"\"\n"
               "\t},\n"
               "\t\"wifi_ap_config\":\t{\n"
               "\t\t\"password\":\t\"\",\n"
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
               "\t\"http_stat_pass\":\t\"\",\n"
               "\t\"http_stat_use_ssl_client_cert\":\tfalse,\n"
               "\t\"http_stat_use_ssl_server_cert\":\tfalse,\n"
               "\t\"use_mqtt\":\tfalse,\n"
               "\t\"mqtt_disable_retained_messages\":\tfalse,\n"
               "\t\"mqtt_transport\":\t\"TCP\",\n"
               "\t\"mqtt_server\":\t\"test.mosquitto.org\",\n"
               "\t\"mqtt_port\":\t1883,\n"
               "\t\"mqtt_sending_interval\":\t0,\n"
               "\t\"mqtt_prefix\":\t\"ruuvi/AA:BB:CC:DD:EE:FF/\",\n"
               "\t\"mqtt_client_id\":\t\"AA:BB:CC:DD:EE:FF\",\n"
               "\t\"mqtt_user\":\t\"\",\n"
               "\t\"mqtt_pass\":\t\"\",\n"
               "\t\"mqtt_use_ssl_client_cert\":\tfalse,\n"
               "\t\"mqtt_use_ssl_server_cert\":\tfalse,\n"
               "\t\"lan_auth_type\":\t\"lan_auth_default\",\n"
               "\t\"lan_auth_user\":\t\"Admin\",\n"
               "\t\"lan_auth_api_key\":\t\"\",\n"
               "\t\"lan_auth_api_key_rw\":\t\"\",\n"
               "\t\"auto_update_cycle\":\t\"beta\",\n"
               "\t\"auto_update_weekdays_bitmask\":\t126,\n"
               "\t\"auto_update_interval_from\":\t1,\n"
               "\t\"auto_update_interval_to\":\t23,\n"
               "\t\"auto_update_tz_offset_hours\":\t4,\n"
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
               "\t\"coordinates\":\t\"\",\n"
               "\t\"fw_update_url\":\t\"https://network.ruuvi.com/firmwareupdate\"\n"
               "}"),
        string(json_str.p_str));
    ASSERT_TRUE(esp_log_wrapper_is_empty());

    gw_cfg_t gw_cfg2 = get_gateway_config_default();
    ASSERT_TRUE(gw_cfg_json_parse("my.json", nullptr, json_str.p_str, &gw_cfg2));
    cjson_wrap_free_json_str(&json_str);

    ASSERT_TRUE(0 == memcmp(&gw_cfg, &gw_cfg2, sizeof(gw_cfg)));
}

TEST_F(TestGwCfgJson, gw_cfg_json_generate_auto_update_manual) // NOLINT
{
    gw_cfg_t         gw_cfg   = get_gateway_config_default();
    cjson_wrap_str_t json_str = cjson_wrap_str_null();

    gw_cfg.ruuvi_cfg.auto_update.auto_update_cycle            = AUTO_UPDATE_CYCLE_TYPE_MANUAL;
    gw_cfg.ruuvi_cfg.auto_update.auto_update_weekdays_bitmask = 125;
    gw_cfg.ruuvi_cfg.auto_update.auto_update_interval_from    = 2;
    gw_cfg.ruuvi_cfg.auto_update.auto_update_interval_to      = 22;
    gw_cfg.ruuvi_cfg.auto_update.auto_update_tz_offset_hours  = -4;
    ASSERT_TRUE(gw_cfg_json_generate_for_saving(&gw_cfg, &json_str));
    ASSERT_NE(nullptr, json_str.p_str);
    ASSERT_EQ(
        string("{\n"
               "\t\"wifi_sta_config\":\t{\n"
               "\t\t\"ssid\":\t\"\",\n"
               "\t\t\"password\":\t\"\"\n"
               "\t},\n"
               "\t\"wifi_ap_config\":\t{\n"
               "\t\t\"password\":\t\"\",\n"
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
               "\t\"http_stat_pass\":\t\"\",\n"
               "\t\"http_stat_use_ssl_client_cert\":\tfalse,\n"
               "\t\"http_stat_use_ssl_server_cert\":\tfalse,\n"
               "\t\"use_mqtt\":\tfalse,\n"
               "\t\"mqtt_disable_retained_messages\":\tfalse,\n"
               "\t\"mqtt_transport\":\t\"TCP\",\n"
               "\t\"mqtt_server\":\t\"test.mosquitto.org\",\n"
               "\t\"mqtt_port\":\t1883,\n"
               "\t\"mqtt_sending_interval\":\t0,\n"
               "\t\"mqtt_prefix\":\t\"ruuvi/AA:BB:CC:DD:EE:FF/\",\n"
               "\t\"mqtt_client_id\":\t\"AA:BB:CC:DD:EE:FF\",\n"
               "\t\"mqtt_user\":\t\"\",\n"
               "\t\"mqtt_pass\":\t\"\",\n"
               "\t\"mqtt_use_ssl_client_cert\":\tfalse,\n"
               "\t\"mqtt_use_ssl_server_cert\":\tfalse,\n"
               "\t\"lan_auth_type\":\t\"lan_auth_default\",\n"
               "\t\"lan_auth_user\":\t\"Admin\",\n"
               "\t\"lan_auth_api_key\":\t\"\",\n"
               "\t\"lan_auth_api_key_rw\":\t\"\",\n"
               "\t\"auto_update_cycle\":\t\"manual\",\n"
               "\t\"auto_update_weekdays_bitmask\":\t125,\n"
               "\t\"auto_update_interval_from\":\t2,\n"
               "\t\"auto_update_interval_to\":\t22,\n"
               "\t\"auto_update_tz_offset_hours\":\t-4,\n"
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
               "\t\"coordinates\":\t\"\",\n"
               "\t\"fw_update_url\":\t\"https://network.ruuvi.com/firmwareupdate\"\n"
               "}"),
        string(json_str.p_str));
    ASSERT_TRUE(esp_log_wrapper_is_empty());

    gw_cfg_t gw_cfg2 = get_gateway_config_default();
    ASSERT_TRUE(gw_cfg_json_parse("my.json", nullptr, json_str.p_str, &gw_cfg2));
    cjson_wrap_free_json_str(&json_str);

    ASSERT_TRUE(0 == memcmp(&gw_cfg, &gw_cfg2, sizeof(gw_cfg)));
}

TEST_F(TestGwCfgJson, gw_cfg_json_generate_auto_update_unknown) // NOLINT
{
    gw_cfg_t         gw_cfg   = get_gateway_config_default();
    cjson_wrap_str_t json_str = cjson_wrap_str_null();

    gw_cfg.ruuvi_cfg.auto_update.auto_update_cycle = (auto_update_cycle_type_e)-1;
    ASSERT_TRUE(gw_cfg_json_generate_for_saving(&gw_cfg, &json_str));
    ASSERT_NE(nullptr, json_str.p_str);
    ASSERT_EQ(
        string("{\n"
               "\t\"wifi_sta_config\":\t{\n"
               "\t\t\"ssid\":\t\"\",\n"
               "\t\t\"password\":\t\"\"\n"
               "\t},\n"
               "\t\"wifi_ap_config\":\t{\n"
               "\t\t\"password\":\t\"\",\n"
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
               "\t\"http_stat_pass\":\t\"\",\n"
               "\t\"http_stat_use_ssl_client_cert\":\tfalse,\n"
               "\t\"http_stat_use_ssl_server_cert\":\tfalse,\n"
               "\t\"use_mqtt\":\tfalse,\n"
               "\t\"mqtt_disable_retained_messages\":\tfalse,\n"
               "\t\"mqtt_transport\":\t\"TCP\",\n"
               "\t\"mqtt_server\":\t\"test.mosquitto.org\",\n"
               "\t\"mqtt_port\":\t1883,\n"
               "\t\"mqtt_sending_interval\":\t0,\n"
               "\t\"mqtt_prefix\":\t\"ruuvi/AA:BB:CC:DD:EE:FF/\",\n"
               "\t\"mqtt_client_id\":\t\"AA:BB:CC:DD:EE:FF\",\n"
               "\t\"mqtt_user\":\t\"\",\n"
               "\t\"mqtt_pass\":\t\"\",\n"
               "\t\"mqtt_use_ssl_client_cert\":\tfalse,\n"
               "\t\"mqtt_use_ssl_server_cert\":\tfalse,\n"
               "\t\"lan_auth_type\":\t\"lan_auth_default\",\n"
               "\t\"lan_auth_user\":\t\"Admin\",\n"
               "\t\"lan_auth_api_key\":\t\"\",\n"
               "\t\"lan_auth_api_key_rw\":\t\"\",\n"
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
               "\t\"coordinates\":\t\"\",\n"
               "\t\"fw_update_url\":\t\"https://network.ruuvi.com/firmwareupdate\"\n"
               "}"),
        string(json_str.p_str));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    cjson_wrap_free_json_str(&json_str);
}

TEST_F(TestGwCfgJson, gw_cfg_json_generate_ntp_disabled) // NOLINT
{
    gw_cfg_t         gw_cfg   = get_gateway_config_default();
    cjson_wrap_str_t json_str = cjson_wrap_str_null();

    gw_cfg.ruuvi_cfg.ntp.ntp_use      = false;
    gw_cfg.ruuvi_cfg.ntp.ntp_use_dhcp = false;

    ASSERT_TRUE(gw_cfg_json_generate_for_saving(&gw_cfg, &json_str));
    ASSERT_NE(nullptr, json_str.p_str);
    ASSERT_EQ(
        string("{\n"
               "\t\"wifi_sta_config\":\t{\n"
               "\t\t\"ssid\":\t\"\",\n"
               "\t\t\"password\":\t\"\"\n"
               "\t},\n"
               "\t\"wifi_ap_config\":\t{\n"
               "\t\t\"password\":\t\"\",\n"
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
               "\t\"http_stat_pass\":\t\"\",\n"
               "\t\"http_stat_use_ssl_client_cert\":\tfalse,\n"
               "\t\"http_stat_use_ssl_server_cert\":\tfalse,\n"
               "\t\"use_mqtt\":\tfalse,\n"
               "\t\"mqtt_disable_retained_messages\":\tfalse,\n"
               "\t\"mqtt_transport\":\t\"TCP\",\n"
               "\t\"mqtt_server\":\t\"test.mosquitto.org\",\n"
               "\t\"mqtt_port\":\t1883,\n"
               "\t\"mqtt_sending_interval\":\t0,\n"
               "\t\"mqtt_prefix\":\t\"ruuvi/AA:BB:CC:DD:EE:FF/\",\n"
               "\t\"mqtt_client_id\":\t\"AA:BB:CC:DD:EE:FF\",\n"
               "\t\"mqtt_user\":\t\"\",\n"
               "\t\"mqtt_pass\":\t\"\",\n"
               "\t\"mqtt_use_ssl_client_cert\":\tfalse,\n"
               "\t\"mqtt_use_ssl_server_cert\":\tfalse,\n"
               "\t\"lan_auth_type\":\t\"lan_auth_default\",\n"
               "\t\"lan_auth_user\":\t\"Admin\",\n"
               "\t\"lan_auth_api_key\":\t\"\",\n"
               "\t\"lan_auth_api_key_rw\":\t\"\",\n"
               "\t\"auto_update_cycle\":\t\"regular\",\n"
               "\t\"auto_update_weekdays_bitmask\":\t127,\n"
               "\t\"auto_update_interval_from\":\t0,\n"
               "\t\"auto_update_interval_to\":\t24,\n"
               "\t\"auto_update_tz_offset_hours\":\t3,\n"
               "\t\"ntp_use\":\tfalse,\n"
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
               "\t\"coordinates\":\t\"\",\n"
               "\t\"fw_update_url\":\t\"https://network.ruuvi.com/firmwareupdate\"\n"
               "}"),
        string(json_str.p_str));
    ASSERT_TRUE(esp_log_wrapper_is_empty());

    gw_cfg_t gw_cfg2 = get_gateway_config_default();
    ASSERT_TRUE(gw_cfg_json_parse("my.json", nullptr, json_str.p_str, &gw_cfg2));
    cjson_wrap_free_json_str(&json_str);

    ASSERT_TRUE(0 == memcmp(&gw_cfg, &gw_cfg2, sizeof(gw_cfg)));
}

TEST_F(TestGwCfgJson, gw_cfg_json_generate_ntp_enabled_via_dhcp) // NOLINT
{
    gw_cfg_t         gw_cfg   = get_gateway_config_default();
    cjson_wrap_str_t json_str = cjson_wrap_str_null();

    gw_cfg.ruuvi_cfg.ntp.ntp_use      = true;
    gw_cfg.ruuvi_cfg.ntp.ntp_use_dhcp = true;

    ASSERT_TRUE(gw_cfg_json_generate_for_saving(&gw_cfg, &json_str));
    ASSERT_NE(nullptr, json_str.p_str);
    ASSERT_EQ(
        string("{\n"
               "\t\"wifi_sta_config\":\t{\n"
               "\t\t\"ssid\":\t\"\",\n"
               "\t\t\"password\":\t\"\"\n"
               "\t},\n"
               "\t\"wifi_ap_config\":\t{\n"
               "\t\t\"password\":\t\"\",\n"
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
               "\t\"http_stat_pass\":\t\"\",\n"
               "\t\"http_stat_use_ssl_client_cert\":\tfalse,\n"
               "\t\"http_stat_use_ssl_server_cert\":\tfalse,\n"
               "\t\"use_mqtt\":\tfalse,\n"
               "\t\"mqtt_disable_retained_messages\":\tfalse,\n"
               "\t\"mqtt_transport\":\t\"TCP\",\n"
               "\t\"mqtt_server\":\t\"test.mosquitto.org\",\n"
               "\t\"mqtt_port\":\t1883,\n"
               "\t\"mqtt_sending_interval\":\t0,\n"
               "\t\"mqtt_prefix\":\t\"ruuvi/AA:BB:CC:DD:EE:FF/\",\n"
               "\t\"mqtt_client_id\":\t\"AA:BB:CC:DD:EE:FF\",\n"
               "\t\"mqtt_user\":\t\"\",\n"
               "\t\"mqtt_pass\":\t\"\",\n"
               "\t\"mqtt_use_ssl_client_cert\":\tfalse,\n"
               "\t\"mqtt_use_ssl_server_cert\":\tfalse,\n"
               "\t\"lan_auth_type\":\t\"lan_auth_default\",\n"
               "\t\"lan_auth_user\":\t\"Admin\",\n"
               "\t\"lan_auth_api_key\":\t\"\",\n"
               "\t\"lan_auth_api_key_rw\":\t\"\",\n"
               "\t\"auto_update_cycle\":\t\"regular\",\n"
               "\t\"auto_update_weekdays_bitmask\":\t127,\n"
               "\t\"auto_update_interval_from\":\t0,\n"
               "\t\"auto_update_interval_to\":\t24,\n"
               "\t\"auto_update_tz_offset_hours\":\t3,\n"
               "\t\"ntp_use\":\ttrue,\n"
               "\t\"ntp_use_dhcp\":\ttrue,\n"
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
               "\t\"coordinates\":\t\"\",\n"
               "\t\"fw_update_url\":\t\"https://network.ruuvi.com/firmwareupdate\"\n"
               "}"),
        string(json_str.p_str));
    ASSERT_TRUE(esp_log_wrapper_is_empty());

    gw_cfg_t gw_cfg2 = get_gateway_config_default();
    ASSERT_TRUE(gw_cfg_json_parse("my.json", nullptr, json_str.p_str, &gw_cfg2));
    cjson_wrap_free_json_str(&json_str);

    ASSERT_TRUE(0 == memcmp(&gw_cfg, &gw_cfg2, sizeof(gw_cfg)));
}

TEST_F(TestGwCfgJson, gw_cfg_json_generate_ntp_custom) // NOLINT
{
    gw_cfg_t         gw_cfg   = get_gateway_config_default();
    cjson_wrap_str_t json_str = cjson_wrap_str_null();

    memset(&gw_cfg.ruuvi_cfg.ntp, 0, sizeof(gw_cfg.ruuvi_cfg.ntp));
    gw_cfg.ruuvi_cfg.ntp.ntp_use      = true;
    gw_cfg.ruuvi_cfg.ntp.ntp_use_dhcp = false;
    memset(gw_cfg.ruuvi_cfg.ntp.ntp_server1.buf, 0, sizeof(gw_cfg.ruuvi_cfg.ntp.ntp_server1.buf));
    memset(gw_cfg.ruuvi_cfg.ntp.ntp_server2.buf, 0, sizeof(gw_cfg.ruuvi_cfg.ntp.ntp_server2.buf));
    memset(gw_cfg.ruuvi_cfg.ntp.ntp_server3.buf, 0, sizeof(gw_cfg.ruuvi_cfg.ntp.ntp_server3.buf));
    memset(gw_cfg.ruuvi_cfg.ntp.ntp_server4.buf, 0, sizeof(gw_cfg.ruuvi_cfg.ntp.ntp_server4.buf));
    snprintf(gw_cfg.ruuvi_cfg.ntp.ntp_server1.buf, sizeof(gw_cfg.ruuvi_cfg.ntp.ntp_server1.buf), "time1.server.com");
    snprintf(gw_cfg.ruuvi_cfg.ntp.ntp_server2.buf, sizeof(gw_cfg.ruuvi_cfg.ntp.ntp_server2.buf), "time2.server.com");
    snprintf(gw_cfg.ruuvi_cfg.ntp.ntp_server3.buf, sizeof(gw_cfg.ruuvi_cfg.ntp.ntp_server3.buf), "time3.server.com");
    snprintf(gw_cfg.ruuvi_cfg.ntp.ntp_server4.buf, sizeof(gw_cfg.ruuvi_cfg.ntp.ntp_server4.buf), "time4.server.com");

    ASSERT_TRUE(gw_cfg_json_generate_for_saving(&gw_cfg, &json_str));
    ASSERT_NE(nullptr, json_str.p_str);
    ASSERT_EQ(
        string("{\n"
               "\t\"wifi_sta_config\":\t{\n"
               "\t\t\"ssid\":\t\"\",\n"
               "\t\t\"password\":\t\"\"\n"
               "\t},\n"
               "\t\"wifi_ap_config\":\t{\n"
               "\t\t\"password\":\t\"\",\n"
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
               "\t\"http_stat_pass\":\t\"\",\n"
               "\t\"http_stat_use_ssl_client_cert\":\tfalse,\n"
               "\t\"http_stat_use_ssl_server_cert\":\tfalse,\n"
               "\t\"use_mqtt\":\tfalse,\n"
               "\t\"mqtt_disable_retained_messages\":\tfalse,\n"
               "\t\"mqtt_transport\":\t\"TCP\",\n"
               "\t\"mqtt_server\":\t\"test.mosquitto.org\",\n"
               "\t\"mqtt_port\":\t1883,\n"
               "\t\"mqtt_sending_interval\":\t0,\n"
               "\t\"mqtt_prefix\":\t\"ruuvi/AA:BB:CC:DD:EE:FF/\",\n"
               "\t\"mqtt_client_id\":\t\"AA:BB:CC:DD:EE:FF\",\n"
               "\t\"mqtt_user\":\t\"\",\n"
               "\t\"mqtt_pass\":\t\"\",\n"
               "\t\"mqtt_use_ssl_client_cert\":\tfalse,\n"
               "\t\"mqtt_use_ssl_server_cert\":\tfalse,\n"
               "\t\"lan_auth_type\":\t\"lan_auth_default\",\n"
               "\t\"lan_auth_user\":\t\"Admin\",\n"
               "\t\"lan_auth_api_key\":\t\"\",\n"
               "\t\"lan_auth_api_key_rw\":\t\"\",\n"
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
               "\t\"coordinates\":\t\"\",\n"
               "\t\"fw_update_url\":\t\"https://network.ruuvi.com/firmwareupdate\"\n"
               "}"),
        string(json_str.p_str));
    ASSERT_TRUE(esp_log_wrapper_is_empty());

    gw_cfg_t gw_cfg2 = get_gateway_config_default();
    ASSERT_TRUE(gw_cfg_json_parse("my.json", nullptr, json_str.p_str, &gw_cfg2));
    cjson_wrap_free_json_str(&json_str);

    ASSERT_TRUE(gw_cfg_ruuvi_cmp(&gw_cfg.ruuvi_cfg, &gw_cfg2.ruuvi_cfg));
}

TEST_F(TestGwCfgJson, gw_cfg_json_generate_filter_enabled) // NOLINT
{
    gw_cfg_t         gw_cfg   = get_gateway_config_default();
    cjson_wrap_str_t json_str = cjson_wrap_str_null();

    gw_cfg.ruuvi_cfg.filter.company_id            = 1234;
    gw_cfg.ruuvi_cfg.filter.company_use_filtering = true;

    ASSERT_TRUE(gw_cfg_json_generate_for_saving(&gw_cfg, &json_str));
    ASSERT_NE(nullptr, json_str.p_str);
    ASSERT_EQ(
        string("{\n"
               "\t\"wifi_sta_config\":\t{\n"
               "\t\t\"ssid\":\t\"\",\n"
               "\t\t\"password\":\t\"\"\n"
               "\t},\n"
               "\t\"wifi_ap_config\":\t{\n"
               "\t\t\"password\":\t\"\",\n"
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
               "\t\"http_stat_pass\":\t\"\",\n"
               "\t\"http_stat_use_ssl_client_cert\":\tfalse,\n"
               "\t\"http_stat_use_ssl_server_cert\":\tfalse,\n"
               "\t\"use_mqtt\":\tfalse,\n"
               "\t\"mqtt_disable_retained_messages\":\tfalse,\n"
               "\t\"mqtt_transport\":\t\"TCP\",\n"
               "\t\"mqtt_server\":\t\"test.mosquitto.org\",\n"
               "\t\"mqtt_port\":\t1883,\n"
               "\t\"mqtt_sending_interval\":\t0,\n"
               "\t\"mqtt_prefix\":\t\"ruuvi/AA:BB:CC:DD:EE:FF/\",\n"
               "\t\"mqtt_client_id\":\t\"AA:BB:CC:DD:EE:FF\",\n"
               "\t\"mqtt_user\":\t\"\",\n"
               "\t\"mqtt_pass\":\t\"\",\n"
               "\t\"mqtt_use_ssl_client_cert\":\tfalse,\n"
               "\t\"mqtt_use_ssl_server_cert\":\tfalse,\n"
               "\t\"lan_auth_type\":\t\"lan_auth_default\",\n"
               "\t\"lan_auth_user\":\t\"Admin\",\n"
               "\t\"lan_auth_api_key\":\t\"\",\n"
               "\t\"lan_auth_api_key_rw\":\t\"\",\n"
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
               "\t\"company_id\":\t1234,\n"
               "\t\"company_use_filtering\":\ttrue,\n"
               "\t\"scan_coded_phy\":\tfalse,\n"
               "\t\"scan_1mbit_phy\":\ttrue,\n"
               "\t\"scan_extended_payload\":\ttrue,\n"
               "\t\"scan_channel_37\":\ttrue,\n"
               "\t\"scan_channel_38\":\ttrue,\n"
               "\t\"scan_channel_39\":\ttrue,\n"
               "\t\"scan_filter_allow_listed\":\tfalse,\n"
               "\t\"scan_filter_list\":\t[],\n"
               "\t\"coordinates\":\t\"\",\n"
               "\t\"fw_update_url\":\t\"https://network.ruuvi.com/firmwareupdate\"\n"
               "}"),
        string(json_str.p_str));
    ASSERT_TRUE(esp_log_wrapper_is_empty());

    gw_cfg_t gw_cfg2 = get_gateway_config_default();
    ASSERT_TRUE(gw_cfg_json_parse("my.json", nullptr, json_str.p_str, &gw_cfg2));
    cjson_wrap_free_json_str(&json_str);

    ASSERT_TRUE(0 == memcmp(&gw_cfg, &gw_cfg2, sizeof(gw_cfg)));
}

TEST_F(TestGwCfgJson, gw_cfg_json_generate_filter_disabled) // NOLINT
{
    gw_cfg_t         gw_cfg   = get_gateway_config_default();
    cjson_wrap_str_t json_str = cjson_wrap_str_null();

    gw_cfg.ruuvi_cfg.filter.company_id            = 1235;
    gw_cfg.ruuvi_cfg.filter.company_use_filtering = false;

    ASSERT_TRUE(gw_cfg_json_generate_for_saving(&gw_cfg, &json_str));
    ASSERT_NE(nullptr, json_str.p_str);
    ASSERT_EQ(
        string("{\n"
               "\t\"wifi_sta_config\":\t{\n"
               "\t\t\"ssid\":\t\"\",\n"
               "\t\t\"password\":\t\"\"\n"
               "\t},\n"
               "\t\"wifi_ap_config\":\t{\n"
               "\t\t\"password\":\t\"\",\n"
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
               "\t\"http_stat_pass\":\t\"\",\n"
               "\t\"http_stat_use_ssl_client_cert\":\tfalse,\n"
               "\t\"http_stat_use_ssl_server_cert\":\tfalse,\n"
               "\t\"use_mqtt\":\tfalse,\n"
               "\t\"mqtt_disable_retained_messages\":\tfalse,\n"
               "\t\"mqtt_transport\":\t\"TCP\",\n"
               "\t\"mqtt_server\":\t\"test.mosquitto.org\",\n"
               "\t\"mqtt_port\":\t1883,\n"
               "\t\"mqtt_sending_interval\":\t0,\n"
               "\t\"mqtt_prefix\":\t\"ruuvi/AA:BB:CC:DD:EE:FF/\",\n"
               "\t\"mqtt_client_id\":\t\"AA:BB:CC:DD:EE:FF\",\n"
               "\t\"mqtt_user\":\t\"\",\n"
               "\t\"mqtt_pass\":\t\"\",\n"
               "\t\"mqtt_use_ssl_client_cert\":\tfalse,\n"
               "\t\"mqtt_use_ssl_server_cert\":\tfalse,\n"
               "\t\"lan_auth_type\":\t\"lan_auth_default\",\n"
               "\t\"lan_auth_user\":\t\"Admin\",\n"
               "\t\"lan_auth_api_key\":\t\"\",\n"
               "\t\"lan_auth_api_key_rw\":\t\"\",\n"
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
               "\t\"company_id\":\t1235,\n"
               "\t\"company_use_filtering\":\tfalse,\n"
               "\t\"scan_coded_phy\":\tfalse,\n"
               "\t\"scan_1mbit_phy\":\ttrue,\n"
               "\t\"scan_extended_payload\":\ttrue,\n"
               "\t\"scan_channel_37\":\ttrue,\n"
               "\t\"scan_channel_38\":\ttrue,\n"
               "\t\"scan_channel_39\":\ttrue,\n"
               "\t\"scan_filter_allow_listed\":\tfalse,\n"
               "\t\"scan_filter_list\":\t[],\n"
               "\t\"coordinates\":\t\"\",\n"
               "\t\"fw_update_url\":\t\"https://network.ruuvi.com/firmwareupdate\"\n"
               "}"),
        string(json_str.p_str));
    ASSERT_TRUE(esp_log_wrapper_is_empty());

    gw_cfg_t gw_cfg2 = get_gateway_config_default();
    ASSERT_TRUE(gw_cfg_json_parse("my.json", nullptr, json_str.p_str, &gw_cfg2));
    cjson_wrap_free_json_str(&json_str);

    ASSERT_TRUE(0 == memcmp(&gw_cfg, &gw_cfg2, sizeof(gw_cfg)));
}

TEST_F(TestGwCfgJson, gw_cfg_json_generate_scan_default) // NOLINT
{
    gw_cfg_t         gw_cfg   = get_gateway_config_default();
    cjson_wrap_str_t json_str = cjson_wrap_str_null();

    gw_cfg.ruuvi_cfg.scan.scan_coded_phy                  = false;
    gw_cfg.ruuvi_cfg.scan.scan_1mbit_phy                  = true;
    gw_cfg.ruuvi_cfg.scan.scan_extended_payload           = true;
    gw_cfg.ruuvi_cfg.scan.scan_channel_37                 = true;
    gw_cfg.ruuvi_cfg.scan.scan_channel_38                 = true;
    gw_cfg.ruuvi_cfg.scan.scan_channel_39                 = true;
    gw_cfg.ruuvi_cfg.scan_filter.scan_filter_allow_listed = false;
    gw_cfg.ruuvi_cfg.scan_filter.scan_filter_length       = 0;

    ASSERT_TRUE(gw_cfg_json_generate_for_saving(&gw_cfg, &json_str));
    ASSERT_NE(nullptr, json_str.p_str);
    ASSERT_EQ(
        string("{\n"
               "\t\"wifi_sta_config\":\t{\n"
               "\t\t\"ssid\":\t\"\",\n"
               "\t\t\"password\":\t\"\"\n"
               "\t},\n"
               "\t\"wifi_ap_config\":\t{\n"
               "\t\t\"password\":\t\"\",\n"
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
               "\t\"http_stat_pass\":\t\"\",\n"
               "\t\"http_stat_use_ssl_client_cert\":\tfalse,\n"
               "\t\"http_stat_use_ssl_server_cert\":\tfalse,\n"
               "\t\"use_mqtt\":\tfalse,\n"
               "\t\"mqtt_disable_retained_messages\":\tfalse,\n"
               "\t\"mqtt_transport\":\t\"TCP\",\n"
               "\t\"mqtt_server\":\t\"test.mosquitto.org\",\n"
               "\t\"mqtt_port\":\t1883,\n"
               "\t\"mqtt_sending_interval\":\t0,\n"
               "\t\"mqtt_prefix\":\t\"ruuvi/AA:BB:CC:DD:EE:FF/\",\n"
               "\t\"mqtt_client_id\":\t\"AA:BB:CC:DD:EE:FF\",\n"
               "\t\"mqtt_user\":\t\"\",\n"
               "\t\"mqtt_pass\":\t\"\",\n"
               "\t\"mqtt_use_ssl_client_cert\":\tfalse,\n"
               "\t\"mqtt_use_ssl_server_cert\":\tfalse,\n"
               "\t\"lan_auth_type\":\t\"lan_auth_default\",\n"
               "\t\"lan_auth_user\":\t\"Admin\",\n"
               "\t\"lan_auth_api_key\":\t\"\",\n"
               "\t\"lan_auth_api_key_rw\":\t\"\",\n"
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
               "\t\"coordinates\":\t\"\",\n"
               "\t\"fw_update_url\":\t\"https://network.ruuvi.com/firmwareupdate\"\n"
               "}"),
        string(json_str.p_str));
    ASSERT_TRUE(esp_log_wrapper_is_empty());

    gw_cfg_t gw_cfg2 = get_gateway_config_default();
    ASSERT_TRUE(gw_cfg_json_parse("my.json", nullptr, json_str.p_str, &gw_cfg2));
    cjson_wrap_free_json_str(&json_str);

    ASSERT_TRUE(0 == memcmp(&gw_cfg, &gw_cfg2, sizeof(gw_cfg)));
}

TEST_F(TestGwCfgJson, gw_cfg_json_generate_scan_coded_phy_true) // NOLINT
{
    gw_cfg_t         gw_cfg   = get_gateway_config_default();
    cjson_wrap_str_t json_str = cjson_wrap_str_null();

    gw_cfg.ruuvi_cfg.scan.scan_coded_phy                  = true;
    gw_cfg.ruuvi_cfg.scan.scan_1mbit_phy                  = true;
    gw_cfg.ruuvi_cfg.scan.scan_extended_payload           = true;
    gw_cfg.ruuvi_cfg.scan.scan_channel_37                 = true;
    gw_cfg.ruuvi_cfg.scan.scan_channel_38                 = true;
    gw_cfg.ruuvi_cfg.scan.scan_channel_39                 = true;
    gw_cfg.ruuvi_cfg.scan_filter.scan_filter_allow_listed = false;
    gw_cfg.ruuvi_cfg.scan_filter.scan_filter_length       = 0;

    ASSERT_TRUE(gw_cfg_json_generate_for_saving(&gw_cfg, &json_str));
    ASSERT_NE(nullptr, json_str.p_str);
    ASSERT_EQ(
        string("{\n"
               "\t\"wifi_sta_config\":\t{\n"
               "\t\t\"ssid\":\t\"\",\n"
               "\t\t\"password\":\t\"\"\n"
               "\t},\n"
               "\t\"wifi_ap_config\":\t{\n"
               "\t\t\"password\":\t\"\",\n"
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
               "\t\"http_stat_pass\":\t\"\",\n"
               "\t\"http_stat_use_ssl_client_cert\":\tfalse,\n"
               "\t\"http_stat_use_ssl_server_cert\":\tfalse,\n"
               "\t\"use_mqtt\":\tfalse,\n"
               "\t\"mqtt_disable_retained_messages\":\tfalse,\n"
               "\t\"mqtt_transport\":\t\"TCP\",\n"
               "\t\"mqtt_server\":\t\"test.mosquitto.org\",\n"
               "\t\"mqtt_port\":\t1883,\n"
               "\t\"mqtt_sending_interval\":\t0,\n"
               "\t\"mqtt_prefix\":\t\"ruuvi/AA:BB:CC:DD:EE:FF/\",\n"
               "\t\"mqtt_client_id\":\t\"AA:BB:CC:DD:EE:FF\",\n"
               "\t\"mqtt_user\":\t\"\",\n"
               "\t\"mqtt_pass\":\t\"\",\n"
               "\t\"mqtt_use_ssl_client_cert\":\tfalse,\n"
               "\t\"mqtt_use_ssl_server_cert\":\tfalse,\n"
               "\t\"lan_auth_type\":\t\"lan_auth_default\",\n"
               "\t\"lan_auth_user\":\t\"Admin\",\n"
               "\t\"lan_auth_api_key\":\t\"\",\n"
               "\t\"lan_auth_api_key_rw\":\t\"\",\n"
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
               "\t\"scan_coded_phy\":\ttrue,\n"
               "\t\"scan_1mbit_phy\":\ttrue,\n"
               "\t\"scan_extended_payload\":\ttrue,\n"
               "\t\"scan_channel_37\":\ttrue,\n"
               "\t\"scan_channel_38\":\ttrue,\n"
               "\t\"scan_channel_39\":\ttrue,\n"
               "\t\"scan_filter_allow_listed\":\tfalse,\n"
               "\t\"scan_filter_list\":\t[],\n"
               "\t\"coordinates\":\t\"\",\n"
               "\t\"fw_update_url\":\t\"https://network.ruuvi.com/firmwareupdate\"\n"
               "}"),
        string(json_str.p_str));
    ASSERT_TRUE(esp_log_wrapper_is_empty());

    gw_cfg_t gw_cfg2 = get_gateway_config_default();
    ASSERT_TRUE(gw_cfg_json_parse("my.json", nullptr, json_str.p_str, &gw_cfg2));
    cjson_wrap_free_json_str(&json_str);

    ASSERT_TRUE(0 == memcmp(&gw_cfg, &gw_cfg2, sizeof(gw_cfg)));
}

TEST_F(TestGwCfgJson, gw_cfg_json_generate_scan_1mbit_phy_false) // NOLINT
{
    gw_cfg_t         gw_cfg   = get_gateway_config_default();
    cjson_wrap_str_t json_str = cjson_wrap_str_null();

    gw_cfg.ruuvi_cfg.scan.scan_coded_phy                  = false;
    gw_cfg.ruuvi_cfg.scan.scan_1mbit_phy                  = false;
    gw_cfg.ruuvi_cfg.scan.scan_extended_payload           = true;
    gw_cfg.ruuvi_cfg.scan.scan_channel_37                 = true;
    gw_cfg.ruuvi_cfg.scan.scan_channel_38                 = true;
    gw_cfg.ruuvi_cfg.scan.scan_channel_39                 = true;
    gw_cfg.ruuvi_cfg.scan_filter.scan_filter_allow_listed = false;
    gw_cfg.ruuvi_cfg.scan_filter.scan_filter_length       = 0;

    ASSERT_TRUE(gw_cfg_json_generate_for_saving(&gw_cfg, &json_str));
    ASSERT_NE(nullptr, json_str.p_str);
    ASSERT_EQ(
        string("{\n"
               "\t\"wifi_sta_config\":\t{\n"
               "\t\t\"ssid\":\t\"\",\n"
               "\t\t\"password\":\t\"\"\n"
               "\t},\n"
               "\t\"wifi_ap_config\":\t{\n"
               "\t\t\"password\":\t\"\",\n"
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
               "\t\"http_stat_pass\":\t\"\",\n"
               "\t\"http_stat_use_ssl_client_cert\":\tfalse,\n"
               "\t\"http_stat_use_ssl_server_cert\":\tfalse,\n"
               "\t\"use_mqtt\":\tfalse,\n"
               "\t\"mqtt_disable_retained_messages\":\tfalse,\n"
               "\t\"mqtt_transport\":\t\"TCP\",\n"
               "\t\"mqtt_server\":\t\"test.mosquitto.org\",\n"
               "\t\"mqtt_port\":\t1883,\n"
               "\t\"mqtt_sending_interval\":\t0,\n"
               "\t\"mqtt_prefix\":\t\"ruuvi/AA:BB:CC:DD:EE:FF/\",\n"
               "\t\"mqtt_client_id\":\t\"AA:BB:CC:DD:EE:FF\",\n"
               "\t\"mqtt_user\":\t\"\",\n"
               "\t\"mqtt_pass\":\t\"\",\n"
               "\t\"mqtt_use_ssl_client_cert\":\tfalse,\n"
               "\t\"mqtt_use_ssl_server_cert\":\tfalse,\n"
               "\t\"lan_auth_type\":\t\"lan_auth_default\",\n"
               "\t\"lan_auth_user\":\t\"Admin\",\n"
               "\t\"lan_auth_api_key\":\t\"\",\n"
               "\t\"lan_auth_api_key_rw\":\t\"\",\n"
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
               "\t\"scan_1mbit_phy\":\tfalse,\n"
               "\t\"scan_extended_payload\":\ttrue,\n"
               "\t\"scan_channel_37\":\ttrue,\n"
               "\t\"scan_channel_38\":\ttrue,\n"
               "\t\"scan_channel_39\":\ttrue,\n"
               "\t\"scan_filter_allow_listed\":\tfalse,\n"
               "\t\"scan_filter_list\":\t[],\n"
               "\t\"coordinates\":\t\"\",\n"
               "\t\"fw_update_url\":\t\"https://network.ruuvi.com/firmwareupdate\"\n"
               "}"),
        string(json_str.p_str));
    ASSERT_TRUE(esp_log_wrapper_is_empty());

    gw_cfg_t gw_cfg2 = get_gateway_config_default();
    ASSERT_TRUE(gw_cfg_json_parse("my.json", nullptr, json_str.p_str, &gw_cfg2));
    cjson_wrap_free_json_str(&json_str);

    ASSERT_TRUE(0 == memcmp(&gw_cfg, &gw_cfg2, sizeof(gw_cfg)));
}

TEST_F(TestGwCfgJson, gw_cfg_json_generate_scan_extended_payload_false) // NOLINT
{
    gw_cfg_t         gw_cfg   = get_gateway_config_default();
    cjson_wrap_str_t json_str = cjson_wrap_str_null();

    gw_cfg.ruuvi_cfg.scan.scan_coded_phy                  = false;
    gw_cfg.ruuvi_cfg.scan.scan_1mbit_phy                  = true;
    gw_cfg.ruuvi_cfg.scan.scan_extended_payload           = false;
    gw_cfg.ruuvi_cfg.scan.scan_channel_37                 = true;
    gw_cfg.ruuvi_cfg.scan.scan_channel_38                 = true;
    gw_cfg.ruuvi_cfg.scan.scan_channel_39                 = true;
    gw_cfg.ruuvi_cfg.scan_filter.scan_filter_allow_listed = false;
    gw_cfg.ruuvi_cfg.scan_filter.scan_filter_length       = 0;

    ASSERT_TRUE(gw_cfg_json_generate_for_saving(&gw_cfg, &json_str));
    ASSERT_NE(nullptr, json_str.p_str);
    ASSERT_EQ(
        string("{\n"
               "\t\"wifi_sta_config\":\t{\n"
               "\t\t\"ssid\":\t\"\",\n"
               "\t\t\"password\":\t\"\"\n"
               "\t},\n"
               "\t\"wifi_ap_config\":\t{\n"
               "\t\t\"password\":\t\"\",\n"
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
               "\t\"http_stat_pass\":\t\"\",\n"
               "\t\"http_stat_use_ssl_client_cert\":\tfalse,\n"
               "\t\"http_stat_use_ssl_server_cert\":\tfalse,\n"
               "\t\"use_mqtt\":\tfalse,\n"
               "\t\"mqtt_disable_retained_messages\":\tfalse,\n"
               "\t\"mqtt_transport\":\t\"TCP\",\n"
               "\t\"mqtt_server\":\t\"test.mosquitto.org\",\n"
               "\t\"mqtt_port\":\t1883,\n"
               "\t\"mqtt_sending_interval\":\t0,\n"
               "\t\"mqtt_prefix\":\t\"ruuvi/AA:BB:CC:DD:EE:FF/\",\n"
               "\t\"mqtt_client_id\":\t\"AA:BB:CC:DD:EE:FF\",\n"
               "\t\"mqtt_user\":\t\"\",\n"
               "\t\"mqtt_pass\":\t\"\",\n"
               "\t\"mqtt_use_ssl_client_cert\":\tfalse,\n"
               "\t\"mqtt_use_ssl_server_cert\":\tfalse,\n"
               "\t\"lan_auth_type\":\t\"lan_auth_default\",\n"
               "\t\"lan_auth_user\":\t\"Admin\",\n"
               "\t\"lan_auth_api_key\":\t\"\",\n"
               "\t\"lan_auth_api_key_rw\":\t\"\",\n"
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
               "\t\"scan_extended_payload\":\tfalse,\n"
               "\t\"scan_channel_37\":\ttrue,\n"
               "\t\"scan_channel_38\":\ttrue,\n"
               "\t\"scan_channel_39\":\ttrue,\n"
               "\t\"scan_filter_allow_listed\":\tfalse,\n"
               "\t\"scan_filter_list\":\t[],\n"
               "\t\"coordinates\":\t\"\",\n"
               "\t\"fw_update_url\":\t\"https://network.ruuvi.com/firmwareupdate\"\n"
               "}"),
        string(json_str.p_str));
    ASSERT_TRUE(esp_log_wrapper_is_empty());

    gw_cfg_t gw_cfg2 = get_gateway_config_default();
    ASSERT_TRUE(gw_cfg_json_parse("my.json", nullptr, json_str.p_str, &gw_cfg2));
    cjson_wrap_free_json_str(&json_str);

    ASSERT_TRUE(0 == memcmp(&gw_cfg, &gw_cfg2, sizeof(gw_cfg)));
}

TEST_F(TestGwCfgJson, gw_cfg_json_generate_scan_channel_37_false) // NOLINT
{
    gw_cfg_t         gw_cfg   = get_gateway_config_default();
    cjson_wrap_str_t json_str = cjson_wrap_str_null();

    gw_cfg.ruuvi_cfg.scan.scan_coded_phy                  = false;
    gw_cfg.ruuvi_cfg.scan.scan_1mbit_phy                  = true;
    gw_cfg.ruuvi_cfg.scan.scan_extended_payload           = true;
    gw_cfg.ruuvi_cfg.scan.scan_channel_37                 = false;
    gw_cfg.ruuvi_cfg.scan.scan_channel_38                 = true;
    gw_cfg.ruuvi_cfg.scan.scan_channel_39                 = true;
    gw_cfg.ruuvi_cfg.scan_filter.scan_filter_allow_listed = false;
    gw_cfg.ruuvi_cfg.scan_filter.scan_filter_length       = 0;

    ASSERT_TRUE(gw_cfg_json_generate_for_saving(&gw_cfg, &json_str));
    ASSERT_NE(nullptr, json_str.p_str);
    ASSERT_EQ(
        string("{\n"
               "\t\"wifi_sta_config\":\t{\n"
               "\t\t\"ssid\":\t\"\",\n"
               "\t\t\"password\":\t\"\"\n"
               "\t},\n"
               "\t\"wifi_ap_config\":\t{\n"
               "\t\t\"password\":\t\"\",\n"
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
               "\t\"http_stat_pass\":\t\"\",\n"
               "\t\"http_stat_use_ssl_client_cert\":\tfalse,\n"
               "\t\"http_stat_use_ssl_server_cert\":\tfalse,\n"
               "\t\"use_mqtt\":\tfalse,\n"
               "\t\"mqtt_disable_retained_messages\":\tfalse,\n"
               "\t\"mqtt_transport\":\t\"TCP\",\n"
               "\t\"mqtt_server\":\t\"test.mosquitto.org\",\n"
               "\t\"mqtt_port\":\t1883,\n"
               "\t\"mqtt_sending_interval\":\t0,\n"
               "\t\"mqtt_prefix\":\t\"ruuvi/AA:BB:CC:DD:EE:FF/\",\n"
               "\t\"mqtt_client_id\":\t\"AA:BB:CC:DD:EE:FF\",\n"
               "\t\"mqtt_user\":\t\"\",\n"
               "\t\"mqtt_pass\":\t\"\",\n"
               "\t\"mqtt_use_ssl_client_cert\":\tfalse,\n"
               "\t\"mqtt_use_ssl_server_cert\":\tfalse,\n"
               "\t\"lan_auth_type\":\t\"lan_auth_default\",\n"
               "\t\"lan_auth_user\":\t\"Admin\",\n"
               "\t\"lan_auth_api_key\":\t\"\",\n"
               "\t\"lan_auth_api_key_rw\":\t\"\",\n"
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
               "\t\"scan_channel_37\":\tfalse,\n"
               "\t\"scan_channel_38\":\ttrue,\n"
               "\t\"scan_channel_39\":\ttrue,\n"
               "\t\"scan_filter_allow_listed\":\tfalse,\n"
               "\t\"scan_filter_list\":\t[],\n"
               "\t\"coordinates\":\t\"\",\n"
               "\t\"fw_update_url\":\t\"https://network.ruuvi.com/firmwareupdate\"\n"
               "}"),
        string(json_str.p_str));
    ASSERT_TRUE(esp_log_wrapper_is_empty());

    gw_cfg_t gw_cfg2 = get_gateway_config_default();
    ASSERT_TRUE(gw_cfg_json_parse("my.json", nullptr, json_str.p_str, &gw_cfg2));
    cjson_wrap_free_json_str(&json_str);

    ASSERT_TRUE(0 == memcmp(&gw_cfg, &gw_cfg2, sizeof(gw_cfg)));
}

TEST_F(TestGwCfgJson, gw_cfg_json_generate_scan_channel_38_false) // NOLINT
{
    gw_cfg_t         gw_cfg   = get_gateway_config_default();
    cjson_wrap_str_t json_str = cjson_wrap_str_null();

    gw_cfg.ruuvi_cfg.scan.scan_coded_phy                  = false;
    gw_cfg.ruuvi_cfg.scan.scan_1mbit_phy                  = true;
    gw_cfg.ruuvi_cfg.scan.scan_extended_payload           = true;
    gw_cfg.ruuvi_cfg.scan.scan_channel_37                 = true;
    gw_cfg.ruuvi_cfg.scan.scan_channel_38                 = false;
    gw_cfg.ruuvi_cfg.scan.scan_channel_39                 = true;
    gw_cfg.ruuvi_cfg.scan_filter.scan_filter_allow_listed = false;
    gw_cfg.ruuvi_cfg.scan_filter.scan_filter_length       = 0;

    ASSERT_TRUE(gw_cfg_json_generate_for_saving(&gw_cfg, &json_str));
    ASSERT_NE(nullptr, json_str.p_str);
    ASSERT_EQ(
        string("{\n"
               "\t\"wifi_sta_config\":\t{\n"
               "\t\t\"ssid\":\t\"\",\n"
               "\t\t\"password\":\t\"\"\n"
               "\t},\n"
               "\t\"wifi_ap_config\":\t{\n"
               "\t\t\"password\":\t\"\",\n"
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
               "\t\"http_stat_pass\":\t\"\",\n"
               "\t\"http_stat_use_ssl_client_cert\":\tfalse,\n"
               "\t\"http_stat_use_ssl_server_cert\":\tfalse,\n"
               "\t\"use_mqtt\":\tfalse,\n"
               "\t\"mqtt_disable_retained_messages\":\tfalse,\n"
               "\t\"mqtt_transport\":\t\"TCP\",\n"
               "\t\"mqtt_server\":\t\"test.mosquitto.org\",\n"
               "\t\"mqtt_port\":\t1883,\n"
               "\t\"mqtt_sending_interval\":\t0,\n"
               "\t\"mqtt_prefix\":\t\"ruuvi/AA:BB:CC:DD:EE:FF/\",\n"
               "\t\"mqtt_client_id\":\t\"AA:BB:CC:DD:EE:FF\",\n"
               "\t\"mqtt_user\":\t\"\",\n"
               "\t\"mqtt_pass\":\t\"\",\n"
               "\t\"mqtt_use_ssl_client_cert\":\tfalse,\n"
               "\t\"mqtt_use_ssl_server_cert\":\tfalse,\n"
               "\t\"lan_auth_type\":\t\"lan_auth_default\",\n"
               "\t\"lan_auth_user\":\t\"Admin\",\n"
               "\t\"lan_auth_api_key\":\t\"\",\n"
               "\t\"lan_auth_api_key_rw\":\t\"\",\n"
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
               "\t\"scan_channel_38\":\tfalse,\n"
               "\t\"scan_channel_39\":\ttrue,\n"
               "\t\"scan_filter_allow_listed\":\tfalse,\n"
               "\t\"scan_filter_list\":\t[],\n"
               "\t\"coordinates\":\t\"\",\n"
               "\t\"fw_update_url\":\t\"https://network.ruuvi.com/firmwareupdate\"\n"
               "}"),
        string(json_str.p_str));
    ASSERT_TRUE(esp_log_wrapper_is_empty());

    gw_cfg_t gw_cfg2 = get_gateway_config_default();
    ASSERT_TRUE(gw_cfg_json_parse("my.json", nullptr, json_str.p_str, &gw_cfg2));
    cjson_wrap_free_json_str(&json_str);

    ASSERT_TRUE(0 == memcmp(&gw_cfg, &gw_cfg2, sizeof(gw_cfg)));
}

TEST_F(TestGwCfgJson, gw_cfg_json_generate_scan_channel_39_false) // NOLINT
{
    gw_cfg_t         gw_cfg   = get_gateway_config_default();
    cjson_wrap_str_t json_str = cjson_wrap_str_null();

    gw_cfg.ruuvi_cfg.scan.scan_coded_phy                  = false;
    gw_cfg.ruuvi_cfg.scan.scan_1mbit_phy                  = true;
    gw_cfg.ruuvi_cfg.scan.scan_extended_payload           = true;
    gw_cfg.ruuvi_cfg.scan.scan_channel_37                 = true;
    gw_cfg.ruuvi_cfg.scan.scan_channel_38                 = true;
    gw_cfg.ruuvi_cfg.scan.scan_channel_39                 = false;
    gw_cfg.ruuvi_cfg.scan_filter.scan_filter_allow_listed = false;
    gw_cfg.ruuvi_cfg.scan_filter.scan_filter_length       = 0;

    ASSERT_TRUE(gw_cfg_json_generate_for_saving(&gw_cfg, &json_str));
    ASSERT_NE(nullptr, json_str.p_str);
    ASSERT_EQ(
        string("{\n"
               "\t\"wifi_sta_config\":\t{\n"
               "\t\t\"ssid\":\t\"\",\n"
               "\t\t\"password\":\t\"\"\n"
               "\t},\n"
               "\t\"wifi_ap_config\":\t{\n"
               "\t\t\"password\":\t\"\",\n"
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
               "\t\"http_stat_pass\":\t\"\",\n"
               "\t\"http_stat_use_ssl_client_cert\":\tfalse,\n"
               "\t\"http_stat_use_ssl_server_cert\":\tfalse,\n"
               "\t\"use_mqtt\":\tfalse,\n"
               "\t\"mqtt_disable_retained_messages\":\tfalse,\n"
               "\t\"mqtt_transport\":\t\"TCP\",\n"
               "\t\"mqtt_server\":\t\"test.mosquitto.org\",\n"
               "\t\"mqtt_port\":\t1883,\n"
               "\t\"mqtt_sending_interval\":\t0,\n"
               "\t\"mqtt_prefix\":\t\"ruuvi/AA:BB:CC:DD:EE:FF/\",\n"
               "\t\"mqtt_client_id\":\t\"AA:BB:CC:DD:EE:FF\",\n"
               "\t\"mqtt_user\":\t\"\",\n"
               "\t\"mqtt_pass\":\t\"\",\n"
               "\t\"mqtt_use_ssl_client_cert\":\tfalse,\n"
               "\t\"mqtt_use_ssl_server_cert\":\tfalse,\n"
               "\t\"lan_auth_type\":\t\"lan_auth_default\",\n"
               "\t\"lan_auth_user\":\t\"Admin\",\n"
               "\t\"lan_auth_api_key\":\t\"\",\n"
               "\t\"lan_auth_api_key_rw\":\t\"\",\n"
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
               "\t\"scan_channel_39\":\tfalse,\n"
               "\t\"scan_filter_allow_listed\":\tfalse,\n"
               "\t\"scan_filter_list\":\t[],\n"
               "\t\"coordinates\":\t\"\",\n"
               "\t\"fw_update_url\":\t\"https://network.ruuvi.com/firmwareupdate\"\n"
               "}"),
        string(json_str.p_str));
    ASSERT_TRUE(esp_log_wrapper_is_empty());

    gw_cfg_t gw_cfg2 = get_gateway_config_default();
    ASSERT_TRUE(gw_cfg_json_parse("my.json", nullptr, json_str.p_str, &gw_cfg2));
    cjson_wrap_free_json_str(&json_str);

    ASSERT_TRUE(0 == memcmp(&gw_cfg, &gw_cfg2, sizeof(gw_cfg)));
}

TEST_F(TestGwCfgJson, gw_cfg_json_generate_scan_filter1_not_allow_listed) // NOLINT
{
    gw_cfg_t         gw_cfg   = get_gateway_config_default();
    cjson_wrap_str_t json_str = cjson_wrap_str_null();

    gw_cfg.ruuvi_cfg.scan.scan_coded_phy                  = false;
    gw_cfg.ruuvi_cfg.scan.scan_1mbit_phy                  = true;
    gw_cfg.ruuvi_cfg.scan.scan_extended_payload           = true;
    gw_cfg.ruuvi_cfg.scan.scan_channel_37                 = true;
    gw_cfg.ruuvi_cfg.scan.scan_channel_38                 = true;
    gw_cfg.ruuvi_cfg.scan.scan_channel_39                 = false;
    gw_cfg.ruuvi_cfg.scan_filter.scan_filter_allow_listed = false;
    gw_cfg.ruuvi_cfg.scan_filter.scan_filter_length       = 1;
    mac_addr_from_str("AA:BB:CC:DD:EE:FF", &gw_cfg.ruuvi_cfg.scan_filter.scan_filter_list[0]);

    ASSERT_TRUE(gw_cfg_json_generate_for_saving(&gw_cfg, &json_str));
    ASSERT_NE(nullptr, json_str.p_str);
    ASSERT_EQ(
        string("{\n"
               "\t\"wifi_sta_config\":\t{\n"
               "\t\t\"ssid\":\t\"\",\n"
               "\t\t\"password\":\t\"\"\n"
               "\t},\n"
               "\t\"wifi_ap_config\":\t{\n"
               "\t\t\"password\":\t\"\",\n"
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
               "\t\"http_stat_pass\":\t\"\",\n"
               "\t\"http_stat_use_ssl_client_cert\":\tfalse,\n"
               "\t\"http_stat_use_ssl_server_cert\":\tfalse,\n"
               "\t\"use_mqtt\":\tfalse,\n"
               "\t\"mqtt_disable_retained_messages\":\tfalse,\n"
               "\t\"mqtt_transport\":\t\"TCP\",\n"
               "\t\"mqtt_server\":\t\"test.mosquitto.org\",\n"
               "\t\"mqtt_port\":\t1883,\n"
               "\t\"mqtt_sending_interval\":\t0,\n"
               "\t\"mqtt_prefix\":\t\"ruuvi/AA:BB:CC:DD:EE:FF/\",\n"
               "\t\"mqtt_client_id\":\t\"AA:BB:CC:DD:EE:FF\",\n"
               "\t\"mqtt_user\":\t\"\",\n"
               "\t\"mqtt_pass\":\t\"\",\n"
               "\t\"mqtt_use_ssl_client_cert\":\tfalse,\n"
               "\t\"mqtt_use_ssl_server_cert\":\tfalse,\n"
               "\t\"lan_auth_type\":\t\"lan_auth_default\",\n"
               "\t\"lan_auth_user\":\t\"Admin\",\n"
               "\t\"lan_auth_api_key\":\t\"\",\n"
               "\t\"lan_auth_api_key_rw\":\t\"\",\n"
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
               "\t\"scan_channel_39\":\tfalse,\n"
               "\t\"scan_filter_allow_listed\":\tfalse,\n"
               "\t\"scan_filter_list\":\t[\"AA:BB:CC:DD:EE:FF\"],\n"
               "\t\"coordinates\":\t\"\",\n"
               "\t\"fw_update_url\":\t\"https://network.ruuvi.com/firmwareupdate\"\n"
               "}"),
        string(json_str.p_str));
    ASSERT_TRUE(esp_log_wrapper_is_empty());

    gw_cfg_t gw_cfg2 = get_gateway_config_default();
    ASSERT_TRUE(gw_cfg_json_parse("my.json", nullptr, json_str.p_str, &gw_cfg2));
    cjson_wrap_free_json_str(&json_str);

    ASSERT_TRUE(0 == memcmp(&gw_cfg, &gw_cfg2, sizeof(gw_cfg)));
}

TEST_F(TestGwCfgJson, gw_cfg_json_generate_scan_filter2_allow_listed) // NOLINT
{
    gw_cfg_t         gw_cfg   = get_gateway_config_default();
    cjson_wrap_str_t json_str = cjson_wrap_str_null();

    gw_cfg.ruuvi_cfg.scan.scan_coded_phy                  = false;
    gw_cfg.ruuvi_cfg.scan.scan_1mbit_phy                  = true;
    gw_cfg.ruuvi_cfg.scan.scan_extended_payload           = true;
    gw_cfg.ruuvi_cfg.scan.scan_channel_37                 = true;
    gw_cfg.ruuvi_cfg.scan.scan_channel_38                 = true;
    gw_cfg.ruuvi_cfg.scan.scan_channel_39                 = false;
    gw_cfg.ruuvi_cfg.scan_filter.scan_filter_allow_listed = true;
    gw_cfg.ruuvi_cfg.scan_filter.scan_filter_length       = 2;
    mac_addr_from_str("A0:B0:C0:D0:E0:F0", &gw_cfg.ruuvi_cfg.scan_filter.scan_filter_list[0]);
    mac_addr_from_str("A1:B1:C1:D1:E1:F1", &gw_cfg.ruuvi_cfg.scan_filter.scan_filter_list[1]);

    ASSERT_TRUE(gw_cfg_json_generate_for_saving(&gw_cfg, &json_str));
    ASSERT_NE(nullptr, json_str.p_str);
    ASSERT_EQ(
        string("{\n"
               "\t\"wifi_sta_config\":\t{\n"
               "\t\t\"ssid\":\t\"\",\n"
               "\t\t\"password\":\t\"\"\n"
               "\t},\n"
               "\t\"wifi_ap_config\":\t{\n"
               "\t\t\"password\":\t\"\",\n"
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
               "\t\"http_stat_pass\":\t\"\",\n"
               "\t\"http_stat_use_ssl_client_cert\":\tfalse,\n"
               "\t\"http_stat_use_ssl_server_cert\":\tfalse,\n"
               "\t\"use_mqtt\":\tfalse,\n"
               "\t\"mqtt_disable_retained_messages\":\tfalse,\n"
               "\t\"mqtt_transport\":\t\"TCP\",\n"
               "\t\"mqtt_server\":\t\"test.mosquitto.org\",\n"
               "\t\"mqtt_port\":\t1883,\n"
               "\t\"mqtt_sending_interval\":\t0,\n"
               "\t\"mqtt_prefix\":\t\"ruuvi/AA:BB:CC:DD:EE:FF/\",\n"
               "\t\"mqtt_client_id\":\t\"AA:BB:CC:DD:EE:FF\",\n"
               "\t\"mqtt_user\":\t\"\",\n"
               "\t\"mqtt_pass\":\t\"\",\n"
               "\t\"mqtt_use_ssl_client_cert\":\tfalse,\n"
               "\t\"mqtt_use_ssl_server_cert\":\tfalse,\n"
               "\t\"lan_auth_type\":\t\"lan_auth_default\",\n"
               "\t\"lan_auth_user\":\t\"Admin\",\n"
               "\t\"lan_auth_api_key\":\t\"\",\n"
               "\t\"lan_auth_api_key_rw\":\t\"\",\n"
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
               "\t\"scan_channel_39\":\tfalse,\n"
               "\t\"scan_filter_allow_listed\":\ttrue,\n"
               "\t\"scan_filter_list\":\t[\"A0:B0:C0:D0:E0:F0\", \"A1:B1:C1:D1:E1:F1\"],\n"
               "\t\"coordinates\":\t\"\",\n"
               "\t\"fw_update_url\":\t\"https://network.ruuvi.com/firmwareupdate\"\n"
               "}"),
        string(json_str.p_str));
    ASSERT_TRUE(esp_log_wrapper_is_empty());

    gw_cfg_t gw_cfg2 = get_gateway_config_default();
    ASSERT_TRUE(gw_cfg_json_parse("my.json", nullptr, json_str.p_str, &gw_cfg2));
    cjson_wrap_free_json_str(&json_str);

    ASSERT_TRUE(0 == memcmp(&gw_cfg, &gw_cfg2, sizeof(gw_cfg)));
}

TEST_F(TestGwCfgJson, gw_cfg_json_generate_coordinates) // NOLINT
{
    gw_cfg_t         gw_cfg   = get_gateway_config_default();
    cjson_wrap_str_t json_str = cjson_wrap_str_null();

    snprintf(gw_cfg.ruuvi_cfg.coordinates.buf, sizeof(gw_cfg.ruuvi_cfg.coordinates.buf), "123,456");

    ASSERT_TRUE(gw_cfg_json_generate_for_saving(&gw_cfg, &json_str));
    ASSERT_NE(nullptr, json_str.p_str);
    ASSERT_EQ(
        string("{\n"
               "\t\"wifi_sta_config\":\t{\n"
               "\t\t\"ssid\":\t\"\",\n"
               "\t\t\"password\":\t\"\"\n"
               "\t},\n"
               "\t\"wifi_ap_config\":\t{\n"
               "\t\t\"password\":\t\"\",\n"
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
               "\t\"http_stat_pass\":\t\"\",\n"
               "\t\"http_stat_use_ssl_client_cert\":\tfalse,\n"
               "\t\"http_stat_use_ssl_server_cert\":\tfalse,\n"
               "\t\"use_mqtt\":\tfalse,\n"
               "\t\"mqtt_disable_retained_messages\":\tfalse,\n"
               "\t\"mqtt_transport\":\t\"TCP\",\n"
               "\t\"mqtt_server\":\t\"test.mosquitto.org\",\n"
               "\t\"mqtt_port\":\t1883,\n"
               "\t\"mqtt_sending_interval\":\t0,\n"
               "\t\"mqtt_prefix\":\t\"ruuvi/AA:BB:CC:DD:EE:FF/\",\n"
               "\t\"mqtt_client_id\":\t\"AA:BB:CC:DD:EE:FF\",\n"
               "\t\"mqtt_user\":\t\"\",\n"
               "\t\"mqtt_pass\":\t\"\",\n"
               "\t\"mqtt_use_ssl_client_cert\":\tfalse,\n"
               "\t\"mqtt_use_ssl_server_cert\":\tfalse,\n"
               "\t\"lan_auth_type\":\t\"lan_auth_default\",\n"
               "\t\"lan_auth_user\":\t\"Admin\",\n"
               "\t\"lan_auth_api_key\":\t\"\",\n"
               "\t\"lan_auth_api_key_rw\":\t\"\",\n"
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
               "\t\"coordinates\":\t\"123,456\",\n"
               "\t\"fw_update_url\":\t\"https://network.ruuvi.com/firmwareupdate\"\n"
               "}"),
        string(json_str.p_str));
    ASSERT_TRUE(esp_log_wrapper_is_empty());

    gw_cfg_t gw_cfg2 = get_gateway_config_default();
    ASSERT_TRUE(gw_cfg_json_parse("my.json", nullptr, json_str.p_str, &gw_cfg2));
    cjson_wrap_free_json_str(&json_str);

    ASSERT_TRUE(0 == memcmp(&gw_cfg, &gw_cfg2, sizeof(gw_cfg)));
}

TEST_F(TestGwCfgJson, gw_cfg_json_generate_parse_generate_default) // NOLINT
{
    const gw_cfg_t   gw_cfg   = get_gateway_config_default();
    cjson_wrap_str_t json_str = cjson_wrap_str_null();

    ASSERT_TRUE(gw_cfg_json_generate_for_saving(&gw_cfg, &json_str));
    ASSERT_NE(nullptr, json_str.p_str);
    gw_cfg_t gw_cfg2 = get_gateway_config_default();
    ASSERT_TRUE(gw_cfg_json_parse("my.json", nullptr, json_str.p_str, &gw_cfg2));
    cjson_wrap_free_json_str(&json_str);

    ASSERT_EQ(true, gw_cfg2.eth_cfg.use_eth);
    ASSERT_EQ(true, gw_cfg2.eth_cfg.eth_dhcp);
    ASSERT_EQ(string(""), gw_cfg2.eth_cfg.eth_static_ip.buf);
    ASSERT_EQ(string(""), gw_cfg2.eth_cfg.eth_netmask.buf);
    ASSERT_EQ(string(""), gw_cfg2.eth_cfg.eth_gw.buf);
    ASSERT_EQ(string(""), gw_cfg2.eth_cfg.eth_dns1.buf);
    ASSERT_EQ(string(""), gw_cfg2.eth_cfg.eth_dns2.buf);

    ASSERT_EQ(false, gw_cfg2.ruuvi_cfg.mqtt.use_mqtt);
    ASSERT_EQ(false, gw_cfg2.ruuvi_cfg.mqtt.mqtt_disable_retained_messages);
    ASSERT_EQ(string("TCP"), gw_cfg2.ruuvi_cfg.mqtt.mqtt_transport.buf);
    ASSERT_EQ(string("test.mosquitto.org"), gw_cfg2.ruuvi_cfg.mqtt.mqtt_server.buf);
    ASSERT_EQ(1883, gw_cfg2.ruuvi_cfg.mqtt.mqtt_port);
    ASSERT_EQ(string("ruuvi/AA:BB:CC:DD:EE:FF/"), gw_cfg2.ruuvi_cfg.mqtt.mqtt_prefix.buf);
    ASSERT_EQ(string("AA:BB:CC:DD:EE:FF"), gw_cfg2.ruuvi_cfg.mqtt.mqtt_client_id.buf);
    ASSERT_EQ(string(""), gw_cfg2.ruuvi_cfg.mqtt.mqtt_user.buf);
    ASSERT_EQ(string(""), gw_cfg2.ruuvi_cfg.mqtt.mqtt_pass.buf);

    ASSERT_EQ(true, gw_cfg2.ruuvi_cfg.http.use_http_ruuvi);
    ASSERT_EQ(false, gw_cfg2.ruuvi_cfg.http.use_http);
    ASSERT_EQ(string("https://network.ruuvi.com/record"), gw_cfg2.ruuvi_cfg.http.http_url.buf);
    ASSERT_EQ(GW_CFG_HTTP_DATA_FORMAT_RUUVI, gw_cfg2.ruuvi_cfg.http.data_format);
    ASSERT_EQ(GW_CFG_HTTP_AUTH_TYPE_NONE, gw_cfg2.ruuvi_cfg.http.auth_type);

    ASSERT_EQ(true, gw_cfg2.ruuvi_cfg.http_stat.use_http_stat);
    ASSERT_EQ(string(RUUVI_GATEWAY_HTTP_STATUS_URL), gw_cfg2.ruuvi_cfg.http_stat.http_stat_url.buf);
    ASSERT_EQ(string(""), gw_cfg2.ruuvi_cfg.http_stat.http_stat_user.buf);
    ASSERT_EQ(string(""), gw_cfg2.ruuvi_cfg.http_stat.http_stat_pass.buf);

    ASSERT_EQ(HTTP_SERVER_AUTH_TYPE_DEFAULT, gw_cfg2.ruuvi_cfg.lan_auth.lan_auth_type);
    ASSERT_EQ(string(RUUVI_GATEWAY_AUTH_DEFAULT_USER), gw_cfg2.ruuvi_cfg.lan_auth.lan_auth_user.buf);
    ASSERT_EQ(string("0d6c6f1c27ca628806eb9247740d8ba1"), gw_cfg2.ruuvi_cfg.lan_auth.lan_auth_pass.buf);
    ASSERT_EQ(string(""), gw_cfg2.ruuvi_cfg.lan_auth.lan_auth_api_key.buf);
    ASSERT_EQ(string(""), gw_cfg2.ruuvi_cfg.lan_auth.lan_auth_api_key_rw.buf);

    ASSERT_EQ(AUTO_UPDATE_CYCLE_TYPE_REGULAR, gw_cfg2.ruuvi_cfg.auto_update.auto_update_cycle);
    ASSERT_EQ(0x7F, gw_cfg2.ruuvi_cfg.auto_update.auto_update_weekdays_bitmask);
    ASSERT_EQ(0, gw_cfg2.ruuvi_cfg.auto_update.auto_update_interval_from);
    ASSERT_EQ(24, gw_cfg2.ruuvi_cfg.auto_update.auto_update_interval_to);
    ASSERT_EQ(3, gw_cfg2.ruuvi_cfg.auto_update.auto_update_tz_offset_hours);

    ASSERT_EQ(true, gw_cfg2.ruuvi_cfg.ntp.ntp_use);
    ASSERT_EQ(false, gw_cfg2.ruuvi_cfg.ntp.ntp_use_dhcp);
    ASSERT_EQ(string("time.google.com"), string(gw_cfg2.ruuvi_cfg.ntp.ntp_server1.buf));
    ASSERT_EQ(string("time.cloudflare.com"), string(gw_cfg2.ruuvi_cfg.ntp.ntp_server2.buf));
    ASSERT_EQ(string("time.nist.gov"), string(gw_cfg2.ruuvi_cfg.ntp.ntp_server3.buf));
    ASSERT_EQ(string("pool.ntp.org"), string(gw_cfg2.ruuvi_cfg.ntp.ntp_server4.buf));

    ASSERT_EQ(RUUVI_COMPANY_ID, gw_cfg2.ruuvi_cfg.filter.company_id);
    ASSERT_EQ(true, gw_cfg2.ruuvi_cfg.filter.company_use_filtering);

    ASSERT_EQ(false, gw_cfg2.ruuvi_cfg.scan.scan_coded_phy);
    ASSERT_EQ(true, gw_cfg2.ruuvi_cfg.scan.scan_1mbit_phy);
    ASSERT_EQ(true, gw_cfg2.ruuvi_cfg.scan.scan_extended_payload);
    ASSERT_EQ(true, gw_cfg2.ruuvi_cfg.scan.scan_channel_37);
    ASSERT_EQ(true, gw_cfg2.ruuvi_cfg.scan.scan_channel_38);
    ASSERT_EQ(true, gw_cfg2.ruuvi_cfg.scan.scan_channel_39);
    ASSERT_EQ(false, gw_cfg2.ruuvi_cfg.scan_filter.scan_filter_allow_listed);
    ASSERT_EQ(0, gw_cfg2.ruuvi_cfg.scan_filter.scan_filter_length);

    ASSERT_EQ(string(""), gw_cfg2.ruuvi_cfg.coordinates.buf);

    ASSERT_TRUE(0 == memcmp(&gw_cfg, &gw_cfg2, sizeof(gw_cfg)));

    ASSERT_TRUE(gw_cfg_json_generate_for_saving(&gw_cfg2, &json_str));
    ASSERT_EQ(
        string("{\n"
               "\t\"wifi_sta_config\":\t{\n"
               "\t\t\"ssid\":\t\"\",\n"
               "\t\t\"password\":\t\"\"\n"
               "\t},\n"
               "\t\"wifi_ap_config\":\t{\n"
               "\t\t\"password\":\t\"\",\n"
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
               "\t\"http_stat_pass\":\t\"\",\n"
               "\t\"http_stat_use_ssl_client_cert\":\tfalse,\n"
               "\t\"http_stat_use_ssl_server_cert\":\tfalse,\n"
               "\t\"use_mqtt\":\tfalse,\n"
               "\t\"mqtt_disable_retained_messages\":\tfalse,\n"
               "\t\"mqtt_transport\":\t\"TCP\",\n"
               "\t\"mqtt_server\":\t\"test.mosquitto.org\",\n"
               "\t\"mqtt_port\":\t1883,\n"
               "\t\"mqtt_sending_interval\":\t0,\n"
               "\t\"mqtt_prefix\":\t\"ruuvi/AA:BB:CC:DD:EE:FF/\",\n"
               "\t\"mqtt_client_id\":\t\"AA:BB:CC:DD:EE:FF\",\n"
               "\t\"mqtt_user\":\t\"\",\n"
               "\t\"mqtt_pass\":\t\"\",\n"
               "\t\"mqtt_use_ssl_client_cert\":\tfalse,\n"
               "\t\"mqtt_use_ssl_server_cert\":\tfalse,\n"
               "\t\"lan_auth_type\":\t\"lan_auth_default\",\n"
               "\t\"lan_auth_user\":\t\"Admin\",\n"
               "\t\"lan_auth_api_key\":\t\"\",\n"
               "\t\"lan_auth_api_key_rw\":\t\"\",\n"
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
               "\t\"coordinates\":\t\"\",\n"
               "\t\"fw_update_url\":\t\"https://network.ruuvi.com/firmwareupdate\"\n"
               "}"),
        string(json_str.p_str));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    cjson_wrap_free_json_str(&json_str);
}

TEST_F(TestGwCfgJson, gw_cfg_json_generate_parse_generate_auto_update_regular) // NOLINT
{
    const gw_cfg_t gw_cfg   = {
        .device_info = {
            .esp32_fw_ver = { "v1.10.0" },
            .nrf52_fw_ver = { "v0.7.2" },
            .nrf52_mac_addr = { "AA:BB:CC:DD:EE:FF" },
            .nrf52_device_id = { "11:22:33:44:55:66:77:88" },
        },
        .ruuvi_cfg = {
            .http = {
                .use_http_ruuvi = false,
                .use_http = false,
                .http_use_ssl_client_cert = false,
                .http_use_ssl_server_cert = false,
                .http_url = { "https://myserver1.com" },
                .data_format = GW_CFG_HTTP_DATA_FORMAT_RUUVI,
                .auth_type = GW_CFG_HTTP_AUTH_TYPE_BASIC,
                .auth = {
                    .auth_basic = {
                        .user = { "h_user1" },
                        .password = { "h_pass1" },
                    },
                },
            },
            .http_stat = {
                .use_http_stat = false,
                .http_stat_use_ssl_client_cert = false,
                .http_stat_use_ssl_server_cert = false,
                .http_stat_url = { "https://myserver1.com/status" },
                .http_stat_user = { "h_user2" },
                .http_stat_pass = { "h_pass2" },
            },
            .mqtt = {
                .use_mqtt = true,
                .mqtt_disable_retained_messages = false,
                .use_ssl_client_cert = false,
                .use_ssl_server_cert = false,
                .mqtt_transport = { "SSL" },
                .mqtt_server = { "test.mosquitto.org" },
                .mqtt_port = 1338,
                .mqtt_prefix = { "my_prefix" },
                .mqtt_client_id = { "my_client" },
                .mqtt_user = { "m_user1" },
                .mqtt_pass = { "m_pass1" },
            },
            .lan_auth = {
                HTTP_SERVER_AUTH_TYPE_DIGEST,
                "l_user1",
                "l_pass1",
                "",
            },
            .auto_update = {
                .auto_update_cycle = AUTO_UPDATE_CYCLE_TYPE_REGULAR,
                .auto_update_weekdays_bitmask = 0x3F,
                .auto_update_interval_from = 2,
                .auto_update_interval_to = 22,
                .auto_update_tz_offset_hours = 5,
            },
            .ntp = {
                .ntp_use = true,
                .ntp_use_dhcp = false,
                .ntp_server1 = { "time.google.com", },
                .ntp_server2 = { "time.cloudflare.com", },
                .ntp_server3 = { "time.nist.gov", },
                .ntp_server4 = { "pool.ntp.org", },
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
                .scan_filter_list = { 0 },
            },
            .coordinates = { "coordinates1" },
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

    ASSERT_TRUE(gw_cfg_json_generate_for_saving(&gw_cfg, &json_str));
    ASSERT_NE(nullptr, json_str.p_str);

    gw_cfg_t gw_cfg2 = get_gateway_config_default();
    ASSERT_TRUE(gw_cfg_json_parse("my.json", nullptr, json_str.p_str, &gw_cfg2));
    cjson_wrap_free_json_str(&json_str);

    ASSERT_EQ(true, gw_cfg2.eth_cfg.use_eth);
    ASSERT_EQ(false, gw_cfg2.eth_cfg.eth_dhcp);
    ASSERT_EQ(string("192.168.1.10"), gw_cfg2.eth_cfg.eth_static_ip.buf);
    ASSERT_EQ(string("255.255.255.0"), gw_cfg2.eth_cfg.eth_netmask.buf);
    ASSERT_EQ(string("192.168.1.1"), gw_cfg2.eth_cfg.eth_gw.buf);
    ASSERT_EQ(string("8.8.8.8"), gw_cfg2.eth_cfg.eth_dns1.buf);
    ASSERT_EQ(string("4.4.4.4"), gw_cfg2.eth_cfg.eth_dns2.buf);

    ASSERT_EQ(true, gw_cfg2.ruuvi_cfg.mqtt.use_mqtt);
    ASSERT_EQ(false, gw_cfg2.ruuvi_cfg.mqtt.mqtt_disable_retained_messages);
    ASSERT_EQ(string("SSL"), gw_cfg2.ruuvi_cfg.mqtt.mqtt_transport.buf);
    ASSERT_EQ(string("test.mosquitto.org"), gw_cfg2.ruuvi_cfg.mqtt.mqtt_server.buf);
    ASSERT_EQ(1338, gw_cfg2.ruuvi_cfg.mqtt.mqtt_port);
    ASSERT_EQ(string("my_prefix"), gw_cfg2.ruuvi_cfg.mqtt.mqtt_prefix.buf);
    ASSERT_EQ(string("my_client"), gw_cfg2.ruuvi_cfg.mqtt.mqtt_client_id.buf);
    ASSERT_EQ(string("m_user1"), gw_cfg2.ruuvi_cfg.mqtt.mqtt_user.buf);
    ASSERT_EQ(string("m_pass1"), gw_cfg2.ruuvi_cfg.mqtt.mqtt_pass.buf);

    ASSERT_EQ(false, gw_cfg2.ruuvi_cfg.http.use_http_ruuvi);
    ASSERT_EQ(false, gw_cfg2.ruuvi_cfg.http.use_http);

    ASSERT_EQ(false, gw_cfg2.ruuvi_cfg.http_stat.use_http_stat);
    ASSERT_EQ(string("https://myserver1.com/status"), gw_cfg2.ruuvi_cfg.http_stat.http_stat_url.buf);
    ASSERT_EQ(string("h_user2"), gw_cfg2.ruuvi_cfg.http_stat.http_stat_user.buf);
    ASSERT_EQ(string("h_pass2"), gw_cfg2.ruuvi_cfg.http_stat.http_stat_pass.buf);

    ASSERT_EQ(HTTP_SERVER_AUTH_TYPE_DIGEST, gw_cfg2.ruuvi_cfg.lan_auth.lan_auth_type);
    ASSERT_EQ(string("l_user1"), gw_cfg2.ruuvi_cfg.lan_auth.lan_auth_user.buf);
    ASSERT_EQ(string("l_pass1"), gw_cfg2.ruuvi_cfg.lan_auth.lan_auth_pass.buf);
    ASSERT_EQ(string(""), gw_cfg2.ruuvi_cfg.lan_auth.lan_auth_api_key.buf);
    ASSERT_EQ(string(""), gw_cfg2.ruuvi_cfg.lan_auth.lan_auth_api_key_rw.buf);

    ASSERT_EQ(AUTO_UPDATE_CYCLE_TYPE_REGULAR, gw_cfg2.ruuvi_cfg.auto_update.auto_update_cycle);
    ASSERT_EQ(0x3F, gw_cfg2.ruuvi_cfg.auto_update.auto_update_weekdays_bitmask);
    ASSERT_EQ(2, gw_cfg2.ruuvi_cfg.auto_update.auto_update_interval_from);
    ASSERT_EQ(22, gw_cfg2.ruuvi_cfg.auto_update.auto_update_interval_to);
    ASSERT_EQ(5, gw_cfg2.ruuvi_cfg.auto_update.auto_update_tz_offset_hours);

    ASSERT_EQ(true, gw_cfg2.ruuvi_cfg.ntp.ntp_use);
    ASSERT_EQ(false, gw_cfg2.ruuvi_cfg.ntp.ntp_use_dhcp);
    ASSERT_EQ(string("time.google.com"), string(gw_cfg2.ruuvi_cfg.ntp.ntp_server1.buf));
    ASSERT_EQ(string("time.cloudflare.com"), string(gw_cfg2.ruuvi_cfg.ntp.ntp_server2.buf));
    ASSERT_EQ(string("time.nist.gov"), string(gw_cfg2.ruuvi_cfg.ntp.ntp_server3.buf));
    ASSERT_EQ(string("pool.ntp.org"), string(gw_cfg2.ruuvi_cfg.ntp.ntp_server4.buf));

    ASSERT_EQ(RUUVI_COMPANY_ID + 1, gw_cfg2.ruuvi_cfg.filter.company_id);
    ASSERT_EQ(false, gw_cfg2.ruuvi_cfg.filter.company_use_filtering);

    ASSERT_EQ(true, gw_cfg2.ruuvi_cfg.scan.scan_coded_phy);
    ASSERT_EQ(false, gw_cfg2.ruuvi_cfg.scan.scan_1mbit_phy);
    ASSERT_EQ(false, gw_cfg2.ruuvi_cfg.scan.scan_extended_payload);
    ASSERT_EQ(false, gw_cfg2.ruuvi_cfg.scan.scan_channel_37);
    ASSERT_EQ(false, gw_cfg2.ruuvi_cfg.scan.scan_channel_38);
    ASSERT_EQ(false, gw_cfg2.ruuvi_cfg.scan.scan_channel_39);
    ASSERT_EQ(false, gw_cfg2.ruuvi_cfg.scan_filter.scan_filter_allow_listed);
    ASSERT_EQ(0, gw_cfg2.ruuvi_cfg.scan_filter.scan_filter_length);

    ASSERT_EQ(string("coordinates1"), gw_cfg2.ruuvi_cfg.coordinates.buf);
    ASSERT_EQ(string("https://myserver1.com/fw_update_info.json"), gw_cfg2.ruuvi_cfg.fw_update.fw_update_url);

    ASSERT_TRUE(gw_cfg_json_generate_for_saving(&gw_cfg2, &json_str));
    ASSERT_EQ(
        string("{\n"
               "\t\"wifi_sta_config\":\t{\n"
               "\t\t\"ssid\":\t\"\",\n"
               "\t\t\"password\":\t\"\"\n"
               "\t},\n"
               "\t\"wifi_ap_config\":\t{\n"
               "\t\t\"password\":\t\"\",\n"
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
               "\t\"use_http\":\tfalse,\n"
               "\t\"use_http_stat\":\tfalse,\n"
               "\t\"http_stat_url\":\t\"https://myserver1.com/status\",\n"
               "\t\"http_stat_user\":\t\"h_user2\",\n"
               "\t\"http_stat_pass\":\t\"h_pass2\",\n"
               "\t\"http_stat_use_ssl_client_cert\":\tfalse,\n"
               "\t\"http_stat_use_ssl_server_cert\":\tfalse,\n"
               "\t\"use_mqtt\":\ttrue,\n"
               "\t\"mqtt_disable_retained_messages\":\tfalse,\n"
               "\t\"mqtt_transport\":\t\"SSL\",\n"
               "\t\"mqtt_server\":\t\"test.mosquitto.org\",\n"
               "\t\"mqtt_port\":\t1338,\n"
               "\t\"mqtt_sending_interval\":\t0,\n"
               "\t\"mqtt_prefix\":\t\"my_prefix\",\n"
               "\t\"mqtt_client_id\":\t\"my_client\",\n"
               "\t\"mqtt_user\":\t\"m_user1\",\n"
               "\t\"mqtt_pass\":\t\"m_pass1\",\n"
               "\t\"mqtt_use_ssl_client_cert\":\tfalse,\n"
               "\t\"mqtt_use_ssl_server_cert\":\tfalse,\n"
               "\t\"lan_auth_type\":\t\"lan_auth_digest\",\n"
               "\t\"lan_auth_user\":\t\"l_user1\",\n"
               "\t\"lan_auth_pass\":\t\"l_pass1\",\n"
               "\t\"lan_auth_api_key\":\t\"\",\n"
               "\t\"lan_auth_api_key_rw\":\t\"\",\n"
               "\t\"auto_update_cycle\":\t\"regular\",\n"
               "\t\"auto_update_weekdays_bitmask\":\t63,\n"
               "\t\"auto_update_interval_from\":\t2,\n"
               "\t\"auto_update_interval_to\":\t22,\n"
               "\t\"auto_update_tz_offset_hours\":\t5,\n"
               "\t\"ntp_use\":\ttrue,\n"
               "\t\"ntp_use_dhcp\":\tfalse,\n"
               "\t\"ntp_server1\":\t\"time.google.com\",\n"
               "\t\"ntp_server2\":\t\"time.cloudflare.com\",\n"
               "\t\"ntp_server3\":\t\"time.nist.gov\",\n"
               "\t\"ntp_server4\":\t\"pool.ntp.org\",\n"
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
               "\t\"coordinates\":\t\"coordinates1\",\n"
               "\t\"fw_update_url\":\t\"https://myserver1.com/fw_update_info.json\"\n"
               "}"),
        string(json_str.p_str));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    cjson_wrap_free_json_str(&json_str);
}

TEST_F(TestGwCfgJson, gw_cfg_json_generate_parse_generate_auto_update_beta_tester) // NOLINT
{
    const gw_cfg_t gw_cfg   = {
        .device_info = {
            .esp32_fw_ver = { "v1.10.0" },
            .nrf52_fw_ver = { "v0.7.2" },
            .nrf52_mac_addr = { "AA:BB:CC:DD:EE:FF" },
            .nrf52_device_id = { "11:22:33:44:55:66:77:88" },
        },
        .ruuvi_cfg = {
            .http = {
                .use_http_ruuvi = false,
                .use_http = false,
                .http_use_ssl_client_cert = false,
                .http_use_ssl_server_cert = false,
                .http_url = { "https://myserver1.com" },
                .data_format = GW_CFG_HTTP_DATA_FORMAT_RUUVI,
                .auth_type = GW_CFG_HTTP_AUTH_TYPE_BASIC,
                .auth = {
                    .auth_basic = {
                        .user = { "h_user1" },
                        .password = { "h_pass1" },
                    },
                },
            },
            .http_stat = {
                .use_http_stat = false,
                .http_stat_use_ssl_client_cert = false,
                .http_stat_use_ssl_server_cert = false,
                .http_stat_url = { "https://myserver1.com/status" },
                .http_stat_user = { "h_user2" },
                .http_stat_pass = { "h_pass2" },
            },
            .mqtt = {
                .use_mqtt = true,
                .mqtt_disable_retained_messages = true,
                .use_ssl_client_cert = false,
                .use_ssl_server_cert = false,
                .mqtt_transport = { "SSL" },
                .mqtt_server = { "test.mosquitto.org" },
                .mqtt_port = 1338,
                .mqtt_prefix = { "my_prefix" },
                .mqtt_client_id = { "my_client" },
                .mqtt_user = { "m_user1" },
                .mqtt_pass = { "m_pass1" },
            },
            .lan_auth = {
                HTTP_SERVER_AUTH_TYPE_DIGEST,
                "l_user1",
                "l_pass1",
                "",
            },
            .auto_update = {
                .auto_update_cycle = AUTO_UPDATE_CYCLE_TYPE_BETA_TESTER,
                .auto_update_weekdays_bitmask = 0x3F,
                .auto_update_interval_from = 2,
                .auto_update_interval_to = 22,
                .auto_update_tz_offset_hours = 5,
            },
            .ntp = {
                .ntp_use = true,
                .ntp_use_dhcp = false,
                .ntp_server1 = { "time.google.com", },
                .ntp_server2 = { "time.cloudflare.com", },
                .ntp_server3 = { "time.nist.gov", },
                .ntp_server4 = { "pool.ntp.org", },
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
                .scan_filter_list = { 0 },
            },
            .coordinates = { "coordinates1" },
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

    ASSERT_TRUE(gw_cfg_json_generate_for_saving(&gw_cfg, &json_str));
    ASSERT_NE(nullptr, json_str.p_str);

    gw_cfg_t gw_cfg2 = get_gateway_config_default();
    ASSERT_TRUE(gw_cfg_json_parse("my.json", nullptr, json_str.p_str, &gw_cfg2));
    cjson_wrap_free_json_str(&json_str);

    ASSERT_EQ(true, gw_cfg2.eth_cfg.use_eth);
    ASSERT_EQ(false, gw_cfg2.eth_cfg.eth_dhcp);
    ASSERT_EQ(string("192.168.1.10"), gw_cfg2.eth_cfg.eth_static_ip.buf);
    ASSERT_EQ(string("255.255.255.0"), gw_cfg2.eth_cfg.eth_netmask.buf);
    ASSERT_EQ(string("192.168.1.1"), gw_cfg2.eth_cfg.eth_gw.buf);
    ASSERT_EQ(string("8.8.8.8"), gw_cfg2.eth_cfg.eth_dns1.buf);
    ASSERT_EQ(string("4.4.4.4"), gw_cfg2.eth_cfg.eth_dns2.buf);

    ASSERT_EQ(true, gw_cfg2.ruuvi_cfg.mqtt.use_mqtt);
    ASSERT_EQ(true, gw_cfg2.ruuvi_cfg.mqtt.mqtt_disable_retained_messages);
    ASSERT_EQ(string("SSL"), gw_cfg2.ruuvi_cfg.mqtt.mqtt_transport.buf);
    ASSERT_EQ(string("test.mosquitto.org"), gw_cfg2.ruuvi_cfg.mqtt.mqtt_server.buf);
    ASSERT_EQ(1338, gw_cfg2.ruuvi_cfg.mqtt.mqtt_port);
    ASSERT_EQ(string("my_prefix"), gw_cfg2.ruuvi_cfg.mqtt.mqtt_prefix.buf);
    ASSERT_EQ(string("my_client"), gw_cfg2.ruuvi_cfg.mqtt.mqtt_client_id.buf);
    ASSERT_EQ(string("m_user1"), gw_cfg2.ruuvi_cfg.mqtt.mqtt_user.buf);
    ASSERT_EQ(string("m_pass1"), gw_cfg2.ruuvi_cfg.mqtt.mqtt_pass.buf);

    ASSERT_EQ(false, gw_cfg2.ruuvi_cfg.http.use_http_ruuvi);
    ASSERT_EQ(false, gw_cfg2.ruuvi_cfg.http.use_http);

    ASSERT_EQ(false, gw_cfg2.ruuvi_cfg.http_stat.use_http_stat);
    ASSERT_EQ(string("https://myserver1.com/status"), gw_cfg2.ruuvi_cfg.http_stat.http_stat_url.buf);
    ASSERT_EQ(string("h_user2"), gw_cfg2.ruuvi_cfg.http_stat.http_stat_user.buf);
    ASSERT_EQ(string("h_pass2"), gw_cfg2.ruuvi_cfg.http_stat.http_stat_pass.buf);

    ASSERT_EQ(HTTP_SERVER_AUTH_TYPE_DIGEST, gw_cfg2.ruuvi_cfg.lan_auth.lan_auth_type);
    ASSERT_EQ(string("l_user1"), gw_cfg2.ruuvi_cfg.lan_auth.lan_auth_user.buf);
    ASSERT_EQ(string("l_pass1"), gw_cfg2.ruuvi_cfg.lan_auth.lan_auth_pass.buf);
    ASSERT_EQ(string(""), gw_cfg2.ruuvi_cfg.lan_auth.lan_auth_api_key.buf);
    ASSERT_EQ(string(""), gw_cfg2.ruuvi_cfg.lan_auth.lan_auth_api_key_rw.buf);

    ASSERT_EQ(AUTO_UPDATE_CYCLE_TYPE_BETA_TESTER, gw_cfg2.ruuvi_cfg.auto_update.auto_update_cycle);
    ASSERT_EQ(0x3F, gw_cfg2.ruuvi_cfg.auto_update.auto_update_weekdays_bitmask);
    ASSERT_EQ(2, gw_cfg2.ruuvi_cfg.auto_update.auto_update_interval_from);
    ASSERT_EQ(22, gw_cfg2.ruuvi_cfg.auto_update.auto_update_interval_to);
    ASSERT_EQ(5, gw_cfg2.ruuvi_cfg.auto_update.auto_update_tz_offset_hours);

    ASSERT_EQ(true, gw_cfg2.ruuvi_cfg.ntp.ntp_use);
    ASSERT_EQ(false, gw_cfg2.ruuvi_cfg.ntp.ntp_use_dhcp);
    ASSERT_EQ(string("time.google.com"), string(gw_cfg2.ruuvi_cfg.ntp.ntp_server1.buf));
    ASSERT_EQ(string("time.cloudflare.com"), string(gw_cfg2.ruuvi_cfg.ntp.ntp_server2.buf));
    ASSERT_EQ(string("time.nist.gov"), string(gw_cfg2.ruuvi_cfg.ntp.ntp_server3.buf));
    ASSERT_EQ(string("pool.ntp.org"), string(gw_cfg2.ruuvi_cfg.ntp.ntp_server4.buf));

    ASSERT_EQ(RUUVI_COMPANY_ID + 1, gw_cfg2.ruuvi_cfg.filter.company_id);
    ASSERT_EQ(false, gw_cfg2.ruuvi_cfg.filter.company_use_filtering);

    ASSERT_EQ(true, gw_cfg2.ruuvi_cfg.scan.scan_coded_phy);
    ASSERT_EQ(false, gw_cfg2.ruuvi_cfg.scan.scan_1mbit_phy);
    ASSERT_EQ(false, gw_cfg2.ruuvi_cfg.scan.scan_extended_payload);
    ASSERT_EQ(false, gw_cfg2.ruuvi_cfg.scan.scan_channel_37);
    ASSERT_EQ(false, gw_cfg2.ruuvi_cfg.scan.scan_channel_38);
    ASSERT_EQ(false, gw_cfg2.ruuvi_cfg.scan.scan_channel_39);
    ASSERT_EQ(false, gw_cfg2.ruuvi_cfg.scan_filter.scan_filter_allow_listed);
    ASSERT_EQ(0, gw_cfg2.ruuvi_cfg.scan_filter.scan_filter_length);

    ASSERT_EQ(string("coordinates1"), gw_cfg2.ruuvi_cfg.coordinates.buf);
    ASSERT_EQ(string("https://myserver1.com/fw_update_info.json"), gw_cfg2.ruuvi_cfg.fw_update.fw_update_url);

    ASSERT_TRUE(gw_cfg_json_generate_for_saving(&gw_cfg2, &json_str));
    ASSERT_EQ(
        string("{\n"
               "\t\"wifi_sta_config\":\t{\n"
               "\t\t\"ssid\":\t\"\",\n"
               "\t\t\"password\":\t\"\"\n"
               "\t},\n"
               "\t\"wifi_ap_config\":\t{\n"
               "\t\t\"password\":\t\"\",\n"
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
               "\t\"use_http\":\tfalse,\n"
               "\t\"use_http_stat\":\tfalse,\n"
               "\t\"http_stat_url\":\t\"https://myserver1.com/status\",\n"
               "\t\"http_stat_user\":\t\"h_user2\",\n"
               "\t\"http_stat_pass\":\t\"h_pass2\",\n"
               "\t\"http_stat_use_ssl_client_cert\":\tfalse,\n"
               "\t\"http_stat_use_ssl_server_cert\":\tfalse,\n"
               "\t\"use_mqtt\":\ttrue,\n"
               "\t\"mqtt_disable_retained_messages\":\ttrue,\n"
               "\t\"mqtt_transport\":\t\"SSL\",\n"
               "\t\"mqtt_server\":\t\"test.mosquitto.org\",\n"
               "\t\"mqtt_port\":\t1338,\n"
               "\t\"mqtt_sending_interval\":\t0,\n"
               "\t\"mqtt_prefix\":\t\"my_prefix\",\n"
               "\t\"mqtt_client_id\":\t\"my_client\",\n"
               "\t\"mqtt_user\":\t\"m_user1\",\n"
               "\t\"mqtt_pass\":\t\"m_pass1\",\n"
               "\t\"mqtt_use_ssl_client_cert\":\tfalse,\n"
               "\t\"mqtt_use_ssl_server_cert\":\tfalse,\n"
               "\t\"lan_auth_type\":\t\"lan_auth_digest\",\n"
               "\t\"lan_auth_user\":\t\"l_user1\",\n"
               "\t\"lan_auth_pass\":\t\"l_pass1\",\n"
               "\t\"lan_auth_api_key\":\t\"\",\n"
               "\t\"lan_auth_api_key_rw\":\t\"\",\n"
               "\t\"auto_update_cycle\":\t\"beta\",\n"
               "\t\"auto_update_weekdays_bitmask\":\t63,\n"
               "\t\"auto_update_interval_from\":\t2,\n"
               "\t\"auto_update_interval_to\":\t22,\n"
               "\t\"auto_update_tz_offset_hours\":\t5,\n"
               "\t\"ntp_use\":\ttrue,\n"
               "\t\"ntp_use_dhcp\":\tfalse,\n"
               "\t\"ntp_server1\":\t\"time.google.com\",\n"
               "\t\"ntp_server2\":\t\"time.cloudflare.com\",\n"
               "\t\"ntp_server3\":\t\"time.nist.gov\",\n"
               "\t\"ntp_server4\":\t\"pool.ntp.org\",\n"
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
               "\t\"coordinates\":\t\"coordinates1\",\n"
               "\t\"fw_update_url\":\t\"https://myserver1.com/fw_update_info.json\"\n"
               "}"),
        string(json_str.p_str));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    cjson_wrap_free_json_str(&json_str);
}

TEST_F(TestGwCfgJson, gw_cfg_json_generate_parse_generate_auto_update_manual) // NOLINT
{
    const gw_cfg_t gw_cfg   = {
        .device_info = {
            .esp32_fw_ver = { "v1.10.0" },
            .nrf52_fw_ver = { "v0.7.2" },
            .nrf52_mac_addr = { "AA:BB:CC:DD:EE:FF" },
            .nrf52_device_id = { "11:22:33:44:55:66:77:88" },
        },
        .ruuvi_cfg = {
            .http = {
                .use_http_ruuvi = false,
                .use_http = false,
                .http_use_ssl_client_cert = false,
                .http_use_ssl_server_cert = false,
                .http_url = { "https://myserver1.com" },
                .data_format = GW_CFG_HTTP_DATA_FORMAT_RUUVI,
                .auth_type = GW_CFG_HTTP_AUTH_TYPE_BASIC,
                .auth = {
                    .auth_basic = {
                        .user = { "h_user1" },
                        .password = { "h_pass1" },
                    },
                },
            },
            .http_stat = {
                .use_http_stat = false,
                .http_stat_use_ssl_client_cert = false,
                .http_stat_use_ssl_server_cert = false,
                .http_stat_url = { "https://myserver1.com/status" },
                .http_stat_user = { "h_user2" },
                .http_stat_pass = { "h_pass2" },
            },
            .mqtt = {
                .use_mqtt = true,
                .mqtt_disable_retained_messages = false,
                .use_ssl_client_cert = false,
                .use_ssl_server_cert = false,
                .mqtt_transport = { "SSL" },
                .mqtt_server = { "test.mosquitto.org" },
                .mqtt_port = 1338,
                .mqtt_prefix = { "my_prefix" },
                .mqtt_client_id = { "my_client" },
                .mqtt_user = { "m_user1" },
                .mqtt_pass = { "m_pass1" },
            },
            .lan_auth = {
                HTTP_SERVER_AUTH_TYPE_DIGEST,
                "l_user1",
                "l_pass1",
                "",
            },
            .auto_update = {
                .auto_update_cycle = AUTO_UPDATE_CYCLE_TYPE_MANUAL,
                .auto_update_weekdays_bitmask = 0x3F,
                .auto_update_interval_from = 2,
                .auto_update_interval_to = 22,
                .auto_update_tz_offset_hours = 5,
            },
            .ntp = {
                .ntp_use = true,
                .ntp_use_dhcp = false,
                .ntp_server1 = { "time.google.com", },
                .ntp_server2 = { "time.cloudflare.com", },
                .ntp_server3 = { "time.nist.gov", },
                .ntp_server4 = { "pool.ntp.org", },
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
                .scan_filter_list = { 0 },
            },
            .coordinates = { "coordinates1" },
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

    ASSERT_TRUE(gw_cfg_json_generate_for_saving(&gw_cfg, &json_str));
    ASSERT_NE(nullptr, json_str.p_str);

    gw_cfg_t gw_cfg2 = get_gateway_config_default();
    ASSERT_TRUE(gw_cfg_json_parse("my.json", nullptr, json_str.p_str, &gw_cfg2));
    cjson_wrap_free_json_str(&json_str);

    ASSERT_EQ(true, gw_cfg2.eth_cfg.use_eth);
    ASSERT_EQ(false, gw_cfg2.eth_cfg.eth_dhcp);
    ASSERT_EQ(string("192.168.1.10"), gw_cfg2.eth_cfg.eth_static_ip.buf);
    ASSERT_EQ(string("255.255.255.0"), gw_cfg2.eth_cfg.eth_netmask.buf);
    ASSERT_EQ(string("192.168.1.1"), gw_cfg2.eth_cfg.eth_gw.buf);
    ASSERT_EQ(string("8.8.8.8"), gw_cfg2.eth_cfg.eth_dns1.buf);
    ASSERT_EQ(string("4.4.4.4"), gw_cfg2.eth_cfg.eth_dns2.buf);

    ASSERT_EQ(true, gw_cfg2.ruuvi_cfg.mqtt.use_mqtt);
    ASSERT_EQ(false, gw_cfg2.ruuvi_cfg.mqtt.mqtt_disable_retained_messages);
    ASSERT_EQ(string("SSL"), gw_cfg2.ruuvi_cfg.mqtt.mqtt_transport.buf);
    ASSERT_EQ(string("test.mosquitto.org"), gw_cfg2.ruuvi_cfg.mqtt.mqtt_server.buf);
    ASSERT_EQ(1338, gw_cfg2.ruuvi_cfg.mqtt.mqtt_port);
    ASSERT_EQ(string("my_prefix"), gw_cfg2.ruuvi_cfg.mqtt.mqtt_prefix.buf);
    ASSERT_EQ(string("my_client"), gw_cfg2.ruuvi_cfg.mqtt.mqtt_client_id.buf);
    ASSERT_EQ(string("m_user1"), gw_cfg2.ruuvi_cfg.mqtt.mqtt_user.buf);
    ASSERT_EQ(string("m_pass1"), gw_cfg2.ruuvi_cfg.mqtt.mqtt_pass.buf);

    ASSERT_EQ(false, gw_cfg2.ruuvi_cfg.http.use_http_ruuvi);
    ASSERT_EQ(false, gw_cfg2.ruuvi_cfg.http.use_http);

    ASSERT_EQ(false, gw_cfg2.ruuvi_cfg.http_stat.use_http_stat);
    ASSERT_EQ(string("https://myserver1.com/status"), gw_cfg2.ruuvi_cfg.http_stat.http_stat_url.buf);
    ASSERT_EQ(string("h_user2"), gw_cfg2.ruuvi_cfg.http_stat.http_stat_user.buf);
    ASSERT_EQ(string("h_pass2"), gw_cfg2.ruuvi_cfg.http_stat.http_stat_pass.buf);

    ASSERT_EQ(HTTP_SERVER_AUTH_TYPE_DIGEST, gw_cfg2.ruuvi_cfg.lan_auth.lan_auth_type);
    ASSERT_EQ(string("l_user1"), gw_cfg2.ruuvi_cfg.lan_auth.lan_auth_user.buf);
    ASSERT_EQ(string("l_pass1"), gw_cfg2.ruuvi_cfg.lan_auth.lan_auth_pass.buf);
    ASSERT_EQ(string(""), gw_cfg2.ruuvi_cfg.lan_auth.lan_auth_api_key.buf);
    ASSERT_EQ(string(""), gw_cfg2.ruuvi_cfg.lan_auth.lan_auth_api_key_rw.buf);

    ASSERT_EQ(AUTO_UPDATE_CYCLE_TYPE_MANUAL, gw_cfg2.ruuvi_cfg.auto_update.auto_update_cycle);
    ASSERT_EQ(0x3F, gw_cfg2.ruuvi_cfg.auto_update.auto_update_weekdays_bitmask);
    ASSERT_EQ(2, gw_cfg2.ruuvi_cfg.auto_update.auto_update_interval_from);
    ASSERT_EQ(22, gw_cfg2.ruuvi_cfg.auto_update.auto_update_interval_to);
    ASSERT_EQ(5, gw_cfg2.ruuvi_cfg.auto_update.auto_update_tz_offset_hours);

    ASSERT_EQ(true, gw_cfg2.ruuvi_cfg.ntp.ntp_use);
    ASSERT_EQ(false, gw_cfg2.ruuvi_cfg.ntp.ntp_use_dhcp);
    ASSERT_EQ(string("time.google.com"), string(gw_cfg2.ruuvi_cfg.ntp.ntp_server1.buf));
    ASSERT_EQ(string("time.cloudflare.com"), string(gw_cfg2.ruuvi_cfg.ntp.ntp_server2.buf));
    ASSERT_EQ(string("time.nist.gov"), string(gw_cfg2.ruuvi_cfg.ntp.ntp_server3.buf));
    ASSERT_EQ(string("pool.ntp.org"), string(gw_cfg2.ruuvi_cfg.ntp.ntp_server4.buf));

    ASSERT_EQ(RUUVI_COMPANY_ID + 1, gw_cfg2.ruuvi_cfg.filter.company_id);
    ASSERT_EQ(false, gw_cfg2.ruuvi_cfg.filter.company_use_filtering);

    ASSERT_EQ(true, gw_cfg2.ruuvi_cfg.scan.scan_coded_phy);
    ASSERT_EQ(false, gw_cfg2.ruuvi_cfg.scan.scan_1mbit_phy);
    ASSERT_EQ(false, gw_cfg2.ruuvi_cfg.scan.scan_extended_payload);
    ASSERT_EQ(false, gw_cfg2.ruuvi_cfg.scan.scan_channel_37);
    ASSERT_EQ(false, gw_cfg2.ruuvi_cfg.scan.scan_channel_38);
    ASSERT_EQ(false, gw_cfg2.ruuvi_cfg.scan.scan_channel_39);
    ASSERT_EQ(false, gw_cfg2.ruuvi_cfg.scan_filter.scan_filter_allow_listed);
    ASSERT_EQ(0, gw_cfg2.ruuvi_cfg.scan_filter.scan_filter_length);

    ASSERT_EQ(string("coordinates1"), gw_cfg2.ruuvi_cfg.coordinates.buf);
    ASSERT_EQ(string("https://myserver1.com/fw_update_info.json"), gw_cfg2.ruuvi_cfg.fw_update.fw_update_url);

    ASSERT_TRUE(gw_cfg_json_generate_for_saving(&gw_cfg2, &json_str));
    ASSERT_EQ(
        string("{\n"
               "\t\"wifi_sta_config\":\t{\n"
               "\t\t\"ssid\":\t\"\",\n"
               "\t\t\"password\":\t\"\"\n"
               "\t},\n"
               "\t\"wifi_ap_config\":\t{\n"
               "\t\t\"password\":\t\"\",\n"
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
               "\t\"use_http\":\tfalse,\n"
               "\t\"use_http_stat\":\tfalse,\n"
               "\t\"http_stat_url\":\t\"https://myserver1.com/status\",\n"
               "\t\"http_stat_user\":\t\"h_user2\",\n"
               "\t\"http_stat_pass\":\t\"h_pass2\",\n"
               "\t\"http_stat_use_ssl_client_cert\":\tfalse,\n"
               "\t\"http_stat_use_ssl_server_cert\":\tfalse,\n"
               "\t\"use_mqtt\":\ttrue,\n"
               "\t\"mqtt_disable_retained_messages\":\tfalse,\n"
               "\t\"mqtt_transport\":\t\"SSL\",\n"
               "\t\"mqtt_server\":\t\"test.mosquitto.org\",\n"
               "\t\"mqtt_port\":\t1338,\n"
               "\t\"mqtt_sending_interval\":\t0,\n"
               "\t\"mqtt_prefix\":\t\"my_prefix\",\n"
               "\t\"mqtt_client_id\":\t\"my_client\",\n"
               "\t\"mqtt_user\":\t\"m_user1\",\n"
               "\t\"mqtt_pass\":\t\"m_pass1\",\n"
               "\t\"mqtt_use_ssl_client_cert\":\tfalse,\n"
               "\t\"mqtt_use_ssl_server_cert\":\tfalse,\n"
               "\t\"lan_auth_type\":\t\"lan_auth_digest\",\n"
               "\t\"lan_auth_user\":\t\"l_user1\",\n"
               "\t\"lan_auth_pass\":\t\"l_pass1\",\n"
               "\t\"lan_auth_api_key\":\t\"\",\n"
               "\t\"lan_auth_api_key_rw\":\t\"\",\n"
               "\t\"auto_update_cycle\":\t\"manual\",\n"
               "\t\"auto_update_weekdays_bitmask\":\t63,\n"
               "\t\"auto_update_interval_from\":\t2,\n"
               "\t\"auto_update_interval_to\":\t22,\n"
               "\t\"auto_update_tz_offset_hours\":\t5,\n"
               "\t\"ntp_use\":\ttrue,\n"
               "\t\"ntp_use_dhcp\":\tfalse,\n"
               "\t\"ntp_server1\":\t\"time.google.com\",\n"
               "\t\"ntp_server2\":\t\"time.cloudflare.com\",\n"
               "\t\"ntp_server3\":\t\"time.nist.gov\",\n"
               "\t\"ntp_server4\":\t\"pool.ntp.org\",\n"
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
               "\t\"coordinates\":\t\"coordinates1\",\n"
               "\t\"fw_update_url\":\t\"https://myserver1.com/fw_update_info.json\"\n"
               "}"),
        string(json_str.p_str));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    cjson_wrap_free_json_str(&json_str);
}

TEST_F(TestGwCfgJson, gw_cfg_json_generate_parse_generate_auto_update_unknown) // NOLINT
{
    const char* const p_json_str
        = "{\n"
          "\t\"wifi_sta_config\":\t{\n"
          "\t\t\"ssid\":\t\"\",\n"
          "\t\t\"password\":\t\"\"\n"
          "\t},\n"
          "\t\"wifi_ap_config\":\t{\n"
          "\t\t\"password\":\t\"\",\n"
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
          "\t\"use_http\":\tfalse,\n"
          "\t\"http_url\":\t\"https://myserver1.com\",\n"
          "\t\"http_data_format\":\t\"ruuvi\",\n"
          "\t\"http_auth\":\t\"none\",\n"
          "\t\"use_http_stat\":\tfalse,\n"
          "\t\"http_stat_url\":\t\"https://myserver1.com/status\",\n"
          "\t\"http_stat_user\":\t\"h_user2\",\n"
          "\t\"http_stat_pass\":\t\"h_pass2\",\n"
          "\t\"http_stat_use_ssl_client_cert\":\tfalse,\n"
          "\t\"http_stat_use_ssl_server_cert\":\tfalse,\n"
          "\t\"use_mqtt\":\ttrue,\n"
          "\t\"mqtt_disable_retained_messages\":\tfalse,\n"
          "\t\"mqtt_transport\":\t\"SSL\",\n"
          "\t\"mqtt_server\":\t\"test.mosquitto.org\",\n"
          "\t\"mqtt_port\":\t1338,\n"
          "\t\"mqtt_sending_interval\":\t0,\n"
          "\t\"mqtt_prefix\":\t\"my_prefix\",\n"
          "\t\"mqtt_client_id\":\t\"my_client\",\n"
          "\t\"mqtt_user\":\t\"m_user1\",\n"
          "\t\"mqtt_pass\":\t\"m_pass1\",\n"
          "\t\"mqtt_use_ssl_client_cert\":\tfalse,\n"
          "\t\"mqtt_use_ssl_server_cert\":\tfalse,\n"
          "\t\"lan_auth_type\":\t\"lan_auth_digest\",\n"
          "\t\"lan_auth_user\":\t\"l_user1\",\n"
          "\t\"lan_auth_pass\":\t\"l_pass1\",\n"
          "\t\"lan_auth_api_key\":\t\"\",\n"
          "\t\"lan_auth_api_key_rw\":\t\"\",\n"
          "\t\"auto_update_cycle\":\t\"unknown\",\n"
          "\t\"auto_update_weekdays_bitmask\":\t63,\n"
          "\t\"auto_update_interval_from\":\t2,\n"
          "\t\"auto_update_interval_to\":\t22,\n"
          "\t\"auto_update_tz_offset_hours\":\t5,\n"
          "\t\"ntp_use\":\ttrue,\n"
          "\t\"ntp_use_dhcp\":\tfalse,\n"
          "\t\"ntp_server1\":\t\"time.google.com\",\n"
          "\t\"ntp_server2\":\t\"time.cloudflare.com\",\n"
          "\t\"ntp_server3\":\t\"time.nist.gov\",\n"
          "\t\"ntp_server4\":\t\"pool.ntp.org\",\n"
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
          "\t\"coordinates\":\t\"coordinates1\",\n"
          "\t\"fw_update_url\":\t\"https://network.ruuvi.com/firmwareupdate\"\n"
          "}";

    gw_cfg_t gw_cfg2 = get_gateway_config_default();
    ASSERT_TRUE(gw_cfg_json_parse("my.json", nullptr, p_json_str, &gw_cfg2));
    TEST_CHECK_LOG_RECORD(ESP_LOG_WARN, string("Unknown auto_update_cycle='unknown', use REGULAR"));
    ASSERT_TRUE(esp_log_wrapper_is_empty());

    ASSERT_EQ(true, gw_cfg2.eth_cfg.use_eth);
    ASSERT_EQ(false, gw_cfg2.eth_cfg.eth_dhcp);
    ASSERT_EQ(string("192.168.1.10"), gw_cfg2.eth_cfg.eth_static_ip.buf);
    ASSERT_EQ(string("255.255.255.0"), gw_cfg2.eth_cfg.eth_netmask.buf);
    ASSERT_EQ(string("192.168.1.1"), gw_cfg2.eth_cfg.eth_gw.buf);
    ASSERT_EQ(string("8.8.8.8"), gw_cfg2.eth_cfg.eth_dns1.buf);
    ASSERT_EQ(string("4.4.4.4"), gw_cfg2.eth_cfg.eth_dns2.buf);

    ASSERT_EQ(true, gw_cfg2.ruuvi_cfg.mqtt.use_mqtt);
    ASSERT_EQ(false, gw_cfg2.ruuvi_cfg.mqtt.mqtt_disable_retained_messages);
    ASSERT_EQ(string("SSL"), gw_cfg2.ruuvi_cfg.mqtt.mqtt_transport.buf);
    ASSERT_EQ(string("test.mosquitto.org"), gw_cfg2.ruuvi_cfg.mqtt.mqtt_server.buf);
    ASSERT_EQ(1338, gw_cfg2.ruuvi_cfg.mqtt.mqtt_port);
    ASSERT_EQ(string("my_prefix"), gw_cfg2.ruuvi_cfg.mqtt.mqtt_prefix.buf);
    ASSERT_EQ(string("my_client"), gw_cfg2.ruuvi_cfg.mqtt.mqtt_client_id.buf);
    ASSERT_EQ(string("m_user1"), gw_cfg2.ruuvi_cfg.mqtt.mqtt_user.buf);
    ASSERT_EQ(string("m_pass1"), gw_cfg2.ruuvi_cfg.mqtt.mqtt_pass.buf);

    ASSERT_EQ(false, gw_cfg2.ruuvi_cfg.http.use_http_ruuvi);
    ASSERT_EQ(false, gw_cfg2.ruuvi_cfg.http.use_http);

    ASSERT_EQ(false, gw_cfg2.ruuvi_cfg.http_stat.use_http_stat);
    ASSERT_EQ(string("https://myserver1.com/status"), gw_cfg2.ruuvi_cfg.http_stat.http_stat_url.buf);
    ASSERT_EQ(string("h_user2"), gw_cfg2.ruuvi_cfg.http_stat.http_stat_user.buf);
    ASSERT_EQ(string("h_pass2"), gw_cfg2.ruuvi_cfg.http_stat.http_stat_pass.buf);

    ASSERT_EQ(HTTP_SERVER_AUTH_TYPE_DIGEST, gw_cfg2.ruuvi_cfg.lan_auth.lan_auth_type);
    ASSERT_EQ(string("l_user1"), gw_cfg2.ruuvi_cfg.lan_auth.lan_auth_user.buf);
    ASSERT_EQ(string("l_pass1"), gw_cfg2.ruuvi_cfg.lan_auth.lan_auth_pass.buf);
    ASSERT_EQ(string(""), gw_cfg2.ruuvi_cfg.lan_auth.lan_auth_api_key.buf);
    ASSERT_EQ(string(""), gw_cfg2.ruuvi_cfg.lan_auth.lan_auth_api_key_rw.buf);

    ASSERT_EQ(AUTO_UPDATE_CYCLE_TYPE_REGULAR, gw_cfg2.ruuvi_cfg.auto_update.auto_update_cycle);
    ASSERT_EQ(0x3F, gw_cfg2.ruuvi_cfg.auto_update.auto_update_weekdays_bitmask);
    ASSERT_EQ(2, gw_cfg2.ruuvi_cfg.auto_update.auto_update_interval_from);
    ASSERT_EQ(22, gw_cfg2.ruuvi_cfg.auto_update.auto_update_interval_to);
    ASSERT_EQ(5, gw_cfg2.ruuvi_cfg.auto_update.auto_update_tz_offset_hours);

    ASSERT_EQ(true, gw_cfg2.ruuvi_cfg.ntp.ntp_use);
    ASSERT_EQ(false, gw_cfg2.ruuvi_cfg.ntp.ntp_use_dhcp);
    ASSERT_EQ(string("time.google.com"), string(gw_cfg2.ruuvi_cfg.ntp.ntp_server1.buf));
    ASSERT_EQ(string("time.cloudflare.com"), string(gw_cfg2.ruuvi_cfg.ntp.ntp_server2.buf));
    ASSERT_EQ(string("time.nist.gov"), string(gw_cfg2.ruuvi_cfg.ntp.ntp_server3.buf));
    ASSERT_EQ(string("pool.ntp.org"), string(gw_cfg2.ruuvi_cfg.ntp.ntp_server4.buf));

    ASSERT_EQ(RUUVI_COMPANY_ID + 1, gw_cfg2.ruuvi_cfg.filter.company_id);
    ASSERT_EQ(false, gw_cfg2.ruuvi_cfg.filter.company_use_filtering);

    ASSERT_EQ(true, gw_cfg2.ruuvi_cfg.scan.scan_coded_phy);
    ASSERT_EQ(false, gw_cfg2.ruuvi_cfg.scan.scan_1mbit_phy);
    ASSERT_EQ(false, gw_cfg2.ruuvi_cfg.scan.scan_extended_payload);
    ASSERT_EQ(false, gw_cfg2.ruuvi_cfg.scan.scan_channel_37);
    ASSERT_EQ(false, gw_cfg2.ruuvi_cfg.scan.scan_channel_38);
    ASSERT_EQ(false, gw_cfg2.ruuvi_cfg.scan.scan_channel_39);
    ASSERT_EQ(false, gw_cfg2.ruuvi_cfg.scan_filter.scan_filter_allow_listed);
    ASSERT_EQ(0, gw_cfg2.ruuvi_cfg.scan_filter.scan_filter_length);

    ASSERT_EQ(string("coordinates1"), gw_cfg2.ruuvi_cfg.coordinates.buf);
    ASSERT_EQ(string("https://network.ruuvi.com/firmwareupdate"), gw_cfg2.ruuvi_cfg.fw_update.fw_update_url);

    cjson_wrap_str_t json_str = cjson_wrap_str_null();
    ASSERT_TRUE(gw_cfg_json_generate_for_saving(&gw_cfg2, &json_str));
    ASSERT_EQ(
        string("{\n"
               "\t\"wifi_sta_config\":\t{\n"
               "\t\t\"ssid\":\t\"\",\n"
               "\t\t\"password\":\t\"\"\n"
               "\t},\n"
               "\t\"wifi_ap_config\":\t{\n"
               "\t\t\"password\":\t\"\",\n"
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
               "\t\"use_http\":\tfalse,\n"
               "\t\"use_http_stat\":\tfalse,\n"
               "\t\"http_stat_url\":\t\"https://myserver1.com/status\",\n"
               "\t\"http_stat_user\":\t\"h_user2\",\n"
               "\t\"http_stat_pass\":\t\"h_pass2\",\n"
               "\t\"http_stat_use_ssl_client_cert\":\tfalse,\n"
               "\t\"http_stat_use_ssl_server_cert\":\tfalse,\n"
               "\t\"use_mqtt\":\ttrue,\n"
               "\t\"mqtt_disable_retained_messages\":\tfalse,\n"
               "\t\"mqtt_transport\":\t\"SSL\",\n"
               "\t\"mqtt_server\":\t\"test.mosquitto.org\",\n"
               "\t\"mqtt_port\":\t1338,\n"
               "\t\"mqtt_sending_interval\":\t0,\n"
               "\t\"mqtt_prefix\":\t\"my_prefix\",\n"
               "\t\"mqtt_client_id\":\t\"my_client\",\n"
               "\t\"mqtt_user\":\t\"m_user1\",\n"
               "\t\"mqtt_pass\":\t\"m_pass1\",\n"
               "\t\"mqtt_use_ssl_client_cert\":\tfalse,\n"
               "\t\"mqtt_use_ssl_server_cert\":\tfalse,\n"
               "\t\"lan_auth_type\":\t\"lan_auth_digest\",\n"
               "\t\"lan_auth_user\":\t\"l_user1\",\n"
               "\t\"lan_auth_pass\":\t\"l_pass1\",\n"
               "\t\"lan_auth_api_key\":\t\"\",\n"
               "\t\"lan_auth_api_key_rw\":\t\"\",\n"
               "\t\"auto_update_cycle\":\t\"regular\",\n"
               "\t\"auto_update_weekdays_bitmask\":\t63,\n"
               "\t\"auto_update_interval_from\":\t2,\n"
               "\t\"auto_update_interval_to\":\t22,\n"
               "\t\"auto_update_tz_offset_hours\":\t5,\n"
               "\t\"ntp_use\":\ttrue,\n"
               "\t\"ntp_use_dhcp\":\tfalse,\n"
               "\t\"ntp_server1\":\t\"time.google.com\",\n"
               "\t\"ntp_server2\":\t\"time.cloudflare.com\",\n"
               "\t\"ntp_server3\":\t\"time.nist.gov\",\n"
               "\t\"ntp_server4\":\t\"pool.ntp.org\",\n"
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
               "\t\"coordinates\":\t\"coordinates1\",\n"
               "\t\"fw_update_url\":\t\"https://network.ruuvi.com/firmwareupdate\"\n"
               "}"),
        string(json_str.p_str));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    cjson_wrap_free_json_str(&json_str);
}

TEST_F(TestGwCfgJson, gw_cfg_json_generate_parse_generate_ntp_disabled) // NOLINT
{
    const gw_cfg_t gw_cfg   = {
        .device_info = {
            .esp32_fw_ver = { "v1.10.0" },
            .nrf52_fw_ver = { "v0.7.2" },
            .nrf52_mac_addr = { "AA:BB:CC:DD:EE:FF" },
            .nrf52_device_id = { "11:22:33:44:55:66:77:88" },
        },
        .ruuvi_cfg = {
            .http = {
                .use_http_ruuvi = false,
                .use_http = false,
                .http_use_ssl_client_cert = false,
                .http_use_ssl_server_cert = false,
                .http_url = { "https://myserver1.com" },
                .data_format = GW_CFG_HTTP_DATA_FORMAT_RUUVI,
                .auth_type = GW_CFG_HTTP_AUTH_TYPE_BASIC,
                .auth = {
                    .auth_basic = {
                        .user = { "h_user1" },
                        .password = { "h_pass1" },
                    },
                },
            },
            .http_stat = {
                .use_http_stat = false,
                .http_stat_use_ssl_client_cert = false,
                .http_stat_use_ssl_server_cert = false,
                .http_stat_url = { "https://myserver1.com/status" },
                .http_stat_user = { "h_user2" },
                .http_stat_pass = { "h_pass2" },
            },
            .mqtt = {
                .use_mqtt = true,
                .mqtt_disable_retained_messages = false,
                .use_ssl_client_cert = false,
                .use_ssl_server_cert = false,
                .mqtt_transport = { "SSL" },
                .mqtt_server = { "test.mosquitto.org" },
                .mqtt_port = 1338,
                .mqtt_prefix = { "my_prefix" },
                .mqtt_client_id = { "my_client" },
                .mqtt_user = { "m_user1" },
                .mqtt_pass = { "m_pass1" },
            },
            .lan_auth = {
                HTTP_SERVER_AUTH_TYPE_DIGEST,
                "l_user1",
                "l_pass1",
                "",
            },
            .auto_update = {
                .auto_update_cycle = AUTO_UPDATE_CYCLE_TYPE_REGULAR,
                .auto_update_weekdays_bitmask = 0x3F,
                .auto_update_interval_from = 2,
                .auto_update_interval_to = 22,
                .auto_update_tz_offset_hours = 5,
            },
            .ntp = {
                .ntp_use = false,
                .ntp_use_dhcp = false,
                .ntp_server1 = { "", },
                .ntp_server2 = { "", },
                .ntp_server3 = { "", },
                .ntp_server4 = { "", },
            },
            .filter = {
                .company_id = RUUVI_COMPANY_ID,
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
                .scan_filter_list = { 0 },
            },
            .coordinates = { "coordinates1" },
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

    ASSERT_TRUE(gw_cfg_json_generate_for_saving(&gw_cfg, &json_str));
    ASSERT_NE(nullptr, json_str.p_str);

    gw_cfg_t gw_cfg2 = get_gateway_config_default();
    ASSERT_TRUE(gw_cfg_json_parse("my.json", nullptr, json_str.p_str, &gw_cfg2));
    cjson_wrap_free_json_str(&json_str);

    ASSERT_EQ(true, gw_cfg2.eth_cfg.use_eth);
    ASSERT_EQ(false, gw_cfg2.eth_cfg.eth_dhcp);
    ASSERT_EQ(string("192.168.1.10"), gw_cfg2.eth_cfg.eth_static_ip.buf);
    ASSERT_EQ(string("255.255.255.0"), gw_cfg2.eth_cfg.eth_netmask.buf);
    ASSERT_EQ(string("192.168.1.1"), gw_cfg2.eth_cfg.eth_gw.buf);
    ASSERT_EQ(string("8.8.8.8"), gw_cfg2.eth_cfg.eth_dns1.buf);
    ASSERT_EQ(string("4.4.4.4"), gw_cfg2.eth_cfg.eth_dns2.buf);

    ASSERT_EQ(true, gw_cfg2.ruuvi_cfg.mqtt.use_mqtt);
    ASSERT_EQ(false, gw_cfg2.ruuvi_cfg.mqtt.mqtt_disable_retained_messages);
    ASSERT_EQ(string("SSL"), gw_cfg2.ruuvi_cfg.mqtt.mqtt_transport.buf);
    ASSERT_EQ(string("test.mosquitto.org"), gw_cfg2.ruuvi_cfg.mqtt.mqtt_server.buf);
    ASSERT_EQ(1338, gw_cfg2.ruuvi_cfg.mqtt.mqtt_port);
    ASSERT_EQ(string("my_prefix"), gw_cfg2.ruuvi_cfg.mqtt.mqtt_prefix.buf);
    ASSERT_EQ(string("my_client"), gw_cfg2.ruuvi_cfg.mqtt.mqtt_client_id.buf);
    ASSERT_EQ(string("m_user1"), gw_cfg2.ruuvi_cfg.mqtt.mqtt_user.buf);
    ASSERT_EQ(string("m_pass1"), gw_cfg2.ruuvi_cfg.mqtt.mqtt_pass.buf);

    ASSERT_EQ(false, gw_cfg2.ruuvi_cfg.http.use_http_ruuvi);
    ASSERT_EQ(false, gw_cfg2.ruuvi_cfg.http.use_http);

    ASSERT_EQ(false, gw_cfg2.ruuvi_cfg.http_stat.use_http_stat);
    ASSERT_EQ(string("https://myserver1.com/status"), gw_cfg2.ruuvi_cfg.http_stat.http_stat_url.buf);
    ASSERT_EQ(string("h_user2"), gw_cfg2.ruuvi_cfg.http_stat.http_stat_user.buf);
    ASSERT_EQ(string("h_pass2"), gw_cfg2.ruuvi_cfg.http_stat.http_stat_pass.buf);

    ASSERT_EQ(HTTP_SERVER_AUTH_TYPE_DIGEST, gw_cfg2.ruuvi_cfg.lan_auth.lan_auth_type);
    ASSERT_EQ(string("l_user1"), gw_cfg2.ruuvi_cfg.lan_auth.lan_auth_user.buf);
    ASSERT_EQ(string("l_pass1"), gw_cfg2.ruuvi_cfg.lan_auth.lan_auth_pass.buf);
    ASSERT_EQ(string(""), gw_cfg2.ruuvi_cfg.lan_auth.lan_auth_api_key.buf);
    ASSERT_EQ(string(""), gw_cfg2.ruuvi_cfg.lan_auth.lan_auth_api_key_rw.buf);

    ASSERT_EQ(AUTO_UPDATE_CYCLE_TYPE_REGULAR, gw_cfg2.ruuvi_cfg.auto_update.auto_update_cycle);
    ASSERT_EQ(0x3F, gw_cfg2.ruuvi_cfg.auto_update.auto_update_weekdays_bitmask);
    ASSERT_EQ(2, gw_cfg2.ruuvi_cfg.auto_update.auto_update_interval_from);
    ASSERT_EQ(22, gw_cfg2.ruuvi_cfg.auto_update.auto_update_interval_to);
    ASSERT_EQ(5, gw_cfg2.ruuvi_cfg.auto_update.auto_update_tz_offset_hours);

    ASSERT_EQ(false, gw_cfg2.ruuvi_cfg.ntp.ntp_use);
    ASSERT_EQ(false, gw_cfg2.ruuvi_cfg.ntp.ntp_use_dhcp);
    ASSERT_EQ(string("time.google.com"), string(gw_cfg2.ruuvi_cfg.ntp.ntp_server1.buf));
    ASSERT_EQ(string("time.cloudflare.com"), string(gw_cfg2.ruuvi_cfg.ntp.ntp_server2.buf));
    ASSERT_EQ(string("time.nist.gov"), string(gw_cfg2.ruuvi_cfg.ntp.ntp_server3.buf));
    ASSERT_EQ(string("pool.ntp.org"), string(gw_cfg2.ruuvi_cfg.ntp.ntp_server4.buf));

    ASSERT_EQ(RUUVI_COMPANY_ID, gw_cfg2.ruuvi_cfg.filter.company_id);
    ASSERT_EQ(false, gw_cfg2.ruuvi_cfg.filter.company_use_filtering);

    ASSERT_EQ(true, gw_cfg2.ruuvi_cfg.scan.scan_coded_phy);
    ASSERT_EQ(false, gw_cfg2.ruuvi_cfg.scan.scan_1mbit_phy);
    ASSERT_EQ(false, gw_cfg2.ruuvi_cfg.scan.scan_extended_payload);
    ASSERT_EQ(false, gw_cfg2.ruuvi_cfg.scan.scan_channel_37);
    ASSERT_EQ(false, gw_cfg2.ruuvi_cfg.scan.scan_channel_38);
    ASSERT_EQ(false, gw_cfg2.ruuvi_cfg.scan.scan_channel_39);
    ASSERT_EQ(false, gw_cfg2.ruuvi_cfg.scan_filter.scan_filter_allow_listed);
    ASSERT_EQ(0, gw_cfg2.ruuvi_cfg.scan_filter.scan_filter_length);

    ASSERT_EQ(string("coordinates1"), gw_cfg2.ruuvi_cfg.coordinates.buf);
    ASSERT_EQ(string("https://myserver1.com/fw_update_info.json"), gw_cfg2.ruuvi_cfg.fw_update.fw_update_url);

    ASSERT_TRUE(gw_cfg_json_generate_for_saving(&gw_cfg2, &json_str));
    ASSERT_EQ(
        string("{\n"
               "\t\"wifi_sta_config\":\t{\n"
               "\t\t\"ssid\":\t\"\",\n"
               "\t\t\"password\":\t\"\"\n"
               "\t},\n"
               "\t\"wifi_ap_config\":\t{\n"
               "\t\t\"password\":\t\"\",\n"
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
               "\t\"use_http\":\tfalse,\n"
               "\t\"use_http_stat\":\tfalse,\n"
               "\t\"http_stat_url\":\t\"https://myserver1.com/status\",\n"
               "\t\"http_stat_user\":\t\"h_user2\",\n"
               "\t\"http_stat_pass\":\t\"h_pass2\",\n"
               "\t\"http_stat_use_ssl_client_cert\":\tfalse,\n"
               "\t\"http_stat_use_ssl_server_cert\":\tfalse,\n"
               "\t\"use_mqtt\":\ttrue,\n"
               "\t\"mqtt_disable_retained_messages\":\tfalse,\n"
               "\t\"mqtt_transport\":\t\"SSL\",\n"
               "\t\"mqtt_server\":\t\"test.mosquitto.org\",\n"
               "\t\"mqtt_port\":\t1338,\n"
               "\t\"mqtt_sending_interval\":\t0,\n"
               "\t\"mqtt_prefix\":\t\"my_prefix\",\n"
               "\t\"mqtt_client_id\":\t\"my_client\",\n"
               "\t\"mqtt_user\":\t\"m_user1\",\n"
               "\t\"mqtt_pass\":\t\"m_pass1\",\n"
               "\t\"mqtt_use_ssl_client_cert\":\tfalse,\n"
               "\t\"mqtt_use_ssl_server_cert\":\tfalse,\n"
               "\t\"lan_auth_type\":\t\"lan_auth_digest\",\n"
               "\t\"lan_auth_user\":\t\"l_user1\",\n"
               "\t\"lan_auth_pass\":\t\"l_pass1\",\n"
               "\t\"lan_auth_api_key\":\t\"\",\n"
               "\t\"lan_auth_api_key_rw\":\t\"\",\n"
               "\t\"auto_update_cycle\":\t\"regular\",\n"
               "\t\"auto_update_weekdays_bitmask\":\t63,\n"
               "\t\"auto_update_interval_from\":\t2,\n"
               "\t\"auto_update_interval_to\":\t22,\n"
               "\t\"auto_update_tz_offset_hours\":\t5,\n"
               "\t\"ntp_use\":\tfalse,\n"
               "\t\"ntp_use_dhcp\":\tfalse,\n"
               "\t\"ntp_server1\":\t\"time.google.com\",\n"
               "\t\"ntp_server2\":\t\"time.cloudflare.com\",\n"
               "\t\"ntp_server3\":\t\"time.nist.gov\",\n"
               "\t\"ntp_server4\":\t\"pool.ntp.org\",\n"
               "\t\"company_id\":\t1177,\n"
               "\t\"company_use_filtering\":\tfalse,\n"
               "\t\"scan_coded_phy\":\ttrue,\n"
               "\t\"scan_1mbit_phy\":\tfalse,\n"
               "\t\"scan_extended_payload\":\tfalse,\n"
               "\t\"scan_channel_37\":\tfalse,\n"
               "\t\"scan_channel_38\":\tfalse,\n"
               "\t\"scan_channel_39\":\tfalse,\n"
               "\t\"scan_filter_allow_listed\":\tfalse,\n"
               "\t\"scan_filter_list\":\t[],\n"
               "\t\"coordinates\":\t\"coordinates1\",\n"
               "\t\"fw_update_url\":\t\"https://myserver1.com/fw_update_info.json\"\n"
               "}"),
        string(json_str.p_str));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    cjson_wrap_free_json_str(&json_str);
}

TEST_F(TestGwCfgJson, gw_cfg_json_generate_parse_generate_ntp_enabled_via_dhcp) // NOLINT
{
    const gw_cfg_t gw_cfg   = {
        .device_info = {
            .esp32_fw_ver = { "v1.10.0" },
            .nrf52_fw_ver = { "v0.7.2" },
            .nrf52_mac_addr = { "AA:BB:CC:DD:EE:FF" },
            .nrf52_device_id = { "11:22:33:44:55:66:77:88" },
        },
        .ruuvi_cfg = {
            .http = {
                .use_http_ruuvi = false,
                .use_http = false,
                .http_use_ssl_client_cert = false,
                .http_use_ssl_server_cert = false,
                .http_url = { "https://myserver1.com" },
                .data_format = GW_CFG_HTTP_DATA_FORMAT_RUUVI,
                .auth_type = GW_CFG_HTTP_AUTH_TYPE_BASIC,
                .auth = {
                    .auth_basic = {
                        .user = { "h_user1" },
                        .password = { "h_pass1" },
                    },
                },
            },
            .http_stat = {
                .use_http_stat = false,
                .http_stat_use_ssl_client_cert = false,
                .http_stat_use_ssl_server_cert = false,
                .http_stat_url = { "https://myserver1.com/status" },
                .http_stat_user = { "h_user2" },
                .http_stat_pass = { "h_pass2" },
            },
            .mqtt = {
                .use_mqtt = true,
                .mqtt_disable_retained_messages = false,
                .use_ssl_client_cert = false,
                .use_ssl_server_cert = false,
                .mqtt_transport = { "SSL" },
                .mqtt_server = { "test.mosquitto.org" },
                .mqtt_port = 1338,
                .mqtt_prefix = { "my_prefix" },
                .mqtt_client_id = { "my_client" },
                .mqtt_user = { "m_user1" },
                .mqtt_pass = { "m_pass1" },
            },
            .lan_auth = {
                HTTP_SERVER_AUTH_TYPE_DIGEST,
                "l_user1",
                "l_pass1",
                "",
            },
            .auto_update = {
                .auto_update_cycle = AUTO_UPDATE_CYCLE_TYPE_REGULAR,
                .auto_update_weekdays_bitmask = 0x3F,
                .auto_update_interval_from = 2,
                .auto_update_interval_to = 22,
                .auto_update_tz_offset_hours = 5,
            },
            .ntp = {
                .ntp_use = true,
                .ntp_use_dhcp = true,
                .ntp_server1 = { "", },
                .ntp_server2 = { "", },
                .ntp_server3 = { "", },
                .ntp_server4 = { "", },
            },
            .filter = {
                .company_id = RUUVI_COMPANY_ID,
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
                .scan_filter_list = { 0 },
            },
            .coordinates = { "coordinates1" },
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

    ASSERT_TRUE(gw_cfg_json_generate_for_saving(&gw_cfg, &json_str));
    ASSERT_NE(nullptr, json_str.p_str);

    gw_cfg_t gw_cfg2 = get_gateway_config_default();
    ASSERT_TRUE(gw_cfg_json_parse("my.json", nullptr, json_str.p_str, &gw_cfg2));
    cjson_wrap_free_json_str(&json_str);

    ASSERT_EQ(true, gw_cfg2.eth_cfg.use_eth);
    ASSERT_EQ(false, gw_cfg2.eth_cfg.eth_dhcp);
    ASSERT_EQ(string("192.168.1.10"), gw_cfg2.eth_cfg.eth_static_ip.buf);
    ASSERT_EQ(string("255.255.255.0"), gw_cfg2.eth_cfg.eth_netmask.buf);
    ASSERT_EQ(string("192.168.1.1"), gw_cfg2.eth_cfg.eth_gw.buf);
    ASSERT_EQ(string("8.8.8.8"), gw_cfg2.eth_cfg.eth_dns1.buf);
    ASSERT_EQ(string("4.4.4.4"), gw_cfg2.eth_cfg.eth_dns2.buf);

    ASSERT_EQ(true, gw_cfg2.ruuvi_cfg.mqtt.use_mqtt);
    ASSERT_EQ(false, gw_cfg2.ruuvi_cfg.mqtt.mqtt_disable_retained_messages);
    ASSERT_EQ(string("SSL"), gw_cfg2.ruuvi_cfg.mqtt.mqtt_transport.buf);
    ASSERT_EQ(string("test.mosquitto.org"), gw_cfg2.ruuvi_cfg.mqtt.mqtt_server.buf);
    ASSERT_EQ(1338, gw_cfg2.ruuvi_cfg.mqtt.mqtt_port);
    ASSERT_EQ(string("my_prefix"), gw_cfg2.ruuvi_cfg.mqtt.mqtt_prefix.buf);
    ASSERT_EQ(string("my_client"), gw_cfg2.ruuvi_cfg.mqtt.mqtt_client_id.buf);
    ASSERT_EQ(string("m_user1"), gw_cfg2.ruuvi_cfg.mqtt.mqtt_user.buf);
    ASSERT_EQ(string("m_pass1"), gw_cfg2.ruuvi_cfg.mqtt.mqtt_pass.buf);

    ASSERT_EQ(false, gw_cfg2.ruuvi_cfg.http.use_http_ruuvi);
    ASSERT_EQ(false, gw_cfg2.ruuvi_cfg.http.use_http);

    ASSERT_EQ(false, gw_cfg2.ruuvi_cfg.http_stat.use_http_stat);
    ASSERT_EQ(string("https://myserver1.com/status"), gw_cfg2.ruuvi_cfg.http_stat.http_stat_url.buf);
    ASSERT_EQ(string("h_user2"), gw_cfg2.ruuvi_cfg.http_stat.http_stat_user.buf);
    ASSERT_EQ(string("h_pass2"), gw_cfg2.ruuvi_cfg.http_stat.http_stat_pass.buf);

    ASSERT_EQ(HTTP_SERVER_AUTH_TYPE_DIGEST, gw_cfg2.ruuvi_cfg.lan_auth.lan_auth_type);
    ASSERT_EQ(string("l_user1"), gw_cfg2.ruuvi_cfg.lan_auth.lan_auth_user.buf);
    ASSERT_EQ(string("l_pass1"), gw_cfg2.ruuvi_cfg.lan_auth.lan_auth_pass.buf);
    ASSERT_EQ(string(""), gw_cfg2.ruuvi_cfg.lan_auth.lan_auth_api_key.buf);
    ASSERT_EQ(string(""), gw_cfg2.ruuvi_cfg.lan_auth.lan_auth_api_key_rw.buf);

    ASSERT_EQ(AUTO_UPDATE_CYCLE_TYPE_REGULAR, gw_cfg2.ruuvi_cfg.auto_update.auto_update_cycle);
    ASSERT_EQ(0x3F, gw_cfg2.ruuvi_cfg.auto_update.auto_update_weekdays_bitmask);
    ASSERT_EQ(2, gw_cfg2.ruuvi_cfg.auto_update.auto_update_interval_from);
    ASSERT_EQ(22, gw_cfg2.ruuvi_cfg.auto_update.auto_update_interval_to);
    ASSERT_EQ(5, gw_cfg2.ruuvi_cfg.auto_update.auto_update_tz_offset_hours);

    ASSERT_EQ(true, gw_cfg2.ruuvi_cfg.ntp.ntp_use);
    ASSERT_EQ(true, gw_cfg2.ruuvi_cfg.ntp.ntp_use_dhcp);
    ASSERT_EQ(string("time.google.com"), string(gw_cfg2.ruuvi_cfg.ntp.ntp_server1.buf));
    ASSERT_EQ(string("time.cloudflare.com"), string(gw_cfg2.ruuvi_cfg.ntp.ntp_server2.buf));
    ASSERT_EQ(string("time.nist.gov"), string(gw_cfg2.ruuvi_cfg.ntp.ntp_server3.buf));
    ASSERT_EQ(string("pool.ntp.org"), string(gw_cfg2.ruuvi_cfg.ntp.ntp_server4.buf));

    ASSERT_EQ(RUUVI_COMPANY_ID, gw_cfg2.ruuvi_cfg.filter.company_id);
    ASSERT_EQ(false, gw_cfg2.ruuvi_cfg.filter.company_use_filtering);

    ASSERT_EQ(true, gw_cfg2.ruuvi_cfg.scan.scan_coded_phy);
    ASSERT_EQ(false, gw_cfg2.ruuvi_cfg.scan.scan_1mbit_phy);
    ASSERT_EQ(false, gw_cfg2.ruuvi_cfg.scan.scan_extended_payload);
    ASSERT_EQ(false, gw_cfg2.ruuvi_cfg.scan.scan_channel_37);
    ASSERT_EQ(false, gw_cfg2.ruuvi_cfg.scan.scan_channel_38);
    ASSERT_EQ(false, gw_cfg2.ruuvi_cfg.scan.scan_channel_39);
    ASSERT_EQ(false, gw_cfg2.ruuvi_cfg.scan_filter.scan_filter_allow_listed);
    ASSERT_EQ(0, gw_cfg2.ruuvi_cfg.scan_filter.scan_filter_length);

    ASSERT_EQ(string("coordinates1"), gw_cfg2.ruuvi_cfg.coordinates.buf);
    ASSERT_EQ(string("https://myserver1.com/fw_update_info.json"), gw_cfg2.ruuvi_cfg.fw_update.fw_update_url);

    ASSERT_TRUE(gw_cfg_json_generate_for_saving(&gw_cfg2, &json_str));
    ASSERT_EQ(
        string("{\n"
               "\t\"wifi_sta_config\":\t{\n"
               "\t\t\"ssid\":\t\"\",\n"
               "\t\t\"password\":\t\"\"\n"
               "\t},\n"
               "\t\"wifi_ap_config\":\t{\n"
               "\t\t\"password\":\t\"\",\n"
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
               "\t\"use_http\":\tfalse,\n"
               "\t\"use_http_stat\":\tfalse,\n"
               "\t\"http_stat_url\":\t\"https://myserver1.com/status\",\n"
               "\t\"http_stat_user\":\t\"h_user2\",\n"
               "\t\"http_stat_pass\":\t\"h_pass2\",\n"
               "\t\"http_stat_use_ssl_client_cert\":\tfalse,\n"
               "\t\"http_stat_use_ssl_server_cert\":\tfalse,\n"
               "\t\"use_mqtt\":\ttrue,\n"
               "\t\"mqtt_disable_retained_messages\":\tfalse,\n"
               "\t\"mqtt_transport\":\t\"SSL\",\n"
               "\t\"mqtt_server\":\t\"test.mosquitto.org\",\n"
               "\t\"mqtt_port\":\t1338,\n"
               "\t\"mqtt_sending_interval\":\t0,\n"
               "\t\"mqtt_prefix\":\t\"my_prefix\",\n"
               "\t\"mqtt_client_id\":\t\"my_client\",\n"
               "\t\"mqtt_user\":\t\"m_user1\",\n"
               "\t\"mqtt_pass\":\t\"m_pass1\",\n"
               "\t\"mqtt_use_ssl_client_cert\":\tfalse,\n"
               "\t\"mqtt_use_ssl_server_cert\":\tfalse,\n"
               "\t\"lan_auth_type\":\t\"lan_auth_digest\",\n"
               "\t\"lan_auth_user\":\t\"l_user1\",\n"
               "\t\"lan_auth_pass\":\t\"l_pass1\",\n"
               "\t\"lan_auth_api_key\":\t\"\",\n"
               "\t\"lan_auth_api_key_rw\":\t\"\",\n"
               "\t\"auto_update_cycle\":\t\"regular\",\n"
               "\t\"auto_update_weekdays_bitmask\":\t63,\n"
               "\t\"auto_update_interval_from\":\t2,\n"
               "\t\"auto_update_interval_to\":\t22,\n"
               "\t\"auto_update_tz_offset_hours\":\t5,\n"
               "\t\"ntp_use\":\ttrue,\n"
               "\t\"ntp_use_dhcp\":\ttrue,\n"
               "\t\"ntp_server1\":\t\"time.google.com\",\n"
               "\t\"ntp_server2\":\t\"time.cloudflare.com\",\n"
               "\t\"ntp_server3\":\t\"time.nist.gov\",\n"
               "\t\"ntp_server4\":\t\"pool.ntp.org\",\n"
               "\t\"company_id\":\t1177,\n"
               "\t\"company_use_filtering\":\tfalse,\n"
               "\t\"scan_coded_phy\":\ttrue,\n"
               "\t\"scan_1mbit_phy\":\tfalse,\n"
               "\t\"scan_extended_payload\":\tfalse,\n"
               "\t\"scan_channel_37\":\tfalse,\n"
               "\t\"scan_channel_38\":\tfalse,\n"
               "\t\"scan_channel_39\":\tfalse,\n"
               "\t\"scan_filter_allow_listed\":\tfalse,\n"
               "\t\"scan_filter_list\":\t[],\n"
               "\t\"coordinates\":\t\"coordinates1\",\n"
               "\t\"fw_update_url\":\t\"https://myserver1.com/fw_update_info.json\"\n"
               "}"),
        string(json_str.p_str));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    cjson_wrap_free_json_str(&json_str);
}

TEST_F(TestGwCfgJson, gw_cfg_json_generate_parse_generate_ntp_custom) // NOLINT
{
    const gw_cfg_t gw_cfg   = {
        .device_info = {
            .esp32_fw_ver = { "v1.10.0" },
            .nrf52_fw_ver = { "v0.7.2" },
            .nrf52_mac_addr = { "AA:BB:CC:DD:EE:FF" },
            .nrf52_device_id = { "11:22:33:44:55:66:77:88" },
        },
        .ruuvi_cfg = {
            .http = {
                .use_http_ruuvi = false,
                .use_http = false,
                .http_use_ssl_client_cert = false,
                .http_use_ssl_server_cert = false,
                .http_url = { "https://myserver1.com" },
                .data_format = GW_CFG_HTTP_DATA_FORMAT_RUUVI,
                .auth_type = GW_CFG_HTTP_AUTH_TYPE_BASIC,
                .auth = {
                    .auth_basic = {
                        .user = { "h_user1" },
                        .password = { "h_pass1" },
                    },
                },
            },
            .http_stat = {
                .use_http_stat = false,
                .http_stat_use_ssl_client_cert = false,
                .http_stat_use_ssl_server_cert = false,
                .http_stat_url = { "https://myserver1.com/status" },
                .http_stat_user = { "h_user2" },
                .http_stat_pass = { "h_pass2" },
            },
            .mqtt = {
                .use_mqtt = true,
                .mqtt_disable_retained_messages = false,
                .use_ssl_client_cert = false,
                .use_ssl_server_cert = false,
                .mqtt_transport = { "SSL" },
                .mqtt_server = { "test.mosquitto.org" },
                .mqtt_port = 1338,
                .mqtt_prefix = { "my_prefix" },
                .mqtt_client_id = { "my_client" },
                .mqtt_user = { "m_user1" },
                .mqtt_pass = { "m_pass1" },
            },
            .lan_auth = {
                HTTP_SERVER_AUTH_TYPE_DIGEST,
                "l_user1",
                "l_pass1",
                "",
            },
            .auto_update = {
                .auto_update_cycle = AUTO_UPDATE_CYCLE_TYPE_REGULAR,
                .auto_update_weekdays_bitmask = 0x3F,
                .auto_update_interval_from = 2,
                .auto_update_interval_to = 22,
                .auto_update_tz_offset_hours = 5,
            },
            .ntp = {
                .ntp_use = true,
                .ntp_use_dhcp = false,
                .ntp_server1 = { "time1.server.com", },
                .ntp_server2 = { "time2.server.com", },
                .ntp_server3 = { "time3.server.com", },
                .ntp_server4 = { "time4.server.com", },
            },
            .filter = {
                .company_id = RUUVI_COMPANY_ID,
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
                .scan_filter_list = { 0 },
            },
            .coordinates = { "coordinates1" },
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

    ASSERT_TRUE(gw_cfg_json_generate_for_saving(&gw_cfg, &json_str));
    ASSERT_NE(nullptr, json_str.p_str);

    gw_cfg_t gw_cfg2 = get_gateway_config_default();
    ASSERT_TRUE(gw_cfg_json_parse("my.json", nullptr, json_str.p_str, &gw_cfg2));
    cjson_wrap_free_json_str(&json_str);

    ASSERT_EQ(true, gw_cfg2.eth_cfg.use_eth);
    ASSERT_EQ(false, gw_cfg2.eth_cfg.eth_dhcp);
    ASSERT_EQ(string("192.168.1.10"), gw_cfg2.eth_cfg.eth_static_ip.buf);
    ASSERT_EQ(string("255.255.255.0"), gw_cfg2.eth_cfg.eth_netmask.buf);
    ASSERT_EQ(string("192.168.1.1"), gw_cfg2.eth_cfg.eth_gw.buf);
    ASSERT_EQ(string("8.8.8.8"), gw_cfg2.eth_cfg.eth_dns1.buf);
    ASSERT_EQ(string("4.4.4.4"), gw_cfg2.eth_cfg.eth_dns2.buf);

    ASSERT_EQ(true, gw_cfg2.ruuvi_cfg.mqtt.use_mqtt);
    ASSERT_EQ(false, gw_cfg2.ruuvi_cfg.mqtt.mqtt_disable_retained_messages);
    ASSERT_EQ(string("SSL"), gw_cfg2.ruuvi_cfg.mqtt.mqtt_transport.buf);
    ASSERT_EQ(string("test.mosquitto.org"), gw_cfg2.ruuvi_cfg.mqtt.mqtt_server.buf);
    ASSERT_EQ(1338, gw_cfg2.ruuvi_cfg.mqtt.mqtt_port);
    ASSERT_EQ(string("my_prefix"), gw_cfg2.ruuvi_cfg.mqtt.mqtt_prefix.buf);
    ASSERT_EQ(string("my_client"), gw_cfg2.ruuvi_cfg.mqtt.mqtt_client_id.buf);
    ASSERT_EQ(string("m_user1"), gw_cfg2.ruuvi_cfg.mqtt.mqtt_user.buf);
    ASSERT_EQ(string("m_pass1"), gw_cfg2.ruuvi_cfg.mqtt.mqtt_pass.buf);

    ASSERT_EQ(false, gw_cfg2.ruuvi_cfg.http.use_http_ruuvi);
    ASSERT_EQ(false, gw_cfg2.ruuvi_cfg.http.use_http);

    ASSERT_EQ(false, gw_cfg2.ruuvi_cfg.http_stat.use_http_stat);
    ASSERT_EQ(string("https://myserver1.com/status"), gw_cfg2.ruuvi_cfg.http_stat.http_stat_url.buf);
    ASSERT_EQ(string("h_user2"), gw_cfg2.ruuvi_cfg.http_stat.http_stat_user.buf);
    ASSERT_EQ(string("h_pass2"), gw_cfg2.ruuvi_cfg.http_stat.http_stat_pass.buf);

    ASSERT_EQ(HTTP_SERVER_AUTH_TYPE_DIGEST, gw_cfg2.ruuvi_cfg.lan_auth.lan_auth_type);
    ASSERT_EQ(string("l_user1"), gw_cfg2.ruuvi_cfg.lan_auth.lan_auth_user.buf);
    ASSERT_EQ(string("l_pass1"), gw_cfg2.ruuvi_cfg.lan_auth.lan_auth_pass.buf);
    ASSERT_EQ(string(""), gw_cfg2.ruuvi_cfg.lan_auth.lan_auth_api_key.buf);
    ASSERT_EQ(string(""), gw_cfg2.ruuvi_cfg.lan_auth.lan_auth_api_key_rw.buf);

    ASSERT_EQ(AUTO_UPDATE_CYCLE_TYPE_REGULAR, gw_cfg2.ruuvi_cfg.auto_update.auto_update_cycle);
    ASSERT_EQ(0x3F, gw_cfg2.ruuvi_cfg.auto_update.auto_update_weekdays_bitmask);
    ASSERT_EQ(2, gw_cfg2.ruuvi_cfg.auto_update.auto_update_interval_from);
    ASSERT_EQ(22, gw_cfg2.ruuvi_cfg.auto_update.auto_update_interval_to);
    ASSERT_EQ(5, gw_cfg2.ruuvi_cfg.auto_update.auto_update_tz_offset_hours);

    ASSERT_EQ(true, gw_cfg2.ruuvi_cfg.ntp.ntp_use);
    ASSERT_EQ(false, gw_cfg2.ruuvi_cfg.ntp.ntp_use_dhcp);
    ASSERT_EQ(string("time1.server.com"), string(gw_cfg2.ruuvi_cfg.ntp.ntp_server1.buf));
    ASSERT_EQ(string("time2.server.com"), string(gw_cfg2.ruuvi_cfg.ntp.ntp_server2.buf));
    ASSERT_EQ(string("time3.server.com"), string(gw_cfg2.ruuvi_cfg.ntp.ntp_server3.buf));
    ASSERT_EQ(string("time4.server.com"), string(gw_cfg2.ruuvi_cfg.ntp.ntp_server4.buf));

    ASSERT_EQ(RUUVI_COMPANY_ID, gw_cfg2.ruuvi_cfg.filter.company_id);
    ASSERT_EQ(false, gw_cfg2.ruuvi_cfg.filter.company_use_filtering);

    ASSERT_EQ(true, gw_cfg2.ruuvi_cfg.scan.scan_coded_phy);
    ASSERT_EQ(false, gw_cfg2.ruuvi_cfg.scan.scan_1mbit_phy);
    ASSERT_EQ(false, gw_cfg2.ruuvi_cfg.scan.scan_extended_payload);
    ASSERT_EQ(false, gw_cfg2.ruuvi_cfg.scan.scan_channel_37);
    ASSERT_EQ(false, gw_cfg2.ruuvi_cfg.scan.scan_channel_38);
    ASSERT_EQ(false, gw_cfg2.ruuvi_cfg.scan.scan_channel_39);
    ASSERT_EQ(false, gw_cfg2.ruuvi_cfg.scan_filter.scan_filter_allow_listed);
    ASSERT_EQ(0, gw_cfg2.ruuvi_cfg.scan_filter.scan_filter_length);

    ASSERT_EQ(string("coordinates1"), gw_cfg2.ruuvi_cfg.coordinates.buf);
    ASSERT_EQ(string("https://myserver1.com/fw_update_info.json"), gw_cfg2.ruuvi_cfg.fw_update.fw_update_url);

    ASSERT_TRUE(gw_cfg_json_generate_for_saving(&gw_cfg2, &json_str));
    ASSERT_EQ(
        string("{\n"
               "\t\"wifi_sta_config\":\t{\n"
               "\t\t\"ssid\":\t\"\",\n"
               "\t\t\"password\":\t\"\"\n"
               "\t},\n"
               "\t\"wifi_ap_config\":\t{\n"
               "\t\t\"password\":\t\"\",\n"
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
               "\t\"use_http\":\tfalse,\n"
               "\t\"use_http_stat\":\tfalse,\n"
               "\t\"http_stat_url\":\t\"https://myserver1.com/status\",\n"
               "\t\"http_stat_user\":\t\"h_user2\",\n"
               "\t\"http_stat_pass\":\t\"h_pass2\",\n"
               "\t\"http_stat_use_ssl_client_cert\":\tfalse,\n"
               "\t\"http_stat_use_ssl_server_cert\":\tfalse,\n"
               "\t\"use_mqtt\":\ttrue,\n"
               "\t\"mqtt_disable_retained_messages\":\tfalse,\n"
               "\t\"mqtt_transport\":\t\"SSL\",\n"
               "\t\"mqtt_server\":\t\"test.mosquitto.org\",\n"
               "\t\"mqtt_port\":\t1338,\n"
               "\t\"mqtt_sending_interval\":\t0,\n"
               "\t\"mqtt_prefix\":\t\"my_prefix\",\n"
               "\t\"mqtt_client_id\":\t\"my_client\",\n"
               "\t\"mqtt_user\":\t\"m_user1\",\n"
               "\t\"mqtt_pass\":\t\"m_pass1\",\n"
               "\t\"mqtt_use_ssl_client_cert\":\tfalse,\n"
               "\t\"mqtt_use_ssl_server_cert\":\tfalse,\n"
               "\t\"lan_auth_type\":\t\"lan_auth_digest\",\n"
               "\t\"lan_auth_user\":\t\"l_user1\",\n"
               "\t\"lan_auth_pass\":\t\"l_pass1\",\n"
               "\t\"lan_auth_api_key\":\t\"\",\n"
               "\t\"lan_auth_api_key_rw\":\t\"\",\n"
               "\t\"auto_update_cycle\":\t\"regular\",\n"
               "\t\"auto_update_weekdays_bitmask\":\t63,\n"
               "\t\"auto_update_interval_from\":\t2,\n"
               "\t\"auto_update_interval_to\":\t22,\n"
               "\t\"auto_update_tz_offset_hours\":\t5,\n"
               "\t\"ntp_use\":\ttrue,\n"
               "\t\"ntp_use_dhcp\":\tfalse,\n"
               "\t\"ntp_server1\":\t\"time1.server.com\",\n"
               "\t\"ntp_server2\":\t\"time2.server.com\",\n"
               "\t\"ntp_server3\":\t\"time3.server.com\",\n"
               "\t\"ntp_server4\":\t\"time4.server.com\",\n"
               "\t\"company_id\":\t1177,\n"
               "\t\"company_use_filtering\":\tfalse,\n"
               "\t\"scan_coded_phy\":\ttrue,\n"
               "\t\"scan_1mbit_phy\":\tfalse,\n"
               "\t\"scan_extended_payload\":\tfalse,\n"
               "\t\"scan_channel_37\":\tfalse,\n"
               "\t\"scan_channel_38\":\tfalse,\n"
               "\t\"scan_channel_39\":\tfalse,\n"
               "\t\"scan_filter_allow_listed\":\tfalse,\n"
               "\t\"scan_filter_list\":\t[],\n"
               "\t\"coordinates\":\t\"coordinates1\",\n"
               "\t\"fw_update_url\":\t\"https://myserver1.com/fw_update_info.json\"\n"
               "}"),
        string(json_str.p_str));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    cjson_wrap_free_json_str(&json_str);
}

TEST_F(TestGwCfgJson, gw_cfg_json_parse_lan_auth_ruuvi_conv_to_default) // NOLINT
{
    gw_cfg_t gw_cfg                         = get_gateway_config_default();
    gw_cfg.ruuvi_cfg.lan_auth.lan_auth_type = HTTP_SERVER_AUTH_TYPE_RUUVI;

    cjson_wrap_str_t json_str = cjson_wrap_str_null();

    ASSERT_TRUE(gw_cfg_json_generate_for_saving(&gw_cfg, &json_str));
    ASSERT_NE(nullptr, json_str.p_str);
    ASSERT_EQ(
        string("{\n"
               "\t\"wifi_sta_config\":\t{\n"
               "\t\t\"ssid\":\t\"\",\n"
               "\t\t\"password\":\t\"\"\n"
               "\t},\n"
               "\t\"wifi_ap_config\":\t{\n"
               "\t\t\"password\":\t\"\",\n"
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
               "\t\"http_stat_url\":\t\"https://network.ruuvi.com/status\",\n"
               "\t\"http_stat_user\":\t\"\",\n"
               "\t\"http_stat_pass\":\t\"\",\n"
               "\t\"http_stat_use_ssl_client_cert\":\tfalse,\n"
               "\t\"http_stat_use_ssl_server_cert\":\tfalse,\n"
               "\t\"use_mqtt\":\tfalse,\n"
               "\t\"mqtt_disable_retained_messages\":\tfalse,\n"
               "\t\"mqtt_transport\":\t\"TCP\",\n"
               "\t\"mqtt_server\":\t\"test.mosquitto.org\",\n"
               "\t\"mqtt_port\":\t1883,\n"
               "\t\"mqtt_sending_interval\":\t0,\n"
               "\t\"mqtt_prefix\":\t\"ruuvi/AA:BB:CC:DD:EE:FF/\",\n"
               "\t\"mqtt_client_id\":\t\"AA:BB:CC:DD:EE:FF\",\n"
               "\t\"mqtt_user\":\t\"\",\n"
               "\t\"mqtt_pass\":\t\"\",\n"
               "\t\"mqtt_use_ssl_client_cert\":\tfalse,\n"
               "\t\"mqtt_use_ssl_server_cert\":\tfalse,\n"
               "\t\"lan_auth_type\":\t\"lan_auth_default\",\n" // <--
               "\t\"lan_auth_user\":\t\"Admin\",\n"
               "\t\"lan_auth_pass\":\t\"0d6c6f1c27ca628806eb9247740d8ba1\",\n"
               "\t\"lan_auth_api_key\":\t\"\",\n"
               "\t\"lan_auth_api_key_rw\":\t\"\",\n"
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
               "\t\"coordinates\":\t\"\",\n"
               "\t\"fw_update_url\":\t\"https://network.ruuvi.com/firmwareupdate\"\n"
               "}"),
        json_str.p_str);
    cjson_wrap_free_json_str(&json_str);

    const char* const p_json_str
        = "{\n"
          "\t\"wifi_sta_config\":\t{\n"
          "\t\t\"ssid\":\t\"\",\n"
          "\t\t\"password\":\t\"\"\n"
          "\t},\n"
          "\t\"wifi_ap_config\":\t{\n"
          "\t\t\"password\":\t\"\",\n"
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
          "\t\"http_stat_pass\":\t\"\",\n"
          "\t\"http_stat_use_ssl_client_cert\":\tfalse,\n"
          "\t\"http_stat_use_ssl_server_cert\":\tfalse,\n"
          "\t\"use_mqtt\":\tfalse,\n"
          "\t\"mqtt_disable_retained_messages\":\tfalse,\n"
          "\t\"mqtt_transport\":\t\"TCP\",\n"
          "\t\"mqtt_server\":\t\"test.mosquitto.org\",\n"
          "\t\"mqtt_port\":\t1883,\n"
          "\t\"mqtt_sending_interval\":\t0,\n"
          "\t\"mqtt_prefix\":\t\"ruuvi/AA:BB:CC:DD:EE:FF/\",\n"
          "\t\"mqtt_client_id\":\t\"AA:BB:CC:DD:EE:FF\",\n"
          "\t\"mqtt_user\":\t\"\",\n"
          "\t\"mqtt_pass\":\t\"\",\n"
          "\t\"mqtt_use_ssl_client_cert\":\tfalse,\n"
          "\t\"mqtt_use_ssl_server_cert\":\tfalse,\n"
          "\t\"lan_auth_type\":\t\"lan_auth_ruuvi\",\n" // <--
          "\t\"lan_auth_user\":\t\"Admin\",\n"
          "\t\"lan_auth_pass\":\t\"0d6c6f1c27ca628806eb9247740d8ba1\",\n"
          "\t\"lan_auth_api_key\":\t\"\",\n"
          "\t\"lan_auth_api_key_rw\":\t\"\",\n"
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
          "\t\"coordinates\":\t\"\",\n"
          "\t\"fw_update_url\":\t\"https://network.ruuvi.com/firmwareupdate\"\n"
          "}";

    gw_cfg_t gw_cfg2 = get_gateway_config_default();
    ASSERT_TRUE(gw_cfg_json_parse("my.json", nullptr, p_json_str, &gw_cfg2));

    ASSERT_EQ(HTTP_SERVER_AUTH_TYPE_DEFAULT, gw_cfg2.ruuvi_cfg.lan_auth.lan_auth_type);
    ASSERT_EQ(string("Admin"), gw_cfg2.ruuvi_cfg.lan_auth.lan_auth_user.buf);
    ASSERT_EQ(string("0d6c6f1c27ca628806eb9247740d8ba1"), gw_cfg2.ruuvi_cfg.lan_auth.lan_auth_pass.buf);
    ASSERT_EQ(string(""), gw_cfg2.ruuvi_cfg.lan_auth.lan_auth_api_key.buf);
    ASSERT_EQ(string(""), gw_cfg2.ruuvi_cfg.lan_auth.lan_auth_api_key_rw.buf);

    ASSERT_TRUE(gw_cfg_json_generate_for_saving(&gw_cfg2, &json_str));
    ASSERT_EQ(
        string("{\n"
               "\t\"wifi_sta_config\":\t{\n"
               "\t\t\"ssid\":\t\"\",\n"
               "\t\t\"password\":\t\"\"\n"
               "\t},\n"
               "\t\"wifi_ap_config\":\t{\n"
               "\t\t\"password\":\t\"\",\n"
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
               "\t\"http_stat_pass\":\t\"\",\n"
               "\t\"http_stat_use_ssl_client_cert\":\tfalse,\n"
               "\t\"http_stat_use_ssl_server_cert\":\tfalse,\n"
               "\t\"use_mqtt\":\tfalse,\n"
               "\t\"mqtt_disable_retained_messages\":\tfalse,\n"
               "\t\"mqtt_transport\":\t\"TCP\",\n"
               "\t\"mqtt_server\":\t\"test.mosquitto.org\",\n"
               "\t\"mqtt_port\":\t1883,\n"
               "\t\"mqtt_sending_interval\":\t0,\n"
               "\t\"mqtt_prefix\":\t\"ruuvi/AA:BB:CC:DD:EE:FF/\",\n"
               "\t\"mqtt_client_id\":\t\"AA:BB:CC:DD:EE:FF\",\n"
               "\t\"mqtt_user\":\t\"\",\n"
               "\t\"mqtt_pass\":\t\"\",\n"
               "\t\"mqtt_use_ssl_client_cert\":\tfalse,\n"
               "\t\"mqtt_use_ssl_server_cert\":\tfalse,\n"
               "\t\"lan_auth_type\":\t\"lan_auth_default\",\n"
               "\t\"lan_auth_user\":\t\"Admin\",\n"
               "\t\"lan_auth_api_key\":\t\"\",\n"
               "\t\"lan_auth_api_key_rw\":\t\"\",\n"
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
               "\t\"coordinates\":\t\"\",\n"
               "\t\"fw_update_url\":\t\"https://network.ruuvi.com/firmwareupdate\"\n"
               "}"),
        string(json_str.p_str));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    cjson_wrap_free_json_str(&json_str);
}

TEST_F(TestGwCfgJson, gw_cfg_json_parse_lan_auth_ruuvi) // NOLINT
{
    gw_cfg_t gw_cfg                         = get_gateway_config_default();
    gw_cfg.ruuvi_cfg.lan_auth.lan_auth_type = HTTP_SERVER_AUTH_TYPE_RUUVI;
    snprintf(
        gw_cfg.ruuvi_cfg.lan_auth.lan_auth_pass.buf,
        sizeof(gw_cfg.ruuvi_cfg.lan_auth.lan_auth_pass.buf),
        "non_default_pass");

    cjson_wrap_str_t json_str = cjson_wrap_str_null();

    ASSERT_TRUE(gw_cfg_json_generate_for_saving(&gw_cfg, &json_str));
    ASSERT_NE(nullptr, json_str.p_str);
    ASSERT_EQ(
        string("{\n"
               "\t\"wifi_sta_config\":\t{\n"
               "\t\t\"ssid\":\t\"\",\n"
               "\t\t\"password\":\t\"\"\n"
               "\t},\n"
               "\t\"wifi_ap_config\":\t{\n"
               "\t\t\"password\":\t\"\",\n"
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
               "\t\"http_stat_pass\":\t\"\",\n"
               "\t\"http_stat_use_ssl_client_cert\":\tfalse,\n"
               "\t\"http_stat_use_ssl_server_cert\":\tfalse,\n"
               "\t\"use_mqtt\":\tfalse,\n"
               "\t\"mqtt_disable_retained_messages\":\tfalse,\n"
               "\t\"mqtt_transport\":\t\"TCP\",\n"
               "\t\"mqtt_server\":\t\"test.mosquitto.org\",\n"
               "\t\"mqtt_port\":\t1883,\n"
               "\t\"mqtt_sending_interval\":\t0,\n"
               "\t\"mqtt_prefix\":\t\"ruuvi/AA:BB:CC:DD:EE:FF/\",\n"
               "\t\"mqtt_client_id\":\t\"AA:BB:CC:DD:EE:FF\",\n"
               "\t\"mqtt_user\":\t\"\",\n"
               "\t\"mqtt_pass\":\t\"\",\n"
               "\t\"mqtt_use_ssl_client_cert\":\tfalse,\n"
               "\t\"mqtt_use_ssl_server_cert\":\tfalse,\n"
               "\t\"lan_auth_type\":\t\"lan_auth_ruuvi\",\n"
               "\t\"lan_auth_user\":\t\"Admin\",\n"
               "\t\"lan_auth_pass\":\t\"non_default_pass\",\n"
               "\t\"lan_auth_api_key\":\t\"\",\n"
               "\t\"lan_auth_api_key_rw\":\t\"\",\n"
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
               "\t\"coordinates\":\t\"\",\n"
               "\t\"fw_update_url\":\t\"https://network.ruuvi.com/firmwareupdate\"\n"
               "}"),
        json_str.p_str);

    gw_cfg_t gw_cfg2 = get_gateway_config_default();
    ASSERT_TRUE(gw_cfg_json_parse("my.json", nullptr, json_str.p_str, &gw_cfg2));
    cjson_wrap_free_json_str(&json_str);

    ASSERT_EQ(HTTP_SERVER_AUTH_TYPE_RUUVI, gw_cfg2.ruuvi_cfg.lan_auth.lan_auth_type);
    ASSERT_EQ(string("Admin"), gw_cfg2.ruuvi_cfg.lan_auth.lan_auth_user.buf);
    ASSERT_EQ(string("non_default_pass"), gw_cfg2.ruuvi_cfg.lan_auth.lan_auth_pass.buf);
    ASSERT_EQ(string(""), gw_cfg2.ruuvi_cfg.lan_auth.lan_auth_api_key.buf);
    ASSERT_EQ(string(""), gw_cfg2.ruuvi_cfg.lan_auth.lan_auth_api_key_rw.buf);

    ASSERT_TRUE(gw_cfg_json_generate_for_saving(&gw_cfg2, &json_str));
    ASSERT_EQ(
        string("{\n"
               "\t\"wifi_sta_config\":\t{\n"
               "\t\t\"ssid\":\t\"\",\n"
               "\t\t\"password\":\t\"\"\n"
               "\t},\n"
               "\t\"wifi_ap_config\":\t{\n"
               "\t\t\"password\":\t\"\",\n"
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
               "\t\"http_stat_pass\":\t\"\",\n"
               "\t\"http_stat_use_ssl_client_cert\":\tfalse,\n"
               "\t\"http_stat_use_ssl_server_cert\":\tfalse,\n"
               "\t\"use_mqtt\":\tfalse,\n"
               "\t\"mqtt_disable_retained_messages\":\tfalse,\n"
               "\t\"mqtt_transport\":\t\"TCP\",\n"
               "\t\"mqtt_server\":\t\"test.mosquitto.org\",\n"
               "\t\"mqtt_port\":\t1883,\n"
               "\t\"mqtt_sending_interval\":\t0,\n"
               "\t\"mqtt_prefix\":\t\"ruuvi/AA:BB:CC:DD:EE:FF/\",\n"
               "\t\"mqtt_client_id\":\t\"AA:BB:CC:DD:EE:FF\",\n"
               "\t\"mqtt_user\":\t\"\",\n"
               "\t\"mqtt_pass\":\t\"\",\n"
               "\t\"mqtt_use_ssl_client_cert\":\tfalse,\n"
               "\t\"mqtt_use_ssl_server_cert\":\tfalse,\n"
               "\t\"lan_auth_type\":\t\"lan_auth_ruuvi\",\n"
               "\t\"lan_auth_user\":\t\"Admin\",\n"
               "\t\"lan_auth_pass\":\t\"non_default_pass\",\n"
               "\t\"lan_auth_api_key\":\t\"\",\n"
               "\t\"lan_auth_api_key_rw\":\t\"\",\n"
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
               "\t\"coordinates\":\t\"\",\n"
               "\t\"fw_update_url\":\t\"https://network.ruuvi.com/firmwareupdate\"\n"
               "}"),
        string(json_str.p_str));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    cjson_wrap_free_json_str(&json_str);
}

TEST_F(TestGwCfgJson, gw_cfg_json_parse_empty_json) // NOLINT
{
    cjson_wrap_str_t json_str = cjson_wrap_str_null();

    gw_cfg_t gw_cfg2 = get_gateway_config_default();
    ASSERT_TRUE(gw_cfg_json_parse("my.json", nullptr, "{}", &gw_cfg2));

    ASSERT_EQ(true, gw_cfg2.eth_cfg.use_eth);
    ASSERT_EQ(true, gw_cfg2.eth_cfg.eth_dhcp);
    ASSERT_EQ(string(""), gw_cfg2.eth_cfg.eth_static_ip.buf);
    ASSERT_EQ(string(""), gw_cfg2.eth_cfg.eth_netmask.buf);
    ASSERT_EQ(string(""), gw_cfg2.eth_cfg.eth_gw.buf);
    ASSERT_EQ(string(""), gw_cfg2.eth_cfg.eth_dns1.buf);
    ASSERT_EQ(string(""), gw_cfg2.eth_cfg.eth_dns2.buf);

    ASSERT_EQ(false, gw_cfg2.ruuvi_cfg.mqtt.use_mqtt);
    ASSERT_EQ(false, gw_cfg2.ruuvi_cfg.mqtt.mqtt_disable_retained_messages);
    ASSERT_EQ(string("TCP"), gw_cfg2.ruuvi_cfg.mqtt.mqtt_transport.buf);
    ASSERT_EQ(string("test.mosquitto.org"), gw_cfg2.ruuvi_cfg.mqtt.mqtt_server.buf);
    ASSERT_EQ(1883, gw_cfg2.ruuvi_cfg.mqtt.mqtt_port);
    ASSERT_EQ(string("ruuvi/AA:BB:CC:DD:EE:FF/"), gw_cfg2.ruuvi_cfg.mqtt.mqtt_prefix.buf);
    ASSERT_EQ(string("AA:BB:CC:DD:EE:FF"), gw_cfg2.ruuvi_cfg.mqtt.mqtt_client_id.buf);
    ASSERT_EQ(string(""), gw_cfg2.ruuvi_cfg.mqtt.mqtt_user.buf);
    ASSERT_EQ(string(""), gw_cfg2.ruuvi_cfg.mqtt.mqtt_pass.buf);

    ASSERT_EQ(true, gw_cfg2.ruuvi_cfg.http.use_http_ruuvi);
    ASSERT_EQ(false, gw_cfg2.ruuvi_cfg.http.use_http);
    ASSERT_EQ(string(RUUVI_GATEWAY_HTTP_DEFAULT_URL), gw_cfg2.ruuvi_cfg.http.http_url.buf);
    ASSERT_EQ(GW_CFG_HTTP_DATA_FORMAT_RUUVI, gw_cfg2.ruuvi_cfg.http.data_format);
    ASSERT_EQ(GW_CFG_HTTP_AUTH_TYPE_NONE, gw_cfg2.ruuvi_cfg.http.auth_type);

    ASSERT_EQ(true, gw_cfg2.ruuvi_cfg.http_stat.use_http_stat);
    ASSERT_EQ(string(RUUVI_GATEWAY_HTTP_STATUS_URL), gw_cfg2.ruuvi_cfg.http_stat.http_stat_url.buf);
    ASSERT_EQ(string(""), gw_cfg2.ruuvi_cfg.http_stat.http_stat_user.buf);
    ASSERT_EQ(string(""), gw_cfg2.ruuvi_cfg.http_stat.http_stat_pass.buf);

    ASSERT_EQ(HTTP_SERVER_AUTH_TYPE_DEFAULT, gw_cfg2.ruuvi_cfg.lan_auth.lan_auth_type);
    ASSERT_EQ(string(RUUVI_GATEWAY_AUTH_DEFAULT_USER), gw_cfg2.ruuvi_cfg.lan_auth.lan_auth_user.buf);
    ASSERT_EQ(string("0d6c6f1c27ca628806eb9247740d8ba1"), gw_cfg2.ruuvi_cfg.lan_auth.lan_auth_pass.buf);
    ASSERT_EQ(string(""), gw_cfg2.ruuvi_cfg.lan_auth.lan_auth_api_key.buf);
    ASSERT_EQ(string(""), gw_cfg2.ruuvi_cfg.lan_auth.lan_auth_api_key_rw.buf);

    ASSERT_EQ(AUTO_UPDATE_CYCLE_TYPE_REGULAR, gw_cfg2.ruuvi_cfg.auto_update.auto_update_cycle);
    ASSERT_EQ(0x7F, gw_cfg2.ruuvi_cfg.auto_update.auto_update_weekdays_bitmask);
    ASSERT_EQ(0, gw_cfg2.ruuvi_cfg.auto_update.auto_update_interval_from);
    ASSERT_EQ(24, gw_cfg2.ruuvi_cfg.auto_update.auto_update_interval_to);
    ASSERT_EQ(3, gw_cfg2.ruuvi_cfg.auto_update.auto_update_tz_offset_hours);

    ASSERT_EQ(true, gw_cfg2.ruuvi_cfg.ntp.ntp_use);
    ASSERT_EQ(false, gw_cfg2.ruuvi_cfg.ntp.ntp_use_dhcp);
    ASSERT_EQ(string("time.google.com"), string(gw_cfg2.ruuvi_cfg.ntp.ntp_server1.buf));
    ASSERT_EQ(string("time.cloudflare.com"), string(gw_cfg2.ruuvi_cfg.ntp.ntp_server2.buf));
    ASSERT_EQ(string("time.nist.gov"), string(gw_cfg2.ruuvi_cfg.ntp.ntp_server3.buf));
    ASSERT_EQ(string("pool.ntp.org"), string(gw_cfg2.ruuvi_cfg.ntp.ntp_server4.buf));

    ASSERT_EQ(RUUVI_COMPANY_ID, gw_cfg2.ruuvi_cfg.filter.company_id);
    ASSERT_EQ(true, gw_cfg2.ruuvi_cfg.filter.company_use_filtering);

    ASSERT_EQ(false, gw_cfg2.ruuvi_cfg.scan.scan_coded_phy);
    ASSERT_EQ(true, gw_cfg2.ruuvi_cfg.scan.scan_1mbit_phy);
    ASSERT_EQ(true, gw_cfg2.ruuvi_cfg.scan.scan_extended_payload);
    ASSERT_EQ(true, gw_cfg2.ruuvi_cfg.scan.scan_channel_37);
    ASSERT_EQ(true, gw_cfg2.ruuvi_cfg.scan.scan_channel_38);
    ASSERT_EQ(true, gw_cfg2.ruuvi_cfg.scan.scan_channel_39);
    ASSERT_EQ(false, gw_cfg2.ruuvi_cfg.scan_filter.scan_filter_allow_listed);
    ASSERT_EQ(0, gw_cfg2.ruuvi_cfg.scan_filter.scan_filter_length);

    ASSERT_EQ(string(""), gw_cfg2.ruuvi_cfg.coordinates.buf);

    gw_cfg_t gateway_config_default {};
    gw_cfg_default_get(&gateway_config_default);
    ASSERT_TRUE(0 == memcmp(&gateway_config_default, &gw_cfg2, sizeof(gateway_config_default)));

    ASSERT_TRUE(gw_cfg_json_generate_for_saving(&gw_cfg2, &json_str));
    ASSERT_EQ(
        string("{\n"
               "\t\"wifi_sta_config\":\t{\n"
               "\t\t\"ssid\":\t\"\",\n"
               "\t\t\"password\":\t\"\"\n"
               "\t},\n"
               "\t\"wifi_ap_config\":\t{\n"
               "\t\t\"password\":\t\"\",\n"
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
               "\t\"http_stat_pass\":\t\"\",\n"
               "\t\"http_stat_use_ssl_client_cert\":\tfalse,\n"
               "\t\"http_stat_use_ssl_server_cert\":\tfalse,\n"
               "\t\"use_mqtt\":\tfalse,\n"
               "\t\"mqtt_disable_retained_messages\":\tfalse,\n"
               "\t\"mqtt_transport\":\t\"TCP\",\n"
               "\t\"mqtt_server\":\t\"test.mosquitto.org\",\n"
               "\t\"mqtt_port\":\t1883,\n"
               "\t\"mqtt_sending_interval\":\t0,\n"
               "\t\"mqtt_prefix\":\t\"ruuvi/AA:BB:CC:DD:EE:FF/\",\n"
               "\t\"mqtt_client_id\":\t\"AA:BB:CC:DD:EE:FF\",\n"
               "\t\"mqtt_user\":\t\"\",\n"
               "\t\"mqtt_pass\":\t\"\",\n"
               "\t\"mqtt_use_ssl_client_cert\":\tfalse,\n"
               "\t\"mqtt_use_ssl_server_cert\":\tfalse,\n"
               "\t\"lan_auth_type\":\t\"lan_auth_default\",\n"
               "\t\"lan_auth_user\":\t\"Admin\",\n"
               "\t\"lan_auth_api_key\":\t\"\",\n"
               "\t\"lan_auth_api_key_rw\":\t\"\",\n"
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
               "\t\"coordinates\":\t\"\",\n"
               "\t\"fw_update_url\":\t\"https://network.ruuvi.com/firmwareupdate\"\n"
               "}"),
        string(json_str.p_str));
    TEST_CHECK_LOG_RECORD(ESP_LOG_WARN, string("Can't find key 'remote_cfg_use' in config-json"));
    TEST_CHECK_LOG_RECORD(ESP_LOG_WARN, string("Can't find key 'remote_cfg_url' in config-json"));
    TEST_CHECK_LOG_RECORD(ESP_LOG_WARN, string("Can't find key 'remote_cfg_auth_type' in config-json"));
    TEST_CHECK_LOG_RECORD(ESP_LOG_WARN, string("Can't find key 'remote_cfg_use_ssl_client_cert' in config-json"));
    TEST_CHECK_LOG_RECORD(ESP_LOG_WARN, string("Can't find key 'remote_cfg_use_ssl_server_cert' in config-json"));
    TEST_CHECK_LOG_RECORD(ESP_LOG_WARN, string("Can't find key 'remote_cfg_refresh_interval_minutes' in config-json"));
    TEST_CHECK_LOG_RECORD(ESP_LOG_WARN, string("Can't find key 'use_http_ruuvi' in config-json"));
    TEST_CHECK_LOG_RECORD(ESP_LOG_WARN, string("Can't find key 'use_http' in config-json"));
    TEST_CHECK_LOG_RECORD(ESP_LOG_WARN, string("Can't find key 'use_http_stat' in config-json"));
    TEST_CHECK_LOG_RECORD(ESP_LOG_WARN, string("Can't find key 'http_stat_url' in config-json"));
    TEST_CHECK_LOG_RECORD(ESP_LOG_WARN, string("Can't find key 'http_stat_user' in config-json"));
    TEST_CHECK_LOG_RECORD(
        ESP_LOG_INFO,
        string("Can't find key 'http_stat_pass' in config-json, leave the previous value unchanged"));
    TEST_CHECK_LOG_RECORD(ESP_LOG_WARN, string("Can't find key 'http_stat_use_ssl_client_cert' in config-json"));
    TEST_CHECK_LOG_RECORD(ESP_LOG_WARN, string("Can't find key 'http_stat_use_ssl_server_cert' in config-json"));
    TEST_CHECK_LOG_RECORD(ESP_LOG_WARN, string("Can't find key 'use_mqtt' in config-json"));
    TEST_CHECK_LOG_RECORD(ESP_LOG_WARN, string("Can't find key 'mqtt_disable_retained_messages' in config-json"));
    TEST_CHECK_LOG_RECORD(ESP_LOG_WARN, string("Can't find key 'mqtt_transport' in config-json"));
    TEST_CHECK_LOG_RECORD(ESP_LOG_WARN, string("Can't find key 'mqtt_server' in config-json"));
    TEST_CHECK_LOG_RECORD(ESP_LOG_WARN, string("Can't find key 'mqtt_port' in config-json"));
    TEST_CHECK_LOG_RECORD(ESP_LOG_WARN, string("Can't find key 'mqtt_sending_interval' in config-json"));
    TEST_CHECK_LOG_RECORD(
        ESP_LOG_WARN,
        string("Can't find key 'mqtt_prefix' in config-json, use default value: ruuvi/AA:BB:CC:DD:EE:FF/"));
    TEST_CHECK_LOG_RECORD(
        ESP_LOG_WARN,
        string("Can't find key 'mqtt_client_id' in config-json, use default value: AA:BB:CC:DD:EE:FF"));
    TEST_CHECK_LOG_RECORD(ESP_LOG_WARN, string("Can't find key 'mqtt_user' in config-json"));
    TEST_CHECK_LOG_RECORD(
        ESP_LOG_INFO,
        string("Can't find key 'mqtt_pass' in config-json, leave the previous value unchanged"));
    TEST_CHECK_LOG_RECORD(
        ESP_LOG_INFO,
        string("Can't find key 'lan_auth_type' in config-json, leave the previous value unchanged"));
    TEST_CHECK_LOG_RECORD(
        ESP_LOG_INFO,
        string("Can't find key 'lan_auth_api_key' in config-json, leave the previous value unchanged"));
    TEST_CHECK_LOG_RECORD(
        ESP_LOG_INFO,
        string("Can't find key 'lan_auth_api_key_rw' in config-json, leave the previous value unchanged"));
    TEST_CHECK_LOG_RECORD(
        ESP_LOG_WARN,
        string("Can't find key 'auto_update_cycle' in config-json, leave the previous value unchanged"));
    TEST_CHECK_LOG_RECORD(ESP_LOG_WARN, string("Can't find key 'auto_update_weekdays_bitmask' in config-json"));
    TEST_CHECK_LOG_RECORD(ESP_LOG_WARN, string("Can't find key 'auto_update_interval_from' in config-json"));
    TEST_CHECK_LOG_RECORD(ESP_LOG_WARN, string("Can't find key 'auto_update_interval_to' in config-json"));
    TEST_CHECK_LOG_RECORD(ESP_LOG_WARN, string("Can't find key 'auto_update_tz_offset_hours' in config-json"));
    TEST_CHECK_LOG_RECORD(ESP_LOG_WARN, string("Can't find key 'ntp_use' in config-json"));
    TEST_CHECK_LOG_RECORD(ESP_LOG_WARN, string("Can't find key 'ntp_use_dhcp' in config-json"));
    TEST_CHECK_LOG_RECORD(ESP_LOG_WARN, string("Can't find key 'ntp_server1' in config-json"));
    TEST_CHECK_LOG_RECORD(ESP_LOG_WARN, string("Can't find key 'ntp_server2' in config-json"));
    TEST_CHECK_LOG_RECORD(ESP_LOG_WARN, string("Can't find key 'ntp_server3' in config-json"));
    TEST_CHECK_LOG_RECORD(ESP_LOG_WARN, string("Can't find key 'ntp_server4' in config-json"));
    TEST_CHECK_LOG_RECORD(ESP_LOG_WARN, string("Can't find key 'company_id' in config-json"));
    TEST_CHECK_LOG_RECORD(ESP_LOG_WARN, string("Can't find key 'company_use_filtering' in config-json"));
    TEST_CHECK_LOG_RECORD(ESP_LOG_WARN, string("Can't find key 'scan_coded_phy' in config-json"));
    TEST_CHECK_LOG_RECORD(ESP_LOG_WARN, string("Can't find key 'scan_1mbit_phy' in config-json"));
    TEST_CHECK_LOG_RECORD(ESP_LOG_WARN, string("Can't find key 'scan_extended_payload' in config-json"));
    TEST_CHECK_LOG_RECORD(ESP_LOG_WARN, string("Can't find key 'scan_channel_37' in config-json"));
    TEST_CHECK_LOG_RECORD(ESP_LOG_WARN, string("Can't find key 'scan_channel_38' in config-json"));
    TEST_CHECK_LOG_RECORD(ESP_LOG_WARN, string("Can't find key 'scan_channel_39' in config-json"));
    TEST_CHECK_LOG_RECORD(ESP_LOG_WARN, string("Can't find key 'scan_filter_allow_listed' in config-json"));
    TEST_CHECK_LOG_RECORD(ESP_LOG_WARN, string("Can't find key 'scan_filter_list' in config-json"));
    TEST_CHECK_LOG_RECORD(ESP_LOG_WARN, string("Can't find key 'coordinates' in config-json"));
    TEST_CHECK_LOG_RECORD(ESP_LOG_WARN, string("Can't find key 'fw_update_url' in config-json"));
    TEST_CHECK_LOG_RECORD(ESP_LOG_WARN, string("Can't find key 'use_eth' in config-json"));
    TEST_CHECK_LOG_RECORD(ESP_LOG_WARN, string("Can't find key 'eth_dhcp' in config-json"));
    TEST_CHECK_LOG_RECORD(ESP_LOG_WARN, string("Can't find key 'wifi_ap_config' in config-json"));
    TEST_CHECK_LOG_RECORD(ESP_LOG_WARN, string("Can't find key 'wifi_sta_config' in config-json"));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    cjson_wrap_free_json_str(&json_str);
}

TEST_F(TestGwCfgJson, gw_cfg_json_parse_empty_string) // NOLINT
{
    cjson_wrap_str_t json_str = cjson_wrap_str_null();

    gw_cfg_t gw_cfg2 = get_gateway_config_default();
    ASSERT_FALSE(gw_cfg_json_parse("my.json", nullptr, "", &gw_cfg2));

    ASSERT_EQ(true, gw_cfg2.eth_cfg.use_eth);
    ASSERT_EQ(true, gw_cfg2.eth_cfg.eth_dhcp);
    ASSERT_EQ(string(""), gw_cfg2.eth_cfg.eth_static_ip.buf);
    ASSERT_EQ(string(""), gw_cfg2.eth_cfg.eth_netmask.buf);
    ASSERT_EQ(string(""), gw_cfg2.eth_cfg.eth_gw.buf);
    ASSERT_EQ(string(""), gw_cfg2.eth_cfg.eth_dns1.buf);
    ASSERT_EQ(string(""), gw_cfg2.eth_cfg.eth_dns2.buf);

    ASSERT_EQ(false, gw_cfg2.ruuvi_cfg.mqtt.use_mqtt);
    ASSERT_EQ(false, gw_cfg2.ruuvi_cfg.mqtt.mqtt_disable_retained_messages);
    ASSERT_EQ(string("TCP"), gw_cfg2.ruuvi_cfg.mqtt.mqtt_transport.buf);
    ASSERT_EQ(string("test.mosquitto.org"), gw_cfg2.ruuvi_cfg.mqtt.mqtt_server.buf);
    ASSERT_EQ(1883, gw_cfg2.ruuvi_cfg.mqtt.mqtt_port);
    ASSERT_EQ(string("ruuvi/AA:BB:CC:DD:EE:FF/"), gw_cfg2.ruuvi_cfg.mqtt.mqtt_prefix.buf);
    ASSERT_EQ(string("AA:BB:CC:DD:EE:FF"), gw_cfg2.ruuvi_cfg.mqtt.mqtt_client_id.buf);
    ASSERT_EQ(string(""), gw_cfg2.ruuvi_cfg.mqtt.mqtt_user.buf);
    ASSERT_EQ(string(""), gw_cfg2.ruuvi_cfg.mqtt.mqtt_pass.buf);

    ASSERT_EQ(true, gw_cfg2.ruuvi_cfg.http.use_http_ruuvi);
    ASSERT_EQ(false, gw_cfg2.ruuvi_cfg.http.use_http);
    ASSERT_EQ(string(RUUVI_GATEWAY_HTTP_DEFAULT_URL), gw_cfg2.ruuvi_cfg.http.http_url.buf);

    ASSERT_EQ(true, gw_cfg2.ruuvi_cfg.http_stat.use_http_stat);
    ASSERT_EQ(string(RUUVI_GATEWAY_HTTP_STATUS_URL), gw_cfg2.ruuvi_cfg.http_stat.http_stat_url.buf);
    ASSERT_EQ(string(""), gw_cfg2.ruuvi_cfg.http_stat.http_stat_user.buf);
    ASSERT_EQ(string(""), gw_cfg2.ruuvi_cfg.http_stat.http_stat_pass.buf);

    ASSERT_EQ(HTTP_SERVER_AUTH_TYPE_DEFAULT, gw_cfg2.ruuvi_cfg.lan_auth.lan_auth_type);
    ASSERT_EQ(string(RUUVI_GATEWAY_AUTH_DEFAULT_USER), gw_cfg2.ruuvi_cfg.lan_auth.lan_auth_user.buf);
    ASSERT_EQ(string("0d6c6f1c27ca628806eb9247740d8ba1"), gw_cfg2.ruuvi_cfg.lan_auth.lan_auth_pass.buf);
    ASSERT_EQ(string(""), gw_cfg2.ruuvi_cfg.lan_auth.lan_auth_api_key.buf);
    ASSERT_EQ(string(""), gw_cfg2.ruuvi_cfg.lan_auth.lan_auth_api_key_rw.buf);

    ASSERT_EQ(AUTO_UPDATE_CYCLE_TYPE_REGULAR, gw_cfg2.ruuvi_cfg.auto_update.auto_update_cycle);
    ASSERT_EQ(0x7F, gw_cfg2.ruuvi_cfg.auto_update.auto_update_weekdays_bitmask);
    ASSERT_EQ(0, gw_cfg2.ruuvi_cfg.auto_update.auto_update_interval_from);
    ASSERT_EQ(24, gw_cfg2.ruuvi_cfg.auto_update.auto_update_interval_to);
    ASSERT_EQ(3, gw_cfg2.ruuvi_cfg.auto_update.auto_update_tz_offset_hours);

    ASSERT_EQ(true, gw_cfg2.ruuvi_cfg.ntp.ntp_use);
    ASSERT_EQ(false, gw_cfg2.ruuvi_cfg.ntp.ntp_use_dhcp);
    ASSERT_EQ(string("time.google.com"), string(gw_cfg2.ruuvi_cfg.ntp.ntp_server1.buf));
    ASSERT_EQ(string("time.cloudflare.com"), string(gw_cfg2.ruuvi_cfg.ntp.ntp_server2.buf));
    ASSERT_EQ(string("time.nist.gov"), string(gw_cfg2.ruuvi_cfg.ntp.ntp_server3.buf));
    ASSERT_EQ(string("pool.ntp.org"), string(gw_cfg2.ruuvi_cfg.ntp.ntp_server4.buf));

    ASSERT_EQ(RUUVI_COMPANY_ID, gw_cfg2.ruuvi_cfg.filter.company_id);
    ASSERT_EQ(true, gw_cfg2.ruuvi_cfg.filter.company_use_filtering);

    ASSERT_EQ(false, gw_cfg2.ruuvi_cfg.scan.scan_coded_phy);
    ASSERT_EQ(true, gw_cfg2.ruuvi_cfg.scan.scan_1mbit_phy);
    ASSERT_EQ(true, gw_cfg2.ruuvi_cfg.scan.scan_extended_payload);
    ASSERT_EQ(true, gw_cfg2.ruuvi_cfg.scan.scan_channel_37);
    ASSERT_EQ(true, gw_cfg2.ruuvi_cfg.scan.scan_channel_38);
    ASSERT_EQ(true, gw_cfg2.ruuvi_cfg.scan.scan_channel_39);
    ASSERT_EQ(false, gw_cfg2.ruuvi_cfg.scan_filter.scan_filter_allow_listed);
    ASSERT_EQ(0, gw_cfg2.ruuvi_cfg.scan_filter.scan_filter_length);

    ASSERT_EQ(string(""), gw_cfg2.ruuvi_cfg.coordinates.buf);

    gw_cfg_t gateway_config_default {};
    gw_cfg_default_get(&gateway_config_default);
    ASSERT_TRUE(0 == memcmp(&gateway_config_default, &gw_cfg2, sizeof(gateway_config_default)));

    ASSERT_TRUE(gw_cfg_json_generate_for_saving(&gw_cfg2, &json_str));
    ASSERT_EQ(
        string("{\n"
               "\t\"wifi_sta_config\":\t{\n"
               "\t\t\"ssid\":\t\"\",\n"
               "\t\t\"password\":\t\"\"\n"
               "\t},\n"
               "\t\"wifi_ap_config\":\t{\n"
               "\t\t\"password\":\t\"\",\n"
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
               "\t\"http_stat_pass\":\t\"\",\n"
               "\t\"http_stat_use_ssl_client_cert\":\tfalse,\n"
               "\t\"http_stat_use_ssl_server_cert\":\tfalse,\n"
               "\t\"use_mqtt\":\tfalse,\n"
               "\t\"mqtt_disable_retained_messages\":\tfalse,\n"
               "\t\"mqtt_transport\":\t\"TCP\",\n"
               "\t\"mqtt_server\":\t\"test.mosquitto.org\",\n"
               "\t\"mqtt_port\":\t1883,\n"
               "\t\"mqtt_sending_interval\":\t0,\n"
               "\t\"mqtt_prefix\":\t\"ruuvi/AA:BB:CC:DD:EE:FF/\",\n"
               "\t\"mqtt_client_id\":\t\"AA:BB:CC:DD:EE:FF\",\n"
               "\t\"mqtt_user\":\t\"\",\n"
               "\t\"mqtt_pass\":\t\"\",\n"
               "\t\"mqtt_use_ssl_client_cert\":\tfalse,\n"
               "\t\"mqtt_use_ssl_server_cert\":\tfalse,\n"
               "\t\"lan_auth_type\":\t\"lan_auth_default\",\n"
               "\t\"lan_auth_user\":\t\"Admin\",\n"
               "\t\"lan_auth_api_key\":\t\"\",\n"
               "\t\"lan_auth_api_key_rw\":\t\"\",\n"
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
               "\t\"coordinates\":\t\"\",\n"
               "\t\"fw_update_url\":\t\"https://network.ruuvi.com/firmwareupdate\"\n"
               "}"),
        string(json_str.p_str));
    cjson_wrap_free_json_str(&json_str);
}

TEST_F(TestGwCfgJson, gw_cfg_json_parse_malloc_failed) // NOLINT
{
    cJSON_Hooks hooks = {
        .malloc_fn = &os_malloc,
        .free_fn   = &os_free_internal,
    };
    cJSON_InitHooks(&hooks);

    const char* const p_json_str
        = "{\n"
          "\t\"wifi_sta_config\":\t{\n"
          "\t\t\"ssid\":\t\"\",\n"
          "\t\t\"password\":\t\"\"\n"
          "\t},\n"
          "\t\"wifi_ap_config\":\t{\n"
          "\t\t\"password\":\t\"\",\n"
          "\t\t\"channel\":\t1\n"
          "\t},\n"
          "\t\"use_eth\":\tfalse,\n"
          "\t\"eth_dhcp\":\ttrue,\n"
          "\t\"eth_static_ip\":\t\"\",\n"
          "\t\"eth_netmask\":\t\"\","
          "\n\t\"eth_gw\":\t\"\",\n"
          "\t\"eth_dns1\":\t\"\",\n"
          "\t\"eth_dns2\":\t\"\",\n"
          "\t\"remote_cfg_use\":\tfalse,\n"
          "\t\"remote_cfg_url\":\t\"\",\n"
          "\t\"remote_cfg_auth_type\":\t\"none\",\n"
          "\t\"remote_cfg_refresh_interval_minutes\":\t0,\n"
          "\t\"use_http\":\ttrue,\n"
          "\t\"http_url\":\t\"https://network.ruuvi.com/record\",\n"
          "\t\"http_user\":\t\"\",\n"
          "\t\"http_pass\":\t\"\",\n"
          "\t\"use_http_stat\":\ttrue,\n"
          "\t\"http_stat_url\":\t\"https://network.ruuvi.com/status\",\n"
          "\t\"http_stat_user\":\t\"\",\n"
          "\t\"http_stat_pass\":\t\"\",\n"
          "\t\"use_mqtt\":\tfalse,\n"
          "\t\"mqtt_disable_retained_messages\":\tfalse,\n"
          "\t\"mqtt_transport\":\t\"TCP\",\n"
          "\t\"mqtt_server\":\t\"test.mosquitto.org\",\n"
          "\t\"mqtt_port\":\t1883,\n"
          "\t\"mqtt_sending_interval\":\t0,\n"
          "\t\"mqtt_prefix\":\t\"ruuvi/AA:BB:CC:DD:EE:FF/\",\n"
          "\t\"mqtt_client_id\":\t\"AA:BB:CC:DD:EE:FF\",\n"
          "\t\"mqtt_user\":\t\"\",\n"
          "\t\"mqtt_pass\":\t\"\",\n"
          "\t\"mqtt_use_ssl_client_cert\":\tfalse,\n"
          "\t\"mqtt_use_ssl_server_cert\":\tfalse,\n"
          "\t\"lan_auth_type\":\t\"lan_auth_ruuvi\",\n"
          "\t\"lan_auth_user\":\t\"Admin\",\n"
          "\t\"lan_auth_pass\":\t\"\377password_md5\377\",\n"
          "\t\"lan_auth_api_key\":\t\"\",\n"
          "\t\"lan_auth_api_key_rw\":\t\"\",\n"
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
          "\t\"coordinates\":\t\"\",\n"
          "\t\"fw_update_url\":\t\"https://network.ruuvi.com/firmwareupdate\"\n"
          "}";
    for (uint32_t i = 0; i < 165; ++i)
    {
        this->m_malloc_cnt         = 0;
        this->m_malloc_fail_on_cnt = i + 1;
        gw_cfg_t gw_cfg2           = get_gateway_config_default();
        if (gw_cfg_json_parse("my.json", nullptr, p_json_str, &gw_cfg2))
        {
            ASSERT_FALSE(true);
        }
        ASSERT_TRUE(g_pTestClass->m_mem_alloc_trace.is_empty());
    }

    {
        this->m_malloc_cnt         = 0;
        this->m_malloc_fail_on_cnt = 166;
        gw_cfg_t gw_cfg2           = get_gateway_config_default();
        ASSERT_TRUE(gw_cfg_json_parse("my.json", nullptr, p_json_str, &gw_cfg2));
        ASSERT_TRUE(g_pTestClass->m_mem_alloc_trace.is_empty());
    }
}

TEST_F(TestGwCfgJson, gw_cfg_json_generate_malloc_failed_on_json_creation) // NOLINT
{
    const gw_cfg_t   gw_cfg   = get_gateway_config_default_lan_auth_ruuvi();
    cjson_wrap_str_t json_str = cjson_wrap_str_null();

    cJSON_Hooks hooks = {
        .malloc_fn = &os_malloc,
        .free_fn   = &os_free_internal,
    };
    cJSON_InitHooks(&hooks);
    this->m_malloc_fail_on_cnt = 1;

    ASSERT_FALSE(gw_cfg_json_generate_for_saving(&gw_cfg, &json_str));
    ASSERT_EQ(nullptr, json_str.p_str);
    TEST_CHECK_LOG_RECORD(ESP_LOG_ERROR, string("Can't create json object"));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_TRUE(g_pTestClass->m_mem_alloc_trace.is_empty());
}

TEST_F(TestGwCfgJson, gw_cfg_json_generate_malloc_failed_on_wifi_sta_config) // NOLINT
{
    const gw_cfg_t   gw_cfg   = get_gateway_config_default_lan_auth_ruuvi();
    cjson_wrap_str_t json_str = cjson_wrap_str_null();

    cJSON_Hooks hooks = {
        .malloc_fn = &os_malloc,
        .free_fn   = &os_free_internal,
    };
    cJSON_InitHooks(&hooks);
    this->m_malloc_fail_on_cnt = 2;

    ASSERT_FALSE(gw_cfg_json_generate_for_saving(&gw_cfg, &json_str));
    ASSERT_EQ(nullptr, json_str.p_str);
    TEST_CHECK_LOG_RECORD(ESP_LOG_ERROR, string("Can't add json item: wifi_sta_config"));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_TRUE(g_pTestClass->m_mem_alloc_trace.is_empty());
}

TEST_F(TestGwCfgJson, gw_cfg_json_generate_malloc_failed_on_wifi_sta_config2) // NOLINT
{
    const gw_cfg_t   gw_cfg   = get_gateway_config_default_lan_auth_ruuvi();
    cjson_wrap_str_t json_str = cjson_wrap_str_null();

    cJSON_Hooks hooks = {
        .malloc_fn = &os_malloc,
        .free_fn   = &os_free_internal,
    };
    cJSON_InitHooks(&hooks);
    this->m_malloc_fail_on_cnt = 3;

    ASSERT_FALSE(gw_cfg_json_generate_for_saving(&gw_cfg, &json_str));
    ASSERT_EQ(nullptr, json_str.p_str);
    TEST_CHECK_LOG_RECORD(ESP_LOG_ERROR, string("Can't add json item: wifi_sta_config"));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_TRUE(g_pTestClass->m_mem_alloc_trace.is_empty());
}

TEST_F(TestGwCfgJson, gw_cfg_json_generate_malloc_failed_on_wifi_sta_config_ssid) // NOLINT
{
    const gw_cfg_t   gw_cfg   = get_gateway_config_default_lan_auth_ruuvi();
    cjson_wrap_str_t json_str = cjson_wrap_str_null();

    cJSON_Hooks hooks = {
        .malloc_fn = &os_malloc,
        .free_fn   = &os_free_internal,
    };
    cJSON_InitHooks(&hooks);
    this->m_malloc_fail_on_cnt = 4;

    ASSERT_FALSE(gw_cfg_json_generate_for_saving(&gw_cfg, &json_str));
    ASSERT_EQ(nullptr, json_str.p_str);
    TEST_CHECK_LOG_RECORD(ESP_LOG_ERROR, string("Can't add json item: ssid"));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_TRUE(g_pTestClass->m_mem_alloc_trace.is_empty());
}

TEST_F(TestGwCfgJson, gw_cfg_json_generate_malloc_failed_on_wifi_sta_config_ssid_2) // NOLINT
{
    const gw_cfg_t   gw_cfg   = get_gateway_config_default_lan_auth_ruuvi();
    cjson_wrap_str_t json_str = cjson_wrap_str_null();

    cJSON_Hooks hooks = {
        .malloc_fn = &os_malloc,
        .free_fn   = &os_free_internal,
    };
    cJSON_InitHooks(&hooks);
    this->m_malloc_fail_on_cnt = 5;

    ASSERT_FALSE(gw_cfg_json_generate_for_saving(&gw_cfg, &json_str));
    ASSERT_EQ(nullptr, json_str.p_str);
    TEST_CHECK_LOG_RECORD(ESP_LOG_ERROR, string("Can't add json item: ssid"));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_TRUE(g_pTestClass->m_mem_alloc_trace.is_empty());
}

TEST_F(TestGwCfgJson, gw_cfg_json_generate_malloc_failed_on_wifi_sta_config_ssid_3) // NOLINT
{
    const gw_cfg_t   gw_cfg   = get_gateway_config_default_lan_auth_ruuvi();
    cjson_wrap_str_t json_str = cjson_wrap_str_null();

    cJSON_Hooks hooks = {
        .malloc_fn = &os_malloc,
        .free_fn   = &os_free_internal,
    };
    cJSON_InitHooks(&hooks);
    this->m_malloc_fail_on_cnt = 6;

    ASSERT_FALSE(gw_cfg_json_generate_for_saving(&gw_cfg, &json_str));
    ASSERT_EQ(nullptr, json_str.p_str);
    TEST_CHECK_LOG_RECORD(ESP_LOG_ERROR, string("Can't add json item: ssid"));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_TRUE(g_pTestClass->m_mem_alloc_trace.is_empty());
}

TEST_F(TestGwCfgJson, gw_cfg_json_generate_malloc_failed_on_wifi_sta_config_password) // NOLINT
{
    const gw_cfg_t   gw_cfg   = get_gateway_config_default_lan_auth_ruuvi();
    cjson_wrap_str_t json_str = cjson_wrap_str_null();

    cJSON_Hooks hooks = {
        .malloc_fn = &os_malloc,
        .free_fn   = &os_free_internal,
    };
    cJSON_InitHooks(&hooks);
    this->m_malloc_fail_on_cnt = 7;

    ASSERT_FALSE(gw_cfg_json_generate_for_saving(&gw_cfg, &json_str));
    ASSERT_EQ(nullptr, json_str.p_str);
    TEST_CHECK_LOG_RECORD(ESP_LOG_ERROR, string("Can't add json item: password"));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_TRUE(g_pTestClass->m_mem_alloc_trace.is_empty());
}

TEST_F(TestGwCfgJson, gw_cfg_json_generate_malloc_failed_on_wifi_sta_config_password_2) // NOLINT
{
    const gw_cfg_t   gw_cfg   = get_gateway_config_default_lan_auth_ruuvi();
    cjson_wrap_str_t json_str = cjson_wrap_str_null();

    cJSON_Hooks hooks = {
        .malloc_fn = &os_malloc,
        .free_fn   = &os_free_internal,
    };
    cJSON_InitHooks(&hooks);
    this->m_malloc_fail_on_cnt = 8;

    ASSERT_FALSE(gw_cfg_json_generate_for_saving(&gw_cfg, &json_str));
    ASSERT_EQ(nullptr, json_str.p_str);
    TEST_CHECK_LOG_RECORD(ESP_LOG_ERROR, string("Can't add json item: password"));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_TRUE(g_pTestClass->m_mem_alloc_trace.is_empty());
}

TEST_F(TestGwCfgJson, gw_cfg_json_generate_malloc_failed_on_wifi_sta_config_password_3) // NOLINT
{
    const gw_cfg_t   gw_cfg   = get_gateway_config_default_lan_auth_ruuvi();
    cjson_wrap_str_t json_str = cjson_wrap_str_null();

    cJSON_Hooks hooks = {
        .malloc_fn = &os_malloc,
        .free_fn   = &os_free_internal,
    };
    cJSON_InitHooks(&hooks);
    this->m_malloc_fail_on_cnt = 9;

    ASSERT_FALSE(gw_cfg_json_generate_for_saving(&gw_cfg, &json_str));
    ASSERT_EQ(nullptr, json_str.p_str);
    TEST_CHECK_LOG_RECORD(ESP_LOG_ERROR, string("Can't add json item: password"));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_TRUE(g_pTestClass->m_mem_alloc_trace.is_empty());
}

TEST_F(TestGwCfgJson, gw_cfg_json_generate_malloc_failed_on_wifi_ap_config) // NOLINT
{
    const gw_cfg_t   gw_cfg   = get_gateway_config_default_lan_auth_ruuvi();
    cjson_wrap_str_t json_str = cjson_wrap_str_null();

    cJSON_Hooks hooks = {
        .malloc_fn = &os_malloc,
        .free_fn   = &os_free_internal,
    };
    cJSON_InitHooks(&hooks);
    this->m_malloc_fail_on_cnt = 10;

    ASSERT_FALSE(gw_cfg_json_generate_for_saving(&gw_cfg, &json_str));
    ASSERT_EQ(nullptr, json_str.p_str);
    TEST_CHECK_LOG_RECORD(ESP_LOG_ERROR, string("Can't add json item: wifi_ap_config"));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_TRUE(g_pTestClass->m_mem_alloc_trace.is_empty());
}

TEST_F(TestGwCfgJson, gw_cfg_json_generate_malloc_failed_on_wifi_ap_config_2) // NOLINT
{
    const gw_cfg_t   gw_cfg   = get_gateway_config_default_lan_auth_ruuvi();
    cjson_wrap_str_t json_str = cjson_wrap_str_null();

    cJSON_Hooks hooks = {
        .malloc_fn = &os_malloc,
        .free_fn   = &os_free_internal,
    };
    cJSON_InitHooks(&hooks);
    this->m_malloc_fail_on_cnt = 11;

    ASSERT_FALSE(gw_cfg_json_generate_for_saving(&gw_cfg, &json_str));
    ASSERT_EQ(nullptr, json_str.p_str);
    TEST_CHECK_LOG_RECORD(ESP_LOG_ERROR, string("Can't add json item: wifi_ap_config"));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_TRUE(g_pTestClass->m_mem_alloc_trace.is_empty());
}

TEST_F(TestGwCfgJson, gw_cfg_json_generate_malloc_failed_on_wifi_ap_config_password) // NOLINT
{
    const gw_cfg_t   gw_cfg   = get_gateway_config_default_lan_auth_ruuvi();
    cjson_wrap_str_t json_str = cjson_wrap_str_null();

    cJSON_Hooks hooks = {
        .malloc_fn = &os_malloc,
        .free_fn   = &os_free_internal,
    };
    cJSON_InitHooks(&hooks);
    this->m_malloc_fail_on_cnt = 12;

    ASSERT_FALSE(gw_cfg_json_generate_for_saving(&gw_cfg, &json_str));
    ASSERT_EQ(nullptr, json_str.p_str);
    TEST_CHECK_LOG_RECORD(ESP_LOG_ERROR, string("Can't add json item: password"));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_TRUE(g_pTestClass->m_mem_alloc_trace.is_empty());
}

TEST_F(TestGwCfgJson, gw_cfg_json_generate_malloc_failed_on_wifi_ap_config_password_2) // NOLINT
{
    const gw_cfg_t   gw_cfg   = get_gateway_config_default_lan_auth_ruuvi();
    cjson_wrap_str_t json_str = cjson_wrap_str_null();

    cJSON_Hooks hooks = {
        .malloc_fn = &os_malloc,
        .free_fn   = &os_free_internal,
    };
    cJSON_InitHooks(&hooks);
    this->m_malloc_fail_on_cnt = 13;

    ASSERT_FALSE(gw_cfg_json_generate_for_saving(&gw_cfg, &json_str));
    ASSERT_EQ(nullptr, json_str.p_str);
    TEST_CHECK_LOG_RECORD(ESP_LOG_ERROR, string("Can't add json item: password"));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_TRUE(g_pTestClass->m_mem_alloc_trace.is_empty());
}

TEST_F(TestGwCfgJson, gw_cfg_json_generate_malloc_failed_on_wifi_ap_config_password_3) // NOLINT
{
    const gw_cfg_t   gw_cfg   = get_gateway_config_default_lan_auth_ruuvi();
    cjson_wrap_str_t json_str = cjson_wrap_str_null();

    cJSON_Hooks hooks = {
        .malloc_fn = &os_malloc,
        .free_fn   = &os_free_internal,
    };
    cJSON_InitHooks(&hooks);
    this->m_malloc_fail_on_cnt = 14;

    ASSERT_FALSE(gw_cfg_json_generate_for_saving(&gw_cfg, &json_str));
    ASSERT_EQ(nullptr, json_str.p_str);
    TEST_CHECK_LOG_RECORD(ESP_LOG_ERROR, string("Can't add json item: password"));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_TRUE(g_pTestClass->m_mem_alloc_trace.is_empty());
}

TEST_F(TestGwCfgJson, gw_cfg_json_generate_malloc_failed_on_wifi_ap_config_channel) // NOLINT
{
    const gw_cfg_t   gw_cfg   = get_gateway_config_default_lan_auth_ruuvi();
    cjson_wrap_str_t json_str = cjson_wrap_str_null();

    cJSON_Hooks hooks = {
        .malloc_fn = &os_malloc,
        .free_fn   = &os_free_internal,
    };
    cJSON_InitHooks(&hooks);
    this->m_malloc_fail_on_cnt = 15;

    ASSERT_FALSE(gw_cfg_json_generate_for_saving(&gw_cfg, &json_str));
    ASSERT_EQ(nullptr, json_str.p_str);
    TEST_CHECK_LOG_RECORD(ESP_LOG_ERROR, string("Can't add json item: channel"));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_TRUE(g_pTestClass->m_mem_alloc_trace.is_empty());
}

TEST_F(TestGwCfgJson, gw_cfg_json_generate_malloc_failed_on_wifi_ap_config_channel_2) // NOLINT
{
    const gw_cfg_t   gw_cfg   = get_gateway_config_default_lan_auth_ruuvi();
    cjson_wrap_str_t json_str = cjson_wrap_str_null();

    cJSON_Hooks hooks = {
        .malloc_fn = &os_malloc,
        .free_fn   = &os_free_internal,
    };
    cJSON_InitHooks(&hooks);
    this->m_malloc_fail_on_cnt = 16;

    ASSERT_FALSE(gw_cfg_json_generate_for_saving(&gw_cfg, &json_str));
    ASSERT_EQ(nullptr, json_str.p_str);
    TEST_CHECK_LOG_RECORD(ESP_LOG_ERROR, string("Can't add json item: channel"));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_TRUE(g_pTestClass->m_mem_alloc_trace.is_empty());
}

TEST_F(TestGwCfgJson, gw_cfg_json_generate_malloc_failed_on_use_eth) // NOLINT
{
    const gw_cfg_t   gw_cfg   = get_gateway_config_default_lan_auth_ruuvi();
    cjson_wrap_str_t json_str = cjson_wrap_str_null();

    cJSON_Hooks hooks = {
        .malloc_fn = &os_malloc,
        .free_fn   = &os_free_internal,
    };
    cJSON_InitHooks(&hooks);
    this->m_malloc_fail_on_cnt = 17;

    ASSERT_FALSE(gw_cfg_json_generate_for_saving(&gw_cfg, &json_str));
    ASSERT_EQ(nullptr, json_str.p_str);
    TEST_CHECK_LOG_RECORD(ESP_LOG_ERROR, string("Can't add json item: use_eth"));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_TRUE(g_pTestClass->m_mem_alloc_trace.is_empty());
}

TEST_F(TestGwCfgJson, gw_cfg_json_generate_malloc_failed_on_use_eth_2) // NOLINT
{
    const gw_cfg_t   gw_cfg   = get_gateway_config_default_lan_auth_ruuvi();
    cjson_wrap_str_t json_str = cjson_wrap_str_null();

    cJSON_Hooks hooks = {
        .malloc_fn = &os_malloc,
        .free_fn   = &os_free_internal,
    };
    cJSON_InitHooks(&hooks);
    this->m_malloc_fail_on_cnt = 18;

    ASSERT_FALSE(gw_cfg_json_generate_for_saving(&gw_cfg, &json_str));
    ASSERT_EQ(nullptr, json_str.p_str);
    TEST_CHECK_LOG_RECORD(ESP_LOG_ERROR, string("Can't add json item: use_eth"));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_TRUE(g_pTestClass->m_mem_alloc_trace.is_empty());
}

TEST_F(TestGwCfgJson, gw_cfg_json_generate_malloc_failed_on_eth_dhcp) // NOLINT
{
    const gw_cfg_t   gw_cfg   = get_gateway_config_default_lan_auth_ruuvi();
    cjson_wrap_str_t json_str = cjson_wrap_str_null();

    cJSON_Hooks hooks = {
        .malloc_fn = &os_malloc,
        .free_fn   = &os_free_internal,
    };
    cJSON_InitHooks(&hooks);
    this->m_malloc_fail_on_cnt = 19;

    ASSERT_FALSE(gw_cfg_json_generate_for_saving(&gw_cfg, &json_str));
    ASSERT_EQ(nullptr, json_str.p_str);
    TEST_CHECK_LOG_RECORD(ESP_LOG_ERROR, string("Can't add json item: eth_dhcp"));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_TRUE(g_pTestClass->m_mem_alloc_trace.is_empty());
}

TEST_F(TestGwCfgJson, gw_cfg_json_generate_malloc_failed_on_eth_dhcp_2) // NOLINT
{
    const gw_cfg_t   gw_cfg   = get_gateway_config_default_lan_auth_ruuvi();
    cjson_wrap_str_t json_str = cjson_wrap_str_null();

    cJSON_Hooks hooks = {
        .malloc_fn = &os_malloc,
        .free_fn   = &os_free_internal,
    };
    cJSON_InitHooks(&hooks);
    this->m_malloc_fail_on_cnt = 20;

    ASSERT_FALSE(gw_cfg_json_generate_for_saving(&gw_cfg, &json_str));
    ASSERT_EQ(nullptr, json_str.p_str);
    TEST_CHECK_LOG_RECORD(ESP_LOG_ERROR, string("Can't add json item: eth_dhcp"));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_TRUE(g_pTestClass->m_mem_alloc_trace.is_empty());
}

TEST_F(TestGwCfgJson, gw_cfg_json_generate_malloc_failed_on_eth_static_ip) // NOLINT
{
    const gw_cfg_t   gw_cfg   = get_gateway_config_default_lan_auth_ruuvi();
    cjson_wrap_str_t json_str = cjson_wrap_str_null();

    cJSON_Hooks hooks = {
        .malloc_fn = &os_malloc,
        .free_fn   = &os_free_internal,
    };
    cJSON_InitHooks(&hooks);
    this->m_malloc_fail_on_cnt = 21;

    ASSERT_FALSE(gw_cfg_json_generate_for_saving(&gw_cfg, &json_str));
    ASSERT_EQ(nullptr, json_str.p_str);
    TEST_CHECK_LOG_RECORD(ESP_LOG_ERROR, string("Can't add json item: eth_static_ip"));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_TRUE(g_pTestClass->m_mem_alloc_trace.is_empty());
}

TEST_F(TestGwCfgJson, gw_cfg_json_generate_malloc_failed_on_eth_static_ip_2) // NOLINT
{
    const gw_cfg_t   gw_cfg   = get_gateway_config_default_lan_auth_ruuvi();
    cjson_wrap_str_t json_str = cjson_wrap_str_null();

    cJSON_Hooks hooks = {
        .malloc_fn = &os_malloc,
        .free_fn   = &os_free_internal,
    };
    cJSON_InitHooks(&hooks);
    this->m_malloc_fail_on_cnt = 22;

    ASSERT_FALSE(gw_cfg_json_generate_for_saving(&gw_cfg, &json_str));
    ASSERT_EQ(nullptr, json_str.p_str);
    TEST_CHECK_LOG_RECORD(ESP_LOG_ERROR, string("Can't add json item: eth_static_ip"));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_TRUE(g_pTestClass->m_mem_alloc_trace.is_empty());
}

TEST_F(TestGwCfgJson, gw_cfg_json_generate_malloc_failed_on_eth_static_ip_3) // NOLINT
{
    const gw_cfg_t   gw_cfg   = get_gateway_config_default_lan_auth_ruuvi();
    cjson_wrap_str_t json_str = cjson_wrap_str_null();

    cJSON_Hooks hooks = {
        .malloc_fn = &os_malloc,
        .free_fn   = &os_free_internal,
    };
    cJSON_InitHooks(&hooks);
    this->m_malloc_fail_on_cnt = 23;

    ASSERT_FALSE(gw_cfg_json_generate_for_saving(&gw_cfg, &json_str));
    ASSERT_EQ(nullptr, json_str.p_str);
    TEST_CHECK_LOG_RECORD(ESP_LOG_ERROR, string("Can't add json item: eth_static_ip"));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_TRUE(g_pTestClass->m_mem_alloc_trace.is_empty());
}

TEST_F(TestGwCfgJson, gw_cfg_json_generate_malloc_failed_on_eth_netmask) // NOLINT
{
    const gw_cfg_t   gw_cfg   = get_gateway_config_default_lan_auth_ruuvi();
    cjson_wrap_str_t json_str = cjson_wrap_str_null();

    cJSON_Hooks hooks = {
        .malloc_fn = &os_malloc,
        .free_fn   = &os_free_internal,
    };
    cJSON_InitHooks(&hooks);
    this->m_malloc_fail_on_cnt = 24;

    ASSERT_FALSE(gw_cfg_json_generate_for_saving(&gw_cfg, &json_str));
    ASSERT_EQ(nullptr, json_str.p_str);
    TEST_CHECK_LOG_RECORD(ESP_LOG_ERROR, string("Can't add json item: eth_netmask"));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_TRUE(g_pTestClass->m_mem_alloc_trace.is_empty());
}

TEST_F(TestGwCfgJson, gw_cfg_json_generate_malloc_failed_on_eth_netmask_2) // NOLINT
{
    const gw_cfg_t   gw_cfg   = get_gateway_config_default_lan_auth_ruuvi();
    cjson_wrap_str_t json_str = cjson_wrap_str_null();

    cJSON_Hooks hooks = {
        .malloc_fn = &os_malloc,
        .free_fn   = &os_free_internal,
    };
    cJSON_InitHooks(&hooks);
    this->m_malloc_fail_on_cnt = 25;

    ASSERT_FALSE(gw_cfg_json_generate_for_saving(&gw_cfg, &json_str));
    ASSERT_EQ(nullptr, json_str.p_str);
    TEST_CHECK_LOG_RECORD(ESP_LOG_ERROR, string("Can't add json item: eth_netmask"));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_TRUE(g_pTestClass->m_mem_alloc_trace.is_empty());
}

TEST_F(TestGwCfgJson, gw_cfg_json_generate_malloc_failed_on_eth_netmask_3) // NOLINT
{
    const gw_cfg_t   gw_cfg   = get_gateway_config_default_lan_auth_ruuvi();
    cjson_wrap_str_t json_str = cjson_wrap_str_null();

    cJSON_Hooks hooks = {
        .malloc_fn = &os_malloc,
        .free_fn   = &os_free_internal,
    };
    cJSON_InitHooks(&hooks);
    this->m_malloc_fail_on_cnt = 26;

    ASSERT_FALSE(gw_cfg_json_generate_for_saving(&gw_cfg, &json_str));
    ASSERT_EQ(nullptr, json_str.p_str);
    TEST_CHECK_LOG_RECORD(ESP_LOG_ERROR, string("Can't add json item: eth_netmask"));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_TRUE(g_pTestClass->m_mem_alloc_trace.is_empty());
}

TEST_F(TestGwCfgJson, gw_cfg_json_generate_malloc_failed_on_eth_gw) // NOLINT
{
    const gw_cfg_t   gw_cfg   = get_gateway_config_default_lan_auth_ruuvi();
    cjson_wrap_str_t json_str = cjson_wrap_str_null();

    cJSON_Hooks hooks = {
        .malloc_fn = &os_malloc,
        .free_fn   = &os_free_internal,
    };
    cJSON_InitHooks(&hooks);
    this->m_malloc_fail_on_cnt = 27;

    ASSERT_FALSE(gw_cfg_json_generate_for_saving(&gw_cfg, &json_str));
    ASSERT_EQ(nullptr, json_str.p_str);
    TEST_CHECK_LOG_RECORD(ESP_LOG_ERROR, string("Can't add json item: eth_gw"));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_TRUE(g_pTestClass->m_mem_alloc_trace.is_empty());
}

TEST_F(TestGwCfgJson, gw_cfg_json_generate_malloc_failed_on_eth_gw_2) // NOLINT
{
    const gw_cfg_t   gw_cfg   = get_gateway_config_default_lan_auth_ruuvi();
    cjson_wrap_str_t json_str = cjson_wrap_str_null();

    cJSON_Hooks hooks = {
        .malloc_fn = &os_malloc,
        .free_fn   = &os_free_internal,
    };
    cJSON_InitHooks(&hooks);
    this->m_malloc_fail_on_cnt = 28;

    ASSERT_FALSE(gw_cfg_json_generate_for_saving(&gw_cfg, &json_str));
    ASSERT_EQ(nullptr, json_str.p_str);
    TEST_CHECK_LOG_RECORD(ESP_LOG_ERROR, string("Can't add json item: eth_gw"));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_TRUE(g_pTestClass->m_mem_alloc_trace.is_empty());
}

TEST_F(TestGwCfgJson, gw_cfg_json_generate_malloc_failed_on_eth_gw_3) // NOLINT
{
    const gw_cfg_t   gw_cfg   = get_gateway_config_default_lan_auth_ruuvi();
    cjson_wrap_str_t json_str = cjson_wrap_str_null();

    cJSON_Hooks hooks = {
        .malloc_fn = &os_malloc,
        .free_fn   = &os_free_internal,
    };
    cJSON_InitHooks(&hooks);
    this->m_malloc_fail_on_cnt = 29;

    ASSERT_FALSE(gw_cfg_json_generate_for_saving(&gw_cfg, &json_str));
    ASSERT_EQ(nullptr, json_str.p_str);
    TEST_CHECK_LOG_RECORD(ESP_LOG_ERROR, string("Can't add json item: eth_gw"));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_TRUE(g_pTestClass->m_mem_alloc_trace.is_empty());
}

TEST_F(TestGwCfgJson, gw_cfg_json_generate_malloc_failed_on_eth_dns1) // NOLINT
{
    const gw_cfg_t   gw_cfg   = get_gateway_config_default_lan_auth_ruuvi();
    cjson_wrap_str_t json_str = cjson_wrap_str_null();

    cJSON_Hooks hooks = {
        .malloc_fn = &os_malloc,
        .free_fn   = &os_free_internal,
    };
    cJSON_InitHooks(&hooks);
    this->m_malloc_fail_on_cnt = 30;

    ASSERT_FALSE(gw_cfg_json_generate_for_saving(&gw_cfg, &json_str));
    ASSERT_EQ(nullptr, json_str.p_str);
    TEST_CHECK_LOG_RECORD(ESP_LOG_ERROR, string("Can't add json item: eth_dns1"));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_TRUE(g_pTestClass->m_mem_alloc_trace.is_empty());
}

TEST_F(TestGwCfgJson, gw_cfg_json_generate_malloc_failed_on_eth_dns1_2) // NOLINT
{
    const gw_cfg_t   gw_cfg   = get_gateway_config_default_lan_auth_ruuvi();
    cjson_wrap_str_t json_str = cjson_wrap_str_null();

    cJSON_Hooks hooks = {
        .malloc_fn = &os_malloc,
        .free_fn   = &os_free_internal,
    };
    cJSON_InitHooks(&hooks);
    this->m_malloc_fail_on_cnt = 31;

    ASSERT_FALSE(gw_cfg_json_generate_for_saving(&gw_cfg, &json_str));
    ASSERT_EQ(nullptr, json_str.p_str);
    TEST_CHECK_LOG_RECORD(ESP_LOG_ERROR, string("Can't add json item: eth_dns1"));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_TRUE(g_pTestClass->m_mem_alloc_trace.is_empty());
}

TEST_F(TestGwCfgJson, gw_cfg_json_generate_malloc_failed_on_eth_dns1_3) // NOLINT
{
    const gw_cfg_t   gw_cfg   = get_gateway_config_default_lan_auth_ruuvi();
    cjson_wrap_str_t json_str = cjson_wrap_str_null();

    cJSON_Hooks hooks = {
        .malloc_fn = &os_malloc,
        .free_fn   = &os_free_internal,
    };
    cJSON_InitHooks(&hooks);
    this->m_malloc_fail_on_cnt = 32;

    ASSERT_FALSE(gw_cfg_json_generate_for_saving(&gw_cfg, &json_str));
    ASSERT_EQ(nullptr, json_str.p_str);
    TEST_CHECK_LOG_RECORD(ESP_LOG_ERROR, string("Can't add json item: eth_dns1"));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_TRUE(g_pTestClass->m_mem_alloc_trace.is_empty());
}

TEST_F(TestGwCfgJson, gw_cfg_json_generate_malloc_failed_on_eth_dns2) // NOLINT
{
    const gw_cfg_t   gw_cfg   = get_gateway_config_default_lan_auth_ruuvi();
    cjson_wrap_str_t json_str = cjson_wrap_str_null();

    cJSON_Hooks hooks = {
        .malloc_fn = &os_malloc,
        .free_fn   = &os_free_internal,
    };
    cJSON_InitHooks(&hooks);
    this->m_malloc_fail_on_cnt = 33;

    ASSERT_FALSE(gw_cfg_json_generate_for_saving(&gw_cfg, &json_str));
    ASSERT_EQ(nullptr, json_str.p_str);
    TEST_CHECK_LOG_RECORD(ESP_LOG_ERROR, string("Can't add json item: eth_dns2"));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_TRUE(g_pTestClass->m_mem_alloc_trace.is_empty());
}

TEST_F(TestGwCfgJson, gw_cfg_json_generate_malloc_failed_on_eth_dns2_1) // NOLINT
{
    const gw_cfg_t   gw_cfg   = get_gateway_config_default_lan_auth_ruuvi();
    cjson_wrap_str_t json_str = cjson_wrap_str_null();

    cJSON_Hooks hooks = {
        .malloc_fn = &os_malloc,
        .free_fn   = &os_free_internal,
    };
    cJSON_InitHooks(&hooks);
    this->m_malloc_fail_on_cnt = 34;

    ASSERT_FALSE(gw_cfg_json_generate_for_saving(&gw_cfg, &json_str));
    ASSERT_EQ(nullptr, json_str.p_str);
    TEST_CHECK_LOG_RECORD(ESP_LOG_ERROR, string("Can't add json item: eth_dns2"));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_TRUE(g_pTestClass->m_mem_alloc_trace.is_empty());
}

TEST_F(TestGwCfgJson, gw_cfg_json_generate_malloc_failed_on_eth_dns2_2) // NOLINT
{
    const gw_cfg_t   gw_cfg   = get_gateway_config_default_lan_auth_ruuvi();
    cjson_wrap_str_t json_str = cjson_wrap_str_null();

    cJSON_Hooks hooks = {
        .malloc_fn = &os_malloc,
        .free_fn   = &os_free_internal,
    };
    cJSON_InitHooks(&hooks);
    this->m_malloc_fail_on_cnt = 35;

    ASSERT_FALSE(gw_cfg_json_generate_for_saving(&gw_cfg, &json_str));
    ASSERT_EQ(nullptr, json_str.p_str);
    TEST_CHECK_LOG_RECORD(ESP_LOG_ERROR, string("Can't add json item: eth_dns2"));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_TRUE(g_pTestClass->m_mem_alloc_trace.is_empty());
}

TEST_F(TestGwCfgJson, gw_cfg_json_generate_malloc_failed_on_remote_cfg_use) // NOLINT
{
    const gw_cfg_t   gw_cfg   = get_gateway_config_default_lan_auth_ruuvi();
    cjson_wrap_str_t json_str = cjson_wrap_str_null();

    cJSON_Hooks hooks = {
        .malloc_fn = &os_malloc,
        .free_fn   = &os_free_internal,
    };
    cJSON_InitHooks(&hooks);
    this->m_malloc_fail_on_cnt = 36;

    ASSERT_FALSE(gw_cfg_json_generate_for_saving(&gw_cfg, &json_str));
    ASSERT_EQ(nullptr, json_str.p_str);
    TEST_CHECK_LOG_RECORD(ESP_LOG_ERROR, string("Can't add json item: remote_cfg_use"));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_TRUE(g_pTestClass->m_mem_alloc_trace.is_empty());
}

TEST_F(TestGwCfgJson, gw_cfg_json_generate_malloc_failed_on_remote_cfg_use_2) // NOLINT
{
    const gw_cfg_t   gw_cfg   = get_gateway_config_default_lan_auth_ruuvi();
    cjson_wrap_str_t json_str = cjson_wrap_str_null();

    cJSON_Hooks hooks = {
        .malloc_fn = &os_malloc,
        .free_fn   = &os_free_internal,
    };
    cJSON_InitHooks(&hooks);
    this->m_malloc_fail_on_cnt = 37;

    ASSERT_FALSE(gw_cfg_json_generate_for_saving(&gw_cfg, &json_str));
    ASSERT_EQ(nullptr, json_str.p_str);
    TEST_CHECK_LOG_RECORD(ESP_LOG_ERROR, string("Can't add json item: remote_cfg_use"));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_TRUE(g_pTestClass->m_mem_alloc_trace.is_empty());
}

TEST_F(TestGwCfgJson, gw_cfg_json_generate_malloc_failed_on_remote_cfg_url) // NOLINT
{
    const gw_cfg_t   gw_cfg   = get_gateway_config_default_lan_auth_ruuvi();
    cjson_wrap_str_t json_str = cjson_wrap_str_null();

    cJSON_Hooks hooks = {
        .malloc_fn = &os_malloc,
        .free_fn   = &os_free_internal,
    };
    cJSON_InitHooks(&hooks);
    this->m_malloc_fail_on_cnt = 38;

    ASSERT_FALSE(gw_cfg_json_generate_for_saving(&gw_cfg, &json_str));
    ASSERT_EQ(nullptr, json_str.p_str);
    TEST_CHECK_LOG_RECORD(ESP_LOG_ERROR, string("Can't add json item: remote_cfg_url"));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_TRUE(g_pTestClass->m_mem_alloc_trace.is_empty());
}

TEST_F(TestGwCfgJson, gw_cfg_json_generate_malloc_failed_on_remote_cfg_url_2) // NOLINT
{
    const gw_cfg_t   gw_cfg   = get_gateway_config_default_lan_auth_ruuvi();
    cjson_wrap_str_t json_str = cjson_wrap_str_null();

    cJSON_Hooks hooks = {
        .malloc_fn = &os_malloc,
        .free_fn   = &os_free_internal,
    };
    cJSON_InitHooks(&hooks);
    this->m_malloc_fail_on_cnt = 39;

    ASSERT_FALSE(gw_cfg_json_generate_for_saving(&gw_cfg, &json_str));
    ASSERT_EQ(nullptr, json_str.p_str);
    TEST_CHECK_LOG_RECORD(ESP_LOG_ERROR, string("Can't add json item: remote_cfg_url"));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_TRUE(g_pTestClass->m_mem_alloc_trace.is_empty());
}

TEST_F(TestGwCfgJson, gw_cfg_json_generate_malloc_failed_on_remote_cfg_url_3) // NOLINT
{
    const gw_cfg_t   gw_cfg   = get_gateway_config_default_lan_auth_ruuvi();
    cjson_wrap_str_t json_str = cjson_wrap_str_null();

    cJSON_Hooks hooks = {
        .malloc_fn = &os_malloc,
        .free_fn   = &os_free_internal,
    };
    cJSON_InitHooks(&hooks);
    this->m_malloc_fail_on_cnt = 40;

    ASSERT_FALSE(gw_cfg_json_generate_for_saving(&gw_cfg, &json_str));
    ASSERT_EQ(nullptr, json_str.p_str);
    TEST_CHECK_LOG_RECORD(ESP_LOG_ERROR, string("Can't add json item: remote_cfg_url"));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_TRUE(g_pTestClass->m_mem_alloc_trace.is_empty());
}

TEST_F(TestGwCfgJson, gw_cfg_json_generate_malloc_failed_on_remote_cfg_auth_type) // NOLINT
{
    const gw_cfg_t   gw_cfg   = get_gateway_config_default_lan_auth_ruuvi();
    cjson_wrap_str_t json_str = cjson_wrap_str_null();

    cJSON_Hooks hooks = {
        .malloc_fn = &os_malloc,
        .free_fn   = &os_free_internal,
    };
    cJSON_InitHooks(&hooks);
    this->m_malloc_fail_on_cnt = 41;

    ASSERT_FALSE(gw_cfg_json_generate_for_saving(&gw_cfg, &json_str));
    ASSERT_EQ(nullptr, json_str.p_str);
    TEST_CHECK_LOG_RECORD(ESP_LOG_ERROR, string("Can't add json item: remote_cfg_auth_type"));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_TRUE(g_pTestClass->m_mem_alloc_trace.is_empty());
}

TEST_F(TestGwCfgJson, gw_cfg_json_generate_malloc_failed_on_remote_cfg_auth_type_2) // NOLINT
{
    const gw_cfg_t   gw_cfg   = get_gateway_config_default_lan_auth_ruuvi();
    cjson_wrap_str_t json_str = cjson_wrap_str_null();

    cJSON_Hooks hooks = {
        .malloc_fn = &os_malloc,
        .free_fn   = &os_free_internal,
    };
    cJSON_InitHooks(&hooks);
    this->m_malloc_fail_on_cnt = 42;

    ASSERT_FALSE(gw_cfg_json_generate_for_saving(&gw_cfg, &json_str));
    ASSERT_EQ(nullptr, json_str.p_str);
    TEST_CHECK_LOG_RECORD(ESP_LOG_ERROR, string("Can't add json item: remote_cfg_auth_type"));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_TRUE(g_pTestClass->m_mem_alloc_trace.is_empty());
}

TEST_F(TestGwCfgJson, gw_cfg_json_generate_malloc_failed_on_remote_cfg_auth_type_3) // NOLINT
{
    const gw_cfg_t   gw_cfg   = get_gateway_config_default_lan_auth_ruuvi();
    cjson_wrap_str_t json_str = cjson_wrap_str_null();

    cJSON_Hooks hooks = {
        .malloc_fn = &os_malloc,
        .free_fn   = &os_free_internal,
    };
    cJSON_InitHooks(&hooks);
    this->m_malloc_fail_on_cnt = 43;

    ASSERT_FALSE(gw_cfg_json_generate_for_saving(&gw_cfg, &json_str));
    ASSERT_EQ(nullptr, json_str.p_str);
    TEST_CHECK_LOG_RECORD(ESP_LOG_ERROR, string("Can't add json item: remote_cfg_auth_type"));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_TRUE(g_pTestClass->m_mem_alloc_trace.is_empty());
}

TEST_F(TestGwCfgJson, gw_cfg_json_generate_malloc_failed_on_remote_cfg_use_ssl_client_cert) // NOLINT
{
    const gw_cfg_t   gw_cfg   = get_gateway_config_default_lan_auth_ruuvi();
    cjson_wrap_str_t json_str = cjson_wrap_str_null();

    cJSON_Hooks hooks = {
        .malloc_fn = &os_malloc,
        .free_fn   = &os_free_internal,
    };
    cJSON_InitHooks(&hooks);
    this->m_malloc_fail_on_cnt = 44;

    ASSERT_FALSE(gw_cfg_json_generate_for_saving(&gw_cfg, &json_str));
    ASSERT_EQ(nullptr, json_str.p_str);
    TEST_CHECK_LOG_RECORD(ESP_LOG_ERROR, string("Can't add json item: remote_cfg_use_ssl_client_cert"));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_TRUE(g_pTestClass->m_mem_alloc_trace.is_empty());
}

TEST_F(TestGwCfgJson, gw_cfg_json_generate_malloc_failed_on_remote_cfg_use_ssl_client_cert_2) // NOLINT
{
    const gw_cfg_t   gw_cfg   = get_gateway_config_default_lan_auth_ruuvi();
    cjson_wrap_str_t json_str = cjson_wrap_str_null();

    cJSON_Hooks hooks = {
        .malloc_fn = &os_malloc,
        .free_fn   = &os_free_internal,
    };
    cJSON_InitHooks(&hooks);
    this->m_malloc_fail_on_cnt = 45;

    ASSERT_FALSE(gw_cfg_json_generate_for_saving(&gw_cfg, &json_str));
    ASSERT_EQ(nullptr, json_str.p_str);
    TEST_CHECK_LOG_RECORD(ESP_LOG_ERROR, string("Can't add json item: remote_cfg_use_ssl_client_cert"));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_TRUE(g_pTestClass->m_mem_alloc_trace.is_empty());
}

TEST_F(TestGwCfgJson, gw_cfg_json_generate_malloc_failed_on_remote_cfg_use_ssl_server_cert) // NOLINT
{
    const gw_cfg_t   gw_cfg   = get_gateway_config_default_lan_auth_ruuvi();
    cjson_wrap_str_t json_str = cjson_wrap_str_null();

    cJSON_Hooks hooks = {
        .malloc_fn = &os_malloc,
        .free_fn   = &os_free_internal,
    };
    cJSON_InitHooks(&hooks);
    this->m_malloc_fail_on_cnt = 46;

    ASSERT_FALSE(gw_cfg_json_generate_for_saving(&gw_cfg, &json_str));
    ASSERT_EQ(nullptr, json_str.p_str);
    TEST_CHECK_LOG_RECORD(ESP_LOG_ERROR, string("Can't add json item: remote_cfg_use_ssl_server_cert"));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_TRUE(g_pTestClass->m_mem_alloc_trace.is_empty());
}

TEST_F(TestGwCfgJson, gw_cfg_json_generate_malloc_failed_on_remote_cfg_use_ssl_server_cert_2) // NOLINT
{
    const gw_cfg_t   gw_cfg   = get_gateway_config_default_lan_auth_ruuvi();
    cjson_wrap_str_t json_str = cjson_wrap_str_null();

    cJSON_Hooks hooks = {
        .malloc_fn = &os_malloc,
        .free_fn   = &os_free_internal,
    };
    cJSON_InitHooks(&hooks);
    this->m_malloc_fail_on_cnt = 47;

    ASSERT_FALSE(gw_cfg_json_generate_for_saving(&gw_cfg, &json_str));
    ASSERT_EQ(nullptr, json_str.p_str);
    TEST_CHECK_LOG_RECORD(ESP_LOG_ERROR, string("Can't add json item: remote_cfg_use_ssl_server_cert"));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_TRUE(g_pTestClass->m_mem_alloc_trace.is_empty());
}

TEST_F(TestGwCfgJson, gw_cfg_json_generate_malloc_failed_on_remote_cfg_refresh_interval_minutes) // NOLINT
{
    const gw_cfg_t   gw_cfg   = get_gateway_config_default_lan_auth_ruuvi();
    cjson_wrap_str_t json_str = cjson_wrap_str_null();

    cJSON_Hooks hooks = {
        .malloc_fn = &os_malloc,
        .free_fn   = &os_free_internal,
    };
    cJSON_InitHooks(&hooks);
    this->m_malloc_fail_on_cnt = 48;

    ASSERT_FALSE(gw_cfg_json_generate_for_saving(&gw_cfg, &json_str));
    ASSERT_EQ(nullptr, json_str.p_str);
    TEST_CHECK_LOG_RECORD(ESP_LOG_ERROR, string("Can't add json item: remote_cfg_refresh_interval_minutes"));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_TRUE(g_pTestClass->m_mem_alloc_trace.is_empty());
}

TEST_F(TestGwCfgJson, gw_cfg_json_generate_malloc_failed_on_remote_cfg_refresh_interval_minutes_2) // NOLINT
{
    const gw_cfg_t   gw_cfg   = get_gateway_config_default_lan_auth_ruuvi();
    cjson_wrap_str_t json_str = cjson_wrap_str_null();

    cJSON_Hooks hooks = {
        .malloc_fn = &os_malloc,
        .free_fn   = &os_free_internal,
    };
    cJSON_InitHooks(&hooks);
    this->m_malloc_fail_on_cnt = 49;

    ASSERT_FALSE(gw_cfg_json_generate_for_saving(&gw_cfg, &json_str));
    ASSERT_EQ(nullptr, json_str.p_str);
    TEST_CHECK_LOG_RECORD(ESP_LOG_ERROR, string("Can't add json item: remote_cfg_refresh_interval_minutes"));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_TRUE(g_pTestClass->m_mem_alloc_trace.is_empty());
}

TEST_F(TestGwCfgJson, gw_cfg_json_generate_malloc_failed_on_use_http_ruuvi) // NOLINT
{
    const gw_cfg_t   gw_cfg   = get_gateway_config_default_lan_auth_ruuvi();
    cjson_wrap_str_t json_str = cjson_wrap_str_null();

    cJSON_Hooks hooks = {
        .malloc_fn = &os_malloc,
        .free_fn   = &os_free_internal,
    };
    cJSON_InitHooks(&hooks);
    this->m_malloc_fail_on_cnt = 50;

    ASSERT_FALSE(gw_cfg_json_generate_for_saving(&gw_cfg, &json_str));
    ASSERT_EQ(nullptr, json_str.p_str);
    TEST_CHECK_LOG_RECORD(ESP_LOG_ERROR, string("Can't add json item: use_http_ruuvi"));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_TRUE(g_pTestClass->m_mem_alloc_trace.is_empty());
}

TEST_F(TestGwCfgJson, gw_cfg_json_generate_malloc_failed_on_use_http_ruuvi_2) // NOLINT
{
    const gw_cfg_t   gw_cfg   = get_gateway_config_default_lan_auth_ruuvi();
    cjson_wrap_str_t json_str = cjson_wrap_str_null();

    cJSON_Hooks hooks = {
        .malloc_fn = &os_malloc,
        .free_fn   = &os_free_internal,
    };
    cJSON_InitHooks(&hooks);
    this->m_malloc_fail_on_cnt = 51;

    ASSERT_FALSE(gw_cfg_json_generate_for_saving(&gw_cfg, &json_str));
    ASSERT_EQ(nullptr, json_str.p_str);
    TEST_CHECK_LOG_RECORD(ESP_LOG_ERROR, string("Can't add json item: use_http_ruuvi"));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_TRUE(g_pTestClass->m_mem_alloc_trace.is_empty());
}

TEST_F(TestGwCfgJson, gw_cfg_json_generate_malloc_failed_on_use_http) // NOLINT
{
    const gw_cfg_t   gw_cfg   = get_gateway_config_default_lan_auth_ruuvi();
    cjson_wrap_str_t json_str = cjson_wrap_str_null();

    cJSON_Hooks hooks = {
        .malloc_fn = &os_malloc,
        .free_fn   = &os_free_internal,
    };
    cJSON_InitHooks(&hooks);
    this->m_malloc_fail_on_cnt = 52;

    ASSERT_FALSE(gw_cfg_json_generate_for_saving(&gw_cfg, &json_str));
    ASSERT_EQ(nullptr, json_str.p_str);
    TEST_CHECK_LOG_RECORD(ESP_LOG_ERROR, string("Can't add json item: use_http"));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_TRUE(g_pTestClass->m_mem_alloc_trace.is_empty());
}

TEST_F(TestGwCfgJson, gw_cfg_json_generate_malloc_failed_on_use_http_2) // NOLINT
{
    const gw_cfg_t   gw_cfg   = get_gateway_config_default_lan_auth_ruuvi();
    cjson_wrap_str_t json_str = cjson_wrap_str_null();

    cJSON_Hooks hooks = {
        .malloc_fn = &os_malloc,
        .free_fn   = &os_free_internal,
    };
    cJSON_InitHooks(&hooks);
    this->m_malloc_fail_on_cnt = 53;

    ASSERT_FALSE(gw_cfg_json_generate_for_saving(&gw_cfg, &json_str));
    ASSERT_EQ(nullptr, json_str.p_str);
    TEST_CHECK_LOG_RECORD(ESP_LOG_ERROR, string("Can't add json item: use_http"));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_TRUE(g_pTestClass->m_mem_alloc_trace.is_empty());
}

TEST_F(TestGwCfgJson, gw_cfg_json_generate_malloc_failed_on_http_url) // NOLINT
{
    const gw_cfg_t   gw_cfg   = get_gateway_config_default_lan_auth_ruuvi();
    cjson_wrap_str_t json_str = cjson_wrap_str_null();

    cJSON_Hooks hooks = {
        .malloc_fn = &os_malloc,
        .free_fn   = &os_free_internal,
    };
    cJSON_InitHooks(&hooks);
    this->m_malloc_fail_on_cnt = 54;

    ASSERT_FALSE(gw_cfg_json_generate_for_saving(&gw_cfg, &json_str));
    ASSERT_EQ(nullptr, json_str.p_str);
    TEST_CHECK_LOG_RECORD(ESP_LOG_ERROR, string("Can't add json item: http_url"));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_TRUE(g_pTestClass->m_mem_alloc_trace.is_empty());
}

TEST_F(TestGwCfgJson, gw_cfg_json_generate_malloc_failed_on_http_url_2) // NOLINT
{
    const gw_cfg_t   gw_cfg   = get_gateway_config_default_lan_auth_ruuvi();
    cjson_wrap_str_t json_str = cjson_wrap_str_null();

    cJSON_Hooks hooks = {
        .malloc_fn = &os_malloc,
        .free_fn   = &os_free_internal,
    };
    cJSON_InitHooks(&hooks);
    this->m_malloc_fail_on_cnt = 55;

    ASSERT_FALSE(gw_cfg_json_generate_for_saving(&gw_cfg, &json_str));
    ASSERT_EQ(nullptr, json_str.p_str);
    TEST_CHECK_LOG_RECORD(ESP_LOG_ERROR, string("Can't add json item: http_url"));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_TRUE(g_pTestClass->m_mem_alloc_trace.is_empty());
}

TEST_F(TestGwCfgJson, gw_cfg_json_generate_malloc_failed_on_http_url_3) // NOLINT
{
    const gw_cfg_t   gw_cfg   = get_gateway_config_default_lan_auth_ruuvi();
    cjson_wrap_str_t json_str = cjson_wrap_str_null();

    cJSON_Hooks hooks = {
        .malloc_fn = &os_malloc,
        .free_fn   = &os_free_internal,
    };
    cJSON_InitHooks(&hooks);
    this->m_malloc_fail_on_cnt = 56;

    ASSERT_FALSE(gw_cfg_json_generate_for_saving(&gw_cfg, &json_str));
    ASSERT_EQ(nullptr, json_str.p_str);
    TEST_CHECK_LOG_RECORD(ESP_LOG_ERROR, string("Can't add json item: http_url"));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_TRUE(g_pTestClass->m_mem_alloc_trace.is_empty());
}

TEST_F(TestGwCfgJson, gw_cfg_json_generate_malloc_failed_on_http_data_format) // NOLINT
{
    const gw_cfg_t   gw_cfg   = get_gateway_config_default_lan_auth_ruuvi();
    cjson_wrap_str_t json_str = cjson_wrap_str_null();

    cJSON_Hooks hooks = {
        .malloc_fn = &os_malloc,
        .free_fn   = &os_free_internal,
    };
    cJSON_InitHooks(&hooks);
    this->m_malloc_fail_on_cnt = 57;

    ASSERT_FALSE(gw_cfg_json_generate_for_saving(&gw_cfg, &json_str));
    ASSERT_EQ(nullptr, json_str.p_str);
    TEST_CHECK_LOG_RECORD(ESP_LOG_ERROR, string("Can't add json item: http_data_format"));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_TRUE(g_pTestClass->m_mem_alloc_trace.is_empty());
}

TEST_F(TestGwCfgJson, gw_cfg_json_generate_malloc_failed_on_data_format_2) // NOLINT
{
    const gw_cfg_t   gw_cfg   = get_gateway_config_default_lan_auth_ruuvi();
    cjson_wrap_str_t json_str = cjson_wrap_str_null();

    cJSON_Hooks hooks = {
        .malloc_fn = &os_malloc,
        .free_fn   = &os_free_internal,
    };
    cJSON_InitHooks(&hooks);
    this->m_malloc_fail_on_cnt = 58;

    ASSERT_FALSE(gw_cfg_json_generate_for_saving(&gw_cfg, &json_str));
    ASSERT_EQ(nullptr, json_str.p_str);
    TEST_CHECK_LOG_RECORD(ESP_LOG_ERROR, string("Can't add json item: http_data_format"));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_TRUE(g_pTestClass->m_mem_alloc_trace.is_empty());
}

TEST_F(TestGwCfgJson, gw_cfg_json_generate_malloc_failed_on_data_format_3) // NOLINT
{
    const gw_cfg_t   gw_cfg   = get_gateway_config_default_lan_auth_ruuvi();
    cjson_wrap_str_t json_str = cjson_wrap_str_null();

    cJSON_Hooks hooks = {
        .malloc_fn = &os_malloc,
        .free_fn   = &os_free_internal,
    };
    cJSON_InitHooks(&hooks);
    this->m_malloc_fail_on_cnt = 59;

    ASSERT_FALSE(gw_cfg_json_generate_for_saving(&gw_cfg, &json_str));
    ASSERT_EQ(nullptr, json_str.p_str);
    TEST_CHECK_LOG_RECORD(ESP_LOG_ERROR, string("Can't add json item: http_data_format"));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_TRUE(g_pTestClass->m_mem_alloc_trace.is_empty());
}

TEST_F(TestGwCfgJson, gw_cfg_json_generate_malloc_failed_on_http_auth) // NOLINT
{
    const gw_cfg_t   gw_cfg   = get_gateway_config_default_lan_auth_ruuvi();
    cjson_wrap_str_t json_str = cjson_wrap_str_null();

    cJSON_Hooks hooks = {
        .malloc_fn = &os_malloc,
        .free_fn   = &os_free_internal,
    };
    cJSON_InitHooks(&hooks);
    this->m_malloc_fail_on_cnt = 60;

    ASSERT_FALSE(gw_cfg_json_generate_for_saving(&gw_cfg, &json_str));
    ASSERT_EQ(nullptr, json_str.p_str);
    TEST_CHECK_LOG_RECORD(ESP_LOG_ERROR, string("Can't add json item: http_auth"));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_TRUE(g_pTestClass->m_mem_alloc_trace.is_empty());
}

TEST_F(TestGwCfgJson, gw_cfg_json_generate_malloc_failed_on_http_auth_2) // NOLINT
{
    const gw_cfg_t   gw_cfg   = get_gateway_config_default_lan_auth_ruuvi();
    cjson_wrap_str_t json_str = cjson_wrap_str_null();

    cJSON_Hooks hooks = {
        .malloc_fn = &os_malloc,
        .free_fn   = &os_free_internal,
    };
    cJSON_InitHooks(&hooks);
    this->m_malloc_fail_on_cnt = 61;

    ASSERT_FALSE(gw_cfg_json_generate_for_saving(&gw_cfg, &json_str));
    ASSERT_EQ(nullptr, json_str.p_str);
    TEST_CHECK_LOG_RECORD(ESP_LOG_ERROR, string("Can't add json item: http_auth"));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_TRUE(g_pTestClass->m_mem_alloc_trace.is_empty());
}

TEST_F(TestGwCfgJson, gw_cfg_json_generate_malloc_failed_on_http_auth_3) // NOLINT
{
    const gw_cfg_t   gw_cfg   = get_gateway_config_default_lan_auth_ruuvi();
    cjson_wrap_str_t json_str = cjson_wrap_str_null();

    cJSON_Hooks hooks = {
        .malloc_fn = &os_malloc,
        .free_fn   = &os_free_internal,
    };
    cJSON_InitHooks(&hooks);
    this->m_malloc_fail_on_cnt = 62;

    ASSERT_FALSE(gw_cfg_json_generate_for_saving(&gw_cfg, &json_str));
    ASSERT_EQ(nullptr, json_str.p_str);
    TEST_CHECK_LOG_RECORD(ESP_LOG_ERROR, string("Can't add json item: http_auth"));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_TRUE(g_pTestClass->m_mem_alloc_trace.is_empty());
}

TEST_F(TestGwCfgJson, gw_cfg_json_generate_malloc_failed_on_use_http_stat) // NOLINT
{
    const gw_cfg_t   gw_cfg   = get_gateway_config_default_lan_auth_ruuvi();
    cjson_wrap_str_t json_str = cjson_wrap_str_null();

    cJSON_Hooks hooks = {
        .malloc_fn = &os_malloc,
        .free_fn   = &os_free_internal,
    };
    cJSON_InitHooks(&hooks);
    this->m_malloc_fail_on_cnt = 63;

    ASSERT_FALSE(gw_cfg_json_generate_for_saving(&gw_cfg, &json_str));
    ASSERT_EQ(nullptr, json_str.p_str);
    TEST_CHECK_LOG_RECORD(ESP_LOG_ERROR, string("Can't add json item: use_http_stat"));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_TRUE(g_pTestClass->m_mem_alloc_trace.is_empty());
}

TEST_F(TestGwCfgJson, gw_cfg_json_generate_malloc_failed_on_use_http_stat_2) // NOLINT
{
    const gw_cfg_t   gw_cfg   = get_gateway_config_default_lan_auth_ruuvi();
    cjson_wrap_str_t json_str = cjson_wrap_str_null();

    cJSON_Hooks hooks = {
        .malloc_fn = &os_malloc,
        .free_fn   = &os_free_internal,
    };
    cJSON_InitHooks(&hooks);
    this->m_malloc_fail_on_cnt = 64;

    ASSERT_FALSE(gw_cfg_json_generate_for_saving(&gw_cfg, &json_str));
    ASSERT_EQ(nullptr, json_str.p_str);
    TEST_CHECK_LOG_RECORD(ESP_LOG_ERROR, string("Can't add json item: use_http_stat"));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_TRUE(g_pTestClass->m_mem_alloc_trace.is_empty());
}

TEST_F(TestGwCfgJson, gw_cfg_json_generate_malloc_failed_on_http_stat_url) // NOLINT
{
    const gw_cfg_t   gw_cfg   = get_gateway_config_default_lan_auth_ruuvi();
    cjson_wrap_str_t json_str = cjson_wrap_str_null();

    cJSON_Hooks hooks = {
        .malloc_fn = &os_malloc,
        .free_fn   = &os_free_internal,
    };
    cJSON_InitHooks(&hooks);
    this->m_malloc_fail_on_cnt = 65;

    ASSERT_FALSE(gw_cfg_json_generate_for_saving(&gw_cfg, &json_str));
    ASSERT_EQ(nullptr, json_str.p_str);
    TEST_CHECK_LOG_RECORD(ESP_LOG_ERROR, string("Can't add json item: http_stat_url"));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_TRUE(g_pTestClass->m_mem_alloc_trace.is_empty());
}

TEST_F(TestGwCfgJson, gw_cfg_json_generate_malloc_failed_on_http_stat_url_2) // NOLINT
{
    const gw_cfg_t   gw_cfg   = get_gateway_config_default_lan_auth_ruuvi();
    cjson_wrap_str_t json_str = cjson_wrap_str_null();

    cJSON_Hooks hooks = {
        .malloc_fn = &os_malloc,
        .free_fn   = &os_free_internal,
    };
    cJSON_InitHooks(&hooks);
    this->m_malloc_fail_on_cnt = 66;

    ASSERT_FALSE(gw_cfg_json_generate_for_saving(&gw_cfg, &json_str));
    ASSERT_EQ(nullptr, json_str.p_str);
    TEST_CHECK_LOG_RECORD(ESP_LOG_ERROR, string("Can't add json item: http_stat_url"));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_TRUE(g_pTestClass->m_mem_alloc_trace.is_empty());
}

TEST_F(TestGwCfgJson, gw_cfg_json_generate_malloc_failed_on_http_stat_url_3) // NOLINT
{
    const gw_cfg_t   gw_cfg   = get_gateway_config_default_lan_auth_ruuvi();
    cjson_wrap_str_t json_str = cjson_wrap_str_null();

    cJSON_Hooks hooks = {
        .malloc_fn = &os_malloc,
        .free_fn   = &os_free_internal,
    };
    cJSON_InitHooks(&hooks);
    this->m_malloc_fail_on_cnt = 67;

    ASSERT_FALSE(gw_cfg_json_generate_for_saving(&gw_cfg, &json_str));
    ASSERT_EQ(nullptr, json_str.p_str);
    TEST_CHECK_LOG_RECORD(ESP_LOG_ERROR, string("Can't add json item: http_stat_url"));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_TRUE(g_pTestClass->m_mem_alloc_trace.is_empty());
}

TEST_F(TestGwCfgJson, gw_cfg_json_generate_malloc_failed_on_http_stat_user) // NOLINT
{
    const gw_cfg_t   gw_cfg   = get_gateway_config_default_lan_auth_ruuvi();
    cjson_wrap_str_t json_str = cjson_wrap_str_null();

    cJSON_Hooks hooks = {
        .malloc_fn = &os_malloc,
        .free_fn   = &os_free_internal,
    };
    cJSON_InitHooks(&hooks);
    this->m_malloc_fail_on_cnt = 68;

    ASSERT_FALSE(gw_cfg_json_generate_for_saving(&gw_cfg, &json_str));
    ASSERT_EQ(nullptr, json_str.p_str);
    TEST_CHECK_LOG_RECORD(ESP_LOG_ERROR, string("Can't add json item: http_stat_user"));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_TRUE(g_pTestClass->m_mem_alloc_trace.is_empty());
}

TEST_F(TestGwCfgJson, gw_cfg_json_generate_malloc_failed_on_http_stat_user_2) // NOLINT
{
    const gw_cfg_t   gw_cfg   = get_gateway_config_default_lan_auth_ruuvi();
    cjson_wrap_str_t json_str = cjson_wrap_str_null();

    cJSON_Hooks hooks = {
        .malloc_fn = &os_malloc,
        .free_fn   = &os_free_internal,
    };
    cJSON_InitHooks(&hooks);
    this->m_malloc_fail_on_cnt = 69;

    ASSERT_FALSE(gw_cfg_json_generate_for_saving(&gw_cfg, &json_str));
    ASSERT_EQ(nullptr, json_str.p_str);
    TEST_CHECK_LOG_RECORD(ESP_LOG_ERROR, string("Can't add json item: http_stat_user"));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_TRUE(g_pTestClass->m_mem_alloc_trace.is_empty());
}

TEST_F(TestGwCfgJson, gw_cfg_json_generate_malloc_failed_on_http_stat_user_3) // NOLINT
{
    const gw_cfg_t   gw_cfg   = get_gateway_config_default_lan_auth_ruuvi();
    cjson_wrap_str_t json_str = cjson_wrap_str_null();

    cJSON_Hooks hooks = {
        .malloc_fn = &os_malloc,
        .free_fn   = &os_free_internal,
    };
    cJSON_InitHooks(&hooks);
    this->m_malloc_fail_on_cnt = 70;

    ASSERT_FALSE(gw_cfg_json_generate_for_saving(&gw_cfg, &json_str));
    ASSERT_EQ(nullptr, json_str.p_str);
    TEST_CHECK_LOG_RECORD(ESP_LOG_ERROR, string("Can't add json item: http_stat_user"));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_TRUE(g_pTestClass->m_mem_alloc_trace.is_empty());
}

TEST_F(TestGwCfgJson, gw_cfg_json_generate_malloc_failed_on_http_stat_pass) // NOLINT
{
    const gw_cfg_t   gw_cfg   = get_gateway_config_default_lan_auth_ruuvi();
    cjson_wrap_str_t json_str = cjson_wrap_str_null();

    cJSON_Hooks hooks = {
        .malloc_fn = &os_malloc,
        .free_fn   = &os_free_internal,
    };
    cJSON_InitHooks(&hooks);
    this->m_malloc_fail_on_cnt = 71;

    ASSERT_FALSE(gw_cfg_json_generate_for_saving(&gw_cfg, &json_str));
    ASSERT_EQ(nullptr, json_str.p_str);
    TEST_CHECK_LOG_RECORD(ESP_LOG_ERROR, string("Can't add json item: http_stat_pass"));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_TRUE(g_pTestClass->m_mem_alloc_trace.is_empty());
}

TEST_F(TestGwCfgJson, gw_cfg_json_generate_malloc_failed_on_http_stat_pass_2) // NOLINT
{
    const gw_cfg_t   gw_cfg   = get_gateway_config_default_lan_auth_ruuvi();
    cjson_wrap_str_t json_str = cjson_wrap_str_null();

    cJSON_Hooks hooks = {
        .malloc_fn = &os_malloc,
        .free_fn   = &os_free_internal,
    };
    cJSON_InitHooks(&hooks);
    this->m_malloc_fail_on_cnt = 72;

    ASSERT_FALSE(gw_cfg_json_generate_for_saving(&gw_cfg, &json_str));
    ASSERT_EQ(nullptr, json_str.p_str);
    TEST_CHECK_LOG_RECORD(ESP_LOG_ERROR, string("Can't add json item: http_stat_pass"));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_TRUE(g_pTestClass->m_mem_alloc_trace.is_empty());
}

TEST_F(TestGwCfgJson, gw_cfg_json_generate_malloc_failed_on_http_stat_pass_3) // NOLINT
{
    const gw_cfg_t   gw_cfg   = get_gateway_config_default_lan_auth_ruuvi();
    cjson_wrap_str_t json_str = cjson_wrap_str_null();

    cJSON_Hooks hooks = {
        .malloc_fn = &os_malloc,
        .free_fn   = &os_free_internal,
    };
    cJSON_InitHooks(&hooks);
    this->m_malloc_fail_on_cnt = 73;

    ASSERT_FALSE(gw_cfg_json_generate_for_saving(&gw_cfg, &json_str));
    ASSERT_EQ(nullptr, json_str.p_str);
    TEST_CHECK_LOG_RECORD(ESP_LOG_ERROR, string("Can't add json item: http_stat_pass"));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_TRUE(g_pTestClass->m_mem_alloc_trace.is_empty());
}

TEST_F(TestGwCfgJson, gw_cfg_json_generate_malloc_failed_on_http_stat_use_ssl_client_cert) // NOLINT
{
    const gw_cfg_t   gw_cfg   = get_gateway_config_default_lan_auth_ruuvi();
    cjson_wrap_str_t json_str = cjson_wrap_str_null();

    cJSON_Hooks hooks = {
        .malloc_fn = &os_malloc,
        .free_fn   = &os_free_internal,
    };
    cJSON_InitHooks(&hooks);
    this->m_malloc_fail_on_cnt = 74;

    ASSERT_FALSE(gw_cfg_json_generate_for_saving(&gw_cfg, &json_str));
    ASSERT_EQ(nullptr, json_str.p_str);
    TEST_CHECK_LOG_RECORD(ESP_LOG_ERROR, string("Can't add json item: http_stat_use_ssl_client_cert"));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_TRUE(g_pTestClass->m_mem_alloc_trace.is_empty());
}

TEST_F(TestGwCfgJson, gw_cfg_json_generate_malloc_failed_on_http_stat_use_ssl_client_cert_2) // NOLINT
{
    const gw_cfg_t   gw_cfg   = get_gateway_config_default_lan_auth_ruuvi();
    cjson_wrap_str_t json_str = cjson_wrap_str_null();

    cJSON_Hooks hooks = {
        .malloc_fn = &os_malloc,
        .free_fn   = &os_free_internal,
    };
    cJSON_InitHooks(&hooks);
    this->m_malloc_fail_on_cnt = 75;

    ASSERT_FALSE(gw_cfg_json_generate_for_saving(&gw_cfg, &json_str));
    ASSERT_EQ(nullptr, json_str.p_str);
    TEST_CHECK_LOG_RECORD(ESP_LOG_ERROR, string("Can't add json item: http_stat_use_ssl_client_cert"));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_TRUE(g_pTestClass->m_mem_alloc_trace.is_empty());
}

TEST_F(TestGwCfgJson, gw_cfg_json_generate_malloc_failed_on_http_stat_use_ssl_server_cert) // NOLINT
{
    const gw_cfg_t   gw_cfg   = get_gateway_config_default_lan_auth_ruuvi();
    cjson_wrap_str_t json_str = cjson_wrap_str_null();

    cJSON_Hooks hooks = {
        .malloc_fn = &os_malloc,
        .free_fn   = &os_free_internal,
    };
    cJSON_InitHooks(&hooks);
    this->m_malloc_fail_on_cnt = 76;

    ASSERT_FALSE(gw_cfg_json_generate_for_saving(&gw_cfg, &json_str));
    ASSERT_EQ(nullptr, json_str.p_str);
    TEST_CHECK_LOG_RECORD(ESP_LOG_ERROR, string("Can't add json item: http_stat_use_ssl_server_cert"));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_TRUE(g_pTestClass->m_mem_alloc_trace.is_empty());
}

TEST_F(TestGwCfgJson, gw_cfg_json_generate_malloc_failed_on_http_stat_use_ssl_server_cert_2) // NOLINT
{
    const gw_cfg_t   gw_cfg   = get_gateway_config_default_lan_auth_ruuvi();
    cjson_wrap_str_t json_str = cjson_wrap_str_null();

    cJSON_Hooks hooks = {
        .malloc_fn = &os_malloc,
        .free_fn   = &os_free_internal,
    };
    cJSON_InitHooks(&hooks);
    this->m_malloc_fail_on_cnt = 76;

    ASSERT_FALSE(gw_cfg_json_generate_for_saving(&gw_cfg, &json_str));
    ASSERT_EQ(nullptr, json_str.p_str);
    TEST_CHECK_LOG_RECORD(ESP_LOG_ERROR, string("Can't add json item: http_stat_use_ssl_server_cert"));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_TRUE(g_pTestClass->m_mem_alloc_trace.is_empty());
}

TEST_F(TestGwCfgJson, gw_cfg_json_generate_malloc_failed_on_use_mqtt) // NOLINT
{
    const gw_cfg_t   gw_cfg   = get_gateway_config_default_lan_auth_ruuvi();
    cjson_wrap_str_t json_str = cjson_wrap_str_null();

    cJSON_Hooks hooks = {
        .malloc_fn = &os_malloc,
        .free_fn   = &os_free_internal,
    };
    cJSON_InitHooks(&hooks);
    this->m_malloc_fail_on_cnt = 78;

    ASSERT_FALSE(gw_cfg_json_generate_for_saving(&gw_cfg, &json_str));
    ASSERT_EQ(nullptr, json_str.p_str);
    TEST_CHECK_LOG_RECORD(ESP_LOG_ERROR, string("Can't add json item: use_mqtt"));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_TRUE(g_pTestClass->m_mem_alloc_trace.is_empty());
}

TEST_F(TestGwCfgJson, gw_cfg_json_generate_malloc_failed_on_use_mqtt_2) // NOLINT
{
    const gw_cfg_t   gw_cfg   = get_gateway_config_default_lan_auth_ruuvi();
    cjson_wrap_str_t json_str = cjson_wrap_str_null();

    cJSON_Hooks hooks = {
        .malloc_fn = &os_malloc,
        .free_fn   = &os_free_internal,
    };
    cJSON_InitHooks(&hooks);
    this->m_malloc_fail_on_cnt = 79;

    ASSERT_FALSE(gw_cfg_json_generate_for_saving(&gw_cfg, &json_str));
    ASSERT_EQ(nullptr, json_str.p_str);
    TEST_CHECK_LOG_RECORD(ESP_LOG_ERROR, string("Can't add json item: use_mqtt"));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_TRUE(g_pTestClass->m_mem_alloc_trace.is_empty());
}

TEST_F(TestGwCfgJson, gw_cfg_json_generate_malloc_failed_on_mqtt_disable_retained_messages) // NOLINT
{
    const gw_cfg_t   gw_cfg   = get_gateway_config_default_lan_auth_ruuvi();
    cjson_wrap_str_t json_str = cjson_wrap_str_null();

    cJSON_Hooks hooks = {
        .malloc_fn = &os_malloc,
        .free_fn   = &os_free_internal,
    };
    cJSON_InitHooks(&hooks);
    this->m_malloc_fail_on_cnt = 80;

    ASSERT_FALSE(gw_cfg_json_generate_for_saving(&gw_cfg, &json_str));
    ASSERT_EQ(nullptr, json_str.p_str);
    TEST_CHECK_LOG_RECORD(ESP_LOG_ERROR, string("Can't add json item: mqtt_disable_retained_messages"));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_TRUE(g_pTestClass->m_mem_alloc_trace.is_empty());
}

TEST_F(TestGwCfgJson, gw_cfg_json_generate_malloc_failed_on_mqtt_disable_retained_messages_2) // NOLINT
{
    const gw_cfg_t   gw_cfg   = get_gateway_config_default_lan_auth_ruuvi();
    cjson_wrap_str_t json_str = cjson_wrap_str_null();

    cJSON_Hooks hooks = {
        .malloc_fn = &os_malloc,
        .free_fn   = &os_free_internal,
    };
    cJSON_InitHooks(&hooks);
    this->m_malloc_fail_on_cnt = 81;

    ASSERT_FALSE(gw_cfg_json_generate_for_saving(&gw_cfg, &json_str));
    ASSERT_EQ(nullptr, json_str.p_str);
    TEST_CHECK_LOG_RECORD(ESP_LOG_ERROR, string("Can't add json item: mqtt_disable_retained_messages"));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_TRUE(g_pTestClass->m_mem_alloc_trace.is_empty());
}

TEST_F(TestGwCfgJson, gw_cfg_json_generate_malloc_failed_on_mqtt_trnasport) // NOLINT
{
    const gw_cfg_t   gw_cfg   = get_gateway_config_default_lan_auth_ruuvi();
    cjson_wrap_str_t json_str = cjson_wrap_str_null();

    cJSON_Hooks hooks = {
        .malloc_fn = &os_malloc,
        .free_fn   = &os_free_internal,
    };
    cJSON_InitHooks(&hooks);
    this->m_malloc_fail_on_cnt = 82;

    ASSERT_FALSE(gw_cfg_json_generate_for_saving(&gw_cfg, &json_str));
    ASSERT_EQ(nullptr, json_str.p_str);
    TEST_CHECK_LOG_RECORD(ESP_LOG_ERROR, string("Can't add json item: mqtt_transport"));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_TRUE(g_pTestClass->m_mem_alloc_trace.is_empty());
}

TEST_F(TestGwCfgJson, gw_cfg_json_generate_malloc_failed_on_mqtt_trnasport_2) // NOLINT
{
    const gw_cfg_t   gw_cfg   = get_gateway_config_default_lan_auth_ruuvi();
    cjson_wrap_str_t json_str = cjson_wrap_str_null();

    cJSON_Hooks hooks = {
        .malloc_fn = &os_malloc,
        .free_fn   = &os_free_internal,
    };
    cJSON_InitHooks(&hooks);
    this->m_malloc_fail_on_cnt = 83;

    ASSERT_FALSE(gw_cfg_json_generate_for_saving(&gw_cfg, &json_str));
    ASSERT_EQ(nullptr, json_str.p_str);
    TEST_CHECK_LOG_RECORD(ESP_LOG_ERROR, string("Can't add json item: mqtt_transport"));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_TRUE(g_pTestClass->m_mem_alloc_trace.is_empty());
}

TEST_F(TestGwCfgJson, gw_cfg_json_generate_malloc_failed_on_mqtt_trnasport_3) // NOLINT
{
    const gw_cfg_t   gw_cfg   = get_gateway_config_default_lan_auth_ruuvi();
    cjson_wrap_str_t json_str = cjson_wrap_str_null();

    cJSON_Hooks hooks = {
        .malloc_fn = &os_malloc,
        .free_fn   = &os_free_internal,
    };
    cJSON_InitHooks(&hooks);
    this->m_malloc_fail_on_cnt = 84;

    ASSERT_FALSE(gw_cfg_json_generate_for_saving(&gw_cfg, &json_str));
    ASSERT_EQ(nullptr, json_str.p_str);
    TEST_CHECK_LOG_RECORD(ESP_LOG_ERROR, string("Can't add json item: mqtt_transport"));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_TRUE(g_pTestClass->m_mem_alloc_trace.is_empty());
}

TEST_F(TestGwCfgJson, gw_cfg_json_generate_malloc_failed_on_mqtt_server) // NOLINT
{
    const gw_cfg_t   gw_cfg   = get_gateway_config_default_lan_auth_ruuvi();
    cjson_wrap_str_t json_str = cjson_wrap_str_null();

    cJSON_Hooks hooks = {
        .malloc_fn = &os_malloc,
        .free_fn   = &os_free_internal,
    };
    cJSON_InitHooks(&hooks);
    this->m_malloc_fail_on_cnt = 85;

    ASSERT_FALSE(gw_cfg_json_generate_for_saving(&gw_cfg, &json_str));
    ASSERT_EQ(nullptr, json_str.p_str);
    TEST_CHECK_LOG_RECORD(ESP_LOG_ERROR, string("Can't add json item: mqtt_server"));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_TRUE(g_pTestClass->m_mem_alloc_trace.is_empty());
}

TEST_F(TestGwCfgJson, gw_cfg_json_generate_malloc_failed_on_mqtt_server_2) // NOLINT
{
    const gw_cfg_t   gw_cfg   = get_gateway_config_default_lan_auth_ruuvi();
    cjson_wrap_str_t json_str = cjson_wrap_str_null();

    cJSON_Hooks hooks = {
        .malloc_fn = &os_malloc,
        .free_fn   = &os_free_internal,
    };
    cJSON_InitHooks(&hooks);
    this->m_malloc_fail_on_cnt = 86;

    ASSERT_FALSE(gw_cfg_json_generate_for_saving(&gw_cfg, &json_str));
    ASSERT_EQ(nullptr, json_str.p_str);
    TEST_CHECK_LOG_RECORD(ESP_LOG_ERROR, string("Can't add json item: mqtt_server"));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_TRUE(g_pTestClass->m_mem_alloc_trace.is_empty());
}

TEST_F(TestGwCfgJson, gw_cfg_json_generate_malloc_failed_on_mqtt_server_3) // NOLINT
{
    const gw_cfg_t   gw_cfg   = get_gateway_config_default_lan_auth_ruuvi();
    cjson_wrap_str_t json_str = cjson_wrap_str_null();

    cJSON_Hooks hooks = {
        .malloc_fn = &os_malloc,
        .free_fn   = &os_free_internal,
    };
    cJSON_InitHooks(&hooks);
    this->m_malloc_fail_on_cnt = 87;

    ASSERT_FALSE(gw_cfg_json_generate_for_saving(&gw_cfg, &json_str));
    ASSERT_EQ(nullptr, json_str.p_str);
    TEST_CHECK_LOG_RECORD(ESP_LOG_ERROR, string("Can't add json item: mqtt_server"));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_TRUE(g_pTestClass->m_mem_alloc_trace.is_empty());
}

TEST_F(TestGwCfgJson, gw_cfg_json_generate_malloc_failed_on_mqtt_port) // NOLINT
{
    const gw_cfg_t   gw_cfg   = get_gateway_config_default_lan_auth_ruuvi();
    cjson_wrap_str_t json_str = cjson_wrap_str_null();

    cJSON_Hooks hooks = {
        .malloc_fn = &os_malloc,
        .free_fn   = &os_free_internal,
    };
    cJSON_InitHooks(&hooks);
    this->m_malloc_fail_on_cnt = 88;

    ASSERT_FALSE(gw_cfg_json_generate_for_saving(&gw_cfg, &json_str));
    ASSERT_EQ(nullptr, json_str.p_str);
    TEST_CHECK_LOG_RECORD(ESP_LOG_ERROR, string("Can't add json item: mqtt_port"));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_TRUE(g_pTestClass->m_mem_alloc_trace.is_empty());
}

TEST_F(TestGwCfgJson, gw_cfg_json_generate_malloc_failed_on_mqtt_port_2) // NOLINT
{
    const gw_cfg_t   gw_cfg   = get_gateway_config_default_lan_auth_ruuvi();
    cjson_wrap_str_t json_str = cjson_wrap_str_null();

    cJSON_Hooks hooks = {
        .malloc_fn = &os_malloc,
        .free_fn   = &os_free_internal,
    };
    cJSON_InitHooks(&hooks);
    this->m_malloc_fail_on_cnt = 89;

    ASSERT_FALSE(gw_cfg_json_generate_for_saving(&gw_cfg, &json_str));
    ASSERT_EQ(nullptr, json_str.p_str);
    TEST_CHECK_LOG_RECORD(ESP_LOG_ERROR, string("Can't add json item: mqtt_port"));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_TRUE(g_pTestClass->m_mem_alloc_trace.is_empty());
}

TEST_F(TestGwCfgJson, gw_cfg_json_generate_malloc_failed_on_mqtt_mqtt_sending_interval) // NOLINT
{
    const gw_cfg_t   gw_cfg   = get_gateway_config_default_lan_auth_ruuvi();
    cjson_wrap_str_t json_str = cjson_wrap_str_null();

    cJSON_Hooks hooks = {
        .malloc_fn = &os_malloc,
        .free_fn   = &os_free_internal,
    };
    cJSON_InitHooks(&hooks);
    this->m_malloc_fail_on_cnt = 90;

    ASSERT_FALSE(gw_cfg_json_generate_for_saving(&gw_cfg, &json_str));
    ASSERT_EQ(nullptr, json_str.p_str);
    TEST_CHECK_LOG_RECORD(ESP_LOG_ERROR, string("Can't add json item: mqtt_sending_interval"));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_TRUE(g_pTestClass->m_mem_alloc_trace.is_empty());
}

TEST_F(TestGwCfgJson, gw_cfg_json_generate_malloc_failed_on_mqtt_sending_interval_2) // NOLINT
{
    const gw_cfg_t   gw_cfg   = get_gateway_config_default_lan_auth_ruuvi();
    cjson_wrap_str_t json_str = cjson_wrap_str_null();

    cJSON_Hooks hooks = {
        .malloc_fn = &os_malloc,
        .free_fn   = &os_free_internal,
    };
    cJSON_InitHooks(&hooks);
    this->m_malloc_fail_on_cnt = 91;

    ASSERT_FALSE(gw_cfg_json_generate_for_saving(&gw_cfg, &json_str));
    ASSERT_EQ(nullptr, json_str.p_str);
    TEST_CHECK_LOG_RECORD(ESP_LOG_ERROR, string("Can't add json item: mqtt_sending_interval"));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_TRUE(g_pTestClass->m_mem_alloc_trace.is_empty());
}

TEST_F(TestGwCfgJson, gw_cfg_json_generate_malloc_failed_on_mqtt_prefix) // NOLINT
{
    const gw_cfg_t   gw_cfg   = get_gateway_config_default_lan_auth_ruuvi();
    cjson_wrap_str_t json_str = cjson_wrap_str_null();

    cJSON_Hooks hooks = {
        .malloc_fn = &os_malloc,
        .free_fn   = &os_free_internal,
    };
    cJSON_InitHooks(&hooks);
    this->m_malloc_fail_on_cnt = 92;

    ASSERT_FALSE(gw_cfg_json_generate_for_saving(&gw_cfg, &json_str));
    ASSERT_EQ(nullptr, json_str.p_str);
    TEST_CHECK_LOG_RECORD(ESP_LOG_ERROR, string("Can't add json item: mqtt_prefix"));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_TRUE(g_pTestClass->m_mem_alloc_trace.is_empty());
}

TEST_F(TestGwCfgJson, gw_cfg_json_generate_malloc_failed_on_mqtt_prefix_2) // NOLINT
{
    const gw_cfg_t   gw_cfg   = get_gateway_config_default_lan_auth_ruuvi();
    cjson_wrap_str_t json_str = cjson_wrap_str_null();

    cJSON_Hooks hooks = {
        .malloc_fn = &os_malloc,
        .free_fn   = &os_free_internal,
    };
    cJSON_InitHooks(&hooks);
    this->m_malloc_fail_on_cnt = 93;

    ASSERT_FALSE(gw_cfg_json_generate_for_saving(&gw_cfg, &json_str));
    ASSERT_EQ(nullptr, json_str.p_str);
    TEST_CHECK_LOG_RECORD(ESP_LOG_ERROR, string("Can't add json item: mqtt_prefix"));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_TRUE(g_pTestClass->m_mem_alloc_trace.is_empty());
}

TEST_F(TestGwCfgJson, gw_cfg_json_generate_malloc_failed_on_mqtt_prefix_3) // NOLINT
{
    const gw_cfg_t   gw_cfg   = get_gateway_config_default_lan_auth_ruuvi();
    cjson_wrap_str_t json_str = cjson_wrap_str_null();

    cJSON_Hooks hooks = {
        .malloc_fn = &os_malloc,
        .free_fn   = &os_free_internal,
    };
    cJSON_InitHooks(&hooks);
    this->m_malloc_fail_on_cnt = 94;

    ASSERT_FALSE(gw_cfg_json_generate_for_saving(&gw_cfg, &json_str));
    ASSERT_EQ(nullptr, json_str.p_str);
    TEST_CHECK_LOG_RECORD(ESP_LOG_ERROR, string("Can't add json item: mqtt_prefix"));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_TRUE(g_pTestClass->m_mem_alloc_trace.is_empty());
}

TEST_F(TestGwCfgJson, gw_cfg_json_generate_malloc_failed_on_mqtt_client_id) // NOLINT
{
    const gw_cfg_t   gw_cfg   = get_gateway_config_default_lan_auth_ruuvi();
    cjson_wrap_str_t json_str = cjson_wrap_str_null();

    cJSON_Hooks hooks = {
        .malloc_fn = &os_malloc,
        .free_fn   = &os_free_internal,
    };
    cJSON_InitHooks(&hooks);
    this->m_malloc_fail_on_cnt = 95;

    ASSERT_FALSE(gw_cfg_json_generate_for_saving(&gw_cfg, &json_str));
    ASSERT_EQ(nullptr, json_str.p_str);
    TEST_CHECK_LOG_RECORD(ESP_LOG_ERROR, string("Can't add json item: mqtt_client_id"));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_TRUE(g_pTestClass->m_mem_alloc_trace.is_empty());
}

TEST_F(TestGwCfgJson, gw_cfg_json_generate_malloc_failed_on_mqtt_client_id_2) // NOLINT
{
    const gw_cfg_t   gw_cfg   = get_gateway_config_default_lan_auth_ruuvi();
    cjson_wrap_str_t json_str = cjson_wrap_str_null();

    cJSON_Hooks hooks = {
        .malloc_fn = &os_malloc,
        .free_fn   = &os_free_internal,
    };
    cJSON_InitHooks(&hooks);
    this->m_malloc_fail_on_cnt = 96;

    ASSERT_FALSE(gw_cfg_json_generate_for_saving(&gw_cfg, &json_str));
    ASSERT_EQ(nullptr, json_str.p_str);
    TEST_CHECK_LOG_RECORD(ESP_LOG_ERROR, string("Can't add json item: mqtt_client_id"));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_TRUE(g_pTestClass->m_mem_alloc_trace.is_empty());
}

TEST_F(TestGwCfgJson, gw_cfg_json_generate_malloc_failed_on_mqtt_client_id_3) // NOLINT
{
    const gw_cfg_t   gw_cfg   = get_gateway_config_default_lan_auth_ruuvi();
    cjson_wrap_str_t json_str = cjson_wrap_str_null();

    cJSON_Hooks hooks = {
        .malloc_fn = &os_malloc,
        .free_fn   = &os_free_internal,
    };
    cJSON_InitHooks(&hooks);
    this->m_malloc_fail_on_cnt = 97;

    ASSERT_FALSE(gw_cfg_json_generate_for_saving(&gw_cfg, &json_str));
    ASSERT_EQ(nullptr, json_str.p_str);
    TEST_CHECK_LOG_RECORD(ESP_LOG_ERROR, string("Can't add json item: mqtt_client_id"));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_TRUE(g_pTestClass->m_mem_alloc_trace.is_empty());
}

TEST_F(TestGwCfgJson, gw_cfg_json_generate_malloc_failed_on_mqtt_user) // NOLINT
{
    const gw_cfg_t   gw_cfg   = get_gateway_config_default_lan_auth_ruuvi();
    cjson_wrap_str_t json_str = cjson_wrap_str_null();

    cJSON_Hooks hooks = {
        .malloc_fn = &os_malloc,
        .free_fn   = &os_free_internal,
    };
    cJSON_InitHooks(&hooks);
    this->m_malloc_fail_on_cnt = 98;

    ASSERT_FALSE(gw_cfg_json_generate_for_saving(&gw_cfg, &json_str));
    ASSERT_EQ(nullptr, json_str.p_str);
    TEST_CHECK_LOG_RECORD(ESP_LOG_ERROR, string("Can't add json item: mqtt_user"));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_TRUE(g_pTestClass->m_mem_alloc_trace.is_empty());
}

TEST_F(TestGwCfgJson, gw_cfg_json_generate_malloc_failed_on_mqtt_user_2) // NOLINT
{
    const gw_cfg_t   gw_cfg   = get_gateway_config_default_lan_auth_ruuvi();
    cjson_wrap_str_t json_str = cjson_wrap_str_null();

    cJSON_Hooks hooks = {
        .malloc_fn = &os_malloc,
        .free_fn   = &os_free_internal,
    };
    cJSON_InitHooks(&hooks);
    this->m_malloc_fail_on_cnt = 99;

    ASSERT_FALSE(gw_cfg_json_generate_for_saving(&gw_cfg, &json_str));
    ASSERT_EQ(nullptr, json_str.p_str);
    TEST_CHECK_LOG_RECORD(ESP_LOG_ERROR, string("Can't add json item: mqtt_user"));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_TRUE(g_pTestClass->m_mem_alloc_trace.is_empty());
}

TEST_F(TestGwCfgJson, gw_cfg_json_generate_malloc_failed_on_mqtt_user_3) // NOLINT
{
    const gw_cfg_t   gw_cfg   = get_gateway_config_default_lan_auth_ruuvi();
    cjson_wrap_str_t json_str = cjson_wrap_str_null();

    cJSON_Hooks hooks = {
        .malloc_fn = &os_malloc,
        .free_fn   = &os_free_internal,
    };
    cJSON_InitHooks(&hooks);
    this->m_malloc_fail_on_cnt = 100;

    ASSERT_FALSE(gw_cfg_json_generate_for_saving(&gw_cfg, &json_str));
    ASSERT_EQ(nullptr, json_str.p_str);
    TEST_CHECK_LOG_RECORD(ESP_LOG_ERROR, string("Can't add json item: mqtt_user"));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_TRUE(g_pTestClass->m_mem_alloc_trace.is_empty());
}

TEST_F(TestGwCfgJson, gw_cfg_json_generate_malloc_failed_on_mqtt_pass) // NOLINT
{
    const gw_cfg_t   gw_cfg   = get_gateway_config_default_lan_auth_ruuvi();
    cjson_wrap_str_t json_str = cjson_wrap_str_null();

    cJSON_Hooks hooks = {
        .malloc_fn = &os_malloc,
        .free_fn   = &os_free_internal,
    };
    cJSON_InitHooks(&hooks);
    this->m_malloc_fail_on_cnt = 101;

    ASSERT_FALSE(gw_cfg_json_generate_for_saving(&gw_cfg, &json_str));
    ASSERT_EQ(nullptr, json_str.p_str);
    TEST_CHECK_LOG_RECORD(ESP_LOG_ERROR, string("Can't add json item: mqtt_pass"));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_TRUE(g_pTestClass->m_mem_alloc_trace.is_empty());
}

TEST_F(TestGwCfgJson, gw_cfg_json_generate_malloc_failed_on_mqtt_pass_2) // NOLINT
{
    const gw_cfg_t   gw_cfg   = get_gateway_config_default_lan_auth_ruuvi();
    cjson_wrap_str_t json_str = cjson_wrap_str_null();

    cJSON_Hooks hooks = {
        .malloc_fn = &os_malloc,
        .free_fn   = &os_free_internal,
    };
    cJSON_InitHooks(&hooks);
    this->m_malloc_fail_on_cnt = 102;

    ASSERT_FALSE(gw_cfg_json_generate_for_saving(&gw_cfg, &json_str));
    ASSERT_EQ(nullptr, json_str.p_str);
    TEST_CHECK_LOG_RECORD(ESP_LOG_ERROR, string("Can't add json item: mqtt_pass"));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_TRUE(g_pTestClass->m_mem_alloc_trace.is_empty());
}

TEST_F(TestGwCfgJson, gw_cfg_json_generate_malloc_failed_on_mqtt_pass_3) // NOLINT
{
    const gw_cfg_t   gw_cfg   = get_gateway_config_default_lan_auth_ruuvi();
    cjson_wrap_str_t json_str = cjson_wrap_str_null();

    cJSON_Hooks hooks = {
        .malloc_fn = &os_malloc,
        .free_fn   = &os_free_internal,
    };
    cJSON_InitHooks(&hooks);
    this->m_malloc_fail_on_cnt = 103;

    ASSERT_FALSE(gw_cfg_json_generate_for_saving(&gw_cfg, &json_str));
    ASSERT_EQ(nullptr, json_str.p_str);
    TEST_CHECK_LOG_RECORD(ESP_LOG_ERROR, string("Can't add json item: mqtt_pass"));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_TRUE(g_pTestClass->m_mem_alloc_trace.is_empty());
}

TEST_F(TestGwCfgJson, gw_cfg_json_generate_malloc_failed_on_mqtt_use_ssl_client_cert) // NOLINT
{
    const gw_cfg_t   gw_cfg   = get_gateway_config_default_lan_auth_ruuvi();
    cjson_wrap_str_t json_str = cjson_wrap_str_null();

    cJSON_Hooks hooks = {
        .malloc_fn = &os_malloc,
        .free_fn   = &os_free_internal,
    };
    cJSON_InitHooks(&hooks);
    this->m_malloc_fail_on_cnt = 104;

    ASSERT_FALSE(gw_cfg_json_generate_for_saving(&gw_cfg, &json_str));
    ASSERT_EQ(nullptr, json_str.p_str);
    TEST_CHECK_LOG_RECORD(ESP_LOG_ERROR, string("Can't add json item: mqtt_use_ssl_client_cert"));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_TRUE(g_pTestClass->m_mem_alloc_trace.is_empty());
}

TEST_F(TestGwCfgJson, gw_cfg_json_generate_malloc_failed_on_mqtt_use_ssl_client_cert_2) // NOLINT
{
    const gw_cfg_t   gw_cfg   = get_gateway_config_default_lan_auth_ruuvi();
    cjson_wrap_str_t json_str = cjson_wrap_str_null();

    cJSON_Hooks hooks = {
        .malloc_fn = &os_malloc,
        .free_fn   = &os_free_internal,
    };
    cJSON_InitHooks(&hooks);
    this->m_malloc_fail_on_cnt = 105;

    ASSERT_FALSE(gw_cfg_json_generate_for_saving(&gw_cfg, &json_str));
    ASSERT_EQ(nullptr, json_str.p_str);
    TEST_CHECK_LOG_RECORD(ESP_LOG_ERROR, string("Can't add json item: mqtt_use_ssl_client_cert"));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_TRUE(g_pTestClass->m_mem_alloc_trace.is_empty());
}

TEST_F(TestGwCfgJson, gw_cfg_json_generate_malloc_failed_on_mqtt_use_ssl_server_cert) // NOLINT
{
    const gw_cfg_t   gw_cfg   = get_gateway_config_default_lan_auth_ruuvi();
    cjson_wrap_str_t json_str = cjson_wrap_str_null();

    cJSON_Hooks hooks = {
        .malloc_fn = &os_malloc,
        .free_fn   = &os_free_internal,
    };
    cJSON_InitHooks(&hooks);
    this->m_malloc_fail_on_cnt = 106;

    ASSERT_FALSE(gw_cfg_json_generate_for_saving(&gw_cfg, &json_str));
    ASSERT_EQ(nullptr, json_str.p_str);
    TEST_CHECK_LOG_RECORD(ESP_LOG_ERROR, string("Can't add json item: mqtt_use_ssl_server_cert"));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_TRUE(g_pTestClass->m_mem_alloc_trace.is_empty());
}

TEST_F(TestGwCfgJson, gw_cfg_json_generate_malloc_failed_on_mqtt_use_ssl_server_cert_2) // NOLINT
{
    const gw_cfg_t   gw_cfg   = get_gateway_config_default_lan_auth_ruuvi();
    cjson_wrap_str_t json_str = cjson_wrap_str_null();

    cJSON_Hooks hooks = {
        .malloc_fn = &os_malloc,
        .free_fn   = &os_free_internal,
    };
    cJSON_InitHooks(&hooks);
    this->m_malloc_fail_on_cnt = 107;

    ASSERT_FALSE(gw_cfg_json_generate_for_saving(&gw_cfg, &json_str));
    ASSERT_EQ(nullptr, json_str.p_str);
    TEST_CHECK_LOG_RECORD(ESP_LOG_ERROR, string("Can't add json item: mqtt_use_ssl_server_cert"));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_TRUE(g_pTestClass->m_mem_alloc_trace.is_empty());
}

TEST_F(TestGwCfgJson, gw_cfg_json_generate_malloc_failed_on_lan_auth_type) // NOLINT
{
    const gw_cfg_t   gw_cfg   = get_gateway_config_default_lan_auth_ruuvi();
    cjson_wrap_str_t json_str = cjson_wrap_str_null();

    cJSON_Hooks hooks = {
        .malloc_fn = &os_malloc,
        .free_fn   = &os_free_internal,
    };
    cJSON_InitHooks(&hooks);
    this->m_malloc_fail_on_cnt = 108;

    ASSERT_FALSE(gw_cfg_json_generate_for_saving(&gw_cfg, &json_str));
    ASSERT_EQ(nullptr, json_str.p_str);
    TEST_CHECK_LOG_RECORD(ESP_LOG_ERROR, string("Can't add json item: lan_auth_type"));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_TRUE(g_pTestClass->m_mem_alloc_trace.is_empty());
}

TEST_F(TestGwCfgJson, gw_cfg_json_generate_malloc_failed_on_lan_auth_type_2) // NOLINT
{
    const gw_cfg_t   gw_cfg   = get_gateway_config_default_lan_auth_ruuvi();
    cjson_wrap_str_t json_str = cjson_wrap_str_null();

    cJSON_Hooks hooks = {
        .malloc_fn = &os_malloc,
        .free_fn   = &os_free_internal,
    };
    cJSON_InitHooks(&hooks);
    this->m_malloc_fail_on_cnt = 109;

    ASSERT_FALSE(gw_cfg_json_generate_for_saving(&gw_cfg, &json_str));
    ASSERT_EQ(nullptr, json_str.p_str);
    TEST_CHECK_LOG_RECORD(ESP_LOG_ERROR, string("Can't add json item: lan_auth_type"));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_TRUE(g_pTestClass->m_mem_alloc_trace.is_empty());
}

TEST_F(TestGwCfgJson, gw_cfg_json_generate_malloc_failed_on_lan_auth_type_3) // NOLINT
{
    const gw_cfg_t   gw_cfg   = get_gateway_config_default_lan_auth_ruuvi();
    cjson_wrap_str_t json_str = cjson_wrap_str_null();

    cJSON_Hooks hooks = {
        .malloc_fn = &os_malloc,
        .free_fn   = &os_free_internal,
    };
    cJSON_InitHooks(&hooks);
    this->m_malloc_fail_on_cnt = 110;

    ASSERT_FALSE(gw_cfg_json_generate_for_saving(&gw_cfg, &json_str));
    ASSERT_EQ(nullptr, json_str.p_str);
    TEST_CHECK_LOG_RECORD(ESP_LOG_ERROR, string("Can't add json item: lan_auth_type"));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_TRUE(g_pTestClass->m_mem_alloc_trace.is_empty());
}

TEST_F(TestGwCfgJson, gw_cfg_json_generate_malloc_failed_on_lan_auth_user) // NOLINT
{
    const gw_cfg_t   gw_cfg   = get_gateway_config_default_lan_auth_ruuvi();
    cjson_wrap_str_t json_str = cjson_wrap_str_null();

    cJSON_Hooks hooks = {
        .malloc_fn = &os_malloc,
        .free_fn   = &os_free_internal,
    };
    cJSON_InitHooks(&hooks);
    this->m_malloc_fail_on_cnt = 111;

    ASSERT_FALSE(gw_cfg_json_generate_for_saving(&gw_cfg, &json_str));
    ASSERT_EQ(nullptr, json_str.p_str);
    TEST_CHECK_LOG_RECORD(ESP_LOG_ERROR, string("Can't add json item: lan_auth_user"));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_TRUE(g_pTestClass->m_mem_alloc_trace.is_empty());
}

TEST_F(TestGwCfgJson, gw_cfg_json_generate_malloc_failed_on_lan_auth_user_2) // NOLINT
{
    const gw_cfg_t   gw_cfg   = get_gateway_config_default_lan_auth_ruuvi();
    cjson_wrap_str_t json_str = cjson_wrap_str_null();

    cJSON_Hooks hooks = {
        .malloc_fn = &os_malloc,
        .free_fn   = &os_free_internal,
    };
    cJSON_InitHooks(&hooks);
    this->m_malloc_fail_on_cnt = 112;

    ASSERT_FALSE(gw_cfg_json_generate_for_saving(&gw_cfg, &json_str));
    ASSERT_EQ(nullptr, json_str.p_str);
    TEST_CHECK_LOG_RECORD(ESP_LOG_ERROR, string("Can't add json item: lan_auth_user"));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_TRUE(g_pTestClass->m_mem_alloc_trace.is_empty());
}

TEST_F(TestGwCfgJson, gw_cfg_json_generate_malloc_failed_on_lan_auth_user_3) // NOLINT
{
    const gw_cfg_t   gw_cfg   = get_gateway_config_default_lan_auth_ruuvi();
    cjson_wrap_str_t json_str = cjson_wrap_str_null();

    cJSON_Hooks hooks = {
        .malloc_fn = &os_malloc,
        .free_fn   = &os_free_internal,
    };
    cJSON_InitHooks(&hooks);
    this->m_malloc_fail_on_cnt = 113;

    ASSERT_FALSE(gw_cfg_json_generate_for_saving(&gw_cfg, &json_str));
    ASSERT_EQ(nullptr, json_str.p_str);
    TEST_CHECK_LOG_RECORD(ESP_LOG_ERROR, string("Can't add json item: lan_auth_user"));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_TRUE(g_pTestClass->m_mem_alloc_trace.is_empty());
}

TEST_F(TestGwCfgJson, gw_cfg_json_generate_malloc_failed_on_lan_auth_pass) // NOLINT
{
    const gw_cfg_t   gw_cfg   = get_gateway_config_default_lan_auth_ruuvi();
    cjson_wrap_str_t json_str = cjson_wrap_str_null();

    cJSON_Hooks hooks = {
        .malloc_fn = &os_malloc,
        .free_fn   = &os_free_internal,
    };
    cJSON_InitHooks(&hooks);
    this->m_malloc_fail_on_cnt = 114;

    ASSERT_FALSE(gw_cfg_json_generate_for_saving(&gw_cfg, &json_str));
    ASSERT_EQ(nullptr, json_str.p_str);
    TEST_CHECK_LOG_RECORD(ESP_LOG_ERROR, string("Can't add json item: lan_auth_pass"));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_TRUE(g_pTestClass->m_mem_alloc_trace.is_empty());
}

TEST_F(TestGwCfgJson, gw_cfg_json_generate_malloc_failed_on_lan_auth_pass_2) // NOLINT
{
    const gw_cfg_t   gw_cfg   = get_gateway_config_default_lan_auth_ruuvi();
    cjson_wrap_str_t json_str = cjson_wrap_str_null();

    cJSON_Hooks hooks = {
        .malloc_fn = &os_malloc,
        .free_fn   = &os_free_internal,
    };
    cJSON_InitHooks(&hooks);
    this->m_malloc_fail_on_cnt = 115;

    ASSERT_FALSE(gw_cfg_json_generate_for_saving(&gw_cfg, &json_str));
    ASSERT_EQ(nullptr, json_str.p_str);
    TEST_CHECK_LOG_RECORD(ESP_LOG_ERROR, string("Can't add json item: lan_auth_pass"));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_TRUE(g_pTestClass->m_mem_alloc_trace.is_empty());
}

TEST_F(TestGwCfgJson, gw_cfg_json_generate_malloc_failed_on_lan_auth_pass_3) // NOLINT
{
    const gw_cfg_t   gw_cfg   = get_gateway_config_default_lan_auth_ruuvi();
    cjson_wrap_str_t json_str = cjson_wrap_str_null();

    cJSON_Hooks hooks = {
        .malloc_fn = &os_malloc,
        .free_fn   = &os_free_internal,
    };
    cJSON_InitHooks(&hooks);
    this->m_malloc_fail_on_cnt = 116;

    ASSERT_FALSE(gw_cfg_json_generate_for_saving(&gw_cfg, &json_str));
    ASSERT_EQ(nullptr, json_str.p_str);
    TEST_CHECK_LOG_RECORD(ESP_LOG_ERROR, string("Can't add json item: lan_auth_pass"));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_TRUE(g_pTestClass->m_mem_alloc_trace.is_empty());
}

TEST_F(TestGwCfgJson, gw_cfg_json_generate_malloc_failed_on_lan_auth_api_key) // NOLINT
{
    const gw_cfg_t   gw_cfg   = get_gateway_config_default_lan_auth_ruuvi();
    cjson_wrap_str_t json_str = cjson_wrap_str_null();

    cJSON_Hooks hooks = {
        .malloc_fn = &os_malloc,
        .free_fn   = &os_free_internal,
    };
    cJSON_InitHooks(&hooks);
    this->m_malloc_fail_on_cnt = 117;

    ASSERT_FALSE(gw_cfg_json_generate_for_saving(&gw_cfg, &json_str));
    ASSERT_EQ(nullptr, json_str.p_str);
    TEST_CHECK_LOG_RECORD(ESP_LOG_ERROR, string("Can't add json item: lan_auth_api_key"));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_TRUE(g_pTestClass->m_mem_alloc_trace.is_empty());
}

TEST_F(TestGwCfgJson, gw_cfg_json_generate_malloc_failed_on_lan_auth_api_key_2) // NOLINT
{
    const gw_cfg_t   gw_cfg   = get_gateway_config_default_lan_auth_ruuvi();
    cjson_wrap_str_t json_str = cjson_wrap_str_null();

    cJSON_Hooks hooks = {
        .malloc_fn = &os_malloc,
        .free_fn   = &os_free_internal,
    };
    cJSON_InitHooks(&hooks);
    this->m_malloc_fail_on_cnt = 118;

    ASSERT_FALSE(gw_cfg_json_generate_for_saving(&gw_cfg, &json_str));
    ASSERT_EQ(nullptr, json_str.p_str);
    TEST_CHECK_LOG_RECORD(ESP_LOG_ERROR, string("Can't add json item: lan_auth_api_key"));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_TRUE(g_pTestClass->m_mem_alloc_trace.is_empty());
}

TEST_F(TestGwCfgJson, gw_cfg_json_generate_malloc_failed_on_lan_auth_api_key_3) // NOLINT
{
    const gw_cfg_t   gw_cfg   = get_gateway_config_default_lan_auth_ruuvi();
    cjson_wrap_str_t json_str = cjson_wrap_str_null();

    cJSON_Hooks hooks = {
        .malloc_fn = &os_malloc,
        .free_fn   = &os_free_internal,
    };
    cJSON_InitHooks(&hooks);
    this->m_malloc_fail_on_cnt = 119;

    ASSERT_FALSE(gw_cfg_json_generate_for_saving(&gw_cfg, &json_str));
    ASSERT_EQ(nullptr, json_str.p_str);
    TEST_CHECK_LOG_RECORD(ESP_LOG_ERROR, string("Can't add json item: lan_auth_api_key"));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_TRUE(g_pTestClass->m_mem_alloc_trace.is_empty());
}

TEST_F(TestGwCfgJson, gw_cfg_json_generate_malloc_failed_on_lan_auth_api_key_rw) // NOLINT
{
    const gw_cfg_t   gw_cfg   = get_gateway_config_default_lan_auth_ruuvi();
    cjson_wrap_str_t json_str = cjson_wrap_str_null();

    cJSON_Hooks hooks = {
        .malloc_fn = &os_malloc,
        .free_fn   = &os_free_internal,
    };
    cJSON_InitHooks(&hooks);
    this->m_malloc_fail_on_cnt = 120;

    ASSERT_FALSE(gw_cfg_json_generate_for_saving(&gw_cfg, &json_str));
    ASSERT_EQ(nullptr, json_str.p_str);
    TEST_CHECK_LOG_RECORD(ESP_LOG_ERROR, string("Can't add json item: lan_auth_api_key_rw"));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_TRUE(g_pTestClass->m_mem_alloc_trace.is_empty());
}

TEST_F(TestGwCfgJson, gw_cfg_json_generate_malloc_failed_on_lan_auth_api_key_rw_2) // NOLINT
{
    const gw_cfg_t   gw_cfg   = get_gateway_config_default_lan_auth_ruuvi();
    cjson_wrap_str_t json_str = cjson_wrap_str_null();

    cJSON_Hooks hooks = {
        .malloc_fn = &os_malloc,
        .free_fn   = &os_free_internal,
    };
    cJSON_InitHooks(&hooks);
    this->m_malloc_fail_on_cnt = 121;

    ASSERT_FALSE(gw_cfg_json_generate_for_saving(&gw_cfg, &json_str));
    ASSERT_EQ(nullptr, json_str.p_str);
    TEST_CHECK_LOG_RECORD(ESP_LOG_ERROR, string("Can't add json item: lan_auth_api_key_rw"));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_TRUE(g_pTestClass->m_mem_alloc_trace.is_empty());
}

TEST_F(TestGwCfgJson, gw_cfg_json_generate_malloc_failed_on_lan_auth_api_key_rw_3) // NOLINT
{
    const gw_cfg_t   gw_cfg   = get_gateway_config_default_lan_auth_ruuvi();
    cjson_wrap_str_t json_str = cjson_wrap_str_null();

    cJSON_Hooks hooks = {
        .malloc_fn = &os_malloc,
        .free_fn   = &os_free_internal,
    };
    cJSON_InitHooks(&hooks);
    this->m_malloc_fail_on_cnt = 122;

    ASSERT_FALSE(gw_cfg_json_generate_for_saving(&gw_cfg, &json_str));
    ASSERT_EQ(nullptr, json_str.p_str);
    TEST_CHECK_LOG_RECORD(ESP_LOG_ERROR, string("Can't add json item: lan_auth_api_key_rw"));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_TRUE(g_pTestClass->m_mem_alloc_trace.is_empty());
}

TEST_F(TestGwCfgJson, gw_cfg_json_generate_malloc_failed_on_auto_update_cycle) // NOLINT
{
    const gw_cfg_t   gw_cfg   = get_gateway_config_default_lan_auth_ruuvi();
    cjson_wrap_str_t json_str = cjson_wrap_str_null();

    cJSON_Hooks hooks = {
        .malloc_fn = &os_malloc,
        .free_fn   = &os_free_internal,
    };
    cJSON_InitHooks(&hooks);
    this->m_malloc_fail_on_cnt = 123;

    ASSERT_FALSE(gw_cfg_json_generate_for_saving(&gw_cfg, &json_str));
    ASSERT_EQ(nullptr, json_str.p_str);
    TEST_CHECK_LOG_RECORD(ESP_LOG_ERROR, string("Can't add json item: auto_update_cycle"));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_TRUE(g_pTestClass->m_mem_alloc_trace.is_empty());
}

TEST_F(TestGwCfgJson, gw_cfg_json_generate_malloc_failed_on_auto_update_cycle_2) // NOLINT
{
    const gw_cfg_t   gw_cfg   = get_gateway_config_default_lan_auth_ruuvi();
    cjson_wrap_str_t json_str = cjson_wrap_str_null();

    cJSON_Hooks hooks = {
        .malloc_fn = &os_malloc,
        .free_fn   = &os_free_internal,
    };
    cJSON_InitHooks(&hooks);
    this->m_malloc_fail_on_cnt = 124;

    ASSERT_FALSE(gw_cfg_json_generate_for_saving(&gw_cfg, &json_str));
    ASSERT_EQ(nullptr, json_str.p_str);
    TEST_CHECK_LOG_RECORD(ESP_LOG_ERROR, string("Can't add json item: auto_update_cycle"));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_TRUE(g_pTestClass->m_mem_alloc_trace.is_empty());
}

TEST_F(TestGwCfgJson, gw_cfg_json_generate_malloc_failed_on_auto_update_cycle_3) // NOLINT
{
    const gw_cfg_t   gw_cfg   = get_gateway_config_default_lan_auth_ruuvi();
    cjson_wrap_str_t json_str = cjson_wrap_str_null();

    cJSON_Hooks hooks = {
        .malloc_fn = &os_malloc,
        .free_fn   = &os_free_internal,
    };
    cJSON_InitHooks(&hooks);
    this->m_malloc_fail_on_cnt = 125;

    ASSERT_FALSE(gw_cfg_json_generate_for_saving(&gw_cfg, &json_str));
    ASSERT_EQ(nullptr, json_str.p_str);
    TEST_CHECK_LOG_RECORD(ESP_LOG_ERROR, string("Can't add json item: auto_update_cycle"));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_TRUE(g_pTestClass->m_mem_alloc_trace.is_empty());
}

TEST_F(TestGwCfgJson, gw_cfg_json_generate_malloc_failed_on_auto_update_weekdays_bitmask) // NOLINT
{
    const gw_cfg_t   gw_cfg   = get_gateway_config_default_lan_auth_ruuvi();
    cjson_wrap_str_t json_str = cjson_wrap_str_null();

    cJSON_Hooks hooks = {
        .malloc_fn = &os_malloc,
        .free_fn   = &os_free_internal,
    };
    cJSON_InitHooks(&hooks);
    this->m_malloc_fail_on_cnt = 126;

    ASSERT_FALSE(gw_cfg_json_generate_for_saving(&gw_cfg, &json_str));
    ASSERT_EQ(nullptr, json_str.p_str);
    TEST_CHECK_LOG_RECORD(ESP_LOG_ERROR, string("Can't add json item: auto_update_weekdays_bitmask"));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_TRUE(g_pTestClass->m_mem_alloc_trace.is_empty());
}

TEST_F(TestGwCfgJson, gw_cfg_json_generate_malloc_failed_on_auto_update_weekdays_bitmask_2) // NOLINT
{
    const gw_cfg_t   gw_cfg   = get_gateway_config_default_lan_auth_ruuvi();
    cjson_wrap_str_t json_str = cjson_wrap_str_null();

    cJSON_Hooks hooks = {
        .malloc_fn = &os_malloc,
        .free_fn   = &os_free_internal,
    };
    cJSON_InitHooks(&hooks);
    this->m_malloc_fail_on_cnt = 127;

    ASSERT_FALSE(gw_cfg_json_generate_for_saving(&gw_cfg, &json_str));
    ASSERT_EQ(nullptr, json_str.p_str);
    TEST_CHECK_LOG_RECORD(ESP_LOG_ERROR, string("Can't add json item: auto_update_weekdays_bitmask"));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_TRUE(g_pTestClass->m_mem_alloc_trace.is_empty());
}

TEST_F(TestGwCfgJson, gw_cfg_json_generate_malloc_failed_on_auto_update_interval_from) // NOLINT
{
    const gw_cfg_t   gw_cfg   = get_gateway_config_default_lan_auth_ruuvi();
    cjson_wrap_str_t json_str = cjson_wrap_str_null();

    cJSON_Hooks hooks = {
        .malloc_fn = &os_malloc,
        .free_fn   = &os_free_internal,
    };
    cJSON_InitHooks(&hooks);
    this->m_malloc_fail_on_cnt = 128;

    ASSERT_FALSE(gw_cfg_json_generate_for_saving(&gw_cfg, &json_str));
    ASSERT_EQ(nullptr, json_str.p_str);
    TEST_CHECK_LOG_RECORD(ESP_LOG_ERROR, string("Can't add json item: auto_update_interval_from"));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_TRUE(g_pTestClass->m_mem_alloc_trace.is_empty());
}

TEST_F(TestGwCfgJson, gw_cfg_json_generate_malloc_failed_on_auto_update_interval_from_2) // NOLINT
{
    const gw_cfg_t   gw_cfg   = get_gateway_config_default_lan_auth_ruuvi();
    cjson_wrap_str_t json_str = cjson_wrap_str_null();

    cJSON_Hooks hooks = {
        .malloc_fn = &os_malloc,
        .free_fn   = &os_free_internal,
    };
    cJSON_InitHooks(&hooks);
    this->m_malloc_fail_on_cnt = 129;

    ASSERT_FALSE(gw_cfg_json_generate_for_saving(&gw_cfg, &json_str));
    ASSERT_EQ(nullptr, json_str.p_str);
    TEST_CHECK_LOG_RECORD(ESP_LOG_ERROR, string("Can't add json item: auto_update_interval_from"));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_TRUE(g_pTestClass->m_mem_alloc_trace.is_empty());
}

TEST_F(TestGwCfgJson, gw_cfg_json_generate_malloc_failed_on_auto_update_interval_to) // NOLINT
{
    const gw_cfg_t   gw_cfg   = get_gateway_config_default_lan_auth_ruuvi();
    cjson_wrap_str_t json_str = cjson_wrap_str_null();

    cJSON_Hooks hooks = {
        .malloc_fn = &os_malloc,
        .free_fn   = &os_free_internal,
    };
    cJSON_InitHooks(&hooks);
    this->m_malloc_fail_on_cnt = 130;

    ASSERT_FALSE(gw_cfg_json_generate_for_saving(&gw_cfg, &json_str));
    ASSERT_EQ(nullptr, json_str.p_str);
    TEST_CHECK_LOG_RECORD(ESP_LOG_ERROR, string("Can't add json item: auto_update_interval_to"));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_TRUE(g_pTestClass->m_mem_alloc_trace.is_empty());
}

TEST_F(TestGwCfgJson, gw_cfg_json_generate_malloc_failed_on_auto_update_interval_to_2) // NOLINT
{
    const gw_cfg_t   gw_cfg   = get_gateway_config_default_lan_auth_ruuvi();
    cjson_wrap_str_t json_str = cjson_wrap_str_null();

    cJSON_Hooks hooks = {
        .malloc_fn = &os_malloc,
        .free_fn   = &os_free_internal,
    };
    cJSON_InitHooks(&hooks);
    this->m_malloc_fail_on_cnt = 131;

    ASSERT_FALSE(gw_cfg_json_generate_for_saving(&gw_cfg, &json_str));
    ASSERT_EQ(nullptr, json_str.p_str);
    TEST_CHECK_LOG_RECORD(ESP_LOG_ERROR, string("Can't add json item: auto_update_interval_to"));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_TRUE(g_pTestClass->m_mem_alloc_trace.is_empty());
}

TEST_F(TestGwCfgJson, gw_cfg_json_generate_malloc_failed_on_auto_update_tz) // NOLINT
{
    const gw_cfg_t   gw_cfg   = get_gateway_config_default_lan_auth_ruuvi();
    cjson_wrap_str_t json_str = cjson_wrap_str_null();

    cJSON_Hooks hooks = {
        .malloc_fn = &os_malloc,
        .free_fn   = &os_free_internal,
    };
    cJSON_InitHooks(&hooks);
    this->m_malloc_fail_on_cnt = 132;

    ASSERT_FALSE(gw_cfg_json_generate_for_saving(&gw_cfg, &json_str));
    ASSERT_EQ(nullptr, json_str.p_str);
    TEST_CHECK_LOG_RECORD(ESP_LOG_ERROR, string("Can't add json item: auto_update_tz_offset_hours"));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_TRUE(g_pTestClass->m_mem_alloc_trace.is_empty());
}

TEST_F(TestGwCfgJson, gw_cfg_json_generate_malloc_failed_on_auto_update_tz_2) // NOLINT
{
    const gw_cfg_t   gw_cfg   = get_gateway_config_default_lan_auth_ruuvi();
    cjson_wrap_str_t json_str = cjson_wrap_str_null();

    cJSON_Hooks hooks = {
        .malloc_fn = &os_malloc,
        .free_fn   = &os_free_internal,
    };
    cJSON_InitHooks(&hooks);
    this->m_malloc_fail_on_cnt = 133;

    ASSERT_FALSE(gw_cfg_json_generate_for_saving(&gw_cfg, &json_str));
    ASSERT_EQ(nullptr, json_str.p_str);
    TEST_CHECK_LOG_RECORD(ESP_LOG_ERROR, string("Can't add json item: auto_update_tz_offset_hours"));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_TRUE(g_pTestClass->m_mem_alloc_trace.is_empty());
}

TEST_F(TestGwCfgJson, gw_cfg_json_generate_malloc_failed_on_ntp_use) // NOLINT
{
    const gw_cfg_t   gw_cfg   = get_gateway_config_default_lan_auth_ruuvi();
    cjson_wrap_str_t json_str = cjson_wrap_str_null();

    cJSON_Hooks hooks = {
        .malloc_fn = &os_malloc,
        .free_fn   = &os_free_internal,
    };
    cJSON_InitHooks(&hooks);
    this->m_malloc_fail_on_cnt = 134;

    ASSERT_FALSE(gw_cfg_json_generate_for_saving(&gw_cfg, &json_str));
    ASSERT_EQ(nullptr, json_str.p_str);
    TEST_CHECK_LOG_RECORD(ESP_LOG_ERROR, string("Can't add json item: ntp_use"));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_TRUE(g_pTestClass->m_mem_alloc_trace.is_empty());
}

TEST_F(TestGwCfgJson, gw_cfg_json_generate_malloc_failed_on_ntp_use_2) // NOLINT
{
    const gw_cfg_t   gw_cfg   = get_gateway_config_default_lan_auth_ruuvi();
    cjson_wrap_str_t json_str = cjson_wrap_str_null();

    cJSON_Hooks hooks = {
        .malloc_fn = &os_malloc,
        .free_fn   = &os_free_internal,
    };
    cJSON_InitHooks(&hooks);
    this->m_malloc_fail_on_cnt = 135;

    ASSERT_FALSE(gw_cfg_json_generate_for_saving(&gw_cfg, &json_str));
    ASSERT_EQ(nullptr, json_str.p_str);
    TEST_CHECK_LOG_RECORD(ESP_LOG_ERROR, string("Can't add json item: ntp_use"));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_TRUE(g_pTestClass->m_mem_alloc_trace.is_empty());
}

TEST_F(TestGwCfgJson, gw_cfg_json_generate_malloc_failed_on_ntp_use_dhcp) // NOLINT
{
    const gw_cfg_t   gw_cfg   = get_gateway_config_default_lan_auth_ruuvi();
    cjson_wrap_str_t json_str = cjson_wrap_str_null();

    cJSON_Hooks hooks = {
        .malloc_fn = &os_malloc,
        .free_fn   = &os_free_internal,
    };
    cJSON_InitHooks(&hooks);
    this->m_malloc_fail_on_cnt = 136;

    ASSERT_FALSE(gw_cfg_json_generate_for_saving(&gw_cfg, &json_str));
    ASSERT_EQ(nullptr, json_str.p_str);
    TEST_CHECK_LOG_RECORD(ESP_LOG_ERROR, string("Can't add json item: ntp_use_dhcp"));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_TRUE(g_pTestClass->m_mem_alloc_trace.is_empty());
}

TEST_F(TestGwCfgJson, gw_cfg_json_generate_malloc_failed_on_ntp_use_dhcp_2) // NOLINT
{
    const gw_cfg_t   gw_cfg   = get_gateway_config_default_lan_auth_ruuvi();
    cjson_wrap_str_t json_str = cjson_wrap_str_null();

    cJSON_Hooks hooks = {
        .malloc_fn = &os_malloc,
        .free_fn   = &os_free_internal,
    };
    cJSON_InitHooks(&hooks);
    this->m_malloc_fail_on_cnt = 137;

    ASSERT_FALSE(gw_cfg_json_generate_for_saving(&gw_cfg, &json_str));
    ASSERT_EQ(nullptr, json_str.p_str);
    TEST_CHECK_LOG_RECORD(ESP_LOG_ERROR, string("Can't add json item: ntp_use_dhcp"));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_TRUE(g_pTestClass->m_mem_alloc_trace.is_empty());
}

TEST_F(TestGwCfgJson, gw_cfg_json_generate_malloc_failed_on_ntp_server1) // NOLINT
{
    const gw_cfg_t   gw_cfg   = get_gateway_config_default_lan_auth_ruuvi();
    cjson_wrap_str_t json_str = cjson_wrap_str_null();

    cJSON_Hooks hooks = {
        .malloc_fn = &os_malloc,
        .free_fn   = &os_free_internal,
    };
    cJSON_InitHooks(&hooks);
    this->m_malloc_fail_on_cnt = 138;

    ASSERT_FALSE(gw_cfg_json_generate_for_saving(&gw_cfg, &json_str));
    ASSERT_EQ(nullptr, json_str.p_str);
    TEST_CHECK_LOG_RECORD(ESP_LOG_ERROR, string("Can't add json item: ntp_server1"));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_TRUE(g_pTestClass->m_mem_alloc_trace.is_empty());
}

TEST_F(TestGwCfgJson, gw_cfg_json_generate_malloc_failed_on_ntp_server1_2) // NOLINT
{
    const gw_cfg_t   gw_cfg   = get_gateway_config_default_lan_auth_ruuvi();
    cjson_wrap_str_t json_str = cjson_wrap_str_null();

    cJSON_Hooks hooks = {
        .malloc_fn = &os_malloc,
        .free_fn   = &os_free_internal,
    };
    cJSON_InitHooks(&hooks);
    this->m_malloc_fail_on_cnt = 139;

    ASSERT_FALSE(gw_cfg_json_generate_for_saving(&gw_cfg, &json_str));
    ASSERT_EQ(nullptr, json_str.p_str);
    TEST_CHECK_LOG_RECORD(ESP_LOG_ERROR, string("Can't add json item: ntp_server1"));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_TRUE(g_pTestClass->m_mem_alloc_trace.is_empty());
}

TEST_F(TestGwCfgJson, gw_cfg_json_generate_malloc_failed_on_ntp_server1_3) // NOLINT
{
    const gw_cfg_t   gw_cfg   = get_gateway_config_default_lan_auth_ruuvi();
    cjson_wrap_str_t json_str = cjson_wrap_str_null();

    cJSON_Hooks hooks = {
        .malloc_fn = &os_malloc,
        .free_fn   = &os_free_internal,
    };
    cJSON_InitHooks(&hooks);
    this->m_malloc_fail_on_cnt = 140;

    ASSERT_FALSE(gw_cfg_json_generate_for_saving(&gw_cfg, &json_str));
    ASSERT_EQ(nullptr, json_str.p_str);
    TEST_CHECK_LOG_RECORD(ESP_LOG_ERROR, string("Can't add json item: ntp_server1"));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_TRUE(g_pTestClass->m_mem_alloc_trace.is_empty());
}

TEST_F(TestGwCfgJson, gw_cfg_json_generate_malloc_failed_on_ntp_server2) // NOLINT
{
    const gw_cfg_t   gw_cfg   = get_gateway_config_default_lan_auth_ruuvi();
    cjson_wrap_str_t json_str = cjson_wrap_str_null();

    cJSON_Hooks hooks = {
        .malloc_fn = &os_malloc,
        .free_fn   = &os_free_internal,
    };
    cJSON_InitHooks(&hooks);
    this->m_malloc_fail_on_cnt = 141;

    ASSERT_FALSE(gw_cfg_json_generate_for_saving(&gw_cfg, &json_str));
    ASSERT_EQ(nullptr, json_str.p_str);
    TEST_CHECK_LOG_RECORD(ESP_LOG_ERROR, string("Can't add json item: ntp_server2"));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_TRUE(g_pTestClass->m_mem_alloc_trace.is_empty());
}

TEST_F(TestGwCfgJson, gw_cfg_json_generate_malloc_failed_on_ntp_server2_2) // NOLINT
{
    const gw_cfg_t   gw_cfg   = get_gateway_config_default_lan_auth_ruuvi();
    cjson_wrap_str_t json_str = cjson_wrap_str_null();

    cJSON_Hooks hooks = {
        .malloc_fn = &os_malloc,
        .free_fn   = &os_free_internal,
    };
    cJSON_InitHooks(&hooks);
    this->m_malloc_fail_on_cnt = 142;

    ASSERT_FALSE(gw_cfg_json_generate_for_saving(&gw_cfg, &json_str));
    ASSERT_EQ(nullptr, json_str.p_str);
    TEST_CHECK_LOG_RECORD(ESP_LOG_ERROR, string("Can't add json item: ntp_server2"));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_TRUE(g_pTestClass->m_mem_alloc_trace.is_empty());
}

TEST_F(TestGwCfgJson, gw_cfg_json_generate_malloc_failed_on_ntp_server2_3) // NOLINT
{
    const gw_cfg_t   gw_cfg   = get_gateway_config_default_lan_auth_ruuvi();
    cjson_wrap_str_t json_str = cjson_wrap_str_null();

    cJSON_Hooks hooks = {
        .malloc_fn = &os_malloc,
        .free_fn   = &os_free_internal,
    };
    cJSON_InitHooks(&hooks);
    this->m_malloc_fail_on_cnt = 143;

    ASSERT_FALSE(gw_cfg_json_generate_for_saving(&gw_cfg, &json_str));
    ASSERT_EQ(nullptr, json_str.p_str);
    TEST_CHECK_LOG_RECORD(ESP_LOG_ERROR, string("Can't add json item: ntp_server2"));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_TRUE(g_pTestClass->m_mem_alloc_trace.is_empty());
}

TEST_F(TestGwCfgJson, gw_cfg_json_generate_malloc_failed_on_ntp_server3) // NOLINT
{
    const gw_cfg_t   gw_cfg   = get_gateway_config_default_lan_auth_ruuvi();
    cjson_wrap_str_t json_str = cjson_wrap_str_null();

    cJSON_Hooks hooks = {
        .malloc_fn = &os_malloc,
        .free_fn   = &os_free_internal,
    };
    cJSON_InitHooks(&hooks);
    this->m_malloc_fail_on_cnt = 144;

    ASSERT_FALSE(gw_cfg_json_generate_for_saving(&gw_cfg, &json_str));
    ASSERT_EQ(nullptr, json_str.p_str);
    TEST_CHECK_LOG_RECORD(ESP_LOG_ERROR, string("Can't add json item: ntp_server3"));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_TRUE(g_pTestClass->m_mem_alloc_trace.is_empty());
}

TEST_F(TestGwCfgJson, gw_cfg_json_generate_malloc_failed_on_ntp_server3_2) // NOLINT
{
    const gw_cfg_t   gw_cfg   = get_gateway_config_default_lan_auth_ruuvi();
    cjson_wrap_str_t json_str = cjson_wrap_str_null();

    cJSON_Hooks hooks = {
        .malloc_fn = &os_malloc,
        .free_fn   = &os_free_internal,
    };
    cJSON_InitHooks(&hooks);
    this->m_malloc_fail_on_cnt = 145;

    ASSERT_FALSE(gw_cfg_json_generate_for_saving(&gw_cfg, &json_str));
    ASSERT_EQ(nullptr, json_str.p_str);
    TEST_CHECK_LOG_RECORD(ESP_LOG_ERROR, string("Can't add json item: ntp_server3"));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_TRUE(g_pTestClass->m_mem_alloc_trace.is_empty());
}

TEST_F(TestGwCfgJson, gw_cfg_json_generate_malloc_failed_on_ntp_server3_3) // NOLINT
{
    const gw_cfg_t   gw_cfg   = get_gateway_config_default_lan_auth_ruuvi();
    cjson_wrap_str_t json_str = cjson_wrap_str_null();

    cJSON_Hooks hooks = {
        .malloc_fn = &os_malloc,
        .free_fn   = &os_free_internal,
    };
    cJSON_InitHooks(&hooks);
    this->m_malloc_fail_on_cnt = 146;

    ASSERT_FALSE(gw_cfg_json_generate_for_saving(&gw_cfg, &json_str));
    ASSERT_EQ(nullptr, json_str.p_str);
    TEST_CHECK_LOG_RECORD(ESP_LOG_ERROR, string("Can't add json item: ntp_server3"));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_TRUE(g_pTestClass->m_mem_alloc_trace.is_empty());
}

TEST_F(TestGwCfgJson, gw_cfg_json_generate_malloc_failed_on_ntp_server4) // NOLINT
{
    const gw_cfg_t   gw_cfg   = get_gateway_config_default_lan_auth_ruuvi();
    cjson_wrap_str_t json_str = cjson_wrap_str_null();

    cJSON_Hooks hooks = {
        .malloc_fn = &os_malloc,
        .free_fn   = &os_free_internal,
    };
    cJSON_InitHooks(&hooks);
    this->m_malloc_fail_on_cnt = 147;

    ASSERT_FALSE(gw_cfg_json_generate_for_saving(&gw_cfg, &json_str));
    ASSERT_EQ(nullptr, json_str.p_str);
    TEST_CHECK_LOG_RECORD(ESP_LOG_ERROR, string("Can't add json item: ntp_server4"));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_TRUE(g_pTestClass->m_mem_alloc_trace.is_empty());
}

TEST_F(TestGwCfgJson, gw_cfg_json_generate_malloc_failed_on_ntp_server4_1) // NOLINT
{
    const gw_cfg_t   gw_cfg   = get_gateway_config_default_lan_auth_ruuvi();
    cjson_wrap_str_t json_str = cjson_wrap_str_null();

    cJSON_Hooks hooks = {
        .malloc_fn = &os_malloc,
        .free_fn   = &os_free_internal,
    };
    cJSON_InitHooks(&hooks);
    this->m_malloc_fail_on_cnt = 148;

    ASSERT_FALSE(gw_cfg_json_generate_for_saving(&gw_cfg, &json_str));
    ASSERT_EQ(nullptr, json_str.p_str);
    TEST_CHECK_LOG_RECORD(ESP_LOG_ERROR, string("Can't add json item: ntp_server4"));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_TRUE(g_pTestClass->m_mem_alloc_trace.is_empty());
}

TEST_F(TestGwCfgJson, gw_cfg_json_generate_malloc_failed_on_ntp_server4_2) // NOLINT
{
    const gw_cfg_t   gw_cfg   = get_gateway_config_default_lan_auth_ruuvi();
    cjson_wrap_str_t json_str = cjson_wrap_str_null();

    cJSON_Hooks hooks = {
        .malloc_fn = &os_malloc,
        .free_fn   = &os_free_internal,
    };
    cJSON_InitHooks(&hooks);
    this->m_malloc_fail_on_cnt = 149;

    ASSERT_FALSE(gw_cfg_json_generate_for_saving(&gw_cfg, &json_str));
    ASSERT_EQ(nullptr, json_str.p_str);
    TEST_CHECK_LOG_RECORD(ESP_LOG_ERROR, string("Can't add json item: ntp_server4"));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_TRUE(g_pTestClass->m_mem_alloc_trace.is_empty());
}

TEST_F(TestGwCfgJson, gw_cfg_json_generate_malloc_failed_on_company_id) // NOLINT
{
    const gw_cfg_t   gw_cfg   = get_gateway_config_default_lan_auth_ruuvi();
    cjson_wrap_str_t json_str = cjson_wrap_str_null();

    cJSON_Hooks hooks = {
        .malloc_fn = &os_malloc,
        .free_fn   = &os_free_internal,
    };
    cJSON_InitHooks(&hooks);
    this->m_malloc_fail_on_cnt = 150;

    ASSERT_FALSE(gw_cfg_json_generate_for_saving(&gw_cfg, &json_str));
    ASSERT_EQ(nullptr, json_str.p_str);
    TEST_CHECK_LOG_RECORD(ESP_LOG_ERROR, string("Can't add json item: company_id"));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_TRUE(g_pTestClass->m_mem_alloc_trace.is_empty());
}

TEST_F(TestGwCfgJson, gw_cfg_json_generate_malloc_failed_on_company_id_2) // NOLINT
{
    const gw_cfg_t   gw_cfg   = get_gateway_config_default_lan_auth_ruuvi();
    cjson_wrap_str_t json_str = cjson_wrap_str_null();

    cJSON_Hooks hooks = {
        .malloc_fn = &os_malloc,
        .free_fn   = &os_free_internal,
    };
    cJSON_InitHooks(&hooks);
    this->m_malloc_fail_on_cnt = 151;

    ASSERT_FALSE(gw_cfg_json_generate_for_saving(&gw_cfg, &json_str));
    ASSERT_EQ(nullptr, json_str.p_str);
    TEST_CHECK_LOG_RECORD(ESP_LOG_ERROR, string("Can't add json item: company_id"));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_TRUE(g_pTestClass->m_mem_alloc_trace.is_empty());
}

TEST_F(TestGwCfgJson, gw_cfg_json_generate_malloc_failed_on_use_filtering) // NOLINT
{
    const gw_cfg_t   gw_cfg   = get_gateway_config_default_lan_auth_ruuvi();
    cjson_wrap_str_t json_str = cjson_wrap_str_null();

    cJSON_Hooks hooks = {
        .malloc_fn = &os_malloc,
        .free_fn   = &os_free_internal,
    };
    cJSON_InitHooks(&hooks);
    this->m_malloc_fail_on_cnt = 152;

    ASSERT_FALSE(gw_cfg_json_generate_for_saving(&gw_cfg, &json_str));
    ASSERT_EQ(nullptr, json_str.p_str);
    TEST_CHECK_LOG_RECORD(ESP_LOG_ERROR, string("Can't add json item: company_use_filtering"));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_TRUE(g_pTestClass->m_mem_alloc_trace.is_empty());
}

TEST_F(TestGwCfgJson, gw_cfg_json_generate_malloc_failed_on_use_filtering_2) // NOLINT
{
    const gw_cfg_t   gw_cfg   = get_gateway_config_default_lan_auth_ruuvi();
    cjson_wrap_str_t json_str = cjson_wrap_str_null();

    cJSON_Hooks hooks = {
        .malloc_fn = &os_malloc,
        .free_fn   = &os_free_internal,
    };
    cJSON_InitHooks(&hooks);
    this->m_malloc_fail_on_cnt = 153;

    ASSERT_FALSE(gw_cfg_json_generate_for_saving(&gw_cfg, &json_str));
    ASSERT_EQ(nullptr, json_str.p_str);
    TEST_CHECK_LOG_RECORD(ESP_LOG_ERROR, string("Can't add json item: company_use_filtering"));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_TRUE(g_pTestClass->m_mem_alloc_trace.is_empty());
}

TEST_F(TestGwCfgJson, gw_cfg_json_generate_malloc_failed_on_use_coded_phy) // NOLINT
{
    const gw_cfg_t   gw_cfg   = get_gateway_config_default_lan_auth_ruuvi();
    cjson_wrap_str_t json_str = cjson_wrap_str_null();

    cJSON_Hooks hooks = {
        .malloc_fn = &os_malloc,
        .free_fn   = &os_free_internal,
    };
    cJSON_InitHooks(&hooks);
    this->m_malloc_fail_on_cnt = 154;

    ASSERT_FALSE(gw_cfg_json_generate_for_saving(&gw_cfg, &json_str));
    ASSERT_EQ(nullptr, json_str.p_str);
    TEST_CHECK_LOG_RECORD(ESP_LOG_ERROR, string("Can't add json item: scan_coded_phy"));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_TRUE(g_pTestClass->m_mem_alloc_trace.is_empty());
}

TEST_F(TestGwCfgJson, gw_cfg_json_generate_malloc_failed_on_use_coded_phy_2) // NOLINT
{
    const gw_cfg_t   gw_cfg   = get_gateway_config_default_lan_auth_ruuvi();
    cjson_wrap_str_t json_str = cjson_wrap_str_null();

    cJSON_Hooks hooks = {
        .malloc_fn = &os_malloc,
        .free_fn   = &os_free_internal,
    };
    cJSON_InitHooks(&hooks);
    this->m_malloc_fail_on_cnt = 155;

    ASSERT_FALSE(gw_cfg_json_generate_for_saving(&gw_cfg, &json_str));
    ASSERT_EQ(nullptr, json_str.p_str);
    TEST_CHECK_LOG_RECORD(ESP_LOG_ERROR, string("Can't add json item: scan_coded_phy"));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_TRUE(g_pTestClass->m_mem_alloc_trace.is_empty());
}

TEST_F(TestGwCfgJson, gw_cfg_json_generate_malloc_failed_on_use_1mbit_phy) // NOLINT
{
    const gw_cfg_t   gw_cfg   = get_gateway_config_default_lan_auth_ruuvi();
    cjson_wrap_str_t json_str = cjson_wrap_str_null();

    cJSON_Hooks hooks = {
        .malloc_fn = &os_malloc,
        .free_fn   = &os_free_internal,
    };
    cJSON_InitHooks(&hooks);
    this->m_malloc_fail_on_cnt = 156;

    ASSERT_FALSE(gw_cfg_json_generate_for_saving(&gw_cfg, &json_str));
    ASSERT_EQ(nullptr, json_str.p_str);
    TEST_CHECK_LOG_RECORD(ESP_LOG_ERROR, string("Can't add json item: scan_1mbit_phy"));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_TRUE(g_pTestClass->m_mem_alloc_trace.is_empty());
}

TEST_F(TestGwCfgJson, gw_cfg_json_generate_malloc_failed_on_use_1mbit_phy_2) // NOLINT
{
    const gw_cfg_t   gw_cfg   = get_gateway_config_default_lan_auth_ruuvi();
    cjson_wrap_str_t json_str = cjson_wrap_str_null();

    cJSON_Hooks hooks = {
        .malloc_fn = &os_malloc,
        .free_fn   = &os_free_internal,
    };
    cJSON_InitHooks(&hooks);
    this->m_malloc_fail_on_cnt = 157;

    ASSERT_FALSE(gw_cfg_json_generate_for_saving(&gw_cfg, &json_str));
    ASSERT_EQ(nullptr, json_str.p_str);
    TEST_CHECK_LOG_RECORD(ESP_LOG_ERROR, string("Can't add json item: scan_1mbit_phy"));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_TRUE(g_pTestClass->m_mem_alloc_trace.is_empty());
}

TEST_F(TestGwCfgJson, gw_cfg_json_generate_malloc_failed_on_use_extended_payload) // NOLINT
{
    const gw_cfg_t   gw_cfg   = get_gateway_config_default_lan_auth_ruuvi();
    cjson_wrap_str_t json_str = cjson_wrap_str_null();

    cJSON_Hooks hooks = {
        .malloc_fn = &os_malloc,
        .free_fn   = &os_free_internal,
    };
    cJSON_InitHooks(&hooks);
    this->m_malloc_fail_on_cnt = 158;

    ASSERT_FALSE(gw_cfg_json_generate_for_saving(&gw_cfg, &json_str));
    ASSERT_EQ(nullptr, json_str.p_str);
    TEST_CHECK_LOG_RECORD(ESP_LOG_ERROR, string("Can't add json item: scan_extended_payload"));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_TRUE(g_pTestClass->m_mem_alloc_trace.is_empty());
}

TEST_F(TestGwCfgJson, gw_cfg_json_generate_malloc_failed_on_use_extended_payload_2) // NOLINT
{
    const gw_cfg_t   gw_cfg   = get_gateway_config_default_lan_auth_ruuvi();
    cjson_wrap_str_t json_str = cjson_wrap_str_null();

    cJSON_Hooks hooks = {
        .malloc_fn = &os_malloc,
        .free_fn   = &os_free_internal,
    };
    cJSON_InitHooks(&hooks);
    this->m_malloc_fail_on_cnt = 159;

    ASSERT_FALSE(gw_cfg_json_generate_for_saving(&gw_cfg, &json_str));
    ASSERT_EQ(nullptr, json_str.p_str);
    TEST_CHECK_LOG_RECORD(ESP_LOG_ERROR, string("Can't add json item: scan_extended_payload"));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_TRUE(g_pTestClass->m_mem_alloc_trace.is_empty());
}

TEST_F(TestGwCfgJson, gw_cfg_json_generate_malloc_failed_on_use_channel_37) // NOLINT
{
    const gw_cfg_t   gw_cfg   = get_gateway_config_default_lan_auth_ruuvi();
    cjson_wrap_str_t json_str = cjson_wrap_str_null();

    cJSON_Hooks hooks = {
        .malloc_fn = &os_malloc,
        .free_fn   = &os_free_internal,
    };
    cJSON_InitHooks(&hooks);
    this->m_malloc_fail_on_cnt = 160;

    ASSERT_FALSE(gw_cfg_json_generate_for_saving(&gw_cfg, &json_str));
    ASSERT_EQ(nullptr, json_str.p_str);
    TEST_CHECK_LOG_RECORD(ESP_LOG_ERROR, string("Can't add json item: scan_channel_37"));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_TRUE(g_pTestClass->m_mem_alloc_trace.is_empty());
}

TEST_F(TestGwCfgJson, gw_cfg_json_generate_malloc_failed_on_use_channel_37_2) // NOLINT
{
    const gw_cfg_t   gw_cfg   = get_gateway_config_default_lan_auth_ruuvi();
    cjson_wrap_str_t json_str = cjson_wrap_str_null();

    cJSON_Hooks hooks = {
        .malloc_fn = &os_malloc,
        .free_fn   = &os_free_internal,
    };
    cJSON_InitHooks(&hooks);
    this->m_malloc_fail_on_cnt = 161;

    ASSERT_FALSE(gw_cfg_json_generate_for_saving(&gw_cfg, &json_str));
    ASSERT_EQ(nullptr, json_str.p_str);
    TEST_CHECK_LOG_RECORD(ESP_LOG_ERROR, string("Can't add json item: scan_channel_37"));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_TRUE(g_pTestClass->m_mem_alloc_trace.is_empty());
}

TEST_F(TestGwCfgJson, gw_cfg_json_generate_malloc_failed_on_use_channel_38) // NOLINT
{
    const gw_cfg_t   gw_cfg   = get_gateway_config_default_lan_auth_ruuvi();
    cjson_wrap_str_t json_str = cjson_wrap_str_null();

    cJSON_Hooks hooks = {
        .malloc_fn = &os_malloc,
        .free_fn   = &os_free_internal,
    };
    cJSON_InitHooks(&hooks);
    this->m_malloc_fail_on_cnt = 162;

    ASSERT_FALSE(gw_cfg_json_generate_for_saving(&gw_cfg, &json_str));
    ASSERT_EQ(nullptr, json_str.p_str);
    TEST_CHECK_LOG_RECORD(ESP_LOG_ERROR, string("Can't add json item: scan_channel_38"));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_TRUE(g_pTestClass->m_mem_alloc_trace.is_empty());
}

TEST_F(TestGwCfgJson, gw_cfg_json_generate_malloc_failed_on_use_channel_38_2) // NOLINT
{
    const gw_cfg_t   gw_cfg   = get_gateway_config_default_lan_auth_ruuvi();
    cjson_wrap_str_t json_str = cjson_wrap_str_null();

    cJSON_Hooks hooks = {
        .malloc_fn = &os_malloc,
        .free_fn   = &os_free_internal,
    };
    cJSON_InitHooks(&hooks);
    this->m_malloc_fail_on_cnt = 163;

    ASSERT_FALSE(gw_cfg_json_generate_for_saving(&gw_cfg, &json_str));
    ASSERT_EQ(nullptr, json_str.p_str);
    TEST_CHECK_LOG_RECORD(ESP_LOG_ERROR, string("Can't add json item: scan_channel_38"));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_TRUE(g_pTestClass->m_mem_alloc_trace.is_empty());
}

TEST_F(TestGwCfgJson, gw_cfg_json_generate_malloc_failed_on_use_channel_39) // NOLINT
{
    const gw_cfg_t   gw_cfg   = get_gateway_config_default_lan_auth_ruuvi();
    cjson_wrap_str_t json_str = cjson_wrap_str_null();

    cJSON_Hooks hooks = {
        .malloc_fn = &os_malloc,
        .free_fn   = &os_free_internal,
    };
    cJSON_InitHooks(&hooks);
    this->m_malloc_fail_on_cnt = 164;

    ASSERT_FALSE(gw_cfg_json_generate_for_saving(&gw_cfg, &json_str));
    ASSERT_EQ(nullptr, json_str.p_str);
    TEST_CHECK_LOG_RECORD(ESP_LOG_ERROR, string("Can't add json item: scan_channel_39"));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_TRUE(g_pTestClass->m_mem_alloc_trace.is_empty());
}

TEST_F(TestGwCfgJson, gw_cfg_json_generate_malloc_failed_on_use_channel_39_2) // NOLINT
{
    const gw_cfg_t   gw_cfg   = get_gateway_config_default_lan_auth_ruuvi();
    cjson_wrap_str_t json_str = cjson_wrap_str_null();

    cJSON_Hooks hooks = {
        .malloc_fn = &os_malloc,
        .free_fn   = &os_free_internal,
    };
    cJSON_InitHooks(&hooks);
    this->m_malloc_fail_on_cnt = 165;

    ASSERT_FALSE(gw_cfg_json_generate_for_saving(&gw_cfg, &json_str));
    ASSERT_EQ(nullptr, json_str.p_str);
    TEST_CHECK_LOG_RECORD(ESP_LOG_ERROR, string("Can't add json item: scan_channel_39"));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_TRUE(g_pTestClass->m_mem_alloc_trace.is_empty());
}

TEST_F(TestGwCfgJson, gw_cfg_json_generate_malloc_failed_on_scan_filter_allow_listed) // NOLINT
{
    const gw_cfg_t   gw_cfg   = get_gateway_config_default_lan_auth_ruuvi();
    cjson_wrap_str_t json_str = cjson_wrap_str_null();

    cJSON_Hooks hooks = {
        .malloc_fn = &os_malloc,
        .free_fn   = &os_free_internal,
    };
    cJSON_InitHooks(&hooks);
    this->m_malloc_fail_on_cnt = 166;

    ASSERT_FALSE(gw_cfg_json_generate_for_saving(&gw_cfg, &json_str));
    ASSERT_EQ(nullptr, json_str.p_str);
    TEST_CHECK_LOG_RECORD(ESP_LOG_ERROR, string("Can't add json item: scan_filter_allow_listed"));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_TRUE(g_pTestClass->m_mem_alloc_trace.is_empty());
}

TEST_F(TestGwCfgJson, gw_cfg_json_generate_malloc_failed_on_scan_filter_allow_listed_2) // NOLINT
{
    const gw_cfg_t   gw_cfg   = get_gateway_config_default_lan_auth_ruuvi();
    cjson_wrap_str_t json_str = cjson_wrap_str_null();

    cJSON_Hooks hooks = {
        .malloc_fn = &os_malloc,
        .free_fn   = &os_free_internal,
    };
    cJSON_InitHooks(&hooks);
    this->m_malloc_fail_on_cnt = 167;

    ASSERT_FALSE(gw_cfg_json_generate_for_saving(&gw_cfg, &json_str));
    ASSERT_EQ(nullptr, json_str.p_str);
    TEST_CHECK_LOG_RECORD(ESP_LOG_ERROR, string("Can't add json item: scan_filter_allow_listed"));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_TRUE(g_pTestClass->m_mem_alloc_trace.is_empty());
}

TEST_F(TestGwCfgJson, gw_cfg_json_generate_malloc_failed_on_scan_filter_list) // NOLINT
{
    const gw_cfg_t   gw_cfg   = get_gateway_config_default_lan_auth_ruuvi();
    cjson_wrap_str_t json_str = cjson_wrap_str_null();

    cJSON_Hooks hooks = {
        .malloc_fn = &os_malloc,
        .free_fn   = &os_free_internal,
    };
    cJSON_InitHooks(&hooks);
    this->m_malloc_fail_on_cnt = 168;

    ASSERT_FALSE(gw_cfg_json_generate_for_saving(&gw_cfg, &json_str));
    ASSERT_EQ(nullptr, json_str.p_str);
    TEST_CHECK_LOG_RECORD(ESP_LOG_ERROR, string("Can't add json item: scan_filter_list"));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_TRUE(g_pTestClass->m_mem_alloc_trace.is_empty());
}

TEST_F(TestGwCfgJson, gw_cfg_json_generate_malloc_failed_on_scan_filter_list_2) // NOLINT
{
    const gw_cfg_t   gw_cfg   = get_gateway_config_default_lan_auth_ruuvi();
    cjson_wrap_str_t json_str = cjson_wrap_str_null();

    cJSON_Hooks hooks = {
        .malloc_fn = &os_malloc,
        .free_fn   = &os_free_internal,
    };
    cJSON_InitHooks(&hooks);
    this->m_malloc_fail_on_cnt = 169;

    ASSERT_FALSE(gw_cfg_json_generate_for_saving(&gw_cfg, &json_str));
    ASSERT_EQ(nullptr, json_str.p_str);
    TEST_CHECK_LOG_RECORD(ESP_LOG_ERROR, string("Can't add json item: scan_filter_list"));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_TRUE(g_pTestClass->m_mem_alloc_trace.is_empty());
}

TEST_F(TestGwCfgJson, gw_cfg_json_generate_malloc_failed_on_coordinates) // NOLINT
{
    const gw_cfg_t   gw_cfg   = get_gateway_config_default_lan_auth_ruuvi();
    cjson_wrap_str_t json_str = cjson_wrap_str_null();

    cJSON_Hooks hooks = {
        .malloc_fn = &os_malloc,
        .free_fn   = &os_free_internal,
    };
    cJSON_InitHooks(&hooks);
    this->m_malloc_fail_on_cnt = 170;

    ASSERT_FALSE(gw_cfg_json_generate_for_saving(&gw_cfg, &json_str));
    ASSERT_EQ(nullptr, json_str.p_str);
    TEST_CHECK_LOG_RECORD(ESP_LOG_ERROR, string("Can't add json item: coordinates"));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_TRUE(g_pTestClass->m_mem_alloc_trace.is_empty());
}

TEST_F(TestGwCfgJson, gw_cfg_json_generate_malloc_failed_on_coordinates_2) // NOLINT
{
    const gw_cfg_t   gw_cfg   = get_gateway_config_default_lan_auth_ruuvi();
    cjson_wrap_str_t json_str = cjson_wrap_str_null();

    cJSON_Hooks hooks = {
        .malloc_fn = &os_malloc,
        .free_fn   = &os_free_internal,
    };
    cJSON_InitHooks(&hooks);
    this->m_malloc_fail_on_cnt = 171;

    ASSERT_FALSE(gw_cfg_json_generate_for_saving(&gw_cfg, &json_str));
    ASSERT_EQ(nullptr, json_str.p_str);
    TEST_CHECK_LOG_RECORD(ESP_LOG_ERROR, string("Can't add json item: coordinates"));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_TRUE(g_pTestClass->m_mem_alloc_trace.is_empty());
}

TEST_F(TestGwCfgJson, gw_cfg_json_generate_malloc_failed_on_coordinates_3) // NOLINT
{
    const gw_cfg_t   gw_cfg   = get_gateway_config_default_lan_auth_ruuvi();
    cjson_wrap_str_t json_str = cjson_wrap_str_null();

    cJSON_Hooks hooks = {
        .malloc_fn = &os_malloc,
        .free_fn   = &os_free_internal,
    };
    cJSON_InitHooks(&hooks);
    this->m_malloc_fail_on_cnt = 172;

    ASSERT_FALSE(gw_cfg_json_generate_for_saving(&gw_cfg, &json_str));
    ASSERT_EQ(nullptr, json_str.p_str);
    TEST_CHECK_LOG_RECORD(ESP_LOG_ERROR, string("Can't add json item: coordinates"));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_TRUE(g_pTestClass->m_mem_alloc_trace.is_empty());
}

TEST_F(TestGwCfgJson, gw_cfg_json_generate_malloc_failed_on_fw_update_url) // NOLINT
{
    const gw_cfg_t   gw_cfg   = get_gateway_config_default_lan_auth_ruuvi();
    cjson_wrap_str_t json_str = cjson_wrap_str_null();

    cJSON_Hooks hooks = {
        .malloc_fn = &os_malloc,
        .free_fn   = &os_free_internal,
    };
    cJSON_InitHooks(&hooks);
    this->m_malloc_fail_on_cnt = 173;

    ASSERT_FALSE(gw_cfg_json_generate_for_saving(&gw_cfg, &json_str));
    ASSERT_EQ(nullptr, json_str.p_str);
    TEST_CHECK_LOG_RECORD(ESP_LOG_ERROR, string("Can't add json item: fw_update_url"));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_TRUE(g_pTestClass->m_mem_alloc_trace.is_empty());
}

TEST_F(TestGwCfgJson, gw_cfg_json_generate_malloc_failed_on_fw_update_url_2) // NOLINT
{
    const gw_cfg_t   gw_cfg   = get_gateway_config_default_lan_auth_ruuvi();
    cjson_wrap_str_t json_str = cjson_wrap_str_null();

    cJSON_Hooks hooks = {
        .malloc_fn = &os_malloc,
        .free_fn   = &os_free_internal,
    };
    cJSON_InitHooks(&hooks);
    this->m_malloc_fail_on_cnt = 174;

    ASSERT_FALSE(gw_cfg_json_generate_for_saving(&gw_cfg, &json_str));
    ASSERT_EQ(nullptr, json_str.p_str);
    TEST_CHECK_LOG_RECORD(ESP_LOG_ERROR, string("Can't add json item: fw_update_url"));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_TRUE(g_pTestClass->m_mem_alloc_trace.is_empty());
}

TEST_F(TestGwCfgJson, gw_cfg_json_generate_malloc_failed_on_fw_update_url_3) // NOLINT
{
    const gw_cfg_t   gw_cfg   = get_gateway_config_default_lan_auth_ruuvi();
    cjson_wrap_str_t json_str = cjson_wrap_str_null();

    cJSON_Hooks hooks = {
        .malloc_fn = &os_malloc,
        .free_fn   = &os_free_internal,
    };
    cJSON_InitHooks(&hooks);
    this->m_malloc_fail_on_cnt = 175;

    ASSERT_FALSE(gw_cfg_json_generate_for_saving(&gw_cfg, &json_str));
    ASSERT_EQ(nullptr, json_str.p_str);
    TEST_CHECK_LOG_RECORD(ESP_LOG_ERROR, string("Can't add json item: fw_update_url"));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_TRUE(g_pTestClass->m_mem_alloc_trace.is_empty());
}

TEST_F(TestGwCfgJson, gw_cfg_json_generate_malloc_failed_on_converting_to_json_string) // NOLINT
{
    const gw_cfg_t   gw_cfg   = get_gateway_config_default_lan_auth_ruuvi();
    cjson_wrap_str_t json_str = cjson_wrap_str_null();

    cJSON_Hooks hooks = {
        .malloc_fn = &os_malloc,
        .free_fn   = &os_free_internal,
    };
    cJSON_InitHooks(&hooks);
    this->m_malloc_fail_on_cnt = 176;

    ASSERT_FALSE(gw_cfg_json_generate_for_saving(&gw_cfg, &json_str));
    ASSERT_EQ(nullptr, json_str.p_str);
    TEST_CHECK_LOG_RECORD(ESP_LOG_ERROR, string("Can't create json string"));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_TRUE(g_pTestClass->m_mem_alloc_trace.is_empty());
}
