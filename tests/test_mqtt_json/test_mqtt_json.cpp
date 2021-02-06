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

using namespace std;

/*** Google-test class implementation
 * *********************************************************************************/

class MqttJson;
static MqttJson *g_pTestClass;

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
};

class MqttJson : public ::testing::Test
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
    MqttJson();

    ~MqttJson() override;

    MemAllocTrace    m_mem_alloc_trace;
    uint32_t         m_malloc_cnt;
    uint32_t         m_malloc_fail_on_cnt;
    cjson_wrap_str_t m_json_str;
};

MqttJson::MqttJson()
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
    void *p_mem = malloc(size);
    assert(nullptr != p_mem);
    g_pTestClass->m_mem_alloc_trace.add(p_mem);
    return p_mem;
}

void
os_free_internal(void *p_mem)
{
    g_pTestClass->m_mem_alloc_trace.remove(p_mem);
    free(p_mem);
}

void *
os_calloc(const size_t nmemb, const size_t size)
{
    if (++g_pTestClass->m_malloc_cnt == g_pTestClass->m_malloc_fail_on_cnt)
    {
        return nullptr;
    }
    void *p_mem = calloc(nmemb, size);
    assert(nullptr != p_mem);
    g_pTestClass->m_mem_alloc_trace.add(p_mem);
    return p_mem;
}

} // extern "C"

MqttJson::~MqttJson() = default;

/*** Unit-Tests
 * *******************************************************************************************************/

TEST_F(MqttJson, test_1) // NOLINT
{
    const time_t            timestamp     = 1612358920;
    const mac_address_str_t gw_mac_addr   = { .str_buf = "AA:CC:EE:00:11:22" };
    const char *            p_coordinates = "170.112233,59.445566";
    const adv_report_t      adv_report    = {
        .tag_mac   = { 0xaa, 0xbb, 0xcc, 0x01, 0x02, 0x03 },
        .timestamp = 1612358929,
        .rssi      = -70,
        .data      = { 'a', 'a', '\0' },
    };
    ASSERT_TRUE(mqtt_create_json_str(&adv_report, timestamp, &gw_mac_addr, p_coordinates, &this->m_json_str));
    ASSERT_EQ(
        string("{\n"
               "\t\"gw_mac\":\t\"AA:CC:EE:00:11:22\",\n"
               "\t\"rssi\":\t-70,\n"
               "\t\"aoa\":\t[],\n"
               "\t\"gwts\":\t\"1612358920\",\n"
               "\t\"ts\":\t\"1612358929\",\n"
               "\t\"data\":\t\"aa\",\n"
               "\t\"coords\":\t\"170.112233,59.445566\"\n"
               "}"),
        string(this->m_json_str.p_str));
    cjson_wrap_free_json_str(&this->m_json_str);
    ASSERT_EQ(22, this->m_malloc_cnt);
    ASSERT_TRUE(this->m_mem_alloc_trace.is_empty());
}

TEST_F(MqttJson, test_malloc_failed_1_of_22) // NOLINT
{
    const time_t            timestamp     = 1612358920;
    const mac_address_str_t gw_mac_addr   = { .str_buf = "AA:CC:EE:00:11:22" };
    const char *            p_coordinates = "170.112233,59.445566";
    const adv_report_t      adv_report    = {
        .tag_mac   = { 0xaa, 0xbb, 0xcc, 0x01, 0x02, 0x03 },
        .timestamp = 1612358929,
        .rssi      = -70,
        .data      = { 'a', 'a', '\0' },
    };

    this->m_malloc_fail_on_cnt = 1;
    ASSERT_FALSE(mqtt_create_json_str(&adv_report, timestamp, &gw_mac_addr, p_coordinates, &this->m_json_str));
    ASSERT_EQ(nullptr, this->m_json_str.p_str);
    ASSERT_TRUE(this->m_mem_alloc_trace.is_empty());
}

TEST_F(MqttJson, test_malloc_failed_2_of_22) // NOLINT
{
    const time_t            timestamp     = 1612358920;
    const mac_address_str_t gw_mac_addr   = { .str_buf = "AA:CC:EE:00:11:22" };
    const char *            p_coordinates = "170.112233,59.445566";
    const adv_report_t      adv_report    = {
        .tag_mac   = { 0xaa, 0xbb, 0xcc, 0x01, 0x02, 0x03 },
        .timestamp = 1612358929,
        .rssi      = -70,
        .data      = { 'a', 'a', '\0' },
    };

    this->m_malloc_fail_on_cnt = 2;
    ASSERT_FALSE(mqtt_create_json_str(&adv_report, timestamp, &gw_mac_addr, p_coordinates, &this->m_json_str));
    ASSERT_EQ(nullptr, this->m_json_str.p_str);
    ASSERT_TRUE(this->m_mem_alloc_trace.is_empty());
}

TEST_F(MqttJson, test_malloc_failed_3_of_22) // NOLINT
{
    const time_t            timestamp     = 1612358920;
    const mac_address_str_t gw_mac_addr   = { .str_buf = "AA:CC:EE:00:11:22" };
    const char *            p_coordinates = "170.112233,59.445566";
    const adv_report_t      adv_report    = {
        .tag_mac   = { 0xaa, 0xbb, 0xcc, 0x01, 0x02, 0x03 },
        .timestamp = 1612358929,
        .rssi      = -70,
        .data      = { 'a', 'a', '\0' },
    };

    this->m_malloc_fail_on_cnt = 3;
    ASSERT_FALSE(mqtt_create_json_str(&adv_report, timestamp, &gw_mac_addr, p_coordinates, &this->m_json_str));
    ASSERT_EQ(nullptr, this->m_json_str.p_str);
    ASSERT_TRUE(this->m_mem_alloc_trace.is_empty());
}

TEST_F(MqttJson, test_malloc_failed_4_of_22) // NOLINT
{
    const time_t            timestamp     = 1612358920;
    const mac_address_str_t gw_mac_addr   = { .str_buf = "AA:CC:EE:00:11:22" };
    const char *            p_coordinates = "170.112233,59.445566";
    const adv_report_t      adv_report    = {
        .tag_mac   = { 0xaa, 0xbb, 0xcc, 0x01, 0x02, 0x03 },
        .timestamp = 1612358929,
        .rssi      = -70,
        .data      = { 'a', 'a', '\0' },
    };

    this->m_malloc_fail_on_cnt = 4;
    ASSERT_FALSE(mqtt_create_json_str(&adv_report, timestamp, &gw_mac_addr, p_coordinates, &this->m_json_str));
    ASSERT_EQ(nullptr, this->m_json_str.p_str);
    ASSERT_TRUE(this->m_mem_alloc_trace.is_empty());
}

TEST_F(MqttJson, test_malloc_failed_5_of_22) // NOLINT
{
    const time_t            timestamp     = 1612358920;
    const mac_address_str_t gw_mac_addr   = { .str_buf = "AA:CC:EE:00:11:22" };
    const char *            p_coordinates = "170.112233,59.445566";
    const adv_report_t      adv_report    = {
        .tag_mac   = { 0xaa, 0xbb, 0xcc, 0x01, 0x02, 0x03 },
        .timestamp = 1612358929,
        .rssi      = -70,
        .data      = { 'a', 'a', '\0' },
    };

    this->m_malloc_fail_on_cnt = 5;
    ASSERT_FALSE(mqtt_create_json_str(&adv_report, timestamp, &gw_mac_addr, p_coordinates, &this->m_json_str));
    ASSERT_EQ(nullptr, this->m_json_str.p_str);
    ASSERT_TRUE(this->m_mem_alloc_trace.is_empty());
}

TEST_F(MqttJson, test_malloc_failed_6_of_22) // NOLINT
{
    const time_t            timestamp     = 1612358920;
    const mac_address_str_t gw_mac_addr   = { .str_buf = "AA:CC:EE:00:11:22" };
    const char *            p_coordinates = "170.112233,59.445566";
    const adv_report_t      adv_report    = {
        .tag_mac   = { 0xaa, 0xbb, 0xcc, 0x01, 0x02, 0x03 },
        .timestamp = 1612358929,
        .rssi      = -70,
        .data      = { 'a', 'a', '\0' },
    };

    this->m_malloc_fail_on_cnt = 6;
    ASSERT_FALSE(mqtt_create_json_str(&adv_report, timestamp, &gw_mac_addr, p_coordinates, &this->m_json_str));
    ASSERT_EQ(nullptr, this->m_json_str.p_str);
    ASSERT_TRUE(this->m_mem_alloc_trace.is_empty());
}

TEST_F(MqttJson, test_malloc_failed_7_of_22) // NOLINT
{
    const time_t            timestamp     = 1612358920;
    const mac_address_str_t gw_mac_addr   = { .str_buf = "AA:CC:EE:00:11:22" };
    const char *            p_coordinates = "170.112233,59.445566";
    const adv_report_t      adv_report    = {
        .tag_mac   = { 0xaa, 0xbb, 0xcc, 0x01, 0x02, 0x03 },
        .timestamp = 1612358929,
        .rssi      = -70,
        .data      = { 'a', 'a', '\0' },
    };

    this->m_malloc_fail_on_cnt = 7;
    ASSERT_FALSE(mqtt_create_json_str(&adv_report, timestamp, &gw_mac_addr, p_coordinates, &this->m_json_str));
    ASSERT_EQ(nullptr, this->m_json_str.p_str);
    ASSERT_TRUE(this->m_mem_alloc_trace.is_empty());
}

TEST_F(MqttJson, test_malloc_failed_8_of_22) // NOLINT
{
    const time_t            timestamp     = 1612358920;
    const mac_address_str_t gw_mac_addr   = { .str_buf = "AA:CC:EE:00:11:22" };
    const char *            p_coordinates = "170.112233,59.445566";
    const adv_report_t      adv_report    = {
        .tag_mac   = { 0xaa, 0xbb, 0xcc, 0x01, 0x02, 0x03 },
        .timestamp = 1612358929,
        .rssi      = -70,
        .data      = { 'a', 'a', '\0' },
    };

    this->m_malloc_fail_on_cnt = 8;
    ASSERT_FALSE(mqtt_create_json_str(&adv_report, timestamp, &gw_mac_addr, p_coordinates, &this->m_json_str));
    ASSERT_EQ(nullptr, this->m_json_str.p_str);
    ASSERT_TRUE(this->m_mem_alloc_trace.is_empty());
}

TEST_F(MqttJson, test_malloc_failed_9_of_22) // NOLINT
{
    const time_t            timestamp     = 1612358920;
    const mac_address_str_t gw_mac_addr   = { .str_buf = "AA:CC:EE:00:11:22" };
    const char *            p_coordinates = "170.112233,59.445566";
    const adv_report_t      adv_report    = {
        .tag_mac   = { 0xaa, 0xbb, 0xcc, 0x01, 0x02, 0x03 },
        .timestamp = 1612358929,
        .rssi      = -70,
        .data      = { 'a', 'a', '\0' },
    };

    this->m_malloc_fail_on_cnt = 9;
    ASSERT_FALSE(mqtt_create_json_str(&adv_report, timestamp, &gw_mac_addr, p_coordinates, &this->m_json_str));
    ASSERT_EQ(nullptr, this->m_json_str.p_str);
    ASSERT_TRUE(this->m_mem_alloc_trace.is_empty());
}

TEST_F(MqttJson, test_malloc_failed_10_of_22) // NOLINT
{
    const time_t            timestamp     = 1612358920;
    const mac_address_str_t gw_mac_addr   = { .str_buf = "AA:CC:EE:00:11:22" };
    const char *            p_coordinates = "170.112233,59.445566";
    const adv_report_t      adv_report    = {
        .tag_mac   = { 0xaa, 0xbb, 0xcc, 0x01, 0x02, 0x03 },
        .timestamp = 1612358929,
        .rssi      = -70,
        .data      = { 'a', 'a', '\0' },
    };

    this->m_malloc_fail_on_cnt = 10;
    ASSERT_FALSE(mqtt_create_json_str(&adv_report, timestamp, &gw_mac_addr, p_coordinates, &this->m_json_str));
    ASSERT_EQ(nullptr, this->m_json_str.p_str);
    ASSERT_TRUE(this->m_mem_alloc_trace.is_empty());
}

TEST_F(MqttJson, test_malloc_failed_11_of_22) // NOLINT
{
    const time_t            timestamp     = 1612358920;
    const mac_address_str_t gw_mac_addr   = { .str_buf = "AA:CC:EE:00:11:22" };
    const char *            p_coordinates = "170.112233,59.445566";
    const adv_report_t      adv_report    = {
        .tag_mac   = { 0xaa, 0xbb, 0xcc, 0x01, 0x02, 0x03 },
        .timestamp = 1612358929,
        .rssi      = -70,
        .data      = { 'a', 'a', '\0' },
    };

    this->m_malloc_fail_on_cnt = 11;
    ASSERT_FALSE(mqtt_create_json_str(&adv_report, timestamp, &gw_mac_addr, p_coordinates, &this->m_json_str));
    ASSERT_EQ(nullptr, this->m_json_str.p_str);
    ASSERT_TRUE(this->m_mem_alloc_trace.is_empty());
}

TEST_F(MqttJson, test_malloc_failed_12_of_22) // NOLINT
{
    const time_t            timestamp     = 1612358920;
    const mac_address_str_t gw_mac_addr   = { .str_buf = "AA:CC:EE:00:11:22" };
    const char *            p_coordinates = "170.112233,59.445566";
    const adv_report_t      adv_report    = {
        .tag_mac   = { 0xaa, 0xbb, 0xcc, 0x01, 0x02, 0x03 },
        .timestamp = 1612358929,
        .rssi      = -70,
        .data      = { 'a', 'a', '\0' },
    };

    this->m_malloc_fail_on_cnt = 12;
    ASSERT_FALSE(mqtt_create_json_str(&adv_report, timestamp, &gw_mac_addr, p_coordinates, &this->m_json_str));
    ASSERT_EQ(nullptr, this->m_json_str.p_str);
    ASSERT_TRUE(this->m_mem_alloc_trace.is_empty());
}

TEST_F(MqttJson, test_malloc_failed_13_of_22) // NOLINT
{
    const time_t            timestamp     = 1612358920;
    const mac_address_str_t gw_mac_addr   = { .str_buf = "AA:CC:EE:00:11:22" };
    const char *            p_coordinates = "170.112233,59.445566";
    const adv_report_t      adv_report    = {
        .tag_mac   = { 0xaa, 0xbb, 0xcc, 0x01, 0x02, 0x03 },
        .timestamp = 1612358929,
        .rssi      = -70,
        .data      = { 'a', 'a', '\0' },
    };

    this->m_malloc_fail_on_cnt = 13;
    ASSERT_FALSE(mqtt_create_json_str(&adv_report, timestamp, &gw_mac_addr, p_coordinates, &this->m_json_str));
    ASSERT_EQ(nullptr, this->m_json_str.p_str);
    ASSERT_TRUE(this->m_mem_alloc_trace.is_empty());
}

TEST_F(MqttJson, test_malloc_failed_14_of_22) // NOLINT
{
    const time_t            timestamp     = 1612358920;
    const mac_address_str_t gw_mac_addr   = { .str_buf = "AA:CC:EE:00:11:22" };
    const char *            p_coordinates = "170.112233,59.445566";
    const adv_report_t      adv_report    = {
        .tag_mac   = { 0xaa, 0xbb, 0xcc, 0x01, 0x02, 0x03 },
        .timestamp = 1612358929,
        .rssi      = -70,
        .data      = { 'a', 'a', '\0' },
    };

    this->m_malloc_fail_on_cnt = 14;
    ASSERT_FALSE(mqtt_create_json_str(&adv_report, timestamp, &gw_mac_addr, p_coordinates, &this->m_json_str));
    ASSERT_EQ(nullptr, this->m_json_str.p_str);
    ASSERT_TRUE(this->m_mem_alloc_trace.is_empty());
}

TEST_F(MqttJson, test_malloc_failed_15_of_22) // NOLINT
{
    const time_t            timestamp     = 1612358920;
    const mac_address_str_t gw_mac_addr   = { .str_buf = "AA:CC:EE:00:11:22" };
    const char *            p_coordinates = "170.112233,59.445566";
    const adv_report_t      adv_report    = {
        .tag_mac   = { 0xaa, 0xbb, 0xcc, 0x01, 0x02, 0x03 },
        .timestamp = 1612358929,
        .rssi      = -70,
        .data      = { 'a', 'a', '\0' },
    };

    this->m_malloc_fail_on_cnt = 15;
    ASSERT_FALSE(mqtt_create_json_str(&adv_report, timestamp, &gw_mac_addr, p_coordinates, &this->m_json_str));
    ASSERT_EQ(nullptr, this->m_json_str.p_str);
    ASSERT_TRUE(this->m_mem_alloc_trace.is_empty());
}

TEST_F(MqttJson, test_malloc_failed_16_of_22) // NOLINT
{
    const time_t            timestamp     = 1612358920;
    const mac_address_str_t gw_mac_addr   = { .str_buf = "AA:CC:EE:00:11:22" };
    const char *            p_coordinates = "170.112233,59.445566";
    const adv_report_t      adv_report    = {
        .tag_mac   = { 0xaa, 0xbb, 0xcc, 0x01, 0x02, 0x03 },
        .timestamp = 1612358929,
        .rssi      = -70,
        .data      = { 'a', 'a', '\0' },
    };

    this->m_malloc_fail_on_cnt = 16;
    ASSERT_FALSE(mqtt_create_json_str(&adv_report, timestamp, &gw_mac_addr, p_coordinates, &this->m_json_str));
    ASSERT_EQ(nullptr, this->m_json_str.p_str);
    ASSERT_TRUE(this->m_mem_alloc_trace.is_empty());
}

TEST_F(MqttJson, test_malloc_failed_17_of_22) // NOLINT
{
    const time_t            timestamp     = 1612358920;
    const mac_address_str_t gw_mac_addr   = { .str_buf = "AA:CC:EE:00:11:22" };
    const char *            p_coordinates = "170.112233,59.445566";
    const adv_report_t      adv_report    = {
        .tag_mac   = { 0xaa, 0xbb, 0xcc, 0x01, 0x02, 0x03 },
        .timestamp = 1612358929,
        .rssi      = -70,
        .data      = { 'a', 'a', '\0' },
    };

    this->m_malloc_fail_on_cnt = 17;
    ASSERT_FALSE(mqtt_create_json_str(&adv_report, timestamp, &gw_mac_addr, p_coordinates, &this->m_json_str));
    ASSERT_EQ(nullptr, this->m_json_str.p_str);
    ASSERT_TRUE(this->m_mem_alloc_trace.is_empty());
}

TEST_F(MqttJson, test_malloc_failed_18_of_22) // NOLINT
{
    const time_t            timestamp     = 1612358920;
    const mac_address_str_t gw_mac_addr   = { .str_buf = "AA:CC:EE:00:11:22" };
    const char *            p_coordinates = "170.112233,59.445566";
    const adv_report_t      adv_report    = {
        .tag_mac   = { 0xaa, 0xbb, 0xcc, 0x01, 0x02, 0x03 },
        .timestamp = 1612358929,
        .rssi      = -70,
        .data      = { 'a', 'a', '\0' },
    };

    this->m_malloc_fail_on_cnt = 18;
    ASSERT_FALSE(mqtt_create_json_str(&adv_report, timestamp, &gw_mac_addr, p_coordinates, &this->m_json_str));
    ASSERT_EQ(nullptr, this->m_json_str.p_str);
    ASSERT_TRUE(this->m_mem_alloc_trace.is_empty());
}

TEST_F(MqttJson, test_malloc_failed_19_of_22) // NOLINT
{
    const time_t            timestamp     = 1612358920;
    const mac_address_str_t gw_mac_addr   = { .str_buf = "AA:CC:EE:00:11:22" };
    const char *            p_coordinates = "170.112233,59.445566";
    const adv_report_t      adv_report    = {
        .tag_mac   = { 0xaa, 0xbb, 0xcc, 0x01, 0x02, 0x03 },
        .timestamp = 1612358929,
        .rssi      = -70,
        .data      = { 'a', 'a', '\0' },
    };

    this->m_malloc_fail_on_cnt = 19;
    ASSERT_FALSE(mqtt_create_json_str(&adv_report, timestamp, &gw_mac_addr, p_coordinates, &this->m_json_str));
    ASSERT_EQ(nullptr, this->m_json_str.p_str);
    ASSERT_TRUE(this->m_mem_alloc_trace.is_empty());
}

TEST_F(MqttJson, test_malloc_failed_20_of_22) // NOLINT
{
    const time_t            timestamp     = 1612358920;
    const mac_address_str_t gw_mac_addr   = { .str_buf = "AA:CC:EE:00:11:22" };
    const char *            p_coordinates = "170.112233,59.445566";
    const adv_report_t      adv_report    = {
        .tag_mac   = { 0xaa, 0xbb, 0xcc, 0x01, 0x02, 0x03 },
        .timestamp = 1612358929,
        .rssi      = -70,
        .data      = { 'a', 'a', '\0' },
    };

    this->m_malloc_fail_on_cnt = 20;
    ASSERT_FALSE(mqtt_create_json_str(&adv_report, timestamp, &gw_mac_addr, p_coordinates, &this->m_json_str));
    ASSERT_EQ(nullptr, this->m_json_str.p_str);
    ASSERT_TRUE(this->m_mem_alloc_trace.is_empty());
}

TEST_F(MqttJson, test_malloc_failed_21_of_22) // NOLINT
{
    const time_t            timestamp     = 1612358920;
    const mac_address_str_t gw_mac_addr   = { .str_buf = "AA:CC:EE:00:11:22" };
    const char *            p_coordinates = "170.112233,59.445566";
    const adv_report_t      adv_report    = {
        .tag_mac   = { 0xaa, 0xbb, 0xcc, 0x01, 0x02, 0x03 },
        .timestamp = 1612358929,
        .rssi      = -70,
        .data      = { 'a', 'a', '\0' },
    };

    this->m_malloc_fail_on_cnt = 21;
    ASSERT_FALSE(mqtt_create_json_str(&adv_report, timestamp, &gw_mac_addr, p_coordinates, &this->m_json_str));
    ASSERT_EQ(nullptr, this->m_json_str.p_str);
    ASSERT_TRUE(this->m_mem_alloc_trace.is_empty());
}

TEST_F(MqttJson, test_malloc_failed_22_of_22) // NOLINT
{
    const time_t            timestamp     = 1612358920;
    const mac_address_str_t gw_mac_addr   = { .str_buf = "AA:CC:EE:00:11:22" };
    const char *            p_coordinates = "170.112233,59.445566";
    const adv_report_t      adv_report    = {
        .tag_mac   = { 0xaa, 0xbb, 0xcc, 0x01, 0x02, 0x03 },
        .timestamp = 1612358929,
        .rssi      = -70,
        .data      = { 'a', 'a', '\0' },
    };

    this->m_malloc_fail_on_cnt = 22;
    ASSERT_FALSE(mqtt_create_json_str(&adv_report, timestamp, &gw_mac_addr, p_coordinates, &this->m_json_str));
    ASSERT_EQ(nullptr, this->m_json_str.p_str);
    ASSERT_TRUE(this->m_mem_alloc_trace.is_empty());
}
