/**
 * @file mac_addr.c
 * @author TheSomeMan
 * @date 2020-10-23
 * @copyright Ruuvi Innovations Ltd, license BSD-3-Clause.
 */

#include "mac_addr.h"
#include <string.h>
#include "str_buf.h"

void
mac_address_bin_init(mac_address_bin_t *p_mac, const uint8_t mac[MAC_ADDRESS_NUM_BYTES])
{
    _Static_assert(
        MAC_ADDRESS_NUM_BYTES == sizeof(p_mac->mac),
        "Size of mac_address_bin_t must be equal to MAC_ADDRESS_NUM_BYTES");
    memcpy(p_mac->mac, mac, sizeof(p_mac->mac));
}

mac_address_str_t
mac_address_to_str(const mac_address_bin_t *p_mac)
{
    mac_address_str_t mac_str = { 0 };
    str_buf_t         str_buf = {
        .buf  = mac_str.str_buf,
        .size = sizeof(mac_str.str_buf),
        .idx  = 0,
    };
    for (unsigned i = 0; i < sizeof(p_mac->mac); ++i)
    {
        if (0 != i)
        {
            str_buf_printf(&str_buf, ":");
        }
        str_buf_printf(&str_buf, "%02X", p_mac->mac[i]);
    }
    return mac_str;
}
