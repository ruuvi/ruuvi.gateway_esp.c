/**
 * @file gpio.c
 * @author Jukka Saari
 * @date 2019-11-27
 * @copyright Ruuvi Innovations Ltd, license BSD-3-Clause.
 */

#include "driver/gpio.h"
#include "driver/timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/portmacro.h"
#include "freertos/projdefs.h"
#include "freertos/queue.h"
#include "gpio.h"
#include "http_server.h"
#include "ruuvi_boards.h"
#include "ruuvi_gateway.h"
#include "attribs.h"
#include "log.h"

#define CONFIG_WIFI_RESET_BUTTON_GPIO (RB_BUTTON_RESET_PIN)

#define TIMER_DIVIDER         (16)
#define TIMER_SCALE           (TIMER_BASE_CLK / TIMER_DIVIDER) /*!< used to calculate counter value */
#define ESP_INTR_FLAG_DEFAULT (0)

typedef int gpio_level_t;

typedef struct gpio_event_t
{
    gpio_num_t gpio_num;
} gpio_event_t;

static xQueueHandle gp_gpio_evt_queue;

static const char TAG[] = "gpio";

static void IRAM_ATTR
gpio_reset_button_handler_isr(void *p_arg)
{
    (void)p_arg;
    const gpio_event_t gpio_evt = {
        .gpio_num = CONFIG_WIFI_RESET_BUTTON_GPIO,
    };
    xQueueSendFromISR(gp_gpio_evt_queue, &gpio_evt, NULL);
}

static void IRAM_ATTR
gpio_timer_isr(void *p_arg)
{
    (void)p_arg;
    BaseType_t is_higher_priority_task_woken = pdFALSE;

    const BaseType_t result = xEventGroupSetBitsFromISR(status_bits, RESET_BUTTON_BIT, &is_higher_priority_task_woken);

    TIMERG0.int_clr_timers.t0 = 1;

    if ((pdPASS == result) && (pdFALSE != is_higher_priority_task_woken))
    {
        portYIELD_FROM_ISR();
    }
}

static void
gpio_config_timer(void)
{
    /* Select and initialize basic parameters of the timer */
    timer_config_t config;
    config.divider     = TIMER_DIVIDER;
    config.counter_dir = TIMER_COUNT_UP;
    config.counter_en  = TIMER_PAUSE;
    config.alarm_en    = TIMER_ALARM_EN;
    config.intr_type   = TIMER_INTR_LEVEL;
    config.auto_reload = 0;
    timer_init(TIMER_GROUP_0, TIMER_0, &config);
    /* Timer's counter will initially start from value below.
    Also, if auto_reload is set, this value will be automatically reload on alarm
    */
    timer_set_counter_value(TIMER_GROUP_0, TIMER_0, 0x00000000ULL);
    /* Configure the alarm value and the interrupt on alarm. */
    const uint32_t timeout_interval_seconds = 3;
    timer_set_alarm_value(TIMER_GROUP_0, TIMER_0, timeout_interval_seconds * TIMER_SCALE);
    timer_enable_intr(TIMER_GROUP_0, TIMER_0);
    timer_isr_register(TIMER_GROUP_0, TIMER_0, &gpio_timer_isr, (void *)TIMER_0, ESP_INTR_FLAG_IRAM, NULL);
}

static void
gpio_task_handle_reset_button(const gpio_level_t io_level, bool *p_is_timer_started)
{
    if ((!*p_is_timer_started) && (0 == io_level))
    {
        LOG_DBG("Button pressed");
        // Start the timer
        timer_start(TIMER_GROUP_0, TIMER_0);
        *p_is_timer_started = true;
    }
    else
    {
        // Stop and reinitialize the timer
        LOG_DBG("Button released");
        gpio_config_timer();
        *p_is_timer_started = false;
        http_server_start();
    }
}

ATTR_NORETURN
static void
gpio_task(void *p_arg)
{
    (void)p_arg;
    bool flag_timer_started = false;
    for (;;)
    {
        gpio_event_t gpio_evt = {
            .gpio_num = GPIO_NUM_NC,
        };
        if (pdPASS != xQueueReceive(gp_gpio_evt_queue, &gpio_evt, portMAX_DELAY))
        {
            continue;
        }
        if (CONFIG_WIFI_RESET_BUTTON_GPIO == gpio_evt.gpio_num)
        {
            gpio_task_handle_reset_button(gpio_get_level(gpio_evt.gpio_num), &flag_timer_started);
        }
    }
}

static bool
gpio_config_input_reset_button(void)
{
    /* INPUT GPIO WIFI_RESET_BUTTON */
    static const gpio_config_t io_conf_reset_button = {
        .pin_bit_mask = (1ULL << (uint32_t)CONFIG_WIFI_RESET_BUTTON_GPIO),
        .mode         = GPIO_MODE_INPUT,
        .pull_up_en   = 1,
        .pull_down_en = 0,
        .intr_type    = GPIO_INTR_ANYEDGE,
    };
    const esp_err_t err = gpio_config(&io_conf_reset_button);
    if (ESP_OK != err)
    {
        LOG_ERR("%s failed, err=%d", "gpio_config", err);
        return false;
    }
    return true;
}

static bool
gpio_config_output_lna_crx(void)
{
    /* OUTPUT GPIO LNA_CRX */
    const gpio_config_t io_conf_lna_crx = {
        .pin_bit_mask = 1ULL << (uint32_t)RB_GWBUS_LNA,
        .mode         = GPIO_MODE_OUTPUT,
        .pull_up_en   = 0,
        .pull_down_en = 0,
        .intr_type    = GPIO_INTR_DISABLE,
    };
    const esp_err_t err = gpio_config(&io_conf_lna_crx);
    if (ESP_OK != err)
    {
        LOG_ERR("%s failed, err=%d", "gpio_config", err);
        return false;
    }
    return true;
}

static bool
gpio_init_evt_queue(void)
{
    // create a queue to handle gpio event from ISR
    const UBaseType_t queue_len = 10;
    gp_gpio_evt_queue           = xQueueCreate(queue_len, sizeof(gpio_event_t));
    if (NULL == gp_gpio_evt_queue)
    {
        LOG_ERR("%s failed", "xQueueCreate");
        return false;
    }
    return true;
}

static bool
gpio_init_install_isr_service(void)
{
    const esp_err_t err = gpio_install_isr_service(ESP_INTR_FLAG_DEFAULT);
    if (ESP_OK != err)
    {
        LOG_ERR("%s failed, err=%d", "gpio_install_isr_service", err);
        return false;
    }
    return true;
}

static bool
gpio_hook_isr_for_reset_button(void)
{
    const esp_err_t err = gpio_isr_handler_add(CONFIG_WIFI_RESET_BUTTON_GPIO, &gpio_reset_button_handler_isr, NULL);
    if (ESP_OK != err)
    {
        LOG_ERR("%s failed, err=%d", "gpio_isr_handler_add", err);
        return false;
    }
    return true;
}

static bool
gpio_start_task(void)
{
    const uint32_t    stack_size    = 3U * 1024U;
    const UBaseType_t task_priority = 1U;
    const BaseType_t  res           = xTaskCreate(&gpio_task, "gpio_task", stack_size, NULL, task_priority, NULL);
    if (pdPASS != res)
    {
        LOG_ERR("%s failed, err=%d", "xTaskCreate", res);
        return false;
    }
    return true;
}

void
gpio_init(void)
{
    esp_log_level_set(TAG, ESP_LOG_DEBUG);

    if (!gpio_config_input_reset_button())
    {
        return;
    }
    if (!gpio_config_output_lna_crx())
    {
        return;
    }
    if (!gpio_init_evt_queue())
    {
        return;
    }

    gpio_config_timer();

    if (!gpio_init_install_isr_service())
    {
        return;
    }

    if (!gpio_hook_isr_for_reset_button())
    {
        return;
    }

    if (!gpio_start_task())
    {
        return;
    }
}
