/**
 * @file http_post_event_handler.c
 * @author TheSomeMan
 * @date 2026-05-24
 * @copyright Ruuvi Innovations Ltd, license BSD-3-Clause.
 */

#include "http_post_event_handler.h"
#include "os_str.h"
#include "os_malloc.h"
#include "adv_post_async_comm.h"
#include "time_units.h"
#include "http.h"
#include "ruuvi_gateway.h"

#define LOG_LOCAL_LEVEL LOG_LEVEL_INFO
#include "log.h"
static const char TAG[] = "http";
#if (LOG_LOCAL_LEVEL >= LOG_LEVEL_DEBUG)
#warning Debug log level prints out the passwords as a "plaintext".
#endif

#define BASE_10 (10U)

static void
http_post_event_handler_on_header(const esp_http_client_event_t* const p_evt)
{
    LOG_DBG(
        "HTTP_EVENT_ON_HEADER, key=%s, value=%s, user_data=%p",
        p_evt->header_key,
        p_evt->header_value,
        p_evt->user_data);
    if (0 == strcasecmp("Ruuvi-HMAC-KEY", p_evt->header_key))
    {
        if (!adv_post_set_hmac_sha256_key(p_evt->header_value))
        {
            LOG_ERR("Failed to update Ruuvi-HMAC-KEY");
        }
    }
    else if (0 == strcasecmp("X-Ruuvi-Gateway-Rate", p_evt->header_key))
    {
        uint32_t period_seconds = os_str_to_uint32_cptr(p_evt->header_value, NULL, BASE_10);
        if ((0 == period_seconds) || (period_seconds > (TIME_UNITS_MINUTES_PER_HOUR * TIME_UNITS_SECONDS_PER_MINUTE)))
        {
            LOG_WARN("X-Ruuvi-Gateway-Rate: Got incorrect value: %s", p_evt->header_value);
            period_seconds = ADV_POST_DEFAULT_INTERVAL_SECONDS;
        }
        adv_post_set_default_period(period_seconds * TIME_UNITS_MS_PER_SECOND);
    }
    else if ((0 == strcasecmp("Content-Length", p_evt->header_key)) && (NULL != p_evt->user_data))
    {
        http_resp_cb_info_t* const p_cb_info = p_evt->user_data;
        p_cb_info->content_length            = os_str_to_uint32_cptr(p_evt->header_value, NULL, BASE_10);
        p_cb_info->offset                    = 0;
        p_cb_info->p_buf                     = os_malloc(p_cb_info->content_length + 1);
        if (NULL == p_cb_info->p_buf)
        {
            LOG_ERR("Can't allocate %lu bytes for HTTP response", (printf_ulong_t)p_cb_info->content_length);
        }
        else
        {
            p_cb_info->p_buf[0] = '\0';
        }
    }
    else
    {
        // MISRA C:2012, 15.7 - All if...else if constructs shall be terminated with an else statement
    }
}

static void
http_post_event_handler_on_data(const esp_http_client_event_t* const p_evt)
{
    LOG_ERR("HTTP_EVENT_ON_DATA, len=%d: %.*s", p_evt->data_len, p_evt->data_len, (char*)p_evt->data);
    if (NULL != p_evt->user_data)
    {
        http_resp_cb_info_t* const p_cb_info = p_evt->user_data;
        if (NULL != p_cb_info->p_buf)
        {
            const uint32_t remaining_buf = p_cb_info->content_length - p_cb_info->offset;
            const uint32_t len           = (p_evt->data_len <= remaining_buf) ? p_evt->data_len : remaining_buf;
            memcpy(&p_cb_info->p_buf[p_cb_info->offset], p_evt->data, len);
            p_cb_info->offset += len;
            p_cb_info->p_buf[p_cb_info->offset] = '\0';
        }
    }
}

esp_err_t
http_post_event_handler(esp_http_client_event_t* p_evt) // NOSONAR
{
    switch (p_evt->event_id)
    {
        case HTTP_EVENT_ERROR:
            LOG_ERR("HTTP_EVENT_ERROR");
            break;

        case HTTP_EVENT_ON_CONNECTED:
            LOG_DBG("HTTP_EVENT_ON_CONNECTED");
            break;

        case HTTP_EVENT_HEADER_SENT:
            LOG_DBG("HTTP_EVENT_HEADER_SENT");
            break;

        case HTTP_EVENT_ON_HEADER:
            http_post_event_handler_on_header(p_evt);
            break;

        case HTTP_EVENT_ON_DATA:
            http_post_event_handler_on_data(p_evt);
            break;

        case HTTP_EVENT_ON_FINISH:
            LOG_DBG("HTTP_EVENT_ON_FINISH");
            break;

        case HTTP_EVENT_DISCONNECTED:
            LOG_DBG("HTTP_EVENT_DISCONNECTED");
            break;

        default:
            break;
    }
    return ESP_OK;
}
