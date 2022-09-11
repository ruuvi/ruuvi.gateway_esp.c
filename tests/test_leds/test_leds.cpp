#include "TQueue.hpp"
#include <cstdio>
#include "driver/ledc.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "esp_err.h"
#include "esp_timer.h"
#include "leds.h"
#include "test_events.h"
#include "gtest/gtest.h"
#include <mqueue.h>
#include <semaphore.h>
#include "esp_log_wrapper.hpp"

#define LEDC_TEST_DUTY_OFF  (1023 - 0 /* 0% */)
#define LEDC_TEST_DUTY_ON   (1023 - 256 /* 256 / 1024 = 25% */)
#define LEDC_TEST_FADE_TIME (25)

using namespace std;

typedef enum MainTaskCmd_Tag
{
    MainTaskCmd_Exit,
    MainTaskCmd_LedsInit,
    MainTaskCmd_LedsOn,
    MainTaskCmd_LedsOff,
    MainTaskCmd_LedsStartBlink,
    MainTaskCmd_LedsStartBlinkWithZeroDutyCycle,
    MainTaskCmd_LedsStartBlinkWithZeroPeriod,
} MainTaskCmd_e;

/*** Google-test class implementation
 * *********************************************************************************/

static void *
freertosStartup(void *arg);

class TestLeds : public ::testing::Test
{
private:
protected:
    void
    SetUp() override
    {
        esp_log_wrapper_init();
        sem_init(&semaFreeRTOS, 0, 0);
        pid_test      = pthread_self();
        const int err = pthread_create(&pid_freertos, nullptr, &freertosStartup, this);
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
        pthread_join(pid_freertos, &ret_code);
        sem_destroy(&semaFreeRTOS);
        esp_log_wrapper_deinit();
    }

public:
    pthread_t                pid_test;
    pthread_t                pid_freertos;
    sem_t                    semaFreeRTOS;
    TQueue<MainTaskCmd_e>    cmdQueue;
    std::vector<TestEvent *> testEvents;

    TestLeds();

    ~TestLeds() override;
};

static TestLeds *g_pTestLeds;

TestLeds::TestLeds()
    : Test()
{
    g_pTestLeds = this;
}

TestLeds::~TestLeds()
{
    g_pTestLeds = nullptr;
}

#ifdef __cplusplus
extern "C" {
#endif

/*** System functions
 * *****************************************************************************************/

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
    if (nullptr == g_pTestLeds)
    {
        return false;
    }
    if (g_flagDisableCheckIsThreadFreeRTOS)
    {
        return true;
    }
    const pthread_t cur_thread_pid = pthread_self();
    if (cur_thread_pid == g_pTestLeds->pid_test)
    {
        return false;
    }
    return true;
}

/*** driver/ledc.c stub functions
 * *************************************************************************************/

esp_err_t
ledc_timer_config(const ledc_timer_config_t *timer_conf)
{
    g_pTestLeds->testEvents.push_back(new TestEventLedcTimerConfig(timer_conf));
    return ESP_OK;
}

esp_err_t
ledc_channel_config(const ledc_channel_config_t *ledc_conf)
{
    g_pTestLeds->testEvents.push_back(new TestEventLedcChannelConfig(ledc_conf));
    return ESP_OK;
}

esp_err_t
ledc_set_fade_with_time(ledc_mode_t speed_mode, ledc_channel_t channel, uint32_t target_duty, int max_fade_time_ms)
{
    g_pTestLeds->testEvents.push_back(
        new TestEventLedcSetFadeWithTime(speed_mode, channel, target_duty, max_fade_time_ms));
    return ESP_OK;
}

esp_err_t
ledc_fade_start(ledc_mode_t speed_mode, ledc_channel_t channel, ledc_fade_mode_t fade_mode)
{
    g_pTestLeds->testEvents.push_back(new TestEventLedcFadeStart(speed_mode, channel, fade_mode));
    return ESP_OK;
}

esp_err_t
ledc_fade_func_install(int intr_alloc_flags)
{
    g_pTestLeds->testEvents.push_back(new TestEventLedcFadeFuncInstall(intr_alloc_flags));
    return ESP_OK;
}

/*** gpio_switch_ctrl.c stub functions
 * *************************************************************************************/

void
gpio_switch_ctrl_activate(void)
{
}

void
gpio_switch_ctrl_deactivate(void)
{
}

esp_err_t
esp_task_wdt_reset()
{
    return ESP_OK;
}

esp_err_t
esp_task_wdt_add(TaskHandle_t handle)
{
    return ESP_OK;
}

#ifdef __cplusplus
}
#endif

/*** Cmd-handler task
 * *************************************************************************************************/

static void
cmdHandlerTask(void *parameters)
{
    auto *pTestLeds = static_cast<TestLeds *>(parameters);
    bool  flagExit  = false;
    sem_post(&pTestLeds->semaFreeRTOS);
    while (!flagExit)
    {
        const MainTaskCmd_e cmd = pTestLeds->cmdQueue.pop();
        switch (cmd)
        {
            case MainTaskCmd_Exit:
                flagExit = true;
                break;
            case MainTaskCmd_LedsInit:
                leds_init();
                break;
            case MainTaskCmd_LedsOn:
                leds_on();
                break;
            case MainTaskCmd_LedsOff:
                leds_off();
                break;
            case MainTaskCmd_LedsStartBlink:
                leds_start_blink(1000, 50);
                break;
            case MainTaskCmd_LedsStartBlinkWithZeroDutyCycle:
                leds_start_blink(1000, 0);
                break;
            case MainTaskCmd_LedsStartBlinkWithZeroPeriod:
                leds_start_blink(0, 50);
                break;
            default:
                printf("Error: Unknown cmd %d\n", (int)cmd);
                exit(1);
                break;
        }
        pTestLeds->cmdQueue.notify_handled();
    }
    vTaskDelete(nullptr);
}

static void *
freertosStartup(void *arg)
{
    auto *pObj = static_cast<TestLeds *>(arg);
    disableCheckingIfCurThreadIsFreeRTOS();
    BaseType_t res = xTaskCreate(
        cmdHandlerTask,
        "cmdHandlerTask",
        configMINIMAL_STACK_SIZE,
        pObj,
        (tskIDLE_PRIORITY + 1),
        (xTaskHandle *)nullptr);
    assert(pdPASS == res);
    vTaskStartScheduler();
    return nullptr;
}

static void
msleep(uint32_t msec)
{
    struct timespec ts = {
        .tv_sec  = msec / 1000U,
        .tv_nsec = (msec % 1000U) * 1000000U,
    };
    int res = 0;
    do
    {
        res = nanosleep(&ts, &ts);
    } while (res && errno == EINTR);
}

/*** Unit-Tests
 * *******************************************************************************************************/

TEST_F(TestLeds, test_all) // NOLINT
{
    cmdQueue.push_and_wait(MainTaskCmd_LedsInit);

    int idx = 0;
    ASSERT_EQ(idx + 3, testEvents.size());
    {
        auto *pEv = reinterpret_cast<TestEventLedcTimerConfig *>(testEvents[idx++]);
        ASSERT_EQ(TestEventType_LedcTimerConfig, pEv->eventType);
        ASSERT_EQ(LEDC_HIGH_SPEED_MODE, pEv->speed_mode);
        ASSERT_EQ(LEDC_TIMER_10_BIT, pEv->duty_resolution);
        ASSERT_EQ(LEDC_TIMER_1, pEv->timer_num);
        ASSERT_EQ(5000, pEv->freq_hz);
        ASSERT_EQ(LEDC_AUTO_CLK, pEv->clk_cfg);
    }
    {
        auto *pEv = reinterpret_cast<TestEventLedcChannelConfig *>(testEvents[idx++]);
        ASSERT_EQ(TestEventType_LedcChannelConfig, pEv->eventType);
        ASSERT_EQ(LEDC_CHANNEL_0, pEv->channel);
        ASSERT_EQ(0, pEv->duty);
        ASSERT_EQ(13, pEv->gpio_num);
        ASSERT_EQ(LEDC_HIGH_SPEED_MODE, pEv->speed_mode);
        ASSERT_EQ(0, pEv->hpoint);
        ASSERT_EQ(LEDC_TIMER_1, pEv->timer_sel);
    }
    {
        auto *pEv = reinterpret_cast<TestEventLedcFadeFuncInstall *>(testEvents[idx++]);
        ASSERT_EQ(TestEventType_LedcFadeFuncInstall, pEv->eventType);
        ASSERT_EQ(0, pEv->intr_alloc_flags);
    }
    ASSERT_EQ(idx, testEvents.size());

    // *** LedsOn

    cmdQueue.push_and_wait(MainTaskCmd_LedsOn);
    msleep(100);
    ASSERT_EQ(idx + 2, testEvents.size());
    {
        auto *pEv = reinterpret_cast<TestEventLedcSetFadeWithTime *>(testEvents[idx++]);
        ASSERT_EQ(TestEventType_LedcSetFadeWithTime, pEv->eventType);
        ASSERT_EQ(LEDC_HIGH_SPEED_MODE, pEv->speed_mode);
        ASSERT_EQ(LEDC_CHANNEL_0, pEv->channel);
        ASSERT_EQ(LEDC_TEST_DUTY_ON, pEv->target_duty);
        ASSERT_EQ(LEDC_TEST_FADE_TIME, pEv->max_fade_time_ms);
    }
    {
        auto *pEv = reinterpret_cast<TestEventLedcFadeStart *>(testEvents[idx++]);
        ASSERT_EQ(TestEventType_LedcFadeStart, pEv->eventType);
        ASSERT_EQ(LEDC_HIGH_SPEED_MODE, pEv->speed_mode);
        ASSERT_EQ(LEDC_CHANNEL_0, pEv->channel);
        ASSERT_EQ(LEDC_FADE_WAIT_DONE, pEv->fade_mode);
    }
    ASSERT_EQ(idx, testEvents.size());

    // *** LedsOff

    cmdQueue.push_and_wait(MainTaskCmd_LedsOff);
    msleep(100);
    ASSERT_EQ(idx + 2, testEvents.size());
    {
        auto *pEv = reinterpret_cast<TestEventLedcSetFadeWithTime *>(testEvents[idx++]);
        ASSERT_EQ(TestEventType_LedcSetFadeWithTime, pEv->eventType);
        ASSERT_EQ(LEDC_HIGH_SPEED_MODE, pEv->speed_mode);
        ASSERT_EQ(LEDC_CHANNEL_0, pEv->channel);
        ASSERT_EQ(LEDC_TEST_DUTY_OFF, pEv->target_duty);
        ASSERT_EQ(LEDC_TEST_FADE_TIME, pEv->max_fade_time_ms);
    }
    {
        auto *pEv = reinterpret_cast<TestEventLedcFadeStart *>(testEvents[idx++]);
        ASSERT_EQ(TestEventType_LedcFadeStart, pEv->eventType);
        ASSERT_EQ(LEDC_HIGH_SPEED_MODE, pEv->speed_mode);
        ASSERT_EQ(LEDC_CHANNEL_0, pEv->channel);
        ASSERT_EQ(LEDC_FADE_WAIT_DONE, pEv->fade_mode);
    }
    ASSERT_EQ(idx, testEvents.size());

    // *** LedsStartBlink

    cmdQueue.push_and_wait(MainTaskCmd_LedsStartBlink);
    msleep(100);

    ASSERT_EQ(idx + 2, testEvents.size());
    {
        auto *pEv = reinterpret_cast<TestEventLedcSetFadeWithTime *>(testEvents[idx++]);
        ASSERT_EQ(TestEventType_LedcSetFadeWithTime, pEv->eventType);
        ASSERT_EQ(LEDC_HIGH_SPEED_MODE, pEv->speed_mode);
        ASSERT_EQ(LEDC_CHANNEL_0, pEv->channel);
        ASSERT_EQ(LEDC_TEST_DUTY_ON, pEv->target_duty);
        ASSERT_EQ(LEDC_TEST_FADE_TIME, pEv->max_fade_time_ms);
    }
    {
        auto *pEv = reinterpret_cast<TestEventLedcFadeStart *>(testEvents[idx++]);
        ASSERT_EQ(TestEventType_LedcFadeStart, pEv->eventType);
        ASSERT_EQ(LEDC_HIGH_SPEED_MODE, pEv->speed_mode);
        ASSERT_EQ(LEDC_CHANNEL_0, pEv->channel);
        ASSERT_EQ(LEDC_FADE_WAIT_DONE, pEv->fade_mode);
    }
    ASSERT_EQ(idx, testEvents.size());

    // *** Wait timer OFF
    msleep(500);

    ASSERT_EQ(idx + 2, testEvents.size());
    {
        auto *pEv = reinterpret_cast<TestEventLedcSetFadeWithTime *>(testEvents[idx++]);
        ASSERT_EQ(TestEventType_LedcSetFadeWithTime, pEv->eventType);
        ASSERT_EQ(LEDC_HIGH_SPEED_MODE, pEv->speed_mode);
        ASSERT_EQ(LEDC_CHANNEL_0, pEv->channel);
        ASSERT_EQ(LEDC_TEST_DUTY_OFF, pEv->target_duty);
        ASSERT_EQ(LEDC_TEST_FADE_TIME, pEv->max_fade_time_ms);
    }
    {
        auto *pEv = reinterpret_cast<TestEventLedcFadeStart *>(testEvents[idx++]);
        ASSERT_EQ(TestEventType_LedcFadeStart, pEv->eventType);
        ASSERT_EQ(LEDC_HIGH_SPEED_MODE, pEv->speed_mode);
        ASSERT_EQ(LEDC_CHANNEL_0, pEv->channel);
        ASSERT_EQ(LEDC_FADE_WAIT_DONE, pEv->fade_mode);
    }
    ASSERT_EQ(idx, testEvents.size());

    // *** Wait timer ON
    msleep(500);

    ASSERT_EQ(idx + 2, testEvents.size());
    {
        auto *pEv = reinterpret_cast<TestEventLedcSetFadeWithTime *>(testEvents[idx++]);
        ASSERT_EQ(TestEventType_LedcSetFadeWithTime, pEv->eventType);
        ASSERT_EQ(LEDC_HIGH_SPEED_MODE, pEv->speed_mode);
        ASSERT_EQ(LEDC_CHANNEL_0, pEv->channel);
        ASSERT_EQ(LEDC_TEST_DUTY_ON, pEv->target_duty);
        ASSERT_EQ(LEDC_TEST_FADE_TIME, pEv->max_fade_time_ms);
    }
    {
        auto *pEv = reinterpret_cast<TestEventLedcFadeStart *>(testEvents[idx++]);
        ASSERT_EQ(TestEventType_LedcFadeStart, pEv->eventType);
        ASSERT_EQ(LEDC_HIGH_SPEED_MODE, pEv->speed_mode);
        ASSERT_EQ(LEDC_CHANNEL_0, pEv->channel);
        ASSERT_EQ(LEDC_FADE_WAIT_DONE, pEv->fade_mode);
    }
    ASSERT_EQ(idx, testEvents.size());

    // *** Wait timer OFF
    msleep(500);

    ASSERT_EQ(idx + 2, testEvents.size());
    {
        auto *pEv = reinterpret_cast<TestEventLedcSetFadeWithTime *>(testEvents[idx++]);
        ASSERT_EQ(TestEventType_LedcSetFadeWithTime, pEv->eventType);
        ASSERT_EQ(LEDC_HIGH_SPEED_MODE, pEv->speed_mode);
        ASSERT_EQ(LEDC_CHANNEL_0, pEv->channel);
        ASSERT_EQ(LEDC_TEST_DUTY_OFF, pEv->target_duty);
        ASSERT_EQ(LEDC_TEST_FADE_TIME, pEv->max_fade_time_ms);
    }
    {
        auto *pEv = reinterpret_cast<TestEventLedcFadeStart *>(testEvents[idx++]);
        ASSERT_EQ(TestEventType_LedcFadeStart, pEv->eventType);
        ASSERT_EQ(LEDC_HIGH_SPEED_MODE, pEv->speed_mode);
        ASSERT_EQ(LEDC_CHANNEL_0, pEv->channel);
        ASSERT_EQ(LEDC_FADE_WAIT_DONE, pEv->fade_mode);
    }
    ASSERT_EQ(idx, testEvents.size());

    // *** LedsStopBlink

    cmdQueue.push_and_wait(MainTaskCmd_LedsOff);
    msleep(100);
    ASSERT_EQ(idx + 2, testEvents.size());
    {
        auto *pEv = reinterpret_cast<TestEventLedcSetFadeWithTime *>(testEvents[idx++]);
        ASSERT_EQ(TestEventType_LedcSetFadeWithTime, pEv->eventType);
        ASSERT_EQ(LEDC_HIGH_SPEED_MODE, pEv->speed_mode);
        ASSERT_EQ(LEDC_CHANNEL_0, pEv->channel);
        ASSERT_EQ(LEDC_TEST_DUTY_OFF, pEv->target_duty);
        ASSERT_EQ(LEDC_TEST_FADE_TIME, pEv->max_fade_time_ms);
    }
    {
        auto *pEv = reinterpret_cast<TestEventLedcFadeStart *>(testEvents[idx++]);
        ASSERT_EQ(TestEventType_LedcFadeStart, pEv->eventType);
        ASSERT_EQ(LEDC_HIGH_SPEED_MODE, pEv->speed_mode);
        ASSERT_EQ(LEDC_CHANNEL_0, pEv->channel);
        ASSERT_EQ(LEDC_FADE_WAIT_DONE, pEv->fade_mode);
    }
    ASSERT_EQ(idx, testEvents.size());

    cmdQueue.push_and_wait(MainTaskCmd_LedsStartBlinkWithZeroDutyCycle);
    msleep(100);
    ASSERT_EQ(idx + 2, testEvents.size());
    {
        auto *pEv = reinterpret_cast<TestEventLedcSetFadeWithTime *>(testEvents[idx++]);
        ASSERT_EQ(TestEventType_LedcSetFadeWithTime, pEv->eventType);
        ASSERT_EQ(LEDC_HIGH_SPEED_MODE, pEv->speed_mode);
        ASSERT_EQ(LEDC_CHANNEL_0, pEv->channel);
        ASSERT_EQ(LEDC_TEST_DUTY_OFF, pEv->target_duty);
        ASSERT_EQ(LEDC_TEST_FADE_TIME, pEv->max_fade_time_ms);
    }
    {
        auto *pEv = reinterpret_cast<TestEventLedcFadeStart *>(testEvents[idx++]);
        ASSERT_EQ(TestEventType_LedcFadeStart, pEv->eventType);
        ASSERT_EQ(LEDC_HIGH_SPEED_MODE, pEv->speed_mode);
        ASSERT_EQ(LEDC_CHANNEL_0, pEv->channel);
        ASSERT_EQ(LEDC_FADE_WAIT_DONE, pEv->fade_mode);
    }
    ASSERT_EQ(idx, testEvents.size());

    cmdQueue.push_and_wait(MainTaskCmd_LedsOn);
    msleep(100);
    ASSERT_EQ(idx + 2, testEvents.size());
    {
        auto *pEv = reinterpret_cast<TestEventLedcSetFadeWithTime *>(testEvents[idx++]);
        ASSERT_EQ(TestEventType_LedcSetFadeWithTime, pEv->eventType);
        ASSERT_EQ(LEDC_HIGH_SPEED_MODE, pEv->speed_mode);
        ASSERT_EQ(LEDC_CHANNEL_0, pEv->channel);
        ASSERT_EQ(LEDC_TEST_DUTY_ON, pEv->target_duty);
        ASSERT_EQ(LEDC_TEST_FADE_TIME, pEv->max_fade_time_ms);
    }
    {
        auto *pEv = reinterpret_cast<TestEventLedcFadeStart *>(testEvents[idx++]);
        ASSERT_EQ(TestEventType_LedcFadeStart, pEv->eventType);
        ASSERT_EQ(LEDC_HIGH_SPEED_MODE, pEv->speed_mode);
        ASSERT_EQ(LEDC_CHANNEL_0, pEv->channel);
        ASSERT_EQ(LEDC_FADE_WAIT_DONE, pEv->fade_mode);
    }
    ASSERT_EQ(idx, testEvents.size());

    cmdQueue.push_and_wait(MainTaskCmd_LedsStartBlinkWithZeroDutyCycle);
    msleep(100);

    ASSERT_EQ(idx + 2, testEvents.size());
    {
        auto *pEv = reinterpret_cast<TestEventLedcSetFadeWithTime *>(testEvents[idx++]);
        ASSERT_EQ(TestEventType_LedcSetFadeWithTime, pEv->eventType);
        ASSERT_EQ(LEDC_HIGH_SPEED_MODE, pEv->speed_mode);
        ASSERT_EQ(LEDC_CHANNEL_0, pEv->channel);
        ASSERT_EQ(LEDC_TEST_DUTY_OFF, pEv->target_duty);
        ASSERT_EQ(LEDC_TEST_FADE_TIME, pEv->max_fade_time_ms);
    }
    {
        auto *pEv = reinterpret_cast<TestEventLedcFadeStart *>(testEvents[idx++]);
        ASSERT_EQ(TestEventType_LedcFadeStart, pEv->eventType);
        ASSERT_EQ(LEDC_HIGH_SPEED_MODE, pEv->speed_mode);
        ASSERT_EQ(LEDC_CHANNEL_0, pEv->channel);
        ASSERT_EQ(LEDC_FADE_WAIT_DONE, pEv->fade_mode);
    }
    ASSERT_EQ(idx, testEvents.size());

    cmdQueue.push_and_wait(MainTaskCmd_LedsStartBlinkWithZeroPeriod);
    msleep(100);
    ASSERT_EQ(idx + 2, testEvents.size());
    {
        auto *pEv = reinterpret_cast<TestEventLedcSetFadeWithTime *>(testEvents[idx++]);
        ASSERT_EQ(TestEventType_LedcSetFadeWithTime, pEv->eventType);
        ASSERT_EQ(LEDC_HIGH_SPEED_MODE, pEv->speed_mode);
        ASSERT_EQ(LEDC_CHANNEL_0, pEv->channel);
        ASSERT_EQ(LEDC_TEST_DUTY_ON, pEv->target_duty);
        ASSERT_EQ(LEDC_TEST_FADE_TIME, pEv->max_fade_time_ms);
    }
    {
        auto *pEv = reinterpret_cast<TestEventLedcFadeStart *>(testEvents[idx++]);
        ASSERT_EQ(TestEventType_LedcFadeStart, pEv->eventType);
        ASSERT_EQ(LEDC_HIGH_SPEED_MODE, pEv->speed_mode);
        ASSERT_EQ(LEDC_CHANNEL_0, pEv->channel);
        ASSERT_EQ(LEDC_FADE_WAIT_DONE, pEv->fade_mode);
    }
    ASSERT_EQ(idx, testEvents.size());

    /* Test LED turn on, then start blinking, then turn on again */

    // *** LedsOn

    cmdQueue.push_and_wait(MainTaskCmd_LedsOn);
    msleep(100);
    ASSERT_EQ(idx + 2, testEvents.size());
    {
        auto *pEv = reinterpret_cast<TestEventLedcSetFadeWithTime *>(testEvents[idx++]);
        ASSERT_EQ(TestEventType_LedcSetFadeWithTime, pEv->eventType);
        ASSERT_EQ(LEDC_HIGH_SPEED_MODE, pEv->speed_mode);
        ASSERT_EQ(LEDC_CHANNEL_0, pEv->channel);
        ASSERT_EQ(LEDC_TEST_DUTY_ON, pEv->target_duty);
        ASSERT_EQ(LEDC_TEST_FADE_TIME, pEv->max_fade_time_ms);
    }
    {
        auto *pEv = reinterpret_cast<TestEventLedcFadeStart *>(testEvents[idx++]);
        ASSERT_EQ(TestEventType_LedcFadeStart, pEv->eventType);
        ASSERT_EQ(LEDC_HIGH_SPEED_MODE, pEv->speed_mode);
        ASSERT_EQ(LEDC_CHANNEL_0, pEv->channel);
        ASSERT_EQ(LEDC_FADE_WAIT_DONE, pEv->fade_mode);
    }
    ASSERT_EQ(idx, testEvents.size());

    // *** LedsStartBlink

    cmdQueue.push_and_wait(MainTaskCmd_LedsStartBlink);
    msleep(100);

    ASSERT_EQ(idx + 2, testEvents.size());
    {
        auto *pEv = reinterpret_cast<TestEventLedcSetFadeWithTime *>(testEvents[idx++]);
        ASSERT_EQ(TestEventType_LedcSetFadeWithTime, pEv->eventType);
        ASSERT_EQ(LEDC_HIGH_SPEED_MODE, pEv->speed_mode);
        ASSERT_EQ(LEDC_CHANNEL_0, pEv->channel);
        ASSERT_EQ(LEDC_TEST_DUTY_ON, pEv->target_duty);
        ASSERT_EQ(LEDC_TEST_FADE_TIME, pEv->max_fade_time_ms);
    }
    {
        auto *pEv = reinterpret_cast<TestEventLedcFadeStart *>(testEvents[idx++]);
        ASSERT_EQ(TestEventType_LedcFadeStart, pEv->eventType);
        ASSERT_EQ(LEDC_HIGH_SPEED_MODE, pEv->speed_mode);
        ASSERT_EQ(LEDC_CHANNEL_0, pEv->channel);
        ASSERT_EQ(LEDC_FADE_WAIT_DONE, pEv->fade_mode);
    }
    ASSERT_EQ(idx, testEvents.size());

    // *** Wait timer OFF
    msleep(500);

    ASSERT_EQ(idx + 2, testEvents.size());
    {
        auto *pEv = reinterpret_cast<TestEventLedcSetFadeWithTime *>(testEvents[idx++]);
        ASSERT_EQ(TestEventType_LedcSetFadeWithTime, pEv->eventType);
        ASSERT_EQ(LEDC_HIGH_SPEED_MODE, pEv->speed_mode);
        ASSERT_EQ(LEDC_CHANNEL_0, pEv->channel);
        ASSERT_EQ(LEDC_TEST_DUTY_OFF, pEv->target_duty);
        ASSERT_EQ(LEDC_TEST_FADE_TIME, pEv->max_fade_time_ms);
    }
    {
        auto *pEv = reinterpret_cast<TestEventLedcFadeStart *>(testEvents[idx++]);
        ASSERT_EQ(TestEventType_LedcFadeStart, pEv->eventType);
        ASSERT_EQ(LEDC_HIGH_SPEED_MODE, pEv->speed_mode);
        ASSERT_EQ(LEDC_CHANNEL_0, pEv->channel);
        ASSERT_EQ(LEDC_FADE_WAIT_DONE, pEv->fade_mode);
    }
    ASSERT_EQ(idx, testEvents.size());

    // *** Wait timer ON
    msleep(500);

    ASSERT_EQ(idx + 2, testEvents.size());
    {
        auto *pEv = reinterpret_cast<TestEventLedcSetFadeWithTime *>(testEvents[idx++]);
        ASSERT_EQ(TestEventType_LedcSetFadeWithTime, pEv->eventType);
        ASSERT_EQ(LEDC_HIGH_SPEED_MODE, pEv->speed_mode);
        ASSERT_EQ(LEDC_CHANNEL_0, pEv->channel);
        ASSERT_EQ(LEDC_TEST_DUTY_ON, pEv->target_duty);
        ASSERT_EQ(LEDC_TEST_FADE_TIME, pEv->max_fade_time_ms);
    }
    {
        auto *pEv = reinterpret_cast<TestEventLedcFadeStart *>(testEvents[idx++]);
        ASSERT_EQ(TestEventType_LedcFadeStart, pEv->eventType);
        ASSERT_EQ(LEDC_HIGH_SPEED_MODE, pEv->speed_mode);
        ASSERT_EQ(LEDC_CHANNEL_0, pEv->channel);
        ASSERT_EQ(LEDC_FADE_WAIT_DONE, pEv->fade_mode);
    }
    ASSERT_EQ(idx, testEvents.size());

    // *** LedsOn

    cmdQueue.push_and_wait(MainTaskCmd_LedsOn);
    msleep(100);
    ASSERT_EQ(idx + 2, testEvents.size());
    {
        auto *pEv = reinterpret_cast<TestEventLedcSetFadeWithTime *>(testEvents[idx++]);
        ASSERT_EQ(TestEventType_LedcSetFadeWithTime, pEv->eventType);
        ASSERT_EQ(LEDC_HIGH_SPEED_MODE, pEv->speed_mode);
        ASSERT_EQ(LEDC_CHANNEL_0, pEv->channel);
        ASSERT_EQ(LEDC_TEST_DUTY_ON, pEv->target_duty);
        ASSERT_EQ(LEDC_TEST_FADE_TIME, pEv->max_fade_time_ms);
    }
    {
        auto *pEv = reinterpret_cast<TestEventLedcFadeStart *>(testEvents[idx++]);
        ASSERT_EQ(TestEventType_LedcFadeStart, pEv->eventType);
        ASSERT_EQ(LEDC_HIGH_SPEED_MODE, pEv->speed_mode);
        ASSERT_EQ(LEDC_CHANNEL_0, pEv->channel);
        ASSERT_EQ(LEDC_FADE_WAIT_DONE, pEv->fade_mode);
    }
    ASSERT_EQ(idx, testEvents.size());

    msleep(500);
    ASSERT_EQ(idx, testEvents.size());
}
