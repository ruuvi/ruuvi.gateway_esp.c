/**
 * @file http_server_cb.c
 * @author TheSomeMan
 * @date 2020-10-26
 * @copyright Ruuvi Innovations Ltd, license BSD-3-Clause.
 */

#include "http_server_cb.h"
#include <string.h>
#include "ruuvi_gateway.h"
#include "wifi_manager.h"
#include "flashfatfs.h"
#include "os_malloc.h"
#include "http_server.h"
#include "http.h"
#include "fw_update.h"
#include "json_helper.h"
#include "os_time.h"
#include "time_str.h"
#include "reset_task.h"
#include "gw_cfg.h"
#include "gw_cfg_json_parse.h"
#include "gw_status.h"
#include "url_encode.h"
#include "gw_cfg_storage.h"

#if RUUVI_TESTS_HTTP_SERVER_CB
#define LOG_LOCAL_LEVEL LOG_LEVEL_DEBUG
#else
#define LOG_LOCAL_LEVEL LOG_LEVEL_INFO
#endif
#include "log.h"

#if (LOG_LOCAL_LEVEL >= LOG_LEVEL_DEBUG) && !RUUVI_TESTS
#warning Debug log level prints out the passwords as a "plaintext".
#endif

#define GW_CFG_REMOTE_URL_MIN_LEN (3)

static const char TAG[] = "http_server";

const flash_fat_fs_t* gp_ffs_gwui;

#if !RUUVI_TESTS_HTTP_SERVER_CB
time_t
http_server_get_cur_time(void)
{
    return os_time_get();
}
#endif

bool
http_server_cb_init(const char* const p_fatfs_gwui_partition_name)
{
    const char*                    mount_point   = "/fs_gwui";
    const flash_fat_fs_num_files_t max_num_files = 4U;

    gp_ffs_gwui = flashfatfs_mount(mount_point, p_fatfs_gwui_partition_name, max_num_files);
    if (NULL == gp_ffs_gwui)
    {
        LOG_ERR("flashfatfs_mount: failed to mount partition '%s'", GW_GWUI_PARTITION);
        return false;
    }
    return true;
}

void
http_server_cb_deinit(void)
{
    if (NULL != gp_ffs_gwui)
    {
        flashfatfs_unmount(&gp_ffs_gwui);
        gp_ffs_gwui = NULL;
    }
}

HTTP_SERVER_CB_STATIC
http_server_resp_t
http_server_cb_on_delete_ssl_cert(const char* const p_uri_params)
{
    LOG_DBG("DELETE /ssl_cert %s", (NULL != p_uri_params) ? p_uri_params : "NULL");
    str_buf_t filename_str_buf = http_server_get_from_params_with_decoding(p_uri_params, "file=");
    if (NULL == filename_str_buf.buf)
    {
        LOG_ERR("HTTP delete_ssl_cert: can't find 'file' in params: %s", p_uri_params);
        return http_server_resp_400();
    }

    if ((0 != strcmp(GW_CFG_STORAGE_SSL_HTTP_CLI_CERT, filename_str_buf.buf))
        && (0 != strcmp(GW_CFG_STORAGE_SSL_HTTP_CLI_KEY, filename_str_buf.buf))
        && (0 != strcmp(GW_CFG_STORAGE_SSL_STAT_CLI_CERT, filename_str_buf.buf))
        && (0 != strcmp(GW_CFG_STORAGE_SSL_STAT_CLI_KEY, filename_str_buf.buf))
        && (0 != strcmp(GW_CFG_STORAGE_SSL_MQTT_CLI_CERT, filename_str_buf.buf))
        && (0 != strcmp(GW_CFG_STORAGE_SSL_MQTT_CLI_KEY, filename_str_buf.buf))
        && (0 != strcmp(GW_CFG_STORAGE_SSL_REMOTE_CFG_CLI_CERT, filename_str_buf.buf))
        && (0 != strcmp(GW_CFG_STORAGE_SSL_REMOTE_CFG_CLI_KEY, filename_str_buf.buf))
        && (0 != strcmp(GW_CFG_STORAGE_SSL_HTTP_SRV_CERT, filename_str_buf.buf))
        && (0 != strcmp(GW_CFG_STORAGE_SSL_STAT_SRV_CERT, filename_str_buf.buf))
        && (0 != strcmp(GW_CFG_STORAGE_SSL_MQTT_SRV_CERT, filename_str_buf.buf))
        && (0 != strcmp(GW_CFG_STORAGE_SSL_REMOTE_CFG_SRV_CERT, filename_str_buf.buf)))
    {
        LOG_ERR("HTTP delete_ssl_cert: Unknown file name: %s", filename_str_buf.buf);
        str_buf_free_buf(&filename_str_buf);
        return http_server_resp_400();
    }

    if (!gw_cfg_storage_delete_file(filename_str_buf.buf))
    {
        LOG_ERR("HTTP delete_ssl_cert: Failed to delete file: %s", filename_str_buf.buf);
        str_buf_free_buf(&filename_str_buf);
        return http_server_resp_500();
    }

    str_buf_free_buf(&filename_str_buf);
    return http_server_resp_200_json("{}");
}

http_server_resp_t
http_server_cb_on_delete(
    const char* const               p_file_name,
    const char* const               p_uri_params,
    const bool                      flag_access_from_lan,
    const http_server_resp_t* const p_resp_auth)
{
    (void)flag_access_from_lan;
    (void)p_resp_auth;
    if (0 == strcmp(p_file_name, "ssl_cert"))
    {
        return http_server_cb_on_delete_ssl_cert(p_uri_params);
    }
    LOG_INFO("DELETE /%s, params=%s", p_file_name, (NULL != p_uri_params) ? p_uri_params : "");
    return http_server_resp_404();
}

static void
http_server_cb_on_user_req_download_latest_release_info(void)
{
    LOG_INFO("Download latest release info");
    http_server_download_info_t latest_release_info = http_download_latest_release_info(false);
    if (latest_release_info.is_error)
    {
        LOG_ERR("Failed to download latest firmware release info");
        main_task_schedule_retry_check_for_fw_updates();
        return;
    }
    LOG_INFO("github_latest_release.json: %s", latest_release_info.p_json_buf);

    main_task_schedule_next_check_for_fw_updates();

    bool      flag_found_tag_name    = false;
    time_t    unix_time_published_at = 0;
    str_buf_t tag_name               = json_helper_get_by_key(latest_release_info.p_json_buf, "tag_name");
    if (NULL != tag_name.buf)
    {
        LOG_INFO("github_latest_release.json: tag_name: %s", tag_name.buf);
        const ruuvi_esp32_fw_ver_str_t* const p_esp32_fw_ver = gw_cfg_get_esp32_fw_ver();
        if (0 == strcmp(p_esp32_fw_ver->buf, tag_name.buf))
        {
            LOG_INFO("github_latest_release.json: No update is required, the latest version is already installed");
            os_free(latest_release_info.p_json_buf);
            return;
        }
        LOG_INFO(
            "github_latest_release.json: Update is required (current version: %s, latest version: %s)",
            p_esp32_fw_ver->buf,
            tag_name.buf);

        fw_update_set_url("https://github.com/ruuvi/ruuvi.gateway_esp.c/releases/download/%s", tag_name.buf);
        flag_found_tag_name = true;
        str_buf_free_buf(&tag_name);
    }
    str_buf_t published_at = json_helper_get_by_key(latest_release_info.p_json_buf, "published_at");
    if (NULL != published_at.buf)
    {
        LOG_INFO("github_latest_release.json: published_at: %s", published_at.buf);
        unix_time_published_at = time_str_conv_to_unix_time(published_at.buf);
        str_buf_free_buf(&published_at);
    }
    os_free(latest_release_info.p_json_buf);

    if ((!flag_found_tag_name) || (0 == unix_time_published_at))
    {
        LOG_WARN("github_latest_release.json: 'tag_name' or 'published_at' is not found");
        return;
    }

    if (AUTO_UPDATE_CYCLE_TYPE_REGULAR == gw_cfg_get_auto_update_cycle())
    {
        const time_t cur_unix_time = http_server_get_cur_time();
        if ((cur_unix_time - unix_time_published_at) < FW_UPDATING_REGULAR_CYCLE_DELAY_SECONDS)
        {
            LOG_INFO(
                "github_latest_release.json: postpone the update because less than 14 days have passed since the "
                "update was published");
            return;
        }
    }

    LOG_INFO("github_latest_release.json: Run firmware auto-updating from URL: %s", fw_update_get_url());
    fw_update_run(true);
}

static http_server_download_info_t
http_server_download_gw_cfg(const ruuvi_gw_cfg_remote_t* const p_remote, const bool flag_free_memory)
{
    const mac_address_str_t* const p_nrf52_mac_addr = gw_cfg_get_nrf52_mac_addr();

    const http_header_item_t extra_header_item = {
        .p_key   = "ruuvi_gw_mac",
        .p_value = p_nrf52_mac_addr->str_buf,
    };

    size_t base_url_len = strlen(p_remote->url.buf);
    if (base_url_len < GW_CFG_REMOTE_URL_MIN_LEN)
    {
        LOG_ERR("Remote cfg URL is too short: '%s'", p_remote->url.buf);
        const http_server_download_info_t download_info = {
            .is_error       = true,
            .http_resp_code = HTTP_RESP_CODE_400,
            .p_json_buf     = NULL,
            .json_buf_size  = 0,
        };
        return download_info;
    }

    const TimeUnitsSeconds_t    timeout_seconds = 10;
    http_server_download_info_t download_info   = { 0 };

    str_buf_t str_buf_server_cert_remote = str_buf_init_null();
    str_buf_t str_buf_client_cert        = str_buf_init_null();
    str_buf_t str_buf_client_key         = str_buf_init_null();
    if (p_remote->use_ssl_client_cert)
    {
        str_buf_client_cert = gw_cfg_storage_read_file(GW_CFG_STORAGE_SSL_REMOTE_CFG_CLI_CERT);
        str_buf_client_key  = gw_cfg_storage_read_file(GW_CFG_STORAGE_SSL_REMOTE_CFG_CLI_KEY);
    }
    if (p_remote->use_ssl_server_cert)
    {
        str_buf_server_cert_remote = gw_cfg_storage_read_file(GW_CFG_STORAGE_SSL_REMOTE_CFG_SRV_CERT);
    }

    const char* const p_ext = strrchr(p_remote->url.buf, '.');
    if ((NULL != p_ext) && (0 == strcmp(".json", p_ext)))
    {
        LOG_INFO("Try to download gateway configuration from the remote server: %s", p_remote->url.buf);
        download_info = http_download_json(
            p_remote->url.buf,
            timeout_seconds,
            p_remote->auth_type,
            &p_remote->auth,
            &extra_header_item,
            flag_free_memory,
            str_buf_server_cert_remote.buf,
            str_buf_client_cert.buf,
            str_buf_client_key.buf);
    }
    else
    {
        if ('/' == p_remote->url.buf[base_url_len - 1])
        {
            base_url_len -= 1;
        }
        str_buf_t url = str_buf_printf_with_alloc(
            "%.*s/%.2s%.2s%.2s%.2s%.2s%.2s.json",
            (printf_int_t)base_url_len,
            &p_remote->url.buf[0],
            &p_nrf52_mac_addr->str_buf[MAC_ADDR_STR_BYTE_OFFSET(0)],
            &p_nrf52_mac_addr->str_buf[MAC_ADDR_STR_BYTE_OFFSET(1)],
            &p_nrf52_mac_addr->str_buf[MAC_ADDR_STR_BYTE_OFFSET(2)],
            &p_nrf52_mac_addr->str_buf[MAC_ADDR_STR_BYTE_OFFSET(3)],
            &p_nrf52_mac_addr->str_buf[MAC_ADDR_STR_BYTE_OFFSET(4)],
            &p_nrf52_mac_addr->str_buf[MAC_ADDR_STR_BYTE_OFFSET(5)]);
        if (NULL == url.buf)
        {
            LOG_ERR("Can't allocate memory for URL");
            download_info = (http_server_download_info_t) {
                .is_error       = true,
                .http_resp_code = HTTP_RESP_CODE_500,
                .p_json_buf     = NULL,
                .json_buf_size  = 0,
            };
            str_buf_free_buf(&str_buf_server_cert_remote);
            str_buf_free_buf(&str_buf_client_cert);
            str_buf_free_buf(&str_buf_client_key);
            return download_info;
        }
        LOG_INFO("Try to download gateway configuration from the remote server: %s", url.buf);
        download_info = http_download_json(
            url.buf,
            timeout_seconds,
            p_remote->auth_type,
            &p_remote->auth,
            &extra_header_item,
            flag_free_memory,
            str_buf_server_cert_remote.buf,
            str_buf_client_cert.buf,
            str_buf_client_key.buf);

        str_buf_free_buf(&url);

        if (download_info.is_error)
        {
            LOG_WARN("Download gw_cfg: failed, http_resp_code=%u", (printf_uint_t)download_info.http_resp_code);
            url = str_buf_printf_with_alloc("%.*s/gw_cfg.json", (printf_int_t)base_url_len, p_remote->url.buf);
            if (NULL == url.buf)
            {
                LOG_ERR("Can't allocate memory for URL");
                download_info = (http_server_download_info_t) {
                    .is_error       = true,
                    .http_resp_code = HTTP_RESP_CODE_500,
                    .p_json_buf     = NULL,
                    .json_buf_size  = 0,
                };
                str_buf_free_buf(&str_buf_server_cert_remote);
                str_buf_free_buf(&str_buf_client_cert);
                str_buf_free_buf(&str_buf_client_key);
                return download_info;
            }

            LOG_INFO("Try to download gateway configuration from the remote server: %s", url.buf);
            download_info = http_download_json(
                url.buf,
                timeout_seconds,
                p_remote->auth_type,
                &p_remote->auth,
                &extra_header_item,
                flag_free_memory,
                str_buf_server_cert_remote.buf,
                str_buf_client_cert.buf,
                str_buf_client_key.buf);

            str_buf_free_buf(&str_buf_server_cert_remote);
            str_buf_free_buf(&str_buf_client_cert);
            str_buf_free_buf(&str_buf_client_key);
            str_buf_free_buf(&url);
        }
    }
    return download_info;
}

http_resp_code_e
http_server_gw_cfg_download_and_parse(
    const ruuvi_gw_cfg_remote_t* const p_remote_cfg,
    const bool                         flag_free_memory,
    gw_cfg_t**                         p_p_gw_cfg_tmp,
    str_buf_t* const                   p_err_msg)
{
    const bool flag_wait_until_relaying_stopped = true;
    if (flag_free_memory || gw_cfg_get_mqtt_use_mqtt_over_ssl_or_wss())
    {
        gw_status_suspend_relaying(flag_wait_until_relaying_stopped);
    }
    else
    {
        gw_status_suspend_http_relaying(flag_wait_until_relaying_stopped);
    }

    http_server_download_info_t download_info = http_server_download_gw_cfg(p_remote_cfg, flag_free_memory);

    const bool flag_wait_until_relaying_resumed = true;
    if (flag_free_memory || gw_cfg_get_mqtt_use_mqtt_over_ssl_or_wss())
    {
        gw_status_resume_relaying(flag_wait_until_relaying_resumed);
    }
    else
    {
        gw_status_resume_http_relaying(flag_wait_until_relaying_resumed);
    }

    if (download_info.is_error)
    {
        LOG_ERR("Download gw_cfg: failed, http_resp_code=%u", (printf_uint_t)download_info.http_resp_code);
        return download_info.http_resp_code;
    }
    LOG_INFO("Download gw_cfg: successfully completed");
    LOG_DBG("gw_cfg.json: %s", download_info.p_json_buf);

    if ('{' != download_info.p_json_buf[0])
    {
        LOG_ERR(
            "Invalid first byte of json, expected '{', actual '%c' (%d)",
            download_info.p_json_buf[0],
            (printf_int_t)download_info.p_json_buf[0]);
        if (NULL != p_err_msg)
        {
            *p_err_msg = str_buf_printf_with_alloc(
                "Invalid first byte of json, expected '{', actual '%c' (%d)",
                download_info.p_json_buf[0],
                (printf_int_t)download_info.p_json_buf[0]);
        }
        os_free(download_info.p_json_buf);
        return HTTP_RESP_CODE_502;
    }

    gw_cfg_t* p_gw_cfg_tmp = os_calloc(1, sizeof(*p_gw_cfg_tmp));
    if (NULL == p_gw_cfg_tmp)
    {
        LOG_ERR("Failed to allocate memory for gw_cfg");
        if (NULL != p_err_msg)
        {
            *p_err_msg = str_buf_printf_with_alloc("Failed to allocate memory for gw_cfg");
        }
        os_free(download_info.p_json_buf);
        return HTTP_RESP_CODE_502;
    }
    gw_cfg_get_copy(p_gw_cfg_tmp);
    p_gw_cfg_tmp->ruuvi_cfg.remote.use_remote_cfg = false;

    if (!gw_cfg_json_parse(
            "gw_cfg.json",
            "Read Gateway SETTINGS from remote server:",
            download_info.p_json_buf,
            p_gw_cfg_tmp))
    {
        LOG_ERR("Failed to parse gw_cfg.json or no memory");
        if (NULL != p_err_msg)
        {
            *p_err_msg = str_buf_printf_with_alloc("Failed to parse gw_cfg.json or no memory");
        }
        os_free(p_gw_cfg_tmp);
        os_free(download_info.p_json_buf);
        return HTTP_RESP_CODE_502;
    }
    os_free(download_info.p_json_buf);

    if (!p_gw_cfg_tmp->ruuvi_cfg.remote.use_remote_cfg)
    {
        LOG_ERR("Invalid gw_cfg.json: 'use_remote_cfg' is missing or 'false'");
        if (NULL != p_err_msg)
        {
            *p_err_msg = str_buf_printf_with_alloc("Invalid gw_cfg.json: 'use_remote_cfg' is missing or 'false'");
        }
        os_free(p_gw_cfg_tmp);
        return HTTP_RESP_CODE_502;
    }
    *p_p_gw_cfg_tmp = p_gw_cfg_tmp;
    return HTTP_RESP_CODE_200;
}

http_resp_code_e
http_server_gw_cfg_download_and_update(
    bool* const      p_flag_reboot_needed,
    const bool       flag_free_memory,
    str_buf_t* const p_err_msg)
{
    gw_cfg_t*                    p_gw_cfg_tmp = NULL;
    const ruuvi_gw_cfg_remote_t* p_remote_cfg = gw_cfg_get_remote_cfg_copy();
    if (NULL == p_remote_cfg)
    {
        return HTTP_RESP_CODE_502;
    }
    const http_resp_code_e resp_code = http_server_gw_cfg_download_and_parse(
        p_remote_cfg,
        flag_free_memory,
        &p_gw_cfg_tmp,
        p_err_msg);
    os_free(p_remote_cfg);
    if (HTTP_RESP_CODE_200 != resp_code)
    {
        return resp_code;
    }
    if (NULL != p_err_msg)
    {
        str_buf_free_buf(p_err_msg);
    }

    const gw_cfg_update_status_t update_status = gw_cfg_update(p_gw_cfg_tmp);
    if (update_status.flag_eth_cfg_modified || update_status.flag_wifi_ap_cfg_modified
        || update_status.flag_wifi_sta_cfg_modified)
    {
        LOG_INFO("Network configuration in gw_cfg.json differs from the current settings, need to restart gateway");
        if (NULL != p_flag_reboot_needed)
        {
            *p_flag_reboot_needed = true;
        }
        reset_task_reboot_after_timeout();
    }
    else if (update_status.flag_ruuvi_cfg_modified)
    {
        LOG_INFO("Ruuvi configuration in gw_cfg.json differs from the current settings, need to restart services");
        main_task_send_sig_restart_services();
    }
    else
    {
        LOG_INFO("Gateway SETTINGS (from remote server) are the same as the current ones");
    }
    os_free(p_gw_cfg_tmp);
    return HTTP_RESP_CODE_200;
}

void
http_server_cb_on_user_req(const http_server_user_req_code_e req_code)
{
    switch (req_code)
    {
        case HTTP_SERVER_USER_REQ_CODE_DOWNLOAD_LATEST_RELEASE_INFO:
            http_server_cb_on_user_req_download_latest_release_info();
            break;
        case HTTP_SERVER_USER_REQ_CODE_DOWNLOAD_GW_CFG:
            (void)http_server_gw_cfg_download_and_update(NULL, false, NULL);
            break;
        default:
            LOG_ERR("Unknown req_code=%d", (printf_int_t)req_code);
            break;
    }
}

ATTR_PRINTF(2, 3)
ATTR_NONNULL(2)
http_server_resp_t
http_server_cb_gen_resp(const http_resp_code_e resp_code, const char* const p_fmt, ...)
{
    va_list args;
    va_start(args, p_fmt);
    str_buf_t msg = str_buf_vprintf_with_alloc(p_fmt, args);
    va_end(args);
    if (NULL == msg.buf)
    {
        LOG_ERR("Can't allocate memory for response");
        return http_server_resp_500();
    }

    const str_buf_t resp_buf = str_buf_printf_with_alloc(
        "{\"status\": %u, \"message\": \"%s\"}",
        (printf_uint_t)resp_code,
        (NULL != msg.buf) ? msg.buf : "");
    str_buf_free_buf(&msg);
    if (NULL == resp_buf.buf)
    {
        LOG_ERR("Can't allocate memory for response");
        return http_server_resp_500();
    }
    return http_server_resp_json_in_heap(HTTP_RESP_CODE_200, resp_buf.buf);
}

HTTP_SERVER_CB_STATIC
str_buf_t
http_server_get_from_params(const char* const p_params, const char* const p_key)
{
    const size_t      key_len = strlen(p_key);
    const char* const p_param = strstr(p_params, p_key);
    if (NULL == p_param)
    {
        LOG_DBG("Can't find key '%s' in URL params", p_key);
        return str_buf_init_null();
    }
    const char* const p_value = &p_param[key_len];
    const char* const p_end   = strchr(p_value, '&');
    const size_t      val_len = (NULL == p_end) ? strlen(p_value) : (size_t)(ptrdiff_t)(p_end - p_value);
    LOG_DBG("HTTP params: %s%.*s", p_key, val_len, p_value);

    const str_buf_t result = str_buf_printf_with_alloc("%.*s", (printf_int_t)val_len, p_value);
    if (NULL == result.buf)
    {
        LOG_ERR("Can't allocate memory param %s%.*s", p_key, val_len, p_value);
    }
    return result;
}

str_buf_t
http_server_get_from_params_with_decoding(const char* const p_params, const char* const p_key)
{
    str_buf_t val_encoded = http_server_get_from_params(p_params, p_key);
    if (NULL == val_encoded.buf)
    {
        LOG_ERR("HTTP params: Can't find '%s'", p_key);
        return str_buf_init_null();
    }
    LOG_DBG("HTTP params: key '%s': value (encoded): %s", p_key, val_encoded.buf);

    const str_buf_t val_decoded = url_decode_with_alloc(val_encoded.buf);
    str_buf_free_buf(&val_encoded);
    if (NULL == val_decoded.buf)
    {
        LOG_ERR("HTTP params: key '%s': Can't decode value: %s", p_key, val_encoded.buf);
        return str_buf_init_null();
    }
    LOG_DBG("HTTP params: key '%s': value (decoded): %s", p_key, val_decoded.buf);
    return val_decoded;
}
