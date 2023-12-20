#ifndef TEST_EVENTS_H
#define TEST_EVENTS_H

typedef enum TestEventType_Tag
{
    TestEventType_GreenLedStateChanged,
    TestEventType_LedcTimerConfig,
    TestEventType_LedcChannelConfig,
    TestEventType_LedcSetFadeWithTime,
    TestEventType_LedcFadeStart,
    TestEventType_LedcFadeFuncInstall,
    TestEventType_LedcStop,
    TestEventType_TaskWdtReset,
} TestEventType_e;

/*** Test-Events classes implementation
 * *******************************************************************************/

class TestEvent
{
public:
    TestEventType_e eventType;

    explicit TestEvent(const TestEventType_e eventType)
        : eventType(eventType)
    {
    }
};

class TestEventGreenLedStateChanged : public TestEvent
{
public:
    explicit TestEventGreenLedStateChanged()
        : TestEvent(TestEventType_GreenLedStateChanged)
    {
    }
};

class TestEventLedcTimerConfig : public TestEvent
{
public:
    ledc_mode_t      speed_mode;
    ledc_timer_bit_t duty_resolution;
    ledc_timer_t     timer_num;
    uint32_t         freq_hz;
    ledc_clk_cfg_t   clk_cfg;

    explicit TestEventLedcTimerConfig(const ledc_timer_config_t* timer_conf)
        : TestEvent(TestEventType_LedcTimerConfig)
        , speed_mode(timer_conf->speed_mode)
        , duty_resolution(timer_conf->duty_resolution)
        , timer_num(timer_conf->timer_num)
        , freq_hz(timer_conf->freq_hz)
        , clk_cfg(timer_conf->clk_cfg)
    {
    }
};

class TestEventLedcChannelConfig : public TestEvent
{
public:
    int              gpio_num;
    ledc_mode_t      speed_mode;
    ledc_channel_t   channel;
    ledc_intr_type_t intr_type;
    ledc_timer_t     timer_sel;
    uint32_t         duty;
    int              hpoint;

    explicit TestEventLedcChannelConfig(const ledc_channel_config_t* ledc_conf)
        : TestEvent(TestEventType_LedcChannelConfig)
        , gpio_num(ledc_conf->gpio_num)
        , speed_mode(ledc_conf->speed_mode)
        , channel(ledc_conf->channel)
        , intr_type(ledc_conf->intr_type)
        , timer_sel(ledc_conf->timer_sel)
        , duty(ledc_conf->duty)
        , hpoint(ledc_conf->hpoint)
    {
    }
};

class TestEventLedcSetFadeWithTime : public TestEvent
{
public:
    ledc_mode_t    speed_mode;
    ledc_channel_t channel;
    uint32_t       target_duty;
    int            max_fade_time_ms;

    TestEventLedcSetFadeWithTime(
        ledc_mode_t    speed_mode,
        ledc_channel_t channel,
        uint32_t       target_duty,
        int            max_fade_time_ms)
        : TestEvent(TestEventType_LedcSetFadeWithTime)
        , speed_mode(speed_mode)
        , channel(channel)
        , target_duty(target_duty)
        , max_fade_time_ms(max_fade_time_ms)
    {
    }
};

class TestEventLedcFadeStart : public TestEvent
{
public:
    ledc_mode_t      speed_mode;
    ledc_channel_t   channel;
    ledc_fade_mode_t fade_mode;

    TestEventLedcFadeStart(ledc_mode_t speed_mode, ledc_channel_t channel, ledc_fade_mode_t fade_mode)
        : TestEvent(TestEventType_LedcFadeStart)
        , speed_mode(speed_mode)
        , channel(channel)
        , fade_mode(fade_mode)
    {
    }
};

class TestEventLedcFadeFuncInstall : public TestEvent
{
public:
    int intr_alloc_flags;

    explicit TestEventLedcFadeFuncInstall(int intr_alloc_flags)
        : TestEvent(TestEventType_LedcFadeFuncInstall)
        , intr_alloc_flags(intr_alloc_flags)
    {
    }
};

class TestEventLedcStop : public TestEvent
{
public:
    ledc_mode_t    speed_mode;
    ledc_channel_t channel;
    uint32_t       idle_level;

    explicit TestEventLedcStop(ledc_mode_t speed_mode, ledc_channel_t channel, uint32_t idle_level)
        : TestEvent(TestEventType_LedcStop)
        , speed_mode(speed_mode)
        , channel(channel)
        , idle_level(idle_level)
    {
    }
};

class TestEventTaskWdtReset : public TestEvent
{
public:
    explicit TestEventTaskWdtReset()
        : TestEvent(TestEventType_TaskWdtReset)
    {
    }
};

#endif // TEST_EVENTS_H
