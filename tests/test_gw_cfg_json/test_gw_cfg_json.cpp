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
#include "gw_cfg_json.h"
#include "esp_log_wrapper.hpp"
#include "os_mutex_recursive.h"

using namespace std;

/*** Google-test class implementation
 * *********************************************************************************/

class TestGwCfgJson;
static TestGwCfgJson *g_pTestClass;

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
        g_pTestClass         = this;
        this->m_fw_ver       = string("v1.3.3");
        this->m_nrf52_fw_ver = string("v0.7.1");
        gw_cfg_init();

        this->m_mem_alloc_trace.clear();
        this->m_malloc_cnt         = 0;
        this->m_malloc_fail_on_cnt = 0;

        gw_cfg_default_set_lan_auth_password("\xFFpassword_md5\xFF");
    }

    void
    TearDown() override
    {
        g_pTestClass = nullptr;
        esp_log_wrapper_deinit();
    }

public:
    TestGwCfgJson();

    ~TestGwCfgJson() override;

    MemAllocTrace m_mem_alloc_trace;
    uint32_t      m_malloc_cnt {};
    uint32_t      m_malloc_fail_on_cnt {};
    string        m_fw_ver {};
    string        m_nrf52_fw_ver {};
};

TestGwCfgJson::TestGwCfgJson()
    : Test()
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
    return nullptr;
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

TestGwCfgJson::~TestGwCfgJson() = default;

#define TEST_CHECK_LOG_RECORD(level_, msg_) ESP_LOG_WRAPPER_TEST_CHECK_LOG_RECORD("gw_cfg", level_, msg_)

static ruuvi_gateway_config_t
get_gateway_config_default()
{
    ruuvi_gateway_config_t gw_cfg {};
    gw_cfg_default_get(&gw_cfg);
    return gw_cfg;
}

/*** Unit-Tests
 * *******************************************************************************************************/

TEST_F(TestGwCfgJson, gw_cfg_json_generate_default) // NOLINT
{
    const ruuvi_gateway_config_t gw_cfg   = get_gateway_config_default();
    cjson_wrap_str_t             json_str = cjson_wrap_str_null();

    ASSERT_TRUE(gw_cfg_json_generate(&gw_cfg, &json_str));
    ASSERT_NE(nullptr, json_str.p_str);
    ASSERT_EQ(
        string("{\n"
               "\t\"use_eth\":\tfalse,\n"
               "\t\"eth_dhcp\":\ttrue,\n"
               "\t\"eth_static_ip\":\t\"\",\n"
               "\t\"eth_netmask\":\t\"\",\n"
               "\t\"eth_gw\":\t\"\",\n"
               "\t\"eth_dns1\":\t\"\",\n"
               "\t\"eth_dns2\":\t\"\",\n"
               "\t\"use_http\":\ttrue,\n"
               "\t\"http_url\":\t\"" RUUVI_GATEWAY_HTTP_DEFAULT_URL "\",\n"
               "\t\"http_user\":\t\"\",\n"
               "\t\"http_pass\":\t\"\",\n"
               "\t\"use_http_stat\":\ttrue,\n"
               "\t\"http_stat_url\":\t\"" RUUVI_GATEWAY_HTTP_STATUS_URL "\",\n"
               "\t\"http_stat_user\":\t\"\",\n"
               "\t\"http_stat_pass\":\t\"\",\n"
               "\t\"use_mqtt\":\tfalse,\n"
               "\t\"mqtt_use_default_prefix\":\ttrue,\n"
               "\t\"mqtt_transport\":\t\"TCP\",\n"
               "\t\"mqtt_server\":\t\"\",\n"
               "\t\"mqtt_port\":\t0,\n"
               "\t\"mqtt_prefix\":\t\"\",\n"
               "\t\"mqtt_client_id\":\t\"\",\n"
               "\t\"mqtt_user\":\t\"\",\n"
               "\t\"mqtt_pass\":\t\"\",\n"
               "\t\"lan_auth_type\":\t\"lan_auth_ruuvi\",\n"
               "\t\"lan_auth_user\":\t\"Admin\",\n"
               "\t\"lan_auth_pass\":\t\"\xFFpassword_md5\xFF\",\n"
               "\t\"lan_auth_api_key\":\t\"\",\n"
               "\t\"auto_update_cycle\":\t\"regular\",\n"
               "\t\"auto_update_weekdays_bitmask\":\t127,\n"
               "\t\"auto_update_interval_from\":\t0,\n"
               "\t\"auto_update_interval_to\":\t24,\n"
               "\t\"auto_update_tz_offset_hours\":\t3,\n"
               "\t\"company_id\":\t1177,\n"
               "\t\"company_use_filtering\":\ttrue,\n"
               "\t\"scan_coded_phy\":\tfalse,\n"
               "\t\"scan_1mbit_phy\":\ttrue,\n"
               "\t\"scan_extended_payload\":\ttrue,\n"
               "\t\"scan_channel_37\":\ttrue,\n"
               "\t\"scan_channel_38\":\ttrue,\n"
               "\t\"scan_channel_39\":\ttrue,\n"
               "\t\"coordinates\":\t\"\"\n"
               "}"),
        string(json_str.p_str));
    ASSERT_TRUE(esp_log_wrapper_is_empty());

    ruuvi_gateway_config_t gw_cfg2 = {};
    ASSERT_TRUE(gw_cfg_json_parse(json_str.p_str, &gw_cfg2));
    cjson_wrap_free_json_str(&json_str);

    ASSERT_TRUE(0 == memcmp(&gw_cfg, &gw_cfg2, sizeof(gw_cfg)));
}

TEST_F(TestGwCfgJson, gw_cfg_json_generate_eth_disabled) // NOLINT
{
    ruuvi_gateway_config_t gw_cfg   = get_gateway_config_default();
    cjson_wrap_str_t       json_str = cjson_wrap_str_null();

    gw_cfg.eth.use_eth = false;

    ASSERT_TRUE(gw_cfg_json_generate(&gw_cfg, &json_str));
    ASSERT_NE(nullptr, json_str.p_str);
    ASSERT_EQ(
        string("{\n"
               "\t\"use_eth\":\tfalse,\n"
               "\t\"eth_dhcp\":\ttrue,\n"
               "\t\"eth_static_ip\":\t\"\",\n"
               "\t\"eth_netmask\":\t\"\",\n"
               "\t\"eth_gw\":\t\"\",\n"
               "\t\"eth_dns1\":\t\"\",\n"
               "\t\"eth_dns2\":\t\"\",\n"
               "\t\"use_http\":\ttrue,\n"
               "\t\"http_url\":\t\"" RUUVI_GATEWAY_HTTP_DEFAULT_URL "\",\n"
               "\t\"http_user\":\t\"\",\n"
               "\t\"http_pass\":\t\"\",\n"
               "\t\"use_http_stat\":\ttrue,\n"
               "\t\"http_stat_url\":\t\"" RUUVI_GATEWAY_HTTP_STATUS_URL "\",\n"
               "\t\"http_stat_user\":\t\"\",\n"
               "\t\"http_stat_pass\":\t\"\",\n"
               "\t\"use_mqtt\":\tfalse,\n"
               "\t\"mqtt_use_default_prefix\":\ttrue,\n"
               "\t\"mqtt_transport\":\t\"TCP\",\n"
               "\t\"mqtt_server\":\t\"\",\n"
               "\t\"mqtt_port\":\t0,\n"
               "\t\"mqtt_prefix\":\t\"\",\n"
               "\t\"mqtt_client_id\":\t\"\",\n"
               "\t\"mqtt_user\":\t\"\",\n"
               "\t\"mqtt_pass\":\t\"\",\n"
               "\t\"lan_auth_type\":\t\"lan_auth_ruuvi\",\n"
               "\t\"lan_auth_user\":\t\"Admin\",\n"
               "\t\"lan_auth_pass\":\t\"\xFFpassword_md5\xFF\",\n"
               "\t\"lan_auth_api_key\":\t\"\",\n"
               "\t\"auto_update_cycle\":\t\"regular\",\n"
               "\t\"auto_update_weekdays_bitmask\":\t127,\n"
               "\t\"auto_update_interval_from\":\t0,\n"
               "\t\"auto_update_interval_to\":\t24,\n"
               "\t\"auto_update_tz_offset_hours\":\t3,\n"
               "\t\"company_id\":\t1177,\n"
               "\t\"company_use_filtering\":\ttrue,\n"
               "\t\"scan_coded_phy\":\tfalse,\n"
               "\t\"scan_1mbit_phy\":\ttrue,\n"
               "\t\"scan_extended_payload\":\ttrue,\n"
               "\t\"scan_channel_37\":\ttrue,\n"
               "\t\"scan_channel_38\":\ttrue,\n"
               "\t\"scan_channel_39\":\ttrue,\n"
               "\t\"coordinates\":\t\"\"\n"
               "}"),
        string(json_str.p_str));
    ASSERT_TRUE(esp_log_wrapper_is_empty());

    ruuvi_gateway_config_t gw_cfg2 = {};
    ASSERT_TRUE(gw_cfg_json_parse(json_str.p_str, &gw_cfg2));
    cjson_wrap_free_json_str(&json_str);

    ASSERT_TRUE(0 == memcmp(&gw_cfg, &gw_cfg2, sizeof(gw_cfg)));
}

TEST_F(TestGwCfgJson, gw_cfg_json_generate_eth_enabled_dhcp_enabled) // NOLINT
{
    ruuvi_gateway_config_t gw_cfg   = get_gateway_config_default();
    cjson_wrap_str_t       json_str = cjson_wrap_str_null();

    gw_cfg.eth.use_eth  = true;
    gw_cfg.eth.eth_dhcp = true;

    ASSERT_TRUE(gw_cfg_json_generate(&gw_cfg, &json_str));
    ASSERT_NE(nullptr, json_str.p_str);
    ASSERT_EQ(
        string("{\n"
               "\t\"use_eth\":\ttrue,\n"
               "\t\"eth_dhcp\":\ttrue,\n"
               "\t\"eth_static_ip\":\t\"\",\n"
               "\t\"eth_netmask\":\t\"\",\n"
               "\t\"eth_gw\":\t\"\",\n"
               "\t\"eth_dns1\":\t\"\",\n"
               "\t\"eth_dns2\":\t\"\",\n"
               "\t\"use_http\":\ttrue,\n"
               "\t\"http_url\":\t\"" RUUVI_GATEWAY_HTTP_DEFAULT_URL "\",\n"
               "\t\"http_user\":\t\"\",\n"
               "\t\"http_pass\":\t\"\",\n"
               "\t\"use_http_stat\":\ttrue,\n"
               "\t\"http_stat_url\":\t\"" RUUVI_GATEWAY_HTTP_STATUS_URL "\",\n"
               "\t\"http_stat_user\":\t\"\",\n"
               "\t\"http_stat_pass\":\t\"\",\n"
               "\t\"use_mqtt\":\tfalse,\n"
               "\t\"mqtt_use_default_prefix\":\ttrue,\n"
               "\t\"mqtt_transport\":\t\"TCP\",\n"
               "\t\"mqtt_server\":\t\"\",\n"
               "\t\"mqtt_port\":\t0,\n"
               "\t\"mqtt_prefix\":\t\"\",\n"
               "\t\"mqtt_client_id\":\t\"\",\n"
               "\t\"mqtt_user\":\t\"\",\n"
               "\t\"mqtt_pass\":\t\"\",\n"
               "\t\"lan_auth_type\":\t\"lan_auth_ruuvi\",\n"
               "\t\"lan_auth_user\":\t\"Admin\",\n"
               "\t\"lan_auth_pass\":\t\"\xFFpassword_md5\xFF\",\n"
               "\t\"lan_auth_api_key\":\t\"\",\n"
               "\t\"auto_update_cycle\":\t\"regular\",\n"
               "\t\"auto_update_weekdays_bitmask\":\t127,\n"
               "\t\"auto_update_interval_from\":\t0,\n"
               "\t\"auto_update_interval_to\":\t24,\n"
               "\t\"auto_update_tz_offset_hours\":\t3,\n"
               "\t\"company_id\":\t1177,\n"
               "\t\"company_use_filtering\":\ttrue,\n"
               "\t\"scan_coded_phy\":\tfalse,\n"
               "\t\"scan_1mbit_phy\":\ttrue,\n"
               "\t\"scan_extended_payload\":\ttrue,\n"
               "\t\"scan_channel_37\":\ttrue,\n"
               "\t\"scan_channel_38\":\ttrue,\n"
               "\t\"scan_channel_39\":\ttrue,\n"
               "\t\"coordinates\":\t\"\"\n"
               "}"),
        string(json_str.p_str));
    ASSERT_TRUE(esp_log_wrapper_is_empty());

    ruuvi_gateway_config_t gw_cfg2 = {};
    ASSERT_TRUE(gw_cfg_json_parse(json_str.p_str, &gw_cfg2));
    cjson_wrap_free_json_str(&json_str);

    ASSERT_TRUE(0 == memcmp(&gw_cfg, &gw_cfg2, sizeof(gw_cfg)));
}

TEST_F(TestGwCfgJson, gw_cfg_json_generate_eth_enabled_dhcp_disabled) // NOLINT
{
    ruuvi_gateway_config_t gw_cfg   = get_gateway_config_default();
    cjson_wrap_str_t       json_str = cjson_wrap_str_null();

    gw_cfg.eth.use_eth  = true;
    gw_cfg.eth.eth_dhcp = false;
    snprintf(gw_cfg.eth.eth_static_ip.buf, sizeof(gw_cfg.eth.eth_static_ip.buf), "192.168.1.10");
    snprintf(gw_cfg.eth.eth_netmask.buf, sizeof(gw_cfg.eth.eth_netmask.buf), "255.255.255.0");
    snprintf(gw_cfg.eth.eth_gw.buf, sizeof(gw_cfg.eth.eth_gw.buf), "192.168.1.1");
    snprintf(gw_cfg.eth.eth_dns1.buf, sizeof(gw_cfg.eth.eth_dns1.buf), "8.8.8.8");
    snprintf(gw_cfg.eth.eth_dns2.buf, sizeof(gw_cfg.eth.eth_dns2.buf), "4.4.4.4");

    ASSERT_TRUE(gw_cfg_json_generate(&gw_cfg, &json_str));
    ASSERT_NE(nullptr, json_str.p_str);
    ASSERT_EQ(
        string("{\n"
               "\t\"use_eth\":\ttrue,\n"
               "\t\"eth_dhcp\":\tfalse,\n"
               "\t\"eth_static_ip\":\t\"192.168.1.10\",\n"
               "\t\"eth_netmask\":\t\"255.255.255.0\",\n"
               "\t\"eth_gw\":\t\"192.168.1.1\",\n"
               "\t\"eth_dns1\":\t\"8.8.8.8\",\n"
               "\t\"eth_dns2\":\t\"4.4.4.4\",\n"
               "\t\"use_http\":\ttrue,\n"
               "\t\"http_url\":\t\"" RUUVI_GATEWAY_HTTP_DEFAULT_URL "\",\n"
               "\t\"http_user\":\t\"\",\n"
               "\t\"http_pass\":\t\"\",\n"
               "\t\"use_http_stat\":\ttrue,\n"
               "\t\"http_stat_url\":\t\"" RUUVI_GATEWAY_HTTP_STATUS_URL "\",\n"
               "\t\"http_stat_user\":\t\"\",\n"
               "\t\"http_stat_pass\":\t\"\",\n"
               "\t\"use_mqtt\":\tfalse,\n"
               "\t\"mqtt_use_default_prefix\":\ttrue,\n"
               "\t\"mqtt_transport\":\t\"TCP\",\n"
               "\t\"mqtt_server\":\t\"\",\n"
               "\t\"mqtt_port\":\t0,\n"
               "\t\"mqtt_prefix\":\t\"\",\n"
               "\t\"mqtt_client_id\":\t\"\",\n"
               "\t\"mqtt_user\":\t\"\",\n"
               "\t\"mqtt_pass\":\t\"\",\n"
               "\t\"lan_auth_type\":\t\"lan_auth_ruuvi\",\n"
               "\t\"lan_auth_user\":\t\"Admin\",\n"
               "\t\"lan_auth_pass\":\t\"\xFFpassword_md5\xFF\",\n"
               "\t\"lan_auth_api_key\":\t\"\",\n"
               "\t\"auto_update_cycle\":\t\"regular\",\n"
               "\t\"auto_update_weekdays_bitmask\":\t127,\n"
               "\t\"auto_update_interval_from\":\t0,\n"
               "\t\"auto_update_interval_to\":\t24,\n"
               "\t\"auto_update_tz_offset_hours\":\t3,\n"
               "\t\"company_id\":\t1177,\n"
               "\t\"company_use_filtering\":\ttrue,\n"
               "\t\"scan_coded_phy\":\tfalse,\n"
               "\t\"scan_1mbit_phy\":\ttrue,\n"
               "\t\"scan_extended_payload\":\ttrue,\n"
               "\t\"scan_channel_37\":\ttrue,\n"
               "\t\"scan_channel_38\":\ttrue,\n"
               "\t\"scan_channel_39\":\ttrue,\n"
               "\t\"coordinates\":\t\"\"\n"
               "}"),
        string(json_str.p_str));
    ASSERT_TRUE(esp_log_wrapper_is_empty());

    ruuvi_gateway_config_t gw_cfg2 = {};
    ASSERT_TRUE(gw_cfg_json_parse(json_str.p_str, &gw_cfg2));
    cjson_wrap_free_json_str(&json_str);

    ASSERT_TRUE(0 == memcmp(&gw_cfg, &gw_cfg2, sizeof(gw_cfg)));
}

TEST_F(TestGwCfgJson, gw_cfg_json_generate_mqtt_disabled) // NOLINT
{
    ruuvi_gateway_config_t gw_cfg   = get_gateway_config_default();
    cjson_wrap_str_t       json_str = cjson_wrap_str_null();

    gw_cfg.mqtt.use_mqtt = false;

    ASSERT_TRUE(gw_cfg_json_generate(&gw_cfg, &json_str));
    ASSERT_NE(nullptr, json_str.p_str);
    ASSERT_EQ(
        string("{\n"
               "\t\"use_eth\":\tfalse,\n"
               "\t\"eth_dhcp\":\ttrue,\n"
               "\t\"eth_static_ip\":\t\"\",\n"
               "\t\"eth_netmask\":\t\"\",\n"
               "\t\"eth_gw\":\t\"\",\n"
               "\t\"eth_dns1\":\t\"\",\n"
               "\t\"eth_dns2\":\t\"\",\n"
               "\t\"use_http\":\ttrue,\n"
               "\t\"http_url\":\t\"" RUUVI_GATEWAY_HTTP_DEFAULT_URL "\",\n"
               "\t\"http_user\":\t\"\",\n"
               "\t\"http_pass\":\t\"\",\n"
               "\t\"use_http_stat\":\ttrue,\n"
               "\t\"http_stat_url\":\t\"" RUUVI_GATEWAY_HTTP_STATUS_URL "\",\n"
               "\t\"http_stat_user\":\t\"\",\n"
               "\t\"http_stat_pass\":\t\"\",\n"
               "\t\"use_mqtt\":\tfalse,\n"
               "\t\"mqtt_use_default_prefix\":\ttrue,\n"
               "\t\"mqtt_transport\":\t\"TCP\",\n"
               "\t\"mqtt_server\":\t\"\",\n"
               "\t\"mqtt_port\":\t0,\n"
               "\t\"mqtt_prefix\":\t\"\",\n"
               "\t\"mqtt_client_id\":\t\"\",\n"
               "\t\"mqtt_user\":\t\"\",\n"
               "\t\"mqtt_pass\":\t\"\",\n"
               "\t\"lan_auth_type\":\t\"lan_auth_ruuvi\",\n"
               "\t\"lan_auth_user\":\t\"Admin\",\n"
               "\t\"lan_auth_pass\":\t\"\xFFpassword_md5\xFF\",\n"
               "\t\"lan_auth_api_key\":\t\"\",\n"
               "\t\"auto_update_cycle\":\t\"regular\",\n"
               "\t\"auto_update_weekdays_bitmask\":\t127,\n"
               "\t\"auto_update_interval_from\":\t0,\n"
               "\t\"auto_update_interval_to\":\t24,\n"
               "\t\"auto_update_tz_offset_hours\":\t3,\n"
               "\t\"company_id\":\t1177,\n"
               "\t\"company_use_filtering\":\ttrue,\n"
               "\t\"scan_coded_phy\":\tfalse,\n"
               "\t\"scan_1mbit_phy\":\ttrue,\n"
               "\t\"scan_extended_payload\":\ttrue,\n"
               "\t\"scan_channel_37\":\ttrue,\n"
               "\t\"scan_channel_38\":\ttrue,\n"
               "\t\"scan_channel_39\":\ttrue,\n"
               "\t\"coordinates\":\t\"\"\n"
               "}"),
        string(json_str.p_str));
    ASSERT_TRUE(esp_log_wrapper_is_empty());

    ruuvi_gateway_config_t gw_cfg2 = {};
    ASSERT_TRUE(gw_cfg_json_parse(json_str.p_str, &gw_cfg2));
    cjson_wrap_free_json_str(&json_str);

    ASSERT_TRUE(0 == memcmp(&gw_cfg, &gw_cfg2, sizeof(gw_cfg)));
}

TEST_F(TestGwCfgJson, gw_cfg_json_generate_mqtt_enabled_TCP) // NOLINT
{
    ruuvi_gateway_config_t gw_cfg   = get_gateway_config_default();
    cjson_wrap_str_t       json_str = cjson_wrap_str_null();

    gw_cfg.mqtt.use_mqtt                = true;
    gw_cfg.mqtt.mqtt_use_default_prefix = false;
    snprintf(gw_cfg.mqtt.mqtt_transport.buf, sizeof(gw_cfg.mqtt.mqtt_transport.buf), MQTT_TRANSPORT_TCP);
    snprintf(gw_cfg.mqtt.mqtt_server.buf, sizeof(gw_cfg.mqtt.mqtt_server.buf), "mqtt_server1.com");
    gw_cfg.mqtt.mqtt_port = 1339;
    snprintf(gw_cfg.mqtt.mqtt_prefix.buf, sizeof(gw_cfg.mqtt.mqtt_prefix.buf), "prefix1");
    snprintf(gw_cfg.mqtt.mqtt_client_id.buf, sizeof(gw_cfg.mqtt.mqtt_client_id.buf), "client123");
    snprintf(gw_cfg.mqtt.mqtt_user.buf, sizeof(gw_cfg.mqtt.mqtt_user.buf), "user1");
    snprintf(gw_cfg.mqtt.mqtt_pass.buf, sizeof(gw_cfg.mqtt.mqtt_pass.buf), "pass1");

    ASSERT_TRUE(gw_cfg_json_generate(&gw_cfg, &json_str));
    ASSERT_NE(nullptr, json_str.p_str);
    ASSERT_EQ(
        string("{\n"
               "\t\"use_eth\":\tfalse,\n"
               "\t\"eth_dhcp\":\ttrue,\n"
               "\t\"eth_static_ip\":\t\"\",\n"
               "\t\"eth_netmask\":\t\"\",\n"
               "\t\"eth_gw\":\t\"\",\n"
               "\t\"eth_dns1\":\t\"\",\n"
               "\t\"eth_dns2\":\t\"\",\n"
               "\t\"use_http\":\ttrue,\n"
               "\t\"http_url\":\t\"" RUUVI_GATEWAY_HTTP_DEFAULT_URL "\",\n"
               "\t\"http_user\":\t\"\",\n"
               "\t\"http_pass\":\t\"\",\n"
               "\t\"use_http_stat\":\ttrue,\n"
               "\t\"http_stat_url\":\t\"" RUUVI_GATEWAY_HTTP_STATUS_URL "\",\n"
               "\t\"http_stat_user\":\t\"\",\n"
               "\t\"http_stat_pass\":\t\"\",\n"
               "\t\"use_mqtt\":\ttrue,\n"
               "\t\"mqtt_use_default_prefix\":\tfalse,\n"
               "\t\"mqtt_transport\":\t\"" MQTT_TRANSPORT_TCP "\",\n"
               "\t\"mqtt_server\":\t\"mqtt_server1.com\",\n"
               "\t\"mqtt_port\":\t1339,\n"
               "\t\"mqtt_prefix\":\t\"prefix1\",\n"
               "\t\"mqtt_client_id\":\t\"client123\",\n"
               "\t\"mqtt_user\":\t\"user1\",\n"
               "\t\"mqtt_pass\":\t\"pass1\",\n"
               "\t\"lan_auth_type\":\t\"lan_auth_ruuvi\",\n"
               "\t\"lan_auth_user\":\t\"Admin\",\n"
               "\t\"lan_auth_pass\":\t\"\xFFpassword_md5\xFF\",\n"
               "\t\"lan_auth_api_key\":\t\"\",\n"
               "\t\"auto_update_cycle\":\t\"regular\",\n"
               "\t\"auto_update_weekdays_bitmask\":\t127,\n"
               "\t\"auto_update_interval_from\":\t0,\n"
               "\t\"auto_update_interval_to\":\t24,\n"
               "\t\"auto_update_tz_offset_hours\":\t3,\n"
               "\t\"company_id\":\t1177,\n"
               "\t\"company_use_filtering\":\ttrue,\n"
               "\t\"scan_coded_phy\":\tfalse,\n"
               "\t\"scan_1mbit_phy\":\ttrue,\n"
               "\t\"scan_extended_payload\":\ttrue,\n"
               "\t\"scan_channel_37\":\ttrue,\n"
               "\t\"scan_channel_38\":\ttrue,\n"
               "\t\"scan_channel_39\":\ttrue,\n"
               "\t\"coordinates\":\t\"\"\n"
               "}"),
        string(json_str.p_str));
    ASSERT_TRUE(esp_log_wrapper_is_empty());

    ruuvi_gateway_config_t gw_cfg2 = {};
    ASSERT_TRUE(gw_cfg_json_parse(json_str.p_str, &gw_cfg2));
    cjson_wrap_free_json_str(&json_str);

    ASSERT_TRUE(0 == memcmp(&gw_cfg, &gw_cfg2, sizeof(gw_cfg)));
}

TEST_F(TestGwCfgJson, gw_cfg_json_generate_mqtt_enabled_SSL) // NOLINT
{
    ruuvi_gateway_config_t gw_cfg   = get_gateway_config_default();
    cjson_wrap_str_t       json_str = cjson_wrap_str_null();

    gw_cfg.mqtt.use_mqtt                = true;
    gw_cfg.mqtt.mqtt_use_default_prefix = true;
    snprintf(gw_cfg.mqtt.mqtt_transport.buf, sizeof(gw_cfg.mqtt.mqtt_transport.buf), MQTT_TRANSPORT_SSL);
    snprintf(gw_cfg.mqtt.mqtt_server.buf, sizeof(gw_cfg.mqtt.mqtt_server.buf), "mqtt_server2.com");
    gw_cfg.mqtt.mqtt_port = 1340;
    snprintf(gw_cfg.mqtt.mqtt_prefix.buf, sizeof(gw_cfg.mqtt.mqtt_prefix.buf), "prefix2");
    snprintf(gw_cfg.mqtt.mqtt_client_id.buf, sizeof(gw_cfg.mqtt.mqtt_client_id.buf), "client124");
    snprintf(gw_cfg.mqtt.mqtt_user.buf, sizeof(gw_cfg.mqtt.mqtt_user.buf), "user2");
    snprintf(gw_cfg.mqtt.mqtt_pass.buf, sizeof(gw_cfg.mqtt.mqtt_pass.buf), "pass2");

    ASSERT_TRUE(gw_cfg_json_generate(&gw_cfg, &json_str));
    ASSERT_NE(nullptr, json_str.p_str);
    ASSERT_EQ(
        string("{\n"
               "\t\"use_eth\":\tfalse,\n"
               "\t\"eth_dhcp\":\ttrue,\n"
               "\t\"eth_static_ip\":\t\"\",\n"
               "\t\"eth_netmask\":\t\"\",\n"
               "\t\"eth_gw\":\t\"\",\n"
               "\t\"eth_dns1\":\t\"\",\n"
               "\t\"eth_dns2\":\t\"\",\n"
               "\t\"use_http\":\ttrue,\n"
               "\t\"http_url\":\t\"" RUUVI_GATEWAY_HTTP_DEFAULT_URL "\",\n"
               "\t\"http_user\":\t\"\",\n"
               "\t\"http_pass\":\t\"\",\n"
               "\t\"use_http_stat\":\ttrue,\n"
               "\t\"http_stat_url\":\t\"" RUUVI_GATEWAY_HTTP_STATUS_URL "\",\n"
               "\t\"http_stat_user\":\t\"\",\n"
               "\t\"http_stat_pass\":\t\"\",\n"
               "\t\"use_mqtt\":\ttrue,\n"
               "\t\"mqtt_use_default_prefix\":\ttrue,\n"
               "\t\"mqtt_transport\":\t\"" MQTT_TRANSPORT_SSL "\",\n"
               "\t\"mqtt_server\":\t\"mqtt_server2.com\",\n"
               "\t\"mqtt_port\":\t1340,\n"
               "\t\"mqtt_prefix\":\t\"prefix2\",\n"
               "\t\"mqtt_client_id\":\t\"client124\",\n"
               "\t\"mqtt_user\":\t\"user2\",\n"
               "\t\"mqtt_pass\":\t\"pass2\",\n"
               "\t\"lan_auth_type\":\t\"lan_auth_ruuvi\",\n"
               "\t\"lan_auth_user\":\t\"Admin\",\n"
               "\t\"lan_auth_pass\":\t\"\xFFpassword_md5\xFF\",\n"
               "\t\"lan_auth_api_key\":\t\"\",\n"
               "\t\"auto_update_cycle\":\t\"regular\",\n"
               "\t\"auto_update_weekdays_bitmask\":\t127,\n"
               "\t\"auto_update_interval_from\":\t0,\n"
               "\t\"auto_update_interval_to\":\t24,\n"
               "\t\"auto_update_tz_offset_hours\":\t3,\n"
               "\t\"company_id\":\t1177,\n"
               "\t\"company_use_filtering\":\ttrue,\n"
               "\t\"scan_coded_phy\":\tfalse,\n"
               "\t\"scan_1mbit_phy\":\ttrue,\n"
               "\t\"scan_extended_payload\":\ttrue,\n"
               "\t\"scan_channel_37\":\ttrue,\n"
               "\t\"scan_channel_38\":\ttrue,\n"
               "\t\"scan_channel_39\":\ttrue,\n"
               "\t\"coordinates\":\t\"\"\n"
               "}"),
        string(json_str.p_str));
    ASSERT_TRUE(esp_log_wrapper_is_empty());

    ruuvi_gateway_config_t gw_cfg2 = {};
    ASSERT_TRUE(gw_cfg_json_parse(json_str.p_str, &gw_cfg2));
    cjson_wrap_free_json_str(&json_str);

    ASSERT_TRUE(0 == memcmp(&gw_cfg, &gw_cfg2, sizeof(gw_cfg)));
}

TEST_F(TestGwCfgJson, gw_cfg_json_generate_mqtt_enabled_WS) // NOLINT
{
    ruuvi_gateway_config_t gw_cfg   = get_gateway_config_default();
    cjson_wrap_str_t       json_str = cjson_wrap_str_null();

    gw_cfg.mqtt.use_mqtt                = true;
    gw_cfg.mqtt.mqtt_use_default_prefix = true;
    snprintf(gw_cfg.mqtt.mqtt_transport.buf, sizeof(gw_cfg.mqtt.mqtt_transport.buf), MQTT_TRANSPORT_WS);
    snprintf(gw_cfg.mqtt.mqtt_server.buf, sizeof(gw_cfg.mqtt.mqtt_server.buf), "mqtt_server2.com");
    gw_cfg.mqtt.mqtt_port = 1340;
    snprintf(gw_cfg.mqtt.mqtt_prefix.buf, sizeof(gw_cfg.mqtt.mqtt_prefix.buf), "prefix2");
    snprintf(gw_cfg.mqtt.mqtt_client_id.buf, sizeof(gw_cfg.mqtt.mqtt_client_id.buf), "client124");
    snprintf(gw_cfg.mqtt.mqtt_user.buf, sizeof(gw_cfg.mqtt.mqtt_user.buf), "user2");
    snprintf(gw_cfg.mqtt.mqtt_pass.buf, sizeof(gw_cfg.mqtt.mqtt_pass.buf), "pass2");

    ASSERT_TRUE(gw_cfg_json_generate(&gw_cfg, &json_str));
    ASSERT_NE(nullptr, json_str.p_str);
    ASSERT_EQ(
        string("{\n"
               "\t\"use_eth\":\tfalse,\n"
               "\t\"eth_dhcp\":\ttrue,\n"
               "\t\"eth_static_ip\":\t\"\",\n"
               "\t\"eth_netmask\":\t\"\",\n"
               "\t\"eth_gw\":\t\"\",\n"
               "\t\"eth_dns1\":\t\"\",\n"
               "\t\"eth_dns2\":\t\"\",\n"
               "\t\"use_http\":\ttrue,\n"
               "\t\"http_url\":\t\"" RUUVI_GATEWAY_HTTP_DEFAULT_URL "\",\n"
               "\t\"http_user\":\t\"\",\n"
               "\t\"http_pass\":\t\"\",\n"
               "\t\"use_http_stat\":\ttrue,\n"
               "\t\"http_stat_url\":\t\"" RUUVI_GATEWAY_HTTP_STATUS_URL "\",\n"
               "\t\"http_stat_user\":\t\"\",\n"
               "\t\"http_stat_pass\":\t\"\",\n"
               "\t\"use_mqtt\":\ttrue,\n"
               "\t\"mqtt_use_default_prefix\":\ttrue,\n"
               "\t\"mqtt_transport\":\t\"" MQTT_TRANSPORT_WS "\",\n"
               "\t\"mqtt_server\":\t\"mqtt_server2.com\",\n"
               "\t\"mqtt_port\":\t1340,\n"
               "\t\"mqtt_prefix\":\t\"prefix2\",\n"
               "\t\"mqtt_client_id\":\t\"client124\",\n"
               "\t\"mqtt_user\":\t\"user2\",\n"
               "\t\"mqtt_pass\":\t\"pass2\",\n"
               "\t\"lan_auth_type\":\t\"lan_auth_ruuvi\",\n"
               "\t\"lan_auth_user\":\t\"Admin\",\n"
               "\t\"lan_auth_pass\":\t\"\xFFpassword_md5\xFF\",\n"
               "\t\"lan_auth_api_key\":\t\"\",\n"
               "\t\"auto_update_cycle\":\t\"regular\",\n"
               "\t\"auto_update_weekdays_bitmask\":\t127,\n"
               "\t\"auto_update_interval_from\":\t0,\n"
               "\t\"auto_update_interval_to\":\t24,\n"
               "\t\"auto_update_tz_offset_hours\":\t3,\n"
               "\t\"company_id\":\t1177,\n"
               "\t\"company_use_filtering\":\ttrue,\n"
               "\t\"scan_coded_phy\":\tfalse,\n"
               "\t\"scan_1mbit_phy\":\ttrue,\n"
               "\t\"scan_extended_payload\":\ttrue,\n"
               "\t\"scan_channel_37\":\ttrue,\n"
               "\t\"scan_channel_38\":\ttrue,\n"
               "\t\"scan_channel_39\":\ttrue,\n"
               "\t\"coordinates\":\t\"\"\n"
               "}"),
        string(json_str.p_str));
    ASSERT_TRUE(esp_log_wrapper_is_empty());

    ruuvi_gateway_config_t gw_cfg2 = {};
    ASSERT_TRUE(gw_cfg_json_parse(json_str.p_str, &gw_cfg2));
    cjson_wrap_free_json_str(&json_str);

    ASSERT_TRUE(0 == memcmp(&gw_cfg, &gw_cfg2, sizeof(gw_cfg)));
}

TEST_F(TestGwCfgJson, gw_cfg_json_generate_mqtt_enabled_WSS) // NOLINT
{
    ruuvi_gateway_config_t gw_cfg   = get_gateway_config_default();
    cjson_wrap_str_t       json_str = cjson_wrap_str_null();

    gw_cfg.mqtt.use_mqtt                = true;
    gw_cfg.mqtt.mqtt_use_default_prefix = true;
    snprintf(gw_cfg.mqtt.mqtt_transport.buf, sizeof(gw_cfg.mqtt.mqtt_transport.buf), MQTT_TRANSPORT_WSS);
    snprintf(gw_cfg.mqtt.mqtt_server.buf, sizeof(gw_cfg.mqtt.mqtt_server.buf), "mqtt_server2.com");
    gw_cfg.mqtt.mqtt_port = 1340;
    snprintf(gw_cfg.mqtt.mqtt_prefix.buf, sizeof(gw_cfg.mqtt.mqtt_prefix.buf), "prefix2");
    snprintf(gw_cfg.mqtt.mqtt_client_id.buf, sizeof(gw_cfg.mqtt.mqtt_client_id.buf), "client124");
    snprintf(gw_cfg.mqtt.mqtt_user.buf, sizeof(gw_cfg.mqtt.mqtt_user.buf), "user2");
    snprintf(gw_cfg.mqtt.mqtt_pass.buf, sizeof(gw_cfg.mqtt.mqtt_pass.buf), "pass2");

    ASSERT_TRUE(gw_cfg_json_generate(&gw_cfg, &json_str));
    ASSERT_NE(nullptr, json_str.p_str);
    ASSERT_EQ(
        string("{\n"
               "\t\"use_eth\":\tfalse,\n"
               "\t\"eth_dhcp\":\ttrue,\n"
               "\t\"eth_static_ip\":\t\"\",\n"
               "\t\"eth_netmask\":\t\"\",\n"
               "\t\"eth_gw\":\t\"\",\n"
               "\t\"eth_dns1\":\t\"\",\n"
               "\t\"eth_dns2\":\t\"\",\n"
               "\t\"use_http\":\ttrue,\n"
               "\t\"http_url\":\t\"" RUUVI_GATEWAY_HTTP_DEFAULT_URL "\",\n"
               "\t\"http_user\":\t\"\",\n"
               "\t\"http_pass\":\t\"\",\n"
               "\t\"use_http_stat\":\ttrue,\n"
               "\t\"http_stat_url\":\t\"" RUUVI_GATEWAY_HTTP_STATUS_URL "\",\n"
               "\t\"http_stat_user\":\t\"\",\n"
               "\t\"http_stat_pass\":\t\"\",\n"
               "\t\"use_mqtt\":\ttrue,\n"
               "\t\"mqtt_use_default_prefix\":\ttrue,\n"
               "\t\"mqtt_transport\":\t\"" MQTT_TRANSPORT_WSS "\",\n"
               "\t\"mqtt_server\":\t\"mqtt_server2.com\",\n"
               "\t\"mqtt_port\":\t1340,\n"
               "\t\"mqtt_prefix\":\t\"prefix2\",\n"
               "\t\"mqtt_client_id\":\t\"client124\",\n"
               "\t\"mqtt_user\":\t\"user2\",\n"
               "\t\"mqtt_pass\":\t\"pass2\",\n"
               "\t\"lan_auth_type\":\t\"lan_auth_ruuvi\",\n"
               "\t\"lan_auth_user\":\t\"Admin\",\n"
               "\t\"lan_auth_pass\":\t\"\xFFpassword_md5\xFF\",\n"
               "\t\"lan_auth_api_key\":\t\"\",\n"
               "\t\"auto_update_cycle\":\t\"regular\",\n"
               "\t\"auto_update_weekdays_bitmask\":\t127,\n"
               "\t\"auto_update_interval_from\":\t0,\n"
               "\t\"auto_update_interval_to\":\t24,\n"
               "\t\"auto_update_tz_offset_hours\":\t3,\n"
               "\t\"company_id\":\t1177,\n"
               "\t\"company_use_filtering\":\ttrue,\n"
               "\t\"scan_coded_phy\":\tfalse,\n"
               "\t\"scan_1mbit_phy\":\ttrue,\n"
               "\t\"scan_extended_payload\":\ttrue,\n"
               "\t\"scan_channel_37\":\ttrue,\n"
               "\t\"scan_channel_38\":\ttrue,\n"
               "\t\"scan_channel_39\":\ttrue,\n"
               "\t\"coordinates\":\t\"\"\n"
               "}"),
        string(json_str.p_str));
    ASSERT_TRUE(esp_log_wrapper_is_empty());

    ruuvi_gateway_config_t gw_cfg2 = {};
    ASSERT_TRUE(gw_cfg_json_parse(json_str.p_str, &gw_cfg2));
    cjson_wrap_free_json_str(&json_str);

    ASSERT_TRUE(0 == memcmp(&gw_cfg, &gw_cfg2, sizeof(gw_cfg)));
}

TEST_F(TestGwCfgJson, gw_cfg_json_generate_http_disabled) // NOLINT
{
    ruuvi_gateway_config_t gw_cfg   = get_gateway_config_default();
    cjson_wrap_str_t       json_str = cjson_wrap_str_null();

    gw_cfg.http.use_http = false;
    ASSERT_TRUE(gw_cfg_json_generate(&gw_cfg, &json_str));
    ASSERT_NE(nullptr, json_str.p_str);
    ASSERT_EQ(
        string("{\n"
               "\t\"use_eth\":\tfalse,\n"
               "\t\"eth_dhcp\":\ttrue,\n"
               "\t\"eth_static_ip\":\t\"\",\n"
               "\t\"eth_netmask\":\t\"\",\n"
               "\t\"eth_gw\":\t\"\",\n"
               "\t\"eth_dns1\":\t\"\",\n"
               "\t\"eth_dns2\":\t\"\",\n"
               "\t\"use_http\":\tfalse,\n"
               "\t\"http_url\":\t\"" RUUVI_GATEWAY_HTTP_DEFAULT_URL "\",\n"
               "\t\"http_user\":\t\"\",\n"
               "\t\"http_pass\":\t\"\",\n"
               "\t\"use_http_stat\":\ttrue,\n"
               "\t\"http_stat_url\":\t\"" RUUVI_GATEWAY_HTTP_STATUS_URL "\",\n"
               "\t\"http_stat_user\":\t\"\",\n"
               "\t\"http_stat_pass\":\t\"\",\n"
               "\t\"use_mqtt\":\tfalse,\n"
               "\t\"mqtt_use_default_prefix\":\ttrue,\n"
               "\t\"mqtt_transport\":\t\"TCP\",\n"
               "\t\"mqtt_server\":\t\"\",\n"
               "\t\"mqtt_port\":\t0,\n"
               "\t\"mqtt_prefix\":\t\"\",\n"
               "\t\"mqtt_client_id\":\t\"\",\n"
               "\t\"mqtt_user\":\t\"\",\n"
               "\t\"mqtt_pass\":\t\"\",\n"
               "\t\"lan_auth_type\":\t\"lan_auth_ruuvi\",\n"
               "\t\"lan_auth_user\":\t\"Admin\",\n"
               "\t\"lan_auth_pass\":\t\"\xFFpassword_md5\xFF\",\n"
               "\t\"lan_auth_api_key\":\t\"\",\n"
               "\t\"auto_update_cycle\":\t\"regular\",\n"
               "\t\"auto_update_weekdays_bitmask\":\t127,\n"
               "\t\"auto_update_interval_from\":\t0,\n"
               "\t\"auto_update_interval_to\":\t24,\n"
               "\t\"auto_update_tz_offset_hours\":\t3,\n"
               "\t\"company_id\":\t1177,\n"
               "\t\"company_use_filtering\":\ttrue,\n"
               "\t\"scan_coded_phy\":\tfalse,\n"
               "\t\"scan_1mbit_phy\":\ttrue,\n"
               "\t\"scan_extended_payload\":\ttrue,\n"
               "\t\"scan_channel_37\":\ttrue,\n"
               "\t\"scan_channel_38\":\ttrue,\n"
               "\t\"scan_channel_39\":\ttrue,\n"
               "\t\"coordinates\":\t\"\"\n"
               "}"),
        string(json_str.p_str));
    ASSERT_TRUE(esp_log_wrapper_is_empty());

    ruuvi_gateway_config_t gw_cfg2 = {};
    ASSERT_TRUE(gw_cfg_json_parse(json_str.p_str, &gw_cfg2));
    cjson_wrap_free_json_str(&json_str);

    ASSERT_TRUE(0 == memcmp(&gw_cfg, &gw_cfg2, sizeof(gw_cfg)));
}

TEST_F(TestGwCfgJson, gw_cfg_json_generate_http_enabled) // NOLINT
{
    ruuvi_gateway_config_t gw_cfg   = get_gateway_config_default();
    cjson_wrap_str_t       json_str = cjson_wrap_str_null();

    gw_cfg.http.use_http = true;
    snprintf(gw_cfg.http.http_url.buf, sizeof(gw_cfg.http.http_url.buf), "https://my_url1.com/status");
    snprintf(gw_cfg.http.http_user.buf, sizeof(gw_cfg.http.http_user.buf), "user2");
    snprintf(gw_cfg.http.http_pass.buf, sizeof(gw_cfg.http.http_pass.buf), "pass2");
    ASSERT_TRUE(gw_cfg_json_generate(&gw_cfg, &json_str));
    ASSERT_NE(nullptr, json_str.p_str);
    ASSERT_EQ(
        string("{\n"
               "\t\"use_eth\":\tfalse,\n"
               "\t\"eth_dhcp\":\ttrue,\n"
               "\t\"eth_static_ip\":\t\"\",\n"
               "\t\"eth_netmask\":\t\"\",\n"
               "\t\"eth_gw\":\t\"\",\n"
               "\t\"eth_dns1\":\t\"\",\n"
               "\t\"eth_dns2\":\t\"\",\n"
               "\t\"use_http\":\ttrue,\n"
               "\t\"http_url\":\t\""
               "https://my_url1.com/status"
               "\",\n"
               "\t\"http_user\":\t\"user2\",\n"
               "\t\"http_pass\":\t\"pass2\",\n"
               "\t\"use_http_stat\":\ttrue,\n"
               "\t\"http_stat_url\":\t\"" RUUVI_GATEWAY_HTTP_STATUS_URL "\",\n"
               "\t\"http_stat_user\":\t\"\",\n"
               "\t\"http_stat_pass\":\t\"\",\n"
               "\t\"use_mqtt\":\tfalse,\n"
               "\t\"mqtt_use_default_prefix\":\ttrue,\n"
               "\t\"mqtt_transport\":\t\"TCP\",\n"
               "\t\"mqtt_server\":\t\"\",\n"
               "\t\"mqtt_port\":\t0,\n"
               "\t\"mqtt_prefix\":\t\"\",\n"
               "\t\"mqtt_client_id\":\t\"\",\n"
               "\t\"mqtt_user\":\t\"\",\n"
               "\t\"mqtt_pass\":\t\"\",\n"
               "\t\"lan_auth_type\":\t\"lan_auth_ruuvi\",\n"
               "\t\"lan_auth_user\":\t\"Admin\",\n"
               "\t\"lan_auth_pass\":\t\"\xFFpassword_md5\xFF\",\n"
               "\t\"lan_auth_api_key\":\t\"\",\n"
               "\t\"auto_update_cycle\":\t\"regular\",\n"
               "\t\"auto_update_weekdays_bitmask\":\t127,\n"
               "\t\"auto_update_interval_from\":\t0,\n"
               "\t\"auto_update_interval_to\":\t24,\n"
               "\t\"auto_update_tz_offset_hours\":\t3,\n"
               "\t\"company_id\":\t1177,\n"
               "\t\"company_use_filtering\":\ttrue,\n"
               "\t\"scan_coded_phy\":\tfalse,\n"
               "\t\"scan_1mbit_phy\":\ttrue,\n"
               "\t\"scan_extended_payload\":\ttrue,\n"
               "\t\"scan_channel_37\":\ttrue,\n"
               "\t\"scan_channel_38\":\ttrue,\n"
               "\t\"scan_channel_39\":\ttrue,\n"
               "\t\"coordinates\":\t\"\"\n"
               "}"),
        string(json_str.p_str));
    ASSERT_TRUE(esp_log_wrapper_is_empty());

    ruuvi_gateway_config_t gw_cfg2 = {};
    ASSERT_TRUE(gw_cfg_json_parse(json_str.p_str, &gw_cfg2));
    cjson_wrap_free_json_str(&json_str);

    ASSERT_TRUE(0 == memcmp(&gw_cfg, &gw_cfg2, sizeof(gw_cfg)));
}

TEST_F(TestGwCfgJson, gw_cfg_json_generate_http_stat_disabled) // NOLINT
{
    ruuvi_gateway_config_t gw_cfg   = get_gateway_config_default();
    cjson_wrap_str_t       json_str = cjson_wrap_str_null();

    gw_cfg.http_stat.use_http_stat = false;
    ASSERT_TRUE(gw_cfg_json_generate(&gw_cfg, &json_str));
    ASSERT_NE(nullptr, json_str.p_str);
    ASSERT_EQ(
        string("{\n"
               "\t\"use_eth\":\tfalse,\n"
               "\t\"eth_dhcp\":\ttrue,\n"
               "\t\"eth_static_ip\":\t\"\",\n"
               "\t\"eth_netmask\":\t\"\",\n"
               "\t\"eth_gw\":\t\"\",\n"
               "\t\"eth_dns1\":\t\"\",\n"
               "\t\"eth_dns2\":\t\"\",\n"
               "\t\"use_http\":\ttrue,\n"
               "\t\"http_url\":\t\"" RUUVI_GATEWAY_HTTP_DEFAULT_URL "\",\n"
               "\t\"http_user\":\t\"\",\n"
               "\t\"http_pass\":\t\"\",\n"
               "\t\"use_http_stat\":\tfalse,\n"
               "\t\"http_stat_url\":\t\"" RUUVI_GATEWAY_HTTP_STATUS_URL "\",\n"
               "\t\"http_stat_user\":\t\"\",\n"
               "\t\"http_stat_pass\":\t\"\",\n"
               "\t\"use_mqtt\":\tfalse,\n"
               "\t\"mqtt_use_default_prefix\":\ttrue,\n"
               "\t\"mqtt_transport\":\t\"TCP\",\n"
               "\t\"mqtt_server\":\t\"\",\n"
               "\t\"mqtt_port\":\t0,\n"
               "\t\"mqtt_prefix\":\t\"\",\n"
               "\t\"mqtt_client_id\":\t\"\",\n"
               "\t\"mqtt_user\":\t\"\",\n"
               "\t\"mqtt_pass\":\t\"\",\n"
               "\t\"lan_auth_type\":\t\"lan_auth_ruuvi\",\n"
               "\t\"lan_auth_user\":\t\"Admin\",\n"
               "\t\"lan_auth_pass\":\t\"\xFFpassword_md5\xFF\",\n"
               "\t\"lan_auth_api_key\":\t\"\",\n"
               "\t\"auto_update_cycle\":\t\"regular\",\n"
               "\t\"auto_update_weekdays_bitmask\":\t127,\n"
               "\t\"auto_update_interval_from\":\t0,\n"
               "\t\"auto_update_interval_to\":\t24,\n"
               "\t\"auto_update_tz_offset_hours\":\t3,\n"
               "\t\"company_id\":\t1177,\n"
               "\t\"company_use_filtering\":\ttrue,\n"
               "\t\"scan_coded_phy\":\tfalse,\n"
               "\t\"scan_1mbit_phy\":\ttrue,\n"
               "\t\"scan_extended_payload\":\ttrue,\n"
               "\t\"scan_channel_37\":\ttrue,\n"
               "\t\"scan_channel_38\":\ttrue,\n"
               "\t\"scan_channel_39\":\ttrue,\n"
               "\t\"coordinates\":\t\"\"\n"
               "}"),
        string(json_str.p_str));
    ASSERT_TRUE(esp_log_wrapper_is_empty());

    ruuvi_gateway_config_t gw_cfg2 = {};
    ASSERT_TRUE(gw_cfg_json_parse(json_str.p_str, &gw_cfg2));
    cjson_wrap_free_json_str(&json_str);

    ASSERT_TRUE(0 == memcmp(&gw_cfg, &gw_cfg2, sizeof(gw_cfg)));
}

TEST_F(TestGwCfgJson, gw_cfg_json_generate_http_stat_enabled) // NOLINT
{
    ruuvi_gateway_config_t gw_cfg   = get_gateway_config_default();
    cjson_wrap_str_t       json_str = cjson_wrap_str_null();

    gw_cfg.http_stat.use_http_stat = true;
    snprintf(
        gw_cfg.http_stat.http_stat_url.buf,
        sizeof(gw_cfg.http_stat.http_stat_url.buf),
        "https://my_stat_url1.com/status");
    snprintf(gw_cfg.http_stat.http_stat_user.buf, sizeof(gw_cfg.http_stat.http_stat_user.buf), "user1");
    snprintf(gw_cfg.http_stat.http_stat_pass.buf, sizeof(gw_cfg.http_stat.http_stat_pass.buf), "pass1");
    ASSERT_TRUE(gw_cfg_json_generate(&gw_cfg, &json_str));
    ASSERT_NE(nullptr, json_str.p_str);
    ASSERT_EQ(
        string("{\n"
               "\t\"use_eth\":\tfalse,\n"
               "\t\"eth_dhcp\":\ttrue,\n"
               "\t\"eth_static_ip\":\t\"\",\n"
               "\t\"eth_netmask\":\t\"\",\n"
               "\t\"eth_gw\":\t\"\",\n"
               "\t\"eth_dns1\":\t\"\",\n"
               "\t\"eth_dns2\":\t\"\",\n"
               "\t\"use_http\":\ttrue,\n"
               "\t\"http_url\":\t\"" RUUVI_GATEWAY_HTTP_DEFAULT_URL "\",\n"
               "\t\"http_user\":\t\"\",\n"
               "\t\"http_pass\":\t\"\",\n"
               "\t\"use_http_stat\":\ttrue,\n"
               "\t\"http_stat_url\":\t\"https://my_stat_url1.com/status\",\n"
               "\t\"http_stat_user\":\t\"user1\",\n"
               "\t\"http_stat_pass\":\t\"pass1\",\n"
               "\t\"use_mqtt\":\tfalse,\n"
               "\t\"mqtt_use_default_prefix\":\ttrue,\n"
               "\t\"mqtt_transport\":\t\"TCP\",\n"
               "\t\"mqtt_server\":\t\"\",\n"
               "\t\"mqtt_port\":\t0,\n"
               "\t\"mqtt_prefix\":\t\"\",\n"
               "\t\"mqtt_client_id\":\t\"\",\n"
               "\t\"mqtt_user\":\t\"\",\n"
               "\t\"mqtt_pass\":\t\"\",\n"
               "\t\"lan_auth_type\":\t\"lan_auth_ruuvi\",\n"
               "\t\"lan_auth_user\":\t\"Admin\",\n"
               "\t\"lan_auth_pass\":\t\"\xFFpassword_md5\xFF\",\n"
               "\t\"lan_auth_api_key\":\t\"\",\n"
               "\t\"auto_update_cycle\":\t\"regular\",\n"
               "\t\"auto_update_weekdays_bitmask\":\t127,\n"
               "\t\"auto_update_interval_from\":\t0,\n"
               "\t\"auto_update_interval_to\":\t24,\n"
               "\t\"auto_update_tz_offset_hours\":\t3,\n"
               "\t\"company_id\":\t1177,\n"
               "\t\"company_use_filtering\":\ttrue,\n"
               "\t\"scan_coded_phy\":\tfalse,\n"
               "\t\"scan_1mbit_phy\":\ttrue,\n"
               "\t\"scan_extended_payload\":\ttrue,\n"
               "\t\"scan_channel_37\":\ttrue,\n"
               "\t\"scan_channel_38\":\ttrue,\n"
               "\t\"scan_channel_39\":\ttrue,\n"
               "\t\"coordinates\":\t\"\"\n"
               "}"),
        string(json_str.p_str));
    ASSERT_TRUE(esp_log_wrapper_is_empty());

    ruuvi_gateway_config_t gw_cfg2 = {};
    ASSERT_TRUE(gw_cfg_json_parse(json_str.p_str, &gw_cfg2));
    cjson_wrap_free_json_str(&json_str);

    ASSERT_TRUE(0 == memcmp(&gw_cfg, &gw_cfg2, sizeof(gw_cfg)));
}

TEST_F(TestGwCfgJson, gw_cfg_json_generate_lan_auth_ruuvi) // NOLINT
{
    ruuvi_gateway_config_t gw_cfg   = get_gateway_config_default();
    cjson_wrap_str_t       json_str = cjson_wrap_str_null();

    snprintf(gw_cfg.lan_auth.lan_auth_type, sizeof(gw_cfg.lan_auth.lan_auth_type), HTTP_SERVER_AUTH_TYPE_STR_RUUVI);
    snprintf(gw_cfg.lan_auth.lan_auth_user, sizeof(gw_cfg.lan_auth.lan_auth_user), "user1");
    snprintf(gw_cfg.lan_auth.lan_auth_pass, sizeof(gw_cfg.lan_auth.lan_auth_pass), "pass1");
    gw_cfg.lan_auth.lan_auth_api_key[0] = '\0';

    ASSERT_TRUE(gw_cfg_json_generate(&gw_cfg, &json_str));
    ASSERT_NE(nullptr, json_str.p_str);
    ASSERT_EQ(
        string("{\n"
               "\t\"use_eth\":\tfalse,\n"
               "\t\"eth_dhcp\":\ttrue,\n"
               "\t\"eth_static_ip\":\t\"\",\n"
               "\t\"eth_netmask\":\t\"\",\n"
               "\t\"eth_gw\":\t\"\",\n"
               "\t\"eth_dns1\":\t\"\",\n"
               "\t\"eth_dns2\":\t\"\",\n"
               "\t\"use_http\":\ttrue,\n"
               "\t\"http_url\":\t\"" RUUVI_GATEWAY_HTTP_DEFAULT_URL "\",\n"
               "\t\"http_user\":\t\"\",\n"
               "\t\"http_pass\":\t\"\",\n"
               "\t\"use_http_stat\":\ttrue,\n"
               "\t\"http_stat_url\":\t\"" RUUVI_GATEWAY_HTTP_STATUS_URL "\",\n"
               "\t\"http_stat_user\":\t\"\",\n"
               "\t\"http_stat_pass\":\t\"\",\n"
               "\t\"use_mqtt\":\tfalse,\n"
               "\t\"mqtt_use_default_prefix\":\ttrue,\n"
               "\t\"mqtt_transport\":\t\"TCP\",\n"
               "\t\"mqtt_server\":\t\"\",\n"
               "\t\"mqtt_port\":\t0,\n"
               "\t\"mqtt_prefix\":\t\"\",\n"
               "\t\"mqtt_client_id\":\t\"\",\n"
               "\t\"mqtt_user\":\t\"\",\n"
               "\t\"mqtt_pass\":\t\"\",\n"
               "\t\"lan_auth_type\":\t\"lan_auth_ruuvi\",\n"
               "\t\"lan_auth_user\":\t\"user1\",\n"
               "\t\"lan_auth_pass\":\t\"pass1\",\n"
               "\t\"lan_auth_api_key\":\t\"\",\n"
               "\t\"auto_update_cycle\":\t\"regular\",\n"
               "\t\"auto_update_weekdays_bitmask\":\t127,\n"
               "\t\"auto_update_interval_from\":\t0,\n"
               "\t\"auto_update_interval_to\":\t24,\n"
               "\t\"auto_update_tz_offset_hours\":\t3,\n"
               "\t\"company_id\":\t1177,\n"
               "\t\"company_use_filtering\":\ttrue,\n"
               "\t\"scan_coded_phy\":\tfalse,\n"
               "\t\"scan_1mbit_phy\":\ttrue,\n"
               "\t\"scan_extended_payload\":\ttrue,\n"
               "\t\"scan_channel_37\":\ttrue,\n"
               "\t\"scan_channel_38\":\ttrue,\n"
               "\t\"scan_channel_39\":\ttrue,\n"
               "\t\"coordinates\":\t\"\"\n"
               "}"),
        string(json_str.p_str));
    ASSERT_TRUE(esp_log_wrapper_is_empty());

    ruuvi_gateway_config_t gw_cfg2 = {};
    ASSERT_TRUE(gw_cfg_json_parse(json_str.p_str, &gw_cfg2));
    cjson_wrap_free_json_str(&json_str);

    ASSERT_TRUE(0 == memcmp(&gw_cfg, &gw_cfg2, sizeof(gw_cfg)));
}

TEST_F(TestGwCfgJson, gw_cfg_json_generate_lan_auth_ruuvi_with_api_key) // NOLINT
{
    ruuvi_gateway_config_t gw_cfg   = get_gateway_config_default();
    cjson_wrap_str_t       json_str = cjson_wrap_str_null();

    snprintf(gw_cfg.lan_auth.lan_auth_type, sizeof(gw_cfg.lan_auth.lan_auth_type), HTTP_SERVER_AUTH_TYPE_STR_RUUVI);
    snprintf(gw_cfg.lan_auth.lan_auth_user, sizeof(gw_cfg.lan_auth.lan_auth_user), "user1");
    snprintf(gw_cfg.lan_auth.lan_auth_pass, sizeof(gw_cfg.lan_auth.lan_auth_pass), "pass1");
    snprintf(
        gw_cfg.lan_auth.lan_auth_api_key,
        sizeof(gw_cfg.lan_auth.lan_auth_api_key),
        "wH3F9SIiAA3rhG32aJki2Z7ekdFc0vtxuDhxl39zFvw=");

    ASSERT_TRUE(gw_cfg_json_generate(&gw_cfg, &json_str));
    ASSERT_NE(nullptr, json_str.p_str);
    ASSERT_EQ(
        string("{\n"
               "\t\"use_eth\":\tfalse,\n"
               "\t\"eth_dhcp\":\ttrue,\n"
               "\t\"eth_static_ip\":\t\"\",\n"
               "\t\"eth_netmask\":\t\"\",\n"
               "\t\"eth_gw\":\t\"\",\n"
               "\t\"eth_dns1\":\t\"\",\n"
               "\t\"eth_dns2\":\t\"\",\n"
               "\t\"use_http\":\ttrue,\n"
               "\t\"http_url\":\t\"" RUUVI_GATEWAY_HTTP_DEFAULT_URL "\",\n"
               "\t\"http_user\":\t\"\",\n"
               "\t\"http_pass\":\t\"\",\n"
               "\t\"use_http_stat\":\ttrue,\n"
               "\t\"http_stat_url\":\t\"" RUUVI_GATEWAY_HTTP_STATUS_URL "\",\n"
               "\t\"http_stat_user\":\t\"\",\n"
               "\t\"http_stat_pass\":\t\"\",\n"
               "\t\"use_mqtt\":\tfalse,\n"
               "\t\"mqtt_use_default_prefix\":\ttrue,\n"
               "\t\"mqtt_transport\":\t\"TCP\",\n"
               "\t\"mqtt_server\":\t\"\",\n"
               "\t\"mqtt_port\":\t0,\n"
               "\t\"mqtt_prefix\":\t\"\",\n"
               "\t\"mqtt_client_id\":\t\"\",\n"
               "\t\"mqtt_user\":\t\"\",\n"
               "\t\"mqtt_pass\":\t\"\",\n"
               "\t\"lan_auth_type\":\t\"lan_auth_ruuvi\",\n"
               "\t\"lan_auth_user\":\t\"user1\",\n"
               "\t\"lan_auth_pass\":\t\"pass1\",\n"
               "\t\"lan_auth_api_key\":\t\"wH3F9SIiAA3rhG32aJki2Z7ekdFc0vtxuDhxl39zFvw=\",\n"
               "\t\"auto_update_cycle\":\t\"regular\",\n"
               "\t\"auto_update_weekdays_bitmask\":\t127,\n"
               "\t\"auto_update_interval_from\":\t0,\n"
               "\t\"auto_update_interval_to\":\t24,\n"
               "\t\"auto_update_tz_offset_hours\":\t3,\n"
               "\t\"company_id\":\t1177,\n"
               "\t\"company_use_filtering\":\ttrue,\n"
               "\t\"scan_coded_phy\":\tfalse,\n"
               "\t\"scan_1mbit_phy\":\ttrue,\n"
               "\t\"scan_extended_payload\":\ttrue,\n"
               "\t\"scan_channel_37\":\ttrue,\n"
               "\t\"scan_channel_38\":\ttrue,\n"
               "\t\"scan_channel_39\":\ttrue,\n"
               "\t\"coordinates\":\t\"\"\n"
               "}"),
        string(json_str.p_str));
    ASSERT_TRUE(esp_log_wrapper_is_empty());

    ruuvi_gateway_config_t gw_cfg2 = {};
    ASSERT_TRUE(gw_cfg_json_parse(json_str.p_str, &gw_cfg2));
    cjson_wrap_free_json_str(&json_str);

    ASSERT_TRUE(0 == memcmp(&gw_cfg, &gw_cfg2, sizeof(gw_cfg)));
}

TEST_F(TestGwCfgJson, gw_cfg_json_generate_lan_auth_digest) // NOLINT
{
    ruuvi_gateway_config_t gw_cfg   = get_gateway_config_default();
    cjson_wrap_str_t       json_str = cjson_wrap_str_null();

    snprintf(gw_cfg.lan_auth.lan_auth_type, sizeof(gw_cfg.lan_auth.lan_auth_type), HTTP_SERVER_AUTH_TYPE_STR_DIGEST);
    snprintf(gw_cfg.lan_auth.lan_auth_user, sizeof(gw_cfg.lan_auth.lan_auth_user), "user1");
    snprintf(gw_cfg.lan_auth.lan_auth_pass, sizeof(gw_cfg.lan_auth.lan_auth_pass), "pass1");
    gw_cfg.lan_auth.lan_auth_api_key[0] = '\0';

    ASSERT_TRUE(gw_cfg_json_generate(&gw_cfg, &json_str));
    ASSERT_NE(nullptr, json_str.p_str);
    ASSERT_EQ(
        string("{\n"
               "\t\"use_eth\":\tfalse,\n"
               "\t\"eth_dhcp\":\ttrue,\n"
               "\t\"eth_static_ip\":\t\"\",\n"
               "\t\"eth_netmask\":\t\"\",\n"
               "\t\"eth_gw\":\t\"\",\n"
               "\t\"eth_dns1\":\t\"\",\n"
               "\t\"eth_dns2\":\t\"\",\n"
               "\t\"use_http\":\ttrue,\n"
               "\t\"http_url\":\t\"" RUUVI_GATEWAY_HTTP_DEFAULT_URL "\",\n"
               "\t\"http_user\":\t\"\",\n"
               "\t\"http_pass\":\t\"\",\n"
               "\t\"use_http_stat\":\ttrue,\n"
               "\t\"http_stat_url\":\t\"" RUUVI_GATEWAY_HTTP_STATUS_URL "\",\n"
               "\t\"http_stat_user\":\t\"\",\n"
               "\t\"http_stat_pass\":\t\"\",\n"
               "\t\"use_mqtt\":\tfalse,\n"
               "\t\"mqtt_use_default_prefix\":\ttrue,\n"
               "\t\"mqtt_transport\":\t\"TCP\",\n"
               "\t\"mqtt_server\":\t\"\",\n"
               "\t\"mqtt_port\":\t0,\n"
               "\t\"mqtt_prefix\":\t\"\",\n"
               "\t\"mqtt_client_id\":\t\"\",\n"
               "\t\"mqtt_user\":\t\"\",\n"
               "\t\"mqtt_pass\":\t\"\",\n"
               "\t\"lan_auth_type\":\t\"lan_auth_digest\",\n"
               "\t\"lan_auth_user\":\t\"user1\",\n"
               "\t\"lan_auth_pass\":\t\"pass1\",\n"
               "\t\"lan_auth_api_key\":\t\"\",\n"
               "\t\"auto_update_cycle\":\t\"regular\",\n"
               "\t\"auto_update_weekdays_bitmask\":\t127,\n"
               "\t\"auto_update_interval_from\":\t0,\n"
               "\t\"auto_update_interval_to\":\t24,\n"
               "\t\"auto_update_tz_offset_hours\":\t3,\n"
               "\t\"company_id\":\t1177,\n"
               "\t\"company_use_filtering\":\ttrue,\n"
               "\t\"scan_coded_phy\":\tfalse,\n"
               "\t\"scan_1mbit_phy\":\ttrue,\n"
               "\t\"scan_extended_payload\":\ttrue,\n"
               "\t\"scan_channel_37\":\ttrue,\n"
               "\t\"scan_channel_38\":\ttrue,\n"
               "\t\"scan_channel_39\":\ttrue,\n"
               "\t\"coordinates\":\t\"\"\n"
               "}"),
        string(json_str.p_str));
    ASSERT_TRUE(esp_log_wrapper_is_empty());

    ruuvi_gateway_config_t gw_cfg2 = {};
    ASSERT_TRUE(gw_cfg_json_parse(json_str.p_str, &gw_cfg2));
    cjson_wrap_free_json_str(&json_str);

    ASSERT_TRUE(0 == memcmp(&gw_cfg, &gw_cfg2, sizeof(gw_cfg)));
}

TEST_F(TestGwCfgJson, gw_cfg_json_generate_lan_auth_basic) // NOLINT
{
    ruuvi_gateway_config_t gw_cfg   = get_gateway_config_default();
    cjson_wrap_str_t       json_str = cjson_wrap_str_null();

    snprintf(gw_cfg.lan_auth.lan_auth_type, sizeof(gw_cfg.lan_auth.lan_auth_type), HTTP_SERVER_AUTH_TYPE_STR_BASIC);
    snprintf(gw_cfg.lan_auth.lan_auth_user, sizeof(gw_cfg.lan_auth.lan_auth_user), "user1");
    snprintf(gw_cfg.lan_auth.lan_auth_pass, sizeof(gw_cfg.lan_auth.lan_auth_pass), "pass1");
    gw_cfg.lan_auth.lan_auth_api_key[0] = '\0';

    ASSERT_TRUE(gw_cfg_json_generate(&gw_cfg, &json_str));
    ASSERT_NE(nullptr, json_str.p_str);
    ASSERT_EQ(
        string("{\n"
               "\t\"use_eth\":\tfalse,\n"
               "\t\"eth_dhcp\":\ttrue,\n"
               "\t\"eth_static_ip\":\t\"\",\n"
               "\t\"eth_netmask\":\t\"\",\n"
               "\t\"eth_gw\":\t\"\",\n"
               "\t\"eth_dns1\":\t\"\",\n"
               "\t\"eth_dns2\":\t\"\",\n"
               "\t\"use_http\":\ttrue,\n"
               "\t\"http_url\":\t\"" RUUVI_GATEWAY_HTTP_DEFAULT_URL "\",\n"
               "\t\"http_user\":\t\"\",\n"
               "\t\"http_pass\":\t\"\",\n"
               "\t\"use_http_stat\":\ttrue,\n"
               "\t\"http_stat_url\":\t\"" RUUVI_GATEWAY_HTTP_STATUS_URL "\",\n"
               "\t\"http_stat_user\":\t\"\",\n"
               "\t\"http_stat_pass\":\t\"\",\n"
               "\t\"use_mqtt\":\tfalse,\n"
               "\t\"mqtt_use_default_prefix\":\ttrue,\n"
               "\t\"mqtt_transport\":\t\"TCP\",\n"
               "\t\"mqtt_server\":\t\"\",\n"
               "\t\"mqtt_port\":\t0,\n"
               "\t\"mqtt_prefix\":\t\"\",\n"
               "\t\"mqtt_client_id\":\t\"\",\n"
               "\t\"mqtt_user\":\t\"\",\n"
               "\t\"mqtt_pass\":\t\"\",\n"
               "\t\"lan_auth_type\":\t\"lan_auth_basic\",\n"
               "\t\"lan_auth_user\":\t\"user1\",\n"
               "\t\"lan_auth_pass\":\t\"pass1\",\n"
               "\t\"lan_auth_api_key\":\t\"\",\n"
               "\t\"auto_update_cycle\":\t\"regular\",\n"
               "\t\"auto_update_weekdays_bitmask\":\t127,\n"
               "\t\"auto_update_interval_from\":\t0,\n"
               "\t\"auto_update_interval_to\":\t24,\n"
               "\t\"auto_update_tz_offset_hours\":\t3,\n"
               "\t\"company_id\":\t1177,\n"
               "\t\"company_use_filtering\":\ttrue,\n"
               "\t\"scan_coded_phy\":\tfalse,\n"
               "\t\"scan_1mbit_phy\":\ttrue,\n"
               "\t\"scan_extended_payload\":\ttrue,\n"
               "\t\"scan_channel_37\":\ttrue,\n"
               "\t\"scan_channel_38\":\ttrue,\n"
               "\t\"scan_channel_39\":\ttrue,\n"
               "\t\"coordinates\":\t\"\"\n"
               "}"),
        string(json_str.p_str));
    ASSERT_TRUE(esp_log_wrapper_is_empty());

    ruuvi_gateway_config_t gw_cfg2 = {};
    ASSERT_TRUE(gw_cfg_json_parse(json_str.p_str, &gw_cfg2));
    cjson_wrap_free_json_str(&json_str);

    ASSERT_TRUE(0 == memcmp(&gw_cfg, &gw_cfg2, sizeof(gw_cfg)));
}

TEST_F(TestGwCfgJson, gw_cfg_json_generate_lan_auth_allow) // NOLINT
{
    ruuvi_gateway_config_t gw_cfg   = get_gateway_config_default();
    cjson_wrap_str_t       json_str = cjson_wrap_str_null();

    snprintf(gw_cfg.lan_auth.lan_auth_type, sizeof(gw_cfg.lan_auth.lan_auth_type), HTTP_SERVER_AUTH_TYPE_STR_ALLOW);
    gw_cfg.lan_auth.lan_auth_user[0]    = '\0';
    gw_cfg.lan_auth.lan_auth_pass[0]    = '\0';
    gw_cfg.lan_auth.lan_auth_api_key[0] = '\0';

    ASSERT_TRUE(gw_cfg_json_generate(&gw_cfg, &json_str));
    ASSERT_NE(nullptr, json_str.p_str);
    ASSERT_EQ(
        string("{\n"
               "\t\"use_eth\":\tfalse,\n"
               "\t\"eth_dhcp\":\ttrue,\n"
               "\t\"eth_static_ip\":\t\"\",\n"
               "\t\"eth_netmask\":\t\"\",\n"
               "\t\"eth_gw\":\t\"\",\n"
               "\t\"eth_dns1\":\t\"\",\n"
               "\t\"eth_dns2\":\t\"\",\n"
               "\t\"use_http\":\ttrue,\n"
               "\t\"http_url\":\t\"" RUUVI_GATEWAY_HTTP_DEFAULT_URL "\",\n"
               "\t\"http_user\":\t\"\",\n"
               "\t\"http_pass\":\t\"\",\n"
               "\t\"use_http_stat\":\ttrue,\n"
               "\t\"http_stat_url\":\t\"" RUUVI_GATEWAY_HTTP_STATUS_URL "\",\n"
               "\t\"http_stat_user\":\t\"\",\n"
               "\t\"http_stat_pass\":\t\"\",\n"
               "\t\"use_mqtt\":\tfalse,\n"
               "\t\"mqtt_use_default_prefix\":\ttrue,\n"
               "\t\"mqtt_transport\":\t\"TCP\",\n"
               "\t\"mqtt_server\":\t\"\",\n"
               "\t\"mqtt_port\":\t0,\n"
               "\t\"mqtt_prefix\":\t\"\",\n"
               "\t\"mqtt_client_id\":\t\"\",\n"
               "\t\"mqtt_user\":\t\"\",\n"
               "\t\"mqtt_pass\":\t\"\",\n"
               "\t\"lan_auth_type\":\t\"lan_auth_allow\",\n"
               "\t\"lan_auth_user\":\t\"\",\n"
               "\t\"lan_auth_pass\":\t\"\",\n"
               "\t\"lan_auth_api_key\":\t\"\",\n"
               "\t\"auto_update_cycle\":\t\"regular\",\n"
               "\t\"auto_update_weekdays_bitmask\":\t127,\n"
               "\t\"auto_update_interval_from\":\t0,\n"
               "\t\"auto_update_interval_to\":\t24,\n"
               "\t\"auto_update_tz_offset_hours\":\t3,\n"
               "\t\"company_id\":\t1177,\n"
               "\t\"company_use_filtering\":\ttrue,\n"
               "\t\"scan_coded_phy\":\tfalse,\n"
               "\t\"scan_1mbit_phy\":\ttrue,\n"
               "\t\"scan_extended_payload\":\ttrue,\n"
               "\t\"scan_channel_37\":\ttrue,\n"
               "\t\"scan_channel_38\":\ttrue,\n"
               "\t\"scan_channel_39\":\ttrue,\n"
               "\t\"coordinates\":\t\"\"\n"
               "}"),
        string(json_str.p_str));
    ASSERT_TRUE(esp_log_wrapper_is_empty());

    ruuvi_gateway_config_t gw_cfg2 = {};
    ASSERT_TRUE(gw_cfg_json_parse(json_str.p_str, &gw_cfg2));
    cjson_wrap_free_json_str(&json_str);

    ASSERT_TRUE(0 == memcmp(&gw_cfg, &gw_cfg2, sizeof(gw_cfg)));
}

TEST_F(TestGwCfgJson, gw_cfg_json_generate_lan_auth_deny) // NOLINT
{
    ruuvi_gateway_config_t gw_cfg   = get_gateway_config_default();
    cjson_wrap_str_t       json_str = cjson_wrap_str_null();

    snprintf(gw_cfg.lan_auth.lan_auth_type, sizeof(gw_cfg.lan_auth.lan_auth_type), HTTP_SERVER_AUTH_TYPE_STR_DENY);
    gw_cfg.lan_auth.lan_auth_user[0]    = '\0';
    gw_cfg.lan_auth.lan_auth_pass[0]    = '\0';
    gw_cfg.lan_auth.lan_auth_api_key[0] = '\0';

    ASSERT_TRUE(gw_cfg_json_generate(&gw_cfg, &json_str));
    ASSERT_NE(nullptr, json_str.p_str);
    ASSERT_EQ(
        string("{\n"
               "\t\"use_eth\":\tfalse,\n"
               "\t\"eth_dhcp\":\ttrue,\n"
               "\t\"eth_static_ip\":\t\"\",\n"
               "\t\"eth_netmask\":\t\"\",\n"
               "\t\"eth_gw\":\t\"\",\n"
               "\t\"eth_dns1\":\t\"\",\n"
               "\t\"eth_dns2\":\t\"\",\n"
               "\t\"use_http\":\ttrue,\n"
               "\t\"http_url\":\t\"" RUUVI_GATEWAY_HTTP_DEFAULT_URL "\",\n"
               "\t\"http_user\":\t\"\",\n"
               "\t\"http_pass\":\t\"\",\n"
               "\t\"use_http_stat\":\ttrue,\n"
               "\t\"http_stat_url\":\t\"" RUUVI_GATEWAY_HTTP_STATUS_URL "\",\n"
               "\t\"http_stat_user\":\t\"\",\n"
               "\t\"http_stat_pass\":\t\"\",\n"
               "\t\"use_mqtt\":\tfalse,\n"
               "\t\"mqtt_use_default_prefix\":\ttrue,\n"
               "\t\"mqtt_transport\":\t\"TCP\",\n"
               "\t\"mqtt_server\":\t\"\",\n"
               "\t\"mqtt_port\":\t0,\n"
               "\t\"mqtt_prefix\":\t\"\",\n"
               "\t\"mqtt_client_id\":\t\"\",\n"
               "\t\"mqtt_user\":\t\"\",\n"
               "\t\"mqtt_pass\":\t\"\",\n"
               "\t\"lan_auth_type\":\t\"lan_auth_deny\",\n"
               "\t\"lan_auth_user\":\t\"\",\n"
               "\t\"lan_auth_pass\":\t\"\",\n"
               "\t\"lan_auth_api_key\":\t\"\",\n"
               "\t\"auto_update_cycle\":\t\"regular\",\n"
               "\t\"auto_update_weekdays_bitmask\":\t127,\n"
               "\t\"auto_update_interval_from\":\t0,\n"
               "\t\"auto_update_interval_to\":\t24,\n"
               "\t\"auto_update_tz_offset_hours\":\t3,\n"
               "\t\"company_id\":\t1177,\n"
               "\t\"company_use_filtering\":\ttrue,\n"
               "\t\"scan_coded_phy\":\tfalse,\n"
               "\t\"scan_1mbit_phy\":\ttrue,\n"
               "\t\"scan_extended_payload\":\ttrue,\n"
               "\t\"scan_channel_37\":\ttrue,\n"
               "\t\"scan_channel_38\":\ttrue,\n"
               "\t\"scan_channel_39\":\ttrue,\n"
               "\t\"coordinates\":\t\"\"\n"
               "}"),
        string(json_str.p_str));
    ASSERT_TRUE(esp_log_wrapper_is_empty());

    ruuvi_gateway_config_t gw_cfg2 = {};
    ASSERT_TRUE(gw_cfg_json_parse(json_str.p_str, &gw_cfg2));
    cjson_wrap_free_json_str(&json_str);

    ASSERT_TRUE(0 == memcmp(&gw_cfg, &gw_cfg2, sizeof(gw_cfg)));
}

TEST_F(TestGwCfgJson, gw_cfg_json_generate_auto_update_beta_tester) // NOLINT
{
    ruuvi_gateway_config_t gw_cfg   = get_gateway_config_default();
    cjson_wrap_str_t       json_str = cjson_wrap_str_null();

    gw_cfg.auto_update.auto_update_cycle            = AUTO_UPDATE_CYCLE_TYPE_BETA_TESTER;
    gw_cfg.auto_update.auto_update_weekdays_bitmask = 126;
    gw_cfg.auto_update.auto_update_interval_from    = 1;
    gw_cfg.auto_update.auto_update_interval_to      = 23;
    gw_cfg.auto_update.auto_update_tz_offset_hours  = 4;
    ASSERT_TRUE(gw_cfg_json_generate(&gw_cfg, &json_str));
    ASSERT_NE(nullptr, json_str.p_str);
    ASSERT_EQ(
        string("{\n"
               "\t\"use_eth\":\tfalse,\n"
               "\t\"eth_dhcp\":\ttrue,\n"
               "\t\"eth_static_ip\":\t\"\",\n"
               "\t\"eth_netmask\":\t\"\",\n"
               "\t\"eth_gw\":\t\"\",\n"
               "\t\"eth_dns1\":\t\"\",\n"
               "\t\"eth_dns2\":\t\"\",\n"
               "\t\"use_http\":\ttrue,\n"
               "\t\"http_url\":\t\"" RUUVI_GATEWAY_HTTP_DEFAULT_URL "\",\n"
               "\t\"http_user\":\t\"\",\n"
               "\t\"http_pass\":\t\"\",\n"
               "\t\"use_http_stat\":\ttrue,\n"
               "\t\"http_stat_url\":\t\"" RUUVI_GATEWAY_HTTP_STATUS_URL "\",\n"
               "\t\"http_stat_user\":\t\"\",\n"
               "\t\"http_stat_pass\":\t\"\",\n"
               "\t\"use_mqtt\":\tfalse,\n"
               "\t\"mqtt_use_default_prefix\":\ttrue,\n"
               "\t\"mqtt_transport\":\t\"TCP\",\n"
               "\t\"mqtt_server\":\t\"\",\n"
               "\t\"mqtt_port\":\t0,\n"
               "\t\"mqtt_prefix\":\t\"\",\n"
               "\t\"mqtt_client_id\":\t\"\",\n"
               "\t\"mqtt_user\":\t\"\",\n"
               "\t\"mqtt_pass\":\t\"\",\n"
               "\t\"lan_auth_type\":\t\"lan_auth_ruuvi\",\n"
               "\t\"lan_auth_user\":\t\"Admin\",\n"
               "\t\"lan_auth_pass\":\t\"\xFFpassword_md5\xFF\",\n"
               "\t\"lan_auth_api_key\":\t\"\",\n"
               "\t\"auto_update_cycle\":\t\"beta\",\n"
               "\t\"auto_update_weekdays_bitmask\":\t126,\n"
               "\t\"auto_update_interval_from\":\t1,\n"
               "\t\"auto_update_interval_to\":\t23,\n"
               "\t\"auto_update_tz_offset_hours\":\t4,\n"
               "\t\"company_id\":\t1177,\n"
               "\t\"company_use_filtering\":\ttrue,\n"
               "\t\"scan_coded_phy\":\tfalse,\n"
               "\t\"scan_1mbit_phy\":\ttrue,\n"
               "\t\"scan_extended_payload\":\ttrue,\n"
               "\t\"scan_channel_37\":\ttrue,\n"
               "\t\"scan_channel_38\":\ttrue,\n"
               "\t\"scan_channel_39\":\ttrue,\n"
               "\t\"coordinates\":\t\"\"\n"
               "}"),
        string(json_str.p_str));
    ASSERT_TRUE(esp_log_wrapper_is_empty());

    ruuvi_gateway_config_t gw_cfg2 = {};
    ASSERT_TRUE(gw_cfg_json_parse(json_str.p_str, &gw_cfg2));
    cjson_wrap_free_json_str(&json_str);

    ASSERT_TRUE(0 == memcmp(&gw_cfg, &gw_cfg2, sizeof(gw_cfg)));
}

TEST_F(TestGwCfgJson, gw_cfg_json_generate_auto_update_manual) // NOLINT
{
    ruuvi_gateway_config_t gw_cfg   = get_gateway_config_default();
    cjson_wrap_str_t       json_str = cjson_wrap_str_null();

    gw_cfg.auto_update.auto_update_cycle            = AUTO_UPDATE_CYCLE_TYPE_MANUAL;
    gw_cfg.auto_update.auto_update_weekdays_bitmask = 125;
    gw_cfg.auto_update.auto_update_interval_from    = 2;
    gw_cfg.auto_update.auto_update_interval_to      = 22;
    gw_cfg.auto_update.auto_update_tz_offset_hours  = -4;
    ASSERT_TRUE(gw_cfg_json_generate(&gw_cfg, &json_str));
    ASSERT_NE(nullptr, json_str.p_str);
    ASSERT_EQ(
        string("{\n"
               "\t\"use_eth\":\tfalse,\n"
               "\t\"eth_dhcp\":\ttrue,\n"
               "\t\"eth_static_ip\":\t\"\",\n"
               "\t\"eth_netmask\":\t\"\",\n"
               "\t\"eth_gw\":\t\"\",\n"
               "\t\"eth_dns1\":\t\"\",\n"
               "\t\"eth_dns2\":\t\"\",\n"
               "\t\"use_http\":\ttrue,\n"
               "\t\"http_url\":\t\"" RUUVI_GATEWAY_HTTP_DEFAULT_URL "\",\n"
               "\t\"http_user\":\t\"\",\n"
               "\t\"http_pass\":\t\"\",\n"
               "\t\"use_http_stat\":\ttrue,\n"
               "\t\"http_stat_url\":\t\"" RUUVI_GATEWAY_HTTP_STATUS_URL "\",\n"
               "\t\"http_stat_user\":\t\"\",\n"
               "\t\"http_stat_pass\":\t\"\",\n"
               "\t\"use_mqtt\":\tfalse,\n"
               "\t\"mqtt_use_default_prefix\":\ttrue,\n"
               "\t\"mqtt_transport\":\t\"TCP\",\n"
               "\t\"mqtt_server\":\t\"\",\n"
               "\t\"mqtt_port\":\t0,\n"
               "\t\"mqtt_prefix\":\t\"\",\n"
               "\t\"mqtt_client_id\":\t\"\",\n"
               "\t\"mqtt_user\":\t\"\",\n"
               "\t\"mqtt_pass\":\t\"\",\n"
               "\t\"lan_auth_type\":\t\"lan_auth_ruuvi\",\n"
               "\t\"lan_auth_user\":\t\"Admin\",\n"
               "\t\"lan_auth_pass\":\t\"\xFFpassword_md5\xFF\",\n"
               "\t\"lan_auth_api_key\":\t\"\",\n"
               "\t\"auto_update_cycle\":\t\"manual\",\n"
               "\t\"auto_update_weekdays_bitmask\":\t125,\n"
               "\t\"auto_update_interval_from\":\t2,\n"
               "\t\"auto_update_interval_to\":\t22,\n"
               "\t\"auto_update_tz_offset_hours\":\t-4,\n"
               "\t\"company_id\":\t1177,\n"
               "\t\"company_use_filtering\":\ttrue,\n"
               "\t\"scan_coded_phy\":\tfalse,\n"
               "\t\"scan_1mbit_phy\":\ttrue,\n"
               "\t\"scan_extended_payload\":\ttrue,\n"
               "\t\"scan_channel_37\":\ttrue,\n"
               "\t\"scan_channel_38\":\ttrue,\n"
               "\t\"scan_channel_39\":\ttrue,\n"
               "\t\"coordinates\":\t\"\"\n"
               "}"),
        string(json_str.p_str));
    ASSERT_TRUE(esp_log_wrapper_is_empty());

    ruuvi_gateway_config_t gw_cfg2 = {};
    ASSERT_TRUE(gw_cfg_json_parse(json_str.p_str, &gw_cfg2));
    cjson_wrap_free_json_str(&json_str);

    ASSERT_TRUE(0 == memcmp(&gw_cfg, &gw_cfg2, sizeof(gw_cfg)));
}

TEST_F(TestGwCfgJson, gw_cfg_json_generate_auto_update_unknown) // NOLINT
{
    ruuvi_gateway_config_t gw_cfg   = get_gateway_config_default();
    cjson_wrap_str_t       json_str = cjson_wrap_str_null();

    gw_cfg.auto_update.auto_update_cycle = (auto_update_cycle_type_e)-1;
    ASSERT_TRUE(gw_cfg_json_generate(&gw_cfg, &json_str));
    ASSERT_NE(nullptr, json_str.p_str);
    ASSERT_EQ(
        string("{\n"
               "\t\"use_eth\":\tfalse,\n"
               "\t\"eth_dhcp\":\ttrue,\n"
               "\t\"eth_static_ip\":\t\"\",\n"
               "\t\"eth_netmask\":\t\"\",\n"
               "\t\"eth_gw\":\t\"\",\n"
               "\t\"eth_dns1\":\t\"\",\n"
               "\t\"eth_dns2\":\t\"\",\n"
               "\t\"use_http\":\ttrue,\n"
               "\t\"http_url\":\t\"" RUUVI_GATEWAY_HTTP_DEFAULT_URL "\",\n"
               "\t\"http_user\":\t\"\",\n"
               "\t\"http_pass\":\t\"\",\n"
               "\t\"use_http_stat\":\ttrue,\n"
               "\t\"http_stat_url\":\t\"" RUUVI_GATEWAY_HTTP_STATUS_URL "\",\n"
               "\t\"http_stat_user\":\t\"\",\n"
               "\t\"http_stat_pass\":\t\"\",\n"
               "\t\"use_mqtt\":\tfalse,\n"
               "\t\"mqtt_use_default_prefix\":\ttrue,\n"
               "\t\"mqtt_transport\":\t\"TCP\",\n"
               "\t\"mqtt_server\":\t\"\",\n"
               "\t\"mqtt_port\":\t0,\n"
               "\t\"mqtt_prefix\":\t\"\",\n"
               "\t\"mqtt_client_id\":\t\"\",\n"
               "\t\"mqtt_user\":\t\"\",\n"
               "\t\"mqtt_pass\":\t\"\",\n"
               "\t\"lan_auth_type\":\t\"lan_auth_ruuvi\",\n"
               "\t\"lan_auth_user\":\t\"Admin\",\n"
               "\t\"lan_auth_pass\":\t\"\xFFpassword_md5\xFF\",\n"
               "\t\"lan_auth_api_key\":\t\"\",\n"
               "\t\"auto_update_cycle\":\t\"regular\",\n"
               "\t\"auto_update_weekdays_bitmask\":\t127,\n"
               "\t\"auto_update_interval_from\":\t0,\n"
               "\t\"auto_update_interval_to\":\t24,\n"
               "\t\"auto_update_tz_offset_hours\":\t3,\n"
               "\t\"company_id\":\t1177,\n"
               "\t\"company_use_filtering\":\ttrue,\n"
               "\t\"scan_coded_phy\":\tfalse,\n"
               "\t\"scan_1mbit_phy\":\ttrue,\n"
               "\t\"scan_extended_payload\":\ttrue,\n"
               "\t\"scan_channel_37\":\ttrue,\n"
               "\t\"scan_channel_38\":\ttrue,\n"
               "\t\"scan_channel_39\":\ttrue,\n"
               "\t\"coordinates\":\t\"\"\n"
               "}"),
        string(json_str.p_str));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    cjson_wrap_free_json_str(&json_str);
}

TEST_F(TestGwCfgJson, gw_cfg_json_generate_filter_enabled) // NOLINT
{
    ruuvi_gateway_config_t gw_cfg   = get_gateway_config_default();
    cjson_wrap_str_t       json_str = cjson_wrap_str_null();

    gw_cfg.filter.company_id            = 1234;
    gw_cfg.filter.company_use_filtering = true;

    ASSERT_TRUE(gw_cfg_json_generate(&gw_cfg, &json_str));
    ASSERT_NE(nullptr, json_str.p_str);
    ASSERT_EQ(
        string("{\n"
               "\t\"use_eth\":\tfalse,\n"
               "\t\"eth_dhcp\":\ttrue,\n"
               "\t\"eth_static_ip\":\t\"\",\n"
               "\t\"eth_netmask\":\t\"\",\n"
               "\t\"eth_gw\":\t\"\",\n"
               "\t\"eth_dns1\":\t\"\",\n"
               "\t\"eth_dns2\":\t\"\",\n"
               "\t\"use_http\":\ttrue,\n"
               "\t\"http_url\":\t\"" RUUVI_GATEWAY_HTTP_DEFAULT_URL "\",\n"
               "\t\"http_user\":\t\"\",\n"
               "\t\"http_pass\":\t\"\",\n"
               "\t\"use_http_stat\":\ttrue,\n"
               "\t\"http_stat_url\":\t\"" RUUVI_GATEWAY_HTTP_STATUS_URL "\",\n"
               "\t\"http_stat_user\":\t\"\",\n"
               "\t\"http_stat_pass\":\t\"\",\n"
               "\t\"use_mqtt\":\tfalse,\n"
               "\t\"mqtt_use_default_prefix\":\ttrue,\n"
               "\t\"mqtt_transport\":\t\"TCP\",\n"
               "\t\"mqtt_server\":\t\"\",\n"
               "\t\"mqtt_port\":\t0,\n"
               "\t\"mqtt_prefix\":\t\"\",\n"
               "\t\"mqtt_client_id\":\t\"\",\n"
               "\t\"mqtt_user\":\t\"\",\n"
               "\t\"mqtt_pass\":\t\"\",\n"
               "\t\"lan_auth_type\":\t\"lan_auth_ruuvi\",\n"
               "\t\"lan_auth_user\":\t\"Admin\",\n"
               "\t\"lan_auth_pass\":\t\"\xFFpassword_md5\xFF\",\n"
               "\t\"lan_auth_api_key\":\t\"\",\n"
               "\t\"auto_update_cycle\":\t\"regular\",\n"
               "\t\"auto_update_weekdays_bitmask\":\t127,\n"
               "\t\"auto_update_interval_from\":\t0,\n"
               "\t\"auto_update_interval_to\":\t24,\n"
               "\t\"auto_update_tz_offset_hours\":\t3,\n"
               "\t\"company_id\":\t1234,\n"
               "\t\"company_use_filtering\":\ttrue,\n"
               "\t\"scan_coded_phy\":\tfalse,\n"
               "\t\"scan_1mbit_phy\":\ttrue,\n"
               "\t\"scan_extended_payload\":\ttrue,\n"
               "\t\"scan_channel_37\":\ttrue,\n"
               "\t\"scan_channel_38\":\ttrue,\n"
               "\t\"scan_channel_39\":\ttrue,\n"
               "\t\"coordinates\":\t\"\"\n"
               "}"),
        string(json_str.p_str));
    ASSERT_TRUE(esp_log_wrapper_is_empty());

    ruuvi_gateway_config_t gw_cfg2 = {};
    ASSERT_TRUE(gw_cfg_json_parse(json_str.p_str, &gw_cfg2));
    cjson_wrap_free_json_str(&json_str);

    ASSERT_TRUE(0 == memcmp(&gw_cfg, &gw_cfg2, sizeof(gw_cfg)));
}

TEST_F(TestGwCfgJson, gw_cfg_json_generate_filter_disabled) // NOLINT
{
    ruuvi_gateway_config_t gw_cfg   = get_gateway_config_default();
    cjson_wrap_str_t       json_str = cjson_wrap_str_null();

    gw_cfg.filter.company_id            = 1235;
    gw_cfg.filter.company_use_filtering = false;

    ASSERT_TRUE(gw_cfg_json_generate(&gw_cfg, &json_str));
    ASSERT_NE(nullptr, json_str.p_str);
    ASSERT_EQ(
        string("{\n"
               "\t\"use_eth\":\tfalse,\n"
               "\t\"eth_dhcp\":\ttrue,\n"
               "\t\"eth_static_ip\":\t\"\",\n"
               "\t\"eth_netmask\":\t\"\",\n"
               "\t\"eth_gw\":\t\"\",\n"
               "\t\"eth_dns1\":\t\"\",\n"
               "\t\"eth_dns2\":\t\"\",\n"
               "\t\"use_http\":\ttrue,\n"
               "\t\"http_url\":\t\"" RUUVI_GATEWAY_HTTP_DEFAULT_URL "\",\n"
               "\t\"http_user\":\t\"\",\n"
               "\t\"http_pass\":\t\"\",\n"
               "\t\"use_http_stat\":\ttrue,\n"
               "\t\"http_stat_url\":\t\"" RUUVI_GATEWAY_HTTP_STATUS_URL "\",\n"
               "\t\"http_stat_user\":\t\"\",\n"
               "\t\"http_stat_pass\":\t\"\",\n"
               "\t\"use_mqtt\":\tfalse,\n"
               "\t\"mqtt_use_default_prefix\":\ttrue,\n"
               "\t\"mqtt_transport\":\t\"TCP\",\n"
               "\t\"mqtt_server\":\t\"\",\n"
               "\t\"mqtt_port\":\t0,\n"
               "\t\"mqtt_prefix\":\t\"\",\n"
               "\t\"mqtt_client_id\":\t\"\",\n"
               "\t\"mqtt_user\":\t\"\",\n"
               "\t\"mqtt_pass\":\t\"\",\n"
               "\t\"lan_auth_type\":\t\"lan_auth_ruuvi\",\n"
               "\t\"lan_auth_user\":\t\"Admin\",\n"
               "\t\"lan_auth_pass\":\t\"\xFFpassword_md5\xFF\",\n"
               "\t\"lan_auth_api_key\":\t\"\",\n"
               "\t\"auto_update_cycle\":\t\"regular\",\n"
               "\t\"auto_update_weekdays_bitmask\":\t127,\n"
               "\t\"auto_update_interval_from\":\t0,\n"
               "\t\"auto_update_interval_to\":\t24,\n"
               "\t\"auto_update_tz_offset_hours\":\t3,\n"
               "\t\"company_id\":\t1235,\n"
               "\t\"company_use_filtering\":\tfalse,\n"
               "\t\"scan_coded_phy\":\tfalse,\n"
               "\t\"scan_1mbit_phy\":\ttrue,\n"
               "\t\"scan_extended_payload\":\ttrue,\n"
               "\t\"scan_channel_37\":\ttrue,\n"
               "\t\"scan_channel_38\":\ttrue,\n"
               "\t\"scan_channel_39\":\ttrue,\n"
               "\t\"coordinates\":\t\"\"\n"
               "}"),
        string(json_str.p_str));
    ASSERT_TRUE(esp_log_wrapper_is_empty());

    ruuvi_gateway_config_t gw_cfg2 = {};
    ASSERT_TRUE(gw_cfg_json_parse(json_str.p_str, &gw_cfg2));
    cjson_wrap_free_json_str(&json_str);

    ASSERT_TRUE(0 == memcmp(&gw_cfg, &gw_cfg2, sizeof(gw_cfg)));
}

TEST_F(TestGwCfgJson, gw_cfg_json_generate_scan_default) // NOLINT
{
    ruuvi_gateway_config_t gw_cfg   = get_gateway_config_default();
    cjson_wrap_str_t       json_str = cjson_wrap_str_null();

    gw_cfg.scan.scan_coded_phy        = false;
    gw_cfg.scan.scan_1mbit_phy        = true;
    gw_cfg.scan.scan_extended_payload = true;
    gw_cfg.scan.scan_channel_37       = true;
    gw_cfg.scan.scan_channel_38       = true;
    gw_cfg.scan.scan_channel_39       = true;

    ASSERT_TRUE(gw_cfg_json_generate(&gw_cfg, &json_str));
    ASSERT_NE(nullptr, json_str.p_str);
    ASSERT_EQ(
        string("{\n"
               "\t\"use_eth\":\tfalse,\n"
               "\t\"eth_dhcp\":\ttrue,\n"
               "\t\"eth_static_ip\":\t\"\",\n"
               "\t\"eth_netmask\":\t\"\",\n"
               "\t\"eth_gw\":\t\"\",\n"
               "\t\"eth_dns1\":\t\"\",\n"
               "\t\"eth_dns2\":\t\"\",\n"
               "\t\"use_http\":\ttrue,\n"
               "\t\"http_url\":\t\"" RUUVI_GATEWAY_HTTP_DEFAULT_URL "\",\n"
               "\t\"http_user\":\t\"\",\n"
               "\t\"http_pass\":\t\"\",\n"
               "\t\"use_http_stat\":\ttrue,\n"
               "\t\"http_stat_url\":\t\"" RUUVI_GATEWAY_HTTP_STATUS_URL "\",\n"
               "\t\"http_stat_user\":\t\"\",\n"
               "\t\"http_stat_pass\":\t\"\",\n"
               "\t\"use_mqtt\":\tfalse,\n"
               "\t\"mqtt_use_default_prefix\":\ttrue,\n"
               "\t\"mqtt_transport\":\t\"TCP\",\n"
               "\t\"mqtt_server\":\t\"\",\n"
               "\t\"mqtt_port\":\t0,\n"
               "\t\"mqtt_prefix\":\t\"\",\n"
               "\t\"mqtt_client_id\":\t\"\",\n"
               "\t\"mqtt_user\":\t\"\",\n"
               "\t\"mqtt_pass\":\t\"\",\n"
               "\t\"lan_auth_type\":\t\"lan_auth_ruuvi\",\n"
               "\t\"lan_auth_user\":\t\"Admin\",\n"
               "\t\"lan_auth_pass\":\t\"\xFFpassword_md5\xFF\",\n"
               "\t\"lan_auth_api_key\":\t\"\",\n"
               "\t\"auto_update_cycle\":\t\"regular\",\n"
               "\t\"auto_update_weekdays_bitmask\":\t127,\n"
               "\t\"auto_update_interval_from\":\t0,\n"
               "\t\"auto_update_interval_to\":\t24,\n"
               "\t\"auto_update_tz_offset_hours\":\t3,\n"
               "\t\"company_id\":\t1177,\n"
               "\t\"company_use_filtering\":\ttrue,\n"
               "\t\"scan_coded_phy\":\tfalse,\n"
               "\t\"scan_1mbit_phy\":\ttrue,\n"
               "\t\"scan_extended_payload\":\ttrue,\n"
               "\t\"scan_channel_37\":\ttrue,\n"
               "\t\"scan_channel_38\":\ttrue,\n"
               "\t\"scan_channel_39\":\ttrue,\n"
               "\t\"coordinates\":\t\"\"\n"
               "}"),
        string(json_str.p_str));
    ASSERT_TRUE(esp_log_wrapper_is_empty());

    ruuvi_gateway_config_t gw_cfg2 = {};
    ASSERT_TRUE(gw_cfg_json_parse(json_str.p_str, &gw_cfg2));
    cjson_wrap_free_json_str(&json_str);

    ASSERT_TRUE(0 == memcmp(&gw_cfg, &gw_cfg2, sizeof(gw_cfg)));
}

TEST_F(TestGwCfgJson, gw_cfg_json_generate_scan_coded_phy_true) // NOLINT
{
    ruuvi_gateway_config_t gw_cfg   = get_gateway_config_default();
    cjson_wrap_str_t       json_str = cjson_wrap_str_null();

    gw_cfg.scan.scan_coded_phy        = true;
    gw_cfg.scan.scan_1mbit_phy        = true;
    gw_cfg.scan.scan_extended_payload = true;
    gw_cfg.scan.scan_channel_37       = true;
    gw_cfg.scan.scan_channel_38       = true;
    gw_cfg.scan.scan_channel_39       = true;

    ASSERT_TRUE(gw_cfg_json_generate(&gw_cfg, &json_str));
    ASSERT_NE(nullptr, json_str.p_str);
    ASSERT_EQ(
        string("{\n"
               "\t\"use_eth\":\tfalse,\n"
               "\t\"eth_dhcp\":\ttrue,\n"
               "\t\"eth_static_ip\":\t\"\",\n"
               "\t\"eth_netmask\":\t\"\",\n"
               "\t\"eth_gw\":\t\"\",\n"
               "\t\"eth_dns1\":\t\"\",\n"
               "\t\"eth_dns2\":\t\"\",\n"
               "\t\"use_http\":\ttrue,\n"
               "\t\"http_url\":\t\"" RUUVI_GATEWAY_HTTP_DEFAULT_URL "\",\n"
               "\t\"http_user\":\t\"\",\n"
               "\t\"http_pass\":\t\"\",\n"
               "\t\"use_http_stat\":\ttrue,\n"
               "\t\"http_stat_url\":\t\"" RUUVI_GATEWAY_HTTP_STATUS_URL "\",\n"
               "\t\"http_stat_user\":\t\"\",\n"
               "\t\"http_stat_pass\":\t\"\",\n"
               "\t\"use_mqtt\":\tfalse,\n"
               "\t\"mqtt_use_default_prefix\":\ttrue,\n"
               "\t\"mqtt_transport\":\t\"TCP\",\n"
               "\t\"mqtt_server\":\t\"\",\n"
               "\t\"mqtt_port\":\t0,\n"
               "\t\"mqtt_prefix\":\t\"\",\n"
               "\t\"mqtt_client_id\":\t\"\",\n"
               "\t\"mqtt_user\":\t\"\",\n"
               "\t\"mqtt_pass\":\t\"\",\n"
               "\t\"lan_auth_type\":\t\"lan_auth_ruuvi\",\n"
               "\t\"lan_auth_user\":\t\"Admin\",\n"
               "\t\"lan_auth_pass\":\t\"\xFFpassword_md5\xFF\",\n"
               "\t\"lan_auth_api_key\":\t\"\",\n"
               "\t\"auto_update_cycle\":\t\"regular\",\n"
               "\t\"auto_update_weekdays_bitmask\":\t127,\n"
               "\t\"auto_update_interval_from\":\t0,\n"
               "\t\"auto_update_interval_to\":\t24,\n"
               "\t\"auto_update_tz_offset_hours\":\t3,\n"
               "\t\"company_id\":\t1177,\n"
               "\t\"company_use_filtering\":\ttrue,\n"
               "\t\"scan_coded_phy\":\ttrue,\n"
               "\t\"scan_1mbit_phy\":\ttrue,\n"
               "\t\"scan_extended_payload\":\ttrue,\n"
               "\t\"scan_channel_37\":\ttrue,\n"
               "\t\"scan_channel_38\":\ttrue,\n"
               "\t\"scan_channel_39\":\ttrue,\n"
               "\t\"coordinates\":\t\"\"\n"
               "}"),
        string(json_str.p_str));
    ASSERT_TRUE(esp_log_wrapper_is_empty());

    ruuvi_gateway_config_t gw_cfg2 = {};
    ASSERT_TRUE(gw_cfg_json_parse(json_str.p_str, &gw_cfg2));
    cjson_wrap_free_json_str(&json_str);

    ASSERT_TRUE(0 == memcmp(&gw_cfg, &gw_cfg2, sizeof(gw_cfg)));
}

TEST_F(TestGwCfgJson, gw_cfg_json_generate_scan_1mbit_phy_false) // NOLINT
{
    ruuvi_gateway_config_t gw_cfg   = get_gateway_config_default();
    cjson_wrap_str_t       json_str = cjson_wrap_str_null();

    gw_cfg.scan.scan_coded_phy        = false;
    gw_cfg.scan.scan_1mbit_phy        = false;
    gw_cfg.scan.scan_extended_payload = true;
    gw_cfg.scan.scan_channel_37       = true;
    gw_cfg.scan.scan_channel_38       = true;
    gw_cfg.scan.scan_channel_39       = true;

    ASSERT_TRUE(gw_cfg_json_generate(&gw_cfg, &json_str));
    ASSERT_NE(nullptr, json_str.p_str);
    ASSERT_EQ(
        string("{\n"
               "\t\"use_eth\":\tfalse,\n"
               "\t\"eth_dhcp\":\ttrue,\n"
               "\t\"eth_static_ip\":\t\"\",\n"
               "\t\"eth_netmask\":\t\"\",\n"
               "\t\"eth_gw\":\t\"\",\n"
               "\t\"eth_dns1\":\t\"\",\n"
               "\t\"eth_dns2\":\t\"\",\n"
               "\t\"use_http\":\ttrue,\n"
               "\t\"http_url\":\t\"" RUUVI_GATEWAY_HTTP_DEFAULT_URL "\",\n"
               "\t\"http_user\":\t\"\",\n"
               "\t\"http_pass\":\t\"\",\n"
               "\t\"use_http_stat\":\ttrue,\n"
               "\t\"http_stat_url\":\t\"" RUUVI_GATEWAY_HTTP_STATUS_URL "\",\n"
               "\t\"http_stat_user\":\t\"\",\n"
               "\t\"http_stat_pass\":\t\"\",\n"
               "\t\"use_mqtt\":\tfalse,\n"
               "\t\"mqtt_use_default_prefix\":\ttrue,\n"
               "\t\"mqtt_transport\":\t\"TCP\",\n"
               "\t\"mqtt_server\":\t\"\",\n"
               "\t\"mqtt_port\":\t0,\n"
               "\t\"mqtt_prefix\":\t\"\",\n"
               "\t\"mqtt_client_id\":\t\"\",\n"
               "\t\"mqtt_user\":\t\"\",\n"
               "\t\"mqtt_pass\":\t\"\",\n"
               "\t\"lan_auth_type\":\t\"lan_auth_ruuvi\",\n"
               "\t\"lan_auth_user\":\t\"Admin\",\n"
               "\t\"lan_auth_pass\":\t\"\xFFpassword_md5\xFF\",\n"
               "\t\"lan_auth_api_key\":\t\"\",\n"
               "\t\"auto_update_cycle\":\t\"regular\",\n"
               "\t\"auto_update_weekdays_bitmask\":\t127,\n"
               "\t\"auto_update_interval_from\":\t0,\n"
               "\t\"auto_update_interval_to\":\t24,\n"
               "\t\"auto_update_tz_offset_hours\":\t3,\n"
               "\t\"company_id\":\t1177,\n"
               "\t\"company_use_filtering\":\ttrue,\n"
               "\t\"scan_coded_phy\":\tfalse,\n"
               "\t\"scan_1mbit_phy\":\tfalse,\n"
               "\t\"scan_extended_payload\":\ttrue,\n"
               "\t\"scan_channel_37\":\ttrue,\n"
               "\t\"scan_channel_38\":\ttrue,\n"
               "\t\"scan_channel_39\":\ttrue,\n"
               "\t\"coordinates\":\t\"\"\n"
               "}"),
        string(json_str.p_str));
    ASSERT_TRUE(esp_log_wrapper_is_empty());

    ruuvi_gateway_config_t gw_cfg2 = {};
    ASSERT_TRUE(gw_cfg_json_parse(json_str.p_str, &gw_cfg2));
    cjson_wrap_free_json_str(&json_str);

    ASSERT_TRUE(0 == memcmp(&gw_cfg, &gw_cfg2, sizeof(gw_cfg)));
}

TEST_F(TestGwCfgJson, gw_cfg_json_generate_scan_extended_payload_false) // NOLINT
{
    ruuvi_gateway_config_t gw_cfg   = get_gateway_config_default();
    cjson_wrap_str_t       json_str = cjson_wrap_str_null();

    gw_cfg.scan.scan_coded_phy        = false;
    gw_cfg.scan.scan_1mbit_phy        = true;
    gw_cfg.scan.scan_extended_payload = false;
    gw_cfg.scan.scan_channel_37       = true;
    gw_cfg.scan.scan_channel_38       = true;
    gw_cfg.scan.scan_channel_39       = true;

    ASSERT_TRUE(gw_cfg_json_generate(&gw_cfg, &json_str));
    ASSERT_NE(nullptr, json_str.p_str);
    ASSERT_EQ(
        string("{\n"
               "\t\"use_eth\":\tfalse,\n"
               "\t\"eth_dhcp\":\ttrue,\n"
               "\t\"eth_static_ip\":\t\"\",\n"
               "\t\"eth_netmask\":\t\"\",\n"
               "\t\"eth_gw\":\t\"\",\n"
               "\t\"eth_dns1\":\t\"\",\n"
               "\t\"eth_dns2\":\t\"\",\n"
               "\t\"use_http\":\ttrue,\n"
               "\t\"http_url\":\t\"" RUUVI_GATEWAY_HTTP_DEFAULT_URL "\",\n"
               "\t\"http_user\":\t\"\",\n"
               "\t\"http_pass\":\t\"\",\n"
               "\t\"use_http_stat\":\ttrue,\n"
               "\t\"http_stat_url\":\t\"" RUUVI_GATEWAY_HTTP_STATUS_URL "\",\n"
               "\t\"http_stat_user\":\t\"\",\n"
               "\t\"http_stat_pass\":\t\"\",\n"
               "\t\"use_mqtt\":\tfalse,\n"
               "\t\"mqtt_use_default_prefix\":\ttrue,\n"
               "\t\"mqtt_transport\":\t\"TCP\",\n"
               "\t\"mqtt_server\":\t\"\",\n"
               "\t\"mqtt_port\":\t0,\n"
               "\t\"mqtt_prefix\":\t\"\",\n"
               "\t\"mqtt_client_id\":\t\"\",\n"
               "\t\"mqtt_user\":\t\"\",\n"
               "\t\"mqtt_pass\":\t\"\",\n"
               "\t\"lan_auth_type\":\t\"lan_auth_ruuvi\",\n"
               "\t\"lan_auth_user\":\t\"Admin\",\n"
               "\t\"lan_auth_pass\":\t\"\xFFpassword_md5\xFF\",\n"
               "\t\"lan_auth_api_key\":\t\"\",\n"
               "\t\"auto_update_cycle\":\t\"regular\",\n"
               "\t\"auto_update_weekdays_bitmask\":\t127,\n"
               "\t\"auto_update_interval_from\":\t0,\n"
               "\t\"auto_update_interval_to\":\t24,\n"
               "\t\"auto_update_tz_offset_hours\":\t3,\n"
               "\t\"company_id\":\t1177,\n"
               "\t\"company_use_filtering\":\ttrue,\n"
               "\t\"scan_coded_phy\":\tfalse,\n"
               "\t\"scan_1mbit_phy\":\ttrue,\n"
               "\t\"scan_extended_payload\":\tfalse,\n"
               "\t\"scan_channel_37\":\ttrue,\n"
               "\t\"scan_channel_38\":\ttrue,\n"
               "\t\"scan_channel_39\":\ttrue,\n"
               "\t\"coordinates\":\t\"\"\n"
               "}"),
        string(json_str.p_str));
    ASSERT_TRUE(esp_log_wrapper_is_empty());

    ruuvi_gateway_config_t gw_cfg2 = {};
    ASSERT_TRUE(gw_cfg_json_parse(json_str.p_str, &gw_cfg2));
    cjson_wrap_free_json_str(&json_str);

    ASSERT_TRUE(0 == memcmp(&gw_cfg, &gw_cfg2, sizeof(gw_cfg)));
}

TEST_F(TestGwCfgJson, gw_cfg_json_generate_scan_channel_37_false) // NOLINT
{
    ruuvi_gateway_config_t gw_cfg   = get_gateway_config_default();
    cjson_wrap_str_t       json_str = cjson_wrap_str_null();

    gw_cfg.scan.scan_coded_phy        = false;
    gw_cfg.scan.scan_1mbit_phy        = true;
    gw_cfg.scan.scan_extended_payload = true;
    gw_cfg.scan.scan_channel_37       = false;
    gw_cfg.scan.scan_channel_38       = true;
    gw_cfg.scan.scan_channel_39       = true;

    ASSERT_TRUE(gw_cfg_json_generate(&gw_cfg, &json_str));
    ASSERT_NE(nullptr, json_str.p_str);
    ASSERT_EQ(
        string("{\n"
               "\t\"use_eth\":\tfalse,\n"
               "\t\"eth_dhcp\":\ttrue,\n"
               "\t\"eth_static_ip\":\t\"\",\n"
               "\t\"eth_netmask\":\t\"\",\n"
               "\t\"eth_gw\":\t\"\",\n"
               "\t\"eth_dns1\":\t\"\",\n"
               "\t\"eth_dns2\":\t\"\",\n"
               "\t\"use_http\":\ttrue,\n"
               "\t\"http_url\":\t\"" RUUVI_GATEWAY_HTTP_DEFAULT_URL "\",\n"
               "\t\"http_user\":\t\"\",\n"
               "\t\"http_pass\":\t\"\",\n"
               "\t\"use_http_stat\":\ttrue,\n"
               "\t\"http_stat_url\":\t\"" RUUVI_GATEWAY_HTTP_STATUS_URL "\",\n"
               "\t\"http_stat_user\":\t\"\",\n"
               "\t\"http_stat_pass\":\t\"\",\n"
               "\t\"use_mqtt\":\tfalse,\n"
               "\t\"mqtt_use_default_prefix\":\ttrue,\n"
               "\t\"mqtt_transport\":\t\"TCP\",\n"
               "\t\"mqtt_server\":\t\"\",\n"
               "\t\"mqtt_port\":\t0,\n"
               "\t\"mqtt_prefix\":\t\"\",\n"
               "\t\"mqtt_client_id\":\t\"\",\n"
               "\t\"mqtt_user\":\t\"\",\n"
               "\t\"mqtt_pass\":\t\"\",\n"
               "\t\"lan_auth_type\":\t\"lan_auth_ruuvi\",\n"
               "\t\"lan_auth_user\":\t\"Admin\",\n"
               "\t\"lan_auth_pass\":\t\"\xFFpassword_md5\xFF\",\n"
               "\t\"lan_auth_api_key\":\t\"\",\n"
               "\t\"auto_update_cycle\":\t\"regular\",\n"
               "\t\"auto_update_weekdays_bitmask\":\t127,\n"
               "\t\"auto_update_interval_from\":\t0,\n"
               "\t\"auto_update_interval_to\":\t24,\n"
               "\t\"auto_update_tz_offset_hours\":\t3,\n"
               "\t\"company_id\":\t1177,\n"
               "\t\"company_use_filtering\":\ttrue,\n"
               "\t\"scan_coded_phy\":\tfalse,\n"
               "\t\"scan_1mbit_phy\":\ttrue,\n"
               "\t\"scan_extended_payload\":\ttrue,\n"
               "\t\"scan_channel_37\":\tfalse,\n"
               "\t\"scan_channel_38\":\ttrue,\n"
               "\t\"scan_channel_39\":\ttrue,\n"
               "\t\"coordinates\":\t\"\"\n"
               "}"),
        string(json_str.p_str));
    ASSERT_TRUE(esp_log_wrapper_is_empty());

    ruuvi_gateway_config_t gw_cfg2 = {};
    ASSERT_TRUE(gw_cfg_json_parse(json_str.p_str, &gw_cfg2));
    cjson_wrap_free_json_str(&json_str);

    ASSERT_TRUE(0 == memcmp(&gw_cfg, &gw_cfg2, sizeof(gw_cfg)));
}

TEST_F(TestGwCfgJson, gw_cfg_json_generate_scan_channel_38_false) // NOLINT
{
    ruuvi_gateway_config_t gw_cfg   = get_gateway_config_default();
    cjson_wrap_str_t       json_str = cjson_wrap_str_null();

    gw_cfg.scan.scan_coded_phy        = false;
    gw_cfg.scan.scan_1mbit_phy        = true;
    gw_cfg.scan.scan_extended_payload = true;
    gw_cfg.scan.scan_channel_37       = true;
    gw_cfg.scan.scan_channel_38       = false;
    gw_cfg.scan.scan_channel_39       = true;

    ASSERT_TRUE(gw_cfg_json_generate(&gw_cfg, &json_str));
    ASSERT_NE(nullptr, json_str.p_str);
    ASSERT_EQ(
        string("{\n"
               "\t\"use_eth\":\tfalse,\n"
               "\t\"eth_dhcp\":\ttrue,\n"
               "\t\"eth_static_ip\":\t\"\",\n"
               "\t\"eth_netmask\":\t\"\",\n"
               "\t\"eth_gw\":\t\"\",\n"
               "\t\"eth_dns1\":\t\"\",\n"
               "\t\"eth_dns2\":\t\"\",\n"
               "\t\"use_http\":\ttrue,\n"
               "\t\"http_url\":\t\"" RUUVI_GATEWAY_HTTP_DEFAULT_URL "\",\n"
               "\t\"http_user\":\t\"\",\n"
               "\t\"http_pass\":\t\"\",\n"
               "\t\"use_http_stat\":\ttrue,\n"
               "\t\"http_stat_url\":\t\"" RUUVI_GATEWAY_HTTP_STATUS_URL "\",\n"
               "\t\"http_stat_user\":\t\"\",\n"
               "\t\"http_stat_pass\":\t\"\",\n"
               "\t\"use_mqtt\":\tfalse,\n"
               "\t\"mqtt_use_default_prefix\":\ttrue,\n"
               "\t\"mqtt_transport\":\t\"TCP\",\n"
               "\t\"mqtt_server\":\t\"\",\n"
               "\t\"mqtt_port\":\t0,\n"
               "\t\"mqtt_prefix\":\t\"\",\n"
               "\t\"mqtt_client_id\":\t\"\",\n"
               "\t\"mqtt_user\":\t\"\",\n"
               "\t\"mqtt_pass\":\t\"\",\n"
               "\t\"lan_auth_type\":\t\"lan_auth_ruuvi\",\n"
               "\t\"lan_auth_user\":\t\"Admin\",\n"
               "\t\"lan_auth_pass\":\t\"\xFFpassword_md5\xFF\",\n"
               "\t\"lan_auth_api_key\":\t\"\",\n"
               "\t\"auto_update_cycle\":\t\"regular\",\n"
               "\t\"auto_update_weekdays_bitmask\":\t127,\n"
               "\t\"auto_update_interval_from\":\t0,\n"
               "\t\"auto_update_interval_to\":\t24,\n"
               "\t\"auto_update_tz_offset_hours\":\t3,\n"
               "\t\"company_id\":\t1177,\n"
               "\t\"company_use_filtering\":\ttrue,\n"
               "\t\"scan_coded_phy\":\tfalse,\n"
               "\t\"scan_1mbit_phy\":\ttrue,\n"
               "\t\"scan_extended_payload\":\ttrue,\n"
               "\t\"scan_channel_37\":\ttrue,\n"
               "\t\"scan_channel_38\":\tfalse,\n"
               "\t\"scan_channel_39\":\ttrue,\n"
               "\t\"coordinates\":\t\"\"\n"
               "}"),
        string(json_str.p_str));
    ASSERT_TRUE(esp_log_wrapper_is_empty());

    ruuvi_gateway_config_t gw_cfg2 = {};
    ASSERT_TRUE(gw_cfg_json_parse(json_str.p_str, &gw_cfg2));
    cjson_wrap_free_json_str(&json_str);

    ASSERT_TRUE(0 == memcmp(&gw_cfg, &gw_cfg2, sizeof(gw_cfg)));
}

TEST_F(TestGwCfgJson, gw_cfg_json_generate_scan_channel_39_false) // NOLINT
{
    ruuvi_gateway_config_t gw_cfg   = get_gateway_config_default();
    cjson_wrap_str_t       json_str = cjson_wrap_str_null();

    gw_cfg.scan.scan_coded_phy        = false;
    gw_cfg.scan.scan_1mbit_phy        = true;
    gw_cfg.scan.scan_extended_payload = true;
    gw_cfg.scan.scan_channel_37       = true;
    gw_cfg.scan.scan_channel_38       = true;
    gw_cfg.scan.scan_channel_39       = false;

    ASSERT_TRUE(gw_cfg_json_generate(&gw_cfg, &json_str));
    ASSERT_NE(nullptr, json_str.p_str);
    ASSERT_EQ(
        string("{\n"
               "\t\"use_eth\":\tfalse,\n"
               "\t\"eth_dhcp\":\ttrue,\n"
               "\t\"eth_static_ip\":\t\"\",\n"
               "\t\"eth_netmask\":\t\"\",\n"
               "\t\"eth_gw\":\t\"\",\n"
               "\t\"eth_dns1\":\t\"\",\n"
               "\t\"eth_dns2\":\t\"\",\n"
               "\t\"use_http\":\ttrue,\n"
               "\t\"http_url\":\t\"" RUUVI_GATEWAY_HTTP_DEFAULT_URL "\",\n"
               "\t\"http_user\":\t\"\",\n"
               "\t\"http_pass\":\t\"\",\n"
               "\t\"use_http_stat\":\ttrue,\n"
               "\t\"http_stat_url\":\t\"" RUUVI_GATEWAY_HTTP_STATUS_URL "\",\n"
               "\t\"http_stat_user\":\t\"\",\n"
               "\t\"http_stat_pass\":\t\"\",\n"
               "\t\"use_mqtt\":\tfalse,\n"
               "\t\"mqtt_use_default_prefix\":\ttrue,\n"
               "\t\"mqtt_transport\":\t\"TCP\",\n"
               "\t\"mqtt_server\":\t\"\",\n"
               "\t\"mqtt_port\":\t0,\n"
               "\t\"mqtt_prefix\":\t\"\",\n"
               "\t\"mqtt_client_id\":\t\"\",\n"
               "\t\"mqtt_user\":\t\"\",\n"
               "\t\"mqtt_pass\":\t\"\",\n"
               "\t\"lan_auth_type\":\t\"lan_auth_ruuvi\",\n"
               "\t\"lan_auth_user\":\t\"Admin\",\n"
               "\t\"lan_auth_pass\":\t\"\xFFpassword_md5\xFF\",\n"
               "\t\"lan_auth_api_key\":\t\"\",\n"
               "\t\"auto_update_cycle\":\t\"regular\",\n"
               "\t\"auto_update_weekdays_bitmask\":\t127,\n"
               "\t\"auto_update_interval_from\":\t0,\n"
               "\t\"auto_update_interval_to\":\t24,\n"
               "\t\"auto_update_tz_offset_hours\":\t3,\n"
               "\t\"company_id\":\t1177,\n"
               "\t\"company_use_filtering\":\ttrue,\n"
               "\t\"scan_coded_phy\":\tfalse,\n"
               "\t\"scan_1mbit_phy\":\ttrue,\n"
               "\t\"scan_extended_payload\":\ttrue,\n"
               "\t\"scan_channel_37\":\ttrue,\n"
               "\t\"scan_channel_38\":\ttrue,\n"
               "\t\"scan_channel_39\":\tfalse,\n"
               "\t\"coordinates\":\t\"\"\n"
               "}"),
        string(json_str.p_str));
    ASSERT_TRUE(esp_log_wrapper_is_empty());

    ruuvi_gateway_config_t gw_cfg2 = {};
    ASSERT_TRUE(gw_cfg_json_parse(json_str.p_str, &gw_cfg2));
    cjson_wrap_free_json_str(&json_str);

    ASSERT_TRUE(0 == memcmp(&gw_cfg, &gw_cfg2, sizeof(gw_cfg)));
}

TEST_F(TestGwCfgJson, gw_cfg_json_generate_coordinates) // NOLINT
{
    ruuvi_gateway_config_t gw_cfg   = get_gateway_config_default();
    cjson_wrap_str_t       json_str = cjson_wrap_str_null();

    snprintf(gw_cfg.coordinates.buf, sizeof(gw_cfg.coordinates.buf), "123,456");

    ASSERT_TRUE(gw_cfg_json_generate(&gw_cfg, &json_str));
    ASSERT_NE(nullptr, json_str.p_str);
    ASSERT_EQ(
        string("{\n"
               "\t\"use_eth\":\tfalse,\n"
               "\t\"eth_dhcp\":\ttrue,\n"
               "\t\"eth_static_ip\":\t\"\",\n"
               "\t\"eth_netmask\":\t\"\",\n"
               "\t\"eth_gw\":\t\"\",\n"
               "\t\"eth_dns1\":\t\"\",\n"
               "\t\"eth_dns2\":\t\"\",\n"
               "\t\"use_http\":\ttrue,\n"
               "\t\"http_url\":\t\"" RUUVI_GATEWAY_HTTP_DEFAULT_URL "\",\n"
               "\t\"http_user\":\t\"\",\n"
               "\t\"http_pass\":\t\"\",\n"
               "\t\"use_http_stat\":\ttrue,\n"
               "\t\"http_stat_url\":\t\"" RUUVI_GATEWAY_HTTP_STATUS_URL "\",\n"
               "\t\"http_stat_user\":\t\"\",\n"
               "\t\"http_stat_pass\":\t\"\",\n"
               "\t\"use_mqtt\":\tfalse,\n"
               "\t\"mqtt_use_default_prefix\":\ttrue,\n"
               "\t\"mqtt_transport\":\t\"TCP\",\n"
               "\t\"mqtt_server\":\t\"\",\n"
               "\t\"mqtt_port\":\t0,\n"
               "\t\"mqtt_prefix\":\t\"\",\n"
               "\t\"mqtt_client_id\":\t\"\",\n"
               "\t\"mqtt_user\":\t\"\",\n"
               "\t\"mqtt_pass\":\t\"\",\n"
               "\t\"lan_auth_type\":\t\"lan_auth_ruuvi\",\n"
               "\t\"lan_auth_user\":\t\"Admin\",\n"
               "\t\"lan_auth_pass\":\t\"\xFFpassword_md5\xFF\",\n"
               "\t\"lan_auth_api_key\":\t\"\",\n"
               "\t\"auto_update_cycle\":\t\"regular\",\n"
               "\t\"auto_update_weekdays_bitmask\":\t127,\n"
               "\t\"auto_update_interval_from\":\t0,\n"
               "\t\"auto_update_interval_to\":\t24,\n"
               "\t\"auto_update_tz_offset_hours\":\t3,\n"
               "\t\"company_id\":\t1177,\n"
               "\t\"company_use_filtering\":\ttrue,\n"
               "\t\"scan_coded_phy\":\tfalse,\n"
               "\t\"scan_1mbit_phy\":\ttrue,\n"
               "\t\"scan_extended_payload\":\ttrue,\n"
               "\t\"scan_channel_37\":\ttrue,\n"
               "\t\"scan_channel_38\":\ttrue,\n"
               "\t\"scan_channel_39\":\ttrue,\n"
               "\t\"coordinates\":\t\"123,456\"\n"
               "}"),
        string(json_str.p_str));
    ASSERT_TRUE(esp_log_wrapper_is_empty());

    ruuvi_gateway_config_t gw_cfg2 = {};
    ASSERT_TRUE(gw_cfg_json_parse(json_str.p_str, &gw_cfg2));
    cjson_wrap_free_json_str(&json_str);

    ASSERT_TRUE(0 == memcmp(&gw_cfg, &gw_cfg2, sizeof(gw_cfg)));
}

TEST_F(TestGwCfgJson, gw_cfg_json_generate_parse_generate_default) // NOLINT
{
    const ruuvi_gateway_config_t gw_cfg   = get_gateway_config_default();
    cjson_wrap_str_t             json_str = cjson_wrap_str_null();

    ASSERT_TRUE(gw_cfg_json_generate(&gw_cfg, &json_str));
    ASSERT_NE(nullptr, json_str.p_str);
    ruuvi_gateway_config_t gw_cfg2 = {};
    ASSERT_TRUE(gw_cfg_json_parse(json_str.p_str, &gw_cfg2));
    cjson_wrap_free_json_str(&json_str);

    ASSERT_EQ(false, gw_cfg2.eth.use_eth);
    ASSERT_EQ(true, gw_cfg2.eth.eth_dhcp);
    ASSERT_EQ(string(""), gw_cfg2.eth.eth_static_ip.buf);
    ASSERT_EQ(string(""), gw_cfg2.eth.eth_netmask.buf);
    ASSERT_EQ(string(""), gw_cfg2.eth.eth_gw.buf);
    ASSERT_EQ(string(""), gw_cfg2.eth.eth_dns1.buf);
    ASSERT_EQ(string(""), gw_cfg2.eth.eth_dns2.buf);

    ASSERT_EQ(false, gw_cfg2.mqtt.use_mqtt);
    ASSERT_EQ(true, gw_cfg2.mqtt.mqtt_use_default_prefix);
    ASSERT_EQ(string("TCP"), gw_cfg2.mqtt.mqtt_transport.buf);
    ASSERT_EQ(string(""), gw_cfg2.mqtt.mqtt_server.buf);
    ASSERT_EQ(0, gw_cfg2.mqtt.mqtt_port);
    ASSERT_EQ(string(""), gw_cfg2.mqtt.mqtt_prefix.buf);
    ASSERT_EQ(string(""), gw_cfg2.mqtt.mqtt_client_id.buf);
    ASSERT_EQ(string(""), gw_cfg2.mqtt.mqtt_user.buf);
    ASSERT_EQ(string(""), gw_cfg2.mqtt.mqtt_pass.buf);

    ASSERT_EQ(true, gw_cfg2.http.use_http);
    ASSERT_EQ(string(RUUVI_GATEWAY_HTTP_DEFAULT_URL), gw_cfg2.http.http_url.buf);
    ASSERT_EQ(string(""), gw_cfg2.http.http_user.buf);
    ASSERT_EQ(string(""), gw_cfg2.http.http_pass.buf);

    ASSERT_EQ(true, gw_cfg2.http_stat.use_http_stat);
    ASSERT_EQ(string(RUUVI_GATEWAY_HTTP_STATUS_URL), gw_cfg2.http_stat.http_stat_url.buf);
    ASSERT_EQ(string(""), gw_cfg2.http_stat.http_stat_user.buf);
    ASSERT_EQ(string(""), gw_cfg2.http_stat.http_stat_pass.buf);

    ASSERT_EQ(string(HTTP_SERVER_AUTH_TYPE_STR_RUUVI), gw_cfg2.lan_auth.lan_auth_type);
    ASSERT_EQ(string(RUUVI_GATEWAY_AUTH_DEFAULT_USER), gw_cfg2.lan_auth.lan_auth_user);
    ASSERT_EQ(string("\xFFpassword_md5\xFF"), gw_cfg2.lan_auth.lan_auth_pass);
    ASSERT_EQ(string(""), gw_cfg2.lan_auth.lan_auth_api_key);

    ASSERT_EQ(AUTO_UPDATE_CYCLE_TYPE_REGULAR, gw_cfg2.auto_update.auto_update_cycle);
    ASSERT_EQ(0x7F, gw_cfg2.auto_update.auto_update_weekdays_bitmask);
    ASSERT_EQ(0, gw_cfg2.auto_update.auto_update_interval_from);
    ASSERT_EQ(24, gw_cfg2.auto_update.auto_update_interval_to);
    ASSERT_EQ(3, gw_cfg2.auto_update.auto_update_tz_offset_hours);

    ASSERT_EQ(RUUVI_COMPANY_ID, gw_cfg2.filter.company_id);
    ASSERT_EQ(true, gw_cfg2.filter.company_use_filtering);

    ASSERT_EQ(false, gw_cfg2.scan.scan_coded_phy);
    ASSERT_EQ(true, gw_cfg2.scan.scan_1mbit_phy);
    ASSERT_EQ(true, gw_cfg2.scan.scan_extended_payload);
    ASSERT_EQ(true, gw_cfg2.scan.scan_channel_37);
    ASSERT_EQ(true, gw_cfg2.scan.scan_channel_38);
    ASSERT_EQ(true, gw_cfg2.scan.scan_channel_39);

    ASSERT_EQ(string(""), gw_cfg2.coordinates.buf);

    ASSERT_TRUE(0 == memcmp(&gw_cfg, &gw_cfg2, sizeof(gw_cfg)));

    ASSERT_TRUE(gw_cfg_json_generate(&gw_cfg2, &json_str));
    ASSERT_EQ(
        string("{\n"
               "\t\"use_eth\":\tfalse,\n"
               "\t\"eth_dhcp\":\ttrue,\n"
               "\t\"eth_static_ip\":\t\"\",\n"
               "\t\"eth_netmask\":\t\"\",\n"
               "\t\"eth_gw\":\t\"\",\n"
               "\t\"eth_dns1\":\t\"\",\n"
               "\t\"eth_dns2\":\t\"\",\n"
               "\t\"use_http\":\ttrue,\n"
               "\t\"http_url\":\t\"" RUUVI_GATEWAY_HTTP_DEFAULT_URL "\",\n"
               "\t\"http_user\":\t\"\",\n"
               "\t\"http_pass\":\t\"\",\n"
               "\t\"use_http_stat\":\ttrue,\n"
               "\t\"http_stat_url\":\t\"" RUUVI_GATEWAY_HTTP_STATUS_URL "\",\n"
               "\t\"http_stat_user\":\t\"\",\n"
               "\t\"http_stat_pass\":\t\"\",\n"
               "\t\"use_mqtt\":\tfalse,\n"
               "\t\"mqtt_use_default_prefix\":\ttrue,\n"
               "\t\"mqtt_transport\":\t\"TCP\",\n"
               "\t\"mqtt_server\":\t\"\",\n"
               "\t\"mqtt_port\":\t0,\n"
               "\t\"mqtt_prefix\":\t\"\",\n"
               "\t\"mqtt_client_id\":\t\"\",\n"
               "\t\"mqtt_user\":\t\"\",\n"
               "\t\"mqtt_pass\":\t\"\",\n"
               "\t\"lan_auth_type\":\t\"lan_auth_ruuvi\",\n"
               "\t\"lan_auth_user\":\t\"Admin\",\n"
               "\t\"lan_auth_pass\":\t\"\xFFpassword_md5\xFF\",\n"
               "\t\"lan_auth_api_key\":\t\"\",\n"
               "\t\"auto_update_cycle\":\t\"regular\",\n"
               "\t\"auto_update_weekdays_bitmask\":\t127,\n"
               "\t\"auto_update_interval_from\":\t0,\n"
               "\t\"auto_update_interval_to\":\t24,\n"
               "\t\"auto_update_tz_offset_hours\":\t3,\n"
               "\t\"company_id\":\t1177,\n"
               "\t\"company_use_filtering\":\ttrue,\n"
               "\t\"scan_coded_phy\":\tfalse,\n"
               "\t\"scan_1mbit_phy\":\ttrue,\n"
               "\t\"scan_extended_payload\":\ttrue,\n"
               "\t\"scan_channel_37\":\ttrue,\n"
               "\t\"scan_channel_38\":\ttrue,\n"
               "\t\"scan_channel_39\":\ttrue,\n"
               "\t\"coordinates\":\t\"\"\n"
               "}"),
        string(json_str.p_str));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    cjson_wrap_free_json_str(&json_str);
}

TEST_F(TestGwCfgJson, gw_cfg_json_generate_parse_generate_auto_update_regular) // NOLINT
{
    const ruuvi_gateway_config_t gw_cfg   = {
        .eth = {
            true,
            false,
            "192.168.1.10",
            "255.255.255.0",
            "192.168.1.1",
            "8.8.8.8",
            "4.4.4.4",
        },
        .mqtt = {
            true,
            false,
            "SSL",
            "test.mosquitto.org",
            1338,
            "my_prefix",
            "my_client",
            "m_user1",
            "m_pass1",
        },
        .http = {
            false,
            "https://myserver1.com",
            "h_user1",
            "h_pass1",
        },
        .http_stat = {
            false,
            "https://myserver1.com/status",
            "h_user2",
            "h_pass2",
        },
        .lan_auth = {
            HTTP_SERVER_AUTH_TYPE_STR_DIGEST,
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
        .coordinates = { "coordinates1" },
    };
    cjson_wrap_str_t json_str = cjson_wrap_str_null();

    ASSERT_TRUE(gw_cfg_json_generate(&gw_cfg, &json_str));
    ASSERT_NE(nullptr, json_str.p_str);

    ruuvi_gateway_config_t gw_cfg2 = {};
    ASSERT_TRUE(gw_cfg_json_parse(json_str.p_str, &gw_cfg2));
    cjson_wrap_free_json_str(&json_str);

    ASSERT_EQ(true, gw_cfg2.eth.use_eth);
    ASSERT_EQ(false, gw_cfg2.eth.eth_dhcp);
    ASSERT_EQ(string("192.168.1.10"), gw_cfg2.eth.eth_static_ip.buf);
    ASSERT_EQ(string("255.255.255.0"), gw_cfg2.eth.eth_netmask.buf);
    ASSERT_EQ(string("192.168.1.1"), gw_cfg2.eth.eth_gw.buf);
    ASSERT_EQ(string("8.8.8.8"), gw_cfg2.eth.eth_dns1.buf);
    ASSERT_EQ(string("4.4.4.4"), gw_cfg2.eth.eth_dns2.buf);

    ASSERT_EQ(true, gw_cfg2.mqtt.use_mqtt);
    ASSERT_EQ(false, gw_cfg2.mqtt.mqtt_use_default_prefix);
    ASSERT_EQ(string("SSL"), gw_cfg2.mqtt.mqtt_transport.buf);
    ASSERT_EQ(string("test.mosquitto.org"), gw_cfg2.mqtt.mqtt_server.buf);
    ASSERT_EQ(1338, gw_cfg2.mqtt.mqtt_port);
    ASSERT_EQ(string("my_prefix"), gw_cfg2.mqtt.mqtt_prefix.buf);
    ASSERT_EQ(string("my_client"), gw_cfg2.mqtt.mqtt_client_id.buf);
    ASSERT_EQ(string("m_user1"), gw_cfg2.mqtt.mqtt_user.buf);
    ASSERT_EQ(string("m_pass1"), gw_cfg2.mqtt.mqtt_pass.buf);

    ASSERT_EQ(false, gw_cfg2.http.use_http);
    ASSERT_EQ(string("https://myserver1.com"), gw_cfg2.http.http_url.buf);
    ASSERT_EQ(string("h_user1"), gw_cfg2.http.http_user.buf);
    ASSERT_EQ(string("h_pass1"), gw_cfg2.http.http_pass.buf);

    ASSERT_EQ(false, gw_cfg2.http_stat.use_http_stat);
    ASSERT_EQ(string("https://myserver1.com/status"), gw_cfg2.http_stat.http_stat_url.buf);
    ASSERT_EQ(string("h_user2"), gw_cfg2.http_stat.http_stat_user.buf);
    ASSERT_EQ(string("h_pass2"), gw_cfg2.http_stat.http_stat_pass.buf);

    ASSERT_EQ(string(HTTP_SERVER_AUTH_TYPE_STR_DIGEST), gw_cfg2.lan_auth.lan_auth_type);
    ASSERT_EQ(string("l_user1"), gw_cfg2.lan_auth.lan_auth_user);
    ASSERT_EQ(string("l_pass1"), gw_cfg2.lan_auth.lan_auth_pass);
    ASSERT_EQ(string(""), gw_cfg2.lan_auth.lan_auth_api_key);

    ASSERT_EQ(AUTO_UPDATE_CYCLE_TYPE_REGULAR, gw_cfg2.auto_update.auto_update_cycle);
    ASSERT_EQ(0x3F, gw_cfg2.auto_update.auto_update_weekdays_bitmask);
    ASSERT_EQ(2, gw_cfg2.auto_update.auto_update_interval_from);
    ASSERT_EQ(22, gw_cfg2.auto_update.auto_update_interval_to);
    ASSERT_EQ(5, gw_cfg2.auto_update.auto_update_tz_offset_hours);

    ASSERT_EQ(RUUVI_COMPANY_ID + 1, gw_cfg2.filter.company_id);
    ASSERT_EQ(false, gw_cfg2.filter.company_use_filtering);

    ASSERT_EQ(true, gw_cfg2.scan.scan_coded_phy);
    ASSERT_EQ(false, gw_cfg2.scan.scan_1mbit_phy);
    ASSERT_EQ(false, gw_cfg2.scan.scan_extended_payload);
    ASSERT_EQ(false, gw_cfg2.scan.scan_channel_37);
    ASSERT_EQ(false, gw_cfg2.scan.scan_channel_38);
    ASSERT_EQ(false, gw_cfg2.scan.scan_channel_39);

    ASSERT_EQ(string("coordinates1"), gw_cfg2.coordinates.buf);

    ASSERT_TRUE(gw_cfg_json_generate(&gw_cfg2, &json_str));
    ASSERT_EQ(
        string("{\n"
               "\t\"use_eth\":\ttrue,\n"
               "\t\"eth_dhcp\":\tfalse,\n"
               "\t\"eth_static_ip\":\t\"192.168.1.10\",\n"
               "\t\"eth_netmask\":\t\"255.255.255.0\",\n"
               "\t\"eth_gw\":\t\"192.168.1.1\",\n"
               "\t\"eth_dns1\":\t\"8.8.8.8\",\n"
               "\t\"eth_dns2\":\t\"4.4.4.4\",\n"
               "\t\"use_http\":\tfalse,\n"
               "\t\"http_url\":\t\"https://myserver1.com\",\n"
               "\t\"http_user\":\t\"h_user1\",\n"
               "\t\"http_pass\":\t\"h_pass1\",\n"
               "\t\"use_http_stat\":\tfalse,\n"
               "\t\"http_stat_url\":\t\"https://myserver1.com/status\",\n"
               "\t\"http_stat_user\":\t\"h_user2\",\n"
               "\t\"http_stat_pass\":\t\"h_pass2\",\n"
               "\t\"use_mqtt\":\ttrue,\n"
               "\t\"mqtt_use_default_prefix\":\tfalse,\n"
               "\t\"mqtt_transport\":\t\"SSL\",\n"
               "\t\"mqtt_server\":\t\"test.mosquitto.org\",\n"
               "\t\"mqtt_port\":\t1338,\n"
               "\t\"mqtt_prefix\":\t\"my_prefix\",\n"
               "\t\"mqtt_client_id\":\t\"my_client\",\n"
               "\t\"mqtt_user\":\t\"m_user1\",\n"
               "\t\"mqtt_pass\":\t\"m_pass1\",\n"
               "\t\"lan_auth_type\":\t\"lan_auth_digest\",\n"
               "\t\"lan_auth_user\":\t\"l_user1\",\n"
               "\t\"lan_auth_pass\":\t\"l_pass1\",\n"
               "\t\"lan_auth_api_key\":\t\"\",\n"
               "\t\"auto_update_cycle\":\t\"regular\",\n"
               "\t\"auto_update_weekdays_bitmask\":\t63,\n"
               "\t\"auto_update_interval_from\":\t2,\n"
               "\t\"auto_update_interval_to\":\t22,\n"
               "\t\"auto_update_tz_offset_hours\":\t5,\n"
               "\t\"company_id\":\t1178,\n"
               "\t\"company_use_filtering\":\tfalse,\n"
               "\t\"scan_coded_phy\":\ttrue,\n"
               "\t\"scan_1mbit_phy\":\tfalse,\n"
               "\t\"scan_extended_payload\":\tfalse,\n"
               "\t\"scan_channel_37\":\tfalse,\n"
               "\t\"scan_channel_38\":\tfalse,\n"
               "\t\"scan_channel_39\":\tfalse,\n"
               "\t\"coordinates\":\t\"coordinates1\"\n"
               "}"),
        string(json_str.p_str));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    cjson_wrap_free_json_str(&json_str);
}

TEST_F(TestGwCfgJson, gw_cfg_json_generate_parse_generate_auto_update_beta_tester) // NOLINT
{
    const ruuvi_gateway_config_t gw_cfg   = {
        .eth = {
            true,
            false,
            "192.168.1.10",
            "255.255.255.0",
            "192.168.1.1",
            "8.8.8.8",
            "4.4.4.4",
        },
        .mqtt = {
            true,
            false,
            "SSL",
            "test.mosquitto.org",
            1338,
            "my_prefix",
            "my_client",
            "m_user1",
            "m_pass1",
        },
        .http = {
            false,
            "https://myserver1.com",
            "h_user1",
            "h_pass1",
        },
        .http_stat = {
            false,
            "https://myserver1.com/status",
            "h_user2",
            "h_pass2",
        },
        .lan_auth = {
            HTTP_SERVER_AUTH_TYPE_STR_DIGEST,
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
        .coordinates = { "coordinates1" },
    };
    cjson_wrap_str_t json_str = cjson_wrap_str_null();

    ASSERT_TRUE(gw_cfg_json_generate(&gw_cfg, &json_str));
    ASSERT_NE(nullptr, json_str.p_str);

    ruuvi_gateway_config_t gw_cfg2 = {};
    ASSERT_TRUE(gw_cfg_json_parse(json_str.p_str, &gw_cfg2));
    cjson_wrap_free_json_str(&json_str);

    ASSERT_EQ(true, gw_cfg2.eth.use_eth);
    ASSERT_EQ(false, gw_cfg2.eth.eth_dhcp);
    ASSERT_EQ(string("192.168.1.10"), gw_cfg2.eth.eth_static_ip.buf);
    ASSERT_EQ(string("255.255.255.0"), gw_cfg2.eth.eth_netmask.buf);
    ASSERT_EQ(string("192.168.1.1"), gw_cfg2.eth.eth_gw.buf);
    ASSERT_EQ(string("8.8.8.8"), gw_cfg2.eth.eth_dns1.buf);
    ASSERT_EQ(string("4.4.4.4"), gw_cfg2.eth.eth_dns2.buf);

    ASSERT_EQ(true, gw_cfg2.mqtt.use_mqtt);
    ASSERT_EQ(false, gw_cfg2.mqtt.mqtt_use_default_prefix);
    ASSERT_EQ(string("SSL"), gw_cfg2.mqtt.mqtt_transport.buf);
    ASSERT_EQ(string("test.mosquitto.org"), gw_cfg2.mqtt.mqtt_server.buf);
    ASSERT_EQ(1338, gw_cfg2.mqtt.mqtt_port);
    ASSERT_EQ(string("my_prefix"), gw_cfg2.mqtt.mqtt_prefix.buf);
    ASSERT_EQ(string("my_client"), gw_cfg2.mqtt.mqtt_client_id.buf);
    ASSERT_EQ(string("m_user1"), gw_cfg2.mqtt.mqtt_user.buf);
    ASSERT_EQ(string("m_pass1"), gw_cfg2.mqtt.mqtt_pass.buf);

    ASSERT_EQ(false, gw_cfg2.http.use_http);
    ASSERT_EQ(string("https://myserver1.com"), gw_cfg2.http.http_url.buf);
    ASSERT_EQ(string("h_user1"), gw_cfg2.http.http_user.buf);
    ASSERT_EQ(string("h_pass1"), gw_cfg2.http.http_pass.buf);

    ASSERT_EQ(false, gw_cfg2.http_stat.use_http_stat);
    ASSERT_EQ(string("https://myserver1.com/status"), gw_cfg2.http_stat.http_stat_url.buf);
    ASSERT_EQ(string("h_user2"), gw_cfg2.http_stat.http_stat_user.buf);
    ASSERT_EQ(string("h_pass2"), gw_cfg2.http_stat.http_stat_pass.buf);

    ASSERT_EQ(string(HTTP_SERVER_AUTH_TYPE_STR_DIGEST), gw_cfg2.lan_auth.lan_auth_type);
    ASSERT_EQ(string("l_user1"), gw_cfg2.lan_auth.lan_auth_user);
    ASSERT_EQ(string("l_pass1"), gw_cfg2.lan_auth.lan_auth_pass);
    ASSERT_EQ(string(""), gw_cfg2.lan_auth.lan_auth_api_key);

    ASSERT_EQ(AUTO_UPDATE_CYCLE_TYPE_BETA_TESTER, gw_cfg2.auto_update.auto_update_cycle);
    ASSERT_EQ(0x3F, gw_cfg2.auto_update.auto_update_weekdays_bitmask);
    ASSERT_EQ(2, gw_cfg2.auto_update.auto_update_interval_from);
    ASSERT_EQ(22, gw_cfg2.auto_update.auto_update_interval_to);
    ASSERT_EQ(5, gw_cfg2.auto_update.auto_update_tz_offset_hours);

    ASSERT_EQ(RUUVI_COMPANY_ID + 1, gw_cfg2.filter.company_id);
    ASSERT_EQ(false, gw_cfg2.filter.company_use_filtering);

    ASSERT_EQ(true, gw_cfg2.scan.scan_coded_phy);
    ASSERT_EQ(false, gw_cfg2.scan.scan_1mbit_phy);
    ASSERT_EQ(false, gw_cfg2.scan.scan_extended_payload);
    ASSERT_EQ(false, gw_cfg2.scan.scan_channel_37);
    ASSERT_EQ(false, gw_cfg2.scan.scan_channel_38);
    ASSERT_EQ(false, gw_cfg2.scan.scan_channel_39);

    ASSERT_EQ(string("coordinates1"), gw_cfg2.coordinates.buf);

    ASSERT_TRUE(gw_cfg_json_generate(&gw_cfg2, &json_str));
    ASSERT_EQ(
        string("{\n"
               "\t\"use_eth\":\ttrue,\n"
               "\t\"eth_dhcp\":\tfalse,\n"
               "\t\"eth_static_ip\":\t\"192.168.1.10\",\n"
               "\t\"eth_netmask\":\t\"255.255.255.0\",\n"
               "\t\"eth_gw\":\t\"192.168.1.1\",\n"
               "\t\"eth_dns1\":\t\"8.8.8.8\",\n"
               "\t\"eth_dns2\":\t\"4.4.4.4\",\n"
               "\t\"use_http\":\tfalse,\n"
               "\t\"http_url\":\t\"https://myserver1.com\",\n"
               "\t\"http_user\":\t\"h_user1\",\n"
               "\t\"http_pass\":\t\"h_pass1\",\n"
               "\t\"use_http_stat\":\tfalse,\n"
               "\t\"http_stat_url\":\t\"https://myserver1.com/status\",\n"
               "\t\"http_stat_user\":\t\"h_user2\",\n"
               "\t\"http_stat_pass\":\t\"h_pass2\",\n"
               "\t\"use_mqtt\":\ttrue,\n"
               "\t\"mqtt_use_default_prefix\":\tfalse,\n"
               "\t\"mqtt_transport\":\t\"SSL\",\n"
               "\t\"mqtt_server\":\t\"test.mosquitto.org\",\n"
               "\t\"mqtt_port\":\t1338,\n"
               "\t\"mqtt_prefix\":\t\"my_prefix\",\n"
               "\t\"mqtt_client_id\":\t\"my_client\",\n"
               "\t\"mqtt_user\":\t\"m_user1\",\n"
               "\t\"mqtt_pass\":\t\"m_pass1\",\n"
               "\t\"lan_auth_type\":\t\"lan_auth_digest\",\n"
               "\t\"lan_auth_user\":\t\"l_user1\",\n"
               "\t\"lan_auth_pass\":\t\"l_pass1\",\n"
               "\t\"lan_auth_api_key\":\t\"\",\n"
               "\t\"auto_update_cycle\":\t\"beta\",\n"
               "\t\"auto_update_weekdays_bitmask\":\t63,\n"
               "\t\"auto_update_interval_from\":\t2,\n"
               "\t\"auto_update_interval_to\":\t22,\n"
               "\t\"auto_update_tz_offset_hours\":\t5,\n"
               "\t\"company_id\":\t1178,\n"
               "\t\"company_use_filtering\":\tfalse,\n"
               "\t\"scan_coded_phy\":\ttrue,\n"
               "\t\"scan_1mbit_phy\":\tfalse,\n"
               "\t\"scan_extended_payload\":\tfalse,\n"
               "\t\"scan_channel_37\":\tfalse,\n"
               "\t\"scan_channel_38\":\tfalse,\n"
               "\t\"scan_channel_39\":\tfalse,\n"
               "\t\"coordinates\":\t\"coordinates1\"\n"
               "}"),
        string(json_str.p_str));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    cjson_wrap_free_json_str(&json_str);
}

TEST_F(TestGwCfgJson, gw_cfg_json_generate_parse_generate_auto_update_manual) // NOLINT
{
    const ruuvi_gateway_config_t gw_cfg   = {
        .eth = {
            true,
            false,
            "192.168.1.10",
            "255.255.255.0",
            "192.168.1.1",
            "8.8.8.8",
            "4.4.4.4",
        },
        .mqtt = {
            true,
            false,
            "SSL",
            "test.mosquitto.org",
            1338,
            "my_prefix",
            "my_client",
            "m_user1",
            "m_pass1",
        },
        .http = {
            false,
            "https://myserver1.com",
            "h_user1",
            "h_pass1",
        },
        .http_stat = {
            false,
            "https://myserver1.com/status",
            "h_user2",
            "h_pass2",
        },
        .lan_auth = {
            HTTP_SERVER_AUTH_TYPE_STR_DIGEST,
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
        .coordinates = { "coordinates1" },
    };
    cjson_wrap_str_t json_str = cjson_wrap_str_null();

    ASSERT_TRUE(gw_cfg_json_generate(&gw_cfg, &json_str));
    ASSERT_NE(nullptr, json_str.p_str);

    ruuvi_gateway_config_t gw_cfg2 = {};
    ASSERT_TRUE(gw_cfg_json_parse(json_str.p_str, &gw_cfg2));
    cjson_wrap_free_json_str(&json_str);

    ASSERT_EQ(true, gw_cfg2.eth.use_eth);
    ASSERT_EQ(false, gw_cfg2.eth.eth_dhcp);
    ASSERT_EQ(string("192.168.1.10"), gw_cfg2.eth.eth_static_ip.buf);
    ASSERT_EQ(string("255.255.255.0"), gw_cfg2.eth.eth_netmask.buf);
    ASSERT_EQ(string("192.168.1.1"), gw_cfg2.eth.eth_gw.buf);
    ASSERT_EQ(string("8.8.8.8"), gw_cfg2.eth.eth_dns1.buf);
    ASSERT_EQ(string("4.4.4.4"), gw_cfg2.eth.eth_dns2.buf);

    ASSERT_EQ(true, gw_cfg2.mqtt.use_mqtt);
    ASSERT_EQ(false, gw_cfg2.mqtt.mqtt_use_default_prefix);
    ASSERT_EQ(string("SSL"), gw_cfg2.mqtt.mqtt_transport.buf);
    ASSERT_EQ(string("test.mosquitto.org"), gw_cfg2.mqtt.mqtt_server.buf);
    ASSERT_EQ(1338, gw_cfg2.mqtt.mqtt_port);
    ASSERT_EQ(string("my_prefix"), gw_cfg2.mqtt.mqtt_prefix.buf);
    ASSERT_EQ(string("my_client"), gw_cfg2.mqtt.mqtt_client_id.buf);
    ASSERT_EQ(string("m_user1"), gw_cfg2.mqtt.mqtt_user.buf);
    ASSERT_EQ(string("m_pass1"), gw_cfg2.mqtt.mqtt_pass.buf);

    ASSERT_EQ(false, gw_cfg2.http.use_http);
    ASSERT_EQ(string("https://myserver1.com"), gw_cfg2.http.http_url.buf);
    ASSERT_EQ(string("h_user1"), gw_cfg2.http.http_user.buf);
    ASSERT_EQ(string("h_pass1"), gw_cfg2.http.http_pass.buf);

    ASSERT_EQ(false, gw_cfg2.http_stat.use_http_stat);
    ASSERT_EQ(string("https://myserver1.com/status"), gw_cfg2.http_stat.http_stat_url.buf);
    ASSERT_EQ(string("h_user2"), gw_cfg2.http_stat.http_stat_user.buf);
    ASSERT_EQ(string("h_pass2"), gw_cfg2.http_stat.http_stat_pass.buf);

    ASSERT_EQ(string(HTTP_SERVER_AUTH_TYPE_STR_DIGEST), gw_cfg2.lan_auth.lan_auth_type);
    ASSERT_EQ(string("l_user1"), gw_cfg2.lan_auth.lan_auth_user);
    ASSERT_EQ(string("l_pass1"), gw_cfg2.lan_auth.lan_auth_pass);
    ASSERT_EQ(string(""), gw_cfg2.lan_auth.lan_auth_api_key);

    ASSERT_EQ(AUTO_UPDATE_CYCLE_TYPE_MANUAL, gw_cfg2.auto_update.auto_update_cycle);
    ASSERT_EQ(0x3F, gw_cfg2.auto_update.auto_update_weekdays_bitmask);
    ASSERT_EQ(2, gw_cfg2.auto_update.auto_update_interval_from);
    ASSERT_EQ(22, gw_cfg2.auto_update.auto_update_interval_to);
    ASSERT_EQ(5, gw_cfg2.auto_update.auto_update_tz_offset_hours);

    ASSERT_EQ(RUUVI_COMPANY_ID + 1, gw_cfg2.filter.company_id);
    ASSERT_EQ(false, gw_cfg2.filter.company_use_filtering);

    ASSERT_EQ(true, gw_cfg2.scan.scan_coded_phy);
    ASSERT_EQ(false, gw_cfg2.scan.scan_1mbit_phy);
    ASSERT_EQ(false, gw_cfg2.scan.scan_extended_payload);
    ASSERT_EQ(false, gw_cfg2.scan.scan_channel_37);
    ASSERT_EQ(false, gw_cfg2.scan.scan_channel_38);
    ASSERT_EQ(false, gw_cfg2.scan.scan_channel_39);

    ASSERT_EQ(string("coordinates1"), gw_cfg2.coordinates.buf);

    ASSERT_TRUE(gw_cfg_json_generate(&gw_cfg2, &json_str));
    ASSERT_EQ(
        string("{\n"
               "\t\"use_eth\":\ttrue,\n"
               "\t\"eth_dhcp\":\tfalse,\n"
               "\t\"eth_static_ip\":\t\"192.168.1.10\",\n"
               "\t\"eth_netmask\":\t\"255.255.255.0\",\n"
               "\t\"eth_gw\":\t\"192.168.1.1\",\n"
               "\t\"eth_dns1\":\t\"8.8.8.8\",\n"
               "\t\"eth_dns2\":\t\"4.4.4.4\",\n"
               "\t\"use_http\":\tfalse,\n"
               "\t\"http_url\":\t\"https://myserver1.com\",\n"
               "\t\"http_user\":\t\"h_user1\",\n"
               "\t\"http_pass\":\t\"h_pass1\",\n"
               "\t\"use_http_stat\":\tfalse,\n"
               "\t\"http_stat_url\":\t\"https://myserver1.com/status\",\n"
               "\t\"http_stat_user\":\t\"h_user2\",\n"
               "\t\"http_stat_pass\":\t\"h_pass2\",\n"
               "\t\"use_mqtt\":\ttrue,\n"
               "\t\"mqtt_use_default_prefix\":\tfalse,\n"
               "\t\"mqtt_transport\":\t\"SSL\",\n"
               "\t\"mqtt_server\":\t\"test.mosquitto.org\",\n"
               "\t\"mqtt_port\":\t1338,\n"
               "\t\"mqtt_prefix\":\t\"my_prefix\",\n"
               "\t\"mqtt_client_id\":\t\"my_client\",\n"
               "\t\"mqtt_user\":\t\"m_user1\",\n"
               "\t\"mqtt_pass\":\t\"m_pass1\",\n"
               "\t\"lan_auth_type\":\t\"lan_auth_digest\",\n"
               "\t\"lan_auth_user\":\t\"l_user1\",\n"
               "\t\"lan_auth_pass\":\t\"l_pass1\",\n"
               "\t\"lan_auth_api_key\":\t\"\",\n"
               "\t\"auto_update_cycle\":\t\"manual\",\n"
               "\t\"auto_update_weekdays_bitmask\":\t63,\n"
               "\t\"auto_update_interval_from\":\t2,\n"
               "\t\"auto_update_interval_to\":\t22,\n"
               "\t\"auto_update_tz_offset_hours\":\t5,\n"
               "\t\"company_id\":\t1178,\n"
               "\t\"company_use_filtering\":\tfalse,\n"
               "\t\"scan_coded_phy\":\ttrue,\n"
               "\t\"scan_1mbit_phy\":\tfalse,\n"
               "\t\"scan_extended_payload\":\tfalse,\n"
               "\t\"scan_channel_37\":\tfalse,\n"
               "\t\"scan_channel_38\":\tfalse,\n"
               "\t\"scan_channel_39\":\tfalse,\n"
               "\t\"coordinates\":\t\"coordinates1\"\n"
               "}"),
        string(json_str.p_str));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    cjson_wrap_free_json_str(&json_str);
}

TEST_F(TestGwCfgJson, gw_cfg_json_generate_parse_generate_auto_update_unknown) // NOLINT
{
    const char *const p_json_str
        = "{\n"
          "\t\"use_eth\":\ttrue,\n"
          "\t\"eth_dhcp\":\tfalse,\n"
          "\t\"eth_static_ip\":\t\"192.168.1.10\",\n"
          "\t\"eth_netmask\":\t\"255.255.255.0\",\n"
          "\t\"eth_gw\":\t\"192.168.1.1\",\n"
          "\t\"eth_dns1\":\t\"8.8.8.8\",\n"
          "\t\"eth_dns2\":\t\"4.4.4.4\",\n"
          "\t\"use_http\":\tfalse,\n"
          "\t\"http_url\":\t\"https://myserver1.com\",\n"
          "\t\"http_user\":\t\"h_user1\",\n"
          "\t\"http_pass\":\t\"h_pass1\",\n"
          "\t\"use_http_stat\":\tfalse,\n"
          "\t\"http_stat_url\":\t\"https://myserver1.com/status\",\n"
          "\t\"http_stat_user\":\t\"h_user2\",\n"
          "\t\"http_stat_pass\":\t\"h_pass2\",\n"
          "\t\"use_mqtt\":\ttrue,\n"
          "\t\"mqtt_use_default_prefix\":\tfalse,\n"
          "\t\"mqtt_transport\":\t\"SSL\",\n"
          "\t\"mqtt_server\":\t\"test.mosquitto.org\",\n"
          "\t\"mqtt_port\":\t1338,\n"
          "\t\"mqtt_prefix\":\t\"my_prefix\",\n"
          "\t\"mqtt_client_id\":\t\"my_client\",\n"
          "\t\"mqtt_user\":\t\"m_user1\",\n"
          "\t\"mqtt_pass\":\t\"m_pass1\",\n"
          "\t\"lan_auth_type\":\t\"lan_auth_digest\",\n"
          "\t\"lan_auth_user\":\t\"l_user1\",\n"
          "\t\"lan_auth_pass\":\t\"l_pass1\",\n"
          "\t\"lan_auth_api_key\":\t\"\",\n"
          "\t\"auto_update_cycle\":\t\"unknown\",\n"
          "\t\"auto_update_weekdays_bitmask\":\t63,\n"
          "\t\"auto_update_interval_from\":\t2,\n"
          "\t\"auto_update_interval_to\":\t22,\n"
          "\t\"auto_update_tz_offset_hours\":\t5,\n"
          "\t\"company_id\":\t1178,\n"
          "\t\"company_use_filtering\":\tfalse,\n"
          "\t\"scan_coded_phy\":\ttrue,\n"
          "\t\"scan_1mbit_phy\":\tfalse,\n"
          "\t\"scan_extended_payload\":\tfalse,\n"
          "\t\"scan_channel_37\":\tfalse,\n"
          "\t\"scan_channel_38\":\tfalse,\n"
          "\t\"scan_channel_39\":\tfalse,\n"
          "\t\"coordinates\":\t\"coordinates1\"\n"
          "}";

    ruuvi_gateway_config_t gw_cfg2 = {};
    ASSERT_TRUE(gw_cfg_json_parse(p_json_str, &gw_cfg2));
    TEST_CHECK_LOG_RECORD(ESP_LOG_WARN, string("Unknown auto_update_cycle='unknown', use REGULAR"));
    ASSERT_TRUE(esp_log_wrapper_is_empty());

    ASSERT_EQ(true, gw_cfg2.eth.use_eth);
    ASSERT_EQ(false, gw_cfg2.eth.eth_dhcp);
    ASSERT_EQ(string("192.168.1.10"), gw_cfg2.eth.eth_static_ip.buf);
    ASSERT_EQ(string("255.255.255.0"), gw_cfg2.eth.eth_netmask.buf);
    ASSERT_EQ(string("192.168.1.1"), gw_cfg2.eth.eth_gw.buf);
    ASSERT_EQ(string("8.8.8.8"), gw_cfg2.eth.eth_dns1.buf);
    ASSERT_EQ(string("4.4.4.4"), gw_cfg2.eth.eth_dns2.buf);

    ASSERT_EQ(true, gw_cfg2.mqtt.use_mqtt);
    ASSERT_EQ(false, gw_cfg2.mqtt.mqtt_use_default_prefix);
    ASSERT_EQ(string("SSL"), gw_cfg2.mqtt.mqtt_transport.buf);
    ASSERT_EQ(string("test.mosquitto.org"), gw_cfg2.mqtt.mqtt_server.buf);
    ASSERT_EQ(1338, gw_cfg2.mqtt.mqtt_port);
    ASSERT_EQ(string("my_prefix"), gw_cfg2.mqtt.mqtt_prefix.buf);
    ASSERT_EQ(string("my_client"), gw_cfg2.mqtt.mqtt_client_id.buf);
    ASSERT_EQ(string("m_user1"), gw_cfg2.mqtt.mqtt_user.buf);
    ASSERT_EQ(string("m_pass1"), gw_cfg2.mqtt.mqtt_pass.buf);

    ASSERT_EQ(false, gw_cfg2.http.use_http);
    ASSERT_EQ(string("https://myserver1.com"), gw_cfg2.http.http_url.buf);
    ASSERT_EQ(string("h_user1"), gw_cfg2.http.http_user.buf);
    ASSERT_EQ(string("h_pass1"), gw_cfg2.http.http_pass.buf);

    ASSERT_EQ(false, gw_cfg2.http_stat.use_http_stat);
    ASSERT_EQ(string("https://myserver1.com/status"), gw_cfg2.http_stat.http_stat_url.buf);
    ASSERT_EQ(string("h_user2"), gw_cfg2.http_stat.http_stat_user.buf);
    ASSERT_EQ(string("h_pass2"), gw_cfg2.http_stat.http_stat_pass.buf);

    ASSERT_EQ(string(HTTP_SERVER_AUTH_TYPE_STR_DIGEST), gw_cfg2.lan_auth.lan_auth_type);
    ASSERT_EQ(string("l_user1"), gw_cfg2.lan_auth.lan_auth_user);
    ASSERT_EQ(string("l_pass1"), gw_cfg2.lan_auth.lan_auth_pass);
    ASSERT_EQ(string(""), gw_cfg2.lan_auth.lan_auth_api_key);

    ASSERT_EQ(AUTO_UPDATE_CYCLE_TYPE_REGULAR, gw_cfg2.auto_update.auto_update_cycle);
    ASSERT_EQ(0x3F, gw_cfg2.auto_update.auto_update_weekdays_bitmask);
    ASSERT_EQ(2, gw_cfg2.auto_update.auto_update_interval_from);
    ASSERT_EQ(22, gw_cfg2.auto_update.auto_update_interval_to);
    ASSERT_EQ(5, gw_cfg2.auto_update.auto_update_tz_offset_hours);

    ASSERT_EQ(RUUVI_COMPANY_ID + 1, gw_cfg2.filter.company_id);
    ASSERT_EQ(false, gw_cfg2.filter.company_use_filtering);

    ASSERT_EQ(true, gw_cfg2.scan.scan_coded_phy);
    ASSERT_EQ(false, gw_cfg2.scan.scan_1mbit_phy);
    ASSERT_EQ(false, gw_cfg2.scan.scan_extended_payload);
    ASSERT_EQ(false, gw_cfg2.scan.scan_channel_37);
    ASSERT_EQ(false, gw_cfg2.scan.scan_channel_38);
    ASSERT_EQ(false, gw_cfg2.scan.scan_channel_39);

    ASSERT_EQ(string("coordinates1"), gw_cfg2.coordinates.buf);

    cjson_wrap_str_t json_str = cjson_wrap_str_null();
    ASSERT_TRUE(gw_cfg_json_generate(&gw_cfg2, &json_str));
    ASSERT_EQ(
        string("{\n"
               "\t\"use_eth\":\ttrue,\n"
               "\t\"eth_dhcp\":\tfalse,\n"
               "\t\"eth_static_ip\":\t\"192.168.1.10\",\n"
               "\t\"eth_netmask\":\t\"255.255.255.0\",\n"
               "\t\"eth_gw\":\t\"192.168.1.1\",\n"
               "\t\"eth_dns1\":\t\"8.8.8.8\",\n"
               "\t\"eth_dns2\":\t\"4.4.4.4\",\n"
               "\t\"use_http\":\tfalse,\n"
               "\t\"http_url\":\t\"https://myserver1.com\",\n"
               "\t\"http_user\":\t\"h_user1\",\n"
               "\t\"http_pass\":\t\"h_pass1\",\n"
               "\t\"use_http_stat\":\tfalse,\n"
               "\t\"http_stat_url\":\t\"https://myserver1.com/status\",\n"
               "\t\"http_stat_user\":\t\"h_user2\",\n"
               "\t\"http_stat_pass\":\t\"h_pass2\",\n"
               "\t\"use_mqtt\":\ttrue,\n"
               "\t\"mqtt_use_default_prefix\":\tfalse,\n"
               "\t\"mqtt_transport\":\t\"SSL\",\n"
               "\t\"mqtt_server\":\t\"test.mosquitto.org\",\n"
               "\t\"mqtt_port\":\t1338,\n"
               "\t\"mqtt_prefix\":\t\"my_prefix\",\n"
               "\t\"mqtt_client_id\":\t\"my_client\",\n"
               "\t\"mqtt_user\":\t\"m_user1\",\n"
               "\t\"mqtt_pass\":\t\"m_pass1\",\n"
               "\t\"lan_auth_type\":\t\"lan_auth_digest\",\n"
               "\t\"lan_auth_user\":\t\"l_user1\",\n"
               "\t\"lan_auth_pass\":\t\"l_pass1\",\n"
               "\t\"lan_auth_api_key\":\t\"\",\n"
               "\t\"auto_update_cycle\":\t\"regular\",\n"
               "\t\"auto_update_weekdays_bitmask\":\t63,\n"
               "\t\"auto_update_interval_from\":\t2,\n"
               "\t\"auto_update_interval_to\":\t22,\n"
               "\t\"auto_update_tz_offset_hours\":\t5,\n"
               "\t\"company_id\":\t1178,\n"
               "\t\"company_use_filtering\":\tfalse,\n"
               "\t\"scan_coded_phy\":\ttrue,\n"
               "\t\"scan_1mbit_phy\":\tfalse,\n"
               "\t\"scan_extended_payload\":\tfalse,\n"
               "\t\"scan_channel_37\":\tfalse,\n"
               "\t\"scan_channel_38\":\tfalse,\n"
               "\t\"scan_channel_39\":\tfalse,\n"
               "\t\"coordinates\":\t\"coordinates1\"\n"
               "}"),
        string(json_str.p_str));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    cjson_wrap_free_json_str(&json_str);
}

TEST_F(TestGwCfgJson, gw_cfg_json_parse_empty_json) // NOLINT
{
    cjson_wrap_str_t json_str = cjson_wrap_str_null();

    ruuvi_gateway_config_t gw_cfg2 = {};
    ASSERT_TRUE(gw_cfg_json_parse("{}", &gw_cfg2));

    ASSERT_EQ(false, gw_cfg2.eth.use_eth);
    ASSERT_EQ(true, gw_cfg2.eth.eth_dhcp);
    ASSERT_EQ(string(""), gw_cfg2.eth.eth_static_ip.buf);
    ASSERT_EQ(string(""), gw_cfg2.eth.eth_netmask.buf);
    ASSERT_EQ(string(""), gw_cfg2.eth.eth_gw.buf);
    ASSERT_EQ(string(""), gw_cfg2.eth.eth_dns1.buf);
    ASSERT_EQ(string(""), gw_cfg2.eth.eth_dns2.buf);

    ASSERT_EQ(false, gw_cfg2.mqtt.use_mqtt);
    ASSERT_EQ(true, gw_cfg2.mqtt.mqtt_use_default_prefix);
    ASSERT_EQ(string("TCP"), gw_cfg2.mqtt.mqtt_transport.buf);
    ASSERT_EQ(string(""), gw_cfg2.mqtt.mqtt_server.buf);
    ASSERT_EQ(0, gw_cfg2.mqtt.mqtt_port);
    ASSERT_EQ(string(""), gw_cfg2.mqtt.mqtt_prefix.buf);
    ASSERT_EQ(string(""), gw_cfg2.mqtt.mqtt_client_id.buf);
    ASSERT_EQ(string(""), gw_cfg2.mqtt.mqtt_user.buf);
    ASSERT_EQ(string(""), gw_cfg2.mqtt.mqtt_pass.buf);

    ASSERT_EQ(true, gw_cfg2.http.use_http);
    ASSERT_EQ(string(RUUVI_GATEWAY_HTTP_DEFAULT_URL), gw_cfg2.http.http_url.buf);
    ASSERT_EQ(string(""), gw_cfg2.http.http_user.buf);
    ASSERT_EQ(string(""), gw_cfg2.http.http_pass.buf);

    ASSERT_EQ(true, gw_cfg2.http_stat.use_http_stat);
    ASSERT_EQ(string(RUUVI_GATEWAY_HTTP_STATUS_URL), gw_cfg2.http_stat.http_stat_url.buf);
    ASSERT_EQ(string(""), gw_cfg2.http_stat.http_stat_user.buf);
    ASSERT_EQ(string(""), gw_cfg2.http_stat.http_stat_pass.buf);

    ASSERT_EQ(string(HTTP_SERVER_AUTH_TYPE_STR_RUUVI), gw_cfg2.lan_auth.lan_auth_type);
    ASSERT_EQ(string(RUUVI_GATEWAY_AUTH_DEFAULT_USER), gw_cfg2.lan_auth.lan_auth_user);
    ASSERT_EQ(string("\xFFpassword_md5\xFF"), gw_cfg2.lan_auth.lan_auth_pass);
    ASSERT_EQ(string(""), gw_cfg2.lan_auth.lan_auth_api_key);

    ASSERT_EQ(AUTO_UPDATE_CYCLE_TYPE_REGULAR, gw_cfg2.auto_update.auto_update_cycle);
    ASSERT_EQ(0x7F, gw_cfg2.auto_update.auto_update_weekdays_bitmask);
    ASSERT_EQ(0, gw_cfg2.auto_update.auto_update_interval_from);
    ASSERT_EQ(24, gw_cfg2.auto_update.auto_update_interval_to);
    ASSERT_EQ(3, gw_cfg2.auto_update.auto_update_tz_offset_hours);

    ASSERT_EQ(RUUVI_COMPANY_ID, gw_cfg2.filter.company_id);
    ASSERT_EQ(true, gw_cfg2.filter.company_use_filtering);

    ASSERT_EQ(false, gw_cfg2.scan.scan_coded_phy);
    ASSERT_EQ(true, gw_cfg2.scan.scan_1mbit_phy);
    ASSERT_EQ(true, gw_cfg2.scan.scan_extended_payload);
    ASSERT_EQ(true, gw_cfg2.scan.scan_channel_37);
    ASSERT_EQ(true, gw_cfg2.scan.scan_channel_38);
    ASSERT_EQ(true, gw_cfg2.scan.scan_channel_39);

    ASSERT_EQ(string(""), gw_cfg2.coordinates.buf);

    ruuvi_gateway_config_t gateway_config_default {};
    gw_cfg_default_get(&gateway_config_default);
    ASSERT_TRUE(0 == memcmp(&gateway_config_default, &gw_cfg2, sizeof(gateway_config_default)));

    ASSERT_TRUE(gw_cfg_json_generate(&gw_cfg2, &json_str));
    ASSERT_EQ(
        string("{\n"
               "\t\"use_eth\":\tfalse,\n"
               "\t\"eth_dhcp\":\ttrue,\n"
               "\t\"eth_static_ip\":\t\"\",\n"
               "\t\"eth_netmask\":\t\"\",\n"
               "\t\"eth_gw\":\t\"\",\n"
               "\t\"eth_dns1\":\t\"\",\n"
               "\t\"eth_dns2\":\t\"\",\n"
               "\t\"use_http\":\ttrue,\n"
               "\t\"http_url\":\t\"" RUUVI_GATEWAY_HTTP_DEFAULT_URL "\",\n"
               "\t\"http_user\":\t\"\",\n"
               "\t\"http_pass\":\t\"\",\n"
               "\t\"use_http_stat\":\ttrue,\n"
               "\t\"http_stat_url\":\t\"" RUUVI_GATEWAY_HTTP_STATUS_URL "\",\n"
               "\t\"http_stat_user\":\t\"\",\n"
               "\t\"http_stat_pass\":\t\"\",\n"
               "\t\"use_mqtt\":\tfalse,\n"
               "\t\"mqtt_use_default_prefix\":\ttrue,\n"
               "\t\"mqtt_transport\":\t\"TCP\",\n"
               "\t\"mqtt_server\":\t\"\",\n"
               "\t\"mqtt_port\":\t0,\n"
               "\t\"mqtt_prefix\":\t\"\",\n"
               "\t\"mqtt_client_id\":\t\"\",\n"
               "\t\"mqtt_user\":\t\"\",\n"
               "\t\"mqtt_pass\":\t\"\",\n"
               "\t\"lan_auth_type\":\t\"lan_auth_ruuvi\",\n"
               "\t\"lan_auth_user\":\t\"Admin\",\n"
               "\t\"lan_auth_pass\":\t\"\xFFpassword_md5\xFF\",\n"
               "\t\"lan_auth_api_key\":\t\"\",\n"
               "\t\"auto_update_cycle\":\t\"regular\",\n"
               "\t\"auto_update_weekdays_bitmask\":\t127,\n"
               "\t\"auto_update_interval_from\":\t0,\n"
               "\t\"auto_update_interval_to\":\t24,\n"
               "\t\"auto_update_tz_offset_hours\":\t3,\n"
               "\t\"company_id\":\t1177,\n"
               "\t\"company_use_filtering\":\ttrue,\n"
               "\t\"scan_coded_phy\":\tfalse,\n"
               "\t\"scan_1mbit_phy\":\ttrue,\n"
               "\t\"scan_extended_payload\":\ttrue,\n"
               "\t\"scan_channel_37\":\ttrue,\n"
               "\t\"scan_channel_38\":\ttrue,\n"
               "\t\"scan_channel_39\":\ttrue,\n"
               "\t\"coordinates\":\t\"\"\n"
               "}"),
        string(json_str.p_str));
    TEST_CHECK_LOG_RECORD(ESP_LOG_WARN, string("Can't find key 'use_eth' in config-json"));
    TEST_CHECK_LOG_RECORD(ESP_LOG_WARN, string("Can't find key 'eth_dhcp' in config-json"));
    TEST_CHECK_LOG_RECORD(ESP_LOG_WARN, string("Can't find key 'eth_static_ip' in config-json"));
    TEST_CHECK_LOG_RECORD(ESP_LOG_WARN, string("Can't find key 'eth_netmask' in config-json"));
    TEST_CHECK_LOG_RECORD(ESP_LOG_WARN, string("Can't find key 'eth_gw' in config-json"));
    TEST_CHECK_LOG_RECORD(ESP_LOG_WARN, string("Can't find key 'eth_dns1' in config-json"));
    TEST_CHECK_LOG_RECORD(ESP_LOG_WARN, string("Can't find key 'eth_dns2' in config-json"));
    TEST_CHECK_LOG_RECORD(ESP_LOG_WARN, string("Can't find key 'use_mqtt' in config-json"));
    TEST_CHECK_LOG_RECORD(ESP_LOG_WARN, string("Can't find key 'mqtt_use_default_prefix' in config-json"));
    TEST_CHECK_LOG_RECORD(ESP_LOG_WARN, string("Can't find key 'mqtt_transport' in config-json"));
    TEST_CHECK_LOG_RECORD(ESP_LOG_WARN, string("Can't find key 'mqtt_server' in config-json"));
    TEST_CHECK_LOG_RECORD(ESP_LOG_WARN, string("Can't find key 'mqtt_port' in config-json"));
    TEST_CHECK_LOG_RECORD(ESP_LOG_WARN, string("Can't find key 'mqtt_prefix' in config-json"));
    TEST_CHECK_LOG_RECORD(ESP_LOG_WARN, string("Can't find key 'mqtt_client_id' in config-json"));
    TEST_CHECK_LOG_RECORD(ESP_LOG_WARN, string("Can't find key 'mqtt_user' in config-json"));
    TEST_CHECK_LOG_RECORD(ESP_LOG_WARN, string("Can't find key 'mqtt_pass' in config-json"));
    TEST_CHECK_LOG_RECORD(ESP_LOG_WARN, string("Can't find key 'use_http' in config-json"));
    TEST_CHECK_LOG_RECORD(ESP_LOG_WARN, string("Can't find key 'http_url' in config-json"));
    TEST_CHECK_LOG_RECORD(ESP_LOG_WARN, string("Can't find key 'http_user' in config-json"));
    TEST_CHECK_LOG_RECORD(ESP_LOG_WARN, string("Can't find key 'http_pass' in config-json"));
    TEST_CHECK_LOG_RECORD(ESP_LOG_WARN, string("Can't find key 'use_http_stat' in config-json"));
    TEST_CHECK_LOG_RECORD(ESP_LOG_WARN, string("Can't find key 'http_stat_url' in config-json"));
    TEST_CHECK_LOG_RECORD(ESP_LOG_WARN, string("Can't find key 'http_stat_user' in config-json"));
    TEST_CHECK_LOG_RECORD(ESP_LOG_WARN, string("Can't find key 'http_stat_pass' in config-json"));
    TEST_CHECK_LOG_RECORD(ESP_LOG_WARN, string("Can't find key 'lan_auth_type' in config-json"));
    TEST_CHECK_LOG_RECORD(ESP_LOG_WARN, string("Can't find key 'lan_auth_user' in config-json"));
    TEST_CHECK_LOG_RECORD(ESP_LOG_WARN, string("Can't find key 'lan_auth_pass' in config-json"));
    TEST_CHECK_LOG_RECORD(ESP_LOG_WARN, string("Can't find key 'lan_auth_api_key' in config-json"));
    TEST_CHECK_LOG_RECORD(ESP_LOG_WARN, string("Can't find key 'auto_update_cycle' in config-json"));
    TEST_CHECK_LOG_RECORD(ESP_LOG_WARN, string("Can't find key 'auto_update_weekdays_bitmask' in config-json"));
    TEST_CHECK_LOG_RECORD(ESP_LOG_WARN, string("Can't find key 'auto_update_interval_from' in config-json"));
    TEST_CHECK_LOG_RECORD(ESP_LOG_WARN, string("Can't find key 'auto_update_interval_to' in config-json"));
    TEST_CHECK_LOG_RECORD(ESP_LOG_WARN, string("Can't find key 'auto_update_tz_offset_hours' in config-json"));
    TEST_CHECK_LOG_RECORD(ESP_LOG_WARN, string("Can't find key 'company_id' in config-json"));
    TEST_CHECK_LOG_RECORD(ESP_LOG_WARN, string("Can't find key 'company_use_filtering' in config-json"));
    TEST_CHECK_LOG_RECORD(ESP_LOG_WARN, string("Can't find key 'scan_coded_phy' in config-json"));
    TEST_CHECK_LOG_RECORD(ESP_LOG_WARN, string("Can't find key 'scan_1mbit_phy' in config-json"));
    TEST_CHECK_LOG_RECORD(ESP_LOG_WARN, string("Can't find key 'scan_extended_payload' in config-json"));
    TEST_CHECK_LOG_RECORD(ESP_LOG_WARN, string("Can't find key 'scan_channel_37' in config-json"));
    TEST_CHECK_LOG_RECORD(ESP_LOG_WARN, string("Can't find key 'scan_channel_38' in config-json"));
    TEST_CHECK_LOG_RECORD(ESP_LOG_WARN, string("Can't find key 'scan_channel_39' in config-json"));
    TEST_CHECK_LOG_RECORD(ESP_LOG_WARN, string("Can't find key 'coordinates' in config-json"));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    cjson_wrap_free_json_str(&json_str);
}

TEST_F(TestGwCfgJson, gw_cfg_json_parse_empty_string) // NOLINT
{
    cjson_wrap_str_t json_str = cjson_wrap_str_null();

    ruuvi_gateway_config_t gw_cfg2 = {};
    ASSERT_TRUE(gw_cfg_json_parse("", &gw_cfg2));

    ASSERT_EQ(false, gw_cfg2.eth.use_eth);
    ASSERT_EQ(true, gw_cfg2.eth.eth_dhcp);
    ASSERT_EQ(string(""), gw_cfg2.eth.eth_static_ip.buf);
    ASSERT_EQ(string(""), gw_cfg2.eth.eth_netmask.buf);
    ASSERT_EQ(string(""), gw_cfg2.eth.eth_gw.buf);
    ASSERT_EQ(string(""), gw_cfg2.eth.eth_dns1.buf);
    ASSERT_EQ(string(""), gw_cfg2.eth.eth_dns2.buf);

    ASSERT_EQ(false, gw_cfg2.mqtt.use_mqtt);
    ASSERT_EQ(true, gw_cfg2.mqtt.mqtt_use_default_prefix);
    ASSERT_EQ(string("TCP"), gw_cfg2.mqtt.mqtt_transport.buf);
    ASSERT_EQ(string(""), gw_cfg2.mqtt.mqtt_server.buf);
    ASSERT_EQ(0, gw_cfg2.mqtt.mqtt_port);
    ASSERT_EQ(string(""), gw_cfg2.mqtt.mqtt_prefix.buf);
    ASSERT_EQ(string(""), gw_cfg2.mqtt.mqtt_client_id.buf);
    ASSERT_EQ(string(""), gw_cfg2.mqtt.mqtt_user.buf);
    ASSERT_EQ(string(""), gw_cfg2.mqtt.mqtt_pass.buf);

    ASSERT_EQ(true, gw_cfg2.http.use_http);
    ASSERT_EQ(string(RUUVI_GATEWAY_HTTP_DEFAULT_URL), gw_cfg2.http.http_url.buf);
    ASSERT_EQ(string(""), gw_cfg2.http.http_user.buf);
    ASSERT_EQ(string(""), gw_cfg2.http.http_pass.buf);

    ASSERT_EQ(true, gw_cfg2.http_stat.use_http_stat);
    ASSERT_EQ(string(RUUVI_GATEWAY_HTTP_STATUS_URL), gw_cfg2.http_stat.http_stat_url.buf);
    ASSERT_EQ(string(""), gw_cfg2.http_stat.http_stat_user.buf);
    ASSERT_EQ(string(""), gw_cfg2.http_stat.http_stat_pass.buf);

    ASSERT_EQ(string(HTTP_SERVER_AUTH_TYPE_STR_RUUVI), gw_cfg2.lan_auth.lan_auth_type);
    ASSERT_EQ(string(RUUVI_GATEWAY_AUTH_DEFAULT_USER), gw_cfg2.lan_auth.lan_auth_user);
    ASSERT_EQ(string("\xFFpassword_md5\xFF"), gw_cfg2.lan_auth.lan_auth_pass);
    ASSERT_EQ(string(""), gw_cfg2.lan_auth.lan_auth_api_key);

    ASSERT_EQ(AUTO_UPDATE_CYCLE_TYPE_REGULAR, gw_cfg2.auto_update.auto_update_cycle);
    ASSERT_EQ(0x7F, gw_cfg2.auto_update.auto_update_weekdays_bitmask);
    ASSERT_EQ(0, gw_cfg2.auto_update.auto_update_interval_from);
    ASSERT_EQ(24, gw_cfg2.auto_update.auto_update_interval_to);
    ASSERT_EQ(3, gw_cfg2.auto_update.auto_update_tz_offset_hours);

    ASSERT_EQ(RUUVI_COMPANY_ID, gw_cfg2.filter.company_id);
    ASSERT_EQ(true, gw_cfg2.filter.company_use_filtering);

    ASSERT_EQ(false, gw_cfg2.scan.scan_coded_phy);
    ASSERT_EQ(true, gw_cfg2.scan.scan_1mbit_phy);
    ASSERT_EQ(true, gw_cfg2.scan.scan_extended_payload);
    ASSERT_EQ(true, gw_cfg2.scan.scan_channel_37);
    ASSERT_EQ(true, gw_cfg2.scan.scan_channel_38);
    ASSERT_EQ(true, gw_cfg2.scan.scan_channel_39);

    ASSERT_EQ(string(""), gw_cfg2.coordinates.buf);

    ruuvi_gateway_config_t gateway_config_default {};
    gw_cfg_default_get(&gateway_config_default);
    ASSERT_TRUE(0 == memcmp(&gateway_config_default, &gw_cfg2, sizeof(gateway_config_default)));

    ASSERT_TRUE(gw_cfg_json_generate(&gw_cfg2, &json_str));
    ASSERT_EQ(
        string("{\n"
               "\t\"use_eth\":\tfalse,\n"
               "\t\"eth_dhcp\":\ttrue,\n"
               "\t\"eth_static_ip\":\t\"\",\n"
               "\t\"eth_netmask\":\t\"\",\n"
               "\t\"eth_gw\":\t\"\",\n"
               "\t\"eth_dns1\":\t\"\",\n"
               "\t\"eth_dns2\":\t\"\",\n"
               "\t\"use_http\":\ttrue,\n"
               "\t\"http_url\":\t\"" RUUVI_GATEWAY_HTTP_DEFAULT_URL "\",\n"
               "\t\"http_user\":\t\"\",\n"
               "\t\"http_pass\":\t\"\",\n"
               "\t\"use_http_stat\":\ttrue,\n"
               "\t\"http_stat_url\":\t\"" RUUVI_GATEWAY_HTTP_STATUS_URL "\",\n"
               "\t\"http_stat_user\":\t\"\",\n"
               "\t\"http_stat_pass\":\t\"\",\n"
               "\t\"use_mqtt\":\tfalse,\n"
               "\t\"mqtt_use_default_prefix\":\ttrue,\n"
               "\t\"mqtt_transport\":\t\"TCP\",\n"
               "\t\"mqtt_server\":\t\"\",\n"
               "\t\"mqtt_port\":\t0,\n"
               "\t\"mqtt_prefix\":\t\"\",\n"
               "\t\"mqtt_client_id\":\t\"\",\n"
               "\t\"mqtt_user\":\t\"\",\n"
               "\t\"mqtt_pass\":\t\"\",\n"
               "\t\"lan_auth_type\":\t\"lan_auth_ruuvi\",\n"
               "\t\"lan_auth_user\":\t\"Admin\",\n"
               "\t\"lan_auth_pass\":\t\"\xFFpassword_md5\xFF\",\n"
               "\t\"lan_auth_api_key\":\t\"\",\n"
               "\t\"auto_update_cycle\":\t\"regular\",\n"
               "\t\"auto_update_weekdays_bitmask\":\t127,\n"
               "\t\"auto_update_interval_from\":\t0,\n"
               "\t\"auto_update_interval_to\":\t24,\n"
               "\t\"auto_update_tz_offset_hours\":\t3,\n"
               "\t\"company_id\":\t1177,\n"
               "\t\"company_use_filtering\":\ttrue,\n"
               "\t\"scan_coded_phy\":\tfalse,\n"
               "\t\"scan_1mbit_phy\":\ttrue,\n"
               "\t\"scan_extended_payload\":\ttrue,\n"
               "\t\"scan_channel_37\":\ttrue,\n"
               "\t\"scan_channel_38\":\ttrue,\n"
               "\t\"scan_channel_39\":\ttrue,\n"
               "\t\"coordinates\":\t\"\"\n"
               "}"),
        string(json_str.p_str));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    cjson_wrap_free_json_str(&json_str);
}

TEST_F(TestGwCfgJson, gw_cfg_json_parse_malloc_failed) // NOLINT
{
    cJSON_Hooks hooks = {
        .malloc_fn = &os_malloc,
        .free_fn   = &os_free_internal,
    };
    cJSON_InitHooks(&hooks);

    const char *const p_json_str
        = "{\n"
          "\t\"use_eth\":\tfalse,\n"
          "\t\"eth_dhcp\":\ttrue,\n"
          "\t\"eth_static_ip\":\t\"\",\n"
          "\t\"eth_netmask\":\t\"\","
          "\n\t\"eth_gw\":\t\"\",\n"
          "\t\"eth_dns1\":\t\"\",\n"
          "\t\"eth_dns2\":\t\"\",\n"
          "\t\"use_http\":\ttrue,\n"
          "\t\"http_url\":\t\"https://network.ruuvi.com/record\",\n"
          "\t\"http_user\":\t\"\",\n"
          "\t\"http_pass\":\t\"\",\n"
          "\t\"use_http_stat\":\ttrue,\n"
          "\t\"http_stat_url\":\t\"https://network.ruuvi.com/status\",\n"
          "\t\"http_stat_user\":\t\"\",\n"
          "\t\"http_stat_pass\":\t\"\",\n"
          "\t\"use_mqtt\":\tfalse,\n"
          "\t\"mqtt_use_default_prefix\":\ttrue,\n"
          "\t\"mqtt_transport\":\t\"TCP\",\n"
          "\t\"mqtt_server\":\t\"\",\n"
          "\t\"mqtt_port\":\t0,\n"
          "\t\"mqtt_prefix\":\t\"\",\n"
          "\t\"mqtt_client_id\":\t\"\",\n"
          "\t\"mqtt_user\":\t\"\",\n"
          "\t\"mqtt_pass\":\t\"\",\n"
          "\t\"lan_auth_type\":\t\"lan_auth_ruuvi\",\n"
          "\t\"lan_auth_user\":\t\"Admin\",\n"
          "\t\"lan_auth_pass\":\t\"\377password_md5\377\",\n"
          "\t\"lan_auth_api_key\":\t\"\",\n"
          "\t\"auto_update_cycle\":\t\"regular\",\n"
          "\t\"auto_update_weekdays_bitmask\":\t127,\n"
          "\t\"auto_update_interval_from\":\t0,\n"
          "\t\"auto_update_interval_to\":\t24,\n"
          "\t\"auto_update_tz_offset_hours\":\t3,\n"
          "\t\"company_id\":\t1177,\n"
          "\t\"company_use_filtering\":\ttrue,\n"
          "\t\"scan_coded_phy\":\tfalse,\n"
          "\t\"scan_1mbit_phy\":\ttrue,\n"
          "\t\"scan_extended_payload\":\ttrue,\n"
          "\t\"scan_channel_37\":\ttrue,\n"
          "\t\"scan_channel_38\":\ttrue,\n"
          "\t\"scan_channel_39\":\ttrue,\n"
          "\t\"coordinates\":\t\"\"\n"
          "}";
    for (uint32_t i = 0; i < 108; ++i)
    {
        this->m_malloc_cnt             = 0;
        this->m_malloc_fail_on_cnt     = i + 1;
        ruuvi_gateway_config_t gw_cfg2 = {};
        if (gw_cfg_json_parse(p_json_str, &gw_cfg2))
        {
            ASSERT_FALSE(true);
        }
        ASSERT_TRUE(esp_log_wrapper_is_empty());
        ASSERT_TRUE(g_pTestClass->m_mem_alloc_trace.is_empty());
    }

    {
        this->m_malloc_cnt             = 0;
        this->m_malloc_fail_on_cnt     = 109;
        ruuvi_gateway_config_t gw_cfg2 = {};
        ASSERT_TRUE(gw_cfg_json_parse(p_json_str, &gw_cfg2));
        ASSERT_TRUE(esp_log_wrapper_is_empty());
        ASSERT_TRUE(g_pTestClass->m_mem_alloc_trace.is_empty());
    }
}

TEST_F(TestGwCfgJson, gw_cfg_json_generate_malloc_failed_on_json_creation) // NOLINT
{
    const ruuvi_gateway_config_t gw_cfg   = get_gateway_config_default();
    cjson_wrap_str_t             json_str = cjson_wrap_str_null();

    cJSON_Hooks hooks = {
        .malloc_fn = &os_malloc,
        .free_fn   = &os_free_internal,
    };
    cJSON_InitHooks(&hooks);
    this->m_malloc_fail_on_cnt = 1;

    ASSERT_FALSE(gw_cfg_json_generate(&gw_cfg, &json_str));
    ASSERT_EQ(nullptr, json_str.p_str);
    TEST_CHECK_LOG_RECORD(ESP_LOG_ERROR, string("Can't create json object"));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_TRUE(g_pTestClass->m_mem_alloc_trace.is_empty());
}

TEST_F(TestGwCfgJson, gw_cfg_json_generate_malloc_failed_on_use_eth) // NOLINT
{
    const ruuvi_gateway_config_t gw_cfg   = get_gateway_config_default();
    cjson_wrap_str_t             json_str = cjson_wrap_str_null();

    cJSON_Hooks hooks = {
        .malloc_fn = &os_malloc,
        .free_fn   = &os_free_internal,
    };
    cJSON_InitHooks(&hooks);
    this->m_malloc_fail_on_cnt = 2;

    ASSERT_FALSE(gw_cfg_json_generate(&gw_cfg, &json_str));
    ASSERT_EQ(nullptr, json_str.p_str);
    TEST_CHECK_LOG_RECORD(ESP_LOG_ERROR, string("Can't add json item: use_eth"));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_TRUE(g_pTestClass->m_mem_alloc_trace.is_empty());
}

TEST_F(TestGwCfgJson, gw_cfg_json_generate_malloc_failed_on_use_eth_2) // NOLINT
{
    const ruuvi_gateway_config_t gw_cfg   = get_gateway_config_default();
    cjson_wrap_str_t             json_str = cjson_wrap_str_null();

    cJSON_Hooks hooks = {
        .malloc_fn = &os_malloc,
        .free_fn   = &os_free_internal,
    };
    cJSON_InitHooks(&hooks);
    this->m_malloc_fail_on_cnt = 3;

    ASSERT_FALSE(gw_cfg_json_generate(&gw_cfg, &json_str));
    ASSERT_EQ(nullptr, json_str.p_str);
    TEST_CHECK_LOG_RECORD(ESP_LOG_ERROR, string("Can't add json item: use_eth"));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_TRUE(g_pTestClass->m_mem_alloc_trace.is_empty());
}

TEST_F(TestGwCfgJson, gw_cfg_json_generate_malloc_failed_on_eth_dhcp) // NOLINT
{
    const ruuvi_gateway_config_t gw_cfg   = get_gateway_config_default();
    cjson_wrap_str_t             json_str = cjson_wrap_str_null();

    cJSON_Hooks hooks = {
        .malloc_fn = &os_malloc,
        .free_fn   = &os_free_internal,
    };
    cJSON_InitHooks(&hooks);
    this->m_malloc_fail_on_cnt = 4;

    ASSERT_FALSE(gw_cfg_json_generate(&gw_cfg, &json_str));
    ASSERT_EQ(nullptr, json_str.p_str);
    TEST_CHECK_LOG_RECORD(ESP_LOG_ERROR, string("Can't add json item: eth_dhcp"));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_TRUE(g_pTestClass->m_mem_alloc_trace.is_empty());
}

TEST_F(TestGwCfgJson, gw_cfg_json_generate_malloc_failed_on_eth_dhcp_2) // NOLINT
{
    const ruuvi_gateway_config_t gw_cfg   = get_gateway_config_default();
    cjson_wrap_str_t             json_str = cjson_wrap_str_null();

    cJSON_Hooks hooks = {
        .malloc_fn = &os_malloc,
        .free_fn   = &os_free_internal,
    };
    cJSON_InitHooks(&hooks);
    this->m_malloc_fail_on_cnt = 5;

    ASSERT_FALSE(gw_cfg_json_generate(&gw_cfg, &json_str));
    ASSERT_EQ(nullptr, json_str.p_str);
    TEST_CHECK_LOG_RECORD(ESP_LOG_ERROR, string("Can't add json item: eth_dhcp"));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_TRUE(g_pTestClass->m_mem_alloc_trace.is_empty());
}

TEST_F(TestGwCfgJson, gw_cfg_json_generate_malloc_failed_on_eth_static_ip) // NOLINT
{
    const ruuvi_gateway_config_t gw_cfg   = get_gateway_config_default();
    cjson_wrap_str_t             json_str = cjson_wrap_str_null();

    cJSON_Hooks hooks = {
        .malloc_fn = &os_malloc,
        .free_fn   = &os_free_internal,
    };
    cJSON_InitHooks(&hooks);
    this->m_malloc_fail_on_cnt = 6;

    ASSERT_FALSE(gw_cfg_json_generate(&gw_cfg, &json_str));
    ASSERT_EQ(nullptr, json_str.p_str);
    TEST_CHECK_LOG_RECORD(ESP_LOG_ERROR, string("Can't add json item: eth_static_ip"));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_TRUE(g_pTestClass->m_mem_alloc_trace.is_empty());
}

TEST_F(TestGwCfgJson, gw_cfg_json_generate_malloc_failed_on_eth_static_ip_2) // NOLINT
{
    const ruuvi_gateway_config_t gw_cfg   = get_gateway_config_default();
    cjson_wrap_str_t             json_str = cjson_wrap_str_null();

    cJSON_Hooks hooks = {
        .malloc_fn = &os_malloc,
        .free_fn   = &os_free_internal,
    };
    cJSON_InitHooks(&hooks);
    this->m_malloc_fail_on_cnt = 7;

    ASSERT_FALSE(gw_cfg_json_generate(&gw_cfg, &json_str));
    ASSERT_EQ(nullptr, json_str.p_str);
    TEST_CHECK_LOG_RECORD(ESP_LOG_ERROR, string("Can't add json item: eth_static_ip"));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_TRUE(g_pTestClass->m_mem_alloc_trace.is_empty());
}

TEST_F(TestGwCfgJson, gw_cfg_json_generate_malloc_failed_on_eth_static_ip_3) // NOLINT
{
    const ruuvi_gateway_config_t gw_cfg   = get_gateway_config_default();
    cjson_wrap_str_t             json_str = cjson_wrap_str_null();

    cJSON_Hooks hooks = {
        .malloc_fn = &os_malloc,
        .free_fn   = &os_free_internal,
    };
    cJSON_InitHooks(&hooks);
    this->m_malloc_fail_on_cnt = 8;

    ASSERT_FALSE(gw_cfg_json_generate(&gw_cfg, &json_str));
    ASSERT_EQ(nullptr, json_str.p_str);
    TEST_CHECK_LOG_RECORD(ESP_LOG_ERROR, string("Can't add json item: eth_static_ip"));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_TRUE(g_pTestClass->m_mem_alloc_trace.is_empty());
}

TEST_F(TestGwCfgJson, gw_cfg_json_generate_malloc_failed_on_eth_netmask) // NOLINT
{
    const ruuvi_gateway_config_t gw_cfg   = get_gateway_config_default();
    cjson_wrap_str_t             json_str = cjson_wrap_str_null();

    cJSON_Hooks hooks = {
        .malloc_fn = &os_malloc,
        .free_fn   = &os_free_internal,
    };
    cJSON_InitHooks(&hooks);
    this->m_malloc_fail_on_cnt = 9;

    ASSERT_FALSE(gw_cfg_json_generate(&gw_cfg, &json_str));
    ASSERT_EQ(nullptr, json_str.p_str);
    TEST_CHECK_LOG_RECORD(ESP_LOG_ERROR, string("Can't add json item: eth_netmask"));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_TRUE(g_pTestClass->m_mem_alloc_trace.is_empty());
}

TEST_F(TestGwCfgJson, gw_cfg_json_generate_malloc_failed_on_eth_netmask_2) // NOLINT
{
    const ruuvi_gateway_config_t gw_cfg   = get_gateway_config_default();
    cjson_wrap_str_t             json_str = cjson_wrap_str_null();

    cJSON_Hooks hooks = {
        .malloc_fn = &os_malloc,
        .free_fn   = &os_free_internal,
    };
    cJSON_InitHooks(&hooks);
    this->m_malloc_fail_on_cnt = 10;

    ASSERT_FALSE(gw_cfg_json_generate(&gw_cfg, &json_str));
    ASSERT_EQ(nullptr, json_str.p_str);
    TEST_CHECK_LOG_RECORD(ESP_LOG_ERROR, string("Can't add json item: eth_netmask"));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_TRUE(g_pTestClass->m_mem_alloc_trace.is_empty());
}

TEST_F(TestGwCfgJson, gw_cfg_json_generate_malloc_failed_on_eth_netmask_3) // NOLINT
{
    const ruuvi_gateway_config_t gw_cfg   = get_gateway_config_default();
    cjson_wrap_str_t             json_str = cjson_wrap_str_null();

    cJSON_Hooks hooks = {
        .malloc_fn = &os_malloc,
        .free_fn   = &os_free_internal,
    };
    cJSON_InitHooks(&hooks);
    this->m_malloc_fail_on_cnt = 11;

    ASSERT_FALSE(gw_cfg_json_generate(&gw_cfg, &json_str));
    ASSERT_EQ(nullptr, json_str.p_str);
    TEST_CHECK_LOG_RECORD(ESP_LOG_ERROR, string("Can't add json item: eth_netmask"));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_TRUE(g_pTestClass->m_mem_alloc_trace.is_empty());
}

TEST_F(TestGwCfgJson, gw_cfg_json_generate_malloc_failed_on_eth_gw) // NOLINT
{
    const ruuvi_gateway_config_t gw_cfg   = get_gateway_config_default();
    cjson_wrap_str_t             json_str = cjson_wrap_str_null();

    cJSON_Hooks hooks = {
        .malloc_fn = &os_malloc,
        .free_fn   = &os_free_internal,
    };
    cJSON_InitHooks(&hooks);
    this->m_malloc_fail_on_cnt = 12;

    ASSERT_FALSE(gw_cfg_json_generate(&gw_cfg, &json_str));
    ASSERT_EQ(nullptr, json_str.p_str);
    TEST_CHECK_LOG_RECORD(ESP_LOG_ERROR, string("Can't add json item: eth_gw"));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_TRUE(g_pTestClass->m_mem_alloc_trace.is_empty());
}

TEST_F(TestGwCfgJson, gw_cfg_json_generate_malloc_failed_on_eth_gw_2) // NOLINT
{
    const ruuvi_gateway_config_t gw_cfg   = get_gateway_config_default();
    cjson_wrap_str_t             json_str = cjson_wrap_str_null();

    cJSON_Hooks hooks = {
        .malloc_fn = &os_malloc,
        .free_fn   = &os_free_internal,
    };
    cJSON_InitHooks(&hooks);
    this->m_malloc_fail_on_cnt = 13;

    ASSERT_FALSE(gw_cfg_json_generate(&gw_cfg, &json_str));
    ASSERT_EQ(nullptr, json_str.p_str);
    TEST_CHECK_LOG_RECORD(ESP_LOG_ERROR, string("Can't add json item: eth_gw"));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_TRUE(g_pTestClass->m_mem_alloc_trace.is_empty());
}

TEST_F(TestGwCfgJson, gw_cfg_json_generate_malloc_failed_on_eth_gw_3) // NOLINT
{
    const ruuvi_gateway_config_t gw_cfg   = get_gateway_config_default();
    cjson_wrap_str_t             json_str = cjson_wrap_str_null();

    cJSON_Hooks hooks = {
        .malloc_fn = &os_malloc,
        .free_fn   = &os_free_internal,
    };
    cJSON_InitHooks(&hooks);
    this->m_malloc_fail_on_cnt = 14;

    ASSERT_FALSE(gw_cfg_json_generate(&gw_cfg, &json_str));
    ASSERT_EQ(nullptr, json_str.p_str);
    TEST_CHECK_LOG_RECORD(ESP_LOG_ERROR, string("Can't add json item: eth_gw"));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_TRUE(g_pTestClass->m_mem_alloc_trace.is_empty());
}

TEST_F(TestGwCfgJson, gw_cfg_json_generate_malloc_failed_on_eth_dns1) // NOLINT
{
    const ruuvi_gateway_config_t gw_cfg   = get_gateway_config_default();
    cjson_wrap_str_t             json_str = cjson_wrap_str_null();

    cJSON_Hooks hooks = {
        .malloc_fn = &os_malloc,
        .free_fn   = &os_free_internal,
    };
    cJSON_InitHooks(&hooks);
    this->m_malloc_fail_on_cnt = 15;

    ASSERT_FALSE(gw_cfg_json_generate(&gw_cfg, &json_str));
    ASSERT_EQ(nullptr, json_str.p_str);
    TEST_CHECK_LOG_RECORD(ESP_LOG_ERROR, string("Can't add json item: eth_dns1"));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_TRUE(g_pTestClass->m_mem_alloc_trace.is_empty());
}

TEST_F(TestGwCfgJson, gw_cfg_json_generate_malloc_failed_on_eth_dns1_2) // NOLINT
{
    const ruuvi_gateway_config_t gw_cfg   = get_gateway_config_default();
    cjson_wrap_str_t             json_str = cjson_wrap_str_null();

    cJSON_Hooks hooks = {
        .malloc_fn = &os_malloc,
        .free_fn   = &os_free_internal,
    };
    cJSON_InitHooks(&hooks);
    this->m_malloc_fail_on_cnt = 16;

    ASSERT_FALSE(gw_cfg_json_generate(&gw_cfg, &json_str));
    ASSERT_EQ(nullptr, json_str.p_str);
    TEST_CHECK_LOG_RECORD(ESP_LOG_ERROR, string("Can't add json item: eth_dns1"));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_TRUE(g_pTestClass->m_mem_alloc_trace.is_empty());
}

TEST_F(TestGwCfgJson, gw_cfg_json_generate_malloc_failed_on_eth_dns1_3) // NOLINT
{
    const ruuvi_gateway_config_t gw_cfg   = get_gateway_config_default();
    cjson_wrap_str_t             json_str = cjson_wrap_str_null();

    cJSON_Hooks hooks = {
        .malloc_fn = &os_malloc,
        .free_fn   = &os_free_internal,
    };
    cJSON_InitHooks(&hooks);
    this->m_malloc_fail_on_cnt = 17;

    ASSERT_FALSE(gw_cfg_json_generate(&gw_cfg, &json_str));
    ASSERT_EQ(nullptr, json_str.p_str);
    TEST_CHECK_LOG_RECORD(ESP_LOG_ERROR, string("Can't add json item: eth_dns1"));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_TRUE(g_pTestClass->m_mem_alloc_trace.is_empty());
}

TEST_F(TestGwCfgJson, gw_cfg_json_generate_malloc_failed_on_eth_dns2) // NOLINT
{
    const ruuvi_gateway_config_t gw_cfg   = get_gateway_config_default();
    cjson_wrap_str_t             json_str = cjson_wrap_str_null();

    cJSON_Hooks hooks = {
        .malloc_fn = &os_malloc,
        .free_fn   = &os_free_internal,
    };
    cJSON_InitHooks(&hooks);
    this->m_malloc_fail_on_cnt = 18;

    ASSERT_FALSE(gw_cfg_json_generate(&gw_cfg, &json_str));
    ASSERT_EQ(nullptr, json_str.p_str);
    TEST_CHECK_LOG_RECORD(ESP_LOG_ERROR, string("Can't add json item: eth_dns2"));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_TRUE(g_pTestClass->m_mem_alloc_trace.is_empty());
}

TEST_F(TestGwCfgJson, gw_cfg_json_generate_malloc_failed_on_eth_dns2_1) // NOLINT
{
    const ruuvi_gateway_config_t gw_cfg   = get_gateway_config_default();
    cjson_wrap_str_t             json_str = cjson_wrap_str_null();

    cJSON_Hooks hooks = {
        .malloc_fn = &os_malloc,
        .free_fn   = &os_free_internal,
    };
    cJSON_InitHooks(&hooks);
    this->m_malloc_fail_on_cnt = 19;

    ASSERT_FALSE(gw_cfg_json_generate(&gw_cfg, &json_str));
    ASSERT_EQ(nullptr, json_str.p_str);
    TEST_CHECK_LOG_RECORD(ESP_LOG_ERROR, string("Can't add json item: eth_dns2"));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_TRUE(g_pTestClass->m_mem_alloc_trace.is_empty());
}

TEST_F(TestGwCfgJson, gw_cfg_json_generate_malloc_failed_on_eth_dns2_2) // NOLINT
{
    const ruuvi_gateway_config_t gw_cfg   = get_gateway_config_default();
    cjson_wrap_str_t             json_str = cjson_wrap_str_null();

    cJSON_Hooks hooks = {
        .malloc_fn = &os_malloc,
        .free_fn   = &os_free_internal,
    };
    cJSON_InitHooks(&hooks);
    this->m_malloc_fail_on_cnt = 20;

    ASSERT_FALSE(gw_cfg_json_generate(&gw_cfg, &json_str));
    ASSERT_EQ(nullptr, json_str.p_str);
    TEST_CHECK_LOG_RECORD(ESP_LOG_ERROR, string("Can't add json item: eth_dns2"));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_TRUE(g_pTestClass->m_mem_alloc_trace.is_empty());
}

TEST_F(TestGwCfgJson, gw_cfg_json_generate_malloc_failed_on_use_http) // NOLINT
{
    const ruuvi_gateway_config_t gw_cfg   = get_gateway_config_default();
    cjson_wrap_str_t             json_str = cjson_wrap_str_null();

    cJSON_Hooks hooks = {
        .malloc_fn = &os_malloc,
        .free_fn   = &os_free_internal,
    };
    cJSON_InitHooks(&hooks);
    this->m_malloc_fail_on_cnt = 21;

    ASSERT_FALSE(gw_cfg_json_generate(&gw_cfg, &json_str));
    ASSERT_EQ(nullptr, json_str.p_str);
    TEST_CHECK_LOG_RECORD(ESP_LOG_ERROR, string("Can't add json item: use_http"));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_TRUE(g_pTestClass->m_mem_alloc_trace.is_empty());
}

TEST_F(TestGwCfgJson, gw_cfg_json_generate_malloc_failed_on_use_http_2) // NOLINT
{
    const ruuvi_gateway_config_t gw_cfg   = get_gateway_config_default();
    cjson_wrap_str_t             json_str = cjson_wrap_str_null();

    cJSON_Hooks hooks = {
        .malloc_fn = &os_malloc,
        .free_fn   = &os_free_internal,
    };
    cJSON_InitHooks(&hooks);
    this->m_malloc_fail_on_cnt = 22;

    ASSERT_FALSE(gw_cfg_json_generate(&gw_cfg, &json_str));
    ASSERT_EQ(nullptr, json_str.p_str);
    TEST_CHECK_LOG_RECORD(ESP_LOG_ERROR, string("Can't add json item: use_http"));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_TRUE(g_pTestClass->m_mem_alloc_trace.is_empty());
}

TEST_F(TestGwCfgJson, gw_cfg_json_generate_malloc_failed_on_http_url) // NOLINT
{
    const ruuvi_gateway_config_t gw_cfg   = get_gateway_config_default();
    cjson_wrap_str_t             json_str = cjson_wrap_str_null();

    cJSON_Hooks hooks = {
        .malloc_fn = &os_malloc,
        .free_fn   = &os_free_internal,
    };
    cJSON_InitHooks(&hooks);
    this->m_malloc_fail_on_cnt = 23;

    ASSERT_FALSE(gw_cfg_json_generate(&gw_cfg, &json_str));
    ASSERT_EQ(nullptr, json_str.p_str);
    TEST_CHECK_LOG_RECORD(ESP_LOG_ERROR, string("Can't add json item: http_url"));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_TRUE(g_pTestClass->m_mem_alloc_trace.is_empty());
}

TEST_F(TestGwCfgJson, gw_cfg_json_generate_malloc_failed_on_http_url_2) // NOLINT
{
    const ruuvi_gateway_config_t gw_cfg   = get_gateway_config_default();
    cjson_wrap_str_t             json_str = cjson_wrap_str_null();

    cJSON_Hooks hooks = {
        .malloc_fn = &os_malloc,
        .free_fn   = &os_free_internal,
    };
    cJSON_InitHooks(&hooks);
    this->m_malloc_fail_on_cnt = 24;

    ASSERT_FALSE(gw_cfg_json_generate(&gw_cfg, &json_str));
    ASSERT_EQ(nullptr, json_str.p_str);
    TEST_CHECK_LOG_RECORD(ESP_LOG_ERROR, string("Can't add json item: http_url"));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_TRUE(g_pTestClass->m_mem_alloc_trace.is_empty());
}

TEST_F(TestGwCfgJson, gw_cfg_json_generate_malloc_failed_on_http_url_3) // NOLINT
{
    const ruuvi_gateway_config_t gw_cfg   = get_gateway_config_default();
    cjson_wrap_str_t             json_str = cjson_wrap_str_null();

    cJSON_Hooks hooks = {
        .malloc_fn = &os_malloc,
        .free_fn   = &os_free_internal,
    };
    cJSON_InitHooks(&hooks);
    this->m_malloc_fail_on_cnt = 25;

    ASSERT_FALSE(gw_cfg_json_generate(&gw_cfg, &json_str));
    ASSERT_EQ(nullptr, json_str.p_str);
    TEST_CHECK_LOG_RECORD(ESP_LOG_ERROR, string("Can't add json item: http_url"));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_TRUE(g_pTestClass->m_mem_alloc_trace.is_empty());
}

TEST_F(TestGwCfgJson, gw_cfg_json_generate_malloc_failed_on_http_user) // NOLINT
{
    const ruuvi_gateway_config_t gw_cfg   = get_gateway_config_default();
    cjson_wrap_str_t             json_str = cjson_wrap_str_null();

    cJSON_Hooks hooks = {
        .malloc_fn = &os_malloc,
        .free_fn   = &os_free_internal,
    };
    cJSON_InitHooks(&hooks);
    this->m_malloc_fail_on_cnt = 26;

    ASSERT_FALSE(gw_cfg_json_generate(&gw_cfg, &json_str));
    ASSERT_EQ(nullptr, json_str.p_str);
    TEST_CHECK_LOG_RECORD(ESP_LOG_ERROR, string("Can't add json item: http_user"));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_TRUE(g_pTestClass->m_mem_alloc_trace.is_empty());
}

TEST_F(TestGwCfgJson, gw_cfg_json_generate_malloc_failed_on_http_user_2) // NOLINT
{
    const ruuvi_gateway_config_t gw_cfg   = get_gateway_config_default();
    cjson_wrap_str_t             json_str = cjson_wrap_str_null();

    cJSON_Hooks hooks = {
        .malloc_fn = &os_malloc,
        .free_fn   = &os_free_internal,
    };
    cJSON_InitHooks(&hooks);
    this->m_malloc_fail_on_cnt = 27;

    ASSERT_FALSE(gw_cfg_json_generate(&gw_cfg, &json_str));
    ASSERT_EQ(nullptr, json_str.p_str);
    TEST_CHECK_LOG_RECORD(ESP_LOG_ERROR, string("Can't add json item: http_user"));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_TRUE(g_pTestClass->m_mem_alloc_trace.is_empty());
}

TEST_F(TestGwCfgJson, gw_cfg_json_generate_malloc_failed_on_http_user_3) // NOLINT
{
    const ruuvi_gateway_config_t gw_cfg   = get_gateway_config_default();
    cjson_wrap_str_t             json_str = cjson_wrap_str_null();

    cJSON_Hooks hooks = {
        .malloc_fn = &os_malloc,
        .free_fn   = &os_free_internal,
    };
    cJSON_InitHooks(&hooks);
    this->m_malloc_fail_on_cnt = 28;

    ASSERT_FALSE(gw_cfg_json_generate(&gw_cfg, &json_str));
    ASSERT_EQ(nullptr, json_str.p_str);
    TEST_CHECK_LOG_RECORD(ESP_LOG_ERROR, string("Can't add json item: http_user"));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_TRUE(g_pTestClass->m_mem_alloc_trace.is_empty());
}

TEST_F(TestGwCfgJson, gw_cfg_json_generate_malloc_failed_on_http_pass) // NOLINT
{
    const ruuvi_gateway_config_t gw_cfg   = get_gateway_config_default();
    cjson_wrap_str_t             json_str = cjson_wrap_str_null();

    cJSON_Hooks hooks = {
        .malloc_fn = &os_malloc,
        .free_fn   = &os_free_internal,
    };
    cJSON_InitHooks(&hooks);
    this->m_malloc_fail_on_cnt = 29;

    ASSERT_FALSE(gw_cfg_json_generate(&gw_cfg, &json_str));
    ASSERT_EQ(nullptr, json_str.p_str);
    TEST_CHECK_LOG_RECORD(ESP_LOG_ERROR, string("Can't add json item: http_pass"));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_TRUE(g_pTestClass->m_mem_alloc_trace.is_empty());
}

TEST_F(TestGwCfgJson, gw_cfg_json_generate_malloc_failed_on_http_pass_2) // NOLINT
{
    const ruuvi_gateway_config_t gw_cfg   = get_gateway_config_default();
    cjson_wrap_str_t             json_str = cjson_wrap_str_null();

    cJSON_Hooks hooks = {
        .malloc_fn = &os_malloc,
        .free_fn   = &os_free_internal,
    };
    cJSON_InitHooks(&hooks);
    this->m_malloc_fail_on_cnt = 30;

    ASSERT_FALSE(gw_cfg_json_generate(&gw_cfg, &json_str));
    ASSERT_EQ(nullptr, json_str.p_str);
    TEST_CHECK_LOG_RECORD(ESP_LOG_ERROR, string("Can't add json item: http_pass"));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_TRUE(g_pTestClass->m_mem_alloc_trace.is_empty());
}

TEST_F(TestGwCfgJson, gw_cfg_json_generate_malloc_failed_on_http_pass_3) // NOLINT
{
    const ruuvi_gateway_config_t gw_cfg   = get_gateway_config_default();
    cjson_wrap_str_t             json_str = cjson_wrap_str_null();

    cJSON_Hooks hooks = {
        .malloc_fn = &os_malloc,
        .free_fn   = &os_free_internal,
    };
    cJSON_InitHooks(&hooks);
    this->m_malloc_fail_on_cnt = 31;

    ASSERT_FALSE(gw_cfg_json_generate(&gw_cfg, &json_str));
    ASSERT_EQ(nullptr, json_str.p_str);
    TEST_CHECK_LOG_RECORD(ESP_LOG_ERROR, string("Can't add json item: http_pass"));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_TRUE(g_pTestClass->m_mem_alloc_trace.is_empty());
}

TEST_F(TestGwCfgJson, gw_cfg_json_generate_malloc_failed_on_use_http_stat) // NOLINT
{
    const ruuvi_gateway_config_t gw_cfg   = get_gateway_config_default();
    cjson_wrap_str_t             json_str = cjson_wrap_str_null();

    cJSON_Hooks hooks = {
        .malloc_fn = &os_malloc,
        .free_fn   = &os_free_internal,
    };
    cJSON_InitHooks(&hooks);
    this->m_malloc_fail_on_cnt = 32;

    ASSERT_FALSE(gw_cfg_json_generate(&gw_cfg, &json_str));
    ASSERT_EQ(nullptr, json_str.p_str);
    TEST_CHECK_LOG_RECORD(ESP_LOG_ERROR, string("Can't add json item: use_http_stat"));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_TRUE(g_pTestClass->m_mem_alloc_trace.is_empty());
}

TEST_F(TestGwCfgJson, gw_cfg_json_generate_malloc_failed_on_use_http_stat_2) // NOLINT
{
    const ruuvi_gateway_config_t gw_cfg   = get_gateway_config_default();
    cjson_wrap_str_t             json_str = cjson_wrap_str_null();

    cJSON_Hooks hooks = {
        .malloc_fn = &os_malloc,
        .free_fn   = &os_free_internal,
    };
    cJSON_InitHooks(&hooks);
    this->m_malloc_fail_on_cnt = 33;

    ASSERT_FALSE(gw_cfg_json_generate(&gw_cfg, &json_str));
    ASSERT_EQ(nullptr, json_str.p_str);
    TEST_CHECK_LOG_RECORD(ESP_LOG_ERROR, string("Can't add json item: use_http_stat"));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_TRUE(g_pTestClass->m_mem_alloc_trace.is_empty());
}

TEST_F(TestGwCfgJson, gw_cfg_json_generate_malloc_failed_on_http_stat_url) // NOLINT
{
    const ruuvi_gateway_config_t gw_cfg   = get_gateway_config_default();
    cjson_wrap_str_t             json_str = cjson_wrap_str_null();

    cJSON_Hooks hooks = {
        .malloc_fn = &os_malloc,
        .free_fn   = &os_free_internal,
    };
    cJSON_InitHooks(&hooks);
    this->m_malloc_fail_on_cnt = 34;

    ASSERT_FALSE(gw_cfg_json_generate(&gw_cfg, &json_str));
    ASSERT_EQ(nullptr, json_str.p_str);
    TEST_CHECK_LOG_RECORD(ESP_LOG_ERROR, string("Can't add json item: http_stat_url"));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_TRUE(g_pTestClass->m_mem_alloc_trace.is_empty());
}

TEST_F(TestGwCfgJson, gw_cfg_json_generate_malloc_failed_on_http_stat_url_2) // NOLINT
{
    const ruuvi_gateway_config_t gw_cfg   = get_gateway_config_default();
    cjson_wrap_str_t             json_str = cjson_wrap_str_null();

    cJSON_Hooks hooks = {
        .malloc_fn = &os_malloc,
        .free_fn   = &os_free_internal,
    };
    cJSON_InitHooks(&hooks);
    this->m_malloc_fail_on_cnt = 35;

    ASSERT_FALSE(gw_cfg_json_generate(&gw_cfg, &json_str));
    ASSERT_EQ(nullptr, json_str.p_str);
    TEST_CHECK_LOG_RECORD(ESP_LOG_ERROR, string("Can't add json item: http_stat_url"));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_TRUE(g_pTestClass->m_mem_alloc_trace.is_empty());
}

TEST_F(TestGwCfgJson, gw_cfg_json_generate_malloc_failed_on_http_stat_url_3) // NOLINT
{
    const ruuvi_gateway_config_t gw_cfg   = get_gateway_config_default();
    cjson_wrap_str_t             json_str = cjson_wrap_str_null();

    cJSON_Hooks hooks = {
        .malloc_fn = &os_malloc,
        .free_fn   = &os_free_internal,
    };
    cJSON_InitHooks(&hooks);
    this->m_malloc_fail_on_cnt = 36;

    ASSERT_FALSE(gw_cfg_json_generate(&gw_cfg, &json_str));
    ASSERT_EQ(nullptr, json_str.p_str);
    TEST_CHECK_LOG_RECORD(ESP_LOG_ERROR, string("Can't add json item: http_stat_url"));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_TRUE(g_pTestClass->m_mem_alloc_trace.is_empty());
}

TEST_F(TestGwCfgJson, gw_cfg_json_generate_malloc_failed_on_http_stat_user) // NOLINT
{
    const ruuvi_gateway_config_t gw_cfg   = get_gateway_config_default();
    cjson_wrap_str_t             json_str = cjson_wrap_str_null();

    cJSON_Hooks hooks = {
        .malloc_fn = &os_malloc,
        .free_fn   = &os_free_internal,
    };
    cJSON_InitHooks(&hooks);
    this->m_malloc_fail_on_cnt = 37;

    ASSERT_FALSE(gw_cfg_json_generate(&gw_cfg, &json_str));
    ASSERT_EQ(nullptr, json_str.p_str);
    TEST_CHECK_LOG_RECORD(ESP_LOG_ERROR, string("Can't add json item: http_stat_user"));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_TRUE(g_pTestClass->m_mem_alloc_trace.is_empty());
}

TEST_F(TestGwCfgJson, gw_cfg_json_generate_malloc_failed_on_http_stat_user_2) // NOLINT
{
    const ruuvi_gateway_config_t gw_cfg   = get_gateway_config_default();
    cjson_wrap_str_t             json_str = cjson_wrap_str_null();

    cJSON_Hooks hooks = {
        .malloc_fn = &os_malloc,
        .free_fn   = &os_free_internal,
    };
    cJSON_InitHooks(&hooks);
    this->m_malloc_fail_on_cnt = 38;

    ASSERT_FALSE(gw_cfg_json_generate(&gw_cfg, &json_str));
    ASSERT_EQ(nullptr, json_str.p_str);
    TEST_CHECK_LOG_RECORD(ESP_LOG_ERROR, string("Can't add json item: http_stat_user"));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_TRUE(g_pTestClass->m_mem_alloc_trace.is_empty());
}

TEST_F(TestGwCfgJson, gw_cfg_json_generate_malloc_failed_on_http_stat_user_3) // NOLINT
{
    const ruuvi_gateway_config_t gw_cfg   = get_gateway_config_default();
    cjson_wrap_str_t             json_str = cjson_wrap_str_null();

    cJSON_Hooks hooks = {
        .malloc_fn = &os_malloc,
        .free_fn   = &os_free_internal,
    };
    cJSON_InitHooks(&hooks);
    this->m_malloc_fail_on_cnt = 39;

    ASSERT_FALSE(gw_cfg_json_generate(&gw_cfg, &json_str));
    ASSERT_EQ(nullptr, json_str.p_str);
    TEST_CHECK_LOG_RECORD(ESP_LOG_ERROR, string("Can't add json item: http_stat_user"));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_TRUE(g_pTestClass->m_mem_alloc_trace.is_empty());
}

TEST_F(TestGwCfgJson, gw_cfg_json_generate_malloc_failed_on_http_stat_pass) // NOLINT
{
    const ruuvi_gateway_config_t gw_cfg   = get_gateway_config_default();
    cjson_wrap_str_t             json_str = cjson_wrap_str_null();

    cJSON_Hooks hooks = {
        .malloc_fn = &os_malloc,
        .free_fn   = &os_free_internal,
    };
    cJSON_InitHooks(&hooks);
    this->m_malloc_fail_on_cnt = 40;

    ASSERT_FALSE(gw_cfg_json_generate(&gw_cfg, &json_str));
    ASSERT_EQ(nullptr, json_str.p_str);
    TEST_CHECK_LOG_RECORD(ESP_LOG_ERROR, string("Can't add json item: http_stat_pass"));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_TRUE(g_pTestClass->m_mem_alloc_trace.is_empty());
}

TEST_F(TestGwCfgJson, gw_cfg_json_generate_malloc_failed_on_http_stat_pass_2) // NOLINT
{
    const ruuvi_gateway_config_t gw_cfg   = get_gateway_config_default();
    cjson_wrap_str_t             json_str = cjson_wrap_str_null();

    cJSON_Hooks hooks = {
        .malloc_fn = &os_malloc,
        .free_fn   = &os_free_internal,
    };
    cJSON_InitHooks(&hooks);
    this->m_malloc_fail_on_cnt = 41;

    ASSERT_FALSE(gw_cfg_json_generate(&gw_cfg, &json_str));
    ASSERT_EQ(nullptr, json_str.p_str);
    TEST_CHECK_LOG_RECORD(ESP_LOG_ERROR, string("Can't add json item: http_stat_pass"));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_TRUE(g_pTestClass->m_mem_alloc_trace.is_empty());
}

TEST_F(TestGwCfgJson, gw_cfg_json_generate_malloc_failed_on_http_stat_pass_3) // NOLINT
{
    const ruuvi_gateway_config_t gw_cfg   = get_gateway_config_default();
    cjson_wrap_str_t             json_str = cjson_wrap_str_null();

    cJSON_Hooks hooks = {
        .malloc_fn = &os_malloc,
        .free_fn   = &os_free_internal,
    };
    cJSON_InitHooks(&hooks);
    this->m_malloc_fail_on_cnt = 42;

    ASSERT_FALSE(gw_cfg_json_generate(&gw_cfg, &json_str));
    ASSERT_EQ(nullptr, json_str.p_str);
    TEST_CHECK_LOG_RECORD(ESP_LOG_ERROR, string("Can't add json item: http_stat_pass"));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_TRUE(g_pTestClass->m_mem_alloc_trace.is_empty());
}

TEST_F(TestGwCfgJson, gw_cfg_json_generate_malloc_failed_on_use_mqtt) // NOLINT
{
    const ruuvi_gateway_config_t gw_cfg   = get_gateway_config_default();
    cjson_wrap_str_t             json_str = cjson_wrap_str_null();

    cJSON_Hooks hooks = {
        .malloc_fn = &os_malloc,
        .free_fn   = &os_free_internal,
    };
    cJSON_InitHooks(&hooks);
    this->m_malloc_fail_on_cnt = 43;

    ASSERT_FALSE(gw_cfg_json_generate(&gw_cfg, &json_str));
    ASSERT_EQ(nullptr, json_str.p_str);
    TEST_CHECK_LOG_RECORD(ESP_LOG_ERROR, string("Can't add json item: use_mqtt"));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_TRUE(g_pTestClass->m_mem_alloc_trace.is_empty());
}

TEST_F(TestGwCfgJson, gw_cfg_json_generate_malloc_failed_on_use_mqtt_2) // NOLINT
{
    const ruuvi_gateway_config_t gw_cfg   = get_gateway_config_default();
    cjson_wrap_str_t             json_str = cjson_wrap_str_null();

    cJSON_Hooks hooks = {
        .malloc_fn = &os_malloc,
        .free_fn   = &os_free_internal,
    };
    cJSON_InitHooks(&hooks);
    this->m_malloc_fail_on_cnt = 44;

    ASSERT_FALSE(gw_cfg_json_generate(&gw_cfg, &json_str));
    ASSERT_EQ(nullptr, json_str.p_str);
    TEST_CHECK_LOG_RECORD(ESP_LOG_ERROR, string("Can't add json item: use_mqtt"));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_TRUE(g_pTestClass->m_mem_alloc_trace.is_empty());
}

TEST_F(TestGwCfgJson, gw_cfg_json_generate_malloc_failed_on_mqtt_use_default_prefix) // NOLINT
{
    const ruuvi_gateway_config_t gw_cfg   = get_gateway_config_default();
    cjson_wrap_str_t             json_str = cjson_wrap_str_null();

    cJSON_Hooks hooks = {
        .malloc_fn = &os_malloc,
        .free_fn   = &os_free_internal,
    };
    cJSON_InitHooks(&hooks);
    this->m_malloc_fail_on_cnt = 45;

    ASSERT_FALSE(gw_cfg_json_generate(&gw_cfg, &json_str));
    ASSERT_EQ(nullptr, json_str.p_str);
    TEST_CHECK_LOG_RECORD(ESP_LOG_ERROR, string("Can't add json item: mqtt_use_default_prefix"));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_TRUE(g_pTestClass->m_mem_alloc_trace.is_empty());
}

TEST_F(TestGwCfgJson, gw_cfg_json_generate_malloc_failed_on_mqtt_use_default_prefix_2) // NOLINT
{
    const ruuvi_gateway_config_t gw_cfg   = get_gateway_config_default();
    cjson_wrap_str_t             json_str = cjson_wrap_str_null();

    cJSON_Hooks hooks = {
        .malloc_fn = &os_malloc,
        .free_fn   = &os_free_internal,
    };
    cJSON_InitHooks(&hooks);
    this->m_malloc_fail_on_cnt = 46;

    ASSERT_FALSE(gw_cfg_json_generate(&gw_cfg, &json_str));
    ASSERT_EQ(nullptr, json_str.p_str);
    TEST_CHECK_LOG_RECORD(ESP_LOG_ERROR, string("Can't add json item: mqtt_use_default_prefix"));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_TRUE(g_pTestClass->m_mem_alloc_trace.is_empty());
}

TEST_F(TestGwCfgJson, gw_cfg_json_generate_malloc_failed_on_mqtt_trnasport) // NOLINT
{
    const ruuvi_gateway_config_t gw_cfg   = get_gateway_config_default();
    cjson_wrap_str_t             json_str = cjson_wrap_str_null();

    cJSON_Hooks hooks = {
        .malloc_fn = &os_malloc,
        .free_fn   = &os_free_internal,
    };
    cJSON_InitHooks(&hooks);
    this->m_malloc_fail_on_cnt = 47;

    ASSERT_FALSE(gw_cfg_json_generate(&gw_cfg, &json_str));
    ASSERT_EQ(nullptr, json_str.p_str);
    TEST_CHECK_LOG_RECORD(ESP_LOG_ERROR, string("Can't add json item: mqtt_transport"));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_TRUE(g_pTestClass->m_mem_alloc_trace.is_empty());
}

TEST_F(TestGwCfgJson, gw_cfg_json_generate_malloc_failed_on_mqtt_trnasport_2) // NOLINT
{
    const ruuvi_gateway_config_t gw_cfg   = get_gateway_config_default();
    cjson_wrap_str_t             json_str = cjson_wrap_str_null();

    cJSON_Hooks hooks = {
        .malloc_fn = &os_malloc,
        .free_fn   = &os_free_internal,
    };
    cJSON_InitHooks(&hooks);
    this->m_malloc_fail_on_cnt = 48;

    ASSERT_FALSE(gw_cfg_json_generate(&gw_cfg, &json_str));
    ASSERT_EQ(nullptr, json_str.p_str);
    TEST_CHECK_LOG_RECORD(ESP_LOG_ERROR, string("Can't add json item: mqtt_transport"));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_TRUE(g_pTestClass->m_mem_alloc_trace.is_empty());
}

TEST_F(TestGwCfgJson, gw_cfg_json_generate_malloc_failed_on_mqtt_trnasport_3) // NOLINT
{
    const ruuvi_gateway_config_t gw_cfg   = get_gateway_config_default();
    cjson_wrap_str_t             json_str = cjson_wrap_str_null();

    cJSON_Hooks hooks = {
        .malloc_fn = &os_malloc,
        .free_fn   = &os_free_internal,
    };
    cJSON_InitHooks(&hooks);
    this->m_malloc_fail_on_cnt = 49;

    ASSERT_FALSE(gw_cfg_json_generate(&gw_cfg, &json_str));
    ASSERT_EQ(nullptr, json_str.p_str);
    TEST_CHECK_LOG_RECORD(ESP_LOG_ERROR, string("Can't add json item: mqtt_transport"));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_TRUE(g_pTestClass->m_mem_alloc_trace.is_empty());
}

TEST_F(TestGwCfgJson, gw_cfg_json_generate_malloc_failed_on_mqtt_server) // NOLINT
{
    const ruuvi_gateway_config_t gw_cfg   = get_gateway_config_default();
    cjson_wrap_str_t             json_str = cjson_wrap_str_null();

    cJSON_Hooks hooks = {
        .malloc_fn = &os_malloc,
        .free_fn   = &os_free_internal,
    };
    cJSON_InitHooks(&hooks);
    this->m_malloc_fail_on_cnt = 50;

    ASSERT_FALSE(gw_cfg_json_generate(&gw_cfg, &json_str));
    ASSERT_EQ(nullptr, json_str.p_str);
    TEST_CHECK_LOG_RECORD(ESP_LOG_ERROR, string("Can't add json item: mqtt_server"));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_TRUE(g_pTestClass->m_mem_alloc_trace.is_empty());
}

TEST_F(TestGwCfgJson, gw_cfg_json_generate_malloc_failed_on_mqtt_server_2) // NOLINT
{
    const ruuvi_gateway_config_t gw_cfg   = get_gateway_config_default();
    cjson_wrap_str_t             json_str = cjson_wrap_str_null();

    cJSON_Hooks hooks = {
        .malloc_fn = &os_malloc,
        .free_fn   = &os_free_internal,
    };
    cJSON_InitHooks(&hooks);
    this->m_malloc_fail_on_cnt = 51;

    ASSERT_FALSE(gw_cfg_json_generate(&gw_cfg, &json_str));
    ASSERT_EQ(nullptr, json_str.p_str);
    TEST_CHECK_LOG_RECORD(ESP_LOG_ERROR, string("Can't add json item: mqtt_server"));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_TRUE(g_pTestClass->m_mem_alloc_trace.is_empty());
}

TEST_F(TestGwCfgJson, gw_cfg_json_generate_malloc_failed_on_mqtt_server_3) // NOLINT
{
    const ruuvi_gateway_config_t gw_cfg   = get_gateway_config_default();
    cjson_wrap_str_t             json_str = cjson_wrap_str_null();

    cJSON_Hooks hooks = {
        .malloc_fn = &os_malloc,
        .free_fn   = &os_free_internal,
    };
    cJSON_InitHooks(&hooks);
    this->m_malloc_fail_on_cnt = 52;

    ASSERT_FALSE(gw_cfg_json_generate(&gw_cfg, &json_str));
    ASSERT_EQ(nullptr, json_str.p_str);
    TEST_CHECK_LOG_RECORD(ESP_LOG_ERROR, string("Can't add json item: mqtt_server"));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_TRUE(g_pTestClass->m_mem_alloc_trace.is_empty());
}

TEST_F(TestGwCfgJson, gw_cfg_json_generate_malloc_failed_on_mqtt_port) // NOLINT
{
    const ruuvi_gateway_config_t gw_cfg   = get_gateway_config_default();
    cjson_wrap_str_t             json_str = cjson_wrap_str_null();

    cJSON_Hooks hooks = {
        .malloc_fn = &os_malloc,
        .free_fn   = &os_free_internal,
    };
    cJSON_InitHooks(&hooks);
    this->m_malloc_fail_on_cnt = 53;

    ASSERT_FALSE(gw_cfg_json_generate(&gw_cfg, &json_str));
    ASSERT_EQ(nullptr, json_str.p_str);
    TEST_CHECK_LOG_RECORD(ESP_LOG_ERROR, string("Can't add json item: mqtt_port"));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_TRUE(g_pTestClass->m_mem_alloc_trace.is_empty());
}

TEST_F(TestGwCfgJson, gw_cfg_json_generate_malloc_failed_on_mqtt_port_2) // NOLINT
{
    const ruuvi_gateway_config_t gw_cfg   = get_gateway_config_default();
    cjson_wrap_str_t             json_str = cjson_wrap_str_null();

    cJSON_Hooks hooks = {
        .malloc_fn = &os_malloc,
        .free_fn   = &os_free_internal,
    };
    cJSON_InitHooks(&hooks);
    this->m_malloc_fail_on_cnt = 54;

    ASSERT_FALSE(gw_cfg_json_generate(&gw_cfg, &json_str));
    ASSERT_EQ(nullptr, json_str.p_str);
    TEST_CHECK_LOG_RECORD(ESP_LOG_ERROR, string("Can't add json item: mqtt_port"));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_TRUE(g_pTestClass->m_mem_alloc_trace.is_empty());
}

TEST_F(TestGwCfgJson, gw_cfg_json_generate_malloc_failed_on_mqtt_prefix) // NOLINT
{
    const ruuvi_gateway_config_t gw_cfg   = get_gateway_config_default();
    cjson_wrap_str_t             json_str = cjson_wrap_str_null();

    cJSON_Hooks hooks = {
        .malloc_fn = &os_malloc,
        .free_fn   = &os_free_internal,
    };
    cJSON_InitHooks(&hooks);
    this->m_malloc_fail_on_cnt = 55;

    ASSERT_FALSE(gw_cfg_json_generate(&gw_cfg, &json_str));
    ASSERT_EQ(nullptr, json_str.p_str);
    TEST_CHECK_LOG_RECORD(ESP_LOG_ERROR, string("Can't add json item: mqtt_prefix"));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_TRUE(g_pTestClass->m_mem_alloc_trace.is_empty());
}

TEST_F(TestGwCfgJson, gw_cfg_json_generate_malloc_failed_on_mqtt_prefix_2) // NOLINT
{
    const ruuvi_gateway_config_t gw_cfg   = get_gateway_config_default();
    cjson_wrap_str_t             json_str = cjson_wrap_str_null();

    cJSON_Hooks hooks = {
        .malloc_fn = &os_malloc,
        .free_fn   = &os_free_internal,
    };
    cJSON_InitHooks(&hooks);
    this->m_malloc_fail_on_cnt = 56;

    ASSERT_FALSE(gw_cfg_json_generate(&gw_cfg, &json_str));
    ASSERT_EQ(nullptr, json_str.p_str);
    TEST_CHECK_LOG_RECORD(ESP_LOG_ERROR, string("Can't add json item: mqtt_prefix"));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_TRUE(g_pTestClass->m_mem_alloc_trace.is_empty());
}

TEST_F(TestGwCfgJson, gw_cfg_json_generate_malloc_failed_on_mqtt_prefix_3) // NOLINT
{
    const ruuvi_gateway_config_t gw_cfg   = get_gateway_config_default();
    cjson_wrap_str_t             json_str = cjson_wrap_str_null();

    cJSON_Hooks hooks = {
        .malloc_fn = &os_malloc,
        .free_fn   = &os_free_internal,
    };
    cJSON_InitHooks(&hooks);
    this->m_malloc_fail_on_cnt = 57;

    ASSERT_FALSE(gw_cfg_json_generate(&gw_cfg, &json_str));
    ASSERT_EQ(nullptr, json_str.p_str);
    TEST_CHECK_LOG_RECORD(ESP_LOG_ERROR, string("Can't add json item: mqtt_prefix"));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_TRUE(g_pTestClass->m_mem_alloc_trace.is_empty());
}

TEST_F(TestGwCfgJson, gw_cfg_json_generate_malloc_failed_on_mqtt_client_id) // NOLINT
{
    const ruuvi_gateway_config_t gw_cfg   = get_gateway_config_default();
    cjson_wrap_str_t             json_str = cjson_wrap_str_null();

    cJSON_Hooks hooks = {
        .malloc_fn = &os_malloc,
        .free_fn   = &os_free_internal,
    };
    cJSON_InitHooks(&hooks);
    this->m_malloc_fail_on_cnt = 58;

    ASSERT_FALSE(gw_cfg_json_generate(&gw_cfg, &json_str));
    ASSERT_EQ(nullptr, json_str.p_str);
    TEST_CHECK_LOG_RECORD(ESP_LOG_ERROR, string("Can't add json item: mqtt_client_id"));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_TRUE(g_pTestClass->m_mem_alloc_trace.is_empty());
}

TEST_F(TestGwCfgJson, gw_cfg_json_generate_malloc_failed_on_mqtt_client_id_2) // NOLINT
{
    const ruuvi_gateway_config_t gw_cfg   = get_gateway_config_default();
    cjson_wrap_str_t             json_str = cjson_wrap_str_null();

    cJSON_Hooks hooks = {
        .malloc_fn = &os_malloc,
        .free_fn   = &os_free_internal,
    };
    cJSON_InitHooks(&hooks);
    this->m_malloc_fail_on_cnt = 59;

    ASSERT_FALSE(gw_cfg_json_generate(&gw_cfg, &json_str));
    ASSERT_EQ(nullptr, json_str.p_str);
    TEST_CHECK_LOG_RECORD(ESP_LOG_ERROR, string("Can't add json item: mqtt_client_id"));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_TRUE(g_pTestClass->m_mem_alloc_trace.is_empty());
}

TEST_F(TestGwCfgJson, gw_cfg_json_generate_malloc_failed_on_mqtt_client_id_3) // NOLINT
{
    const ruuvi_gateway_config_t gw_cfg   = get_gateway_config_default();
    cjson_wrap_str_t             json_str = cjson_wrap_str_null();

    cJSON_Hooks hooks = {
        .malloc_fn = &os_malloc,
        .free_fn   = &os_free_internal,
    };
    cJSON_InitHooks(&hooks);
    this->m_malloc_fail_on_cnt = 60;

    ASSERT_FALSE(gw_cfg_json_generate(&gw_cfg, &json_str));
    ASSERT_EQ(nullptr, json_str.p_str);
    TEST_CHECK_LOG_RECORD(ESP_LOG_ERROR, string("Can't add json item: mqtt_client_id"));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_TRUE(g_pTestClass->m_mem_alloc_trace.is_empty());
}

TEST_F(TestGwCfgJson, gw_cfg_json_generate_malloc_failed_on_mqtt_user) // NOLINT
{
    const ruuvi_gateway_config_t gw_cfg   = get_gateway_config_default();
    cjson_wrap_str_t             json_str = cjson_wrap_str_null();

    cJSON_Hooks hooks = {
        .malloc_fn = &os_malloc,
        .free_fn   = &os_free_internal,
    };
    cJSON_InitHooks(&hooks);
    this->m_malloc_fail_on_cnt = 61;

    ASSERT_FALSE(gw_cfg_json_generate(&gw_cfg, &json_str));
    ASSERT_EQ(nullptr, json_str.p_str);
    TEST_CHECK_LOG_RECORD(ESP_LOG_ERROR, string("Can't add json item: mqtt_user"));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_TRUE(g_pTestClass->m_mem_alloc_trace.is_empty());
}

TEST_F(TestGwCfgJson, gw_cfg_json_generate_malloc_failed_on_mqtt_user_2) // NOLINT
{
    const ruuvi_gateway_config_t gw_cfg   = get_gateway_config_default();
    cjson_wrap_str_t             json_str = cjson_wrap_str_null();

    cJSON_Hooks hooks = {
        .malloc_fn = &os_malloc,
        .free_fn   = &os_free_internal,
    };
    cJSON_InitHooks(&hooks);
    this->m_malloc_fail_on_cnt = 62;

    ASSERT_FALSE(gw_cfg_json_generate(&gw_cfg, &json_str));
    ASSERT_EQ(nullptr, json_str.p_str);
    TEST_CHECK_LOG_RECORD(ESP_LOG_ERROR, string("Can't add json item: mqtt_user"));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_TRUE(g_pTestClass->m_mem_alloc_trace.is_empty());
}

TEST_F(TestGwCfgJson, gw_cfg_json_generate_malloc_failed_on_mqtt_user_3) // NOLINT
{
    const ruuvi_gateway_config_t gw_cfg   = get_gateway_config_default();
    cjson_wrap_str_t             json_str = cjson_wrap_str_null();

    cJSON_Hooks hooks = {
        .malloc_fn = &os_malloc,
        .free_fn   = &os_free_internal,
    };
    cJSON_InitHooks(&hooks);
    this->m_malloc_fail_on_cnt = 63;

    ASSERT_FALSE(gw_cfg_json_generate(&gw_cfg, &json_str));
    ASSERT_EQ(nullptr, json_str.p_str);
    TEST_CHECK_LOG_RECORD(ESP_LOG_ERROR, string("Can't add json item: mqtt_user"));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_TRUE(g_pTestClass->m_mem_alloc_trace.is_empty());
}

TEST_F(TestGwCfgJson, gw_cfg_json_generate_malloc_failed_on_mqtt_pass) // NOLINT
{
    const ruuvi_gateway_config_t gw_cfg   = get_gateway_config_default();
    cjson_wrap_str_t             json_str = cjson_wrap_str_null();

    cJSON_Hooks hooks = {
        .malloc_fn = &os_malloc,
        .free_fn   = &os_free_internal,
    };
    cJSON_InitHooks(&hooks);
    this->m_malloc_fail_on_cnt = 64;

    ASSERT_FALSE(gw_cfg_json_generate(&gw_cfg, &json_str));
    ASSERT_EQ(nullptr, json_str.p_str);
    TEST_CHECK_LOG_RECORD(ESP_LOG_ERROR, string("Can't add json item: mqtt_pass"));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_TRUE(g_pTestClass->m_mem_alloc_trace.is_empty());
}

TEST_F(TestGwCfgJson, gw_cfg_json_generate_malloc_failed_on_mqtt_pass_2) // NOLINT
{
    const ruuvi_gateway_config_t gw_cfg   = get_gateway_config_default();
    cjson_wrap_str_t             json_str = cjson_wrap_str_null();

    cJSON_Hooks hooks = {
        .malloc_fn = &os_malloc,
        .free_fn   = &os_free_internal,
    };
    cJSON_InitHooks(&hooks);
    this->m_malloc_fail_on_cnt = 65;

    ASSERT_FALSE(gw_cfg_json_generate(&gw_cfg, &json_str));
    ASSERT_EQ(nullptr, json_str.p_str);
    TEST_CHECK_LOG_RECORD(ESP_LOG_ERROR, string("Can't add json item: mqtt_pass"));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_TRUE(g_pTestClass->m_mem_alloc_trace.is_empty());
}

TEST_F(TestGwCfgJson, gw_cfg_json_generate_malloc_failed_on_mqtt_pass_3) // NOLINT
{
    const ruuvi_gateway_config_t gw_cfg   = get_gateway_config_default();
    cjson_wrap_str_t             json_str = cjson_wrap_str_null();

    cJSON_Hooks hooks = {
        .malloc_fn = &os_malloc,
        .free_fn   = &os_free_internal,
    };
    cJSON_InitHooks(&hooks);
    this->m_malloc_fail_on_cnt = 66;

    ASSERT_FALSE(gw_cfg_json_generate(&gw_cfg, &json_str));
    ASSERT_EQ(nullptr, json_str.p_str);
    TEST_CHECK_LOG_RECORD(ESP_LOG_ERROR, string("Can't add json item: mqtt_pass"));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_TRUE(g_pTestClass->m_mem_alloc_trace.is_empty());
}

TEST_F(TestGwCfgJson, gw_cfg_json_generate_malloc_failed_on_lan_auth_type) // NOLINT
{
    const ruuvi_gateway_config_t gw_cfg   = get_gateway_config_default();
    cjson_wrap_str_t             json_str = cjson_wrap_str_null();

    cJSON_Hooks hooks = {
        .malloc_fn = &os_malloc,
        .free_fn   = &os_free_internal,
    };
    cJSON_InitHooks(&hooks);
    this->m_malloc_fail_on_cnt = 67;

    ASSERT_FALSE(gw_cfg_json_generate(&gw_cfg, &json_str));
    ASSERT_EQ(nullptr, json_str.p_str);
    TEST_CHECK_LOG_RECORD(ESP_LOG_ERROR, string("Can't add json item: lan_auth_type"));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_TRUE(g_pTestClass->m_mem_alloc_trace.is_empty());
}

TEST_F(TestGwCfgJson, gw_cfg_json_generate_malloc_failed_on_lan_auth_type_2) // NOLINT
{
    const ruuvi_gateway_config_t gw_cfg   = get_gateway_config_default();
    cjson_wrap_str_t             json_str = cjson_wrap_str_null();

    cJSON_Hooks hooks = {
        .malloc_fn = &os_malloc,
        .free_fn   = &os_free_internal,
    };
    cJSON_InitHooks(&hooks);
    this->m_malloc_fail_on_cnt = 68;

    ASSERT_FALSE(gw_cfg_json_generate(&gw_cfg, &json_str));
    ASSERT_EQ(nullptr, json_str.p_str);
    TEST_CHECK_LOG_RECORD(ESP_LOG_ERROR, string("Can't add json item: lan_auth_type"));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_TRUE(g_pTestClass->m_mem_alloc_trace.is_empty());
}

TEST_F(TestGwCfgJson, gw_cfg_json_generate_malloc_failed_on_lan_auth_type_3) // NOLINT
{
    const ruuvi_gateway_config_t gw_cfg   = get_gateway_config_default();
    cjson_wrap_str_t             json_str = cjson_wrap_str_null();

    cJSON_Hooks hooks = {
        .malloc_fn = &os_malloc,
        .free_fn   = &os_free_internal,
    };
    cJSON_InitHooks(&hooks);
    this->m_malloc_fail_on_cnt = 69;

    ASSERT_FALSE(gw_cfg_json_generate(&gw_cfg, &json_str));
    ASSERT_EQ(nullptr, json_str.p_str);
    TEST_CHECK_LOG_RECORD(ESP_LOG_ERROR, string("Can't add json item: lan_auth_type"));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_TRUE(g_pTestClass->m_mem_alloc_trace.is_empty());
}

TEST_F(TestGwCfgJson, gw_cfg_json_generate_malloc_failed_on_lan_auth_user) // NOLINT
{
    const ruuvi_gateway_config_t gw_cfg   = get_gateway_config_default();
    cjson_wrap_str_t             json_str = cjson_wrap_str_null();

    cJSON_Hooks hooks = {
        .malloc_fn = &os_malloc,
        .free_fn   = &os_free_internal,
    };
    cJSON_InitHooks(&hooks);
    this->m_malloc_fail_on_cnt = 70;

    ASSERT_FALSE(gw_cfg_json_generate(&gw_cfg, &json_str));
    ASSERT_EQ(nullptr, json_str.p_str);
    TEST_CHECK_LOG_RECORD(ESP_LOG_ERROR, string("Can't add json item: lan_auth_user"));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_TRUE(g_pTestClass->m_mem_alloc_trace.is_empty());
}

TEST_F(TestGwCfgJson, gw_cfg_json_generate_malloc_failed_on_lan_auth_user_2) // NOLINT
{
    const ruuvi_gateway_config_t gw_cfg   = get_gateway_config_default();
    cjson_wrap_str_t             json_str = cjson_wrap_str_null();

    cJSON_Hooks hooks = {
        .malloc_fn = &os_malloc,
        .free_fn   = &os_free_internal,
    };
    cJSON_InitHooks(&hooks);
    this->m_malloc_fail_on_cnt = 71;

    ASSERT_FALSE(gw_cfg_json_generate(&gw_cfg, &json_str));
    ASSERT_EQ(nullptr, json_str.p_str);
    TEST_CHECK_LOG_RECORD(ESP_LOG_ERROR, string("Can't add json item: lan_auth_user"));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_TRUE(g_pTestClass->m_mem_alloc_trace.is_empty());
}

TEST_F(TestGwCfgJson, gw_cfg_json_generate_malloc_failed_on_lan_auth_user_3) // NOLINT
{
    const ruuvi_gateway_config_t gw_cfg   = get_gateway_config_default();
    cjson_wrap_str_t             json_str = cjson_wrap_str_null();

    cJSON_Hooks hooks = {
        .malloc_fn = &os_malloc,
        .free_fn   = &os_free_internal,
    };
    cJSON_InitHooks(&hooks);
    this->m_malloc_fail_on_cnt = 72;

    ASSERT_FALSE(gw_cfg_json_generate(&gw_cfg, &json_str));
    ASSERT_EQ(nullptr, json_str.p_str);
    TEST_CHECK_LOG_RECORD(ESP_LOG_ERROR, string("Can't add json item: lan_auth_user"));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_TRUE(g_pTestClass->m_mem_alloc_trace.is_empty());
}

TEST_F(TestGwCfgJson, gw_cfg_json_generate_malloc_failed_on_lan_auth_pass) // NOLINT
{
    const ruuvi_gateway_config_t gw_cfg   = get_gateway_config_default();
    cjson_wrap_str_t             json_str = cjson_wrap_str_null();

    cJSON_Hooks hooks = {
        .malloc_fn = &os_malloc,
        .free_fn   = &os_free_internal,
    };
    cJSON_InitHooks(&hooks);
    this->m_malloc_fail_on_cnt = 73;

    ASSERT_FALSE(gw_cfg_json_generate(&gw_cfg, &json_str));
    ASSERT_EQ(nullptr, json_str.p_str);
    TEST_CHECK_LOG_RECORD(ESP_LOG_ERROR, string("Can't add json item: lan_auth_pass"));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_TRUE(g_pTestClass->m_mem_alloc_trace.is_empty());
}

TEST_F(TestGwCfgJson, gw_cfg_json_generate_malloc_failed_on_lan_auth_pass_2) // NOLINT
{
    const ruuvi_gateway_config_t gw_cfg   = get_gateway_config_default();
    cjson_wrap_str_t             json_str = cjson_wrap_str_null();

    cJSON_Hooks hooks = {
        .malloc_fn = &os_malloc,
        .free_fn   = &os_free_internal,
    };
    cJSON_InitHooks(&hooks);
    this->m_malloc_fail_on_cnt = 74;

    ASSERT_FALSE(gw_cfg_json_generate(&gw_cfg, &json_str));
    ASSERT_EQ(nullptr, json_str.p_str);
    TEST_CHECK_LOG_RECORD(ESP_LOG_ERROR, string("Can't add json item: lan_auth_pass"));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_TRUE(g_pTestClass->m_mem_alloc_trace.is_empty());
}

TEST_F(TestGwCfgJson, gw_cfg_json_generate_malloc_failed_on_lan_auth_pass_3) // NOLINT
{
    const ruuvi_gateway_config_t gw_cfg   = get_gateway_config_default();
    cjson_wrap_str_t             json_str = cjson_wrap_str_null();

    cJSON_Hooks hooks = {
        .malloc_fn = &os_malloc,
        .free_fn   = &os_free_internal,
    };
    cJSON_InitHooks(&hooks);
    this->m_malloc_fail_on_cnt = 75;

    ASSERT_FALSE(gw_cfg_json_generate(&gw_cfg, &json_str));
    ASSERT_EQ(nullptr, json_str.p_str);
    TEST_CHECK_LOG_RECORD(ESP_LOG_ERROR, string("Can't add json item: lan_auth_pass"));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_TRUE(g_pTestClass->m_mem_alloc_trace.is_empty());
}

TEST_F(TestGwCfgJson, gw_cfg_json_generate_malloc_failed_on_lan_auth_api_key) // NOLINT
{
    const ruuvi_gateway_config_t gw_cfg   = get_gateway_config_default();
    cjson_wrap_str_t             json_str = cjson_wrap_str_null();

    cJSON_Hooks hooks = {
        .malloc_fn = &os_malloc,
        .free_fn   = &os_free_internal,
    };
    cJSON_InitHooks(&hooks);
    this->m_malloc_fail_on_cnt = 76;

    ASSERT_FALSE(gw_cfg_json_generate(&gw_cfg, &json_str));
    ASSERT_EQ(nullptr, json_str.p_str);
    TEST_CHECK_LOG_RECORD(ESP_LOG_ERROR, string("Can't add json item: lan_auth_api_key"));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_TRUE(g_pTestClass->m_mem_alloc_trace.is_empty());
}

TEST_F(TestGwCfgJson, gw_cfg_json_generate_malloc_failed_on_lan_auth_api_key_2) // NOLINT
{
    const ruuvi_gateway_config_t gw_cfg   = get_gateway_config_default();
    cjson_wrap_str_t             json_str = cjson_wrap_str_null();

    cJSON_Hooks hooks = {
        .malloc_fn = &os_malloc,
        .free_fn   = &os_free_internal,
    };
    cJSON_InitHooks(&hooks);
    this->m_malloc_fail_on_cnt = 77;

    ASSERT_FALSE(gw_cfg_json_generate(&gw_cfg, &json_str));
    ASSERT_EQ(nullptr, json_str.p_str);
    TEST_CHECK_LOG_RECORD(ESP_LOG_ERROR, string("Can't add json item: lan_auth_api_key"));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_TRUE(g_pTestClass->m_mem_alloc_trace.is_empty());
}

TEST_F(TestGwCfgJson, gw_cfg_json_generate_malloc_failed_on_lan_auth_api_key_3) // NOLINT
{
    const ruuvi_gateway_config_t gw_cfg   = get_gateway_config_default();
    cjson_wrap_str_t             json_str = cjson_wrap_str_null();

    cJSON_Hooks hooks = {
        .malloc_fn = &os_malloc,
        .free_fn   = &os_free_internal,
    };
    cJSON_InitHooks(&hooks);
    this->m_malloc_fail_on_cnt = 78;

    ASSERT_FALSE(gw_cfg_json_generate(&gw_cfg, &json_str));
    ASSERT_EQ(nullptr, json_str.p_str);
    TEST_CHECK_LOG_RECORD(ESP_LOG_ERROR, string("Can't add json item: lan_auth_api_key"));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_TRUE(g_pTestClass->m_mem_alloc_trace.is_empty());
}

TEST_F(TestGwCfgJson, gw_cfg_json_generate_malloc_failed_on_auto_update_cycle) // NOLINT
{
    const ruuvi_gateway_config_t gw_cfg   = get_gateway_config_default();
    cjson_wrap_str_t             json_str = cjson_wrap_str_null();

    cJSON_Hooks hooks = {
        .malloc_fn = &os_malloc,
        .free_fn   = &os_free_internal,
    };
    cJSON_InitHooks(&hooks);
    this->m_malloc_fail_on_cnt = 79;

    ASSERT_FALSE(gw_cfg_json_generate(&gw_cfg, &json_str));
    ASSERT_EQ(nullptr, json_str.p_str);
    TEST_CHECK_LOG_RECORD(ESP_LOG_ERROR, string("Can't add json item: auto_update_cycle"));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_TRUE(g_pTestClass->m_mem_alloc_trace.is_empty());
}

TEST_F(TestGwCfgJson, gw_cfg_json_generate_malloc_failed_on_auto_update_cycle_2) // NOLINT
{
    const ruuvi_gateway_config_t gw_cfg   = get_gateway_config_default();
    cjson_wrap_str_t             json_str = cjson_wrap_str_null();

    cJSON_Hooks hooks = {
        .malloc_fn = &os_malloc,
        .free_fn   = &os_free_internal,
    };
    cJSON_InitHooks(&hooks);
    this->m_malloc_fail_on_cnt = 80;

    ASSERT_FALSE(gw_cfg_json_generate(&gw_cfg, &json_str));
    ASSERT_EQ(nullptr, json_str.p_str);
    TEST_CHECK_LOG_RECORD(ESP_LOG_ERROR, string("Can't add json item: auto_update_cycle"));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_TRUE(g_pTestClass->m_mem_alloc_trace.is_empty());
}

TEST_F(TestGwCfgJson, gw_cfg_json_generate_malloc_failed_on_auto_update_cycle_3) // NOLINT
{
    const ruuvi_gateway_config_t gw_cfg   = get_gateway_config_default();
    cjson_wrap_str_t             json_str = cjson_wrap_str_null();

    cJSON_Hooks hooks = {
        .malloc_fn = &os_malloc,
        .free_fn   = &os_free_internal,
    };
    cJSON_InitHooks(&hooks);
    this->m_malloc_fail_on_cnt = 81;

    ASSERT_FALSE(gw_cfg_json_generate(&gw_cfg, &json_str));
    ASSERT_EQ(nullptr, json_str.p_str);
    TEST_CHECK_LOG_RECORD(ESP_LOG_ERROR, string("Can't add json item: auto_update_cycle"));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_TRUE(g_pTestClass->m_mem_alloc_trace.is_empty());
}

TEST_F(TestGwCfgJson, gw_cfg_json_generate_malloc_failed_on_auto_update_weekdays_bitmask) // NOLINT
{
    const ruuvi_gateway_config_t gw_cfg   = get_gateway_config_default();
    cjson_wrap_str_t             json_str = cjson_wrap_str_null();

    cJSON_Hooks hooks = {
        .malloc_fn = &os_malloc,
        .free_fn   = &os_free_internal,
    };
    cJSON_InitHooks(&hooks);
    this->m_malloc_fail_on_cnt = 82;

    ASSERT_FALSE(gw_cfg_json_generate(&gw_cfg, &json_str));
    ASSERT_EQ(nullptr, json_str.p_str);
    TEST_CHECK_LOG_RECORD(ESP_LOG_ERROR, string("Can't add json item: auto_update_weekdays_bitmask"));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_TRUE(g_pTestClass->m_mem_alloc_trace.is_empty());
}

TEST_F(TestGwCfgJson, gw_cfg_json_generate_malloc_failed_on_auto_update_weekdays_bitmask_2) // NOLINT
{
    const ruuvi_gateway_config_t gw_cfg   = get_gateway_config_default();
    cjson_wrap_str_t             json_str = cjson_wrap_str_null();

    cJSON_Hooks hooks = {
        .malloc_fn = &os_malloc,
        .free_fn   = &os_free_internal,
    };
    cJSON_InitHooks(&hooks);
    this->m_malloc_fail_on_cnt = 83;

    ASSERT_FALSE(gw_cfg_json_generate(&gw_cfg, &json_str));
    ASSERT_EQ(nullptr, json_str.p_str);
    TEST_CHECK_LOG_RECORD(ESP_LOG_ERROR, string("Can't add json item: auto_update_weekdays_bitmask"));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_TRUE(g_pTestClass->m_mem_alloc_trace.is_empty());
}

TEST_F(TestGwCfgJson, gw_cfg_json_generate_malloc_failed_on_auto_update_interval_from) // NOLINT
{
    const ruuvi_gateway_config_t gw_cfg   = get_gateway_config_default();
    cjson_wrap_str_t             json_str = cjson_wrap_str_null();

    cJSON_Hooks hooks = {
        .malloc_fn = &os_malloc,
        .free_fn   = &os_free_internal,
    };
    cJSON_InitHooks(&hooks);
    this->m_malloc_fail_on_cnt = 84;

    ASSERT_FALSE(gw_cfg_json_generate(&gw_cfg, &json_str));
    ASSERT_EQ(nullptr, json_str.p_str);
    TEST_CHECK_LOG_RECORD(ESP_LOG_ERROR, string("Can't add json item: auto_update_interval_from"));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_TRUE(g_pTestClass->m_mem_alloc_trace.is_empty());
}

TEST_F(TestGwCfgJson, gw_cfg_json_generate_malloc_failed_on_auto_update_interval_from_2) // NOLINT
{
    const ruuvi_gateway_config_t gw_cfg   = get_gateway_config_default();
    cjson_wrap_str_t             json_str = cjson_wrap_str_null();

    cJSON_Hooks hooks = {
        .malloc_fn = &os_malloc,
        .free_fn   = &os_free_internal,
    };
    cJSON_InitHooks(&hooks);
    this->m_malloc_fail_on_cnt = 85;

    ASSERT_FALSE(gw_cfg_json_generate(&gw_cfg, &json_str));
    ASSERT_EQ(nullptr, json_str.p_str);
    TEST_CHECK_LOG_RECORD(ESP_LOG_ERROR, string("Can't add json item: auto_update_interval_from"));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_TRUE(g_pTestClass->m_mem_alloc_trace.is_empty());
}

TEST_F(TestGwCfgJson, gw_cfg_json_generate_malloc_failed_on_auto_update_interval_to) // NOLINT
{
    const ruuvi_gateway_config_t gw_cfg   = get_gateway_config_default();
    cjson_wrap_str_t             json_str = cjson_wrap_str_null();

    cJSON_Hooks hooks = {
        .malloc_fn = &os_malloc,
        .free_fn   = &os_free_internal,
    };
    cJSON_InitHooks(&hooks);
    this->m_malloc_fail_on_cnt = 86;

    ASSERT_FALSE(gw_cfg_json_generate(&gw_cfg, &json_str));
    ASSERT_EQ(nullptr, json_str.p_str);
    TEST_CHECK_LOG_RECORD(ESP_LOG_ERROR, string("Can't add json item: auto_update_interval_to"));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_TRUE(g_pTestClass->m_mem_alloc_trace.is_empty());
}

TEST_F(TestGwCfgJson, gw_cfg_json_generate_malloc_failed_on_auto_update_interval_to_2) // NOLINT
{
    const ruuvi_gateway_config_t gw_cfg   = get_gateway_config_default();
    cjson_wrap_str_t             json_str = cjson_wrap_str_null();

    cJSON_Hooks hooks = {
        .malloc_fn = &os_malloc,
        .free_fn   = &os_free_internal,
    };
    cJSON_InitHooks(&hooks);
    this->m_malloc_fail_on_cnt = 87;

    ASSERT_FALSE(gw_cfg_json_generate(&gw_cfg, &json_str));
    ASSERT_EQ(nullptr, json_str.p_str);
    TEST_CHECK_LOG_RECORD(ESP_LOG_ERROR, string("Can't add json item: auto_update_interval_to"));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_TRUE(g_pTestClass->m_mem_alloc_trace.is_empty());
}

TEST_F(TestGwCfgJson, gw_cfg_json_generate_malloc_failed_on_auto_update_tz) // NOLINT
{
    const ruuvi_gateway_config_t gw_cfg   = get_gateway_config_default();
    cjson_wrap_str_t             json_str = cjson_wrap_str_null();

    cJSON_Hooks hooks = {
        .malloc_fn = &os_malloc,
        .free_fn   = &os_free_internal,
    };
    cJSON_InitHooks(&hooks);
    this->m_malloc_fail_on_cnt = 88;

    ASSERT_FALSE(gw_cfg_json_generate(&gw_cfg, &json_str));
    ASSERT_EQ(nullptr, json_str.p_str);
    TEST_CHECK_LOG_RECORD(ESP_LOG_ERROR, string("Can't add json item: auto_update_tz_offset_hours"));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_TRUE(g_pTestClass->m_mem_alloc_trace.is_empty());
}

TEST_F(TestGwCfgJson, gw_cfg_json_generate_malloc_failed_on_auto_update_tz_2) // NOLINT
{
    const ruuvi_gateway_config_t gw_cfg   = get_gateway_config_default();
    cjson_wrap_str_t             json_str = cjson_wrap_str_null();

    cJSON_Hooks hooks = {
        .malloc_fn = &os_malloc,
        .free_fn   = &os_free_internal,
    };
    cJSON_InitHooks(&hooks);
    this->m_malloc_fail_on_cnt = 89;

    ASSERT_FALSE(gw_cfg_json_generate(&gw_cfg, &json_str));
    ASSERT_EQ(nullptr, json_str.p_str);
    TEST_CHECK_LOG_RECORD(ESP_LOG_ERROR, string("Can't add json item: auto_update_tz_offset_hours"));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_TRUE(g_pTestClass->m_mem_alloc_trace.is_empty());
}

TEST_F(TestGwCfgJson, gw_cfg_json_generate_malloc_failed_on_company_id) // NOLINT
{
    const ruuvi_gateway_config_t gw_cfg   = get_gateway_config_default();
    cjson_wrap_str_t             json_str = cjson_wrap_str_null();

    cJSON_Hooks hooks = {
        .malloc_fn = &os_malloc,
        .free_fn   = &os_free_internal,
    };
    cJSON_InitHooks(&hooks);
    this->m_malloc_fail_on_cnt = 90;

    ASSERT_FALSE(gw_cfg_json_generate(&gw_cfg, &json_str));
    ASSERT_EQ(nullptr, json_str.p_str);
    TEST_CHECK_LOG_RECORD(ESP_LOG_ERROR, string("Can't add json item: company_id"));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_TRUE(g_pTestClass->m_mem_alloc_trace.is_empty());
}

TEST_F(TestGwCfgJson, gw_cfg_json_generate_malloc_failed_on_company_id_2) // NOLINT
{
    const ruuvi_gateway_config_t gw_cfg   = get_gateway_config_default();
    cjson_wrap_str_t             json_str = cjson_wrap_str_null();

    cJSON_Hooks hooks = {
        .malloc_fn = &os_malloc,
        .free_fn   = &os_free_internal,
    };
    cJSON_InitHooks(&hooks);
    this->m_malloc_fail_on_cnt = 91;

    ASSERT_FALSE(gw_cfg_json_generate(&gw_cfg, &json_str));
    ASSERT_EQ(nullptr, json_str.p_str);
    TEST_CHECK_LOG_RECORD(ESP_LOG_ERROR, string("Can't add json item: company_id"));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_TRUE(g_pTestClass->m_mem_alloc_trace.is_empty());
}

TEST_F(TestGwCfgJson, gw_cfg_json_generate_malloc_failed_on_use_filtering) // NOLINT
{
    const ruuvi_gateway_config_t gw_cfg   = get_gateway_config_default();
    cjson_wrap_str_t             json_str = cjson_wrap_str_null();

    cJSON_Hooks hooks = {
        .malloc_fn = &os_malloc,
        .free_fn   = &os_free_internal,
    };
    cJSON_InitHooks(&hooks);
    this->m_malloc_fail_on_cnt = 92;

    ASSERT_FALSE(gw_cfg_json_generate(&gw_cfg, &json_str));
    ASSERT_EQ(nullptr, json_str.p_str);
    TEST_CHECK_LOG_RECORD(ESP_LOG_ERROR, string("Can't add json item: company_use_filtering"));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_TRUE(g_pTestClass->m_mem_alloc_trace.is_empty());
}

TEST_F(TestGwCfgJson, gw_cfg_json_generate_malloc_failed_on_use_filtering_2) // NOLINT
{
    const ruuvi_gateway_config_t gw_cfg   = get_gateway_config_default();
    cjson_wrap_str_t             json_str = cjson_wrap_str_null();

    cJSON_Hooks hooks = {
        .malloc_fn = &os_malloc,
        .free_fn   = &os_free_internal,
    };
    cJSON_InitHooks(&hooks);
    this->m_malloc_fail_on_cnt = 93;

    ASSERT_FALSE(gw_cfg_json_generate(&gw_cfg, &json_str));
    ASSERT_EQ(nullptr, json_str.p_str);
    TEST_CHECK_LOG_RECORD(ESP_LOG_ERROR, string("Can't add json item: company_use_filtering"));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_TRUE(g_pTestClass->m_mem_alloc_trace.is_empty());
}

TEST_F(TestGwCfgJson, gw_cfg_json_generate_malloc_failed_on_use_coded_phy) // NOLINT
{
    const ruuvi_gateway_config_t gw_cfg   = get_gateway_config_default();
    cjson_wrap_str_t             json_str = cjson_wrap_str_null();

    cJSON_Hooks hooks = {
        .malloc_fn = &os_malloc,
        .free_fn   = &os_free_internal,
    };
    cJSON_InitHooks(&hooks);
    this->m_malloc_fail_on_cnt = 94;

    ASSERT_FALSE(gw_cfg_json_generate(&gw_cfg, &json_str));
    ASSERT_EQ(nullptr, json_str.p_str);
    TEST_CHECK_LOG_RECORD(ESP_LOG_ERROR, string("Can't add json item: scan_coded_phy"));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_TRUE(g_pTestClass->m_mem_alloc_trace.is_empty());
}

TEST_F(TestGwCfgJson, gw_cfg_json_generate_malloc_failed_on_use_coded_phy_2) // NOLINT
{
    const ruuvi_gateway_config_t gw_cfg   = get_gateway_config_default();
    cjson_wrap_str_t             json_str = cjson_wrap_str_null();

    cJSON_Hooks hooks = {
        .malloc_fn = &os_malloc,
        .free_fn   = &os_free_internal,
    };
    cJSON_InitHooks(&hooks);
    this->m_malloc_fail_on_cnt = 95;

    ASSERT_FALSE(gw_cfg_json_generate(&gw_cfg, &json_str));
    ASSERT_EQ(nullptr, json_str.p_str);
    TEST_CHECK_LOG_RECORD(ESP_LOG_ERROR, string("Can't add json item: scan_coded_phy"));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_TRUE(g_pTestClass->m_mem_alloc_trace.is_empty());
}

TEST_F(TestGwCfgJson, gw_cfg_json_generate_malloc_failed_on_use_1mbit_phy) // NOLINT
{
    const ruuvi_gateway_config_t gw_cfg   = get_gateway_config_default();
    cjson_wrap_str_t             json_str = cjson_wrap_str_null();

    cJSON_Hooks hooks = {
        .malloc_fn = &os_malloc,
        .free_fn   = &os_free_internal,
    };
    cJSON_InitHooks(&hooks);
    this->m_malloc_fail_on_cnt = 96;

    ASSERT_FALSE(gw_cfg_json_generate(&gw_cfg, &json_str));
    ASSERT_EQ(nullptr, json_str.p_str);
    TEST_CHECK_LOG_RECORD(ESP_LOG_ERROR, string("Can't add json item: scan_1mbit_phy"));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_TRUE(g_pTestClass->m_mem_alloc_trace.is_empty());
}

TEST_F(TestGwCfgJson, gw_cfg_json_generate_malloc_failed_on_use_1mbit_phy_2) // NOLINT
{
    const ruuvi_gateway_config_t gw_cfg   = get_gateway_config_default();
    cjson_wrap_str_t             json_str = cjson_wrap_str_null();

    cJSON_Hooks hooks = {
        .malloc_fn = &os_malloc,
        .free_fn   = &os_free_internal,
    };
    cJSON_InitHooks(&hooks);
    this->m_malloc_fail_on_cnt = 97;

    ASSERT_FALSE(gw_cfg_json_generate(&gw_cfg, &json_str));
    ASSERT_EQ(nullptr, json_str.p_str);
    TEST_CHECK_LOG_RECORD(ESP_LOG_ERROR, string("Can't add json item: scan_1mbit_phy"));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_TRUE(g_pTestClass->m_mem_alloc_trace.is_empty());
}

TEST_F(TestGwCfgJson, gw_cfg_json_generate_malloc_failed_on_use_extended_payload) // NOLINT
{
    const ruuvi_gateway_config_t gw_cfg   = get_gateway_config_default();
    cjson_wrap_str_t             json_str = cjson_wrap_str_null();

    cJSON_Hooks hooks = {
        .malloc_fn = &os_malloc,
        .free_fn   = &os_free_internal,
    };
    cJSON_InitHooks(&hooks);
    this->m_malloc_fail_on_cnt = 98;

    ASSERT_FALSE(gw_cfg_json_generate(&gw_cfg, &json_str));
    ASSERT_EQ(nullptr, json_str.p_str);
    TEST_CHECK_LOG_RECORD(ESP_LOG_ERROR, string("Can't add json item: scan_extended_payload"));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_TRUE(g_pTestClass->m_mem_alloc_trace.is_empty());
}

TEST_F(TestGwCfgJson, gw_cfg_json_generate_malloc_failed_on_use_extended_payload_2) // NOLINT
{
    const ruuvi_gateway_config_t gw_cfg   = get_gateway_config_default();
    cjson_wrap_str_t             json_str = cjson_wrap_str_null();

    cJSON_Hooks hooks = {
        .malloc_fn = &os_malloc,
        .free_fn   = &os_free_internal,
    };
    cJSON_InitHooks(&hooks);
    this->m_malloc_fail_on_cnt = 99;

    ASSERT_FALSE(gw_cfg_json_generate(&gw_cfg, &json_str));
    ASSERT_EQ(nullptr, json_str.p_str);
    TEST_CHECK_LOG_RECORD(ESP_LOG_ERROR, string("Can't add json item: scan_extended_payload"));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_TRUE(g_pTestClass->m_mem_alloc_trace.is_empty());
}

TEST_F(TestGwCfgJson, gw_cfg_json_generate_malloc_failed_on_use_channel_37) // NOLINT
{
    const ruuvi_gateway_config_t gw_cfg   = get_gateway_config_default();
    cjson_wrap_str_t             json_str = cjson_wrap_str_null();

    cJSON_Hooks hooks = {
        .malloc_fn = &os_malloc,
        .free_fn   = &os_free_internal,
    };
    cJSON_InitHooks(&hooks);
    this->m_malloc_fail_on_cnt = 100;

    ASSERT_FALSE(gw_cfg_json_generate(&gw_cfg, &json_str));
    ASSERT_EQ(nullptr, json_str.p_str);
    TEST_CHECK_LOG_RECORD(ESP_LOG_ERROR, string("Can't add json item: scan_channel_37"));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_TRUE(g_pTestClass->m_mem_alloc_trace.is_empty());
}

TEST_F(TestGwCfgJson, gw_cfg_json_generate_malloc_failed_on_use_channel_37_2) // NOLINT
{
    const ruuvi_gateway_config_t gw_cfg   = get_gateway_config_default();
    cjson_wrap_str_t             json_str = cjson_wrap_str_null();

    cJSON_Hooks hooks = {
        .malloc_fn = &os_malloc,
        .free_fn   = &os_free_internal,
    };
    cJSON_InitHooks(&hooks);
    this->m_malloc_fail_on_cnt = 101;

    ASSERT_FALSE(gw_cfg_json_generate(&gw_cfg, &json_str));
    ASSERT_EQ(nullptr, json_str.p_str);
    TEST_CHECK_LOG_RECORD(ESP_LOG_ERROR, string("Can't add json item: scan_channel_37"));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_TRUE(g_pTestClass->m_mem_alloc_trace.is_empty());
}

TEST_F(TestGwCfgJson, gw_cfg_json_generate_malloc_failed_on_use_channel_38) // NOLINT
{
    const ruuvi_gateway_config_t gw_cfg   = get_gateway_config_default();
    cjson_wrap_str_t             json_str = cjson_wrap_str_null();

    cJSON_Hooks hooks = {
        .malloc_fn = &os_malloc,
        .free_fn   = &os_free_internal,
    };
    cJSON_InitHooks(&hooks);
    this->m_malloc_fail_on_cnt = 102;

    ASSERT_FALSE(gw_cfg_json_generate(&gw_cfg, &json_str));
    ASSERT_EQ(nullptr, json_str.p_str);
    TEST_CHECK_LOG_RECORD(ESP_LOG_ERROR, string("Can't add json item: scan_channel_38"));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_TRUE(g_pTestClass->m_mem_alloc_trace.is_empty());
}

TEST_F(TestGwCfgJson, gw_cfg_json_generate_malloc_failed_on_use_channel_38_2) // NOLINT
{
    const ruuvi_gateway_config_t gw_cfg   = get_gateway_config_default();
    cjson_wrap_str_t             json_str = cjson_wrap_str_null();

    cJSON_Hooks hooks = {
        .malloc_fn = &os_malloc,
        .free_fn   = &os_free_internal,
    };
    cJSON_InitHooks(&hooks);
    this->m_malloc_fail_on_cnt = 103;

    ASSERT_FALSE(gw_cfg_json_generate(&gw_cfg, &json_str));
    ASSERT_EQ(nullptr, json_str.p_str);
    TEST_CHECK_LOG_RECORD(ESP_LOG_ERROR, string("Can't add json item: scan_channel_38"));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_TRUE(g_pTestClass->m_mem_alloc_trace.is_empty());
}

TEST_F(TestGwCfgJson, gw_cfg_json_generate_malloc_failed_on_use_channel_39) // NOLINT
{
    const ruuvi_gateway_config_t gw_cfg   = get_gateway_config_default();
    cjson_wrap_str_t             json_str = cjson_wrap_str_null();

    cJSON_Hooks hooks = {
        .malloc_fn = &os_malloc,
        .free_fn   = &os_free_internal,
    };
    cJSON_InitHooks(&hooks);
    this->m_malloc_fail_on_cnt = 104;

    ASSERT_FALSE(gw_cfg_json_generate(&gw_cfg, &json_str));
    ASSERT_EQ(nullptr, json_str.p_str);
    TEST_CHECK_LOG_RECORD(ESP_LOG_ERROR, string("Can't add json item: scan_channel_39"));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_TRUE(g_pTestClass->m_mem_alloc_trace.is_empty());
}

TEST_F(TestGwCfgJson, gw_cfg_json_generate_malloc_failed_on_use_channel_39_2) // NOLINT
{
    const ruuvi_gateway_config_t gw_cfg   = get_gateway_config_default();
    cjson_wrap_str_t             json_str = cjson_wrap_str_null();

    cJSON_Hooks hooks = {
        .malloc_fn = &os_malloc,
        .free_fn   = &os_free_internal,
    };
    cJSON_InitHooks(&hooks);
    this->m_malloc_fail_on_cnt = 105;

    ASSERT_FALSE(gw_cfg_json_generate(&gw_cfg, &json_str));
    ASSERT_EQ(nullptr, json_str.p_str);
    TEST_CHECK_LOG_RECORD(ESP_LOG_ERROR, string("Can't add json item: scan_channel_39"));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_TRUE(g_pTestClass->m_mem_alloc_trace.is_empty());
}

TEST_F(TestGwCfgJson, gw_cfg_json_generate_malloc_failed_on_coordinates) // NOLINT
{
    const ruuvi_gateway_config_t gw_cfg   = get_gateway_config_default();
    cjson_wrap_str_t             json_str = cjson_wrap_str_null();

    cJSON_Hooks hooks = {
        .malloc_fn = &os_malloc,
        .free_fn   = &os_free_internal,
    };
    cJSON_InitHooks(&hooks);
    this->m_malloc_fail_on_cnt = 106;

    ASSERT_FALSE(gw_cfg_json_generate(&gw_cfg, &json_str));
    ASSERT_EQ(nullptr, json_str.p_str);
    TEST_CHECK_LOG_RECORD(ESP_LOG_ERROR, string("Can't add json item: coordinates"));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_TRUE(g_pTestClass->m_mem_alloc_trace.is_empty());
}

TEST_F(TestGwCfgJson, gw_cfg_json_generate_malloc_failed_on_coordinates_2) // NOLINT
{
    const ruuvi_gateway_config_t gw_cfg   = get_gateway_config_default();
    cjson_wrap_str_t             json_str = cjson_wrap_str_null();

    cJSON_Hooks hooks = {
        .malloc_fn = &os_malloc,
        .free_fn   = &os_free_internal,
    };
    cJSON_InitHooks(&hooks);
    this->m_malloc_fail_on_cnt = 107;

    ASSERT_FALSE(gw_cfg_json_generate(&gw_cfg, &json_str));
    ASSERT_EQ(nullptr, json_str.p_str);
    TEST_CHECK_LOG_RECORD(ESP_LOG_ERROR, string("Can't add json item: coordinates"));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_TRUE(g_pTestClass->m_mem_alloc_trace.is_empty());
}

TEST_F(TestGwCfgJson, gw_cfg_json_generate_malloc_failed_on_coordinates_3) // NOLINT
{
    const ruuvi_gateway_config_t gw_cfg   = get_gateway_config_default();
    cjson_wrap_str_t             json_str = cjson_wrap_str_null();

    cJSON_Hooks hooks = {
        .malloc_fn = &os_malloc,
        .free_fn   = &os_free_internal,
    };
    cJSON_InitHooks(&hooks);
    this->m_malloc_fail_on_cnt = 108;

    ASSERT_FALSE(gw_cfg_json_generate(&gw_cfg, &json_str));
    ASSERT_EQ(nullptr, json_str.p_str);
    TEST_CHECK_LOG_RECORD(ESP_LOG_ERROR, string("Can't add json item: coordinates"));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_TRUE(g_pTestClass->m_mem_alloc_trace.is_empty());
}

TEST_F(TestGwCfgJson, gw_cfg_json_generate_malloc_failed_on_converting_to_json_string) // NOLINT
{
    const ruuvi_gateway_config_t gw_cfg   = get_gateway_config_default();
    cjson_wrap_str_t             json_str = cjson_wrap_str_null();

    cJSON_Hooks hooks = {
        .malloc_fn = &os_malloc,
        .free_fn   = &os_free_internal,
    };
    cJSON_InitHooks(&hooks);
    this->m_malloc_fail_on_cnt = 109;

    ASSERT_FALSE(gw_cfg_json_generate(&gw_cfg, &json_str));
    ASSERT_EQ(nullptr, json_str.p_str);
    TEST_CHECK_LOG_RECORD(ESP_LOG_ERROR, string("Can't create json string"));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_TRUE(g_pTestClass->m_mem_alloc_trace.is_empty());
}
