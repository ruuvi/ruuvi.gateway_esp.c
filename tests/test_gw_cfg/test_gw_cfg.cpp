/**
 * @file test_gw_cfg.cpp
 * @author TheSomeMan
 * @date 2020-08-27
 * @copyright Ruuvi Innovations Ltd, license BSD-3-Clause.
 */

#include <cstring>
#include "gtest/gtest.h"
#include "freertos/FreeRTOS.h"
#include "gw_cfg.h"
#include "gw_cfg_log.h"
#include "gw_cfg_default.h"
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

class TestGwCfg;
static TestGwCfg* g_pTestClass;

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

class TestGwCfg : public ::testing::Test
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
        gw_cfg_init(nullptr);
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
    TestGwCfg();

    ~TestGwCfg() override;

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
};

TestGwCfg::TestGwCfg()
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

TestGwCfg::~TestGwCfg() = default;

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

TEST_F(TestGwCfg, gw_cfg_print_to_log_default) // NOLINT
{
    const gw_cfg_t gw_cfg = get_gateway_config_default();
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
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, string("config: mqtt server: test.mosquitto.org"));
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, string("config: mqtt port: 1883"));
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
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, string("config: NTP: Server3: time.nist.gov"));
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, string("config: NTP: Server4: pool.ntp.org"));
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, string("config: use company id filter: 1"));
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, string("config: company id: 0x0499"));
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, string("config: use scan coded phy: 0"));
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, string("config: use scan 1mbit/phy: 1"));
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, string("config: use scan extended payload: 1"));
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, string("config: use scan channel 37: 1"));
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, string("config: use scan channel 38: 1"));
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, string("config: use scan channel 39: 1"));
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, string("config: use scan filter: no"));
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, string("config: coordinates: "));
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

TEST_F(TestGwCfg, gw_cfg_print_to_log_default_and_gw_cfg_stroage_is_ready) // NOLINT
{
    esp_log_wrapper_clear();

    this->m_flag_storage_ready               = true;
    this->m_flag_storage_http_cli_cert       = true;
    this->m_flag_storage_http_cli_key        = true;
    this->m_flag_storage_stat_cli_cert       = false;
    this->m_flag_storage_stat_cli_key        = false;
    this->m_flag_storage_mqtt_cli_cert       = false;
    this->m_flag_storage_mqtt_cli_key        = false;
    this->m_flag_storage_remote_cfg_cli_cert = false;
    this->m_flag_storage_remote_cfg_cli_key  = false;

    this->m_flag_storage_http_srv_cert       = false;
    this->m_flag_storage_stat_srv_cert       = false;
    this->m_flag_storage_mqtt_srv_cert       = false;
    this->m_flag_storage_remote_cfg_srv_cert = false;

    const gw_cfg_t gw_cfg = get_gateway_config_default();
    gw_cfg_log(&gw_cfg, "Gateway SETTINGS", true);

    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, string("Gateway SETTINGS:"));
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, string("config: device_info: WiFi AP SSID: my_ssid1"));
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, string("config: device_info: Hostname: RuuviGatewayAABB"));
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, string("config: device_info: ESP32 fw ver: v1.10.0"));
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, string("config: device_info: ESP32 WiFi MAC ADDR: AA:BB:CC:DD:EE:11"));
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, string("config: device_info: ESP32 Eth MAC ADDR: AA:BB:CC:DD:EE:22"));
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, string("config: device_info: NRF52 fw ver: v0.7.2"));
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, string("config: device_info: NRF52 MAC ADDR: AA:BB:CC:DD:EE:FF"));
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, string("config: device_info: NRF52 DEVICE ID: 11:22:33:44:55:66:77:88"));
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, string("config: device_info: storage_ready: 1"));
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, string("config: device_info: storage: http_cli_cert: 1"));
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, string("config: device_info: storage: http_cli_key: 1"));
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, string("config: device_info: storage: http_srv_cert: 0"));
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, string("config: device_info: storage: stat_cli_cert: 0"));
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, string("config: device_info: storage: stat_cli_key: 0"));
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, string("config: device_info: storage: stat_srv_cert: 0"));
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, string("config: device_info: storage: mqtt_cli_cert: 0"));
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, string("config: device_info: storage: mqtt_cli_key: 0"));
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, string("config: device_info: storage: mqtt_srv_cert: 0"));
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, string("config: device_info: storage: rcfg_cli_cert: 0"));
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, string("config: device_info: storage: rcfg_cli_key: 0"));
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, string("config: device_info: storage: rcfg_srv_cert: 0"));
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
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, string("config: mqtt server: test.mosquitto.org"));
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, string("config: mqtt port: 1883"));
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
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, string("config: NTP: Server3: time.nist.gov"));
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, string("config: NTP: Server4: pool.ntp.org"));
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, string("config: use company id filter: 1"));
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, string("config: company id: 0x0499"));
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, string("config: use scan coded phy: 0"));
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, string("config: use scan 1mbit/phy: 1"));
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, string("config: use scan extended payload: 1"));
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, string("config: use scan channel 37: 1"));
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, string("config: use scan channel 38: 1"));
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, string("config: use scan channel 39: 1"));
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, string("config: use scan filter: no"));
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, string("config: coordinates: "));
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

TEST_F(TestGwCfg, gw_cfg_print_to_log_default_with_http_custom) // NOLINT
{
    gw_cfg_t gw_cfg = get_gateway_config_default();

    gw_cfg.ruuvi_cfg.http.use_http_ruuvi           = false;
    gw_cfg.ruuvi_cfg.http.use_http                 = true;
    gw_cfg.ruuvi_cfg.http.http_use_ssl_client_cert = true;
    gw_cfg.ruuvi_cfg.http.http_use_ssl_server_cert = false;
    snprintf(gw_cfg.ruuvi_cfg.http.http_url.buf, sizeof(gw_cfg.ruuvi_cfg.http.http_url.buf), "http://my_server1.com");
    gw_cfg.ruuvi_cfg.http.data_format = GW_CFG_HTTP_DATA_FORMAT_RUUVI;
    gw_cfg.ruuvi_cfg.http.auth_type   = GW_CFG_HTTP_AUTH_TYPE_NONE;

    esp_log_wrapper_clear();

    gw_cfg_log(&gw_cfg, "Gateway SETTINGS", true);

    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, string("Gateway SETTINGS:"));
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
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, string("config: use http ruuvi: 0"));
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, string("config: use http: 1"));
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, string("config: http url: http://my_server1.com"));
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, string("config: http data format: ruuvi"));
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, string("config: http auth_type: none"));
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, string("config: http: use SSL client cert: 1"));
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, string("config: http: use SSL server cert: 0"));
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, string("config: use http_stat: 1"));
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, string("config: http_stat url: " RUUVI_GATEWAY_HTTP_STATUS_URL));
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, string("config: http_stat user: "));
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, string("config: http_stat pass: ********"));
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, string("config: http_stat: use SSL client cert: 0"));
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, string("config: http_stat: use SSL server cert: 0"));
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, string("config: use mqtt: 0"));
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, string("config: mqtt disable retained messages: 0"));
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, string("config: mqtt transport: TCP"));
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, string("config: mqtt server: test.mosquitto.org"));
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, string("config: mqtt port: 1883"));
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
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, string("config: NTP: Server3: time.nist.gov"));
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, string("config: NTP: Server4: pool.ntp.org"));
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, string("config: use company id filter: 1"));
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, string("config: company id: 0x0499"));
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, string("config: use scan coded phy: 0"));
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, string("config: use scan 1mbit/phy: 1"));
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, string("config: use scan extended payload: 1"));
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, string("config: use scan channel 37: 1"));
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, string("config: use scan channel 38: 1"));
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, string("config: use scan channel 39: 1"));
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, string("config: use scan filter: no"));
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, string("config: coordinates: "));
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

TEST_F(TestGwCfg, gw_cfg_print_to_log_default_remote_enabled_auth_no) // NOLINT
{
    gw_cfg_t gw_cfg                        = get_gateway_config_default();
    gw_cfg.ruuvi_cfg.remote.use_remote_cfg = true;
    snprintf(gw_cfg.ruuvi_cfg.remote.url.buf, sizeof(gw_cfg.ruuvi_cfg.remote.url.buf), "http://my_server1.com");
    gw_cfg.ruuvi_cfg.remote.auth_type                = GW_CFG_HTTP_AUTH_TYPE_NONE;
    gw_cfg.ruuvi_cfg.remote.refresh_interval_minutes = 10;

    esp_log_wrapper_clear();

    gw_cfg_log(&gw_cfg, "Gateway SETTINGS", true);

    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, string("Gateway SETTINGS:"));
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
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, string("config: use remote cfg: 1"));
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, string("config: remote cfg: URL: http://my_server1.com"));
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, string("config: remote cfg: auth_type: none"));
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, string("config: remote cfg: use SSL client cert: 0"));
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, string("config: remote cfg: use SSL server cert: 0"));
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, string("config: remote cfg: refresh_interval_minutes: 10"));
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
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, string("config: mqtt server: test.mosquitto.org"));
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, string("config: mqtt port: 1883"));
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
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, string("config: NTP: Server3: time.nist.gov"));
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, string("config: NTP: Server4: pool.ntp.org"));
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, string("config: use company id filter: 1"));
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, string("config: company id: 0x0499"));
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, string("config: use scan coded phy: 0"));
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, string("config: use scan 1mbit/phy: 1"));
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, string("config: use scan extended payload: 1"));
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, string("config: use scan channel 37: 1"));
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, string("config: use scan channel 38: 1"));
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, string("config: use scan channel 39: 1"));
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, string("config: use scan filter: no"));
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, string("config: coordinates: "));
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

TEST_F(TestGwCfg, gw_cfg_print_to_log_default_remote_enabled_auth_basic) // NOLINT
{
    gw_cfg_t gw_cfg                        = get_gateway_config_default();
    gw_cfg.ruuvi_cfg.remote.use_remote_cfg = true;
    snprintf(gw_cfg.ruuvi_cfg.remote.url.buf, sizeof(gw_cfg.ruuvi_cfg.remote.url.buf), "https://my_server1.com");
    gw_cfg.ruuvi_cfg.remote.auth_type = GW_CFG_HTTP_AUTH_TYPE_BASIC;
    snprintf(
        gw_cfg.ruuvi_cfg.remote.auth.auth_basic.user.buf,
        sizeof(gw_cfg.ruuvi_cfg.remote.auth.auth_basic.user.buf),
        "user1");
    snprintf(
        gw_cfg.ruuvi_cfg.remote.auth.auth_basic.password.buf,
        sizeof(gw_cfg.ruuvi_cfg.remote.auth.auth_basic.password.buf),
        "pass1");
    gw_cfg.ruuvi_cfg.remote.refresh_interval_minutes = 20;

    esp_log_wrapper_clear();

    gw_cfg_log(&gw_cfg, "Gateway SETTINGS", true);

    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, string("Gateway SETTINGS:"));
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
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, string("config: use remote cfg: 1"));
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, string("config: remote cfg: URL: https://my_server1.com"));
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, string("config: remote cfg: auth_type: basic"));
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, string("config: remote cfg: auth user: user1"));
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, string("config: remote cfg: auth pass: ********"));
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, string("config: remote cfg: use SSL client cert: 0"));
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, string("config: remote cfg: use SSL server cert: 0"));
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, string("config: remote cfg: refresh_interval_minutes: 20"));
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
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, string("config: mqtt server: test.mosquitto.org"));
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, string("config: mqtt port: 1883"));
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
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, string("config: NTP: Server3: time.nist.gov"));
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, string("config: NTP: Server4: pool.ntp.org"));
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, string("config: use company id filter: 1"));
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, string("config: company id: 0x0499"));
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, string("config: use scan coded phy: 0"));
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, string("config: use scan 1mbit/phy: 1"));
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, string("config: use scan extended payload: 1"));
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, string("config: use scan channel 37: 1"));
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, string("config: use scan channel 38: 1"));
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, string("config: use scan channel 39: 1"));
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, string("config: use scan filter: no"));
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, string("config: coordinates: "));
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

TEST_F(TestGwCfg, gw_cfg_print_to_log_default_remote_enabled_auth_bearer) // NOLINT
{
    gw_cfg_t gw_cfg                        = get_gateway_config_default();
    gw_cfg.ruuvi_cfg.remote.use_remote_cfg = true;
    snprintf(gw_cfg.ruuvi_cfg.remote.url.buf, sizeof(gw_cfg.ruuvi_cfg.remote.url.buf), "https://my_server1.com");
    gw_cfg.ruuvi_cfg.remote.auth_type = GW_CFG_HTTP_AUTH_TYPE_BEARER;
    snprintf(
        gw_cfg.ruuvi_cfg.remote.auth.auth_bearer.token.buf,
        sizeof(gw_cfg.ruuvi_cfg.remote.auth.auth_bearer.token.buf),
        "token1");
    gw_cfg.ruuvi_cfg.remote.refresh_interval_minutes = 30;

    esp_log_wrapper_clear();

    gw_cfg_log(&gw_cfg, "Gateway SETTINGS", true);

    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, string("Gateway SETTINGS:"));
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
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, string("config: use remote cfg: 1"));
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, string("config: remote cfg: URL: https://my_server1.com"));
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, string("config: remote cfg: auth_type: bearer"));
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, string("config: remote cfg: auth bearer token: ********"));
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, string("config: remote cfg: use SSL client cert: 0"));
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, string("config: remote cfg: use SSL server cert: 0"));
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, string("config: remote cfg: refresh_interval_minutes: 30"));
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
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, string("config: mqtt server: test.mosquitto.org"));
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, string("config: mqtt port: 1883"));
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
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, string("config: NTP: Server3: time.nist.gov"));
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, string("config: NTP: Server4: pool.ntp.org"));
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, string("config: use company id filter: 1"));
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, string("config: company id: 0x0499"));
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, string("config: use scan coded phy: 0"));
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, string("config: use scan 1mbit/phy: 1"));
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, string("config: use scan extended payload: 1"));
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, string("config: use scan channel 37: 1"));
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, string("config: use scan channel 38: 1"));
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, string("config: use scan channel 39: 1"));
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, string("config: use scan filter: no"));
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, string("config: coordinates: "));
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

TEST_F(TestGwCfg, gw_cfg_print_to_log_default_auto_update_cycle_beta_tester_and_negative_tz) // NOLINT
{
    gw_cfg_t gw_cfg                                          = get_gateway_config_default();
    gw_cfg.ruuvi_cfg.auto_update.auto_update_cycle           = AUTO_UPDATE_CYCLE_TYPE_BETA_TESTER;
    gw_cfg.ruuvi_cfg.auto_update.auto_update_tz_offset_hours = -5;

    esp_log_wrapper_clear();

    gw_cfg_log(&gw_cfg, "Gateway SETTINGS", true);

    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, string("Gateway SETTINGS:"));
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
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, string("config: mqtt server: test.mosquitto.org"));
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, string("config: mqtt port: 1883"));
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
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, string("config: Auto update cycle: beta"));
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, string("config: Auto update weekdays_bitmask: 0x7f"));
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, string("config: Auto update interval: 00:00..24:00"));
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, string("config: Auto update TZ: UTC-5"));
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, string("config: NTP: Use: yes"));
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, string("config: NTP: Use DHCP: no"));
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, string("config: NTP: Server1: time.google.com"));
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, string("config: NTP: Server2: time.cloudflare.com"));
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, string("config: NTP: Server3: time.nist.gov"));
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, string("config: NTP: Server4: pool.ntp.org"));
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, string("config: use company id filter: 1"));
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, string("config: company id: 0x0499"));
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, string("config: use scan coded phy: 0"));
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, string("config: use scan 1mbit/phy: 1"));
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, string("config: use scan extended payload: 1"));
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, string("config: use scan channel 37: 1"));
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, string("config: use scan channel 38: 1"));
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, string("config: use scan channel 39: 1"));
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, string("config: use scan filter: no"));
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, string("config: coordinates: "));
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

TEST_F(TestGwCfg, gw_cfg_print_to_log_default_auto_update_cycle_manual) // NOLINT
{
    gw_cfg_t gw_cfg                                = get_gateway_config_default();
    gw_cfg.ruuvi_cfg.auto_update.auto_update_cycle = AUTO_UPDATE_CYCLE_TYPE_MANUAL;

    esp_log_wrapper_clear();

    gw_cfg_log(&gw_cfg, "Gateway SETTINGS", true);

    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, string("Gateway SETTINGS:"));
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
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, string("config: mqtt server: test.mosquitto.org"));
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, string("config: mqtt port: 1883"));
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
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, string("config: Auto update cycle: manual"));
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, string("config: Auto update weekdays_bitmask: 0x7f"));
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, string("config: Auto update interval: 00:00..24:00"));
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, string("config: Auto update TZ: UTC+3"));
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, string("config: NTP: Use: yes"));
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, string("config: NTP: Use DHCP: no"));
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, string("config: NTP: Server1: time.google.com"));
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, string("config: NTP: Server2: time.cloudflare.com"));
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, string("config: NTP: Server3: time.nist.gov"));
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, string("config: NTP: Server4: pool.ntp.org"));
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, string("config: use company id filter: 1"));
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, string("config: company id: 0x0499"));
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, string("config: use scan coded phy: 0"));
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, string("config: use scan 1mbit/phy: 1"));
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, string("config: use scan extended payload: 1"));
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, string("config: use scan channel 37: 1"));
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, string("config: use scan channel 38: 1"));
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, string("config: use scan channel 39: 1"));
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, string("config: use scan filter: no"));
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, string("config: coordinates: "));
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

TEST_F(TestGwCfg, gw_cfg_print_to_log_default_auto_update_cycle_invalid) // NOLINT
{
    gw_cfg_t gw_cfg                                = get_gateway_config_default();
    gw_cfg.ruuvi_cfg.auto_update.auto_update_cycle = (auto_update_cycle_type_e)-1;

    esp_log_wrapper_clear();

    gw_cfg_log(&gw_cfg, "Gateway SETTINGS", true);

    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, string("Gateway SETTINGS:"));
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
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, string("config: mqtt server: test.mosquitto.org"));
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, string("config: mqtt port: 1883"));
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
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, string("config: Auto update cycle: Unknown (-1)"));
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, string("config: Auto update weekdays_bitmask: 0x7f"));
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, string("config: Auto update interval: 00:00..24:00"));
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, string("config: Auto update TZ: UTC+3"));
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, string("config: NTP: Use: yes"));
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, string("config: NTP: Use DHCP: no"));
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, string("config: NTP: Server1: time.google.com"));
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, string("config: NTP: Server2: time.cloudflare.com"));
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, string("config: NTP: Server3: time.nist.gov"));
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, string("config: NTP: Server4: pool.ntp.org"));
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, string("config: use company id filter: 1"));
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, string("config: company id: 0x0499"));
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, string("config: use scan coded phy: 0"));
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, string("config: use scan 1mbit/phy: 1"));
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, string("config: use scan extended payload: 1"));
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, string("config: use scan channel 37: 1"));
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, string("config: use scan channel 38: 1"));
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, string("config: use scan channel 39: 1"));
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, string("config: use scan filter: no"));
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, string("config: coordinates: "));
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

TEST_F(TestGwCfg, gw_cfg_print_to_log_ntp_changed) // NOLINT
{
    gw_cfg_t gw_cfg = get_gateway_config_default();

    gw_cfg.ruuvi_cfg.ntp.ntp_use      = false;
    gw_cfg.ruuvi_cfg.ntp.ntp_use_dhcp = true;
    gw_cfg.ruuvi_cfg.ntp.ntp_server1  = (ruuvi_gw_cfg_ntp_server_addr_str_t) { "time1.server.com" };
    gw_cfg.ruuvi_cfg.ntp.ntp_server2  = (ruuvi_gw_cfg_ntp_server_addr_str_t) { "time2.server.com" };
    gw_cfg.ruuvi_cfg.ntp.ntp_server3  = (ruuvi_gw_cfg_ntp_server_addr_str_t) { "time3.server.com" };
    gw_cfg.ruuvi_cfg.ntp.ntp_server4  = (ruuvi_gw_cfg_ntp_server_addr_str_t) { "time4.server.com" };

    esp_log_wrapper_clear();

    gw_cfg_log(&gw_cfg, "Gateway SETTINGS", true);

    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, string("Gateway SETTINGS:"));
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
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, string("config: mqtt server: test.mosquitto.org"));
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, string("config: mqtt port: 1883"));
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
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, string("config: NTP: Use: no"));
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, string("config: NTP: Use DHCP: yes"));
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, string("config: NTP: Server1: time1.server.com"));
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, string("config: NTP: Server2: time2.server.com"));
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, string("config: NTP: Server3: time3.server.com"));
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, string("config: NTP: Server4: time4.server.com"));
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, string("config: use company id filter: 1"));
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, string("config: company id: 0x0499"));
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, string("config: use scan coded phy: 0"));
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, string("config: use scan 1mbit/phy: 1"));
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, string("config: use scan extended payload: 1"));
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, string("config: use scan channel 37: 1"));
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, string("config: use scan channel 38: 1"));
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, string("config: use scan channel 39: 1"));
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, string("config: use scan filter: no"));
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, string("config: coordinates: "));
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

TEST_F(TestGwCfg, gw_cfg_print_to_log_scan_filter_1) // NOLINT
{
    gw_cfg_t gw_cfg = get_gateway_config_default();

    gw_cfg.ruuvi_cfg.scan_filter.scan_filter_allow_listed = false;
    gw_cfg.ruuvi_cfg.scan_filter.scan_filter_length       = 1;
    mac_addr_from_str("AA:BB:CC:DD:EE:FF", &gw_cfg.ruuvi_cfg.scan_filter.scan_filter_list[0]);

    esp_log_wrapper_clear();

    gw_cfg_log(&gw_cfg, "Gateway SETTINGS", true);

    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, string("Gateway SETTINGS:"));
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
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, string("config: mqtt server: test.mosquitto.org"));
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, string("config: mqtt port: 1883"));
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
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, string("config: NTP: Server3: time.nist.gov"));
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, string("config: NTP: Server4: pool.ntp.org"));
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, string("config: use company id filter: 1"));
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, string("config: company id: 0x0499"));
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, string("config: use scan coded phy: 0"));
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, string("config: use scan 1mbit/phy: 1"));
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, string("config: use scan extended payload: 1"));
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, string("config: use scan channel 37: 1"));
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, string("config: use scan channel 38: 1"));
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, string("config: use scan channel 39: 1"));
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, string("config: use scan filter: yes"));
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, string("config: scan filter: allow_listed: 0"));
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, string("config: scan filter [ 0]: AA:BB:CC:DD:EE:FF"));
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, string("config: coordinates: "));
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

TEST_F(TestGwCfg, gw_cfg_print_to_log_scan_filter_2) // NOLINT
{
    gw_cfg_t gw_cfg = get_gateway_config_default();

    gw_cfg.ruuvi_cfg.scan_filter.scan_filter_allow_listed = true;
    gw_cfg.ruuvi_cfg.scan_filter.scan_filter_length       = 2;
    mac_addr_from_str("A0:B0:C0:D0:E0:F0", &gw_cfg.ruuvi_cfg.scan_filter.scan_filter_list[0]);
    mac_addr_from_str("A1:B1:C1:D1:E1:F1", &gw_cfg.ruuvi_cfg.scan_filter.scan_filter_list[1]);

    esp_log_wrapper_clear();

    gw_cfg_log(&gw_cfg, "Gateway SETTINGS", true);

    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, string("Gateway SETTINGS:"));
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
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, string("config: mqtt server: test.mosquitto.org"));
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, string("config: mqtt port: 1883"));
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
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, string("config: NTP: Server3: time.nist.gov"));
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, string("config: NTP: Server4: pool.ntp.org"));
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, string("config: use company id filter: 1"));
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, string("config: company id: 0x0499"));
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, string("config: use scan coded phy: 0"));
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, string("config: use scan 1mbit/phy: 1"));
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, string("config: use scan extended payload: 1"));
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, string("config: use scan channel 37: 1"));
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, string("config: use scan channel 38: 1"));
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, string("config: use scan channel 39: 1"));
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, string("config: use scan filter: yes"));
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, string("config: scan filter: allow_listed: 1"));
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, string("config: scan filter [ 0]: A0:B0:C0:D0:E0:F0"));
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, string("config: scan filter [ 1]: A1:B1:C1:D1:E1:F1"));
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, string("config: coordinates: "));
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
