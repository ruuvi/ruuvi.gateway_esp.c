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
os_mutex_create_static(os_mutex_static_t* const p_mutex_static)
{
    return reinterpret_cast<os_mutex_t>(p_mutex_static);
}

void
os_mutex_delete(os_mutex_t* const ph_mutex)
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

#define NUMARGS(...) (sizeof((int[]) { __VA_ARGS__ }) / sizeof(int))

#define DECL_ADV_REPORT(adv_var_name_, mac_addr_, timestamp_, rssi_, data_var_name_, ...) \
    auto               data_var_name_ = make_array<uint8_t>(__VA_ARGS__); \
    const adv_report_t adv_var_name_  = { \
         .timestamp = timestamp_, \
         .tag_mac   = { ((mac_addr_) >> 5U * 8U) & 0xFFU, \
                        ((mac_addr_) >> 4U * 8U) & 0xFFU, \
                        ((mac_addr_) >> 3U * 8U) & 0xFFU, \
                        ((mac_addr_) >> 2U * 8U) & 0xFFU, \
                        ((mac_addr_) >> 1U * 8U) & 0xFFU, \
                        ((mac_addr_) >> 0U * 8U) & 0xFFU }, \
         .rssi      = rssi_, \
         .data_len  = data_var_name_.size(), \
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

template<typename V, typename... T>
constexpr auto
make_array(T&&... values) -> std::array<V, sizeof...(T)>
{
    return std::array<V, sizeof...(T)> { std::forward<V>(values)... };
}

/*** Unit-Tests
 * *******************************************************************************************************/

TEST_F(TestAdvTable, test_1) // NOLINT
{
    const uint64_t    mac_addr       = 0x112233445566LLU;
    const time_t      base_timestamp = 1611154440;
    const wifi_rssi_t rssi           = 50;
    DECL_ADV_REPORT(adv, mac_addr, base_timestamp, rssi, data, 0xAAU, 0xBBU);

    adv_table_put(&adv);
    {
        adv_report_table_t reports = {};
        adv_table_read_retransmission_list1_and_clear(&reports);
        ASSERT_EQ(1, reports.num_of_advs);
        CHECK_ADV_REPORT(adv, data, &reports.table[0]);
    }

    // Test reading of the history
    const uint32_t time_interval_seconds = 10;
    {
        adv_report_table_t reports = {};
        adv_table_history_read(&reports, base_timestamp + 1, true, time_interval_seconds, true);
        ASSERT_EQ(1, reports.num_of_advs);
        CHECK_ADV_REPORT(adv, data, &reports.table[0]);
    }

    {
        adv_report_table_t reports = {};
        adv_table_history_read(&reports, base_timestamp + time_interval_seconds, true, time_interval_seconds, true);
        ASSERT_EQ(1, reports.num_of_advs);
        CHECK_ADV_REPORT(adv, data, &reports.table[0]);
    }

    {
        adv_report_table_t reports = {};
        adv_table_history_read(&reports, base_timestamp + time_interval_seconds + 1, true, time_interval_seconds, true);
        ASSERT_EQ(0, reports.num_of_advs);
    }
}

TEST_F(TestAdvTable, test_1_read_clear_add) // NOLINT
{
    const uint64_t    mac_addr       = 0x112233445566LLU;
    const time_t      base_timestamp = 1611154440;
    const wifi_rssi_t rssi           = 50;
    DECL_ADV_REPORT(adv, mac_addr, base_timestamp, rssi, data, 0xAAU, 0xBBU);

    adv_table_put(&adv);
    {
        adv_report_table_t reports = {};
        adv_table_read_retransmission_list1_and_clear(&reports);
        ASSERT_EQ(1, reports.num_of_advs);
        CHECK_ADV_REPORT(adv, data, &reports.table[0]);
    }

    adv_table_put(&adv);
    {
        adv_report_table_t reports = {};
        adv_table_read_retransmission_list1_and_clear(&reports);
        ASSERT_EQ(1, reports.num_of_advs);
        CHECK_ADV_REPORT(adv, data, &reports.table[0]);
    }

    // Test reading of the history
    const uint32_t time_interval_seconds = 10;
    {
        adv_report_table_t reports = {};
        adv_table_history_read(&reports, base_timestamp + 1, true, time_interval_seconds, true);
        ASSERT_EQ(1, reports.num_of_advs);
        CHECK_ADV_REPORT(adv, data, &reports.table[0]);
    }
}

TEST_F(TestAdvTable, test_2) // NOLINT
{
    const uint64_t    mac_addr       = 0x112233445566LLU;
    const time_t      base_timestamp = 1611154440;
    const wifi_rssi_t rssi           = 50;
    DECL_ADV_REPORT(adv1, mac_addr + 0, base_timestamp + 0, rssi + 0, data1, 0xA1U, 0xB1U);
    DECL_ADV_REPORT(adv2, mac_addr + 1, base_timestamp + 1, rssi + 1, data2, 0xA2U, 0xB2U, 0xC2);

    adv_table_put(&adv1);
    adv_table_put(&adv2);

    {
        adv_report_table_t reports = {};
        adv_table_read_retransmission_list1_and_clear(&reports);
        ASSERT_EQ(2, reports.num_of_advs);
        CHECK_ADV_REPORT(adv1, data1, &reports.table[0]);
        CHECK_ADV_REPORT(adv2, data2, &reports.table[1]);
    }

    // Test reading of the history
    const uint32_t time_interval_seconds = 10;
    {
        adv_report_table_t reports = {};
        adv_table_history_read(&reports, base_timestamp + time_interval_seconds, true, time_interval_seconds, true);
        ASSERT_EQ(2, reports.num_of_advs);
        CHECK_ADV_REPORT(adv2, data2, &reports.table[0]);
        CHECK_ADV_REPORT(adv1, data1, &reports.table[1]);
    }

    {
        adv_report_table_t reports = {};
        adv_table_history_read(&reports, base_timestamp + time_interval_seconds + 1, true, time_interval_seconds, true);
        ASSERT_EQ(1, reports.num_of_advs);
        CHECK_ADV_REPORT(adv2, data2, &reports.table[0]);
    }

    {
        adv_report_table_t reports = {};
        adv_table_history_read(&reports, base_timestamp + time_interval_seconds + 2, true, time_interval_seconds, true);
        ASSERT_EQ(0, reports.num_of_advs);
    }
}

TEST_F(TestAdvTable, test_2_filter_by_timestamp) // NOLINT
{
    const uint64_t    mac_addr       = 0x112233445566LLU;
    const time_t      base_timestamp = 1611154440;
    const wifi_rssi_t rssi           = 50;
    DECL_ADV_REPORT(adv1, mac_addr + 0, base_timestamp + 0, rssi + 0, data1, 0xA1U, 0xB1U);
    DECL_ADV_REPORT(adv2, mac_addr + 1, base_timestamp + 5, rssi + 1, data2, 0xA2U, 0xB2U, 0xC2);

    adv_table_put(&adv1);
    adv_table_put(&adv2);

    {
        adv_report_table_t reports = {};
        adv_table_read_retransmission_list1_and_clear(&reports);
        ASSERT_EQ(2, reports.num_of_advs);
        CHECK_ADV_REPORT(adv1, data1, &reports.table[0]);
        CHECK_ADV_REPORT(adv2, data2, &reports.table[1]);
    }

    // Test reading of the history
    const uint32_t time_interval_seconds = 5;
    {
        adv_report_table_t reports        = {};
        const uint32_t     filter_seconds = 3;
        adv_table_history_read(&reports, base_timestamp + time_interval_seconds, true, filter_seconds, true);
        ASSERT_EQ(1, reports.num_of_advs);
        CHECK_ADV_REPORT(adv2, data2, &reports.table[0]);
    }

    {
        adv_report_table_t reports        = {};
        const uint32_t     filter_seconds = 3;
        adv_table_history_read(&reports, base_timestamp + time_interval_seconds, true, filter_seconds, false);
        ASSERT_EQ(2, reports.num_of_advs);
        CHECK_ADV_REPORT(adv2, data2, &reports.table[0]);
        CHECK_ADV_REPORT(adv1, data1, &reports.table[1]);
    }

    {
        adv_report_table_t reports        = {};
        const uint32_t     filter_seconds = 5;
        adv_table_history_read(&reports, base_timestamp + time_interval_seconds, true, filter_seconds, true);
        ASSERT_EQ(2, reports.num_of_advs);
        CHECK_ADV_REPORT(adv2, data2, &reports.table[0]);
        CHECK_ADV_REPORT(adv1, data1, &reports.table[1]);
    }
}

TEST_F(TestAdvTable, test_2_without_timestamps) // NOLINT
{
    const uint64_t    mac_addr     = 0x112233445566LLU;
    const time_t      base_counter = 1000;
    const wifi_rssi_t rssi         = 50;
    DECL_ADV_REPORT(adv1, mac_addr + 0, base_counter + 1, rssi + 0, data1, 0xA1U, 0xB1U);
    DECL_ADV_REPORT(adv2, mac_addr + 1, base_counter + 2, rssi + 1, data2, 0xA2U, 0xB2U, 0xC2);

    adv_table_put(&adv1);
    adv_table_put(&adv2);

    {
        adv_report_table_t reports = {};
        adv_table_read_retransmission_list1_and_clear(&reports);
        ASSERT_EQ(2, reports.num_of_advs);
        CHECK_ADV_REPORT(adv1, data1, &reports.table[0]);
        CHECK_ADV_REPORT(adv2, data2, &reports.table[1]);
    }

    // Test reading of the history
    {
        adv_report_table_t reports        = {};
        const uint32_t     filter_counter = 1000;
        adv_table_history_read(&reports, 0, false, filter_counter, true);
        ASSERT_EQ(2, reports.num_of_advs);
        CHECK_ADV_REPORT(adv2, data2, &reports.table[0]);
        CHECK_ADV_REPORT(adv1, data1, &reports.table[1]);
    }
    {
        adv_report_table_t reports        = {};
        const uint32_t     filter_counter = 1001;
        adv_table_history_read(&reports, 0, false, filter_counter, true);
        ASSERT_EQ(1, reports.num_of_advs);
        CHECK_ADV_REPORT(adv2, data2, &reports.table[0]);
    }
    {
        adv_report_table_t reports        = {};
        const uint32_t     filter_counter = 1002;
        adv_table_history_read(&reports, 0, false, filter_counter, true);
        ASSERT_EQ(0, reports.num_of_advs);
    }

    {
        adv_report_table_t reports        = {};
        const uint32_t     filter_counter = 1002;
        adv_table_history_read(&reports, 0, false, filter_counter, false);
        ASSERT_EQ(2, reports.num_of_advs);
        CHECK_ADV_REPORT(adv2, data2, &reports.table[0]);
        CHECK_ADV_REPORT(adv1, data1, &reports.table[1]);
    }
}

TEST_F(TestAdvTable, test_3_without_timestamps_counter_overflow) // NOLINT
{
    const uint64_t    mac_addr     = 0x112233445566LLU;
    const time_t      base_counter = 0xfffffffe;
    const wifi_rssi_t rssi         = 50;
    DECL_ADV_REPORT(adv1, mac_addr + 0, base_counter + 1, rssi + 0, data1, 0xA1U, 0xB1U);
    DECL_ADV_REPORT(adv2, mac_addr + 1, base_counter + 2, rssi + 1, data2, 0xA2U, 0xB2U, 0xC2);
    DECL_ADV_REPORT(adv3, mac_addr + 2, base_counter + 3, rssi + 2, data3, 0xA3U, 0xB3U, 0xC3);

    adv_table_put(&adv1);
    adv_table_put(&adv2);
    adv_table_put(&adv3);

    {
        adv_report_table_t reports = {};
        adv_table_read_retransmission_list1_and_clear(&reports);
        ASSERT_EQ(3, reports.num_of_advs);
        CHECK_ADV_REPORT(adv1, data1, &reports.table[0]);
        CHECK_ADV_REPORT(adv2, data2, &reports.table[1]);
        CHECK_ADV_REPORT(adv3, data3, &reports.table[2]);
    }

    // Test reading of the history
    {
        adv_report_table_t reports        = {};
        const uint32_t     filter_counter = 0xfffffffe;
        adv_table_history_read(&reports, 0, false, filter_counter, true);
        ASSERT_EQ(3, reports.num_of_advs);
        CHECK_ADV_REPORT(adv3, data3, &reports.table[0]);
        CHECK_ADV_REPORT(adv2, data2, &reports.table[1]);
        CHECK_ADV_REPORT(adv1, data1, &reports.table[2]);
    }
    {
        adv_report_table_t reports        = {};
        const uint32_t     filter_counter = 0xffffffff;
        adv_table_history_read(&reports, 0, false, filter_counter, true);
        ASSERT_EQ(2, reports.num_of_advs);
        CHECK_ADV_REPORT(adv3, data3, &reports.table[0]);
        CHECK_ADV_REPORT(adv2, data2, &reports.table[1]);
    }
    {
        adv_report_table_t reports        = {};
        const uint32_t     filter_counter = 0;
        adv_table_history_read(&reports, 0, false, filter_counter, true);
        ASSERT_EQ(1, reports.num_of_advs);
        CHECK_ADV_REPORT(adv3, data3, &reports.table[0]);
    }
    {
        adv_report_table_t reports        = {};
        const uint32_t     filter_counter = 1;
        adv_table_history_read(&reports, 0, false, filter_counter, true);
        ASSERT_EQ(0, reports.num_of_advs);
    }

    {
        adv_report_table_t reports        = {};
        const uint32_t     filter_counter = 0;
        adv_table_history_read(&reports, 0, false, filter_counter, false);
        ASSERT_EQ(3, reports.num_of_advs);
        CHECK_ADV_REPORT(adv3, data3, &reports.table[0]);
        CHECK_ADV_REPORT(adv2, data2, &reports.table[1]);
        CHECK_ADV_REPORT(adv1, data1, &reports.table[2]);
    }
}

TEST_F(TestAdvTable, test_2_with_the_same_hash) // NOLINT
{
    const uint64_t    mac_addr       = 0x112233445566LLU;
    const time_t      base_timestamp = 1611154440;
    const wifi_rssi_t rssi           = 50;
    DECL_ADV_REPORT(adv1, mac_addr ^ 0x000000000000LLU, base_timestamp + 0, rssi + 0, data1, 0xA1U, 0xB1U);
    DECL_ADV_REPORT(adv2, mac_addr ^ 0x000001000001LLU, base_timestamp + 1, rssi + 1, data2, 0xA2U, 0xB2U, 0xC2);

    ASSERT_EQ(adv_report_calc_hash(&adv1.tag_mac), adv_report_calc_hash(&adv2.tag_mac));

    adv_table_put(&adv1);
    adv_table_put(&adv2);

    {
        adv_report_table_t reports = {};
        adv_table_read_retransmission_list1_and_clear(&reports);
        ASSERT_EQ(2, reports.num_of_advs);
        CHECK_ADV_REPORT(adv1, data1, &reports.table[0]);
        CHECK_ADV_REPORT(adv2, data2, &reports.table[1]);
    }

    // Test reading of the history
    const uint32_t time_interval_seconds = 10;
    {
        adv_report_table_t reports = {};
        adv_table_history_read(&reports, base_timestamp + time_interval_seconds, true, time_interval_seconds, true);
        ASSERT_EQ(2, reports.num_of_advs);
        CHECK_ADV_REPORT(adv2, data2, &reports.table[0]);
        CHECK_ADV_REPORT(adv1, data1, &reports.table[1]);
    }

    {
        adv_report_table_t reports = {};
        adv_table_history_read(&reports, base_timestamp + time_interval_seconds + 1, true, time_interval_seconds, true);
        ASSERT_EQ(1, reports.num_of_advs);
        CHECK_ADV_REPORT(adv2, data2, &reports.table[0]);
    }

    {
        adv_report_table_t reports = {};
        adv_table_history_read(&reports, base_timestamp + time_interval_seconds + 2, true, time_interval_seconds, true);
        ASSERT_EQ(0, reports.num_of_advs);
    }
}

TEST_F(TestAdvTable, test_2_with_the_same_hash_with_clear) // NOLINT
{
    const uint64_t    mac_addr       = 0x112233445566LLU;
    const time_t      base_timestamp = 1611154440;
    const wifi_rssi_t rssi           = 50;
    DECL_ADV_REPORT(adv1, mac_addr ^ 0x000000000000LLU, base_timestamp + 0, rssi + 0, data1, 0xA1U, 0xB1U);

    adv_table_put(&adv1);

    {
        adv_report_table_t reports = {};
        adv_table_read_retransmission_list1_and_clear(&reports);
        ASSERT_EQ(1, reports.num_of_advs);
        CHECK_ADV_REPORT(adv1, data1, &reports.table[0]);
    }

    DECL_ADV_REPORT(adv2, mac_addr ^ 0x000001000001LLU, base_timestamp + 1, rssi + 1, data2, 0xA2U, 0xB2U, 0xC2);

    ASSERT_EQ(adv_report_calc_hash(&adv1.tag_mac), adv_report_calc_hash(&adv2.tag_mac));

    adv_table_put(&adv2);

    {
        adv_report_table_t reports = {};
        adv_table_read_retransmission_list1_and_clear(&reports);
        ASSERT_EQ(1, reports.num_of_advs);
        CHECK_ADV_REPORT(adv2, data2, &reports.table[0]);
    }

    // Test reading of the history
    const uint32_t time_interval_seconds = 10;
    {
        adv_report_table_t reports = {};
        adv_table_history_read(&reports, base_timestamp + time_interval_seconds, true, time_interval_seconds, true);
        ASSERT_EQ(2, reports.num_of_advs);
        CHECK_ADV_REPORT(adv2, data2, &reports.table[0]);
        CHECK_ADV_REPORT(adv1, data1, &reports.table[1]);
    }

    {
        adv_report_table_t reports = {};
        adv_table_history_read(&reports, base_timestamp + time_interval_seconds + 1, true, time_interval_seconds, true);
        ASSERT_EQ(1, reports.num_of_advs);
        CHECK_ADV_REPORT(adv2, data2, &reports.table[0]);
    }

    {
        adv_report_table_t reports = {};
        adv_table_history_read(&reports, base_timestamp + time_interval_seconds + 2, true, time_interval_seconds, true);
        ASSERT_EQ(0, reports.num_of_advs);
    }
}

TEST_F(TestAdvTable, test_3_overwrite_1) // NOLINT
{
    const uint64_t    mac            = 0x112233445566LLU;
    const time_t      base_timestamp = 1611154440;
    const wifi_rssi_t rssi           = 50;
    DECL_ADV_REPORT(adv1, mac ^ 0x000000000000LLU, base_timestamp + 0, rssi + 0, data1, 0xA1U, 0xB1U);
    DECL_ADV_REPORT(adv2, mac ^ 0x000000000001LLU, base_timestamp + 1, rssi + 1, data2, 0xA2U, 0xB2U, 0xC2);
    DECL_ADV_REPORT(adv3, mac ^ 0x000000000000LLU, base_timestamp + 2, rssi + 2, data3, 0xA3U, 0xB3U, 0xC3U, 0xD3U);

    adv_table_put(&adv1);
    adv_table_put(&adv2);
    adv_table_put(&adv3);

    {
        adv_report_table_t reports = {};
        adv_table_read_retransmission_list1_and_clear(&reports);
        ASSERT_EQ(2, reports.num_of_advs);
        CHECK_ADV_REPORT(adv3, data3, &reports.table[0]);
        CHECK_ADV_REPORT(adv2, data2, &reports.table[1]);
    }

    // Test reading of the history
    const uint32_t time_interval_seconds = 10;
    {
        adv_report_table_t reports = {};
        adv_table_history_read(&reports, base_timestamp + time_interval_seconds, true, time_interval_seconds, true);
        ASSERT_EQ(2, reports.num_of_advs);
        CHECK_ADV_REPORT(adv3, data3, &reports.table[0]);
        CHECK_ADV_REPORT(adv2, data2, &reports.table[1]);
    }

    {
        adv_report_table_t reports = {};
        adv_table_history_read(&reports, base_timestamp + time_interval_seconds + 1, true, time_interval_seconds, true);
        ASSERT_EQ(2, reports.num_of_advs);
        CHECK_ADV_REPORT(adv3, data3, &reports.table[0]);
        CHECK_ADV_REPORT(adv2, data2, &reports.table[1]);
    }

    {
        adv_report_table_t reports = {};
        adv_table_history_read(&reports, base_timestamp + time_interval_seconds + 2, true, time_interval_seconds, true);
        ASSERT_EQ(1, reports.num_of_advs);
        CHECK_ADV_REPORT(adv3, data3, &reports.table[0]);
    }

    {
        adv_report_table_t reports = {};
        adv_table_history_read(&reports, base_timestamp + time_interval_seconds + 3, true, time_interval_seconds, true);
        ASSERT_EQ(0, reports.num_of_advs);
    }
}

TEST_F(TestAdvTable, test_3_overwrite_2) // NOLINT
{
    const uint64_t    mac            = 0x112233445566LLU;
    const time_t      base_timestamp = 1611154440;
    const wifi_rssi_t rssi           = 50;
    DECL_ADV_REPORT(adv1, mac ^ 0x000000000000LLU, base_timestamp + 0, rssi + 0, data1, 0xA1U, 0xB1U);
    DECL_ADV_REPORT(adv2, mac ^ 0x000000000001LLU, base_timestamp + 1, rssi + 1, data2, 0xA2U, 0xB2U, 0xC2);
    DECL_ADV_REPORT(adv3, mac ^ 0x000000000001LLU, base_timestamp + 2, rssi + 2, data3, 0xA3U, 0xB3U, 0xC3U, 0xD3U);

    adv_table_put(&adv1);
    adv_table_put(&adv2);
    adv_table_put(&adv3);

    {
        adv_report_table_t reports = {};
        adv_table_read_retransmission_list1_and_clear(&reports);
        ASSERT_EQ(2, reports.num_of_advs);
        CHECK_ADV_REPORT(adv1, data1, &reports.table[0]);
        CHECK_ADV_REPORT(adv3, data3, &reports.table[1]);
    }

    // Test reading of the history
    const uint32_t time_interval_seconds = 10;
    {
        adv_report_table_t reports = {};
        adv_table_history_read(&reports, base_timestamp + time_interval_seconds, true, time_interval_seconds, true);
        ASSERT_EQ(2, reports.num_of_advs);
        CHECK_ADV_REPORT(adv3, data3, &reports.table[0]);
        CHECK_ADV_REPORT(adv1, data1, &reports.table[1]);
    }

    {
        adv_report_table_t reports = {};
        adv_table_history_read(&reports, base_timestamp + time_interval_seconds + 1, true, time_interval_seconds, true);
        ASSERT_EQ(1, reports.num_of_advs);
        CHECK_ADV_REPORT(adv3, data3, &reports.table[0]);
    }

    {
        adv_report_table_t reports = {};
        adv_table_history_read(&reports, base_timestamp + time_interval_seconds + 2, true, time_interval_seconds, true);
        ASSERT_EQ(1, reports.num_of_advs);
        CHECK_ADV_REPORT(adv3, data3, &reports.table[0]);
    }

    {
        adv_report_table_t reports = {};
        adv_table_history_read(&reports, base_timestamp + time_interval_seconds + 3, true, time_interval_seconds, true);
        ASSERT_EQ(0, reports.num_of_advs);
    }
}

TEST_F(TestAdvTable, test_3_with_the_same_hash_overwrite_1) // NOLINT
{
    const uint64_t    mac            = 0x112233445566LLU;
    const time_t      base_timestamp = 1611154440;
    const wifi_rssi_t rssi           = 50;
    DECL_ADV_REPORT(adv1, mac ^ 0x000000000000LLU, base_timestamp + 0, rssi + 0, data1, 0xA1U, 0xB1U);
    DECL_ADV_REPORT(adv2, mac ^ 0x000001000001LLU, base_timestamp + 1, rssi + 1, data2, 0xA2U, 0xB2U, 0xC2);
    DECL_ADV_REPORT(adv3, mac ^ 0x000000000000LLU, base_timestamp + 2, rssi + 2, data3, 0xA3U, 0xB3U, 0xC3U, 0xD3U);

    adv_table_put(&adv1);
    adv_table_put(&adv2);
    adv_table_put(&adv3);

    {
        adv_report_table_t reports = {};
        adv_table_read_retransmission_list1_and_clear(&reports);
        ASSERT_EQ(2, reports.num_of_advs);
        CHECK_ADV_REPORT(adv3, data3, &reports.table[0]);
        CHECK_ADV_REPORT(adv2, data2, &reports.table[1]);
    }

    // Test reading of the history
    const uint32_t time_interval_seconds = 10;
    {
        adv_report_table_t reports = {};
        adv_table_history_read(&reports, base_timestamp + time_interval_seconds, true, time_interval_seconds, true);
        ASSERT_EQ(2, reports.num_of_advs);
        CHECK_ADV_REPORT(adv3, data3, &reports.table[0]);
        CHECK_ADV_REPORT(adv2, data2, &reports.table[1]);
    }

    {
        adv_report_table_t reports = {};
        adv_table_history_read(&reports, base_timestamp + time_interval_seconds + 1, true, time_interval_seconds, true);
        ASSERT_EQ(2, reports.num_of_advs);
        CHECK_ADV_REPORT(adv3, data3, &reports.table[0]);
        CHECK_ADV_REPORT(adv2, data2, &reports.table[1]);
    }

    {
        adv_report_table_t reports = {};
        adv_table_history_read(&reports, base_timestamp + time_interval_seconds + 2, true, time_interval_seconds, true);
        ASSERT_EQ(1, reports.num_of_advs);
        CHECK_ADV_REPORT(adv3, data3, &reports.table[0]);
    }

    {
        adv_report_table_t reports = {};
        adv_table_history_read(&reports, base_timestamp + time_interval_seconds + 3, true, time_interval_seconds, true);
        ASSERT_EQ(0, reports.num_of_advs);
    }
}

TEST_F(TestAdvTable, test_3_with_the_same_hash_overwrite_2) // NOLINT
{
    const uint64_t    mac            = 0x112233445566LLU;
    const time_t      base_timestamp = 1611154440;
    const wifi_rssi_t rssi           = 50;
    DECL_ADV_REPORT(adv1, mac ^ 0x000000000000LLU, base_timestamp + 0, rssi + 0, data1, 0xA1U, 0xB1U);
    DECL_ADV_REPORT(adv2, mac ^ 0x000001000001LLU, base_timestamp + 1, rssi + 1, data2, 0xA2U, 0xB2U, 0xC2);
    DECL_ADV_REPORT(adv3, mac ^ 0x000001000001LLU, base_timestamp + 2, rssi + 2, data3, 0xA3U, 0xB3U, 0xC3U, 0xD3U);

    ASSERT_EQ(adv_report_calc_hash(&adv1.tag_mac), adv_report_calc_hash(&adv2.tag_mac));

    adv_table_put(&adv1);
    adv_table_put(&adv2);
    adv_table_put(&adv3);

    {
        adv_report_table_t reports = {};
        adv_table_read_retransmission_list1_and_clear(&reports);
        ASSERT_EQ(2, reports.num_of_advs);
        CHECK_ADV_REPORT(adv1, data1, &reports.table[0]);
        CHECK_ADV_REPORT(adv3, data3, &reports.table[1]);
    }

    // Test reading of the history
    const uint32_t time_interval_seconds = 10;
    {
        adv_report_table_t reports = {};
        adv_table_history_read(&reports, base_timestamp + time_interval_seconds, true, time_interval_seconds, true);
        ASSERT_EQ(2, reports.num_of_advs);
        CHECK_ADV_REPORT(adv3, data3, &reports.table[0]);
        CHECK_ADV_REPORT(adv1, data1, &reports.table[1]);
    }

    {
        adv_report_table_t reports = {};
        adv_table_history_read(&reports, base_timestamp + time_interval_seconds + 1, true, time_interval_seconds, true);
        ASSERT_EQ(1, reports.num_of_advs);
        CHECK_ADV_REPORT(adv3, data3, &reports.table[0]);
    }

    {
        adv_report_table_t reports = {};
        adv_table_history_read(&reports, base_timestamp + time_interval_seconds + 2, true, time_interval_seconds, true);
        ASSERT_EQ(1, reports.num_of_advs);
        CHECK_ADV_REPORT(adv3, data3, &reports.table[0]);
    }

    {
        adv_report_table_t reports = {};
        adv_table_history_read(&reports, base_timestamp + time_interval_seconds + 3, true, time_interval_seconds, true);
        ASSERT_EQ(0, reports.num_of_advs);
    }
}
