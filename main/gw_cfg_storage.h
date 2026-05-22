/**
 * @file gw_cfg_storage.h
 * @author TheSomeMan
 * @date 2023-05-06
 * @copyright Ruuvi Innovations Ltd, license BSD-3-Clause.
 */

#ifndef RUUVI_GATEWAY_ESP_GW_CFG_STORAGE_H
#define RUUVI_GATEWAY_ESP_GW_CFG_STORAGE_H

#include <stdbool.h>
#include <stddef.h>
#include "str_buf.h"

#ifdef __cplusplus
extern "C" {
#endif

#define GW_CFG_STORAGE_MAX_FILE_NAME_LEN (15U)

#define GW_CFG_STORAGE_GW_CFG_DEFAULT "gw_cfg_default"
#ifndef __cplusplus
_Static_assert(
    sizeof(GW_CFG_STORAGE_GW_CFG_DEFAULT) <= (GW_CFG_STORAGE_MAX_FILE_NAME_LEN + 1),
    "sizeof(GW_CFG_STORAGE_GW_CFG_DEFAULT)");
#endif

#define GW_CFG_STORAGE_SSL_HTTP_CLI_CERT "http_cli_cert"
#ifndef __cplusplus
_Static_assert(
    sizeof(GW_CFG_STORAGE_SSL_HTTP_CLI_CERT) <= (GW_CFG_STORAGE_MAX_FILE_NAME_LEN + 1),
    "sizeof(GW_CFG_STORAGE_SSL_HTTP_CLI_CERT)");
#endif

#define GW_CFG_STORAGE_SSL_HTTP_CLI_KEY "http_cli_key"
#ifndef __cplusplus
_Static_assert(
    sizeof(GW_CFG_STORAGE_SSL_HTTP_CLI_KEY) <= (GW_CFG_STORAGE_MAX_FILE_NAME_LEN + 1),
    "sizeof(GW_CFG_STORAGE_SSL_HTTP_CLI_KEY)");
#endif

#define GW_CFG_STORAGE_SSL_STAT_CLI_CERT "stat_cli_cert"
#ifndef __cplusplus
_Static_assert(
    sizeof(GW_CFG_STORAGE_SSL_STAT_CLI_CERT) <= (GW_CFG_STORAGE_MAX_FILE_NAME_LEN + 1),
    "sizeof(GW_CFG_STORAGE_SSL_STAT_CLI_CERT)");
#endif

#define GW_CFG_STORAGE_SSL_STAT_CLI_KEY "stat_cli_key"
#ifndef __cplusplus
_Static_assert(
    sizeof(GW_CFG_STORAGE_SSL_STAT_CLI_KEY) <= (GW_CFG_STORAGE_MAX_FILE_NAME_LEN + 1),
    "sizeof(GW_CFG_STORAGE_SSL_STAT_CLI_KEY)");
#endif

#define GW_CFG_STORAGE_SSL_MQTT_CLI_CERT "mqtt_cli_cert"
#ifndef __cplusplus
_Static_assert(
    sizeof(GW_CFG_STORAGE_SSL_MQTT_CLI_CERT) <= (GW_CFG_STORAGE_MAX_FILE_NAME_LEN + 1),
    "sizeof(GW_CFG_STORAGE_SSL_MQTT_CLI_CERT)");
#endif

#define GW_CFG_STORAGE_SSL_MQTT_CLI_KEY "mqtt_cli_key"
#ifndef __cplusplus
_Static_assert(
    sizeof(GW_CFG_STORAGE_SSL_MQTT_CLI_KEY) <= (GW_CFG_STORAGE_MAX_FILE_NAME_LEN + 1),
    "sizeof(GW_CFG_STORAGE_SSL_MQTT_CLI_KEY)");
#endif

#define GW_CFG_STORAGE_SSL_REMOTE_CFG_CLI_CERT "rcfg_cli_cert"
#ifndef __cplusplus
_Static_assert(
    sizeof(GW_CFG_STORAGE_SSL_REMOTE_CFG_CLI_CERT) <= (GW_CFG_STORAGE_MAX_FILE_NAME_LEN + 1),
    "sizeof(GW_CFG_STORAGE_SSL_REMOTE_CFG_CLI_CERT)");
#endif

#define GW_CFG_STORAGE_SSL_REMOTE_CFG_CLI_KEY "rcfg_cli_key"
#ifndef __cplusplus
_Static_assert(
    sizeof(GW_CFG_STORAGE_SSL_REMOTE_CFG_CLI_KEY) <= (GW_CFG_STORAGE_MAX_FILE_NAME_LEN + 1),
    "sizeof(GW_CFG_STORAGE_SSL_REMOTE_CFG_CLI_KEY)");
#endif

#define GW_CFG_STORAGE_SSL_HTTP_SRV_CERT "http_srv_cert"
#ifndef __cplusplus
_Static_assert(
    sizeof(GW_CFG_STORAGE_SSL_HTTP_SRV_CERT) <= (GW_CFG_STORAGE_MAX_FILE_NAME_LEN + 1),
    "sizeof(GW_CFG_STORAGE_SSL_HTTP_SRV_CERT)");
#endif

#define GW_CFG_STORAGE_SSL_STAT_SRV_CERT "stat_srv_cert"
#ifndef __cplusplus
_Static_assert(
    sizeof(GW_CFG_STORAGE_SSL_STAT_SRV_CERT) <= (GW_CFG_STORAGE_MAX_FILE_NAME_LEN + 1),
    "sizeof(GW_CFG_STORAGE_SSL_STAT_SRV_CERT)");
#endif

#define GW_CFG_STORAGE_SSL_MQTT_SRV_CERT "mqtt_srv_cert"
#ifndef __cplusplus
_Static_assert(
    sizeof(GW_CFG_STORAGE_SSL_MQTT_SRV_CERT) <= (GW_CFG_STORAGE_MAX_FILE_NAME_LEN + 1),
    "sizeof(GW_CFG_STORAGE_SSL_MQTT_SRV_CERT)");
#endif

#define GW_CFG_STORAGE_SSL_REMOTE_CFG_SRV_CERT "rcfg_srv_cert"
#ifndef __cplusplus
_Static_assert(
    sizeof(GW_CFG_STORAGE_SSL_REMOTE_CFG_SRV_CERT) <= (GW_CFG_STORAGE_MAX_FILE_NAME_LEN + 1),
    "sizeof(GW_CFG_STORAGE_SSL_REMOTE_CFG_SRV_CERT)");
#endif

#define GW_CFG_STORAGE_HTTP_PATH "http_path"
#ifndef __cplusplus
_Static_assert(
    sizeof(GW_CFG_STORAGE_HTTP_PATH) <= (GW_CFG_STORAGE_MAX_FILE_NAME_LEN + 1),
    "sizeof(GW_CFG_STORAGE_HTTP_PATH)");
#endif

#define GW_CFG_STORAGE_HTTP_QUERY "http_query"
#ifndef __cplusplus
_Static_assert(
    sizeof(GW_CFG_STORAGE_HTTP_QUERY) <= (GW_CFG_STORAGE_MAX_FILE_NAME_LEN + 1),
    "sizeof(GW_CFG_STORAGE_HTTP_QUERY)");
#endif

#define GW_CFG_STORAGE_HTTP_HEADERS "http_headers"
#ifndef __cplusplus
_Static_assert(
    sizeof(GW_CFG_STORAGE_HTTP_HEADERS) <= (GW_CFG_STORAGE_MAX_FILE_NAME_LEN + 1),
    "sizeof(GW_CFG_STORAGE_HTTP_HEADERS)");
#endif

typedef bool (
    *gw_cfg_storage_files_iterate_cb_t)(const char* const p_file_name, const bool is_blob, void* const p_user_data);

void
gw_cfg_storage_files_iterate(gw_cfg_storage_files_iterate_cb_t cb, void* const p_user_data);

bool
gw_cfg_storage_check(void);

bool
gw_cfg_storage_is_known_filename(const char* const p_file_name, bool* const p_is_blob);

bool
gw_cfg_storage_check_file(const char* const p_file_name, const bool is_blob, size_t* const p_file_size);

str_buf_t
gw_cfg_storage_read_file_as_string(const char* const p_file_name);

str_buf_t
gw_cfg_storage_read_file_as_blob(const char* const p_file_name);

bool
gw_cfg_storage_write_file_as_string(const char* const p_file_name, const char* const p_content);

bool
gw_cfg_storage_write_file_as_blob(const char* const p_file_name, const uint8_t* const p_content, const size_t len);

bool
gw_cfg_storage_delete_file(const char* const p_file_name);

void
gw_cfg_storage_deinit_erase_init(void);

str_buf_t
gw_cfg_storage_read_string_from_nvs(void* const hndl, const char* const p_nvs_key);

str_buf_t
gw_cfg_storage_read_blob_from_nvs(void* const hndl, const char* const p_nvs_key);

#ifdef __cplusplus
}
#endif

#endif // RUUVI_GATEWAY_ESP_GW_CFG_STORAGE_H
