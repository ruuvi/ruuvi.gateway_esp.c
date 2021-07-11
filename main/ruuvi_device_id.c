/**
 * @file ruuvi_device_id.c
 * @author TheSomeMan
 * @date 2021-07-08
 * @copyright Ruuvi Innovations Ltd, license BSD-3-Clause.
 */

#include "ruuvi_device_id.h"
#include <string.h>
#include "str_buf.h"

static nrf52_device_id_t g_nrf52_device_id = { 0 };
static mac_address_bin_t g_nrf52_mac_addr  = { 0 };
static volatile bool     g_ruuvi_device_id_flag_nrf52_id_received;

void
ruuvi_device_id_init(void)
{
    memset(&g_nrf52_device_id, 0, sizeof(g_nrf52_device_id));
    memset(&g_nrf52_mac_addr, 0, sizeof(g_nrf52_mac_addr));
    g_ruuvi_device_id_flag_nrf52_id_received = false;
}

void
ruuvi_device_id_deinit(void)
{
    g_ruuvi_device_id_flag_nrf52_id_received = false;
    memset(&g_nrf52_device_id, 0, sizeof(g_nrf52_device_id));
    memset(&g_nrf52_mac_addr, 0, sizeof(g_nrf52_mac_addr));
}

mac_address_bin_t
ruuvi_device_id_get_nrf52_mac_address(void)
{
    return g_nrf52_mac_addr;
}

mac_address_str_t
ruuvi_device_id_get_nrf52_mac_address_str(void)
{
    return mac_address_to_str(&g_nrf52_mac_addr);
}

nrf52_device_id_t
ruuvi_device_id_get(void)
{
    return g_nrf52_device_id;
}

static nrf52_device_id_str_t
nrf52_device_id_to_str(const nrf52_device_id_t *const p_dev_id)
{
    nrf52_device_id_str_t device_id_str = { 0 };
    str_buf_t             str_buf       = {
        .buf  = device_id_str.str_buf,
        .size = sizeof(device_id_str.str_buf),
        .idx  = 0,
    };
    for (size_t i = 0; i < sizeof(p_dev_id->id); ++i)
    {
        if (0 != i)
        {
            str_buf_printf(&str_buf, ":");
        }
        str_buf_printf(&str_buf, "%02X", p_dev_id->id[i]);
    }
    return device_id_str;
}

nrf52_device_id_str_t
ruuvi_device_id_get_str(void)
{
    const nrf52_device_id_t device_id = ruuvi_device_id_get();
    return nrf52_device_id_to_str(&device_id);
}

void
ruuvi_device_id_set(const nrf52_device_id_t *const p_nrf52_device_id, const mac_address_bin_t *const p_nrf52_mac_addr)
{
    g_nrf52_device_id                        = *p_nrf52_device_id;
    g_nrf52_mac_addr                         = *p_nrf52_mac_addr;
    g_ruuvi_device_id_flag_nrf52_id_received = true;
}

bool
ruuvi_device_id_is_set(void)
{
    return g_ruuvi_device_id_flag_nrf52_id_received;
}
