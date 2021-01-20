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
os_mutex_lock(os_mutex_t const h_mutex)
{
}

void
os_mutex_unlock(os_mutex_t const h_mutex)
{
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
