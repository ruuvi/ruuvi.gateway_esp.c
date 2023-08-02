/**
 * @file http_download.c
 * @author TheSomeMan
 * @date 2020-10-26
 * @copyright Ruuvi Innovations Ltd, license BSD-3-Clause.
 */

#include "http_download.h"
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "str_buf.h"
#include "os_malloc.h"
#include "time_str.h"
#include "ruuvi_gateway.h"
#include "http_server_cb.h"

#if RUUVI_TESTS_HTTP_SERVER_CB
#define LOG_LOCAL_LEVEL LOG_LEVEL_DEBUG
#else
#define LOG_LOCAL_LEVEL LOG_LEVEL_INFO
#endif
#include "log.h"

static const char TAG[] = "http_download";

static bool
cb_on_http_download_json_data(
    const uint8_t* const   p_buf,
    const size_t           buf_size,
    const size_t           offset,
    const size_t           content_length,
    const http_resp_code_e http_resp_code,
    void*                  p_user_data)
{
    LOG_INFO("%s: buf_size=%lu", __func__, (printf_ulong_t)buf_size);

    http_server_download_info_t* const p_info = p_user_data;
    if (p_info->is_error)
    {
        LOG_ERR("Error occurred while downloading");
        return false;
    }
    if ((HTTP_RESP_CODE_301 == http_resp_code) || (HTTP_RESP_CODE_302 == http_resp_code))
    {
        LOG_INFO("Got HTTP error %d: Redirect to another location", (printf_int_t)http_resp_code);
        return true;
    }
    p_info->http_resp_code = http_resp_code;
    if (HTTP_RESP_CODE_200 != http_resp_code)
    {
        LOG_ERR("Got HTTP error %d: %.*s", (printf_int_t)http_resp_code, (printf_int_t)buf_size, (const char*)p_buf);
        p_info->is_error = true;
        if (NULL != p_info->p_json_buf)
        {
            os_free(p_info->p_json_buf);
            p_info->p_json_buf = NULL;
        }
        return false;
    }

    if (NULL == p_info->p_json_buf)
    {
        if (0 == content_length)
        {
            p_info->json_buf_size = buf_size + 1;
        }
        else
        {
            p_info->json_buf_size = content_length + 1;
        }
        p_info->p_json_buf = os_calloc(1, p_info->json_buf_size);
        if (NULL == p_info->p_json_buf)
        {
            p_info->is_error = true;
            LOG_ERR("Can't allocate %lu bytes for json file", (printf_ulong_t)p_info->json_buf_size);
            return false;
        }
    }
    else
    {
        if (0 == content_length)
        {
            p_info->json_buf_size += buf_size;
            LOG_INFO("Reallocate %lu bytes", (printf_ulong_t)p_info->json_buf_size);
            if (!os_realloc_safe_and_clean((void**)&p_info->p_json_buf, p_info->json_buf_size))
            {
                p_info->is_error = true;
                LOG_ERR("Can't allocate %lu bytes for json file", (printf_ulong_t)p_info->json_buf_size);
                return false;
            }
        }
    }
    if ((offset >= p_info->json_buf_size) || ((offset + buf_size) >= p_info->json_buf_size))
    {
        p_info->is_error = true;
        if (NULL != p_info->p_json_buf)
        {
            os_free(p_info->p_json_buf);
            p_info->p_json_buf = NULL;
        }
        LOG_ERR(
            "Buffer overflow while downloading json file: json_buf_size=%lu, offset=%lu, len=%lu",
            (printf_ulong_t)p_info->json_buf_size,
            (printf_ulong_t)offset,
            (printf_ulong_t)buf_size);
        return false;
    }
    memcpy(&p_info->p_json_buf[offset], p_buf, buf_size);
    p_info->p_json_buf[offset + buf_size] = '\0';
    return true;
}

http_server_download_info_t
http_download_json(
    const char* const                     p_url,
    const TimeUnitsSeconds_t              timeout_seconds,
    const gw_cfg_http_auth_type_e         auth_type,
    const ruuvi_gw_cfg_http_auth_t* const p_http_auth,
    const http_header_item_t* const       p_extra_header_item,
    const bool                            flag_free_memory,
    const char* const                     p_server_cert,
    const char* const                     p_client_cert,
    const char* const                     p_client_key)
{
    http_server_download_info_t info = {
        .is_error       = false,
        .http_resp_code = HTTP_RESP_CODE_200,
        .p_json_buf     = NULL,
        .json_buf_size  = 0,
    };
    const TickType_t       download_started_at_tick = xTaskGetTickCount();
    const bool             flag_feed_task_watchdog  = true;
    http_download_param_t* p_download_param         = os_calloc(1, sizeof(*p_download_param));
    if (NULL == p_download_param)
    {
        LOG_ERR("Can't allocate memory");
        info.is_error       = true;
        info.http_resp_code = HTTP_RESP_CODE_500;
        return info;
    }
    p_download_param->p_url                   = p_url;
    p_download_param->timeout_seconds         = timeout_seconds;
    p_download_param->p_cb_on_data            = &cb_on_http_download_json_data;
    p_download_param->p_user_data             = &info;
    p_download_param->flag_feed_task_watchdog = flag_feed_task_watchdog;
    p_download_param->flag_free_memory        = flag_free_memory;
    p_download_param->p_server_cert           = p_server_cert;
    p_download_param->p_client_cert           = p_client_cert;
    p_download_param->p_client_key            = p_client_key;

    if (!http_download_with_auth(p_download_param, auth_type, p_http_auth, p_extra_header_item))
    {
        LOG_ERR("http_download failed for URL: %s", p_url);
        info.is_error = true;
    }
    os_free(p_download_param);
    if (HTTP_RESP_CODE_200 != info.http_resp_code)
    {
        if (NULL == info.p_json_buf)
        {
            LOG_ERR("http_download failed, HTTP resp code %d", (printf_int_t)info.http_resp_code);
        }
        else
        {
            LOG_ERR("http_download failed, HTTP resp code %d: %s", (printf_int_t)info.http_resp_code, info.p_json_buf);
            os_free(info.p_json_buf);
        }
        info.is_error = true;
    }
    else if (NULL == info.p_json_buf)
    {
        LOG_ERR("http_download returned NULL buffer");
        info.is_error = true;
    }
    else
    {
        // MISRA C:2012, 15.7 - All if...else if constructs shall be terminated with an else statement
    }
    if (info.is_error && (HTTP_RESP_CODE_200 == info.http_resp_code))
    {
        info.http_resp_code = HTTP_RESP_CODE_400;
    }
    const TickType_t download_completed_within_ticks = xTaskGetTickCount() - download_started_at_tick;
    LOG_INFO("%s: completed within %u ticks", __func__, (printf_uint_t)download_completed_within_ticks);
    return info;
}

http_server_download_info_t
http_download_latest_release_info(const bool flag_free_memory)
{
    const char* const p_url = "https://network.ruuvi.com/firmwareupdate";
    return http_download_json(
        p_url,
        HTTP_DOWNLOAD_FW_RELEASE_INFO_TIMEOUT_SECONDS,
        GW_CFG_HTTP_AUTH_TYPE_NONE,
        NULL,
        NULL,
        flag_free_memory,
        NULL,
        NULL,
        NULL);
}

http_resp_code_e
http_check(
    const char* const                     p_url,
    const TimeUnitsSeconds_t              timeout_seconds,
    const gw_cfg_http_auth_type_e         auth_type,
    const ruuvi_gw_cfg_http_auth_t* const p_http_auth,
    const http_header_item_t* const       p_extra_header_item,
    const bool                            flag_free_memory)
{
    const TickType_t    download_started_at_tick = xTaskGetTickCount();
    const bool          flag_feed_task_watchdog  = true;
    http_resp_code_e    http_resp_code           = HTTP_RESP_CODE_200;
    http_check_param_t* p_check_param            = os_calloc(1, sizeof(*p_check_param));
    if (NULL == p_check_param)
    {
        LOG_ERR("Can't allocate memory");
        return HTTP_RESP_CODE_500;
    }
    p_check_param->p_url                   = p_url;
    p_check_param->timeout_seconds         = timeout_seconds;
    p_check_param->flag_feed_task_watchdog = flag_feed_task_watchdog;
    p_check_param->flag_free_memory        = flag_free_memory;
    p_check_param->p_server_cert           = NULL;
    p_check_param->p_client_cert           = NULL;
    p_check_param->p_client_key            = NULL;

    if (!http_check_with_auth(p_check_param, auth_type, p_http_auth, p_extra_header_item, &http_resp_code))
    {
        LOG_ERR("http_check failed for URL: %s", p_url);
    }
    os_free(p_check_param);
    const TickType_t download_completed_within_ticks = xTaskGetTickCount() - download_started_at_tick;
    LOG_INFO("%s: completed within %u ticks", __func__, (printf_uint_t)download_completed_within_ticks);
    return http_resp_code;
}
