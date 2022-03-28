/**
 * @file gw_cfg_default_json.c
 * @author TheSomeMan
 * @date 2022-02-28
 * @copyright Ruuvi Innovations Ltd, license BSD-3-Clause.
 */

#include "gw_cfg_default_json.h"
#include <string.h>
#include "gw_cfg_default.h"
#include "gw_cfg.h"
#include "cJSON.h"
#include "os_malloc.h"
#include "flashfatfs.h"
#include "gw_cfg_json.h"

#if defined(RUUVI_TESTS_GW_CFG_DEFAULT_JSON)
#define LOG_LOCAL_LEVEL LOG_LEVEL_DEBUG
#else
// Warning: Debug log level prints out the passwords as a "plaintext" so accidents won't happen.
#define LOG_LOCAL_LEVEL LOG_LEVEL_INFO
#endif
#include "log.h"

static const char TAG[] = "gw_cfg";

static void
gw_cfg_default_json_parse_cjson_wifi_sta_config(
    const cJSON *const       p_json_wifi_sta_cfg,
    wifi_sta_config_t *const p_wifi_sta_cfg)
{
    memset(p_wifi_sta_cfg, 0, sizeof(*p_wifi_sta_cfg));
    p_wifi_sta_cfg->ssid[0]     = '\0';
    p_wifi_sta_cfg->password[0] = '\0';

    if (!json_wrap_copy_string_val(
            p_json_wifi_sta_cfg,
            "ssid",
            (char *)p_wifi_sta_cfg->ssid,
            sizeof(p_wifi_sta_cfg->ssid)))
    {
        LOG_WARN("Can't find key '%s' in config-json", "wifi_sta_config/ssid");
    }
    if (!json_wrap_copy_string_val(
            p_json_wifi_sta_cfg,
            "password",
            (char *)p_wifi_sta_cfg->password,
            sizeof(p_wifi_sta_cfg->password)))
    {
        LOG_WARN("Can't find key '%s' in config-json", "wifi_sta_config/password");
    }
}

static void
gw_cfg_default_json_parse_cjson(const cJSON *const p_json_root, gw_cfg_default_t *const p_gw_cfg)
{
    gw_cfg_json_parse_cjson(p_json_root, &p_gw_cfg->ruuvi_gw_cfg);

    const cJSON *const p_json_wifi_sta_cfg = cJSON_GetObjectItem(p_json_root, "wifi_sta_config");
    if (NULL == p_json_wifi_sta_cfg)
    {
        LOG_WARN("Can't find key '%s' in config-json", "wifi_sta_config");
        return;
    }
    gw_cfg_default_json_parse_cjson_wifi_sta_config(p_json_wifi_sta_cfg, &p_gw_cfg->wifi_sta_config);
}

bool
gw_cfg_default_json_parse(const char *const p_json_str, gw_cfg_default_t *const p_gw_cfg)
{
    gw_cfg_default_get(&p_gw_cfg->ruuvi_gw_cfg);

    if ('\0' == p_json_str[0])
    {
        LOG_WARN("gw_cfg_default.json is empty");
        return true;
    }

    cJSON *p_json_root = cJSON_Parse(p_json_str);
    if (NULL == p_json_root)
    {
        LOG_ERR("Failed to parse gw_cfg_default.json");
        return false;
    }

    gw_cfg_default_json_parse_cjson(p_json_root, p_gw_cfg);

    cJSON_Delete(p_json_root);
    return true;
}

static bool
gw_cfg_default_read_from_fd(
    FILE *const             p_fd,
    char *                  p_json_cfg_buf,
    const size_t            json_cfg_buf_size,
    gw_cfg_default_t *const p_gw_cfg)
{
    const size_t json_cfg_len   = json_cfg_buf_size - 1;
    const size_t num_bytes_read = fread(p_json_cfg_buf, sizeof(char), json_cfg_len, p_fd);
    if (num_bytes_read != json_cfg_len)
    {
        LOG_ERR(
            "Read %u of %u bytes from default_gw_cfg.json",
            (printf_uint_t)num_bytes_read,
            (printf_uint_t)json_cfg_len);
        return false;
    }
    p_json_cfg_buf[json_cfg_len] = '\0';
    LOG_DBG("Got default config from file: %s", p_json_cfg_buf);

    return gw_cfg_default_json_parse(p_json_cfg_buf, p_gw_cfg);
}

static bool
gw_cfg_default_json_read_from_file(
    const flash_fat_fs_t *  p_ffs,
    const char *            p_path_gw_cfg_default_json,
    gw_cfg_default_t *const p_gw_cfg)
{
    const bool flag_use_binary_mode = false;

    size_t json_size = 0;
    if (!flashfatfs_get_file_size(p_ffs, p_path_gw_cfg_default_json, &json_size))
    {
        LOG_ERR("Can't get file size: %s", p_path_gw_cfg_default_json);
        return false;
    }

    FILE *p_fd = flashfatfs_fopen(p_ffs, p_path_gw_cfg_default_json, flag_use_binary_mode);
    if (NULL == p_fd)
    {
        LOG_ERR("Can't open: %s", p_path_gw_cfg_default_json);
        return false;
    }

    const size_t json_cfg_buf_size = json_size + 1;
    char *       p_json_cfg_buf    = os_malloc(json_cfg_buf_size);
    if (NULL == p_json_cfg_buf)
    {
        LOG_ERR("Can't allocate %u bytes for default_gw_cfg.json", (printf_uint_t)(json_size + 1));
        fclose(p_fd);
        return false;
    }

    const bool res = gw_cfg_default_read_from_fd(p_fd, p_json_cfg_buf, json_cfg_buf_size, p_gw_cfg);

    os_free(p_json_cfg_buf);
    fclose(p_fd);

    return res;
}

static void
gw_cfg_default_json_print_to_log_wifi_sta_config(const wifi_sta_config_t *const p_wifi_sta_config)
{
    LOG_INFO("config: wifi_sta_config: SSID: %s", p_wifi_sta_config->ssid);
#if LOG_LOCAL_LEVEL >= LOG_LEVEL_DEBUG
    LOG_DBG("config: wifi_sta_config: password: %s", p_wifi_sta_config->password);
#else
    LOG_INFO("config: wifi_sta_config: password: %s", "********");
#endif
    LOG_INFO(
        "config: wifi_sta_config: scan_method: %s",
        (WIFI_FAST_SCAN == p_wifi_sta_config->scan_method) ? "Fast" : "All");
    if (p_wifi_sta_config->bssid_set)
    {
        LOG_INFO(
            "config: wifi_sta_config: Use BSSID: %02x:%02x:%02x:%02x:%02x:%02x",
            p_wifi_sta_config->bssid[0],
            p_wifi_sta_config->bssid[1],
            p_wifi_sta_config->bssid[2],
            p_wifi_sta_config->bssid[3],
            p_wifi_sta_config->bssid[4],
            p_wifi_sta_config->bssid[5]);
    }
    else
    {
        LOG_INFO("config: wifi_sta_config: Use BSSID: %s", "false");
    }
    LOG_INFO("config: wifi_sta_config: Channel: %u", (printf_uint_t)p_wifi_sta_config->channel);
    LOG_INFO("config: wifi_sta_config: Listen interval: %u", (printf_uint_t)p_wifi_sta_config->listen_interval);
    LOG_INFO(
        "config: wifi_sta_config: Sort method: %s",
        (WIFI_CONNECT_AP_BY_SIGNAL == p_wifi_sta_config->sort_method) ? "by RSSI" : "by security mode");
    LOG_INFO("config: wifi_sta_config: Fast scan: min RSSI: %d", (printf_int_t)p_wifi_sta_config->threshold.rssi);
    LOG_INFO(
        "config: wifi_sta_config: Fast scan: weakest auth mode: %d",
        (printf_int_t)p_wifi_sta_config->threshold.authmode);
    LOG_INFO(
        "config: wifi_sta_config: Protected Management Frame: Capable: %s",
        p_wifi_sta_config->pmf_cfg.capable ? "true" : "false");
    LOG_INFO(
        "config: wifi_sta_config: Protected Management Frame: Required: %s",
        p_wifi_sta_config->pmf_cfg.required ? "true" : "false");
}

bool
gw_cfg_default_json_read(void)
{
    const char *                   mount_point   = "/fs_cfg";
    const flash_fat_fs_num_files_t max_num_files = 4U;

    const flash_fat_fs_t *p_ffs = flashfatfs_mount(mount_point, GW_CFG_PARTITION, max_num_files);
    if (NULL == p_ffs)
    {
        LOG_ERR("flashfatfs_mount: failed to mount partition '%s'", GW_CFG_PARTITION);
        return false;
    }

    gw_cfg_default_t *const p_gw_def_default = gw_cfg_default_get_ptr();
    const bool              res = gw_cfg_default_json_read_from_file(p_ffs, "gw_cfg_default.json", p_gw_def_default);

    flashfatfs_unmount(&p_ffs);

    gw_cfg_print_to_log(&p_gw_def_default->ruuvi_gw_cfg, "Gateway SETTINGS (default)");
    gw_cfg_default_json_print_to_log_wifi_sta_config(&p_gw_def_default->wifi_sta_config);

    return res;
}