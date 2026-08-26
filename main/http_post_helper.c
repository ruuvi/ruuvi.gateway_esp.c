/**
 * @file http_post_helper.c
 * @author TheSomeMan
 * @date 2026-05-20
 * @copyright Ruuvi Innovations Ltd, license BSD-3-Clause.
 */

#include "http_post_helper.h"
#include <stdbool.h>
#include "wifi_manager_defs.h"
#include "http_server_resp.h"
#include "http_server_cb.h"

http_server_resp_t
http_post_helper_wait_until_async_req_completed_and_gen_resp(
    http_async_info_t* const p_http_async_info,
    const TimeUnitsSeconds_t timeout_seconds)
{
    const bool         flag_feed_task_watchdog = true;
    http_server_resp_t server_resp             = http_wait_until_async_req_completed(
        p_http_async_info->p_http_client_handle,
        &p_http_async_info->http_resp_cb_info,
        flag_feed_task_watchdog,
        timeout_seconds);

    http_resp_code_e http_resp_code = server_resp.http_resp_code;
    if (HTTP_RESP_CODE_429 == http_resp_code)
    {
        // Return OK if we got error "Too Many Requests"
        http_resp_code = HTTP_RESP_CODE_200;
    }

    size_t            len    = 0;
    const char* const p_json = (const char*)http_server_resp_get_content_ptr_if_in_memory(&server_resp, &len);

    const http_server_resp_t resp = http_server_cb_gen_resp(
        http_resp_code,
        "%.*s",
        (printf_int_t)len,
        (NULL != p_json) ? p_json : "");

    http_server_resp_free(&server_resp);

    esp_http_client_cleanup(p_http_async_info->p_http_client_handle);
    p_http_async_info->p_http_client_handle = NULL;
    http_async_info_free_data(p_http_async_info);

    return resp;
}
