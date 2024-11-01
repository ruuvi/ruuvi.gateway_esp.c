/**
 * @file test_gw_cfg_blob.cpp
 * @author TheSomeMan
 * @date 2021-10-05
 * @copyright Ruuvi Innovations Ltd, license BSD-3-Clause.
 */

#include <cstring>
#include "gtest/gtest.h"
#include "esp_log_wrapper.hpp"
#include "gw_cfg.h"
#include "gw_cfg_blob.h"
#include "gw_cfg_default.h"
#include "os_mutex_recursive.h"
#include "os_mutex.h"
#include "os_task.h"
#include "lwip/ip4_addr.h"
#include "event_mgr.h"
#include "gw_cfg_storage.h"

using namespace std;

/*** Google-test class implementation
 * *********************************************************************************/

class TestGwCfgBlob;
static TestGwCfgBlob* g_pTestClass;

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

class TestGwCfgBlob : public ::testing::Test
{
private:
protected:
    void
    SetUp() override
    {
        esp_log_wrapper_init();
        g_pTestClass                                  = this;
        const gw_cfg_default_init_param_t init_params = {
            .wifi_ap_ssid        = { "my_wifi1" },
            .hostname            = { "my_wifi1" },
            .device_id           = { 0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77 },
            .esp32_fw_ver        = { "v1.10.0" },
            .nrf52_fw_ver        = { "v0.7.2" },
            .nrf52_mac_addr      = { 0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF },
            .esp32_mac_addr_wifi = { 0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0x11 },
            .esp32_mac_addr_eth  = { 0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0x22 },
        };
        gw_cfg_default_init(&init_params, nullptr);
        esp_log_wrapper_clear();
    }

    void
    TearDown() override
    {
        gw_cfg_default_deinit();
        g_pTestClass = nullptr;
        esp_log_wrapper_deinit();
    }

public:
    TestGwCfgBlob();

    ~TestGwCfgBlob() override;

    bool m_flag_storage_ready { false };
    bool m_flag_storage_http_cli_cert { false };
    bool m_flag_storage_http_cli_key { false };
    bool m_flag_storage_stat_cli_cert { false };
    bool m_flag_storage_stat_cli_key { false };
    bool m_flag_storage_mqtt_cli_cert { false };
    bool m_flag_storage_mqtt_cli_key { false };
    bool m_flag_storage_remote_cfg_cli_cert { false };
    bool m_flag_storage_remote_cfg_cli_key { false };

    bool m_flag_storage_http_srv_cert { false };
    bool m_flag_storage_stat_srv_cert { false };
    bool m_flag_storage_mqtt_srv_cert { false };
    bool m_flag_storage_remote_cfg_srv_cert { false };
};

TestGwCfgBlob::TestGwCfgBlob()
    : Test()
{
}

extern "C" {

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

TestGwCfgBlob::~TestGwCfgBlob() = default;

#define TEST_CHECK_LOG_RECORD(level_, msg_) ESP_LOG_WRAPPER_TEST_CHECK_LOG_RECORD("gw_cfg", level_, msg_)

/*** Unit-Tests
 * *******************************************************************************************************/

TEST_F(TestGwCfgBlob, test_gw_cfg_blob_convert_lan_auth_ruuvi_to_default) // NOLINT
{
    const ruuvi_gw_cfg_lan_auth_t* const p_default_lan_auth = gw_cfg_default_get_lan_auth();
    const ruuvi_gateway_config_blob_t gw_cfg_blob = {
        .header = RUUVI_GATEWAY_CONFIG_BLOB_HEADER,
        .fmt_version = RUUVI_GATEWAY_CONFIG_BLOB_FMT_VERSION,
        .eth = {
            .use_eth = false,
            .eth_dhcp = true,
            .eth_static_ip = { 0 },
            .eth_netmask = { 0 },
            .eth_gw = { 0 },
            .eth_dns1 = { 0 },
            .eth_dns2 = { 0 },
        },
        .mqtt = {
            .use_mqtt = false,
            .mqtt_use_default_prefix = true,
            .mqtt_server = { 0 },
            .mqtt_port = 0,
            .mqtt_prefix = { 0 },
            .mqtt_client_id = { 0 },
            .mqtt_user = { 0 },
            .mqtt_pass = { 0 },
        },
        .http = {
            true,
            "https://network.ruuvi.com/record",
            "",
            "",
        },
        .lan_auth = {
            RUUVI_GW_CFG_BLOB_AUTH_TYPE_STR_RUUVI,
            RUUVI_GATEWAY_AUTH_DEFAULT_USER,
            "1d45bdb0aab662c03ac3fac45da43f8b",
        },
        .auto_update = {
            .auto_update_cycle = RUUVI_GW_CFG_BLOB_AUTO_UPDATE_CYCLE_TYPE_REGULAR,
            .auto_update_weekdays_bitmask = 0x7F,
            .auto_update_interval_from = 0,
            .auto_update_interval_to = 24,
            .auto_update_tz_offset_hours = 3,
        },
        .filter = {
            .company_id = RUUVI_COMPANY_ID,
            .company_filter = true,
        },
        .scan = {
            .scan_coded_phy = false,
            .scan_1mbit_phy = true,
            .scan_2mbit_phy = true,
            .scan_channel_37 = true,
            .scan_channel_38 = true,
            .scan_channel_39 = true,
        },
        .coordinates = { 0 },
    };

    gw_cfg_t gw_cfg {};
    gw_cfg_blob_convert(&gw_cfg, &gw_cfg_blob);

    ASSERT_EQ(HTTP_SERVER_AUTH_TYPE_DEFAULT, gw_cfg.ruuvi_cfg.lan_auth.lan_auth_type);
    ASSERT_EQ(string(RUUVI_GATEWAY_AUTH_DEFAULT_USER), gw_cfg.ruuvi_cfg.lan_auth.lan_auth_user.buf);
    ASSERT_EQ(string(p_default_lan_auth->lan_auth_pass.buf), gw_cfg.ruuvi_cfg.lan_auth.lan_auth_pass.buf);
    ASSERT_EQ(string(""), gw_cfg.ruuvi_cfg.lan_auth.lan_auth_api_key.buf);
}

TEST_F(TestGwCfgBlob, test_gw_cfg_blob_do_not_convert_lan_auth_ruuvi_to_default_non_default_password) // NOLINT
{
    const ruuvi_gw_cfg_lan_auth_t* const p_default_lan_auth = gw_cfg_default_get_lan_auth();
    const ruuvi_gateway_config_blob_t gw_cfg_blob = {
        .header = RUUVI_GATEWAY_CONFIG_BLOB_HEADER,
        .fmt_version = RUUVI_GATEWAY_CONFIG_BLOB_FMT_VERSION,
        .eth = {
            .use_eth = false,
            .eth_dhcp = true,
            .eth_static_ip = { 0 },
            .eth_netmask = { 0 },
            .eth_gw = { 0 },
            .eth_dns1 = { 0 },
            .eth_dns2 = { 0 },
        },
        .mqtt = {
            .use_mqtt = false,
            .mqtt_use_default_prefix = true,
            .mqtt_server = { 0 },
            .mqtt_port = 0,
            .mqtt_prefix = { 0 },
            .mqtt_client_id = { 0 },
            .mqtt_user = { 0 },
            .mqtt_pass = { 0 },
        },
        .http = {
            true,
            "https://network.ruuvi.com/record",
            "",
            "",
        },
        .lan_auth = {
            RUUVI_GW_CFG_BLOB_AUTH_TYPE_STR_RUUVI,
            RUUVI_GATEWAY_AUTH_DEFAULT_USER,
            "non_default_password",
        },
        .auto_update = {
            .auto_update_cycle = RUUVI_GW_CFG_BLOB_AUTO_UPDATE_CYCLE_TYPE_REGULAR,
            .auto_update_weekdays_bitmask = 0x7F,
            .auto_update_interval_from = 0,
            .auto_update_interval_to = 24,
            .auto_update_tz_offset_hours = 3,
        },
        .filter = {
            .company_id = RUUVI_COMPANY_ID,
            .company_filter = true,
        },
        .scan = {
            .scan_coded_phy = false,
            .scan_1mbit_phy = true,
            .scan_2mbit_phy = true,
            .scan_channel_37 = true,
            .scan_channel_38 = true,
            .scan_channel_39 = true,
        },
                  .coordinates = { 0 },
    };

    gw_cfg_t gw_cfg {};
    gw_cfg_blob_convert(&gw_cfg, &gw_cfg_blob);

    ASSERT_EQ(HTTP_SERVER_AUTH_TYPE_RUUVI, gw_cfg.ruuvi_cfg.lan_auth.lan_auth_type);
    ASSERT_EQ(string(RUUVI_GATEWAY_AUTH_DEFAULT_USER), gw_cfg.ruuvi_cfg.lan_auth.lan_auth_user.buf);
    ASSERT_EQ(string("non_default_password"), gw_cfg.ruuvi_cfg.lan_auth.lan_auth_pass.buf);
    ASSERT_EQ(string(""), gw_cfg.ruuvi_cfg.lan_auth.lan_auth_api_key.buf);
}

TEST_F(TestGwCfgBlob, test_gw_cfg_blob_do_not_convert_lan_auth_ruuvi_to_default_non_default_user) // NOLINT
{
    const ruuvi_gw_cfg_lan_auth_t* const p_default_lan_auth = gw_cfg_default_get_lan_auth();
    const ruuvi_gateway_config_blob_t gw_cfg_blob = {
        .header = RUUVI_GATEWAY_CONFIG_BLOB_HEADER,
        .fmt_version = RUUVI_GATEWAY_CONFIG_BLOB_FMT_VERSION,
        .eth = {
            .use_eth = false,
            .eth_dhcp = true,
            .eth_static_ip = { 0 },
            .eth_netmask = { 0 },
            .eth_gw = { 0 },
            .eth_dns1 = { 0 },
            .eth_dns2 = { 0 },
        },
        .mqtt = {
            .use_mqtt = false,
            .mqtt_use_default_prefix = true,
            .mqtt_server = { 0 },
            .mqtt_port = 0,
            .mqtt_prefix = { 0 },
            .mqtt_client_id = { 0 },
            .mqtt_user = { 0 },
            .mqtt_pass = { 0 },
        },
        .http = {
            true,
            "https://network.ruuvi.com/record",
            "",
            "",
        },
        .lan_auth = {
            RUUVI_GW_CFG_BLOB_AUTH_TYPE_STR_RUUVI,
            "non_default_user",
            "1d45bdb0aab662c03ac3fac45da43f8b",
        },
        .auto_update = {
            .auto_update_cycle = RUUVI_GW_CFG_BLOB_AUTO_UPDATE_CYCLE_TYPE_REGULAR,
            .auto_update_weekdays_bitmask = 0x7F,
            .auto_update_interval_from = 0,
            .auto_update_interval_to = 24,
            .auto_update_tz_offset_hours = 3,
        },
        .filter = {
            .company_id = RUUVI_COMPANY_ID,
            .company_filter = true,
        },
        .scan = {
            .scan_coded_phy = false,
            .scan_1mbit_phy = true,
            .scan_2mbit_phy = true,
            .scan_channel_37 = true,
            .scan_channel_38 = true,
            .scan_channel_39 = true,
        },
                  .coordinates = { 0 },
    };

    gw_cfg_t gw_cfg {};
    gw_cfg_blob_convert(&gw_cfg, &gw_cfg_blob);

    ASSERT_EQ(HTTP_SERVER_AUTH_TYPE_RUUVI, gw_cfg.ruuvi_cfg.lan_auth.lan_auth_type);
    ASSERT_EQ(string("non_default_user"), gw_cfg.ruuvi_cfg.lan_auth.lan_auth_user.buf);
    ASSERT_EQ(string(p_default_lan_auth->lan_auth_pass.buf), gw_cfg.ruuvi_cfg.lan_auth.lan_auth_pass.buf);
    ASSERT_EQ(string(""), gw_cfg.ruuvi_cfg.lan_auth.lan_auth_api_key.buf);
}

TEST_F(TestGwCfgBlob, test_gw_cfg_blob_convert_update_cycle_regular) // NOLINT
{
    const ruuvi_gateway_config_blob_t gw_cfg_blob = {
        .header = RUUVI_GATEWAY_CONFIG_BLOB_HEADER,
        .fmt_version = RUUVI_GATEWAY_CONFIG_BLOB_FMT_VERSION,
        .eth = {
            .use_eth = false,
            .eth_dhcp = true,
            .eth_static_ip = { 0 },
            .eth_netmask = { 0 },
            .eth_gw = { 0 },
            .eth_dns1 = { 0 },
            .eth_dns2 = { 0 },
        },
        .mqtt = {
            .use_mqtt = false,
            .mqtt_use_default_prefix = true,
            .mqtt_server = { 0 },
            .mqtt_port = 0,
            .mqtt_prefix = { 0 },
            .mqtt_client_id = { 0 },
            .mqtt_user = { 0 },
            .mqtt_pass = { 0 },
        },
        .http = {
            true,
            "https://network.ruuvi.com/record",
            "",
            "",
        },
        .lan_auth = {
            RUUVI_GW_CFG_BLOB_AUTH_TYPE_STR_RUUVI,
            RUUVI_GATEWAY_AUTH_DEFAULT_USER,
            "",
        },
        .auto_update = {
            .auto_update_cycle = RUUVI_GW_CFG_BLOB_AUTO_UPDATE_CYCLE_TYPE_REGULAR,
            .auto_update_weekdays_bitmask = 0x7F,
            .auto_update_interval_from = 0,
            .auto_update_interval_to = 24,
            .auto_update_tz_offset_hours = 3,
        },
        .filter = {
            .company_id = RUUVI_COMPANY_ID,
            .company_filter = true,
        },
        .scan = {
            .scan_coded_phy = false,
            .scan_1mbit_phy = true,
            .scan_2mbit_phy = true,
            .scan_channel_37 = true,
            .scan_channel_38 = true,
            .scan_channel_39 = true,
        },
        .coordinates = { 0 },
    };

    gw_cfg_t gw_cfg {};
    gw_cfg_blob_convert(&gw_cfg, &gw_cfg_blob);

    ASSERT_EQ(false, gw_cfg.eth_cfg.use_eth);
    ASSERT_EQ(true, gw_cfg.eth_cfg.eth_dhcp);
    ASSERT_EQ(string(""), gw_cfg.eth_cfg.eth_static_ip.buf);
    ASSERT_EQ(string(""), gw_cfg.eth_cfg.eth_netmask.buf);
    ASSERT_EQ(string(""), gw_cfg.eth_cfg.eth_gw.buf);
    ASSERT_EQ(string(""), gw_cfg.eth_cfg.eth_dns1.buf);
    ASSERT_EQ(string(""), gw_cfg.eth_cfg.eth_dns2.buf);

    ASSERT_EQ(false, gw_cfg.ruuvi_cfg.mqtt.use_mqtt);
    ASSERT_EQ(string("TCP"), gw_cfg.ruuvi_cfg.mqtt.mqtt_transport.buf);
    ASSERT_EQ(string(""), gw_cfg.ruuvi_cfg.mqtt.mqtt_server.buf);
    ASSERT_EQ(0, gw_cfg.ruuvi_cfg.mqtt.mqtt_port);
    ASSERT_EQ(string("ruuvi/AA:BB:CC:DD:EE:FF/"), gw_cfg.ruuvi_cfg.mqtt.mqtt_prefix.buf);
    ASSERT_EQ(string("AA:BB:CC:DD:EE:FF"), gw_cfg.ruuvi_cfg.mqtt.mqtt_client_id.buf);

    ASSERT_EQ(true, gw_cfg.ruuvi_cfg.http.use_http_ruuvi);
    ASSERT_EQ(false, gw_cfg.ruuvi_cfg.http.use_http);
    ASSERT_EQ(GW_CFG_HTTP_DATA_FORMAT_RUUVI, gw_cfg.ruuvi_cfg.http.data_format);
    ASSERT_EQ(GW_CFG_HTTP_AUTH_TYPE_NONE, gw_cfg.ruuvi_cfg.http.auth_type);
    ASSERT_EQ(string(RUUVI_GATEWAY_HTTP_DEFAULT_URL), gw_cfg.ruuvi_cfg.http.http_url.buf);

    ASSERT_EQ(true, gw_cfg.ruuvi_cfg.http_stat.use_http_stat);
    ASSERT_EQ(string(RUUVI_GATEWAY_HTTP_STATUS_URL), gw_cfg.ruuvi_cfg.http_stat.http_stat_url.buf);
    ASSERT_EQ(string(""), gw_cfg.ruuvi_cfg.http_stat.http_stat_user.buf);
    ASSERT_EQ(string(""), gw_cfg.ruuvi_cfg.http_stat.http_stat_pass.buf);

    ASSERT_EQ(HTTP_SERVER_AUTH_TYPE_RUUVI, gw_cfg.ruuvi_cfg.lan_auth.lan_auth_type);
    ASSERT_EQ(string(RUUVI_GATEWAY_AUTH_DEFAULT_USER), gw_cfg.ruuvi_cfg.lan_auth.lan_auth_user.buf);
    ASSERT_EQ(string(""), gw_cfg.ruuvi_cfg.lan_auth.lan_auth_pass.buf);
    ASSERT_EQ(string(""), gw_cfg.ruuvi_cfg.lan_auth.lan_auth_api_key.buf);

    ASSERT_EQ(AUTO_UPDATE_CYCLE_TYPE_REGULAR, gw_cfg.ruuvi_cfg.auto_update.auto_update_cycle);
    ASSERT_EQ(0x7F, gw_cfg.ruuvi_cfg.auto_update.auto_update_weekdays_bitmask);
    ASSERT_EQ(0, gw_cfg.ruuvi_cfg.auto_update.auto_update_interval_from);
    ASSERT_EQ(24, gw_cfg.ruuvi_cfg.auto_update.auto_update_interval_to);
    ASSERT_EQ(3, gw_cfg.ruuvi_cfg.auto_update.auto_update_tz_offset_hours);

    ASSERT_EQ(RUUVI_COMPANY_ID, gw_cfg.ruuvi_cfg.filter.company_id);
    ASSERT_EQ(true, gw_cfg.ruuvi_cfg.filter.company_use_filtering);

    ASSERT_EQ(false, gw_cfg.ruuvi_cfg.scan.scan_coded_phy);
    ASSERT_EQ(true, gw_cfg.ruuvi_cfg.scan.scan_1mbit_phy);
    ASSERT_EQ(true, gw_cfg.ruuvi_cfg.scan.scan_2mbit_phy);
    ASSERT_EQ(true, gw_cfg.ruuvi_cfg.scan.scan_channel_37);
    ASSERT_EQ(true, gw_cfg.ruuvi_cfg.scan.scan_channel_38);
    ASSERT_EQ(true, gw_cfg.ruuvi_cfg.scan.scan_channel_39);

    ASSERT_EQ(string(""), gw_cfg.ruuvi_cfg.coordinates.buf);
}

TEST_F(TestGwCfgBlob, test_gw_cfg_blob_convert_update_cycle_beta) // NOLINT
{
    const ruuvi_gateway_config_blob_t gw_cfg_blob = {
        .header = RUUVI_GATEWAY_CONFIG_BLOB_HEADER,
        .fmt_version = RUUVI_GATEWAY_CONFIG_BLOB_FMT_VERSION,
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
            "test.mosquitto.org",
            1883,
            "prefix1",
            "my_client",
            "m_user1",
            "m_pass1",
        },
        .http = {
            true,
            "https://myserver.com/record",
            "h_user1",
            "h_pass1",
        },
        .lan_auth = {
            RUUVI_GW_CFG_BLOB_AUTH_TYPE_STR_DIGEST,
            "l_user1",
            "l_pass1",
        },
        .auto_update = {
            .auto_update_cycle = RUUVI_GW_CFG_BLOB_AUTO_UPDATE_CYCLE_TYPE_BETA_TESTER,
            .auto_update_weekdays_bitmask = 0x0F,
            .auto_update_interval_from = 1,
            .auto_update_interval_to = 23,
            .auto_update_tz_offset_hours = 2,
        },
        .filter = {
            .company_id = RUUVI_COMPANY_ID - 1,
            .company_filter = false,
        },
        .scan = {
            .scan_coded_phy = true,
            .scan_1mbit_phy = false,
            .scan_2mbit_phy = false,
            .scan_channel_37 = false,
            .scan_channel_38 = false,
            .scan_channel_39 = false,
        },
        .coordinates = { 'q', 'w', 'e', '\0', },
    };

    gw_cfg_t gw_cfg {};
    gw_cfg_blob_convert(&gw_cfg, &gw_cfg_blob);

    ASSERT_EQ(true, gw_cfg.eth_cfg.use_eth);
    ASSERT_EQ(false, gw_cfg.eth_cfg.eth_dhcp);
    ASSERT_EQ(string("192.168.1.10"), gw_cfg.eth_cfg.eth_static_ip.buf);
    ASSERT_EQ(string("255.255.255.0"), gw_cfg.eth_cfg.eth_netmask.buf);
    ASSERT_EQ(string("192.168.1.1"), gw_cfg.eth_cfg.eth_gw.buf);
    ASSERT_EQ(string("8.8.8.8"), gw_cfg.eth_cfg.eth_dns1.buf);
    ASSERT_EQ(string("4.4.4.4"), gw_cfg.eth_cfg.eth_dns2.buf);

    ASSERT_EQ(true, gw_cfg.ruuvi_cfg.mqtt.use_mqtt);
    ASSERT_EQ(string("TCP"), gw_cfg.ruuvi_cfg.mqtt.mqtt_transport.buf);
    ASSERT_EQ(string("test.mosquitto.org"), gw_cfg.ruuvi_cfg.mqtt.mqtt_server.buf);
    ASSERT_EQ(1883, gw_cfg.ruuvi_cfg.mqtt.mqtt_port);
    ASSERT_EQ(string("prefix1"), gw_cfg.ruuvi_cfg.mqtt.mqtt_prefix.buf);
    ASSERT_EQ(string("my_client"), gw_cfg.ruuvi_cfg.mqtt.mqtt_client_id.buf);
    ASSERT_EQ(string("m_user1"), gw_cfg.ruuvi_cfg.mqtt.mqtt_user.buf);
    ASSERT_EQ(string("m_pass1"), gw_cfg.ruuvi_cfg.mqtt.mqtt_pass.buf);

    ASSERT_EQ(false, gw_cfg.ruuvi_cfg.http.use_http_ruuvi);
    ASSERT_EQ(true, gw_cfg.ruuvi_cfg.http.use_http);
    ASSERT_EQ(GW_CFG_HTTP_DATA_FORMAT_RUUVI, gw_cfg.ruuvi_cfg.http.data_format);
    ASSERT_EQ(GW_CFG_HTTP_AUTH_TYPE_BASIC, gw_cfg.ruuvi_cfg.http.auth_type);
    ASSERT_EQ(string("https://myserver.com/record"), gw_cfg.ruuvi_cfg.http.http_url.buf);
    ASSERT_EQ(string("h_user1"), gw_cfg.ruuvi_cfg.http.auth.auth_basic.user.buf);
    ASSERT_EQ(string("h_pass1"), gw_cfg.ruuvi_cfg.http.auth.auth_basic.password.buf);

    ASSERT_EQ(true, gw_cfg.ruuvi_cfg.http_stat.use_http_stat);
    ASSERT_EQ(string(RUUVI_GATEWAY_HTTP_STATUS_URL), gw_cfg.ruuvi_cfg.http_stat.http_stat_url.buf);
    ASSERT_EQ(string(""), gw_cfg.ruuvi_cfg.http_stat.http_stat_user.buf);
    ASSERT_EQ(string(""), gw_cfg.ruuvi_cfg.http_stat.http_stat_pass.buf);

    ASSERT_EQ(HTTP_SERVER_AUTH_TYPE_DIGEST, gw_cfg.ruuvi_cfg.lan_auth.lan_auth_type);
    ASSERT_EQ(string("l_user1"), gw_cfg.ruuvi_cfg.lan_auth.lan_auth_user.buf);
    ASSERT_EQ(string("l_pass1"), gw_cfg.ruuvi_cfg.lan_auth.lan_auth_pass.buf);
    ASSERT_EQ(string(""), gw_cfg.ruuvi_cfg.lan_auth.lan_auth_api_key.buf);

    ASSERT_EQ(AUTO_UPDATE_CYCLE_TYPE_BETA_TESTER, gw_cfg.ruuvi_cfg.auto_update.auto_update_cycle);
    ASSERT_EQ(0x0F, gw_cfg.ruuvi_cfg.auto_update.auto_update_weekdays_bitmask);
    ASSERT_EQ(1, gw_cfg.ruuvi_cfg.auto_update.auto_update_interval_from);
    ASSERT_EQ(23, gw_cfg.ruuvi_cfg.auto_update.auto_update_interval_to);
    ASSERT_EQ(2, gw_cfg.ruuvi_cfg.auto_update.auto_update_tz_offset_hours);

    ASSERT_EQ(RUUVI_COMPANY_ID - 1, gw_cfg.ruuvi_cfg.filter.company_id);
    ASSERT_EQ(false, gw_cfg.ruuvi_cfg.filter.company_use_filtering);

    ASSERT_EQ(true, gw_cfg.ruuvi_cfg.scan.scan_coded_phy);
    ASSERT_EQ(false, gw_cfg.ruuvi_cfg.scan.scan_1mbit_phy);
    ASSERT_EQ(false, gw_cfg.ruuvi_cfg.scan.scan_2mbit_phy);
    ASSERT_EQ(false, gw_cfg.ruuvi_cfg.scan.scan_channel_37);
    ASSERT_EQ(false, gw_cfg.ruuvi_cfg.scan.scan_channel_38);
    ASSERT_EQ(false, gw_cfg.ruuvi_cfg.scan.scan_channel_39);

    ASSERT_EQ(string("qwe"), gw_cfg.ruuvi_cfg.coordinates.buf);
}

TEST_F(TestGwCfgBlob, test_gw_cfg_blob_convert_update_cycle_manual) // NOLINT
{
    const ruuvi_gateway_config_blob_t gw_cfg_blob = {
        .header = RUUVI_GATEWAY_CONFIG_BLOB_HEADER,
        .fmt_version = RUUVI_GATEWAY_CONFIG_BLOB_FMT_VERSION,
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
            "test.mosquitto.org",
            1883,
            "prefix1",
            "my_client",
            "m_user1",
            "m_pass1",
        },
        .http = {
            false,
            "https://myserver.com/record",
            "h_user1",
            "h_pass1",
        },
        .lan_auth = {
            RUUVI_GW_CFG_BLOB_AUTH_TYPE_STR_DIGEST,
            "l_user1",
            "l_pass1",
        },
        .auto_update = {
            .auto_update_cycle = RUUVI_GW_CFG_BLOB_AUTO_UPDATE_CYCLE_TYPE_MANUAL,
            .auto_update_weekdays_bitmask = 0x0F,
            .auto_update_interval_from = 1,
            .auto_update_interval_to = 23,
            .auto_update_tz_offset_hours = 2,
        },
        .filter = {
            .company_id = RUUVI_COMPANY_ID - 1,
            .company_filter = false,
        },
        .scan = {
            .scan_coded_phy = true,
            .scan_1mbit_phy = false,
            .scan_2mbit_phy = false,
            .scan_channel_37 = false,
            .scan_channel_38 = false,
            .scan_channel_39 = false,
        },
        .coordinates = { 'q', 'w', 'e', '\0', },
    };

    gw_cfg_t gw_cfg {};
    gw_cfg_blob_convert(&gw_cfg, &gw_cfg_blob);

    ASSERT_EQ(true, gw_cfg.eth_cfg.use_eth);
    ASSERT_EQ(false, gw_cfg.eth_cfg.eth_dhcp);
    ASSERT_EQ(string("192.168.1.10"), gw_cfg.eth_cfg.eth_static_ip.buf);
    ASSERT_EQ(string("255.255.255.0"), gw_cfg.eth_cfg.eth_netmask.buf);
    ASSERT_EQ(string("192.168.1.1"), gw_cfg.eth_cfg.eth_gw.buf);
    ASSERT_EQ(string("8.8.8.8"), gw_cfg.eth_cfg.eth_dns1.buf);
    ASSERT_EQ(string("4.4.4.4"), gw_cfg.eth_cfg.eth_dns2.buf);

    ASSERT_EQ(true, gw_cfg.ruuvi_cfg.mqtt.use_mqtt);
    ASSERT_EQ(string("TCP"), gw_cfg.ruuvi_cfg.mqtt.mqtt_transport.buf);
    ASSERT_EQ(string("test.mosquitto.org"), gw_cfg.ruuvi_cfg.mqtt.mqtt_server.buf);
    ASSERT_EQ(1883, gw_cfg.ruuvi_cfg.mqtt.mqtt_port);
    ASSERT_EQ(string("prefix1"), gw_cfg.ruuvi_cfg.mqtt.mqtt_prefix.buf);
    ASSERT_EQ(string("my_client"), gw_cfg.ruuvi_cfg.mqtt.mqtt_client_id.buf);
    ASSERT_EQ(string("m_user1"), gw_cfg.ruuvi_cfg.mqtt.mqtt_user.buf);
    ASSERT_EQ(string("m_pass1"), gw_cfg.ruuvi_cfg.mqtt.mqtt_pass.buf);

    ASSERT_EQ(false, gw_cfg.ruuvi_cfg.http.use_http_ruuvi);
    ASSERT_EQ(false, gw_cfg.ruuvi_cfg.http.use_http);

    ASSERT_EQ(true, gw_cfg.ruuvi_cfg.http_stat.use_http_stat);
    ASSERT_EQ(string(RUUVI_GATEWAY_HTTP_STATUS_URL), gw_cfg.ruuvi_cfg.http_stat.http_stat_url.buf);
    ASSERT_EQ(string(""), gw_cfg.ruuvi_cfg.http_stat.http_stat_user.buf);
    ASSERT_EQ(string(""), gw_cfg.ruuvi_cfg.http_stat.http_stat_pass.buf);

    ASSERT_EQ(HTTP_SERVER_AUTH_TYPE_DIGEST, gw_cfg.ruuvi_cfg.lan_auth.lan_auth_type);
    ASSERT_EQ(string("l_user1"), gw_cfg.ruuvi_cfg.lan_auth.lan_auth_user.buf);
    ASSERT_EQ(string("l_pass1"), gw_cfg.ruuvi_cfg.lan_auth.lan_auth_pass.buf);
    ASSERT_EQ(string(""), gw_cfg.ruuvi_cfg.lan_auth.lan_auth_api_key.buf);

    ASSERT_EQ(AUTO_UPDATE_CYCLE_TYPE_MANUAL, gw_cfg.ruuvi_cfg.auto_update.auto_update_cycle);
    ASSERT_EQ(0x0F, gw_cfg.ruuvi_cfg.auto_update.auto_update_weekdays_bitmask);
    ASSERT_EQ(1, gw_cfg.ruuvi_cfg.auto_update.auto_update_interval_from);
    ASSERT_EQ(23, gw_cfg.ruuvi_cfg.auto_update.auto_update_interval_to);
    ASSERT_EQ(2, gw_cfg.ruuvi_cfg.auto_update.auto_update_tz_offset_hours);

    ASSERT_EQ(RUUVI_COMPANY_ID - 1, gw_cfg.ruuvi_cfg.filter.company_id);
    ASSERT_EQ(false, gw_cfg.ruuvi_cfg.filter.company_use_filtering);

    ASSERT_EQ(true, gw_cfg.ruuvi_cfg.scan.scan_coded_phy);
    ASSERT_EQ(false, gw_cfg.ruuvi_cfg.scan.scan_1mbit_phy);
    ASSERT_EQ(false, gw_cfg.ruuvi_cfg.scan.scan_2mbit_phy);
    ASSERT_EQ(false, gw_cfg.ruuvi_cfg.scan.scan_channel_37);
    ASSERT_EQ(false, gw_cfg.ruuvi_cfg.scan.scan_channel_38);
    ASSERT_EQ(false, gw_cfg.ruuvi_cfg.scan.scan_channel_39);

    ASSERT_EQ(string("qwe"), gw_cfg.ruuvi_cfg.coordinates.buf);
}

TEST_F(TestGwCfgBlob, test_gw_cfg_blob_convert_update_cycle_unknown) // NOLINT
{
    const ruuvi_gateway_config_blob_t gw_cfg_blob = {
        .header = RUUVI_GATEWAY_CONFIG_BLOB_HEADER,
        .fmt_version = RUUVI_GATEWAY_CONFIG_BLOB_FMT_VERSION,
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
            "test.mosquitto.org",
            1883,
            "prefix1",
            "my_client",
            "m_user1",
            "m_pass1",
        },
        .http = {
            false,
            "https://myserver.com/record",
            "h_user1",
            "h_pass1",
        },
        .lan_auth = {
            RUUVI_GW_CFG_BLOB_AUTH_TYPE_STR_DIGEST,
            "l_user1",
            "l_pass1",
        },
        .auto_update = {
            .auto_update_cycle = (ruuvi_gw_cfg_blob_auto_update_cycle_type_e)-1,
            .auto_update_weekdays_bitmask = 0x0F,
            .auto_update_interval_from = 1,
            .auto_update_interval_to = 23,
            .auto_update_tz_offset_hours = 2,
        },
        .filter = {
            .company_id = RUUVI_COMPANY_ID - 1,
            .company_filter = false,
        },
        .scan = {
            .scan_coded_phy = true,
            .scan_1mbit_phy = false,
            .scan_2mbit_phy = false,
            .scan_channel_37 = false,
            .scan_channel_38 = false,
            .scan_channel_39 = false,
        },
        .coordinates = { 'q', 'w', 'e', '\0', },
    };

    gw_cfg_t gw_cfg {};
    gw_cfg_blob_convert(&gw_cfg, &gw_cfg_blob);

    ASSERT_EQ(true, gw_cfg.eth_cfg.use_eth);
    ASSERT_EQ(false, gw_cfg.eth_cfg.eth_dhcp);
    ASSERT_EQ(string("192.168.1.10"), gw_cfg.eth_cfg.eth_static_ip.buf);
    ASSERT_EQ(string("255.255.255.0"), gw_cfg.eth_cfg.eth_netmask.buf);
    ASSERT_EQ(string("192.168.1.1"), gw_cfg.eth_cfg.eth_gw.buf);
    ASSERT_EQ(string("8.8.8.8"), gw_cfg.eth_cfg.eth_dns1.buf);
    ASSERT_EQ(string("4.4.4.4"), gw_cfg.eth_cfg.eth_dns2.buf);

    ASSERT_EQ(true, gw_cfg.ruuvi_cfg.mqtt.use_mqtt);
    ASSERT_EQ(string("TCP"), gw_cfg.ruuvi_cfg.mqtt.mqtt_transport.buf);
    ASSERT_EQ(string("test.mosquitto.org"), gw_cfg.ruuvi_cfg.mqtt.mqtt_server.buf);
    ASSERT_EQ(1883, gw_cfg.ruuvi_cfg.mqtt.mqtt_port);
    ASSERT_EQ(string("prefix1"), gw_cfg.ruuvi_cfg.mqtt.mqtt_prefix.buf);
    ASSERT_EQ(string("my_client"), gw_cfg.ruuvi_cfg.mqtt.mqtt_client_id.buf);
    ASSERT_EQ(string("m_user1"), gw_cfg.ruuvi_cfg.mqtt.mqtt_user.buf);
    ASSERT_EQ(string("m_pass1"), gw_cfg.ruuvi_cfg.mqtt.mqtt_pass.buf);

    ASSERT_EQ(false, gw_cfg.ruuvi_cfg.http.use_http_ruuvi);
    ASSERT_EQ(false, gw_cfg.ruuvi_cfg.http.use_http);

    ASSERT_EQ(true, gw_cfg.ruuvi_cfg.http_stat.use_http_stat);
    ASSERT_EQ(string(RUUVI_GATEWAY_HTTP_STATUS_URL), gw_cfg.ruuvi_cfg.http_stat.http_stat_url.buf);
    ASSERT_EQ(string(""), gw_cfg.ruuvi_cfg.http_stat.http_stat_user.buf);
    ASSERT_EQ(string(""), gw_cfg.ruuvi_cfg.http_stat.http_stat_pass.buf);

    ASSERT_EQ(HTTP_SERVER_AUTH_TYPE_DIGEST, gw_cfg.ruuvi_cfg.lan_auth.lan_auth_type);
    ASSERT_EQ(string("l_user1"), gw_cfg.ruuvi_cfg.lan_auth.lan_auth_user.buf);
    ASSERT_EQ(string("l_pass1"), gw_cfg.ruuvi_cfg.lan_auth.lan_auth_pass.buf);
    ASSERT_EQ(string(""), gw_cfg.ruuvi_cfg.lan_auth.lan_auth_api_key.buf);

    ASSERT_EQ(AUTO_UPDATE_CYCLE_TYPE_REGULAR, gw_cfg.ruuvi_cfg.auto_update.auto_update_cycle);
    ASSERT_EQ(0x0F, gw_cfg.ruuvi_cfg.auto_update.auto_update_weekdays_bitmask);
    ASSERT_EQ(1, gw_cfg.ruuvi_cfg.auto_update.auto_update_interval_from);
    ASSERT_EQ(23, gw_cfg.ruuvi_cfg.auto_update.auto_update_interval_to);
    ASSERT_EQ(2, gw_cfg.ruuvi_cfg.auto_update.auto_update_tz_offset_hours);

    ASSERT_EQ(RUUVI_COMPANY_ID - 1, gw_cfg.ruuvi_cfg.filter.company_id);
    ASSERT_EQ(false, gw_cfg.ruuvi_cfg.filter.company_use_filtering);

    ASSERT_EQ(true, gw_cfg.ruuvi_cfg.scan.scan_coded_phy);
    ASSERT_EQ(false, gw_cfg.ruuvi_cfg.scan.scan_1mbit_phy);
    ASSERT_EQ(false, gw_cfg.ruuvi_cfg.scan.scan_2mbit_phy);
    ASSERT_EQ(false, gw_cfg.ruuvi_cfg.scan.scan_channel_37);
    ASSERT_EQ(false, gw_cfg.ruuvi_cfg.scan.scan_channel_38);
    ASSERT_EQ(false, gw_cfg.ruuvi_cfg.scan.scan_channel_39);

    ASSERT_EQ(string("qwe"), gw_cfg.ruuvi_cfg.coordinates.buf);
}

TEST_F(TestGwCfgBlob, test_gw_cfg_blob_convert_with_incorrect_header) // NOLINT
{
    const ruuvi_gateway_config_blob_t gw_cfg_blob = {
        .header = RUUVI_GATEWAY_CONFIG_BLOB_HEADER + 1,
        .fmt_version = RUUVI_GATEWAY_CONFIG_BLOB_FMT_VERSION,
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
            "test.mosquitto.org",
            1883,
            "prefix1",
            "my_client",
            "m_user1",
            "m_pass1",
        },
        .http = {
            false,
            "https://myserver.com/record",
            "h_user1",
            "h_pass1",
        },
        .lan_auth = {
            RUUVI_GW_CFG_BLOB_AUTH_TYPE_STR_DIGEST,
            "l_user1",
            "l_pass1",
        },
        .auto_update = {
            .auto_update_cycle = RUUVI_GW_CFG_BLOB_AUTO_UPDATE_CYCLE_TYPE_BETA_TESTER,
            .auto_update_weekdays_bitmask = 0x0F,
            .auto_update_interval_from = 1,
            .auto_update_interval_to = 23,
            .auto_update_tz_offset_hours = 2,
        },
        .filter = {
            .company_id = RUUVI_COMPANY_ID - 1,
            .company_filter = false,
        },
        .scan = {
            .scan_coded_phy = true,
            .scan_1mbit_phy = false,
            .scan_2mbit_phy = false,
            .scan_channel_37 = false,
            .scan_channel_38 = false,
            .scan_channel_39 = false,
        },
        .coordinates = { 'q', 'w', 'e', '\0', },
    };

    gw_cfg_t gw_cfg {};
    gw_cfg_blob_convert(&gw_cfg, &gw_cfg_blob);

    ASSERT_EQ(true, gw_cfg.eth_cfg.use_eth);
    ASSERT_EQ(true, gw_cfg.eth_cfg.eth_dhcp);
    ASSERT_EQ(string(""), gw_cfg.eth_cfg.eth_static_ip.buf);
    ASSERT_EQ(string(""), gw_cfg.eth_cfg.eth_netmask.buf);
    ASSERT_EQ(string(""), gw_cfg.eth_cfg.eth_gw.buf);
    ASSERT_EQ(string(""), gw_cfg.eth_cfg.eth_dns1.buf);
    ASSERT_EQ(string(""), gw_cfg.eth_cfg.eth_dns2.buf);

    ASSERT_EQ(false, gw_cfg.ruuvi_cfg.mqtt.use_mqtt);
    ASSERT_EQ(string("TCP"), gw_cfg.ruuvi_cfg.mqtt.mqtt_transport.buf);
    ASSERT_EQ(string("test.mosquitto.org"), gw_cfg.ruuvi_cfg.mqtt.mqtt_server.buf);
    ASSERT_EQ(1883, gw_cfg.ruuvi_cfg.mqtt.mqtt_port);
    ASSERT_EQ(string("ruuvi/AA:BB:CC:DD:EE:FF/"), gw_cfg.ruuvi_cfg.mqtt.mqtt_prefix.buf);
    ASSERT_EQ(string("AA:BB:CC:DD:EE:FF"), gw_cfg.ruuvi_cfg.mqtt.mqtt_client_id.buf);
    ASSERT_EQ(string(""), gw_cfg.ruuvi_cfg.mqtt.mqtt_user.buf);
    ASSERT_EQ(string(""), gw_cfg.ruuvi_cfg.mqtt.mqtt_pass.buf);

    ASSERT_EQ(true, gw_cfg.ruuvi_cfg.http.use_http_ruuvi);
    ASSERT_EQ(false, gw_cfg.ruuvi_cfg.http.use_http);
    ASSERT_EQ(GW_CFG_HTTP_DATA_FORMAT_RUUVI, gw_cfg.ruuvi_cfg.http.data_format);
    ASSERT_EQ(GW_CFG_HTTP_AUTH_TYPE_NONE, gw_cfg.ruuvi_cfg.http.auth_type);
    ASSERT_EQ(string(RUUVI_GATEWAY_HTTP_DEFAULT_URL), gw_cfg.ruuvi_cfg.http.http_url.buf);

    ASSERT_EQ(true, gw_cfg.ruuvi_cfg.http_stat.use_http_stat);
    ASSERT_EQ(string(RUUVI_GATEWAY_HTTP_STATUS_URL), gw_cfg.ruuvi_cfg.http_stat.http_stat_url.buf);
    ASSERT_EQ(string(""), gw_cfg.ruuvi_cfg.http_stat.http_stat_user.buf);
    ASSERT_EQ(string(""), gw_cfg.ruuvi_cfg.http_stat.http_stat_pass.buf);

    ASSERT_EQ(HTTP_SERVER_AUTH_TYPE_DEFAULT, gw_cfg.ruuvi_cfg.lan_auth.lan_auth_type);
    ASSERT_EQ(string(RUUVI_GATEWAY_AUTH_DEFAULT_USER), gw_cfg.ruuvi_cfg.lan_auth.lan_auth_user.buf);
    ASSERT_EQ(string("1d45bdb0aab662c03ac3fac45da43f8b"), gw_cfg.ruuvi_cfg.lan_auth.lan_auth_pass.buf);
    ASSERT_EQ(string(""), gw_cfg.ruuvi_cfg.lan_auth.lan_auth_api_key.buf);

    ASSERT_EQ(AUTO_UPDATE_CYCLE_TYPE_REGULAR, gw_cfg.ruuvi_cfg.auto_update.auto_update_cycle);
    ASSERT_EQ(0x7F, gw_cfg.ruuvi_cfg.auto_update.auto_update_weekdays_bitmask);
    ASSERT_EQ(0, gw_cfg.ruuvi_cfg.auto_update.auto_update_interval_from);
    ASSERT_EQ(24, gw_cfg.ruuvi_cfg.auto_update.auto_update_interval_to);
    ASSERT_EQ(3, gw_cfg.ruuvi_cfg.auto_update.auto_update_tz_offset_hours);

    ASSERT_EQ(RUUVI_COMPANY_ID, gw_cfg.ruuvi_cfg.filter.company_id);
    ASSERT_EQ(true, gw_cfg.ruuvi_cfg.filter.company_use_filtering);

    ASSERT_EQ(false, gw_cfg.ruuvi_cfg.scan.scan_coded_phy);
    ASSERT_EQ(true, gw_cfg.ruuvi_cfg.scan.scan_1mbit_phy);
    ASSERT_EQ(true, gw_cfg.ruuvi_cfg.scan.scan_2mbit_phy);
    ASSERT_EQ(true, gw_cfg.ruuvi_cfg.scan.scan_channel_37);
    ASSERT_EQ(true, gw_cfg.ruuvi_cfg.scan.scan_channel_38);
    ASSERT_EQ(true, gw_cfg.ruuvi_cfg.scan.scan_channel_39);

    ASSERT_EQ(string(""), gw_cfg.ruuvi_cfg.coordinates.buf);
}

TEST_F(TestGwCfgBlob, test_gw_cfg_blob_convert_with_incorrect_fmt_version) // NOLINT
{
    const ruuvi_gateway_config_blob_t gw_cfg_blob = {
        .header = RUUVI_GATEWAY_CONFIG_BLOB_HEADER,
        .fmt_version = RUUVI_GATEWAY_CONFIG_BLOB_FMT_VERSION - 1,
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
            "test.mosquitto.org",
            1883,
            "prefix1",
            "my_client",
            "m_user1",
            "m_pass1",
        },
        .http = {
            false,
            "https://myserver.com/record",
            "h_user1",
            "h_pass1",
        },
        .lan_auth = {
            RUUVI_GW_CFG_BLOB_AUTH_TYPE_STR_DIGEST,
            "l_user1",
            "l_pass1",
        },
        .auto_update = {
            .auto_update_cycle = RUUVI_GW_CFG_BLOB_AUTO_UPDATE_CYCLE_TYPE_BETA_TESTER,
            .auto_update_weekdays_bitmask = 0x0F,
            .auto_update_interval_from = 1,
            .auto_update_interval_to = 23,
            .auto_update_tz_offset_hours = 2,
        },
        .filter = {
            .company_id = RUUVI_COMPANY_ID - 1,
            .company_filter = false,
        },
        .scan = {
            .scan_coded_phy = true,
            .scan_1mbit_phy = false,
            .scan_2mbit_phy = false,
            .scan_channel_37 = false,
            .scan_channel_38 = false,
            .scan_channel_39 = false,
        },
        .coordinates = { 'q', 'w', 'e', '\0', },
    };

    gw_cfg_t gw_cfg {};
    gw_cfg_blob_convert(&gw_cfg, &gw_cfg_blob);

    ASSERT_EQ(true, gw_cfg.eth_cfg.use_eth);
    ASSERT_EQ(true, gw_cfg.eth_cfg.eth_dhcp);
    ASSERT_EQ(string(""), gw_cfg.eth_cfg.eth_static_ip.buf);
    ASSERT_EQ(string(""), gw_cfg.eth_cfg.eth_netmask.buf);
    ASSERT_EQ(string(""), gw_cfg.eth_cfg.eth_gw.buf);
    ASSERT_EQ(string(""), gw_cfg.eth_cfg.eth_dns1.buf);
    ASSERT_EQ(string(""), gw_cfg.eth_cfg.eth_dns2.buf);

    ASSERT_EQ(false, gw_cfg.ruuvi_cfg.mqtt.use_mqtt);
    ASSERT_EQ(string("TCP"), gw_cfg.ruuvi_cfg.mqtt.mqtt_transport.buf);
    ASSERT_EQ(string("test.mosquitto.org"), gw_cfg.ruuvi_cfg.mqtt.mqtt_server.buf);
    ASSERT_EQ(1883, gw_cfg.ruuvi_cfg.mqtt.mqtt_port);
    ASSERT_EQ(string("ruuvi/AA:BB:CC:DD:EE:FF/"), gw_cfg.ruuvi_cfg.mqtt.mqtt_prefix.buf);
    ASSERT_EQ(string("AA:BB:CC:DD:EE:FF"), gw_cfg.ruuvi_cfg.mqtt.mqtt_client_id.buf);
    ASSERT_EQ(string(""), gw_cfg.ruuvi_cfg.mqtt.mqtt_user.buf);
    ASSERT_EQ(string(""), gw_cfg.ruuvi_cfg.mqtt.mqtt_pass.buf);

    ASSERT_EQ(true, gw_cfg.ruuvi_cfg.http.use_http_ruuvi);
    ASSERT_EQ(false, gw_cfg.ruuvi_cfg.http.use_http);
    ASSERT_EQ(GW_CFG_HTTP_DATA_FORMAT_RUUVI, gw_cfg.ruuvi_cfg.http.data_format);
    ASSERT_EQ(GW_CFG_HTTP_AUTH_TYPE_NONE, gw_cfg.ruuvi_cfg.http.auth_type);
    ASSERT_EQ(string(RUUVI_GATEWAY_HTTP_DEFAULT_URL), gw_cfg.ruuvi_cfg.http.http_url.buf);

    ASSERT_EQ(true, gw_cfg.ruuvi_cfg.http_stat.use_http_stat);
    ASSERT_EQ(string(RUUVI_GATEWAY_HTTP_STATUS_URL), gw_cfg.ruuvi_cfg.http_stat.http_stat_url.buf);
    ASSERT_EQ(string(""), gw_cfg.ruuvi_cfg.http_stat.http_stat_user.buf);
    ASSERT_EQ(string(""), gw_cfg.ruuvi_cfg.http_stat.http_stat_pass.buf);

    ASSERT_EQ(HTTP_SERVER_AUTH_TYPE_DEFAULT, gw_cfg.ruuvi_cfg.lan_auth.lan_auth_type);
    ASSERT_EQ(string(RUUVI_GATEWAY_AUTH_DEFAULT_USER), gw_cfg.ruuvi_cfg.lan_auth.lan_auth_user.buf);
    ASSERT_EQ(string("1d45bdb0aab662c03ac3fac45da43f8b"), gw_cfg.ruuvi_cfg.lan_auth.lan_auth_pass.buf);
    ASSERT_EQ(string(""), gw_cfg.ruuvi_cfg.lan_auth.lan_auth_api_key.buf);

    ASSERT_EQ(AUTO_UPDATE_CYCLE_TYPE_REGULAR, gw_cfg.ruuvi_cfg.auto_update.auto_update_cycle);
    ASSERT_EQ(0x7F, gw_cfg.ruuvi_cfg.auto_update.auto_update_weekdays_bitmask);
    ASSERT_EQ(0, gw_cfg.ruuvi_cfg.auto_update.auto_update_interval_from);
    ASSERT_EQ(24, gw_cfg.ruuvi_cfg.auto_update.auto_update_interval_to);
    ASSERT_EQ(3, gw_cfg.ruuvi_cfg.auto_update.auto_update_tz_offset_hours);

    ASSERT_EQ(RUUVI_COMPANY_ID, gw_cfg.ruuvi_cfg.filter.company_id);
    ASSERT_EQ(true, gw_cfg.ruuvi_cfg.filter.company_use_filtering);

    ASSERT_EQ(false, gw_cfg.ruuvi_cfg.scan.scan_coded_phy);
    ASSERT_EQ(true, gw_cfg.ruuvi_cfg.scan.scan_1mbit_phy);
    ASSERT_EQ(true, gw_cfg.ruuvi_cfg.scan.scan_2mbit_phy);
    ASSERT_EQ(true, gw_cfg.ruuvi_cfg.scan.scan_channel_37);
    ASSERT_EQ(true, gw_cfg.ruuvi_cfg.scan.scan_channel_38);
    ASSERT_EQ(true, gw_cfg.ruuvi_cfg.scan.scan_channel_39);

    ASSERT_EQ(string(""), gw_cfg.ruuvi_cfg.coordinates.buf);
}
