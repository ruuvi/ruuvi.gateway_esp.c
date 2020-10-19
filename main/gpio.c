#include "driver/gpio.h"
#include "driver/timer.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/portmacro.h"
#include "freertos/projdefs.h"
#include "freertos/queue.h"
#include "gpio.h"
#include "http_server.h"
#include "leds.h"
#include "ruuvi_boards.h"
#include "ruuvi_gateway.h"

#define CONFIG_WIFI_RESET_BUTTON_GPIO RB_BUTTON_RESET_PIN

#define TIMER_DIVIDER         16
#define TIMER_SCALE           (TIMER_BASE_CLK / TIMER_DIVIDER) /*!< used to calculate counter value */
#define ESP_INTR_FLAG_DEFAULT 0

static xQueueHandle       gpio_evt_queue = NULL;
extern EventGroupHandle_t status_bits;

static const char TAG[] = "gpio";

static void IRAM_ATTR
gpio_isr_handler(void *arg)
{
    uint32_t gpio_num = (uint32_t)arg;
    xQueueSendFromISR(gpio_evt_queue, &gpio_num, NULL);
}

static void IRAM_ATTR
timer_isr(void *para)
{
    BaseType_t xHigherPriorityTaskWoken, result;
    xHigherPriorityTaskWoken  = pdFALSE;
    result                    = xEventGroupSetBitsFromISR(status_bits, RESET_BUTTON_BIT, &xHigherPriorityTaskWoken);
    TIMERG0.int_clr_timers.t0 = 1;

    if (result == pdPASS)
    {
        portYIELD_FROM_ISR();
    }
}

static void
config_timer(void)
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
    timer_set_alarm_value(TIMER_GROUP_0, TIMER_0,
                          3 * TIMER_SCALE); // 3 seconds interval
    timer_enable_intr(TIMER_GROUP_0, TIMER_0);
    timer_isr_register(TIMER_GROUP_0, TIMER_0, timer_isr, (void *)TIMER_0, ESP_INTR_FLAG_IRAM, NULL);
}

_Noreturn static void
gpio_task(void *arg)
{
    uint32_t io_num;
    char     timer_started = 0;

    for (;;)
    {
        if (xQueueReceive(gpio_evt_queue, &io_num, portMAX_DELAY))
        {
            if (io_num == CONFIG_WIFI_RESET_BUTTON_GPIO)
            {
                if (timer_started == 0 && gpio_get_level(io_num) == 0)
                {
                    ESP_LOGD(TAG, "Button pressed");
                    // Start the timer
                    timer_start(TIMER_GROUP_0, TIMER_0);
                    timer_started = 1;
                }
                else
                {
                    // Stop and reinitialize the timer
                    ESP_LOGD(TAG, "Button released");
                    config_timer();
                    timer_started = 0;
                    http_server_start();
                }
            }
        }
    }
}

void
gpio_init(void)
{
    esp_log_level_set(TAG, ESP_LOG_DEBUG);

    /*INPUT GPIO WIFI_RESET_BUTTON  -------------------------------------*/
    {
        static const gpio_config_t io_conf_reset_button = {
            .pin_bit_mask = (1ULL << (unsigned)CONFIG_WIFI_RESET_BUTTON_GPIO),
            .mode         = GPIO_MODE_INPUT,
            .pull_up_en   = 1,
            .pull_down_en = 0,
            .intr_type    = GPIO_INTR_ANYEDGE,
        };
        const esp_err_t err = gpio_config(&io_conf_reset_button);
        if (ESP_OK != err)
        {
            ESP_LOGE(TAG, "gpio_config failed for '%s', res=%d", "reset button", err);
        }
    }

    /* OUTPUT GPIO LNA_CRX  -------------------------------------*/
    {
        const gpio_config_t io_conf_lna_crx = {
            .pin_bit_mask = 1ULL << (unsigned)RB_GWBUS_LNA,
            .mode         = GPIO_MODE_OUTPUT,
            .pull_up_en   = 0,
            .pull_down_en = 0,
            .intr_type    = GPIO_INTR_DISABLE,
        };
        const esp_err_t err = gpio_config(&io_conf_lna_crx);
        if (ESP_OK != err)
        {
            ESP_LOGE(TAG, "gpio_config failed for '%s', res=%d", "bluetooth LNA CRX", err);
        }
    }

    /*-------------------------------------------------------------------*/
    // create a queue to handle gpio event from isr
    gpio_evt_queue = xQueueCreate(10, sizeof(uint32_t));
    config_timer();
    // install gpio isr service
    gpio_install_isr_service(ESP_INTR_FLAG_DEFAULT);
    // hook isr handler for specific gpio pin
    gpio_isr_handler_add(CONFIG_WIFI_RESET_BUTTON_GPIO, gpio_isr_handler, (void *)CONFIG_WIFI_RESET_BUTTON_GPIO);
    xTaskCreate(gpio_task, "gpio_task", 3072, NULL, 1, NULL);
}
