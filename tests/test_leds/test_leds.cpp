#include "TQueue.hpp"
#include <cstdio>
#include <chrono>
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
#include "event_mgr.h"
#include "leds_ctrl.h"

#define LEDC_TEST_DUTY_OFF  (1023 - 0 /* 0% */)
#define LEDC_TEST_DUTY_ON   (1023 - 256 /* 256 / 1024 = 25% */)
#define LEDC_TEST_FADE_TIME (25)

using namespace std;

typedef enum MainTaskCmd_Tag
{
    MainTaskCmd_Exit,
    MainTaskCmd_LedsInit,
    MainTaskCmd_leds_notify_nrf52_fw_check,
    MainTaskCmd_leds_notify_nrf52_ready,
    MainTaskCmd_leds_notify_cfg_ready,
    MainTaskCmd_leds_notify_cfg_changed_ruuvi,
    MainTaskCmd_leds_notify_wifi_connected,
    MainTaskCmd_leds_notify_http1_data_sent_successfully,
    MainTaskCmd_leds_notify_http1_data_sent_fail,
} MainTaskCmd_e;

static os_signal_num_e g_event_to_sig_num[EVENT_MGR_EV_LAST];
static os_signal_t*    g_p_signal;

/*** Google-test class implementation
 * *********************************************************************************/

static void*
freertosStartup(void* arg);

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
        void* ret_code = nullptr;
        pthread_join(pid_freertos, &ret_code);
        sem_destroy(&semaFreeRTOS);
        esp_log_wrapper_deinit();
    }

public:
    pthread_t               pid_test;
    pthread_t               pid_freertos;
    sem_t                   semaFreeRTOS;
    TQueue<MainTaskCmd_e>   cmdQueue;
    std::vector<TestEvent*> testEvents;

    TestLeds();

    ~TestLeds() override;

    bool
    waitEvent(const std::chrono::duration<double, std::milli> timeout);
};

static TestLeds* g_pTestLeds;

TestLeds::TestLeds()
    : Test()
{
    g_pTestLeds = this;
}

TestLeds::~TestLeds()
{
    g_pTestLeds = nullptr;
}

bool
TestLeds::waitEvent(const std::chrono::duration<double, std::milli> timeout)
{
    const auto initialTestEventsSize = testEvents.size();
    const auto start                 = std::chrono::steady_clock::now();
    while (std::chrono::steady_clock::now() - start < timeout)
    {
        if (testEvents.size() > initialTestEventsSize)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
            break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }
    if (testEvents.size() > initialTestEventsSize)
    {
        return true;
    }
    return false;
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
ledc_timer_config(const ledc_timer_config_t* timer_conf)
{
    g_pTestLeds->testEvents.push_back(new TestEventLedcTimerConfig(timer_conf));
    return ESP_OK;
}

esp_err_t
ledc_channel_config(const ledc_channel_config_t* ledc_conf)
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

esp_err_t
ledc_stop(ledc_mode_t speed_mode, ledc_channel_t channel, uint32_t idle_level)
{
    g_pTestLeds->testEvents.push_back(new TestEventLedcStop(speed_mode, channel, idle_level));
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
    g_pTestLeds->testEvents.push_back(new TestEventTaskWdtReset());
    return ESP_OK;
}

esp_err_t
esp_task_wdt_add(TaskHandle_t handle)
{
    return ESP_OK;
}

void
event_mgr_notify(const event_mgr_ev_e event)
{
    if (EVENT_MGR_EV_GREEN_LED_TURN_ON == event)
    {
        g_pTestLeds->testEvents.push_back(new TestEventGreenLedTurnOn());
        return;
    }
    if (EVENT_MGR_EV_GREEN_LED_TURN_OFF == event)
    {
        g_pTestLeds->testEvents.push_back(new TestEventGreenLedTurnOff());
        return;
    }
    if (OS_SIGNAL_NUM_NONE != g_event_to_sig_num[event])
    {
        os_signal_send(g_p_signal, g_event_to_sig_num[event]);
    }
}

bool
gw_cfg_get_http_use_http_ruuvi(void)
{
    return true;
}

bool
gw_cfg_get_http_use_http(void)
{
    return false;
}

bool
gw_cfg_get_mqtt_use_mqtt(void)
{
    return false;
}

void
nrf52fw_hw_reset_nrf52(const bool flag_reset)
{
}

void
event_mgr_subscribe_sig_static(
    event_mgr_ev_info_static_t* const p_ev_info_mem,
    const event_mgr_ev_e              event,
    os_signal_t* const                p_signal,
    const os_signal_num_e             sig_num)
{
    g_p_signal                = p_signal;
    g_event_to_sig_num[event] = sig_num;
}

#ifdef __cplusplus
}
#endif

/*** Cmd-handler task
 * *************************************************************************************************/

static void
cmdHandlerTask(void* parameters)
{
    auto* pTestLeds = static_cast<TestLeds*>(parameters);
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
                leds_init(false);
                break;
            case MainTaskCmd_leds_notify_nrf52_fw_check:
                leds_notify_nrf52_fw_check();
                break;
            case MainTaskCmd_leds_notify_nrf52_ready:
                leds_notify_nrf52_ready();
                break;
            case MainTaskCmd_leds_notify_cfg_ready:
                event_mgr_notify(EVENT_MGR_EV_GW_CFG_READY);
                break;
            case MainTaskCmd_leds_notify_cfg_changed_ruuvi:
                event_mgr_notify(EVENT_MGR_EV_GW_CFG_CHANGED_RUUVI);
                break;
            case MainTaskCmd_leds_notify_wifi_connected:
                event_mgr_notify(EVENT_MGR_EV_WIFI_CONNECTED);
                break;
            case MainTaskCmd_leds_notify_http1_data_sent_successfully:
                leds_notify_http1_data_sent_successfully();
                break;
            case MainTaskCmd_leds_notify_http1_data_sent_fail:
                leds_notify_http1_data_sent_fail();
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

static void*
freertosStartup(void* arg)
{
    auto* pObj = static_cast<TestLeds*>(arg);
    disableCheckingIfCurThreadIsFreeRTOS();
    BaseType_t res = xTaskCreate(
        cmdHandlerTask,
        "cmdHandlerTask",
        configMINIMAL_STACK_SIZE,
        pObj,
        (tskIDLE_PRIORITY + 1),
        (xTaskHandle*)nullptr);
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

#define TEST_LEDC_SET_FADE_AND_START(target_duty_, pEv, idx) \
    { \
        auto* pEv = reinterpret_cast<TestEventLedcSetFadeWithTime*>(testEvents[idx++]); \
        ASSERT_EQ(TestEventType_LedcSetFadeWithTime, pEv->eventType); \
        ASSERT_EQ(LEDC_HIGH_SPEED_MODE, pEv->speed_mode); \
        ASSERT_EQ(LEDC_CHANNEL_0, pEv->channel); \
        ASSERT_EQ(target_duty_, pEv->target_duty); \
        ASSERT_EQ(25, pEv->max_fade_time_ms); \
    } \
    { \
        auto* pEv = reinterpret_cast<TestEventLedcFadeStart*>(testEvents[idx++]); \
        ASSERT_EQ(TestEventType_LedcFadeStart, pEv->eventType); \
        ASSERT_EQ(LEDC_HIGH_SPEED_MODE, pEv->speed_mode); \
        ASSERT_EQ(LEDC_CHANNEL_0, pEv->channel); \
        ASSERT_EQ(LEDC_FADE_WAIT_DONE, pEv->fade_mode); \
    }

/*** Unit-Tests
 * *******************************************************************************************************/

TEST_F(TestLeds, test_all) // NOLINT
{
    cmdQueue.push_and_wait(MainTaskCmd_LedsInit);

    ASSERT_EQ(LEDS_CTRL_STATE_AFTER_REBOOT, leds_ctrl_get_state());

    int idx = 0;
    ASSERT_EQ(idx + 4, testEvents.size());
    {
        auto* pEv = reinterpret_cast<TestEventLedcTimerConfig*>(testEvents[idx++]);
        ASSERT_EQ(TestEventType_LedcTimerConfig, pEv->eventType);
        ASSERT_EQ(LEDC_HIGH_SPEED_MODE, pEv->speed_mode);
        ASSERT_EQ(LEDC_TIMER_10_BIT, pEv->duty_resolution);
        ASSERT_EQ(LEDC_TIMER_1, pEv->timer_num);
        ASSERT_EQ(5000, pEv->freq_hz);
        ASSERT_EQ(LEDC_AUTO_CLK, pEv->clk_cfg);
    }
    {
        auto* pEv = reinterpret_cast<TestEventLedcChannelConfig*>(testEvents[idx++]);
        ASSERT_EQ(TestEventType_LedcChannelConfig, pEv->eventType);
        ASSERT_EQ(LEDC_CHANNEL_0, pEv->channel);
        ASSERT_EQ(0, pEv->duty);
        ASSERT_EQ(13, pEv->gpio_num);
        ASSERT_EQ(LEDC_HIGH_SPEED_MODE, pEv->speed_mode);
        ASSERT_EQ(0, pEv->hpoint);
        ASSERT_EQ(LEDC_TIMER_1, pEv->timer_sel);
    }
    {
        auto* pEv = reinterpret_cast<TestEventLedcFadeFuncInstall*>(testEvents[idx++]);
        ASSERT_EQ(TestEventType_LedcFadeFuncInstall, pEv->eventType);
        ASSERT_EQ(0, pEv->intr_alloc_flags);
    }
    {
        auto* pEv = reinterpret_cast<TestEventLedcStop*>(testEvents[idx++]);
        ASSERT_EQ(TestEventType_LedcStop, pEv->eventType);
        ASSERT_EQ(0, pEv->channel);
    }
    ASSERT_EQ(idx, testEvents.size());

    ASSERT_EQ(LEDS_CTRL_STATE_AFTER_REBOOT, leds_ctrl_get_state());
    cmdQueue.push_and_wait(MainTaskCmd_leds_notify_nrf52_fw_check);
    msleep(20);
    ASSERT_EQ(LEDS_CTRL_STATE_CHECKING_NRF52_FW, leds_ctrl_get_state());
    ASSERT_EQ(idx, testEvents.size());

    cmdQueue.push_and_wait(MainTaskCmd_leds_notify_nrf52_ready);
    ASSERT_EQ(LEDS_CTRL_STATE_WAITING_CFG_READY, leds_ctrl_get_state());
    ASSERT_EQ(idx, testEvents.size());

    cmdQueue.push_and_wait(MainTaskCmd_leds_notify_cfg_ready);
    ASSERT_EQ(LEDS_CTRL_STATE_SUBSTATE, leds_ctrl_get_state());
    ASSERT_EQ(idx, testEvents.size());

    cmdQueue.push_and_wait(MainTaskCmd_leds_notify_cfg_changed_ruuvi);
    ASSERT_EQ(LEDS_CTRL_STATE_SUBSTATE, leds_ctrl_get_state());
    ASSERT_EQ(idx, testEvents.size());

    ASSERT_EQ(string(LEDS_BLINKING_AFTER_REBOOT), string(g_leds_blinking_mode.p_sequence));
    ASSERT_EQ(0, g_leds_blinking_sequence_idx);

    for (int i = 0; i < 4; ++i)
    {
        ASSERT_TRUE(waitEvent(std::chrono::milliseconds(150)));
        ASSERT_EQ(idx + 2, testEvents.size());
        TEST_LEDC_SET_FADE_AND_START(767, pEv, idx);
        ASSERT_EQ(LEDS_CTRL_STATE_SUBSTATE, leds_ctrl_get_state());

        ASSERT_TRUE(waitEvent(std::chrono::milliseconds(150)));
        ASSERT_EQ(idx + 2, testEvents.size());
        TEST_LEDC_SET_FADE_AND_START(1024, pEv, idx);
        ASSERT_EQ(LEDS_CTRL_STATE_SUBSTATE, leds_ctrl_get_state());
    }

    ASSERT_EQ(0, g_leds_blinking_sequence_idx);

    cmdQueue.push_and_wait(MainTaskCmd_leds_notify_wifi_connected);
    ASSERT_EQ(LEDS_CTRL_STATE_SUBSTATE, leds_ctrl_get_state());
    ASSERT_EQ(idx, testEvents.size());
    ASSERT_EQ(string(LEDS_BLINKING_MODE_CONNECTED_TO_ALL_TARGETS), string(g_leds_blinking_mode.p_sequence));

    ASSERT_TRUE(waitEvent(std::chrono::milliseconds(150)));
    ASSERT_EQ(idx + 1, testEvents.size());
    {
        auto* pEv = reinterpret_cast<TestEventGreenLedTurnOn*>(testEvents[idx++]);
        ASSERT_EQ(TestEventType_GreenLedTurnOn, pEv->eventType);
    }
    ASSERT_EQ(LEDS_CTRL_STATE_SUBSTATE, leds_ctrl_get_state());

    ASSERT_TRUE(waitEvent(std::chrono::milliseconds(500)));
    ASSERT_EQ(idx + 1, testEvents.size());
    {
        auto* pEv = reinterpret_cast<TestEventGreenLedTurnOn*>(testEvents[idx++]);
        ASSERT_EQ(TestEventType_TaskWdtReset, pEv->eventType);
    }
    ASSERT_EQ(LEDS_CTRL_STATE_SUBSTATE, leds_ctrl_get_state());

    cmdQueue.push_and_wait(MainTaskCmd_leds_notify_http1_data_sent_fail);
    ASSERT_EQ(LEDS_CTRL_STATE_SUBSTATE, leds_ctrl_get_state());
    ASSERT_EQ(idx, testEvents.size());
    ASSERT_TRUE(waitEvent(std::chrono::milliseconds(200)));
    ASSERT_EQ(idx + 3, testEvents.size());
    {
        auto* pEv = reinterpret_cast<TestEventGreenLedTurnOff*>(testEvents[idx++]);
        ASSERT_EQ(TestEventType_GreenLedTurnOff, pEv->eventType);
    }
    TEST_LEDC_SET_FADE_AND_START(767, pEv, idx);
    ASSERT_EQ(idx, testEvents.size());
    ASSERT_EQ(string(LEDS_BLINKING_MODE_NOT_CONNECTED_TO_ANY_TARGET), string(g_leds_blinking_mode.p_sequence));
}
