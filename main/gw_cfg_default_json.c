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
#include "gw_cfg_log.h"

#if defined(RUUVI_TESTS_GW_CFG_DEFAULT_JSON)
#define LOG_LOCAL_LEVEL LOG_LEVEL_DEBUG
#else
#define LOG_LOCAL_LEVEL LOG_LEVEL_INFO
#endif
#include "log.h"

#if (LOG_LOCAL_LEVEL >= LOG_LEVEL_DEBUG) && !RUUVI_TESTS
#warning Debug log level prints out the passwords as a "plaintext".
#endif

static const char TAG[] = "gw_cfg";

static bool
gw_cfg_default_read_from_fd(
    FILE *const     p_fd,
    char *const     p_json_cfg_buf,
    const size_t    json_cfg_buf_size,
    const char *    p_path_gw_cfg_default_json,
    gw_cfg_t *const p_gw_cfg_default)
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

    return gw_cfg_json_parse(p_path_gw_cfg_default_json, NULL, p_json_cfg_buf, p_gw_cfg_default, NULL);
}

static bool
gw_cfg_default_json_read_from_file(
    const flash_fat_fs_t *p_ffs,
    const char *          p_path_gw_cfg_default_json,
    gw_cfg_t *const       p_gw_cfg_default)
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

    const bool res = gw_cfg_default_read_from_fd(
        p_fd,
        p_json_cfg_buf,
        json_cfg_buf_size,
        p_path_gw_cfg_default_json,
        p_gw_cfg_default);

    os_free(p_json_cfg_buf);
    fclose(p_fd);

    return res;
}

bool
gw_cfg_default_json_read(gw_cfg_t *const p_gw_cfg_default)
{
    const char *                   mount_point   = "/fs_cfg";
    const flash_fat_fs_num_files_t max_num_files = 4U;

    const flash_fat_fs_t *p_ffs = flashfatfs_mount(mount_point, GW_CFG_PARTITION, max_num_files);
    if (NULL == p_ffs)
    {
        LOG_ERR("flashfatfs_mount: failed to mount partition '%s'", GW_CFG_PARTITION);
        return false;
    }

    const bool res = gw_cfg_default_json_read_from_file(p_ffs, "gw_cfg_default.json", p_gw_cfg_default);

    flashfatfs_unmount(&p_ffs);

    return res;
}