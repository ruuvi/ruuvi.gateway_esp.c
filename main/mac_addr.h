/**
 * @file mac_addr.h
 * @author TheSomeMan
 * @date 2020-10-23
 * @copyright Ruuvi Innovations Ltd, license BSD-3-Clause.
 */

#ifndef RUUVI_MAC_ADDR_H
#define RUUVI_MAC_ADDR_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define MAC_ADDRESS_NUM_BYTES (6)

typedef struct mac_address_bin_t
{
    uint8_t mac[MAC_ADDRESS_NUM_BYTES];
} mac_address_bin_t;

typedef struct mac_address_str_t
{
    char str_buf[(MAC_ADDRESS_NUM_BYTES * 2) + (MAC_ADDRESS_NUM_BYTES - 1) + 1]; // format: XX:XX:XX:XX:XX:XX
} mac_address_str_t;

void
mac_address_bin_init(mac_address_bin_t *p_mac, const uint8_t mac[MAC_ADDRESS_NUM_BYTES]);

mac_address_str_t
mac_address_to_str(const mac_address_bin_t *p_mac);

#ifdef __cplusplus
}
#endif

#endif // RUUVI_MAC_ADDR_H
