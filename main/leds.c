/**
 * @file leds.c
 * @author Jukka Saari
 * @date 2019-11-27
 * @copyright Ruuvi Innovations Ltd, license BSD-3-Clause.
 */

#include "leds.h"
#include <stdbool.h>
#include <stdio.h>
#include "driver/ledc.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/projdefs.h"
#include "freertos/task.h"
#include "attribs.h"
#include "time_units.h"

const char *TAG = "LEDS";

EventGroupHandle_t led_bits = NULL;

#define LED_PIN 23

#define LEDC_HS_TIMER       LEDC_TIMER_1
#define LEDC_HS_MODE        LEDC_HIGH_SPEED_MODE
#define LEDC_HS_CH0_GPIO    LED_PIN
#define LEDC_HS_CH0_CHANNEL LEDC_CHANNEL_0

#define LEDC_LS_TIMER LEDC_TIMER_2
#define LEDC_LS_MODE  LEDC_LOW_SPEED_MODE

#define LEDC_TEST_CH_NUM    (4)
#define LEDC_TEST_DUTY      (1023)
#define LEDC_TEST_FADE_TIME (50)

ledc_channel_config_t ledc_channel[1] = {
    {
        .gpio_num   = LEDC_HS_CH0_GPIO,
        .speed_mode = LEDC_HS_MODE,
        .channel    = LEDC_HS_CH0_CHANNEL,
        .intr_type  = LEDC_INTR_DISABLE,
        .timer_sel  = LEDC_HS_TIMER,
        .duty       = 0,
        .hpoint     = 0,
    },
};

esp_timer_handle_t blink_timer;
static bool        led_state = false;

#if !defined(RUUVI_LEDS_DISABLE)
#define RUUVI_LEDS_DISABLE (0)
#endif
static bool g_leds_enabled = (0 == RUUVI_LEDS_DISABLE);

void
leds_on(void)
{
    if (!g_leds_enabled)
    {
        return;
    }
    ledc_set_fade_with_time(ledc_channel[0].speed_mode, ledc_channel[0].channel, LEDC_TEST_DUTY, LEDC_TEST_FADE_TIME);
    ledc_fade_start(ledc_channel[0].speed_mode, ledc_channel[0].channel, LEDC_FADE_NO_WAIT);
}

void
leds_off(void)
{
    if (!g_leds_enabled)
    {
        return;
    }
    ledc_set_fade_with_time(ledc_channel[0].speed_mode, ledc_channel[0].channel, 0, LEDC_TEST_FADE_TIME);
    ledc_fade_start(ledc_channel[0].speed_mode, ledc_channel[0].channel, LEDC_FADE_NO_WAIT);
}

static void
blink_timer_handler(void *arg)
{
    (void)arg;
    if (led_state)
    {
        leds_off();
    }
    else
    {
        leds_on();
    }

    led_state = !led_state;
}

void
leds_start_blink(const TimeUnitsMilliSeconds_t interval_ms)
{
    if (!g_leds_enabled)
    {
        return;
    }
    ESP_LOGI(TAG, "start led blinking, interval: %u ms", (unsigned)interval_ms);
    esp_timer_start_periodic(blink_timer, time_units_conv_ms_to_us(interval_ms));
}

void
leds_stop_blink(void)
{
    if (!g_leds_enabled)
    {
        return;
    }
    ESP_LOGI(TAG, "stop led blinking");
    esp_timer_stop(blink_timer);
    ledc_set_fade_with_time(ledc_channel[0].speed_mode, ledc_channel[0].channel, 0, LEDC_TEST_FADE_TIME);
    ledc_fade_start(ledc_channel[0].speed_mode, ledc_channel[0].channel, LEDC_FADE_NO_WAIT);
    led_state = false;
}

ATTR_NORETURN
static void
leds_task(void *arg)
{
    (void)arg;
    ESP_LOGI(TAG, "%s started", __func__);

    EventBits_t bits;
    while (1)
    {
        bits = xEventGroupWaitBits(
            led_bits,
            LEDS_ON_BIT | LEDS_OFF_BIT | LEDS_BLINK_BIT,
            pdTRUE,
            pdFALSE,
            pdMS_TO_TICKS(1000));
        if (0 != (bits & LEDS_BLINK_BIT))
        {
            ESP_LOGI(TAG, "led blink");
            const TimeUnitsMilliSeconds_t blink_interval_ms = time_units_conv_seconds_to_ms(1);
            leds_start_blink(blink_interval_ms);
        }
        else if (0 != (bits & LEDS_ON_BIT))
        {
            ESP_LOGI(TAG, "led on");
            leds_on();
        }
        else if (0 != (bits & LEDS_OFF_BIT))
        {
            ESP_LOGI(TAG, "led off");
            leds_off();
        }
        else
        {
            // MISRA C:2012, 15.7 - All if...else if constructs shall be terminated with an else statement
        }
    }
}

void
leds_init(void)
{
    if (!g_leds_enabled)
    {
        ESP_LOGI(TAG, "%s: LEDs disabled", __func__);
        return;
    }
    ESP_LOGI(TAG, "%s", __func__);
    esp_timer_init();
    esp_timer_create_args_t timer_args = {
        .callback        = blink_timer_handler,
        .arg             = NULL,
        .dispatch_method = ESP_TIMER_TASK,
        .name            = "blink_timer",
    };
    esp_timer_create(&timer_args, &blink_timer);

    ledc_timer_config_t ledc_timer = {
        .duty_resolution = LEDC_TIMER_10_BIT, // resolution of PWM duty
        .freq_hz         = 5000,              // frequency of PWM signal
        .speed_mode      = LEDC_HS_MODE,      // timer mode
        .timer_num       = LEDC_HS_TIMER,     // timer index
        .clk_cfg         = LEDC_AUTO_CLK,     // Auto select the source clock
    };

    ledc_timer_config(&ledc_timer);

    ledc_channel_config(&ledc_channel[0]);

    ledc_fade_func_install(0);

    led_bits = xEventGroupCreate();
    if (led_bits == NULL)
    {
        ESP_LOGE(TAG, "Can't create event group");
    }

    const uint32_t stack_size = 2U * 1024U;
    xTaskCreate(&leds_task, "leds_task", stack_size, NULL, 1, NULL);
}
