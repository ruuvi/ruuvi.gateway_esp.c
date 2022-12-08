/**
 * @file reset_reason.c
 * @author TheSomeMan
 * @date 2022-11-18
 * @copyright Ruuvi Innovations Ltd, license BSD-3-Clause.
 */

#include "reset_reason.h"

const char*
reset_reason_to_str(const esp_reset_reason_t reset_reason)
{
    switch (reset_reason)
    {
        case ESP_RST_UNKNOWN: // Reset reason can not be determined
            return "UNKNOWN";
        case ESP_RST_POWERON: // Reset due to power-on event
            return "POWER_ON";
        case ESP_RST_EXT: // Reset by external pin (not applicable for ESP32)
            return "EXT";
        case ESP_RST_SW: // Software reset via esp_restart
            return "SW";
        case ESP_RST_PANIC: // Software reset due to exception/panic
            return "PANIC";
        case ESP_RST_INT_WDT: // Reset (software or hardware) due to interrupt watchdog
            return "INT_WDT";
        case ESP_RST_TASK_WDT: // Reset due to task watchdog
            return "TASK_WDT";
        case ESP_RST_WDT: // Reset due to other watchdogs
            return "WDT";
        case ESP_RST_DEEPSLEEP: // Reset after exiting deep sleep mode
            return "DEEP_SLEEP";
        case ESP_RST_BROWNOUT: // Brownout reset (software or hardware)
            return "BROWNOUT";
        case ESP_RST_SDIO: // Reset over SDIO
            return "SDIO";
        default:
            return "UNSPECIFIED";
    }
}
