/**
 * @file gw_cfg_storage.h
 * @author TheSomeMan
 * @date 2023-05-06
 * @copyright Ruuvi Innovations Ltd, license BSD-3-Clause.
 */

#ifndef RUUVI_GATEWAY_ESP_GW_CFG_STORAGE_H
#define RUUVI_GATEWAY_ESP_GW_CFG_STORAGE_H

#include <stdbool.h>
#include "str_buf.h"

#define GW_CFG_STORAGE_MAX_FILE_NAME_LEN (15U)

#define GW_CFG_STORAGE_GW_CFG_DEFAULT "gw_cfg_default"
_Static_assert(
    sizeof(GW_CFG_STORAGE_GW_CFG_DEFAULT) <= (GW_CFG_STORAGE_MAX_FILE_NAME_LEN + 1),
    "sizeof(GW_CFG_STORAGE_GW_CFG_DEFAULT)");

#define GW_CFG_STORAGE_SSL_CLIENT_CERT "client_cert.pem"
_Static_assert(
    sizeof(GW_CFG_STORAGE_SSL_CLIENT_CERT) <= (GW_CFG_STORAGE_MAX_FILE_NAME_LEN + 1),
    "sizeof(GW_CFG_STORAGE_SSL_CLIENT_CERT)");

#define GW_CFG_STORAGE_SSL_CLIENT_KEY "client_key.pem"
_Static_assert(
    sizeof(GW_CFG_STORAGE_SSL_CLIENT_KEY) <= (GW_CFG_STORAGE_MAX_FILE_NAME_LEN + 1),
    "sizeof(GW_CFG_STORAGE_SSL_CLIENT_KEY)");

#define GW_CFG_STORAGE_SSL_CERT_HTTP "cert_http.pem"
_Static_assert(
    sizeof(GW_CFG_STORAGE_SSL_CERT_HTTP) <= (GW_CFG_STORAGE_MAX_FILE_NAME_LEN + 1),
    "sizeof(GW_CFG_STORAGE_SSL_CERT_HTTP)");

#define GW_CFG_STORAGE_SSL_CERT_STAT "cert_stat.pem"
_Static_assert(
    sizeof(GW_CFG_STORAGE_SSL_CERT_STAT) <= (GW_CFG_STORAGE_MAX_FILE_NAME_LEN + 1),
    "sizeof(GW_CFG_STORAGE_SSL_CERT_STAT)");

#define GW_CFG_STORAGE_SSL_CERT_MQTT "cert_mqtt.pem"
_Static_assert(
    sizeof(GW_CFG_STORAGE_SSL_CERT_MQTT) <= (GW_CFG_STORAGE_MAX_FILE_NAME_LEN + 1),
    "sizeof(GW_CFG_STORAGE_SSL_CERT_MQTT)");

#define GW_CFG_STORAGE_SSL_CERT_REMOTE "cert_remote.pem"
_Static_assert(
    sizeof(GW_CFG_STORAGE_SSL_CERT_REMOTE) <= (GW_CFG_STORAGE_MAX_FILE_NAME_LEN + 1),
    "sizeof(GW_CFG_STORAGE_SSL_CERT_REMOTE)");

#ifdef __cplusplus
extern "C" {
#endif

bool
gw_cfg_storage_check_file(const char* const p_file_name);

str_buf_t
gw_cfg_storage_read_file(const char* const p_file_name);

bool
gw_cfg_storage_write_file(const char* const p_file_name, const char* const p_content);

bool
gw_cfg_storage_delete_file(const char* const p_file_name);

void
gw_cfg_storage_deinit_erase_init(void);

#ifdef __cplusplus
}
#endif

#endif // RUUVI_GATEWAY_ESP_GW_CFG_STORAGE_H
