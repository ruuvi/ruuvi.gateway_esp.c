/**
 * @file ruuvi_auth.c
 * @author TheSomeMan
 * @date 2021-09-14
 * @copyright Ruuvi Innovations Ltd, license BSD-3-Clause.
 */

#include "ruuvi_auth.h"
#include <string.h>
#include "wifiman_md5.h"
#include "str_buf.h"
#include "ruuvi_device_id.h"
#include "http_server_auth_type.h"
#include "gw_cfg.h"
#include "gw_cfg_default.h"

#define LOG_LOCAL_LEVEL LOG_LEVEL_INFO
#include "log.h"

static const char TAG[] = "gw_cfg";

bool
ruuvi_auth_set_from_config(void)
{
    if ((0 == strcmp(HTTP_SERVER_AUTH_TYPE_STR_RUUVI, g_gateway_config.lan_auth.lan_auth_type))
        && (0 == strcmp(RUUVI_GATEWAY_AUTH_DEFAULT_USER, g_gateway_config.lan_auth.lan_auth_user))
        && (0 == strcmp(RUUVI_GATEWAY_AUTH_DEFAULT_PASS_USE_DEVICE_ID, g_gateway_config.lan_auth.lan_auth_pass)))
    {
        const nrf52_device_id_str_t device_id = ruuvi_device_id_get_str();

        str_buf_t str_buf = str_buf_printf_with_alloc(
            "%s:%s:%s",
            RUUVI_GATEWAY_AUTH_DEFAULT_USER,
            g_gw_wifi_ssid.ssid_buf,
            device_id.str_buf);

        const wifiman_md5_digest_hex_str_t password_md5 = wifiman_md5_calc_hex_str(
            str_buf.buf,
            str_buf_get_len(&str_buf));

        str_buf_free_buf(&str_buf);

        LOG_INFO(
            "Set default LAN auth: user=%s, password=%s, md5=%s",
            RUUVI_GATEWAY_AUTH_DEFAULT_USER,
            device_id.str_buf,
            password_md5.buf);

        return http_server_set_auth(HTTP_SERVER_AUTH_TYPE_STR_RUUVI, RUUVI_GATEWAY_AUTH_DEFAULT_USER, password_md5.buf);
    }
    else
    {
        return http_server_set_auth(
            g_gateway_config.lan_auth.lan_auth_type,
            g_gateway_config.lan_auth.lan_auth_user,
            g_gateway_config.lan_auth.lan_auth_pass);
    }
}
