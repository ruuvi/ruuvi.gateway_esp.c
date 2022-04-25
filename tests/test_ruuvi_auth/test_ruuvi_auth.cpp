/**
 * @file test_ruuvi_auth.cpp
 * @author TheSomeMan
 * @date 2021-09-14
 * @copyright Ruuvi Innovations Ltd, license BSD-3-Clause.
 */

#include "ruuvi_auth.h"
#include "gtest/gtest.h"
#include <string>
#include "os_mutex_recursive.h"
#include "ruuvi_device_id.h"
#include "esp_log_wrapper.hpp"
#include "http_server_auth.h"
#include "gw_cfg.h"
#include "gw_cfg_default.h"
#include "str_buf.h"
#include "wifiman_md5.h"
#include "os_mutex.h"
#include "lwip/ip4_addr.h"

using namespace std;

class TestRuuviAuth;
static TestRuuviAuth *g_pTestClass;

/*** Google-test class implementation
 * *********************************************************************************/

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

class TestRuuviAuth : public ::testing::Test
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
    }

    void
    TearDown() override
    {
        g_pTestClass = nullptr;
        gw_cfg_deinit();
        esp_log_wrapper_deinit();
    }

public:
    TestRuuviAuth();

    ~TestRuuviAuth() override;

    MemAllocTrace m_mem_alloc_trace {};
    uint32_t      m_malloc_cnt {};
    uint32_t      m_malloc_fail_on_cnt {};

    void
    initGwCfg(const nrf52_device_id_t &device_id)
    {
        const gw_cfg_default_init_param_t init_params = {
            .wifi_ap_ssid        = { "my_ssid1" },
            .device_id           = device_id,
            .esp32_fw_ver        = { "v1.10.0" },
            .nrf52_fw_ver        = { "v0.7.1" },
            .nrf52_mac_addr      = { 0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF },
            .esp32_mac_addr_wifi = { 0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0x11 },
            .esp32_mac_addr_eth  = { 0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0x22 },
        };
        gw_cfg_default_init(&init_params, nullptr);
        gw_cfg_init();
    }
};

TestRuuviAuth::TestRuuviAuth()
    : Test()
{
}

TestRuuviAuth::~TestRuuviAuth() = default;

extern "C" {

const char *
os_task_get_name(void)
{
    static const char g_task_name[] = "main";
    return const_cast<char *>(g_task_name);
}

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

os_mutex_t
os_mutex_create_static(os_mutex_static_t *const p_mutex_static)
{
    return reinterpret_cast<os_mutex_t>(p_mutex_static);
}

void
os_mutex_delete(os_mutex_t *const ph_mutex)
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

char *
esp_ip4addr_ntoa(const esp_ip4_addr_t *addr, char *buf, int buflen)
{
    return ip4addr_ntoa_r((ip4_addr_t *)addr, buf, buflen);
}

uint32_t
esp_ip4addr_aton(const char *addr)
{
    return ipaddr_addr(addr);
}

void
wifi_manager_cb_save_wifi_config(const wifiman_config_t *const p_cfg)
{
}

} // extern "C"

static gw_cfg_t
get_gateway_config_default()
{
    gw_cfg_t gw_cfg {};
    gw_cfg_default_get(&gw_cfg);
    return gw_cfg;
}

/*** Unit-Tests
 * *******************************************************************************************************/

TEST_F(TestRuuviAuth, test_default_auth_zero_id) // NOLINT
{
    this->initGwCfg((nrf52_device_id_t) { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 });
    ASSERT_TRUE(ruuvi_auth_set_from_config());
    const http_server_auth_info_t *const p_auth_info = http_server_get_auth();
    ASSERT_NE(nullptr, p_auth_info);
    ASSERT_EQ(HTTP_SERVER_AUTH_TYPE_RUUVI, p_auth_info->auth_type);
    ASSERT_EQ("Admin", string(p_auth_info->auth_user.buf));
    ASSERT_EQ("6bd2d4090d98c5a5c9992fbb35d6b821", string(p_auth_info->auth_pass.buf));
}

TEST_F(TestRuuviAuth, test_default_auth_non_zero_id) // NOLINT
{
    this->initGwCfg((nrf52_device_id_t) { 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x0A, 0x0B });
    ASSERT_TRUE(ruuvi_auth_set_from_config());
    const http_server_auth_info_t *const p_auth_info = http_server_get_auth();
    ASSERT_NE(nullptr, p_auth_info);
    ASSERT_EQ(HTTP_SERVER_AUTH_TYPE_RUUVI, p_auth_info->auth_type);
    ASSERT_EQ("Admin", string(p_auth_info->auth_user.buf));
    ASSERT_EQ("cc6b2a4405af90c7a738f72b64f3c34e", string(p_auth_info->auth_pass.buf));
}

TEST_F(TestRuuviAuth, test_non_default_auth_password) // NOLINT
{
    this->initGwCfg((nrf52_device_id_t) { 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x0A, 0x0B });
    gw_cfg_t gw_cfg_tmp = get_gateway_config_default();
    snprintf(
        &gw_cfg_tmp.ruuvi_cfg.lan_auth.lan_auth_pass.buf[0],
        sizeof(gw_cfg_tmp.ruuvi_cfg.lan_auth.lan_auth_pass.buf),
        "qwe");
    gw_cfg_update_ruuvi_cfg(&gw_cfg_tmp.ruuvi_cfg);
    ASSERT_TRUE(ruuvi_auth_set_from_config());
    const http_server_auth_info_t *const p_auth_info = http_server_get_auth();
    ASSERT_NE(nullptr, p_auth_info);
    ASSERT_EQ(HTTP_SERVER_AUTH_TYPE_RUUVI, p_auth_info->auth_type);
    ASSERT_EQ("Admin", string(p_auth_info->auth_user.buf));
    ASSERT_EQ("qwe", string(p_auth_info->auth_pass.buf));
}

TEST_F(TestRuuviAuth, test_non_default_auth_user_password) // NOLINT
{
    this->initGwCfg((nrf52_device_id_t) { 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x0A, 0x0B });
    gw_cfg_t gw_cfg_tmp = get_gateway_config_default();
    snprintf(
        &gw_cfg_tmp.ruuvi_cfg.lan_auth.lan_auth_user.buf[0],
        sizeof(gw_cfg_tmp.ruuvi_cfg.lan_auth.lan_auth_user.buf),
        "user1");
    snprintf(
        &gw_cfg_tmp.ruuvi_cfg.lan_auth.lan_auth_pass.buf[0],
        sizeof(gw_cfg_tmp.ruuvi_cfg.lan_auth.lan_auth_pass.buf),
        "qwe");
    gw_cfg_update_ruuvi_cfg(&gw_cfg_tmp.ruuvi_cfg);
    ASSERT_TRUE(ruuvi_auth_set_from_config());
    const http_server_auth_info_t *const p_auth_info = http_server_get_auth();
    ASSERT_NE(nullptr, p_auth_info);
    ASSERT_EQ(HTTP_SERVER_AUTH_TYPE_RUUVI, p_auth_info->auth_type);
    ASSERT_EQ("user1", string(p_auth_info->auth_user.buf));
    ASSERT_EQ("qwe", string(p_auth_info->auth_pass.buf));
}

TEST_F(TestRuuviAuth, test_non_default_auth_type_user_password) // NOLINT
{
    this->initGwCfg((nrf52_device_id_t) { 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x0A, 0x0B });
    gw_cfg_t gw_cfg_tmp                         = get_gateway_config_default();
    gw_cfg_tmp.ruuvi_cfg.lan_auth.lan_auth_type = HTTP_SERVER_AUTH_TYPE_DIGEST;
    snprintf(
        &gw_cfg_tmp.ruuvi_cfg.lan_auth.lan_auth_user.buf[0],
        sizeof(gw_cfg_tmp.ruuvi_cfg.lan_auth.lan_auth_user.buf),
        "user1");
    snprintf(
        &gw_cfg_tmp.ruuvi_cfg.lan_auth.lan_auth_pass.buf[0],
        sizeof(gw_cfg_tmp.ruuvi_cfg.lan_auth.lan_auth_pass.buf),
        "qwe");
    gw_cfg_update_ruuvi_cfg(&gw_cfg_tmp.ruuvi_cfg);
    ASSERT_TRUE(ruuvi_auth_set_from_config());
    const http_server_auth_info_t *const p_auth_info = http_server_get_auth();
    ASSERT_NE(nullptr, p_auth_info);
    ASSERT_EQ(HTTP_SERVER_AUTH_TYPE_DIGEST, p_auth_info->auth_type);
    ASSERT_EQ("user1", string(p_auth_info->auth_user.buf));
    ASSERT_EQ("qwe", string(p_auth_info->auth_pass.buf));
}
