/**
 * @file test_http_json.cpp
 * @author TheSomeMan
 * @date 2020-02-03
 * @copyright Ruuvi Innovations Ltd, license BSD-3-Clause.
 */

#include "http_json.h"
#include <cstring>
#include <vector>
#include <array>
#include <string>
#include "gtest/gtest.h"
#include "os_malloc.h"

using namespace std;

/*** Google-test class implementation
 * *********************************************************************************/

class TestHttpJson;
static TestHttpJson* g_pTestClass;

class MemAllocTrace
{
    vector<uint32_t*> allocated_mem;

    std::vector<uint32_t*>::iterator
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
    add(uint32_t* ptr)
    {
        auto iter = find(ptr);
        assert(iter == this->allocated_mem.end()); // ptr was found in the list of allocated memory blocks
        this->allocated_mem.push_back(ptr);
    }
    void
    remove(uint32_t* ptr)
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

void*
os_malloc(const size_t size)
{
    if (++g_pTestClass->m_malloc_cnt == g_pTestClass->m_malloc_fail_on_cnt)
    {
        return nullptr;
    }
    auto p_mem = static_cast<uint32_t*>(malloc(size + sizeof(uint64_t)));
    assert(nullptr != p_mem);
    *p_mem = g_pTestClass->m_malloc_cnt;
    g_pTestClass->m_mem_alloc_trace.add(p_mem);
    p_mem += 1;
    return static_cast<void*>(p_mem);
}

void
os_free_internal(void* p_mem)
{
    auto p_mem2 = static_cast<uint32_t*>(p_mem) - 1;
    g_pTestClass->m_mem_alloc_trace.remove(p_mem2);
    free(p_mem2);
}

void*
os_calloc(const size_t nmemb, const size_t size)
{
    if (++g_pTestClass->m_malloc_cnt == g_pTestClass->m_malloc_fail_on_cnt)
    {
        return nullptr;
    }
    auto p_mem = static_cast<uint32_t*>(calloc(nmemb, size));
    assert(nullptr != p_mem);
    *p_mem = g_pTestClass->m_malloc_cnt;
    g_pTestClass->m_mem_alloc_trace.add(p_mem);
    p_mem += 1;
    return static_cast<void*>(p_mem);
}

bool
runtime_stat_for_each_accumulated_info(
    bool (*p_cb)(const char* const p_task_name, const uint32_t min_free_stack_size, void* p_userdata),
    void* p_userdata)
{
    if (!p_cb("main", 1000, p_userdata))
    {
        return false;
    }
    if (!p_cb("IDLE0", 500, p_userdata))
    {
        return false;
    }
    return true;
}

} // extern "C"

TestHttpJson::~TestHttpJson() = default;

/*** Unit-Tests
 * *******************************************************************************************************/

TEST_F(TestHttpJson, test_df_5_with_raw_data) // NOLINT
{
    const time_t                     timestamp   = 1612358920;
    const mac_address_str_t          gw_mac_addr = { "AA:CC:EE:00:11:22" };
    const ruuvi_gw_cfg_coordinates_t coordinates = { "170.112233,59.445566" };
    const std::array<uint8_t, 31>    data        = {
                  0x02U, 0x01U, 0x06U, 0x1BU, 0xFFU, 0x99U, 0x04U, 0x05U, 0x15U, 0x71U, 0x4DU, 0x9FU, 0xC5U, 0x6DU, 0xFFU, 0xF0U,
                  0xFFU, 0xF8U, 0x03U, 0xE4U, 0xB5U, 0x16U, 0xE8U, 0x4DU, 0x7EU, 0xF4U, 0x1FU, 0x0CU, 0x28U, 0xCBU, 0xD6U,
    };

    adv_report_table_t adv_table = {
        .num_of_advs = 1,
        .table = {
            {
                .timestamp = 1612358929,
                .tag_mac = {0xaa, 0xbb, 0xcc, 0x01, 0x02, 0x03},
                .rssi = -70,
                .primary_phy = RE_CA_UART_BLE_PHY_1MBPS,
                .secondary_phy = RE_CA_UART_BLE_PHY_NOT_SET,
                .ch_index = 37,
                .is_coded_phy = false,
                .tx_power = RE_CA_UART_BLE_GAP_POWER_LEVEL_INVALID,
                .data_len = data.size(),
            },
        },
    };
    memcpy(adv_table.table[0].data_buf, data.data(), data.size());

    const bool     flag_raw_data       = true;
    const bool     flag_decode         = false;
    const bool     flag_use_timestamps = true;
    const bool     flag_use_nonce      = true;
    const uint32_t nonce               = 12345678;

    const http_json_create_stream_gen_advs_params_t params = {
        .flag_raw_data       = flag_raw_data,
        .flag_decode         = flag_decode,
        .flag_use_timestamps = flag_use_timestamps,
        .cur_time            = timestamp,
        .flag_use_nonce      = flag_use_nonce,
        .nonce               = nonce,
        .p_mac_addr          = &gw_mac_addr,
        .p_coordinates       = &coordinates,
    };

    json_stream_gen_t* p_gen = http_json_create_stream_gen_advs(&adv_table, &params);
    ASSERT_NE(nullptr, p_gen);

    string json_str("");
    while (true)
    {
        const char* p_chunk = json_stream_gen_get_next_chunk(p_gen);
        if (nullptr == p_chunk)
        {
            ASSERT_FALSE(nullptr == p_chunk);
        }

        if ('\0' == p_chunk[0])
        {
            break;
        }
        json_str += string(p_chunk);
    }

    ASSERT_EQ(
        string("{\n"
               "  \"data\": {\n"
               "    \"coordinates\": \"170.112233,59.445566\",\n"
               "    \"timestamp\": 1612358920,\n"
               "    \"nonce\": 12345678,\n"
               "    \"gw_mac\": \"AA:CC:EE:00:11:22\",\n"
               "    \"tags\": {\n"
               "      \"AA:BB:CC:01:02:03\": {\n"
               "        \"rssi\": -70,\n"
               "        \"timestamp\": 1612358929,\n"
               "        \"ble_phy\": \"1M\",\n"
               "        \"ble_chan\": 37,\n"
               "        \"data\": \"0201061BFF99040515714D9FC56DFFF0FFF803E4B516E84D7EF41F0C28CBD6\"\n"
               "      }\n"
               "    }\n"
               "  }\n"
               "}"),
        json_str);
    json_stream_gen_delete(&p_gen);
    ASSERT_TRUE(this->m_mem_alloc_trace.is_empty());
}

TEST_F(TestHttpJson, test_df_5_with_raw_and_decoded_data) // NOLINT
{
    const time_t                     timestamp   = 1612358920;
    const mac_address_str_t          gw_mac_addr = { "AA:CC:EE:00:11:22" };
    const ruuvi_gw_cfg_coordinates_t coordinates = { "170.112233,59.445566" };
    const std::array<uint8_t, 31>    data        = {
                  0x02U, 0x01U, 0x06U, 0x1BU, 0xFFU, 0x99U, 0x04U, 0x05U, 0x15U, 0x71U, 0x4DU, 0x9FU, 0xC5U, 0x6DU, 0xFFU, 0xF0U,
                  0xFFU, 0xF8U, 0x03U, 0xE4U, 0xB5U, 0x16U, 0xE8U, 0x4DU, 0x7EU, 0xF4U, 0x1FU, 0x0CU, 0x28U, 0xCBU, 0xD6U,
    };

    adv_report_table_t adv_table = {
        .num_of_advs = 1,
        .table = {
            {
                .timestamp = 1612358929,
                .tag_mac = {0xaa, 0xbb, 0xcc, 0x01, 0x02, 0x03},
                .rssi = -70,
                .primary_phy = RE_CA_UART_BLE_PHY_1MBPS,
                .secondary_phy = RE_CA_UART_BLE_PHY_2MBPS,
                .ch_index = 25,
                .is_coded_phy = false,
                .tx_power = 7,
                .data_len = data.size(),
            },
        },
    };
    memcpy(adv_table.table[0].data_buf, data.data(), data.size());

    const bool     flag_raw_data       = true;
    const bool     flag_decode         = true;
    const bool     flag_use_timestamps = true;
    const bool     flag_use_nonce      = true;
    const uint32_t nonce               = 12345678;

    const http_json_create_stream_gen_advs_params_t params = {
        .flag_raw_data       = flag_raw_data,
        .flag_decode         = flag_decode,
        .flag_use_timestamps = flag_use_timestamps,
        .cur_time            = timestamp,
        .flag_use_nonce      = flag_use_nonce,
        .nonce               = nonce,
        .p_mac_addr          = &gw_mac_addr,
        .p_coordinates       = &coordinates,
    };

    json_stream_gen_t* p_gen = http_json_create_stream_gen_advs(&adv_table, &params);
    ASSERT_NE(nullptr, p_gen);

    string json_str("");
    while (true)
    {
        const char* p_chunk = json_stream_gen_get_next_chunk(p_gen);
        if (nullptr == p_chunk)
        {
            ASSERT_FALSE(nullptr == p_chunk);
        }

        if ('\0' == p_chunk[0])
        {
            break;
        }
        json_str += string(p_chunk);
    }

    ASSERT_EQ(
        string("{\n"
               "  \"data\": {\n"
               "    \"coordinates\": \"170.112233,59.445566\",\n"
               "    \"timestamp\": 1612358920,\n"
               "    \"nonce\": 12345678,\n"
               "    \"gw_mac\": \"AA:CC:EE:00:11:22\",\n"
               "    \"tags\": {\n"
               "      \"AA:BB:CC:01:02:03\": {\n"
               "        \"rssi\": -70,\n"
               "        \"timestamp\": 1612358929,\n"
               "        \"ble_phy\": \"2M\",\n"
               "        \"ble_chan\": 25,\n"
               "        \"ble_tx_power\": 7,\n"
               "        \"data\": \"0201061BFF99040515714D9FC56DFFF0FFF803E4B516E84D7EF41F0C28CBD6\",\n"
               "        \"dataFormat\": 5,\n"
               "        \"temperature\": 27.445,\n"
               "        \"humidity\": 49.6775,\n"
               "        \"pressure\": 100541,\n"
               "        \"accelX\": -0.016,\n"
               "        \"accelY\": -0.008,\n"
               "        \"accelZ\": 0.996,\n"
               "        \"movementCounter\": 232,\n"
               "        \"voltage\": 3.048,\n"
               "        \"txPower\": 4,\n"
               "        \"measurementSequenceNumber\": 19838,\n"
               "        \"id\": \"F4:1F:0C:28:CB:D6\"\n"
               "      }\n"
               "    }\n"
               "  }\n"
               "}"),
        json_str);
    json_stream_gen_delete(&p_gen);
    ASSERT_TRUE(this->m_mem_alloc_trace.is_empty());
}

TEST_F(TestHttpJson, test_df_5_without_raw_and_with_decoded) // NOLINT
{
    const time_t                     timestamp   = 1612358920;
    const mac_address_str_t          gw_mac_addr = { "AA:CC:EE:00:11:22" };
    const ruuvi_gw_cfg_coordinates_t coordinates = { "170.112233,59.445566" };
    const std::array<uint8_t, 31>    data        = {
                  0x02U, 0x01U, 0x06U, 0x1BU, 0xFFU, 0x99U, 0x04U, 0x05U, 0x15U, 0x71U, 0x4DU, 0x9FU, 0xC5U, 0x6DU, 0xFFU, 0xF0U,
                  0xFFU, 0xF8U, 0x03U, 0xE4U, 0xB5U, 0x16U, 0xE8U, 0x4DU, 0x7EU, 0xF4U, 0x1FU, 0x0CU, 0x28U, 0xCBU, 0xD6U,
    };

    adv_report_table_t adv_table = {
        .num_of_advs = 1,
        .table = {
            {
                .timestamp = 1612358929,
                .tag_mac = {0xaa, 0xbb, 0xcc, 0x01, 0x02, 0x03},
                .rssi = -70,
                .primary_phy = RE_CA_UART_BLE_PHY_CODED,
                .secondary_phy = RE_CA_UART_BLE_PHY_CODED,
                .ch_index = 15,
                .is_coded_phy = true,
                .tx_power = 8,
                .data_len = data.size(),
            },
        },
    };
    memcpy(adv_table.table[0].data_buf, data.data(), data.size());

    const bool     flag_raw_data       = false;
    const bool     flag_decode         = true;
    const bool     flag_use_timestamps = true;
    const bool     flag_use_nonce      = true;
    const uint32_t nonce               = 12345678;

    const http_json_create_stream_gen_advs_params_t params = {
        .flag_raw_data       = flag_raw_data,
        .flag_decode         = flag_decode,
        .flag_use_timestamps = flag_use_timestamps,
        .cur_time            = timestamp,
        .flag_use_nonce      = flag_use_nonce,
        .nonce               = nonce,
        .p_mac_addr          = &gw_mac_addr,
        .p_coordinates       = &coordinates,
    };

    json_stream_gen_t* p_gen = http_json_create_stream_gen_advs(&adv_table, &params);
    ASSERT_NE(nullptr, p_gen);

    string json_str("");
    while (true)
    {
        const char* p_chunk = json_stream_gen_get_next_chunk(p_gen);
        if (nullptr == p_chunk)
        {
            ASSERT_FALSE(nullptr == p_chunk);
        }

        if ('\0' == p_chunk[0])
        {
            break;
        }
        json_str += string(p_chunk);
    }

    ASSERT_EQ(
        string("{\n"
               "  \"data\": {\n"
               "    \"coordinates\": \"170.112233,59.445566\",\n"
               "    \"timestamp\": 1612358920,\n"
               "    \"nonce\": 12345678,\n"
               "    \"gw_mac\": \"AA:CC:EE:00:11:22\",\n"
               "    \"tags\": {\n"
               "      \"AA:BB:CC:01:02:03\": {\n"
               "        \"rssi\": -70,\n"
               "        \"timestamp\": 1612358929,\n"
               "        \"ble_phy\": \"Coded\",\n"
               "        \"ble_chan\": 15,\n"
               "        \"ble_tx_power\": 8,\n"
               "        \"dataFormat\": 5,\n"
               "        \"temperature\": 27.445,\n"
               "        \"humidity\": 49.6775,\n"
               "        \"pressure\": 100541,\n"
               "        \"accelX\": -0.016,\n"
               "        \"accelY\": -0.008,\n"
               "        \"accelZ\": 0.996,\n"
               "        \"movementCounter\": 232,\n"
               "        \"voltage\": 3.048,\n"
               "        \"txPower\": 4,\n"
               "        \"measurementSequenceNumber\": 19838,\n"
               "        \"id\": \"F4:1F:0C:28:CB:D6\"\n"
               "      }\n"
               "    }\n"
               "  }\n"
               "}"),
        json_str);
    json_stream_gen_delete(&p_gen);
    ASSERT_TRUE(this->m_mem_alloc_trace.is_empty());
}

TEST_F(TestHttpJson, test_df_6_with_raw_data) // NOLINT
{
    const time_t                     timestamp   = 1612358920;
    const mac_address_str_t          gw_mac_addr = { "AA:CC:EE:00:11:22" };
    const ruuvi_gw_cfg_coordinates_t coordinates = { "170.112233,59.445566" };
    const std::array<uint8_t, 31>    data        = {
                  0x02U, 0x01U, 0x06U,        // BLE advertising header
                  0x03U, 0x03U, 0x98U, 0xFCU, // Ruuvi tag service UUID
                  0x17U, 0xFFU, 0x99U, 0x04U, // Manufacturer-specific data header
                  0x06U,                      // Data format 6
                  0x14U, 0x64U,               // Temperature in 0.005 degrees Celsius
                  0x57U, 0xF8U,               // Humidity in 0.0025 percent
                  0xC7U, 0x9DU,               // Pressure in hPa, from offset 50000
                  0x00U, 0x1DU,               // PM 2.5 in 0.1 µg/m³
                  0x03U, 0x26U,               // CO2 in ppm
                  0x3AU,                      // VOC in ppb
                  0x01U,                      // NOx in ppb
                  0x14U,                      // Luminosity
                  0x6BU,                      // Sound level avg in 0.2 dBA, from offset 18
                  0x05U,                      // Seq cnt
                  0xD0U,                      // Flags
                  0x4EU, 0xB2U, 0x72U,        // 3 least significant bytes of MAC address
    };

    adv_report_table_t adv_table = {
        .num_of_advs = 1,
        .table = {
            {
                .timestamp = 1612358929,
                .tag_mac = {0xaa, 0xbb, 0xcc, 0x01, 0x02, 0x03},
                .rssi = -70,
                .primary_phy = RE_CA_UART_BLE_PHY_1MBPS,
                .secondary_phy = RE_CA_UART_BLE_PHY_NOT_SET,
                .ch_index = 37,
                .is_coded_phy = false,
                .tx_power = RE_CA_UART_BLE_GAP_POWER_LEVEL_INVALID,
                .data_len = data.size(),
            },
        },
    };
    memcpy(adv_table.table[0].data_buf, data.data(), data.size());

    const bool     flag_raw_data       = true;
    const bool     flag_decode         = false;
    const bool     flag_use_timestamps = true;
    const bool     flag_use_nonce      = true;
    const uint32_t nonce               = 12345678;

    const http_json_create_stream_gen_advs_params_t params = {
        .flag_raw_data       = flag_raw_data,
        .flag_decode         = flag_decode,
        .flag_use_timestamps = flag_use_timestamps,
        .cur_time            = timestamp,
        .flag_use_nonce      = flag_use_nonce,
        .nonce               = nonce,
        .p_mac_addr          = &gw_mac_addr,
        .p_coordinates       = &coordinates,
    };

    json_stream_gen_t* p_gen = http_json_create_stream_gen_advs(&adv_table, &params);
    ASSERT_NE(nullptr, p_gen);

    string json_str("");
    while (true)
    {
        const char* p_chunk = json_stream_gen_get_next_chunk(p_gen);
        if (nullptr == p_chunk)
        {
            ASSERT_FALSE(nullptr == p_chunk);
        }

        if ('\0' == p_chunk[0])
        {
            break;
        }
        json_str += string(p_chunk);
    }

    ASSERT_EQ(
        string("{\n"
               "  \"data\": {\n"
               "    \"coordinates\": \"170.112233,59.445566\",\n"
               "    \"timestamp\": 1612358920,\n"
               "    \"nonce\": 12345678,\n"
               "    \"gw_mac\": \"AA:CC:EE:00:11:22\",\n"
               "    \"tags\": {\n"
               "      \"AA:BB:CC:01:02:03\": {\n"
               "        \"rssi\": -70,\n"
               "        \"timestamp\": 1612358929,\n"
               "        \"ble_phy\": \"1M\",\n"
               "        \"ble_chan\": 37,\n"
               "        \"data\": \"020106030398FC17FF990406146457F8C79D001D03263A01146B05D04EB272\"\n"
               "      }\n"
               "    }\n"
               "  }\n"
               "}"),
        json_str);
    json_stream_gen_delete(&p_gen);
    ASSERT_TRUE(this->m_mem_alloc_trace.is_empty());
}

TEST_F(TestHttpJson, test_df_6_with_raw_and_decoded_data) // NOLINT
{
    const time_t                     timestamp   = 1612358920;
    const mac_address_str_t          gw_mac_addr = { "AA:CC:EE:00:11:22" };
    const ruuvi_gw_cfg_coordinates_t coordinates = { "170.112233,59.445566" };
    const std::array<uint8_t, 31>    data        = {
                  0x02U, 0x01U, 0x06U,        // BLE advertising header
                  0x03U, 0x03U, 0x98U, 0xFCU, // Ruuvi tag service UUID
                  0x17U, 0xFFU, 0x99U, 0x04U, // Manufacturer-specific data header
                  0x06U,                      // Data format 6
                  0x14U, 0x64U,               // Temperature in 0.005 degrees Celsius
                  0x57U, 0xF8U,               // Humidity in 0.0025 percent
                  0xC7U, 0x9DU,               // Pressure in hPa, from offset 50000
                  0x00U, 0x1DU,               // PM 2.5 in 0.1 µg/m³
                  0x03U, 0x26U,               // CO2 in ppm
                  0x3AU,                      // VOC in ppb
                  0x01U,                      // NOx in ppb
                  0x14U,                      // Luminosity
                  0x6BU,                      // Sound level avg in 0.2 dBA, from offset 18
                  0x05U,                      // Seq cnt
                  0xD0U,                      // Flags
                  0x4EU, 0xB2U, 0x72U,        // 3 least significant bytes of MAC address
    };

    adv_report_table_t adv_table = {
        .num_of_advs = 1,
        .table = {
            {
                .timestamp = 1612358929,
                .tag_mac = {0xaa, 0xbb, 0xcc, 0x01, 0x02, 0x03},
                .rssi = -70,
                .primary_phy = RE_CA_UART_BLE_PHY_1MBPS,
                .secondary_phy = RE_CA_UART_BLE_PHY_NOT_SET,
                .ch_index = 37,
                .is_coded_phy = false,
                .tx_power = RE_CA_UART_BLE_GAP_POWER_LEVEL_INVALID,
                .data_len = data.size(),
            },
        },
    };
    memcpy(adv_table.table[0].data_buf, data.data(), data.size());

    const bool     flag_raw_data       = true;
    const bool     flag_decode         = true;
    const bool     flag_use_timestamps = true;
    const bool     flag_use_nonce      = true;
    const uint32_t nonce               = 12345678;

    const http_json_create_stream_gen_advs_params_t params = {
        .flag_raw_data       = flag_raw_data,
        .flag_decode         = flag_decode,
        .flag_use_timestamps = flag_use_timestamps,
        .cur_time            = timestamp,
        .flag_use_nonce      = flag_use_nonce,
        .nonce               = nonce,
        .p_mac_addr          = &gw_mac_addr,
        .p_coordinates       = &coordinates,
    };

    json_stream_gen_t* p_gen = http_json_create_stream_gen_advs(&adv_table, &params);
    ASSERT_NE(nullptr, p_gen);

    string json_str("");
    while (true)
    {
        const char* p_chunk = json_stream_gen_get_next_chunk(p_gen);
        if (nullptr == p_chunk)
        {
            ASSERT_FALSE(nullptr == p_chunk);
        }

        if ('\0' == p_chunk[0])
        {
            break;
        }
        json_str += string(p_chunk);
    }

    ASSERT_EQ(
        string("{\n"
               "  \"data\": {\n"
               "    \"coordinates\": \"170.112233,59.445566\",\n"
               "    \"timestamp\": 1612358920,\n"
               "    \"nonce\": 12345678,\n"
               "    \"gw_mac\": \"AA:CC:EE:00:11:22\",\n"
               "    \"tags\": {\n"
               "      \"AA:BB:CC:01:02:03\": {\n"
               "        \"rssi\": -70,\n"
               "        \"timestamp\": 1612358929,\n"
               "        \"ble_phy\": \"1M\",\n"
               "        \"ble_chan\": 37,\n"
               "        \"data\": \"020106030398FC17FF990406146457F8C79D001D03263A01146B05D04EB272\",\n"
               "        \"dataFormat\": 6,\n"
               "        \"temperature\": 26.1,\n"
               "        \"humidity\": 56.3,\n"
               "        \"pressure\": 101101,\n"
               "        \"PM2.5\": 2.9,\n"
               "        \"CO2\": 806,\n"
               "        \"VOC\": 117,\n"
               "        \"NOx\": 3,\n"
               "        \"luminosity\": 1,\n"
               "        \"sound_avg_dba\": 61.0,\n"
               "        \"measurementSequenceNumber\": 5,\n"
               "        \"flag_calibration_in_progress\": false,\n"
               "        \"flag_button_pressed\": false,\n"
               "        \"flag_rtc_running_on_boot\": false,\n"
               "        \"id\": \"4E:B2:72\"\n"
               "      }\n"
               "    }\n"
               "  }\n"
               "}"),
        json_str);
    json_stream_gen_delete(&p_gen);
    ASSERT_TRUE(this->m_mem_alloc_trace.is_empty());
}

TEST_F(TestHttpJson, test_df_6_without_raw_and_with_decoded_data) // NOLINT
{
    const time_t                     timestamp   = 1612358920;
    const mac_address_str_t          gw_mac_addr = { "AA:CC:EE:00:11:22" };
    const ruuvi_gw_cfg_coordinates_t coordinates = { "170.112233,59.445566" };
    const std::array<uint8_t, 31>    data        = {
                  0x02U, 0x01U, 0x06U,        // BLE advertising header
                  0x03U, 0x03U, 0x98U, 0xFCU, // Ruuvi tag service UUID
                  0x17U, 0xFFU, 0x99U, 0x04U, // Manufacturer-specific data header
                  0x06U,                      // Data format 6
                  0x14U, 0x64U,               // Temperature in 0.005 degrees Celsius
                  0x57U, 0xF8U,               // Humidity in 0.0025 percent
                  0xC7U, 0x9DU,               // Pressure in hPa, from offset 50000
                  0x00U, 0x1DU,               // PM 2.5 in 0.1 µg/m³
                  0x03U, 0x26U,               // CO2 in ppm
                  0x3AU,                      // VOC in ppb
                  0x01U,                      // NOx in ppb
                  0x14U,                      // Luminosity
                  0x6BU,                      // Sound level avg in 0.2 dBA, from offset 18
                  0x05U,                      // Seq cnt
                  0xD0U,                      // Flags
                  0x4EU, 0xB2U, 0x72U,        // 3 least significant bytes of MAC address
    };

    adv_report_table_t adv_table = {
        .num_of_advs = 1,
        .table = {
            {
                .timestamp = 1612358929,
                .tag_mac = {0xaa, 0xbb, 0xcc, 0x01, 0x02, 0x03},
                .rssi = -70,
                .primary_phy = RE_CA_UART_BLE_PHY_1MBPS,
                .secondary_phy = RE_CA_UART_BLE_PHY_NOT_SET,
                .ch_index = 37,
                .is_coded_phy = false,
                .tx_power = RE_CA_UART_BLE_GAP_POWER_LEVEL_INVALID,
                .data_len = data.size(),
            },
        },
    };
    memcpy(adv_table.table[0].data_buf, data.data(), data.size());

    const bool     flag_raw_data       = false;
    const bool     flag_decode         = true;
    const bool     flag_use_timestamps = true;
    const bool     flag_use_nonce      = true;
    const uint32_t nonce               = 12345678;

    const http_json_create_stream_gen_advs_params_t params = {
        .flag_raw_data       = flag_raw_data,
        .flag_decode         = flag_decode,
        .flag_use_timestamps = flag_use_timestamps,
        .cur_time            = timestamp,
        .flag_use_nonce      = flag_use_nonce,
        .nonce               = nonce,
        .p_mac_addr          = &gw_mac_addr,
        .p_coordinates       = &coordinates,
    };

    json_stream_gen_t* p_gen = http_json_create_stream_gen_advs(&adv_table, &params);
    ASSERT_NE(nullptr, p_gen);

    string json_str("");
    while (true)
    {
        const char* p_chunk = json_stream_gen_get_next_chunk(p_gen);
        if (nullptr == p_chunk)
        {
            ASSERT_FALSE(nullptr == p_chunk);
        }

        if ('\0' == p_chunk[0])
        {
            break;
        }
        json_str += string(p_chunk);
    }

    ASSERT_EQ(
        string("{\n"
               "  \"data\": {\n"
               "    \"coordinates\": \"170.112233,59.445566\",\n"
               "    \"timestamp\": 1612358920,\n"
               "    \"nonce\": 12345678,\n"
               "    \"gw_mac\": \"AA:CC:EE:00:11:22\",\n"
               "    \"tags\": {\n"
               "      \"AA:BB:CC:01:02:03\": {\n"
               "        \"rssi\": -70,\n"
               "        \"timestamp\": 1612358929,\n"
               "        \"ble_phy\": \"1M\",\n"
               "        \"ble_chan\": 37,\n"
               "        \"dataFormat\": 6,\n"
               "        \"temperature\": 26.1,\n"
               "        \"humidity\": 56.3,\n"
               "        \"pressure\": 101101,\n"
               "        \"PM2.5\": 2.9,\n"
               "        \"CO2\": 806,\n"
               "        \"VOC\": 117,\n"
               "        \"NOx\": 3,\n"
               "        \"luminosity\": 1,\n"
               "        \"sound_avg_dba\": 61.0,\n"
               "        \"measurementSequenceNumber\": 5,\n"
               "        \"flag_calibration_in_progress\": false,\n"
               "        \"flag_button_pressed\": false,\n"
               "        \"flag_rtc_running_on_boot\": false,\n"
               "        \"id\": \"4E:B2:72\"\n"
               "      }\n"
               "    }\n"
               "  }\n"
               "}"),
        json_str);
    json_stream_gen_delete(&p_gen);
    ASSERT_TRUE(this->m_mem_alloc_trace.is_empty());
}

TEST_F(TestHttpJson, test_df_7_with_raw_and_without_decoded_data) // NOLINT
{
    const time_t                     timestamp   = 1612358920;
    const mac_address_str_t          gw_mac_addr = { "AA:CC:EE:00:11:22" };
    const ruuvi_gw_cfg_coordinates_t coordinates = { "170.112233,59.445566" };
    const std::array<uint8_t, 27>    data        = {
                  0x02U, 0x01U, 0x06U,        // BLE advertising header
                  0x17U, 0xFFU, 0x99U, 0x04U, // Manufacturer-specific data header
                  0x07U,                      // Data format 7
                  0x05U,                      // Message counter
                  0x03U,                      // State flags
                  0x14U, 0x64U,               // Temperature in 0.005 degrees Celsius
                  0x57U, 0xF8U,               // Humidity in 0.0025 percent
                  0xC7U, 0x9DU,               // Pressure in hPa, from offset 50000
                  0x1BU,                      // Tilt X (pitch) in 0.1 degrees
                  0xE5U,                      // Tilt Y (roll) in 0.1 degrees
                  0x00U, 0x01U,               // Luminosity
                  0x00U,                      // Color temperature
                  0xAAU,                      // Battery (4-bit) + Motion intensity (4-bit)
                  0x05U,                      // Motion count
                  0x0FU,                      // CRC8 over bytes 0-15
                  0x4EU, 0xB2U, 0x72U,        // 3 least significant bytes of MAC address
    };

    adv_report_table_t adv_table = {
        .num_of_advs = 1,
        .table = {
            {
                .timestamp = 1612358929,
                .tag_mac = {0xaa, 0xbb, 0xcc, 0x01, 0x02, 0x03},
                .rssi = -70,
                .primary_phy = RE_CA_UART_BLE_PHY_1MBPS,
                .secondary_phy = RE_CA_UART_BLE_PHY_NOT_SET,
                .ch_index = 37,
                .is_coded_phy = false,
                .tx_power = RE_CA_UART_BLE_GAP_POWER_LEVEL_INVALID,
                .data_len = data.size(),
            },
        },
    };
    memcpy(adv_table.table[0].data_buf, data.data(), data.size());

    const bool     flag_raw_data       = true;
    const bool     flag_decode         = false;
    const bool     flag_use_timestamps = true;
    const bool     flag_use_nonce      = true;
    const uint32_t nonce               = 12345678;

    const http_json_create_stream_gen_advs_params_t params = {
        .flag_raw_data       = flag_raw_data,
        .flag_decode         = flag_decode,
        .flag_use_timestamps = flag_use_timestamps,
        .cur_time            = timestamp,
        .flag_use_nonce      = flag_use_nonce,
        .nonce               = nonce,
        .p_mac_addr          = &gw_mac_addr,
        .p_coordinates       = &coordinates,
    };

    json_stream_gen_t* p_gen = http_json_create_stream_gen_advs(&adv_table, &params);
    ASSERT_NE(nullptr, p_gen);

    string json_str("");
    while (true)
    {
        const char* p_chunk = json_stream_gen_get_next_chunk(p_gen);
        if (nullptr == p_chunk)
        {
            ASSERT_FALSE(nullptr == p_chunk);
        }

        if ('\0' == p_chunk[0])
        {
            break;
        }
        json_str += string(p_chunk);
    }

    ASSERT_EQ(
        string("{\n"
               "  \"data\": {\n"
               "    \"coordinates\": \"170.112233,59.445566\",\n"
               "    \"timestamp\": 1612358920,\n"
               "    \"nonce\": 12345678,\n"
               "    \"gw_mac\": \"AA:CC:EE:00:11:22\",\n"
               "    \"tags\": {\n"
               "      \"AA:BB:CC:01:02:03\": {\n"
               "        \"rssi\": -70,\n"
               "        \"timestamp\": 1612358929,\n"
               "        \"ble_phy\": \"1M\",\n"
               "        \"ble_chan\": 37,\n"
               "        \"data\": \"02010617FF9904070503146457F8C79D1BE5000100AA050F4EB272\"\n"
               "      }\n"
               "    }\n"
               "  }\n"
               "}"),
        json_str);
    json_stream_gen_delete(&p_gen);
    ASSERT_TRUE(this->m_mem_alloc_trace.is_empty());
}

TEST_F(TestHttpJson, test_df_7_with_raw_and_decoded_data) // NOLINT
{
    const time_t                     timestamp   = 1612358920;
    const mac_address_str_t          gw_mac_addr = { "AA:CC:EE:00:11:22" };
    const ruuvi_gw_cfg_coordinates_t coordinates = { "170.112233,59.445566" };
    const std::array<uint8_t, 27>    data        = {
                  0x02U, 0x01U, 0x06U,        // BLE advertising header
                  0x17U, 0xFFU, 0x99U, 0x04U, // Manufacturer-specific data header
                  0x07U,                      // Data format 7
                  0x05U,                      // Message counter
                  0x03U,                      // State flags
                  0x14U, 0x64U,               // Temperature in 0.005 degrees Celsius
                  0x57U, 0xF8U,               // Humidity in 0.0025 percent
                  0xC7U, 0x9DU,               // Pressure in hPa, from offset 50000
                  0x1BU,                      // Tilt X (pitch) in 0.1 degrees
                  0xE5U,                      // Tilt Y (roll) in 0.1 degrees
                  0x00U, 0x01U,               // Luminosity
                  0x00U,                      // Color temperature
                  0xAAU,                      // Battery (4-bit) + Motion intensity (4-bit)
                  0x05U,                      // Motion count
                  0x0FU,                      // CRC8 over bytes 0-15
                  0x4EU, 0xB2U, 0x72U,        // 3 least significant bytes of MAC address
    };

    adv_report_table_t adv_table = {
        .num_of_advs = 1,
        .table = {
            {
                .timestamp = 1612358929,
                .tag_mac = {0xaa, 0xbb, 0xcc, 0x01, 0x02, 0x03},
                .rssi = -70,
                .primary_phy = RE_CA_UART_BLE_PHY_1MBPS,
                .secondary_phy = RE_CA_UART_BLE_PHY_NOT_SET,
                .ch_index = 37,
                .is_coded_phy = false,
                .tx_power = RE_CA_UART_BLE_GAP_POWER_LEVEL_INVALID,
                .data_len = data.size(),
            },
        },
    };
    memcpy(adv_table.table[0].data_buf, data.data(), data.size());

    const bool     flag_raw_data       = true;
    const bool     flag_decode         = true;
    const bool     flag_use_timestamps = true;
    const bool     flag_use_nonce      = true;
    const uint32_t nonce               = 12345678;

    const http_json_create_stream_gen_advs_params_t params = {
        .flag_raw_data       = flag_raw_data,
        .flag_decode         = flag_decode,
        .flag_use_timestamps = flag_use_timestamps,
        .cur_time            = timestamp,
        .flag_use_nonce      = flag_use_nonce,
        .nonce               = nonce,
        .p_mac_addr          = &gw_mac_addr,
        .p_coordinates       = &coordinates,
    };

    json_stream_gen_t* p_gen = http_json_create_stream_gen_advs(&adv_table, &params);
    ASSERT_NE(nullptr, p_gen);

    string json_str("");
    while (true)
    {
        const char* p_chunk = json_stream_gen_get_next_chunk(p_gen);
        if (nullptr == p_chunk)
        {
            ASSERT_FALSE(nullptr == p_chunk);
        }

        if ('\0' == p_chunk[0])
        {
            break;
        }
        json_str += string(p_chunk);
    }

    ASSERT_EQ(
        string("{\n"
               "  \"data\": {\n"
               "    \"coordinates\": \"170.112233,59.445566\",\n"
               "    \"timestamp\": 1612358920,\n"
               "    \"nonce\": 12345678,\n"
               "    \"gw_mac\": \"AA:CC:EE:00:11:22\",\n"
               "    \"tags\": {\n"
               "      \"AA:BB:CC:01:02:03\": {\n"
               "        \"rssi\": -70,\n"
               "        \"timestamp\": 1612358929,\n"
               "        \"ble_phy\": \"1M\",\n"
               "        \"ble_chan\": 37,\n"
               "        \"data\": \"02010617FF9904070503146457F8C79D1BE5000100AA050F4EB272\",\n"
               "        \"dataFormat\": 7,\n"
               "        \"temperature\": 26.1,\n"
               "        \"humidity\": 56.3,\n"
               "        \"pressure\": 101101,\n"
               "        \"tiltX\": 19.3,\n"
               "        \"tiltY\": -19.3,\n"
               "        \"luminosity\": 1,\n"
               "        \"colorTemperature\": 1000,\n"
               "        \"battery\": 3.086,\n"
               "        \"motionIntensity\": 10,\n"
               "        \"motionCount\": 5,\n"
               "        \"measurementSequenceNumber\": 5,\n"
               "        \"motionDetected\": true,\n"
               "        \"presenceDetected\": true,\n"
               "        \"id\": \"4E:B2:72\"\n"
               "      }\n"
               "    }\n"
               "  }\n"
               "}"),
        json_str);
    json_stream_gen_delete(&p_gen);
    ASSERT_TRUE(this->m_mem_alloc_trace.is_empty());
}

TEST_F(TestHttpJson, test_df_7_without_raw_and_with_decoded_data) // NOLINT
{
    const time_t                     timestamp   = 1612358920;
    const mac_address_str_t          gw_mac_addr = { "AA:CC:EE:00:11:22" };
    const ruuvi_gw_cfg_coordinates_t coordinates = { "170.112233,59.445566" };
    const std::array<uint8_t, 27>    data        = {
                  0x02U, 0x01U, 0x06U,        // BLE advertising header
                  0x17U, 0xFFU, 0x99U, 0x04U, // Manufacturer-specific data header
                  0x07U,                      // Data format 7
                  0x05U,                      // Message counter
                  0x03U,                      // State flags
                  0x14U, 0x64U,               // Temperature in 0.005 degrees Celsius
                  0x57U, 0xF8U,               // Humidity in 0.0025 percent
                  0xC7U, 0x9DU,               // Pressure in hPa, from offset 50000
                  0x1BU,                      // Tilt X (pitch) in 0.1 degrees
                  0xE5U,                      // Tilt Y (roll) in 0.1 degrees
                  0x00U, 0x01U,               // Luminosity
                  0x00U,                      // Color temperature
                  0xAAU,                      // Battery (4-bit) + Motion intensity (4-bit)
                  0x05U,                      // Motion count
                  0x0FU,                      // CRC8 over bytes 0-15
                  0x4EU, 0xB2U, 0x72U,        // 3 least significant bytes of MAC address
    };

    adv_report_table_t adv_table = {
        .num_of_advs = 1,
        .table = {
            {
                .timestamp = 1612358929,
                .tag_mac = {0xaa, 0xbb, 0xcc, 0x01, 0x02, 0x03},
                .rssi = -70,
                .primary_phy = RE_CA_UART_BLE_PHY_1MBPS,
                .secondary_phy = RE_CA_UART_BLE_PHY_NOT_SET,
                .ch_index = 37,
                .is_coded_phy = false,
                .tx_power = RE_CA_UART_BLE_GAP_POWER_LEVEL_INVALID,
                .data_len = data.size(),
            },
        },
    };
    memcpy(adv_table.table[0].data_buf, data.data(), data.size());

    const bool     flag_raw_data       = false;
    const bool     flag_decode         = true;
    const bool     flag_use_timestamps = true;
    const bool     flag_use_nonce      = true;
    const uint32_t nonce               = 12345678;

    const http_json_create_stream_gen_advs_params_t params = {
        .flag_raw_data       = flag_raw_data,
        .flag_decode         = flag_decode,
        .flag_use_timestamps = flag_use_timestamps,
        .cur_time            = timestamp,
        .flag_use_nonce      = flag_use_nonce,
        .nonce               = nonce,
        .p_mac_addr          = &gw_mac_addr,
        .p_coordinates       = &coordinates,
    };

    json_stream_gen_t* p_gen = http_json_create_stream_gen_advs(&adv_table, &params);
    ASSERT_NE(nullptr, p_gen);

    string json_str("");
    while (true)
    {
        const char* p_chunk = json_stream_gen_get_next_chunk(p_gen);
        if (nullptr == p_chunk)
        {
            ASSERT_FALSE(nullptr == p_chunk);
        }

        if ('\0' == p_chunk[0])
        {
            break;
        }
        json_str += string(p_chunk);
    }

    ASSERT_EQ(
        string("{\n"
               "  \"data\": {\n"
               "    \"coordinates\": \"170.112233,59.445566\",\n"
               "    \"timestamp\": 1612358920,\n"
               "    \"nonce\": 12345678,\n"
               "    \"gw_mac\": \"AA:CC:EE:00:11:22\",\n"
               "    \"tags\": {\n"
               "      \"AA:BB:CC:01:02:03\": {\n"
               "        \"rssi\": -70,\n"
               "        \"timestamp\": 1612358929,\n"
               "        \"ble_phy\": \"1M\",\n"
               "        \"ble_chan\": 37,\n"
               "        \"dataFormat\": 7,\n"
               "        \"temperature\": 26.1,\n"
               "        \"humidity\": 56.3,\n"
               "        \"pressure\": 101101,\n"
               "        \"tiltX\": 19.3,\n"
               "        \"tiltY\": -19.3,\n"
               "        \"luminosity\": 1,\n"
               "        \"colorTemperature\": 1000,\n"
               "        \"battery\": 3.086,\n"
               "        \"motionIntensity\": 10,\n"
               "        \"motionCount\": 5,\n"
               "        \"measurementSequenceNumber\": 5,\n"
               "        \"motionDetected\": true,\n"
               "        \"presenceDetected\": true,\n"
               "        \"id\": \"4E:B2:72\"\n"
               "      }\n"
               "    }\n"
               "  }\n"
               "}"),
        json_str);
    json_stream_gen_delete(&p_gen);
    ASSERT_TRUE(this->m_mem_alloc_trace.is_empty());
}

TEST_F(TestHttpJson, test_df_e1_with_raw_data) // NOLINT
{
    const time_t                     timestamp   = 1612358920;
    const mac_address_str_t          gw_mac_addr = { "AA:CC:EE:00:11:22" };
    const ruuvi_gw_cfg_coordinates_t coordinates = { "170.112233,59.445566" };
    const std::array<uint8_t, 48>    data        = {
                  0x2BU, 0xFFU, 0x99U, 0x04U,               // Manufacturer-specific data header
                  0xE1U,                                    // Data format E1
                  0x16U, 0x1EU,                             // Temperature in 0.005 degrees Celsius
                  0x52U, 0x6CU,                             // Humidity in 0.0025 percent
                  0xC6U, 0x8BU,                             // Pressure in hPa, from offset 50000
                  0x00U, 0x6EU,                             // PM 1.0 in 0.1 µg/m³
                  0x00U, 0x72U,                             // PM 2.5 in 0.1 µg/m³
                  0x00U, 0x73U,                             // PM 4.0 in 0.1 µg/m³
                  0x00U, 0x74U,                             // PM 10.0 in 0.1 µg/m³
                  0x03U, 0x76U,                             // CO2 in ppm
                  0x10U,                                    // VOC in ppb
                  0x01U,                                    // NOx in ppb
                  0x0CU, 0x1DU, 0x90U,                      // Luminosity
                  0x1EU,                                    // Sound level inst in 0.2 dBA, from offset 18
                  0x5CU,                                    // Sound level avg in 0.2 dBA, from offset 18
                  0xB6U,                                    // Sound level peak in 0.2 SPL dB, from offset 18
                  0x12U, 0x13U, 0x14U,                      // Seq cnt
                  0xF8U,                                    // Flags
                  0xFFU, 0xFFU, 0xFFU, 0xFFU, 0xFFU,        // Reserved
                  0xE3U, 0x75U, 0xCFU, 0x37U, 0x4EU, 0x23U, // MAC address
                  0x03U, 0x03U, 0x98U, 0xFCU,               // Ruuvi tag service UUID
    };

    adv_report_table_t adv_table = {
        .num_of_advs = 1,
        .table = {
            {
                .timestamp = 1612358929,
                .tag_mac = {0xaa, 0xbb, 0xcc, 0x01, 0x02, 0x03},
                .rssi = -70,
                .primary_phy = RE_CA_UART_BLE_PHY_1MBPS,
                .secondary_phy = RE_CA_UART_BLE_PHY_NOT_SET,
                .ch_index = 37,
                .is_coded_phy = false,
                .tx_power = RE_CA_UART_BLE_GAP_POWER_LEVEL_INVALID,
                .data_len = data.size(),
            },
        },
    };
    memcpy(adv_table.table[0].data_buf, data.data(), data.size());

    const bool     flag_raw_data       = true;
    const bool     flag_decode         = false;
    const bool     flag_use_timestamps = true;
    const bool     flag_use_nonce      = true;
    const uint32_t nonce               = 12345678;

    const http_json_create_stream_gen_advs_params_t params = {
        .flag_raw_data       = flag_raw_data,
        .flag_decode         = flag_decode,
        .flag_use_timestamps = flag_use_timestamps,
        .cur_time            = timestamp,
        .flag_use_nonce      = flag_use_nonce,
        .nonce               = nonce,
        .p_mac_addr          = &gw_mac_addr,
        .p_coordinates       = &coordinates,
    };

    json_stream_gen_t* p_gen = http_json_create_stream_gen_advs(&adv_table, &params);
    ASSERT_NE(nullptr, p_gen);

    string json_str("");
    while (true)
    {
        const char* p_chunk = json_stream_gen_get_next_chunk(p_gen);
        if (nullptr == p_chunk)
        {
            ASSERT_FALSE(nullptr == p_chunk);
        }

        if ('\0' == p_chunk[0])
        {
            break;
        }
        json_str += string(p_chunk);
    }

    ASSERT_EQ(
        string("{\n"
               "  \"data\": {\n"
               "    \"coordinates\": \"170.112233,59.445566\",\n"
               "    \"timestamp\": 1612358920,\n"
               "    \"nonce\": 12345678,\n"
               "    \"gw_mac\": \"AA:CC:EE:00:11:22\",\n"
               "    \"tags\": {\n"
               "      \"AA:BB:CC:01:02:03\": {\n"
               "        \"rssi\": -70,\n"
               "        \"timestamp\": 1612358929,\n"
               "        \"ble_phy\": \"1M\",\n"
               "        \"ble_chan\": 37,\n"
               "        \"data\": "
               "\"2BFF9904E1161E526CC68B006E007200730074037610010C1D901E5CB6121314F8FFFFFFFFFFE375CF374E23030398FC\"\n"
               "      }\n"
               "    }\n"
               "  }\n"
               "}"),
        json_str);
    json_stream_gen_delete(&p_gen);
    ASSERT_TRUE(this->m_mem_alloc_trace.is_empty());
}

TEST_F(TestHttpJson, test_df_e1_with_raw_and_decoded_data) // NOLINT
{
    const time_t                     timestamp   = 1612358920;
    const mac_address_str_t          gw_mac_addr = { "AA:CC:EE:00:11:22" };
    const ruuvi_gw_cfg_coordinates_t coordinates = { "170.112233,59.445566" };
    const std::array<uint8_t, 48>    data        = {
                  0x2BU, 0xFFU, 0x99U, 0x04U,               // Manufacturer-specific data header
                  0xE1U,                                    // Data format E1
                  0x16U, 0x1EU,                             // Temperature in 0.005 degrees Celsius
                  0x52U, 0x6CU,                             // Humidity in 0.0025 percent
                  0xC6U, 0x8BU,                             // Pressure in hPa, from offset 50000
                  0x00U, 0x6EU,                             // PM 1.0 in 0.1 µg/m³
                  0x00U, 0x72U,                             // PM 2.5 in 0.1 µg/m³
                  0x00U, 0x73U,                             // PM 4.0 in 0.1 µg/m³
                  0x00U, 0x74U,                             // PM 10.0 in 0.1 µg/m³
                  0x03U, 0x76U,                             // CO2 in ppm
                  0x10U,                                    // VOC in ppb
                  0x01U,                                    // NOx in ppb
                  0x0CU, 0x1DU, 0x90U,                      // Luminosity
                  0x1EU,                                    // Sound level inst in 0.2 dBA, from offset 18
                  0x5CU,                                    // Sound level avg in 0.2 dBA, from offset 18
                  0xB6U,                                    // Sound level peak in 0.2 SPL dB, from offset 18
                  0x12U, 0x13U, 0x14U,                      // Seq cnt
                  0xF8U,                                    // Flags
                  0xFFU, 0xFFU, 0xFFU, 0xFFU, 0xFFU,        // Reserved
                  0xE3U, 0x75U, 0xCFU, 0x37U, 0x4EU, 0x23U, // MAC address
                  0x03U, 0x03U, 0x98U, 0xFCU,               // Ruuvi tag service UUID
    };

    adv_report_table_t adv_table = {
        .num_of_advs = 1,
        .table = {
            {
                .timestamp = 1612358929,
                .tag_mac = {0xaa, 0xbb, 0xcc, 0x01, 0x02, 0x03},
                .rssi = -70,
                .primary_phy = RE_CA_UART_BLE_PHY_1MBPS,
                .secondary_phy = RE_CA_UART_BLE_PHY_NOT_SET,
                .ch_index = 37,
                .is_coded_phy = false,
                .tx_power = RE_CA_UART_BLE_GAP_POWER_LEVEL_INVALID,
                .data_len = data.size(),
            },
        },
    };
    memcpy(adv_table.table[0].data_buf, data.data(), data.size());

    const bool     flag_raw_data       = true;
    const bool     flag_decode         = true;
    const bool     flag_use_timestamps = true;
    const bool     flag_use_nonce      = true;
    const uint32_t nonce               = 12345678;

    const http_json_create_stream_gen_advs_params_t params = {
        .flag_raw_data       = flag_raw_data,
        .flag_decode         = flag_decode,
        .flag_use_timestamps = flag_use_timestamps,
        .cur_time            = timestamp,
        .flag_use_nonce      = flag_use_nonce,
        .nonce               = nonce,
        .p_mac_addr          = &gw_mac_addr,
        .p_coordinates       = &coordinates,
    };

    json_stream_gen_t* p_gen = http_json_create_stream_gen_advs(&adv_table, &params);
    ASSERT_NE(nullptr, p_gen);

    string json_str("");
    while (true)
    {
        const char* p_chunk = json_stream_gen_get_next_chunk(p_gen);
        if (nullptr == p_chunk)
        {
            ASSERT_FALSE(nullptr == p_chunk);
        }

        if ('\0' == p_chunk[0])
        {
            break;
        }
        json_str += string(p_chunk);
    }

    ASSERT_EQ(
        string("{\n"
               "  \"data\": {\n"
               "    \"coordinates\": \"170.112233,59.445566\",\n"
               "    \"timestamp\": 1612358920,\n"
               "    \"nonce\": 12345678,\n"
               "    \"gw_mac\": \"AA:CC:EE:00:11:22\",\n"
               "    \"tags\": {\n"
               "      \"AA:BB:CC:01:02:03\": {\n"
               "        \"rssi\": -70,\n"
               "        \"timestamp\": 1612358929,\n"
               "        \"ble_phy\": \"1M\",\n"
               "        \"ble_chan\": 37,\n"
               "        \"data\": "
               "\"2BFF9904E1161E526CC68B006E007200730074037610010C1D901E5CB6121314F8FFFFFFFFFFE375CF374E23030398FC\",\n"
               "        \"dataFormat\": 225,\n"
               "        \"temperature\": 28.310,\n"
               "        \"humidity\": 52.75,\n"
               "        \"pressure\": 100827,\n"
               "        \"PM1.0\": 11.0,\n"
               "        \"PM2.5\": 11.4,\n"
               "        \"PM4.0\": 11.5,\n"
               "        \"PM10.0\": 11.6,\n"
               "        \"CO2\": 886,\n"
               "        \"VOC\": 33,\n"
               "        \"NOx\": 3,\n"
               "        \"luminosity\": 7940,\n"
               "        \"sound_inst_dba\": 30.2,\n"
               "        \"sound_avg_dba\": 55.0,\n"
               "        \"sound_peak_spl_db\": 91.0,\n"
               "        \"measurementSequenceNumber\": 1184532,\n"
               "        \"flag_calibration_in_progress\": false,\n"
               "        \"flag_button_pressed\": false,\n"
               "        \"flag_rtc_running_on_boot\": false,\n"
               "        \"id\": \"E3:75:CF:37:4E:23\"\n"
               "      }\n"
               "    }\n"
               "  }\n"
               "}"),
        json_str);
    json_stream_gen_delete(&p_gen);
    ASSERT_TRUE(this->m_mem_alloc_trace.is_empty());
}

TEST_F(TestHttpJson, test_df_e1_without_raw_and_with_decoded_data) // NOLINT
{
    const time_t                     timestamp   = 1612358920;
    const mac_address_str_t          gw_mac_addr = { "AA:CC:EE:00:11:22" };
    const ruuvi_gw_cfg_coordinates_t coordinates = { "170.112233,59.445566" };
    const std::array<uint8_t, 48>    data        = {
                  0x2BU, 0xFFU, 0x99U, 0x04U,               // Manufacturer-specific data header
                  0xE1U,                                    // Data format E1
                  0x16U, 0x1EU,                             // Temperature in 0.005 degrees Celsius
                  0x52U, 0x6CU,                             // Humidity in 0.0025 percent
                  0xC6U, 0x8BU,                             // Pressure in hPa, from offset 50000
                  0x00U, 0x6EU,                             // PM 1.0 in 0.1 µg/m³
                  0x00U, 0x72U,                             // PM 2.5 in 0.1 µg/m³
                  0x00U, 0x73U,                             // PM 4.0 in 0.1 µg/m³
                  0x00U, 0x74U,                             // PM 10.0 in 0.1 µg/m³
                  0x03U, 0x76U,                             // CO2 in ppm
                  0x10U,                                    // VOC in ppb
                  0x01U,                                    // NOx in ppb
                  0x0CU, 0x1DU, 0x90U,                      // Luminosity
                  0x1EU,                                    // Sound level inst in 0.2 dBA, from offset 18
                  0x5CU,                                    // Sound level avg in 0.2 dBA, from offset 18
                  0xB6U,                                    // Sound level peak in 0.2 SPL dB, from offset 18
                  0x12U, 0x13U, 0x14U,                      // Seq cnt
                  0xF8U,                                    // Flags
                  0xFFU, 0xFFU, 0xFFU, 0xFFU, 0xFFU,        // Reserved
                  0xE3U, 0x75U, 0xCFU, 0x37U, 0x4EU, 0x23U, // MAC address
                  0x03U, 0x03U, 0x98U, 0xFCU,               // Ruuvi tag service UUID
    };

    adv_report_table_t adv_table = {
        .num_of_advs = 1,
        .table = {
            {
                .timestamp = 1612358929,
                .tag_mac = {0xaa, 0xbb, 0xcc, 0x01, 0x02, 0x03},
                .rssi = -70,
                .primary_phy = RE_CA_UART_BLE_PHY_1MBPS,
                .secondary_phy = RE_CA_UART_BLE_PHY_NOT_SET,
                .ch_index = 37,
                .is_coded_phy = false,
                .tx_power = RE_CA_UART_BLE_GAP_POWER_LEVEL_INVALID,
                .data_len = data.size(),
            },
        },
    };
    memcpy(adv_table.table[0].data_buf, data.data(), data.size());

    const bool     flag_raw_data       = false;
    const bool     flag_decode         = true;
    const bool     flag_use_timestamps = true;
    const bool     flag_use_nonce      = true;
    const uint32_t nonce               = 12345678;

    const http_json_create_stream_gen_advs_params_t params = {
        .flag_raw_data       = flag_raw_data,
        .flag_decode         = flag_decode,
        .flag_use_timestamps = flag_use_timestamps,
        .cur_time            = timestamp,
        .flag_use_nonce      = flag_use_nonce,
        .nonce               = nonce,
        .p_mac_addr          = &gw_mac_addr,
        .p_coordinates       = &coordinates,
    };

    json_stream_gen_t* p_gen = http_json_create_stream_gen_advs(&adv_table, &params);
    ASSERT_NE(nullptr, p_gen);

    string json_str("");
    while (true)
    {
        const char* p_chunk = json_stream_gen_get_next_chunk(p_gen);
        if (nullptr == p_chunk)
        {
            ASSERT_FALSE(nullptr == p_chunk);
        }

        if ('\0' == p_chunk[0])
        {
            break;
        }
        json_str += string(p_chunk);
    }

    ASSERT_EQ(
        string("{\n"
               "  \"data\": {\n"
               "    \"coordinates\": \"170.112233,59.445566\",\n"
               "    \"timestamp\": 1612358920,\n"
               "    \"nonce\": 12345678,\n"
               "    \"gw_mac\": \"AA:CC:EE:00:11:22\",\n"
               "    \"tags\": {\n"
               "      \"AA:BB:CC:01:02:03\": {\n"
               "        \"rssi\": -70,\n"
               "        \"timestamp\": 1612358929,\n"
               "        \"ble_phy\": \"1M\",\n"
               "        \"ble_chan\": 37,\n"
               "        \"dataFormat\": 225,\n"
               "        \"temperature\": 28.310,\n"
               "        \"humidity\": 52.75,\n"
               "        \"pressure\": 100827,\n"
               "        \"PM1.0\": 11.0,\n"
               "        \"PM2.5\": 11.4,\n"
               "        \"PM4.0\": 11.5,\n"
               "        \"PM10.0\": 11.6,\n"
               "        \"CO2\": 886,\n"
               "        \"VOC\": 33,\n"
               "        \"NOx\": 3,\n"
               "        \"luminosity\": 7940,\n"
               "        \"sound_inst_dba\": 30.2,\n"
               "        \"sound_avg_dba\": 55.0,\n"
               "        \"sound_peak_spl_db\": 91.0,\n"
               "        \"measurementSequenceNumber\": 1184532,\n"
               "        \"flag_calibration_in_progress\": false,\n"
               "        \"flag_button_pressed\": false,\n"
               "        \"flag_rtc_running_on_boot\": false,\n"
               "        \"id\": \"E3:75:CF:37:4E:23\"\n"
               "      }\n"
               "    }\n"
               "  }\n"
               "}"),
        json_str);
    json_stream_gen_delete(&p_gen);
    ASSERT_TRUE(this->m_mem_alloc_trace.is_empty());
}

TEST_F(TestHttpJson, test_1_without_timestamp) // NOLINT
{
    const time_t                     timestamp   = 1612358920;
    const mac_address_str_t          gw_mac_addr = { "AA:CC:EE:00:11:22" };
    const ruuvi_gw_cfg_coordinates_t coordinates = { "170.112233,59.445566" };
    const std::array<uint8_t, 1>     data        = { 0xAAU };

    adv_report_table_t adv_table = {
        .num_of_advs = 1,
        .table = {
            {
                .timestamp = 1011,
                .tag_mac = {0xaa, 0xbb, 0xcc, 0x01, 0x02, 0x03},
                .rssi = -70,
                .primary_phy = RE_CA_UART_BLE_PHY_1MBPS,
                .secondary_phy = RE_CA_UART_BLE_PHY_NOT_SET,
                .ch_index = 37,
                .is_coded_phy = false,
                .tx_power = 8,
                .data_len = data.size(),
            },
        },
    };
    memcpy(adv_table.table[0].data_buf, data.data(), data.size());

    const bool     flag_raw_data       = true;
    const bool     flag_decode         = false;
    const bool     flag_use_timestamps = false;
    const bool     flag_use_nonce      = true;
    const uint32_t nonce               = 12345678;

    const http_json_create_stream_gen_advs_params_t params = {
        .flag_raw_data       = flag_raw_data,
        .flag_decode         = flag_decode,
        .flag_use_timestamps = flag_use_timestamps,
        .cur_time            = timestamp,
        .flag_use_nonce      = flag_use_nonce,
        .nonce               = nonce,
        .p_mac_addr          = &gw_mac_addr,
        .p_coordinates       = &coordinates,
    };

    json_stream_gen_t* p_gen = http_json_create_stream_gen_advs(&adv_table, &params);
    ASSERT_NE(nullptr, p_gen);

    string json_str("");
    while (true)
    {
        const char* p_chunk = json_stream_gen_get_next_chunk(p_gen);
        if (nullptr == p_chunk)
        {
            ASSERT_FALSE(nullptr == p_chunk);
        }

        if ('\0' == p_chunk[0])
        {
            break;
        }
        json_str += string(p_chunk);
    }

    ASSERT_EQ(
        string("{\n"
               "  \"data\": {\n"
               "    \"coordinates\": \"170.112233,59.445566\",\n"
               "    \"nonce\": 12345678,\n"
               "    \"gw_mac\": \"AA:CC:EE:00:11:22\",\n"
               "    \"tags\": {\n"
               "      \"AA:BB:CC:01:02:03\": {\n"
               "        \"rssi\": -70,\n"
               "        \"counter\": 1011,\n"
               "        \"ble_phy\": \"1M\",\n"
               "        \"ble_chan\": 37,\n"
               "        \"ble_tx_power\": 8,\n"
               "        \"data\": \"AA\"\n"
               "      }\n"
               "    }\n"
               "  }\n"
               "}"),
        json_str);
    json_stream_gen_delete(&p_gen);
    ASSERT_TRUE(this->m_mem_alloc_trace.is_empty());
}

TEST_F(TestHttpJson, test_2) // NOLINT
{
    const time_t                     timestamp   = 1612358920;
    const mac_address_str_t          gw_mac_addr = { "AA:CC:EE:00:11:22" };
    const ruuvi_gw_cfg_coordinates_t coordinates = { "170.112233,59.445566" };
    const std::array<uint8_t, 1>     data1       = { 0xAAU };
    const std::array<uint8_t, 1>     data2       = { 0xBBU };

    adv_report_table_t adv_table = {
        .num_of_advs = 2,
        .table = {
            {
                .timestamp = 1612358929,
                .tag_mac = {0xaa, 0xbb, 0xcc, 0x01, 0x02, 0x03},
                .rssi = -70,
                .primary_phy = RE_CA_UART_BLE_PHY_1MBPS,
                .secondary_phy = RE_CA_UART_BLE_PHY_NOT_SET,
                .ch_index = 37,
                .is_coded_phy = false,
                .tx_power = RE_CA_UART_BLE_GAP_POWER_LEVEL_INVALID,
                .data_len = data1.size(),
            },
            {
                .timestamp = 1612358930,
                .tag_mac = {0xaa, 0xbb, 0xcc, 0x01, 0x02, 0x04},
                .rssi = -71,
                .primary_phy = RE_CA_UART_BLE_PHY_1MBPS,
                .secondary_phy = RE_CA_UART_BLE_PHY_NOT_SET,
                .ch_index = 38,
                .is_coded_phy = false,
                .tx_power = RE_CA_UART_BLE_GAP_POWER_LEVEL_INVALID,
                .data_len = data2.size(),
            },
        },
    };
    memcpy(adv_table.table[0].data_buf, data1.data(), data1.size());
    memcpy(adv_table.table[1].data_buf, data2.data(), data2.size());

    const bool     flag_raw_data       = true;
    const bool     flag_decode         = false;
    const bool     flag_use_timestamps = true;
    const bool     flag_use_nonce      = true;
    const uint32_t nonce               = 12345678;

    const http_json_create_stream_gen_advs_params_t params = {
        .flag_raw_data       = flag_raw_data,
        .flag_decode         = flag_decode,
        .flag_use_timestamps = flag_use_timestamps,
        .cur_time            = timestamp,
        .flag_use_nonce      = flag_use_nonce,
        .nonce               = nonce,
        .p_mac_addr          = &gw_mac_addr,
        .p_coordinates       = &coordinates,
    };

    json_stream_gen_t* p_gen = http_json_create_stream_gen_advs(&adv_table, &params);
    ASSERT_NE(nullptr, p_gen);

    string json_str("");
    while (true)
    {
        const char* p_chunk = json_stream_gen_get_next_chunk(p_gen);
        if (nullptr == p_chunk)
        {
            ASSERT_FALSE(nullptr == p_chunk);
        }

        if ('\0' == p_chunk[0])
        {
            break;
        }
        json_str += string(p_chunk);
    }

    ASSERT_EQ(
        string("{\n"
               "  \"data\": {\n"
               "    \"coordinates\": \"170.112233,59.445566\",\n"
               "    \"timestamp\": 1612358920,\n"
               "    \"nonce\": 12345678,\n"
               "    \"gw_mac\": \"AA:CC:EE:00:11:22\",\n"
               "    \"tags\": {\n"
               "      \"AA:BB:CC:01:02:03\": {\n"
               "        \"rssi\": -70,\n"
               "        \"timestamp\": 1612358929,\n"
               "        \"ble_phy\": \"1M\",\n"
               "        \"ble_chan\": 37,\n"
               "        \"data\": \"AA\"\n"
               "      },\n"
               "      \"AA:BB:CC:01:02:04\": {\n"
               "        \"rssi\": -71,\n"
               "        \"timestamp\": 1612358930,\n"
               "        \"ble_phy\": \"1M\",\n"
               "        \"ble_chan\": 38,\n"
               "        \"data\": \"BB\"\n"
               "      }\n"
               "    }\n"
               "  }\n"
               "}"),
        json_str);
    json_stream_gen_delete(&p_gen);
    ASSERT_TRUE(this->m_mem_alloc_trace.is_empty());
}

TEST_F(TestHttpJson, test_create_status_json_str_connection_wifi) // NOLINT
{
    const mac_address_str_t      nrf52_mac_addr         = { .str_buf = "AA:CC:EE:00:11:22" };
    const uint32_t               uptime                 = 123;
    const bool                   is_wifi                = true;
    const uint32_t               network_disconnect_cnt = 3;
    const uint32_t               nonce                  = 1234567;
    const std::array<uint8_t, 1> data                   = { 0xAAU };

    adv_report_table_t adv_table = {
        .num_of_advs = 4,
        .table = {
            {
                .timestamp = 1612358929,
                .samples_counter = 11,
                .tag_mac = {0xaa, 0xbb, 0xcc, 0x01, 0x02, 0x03},
                .rssi = -70,
                .primary_phy = RE_CA_UART_BLE_PHY_1MBPS,
                .secondary_phy = RE_CA_UART_BLE_PHY_NOT_SET,
                .ch_index = 37,
                .is_coded_phy = false,
                .tx_power = 8,
                .data_len = data.size(),
            },
            {
                .timestamp = 1612358928,
                .samples_counter = 10,
                .tag_mac = {0xaa, 0xbb, 0xcc, 0x01, 0x02, 0x04},
                .rssi = -70,
                .primary_phy = RE_CA_UART_BLE_PHY_1MBPS,
                .secondary_phy = RE_CA_UART_BLE_PHY_NOT_SET,
                .ch_index = 37,
                .is_coded_phy = false,
                .tx_power = 8,
                .data_len = data.size(),
            },
            {
                .timestamp = 1612358925,
                .samples_counter = 0,
                .tag_mac = {0xaa, 0xbb, 0xcc, 0x01, 0x02, 0x05},
                .rssi = -70,
                .primary_phy = RE_CA_UART_BLE_PHY_1MBPS,
                .secondary_phy = RE_CA_UART_BLE_PHY_NOT_SET,
                .ch_index = 37,
                .is_coded_phy = false,
                .tx_power = 8,
                .data_len = data.size(),
            },
            {
                .timestamp = 1612358924,
                .samples_counter = 0,
                .tag_mac = {0xaa, 0xbb, 0xcc, 0x01, 0x02, 0x06},
                .rssi = -70,
                .primary_phy = RE_CA_UART_BLE_PHY_1MBPS,
                .secondary_phy = RE_CA_UART_BLE_PHY_NOT_SET,
                .ch_index = 37,
                .is_coded_phy = false,
                .tx_power = 8,
                .data_len = data.size(),
            },
        },
    };
    memcpy(adv_table.table[0].data_buf, data.data(), data.size());
    const http_json_statistics_info_t stat_info = {
        .nrf52_mac_addr              = nrf52_mac_addr,
        .esp_fw                      = { "1.9.0" },
        .nrf_fw                      = { "0.7.1" },
        .uptime                      = uptime,
        .nonce                       = nonce,
        .nrf_status                  = true,
        .is_connected_to_wifi        = is_wifi,
        .network_disconnect_cnt      = network_disconnect_cnt,
        .nrf_self_reboot_cnt         = 3,
        .nrf_ext_hw_reset_cnt        = 2,
        .nrf_lost_ack_cnt            = 117,
        .total_free_bytes_internal   = 123456,
        .total_free_bytes_default    = 67890,
        .largest_free_block_internal = 97125,
        .largest_free_block_default  = 54321,
        .reset_reason                = { "POWER_ON" },
        .reset_cnt                   = 3,
        .p_reset_info                = "",
    };
    ASSERT_TRUE(http_json_create_status_str(&stat_info, &adv_table, &this->m_json_str));
    ASSERT_EQ(
        string("{\n"
               "\t\"DEVICE_ADDR\":\t\"AA:CC:EE:00:11:22\",\n"
               "\t\"ESP_FW\":\t\"1.9.0\",\n"
               "\t\"NRF_FW\":\t\"0.7.1\",\n"
               "\t\"NRF_STATUS\":\ttrue,\n"
               "\t\"UPTIME\":\t\"123\",\n"
               "\t\"NONCE\":\t\"1234567\",\n"
               "\t\"CONNECTION\":\t\"WIFI\",\n"
               "\t\"NUM_CONN_LOST\":\t\"3\",\n"
               "\t\"RESET_REASON\":\t\"POWER_ON\",\n"
               "\t\"RESET_CNT\":\t\"3\",\n"
               "\t\"RESET_INFO\":\t\"\",\n"
               "\t\"NRF_SELF_REBOOT_CNT\":\t\"3\",\n"
               "\t\"NRF_EXT_HW_RESET_CNT\":\t\"2\",\n"
               "\t\"NRF_LOST_ACK_CNT\":\t\"117\",\n"
               "\t\"TOTAL_FREE_BYTES_INTERNAL\":\t\"123456\",\n"
               "\t\"TOTAL_FREE_BYTES_DEFAULT\":\t\"67890\",\n"
               "\t\"LARGEST_FREE_BLOCK_INTERNAL\":\t\"97125\",\n"
               "\t\"LARGEST_FREE_BLOCK_DEFAULT\":\t\"54321\",\n"
               "\t\"SENSORS_SEEN\":\t\"2\",\n"
               "\t\"ACTIVE_SENSORS\":\t[{\n"
               "\t\t\t\"MAC\":\t\"AA:BB:CC:01:02:03\",\n"
               "\t\t\t\"COUNTER\":\t\"11\"\n"
               "\t\t}, {\n"
               "\t\t\t\"MAC\":\t\"AA:BB:CC:01:02:04\",\n"
               "\t\t\t\"COUNTER\":\t\"10\"\n"
               "\t\t}],\n"
               "\t\"INACTIVE_SENSORS\":\t[\"AA:BB:CC:01:02:05\", \"AA:BB:CC:01:02:06\"],\n"
               "\t\"TASKS\":\t[{\n"
               "\t\t\t\"TASK_NAME\":\t\"main\",\n"
               "\t\t\t\"MIN_FREE_STACK_SIZE\":\t1000\n"
               "\t\t}, {\n"
               "\t\t\t\"TASK_NAME\":\t\"IDLE0\",\n"
               "\t\t\t\"MIN_FREE_STACK_SIZE\":\t500\n"
               "\t\t}]\n"
               "}"),
        string(this->m_json_str.p_str));
    cjson_wrap_free_json_str(&this->m_json_str);
    ASSERT_EQ(97, this->m_malloc_cnt);
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

    adv_report_table_t adv_table = {
        .num_of_advs = 3,
        .table = {
            {
                .timestamp = 1612358930,
                .samples_counter = 12,
                .tag_mac = {0xab, 0xbb, 0xcc, 0x01, 0x02, 0xF3},
                .rssi = -69,
                .primary_phy = RE_CA_UART_BLE_PHY_1MBPS,
                .secondary_phy = RE_CA_UART_BLE_PHY_NOT_SET,
                .ch_index = 37,
                .is_coded_phy = false,
                .tx_power = 8,
                .data_len = data.size(),
            },
            {
                .timestamp = 1612358929,
                .samples_counter = 11,
                .tag_mac = {0xab, 0xbb, 0xcc, 0x01, 0x02, 0xF4},
                .rssi = -68,
                .primary_phy = RE_CA_UART_BLE_PHY_1MBPS,
                .secondary_phy = RE_CA_UART_BLE_PHY_NOT_SET,
                .ch_index = 37,
                .is_coded_phy = false,
                .tx_power = 8,
                .data_len = data.size(),
            },
            {
                .timestamp = 1612358926,
                .samples_counter = 0,
                .tag_mac = {0xab, 0xbb, 0xcc, 0x01, 0x02, 0xF5},
                .rssi = -67,
                .primary_phy = RE_CA_UART_BLE_PHY_1MBPS,
                .secondary_phy = RE_CA_UART_BLE_PHY_NOT_SET,
                .ch_index = 37,
                .is_coded_phy = false,
                .tx_power = 8,
                .data_len = data.size(),
            },
        },
    };
    memcpy(adv_table.table[0].data_buf, data.data(), data.size());
    const http_json_statistics_info_t stat_info = {
        .nrf52_mac_addr              = nrf52_mac_addr,
        .esp_fw                      = { "1.9.0" },
        .nrf_fw                      = { "0.7.1" },
        .uptime                      = uptime,
        .nonce                       = nonce,
        .nrf_status                  = false,
        .is_connected_to_wifi        = is_wifi,
        .network_disconnect_cnt      = network_disconnect_cnt,
        .nrf_self_reboot_cnt         = 3,
        .nrf_ext_hw_reset_cnt        = 2,
        .nrf_lost_ack_cnt            = 117,
        .total_free_bytes_internal   = 123456,
        .total_free_bytes_default    = 67890,
        .largest_free_block_internal = 97125,
        .largest_free_block_default  = 54321,
        .reset_reason                = { "TASK_WDT" },
        .reset_cnt                   = 4,
        .p_reset_info                = "main (active task: idle)",
    };
    ASSERT_TRUE(http_json_create_status_str(&stat_info, &adv_table, &this->m_json_str));
    ASSERT_EQ(
        string("{\n"
               "\t\"DEVICE_ADDR\":\t\"AB:CD:EF:01:12:23\",\n"
               "\t\"ESP_FW\":\t\"1.9.0\",\n"
               "\t\"NRF_FW\":\t\"0.7.1\",\n"
               "\t\"NRF_STATUS\":\tfalse,\n"
               "\t\"UPTIME\":\t\"124\",\n"
               "\t\"NONCE\":\t\"1234568\",\n"
               "\t\"CONNECTION\":\t\"ETHERNET\",\n"
               "\t\"NUM_CONN_LOST\":\t\"4\",\n"
               "\t\"RESET_REASON\":\t\"TASK_WDT\",\n"
               "\t\"RESET_CNT\":\t\"4\",\n"
               "\t\"RESET_INFO\":\t\"main (active task: idle)\",\n"
               "\t\"NRF_SELF_REBOOT_CNT\":\t\"3\",\n"
               "\t\"NRF_EXT_HW_RESET_CNT\":\t\"2\",\n"
               "\t\"NRF_LOST_ACK_CNT\":\t\"117\",\n"
               "\t\"TOTAL_FREE_BYTES_INTERNAL\":\t\"123456\",\n"
               "\t\"TOTAL_FREE_BYTES_DEFAULT\":\t\"67890\",\n"
               "\t\"LARGEST_FREE_BLOCK_INTERNAL\":\t\"97125\",\n"
               "\t\"LARGEST_FREE_BLOCK_DEFAULT\":\t\"54321\",\n"
               "\t\"SENSORS_SEEN\":\t\"2\",\n"
               "\t\"ACTIVE_SENSORS\":\t[{\n"
               "\t\t\t\"MAC\":\t\"AB:BB:CC:01:02:F3\",\n"
               "\t\t\t\"COUNTER\":\t\"12\"\n"
               "\t\t}, {\n"
               "\t\t\t\"MAC\":\t\"AB:BB:CC:01:02:F4\",\n"
               "\t\t\t\"COUNTER\":\t\"11\"\n"
               "\t\t}],\n"
               "\t\"INACTIVE_SENSORS\":\t[\"AB:BB:CC:01:02:F5\"],\n"
               "\t\"TASKS\":\t[{\n"
               "\t\t\t\"TASK_NAME\":\t\"main\",\n"
               "\t\t\t\"MIN_FREE_STACK_SIZE\":\t1000\n"
               "\t\t}, {\n"
               "\t\t\t\"TASK_NAME\":\t\"IDLE0\",\n"
               "\t\t\t\"MIN_FREE_STACK_SIZE\":\t500\n"
               "\t\t}]\n"
               "}"),
        string(this->m_json_str.p_str));
    cjson_wrap_free_json_str(&this->m_json_str);
    ASSERT_EQ(95, this->m_malloc_cnt);
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

    adv_report_table_t adv_table = {
        .num_of_advs = 4,
        .table = {
            {
                .timestamp = 1612358929,
                .samples_counter = 11,
                .tag_mac = {0xaa, 0xbb, 0xcc, 0x01, 0x02, 0x03},
                .rssi = -70,
                .primary_phy = RE_CA_UART_BLE_PHY_1MBPS,
                .secondary_phy = RE_CA_UART_BLE_PHY_NOT_SET,
                .ch_index = 37,
                .is_coded_phy = false,
                .tx_power = 8,
                .data_len = data.size(),
            },
            {
                .timestamp = 1612358928,
                .samples_counter = 10,
                .tag_mac = {0xaa, 0xbb, 0xcc, 0x01, 0x02, 0x04},
                .rssi = -70,
                .primary_phy = RE_CA_UART_BLE_PHY_1MBPS,
                .secondary_phy = RE_CA_UART_BLE_PHY_NOT_SET,
                .ch_index = 37,
                .is_coded_phy = false,
                .tx_power = 8,
                .data_len = data.size(),
            },
            {
                .timestamp = 1612358925,
                .samples_counter = 0,
                .tag_mac = {0xaa, 0xbb, 0xcc, 0x01, 0x02, 0x05},
                .rssi = -70,
                .primary_phy = RE_CA_UART_BLE_PHY_1MBPS,
                .secondary_phy = RE_CA_UART_BLE_PHY_NOT_SET,
                .ch_index = 37,
                .is_coded_phy = false,
                .tx_power = 8,
                .data_len = data.size(),
            },
            {
                .timestamp = 1612358924,
                .samples_counter = 0,
                .tag_mac = {0xaa, 0xbb, 0xcc, 0x01, 0x02, 0x06},
                .rssi = -70,
                .primary_phy = RE_CA_UART_BLE_PHY_1MBPS,
                .secondary_phy = RE_CA_UART_BLE_PHY_NOT_SET,
                .ch_index = 37,
                .is_coded_phy = false,
                .tx_power = 8,
                .data_len = data.size(),
            },
        },
    };
    memcpy(adv_table.table[0].data_buf, data.data(), data.size());

    const http_json_statistics_info_t stat_info = {
        .nrf52_mac_addr              = nrf52_mac_addr,
        .esp_fw                      = { "1.9.0" },
        .nrf_fw                      = { "0.7.1" },
        .uptime                      = uptime,
        .nonce                       = nonce,
        .nrf_status                  = true,
        .is_connected_to_wifi        = is_wifi,
        .network_disconnect_cnt      = network_disconnect_cnt,
        .nrf_self_reboot_cnt         = 3,
        .nrf_ext_hw_reset_cnt        = 2,
        .nrf_lost_ack_cnt            = 117,
        .total_free_bytes_internal   = 123456,
        .total_free_bytes_default    = 67890,
        .largest_free_block_internal = 97125,
        .largest_free_block_default  = 54321,
        .reset_reason                = { "SW" },
        .reset_cnt                   = 3,
        .p_reset_info                = "",
    };

    for (uint32_t i = 1; i < 97; ++i)
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
        this->m_malloc_fail_on_cnt = 98;
        this->m_malloc_cnt         = 0;
        ASSERT_TRUE(http_json_create_status_str(&stat_info, &adv_table, &this->m_json_str));
        ASSERT_NE(nullptr, this->m_json_str.p_str);
        cjson_wrap_free_json_str(&this->m_json_str);
        ASSERT_EQ(97, this->m_malloc_cnt);
        ASSERT_TRUE(this->m_mem_alloc_trace.is_empty());
    }
}
