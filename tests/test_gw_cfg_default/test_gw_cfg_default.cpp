/**
 * @file test_gw_cfg_default.cpp
 * @author TheSomeMan
 * @date 2021-12-10
 * @copyright Ruuvi Innovations Ltd, license BSD-3-Clause.
 */

#include "gw_cfg_default.h"
#include "gtest/gtest.h"

using namespace std;

/*** Google-test class implementation
 * *********************************************************************************/

class TestGwCfgDefault;
static TestGwCfgDefault *g_pTestClass;

class TestGwCfgDefault : public ::testing::Test
{
private:
protected:
    void
    SetUp() override
    {
        g_pTestClass                              = this;
        const wifi_ssid_t           wifi_ssid     = { "my_ssid1" };
        const nrf52_device_id_str_t device_id_str = { "11:22:33:44:55:66:77:88" };
        gw_cfg_default_init(&wifi_ssid, device_id_str);
    }

    void
    TearDown() override
    {
        g_pTestClass = nullptr;
    }

public:
    TestGwCfgDefault();
    ~TestGwCfgDefault() override;
};

TestGwCfgDefault::TestGwCfgDefault()
    : Test()
{
}

TestGwCfgDefault::~TestGwCfgDefault() = default;

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

TEST_F(TestGwCfgDefault, test_1) // NOLINT
{
    ruuvi_gateway_config_t gw_cfg {};
    gw_cfg_default_get(&gw_cfg);

    ASSERT_FALSE(gw_cfg.eth.use_eth);
    ASSERT_TRUE(gw_cfg.eth.eth_dhcp);
    ASSERT_EQ(string(""), string(gw_cfg.eth.eth_static_ip.buf));
    ASSERT_EQ(string(""), string(gw_cfg.eth.eth_netmask.buf));
    ASSERT_EQ(string(""), string(gw_cfg.eth.eth_gw.buf));
    ASSERT_EQ(string(""), string(gw_cfg.eth.eth_dns1.buf));
    ASSERT_EQ(string(""), string(gw_cfg.eth.eth_dns2.buf));

    ASSERT_FALSE(gw_cfg.mqtt.use_mqtt);
    ASSERT_TRUE(gw_cfg.mqtt.mqtt_use_default_prefix);
    ASSERT_EQ(string(MQTT_TRANSPORT_TCP), string(gw_cfg.mqtt.mqtt_transport.buf));
    ASSERT_EQ(string(""), string(gw_cfg.mqtt.mqtt_server.buf));
    ASSERT_EQ(0, gw_cfg.mqtt.mqtt_port);
    ASSERT_EQ(string(""), string(gw_cfg.mqtt.mqtt_prefix.buf));
    ASSERT_EQ(string(""), string(gw_cfg.mqtt.mqtt_client_id.buf));
    ASSERT_EQ(string(""), string(gw_cfg.mqtt.mqtt_user.buf));
    ASSERT_EQ(string(""), string(gw_cfg.mqtt.mqtt_pass.buf));

    ASSERT_TRUE(gw_cfg.http.use_http);
    ASSERT_EQ(string(RUUVI_GATEWAY_HTTP_DEFAULT_URL), string(gw_cfg.http.http_url.buf));
    ASSERT_EQ(string(""), string(gw_cfg.http.http_user.buf));
    ASSERT_EQ(string(""), string(gw_cfg.http.http_pass.buf));

    ASSERT_TRUE(gw_cfg.http_stat.use_http_stat);
    ASSERT_EQ(string(RUUVI_GATEWAY_HTTP_STATUS_URL), string(gw_cfg.http_stat.http_stat_url.buf));
    ASSERT_EQ(string(""), string(gw_cfg.http_stat.http_stat_user.buf));
    ASSERT_EQ(string(""), string(gw_cfg.http_stat.http_stat_pass.buf));

    ASSERT_EQ(string(HTTP_SERVER_AUTH_TYPE_STR_RUUVI), string(gw_cfg.lan_auth.lan_auth_type));
    ASSERT_EQ(string(RUUVI_GATEWAY_AUTH_DEFAULT_USER), string(gw_cfg.lan_auth.lan_auth_user));
    ASSERT_EQ(string("0d6c6f1c27ca628806eb9247740d8ba1"), string(gw_cfg.lan_auth.lan_auth_pass));
    ASSERT_EQ(string(""), string(gw_cfg.lan_auth.lan_auth_api_key));

    ASSERT_EQ(AUTO_UPDATE_CYCLE_TYPE_REGULAR, gw_cfg.auto_update.auto_update_cycle);
    ASSERT_EQ(0x7F, gw_cfg.auto_update.auto_update_weekdays_bitmask);
    ASSERT_EQ(0, gw_cfg.auto_update.auto_update_interval_from);
    ASSERT_EQ(24, gw_cfg.auto_update.auto_update_interval_to);
    ASSERT_EQ(3, gw_cfg.auto_update.auto_update_tz_offset_hours);

    ASSERT_EQ(RUUVI_COMPANY_ID, gw_cfg.filter.company_id);
    ASSERT_TRUE(gw_cfg.filter.company_use_filtering);

    ASSERT_FALSE(gw_cfg.scan.scan_coded_phy);
    ASSERT_TRUE(gw_cfg.scan.scan_1mbit_phy);
    ASSERT_TRUE(gw_cfg.scan.scan_extended_payload);
    ASSERT_TRUE(gw_cfg.scan.scan_channel_37);
    ASSERT_TRUE(gw_cfg.scan.scan_channel_38);
    ASSERT_TRUE(gw_cfg.scan.scan_channel_39);

    ASSERT_EQ(string(""), string(gw_cfg.coordinates.buf));
}

TEST_F(TestGwCfgDefault, test_gw_cfg_default_get_lan_auth) // NOLINT
{
    {
        const ruuvi_gw_cfg_lan_auth_t *const p_lan_auth = gw_cfg_default_get_lan_auth();
        ASSERT_EQ(string(HTTP_SERVER_AUTH_TYPE_STR_RUUVI), string(p_lan_auth->lan_auth_type));
        ASSERT_EQ(string(RUUVI_GATEWAY_AUTH_DEFAULT_USER), string(p_lan_auth->lan_auth_user));
        ASSERT_EQ(string("0d6c6f1c27ca628806eb9247740d8ba1"), string(p_lan_auth->lan_auth_pass));
    }
}

TEST_F(TestGwCfgDefault, test_gw_cfg_default_get_eth) // NOLINT
{
    const ruuvi_gw_cfg_eth_t gw_cfg_eth = gw_cfg_default_get_eth();

    ASSERT_FALSE(gw_cfg_eth.use_eth);
    ASSERT_TRUE(gw_cfg_eth.eth_dhcp);
    ASSERT_EQ(string(""), string(gw_cfg_eth.eth_static_ip.buf));
    ASSERT_EQ(string(""), string(gw_cfg_eth.eth_netmask.buf));
    ASSERT_EQ(string(""), string(gw_cfg_eth.eth_gw.buf));
    ASSERT_EQ(string(""), string(gw_cfg_eth.eth_dns1.buf));
    ASSERT_EQ(string(""), string(gw_cfg_eth.eth_dns2.buf));
}
