#include "TQueue.hpp"
#include "time_task.h"
#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "test_events.hpp"
#include "lwip/apps/sntp.h"
#include "sntp.h"
#include "gtest/gtest.h"
#include <semaphore.h>
#include <stdio.h>
#include "esp_log_wrapper.hpp"
#include "event_mgr.h"

#define CMD_HANDLER_TASK_NAME "cmd_handler"

using namespace std;

typedef enum main_task_cmd_e
{
    MAIN_TASK_CMD_EXIT,
    MAIN_TASK_CMD_TIME_TASK_INIT,
} main_task_cmd_e;

extern "C" {

static struct timespec
timespec_get_clock_monotonic(void)
{
    struct timespec timestamp = {};
    clock_gettime(CLOCK_MONOTONIC, &timestamp);
    return timestamp;
}

static struct timespec
timespec_diff(const struct timespec *p_t2, const struct timespec *p_t1)
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
timespec_diff_ms(const struct timespec *p_t2, const struct timespec *p_t1)
{
    struct timespec diff = timespec_diff(p_t2, p_t1);
    return diff.tv_sec * 1000 + diff.tv_nsec / 1000000;
}

} // extern "C"

/*** Google-test class implementation
 * *********************************************************************************/

static void *
freertos_startup(void *p_arg);

class TestTimeTask : public ::testing::Test
{
private:
protected:
    pthread_t pid;

    void
    SetUp() override
    {
        this->result_time_task_init = false;
        esp_log_wrapper_init();
        sem_init(&semaFreeRTOS, 0, 0);
        const int err = pthread_create(&pid, nullptr, &freertos_startup, this);
        assert(0 == err);
        while (0 != sem_wait(&semaFreeRTOS))
        {
        }
    }

    void
    TearDown() override
    {
        cmdQueue.push_and_wait(MAIN_TASK_CMD_EXIT);
        vTaskEndScheduler();
        void *p_ret_code = nullptr;
        pthread_join(pid, &p_ret_code);
        sem_destroy(&semaFreeRTOS);
        esp_log_wrapper_deinit();
    }

public:
    sem_t                    semaFreeRTOS;
    TQueue<main_task_cmd_e>  cmdQueue;
    std::vector<TestEvent *> testEvents;
    bool                     result_time_task_init;
    time_t                   cur_time;
    sntp_sync_time_cb_t      sntp_sync_time_cb;

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
        return false;
    }
};

static TestTimeTask *gp_obj;

TestTimeTask::TestTimeTask()
    : Test()
    , pid(0)
    , semaFreeRTOS({ 0 })
    , result_time_task_init(false)
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
    gp_obj->testEvents.push_back(new TestEventSntpSetSyncMode(sync_mode));
}

void
sntp_setservername(u8_t idx, const char *p_server)
{
    gp_obj->testEvents.push_back(new TestEventSntpSetServerName(idx, p_server));
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
cmd_handler_task(void *p_param)
{
    auto *p_obj     = static_cast<TestTimeTask *>(p_param);
    bool  flag_exit = false;
    sem_post(&p_obj->semaFreeRTOS);
    while (!flag_exit)
    {
        const main_task_cmd_e cmd = p_obj->cmdQueue.pop();
        switch (cmd)
        {
            case MAIN_TASK_CMD_EXIT:
                flag_exit = true;
                break;
            case MAIN_TASK_CMD_TIME_TASK_INIT:
                p_obj->result_time_task_init = time_task_init();
                break;
            default:
                printf("Error: Unknown cmd %d\n", (int)cmd);
                exit(1);
                break;
        }
        p_obj->cmdQueue.notify_handled();
    }
    vTaskDelete(nullptr);
}

static void *
freertos_startup(void *p_arg)
{
    auto *     p_obj = static_cast<TestTimeTask *>(p_arg);
    BaseType_t res   = xTaskCreate(
        &cmd_handler_task,
        CMD_HANDLER_TASK_NAME,
        configMINIMAL_STACK_SIZE,
        p_obj,
        (tskIDLE_PRIORITY + 1),
        (xTaskHandle *)nullptr);
    assert(pdPASS == res);
    vTaskStartScheduler();
    return nullptr;
}

#define TEST_CHECK_LOG_RECORD_TIME(level_, thread_, msg_) \
    ESP_LOG_WRAPPER_TEST_CHECK_LOG_RECORD_WITH_THREAD("TIME", level_, thread_, msg_);

#define TEST_CHECK_LOG_RECORD_OS_TASK(level_, thread_, msg_) \
    ESP_LOG_WRAPPER_TEST_CHECK_LOG_RECORD_WITH_THREAD("os_task", level_, thread_, msg_);

/*** Unit-Tests
 * *******************************************************************************************************/

TEST_F(TestTimeTask, test_all) // NOLINT
{
    ASSERT_TRUE(event_mgr_init());

    cmdQueue.push_and_wait(MAIN_TASK_CMD_TIME_TASK_INIT);
    ASSERT_TRUE(this->result_time_task_init);
    TEST_CHECK_LOG_RECORD_OS_TASK(
        ESP_LOG_INFO,
        CMD_HANDLER_TASK_NAME,
        "Start thread(static) 'time_task' with priority 1, stack size 3072 bytes");
    TEST_CHECK_LOG_RECORD_TIME(ESP_LOG_INFO, "time_task", "time_task started");
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_NE(nullptr, this->sntp_sync_time_cb);

    {
        const int exp_num_events = 6;
        ASSERT_EQ(exp_num_events, testEvents.size());
        int idx = 0;
        {
            auto *p_base_ev = testEvents[idx++];
            ASSERT_EQ(TestEventType_SNTP_SetOperatingMode, p_base_ev->eventType);
            auto *p_ev = reinterpret_cast<TestEventSntpSetOperatingMode *>(p_base_ev);
            ASSERT_EQ(SNTP_OPMODE_POLL, p_ev->operating_mode);
        }
        {
            auto *p_base_ev = testEvents[idx++];
            ASSERT_EQ(TestEventType_SNTP_SetSyncMode, p_base_ev->eventType);
            auto *p_ev = reinterpret_cast<TestEventSntpSetSyncMode *>(p_base_ev);
            ASSERT_EQ(SNTP_SYNC_MODE_SMOOTH, p_ev->sync_mode);
        }
        {
            auto *p_base_ev = testEvents[idx++];
            ASSERT_EQ(TestEventType_SNTP_SetServerName, p_base_ev->eventType);
            auto *p_ev = reinterpret_cast<TestEventSntpSetServerName *>(p_base_ev);
            ASSERT_EQ(0, p_ev->idx);
            ASSERT_EQ(string("time1.google.com"), p_ev->server);
        }
        {
            auto *p_base_ev = testEvents[idx++];
            ASSERT_EQ(TestEventType_SNTP_SetServerName, p_base_ev->eventType);
            auto *p_ev = reinterpret_cast<TestEventSntpSetServerName *>(p_base_ev);
            ASSERT_EQ(1, p_ev->idx);
            ASSERT_EQ(string("time2.google.com"), p_ev->server);
        }
        {
            auto *p_base_ev = testEvents[idx++];
            ASSERT_EQ(TestEventType_SNTP_SetServerName, p_base_ev->eventType);
            auto *p_ev = reinterpret_cast<TestEventSntpSetServerName *>(p_base_ev);
            ASSERT_EQ(2, p_ev->idx);
            ASSERT_EQ(string("time3.google.com"), p_ev->server);
        }
        {
            auto *p_base_ev = testEvents[idx++];
            ASSERT_EQ(TestEventType_SNTP_SetServerName, p_base_ev->eventType);
            auto *p_ev = reinterpret_cast<TestEventSntpSetServerName *>(p_base_ev);
            ASSERT_EQ(3, p_ev->idx);
            ASSERT_EQ(string("pool.ntp.org"), p_ev->server);
        }
    }
    testEvents.clear();

    event_mgr_notify(EVENT_MGR_EV_WIFI_CONNECTED);
    ASSERT_TRUE(this->wait_for_events());
    TEST_CHECK_LOG_RECORD_TIME(ESP_LOG_INFO, "time_task", "Activate SNTP time synchronization");
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    {
        const int exp_num_events = 1;
        ASSERT_EQ(exp_num_events, testEvents.size());
        int idx = 0;
        {
            auto *p_base_ev = testEvents[idx++];
            ASSERT_EQ(TestEventType_SNTP_Init, p_base_ev->eventType);
        }
    }
    testEvents.clear();

    event_mgr_notify(EVENT_MGR_EV_WIFI_DISCONNECTED);
    ASSERT_TRUE(this->wait_for_events());
    TEST_CHECK_LOG_RECORD_TIME(ESP_LOG_INFO, "time_task", "Deactivate SNTP time synchronization");
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    {
        const int exp_num_events = 1;
        ASSERT_EQ(exp_num_events, testEvents.size());
        int idx = 0;
        {
            auto *p_base_ev = testEvents[idx++];
            ASSERT_EQ(TestEventType_SNTP_Stop, p_base_ev->eventType);
        }
    }
    testEvents.clear();

    event_mgr_notify(EVENT_MGR_EV_ETH_CONNECTED);
    ASSERT_TRUE(this->wait_for_events());
    TEST_CHECK_LOG_RECORD_TIME(ESP_LOG_INFO, "time_task", "Activate SNTP time synchronization");
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    {
        const int exp_num_events = 1;
        ASSERT_EQ(exp_num_events, testEvents.size());
        int idx = 0;
        {
            auto *p_base_ev = testEvents[idx++];
            ASSERT_EQ(TestEventType_SNTP_Init, p_base_ev->eventType);
        }
    }
    testEvents.clear();

    event_mgr_notify(EVENT_MGR_EV_ETH_DISCONNECTED);
    ASSERT_TRUE(this->wait_for_events());
    TEST_CHECK_LOG_RECORD_TIME(ESP_LOG_INFO, "time_task", "Deactivate SNTP time synchronization");
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    {
        const int exp_num_events = 1;
        ASSERT_EQ(exp_num_events, testEvents.size());
        int idx = 0;
        {
            auto *p_base_ev = testEvents[idx++];
            ASSERT_EQ(TestEventType_SNTP_Stop, p_base_ev->eventType);
        }
    }
    testEvents.clear();
}
