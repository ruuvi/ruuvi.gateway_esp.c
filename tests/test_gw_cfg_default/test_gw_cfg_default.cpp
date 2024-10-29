/**
 * @file test_gw_cfg_default.cpp
 * @author TheSomeMan
 * @date 2021-12-10
 * @copyright Ruuvi Innovations Ltd, license BSD-3-Clause.
 */

#include "gw_cfg_default.h"
#include "gtest/gtest.h"
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

class TestGwCfgDefault;
static TestGwCfgDefault* g_pTestClass;

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

class TestGwCfgDefault : public ::testing::Test
{
private:
protected:
    void
    SetUp() override
    {
        esp_log_wrapper_init();
        g_pTestClass                                  = this;
        const gw_cfg_default_init_param_t init_params = {
            .wifi_ap_ssid        = { "my_ssid1" },
            .hostname            = { "RuuviGatewayAABB" },
            .device_id           = { 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88 },
            .esp32_fw_ver        = { "v1.10.0" },
            .nrf52_fw_ver        = { "v0.7.2" },
            .nrf52_mac_addr      = { 0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF },
            .esp32_mac_addr_wifi = { 0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0x11 },
            .esp32_mac_addr_eth  = { 0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0x22 },
        };

        gw_cfg_default_init(&init_params, nullptr);
        gw_cfg_default_log();
    }

    void
    TearDown() override
    {
        gw_cfg_default_deinit();
        g_pTestClass = nullptr;
        esp_log_wrapper_deinit();
    }

public:
    TestGwCfgDefault();
    ~TestGwCfgDefault() override;

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

TestGwCfgDefault::TestGwCfgDefault()
    : Test()
{
}

TestGwCfgDefault::~TestGwCfgDefault() = default;

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

TEST_F(TestGwCfgDefault, test_1) // NOLINT
{
    gw_cfg_t gw_cfg {};
    gw_cfg_default_get(&gw_cfg);

    ASSERT_TRUE(gw_cfg.eth_cfg.use_eth);
    ASSERT_TRUE(gw_cfg.eth_cfg.eth_dhcp);
    ASSERT_EQ(string(""), string(gw_cfg.eth_cfg.eth_static_ip.buf));
    ASSERT_EQ(string(""), string(gw_cfg.eth_cfg.eth_netmask.buf));
    ASSERT_EQ(string(""), string(gw_cfg.eth_cfg.eth_gw.buf));
    ASSERT_EQ(string(""), string(gw_cfg.eth_cfg.eth_dns1.buf));
    ASSERT_EQ(string(""), string(gw_cfg.eth_cfg.eth_dns2.buf));

    ASSERT_FALSE(gw_cfg.ruuvi_cfg.remote.use_remote_cfg);
    ASSERT_EQ(string(""), string(gw_cfg.ruuvi_cfg.remote.url.buf));
    ASSERT_EQ(GW_CFG_HTTP_AUTH_TYPE_NONE, gw_cfg.ruuvi_cfg.remote.auth_type);
    ASSERT_EQ(0, gw_cfg.ruuvi_cfg.remote.refresh_interval_minutes);

    ASSERT_TRUE(gw_cfg.ruuvi_cfg.http.use_http_ruuvi);
    ASSERT_FALSE(gw_cfg.ruuvi_cfg.http.use_http);
    ASSERT_EQ(GW_CFG_HTTP_DATA_FORMAT_RUUVI, gw_cfg.ruuvi_cfg.http.data_format);
    ASSERT_EQ(GW_CFG_HTTP_AUTH_TYPE_NONE, gw_cfg.ruuvi_cfg.http.auth_type);
    ASSERT_EQ(string(RUUVI_GATEWAY_HTTP_DEFAULT_URL), string(gw_cfg.ruuvi_cfg.http.http_url.buf));

    ASSERT_TRUE(gw_cfg.ruuvi_cfg.http_stat.use_http_stat);
    ASSERT_EQ(string(RUUVI_GATEWAY_HTTP_STATUS_URL), string(gw_cfg.ruuvi_cfg.http_stat.http_stat_url.buf));
    ASSERT_EQ(string(""), string(gw_cfg.ruuvi_cfg.http_stat.http_stat_user.buf));
    ASSERT_EQ(string(""), string(gw_cfg.ruuvi_cfg.http_stat.http_stat_pass.buf));

    ASSERT_FALSE(gw_cfg.ruuvi_cfg.mqtt.use_mqtt);
    ASSERT_EQ(string(MQTT_TRANSPORT_TCP), string(gw_cfg.ruuvi_cfg.mqtt.mqtt_transport.buf));
    ASSERT_EQ(string("test.mosquitto.org"), string(gw_cfg.ruuvi_cfg.mqtt.mqtt_server.buf));
    ASSERT_EQ(1883, gw_cfg.ruuvi_cfg.mqtt.mqtt_port);
    ASSERT_EQ(string("ruuvi/AA:BB:CC:DD:EE:FF/"), string(gw_cfg.ruuvi_cfg.mqtt.mqtt_prefix.buf));
    ASSERT_EQ(string("AA:BB:CC:DD:EE:FF"), string(gw_cfg.ruuvi_cfg.mqtt.mqtt_client_id.buf));
    ASSERT_EQ(string(""), string(gw_cfg.ruuvi_cfg.mqtt.mqtt_user.buf));
    ASSERT_EQ(string(""), string(gw_cfg.ruuvi_cfg.mqtt.mqtt_pass.buf));

    ASSERT_EQ(HTTP_SERVER_AUTH_TYPE_DEFAULT, gw_cfg.ruuvi_cfg.lan_auth.lan_auth_type);
    ASSERT_EQ(string(RUUVI_GATEWAY_AUTH_DEFAULT_USER), string(gw_cfg.ruuvi_cfg.lan_auth.lan_auth_user.buf));
    ASSERT_EQ(string("35305258d71bec64f796d2f36a55ffb1"), string(gw_cfg.ruuvi_cfg.lan_auth.lan_auth_pass.buf));
    ASSERT_EQ(string(""), string(gw_cfg.ruuvi_cfg.lan_auth.lan_auth_api_key.buf));

    ASSERT_EQ(AUTO_UPDATE_CYCLE_TYPE_REGULAR, gw_cfg.ruuvi_cfg.auto_update.auto_update_cycle);
    ASSERT_EQ(0x7F, gw_cfg.ruuvi_cfg.auto_update.auto_update_weekdays_bitmask);
    ASSERT_EQ(0, gw_cfg.ruuvi_cfg.auto_update.auto_update_interval_from);
    ASSERT_EQ(24, gw_cfg.ruuvi_cfg.auto_update.auto_update_interval_to);
    ASSERT_EQ(3, gw_cfg.ruuvi_cfg.auto_update.auto_update_tz_offset_hours);

    ASSERT_TRUE(gw_cfg.ruuvi_cfg.ntp.ntp_use);
    ASSERT_FALSE(gw_cfg.ruuvi_cfg.ntp.ntp_use_dhcp);
    ASSERT_EQ(string("time.google.com"), string(gw_cfg.ruuvi_cfg.ntp.ntp_server1.buf));
    ASSERT_EQ(string("time.cloudflare.com"), string(gw_cfg.ruuvi_cfg.ntp.ntp_server2.buf));
    ASSERT_EQ(string("pool.ntp.org"), string(gw_cfg.ruuvi_cfg.ntp.ntp_server3.buf));
    ASSERT_EQ(string("time.ruuvi.com"), string(gw_cfg.ruuvi_cfg.ntp.ntp_server4.buf));

    ASSERT_EQ(RUUVI_COMPANY_ID, gw_cfg.ruuvi_cfg.filter.company_id);
    ASSERT_TRUE(gw_cfg.ruuvi_cfg.filter.company_use_filtering);

    ASSERT_FALSE(gw_cfg.ruuvi_cfg.scan.scan_coded_phy);
    ASSERT_TRUE(gw_cfg.ruuvi_cfg.scan.scan_1mbit_phy);
    ASSERT_TRUE(gw_cfg.ruuvi_cfg.scan.scan_2mbit_phy);
    ASSERT_TRUE(gw_cfg.ruuvi_cfg.scan.scan_channel_37);
    ASSERT_TRUE(gw_cfg.ruuvi_cfg.scan.scan_channel_38);
    ASSERT_TRUE(gw_cfg.ruuvi_cfg.scan.scan_channel_39);

    ASSERT_EQ(string(""), string(gw_cfg.ruuvi_cfg.coordinates.buf));

    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, string("Gateway SETTINGS (default):"));
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, string("config: device_info: WiFi AP SSID: my_ssid1"));
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, string("config: device_info: Hostname: RuuviGatewayAABB"));
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, string("config: device_info: ESP32 fw ver: v1.10.0"));
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, string("config: device_info: ESP32 WiFi MAC ADDR: AA:BB:CC:DD:EE:11"));
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, string("config: device_info: ESP32 Eth MAC ADDR: AA:BB:CC:DD:EE:22"));
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, string("config: device_info: NRF52 fw ver: v0.7.2"));
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, string("config: device_info: NRF52 MAC ADDR: AA:BB:CC:DD:EE:FF"));
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, string("config: device_info: NRF52 DEVICE ID: 11:22:33:44:55:66:77:88"));
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, string("config: device_info: storage_ready: 0"));
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, string("config: Use eth: yes"));
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, string("config: eth: use DHCP: yes"));
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, string("config: eth: static IP: "));
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, string("config: eth: netmask: "));
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, string("config: eth: GW: "));
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, string("config: eth: DNS1: "));
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, string("config: eth: DNS2: "));
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, string("config: use remote cfg: 0"));
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, string("config: remote cfg: URL: "));
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, string("config: remote cfg: auth_type: none"));
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, string("config: remote cfg: use SSL client cert: 0"));
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, string("config: remote cfg: use SSL server cert: 0"));
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, string("config: remote cfg: refresh_interval_minutes: 0"));
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, string("config: use http ruuvi: 1"));
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, string("config: use http: 0"));
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, string("config: use http_stat: 1"));
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, string("config: http_stat url: " RUUVI_GATEWAY_HTTP_STATUS_URL));
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, string("config: http_stat user: "));
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, string("config: http_stat pass: ********"));
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, string("config: http_stat: use SSL client cert: 0"));
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, string("config: http_stat: use SSL server cert: 0"));
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, string("config: use mqtt: 0"));
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, string("config: mqtt disable retained messages: 0"));
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, string("config: mqtt transport: TCP"));
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, string("config: mqtt data format: ruuvi_raw"));
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, string("config: mqtt server: test.mosquitto.org"));
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, string("config: mqtt port: 1883"));
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, string("config: mqtt sending interval: 0"));
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, string("config: mqtt prefix: ruuvi/AA:BB:CC:DD:EE:FF/"));
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, string("config: mqtt client id: AA:BB:CC:DD:EE:FF"));
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, string("config: mqtt user: "));
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, string("config: mqtt password: ********"));
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, string("config: mqtt: use SSL client cert: 0"));
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, string("config: mqtt: use SSL server cert: 0"));
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, string("config: LAN auth type: lan_auth_default"));
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, string("config: LAN auth user: Admin"));
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, string("config: LAN auth pass: ********"));
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, string("config: LAN auth API key: ********"));
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, string("config: LAN auth API key (RW): ********"));
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, string("config: Auto update cycle: regular"));
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, string("config: Auto update weekdays_bitmask: 0x7f"));
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, string("config: Auto update interval: 00:00..24:00"));
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, string("config: Auto update TZ: UTC+3"));
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, string("config: NTP: Use: yes"));
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, string("config: NTP: Use DHCP: no"));
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, string("config: NTP: Server1: time.google.com"));
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, string("config: NTP: Server2: time.cloudflare.com"));
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, string("config: NTP: Server3: pool.ntp.org"));
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, string("config: NTP: Server4: time.ruuvi.com"));
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, string("config: use company id filter: 1"));
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, string("config: company id: 0x0499"));
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, string("config: use scan coded phy: 0"));
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, string("config: use scan 1mbit/phy: 1"));
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, string("config: use scan 2mbit/phy: 1"));
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, string("config: use scan channel 37: 1"));
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, string("config: use scan channel 38: 1"));
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, string("config: use scan channel 39: 1"));
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, string("config: scan default       : 1"));
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, string("config: use scan filter: no"));
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, string("config: coordinates: "));
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, string("config: fw_update: url: https://network.ruuvi.com/firmwareupdate"));
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, string("config: wifi_ap_config: SSID: my_ssid1"));
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, string("config: wifi_ap_config: password: "));
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, string("config: wifi_ap_config: ssid_len: 0"));
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, string("config: wifi_ap_config: channel: 1"));
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, string("config: wifi_ap_config: auth_mode: OPEN"));
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, string("config: wifi_ap_config: ssid_hidden: 0"));
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, string("config: wifi_ap_config: max_connections: 4"));
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, string("config: wifi_ap_config: beacon_interval: 100"));
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, string("config: wifi_ap_settings: bandwidth: 20MHz"));
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, string("config: wifi_ap_settings: IP: 10.10.0.1"));
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, string("config: wifi_ap_settings: GW: 10.10.0.1"));
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, string("config: wifi_ap_settings: Netmask: 255.255.255.0"));
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, string("config: wifi_sta_config: SSID:'', password:'********'"));
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, string("config: wifi_sta_config: scan_method: Fast"));
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, string("config: wifi_sta_config: Use BSSID: false"));
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, string("config: wifi_sta_config: Channel: 0"));
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, string("config: wifi_sta_config: Listen interval: 0"));
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, string("config: wifi_sta_config: Sort method: by RSSI"));
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, string("config: wifi_sta_config: Fast scan: min RSSI: 0"));
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, string("config: wifi_sta_config: Fast scan: weakest auth mode: 0"));
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, string("config: wifi_sta_config: Protected Management Frame: Capable: false"));
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, string("config: wifi_sta_config: Protected Management Frame: Required: false"));
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, string("config: wifi_sta_settings: Power save: NONE"));
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, string("config: wifi_sta_settings: Use Static IP: no"));
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, string("config: Host info: Hostname: RuuviGatewayAABB"));
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, string("config: Host info: fw_ver: v1.10.0"));
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, string("config: Host info: nrf52_fw_ver: v0.7.2"));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
}

TEST_F(TestGwCfgDefault, test_gw_cfg_default_get_lan_auth) // NOLINT
{
    const ruuvi_gw_cfg_lan_auth_t* const p_lan_auth = gw_cfg_default_get_lan_auth();
    ASSERT_EQ(HTTP_SERVER_AUTH_TYPE_DEFAULT, p_lan_auth->lan_auth_type);
    ASSERT_EQ(string(RUUVI_GATEWAY_AUTH_DEFAULT_USER), string(p_lan_auth->lan_auth_user.buf));
    ASSERT_EQ(string("35305258d71bec64f796d2f36a55ffb1"), string(p_lan_auth->lan_auth_pass.buf));
}

TEST_F(TestGwCfgDefault, test_gw_cfg_default_get_eth) // NOLINT
{
    const gw_cfg_eth_t gw_cfg_eth = *gw_cfg_default_get_eth();

    ASSERT_TRUE(gw_cfg_eth.use_eth);
    ASSERT_TRUE(gw_cfg_eth.eth_dhcp);
    ASSERT_EQ(string(""), string(gw_cfg_eth.eth_static_ip.buf));
    ASSERT_EQ(string(""), string(gw_cfg_eth.eth_netmask.buf));
    ASSERT_EQ(string(""), string(gw_cfg_eth.eth_gw.buf));
    ASSERT_EQ(string(""), string(gw_cfg_eth.eth_dns1.buf));
    ASSERT_EQ(string(""), string(gw_cfg_eth.eth_dns2.buf));
}
