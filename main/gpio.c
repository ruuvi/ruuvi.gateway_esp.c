#include "freertos/FreeRTOS.h"
#include "freertos/projdefs.h"
#include "freertos/queue.h"
#include "freertos/event_groups.h"
#include "freertos/portmacro.h"
#include "driver/gpio.h"
#include "driver/timer.h"
#include "gpio.h"
#include "esp_log.h"
#include "ruuvidongle.h"
#include "leds.h"
#include "ruuvi_board_gwesp.h"

#define CONFIG_WIFI_RESET_BUTTON_GPIO   RB_BUTTON_RESET_PIN
#define GPIO_WIFI_RESET_BUTTON_MASK     (1ULL<<CONFIG_WIFI_RESET_BUTTON_GPIO)

#define TIMER_DIVIDER 16
#define TIMER_SCALE    (TIMER_BASE_CLK / TIMER_DIVIDER)  /*!< used to calculate counter value */
#define ESP_INTR_FLAG_DEFAULT 0

static xQueueHandle gpio_evt_queue = NULL;
extern EventGroupHandle_t status_bits;

static const char TAG[] = "gpio";

static void IRAM_ATTR gpio_isr_handler (void * arg)
{
    uint32_t gpio_num = (uint32_t) arg;
    xQueueSendFromISR (gpio_evt_queue, &gpio_num, NULL);
}

static void IRAM_ATTR timer_isr (void * para)
{
    BaseType_t xHigherPriorityTaskWoken, result;
    xHigherPriorityTaskWoken = pdFALSE;
    result = xEventGroupSetBitsFromISR (status_bits, RESET_BUTTON_BIT,
                                        &xHigherPriorityTaskWoken);
    TIMERG0.int_clr_timers.t0 = 1;

    if (result == pdPASS)
    {
        portYIELD_FROM_ISR();
    }
}

static void config_timer (void)
{
    /* Select and initialize basic parameters of the timer */
    timer_config_t config;
    config.divider = TIMER_DIVIDER;
    config.counter_dir = TIMER_COUNT_UP;
    config.counter_en = TIMER_PAUSE;
    config.alarm_en = TIMER_ALARM_EN;
    config.intr_type = TIMER_INTR_LEVEL;
    config.auto_reload = 0;
    timer_init (TIMER_GROUP_0, TIMER_0, &config);
    /* Timer's counter will initially start from value below.
    Also, if auto_reload is set, this value will be automatically reload on alarm */
    timer_set_counter_value (TIMER_GROUP_0, TIMER_0, 0x00000000ULL);
    /* Configure the alarm value and the interrupt on alarm. */
    timer_set_alarm_value (TIMER_GROUP_0, TIMER_0, 3 * TIMER_SCALE); // 3 seconds interval
    timer_enable_intr (TIMER_GROUP_0, TIMER_0);
    timer_isr_register (TIMER_GROUP_0, TIMER_0, timer_isr, (void *) TIMER_0,
                        ESP_INTR_FLAG_IRAM,
                        NULL);
}

static void gpio_task (void * arg)
{
    uint32_t io_num;
    char timer_started = 0;

    for (;;)
    {
        if (xQueueReceive (gpio_evt_queue, &io_num, portMAX_DELAY))
        {
            if (io_num == CONFIG_WIFI_RESET_BUTTON_GPIO)
            {
                if (timer_started == 0 && gpio_get_level (io_num) == 0)
                {
                    ESP_LOGD (TAG, "Button pressed");
                    // Start the timer
                    timer_start (TIMER_GROUP_0, TIMER_0);
                    timer_started = 1;
                }
                else
                {
                    // Stop and reinitialize the timer
                    ESP_LOGD (TAG, "Button released");
                    config_timer();
                    timer_started = 0;
                }
            }
        }
    }
}

void gpio_init (void)
{
    esp_log_level_set (TAG, ESP_LOG_DEBUG);
    gpio_config_t io_conf;
    /*INPUT GPIO WIFI_RESET_BUTTON  -------------------------------------*/
    //interrupt of rising edge
    io_conf.intr_type = GPIO_PIN_INTR_ANYEDGE; //GPIO_PIN_INTR_POSEDGE; Rizwan
    //bit mask of the pins
    io_conf.pin_bit_mask = GPIO_WIFI_RESET_BUTTON_MASK;
    //set as input mode
    io_conf.mode = GPIO_MODE_INPUT;
    //disable pull-up mode
    io_conf.pull_up_en = 0;
    gpio_config (&io_conf);
    /*-------------------------------------------------------------------*/
    //create a queue to handle gpio event from isr
    gpio_evt_queue = xQueueCreate (10, sizeof (uint32_t));
    config_timer();
    //install gpio isr service
    gpio_install_isr_service (ESP_INTR_FLAG_DEFAULT);
    //hook isr handler for specific gpio pin
    gpio_isr_handler_add (CONFIG_WIFI_RESET_BUTTON_GPIO, gpio_isr_handler,
                          (void *) CONFIG_WIFI_RESET_BUTTON_GPIO);
    xTaskCreate (gpio_task, "gpio_task", 3072, NULL, 1, NULL);
}
