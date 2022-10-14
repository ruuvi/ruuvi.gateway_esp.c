/**
 * @file gpio.c
 * @author Jukka Saari
 * @date 2019-11-27
 * @copyright Ruuvi Innovations Ltd, license BSD-3-Clause.
 */

#include "driver/gpio.h"
#include "gpio.h"
#include "ruuvi_boards.h"
#include "reset_task.h"
#include "gpio_switch_ctrl.h"

#define LOG_LOCAL_LEVEL LOG_LEVEL_DEBUG
#include "log.h"

#define CONFIG_WIFI_RESET_BUTTON_GPIO (RB_BUTTON_RESET_PIN)

#define ESP_INTR_FLAG_DEFAULT (0)

static const char TAG[] = "gpio";

static void IRAM_ATTR
gpio_configure_button_handler_isr(void* p_arg)
{
    (void)p_arg;
    static bool g_gpio_is_configure_button_pressed = false;

    if (0 == gpio_get_level(CONFIG_WIFI_RESET_BUTTON_GPIO))
    {
        if (!g_gpio_is_configure_button_pressed)
        {
            g_gpio_is_configure_button_pressed = true;
            reset_task_notify_configure_button_pressed();
        }
    }
    else
    {
        if (g_gpio_is_configure_button_pressed)
        {
            g_gpio_is_configure_button_pressed = false;
            reset_task_notify_configure_button_released();
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
gpio_hook_isr_for_configure_button(void)
{
    const esp_err_t err = gpio_isr_handler_add(CONFIG_WIFI_RESET_BUTTON_GPIO, &gpio_configure_button_handler_isr, NULL);
    if (ESP_OK != err)
    {
        LOG_ERR("%s failed, err=%d", "gpio_isr_handler_add", err);
        return false;
    }
    return true;
}

void
gpio_init(void)
{

    gpio_switch_ctrl_init();
    if (!gpio_config_input_reset_button())
    {
        return;
    }
    if (!gpio_config_output_lna_crx())
    {
        return;
    }

    if (!gpio_init_install_isr_service())
    {
        return;
    }

    if (!gpio_hook_isr_for_configure_button())
    {
        return;
    }
}
