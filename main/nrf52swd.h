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

#if !defined(RUUVI_TESTS_NRF52SWD)
#define RUUVI_TESTS_NRF52SWD (0)
#endif

#if RUUVI_TESTS_NRF52SWD
#define NRF52SWD_STATIC
#else
#define NRF52SWD_STATIC static
#endif

#ifdef __cplusplus
extern "C" {
#endif

#define NRF52SWD_SHA256_MAX_SEGMENTS (8U)

#define NRF52SWD_SHA256_DIGEST_SIZE_BYTES (32U)

typedef struct nrf52swd_sha256_t
{
    uint8_t digest[NRF52SWD_SHA256_DIGEST_SIZE_BYTES];
} nrf52swd_sha256_t;

typedef struct nrf52swd_segment_t
{
    uint32_t start_addr;
    uint32_t size_bytes;
} nrf52swd_segment_t;

bool
nrf52swd_init_gpio_cfg_nreset(void);

bool
nrf52swd_deinit_gpio_cfg_nreset(void);

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
nrf52swd_debug_enable_reset_vector_catch(void);

bool
nrf52swd_debug_reset(void);

bool
nrf52swd_debug_run(void);

bool
nrf52swd_erase_all(void);

bool
nrf52swd_read_mem(const uint32_t addr, const uint32_t num_words, uint32_t* p_buf);

bool
nrf52swd_write_mem(const uint32_t addr, const uint32_t num_words, const uint32_t* p_buf);

bool
nrf52swd_calc_sha256_digest_on_nrf52(
    const nrf52swd_segment_t* p_segments,
    const uint32_t            num_segments,
    nrf52swd_sha256_t* const  p_sha256);

#if RUUVI_TESTS_NRF52SWD

NRF52SWD_STATIC
bool
nrf52swd_init_gpio_cfg_nreset(void);

NRF52SWD_STATIC
bool
nrf52swd_init_spi_init(void);

NRF52SWD_STATIC
bool
nrf52swd_init_spi_add_device(void);

NRF52SWD_STATIC
bool
nrf52swd_read_reg(const uint32_t reg_addr, uint32_t* p_val);

NRF52SWD_STATIC
bool
nrf52swd_write_reg(const uint32_t reg_addr, const uint32_t val);

NRF52SWD_STATIC
bool
nrf51swd_nvmc_is_ready_or_err(bool* p_result);

NRF52SWD_STATIC
bool
nrf51swd_nvmc_wait_while_busy(void);

#endif

#ifdef __cplusplus
}
#endif

#endif // RUUVI_GATEWAY_ESP_NRF52SWD_H
