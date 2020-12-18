/**
 * @file test_os_signal_freertos.cpp
 * @author TheSomeMan
 * @date 2020-12-06
 * @copyright Ruuvi Innovations Ltd, license BSD-3-Clause.
 */

#include <string>
#include <semaphore.h>
#include "gtest/gtest.h"
#include "os_signal.h"
#include "os_task.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "TQueue.hpp"
#include "esp_log_wrapper.hpp"
#include "test_events.hpp"
#include "event_mgr.h"

using namespace std;

typedef enum MainTaskCmd_Tag
{
    MainTaskCmd_Exit,
    MainTaskCmd_RunSignalHandlerTask1,
    MainTaskCmd_RunSignalHandlerTask2,
    MainTaskCmd_SendToTask1Signal0,
    MainTaskCmd_SendToTask1Signal1,
    MainTaskCmd_SendToTask1Signal2,
    MainTaskCmd_SendToTask1Signal3,
    MainTaskCmd_SendToTask2Signal0,
    MainTaskCmd_SendToTask2Signal1,
    MainTaskCmd_SendToTask2Signal2,
    MainTaskCmd_SendToTask2Signal3,
} MainTaskCmd_e;

/*** Google-test class implementation
 * *********************************************************************************/

class TestEventMgr;
static TestEventMgr *g_pTestClass;

static void *
freertosStartup(void *arg);

class TestEventMgr : public ::testing::Test
{
private:
protected:
    pthread_t pid;

    void
    SetUp() override
    {
        esp_log_wrapper_init();
        sem_init(&semaFreeRTOS, 0, 0);
        const int err = pthread_create(&pid, nullptr, &freertosStartup, this);
        assert(0 == err);
        while (0 != sem_wait(&semaFreeRTOS))
        {
        }
        g_pTestClass = this;
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
        g_pTestClass = nullptr;
    }

public:
    sem_t                    semaFreeRTOS;
    TQueue<MainTaskCmd_e>    cmdQueue;
    std::vector<TestEvent *> testEvents;
    os_signal_t *            p_signal;
    os_signal_t *            p_signal2;
    bool                     result_run_signal_handler_task;

    TestEventMgr();

    ~TestEventMgr() override;

    bool
    wait_until_thread1_registered(const uint32_t timeout_ms) const;

    bool
    wait_until_thread2_registered(const uint32_t timeout_ms) const;

    bool
    wait_until_new_events_pushed(const uint32_t exp_num_events, const uint32_t timeout_ms) const;
};

TestEventMgr::TestEventMgr()
    : Test()
    , p_signal(nullptr)
    , p_signal2(nullptr)
    , result_run_signal_handler_task(false)
{
}

TestEventMgr::~TestEventMgr() = default;

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

bool
TestEventMgr::wait_until_thread1_registered(const uint32_t timeout_ms) const
{
    struct timespec t1 = timespec_get_clock_monotonic();
    struct timespec t2 = t1;
    while (timespec_diff_ms(&t2, &t1) < timeout_ms)
    {
        if (nullptr != this->p_signal)
        {
            if (os_signal_is_any_thread_registered(this->p_signal))
            {
                return true;
            }
        }
        sleep(1);
        t2 = timespec_get_clock_monotonic();
    }
    return false;
}

bool
TestEventMgr::wait_until_thread2_registered(const uint32_t timeout_ms) const
{
    struct timespec t1 = timespec_get_clock_monotonic();
    struct timespec t2 = t1;
    while (timespec_diff_ms(&t2, &t1) < timeout_ms)
    {
        if (nullptr != this->p_signal2)
        {
            if (os_signal_is_any_thread_registered(this->p_signal2))
            {
                return true;
            }
        }
        sleep(1);
        t2 = timespec_get_clock_monotonic();
    }
    return false;
}

bool
TestEventMgr::wait_until_new_events_pushed(const uint32_t exp_num_events, const uint32_t timeout_ms) const
{
    struct timespec t1 = timespec_get_clock_monotonic();
    struct timespec t2 = t1;
    while (timespec_diff_ms(&t2, &t1) < timeout_ms)
    {
        if (exp_num_events == this->testEvents.size())
        {
            return true;
        }
        sleep(1);
        t2 = timespec_get_clock_monotonic();
    }
    return false;
}

ATTR_NORETURN
static void
signalHandlerTask1(void *p_param)
{
    auto *       pObj     = static_cast<TestEventMgr *>(p_param);
    os_signal_t *p_signal = os_signal_create();
    assert(nullptr != p_signal);
    pObj->p_signal = p_signal;
    if (!os_signal_add(p_signal, OS_SIGNAL_NUM_0))
    {
        assert(0);
    }
    if (!os_signal_add(p_signal, OS_SIGNAL_NUM_1))
    {
        assert(0);
    }
    if (!os_signal_add(p_signal, OS_SIGNAL_NUM_2))
    {
        assert(0);
    }
    if (!event_mgr_subscribe_sig(EVENT_MGR_EV_WIFI_CONNECTED, p_signal, OS_SIGNAL_NUM_0))
    {
        assert(0);
    }
    static event_mgr_ev_info_static_t ev_info_mem;
    event_mgr_subscribe_sig_static(&ev_info_mem, EVENT_MGR_EV_WIFI_DISCONNECTED, p_signal, OS_SIGNAL_NUM_1);

    os_signal_register_cur_thread(p_signal);
    bool flag_exit = false;
    while (!flag_exit)
    {
        os_signal_events_t sig_events = {};
        os_signal_wait(p_signal, &sig_events);
        for (;;)
        {
            os_signal_num_e sig_num = os_signal_num_get_next(&sig_events);
            if (OS_SIGNAL_NUM_NONE == sig_num)
            {
                break;
            }
            pObj->testEvents.push_back(new TestEventSignal(TEST_EVENT_THREAD_NUM_1, sig_num));
            if (OS_SIGNAL_NUM_2 == sig_num)
            {
                flag_exit = true;
                break;
            }
        }
    }
    p_signal = nullptr;
    os_signal_delete(&pObj->p_signal);
    pObj->testEvents.push_back(new TestEventThreadExit(TEST_EVENT_THREAD_NUM_1));
    vTaskDelete(nullptr);
    for (;;)
    {
        vTaskDelay(1);
    }
}

ATTR_NORETURN
static void
signalHandlerTask2(void *p_param)
{
    auto *                    pObj       = static_cast<TestEventMgr *>(p_param);
    static os_signal_static_t signal_mem = {};
    os_signal_t *             p_signal   = os_signal_create_static(&signal_mem);
    assert(reinterpret_cast<void *>(&signal_mem) == reinterpret_cast<void *>(p_signal));
    pObj->p_signal2 = p_signal;
    if (!os_signal_add(p_signal, OS_SIGNAL_NUM_0))
    {
        assert(0);
    }
    if (!os_signal_add(p_signal, OS_SIGNAL_NUM_1))
    {
        assert(0);
    }
    if (!os_signal_add(p_signal, OS_SIGNAL_NUM_2))
    {
        assert(0);
    }
    if (!event_mgr_subscribe_sig(EVENT_MGR_EV_ETH_CONNECTED, p_signal, OS_SIGNAL_NUM_0))
    {
        assert(0);
    }
    static event_mgr_ev_info_static_t ev_info_mem;
    event_mgr_subscribe_sig_static(&ev_info_mem, EVENT_MGR_EV_ETH_DISCONNECTED, p_signal, OS_SIGNAL_NUM_1);
    os_signal_register_cur_thread(p_signal);
    bool flag_exit = false;
    while (!flag_exit)
    {
        os_signal_events_t sig_events = {};
        os_signal_wait(p_signal, &sig_events);
        for (;;)
        {
            os_signal_num_e sig_num = os_signal_num_get_next(&sig_events);
            if (OS_SIGNAL_NUM_NONE == sig_num)
            {
                break;
            }
            pObj->testEvents.push_back(new TestEventSignal(TEST_EVENT_THREAD_NUM_2, sig_num));
            if (OS_SIGNAL_NUM_2 == sig_num)
            {
                flag_exit = true;
                break;
            }
        }
    }
    p_signal = nullptr;
    os_signal_delete(&pObj->p_signal2);
    pObj->testEvents.push_back(new TestEventThreadExit(TEST_EVENT_THREAD_NUM_2));
    vTaskDelete(nullptr);
    for (;;)
    {
        vTaskDelay(1);
    }
}

static void
cmdHandlerTask(void *p_param)
{
    auto *pObj     = static_cast<TestEventMgr *>(p_param);
    bool  flagExit = false;
    sem_post(&pObj->semaFreeRTOS);
    while (!flagExit)
    {
        const MainTaskCmd_e cmd = pObj->cmdQueue.pop();
        switch (cmd)
        {
            case MainTaskCmd_Exit:
                flagExit = true;
                break;
            case MainTaskCmd_RunSignalHandlerTask1:
            {
                os_task_handle_t h_task              = nullptr;
                pObj->result_run_signal_handler_task = os_task_create(
                    &signalHandlerTask1,
                    "SignalHandler1",
                    configMINIMAL_STACK_SIZE,
                    pObj,
                    tskIDLE_PRIORITY + 1,
                    &h_task);
                break;
            }
            case MainTaskCmd_RunSignalHandlerTask2:
            {
                os_task_handle_t h_task              = nullptr;
                pObj->result_run_signal_handler_task = os_task_create(
                    &signalHandlerTask2,
                    "SignalHandler2",
                    configMINIMAL_STACK_SIZE,
                    pObj,
                    tskIDLE_PRIORITY + 1,
                    &h_task);
                break;
            }
            case MainTaskCmd_SendToTask1Signal0:
                os_signal_send(pObj->p_signal, OS_SIGNAL_NUM_0);
                break;
            case MainTaskCmd_SendToTask1Signal1:
                os_signal_send(pObj->p_signal, OS_SIGNAL_NUM_1);
                break;
            case MainTaskCmd_SendToTask1Signal2:
                os_signal_send(pObj->p_signal, OS_SIGNAL_NUM_2);
                break;
            case MainTaskCmd_SendToTask1Signal3:
                os_signal_send(pObj->p_signal, OS_SIGNAL_NUM_3);
                break;
            case MainTaskCmd_SendToTask2Signal0:
                os_signal_send(pObj->p_signal2, OS_SIGNAL_NUM_0);
                break;
            case MainTaskCmd_SendToTask2Signal1:
                os_signal_send(pObj->p_signal2, OS_SIGNAL_NUM_1);
                break;
            case MainTaskCmd_SendToTask2Signal2:
                os_signal_send(pObj->p_signal2, OS_SIGNAL_NUM_2);
                break;
            case MainTaskCmd_SendToTask2Signal3:
                os_signal_send(pObj->p_signal2, OS_SIGNAL_NUM_3);
                break;
            default:
                printf("Error: Unknown cmd %d\n", (int)cmd);
                exit(1);
                break;
        }
        pObj->cmdQueue.notify_handled();
    }
    vTaskDelete(nullptr);
}

static void *
freertosStartup(void *arg)
{
    auto *     pObj = static_cast<TestEventMgr *>(arg);
    const bool res
        = xTaskCreate(&cmdHandlerTask, "cmdHandlerTask", configMINIMAL_STACK_SIZE, pObj, tskIDLE_PRIORITY + 1, nullptr);
    assert(res);
    vTaskStartScheduler();
    return nullptr;
}

/*** Unit-Tests
 * *******************************************************************************************************/

TEST_F(TestEventMgr, test1) // NOLINT
{
    ASSERT_TRUE(event_mgr_init());

    this->result_run_signal_handler_task = false;
    cmdQueue.push_and_wait(MainTaskCmd_RunSignalHandlerTask1);
    ASSERT_TRUE(this->result_run_signal_handler_task);
    ASSERT_TRUE(wait_until_thread1_registered(1000));

    this->result_run_signal_handler_task = false;
    cmdQueue.push_and_wait(MainTaskCmd_RunSignalHandlerTask2);
    ASSERT_TRUE(this->result_run_signal_handler_task);
    ASSERT_TRUE(wait_until_thread2_registered(1000));

    event_mgr_notify(EVENT_MGR_EV_WIFI_CONNECTED);
    ASSERT_TRUE(wait_until_new_events_pushed(1, 1000));
    {
        auto *pBaseEv = testEvents[0];
        ASSERT_EQ(TestEventType_Signal, pBaseEv->eventType);
        auto *pEv = reinterpret_cast<TestEventSignal *>(pBaseEv);
        ASSERT_EQ(TEST_EVENT_THREAD_NUM_1, pEv->thread_num);
        ASSERT_EQ(OS_SIGNAL_NUM_0, pEv->sig_num);
    }
    testEvents.clear();

    event_mgr_notify(EVENT_MGR_EV_WIFI_DISCONNECTED);
    ASSERT_TRUE(wait_until_new_events_pushed(1, 1000));
    {
        auto *pBaseEv = testEvents[0];
        ASSERT_EQ(TestEventType_Signal, pBaseEv->eventType);
        auto *pEv = reinterpret_cast<TestEventSignal *>(pBaseEv);
        ASSERT_EQ(TEST_EVENT_THREAD_NUM_1, pEv->thread_num);
        ASSERT_EQ(OS_SIGNAL_NUM_1, pEv->sig_num);
    }
    testEvents.clear();

    event_mgr_notify(EVENT_MGR_EV_ETH_CONNECTED);
    ASSERT_TRUE(wait_until_new_events_pushed(1, 1000));
    {
        auto *pBaseEv = testEvents[0];
        ASSERT_EQ(TestEventType_Signal, pBaseEv->eventType);
        auto *pEv = reinterpret_cast<TestEventSignal *>(pBaseEv);
        ASSERT_EQ(TEST_EVENT_THREAD_NUM_2, pEv->thread_num);
        ASSERT_EQ(OS_SIGNAL_NUM_0, pEv->sig_num);
    }
    testEvents.clear();

    event_mgr_notify(EVENT_MGR_EV_ETH_DISCONNECTED);
    ASSERT_TRUE(wait_until_new_events_pushed(1, 1000));
    {
        auto *pBaseEv = testEvents[0];
        ASSERT_EQ(TestEventType_Signal, pBaseEv->eventType);
        auto *pEv = reinterpret_cast<TestEventSignal *>(pBaseEv);
        ASSERT_EQ(TEST_EVENT_THREAD_NUM_2, pEv->thread_num);
        ASSERT_EQ(OS_SIGNAL_NUM_1, pEv->sig_num);
    }
    testEvents.clear();

    cmdQueue.push_and_wait(MainTaskCmd_SendToTask1Signal2);
    ASSERT_TRUE(wait_until_new_events_pushed(2, 1000));
    {
        auto *pBaseEv = testEvents[0];
        ASSERT_EQ(TestEventType_Signal, pBaseEv->eventType);
        auto *pEv = reinterpret_cast<TestEventSignal *>(pBaseEv);
        ASSERT_EQ(OS_SIGNAL_NUM_2, pEv->sig_num);
    }
    {
        auto *pBaseEv = testEvents[1];
        ASSERT_EQ(TestEventType_ThreadExit, pBaseEv->eventType);
        auto *pEv = reinterpret_cast<TestEventThreadExit *>(pBaseEv);
        ASSERT_EQ(TEST_EVENT_THREAD_NUM_1, pEv->thread_num);
    }
    testEvents.clear();

    cmdQueue.push_and_wait(MainTaskCmd_SendToTask2Signal2);
    ASSERT_TRUE(wait_until_new_events_pushed(2, 1000));
    {
        auto *pBaseEv = testEvents[0];
        ASSERT_EQ(TestEventType_Signal, pBaseEv->eventType);
        auto *pEv = reinterpret_cast<TestEventSignal *>(pBaseEv);
        ASSERT_EQ(OS_SIGNAL_NUM_2, pEv->sig_num);
    }
    {
        auto *pBaseEv = testEvents[1];
        ASSERT_EQ(TestEventType_ThreadExit, pBaseEv->eventType);
        auto *pEv = reinterpret_cast<TestEventThreadExit *>(pBaseEv);
        ASSERT_EQ(TEST_EVENT_THREAD_NUM_2, pEv->thread_num);
    }
    testEvents.clear();

    event_mgr_deinit();
}