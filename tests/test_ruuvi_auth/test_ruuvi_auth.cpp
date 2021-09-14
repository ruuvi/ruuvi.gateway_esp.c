/**
 * @file test_ruuvi_auth.cpp
 * @author TheSomeMan
 * @date 2021-09-14
 * @copyright Ruuvi Innovations Ltd, license BSD-3-Clause.
 */

#include "ruuvi_auth.h"
#include "gtest/gtest.h"
#include <string>
#include "ruuvi_device_id.h"
#include "esp_log_wrapper.hpp"
#include "http_server_auth.h"
#include "gw_cfg.h"

using namespace std;

class TestRuuviAuth;
static TestRuuviAuth *g_pTestClass;

/*** Google-test class implementation
 * *********************************************************************************/

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

    string m_device_id {};

    void
    setDeviceId(const string &device_id)
    {
        this->m_device_id = device_id;
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
    snprintf(&g_gateway_config.lan_auth.lan_auth_pass[0], sizeof(g_gateway_config.lan_auth.lan_auth_pass), "qwe");
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
    snprintf(&g_gateway_config.lan_auth.lan_auth_user[0], sizeof(g_gateway_config.lan_auth.lan_auth_user), "user1");
    snprintf(&g_gateway_config.lan_auth.lan_auth_pass[0], sizeof(g_gateway_config.lan_auth.lan_auth_pass), "qwe");
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
    snprintf(
        &g_gateway_config.lan_auth.lan_auth_type[0],
        sizeof(g_gateway_config.lan_auth.lan_auth_type),
        HTTP_SERVER_AUTH_TYPE_STR_DIGEST);
    snprintf(&g_gateway_config.lan_auth.lan_auth_user[0], sizeof(g_gateway_config.lan_auth.lan_auth_user), "user1");
    snprintf(&g_gateway_config.lan_auth.lan_auth_pass[0], sizeof(g_gateway_config.lan_auth.lan_auth_pass), "qwe");
    ASSERT_TRUE(ruuvi_auth_set_from_config());
    const http_server_auth_info_t *const p_auth_info = http_server_get_auth();
    ASSERT_NE(nullptr, p_auth_info);
    ASSERT_EQ(HTTP_SERVER_AUTH_TYPE_DIGEST, p_auth_info->auth_type);
    ASSERT_EQ("user1", string(p_auth_info->auth_user));
    ASSERT_EQ("qwe", string(p_auth_info->auth_pass));
}
