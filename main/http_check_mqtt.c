/**
 * @file http_check_mqtt.c
 * @author TheSomeMan
 * @date 2023-07-31
 * @copyright Ruuvi Innovations Ltd, license BSD-3-Clause.
 */

#include "http.h"
#include <esp_task_wdt.h>
#include "mqtt.h"
#include "gw_status.h"
#include "http_server_cb.h"

#define LOG_LOCAL_LEVEL LOG_LEVEL_INFO
#include "log.h"
static const char TAG[] = "http";
#if (LOG_LOCAL_LEVEL >= LOG_LEVEL_DEBUG)
#warning Debug log level prints out the passwords as a "plaintext".
#endif

static http_server_resp_t
http_check_mqtt_internal(const ruuvi_gw_cfg_mqtt_t* const p_mqtt_cfg, const TimeUnitsSeconds_t timeout_seconds)
{
    LOG_DBG("mqtt_app_start");
    mqtt_app_start(p_mqtt_cfg);

    const TickType_t tick_start = xTaskGetTickCount();
    LOG_DBG("Wait until MQTT connected for %u seconds", (printf_int_t)timeout_seconds);
    while (!gw_status_is_mqtt_connected_or_error())
    {
        if ((xTaskGetTickCount() - tick_start) >= pdMS_TO_TICKS(timeout_seconds * TIME_UNITS_MS_PER_SECOND))
        {
            LOG_ERR("Timeout waiting until MQTT connected");
            break;
        }
        esp_task_wdt_reset();
        vTaskDelay(pdMS_TO_TICKS(50));
    }

    const bool         flag_is_mqtt_connected = gw_status_is_mqtt_connected();
    const mqtt_error_e mqtt_error             = gw_status_get_mqtt_error();
    str_buf_t          mqtt_err_msg           = mqtt_app_get_error_message();

    LOG_DBG("mqtt_app_stop");
    LOG_DBG("mqtt_err_msg: %s", mqtt_err_msg.buf ? mqtt_err_msg.buf : ":NULL:");
    mqtt_app_stop();

    http_resp_code_e http_resp_code = HTTP_RESP_CODE_200;
    str_buf_t        err_desc       = STR_BUF_INIT_NULL();
    if (!flag_is_mqtt_connected)
    {
        switch (mqtt_error)
        {
            case MQTT_ERROR_NONE:
                http_resp_code = HTTP_RESP_CODE_502; // Not Found
                err_desc       = str_buf_printf_with_alloc("Timeout waiting for a response from the server");
                break;
            case MQTT_ERROR_DNS:
                http_resp_code = HTTP_RESP_CODE_502; // Not Found
                err_desc       = str_buf_printf_with_alloc("Failed to resolve hostname");
                break;
            case MQTT_ERROR_AUTH:
                http_resp_code = HTTP_RESP_CODE_401; // Unauthorized
                err_desc       = str_buf_printf_with_alloc(
                    "Access with the specified credentials is prohibited (%s)",
                    (NULL != mqtt_err_msg.buf) ? mqtt_err_msg.buf : "");
                break;
            case MQTT_ERROR_CONNECT:
                http_resp_code = HTTP_RESP_CODE_502; // Not Found
                err_desc       = str_buf_printf_with_alloc(
                    "Failed to connect (%s)",
                    (NULL != mqtt_err_msg.buf) ? mqtt_err_msg.buf : "");
                break;
        }
    }
    str_buf_free_buf(&mqtt_err_msg);

    const http_server_resp_t resp = http_server_cb_gen_resp(
        http_resp_code,
        "%s",
        (NULL != err_desc.buf) ? err_desc.buf : "");
    str_buf_free_buf(&err_desc);
    return resp;
}

http_server_resp_t
http_check_mqtt(const ruuvi_gw_cfg_mqtt_t* const p_mqtt_cfg, const TimeUnitsSeconds_t timeout_seconds)
{
    http_async_info_t* const p_http_async_info = http_get_async_info();
    LOG_DBG("os_sema_wait_immediate: p_http_async_sema");
    if (!os_sema_wait_immediate(p_http_async_info->p_http_async_sema))
    {
        /* Because the amount of memory is limited and may not be sufficient for two simultaneous queries,
         * this function should only be called from a single thread. */
        LOG_ERR("This function is called twice from two different threads, which can lead to memory shortages");
        assert(0);
    }
    p_http_async_info->p_task = xTaskGetCurrentTaskHandle();

    const http_server_resp_t resp = http_check_mqtt_internal(p_mqtt_cfg, timeout_seconds);

    LOG_DBG("os_sema_signal: p_http_async_sema");
    os_sema_signal(p_http_async_info->p_http_async_sema);

    return resp;
}
