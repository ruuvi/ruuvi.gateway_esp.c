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
    adv_report_table_t           adv_table     = { .num_of_advs = 1,
                                     .table       = { {
                                         .timestamp = 1612358929,
                                         .tag_mac   = { 0xaa, 0xbb, 0xcc, 0x01, 0x02, 0x03 },
                                         .rssi      = -70,
                                         .data_len  = data.size(),
                                     } } };
    memcpy(adv_table.table[0].data_buf, data.data(), data.size());
    ASSERT_TRUE(http_json_create_records_str(
        &adv_table,
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

TEST_F(TestHttpJson, test_2) // NOLINT
{
    const time_t                 timestamp     = 1612358920;
    const mac_address_str_t      gw_mac_addr   = { .str_buf = "AA:CC:EE:00:11:22" };
    const char *                 p_coordinates = "170.112233,59.445566";
    const std::array<uint8_t, 1> data1         = { 0xAAU };
    const std::array<uint8_t, 1> data2         = { 0xBBU };
    adv_report_table_t           adv_table     = { .num_of_advs = 2,
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

TEST_F(TestHttpJson, test_malloc_failed_1_of_27) // NOLINT
{
    const time_t                 timestamp     = 1612358920;
    const mac_address_str_t      gw_mac_addr   = { .str_buf = "AA:CC:EE:00:11:22" };
    const char *                 p_coordinates = "170.112233,59.445566";
    const std::array<uint8_t, 1> data          = { 0xAAU };
    adv_report_table_t           adv_table     = { .num_of_advs = 1,
                                     .table       = { {
                                         .timestamp = 1612358929,
                                         .tag_mac   = { 0xaa, 0xbb, 0xcc, 0x01, 0x02, 0x03 },
                                         .rssi      = -70,
                                         .data_len  = data.size(),
                                     } } };
    memcpy(adv_table.table[0].data_buf, data.data(), data.size());

    this->m_malloc_fail_on_cnt = 1;
    ASSERT_FALSE(http_json_create_records_str(
        &adv_table,
        timestamp,
        &gw_mac_addr,
        p_coordinates,
        true,
        12345678,
        &this->m_json_str));
    ASSERT_EQ(nullptr, this->m_json_str.p_str);
    ASSERT_TRUE(this->m_mem_alloc_trace.is_empty());
}

TEST_F(TestHttpJson, test_malloc_failed_2_of_27) // NOLINT
{
    const time_t                 timestamp     = 1612358920;
    const mac_address_str_t      gw_mac_addr   = { .str_buf = "AA:CC:EE:00:11:22" };
    const char *                 p_coordinates = "170.112233,59.445566";
    const std::array<uint8_t, 1> data          = { 0xAAU };
    adv_report_table_t           adv_table     = { .num_of_advs = 1,
                                     .table       = { {
                                         .timestamp = 1612358929,
                                         .tag_mac   = { 0xaa, 0xbb, 0xcc, 0x01, 0x02, 0x03 },
                                         .rssi      = -70,
                                         .data_len  = data.size(),
                                     } } };
    memcpy(adv_table.table[0].data_buf, data.data(), data.size());

    this->m_malloc_fail_on_cnt = 2;
    ASSERT_FALSE(http_json_create_records_str(
        &adv_table,
        timestamp,
        &gw_mac_addr,
        p_coordinates,
        true,
        12345678,
        &this->m_json_str));
    ASSERT_EQ(nullptr, this->m_json_str.p_str);
    ASSERT_TRUE(this->m_mem_alloc_trace.is_empty());
}

TEST_F(TestHttpJson, test_malloc_failed_3_of_27) // NOLINT
{
    const time_t                 timestamp     = 1612358920;
    const mac_address_str_t      gw_mac_addr   = { .str_buf = "AA:CC:EE:00:11:22" };
    const char *                 p_coordinates = "170.112233,59.445566";
    const std::array<uint8_t, 1> data          = { 0xAAU };
    adv_report_table_t           adv_table     = { .num_of_advs = 1,
                                     .table       = { {
                                         .timestamp = 1612358929,
                                         .tag_mac   = { 0xaa, 0xbb, 0xcc, 0x01, 0x02, 0x03 },
                                         .rssi      = -70,
                                         .data_len  = data.size(),
                                     } } };
    memcpy(adv_table.table[0].data_buf, data.data(), data.size());

    this->m_malloc_fail_on_cnt = 3;
    ASSERT_FALSE(http_json_create_records_str(
        &adv_table,
        timestamp,
        &gw_mac_addr,
        p_coordinates,
        true,
        12345678,
        &this->m_json_str));
    ASSERT_EQ(nullptr, this->m_json_str.p_str);
    ASSERT_TRUE(this->m_mem_alloc_trace.is_empty());
}

TEST_F(TestHttpJson, test_malloc_failed_4_of_27) // NOLINT
{
    const time_t                 timestamp     = 1612358920;
    const mac_address_str_t      gw_mac_addr   = { .str_buf = "AA:CC:EE:00:11:22" };
    const char *                 p_coordinates = "170.112233,59.445566";
    const std::array<uint8_t, 1> data          = { 0xAAU };
    adv_report_table_t           adv_table     = { .num_of_advs = 1,
                                     .table       = { {
                                         .timestamp = 1612358929,
                                         .tag_mac   = { 0xaa, 0xbb, 0xcc, 0x01, 0x02, 0x03 },
                                         .rssi      = -70,
                                         .data_len  = data.size(),
                                     } } };
    memcpy(adv_table.table[0].data_buf, data.data(), data.size());

    this->m_malloc_fail_on_cnt = 4;
    ASSERT_FALSE(http_json_create_records_str(
        &adv_table,
        timestamp,
        &gw_mac_addr,
        p_coordinates,
        true,
        12345678,
        &this->m_json_str));
    ASSERT_EQ(nullptr, this->m_json_str.p_str);
    ASSERT_TRUE(this->m_mem_alloc_trace.is_empty());
}

TEST_F(TestHttpJson, test_malloc_failed_5_of_27) // NOLINT
{
    const time_t                 timestamp     = 1612358920;
    const mac_address_str_t      gw_mac_addr   = { .str_buf = "AA:CC:EE:00:11:22" };
    const char *                 p_coordinates = "170.112233,59.445566";
    const std::array<uint8_t, 1> data          = { 0xAAU };
    adv_report_table_t           adv_table     = { .num_of_advs = 1,
                                     .table       = { {
                                         .timestamp = 1612358929,
                                         .tag_mac   = { 0xaa, 0xbb, 0xcc, 0x01, 0x02, 0x03 },
                                         .rssi      = -70,
                                         .data_len  = data.size(),
                                     } } };
    memcpy(adv_table.table[0].data_buf, data.data(), data.size());

    this->m_malloc_fail_on_cnt = 5;
    ASSERT_FALSE(http_json_create_records_str(
        &adv_table,
        timestamp,
        &gw_mac_addr,
        p_coordinates,
        true,
        12345678,
        &this->m_json_str));
    ASSERT_EQ(nullptr, this->m_json_str.p_str);
    ASSERT_TRUE(this->m_mem_alloc_trace.is_empty());
}

TEST_F(TestHttpJson, test_malloc_failed_6_of_27) // NOLINT
{
    const time_t                 timestamp     = 1612358920;
    const mac_address_str_t      gw_mac_addr   = { .str_buf = "AA:CC:EE:00:11:22" };
    const char *                 p_coordinates = "170.112233,59.445566";
    const std::array<uint8_t, 1> data          = { 0xAAU };
    adv_report_table_t           adv_table     = { .num_of_advs = 1,
                                     .table       = { {
                                         .timestamp = 1612358929,
                                         .tag_mac   = { 0xaa, 0xbb, 0xcc, 0x01, 0x02, 0x03 },
                                         .rssi      = -70,
                                         .data_len  = data.size(),
                                     } } };
    memcpy(adv_table.table[0].data_buf, data.data(), data.size());

    this->m_malloc_fail_on_cnt = 6;
    ASSERT_FALSE(http_json_create_records_str(
        &adv_table,
        timestamp,
        &gw_mac_addr,
        p_coordinates,
        true,
        12345678,
        &this->m_json_str));
    ASSERT_EQ(nullptr, this->m_json_str.p_str);
    ASSERT_TRUE(this->m_mem_alloc_trace.is_empty());
}

TEST_F(TestHttpJson, test_malloc_failed_7_of_27) // NOLINT
{
    const time_t                 timestamp     = 1612358920;
    const mac_address_str_t      gw_mac_addr   = { .str_buf = "AA:CC:EE:00:11:22" };
    const char *                 p_coordinates = "170.112233,59.445566";
    const std::array<uint8_t, 1> data          = { 0xAAU };
    adv_report_table_t           adv_table     = { .num_of_advs = 1,
                                     .table       = { {
                                         .timestamp = 1612358929,
                                         .tag_mac   = { 0xaa, 0xbb, 0xcc, 0x01, 0x02, 0x03 },
                                         .rssi      = -70,
                                         .data_len  = data.size(),
                                     } } };
    memcpy(adv_table.table[0].data_buf, data.data(), data.size());

    this->m_malloc_fail_on_cnt = 7;
    ASSERT_FALSE(http_json_create_records_str(
        &adv_table,
        timestamp,
        &gw_mac_addr,
        p_coordinates,
        true,
        12345678,
        &this->m_json_str));
    ASSERT_EQ(nullptr, this->m_json_str.p_str);
    ASSERT_TRUE(this->m_mem_alloc_trace.is_empty());
}

TEST_F(TestHttpJson, test_malloc_failed_8_of_27) // NOLINT
{
    const time_t                 timestamp     = 1612358920;
    const mac_address_str_t      gw_mac_addr   = { .str_buf = "AA:CC:EE:00:11:22" };
    const char *                 p_coordinates = "170.112233,59.445566";
    const std::array<uint8_t, 1> data          = { 0xAAU };
    adv_report_table_t           adv_table     = { .num_of_advs = 1,
                                     .table       = { {
                                         .timestamp = 1612358929,
                                         .tag_mac   = { 0xaa, 0xbb, 0xcc, 0x01, 0x02, 0x03 },
                                         .rssi      = -70,
                                         .data_len  = data.size(),
                                     } } };
    memcpy(adv_table.table[0].data_buf, data.data(), data.size());

    this->m_malloc_fail_on_cnt = 8;
    ASSERT_FALSE(http_json_create_records_str(
        &adv_table,
        timestamp,
        &gw_mac_addr,
        p_coordinates,
        true,
        12345678,
        &this->m_json_str));
    ASSERT_EQ(nullptr, this->m_json_str.p_str);
    ASSERT_TRUE(this->m_mem_alloc_trace.is_empty());
}

TEST_F(TestHttpJson, test_malloc_failed_9_of_27) // NOLINT
{
    const time_t                 timestamp     = 1612358920;
    const mac_address_str_t      gw_mac_addr   = { .str_buf = "AA:CC:EE:00:11:22" };
    const char *                 p_coordinates = "170.112233,59.445566";
    const std::array<uint8_t, 1> data          = { 0xAAU };
    adv_report_table_t           adv_table     = { .num_of_advs = 1,
                                     .table       = { {
                                         .timestamp = 1612358929,
                                         .tag_mac   = { 0xaa, 0xbb, 0xcc, 0x01, 0x02, 0x03 },
                                         .rssi      = -70,
                                         .data_len  = data.size(),
                                     } } };
    memcpy(adv_table.table[0].data_buf, data.data(), data.size());

    this->m_malloc_fail_on_cnt = 9;
    ASSERT_FALSE(http_json_create_records_str(
        &adv_table,
        timestamp,
        &gw_mac_addr,
        p_coordinates,
        true,
        12345678,
        &this->m_json_str));
    ASSERT_EQ(nullptr, this->m_json_str.p_str);
    ASSERT_TRUE(this->m_mem_alloc_trace.is_empty());
}

TEST_F(TestHttpJson, test_malloc_failed_10_of_27) // NOLINT
{
    const time_t                 timestamp     = 1612358920;
    const mac_address_str_t      gw_mac_addr   = { .str_buf = "AA:CC:EE:00:11:22" };
    const char *                 p_coordinates = "170.112233,59.445566";
    const std::array<uint8_t, 1> data          = { 0xAAU };
    adv_report_table_t           adv_table     = { .num_of_advs = 1,
                                     .table       = { {
                                         .timestamp = 1612358929,
                                         .tag_mac   = { 0xaa, 0xbb, 0xcc, 0x01, 0x02, 0x03 },
                                         .rssi      = -70,
                                         .data_len  = data.size(),
                                     } } };
    memcpy(adv_table.table[0].data_buf, data.data(), data.size());

    this->m_malloc_fail_on_cnt = 10;
    ASSERT_FALSE(http_json_create_records_str(
        &adv_table,
        timestamp,
        &gw_mac_addr,
        p_coordinates,
        true,
        12345678,
        &this->m_json_str));
    ASSERT_EQ(nullptr, this->m_json_str.p_str);
    ASSERT_TRUE(this->m_mem_alloc_trace.is_empty());
}

TEST_F(TestHttpJson, test_malloc_failed_11_of_27) // NOLINT
{
    const time_t                 timestamp     = 1612358920;
    const mac_address_str_t      gw_mac_addr   = { .str_buf = "AA:CC:EE:00:11:22" };
    const char *                 p_coordinates = "170.112233,59.445566";
    const std::array<uint8_t, 1> data          = { 0xAAU };
    adv_report_table_t           adv_table     = { .num_of_advs = 1,
                                     .table       = { {
                                         .timestamp = 1612358929,
                                         .tag_mac   = { 0xaa, 0xbb, 0xcc, 0x01, 0x02, 0x03 },
                                         .rssi      = -70,
                                         .data_len  = data.size(),
                                     } } };
    memcpy(adv_table.table[0].data_buf, data.data(), data.size());

    this->m_malloc_fail_on_cnt = 11;
    ASSERT_FALSE(http_json_create_records_str(
        &adv_table,
        timestamp,
        &gw_mac_addr,
        p_coordinates,
        true,
        12345678,
        &this->m_json_str));
    ASSERT_EQ(nullptr, this->m_json_str.p_str);
    ASSERT_TRUE(this->m_mem_alloc_trace.is_empty());
}

TEST_F(TestHttpJson, test_malloc_failed_12_of_27) // NOLINT
{
    const time_t                 timestamp     = 1612358920;
    const mac_address_str_t      gw_mac_addr   = { .str_buf = "AA:CC:EE:00:11:22" };
    const char *                 p_coordinates = "170.112233,59.445566";
    const std::array<uint8_t, 1> data          = { 0xAAU };
    adv_report_table_t           adv_table     = { .num_of_advs = 1,
                                     .table       = { {
                                         .timestamp = 1612358929,
                                         .tag_mac   = { 0xaa, 0xbb, 0xcc, 0x01, 0x02, 0x03 },
                                         .rssi      = -70,
                                         .data_len  = data.size(),
                                     } } };
    memcpy(adv_table.table[0].data_buf, data.data(), data.size());

    this->m_malloc_fail_on_cnt = 12;
    ASSERT_FALSE(http_json_create_records_str(
        &adv_table,
        timestamp,
        &gw_mac_addr,
        p_coordinates,
        true,
        12345678,
        &this->m_json_str));
    ASSERT_EQ(nullptr, this->m_json_str.p_str);
    ASSERT_TRUE(this->m_mem_alloc_trace.is_empty());
}

TEST_F(TestHttpJson, test_malloc_failed_13_of_27) // NOLINT
{
    const time_t                 timestamp     = 1612358920;
    const mac_address_str_t      gw_mac_addr   = { .str_buf = "AA:CC:EE:00:11:22" };
    const char *                 p_coordinates = "170.112233,59.445566";
    const std::array<uint8_t, 1> data          = { 0xAAU };
    const adv_report_table_t     adv_table     = { .num_of_advs = 1,
                                           .table       = { {
                                               .timestamp = 1612358929,
                                               .tag_mac   = { 0xaa, 0xbb, 0xcc, 0x01, 0x02, 0x03 },
                                               .rssi      = -70,
                                               .data_len  = data.size(),
                                           } } };
    memcpy((void *)adv_table.table[0].data_buf, (void *)data.data(), data.size());

    this->m_malloc_fail_on_cnt = 13;
    ASSERT_FALSE(http_json_create_records_str(
        &adv_table,
        timestamp,
        &gw_mac_addr,
        p_coordinates,
        true,
        12345678,
        &this->m_json_str));
    ASSERT_EQ(nullptr, this->m_json_str.p_str);
    ASSERT_TRUE(this->m_mem_alloc_trace.is_empty());
}

TEST_F(TestHttpJson, test_malloc_failed_14_of_27) // NOLINT
{
    const time_t                 timestamp     = 1612358920;
    const mac_address_str_t      gw_mac_addr   = { .str_buf = "AA:CC:EE:00:11:22" };
    const char *                 p_coordinates = "170.112233,59.445566";
    const std::array<uint8_t, 1> data          = { 0xAAU };
    const adv_report_table_t     adv_table     = { .num_of_advs = 1,
                                           .table       = { {
                                               .timestamp = 1612358929,
                                               .tag_mac   = { 0xaa, 0xbb, 0xcc, 0x01, 0x02, 0x03 },
                                               .rssi      = -70,
                                               .data_len  = data.size(),
                                           } } };
    memcpy((void *)adv_table.table[0].data_buf, (void *)data.data(), data.size());

    this->m_malloc_fail_on_cnt = 14;
    ASSERT_FALSE(http_json_create_records_str(
        &adv_table,
        timestamp,
        &gw_mac_addr,
        p_coordinates,
        true,
        12345678,
        &this->m_json_str));
    ASSERT_EQ(nullptr, this->m_json_str.p_str);
    ASSERT_TRUE(this->m_mem_alloc_trace.is_empty());
}

TEST_F(TestHttpJson, test_malloc_failed_15_of_27) // NOLINT
{
    const time_t                 timestamp     = 1612358920;
    const mac_address_str_t      gw_mac_addr   = { .str_buf = "AA:CC:EE:00:11:22" };
    const char *                 p_coordinates = "170.112233,59.445566";
    const std::array<uint8_t, 1> data          = { 0xAAU };
    const adv_report_table_t     adv_table     = { .num_of_advs = 1,
                                           .table       = { {
                                               .timestamp = 1612358929,
                                               .tag_mac   = { 0xaa, 0xbb, 0xcc, 0x01, 0x02, 0x03 },
                                               .rssi      = -70,
                                               .data_len  = data.size(),
                                           } } };
    memcpy((void *)adv_table.table[0].data_buf, (void *)data.data(), data.size());

    this->m_malloc_fail_on_cnt = 15;
    ASSERT_FALSE(http_json_create_records_str(
        &adv_table,
        timestamp,
        &gw_mac_addr,
        p_coordinates,
        true,
        12345678,
        &this->m_json_str));
    ASSERT_EQ(nullptr, this->m_json_str.p_str);
    ASSERT_TRUE(this->m_mem_alloc_trace.is_empty());
}

TEST_F(TestHttpJson, test_malloc_failed_16_of_27) // NOLINT
{
    const time_t                 timestamp     = 1612358920;
    const mac_address_str_t      gw_mac_addr   = { .str_buf = "AA:CC:EE:00:11:22" };
    const char *                 p_coordinates = "170.112233,59.445566";
    const std::array<uint8_t, 1> data          = { 0xAAU };
    const adv_report_table_t     adv_table     = { .num_of_advs = 1,
                                           .table       = { {
                                               .timestamp = 1612358929,
                                               .tag_mac   = { 0xaa, 0xbb, 0xcc, 0x01, 0x02, 0x03 },
                                               .rssi      = -70,
                                               .data_len  = data.size(),
                                           } } };
    memcpy((void *)adv_table.table[0].data_buf, (void *)data.data(), data.size());

    this->m_malloc_fail_on_cnt = 16;
    ASSERT_FALSE(http_json_create_records_str(
        &adv_table,
        timestamp,
        &gw_mac_addr,
        p_coordinates,
        true,
        12345678,
        &this->m_json_str));
    ASSERT_EQ(nullptr, this->m_json_str.p_str);
    ASSERT_TRUE(this->m_mem_alloc_trace.is_empty());
}

TEST_F(TestHttpJson, test_malloc_failed_17_of_27) // NOLINT
{
    const time_t                 timestamp     = 1612358920;
    const mac_address_str_t      gw_mac_addr   = { .str_buf = "AA:CC:EE:00:11:22" };
    const char *                 p_coordinates = "170.112233,59.445566";
    const std::array<uint8_t, 1> data          = { 0xAAU };
    const adv_report_table_t     adv_table     = { .num_of_advs = 1,
                                           .table       = { {
                                               .timestamp = 1612358929,
                                               .tag_mac   = { 0xaa, 0xbb, 0xcc, 0x01, 0x02, 0x03 },
                                               .rssi      = -70,
                                               .data_len  = data.size(),
                                           } } };
    memcpy((void *)adv_table.table[0].data_buf, (void *)data.data(), data.size());

    this->m_malloc_fail_on_cnt = 17;
    ASSERT_FALSE(http_json_create_records_str(
        &adv_table,
        timestamp,
        &gw_mac_addr,
        p_coordinates,
        true,
        12345678,
        &this->m_json_str));
    ASSERT_EQ(nullptr, this->m_json_str.p_str);
    ASSERT_TRUE(this->m_mem_alloc_trace.is_empty());
}

TEST_F(TestHttpJson, test_malloc_failed_18_of_27) // NOLINT
{
    const time_t                 timestamp     = 1612358920;
    const mac_address_str_t      gw_mac_addr   = { .str_buf = "AA:CC:EE:00:11:22" };
    const char *                 p_coordinates = "170.112233,59.445566";
    const std::array<uint8_t, 1> data          = { 0xAAU };
    const adv_report_table_t     adv_table     = { .num_of_advs = 1,
                                           .table       = { {
                                               .timestamp = 1612358929,
                                               .tag_mac   = { 0xaa, 0xbb, 0xcc, 0x01, 0x02, 0x03 },
                                               .rssi      = -70,
                                               .data_len  = data.size(),
                                           } } };
    memcpy((void *)adv_table.table[0].data_buf, (void *)data.data(), data.size());

    this->m_malloc_fail_on_cnt = 18;
    ASSERT_FALSE(http_json_create_records_str(
        &adv_table,
        timestamp,
        &gw_mac_addr,
        p_coordinates,
        true,
        12345678,
        &this->m_json_str));
    ASSERT_EQ(nullptr, this->m_json_str.p_str);
    ASSERT_TRUE(this->m_mem_alloc_trace.is_empty());
}

TEST_F(TestHttpJson, test_malloc_failed_19_of_27) // NOLINT
{
    const time_t                 timestamp     = 1612358920;
    const mac_address_str_t      gw_mac_addr   = { .str_buf = "AA:CC:EE:00:11:22" };
    const char *                 p_coordinates = "170.112233,59.445566";
    const std::array<uint8_t, 1> data          = { 0xAAU };
    adv_report_table_t           adv_table     = { .num_of_advs = 1,
                                     .table       = { {
                                         .timestamp = 1612358929,
                                         .tag_mac   = { 0xaa, 0xbb, 0xcc, 0x01, 0x02, 0x03 },
                                         .rssi      = -70,
                                         .data_len  = data.size(),
                                     } } };
    memcpy(adv_table.table[0].data_buf, data.data(), data.size());

    this->m_malloc_fail_on_cnt = 19;
    ASSERT_FALSE(http_json_create_records_str(
        &adv_table,
        timestamp,
        &gw_mac_addr,
        p_coordinates,
        true,
        12345678,
        &this->m_json_str));
    ASSERT_EQ(nullptr, this->m_json_str.p_str);
    ASSERT_TRUE(this->m_mem_alloc_trace.is_empty());
}

TEST_F(TestHttpJson, test_malloc_failed_20_of_27) // NOLINT
{
    const time_t                 timestamp     = 1612358920;
    const mac_address_str_t      gw_mac_addr   = { .str_buf = "AA:CC:EE:00:11:22" };
    const char *                 p_coordinates = "170.112233,59.445566";
    const std::array<uint8_t, 1> data          = { 0xAAU };
    const adv_report_table_t     adv_table     = { .num_of_advs = 1,
                                           .table       = { {
                                               .timestamp = 1612358929,
                                               .tag_mac   = { 0xaa, 0xbb, 0xcc, 0x01, 0x02, 0x03 },
                                               .rssi      = -70,
                                               .data_len  = data.size(),
                                           } } };
    memcpy((void *)adv_table.table[0].data_buf, (void *)data.data(), data.size());

    this->m_malloc_fail_on_cnt = 20;
    ASSERT_FALSE(http_json_create_records_str(
        &adv_table,
        timestamp,
        &gw_mac_addr,
        p_coordinates,
        true,
        12345678,
        &this->m_json_str));
    ASSERT_EQ(nullptr, this->m_json_str.p_str);
    ASSERT_TRUE(this->m_mem_alloc_trace.is_empty());
}

TEST_F(TestHttpJson, test_malloc_failed_21_of_27) // NOLINT
{
    const time_t                 timestamp     = 1612358920;
    const mac_address_str_t      gw_mac_addr   = { .str_buf = "AA:CC:EE:00:11:22" };
    const char *                 p_coordinates = "170.112233,59.445566";
    const std::array<uint8_t, 1> data          = { 0xAAU };
    const adv_report_table_t     adv_table     = { .num_of_advs = 1,
                                           .table       = { {
                                               .timestamp = 1612358929,
                                               .tag_mac   = { 0xaa, 0xbb, 0xcc, 0x01, 0x02, 0x03 },
                                               .rssi      = -70,
                                               .data_len  = data.size(),
                                           } } };
    memcpy((void *)adv_table.table[0].data_buf, (void *)data.data(), data.size());

    this->m_malloc_fail_on_cnt = 21;
    ASSERT_FALSE(http_json_create_records_str(
        &adv_table,
        timestamp,
        &gw_mac_addr,
        p_coordinates,
        true,
        12345678,
        &this->m_json_str));
    ASSERT_EQ(nullptr, this->m_json_str.p_str);
    ASSERT_TRUE(this->m_mem_alloc_trace.is_empty());
}

TEST_F(TestHttpJson, test_malloc_failed_22_of_27) // NOLINT
{
    const time_t                 timestamp     = 1612358920;
    const mac_address_str_t      gw_mac_addr   = { .str_buf = "AA:CC:EE:00:11:22" };
    const char *                 p_coordinates = "170.112233,59.445566";
    const std::array<uint8_t, 1> data          = { 0xAAU };
    const adv_report_table_t     adv_table     = { .num_of_advs = 1,
                                           .table       = { {
                                               .timestamp = 1612358929,
                                               .tag_mac   = { 0xaa, 0xbb, 0xcc, 0x01, 0x02, 0x03 },
                                               .rssi      = -70,
                                               .data_len  = data.size(),
                                           } } };
    memcpy((void *)adv_table.table[0].data_buf, (void *)data.data(), data.size());

    this->m_malloc_fail_on_cnt = 22;
    ASSERT_FALSE(http_json_create_records_str(
        &adv_table,
        timestamp,
        &gw_mac_addr,
        p_coordinates,
        true,
        12345678,
        &this->m_json_str));
    ASSERT_EQ(nullptr, this->m_json_str.p_str);
    ASSERT_TRUE(this->m_mem_alloc_trace.is_empty());
}

TEST_F(TestHttpJson, test_malloc_failed_23_of_27) // NOLINT
{
    const time_t                 timestamp     = 1612358920;
    const mac_address_str_t      gw_mac_addr   = { .str_buf = "AA:CC:EE:00:11:22" };
    const char *                 p_coordinates = "170.112233,59.445566";
    const std::array<uint8_t, 1> data          = { 0xAAU };
    const adv_report_table_t     adv_table     = { .num_of_advs = 1,
                                           .table       = { {
                                               .timestamp = 1612358929,
                                               .tag_mac   = { 0xaa, 0xbb, 0xcc, 0x01, 0x02, 0x03 },
                                               .rssi      = -70,
                                               .data_len  = data.size(),
                                           } } };
    memcpy((void *)adv_table.table[0].data_buf, (void *)data.data(), data.size());

    this->m_malloc_fail_on_cnt = 23;
    ASSERT_FALSE(http_json_create_records_str(
        &adv_table,
        timestamp,
        &gw_mac_addr,
        p_coordinates,
        true,
        12345678,
        &this->m_json_str));
    ASSERT_EQ(nullptr, this->m_json_str.p_str);
    ASSERT_TRUE(this->m_mem_alloc_trace.is_empty());
}

TEST_F(TestHttpJson, test_malloc_failed_24_of_27) // NOLINT
{
    const time_t                 timestamp     = 1612358920;
    const mac_address_str_t      gw_mac_addr   = { .str_buf = "AA:CC:EE:00:11:22" };
    const char *                 p_coordinates = "170.112233,59.445566";
    const std::array<uint8_t, 1> data          = { 0xAAU };
    const adv_report_table_t     adv_table     = { .num_of_advs = 1,
                                           .table       = { {
                                               .timestamp = 1612358929,
                                               .tag_mac   = { 0xaa, 0xbb, 0xcc, 0x01, 0x02, 0x03 },
                                               .rssi      = -70,
                                               .data_len  = data.size(),
                                           } } };
    memcpy((void *)adv_table.table[0].data_buf, (void *)data.data(), data.size());

    this->m_malloc_fail_on_cnt = 24;
    ASSERT_FALSE(http_json_create_records_str(
        &adv_table,
        timestamp,
        &gw_mac_addr,
        p_coordinates,
        true,
        12345678,
        &this->m_json_str));
    ASSERT_EQ(nullptr, this->m_json_str.p_str);
    ASSERT_TRUE(this->m_mem_alloc_trace.is_empty());
}

TEST_F(TestHttpJson, test_malloc_failed_25_of_27) // NOLINT
{
    const time_t                 timestamp     = 1612358920;
    const mac_address_str_t      gw_mac_addr   = { .str_buf = "AA:CC:EE:00:11:22" };
    const char *                 p_coordinates = "170.112233,59.445566";
    const std::array<uint8_t, 1> data          = { 0xAAU };
    const adv_report_table_t     adv_table     = { .num_of_advs = 1,
                                           .table       = { {
                                               .timestamp = 1612358929,
                                               .tag_mac   = { 0xaa, 0xbb, 0xcc, 0x01, 0x02, 0x03 },
                                               .rssi      = -70,
                                               .data_len  = data.size(),
                                           } } };
    memcpy((void *)adv_table.table[0].data_buf, (void *)data.data(), data.size());

    this->m_malloc_fail_on_cnt = 25;
    ASSERT_FALSE(http_json_create_records_str(
        &adv_table,
        timestamp,
        &gw_mac_addr,
        p_coordinates,
        true,
        12345678,
        &this->m_json_str));
    ASSERT_EQ(nullptr, this->m_json_str.p_str);
    ASSERT_TRUE(this->m_mem_alloc_trace.is_empty());
}

TEST_F(TestHttpJson, test_malloc_failed_26_of_27) // NOLINT
{
    const time_t                 timestamp     = 1612358920;
    const mac_address_str_t      gw_mac_addr   = { .str_buf = "AA:CC:EE:00:11:22" };
    const char *                 p_coordinates = "170.112233,59.445566";
    const std::array<uint8_t, 1> data          = { 0xAAU };
    const adv_report_table_t     adv_table     = { .num_of_advs = 1,
                                           .table       = { {
                                               .timestamp = 1612358929,
                                               .tag_mac   = { 0xaa, 0xbb, 0xcc, 0x01, 0x02, 0x03 },
                                               .rssi      = -70,
                                               .data_len  = data.size(),
                                           } } };
    memcpy((void *)adv_table.table[0].data_buf, (void *)data.data(), data.size());

    this->m_malloc_fail_on_cnt = 26;
    ASSERT_FALSE(http_json_create_records_str(
        &adv_table,
        timestamp,
        &gw_mac_addr,
        p_coordinates,
        true,
        12345678,
        &this->m_json_str));
    ASSERT_EQ(nullptr, this->m_json_str.p_str);
    ASSERT_TRUE(this->m_mem_alloc_trace.is_empty());
}

TEST_F(TestHttpJson, test_malloc_failed_27_of_27) // NOLINT
{
    const time_t                 timestamp     = 1612358920;
    const mac_address_str_t      gw_mac_addr   = { .str_buf = "AA:CC:EE:00:11:22" };
    const char *                 p_coordinates = "170.112233,59.445566";
    const std::array<uint8_t, 1> data          = { 0xAAU };
    const adv_report_table_t     adv_table     = { .num_of_advs = 1,
                                           .table       = { {
                                               .timestamp = 1612358929,
                                               .tag_mac   = { 0xaa, 0xbb, 0xcc, 0x01, 0x02, 0x03 },
                                               .rssi      = -70,
                                               .data_len  = data.size(),
                                           } } };
    memcpy((void *)adv_table.table[0].data_buf, (void *)data.data(), data.size());

    this->m_malloc_fail_on_cnt = 27;
    ASSERT_FALSE(http_json_create_records_str(
        &adv_table,
        timestamp,
        &gw_mac_addr,
        p_coordinates,
        true,
        12345678,
        &this->m_json_str));
    ASSERT_EQ(nullptr, this->m_json_str.p_str);
    ASSERT_TRUE(this->m_mem_alloc_trace.is_empty());
}

TEST_F(TestHttpJson, test_create_status_json_str_1) // NOLINT
{
    const mac_address_str_t      nrf52_mac_addr         = { .str_buf = "AA:CC:EE:00:11:22" };
    const uint32_t               uptime                 = 123;
    const bool                   is_wifi                = true;
    const uint32_t               network_disconnect_cnt = 3;
    const uint32_t               nonce                  = 1234567;
    const std::array<uint8_t, 1> data                   = { 0xAAU };
    adv_report_table_t           adv_table              = { .num_of_advs = 4,
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

TEST_F(TestHttpJson, test_create_status_json_str_malloc_failed_1_of_50) // NOLINT
{
    const mac_address_str_t      nrf52_mac_addr         = { .str_buf = "AA:CC:EE:00:11:22" };
    const uint32_t               uptime                 = 123;
    const bool                   is_wifi                = true;
    const uint32_t               network_disconnect_cnt = 3;
    const uint32_t               nonce                  = 1234567;
    const std::array<uint8_t, 1> data                   = { 0xAAU };
    adv_report_table_t           adv_table              = { .num_of_advs = 4,
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

    this->m_malloc_fail_on_cnt                  = 1;
    const http_json_statistics_info_t stat_info = {
        .nrf52_mac_addr         = nrf52_mac_addr,
        .esp_fw                 = { "1.9.0" },
        .nrf_fw                 = { "0.7.1" },
        .uptime                 = uptime,
        .nonce                  = nonce,
        .is_connected_to_wifi   = is_wifi,
        .network_disconnect_cnt = network_disconnect_cnt,
    };
    ASSERT_FALSE(http_json_create_status_str(&stat_info, &adv_table, &this->m_json_str));
    ASSERT_EQ(nullptr, this->m_json_str.p_str);
    ASSERT_TRUE(this->m_mem_alloc_trace.is_empty());
}

TEST_F(TestHttpJson, test_create_status_json_str_malloc_failed_2_of_50) // NOLINT
{
    const mac_address_str_t      nrf52_mac_addr         = { .str_buf = "AA:CC:EE:00:11:22" };
    const uint32_t               uptime                 = 123;
    const bool                   is_wifi                = true;
    const uint32_t               network_disconnect_cnt = 3;
    const uint32_t               nonce                  = 1234567;
    const std::array<uint8_t, 1> data                   = { 0xAAU };
    adv_report_table_t           adv_table              = { .num_of_advs = 4,
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

    this->m_malloc_fail_on_cnt                  = 2;
    const http_json_statistics_info_t stat_info = {
        .nrf52_mac_addr         = nrf52_mac_addr,
        .esp_fw                 = { "1.9.0" },
        .nrf_fw                 = { "0.7.1" },
        .uptime                 = uptime,
        .nonce                  = nonce,
        .is_connected_to_wifi   = is_wifi,
        .network_disconnect_cnt = network_disconnect_cnt,
    };
    ASSERT_FALSE(http_json_create_status_str(&stat_info, &adv_table, &this->m_json_str));
    ASSERT_EQ(nullptr, this->m_json_str.p_str);
    ASSERT_TRUE(this->m_mem_alloc_trace.is_empty());
}

TEST_F(TestHttpJson, test_create_status_json_str_malloc_failed_3_of_50) // NOLINT
{
    const mac_address_str_t      nrf52_mac_addr         = { .str_buf = "AA:CC:EE:00:11:22" };
    const uint32_t               uptime                 = 123;
    const bool                   is_wifi                = true;
    const uint32_t               network_disconnect_cnt = 3;
    const uint32_t               nonce                  = 1234567;
    const std::array<uint8_t, 1> data                   = { 0xAAU };
    adv_report_table_t           adv_table              = { .num_of_advs = 4,
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

    this->m_malloc_fail_on_cnt                  = 3;
    const http_json_statistics_info_t stat_info = {
        .nrf52_mac_addr         = nrf52_mac_addr,
        .esp_fw                 = { "1.9.0" },
        .nrf_fw                 = { "0.7.1" },
        .uptime                 = uptime,
        .nonce                  = nonce,
        .is_connected_to_wifi   = is_wifi,
        .network_disconnect_cnt = network_disconnect_cnt,
    };
    ASSERT_FALSE(http_json_create_status_str(&stat_info, &adv_table, &this->m_json_str));
    ASSERT_EQ(nullptr, this->m_json_str.p_str);
    ASSERT_TRUE(this->m_mem_alloc_trace.is_empty());
}

TEST_F(TestHttpJson, test_create_status_json_str_malloc_failed_4_of_50) // NOLINT
{
    const mac_address_str_t      nrf52_mac_addr         = { .str_buf = "AA:CC:EE:00:11:22" };
    const uint32_t               uptime                 = 123;
    const bool                   is_wifi                = true;
    const uint32_t               network_disconnect_cnt = 3;
    const uint32_t               nonce                  = 1234567;
    const std::array<uint8_t, 1> data                   = { 0xAAU };
    adv_report_table_t           adv_table              = { .num_of_advs = 4,
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

    this->m_malloc_fail_on_cnt                  = 4;
    const http_json_statistics_info_t stat_info = {
        .nrf52_mac_addr         = nrf52_mac_addr,
        .esp_fw                 = { "1.9.0" },
        .nrf_fw                 = { "0.7.1" },
        .uptime                 = uptime,
        .nonce                  = nonce,
        .is_connected_to_wifi   = is_wifi,
        .network_disconnect_cnt = network_disconnect_cnt,
    };
    ASSERT_FALSE(http_json_create_status_str(&stat_info, &adv_table, &this->m_json_str));
    ASSERT_EQ(nullptr, this->m_json_str.p_str);
    ASSERT_TRUE(this->m_mem_alloc_trace.is_empty());
}

TEST_F(TestHttpJson, test_create_status_json_str_malloc_failed_5_of_50) // NOLINT
{
    const mac_address_str_t      nrf52_mac_addr         = { .str_buf = "AA:CC:EE:00:11:22" };
    const uint32_t               uptime                 = 123;
    const bool                   is_wifi                = true;
    const uint32_t               network_disconnect_cnt = 3;
    const uint32_t               nonce                  = 1234567;
    const std::array<uint8_t, 1> data                   = { 0xAAU };
    adv_report_table_t           adv_table              = { .num_of_advs = 4,
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

    this->m_malloc_fail_on_cnt                  = 5;
    const http_json_statistics_info_t stat_info = {
        .nrf52_mac_addr         = nrf52_mac_addr,
        .esp_fw                 = { "1.9.0" },
        .nrf_fw                 = { "0.7.1" },
        .uptime                 = uptime,
        .nonce                  = nonce,
        .is_connected_to_wifi   = is_wifi,
        .network_disconnect_cnt = network_disconnect_cnt,
    };
    ASSERT_FALSE(http_json_create_status_str(&stat_info, &adv_table, &this->m_json_str));
    ASSERT_EQ(nullptr, this->m_json_str.p_str);
    ASSERT_TRUE(this->m_mem_alloc_trace.is_empty());
}

TEST_F(TestHttpJson, test_create_status_json_str_malloc_failed_6_of_50) // NOLINT
{
    const mac_address_str_t      nrf52_mac_addr         = { .str_buf = "AA:CC:EE:00:11:22" };
    const uint32_t               uptime                 = 123;
    const bool                   is_wifi                = true;
    const uint32_t               network_disconnect_cnt = 3;
    const uint32_t               nonce                  = 1234567;
    const std::array<uint8_t, 1> data                   = { 0xAAU };
    adv_report_table_t           adv_table              = { .num_of_advs = 4,
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

    this->m_malloc_fail_on_cnt                  = 6;
    const http_json_statistics_info_t stat_info = {
        .nrf52_mac_addr         = nrf52_mac_addr,
        .esp_fw                 = { "1.9.0" },
        .nrf_fw                 = { "0.7.1" },
        .uptime                 = uptime,
        .nonce                  = nonce,
        .is_connected_to_wifi   = is_wifi,
        .network_disconnect_cnt = network_disconnect_cnt,
    };
    ASSERT_FALSE(http_json_create_status_str(&stat_info, &adv_table, &this->m_json_str));
    ASSERT_EQ(nullptr, this->m_json_str.p_str);
    ASSERT_TRUE(this->m_mem_alloc_trace.is_empty());
}

TEST_F(TestHttpJson, test_create_status_json_str_malloc_failed_7_of_50) // NOLINT
{
    const mac_address_str_t      nrf52_mac_addr         = { .str_buf = "AA:CC:EE:00:11:22" };
    const uint32_t               uptime                 = 123;
    const bool                   is_wifi                = true;
    const uint32_t               network_disconnect_cnt = 3;
    const uint32_t               nonce                  = 1234567;
    const std::array<uint8_t, 1> data                   = { 0xAAU };
    adv_report_table_t           adv_table              = { .num_of_advs = 4,
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

    this->m_malloc_fail_on_cnt                  = 7;
    const http_json_statistics_info_t stat_info = {
        .nrf52_mac_addr         = nrf52_mac_addr,
        .esp_fw                 = { "1.9.0" },
        .nrf_fw                 = { "0.7.1" },
        .uptime                 = uptime,
        .nonce                  = nonce,
        .is_connected_to_wifi   = is_wifi,
        .network_disconnect_cnt = network_disconnect_cnt,
    };
    ASSERT_FALSE(http_json_create_status_str(&stat_info, &adv_table, &this->m_json_str));
    ASSERT_EQ(nullptr, this->m_json_str.p_str);
    ASSERT_TRUE(this->m_mem_alloc_trace.is_empty());
}

TEST_F(TestHttpJson, test_create_status_json_str_malloc_failed_8_of_50) // NOLINT
{
    const mac_address_str_t      nrf52_mac_addr         = { .str_buf = "AA:CC:EE:00:11:22" };
    const uint32_t               uptime                 = 123;
    const bool                   is_wifi                = true;
    const uint32_t               network_disconnect_cnt = 3;
    const uint32_t               nonce                  = 1234567;
    const std::array<uint8_t, 1> data                   = { 0xAAU };
    adv_report_table_t           adv_table              = { .num_of_advs = 4,
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

    this->m_malloc_fail_on_cnt                  = 8;
    const http_json_statistics_info_t stat_info = {
        .nrf52_mac_addr         = nrf52_mac_addr,
        .esp_fw                 = { "1.9.0" },
        .nrf_fw                 = { "0.7.1" },
        .uptime                 = uptime,
        .nonce                  = nonce,
        .is_connected_to_wifi   = is_wifi,
        .network_disconnect_cnt = network_disconnect_cnt,
    };
    ASSERT_FALSE(http_json_create_status_str(&stat_info, &adv_table, &this->m_json_str));
    ASSERT_EQ(nullptr, this->m_json_str.p_str);
    ASSERT_TRUE(this->m_mem_alloc_trace.is_empty());
}

TEST_F(TestHttpJson, test_create_status_json_str_malloc_failed_9_of_50) // NOLINT
{
    const mac_address_str_t      nrf52_mac_addr         = { .str_buf = "AA:CC:EE:00:11:22" };
    const uint32_t               uptime                 = 123;
    const bool                   is_wifi                = true;
    const uint32_t               network_disconnect_cnt = 3;
    const uint32_t               nonce                  = 1234567;
    const std::array<uint8_t, 1> data                   = { 0xAAU };
    adv_report_table_t           adv_table              = { .num_of_advs = 4,
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

    this->m_malloc_fail_on_cnt                  = 9;
    const http_json_statistics_info_t stat_info = {
        .nrf52_mac_addr         = nrf52_mac_addr,
        .esp_fw                 = { "1.9.0" },
        .nrf_fw                 = { "0.7.1" },
        .uptime                 = uptime,
        .nonce                  = nonce,
        .is_connected_to_wifi   = is_wifi,
        .network_disconnect_cnt = network_disconnect_cnt,
    };
    ASSERT_FALSE(http_json_create_status_str(&stat_info, &adv_table, &this->m_json_str));
    ASSERT_EQ(nullptr, this->m_json_str.p_str);
    ASSERT_TRUE(this->m_mem_alloc_trace.is_empty());
}

TEST_F(TestHttpJson, test_create_status_json_str_malloc_failed_10_of_50) // NOLINT
{
    const mac_address_str_t      nrf52_mac_addr         = { .str_buf = "AA:CC:EE:00:11:22" };
    const uint32_t               uptime                 = 123;
    const bool                   is_wifi                = true;
    const uint32_t               network_disconnect_cnt = 3;
    const uint32_t               nonce                  = 1234567;
    const std::array<uint8_t, 1> data                   = { 0xAAU };
    adv_report_table_t           adv_table              = { .num_of_advs = 4,
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

    this->m_malloc_fail_on_cnt                  = 10;
    const http_json_statistics_info_t stat_info = {
        .nrf52_mac_addr         = nrf52_mac_addr,
        .esp_fw                 = { "1.9.0" },
        .nrf_fw                 = { "0.7.1" },
        .uptime                 = uptime,
        .nonce                  = nonce,
        .is_connected_to_wifi   = is_wifi,
        .network_disconnect_cnt = network_disconnect_cnt,
    };
    ASSERT_FALSE(http_json_create_status_str(&stat_info, &adv_table, &this->m_json_str));
    ASSERT_EQ(nullptr, this->m_json_str.p_str);
    ASSERT_TRUE(this->m_mem_alloc_trace.is_empty());
}

TEST_F(TestHttpJson, test_create_status_json_str_malloc_failed_11_of_50) // NOLINT
{
    const mac_address_str_t      nrf52_mac_addr         = { .str_buf = "AA:CC:EE:00:11:22" };
    const uint32_t               uptime                 = 123;
    const bool                   is_wifi                = true;
    const uint32_t               network_disconnect_cnt = 3;
    const uint32_t               nonce                  = 1234567;
    const std::array<uint8_t, 1> data                   = { 0xAAU };
    adv_report_table_t           adv_table              = { .num_of_advs = 4,
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

    this->m_malloc_fail_on_cnt                  = 11;
    const http_json_statistics_info_t stat_info = {
        .nrf52_mac_addr         = nrf52_mac_addr,
        .esp_fw                 = { "1.9.0" },
        .nrf_fw                 = { "0.7.1" },
        .uptime                 = uptime,
        .nonce                  = nonce,
        .is_connected_to_wifi   = is_wifi,
        .network_disconnect_cnt = network_disconnect_cnt,
    };
    ASSERT_FALSE(http_json_create_status_str(&stat_info, &adv_table, &this->m_json_str));
    ASSERT_EQ(nullptr, this->m_json_str.p_str);
    ASSERT_TRUE(this->m_mem_alloc_trace.is_empty());
}

TEST_F(TestHttpJson, test_create_status_json_str_malloc_failed_12_of_50) // NOLINT
{
    const mac_address_str_t      nrf52_mac_addr         = { .str_buf = "AA:CC:EE:00:11:22" };
    const uint32_t               uptime                 = 123;
    const bool                   is_wifi                = true;
    const uint32_t               network_disconnect_cnt = 3;
    const uint32_t               nonce                  = 1234567;
    const std::array<uint8_t, 1> data                   = { 0xAAU };
    adv_report_table_t           adv_table              = { .num_of_advs = 4,
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

    this->m_malloc_fail_on_cnt                  = 12;
    const http_json_statistics_info_t stat_info = {
        .nrf52_mac_addr         = nrf52_mac_addr,
        .esp_fw                 = { "1.9.0" },
        .nrf_fw                 = { "0.7.1" },
        .uptime                 = uptime,
        .nonce                  = nonce,
        .is_connected_to_wifi   = is_wifi,
        .network_disconnect_cnt = network_disconnect_cnt,
    };
    ASSERT_FALSE(http_json_create_status_str(&stat_info, &adv_table, &this->m_json_str));
    ASSERT_EQ(nullptr, this->m_json_str.p_str);
    ASSERT_TRUE(this->m_mem_alloc_trace.is_empty());
}

TEST_F(TestHttpJson, test_create_status_json_str_malloc_failed_13_of_50) // NOLINT
{
    const mac_address_str_t      nrf52_mac_addr         = { .str_buf = "AA:CC:EE:00:11:22" };
    const uint32_t               uptime                 = 123;
    const bool                   is_wifi                = true;
    const uint32_t               network_disconnect_cnt = 3;
    const uint32_t               nonce                  = 1234567;
    const std::array<uint8_t, 1> data                   = { 0xAAU };
    adv_report_table_t           adv_table              = { .num_of_advs = 4,
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

    this->m_malloc_fail_on_cnt                  = 13;
    const http_json_statistics_info_t stat_info = {
        .nrf52_mac_addr         = nrf52_mac_addr,
        .esp_fw                 = { "1.9.0" },
        .nrf_fw                 = { "0.7.1" },
        .uptime                 = uptime,
        .nonce                  = nonce,
        .is_connected_to_wifi   = is_wifi,
        .network_disconnect_cnt = network_disconnect_cnt,
    };
    ASSERT_FALSE(http_json_create_status_str(&stat_info, &adv_table, &this->m_json_str));
    ASSERT_EQ(nullptr, this->m_json_str.p_str);
    ASSERT_TRUE(this->m_mem_alloc_trace.is_empty());
}

TEST_F(TestHttpJson, test_create_status_json_str_malloc_failed_14_of_50) // NOLINT
{
    const mac_address_str_t      nrf52_mac_addr         = { .str_buf = "AA:CC:EE:00:11:22" };
    const uint32_t               uptime                 = 123;
    const bool                   is_wifi                = true;
    const uint32_t               network_disconnect_cnt = 3;
    const uint32_t               nonce                  = 1234567;
    const std::array<uint8_t, 1> data                   = { 0xAAU };
    adv_report_table_t           adv_table              = { .num_of_advs = 4,
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

    this->m_malloc_fail_on_cnt                  = 14;
    const http_json_statistics_info_t stat_info = {
        .nrf52_mac_addr         = nrf52_mac_addr,
        .esp_fw                 = { "1.9.0" },
        .nrf_fw                 = { "0.7.1" },
        .uptime                 = uptime,
        .nonce                  = nonce,
        .is_connected_to_wifi   = is_wifi,
        .network_disconnect_cnt = network_disconnect_cnt,
    };
    ASSERT_FALSE(http_json_create_status_str(&stat_info, &adv_table, &this->m_json_str));
    ASSERT_EQ(nullptr, this->m_json_str.p_str);
    ASSERT_TRUE(this->m_mem_alloc_trace.is_empty());
}

TEST_F(TestHttpJson, test_create_status_json_str_malloc_failed_15_of_50) // NOLINT
{
    const mac_address_str_t      nrf52_mac_addr         = { .str_buf = "AA:CC:EE:00:11:22" };
    const uint32_t               uptime                 = 123;
    const bool                   is_wifi                = true;
    const uint32_t               network_disconnect_cnt = 3;
    const uint32_t               nonce                  = 1234567;
    const std::array<uint8_t, 1> data                   = { 0xAAU };
    adv_report_table_t           adv_table              = { .num_of_advs = 4,
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

    this->m_malloc_fail_on_cnt                  = 15;
    const http_json_statistics_info_t stat_info = {
        .nrf52_mac_addr         = nrf52_mac_addr,
        .esp_fw                 = { "1.9.0" },
        .nrf_fw                 = { "0.7.1" },
        .uptime                 = uptime,
        .nonce                  = nonce,
        .is_connected_to_wifi   = is_wifi,
        .network_disconnect_cnt = network_disconnect_cnt,
    };
    ASSERT_FALSE(http_json_create_status_str(&stat_info, &adv_table, &this->m_json_str));
    ASSERT_EQ(nullptr, this->m_json_str.p_str);
    ASSERT_TRUE(this->m_mem_alloc_trace.is_empty());
}

TEST_F(TestHttpJson, test_create_status_json_str_malloc_failed_16_of_50) // NOLINT
{
    const mac_address_str_t      nrf52_mac_addr         = { .str_buf = "AA:CC:EE:00:11:22" };
    const uint32_t               uptime                 = 123;
    const bool                   is_wifi                = true;
    const uint32_t               network_disconnect_cnt = 3;
    const uint32_t               nonce                  = 1234567;
    const std::array<uint8_t, 1> data                   = { 0xAAU };
    adv_report_table_t           adv_table              = { .num_of_advs = 4,
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

    this->m_malloc_fail_on_cnt                  = 16;
    const http_json_statistics_info_t stat_info = {
        .nrf52_mac_addr         = nrf52_mac_addr,
        .esp_fw                 = { "1.9.0" },
        .nrf_fw                 = { "0.7.1" },
        .uptime                 = uptime,
        .nonce                  = nonce,
        .is_connected_to_wifi   = is_wifi,
        .network_disconnect_cnt = network_disconnect_cnt,
    };
    ASSERT_FALSE(http_json_create_status_str(&stat_info, &adv_table, &this->m_json_str));
    ASSERT_EQ(nullptr, this->m_json_str.p_str);
    ASSERT_TRUE(this->m_mem_alloc_trace.is_empty());
}

TEST_F(TestHttpJson, test_create_status_json_str_malloc_failed_17_of_50) // NOLINT
{
    const mac_address_str_t      nrf52_mac_addr         = { .str_buf = "AA:CC:EE:00:11:22" };
    const uint32_t               uptime                 = 123;
    const bool                   is_wifi                = true;
    const uint32_t               network_disconnect_cnt = 3;
    const uint32_t               nonce                  = 1234567;
    const std::array<uint8_t, 1> data                   = { 0xAAU };
    adv_report_table_t           adv_table              = { .num_of_advs = 4,
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

    this->m_malloc_fail_on_cnt                  = 17;
    const http_json_statistics_info_t stat_info = {
        .nrf52_mac_addr         = nrf52_mac_addr,
        .esp_fw                 = { "1.9.0" },
        .nrf_fw                 = { "0.7.1" },
        .uptime                 = uptime,
        .nonce                  = nonce,
        .is_connected_to_wifi   = is_wifi,
        .network_disconnect_cnt = network_disconnect_cnt,
    };
    ASSERT_FALSE(http_json_create_status_str(&stat_info, &adv_table, &this->m_json_str));
    ASSERT_EQ(nullptr, this->m_json_str.p_str);
    ASSERT_TRUE(this->m_mem_alloc_trace.is_empty());
}

TEST_F(TestHttpJson, test_create_status_json_str_malloc_failed_18_of_50) // NOLINT
{
    const mac_address_str_t      nrf52_mac_addr         = { .str_buf = "AA:CC:EE:00:11:22" };
    const uint32_t               uptime                 = 123;
    const bool                   is_wifi                = true;
    const uint32_t               network_disconnect_cnt = 3;
    const uint32_t               nonce                  = 1234567;
    const std::array<uint8_t, 1> data                   = { 0xAAU };
    adv_report_table_t           adv_table              = { .num_of_advs = 4,
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

    this->m_malloc_fail_on_cnt                  = 18;
    const http_json_statistics_info_t stat_info = {
        .nrf52_mac_addr         = nrf52_mac_addr,
        .esp_fw                 = { "1.9.0" },
        .nrf_fw                 = { "0.7.1" },
        .uptime                 = uptime,
        .nonce                  = nonce,
        .is_connected_to_wifi   = is_wifi,
        .network_disconnect_cnt = network_disconnect_cnt,
    };
    ASSERT_FALSE(http_json_create_status_str(&stat_info, &adv_table, &this->m_json_str));
    ASSERT_EQ(nullptr, this->m_json_str.p_str);
    ASSERT_TRUE(this->m_mem_alloc_trace.is_empty());
}

TEST_F(TestHttpJson, test_create_status_json_str_malloc_failed_19_of_50) // NOLINT
{
    const mac_address_str_t      nrf52_mac_addr         = { .str_buf = "AA:CC:EE:00:11:22" };
    const uint32_t               uptime                 = 123;
    const bool                   is_wifi                = true;
    const uint32_t               network_disconnect_cnt = 3;
    const uint32_t               nonce                  = 1234567;
    const std::array<uint8_t, 1> data                   = { 0xAAU };
    adv_report_table_t           adv_table              = { .num_of_advs = 4,
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

    this->m_malloc_fail_on_cnt                  = 19;
    const http_json_statistics_info_t stat_info = {
        .nrf52_mac_addr         = nrf52_mac_addr,
        .esp_fw                 = { "1.9.0" },
        .nrf_fw                 = { "0.7.1" },
        .uptime                 = uptime,
        .nonce                  = nonce,
        .is_connected_to_wifi   = is_wifi,
        .network_disconnect_cnt = network_disconnect_cnt,
    };
    ASSERT_FALSE(http_json_create_status_str(&stat_info, &adv_table, &this->m_json_str));
    ASSERT_EQ(nullptr, this->m_json_str.p_str);
    ASSERT_TRUE(this->m_mem_alloc_trace.is_empty());
}

TEST_F(TestHttpJson, test_create_status_json_str_malloc_failed_20_of_50) // NOLINT
{
    const mac_address_str_t      nrf52_mac_addr         = { .str_buf = "AA:CC:EE:00:11:22" };
    const uint32_t               uptime                 = 123;
    const bool                   is_wifi                = true;
    const uint32_t               network_disconnect_cnt = 3;
    const uint32_t               nonce                  = 1234567;
    const std::array<uint8_t, 1> data                   = { 0xAAU };
    adv_report_table_t           adv_table              = { .num_of_advs = 4,
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

    this->m_malloc_fail_on_cnt                  = 20;
    const http_json_statistics_info_t stat_info = {
        .nrf52_mac_addr         = nrf52_mac_addr,
        .esp_fw                 = { "1.9.0" },
        .nrf_fw                 = { "0.7.1" },
        .uptime                 = uptime,
        .nonce                  = nonce,
        .is_connected_to_wifi   = is_wifi,
        .network_disconnect_cnt = network_disconnect_cnt,
    };
    ASSERT_FALSE(http_json_create_status_str(&stat_info, &adv_table, &this->m_json_str));
    ASSERT_EQ(nullptr, this->m_json_str.p_str);
    ASSERT_TRUE(this->m_mem_alloc_trace.is_empty());
}

TEST_F(TestHttpJson, test_create_status_json_str_malloc_failed_21_of_50) // NOLINT
{
    const mac_address_str_t      nrf52_mac_addr         = { .str_buf = "AA:CC:EE:00:11:22" };
    const uint32_t               uptime                 = 123;
    const bool                   is_wifi                = true;
    const uint32_t               network_disconnect_cnt = 3;
    const uint32_t               nonce                  = 1234567;
    const std::array<uint8_t, 1> data                   = { 0xAAU };
    adv_report_table_t           adv_table              = { .num_of_advs = 4,
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

    this->m_malloc_fail_on_cnt                  = 21;
    const http_json_statistics_info_t stat_info = {
        .nrf52_mac_addr         = nrf52_mac_addr,
        .esp_fw                 = { "1.9.0" },
        .nrf_fw                 = { "0.7.1" },
        .uptime                 = uptime,
        .nonce                  = nonce,
        .is_connected_to_wifi   = is_wifi,
        .network_disconnect_cnt = network_disconnect_cnt,
    };
    ASSERT_FALSE(http_json_create_status_str(&stat_info, &adv_table, &this->m_json_str));
    ASSERT_EQ(nullptr, this->m_json_str.p_str);
    ASSERT_TRUE(this->m_mem_alloc_trace.is_empty());
}

TEST_F(TestHttpJson, test_create_status_json_str_malloc_failed_22_of_50) // NOLINT
{
    const mac_address_str_t      nrf52_mac_addr         = { .str_buf = "AA:CC:EE:00:11:22" };
    const uint32_t               uptime                 = 123;
    const bool                   is_wifi                = true;
    const uint32_t               network_disconnect_cnt = 3;
    const uint32_t               nonce                  = 1234567;
    const std::array<uint8_t, 1> data                   = { 0xAAU };
    adv_report_table_t           adv_table              = { .num_of_advs = 4,
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

    this->m_malloc_fail_on_cnt                  = 22;
    const http_json_statistics_info_t stat_info = {
        .nrf52_mac_addr         = nrf52_mac_addr,
        .esp_fw                 = { "1.9.0" },
        .nrf_fw                 = { "0.7.1" },
        .uptime                 = uptime,
        .nonce                  = nonce,
        .is_connected_to_wifi   = is_wifi,
        .network_disconnect_cnt = network_disconnect_cnt,
    };
    ASSERT_FALSE(http_json_create_status_str(&stat_info, &adv_table, &this->m_json_str));
    ASSERT_EQ(nullptr, this->m_json_str.p_str);
    ASSERT_TRUE(this->m_mem_alloc_trace.is_empty());
}

TEST_F(TestHttpJson, test_create_status_json_str_malloc_failed_23_of_50) // NOLINT
{
    const mac_address_str_t      nrf52_mac_addr         = { .str_buf = "AA:CC:EE:00:11:22" };
    const uint32_t               uptime                 = 123;
    const bool                   is_wifi                = true;
    const uint32_t               network_disconnect_cnt = 3;
    const uint32_t               nonce                  = 1234567;
    const std::array<uint8_t, 1> data                   = { 0xAAU };
    adv_report_table_t           adv_table              = { .num_of_advs = 4,
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

    this->m_malloc_fail_on_cnt                  = 23;
    const http_json_statistics_info_t stat_info = {
        .nrf52_mac_addr         = nrf52_mac_addr,
        .esp_fw                 = { "1.9.0" },
        .nrf_fw                 = { "0.7.1" },
        .uptime                 = uptime,
        .nonce                  = nonce,
        .is_connected_to_wifi   = is_wifi,
        .network_disconnect_cnt = network_disconnect_cnt,
    };
    ASSERT_FALSE(http_json_create_status_str(&stat_info, &adv_table, &this->m_json_str));
    ASSERT_EQ(nullptr, this->m_json_str.p_str);
    ASSERT_TRUE(this->m_mem_alloc_trace.is_empty());
}

TEST_F(TestHttpJson, test_create_status_json_str_malloc_failed_24_of_50) // NOLINT
{
    const mac_address_str_t      nrf52_mac_addr         = { .str_buf = "AA:CC:EE:00:11:22" };
    const uint32_t               uptime                 = 123;
    const bool                   is_wifi                = true;
    const uint32_t               network_disconnect_cnt = 3;
    const uint32_t               nonce                  = 1234567;
    const std::array<uint8_t, 1> data                   = { 0xAAU };
    adv_report_table_t           adv_table              = { .num_of_advs = 4,
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

    this->m_malloc_fail_on_cnt                  = 24;
    const http_json_statistics_info_t stat_info = {
        .nrf52_mac_addr         = nrf52_mac_addr,
        .esp_fw                 = { "1.9.0" },
        .nrf_fw                 = { "0.7.1" },
        .uptime                 = uptime,
        .nonce                  = nonce,
        .is_connected_to_wifi   = is_wifi,
        .network_disconnect_cnt = network_disconnect_cnt,
    };
    ASSERT_FALSE(http_json_create_status_str(&stat_info, &adv_table, &this->m_json_str));
    ASSERT_EQ(nullptr, this->m_json_str.p_str);
    ASSERT_TRUE(this->m_mem_alloc_trace.is_empty());
}

TEST_F(TestHttpJson, test_create_status_json_str_malloc_failed_25_of_50) // NOLINT
{
    const mac_address_str_t      nrf52_mac_addr         = { .str_buf = "AA:CC:EE:00:11:22" };
    const uint32_t               uptime                 = 123;
    const bool                   is_wifi                = true;
    const uint32_t               network_disconnect_cnt = 3;
    const uint32_t               nonce                  = 1234567;
    const std::array<uint8_t, 1> data                   = { 0xAAU };
    adv_report_table_t           adv_table              = { .num_of_advs = 4,
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

    this->m_malloc_fail_on_cnt                  = 25;
    const http_json_statistics_info_t stat_info = {
        .nrf52_mac_addr         = nrf52_mac_addr,
        .esp_fw                 = { "1.9.0" },
        .nrf_fw                 = { "0.7.1" },
        .uptime                 = uptime,
        .nonce                  = nonce,
        .is_connected_to_wifi   = is_wifi,
        .network_disconnect_cnt = network_disconnect_cnt,
    };
    ASSERT_FALSE(http_json_create_status_str(&stat_info, &adv_table, &this->m_json_str));
    ASSERT_EQ(nullptr, this->m_json_str.p_str);
    ASSERT_TRUE(this->m_mem_alloc_trace.is_empty());
}

TEST_F(TestHttpJson, test_create_status_json_str_malloc_failed_26_of_50) // NOLINT
{
    const mac_address_str_t      nrf52_mac_addr         = { .str_buf = "AA:CC:EE:00:11:22" };
    const uint32_t               uptime                 = 123;
    const bool                   is_wifi                = true;
    const uint32_t               network_disconnect_cnt = 3;
    const uint32_t               nonce                  = 1234567;
    const std::array<uint8_t, 1> data                   = { 0xAAU };
    adv_report_table_t           adv_table              = { .num_of_advs = 4,
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

    this->m_malloc_fail_on_cnt                  = 26;
    const http_json_statistics_info_t stat_info = {
        .nrf52_mac_addr         = nrf52_mac_addr,
        .esp_fw                 = { "1.9.0" },
        .nrf_fw                 = { "0.7.1" },
        .uptime                 = uptime,
        .nonce                  = nonce,
        .is_connected_to_wifi   = is_wifi,
        .network_disconnect_cnt = network_disconnect_cnt,
    };
    ASSERT_FALSE(http_json_create_status_str(&stat_info, &adv_table, &this->m_json_str));
    ASSERT_EQ(nullptr, this->m_json_str.p_str);
    ASSERT_TRUE(this->m_mem_alloc_trace.is_empty());
}

TEST_F(TestHttpJson, test_create_status_json_str_malloc_failed_27_of_50) // NOLINT
{
    const mac_address_str_t      nrf52_mac_addr         = { .str_buf = "AA:CC:EE:00:11:22" };
    const uint32_t               uptime                 = 123;
    const bool                   is_wifi                = true;
    const uint32_t               network_disconnect_cnt = 3;
    const uint32_t               nonce                  = 1234567;
    const std::array<uint8_t, 1> data                   = { 0xAAU };
    adv_report_table_t           adv_table              = { .num_of_advs = 4,
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

    this->m_malloc_fail_on_cnt                  = 27;
    const http_json_statistics_info_t stat_info = {
        .nrf52_mac_addr         = nrf52_mac_addr,
        .esp_fw                 = { "1.9.0" },
        .nrf_fw                 = { "0.7.1" },
        .uptime                 = uptime,
        .nonce                  = nonce,
        .is_connected_to_wifi   = is_wifi,
        .network_disconnect_cnt = network_disconnect_cnt,
    };
    ASSERT_FALSE(http_json_create_status_str(&stat_info, &adv_table, &this->m_json_str));
    ASSERT_EQ(nullptr, this->m_json_str.p_str);
    ASSERT_TRUE(this->m_mem_alloc_trace.is_empty());
}

TEST_F(TestHttpJson, test_create_status_json_str_malloc_failed_28_of_50) // NOLINT
{
    const mac_address_str_t      nrf52_mac_addr         = { .str_buf = "AA:CC:EE:00:11:22" };
    const uint32_t               uptime                 = 123;
    const bool                   is_wifi                = true;
    const uint32_t               network_disconnect_cnt = 3;
    const uint32_t               nonce                  = 1234567;
    const std::array<uint8_t, 1> data                   = { 0xAAU };
    adv_report_table_t           adv_table              = { .num_of_advs = 4,
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

    this->m_malloc_fail_on_cnt                  = 28;
    const http_json_statistics_info_t stat_info = {
        .nrf52_mac_addr         = nrf52_mac_addr,
        .esp_fw                 = { "1.9.0" },
        .nrf_fw                 = { "0.7.1" },
        .uptime                 = uptime,
        .nonce                  = nonce,
        .is_connected_to_wifi   = is_wifi,
        .network_disconnect_cnt = network_disconnect_cnt,
    };
    ASSERT_FALSE(http_json_create_status_str(&stat_info, &adv_table, &this->m_json_str));
    ASSERT_EQ(nullptr, this->m_json_str.p_str);
    ASSERT_TRUE(this->m_mem_alloc_trace.is_empty());
}

TEST_F(TestHttpJson, test_create_status_json_str_malloc_failed_29_of_50) // NOLINT
{
    const mac_address_str_t      nrf52_mac_addr         = { .str_buf = "AA:CC:EE:00:11:22" };
    const uint32_t               uptime                 = 123;
    const bool                   is_wifi                = true;
    const uint32_t               network_disconnect_cnt = 3;
    const uint32_t               nonce                  = 1234567;
    const std::array<uint8_t, 1> data                   = { 0xAAU };
    adv_report_table_t           adv_table              = { .num_of_advs = 4,
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

    this->m_malloc_fail_on_cnt                  = 29;
    const http_json_statistics_info_t stat_info = {
        .nrf52_mac_addr         = nrf52_mac_addr,
        .esp_fw                 = { "1.9.0" },
        .nrf_fw                 = { "0.7.1" },
        .uptime                 = uptime,
        .nonce                  = nonce,
        .is_connected_to_wifi   = is_wifi,
        .network_disconnect_cnt = network_disconnect_cnt,
    };
    ASSERT_FALSE(http_json_create_status_str(&stat_info, &adv_table, &this->m_json_str));
    ASSERT_EQ(nullptr, this->m_json_str.p_str);
    ASSERT_TRUE(this->m_mem_alloc_trace.is_empty());
}

TEST_F(TestHttpJson, test_create_status_json_str_malloc_failed_30_of_50) // NOLINT
{
    const mac_address_str_t      nrf52_mac_addr         = { .str_buf = "AA:CC:EE:00:11:22" };
    const uint32_t               uptime                 = 123;
    const bool                   is_wifi                = true;
    const uint32_t               network_disconnect_cnt = 3;
    const uint32_t               nonce                  = 1234567;
    const std::array<uint8_t, 1> data                   = { 0xAAU };
    adv_report_table_t           adv_table              = { .num_of_advs = 4,
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

    this->m_malloc_fail_on_cnt                  = 30;
    const http_json_statistics_info_t stat_info = {
        .nrf52_mac_addr         = nrf52_mac_addr,
        .esp_fw                 = { "1.9.0" },
        .nrf_fw                 = { "0.7.1" },
        .uptime                 = uptime,
        .nonce                  = nonce,
        .is_connected_to_wifi   = is_wifi,
        .network_disconnect_cnt = network_disconnect_cnt,
    };
    ASSERT_FALSE(http_json_create_status_str(&stat_info, &adv_table, &this->m_json_str));
    ASSERT_EQ(nullptr, this->m_json_str.p_str);
    ASSERT_TRUE(this->m_mem_alloc_trace.is_empty());
}

TEST_F(TestHttpJson, test_create_status_json_str_malloc_failed_31_of_50) // NOLINT
{
    const mac_address_str_t      nrf52_mac_addr         = { .str_buf = "AA:CC:EE:00:11:22" };
    const uint32_t               uptime                 = 123;
    const bool                   is_wifi                = true;
    const uint32_t               network_disconnect_cnt = 3;
    const uint32_t               nonce                  = 1234567;
    const std::array<uint8_t, 1> data                   = { 0xAAU };
    adv_report_table_t           adv_table              = { .num_of_advs = 4,
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

    this->m_malloc_fail_on_cnt                  = 31;
    const http_json_statistics_info_t stat_info = {
        .nrf52_mac_addr         = nrf52_mac_addr,
        .esp_fw                 = { "1.9.0" },
        .nrf_fw                 = { "0.7.1" },
        .uptime                 = uptime,
        .nonce                  = nonce,
        .is_connected_to_wifi   = is_wifi,
        .network_disconnect_cnt = network_disconnect_cnt,
    };
    ASSERT_FALSE(http_json_create_status_str(&stat_info, &adv_table, &this->m_json_str));
    ASSERT_EQ(nullptr, this->m_json_str.p_str);
    ASSERT_TRUE(this->m_mem_alloc_trace.is_empty());
}

TEST_F(TestHttpJson, test_create_status_json_str_malloc_failed_32_of_50) // NOLINT
{
    const mac_address_str_t      nrf52_mac_addr         = { .str_buf = "AA:CC:EE:00:11:22" };
    const uint32_t               uptime                 = 123;
    const bool                   is_wifi                = true;
    const uint32_t               network_disconnect_cnt = 3;
    const uint32_t               nonce                  = 1234567;
    const std::array<uint8_t, 1> data                   = { 0xAAU };
    adv_report_table_t           adv_table              = { .num_of_advs = 4,
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

    this->m_malloc_fail_on_cnt                  = 32;
    const http_json_statistics_info_t stat_info = {
        .nrf52_mac_addr         = nrf52_mac_addr,
        .esp_fw                 = { "1.9.0" },
        .nrf_fw                 = { "0.7.1" },
        .uptime                 = uptime,
        .nonce                  = nonce,
        .is_connected_to_wifi   = is_wifi,
        .network_disconnect_cnt = network_disconnect_cnt,
    };
    ASSERT_FALSE(http_json_create_status_str(&stat_info, &adv_table, &this->m_json_str));
    ASSERT_EQ(nullptr, this->m_json_str.p_str);
    ASSERT_TRUE(this->m_mem_alloc_trace.is_empty());
}

TEST_F(TestHttpJson, test_create_status_json_str_malloc_failed_33_of_50) // NOLINT
{
    const mac_address_str_t      nrf52_mac_addr         = { .str_buf = "AA:CC:EE:00:11:22" };
    const uint32_t               uptime                 = 123;
    const bool                   is_wifi                = true;
    const uint32_t               network_disconnect_cnt = 3;
    const uint32_t               nonce                  = 1234567;
    const std::array<uint8_t, 1> data                   = { 0xAAU };
    adv_report_table_t           adv_table              = { .num_of_advs = 4,
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

    this->m_malloc_fail_on_cnt                  = 33;
    const http_json_statistics_info_t stat_info = {
        .nrf52_mac_addr         = nrf52_mac_addr,
        .esp_fw                 = { "1.9.0" },
        .nrf_fw                 = { "0.7.1" },
        .uptime                 = uptime,
        .nonce                  = nonce,
        .is_connected_to_wifi   = is_wifi,
        .network_disconnect_cnt = network_disconnect_cnt,
    };
    ASSERT_FALSE(http_json_create_status_str(&stat_info, &adv_table, &this->m_json_str));
    ASSERT_EQ(nullptr, this->m_json_str.p_str);
    ASSERT_TRUE(this->m_mem_alloc_trace.is_empty());
}

TEST_F(TestHttpJson, test_create_status_json_str_malloc_failed_34_of_50) // NOLINT
{
    const mac_address_str_t      nrf52_mac_addr         = { .str_buf = "AA:CC:EE:00:11:22" };
    const uint32_t               uptime                 = 123;
    const bool                   is_wifi                = true;
    const uint32_t               network_disconnect_cnt = 3;
    const uint32_t               nonce                  = 1234567;
    const std::array<uint8_t, 1> data                   = { 0xAAU };
    adv_report_table_t           adv_table              = { .num_of_advs = 4,
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

    this->m_malloc_fail_on_cnt                  = 34;
    const http_json_statistics_info_t stat_info = {
        .nrf52_mac_addr         = nrf52_mac_addr,
        .esp_fw                 = { "1.9.0" },
        .nrf_fw                 = { "0.7.1" },
        .uptime                 = uptime,
        .nonce                  = nonce,
        .is_connected_to_wifi   = is_wifi,
        .network_disconnect_cnt = network_disconnect_cnt,
    };
    ASSERT_FALSE(http_json_create_status_str(&stat_info, &adv_table, &this->m_json_str));
    ASSERT_EQ(nullptr, this->m_json_str.p_str);
    ASSERT_TRUE(this->m_mem_alloc_trace.is_empty());
}

TEST_F(TestHttpJson, test_create_status_json_str_malloc_failed_35_of_50) // NOLINT
{
    const mac_address_str_t      nrf52_mac_addr         = { .str_buf = "AA:CC:EE:00:11:22" };
    const uint32_t               uptime                 = 123;
    const bool                   is_wifi                = true;
    const uint32_t               network_disconnect_cnt = 3;
    const uint32_t               nonce                  = 1234567;
    const std::array<uint8_t, 1> data                   = { 0xAAU };
    adv_report_table_t           adv_table              = { .num_of_advs = 4,
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

    this->m_malloc_fail_on_cnt                  = 35;
    const http_json_statistics_info_t stat_info = {
        .nrf52_mac_addr         = nrf52_mac_addr,
        .esp_fw                 = { "1.9.0" },
        .nrf_fw                 = { "0.7.1" },
        .uptime                 = uptime,
        .nonce                  = nonce,
        .is_connected_to_wifi   = is_wifi,
        .network_disconnect_cnt = network_disconnect_cnt,
    };
    ASSERT_FALSE(http_json_create_status_str(&stat_info, &adv_table, &this->m_json_str));
    ASSERT_EQ(nullptr, this->m_json_str.p_str);
    ASSERT_TRUE(this->m_mem_alloc_trace.is_empty());
}

TEST_F(TestHttpJson, test_create_status_json_str_malloc_failed_36_of_50) // NOLINT
{
    const mac_address_str_t      nrf52_mac_addr         = { .str_buf = "AA:CC:EE:00:11:22" };
    const uint32_t               uptime                 = 123;
    const bool                   is_wifi                = true;
    const uint32_t               network_disconnect_cnt = 3;
    const uint32_t               nonce                  = 1234567;
    const std::array<uint8_t, 1> data                   = { 0xAAU };
    adv_report_table_t           adv_table              = { .num_of_advs = 4,
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

    this->m_malloc_fail_on_cnt                  = 36;
    const http_json_statistics_info_t stat_info = {
        .nrf52_mac_addr         = nrf52_mac_addr,
        .esp_fw                 = { "1.9.0" },
        .nrf_fw                 = { "0.7.1" },
        .uptime                 = uptime,
        .nonce                  = nonce,
        .is_connected_to_wifi   = is_wifi,
        .network_disconnect_cnt = network_disconnect_cnt,
    };
    ASSERT_FALSE(http_json_create_status_str(&stat_info, &adv_table, &this->m_json_str));
    ASSERT_EQ(nullptr, this->m_json_str.p_str);
    ASSERT_TRUE(this->m_mem_alloc_trace.is_empty());
}

TEST_F(TestHttpJson, test_create_status_json_str_malloc_failed_37_of_50) // NOLINT
{
    const mac_address_str_t      nrf52_mac_addr         = { .str_buf = "AA:CC:EE:00:11:22" };
    const uint32_t               uptime                 = 123;
    const bool                   is_wifi                = true;
    const uint32_t               network_disconnect_cnt = 3;
    const uint32_t               nonce                  = 1234567;
    const std::array<uint8_t, 1> data                   = { 0xAAU };
    adv_report_table_t           adv_table              = { .num_of_advs = 4,
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

    this->m_malloc_fail_on_cnt                  = 37;
    const http_json_statistics_info_t stat_info = {
        .nrf52_mac_addr         = nrf52_mac_addr,
        .esp_fw                 = { "1.9.0" },
        .nrf_fw                 = { "0.7.1" },
        .uptime                 = uptime,
        .nonce                  = nonce,
        .is_connected_to_wifi   = is_wifi,
        .network_disconnect_cnt = network_disconnect_cnt,
    };
    ASSERT_FALSE(http_json_create_status_str(&stat_info, &adv_table, &this->m_json_str));
    ASSERT_EQ(nullptr, this->m_json_str.p_str);
    ASSERT_TRUE(this->m_mem_alloc_trace.is_empty());
}

TEST_F(TestHttpJson, test_create_status_json_str_malloc_failed_38_of_50) // NOLINT
{
    const mac_address_str_t      nrf52_mac_addr         = { .str_buf = "AA:CC:EE:00:11:22" };
    const uint32_t               uptime                 = 123;
    const bool                   is_wifi                = true;
    const uint32_t               network_disconnect_cnt = 3;
    const uint32_t               nonce                  = 1234567;
    const std::array<uint8_t, 1> data                   = { 0xAAU };
    adv_report_table_t           adv_table              = { .num_of_advs = 4,
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

    this->m_malloc_fail_on_cnt                  = 38;
    const http_json_statistics_info_t stat_info = {
        .nrf52_mac_addr         = nrf52_mac_addr,
        .esp_fw                 = { "1.9.0" },
        .nrf_fw                 = { "0.7.1" },
        .uptime                 = uptime,
        .nonce                  = nonce,
        .is_connected_to_wifi   = is_wifi,
        .network_disconnect_cnt = network_disconnect_cnt,
    };
    ASSERT_FALSE(http_json_create_status_str(&stat_info, &adv_table, &this->m_json_str));
    ASSERT_EQ(nullptr, this->m_json_str.p_str);
    ASSERT_TRUE(this->m_mem_alloc_trace.is_empty());
}

TEST_F(TestHttpJson, test_create_status_json_str_malloc_failed_39_of_50) // NOLINT
{
    const mac_address_str_t      nrf52_mac_addr         = { .str_buf = "AA:CC:EE:00:11:22" };
    const uint32_t               uptime                 = 123;
    const bool                   is_wifi                = true;
    const uint32_t               network_disconnect_cnt = 3;
    const uint32_t               nonce                  = 1234567;
    const std::array<uint8_t, 1> data                   = { 0xAAU };
    adv_report_table_t           adv_table              = { .num_of_advs = 4,
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

    this->m_malloc_fail_on_cnt                  = 39;
    const http_json_statistics_info_t stat_info = {
        .nrf52_mac_addr         = nrf52_mac_addr,
        .esp_fw                 = { "1.9.0" },
        .nrf_fw                 = { "0.7.1" },
        .uptime                 = uptime,
        .nonce                  = nonce,
        .is_connected_to_wifi   = is_wifi,
        .network_disconnect_cnt = network_disconnect_cnt,
    };
    ASSERT_FALSE(http_json_create_status_str(&stat_info, &adv_table, &this->m_json_str));
    ASSERT_EQ(nullptr, this->m_json_str.p_str);
    ASSERT_TRUE(this->m_mem_alloc_trace.is_empty());
}

TEST_F(TestHttpJson, test_create_status_json_str_malloc_failed_40_of_50) // NOLINT
{
    const mac_address_str_t      nrf52_mac_addr         = { .str_buf = "AA:CC:EE:00:11:22" };
    const uint32_t               uptime                 = 123;
    const bool                   is_wifi                = true;
    const uint32_t               network_disconnect_cnt = 3;
    const uint32_t               nonce                  = 1234567;
    const std::array<uint8_t, 1> data                   = { 0xAAU };
    adv_report_table_t           adv_table              = { .num_of_advs = 4,
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

    this->m_malloc_fail_on_cnt                  = 40;
    const http_json_statistics_info_t stat_info = {
        .nrf52_mac_addr         = nrf52_mac_addr,
        .esp_fw                 = { "1.9.0" },
        .nrf_fw                 = { "0.7.1" },
        .uptime                 = uptime,
        .nonce                  = nonce,
        .is_connected_to_wifi   = is_wifi,
        .network_disconnect_cnt = network_disconnect_cnt,
    };
    ASSERT_FALSE(http_json_create_status_str(&stat_info, &adv_table, &this->m_json_str));
    ASSERT_EQ(nullptr, this->m_json_str.p_str);
    ASSERT_TRUE(this->m_mem_alloc_trace.is_empty());
}

TEST_F(TestHttpJson, test_create_status_json_str_malloc_failed_41_of_50) // NOLINT
{
    const mac_address_str_t      nrf52_mac_addr         = { .str_buf = "AA:CC:EE:00:11:22" };
    const uint32_t               uptime                 = 123;
    const bool                   is_wifi                = true;
    const uint32_t               network_disconnect_cnt = 3;
    const uint32_t               nonce                  = 1234567;
    const std::array<uint8_t, 1> data                   = { 0xAAU };
    adv_report_table_t           adv_table              = { .num_of_advs = 4,
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

    this->m_malloc_fail_on_cnt                  = 41;
    const http_json_statistics_info_t stat_info = {
        .nrf52_mac_addr         = nrf52_mac_addr,
        .esp_fw                 = { "1.9.0" },
        .nrf_fw                 = { "0.7.1" },
        .uptime                 = uptime,
        .nonce                  = nonce,
        .is_connected_to_wifi   = is_wifi,
        .network_disconnect_cnt = network_disconnect_cnt,
    };
    ASSERT_FALSE(http_json_create_status_str(&stat_info, &adv_table, &this->m_json_str));
    ASSERT_EQ(nullptr, this->m_json_str.p_str);
    ASSERT_TRUE(this->m_mem_alloc_trace.is_empty());
}

TEST_F(TestHttpJson, test_create_status_json_str_malloc_failed_42_of_50) // NOLINT
{
    const mac_address_str_t      nrf52_mac_addr         = { .str_buf = "AA:CC:EE:00:11:22" };
    const uint32_t               uptime                 = 123;
    const bool                   is_wifi                = true;
    const uint32_t               network_disconnect_cnt = 3;
    const uint32_t               nonce                  = 1234567;
    const std::array<uint8_t, 1> data                   = { 0xAAU };
    adv_report_table_t           adv_table              = { .num_of_advs = 4,
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

    this->m_malloc_fail_on_cnt                  = 42;
    const http_json_statistics_info_t stat_info = {
        .nrf52_mac_addr         = nrf52_mac_addr,
        .esp_fw                 = { "1.9.0" },
        .nrf_fw                 = { "0.7.1" },
        .uptime                 = uptime,
        .nonce                  = nonce,
        .is_connected_to_wifi   = is_wifi,
        .network_disconnect_cnt = network_disconnect_cnt,
    };
    ASSERT_FALSE(http_json_create_status_str(&stat_info, &adv_table, &this->m_json_str));
    ASSERT_EQ(nullptr, this->m_json_str.p_str);
    ASSERT_TRUE(this->m_mem_alloc_trace.is_empty());
}

TEST_F(TestHttpJson, test_create_status_json_str_malloc_failed_43_of_50) // NOLINT
{
    const mac_address_str_t      nrf52_mac_addr         = { .str_buf = "AA:CC:EE:00:11:22" };
    const uint32_t               uptime                 = 123;
    const bool                   is_wifi                = true;
    const uint32_t               network_disconnect_cnt = 3;
    const uint32_t               nonce                  = 1234567;
    const std::array<uint8_t, 1> data                   = { 0xAAU };
    adv_report_table_t           adv_table              = { .num_of_advs = 4,
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

    this->m_malloc_fail_on_cnt                  = 43;
    const http_json_statistics_info_t stat_info = {
        .nrf52_mac_addr         = nrf52_mac_addr,
        .esp_fw                 = { "1.9.0" },
        .nrf_fw                 = { "0.7.1" },
        .uptime                 = uptime,
        .nonce                  = nonce,
        .is_connected_to_wifi   = is_wifi,
        .network_disconnect_cnt = network_disconnect_cnt,
    };
    ASSERT_FALSE(http_json_create_status_str(&stat_info, &adv_table, &this->m_json_str));
    ASSERT_EQ(nullptr, this->m_json_str.p_str);
    ASSERT_TRUE(this->m_mem_alloc_trace.is_empty());
}

TEST_F(TestHttpJson, test_create_status_json_str_malloc_failed_44_of_50) // NOLINT
{
    const mac_address_str_t      nrf52_mac_addr         = { .str_buf = "AA:CC:EE:00:11:22" };
    const uint32_t               uptime                 = 123;
    const bool                   is_wifi                = true;
    const uint32_t               network_disconnect_cnt = 3;
    const uint32_t               nonce                  = 1234567;
    const std::array<uint8_t, 1> data                   = { 0xAAU };
    adv_report_table_t           adv_table              = { .num_of_advs = 4,
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

    this->m_malloc_fail_on_cnt                  = 44;
    const http_json_statistics_info_t stat_info = {
        .nrf52_mac_addr         = nrf52_mac_addr,
        .esp_fw                 = { "1.9.0" },
        .nrf_fw                 = { "0.7.1" },
        .uptime                 = uptime,
        .nonce                  = nonce,
        .is_connected_to_wifi   = is_wifi,
        .network_disconnect_cnt = network_disconnect_cnt,
    };
    ASSERT_FALSE(http_json_create_status_str(&stat_info, &adv_table, &this->m_json_str));
    ASSERT_EQ(nullptr, this->m_json_str.p_str);
    ASSERT_TRUE(this->m_mem_alloc_trace.is_empty());
}

TEST_F(TestHttpJson, test_create_status_json_str_malloc_failed_45_of_50) // NOLINT
{
    const mac_address_str_t      nrf52_mac_addr         = { .str_buf = "AA:CC:EE:00:11:22" };
    const uint32_t               uptime                 = 123;
    const bool                   is_wifi                = true;
    const uint32_t               network_disconnect_cnt = 3;
    const uint32_t               nonce                  = 1234567;
    const std::array<uint8_t, 1> data                   = { 0xAAU };
    adv_report_table_t           adv_table              = { .num_of_advs = 4,
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

    this->m_malloc_fail_on_cnt                  = 45;
    const http_json_statistics_info_t stat_info = {
        .nrf52_mac_addr         = nrf52_mac_addr,
        .esp_fw                 = { "1.9.0" },
        .nrf_fw                 = { "0.7.1" },
        .uptime                 = uptime,
        .nonce                  = nonce,
        .is_connected_to_wifi   = is_wifi,
        .network_disconnect_cnt = network_disconnect_cnt,
    };
    ASSERT_FALSE(http_json_create_status_str(&stat_info, &adv_table, &this->m_json_str));
    ASSERT_EQ(nullptr, this->m_json_str.p_str);
    ASSERT_TRUE(this->m_mem_alloc_trace.is_empty());
}

TEST_F(TestHttpJson, test_create_status_json_str_malloc_failed_46_of_50) // NOLINT
{
    const mac_address_str_t      nrf52_mac_addr         = { .str_buf = "AA:CC:EE:00:11:22" };
    const uint32_t               uptime                 = 123;
    const bool                   is_wifi                = true;
    const uint32_t               network_disconnect_cnt = 3;
    const uint32_t               nonce                  = 1234567;
    const std::array<uint8_t, 1> data                   = { 0xAAU };
    adv_report_table_t           adv_table              = { .num_of_advs = 4,
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

    this->m_malloc_fail_on_cnt                  = 46;
    const http_json_statistics_info_t stat_info = {
        .nrf52_mac_addr         = nrf52_mac_addr,
        .esp_fw                 = { "1.9.0" },
        .nrf_fw                 = { "0.7.1" },
        .uptime                 = uptime,
        .nonce                  = nonce,
        .is_connected_to_wifi   = is_wifi,
        .network_disconnect_cnt = network_disconnect_cnt,
    };
    ASSERT_FALSE(http_json_create_status_str(&stat_info, &adv_table, &this->m_json_str));
    ASSERT_EQ(nullptr, this->m_json_str.p_str);
    ASSERT_TRUE(this->m_mem_alloc_trace.is_empty());
}

TEST_F(TestHttpJson, test_create_status_json_str_malloc_failed_47_of_50) // NOLINT
{
    const mac_address_str_t      nrf52_mac_addr         = { .str_buf = "AA:CC:EE:00:11:22" };
    const uint32_t               uptime                 = 123;
    const bool                   is_wifi                = true;
    const uint32_t               network_disconnect_cnt = 3;
    const uint32_t               nonce                  = 1234567;
    const std::array<uint8_t, 1> data                   = { 0xAAU };
    adv_report_table_t           adv_table              = { .num_of_advs = 4,
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

    this->m_malloc_fail_on_cnt                  = 47;
    const http_json_statistics_info_t stat_info = {
        .nrf52_mac_addr         = nrf52_mac_addr,
        .esp_fw                 = { "1.9.0" },
        .nrf_fw                 = { "0.7.1" },
        .uptime                 = uptime,
        .nonce                  = nonce,
        .is_connected_to_wifi   = is_wifi,
        .network_disconnect_cnt = network_disconnect_cnt,
    };
    ASSERT_FALSE(http_json_create_status_str(&stat_info, &adv_table, &this->m_json_str));
    ASSERT_EQ(nullptr, this->m_json_str.p_str);
    ASSERT_TRUE(this->m_mem_alloc_trace.is_empty());
}

TEST_F(TestHttpJson, test_create_status_json_str_malloc_failed_48_of_50) // NOLINT
{
    const mac_address_str_t      nrf52_mac_addr         = { .str_buf = "AA:CC:EE:00:11:22" };
    const uint32_t               uptime                 = 123;
    const bool                   is_wifi                = true;
    const uint32_t               network_disconnect_cnt = 3;
    const uint32_t               nonce                  = 1234567;
    const std::array<uint8_t, 1> data                   = { 0xAAU };
    adv_report_table_t           adv_table              = { .num_of_advs = 4,
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

    this->m_malloc_fail_on_cnt                  = 48;
    const http_json_statistics_info_t stat_info = {
        .nrf52_mac_addr         = nrf52_mac_addr,
        .esp_fw                 = { "1.9.0" },
        .nrf_fw                 = { "0.7.1" },
        .uptime                 = uptime,
        .nonce                  = nonce,
        .is_connected_to_wifi   = is_wifi,
        .network_disconnect_cnt = network_disconnect_cnt,
    };
    ASSERT_FALSE(http_json_create_status_str(&stat_info, &adv_table, &this->m_json_str));
    ASSERT_EQ(nullptr, this->m_json_str.p_str);
    ASSERT_TRUE(this->m_mem_alloc_trace.is_empty());
}

TEST_F(TestHttpJson, test_create_status_json_str_malloc_failed_49_of_50) // NOLINT
{
    const mac_address_str_t      nrf52_mac_addr         = { .str_buf = "AA:CC:EE:00:11:22" };
    const uint32_t               uptime                 = 123;
    const bool                   is_wifi                = true;
    const uint32_t               network_disconnect_cnt = 3;
    const uint32_t               nonce                  = 1234567;
    const std::array<uint8_t, 1> data                   = { 0xAAU };
    adv_report_table_t           adv_table              = { .num_of_advs = 4,
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

    this->m_malloc_fail_on_cnt                  = 49;
    const http_json_statistics_info_t stat_info = {
        .nrf52_mac_addr         = nrf52_mac_addr,
        .esp_fw                 = { "1.9.0" },
        .nrf_fw                 = { "0.7.1" },
        .uptime                 = uptime,
        .nonce                  = nonce,
        .is_connected_to_wifi   = is_wifi,
        .network_disconnect_cnt = network_disconnect_cnt,
    };
    ASSERT_FALSE(http_json_create_status_str(&stat_info, &adv_table, &this->m_json_str));
    ASSERT_EQ(nullptr, this->m_json_str.p_str);
    ASSERT_TRUE(this->m_mem_alloc_trace.is_empty());
}

TEST_F(TestHttpJson, test_create_status_json_str_malloc_failed_50_of_50) // NOLINT
{
    const mac_address_str_t      nrf52_mac_addr         = { .str_buf = "AA:CC:EE:00:11:22" };
    const uint32_t               uptime                 = 123;
    const bool                   is_wifi                = true;
    const uint32_t               network_disconnect_cnt = 3;
    const uint32_t               nonce                  = 1234567;
    const std::array<uint8_t, 1> data                   = { 0xAAU };
    adv_report_table_t           adv_table              = { .num_of_advs = 4,
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

    this->m_malloc_fail_on_cnt                  = 50;
    const http_json_statistics_info_t stat_info = {
        .nrf52_mac_addr         = nrf52_mac_addr,
        .esp_fw                 = { "1.9.0" },
        .nrf_fw                 = { "0.7.1" },
        .uptime                 = uptime,
        .nonce                  = nonce,
        .is_connected_to_wifi   = is_wifi,
        .network_disconnect_cnt = network_disconnect_cnt,
    };
    ASSERT_FALSE(http_json_create_status_str(&stat_info, &adv_table, &this->m_json_str));
    ASSERT_EQ(nullptr, this->m_json_str.p_str);
    ASSERT_TRUE(this->m_mem_alloc_trace.is_empty());
}
