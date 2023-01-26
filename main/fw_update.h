/**
 * @file fw_update.h
 * @author TheSomeMan
 * @date 2021-05-30
 * @copyright Ruuvi Innovations Ltd, license BSD-3-Clause.
 */

#ifndef RUUVI_GATEWAY_ESP_FW_UPDATE_H
#define RUUVI_GATEWAY_ESP_FW_UPDATE_H

#include <stdbool.h>
#include "esp_ota_ops.h"
#include "attribs.h"
#include "fw_ver.h"

#ifdef __cplusplus
extern "C" {
#endif

#define FW_UPDATE_URL_MAX_LEN               (128U)
#define FW_UPDATE_URL_WITH_FW_IMAGE_MAX_LEN (FW_UPDATE_URL_MAX_LEN + 1 + 32)

typedef struct ruuvi_flash_info_t
{
    bool                   is_valid;
    bool                   is_ota0_active;
    const esp_app_desc_t*  p_app_desc;
    const esp_partition_t* p_boot_partition;
    const esp_partition_t* p_running_partition;
    esp_ota_img_states_t   running_partition_state;
    const esp_partition_t* p_next_update_partition;
    const esp_partition_t* p_next_fatfs_gwui_partition;
    const esp_partition_t* p_next_fatfs_nrf52_partition;
} ruuvi_flash_info_t;

typedef enum fw_update_reason_e
{
    FW_UPDATE_REASON_NONE = 0,
    FW_UPDATE_REASON_AUTO,
    FW_UPDATE_REASON_MANUAL_VIA_HOTSPOT,
    FW_UPDATE_REASON_MANUAL_VIA_LAN,
} fw_updating_reason_e;

bool
fw_update_read_flash_info(void);

bool
fw_update_mark_app_valid_cancel_rollback(void);

const char*
fw_update_get_current_fatfs_nrf52_partition_name(void);

const char*
fw_update_get_current_fatfs_gwui_partition_name(void);

ruuvi_esp32_fw_ver_str_t
fw_update_get_cur_version(void);

bool
json_fw_update_parse_http_body(const char* const p_body);

bool
fw_update_is_url_valid(void);

ATTR_PRINTF(1, 2)
void
fw_update_set_url(const char* const p_url_fmt, ...);

const char*
fw_update_get_url(void);

bool
fw_update_run(const fw_updating_reason_e fw_updating_reason);

void
fw_update_set_extra_info_for_status_json_update_reset(void);

void
fw_update_set_extra_info_for_status_json_update_start(void);

void
fw_update_set_extra_info_for_status_json_update_successful(void);

void
fw_update_set_extra_info_for_status_json_update_failed(const char* const p_message);

void
fw_update_set_extra_info_for_status_json_update_failed_nrf52(const char* const p_message);

void
fw_update_set_stage_nrf52_updating(void);

void
fw_update_nrf52fw_cb_progress(const size_t num_bytes_flashed, const size_t total_size, void* const p_param);

#ifdef __cplusplus
}
#endif

#endif // RUUVI_GATEWAY_ESP_FW_UPDATE_H
