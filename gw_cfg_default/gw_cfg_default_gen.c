/**
 * @file gw_cfg_default_gen.c
 * @author TheSomeMan
 * @date 2026-06-23
 * @copyright Ruuvi Innovations Ltd, license BSD-3-Clause.
 */

#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include "nvs.h"
#include "lwip/ip4_addr.h"
#include "wifi_manager_defs.h"
#include "os_mutex.h"
#include "os_mutex_recursive.h"
#include "os_task.h"
#include "esp_log.h"
#include "gw_cfg.h"
#include "gw_cfg_default.h"
#include "cjson_wrap.h"
#include "gw_cfg_json_generate.h"
#include "event_mgr.h"

void
gw_cfg_log(const gw_cfg_t* const p_gw_cfg, const char* const p_title, const bool flag_log_device_info)
{
    (void)p_gw_cfg;
    (void)p_title;
    (void)flag_log_device_info;
}

void
wifi_manager_cb_save_wifi_config_sta(const wifiman_config_sta_t* const p_cfg_sta)
{
}

const char*
os_task_get_name(void)
{
    static const char g_task_name[] = "main";
    return (char*)g_task_name;
}

os_task_priority_t
os_task_get_priority(void)
{
    return 0;
}

os_mutex_recursive_t
os_mutex_recursive_create_static(os_mutex_recursive_static_t* const p_mutex_static)
{
    return (os_mutex_recursive_t)p_mutex_static;
}

void
os_mutex_recursive_delete(os_mutex_recursive_t* const ph_mutex)
{
}

void
os_mutex_recursive_lock(os_mutex_recursive_t const h_mutex)
{
}

void
os_mutex_recursive_unlock(os_mutex_recursive_t const h_mutex)
{
}

os_mutex_t
os_mutex_create_static(os_mutex_static_t* const p_mutex_static)
{
    return (os_mutex_t)p_mutex_static;
}

void
os_mutex_lock(os_mutex_t const h_mutex)
{
    (void)h_mutex;
}

void
os_mutex_unlock(os_mutex_t const h_mutex)
{
    (void)h_mutex;
}

uint32_t
esp_log_timestamp(void)
{
    return 0;
}

void
esp_log_write(esp_log_level_t level, const char* tag, const char* fmt, ...)
{
    (void)level;
    (void)tag;
    (void)fmt;
}

void
event_mgr_notify(const event_mgr_ev_e event)
{
}

uint32_t
esp_ip4addr_aton(const char* addr)
{
    return ipaddr_addr(addr);
}

bool
ruuvi_nvs_init_gw_cfg_storage(void)
{
    return true;
}

bool
ruuvi_nvs_deinit_gw_cfg_storage(void)
{
    return true;
}

void
ruuvi_nvs_erase_gw_cfg_storage(void)
{
}

bool
ruuvi_nvs_open_gw_cfg_storage(nvs_open_mode_t open_mode, nvs_handle_t* p_handle)
{
    (void)open_mode;
    return true;
}

void
nvs_close(nvs_handle_t handle)
{
    (void)handle;
}

esp_err_t
nvs_get_str(nvs_handle_t handle, const char* key, char* out_value, size_t* length)
{
    (void)handle;
    (void)key;
    (void)out_value;
    (void)length;

    return ESP_ERR_NVS_NOT_FOUND;
}

esp_err_t
nvs_set_str(nvs_handle_t handle, const char* key, const char* value)
{
    (void)handle;
    (void)key;
    (void)value;

    return ESP_OK;
}

esp_err_t
nvs_get_blob(nvs_handle_t handle, const char* key, void* out_value, size_t* length)
{
    (void)handle;
    (void)key;
    (void)out_value;
    (void)length;

    return ESP_ERR_NVS_NOT_FOUND;
}

esp_err_t
nvs_set_blob(nvs_handle_t handle, const char* key, const void* value, size_t length)
{
    (void)handle;
    (void)key;
    (void)value;
    (void)length;

    return ESP_OK;
}

esp_err_t
nvs_erase_key(nvs_handle_t handle, const char* key)
{
    (void)handle;
    (void)key;

    return ESP_OK;
}

const char*
wrap_esp_err_to_name_r(const esp_err_t code, char* const p_buf, const size_t buf_len)
{
    (void)snprintf(p_buf, buf_len, "Error 0x%x(%d)", code, code);
    return p_buf;
}

bool
default_json_read_callback(gw_cfg_t* const p_gw_cfg_default)
{
    (void)p_gw_cfg_default;

    return true;
}

int
main(void)
{
    const gw_cfg_default_init_param_t init_param = { 0 };
    if (!gw_cfg_default_init(&init_param, &default_json_read_callback))
    {
        fprintf(stderr, "Failed to initialize default gateway configuration\n");
        return 1;
    }

    gw_cfg_t gw_cfg = { 0 };
    gw_cfg_default_get(&gw_cfg);
    cjson_wrap_str_t json_str = cjson_wrap_str_null();

    gw_cfg_json_generate_for_saving(&gw_cfg, &json_str);
    printf("%s\n", json_str.p_str);
    cjson_wrap_free_json_str(&json_str);

    return 0;
}
