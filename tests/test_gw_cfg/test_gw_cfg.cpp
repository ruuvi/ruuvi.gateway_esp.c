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
#include "gw_cfg_default.h"
#include "esp_log_wrapper.hpp"
#include "os_mutex_recursive.h"

using namespace std;

/*** Google-test class implementation
 * *********************************************************************************/

class TestGwCfg;
static TestGwCfg *g_pTestClass;

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

class TestGwCfg : public ::testing::Test
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

        snprintf(g_gw_wifi_ssid.ssid_buf, sizeof(g_gw_wifi_ssid.ssid_buf), "my_ssid1");
        const nrf52_device_id_str_t device_id_str = { "11:22:33:44:55:66:77:88" };
        gw_cfg_default_init(&g_gw_wifi_ssid, device_id_str);
    }

    void
    TearDown() override
    {
        g_pTestClass = nullptr;
        esp_log_wrapper_deinit();
    }

public:
    TestGwCfg();

    ~TestGwCfg() override;

    MemAllocTrace m_mem_alloc_trace;
    uint32_t      m_malloc_cnt {};
    uint32_t      m_malloc_fail_on_cnt {};
    string        m_fw_ver {};
    string        m_nrf52_fw_ver {};
};

TestGwCfg::TestGwCfg()
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

TestGwCfg::~TestGwCfg() = default;

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

TEST_F(TestGwCfg, gw_cfg_print_to_log_default) // NOLINT
{
    const ruuvi_gateway_config_t gw_cfg = get_gateway_config_default();
    gw_cfg_print_to_log(&gw_cfg, "Gateway SETTINGS");
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, string("Gateway SETTINGS:"));
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, string("config: use eth: 0"));
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, string("config: use eth dhcp: 1"));
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, string("config: eth static ip: "));
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, string("config: eth netmask: "));
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, string("config: eth gw: "));
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, string("config: eth dns1: "));
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, string("config: eth dns2: "));
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, string("config: use mqtt: 0"));
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, string("config: mqtt transport: TCP"));
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, string("config: mqtt server: "));
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, string("config: mqtt port: 0"));
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, string("config: mqtt use default prefix: 1"));
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, string("config: mqtt prefix: "));
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, string("config: mqtt client id: "));
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, string("config: mqtt user: "));
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, string("config: mqtt password: ********"));
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, string("config: use http: 1"));
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, string("config: http url: " RUUVI_GATEWAY_HTTP_DEFAULT_URL));
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, string("config: http user: "));
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, string("config: http pass: ********"));
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, string("config: use http_stat: 1"));
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, string("config: http_stat url: " RUUVI_GATEWAY_HTTP_STATUS_URL));
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, string("config: http_stat user: "));
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, string("config: http_stat pass: ********"));
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, string("config: LAN auth type: lan_auth_ruuvi"));
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, string("config: LAN auth user: Admin"));
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, string("config: LAN auth pass: ********"));
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, string("config: LAN auth API key: ********"));
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, string("config: Auto update cycle: regular"));
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, string("config: Auto update weekdays_bitmask: 0x7f"));
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, string("config: Auto update interval: 00:00..24:00"));
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, string("config: Auto update TZ: UTC+3"));
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, string("config: coordinates: "));
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, string("config: use company id filter: 1"));
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, string("config: company id: 0x0499"));
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, string("config: use scan coded phy: 0"));
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, string("config: use scan 1mbit/phy: 1"));
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, string("config: use scan extended payload: 1"));
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, string("config: use scan channel 37: 1"));
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, string("config: use scan channel 38: 1"));
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, string("config: use scan channel 39: 1"));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
}

TEST_F(TestGwCfg, gw_cfg_print_to_log_default_auto_update_cycle_beta_tester_and_negative_tz) // NOLINT
{
    ruuvi_gateway_config_t gw_cfg                  = get_gateway_config_default();
    gw_cfg.auto_update.auto_update_cycle           = AUTO_UPDATE_CYCLE_TYPE_BETA_TESTER;
    gw_cfg.auto_update.auto_update_tz_offset_hours = -5;

    gw_cfg_print_to_log(&gw_cfg, "Gateway SETTINGS");

    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, string("Gateway SETTINGS:"));
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, string("config: use eth: 0"));
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, string("config: use eth dhcp: 1"));
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, string("config: eth static ip: "));
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, string("config: eth netmask: "));
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, string("config: eth gw: "));
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, string("config: eth dns1: "));
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, string("config: eth dns2: "));
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, string("config: use mqtt: 0"));
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, string("config: mqtt transport: TCP"));
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, string("config: mqtt server: "));
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, string("config: mqtt port: 0"));
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, string("config: mqtt use default prefix: 1"));
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, string("config: mqtt prefix: "));
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, string("config: mqtt client id: "));
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, string("config: mqtt user: "));
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, string("config: mqtt password: ********"));
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, string("config: use http: 1"));
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, string("config: http url: " RUUVI_GATEWAY_HTTP_DEFAULT_URL));
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, string("config: http user: "));
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, string("config: http pass: ********"));
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, string("config: use http_stat: 1"));
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, string("config: http_stat url: " RUUVI_GATEWAY_HTTP_STATUS_URL));
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, string("config: http_stat user: "));
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, string("config: http_stat pass: ********"));
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, string("config: LAN auth type: lan_auth_ruuvi"));
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, string("config: LAN auth user: Admin"));
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, string("config: LAN auth pass: ********"));
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, string("config: LAN auth API key: ********"));
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, string("config: Auto update cycle: beta"));
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, string("config: Auto update weekdays_bitmask: 0x7f"));
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, string("config: Auto update interval: 00:00..24:00"));
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, string("config: Auto update TZ: UTC-5"));
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, string("config: coordinates: "));
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, string("config: use company id filter: 1"));
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, string("config: company id: 0x0499"));
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, string("config: use scan coded phy: 0"));
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, string("config: use scan 1mbit/phy: 1"));
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, string("config: use scan extended payload: 1"));
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, string("config: use scan channel 37: 1"));
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, string("config: use scan channel 38: 1"));
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, string("config: use scan channel 39: 1"));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
}

TEST_F(TestGwCfg, gw_cfg_print_to_log_default_auto_update_cycle_manual) // NOLINT
{
    ruuvi_gateway_config_t gw_cfg        = get_gateway_config_default();
    gw_cfg.auto_update.auto_update_cycle = AUTO_UPDATE_CYCLE_TYPE_MANUAL;

    gw_cfg_print_to_log(&gw_cfg, "Gateway SETTINGS");

    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, string("Gateway SETTINGS:"));
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, string("config: use eth: 0"));
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, string("config: use eth dhcp: 1"));
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, string("config: eth static ip: "));
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, string("config: eth netmask: "));
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, string("config: eth gw: "));
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, string("config: eth dns1: "));
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, string("config: eth dns2: "));
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, string("config: use mqtt: 0"));
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, string("config: mqtt transport: TCP"));
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, string("config: mqtt server: "));
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, string("config: mqtt port: 0"));
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, string("config: mqtt use default prefix: 1"));
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, string("config: mqtt prefix: "));
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, string("config: mqtt client id: "));
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, string("config: mqtt user: "));
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, string("config: mqtt password: ********"));
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, string("config: use http: 1"));
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, string("config: http url: " RUUVI_GATEWAY_HTTP_DEFAULT_URL));
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, string("config: http user: "));
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, string("config: http pass: ********"));
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, string("config: use http_stat: 1"));
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, string("config: http_stat url: " RUUVI_GATEWAY_HTTP_STATUS_URL));
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, string("config: http_stat user: "));
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, string("config: http_stat pass: ********"));
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, string("config: LAN auth type: lan_auth_ruuvi"));
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, string("config: LAN auth user: Admin"));
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, string("config: LAN auth pass: ********"));
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, string("config: LAN auth API key: ********"));
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, string("config: Auto update cycle: manual"));
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, string("config: Auto update weekdays_bitmask: 0x7f"));
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, string("config: Auto update interval: 00:00..24:00"));
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, string("config: Auto update TZ: UTC+3"));
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, string("config: coordinates: "));
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, string("config: use company id filter: 1"));
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, string("config: company id: 0x0499"));
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, string("config: use scan coded phy: 0"));
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, string("config: use scan 1mbit/phy: 1"));
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, string("config: use scan extended payload: 1"));
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, string("config: use scan channel 37: 1"));
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, string("config: use scan channel 38: 1"));
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, string("config: use scan channel 39: 1"));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
}

TEST_F(TestGwCfg, gw_cfg_print_to_log_default_auto_update_cycle_invalid) // NOLINT
{
    ruuvi_gateway_config_t gw_cfg        = get_gateway_config_default();
    gw_cfg.auto_update.auto_update_cycle = (auto_update_cycle_type_e)-1;

    gw_cfg_print_to_log(&gw_cfg, "Gateway SETTINGS");

    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, string("Gateway SETTINGS:"));
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, string("config: use eth: 0"));
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, string("config: use eth dhcp: 1"));
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, string("config: eth static ip: "));
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, string("config: eth netmask: "));
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, string("config: eth gw: "));
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, string("config: eth dns1: "));
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, string("config: eth dns2: "));
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, string("config: use mqtt: 0"));
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, string("config: mqtt transport: TCP"));
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, string("config: mqtt server: "));
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, string("config: mqtt port: 0"));
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, string("config: mqtt use default prefix: 1"));
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, string("config: mqtt prefix: "));
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, string("config: mqtt client id: "));
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, string("config: mqtt user: "));
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, string("config: mqtt password: ********"));
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, string("config: use http: 1"));
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, string("config: http url: " RUUVI_GATEWAY_HTTP_DEFAULT_URL));
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, string("config: http user: "));
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, string("config: http pass: ********"));
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, string("config: use http_stat: 1"));
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, string("config: http_stat url: " RUUVI_GATEWAY_HTTP_STATUS_URL));
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, string("config: http_stat user: "));
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, string("config: http_stat pass: ********"));
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, string("config: LAN auth type: lan_auth_ruuvi"));
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, string("config: LAN auth user: Admin"));
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, string("config: LAN auth pass: ********"));
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, string("config: LAN auth API key: ********"));
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, string("config: Auto update cycle: manual (-1)"));
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, string("config: Auto update weekdays_bitmask: 0x7f"));
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, string("config: Auto update interval: 00:00..24:00"));
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, string("config: Auto update TZ: UTC+3"));
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, string("config: coordinates: "));
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, string("config: use company id filter: 1"));
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, string("config: company id: 0x0499"));
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, string("config: use scan coded phy: 0"));
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, string("config: use scan 1mbit/phy: 1"));
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, string("config: use scan extended payload: 1"));
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, string("config: use scan channel 37: 1"));
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, string("config: use scan channel 38: 1"));
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, string("config: use scan channel 39: 1"));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
}
