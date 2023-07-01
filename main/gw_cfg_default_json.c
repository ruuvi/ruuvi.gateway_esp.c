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
#include "gw_cfg_json_parse.h"
#include "gw_cfg_log.h"
#include "ruuvi_nvs.h"

#if defined(RUUVI_TESTS_GW_CFG_DEFAULT_JSON)
#define LOG_LOCAL_LEVEL LOG_LEVEL_DEBUG
#else
#define LOG_LOCAL_LEVEL LOG_LEVEL_INFO
#endif
#include "log.h"
#include "gw_cfg_storage.h"

#if (LOG_LOCAL_LEVEL >= LOG_LEVEL_DEBUG) && !RUUVI_TESTS
#warning Debug log level prints out the passwords as a "plaintext".
#endif

static const char TAG[] = "gw_cfg";

bool
gw_cfg_default_json_read(gw_cfg_t* const p_gw_cfg_default)
{
    LOG_INFO("Read default gw_cfg from NVS by key '%s'", GW_CFG_STORAGE_GW_CFG_DEFAULT);
    str_buf_t str_buf_gw_cfg_def = gw_cfg_storage_read_file(GW_CFG_STORAGE_GW_CFG_DEFAULT);

    if (NULL == str_buf_gw_cfg_def.buf)
    {
        LOG_ERR("Failed to read default gw_cfg_def from NVS");
        return false;
    }
    LOG_INFO("Default gw_cfg was successfully read from NVS");
    LOG_DBG("Default gw_cfg: %s", str_buf_gw_cfg_def.buf);

    const bool res = gw_cfg_json_parse(GW_CFG_STORAGE_GW_CFG_DEFAULT, NULL, str_buf_gw_cfg_def.buf, p_gw_cfg_default);
    str_buf_free_buf(&str_buf_gw_cfg_def);

    if (!res)
    {
        LOG_ERR("Failed to parse default gw_cfg from NVS");
        return false;
    }

    return true;
}
