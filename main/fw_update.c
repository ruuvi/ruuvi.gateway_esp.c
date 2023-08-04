/**
 * @file fw_update.c
 * @author TheSomeMan
 * @date 2021-05-30
 * @copyright Ruuvi Innovations Ltd, license BSD-3-Clause.
 */

#include "fw_update.h"
#include <string.h>
#include "http_download.h"
#include "cJSON.h"
#include "cjson_wrap.h"
#include "wifi_manager.h"
#include "esp_ota_ops_patched.h"
#include "nrf52fw.h"
#include "reset_task.h"
#include "settings.h"
#include "ruuvi_gateway.h"
#include "leds.h"
#include "gw_status.h"
#include "os_malloc.h"

#define LOG_LOCAL_LEVEL LOG_LEVEL_INFO
#include "log.h"

#define FW_UPDATE_TASK_STACK_SIZE ((7 * 1024) - 512)
#define FW_UPDATE_TASK_PRIORITY   (1)

#define GW_GWUI_PARTITION_2 GW_GWUI_PARTITION "_2"
#define GW_NRF_PARTITION_2  GW_NRF_PARTITION "_2"

#define FW_UPDATE_DELAY_AFTER_OPERATION_WITH_FLASH_MS (20)

#define FW_UPDATE_PERCENTAGE_50  (50U)
#define FW_UPDATE_PERCENTAGE_100 (100U)

#define FW_UPDATE_DELAY_BEFORE_REBOOT_SECONDS (5U)

typedef struct fw_update_config_t
{
    char url[FW_UPDATE_URL_MAX_LEN + 1];
} fw_update_config_t;

typedef struct fw_update_data_partition_info_t
{
    const esp_partition_t* const p_partition;
    size_t                       offset;
    bool                         is_error;
} fw_update_data_partition_info_t;

typedef struct fw_update_ota_partition_info_t
{
    const esp_partition_t* const p_partition;
    esp_ota_handle_t             out_handle;
    bool                         is_error;
} fw_update_ota_partition_info_t;

typedef enum fw_update_stage_e
{
    FW_UPDATE_STAGE_NONE         = 0,
    FW_UPDATE_STAGE_1            = 1, // start firmware updating process and download ruuvi_gateway_esp.bin
    FW_UPDATE_STAGE_2            = 2, // download fatfs_gwui.bin
    FW_UPDATE_STAGE_3            = 3, // download fatfs_nrf52.bin
    FW_UPDATE_STAGE_4            = 4, // flashing nRF52
    FW_UPDATE_STAGE_SUCCESSFUL   = 5,
    FW_UPDATE_STAGE_FAILED       = 6,
    FW_UPDATE_STAGE_FAILED_NRF52 = 7,
} fw_update_stage_e;

typedef uint32_t fw_update_percentage_t;

static void
fw_update_set_extra_info_for_status_json(
    const enum fw_update_stage_e fw_updating_stage,
    const fw_update_percentage_t fw_updating_percentage);

static const char TAG[] = "fw_update";

static ruuvi_flash_info_t   g_ruuvi_flash_info;
static fw_update_config_t   g_fw_update_cfg;
static fw_update_stage_e    g_update_progress_stage;
static fw_updating_reason_e g_fw_updating_reason;

static const esp_partition_t*
find_data_fat_partition_by_name(const char* const p_partition_name)
{
    esp_partition_iterator_t iter = esp_partition_find(
        ESP_PARTITION_TYPE_DATA,
        ESP_PARTITION_SUBTYPE_DATA_FAT,
        p_partition_name);
    if (NULL == iter)
    {
        LOG_ERR("Can't find partition: %s", p_partition_name);
        return NULL;
    }
    const esp_partition_t* p_partition = esp_partition_get(iter);
    esp_partition_iterator_release(iter);
    return p_partition;
}

static bool
fw_update_read_flash_info_internal(ruuvi_flash_info_t* const p_flash_info)
{
    p_flash_info->is_valid = false;

    p_flash_info->running_partition_state = ESP_OTA_IMG_UNDEFINED;

    p_flash_info->p_app_desc = esp_ota_get_app_description();
    LOG_INFO("### Project name     : %s", p_flash_info->p_app_desc->project_name);
    LOG_INFO("### Firmware version : %s", p_flash_info->p_app_desc->version);

    p_flash_info->p_boot_partition = esp_ota_get_boot_partition();
    if (NULL == p_flash_info->p_boot_partition)
    {
        LOG_ERR("There is no boot partition info");
        return false;
    }
    LOG_INFO("### Boot partition: %s", p_flash_info->p_boot_partition->label);

    p_flash_info->p_running_partition = esp_ota_get_running_partition();
    if (NULL == p_flash_info->p_running_partition)
    {
        LOG_ERR("There is no running partition info");
        return false;
    }
    LOG_INFO("Currently running partition: %s", p_flash_info->p_running_partition->label);

    esp_err_t err = esp_ota_get_state_partition(
        p_flash_info->p_running_partition,
        &p_flash_info->running_partition_state);
    if (ESP_OK != err)
    {
        LOG_ERR_ESP(err, "%s failed", "esp_ota_get_state_partition");
        return false;
    }
    switch (p_flash_info->running_partition_state)
    {
        case ESP_OTA_IMG_NEW:
            LOG_INFO("Currently running partition state: %s", "NEW");
            break;
        case ESP_OTA_IMG_PENDING_VERIFY:
            LOG_INFO("Currently running partition state: %s", "PENDING_VERIFY");
            break;
        case ESP_OTA_IMG_VALID:
            LOG_INFO("Currently running partition state: %s", "VALID");
            break;
        case ESP_OTA_IMG_INVALID:
            LOG_INFO("Currently running partition state: %s", "INVALID");
            break;
        case ESP_OTA_IMG_ABORTED:
            LOG_INFO("Currently running partition state: %s", "ABORTED");
            break;
        case ESP_OTA_IMG_UNDEFINED:
            LOG_INFO("Currently running partition state: %s", "UNDEFINED");
            break;
        default:
            LOG_INFO(
                "Currently running partition state: UNKNOWN (%d)",
                (printf_int_t)p_flash_info->running_partition_state);
            break;
    }

    p_flash_info->p_next_update_partition = esp_ota_get_next_update_partition(p_flash_info->p_running_partition);
    if (NULL == p_flash_info->p_next_update_partition)
    {
        LOG_ERR("Can't find next partition for updating");
        return false;
    }
    LOG_INFO(
        "Next update partition: %s: address 0x%08x, size 0x%x",
        p_flash_info->p_next_update_partition->label,
        p_flash_info->p_next_update_partition->address,
        p_flash_info->p_next_update_partition->size);
    p_flash_info->is_ota0_active = (0 == strcmp("ota_0", p_flash_info->p_running_partition->label)) ? true : false;
    const char* const p_gwui_parition_name    = p_flash_info->is_ota0_active ? GW_GWUI_PARTITION_2 : GW_GWUI_PARTITION;
    p_flash_info->p_next_fatfs_gwui_partition = find_data_fat_partition_by_name(p_gwui_parition_name);
    if (NULL == p_flash_info->p_next_fatfs_gwui_partition)
    {
        return false;
    }
    LOG_INFO(
        "Next fatfs_gwui partition: %s: address 0x%08x, size 0x%x",
        p_flash_info->p_next_fatfs_gwui_partition->label,
        p_flash_info->p_next_fatfs_gwui_partition->address,
        p_flash_info->p_next_fatfs_gwui_partition->size);

    const char* const p_fatfs_nrf52_partition_name = p_flash_info->is_ota0_active ? GW_NRF_PARTITION_2
                                                                                  : GW_NRF_PARTITION;
    p_flash_info->p_next_fatfs_nrf52_partition     = find_data_fat_partition_by_name(p_fatfs_nrf52_partition_name);
    if (NULL == p_flash_info->p_next_fatfs_nrf52_partition)
    {
        return false;
    }
    LOG_INFO(
        "Next fatfs_nrf52 partition: %s: address 0x%08x, size 0x%x",
        p_flash_info->p_next_fatfs_nrf52_partition->label,
        p_flash_info->p_next_fatfs_nrf52_partition->address,
        p_flash_info->p_next_fatfs_nrf52_partition->size);

    p_flash_info->is_valid = true;
    return true;
}

bool
fw_update_read_flash_info(void)
{
    ruuvi_flash_info_t* p_flash_info = &g_ruuvi_flash_info;
    fw_update_read_flash_info_internal(p_flash_info);
    return p_flash_info->is_valid;
}

bool
fw_update_mark_app_valid_cancel_rollback(void)
{
    ruuvi_flash_info_t* p_flash_info = &g_ruuvi_flash_info;
    if (ESP_OTA_IMG_PENDING_VERIFY == p_flash_info->running_partition_state)
    {
        LOG_INFO("Mark current OTA partition valid and cancel rollback");
        const esp_err_t err = esp_ota_mark_app_valid_cancel_rollback();
        if (ESP_OK != err)
        {
            LOG_ERR_ESP(err, "%s failed", "esp_ota_mark_app_valid_cancel_rollback");
            return false;
        }
        p_flash_info->running_partition_state = ESP_OTA_IMG_VALID;
    }
    return true;
}

const char*
fw_update_get_current_fatfs_nrf52_partition_name(void)
{
    const ruuvi_flash_info_t* const p_flash_info = &g_ruuvi_flash_info;
    return p_flash_info->is_ota0_active ? GW_NRF_PARTITION : GW_NRF_PARTITION_2;
}

const char*
fw_update_get_current_fatfs_gwui_partition_name(void)
{
    const ruuvi_flash_info_t* const p_flash_info = &g_ruuvi_flash_info;
    return p_flash_info->is_ota0_active ? GW_GWUI_PARTITION : GW_GWUI_PARTITION_2;
}

ruuvi_esp32_fw_ver_str_t
fw_update_get_cur_version(void)
{
    const ruuvi_flash_info_t* const p_flash_info = &g_ruuvi_flash_info;
    ruuvi_esp32_fw_ver_str_t        version_str  = { 0 };
    snprintf(&version_str.buf[0], sizeof(version_str.buf), "%s", p_flash_info->p_app_desc->version);
    return version_str;
}

esp_err_t
erase_partition_with_sleep(const esp_partition_t* const p_partition)
{
    assert(p_partition != NULL);
    if ((p_partition->size % SPI_FLASH_SEC_SIZE) != 0)
    {
        return ESP_ERR_INVALID_SIZE;
    }
    size_t offset = 0;
    while (offset < p_partition->size)
    {
        const uint32_t  sector_address = p_partition->address + offset;
        const esp_err_t err = esp_flash_erase_region(p_partition->flash_chip, sector_address, SPI_FLASH_SEC_SIZE);
        if (ESP_OK != err)
        {
            LOG_ERR_ESP(err, "Failed to erase sector at 0x%08x", (printf_uint_t)(p_partition->address + offset));
            return err;
        }
        const fw_update_percentage_t percentage = (offset * FW_UPDATE_PERCENTAGE_50) / p_partition->size;
        fw_update_set_extra_info_for_status_json(g_update_progress_stage, percentage);
        vTaskDelay(pdMS_TO_TICKS(FW_UPDATE_DELAY_AFTER_OPERATION_WITH_FLASH_MS));
        offset += SPI_FLASH_SEC_SIZE;
    }
    return ESP_OK;
}

static bool
fw_update_handle_http_resp_code(
    const http_resp_code_e http_resp_code,
    const uint8_t* const   p_buf,
    const size_t           buf_size,
    bool* const            p_result)
{
    if (HTTP_RESP_CODE_200 != http_resp_code)
    {
        if ((HTTP_RESP_CODE_301 == http_resp_code) || (HTTP_RESP_CODE_302 == http_resp_code))
        {
            LOG_INFO("Got HTTP error %d: Redirect to another location", (printf_int_t)http_resp_code);
            *p_result = true;
            return true;
        }
        LOG_ERR("Got HTTP error %d: %.*s", (printf_int_t)http_resp_code, (printf_int_t)buf_size, (const char*)p_buf);
        *p_result = false;
        return true;
    }
    return false;
}

static bool
fw_update_data_partition_cb_on_recv_data(
    const uint8_t* const   p_buf,
    const size_t           buf_size,
    const size_t           offset,
    const size_t           content_length,
    const http_resp_code_e http_resp_code,
    void* const            p_user_data)
{
    fw_update_data_partition_info_t* const p_info = p_user_data;
    if (p_info->is_error)
    {
        return false;
    }
    bool result = false;
    if (fw_update_handle_http_resp_code(http_resp_code, p_buf, buf_size, &result))
    {
        if (!result)
        {
            p_info->is_error = true;
        }
        return result;
    }

    LOG_INFO(
        "Write to data partition %s, offset %lu, size %lu",
        p_info->p_partition->label,
        (printf_ulong_t)offset,
        (printf_ulong_t)buf_size);

    const fw_update_percentage_t percentage = FW_UPDATE_PERCENTAGE_50
                                              + (((offset + buf_size) * FW_UPDATE_PERCENTAGE_50) / content_length);
    fw_update_set_extra_info_for_status_json(g_update_progress_stage, percentage);

    const esp_err_t err = esp_partition_write(p_info->p_partition, p_info->offset, p_buf, buf_size);
    vTaskDelay(pdMS_TO_TICKS(FW_UPDATE_DELAY_AFTER_OPERATION_WITH_FLASH_MS));
    if (ESP_OK != err)
    {
        p_info->is_error = true;
        LOG_ERR_ESP(
            err,
            "Failed to write to partition %s at offset %lu",
            p_info->p_partition->label,
            (printf_ulong_t)p_info->offset);
        return false;
    }
    p_info->offset += buf_size;

    return true;
}

static bool
fw_update_data_partition(const esp_partition_t* const p_partition, const char* const p_url)
{
    LOG_INFO(
        "Update partition %s (address 0x%08x, size 0x%x) from %s",
        p_partition->label,
        p_partition->address,
        p_partition->size,
        p_url);
    fw_update_data_partition_info_t fw_update_info = {
        .p_partition = p_partition,
        .offset      = 0,
        .is_error    = false,
    };
    LOG_INFO("fw_update_data_partition: Erase partition");
    esp_err_t err = erase_partition_with_sleep(p_partition);
    if (ESP_OK != err)
    {
        LOG_ERR_ESP(
            err,
            "Failed to erase partition %s, address 0x%08x, size 0x%x",
            p_partition->label,
            p_partition->address,
            p_partition->size);
        return false;
    }
    LOG_INFO("fw_update_data_partition: Download and write partition data");
    const http_download_param_with_auth_t params = {
        .base = {
            .p_url = p_url,
            .timeout_seconds = HTTP_DOWNLOAD_FW_BINARIES_TIMEOUT_SECONDS,
            .flag_feed_task_watchdog = false,
            .flag_free_memory = true,
            .p_server_cert = NULL,
            .p_client_cert = NULL,
            .p_client_key = NULL,
        },
        .auth_type = GW_CFG_HTTP_AUTH_TYPE_NONE,
        .p_http_auth = NULL,
        .p_extra_header_item = NULL,
    };
    if (!http_download(&params, &fw_update_data_partition_cb_on_recv_data, &fw_update_info))
    {
        LOG_ERR("Failed to update partition %s - failed to download %s", p_partition->label, p_url);
        return false;
    }
    if (fw_update_info.is_error)
    {
        LOG_ERR("Failed to update partition %s - some problem during writing", p_partition->label);
        return false;
    }
    LOG_INFO("Partition %s has been successfully updated", p_partition->label);
    return true;
}

bool
fw_update_fatfs_gwui(const char* const p_url)
{
    const esp_partition_t* const p_partition = g_ruuvi_flash_info.p_next_fatfs_gwui_partition;
    if (NULL == p_partition)
    {
        LOG_ERR("Can't find partition to update fatfs_gwui");
        return false;
    }
    return fw_update_data_partition(p_partition, p_url);
}

bool
fw_update_fatfs_nrf52(const char* const p_url)
{
    const esp_partition_t* const p_partition = g_ruuvi_flash_info.p_next_fatfs_nrf52_partition;
    if (NULL == p_partition)
    {
        LOG_ERR("Can't find partition to update fatfs_nrf52");
        return false;
    }
    return fw_update_data_partition(p_partition, p_url);
}

static bool
fw_update_ota_partition_cb_on_recv_data(
    const uint8_t* const   p_buf,
    const size_t           buf_size,
    const size_t           offset,
    const size_t           content_length,
    const http_resp_code_e http_resp_code,
    void* const            p_user_data)
{
    fw_update_ota_partition_info_t* const p_info = p_user_data;
    if (p_info->is_error)
    {
        LOG_INFO("Drop data after an error, offset %lu, size %lu", (printf_ulong_t)offset, (printf_ulong_t)buf_size);
        return false;
    }
    bool result = false;
    if (fw_update_handle_http_resp_code(http_resp_code, p_buf, buf_size, &result))
    {
        if (!result)
        {
            p_info->is_error = true;
        }
        return result;
    }

    LOG_INFO(
        "Write to OTA-partition %s, offset %lu, size %lu",
        p_info->p_partition->label,
        (printf_ulong_t)offset,
        (printf_ulong_t)buf_size);

    const fw_update_percentage_t percentage = FW_UPDATE_PERCENTAGE_50
                                              + (((offset + buf_size) * FW_UPDATE_PERCENTAGE_50) / content_length);
    fw_update_set_extra_info_for_status_json(g_update_progress_stage, percentage);

    const esp_err_t err = esp_ota_write_patched(p_info->out_handle, p_buf, buf_size);
    vTaskDelay(pdMS_TO_TICKS(FW_UPDATE_DELAY_AFTER_OPERATION_WITH_FLASH_MS));
    if (ESP_OK != err)
    {
        p_info->is_error = true;
        LOG_ERR_ESP(err, "Failed to write to OTA-partition %s", p_info->p_partition->label);
        return false;
    }
    return true;
}

static bool
fw_update_ota_partition(
    const esp_partition_t* const p_partition,
    const esp_ota_handle_t       out_handle,
    const char* const            p_url)
{
    LOG_INFO(
        "Update OTA-partition %s (address 0x%08x, size 0x%x) from %s",
        p_partition->label,
        p_partition->address,
        p_partition->size,
        p_url);

    fw_update_ota_partition_info_t fw_update_info = {
        .p_partition = p_partition,
        .out_handle  = out_handle,
        .is_error    = false,
    };

    const http_download_param_with_auth_t params = {
        .base = {
            .p_url = p_url,
            .timeout_seconds = HTTP_DOWNLOAD_FW_BINARIES_TIMEOUT_SECONDS,
            .flag_feed_task_watchdog = false,
            .flag_free_memory = true,
            .p_server_cert = NULL,
            .p_client_cert = NULL,
            .p_client_key = NULL,
        },
        .auth_type = GW_CFG_HTTP_AUTH_TYPE_NONE,
        .p_http_auth = NULL,
        .p_extra_header_item = NULL,
    };
    if (!http_download(&params, &fw_update_ota_partition_cb_on_recv_data, &fw_update_info))
    {
        LOG_ERR("Failed to update OTA-partition %s - failed to download %s", p_partition->label, p_url);
        return false;
    }
    if (fw_update_info.is_error)
    {
        LOG_ERR("Failed to update OTA-partition %s - some problem during writing", p_partition->label);
        return false;
    }
    fw_update_set_extra_info_for_status_json(g_update_progress_stage, FW_UPDATE_PERCENTAGE_100);
    LOG_INFO("OTA-partition %s has been successfully updated", p_partition->label);
    return true;
}

static bool
fw_update_ota(const char* const p_url)
{
    const esp_partition_t* const p_partition = g_ruuvi_flash_info.p_next_update_partition;
    if (NULL == p_partition)
    {
        LOG_ERR("Can't find partition to update firmware");
        return false;
    }

    LOG_INFO("fw_update_ota: Erase partition");
    esp_ota_handle_t out_handle = 0;
    esp_err_t        err        = esp_ota_begin_patched(p_partition, &out_handle);
    if (ESP_OK != err)
    {
        LOG_ERR("%s failed", "esp_ota_begin");
        return false;
    }

    LOG_INFO("fw_update_ota: Download and write partition data");
    const bool res = fw_update_ota_partition(p_partition, out_handle, p_url);

    LOG_INFO("fw_update_ota: Finish writing to partition");
    err = esp_ota_end_patched(out_handle);
    if (ESP_OK != err)
    {
        LOG_ERR("%s failed", "esp_ota_end");
        return false;
    }

    return res;
}

static bool
json_fw_update_copy_string_val(
    const cJSON* const p_json_root,
    const char* const  p_attr_name,
    char* const        p_buf,
    const size_t       buf_len,
    const bool         flag_log_err_if_not_found)
{
    if (!json_wrap_copy_string_val(p_json_root, p_attr_name, p_buf, buf_len))
    {
        if (flag_log_err_if_not_found)
        {
            LOG_ERR("%s not found", p_attr_name);
        }
        return false;
    }
    LOG_DBG("%s: %s", p_attr_name, p_buf);
    return true;
}

static bool
json_fw_update_parse(const cJSON* const p_json_root, fw_update_config_t* const p_cfg)
{
    if (!json_fw_update_copy_string_val(p_json_root, "url", p_cfg->url, sizeof(p_cfg->url), true))
    {
        return false;
    }
    const size_t len = strlen(p_cfg->url);
    if ('/' == p_cfg->url[len - 1])
    {
        p_cfg->url[len - 1] = '\0';
    }
    return true;
}

bool
json_fw_update_parse_http_body(const char* const p_body)
{
    cJSON* p_json_root = cJSON_Parse(p_body);
    if (NULL == p_json_root)
    {
        LOG_ERR("Failed to parse json or no memory");
        return false;
    }

    const bool ret = json_fw_update_parse(p_json_root, &g_fw_update_cfg);
    cJSON_Delete(p_json_root);
    return ret;
}

static void
fw_update_set_extra_info_for_status_json(
    const enum fw_update_stage_e fw_updating_stage,
    const fw_update_percentage_t fw_updating_percentage)
{
    char extra_info_buf[JSON_NETWORK_EXTRA_INFO_SIZE];
    if (FW_UPDATE_STAGE_NONE == fw_updating_stage)
    {
        extra_info_buf[0] = '\0';
    }
    else
    {
        snprintf(
            extra_info_buf,
            sizeof(extra_info_buf),
            "\"fw_updating\":%u,\"percentage\":%u",
            (printf_uint_t)fw_updating_stage,
            (printf_uint_t)fw_updating_percentage);
    }
    wifi_manager_set_extra_info_for_status_json(extra_info_buf);
}

void
fw_update_set_extra_info_for_status_json_update_reset(void)
{
    fw_update_set_extra_info_for_status_json(FW_UPDATE_STAGE_NONE, 0);
}

void
fw_update_set_extra_info_for_status_json_update_start(void)
{
    fw_update_set_extra_info_for_status_json(FW_UPDATE_STAGE_1, 0);
}

void
fw_update_set_extra_info_for_status_json_update_successful(void)
{
    char extra_info_buf[JSON_NETWORK_EXTRA_INFO_SIZE];
    snprintf(extra_info_buf, sizeof(extra_info_buf), "\"fw_updating\":%u", (printf_uint_t)FW_UPDATE_STAGE_SUCCESSFUL);
    wifi_manager_set_extra_info_for_status_json(extra_info_buf);
}

void
fw_update_set_extra_info_for_status_json_update_failed(const char* const p_message)
{
    char extra_info_buf[JSON_NETWORK_EXTRA_INFO_SIZE];
    snprintf(
        extra_info_buf,
        sizeof(extra_info_buf),
        "\"fw_updating\":%u,\"message\":\"%s\"",
        (printf_uint_t)FW_UPDATE_STAGE_FAILED,
        (NULL != p_message) ? p_message : "");
    wifi_manager_set_extra_info_for_status_json(extra_info_buf);
}

void
fw_update_set_extra_info_for_status_json_update_failed_nrf52(const char* const p_message)
{
    char extra_info_buf[JSON_NETWORK_EXTRA_INFO_SIZE];
    snprintf(
        extra_info_buf,
        sizeof(extra_info_buf),
        "\"fw_updating\":%u,\"message\":\"%s\"",
        (printf_uint_t)FW_UPDATE_STAGE_FAILED_NRF52,
        (NULL != p_message) ? p_message : "");
    wifi_manager_set_extra_info_for_status_json(extra_info_buf);
}

void
fw_update_nrf52fw_cb_progress(const size_t num_bytes_flashed, const size_t total_size, void* const p_param)
{
    (void)p_param;
    const fw_update_percentage_t percentage = (num_bytes_flashed * FW_UPDATE_PERCENTAGE_100) / total_size;
    fw_update_set_extra_info_for_status_json(g_update_progress_stage, percentage);
}

void
fw_update_set_stage_nrf52_updating(void)
{
    g_update_progress_stage = FW_UPDATE_STAGE_4;
    fw_update_set_extra_info_for_status_json(g_update_progress_stage, 0);
}

static void
fw_update_nrf52fw_cb_before_updating(void)
{
    LOG_INFO("### Start nRF52 updating");
}

static void
fw_update_nrf52fw_cb_after_updating(const bool flag_success)
{
    LOG_INFO("### Finish nRF52 updating, flag_success=%d", (printf_int_t)flag_success);
}

static bool
fw_update_ruuvi_gateway_esp_bin(void)
{
    LOG_INFO("fw_update_ota");
    str_buf_t url = str_buf_printf_with_alloc("%s/%s", g_fw_update_cfg.url, "ruuvi_gateway_esp.bin");
    if (NULL == url.buf)
    {
        LOG_ERR("Can't allocate memory");
        return false;
    }
    if (!fw_update_ota(url.buf))
    {
        LOG_ERR("%s failed", "fw_update_ota");
        str_buf_free_buf(&url);
        return false;
    }
    str_buf_free_buf(&url);
    return true;
}

static bool
fw_update_fatfs_gwui_bin(void)
{
    LOG_INFO("fw_update_fatfs_gwui");
    str_buf_t url = str_buf_printf_with_alloc("%s/%s", g_fw_update_cfg.url, "fatfs_gwui.bin");
    if (NULL == url.buf)
    {
        LOG_ERR("Can't allocate memory");
        return false;
    }
    if (!fw_update_fatfs_gwui(url.buf))
    {
        LOG_ERR("%s failed", "fw_update_fatfs_gwui");
        str_buf_free_buf(&url);
        return false;
    }
    str_buf_free_buf(&url);
    return true;
}

static bool
fw_update_fatfs_nrf52_bin(void)
{
    LOG_INFO("fw_update_fatfs_nrf52");
    str_buf_t url = str_buf_printf_with_alloc("%s/%s", g_fw_update_cfg.url, "fatfs_nrf52.bin");
    if (NULL == url.buf)
    {
        LOG_ERR("Can't allocate memory");
        return false;
    }
    if (!fw_update_fatfs_nrf52(url.buf))
    {
        LOG_ERR("%s failed", "fw_update_fatfs_nrf52");
        str_buf_free_buf(&url);
        return false;
    }
    str_buf_free_buf(&url);
    return true;
}

static bool
fw_update_do_actions(void)
{
    g_update_progress_stage = FW_UPDATE_STAGE_1;
    fw_update_set_extra_info_for_status_json(g_update_progress_stage, 0);

    if (!fw_update_ruuvi_gateway_esp_bin())
    {
        LOG_ERR("%s failed", "fw_update_ruuvi_gateway_esp_bin");
        fw_update_set_extra_info_for_status_json_update_failed("Failed to update OTA");
        return false;
    }

    g_update_progress_stage = FW_UPDATE_STAGE_2;
    fw_update_set_extra_info_for_status_json(g_update_progress_stage, 0);

    if (!fw_update_fatfs_gwui_bin())
    {
        LOG_ERR("%s failed", "fw_update_fatfs_gwui_bin");
        fw_update_set_extra_info_for_status_json_update_failed("Failed to update GWUI");
        return false;
    }

    g_update_progress_stage = FW_UPDATE_STAGE_3;
    fw_update_set_extra_info_for_status_json(g_update_progress_stage, 0);

    if (!fw_update_fatfs_nrf52_bin())
    {
        LOG_ERR("%s failed", "fw_update_fatfs_nrf52_bin");
        fw_update_set_extra_info_for_status_json_update_failed("Failed to update nRF52");
        return false;
    }

    LOG_INFO("esp_ota_set_boot_partition");
    const esp_err_t err = esp_ota_set_boot_partition(g_ruuvi_flash_info.p_next_update_partition);
    if (ESP_OK != err)
    {
        LOG_ERR_ESP(err, "%s failed", "esp_ota_set_boot_partition");
        fw_update_set_extra_info_for_status_json_update_failed("Failed to switch boot partition");
        return false;
    }

    g_ruuvi_flash_info.p_boot_partition = g_ruuvi_flash_info.p_next_update_partition;
    g_ruuvi_flash_info.is_ota0_active   = !g_ruuvi_flash_info.is_ota0_active;

    fw_update_set_stage_nrf52_updating();

    LOG_INFO("nrf52fw_update_fw_if_necessary");
    if (!nrf52fw_update_fw_if_necessary(
            fw_update_get_current_fatfs_nrf52_partition_name(),
            &fw_update_nrf52fw_cb_progress,
            NULL,
            &fw_update_nrf52fw_cb_before_updating,
            &fw_update_nrf52fw_cb_after_updating,
            NULL))
    {
        LOG_ERR("%s failed", "nrf52fw_update_fw_if_necessary");
        fw_update_set_extra_info_for_status_json_update_failed_nrf52("Failed to update nRF52 firmware");
        return false;
    }
    fw_update_set_extra_info_for_status_json_update_successful();
    return true;
}

static const char*
fw_update_get_reboot_reason_msg(const fw_updating_reason_e fw_updating_reason, const bool flag_fw_update_successful)
{
    if (!flag_fw_update_successful)
    {
        switch (fw_updating_reason)
        {
            case FW_UPDATE_REASON_NONE:
                LOG_ERR("Firmware updating failed");
                return "Restart the system after firmware update (failed)";
            case FW_UPDATE_REASON_AUTO:
                LOG_INFO("Firmware updating failed (auto-updating)");
                return "Restart the system after firmware update (auto-updating failed)";
            case FW_UPDATE_REASON_MANUAL_VIA_HOTSPOT:
                LOG_INFO("Firmware updating failed (manual updating via WiFi hotspot)");
                return "Restart the system after firmware update (manual updating via WiFi hotspot failed)";
            case FW_UPDATE_REASON_MANUAL_VIA_LAN:
                LOG_INFO("Firmware updating failed (manual updating via LAN)");
                return "Restart the system after firmware update (manual updating via LAN failed)";
        }
        LOG_ERR("Firmware updating failed (reason is unknown)");
        assert(0);
        return "Restart the system after firmware update (failed, reason is unknown)";
    }
    switch (fw_updating_reason)
    {
        case FW_UPDATE_REASON_NONE:
            LOG_INFO("Firmware updating completed successfully (unknown reason)");
            return "Restart the system after firmware update (completed successfully)";
        case FW_UPDATE_REASON_AUTO:
            LOG_INFO("Firmware updating completed successfully (auto-updating)");
            return "Restart the system after firmware update (auto-updating completed successfully)";
        case FW_UPDATE_REASON_MANUAL_VIA_HOTSPOT:
            LOG_INFO("Firmware updating completed successfully (manual updating via WiFi hotspot)");
            return "Restart the system after firmware update (manual updating via WiFi hotspot completed successfully)";
        case FW_UPDATE_REASON_MANUAL_VIA_LAN:
            LOG_INFO("Firmware updating completed successfully (manual updating via LAN)");
            return "Restart the system after firmware update (manual updating via LAN completed successfully)";
    }
    LOG_ERR("Firmware updating completed successfully (unknown reason)");
    assert(0);
    return "Restart the system after firmware update (completed successfully, unknown reason)";
}

static void
fw_update_task(void)
{
    LOG_INFO("Firmware updating started, URL: %s", g_fw_update_cfg.url);

    leds_notify_nrf52_fw_updating();
    main_task_stop_timer_check_for_remote_cfg();

    const bool flag_wait_until_relaying_stopped = true;
    gw_status_suspend_relaying(flag_wait_until_relaying_stopped);

    if (!wifi_manager_is_ap_active())
    {
        LOG_INFO("WiFi AP is not active - start WiFi AP");
        const bool flag_block_req_from_lan = (FW_UPDATE_REASON_MANUAL_VIA_LAN == g_fw_updating_reason) ? false : true;
        wifi_manager_start_ap(flag_block_req_from_lan);
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
    timer_cfg_mode_deactivation_stop();

    const bool flag_fw_update_successful = fw_update_do_actions();

    const char* const p_reboot_reason_msg = fw_update_get_reboot_reason_msg(
        g_fw_updating_reason,
        flag_fw_update_successful);

    if (!flag_fw_update_successful)
    {
        g_fw_updating_reason = FW_UPDATE_REASON_NONE;
    }
    else
    {
        switch (g_fw_updating_reason)
        {
            case FW_UPDATE_REASON_AUTO:
                settings_write_flag_rebooting_after_auto_update(true);
                break;
            case FW_UPDATE_REASON_MANUAL_VIA_HOTSPOT:
                if (wifi_manager_is_ap_sta_ip_assigned())
                {
                    settings_write_flag_force_start_wifi_hotspot(FORCE_START_WIFI_HOTSPOT_ONCE);
                }
                break;
            default:
                break;
        }
        LOG_INFO("Wait 5 seconds before reboot");
        sleep_with_task_watchdog_feeding(FW_UPDATE_DELAY_BEFORE_REBOOT_SECONDS);
    }
    gateway_restart(p_reboot_reason_msg);
}

bool
fw_update_is_url_valid(void)
{
    const char url_http_prefix[] = "http://";
    if (0 == strncmp(g_fw_update_cfg.url, url_http_prefix, strlen(url_http_prefix)))
    {
        return true;
    }
    const char url_https_prefix[] = "https://";
    if (0 == strncmp(g_fw_update_cfg.url, url_https_prefix, strlen(url_https_prefix)))
    {
        return true;
    }
    return false;
}

ATTR_PRINTF(1, 2)
void
fw_update_set_url(const char* const p_url_fmt, ...)
{
    va_list args;
    va_start(args, p_url_fmt);
    (void)vsnprintf(g_fw_update_cfg.url, sizeof(g_fw_update_cfg.url), p_url_fmt, args);
    va_end(args);
}

const char*
fw_update_get_url(void)
{
    return g_fw_update_cfg.url;
}

bool
fw_update_run(const fw_updating_reason_e fw_updating_reason)
{
    g_fw_updating_reason = fw_updating_reason;
    if (!os_task_create_finite_without_param(
            &fw_update_task,
            "fw_update_task",
            FW_UPDATE_TASK_STACK_SIZE,
            FW_UPDATE_TASK_PRIORITY))
    {
        LOG_ERR("Can't create thread");
        g_fw_updating_reason = FW_UPDATE_REASON_NONE;
        return false;
    }
    return true;
}
