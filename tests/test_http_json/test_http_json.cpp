/**
 * @file test_http_json.cpp
 * @author TheSomeMan
 * @date 2020-02-03
 * @copyright Ruuvi Innovations Ltd, license BSD-3-Clause.
 */

#include "http_json.h"
#include <cstring>
#include "gtest/gtest.h"
#include "os_malloc.h"

using namespace std;

/*** Google-test class implementation
 * *********************************************************************************/

class TestHttpJson;
static TestHttpJson *g_pTestClass;

class MemAllocTrace
{
    vector<uint32_t *> allocated_mem;

    std::vector<uint32_t *>::iterator
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
    add(uint32_t *ptr)
    {
        auto iter = find(ptr);
        assert(iter == this->allocated_mem.end()); // ptr was found in the list of allocated memory blocks
        this->allocated_mem.push_back(ptr);
    }
    void
    remove(uint32_t *ptr)
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
};

class TestHttpJson : public ::testing::Test
{
private:
protected:
    void
    SetUp() override
    {
        g_pTestClass = this;
        cjson_wrap_init();
        this->m_json_str = cjson_wrap_str_null();

        this->m_malloc_cnt         = 0;
        this->m_malloc_fail_on_cnt = 0;
    }

    void
    TearDown() override
    {
        g_pTestClass = nullptr;
        cjson_wrap_free_json_str(&this->m_json_str);
    }

public:
    TestHttpJson();

    ~TestHttpJson() override;

    MemAllocTrace    m_mem_alloc_trace;
    uint32_t         m_malloc_cnt;
    uint32_t         m_malloc_fail_on_cnt;
    cjson_wrap_str_t m_json_str;
};

TestHttpJson::TestHttpJson()
    : m_malloc_cnt(0)
    , m_malloc_fail_on_cnt(0)
    , m_json_str()
    , Test()
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
    auto p_mem = static_cast<uint32_t *>(malloc(size + sizeof(uint64_t)));
    assert(nullptr != p_mem);
    *p_mem = g_pTestClass->m_malloc_cnt;
    g_pTestClass->m_mem_alloc_trace.add(p_mem);
    p_mem += 1;
    return static_cast<void *>(p_mem);
}

void
os_free_internal(void *p_mem)
{
    auto p_mem2 = static_cast<uint32_t *>(p_mem) - 1;
    g_pTestClass->m_mem_alloc_trace.remove(p_mem2);
    free(p_mem2);
}

void *
os_calloc(const size_t nmemb, const size_t size)
{
    if (++g_pTestClass->m_malloc_cnt == g_pTestClass->m_malloc_fail_on_cnt)
    {
        return nullptr;
    }
    auto p_mem = static_cast<uint32_t *>(calloc(nmemb, size));
    assert(nullptr != p_mem);
    *p_mem = g_pTestClass->m_malloc_cnt;
    g_pTestClass->m_mem_alloc_trace.add(p_mem);
    p_mem += 1;
    return static_cast<void *>(p_mem);
}

} // extern "C"

TestHttpJson::~TestHttpJson() = default;

/*** Unit-Tests
 * *******************************************************************************************************/

TEST_F(TestHttpJson, test_1) // NOLINT
{
    const time_t                 timestamp     = 1612358920;
    const mac_address_str_t      gw_mac_addr   = { .str_buf = "AA:CC:EE:00:11:22" };
    const char *                 p_coordinates = "170.112233,59.445566";
    const std::array<uint8_t, 1> data          = { 0xAAU };

    adv_report_table_t adv_table = { .num_of_advs = 1,
                                     .table       = { {
                                         .timestamp = 1612358929,
                                         .tag_mac   = { 0xaa, 0xbb, 0xcc, 0x01, 0x02, 0x03 },
                                         .rssi      = -70,
                                         .data_len  = data.size(),
                                     } } };
    memcpy(adv_table.table[0].data_buf, data.data(), data.size());
    ASSERT_TRUE(http_json_create_records_str(
        &adv_table,
        true,
        timestamp,
        &gw_mac_addr,
        p_coordinates,
        true,
        12345678,
        &this->m_json_str));
    ASSERT_EQ(
        string("{\n"
               "\t\"data\":\t{\n"
               "\t\t\"coordinates\":\t\"170.112233,59.445566\",\n"
               "\t\t\"timestamp\":\t\"1612358920\",\n"
               "\t\t\"nonce\":\t\"12345678\",\n"
               "\t\t\"gw_mac\":\t\"AA:CC:EE:00:11:22\",\n"
               "\t\t\"tags\":\t{\n"
               "\t\t\t\"AA:BB:CC:01:02:03\":\t{\n"
               "\t\t\t\t\"rssi\":\t-70,\n"
               "\t\t\t\t\"timestamp\":\t\"1612358929\",\n"
               "\t\t\t\t\"data\":\t\"AA\"\n"
               "\t\t\t}\n"
               "\t\t}\n"
               "\t}\n"
               "}"),
        string(this->m_json_str.p_str));
    cjson_wrap_free_json_str(&this->m_json_str);
    ASSERT_EQ(31, this->m_malloc_cnt);
    ASSERT_TRUE(this->m_mem_alloc_trace.is_empty());
}

TEST_F(TestHttpJson, test_1_without_timestamp) // NOLINT
{
    const time_t                 timestamp     = 1612358920;
    const mac_address_str_t      gw_mac_addr   = { .str_buf = "AA:CC:EE:00:11:22" };
    const char *                 p_coordinates = "170.112233,59.445566";
    const std::array<uint8_t, 1> data          = { 0xAAU };

    adv_report_table_t adv_table = { .num_of_advs = 1,
                                     .table       = { {
                                         .timestamp = 1011,
                                         .tag_mac   = { 0xaa, 0xbb, 0xcc, 0x01, 0x02, 0x03 },
                                         .rssi      = -70,
                                         .data_len  = data.size(),
                                     } } };
    memcpy(adv_table.table[0].data_buf, data.data(), data.size());
    ASSERT_TRUE(http_json_create_records_str(
        &adv_table,
        false,
        timestamp,
        &gw_mac_addr,
        p_coordinates,
        true,
        12345678,
        &this->m_json_str));
    ASSERT_EQ(
        string("{\n"
               "\t\"data\":\t{\n"
               "\t\t\"coordinates\":\t\"170.112233,59.445566\",\n"
               "\t\t\"nonce\":\t\"12345678\",\n"
               "\t\t\"gw_mac\":\t\"AA:CC:EE:00:11:22\",\n"
               "\t\t\"tags\":\t{\n"
               "\t\t\t\"AA:BB:CC:01:02:03\":\t{\n"
               "\t\t\t\t\"rssi\":\t-70,\n"
               "\t\t\t\t\"counter\":\t\"1011\",\n"
               "\t\t\t\t\"data\":\t\"AA\"\n"
               "\t\t\t}\n"
               "\t\t}\n"
               "\t}\n"
               "}"),
        string(this->m_json_str.p_str));
    cjson_wrap_free_json_str(&this->m_json_str);
    ASSERT_EQ(27, this->m_malloc_cnt);
    ASSERT_TRUE(this->m_mem_alloc_trace.is_empty());
}

TEST_F(TestHttpJson, test_2) // NOLINT
{
    const time_t                 timestamp     = 1612358920;
    const mac_address_str_t      gw_mac_addr   = { .str_buf = "AA:CC:EE:00:11:22" };
    const char *                 p_coordinates = "170.112233,59.445566";
    const std::array<uint8_t, 1> data1         = { 0xAAU };
    const std::array<uint8_t, 1> data2         = { 0xBBU };

    adv_report_table_t adv_table = { .num_of_advs = 2,
                                     .table       = {
                                         {
                                             .timestamp = 1612358929,
                                             .tag_mac   = { 0xaa, 0xbb, 0xcc, 0x01, 0x02, 0x03 },
                                             .rssi      = -70,
                                             .data_len  = data1.size(),
                                         },
                                         {
                                             .timestamp = 1612358930,
                                             .tag_mac   = { 0xaa, 0xbb, 0xcc, 0x01, 0x02, 0x04 },
                                             .rssi      = -71,
                                             .data_len  = data2.size(),
                                         },
                                     } };
    memcpy(adv_table.table[0].data_buf, data1.data(), data1.size());
    memcpy(adv_table.table[1].data_buf, data2.data(), data2.size());
    ASSERT_TRUE(http_json_create_records_str(
        &adv_table,
        true,
        timestamp,
        &gw_mac_addr,
        p_coordinates,
        true,
        12345678,
        &this->m_json_str));
    ASSERT_EQ(
        string("{\n"
               "\t\"data\":\t{\n"
               "\t\t\"coordinates\":\t\"170.112233,59.445566\",\n"
               "\t\t\"timestamp\":\t\"1612358920\",\n"
               "\t\t\"nonce\":\t\"12345678\",\n"
               "\t\t\"gw_mac\":\t\"AA:CC:EE:00:11:22\",\n"
               "\t\t\"tags\":\t{\n"
               "\t\t\t\"AA:BB:CC:01:02:03\":\t{\n"
               "\t\t\t\t\"rssi\":\t-70,\n"
               "\t\t\t\t\"timestamp\":\t\"1612358929\",\n"
               "\t\t\t\t\"data\":\t\"AA\"\n"
               "\t\t\t},\n"
               "\t\t\t\"AA:BB:CC:01:02:04\":\t{\n"
               "\t\t\t\t\"rssi\":\t-71,\n"
               "\t\t\t\t\"timestamp\":\t\"1612358930\",\n"
               "\t\t\t\t\"data\":\t\"BB\"\n"
               "\t\t\t}\n"
               "\t\t}\n"
               "\t}\n"
               "}"),
        string(this->m_json_str.p_str));
    cjson_wrap_free_json_str(&this->m_json_str);
    ASSERT_TRUE(this->m_mem_alloc_trace.is_empty());
}

TEST_F(TestHttpJson, test_http_json_create_records_str_malloc_failed) // NOLINT
{
    const time_t                 timestamp     = 1612358920;
    const mac_address_str_t      gw_mac_addr   = { .str_buf = "AA:CC:EE:00:11:22" };
    const char *                 p_coordinates = "170.112233,59.445566";
    const std::array<uint8_t, 1> data          = { 0xAAU };

    adv_report_table_t adv_table = { .num_of_advs = 1,
                                     .table       = { {
                                         .timestamp = 1612358929,
                                         .tag_mac   = { 0xaa, 0xbb, 0xcc, 0x01, 0x02, 0x03 },
                                         .rssi      = -70,
                                         .data_len  = data.size(),
                                     } } };
    memcpy(adv_table.table[0].data_buf, data.data(), data.size());

    const uint32_t num_allocations = 31;
    for (uint32_t i = 1; i <= num_allocations; ++i)
    {
        this->m_malloc_fail_on_cnt = i;
        this->m_malloc_cnt         = 0;
        if (http_json_create_records_str(
                &adv_table,
                true,
                timestamp,
                &gw_mac_addr,
                p_coordinates,
                true,
                12345678,
                &this->m_json_str))
        {
            ASSERT_FALSE(true);
        }
        ASSERT_EQ(nullptr, this->m_json_str.p_str);
        ASSERT_TRUE(this->m_mem_alloc_trace.is_empty());
    }

    {
        this->m_malloc_fail_on_cnt = num_allocations + 1;
        this->m_malloc_cnt         = 0;
        ASSERT_TRUE(http_json_create_records_str(
            &adv_table,
            true,
            timestamp,
            &gw_mac_addr,
            p_coordinates,
            true,
            12345678,
            &this->m_json_str));
        ASSERT_NE(nullptr, this->m_json_str.p_str);
        cjson_wrap_free_json_str(&this->m_json_str);
        ASSERT_TRUE(this->m_mem_alloc_trace.is_empty());
    }
}

TEST_F(TestHttpJson, test_create_status_json_str_connection_wifi) // NOLINT
{
    const mac_address_str_t      nrf52_mac_addr         = { .str_buf = "AA:CC:EE:00:11:22" };
    const uint32_t               uptime                 = 123;
    const bool                   is_wifi                = true;
    const uint32_t               network_disconnect_cnt = 3;
    const uint32_t               nonce                  = 1234567;
    const std::array<uint8_t, 1> data                   = { 0xAAU };

    adv_report_table_t adv_table = { .num_of_advs = 4,
                                     .table       = {
                                         {
                                             .timestamp       = 1612358929,
                                             .samples_counter = 11,
                                             .tag_mac         = { 0xaa, 0xbb, 0xcc, 0x01, 0x02, 0x03 },
                                             .rssi            = -70,
                                             .data_len        = data.size(),
                                         },
                                         {
                                             .timestamp       = 1612358928,
                                             .samples_counter = 10,
                                             .tag_mac         = { 0xaa, 0xbb, 0xcc, 0x01, 0x02, 0x04 },
                                             .rssi            = -70,
                                             .data_len        = data.size(),
                                         },
                                         {
                                             .timestamp       = 1612358925,
                                             .samples_counter = 0,
                                             .tag_mac         = { 0xaa, 0xbb, 0xcc, 0x01, 0x02, 0x05 },
                                             .rssi            = -70,
                                             .data_len        = data.size(),
                                         },
                                         {
                                             .timestamp       = 1612358924,
                                             .samples_counter = 0,
                                             .tag_mac         = { 0xaa, 0xbb, 0xcc, 0x01, 0x02, 0x06 },
                                             .rssi            = -70,
                                             .data_len        = data.size(),
                                         },
                                     } };
    memcpy(adv_table.table[0].data_buf, data.data(), data.size());
    const http_json_statistics_info_t stat_info = {
        .nrf52_mac_addr         = nrf52_mac_addr,
        .esp_fw                 = { "1.9.0" },
        .nrf_fw                 = { "0.7.1" },
        .uptime                 = uptime,
        .nonce                  = nonce,
        .is_connected_to_wifi   = is_wifi,
        .network_disconnect_cnt = network_disconnect_cnt,
    };
    ASSERT_TRUE(http_json_create_status_str(&stat_info, &adv_table, &this->m_json_str));
    ASSERT_EQ(
        string("{\n"
               "\t\"DEVICE_ADDR\":\t\"AA:CC:EE:00:11:22\",\n"
               "\t\"ESP_FW\":\t\"1.9.0\",\n"
               "\t\"NRF_FW\":\t\"0.7.1\",\n"
               "\t\"UPTIME\":\t\"123\",\n"
               "\t\"NONCE\":\t\"1234567\",\n"
               "\t\"CONNECTION\":\t\"WIFI\",\n"
               "\t\"NUM_CONN_LOST\":\t\"3\",\n"
               "\t\"SENSORS_SEEN\":\t\"2\",\n"
               "\t\"ACTIVE_SENSORS\":\t[{\n"
               "\t\t\t\"MAC\":\t\"AA:BB:CC:01:02:03\",\n"
               "\t\t\t\"COUNTER\":\t\"11\"\n"
               "\t\t}, {\n"
               "\t\t\t\"MAC\":\t\"AA:BB:CC:01:02:04\",\n"
               "\t\t\t\"COUNTER\":\t\"10\"\n"
               "\t\t}],\n"
               "\t\"INACTIVE_SENSORS\":\t[\"AA:BB:CC:01:02:05\", \"AA:BB:CC:01:02:06\"]\n"
               "}"),
        string(this->m_json_str.p_str));
    cjson_wrap_free_json_str(&this->m_json_str);
    ASSERT_EQ(50, this->m_malloc_cnt);
    ASSERT_TRUE(this->m_mem_alloc_trace.is_empty());
}

TEST_F(TestHttpJson, test_create_status_json_str_connection_ethernet) // NOLINT
{
    const mac_address_str_t      nrf52_mac_addr         = { .str_buf = "AB:CD:EF:01:12:23" };
    const uint32_t               uptime                 = 124;
    const bool                   is_wifi                = false;
    const uint32_t               network_disconnect_cnt = 4;
    const uint32_t               nonce                  = 1234568;
    const std::array<uint8_t, 1> data                   = { 0xABU };

    adv_report_table_t adv_table = { .num_of_advs = 3,
                                     .table       = {
                                         {
                                             .timestamp       = 1612358930,
                                             .samples_counter = 12,
                                             .tag_mac         = { 0xab, 0xbb, 0xcc, 0x01, 0x02, 0xF3 },
                                             .rssi            = -69,
                                             .data_len        = data.size(),
                                         },
                                         {
                                             .timestamp       = 1612358929,
                                             .samples_counter = 11,
                                             .tag_mac         = { 0xab, 0xbb, 0xcc, 0x01, 0x02, 0xF4 },
                                             .rssi            = -68,
                                             .data_len        = data.size(),
                                         },
                                         {
                                             .timestamp       = 1612358926,
                                             .samples_counter = 0,
                                             .tag_mac         = { 0xab, 0xbb, 0xcc, 0x01, 0x02, 0xF5 },
                                             .rssi            = -67,
                                             .data_len        = data.size(),
                                         },
                                     } };
    memcpy(adv_table.table[0].data_buf, data.data(), data.size());
    const http_json_statistics_info_t stat_info = {
        .nrf52_mac_addr         = nrf52_mac_addr,
        .esp_fw                 = { "1.9.0" },
        .nrf_fw                 = { "0.7.1" },
        .uptime                 = uptime,
        .nonce                  = nonce,
        .is_connected_to_wifi   = is_wifi,
        .network_disconnect_cnt = network_disconnect_cnt,
    };
    ASSERT_TRUE(http_json_create_status_str(&stat_info, &adv_table, &this->m_json_str));
    ASSERT_EQ(
        string("{\n"
               "\t\"DEVICE_ADDR\":\t\"AB:CD:EF:01:12:23\",\n"
               "\t\"ESP_FW\":\t\"1.9.0\",\n"
               "\t\"NRF_FW\":\t\"0.7.1\",\n"
               "\t\"UPTIME\":\t\"124\",\n"
               "\t\"NONCE\":\t\"1234568\",\n"
               "\t\"CONNECTION\":\t\"ETHERNET\",\n"
               "\t\"NUM_CONN_LOST\":\t\"4\",\n"
               "\t\"SENSORS_SEEN\":\t\"2\",\n"
               "\t\"ACTIVE_SENSORS\":\t[{\n"
               "\t\t\t\"MAC\":\t\"AB:BB:CC:01:02:F3\",\n"
               "\t\t\t\"COUNTER\":\t\"12\"\n"
               "\t\t}, {\n"
               "\t\t\t\"MAC\":\t\"AB:BB:CC:01:02:F4\",\n"
               "\t\t\t\"COUNTER\":\t\"11\"\n"
               "\t\t}],\n"
               "\t\"INACTIVE_SENSORS\":\t[\"AB:BB:CC:01:02:F5\"]\n"
               "}"),
        string(this->m_json_str.p_str));
    cjson_wrap_free_json_str(&this->m_json_str);
    ASSERT_EQ(48, this->m_malloc_cnt);
    ASSERT_TRUE(this->m_mem_alloc_trace.is_empty());
}

TEST_F(TestHttpJson, test_create_status_json_str_malloc_failed) // NOLINT
{
    const mac_address_str_t      nrf52_mac_addr         = { .str_buf = "AA:CC:EE:00:11:22" };
    const uint32_t               uptime                 = 123;
    const bool                   is_wifi                = true;
    const uint32_t               network_disconnect_cnt = 3;
    const uint32_t               nonce                  = 1234567;
    const std::array<uint8_t, 1> data                   = { 0xAAU };

    adv_report_table_t adv_table = { .num_of_advs = 4,
                                     .table       = {
                                         {
                                             .timestamp       = 1612358929,
                                             .samples_counter = 11,
                                             .tag_mac         = { 0xaa, 0xbb, 0xcc, 0x01, 0x02, 0x03 },
                                             .rssi            = -70,
                                             .data_len        = data.size(),
                                         },
                                         {
                                             .timestamp       = 1612358928,
                                             .samples_counter = 10,
                                             .tag_mac         = { 0xaa, 0xbb, 0xcc, 0x01, 0x02, 0x04 },
                                             .rssi            = -70,
                                             .data_len        = data.size(),
                                         },
                                         {
                                             .timestamp       = 1612358925,
                                             .samples_counter = 0,
                                             .tag_mac         = { 0xaa, 0xbb, 0xcc, 0x01, 0x02, 0x05 },
                                             .rssi            = -70,
                                             .data_len        = data.size(),
                                         },
                                         {
                                             .timestamp       = 1612358924,
                                             .samples_counter = 0,
                                             .tag_mac         = { 0xaa, 0xbb, 0xcc, 0x01, 0x02, 0x06 },
                                             .rssi            = -70,
                                             .data_len        = data.size(),
                                         },
                                     } };
    memcpy(adv_table.table[0].data_buf, data.data(), data.size());

    const http_json_statistics_info_t stat_info = {
        .nrf52_mac_addr         = nrf52_mac_addr,
        .esp_fw                 = { "1.9.0" },
        .nrf_fw                 = { "0.7.1" },
        .uptime                 = uptime,
        .nonce                  = nonce,
        .is_connected_to_wifi   = is_wifi,
        .network_disconnect_cnt = network_disconnect_cnt,
    };

    for (uint32_t i = 1; i < 50; ++i)
    {
        this->m_malloc_fail_on_cnt = i;
        this->m_malloc_cnt         = 0;
        if (http_json_create_status_str(&stat_info, &adv_table, &this->m_json_str))
        {
            ASSERT_FALSE(true);
        }
        ASSERT_EQ(nullptr, this->m_json_str.p_str);
        ASSERT_TRUE(this->m_mem_alloc_trace.is_empty());
    }

    {
        this->m_malloc_fail_on_cnt = 51;
        this->m_malloc_cnt         = 0;
        ASSERT_TRUE(http_json_create_status_str(&stat_info, &adv_table, &this->m_json_str));
        ASSERT_NE(nullptr, this->m_json_str.p_str);
        cjson_wrap_free_json_str(&this->m_json_str);
        ASSERT_EQ(50, this->m_malloc_cnt);
        ASSERT_TRUE(this->m_mem_alloc_trace.is_empty());
    }
}
