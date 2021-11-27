/**
 * @file test_gw_cfg_blob.cpp
 * @author TheSomeMan
 * @date 2021-10-05
 * @copyright Ruuvi Innovations Ltd, license BSD-3-Clause.
 */

#include <cstring>
#include "gtest/gtest.h"
#include "gw_cfg.h"
#include "gw_cfg_blob.h"
#include "gw_cfg_default.h"

using namespace std;

/*** Google-test class implementation
 * *********************************************************************************/

class TestGwCfgBlob;
static TestGwCfgBlob *g_pTestClass;

extern "C" {

const char *
os_task_get_name(void)
{
    static const char g_task_name[] = "main";
    return const_cast<char *>(g_task_name);
}

} // extern "C"

class TestGwCfgBlob : public ::testing::Test
{
private:
protected:
    void
    SetUp() override
    {
        g_pTestClass = this;
    }

    void
    TearDown() override
    {
        g_pTestClass = nullptr;
    }

public:
    TestGwCfgBlob();

    ~TestGwCfgBlob() override;
};

TestGwCfgBlob::TestGwCfgBlob()
    : Test()
{
}

extern "C" {

} // extern "C"

TestGwCfgBlob::~TestGwCfgBlob() = default;

#define TEST_CHECK_LOG_RECORD(level_, msg_) ESP_LOG_WRAPPER_TEST_CHECK_LOG_RECORD("gw_cfg", level_, msg_)

/*** Unit-Tests
 * *******************************************************************************************************/

TEST_F(TestGwCfgBlob, test_gw_cfg_blob_convert_1) // NOLINT
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
            HTTP_SERVER_AUTH_TYPE_STR_RUUVI,
            RUUVI_GATEWAY_AUTH_DEFAULT_USER,
            RUUVI_GATEWAY_AUTH_DEFAULT_PASS_USE_DEVICE_ID,
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
            .scan_extended_payload = true,
            .scan_channel_37 = true,
            .scan_channel_38 = true,
            .scan_channel_39 = true,
        },
        .coordinates = { 0 },
    };

    ruuvi_gateway_config_t gw_cfg {};
    gw_cfg_blob_convert(&gw_cfg, &gw_cfg_blob);

    ASSERT_EQ(false, gw_cfg.eth.use_eth);
    ASSERT_EQ(true, gw_cfg.eth.eth_dhcp);
    ASSERT_EQ(string(""), gw_cfg.eth.eth_static_ip.buf);
    ASSERT_EQ(string(""), gw_cfg.eth.eth_netmask.buf);
    ASSERT_EQ(string(""), gw_cfg.eth.eth_gw.buf);
    ASSERT_EQ(string(""), gw_cfg.eth.eth_dns1.buf);
    ASSERT_EQ(string(""), gw_cfg.eth.eth_dns2.buf);

    ASSERT_EQ(false, gw_cfg.mqtt.use_mqtt);
    ASSERT_EQ(true, gw_cfg.mqtt.mqtt_use_default_prefix);
    ASSERT_EQ(string("TCP"), gw_cfg.mqtt.mqtt_transport.buf);
    ASSERT_EQ(string(""), gw_cfg.mqtt.mqtt_server.buf);
    ASSERT_EQ(0, gw_cfg.mqtt.mqtt_port);
    ASSERT_EQ(string(""), gw_cfg.mqtt.mqtt_prefix.buf);
    ASSERT_EQ(string(""), gw_cfg.mqtt.mqtt_client_id.buf);
    ASSERT_EQ(string(""), gw_cfg.mqtt.mqtt_user.buf);
    ASSERT_EQ(string(""), gw_cfg.mqtt.mqtt_pass.buf);

    ASSERT_EQ(true, gw_cfg.http.use_http);
    ASSERT_EQ(string(RUUVI_GATEWAY_HTTP_DEFAULT_URL), gw_cfg.http.http_url.buf);
    ASSERT_EQ(string(""), gw_cfg.http.http_user.buf);
    ASSERT_EQ(string(""), gw_cfg.http.http_pass.buf);

    ASSERT_EQ(true, gw_cfg.http_stat.use_http_stat);
    ASSERT_EQ(string(RUUVI_GATEWAY_HTTP_STATUS_URL), gw_cfg.http_stat.http_stat_url.buf);
    ASSERT_EQ(string(""), gw_cfg.http_stat.http_stat_user.buf);
    ASSERT_EQ(string(""), gw_cfg.http_stat.http_stat_pass.buf);

    ASSERT_EQ(string(HTTP_SERVER_AUTH_TYPE_STR_RUUVI), gw_cfg.lan_auth.lan_auth_type);
    ASSERT_EQ(string(RUUVI_GATEWAY_AUTH_DEFAULT_USER), gw_cfg.lan_auth.lan_auth_user);
    ASSERT_EQ(string(RUUVI_GATEWAY_AUTH_DEFAULT_PASS_USE_DEVICE_ID), gw_cfg.lan_auth.lan_auth_pass);

    ASSERT_EQ(AUTO_UPDATE_CYCLE_TYPE_REGULAR, gw_cfg.auto_update.auto_update_cycle);
    ASSERT_EQ(0x7F, gw_cfg.auto_update.auto_update_weekdays_bitmask);
    ASSERT_EQ(0, gw_cfg.auto_update.auto_update_interval_from);
    ASSERT_EQ(24, gw_cfg.auto_update.auto_update_interval_to);
    ASSERT_EQ(3, gw_cfg.auto_update.auto_update_tz_offset_hours);

    ASSERT_EQ(RUUVI_COMPANY_ID, gw_cfg.filter.company_id);
    ASSERT_EQ(true, gw_cfg.filter.company_use_filtering);

    ASSERT_EQ(false, gw_cfg.scan.scan_coded_phy);
    ASSERT_EQ(true, gw_cfg.scan.scan_1mbit_phy);
    ASSERT_EQ(true, gw_cfg.scan.scan_extended_payload);
    ASSERT_EQ(true, gw_cfg.scan.scan_channel_37);
    ASSERT_EQ(true, gw_cfg.scan.scan_channel_38);
    ASSERT_EQ(true, gw_cfg.scan.scan_channel_39);

    ASSERT_EQ(string(""), gw_cfg.coordinates.buf);
}

TEST_F(TestGwCfgBlob, test_gw_cfg_blob_convert_2) // NOLINT
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
            HTTP_SERVER_AUTH_TYPE_STR_DIGEST,
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
            .scan_extended_payload = false,
            .scan_channel_37 = false,
            .scan_channel_38 = false,
            .scan_channel_39 = false,
        },
        .coordinates = { 'q', 'w', 'e', '\0', },
    };

    ruuvi_gateway_config_t gw_cfg {};
    gw_cfg_blob_convert(&gw_cfg, &gw_cfg_blob);

    ASSERT_EQ(true, gw_cfg.eth.use_eth);
    ASSERT_EQ(false, gw_cfg.eth.eth_dhcp);
    ASSERT_EQ(string("192.168.1.10"), gw_cfg.eth.eth_static_ip.buf);
    ASSERT_EQ(string("255.255.255.0"), gw_cfg.eth.eth_netmask.buf);
    ASSERT_EQ(string("192.168.1.1"), gw_cfg.eth.eth_gw.buf);
    ASSERT_EQ(string("8.8.8.8"), gw_cfg.eth.eth_dns1.buf);
    ASSERT_EQ(string("4.4.4.4"), gw_cfg.eth.eth_dns2.buf);

    ASSERT_EQ(true, gw_cfg.mqtt.use_mqtt);
    ASSERT_EQ(false, gw_cfg.mqtt.mqtt_use_default_prefix);
    ASSERT_EQ(string("TCP"), gw_cfg.mqtt.mqtt_transport.buf);
    ASSERT_EQ(string("test.mosquitto.org"), gw_cfg.mqtt.mqtt_server.buf);
    ASSERT_EQ(1883, gw_cfg.mqtt.mqtt_port);
    ASSERT_EQ(string("prefix1"), gw_cfg.mqtt.mqtt_prefix.buf);
    ASSERT_EQ(string("my_client"), gw_cfg.mqtt.mqtt_client_id.buf);
    ASSERT_EQ(string("m_user1"), gw_cfg.mqtt.mqtt_user.buf);
    ASSERT_EQ(string("m_pass1"), gw_cfg.mqtt.mqtt_pass.buf);

    ASSERT_EQ(false, gw_cfg.http.use_http);
    ASSERT_EQ(string("https://myserver.com/record"), gw_cfg.http.http_url.buf);
    ASSERT_EQ(string("h_user1"), gw_cfg.http.http_user.buf);
    ASSERT_EQ(string("h_pass1"), gw_cfg.http.http_pass.buf);

    ASSERT_EQ(true, gw_cfg.http_stat.use_http_stat);
    ASSERT_EQ(string(RUUVI_GATEWAY_HTTP_STATUS_URL), gw_cfg.http_stat.http_stat_url.buf);
    ASSERT_EQ(string(""), gw_cfg.http_stat.http_stat_user.buf);
    ASSERT_EQ(string(""), gw_cfg.http_stat.http_stat_pass.buf);

    ASSERT_EQ(string(HTTP_SERVER_AUTH_TYPE_STR_DIGEST), gw_cfg.lan_auth.lan_auth_type);
    ASSERT_EQ(string("l_user1"), gw_cfg.lan_auth.lan_auth_user);
    ASSERT_EQ(string("l_pass1"), gw_cfg.lan_auth.lan_auth_pass);

    ASSERT_EQ(AUTO_UPDATE_CYCLE_TYPE_BETA_TESTER, gw_cfg.auto_update.auto_update_cycle);
    ASSERT_EQ(0x0F, gw_cfg.auto_update.auto_update_weekdays_bitmask);
    ASSERT_EQ(1, gw_cfg.auto_update.auto_update_interval_from);
    ASSERT_EQ(23, gw_cfg.auto_update.auto_update_interval_to);
    ASSERT_EQ(2, gw_cfg.auto_update.auto_update_tz_offset_hours);

    ASSERT_EQ(RUUVI_COMPANY_ID - 1, gw_cfg.filter.company_id);
    ASSERT_EQ(false, gw_cfg.filter.company_use_filtering);

    ASSERT_EQ(true, gw_cfg.scan.scan_coded_phy);
    ASSERT_EQ(false, gw_cfg.scan.scan_1mbit_phy);
    ASSERT_EQ(false, gw_cfg.scan.scan_extended_payload);
    ASSERT_EQ(false, gw_cfg.scan.scan_channel_37);
    ASSERT_EQ(false, gw_cfg.scan.scan_channel_38);
    ASSERT_EQ(false, gw_cfg.scan.scan_channel_39);

    ASSERT_EQ(string("qwe"), gw_cfg.coordinates.buf);
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
            HTTP_SERVER_AUTH_TYPE_STR_DIGEST,
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
            .scan_extended_payload = false,
            .scan_channel_37 = false,
            .scan_channel_38 = false,
            .scan_channel_39 = false,
        },
        .coordinates = { 'q', 'w', 'e', '\0', },
    };

    ruuvi_gateway_config_t gw_cfg {};
    gw_cfg_blob_convert(&gw_cfg, &gw_cfg_blob);

    ASSERT_EQ(false, gw_cfg.eth.use_eth);
    ASSERT_EQ(true, gw_cfg.eth.eth_dhcp);
    ASSERT_EQ(string(""), gw_cfg.eth.eth_static_ip.buf);
    ASSERT_EQ(string(""), gw_cfg.eth.eth_netmask.buf);
    ASSERT_EQ(string(""), gw_cfg.eth.eth_gw.buf);
    ASSERT_EQ(string(""), gw_cfg.eth.eth_dns1.buf);
    ASSERT_EQ(string(""), gw_cfg.eth.eth_dns2.buf);

    ASSERT_EQ(false, gw_cfg.mqtt.use_mqtt);
    ASSERT_EQ(true, gw_cfg.mqtt.mqtt_use_default_prefix);
    ASSERT_EQ(string("TCP"), gw_cfg.mqtt.mqtt_transport.buf);
    ASSERT_EQ(string(""), gw_cfg.mqtt.mqtt_server.buf);
    ASSERT_EQ(0, gw_cfg.mqtt.mqtt_port);
    ASSERT_EQ(string(""), gw_cfg.mqtt.mqtt_prefix.buf);
    ASSERT_EQ(string(""), gw_cfg.mqtt.mqtt_client_id.buf);
    ASSERT_EQ(string(""), gw_cfg.mqtt.mqtt_user.buf);
    ASSERT_EQ(string(""), gw_cfg.mqtt.mqtt_pass.buf);

    ASSERT_EQ(true, gw_cfg.http.use_http);
    ASSERT_EQ(string(RUUVI_GATEWAY_HTTP_DEFAULT_URL), gw_cfg.http.http_url.buf);
    ASSERT_EQ(string(""), gw_cfg.http.http_user.buf);
    ASSERT_EQ(string(""), gw_cfg.http.http_pass.buf);

    ASSERT_EQ(true, gw_cfg.http_stat.use_http_stat);
    ASSERT_EQ(string(RUUVI_GATEWAY_HTTP_STATUS_URL), gw_cfg.http_stat.http_stat_url.buf);
    ASSERT_EQ(string(""), gw_cfg.http_stat.http_stat_user.buf);
    ASSERT_EQ(string(""), gw_cfg.http_stat.http_stat_pass.buf);

    ASSERT_EQ(string(HTTP_SERVER_AUTH_TYPE_STR_RUUVI), gw_cfg.lan_auth.lan_auth_type);
    ASSERT_EQ(string(RUUVI_GATEWAY_AUTH_DEFAULT_USER), gw_cfg.lan_auth.lan_auth_user);
    ASSERT_EQ(string(RUUVI_GATEWAY_AUTH_DEFAULT_PASS_USE_DEVICE_ID), gw_cfg.lan_auth.lan_auth_pass);

    ASSERT_EQ(AUTO_UPDATE_CYCLE_TYPE_REGULAR, gw_cfg.auto_update.auto_update_cycle);
    ASSERT_EQ(0x7F, gw_cfg.auto_update.auto_update_weekdays_bitmask);
    ASSERT_EQ(0, gw_cfg.auto_update.auto_update_interval_from);
    ASSERT_EQ(24, gw_cfg.auto_update.auto_update_interval_to);
    ASSERT_EQ(3, gw_cfg.auto_update.auto_update_tz_offset_hours);

    ASSERT_EQ(RUUVI_COMPANY_ID, gw_cfg.filter.company_id);
    ASSERT_EQ(true, gw_cfg.filter.company_use_filtering);

    ASSERT_EQ(false, gw_cfg.scan.scan_coded_phy);
    ASSERT_EQ(true, gw_cfg.scan.scan_1mbit_phy);
    ASSERT_EQ(true, gw_cfg.scan.scan_extended_payload);
    ASSERT_EQ(true, gw_cfg.scan.scan_channel_37);
    ASSERT_EQ(true, gw_cfg.scan.scan_channel_38);
    ASSERT_EQ(true, gw_cfg.scan.scan_channel_39);

    ASSERT_EQ(string(""), gw_cfg.coordinates.buf);
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
            HTTP_SERVER_AUTH_TYPE_STR_DIGEST,
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
            .scan_extended_payload = false,
            .scan_channel_37 = false,
            .scan_channel_38 = false,
            .scan_channel_39 = false,
        },
        .coordinates = { 'q', 'w', 'e', '\0', },
    };

    ruuvi_gateway_config_t gw_cfg {};
    gw_cfg_blob_convert(&gw_cfg, &gw_cfg_blob);

    ASSERT_EQ(false, gw_cfg.eth.use_eth);
    ASSERT_EQ(true, gw_cfg.eth.eth_dhcp);
    ASSERT_EQ(string(""), gw_cfg.eth.eth_static_ip.buf);
    ASSERT_EQ(string(""), gw_cfg.eth.eth_netmask.buf);
    ASSERT_EQ(string(""), gw_cfg.eth.eth_gw.buf);
    ASSERT_EQ(string(""), gw_cfg.eth.eth_dns1.buf);
    ASSERT_EQ(string(""), gw_cfg.eth.eth_dns2.buf);

    ASSERT_EQ(false, gw_cfg.mqtt.use_mqtt);
    ASSERT_EQ(true, gw_cfg.mqtt.mqtt_use_default_prefix);
    ASSERT_EQ(string("TCP"), gw_cfg.mqtt.mqtt_transport.buf);
    ASSERT_EQ(string(""), gw_cfg.mqtt.mqtt_server.buf);
    ASSERT_EQ(0, gw_cfg.mqtt.mqtt_port);
    ASSERT_EQ(string(""), gw_cfg.mqtt.mqtt_prefix.buf);
    ASSERT_EQ(string(""), gw_cfg.mqtt.mqtt_client_id.buf);
    ASSERT_EQ(string(""), gw_cfg.mqtt.mqtt_user.buf);
    ASSERT_EQ(string(""), gw_cfg.mqtt.mqtt_pass.buf);

    ASSERT_EQ(true, gw_cfg.http.use_http);
    ASSERT_EQ(string(RUUVI_GATEWAY_HTTP_DEFAULT_URL), gw_cfg.http.http_url.buf);
    ASSERT_EQ(string(""), gw_cfg.http.http_user.buf);
    ASSERT_EQ(string(""), gw_cfg.http.http_pass.buf);

    ASSERT_EQ(true, gw_cfg.http_stat.use_http_stat);
    ASSERT_EQ(string(RUUVI_GATEWAY_HTTP_STATUS_URL), gw_cfg.http_stat.http_stat_url.buf);
    ASSERT_EQ(string(""), gw_cfg.http_stat.http_stat_user.buf);
    ASSERT_EQ(string(""), gw_cfg.http_stat.http_stat_pass.buf);

    ASSERT_EQ(string(HTTP_SERVER_AUTH_TYPE_STR_RUUVI), gw_cfg.lan_auth.lan_auth_type);
    ASSERT_EQ(string(RUUVI_GATEWAY_AUTH_DEFAULT_USER), gw_cfg.lan_auth.lan_auth_user);
    ASSERT_EQ(string(RUUVI_GATEWAY_AUTH_DEFAULT_PASS_USE_DEVICE_ID), gw_cfg.lan_auth.lan_auth_pass);

    ASSERT_EQ(AUTO_UPDATE_CYCLE_TYPE_REGULAR, gw_cfg.auto_update.auto_update_cycle);
    ASSERT_EQ(0x7F, gw_cfg.auto_update.auto_update_weekdays_bitmask);
    ASSERT_EQ(0, gw_cfg.auto_update.auto_update_interval_from);
    ASSERT_EQ(24, gw_cfg.auto_update.auto_update_interval_to);
    ASSERT_EQ(3, gw_cfg.auto_update.auto_update_tz_offset_hours);

    ASSERT_EQ(RUUVI_COMPANY_ID, gw_cfg.filter.company_id);
    ASSERT_EQ(true, gw_cfg.filter.company_use_filtering);

    ASSERT_EQ(false, gw_cfg.scan.scan_coded_phy);
    ASSERT_EQ(true, gw_cfg.scan.scan_1mbit_phy);
    ASSERT_EQ(true, gw_cfg.scan.scan_extended_payload);
    ASSERT_EQ(true, gw_cfg.scan.scan_channel_37);
    ASSERT_EQ(true, gw_cfg.scan.scan_channel_38);
    ASSERT_EQ(true, gw_cfg.scan.scan_channel_39);

    ASSERT_EQ(string(""), gw_cfg.coordinates.buf);
}
