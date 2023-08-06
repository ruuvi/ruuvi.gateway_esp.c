/**
 * @file gw_cfg_storage_files.c
 * @author TheSomeMan
 * @date 2023-07-24
 * @copyright Ruuvi Innovations Ltd, license BSD-3-Clause.
 */

#include "gw_cfg_storage.h"
#include <string.h>

const char* const g_gw_cfg_storage_list_of_allowed_files[GW_CFG_STORAGE_NUM_ALLOWED_FILES] = {
    GW_CFG_STORAGE_SSL_HTTP_CLI_CERT,       GW_CFG_STORAGE_SSL_HTTP_CLI_KEY,
    GW_CFG_STORAGE_SSL_STAT_CLI_CERT,       GW_CFG_STORAGE_SSL_STAT_CLI_KEY,
    GW_CFG_STORAGE_SSL_MQTT_CLI_CERT,       GW_CFG_STORAGE_SSL_MQTT_CLI_KEY,
    GW_CFG_STORAGE_SSL_REMOTE_CFG_CLI_CERT, GW_CFG_STORAGE_SSL_REMOTE_CFG_CLI_KEY,
    GW_CFG_STORAGE_SSL_HTTP_SRV_CERT,       GW_CFG_STORAGE_SSL_STAT_SRV_CERT,
    GW_CFG_STORAGE_SSL_MQTT_SRV_CERT,       GW_CFG_STORAGE_SSL_REMOTE_CFG_SRV_CERT,
};

bool
gw_cfg_storage_is_known_filename(const char* const p_file_name)
{
    bool is_known = false;
    for (int32_t i = 0; i < GW_CFG_STORAGE_NUM_ALLOWED_FILES; ++i)
    {
        if (0 == strcmp(p_file_name, g_gw_cfg_storage_list_of_allowed_files[i]))
        {
            is_known = true;
            break;
        }
    }
    return is_known;
}
