#include <stdio.h>
#include <mqueue.h>
#include <semaphore.h>
#include "gtest/gtest.h"
#include "leds.h"
#include "TQueue.h"
#include "esp_err.h"
#include "esp_timer.h"
#include "driver/ledc.h"
#include "test_events.h"

using namespace std;

typedef enum MainTaskCmd_Tag
{
    MainTaskCmd_Exit,
    MainTaskCmd_LedsInit,
    MainTaskCmd_LedsOn,
    MainTaskCmd_LedsOff,
    MainTaskCmd_LedsStartBlink,
    MainTaskCmd_LedsStopBlink,
} MainTaskCmd_e;

/*** Google-test class implementation *********************************************************************************/

static void* freertosStartup(void *arg);

class TestLeds : public ::testing::Test
{
private:

protected:
    pthread_t pid;

    void SetUp() override
    {
        sem_init(&semaFreeRTOS, 0, 0);
        const int err = pthread_create(&pid, nullptr, &freertosStartup, this);
        assert(0 == err);
        while (0 != sem_wait(&semaFreeRTOS))
        {
        }
    }

    void TearDown() override
    {
        cmdQueue.push_and_wait(MainTaskCmd_Exit);
        vTaskEndScheduler();
        void* ret_code = nullptr;
        pthread_join(pid, &ret_code);
        sem_destroy(&semaFreeRTOS);
    }

public:
    sem_t semaFreeRTOS;
    TQueue<MainTaskCmd_e> cmdQueue;
    std::vector<TestEvent*> testEvents;

    TestLeds();

    ~TestLeds() override;
};

static TestLeds* g_pTestLeds;

TestLeds::TestLeds() : Test()
{
    g_pTestLeds = this;
}

TestLeds::~TestLeds()
{
    g_pTestLeds = nullptr;
}

/*** esp_timer stub functions *****************************************************************************************/

esp_err_t esp_timer_init(void)
{
    g_pTestLeds->testEvents.push_back(new TestEventEspTimerInit());
    return ESP_OK;
}

esp_err_t esp_timer_create(const esp_timer_create_args_t* args,
                           esp_timer_handle_t* out_handle)
{
    g_pTestLeds->testEvents.push_back(
            new TestEventEspTimerCreate(args->callback, args->arg, args->dispatch_method, args->name));
    return ESP_OK;
}

esp_err_t esp_timer_start_periodic(esp_timer_handle_t timer, uint64_t period_us)
{
    g_pTestLeds->testEvents.push_back(new TestEventEspTimerStartPeriodic(timer, period_us));
    return ESP_OK;
}

esp_err_t esp_timer_stop(esp_timer_handle_t timer)
{
    g_pTestLeds->testEvents.push_back(new TestEventEspTimerStop(timer));
    return ESP_OK;
}

/*** driver/ledc.c stub functions *************************************************************************************/

esp_err_t ledc_timer_config(const ledc_timer_config_t* timer_conf)
{
    g_pTestLeds->testEvents.push_back(new TestEventLedcTimerConfig(timer_conf));
    return ESP_OK;
}

esp_err_t ledc_channel_config(const ledc_channel_config_t* ledc_conf)
{
    g_pTestLeds->testEvents.push_back(new TestEventLedcChannelConfig(ledc_conf));
    return ESP_OK;
}

esp_err_t ledc_set_fade_with_time(ledc_mode_t speed_mode, ledc_channel_t channel, uint32_t target_duty, int max_fade_time_ms)
{
    g_pTestLeds->testEvents.push_back(new TestEventLedcSetFadeWithTime(speed_mode, channel, target_duty, max_fade_time_ms));
    return ESP_OK;
}

esp_err_t ledc_fade_start(ledc_mode_t speed_mode, ledc_channel_t channel, ledc_fade_mode_t fade_mode)
{
    g_pTestLeds->testEvents.push_back(new TestEventLedcFadeStart(speed_mode, channel, fade_mode));
    return ESP_OK;
}

esp_err_t ledc_fade_func_install(int intr_alloc_flags)
{
    g_pTestLeds->testEvents.push_back(new TestEventLedcFadeFuncInstall(intr_alloc_flags));
    return ESP_OK;
}

/*** Cmd-handler task *************************************************************************************************/

static void cmdHandlerTask(void *parameters)
{
    auto* pTestLeds = static_cast<TestLeds *>(parameters);
    bool flagExit = false;
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
                leds_start_blink(5123);
                break;
            case MainTaskCmd_LedsStopBlink:
                leds_stop_blink();
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

static void* freertosStartup(void *arg)
{
    auto* pObj = static_cast<TestLeds *>(arg);
    BaseType_t res = xTaskCreate(cmdHandlerTask, "cmdHandlerTask", configMINIMAL_STACK_SIZE, pObj,
                                 (tskIDLE_PRIORITY + 1),
                                 (xTaskHandle *) nullptr);
    assert(pdPASS == res);
    vTaskStartScheduler();
    return nullptr;
}

/*** Unit-Tests *******************************************************************************************************/

TEST_F(TestLeds, test_all) // NOLINT
{
    esp_timer_cb_t timer_cb = nullptr;
    void* timer_cb_arg = nullptr;

    cmdQueue.push_and_wait(MainTaskCmd_LedsInit);

    int idx = 0;
    ASSERT_EQ(idx + 5, testEvents.size());
    {
        auto* pEv = static_cast<TestEventEspTimerInit *>(testEvents[idx++]);
        ASSERT_EQ(TestEventType_EspTimerInit, pEv->eventType);
    }
    {
        auto* pEv = static_cast<TestEventEspTimerCreate *>(testEvents[idx++]);
        ASSERT_EQ(TestEventType_EspTimerCreate, pEv->eventType);
        ASSERT_EQ(ESP_TIMER_TASK, pEv->dispatch_method);
        ASSERT_NE(nullptr, pEv->callback);
        ASSERT_EQ(nullptr, pEv->arg);
        ASSERT_EQ(string("blink_timer"), pEv->name);
        timer_cb = pEv->callback;
        timer_cb_arg = pEv->arg;
    }
    {
        auto* pEv = static_cast<TestEventLedcTimerConfig *>(testEvents[idx++]);
        ASSERT_EQ(TestEventType_LedcTimerConfig, pEv->eventType);
        ASSERT_EQ(LEDC_HIGH_SPEED_MODE, pEv->speed_mode);
        ASSERT_EQ(LEDC_TIMER_10_BIT, pEv->duty_resolution);
        ASSERT_EQ(LEDC_TIMER_1, pEv->timer_num);
        ASSERT_EQ(5000, pEv->freq_hz);
        ASSERT_EQ(LEDC_AUTO_CLK, pEv->clk_cfg);
    }
    {
        auto* pEv = static_cast<TestEventLedcChannelConfig *>(testEvents[idx++]);
        ASSERT_EQ(TestEventType_LedcChannelConfig, pEv->eventType);
        ASSERT_EQ(LEDC_CHANNEL_0, pEv->channel);
        ASSERT_EQ(0, pEv->duty);
        ASSERT_EQ(23, pEv->gpio_num);
        ASSERT_EQ(LEDC_HIGH_SPEED_MODE, pEv->speed_mode);
        ASSERT_EQ(0, pEv->hpoint);
        ASSERT_EQ(LEDC_TIMER_1, pEv->timer_sel);
    }
    {
        auto* pEv = static_cast<TestEventLedcFadeFuncInstall *>(testEvents[idx++]);
        ASSERT_EQ(TestEventType_LedcFadeFuncInstall, pEv->eventType);
        ASSERT_EQ(0, pEv->intr_alloc_flags);
    }
    ASSERT_EQ(idx, testEvents.size());

    // *** LedsOn

    cmdQueue.push_and_wait(MainTaskCmd_LedsOn);
    ASSERT_EQ(idx + 2, testEvents.size());
    {
        auto* pEv = static_cast<TestEventLedcSetFadeWithTime *>(testEvents[idx++]);
        ASSERT_EQ(TestEventType_LedcSetFadeWithTime, pEv->eventType);
        ASSERT_EQ(LEDC_HIGH_SPEED_MODE, pEv->speed_mode);
        ASSERT_EQ(LEDC_CHANNEL_0, pEv->channel);
        ASSERT_EQ(1023, pEv->target_duty);
        ASSERT_EQ(50, pEv->max_fade_time_ms);
    }
    {
        auto* pEv = static_cast<TestEventLedcFadeStart *>(testEvents[idx++]);
        ASSERT_EQ(TestEventType_LedcFadeStart, pEv->eventType);
        ASSERT_EQ(LEDC_HIGH_SPEED_MODE, pEv->speed_mode);
        ASSERT_EQ(LEDC_CHANNEL_0, pEv->channel);
        ASSERT_EQ(LEDC_FADE_NO_WAIT, pEv->fade_mode);
    }
    ASSERT_EQ(idx, testEvents.size());

    // *** LedsOff

    cmdQueue.push_and_wait(MainTaskCmd_LedsOff);
    ASSERT_EQ(idx + 2, testEvents.size());
    {
        auto* pEv = static_cast<TestEventLedcSetFadeWithTime *>(testEvents[idx++]);
        ASSERT_EQ(TestEventType_LedcSetFadeWithTime, pEv->eventType);
        ASSERT_EQ(LEDC_HIGH_SPEED_MODE, pEv->speed_mode);
        ASSERT_EQ(LEDC_CHANNEL_0, pEv->channel);
        ASSERT_EQ(0, pEv->target_duty);
        ASSERT_EQ(50, pEv->max_fade_time_ms);
    }
    {
        auto* pEv = static_cast<TestEventLedcFadeStart *>(testEvents[idx++]);
        ASSERT_EQ(TestEventType_LedcFadeStart, pEv->eventType);
        ASSERT_EQ(LEDC_HIGH_SPEED_MODE, pEv->speed_mode);
        ASSERT_EQ(LEDC_CHANNEL_0, pEv->channel);
        ASSERT_EQ(LEDC_FADE_NO_WAIT, pEv->fade_mode);
    }
    ASSERT_EQ(idx, testEvents.size());

    // *** LedsStartBlink

    cmdQueue.push_and_wait(MainTaskCmd_LedsStartBlink);
    ASSERT_EQ(idx + 1, testEvents.size());
    {
        auto* pEv = static_cast<TestEventEspTimerStartPeriodic *>(testEvents[idx++]);
        ASSERT_EQ(TestEventType_EspTimerStartPeriodic, pEv->eventType);
        ASSERT_EQ(5123 * 1000, pEv->period_us);
        ASSERT_EQ(nullptr, pEv->timer);
    }
    ASSERT_EQ(idx, testEvents.size());

    // *** Simulate timer callback 1

    timer_cb(timer_cb_arg);
    ASSERT_EQ(idx + 2, testEvents.size());
    {
        auto* pEv = static_cast<TestEventLedcSetFadeWithTime *>(testEvents[idx++]);
        ASSERT_EQ(TestEventType_LedcSetFadeWithTime, pEv->eventType);
        ASSERT_EQ(LEDC_HIGH_SPEED_MODE, pEv->speed_mode);
        ASSERT_EQ(LEDC_CHANNEL_0, pEv->channel);
        ASSERT_EQ(1023, pEv->target_duty);
        ASSERT_EQ(50, pEv->max_fade_time_ms);
    }
    {
        auto* pEv = static_cast<TestEventLedcFadeStart *>(testEvents[idx++]);
        ASSERT_EQ(TestEventType_LedcFadeStart, pEv->eventType);
        ASSERT_EQ(LEDC_HIGH_SPEED_MODE, pEv->speed_mode);
        ASSERT_EQ(LEDC_CHANNEL_0, pEv->channel);
        ASSERT_EQ(LEDC_FADE_NO_WAIT, pEv->fade_mode);
    }
    ASSERT_EQ(idx, testEvents.size());

    // *** Simulate timer callback 2

    timer_cb(timer_cb_arg);
    ASSERT_EQ(idx + 2, testEvents.size());
    {
        auto* pEv = static_cast<TestEventLedcSetFadeWithTime *>(testEvents[idx++]);
        ASSERT_EQ(TestEventType_LedcSetFadeWithTime, pEv->eventType);
        ASSERT_EQ(LEDC_HIGH_SPEED_MODE, pEv->speed_mode);
        ASSERT_EQ(LEDC_CHANNEL_0, pEv->channel);
        ASSERT_EQ(0, pEv->target_duty);
        ASSERT_EQ(50, pEv->max_fade_time_ms);
    }
    {
        auto* pEv = static_cast<TestEventLedcFadeStart *>(testEvents[idx++]);
        ASSERT_EQ(TestEventType_LedcFadeStart, pEv->eventType);
        ASSERT_EQ(LEDC_HIGH_SPEED_MODE, pEv->speed_mode);
        ASSERT_EQ(LEDC_CHANNEL_0, pEv->channel);
        ASSERT_EQ(LEDC_FADE_NO_WAIT, pEv->fade_mode);
    }
    ASSERT_EQ(idx, testEvents.size());

    // *** LedsStopBlink

    cmdQueue.push_and_wait(MainTaskCmd_LedsStopBlink);
    ASSERT_EQ(idx + 3, testEvents.size());
    {
        auto* pEv = static_cast<TestEventEspTimerStop *>(testEvents[idx++]);
        ASSERT_EQ(TestEventType_EspTimerStop, pEv->eventType);
        ASSERT_EQ(0, pEv->timer);
    }
    {
        auto* pEv = static_cast<TestEventLedcSetFadeWithTime *>(testEvents[idx++]);
        ASSERT_EQ(TestEventType_LedcSetFadeWithTime, pEv->eventType);
        ASSERT_EQ(LEDC_HIGH_SPEED_MODE, pEv->speed_mode);
        ASSERT_EQ(LEDC_CHANNEL_0, pEv->channel);
        ASSERT_EQ(0, pEv->target_duty);
        ASSERT_EQ(50, pEv->max_fade_time_ms);
    }
    {
        auto* pEv = static_cast<TestEventLedcFadeStart *>(testEvents[idx++]);
        ASSERT_EQ(TestEventType_LedcFadeStart, pEv->eventType);
        ASSERT_EQ(LEDC_HIGH_SPEED_MODE, pEv->speed_mode);
        ASSERT_EQ(LEDC_CHANNEL_0, pEv->channel);
        ASSERT_EQ(LEDC_FADE_NO_WAIT, pEv->fade_mode);
    }
    ASSERT_EQ(idx, testEvents.size());
}
