/**
 * @file esp_opa_patched.h
 * @author TheSomeMan
 * @date 2021-05-30
 * @copyright Ruuvi Innovations Ltd, license BSD-3-Clause.
 */

#ifndef RUUVI_GATEWAY_ESP_ESP_OTA_PATCHED_H
#define RUUVI_GATEWAY_ESP_ESP_OTA_PATCHED_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "esp_err.h"
#include "esp_partition.h"
#include "esp_ota_ops.h"
#include "esp_ota_helpers.h"

#ifdef __cplusplus
extern "C" {
#endif

bool
esp_ota_is_ota_partition(const esp_partition_t* p);

esp_err_t
esp_ota_begin_patched(
    const esp_partition_t* const   p_partition,
    esp_ota_handle_t* const        p_out_handle,
    uint32_t const                 delay_ticks,
    const esp_ota_erase_callback_t callback,
    void* const                    p_user_data);

esp_err_t
esp_ota_write_patched(const esp_ota_handle_t handle, const void* const p_data, const size_t size);

esp_err_t
esp_ota_end_patched(const esp_ota_handle_t handle, esp_ota_sha256_digest_t* const p_pub_key_digest);

#ifdef __cplusplus
}
#endif

#endif // RUUVI_GATEWAY_ESP_ESP_OTA_PATCHED_H
