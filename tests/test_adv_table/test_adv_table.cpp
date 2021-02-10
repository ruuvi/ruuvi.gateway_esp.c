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

#define DECL_ADV_REPORT(adv_var_name_, mac_addr_, timestamp_, rssi_, data_var_name_, ...) \
    const uint8_t tmp_##data_var_name_[] = { __VA_ARGS__ }; \
    const std::array<uint8_t, sizeof(tmp_##data_var_name_) / sizeof(tmp_##data_var_name_[0])> data_var_name_ = { \
        __VA_ARGS__ \
    }; \
    const adv_report_t adv_var_name_ = { \
        .timestamp = timestamp_, \
        .tag_mac   = { ((mac_addr_) >> 5 * 8) & 0xFFU, \
                     ((mac_addr_) >> 4 * 8) & 0xFFU, \
                     ((mac_addr_) >> 3 * 8) & 0xFFU, \
                     ((mac_addr_) >> 2 * 8) & 0xFFU, \
                     ((mac_addr_) >> 1 * 8) & 0xFFU, \
                     ((mac_addr_) >> 0 * 8) & 0xFFU }, \
        .rssi      = rssi_, \
        .data_len  = sizeof(tmp_##data_var_name_) / sizeof(tmp_##data_var_name_[0]), \
        .data_buf  = { __VA_ARGS__ }, \
    }

#define CHECK_ADV_REPORT(exp_adv_, exp_data_, p_adv_) \
    do \
    { \
        ASSERT_EQ(0, memcmp((exp_adv_).tag_mac.mac, (p_adv_)->tag_mac.mac, sizeof((exp_adv_).tag_mac.mac))); \
        ASSERT_EQ((exp_adv_).timestamp, (p_adv_)->timestamp); \
        ASSERT_EQ((exp_adv_).rssi, (p_adv_)->rssi); \
        ASSERT_EQ((exp_data_).size(), (p_adv_)->data_len); \
\
        std::array<uint8_t, (exp_data_).size()> data_from_reports = {}; \
        std::copy((p_adv_)->data_buf, &(p_adv_)->data_buf[data_from_reports.size()], std::begin(data_from_reports)); \
        ASSERT_EQ(exp_data_, data_from_reports); \
    } while (0)

/*** Unit-Tests
 * *******************************************************************************************************/

TEST_F(TestAdvTable, test_1) // NOLINT
{
    DECL_ADV_REPORT(adv, 0x112233445566LLU, 1611154440, 50, data, 0xAAU, 0xBBU);

    ASSERT_TRUE(adv_table_put(&adv));
    {
        adv_report_table_t reports = {};
        adv_table_read_and_clear(&reports);
        ASSERT_EQ(1, reports.num_of_advs);
        CHECK_ADV_REPORT(adv, data, &reports.table[0]);
    }
}

TEST_F(TestAdvTable, test_1_read_clear_add) // NOLINT
{
    DECL_ADV_REPORT(adv, 0x112233445566LLU, 1611154440, 50, data, 0xAAU, 0xBBU);

    ASSERT_TRUE(adv_table_put(&adv));
    {
        adv_report_table_t reports = {};
        adv_table_read_and_clear(&reports);
        ASSERT_EQ(1, reports.num_of_advs);
        CHECK_ADV_REPORT(adv, data, &reports.table[0]);
    }

    ASSERT_TRUE(adv_table_put(&adv));
    {
        adv_report_table_t reports = {};
        adv_table_read_and_clear(&reports);
        ASSERT_EQ(1, reports.num_of_advs);
        CHECK_ADV_REPORT(adv, data, &reports.table[0]);
    }
}

TEST_F(TestAdvTable, test_2) // NOLINT
{
    DECL_ADV_REPORT(adv1, 0x112233445566LLU, 1611154440, 50, data1, 0xA1U, 0xB1U);
    DECL_ADV_REPORT(adv2, 0x122333455667LLU, 1611154441, 51, data2, 0xA2U, 0xB2U, 0xC2U);

    ASSERT_TRUE(adv_table_put(&adv1));
    ASSERT_TRUE(adv_table_put(&adv2));

    {
        adv_report_table_t reports = {};
        adv_table_read_and_clear(&reports);
        ASSERT_EQ(2, reports.num_of_advs);
        CHECK_ADV_REPORT(adv1, data1, &reports.table[0]);
        CHECK_ADV_REPORT(adv2, data2, &reports.table[1]);
    }
}

TEST_F(TestAdvTable, test_2_with_the_same_hash) // NOLINT
{
    DECL_ADV_REPORT(adv1, 0x112233445566LLU, 1611154440, 50, data1, 0xA1U, 0xB1U);
    DECL_ADV_REPORT(adv2, 0x112232445567LLU, 1611154441, 51, data2, 0xA2U, 0xB2U, 0xC2U);

    ASSERT_EQ(adv_report_calc_hash(&adv1.tag_mac), adv_report_calc_hash(&adv2.tag_mac));

    ASSERT_TRUE(adv_table_put(&adv1));
    ASSERT_TRUE(adv_table_put(&adv2));

    {
        adv_report_table_t reports = {};
        adv_table_read_and_clear(&reports);
        ASSERT_EQ(2, reports.num_of_advs);
        CHECK_ADV_REPORT(adv1, data1, &reports.table[0]);
        CHECK_ADV_REPORT(adv2, data2, &reports.table[1]);
    }
}

TEST_F(TestAdvTable, test_2_with_the_same_hash_with_clear) // NOLINT
{
    DECL_ADV_REPORT(adv1, 0x112233445566LLU, 1611154440, 50, data1, 0xA1U, 0xB1U);

    ASSERT_TRUE(adv_table_put(&adv1));

    {
        adv_report_table_t reports = {};
        adv_table_read_and_clear(&reports);
        ASSERT_EQ(1, reports.num_of_advs);
        CHECK_ADV_REPORT(adv1, data1, &reports.table[0]);
    }

    DECL_ADV_REPORT(adv2, 0x112232445567LLU, 1611154441, 51, data2, 0xA2U, 0xB2U, 0xC2U);

    ASSERT_EQ(adv_report_calc_hash(&adv1.tag_mac), adv_report_calc_hash(&adv2.tag_mac));

    ASSERT_TRUE(adv_table_put(&adv2));

    {
        adv_report_table_t reports = {};
        adv_table_read_and_clear(&reports);
        ASSERT_EQ(1, reports.num_of_advs);
        CHECK_ADV_REPORT(adv2, data2, &reports.table[0]);
    }
}

TEST_F(TestAdvTable, test_3_overwrite_1) // NOLINT
{
    DECL_ADV_REPORT(adv1, 0x112233445566LLU, 1611154440, 50, data1, 0xA1U, 0xB1U);
    DECL_ADV_REPORT(adv2, 0x122334455667LLU, 1611154441, 51, data2, 0xA2U, 0xB2U, 0xC2U);
    DECL_ADV_REPORT(adv3, 0x112233445566LLU, 1611154443, 53, data3, 0xA3U, 0xB3U, 0xC3U, 0xD3U);

    ASSERT_TRUE(adv_table_put(&adv1));
    ASSERT_TRUE(adv_table_put(&adv2));
    ASSERT_TRUE(adv_table_put(&adv3));

    {
        adv_report_table_t reports = {};
        adv_table_read_and_clear(&reports);
        ASSERT_EQ(2, reports.num_of_advs);
        CHECK_ADV_REPORT(adv3, data3, &reports.table[0]);
        CHECK_ADV_REPORT(adv2, data2, &reports.table[1]);
    }
}

TEST_F(TestAdvTable, test_3_overwrite_2) // NOLINT
{
    DECL_ADV_REPORT(adv1, 0x112233445566LLU, 1611154440, 50, data1, 0xA1U, 0xB1U);
    DECL_ADV_REPORT(adv2, 0x122334455667LLU, 1611154441, 51, data2, 0xA2U, 0xB2U, 0xC2U);
    DECL_ADV_REPORT(adv3, 0x122334455667LLU, 1611154443, 53, data3, 0xA3U, 0xB3U, 0xC3U, 0xD3U);

    ASSERT_TRUE(adv_table_put(&adv1));
    ASSERT_TRUE(adv_table_put(&adv2));
    ASSERT_TRUE(adv_table_put(&adv3));

    {
        adv_report_table_t reports = {};
        adv_table_read_and_clear(&reports);
        ASSERT_EQ(2, reports.num_of_advs);
        CHECK_ADV_REPORT(adv1, data1, &reports.table[0]);
        CHECK_ADV_REPORT(adv3, data3, &reports.table[1]);
    }
}

TEST_F(TestAdvTable, test_3_with_the_same_hash_overwrite_1) // NOLINT
{
    DECL_ADV_REPORT(adv1, 0x112233445566LLU, 1611154440, 50, data1, 0xA1U, 0xB1U);
    DECL_ADV_REPORT(adv2, 0x112232445567LLU, 1611154441, 51, data2, 0xA2U, 0xB2U, 0xC2U);
    DECL_ADV_REPORT(adv3, 0x112233445566LLU, 1611154441, 51, data3, 0xA3U, 0xB3U, 0xC3U, 0xD3U);

    ASSERT_TRUE(adv_table_put(&adv1));
    ASSERT_TRUE(adv_table_put(&adv2));
    ASSERT_TRUE(adv_table_put(&adv3));

    {
        adv_report_table_t reports = {};
        adv_table_read_and_clear(&reports);
        ASSERT_EQ(2, reports.num_of_advs);
        CHECK_ADV_REPORT(adv3, data3, &reports.table[0]);
        CHECK_ADV_REPORT(adv2, data2, &reports.table[1]);
    }
}

TEST_F(TestAdvTable, test_3_with_the_same_hash_overwrite_2) // NOLINT
{
    DECL_ADV_REPORT(adv1, 0x112233445566LLU, 1611154440, 50, data1, 0xA1U, 0xB1U);
    DECL_ADV_REPORT(adv2, 0x112232445567LLU, 1611154441, 51, data2, 0xA2U, 0xB2U, 0xC2U);
    DECL_ADV_REPORT(adv3, 0x112232445567LLU, 1611154443, 53, data3, 0xA3U, 0xB3U, 0xC3U, 0xD3U);

    ASSERT_EQ(adv_report_calc_hash(&adv1.tag_mac), adv_report_calc_hash(&adv2.tag_mac));

    ASSERT_TRUE(adv_table_put(&adv1));
    ASSERT_TRUE(adv_table_put(&adv2));
    ASSERT_TRUE(adv_table_put(&adv3));

    {
        adv_report_table_t reports = {};
        adv_table_read_and_clear(&reports);
        ASSERT_EQ(2, reports.num_of_advs);
        CHECK_ADV_REPORT(adv1, data1, &reports.table[0]);
        CHECK_ADV_REPORT(adv3, data3, &reports.table[1]);
    }
}
