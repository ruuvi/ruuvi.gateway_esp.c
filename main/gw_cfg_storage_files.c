/**
 * @file gw_cfg_storage_files.c
 * @author TheSomeMan
 * @date 2023-07-24
 * @copyright Ruuvi Innovations Ltd, license BSD-3-Clause.
 */

#include "gw_cfg_storage.h"
#include <string.h>
#include <assert.h>

#define GW_CFG_STORAGE_NUM_ALLOWED_FILES (15U)

typedef struct gw_cfg_storage_file_info
{
    const char* p_file_name;
    bool        is_blob;
} gw_cfg_storage_file_info_t;

const gw_cfg_storage_file_info_t g_gw_cfg_storage_list_of_allowed_files[GW_CFG_STORAGE_NUM_ALLOWED_FILES] = {
    {
        GW_CFG_STORAGE_SSL_HTTP_CLI_CERT,
        false,
    },
    {
        GW_CFG_STORAGE_SSL_HTTP_CLI_KEY,
        false,
    },
    {
        GW_CFG_STORAGE_SSL_STAT_CLI_CERT,
        false,
    },
    {
        GW_CFG_STORAGE_SSL_STAT_CLI_KEY,
        false,
    },
    {
        GW_CFG_STORAGE_SSL_MQTT_CLI_CERT,
        false,
    },
    {
        GW_CFG_STORAGE_SSL_MQTT_CLI_KEY,
        false,
    },
    {
        GW_CFG_STORAGE_SSL_REMOTE_CFG_CLI_CERT,
        false,
    },
    {
        GW_CFG_STORAGE_SSL_REMOTE_CFG_CLI_KEY,
        false,
    },
    {
        GW_CFG_STORAGE_SSL_HTTP_SRV_CERT,
        false,
    },
    {
        GW_CFG_STORAGE_SSL_STAT_SRV_CERT,
        false,
    },
    {
        GW_CFG_STORAGE_SSL_MQTT_SRV_CERT,
        false,
    },
    {
        GW_CFG_STORAGE_SSL_REMOTE_CFG_SRV_CERT,
        false,
    },
    {
        GW_CFG_STORAGE_HTTP_PATH,
        true,
    },
    {
        GW_CFG_STORAGE_HTTP_QUERY,
        true,
    },
    {
        GW_CFG_STORAGE_HTTP_HEADERS,
        true,
    },
};

void
gw_cfg_storage_files_iterate(gw_cfg_storage_files_iterate_cb_t cb, void* const p_user_data)
{
    for (int32_t i = 0; i < GW_CFG_STORAGE_NUM_ALLOWED_FILES; ++i)
    {
        const gw_cfg_storage_file_info_t* const p_info = &g_gw_cfg_storage_list_of_allowed_files[i];
        assert(NULL != p_info->p_file_name);
        if (cb(p_info->p_file_name, p_info->is_blob, p_user_data))
        {
            break;
        }
    }
}

typedef struct gw_cfg_storage_files_iterate_cb_is_known_ctx
{
    const char* p_file_name_to_check;
    bool        is_known;
    bool        is_blob;
} gw_cfg_storage_files_iterate_cb_is_known_ctx_t;

static bool
gw_cfg_storage_files_iterate_cb_is_known(const char* const p_file_name, const bool is_blob, void* const p_user_data)
{
    gw_cfg_storage_files_iterate_cb_is_known_ctx_t* const p_ctx = p_user_data;
    if (0 == strcmp(p_file_name, p_ctx->p_file_name_to_check))
    {
        p_ctx->is_known = true;
        p_ctx->is_blob  = is_blob;
        return true;
    }
    return false;
}

bool
gw_cfg_storage_is_known_filename(const char* const p_file_name, bool* const p_is_blob)
{
    gw_cfg_storage_files_iterate_cb_is_known_ctx_t ctx = {
        .p_file_name_to_check = p_file_name,
        .is_known             = false,
        .is_blob              = false,
    };
    gw_cfg_storage_files_iterate(&gw_cfg_storage_files_iterate_cb_is_known, &ctx);
    if ((NULL != p_is_blob) && ctx.is_known)
    {
        *p_is_blob = ctx.is_blob;
    }
    return ctx.is_known;
}
