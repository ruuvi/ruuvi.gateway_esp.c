#include <cstdio>
#include "TQueue.hpp"
#include "time_task.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "test_events.hpp"
#include "lwip/apps/sntp.h"
#include "sntp.h"
#include "gtest/gtest.h"
#include <semaphore.h>
#include "esp_log_wrapper.hpp"
#include "event_mgr.h"
#include "os_mkgmtime.h"
#include "esp_netif.h"
#include "lwip/ip4_addr.h"
#include "wifi_manager_defs.h"
#include "os_mutex.h"
#include "os_mutex_recursive.h"
#include "gw_cfg.h"
#include "gw_cfg_default.h"

#define CMD_HANDLER_TASK_NAME "cmd_handler"

using namespace std;

typedef enum main_task_cmd_e
{
    MAIN_TASK_CMD_INIT_EVENT_MGR,
    MAIN_TASK_CMD_EVENT_MGR_EV_WIFI_CONNECTED,
    MAIN_TASK_CMD_EVENT_MGR_EV_WIFI_DISCONNECTED,
    MAIN_TASK_EVENT_MGR_EV_ETH_CONNECTED,
    MAIN_TASK_CMD_EVENT_MGR_EV_ETH_DISCONNECTED,
    MAIN_TASK_CMD_CHANGE_NTP_CONFIG1,
    MAIN_TASK_CMD_CHANGE_NTP_CONFIG2,
    MAIN_TASK_CMD_EXIT,
    MAIN_TASK_CMD_TIME_TASK_INIT,
    MAIN_TASK_CMD_TIME_TASK_STOP,
    MAIN_TASK_CMD_SNTP_SYNC_TIME_CB_BAD_TIMESTAMP,
    MAIN_TASK_CMD_SNTP_SYNC_TIME_CB_GOOD_TIMESTAMP,
} main_task_cmd_e;

class TestTimeTask;
static TestTimeTask* gp_obj;

extern "C" {

static struct timespec
timespec_get_clock_monotonic(void)
{
    struct timespec timestamp = {};
    clock_gettime(CLOCK_MONOTONIC, &timestamp);
    return timestamp;
}

static struct timespec
timespec_diff(const struct timespec* p_t2, const struct timespec* p_t1)
{
    struct timespec result = {
        .tv_sec  = p_t2->tv_sec - p_t1->tv_sec,
        .tv_nsec = p_t2->tv_nsec - p_t1->tv_nsec,
    };
    if (result.tv_nsec < 0)
    {
        result.tv_sec -= 1;
        result.tv_nsec += 1000000000;
    }
    return result;
}

static uint32_t
timespec_diff_ms(const struct timespec* p_t2, const struct timespec* p_t1)
{
    struct timespec diff = timespec_diff(p_t2, p_t1);
    return diff.tv_sec * 1000 + diff.tv_nsec / 1000000;
}

} // extern "C"

/*** Google-test class implementation
 * *********************************************************************************/

static void*
freertos_startup(void* p_arg);

class TestTimeTask : public ::testing::Test
{
private:
protected:
    void
    SetUp() override
    {
        this->result_time_task_init = false;
        this->result_time_task_stop = false;
        this->sync_mode             = SNTP_SYNC_MODE_IMMED;
        esp_log_wrapper_init();
        sem_init(&semaFreeRTOS, 0, 0);
        this->pid_test = pthread_self();
        const int err  = pthread_create(&this->pid_freertos, nullptr, &freertos_startup, this);
        assert(0 == err);
        while (0 != sem_wait(&semaFreeRTOS))
        {
        }
    }

    void
    TearDown() override
    {
        cmdQueue.push_and_wait(MAIN_TASK_CMD_EXIT);
        sleep(1);
        vTaskEndScheduler();
        void* p_ret_code = nullptr;
        pthread_join(pid_freertos, &p_ret_code);
        sem_destroy(&semaFreeRTOS);
        esp_log_wrapper_deinit();
    }

public:
    pthread_t               pid_test;
    pthread_t               pid_freertos;
    sem_t                   semaFreeRTOS;
    TQueue<main_task_cmd_e> cmdQueue;
    std::vector<TestEvent*> testEvents;
    bool                    result_time_task_init;
    bool                    result_time_task_stop;
    time_t                  cur_time;
    sntp_sync_time_cb_t     sntp_sync_time_cb;
    sntp_sync_mode_t        sync_mode {};

    TestTimeTask();

    ~TestTimeTask() override;

    bool
    wait_for_events(const uint32_t timeout_ms = 1000U, const uint32_t num_events = 1)
    {
        struct timespec t1 = timespec_get_clock_monotonic();
        struct timespec t2 = t1;
        while (timespec_diff_ms(&t2, &t1) < timeout_ms)
        {
            if (testEvents.size() >= num_events)
            {
                return true;
            }
            sleep(1);
            t2 = timespec_get_clock_monotonic();
        }
        if (testEvents.size() >= num_events)
        {
            return true;
        }
        return false;
    }
};

TestTimeTask::TestTimeTask()
    : Test()
    , pid_test(0)
    , pid_freertos(0)
    , semaFreeRTOS({ 0 })
    , result_time_task_init(false)
    , result_time_task_stop(false)
    , cur_time(0)
    , sntp_sync_time_cb(nullptr)
{
    gp_obj = this;
}

TestTimeTask::~TestTimeTask()
{
    gp_obj = nullptr;
}

extern "C" {

/*** System functions
 * *****************************************************************************************/

os_mutex_recursive_t
os_mutex_recursive_create_static(os_mutex_recursive_static_t* const p_mutex_static)
{
    return (os_mutex_recursive_t)p_mutex_static;
}

void
os_mutex_recursive_delete(os_mutex_recursive_t* const ph_mutex)
{
}

void
os_mutex_recursive_lock(os_mutex_recursive_t const h_mutex)
{
}

void
os_mutex_recursive_unlock(os_mutex_recursive_t const h_mutex)
{
}

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

void
tdd_assert_trap(void)
{
    printf("assert\n");
}

static volatile int32_t g_flagDisableCheckIsThreadFreeRTOS;

void
disableCheckingIfCurThreadIsFreeRTOS(void)
{
    ++g_flagDisableCheckIsThreadFreeRTOS;
}

void
enableCheckingIfCurThreadIsFreeRTOS(void)
{
    --g_flagDisableCheckIsThreadFreeRTOS;
    assert(g_flagDisableCheckIsThreadFreeRTOS >= 0);
}

int
checkIfCurThreadIsFreeRTOS(void)
{
    if (nullptr == gp_obj)
    {
        return false;
    }
    if (g_flagDisableCheckIsThreadFreeRTOS)
    {
        return true;
    }
    const pthread_t cur_thread_pid = pthread_self();
    if (cur_thread_pid == gp_obj->pid_test)
    {
        return false;
    }
    return true;
}

/*** SNTP stub functions
 * *****************************************************************************************/

void
sntp_setoperatingmode(u8_t operating_mode)
{
    gp_obj->testEvents.push_back(new TestEventSntpSetOperatingMode(operating_mode));
}

void
sntp_set_sync_mode(sntp_sync_mode_t sync_mode)
{
    gp_obj->sync_mode = sync_mode;
    gp_obj->testEvents.push_back(new TestEventSntpSetSyncMode(sync_mode));
}

sntp_sync_mode_t
sntp_get_sync_mode(void)
{
    return gp_obj->sync_mode;
}

void
sntp_setservername(u8_t idx, const char* p_server)
{
    gp_obj->testEvents.push_back(new TestEventSntpSetServerName(idx, p_server));
}

void
sntp_setserver(u8_t idx, const ip_addr_t* p_addr)
{
    gp_obj->testEvents.push_back(new TestEventSntpSetServer(idx, p_addr));
}

void
sntp_servermode_dhcp(int set_servers_from_dhcp)
{
    gp_obj->testEvents.push_back(new TestEventSntpSetServerMode(set_servers_from_dhcp));
}

void
sntp_init(void)
{
    gp_obj->testEvents.push_back(new TestEventSntpInit());
}

void
sntp_stop(void)
{
    gp_obj->testEvents.push_back(new TestEventSntpStop());
}

void
sntp_set_time_sync_notification_cb(sntp_sync_time_cb_t p_callback)
{
    gp_obj->sntp_sync_time_cb = p_callback;
}

char*
esp_ip4addr_ntoa(const esp_ip4_addr_t* addr, char* buf, int buflen)
{
    return ip4addr_ntoa_r((ip4_addr_t*)addr, buf, buflen);
}

uint32_t
esp_ip4addr_aton(const char* addr)
{
    return ipaddr_addr(addr);
}

void
wifi_manager_cb_save_wifi_config_sta(const wifiman_config_sta_t* const p_cfg_sta)
{
}

void
main_task_send_sig_reconnect_network(void)
{
    gp_obj->testEvents.push_back(new TestEventNetworkReconnect());
}

/*** os_time stub functions
 * *****************************************************************************************/

time_t
os_time_get(void)
{
    return gp_obj->cur_time;
}

void
os_task_delay(const os_delta_ticks_t delay_ticks)
{
}

} // extern "C"

/*** Cmd-handler task
 * *************************************************************************************************/

static void
cmd_handler_task(void* p_param)
{
    auto* p_obj     = static_cast<TestTimeTask*>(p_param);
    bool  flag_exit = false;

    const gw_cfg_default_init_param_t init_params = {
        .wifi_ap_ssid        = { "" },
        .device_id           = { 0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77 },
        .esp32_fw_ver        = { "v1.10.0" },
        .nrf52_fw_ver        = { "v0.7.1" },
        .nrf52_mac_addr      = { 0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF },
        .esp32_mac_addr_wifi = { 0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0x11 },
        .esp32_mac_addr_eth  = { 0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0x22 },
    };
    gw_cfg_default_init(&init_params, nullptr);
    gw_cfg_init(nullptr);
    esp_log_wrapper_clear();

    sem_post(&p_obj->semaFreeRTOS);
    while (!flag_exit)
    {
        const main_task_cmd_e cmd = p_obj->cmdQueue.pop();
        switch (cmd)
        {
            case MAIN_TASK_CMD_INIT_EVENT_MGR:
                assert(event_mgr_init());
                break;
            case MAIN_TASK_CMD_EVENT_MGR_EV_WIFI_CONNECTED:
                event_mgr_notify(EVENT_MGR_EV_WIFI_CONNECTED);
                break;
            case MAIN_TASK_CMD_EVENT_MGR_EV_WIFI_DISCONNECTED:
                event_mgr_notify(EVENT_MGR_EV_WIFI_DISCONNECTED);
                break;
            case MAIN_TASK_EVENT_MGR_EV_ETH_CONNECTED:
                event_mgr_notify(EVENT_MGR_EV_ETH_CONNECTED);
                break;
            case MAIN_TASK_CMD_EVENT_MGR_EV_ETH_DISCONNECTED:
                event_mgr_notify(EVENT_MGR_EV_ETH_DISCONNECTED);
                break;
            case MAIN_TASK_CMD_CHANGE_NTP_CONFIG1:
            {
                gw_cfg_t gw_cfg = { 0 };
                gw_cfg_default_get(&gw_cfg);
                (void)snprintf(
                    gw_cfg.ruuvi_cfg.ntp.ntp_server1.buf,
                    sizeof(gw_cfg.ruuvi_cfg.ntp.ntp_server1.buf),
                    "time2.google.com");
                (void)snprintf(
                    gw_cfg.ruuvi_cfg.ntp.ntp_server2.buf,
                    sizeof(gw_cfg.ruuvi_cfg.ntp.ntp_server2.buf),
                    "time2.cloudflare.com");
                memset(gw_cfg.ruuvi_cfg.ntp.ntp_server3.buf, 0, sizeof(gw_cfg.ruuvi_cfg.ntp.ntp_server3.buf));
                memset(gw_cfg.ruuvi_cfg.ntp.ntp_server4.buf, 0, sizeof(gw_cfg.ruuvi_cfg.ntp.ntp_server4.buf));
                gw_cfg_update(&gw_cfg);
                event_mgr_notify(EVENT_MGR_EV_GW_CFG_CHANGED_RUUVI);
                break;
            }
            case MAIN_TASK_CMD_CHANGE_NTP_CONFIG2:
            {
                gw_cfg_t gw_cfg = { 0 };
                gw_cfg_default_get(&gw_cfg);
                gw_cfg.ruuvi_cfg.ntp.ntp_use_dhcp = true;
                memset(gw_cfg.ruuvi_cfg.ntp.ntp_server1.buf, 0, sizeof(gw_cfg.ruuvi_cfg.ntp.ntp_server1.buf));
                memset(gw_cfg.ruuvi_cfg.ntp.ntp_server2.buf, 0, sizeof(gw_cfg.ruuvi_cfg.ntp.ntp_server2.buf));
                memset(gw_cfg.ruuvi_cfg.ntp.ntp_server3.buf, 0, sizeof(gw_cfg.ruuvi_cfg.ntp.ntp_server3.buf));
                memset(gw_cfg.ruuvi_cfg.ntp.ntp_server4.buf, 0, sizeof(gw_cfg.ruuvi_cfg.ntp.ntp_server4.buf));
                gw_cfg_update(&gw_cfg);
                event_mgr_notify(EVENT_MGR_EV_GW_CFG_CHANGED_RUUVI);
                break;
            }
            case MAIN_TASK_CMD_EXIT:
                flag_exit = true;
                break;
            case MAIN_TASK_CMD_TIME_TASK_INIT:
                p_obj->result_time_task_init = time_task_init();
                break;
            case MAIN_TASK_CMD_TIME_TASK_STOP:
                p_obj->result_time_task_stop = time_task_stop();
                break;
            case MAIN_TASK_CMD_SNTP_SYNC_TIME_CB_BAD_TIMESTAMP:
            {
                struct tm tm_2020_12_31 = {
                    .tm_sec   = 59,
                    .tm_min   = 59,
                    .tm_hour  = 23,
                    .tm_mday  = 31,
                    .tm_mon   = 11,
                    .tm_year  = 2020 - 1900,
                    .tm_wday  = 0,
                    .tm_yday  = 0,
                    .tm_isdst = -1,
                };
                struct timeval tv = {
                    .tv_sec  = os_mkgmtime(&tm_2020_12_31),
                    .tv_usec = 0,
                };
                gp_obj->sntp_sync_time_cb(&tv);
                break;
            }
            case MAIN_TASK_CMD_SNTP_SYNC_TIME_CB_GOOD_TIMESTAMP:
            {
                struct timeval tv = {
                    .tv_sec  = 1630152456,
                    .tv_usec = 0,
                };
                gp_obj->sntp_sync_time_cb(&tv);
                break;
            }
            default:
                printf("Error: Unknown cmd %d\n", (int)cmd);
                exit(1);
                break;
        }
        p_obj->cmdQueue.notify_handled();
    }
    vTaskDelete(nullptr);
}

static void*
freertos_startup(void* p_arg)
{
    auto* p_obj = static_cast<TestTimeTask*>(p_arg);
    disableCheckingIfCurThreadIsFreeRTOS();
    BaseType_t res = xTaskCreate(
        &cmd_handler_task,
        CMD_HANDLER_TASK_NAME,
        configMINIMAL_STACK_SIZE,
        p_obj,
        (tskIDLE_PRIORITY + 1),
        (xTaskHandle*)nullptr);
    assert(pdPASS == res);
    vTaskStartScheduler();
    return nullptr;
}

#define TEST_CHECK_LOG_RECORD_TIME(level_, thread_, msg_) \
    ESP_LOG_WRAPPER_TEST_CHECK_LOG_RECORD_WITH_THREAD("TIME", level_, thread_, 1, msg_);

#define TEST_CHECK_LOG_RECORD_OS_TASK(level_, thread_, msg_) \
    ESP_LOG_WRAPPER_TEST_CHECK_LOG_RECORD_WITH_THREAD("os_task", level_, thread_, 1, msg_);

/*** Unit-Tests
 * *******************************************************************************************************/

TEST_F(TestTimeTask, test_all) // NOLINT
{
    cmdQueue.push_and_wait(MAIN_TASK_CMD_INIT_EVENT_MGR);

    cmdQueue.push_and_wait(MAIN_TASK_CMD_TIME_TASK_INIT);
    ASSERT_TRUE(this->result_time_task_init);
    TEST_CHECK_LOG_RECORD_TIME(ESP_LOG_INFO, "cmd_handler", "Set time sync mode to IMMED");
    TEST_CHECK_LOG_RECORD_TIME(ESP_LOG_INFO, "cmd_handler", "### Configure SNTP to not use DHCP");
    TEST_CHECK_LOG_RECORD_TIME(ESP_LOG_INFO, "cmd_handler", "Add time server 0: time.google.com");
    TEST_CHECK_LOG_RECORD_TIME(ESP_LOG_INFO, "cmd_handler", "Add time server 1: time.cloudflare.com");
    TEST_CHECK_LOG_RECORD_TIME(ESP_LOG_INFO, "cmd_handler", "Add time server 2: time.nist.gov");
    TEST_CHECK_LOG_RECORD_TIME(ESP_LOG_INFO, "cmd_handler", "Add time server 3: pool.ntp.org");
    TEST_CHECK_LOG_RECORD_OS_TASK(
        ESP_LOG_INFO,
        CMD_HANDLER_TASK_NAME,
        "Start thread(static) 'time_task' with priority 1, stack size 3072 bytes");
    TEST_CHECK_LOG_RECORD_TIME(ESP_LOG_INFO, "time_task", "time_task started");
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_NE(nullptr, this->sntp_sync_time_cb);

    cmdQueue.push_and_wait(MAIN_TASK_CMD_TIME_TASK_INIT);
    ASSERT_FALSE(this->result_time_task_init);
    TEST_CHECK_LOG_RECORD_TIME(ESP_LOG_ERROR, "cmd_handler", "time_task was already initialized");
    ASSERT_TRUE(esp_log_wrapper_is_empty());

    {
        const int exp_num_events = 11;
        ASSERT_EQ(exp_num_events, testEvents.size());
        int idx = 0;
        {
            auto* p_base_ev = testEvents[idx++];
            ASSERT_EQ(TestEventType_SNTP_SetOperatingMode, p_base_ev->eventType);
            auto* p_ev = reinterpret_cast<TestEventSntpSetOperatingMode*>(p_base_ev);
            ASSERT_EQ(SNTP_OPMODE_POLL, p_ev->operating_mode);
        }
        {
            auto* p_base_ev = testEvents[idx++];
            ASSERT_EQ(TestEventType_SNTP_SetSyncMode, p_base_ev->eventType);
            auto* p_ev = reinterpret_cast<TestEventSntpSetSyncMode*>(p_base_ev);
            ASSERT_EQ(SNTP_SYNC_MODE_IMMED, p_ev->sync_mode);
        }
        {
            auto* p_base_ev = testEvents[idx++];
            ASSERT_EQ(TestEventType_SNTP_SetServer, p_base_ev->eventType);
            auto* p_ev = reinterpret_cast<TestEventSntpSetServer*>(p_base_ev);
            ASSERT_EQ(0, p_ev->idx);
            ASSERT_EQ(0, p_ev->addr.u_addr.ip4.addr);
        }
        {
            auto* p_base_ev = testEvents[idx++];
            ASSERT_EQ(TestEventType_SNTP_SetServer, p_base_ev->eventType);
            auto* p_ev = reinterpret_cast<TestEventSntpSetServer*>(p_base_ev);
            ASSERT_EQ(1, p_ev->idx);
            ASSERT_EQ(0, p_ev->addr.u_addr.ip4.addr);
        }
        {
            auto* p_base_ev = testEvents[idx++];
            ASSERT_EQ(TestEventType_SNTP_SetServer, p_base_ev->eventType);
            auto* p_ev = reinterpret_cast<TestEventSntpSetServer*>(p_base_ev);
            ASSERT_EQ(2, p_ev->idx);
            ASSERT_EQ(0, p_ev->addr.u_addr.ip4.addr);
        }
        {
            auto* p_base_ev = testEvents[idx++];
            ASSERT_EQ(TestEventType_SNTP_SetServer, p_base_ev->eventType);
            auto* p_ev = reinterpret_cast<TestEventSntpSetServer*>(p_base_ev);
            ASSERT_EQ(3, p_ev->idx);
            ASSERT_EQ(0, p_ev->addr.u_addr.ip4.addr);
        }
        {
            auto* p_base_ev = testEvents[idx++];
            ASSERT_EQ(TestEventType_SNTP_SetServerMode, p_base_ev->eventType);
            auto* p_ev = reinterpret_cast<TestEventSntpSetServerMode*>(p_base_ev);
            ASSERT_EQ(0, p_ev->set_servers_from_dhcp);
        }
        {
            auto* p_base_ev = testEvents[idx++];
            ASSERT_EQ(TestEventType_SNTP_SetServerName, p_base_ev->eventType);
            auto* p_ev = reinterpret_cast<TestEventSntpSetServerName*>(p_base_ev);
            ASSERT_EQ(0, p_ev->idx);
            ASSERT_EQ(string("time.google.com"), p_ev->server);
        }
        {
            auto* p_base_ev = testEvents[idx++];
            ASSERT_EQ(TestEventType_SNTP_SetServerName, p_base_ev->eventType);
            auto* p_ev = reinterpret_cast<TestEventSntpSetServerName*>(p_base_ev);
            ASSERT_EQ(1, p_ev->idx);
            ASSERT_EQ(string("time.cloudflare.com"), p_ev->server);
        }
        {
            auto* p_base_ev = testEvents[idx++];
            ASSERT_EQ(TestEventType_SNTP_SetServerName, p_base_ev->eventType);
            auto* p_ev = reinterpret_cast<TestEventSntpSetServerName*>(p_base_ev);
            ASSERT_EQ(2, p_ev->idx);
            ASSERT_EQ(string("time.nist.gov"), p_ev->server);
        }
        {
            auto* p_base_ev = testEvents[idx++];
            ASSERT_EQ(TestEventType_SNTP_SetServerName, p_base_ev->eventType);
            auto* p_ev = reinterpret_cast<TestEventSntpSetServerName*>(p_base_ev);
            ASSERT_EQ(3, p_ev->idx);
            ASSERT_EQ(string("pool.ntp.org"), p_ev->server);
        }
        ASSERT_EQ(exp_num_events, idx);
    }
    testEvents.clear();

    cmdQueue.push_and_wait(MAIN_TASK_CMD_EVENT_MGR_EV_WIFI_CONNECTED);
    ASSERT_TRUE(this->wait_for_events());
    TEST_CHECK_LOG_RECORD_TIME(ESP_LOG_INFO, "time_task", "### Activate SNTP time synchronization");
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    {
        const int exp_num_events = 1;
        ASSERT_EQ(exp_num_events, testEvents.size());
        int idx = 0;
        {
            auto* p_base_ev = testEvents[idx++];
            ASSERT_EQ(TestEventType_SNTP_Init, p_base_ev->eventType);
        }
    }
    testEvents.clear();

    ASSERT_NE(nullptr, gp_obj->sntp_sync_time_cb);
    cmdQueue.push_and_wait(MAIN_TASK_CMD_SNTP_SYNC_TIME_CB_BAD_TIMESTAMP);
    ASSERT_EQ(0, testEvents.size());
    testEvents.clear();
    TEST_CHECK_LOG_RECORD_TIME(
        ESP_LOG_WARN,
        "cmd_handler",
        "### Time has been synchronized but timestamp is bad: 2020-12-31 23:59:59.000");
    ASSERT_TRUE(esp_log_wrapper_is_empty());

    ASSERT_NE(nullptr, gp_obj->sntp_sync_time_cb);
    cmdQueue.push_and_wait(MAIN_TASK_CMD_SNTP_SYNC_TIME_CB_GOOD_TIMESTAMP);
    ASSERT_EQ(1, testEvents.size());
    {
        auto* p_base_ev = testEvents[0];
        ASSERT_EQ(TestEventType_SNTP_SetSyncMode, p_base_ev->eventType);
        auto* p_ev = reinterpret_cast<TestEventSntpSetSyncMode*>(p_base_ev);
        ASSERT_EQ(SNTP_SYNC_MODE_SMOOTH, p_ev->sync_mode);
    }
    testEvents.clear();
    TEST_CHECK_LOG_RECORD_TIME(ESP_LOG_INFO, "cmd_handler", "### Time has been synchronized: 2021-08-28 12:07:36.000");
    TEST_CHECK_LOG_RECORD_TIME(ESP_LOG_INFO, "cmd_handler", "Switch time sync mode to SMOOTH");
    ASSERT_TRUE(esp_log_wrapper_is_empty());

    cmdQueue.push_and_wait(MAIN_TASK_CMD_EVENT_MGR_EV_WIFI_DISCONNECTED);
    ASSERT_TRUE(this->wait_for_events());
    TEST_CHECK_LOG_RECORD_TIME(ESP_LOG_INFO, "time_task", "### Deactivate SNTP time synchronization");
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    {
        const int exp_num_events = 1;
        ASSERT_EQ(exp_num_events, testEvents.size());
        int idx = 0;
        {
            auto* p_base_ev = testEvents[idx++];
            ASSERT_EQ(TestEventType_SNTP_Stop, p_base_ev->eventType);
        }
        ASSERT_EQ(exp_num_events, idx);
    }
    testEvents.clear();

    cmdQueue.push_and_wait(MAIN_TASK_EVENT_MGR_EV_ETH_CONNECTED);
    ASSERT_TRUE(this->wait_for_events());
    TEST_CHECK_LOG_RECORD_TIME(ESP_LOG_INFO, "time_task", "### Activate SNTP time synchronization");
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    {
        const int exp_num_events = 1;
        ASSERT_EQ(exp_num_events, testEvents.size());
        int idx = 0;
        {
            auto* p_base_ev = testEvents[idx++];
            ASSERT_EQ(TestEventType_SNTP_Init, p_base_ev->eventType);
        }
        ASSERT_EQ(exp_num_events, idx);
    }
    testEvents.clear();

    cmdQueue.push_and_wait(MAIN_TASK_CMD_EVENT_MGR_EV_ETH_DISCONNECTED);
    ASSERT_TRUE(this->wait_for_events());
    TEST_CHECK_LOG_RECORD_TIME(ESP_LOG_INFO, "time_task", "### Deactivate SNTP time synchronization");
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    {
        const int exp_num_events = 1;
        ASSERT_EQ(exp_num_events, testEvents.size());
        int idx = 0;
        {
            auto* p_base_ev = testEvents[idx++];
            ASSERT_EQ(TestEventType_SNTP_Stop, p_base_ev->eventType);
        }
        ASSERT_EQ(exp_num_events, idx);
    }
    testEvents.clear();

    cmdQueue.push_and_wait(MAIN_TASK_CMD_CHANGE_NTP_CONFIG1);
    {
        const int exp_num_events = 11;
        ASSERT_TRUE(this->wait_for_events(1000, exp_num_events));
        TEST_CHECK_LOG_RECORD_TIME(ESP_LOG_INFO, "time_task", "Got TIME_TASK_SIG_GW_CFG_CHANGED_RUUVI");
        TEST_CHECK_LOG_RECORD_TIME(ESP_LOG_INFO, "time_task", "### Deactivate SNTP time synchronization");
        TEST_CHECK_LOG_RECORD_TIME(ESP_LOG_INFO, "time_task", "Reconfigure SNTP");
        TEST_CHECK_LOG_RECORD_TIME(ESP_LOG_INFO, "time_task", "### Configure SNTP to not use DHCP");
        TEST_CHECK_LOG_RECORD_TIME(ESP_LOG_INFO, "time_task", "Add time server 0: time2.google.com");
        TEST_CHECK_LOG_RECORD_TIME(ESP_LOG_INFO, "time_task", "Add time server 1: time2.cloudflare.com");
        TEST_CHECK_LOG_RECORD_TIME(ESP_LOG_INFO, "time_task", "Add time server 2: NULL");
        TEST_CHECK_LOG_RECORD_TIME(ESP_LOG_INFO, "time_task", "Add time server 3: NULL");
        TEST_CHECK_LOG_RECORD_TIME(ESP_LOG_INFO, "time_task", "### Activate SNTP time synchronization");
        ASSERT_TRUE(esp_log_wrapper_is_empty());

        ASSERT_EQ(exp_num_events, testEvents.size());
        int idx = 0;
        {
            auto* p_base_ev = testEvents[idx++];
            ASSERT_EQ(TestEventType_SNTP_Stop, p_base_ev->eventType);
        }
        {
            auto* p_base_ev = testEvents[idx++];
            ASSERT_EQ(TestEventType_SNTP_SetServer, p_base_ev->eventType);
            auto* p_ev = reinterpret_cast<TestEventSntpSetServer*>(p_base_ev);
            ASSERT_EQ(0, p_ev->idx);
            ASSERT_EQ(0, p_ev->addr.u_addr.ip4.addr);
        }
        {
            auto* p_base_ev = testEvents[idx++];
            ASSERT_EQ(TestEventType_SNTP_SetServer, p_base_ev->eventType);
            auto* p_ev = reinterpret_cast<TestEventSntpSetServer*>(p_base_ev);
            ASSERT_EQ(1, p_ev->idx);
            ASSERT_EQ(0, p_ev->addr.u_addr.ip4.addr);
        }
        {
            auto* p_base_ev = testEvents[idx++];
            ASSERT_EQ(TestEventType_SNTP_SetServer, p_base_ev->eventType);
            auto* p_ev = reinterpret_cast<TestEventSntpSetServer*>(p_base_ev);
            ASSERT_EQ(2, p_ev->idx);
            ASSERT_EQ(0, p_ev->addr.u_addr.ip4.addr);
        }
        {
            auto* p_base_ev = testEvents[idx++];
            ASSERT_EQ(TestEventType_SNTP_SetServer, p_base_ev->eventType);
            auto* p_ev = reinterpret_cast<TestEventSntpSetServer*>(p_base_ev);
            ASSERT_EQ(3, p_ev->idx);
            ASSERT_EQ(0, p_ev->addr.u_addr.ip4.addr);
        }
        {
            auto* p_base_ev = testEvents[idx++];
            ASSERT_EQ(TestEventType_SNTP_SetServerMode, p_base_ev->eventType);
            auto* p_ev = reinterpret_cast<TestEventSntpSetServerMode*>(p_base_ev);
            ASSERT_EQ(0, p_ev->set_servers_from_dhcp);
        }
        {
            auto* p_base_ev = testEvents[idx++];
            ASSERT_EQ(TestEventType_SNTP_SetServerName, p_base_ev->eventType);
            auto* p_ev = reinterpret_cast<TestEventSntpSetServerName*>(p_base_ev);
            ASSERT_EQ(0, p_ev->idx);
            ASSERT_EQ(string("time2.google.com"), p_ev->server);
        }
        {
            auto* p_base_ev = testEvents[idx++];
            ASSERT_EQ(TestEventType_SNTP_SetServerName, p_base_ev->eventType);
            auto* p_ev = reinterpret_cast<TestEventSntpSetServerName*>(p_base_ev);
            ASSERT_EQ(1, p_ev->idx);
            ASSERT_EQ(string("time2.cloudflare.com"), p_ev->server);
        }
        {
            auto* p_base_ev = testEvents[idx++];
            ASSERT_EQ(TestEventType_SNTP_SetServer, p_base_ev->eventType);
            auto* p_ev = reinterpret_cast<TestEventSntpSetServer*>(p_base_ev);
            ASSERT_EQ(2, p_ev->idx);
            ASSERT_EQ(0, p_ev->addr.u_addr.ip4.addr);
        }
        {
            auto* p_base_ev = testEvents[idx++];
            ASSERT_EQ(TestEventType_SNTP_SetServer, p_base_ev->eventType);
            auto* p_ev = reinterpret_cast<TestEventSntpSetServer*>(p_base_ev);
            ASSERT_EQ(3, p_ev->idx);
            ASSERT_EQ(0, p_ev->addr.u_addr.ip4.addr);
        }
        {
            auto* p_base_ev = testEvents[idx++];
            ASSERT_EQ(TestEventType_SNTP_Init, p_base_ev->eventType);
            auto* p_ev = reinterpret_cast<TestEventSntpInit*>(p_base_ev);
        }
        ASSERT_EQ(exp_num_events, idx);
    }
    testEvents.clear();

    cmdQueue.push_and_wait(MAIN_TASK_CMD_CHANGE_NTP_CONFIG2);
    {
        const int exp_num_events = 8;
        ASSERT_TRUE(this->wait_for_events(1000, exp_num_events));
        TEST_CHECK_LOG_RECORD_TIME(ESP_LOG_INFO, "time_task", "Got TIME_TASK_SIG_GW_CFG_CHANGED_RUUVI");
        TEST_CHECK_LOG_RECORD_TIME(ESP_LOG_INFO, "time_task", "### Deactivate SNTP time synchronization");
        TEST_CHECK_LOG_RECORD_TIME(ESP_LOG_INFO, "time_task", "Reconfigure SNTP");
        TEST_CHECK_LOG_RECORD_TIME(ESP_LOG_INFO, "time_task", "### Configure SNTP to use DHCP");
        TEST_CHECK_LOG_RECORD_TIME(ESP_LOG_INFO, "time_task", "### Activate SNTP time synchronization");
        ASSERT_TRUE(esp_log_wrapper_is_empty());

        ASSERT_EQ(exp_num_events, testEvents.size());
        int idx = 0;
        {
            auto* p_base_ev = testEvents[idx++];
            ASSERT_EQ(TestEventType_SNTP_Stop, p_base_ev->eventType);
        }
        {
            auto* p_base_ev = testEvents[idx++];
            ASSERT_EQ(TestEventType_SNTP_SetServer, p_base_ev->eventType);
            auto* p_ev = reinterpret_cast<TestEventSntpSetServer*>(p_base_ev);
            ASSERT_EQ(0, p_ev->idx);
            ASSERT_EQ(0, p_ev->addr.u_addr.ip4.addr);
        }
        {
            auto* p_base_ev = testEvents[idx++];
            ASSERT_EQ(TestEventType_SNTP_SetServer, p_base_ev->eventType);
            auto* p_ev = reinterpret_cast<TestEventSntpSetServer*>(p_base_ev);
            ASSERT_EQ(1, p_ev->idx);
            ASSERT_EQ(0, p_ev->addr.u_addr.ip4.addr);
        }
        {
            auto* p_base_ev = testEvents[idx++];
            ASSERT_EQ(TestEventType_SNTP_SetServer, p_base_ev->eventType);
            auto* p_ev = reinterpret_cast<TestEventSntpSetServer*>(p_base_ev);
            ASSERT_EQ(2, p_ev->idx);
            ASSERT_EQ(0, p_ev->addr.u_addr.ip4.addr);
        }
        {
            auto* p_base_ev = testEvents[idx++];
            ASSERT_EQ(TestEventType_SNTP_SetServer, p_base_ev->eventType);
            auto* p_ev = reinterpret_cast<TestEventSntpSetServer*>(p_base_ev);
            ASSERT_EQ(3, p_ev->idx);
            ASSERT_EQ(0, p_ev->addr.u_addr.ip4.addr);
        }
        {
            auto* p_base_ev = testEvents[idx++];
            ASSERT_EQ(TestEventType_SNTP_SetServerMode, p_base_ev->eventType);
            auto* p_ev = reinterpret_cast<TestEventSntpSetServerMode*>(p_base_ev);
            ASSERT_EQ(1, p_ev->set_servers_from_dhcp);
        }
        {
            auto* p_base_ev = testEvents[idx++];
            ASSERT_EQ(TestEventType_NetworkReconnect, p_base_ev->eventType);
            auto* p_ev = reinterpret_cast<TestEventNetworkReconnect*>(p_base_ev);
        }
        {
            auto* p_base_ev = testEvents[idx++];
            ASSERT_EQ(TestEventType_SNTP_Init, p_base_ev->eventType);
            auto* p_ev = reinterpret_cast<TestEventSntpInit*>(p_base_ev);
        }
        ASSERT_EQ(exp_num_events, idx);
    }
    testEvents.clear();

    cmdQueue.push_and_wait(MAIN_TASK_CMD_TIME_TASK_STOP);
    ASSERT_TRUE(this->result_time_task_stop);
    TEST_CHECK_LOG_RECORD_TIME(ESP_LOG_INFO, "time_task", "Stop time_task");
    ASSERT_TRUE(esp_log_wrapper_is_empty());

    cmdQueue.push_and_wait(MAIN_TASK_CMD_TIME_TASK_STOP);
    ASSERT_FALSE(this->result_time_task_stop);
    TEST_CHECK_LOG_RECORD_TIME(ESP_LOG_ERROR, "cmd_handler", "Can't send signal to stop thread");
    ASSERT_TRUE(esp_log_wrapper_is_empty());
}
