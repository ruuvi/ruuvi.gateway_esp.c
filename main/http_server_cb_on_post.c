/**
 * @file http_server_cb_on_post.c
 * @author TheSomeMan
 * @date 2020-10-26
 * @copyright Ruuvi Innovations Ltd, license BSD-3-Clause.
 */

#include "http_server_cb.h"
#include <string.h>
#include "ruuvi_gateway.h"
#include "wifi_manager.h"
#include "ethernet.h"
#include "json_ruuvi.h"
#include "os_malloc.h"
#include "fw_update.h"
#include "gw_cfg.h"
#include "gw_status.h"
#include "gw_cfg_storage.h"
#include "partition_table.h"
#include "gw_cfg_json_parse_filter.h"
#include "gw_cfg_json_parse_scan.h"
#include "adv_post.h"
#include "reset_task.h"
#include "event_mgr.h"
#include "esp_transport_ssl.h"

#if RUUVI_TESTS_HTTP_SERVER_CB
#define LOG_LOCAL_LEVEL LOG_LEVEL_DEBUG
#else
#define LOG_LOCAL_LEVEL LOG_LEVEL_INFO
#endif
#include "log.h"

#if (LOG_LOCAL_LEVEL >= LOG_LEVEL_DEBUG) && !RUUVI_TESTS
#warning Debug log level prints out the passwords as a "plaintext".
#endif

static const char TAG[] = "http_server";

static const char g_empty_json[] = "{}";

static bool g_http_server_cb_flag_prohibit_cfg_updating = false;

HTTP_SERVER_CB_STATIC
http_server_resp_t
http_server_cb_on_post_ruuvi(const char* p_body, const bool flag_access_from_lan)
{
    LOG_DBG("POST /ruuvi.json, flag_access_from_lan=%d", (printf_int_t)flag_access_from_lan);
    bool      flag_network_cfg = false;
    gw_cfg_t* p_gw_cfg_tmp     = os_calloc(1, sizeof(*p_gw_cfg_tmp));
    if (NULL == p_gw_cfg_tmp)
    {
        LOG_ERR("Failed to allocate memory for gw_cfg");
        return http_server_resp_503();
    }
    gw_cfg_get_copy(p_gw_cfg_tmp);
    if (!json_ruuvi_parse_http_body(p_body, p_gw_cfg_tmp, flag_access_from_lan ? NULL : &flag_network_cfg))
    {
        os_free(p_gw_cfg_tmp);
        return http_server_resp_503();
    }
    if (flag_network_cfg)
    {
        gw_cfg_update_eth_cfg(&p_gw_cfg_tmp->eth_cfg);
        gw_cfg_update_wifi_ap_config(&p_gw_cfg_tmp->wifi_cfg.ap);
        wifi_manager_set_config_ap(&p_gw_cfg_tmp->wifi_cfg.ap);

        if (p_gw_cfg_tmp->eth_cfg.use_eth)
        {
            ethernet_update_ip();
        }
    }
    else
    {
        gw_cfg_update_ruuvi_cfg(&p_gw_cfg_tmp->ruuvi_cfg);
        if (p_gw_cfg_tmp->ruuvi_cfg.remote.use_remote_cfg)
        {
            timer_cfg_mode_deactivation_start();
        }
        else
        {
            timer_cfg_mode_deactivation_start_with_delay(RUUVI_CFG_MODE_DEACTIVATION_SHORT_DELAY_SEC);
        }
    }
    os_free(p_gw_cfg_tmp);

    const bool flag_no_cache = true;
    return http_server_resp_data_in_flash(
        HTTP_CONENT_TYPE_APPLICATION_JSON,
        NULL,
        strlen(g_empty_json),
        HTTP_CONENT_ENCODING_NONE,
        (const uint8_t*)g_empty_json,
        flag_no_cache);
}

HTTP_SERVER_CB_STATIC
http_server_resp_t
http_server_cb_on_post_ble_scanning(const char* const p_body)
{
    cJSON* p_json_root = cJSON_Parse(p_body);
    if (NULL == p_json_root)
    {
        LOG_ERR("Failed to parse json or no memory");
        return http_server_resp_503();
    }
    const gw_cfg_t*       p_gw_cfg = gw_cfg_lock_ro();
    ruuvi_gw_cfg_filter_t filter   = p_gw_cfg->ruuvi_cfg.filter;
    ruuvi_gw_cfg_scan_t   scan     = p_gw_cfg->ruuvi_cfg.scan;
    gw_cfg_unlock_ro(&p_gw_cfg);
    gw_cfg_json_parse_filter(p_json_root, &filter);
    gw_cfg_json_parse_scan(p_json_root, &scan);
    ruuvi_send_nrf_settings(&scan, &filter);
    cJSON_Delete(p_json_root);

    event_mgr_notify(EVENT_MGR_EV_CFG_BLE_SCAN_CHANGED);

    const bool flag_no_cache = true;
    return http_server_resp_data_in_flash(
        HTTP_CONENT_TYPE_APPLICATION_JSON,
        NULL,
        strlen(g_empty_json),
        HTTP_CONENT_ENCODING_NONE,
        (const uint8_t*)g_empty_json,
        flag_no_cache);
}

HTTP_SERVER_CB_STATIC
http_server_resp_t
http_server_cb_on_post_fw_update(const char* p_body, const bool flag_access_from_lan)
{
    LOG_DBG("POST /fw_update.json");

    if (!json_fw_update_parse_http_body(p_body))
    {
        return http_server_cb_gen_resp(HTTP_RESP_CODE_400, "Failed to parse HTTP body");
    }
    if (!fw_update_binaries_is_url_valid())
    {
        return http_server_cb_gen_resp(HTTP_RESP_CODE_400, "Bad URL");
    }

    const bool flag_wait_until_relaying_stopped = true;
    gw_status_suspend_relaying(flag_wait_until_relaying_stopped);

    const char*            p_err_file_name = NULL;
    const http_resp_code_e http_resp_code  = http_server_check_fw_update_binary_files(
        fw_update_get_binaries_url(),
        &p_err_file_name);
    if (HTTP_RESP_CODE_200 != http_resp_code)
    {
        gw_status_resume_relaying(true);
        return http_server_cb_gen_resp(http_resp_code, "Failed to download %s", p_err_file_name);
    }

    fw_update_set_extra_info_for_status_json_update_start();

    if (!fw_update_run(flag_access_from_lan ? FW_UPDATE_REASON_MANUAL_VIA_LAN : FW_UPDATE_REASON_MANUAL_VIA_HOTSPOT))
    {
        return http_server_cb_gen_resp(HTTP_RESP_CODE_503, "Failed to start firmware updating - internal error");
    }

    return http_server_cb_gen_resp(HTTP_RESP_CODE_200, "OK");
}

HTTP_SERVER_CB_STATIC
http_server_resp_t
http_server_cb_on_post_fw_update_url(const char* p_body)
{
    LOG_DBG("POST /fw_update_url.json");

    str_buf_t str_buf_url = json_fw_update_url_parse_http_body_get_url(p_body);
    if (NULL == str_buf_url.buf)
    {
        return http_server_cb_gen_resp(HTTP_RESP_CODE_400, "Failed to parse HTTP body");
    }
    gw_cfg_update_fw_update_url(str_buf_url.buf);
    str_buf_free_buf(&str_buf_url);

    return http_server_cb_gen_resp(HTTP_RESP_CODE_200, "OK");
}

HTTP_SERVER_CB_STATIC
http_server_resp_t
http_server_cb_on_post_fw_update_reset(void)
{
    LOG_DBG("POST /fw_update_reset");
    fw_update_set_extra_info_for_status_json_update_reset();
    const bool flag_no_cache = true;
    return http_server_resp_data_in_flash(
        HTTP_CONENT_TYPE_APPLICATION_JSON,
        NULL,
        strlen(g_empty_json),
        HTTP_CONENT_ENCODING_NONE,
        (const uint8_t*)g_empty_json,
        flag_no_cache);
}

HTTP_SERVER_CB_STATIC
http_server_resp_t
http_server_cb_on_post_gw_cfg_download(void)
{
    LOG_DBG("POST /gw_cfg_download");
    bool flag_reboot_needed = false;

    str_buf_t              err_msg        = str_buf_init_null();
    const http_resp_code_e http_resp_code = http_server_gw_cfg_download_and_update(&flag_reboot_needed, true, &err_msg);

    if (HTTP_RESP_CODE_200 == http_resp_code)
    {
        return http_server_cb_gen_resp(
            HTTP_RESP_CODE_200,
            "%s",
            flag_reboot_needed ? "Network configuration in gw_cfg.json on the server is different from the current "
                                 "one, the gateway will be rebooted."
                               : "");
    }

    const http_server_resp_t resp = http_server_cb_gen_resp(
        http_resp_code,
        "%s",
        (NULL != err_msg.buf) ? err_msg.buf : "");
    if (NULL != err_msg.buf)
    {
        str_buf_free_buf(&err_msg);
    }

    return resp;
}

HTTP_SERVER_CB_STATIC
http_server_resp_t
http_server_cb_on_post_ssl_cert(const char* const p_body, const char* const p_uri_params)
{
    LOG_DBG("POST /ssl_cert %s", (NULL != p_uri_params) ? p_uri_params : "NULL");
    str_buf_t filename_str_buf = http_server_get_from_params_with_decoding(p_uri_params, "file=");
    if (NULL == filename_str_buf.buf)
    {
        LOG_ERR("HTTP post_ssl_cert: can't find 'file' in params: %s", p_uri_params);
        return http_server_resp_400();
    }
    LOG_DBG("Content: %s", p_body);
    if (!gw_cfg_storage_is_known_filename(filename_str_buf.buf))
    {
        LOG_ERR("HTTP post_ssl_cert: Unknown file name: %s", filename_str_buf.buf);
        str_buf_free_buf(&filename_str_buf);
        return http_server_resp_400();
    }

    if (!gw_cfg_storage_write_file(filename_str_buf.buf, p_body))
    {
        LOG_ERR("Can't write file '%s', length=%lu", filename_str_buf.buf, (printf_ulong_t)strlen(p_body));
        str_buf_free_buf(&filename_str_buf);
        return http_server_resp_500();
    }
    str_buf_free_buf(&filename_str_buf);

    return http_server_resp_200_json("{}");
}

HTTP_SERVER_CB_STATIC
http_server_resp_t
http_server_cb_on_post_init_storage(const char* const p_uri_params)
{
    LOG_INFO("POST /init_storage, params=%s", (NULL != p_uri_params) ? p_uri_params : "NULL");

    if (partition_table_check_and_update())
    {
        reset_task_reboot_after_timeout();
    }

    return http_server_resp_200_json("{}");
}

http_server_resp_t
http_server_cb_on_post(
    const char* const p_file_name,
    const char* const p_uri_params,
    const char* const p_body,
    const bool        flag_access_from_lan)
{
    LOG_DBG("http_server_cb_on_post /%s, params=%s", p_file_name, (NULL != p_uri_params) ? p_uri_params : "");
    if (0 == strcmp(p_file_name, "fw_update_reset"))
    {
        return http_server_cb_on_post_fw_update_reset();
    }
    if (g_http_server_cb_flag_prohibit_cfg_updating)
    {
        LOG_ERR("Access to some resources are prohibited while in updating mode: %s", p_file_name);
        return http_server_resp_403_forbidden();
    }
    if (0 == strcmp(p_file_name, "ruuvi.json"))
    {
        LOG_INFO("%s: Clear all saved TLS session tickets", __func__);
        esp_transport_ssl_clear_saved_session_tickets();
        return http_server_cb_on_post_ruuvi(p_body, flag_access_from_lan);
    }
    if (0 == strcmp(p_file_name, "bluetooth_scanning.json"))
    {
        return http_server_cb_on_post_ble_scanning(p_body);
    }
    if (0 == strcmp(p_file_name, "fw_update.json"))
    {
        return http_server_cb_on_post_fw_update(p_body, flag_access_from_lan);
    }
    if (0 == strcmp(p_file_name, "fw_update_url.json"))
    {
        return http_server_cb_on_post_fw_update_url(p_body);
    }
    if (0 == strcmp(p_file_name, "gw_cfg_download"))
    {
        return http_server_cb_on_post_gw_cfg_download();
    }
    if (0 == strcmp(p_file_name, "ssl_cert"))
    {
        return http_server_cb_on_post_ssl_cert(p_body, p_uri_params);
    }
    if (0 == strcmp(p_file_name, "init_storage"))
    {
        return http_server_cb_on_post_init_storage(p_uri_params);
    }
    LOG_WARN("POST /%s", p_file_name);
    return http_server_resp_404();
}

void
http_server_cb_prohibit_cfg_updating(void)
{
    g_http_server_cb_flag_prohibit_cfg_updating = true;
}

void
http_server_cb_allow_cfg_updating(void)
{
    g_http_server_cb_flag_prohibit_cfg_updating = false;
}
