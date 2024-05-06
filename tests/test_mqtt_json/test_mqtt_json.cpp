/**
 * @file test_mqtt_json.cpp
 * @author TheSomeMan
 * @date 2020-02-04
 * @copyright Ruuvi Innovations Ltd, license BSD-3-Clause.
 */

#include "mqtt_json.h"
#include <cstring>
#include "gtest/gtest.h"
#include "os_malloc.h"
#include "str_buf.h"

using namespace std;

/*** Google-test class implementation
 * *********************************************************************************/

class TestMqttJson;
static TestMqttJson* g_pTestClass;

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

class TestMqttJson : public ::testing::Test
{
private:
protected:
    void
    SetUp() override
    {
        g_pTestClass     = this;
        this->m_json_str = str_buf_init_null();

        this->m_malloc_cnt         = 0;
        this->m_malloc_fail_on_cnt = 0;
    }

    void
    TearDown() override
    {
        g_pTestClass = nullptr;
        if (nullptr != this->m_json_str.buf)
        {
            str_buf_free_buf(&this->m_json_str);
        }
    }

public:
    TestMqttJson();

    ~TestMqttJson() override;

    MemAllocTrace m_mem_alloc_trace;
    uint32_t      m_malloc_cnt;
    uint32_t      m_malloc_fail_on_cnt;
    str_buf_t     m_json_str;
};

TestMqttJson::TestMqttJson()
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
    void* p_mem = malloc(size);
    assert(nullptr != p_mem);
    g_pTestClass->m_mem_alloc_trace.add(p_mem);
    return p_mem;
}

void
os_free_internal(void* p_mem)
{
    g_pTestClass->m_mem_alloc_trace.remove(p_mem);
    free(p_mem);
}

void*
os_calloc(const size_t nmemb, const size_t size)
{
    if (++g_pTestClass->m_malloc_cnt == g_pTestClass->m_malloc_fail_on_cnt)
    {
        return nullptr;
    }
    void* p_mem = calloc(nmemb, size);
    assert(nullptr != p_mem);
    g_pTestClass->m_mem_alloc_trace.add(p_mem);
    return p_mem;
}

} // extern "C"

TestMqttJson::~TestMqttJson() = default;

/*** Unit-Tests
 * *******************************************************************************************************/

TEST_F(TestMqttJson, test_1) // NOLINT
{
    const json_stream_gen_size_t max_chunk_size = 1024U;
    const time_t                 timestamp      = 1612358920;
    const mac_address_str_t      gw_mac_addr    = { .str_buf = "AA:CC:EE:00:11:22" };
    const char*                  p_coordinates  = "170.112233,59.445566";
    const std::array<uint8_t, 1> data           = { 0xAAU };

    adv_report_t adv_report = {
        .timestamp = 1612358929,
        .tag_mac   = { 0xaa, 0xbb, 0xcc, 0x01, 0x02, 0x03 },
        .rssi      = -70,
        .data_len  = data.size(),
    };
    memcpy(adv_report.data_buf, data.data(), data.size());

    this->m_json_str = mqtt_create_json_str(
        &adv_report,
        true,
        timestamp,
        &gw_mac_addr,
        p_coordinates,
        GW_CFG_MQTT_DATA_FORMAT_RUUVI_RAW,
        max_chunk_size);
    ASSERT_NE(nullptr, this->m_json_str.buf);

    ASSERT_EQ(
        string("{"
               "\"gw_mac\":\"AA:CC:EE:00:11:22\","
               "\"rssi\":-70,"
               "\"aoa\":[],"
               "\"gwts\":1612358920,"
               "\"ts\":1612358929,"
               "\"data\":\"AA\","
               "\"coords\":\"170.112233,59.445566\""
               "}"),
        string(this->m_json_str.buf));
    str_buf_free_buf(&this->m_json_str);
    ASSERT_EQ(2, this->m_malloc_cnt);
    ASSERT_TRUE(this->m_mem_alloc_trace.is_empty());
}

TEST_F(TestMqttJson, test_insufficient_chunk_size) // NOLINT
{
    const json_stream_gen_size_t max_chunk_size = 50U;
    const time_t                 timestamp      = 1612358920;
    const mac_address_str_t      gw_mac_addr    = { .str_buf = "AA:CC:EE:00:11:22" };
    const char*                  p_coordinates  = "170.112233,59.445566";
    const std::array<uint8_t, 1> data           = { 0xAAU };

    adv_report_t adv_report = {
        .timestamp = 1612358929,
        .tag_mac   = { 0xaa, 0xbb, 0xcc, 0x01, 0x02, 0x03 },
        .rssi      = -70,
        .data_len  = data.size(),
    };
    memcpy(adv_report.data_buf, data.data(), data.size());

    this->m_json_str = mqtt_create_json_str(
        &adv_report,
        true,
        timestamp,
        &gw_mac_addr,
        p_coordinates,
        GW_CFG_MQTT_DATA_FORMAT_RUUVI_RAW,
        max_chunk_size);
    ASSERT_EQ(nullptr, this->m_json_str.buf);

    ASSERT_EQ(2, this->m_malloc_cnt);
    ASSERT_TRUE(this->m_mem_alloc_trace.is_empty());
}

TEST_F(TestMqttJson, test_1_without_timestamps) // NOLINT
{
    const json_stream_gen_size_t max_chunk_size = 1024U;
    const time_t                 timestamp      = 1612358920;
    const mac_address_str_t      gw_mac_addr    = { .str_buf = "AA:CC:EE:00:11:22" };
    const char*                  p_coordinates  = "170.112233,59.445566";
    const std::array<uint8_t, 1> data           = { 0xAAU };

    adv_report_t adv_report = {
        .timestamp = 1011,
        .tag_mac   = { 0xaa, 0xbb, 0xcc, 0x01, 0x02, 0x03 },
        .rssi      = -70,
        .data_len  = data.size(),
    };
    memcpy(adv_report.data_buf, data.data(), data.size());

    this->m_json_str = mqtt_create_json_str(
        &adv_report,
        false,
        timestamp,
        &gw_mac_addr,
        p_coordinates,
        GW_CFG_MQTT_DATA_FORMAT_RUUVI_RAW,
        max_chunk_size);
    ASSERT_NE(nullptr, this->m_json_str.buf);

    ASSERT_EQ(
        string("{"
               "\"gw_mac\":\"AA:CC:EE:00:11:22\","
               "\"rssi\":-70,"
               "\"aoa\":[],"
               "\"cnt\":1011,"
               "\"data\":\"AA\","
               "\"coords\":\"170.112233,59.445566\""
               "}"),
        string(this->m_json_str.buf));
    str_buf_free_buf(&this->m_json_str);
    ASSERT_EQ(2, this->m_malloc_cnt);
    ASSERT_TRUE(this->m_mem_alloc_trace.is_empty());
}

TEST_F(TestMqttJson, test_raw_and_decoded_df5) // NOLINT
{
    const json_stream_gen_size_t  max_chunk_size = 1024U;
    const time_t                  timestamp      = 1612358920;
    const mac_address_str_t       gw_mac_addr    = { .str_buf = "AA:CC:EE:00:11:22" };
    const char*                   p_coordinates  = "170.112233,59.445566";
    const std::array<uint8_t, 31> data           = {
                  0x02U, 0x01U, 0x06U, 0x1BU, 0xFFU, 0x99U, 0x04U, 0x05U, 0x17U, 0x03U, 0x77U, 0x58U, 0xC5U, 0xC7U, 0x03U, 0xFCU,
                  0xFFU, 0x30U, 0xFFU, 0xF8U, 0x94U, 0x16U, 0xB7U, 0x95U, 0x8AU, 0xE3U, 0x75U, 0xCFU, 0x37U, 0x4EU, 0x23U,
    };

    adv_report_t adv_report = {
        .timestamp = 1612358929,
        .tag_mac   = { 0xaa, 0xbb, 0xcc, 0x01, 0x02, 0x03 },
        .rssi      = -70,
        .data_len  = data.size(),
    };
    memcpy(adv_report.data_buf, data.data(), data.size());

    this->m_json_str = mqtt_create_json_str(
        &adv_report,
        true,
        timestamp,
        &gw_mac_addr,
        p_coordinates,
        GW_CFG_MQTT_DATA_FORMAT_RUUVI_RAW_AND_DECODED,
        max_chunk_size);
    ASSERT_NE(nullptr, this->m_json_str.buf);

    ASSERT_EQ(
        string("{"
               "\"gw_mac\":\"AA:CC:EE:00:11:22\","
               "\"rssi\":-70,"
               "\"aoa\":[],"
               "\"gwts\":1612358920,"
               "\"ts\":1612358929,"
               "\"data\":\"0201061BFF99040517037758C5C703FCFF30FFF89416B7958AE375CF374E23\","
               "\"dataFormat\":5,"
               "\"temperature\":29.455,"
               "\"humidity\":76.3800,"
               "\"pressure\":100631,"
               "\"accelX\":1.020,"
               "\"accelY\":-0.208,"
               "\"accelZ\":-0.008,"
               "\"movementCounter\":183,"
               "\"voltage\":2.784,"
               "\"txPower\":4,"
               "\"measurementSequenceNumber\":38282,"
               "\"id\":\"E3:75:CF:37:4E:23\","
               "\"coords\":\"170.112233,59.445566\""
               "}"),
        string(this->m_json_str.buf));
    str_buf_free_buf(&this->m_json_str);
    ASSERT_EQ(2, this->m_malloc_cnt);
    ASSERT_TRUE(this->m_mem_alloc_trace.is_empty());
}

TEST_F(TestMqttJson, test_decoded_df5) // NOLINT
{
    const json_stream_gen_size_t  max_chunk_size = 1024U;
    const time_t                  timestamp      = 1612358920;
    const mac_address_str_t       gw_mac_addr    = { .str_buf = "AA:CC:EE:00:11:22" };
    const char*                   p_coordinates  = "170.112233,59.445566";
    const std::array<uint8_t, 31> data           = {
                  0x02U, 0x01U, 0x06U, 0x1BU, 0xFFU, 0x99U, 0x04U, 0x05U, 0x17U, 0x03U, 0x77U, 0x58U, 0xC5U, 0xC7U, 0x03U, 0xFCU,
                  0xFFU, 0x30U, 0xFFU, 0xF8U, 0x94U, 0x16U, 0xB7U, 0x95U, 0x8AU, 0xE3U, 0x75U, 0xCFU, 0x37U, 0x4EU, 0x23U,
    };

    adv_report_t adv_report = {
        .timestamp = 1612358929,
        .tag_mac   = { 0xaa, 0xbb, 0xcc, 0x01, 0x02, 0x03 },
        .rssi      = -70,
        .data_len  = data.size(),
    };
    memcpy(adv_report.data_buf, data.data(), data.size());

    this->m_json_str = mqtt_create_json_str(
        &adv_report,
        true,
        timestamp,
        &gw_mac_addr,
        p_coordinates,
        GW_CFG_MQTT_DATA_FORMAT_RUUVI_DECODED,
        max_chunk_size);
    ASSERT_NE(nullptr, this->m_json_str.buf);

    ASSERT_EQ(
        string("{"
               "\"gw_mac\":\"AA:CC:EE:00:11:22\","
               "\"rssi\":-70,"
               "\"aoa\":[],"
               "\"gwts\":1612358920,"
               "\"ts\":1612358929,"
               "\"dataFormat\":5,"
               "\"temperature\":29.455,"
               "\"humidity\":76.3800,"
               "\"pressure\":100631,"
               "\"accelX\":1.020,"
               "\"accelY\":-0.208,"
               "\"accelZ\":-0.008,"
               "\"movementCounter\":183,"
               "\"voltage\":2.784,"
               "\"txPower\":4,"
               "\"measurementSequenceNumber\":38282,"
               "\"id\":\"E3:75:CF:37:4E:23\","
               "\"coords\":\"170.112233,59.445566\""
               "}"),
        string(this->m_json_str.buf));
    str_buf_free_buf(&this->m_json_str);
    ASSERT_EQ(2, this->m_malloc_cnt);
    ASSERT_TRUE(this->m_mem_alloc_trace.is_empty());
}

TEST_F(TestMqttJson, test_raw_and_decoded_df6) // NOLINT
{
    const json_stream_gen_size_t  max_chunk_size = 1024U;
    const time_t                  timestamp      = 1612358920;
    const mac_address_str_t       gw_mac_addr    = { .str_buf = "AA:CC:EE:00:11:22" };
    const char*                   p_coordinates  = "170.112233,59.445566";
    const std::array<uint8_t, 31> data           = {
                  0x02U, 0x01U, 0x06U, 0x1BU, 0xFFU, 0x99U, 0x04U, 0x06U, 0x00U, 0x1CU, 0x00U, 0x1DU, 0x00U, 0x1DU, 0x00U, 0x1DU,
                  0x03U, 0x26U, 0x8CU, 0xCEU, 0x80U, 0x11U, 0x05U, 0x14U, 0x22U, 0x94U, 0xB9U, 0x7EU, 0x4EU, 0xB2U, 0x72U,
    };

    adv_report_t adv_report = {
        .timestamp = 1612358929,
        .tag_mac   = { 0xaa, 0xbb, 0xcc, 0x01, 0x02, 0x03 },
        .rssi      = -70,
        .data_len  = data.size(),
    };
    memcpy(adv_report.data_buf, data.data(), data.size());

    this->m_json_str = mqtt_create_json_str(
        &adv_report,
        true,
        timestamp,
        &gw_mac_addr,
        p_coordinates,
        GW_CFG_MQTT_DATA_FORMAT_RUUVI_RAW_AND_DECODED,
        max_chunk_size);
    ASSERT_NE(nullptr, this->m_json_str.buf);

    ASSERT_EQ(
        string("{"
               "\"gw_mac\":\"AA:CC:EE:00:11:22\","
               "\"rssi\":-70,"
               "\"aoa\":[],"
               "\"gwts\":1612358920,"
               "\"ts\":1612358929,"
               "\"data\":\"0201061BFF990406001C001D001D001D03268CCE801105142294B97E4EB272\","
               "\"dataFormat\":6,"
               "\"temperature\":26.1,"
               "\"humidity\":56.3,"
               "\"PM1.0\":2.8,"
               "\"PM2.5\":2.9,"
               "\"PM4.0\":2.9,"
               "\"PM10.0\":2.9,"
               "\"CO2\":806,"
               "\"VOC\":116,"
               "\"NOx\":1,"
               "\"measurementSequenceNumber\":5154,"
               "\"id\":\"94:B9:7E:4E:B2:72\","
               "\"coords\":\"170.112233,59.445566\""
               "}"),
        string(this->m_json_str.buf));
    str_buf_free_buf(&this->m_json_str);
    ASSERT_EQ(2, this->m_malloc_cnt);
    ASSERT_TRUE(this->m_mem_alloc_trace.is_empty());
}

TEST_F(TestMqttJson, test_decoded_df6) // NOLINT
{
    const json_stream_gen_size_t  max_chunk_size = 1024U;
    const time_t                  timestamp      = 1612358920;
    const mac_address_str_t       gw_mac_addr    = { .str_buf = "AA:CC:EE:00:11:22" };
    const char*                   p_coordinates  = "170.112233,59.445566";
    const std::array<uint8_t, 31> data           = {
                  0x02U, 0x01U, 0x06U, 0x1BU, 0xFFU, 0x99U, 0x04U, 0x06U, 0x00U, 0x1CU, 0x00U, 0x1DU, 0x00U, 0x1DU, 0x00U, 0x1DU,
                  0x03U, 0x26U, 0x8CU, 0xCEU, 0x80U, 0x11U, 0x05U, 0x14U, 0x22U, 0x94U, 0xB9U, 0x7EU, 0x4EU, 0xB2U, 0x72U,
    };

    adv_report_t adv_report = {
        .timestamp = 1612358929,
        .tag_mac   = { 0xaa, 0xbb, 0xcc, 0x01, 0x02, 0x03 },
        .rssi      = -70,
        .data_len  = data.size(),
    };
    memcpy(adv_report.data_buf, data.data(), data.size());

    this->m_json_str = mqtt_create_json_str(
        &adv_report,
        true,
        timestamp,
        &gw_mac_addr,
        p_coordinates,
        GW_CFG_MQTT_DATA_FORMAT_RUUVI_DECODED,
        max_chunk_size);
    ASSERT_NE(nullptr, this->m_json_str.buf);

    ASSERT_EQ(
        string("{"
               "\"gw_mac\":\"AA:CC:EE:00:11:22\","
               "\"rssi\":-70,"
               "\"aoa\":[],"
               "\"gwts\":1612358920,"
               "\"ts\":1612358929,"
               "\"dataFormat\":6,"
               "\"temperature\":26.1,"
               "\"humidity\":56.3,"
               "\"PM1.0\":2.8,"
               "\"PM2.5\":2.9,"
               "\"PM4.0\":2.9,"
               "\"PM10.0\":2.9,"
               "\"CO2\":806,"
               "\"VOC\":116,"
               "\"NOx\":1,"
               "\"measurementSequenceNumber\":5154,"
               "\"id\":\"94:B9:7E:4E:B2:72\","
               "\"coords\":\"170.112233,59.445566\""
               "}"),
        string(this->m_json_str.buf));
    str_buf_free_buf(&this->m_json_str);
    ASSERT_EQ(2, this->m_malloc_cnt);
    ASSERT_TRUE(this->m_mem_alloc_trace.is_empty());
}
