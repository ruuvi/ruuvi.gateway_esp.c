/**
 * @file test_adv_post_statistics.cpp
 * @author TheSomeMan
 * @date 2023-09-17
 * @copyright Ruuvi Innovations Ltd, license BSD-3-Clause.
 */

#include "adv_post_statistics.h"
#include "gtest/gtest.h"
#include <string>
#include "esp_system.h"
#include "os_malloc.h"

using namespace std;

class TestAdvPostStatistics;

static TestAdvPostStatistics* g_pTestClass;

class MemAllocTrace
{
    vector<void*> allocated_mem;

    std::vector<void*>::iterator
    find(void* p_mem)
    {
        for (auto iter = this->allocated_mem.begin(); iter != this->allocated_mem.end(); ++iter)
        {
            if (*iter == p_mem)
            {
                return iter;
            }
        }
        return this->allocated_mem.end();
    }

public:
    void
    add(void* p_mem)
    {
        auto iter = find(p_mem);
        assert(iter == this->allocated_mem.end()); // p_mem was found in the list of allocated memory blocks
        this->allocated_mem.push_back(p_mem);
    }
    void
    remove(void* p_mem)
    {
        auto iter = find(p_mem);
        assert(iter != this->allocated_mem.end()); // p_mem was not found in the list of allocated memory blocks
        this->allocated_mem.erase(iter);
    }
    bool
    is_empty()
    {
        return this->allocated_mem.empty();
    }
};

/*** Google-test class implementation
 * *********************************************************************************/

class TestAdvPostStatistics : public ::testing::Test
{
private:
protected:
    void
    SetUp() override
    {
        g_pTestClass                 = this;
        this->m_malloc_cnt           = 0;
        this->m_malloc_fail_on_cnt   = 0;
        this->m_esp_random_cnt       = 0;
        this->m_reset_info_cnt       = 0;
        this->m_is_connected_to_wifi = false;
        this->m_nrf_status           = false;
        this->m_esp_reset_reason     = ESP_RST_POWERON;
        this->m_http_post_stat_res   = false;
        this->m_cfg_http_stat        = {};
        this->m_adv_report_table     = {};

        adv_post_statistics_init();
    }

    void
    TearDown() override
    {
        g_pTestClass = nullptr;
    }

public:
    TestAdvPostStatistics();

    ~TestAdvPostStatistics() override;

    MemAllocTrace            m_mem_alloc_trace;
    uint32_t                 m_malloc_cnt {};
    uint32_t                 m_malloc_fail_on_cnt {};
    uint32_t                 m_esp_random_cnt {};
    uint32_t                 m_reset_info_cnt {};
    bool                     m_is_connected_to_wifi {};
    bool                     m_nrf_status {};
    esp_reset_reason_t       m_esp_reset_reason {};
    ruuvi_gw_cfg_http_stat_t m_cfg_http_stat {};
    bool                     m_http_post_stat_res {};
    adv_report_table_t       m_adv_report_table {};

    http_json_statistics_info_t m_http_post_stat_arg_stat_info;
    string                      m_http_post_stat_arg_stat_info_reset_info_str;
    adv_report_table_t          m_http_post_stat_arg_reports;
    ruuvi_gw_cfg_http_stat_t    m_http_post_stat_arg_cfg_http_stat;
    void*                       m_http_post_stat_arg_user_data;
    bool                        m_http_post_stat_arg_use_ssl_client_cert;
    bool                        m_http_post_stat_arg_use_ssl_server_cert;
};

TestAdvPostStatistics::TestAdvPostStatistics()
    : Test()
{
}

TestAdvPostStatistics::~TestAdvPostStatistics() = default;

extern "C" {

volatile uint32_t g_uptime_counter;
volatile uint32_t g_network_disconnect_cnt;

void*
os_malloc(const size_t size)
{
    assert(nullptr != g_pTestClass);
    if (++g_pTestClass->m_malloc_cnt == g_pTestClass->m_malloc_fail_on_cnt)
    {
        return nullptr;
    }
    void* p_mem = malloc(size);
    assert(nullptr != p_mem);
    g_pTestClass->m_mem_alloc_trace.add(p_mem);
    return p_mem;
}

void
os_free_internal(void* p_mem)
{
    assert(nullptr != g_pTestClass);
    g_pTestClass->m_mem_alloc_trace.remove(p_mem);
    free(p_mem);
}

void*
os_calloc(const size_t nmemb, const size_t size)
{
    assert(nullptr != g_pTestClass);
    if (++g_pTestClass->m_malloc_cnt == g_pTestClass->m_malloc_fail_on_cnt)
    {
        return nullptr;
    }
    void* p_mem = calloc(nmemb, size);
    assert(nullptr != p_mem);
    g_pTestClass->m_mem_alloc_trace.add(p_mem);
    return p_mem;
}

uint32_t
esp_random(void)
{
    return g_pTestClass->m_esp_random_cnt;
}

uint32_t
reset_info_get_cnt(void)
{
    return g_pTestClass->m_reset_info_cnt;
}

bool
wifi_manager_is_connected_to_wifi(void)
{
    return g_pTestClass->m_is_connected_to_wifi;
}

esp_reset_reason_t
esp_reset_reason(void)
{
    return g_pTestClass->m_esp_reset_reason;
}

void
log_runtime_statistics(void)
{
}

str_buf_t
reset_info_get(void)
{
    return str_buf_printf_with_alloc("reset_info");
}

const mac_address_str_t*
gw_cfg_get_nrf52_mac_addr(void)
{
    static const mac_address_str_t mac_addr = { "11:22:33:44:55:66" };
    return &mac_addr;
}

const ruuvi_esp32_fw_ver_str_t*
gw_cfg_get_esp32_fw_ver(void)
{
    static const ruuvi_esp32_fw_ver_str_t fw_ver = { "v1.14.0" };
    return &fw_ver;
}

const ruuvi_nrf52_fw_ver_str_t*
gw_cfg_get_nrf52_fw_ver(void)
{
    static const ruuvi_nrf52_fw_ver_str_t fw_ver = { "v1.1.0" };
    return &fw_ver;
}

bool
gw_status_get_nrf_status(void)
{
    return g_pTestClass->m_nrf_status;
}

ruuvi_gw_cfg_http_stat_t*
gw_cfg_get_http_stat_copy(void)
{
    ruuvi_gw_cfg_http_stat_t* p_cfg_http_stat = (ruuvi_gw_cfg_http_stat_t*)os_calloc(1, sizeof(*p_cfg_http_stat));
    if (NULL == p_cfg_http_stat)
    {
        return NULL;
    }
    memcpy(p_cfg_http_stat, &g_pTestClass->m_cfg_http_stat, sizeof(*p_cfg_http_stat));
    return p_cfg_http_stat;
}

bool
http_post_stat(
    const http_json_statistics_info_t* const p_stat_info,
    const adv_report_table_t* const          p_reports,
    const ruuvi_gw_cfg_http_stat_t* const    p_cfg_http_stat,
    void* const                              p_user_data,
    const bool                               use_ssl_client_cert,
    const bool                               use_ssl_server_cert)
{
    g_pTestClass->m_http_post_stat_arg_stat_info                = *p_stat_info;
    g_pTestClass->m_http_post_stat_arg_stat_info_reset_info_str = string(p_stat_info->p_reset_info);
    g_pTestClass->m_http_post_stat_arg_reports                  = *p_reports;
    g_pTestClass->m_http_post_stat_arg_cfg_http_stat            = *p_cfg_http_stat;
    g_pTestClass->m_http_post_stat_arg_user_data                = p_user_data;
    g_pTestClass->m_http_post_stat_arg_use_ssl_client_cert      = use_ssl_client_cert;
    g_pTestClass->m_http_post_stat_arg_use_ssl_server_cert      = use_ssl_server_cert;

    return g_pTestClass->m_http_post_stat_res;
}

void
adv_table_statistics_read(adv_report_table_t* const p_reports)
{
    *p_reports = g_pTestClass->m_adv_report_table;
}

} // extern "C"

/*** Unit-Tests
 * *******************************************************************************************************/

TEST_F(TestAdvPostStatistics, test_adv_post_statistics_info_generate) // NOLINT
{
    this->m_esp_random_cnt = 0xAA001122;
    adv_post_statistics_init();

    {
        g_uptime_counter             = 10123;
        this->m_nrf_status           = true;
        this->m_is_connected_to_wifi = false;
        g_network_disconnect_cnt     = 157;
        this->m_reset_info_cnt       = 77;
        this->m_esp_reset_reason     = ESP_RST_POWERON;

        str_buf_t reset_info = str_buf_printf_with_alloc("reset reason 123");
        ASSERT_NE(nullptr, reset_info.buf);

        http_json_statistics_info_t* p_stat_info = adv_post_statistics_info_generate(&reset_info);
        ASSERT_NE(nullptr, p_stat_info);
        ASSERT_EQ(string("11:22:33:44:55:66"), string(p_stat_info->nrf52_mac_addr.str_buf));
        ASSERT_EQ(string("v1.14.0"), string(p_stat_info->esp_fw.buf));
        ASSERT_EQ(string("v1.1.0"), string(p_stat_info->nrf_fw.buf));
        ASSERT_EQ(10123, p_stat_info->uptime);
        ASSERT_EQ(0xAA001122, p_stat_info->nonce);
        ASSERT_TRUE(p_stat_info->nrf_status);
        ASSERT_FALSE(p_stat_info->is_connected_to_wifi);
        ASSERT_EQ(157, p_stat_info->network_disconnect_cnt);
        ASSERT_EQ(string("POWER_ON"), string(p_stat_info->reset_reason.buf));
        ASSERT_EQ(77, p_stat_info->reset_cnt);
        ASSERT_EQ(string("reset reason 123"), string(p_stat_info->p_reset_info));
        str_buf_free_buf(&reset_info);
        os_free(p_stat_info);
        ASSERT_TRUE(this->m_mem_alloc_trace.is_empty());
    }
    {
        g_uptime_counter             = 20123;
        this->m_nrf_status           = false;
        this->m_is_connected_to_wifi = true;
        g_network_disconnect_cnt     = 257;
        this->m_reset_info_cnt       = 85;
        this->m_esp_reset_reason     = ESP_RST_PANIC;

        str_buf_t reset_info = str_buf_printf_with_alloc("reset reason 526");
        ASSERT_NE(nullptr, reset_info.buf);

        http_json_statistics_info_t* p_stat_info = adv_post_statistics_info_generate(&reset_info);
        ASSERT_NE(nullptr, p_stat_info);
        ASSERT_EQ(string("11:22:33:44:55:66"), string(p_stat_info->nrf52_mac_addr.str_buf));
        ASSERT_EQ(string("v1.14.0"), string(p_stat_info->esp_fw.buf));
        ASSERT_EQ(string("v1.1.0"), string(p_stat_info->nrf_fw.buf));
        ASSERT_EQ(20123, p_stat_info->uptime);
        ASSERT_EQ(0xAA001122 + 1, p_stat_info->nonce);
        ASSERT_FALSE(p_stat_info->nrf_status);
        ASSERT_TRUE(p_stat_info->is_connected_to_wifi);
        ASSERT_EQ(257, p_stat_info->network_disconnect_cnt);
        ASSERT_EQ(string("PANIC"), string(p_stat_info->reset_reason.buf));
        ASSERT_EQ(85, p_stat_info->reset_cnt);
        ASSERT_EQ(string("reset reason 526"), string(p_stat_info->p_reset_info));
        str_buf_free_buf(&reset_info);
        os_free(p_stat_info);
        ASSERT_TRUE(this->m_mem_alloc_trace.is_empty());
    }
}

TEST_F(TestAdvPostStatistics, adv_post_statistics_do_send) // NOLINT
{
    this->m_esp_random_cnt = 0x33333333;
    adv_post_statistics_init();

    g_uptime_counter             = 30123;
    this->m_nrf_status           = true;
    this->m_is_connected_to_wifi = false;
    g_network_disconnect_cnt     = 258;
    this->m_reset_info_cnt       = 86;
    this->m_esp_reset_reason     = ESP_RST_POWERON;

    {
        this->m_cfg_http_stat = ruuvi_gw_cfg_http_stat_t {
            .use_http_stat                 = true,
            .http_stat_use_ssl_client_cert = false,
            .http_stat_use_ssl_server_cert = false,
            .http_stat_url                 = { "https://stat.ruuvi.com" },
            .http_stat_user                = { "" },
            .http_stat_pass                = { "" },
        };
        this->m_adv_report_table = adv_report_table_t {
            .num_of_advs = 0,
            .table       = {},
        };
        this->m_http_post_stat_res = true;

        ASSERT_TRUE(adv_post_statistics_do_send());

        ASSERT_EQ(this->m_cfg_http_stat.use_http_stat, g_pTestClass->m_http_post_stat_arg_cfg_http_stat.use_http_stat);
        ASSERT_EQ(
            this->m_cfg_http_stat.http_stat_use_ssl_client_cert,
            g_pTestClass->m_http_post_stat_arg_cfg_http_stat.http_stat_use_ssl_client_cert);
        ASSERT_EQ(
            this->m_cfg_http_stat.http_stat_use_ssl_server_cert,
            g_pTestClass->m_http_post_stat_arg_cfg_http_stat.http_stat_use_ssl_server_cert);
        ASSERT_EQ(
            string(this->m_cfg_http_stat.http_stat_url.buf),
            string(g_pTestClass->m_http_post_stat_arg_cfg_http_stat.http_stat_url.buf));
        ASSERT_EQ(
            string(this->m_cfg_http_stat.http_stat_user.buf),
            string(g_pTestClass->m_http_post_stat_arg_cfg_http_stat.http_stat_user.buf));
        ASSERT_EQ(
            string(this->m_cfg_http_stat.http_stat_pass.buf),
            string(g_pTestClass->m_http_post_stat_arg_cfg_http_stat.http_stat_pass.buf));

        ASSERT_EQ(
            string("11:22:33:44:55:66"),
            string(g_pTestClass->m_http_post_stat_arg_stat_info.nrf52_mac_addr.str_buf));
        ASSERT_EQ(string("v1.14.0"), string(g_pTestClass->m_http_post_stat_arg_stat_info.esp_fw.buf));
        ASSERT_EQ(string("v1.1.0"), string(g_pTestClass->m_http_post_stat_arg_stat_info.nrf_fw.buf));
        ASSERT_EQ(30123, g_pTestClass->m_http_post_stat_arg_stat_info.uptime);
        ASSERT_EQ(0x33333333, g_pTestClass->m_http_post_stat_arg_stat_info.nonce);
        ASSERT_TRUE(g_pTestClass->m_http_post_stat_arg_stat_info.nrf_status);
        ASSERT_FALSE(g_pTestClass->m_http_post_stat_arg_stat_info.is_connected_to_wifi);
        ASSERT_EQ(258, g_pTestClass->m_http_post_stat_arg_stat_info.network_disconnect_cnt);
        ASSERT_EQ(string("POWER_ON"), string(g_pTestClass->m_http_post_stat_arg_stat_info.reset_reason.buf));
        ASSERT_EQ(86, g_pTestClass->m_http_post_stat_arg_stat_info.reset_cnt);
        ASSERT_EQ(string("reset_info"), string(g_pTestClass->m_http_post_stat_arg_stat_info_reset_info_str));

        ASSERT_EQ(0, g_pTestClass->m_http_post_stat_arg_reports.num_of_advs);

        ASSERT_EQ(nullptr, g_pTestClass->m_http_post_stat_arg_user_data);
        ASSERT_EQ(
            this->m_cfg_http_stat.http_stat_use_ssl_client_cert,
            g_pTestClass->m_http_post_stat_arg_use_ssl_client_cert);
        ASSERT_EQ(
            this->m_cfg_http_stat.http_stat_use_ssl_server_cert,
            g_pTestClass->m_http_post_stat_arg_use_ssl_server_cert);

        ASSERT_TRUE(this->m_mem_alloc_trace.is_empty());
    }
    {
        this->m_cfg_http_stat = ruuvi_gw_cfg_http_stat_t {
            .use_http_stat                 = true,
            .http_stat_use_ssl_client_cert = true,
            .http_stat_use_ssl_server_cert = false,
            .http_stat_url                 = { "https://stat2.ruuvi.com" },
            .http_stat_user                = { "user123" },
            .http_stat_pass                = { "pass123" },
        };
        this->m_adv_report_table = adv_report_table_t {
            .num_of_advs = 1,
            .table = {
                [0] = {
                    .timestamp = 0x40506070,
                    .samples_counter = 0x12345678,
                    .tag_mac = {0x21, 0x22, 0x23, 0x24, 0x25, 0x26},
                    .rssi = -49,
                    .data_len = 27,
                    .data_buf = {
                        0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F, 0x10,
                        0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19, 0x1A,
                    },
                },
            },
        };
        this->m_http_post_stat_res = true;

        ASSERT_TRUE(adv_post_statistics_do_send());

        ASSERT_EQ(this->m_cfg_http_stat.use_http_stat, g_pTestClass->m_http_post_stat_arg_cfg_http_stat.use_http_stat);
        ASSERT_EQ(
            this->m_cfg_http_stat.http_stat_use_ssl_client_cert,
            g_pTestClass->m_http_post_stat_arg_cfg_http_stat.http_stat_use_ssl_client_cert);
        ASSERT_EQ(
            this->m_cfg_http_stat.http_stat_use_ssl_server_cert,
            g_pTestClass->m_http_post_stat_arg_cfg_http_stat.http_stat_use_ssl_server_cert);
        ASSERT_EQ(
            string(this->m_cfg_http_stat.http_stat_url.buf),
            string(g_pTestClass->m_http_post_stat_arg_cfg_http_stat.http_stat_url.buf));
        ASSERT_EQ(
            string(this->m_cfg_http_stat.http_stat_user.buf),
            string(g_pTestClass->m_http_post_stat_arg_cfg_http_stat.http_stat_user.buf));
        ASSERT_EQ(
            string(this->m_cfg_http_stat.http_stat_pass.buf),
            string(g_pTestClass->m_http_post_stat_arg_cfg_http_stat.http_stat_pass.buf));

        ASSERT_EQ(
            string("11:22:33:44:55:66"),
            string(g_pTestClass->m_http_post_stat_arg_stat_info.nrf52_mac_addr.str_buf));
        ASSERT_EQ(string("v1.14.0"), string(g_pTestClass->m_http_post_stat_arg_stat_info.esp_fw.buf));
        ASSERT_EQ(string("v1.1.0"), string(g_pTestClass->m_http_post_stat_arg_stat_info.nrf_fw.buf));
        ASSERT_EQ(30123, g_pTestClass->m_http_post_stat_arg_stat_info.uptime);
        ASSERT_EQ(0x33333333 + 1, g_pTestClass->m_http_post_stat_arg_stat_info.nonce);
        ASSERT_TRUE(g_pTestClass->m_http_post_stat_arg_stat_info.nrf_status);
        ASSERT_FALSE(g_pTestClass->m_http_post_stat_arg_stat_info.is_connected_to_wifi);
        ASSERT_EQ(258, g_pTestClass->m_http_post_stat_arg_stat_info.network_disconnect_cnt);
        ASSERT_EQ(string("POWER_ON"), string(g_pTestClass->m_http_post_stat_arg_stat_info.reset_reason.buf));
        ASSERT_EQ(86, g_pTestClass->m_http_post_stat_arg_stat_info.reset_cnt);
        ASSERT_EQ(string("reset_info"), string(g_pTestClass->m_http_post_stat_arg_stat_info_reset_info_str));

        ASSERT_EQ(1, g_pTestClass->m_http_post_stat_arg_reports.num_of_advs);
        ASSERT_EQ(0x40506070, g_pTestClass->m_http_post_stat_arg_reports.table[0].timestamp);
        ASSERT_EQ(0x12345678, g_pTestClass->m_http_post_stat_arg_reports.table[0].samples_counter);

        std::array<uint8_t, MAC_ADDRESS_NUM_BYTES> mac_std_array;
        std::copy(
            std::begin(g_pTestClass->m_http_post_stat_arg_reports.table[0].tag_mac.mac),
            std::end(g_pTestClass->m_http_post_stat_arg_reports.table[0].tag_mac.mac),
            mac_std_array.begin());
        std::array<uint8_t, MAC_ADDRESS_NUM_BYTES> expected = { 0x21, 0x22, 0x23, 0x24, 0x25, 0x26 };
        ASSERT_EQ(expected, mac_std_array);
        ASSERT_EQ(-49, g_pTestClass->m_http_post_stat_arg_reports.table[0].rssi);
        ASSERT_EQ(27, g_pTestClass->m_http_post_stat_arg_reports.table[0].data_len);
        ASSERT_EQ(
            0,
            memcmp(
                &this->m_adv_report_table.table[0].data_buf[0],
                &g_pTestClass->m_http_post_stat_arg_reports.table[0].data_buf[0],
                27));

        ASSERT_EQ(nullptr, g_pTestClass->m_http_post_stat_arg_user_data);
        ASSERT_EQ(
            this->m_cfg_http_stat.http_stat_use_ssl_client_cert,
            g_pTestClass->m_http_post_stat_arg_use_ssl_client_cert);
        ASSERT_EQ(
            this->m_cfg_http_stat.http_stat_use_ssl_server_cert,
            g_pTestClass->m_http_post_stat_arg_use_ssl_server_cert);

        ASSERT_TRUE(this->m_mem_alloc_trace.is_empty());
    }
    {
        this->m_cfg_http_stat = ruuvi_gw_cfg_http_stat_t {
            .use_http_stat                 = true,
            .http_stat_use_ssl_client_cert = false,
            .http_stat_use_ssl_server_cert = true,
            .http_stat_url                 = { "https://stat3.ruuvi.com" },
            .http_stat_user                = { "user124" },
            .http_stat_pass                = { "pass124" },
        };
        this->m_adv_report_table = adv_report_table_t {
            .num_of_advs = 2,
            .table = {
                [0] = {
                    .timestamp = 0x40506071,
                    .samples_counter = 0x12345678,
                    .tag_mac = {0x21, 0x22, 0x23, 0x24, 0x25, 0x27},
                    .rssi = -50,
                    .data_len = 27,
                    .data_buf = {
                        0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19, 0x1A, 0x1B, 0x1C, 0x1D, 0x1E, 0x1F, 0x20,
                        0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27, 0x28, 0x29, 0x2A,
                    },
                },
                [1] = {
                    .timestamp = 0x40506072,
                    .samples_counter = 0x12345678,
                    .tag_mac = {0x21, 0x22, 0x23, 0x24, 0x25, 0x28},
                    .rssi = -51,
                    .data_len = 24,
                    .data_buf = {
                        0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0x3A, 0x3B, 0x3C, 0x3D, 0x3E, 0x3F, 0x40,
                        0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x47,
                    },
                },
            },
        };
        this->m_http_post_stat_res = true;

        ASSERT_TRUE(adv_post_statistics_do_send());

        ASSERT_EQ(this->m_cfg_http_stat.use_http_stat, g_pTestClass->m_http_post_stat_arg_cfg_http_stat.use_http_stat);
        ASSERT_EQ(
            this->m_cfg_http_stat.http_stat_use_ssl_client_cert,
            g_pTestClass->m_http_post_stat_arg_cfg_http_stat.http_stat_use_ssl_client_cert);
        ASSERT_EQ(
            this->m_cfg_http_stat.http_stat_use_ssl_server_cert,
            g_pTestClass->m_http_post_stat_arg_cfg_http_stat.http_stat_use_ssl_server_cert);
        ASSERT_EQ(
            string(this->m_cfg_http_stat.http_stat_url.buf),
            string(g_pTestClass->m_http_post_stat_arg_cfg_http_stat.http_stat_url.buf));
        ASSERT_EQ(
            string(this->m_cfg_http_stat.http_stat_user.buf),
            string(g_pTestClass->m_http_post_stat_arg_cfg_http_stat.http_stat_user.buf));
        ASSERT_EQ(
            string(this->m_cfg_http_stat.http_stat_pass.buf),
            string(g_pTestClass->m_http_post_stat_arg_cfg_http_stat.http_stat_pass.buf));

        ASSERT_EQ(
            string("11:22:33:44:55:66"),
            string(g_pTestClass->m_http_post_stat_arg_stat_info.nrf52_mac_addr.str_buf));
        ASSERT_EQ(string("v1.14.0"), string(g_pTestClass->m_http_post_stat_arg_stat_info.esp_fw.buf));
        ASSERT_EQ(string("v1.1.0"), string(g_pTestClass->m_http_post_stat_arg_stat_info.nrf_fw.buf));
        ASSERT_EQ(30123, g_pTestClass->m_http_post_stat_arg_stat_info.uptime);
        ASSERT_EQ(0x33333333 + 2, g_pTestClass->m_http_post_stat_arg_stat_info.nonce);
        ASSERT_TRUE(g_pTestClass->m_http_post_stat_arg_stat_info.nrf_status);
        ASSERT_FALSE(g_pTestClass->m_http_post_stat_arg_stat_info.is_connected_to_wifi);
        ASSERT_EQ(258, g_pTestClass->m_http_post_stat_arg_stat_info.network_disconnect_cnt);
        ASSERT_EQ(string("POWER_ON"), string(g_pTestClass->m_http_post_stat_arg_stat_info.reset_reason.buf));
        ASSERT_EQ(86, g_pTestClass->m_http_post_stat_arg_stat_info.reset_cnt);
        ASSERT_EQ(string("reset_info"), string(g_pTestClass->m_http_post_stat_arg_stat_info_reset_info_str));

        ASSERT_EQ(2, g_pTestClass->m_http_post_stat_arg_reports.num_of_advs);

        ASSERT_EQ(0x40506071, g_pTestClass->m_http_post_stat_arg_reports.table[0].timestamp);
        ASSERT_EQ(0x12345678, g_pTestClass->m_http_post_stat_arg_reports.table[0].samples_counter);
        std::array<uint8_t, MAC_ADDRESS_NUM_BYTES> mac_std_array;
        std::copy(
            std::begin(g_pTestClass->m_http_post_stat_arg_reports.table[0].tag_mac.mac),
            std::end(g_pTestClass->m_http_post_stat_arg_reports.table[0].tag_mac.mac),
            mac_std_array.begin());
        std::array<uint8_t, MAC_ADDRESS_NUM_BYTES> expected = { 0x21, 0x22, 0x23, 0x24, 0x25, 0x27 };
        ASSERT_EQ(expected, mac_std_array);
        ASSERT_EQ(-50, g_pTestClass->m_http_post_stat_arg_reports.table[0].rssi);
        ASSERT_EQ(27, g_pTestClass->m_http_post_stat_arg_reports.table[0].data_len);
        ASSERT_EQ(
            0,
            memcmp(
                &this->m_adv_report_table.table[0].data_buf[0],
                &g_pTestClass->m_http_post_stat_arg_reports.table[0].data_buf[0],
                27));

        ASSERT_EQ(0x40506072, g_pTestClass->m_http_post_stat_arg_reports.table[1].timestamp);
        ASSERT_EQ(0x12345678, g_pTestClass->m_http_post_stat_arg_reports.table[1].samples_counter);
        std::array<uint8_t, MAC_ADDRESS_NUM_BYTES> mac_std_array2;
        std::copy(
            std::begin(g_pTestClass->m_http_post_stat_arg_reports.table[1].tag_mac.mac),
            std::end(g_pTestClass->m_http_post_stat_arg_reports.table[1].tag_mac.mac),
            mac_std_array2.begin());
        std::array<uint8_t, MAC_ADDRESS_NUM_BYTES> expected2 = { 0x21, 0x22, 0x23, 0x24, 0x25, 0x28 };
        ASSERT_EQ(expected2, mac_std_array2);
        ASSERT_EQ(-51, g_pTestClass->m_http_post_stat_arg_reports.table[1].rssi);
        ASSERT_EQ(24, g_pTestClass->m_http_post_stat_arg_reports.table[1].data_len);
        ASSERT_EQ(
            0,
            memcmp(
                &this->m_adv_report_table.table[1].data_buf[0],
                &g_pTestClass->m_http_post_stat_arg_reports.table[1].data_buf[0],
                24));

        ASSERT_EQ(nullptr, g_pTestClass->m_http_post_stat_arg_user_data);
        ASSERT_EQ(
            this->m_cfg_http_stat.http_stat_use_ssl_client_cert,
            g_pTestClass->m_http_post_stat_arg_use_ssl_client_cert);
        ASSERT_EQ(
            this->m_cfg_http_stat.http_stat_use_ssl_server_cert,
            g_pTestClass->m_http_post_stat_arg_use_ssl_server_cert);

        ASSERT_TRUE(this->m_mem_alloc_trace.is_empty());
    }
}

TEST_F(TestAdvPostStatistics, adv_post_statistics_do_send_malloc_failed_1) // NOLINT
{
    this->m_esp_random_cnt = 0x33333333;
    adv_post_statistics_init();

    g_uptime_counter             = 30123;
    this->m_nrf_status           = true;
    this->m_is_connected_to_wifi = false;
    g_network_disconnect_cnt     = 258;
    this->m_reset_info_cnt       = 86;
    this->m_esp_reset_reason     = ESP_RST_POWERON;

    {
        this->m_cfg_http_stat = ruuvi_gw_cfg_http_stat_t {
            .use_http_stat                 = true,
            .http_stat_use_ssl_client_cert = false,
            .http_stat_use_ssl_server_cert = false,
            .http_stat_url                 = { "https://stat.ruuvi.com" },
            .http_stat_user                = { "" },
            .http_stat_pass                = { "" },
        };
        this->m_adv_report_table = adv_report_table_t {
            .num_of_advs = 0,
            .table       = {},
        };
        this->m_http_post_stat_res = true;

        this->m_malloc_fail_on_cnt = 1;

        ASSERT_FALSE(adv_post_statistics_do_send());

        ASSERT_TRUE(this->m_mem_alloc_trace.is_empty());
    }
}

TEST_F(TestAdvPostStatistics, adv_post_statistics_do_send_malloc_failed_2) // NOLINT
{
    this->m_esp_random_cnt = 0x33333333;
    adv_post_statistics_init();

    g_uptime_counter             = 30123;
    this->m_nrf_status           = true;
    this->m_is_connected_to_wifi = false;
    g_network_disconnect_cnt     = 258;
    this->m_reset_info_cnt       = 86;
    this->m_esp_reset_reason     = ESP_RST_POWERON;

    {
        this->m_cfg_http_stat = ruuvi_gw_cfg_http_stat_t {
            .use_http_stat                 = true,
            .http_stat_use_ssl_client_cert = false,
            .http_stat_use_ssl_server_cert = false,
            .http_stat_url                 = { "https://stat.ruuvi.com" },
            .http_stat_user                = { "" },
            .http_stat_pass                = { "" },
        };
        this->m_adv_report_table = adv_report_table_t {
            .num_of_advs = 0,
            .table       = {},
        };
        this->m_http_post_stat_res = true;

        this->m_malloc_fail_on_cnt = 2;

        ASSERT_FALSE(adv_post_statistics_do_send());

        ASSERT_TRUE(this->m_mem_alloc_trace.is_empty());
    }
}

TEST_F(TestAdvPostStatistics, adv_post_statistics_do_send_malloc_failed_3) // NOLINT
{
    this->m_esp_random_cnt = 0x33333333;
    adv_post_statistics_init();

    g_uptime_counter             = 30123;
    this->m_nrf_status           = true;
    this->m_is_connected_to_wifi = false;
    g_network_disconnect_cnt     = 258;
    this->m_reset_info_cnt       = 86;
    this->m_esp_reset_reason     = ESP_RST_POWERON;

    {
        this->m_cfg_http_stat = ruuvi_gw_cfg_http_stat_t {
            .use_http_stat                 = true,
            .http_stat_use_ssl_client_cert = false,
            .http_stat_use_ssl_server_cert = false,
            .http_stat_url                 = { "https://stat.ruuvi.com" },
            .http_stat_user                = { "" },
            .http_stat_pass                = { "" },
        };
        this->m_adv_report_table = adv_report_table_t {
            .num_of_advs = 0,
            .table       = {},
        };
        this->m_http_post_stat_res = true;

        this->m_malloc_fail_on_cnt = 3;

        ASSERT_FALSE(adv_post_statistics_do_send());

        ASSERT_TRUE(this->m_mem_alloc_trace.is_empty());
    }
}

TEST_F(TestAdvPostStatistics, adv_post_statistics_do_send_malloc_failed_4) // NOLINT
{
    this->m_esp_random_cnt = 0x33333333;
    adv_post_statistics_init();

    g_uptime_counter             = 30123;
    this->m_nrf_status           = true;
    this->m_is_connected_to_wifi = false;
    g_network_disconnect_cnt     = 258;
    this->m_reset_info_cnt       = 86;
    this->m_esp_reset_reason     = ESP_RST_POWERON;

    {
        this->m_cfg_http_stat = ruuvi_gw_cfg_http_stat_t {
            .use_http_stat                 = true,
            .http_stat_use_ssl_client_cert = false,
            .http_stat_use_ssl_server_cert = false,
            .http_stat_url                 = { "https://stat.ruuvi.com" },
            .http_stat_user                = { "" },
            .http_stat_pass                = { "" },
        };
        this->m_adv_report_table = adv_report_table_t {
            .num_of_advs = 0,
            .table       = {},
        };
        this->m_http_post_stat_res = true;

        this->m_malloc_fail_on_cnt = 4;

        ASSERT_FALSE(adv_post_statistics_do_send());

        ASSERT_TRUE(this->m_mem_alloc_trace.is_empty());
    }
}

TEST_F(TestAdvPostStatistics, adv_post_statistics_do_send_malloc_failed_5_success) // NOLINT
{
    this->m_esp_random_cnt = 0x33333333;
    adv_post_statistics_init();

    g_uptime_counter             = 30123;
    this->m_nrf_status           = true;
    this->m_is_connected_to_wifi = false;
    g_network_disconnect_cnt     = 258;
    this->m_reset_info_cnt       = 86;
    this->m_esp_reset_reason     = ESP_RST_POWERON;

    {
        this->m_cfg_http_stat = ruuvi_gw_cfg_http_stat_t {
            .use_http_stat                 = true,
            .http_stat_use_ssl_client_cert = false,
            .http_stat_use_ssl_server_cert = false,
            .http_stat_url                 = { "https://stat.ruuvi.com" },
            .http_stat_user                = { "" },
            .http_stat_pass                = { "" },
        };
        this->m_adv_report_table = adv_report_table_t {
            .num_of_advs = 0,
            .table       = {},
        };
        this->m_http_post_stat_res = true;

        this->m_malloc_fail_on_cnt = 5;

        ASSERT_TRUE(adv_post_statistics_do_send());

        ASSERT_TRUE(this->m_mem_alloc_trace.is_empty());
    }
}

TEST_F(TestAdvPostStatistics, adv_post_statistics_do_send_http_post_stat_failed) // NOLINT
{
    this->m_esp_random_cnt = 0x33333333;
    adv_post_statistics_init();

    g_uptime_counter             = 30123;
    this->m_nrf_status           = true;
    this->m_is_connected_to_wifi = false;
    g_network_disconnect_cnt     = 258;
    this->m_reset_info_cnt       = 86;
    this->m_esp_reset_reason     = ESP_RST_POWERON;

    {
        this->m_cfg_http_stat = ruuvi_gw_cfg_http_stat_t {
            .use_http_stat                 = true,
            .http_stat_use_ssl_client_cert = false,
            .http_stat_use_ssl_server_cert = false,
            .http_stat_url                 = { "https://stat.ruuvi.com" },
            .http_stat_user                = { "" },
            .http_stat_pass                = { "" },
        };
        this->m_adv_report_table = adv_report_table_t {
            .num_of_advs = 0,
            .table       = {},
        };

        this->m_http_post_stat_res = false;

        ASSERT_FALSE(adv_post_statistics_do_send());

        ASSERT_TRUE(this->m_mem_alloc_trace.is_empty());
    }
}
