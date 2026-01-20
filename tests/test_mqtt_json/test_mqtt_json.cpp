/**
 * @file test_mqtt_json.cpp
 * @author TheSomeMan
 * @date 2020-02-04
 * @copyright Ruuvi Innovations Ltd, license BSD-3-Clause.
 */

#include "mqtt_json.h"
#include <cstring>
#include <vector>
#include <array>
#include <string>
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

TEST_F(TestMqttJson, test_raw_df6) // NOLINT
{
    const json_stream_gen_size_t  max_chunk_size = 1024U;
    const time_t                  timestamp      = 1612358920;
    const mac_address_str_t       gw_mac_addr    = { .str_buf = "AA:CC:EE:00:11:22" };
    const char*                   p_coordinates  = "170.112233,59.445566";
    const std::array<uint8_t, 31> data           = {
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
               "\"data\":\"020106030398FC17FF990406146457F8C79D001D03263A01146B05D04EB272\","
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
               "\"data\":\"020106030398FC17FF990406146457F8C79D001D03263A01146B05D04EB272\","
               "\"dataFormat\":6,"
               "\"temperature\":26.1,"
               "\"humidity\":56.3,"
               "\"pressure\":101101,"
               "\"PM2.5\":2.9,"
               "\"CO2\":806,"
               "\"VOC\":117,"
               "\"NOx\":3,"
               "\"luminosity\":1,"
               "\"sound_avg_dba\":61.0,"
               "\"measurementSequenceNumber\":5,"
               "\"flag_calibration_in_progress\":false,"
               "\"flag_button_pressed\":false,"
               "\"flag_rtc_running_on_boot\":false,"
               "\"id\":\"4E:B2:72\","
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
               "\"pressure\":101101,"
               "\"PM2.5\":2.9,"
               "\"CO2\":806,"
               "\"VOC\":117,"
               "\"NOx\":3,"
               "\"luminosity\":1,"
               "\"sound_avg_dba\":61.0,"
               "\"measurementSequenceNumber\":5,"
               "\"flag_calibration_in_progress\":false,"
               "\"flag_button_pressed\":false,"
               "\"flag_rtc_running_on_boot\":false,"
               "\"id\":\"4E:B2:72\","
               "\"coords\":\"170.112233,59.445566\""
               "}"),
        string(this->m_json_str.buf));
    str_buf_free_buf(&this->m_json_str);
    ASSERT_EQ(2, this->m_malloc_cnt);
    ASSERT_TRUE(this->m_mem_alloc_trace.is_empty());
}

TEST_F(TestMqttJson, test_raw_df7) // NOLINT
{
    const json_stream_gen_size_t  max_chunk_size = 1024U;
    const time_t                  timestamp      = 1612358920;
    const mac_address_str_t       gw_mac_addr    = { .str_buf = "AA:CC:EE:00:11:22" };
    const char*                   p_coordinates  = "170.112233,59.445566";
    const std::array<uint8_t, 27> data           = {
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
               "\"data\":\"02010617FF9904070503146457F8C79D1BE5000100AA050F4EB272\","
               "\"coords\":\"170.112233,59.445566\""
               "}"),
        string(this->m_json_str.buf));
    str_buf_free_buf(&this->m_json_str);
    ASSERT_EQ(2, this->m_malloc_cnt);
    ASSERT_TRUE(this->m_mem_alloc_trace.is_empty());
}

TEST_F(TestMqttJson, test_raw_and_decoded_df7) // NOLINT
{
    const json_stream_gen_size_t  max_chunk_size = 1024U;
    const time_t                  timestamp      = 1612358920;
    const mac_address_str_t       gw_mac_addr    = { .str_buf = "AA:CC:EE:00:11:22" };
    const char*                   p_coordinates  = "170.112233,59.445566";
    const std::array<uint8_t, 27> data           = {
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
               "\"data\":\"02010617FF9904070503146457F8C79D1BE5000100AA050F4EB272\","
               "\"dataFormat\":7,"
               "\"temperature\":26.1,"
               "\"humidity\":56.3,"
               "\"pressure\":101101,"
               "\"tiltX\":19.3,"
               "\"tiltY\":-19.3,"
               "\"luminosity\":1,"
               "\"colorTemperature\":1000,"
               "\"battery\":3.086,"
               "\"motionIntensity\":10,"
               "\"motionCount\":5,"
               "\"measurementSequenceNumber\":5,"
               "\"motionDetected\":true,"
               "\"presenceDetected\":true,"
               "\"id\":\"4E:B2:72\","
               "\"coords\":\"170.112233,59.445566\""
               "}"),
        string(this->m_json_str.buf));
    str_buf_free_buf(&this->m_json_str);
    ASSERT_EQ(2, this->m_malloc_cnt);
    ASSERT_TRUE(this->m_mem_alloc_trace.is_empty());
}

TEST_F(TestMqttJson, test_decoded_df7) // NOLINT
{
    const json_stream_gen_size_t  max_chunk_size = 1024U;
    const time_t                  timestamp      = 1612358920;
    const mac_address_str_t       gw_mac_addr    = { .str_buf = "AA:CC:EE:00:11:22" };
    const char*                   p_coordinates  = "170.112233,59.445566";
    const std::array<uint8_t, 27> data           = {
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
               "\"dataFormat\":7,"
               "\"temperature\":26.1,"
               "\"humidity\":56.3,"
               "\"pressure\":101101,"
               "\"tiltX\":19.3,"
               "\"tiltY\":-19.3,"
               "\"luminosity\":1,"
               "\"colorTemperature\":1000,"
               "\"battery\":3.086,"
               "\"motionIntensity\":10,"
               "\"motionCount\":5,"
               "\"measurementSequenceNumber\":5,"
               "\"motionDetected\":true,"
               "\"presenceDetected\":true,"
               "\"id\":\"4E:B2:72\","
               "\"coords\":\"170.112233,59.445566\""
               "}"),
        string(this->m_json_str.buf));
    str_buf_free_buf(&this->m_json_str);
    ASSERT_EQ(2, this->m_malloc_cnt);
    ASSERT_TRUE(this->m_mem_alloc_trace.is_empty());
}

TEST_F(TestMqttJson, test_raw_df_e1) // NOLINT
{
    const json_stream_gen_size_t  max_chunk_size = 1024U;
    const time_t                  timestamp      = 1612358920;
    const mac_address_str_t       gw_mac_addr    = { .str_buf = "AA:CC:EE:00:11:22" };
    const char*                   p_coordinates  = "170.112233,59.445566";
    const std::array<uint8_t, 48> data           = {
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
               "\"data\":"
               "\"2BFF9904E1161E526CC68B006E007200730074037610010C1D901E5CB6121314F8FFFFFFFFFFE375CF374E23030398FC\","
               "\"coords\":\"170.112233,59.445566\""
               "}"),
        string(this->m_json_str.buf));
    str_buf_free_buf(&this->m_json_str);
    ASSERT_EQ(2, this->m_malloc_cnt);
    ASSERT_TRUE(this->m_mem_alloc_trace.is_empty());
}

TEST_F(TestMqttJson, test_raw_and_decoded_df_e1) // NOLINT
{
    const json_stream_gen_size_t  max_chunk_size = 1024U;
    const time_t                  timestamp      = 1612358920;
    const mac_address_str_t       gw_mac_addr    = { .str_buf = "AA:CC:EE:00:11:22" };
    const char*                   p_coordinates  = "170.112233,59.445566";
    const std::array<uint8_t, 48> data           = {
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
               "\"data\":"
               "\"2BFF9904E1161E526CC68B006E007200730074037610010C1D901E5CB6121314F8FFFFFFFFFFE375CF374E23030398FC\","
               "\"dataFormat\":225,"
               "\"temperature\":28.310,"
               "\"humidity\":52.75,"
               "\"pressure\":100827,"
               "\"PM1.0\":11.0,"
               "\"PM2.5\":11.4,"
               "\"PM4.0\":11.5,"
               "\"PM10.0\":11.6,"
               "\"CO2\":886,"
               "\"VOC\":33,"
               "\"NOx\":3,"
               "\"luminosity\":7940,"
               "\"sound_inst_dba\":30.2,"
               "\"sound_avg_dba\":55.0,"
               "\"sound_peak_spl_db\":91.0,"
               "\"measurementSequenceNumber\":1184532,"
               "\"flag_calibration_in_progress\":false,"
               "\"flag_button_pressed\":false,"
               "\"flag_rtc_running_on_boot\":false,"
               "\"id\":\"E3:75:CF:37:4E:23\","
               "\"coords\":\"170.112233,59.445566\""
               "}"),
        string(this->m_json_str.buf));
    str_buf_free_buf(&this->m_json_str);
    ASSERT_EQ(2, this->m_malloc_cnt);
    ASSERT_TRUE(this->m_mem_alloc_trace.is_empty());
}

TEST_F(TestMqttJson, test_decoded_df_e1) // NOLINT
{
    const json_stream_gen_size_t  max_chunk_size = 1024U;
    const time_t                  timestamp      = 1612358920;
    const mac_address_str_t       gw_mac_addr    = { .str_buf = "AA:CC:EE:00:11:22" };
    const char*                   p_coordinates  = "170.112233,59.445566";
    const std::array<uint8_t, 48> data           = {
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
               "\"dataFormat\":225,"
               "\"temperature\":28.310,"
               "\"humidity\":52.75,"
               "\"pressure\":100827,"
               "\"PM1.0\":11.0,"
               "\"PM2.5\":11.4,"
               "\"PM4.0\":11.5,"
               "\"PM10.0\":11.6,"
               "\"CO2\":886,"
               "\"VOC\":33,"
               "\"NOx\":3,"
               "\"luminosity\":7940,"
               "\"sound_inst_dba\":30.2,"
               "\"sound_avg_dba\":55.0,"
               "\"sound_peak_spl_db\":91.0,"
               "\"measurementSequenceNumber\":1184532,"
               "\"flag_calibration_in_progress\":false,"
               "\"flag_button_pressed\":false,"
               "\"flag_rtc_running_on_boot\":false,"
               "\"id\":\"E3:75:CF:37:4E:23\","
               "\"coords\":\"170.112233,59.445566\""
               "}"),
        string(this->m_json_str.buf));
    str_buf_free_buf(&this->m_json_str);
    ASSERT_EQ(2, this->m_malloc_cnt);
    ASSERT_TRUE(this->m_mem_alloc_trace.is_empty());
}
