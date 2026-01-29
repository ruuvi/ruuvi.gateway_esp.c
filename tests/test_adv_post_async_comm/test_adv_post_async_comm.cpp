/**
 * @file test_adv_post_async_comm.cpp
 * @author TheSomeMan
 * @date 2023-09-17
 * @copyright Ruuvi Innovations Ltd, license BSD-3-Clause.
 */

#include "adv_post_async_comm.h"
#include "gtest/gtest.h"
#include "gw_cfg.h"
#include "adv_table.h"
#include "adv_post_signals.h"
#include "reset_task.h"
#include "ruuvi_gateway.h"
#include <string>

using namespace std;

class TestAdvPostAsyncComm;

static TestAdvPostAsyncComm* g_pTestClass;

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

/*** Google-test class implementation
 * *********************************************************************************/

class TestAdvPostAsyncComm : public ::testing::Test
{
private:
protected:
    void
    SetUp() override
    {
        g_pTestClass                                    = this;
        this->m_malloc_cnt                              = 0;
        this->m_malloc_fail_on_cnt                      = 0;
        this->m_esp_random_cnt                          = 0;
        this->m_cfg_http                                = {};
        this->m_gw_cfg_get_ntp_use_res                  = false;
        this->m_flag_time_is_synchronized               = false;
        this->m_http_server_mutex_try_lock_res          = false;
        this->m_http_server_mutex_locked                = false;
        this->m_gw_cfg_get_http_stat_use_http_stat_res  = false;
        this->m_gw_cfg_get_mqtt_use_mqtt_res            = false;
        this->m_hmac_sha256_set_key_for_http_ruuvi_res  = false;
        this->m_hmac_sha256_set_key_for_http_ruuvi_key  = string("");
        this->m_hmac_sha256_set_key_for_http_custom_res = false;
        this->m_hmac_sha256_set_key_for_http_custom_key = string("");
        this->m_hmac_sha256_set_key_for_stats_res       = false;
        this->m_hmac_sha256_set_key_for_stats_key       = string("");
        this->m_adv_post_statistics_do_send_res         = false;
        this->m_adv_post_statistics_do_send_call_cnt    = 0;
        this->m_gw_status_is_mqtt_connected_res         = false;

        this->m_mqtt_publish_adv_res                     = false;
        this->m_mqtt_publish_adv_call_cnt                = 0;
        this->m_mqtt_publish_adv_arg_adv                 = {};
        this->m_mqtt_publish_adv_arg_flag_use_timestamps = false;
        this->m_mqtt_publish_adv_arg_timestamp           = 0;

        this->m_http_async_poll_res                           = false;
        this->m_esp_get_free_heap_size_res                    = 0;
        this->m_default_period_for_http_ruuvi                 = 60 * 1000;
        this->m_default_period_for_http_custom                = 65 * 1000;
        this->m_adv_post_signals_send_sig                     = (adv_post_sig_e)OS_SIGNAL_NUM_NONE;
        this->m_adv_post_timers_start_timer_sig_do_async_comm = false;

        this->m_reports = {};

        this->m_leds_notify_http1_data_sent_fail = false;
        this->m_leds_notify_http2_data_sent_fail = false;

        this->m_send_sig_restart_services = 0;

        this->m_flag_gateway_restart_low_memory = false;

        adv_post_async_comm_init();
    }

    void
    TearDown() override
    {
        g_pTestClass = nullptr;
    }

public:
    TestAdvPostAsyncComm();

    ~TestAdvPostAsyncComm() override;

    MemAllocTrace       m_mem_alloc_trace;
    uint32_t            m_malloc_cnt {};
    uint32_t            m_malloc_fail_on_cnt {};
    uint32_t            m_esp_random_cnt {};
    ruuvi_gw_cfg_http_t m_cfg_http {};
    bool                m_gw_cfg_get_ntp_use_res { true };
    bool                m_flag_time_is_synchronized { true };
    bool                m_http_server_mutex_try_lock_res { true };
    bool                m_http_server_mutex_locked { false };
    bool                m_gw_cfg_get_http_stat_use_http_stat_res { false };
    bool                m_gw_cfg_get_mqtt_use_mqtt_res { true };
    bool                m_hmac_sha256_set_key_for_http_ruuvi_res { false };
    string              m_hmac_sha256_set_key_for_http_ruuvi_key {};
    bool                m_hmac_sha256_set_key_for_http_custom_res { false };
    string              m_hmac_sha256_set_key_for_http_custom_key {};
    bool                m_hmac_sha256_set_key_for_stats_res { true };
    string              m_hmac_sha256_set_key_for_stats_key {};
    bool                m_adv_post_statistics_do_send_res { true };
    uint32_t            m_adv_post_statistics_do_send_call_cnt { 0 };
    bool                m_gw_status_is_mqtt_connected_res { true };
    bool                m_mqtt_publish_adv_res { true };
    uint32_t            m_mqtt_publish_adv_call_cnt { 0 };
    adv_report_t        m_mqtt_publish_adv_arg_adv {};
    bool                m_mqtt_publish_adv_arg_flag_use_timestamps { false };
    time_t              m_mqtt_publish_adv_arg_timestamp {};
    bool                m_http_async_poll_res { true };
    uint32_t            m_esp_get_free_heap_size_res { 0 };
    adv_post_sig_e      m_adv_post_signals_send_sig {};
    bool                m_adv_post_timers_start_timer_sig_do_async_comm {};

    adv_report_table_t m_reports;

    bool                m_http_post_advs_res {};
    uint32_t            m_http_post_advs_call_cnt { 0 };
    adv_report_table_t  m_http_post_advs_arg_reports;
    uint32_t            m_http_post_advs_arg_nonce;
    bool                m_http_post_advs_arg_flag_use_timestamps;
    bool                m_http_post_advs_arg_flag_post_to_ruuvi;
    ruuvi_gw_cfg_http_t m_http_post_advs_arg_cfg_http;
    void*               m_http_post_advs_arg_p_user_data;

    uint32_t m_default_period_for_http_ruuvi {};
    uint32_t m_default_period_for_http_custom {};

    bool m_leds_notify_http1_data_sent_fail { false };
    bool m_leds_notify_http2_data_sent_fail { false };

    uint32_t m_send_sig_restart_services { 0 };

    bool m_flag_gateway_restart_low_memory { false };
};

TestAdvPostAsyncComm::TestAdvPostAsyncComm()
    : Test()
{
}

TestAdvPostAsyncComm::~TestAdvPostAsyncComm() = default;

extern "C" {

void*
os_malloc(const size_t size)
{
    assert(nullptr != g_pTestClass);
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
    assert(nullptr != g_pTestClass);
    g_pTestClass->m_mem_alloc_trace.remove(p_mem);
    free(p_mem);
}

void*
os_calloc(const size_t nmemb, const size_t size)
{
    assert(nullptr != g_pTestClass);
    if (++g_pTestClass->m_malloc_cnt == g_pTestClass->m_malloc_fail_on_cnt)
    {
        return nullptr;
    }
    void* p_mem = calloc(nmemb, size);
    assert(nullptr != p_mem);
    g_pTestClass->m_mem_alloc_trace.add(p_mem);
    return p_mem;
}

uint32_t
esp_random(void)
{
    return g_pTestClass->m_esp_random_cnt;
}

ruuvi_gw_cfg_http_t*
gw_cfg_get_http_copy(void)
{
    ruuvi_gw_cfg_http_t* p_cfg_http = (ruuvi_gw_cfg_http_t*)os_calloc(1, sizeof(*p_cfg_http));
    if (NULL == p_cfg_http)
    {
        return NULL;
    }
    memcpy(p_cfg_http, &g_pTestClass->m_cfg_http, sizeof(*p_cfg_http));
    return p_cfg_http;
}

bool
gw_cfg_get_ntp_use(void)
{
    return g_pTestClass->m_gw_cfg_get_ntp_use_res;
}

bool
gw_cfg_get_http_stat_use_http_stat(void)
{
    return g_pTestClass->m_gw_cfg_get_http_stat_use_http_stat_res;
}

bool
gw_cfg_get_mqtt_use_mqtt(void)
{
    return g_pTestClass->m_gw_cfg_get_mqtt_use_mqtt_res;
}

bool
http_post_advs(
    const adv_report_table_t* const  p_reports,
    const uint32_t                   nonce,
    const bool                       flag_use_timestamps,
    const bool                       flag_post_to_ruuvi,
    const ruuvi_gw_cfg_http_t* const p_cfg_http,
    void* const                      p_user_data)
{
    g_pTestClass->m_http_post_advs_arg_reports             = *p_reports;
    g_pTestClass->m_http_post_advs_arg_nonce               = nonce;
    g_pTestClass->m_http_post_advs_arg_flag_use_timestamps = flag_use_timestamps;
    g_pTestClass->m_http_post_advs_arg_flag_post_to_ruuvi  = flag_post_to_ruuvi;
    g_pTestClass->m_http_post_advs_arg_cfg_http            = *p_cfg_http;
    g_pTestClass->m_http_post_advs_arg_p_user_data         = p_user_data;

    g_pTestClass->m_http_post_advs_call_cnt += 1;

    return g_pTestClass->m_http_post_advs_res;
}

void
adv_table_read_retransmission_list1_and_clear(adv_report_table_t* const p_reports)
{
    *p_reports = g_pTestClass->m_reports;
}

void
adv_table_read_retransmission_list2_and_clear(adv_report_table_t* const p_reports)
{
    *p_reports = g_pTestClass->m_reports;
}

void
adv_table_read_retransmission_list3_and_clear(adv_report_table_t* const p_reports)
{
    *p_reports = g_pTestClass->m_reports;
}

void
leds_notify_http1_data_sent_fail(void)
{
    g_pTestClass->m_leds_notify_http1_data_sent_fail = true;
}

void
leds_notify_http2_data_sent_fail(void)
{
    g_pTestClass->m_leds_notify_http2_data_sent_fail = true;
}

bool
time_is_synchronized(void)
{
    return g_pTestClass->m_flag_time_is_synchronized;
}

bool
http_server_mutex_try_lock(void)
{
    if (g_pTestClass->m_http_server_mutex_try_lock_res)
    {
        assert(!g_pTestClass->m_http_server_mutex_locked);
        g_pTestClass->m_http_server_mutex_locked = true;
    }
    return g_pTestClass->m_http_server_mutex_try_lock_res;
}

void
http_server_mutex_unlock(void)
{
    assert(g_pTestClass->m_http_server_mutex_locked);
    g_pTestClass->m_http_server_mutex_locked = false;
}

bool
hmac_sha256_set_key_for_http_ruuvi(const char* const p_key)
{
    g_pTestClass->m_hmac_sha256_set_key_for_http_ruuvi_key = string(p_key);
    return g_pTestClass->m_hmac_sha256_set_key_for_http_ruuvi_res;
}

bool
hmac_sha256_set_key_for_http_custom(const char* const p_key)
{
    g_pTestClass->m_hmac_sha256_set_key_for_http_custom_key = string(p_key);
    return g_pTestClass->m_hmac_sha256_set_key_for_http_custom_res;
}

bool
hmac_sha256_set_key_for_stats(const char* const p_key)
{
    g_pTestClass->m_hmac_sha256_set_key_for_stats_key = string(p_key);
    return g_pTestClass->m_hmac_sha256_set_key_for_stats_res;
}

bool
adv_post_statistics_do_send(void)
{
    g_pTestClass->m_adv_post_statistics_do_send_call_cnt += 1;
    return g_pTestClass->m_adv_post_statistics_do_send_res;
}

bool
gw_status_is_mqtt_connected(void)
{
    return g_pTestClass->m_gw_status_is_mqtt_connected_res;
}

void
adv_post_signals_send_sig(const adv_post_sig_e adv_post_sig)
{
    g_pTestClass->m_adv_post_signals_send_sig = adv_post_sig;
}

bool
mqtt_publish_adv(const adv_report_t* const p_adv, const bool flag_use_timestamps, const time_t timestamp)
{
    g_pTestClass->m_mqtt_publish_adv_arg_adv                 = *p_adv;
    g_pTestClass->m_mqtt_publish_adv_arg_flag_use_timestamps = flag_use_timestamps;
    g_pTestClass->m_mqtt_publish_adv_arg_timestamp           = timestamp;
    g_pTestClass->m_mqtt_publish_adv_call_cnt += 1;
    return g_pTestClass->m_mqtt_publish_adv_res;
}

void
adv_post_timers_start_timer_sig_do_async_comm(void)
{
    g_pTestClass->m_adv_post_timers_start_timer_sig_do_async_comm = true;
}

bool
http_async_poll(void)
{
    return g_pTestClass->m_http_async_poll_res;
}

uint32_t
esp_get_free_heap_size(void)
{
    return g_pTestClass->m_esp_get_free_heap_size_res;
}

void
adv1_post_timer_set_default_period_by_server_resp(const uint32_t period_ms)
{
    g_pTestClass->m_default_period_for_http_ruuvi = period_ms;
}

void
adv2_post_timer_set_default_period_by_server_resp(const uint32_t period_ms)
{
    g_pTestClass->m_default_period_for_http_custom = period_ms;
}

void
main_task_send_sig_restart_services(void)
{
    g_pTestClass->m_send_sig_restart_services++;
}

void
ruuvi_log_heap_usage(void)
{
}

void
gateway_restart_low_memory(void)
{
    g_pTestClass->m_flag_gateway_restart_low_memory = true;
}

} // extern "C"

/*** Unit-Tests
 * *******************************************************************************************************/

TEST_F(TestAdvPostAsyncComm, test_regular_sequence) // NOLINT
{
    this->m_flag_time_is_synchronized              = true;
    this->m_http_server_mutex_try_lock_res         = true;
    this->m_http_post_advs_res                     = true;
    this->m_gw_cfg_get_http_stat_use_http_stat_res = true;
    this->m_gw_cfg_get_mqtt_use_mqtt_res           = true;
    this->m_gw_cfg_get_ntp_use_res                 = true;
    this->m_adv_post_statistics_do_send_res        = true;
    this->m_gw_status_is_mqtt_connected_res        = true;

    adv_post_state_t adv_post_state = {
        .flag_primary_time_sync_is_done  = false,
        .flag_network_connected          = true,
        .flag_async_comm_in_progress     = false,
        .flag_need_to_send_advs1         = true,
        .flag_need_to_send_advs2         = false,
        .flag_need_to_send_statistics    = false,
        .flag_need_to_send_mqtt_periodic = false,
        .flag_relaying_enabled           = true,
        .flag_use_timestamps             = true,
        .flag_stop                       = false,
    };
    this->m_reports = {
        .num_of_advs = 2,
        .table = {
            [0] = {
                .timestamp = 100500,
                .samples_counter = 301,
                .tag_mac = { 0x11, 0x12, 0x13, 0x14, 0x15, 0x16,},
                .rssi = -49,
                .data_len = 24,
                .data_buf = {
                    0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27, 0x28, 0x29, 0x2A, 0x2B, 0x2C, 0x2D, 0x2E, 0x2F,
                    0x30, 0x31, 0x32, 0x33, 0x34, 0x35,
                }
            },
            [1] = {
                .timestamp = 100501,
                .samples_counter = 302,
                .tag_mac = { 0x21, 0x22, 0x23, 0x24, 0x25, 0x26,},
                .rssi = -48,
                .data_len = 24,
                .data_buf = {
                    0x51, 0x52, 0x53, 0x54, 0x55, 0x56, 0x57, 0x58, 0x59, 0x5A, 0x5B, 0x5C, 0x5D, 0x5E, 0x5F,
                    0x50, 0x51, 0x52, 0x53, 0x54, 0x55,
                }
            },
        }
    };

    adv_post_do_async_comm(&adv_post_state);
    ASSERT_FALSE(this->m_flag_gateway_restart_low_memory);
    ASSERT_TRUE(this->m_http_server_mutex_locked);
    ASSERT_FALSE(this->m_leds_notify_http1_data_sent_fail);
    ASSERT_FALSE(this->m_leds_notify_http2_data_sent_fail);
    ASSERT_EQ(1, this->m_http_post_advs_call_cnt);
    ASSERT_EQ(0, this->m_adv_post_statistics_do_send_call_cnt);
    ASSERT_EQ(0, this->m_mqtt_publish_adv_call_cnt);
    ASSERT_TRUE(this->m_adv_post_timers_start_timer_sig_do_async_comm);
    this->m_adv_post_timers_start_timer_sig_do_async_comm = false;
    ASSERT_EQ(ADV_POST_ACTION_POST_ADVS_TO_RUUVI, adv_post_get_adv_post_action());

    this->m_hmac_sha256_set_key_for_http_ruuvi_res = true;
    ASSERT_TRUE(adv_post_set_hmac_sha256_key("key_str123"));
    ASSERT_EQ(string("key_str123"), this->m_hmac_sha256_set_key_for_http_ruuvi_key);
    this->m_hmac_sha256_set_key_for_http_ruuvi_res = false;
    adv_post_set_default_period(15 * 1000);
    ASSERT_EQ(15 * 1000, this->m_default_period_for_http_ruuvi);

    this->m_http_async_poll_res = true;
    adv_post_do_async_comm(&adv_post_state);
    ASSERT_FALSE(this->m_flag_gateway_restart_low_memory);
    ASSERT_FALSE(this->m_http_server_mutex_locked);
    ASSERT_FALSE(this->m_leds_notify_http1_data_sent_fail);
    ASSERT_FALSE(this->m_leds_notify_http2_data_sent_fail);
    ASSERT_EQ(1, this->m_http_post_advs_call_cnt);
    ASSERT_EQ(0, this->m_adv_post_statistics_do_send_call_cnt);
    ASSERT_EQ(0, this->m_mqtt_publish_adv_call_cnt);
    this->m_http_async_poll_res = false;
    ASSERT_EQ(ADV_POST_ACTION_NONE, adv_post_get_adv_post_action());

    adv_post_state.flag_need_to_send_advs1          = false;
    adv_post_state.flag_need_to_send_advs2          = true;
    this->m_hmac_sha256_set_key_for_http_custom_res = true;
    adv_post_do_async_comm(&adv_post_state);
    ASSERT_FALSE(this->m_flag_gateway_restart_low_memory);
    ASSERT_TRUE(this->m_http_server_mutex_locked);
    ASSERT_FALSE(this->m_leds_notify_http1_data_sent_fail);
    ASSERT_FALSE(this->m_leds_notify_http2_data_sent_fail);
    ASSERT_EQ(2, this->m_http_post_advs_call_cnt);
    ASSERT_EQ(0, this->m_adv_post_statistics_do_send_call_cnt);
    ASSERT_EQ(0, this->m_mqtt_publish_adv_call_cnt);
    ASSERT_TRUE(adv_post_set_hmac_sha256_key("key_str124"));
    ASSERT_EQ(string("key_str124"), this->m_hmac_sha256_set_key_for_http_custom_key);
    this->m_hmac_sha256_set_key_for_http_custom_res = false;
    adv_post_set_default_period(25 * 1000);
    ASSERT_EQ(25 * 1000, this->m_default_period_for_http_custom);

    this->m_http_async_poll_res = true;
    adv_post_do_async_comm(&adv_post_state);
    ASSERT_FALSE(this->m_flag_gateway_restart_low_memory);
    ASSERT_FALSE(this->m_http_server_mutex_locked);
    ASSERT_FALSE(this->m_leds_notify_http1_data_sent_fail);
    ASSERT_FALSE(this->m_leds_notify_http2_data_sent_fail);
    ASSERT_EQ(2, this->m_http_post_advs_call_cnt);
    ASSERT_EQ(0, this->m_adv_post_statistics_do_send_call_cnt);
    ASSERT_EQ(0, this->m_mqtt_publish_adv_call_cnt);
    this->m_http_async_poll_res = false;

    adv_post_state.flag_need_to_send_advs2      = false;
    adv_post_state.flag_need_to_send_statistics = true;
    this->m_hmac_sha256_set_key_for_stats_res   = true;
    adv_post_do_async_comm(&adv_post_state);
    ASSERT_FALSE(this->m_flag_gateway_restart_low_memory);
    ASSERT_TRUE(this->m_http_server_mutex_locked);
    ASSERT_FALSE(this->m_leds_notify_http1_data_sent_fail);
    ASSERT_FALSE(this->m_leds_notify_http2_data_sent_fail);
    ASSERT_EQ(2, this->m_http_post_advs_call_cnt);
    ASSERT_EQ(1, this->m_adv_post_statistics_do_send_call_cnt);
    ASSERT_EQ(0, this->m_mqtt_publish_adv_call_cnt);
    ASSERT_TRUE(adv_post_set_hmac_sha256_key("key_str125"));
    ASSERT_EQ(string("key_str125"), this->m_hmac_sha256_set_key_for_stats_key);
    this->m_hmac_sha256_set_key_for_stats_res = false;
    adv_post_set_default_period(35 * 1000);
    ASSERT_EQ(15 * 1000, this->m_default_period_for_http_ruuvi);
    ASSERT_EQ(25 * 1000, this->m_default_period_for_http_custom);

    this->m_http_async_poll_res = true;
    adv_post_do_async_comm(&adv_post_state);
    ASSERT_FALSE(this->m_flag_gateway_restart_low_memory);
    ASSERT_FALSE(this->m_http_server_mutex_locked);
    ASSERT_FALSE(this->m_leds_notify_http1_data_sent_fail);
    ASSERT_FALSE(this->m_leds_notify_http2_data_sent_fail);
    ASSERT_EQ(2, this->m_http_post_advs_call_cnt);
    ASSERT_EQ(1, this->m_adv_post_statistics_do_send_call_cnt);
    ASSERT_EQ(0, this->m_mqtt_publish_adv_call_cnt);
    this->m_http_async_poll_res = false;

    adv_post_state.flag_need_to_send_statistics    = false;
    adv_post_state.flag_need_to_send_mqtt_periodic = true;
    this->m_mqtt_publish_adv_res                   = true;
    adv_post_do_async_comm(&adv_post_state);
    ASSERT_FALSE(this->m_flag_gateway_restart_low_memory);
    ASSERT_FALSE(this->m_http_server_mutex_locked);
    ASSERT_FALSE(this->m_leds_notify_http1_data_sent_fail);
    ASSERT_FALSE(this->m_leds_notify_http2_data_sent_fail);
    ASSERT_EQ(2, this->m_http_post_advs_call_cnt);
    ASSERT_EQ(1, this->m_adv_post_statistics_do_send_call_cnt);
    ASSERT_EQ(0, this->m_mqtt_publish_adv_call_cnt);

    ASSERT_EQ(ADV_POST_SIG_DO_ASYNC_COMM, this->m_adv_post_signals_send_sig);
    ASSERT_TRUE(this->m_adv_post_timers_start_timer_sig_do_async_comm);
    this->m_adv_post_timers_start_timer_sig_do_async_comm = false;
    ASSERT_EQ(0, this->m_mqtt_publish_adv_arg_timestamp);

    this->m_http_async_poll_res = true;
    adv_post_do_async_comm(&adv_post_state);
    ASSERT_FALSE(this->m_flag_gateway_restart_low_memory);
    ASSERT_FALSE(this->m_http_server_mutex_locked);
    ASSERT_FALSE(this->m_leds_notify_http1_data_sent_fail);
    ASSERT_FALSE(this->m_leds_notify_http2_data_sent_fail);
    ASSERT_EQ(2, this->m_http_post_advs_call_cnt);
    ASSERT_EQ(1, this->m_adv_post_statistics_do_send_call_cnt);
    ASSERT_EQ(1, this->m_mqtt_publish_adv_call_cnt);
    ASSERT_TRUE(this->m_adv_post_timers_start_timer_sig_do_async_comm);
    this->m_adv_post_timers_start_timer_sig_do_async_comm = false;
    this->m_http_async_poll_res                           = false;

    ASSERT_TRUE(this->m_mqtt_publish_adv_arg_flag_use_timestamps);
    ASSERT_NE(0, this->m_mqtt_publish_adv_arg_timestamp);
    {
        const adv_report_t* const p_adv = &this->m_reports.table[0];
        ASSERT_EQ(p_adv->timestamp, this->m_mqtt_publish_adv_arg_adv.timestamp);
        ASSERT_EQ(p_adv->samples_counter, this->m_mqtt_publish_adv_arg_adv.samples_counter);
        ASSERT_EQ(p_adv->rssi, this->m_mqtt_publish_adv_arg_adv.rssi);
        ASSERT_EQ(p_adv->data_len, this->m_mqtt_publish_adv_arg_adv.data_len);
        {
            std::array<uint8_t, MAC_ADDRESS_NUM_BYTES> tag_mac;
            const mac_address_bin_t* const             p_mac = &this->m_mqtt_publish_adv_arg_adv.tag_mac;
            std::copy(std::begin(p_mac->mac), std::end(p_mac->mac), tag_mac.begin());
            std::array<uint8_t, MAC_ADDRESS_NUM_BYTES> tag_mac_expected;
            const mac_address_bin_t* const             p_mac_expected = &p_adv->tag_mac;
            std::copy(std::begin(p_mac_expected->mac), std::end(p_mac_expected->mac), tag_mac_expected.begin());
            ASSERT_EQ(tag_mac_expected, tag_mac);
        }
        {
            const size_t            len = p_adv->data_len;
            std::array<uint8_t, 24> data_buf;
            std::copy(
                &this->m_mqtt_publish_adv_arg_adv.data_buf[0],
                &this->m_mqtt_publish_adv_arg_adv.data_buf[len],
                data_buf.begin());
            std::array<uint8_t, 24> data_buf_expected;
            std::copy(&p_adv->data_buf[0], &p_adv->data_buf[len], data_buf_expected.begin());
            ASSERT_EQ(data_buf_expected, data_buf);
        }
    }

    this->m_mqtt_publish_adv_arg_timestamp = 0;
    this->m_http_async_poll_res            = true;
    adv_post_do_async_comm(&adv_post_state);
    ASSERT_FALSE(this->m_flag_gateway_restart_low_memory);
    ASSERT_FALSE(this->m_http_server_mutex_locked);
    ASSERT_FALSE(this->m_leds_notify_http1_data_sent_fail);
    ASSERT_FALSE(this->m_leds_notify_http2_data_sent_fail);
    ASSERT_EQ(2, this->m_http_post_advs_call_cnt);
    ASSERT_EQ(1, this->m_adv_post_statistics_do_send_call_cnt);
    ASSERT_EQ(2, this->m_mqtt_publish_adv_call_cnt);
    ASSERT_FALSE(this->m_adv_post_timers_start_timer_sig_do_async_comm);
    this->m_http_async_poll_res = false;
    ASSERT_TRUE(this->m_mqtt_publish_adv_arg_flag_use_timestamps);
    ASSERT_NE(0, this->m_mqtt_publish_adv_arg_timestamp);

    {
        const adv_report_t* const p_adv = &this->m_reports.table[1];
        ASSERT_EQ(p_adv->timestamp, this->m_mqtt_publish_adv_arg_adv.timestamp);
        ASSERT_EQ(p_adv->samples_counter, this->m_mqtt_publish_adv_arg_adv.samples_counter);
        ASSERT_EQ(p_adv->rssi, this->m_mqtt_publish_adv_arg_adv.rssi);
        ASSERT_EQ(p_adv->data_len, this->m_mqtt_publish_adv_arg_adv.data_len);
        {
            std::array<uint8_t, MAC_ADDRESS_NUM_BYTES> tag_mac;
            const mac_address_bin_t* const             p_mac = &this->m_mqtt_publish_adv_arg_adv.tag_mac;
            std::copy(std::begin(p_mac->mac), std::end(p_mac->mac), tag_mac.begin());
            std::array<uint8_t, MAC_ADDRESS_NUM_BYTES> tag_mac_expected;
            const mac_address_bin_t* const             p_mac_expected = &p_adv->tag_mac;
            std::copy(std::begin(p_mac_expected->mac), std::end(p_mac_expected->mac), tag_mac_expected.begin());
            ASSERT_EQ(tag_mac_expected, tag_mac);
        }
        {
            const size_t            len = p_adv->data_len;
            std::array<uint8_t, 24> data_buf;
            std::copy(
                &this->m_mqtt_publish_adv_arg_adv.data_buf[0],
                &this->m_mqtt_publish_adv_arg_adv.data_buf[len],
                data_buf.begin());
            std::array<uint8_t, 24> data_buf_expected;
            std::copy(&p_adv->data_buf[0], &p_adv->data_buf[len], data_buf_expected.begin());
            ASSERT_EQ(data_buf_expected, data_buf);
        }
    }
    ASSERT_TRUE(this->m_mem_alloc_trace.is_empty());
}

TEST_F(TestAdvPostAsyncComm, test_adv_post_set_adv_post_http_action) // NOLINT
{
    ASSERT_EQ(ADV_POST_ACTION_NONE, adv_post_get_adv_post_action());
    adv_post_set_adv_post_http_action(true);
    ASSERT_EQ(ADV_POST_ACTION_POST_ADVS_TO_RUUVI, adv_post_get_adv_post_action());
    adv_post_set_adv_post_http_action(false);
    ASSERT_EQ(ADV_POST_ACTION_POST_ADVS_TO_CUSTOM, adv_post_get_adv_post_action());
}

TEST_F(TestAdvPostAsyncComm, test_no_timestamps) // NOLINT
{
    this->m_flag_time_is_synchronized              = true;
    this->m_http_server_mutex_try_lock_res         = true;
    this->m_http_post_advs_res                     = true;
    this->m_gw_cfg_get_http_stat_use_http_stat_res = true;
    this->m_gw_cfg_get_mqtt_use_mqtt_res           = true;
    this->m_gw_cfg_get_ntp_use_res                 = true;
    this->m_adv_post_statistics_do_send_res        = true;
    this->m_gw_status_is_mqtt_connected_res        = true;

    adv_post_state_t adv_post_state = {
        .flag_primary_time_sync_is_done  = false,
        .flag_network_connected          = true,
        .flag_async_comm_in_progress     = false,
        .flag_need_to_send_advs1         = true,
        .flag_need_to_send_advs2         = false,
        .flag_need_to_send_statistics    = false,
        .flag_need_to_send_mqtt_periodic = false,
        .flag_relaying_enabled           = true,
        .flag_use_timestamps             = false,
        .flag_stop                       = false,
    };
    this->m_reports = {
        .num_of_advs = 2,
        .table = {
            [0] = {
                .timestamp = 100500,
                .samples_counter = 301,
                .tag_mac = { 0x11, 0x12, 0x13, 0x14, 0x15, 0x16,},
                .rssi = -49,
                .data_len = 24,
                .data_buf = {
                    0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27, 0x28, 0x29, 0x2A, 0x2B, 0x2C, 0x2D, 0x2E, 0x2F,
                    0x30, 0x31, 0x32, 0x33, 0x34, 0x35,
                }
            },
            [1] = {
                .timestamp = 100501,
                .samples_counter = 302,
                .tag_mac = { 0x21, 0x22, 0x23, 0x24, 0x25, 0x26,},
                .rssi = -48,
                .data_len = 24,
                .data_buf = {
                    0x51, 0x52, 0x53, 0x54, 0x55, 0x56, 0x57, 0x58, 0x59, 0x5A, 0x5B, 0x5C, 0x5D, 0x5E, 0x5F,
                    0x50, 0x51, 0x52, 0x53, 0x54, 0x55,
                }
            },
        }
    };

    adv_post_do_async_comm(&adv_post_state);
    ASSERT_FALSE(this->m_flag_gateway_restart_low_memory);
    ASSERT_TRUE(this->m_http_server_mutex_locked);
    ASSERT_FALSE(this->m_leds_notify_http1_data_sent_fail);
    ASSERT_FALSE(this->m_leds_notify_http2_data_sent_fail);
    ASSERT_EQ(1, this->m_http_post_advs_call_cnt);
    ASSERT_EQ(0, this->m_adv_post_statistics_do_send_call_cnt);
    ASSERT_EQ(0, this->m_mqtt_publish_adv_call_cnt);
    ASSERT_TRUE(this->m_adv_post_timers_start_timer_sig_do_async_comm);
    this->m_adv_post_timers_start_timer_sig_do_async_comm = false;

    this->m_hmac_sha256_set_key_for_http_ruuvi_res = true;
    ASSERT_TRUE(adv_post_set_hmac_sha256_key("key_str123"));
    ASSERT_EQ(string("key_str123"), this->m_hmac_sha256_set_key_for_http_ruuvi_key);
    this->m_hmac_sha256_set_key_for_http_ruuvi_res = false;
    adv_post_set_default_period(15 * 1000);
    ASSERT_EQ(15 * 1000, this->m_default_period_for_http_ruuvi);

    this->m_http_async_poll_res = true;
    adv_post_do_async_comm(&adv_post_state);
    ASSERT_FALSE(this->m_flag_gateway_restart_low_memory);
    ASSERT_FALSE(this->m_http_server_mutex_locked);
    ASSERT_FALSE(this->m_leds_notify_http1_data_sent_fail);
    ASSERT_FALSE(this->m_leds_notify_http2_data_sent_fail);
    ASSERT_EQ(1, this->m_http_post_advs_call_cnt);
    ASSERT_EQ(0, this->m_adv_post_statistics_do_send_call_cnt);
    ASSERT_EQ(0, this->m_mqtt_publish_adv_call_cnt);
    this->m_http_async_poll_res = false;

    adv_post_state.flag_need_to_send_advs1          = false;
    adv_post_state.flag_need_to_send_advs2          = true;
    this->m_hmac_sha256_set_key_for_http_custom_res = true;
    adv_post_do_async_comm(&adv_post_state);
    ASSERT_FALSE(this->m_flag_gateway_restart_low_memory);
    ASSERT_TRUE(this->m_http_server_mutex_locked);
    ASSERT_FALSE(this->m_leds_notify_http1_data_sent_fail);
    ASSERT_FALSE(this->m_leds_notify_http2_data_sent_fail);
    ASSERT_EQ(2, this->m_http_post_advs_call_cnt);
    ASSERT_EQ(0, this->m_adv_post_statistics_do_send_call_cnt);
    ASSERT_EQ(0, this->m_mqtt_publish_adv_call_cnt);
    ASSERT_TRUE(adv_post_set_hmac_sha256_key("key_str124"));
    ASSERT_EQ(string("key_str124"), this->m_hmac_sha256_set_key_for_http_custom_key);
    this->m_hmac_sha256_set_key_for_http_custom_res = false;
    adv_post_set_default_period(25 * 1000);
    ASSERT_EQ(25 * 1000, this->m_default_period_for_http_custom);

    this->m_http_async_poll_res = true;
    adv_post_do_async_comm(&adv_post_state);
    ASSERT_FALSE(this->m_flag_gateway_restart_low_memory);
    ASSERT_FALSE(this->m_http_server_mutex_locked);
    ASSERT_FALSE(this->m_leds_notify_http1_data_sent_fail);
    ASSERT_FALSE(this->m_leds_notify_http2_data_sent_fail);
    ASSERT_EQ(2, this->m_http_post_advs_call_cnt);
    ASSERT_EQ(0, this->m_adv_post_statistics_do_send_call_cnt);
    ASSERT_EQ(0, this->m_mqtt_publish_adv_call_cnt);
    this->m_http_async_poll_res = false;

    adv_post_state.flag_need_to_send_advs2      = false;
    adv_post_state.flag_need_to_send_statistics = true;
    this->m_hmac_sha256_set_key_for_stats_res   = true;
    adv_post_do_async_comm(&adv_post_state);
    ASSERT_FALSE(this->m_flag_gateway_restart_low_memory);
    ASSERT_TRUE(this->m_http_server_mutex_locked);
    ASSERT_FALSE(this->m_leds_notify_http1_data_sent_fail);
    ASSERT_FALSE(this->m_leds_notify_http2_data_sent_fail);
    ASSERT_EQ(2, this->m_http_post_advs_call_cnt);
    ASSERT_EQ(1, this->m_adv_post_statistics_do_send_call_cnt);
    ASSERT_EQ(0, this->m_mqtt_publish_adv_call_cnt);
    ASSERT_TRUE(adv_post_set_hmac_sha256_key("key_str125"));
    ASSERT_EQ(string("key_str125"), this->m_hmac_sha256_set_key_for_stats_key);
    this->m_hmac_sha256_set_key_for_stats_res = false;
    adv_post_set_default_period(35 * 1000);
    ASSERT_EQ(15 * 1000, this->m_default_period_for_http_ruuvi);
    ASSERT_EQ(25 * 1000, this->m_default_period_for_http_custom);

    this->m_http_async_poll_res = true;
    adv_post_do_async_comm(&adv_post_state);
    ASSERT_FALSE(this->m_flag_gateway_restart_low_memory);
    ASSERT_FALSE(this->m_http_server_mutex_locked);
    ASSERT_FALSE(this->m_leds_notify_http1_data_sent_fail);
    ASSERT_FALSE(this->m_leds_notify_http2_data_sent_fail);
    ASSERT_EQ(2, this->m_http_post_advs_call_cnt);
    ASSERT_EQ(1, this->m_adv_post_statistics_do_send_call_cnt);
    ASSERT_EQ(0, this->m_mqtt_publish_adv_call_cnt);
    this->m_http_async_poll_res = false;

    adv_post_state.flag_need_to_send_statistics    = false;
    adv_post_state.flag_need_to_send_mqtt_periodic = true;
    this->m_mqtt_publish_adv_res                   = true;
    adv_post_do_async_comm(&adv_post_state);
    ASSERT_FALSE(this->m_flag_gateway_restart_low_memory);
    ASSERT_FALSE(this->m_http_server_mutex_locked);
    ASSERT_FALSE(this->m_leds_notify_http1_data_sent_fail);
    ASSERT_FALSE(this->m_leds_notify_http2_data_sent_fail);
    ASSERT_EQ(2, this->m_http_post_advs_call_cnt);
    ASSERT_EQ(1, this->m_adv_post_statistics_do_send_call_cnt);
    ASSERT_EQ(0, this->m_mqtt_publish_adv_call_cnt);

    ASSERT_EQ(ADV_POST_SIG_DO_ASYNC_COMM, this->m_adv_post_signals_send_sig);
    ASSERT_TRUE(this->m_adv_post_timers_start_timer_sig_do_async_comm);
    this->m_adv_post_timers_start_timer_sig_do_async_comm = false;
    ASSERT_EQ(0, this->m_mqtt_publish_adv_arg_timestamp);

    this->m_http_async_poll_res = true;
    adv_post_do_async_comm(&adv_post_state);
    ASSERT_FALSE(this->m_flag_gateway_restart_low_memory);
    ASSERT_FALSE(this->m_http_server_mutex_locked);
    ASSERT_FALSE(this->m_leds_notify_http1_data_sent_fail);
    ASSERT_FALSE(this->m_leds_notify_http2_data_sent_fail);
    ASSERT_EQ(2, this->m_http_post_advs_call_cnt);
    ASSERT_EQ(1, this->m_adv_post_statistics_do_send_call_cnt);
    ASSERT_EQ(1, this->m_mqtt_publish_adv_call_cnt);
    ASSERT_TRUE(this->m_adv_post_timers_start_timer_sig_do_async_comm);
    this->m_adv_post_timers_start_timer_sig_do_async_comm = false;
    this->m_http_async_poll_res                           = false;

    ASSERT_TRUE(this->m_mqtt_publish_adv_arg_flag_use_timestamps);
    ASSERT_NE(0, this->m_mqtt_publish_adv_arg_timestamp);
    {
        const adv_report_t* const p_adv = &this->m_reports.table[0];
        ASSERT_EQ(p_adv->timestamp, this->m_mqtt_publish_adv_arg_adv.timestamp);
        ASSERT_EQ(p_adv->samples_counter, this->m_mqtt_publish_adv_arg_adv.samples_counter);
        ASSERT_EQ(p_adv->rssi, this->m_mqtt_publish_adv_arg_adv.rssi);
        ASSERT_EQ(p_adv->data_len, this->m_mqtt_publish_adv_arg_adv.data_len);
        {
            std::array<uint8_t, MAC_ADDRESS_NUM_BYTES> tag_mac;
            const mac_address_bin_t* const             p_mac = &this->m_mqtt_publish_adv_arg_adv.tag_mac;
            std::copy(std::begin(p_mac->mac), std::end(p_mac->mac), tag_mac.begin());
            std::array<uint8_t, MAC_ADDRESS_NUM_BYTES> tag_mac_expected;
            const mac_address_bin_t* const             p_mac_expected = &p_adv->tag_mac;
            std::copy(std::begin(p_mac_expected->mac), std::end(p_mac_expected->mac), tag_mac_expected.begin());
            ASSERT_EQ(tag_mac_expected, tag_mac);
        }
        {
            const size_t            len = p_adv->data_len;
            std::array<uint8_t, 24> data_buf;
            std::copy(
                &this->m_mqtt_publish_adv_arg_adv.data_buf[0],
                &this->m_mqtt_publish_adv_arg_adv.data_buf[len],
                data_buf.begin());
            std::array<uint8_t, 24> data_buf_expected;
            std::copy(&p_adv->data_buf[0], &p_adv->data_buf[len], data_buf_expected.begin());
            ASSERT_EQ(data_buf_expected, data_buf);
        }
    }

    this->m_mqtt_publish_adv_arg_timestamp = 0;
    this->m_http_async_poll_res            = true;
    adv_post_do_async_comm(&adv_post_state);
    ASSERT_FALSE(this->m_flag_gateway_restart_low_memory);
    ASSERT_FALSE(this->m_http_server_mutex_locked);
    ASSERT_FALSE(this->m_leds_notify_http1_data_sent_fail);
    ASSERT_FALSE(this->m_leds_notify_http2_data_sent_fail);
    ASSERT_EQ(2, this->m_http_post_advs_call_cnt);
    ASSERT_EQ(1, this->m_adv_post_statistics_do_send_call_cnt);
    ASSERT_EQ(2, this->m_mqtt_publish_adv_call_cnt);
    ASSERT_FALSE(this->m_adv_post_timers_start_timer_sig_do_async_comm);
    this->m_http_async_poll_res = false;
    ASSERT_TRUE(this->m_mqtt_publish_adv_arg_flag_use_timestamps);
    ASSERT_NE(0, this->m_mqtt_publish_adv_arg_timestamp);

    {
        const adv_report_t* const p_adv = &this->m_reports.table[1];
        ASSERT_EQ(p_adv->timestamp, this->m_mqtt_publish_adv_arg_adv.timestamp);
        ASSERT_EQ(p_adv->samples_counter, this->m_mqtt_publish_adv_arg_adv.samples_counter);
        ASSERT_EQ(p_adv->rssi, this->m_mqtt_publish_adv_arg_adv.rssi);
        ASSERT_EQ(p_adv->data_len, this->m_mqtt_publish_adv_arg_adv.data_len);
        {
            std::array<uint8_t, MAC_ADDRESS_NUM_BYTES> tag_mac;
            const mac_address_bin_t* const             p_mac = &this->m_mqtt_publish_adv_arg_adv.tag_mac;
            std::copy(std::begin(p_mac->mac), std::end(p_mac->mac), tag_mac.begin());
            std::array<uint8_t, MAC_ADDRESS_NUM_BYTES> tag_mac_expected;
            const mac_address_bin_t* const             p_mac_expected = &p_adv->tag_mac;
            std::copy(std::begin(p_mac_expected->mac), std::end(p_mac_expected->mac), tag_mac_expected.begin());
            ASSERT_EQ(tag_mac_expected, tag_mac);
        }
        {
            const size_t            len = p_adv->data_len;
            std::array<uint8_t, 24> data_buf;
            std::copy(
                &this->m_mqtt_publish_adv_arg_adv.data_buf[0],
                &this->m_mqtt_publish_adv_arg_adv.data_buf[len],
                data_buf.begin());
            std::array<uint8_t, 24> data_buf_expected;
            std::copy(&p_adv->data_buf[0], &p_adv->data_buf[len], data_buf_expected.begin());
            ASSERT_EQ(data_buf_expected, data_buf);
        }
    }
    ASSERT_TRUE(this->m_mem_alloc_trace.is_empty());
}

TEST_F(TestAdvPostAsyncComm, test_no_time_sync) // NOLINT
{
    this->m_flag_time_is_synchronized              = true;
    this->m_http_server_mutex_try_lock_res         = true;
    this->m_http_post_advs_res                     = true;
    this->m_gw_cfg_get_http_stat_use_http_stat_res = true;
    this->m_gw_cfg_get_mqtt_use_mqtt_res           = true;
    this->m_gw_cfg_get_ntp_use_res                 = false;
    this->m_adv_post_statistics_do_send_res        = true;
    this->m_gw_status_is_mqtt_connected_res        = true;

    adv_post_state_t adv_post_state = {
        .flag_primary_time_sync_is_done  = false,
        .flag_network_connected          = true,
        .flag_async_comm_in_progress     = false,
        .flag_need_to_send_advs1         = true,
        .flag_need_to_send_advs2         = false,
        .flag_need_to_send_statistics    = false,
        .flag_need_to_send_mqtt_periodic = false,
        .flag_relaying_enabled           = true,
        .flag_use_timestamps             = true,
        .flag_stop                       = false,
    };
    this->m_reports = {
        .num_of_advs = 2,
        .table = {
            [0] = {
                .timestamp = 100500,
                .samples_counter = 301,
                .tag_mac = { 0x11, 0x12, 0x13, 0x14, 0x15, 0x16,},
                .rssi = -49,
                .data_len = 24,
                .data_buf = {
                    0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27, 0x28, 0x29, 0x2A, 0x2B, 0x2C, 0x2D, 0x2E, 0x2F,
                    0x30, 0x31, 0x32, 0x33, 0x34, 0x35,
                }
            },
            [1] = {
                .timestamp = 100501,
                .samples_counter = 302,
                .tag_mac = { 0x21, 0x22, 0x23, 0x24, 0x25, 0x26,},
                .rssi = -48,
                .data_len = 24,
                .data_buf = {
                    0x51, 0x52, 0x53, 0x54, 0x55, 0x56, 0x57, 0x58, 0x59, 0x5A, 0x5B, 0x5C, 0x5D, 0x5E, 0x5F,
                    0x50, 0x51, 0x52, 0x53, 0x54, 0x55,
                }
            },
        }
    };

    adv_post_do_async_comm(&adv_post_state);
    ASSERT_FALSE(this->m_flag_gateway_restart_low_memory);
    ASSERT_TRUE(this->m_http_server_mutex_locked);
    ASSERT_FALSE(this->m_leds_notify_http1_data_sent_fail);
    ASSERT_FALSE(this->m_leds_notify_http2_data_sent_fail);
    ASSERT_EQ(1, this->m_http_post_advs_call_cnt);
    ASSERT_EQ(0, this->m_adv_post_statistics_do_send_call_cnt);
    ASSERT_EQ(0, this->m_mqtt_publish_adv_call_cnt);
    ASSERT_TRUE(this->m_adv_post_timers_start_timer_sig_do_async_comm);
    this->m_adv_post_timers_start_timer_sig_do_async_comm = false;

    this->m_hmac_sha256_set_key_for_http_ruuvi_res = true;
    ASSERT_TRUE(adv_post_set_hmac_sha256_key("key_str123"));
    ASSERT_EQ(string("key_str123"), this->m_hmac_sha256_set_key_for_http_ruuvi_key);
    this->m_hmac_sha256_set_key_for_http_ruuvi_res = false;
    adv_post_set_default_period(15 * 1000);
    ASSERT_EQ(15 * 1000, this->m_default_period_for_http_ruuvi);

    this->m_http_async_poll_res = true;
    adv_post_do_async_comm(&adv_post_state);
    ASSERT_FALSE(this->m_flag_gateway_restart_low_memory);
    ASSERT_FALSE(this->m_http_server_mutex_locked);
    ASSERT_FALSE(this->m_leds_notify_http1_data_sent_fail);
    ASSERT_FALSE(this->m_leds_notify_http2_data_sent_fail);
    ASSERT_EQ(1, this->m_http_post_advs_call_cnt);
    ASSERT_EQ(0, this->m_adv_post_statistics_do_send_call_cnt);
    ASSERT_EQ(0, this->m_mqtt_publish_adv_call_cnt);
    this->m_http_async_poll_res = false;

    adv_post_state.flag_need_to_send_advs1          = false;
    adv_post_state.flag_need_to_send_advs2          = true;
    this->m_hmac_sha256_set_key_for_http_custom_res = true;
    adv_post_do_async_comm(&adv_post_state);
    ASSERT_FALSE(this->m_flag_gateway_restart_low_memory);
    ASSERT_TRUE(this->m_http_server_mutex_locked);
    ASSERT_FALSE(this->m_leds_notify_http1_data_sent_fail);
    ASSERT_FALSE(this->m_leds_notify_http2_data_sent_fail);
    ASSERT_EQ(2, this->m_http_post_advs_call_cnt);
    ASSERT_EQ(0, this->m_adv_post_statistics_do_send_call_cnt);
    ASSERT_EQ(0, this->m_mqtt_publish_adv_call_cnt);
    ASSERT_TRUE(adv_post_set_hmac_sha256_key("key_str124"));
    ASSERT_EQ(string("key_str124"), this->m_hmac_sha256_set_key_for_http_custom_key);
    this->m_hmac_sha256_set_key_for_http_custom_res = false;
    adv_post_set_default_period(25 * 1000);
    ASSERT_EQ(25 * 1000, this->m_default_period_for_http_custom);

    this->m_http_async_poll_res = true;
    adv_post_do_async_comm(&adv_post_state);
    ASSERT_FALSE(this->m_flag_gateway_restart_low_memory);
    ASSERT_FALSE(this->m_http_server_mutex_locked);
    ASSERT_FALSE(this->m_leds_notify_http1_data_sent_fail);
    ASSERT_FALSE(this->m_leds_notify_http2_data_sent_fail);
    ASSERT_EQ(2, this->m_http_post_advs_call_cnt);
    ASSERT_EQ(0, this->m_adv_post_statistics_do_send_call_cnt);
    ASSERT_EQ(0, this->m_mqtt_publish_adv_call_cnt);
    this->m_http_async_poll_res = false;

    adv_post_state.flag_need_to_send_advs2      = false;
    adv_post_state.flag_need_to_send_statistics = true;
    this->m_hmac_sha256_set_key_for_stats_res   = true;
    adv_post_do_async_comm(&adv_post_state);
    ASSERT_FALSE(this->m_flag_gateway_restart_low_memory);
    ASSERT_TRUE(this->m_http_server_mutex_locked);
    ASSERT_FALSE(this->m_leds_notify_http1_data_sent_fail);
    ASSERT_FALSE(this->m_leds_notify_http2_data_sent_fail);
    ASSERT_EQ(2, this->m_http_post_advs_call_cnt);
    ASSERT_EQ(1, this->m_adv_post_statistics_do_send_call_cnt);
    ASSERT_EQ(0, this->m_mqtt_publish_adv_call_cnt);
    ASSERT_TRUE(adv_post_set_hmac_sha256_key("key_str125"));
    ASSERT_EQ(string("key_str125"), this->m_hmac_sha256_set_key_for_stats_key);
    this->m_hmac_sha256_set_key_for_stats_res = false;
    adv_post_set_default_period(35 * 1000);
    ASSERT_EQ(15 * 1000, this->m_default_period_for_http_ruuvi);
    ASSERT_EQ(25 * 1000, this->m_default_period_for_http_custom);

    this->m_http_async_poll_res = true;
    adv_post_do_async_comm(&adv_post_state);
    ASSERT_FALSE(this->m_flag_gateway_restart_low_memory);
    ASSERT_FALSE(this->m_http_server_mutex_locked);
    ASSERT_FALSE(this->m_leds_notify_http1_data_sent_fail);
    ASSERT_FALSE(this->m_leds_notify_http2_data_sent_fail);
    ASSERT_EQ(2, this->m_http_post_advs_call_cnt);
    ASSERT_EQ(1, this->m_adv_post_statistics_do_send_call_cnt);
    ASSERT_EQ(0, this->m_mqtt_publish_adv_call_cnt);
    this->m_http_async_poll_res = false;

    adv_post_state.flag_need_to_send_statistics    = false;
    adv_post_state.flag_need_to_send_mqtt_periodic = true;
    this->m_mqtt_publish_adv_res                   = true;
    adv_post_do_async_comm(&adv_post_state);
    ASSERT_FALSE(this->m_flag_gateway_restart_low_memory);
    ASSERT_FALSE(this->m_http_server_mutex_locked);
    ASSERT_FALSE(this->m_leds_notify_http1_data_sent_fail);
    ASSERT_FALSE(this->m_leds_notify_http2_data_sent_fail);
    ASSERT_EQ(2, this->m_http_post_advs_call_cnt);
    ASSERT_EQ(1, this->m_adv_post_statistics_do_send_call_cnt);
    ASSERT_EQ(0, this->m_mqtt_publish_adv_call_cnt);

    ASSERT_EQ(ADV_POST_SIG_DO_ASYNC_COMM, this->m_adv_post_signals_send_sig);
    ASSERT_TRUE(this->m_adv_post_timers_start_timer_sig_do_async_comm);
    this->m_adv_post_timers_start_timer_sig_do_async_comm = false;
    ASSERT_EQ(0, this->m_mqtt_publish_adv_arg_timestamp);

    this->m_http_async_poll_res = true;
    adv_post_do_async_comm(&adv_post_state);
    ASSERT_FALSE(this->m_flag_gateway_restart_low_memory);
    ASSERT_FALSE(this->m_http_server_mutex_locked);
    ASSERT_FALSE(this->m_leds_notify_http1_data_sent_fail);
    ASSERT_FALSE(this->m_leds_notify_http2_data_sent_fail);
    ASSERT_EQ(2, this->m_http_post_advs_call_cnt);
    ASSERT_EQ(1, this->m_adv_post_statistics_do_send_call_cnt);
    ASSERT_EQ(1, this->m_mqtt_publish_adv_call_cnt);
    ASSERT_TRUE(this->m_adv_post_timers_start_timer_sig_do_async_comm);
    this->m_adv_post_timers_start_timer_sig_do_async_comm = false;
    this->m_http_async_poll_res                           = false;

    ASSERT_FALSE(this->m_mqtt_publish_adv_arg_flag_use_timestamps);
    ASSERT_EQ(0, this->m_mqtt_publish_adv_arg_timestamp);
    {
        const adv_report_t* const p_adv = &this->m_reports.table[0];
        ASSERT_EQ(p_adv->timestamp, this->m_mqtt_publish_adv_arg_adv.timestamp);
        ASSERT_EQ(p_adv->samples_counter, this->m_mqtt_publish_adv_arg_adv.samples_counter);
        ASSERT_EQ(p_adv->rssi, this->m_mqtt_publish_adv_arg_adv.rssi);
        ASSERT_EQ(p_adv->data_len, this->m_mqtt_publish_adv_arg_adv.data_len);
        {
            std::array<uint8_t, MAC_ADDRESS_NUM_BYTES> tag_mac;
            const mac_address_bin_t* const             p_mac = &this->m_mqtt_publish_adv_arg_adv.tag_mac;
            std::copy(std::begin(p_mac->mac), std::end(p_mac->mac), tag_mac.begin());
            std::array<uint8_t, MAC_ADDRESS_NUM_BYTES> tag_mac_expected;
            const mac_address_bin_t* const             p_mac_expected = &p_adv->tag_mac;
            std::copy(std::begin(p_mac_expected->mac), std::end(p_mac_expected->mac), tag_mac_expected.begin());
            ASSERT_EQ(tag_mac_expected, tag_mac);
        }
        {
            const size_t            len = p_adv->data_len;
            std::array<uint8_t, 24> data_buf;
            std::copy(
                &this->m_mqtt_publish_adv_arg_adv.data_buf[0],
                &this->m_mqtt_publish_adv_arg_adv.data_buf[len],
                data_buf.begin());
            std::array<uint8_t, 24> data_buf_expected;
            std::copy(&p_adv->data_buf[0], &p_adv->data_buf[len], data_buf_expected.begin());
            ASSERT_EQ(data_buf_expected, data_buf);
        }
    }

    this->m_mqtt_publish_adv_arg_timestamp = 0;
    this->m_http_async_poll_res            = true;
    adv_post_do_async_comm(&adv_post_state);
    ASSERT_FALSE(this->m_flag_gateway_restart_low_memory);
    ASSERT_FALSE(this->m_http_server_mutex_locked);
    ASSERT_FALSE(this->m_leds_notify_http1_data_sent_fail);
    ASSERT_FALSE(this->m_leds_notify_http2_data_sent_fail);
    ASSERT_EQ(2, this->m_http_post_advs_call_cnt);
    ASSERT_EQ(1, this->m_adv_post_statistics_do_send_call_cnt);
    ASSERT_EQ(2, this->m_mqtt_publish_adv_call_cnt);
    ASSERT_FALSE(this->m_adv_post_timers_start_timer_sig_do_async_comm);
    this->m_http_async_poll_res = false;
    ASSERT_FALSE(this->m_mqtt_publish_adv_arg_flag_use_timestamps);
    ASSERT_EQ(0, this->m_mqtt_publish_adv_arg_timestamp);

    {
        const adv_report_t* const p_adv = &this->m_reports.table[1];
        ASSERT_EQ(p_adv->timestamp, this->m_mqtt_publish_adv_arg_adv.timestamp);
        ASSERT_EQ(p_adv->samples_counter, this->m_mqtt_publish_adv_arg_adv.samples_counter);
        ASSERT_EQ(p_adv->rssi, this->m_mqtt_publish_adv_arg_adv.rssi);
        ASSERT_EQ(p_adv->data_len, this->m_mqtt_publish_adv_arg_adv.data_len);
        {
            std::array<uint8_t, MAC_ADDRESS_NUM_BYTES> tag_mac;
            const mac_address_bin_t* const             p_mac = &this->m_mqtt_publish_adv_arg_adv.tag_mac;
            std::copy(std::begin(p_mac->mac), std::end(p_mac->mac), tag_mac.begin());
            std::array<uint8_t, MAC_ADDRESS_NUM_BYTES> tag_mac_expected;
            const mac_address_bin_t* const             p_mac_expected = &p_adv->tag_mac;
            std::copy(std::begin(p_mac_expected->mac), std::end(p_mac_expected->mac), tag_mac_expected.begin());
            ASSERT_EQ(tag_mac_expected, tag_mac);
        }
        {
            const size_t            len = p_adv->data_len;
            std::array<uint8_t, 24> data_buf;
            std::copy(
                &this->m_mqtt_publish_adv_arg_adv.data_buf[0],
                &this->m_mqtt_publish_adv_arg_adv.data_buf[len],
                data_buf.begin());
            std::array<uint8_t, 24> data_buf_expected;
            std::copy(&p_adv->data_buf[0], &p_adv->data_buf[len], data_buf_expected.begin());
            ASSERT_EQ(data_buf_expected, data_buf);
        }
    }
    ASSERT_TRUE(this->m_mem_alloc_trace.is_empty());
}

TEST_F(TestAdvPostAsyncComm, test_time_not_synchronized) // NOLINT
{
    this->m_flag_time_is_synchronized              = false;
    this->m_http_server_mutex_try_lock_res         = true;
    this->m_http_post_advs_res                     = true;
    this->m_gw_cfg_get_http_stat_use_http_stat_res = true;
    this->m_gw_cfg_get_mqtt_use_mqtt_res           = true;
    this->m_gw_cfg_get_ntp_use_res                 = true;
    this->m_adv_post_statistics_do_send_res        = true;
    this->m_gw_status_is_mqtt_connected_res        = true;

    adv_post_state_t adv_post_state = {
        .flag_primary_time_sync_is_done  = false,
        .flag_network_connected          = true,
        .flag_async_comm_in_progress     = false,
        .flag_need_to_send_advs1         = true,
        .flag_need_to_send_advs2         = false,
        .flag_need_to_send_statistics    = false,
        .flag_need_to_send_mqtt_periodic = false,
        .flag_relaying_enabled           = true,
        .flag_use_timestamps             = true,
        .flag_stop                       = false,
    };
    this->m_reports = {
        .num_of_advs = 2,
        .table = {
            [0] = {
                .timestamp = 100500,
                .samples_counter = 301,
                .tag_mac = { 0x11, 0x12, 0x13, 0x14, 0x15, 0x16,},
                .rssi = -49,
                .data_len = 24,
                .data_buf = {
                    0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27, 0x28, 0x29, 0x2A, 0x2B, 0x2C, 0x2D, 0x2E, 0x2F,
                    0x30, 0x31, 0x32, 0x33, 0x34, 0x35,
                }
            },
            [1] = {
                .timestamp = 100501,
                .samples_counter = 302,
                .tag_mac = { 0x21, 0x22, 0x23, 0x24, 0x25, 0x26,},
                .rssi = -48,
                .data_len = 24,
                .data_buf = {
                    0x51, 0x52, 0x53, 0x54, 0x55, 0x56, 0x57, 0x58, 0x59, 0x5A, 0x5B, 0x5C, 0x5D, 0x5E, 0x5F,
                    0x50, 0x51, 0x52, 0x53, 0x54, 0x55,
                }
            },
        }
    };

    adv_post_do_async_comm(&adv_post_state);
    ASSERT_FALSE(this->m_flag_gateway_restart_low_memory);
    ASSERT_FALSE(this->m_http_server_mutex_locked);
    ASSERT_FALSE(this->m_leds_notify_http1_data_sent_fail);
    ASSERT_FALSE(this->m_leds_notify_http2_data_sent_fail);
    ASSERT_EQ(0, this->m_http_post_advs_call_cnt);
    ASSERT_EQ(0, this->m_adv_post_statistics_do_send_call_cnt);
    ASSERT_EQ(0, this->m_mqtt_publish_adv_call_cnt);
    ASSERT_TRUE(this->m_adv_post_timers_start_timer_sig_do_async_comm);

    adv_post_state.flag_need_to_send_advs1 = false;
    adv_post_state.flag_need_to_send_advs2 = true;

    adv_post_do_async_comm(&adv_post_state);
    ASSERT_FALSE(this->m_flag_gateway_restart_low_memory);
    ASSERT_FALSE(this->m_http_server_mutex_locked);
    ASSERT_FALSE(this->m_leds_notify_http1_data_sent_fail);
    ASSERT_FALSE(this->m_leds_notify_http2_data_sent_fail);
    ASSERT_EQ(0, this->m_http_post_advs_call_cnt);
    ASSERT_EQ(0, this->m_adv_post_statistics_do_send_call_cnt);
    ASSERT_EQ(0, this->m_mqtt_publish_adv_call_cnt);
    ASSERT_TRUE(this->m_adv_post_timers_start_timer_sig_do_async_comm);

    adv_post_state.flag_need_to_send_advs2      = false;
    adv_post_state.flag_need_to_send_statistics = true;

    adv_post_do_async_comm(&adv_post_state);
    ASSERT_FALSE(this->m_flag_gateway_restart_low_memory);
    ASSERT_FALSE(this->m_http_server_mutex_locked);
    ASSERT_FALSE(this->m_leds_notify_http1_data_sent_fail);
    ASSERT_FALSE(this->m_leds_notify_http2_data_sent_fail);
    ASSERT_EQ(0, this->m_http_post_advs_call_cnt);
    ASSERT_EQ(0, this->m_adv_post_statistics_do_send_call_cnt);
    ASSERT_EQ(0, this->m_mqtt_publish_adv_call_cnt);
    ASSERT_TRUE(this->m_adv_post_timers_start_timer_sig_do_async_comm);

    adv_post_state.flag_need_to_send_statistics    = false;
    adv_post_state.flag_need_to_send_mqtt_periodic = true;

    adv_post_do_async_comm(&adv_post_state);
    ASSERT_FALSE(this->m_flag_gateway_restart_low_memory);
    ASSERT_FALSE(this->m_http_server_mutex_locked);
    ASSERT_FALSE(this->m_leds_notify_http1_data_sent_fail);
    ASSERT_FALSE(this->m_leds_notify_http2_data_sent_fail);
    ASSERT_EQ(0, this->m_http_post_advs_call_cnt);
    ASSERT_EQ(0, this->m_adv_post_statistics_do_send_call_cnt);
    ASSERT_EQ(0, this->m_mqtt_publish_adv_call_cnt);
    ASSERT_TRUE(this->m_adv_post_timers_start_timer_sig_do_async_comm);

    ASSERT_TRUE(this->m_mem_alloc_trace.is_empty());
}

TEST_F(TestAdvPostAsyncComm, test_no_network_connection) // NOLINT
{
    this->m_flag_time_is_synchronized              = true;
    this->m_http_server_mutex_try_lock_res         = true;
    this->m_http_post_advs_res                     = true;
    this->m_gw_cfg_get_http_stat_use_http_stat_res = true;
    this->m_gw_cfg_get_mqtt_use_mqtt_res           = true;
    this->m_gw_cfg_get_ntp_use_res                 = true;
    this->m_adv_post_statistics_do_send_res        = true;
    this->m_gw_status_is_mqtt_connected_res        = true;

    adv_post_state_t adv_post_state = {
        .flag_primary_time_sync_is_done  = false,
        .flag_network_connected          = false,
        .flag_async_comm_in_progress     = false,
        .flag_need_to_send_advs1         = true,
        .flag_need_to_send_advs2         = false,
        .flag_need_to_send_statistics    = false,
        .flag_need_to_send_mqtt_periodic = false,
        .flag_relaying_enabled           = true,
        .flag_use_timestamps             = true,
        .flag_stop                       = false,
    };
    this->m_reports = {
        .num_of_advs = 2,
        .table = {
            [0] = {
                .timestamp = 100500,
                .samples_counter = 301,
                .tag_mac = { 0x11, 0x12, 0x13, 0x14, 0x15, 0x16,},
                .rssi = -49,
                .data_len = 24,
                .data_buf = {
                    0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27, 0x28, 0x29, 0x2A, 0x2B, 0x2C, 0x2D, 0x2E, 0x2F,
                    0x30, 0x31, 0x32, 0x33, 0x34, 0x35,
                }
            },
            [1] = {
                .timestamp = 100501,
                .samples_counter = 302,
                .tag_mac = { 0x21, 0x22, 0x23, 0x24, 0x25, 0x26,},
                .rssi = -48,
                .data_len = 24,
                .data_buf = {
                    0x51, 0x52, 0x53, 0x54, 0x55, 0x56, 0x57, 0x58, 0x59, 0x5A, 0x5B, 0x5C, 0x5D, 0x5E, 0x5F,
                    0x50, 0x51, 0x52, 0x53, 0x54, 0x55,
                }
            },
        }
    };

    adv_post_do_async_comm(&adv_post_state);
    ASSERT_FALSE(this->m_flag_gateway_restart_low_memory);
    ASSERT_FALSE(this->m_http_server_mutex_locked);
    ASSERT_FALSE(this->m_leds_notify_http1_data_sent_fail);
    ASSERT_FALSE(this->m_leds_notify_http2_data_sent_fail);
    ASSERT_EQ(0, this->m_http_post_advs_call_cnt);
    ASSERT_EQ(0, this->m_adv_post_statistics_do_send_call_cnt);
    ASSERT_EQ(0, this->m_mqtt_publish_adv_call_cnt);
    ASSERT_TRUE(this->m_adv_post_timers_start_timer_sig_do_async_comm);

    adv_post_state.flag_need_to_send_advs1 = false;
    adv_post_state.flag_need_to_send_advs2 = true;

    adv_post_do_async_comm(&adv_post_state);
    ASSERT_FALSE(this->m_flag_gateway_restart_low_memory);
    ASSERT_FALSE(this->m_http_server_mutex_locked);
    ASSERT_FALSE(this->m_leds_notify_http1_data_sent_fail);
    ASSERT_FALSE(this->m_leds_notify_http2_data_sent_fail);
    ASSERT_EQ(0, this->m_http_post_advs_call_cnt);
    ASSERT_EQ(0, this->m_adv_post_statistics_do_send_call_cnt);
    ASSERT_EQ(0, this->m_mqtt_publish_adv_call_cnt);
    ASSERT_TRUE(this->m_adv_post_timers_start_timer_sig_do_async_comm);

    adv_post_state.flag_need_to_send_advs2      = false;
    adv_post_state.flag_need_to_send_statistics = true;

    adv_post_do_async_comm(&adv_post_state);
    ASSERT_FALSE(this->m_flag_gateway_restart_low_memory);
    ASSERT_FALSE(this->m_http_server_mutex_locked);
    ASSERT_FALSE(this->m_leds_notify_http1_data_sent_fail);
    ASSERT_FALSE(this->m_leds_notify_http2_data_sent_fail);
    ASSERT_EQ(0, this->m_http_post_advs_call_cnt);
    ASSERT_EQ(0, this->m_adv_post_statistics_do_send_call_cnt);
    ASSERT_EQ(0, this->m_mqtt_publish_adv_call_cnt);
    ASSERT_TRUE(this->m_adv_post_timers_start_timer_sig_do_async_comm);

    adv_post_state.flag_need_to_send_statistics    = false;
    adv_post_state.flag_need_to_send_mqtt_periodic = true;

    adv_post_do_async_comm(&adv_post_state);
    ASSERT_FALSE(this->m_flag_gateway_restart_low_memory);
    ASSERT_FALSE(this->m_http_server_mutex_locked);
    ASSERT_FALSE(this->m_leds_notify_http1_data_sent_fail);
    ASSERT_FALSE(this->m_leds_notify_http2_data_sent_fail);
    ASSERT_EQ(0, this->m_http_post_advs_call_cnt);
    ASSERT_EQ(0, this->m_adv_post_statistics_do_send_call_cnt);
    ASSERT_EQ(0, this->m_mqtt_publish_adv_call_cnt);
    ASSERT_TRUE(this->m_adv_post_timers_start_timer_sig_do_async_comm);

    ASSERT_TRUE(this->m_mem_alloc_trace.is_empty());
}

TEST_F(TestAdvPostAsyncComm, test_malloc_failed_1) // NOLINT
{
    this->m_flag_time_is_synchronized              = true;
    this->m_http_server_mutex_try_lock_res         = true;
    this->m_http_post_advs_res                     = true;
    this->m_gw_cfg_get_http_stat_use_http_stat_res = true;
    this->m_gw_cfg_get_mqtt_use_mqtt_res           = true;
    this->m_gw_cfg_get_ntp_use_res                 = true;
    this->m_adv_post_statistics_do_send_res        = true;
    this->m_gw_status_is_mqtt_connected_res        = true;

    adv_post_state_t adv_post_state = {
        .flag_primary_time_sync_is_done  = false,
        .flag_network_connected          = true,
        .flag_async_comm_in_progress     = false,
        .flag_need_to_send_advs1         = true,
        .flag_need_to_send_advs2         = false,
        .flag_need_to_send_statistics    = false,
        .flag_need_to_send_mqtt_periodic = false,
        .flag_relaying_enabled           = true,
        .flag_use_timestamps             = true,
        .flag_stop                       = false,
    };
    this->m_reports = {
        .num_of_advs = 2,
        .table = {
            [0] = {
                .timestamp = 100500,
                .samples_counter = 301,
                .tag_mac = { 0x11, 0x12, 0x13, 0x14, 0x15, 0x16,},
                .rssi = -49,
                .data_len = 24,
                .data_buf = {
                    0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27, 0x28, 0x29, 0x2A, 0x2B, 0x2C, 0x2D, 0x2E, 0x2F,
                    0x30, 0x31, 0x32, 0x33, 0x34, 0x35,
                }
            },
            [1] = {
                .timestamp = 100501,
                .samples_counter = 302,
                .tag_mac = { 0x21, 0x22, 0x23, 0x24, 0x25, 0x26,},
                .rssi = -48,
                .data_len = 24,
                .data_buf = {
                    0x51, 0x52, 0x53, 0x54, 0x55, 0x56, 0x57, 0x58, 0x59, 0x5A, 0x5B, 0x5C, 0x5D, 0x5E, 0x5F,
                    0x50, 0x51, 0x52, 0x53, 0x54, 0x55,
                }
            },
        }
    };

    this->m_malloc_fail_on_cnt = 1;

    adv_post_do_async_comm(&adv_post_state);
    ASSERT_TRUE(this->m_flag_gateway_restart_low_memory);
    ASSERT_FALSE(this->m_http_server_mutex_locked);
    ASSERT_TRUE(this->m_leds_notify_http1_data_sent_fail);
    ASSERT_FALSE(this->m_leds_notify_http2_data_sent_fail);
    ASSERT_EQ(0, this->m_http_post_advs_call_cnt);
    ASSERT_FALSE(this->m_http_server_mutex_locked);
    ASSERT_EQ(0, this->m_adv_post_statistics_do_send_call_cnt);
    ASSERT_EQ(0, this->m_mqtt_publish_adv_call_cnt);
    ASSERT_TRUE(this->m_adv_post_timers_start_timer_sig_do_async_comm);
    this->m_adv_post_timers_start_timer_sig_do_async_comm = false;

    ASSERT_TRUE(this->m_mem_alloc_trace.is_empty());
}

TEST_F(TestAdvPostAsyncComm, test_malloc_failed_2) // NOLINT
{
    this->m_flag_time_is_synchronized              = true;
    this->m_http_server_mutex_try_lock_res         = true;
    this->m_http_post_advs_res                     = true;
    this->m_gw_cfg_get_http_stat_use_http_stat_res = true;
    this->m_gw_cfg_get_mqtt_use_mqtt_res           = true;
    this->m_gw_cfg_get_ntp_use_res                 = true;
    this->m_adv_post_statistics_do_send_res        = true;
    this->m_gw_status_is_mqtt_connected_res        = true;

    adv_post_state_t adv_post_state = {
        .flag_primary_time_sync_is_done  = false,
        .flag_network_connected          = true,
        .flag_async_comm_in_progress     = false,
        .flag_need_to_send_advs1         = true,
        .flag_need_to_send_advs2         = false,
        .flag_need_to_send_statistics    = false,
        .flag_need_to_send_mqtt_periodic = false,
        .flag_relaying_enabled           = true,
        .flag_use_timestamps             = true,
        .flag_stop                       = false,
    };
    this->m_reports = {
        .num_of_advs = 2,
        .table = {
            [0] = {
                .timestamp = 100500,
                .samples_counter = 301,
                .tag_mac = { 0x11, 0x12, 0x13, 0x14, 0x15, 0x16,},
                .rssi = -49,
                .data_len = 24,
                .data_buf = {
                    0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27, 0x28, 0x29, 0x2A, 0x2B, 0x2C, 0x2D, 0x2E, 0x2F,
                    0x30, 0x31, 0x32, 0x33, 0x34, 0x35,
                }
            },
            [1] = {
                .timestamp = 100501,
                .samples_counter = 302,
                .tag_mac = { 0x21, 0x22, 0x23, 0x24, 0x25, 0x26,},
                .rssi = -48,
                .data_len = 24,
                .data_buf = {
                    0x51, 0x52, 0x53, 0x54, 0x55, 0x56, 0x57, 0x58, 0x59, 0x5A, 0x5B, 0x5C, 0x5D, 0x5E, 0x5F,
                    0x50, 0x51, 0x52, 0x53, 0x54, 0x55,
                }
            },
        }
    };

    this->m_malloc_fail_on_cnt = 2;

    adv_post_do_async_comm(&adv_post_state);
    ASSERT_FALSE(this->m_flag_gateway_restart_low_memory);
    ASSERT_FALSE(this->m_http_server_mutex_locked);
    ASSERT_TRUE(this->m_leds_notify_http1_data_sent_fail);
    ASSERT_FALSE(this->m_leds_notify_http2_data_sent_fail);
    ASSERT_EQ(0, this->m_http_post_advs_call_cnt);
    ASSERT_EQ(0, this->m_adv_post_statistics_do_send_call_cnt);
    ASSERT_EQ(0, this->m_mqtt_publish_adv_call_cnt);
    ASSERT_TRUE(this->m_adv_post_timers_start_timer_sig_do_async_comm);
    this->m_adv_post_timers_start_timer_sig_do_async_comm = false;

    ASSERT_TRUE(this->m_mem_alloc_trace.is_empty());
}

TEST_F(TestAdvPostAsyncComm, test_malloc_failed_3) // NOLINT
{
    this->m_flag_time_is_synchronized              = true;
    this->m_http_server_mutex_try_lock_res         = true;
    this->m_http_post_advs_res                     = true;
    this->m_gw_cfg_get_http_stat_use_http_stat_res = true;
    this->m_gw_cfg_get_mqtt_use_mqtt_res           = true;
    this->m_gw_cfg_get_ntp_use_res                 = true;
    this->m_adv_post_statistics_do_send_res        = true;
    this->m_gw_status_is_mqtt_connected_res        = true;

    adv_post_state_t adv_post_state = {
        .flag_primary_time_sync_is_done  = false,
        .flag_network_connected          = true,
        .flag_async_comm_in_progress     = false,
        .flag_need_to_send_advs1         = true,
        .flag_need_to_send_advs2         = false,
        .flag_need_to_send_statistics    = false,
        .flag_need_to_send_mqtt_periodic = false,
        .flag_relaying_enabled           = true,
        .flag_use_timestamps             = true,
        .flag_stop                       = false,
    };
    this->m_reports = {
        .num_of_advs = 2,
        .table = {
            [0] = {
                .timestamp = 100500,
                .samples_counter = 301,
                .tag_mac = { 0x11, 0x12, 0x13, 0x14, 0x15, 0x16,},
                .rssi = -49,
                .data_len = 24,
                .data_buf = {
                    0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27, 0x28, 0x29, 0x2A, 0x2B, 0x2C, 0x2D, 0x2E, 0x2F,
                    0x30, 0x31, 0x32, 0x33, 0x34, 0x35,
                }
            },
            [1] = {
                .timestamp = 100501,
                .samples_counter = 302,
                .tag_mac = { 0x21, 0x22, 0x23, 0x24, 0x25, 0x26,},
                .rssi = -48,
                .data_len = 24,
                .data_buf = {
                    0x51, 0x52, 0x53, 0x54, 0x55, 0x56, 0x57, 0x58, 0x59, 0x5A, 0x5B, 0x5C, 0x5D, 0x5E, 0x5F,
                    0x50, 0x51, 0x52, 0x53, 0x54, 0x55,
                }
            },
        }
    };

    this->m_malloc_fail_on_cnt = 3;

    adv_post_do_async_comm(&adv_post_state);
    ASSERT_FALSE(this->m_flag_gateway_restart_low_memory);
    ASSERT_TRUE(this->m_http_server_mutex_locked);
    ASSERT_FALSE(this->m_leds_notify_http1_data_sent_fail);
    ASSERT_FALSE(this->m_leds_notify_http2_data_sent_fail);
    ASSERT_EQ(1, this->m_http_post_advs_call_cnt);
    ASSERT_EQ(0, this->m_adv_post_statistics_do_send_call_cnt);
    ASSERT_EQ(0, this->m_mqtt_publish_adv_call_cnt);
    ASSERT_TRUE(this->m_adv_post_timers_start_timer_sig_do_async_comm);
    this->m_adv_post_timers_start_timer_sig_do_async_comm = false;

    this->m_hmac_sha256_set_key_for_http_ruuvi_res = true;
    ASSERT_TRUE(adv_post_set_hmac_sha256_key("key_str123"));
    ASSERT_EQ(string("key_str123"), this->m_hmac_sha256_set_key_for_http_ruuvi_key);
    this->m_hmac_sha256_set_key_for_http_ruuvi_res = false;
    adv_post_set_default_period(15 * 1000);
    ASSERT_EQ(15 * 1000, this->m_default_period_for_http_ruuvi);

    this->m_http_async_poll_res = true;
    adv_post_do_async_comm(&adv_post_state);
    ASSERT_FALSE(this->m_flag_gateway_restart_low_memory);
    ASSERT_FALSE(this->m_http_server_mutex_locked);
    ASSERT_FALSE(this->m_leds_notify_http1_data_sent_fail);
    ASSERT_FALSE(this->m_leds_notify_http2_data_sent_fail);
    ASSERT_EQ(1, this->m_http_post_advs_call_cnt);
    ASSERT_EQ(0, this->m_adv_post_statistics_do_send_call_cnt);
    ASSERT_EQ(0, this->m_mqtt_publish_adv_call_cnt);
    this->m_http_async_poll_res = false;

    adv_post_state.flag_need_to_send_advs1          = false;
    adv_post_state.flag_need_to_send_advs2          = true;
    this->m_hmac_sha256_set_key_for_http_custom_res = true;
    adv_post_do_async_comm(&adv_post_state);
    ASSERT_TRUE(this->m_flag_gateway_restart_low_memory);
    ASSERT_FALSE(this->m_http_server_mutex_locked);
    ASSERT_FALSE(this->m_leds_notify_http1_data_sent_fail);
    ASSERT_TRUE(this->m_leds_notify_http2_data_sent_fail);
    ASSERT_EQ(1, this->m_http_post_advs_call_cnt);
    ASSERT_EQ(0, this->m_adv_post_statistics_do_send_call_cnt);
    ASSERT_EQ(0, this->m_mqtt_publish_adv_call_cnt);

    ASSERT_TRUE(this->m_mem_alloc_trace.is_empty());
}

TEST_F(TestAdvPostAsyncComm, test_malloc_failed_4) // NOLINT
{
    this->m_flag_time_is_synchronized              = true;
    this->m_http_server_mutex_try_lock_res         = true;
    this->m_http_post_advs_res                     = true;
    this->m_gw_cfg_get_http_stat_use_http_stat_res = true;
    this->m_gw_cfg_get_mqtt_use_mqtt_res           = true;
    this->m_gw_cfg_get_ntp_use_res                 = true;
    this->m_adv_post_statistics_do_send_res        = true;
    this->m_gw_status_is_mqtt_connected_res        = true;

    adv_post_state_t adv_post_state = {
        .flag_primary_time_sync_is_done  = false,
        .flag_network_connected          = true,
        .flag_async_comm_in_progress     = false,
        .flag_need_to_send_advs1         = true,
        .flag_need_to_send_advs2         = false,
        .flag_need_to_send_statistics    = false,
        .flag_need_to_send_mqtt_periodic = false,
        .flag_relaying_enabled           = true,
        .flag_use_timestamps             = true,
        .flag_stop                       = false,
    };
    this->m_reports = {
        .num_of_advs = 2,
        .table = {
            [0] = {
                      .timestamp = 100500,
                      .samples_counter = 301,
                      .tag_mac = { 0x11, 0x12, 0x13, 0x14, 0x15, 0x16,},
                      .rssi = -49,
                      .data_len = 24,
                      .data_buf = {
                    0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27, 0x28, 0x29, 0x2A, 0x2B, 0x2C, 0x2D, 0x2E, 0x2F,
                    0x30, 0x31, 0x32, 0x33, 0x34, 0x35,
                }
            },
            [1] = {
                      .timestamp = 100501,
                      .samples_counter = 302,
                      .tag_mac = { 0x21, 0x22, 0x23, 0x24, 0x25, 0x26,},
                      .rssi = -48,
                      .data_len = 24,
                      .data_buf = {
                    0x51, 0x52, 0x53, 0x54, 0x55, 0x56, 0x57, 0x58, 0x59, 0x5A, 0x5B, 0x5C, 0x5D, 0x5E, 0x5F,
                    0x50, 0x51, 0x52, 0x53, 0x54, 0x55,
                }
            },
        }
    };

    this->m_malloc_fail_on_cnt = 4;

    adv_post_do_async_comm(&adv_post_state);
    ASSERT_FALSE(this->m_flag_gateway_restart_low_memory);
    ASSERT_TRUE(this->m_http_server_mutex_locked);
    ASSERT_FALSE(this->m_leds_notify_http1_data_sent_fail);
    ASSERT_FALSE(this->m_leds_notify_http2_data_sent_fail);
    ASSERT_EQ(1, this->m_http_post_advs_call_cnt);
    ASSERT_EQ(0, this->m_adv_post_statistics_do_send_call_cnt);
    ASSERT_EQ(0, this->m_mqtt_publish_adv_call_cnt);
    ASSERT_TRUE(this->m_adv_post_timers_start_timer_sig_do_async_comm);
    this->m_adv_post_timers_start_timer_sig_do_async_comm = false;

    this->m_hmac_sha256_set_key_for_http_ruuvi_res = true;
    ASSERT_TRUE(adv_post_set_hmac_sha256_key("key_str123"));
    ASSERT_EQ(string("key_str123"), this->m_hmac_sha256_set_key_for_http_ruuvi_key);
    this->m_hmac_sha256_set_key_for_http_ruuvi_res = false;
    adv_post_set_default_period(15 * 1000);
    ASSERT_EQ(15 * 1000, this->m_default_period_for_http_ruuvi);

    this->m_http_async_poll_res = true;
    adv_post_do_async_comm(&adv_post_state);
    ASSERT_FALSE(this->m_flag_gateway_restart_low_memory);
    ASSERT_FALSE(this->m_http_server_mutex_locked);
    ASSERT_FALSE(this->m_leds_notify_http1_data_sent_fail);
    ASSERT_FALSE(this->m_leds_notify_http2_data_sent_fail);
    ASSERT_EQ(1, this->m_http_post_advs_call_cnt);
    ASSERT_EQ(0, this->m_adv_post_statistics_do_send_call_cnt);
    ASSERT_EQ(0, this->m_mqtt_publish_adv_call_cnt);
    this->m_http_async_poll_res = false;

    adv_post_state.flag_need_to_send_advs1          = false;
    adv_post_state.flag_need_to_send_advs2          = true;
    this->m_hmac_sha256_set_key_for_http_custom_res = true;
    adv_post_do_async_comm(&adv_post_state);
    ASSERT_FALSE(this->m_flag_gateway_restart_low_memory);
    ASSERT_FALSE(this->m_http_server_mutex_locked);
    ASSERT_FALSE(this->m_leds_notify_http1_data_sent_fail);
    ASSERT_TRUE(this->m_leds_notify_http2_data_sent_fail);
    ASSERT_EQ(1, this->m_http_post_advs_call_cnt);
    ASSERT_EQ(0, this->m_adv_post_statistics_do_send_call_cnt);
    ASSERT_EQ(0, this->m_mqtt_publish_adv_call_cnt);

    ASSERT_TRUE(this->m_mem_alloc_trace.is_empty());
}

TEST_F(TestAdvPostAsyncComm, test_malloc_failed_5) // NOLINT
{
    this->m_flag_time_is_synchronized              = true;
    this->m_http_server_mutex_try_lock_res         = true;
    this->m_http_post_advs_res                     = true;
    this->m_gw_cfg_get_http_stat_use_http_stat_res = true;
    this->m_gw_cfg_get_mqtt_use_mqtt_res           = true;
    this->m_gw_cfg_get_ntp_use_res                 = true;
    this->m_adv_post_statistics_do_send_res        = true;
    this->m_gw_status_is_mqtt_connected_res        = true;

    adv_post_state_t adv_post_state = {
        .flag_primary_time_sync_is_done  = false,
        .flag_network_connected          = true,
        .flag_async_comm_in_progress     = false,
        .flag_need_to_send_advs1         = true,
        .flag_need_to_send_advs2         = false,
        .flag_need_to_send_statistics    = false,
        .flag_need_to_send_mqtt_periodic = false,
        .flag_relaying_enabled           = true,
        .flag_use_timestamps             = true,
        .flag_stop                       = false,
    };
    this->m_reports = {
        .num_of_advs = 2,
        .table = {
            [0] = {
                .timestamp = 100500,
                .samples_counter = 301,
                .tag_mac = { 0x11, 0x12, 0x13, 0x14, 0x15, 0x16,},
                .rssi = -49,
                .data_len = 24,
                .data_buf = {
                    0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27, 0x28, 0x29, 0x2A, 0x2B, 0x2C, 0x2D, 0x2E, 0x2F,
                    0x30, 0x31, 0x32, 0x33, 0x34, 0x35,
                }
            },
            [1] = {
                .timestamp = 100501,
                .samples_counter = 302,
                .tag_mac = { 0x21, 0x22, 0x23, 0x24, 0x25, 0x26,},
                .rssi = -48,
                .data_len = 24,
                .data_buf = {
                    0x51, 0x52, 0x53, 0x54, 0x55, 0x56, 0x57, 0x58, 0x59, 0x5A, 0x5B, 0x5C, 0x5D, 0x5E, 0x5F,
                    0x50, 0x51, 0x52, 0x53, 0x54, 0x55,
                }
            },
        }
    };

    this->m_malloc_fail_on_cnt = 5;

    adv_post_do_async_comm(&adv_post_state);
    ASSERT_FALSE(this->m_flag_gateway_restart_low_memory);
    ASSERT_TRUE(this->m_http_server_mutex_locked);
    ASSERT_FALSE(this->m_leds_notify_http1_data_sent_fail);
    ASSERT_FALSE(this->m_leds_notify_http2_data_sent_fail);
    ASSERT_EQ(1, this->m_http_post_advs_call_cnt);
    ASSERT_EQ(0, this->m_adv_post_statistics_do_send_call_cnt);
    ASSERT_EQ(0, this->m_mqtt_publish_adv_call_cnt);
    ASSERT_TRUE(this->m_adv_post_timers_start_timer_sig_do_async_comm);
    this->m_adv_post_timers_start_timer_sig_do_async_comm = false;

    this->m_hmac_sha256_set_key_for_http_ruuvi_res = true;
    ASSERT_TRUE(adv_post_set_hmac_sha256_key("key_str123"));
    ASSERT_EQ(string("key_str123"), this->m_hmac_sha256_set_key_for_http_ruuvi_key);
    this->m_hmac_sha256_set_key_for_http_ruuvi_res = false;
    adv_post_set_default_period(15 * 1000);
    ASSERT_EQ(15 * 1000, this->m_default_period_for_http_ruuvi);

    this->m_http_async_poll_res = true;
    adv_post_do_async_comm(&adv_post_state);
    ASSERT_FALSE(this->m_flag_gateway_restart_low_memory);
    ASSERT_FALSE(this->m_http_server_mutex_locked);
    ASSERT_FALSE(this->m_leds_notify_http1_data_sent_fail);
    ASSERT_FALSE(this->m_leds_notify_http2_data_sent_fail);
    ASSERT_EQ(1, this->m_http_post_advs_call_cnt);
    ASSERT_EQ(0, this->m_adv_post_statistics_do_send_call_cnt);
    ASSERT_EQ(0, this->m_mqtt_publish_adv_call_cnt);
    this->m_http_async_poll_res = false;

    adv_post_state.flag_need_to_send_advs1          = false;
    adv_post_state.flag_need_to_send_advs2          = true;
    this->m_hmac_sha256_set_key_for_http_custom_res = true;
    adv_post_do_async_comm(&adv_post_state);
    ASSERT_FALSE(this->m_flag_gateway_restart_low_memory);
    ASSERT_TRUE(this->m_http_server_mutex_locked);
    ASSERT_FALSE(this->m_leds_notify_http1_data_sent_fail);
    ASSERT_FALSE(this->m_leds_notify_http2_data_sent_fail);
    ASSERT_EQ(2, this->m_http_post_advs_call_cnt);
    ASSERT_EQ(0, this->m_adv_post_statistics_do_send_call_cnt);
    ASSERT_EQ(0, this->m_mqtt_publish_adv_call_cnt);
    ASSERT_TRUE(adv_post_set_hmac_sha256_key("key_str124"));
    ASSERT_EQ(string("key_str124"), this->m_hmac_sha256_set_key_for_http_custom_key);
    this->m_hmac_sha256_set_key_for_http_custom_res = false;
    adv_post_set_default_period(25 * 1000);
    ASSERT_EQ(25 * 1000, this->m_default_period_for_http_custom);

    this->m_http_async_poll_res = true;
    adv_post_do_async_comm(&adv_post_state);
    ASSERT_FALSE(this->m_flag_gateway_restart_low_memory);
    ASSERT_FALSE(this->m_http_server_mutex_locked);
    ASSERT_FALSE(this->m_leds_notify_http1_data_sent_fail);
    ASSERT_FALSE(this->m_leds_notify_http2_data_sent_fail);
    ASSERT_EQ(2, this->m_http_post_advs_call_cnt);
    ASSERT_EQ(0, this->m_adv_post_statistics_do_send_call_cnt);
    ASSERT_EQ(0, this->m_mqtt_publish_adv_call_cnt);
    this->m_http_async_poll_res = false;

    adv_post_state.flag_need_to_send_advs2      = false;
    adv_post_state.flag_need_to_send_statistics = true;
    this->m_hmac_sha256_set_key_for_stats_res   = true;
    adv_post_do_async_comm(&adv_post_state);
    ASSERT_FALSE(this->m_flag_gateway_restart_low_memory);
    ASSERT_TRUE(this->m_http_server_mutex_locked);
    ASSERT_FALSE(this->m_leds_notify_http1_data_sent_fail);
    ASSERT_FALSE(this->m_leds_notify_http2_data_sent_fail);
    ASSERT_EQ(2, this->m_http_post_advs_call_cnt);
    ASSERT_EQ(1, this->m_adv_post_statistics_do_send_call_cnt);
    ASSERT_EQ(0, this->m_mqtt_publish_adv_call_cnt);
    ASSERT_TRUE(adv_post_set_hmac_sha256_key("key_str125"));
    ASSERT_EQ(string("key_str125"), this->m_hmac_sha256_set_key_for_stats_key);
    this->m_hmac_sha256_set_key_for_stats_res = false;
    adv_post_set_default_period(35 * 1000);
    ASSERT_EQ(15 * 1000, this->m_default_period_for_http_ruuvi);
    ASSERT_EQ(25 * 1000, this->m_default_period_for_http_custom);

    this->m_http_async_poll_res = true;
    adv_post_do_async_comm(&adv_post_state);
    ASSERT_FALSE(this->m_flag_gateway_restart_low_memory);
    ASSERT_FALSE(this->m_http_server_mutex_locked);
    ASSERT_FALSE(this->m_leds_notify_http1_data_sent_fail);
    ASSERT_FALSE(this->m_leds_notify_http2_data_sent_fail);
    ASSERT_EQ(2, this->m_http_post_advs_call_cnt);
    ASSERT_EQ(1, this->m_adv_post_statistics_do_send_call_cnt);
    ASSERT_EQ(0, this->m_mqtt_publish_adv_call_cnt);
    this->m_http_async_poll_res = false;

    adv_post_state.flag_need_to_send_statistics    = false;
    adv_post_state.flag_need_to_send_mqtt_periodic = true;
    this->m_mqtt_publish_adv_res                   = true;
    ASSERT_EQ(0, this->m_send_sig_restart_services);
    adv_post_do_async_comm(&adv_post_state);
    ASSERT_TRUE(this->m_flag_gateway_restart_low_memory);
    ASSERT_EQ(1, this->m_send_sig_restart_services);
    ASSERT_FALSE(this->m_http_server_mutex_locked);
    ASSERT_FALSE(this->m_leds_notify_http1_data_sent_fail);
    ASSERT_FALSE(this->m_leds_notify_http2_data_sent_fail);
    ASSERT_EQ(2, this->m_http_post_advs_call_cnt);
    ASSERT_EQ(1, this->m_adv_post_statistics_do_send_call_cnt);
    ASSERT_EQ(0, this->m_mqtt_publish_adv_call_cnt);

    ASSERT_EQ(0, this->m_adv_post_signals_send_sig);
    ASSERT_TRUE(this->m_adv_post_timers_start_timer_sig_do_async_comm);
    this->m_adv_post_timers_start_timer_sig_do_async_comm = false;
    ASSERT_EQ(0, this->m_mqtt_publish_adv_arg_timestamp);

    ASSERT_TRUE(this->m_mem_alloc_trace.is_empty());
}

TEST_F(TestAdvPostAsyncComm, test_malloc_failed_6) // NOLINT
{
    this->m_flag_time_is_synchronized              = true;
    this->m_http_server_mutex_try_lock_res         = true;
    this->m_http_post_advs_res                     = true;
    this->m_gw_cfg_get_http_stat_use_http_stat_res = true;
    this->m_gw_cfg_get_mqtt_use_mqtt_res           = true;
    this->m_gw_cfg_get_ntp_use_res                 = true;
    this->m_adv_post_statistics_do_send_res        = true;
    this->m_gw_status_is_mqtt_connected_res        = true;

    adv_post_state_t adv_post_state = {
        .flag_primary_time_sync_is_done  = false,
        .flag_network_connected          = true,
        .flag_async_comm_in_progress     = false,
        .flag_need_to_send_advs1         = true,
        .flag_need_to_send_advs2         = false,
        .flag_need_to_send_statistics    = false,
        .flag_need_to_send_mqtt_periodic = false,
        .flag_relaying_enabled           = true,
        .flag_use_timestamps             = true,
        .flag_stop                       = false,
    };
    this->m_reports = {
        .num_of_advs = 2,
        .table = {
            [0] = {
                .timestamp = 100500,
                .samples_counter = 301,
                .tag_mac = { 0x11, 0x12, 0x13, 0x14, 0x15, 0x16,},
                .rssi = -49,
                .data_len = 24,
                .data_buf = {
                    0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27, 0x28, 0x29, 0x2A, 0x2B, 0x2C, 0x2D, 0x2E, 0x2F,
                    0x30, 0x31, 0x32, 0x33, 0x34, 0x35,
                }
            },
            [1] = {
                .timestamp = 100501,
                .samples_counter = 302,
                .tag_mac = { 0x21, 0x22, 0x23, 0x24, 0x25, 0x26,},
                .rssi = -48,
                .data_len = 24,
                .data_buf = {
                    0x51, 0x52, 0x53, 0x54, 0x55, 0x56, 0x57, 0x58, 0x59, 0x5A, 0x5B, 0x5C, 0x5D, 0x5E, 0x5F,
                    0x50, 0x51, 0x52, 0x53, 0x54, 0x55,
                }
            },
        }
    };

    this->m_malloc_fail_on_cnt = 6;

    adv_post_do_async_comm(&adv_post_state);
    ASSERT_FALSE(this->m_flag_gateway_restart_low_memory);
    ASSERT_TRUE(this->m_http_server_mutex_locked);
    ASSERT_FALSE(this->m_leds_notify_http1_data_sent_fail);
    ASSERT_FALSE(this->m_leds_notify_http2_data_sent_fail);
    ASSERT_EQ(1, this->m_http_post_advs_call_cnt);
    ASSERT_EQ(0, this->m_adv_post_statistics_do_send_call_cnt);
    ASSERT_EQ(0, this->m_mqtt_publish_adv_call_cnt);
    ASSERT_TRUE(this->m_adv_post_timers_start_timer_sig_do_async_comm);
    this->m_adv_post_timers_start_timer_sig_do_async_comm = false;

    this->m_hmac_sha256_set_key_for_http_ruuvi_res = true;
    ASSERT_TRUE(adv_post_set_hmac_sha256_key("key_str123"));
    ASSERT_EQ(string("key_str123"), this->m_hmac_sha256_set_key_for_http_ruuvi_key);
    this->m_hmac_sha256_set_key_for_http_ruuvi_res = false;
    adv_post_set_default_period(15 * 1000);
    ASSERT_EQ(15 * 1000, this->m_default_period_for_http_ruuvi);

    this->m_http_async_poll_res = true;
    adv_post_do_async_comm(&adv_post_state);
    ASSERT_FALSE(this->m_flag_gateway_restart_low_memory);
    ASSERT_FALSE(this->m_http_server_mutex_locked);
    ASSERT_FALSE(this->m_leds_notify_http1_data_sent_fail);
    ASSERT_FALSE(this->m_leds_notify_http2_data_sent_fail);
    ASSERT_EQ(1, this->m_http_post_advs_call_cnt);
    ASSERT_EQ(0, this->m_adv_post_statistics_do_send_call_cnt);
    ASSERT_EQ(0, this->m_mqtt_publish_adv_call_cnt);
    this->m_http_async_poll_res = false;

    adv_post_state.flag_need_to_send_advs1          = false;
    adv_post_state.flag_need_to_send_advs2          = true;
    this->m_hmac_sha256_set_key_for_http_custom_res = true;
    adv_post_do_async_comm(&adv_post_state);
    ASSERT_FALSE(this->m_flag_gateway_restart_low_memory);
    ASSERT_TRUE(this->m_http_server_mutex_locked);
    ASSERT_FALSE(this->m_leds_notify_http1_data_sent_fail);
    ASSERT_FALSE(this->m_leds_notify_http2_data_sent_fail);
    ASSERT_EQ(2, this->m_http_post_advs_call_cnt);
    ASSERT_EQ(0, this->m_adv_post_statistics_do_send_call_cnt);
    ASSERT_EQ(0, this->m_mqtt_publish_adv_call_cnt);
    ASSERT_TRUE(adv_post_set_hmac_sha256_key("key_str124"));
    ASSERT_EQ(string("key_str124"), this->m_hmac_sha256_set_key_for_http_custom_key);
    this->m_hmac_sha256_set_key_for_http_custom_res = false;
    adv_post_set_default_period(25 * 1000);
    ASSERT_EQ(25 * 1000, this->m_default_period_for_http_custom);

    this->m_http_async_poll_res = true;
    adv_post_do_async_comm(&adv_post_state);
    ASSERT_FALSE(this->m_flag_gateway_restart_low_memory);
    ASSERT_FALSE(this->m_http_server_mutex_locked);
    ASSERT_FALSE(this->m_leds_notify_http1_data_sent_fail);
    ASSERT_FALSE(this->m_leds_notify_http2_data_sent_fail);
    ASSERT_EQ(2, this->m_http_post_advs_call_cnt);
    ASSERT_EQ(0, this->m_adv_post_statistics_do_send_call_cnt);
    ASSERT_EQ(0, this->m_mqtt_publish_adv_call_cnt);
    this->m_http_async_poll_res = false;

    adv_post_state.flag_need_to_send_advs2      = false;
    adv_post_state.flag_need_to_send_statistics = true;
    this->m_hmac_sha256_set_key_for_stats_res   = true;
    adv_post_do_async_comm(&adv_post_state);
    ASSERT_FALSE(this->m_flag_gateway_restart_low_memory);
    ASSERT_TRUE(this->m_http_server_mutex_locked);
    ASSERT_FALSE(this->m_leds_notify_http1_data_sent_fail);
    ASSERT_FALSE(this->m_leds_notify_http2_data_sent_fail);
    ASSERT_EQ(2, this->m_http_post_advs_call_cnt);
    ASSERT_EQ(1, this->m_adv_post_statistics_do_send_call_cnt);
    ASSERT_EQ(0, this->m_mqtt_publish_adv_call_cnt);
    ASSERT_TRUE(adv_post_set_hmac_sha256_key("key_str125"));
    ASSERT_EQ(string("key_str125"), this->m_hmac_sha256_set_key_for_stats_key);
    this->m_hmac_sha256_set_key_for_stats_res = false;
    adv_post_set_default_period(35 * 1000);
    ASSERT_EQ(15 * 1000, this->m_default_period_for_http_ruuvi);
    ASSERT_EQ(25 * 1000, this->m_default_period_for_http_custom);

    this->m_http_async_poll_res = true;
    adv_post_do_async_comm(&adv_post_state);
    ASSERT_FALSE(this->m_flag_gateway_restart_low_memory);
    ASSERT_FALSE(this->m_http_server_mutex_locked);
    ASSERT_FALSE(this->m_leds_notify_http1_data_sent_fail);
    ASSERT_FALSE(this->m_leds_notify_http2_data_sent_fail);
    ASSERT_EQ(2, this->m_http_post_advs_call_cnt);
    ASSERT_EQ(1, this->m_adv_post_statistics_do_send_call_cnt);
    ASSERT_EQ(0, this->m_mqtt_publish_adv_call_cnt);
    this->m_http_async_poll_res = false;

    adv_post_state.flag_need_to_send_statistics    = false;
    adv_post_state.flag_need_to_send_mqtt_periodic = true;
    this->m_mqtt_publish_adv_res                   = true;
    adv_post_do_async_comm(&adv_post_state);
    ASSERT_FALSE(this->m_flag_gateway_restart_low_memory);
    ASSERT_FALSE(this->m_http_server_mutex_locked);
    ASSERT_FALSE(this->m_leds_notify_http1_data_sent_fail);
    ASSERT_FALSE(this->m_leds_notify_http2_data_sent_fail);
    ASSERT_EQ(2, this->m_http_post_advs_call_cnt);
    ASSERT_EQ(1, this->m_adv_post_statistics_do_send_call_cnt);
    ASSERT_EQ(0, this->m_mqtt_publish_adv_call_cnt);

    ASSERT_EQ(ADV_POST_SIG_DO_ASYNC_COMM, this->m_adv_post_signals_send_sig);
    ASSERT_TRUE(this->m_adv_post_timers_start_timer_sig_do_async_comm);
    this->m_adv_post_timers_start_timer_sig_do_async_comm = false;
    ASSERT_EQ(0, this->m_mqtt_publish_adv_arg_timestamp);

    this->m_http_async_poll_res = true;
    adv_post_do_async_comm(&adv_post_state);
    ASSERT_FALSE(this->m_flag_gateway_restart_low_memory);
    ASSERT_FALSE(this->m_http_server_mutex_locked);
    ASSERT_FALSE(this->m_leds_notify_http1_data_sent_fail);
    ASSERT_FALSE(this->m_leds_notify_http2_data_sent_fail);
    ASSERT_EQ(2, this->m_http_post_advs_call_cnt);
    ASSERT_EQ(1, this->m_adv_post_statistics_do_send_call_cnt);
    ASSERT_EQ(1, this->m_mqtt_publish_adv_call_cnt);
    ASSERT_TRUE(this->m_adv_post_timers_start_timer_sig_do_async_comm);
    this->m_adv_post_timers_start_timer_sig_do_async_comm = false;
    this->m_http_async_poll_res                           = false;

    ASSERT_TRUE(this->m_mqtt_publish_adv_arg_flag_use_timestamps);
    ASSERT_NE(0, this->m_mqtt_publish_adv_arg_timestamp);
    {
        const adv_report_t* const p_adv = &this->m_reports.table[0];
        ASSERT_EQ(p_adv->timestamp, this->m_mqtt_publish_adv_arg_adv.timestamp);
        ASSERT_EQ(p_adv->samples_counter, this->m_mqtt_publish_adv_arg_adv.samples_counter);
        ASSERT_EQ(p_adv->rssi, this->m_mqtt_publish_adv_arg_adv.rssi);
        ASSERT_EQ(p_adv->data_len, this->m_mqtt_publish_adv_arg_adv.data_len);
        {
            std::array<uint8_t, MAC_ADDRESS_NUM_BYTES> tag_mac;
            const mac_address_bin_t* const             p_mac = &this->m_mqtt_publish_adv_arg_adv.tag_mac;
            std::copy(std::begin(p_mac->mac), std::end(p_mac->mac), tag_mac.begin());
            std::array<uint8_t, MAC_ADDRESS_NUM_BYTES> tag_mac_expected;
            const mac_address_bin_t* const             p_mac_expected = &p_adv->tag_mac;
            std::copy(std::begin(p_mac_expected->mac), std::end(p_mac_expected->mac), tag_mac_expected.begin());
            ASSERT_EQ(tag_mac_expected, tag_mac);
        }
        {
            const size_t            len = p_adv->data_len;
            std::array<uint8_t, 24> data_buf;
            std::copy(
                &this->m_mqtt_publish_adv_arg_adv.data_buf[0],
                &this->m_mqtt_publish_adv_arg_adv.data_buf[len],
                data_buf.begin());
            std::array<uint8_t, 24> data_buf_expected;
            std::copy(&p_adv->data_buf[0], &p_adv->data_buf[len], data_buf_expected.begin());
            ASSERT_EQ(data_buf_expected, data_buf);
        }
    }

    this->m_mqtt_publish_adv_arg_timestamp = 0;
    this->m_http_async_poll_res            = true;
    adv_post_do_async_comm(&adv_post_state);
    ASSERT_FALSE(this->m_flag_gateway_restart_low_memory);
    ASSERT_FALSE(this->m_http_server_mutex_locked);
    ASSERT_FALSE(this->m_leds_notify_http1_data_sent_fail);
    ASSERT_FALSE(this->m_leds_notify_http2_data_sent_fail);
    ASSERT_EQ(2, this->m_http_post_advs_call_cnt);
    ASSERT_EQ(1, this->m_adv_post_statistics_do_send_call_cnt);
    ASSERT_EQ(2, this->m_mqtt_publish_adv_call_cnt);
    ASSERT_FALSE(this->m_adv_post_timers_start_timer_sig_do_async_comm);
    this->m_http_async_poll_res = false;
    ASSERT_TRUE(this->m_mqtt_publish_adv_arg_flag_use_timestamps);
    ASSERT_NE(0, this->m_mqtt_publish_adv_arg_timestamp);

    {
        const adv_report_t* const p_adv = &this->m_reports.table[1];
        ASSERT_EQ(p_adv->timestamp, this->m_mqtt_publish_adv_arg_adv.timestamp);
        ASSERT_EQ(p_adv->samples_counter, this->m_mqtt_publish_adv_arg_adv.samples_counter);
        ASSERT_EQ(p_adv->rssi, this->m_mqtt_publish_adv_arg_adv.rssi);
        ASSERT_EQ(p_adv->data_len, this->m_mqtt_publish_adv_arg_adv.data_len);
        {
            std::array<uint8_t, MAC_ADDRESS_NUM_BYTES> tag_mac;
            const mac_address_bin_t* const             p_mac = &this->m_mqtt_publish_adv_arg_adv.tag_mac;
            std::copy(std::begin(p_mac->mac), std::end(p_mac->mac), tag_mac.begin());
            std::array<uint8_t, MAC_ADDRESS_NUM_BYTES> tag_mac_expected;
            const mac_address_bin_t* const             p_mac_expected = &p_adv->tag_mac;
            std::copy(std::begin(p_mac_expected->mac), std::end(p_mac_expected->mac), tag_mac_expected.begin());
            ASSERT_EQ(tag_mac_expected, tag_mac);
        }
        {
            const size_t            len = p_adv->data_len;
            std::array<uint8_t, 24> data_buf;
            std::copy(
                &this->m_mqtt_publish_adv_arg_adv.data_buf[0],
                &this->m_mqtt_publish_adv_arg_adv.data_buf[len],
                data_buf.begin());
            std::array<uint8_t, 24> data_buf_expected;
            std::copy(&p_adv->data_buf[0], &p_adv->data_buf[len], data_buf_expected.begin());
            ASSERT_EQ(data_buf_expected, data_buf);
        }
    }
    ASSERT_TRUE(this->m_mem_alloc_trace.is_empty());
}

TEST_F(TestAdvPostAsyncComm, test_mqtt_interrupted_periodical_sending) // NOLINT
{
    this->m_flag_time_is_synchronized              = true;
    this->m_http_server_mutex_try_lock_res         = true;
    this->m_http_post_advs_res                     = true;
    this->m_gw_cfg_get_http_stat_use_http_stat_res = true;
    this->m_gw_cfg_get_mqtt_use_mqtt_res           = true;
    this->m_gw_cfg_get_ntp_use_res                 = true;
    this->m_adv_post_statistics_do_send_res        = true;
    this->m_gw_status_is_mqtt_connected_res        = true;

    adv_post_state_t adv_post_state = {
        .flag_primary_time_sync_is_done  = false,
        .flag_network_connected          = true,
        .flag_async_comm_in_progress     = false,
        .flag_need_to_send_advs1         = false,
        .flag_need_to_send_advs2         = false,
        .flag_need_to_send_statistics    = false,
        .flag_need_to_send_mqtt_periodic = true,
        .flag_relaying_enabled           = true,
        .flag_use_timestamps             = true,
        .flag_stop                       = false,
    };
    this->m_reports = {
        .num_of_advs = 2,
        .table = {
            [0] = {
                .timestamp = 100500,
                .samples_counter = 301,
                .tag_mac = { 0x11, 0x12, 0x13, 0x14, 0x15, 0x16,},
                .rssi = -49,
                .data_len = 24,
                .data_buf = {
                    0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27, 0x28, 0x29, 0x2A, 0x2B, 0x2C, 0x2D, 0x2E, 0x2F,
                    0x30, 0x31, 0x32, 0x33, 0x34, 0x35,
                }
            },
            [1] = {
                .timestamp = 100501,
                .samples_counter = 302,
                .tag_mac = { 0x21, 0x22, 0x23, 0x24, 0x25, 0x26,},
                .rssi = -48,
                .data_len = 24,
                .data_buf = {
                    0x51, 0x52, 0x53, 0x54, 0x55, 0x56, 0x57, 0x58, 0x59, 0x5A, 0x5B, 0x5C, 0x5D, 0x5E, 0x5F,
                    0x50, 0x51, 0x52, 0x53, 0x54, 0x55,
                }
            },
        }
    };

    this->m_mqtt_publish_adv_res = true;
    adv_post_do_async_comm(&adv_post_state);
    ASSERT_FALSE(this->m_flag_gateway_restart_low_memory);
    ASSERT_FALSE(this->m_http_server_mutex_locked);
    ASSERT_FALSE(this->m_leds_notify_http1_data_sent_fail);
    ASSERT_FALSE(this->m_leds_notify_http2_data_sent_fail);
    ASSERT_EQ(0, this->m_http_post_advs_call_cnt);
    ASSERT_EQ(0, this->m_adv_post_statistics_do_send_call_cnt);
    ASSERT_EQ(0, this->m_mqtt_publish_adv_call_cnt);

    ASSERT_EQ(ADV_POST_SIG_DO_ASYNC_COMM, this->m_adv_post_signals_send_sig);
    ASSERT_TRUE(this->m_adv_post_timers_start_timer_sig_do_async_comm);
    this->m_adv_post_timers_start_timer_sig_do_async_comm = false;
    ASSERT_EQ(0, this->m_mqtt_publish_adv_arg_timestamp);

    adv_post_state.flag_network_connected = false;
    this->m_mqtt_publish_adv_res          = false;

    this->m_http_async_poll_res = true;
    adv_post_do_async_comm(&adv_post_state);
    ASSERT_FALSE(this->m_flag_gateway_restart_low_memory);
    ASSERT_FALSE(this->m_http_server_mutex_locked);
    ASSERT_FALSE(this->m_leds_notify_http1_data_sent_fail);
    ASSERT_FALSE(this->m_leds_notify_http2_data_sent_fail);
    ASSERT_EQ(0, this->m_http_post_advs_call_cnt);
    ASSERT_EQ(0, this->m_adv_post_statistics_do_send_call_cnt);
    ASSERT_EQ(1, this->m_mqtt_publish_adv_call_cnt);
    ASSERT_FALSE(this->m_adv_post_timers_start_timer_sig_do_async_comm);
    this->m_adv_post_timers_start_timer_sig_do_async_comm = false;
    this->m_http_async_poll_res                           = false;

    ASSERT_TRUE(this->m_mem_alloc_trace.is_empty());
}

TEST_F(TestAdvPostAsyncComm, test_relaying_disabled) // NOLINT
{
    this->m_flag_time_is_synchronized              = true;
    this->m_http_server_mutex_try_lock_res         = true;
    this->m_http_post_advs_res                     = true;
    this->m_gw_cfg_get_http_stat_use_http_stat_res = true;
    this->m_gw_cfg_get_mqtt_use_mqtt_res           = true;
    this->m_gw_cfg_get_ntp_use_res                 = true;
    this->m_adv_post_statistics_do_send_res        = true;
    this->m_gw_status_is_mqtt_connected_res        = true;

    adv_post_state_t adv_post_state = {
        .flag_primary_time_sync_is_done  = false,
        .flag_network_connected          = true,
        .flag_async_comm_in_progress     = false,
        .flag_need_to_send_advs1         = true,
        .flag_need_to_send_advs2         = false,
        .flag_need_to_send_statistics    = false,
        .flag_need_to_send_mqtt_periodic = false,
        .flag_relaying_enabled           = false,
        .flag_use_timestamps             = true,
        .flag_stop                       = false,
    };
    this->m_reports = {
        .num_of_advs = 2,
        .table = {
            [0] = {
                .timestamp = 100500,
                .samples_counter = 301,
                .tag_mac = { 0x11, 0x12, 0x13, 0x14, 0x15, 0x16,},
                .rssi = -49,
                .data_len = 24,
                .data_buf = {
                    0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27, 0x28, 0x29, 0x2A, 0x2B, 0x2C, 0x2D, 0x2E, 0x2F,
                    0x30, 0x31, 0x32, 0x33, 0x34, 0x35,
                }
            },
            [1] = {
                .timestamp = 100501,
                .samples_counter = 302,
                .tag_mac = { 0x21, 0x22, 0x23, 0x24, 0x25, 0x26,},
                .rssi = -48,
                .data_len = 24,
                .data_buf = {
                    0x51, 0x52, 0x53, 0x54, 0x55, 0x56, 0x57, 0x58, 0x59, 0x5A, 0x5B, 0x5C, 0x5D, 0x5E, 0x5F,
                    0x50, 0x51, 0x52, 0x53, 0x54, 0x55,
                }
            },
        }
    };

    adv_post_do_async_comm(&adv_post_state);
    ASSERT_FALSE(this->m_flag_gateway_restart_low_memory);
    ASSERT_FALSE(this->m_http_server_mutex_locked);
    ASSERT_FALSE(this->m_leds_notify_http1_data_sent_fail);
    ASSERT_FALSE(this->m_leds_notify_http2_data_sent_fail);
    ASSERT_EQ(0, this->m_http_post_advs_call_cnt);
    ASSERT_EQ(0, this->m_adv_post_statistics_do_send_call_cnt);
    ASSERT_EQ(0, this->m_mqtt_publish_adv_call_cnt);
    ASSERT_FALSE(this->m_adv_post_timers_start_timer_sig_do_async_comm);
    this->m_adv_post_timers_start_timer_sig_do_async_comm = false;

    this->m_http_async_poll_res = true;
    adv_post_do_async_comm(&adv_post_state);
    ASSERT_FALSE(this->m_flag_gateway_restart_low_memory);
    ASSERT_FALSE(this->m_http_server_mutex_locked);
    ASSERT_FALSE(this->m_leds_notify_http1_data_sent_fail);
    ASSERT_FALSE(this->m_leds_notify_http2_data_sent_fail);
    ASSERT_EQ(0, this->m_http_post_advs_call_cnt);
    ASSERT_EQ(0, this->m_adv_post_statistics_do_send_call_cnt);
    ASSERT_EQ(0, this->m_mqtt_publish_adv_call_cnt);
    this->m_http_async_poll_res = false;

    adv_post_state.flag_need_to_send_advs1          = false;
    adv_post_state.flag_need_to_send_advs2          = true;
    this->m_hmac_sha256_set_key_for_http_custom_res = true;
    adv_post_do_async_comm(&adv_post_state);
    ASSERT_FALSE(this->m_flag_gateway_restart_low_memory);
    ASSERT_FALSE(this->m_http_server_mutex_locked);
    ASSERT_FALSE(this->m_leds_notify_http1_data_sent_fail);
    ASSERT_FALSE(this->m_leds_notify_http2_data_sent_fail);
    ASSERT_EQ(0, this->m_http_post_advs_call_cnt);
    ASSERT_EQ(0, this->m_adv_post_statistics_do_send_call_cnt);
    ASSERT_EQ(0, this->m_mqtt_publish_adv_call_cnt);

    this->m_http_async_poll_res = true;
    adv_post_do_async_comm(&adv_post_state);
    ASSERT_FALSE(this->m_flag_gateway_restart_low_memory);
    ASSERT_FALSE(this->m_http_server_mutex_locked);
    ASSERT_FALSE(this->m_leds_notify_http1_data_sent_fail);
    ASSERT_FALSE(this->m_leds_notify_http2_data_sent_fail);
    ASSERT_EQ(0, this->m_http_post_advs_call_cnt);
    ASSERT_EQ(0, this->m_adv_post_statistics_do_send_call_cnt);
    ASSERT_EQ(0, this->m_mqtt_publish_adv_call_cnt);
    this->m_http_async_poll_res = false;

    adv_post_state.flag_need_to_send_advs2      = false;
    adv_post_state.flag_need_to_send_statistics = true;
    this->m_hmac_sha256_set_key_for_stats_res   = true;
    adv_post_do_async_comm(&adv_post_state);
    ASSERT_FALSE(this->m_flag_gateway_restart_low_memory);
    ASSERT_FALSE(this->m_http_server_mutex_locked);
    ASSERT_FALSE(this->m_leds_notify_http1_data_sent_fail);
    ASSERT_FALSE(this->m_leds_notify_http2_data_sent_fail);
    ASSERT_EQ(0, this->m_http_post_advs_call_cnt);
    ASSERT_EQ(0, this->m_adv_post_statistics_do_send_call_cnt);
    ASSERT_EQ(0, this->m_mqtt_publish_adv_call_cnt);

    this->m_http_async_poll_res = true;
    adv_post_do_async_comm(&adv_post_state);
    ASSERT_FALSE(this->m_flag_gateway_restart_low_memory);
    ASSERT_FALSE(this->m_http_server_mutex_locked);
    ASSERT_FALSE(this->m_leds_notify_http1_data_sent_fail);
    ASSERT_FALSE(this->m_leds_notify_http2_data_sent_fail);
    ASSERT_EQ(0, this->m_http_post_advs_call_cnt);
    ASSERT_EQ(0, this->m_adv_post_statistics_do_send_call_cnt);
    ASSERT_EQ(0, this->m_mqtt_publish_adv_call_cnt);
    this->m_http_async_poll_res = false;

    adv_post_state.flag_need_to_send_statistics    = false;
    adv_post_state.flag_need_to_send_mqtt_periodic = true;
    this->m_mqtt_publish_adv_res                   = true;
    adv_post_do_async_comm(&adv_post_state);
    ASSERT_FALSE(this->m_flag_gateway_restart_low_memory);
    ASSERT_FALSE(this->m_http_server_mutex_locked);
    ASSERT_FALSE(this->m_leds_notify_http1_data_sent_fail);
    ASSERT_FALSE(this->m_leds_notify_http2_data_sent_fail);
    ASSERT_EQ(0, this->m_http_post_advs_call_cnt);
    ASSERT_EQ(0, this->m_adv_post_statistics_do_send_call_cnt);
    ASSERT_EQ(0, this->m_mqtt_publish_adv_call_cnt);

    ASSERT_EQ(0, this->m_adv_post_signals_send_sig);
    ASSERT_FALSE(this->m_adv_post_timers_start_timer_sig_do_async_comm);
    this->m_adv_post_timers_start_timer_sig_do_async_comm = false;
    ASSERT_EQ(0, this->m_mqtt_publish_adv_arg_timestamp);

    this->m_http_async_poll_res = true;
    adv_post_do_async_comm(&adv_post_state);
    ASSERT_FALSE(this->m_flag_gateway_restart_low_memory);
    ASSERT_FALSE(this->m_http_server_mutex_locked);
    ASSERT_FALSE(this->m_leds_notify_http1_data_sent_fail);
    ASSERT_FALSE(this->m_leds_notify_http2_data_sent_fail);
    ASSERT_EQ(0, this->m_http_post_advs_call_cnt);
    ASSERT_EQ(0, this->m_adv_post_statistics_do_send_call_cnt);
    ASSERT_EQ(0, this->m_mqtt_publish_adv_call_cnt);
    ASSERT_FALSE(this->m_adv_post_timers_start_timer_sig_do_async_comm);
    this->m_adv_post_timers_start_timer_sig_do_async_comm = false;
    this->m_http_async_poll_res                           = false;

    ASSERT_TRUE(this->m_mem_alloc_trace.is_empty());
}

TEST_F(TestAdvPostAsyncComm, test_http_post_advs_failed) // NOLINT
{
    this->m_flag_time_is_synchronized              = true;
    this->m_http_server_mutex_try_lock_res         = true;
    this->m_http_post_advs_res                     = false;
    this->m_gw_cfg_get_http_stat_use_http_stat_res = true;
    this->m_gw_cfg_get_mqtt_use_mqtt_res           = true;
    this->m_gw_cfg_get_ntp_use_res                 = true;
    this->m_adv_post_statistics_do_send_res        = true;
    this->m_gw_status_is_mqtt_connected_res        = true;

    adv_post_state_t adv_post_state = {
        .flag_primary_time_sync_is_done  = false,
        .flag_network_connected          = true,
        .flag_async_comm_in_progress     = false,
        .flag_need_to_send_advs1         = true,
        .flag_need_to_send_advs2         = false,
        .flag_need_to_send_statistics    = false,
        .flag_need_to_send_mqtt_periodic = false,
        .flag_relaying_enabled           = true,
        .flag_use_timestamps             = true,
        .flag_stop                       = false,
    };
    this->m_reports = {
        .num_of_advs = 2,
        .table = {
            [0] = {
                .timestamp = 100500,
                .samples_counter = 301,
                .tag_mac = { 0x11, 0x12, 0x13, 0x14, 0x15, 0x16,},
                .rssi = -49,
                .data_len = 24,
                .data_buf = {
                    0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27, 0x28, 0x29, 0x2A, 0x2B, 0x2C, 0x2D, 0x2E, 0x2F,
                    0x30, 0x31, 0x32, 0x33, 0x34, 0x35,
                }
            },
            [1] = {
                .timestamp = 100501,
                .samples_counter = 302,
                .tag_mac = { 0x21, 0x22, 0x23, 0x24, 0x25, 0x26,},
                .rssi = -48,
                .data_len = 24,
                .data_buf = {
                    0x51, 0x52, 0x53, 0x54, 0x55, 0x56, 0x57, 0x58, 0x59, 0x5A, 0x5B, 0x5C, 0x5D, 0x5E, 0x5F,
                    0x50, 0x51, 0x52, 0x53, 0x54, 0x55,
                }
            },
        }
    };

    adv_post_do_async_comm(&adv_post_state);
    ASSERT_FALSE(this->m_flag_gateway_restart_low_memory);
    ASSERT_FALSE(this->m_http_server_mutex_locked);
    ASSERT_TRUE(this->m_leds_notify_http1_data_sent_fail);
    this->m_leds_notify_http1_data_sent_fail = false;
    ASSERT_FALSE(this->m_leds_notify_http2_data_sent_fail);
    ASSERT_EQ(1, this->m_http_post_advs_call_cnt);
    ASSERT_EQ(0, this->m_adv_post_statistics_do_send_call_cnt);
    ASSERT_EQ(0, this->m_mqtt_publish_adv_call_cnt);
    ASSERT_TRUE(this->m_adv_post_timers_start_timer_sig_do_async_comm);
    this->m_adv_post_timers_start_timer_sig_do_async_comm = false;

    this->m_http_async_poll_res = true;
    adv_post_do_async_comm(&adv_post_state);
    ASSERT_FALSE(this->m_flag_gateway_restart_low_memory);
    ASSERT_FALSE(this->m_http_server_mutex_locked);
    ASSERT_TRUE(this->m_leds_notify_http1_data_sent_fail);
    this->m_leds_notify_http1_data_sent_fail = false;
    ASSERT_FALSE(this->m_leds_notify_http2_data_sent_fail);
    ASSERT_EQ(2, this->m_http_post_advs_call_cnt);
    ASSERT_EQ(0, this->m_adv_post_statistics_do_send_call_cnt);
    ASSERT_EQ(0, this->m_mqtt_publish_adv_call_cnt);
    this->m_http_async_poll_res = false;

    adv_post_state.flag_need_to_send_advs1          = false;
    adv_post_state.flag_need_to_send_advs2          = true;
    this->m_hmac_sha256_set_key_for_http_custom_res = true;
    adv_post_do_async_comm(&adv_post_state);
    ASSERT_FALSE(this->m_flag_gateway_restart_low_memory);
    ASSERT_FALSE(this->m_http_server_mutex_locked);
    ASSERT_FALSE(this->m_leds_notify_http1_data_sent_fail);
    ASSERT_TRUE(this->m_leds_notify_http2_data_sent_fail);
    this->m_leds_notify_http2_data_sent_fail = false;
    ASSERT_EQ(3, this->m_http_post_advs_call_cnt);
    ASSERT_EQ(0, this->m_adv_post_statistics_do_send_call_cnt);
    ASSERT_EQ(0, this->m_mqtt_publish_adv_call_cnt);

    this->m_http_async_poll_res = true;
    adv_post_do_async_comm(&adv_post_state);
    ASSERT_FALSE(this->m_flag_gateway_restart_low_memory);
    ASSERT_FALSE(this->m_http_server_mutex_locked);
    ASSERT_FALSE(this->m_leds_notify_http1_data_sent_fail);
    ASSERT_TRUE(this->m_leds_notify_http2_data_sent_fail);
    this->m_leds_notify_http2_data_sent_fail = false;
    ASSERT_EQ(4, this->m_http_post_advs_call_cnt);
    ASSERT_EQ(0, this->m_adv_post_statistics_do_send_call_cnt);
    ASSERT_EQ(0, this->m_mqtt_publish_adv_call_cnt);
    this->m_http_async_poll_res = false;

    adv_post_state.flag_need_to_send_advs2      = false;
    adv_post_state.flag_need_to_send_statistics = true;
    this->m_hmac_sha256_set_key_for_stats_res   = true;
    adv_post_do_async_comm(&adv_post_state);
    ASSERT_FALSE(this->m_flag_gateway_restart_low_memory);
    ASSERT_TRUE(this->m_http_server_mutex_locked);
    ASSERT_FALSE(this->m_leds_notify_http1_data_sent_fail);
    ASSERT_FALSE(this->m_leds_notify_http2_data_sent_fail);
    ASSERT_EQ(4, this->m_http_post_advs_call_cnt);
    ASSERT_EQ(1, this->m_adv_post_statistics_do_send_call_cnt);
    ASSERT_EQ(0, this->m_mqtt_publish_adv_call_cnt);

    this->m_http_async_poll_res = true;
    adv_post_do_async_comm(&adv_post_state);
    ASSERT_FALSE(this->m_flag_gateway_restart_low_memory);
    ASSERT_FALSE(this->m_http_server_mutex_locked);
    ASSERT_FALSE(this->m_leds_notify_http1_data_sent_fail);
    ASSERT_FALSE(this->m_leds_notify_http2_data_sent_fail);
    ASSERT_EQ(4, this->m_http_post_advs_call_cnt);
    ASSERT_EQ(1, this->m_adv_post_statistics_do_send_call_cnt);
    ASSERT_EQ(0, this->m_mqtt_publish_adv_call_cnt);
    this->m_http_async_poll_res = false;

    adv_post_state.flag_need_to_send_statistics    = false;
    adv_post_state.flag_need_to_send_mqtt_periodic = true;
    this->m_mqtt_publish_adv_res                   = true;
    adv_post_do_async_comm(&adv_post_state);
    ASSERT_FALSE(this->m_flag_gateway_restart_low_memory);
    ASSERT_FALSE(this->m_http_server_mutex_locked);
    ASSERT_FALSE(this->m_leds_notify_http1_data_sent_fail);
    ASSERT_FALSE(this->m_leds_notify_http2_data_sent_fail);
    ASSERT_EQ(4, this->m_http_post_advs_call_cnt);
    ASSERT_EQ(1, this->m_adv_post_statistics_do_send_call_cnt);
    ASSERT_EQ(0, this->m_mqtt_publish_adv_call_cnt);

    ASSERT_EQ(ADV_POST_SIG_DO_ASYNC_COMM, this->m_adv_post_signals_send_sig);
    ASSERT_TRUE(this->m_adv_post_timers_start_timer_sig_do_async_comm);
    this->m_adv_post_timers_start_timer_sig_do_async_comm = false;
    ASSERT_EQ(0, this->m_mqtt_publish_adv_arg_timestamp);

    this->m_http_async_poll_res = true;
    adv_post_do_async_comm(&adv_post_state);
    ASSERT_FALSE(this->m_flag_gateway_restart_low_memory);
    ASSERT_FALSE(this->m_http_server_mutex_locked);
    ASSERT_FALSE(this->m_leds_notify_http1_data_sent_fail);
    ASSERT_FALSE(this->m_leds_notify_http2_data_sent_fail);
    ASSERT_EQ(4, this->m_http_post_advs_call_cnt);
    ASSERT_EQ(1, this->m_adv_post_statistics_do_send_call_cnt);
    ASSERT_EQ(1, this->m_mqtt_publish_adv_call_cnt);
    ASSERT_TRUE(this->m_adv_post_timers_start_timer_sig_do_async_comm);
    this->m_adv_post_timers_start_timer_sig_do_async_comm = false;
    this->m_http_async_poll_res                           = false;

    ASSERT_TRUE(this->m_mqtt_publish_adv_arg_flag_use_timestamps);
    ASSERT_NE(0, this->m_mqtt_publish_adv_arg_timestamp);
    {
        const adv_report_t* const p_adv = &this->m_reports.table[0];
        ASSERT_EQ(p_adv->timestamp, this->m_mqtt_publish_adv_arg_adv.timestamp);
        ASSERT_EQ(p_adv->samples_counter, this->m_mqtt_publish_adv_arg_adv.samples_counter);
        ASSERT_EQ(p_adv->rssi, this->m_mqtt_publish_adv_arg_adv.rssi);
        ASSERT_EQ(p_adv->data_len, this->m_mqtt_publish_adv_arg_adv.data_len);
        {
            std::array<uint8_t, MAC_ADDRESS_NUM_BYTES> tag_mac;
            const mac_address_bin_t* const             p_mac = &this->m_mqtt_publish_adv_arg_adv.tag_mac;
            std::copy(std::begin(p_mac->mac), std::end(p_mac->mac), tag_mac.begin());
            std::array<uint8_t, MAC_ADDRESS_NUM_BYTES> tag_mac_expected;
            const mac_address_bin_t* const             p_mac_expected = &p_adv->tag_mac;
            std::copy(std::begin(p_mac_expected->mac), std::end(p_mac_expected->mac), tag_mac_expected.begin());
            ASSERT_EQ(tag_mac_expected, tag_mac);
        }
        {
            const size_t            len = p_adv->data_len;
            std::array<uint8_t, 24> data_buf;
            std::copy(
                &this->m_mqtt_publish_adv_arg_adv.data_buf[0],
                &this->m_mqtt_publish_adv_arg_adv.data_buf[len],
                data_buf.begin());
            std::array<uint8_t, 24> data_buf_expected;
            std::copy(&p_adv->data_buf[0], &p_adv->data_buf[len], data_buf_expected.begin());
            ASSERT_EQ(data_buf_expected, data_buf);
        }
    }

    this->m_mqtt_publish_adv_arg_timestamp = 0;
    this->m_http_async_poll_res            = true;
    adv_post_do_async_comm(&adv_post_state);
    ASSERT_FALSE(this->m_flag_gateway_restart_low_memory);
    ASSERT_FALSE(this->m_http_server_mutex_locked);
    ASSERT_FALSE(this->m_leds_notify_http1_data_sent_fail);
    ASSERT_FALSE(this->m_leds_notify_http2_data_sent_fail);
    ASSERT_EQ(4, this->m_http_post_advs_call_cnt);
    ASSERT_EQ(1, this->m_adv_post_statistics_do_send_call_cnt);
    ASSERT_EQ(2, this->m_mqtt_publish_adv_call_cnt);
    ASSERT_FALSE(this->m_adv_post_timers_start_timer_sig_do_async_comm);
    this->m_http_async_poll_res = false;
    ASSERT_TRUE(this->m_mqtt_publish_adv_arg_flag_use_timestamps);
    ASSERT_NE(0, this->m_mqtt_publish_adv_arg_timestamp);

    {
        const adv_report_t* const p_adv = &this->m_reports.table[1];
        ASSERT_EQ(p_adv->timestamp, this->m_mqtt_publish_adv_arg_adv.timestamp);
        ASSERT_EQ(p_adv->samples_counter, this->m_mqtt_publish_adv_arg_adv.samples_counter);
        ASSERT_EQ(p_adv->rssi, this->m_mqtt_publish_adv_arg_adv.rssi);
        ASSERT_EQ(p_adv->data_len, this->m_mqtt_publish_adv_arg_adv.data_len);
        {
            std::array<uint8_t, MAC_ADDRESS_NUM_BYTES> tag_mac;
            const mac_address_bin_t* const             p_mac = &this->m_mqtt_publish_adv_arg_adv.tag_mac;
            std::copy(std::begin(p_mac->mac), std::end(p_mac->mac), tag_mac.begin());
            std::array<uint8_t, MAC_ADDRESS_NUM_BYTES> tag_mac_expected;
            const mac_address_bin_t* const             p_mac_expected = &p_adv->tag_mac;
            std::copy(std::begin(p_mac_expected->mac), std::end(p_mac_expected->mac), tag_mac_expected.begin());
            ASSERT_EQ(tag_mac_expected, tag_mac);
        }
        {
            const size_t            len = p_adv->data_len;
            std::array<uint8_t, 24> data_buf;
            std::copy(
                &this->m_mqtt_publish_adv_arg_adv.data_buf[0],
                &this->m_mqtt_publish_adv_arg_adv.data_buf[len],
                data_buf.begin());
            std::array<uint8_t, 24> data_buf_expected;
            std::copy(&p_adv->data_buf[0], &p_adv->data_buf[len], data_buf_expected.begin());
            ASSERT_EQ(data_buf_expected, data_buf);
        }
    }
    ASSERT_TRUE(this->m_mem_alloc_trace.is_empty());
}

TEST_F(TestAdvPostAsyncComm, test_http_server_mutex_try_lock_false) // NOLINT
{
    this->m_flag_time_is_synchronized              = true;
    this->m_http_server_mutex_try_lock_res         = false;
    this->m_http_post_advs_res                     = true;
    this->m_gw_cfg_get_http_stat_use_http_stat_res = true;
    this->m_gw_cfg_get_mqtt_use_mqtt_res           = true;
    this->m_gw_cfg_get_ntp_use_res                 = true;
    this->m_adv_post_statistics_do_send_res        = true;
    this->m_gw_status_is_mqtt_connected_res        = true;

    adv_post_state_t adv_post_state = {
        .flag_primary_time_sync_is_done  = false,
        .flag_network_connected          = true,
        .flag_async_comm_in_progress     = false,
        .flag_need_to_send_advs1         = true,
        .flag_need_to_send_advs2         = false,
        .flag_need_to_send_statistics    = false,
        .flag_need_to_send_mqtt_periodic = false,
        .flag_relaying_enabled           = true,
        .flag_use_timestamps             = true,
        .flag_stop                       = false,
    };
    this->m_reports = {
        .num_of_advs = 2,
        .table = {
            [0] = {
                .timestamp = 100500,
                .samples_counter = 301,
                .tag_mac = { 0x11, 0x12, 0x13, 0x14, 0x15, 0x16,},
                .rssi = -49,
                .data_len = 24,
                .data_buf = {
                    0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27, 0x28, 0x29, 0x2A, 0x2B, 0x2C, 0x2D, 0x2E, 0x2F,
                    0x30, 0x31, 0x32, 0x33, 0x34, 0x35,
                }
            },
            [1] = {
                .timestamp = 100501,
                .samples_counter = 302,
                .tag_mac = { 0x21, 0x22, 0x23, 0x24, 0x25, 0x26,},
                .rssi = -48,
                .data_len = 24,
                .data_buf = {
                    0x51, 0x52, 0x53, 0x54, 0x55, 0x56, 0x57, 0x58, 0x59, 0x5A, 0x5B, 0x5C, 0x5D, 0x5E, 0x5F,
                    0x50, 0x51, 0x52, 0x53, 0x54, 0x55,
                }
            },
        }
    };

    adv_post_do_async_comm(&adv_post_state);
    ASSERT_FALSE(this->m_flag_gateway_restart_low_memory);
    ASSERT_FALSE(this->m_http_server_mutex_locked);
    ASSERT_FALSE(this->m_leds_notify_http1_data_sent_fail);
    ASSERT_FALSE(this->m_leds_notify_http2_data_sent_fail);
    ASSERT_EQ(0, this->m_http_post_advs_call_cnt);
    ASSERT_EQ(0, this->m_adv_post_statistics_do_send_call_cnt);
    ASSERT_EQ(0, this->m_mqtt_publish_adv_call_cnt);
    ASSERT_TRUE(this->m_adv_post_timers_start_timer_sig_do_async_comm);
    this->m_adv_post_timers_start_timer_sig_do_async_comm = false;

    this->m_http_async_poll_res = true;
    adv_post_do_async_comm(&adv_post_state);
    ASSERT_FALSE(this->m_flag_gateway_restart_low_memory);
    ASSERT_FALSE(this->m_http_server_mutex_locked);
    ASSERT_FALSE(this->m_leds_notify_http1_data_sent_fail);
    ASSERT_FALSE(this->m_leds_notify_http2_data_sent_fail);
    ASSERT_EQ(0, this->m_http_post_advs_call_cnt);
    ASSERT_EQ(0, this->m_adv_post_statistics_do_send_call_cnt);
    ASSERT_EQ(0, this->m_mqtt_publish_adv_call_cnt);
    this->m_http_async_poll_res = false;

    adv_post_state.flag_need_to_send_advs1          = false;
    adv_post_state.flag_need_to_send_advs2          = true;
    this->m_hmac_sha256_set_key_for_http_custom_res = true;
    adv_post_do_async_comm(&adv_post_state);
    ASSERT_FALSE(this->m_flag_gateway_restart_low_memory);
    ASSERT_FALSE(this->m_http_server_mutex_locked);
    ASSERT_FALSE(this->m_leds_notify_http1_data_sent_fail);
    ASSERT_FALSE(this->m_leds_notify_http2_data_sent_fail);
    ASSERT_EQ(0, this->m_http_post_advs_call_cnt);
    ASSERT_EQ(0, this->m_adv_post_statistics_do_send_call_cnt);
    ASSERT_EQ(0, this->m_mqtt_publish_adv_call_cnt);

    this->m_http_async_poll_res = true;
    adv_post_do_async_comm(&adv_post_state);
    ASSERT_FALSE(this->m_flag_gateway_restart_low_memory);
    ASSERT_FALSE(this->m_http_server_mutex_locked);
    ASSERT_FALSE(this->m_leds_notify_http1_data_sent_fail);
    ASSERT_FALSE(this->m_leds_notify_http2_data_sent_fail);
    ASSERT_EQ(0, this->m_http_post_advs_call_cnt);
    ASSERT_EQ(0, this->m_adv_post_statistics_do_send_call_cnt);
    ASSERT_EQ(0, this->m_mqtt_publish_adv_call_cnt);
    this->m_http_async_poll_res = false;

    adv_post_state.flag_need_to_send_advs2      = false;
    adv_post_state.flag_need_to_send_statistics = true;
    this->m_hmac_sha256_set_key_for_stats_res   = true;
    adv_post_do_async_comm(&adv_post_state);
    ASSERT_FALSE(this->m_flag_gateway_restart_low_memory);
    ASSERT_FALSE(this->m_http_server_mutex_locked);
    ASSERT_FALSE(this->m_leds_notify_http1_data_sent_fail);
    ASSERT_FALSE(this->m_leds_notify_http2_data_sent_fail);
    ASSERT_EQ(0, this->m_http_post_advs_call_cnt);
    ASSERT_EQ(0, this->m_adv_post_statistics_do_send_call_cnt);
    ASSERT_EQ(0, this->m_mqtt_publish_adv_call_cnt);

    this->m_http_async_poll_res = true;
    adv_post_do_async_comm(&adv_post_state);
    ASSERT_FALSE(this->m_flag_gateway_restart_low_memory);
    ASSERT_FALSE(this->m_http_server_mutex_locked);
    ASSERT_FALSE(this->m_leds_notify_http1_data_sent_fail);
    ASSERT_FALSE(this->m_leds_notify_http2_data_sent_fail);
    ASSERT_EQ(0, this->m_http_post_advs_call_cnt);
    ASSERT_EQ(0, this->m_adv_post_statistics_do_send_call_cnt);
    ASSERT_EQ(0, this->m_mqtt_publish_adv_call_cnt);
    this->m_http_async_poll_res = false;

    adv_post_state.flag_need_to_send_statistics    = false;
    adv_post_state.flag_need_to_send_mqtt_periodic = true;
    this->m_mqtt_publish_adv_res                   = true;
    adv_post_do_async_comm(&adv_post_state);
    ASSERT_FALSE(this->m_flag_gateway_restart_low_memory);
    ASSERT_FALSE(this->m_http_server_mutex_locked);
    ASSERT_FALSE(this->m_leds_notify_http1_data_sent_fail);
    ASSERT_FALSE(this->m_leds_notify_http2_data_sent_fail);
    ASSERT_EQ(0, this->m_http_post_advs_call_cnt);
    ASSERT_EQ(0, this->m_adv_post_statistics_do_send_call_cnt);
    ASSERT_EQ(0, this->m_mqtt_publish_adv_call_cnt);

    ASSERT_EQ(ADV_POST_SIG_DO_ASYNC_COMM, this->m_adv_post_signals_send_sig);
    ASSERT_TRUE(this->m_adv_post_timers_start_timer_sig_do_async_comm);
    this->m_adv_post_timers_start_timer_sig_do_async_comm = false;
    ASSERT_EQ(0, this->m_mqtt_publish_adv_arg_timestamp);

    this->m_http_async_poll_res = true;
    adv_post_do_async_comm(&adv_post_state);
    ASSERT_FALSE(this->m_flag_gateway_restart_low_memory);
    ASSERT_FALSE(this->m_http_server_mutex_locked);
    ASSERT_FALSE(this->m_leds_notify_http1_data_sent_fail);
    ASSERT_FALSE(this->m_leds_notify_http2_data_sent_fail);
    ASSERT_EQ(0, this->m_http_post_advs_call_cnt);
    ASSERT_EQ(0, this->m_adv_post_statistics_do_send_call_cnt);
    ASSERT_EQ(1, this->m_mqtt_publish_adv_call_cnt);
    ASSERT_TRUE(this->m_adv_post_timers_start_timer_sig_do_async_comm);
    this->m_adv_post_timers_start_timer_sig_do_async_comm = false;
    this->m_http_async_poll_res                           = false;

    ASSERT_TRUE(this->m_mqtt_publish_adv_arg_flag_use_timestamps);
    ASSERT_NE(0, this->m_mqtt_publish_adv_arg_timestamp);
    {
        const adv_report_t* const p_adv = &this->m_reports.table[0];
        ASSERT_EQ(p_adv->timestamp, this->m_mqtt_publish_adv_arg_adv.timestamp);
        ASSERT_EQ(p_adv->samples_counter, this->m_mqtt_publish_adv_arg_adv.samples_counter);
        ASSERT_EQ(p_adv->rssi, this->m_mqtt_publish_adv_arg_adv.rssi);
        ASSERT_EQ(p_adv->data_len, this->m_mqtt_publish_adv_arg_adv.data_len);
        {
            std::array<uint8_t, MAC_ADDRESS_NUM_BYTES> tag_mac;
            const mac_address_bin_t* const             p_mac = &this->m_mqtt_publish_adv_arg_adv.tag_mac;
            std::copy(std::begin(p_mac->mac), std::end(p_mac->mac), tag_mac.begin());
            std::array<uint8_t, MAC_ADDRESS_NUM_BYTES> tag_mac_expected;
            const mac_address_bin_t* const             p_mac_expected = &p_adv->tag_mac;
            std::copy(std::begin(p_mac_expected->mac), std::end(p_mac_expected->mac), tag_mac_expected.begin());
            ASSERT_EQ(tag_mac_expected, tag_mac);
        }
        {
            const size_t            len = p_adv->data_len;
            std::array<uint8_t, 24> data_buf;
            std::copy(
                &this->m_mqtt_publish_adv_arg_adv.data_buf[0],
                &this->m_mqtt_publish_adv_arg_adv.data_buf[len],
                data_buf.begin());
            std::array<uint8_t, 24> data_buf_expected;
            std::copy(&p_adv->data_buf[0], &p_adv->data_buf[len], data_buf_expected.begin());
            ASSERT_EQ(data_buf_expected, data_buf);
        }
    }

    this->m_mqtt_publish_adv_arg_timestamp = 0;
    this->m_http_async_poll_res            = true;
    adv_post_do_async_comm(&adv_post_state);
    ASSERT_FALSE(this->m_flag_gateway_restart_low_memory);
    ASSERT_FALSE(this->m_http_server_mutex_locked);
    ASSERT_FALSE(this->m_leds_notify_http1_data_sent_fail);
    ASSERT_FALSE(this->m_leds_notify_http2_data_sent_fail);
    ASSERT_EQ(0, this->m_http_post_advs_call_cnt);
    ASSERT_EQ(0, this->m_adv_post_statistics_do_send_call_cnt);
    ASSERT_EQ(2, this->m_mqtt_publish_adv_call_cnt);
    ASSERT_FALSE(this->m_adv_post_timers_start_timer_sig_do_async_comm);
    this->m_http_async_poll_res = false;
    ASSERT_TRUE(this->m_mqtt_publish_adv_arg_flag_use_timestamps);
    ASSERT_NE(0, this->m_mqtt_publish_adv_arg_timestamp);

    {
        const adv_report_t* const p_adv = &this->m_reports.table[1];
        ASSERT_EQ(p_adv->timestamp, this->m_mqtt_publish_adv_arg_adv.timestamp);
        ASSERT_EQ(p_adv->samples_counter, this->m_mqtt_publish_adv_arg_adv.samples_counter);
        ASSERT_EQ(p_adv->rssi, this->m_mqtt_publish_adv_arg_adv.rssi);
        ASSERT_EQ(p_adv->data_len, this->m_mqtt_publish_adv_arg_adv.data_len);
        {
            std::array<uint8_t, MAC_ADDRESS_NUM_BYTES> tag_mac;
            const mac_address_bin_t* const             p_mac = &this->m_mqtt_publish_adv_arg_adv.tag_mac;
            std::copy(std::begin(p_mac->mac), std::end(p_mac->mac), tag_mac.begin());
            std::array<uint8_t, MAC_ADDRESS_NUM_BYTES> tag_mac_expected;
            const mac_address_bin_t* const             p_mac_expected = &p_adv->tag_mac;
            std::copy(std::begin(p_mac_expected->mac), std::end(p_mac_expected->mac), tag_mac_expected.begin());
            ASSERT_EQ(tag_mac_expected, tag_mac);
        }
        {
            const size_t            len = p_adv->data_len;
            std::array<uint8_t, 24> data_buf;
            std::copy(
                &this->m_mqtt_publish_adv_arg_adv.data_buf[0],
                &this->m_mqtt_publish_adv_arg_adv.data_buf[len],
                data_buf.begin());
            std::array<uint8_t, 24> data_buf_expected;
            std::copy(&p_adv->data_buf[0], &p_adv->data_buf[len], data_buf_expected.begin());
            ASSERT_EQ(data_buf_expected, data_buf);
        }
    }
    ASSERT_TRUE(this->m_mem_alloc_trace.is_empty());
}

TEST_F(TestAdvPostAsyncComm, test_http_async_poll_failed) // NOLINT
{
    this->m_flag_time_is_synchronized              = true;
    this->m_http_server_mutex_try_lock_res         = true;
    this->m_http_post_advs_res                     = true;
    this->m_gw_cfg_get_http_stat_use_http_stat_res = true;
    this->m_gw_cfg_get_mqtt_use_mqtt_res           = true;
    this->m_gw_cfg_get_ntp_use_res                 = true;
    this->m_adv_post_statistics_do_send_res        = true;
    this->m_gw_status_is_mqtt_connected_res        = true;

    adv_post_state_t adv_post_state = {
        .flag_primary_time_sync_is_done  = false,
        .flag_network_connected          = true,
        .flag_async_comm_in_progress     = false,
        .flag_need_to_send_advs1         = true,
        .flag_need_to_send_advs2         = false,
        .flag_need_to_send_statistics    = false,
        .flag_need_to_send_mqtt_periodic = false,
        .flag_relaying_enabled           = true,
        .flag_use_timestamps             = true,
        .flag_stop                       = false,
    };
    this->m_reports = {
        .num_of_advs = 2,
        .table = {
            [0] = {
                .timestamp = 100500,
                .samples_counter = 301,
                .tag_mac = { 0x11, 0x12, 0x13, 0x14, 0x15, 0x16,},
                .rssi = -49,
                .data_len = 24,
                .data_buf = {
                    0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27, 0x28, 0x29, 0x2A, 0x2B, 0x2C, 0x2D, 0x2E, 0x2F,
                    0x30, 0x31, 0x32, 0x33, 0x34, 0x35,
                }
            },
            [1] = {
                .timestamp = 100501,
                .samples_counter = 302,
                .tag_mac = { 0x21, 0x22, 0x23, 0x24, 0x25, 0x26,},
                .rssi = -48,
                .data_len = 24,
                .data_buf = {
                    0x51, 0x52, 0x53, 0x54, 0x55, 0x56, 0x57, 0x58, 0x59, 0x5A, 0x5B, 0x5C, 0x5D, 0x5E, 0x5F,
                    0x50, 0x51, 0x52, 0x53, 0x54, 0x55,
                }
            },
        }
    };

    adv_post_do_async_comm(&adv_post_state);
    ASSERT_FALSE(this->m_flag_gateway_restart_low_memory);
    ASSERT_TRUE(this->m_http_server_mutex_locked);
    ASSERT_FALSE(this->m_leds_notify_http1_data_sent_fail);
    ASSERT_FALSE(this->m_leds_notify_http2_data_sent_fail);
    ASSERT_EQ(1, this->m_http_post_advs_call_cnt);
    ASSERT_EQ(0, this->m_adv_post_statistics_do_send_call_cnt);
    ASSERT_EQ(0, this->m_mqtt_publish_adv_call_cnt);
    ASSERT_TRUE(this->m_adv_post_timers_start_timer_sig_do_async_comm);
    this->m_adv_post_timers_start_timer_sig_do_async_comm = false;

    this->m_hmac_sha256_set_key_for_http_ruuvi_res = true;
    ASSERT_TRUE(adv_post_set_hmac_sha256_key("key_str123"));
    ASSERT_EQ(string("key_str123"), this->m_hmac_sha256_set_key_for_http_ruuvi_key);
    this->m_hmac_sha256_set_key_for_http_ruuvi_res = false;
    adv_post_set_default_period(15 * 1000);
    ASSERT_EQ(15 * 1000, this->m_default_period_for_http_ruuvi);

    this->m_http_async_poll_res = false;
    adv_post_do_async_comm(&adv_post_state);
    ASSERT_FALSE(this->m_flag_gateway_restart_low_memory);
    ASSERT_TRUE(this->m_http_server_mutex_locked);
    ASSERT_FALSE(this->m_leds_notify_http1_data_sent_fail);
    ASSERT_FALSE(this->m_leds_notify_http2_data_sent_fail);
    ASSERT_EQ(1, this->m_http_post_advs_call_cnt);
    ASSERT_EQ(0, this->m_adv_post_statistics_do_send_call_cnt);
    ASSERT_EQ(0, this->m_mqtt_publish_adv_call_cnt);

    adv_post_state.flag_need_to_send_advs1          = false;
    adv_post_state.flag_need_to_send_advs2          = true;
    this->m_hmac_sha256_set_key_for_http_custom_res = true;
    adv_post_do_async_comm(&adv_post_state);
    ASSERT_FALSE(this->m_flag_gateway_restart_low_memory);
    ASSERT_TRUE(this->m_http_server_mutex_locked);
    ASSERT_FALSE(this->m_leds_notify_http1_data_sent_fail);
    ASSERT_FALSE(this->m_leds_notify_http2_data_sent_fail);
    ASSERT_EQ(1, this->m_http_post_advs_call_cnt);
    ASSERT_EQ(0, this->m_adv_post_statistics_do_send_call_cnt);
    ASSERT_EQ(0, this->m_mqtt_publish_adv_call_cnt);

    this->m_http_async_poll_res = false;
    adv_post_do_async_comm(&adv_post_state);
    ASSERT_FALSE(this->m_flag_gateway_restart_low_memory);
    ASSERT_TRUE(this->m_http_server_mutex_locked);
    ASSERT_FALSE(this->m_leds_notify_http1_data_sent_fail);
    ASSERT_FALSE(this->m_leds_notify_http2_data_sent_fail);
    ASSERT_EQ(1, this->m_http_post_advs_call_cnt);
    ASSERT_EQ(0, this->m_adv_post_statistics_do_send_call_cnt);
    ASSERT_EQ(0, this->m_mqtt_publish_adv_call_cnt);

    adv_post_state.flag_need_to_send_advs2      = false;
    adv_post_state.flag_need_to_send_statistics = true;
    this->m_hmac_sha256_set_key_for_stats_res   = true;
    adv_post_do_async_comm(&adv_post_state);
    ASSERT_FALSE(this->m_flag_gateway_restart_low_memory);
    ASSERT_TRUE(this->m_http_server_mutex_locked);
    ASSERT_FALSE(this->m_leds_notify_http1_data_sent_fail);
    ASSERT_FALSE(this->m_leds_notify_http2_data_sent_fail);
    ASSERT_EQ(1, this->m_http_post_advs_call_cnt);
    ASSERT_EQ(0, this->m_adv_post_statistics_do_send_call_cnt);
    ASSERT_EQ(0, this->m_mqtt_publish_adv_call_cnt);

    this->m_http_async_poll_res = false;
    adv_post_do_async_comm(&adv_post_state);
    ASSERT_FALSE(this->m_flag_gateway_restart_low_memory);
    ASSERT_TRUE(this->m_http_server_mutex_locked);
    ASSERT_FALSE(this->m_leds_notify_http1_data_sent_fail);
    ASSERT_FALSE(this->m_leds_notify_http2_data_sent_fail);
    ASSERT_EQ(1, this->m_http_post_advs_call_cnt);
    ASSERT_EQ(0, this->m_adv_post_statistics_do_send_call_cnt);
    ASSERT_EQ(0, this->m_mqtt_publish_adv_call_cnt);

    adv_post_state.flag_need_to_send_statistics    = false;
    adv_post_state.flag_need_to_send_mqtt_periodic = true;
    this->m_mqtt_publish_adv_res                   = true;
    adv_post_do_async_comm(&adv_post_state);
    ASSERT_FALSE(this->m_flag_gateway_restart_low_memory);
    ASSERT_TRUE(this->m_http_server_mutex_locked);
    ASSERT_FALSE(this->m_leds_notify_http1_data_sent_fail);
    ASSERT_FALSE(this->m_leds_notify_http2_data_sent_fail);
    ASSERT_EQ(1, this->m_http_post_advs_call_cnt);
    ASSERT_EQ(0, this->m_adv_post_statistics_do_send_call_cnt);
    ASSERT_EQ(0, this->m_mqtt_publish_adv_call_cnt);

    ASSERT_EQ(0, this->m_adv_post_signals_send_sig);
    ASSERT_TRUE(this->m_adv_post_timers_start_timer_sig_do_async_comm);
    this->m_adv_post_timers_start_timer_sig_do_async_comm = false;
    ASSERT_EQ(0, this->m_mqtt_publish_adv_arg_timestamp);

    this->m_http_async_poll_res = false;
    adv_post_do_async_comm(&adv_post_state);
    ASSERT_FALSE(this->m_flag_gateway_restart_low_memory);
    ASSERT_TRUE(this->m_http_server_mutex_locked);
    ASSERT_FALSE(this->m_leds_notify_http1_data_sent_fail);
    ASSERT_FALSE(this->m_leds_notify_http2_data_sent_fail);
    ASSERT_EQ(1, this->m_http_post_advs_call_cnt);
    ASSERT_EQ(0, this->m_adv_post_statistics_do_send_call_cnt);
    ASSERT_EQ(0, this->m_mqtt_publish_adv_call_cnt);
    ASSERT_TRUE(this->m_adv_post_timers_start_timer_sig_do_async_comm);
    this->m_adv_post_timers_start_timer_sig_do_async_comm = false;

    this->m_mqtt_publish_adv_arg_timestamp = 0;
    this->m_http_async_poll_res            = false;
    adv_post_do_async_comm(&adv_post_state);
    ASSERT_FALSE(this->m_flag_gateway_restart_low_memory);
    ASSERT_TRUE(this->m_http_server_mutex_locked);
    ASSERT_FALSE(this->m_leds_notify_http1_data_sent_fail);
    ASSERT_FALSE(this->m_leds_notify_http2_data_sent_fail);
    ASSERT_EQ(1, this->m_http_post_advs_call_cnt);
    ASSERT_EQ(0, this->m_adv_post_statistics_do_send_call_cnt);
    ASSERT_EQ(0, this->m_mqtt_publish_adv_call_cnt);

    ASSERT_TRUE(this->m_adv_post_timers_start_timer_sig_do_async_comm);

    ASSERT_TRUE(this->m_mem_alloc_trace.is_empty());
}

TEST_F(TestAdvPostAsyncComm, test_disable_sending_statistics) // NOLINT
{
    this->m_flag_time_is_synchronized              = true;
    this->m_http_server_mutex_try_lock_res         = true;
    this->m_http_post_advs_res                     = true;
    this->m_gw_cfg_get_http_stat_use_http_stat_res = false;
    this->m_gw_cfg_get_mqtt_use_mqtt_res           = false;
    this->m_gw_cfg_get_ntp_use_res                 = true;
    this->m_adv_post_statistics_do_send_res        = true;
    this->m_gw_status_is_mqtt_connected_res        = true;

    adv_post_state_t adv_post_state = {
        .flag_primary_time_sync_is_done  = false,
        .flag_network_connected          = true,
        .flag_async_comm_in_progress     = false,
        .flag_need_to_send_advs1         = false,
        .flag_need_to_send_advs2         = false,
        .flag_need_to_send_statistics    = true,
        .flag_need_to_send_mqtt_periodic = false,
        .flag_relaying_enabled           = true,
        .flag_use_timestamps             = true,
        .flag_stop                       = false,
    };
    this->m_reports = {
        .num_of_advs = 2,
        .table = {
            [0] = {
                .timestamp = 100500,
                .samples_counter = 301,
                .tag_mac = { 0x11, 0x12, 0x13, 0x14, 0x15, 0x16,},
                .rssi = -49,
                .data_len = 24,
                .data_buf = {
                    0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27, 0x28, 0x29, 0x2A, 0x2B, 0x2C, 0x2D, 0x2E, 0x2F,
                    0x30, 0x31, 0x32, 0x33, 0x34, 0x35,
                }
            },
            [1] = {
                .timestamp = 100501,
                .samples_counter = 302,
                .tag_mac = { 0x21, 0x22, 0x23, 0x24, 0x25, 0x26,},
                .rssi = -48,
                .data_len = 24,
                .data_buf = {
                    0x51, 0x52, 0x53, 0x54, 0x55, 0x56, 0x57, 0x58, 0x59, 0x5A, 0x5B, 0x5C, 0x5D, 0x5E, 0x5F,
                    0x50, 0x51, 0x52, 0x53, 0x54, 0x55,
                }
            },
        }
    };

    adv_post_do_async_comm(&adv_post_state);
    ASSERT_FALSE(this->m_flag_gateway_restart_low_memory);
    ASSERT_FALSE(this->m_http_server_mutex_locked);
    ASSERT_FALSE(this->m_leds_notify_http1_data_sent_fail);
    ASSERT_FALSE(this->m_leds_notify_http2_data_sent_fail);
    ASSERT_EQ(0, this->m_http_post_advs_call_cnt);
    ASSERT_EQ(0, this->m_adv_post_statistics_do_send_call_cnt);
    ASSERT_EQ(0, this->m_mqtt_publish_adv_call_cnt);

    this->m_http_async_poll_res = true;
    adv_post_do_async_comm(&adv_post_state);
    ASSERT_FALSE(this->m_flag_gateway_restart_low_memory);
    ASSERT_FALSE(this->m_http_server_mutex_locked);
    ASSERT_FALSE(this->m_leds_notify_http1_data_sent_fail);
    ASSERT_FALSE(this->m_leds_notify_http2_data_sent_fail);
    ASSERT_EQ(0, this->m_http_post_advs_call_cnt);
    ASSERT_EQ(0, this->m_adv_post_statistics_do_send_call_cnt);
    ASSERT_EQ(0, this->m_mqtt_publish_adv_call_cnt);
    this->m_http_async_poll_res = false;

    adv_post_state.flag_need_to_send_statistics    = false;
    adv_post_state.flag_need_to_send_mqtt_periodic = true;
    this->m_mqtt_publish_adv_res                   = true;
    adv_post_do_async_comm(&adv_post_state);
    ASSERT_FALSE(this->m_flag_gateway_restart_low_memory);
    ASSERT_FALSE(this->m_http_server_mutex_locked);
    ASSERT_FALSE(this->m_leds_notify_http1_data_sent_fail);
    ASSERT_FALSE(this->m_leds_notify_http2_data_sent_fail);
    ASSERT_EQ(0, this->m_http_post_advs_call_cnt);
    ASSERT_EQ(0, this->m_adv_post_statistics_do_send_call_cnt);
    ASSERT_EQ(0, this->m_mqtt_publish_adv_call_cnt);

    ASSERT_EQ(0, this->m_adv_post_signals_send_sig);
    ASSERT_TRUE(this->m_adv_post_timers_start_timer_sig_do_async_comm);
    this->m_adv_post_timers_start_timer_sig_do_async_comm = false;
    ASSERT_EQ(0, this->m_mqtt_publish_adv_arg_timestamp);

    ASSERT_TRUE(this->m_mem_alloc_trace.is_empty());
}

TEST_F(TestAdvPostAsyncComm, test_adv_post_statistics_do_send_failed) // NOLINT
{
    this->m_flag_time_is_synchronized              = true;
    this->m_http_server_mutex_try_lock_res         = true;
    this->m_http_post_advs_res                     = true;
    this->m_gw_cfg_get_http_stat_use_http_stat_res = true;
    this->m_gw_cfg_get_mqtt_use_mqtt_res           = false;
    this->m_gw_cfg_get_ntp_use_res                 = true;
    this->m_adv_post_statistics_do_send_res        = false;
    this->m_gw_status_is_mqtt_connected_res        = true;

    adv_post_state_t adv_post_state = {
        .flag_primary_time_sync_is_done  = false,
        .flag_network_connected          = true,
        .flag_async_comm_in_progress     = false,
        .flag_need_to_send_advs1         = false,
        .flag_need_to_send_advs2         = false,
        .flag_need_to_send_statistics    = true,
        .flag_need_to_send_mqtt_periodic = false,
        .flag_relaying_enabled           = true,
        .flag_use_timestamps             = true,
        .flag_stop                       = false,
    };
    this->m_reports = {
        .num_of_advs = 2,
        .table = {
            [0] = {
                      .timestamp = 100500,
                      .samples_counter = 301,
                      .tag_mac = { 0x11, 0x12, 0x13, 0x14, 0x15, 0x16,},
                      .rssi = -49,
                      .data_len = 24,
                      .data_buf = {
                    0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27, 0x28, 0x29, 0x2A, 0x2B, 0x2C, 0x2D, 0x2E, 0x2F,
                    0x30, 0x31, 0x32, 0x33, 0x34, 0x35,
                }
            },
            [1] = {
                      .timestamp = 100501,
                      .samples_counter = 302,
                      .tag_mac = { 0x21, 0x22, 0x23, 0x24, 0x25, 0x26,},
                      .rssi = -48,
                      .data_len = 24,
                      .data_buf = {
                    0x51, 0x52, 0x53, 0x54, 0x55, 0x56, 0x57, 0x58, 0x59, 0x5A, 0x5B, 0x5C, 0x5D, 0x5E, 0x5F,
                    0x50, 0x51, 0x52, 0x53, 0x54, 0x55,
                }
            },
        }
    };

    adv_post_do_async_comm(&adv_post_state);
    ASSERT_FALSE(this->m_flag_gateway_restart_low_memory);
    ASSERT_FALSE(this->m_http_server_mutex_locked);
    ASSERT_FALSE(this->m_leds_notify_http1_data_sent_fail);
    ASSERT_FALSE(this->m_leds_notify_http2_data_sent_fail);
    ASSERT_EQ(0, this->m_http_post_advs_call_cnt);
    ASSERT_EQ(1, this->m_adv_post_statistics_do_send_call_cnt);
    ASSERT_EQ(0, this->m_mqtt_publish_adv_call_cnt);

    this->m_http_async_poll_res = true;
    adv_post_do_async_comm(&adv_post_state);
    ASSERT_FALSE(this->m_flag_gateway_restart_low_memory);
    ASSERT_FALSE(this->m_http_server_mutex_locked);
    ASSERT_FALSE(this->m_leds_notify_http1_data_sent_fail);
    ASSERT_FALSE(this->m_leds_notify_http2_data_sent_fail);
    ASSERT_EQ(0, this->m_http_post_advs_call_cnt);
    ASSERT_EQ(2, this->m_adv_post_statistics_do_send_call_cnt);
    ASSERT_EQ(0, this->m_mqtt_publish_adv_call_cnt);
    this->m_http_async_poll_res = false;

    adv_post_state.flag_need_to_send_statistics    = false;
    adv_post_state.flag_need_to_send_mqtt_periodic = true;
    this->m_mqtt_publish_adv_res                   = true;
    adv_post_do_async_comm(&adv_post_state);
    ASSERT_FALSE(this->m_flag_gateway_restart_low_memory);
    ASSERT_FALSE(this->m_http_server_mutex_locked);
    ASSERT_FALSE(this->m_leds_notify_http1_data_sent_fail);
    ASSERT_FALSE(this->m_leds_notify_http2_data_sent_fail);
    ASSERT_EQ(0, this->m_http_post_advs_call_cnt);
    ASSERT_EQ(2, this->m_adv_post_statistics_do_send_call_cnt);
    ASSERT_EQ(0, this->m_mqtt_publish_adv_call_cnt);

    ASSERT_EQ(0, this->m_adv_post_signals_send_sig);
    ASSERT_TRUE(this->m_adv_post_timers_start_timer_sig_do_async_comm);
    this->m_adv_post_timers_start_timer_sig_do_async_comm = false;
    ASSERT_EQ(0, this->m_mqtt_publish_adv_arg_timestamp);

    ASSERT_TRUE(this->m_mem_alloc_trace.is_empty());
}