/**
 * @file test_adv_table.cpp
 * @author TheSomeMan
 * @date 2021-01-20
 * @copyright Ruuvi Innovations Ltd, license BSD-3-Clause.
 */

#include "adv_table.h"
#include "gtest/gtest.h"
#include <string>
#include "os_mutex.h"

using namespace std;

/*** Google-test class implementation
 * *********************************************************************************/

class TestAdvTable : public ::testing::Test
{
private:
protected:
    void
    SetUp() override
    {
        adv_table_init();
    }

    void
    TearDown() override
    {
        adv_table_deinit();
    }

public:
    TestAdvTable();

    ~TestAdvTable() override;
};

TestAdvTable::TestAdvTable()
    : Test()
{
}

TestAdvTable::~TestAdvTable() = default;

extern "C" {

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

} // extern "C"

/*** Unit-Tests
 * *******************************************************************************************************/

TEST_F(TestAdvTable, test_1) // NOLINT
{
    const std::array<uint8_t, 2> data = { 0xAAU, 0xBBU };

    adv_report_t adv = {
        .tag_mac   = { 0x11U, 0x22U, 0x33U, 0x44U, 0x55U, 0x66U },
        .timestamp = 1611154440,
        .rssi      = 50,
        .data_len  = data.size(),
    };
    memcpy(adv.data_buf, data.data(), data.size());

    ASSERT_TRUE(adv_table_put(&adv));
    static adv_report_table_t reports = {};
    adv_table_read_and_clear(&reports);
    ASSERT_EQ(1, reports.num_of_advs);
    ASSERT_EQ(0, memcmp(adv.tag_mac.mac, reports.table[0].tag_mac.mac, sizeof(adv.tag_mac.mac)));
    ASSERT_EQ(adv.timestamp, reports.table[0].timestamp);
    ASSERT_EQ(adv.rssi, reports.table[0].rssi);
    std::array<uint8_t, data.size()> data_from_reports = {};
    std::copy(
        reports.table[0].data_buf,
        &reports.table[0].data_buf[data_from_reports.size()],
        std::begin(data_from_reports));
    ASSERT_EQ(data, data_from_reports);
}

TEST_F(TestAdvTable, test_1_read_clear_add) // NOLINT
{
    const std::array<uint8_t, 2> data = { 0xAAU, 0xBBU };

    adv_report_t adv = {
        .tag_mac   = { 0x11U, 0x22U, 0x33U, 0x44U, 0x55U, 0x66U },
        .timestamp = 1611154440,
        .rssi      = 50,
        .data_len  = data.size(),
    };
    memcpy(adv.data_buf, data.data(), data.size());

    ASSERT_TRUE(adv_table_put(&adv));
    static adv_report_table_t reports = { 0 };
    adv_table_read_and_clear(&reports);
    ASSERT_EQ(1, reports.num_of_advs);
    ASSERT_EQ(0, memcmp(adv.tag_mac.mac, reports.table[0].tag_mac.mac, sizeof(adv.tag_mac.mac)));
    ASSERT_EQ(adv.timestamp, reports.table[0].timestamp);
    ASSERT_EQ(adv.rssi, reports.table[0].rssi);
    {
        std::array<uint8_t, data.size()> data_from_reports = {};
        std::copy(
            reports.table[0].data_buf,
            &reports.table[0].data_buf[data_from_reports.size()],
            std::begin(data_from_reports));
        ASSERT_EQ(data, data_from_reports);
    }

    ASSERT_TRUE(adv_table_put(&adv));
    memset(&reports, 0, sizeof(reports));
    adv_table_read_and_clear(&reports);
    ASSERT_EQ(1, reports.num_of_advs);
    ASSERT_EQ(0, memcmp(adv.tag_mac.mac, reports.table[0].tag_mac.mac, sizeof(adv.tag_mac.mac)));
    ASSERT_EQ(adv.timestamp, reports.table[0].timestamp);
    ASSERT_EQ(adv.rssi, reports.table[0].rssi);
    {
        std::array<uint8_t, data.size()> data_from_reports = {};
        std::copy(
            reports.table[0].data_buf,
            &reports.table[0].data_buf[data_from_reports.size()],
            std::begin(data_from_reports));
        ASSERT_EQ(data, data_from_reports);
    }
}

TEST_F(TestAdvTable, test_2) // NOLINT
{
    const std::array<uint8_t, 2> data1 = { 0xAAU, 0xBBU };
    adv_report_t                 adv1  = {
        .tag_mac   = { 0x11U, 0x22U, 0x33U, 0x44U, 0x55U, 0x66U },
        .timestamp = 1611154440,
        .rssi      = 50,
        .data_len  = data1.size(),
    };
    memcpy(adv1.data_buf, data1.data(), data1.size());

    const std::array<uint8_t, 2> data2 = { 0xAAU, 0xBBU };
    adv_report_t                 adv2  = {
        .tag_mac   = { 0x12U, 0x23U, 0x34U, 0x45U, 0x56U, 0x67U },
        .timestamp = 1611154441,
        .rssi      = 51,
        .data_len  = data2.size(),
    };
    memcpy(adv2.data_buf, data2.data(), data2.size());

    ASSERT_TRUE(adv_table_put(&adv1));
    ASSERT_TRUE(adv_table_put(&adv2));

    static adv_report_table_t reports = { 0 };
    adv_table_read_and_clear(&reports);

    ASSERT_EQ(2, reports.num_of_advs);
    {
        const adv_report_t *p_adv_report = &reports.table[0];
        ASSERT_EQ(0, memcmp(adv1.tag_mac.mac, p_adv_report->tag_mac.mac, sizeof(adv1.tag_mac.mac)));
        ASSERT_EQ(adv1.timestamp, p_adv_report->timestamp);
        ASSERT_EQ(adv1.rssi, p_adv_report->rssi);
        {
            std::array<uint8_t, data1.size()> data_from_reports = {};
            std::copy(
                p_adv_report->data_buf,
                &p_adv_report->data_buf[data_from_reports.size()],
                std::begin(data_from_reports));
            ASSERT_EQ(data1, data_from_reports);
        }
    }
    {
        const adv_report_t *p_adv_report = &reports.table[1];
        ASSERT_EQ(0, memcmp(adv2.tag_mac.mac, p_adv_report->tag_mac.mac, sizeof(adv2.tag_mac.mac)));
        ASSERT_EQ(adv2.timestamp, p_adv_report->timestamp);
        ASSERT_EQ(adv2.rssi, p_adv_report->rssi);
        {
            std::array<uint8_t, data2.size()> data_from_reports = {};
            std::copy(
                p_adv_report->data_buf,
                &p_adv_report->data_buf[data_from_reports.size()],
                std::begin(data_from_reports));
            ASSERT_EQ(data2, data_from_reports);
        }
    }
}

TEST_F(TestAdvTable, test_2_with_the_same_hash) // NOLINT
{
    const std::array<uint8_t, 2> data1 = { 0xAAU, 0xBBU };
    adv_report_t                 adv1  = {
        .tag_mac   = { 0x11U, 0x22U, 0x33U, 0x44U, 0x55U, 0x66U },
        .timestamp = 1611154440,
        .rssi      = 50,
        .data_len  = data1.size(),
    };
    memcpy(adv1.data_buf, data1.data(), data1.size());

    const std::array<uint8_t, 2> data2 = { 0xAAU, 0xCCU };
    adv_report_t                 adv2  = {
        .tag_mac   = { 0x11U, 0x22U, 0x32U, 0x44U, 0x55, 0x67 },
        .timestamp = 1611154441,
        .rssi      = 51,
        .data_len  = data2.size(),
    };
    memcpy(adv2.data_buf, data2.data(), data2.size());

    ASSERT_EQ(adv_report_calc_hash(&adv1.tag_mac), adv_report_calc_hash(&adv2.tag_mac));

    ASSERT_TRUE(adv_table_put(&adv1));
    ASSERT_TRUE(adv_table_put(&adv2));

    static adv_report_table_t reports = { 0 };
    adv_table_read_and_clear(&reports);

    ASSERT_EQ(2, reports.num_of_advs);
    {
        const adv_report_t *p_adv_report = &reports.table[0];
        ASSERT_EQ(0, memcmp(adv1.tag_mac.mac, p_adv_report->tag_mac.mac, sizeof(adv1.tag_mac.mac)));
        ASSERT_EQ(adv1.timestamp, p_adv_report->timestamp);
        ASSERT_EQ(adv1.rssi, p_adv_report->rssi);
        {
            std::array<uint8_t, data1.size()> data_from_reports = {};
            std::copy(
                p_adv_report->data_buf,
                &p_adv_report->data_buf[data_from_reports.size()],
                std::begin(data_from_reports));
            ASSERT_EQ(data1, data_from_reports);
        }
    }
    {
        const adv_report_t *p_adv_report = &reports.table[1];
        ASSERT_EQ(0, memcmp(adv2.tag_mac.mac, p_adv_report->tag_mac.mac, sizeof(adv2.tag_mac.mac)));
        ASSERT_EQ(adv2.timestamp, p_adv_report->timestamp);
        ASSERT_EQ(adv2.rssi, p_adv_report->rssi);
        {
            std::array<uint8_t, data2.size()> data_from_reports = {};
            std::copy(
                p_adv_report->data_buf,
                &p_adv_report->data_buf[data_from_reports.size()],
                std::begin(data_from_reports));
            ASSERT_EQ(data2, data_from_reports);
        }
    }
}

TEST_F(TestAdvTable, test_2_with_the_same_hash_with_clear) // NOLINT
{
    const std::array<uint8_t, 2> data1 = { 0xAAU, 0xBBU };
    adv_report_t                 adv1  = {
        .tag_mac   = { 0x11U, 0x22U, 0x33U, 0x44U, 0x55U, 0x66U },
        .timestamp = 1611154440,
        .rssi      = 50,
        .data_len  = data1.size(),
    };
    memcpy(adv1.data_buf, data1.data(), data1.size());

    ASSERT_TRUE(adv_table_put(&adv1));

    static adv_report_table_t reports = { 0 };
    adv_table_read_and_clear(&reports);

    ASSERT_EQ(1, reports.num_of_advs);
    {
        const adv_report_t *p_adv_report = &reports.table[0];
        ASSERT_EQ(0, memcmp(adv1.tag_mac.mac, p_adv_report->tag_mac.mac, sizeof(adv1.tag_mac.mac)));
        ASSERT_EQ(adv1.timestamp, p_adv_report->timestamp);
        ASSERT_EQ(adv1.rssi, p_adv_report->rssi);
        {
            std::array<uint8_t, data1.size()> data_from_reports = {};
            std::copy(
                p_adv_report->data_buf,
                &p_adv_report->data_buf[data_from_reports.size()],
                std::begin(data_from_reports));
            ASSERT_EQ(data1, data_from_reports);
        }
    }

    const std::array<uint8_t, 2> data2 = { 0xAAU, 0xCCU };
    adv_report_t                 adv2  = {
        .tag_mac   = { 0x11U, 0x22U, 0x32U, 0x44U, 0x55, 0x67 },
        .timestamp = 1611154441,
        .rssi      = 51,
        .data_len  = data2.size(),
    };
    memcpy(adv2.data_buf, data2.data(), data2.size());

    ASSERT_EQ(adv_report_calc_hash(&adv1.tag_mac), adv_report_calc_hash(&adv2.tag_mac));

    ASSERT_TRUE(adv_table_put(&adv2));

    memset(&reports, 0, sizeof(reports));
    adv_table_read_and_clear(&reports);

    ASSERT_EQ(1, reports.num_of_advs);
    {
        const adv_report_t *p_adv_report = &reports.table[1];
        ASSERT_EQ(0, memcmp(adv2.tag_mac.mac, p_adv_report->tag_mac.mac, sizeof(adv2.tag_mac.mac)));
        ASSERT_EQ(adv2.timestamp, p_adv_report->timestamp);
        ASSERT_EQ(adv2.rssi, p_adv_report->rssi);
        {
            std::array<uint8_t, data2.size()> data_from_reports = {};
            std::copy(
                p_adv_report->data_buf,
                &p_adv_report->data_buf[data_from_reports.size()],
                std::begin(data_from_reports));
            ASSERT_EQ(data2, data_from_reports);
        }
    }
}

TEST_F(TestAdvTable, test_3_overwrite_1) // NOLINT
{
    const std::array<uint8_t, 2> data1 = { 0xAAU, 0xBBU };
    adv_report_t                 adv1  = {
        .tag_mac   = { 0x11U, 0x22U, 0x33U, 0x44U, 0x55U, 0x66U },
        .timestamp = 1611154440,
        .rssi      = 50,
        .data_len  = data1.size(),
    };
    memcpy(adv1.data_buf, data1.data(), data1.size());

    const std::array<uint8_t, 2> data2 = { 0xAAU, 0xCCU };
    adv_report_t                 adv2  = {
        .tag_mac   = { 0x12U, 0x23U, 0x34U, 0x45U, 0x56U, 0x67U },
        .timestamp = 1611154441,
        .rssi      = 51,
        .data_len  = data2.size(),
    };
    memcpy(adv2.data_buf, data2.data(), data2.size());

    const std::array<uint8_t, 2> data3 = { 0xAAU, 0xDDU };
    adv_report_t                 adv3  = {
        .tag_mac   = { 0x11U, 0x22U, 0x33U, 0x44U, 0x55U, 0x66U },
        .timestamp = 1611154443,
        .rssi      = 53,
        .data_len  = data3.size(),
    };
    memcpy(adv3.data_buf, data3.data(), data3.size());

    ASSERT_TRUE(adv_table_put(&adv1));
    ASSERT_TRUE(adv_table_put(&adv2));
    ASSERT_TRUE(adv_table_put(&adv3));

    static adv_report_table_t reports = { 0 };
    adv_table_read_and_clear(&reports);

    ASSERT_EQ(2, reports.num_of_advs);
    {
        const adv_report_t *p_adv_report = &reports.table[0];
        ASSERT_EQ(0, memcmp(adv3.tag_mac.mac, p_adv_report->tag_mac.mac, sizeof(adv1.tag_mac.mac)));
        ASSERT_EQ(adv3.timestamp, p_adv_report->timestamp);
        ASSERT_EQ(adv3.rssi, p_adv_report->rssi);
        {
            std::array<uint8_t, data3.size()> data_from_reports = {};
            std::copy(
                p_adv_report->data_buf,
                &p_adv_report->data_buf[data_from_reports.size()],
                std::begin(data_from_reports));
            ASSERT_EQ(data3, data_from_reports);
        }
    }
    {
        const adv_report_t *p_adv_report = &reports.table[1];
        ASSERT_EQ(0, memcmp(adv2.tag_mac.mac, p_adv_report->tag_mac.mac, sizeof(adv2.tag_mac.mac)));
        ASSERT_EQ(adv2.timestamp, p_adv_report->timestamp);
        ASSERT_EQ(adv2.rssi, p_adv_report->rssi);
        {
            std::array<uint8_t, data2.size()> data_from_reports = {};
            std::copy(
                p_adv_report->data_buf,
                &p_adv_report->data_buf[data_from_reports.size()],
                std::begin(data_from_reports));
            ASSERT_EQ(data2, data_from_reports);
        }
    }
}

TEST_F(TestAdvTable, test_3_overwrite_2) // NOLINT
{
    const std::array<uint8_t, 2> data1 = { 0xAAU, 0xBBU };
    adv_report_t                 adv1  = {
        .tag_mac   = { 0x11U, 0x22U, 0x33U, 0x44U, 0x55U, 0x66U },
        .timestamp = 1611154440,
        .rssi      = 50,
        .data_len  = data1.size(),
    };
    memcpy(adv1.data_buf, data1.data(), data1.size());

    const std::array<uint8_t, 2> data2 = { 0xAAU, 0xCCU };
    adv_report_t                 adv2  = {
        .tag_mac   = { 0x12U, 0x23U, 0x34U, 0x45U, 0x56U, 0x67U },
        .timestamp = 1611154441,
        .rssi      = 51,
        .data_len  = data2.size(),
    };
    memcpy(adv2.data_buf, data2.data(), data2.size());

    const std::array<uint8_t, 2> data3 = { 0xAAU, 0xBBU };
    adv_report_t                 adv3  = {
        .tag_mac   = { 0x12U, 0x23U, 0x34U, 0x45U, 0x56U, 0x67U },
        .timestamp = 1611154443,
        .rssi      = 53,
        .data_len  = data3.size(),
    };
    memcpy(adv3.data_buf, data3.data(), data3.size());

    ASSERT_TRUE(adv_table_put(&adv1));
    ASSERT_TRUE(adv_table_put(&adv2));
    ASSERT_TRUE(adv_table_put(&adv3));

    static adv_report_table_t reports = { 0 };
    adv_table_read_and_clear(&reports);

    ASSERT_EQ(2, reports.num_of_advs);
    {
        const adv_report_t *p_adv_report = &reports.table[0];
        ASSERT_EQ(0, memcmp(adv1.tag_mac.mac, p_adv_report->tag_mac.mac, sizeof(adv1.tag_mac.mac)));
        ASSERT_EQ(adv1.timestamp, p_adv_report->timestamp);
        ASSERT_EQ(adv1.rssi, p_adv_report->rssi);
        {
            std::array<uint8_t, data1.size()> data_from_reports = {};
            std::copy(
                p_adv_report->data_buf,
                &p_adv_report->data_buf[data_from_reports.size()],
                std::begin(data_from_reports));
            ASSERT_EQ(data1, data_from_reports);
        }
    }
    {
        const adv_report_t *p_adv_report = &reports.table[1];
        ASSERT_EQ(0, memcmp(adv3.tag_mac.mac, p_adv_report->tag_mac.mac, sizeof(adv2.tag_mac.mac)));
        ASSERT_EQ(adv3.timestamp, p_adv_report->timestamp);
        ASSERT_EQ(adv3.rssi, p_adv_report->rssi);
        {
            std::array<uint8_t, data3.size()> data_from_reports = {};
            std::copy(
                p_adv_report->data_buf,
                &p_adv_report->data_buf[data_from_reports.size()],
                std::begin(data_from_reports));
            ASSERT_EQ(data3, data_from_reports);
        }
    }
}

TEST_F(TestAdvTable, test_3_with_the_same_hash_overwrite_1) // NOLINT
{
    const std::array<uint8_t, 2> data1 = { 0xAAU, 0xBBU };
    adv_report_t                 adv1  = {
        .tag_mac   = { 0x11U, 0x22U, 0x33U, 0x44U, 0x55U, 0x66U },
        .timestamp = 1611154440,
        .rssi      = 50,
        .data_len  = data1.size(),
    };
    memcpy(adv1.data_buf, data1.data(), data1.size());

    const std::array<uint8_t, 2> data2 = { 0xAAU, 0xCCU };
    adv_report_t                 adv2  = {
        .tag_mac   = { 0x11U, 0x22U, 0x32U, 0x44U, 0x55, 0x67 },
        .timestamp = 1611154441,
        .rssi      = 51,
        .data_len  = data2.size(),
    };
    memcpy(adv2.data_buf, data2.data(), data2.size());

    ASSERT_EQ(adv_report_calc_hash(&adv1.tag_mac), adv_report_calc_hash(&adv2.tag_mac));
    const std::array<uint8_t, 2> data3 = { 0xAAU, 0xDDU };
    adv_report_t                 adv3  = {
        .tag_mac   = { 0x11U, 0x22U, 0x33U, 0x44U, 0x55U, 0x66U },
        .timestamp = 1611154441,
        .rssi      = 51,
        .data_len  = data3.size(),
    };
    memcpy(adv3.data_buf, data3.data(), data3.size());

    ASSERT_TRUE(adv_table_put(&adv1));
    ASSERT_TRUE(adv_table_put(&adv2));
    ASSERT_TRUE(adv_table_put(&adv3));

    static adv_report_table_t reports = { 0 };
    adv_table_read_and_clear(&reports);

    ASSERT_EQ(2, reports.num_of_advs);
    {
        const adv_report_t *p_adv_report = &reports.table[0];
        ASSERT_EQ(0, memcmp(adv3.tag_mac.mac, p_adv_report->tag_mac.mac, sizeof(adv1.tag_mac.mac)));
        ASSERT_EQ(adv3.timestamp, p_adv_report->timestamp);
        ASSERT_EQ(adv3.rssi, p_adv_report->rssi);
        {
            std::array<uint8_t, data3.size()> data_from_reports = {};
            std::copy(
                p_adv_report->data_buf,
                &p_adv_report->data_buf[data_from_reports.size()],
                std::begin(data_from_reports));
            ASSERT_EQ(data3, data_from_reports);
        }
    }
    {
        const adv_report_t *p_adv_report = &reports.table[1];
        ASSERT_EQ(0, memcmp(adv2.tag_mac.mac, p_adv_report->tag_mac.mac, sizeof(adv2.tag_mac.mac)));
        ASSERT_EQ(adv2.timestamp, p_adv_report->timestamp);
        ASSERT_EQ(adv2.rssi, p_adv_report->rssi);
        {
            std::array<uint8_t, data2.size()> data_from_reports = {};
            std::copy(
                p_adv_report->data_buf,
                &p_adv_report->data_buf[data_from_reports.size()],
                std::begin(data_from_reports));
            ASSERT_EQ(data2, data_from_reports);
        }
    }
}

TEST_F(TestAdvTable, test_3_with_the_same_hash_overwrite_2) // NOLINT
{
    const std::array<uint8_t, 2> data1 = { 0xAAU, 0xBBU };
    adv_report_t                 adv1  = {
        .tag_mac   = { 0x11U, 0x22U, 0x33U, 0x44U, 0x55U, 0x66U },
        .timestamp = 1611154440,
        .rssi      = 50,
        .data_len  = data1.size(),
    };
    memcpy(adv1.data_buf, data1.data(), data1.size());

    const std::array<uint8_t, 2> data2 = { 0xAAU, 0xCCU };
    adv_report_t                 adv2  = {
        .tag_mac   = { 0x11U, 0x22U, 0x32U, 0x44U, 0x55, 0x67 },
        .timestamp = 1611154441,
        .rssi      = 51,
        .data_len  = data2.size(),
    };
    memcpy(adv2.data_buf, data2.data(), data2.size());

    ASSERT_EQ(adv_report_calc_hash(&adv1.tag_mac), adv_report_calc_hash(&adv2.tag_mac));
    const std::array<uint8_t, 2> data3 = { 0xAAU, 0xDDU };
    adv_report_t                 adv3  = {
        .tag_mac   = { 0x11U, 0x22U, 0x32U, 0x44U, 0x55, 0x67 },
        .timestamp = 1611154443,
        .rssi      = 53,
        .data_len  = data3.size(),
    };
    memcpy(adv3.data_buf, data3.data(), data3.size());

    ASSERT_TRUE(adv_table_put(&adv1));
    ASSERT_TRUE(adv_table_put(&adv2));
    ASSERT_TRUE(adv_table_put(&adv3));

    static adv_report_table_t reports = { 0 };
    adv_table_read_and_clear(&reports);

    ASSERT_EQ(2, reports.num_of_advs);
    {
        const adv_report_t *p_adv_report = &reports.table[0];
        ASSERT_EQ(0, memcmp(adv1.tag_mac.mac, p_adv_report->tag_mac.mac, sizeof(adv1.tag_mac.mac)));
        ASSERT_EQ(adv1.timestamp, p_adv_report->timestamp);
        ASSERT_EQ(adv1.rssi, p_adv_report->rssi);
        {
            std::array<uint8_t, data1.size()> data_from_reports = {};
            std::copy(
                p_adv_report->data_buf,
                &p_adv_report->data_buf[data_from_reports.size()],
                std::begin(data_from_reports));
            ASSERT_EQ(data1, data_from_reports);
        }
    }
    {
        const adv_report_t *p_adv_report = &reports.table[1];
        ASSERT_EQ(0, memcmp(adv3.tag_mac.mac, p_adv_report->tag_mac.mac, sizeof(adv2.tag_mac.mac)));
        ASSERT_EQ(adv3.timestamp, p_adv_report->timestamp);
        ASSERT_EQ(adv3.rssi, p_adv_report->rssi);
        {
            std::array<uint8_t, data3.size()> data_from_reports = {};
            std::copy(
                p_adv_report->data_buf,
                &p_adv_report->data_buf[data_from_reports.size()],
                std::begin(data_from_reports));
            ASSERT_EQ(data3, data_from_reports);
        }
    }
}
