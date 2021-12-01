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
    TestRuuviAuth();

    ~TestRuuviAuth() override;

    string                       m_device_id {};
    MemAllocTrace                m_mem_alloc_trace {};
    uint32_t                     m_malloc_cnt {};
    uint32_t                     m_malloc_fail_on_cnt {};
    wifiman_md5_digest_hex_str_t password_md5 {};

    void
    setDeviceId(const string &device_id)
    {
        this->m_device_id = device_id;

        str_buf_t str_buf = str_buf_printf_with_alloc(
            "%s:%s:%s",
            RUUVI_GATEWAY_AUTH_DEFAULT_USER,
            g_gw_wifi_ssid.ssid_buf,
            device_id.c_str());

        this->password_md5 = wifiman_md5_calc_hex_str(str_buf.buf, str_buf_get_len(&str_buf));

        gw_cfg_default_set_lan_auth_password(password_md5.buf);
        gw_cfg_set_default_lan_auth();
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

nrf52_device_id_str_t
ruuvi_device_id_get_str(void)
{
    nrf52_device_id_str_t device_id_str {};
    assert(g_pTestClass->m_device_id.length() > 0);
    snprintf(&device_id_str.str_buf[0], sizeof(device_id_str.str_buf), "%s", g_pTestClass->m_device_id.c_str());
    return device_id_str;
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
os_mutex_recursive_lock(os_mutex_recursive_t const h_mutex)
{
}

void
os_mutex_recursive_unlock(os_mutex_recursive_t const h_mutex)
{
}
}

static ruuvi_gateway_config_t
get_gateway_config_default()
{
    ruuvi_gateway_config_t gw_cfg {};
    gw_cfg_default_get(&gw_cfg);
    return gw_cfg;
}

/*** Unit-Tests
 * *******************************************************************************************************/

TEST_F(TestRuuviAuth, test_default_auth_zero_id) // NOLINT
{
    this->setDeviceId("00:00:00:00:00:00:00:00");
    ASSERT_TRUE(ruuvi_auth_set_from_config());
    const http_server_auth_info_t *const p_auth_info = http_server_get_auth();
    ASSERT_NE(nullptr, p_auth_info);
    ASSERT_EQ(HTTP_SERVER_AUTH_TYPE_RUUVI, p_auth_info->auth_type);
    ASSERT_EQ("Admin", string(p_auth_info->auth_user));
    ASSERT_EQ("a54439b7a379620e26445a27b2785d76", string(p_auth_info->auth_pass));
}

TEST_F(TestRuuviAuth, test_default_auth_non_zero_id) // NOLINT
{
    this->setDeviceId("01:02:03:04:05:06:0A:0B");
    ASSERT_TRUE(ruuvi_auth_set_from_config());
    const http_server_auth_info_t *const p_auth_info = http_server_get_auth();
    ASSERT_NE(nullptr, p_auth_info);
    ASSERT_EQ(HTTP_SERVER_AUTH_TYPE_RUUVI, p_auth_info->auth_type);
    ASSERT_EQ("Admin", string(p_auth_info->auth_user));
    ASSERT_EQ("d39f824d982960f45c28a4823e97b087", string(p_auth_info->auth_pass));
}

TEST_F(TestRuuviAuth, test_non_default_auth_password) // NOLINT
{
    this->setDeviceId("01:02:03:04:05:06:0A:0B");
    ruuvi_gateway_config_t gw_cfg_tmp = get_gateway_config_default();
    snprintf(&gw_cfg_tmp.lan_auth.lan_auth_pass[0], sizeof(gw_cfg_tmp.lan_auth.lan_auth_pass), "qwe");
    gw_cfg_update(&gw_cfg_tmp, false);
    ASSERT_TRUE(ruuvi_auth_set_from_config());
    const http_server_auth_info_t *const p_auth_info = http_server_get_auth();
    ASSERT_NE(nullptr, p_auth_info);
    ASSERT_EQ(HTTP_SERVER_AUTH_TYPE_RUUVI, p_auth_info->auth_type);
    ASSERT_EQ("Admin", string(p_auth_info->auth_user));
    ASSERT_EQ("qwe", string(p_auth_info->auth_pass));
}

TEST_F(TestRuuviAuth, test_non_default_auth_user_password) // NOLINT
{
    this->setDeviceId("01:02:03:04:05:06:0A:0B");
    ruuvi_gateway_config_t gw_cfg_tmp = get_gateway_config_default();
    snprintf(&gw_cfg_tmp.lan_auth.lan_auth_user[0], sizeof(gw_cfg_tmp.lan_auth.lan_auth_user), "user1");
    snprintf(&gw_cfg_tmp.lan_auth.lan_auth_pass[0], sizeof(gw_cfg_tmp.lan_auth.lan_auth_pass), "qwe");
    gw_cfg_update(&gw_cfg_tmp, false);
    ASSERT_TRUE(ruuvi_auth_set_from_config());
    const http_server_auth_info_t *const p_auth_info = http_server_get_auth();
    ASSERT_NE(nullptr, p_auth_info);
    ASSERT_EQ(HTTP_SERVER_AUTH_TYPE_RUUVI, p_auth_info->auth_type);
    ASSERT_EQ("user1", string(p_auth_info->auth_user));
    ASSERT_EQ("qwe", string(p_auth_info->auth_pass));
}

TEST_F(TestRuuviAuth, test_non_default_auth_type_user_password) // NOLINT
{
    this->setDeviceId("01:02:03:04:05:06:0A:0B");
    ruuvi_gateway_config_t gw_cfg_tmp = get_gateway_config_default();
    snprintf(
        &gw_cfg_tmp.lan_auth.lan_auth_type[0],
        sizeof(gw_cfg_tmp.lan_auth.lan_auth_type),
        HTTP_SERVER_AUTH_TYPE_STR_DIGEST);
    snprintf(&gw_cfg_tmp.lan_auth.lan_auth_user[0], sizeof(gw_cfg_tmp.lan_auth.lan_auth_user), "user1");
    snprintf(&gw_cfg_tmp.lan_auth.lan_auth_pass[0], sizeof(gw_cfg_tmp.lan_auth.lan_auth_pass), "qwe");
    gw_cfg_update(&gw_cfg_tmp, false);
    ASSERT_TRUE(ruuvi_auth_set_from_config());
    const http_server_auth_info_t *const p_auth_info = http_server_get_auth();
    ASSERT_NE(nullptr, p_auth_info);
    ASSERT_EQ(HTTP_SERVER_AUTH_TYPE_DIGEST, p_auth_info->auth_type);
    ASSERT_EQ("user1", string(p_auth_info->auth_user));
    ASSERT_EQ("qwe", string(p_auth_info->auth_pass));
}
