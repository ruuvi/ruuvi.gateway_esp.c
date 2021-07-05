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
#include "http_server.h"
#include "http.h"
#include "fw_update.h"
#include "nrf52fw.h"
#include "adv_post.h"

#if RUUVI_TESTS_HTTP_SERVER_CB
#define LOG_LOCAL_LEVEL LOG_LEVEL_DEBUG
#else
#define LOG_LOCAL_LEVEL LOG_LEVEL_INFO
#endif
#include "log.h"

#define HTTP_SERVER_DEFAULT_HISTORY_INTERVAL_SECONDS (60U)

static const char TAG[] = "http_server";

static const char g_empty_json[] = "{}";

static const flash_fat_fs_t *gp_ffs_gwui;

#if !RUUVI_TESTS_HTTP_SERVER_CB
static time_t
http_server_get_cur_time(void)
{
    return time(NULL);
}
#endif

bool
http_server_cb_init(const char *const p_fatfs_gwui_partition_name)
{
    const char *                   mount_point   = "/fs_gwui";
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
http_server_resp_json_ruuvi(void)
{
    cjson_wrap_str_t json_str = cjson_wrap_str_null();
    if (!gw_cfg_generate_json_str(&json_str))
    {
        return http_server_resp_503();
    }
    LOG_INFO("ruuvi.json: %s", json_str.p_str);
    const bool flag_no_cache        = true;
    const bool flag_add_header_date = true;
    return http_server_resp_data_in_heap(
        HTTP_CONENT_TYPE_APPLICATION_JSON,
        NULL,
        strlen(json_str.p_str),
        HTTP_CONENT_ENCODING_NONE,
        (const uint8_t *)json_str.p_str,
        flag_no_cache,
        flag_add_header_date);
}

static bool
json_info_add_string(cJSON *p_json_root, const char *p_item_name, const char *p_val)
{
    if (NULL == cJSON_AddStringToObject(p_json_root, p_item_name, p_val))
    {
        LOG_ERR("Can't add json item: %s", p_item_name);
        return false;
    }
    return true;
}

static bool
json_info_add_uint32(cJSON *p_json_root, const char *p_item_name, const uint32_t val)
{
    if (NULL == cJSON_AddNumberToObject(p_json_root, p_item_name, (double)val))
    {
        LOG_ERR("Can't add json item: %s", p_item_name);
        return false;
    }
    return true;
}

static bool
json_info_add_items(cJSON *p_json_root, const ruuvi_gateway_config_t *p_cfg, const mac_address_str_t *p_mac_sta)
{
    if (!json_info_add_string(p_json_root, "ESP_FW", fw_update_get_cur_version()))
    {
        return false;
    }
    if (!json_info_add_string(p_json_root, "NRF_FW", g_nrf52_firmware_version))
    {
        return false;
    }
    const mac_address_str_t nrf52_mac_addr = nrf52_get_mac_address_str();
    if (!json_info_add_string(p_json_root, "DEVICE_ADDR", nrf52_mac_addr.str_buf))
    {
        return false;
    }
    const nrf52_device_id_str_t nrf52_device_id = nrf52_get_device_id_str();
    if (!json_info_add_string(p_json_root, "DEVICE_ID", nrf52_device_id.str_buf))
    {
        return false;
    }
    if (!json_info_add_string(p_json_root, "ETHERNET_MAC", g_gw_mac_eth_str.str_buf))
    {
        return false;
    }
    if (!json_info_add_string(p_json_root, "WIFI_MAC", g_gw_mac_wifi_str.str_buf))
    {
        return false;
    }
    const time_t        cur_time  = http_server_get_cur_time();
    adv_report_table_t *p_reports = os_malloc(sizeof(*p_reports));
    if (NULL == p_reports)
    {
        return false;
    }
    adv_table_history_read(p_reports, cur_time, HTTP_SERVER_DEFAULT_HISTORY_INTERVAL_SECONDS);
    const num_of_advs_t num_of_advs = p_reports->num_of_advs;
    os_free(p_reports);

    if (!json_info_add_uint32(p_json_root, "TAGS_SEEN", num_of_advs))
    {
        return false;
    }
    return true;
}

static bool
json_info_generate_str(cjson_wrap_str_t *p_json_str)
{
    const ruuvi_gateway_config_t *p_cfg     = &g_gateway_config;
    const mac_address_str_t *     p_mac_sta = &g_gw_mac_sta_str;

    p_json_str->p_str = NULL;

    cJSON *p_json_root = cJSON_CreateObject();
    if (NULL == p_json_root)
    {
        LOG_ERR("Can't create json object");
        return false;
    }
    if (!json_info_add_items(p_json_root, p_cfg, p_mac_sta))
    {
        cjson_wrap_delete(&p_json_root);
        return false;
    }

    *p_json_str = cjson_wrap_print_and_delete(&p_json_root);
    if (NULL == p_json_str->p_str)
    {
        LOG_ERR("Can't create json string");
        return false;
    }
    return true;
}

HTTP_SERVER_CB_STATIC
http_server_resp_t
http_server_resp_json_info(void)
{
    cjson_wrap_str_t json_str = cjson_wrap_str_null();
    if (!json_info_generate_str(&json_str))
    {
        return http_server_resp_503();
    }
    LOG_INFO("info.json: %s", json_str.p_str);
    const bool flag_no_cache        = true;
    const bool flag_add_header_date = true;
    return http_server_resp_data_in_heap(
        HTTP_CONENT_TYPE_APPLICATION_JSON,
        NULL,
        strlen(json_str.p_str),
        HTTP_CONENT_ENCODING_NONE,
        (const uint8_t *)json_str.p_str,
        flag_no_cache,
        flag_add_header_date);
}

typedef struct download_github_latest_release_info_t
{
    bool   is_error;
    char * p_json_buf;
    size_t json_buf_size;
} download_github_latest_release_info_t;

static void
cb_on_http_data_json_github_latest_release(
    const uint8_t *const p_buf,
    const size_t         buf_size,
    const size_t         offset,
    const size_t         content_length,
    void *               p_user_data)
{
    download_github_latest_release_info_t *const p_info = p_user_data;
    if (p_info->is_error)
    {
        return;
    }
    if (NULL == p_info->p_json_buf)
    {
        if (0 == content_length)
        {
            p_info->is_error = true;
            LOG_ERR("Content length is zero");
            return;
        }
        p_info->json_buf_size = content_length + 1;
        p_info->p_json_buf    = os_calloc(1, p_info->json_buf_size);
        if (NULL == p_info->p_json_buf)
        {
            p_info->is_error = true;
            LOG_ERR("Can't allocate %lu bytes for github_latest_release.json", (printf_ulong_t)content_length);
            return;
        }
    }
    if ((offset >= p_info->json_buf_size) || ((offset + buf_size) >= p_info->json_buf_size))
    {
        p_info->is_error = true;
        LOG_ERR(
            "Buffer overflow while downloading github_latest_release.json: json_buf_size=%lu, offset=%lu, len=%lu",
            (printf_ulong_t)p_info->json_buf_size,
            (printf_ulong_t)offset,
            (printf_ulong_t)buf_size);
        return;
    }
    memcpy(&p_info->p_json_buf[offset], p_buf, buf_size);
}

HTTP_SERVER_CB_STATIC
http_server_resp_t
http_server_resp_json_github_latest_release(void)
{
    download_github_latest_release_info_t info = {
        .is_error      = false,
        .p_json_buf    = NULL,
        .json_buf_size = 0,
    };
    if (!http_download(
            "https://api.github.com/repos/ruuvi/ruuvi.gateway_esp.c/releases/latest",
            &cb_on_http_data_json_github_latest_release,
            &info))
    {
        return http_server_resp_504();
    }
    if (NULL == info.p_json_buf)
    {
        return http_server_resp_504();
    }

    cjson_wrap_str_t json_str = { .p_str = info.p_json_buf };
    LOG_INFO("github_latest_release.json: %s", json_str.p_str);
    const bool flag_no_cache        = true;
    const bool flag_add_header_date = true;
    return http_server_resp_data_in_heap(
        HTTP_CONENT_TYPE_APPLICATION_JSON,
        NULL,
        strlen(json_str.p_str),
        HTTP_CONENT_ENCODING_NONE,
        (const uint8_t *)json_str.p_str,
        flag_no_cache,
        flag_add_header_date);
}

HTTP_SERVER_CB_STATIC
http_server_resp_t
http_server_resp_json(const char *p_file_name, const bool flag_access_from_lan)
{
    if (0 == strcmp(p_file_name, "ruuvi.json"))
    {
        return http_server_resp_json_ruuvi();
    }
    else if (0 == strcmp(p_file_name, "github_latest_release.json"))
    {
        return http_server_resp_json_github_latest_release();
    }
    else if ((0 == strcmp(p_file_name, "info.json")) && (!flag_access_from_lan))
    {
        return http_server_resp_json_info();
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
    const bool flag_no_cache        = true;
    const bool flag_add_header_date = true;
    LOG_INFO("metrics: %s", p_metrics);
    return http_server_resp_data_in_heap(
        HTTP_CONENT_TYPE_TEXT_PLAIN,
        "version=0.0.4",
        strlen(p_metrics),
        HTTP_CONENT_ENCODING_NONE,
        (const uint8_t *)p_metrics,
        flag_no_cache,
        flag_add_header_date);
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

    const bool res = http_create_json_str(
        p_reports,
        cur_time,
        &g_gw_mac_sta_str,
        g_gateway_config.coordinates,
        false,
        0,
        p_json_str);
    os_free(p_reports);
    return res;
}

HTTP_SERVER_CB_STATIC
http_server_resp_t
http_server_resp_history(const char *const p_params)
{
    uint32_t time_interval_seconds = HTTP_SERVER_DEFAULT_HISTORY_INTERVAL_SECONDS;
    if (NULL != p_params)
    {
        const char *const p_time_prefix   = "time=";
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

    const bool flag_no_cache        = true;
    const bool flag_add_header_date = true;
    LOG_INFO("History on %u seconds interval: %s", (unsigned)time_interval_seconds, json_str.p_str);
    return http_server_resp_data_in_heap(
        HTTP_CONENT_TYPE_APPLICATION_JSON,
        NULL,
        strlen(json_str.p_str),
        HTTP_CONENT_ENCODING_NONE,
        (const uint8_t *)json_str.p_str,
        flag_no_cache,
        flag_add_header_date);
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
http_server_resp_file(const char *file_path, const http_resp_code_e http_resp_code)
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
    return http_server_resp_data_from_file(http_resp_code, content_type, NULL, file_size, content_encoding, fd);
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
http_server_cb_on_get(const char *p_path, const bool flag_access_from_lan, const http_server_resp_t *const p_resp_auth)
{
    const char *p_file_ext = strrchr(p_path, '.');
    LOG_INFO("GET /%s", p_path);

    if ((NULL != p_file_ext) && (0 == strcmp(p_file_ext, ".json")))
    {
        return http_server_resp_json(p_path, flag_access_from_lan);
    }
    size_t      len      = strlen(p_path);
    const char *p_params = strchr(p_path, '?');
    if (NULL != p_params)
    {
        len = (size_t)(ptrdiff_t)(p_params - p_path);
        p_params += 1;
    }
    if (http_server_is_url_prefix_eq(p_path, len, "metrics"))
    {
        return http_server_resp_metrics();
    }
    if (http_server_is_url_prefix_eq(p_path, len, "history"))
    {
        return http_server_resp_history(p_params);
    }
    const char *p_file_path = ('\0' == p_path[0]) ? "ruuvi.html" : p_path;
    return http_server_resp_file(p_file_path, (NULL != p_resp_auth) ? p_resp_auth->http_resp_code : HTTP_RESP_CODE_200);
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
    if (!http_server_set_auth(
            g_gateway_config.lan_auth.lan_auth_type,
            g_gateway_config.lan_auth.lan_auth_user,
            g_gateway_config.lan_auth.lan_auth_pass))
    {
        LOG_ERR("%s failed", "http_server_set_auth");
    }
    ruuvi_send_nrf_settings(&g_gateway_config);
    ethernet_update_ip();
    return http_server_resp_data_in_flash(
        HTTP_CONENT_TYPE_APPLICATION_JSON,
        NULL,
        strlen(g_empty_json),
        HTTP_CONENT_ENCODING_NONE,
        (const uint8_t *)g_empty_json);
}

HTTP_SERVER_CB_STATIC
http_server_resp_t
http_server_cb_on_post_fw_update(const char *p_body)
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
    if (!fw_update_run())
    {
        fw_update_set_extra_info_for_status_json_update_failed("Internal error");
        return http_server_resp_503();
    }
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
    else if (0 == strcmp(p_file_name, "fw_update.json"))
    {
        return http_server_cb_on_post_fw_update(p_body);
    }
    LOG_WARN("POST /%s", p_file_name);
    return http_server_resp_404();
}

http_server_resp_t
http_server_cb_on_delete(
    const char *                    p_path,
    const bool                      flag_access_from_lan,
    const http_server_resp_t *const p_resp_auth)
{
    (void)p_path;
    (void)p_resp_auth;
    LOG_WARN("DELETE /%s", p_path);
    return http_server_resp_404();
}
