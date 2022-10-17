/**
 * @file gpio_switch_ctrl.c
 * @author TheSomeMan
 * @date 2021-04-03
 * @copyright Ruuvi Innovations Ltd, license BSD-3-Clause.
 */

#include "gpio_switch_ctrl.h"
#include <driver/gpio.h>
#include "os_mutex.h"
#include "esp_type_wrapper.h"
#include "ruuvi_board_gwesp.h"

#define LOG_LOCAL_LEVEL LOG_LEVEL_INFO
#include "log.h"

static const char* TAG = "SwitchCtrl";

static os_mutex_t        g_switch_ctrl_mutex;
static os_mutex_static_t g_switch_ctrl_mutex_mem;
static uint32_t          g_switch_ctrl_cnt;

void
gpio_switch_ctrl_init(void)
{
    LOG_INFO("GPIO SwitchCtrl init");
    g_switch_ctrl_mutex = os_mutex_create_static(&g_switch_ctrl_mutex_mem);
    g_switch_ctrl_cnt   = 0;

    const gpio_config_t io_conf_switch_ctrl = {
        .pin_bit_mask = (1ULL << (uint32_t)RB_ESP32_GPIO_ANALOG_SWITCH_CONTROL),
        .mode         = GPIO_MODE_OUTPUT,
        .pull_up_en   = 0,
        .pull_down_en = 0,
        .intr_type    = GPIO_INTR_DISABLE,
    };
    const esp_err_t err = gpio_config(&io_conf_switch_ctrl);
    if (ESP_OK != err)
    {
        LOG_ERR_ESP(err, "gpio_config(ANALOG_SWITCH_CONTROL)");
        return;
    }
    (void)gpio_set_level(RB_ESP32_GPIO_ANALOG_SWITCH_CONTROL, 0);
}

void
gpio_switch_ctrl_activate(void)
{
    assert(NULL != g_switch_ctrl_mutex);
    os_mutex_lock(g_switch_ctrl_mutex);
    LOG_DBG("GPIO SwitchCtrl activate (cnt=%u)", (printf_uint_t)g_switch_ctrl_cnt);
    if (0 == g_switch_ctrl_cnt)
    {
        (void)gpio_set_level(RB_ESP32_GPIO_ANALOG_SWITCH_CONTROL, 1);
    }
    g_switch_ctrl_cnt += 1;
    os_mutex_unlock(g_switch_ctrl_mutex);
}

void
gpio_switch_ctrl_deactivate(void)
{
    assert(NULL != g_switch_ctrl_mutex);
    os_mutex_lock(g_switch_ctrl_mutex);
    LOG_DBG("GPIO SwitchCtrl deactivate (cnt=%u)", (printf_uint_t)g_switch_ctrl_cnt);
    g_switch_ctrl_cnt -= 1;
    if (0 == g_switch_ctrl_cnt)
    {
        (void)gpio_set_level(RB_ESP32_GPIO_ANALOG_SWITCH_CONTROL, 0);
    }
    os_mutex_unlock(g_switch_ctrl_mutex);
}
