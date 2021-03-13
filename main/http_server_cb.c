/**
 * @file http_server_cb.c
 * @author TheSomeMan
 * @date 2020-10-26
 * @copyright Ruuvi Innovations Ltd, license BSD-3-Clause.
 */

#include "http_server_cb.h"
#include <string.h>
#include "settings.h"
#include "ruuvi_gateway.h"
#include "cjson_wrap.h"
#include "wifi_manager.h"
#include "ethernet.h"
#include "json_ruuvi.h"
#include "flashfatfs.h"
#include "metrics.h"
#include "http_json.h"
#include "os_malloc.h"

#define LOG_LOCAL_LEVEL LOG_LEVEL_DEBUG
#include "log.h"

#define HTTP_SERVER_DEFAULT_HISTORY_INTERVAL_SECONDS (60U)

static const char TAG[] = "http_server";

static const char g_empty_json[] = "{}";

static const flash_fat_fs_t *gp_ffs_gwui;

bool
http_server_cb_init(void)
{
    const char *                   mount_point   = "/fs_gwui";
    const flash_fat_fs_num_files_t max_num_files = 4U;

    gp_ffs_gwui = flashfatfs_mount(mount_point, GW_GWUI_PARTITION, max_num_files);
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
http_server_resp_json_ruuvi(void)
{
    cjson_wrap_str_t json_str = cjson_wrap_str_null();
    if (!gw_cfg_generate_json_str(&json_str))
    {
        return http_server_resp_503();
    }
    LOG_INFO("ruuvi.json: %s", json_str.p_str);
    const bool flag_no_cache = true;
    return http_server_resp_data_in_heap(
        HTTP_CONENT_TYPE_APPLICATION_JSON,
        NULL,
        strlen(json_str.p_str),
        HTTP_CONENT_ENCODING_NONE,
        (const uint8_t *)json_str.p_str,
        flag_no_cache);
}

HTTP_SERVER_CB_STATIC
http_server_resp_t
http_server_resp_json(const char *p_file_name)
{
    if (0 == strcmp(p_file_name, "ruuvi.json"))
    {
        return http_server_resp_json_ruuvi();
    }
    LOG_WARN("Request to unknown json: %s", p_file_name);
    return http_server_resp_404();
}

HTTP_SERVER_CB_STATIC
http_server_resp_t
http_server_resp_metrics(void)
{
    const char *p_metrics = metrics_generate();
    if (NULL == p_metrics)
    {
        LOG_ERR("Not enough memory");
        return http_server_resp_503();
    }
    const bool flag_no_cache = true;
    LOG_INFO("metrics: %s", p_metrics);
    return http_server_resp_data_in_heap(
        HTTP_CONENT_TYPE_TEXT_PLAIN,
        "version=0.0.4",
        strlen(p_metrics),
        HTTP_CONENT_ENCODING_NONE,
        (const uint8_t *)p_metrics,
        flag_no_cache);
}

HTTP_SERVER_CB_STATIC
bool
http_server_read_history(cjson_wrap_str_t *p_json_str, const time_t cur_time, const uint32_t time_interval_seconds)
{
    adv_report_table_t *p_reports = os_malloc(sizeof(*p_reports));
    if (NULL == p_reports)
    {
        return false;
    }
    adv_table_history_read(p_reports, cur_time, time_interval_seconds);

    const bool res = http_create_json_str(p_reports, cur_time, &gw_mac_sta, g_gateway_config.coordinates, p_json_str);
    os_free(p_reports);
    return res;
}

#if !RUUVI_TESTS_HTTP_SERVER_CB
static time_t
http_server_get_cur_time(void)
{
    return time(NULL);
}
#endif

HTTP_SERVER_CB_STATIC
http_server_resp_t
http_server_resp_history(const char *const p_params)
{
    uint32_t time_interval_seconds = HTTP_SERVER_DEFAULT_HISTORY_INTERVAL_SECONDS;
    if (NULL != p_params)
    {
        const char *const p_time_prefix   = "&time=";
        const size_t      time_prefix_len = strlen(p_time_prefix);
        if (0 == strncmp(p_params, p_time_prefix, time_prefix_len))
        {
            time_interval_seconds = (uint32_t)strtoul(&p_params[time_prefix_len], NULL, 0);
        }
    }
    cjson_wrap_str_t json_str = cjson_wrap_str_null();
    const time_t     cur_time = http_server_get_cur_time();
    if (!http_server_read_history(&json_str, cur_time, time_interval_seconds))
    {
        LOG_ERR("Not enough memory");
        return http_server_resp_503();
    }

    const bool flag_no_cache = true;
    LOG_INFO("History on %u seconds interval: %s", (unsigned)time_interval_seconds, json_str.p_str);
    return http_server_resp_data_in_heap(
        HTTP_CONENT_TYPE_APPLICATION_JSON,
        NULL,
        strlen(json_str.p_str),
        HTTP_CONENT_ENCODING_NONE,
        (const uint8_t *)json_str.p_str,
        flag_no_cache);
}

HTTP_SERVER_CB_STATIC
http_content_type_e
http_get_content_type_by_ext(const char *p_file_ext)
{
    if (0 == strcmp(p_file_ext, ".html"))
    {
        return HTTP_CONENT_TYPE_TEXT_HTML;
    }
    if ((0 == strcmp(p_file_ext, ".css")) || (0 == strcmp(p_file_ext, ".scss")))
    {
        return HTTP_CONENT_TYPE_TEXT_CSS;
    }
    if (0 == strcmp(p_file_ext, ".js"))
    {
        return HTTP_CONENT_TYPE_TEXT_JAVASCRIPT;
    }
    if (0 == strcmp(p_file_ext, ".png"))
    {
        return HTTP_CONENT_TYPE_IMAGE_PNG;
    }
    if (0 == strcmp(p_file_ext, ".svg"))
    {
        return HTTP_CONENT_TYPE_IMAGE_SVG_XML;
    }
    if (0 == strcmp(p_file_ext, ".ttf"))
    {
        return HTTP_CONENT_TYPE_APPLICATION_OCTET_STREAM;
    }
    return HTTP_CONENT_TYPE_APPLICATION_OCTET_STREAM;
}

HTTP_SERVER_CB_STATIC
http_server_resp_t
http_server_resp_file(const char *file_path)
{
    LOG_DBG("Try to find file: %s", file_path);
    if (NULL == gp_ffs_gwui)
    {
        LOG_ERR("GWUI partition is not ready");
        return http_server_resp_503();
    }
    const flash_fat_fs_t *p_ffs = gp_ffs_gwui;

    const char *p_file_ext = strrchr(file_path, '.');
    if (NULL == p_file_ext)
    {
        p_file_ext = "";
    }

    size_t       file_size         = 0;
    bool         is_gzipped        = false;
    char         tmp_file_path[64] = { '\0' };
    const char * suffix_gz         = ".gz";
    const size_t suffix_gz_len     = strlen(suffix_gz);
    if ((strlen(file_path) + suffix_gz_len) >= sizeof(tmp_file_path))
    {
        LOG_ERR("Temporary buffer is not enough for the file path '%s'", file_path);
        return http_server_resp_503();
    }
    if ((0 == strcmp(".js", p_file_ext)) || (0 == strcmp(".html", p_file_ext)) || (0 == strcmp(".css", p_file_ext)))
    {
        snprintf(tmp_file_path, sizeof(tmp_file_path), "%s%s", file_path, suffix_gz);
        if (flashfatfs_get_file_size(p_ffs, tmp_file_path, &file_size))
        {
            is_gzipped = true;
        }
    }
    if (!is_gzipped)
    {
        snprintf(tmp_file_path, sizeof(tmp_file_path), "%s", file_path);
        if (!flashfatfs_get_file_size(p_ffs, tmp_file_path, &file_size))
        {
            LOG_ERR("Can't find file: %s", tmp_file_path);
            return http_server_resp_404();
        }
    }
    const http_content_type_e     content_type     = http_get_content_type_by_ext(p_file_ext);
    const http_content_encoding_e content_encoding = is_gzipped ? HTTP_CONENT_ENCODING_GZIP : HTTP_CONENT_ENCODING_NONE;

    const file_descriptor_t fd = flashfatfs_open(p_ffs, tmp_file_path);
    if (fd < 0)
    {
        LOG_ERR("Can't open file: %s", tmp_file_path);
        return http_server_resp_503();
    }
    LOG_DBG("File %s was opened successfully, fd=%d", tmp_file_path, fd);
    return http_server_resp_data_from_file(content_type, NULL, file_size, content_encoding, fd);
}

static bool
http_server_is_url_prefix_eq(
    const char *const p_path,
    const size_t      path_page_name_len,
    const char *const p_exp_page_name)
{
    const size_t exp_page_name_len = strlen(p_exp_page_name);
    if (exp_page_name_len != path_page_name_len)
    {
        return false;
    }
    if (0 != strncmp(p_path, p_exp_page_name, path_page_name_len))
    {
        return false;
    }
    return true;
}

http_server_resp_t
http_server_cb_on_get(const char *const p_path)
{
    const char *p_file_ext = strrchr(p_path, '.');
    LOG_INFO("GET /%s", p_path);

    if ((NULL != p_file_ext) && (0 == strcmp(p_file_ext, ".json")))
    {
        return http_server_resp_json(p_path);
    }
    size_t      len      = strlen(p_path);
    const char *p_params = strchr(p_path, '&');
    if (NULL != p_params)
    {
        len = (size_t)(ptrdiff_t)(p_params - p_path);
    }
    if (http_server_is_url_prefix_eq(p_path, len, "metrics"))
    {
        return http_server_resp_metrics();
    }
    if (http_server_is_url_prefix_eq(p_path, len, "history"))
    {
        return http_server_resp_history(p_params);
    }
    const char *p_file_path = ('\0' == p_path[0]) ? "index.html" : p_path;
    return http_server_resp_file(p_file_path);
}

HTTP_SERVER_CB_STATIC
http_server_resp_t
http_server_cb_on_post_ruuvi(const char *p_body)
{
    LOG_DBG("POST /ruuvi.json");
    if (!json_ruuvi_parse_http_body(p_body, &g_gateway_config))
    {
        return http_server_resp_503();
    }
    gw_cfg_print_to_log(&g_gateway_config);
    settings_save_to_flash(&g_gateway_config);
    ruuvi_send_nrf_settings(&g_gateway_config);
    ethernet_update_ip();
    return http_server_resp_data_in_flash(
        HTTP_CONENT_TYPE_APPLICATION_JSON,
        NULL,
        strlen(g_empty_json),
        HTTP_CONENT_ENCODING_NONE,
        (const uint8_t *)g_empty_json);
}

http_server_resp_t
http_server_cb_on_post(const char *p_file_name, const char *p_body)
{
    if (0 == strcmp(p_file_name, "ruuvi.json"))
    {
        return http_server_cb_on_post_ruuvi(p_body);
    }
    LOG_WARN("POST /%s", p_file_name);
    return http_server_resp_404();
}

http_server_resp_t
http_server_cb_on_delete(const char *p_path)
{
    (void)p_path;
    LOG_WARN("DELETE /%s", p_path);
    return http_server_resp_404();
}
