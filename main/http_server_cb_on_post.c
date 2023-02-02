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
#include "flashfatfs.h"
#include "metrics.h"
#include "os_malloc.h"
#include "fw_update.h"
#include "gw_cfg.h"
#include "gw_status.h"

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
        if (wifi_manager_is_ap_active())
        {
            main_task_stop_wifi_hotspot_after_short_delay();
        }
        else
        {
            main_task_send_sig_restart_services();
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
http_server_cb_on_post_fw_update(const char* p_body, const bool flag_access_from_lan)
{
    LOG_DBG("POST /fw_update.json");

    if (!json_fw_update_parse_http_body(p_body))
    {
        return http_server_cb_gen_resp(HTTP_RESP_CODE_400, "Failed to parse HTTP body");
    }
    if (!fw_update_is_url_valid())
    {
        return http_server_cb_gen_resp(HTTP_RESP_CODE_400, "Bad URL");
    }

    const bool flag_wait_until_relaying_stopped = true;
    gw_status_suspend_relaying(flag_wait_until_relaying_stopped);

    char url[FW_UPDATE_URL_WITH_FW_IMAGE_MAX_LEN];

    (void)snprintf(url, sizeof(url), "%s/%s", fw_update_get_url(), "ruuvi_gateway_esp.bin");
    http_resp_code_e http_resp_code
        = http_check(url, HTTP_DOWNLOAD_TIMEOUT_SECONDS, GW_CFG_REMOTE_AUTH_TYPE_NO, NULL, NULL, true);
    if (HTTP_RESP_CODE_200 != http_resp_code)
    {
        LOG_ERR("Failed to download ruuvi_gateway_esp.bin");
        gw_status_resume_relaying(true);
        return http_server_cb_gen_resp(http_resp_code, "Failed to download ruuvi_gateway_esp.bin");
    }

    (void)snprintf(url, sizeof(url), "%s/%s", fw_update_get_url(), "fatfs_gwui.bin");
    http_resp_code = http_check(url, HTTP_DOWNLOAD_TIMEOUT_SECONDS, GW_CFG_REMOTE_AUTH_TYPE_NO, NULL, NULL, true);
    if (HTTP_RESP_CODE_200 != http_resp_code)
    {
        LOG_ERR("Failed to download fatfs_gwui.bin");
        gw_status_resume_relaying(true);
        return http_server_cb_gen_resp(http_resp_code, "Failed to download fatfs_gwui.bin");
    }

    (void)snprintf(url, sizeof(url), "%s/%s", fw_update_get_url(), "fatfs_nrf52.bin");
    http_resp_code = http_check(url, HTTP_DOWNLOAD_TIMEOUT_SECONDS, GW_CFG_REMOTE_AUTH_TYPE_NO, NULL, NULL, true);
    if (HTTP_RESP_CODE_200 != http_resp_code)
    {
        gw_status_resume_relaying(true);
        LOG_ERR("Failed to download fatfs_nrf52.bin");
        return http_server_cb_gen_resp(http_resp_code, "Failed to download fatfs_nrf52.bin");
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

http_server_resp_t
http_server_cb_on_post(
    const char* const p_file_name,
    const char* const p_uri_params,
    const char* const p_body,
    const bool        flag_access_from_lan)
{
    (void)p_uri_params;
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
        return http_server_cb_on_post_ruuvi(p_body, flag_access_from_lan);
    }
    if (0 == strcmp(p_file_name, "fw_update.json"))
    {
        return http_server_cb_on_post_fw_update(p_body, flag_access_from_lan);
    }
    if (0 == strcmp(p_file_name, "gw_cfg_download"))
    {
        return http_server_cb_on_post_gw_cfg_download();
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
