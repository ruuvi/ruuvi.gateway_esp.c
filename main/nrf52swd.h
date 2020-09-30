/**
 * @file nrf52swd.h
 * @author TheSomeMan
 * @date 2020-09-10
 * @copyright Ruuvi Innovations Ltd, license BSD-3-Clause.
 */

#ifndef RUUVI_GATEWAY_ESP_NRF52SWD_H
#define RUUVI_GATEWAY_ESP_NRF52SWD_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

bool
nrf52swd_init(void);

void
nrf52swd_deinit(void);

bool
nrf52swd_reset(const bool flag_reset);

bool
nrf52swd_check_id_code(void);

bool
nrf52swd_debug_halt(void);

bool
nrf52swd_debug_run(void);

bool
nrf52swd_erase_page(const uint32_t page_address);

bool
nrf52swd_erase_all(void);

bool
nrf52swd_read_mem(const uint32_t addr, const uint32_t num_words, uint32_t *p_buf);

bool
nrf52swd_write_mem(const uint32_t addr, const uint32_t num_words, const uint32_t *p_buf);

#ifdef __cplusplus
}
#endif

#endif // RUUVI_GATEWAY_ESP_NRF52SWD_H
