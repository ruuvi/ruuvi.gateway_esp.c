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

using namespace std;

typedef enum MainTaskCmd_Tag
{
    MainTaskCmd_Exit,
    MainTaskCmd_TimeTaskInit,
    MainTaskCmd_TimeTaskStartSyncing,
    MainTaskCmd_TimeTaskWaitUntilSyncingComplete,
} MainTaskCmd_e;

/*** Google-test class implementation
 * *********************************************************************************/

static void *
freertosStartup(void *arg);

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
        const int err = pthread_create(&pid, nullptr, &freertosStartup, this);
        assert(0 == err);
        while (0 != sem_wait(&semaFreeRTOS))
        {
        }
    }

    void
    TearDown() override
    {
        cmdQueue.push_and_wait(MainTaskCmd_Exit);
        vTaskEndScheduler();
        void *ret_code = nullptr;
        pthread_join(pid, &ret_code);
        sem_destroy(&semaFreeRTOS);
        esp_log_wrapper_deinit();
    }

public:
    sem_t                    semaFreeRTOS;
    TQueue<MainTaskCmd_e>    cmdQueue;
    std::vector<TestEvent *> testEvents;
    bool                     result_time_task_init;
    time_t                   cur_time;

    TestTimeTask();

    ~TestTimeTask() override;
};

static TestTimeTask *g_pTestTimeTask;

TestTimeTask::TestTimeTask()
    : Test()
    , pid(0)
    , semaFreeRTOS({ 0 })
    , result_time_task_init(false)
    , cur_time(0)
{
    g_pTestTimeTask = this;
}

TestTimeTask::~TestTimeTask()
{
    g_pTestTimeTask = nullptr;
}

extern "C" {

/*** SNTP stub functions
 * *****************************************************************************************/

void
sntp_setoperatingmode(u8_t operating_mode)
{
    g_pTestTimeTask->testEvents.push_back(new TestEventSntpSetOperatingMode(operating_mode));
}

void
sntp_set_sync_mode(sntp_sync_mode_t sync_mode)
{
    g_pTestTimeTask->testEvents.push_back(new TestEventSntpSetSyncMode(sync_mode));
}

void
sntp_setservername(u8_t idx, const char *server)
{
    g_pTestTimeTask->testEvents.push_back(new TestEventSntpSetServerName(idx, server));
}

void
sntp_init(void)
{
    g_pTestTimeTask->testEvents.push_back(new TestEventSntpInit());
}

void
sntp_stop(void)
{
    g_pTestTimeTask->testEvents.push_back(new TestEventSntpStop());
}

/*** os_time stub functions
 * *****************************************************************************************/

time_t
os_time_get(void)
{
    return g_pTestTimeTask->cur_time;
}

void
os_task_delay(const os_delta_ticks_t delay_ticks)
{
    g_pTestTimeTask->testEvents.push_back(new TestEventDelay(delay_ticks));
}

} // extern "C"

/*** Cmd-handler task
 * *************************************************************************************************/

static void
cmdHandlerTask(void *parameters)
{
    auto *pTestTimeTask = static_cast<TestTimeTask *>(parameters);
    bool  flagExit      = false;
    sem_post(&pTestTimeTask->semaFreeRTOS);
    while (!flagExit)
    {
        const MainTaskCmd_e cmd = pTestTimeTask->cmdQueue.pop();
        switch (cmd)
        {
            case MainTaskCmd_Exit:
                flagExit = true;
                break;
            case MainTaskCmd_TimeTaskInit:
                pTestTimeTask->result_time_task_init = time_task_init();
                break;
            case MainTaskCmd_TimeTaskStartSyncing:
                time_task_sync_time();
                break;
            case MainTaskCmd_TimeTaskWaitUntilSyncingComplete:
                time_task_wait_until_syncing_complete();
                break;
            default:
                printf("Error: Unknown cmd %d\n", (int)cmd);
                exit(1);
                break;
        }
        pTestTimeTask->cmdQueue.notify_handled();
    }
    vTaskDelete(nullptr);
}

static void *
freertosStartup(void *arg)
{
    auto *     pObj = static_cast<TestTimeTask *>(arg);
    BaseType_t res  = xTaskCreate(
        &cmdHandlerTask,
        "cmdHandlerTask",
        configMINIMAL_STACK_SIZE,
        pObj,
        (tskIDLE_PRIORITY + 1),
        (xTaskHandle *)nullptr);
    assert(pdPASS == res);
    vTaskStartScheduler();
    return nullptr;
}

#define TEST_CHECK_LOG_RECORD_EX(tag_, level_, msg_, flag_skip_file_info_) \
    do \
    { \
        ASSERT_FALSE(esp_log_wrapper_is_empty()); \
        const LogRecord log_record = esp_log_wrapper_pop(); \
        ASSERT_EQ(level_, log_record.level); \
        ASSERT_EQ(string(tag_), log_record.tag); \
        if (flag_skip_file_info_) \
        { \
            const char *p = strchr(log_record.message.c_str(), ' '); \
            assert(NULL != p); \
            p += 1; \
            p = strchr(p, ' '); \
            assert(NULL != p); \
            p += 1; \
            p = strchr(p, ' '); \
            assert(NULL != p); \
            p += 1; \
            ASSERT_EQ(string(msg_), p); \
        } \
        else \
        { \
            ASSERT_EQ(string(msg_), log_record.message); \
        } \
    } while (0)

#define TEST_CHECK_LOG_RECORD_TIME(level_, msg_)         TEST_CHECK_LOG_RECORD_EX("time", level_, msg_, false);
#define TEST_CHECK_LOG_RECORD_TIME_NO_FILE(level_, msg_) TEST_CHECK_LOG_RECORD_EX("time", level_, msg_, true);

#define TEST_CHECK_LOG_RECORD_OS_TASK(level_, msg_)         TEST_CHECK_LOG_RECORD_EX("os_task", level_, msg_, false);
#define TEST_CHECK_LOG_RECORD_OS_TASK_NO_FILE(level_, msg_) TEST_CHECK_LOG_RECORD_EX("os_task", level_, msg_, true);

/*** Unit-Tests
 * *******************************************************************************************************/

TEST_F(TestTimeTask, test_all) // NOLINT
{
    cmdQueue.push_and_wait(MainTaskCmd_TimeTaskInit);
    ASSERT_TRUE(this->result_time_task_init);
    TEST_CHECK_LOG_RECORD_OS_TASK(
        ESP_LOG_INFO,
        "[cmdHandlerTask] Start thread 'time_task' with priority 1, stack size 3072 bytes");
    TEST_CHECK_LOG_RECORD_TIME(ESP_LOG_INFO, "[time_task] time_task started");
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_EQ(0, testEvents.size());

    struct tm tm_2019_12_31 = {
        .tm_sec   = 59,
        .tm_min   = 59,
        .tm_hour  = 23,
        .tm_mday  = 31,
        .tm_mon   = 12 - 1,
        .tm_year  = 2019 - 1900,
        .tm_wday  = 0,
        .tm_yday  = 0,
        .tm_isdst = -1,
    };
    const time_t timestamp_last_invalid = mktime(&tm_2019_12_31);
    ASSERT_FALSE(time_is_valid(timestamp_last_invalid));
    ASSERT_TRUE(time_is_valid(timestamp_last_invalid + 1));

    struct tm tm_2020_11_30 = {
        .tm_sec   = 0,
        .tm_min   = 0,
        .tm_hour  = 0,
        .tm_mday  = 30,
        .tm_mon   = 11 - 1,
        .tm_year  = 2020 - 1900,
        .tm_wday  = 0,
        .tm_yday  = 0,
        .tm_isdst = -1,
    };
    this->cur_time = mktime(&tm_2020_11_30);

    cmdQueue.push_and_wait(MainTaskCmd_TimeTaskStartSyncing);
    cmdQueue.push_and_wait(MainTaskCmd_TimeTaskWaitUntilSyncingComplete);

    const int exp_num_events = 8;
    ASSERT_EQ(exp_num_events, testEvents.size());
    int idx = 0;
    {
        auto *pBaseEv = testEvents[idx++];
        ASSERT_EQ(TestEventType_SNTP_SetOperatingMode, pBaseEv->eventType);
        auto *pEv = reinterpret_cast<TestEventSntpSetOperatingMode *>(pBaseEv);
        ASSERT_EQ(SNTP_OPMODE_POLL, pEv->operating_mode);
    }
    {
        auto *pBaseEv = testEvents[idx++];
        ASSERT_EQ(TestEventType_SNTP_SetSyncMode, pBaseEv->eventType);
        auto *pEv = reinterpret_cast<TestEventSntpSetSyncMode *>(pBaseEv);
        ASSERT_EQ(SNTP_SYNC_MODE_SMOOTH, pEv->sync_mode);
    }
    {
        auto *pBaseEv = testEvents[idx++];
        ASSERT_EQ(TestEventType_SNTP_SetServerName, pBaseEv->eventType);
        auto *pEv = reinterpret_cast<TestEventSntpSetServerName *>(pBaseEv);
        ASSERT_EQ(0, pEv->idx);
        ASSERT_EQ(string("time1.google.com"), pEv->server);
    }
    {
        auto *pBaseEv = testEvents[idx++];
        ASSERT_EQ(TestEventType_SNTP_SetServerName, pBaseEv->eventType);
        auto *pEv = reinterpret_cast<TestEventSntpSetServerName *>(pBaseEv);
        ASSERT_EQ(1, pEv->idx);
        ASSERT_EQ(string("time2.google.com"), pEv->server);
    }
    {
        auto *pBaseEv = testEvents[idx++];
        ASSERT_EQ(TestEventType_SNTP_SetServerName, pBaseEv->eventType);
        auto *pEv = reinterpret_cast<TestEventSntpSetServerName *>(pBaseEv);
        ASSERT_EQ(2, pEv->idx);
        ASSERT_EQ(string("time3.google.com"), pEv->server);
    }
    {
        auto *pBaseEv = testEvents[idx++];
        ASSERT_EQ(TestEventType_SNTP_SetServerName, pBaseEv->eventType);
        auto *pEv = reinterpret_cast<TestEventSntpSetServerName *>(pBaseEv);
        ASSERT_EQ(3, pEv->idx);
        ASSERT_EQ(string("pool.ntp.org"), pEv->server);
    }
    {
        auto *pBaseEv = testEvents[idx++];
        ASSERT_EQ(TestEventType_SNTP_Init, pBaseEv->eventType);
    }
    {
        auto *pBaseEv = testEvents[idx++];
        ASSERT_EQ(TestEventType_SNTP_Stop, pBaseEv->eventType);
    }
    ASSERT_EQ(exp_num_events, idx);
    TEST_CHECK_LOG_RECORD_TIME(ESP_LOG_INFO, "[time_task] Synchronizing SNTP");
    TEST_CHECK_LOG_RECORD_TIME(ESP_LOG_INFO, "[time_task] SNTP Synchronized: Mon Nov 30 00:00:00 2020");
    ASSERT_TRUE(esp_log_wrapper_is_empty());
}
