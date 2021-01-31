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
    const adv_report_t adv = {
        .tag_mac   = { 0x11U, 0x22U, 0x33U, 0x44U, 0x55U, 0x66U },
        .timestamp = 1611154440,
        .rssi      = 50,
        .data      = { 'a', 'a', 'b', 'b', '\0' },
    };
    ASSERT_TRUE(adv_table_put(&adv));
    static adv_report_table_t reports = { 0 };
    adv_table_read_and_clear(&reports);
    ASSERT_EQ(1, reports.num_of_advs);
    ASSERT_EQ(0, memcmp(adv.tag_mac.mac, reports.table[0].tag_mac.mac, sizeof(adv.tag_mac.mac)));
    ASSERT_EQ(adv.timestamp, reports.table[0].timestamp);
    ASSERT_EQ(adv.rssi, reports.table[0].rssi);
    ASSERT_EQ(string("aabb"), string(reports.table[0].data));
}

TEST_F(TestAdvTable, test_1_read_clear_add) // NOLINT
{
    const adv_report_t adv = {
        .tag_mac   = { 0x11U, 0x22U, 0x33U, 0x44U, 0x55U, 0x66U },
        .timestamp = 1611154440,
        .rssi      = 50,
        .data      = { 'a', 'a', 'b', 'b', '\0' },
    };
    ASSERT_TRUE(adv_table_put(&adv));
    static adv_report_table_t reports = { 0 };
    adv_table_read_and_clear(&reports);
    ASSERT_EQ(1, reports.num_of_advs);
    ASSERT_EQ(0, memcmp(adv.tag_mac.mac, reports.table[0].tag_mac.mac, sizeof(adv.tag_mac.mac)));
    ASSERT_EQ(adv.timestamp, reports.table[0].timestamp);
    ASSERT_EQ(adv.rssi, reports.table[0].rssi);
    ASSERT_EQ(string("aabb"), string(reports.table[0].data));

    ASSERT_TRUE(adv_table_put(&adv));
    memset(&reports, 0, sizeof(reports));
    adv_table_read_and_clear(&reports);
    ASSERT_EQ(1, reports.num_of_advs);
    ASSERT_EQ(0, memcmp(adv.tag_mac.mac, reports.table[0].tag_mac.mac, sizeof(adv.tag_mac.mac)));
    ASSERT_EQ(adv.timestamp, reports.table[0].timestamp);
    ASSERT_EQ(adv.rssi, reports.table[0].rssi);
    ASSERT_EQ(string("aabb"), string(reports.table[0].data));
}

TEST_F(TestAdvTable, test_2) // NOLINT
{
    const adv_report_t adv1 = {
        .tag_mac   = { 0x11U, 0x22U, 0x33U, 0x44U, 0x55U, 0x66U },
        .timestamp = 1611154440,
        .rssi      = 50,
        .data      = { 'a', 'a', 'b', 'b', '\0' },
    };
    const adv_report_t adv2 = {
        .tag_mac   = { 0x12U, 0x23U, 0x34U, 0x45U, 0x56U, 0x67U },
        .timestamp = 1611154441,
        .rssi      = 51,
        .data      = { 'a', 'a', 'c', 'c', '\0' },
    };
    ASSERT_TRUE(adv_table_put(&adv1));
    ASSERT_TRUE(adv_table_put(&adv2));

    static adv_report_table_t reports = { 0 };
    adv_table_read_and_clear(&reports);

    ASSERT_EQ(2, reports.num_of_advs);
    {
        ASSERT_EQ(0, memcmp(adv1.tag_mac.mac, reports.table[0].tag_mac.mac, sizeof(adv1.tag_mac.mac)));
        ASSERT_EQ(adv1.timestamp, reports.table[0].timestamp);
        ASSERT_EQ(adv1.rssi, reports.table[0].rssi);
        ASSERT_EQ(string("aabb"), string(reports.table[0].data));
    }
    {
        ASSERT_EQ(0, memcmp(adv2.tag_mac.mac, reports.table[1].tag_mac.mac, sizeof(adv2.tag_mac.mac)));
        ASSERT_EQ(adv2.timestamp, reports.table[1].timestamp);
        ASSERT_EQ(adv2.rssi, reports.table[1].rssi);
        ASSERT_EQ(string("aacc"), string(reports.table[1].data));
    }
}

TEST_F(TestAdvTable, test_2_with_the_same_hash) // NOLINT
{
    const adv_report_t adv1 = {
        .tag_mac   = { 0x11U, 0x22U, 0x33U, 0x44U, 0x55U, 0x66U },
        .timestamp = 1611154440,
        .rssi      = 50,
        .data      = { 'a', 'a', 'b', 'b', '\0' },
    };
    adv_report_t adv2 = {
        .tag_mac   = { 0x11U, 0x22U, 0x32U, 0x44U, 0x55, 0x67 },
        .timestamp = 1611154441,
        .rssi      = 51,
        .data      = { 'a', 'a', 'c', 'c', '\0' },
    };
    ASSERT_EQ(adv_report_calc_hash(&adv1.tag_mac), adv_report_calc_hash(&adv2.tag_mac));

    ASSERT_TRUE(adv_table_put(&adv1));
    ASSERT_TRUE(adv_table_put(&adv2));

    static adv_report_table_t reports = { 0 };
    adv_table_read_and_clear(&reports);

    ASSERT_EQ(2, reports.num_of_advs);
    {
        ASSERT_EQ(0, memcmp(adv1.tag_mac.mac, reports.table[0].tag_mac.mac, sizeof(adv1.tag_mac.mac)));
        ASSERT_EQ(adv1.timestamp, reports.table[0].timestamp);
        ASSERT_EQ(adv1.rssi, reports.table[0].rssi);
        ASSERT_EQ(string("aabb"), string(reports.table[0].data));
    }
    {
        ASSERT_EQ(0, memcmp(adv2.tag_mac.mac, reports.table[1].tag_mac.mac, sizeof(adv2.tag_mac.mac)));
        ASSERT_EQ(adv2.timestamp, reports.table[1].timestamp);
        ASSERT_EQ(adv2.rssi, reports.table[1].rssi);
        ASSERT_EQ(string("aacc"), string(reports.table[1].data));
    }
}

TEST_F(TestAdvTable, test_2_with_the_same_hash_with_clear) // NOLINT
{
    const adv_report_t adv1 = {
        .tag_mac   = { 0x11U, 0x22U, 0x33U, 0x44U, 0x55U, 0x66U },
        .timestamp = 1611154440,
        .rssi      = 50,
        .data      = { 'a', 'a', 'b', 'b', '\0' },
    };
    ASSERT_TRUE(adv_table_put(&adv1));

    static adv_report_table_t reports = { 0 };
    adv_table_read_and_clear(&reports);

    ASSERT_EQ(1, reports.num_of_advs);
    {
        ASSERT_EQ(0, memcmp(adv1.tag_mac.mac, reports.table[0].tag_mac.mac, sizeof(adv1.tag_mac.mac)));
        ASSERT_EQ(adv1.timestamp, reports.table[0].timestamp);
        ASSERT_EQ(adv1.rssi, reports.table[0].rssi);
        ASSERT_EQ(string("aabb"), string(reports.table[0].data));
    }

    adv_report_t adv2 = {
        .tag_mac   = { 0x11U, 0x22U, 0x32U, 0x44U, 0x55, 0x67 },
        .timestamp = 1611154441,
        .rssi      = 51,
        .data      = { 'a', 'a', 'c', 'c', '\0' },
    };
    ASSERT_EQ(adv_report_calc_hash(&adv1.tag_mac), adv_report_calc_hash(&adv2.tag_mac));

    ASSERT_TRUE(adv_table_put(&adv2));

    memset(&reports, 0, sizeof(reports));
    adv_table_read_and_clear(&reports);

    ASSERT_EQ(1, reports.num_of_advs);
    {
        ASSERT_EQ(0, memcmp(adv2.tag_mac.mac, reports.table[1].tag_mac.mac, sizeof(adv2.tag_mac.mac)));
        ASSERT_EQ(adv2.timestamp, reports.table[1].timestamp);
        ASSERT_EQ(adv2.rssi, reports.table[1].rssi);
        ASSERT_EQ(string("aacc"), string(reports.table[1].data));
    }
}

TEST_F(TestAdvTable, test_3_overwrite_1) // NOLINT
{
    const adv_report_t adv1 = {
        .tag_mac   = { 0x11U, 0x22U, 0x33U, 0x44U, 0x55U, 0x66U },
        .timestamp = 1611154440,
        .rssi      = 50,
        .data      = { 'a', 'a', 'b', 'b', '\0' },
    };
    const adv_report_t adv2 = {
        .tag_mac   = { 0x12U, 0x23U, 0x34U, 0x45U, 0x56U, 0x67U },
        .timestamp = 1611154441,
        .rssi      = 51,
        .data      = { 'a', 'a', 'c', 'c', '\0' },
    };
    const adv_report_t adv3 = {
        .tag_mac   = { 0x11U, 0x22U, 0x33U, 0x44U, 0x55U, 0x66U },
        .timestamp = 1611154443,
        .rssi      = 53,
        .data      = { 'a', 'a', 'z', 'z', '\0' },
    };
    ASSERT_TRUE(adv_table_put(&adv1));
    ASSERT_TRUE(adv_table_put(&adv2));
    ASSERT_TRUE(adv_table_put(&adv3));

    static adv_report_table_t reports = { 0 };
    adv_table_read_and_clear(&reports);

    ASSERT_EQ(2, reports.num_of_advs);
    {
        ASSERT_EQ(0, memcmp(adv3.tag_mac.mac, reports.table[0].tag_mac.mac, sizeof(adv1.tag_mac.mac)));
        ASSERT_EQ(adv3.timestamp, reports.table[0].timestamp);
        ASSERT_EQ(adv3.rssi, reports.table[0].rssi);
        ASSERT_EQ(string("aazz"), string(reports.table[0].data));
    }
    {
        ASSERT_EQ(0, memcmp(adv2.tag_mac.mac, reports.table[1].tag_mac.mac, sizeof(adv2.tag_mac.mac)));
        ASSERT_EQ(adv2.timestamp, reports.table[1].timestamp);
        ASSERT_EQ(adv2.rssi, reports.table[1].rssi);
        ASSERT_EQ(string("aacc"), string(reports.table[1].data));
    }
}

TEST_F(TestAdvTable, test_3_overwrite_2) // NOLINT
{
    const adv_report_t adv1 = {
        .tag_mac   = { 0x11U, 0x22U, 0x33U, 0x44U, 0x55U, 0x66U },
        .timestamp = 1611154440,
        .rssi      = 50,
        .data      = { 'a', 'a', 'b', 'b', '\0' },
    };
    const adv_report_t adv2 = {
        .tag_mac   = { 0x12U, 0x23U, 0x34U, 0x45U, 0x56U, 0x67U },
        .timestamp = 1611154441,
        .rssi      = 51,
        .data      = { 'a', 'a', 'c', 'c', '\0' },
    };
    const adv_report_t adv3 = {
        .tag_mac   = { 0x12U, 0x23U, 0x34U, 0x45U, 0x56U, 0x67U },
        .timestamp = 1611154443,
        .rssi      = 53,
        .data      = { 'a', 'a', 'z', 'z', '\0' },
    };
    ASSERT_TRUE(adv_table_put(&adv1));
    ASSERT_TRUE(adv_table_put(&adv2));
    ASSERT_TRUE(adv_table_put(&adv3));

    static adv_report_table_t reports = { 0 };
    adv_table_read_and_clear(&reports);

    ASSERT_EQ(2, reports.num_of_advs);
    {
        ASSERT_EQ(0, memcmp(adv1.tag_mac.mac, reports.table[0].tag_mac.mac, sizeof(adv1.tag_mac.mac)));
        ASSERT_EQ(adv1.timestamp, reports.table[0].timestamp);
        ASSERT_EQ(adv1.rssi, reports.table[0].rssi);
        ASSERT_EQ(string("aabb"), string(reports.table[0].data));
    }
    {
        ASSERT_EQ(0, memcmp(adv3.tag_mac.mac, reports.table[1].tag_mac.mac, sizeof(adv2.tag_mac.mac)));
        ASSERT_EQ(adv3.timestamp, reports.table[1].timestamp);
        ASSERT_EQ(adv3.rssi, reports.table[1].rssi);
        ASSERT_EQ(string("aazz"), string(reports.table[1].data));
    }
}

TEST_F(TestAdvTable, test_3_with_the_same_hash_overwrite_1) // NOLINT
{
    const adv_report_t adv1 = {
        .tag_mac   = { 0x11U, 0x22U, 0x33U, 0x44U, 0x55U, 0x66U },
        .timestamp = 1611154440,
        .rssi      = 50,
        .data      = { 'a', 'a', 'b', 'b', '\0' },
    };
    adv_report_t adv2 = {
        .tag_mac   = { 0x11U, 0x22U, 0x32U, 0x44U, 0x55, 0x67 },
        .timestamp = 1611154441,
        .rssi      = 51,
        .data      = { 'a', 'a', 'c', 'c', '\0' },
    };
    ASSERT_EQ(adv_report_calc_hash(&adv1.tag_mac), adv_report_calc_hash(&adv2.tag_mac));
    const adv_report_t adv3 = {
        .tag_mac   = { 0x11U, 0x22U, 0x33U, 0x44U, 0x55U, 0x66U },
        .timestamp = 1611154441,
        .rssi      = 51,
        .data      = { 'a', 'a', 'z', 'z', '\0' },
    };

    ASSERT_TRUE(adv_table_put(&adv1));
    ASSERT_TRUE(adv_table_put(&adv2));
    ASSERT_TRUE(adv_table_put(&adv3));

    static adv_report_table_t reports = { 0 };
    adv_table_read_and_clear(&reports);

    ASSERT_EQ(2, reports.num_of_advs);
    {
        ASSERT_EQ(0, memcmp(adv3.tag_mac.mac, reports.table[0].tag_mac.mac, sizeof(adv1.tag_mac.mac)));
        ASSERT_EQ(adv3.timestamp, reports.table[0].timestamp);
        ASSERT_EQ(adv3.rssi, reports.table[0].rssi);
        ASSERT_EQ(string("aazz"), string(reports.table[0].data));
    }
    {
        ASSERT_EQ(0, memcmp(adv2.tag_mac.mac, reports.table[1].tag_mac.mac, sizeof(adv2.tag_mac.mac)));
        ASSERT_EQ(adv2.timestamp, reports.table[1].timestamp);
        ASSERT_EQ(adv2.rssi, reports.table[1].rssi);
        ASSERT_EQ(string("aacc"), string(reports.table[1].data));
    }
}

TEST_F(TestAdvTable, test_3_with_the_same_hash_overwrite_2) // NOLINT
{
    const adv_report_t adv1 = {
        .tag_mac   = { 0x11U, 0x22U, 0x33U, 0x44U, 0x55U, 0x66U },
        .timestamp = 1611154440,
        .rssi      = 50,
        .data      = { 'a', 'a', 'b', 'b', '\0' },
    };
    adv_report_t adv2 = {
        .tag_mac   = { 0x11U, 0x22U, 0x32U, 0x44U, 0x55, 0x67 },
        .timestamp = 1611154441,
        .rssi      = 51,
        .data      = { 'a', 'a', 'c', 'c', '\0' },
    };
    ASSERT_EQ(adv_report_calc_hash(&adv1.tag_mac), adv_report_calc_hash(&adv2.tag_mac));
    adv_report_t adv3 = {
        .tag_mac   = { 0x11U, 0x22U, 0x32U, 0x44U, 0x55, 0x67 },
        .timestamp = 1611154443,
        .rssi      = 53,
        .data      = { 'a', 'a', 'z', 'z', '\0' },
    };

    ASSERT_TRUE(adv_table_put(&adv1));
    ASSERT_TRUE(adv_table_put(&adv2));
    ASSERT_TRUE(adv_table_put(&adv3));

    static adv_report_table_t reports = { 0 };
    adv_table_read_and_clear(&reports);

    ASSERT_EQ(2, reports.num_of_advs);
    {
        ASSERT_EQ(0, memcmp(adv1.tag_mac.mac, reports.table[0].tag_mac.mac, sizeof(adv1.tag_mac.mac)));
        ASSERT_EQ(adv1.timestamp, reports.table[0].timestamp);
        ASSERT_EQ(adv1.rssi, reports.table[0].rssi);
        ASSERT_EQ(string("aabb"), string(reports.table[0].data));
    }
    {
        ASSERT_EQ(0, memcmp(adv3.tag_mac.mac, reports.table[1].tag_mac.mac, sizeof(adv2.tag_mac.mac)));
        ASSERT_EQ(adv3.timestamp, reports.table[1].timestamp);
        ASSERT_EQ(adv3.rssi, reports.table[1].rssi);
        ASSERT_EQ(string("aazz"), string(reports.table[1].data));
    }
}
