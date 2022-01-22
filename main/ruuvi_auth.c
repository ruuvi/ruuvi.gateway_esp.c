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

bool
ruuvi_auth_set_from_config(void)
{
    const ruuvi_gw_cfg_lan_auth_t lan_auth = gw_cfg_get_lan_auth();

    return http_server_set_auth(
        lan_auth.lan_auth_type,
        lan_auth.lan_auth_user,
        lan_auth.lan_auth_pass,
        lan_auth.lan_auth_api_key);
}
