/**
 * @file adv_post_green_led.h
 * @author TheSomeMan
 * @date 2023-09-04
 * @copyright Ruuvi Innovations Ltd, license BSD-3-Clause.
 */

#ifndef RUUVI_GATEWAY_ESP_ADV_POST_GREEN_LED_H
#define RUUVI_GATEWAY_ESP_ADV_POST_GREEN_LED_H

#ifdef __cplusplus
extern "C" {
#endif

typedef enum adv_post_green_led_cmd_e
{
    ADV_POST_GREEN_LED_CMD_UPDATE = 0,
    ADV_POST_GREEN_LED_CMD_OFF    = 1,
    ADV_POST_GREEN_LED_CMD_ON     = 2,
} adv_post_green_led_cmd_e;

void
adv_post_green_led_init(void);

void
adv_post_on_green_led_update(const adv_post_green_led_cmd_e cmd);

#ifdef __cplusplus
}
#endif

#endif // RUUVI_GATEWAY_ESP_ADV_POST_GREEN_LED_H
