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
#include "adv_post.h"
#include "gw_cfg.h"

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
http_server_cb_on_post_ruuvi(const char* p_body)
{
    LOG_DBG("POST /ruuvi.json");
    bool      flag_network_cfg = false;
    gw_cfg_t* p_gw_cfg_tmp     = os_calloc(1, sizeof(*p_gw_cfg_tmp));
    if (NULL == p_gw_cfg_tmp)
    {
        LOG_ERR("Failed to allocate memory for gw_cfg");
        return http_server_resp_503();
    }
    gw_cfg_get_copy(p_gw_cfg_tmp);
    if (!json_ruuvi_parse_http_body(p_body, p_gw_cfg_tmp, &flag_network_cfg))
    {
        os_free(p_gw_cfg_tmp);
        return http_server_resp_503();
    }
    if (flag_network_cfg)
    {
        gw_cfg_update_eth_cfg(&p_gw_cfg_tmp->eth_cfg);
        gw_cfg_update_wifi_ap_config(&p_gw_cfg_tmp->wifi_cfg.ap);
        wifi_manager_set_config_ap(&p_gw_cfg_tmp->wifi_cfg.ap);

        adv_post_disable_retransmission();
        if (p_gw_cfg_tmp->eth_cfg.use_eth)
        {
            ethernet_update_ip();
        }
    }
    else
    {
        gw_cfg_update_ruuvi_cfg(&p_gw_cfg_tmp->ruuvi_cfg);
        restart_services();
        adv_post_enable_retransmission();
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
    fw_update_set_extra_info_for_status_json_update_start();

    if (!json_fw_update_parse_http_body(p_body))
    {
        fw_update_set_extra_info_for_status_json_update_failed("Bad request");
        return http_server_resp_400();
    }
    if (!fw_update_is_url_valid())
    {
        fw_update_set_extra_info_for_status_json_update_failed("Bad URL");
        return http_server_resp_400();
    }
    if (!fw_update_run(flag_access_from_lan ? FW_UPDATE_REASON_MANUAL_VIA_LAN : FW_UPDATE_REASON_MANUAL_VIA_HOTSPOT))
    {
        fw_update_set_extra_info_for_status_json_update_failed("Internal error");
        return http_server_resp_503();
    }
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

    const http_resp_code_e http_resp_code = http_server_gw_cfg_download_and_update(&flag_reboot_needed);

    const char* p_resp_content = g_empty_json;
    if (flag_reboot_needed)
    {
        p_resp_content
            = "{\"message\":"
              "\"Network configuration in gw_cfg.json on the server is different from the current one, "
              "the gateway will be rebooted.\"}";
    }

    http_server_resp_t resp = {
        .http_resp_code               = http_resp_code,
        .content_location             = HTTP_CONTENT_LOCATION_FLASH_MEM,
        .flag_no_cache                = true,
        .flag_add_header_date         = true,
        .content_type                 = HTTP_CONENT_TYPE_APPLICATION_JSON,
        .p_content_type_param         = NULL,
        .content_len                  = strlen(p_resp_content),
        .content_encoding             = HTTP_CONENT_ENCODING_NONE,
        .select_location.memory.p_buf = (const uint8_t*)p_resp_content,
    };
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
    if (g_http_server_cb_flag_prohibit_cfg_updating)
    {
        return http_server_resp_404();
    }
    if (0 == strcmp(p_file_name, "ruuvi.json"))
    {
        return http_server_cb_on_post_ruuvi(p_body);
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
