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

/**
 * @brief Read the OTA state (e.g. NEW, PENDING_VERIFY, VALID, INVALID, ABORTED)
 *        recorded in the @c otadata partition for a given OTA app partition.
 *
 * This is a patched version of @c esp_ota_get_state_partition() from ESP-IDF.
 * Compared to the upstream implementation it:
 *  - distinguishes "slot has no @c otadata entry" from "slot has an entry but
 *    its CRC is invalid";
 *  - if both @c otadata sectors map to the requested slot, prefers the entry
 *    with a valid CRC over the one with a broken CRC;
 *  - emits diagnostic logs (number of OTA app partitions, decoded slot for
 *    each @c otadata entry, requested slot, mismatching CRC values) to make
 *    field debugging of OTA state issues easier.
 *
 * The function performs the following steps:
 *  1. Validates the input arguments and verifies that @p p_partition refers
 *     to an OTA application partition
 *     (@c ESP_PARTITION_SUBTYPE_APP_OTA_0 .. @c ESP_PARTITION_SUBTYPE_APP_OTA_MAX-1).
 *  2. Counts the number of OTA app partitions defined in the partition table.
 *  3. Memory-maps the @c otadata partition and copies its two
 *     @c esp_ota_select_entry_t records into a local buffer.
 *  4. For each of the two records, computes the OTA slot it refers to as
 *     <tt>(ota_seq - 1) % ota_app_count</tt>. Records with @c ota_seq equal
 *     to 0 or @c UINT32_MAX (uninitialised flash) are treated as "no slot".
 *  5. If a record maps to the requested slot, writes its @c ota_state to
 *     @p p_ota_state (so that the caller can log it even on CRC errors) and
 *     validates the CRC stored in the record against the value recomputed
 *     by @c bootloader_common_ota_select_crc(). On CRC mismatch the loop
 *     continues so that a second, valid entry mapping to the same slot can
 *     still be returned successfully.
 *
 * @param[in]  p_partition  Pointer to the OTA app partition whose state is
 *                          being queried. Must not be NULL.
 * @param[out] p_ota_state  Pointer that receives the OTA state on success.
 *                          Must not be NULL. On @c ESP_ERR_INVALID_CRC the
 *                          value of the (corrupt) @c otadata entry is still
 *                          written, so the caller can log it. On any other
 *                          error the contents are unspecified.
 *
 * @return
 *   - @c ESP_OK                  The OTA state was read successfully and is
 *                                stored in @p *p_ota_state.
 *   - @c ESP_ERR_INVALID_ARG     @p p_partition or @p p_ota_state is NULL.
 *   - @c ESP_ERR_NOT_SUPPORTED   @p p_partition is not an OTA app partition.
 *   - @c ESP_ERR_NOT_FOUND       No OTA app partitions are defined, the
 *                                @c otadata partition could not be read, or
 *                                no @c otadata entry maps to the requested
 *                                slot.
 *   - @c ESP_ERR_INVALID_CRC     An @c otadata entry maps to the requested
 *                                slot but its CRC is invalid (and there is
 *                                no other entry for the same slot with a
 *                                valid CRC).
 */
esp_err_t
esp_ota_get_state_partition_patched(const esp_partition_t* const p_partition, esp_ota_img_states_t* const p_ota_state);

esp_err_t
esp_ota_set_boot_partition_patched(const esp_partition_t* partition);

#ifdef __cplusplus
}
#endif

#endif // RUUVI_GATEWAY_ESP_ESP_OTA_PATCHED_H
