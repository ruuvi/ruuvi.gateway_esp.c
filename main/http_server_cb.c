/**
 * @file http_server_cb.c
 * @author TheSomeMan
 * @date 2020-10-26
 * @copyright Ruuvi Innovations Ltd, license BSD-3-Clause.
 */

#include "http_server_cb.h"
#include <string.h>
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
#include "adv_post.h"
#include "json_helper.h"
#include "os_time.h"
#include "time_str.h"
#include "reset_task.h"
#include "gw_cfg.h"
#include "gw_cfg_ruuvi_json.h"
#include "gw_cfg_json_parse.h"
#include "time_units.h"

#if RUUVI_TESTS_HTTP_SERVER_CB
#define LOG_LOCAL_LEVEL LOG_LEVEL_DEBUG
#else
#define LOG_LOCAL_LEVEL LOG_LEVEL_INFO
#endif
#include "log.h"

#if (LOG_LOCAL_LEVEL >= LOG_LEVEL_DEBUG) && !RUUVI_TESTS
#warning Debug log level prints out the passwords as a "plaintext".
#endif

#define HTTP_SERVER_DEFAULT_HISTORY_INTERVAL_SECONDS (60U)

typedef double cjson_double_t;

static const char TAG[] = "http_server";

static const char g_empty_json[] = "{}";

static const flash_fat_fs_t *gp_ffs_gwui;

static bool g_http_server_cb_flag_prohibit_cfg_updating = false;

HTTP_SERVER_CB_STATIC
http_server_resp_t
http_server_cb_on_post_gw_cfg_download(void);

HTTP_SERVER_CB_STATIC
http_resp_code_e
http_server_gw_cfg_download_and_update(bool *const p_flag_reboot_needed);

#if !RUUVI_TESTS_HTTP_SERVER_CB
static time_t
http_server_get_cur_time(void)
{
    return os_time_get();
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
    const gw_cfg_t * p_gw_cfg = gw_cfg_lock_ro();
    cjson_wrap_str_t json_str = cjson_wrap_str_null();
    if (!gw_cfg_ruuvi_json_generate(p_gw_cfg, &json_str))
    {
        gw_cfg_unlock_ro(&p_gw_cfg);
        return http_server_resp_503();
    }
    gw_cfg_unlock_ro(&p_gw_cfg);

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
    if (NULL == cJSON_AddNumberToObject(p_json_root, p_item_name, (cjson_double_t)val))
    {
        LOG_ERR("Can't add json item: %s", p_item_name);
        return false;
    }
    return true;
}

static bool
json_info_add_items(cJSON *p_json_root, const bool flag_use_timestamps)
{
    if (!json_info_add_string(p_json_root, "ESP_FW", gw_cfg_get_esp32_fw_ver()->buf))
    {
        return false;
    }
    if (!json_info_add_string(p_json_root, "NRF_FW", gw_cfg_get_nrf52_fw_ver()->buf))
    {
        return false;
    }
    if (!json_info_add_string(p_json_root, "DEVICE_ADDR", gw_cfg_get_nrf52_mac_addr()->str_buf))
    {
        return false;
    }
    if (!json_info_add_string(p_json_root, "DEVICE_ID", gw_cfg_get_nrf52_device_id()->str_buf))
    {
        return false;
    }
    if (!json_info_add_string(p_json_root, "ETHERNET_MAC", gw_cfg_get_esp32_mac_addr_eth()->str_buf))
    {
        return false;
    }
    if (!json_info_add_string(p_json_root, "WIFI_MAC", gw_cfg_get_esp32_mac_addr_wifi()->str_buf))
    {
        return false;
    }
    const time_t        cur_time  = http_server_get_cur_time();
    adv_report_table_t *p_reports = os_malloc(sizeof(*p_reports));
    if (NULL == p_reports)
    {
        return false;
    }
    const bool flag_use_filter = flag_use_timestamps;
    adv_table_history_read(
        p_reports,
        cur_time,
        flag_use_timestamps,
        flag_use_timestamps ? HTTP_SERVER_DEFAULT_HISTORY_INTERVAL_SECONDS : 0,
        flag_use_filter);
    const num_of_advs_t num_of_advs = p_reports->num_of_advs;
    os_free(p_reports);

    if (!json_info_add_uint32(p_json_root, "TAGS_SEEN", num_of_advs))
    {
        return false;
    }
    if (!json_info_add_uint32(p_json_root, "BUTTON_PRESSES", g_cnt_cfg_button_pressed))
    {
        return false;
    }
    return true;
}

static bool
generate_json_info_str(cjson_wrap_str_t *p_json_str, const bool flag_use_timestamps)
{
    p_json_str->p_str = NULL;

    cJSON *p_json_root = cJSON_CreateObject();
    if (NULL == p_json_root)
    {
        LOG_ERR("Can't create json object");
        return false;
    }
    if (!json_info_add_items(p_json_root, flag_use_timestamps))
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
    const gw_cfg_t * p_gw_cfg = gw_cfg_lock_ro();
    cjson_wrap_str_t json_str = cjson_wrap_str_null();
    if (!generate_json_info_str(&json_str, gw_cfg_get_ntp_use()))
    {
        gw_cfg_unlock_ro(&p_gw_cfg);
        return http_server_resp_503();
    }
    gw_cfg_unlock_ro(&p_gw_cfg);
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

static bool
cb_on_http_download_json_data(
    const uint8_t *const   p_buf,
    const size_t           buf_size,
    const size_t           offset,
    const size_t           content_length,
    const http_resp_code_e http_resp_code,
    void *                 p_user_data)
{
    LOG_INFO("%s: buf_size=%lu", __func__, (printf_ulong_t)buf_size);

    http_server_download_info_t *const p_info = p_user_data;
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
    if (HTTP_RESP_CODE_200 != http_resp_code)
    {
        LOG_ERR("Got HTTP error %d: %.*s", (printf_int_t)http_resp_code, (printf_int_t)buf_size, (const char *)p_buf);
        p_info->is_error = true;
        if (NULL != p_info->p_json_buf)
        {
            os_free(p_info->p_json_buf);
            p_info->p_json_buf = NULL;
        }
        return false;
    }

    p_info->http_resp_code = http_resp_code;
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
            if (!os_realloc_safe_and_clean((void **)&p_info->p_json_buf, p_info->json_buf_size))
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

static http_server_download_info_t
http_download_json(
    const char *const                     p_url,
    const TimeUnitsSeconds_t              timeout_seconds,
    const gw_cfg_remote_auth_type_e       auth_type,
    const ruuvi_gw_cfg_http_auth_t *const p_http_auth,
    const http_header_item_t *const       p_extra_header_item)
{
    http_server_download_info_t info = {
        .is_error       = false,
        .http_resp_code = HTTP_RESP_CODE_200,
        .p_json_buf     = NULL,
        .json_buf_size  = 0,
    };
    const TickType_t download_started_at_tick = xTaskGetTickCount();
    const bool       flag_feed_task_watchdog  = true;
    if (!http_download_with_auth(
            p_url,
            timeout_seconds,
            auth_type,
            p_http_auth,
            p_extra_header_item,
            &cb_on_http_download_json_data,
            &info,
            flag_feed_task_watchdog))
    {
        LOG_ERR("http_download failed for URL: %s", p_url);
        info.is_error = true;
    }
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
http_download_latest_release_info(void)
{
    const char *const p_url = "https://api.github.com/repos/ruuvi/ruuvi.gateway_esp.c/releases/latest";
    return http_download_json(p_url, HTTP_DOWNLOAD_TIMEOUT_SECONDS, GW_CFG_REMOTE_AUTH_TYPE_NO, NULL, NULL);
}

HTTP_SERVER_CB_STATIC
http_server_resp_t
http_server_resp_json_github_latest_release(void)
{
    const http_server_download_info_t info = http_download_latest_release_info();
    if (info.is_error)
    {
        return http_server_resp_504();
    }

    LOG_DBG("github_latest_release.json: %s", info.p_json_buf);
    const bool flag_no_cache        = true;
    const bool flag_add_header_date = true;
    return http_server_resp_data_in_heap(
        HTTP_CONENT_TYPE_APPLICATION_JSON,
        NULL,
        strlen(info.p_json_buf),
        HTTP_CONENT_ENCODING_NONE,
        (const uint8_t *)info.p_json_buf,
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
    if (0 == strcmp(p_file_name, "github_latest_release.json"))
    {
        return http_server_resp_json_github_latest_release();
    }
    if ((0 == strcmp(p_file_name, "info.json")) && (!flag_access_from_lan))
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
http_server_read_history(
    cjson_wrap_str_t *   p_json_str,
    const time_t         cur_time,
    const bool           flag_use_timestamps,
    const uint32_t       filter,
    const bool           flag_use_filter,
    num_of_advs_t *const p_num_of_advs)
{
    adv_report_table_t *p_reports = os_malloc(sizeof(*p_reports));
    if (NULL == p_reports)
    {
        return false;
    }
    adv_table_history_read(p_reports, cur_time, flag_use_timestamps, filter, flag_use_filter);
    *p_num_of_advs = p_reports->num_of_advs;

    const ruuvi_gw_cfg_coordinates_t coordinates = gw_cfg_get_coordinates();

    const bool res = http_json_create_records_str(
        p_reports,
        flag_use_timestamps,
        cur_time,
        gw_cfg_get_nrf52_mac_addr(),
        coordinates.buf,
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
    const bool flag_use_timestamps = gw_cfg_get_ntp_use();
    uint32_t   filter              = flag_use_timestamps ? HTTP_SERVER_DEFAULT_HISTORY_INTERVAL_SECONDS : 0;
    bool       flag_use_filter     = flag_use_timestamps ? true : false;
    if (NULL != p_params)
    {
        if (flag_use_timestamps)
        {
            const char *const p_time_prefix   = "time=";
            const size_t      time_prefix_len = strlen(p_time_prefix);
            if (0 == strncmp(p_params, p_time_prefix, time_prefix_len))
            {
                filter          = (uint32_t)strtoul(&p_params[time_prefix_len], NULL, 0);
                flag_use_filter = true;
            }
        }
        else
        {
            const char *const p_counter_prefix   = "counter=";
            const size_t      counter_prefix_len = strlen(p_counter_prefix);
            if (0 == strncmp(p_params, p_counter_prefix, counter_prefix_len))
            {
                filter          = (uint32_t)strtoul(&p_params[counter_prefix_len], NULL, 0);
                flag_use_filter = true;
            }
        }
    }
    cjson_wrap_str_t json_str    = cjson_wrap_str_null();
    const time_t     cur_time    = http_server_get_cur_time();
    num_of_advs_t    num_of_advs = 0;
    if (!http_server_read_history(&json_str, cur_time, flag_use_timestamps, filter, flag_use_filter, &num_of_advs))
    {
        LOG_ERR("Not enough memory");
        return http_server_resp_503();
    }

    const bool flag_no_cache        = true;
    const bool flag_add_header_date = true;
    if (flag_use_filter)
    {
        if (flag_use_timestamps)
        {
            LOG_INFO("History on %u seconds interval: %s", (unsigned)filter, json_str.p_str);
        }
        else
        {
            LOG_INFO("History starting from counter %u: %s", (unsigned)filter, json_str.p_str);
        }
    }
    else
    {
        LOG_INFO("History (without filtering): %s", json_str.p_str);
    }
    if (0 != num_of_advs)
    {
        adv_post_last_successful_network_comm_timestamp_update();
    }

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

static void
http_server_cb_on_user_req_download_latest_release_info(void)
{
    LOG_INFO("Download latest release info");
    http_server_download_info_t latest_release_info = http_download_latest_release_info();
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
        const ruuvi_esp32_fw_ver_str_t *const p_esp32_fw_ver = gw_cfg_get_esp32_fw_ver();
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
http_server_download_gw_cfg(void)
{
    const mac_address_str_t *const p_nrf52_mac_addr = gw_cfg_get_nrf52_mac_addr();

    const http_header_item_t extra_header_item = {
        .p_key   = "ruuvi_gw_mac",
        .p_value = p_nrf52_mac_addr->str_buf,
    };

    const ruuvi_gw_cfg_remote_t *p_remote = gw_cfg_get_remote_cfg_copy();
    if (NULL == p_remote)
    {
        const http_server_download_info_t download_info = {
            .is_error       = true,
            .http_resp_code = HTTP_RESP_CODE_503,
            .p_json_buf     = NULL,
            .json_buf_size  = 0,
        };
        return download_info;
    }

    size_t base_url_len = strlen(p_remote->url.buf);
    if (base_url_len < 3)
    {
        LOG_ERR("Remote cfg URL is too short: '%s'", p_remote->url.buf);
        os_free(p_remote);
        const http_server_download_info_t download_info = {
            .is_error       = true,
            .http_resp_code = HTTP_RESP_CODE_503,
            .p_json_buf     = NULL,
            .json_buf_size  = 0,
        };
        return download_info;
    }

    const TimeUnitsSeconds_t    timeout_seconds = 10;
    http_server_download_info_t download_info   = { 0 };

    const char *const p_ext = strrchr(p_remote->url.buf, '.');
    if ((NULL != p_ext) && (0 == strcmp(".json", p_ext)))
    {
        LOG_INFO("Try to download gateway configuration from the remote server: %s", p_remote->url.buf);
        download_info = http_download_json(
            p_remote->url.buf,
            timeout_seconds,
            p_remote->auth_type,
            &p_remote->auth,
            &extra_header_item);
    }
    else
    {
        ruuvi_gw_cfg_http_url_t url = { 0 };
        if ('/' == p_remote->url.buf[base_url_len - 1])
        {
            base_url_len -= 1;
        }
        (void)snprintf(
            &url.buf[0],
            sizeof(url.buf),
            "%.*s/%.2s%.2s%.2s%.2s%.2s%.2s.json",
            (printf_int_t)base_url_len,
            &p_remote->url.buf[0],
            &p_nrf52_mac_addr->str_buf[MAC_ADDR_STR_BYTE_OFFSET(0)],
            &p_nrf52_mac_addr->str_buf[MAC_ADDR_STR_BYTE_OFFSET(1)],
            &p_nrf52_mac_addr->str_buf[MAC_ADDR_STR_BYTE_OFFSET(2)],
            &p_nrf52_mac_addr->str_buf[MAC_ADDR_STR_BYTE_OFFSET(3)],
            &p_nrf52_mac_addr->str_buf[MAC_ADDR_STR_BYTE_OFFSET(4)],
            &p_nrf52_mac_addr->str_buf[MAC_ADDR_STR_BYTE_OFFSET(5)]);
        LOG_INFO("Try to download gateway configuration from the remote server: %s", url.buf);
        download_info = http_download_json(
            url.buf,
            timeout_seconds,
            p_remote->auth_type,
            &p_remote->auth,
            &extra_header_item);
        if (download_info.is_error)
        {
            LOG_WARN("Download gw_cfg: failed, http_resp_code=%u", (printf_uint_t)download_info.http_resp_code);
            (void)snprintf(
                &url.buf[0],
                sizeof(url.buf),
                "%.*s/gw_cfg.json",
                (printf_int_t)base_url_len,
                p_remote->url.buf);
            LOG_INFO("Try to download gateway configuration from the remote server: %s", url.buf);
            download_info = http_download_json(
                url.buf,
                timeout_seconds,
                p_remote->auth_type,
                &p_remote->auth,
                &extra_header_item);
        }
    }
    os_free(p_remote);
    return download_info;
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
            (void)http_server_gw_cfg_download_and_update(NULL);
            break;
        default:
            LOG_ERR("Unknown req_code=%d", (printf_int_t)req_code);
            break;
    }
}

http_server_resp_t
http_server_cb_on_get(
    const char *const               p_path,
    const char *const               p_uri_params,
    const bool                      flag_access_from_lan,
    const http_server_resp_t *const p_resp_auth)
{
    const char *p_file_ext = strrchr(p_path, '.');
    LOG_DBG(
        "http_server_cb_on_get /%s%s%s",
        p_path,
        (NULL != p_uri_params) ? "?" : "",
        (NULL != p_uri_params) ? p_uri_params : "");

    if ((NULL != p_file_ext) && (0 == strcmp(p_file_ext, ".json")))
    {
        return http_server_resp_json(p_path, flag_access_from_lan);
    }
    if (0 == strcmp(p_path, "metrics"))
    {
        return http_server_resp_metrics();
    }
    if (0 == strcmp(p_path, "history"))
    {
        return http_server_resp_history(p_uri_params);
    }
    const char *p_file_path = ('\0' == p_path[0]) ? "ruuvi.html" : p_path;
    return http_server_resp_file(p_file_path, (NULL != p_resp_auth) ? p_resp_auth->http_resp_code : HTTP_RESP_CODE_200);
}

HTTP_SERVER_CB_STATIC
http_server_resp_t
http_server_cb_on_post_ruuvi(const char *p_body)
{
    LOG_DBG("POST /ruuvi.json");
    bool      flag_network_cfg = false;
    gw_cfg_t *p_gw_cfg_tmp     = os_calloc(1, sizeof(*p_gw_cfg_tmp));
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
        const force_start_wifi_hotspot_t force_start_wifi_hotspot = settings_read_flag_force_start_wifi_hotspot();
        if (FORCE_START_WIFI_HOTSPOT_PERMANENT == force_start_wifi_hotspot)
        {
            settings_write_flag_force_start_wifi_hotspot(FORCE_START_WIFI_HOTSPOT_DISABLED);
        }
    }
    else
    {
        gw_cfg_update_ruuvi_cfg(&p_gw_cfg_tmp->ruuvi_cfg);
        restart_services();
        adv_post_enable_retransmission();
    }
    os_free(p_gw_cfg_tmp);

    return http_server_resp_data_in_flash(
        HTTP_CONENT_TYPE_APPLICATION_JSON,
        NULL,
        strlen(g_empty_json),
        HTTP_CONENT_ENCODING_NONE,
        (const uint8_t *)g_empty_json);
}

HTTP_SERVER_CB_STATIC
http_server_resp_t
http_server_cb_on_post_fw_update(const char *p_body, const bool flag_access_from_lan)
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
    return http_server_resp_data_in_flash(
        HTTP_CONENT_TYPE_APPLICATION_JSON,
        NULL,
        strlen(g_empty_json),
        HTTP_CONENT_ENCODING_NONE,
        (const uint8_t *)g_empty_json);
}

HTTP_SERVER_CB_STATIC
http_resp_code_e
http_server_gw_cfg_download_and_update(bool *const p_flag_reboot_needed)
{
    http_server_download_info_t download_info = http_server_download_gw_cfg();
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
        os_free(download_info.p_json_buf);
        return HTTP_RESP_CODE_503; // 502
    }

    gw_cfg_t *p_gw_cfg_tmp = os_calloc(1, sizeof(*p_gw_cfg_tmp));
    if (NULL == p_gw_cfg_tmp)
    {
        LOG_ERR("Failed to allocate memory for gw_cfg");
        os_free(download_info.p_json_buf);
        return HTTP_RESP_CODE_503;
    }
    gw_cfg_get_copy(p_gw_cfg_tmp);
    p_gw_cfg_tmp->ruuvi_cfg.remote.use_remote_cfg = false;

    if (!gw_cfg_json_parse(
            "gw_cfg.json",
            "Read Gateway SETTINGS from remote server:",
            download_info.p_json_buf,
            p_gw_cfg_tmp,
            NULL))
    {
        LOG_ERR("Failed to parse gw_cfg.json or no memory");
        os_free(p_gw_cfg_tmp);
        os_free(download_info.p_json_buf);
        return HTTP_RESP_CODE_503; // 502
    }
    os_free(download_info.p_json_buf);

    if (!p_gw_cfg_tmp->ruuvi_cfg.remote.use_remote_cfg)
    {
        LOG_ERR("Invalid gw_cfg.json: 'use_remote_cfg' is missing or 'false'");
        os_free(p_gw_cfg_tmp);
        return HTTP_RESP_CODE_503; // 502
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
        restart_services();
    }
    else
    {
        LOG_INFO("Gateway SETTINGS (from remote server) are the same as the current ones");
    }
    os_free(p_gw_cfg_tmp);
    return download_info.http_resp_code;
}

HTTP_SERVER_CB_STATIC
http_server_resp_t
http_server_cb_on_post_gw_cfg_download(void)
{
    LOG_DBG("POST /gw_cfg_download");
    bool flag_reboot_needed = false;

    const http_resp_code_e http_resp_code = http_server_gw_cfg_download_and_update(&flag_reboot_needed);

    const char *p_resp_content = g_empty_json;
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
        .select_location.memory.p_buf = (const uint8_t *)p_resp_content,
    };
    return resp;
}

http_server_resp_t
http_server_cb_on_post(
    const char *const p_file_name,
    const char *const p_uri_params,
    const char *const p_body,
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

http_server_resp_t
http_server_cb_on_delete(
    const char *const               p_path,
    const char *const               p_uri_params,
    const bool                      flag_access_from_lan,
    const http_server_resp_t *const p_resp_auth)
{
    (void)p_path;
    (void)p_uri_params;
    (void)flag_access_from_lan;
    (void)p_resp_auth;
    LOG_WARN("DELETE /%s", p_path);
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
