/**
 * @file fw_update.c
 * @author TheSomeMan
 * @date 2021-05-30
 * @copyright Ruuvi Innovations Ltd, license BSD-3-Clause.
 */

#include "fw_update.h"
#include <string.h>
#include "http.h"
#include "esp_type_wrapper.h"
#include "cJSON.h"
#include "cjson_wrap.h"
#include "wifi_manager.h"
#include "http_server.h"
#include "esp_ota_ops_patched.h"
#include "nrf52fw.h"
#include "leds.h"

#define LOG_LOCAL_LEVEL LOG_LEVEL_DEBUG
#include "log.h"

#define GW_GWUI_PARTITION_2 GW_GWUI_PARTITION "_2"
#define GW_NRF_PARTITION_2  GW_NRF_PARTITION "_2"

#define FW_UPDATE_DELAY_AFTER_OPERATION_WITH_FLASH_MS (20)

#define FW_UPDATE_URL_MAX_LEN (128U)

#define FW_UPDATE_PERCENTAGE_50  (50U)
#define FW_UPDATE_PERCENTAGE_100 (100U)

#define FW_UPDATE_DELAY_BEFORE_REBOOT_MS (5U * 1000U)

typedef struct fw_update_config_t
{
    char url[FW_UPDATE_URL_MAX_LEN + 1];
} fw_update_config_t;

typedef struct fw_update_data_partition_info_t
{
    const esp_partition_t *const p_partition;
    size_t                       offset;
    bool                         is_error;
} fw_update_data_partition_info_t;

typedef struct fw_update_ota_partition_info_t
{
    const esp_partition_t *const p_partition;
    esp_ota_handle_t             out_handle;
    bool                         is_error;
} fw_update_ota_partition_info_t;

typedef enum fw_update_stage_e
{
    FW_UPDATE_STAGE_NONE       = 0,
    FW_UPDATE_STAGE_1          = 1,
    FW_UPDATE_STAGE_2          = 2,
    FW_UPDATE_STAGE_3          = 3,
    FW_UPDATE_STAGE_4          = 4,
    FW_UPDATE_STAGE_SUCCESSFUL = 5,
    FW_UPDATE_STAGE_FAILED     = 6,
} fw_update_stage_e;

typedef uint32_t fw_update_percentage_t;

static void
fw_update_set_extra_info_for_status_json(
    const enum fw_update_stage_e fw_updating_stage,
    const fw_update_percentage_t fw_updating_percentage);

static const char TAG[] = "fw_update";

static ruuvi_flash_info_t g_ruuvi_flash_info;
static fw_update_config_t g_fw_update_cfg;
static fw_update_stage_e  g_update_progress_stage;

static const esp_partition_t *
find_data_fat_partition_by_name(const char *const p_partition_name)
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
    const esp_partition_t *p_partition = esp_partition_get(iter);
    esp_partition_iterator_release(iter);
    return p_partition;
}

static bool
fw_update_read_flash_info_internal(ruuvi_flash_info_t *const p_flash_info)
{
    p_flash_info->is_valid = false;

    p_flash_info->p_app_desc = esp_ota_get_app_description();
    LOG_INFO("Project name     : %s", p_flash_info->p_app_desc->project_name);
    LOG_INFO("Firmware version : %s", p_flash_info->p_app_desc->version);

    p_flash_info->p_boot_partition = esp_ota_get_boot_partition();
    if (NULL == p_flash_info->p_boot_partition)
    {
        LOG_ERR("There is no boot partition info");
        return false;
    }
    p_flash_info->p_running_partition = esp_ota_get_running_partition();
    if (NULL == p_flash_info->p_running_partition)
    {
        LOG_ERR("There is no running partition info");
        return false;
    }
    LOG_INFO("Currently running partition: %s", p_flash_info->p_running_partition->label);
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
    const char *const p_gwui_parition_name    = p_flash_info->is_ota0_active ? GW_GWUI_PARTITION_2 : GW_GWUI_PARTITION;
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

    const char *const p_fatfs_nrf52_partition_name = p_flash_info->is_ota0_active ? GW_NRF_PARTITION_2
                                                                                  : GW_NRF_PARTITION;
    p_flash_info->p_next_fatfs_nrf52_partition = find_data_fat_partition_by_name(p_fatfs_nrf52_partition_name);
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
    ruuvi_flash_info_t *p_flash_info = &g_ruuvi_flash_info;
    fw_update_read_flash_info_internal(p_flash_info);
    return p_flash_info->is_valid;
}

const char *
fw_update_get_current_fatfs_nrf52_partition_name(void)
{
    const ruuvi_flash_info_t *const p_flash_info = &g_ruuvi_flash_info;
    return p_flash_info->is_ota0_active ? GW_NRF_PARTITION : GW_NRF_PARTITION_2;
}

const char *
fw_update_get_current_fatfs_gwui_partition_name(void)
{
    const ruuvi_flash_info_t *const p_flash_info = &g_ruuvi_flash_info;
    return p_flash_info->is_ota0_active ? GW_GWUI_PARTITION : GW_GWUI_PARTITION_2;
}

const char *
fw_update_get_cur_version(void)
{
    const ruuvi_flash_info_t *const p_flash_info = &g_ruuvi_flash_info;
    return p_flash_info->p_app_desc->version;
}

esp_err_t
erase_partition_with_sleep(const esp_partition_t *const p_partition)
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

static void
fw_update_data_partition_cb_on_recv_data(
    const uint8_t *const p_buf,
    const size_t         buf_size,
    const size_t         offset,
    const size_t         content_length,
    void *const          p_user_data)
{
    fw_update_data_partition_info_t *const p_info = p_user_data;
    if (p_info->is_error)
    {
        return;
    }
    LOG_INFO(
        "Write to partition %s, offset %lu, size %lu",
        p_info->p_partition->label,
        (printf_ulong_t)offset,
        (printf_ulong_t)buf_size);

    const fw_update_percentage_t percentage = FW_UPDATE_PERCENTAGE_50
                                              + (((offset + buf_size) * FW_UPDATE_PERCENTAGE_50) / content_length);
    fw_update_set_extra_info_for_status_json(g_update_progress_stage, percentage);

    const esp_err_t err = esp_partition_write(p_info->p_partition, p_info->offset, p_buf, buf_size);
    if (ESP_OK != err)
    {
        p_info->is_error = true;
        LOG_ERR_ESP(
            err,
            "Failed to write to partition %s at offset %lu",
            p_info->p_partition->label,
            (printf_ulong_t)p_info->offset);
        return;
    }
    p_info->offset += buf_size;

    vTaskDelay(pdMS_TO_TICKS(FW_UPDATE_DELAY_AFTER_OPERATION_WITH_FLASH_MS));
}

static bool
fw_update_data_partition(const esp_partition_t *const p_partition, const char *const p_url)
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
    if (!http_download(p_url, &fw_update_data_partition_cb_on_recv_data, &fw_update_info))
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
fw_update_fatfs_gwui(const char *const p_url)
{
    const esp_partition_t *const p_partition = g_ruuvi_flash_info.p_next_fatfs_gwui_partition;
    if (NULL == p_partition)
    {
        LOG_ERR("Can't find partition to update fatfs_gwui");
        return false;
    }
    return fw_update_data_partition(p_partition, p_url);
}

bool
fw_update_fatfs_nrf52(const char *const p_url)
{
    const esp_partition_t *const p_partition = g_ruuvi_flash_info.p_next_fatfs_nrf52_partition;
    if (NULL == p_partition)
    {
        LOG_ERR("Can't find partition to update fatfs_nrf52");
        return false;
    }
    return fw_update_data_partition(p_partition, p_url);
}

static void
fw_update_ota_partition_cb_on_recv_data(
    const uint8_t *const p_buf,
    const size_t         buf_size,
    const size_t         offset,
    const size_t         content_length,
    void *const          p_user_data)
{
    fw_update_ota_partition_info_t *const p_info = p_user_data;
    if (p_info->is_error)
    {
        return;
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
    if (ESP_OK != err)
    {
        p_info->is_error = true;
        LOG_ERR_ESP(err, "Failed to write to OTA-partition %s", p_info->p_partition->label);
    }
    vTaskDelay(pdMS_TO_TICKS(FW_UPDATE_DELAY_AFTER_OPERATION_WITH_FLASH_MS));
}

static bool
fw_update_ota_partition(
    const esp_partition_t *const p_partition,
    const esp_ota_handle_t       out_handle,
    const char *const            p_url)
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

    if (!http_download(p_url, &fw_update_ota_partition_cb_on_recv_data, &fw_update_info))
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
fw_update_ota(const char *const p_url)
{
    const esp_partition_t *const p_partition = g_ruuvi_flash_info.p_next_update_partition;
    if (NULL == p_partition)
    {
        LOG_ERR("Can't find partition to update firmware");
        return false;
    }

    esp_ota_handle_t out_handle = 0;
    esp_err_t        err        = esp_ota_begin_patched(p_partition, &out_handle);
    if (ESP_OK != err)
    {
        LOG_ERR("%s failed", "esp_ota_begin");
        return false;
    }

    const bool res = fw_update_ota_partition(p_partition, out_handle, p_url);

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
    const cJSON *const p_json_root,
    const char *const  p_attr_name,
    char *const        p_buf,
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
json_fw_update_parse(const cJSON *const p_json_root, fw_update_config_t *const p_cfg)
{
    if (!json_fw_update_copy_string_val(p_json_root, "url", p_cfg->url, sizeof(p_cfg->url), true))
    {
        return false;
    }
    return true;
}

bool
json_fw_update_parse_http_body(const char *const p_body)
{
    cJSON *p_json_root = cJSON_Parse(p_body);
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
fw_update_set_extra_info_for_status_json_update_failed(const char *const p_message)
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

static void
fw_update_nrf52fw_cb_progress(const size_t num_bytes_flashed, const size_t total_size, void *const p_param)
{
    (void)p_param;
    const fw_update_percentage_t percentage = (num_bytes_flashed * FW_UPDATE_PERCENTAGE_100) / total_size;
    fw_update_set_extra_info_for_status_json(g_update_progress_stage, percentage);
}

static void
fw_update_task(void)
{
    char url[sizeof(g_fw_update_cfg.url) + 32];

    LOG_INFO("Firmware updating started, URL: %s", g_fw_update_cfg.url);

    http_server_disable_ap_stopping_by_timeout();

    g_update_progress_stage = FW_UPDATE_STAGE_1;
    fw_update_set_extra_info_for_status_json(g_update_progress_stage, 0);

    snprintf(url, sizeof(url), "%s/%s", g_fw_update_cfg.url, "ruuvi_gateway_esp.bin");
    if (!fw_update_ota(url))
    {
        LOG_ERR("%s failed", "fw_update_ota");
        fw_update_set_extra_info_for_status_json_update_failed("Failed to update OTA");
        return;
    }

    g_update_progress_stage = FW_UPDATE_STAGE_2;
    fw_update_set_extra_info_for_status_json(g_update_progress_stage, 0);

    snprintf(url, sizeof(url), "%s/%s", g_fw_update_cfg.url, "fatfs_gwui.bin");
    if (!fw_update_fatfs_gwui(url))
    {
        LOG_ERR("%s failed", "fw_update_fatfs_gwui");
        fw_update_set_extra_info_for_status_json_update_failed("Failed to update GWUI");
        return;
    }

    g_update_progress_stage = FW_UPDATE_STAGE_3;
    fw_update_set_extra_info_for_status_json(g_update_progress_stage, 0);

    snprintf(url, sizeof(url), "%s/%s", g_fw_update_cfg.url, "fatfs_nrf52.bin");
    if (!fw_update_fatfs_nrf52(url))
    {
        LOG_ERR("%s failed", "fw_update_fatfs_nrf52");
        fw_update_set_extra_info_for_status_json_update_failed("Failed to update nRF52");
        return;
    }

    const esp_err_t err = esp_ota_set_boot_partition(g_ruuvi_flash_info.p_next_update_partition);
    if (ESP_OK != err)
    {
        LOG_ERR_ESP(err, "%s failed", "esp_ota_set_boot_partition");
        fw_update_set_extra_info_for_status_json_update_failed("Failed to switch boot partition");
        return;
    }

    g_ruuvi_flash_info.p_boot_partition = g_ruuvi_flash_info.p_next_update_partition;
    g_ruuvi_flash_info.is_ota0_active   = !g_ruuvi_flash_info.is_ota0_active;

    const char *const p_fatfs_nrf52_partition_name = g_ruuvi_flash_info.is_ota0_active ? GW_NRF_PARTITION_2
                                                                                       : GW_NRF_PARTITION;

    g_update_progress_stage = FW_UPDATE_STAGE_4;
    fw_update_set_extra_info_for_status_json(g_update_progress_stage, 0);

    leds_indication_on_nrf52_fw_updating();

    nrf52fw_update_fw_if_necessary(p_fatfs_nrf52_partition_name, &fw_update_nrf52fw_cb_progress, NULL);
    leds_off();

    fw_update_set_extra_info_for_status_json_update_successful();

    LOG_INFO("Wait 5 seconds before reboot");
    vTaskDelay(pdMS_TO_TICKS(FW_UPDATE_DELAY_BEFORE_REBOOT_MS));

    LOG_INFO("Restart system");
    esp_restart();
}

bool
fw_update_is_url_valid(void)
{
    const char url_prefix[] = "http";
    if (0 != strncmp(g_fw_update_cfg.url, url_prefix, strlen(url_prefix)))
    {
        return false;
    }
    return true;
}

bool
fw_update_run(void)
{
    const uint32_t stack_size_for_fw_update_task = 4 * 1024;
    if (!os_task_create_finite_without_param(&fw_update_task, "fw_update_task", stack_size_for_fw_update_task, 1))
    {
        LOG_ERR("Can't create thread");
        return false;
    }
    return true;
}
