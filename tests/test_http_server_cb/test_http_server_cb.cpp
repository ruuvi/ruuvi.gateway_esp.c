/**
 * @file test_http_server_cb.cpp
 * @author TheSomeMan
 * @date 2020-08-27
 * @copyright Ruuvi Innovations Ltd, license BSD-3-Clause.
 */

#include "http_server_cb.h"
#include <cstring>
#include <cerrno>
#include <sys/time.h>
#include <utility>
#include <vector>
#include <functional>
#include "gtest/gtest.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log_wrapper.hpp"
#include "gw_mac.h"
#include "gw_cfg.h"
#include "json_ruuvi.h"
#include "flashfatfs.h"
#include "metrics.h"
#include "adv_table.h"
#include "str_buf.h"
#include "ruuvi_device_id.h"
#include "os_malloc.h"
#include "os_mutex_recursive.h"
#include "os_mutex.h"
#include "os_task.h"
#include "gw_cfg_default.h"
#include "fw_ver.h"
#include "lwip/ip4_addr.h"
#include "event_mgr.h"
#include "gw_cfg_storage.h"
#include "partition_table.h"
#include "http.h"
#include "http_server_resp.h"
#include "cjson_wrap.h"
#include "ruuvi_gateway.h"
#include "http_post_helper.h"
#include "http_download.h"
#include "fw_update.h"

#ifdef __cplusplus
extern "C" {
#endif

#include "http.h"

#ifdef __cplusplus
}
#endif

using namespace std;

/*** Google-test class implementation
 * *********************************************************************************/

class TestHttpServerCb;
static TestHttpServerCb* g_pTestClass;

static struct
{
    const char* p_file_name;
    bool        flag_file_exists;
    size_t      file_size;
    const char* p_content;
    bool        flag_write_success;
    bool        flag_delete_success;
} g_extra_cfg_storage;

struct flash_fat_fs_t
{
    string                   mount_point;
    string                   partition_label;
    flash_fat_fs_num_files_t max_files;
};

extern "C" {

volatile uint32_t g_cnt_cfg_button_pressed;

const char*
os_task_get_name(void)
{
    static const char g_task_name[] = "main";
    return const_cast<char*>(g_task_name);
}

os_task_priority_t
os_task_get_priority(void)
{
    return 0;
}

uint32_t
esp_random(void)
{
    return 0;
}

bool
http_server_decrypt_by_params(
    const char* const p_encrypted_val,
    const char* const p_iv,
    const char* const p_hash,
    str_buf_t* const  p_str_buf)
{
    return false;
}

void
reset_task_reboot_after_timeout(void)
{
}

void
fw_update_set_extra_info_for_status_json_update_start(void)
{
}

void
fw_update_set_extra_info_for_status_json_update_failed(const char* const p_message)
{
}

void
fw_update_set_extra_info_for_status_json_update_reset(void)
{
}

bool
json_fw_update_parse_http_body(const char* const p_body)
{
    return false;
}

bool
fw_update_binaries_is_url_valid(void)
{
    return true;
}

str_buf_t
json_fw_update_url_parse_http_body_get_url(const char* const p_body)
{
    return str_buf_init_null();
}

void
main_task_schedule_next_check_for_fw_updates(void)
{
}

void
main_task_schedule_retry_check_for_fw_updates(void)
{
}

void
network_timeout_update_timestamp(void)
{
}

void
settings_save_to_flash(const gw_cfg_t* const p_gw_cfg);

void
main_task_on_get_history(void)
{
}

void
main_task_send_sig_restart_services(void)
{
    esp_log_write(ESP_LOG_VERBOSE, "test", "V (0) test: [test/1] restart_services");
}

void
main_task_stop_wifi_hotspot_after_short_delay(void)
{
    esp_log_write(ESP_LOG_VERBOSE, "test", "V (0) test: [test/1] stop_wifi_hotspot_after_short_delay");
}

void
gw_status_suspend_relaying(const bool flag_wait_until_relaying_stopped)
{
}

void
gw_status_suspend_http_relaying(const bool flag_wait_until_relaying_stopped)
{
}

void
gw_status_resume_relaying(void)
{
}

void
gw_status_resume_http_relaying(void)
{
}

http_server_resp_t
http_check_post_advs(const http_check_params_t* const p_params, const TimeUnitsSeconds_t timeout_seconds)
{
    if ((0 == strcmp(p_params->p_url, "https://myserver.com/")) && (GW_CFG_HTTP_AUTH_TYPE_NONE == p_params->auth_type)
        && (!p_params->use_extra_http_path) && (!p_params->use_extra_http_query) && (!p_params->use_extra_http_headers))
    {
        static const char resp_content[]
            = "{\n"
              "\t\"status\":\t200,\n"
              "\t\"message\":\t\"{}\"\n"
              "}";
        const str_buf_t str_buf = str_buf_printf_with_alloc("%s", resp_content);
        assert(NULL != str_buf.buf);
        const http_server_resp_t resp = {
            .http_resp_code       = HTTP_RESP_CODE_200,
            .content_location     = HTTP_CONTENT_LOCATION_HEAP,
            .flag_no_cache        = true,
            .flag_add_header_date = true,
            .content_type         = HTTP_CONTENT_TYPE_APPLICATION_JSON,
            .p_content_type_param = NULL,
            .content_len          = strlen(str_buf.buf),
            .content_encoding     = HTTP_CONTENT_ENCODING_NONE,
            .select_location = {
                .heap = {
                    .p_buf = (uint8_t*)str_buf.buf,
                },
            },
        };
        return resp;
    }
    else if (
        (0 == strcmp(p_params->p_url, "https://myserver.com:8000"))
        && (GW_CFG_HTTP_AUTH_TYPE_NONE == p_params->auth_type) && (p_params->use_extra_http_path)
        && (p_params->use_extra_http_query) && (p_params->use_extra_http_headers))
    {
        static const char resp_content[]
            = "{\n"
              "\t\"status\":\t200,\n"
              "\t\"message\":\t\"{}\"\n"
              "}";
        const str_buf_t str_buf = str_buf_printf_with_alloc("%s", resp_content);
        assert(NULL != str_buf.buf);
        const http_server_resp_t resp = {
            .http_resp_code       = HTTP_RESP_CODE_200,
            .content_location     = HTTP_CONTENT_LOCATION_HEAP,
            .flag_no_cache        = true,
            .flag_add_header_date = true,
            .content_type         = HTTP_CONTENT_TYPE_APPLICATION_JSON,
            .p_content_type_param = NULL,
            .content_len          = strlen(str_buf.buf),
            .content_encoding     = HTTP_CONTENT_ENCODING_NONE,
            .select_location = {
                .heap = {
                    .p_buf = (uint8_t*)str_buf.buf,
                },
            },
        };
        return resp;
    }
    else
    {
        const http_server_resp_t resp = {
            .http_resp_code       = HTTP_RESP_CODE_500,
            .content_location     = HTTP_CONTENT_LOCATION_NO_CONTENT,
            .flag_no_cache        = true,
            .flag_add_header_date = true,
            .content_type         = HTTP_CONTENT_TYPE_TEXT_HTML,
            .p_content_type_param = NULL,
            .content_len          = 0,
            .content_encoding     = HTTP_CONTENT_ENCODING_NONE,
        };
        return resp;
    }
}

http_server_resp_t
http_check_post_stat(const http_check_params_t* const p_params, const TimeUnitsSeconds_t timeout_seconds)
{
    const http_server_resp_t resp = {
        .http_resp_code       = HTTP_RESP_CODE_500,
        .content_location     = HTTP_CONTENT_LOCATION_NO_CONTENT,
        .flag_no_cache        = true,
        .flag_add_header_date = true,
        .content_type         = HTTP_CONTENT_TYPE_TEXT_HTML,
        .p_content_type_param = NULL,
        .content_len          = 0,
        .content_encoding     = HTTP_CONTENT_ENCODING_NONE,
    };
    return resp;
}

http_server_resp_t
http_check_mqtt(const ruuvi_gw_cfg_mqtt_t* const p_mqtt_cfg, const TimeUnitsSeconds_t timeout_seconds)
{
    const http_server_resp_t resp = {
        .http_resp_code       = HTTP_RESP_CODE_500,
        .content_location     = HTTP_CONTENT_LOCATION_NO_CONTENT,
        .flag_no_cache        = true,
        .flag_add_header_date = true,
        .content_type         = HTTP_CONTENT_TYPE_TEXT_HTML,
        .p_content_type_param = NULL,
        .content_len          = 0,
        .content_encoding     = HTTP_CONTENT_ENCODING_NONE,
    };
    return resp;
}

bool
runtime_stat_for_each_accumulated_info(
    bool (*p_cb)(const char* const p_task_name, const uint32_t min_free_stack_size, void* p_userdata),
    void* p_userdata)
{
    if (!p_cb("main", 1000, p_userdata))
    {
        return false;
    }
    if (!p_cb("IDLE0", 500, p_userdata))
    {
        return false;
    }
    return true;
}

void
main_task_send_sig_activate_cfg_mode(void)
{
}

void
timer_cfg_mode_deactivation_start(void)
{
}

void
timer_cfg_mode_deactivation_start_with_delay(const TimeUnitsSeconds_t delay_sec)
{
}

bool
gw_cfg_storage_check(void)
{
    return false;
}

bool
gw_cfg_storage_check_file(const char* const p_file_name, const bool is_blob, size_t* const p_file_size)
{
    if (nullptr != p_file_size)
    {
        *p_file_size = 0;
    }
    if ((nullptr != g_extra_cfg_storage.p_file_name) && (0 == strcmp(p_file_name, g_extra_cfg_storage.p_file_name)))
    {
        if (g_extra_cfg_storage.flag_file_exists)
        {
            if (nullptr != p_file_size)
            {
                *p_file_size = g_extra_cfg_storage.file_size;
            }
            return true;
        }
        return false;
    }
    return false;
}

str_buf_t
gw_cfg_storage_read_file_as_string(const char* const p_file_name)
{
    return str_buf_init_null();
}

str_buf_t
gw_cfg_storage_read_file_as_blob(const char* const p_file_name)
{
    if ((nullptr != g_extra_cfg_storage.p_file_name) && (0 == strcmp(p_file_name, g_extra_cfg_storage.p_file_name))
        && (nullptr != g_extra_cfg_storage.p_content))
    {
        const char* const p_content = g_extra_cfg_storage.p_content;
        char*             p_buf     = static_cast<char*>(os_malloc(strlen(p_content) + 1));
        if (nullptr != p_buf)
        {
            strcpy(p_buf, p_content);
            return str_buf_init(p_buf, strlen(p_content) + 1);
        }
    }
    return str_buf_init_null();
}

bool
gw_cfg_storage_write_file_as_string(const char* const p_file_name, const char* const p_content)
{
    return g_extra_cfg_storage.flag_write_success;
}

bool
gw_cfg_storage_write_file_as_blob(const char* const p_file_name, const uint8_t* const p_content, const size_t len)
{
    return g_extra_cfg_storage.flag_write_success;
}

bool
gw_cfg_storage_delete_file(const char* const p_file_name)
{
    return g_extra_cfg_storage.flag_delete_success;
}

bool
partition_table_check_and_update(void)
{
    return true;
}

const char*
wrap_esp_err_to_name_r(const esp_err_t code, char* const p_buf, const size_t buf_len)
{
    (void)snprintf(p_buf, buf_len, "Error 0x%x(%d)", code, code);
    return p_buf;
}

} // extern "C"

class FileInfo
{
public:
    string fileName;
    string content;
    bool   flag_fail_to_open;

    FileInfo(string fileName, string content, const bool flag_fail_to_open)
        : fileName(std::move(fileName))
        , content(std::move(content))
        , flag_fail_to_open(flag_fail_to_open)
    {
    }

    FileInfo(string fileName, string content)
        : FileInfo(std::move(fileName), std::move(content), false)
    {
    }
};

class TestHttpServerCb : public ::testing::Test
{
private:
protected:
    void
    SetUp() override
    {
        os_malloc_trace_init();
        esp_log_wrapper_init();
        g_pTestClass = this;

        cjson_wrap_init();

        g_cnt_cfg_button_pressed = 0;

        this->m_malloc_cnt                        = 0;
        this->m_malloc_fail_on_cnt                = 0;
        this->m_flag_settings_saved_to_flash      = false;
        this->m_flag_settings_ethernet_ip_updated = false;

        const gw_cfg_default_init_param_t init_params = {
            .wifi_ap_ssid        = { "my_ssid1" },
            .hostname            = { "my_ssid1" },
            .device_id           = { 0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77 },
            .esp32_fw_ver        = { "v1.3.3" },
            .nrf52_fw_ver        = { "v0.7.1" },
            .nrf52_mac_addr      = { 0x11, 0x22, 0x33, 0x44, 0x55, 0x66 },
            .esp32_mac_addr_wifi = { 0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0x11 },
            .esp32_mac_addr_eth  = { 0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0x22 },
        };
        gw_cfg_default_init(&init_params, nullptr);
        gw_cfg_init(&settings_save_to_flash);
        esp_log_wrapper_clear();
        this->m_malloc_cnt = 0;
        this->fw_update_url.fill(0);
        this->m_http_check_with_auth_num_of_urls = 0;
        this->m_http_check_with_auth_arr_of_urls = nullptr;
        this->m_firmware_update_resp             = str_buf_init_null();
        this->m_fw_updating_reason               = FW_UPDATE_REASON_NONE;
        this->m_fw_update_is_in_progress         = false;

        this->m_alloc_free_call_count       = 0;
        this->m_flag_alloc_counting_enabled = true;

        this->m_mock_async_resp_set = false;
        memset(&this->m_mock_async_resp, 0, sizeof(this->m_mock_async_resp));
        this->m_mock_esp_http_client_cleanup_called   = false;
        this->m_mock_http_async_info_free_data_called = false;
        this->m_mock_http_download_with_auth          = nullptr;

        this->m_time_is_synchronized  = true;
        this->m_request_timestamp     = 0;
        this->m_settimeofday_call_cnt = 0;
        this->m_settimeofday_tv       = { 0, 0 };
        this->m_settimeofday_fail     = false;

        memset(&g_extra_cfg_storage, 0, sizeof(g_extra_cfg_storage));
    }

    void
    TearDown() override
    {
        this->m_flag_alloc_counting_enabled = false;
        this->m_alloc_free_call_count       = 0;
        http_server_cb_deinit();
        gw_cfg_deinit();
        gw_cfg_default_deinit();
        this->m_files.clear();
        this->m_fd   = -1;
        g_pTestClass = nullptr;
        os_malloc_trace_deinit();
        esp_log_wrapper_deinit();
    }

public:
    TestHttpServerCb();

    ~TestHttpServerCb() override;

    uint32_t              m_malloc_cnt;
    uint32_t              m_malloc_fail_on_cnt;
    bool                  m_flag_settings_saved_to_flash;
    bool                  m_flag_settings_ethernet_ip_updated;
    bool                  m_flag_is_ap_active;
    flash_fat_fs_t        m_fatfs;
    bool                  m_is_fatfs_mounted;
    bool                  m_is_fatfs_mount_fail;
    vector<FileInfo>      m_files;
    file_descriptor_t     m_fd;
    std::array<char, 128> fw_update_url;
    str_buf_t             m_firmware_update_resp;
    const char**          m_http_check_with_auth_arr_of_urls;
    size_t                m_http_check_with_auth_num_of_urls;
    fw_updating_reason_e  m_fw_updating_reason;
    bool                  m_fw_update_is_in_progress;

    bool m_flag_alloc_counting_enabled;
    int  m_alloc_free_call_count;

    http_server_resp_t m_mock_async_resp;
    bool               m_mock_async_resp_set;
    bool               m_mock_esp_http_client_cleanup_called;
    bool               m_mock_http_async_info_free_data_called;

    // Mock for http_download_with_auth used by http_download_json tests
    using http_download_mock_cb_t = std::function<http_server_resp_t(
        const http_download_param_with_auth_t* p_param,
        http_download_cb_on_data_t             p_cb_on_data,
        void*                                  p_user_data,
        bool                                   flag_use_big_tls_buf)>;
    http_download_mock_cb_t m_mock_http_download_with_auth;

    // Mocks for the new firmware-update time-seeding logic
    bool           m_time_is_synchronized;
    time_t         m_request_timestamp;
    uint32_t       m_settimeofday_call_cnt;
    struct timeval m_settimeofday_tv;
    bool           m_settimeofday_fail;
};

TestHttpServerCb::TestHttpServerCb()
    : m_malloc_cnt(0)
    , m_malloc_fail_on_cnt(0)
    , m_flag_settings_saved_to_flash(false)
    , m_flag_settings_ethernet_ip_updated(false)
    , m_flag_is_ap_active(false)
    , m_is_fatfs_mounted(false)
    , m_is_fatfs_mount_fail(false)
    , m_fd(-1)
    , m_flag_alloc_counting_enabled(false)
    , m_alloc_free_call_count(0)
    , m_mock_async_resp()
    , m_mock_async_resp_set(false)
    , m_mock_esp_http_client_cleanup_called(false)
    , m_mock_http_async_info_free_data_called(false)
    , Test()
{
}

extern "C" {

void*
__wrap_malloc(size_t size)
{
    extern void* __real_malloc(size_t _size) __THROW __attribute_malloc__ __attribute_alloc_size__((1)) __wur;
    extern void                                                           __real_free(void* _ptr) __THROW;

    void* ptr = __real_malloc(size);
    assert(nullptr != ptr);
    if (nullptr == ptr)
    {
        return nullptr;
    }
    if (g_pTestClass && g_pTestClass->m_malloc_fail_on_cnt > 0)
    {
        g_pTestClass->m_malloc_cnt += 1;
        if (g_pTestClass->m_malloc_cnt == g_pTestClass->m_malloc_fail_on_cnt)
        {
            __real_free(ptr);
            return nullptr;
        }
    }
    if (g_pTestClass && g_pTestClass->m_flag_alloc_counting_enabled)
    {
        g_pTestClass->m_alloc_free_call_count += 1;
    }
    return ptr;
}

void*
__wrap_calloc(size_t nmemb, size_t size)
{
    extern void*                     __real_calloc(size_t _nmemb, size_t _size)
        __THROW __attribute_malloc__ __attribute_alloc_size__((1, 2)) __wur;
    extern void                      __real_free(void* _ptr) __THROW;

    void* ptr = __real_calloc(nmemb, size);
    assert(nullptr != ptr);
    if (nullptr == ptr)
    {
        return nullptr;
    }
    if (g_pTestClass && g_pTestClass->m_malloc_fail_on_cnt > 0)
    {
        g_pTestClass->m_malloc_cnt += 1;
        if (g_pTestClass->m_malloc_cnt == g_pTestClass->m_malloc_fail_on_cnt)
        {
            __real_free(ptr);
            return nullptr;
        }
    }
    if (g_pTestClass && g_pTestClass->m_flag_alloc_counting_enabled)
    {
        g_pTestClass->m_alloc_free_call_count += 1;
    }
    return ptr;
}

void
__wrap_free(void* ptr)
{
    extern void __real_free(void* _ptr) __THROW;

    __real_free(ptr);
    if (nullptr == ptr)
    {
        return;
    }
    if (g_pTestClass && g_pTestClass->m_flag_alloc_counting_enabled)
    {
        g_pTestClass->m_alloc_free_call_count -= 1;
    }
}

int
__wrap_settimeofday(const struct timeval* tv, const struct timezone* tz)
{
    (void)tz;
    if (nullptr != g_pTestClass)
    {
        g_pTestClass->m_settimeofday_call_cnt += 1;
        if (nullptr != tv)
        {
            g_pTestClass->m_settimeofday_tv = *tv;
        }
        if (g_pTestClass->m_settimeofday_fail)
        {
            errno = EPERM;
            return -1;
        }
    }
    return 0;
}

bool
time_is_synchronized(void)
{
    return (nullptr != g_pTestClass) ? g_pTestClass->m_time_is_synchronized : true;
}

time_t
http_server_get_request_timestamp(void)
{
    return (nullptr != g_pTestClass) ? g_pTestClass->m_request_timestamp : 0;
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
    return reinterpret_cast<os_mutex_t>(p_mutex_static);
}

void
os_mutex_delete(os_mutex_t* const ph_mutex)
{
    *ph_mutex = NULL;
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

void
event_mgr_notify(const event_mgr_ev_e event)
{
}

char*
esp_ip4addr_ntoa(const esp_ip4_addr_t* addr, char* buf, int buflen)
{
    return ip4addr_ntoa_r((ip4_addr_t*)addr, buf, buflen);
}

uint32_t
esp_ip4addr_aton(const char* addr)
{
    return ipaddr_addr(addr);
}

void
wifi_manager_set_config_ap(const wifiman_config_ap_t* const p_wifi_cfg_ap)
{
}

void
wifi_manager_cb_save_wifi_config_sta(const wifiman_config_sta_t* const p_cfg_sta)
{
}

bool
wifi_manager_is_ap_active(void)
{
    return g_pTestClass->m_flag_is_ap_active;
}

char*
metrics_generate(void)
{
    const char* p_metrics_str = "metrics_info";
    char*       p_buf         = static_cast<char*>(os_malloc(strlen(p_metrics_str) + 1));
    if (nullptr != p_buf)
    {
        strcpy(p_buf, p_metrics_str);
    }
    return p_buf;
}

time_t
http_server_get_cur_time(void)
{
    return 1615660220;
}

TickType_t
xTaskGetTickCount(void)
{
    return 0;
}

void
adv_table_history_read(
    adv_report_table_t* const p_reports,
    const time_t              cur_time,
    const bool                flag_use_timestamps,
    const uint32_t            filter,
    const bool                flag_use_filter)
{
    (void)flag_use_timestamps;
    (void)filter;
    (void)flag_use_filter;
    p_reports->num_of_advs = 2;
    {
        adv_report_t* const p_adv   = &p_reports->table[0];
        p_adv->timestamp            = cur_time - 1;
        const mac_address_bin_t mac = { 0xAAU, 0xBBU, 0xCCU, 0x11U, 0x22U, 0x01U };
        p_adv->tag_mac              = mac;
        p_adv->rssi                 = 50;
        p_adv->primary_phy          = RE_CA_UART_BLE_PHY_1MBPS;
        p_adv->secondary_phy        = RE_CA_UART_BLE_PHY_NOT_SET;
        p_adv->ch_index             = 37;
        p_adv->is_coded_phy         = false;
        p_adv->tx_power             = RE_CA_UART_BLE_GAP_POWER_LEVEL_INVALID;
        const uint8_t data_buf[]    = { 0x22U, 0x33U };
        p_adv->data_len             = sizeof(data_buf);
        memcpy(p_adv->data_buf, data_buf, sizeof(data_buf));
    }
    {
        adv_report_t* const p_adv   = &p_reports->table[1];
        p_adv->timestamp            = cur_time - 11;
        const mac_address_bin_t mac = { 0xAAU, 0xBBU, 0xCCU, 0x11U, 0x22U, 0x02U };
        p_adv->tag_mac              = mac;
        p_adv->rssi                 = 51;
        p_adv->primary_phy          = RE_CA_UART_BLE_PHY_1MBPS;
        p_adv->secondary_phy        = RE_CA_UART_BLE_PHY_2MBPS;
        p_adv->ch_index             = 25;
        p_adv->is_coded_phy         = false;
        p_adv->tx_power             = 8;
        const uint8_t data_buf[]    = { 0x22U, 0x33U, 0x44U };
        p_adv->data_len             = sizeof(data_buf);
        memcpy(p_adv->data_buf, data_buf, sizeof(data_buf));
    }
}

void
settings_save_to_flash(const gw_cfg_t* const p_gw_cfg)
{
    g_pTestClass->m_flag_settings_saved_to_flash = true;
}

void
ethernet_update_ip(void)
{
    g_pTestClass->m_flag_settings_ethernet_ip_updated = true;
}

const flash_fat_fs_t*
flashfatfs_mount(const char* mount_point, const char* partition_label, const flash_fat_fs_num_files_t max_files)
{
    assert(!g_pTestClass->m_is_fatfs_mounted);
    if (g_pTestClass->m_is_fatfs_mount_fail)
    {
        return nullptr;
    }
    g_pTestClass->m_fatfs.mount_point.assign(mount_point);
    g_pTestClass->m_fatfs.partition_label.assign(partition_label);
    g_pTestClass->m_fatfs.max_files  = max_files;
    g_pTestClass->m_is_fatfs_mounted = true;
    return &g_pTestClass->m_fatfs;
}

bool
flashfatfs_unmount(const flash_fat_fs_t** pp_ffs)
{
    assert(nullptr != pp_ffs);
    assert(*pp_ffs == &g_pTestClass->m_fatfs);
    assert(g_pTestClass->m_is_fatfs_mounted);
    g_pTestClass->m_is_fatfs_mounted = false;
    g_pTestClass->m_fatfs.mount_point.assign("");
    g_pTestClass->m_fatfs.partition_label.assign("");
    g_pTestClass->m_fatfs.max_files = 0;
    *pp_ffs                         = nullptr;
    return true;
}

bool
flashfatfs_get_file_size(const flash_fat_fs_t* p_ffs, const char* file_path, size_t* p_size)
{
    assert(p_ffs == &g_pTestClass->m_fatfs);
    for (auto& file : g_pTestClass->m_files)
    {
        if (0 == strcmp(file_path, file.fileName.c_str()))
        {
            *p_size = file.content.size();
            return true;
        }
    }
    *p_size = 0;
    return false;
}

file_descriptor_t
flashfatfs_open(const flash_fat_fs_t* p_ffs, const char* file_path)
{
    assert(p_ffs == &g_pTestClass->m_fatfs);
    for (auto& file : g_pTestClass->m_files)
    {
        if (0 == strcmp(file_path, file.fileName.c_str()))
        {
            if (file.flag_fail_to_open)
            {
                return (file_descriptor_t)-1;
            }
            return g_pTestClass->m_fd;
        }
    }
    return (file_descriptor_t)-1;
}

void
fw_update_set_binaries_url(const char* const p_url_fmt, ...)
{
    va_list ap;
    va_start(ap, p_url_fmt);
    (void)vsnprintf(g_pTestClass->fw_update_url.data(), g_pTestClass->fw_update_url.size(), p_url_fmt, ap);
    va_end(ap);
}

const char*
fw_update_get_binaries_url(void)
{
    return g_pTestClass->fw_update_url.data();
}

esp_err_t
esp_task_wdt_reset(void)
{
    return ESP_OK;
}

http_server_resp_t
http_download_with_auth(
    const http_download_param_with_auth_t* const p_param,
    http_download_cb_on_data_t const             p_cb_on_data,
    void* const                                  p_user_data,
    const bool                                   flag_use_big_tls_buf)
{
    if (g_pTestClass->m_mock_http_download_with_auth)
    {
        return g_pTestClass->m_mock_http_download_with_auth(p_param, p_cb_on_data, p_user_data, flag_use_big_tls_buf);
    }
    if (0 == strcmp(p_param->base.p_url, "https://network.ruuvi.com/firmwareupdate"))
    {
        // Request for json always performed from the beginning (partial content is not supported)
        constexpr size_t range_start = 0;
        p_cb_on_data(
            (const uint8_t*)g_pTestClass->m_firmware_update_resp.buf,
            strlen(g_pTestClass->m_firmware_update_resp.buf),
            0,
            0,
            HTTP_RESP_CODE_200,
            range_start,
            p_user_data);
        return http_server_resp_200_json_in_heap(g_pTestClass->m_firmware_update_resp.buf);
    }
    return http_server_resp_400();
}

bool
http_check_with_auth(const http_download_param_with_auth_t* const p_param, http_resp_code_e* const p_http_resp_code)
{
    for (size_t i = 0; i < g_pTestClass->m_http_check_with_auth_num_of_urls; ++i)
    {
        if (0 == strcmp(p_param->base.p_url, g_pTestClass->m_http_check_with_auth_arr_of_urls[i]))
        {
            *p_http_resp_code = HTTP_RESP_CODE_200;
            return true;
        }
    }
    *p_http_resp_code = HTTP_RESP_CODE_404;
    return false;
}

bool
fw_update_run(const fw_updating_reason_e fw_updating_reason)
{
    g_pTestClass->m_fw_updating_reason = fw_updating_reason;
    return true;
}

void
esp_transport_ssl_clear_saved_session_tickets(void)
{
}

void
adv_post_nrf52_cfg_update(const ruuvi_gw_cfg_scan_t* const p_scan, const ruuvi_gw_cfg_filter_t* const p_filter)
{
}

bool
ruuvi_gw_fw_update_is_in_progress(void)
{
    return g_pTestClass->m_fw_update_is_in_progress;
}

http_server_resp_t
http_wait_until_async_req_completed(
    esp_http_client_handle_t   p_http_handle,
    http_resp_cb_info_t* const p_cb_info,
    const bool                 flag_feed_task_watchdog,
    const TimeUnitsSeconds_t   timeout_seconds)
{
    (void)p_http_handle;
    (void)p_cb_info;
    (void)flag_feed_task_watchdog;
    (void)timeout_seconds;
    if (g_pTestClass->m_mock_async_resp_set)
    {
        return g_pTestClass->m_mock_async_resp;
    }
    const http_server_resp_t resp = {
        .http_resp_code       = HTTP_RESP_CODE_200,
        .content_location     = HTTP_CONTENT_LOCATION_NO_CONTENT,
        .flag_no_cache        = true,
        .flag_add_header_date = true,
        .content_type         = HTTP_CONTENT_TYPE_TEXT_HTML,
        .p_content_type_param = NULL,
        .content_len          = 0,
        .content_encoding     = HTTP_CONTENT_ENCODING_NONE,
    };
    return resp;
}

esp_err_t
esp_http_client_cleanup(esp_http_client_handle_t client)
{
    (void)client;
    g_pTestClass->m_mock_esp_http_client_cleanup_called = true;
    return ESP_OK;
}

void
http_async_info_free_data(http_async_info_t* const p_http_async_info)
{
    (void)p_http_async_info;
    g_pTestClass->m_mock_http_async_info_free_data_called = true;
}

} // extern "C"

TestHttpServerCb::~TestHttpServerCb() = default;

#define TEST_CHECK_LOG_RECORD_HTTP_SERVER(level_, msg_) \
    ESP_LOG_WRAPPER_TEST_CHECK_LOG_RECORD("http_server", level_, msg_)

#define TEST_CHECK_LOG_RECORD_HTTP_DOWNLOAD(level_, msg_) \
    ESP_LOG_WRAPPER_TEST_CHECK_LOG_RECORD("http_download", level_, msg_)

#define TEST_CHECK_LOG_RECORD_GW_CFG(level_, msg_) ESP_LOG_WRAPPER_TEST_CHECK_LOG_RECORD("gw_cfg", level_, msg_)

#define TEST_CHECK_LOG_RECORD_TEST(level_, msg_) ESP_LOG_WRAPPER_TEST_CHECK_LOG_RECORD("test", level_, msg_)

static gw_cfg_t
get_gateway_config_default()
{
    gw_cfg_t gw_cfg {};
    gw_cfg_default_get(&gw_cfg);
    return gw_cfg;
}

/*** Unit-Tests
 * *******************************************************************************************************/

TEST_F(TestHttpServerCb, http_server_cb_init_ok_deinit_ok) // NOLINT
{
    ASSERT_FALSE(g_pTestClass->m_is_fatfs_mounted);
    ASSERT_TRUE(http_server_cb_init(GW_GWUI_PARTITION));
    ASSERT_TRUE(g_pTestClass->m_is_fatfs_mounted);
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    http_server_cb_deinit();
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_FALSE(g_pTestClass->m_is_fatfs_mounted);
    os_malloc_trace_dump();
    ESP_LOG_WRAPPER_TEST_CHECK_LOG_RECORD("MEM_TRACE", ESP_LOG_INFO, "Num blocks allocated: 0");
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_EQ(0, this->m_alloc_free_call_count);
}

TEST_F(TestHttpServerCb, http_server_cb_gen_resp_ok) // NOLINT
{
    esp_log_wrapper_clear();
    http_server_resp_t resp = http_server_cb_gen_resp(HTTP_RESP_CODE_200, "%s", "OK");

    ASSERT_EQ(HTTP_RESP_CODE_200, resp.http_resp_code);
    ASSERT_EQ(HTTP_CONTENT_LOCATION_HEAP, resp.content_location);
    ASSERT_TRUE(resp.flag_no_cache);
    ASSERT_EQ(HTTP_CONTENT_TYPE_APPLICATION_JSON, resp.content_type);
    ASSERT_EQ(nullptr, resp.p_content_type_param);
    ASSERT_NE(nullptr, resp.select_location.heap.p_buf);

    const char* expected_json
        = "{\n"
          "\t\"status\":\t200,\n"
          "\t\"message\":\t\"OK\"\n"
          "}";
    ASSERT_EQ(string(expected_json), string(reinterpret_cast<const char*>(resp.select_location.heap.p_buf)));
    ASSERT_EQ(strlen(expected_json), resp.content_len);
    ASSERT_EQ(HTTP_CONTENT_ENCODING_NONE, resp.content_encoding);

    http_server_resp_free(&resp);
    os_malloc_trace_dump();
    ESP_LOG_WRAPPER_TEST_CHECK_LOG_RECORD("MEM_TRACE", ESP_LOG_INFO, "Num blocks allocated: 0");
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_EQ(0, this->m_alloc_free_call_count);
}

TEST_F(TestHttpServerCb, http_server_cb_gen_resp_with_error_code) // NOLINT
{
    esp_log_wrapper_clear();
    http_server_resp_t resp = http_server_cb_gen_resp(HTTP_RESP_CODE_502, "%s", "Bad Gateway");

    ASSERT_EQ(HTTP_RESP_CODE_200, resp.http_resp_code);
    ASSERT_EQ(HTTP_CONTENT_LOCATION_HEAP, resp.content_location);
    ASSERT_TRUE(resp.flag_no_cache);
    ASSERT_EQ(HTTP_CONTENT_TYPE_APPLICATION_JSON, resp.content_type);
    ASSERT_EQ(nullptr, resp.p_content_type_param);
    ASSERT_NE(nullptr, resp.select_location.heap.p_buf);

    const char* expected_json
        = "{\n"
          "\t\"status\":\t502,\n"
          "\t\"message\":\t\"Bad Gateway\"\n"
          "}";
    ASSERT_EQ(string(expected_json), string(reinterpret_cast<const char*>(resp.select_location.heap.p_buf)));
    ASSERT_EQ(strlen(expected_json), resp.content_len);
    ASSERT_EQ(HTTP_CONTENT_ENCODING_NONE, resp.content_encoding);

    http_server_resp_free(&resp);
    os_malloc_trace_dump();
    ESP_LOG_WRAPPER_TEST_CHECK_LOG_RECORD("MEM_TRACE", ESP_LOG_INFO, "Num blocks allocated: 0");
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_EQ(0, this->m_alloc_free_call_count);
}

TEST_F(TestHttpServerCb, http_server_cb_gen_resp_with_formatted_message) // NOLINT
{
    esp_log_wrapper_clear();
    http_server_resp_t resp = http_server_cb_gen_resp(HTTP_RESP_CODE_400, "Incorrect URL: '%s'", "http://bad");

    ASSERT_EQ(HTTP_RESP_CODE_200, resp.http_resp_code);
    ASSERT_EQ(HTTP_CONTENT_LOCATION_HEAP, resp.content_location);
    ASSERT_NE(nullptr, resp.select_location.heap.p_buf);

    const char* expected_json
        = "{\n"
          "\t\"status\":\t400,\n"
          "\t\"message\":\t\"Incorrect URL: 'http://bad'\"\n"
          "}";
    ASSERT_EQ(string(expected_json), string(reinterpret_cast<const char*>(resp.select_location.heap.p_buf)));
    ASSERT_EQ(strlen(expected_json), resp.content_len);

    http_server_resp_free(&resp);
    os_malloc_trace_dump();
    ESP_LOG_WRAPPER_TEST_CHECK_LOG_RECORD("MEM_TRACE", ESP_LOG_INFO, "Num blocks allocated: 0");
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_EQ(0, this->m_alloc_free_call_count);
}

TEST_F(TestHttpServerCb, http_server_cb_gen_resp_with_empty_message) // NOLINT
{
    esp_log_wrapper_clear();
    http_server_resp_t resp = http_server_cb_gen_resp(HTTP_RESP_CODE_200, "%s", "");

    ASSERT_EQ(HTTP_RESP_CODE_200, resp.http_resp_code);
    ASSERT_EQ(HTTP_CONTENT_LOCATION_HEAP, resp.content_location);
    ASSERT_NE(nullptr, resp.select_location.heap.p_buf);

    const char* expected_json
        = "{\n"
          "\t\"status\":\t200,\n"
          "\t\"message\":\t\"\"\n"
          "}";
    ASSERT_EQ(string(expected_json), string(reinterpret_cast<const char*>(resp.select_location.heap.p_buf)));
    ASSERT_EQ(strlen(expected_json), resp.content_len);

    http_server_resp_free(&resp);
    os_malloc_trace_dump();
    ESP_LOG_WRAPPER_TEST_CHECK_LOG_RECORD("MEM_TRACE", ESP_LOG_INFO, "Num blocks allocated: 0");
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_EQ(0, this->m_alloc_free_call_count);
}

TEST_F(TestHttpServerCb, http_server_cb_gen_resp_malloc_fail_on_str_buf) // NOLINT
{
    g_pTestClass->m_malloc_fail_on_cnt = 1;

    esp_log_wrapper_clear();
    const http_server_resp_t resp = http_server_cb_gen_resp(HTTP_RESP_CODE_200, "%s", "OK");

    ASSERT_EQ(HTTP_RESP_CODE_500, resp.http_resp_code);
    ASSERT_EQ(HTTP_CONTENT_LOCATION_NO_CONTENT, resp.content_location);
    ASSERT_TRUE(resp.flag_no_cache);
    ASSERT_EQ(HTTP_CONTENT_TYPE_TEXT_HTML, resp.content_type);
    ASSERT_EQ(nullptr, resp.p_content_type_param);
    ASSERT_EQ(0, resp.content_len);
    ASSERT_EQ(HTTP_CONTENT_ENCODING_NONE, resp.content_encoding);
    TEST_CHECK_LOG_RECORD_HTTP_SERVER(ESP_LOG_ERROR, "Can't allocate memory for response");
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    os_malloc_trace_dump();
    ESP_LOG_WRAPPER_TEST_CHECK_LOG_RECORD("MEM_TRACE", ESP_LOG_INFO, "Num blocks allocated: 0");
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_EQ(0, this->m_alloc_free_call_count);
}

TEST_F(TestHttpServerCb, http_server_cb_gen_resp_malloc_fail_on_cjson_create) // NOLINT
{
    g_pTestClass->m_malloc_fail_on_cnt = 2;

    esp_log_wrapper_clear();
    const http_server_resp_t resp = http_server_cb_gen_resp(HTTP_RESP_CODE_200, "%s", "OK");

    ASSERT_EQ(HTTP_RESP_CODE_500, resp.http_resp_code);
    ASSERT_EQ(HTTP_CONTENT_LOCATION_NO_CONTENT, resp.content_location);
    ASSERT_TRUE(resp.flag_no_cache);
    ASSERT_EQ(HTTP_CONTENT_TYPE_TEXT_HTML, resp.content_type);
    ASSERT_EQ(nullptr, resp.p_content_type_param);
    ASSERT_EQ(0, resp.content_len);
    ASSERT_EQ(HTTP_CONTENT_ENCODING_NONE, resp.content_encoding);
    TEST_CHECK_LOG_RECORD_HTTP_SERVER(ESP_LOG_ERROR, "Can't create json object");
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    os_malloc_trace_dump();
    ESP_LOG_WRAPPER_TEST_CHECK_LOG_RECORD("MEM_TRACE", ESP_LOG_INFO, "Num blocks allocated: 0");
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_EQ(0, this->m_alloc_free_call_count);
}

TEST_F(TestHttpServerCb, http_server_cb_gen_resp_malloc_fail_on_add_status) // NOLINT
{
    g_pTestClass->m_malloc_fail_on_cnt = 3;

    esp_log_wrapper_clear();
    const http_server_resp_t resp = http_server_cb_gen_resp(HTTP_RESP_CODE_200, "%s", "OK");

    ASSERT_EQ(HTTP_RESP_CODE_500, resp.http_resp_code);
    ASSERT_EQ(HTTP_CONTENT_LOCATION_NO_CONTENT, resp.content_location);
    ASSERT_TRUE(resp.flag_no_cache);
    ASSERT_EQ(HTTP_CONTENT_TYPE_TEXT_HTML, resp.content_type);
    ASSERT_EQ(nullptr, resp.p_content_type_param);
    ASSERT_EQ(0, resp.content_len);
    ASSERT_EQ(HTTP_CONTENT_ENCODING_NONE, resp.content_encoding);
    TEST_CHECK_LOG_RECORD_HTTP_SERVER(ESP_LOG_ERROR, "Can't add json item: status");
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    os_malloc_trace_dump();
    ESP_LOG_WRAPPER_TEST_CHECK_LOG_RECORD("MEM_TRACE", ESP_LOG_INFO, "Num blocks allocated: 0");
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_EQ(0, this->m_alloc_free_call_count);
}

TEST_F(TestHttpServerCb, http_server_cb_gen_resp_malloc_fail_on_add_message) // NOLINT
{
    g_pTestClass->m_malloc_fail_on_cnt = 5;

    esp_log_wrapper_clear();
    const http_server_resp_t resp = http_server_cb_gen_resp(HTTP_RESP_CODE_200, "%s", "OK");

    ASSERT_EQ(HTTP_RESP_CODE_500, resp.http_resp_code);
    ASSERT_EQ(HTTP_CONTENT_LOCATION_NO_CONTENT, resp.content_location);
    ASSERT_TRUE(resp.flag_no_cache);
    ASSERT_EQ(HTTP_CONTENT_TYPE_TEXT_HTML, resp.content_type);
    ASSERT_EQ(nullptr, resp.p_content_type_param);
    ASSERT_EQ(0, resp.content_len);
    ASSERT_EQ(HTTP_CONTENT_ENCODING_NONE, resp.content_encoding);
    TEST_CHECK_LOG_RECORD_HTTP_SERVER(ESP_LOG_ERROR, "Can't add json item: message");
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    os_malloc_trace_dump();
    ESP_LOG_WRAPPER_TEST_CHECK_LOG_RECORD("MEM_TRACE", ESP_LOG_INFO, "Num blocks allocated: 0");
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_EQ(0, this->m_alloc_free_call_count);
}

TEST_F(TestHttpServerCb, http_server_cb_gen_resp_json_ok) // NOLINT
{
    esp_log_wrapper_clear();
    http_server_resp_t resp = http_server_cb_gen_resp_json(HTTP_RESP_CODE_200, "{\"key\":\"val\"}");

    ASSERT_EQ(HTTP_RESP_CODE_200, resp.http_resp_code);
    ASSERT_EQ(HTTP_CONTENT_LOCATION_HEAP, resp.content_location);
    ASSERT_TRUE(resp.flag_no_cache);
    ASSERT_EQ(HTTP_CONTENT_TYPE_APPLICATION_JSON, resp.content_type);
    ASSERT_EQ(nullptr, resp.p_content_type_param);
    ASSERT_NE(nullptr, resp.select_location.heap.p_buf);

    const char* expected = "{\"status\": 200, \"json\": {\"key\":\"val\"}}";
    ASSERT_EQ(string(expected), string(reinterpret_cast<const char*>(resp.select_location.heap.p_buf)));
    ASSERT_EQ(strlen(expected), resp.content_len);
    ASSERT_EQ(HTTP_CONTENT_ENCODING_NONE, resp.content_encoding);

    http_server_resp_free(&resp);
    os_malloc_trace_dump();
    ESP_LOG_WRAPPER_TEST_CHECK_LOG_RECORD("MEM_TRACE", ESP_LOG_INFO, "Num blocks allocated: 0");
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_EQ(0, this->m_alloc_free_call_count);
}

TEST_F(TestHttpServerCb, http_server_cb_gen_resp_json_with_error_code) // NOLINT
{
    esp_log_wrapper_clear();
    http_server_resp_t resp = http_server_cb_gen_resp_json(HTTP_RESP_CODE_502, "{\"err\":true}");

    ASSERT_EQ(HTTP_RESP_CODE_200, resp.http_resp_code);
    ASSERT_EQ(HTTP_CONTENT_LOCATION_HEAP, resp.content_location);
    ASSERT_NE(nullptr, resp.select_location.heap.p_buf);

    const char* expected = "{\"status\": 502, \"json\": {\"err\":true}}";
    ASSERT_EQ(string(expected), string(reinterpret_cast<const char*>(resp.select_location.heap.p_buf)));
    ASSERT_EQ(strlen(expected), resp.content_len);

    http_server_resp_free(&resp);
    os_malloc_trace_dump();
    ESP_LOG_WRAPPER_TEST_CHECK_LOG_RECORD("MEM_TRACE", ESP_LOG_INFO, "Num blocks allocated: 0");
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_EQ(0, this->m_alloc_free_call_count);
}

TEST_F(TestHttpServerCb, http_server_cb_gen_resp_json_malloc_fail) // NOLINT
{
    g_pTestClass->m_malloc_fail_on_cnt = 1;

    esp_log_wrapper_clear();
    const http_server_resp_t resp = http_server_cb_gen_resp_json(HTTP_RESP_CODE_200, "{\"key\":\"val\"}");

    ASSERT_EQ(HTTP_RESP_CODE_500, resp.http_resp_code);
    ASSERT_EQ(HTTP_CONTENT_LOCATION_NO_CONTENT, resp.content_location);
    ASSERT_TRUE(resp.flag_no_cache);
    ASSERT_EQ(HTTP_CONTENT_TYPE_TEXT_HTML, resp.content_type);
    ASSERT_EQ(nullptr, resp.p_content_type_param);
    ASSERT_EQ(0, resp.content_len);
    ASSERT_EQ(HTTP_CONTENT_ENCODING_NONE, resp.content_encoding);
    TEST_CHECK_LOG_RECORD_HTTP_SERVER(ESP_LOG_ERROR, "Can't allocate memory for response");
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    os_malloc_trace_dump();
    ESP_LOG_WRAPPER_TEST_CHECK_LOG_RECORD("MEM_TRACE", ESP_LOG_INFO, "Num blocks allocated: 0");
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_EQ(0, this->m_alloc_free_call_count);
}

TEST_F(TestHttpServerCb, http_server_get_from_params_ok_single_param) // NOLINT
{
    esp_log_wrapper_clear();
    str_buf_t result = http_server_get_from_params("key=value123", "key=");

    ASSERT_NE(nullptr, result.buf);
    ASSERT_EQ(string("value123"), string(result.buf));
    TEST_CHECK_LOG_RECORD_HTTP_SERVER(ESP_LOG_DEBUG, "HTTP params: key=value123");
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    str_buf_free_buf(&result);
    os_malloc_trace_dump();
    ESP_LOG_WRAPPER_TEST_CHECK_LOG_RECORD("MEM_TRACE", ESP_LOG_INFO, "Num blocks allocated: 0");
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_EQ(0, this->m_alloc_free_call_count);
}

TEST_F(TestHttpServerCb, http_server_get_from_params_ok_multiple_params) // NOLINT
{
    esp_log_wrapper_clear();
    str_buf_t result = http_server_get_from_params("first=aaa&key=value456&other=zzz", "key=");

    ASSERT_NE(nullptr, result.buf);
    ASSERT_EQ(string("value456"), string(result.buf));
    TEST_CHECK_LOG_RECORD_HTTP_SERVER(ESP_LOG_DEBUG, "HTTP params: key=value456");
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    str_buf_free_buf(&result);
    os_malloc_trace_dump();
    ESP_LOG_WRAPPER_TEST_CHECK_LOG_RECORD("MEM_TRACE", ESP_LOG_INFO, "Num blocks allocated: 0");
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_EQ(0, this->m_alloc_free_call_count);
}

TEST_F(TestHttpServerCb, http_server_get_from_params_ok_last_param) // NOLINT
{
    esp_log_wrapper_clear();
    str_buf_t result = http_server_get_from_params("first=aaa&key=lastval", "key=");

    ASSERT_NE(nullptr, result.buf);
    ASSERT_EQ(string("lastval"), string(result.buf));
    TEST_CHECK_LOG_RECORD_HTTP_SERVER(ESP_LOG_DEBUG, "HTTP params: key=lastval");
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    str_buf_free_buf(&result);
    os_malloc_trace_dump();
    ESP_LOG_WRAPPER_TEST_CHECK_LOG_RECORD("MEM_TRACE", ESP_LOG_INFO, "Num blocks allocated: 0");
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_EQ(0, this->m_alloc_free_call_count);
}

TEST_F(TestHttpServerCb, http_server_get_from_params_ok_empty_value) // NOLINT
{
    esp_log_wrapper_clear();
    str_buf_t result = http_server_get_from_params("key=&other=val", "key=");

    ASSERT_NE(nullptr, result.buf);
    ASSERT_EQ(string(""), string(result.buf));
    TEST_CHECK_LOG_RECORD_HTTP_SERVER(ESP_LOG_DEBUG, "HTTP params: key=");
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    str_buf_free_buf(&result);
    os_malloc_trace_dump();
    ESP_LOG_WRAPPER_TEST_CHECK_LOG_RECORD("MEM_TRACE", ESP_LOG_INFO, "Num blocks allocated: 0");
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_EQ(0, this->m_alloc_free_call_count);
}

TEST_F(TestHttpServerCb, http_server_get_from_params_key_not_found) // NOLINT
{
    esp_log_wrapper_clear();
    str_buf_t result = http_server_get_from_params("other=value", "key=");

    ASSERT_EQ(nullptr, result.buf);
    TEST_CHECK_LOG_RECORD_HTTP_SERVER(ESP_LOG_DEBUG, "Can't find key 'key=' in URL params");
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    os_malloc_trace_dump();
    ESP_LOG_WRAPPER_TEST_CHECK_LOG_RECORD("MEM_TRACE", ESP_LOG_INFO, "Num blocks allocated: 0");
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_EQ(0, this->m_alloc_free_call_count);
}

TEST_F(TestHttpServerCb, http_server_get_from_params_malloc_fail) // NOLINT
{
    g_pTestClass->m_malloc_fail_on_cnt = 1;

    esp_log_wrapper_clear();
    str_buf_t result = http_server_get_from_params("key=value123", "key=");

    ASSERT_EQ(nullptr, result.buf);
    TEST_CHECK_LOG_RECORD_HTTP_SERVER(ESP_LOG_DEBUG, "HTTP params: key=value123");
    TEST_CHECK_LOG_RECORD_HTTP_SERVER(ESP_LOG_ERROR, "Can't allocate memory param key=value123");
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    os_malloc_trace_dump();
    ESP_LOG_WRAPPER_TEST_CHECK_LOG_RECORD("MEM_TRACE", ESP_LOG_INFO, "Num blocks allocated: 0");
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_EQ(0, this->m_alloc_free_call_count);
}

TEST_F(TestHttpServerCb, http_server_get_from_params_with_decoding_ok) // NOLINT
{
    esp_log_wrapper_clear();
    str_buf_t result = http_server_get_from_params_with_decoding("url=http%3A%2F%2Fexample.com&other=1", "url=");

    ASSERT_NE(nullptr, result.buf);
    ASSERT_EQ(string("http://example.com"), string(result.buf));
    TEST_CHECK_LOG_RECORD_HTTP_SERVER(ESP_LOG_DEBUG, "HTTP params: url=http%3A%2F%2Fexample.com");
    TEST_CHECK_LOG_RECORD_HTTP_SERVER(
        ESP_LOG_DEBUG,
        "HTTP params: key 'url=': value (encoded): http%3A%2F%2Fexample.com");
    TEST_CHECK_LOG_RECORD_HTTP_SERVER(ESP_LOG_DEBUG, "HTTP params: key 'url=': value (decoded): http://example.com");
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    str_buf_free_buf(&result);
    os_malloc_trace_dump();
    ESP_LOG_WRAPPER_TEST_CHECK_LOG_RECORD("MEM_TRACE", ESP_LOG_INFO, "Num blocks allocated: 0");
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_EQ(0, this->m_alloc_free_call_count);
}

TEST_F(TestHttpServerCb, http_server_get_from_params_with_decoding_no_encoding_needed) // NOLINT
{
    esp_log_wrapper_clear();
    str_buf_t result = http_server_get_from_params_with_decoding("key=plain_value", "key=");

    ASSERT_NE(nullptr, result.buf);
    ASSERT_EQ(string("plain_value"), string(result.buf));
    TEST_CHECK_LOG_RECORD_HTTP_SERVER(ESP_LOG_DEBUG, "HTTP params: key=plain_value");
    TEST_CHECK_LOG_RECORD_HTTP_SERVER(ESP_LOG_DEBUG, "HTTP params: key 'key=': value (encoded): plain_value");
    TEST_CHECK_LOG_RECORD_HTTP_SERVER(ESP_LOG_DEBUG, "HTTP params: key 'key=': value (decoded): plain_value");
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    str_buf_free_buf(&result);
    os_malloc_trace_dump();
    ESP_LOG_WRAPPER_TEST_CHECK_LOG_RECORD("MEM_TRACE", ESP_LOG_INFO, "Num blocks allocated: 0");
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_EQ(0, this->m_alloc_free_call_count);
}

TEST_F(TestHttpServerCb, http_server_get_from_params_with_decoding_key_not_found) // NOLINT
{
    esp_log_wrapper_clear();
    str_buf_t result = http_server_get_from_params_with_decoding("other=value", "key=");

    ASSERT_EQ(nullptr, result.buf);
    TEST_CHECK_LOG_RECORD_HTTP_SERVER(ESP_LOG_DEBUG, "Can't find key 'key=' in URL params");
    TEST_CHECK_LOG_RECORD_HTTP_SERVER(ESP_LOG_ERROR, "HTTP params: Can't find 'key='");
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    os_malloc_trace_dump();
    ESP_LOG_WRAPPER_TEST_CHECK_LOG_RECORD("MEM_TRACE", ESP_LOG_INFO, "Num blocks allocated: 0");
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_EQ(0, this->m_alloc_free_call_count);
}

TEST_F(TestHttpServerCb, http_server_get_from_params_with_decoding_malloc_fail_on_get_params) // NOLINT
{
    g_pTestClass->m_malloc_fail_on_cnt = 1;

    esp_log_wrapper_clear();
    str_buf_t result = http_server_get_from_params_with_decoding("key=hello%20world", "key=");

    ASSERT_EQ(nullptr, result.buf);
    TEST_CHECK_LOG_RECORD_HTTP_SERVER(ESP_LOG_DEBUG, "HTTP params: key=hello%20world");
    TEST_CHECK_LOG_RECORD_HTTP_SERVER(ESP_LOG_ERROR, "Can't allocate memory param key=hello%20world");
    TEST_CHECK_LOG_RECORD_HTTP_SERVER(ESP_LOG_ERROR, "HTTP params: Can't find 'key='");
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    os_malloc_trace_dump();
    ESP_LOG_WRAPPER_TEST_CHECK_LOG_RECORD("MEM_TRACE", ESP_LOG_INFO, "Num blocks allocated: 0");
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_EQ(0, this->m_alloc_free_call_count);
}

TEST_F(TestHttpServerCb, http_server_get_from_params_with_decoding_malloc_fail_on_decode) // NOLINT
{
    g_pTestClass->m_malloc_fail_on_cnt = 2;

    esp_log_wrapper_clear();
    str_buf_t result = http_server_get_from_params_with_decoding("key=hello%20world", "key=");

    ASSERT_EQ(nullptr, result.buf);
    TEST_CHECK_LOG_RECORD_HTTP_SERVER(ESP_LOG_DEBUG, "HTTP params: key=hello%20world");
    TEST_CHECK_LOG_RECORD_HTTP_SERVER(ESP_LOG_DEBUG, "HTTP params: key 'key=': value (encoded): hello%20world");
    TEST_CHECK_LOG_RECORD_HTTP_SERVER(ESP_LOG_ERROR, "HTTP params: key 'key=': Can't decode value: hello%20world");
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    os_malloc_trace_dump();
    ESP_LOG_WRAPPER_TEST_CHECK_LOG_RECORD("MEM_TRACE", ESP_LOG_INFO, "Num blocks allocated: 0");
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_EQ(0, this->m_alloc_free_call_count);
}

TEST_F(TestHttpServerCb, http_post_helper_wait_and_gen_resp_ok_200) // NOLINT
{
    const char* json_content = "{\"data\":\"test\"}";
    char*       p_heap_buf   = static_cast<char*>(os_malloc(strlen(json_content) + 1));
    ASSERT_NE(nullptr, p_heap_buf);
    strcpy(p_heap_buf, json_content);

    this->m_mock_async_resp_set = true;
    this->m_mock_async_resp     = (http_server_resp_t){
            .http_resp_code       = HTTP_RESP_CODE_200,
            .content_location     = HTTP_CONTENT_LOCATION_HEAP,
            .flag_no_cache        = true,
            .flag_add_header_date = true,
            .content_type         = HTTP_CONTENT_TYPE_APPLICATION_JSON,
            .p_content_type_param = NULL,
            .content_len          = strlen(json_content),
            .content_encoding     = HTTP_CONTENT_ENCODING_NONE,
            .select_location = {
                .heap = {
                    .p_buf = reinterpret_cast<uint8_t*>(p_heap_buf),
                },
            },
    };

    http_async_info_t async_info    = {};
    async_info.p_http_client_handle = reinterpret_cast<esp_http_client_handle_t>(0x1234);

    esp_log_wrapper_clear();
    http_server_resp_t resp = http_post_helper_wait_until_async_req_completed_and_gen_resp(&async_info, 10);

    ASSERT_EQ(HTTP_RESP_CODE_200, resp.http_resp_code);
    ASSERT_EQ(HTTP_CONTENT_LOCATION_HEAP, resp.content_location);
    ASSERT_NE(nullptr, resp.select_location.heap.p_buf);

    const char* expected_json
        = "{\n"
          "\t\"status\":\t200,\n"
          "\t\"message\":\t\"{\\\"data\\\":\\\"test\\\"}\"\n"
          "}";
    ASSERT_EQ(string(expected_json), string(reinterpret_cast<const char*>(resp.select_location.heap.p_buf)));

    ASSERT_TRUE(this->m_mock_esp_http_client_cleanup_called);
    ASSERT_TRUE(this->m_mock_http_async_info_free_data_called);
    ASSERT_EQ(nullptr, async_info.p_http_client_handle);

    http_server_resp_free(&resp);
    os_malloc_trace_dump();
    ESP_LOG_WRAPPER_TEST_CHECK_LOG_RECORD("MEM_TRACE", ESP_LOG_INFO, "Num blocks allocated: 0");
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_EQ(0, this->m_alloc_free_call_count);
}

TEST_F(TestHttpServerCb, http_post_helper_wait_and_gen_resp_429_returns_200) // NOLINT
{
    const char* json_content = "{\"error\":\"too many\"}";
    char*       p_heap_buf   = static_cast<char*>(os_malloc(strlen(json_content) + 1));
    ASSERT_NE(nullptr, p_heap_buf);
    strcpy(p_heap_buf, json_content);

    this->m_mock_async_resp_set = true;
    this->m_mock_async_resp     = (http_server_resp_t){
            .http_resp_code       = HTTP_RESP_CODE_429,
            .content_location     = HTTP_CONTENT_LOCATION_HEAP,
            .flag_no_cache        = true,
            .flag_add_header_date = true,
            .content_type         = HTTP_CONTENT_TYPE_APPLICATION_JSON,
            .p_content_type_param = NULL,
            .content_len          = strlen(json_content),
            .content_encoding     = HTTP_CONTENT_ENCODING_NONE,
            .select_location = {
                .heap = {
                    .p_buf = reinterpret_cast<uint8_t*>(p_heap_buf),
                },
            },
    };

    http_async_info_t async_info    = {};
    async_info.p_http_client_handle = reinterpret_cast<esp_http_client_handle_t>(0x1234);

    esp_log_wrapper_clear();
    http_server_resp_t resp = http_post_helper_wait_until_async_req_completed_and_gen_resp(&async_info, 10);

    ASSERT_EQ(HTTP_RESP_CODE_200, resp.http_resp_code);
    ASSERT_EQ(HTTP_CONTENT_LOCATION_HEAP, resp.content_location);
    ASSERT_NE(nullptr, resp.select_location.heap.p_buf);

    // HTTP_RESP_CODE_429 is converted to HTTP_RESP_CODE_200 in the status field
    const char* expected_json
        = "{\n"
          "\t\"status\":\t200,\n"
          "\t\"message\":\t\"{\\\"error\\\":\\\"too many\\\"}\"\n"
          "}";
    ASSERT_EQ(string(expected_json), string(reinterpret_cast<const char*>(resp.select_location.heap.p_buf)));

    ASSERT_TRUE(this->m_mock_esp_http_client_cleanup_called);
    ASSERT_TRUE(this->m_mock_http_async_info_free_data_called);
    ASSERT_EQ(nullptr, async_info.p_http_client_handle);

    http_server_resp_free(&resp);
    os_malloc_trace_dump();
    ESP_LOG_WRAPPER_TEST_CHECK_LOG_RECORD("MEM_TRACE", ESP_LOG_INFO, "Num blocks allocated: 0");
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_EQ(0, this->m_alloc_free_call_count);
}

TEST_F(TestHttpServerCb, http_post_helper_wait_and_gen_resp_500_error) // NOLINT
{
    this->m_mock_async_resp_set = true;
    this->m_mock_async_resp     = (http_server_resp_t) {
            .http_resp_code       = HTTP_RESP_CODE_500,
            .content_location     = HTTP_CONTENT_LOCATION_NO_CONTENT,
            .flag_no_cache        = true,
            .flag_add_header_date = true,
            .content_type         = HTTP_CONTENT_TYPE_TEXT_HTML,
            .p_content_type_param = NULL,
            .content_len          = 0,
            .content_encoding     = HTTP_CONTENT_ENCODING_NONE,
    };

    http_async_info_t async_info    = {};
    async_info.p_http_client_handle = reinterpret_cast<esp_http_client_handle_t>(0x5678);

    esp_log_wrapper_clear();
    http_server_resp_t resp = http_post_helper_wait_until_async_req_completed_and_gen_resp(&async_info, 30);

    ASSERT_EQ(HTTP_RESP_CODE_200, resp.http_resp_code);
    ASSERT_EQ(HTTP_CONTENT_LOCATION_HEAP, resp.content_location);
    ASSERT_NE(nullptr, resp.select_location.heap.p_buf);

    // No content -> p_json is NULL -> empty message
    const char* expected_json
        = "{\n"
          "\t\"status\":\t500,\n"
          "\t\"message\":\t\"\"\n"
          "}";
    ASSERT_EQ(string(expected_json), string(reinterpret_cast<const char*>(resp.select_location.heap.p_buf)));

    ASSERT_TRUE(this->m_mock_esp_http_client_cleanup_called);
    ASSERT_TRUE(this->m_mock_http_async_info_free_data_called);
    ASSERT_EQ(nullptr, async_info.p_http_client_handle);

    http_server_resp_free(&resp);
    os_malloc_trace_dump();
    ESP_LOG_WRAPPER_TEST_CHECK_LOG_RECORD("MEM_TRACE", ESP_LOG_INFO, "Num blocks allocated: 0");
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_EQ(0, this->m_alloc_free_call_count);
}

TEST_F(TestHttpServerCb, http_server_cb_deinit_of_not_initialized) // NOLINT
{
    ASSERT_FALSE(g_pTestClass->m_is_fatfs_mounted);
    http_server_cb_deinit();
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_FALSE(g_pTestClass->m_is_fatfs_mounted);
    os_malloc_trace_dump();
    ESP_LOG_WRAPPER_TEST_CHECK_LOG_RECORD("MEM_TRACE", ESP_LOG_INFO, "Num blocks allocated: 0");
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_EQ(0, this->m_alloc_free_call_count);
}

TEST_F(TestHttpServerCb, http_server_cb_init_failed) // NOLINT
{
    ASSERT_FALSE(g_pTestClass->m_is_fatfs_mounted);
    this->m_is_fatfs_mount_fail = true;
    ASSERT_FALSE(http_server_cb_init(GW_GWUI_PARTITION));
    ASSERT_FALSE(g_pTestClass->m_is_fatfs_mounted);
    TEST_CHECK_LOG_RECORD_HTTP_SERVER(ESP_LOG_ERROR, "flashfatfs_mount: failed to mount partition 'fatfs_gwui'");
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    os_malloc_trace_dump();
    ESP_LOG_WRAPPER_TEST_CHECK_LOG_RECORD("MEM_TRACE", ESP_LOG_INFO, "Num blocks allocated: 0");
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_EQ(0, this->m_alloc_free_call_count);
}

TEST_F(TestHttpServerCb, resp_json_ruuvi_ok) // NOLINT
{
    const char* expected_json
        = "{\n"
          "\t\"fw_ver\":\t\"v1.3.3\",\n"
          "\t\"nrf52_fw_ver\":\t\"v0.7.1\",\n"
          "\t\"gw_mac\":\t\"11:22:33:44:55:66\",\n"
          "\t\"storage\":\t{\n"
          "\t\t\"storage_ready\":\tfalse\n"
          "\t},\n"
          "\t\"wifi_sta_config\":\t{\n"
          "\t\t\"ssid\":\t\"\"\n"
          "\t},\n"
          "\t\"wifi_ap_config\":\t{\n"
          "\t\t\"channel\":\t1\n"
          "\t},\n"
          "\t\"use_eth\":\ttrue,\n"
          "\t\"eth_dhcp\":\ttrue,\n"
          "\t\"eth_static_ip\":\t\"\",\n"
          "\t\"eth_netmask\":\t\"\",\n"
          "\t\"eth_gw\":\t\"\",\n"
          "\t\"eth_dns1\":\t\"\",\n"
          "\t\"eth_dns2\":\t\"\",\n"
          "\t\"remote_cfg_use\":\tfalse,\n"
          "\t\"remote_cfg_url\":\t\"\",\n"
          "\t\"remote_cfg_auth_type\":\t\"none\",\n"
          "\t\"remote_cfg_use_ssl_client_cert\":\tfalse,\n"
          "\t\"remote_cfg_use_ssl_server_cert\":\tfalse,\n"
          "\t\"remote_cfg_refresh_interval_minutes\":\t0,\n"

          "\t\"use_http_ruuvi\":\tfalse,\n"
          "\t\"use_http\":\tfalse,\n"
          "\t\"http_url\":\t\"" RUUVI_GATEWAY_HTTP_DEFAULT_URL
          "\",\n"
          "\t\"http_period\":\t10,\n"
          "\t\"http_data_format\":\t\"ruuvi\",\n"
          "\t\"http_auth\":\t\"none\",\n"
          "\t\"http_use_ssl_client_cert\":\tfalse,\n"
          "\t\"http_use_ssl_server_cert\":\tfalse,\n"
          "\t\"http_use_extra_http_path\":\tfalse,\n"
          "\t\"http_use_extra_http_query\":\tfalse,\n"
          "\t\"http_use_extra_http_headers\":\tfalse,\n"
          "\t\"use_http_stat\":\ttrue,\n"
          "\t\"http_stat_url\":\t\"" RUUVI_GATEWAY_HTTP_STATUS_URL
          "\",\n"
          "\t\"http_stat_user\":\t\"\",\n"
          "\t\"http_stat_use_ssl_client_cert\":\tfalse,\n"
          "\t\"http_stat_use_ssl_server_cert\":\tfalse,\n"
          "\t\"use_mqtt\":\ttrue,\n"
          "\t\"mqtt_disable_retained_messages\":\tfalse,\n"
          "\t\"mqtt_transport\":\t\"TCP\",\n"
          "\t\"mqtt_data_format\":\t\"ruuvi_raw\",\n"
          "\t\"mqtt_server\":\t\"test.mosquitto.org\",\n"
          "\t\"mqtt_port\":\t1883,\n"
          "\t\"mqtt_sending_interval\":\t0,\n"
          "\t\"mqtt_prefix\":\t\"ruuvi/30:AE:A4:02:84:A4\",\n"
          "\t\"mqtt_client_id\":\t\"30:AE:A4:02:84:A4\",\n"
          "\t\"mqtt_user\":\t\"\",\n"
          "\t\"mqtt_use_ssl_client_cert\":\tfalse,\n"
          "\t\"mqtt_use_ssl_server_cert\":\tfalse,\n"
          "\t\"lan_auth_type\":\t\"lan_auth_default\",\n"
          "\t\"lan_auth_user\":\t\"Admin\",\n"
          "\t\"lan_auth_api_key_use\":\ttrue,\n"
          "\t\"lan_auth_api_key_rw_use\":\tfalse,\n"
          "\t\"auto_update_cycle\":\t\"regular\",\n"
          "\t\"auto_update_weekdays_bitmask\":\t127,\n"
          "\t\"auto_update_interval_from\":\t0,\n"
          "\t\"auto_update_interval_to\":\t24,\n"
          "\t\"auto_update_tz_offset_hours\":\t3,\n"
          "\t\"ntp_use\":\ttrue,\n"
          "\t\"ntp_use_dhcp\":\tfalse,\n"
          "\t\"ntp_server1\":\t\"time1.server.com\",\n"
          "\t\"ntp_server2\":\t\"time2.server.com\",\n"
          "\t\"ntp_server3\":\t\"time3.server.com\",\n"
          "\t\"ntp_server4\":\t\"time4.server.com\",\n"
          "\t\"company_id\":\t1177,\n"
          "\t\"company_use_filtering\":\ttrue,\n"
          "\t\"scan_coded_phy\":\tfalse,\n"
          "\t\"scan_1mbit_phy\":\ttrue,\n"
          "\t\"scan_2mbit_phy\":\ttrue,\n"
          "\t\"scan_channel_37\":\ttrue,\n"
          "\t\"scan_channel_38\":\ttrue,\n"
          "\t\"scan_channel_39\":\ttrue,\n"
          "\t\"scan_default\":\ttrue,\n"
          "\t\"scan_filter_allow_listed\":\tfalse,\n"
          "\t\"scan_filter_list\":\t[],\n"
          "\t\"coordinates\":\t\"\",\n"
          "\t\"fw_update_url\":\t\"https://network.ruuvi.com/firmwareupdate\"\n"
          "}";
    bool     flag_network_cfg = false;
    gw_cfg_t gw_cfg           = get_gateway_config_default();
    ASSERT_TRUE(json_ruuvi_parse_http_body(
        "{"
        "\"remote_cfg_use\":false,\n"
        "\"remote_cfg_url\":\"\",\n"
        "\"remote_cfg_auth_type\":\"none\",\n"
        "\"remote_cfg_refresh_interval_minutes\":0,\n"
        "\"use_mqtt\":true,"
        "\"mqtt_disable_retained_messages\":false,"
        "\"mqtt_server\":\"test.mosquitto.org\","
        "\"mqtt_port\":1883,"
        "\"mqtt_sending_interval\":0,"
        "\"mqtt_prefix\":\"ruuvi/30:AE:A4:02:84:A4\","
        "\"mqtt_client_id\":\"30:AE:A4:02:84:A4\","
        "\"mqtt_user\":\"\","
        "\"mqtt_pass\":\"\","
        "\"use_http_ruuvi\":false,"
        "\"use_http\":false,"
        "\"use_http_stat\":true,"
        "\"http_stat_url\":\"" RUUVI_GATEWAY_HTTP_STATUS_URL
        "\","
        "\"http_stat_user\":\"\","
        "\"http_stat_pass\":\"\","
        "\"lan_auth_api_key\":\"wH3F9SIiAA3rhG32aJki2Z7ekdFc0vtxuDhxl39zFvw=\","
        "\"auto_update_cycle\":\"regular\","
        "\"auto_update_weekdays_bitmask\":127,"
        "\"auto_update_interval_from\":0,"
        "\"auto_update_interval_to\":24,"
        "\"auto_update_tz_offset_hours\":3,"
        "\"ntp_use\":true,"
        "\"ntp_use_dhcp\":false,"
        "\"ntp_server1\":\"time1.server.com\","
        "\"ntp_server2\":\"time2.server.com\","
        "\"ntp_server3\":\"time3.server.com\","
        "\"ntp_server4\":\"time4.server.com\","
        "\"company_use_filtering\":true,"
        "\"fw_update_url\":\"https://network.ruuvi.com/firmwareupdate\""
        "}",
        &gw_cfg,
        &flag_network_cfg));
    ASSERT_FALSE(flag_network_cfg);
    gw_cfg_update_ruuvi_cfg(&gw_cfg.ruuvi_cfg);

    esp_log_wrapper_clear();
    http_server_resp_t resp = http_server_resp_json_ruuvi();

    ASSERT_EQ(HTTP_RESP_CODE_200, resp.http_resp_code);
    ASSERT_EQ(HTTP_CONTENT_LOCATION_HEAP, resp.content_location);
    ASSERT_TRUE(resp.flag_no_cache);
    ASSERT_EQ(HTTP_CONTENT_TYPE_APPLICATION_JSON, resp.content_type);
    ASSERT_EQ(nullptr, resp.p_content_type_param);
    ASSERT_NE(nullptr, resp.select_location.heap.p_buf);
    ASSERT_EQ(string(expected_json), string(reinterpret_cast<char*>(resp.select_location.heap.p_buf)));
    ASSERT_EQ(strlen(expected_json), resp.content_len);
    ASSERT_EQ(HTTP_CONTENT_ENCODING_NONE, resp.content_encoding);
    TEST_CHECK_LOG_RECORD_HTTP_SERVER(ESP_LOG_INFO, string("ruuvi.json: ") + string(expected_json));
    TEST_CHECK_LOG_RECORD_HTTP_SERVER(ESP_LOG_INFO, string("Activate cfg_mode"));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    http_server_resp_free(&resp);
    os_malloc_trace_dump();
    ESP_LOG_WRAPPER_TEST_CHECK_LOG_RECORD("MEM_TRACE", ESP_LOG_INFO, "Num blocks allocated: 0");
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_EQ(0, this->m_alloc_free_call_count);
}

TEST_F(TestHttpServerCb, resp_json_ruuvi_malloc_failed_1) // NOLINT
{
    g_pTestClass->m_malloc_cnt         = 0;
    g_pTestClass->m_malloc_fail_on_cnt = 1;

    bool     flag_network_cfg = false;
    gw_cfg_t gw_cfg           = get_gateway_config_default();
    ASSERT_FALSE(
        json_ruuvi_parse_http_body(
            "{"
            "\"use_mqtt\":true,"
            "\"mqtt_disable_retained_messages\":false,"
            "\"mqtt_server\":\"test.mosquitto.org\","
            "\"mqtt_port\":1883,"
            "\t\"mqtt_sending_interval\":\t0,\n"
            "\"mqtt_prefix\":\"ruuvi/30:AE:A4:02:84:A4\","
            "\"mqtt_client_id\":\"30:AE:A4:02:84:A4\","
            "\"mqtt_user\":\"\","
            "\"mqtt_pass\":\"\","
            "\"use_http\":false,"
            "\"http_url\":\"" RUUVI_GATEWAY_HTTP_DEFAULT_URL "\","
            "\"http_user\":\"\","
            "\"http_pass\":\"\","
            "\"use_http_stat\":false,"
            "\"http_stat_url\":\"" RUUVI_GATEWAY_HTTP_STATUS_URL "\","
            "\"http_stat_user\":\"\","
            "\"http_stat_pass\":\"\","
            "\"company_use_filtering\":true"
            "}",
            &gw_cfg,
            &flag_network_cfg));
    ASSERT_FALSE(flag_network_cfg);
    esp_log_wrapper_clear();
    os_malloc_trace_dump();
    ESP_LOG_WRAPPER_TEST_CHECK_LOG_RECORD("MEM_TRACE", ESP_LOG_INFO, "Num blocks allocated: 0");
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_EQ(0, this->m_alloc_free_call_count);
}

TEST_F(TestHttpServerCb, resp_json_ruuvi_malloc_failed_2) // NOLINT
{
    bool     flag_network_cfg = false;
    gw_cfg_t gw_cfg           = get_gateway_config_default();
    ASSERT_TRUE(
        json_ruuvi_parse_http_body(
            "{"
            "\"use_mqtt\":true,"
            "\"mqtt_disable_retained_messages\":false,"
            "\"mqtt_server\":\"test.mosquitto.org\","
            "\"mqtt_port\":1883,"
            "\"mqtt_sending_interval\":0,"
            "\"mqtt_prefix\":\"ruuvi/30:AE:A4:02:84:A4\","
            "\"mqtt_client_id\":\"30:AE:A4:02:84:A4\","
            "\"mqtt_user\":\"\","
            "\"mqtt_pass\":\"\","
            "\"use_http\":false,"
            "\"http_url\":\"" RUUVI_GATEWAY_HTTP_DEFAULT_URL "\","
            "\"http_user\":\"\","
            "\"http_pass\":\"\","
            "\"use_http_stat\":false,"
            "\"http_stat_url\":\"" RUUVI_GATEWAY_HTTP_STATUS_URL "\","
            "\"http_stat_user\":\"\","
            "\"http_stat_pass\":\"\","
            "\"company_use_filtering\":true"
            "}",
            &gw_cfg,
            &flag_network_cfg));
    ASSERT_FALSE(flag_network_cfg);
    gw_cfg_update_ruuvi_cfg(&gw_cfg.ruuvi_cfg);

    g_pTestClass->m_malloc_cnt         = 0;
    g_pTestClass->m_malloc_fail_on_cnt = 2;

    esp_log_wrapper_clear();
    const http_server_resp_t resp = http_server_resp_json_ruuvi();

    ASSERT_EQ(HTTP_RESP_CODE_503, resp.http_resp_code);
    ASSERT_EQ(HTTP_CONTENT_LOCATION_NO_CONTENT, resp.content_location);
    ASSERT_TRUE(resp.flag_no_cache);
    ASSERT_EQ(HTTP_CONTENT_TYPE_TEXT_HTML, resp.content_type);
    ASSERT_EQ(nullptr, resp.p_content_type_param);
    ASSERT_EQ(0, resp.content_len);
    ASSERT_EQ(HTTP_CONTENT_ENCODING_NONE, resp.content_encoding);
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_ERROR, string("Can't add json item: fw_ver"));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    os_malloc_trace_dump();
    ESP_LOG_WRAPPER_TEST_CHECK_LOG_RECORD("MEM_TRACE", ESP_LOG_INFO, "Num blocks allocated: 0");
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_EQ(0, this->m_alloc_free_call_count);
}

TEST_F(TestHttpServerCb, resp_json_ok) // NOLINT
{
    const char* expected_json
        = "{\n"
          "\t\"fw_ver\":\t\"v1.3.3\",\n"
          "\t\"nrf52_fw_ver\":\t\"v0.7.1\",\n"
          "\t\"gw_mac\":\t\"11:22:33:44:55:66\",\n"
          "\t\"storage\":\t{\n"
          "\t\t\"storage_ready\":\tfalse\n"
          "\t},\n"
          "\t\"wifi_sta_config\":\t{\n"
          "\t\t\"ssid\":\t\"\"\n"
          "\t},\n"
          "\t\"wifi_ap_config\":\t{\n"
          "\t\t\"channel\":\t1\n"
          "\t},\n"
          "\t\"use_eth\":\ttrue,\n"
          "\t\"eth_dhcp\":\ttrue,\n"
          "\t\"eth_static_ip\":\t\"\",\n"
          "\t\"eth_netmask\":\t\"\",\n"
          "\t\"eth_gw\":\t\"\",\n"
          "\t\"eth_dns1\":\t\"\",\n"
          "\t\"eth_dns2\":\t\"\",\n"
          "\t\"remote_cfg_use\":\tfalse,\n"
          "\t\"remote_cfg_url\":\t\"\",\n"
          "\t\"remote_cfg_auth_type\":\t\"none\",\n"
          "\t\"remote_cfg_use_ssl_client_cert\":\tfalse,\n"
          "\t\"remote_cfg_use_ssl_server_cert\":\tfalse,\n"
          "\t\"remote_cfg_refresh_interval_minutes\":\t0,\n"
          "\t\"use_http_ruuvi\":\ttrue,\n"
          "\t\"use_http\":\ttrue,\n"
          "\t\"http_url\":\t\"" RUUVI_GATEWAY_HTTP_DEFAULT_URL
          "\",\n"
          "\t\"http_period\":\t10,\n"
          "\t\"http_data_format\":\t\"ruuvi\",\n"
          "\t\"http_auth\":\t\"none\",\n"
          "\t\"http_use_ssl_client_cert\":\tfalse,\n"
          "\t\"http_use_ssl_server_cert\":\tfalse,\n"
          "\t\"http_use_extra_http_path\":\tfalse,\n"
          "\t\"http_use_extra_http_query\":\tfalse,\n"
          "\t\"http_use_extra_http_headers\":\tfalse,\n"
          "\t\"use_http_stat\":\ttrue,\n"
          "\t\"http_stat_url\":\t\"" RUUVI_GATEWAY_HTTP_STATUS_URL
          "\",\n"
          "\t\"http_stat_user\":\t\"\",\n"
          "\t\"http_stat_use_ssl_client_cert\":\tfalse,\n"
          "\t\"http_stat_use_ssl_server_cert\":\tfalse,\n"
          "\t\"use_mqtt\":\ttrue,\n"
          "\t\"mqtt_disable_retained_messages\":\tfalse,\n"
          "\t\"mqtt_transport\":\t\"TCP\",\n"
          "\t\"mqtt_data_format\":\t\"ruuvi_raw\",\n"
          "\t\"mqtt_server\":\t\"test.mosquitto.org\",\n"
          "\t\"mqtt_port\":\t1883,\n"
          "\t\"mqtt_sending_interval\":\t0,\n"
          "\t\"mqtt_prefix\":\t\"ruuvi/30:AE:A4:02:84:A4\",\n"
          "\t\"mqtt_client_id\":\t\"30:AE:A4:02:84:A4\",\n"
          "\t\"mqtt_user\":\t\"\",\n"
          "\t\"mqtt_use_ssl_client_cert\":\tfalse,\n"
          "\t\"mqtt_use_ssl_server_cert\":\tfalse,\n"
          "\t\"lan_auth_type\":\t\"lan_auth_default\",\n"
          "\t\"lan_auth_user\":\t\"Admin\",\n"
          "\t\"lan_auth_api_key_use\":\tfalse,\n"
          "\t\"lan_auth_api_key_rw_use\":\tfalse,\n"
          "\t\"auto_update_cycle\":\t\"beta\",\n"
          "\t\"auto_update_weekdays_bitmask\":\t126,\n"
          "\t\"auto_update_interval_from\":\t1,\n"
          "\t\"auto_update_interval_to\":\t23,\n"
          "\t\"auto_update_tz_offset_hours\":\t-3,\n"
          "\t\"ntp_use\":\ttrue,\n"
          "\t\"ntp_use_dhcp\":\tfalse,\n"
          "\t\"ntp_server1\":\t\"time1.server.com\",\n"
          "\t\"ntp_server2\":\t\"time2.server.com\",\n"
          "\t\"ntp_server3\":\t\"time3.server.com\",\n"
          "\t\"ntp_server4\":\t\"time4.server.com\",\n"
          "\t\"company_id\":\t1177,\n"
          "\t\"company_use_filtering\":\ttrue,\n"
          "\t\"scan_coded_phy\":\tfalse,\n"
          "\t\"scan_1mbit_phy\":\ttrue,\n"
          "\t\"scan_2mbit_phy\":\ttrue,\n"
          "\t\"scan_channel_37\":\ttrue,\n"
          "\t\"scan_channel_38\":\ttrue,\n"
          "\t\"scan_channel_39\":\ttrue,\n"
          "\t\"scan_default\":\ttrue,\n"
          "\t\"scan_filter_allow_listed\":\tfalse,\n"
          "\t\"scan_filter_list\":\t[],\n"
          "\t\"coordinates\":\t\"\",\n"
          "\t\"fw_update_url\":\t\"https://network.ruuvi.com/firmwareupdate\"\n"
          "}";
    bool     flag_network_cfg = false;
    gw_cfg_t gw_cfg           = get_gateway_config_default();
    ASSERT_TRUE(
        json_ruuvi_parse_http_body(
            "{"
            "\"remote_cfg_use\":false,\n"
            "\"remote_cfg_url\":\"\",\n"
            "\"remote_cfg_auth_type\":\"none\",\n"
            "\"remote_cfg_refresh_interval_minutes\":0,\n"
            "\"use_mqtt\":true,"
            "\"mqtt_disable_retained_messages\":false,"
            "\"mqtt_server\":\"test.mosquitto.org\","
            "\"mqtt_port\":1883,"
            "\"mqtt_sending_interval\":0,"
            "\"mqtt_prefix\":\"ruuvi/30:AE:A4:02:84:A4\","
            "\"mqtt_client_id\":\"30:AE:A4:02:84:A4\","
            "\"mqtt_user\":\"\","
            "\"mqtt_pass\":\"\","
            "\"use_http\":false,"
            "\"http_url\":\"" RUUVI_GATEWAY_HTTP_DEFAULT_URL "\","
            "\"http_user\":\"\","
            "\"http_pass\":\"\","
            "\"use_http_stat\":true,"
            "\"http_stat_url\":\"" RUUVI_GATEWAY_HTTP_STATUS_URL "\","
            "\"http_stat_user\":\"\","
            "\"http_stat_pass\":\"\","
            "\"auto_update_cycle\":\"beta\","
            "\"auto_update_weekdays_bitmask\":126,"
            "\"auto_update_interval_from\":1,"
            "\"auto_update_interval_to\":23,"
            "\"auto_update_tz_offset_hours\":-3,"
            "\"lan_auth_api_key\":\"\","
            "\"ntp_use\":true,"
            "\"ntp_use_dhcp\":false,"
            "\"ntp_server1\":\"time1.server.com\","
            "\"ntp_server2\":\"time2.server.com\","
            "\"ntp_server3\":\"time3.server.com\","
            "\"ntp_server4\":\"time4.server.com\","
            "\"company_use_filtering\":true,"
            "\"fw_update_url\":\"https://network.ruuvi.com/firmwareupdate\""
            "}",
            &gw_cfg,
            &flag_network_cfg));
    ASSERT_FALSE(flag_network_cfg);
    gw_cfg_update_ruuvi_cfg(&gw_cfg.ruuvi_cfg);

    esp_log_wrapper_clear();
    http_server_resp_t resp = http_server_resp_json("ruuvi.json", false);

    ASSERT_EQ(HTTP_RESP_CODE_200, resp.http_resp_code);
    ASSERT_EQ(HTTP_CONTENT_LOCATION_HEAP, resp.content_location);
    ASSERT_TRUE(resp.flag_no_cache);
    ASSERT_EQ(HTTP_CONTENT_TYPE_APPLICATION_JSON, resp.content_type);
    ASSERT_EQ(nullptr, resp.p_content_type_param);
    ASSERT_NE(nullptr, resp.select_location.heap.p_buf);
    ASSERT_EQ(string(expected_json), string(reinterpret_cast<const char*>(resp.select_location.heap.p_buf)));
    ASSERT_EQ(strlen(expected_json), resp.content_len);
    ASSERT_EQ(HTTP_CONTENT_ENCODING_NONE, resp.content_encoding);
    TEST_CHECK_LOG_RECORD_HTTP_SERVER(ESP_LOG_INFO, string("ruuvi.json: ") + string(expected_json));
    TEST_CHECK_LOG_RECORD_HTTP_SERVER(ESP_LOG_INFO, string("Activate cfg_mode"));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    http_server_resp_free(&resp);
    os_malloc_trace_dump();
    ESP_LOG_WRAPPER_TEST_CHECK_LOG_RECORD("MEM_TRACE", ESP_LOG_INFO, "Num blocks allocated: 0");
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_EQ(0, this->m_alloc_free_call_count);
}

TEST_F(TestHttpServerCb, resp_json_unknown) // NOLINT
{
    const http_server_resp_t resp = http_server_resp_json("unknown.json", false);

    ASSERT_EQ(HTTP_RESP_CODE_404, resp.http_resp_code);
    ASSERT_EQ(HTTP_CONTENT_LOCATION_NO_CONTENT, resp.content_location);
    ASSERT_TRUE(resp.flag_no_cache);
    ASSERT_EQ(HTTP_CONTENT_TYPE_TEXT_HTML, resp.content_type);
    ASSERT_EQ(nullptr, resp.p_content_type_param);
    ASSERT_EQ(0, resp.content_len);
    ASSERT_EQ(HTTP_CONTENT_ENCODING_NONE, resp.content_encoding);
    TEST_CHECK_LOG_RECORD_HTTP_SERVER(ESP_LOG_WARN, string("Request to unknown json: unknown.json"));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    os_malloc_trace_dump();
    ESP_LOG_WRAPPER_TEST_CHECK_LOG_RECORD("MEM_TRACE", ESP_LOG_INFO, "Num blocks allocated: 0");
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_EQ(0, this->m_alloc_free_call_count);
}

TEST_F(TestHttpServerCb, resp_metrics_ok) // NOLINT
{
    const char*        expected_resp = "metrics_info";
    http_server_resp_t resp          = http_server_resp_metrics();

    ASSERT_EQ(HTTP_RESP_CODE_200, resp.http_resp_code);
    ASSERT_EQ(HTTP_CONTENT_LOCATION_HEAP, resp.content_location);
    ASSERT_TRUE(resp.flag_no_cache);
    ASSERT_EQ(HTTP_CONTENT_TYPE_TEXT_PLAIN, resp.content_type);
    ASSERT_NE(nullptr, resp.p_content_type_param);
    ASSERT_EQ(string("version=0.0.4"), string(resp.p_content_type_param));
    ASSERT_EQ(strlen(expected_resp), resp.content_len);
    ASSERT_EQ(HTTP_CONTENT_ENCODING_NONE, resp.content_encoding);
    ASSERT_NE(nullptr, resp.select_location.heap.p_buf);
    ASSERT_EQ(string(expected_resp), string(reinterpret_cast<const char*>(resp.select_location.heap.p_buf)));
    TEST_CHECK_LOG_RECORD_HTTP_SERVER(ESP_LOG_INFO, string("metrics: ") + string(expected_resp));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    http_server_resp_free(&resp);
    os_malloc_trace_dump();
    ESP_LOG_WRAPPER_TEST_CHECK_LOG_RECORD("MEM_TRACE", ESP_LOG_INFO, "Num blocks allocated: 0");
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_EQ(0, this->m_alloc_free_call_count);
}

TEST_F(TestHttpServerCb, resp_metrics_malloc_failed) // NOLINT
{
    g_pTestClass->m_malloc_fail_on_cnt = 1;
    const http_server_resp_t resp      = http_server_resp_metrics();

    ASSERT_EQ(HTTP_RESP_CODE_503, resp.http_resp_code);
    ASSERT_EQ(HTTP_CONTENT_LOCATION_NO_CONTENT, resp.content_location);
    ASSERT_TRUE(resp.flag_no_cache);
    ASSERT_EQ(HTTP_CONTENT_TYPE_TEXT_HTML, resp.content_type);
    ASSERT_EQ(nullptr, resp.p_content_type_param);
    ASSERT_EQ(0, resp.content_len);
    ASSERT_EQ(HTTP_CONTENT_ENCODING_NONE, resp.content_encoding);
    TEST_CHECK_LOG_RECORD_HTTP_SERVER(ESP_LOG_ERROR, string("Not enough memory"));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    os_malloc_trace_dump();
    ESP_LOG_WRAPPER_TEST_CHECK_LOG_RECORD("MEM_TRACE", ESP_LOG_INFO, "Num blocks allocated: 0");
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_EQ(0, this->m_alloc_free_call_count);
}

TEST_F(TestHttpServerCb, http_get_content_type_by_ext) // NOLINT
{
    ASSERT_EQ(HTTP_CONTENT_TYPE_TEXT_HTML, http_get_content_type_by_ext(".html"));
    ASSERT_EQ(HTTP_CONTENT_TYPE_TEXT_CSS, http_get_content_type_by_ext(".css"));
    ASSERT_EQ(HTTP_CONTENT_TYPE_TEXT_CSS, http_get_content_type_by_ext(".scss"));
    ASSERT_EQ(HTTP_CONTENT_TYPE_TEXT_JAVASCRIPT, http_get_content_type_by_ext(".js"));
    ASSERT_EQ(HTTP_CONTENT_TYPE_IMAGE_PNG, http_get_content_type_by_ext(".png"));
    ASSERT_EQ(HTTP_CONTENT_TYPE_IMAGE_SVG_XML, http_get_content_type_by_ext(".svg"));
    ASSERT_EQ(HTTP_CONTENT_TYPE_APPLICATION_OCTET_STREAM, http_get_content_type_by_ext(".ttf"));
    ASSERT_EQ(HTTP_CONTENT_TYPE_APPLICATION_OCTET_STREAM, http_get_content_type_by_ext(".unk"));
    os_malloc_trace_dump();
    ESP_LOG_WRAPPER_TEST_CHECK_LOG_RECORD("MEM_TRACE", ESP_LOG_INFO, "Num blocks allocated: 0");
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_EQ(0, this->m_alloc_free_call_count);
}

TEST_F(TestHttpServerCb, resp_file_index_html_fail_partition_not_ready) // NOLINT
{
    const char*             expected_resp = "index_html_content";
    const file_descriptor_t fd            = 1;
    FileInfo                fileInfo      = FileInfo("index.html", expected_resp);
    this->m_files.emplace_back(fileInfo);
    this->m_fd = fd;

    const http_server_resp_t resp = http_server_resp_file("index.html", HTTP_RESP_CODE_200);
    ASSERT_EQ(HTTP_RESP_CODE_503, resp.http_resp_code);
    ASSERT_EQ(HTTP_CONTENT_LOCATION_NO_CONTENT, resp.content_location);
    ASSERT_TRUE(resp.flag_no_cache);
    ASSERT_EQ(HTTP_CONTENT_TYPE_TEXT_HTML, resp.content_type);
    ASSERT_EQ(nullptr, resp.p_content_type_param);
    ASSERT_EQ(0, resp.content_len);
    ASSERT_EQ(HTTP_CONTENT_ENCODING_NONE, resp.content_encoding);
    TEST_CHECK_LOG_RECORD_HTTP_SERVER(ESP_LOG_DEBUG, "Try to find file: index.html");
    TEST_CHECK_LOG_RECORD_HTTP_SERVER(ESP_LOG_ERROR, "GWUI partition is not ready");
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    os_malloc_trace_dump();
    ESP_LOG_WRAPPER_TEST_CHECK_LOG_RECORD("MEM_TRACE", ESP_LOG_INFO, "Num blocks allocated: 0");
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_EQ(0, this->m_alloc_free_call_count);
}

TEST_F(TestHttpServerCb, resp_file_index_html_fail_file_name_too_long) // NOLINT
{
    const char*             file_name     = "a1234567890123456789012345678901234567890123456789012345678901234567890";
    const char*             expected_resp = "index_html_content";
    const file_descriptor_t fd            = 1;
    ASSERT_TRUE(http_server_cb_init(GW_GWUI_PARTITION));
    FileInfo fileInfo = FileInfo(file_name, expected_resp);
    this->m_files.emplace_back(fileInfo);
    this->m_fd = fd;

    const http_server_resp_t resp = http_server_resp_file(file_name, HTTP_RESP_CODE_200);
    ASSERT_EQ(HTTP_RESP_CODE_503, resp.http_resp_code);
    ASSERT_EQ(HTTP_CONTENT_LOCATION_NO_CONTENT, resp.content_location);
    ASSERT_TRUE(resp.flag_no_cache);
    ASSERT_EQ(HTTP_CONTENT_TYPE_TEXT_HTML, resp.content_type);
    ASSERT_EQ(nullptr, resp.p_content_type_param);
    ASSERT_EQ(0, resp.content_len);
    ASSERT_EQ(HTTP_CONTENT_ENCODING_NONE, resp.content_encoding);
    TEST_CHECK_LOG_RECORD_HTTP_SERVER(ESP_LOG_DEBUG, string("Try to find file: ") + string(file_name));
    TEST_CHECK_LOG_RECORD_HTTP_SERVER(
        ESP_LOG_ERROR,
        string("Temporary buffer is not enough for the file path '") + string(file_name) + string("'"));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    os_malloc_trace_dump();
    ESP_LOG_WRAPPER_TEST_CHECK_LOG_RECORD("MEM_TRACE", ESP_LOG_INFO, "Num blocks allocated: 0");
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_EQ(0, this->m_alloc_free_call_count);
}

TEST_F(TestHttpServerCb, resp_file_index_html) // NOLINT
{
    const char*             expected_resp = "index_html_content";
    const file_descriptor_t fd            = 1;
    ASSERT_TRUE(http_server_cb_init(GW_GWUI_PARTITION));
    FileInfo fileInfo = FileInfo("index.html", expected_resp);
    this->m_files.emplace_back(fileInfo);
    this->m_fd = fd;

    const http_server_resp_t resp = http_server_resp_file("index.html", HTTP_RESP_CODE_200);
    ASSERT_EQ(HTTP_RESP_CODE_200, resp.http_resp_code);
    ASSERT_EQ(HTTP_CONTENT_LOCATION_FATFS, resp.content_location);
    ASSERT_TRUE(resp.flag_no_cache);
    ASSERT_EQ(HTTP_CONTENT_TYPE_TEXT_HTML, resp.content_type);
    ASSERT_EQ(nullptr, resp.p_content_type_param);
    ASSERT_EQ(strlen(expected_resp), resp.content_len);
    ASSERT_EQ(HTTP_CONTENT_ENCODING_NONE, resp.content_encoding);
    ASSERT_EQ(fd, resp.select_location.fatfs.fd);
    TEST_CHECK_LOG_RECORD_HTTP_SERVER(ESP_LOG_DEBUG, "Try to find file: index.html");
    TEST_CHECK_LOG_RECORD_HTTP_SERVER(ESP_LOG_DEBUG, "File index.html was opened successfully, fd=1");
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    os_malloc_trace_dump();
    ESP_LOG_WRAPPER_TEST_CHECK_LOG_RECORD("MEM_TRACE", ESP_LOG_INFO, "Num blocks allocated: 0");
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_EQ(0, this->m_alloc_free_call_count);
}

TEST_F(TestHttpServerCb, resp_file_index_html_gzipped) // NOLINT
{
    const char*             expected_resp = "index_html_content";
    const file_descriptor_t fd            = 2;
    ASSERT_TRUE(http_server_cb_init(GW_GWUI_PARTITION));
    FileInfo fileInfo = FileInfo("index.html.gz", expected_resp);
    this->m_files.emplace_back(fileInfo);
    this->m_fd = fd;

    const http_server_resp_t resp = http_server_resp_file("index.html", HTTP_RESP_CODE_200);
    ASSERT_EQ(HTTP_RESP_CODE_200, resp.http_resp_code);
    ASSERT_EQ(HTTP_CONTENT_LOCATION_FATFS, resp.content_location);
    ASSERT_TRUE(resp.flag_no_cache);
    ASSERT_EQ(HTTP_CONTENT_TYPE_TEXT_HTML, resp.content_type);
    ASSERT_EQ(nullptr, resp.p_content_type_param);
    ASSERT_EQ(strlen(expected_resp), resp.content_len);
    ASSERT_EQ(HTTP_CONTENT_ENCODING_GZIP, resp.content_encoding);
    ASSERT_EQ(fd, resp.select_location.fatfs.fd);
    TEST_CHECK_LOG_RECORD_HTTP_SERVER(ESP_LOG_DEBUG, string("Try to find file: index.html"));
    TEST_CHECK_LOG_RECORD_HTTP_SERVER(ESP_LOG_DEBUG, "File index.html.gz was opened successfully, fd=2");
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    os_malloc_trace_dump();
    ESP_LOG_WRAPPER_TEST_CHECK_LOG_RECORD("MEM_TRACE", ESP_LOG_INFO, "Num blocks allocated: 0");
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_EQ(0, this->m_alloc_free_call_count);
}

TEST_F(TestHttpServerCb, resp_file_app_js_gzipped) // NOLINT
{
    const char*             expected_resp = "app_js_gzipped";
    const file_descriptor_t fd            = 1;
    ASSERT_TRUE(http_server_cb_init(GW_GWUI_PARTITION));
    FileInfo fileInfo = FileInfo("app.js.gz", expected_resp);
    this->m_files.emplace_back(fileInfo);
    this->m_fd = fd;

    const http_server_resp_t resp = http_server_resp_file("app.js", HTTP_RESP_CODE_200);
    ASSERT_EQ(HTTP_RESP_CODE_200, resp.http_resp_code);
    ASSERT_EQ(HTTP_CONTENT_LOCATION_FATFS, resp.content_location);
    ASSERT_TRUE(resp.flag_no_cache);
    ASSERT_EQ(HTTP_CONTENT_TYPE_TEXT_JAVASCRIPT, resp.content_type);
    ASSERT_EQ(nullptr, resp.p_content_type_param);
    ASSERT_EQ(strlen(expected_resp), resp.content_len);
    ASSERT_EQ(HTTP_CONTENT_ENCODING_GZIP, resp.content_encoding);
    ASSERT_EQ(fd, resp.select_location.fatfs.fd);
    TEST_CHECK_LOG_RECORD_HTTP_SERVER(ESP_LOG_DEBUG, string("Try to find file: app.js"));
    TEST_CHECK_LOG_RECORD_HTTP_SERVER(ESP_LOG_DEBUG, "File app.js.gz was opened successfully, fd=1");
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    os_malloc_trace_dump();
    ESP_LOG_WRAPPER_TEST_CHECK_LOG_RECORD("MEM_TRACE", ESP_LOG_INFO, "Num blocks allocated: 0");
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_EQ(0, this->m_alloc_free_call_count);
}

TEST_F(TestHttpServerCb, resp_file_app_css_gzipped) // NOLINT
{
    const char*             expected_resp = "slyle_css_gzipped";
    const file_descriptor_t fd            = 1;
    ASSERT_TRUE(http_server_cb_init(GW_GWUI_PARTITION));
    FileInfo fileInfo = FileInfo("style.css.gz", expected_resp);
    this->m_files.emplace_back(fileInfo);
    this->m_fd = fd;

    const http_server_resp_t resp = http_server_resp_file("style.css", HTTP_RESP_CODE_200);
    ASSERT_EQ(HTTP_RESP_CODE_200, resp.http_resp_code);
    ASSERT_EQ(HTTP_CONTENT_LOCATION_FATFS, resp.content_location);
    ASSERT_TRUE(resp.flag_no_cache);
    ASSERT_EQ(HTTP_CONTENT_TYPE_TEXT_CSS, resp.content_type);
    ASSERT_EQ(nullptr, resp.p_content_type_param);
    ASSERT_EQ(strlen(expected_resp), resp.content_len);
    ASSERT_EQ(HTTP_CONTENT_ENCODING_GZIP, resp.content_encoding);
    ASSERT_EQ(fd, resp.select_location.fatfs.fd);
    TEST_CHECK_LOG_RECORD_HTTP_SERVER(ESP_LOG_DEBUG, string("Try to find file: style.css"));
    TEST_CHECK_LOG_RECORD_HTTP_SERVER(ESP_LOG_DEBUG, "File style.css.gz was opened successfully, fd=1");
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    os_malloc_trace_dump();
    ESP_LOG_WRAPPER_TEST_CHECK_LOG_RECORD("MEM_TRACE", ESP_LOG_INFO, "Num blocks allocated: 0");
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_EQ(0, this->m_alloc_free_call_count);
}

TEST_F(TestHttpServerCb, resp_file_binary_without_extension) // NOLINT
{
    const char*             expected_resp = "binary_data";
    const file_descriptor_t fd            = 1;
    ASSERT_TRUE(http_server_cb_init(GW_GWUI_PARTITION));
    FileInfo fileInfo = FileInfo("binary", expected_resp);
    this->m_files.emplace_back(fileInfo);
    this->m_fd = fd;

    const http_server_resp_t resp = http_server_resp_file("binary", HTTP_RESP_CODE_200);
    ASSERT_EQ(HTTP_RESP_CODE_200, resp.http_resp_code);
    ASSERT_EQ(HTTP_CONTENT_LOCATION_FATFS, resp.content_location);
    ASSERT_TRUE(resp.flag_no_cache);
    ASSERT_EQ(HTTP_CONTENT_TYPE_APPLICATION_OCTET_STREAM, resp.content_type);
    ASSERT_EQ(nullptr, resp.p_content_type_param);
    ASSERT_EQ(strlen(expected_resp), resp.content_len);
    ASSERT_EQ(HTTP_CONTENT_ENCODING_NONE, resp.content_encoding);
    ASSERT_EQ(fd, resp.select_location.fatfs.fd);
    TEST_CHECK_LOG_RECORD_HTTP_SERVER(ESP_LOG_DEBUG, string("Try to find file: binary"));
    TEST_CHECK_LOG_RECORD_HTTP_SERVER(ESP_LOG_DEBUG, "File binary was opened successfully, fd=1");
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    os_malloc_trace_dump();
    ESP_LOG_WRAPPER_TEST_CHECK_LOG_RECORD("MEM_TRACE", ESP_LOG_INFO, "Num blocks allocated: 0");
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_EQ(0, this->m_alloc_free_call_count);
}

TEST_F(TestHttpServerCb, resp_file_unknown_html) // NOLINT
{
    ASSERT_TRUE(http_server_cb_init(GW_GWUI_PARTITION));

    const http_server_resp_t resp = http_server_resp_file("unknown.html", HTTP_RESP_CODE_200);
    ASSERT_EQ(HTTP_RESP_CODE_404, resp.http_resp_code);
    ASSERT_EQ(HTTP_CONTENT_LOCATION_NO_CONTENT, resp.content_location);
    ASSERT_TRUE(resp.flag_no_cache);
    ASSERT_EQ(HTTP_CONTENT_TYPE_TEXT_HTML, resp.content_type);
    ASSERT_EQ(nullptr, resp.p_content_type_param);
    ASSERT_EQ(0, resp.content_len);
    ASSERT_EQ(HTTP_CONTENT_ENCODING_NONE, resp.content_encoding);
    TEST_CHECK_LOG_RECORD_HTTP_SERVER(ESP_LOG_DEBUG, string("Try to find file: unknown.html"));
    TEST_CHECK_LOG_RECORD_HTTP_SERVER(ESP_LOG_ERROR, string("Can't find file: unknown.html"));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    os_malloc_trace_dump();
    ESP_LOG_WRAPPER_TEST_CHECK_LOG_RECORD("MEM_TRACE", ESP_LOG_INFO, "Num blocks allocated: 0");
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_EQ(0, this->m_alloc_free_call_count);
}

TEST_F(TestHttpServerCb, resp_file_index_html_failed_on_open) // NOLINT
{
    const char*             expected_resp = "index_html_content";
    const file_descriptor_t fd            = 1;
    ASSERT_TRUE(http_server_cb_init(GW_GWUI_PARTITION));
    FileInfo fileInfo = FileInfo("index.html", expected_resp, true);
    this->m_files.emplace_back(fileInfo);
    this->m_fd = fd;

    const http_server_resp_t resp = http_server_resp_file("index.html", HTTP_RESP_CODE_200);
    ASSERT_EQ(HTTP_RESP_CODE_503, resp.http_resp_code);
    ASSERT_EQ(HTTP_CONTENT_LOCATION_NO_CONTENT, resp.content_location);
    ASSERT_TRUE(resp.flag_no_cache);
    ASSERT_EQ(HTTP_CONTENT_TYPE_TEXT_HTML, resp.content_type);
    ASSERT_EQ(nullptr, resp.p_content_type_param);
    ASSERT_EQ(0, resp.content_len);
    ASSERT_EQ(HTTP_CONTENT_ENCODING_NONE, resp.content_encoding);
    TEST_CHECK_LOG_RECORD_HTTP_SERVER(ESP_LOG_DEBUG, "Try to find file: index.html");
    TEST_CHECK_LOG_RECORD_HTTP_SERVER(ESP_LOG_ERROR, string("Can't open file: index.html"));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    os_malloc_trace_dump();
    ESP_LOG_WRAPPER_TEST_CHECK_LOG_RECORD("MEM_TRACE", ESP_LOG_INFO, "Num blocks allocated: 0");
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_EQ(0, this->m_alloc_free_call_count);
}

TEST_F(TestHttpServerCb, http_server_cb_on_get_default) // NOLINT
{
    ASSERT_TRUE(http_server_cb_init(GW_GWUI_PARTITION));
    this->m_fd = -1;

    const http_server_resp_t resp = http_server_cb_on_get("", nullptr, false, nullptr);
    ASSERT_EQ(HTTP_RESP_CODE_404, resp.http_resp_code);
    ASSERT_EQ(HTTP_CONTENT_LOCATION_NO_CONTENT, resp.content_location);
    ASSERT_TRUE(resp.flag_no_cache);
    ASSERT_EQ(HTTP_CONTENT_TYPE_TEXT_HTML, resp.content_type);
    ASSERT_EQ(nullptr, resp.p_content_type_param);
    ASSERT_EQ(0, resp.content_len);
    ASSERT_EQ(HTTP_CONTENT_ENCODING_NONE, resp.content_encoding);
    ASSERT_EQ(0, resp.select_location.fatfs.fd);
    TEST_CHECK_LOG_RECORD_HTTP_SERVER(ESP_LOG_DEBUG, string("http_server_cb_on_get /"));
    TEST_CHECK_LOG_RECORD_HTTP_SERVER(ESP_LOG_DEBUG, string("Try to find file: ruuvi.html"));
    TEST_CHECK_LOG_RECORD_HTTP_SERVER(ESP_LOG_ERROR, "Can't find file: ruuvi.html");
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    os_malloc_trace_dump();
    ESP_LOG_WRAPPER_TEST_CHECK_LOG_RECORD("MEM_TRACE", ESP_LOG_INFO, "Num blocks allocated: 0");
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_EQ(0, this->m_alloc_free_call_count);
}

TEST_F(TestHttpServerCb, http_server_cb_on_get_default_during_fw_update) // NOLINT
{
    this->m_fw_update_is_in_progress = true;
    ASSERT_TRUE(http_server_cb_init(GW_GWUI_PARTITION));
    this->m_fd = -1;

    const http_server_resp_t resp = http_server_cb_on_get("", nullptr, false, nullptr);
    ASSERT_EQ(HTTP_RESP_CODE_404, resp.http_resp_code);
    ASSERT_EQ(HTTP_CONTENT_LOCATION_NO_CONTENT, resp.content_location);
    ASSERT_TRUE(resp.flag_no_cache);
    ASSERT_EQ(HTTP_CONTENT_TYPE_TEXT_HTML, resp.content_type);
    ASSERT_EQ(nullptr, resp.p_content_type_param);
    ASSERT_EQ(0, resp.content_len);
    ASSERT_EQ(HTTP_CONTENT_ENCODING_NONE, resp.content_encoding);
    ASSERT_EQ(0, resp.select_location.fatfs.fd);
    TEST_CHECK_LOG_RECORD_HTTP_SERVER(ESP_LOG_DEBUG, string("http_server_cb_on_get /"));
    TEST_CHECK_LOG_RECORD_HTTP_SERVER(ESP_LOG_DEBUG, string("Try to find file: ruuvi.html"));
    TEST_CHECK_LOG_RECORD_HTTP_SERVER(ESP_LOG_ERROR, "Can't find file: ruuvi.html");
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    os_malloc_trace_dump();
    ESP_LOG_WRAPPER_TEST_CHECK_LOG_RECORD("MEM_TRACE", ESP_LOG_INFO, "Num blocks allocated: 0");
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_EQ(0, this->m_alloc_free_call_count);
}

TEST_F(TestHttpServerCb, http_server_cb_on_get_index_html) // NOLINT
{
    const char*             expected_resp = "index_html_content";
    const file_descriptor_t fd            = 1;
    ASSERT_TRUE(http_server_cb_init(GW_GWUI_PARTITION));
    FileInfo fileInfo = FileInfo("index.html.gz", expected_resp);
    this->m_files.emplace_back(fileInfo);
    this->m_fd = fd;

    const http_server_resp_t resp = http_server_cb_on_get("index.html", nullptr, false, nullptr);
    ASSERT_EQ(HTTP_RESP_CODE_200, resp.http_resp_code);
    ASSERT_EQ(HTTP_CONTENT_LOCATION_FATFS, resp.content_location);
    ASSERT_TRUE(resp.flag_no_cache);
    ASSERT_EQ(HTTP_CONTENT_TYPE_TEXT_HTML, resp.content_type);
    ASSERT_EQ(nullptr, resp.p_content_type_param);
    ASSERT_EQ(strlen(expected_resp), resp.content_len);
    ASSERT_EQ(HTTP_CONTENT_ENCODING_GZIP, resp.content_encoding);
    ASSERT_EQ(fd, resp.select_location.fatfs.fd);
    TEST_CHECK_LOG_RECORD_HTTP_SERVER(ESP_LOG_DEBUG, string("http_server_cb_on_get /index.html"));
    TEST_CHECK_LOG_RECORD_HTTP_SERVER(ESP_LOG_DEBUG, string("Try to find file: index.html"));
    TEST_CHECK_LOG_RECORD_HTTP_SERVER(ESP_LOG_DEBUG, "File index.html.gz was opened successfully, fd=1");
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    os_malloc_trace_dump();
    ESP_LOG_WRAPPER_TEST_CHECK_LOG_RECORD("MEM_TRACE", ESP_LOG_INFO, "Num blocks allocated: 0");
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_EQ(0, this->m_alloc_free_call_count);
}

TEST_F(TestHttpServerCb, http_server_cb_on_get_index_html_during_fw_update) // NOLINT
{
    this->m_fw_update_is_in_progress      = true;
    const char*             expected_resp = "index_html_content";
    const file_descriptor_t fd            = 1;
    ASSERT_TRUE(http_server_cb_init(GW_GWUI_PARTITION));
    FileInfo fileInfo = FileInfo("index.html.gz", expected_resp);
    this->m_files.emplace_back(fileInfo);
    this->m_fd = fd;

    const http_server_resp_t resp = http_server_cb_on_get("index.html", nullptr, false, nullptr);
    ASSERT_EQ(HTTP_RESP_CODE_200, resp.http_resp_code);
    ASSERT_EQ(HTTP_CONTENT_LOCATION_FATFS, resp.content_location);
    ASSERT_TRUE(resp.flag_no_cache);
    ASSERT_EQ(HTTP_CONTENT_TYPE_TEXT_HTML, resp.content_type);
    ASSERT_EQ(nullptr, resp.p_content_type_param);
    ASSERT_EQ(strlen(expected_resp), resp.content_len);
    ASSERT_EQ(HTTP_CONTENT_ENCODING_GZIP, resp.content_encoding);
    ASSERT_EQ(fd, resp.select_location.fatfs.fd);
    TEST_CHECK_LOG_RECORD_HTTP_SERVER(ESP_LOG_DEBUG, string("http_server_cb_on_get /index.html"));
    TEST_CHECK_LOG_RECORD_HTTP_SERVER(ESP_LOG_DEBUG, string("Try to find file: index.html"));
    TEST_CHECK_LOG_RECORD_HTTP_SERVER(ESP_LOG_DEBUG, "File index.html.gz was opened successfully, fd=1");
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    os_malloc_trace_dump();
    ESP_LOG_WRAPPER_TEST_CHECK_LOG_RECORD("MEM_TRACE", ESP_LOG_INFO, "Num blocks allocated: 0");
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_EQ(0, this->m_alloc_free_call_count);
}

TEST_F(TestHttpServerCb, http_server_cb_on_get_app_js) // NOLINT
{
    const char*             expected_resp = "app_js_gzipped";
    const file_descriptor_t fd            = 1;
    ASSERT_TRUE(http_server_cb_init(GW_GWUI_PARTITION));
    FileInfo fileInfo = FileInfo("app.js.gz", expected_resp);
    this->m_files.emplace_back(fileInfo);
    this->m_fd = fd;

    const http_server_resp_t resp = http_server_cb_on_get("app.js", nullptr, false, nullptr);
    ASSERT_EQ(HTTP_RESP_CODE_200, resp.http_resp_code);
    ASSERT_EQ(HTTP_CONTENT_LOCATION_FATFS, resp.content_location);
    ASSERT_TRUE(resp.flag_no_cache);
    ASSERT_EQ(HTTP_CONTENT_TYPE_TEXT_JAVASCRIPT, resp.content_type);
    ASSERT_EQ(nullptr, resp.p_content_type_param);
    ASSERT_EQ(strlen(expected_resp), resp.content_len);
    ASSERT_EQ(HTTP_CONTENT_ENCODING_GZIP, resp.content_encoding);
    ASSERT_EQ(fd, resp.select_location.fatfs.fd);
    TEST_CHECK_LOG_RECORD_HTTP_SERVER(ESP_LOG_DEBUG, string("http_server_cb_on_get /app.js"));
    TEST_CHECK_LOG_RECORD_HTTP_SERVER(ESP_LOG_DEBUG, string("Try to find file: app.js"));
    TEST_CHECK_LOG_RECORD_HTTP_SERVER(ESP_LOG_DEBUG, "File app.js.gz was opened successfully, fd=1");
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    os_malloc_trace_dump();
    ESP_LOG_WRAPPER_TEST_CHECK_LOG_RECORD("MEM_TRACE", ESP_LOG_INFO, "Num blocks allocated: 0");
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_EQ(0, this->m_alloc_free_call_count);
}

TEST_F(TestHttpServerCb, http_server_cb_on_get_ruuvi_json) // NOLINT
{
    const char* expected_json
        = "{\n"
          "\t\"fw_ver\":\t\"v1.3.3\",\n"
          "\t\"nrf52_fw_ver\":\t\"v0.7.1\",\n"
          "\t\"gw_mac\":\t\"11:22:33:44:55:66\",\n"
          "\t\"storage\":\t{\n"
          "\t\t\"storage_ready\":\tfalse\n"
          "\t},\n"
          "\t\"wifi_sta_config\":\t{\n"
          "\t\t\"ssid\":\t\"\"\n"
          "\t},\n"
          "\t\"wifi_ap_config\":\t{\n"
          "\t\t\"channel\":\t1\n"
          "\t},\n"
          "\t\"use_eth\":\ttrue,\n"
          "\t\"eth_dhcp\":\ttrue,\n"
          "\t\"eth_static_ip\":\t\"\",\n"
          "\t\"eth_netmask\":\t\"\",\n"
          "\t\"eth_gw\":\t\"\",\n"
          "\t\"eth_dns1\":\t\"\",\n"
          "\t\"eth_dns2\":\t\"\",\n"
          "\t\"remote_cfg_use\":\tfalse,\n"
          "\t\"remote_cfg_url\":\t\"\",\n"
          "\t\"remote_cfg_auth_type\":\t\"none\",\n"
          "\t\"remote_cfg_use_ssl_client_cert\":\tfalse,\n"
          "\t\"remote_cfg_use_ssl_server_cert\":\tfalse,\n"
          "\t\"remote_cfg_refresh_interval_minutes\":\t0,\n"
          "\t\"use_http_ruuvi\":\tfalse,\n"
          "\t\"use_http\":\tfalse,\n"
          "\t\"http_url\":\t\"" RUUVI_GATEWAY_HTTP_DEFAULT_URL
          "\",\n"
          "\t\"http_period\":\t10,\n"
          "\t\"http_data_format\":\t\"ruuvi\",\n"
          "\t\"http_auth\":\t\"none\",\n"
          "\t\"http_use_ssl_client_cert\":\tfalse,\n"
          "\t\"http_use_ssl_server_cert\":\tfalse,\n"
          "\t\"http_use_extra_http_path\":\tfalse,\n"
          "\t\"http_use_extra_http_query\":\tfalse,\n"
          "\t\"http_use_extra_http_headers\":\tfalse,\n"
          "\t\"use_http_stat\":\ttrue,\n"
          "\t\"http_stat_url\":\t\"" RUUVI_GATEWAY_HTTP_STATUS_URL
          "\",\n"
          "\t\"http_stat_user\":\t\"\",\n"
          "\t\"http_stat_use_ssl_client_cert\":\tfalse,\n"
          "\t\"http_stat_use_ssl_server_cert\":\tfalse,\n"
          "\t\"use_mqtt\":\ttrue,\n"
          "\t\"mqtt_disable_retained_messages\":\tfalse,\n"
          "\t\"mqtt_transport\":\t\"TCP\",\n"
          "\t\"mqtt_data_format\":\t\"ruuvi_raw\",\n"
          "\t\"mqtt_server\":\t\"test.mosquitto.org\",\n"
          "\t\"mqtt_port\":\t1883,\n"
          "\t\"mqtt_sending_interval\":\t0,\n"
          "\t\"mqtt_prefix\":\t\"ruuvi/30:AE:A4:02:84:A4\",\n"
          "\t\"mqtt_client_id\":\t\"30:AE:A4:02:84:A4\",\n"
          "\t\"mqtt_user\":\t\"\",\n"
          "\t\"mqtt_use_ssl_client_cert\":\tfalse,\n"
          "\t\"mqtt_use_ssl_server_cert\":\tfalse,\n"
          "\t\"lan_auth_type\":\t\"lan_auth_default\",\n"
          "\t\"lan_auth_user\":\t\"Admin\",\n"
          "\t\"lan_auth_api_key_use\":\ttrue,\n"
          "\t\"lan_auth_api_key_rw_use\":\tfalse,\n"
          "\t\"auto_update_cycle\":\t\"regular\",\n"
          "\t\"auto_update_weekdays_bitmask\":\t127,\n"
          "\t\"auto_update_interval_from\":\t0,\n"
          "\t\"auto_update_interval_to\":\t24,\n"
          "\t\"auto_update_tz_offset_hours\":\t3,\n"
          "\t\"ntp_use\":\ttrue,\n"
          "\t\"ntp_use_dhcp\":\tfalse,\n"
          "\t\"ntp_server1\":\t\"time1.server.com\",\n"
          "\t\"ntp_server2\":\t\"time2.server.com\",\n"
          "\t\"ntp_server3\":\t\"time3.server.com\",\n"
          "\t\"ntp_server4\":\t\"time4.server.com\",\n"
          "\t\"company_id\":\t1177,\n"
          "\t\"company_use_filtering\":\ttrue,\n"
          "\t\"scan_coded_phy\":\tfalse,\n"
          "\t\"scan_1mbit_phy\":\ttrue,\n"
          "\t\"scan_2mbit_phy\":\ttrue,\n"
          "\t\"scan_channel_37\":\ttrue,\n"
          "\t\"scan_channel_38\":\ttrue,\n"
          "\t\"scan_channel_39\":\ttrue,\n"
          "\t\"scan_default\":\ttrue,\n"
          "\t\"scan_filter_allow_listed\":\tfalse,\n"
          "\t\"scan_filter_list\":\t[],\n"
          "\t\"coordinates\":\t\"\",\n"
          "\t\"fw_update_url\":\t\"https://network.ruuvi.com/firmwareupdate\"\n"
          "}";
    bool     flag_network_cfg = false;
    gw_cfg_t gw_cfg           = get_gateway_config_default();
    ASSERT_TRUE(
        json_ruuvi_parse_http_body(
            "{"
            "\"remote_cfg_use\":false,\n"
            "\"remote_cfg_url\":\"\",\n"
            "\"remote_cfg_auth_type\":\"none\",\n"
            "\"remote_cfg_refresh_interval_minutes\":0,\n"
            "\"use_mqtt\":true,"
            "\"mqtt_disable_retained_messages\":false,"
            "\"mqtt_server\":\"test.mosquitto.org\","
            "\"mqtt_port\":1883,"
            "\"mqtt_sending_interval\":0,"
            "\"mqtt_prefix\":\"ruuvi/30:AE:A4:02:84:A4\","
            "\"mqtt_client_id\":\"30:AE:A4:02:84:A4\","
            "\"mqtt_user\":\"\","
            "\"mqtt_pass\":\"\","
            "\"use_http_ruuvi\":false,"
            "\"use_http\":false,"
            "\"http_url\":\"" RUUVI_GATEWAY_HTTP_DEFAULT_URL "\","
            "\"http_user\":\"\","
            "\"http_pass\":\"\","
            "\"use_http_stat\":true,"
            "\"http_stat_url\":\"" RUUVI_GATEWAY_HTTP_STATUS_URL "\","
            "\"http_stat_user\":\"\","
            "\"http_stat_pass\":\"\","
            "\"lan_auth_api_key\":\"6kl/fd/c+3qvWm3Mhmwgh3BWNp+HDRQiLp/X0PuwG8Q=\","
            "\"ntp_use\":true,"
            "\"ntp_use_dhcp\":false,"
            "\"ntp_server1\":\"time1.server.com\","
            "\"ntp_server2\":\"time2.server.com\","
            "\"ntp_server3\":\"time3.server.com\","
            "\"ntp_server4\":\"time4.server.com\","
            "\"company_use_filtering\":true,"
            "\"fw_update_url\":\"https://network.ruuvi.com/firmwareupdate\""
            "}",
            &gw_cfg,
            &flag_network_cfg));
    ASSERT_FALSE(flag_network_cfg);
    gw_cfg_update_ruuvi_cfg(&gw_cfg.ruuvi_cfg);

    esp_log_wrapper_clear();
    http_server_resp_t resp = http_server_cb_on_get("ruuvi.json", nullptr, false, nullptr);

    ASSERT_EQ(HTTP_RESP_CODE_200, resp.http_resp_code);
    ASSERT_EQ(HTTP_CONTENT_LOCATION_HEAP, resp.content_location);
    ASSERT_TRUE(resp.flag_no_cache);
    ASSERT_EQ(HTTP_CONTENT_TYPE_APPLICATION_JSON, resp.content_type);
    ASSERT_EQ(nullptr, resp.p_content_type_param);
    ASSERT_NE(nullptr, resp.select_location.heap.p_buf);
    ASSERT_EQ(string(expected_json), string(reinterpret_cast<const char*>(resp.select_location.heap.p_buf)));
    ASSERT_EQ(strlen(expected_json), resp.content_len);
    ASSERT_EQ(HTTP_CONTENT_ENCODING_NONE, resp.content_encoding);
    TEST_CHECK_LOG_RECORD_HTTP_SERVER(ESP_LOG_DEBUG, string("http_server_cb_on_get /ruuvi.json"));
    TEST_CHECK_LOG_RECORD_HTTP_SERVER(ESP_LOG_INFO, string("ruuvi.json: ") + string(expected_json));
    TEST_CHECK_LOG_RECORD_HTTP_SERVER(ESP_LOG_INFO, string("Activate cfg_mode"));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    http_server_resp_free(&resp);
    os_malloc_trace_dump();
    ESP_LOG_WRAPPER_TEST_CHECK_LOG_RECORD("MEM_TRACE", ESP_LOG_INFO, "Num blocks allocated: 0");
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_EQ(0, this->m_alloc_free_call_count);
}

TEST_F(TestHttpServerCb, http_server_cb_on_get_ruuvi_json_during_fw_update) // NOLINT
{
    this->m_fw_update_is_in_progress = true;

    const char* expected_json
        = "{\n"
          "\t\"fw_ver\":\t\"v1.3.3\",\n"
          "\t\"nrf52_fw_ver\":\t\"v0.7.1\",\n"
          "\t\"gw_mac\":\t\"11:22:33:44:55:66\",\n"
          "\t\"storage\":\t{\n"
          "\t\t\"storage_ready\":\tfalse\n"
          "\t},\n"
          "\t\"wifi_sta_config\":\t{\n"
          "\t\t\"ssid\":\t\"\"\n"
          "\t},\n"
          "\t\"wifi_ap_config\":\t{\n"
          "\t\t\"channel\":\t1\n"
          "\t},\n"
          "\t\"use_eth\":\ttrue,\n"
          "\t\"eth_dhcp\":\ttrue,\n"
          "\t\"eth_static_ip\":\t\"\",\n"
          "\t\"eth_netmask\":\t\"\",\n"
          "\t\"eth_gw\":\t\"\",\n"
          "\t\"eth_dns1\":\t\"\",\n"
          "\t\"eth_dns2\":\t\"\",\n"
          "\t\"remote_cfg_use\":\tfalse,\n"
          "\t\"remote_cfg_url\":\t\"\",\n"
          "\t\"remote_cfg_auth_type\":\t\"none\",\n"
          "\t\"remote_cfg_use_ssl_client_cert\":\tfalse,\n"
          "\t\"remote_cfg_use_ssl_server_cert\":\tfalse,\n"
          "\t\"remote_cfg_refresh_interval_minutes\":\t0,\n"
          "\t\"use_http_ruuvi\":\tfalse,\n"
          "\t\"use_http\":\tfalse,\n"
          "\t\"http_url\":\t\"" RUUVI_GATEWAY_HTTP_DEFAULT_URL
          "\",\n"
          "\t\"http_period\":\t10,\n"
          "\t\"http_data_format\":\t\"ruuvi\",\n"
          "\t\"http_auth\":\t\"none\",\n"
          "\t\"http_use_ssl_client_cert\":\tfalse,\n"
          "\t\"http_use_ssl_server_cert\":\tfalse,\n"
          "\t\"http_use_extra_http_path\":\tfalse,\n"
          "\t\"http_use_extra_http_query\":\tfalse,\n"
          "\t\"http_use_extra_http_headers\":\tfalse,\n"
          "\t\"use_http_stat\":\ttrue,\n"
          "\t\"http_stat_url\":\t\"" RUUVI_GATEWAY_HTTP_STATUS_URL
          "\",\n"
          "\t\"http_stat_user\":\t\"\",\n"
          "\t\"http_stat_use_ssl_client_cert\":\tfalse,\n"
          "\t\"http_stat_use_ssl_server_cert\":\tfalse,\n"
          "\t\"use_mqtt\":\ttrue,\n"
          "\t\"mqtt_disable_retained_messages\":\tfalse,\n"
          "\t\"mqtt_transport\":\t\"TCP\",\n"
          "\t\"mqtt_data_format\":\t\"ruuvi_raw\",\n"
          "\t\"mqtt_server\":\t\"test.mosquitto.org\",\n"
          "\t\"mqtt_port\":\t1883,\n"
          "\t\"mqtt_sending_interval\":\t0,\n"
          "\t\"mqtt_prefix\":\t\"ruuvi/30:AE:A4:02:84:A4\",\n"
          "\t\"mqtt_client_id\":\t\"30:AE:A4:02:84:A4\",\n"
          "\t\"mqtt_user\":\t\"\",\n"
          "\t\"mqtt_use_ssl_client_cert\":\tfalse,\n"
          "\t\"mqtt_use_ssl_server_cert\":\tfalse,\n"
          "\t\"lan_auth_type\":\t\"lan_auth_default\",\n"
          "\t\"lan_auth_user\":\t\"Admin\",\n"
          "\t\"lan_auth_api_key_use\":\ttrue,\n"
          "\t\"lan_auth_api_key_rw_use\":\tfalse,\n"
          "\t\"auto_update_cycle\":\t\"regular\",\n"
          "\t\"auto_update_weekdays_bitmask\":\t127,\n"
          "\t\"auto_update_interval_from\":\t0,\n"
          "\t\"auto_update_interval_to\":\t24,\n"
          "\t\"auto_update_tz_offset_hours\":\t3,\n"
          "\t\"ntp_use\":\ttrue,\n"
          "\t\"ntp_use_dhcp\":\tfalse,\n"
          "\t\"ntp_server1\":\t\"time1.server.com\",\n"
          "\t\"ntp_server2\":\t\"time2.server.com\",\n"
          "\t\"ntp_server3\":\t\"time3.server.com\",\n"
          "\t\"ntp_server4\":\t\"time4.server.com\",\n"
          "\t\"company_id\":\t1177,\n"
          "\t\"company_use_filtering\":\ttrue,\n"
          "\t\"scan_coded_phy\":\tfalse,\n"
          "\t\"scan_1mbit_phy\":\ttrue,\n"
          "\t\"scan_2mbit_phy\":\ttrue,\n"
          "\t\"scan_channel_37\":\ttrue,\n"
          "\t\"scan_channel_38\":\ttrue,\n"
          "\t\"scan_channel_39\":\ttrue,\n"
          "\t\"scan_default\":\ttrue,\n"
          "\t\"scan_filter_allow_listed\":\tfalse,\n"
          "\t\"scan_filter_list\":\t[],\n"
          "\t\"coordinates\":\t\"\",\n"
          "\t\"fw_update_url\":\t\"https://network.ruuvi.com/firmwareupdate\"\n"
          "}";
    bool     flag_network_cfg = false;
    gw_cfg_t gw_cfg           = get_gateway_config_default();
    ASSERT_TRUE(
        json_ruuvi_parse_http_body(
            "{"
            "\"remote_cfg_use\":false,\n"
            "\"remote_cfg_url\":\"\",\n"
            "\"remote_cfg_auth_type\":\"none\",\n"
            "\"remote_cfg_refresh_interval_minutes\":0,\n"
            "\"use_mqtt\":true,"
            "\"mqtt_disable_retained_messages\":false,"
            "\"mqtt_server\":\"test.mosquitto.org\","
            "\"mqtt_port\":1883,"
            "\"mqtt_sending_interval\":0,"
            "\"mqtt_prefix\":\"ruuvi/30:AE:A4:02:84:A4\","
            "\"mqtt_client_id\":\"30:AE:A4:02:84:A4\","
            "\"mqtt_user\":\"\","
            "\"mqtt_pass\":\"\","
            "\"use_http_ruuvi\":false,"
            "\"use_http\":false,"
            "\"http_url\":\"" RUUVI_GATEWAY_HTTP_DEFAULT_URL "\","
            "\"http_user\":\"\","
            "\"http_pass\":\"\","
            "\"use_http_stat\":true,"
            "\"http_stat_url\":\"" RUUVI_GATEWAY_HTTP_STATUS_URL "\","
            "\"http_stat_user\":\"\","
            "\"http_stat_pass\":\"\","
            "\"lan_auth_api_key\":\"6kl/fd/c+3qvWm3Mhmwgh3BWNp+HDRQiLp/X0PuwG8Q=\","
            "\"ntp_use\":true,"
            "\"ntp_use_dhcp\":false,"
            "\"ntp_server1\":\"time1.server.com\","
            "\"ntp_server2\":\"time2.server.com\","
            "\"ntp_server3\":\"time3.server.com\","
            "\"ntp_server4\":\"time4.server.com\","
            "\"company_use_filtering\":true,"
            "\"fw_update_url\":\"https://network.ruuvi.com/firmwareupdate\""
            "}",
            &gw_cfg,
            &flag_network_cfg));
    ASSERT_FALSE(flag_network_cfg);
    gw_cfg_update_ruuvi_cfg(&gw_cfg.ruuvi_cfg);

    esp_log_wrapper_clear();
    http_server_resp_t resp = http_server_cb_on_get("ruuvi.json", nullptr, false, nullptr);

    ASSERT_EQ(HTTP_RESP_CODE_200, resp.http_resp_code);
    ASSERT_EQ(HTTP_CONTENT_LOCATION_HEAP, resp.content_location);
    ASSERT_TRUE(resp.flag_no_cache);
    ASSERT_EQ(HTTP_CONTENT_TYPE_APPLICATION_JSON, resp.content_type);
    ASSERT_EQ(nullptr, resp.p_content_type_param);
    ASSERT_NE(nullptr, resp.select_location.heap.p_buf);
    ASSERT_EQ(string(expected_json), string(reinterpret_cast<const char*>(resp.select_location.heap.p_buf)));
    ASSERT_EQ(strlen(expected_json), resp.content_len);
    ASSERT_EQ(HTTP_CONTENT_ENCODING_NONE, resp.content_encoding);
    TEST_CHECK_LOG_RECORD_HTTP_SERVER(ESP_LOG_DEBUG, string("http_server_cb_on_get /ruuvi.json"));
    TEST_CHECK_LOG_RECORD_HTTP_SERVER(ESP_LOG_INFO, string("ruuvi.json: ") + string(expected_json));
    TEST_CHECK_LOG_RECORD_HTTP_SERVER(ESP_LOG_INFO, string("Activate cfg_mode"));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    http_server_resp_free(&resp);
    os_malloc_trace_dump();
    ESP_LOG_WRAPPER_TEST_CHECK_LOG_RECORD("MEM_TRACE", ESP_LOG_INFO, "Num blocks allocated: 0");
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_EQ(0, this->m_alloc_free_call_count);
}

TEST_F(TestHttpServerCb, http_server_cb_on_get_metrics) // NOLINT
{
    const char*        expected_resp = "metrics_info";
    http_server_resp_t resp          = http_server_cb_on_get("metrics", nullptr, false, nullptr);

    ASSERT_EQ(HTTP_RESP_CODE_200, resp.http_resp_code);
    ASSERT_EQ(HTTP_CONTENT_LOCATION_HEAP, resp.content_location);
    ASSERT_TRUE(resp.flag_no_cache);
    ASSERT_EQ(HTTP_CONTENT_TYPE_TEXT_PLAIN, resp.content_type);
    ASSERT_NE(nullptr, resp.p_content_type_param);
    ASSERT_EQ(string("version=0.0.4"), string(resp.p_content_type_param));
    ASSERT_EQ(strlen(expected_resp), resp.content_len);
    ASSERT_EQ(HTTP_CONTENT_ENCODING_NONE, resp.content_encoding);
    ASSERT_NE(nullptr, resp.select_location.heap.p_buf);
    ASSERT_EQ(string(expected_resp), string(reinterpret_cast<const char*>(resp.select_location.heap.p_buf)));
    TEST_CHECK_LOG_RECORD_HTTP_SERVER(ESP_LOG_DEBUG, string("http_server_cb_on_get /metrics"));
    TEST_CHECK_LOG_RECORD_HTTP_SERVER(ESP_LOG_INFO, string("metrics: ") + string(expected_resp));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    http_server_resp_free(&resp);
    os_malloc_trace_dump();
    ESP_LOG_WRAPPER_TEST_CHECK_LOG_RECORD("MEM_TRACE", ESP_LOG_INFO, "Num blocks allocated: 0");
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_EQ(0, this->m_alloc_free_call_count);
}

TEST_F(TestHttpServerCb, http_server_cb_on_get_metrics_during_fw_update) // NOLINT
{
    this->m_fw_update_is_in_progress = true;
    const char*        expected_resp = "metrics_info";
    http_server_resp_t resp          = http_server_cb_on_get("metrics", nullptr, false, nullptr);

    ASSERT_EQ(HTTP_RESP_CODE_200, resp.http_resp_code);
    ASSERT_EQ(HTTP_CONTENT_LOCATION_HEAP, resp.content_location);
    ASSERT_TRUE(resp.flag_no_cache);
    ASSERT_EQ(HTTP_CONTENT_TYPE_TEXT_PLAIN, resp.content_type);
    ASSERT_NE(nullptr, resp.p_content_type_param);
    ASSERT_EQ(string("version=0.0.4"), string(resp.p_content_type_param));
    ASSERT_EQ(strlen(expected_resp), resp.content_len);
    ASSERT_EQ(HTTP_CONTENT_ENCODING_NONE, resp.content_encoding);
    ASSERT_NE(nullptr, resp.select_location.heap.p_buf);
    ASSERT_EQ(string(expected_resp), string(reinterpret_cast<const char*>(resp.select_location.heap.p_buf)));
    TEST_CHECK_LOG_RECORD_HTTP_SERVER(ESP_LOG_DEBUG, string("http_server_cb_on_get /metrics"));
    TEST_CHECK_LOG_RECORD_HTTP_SERVER(ESP_LOG_INFO, string("metrics: ") + string(expected_resp));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    http_server_resp_free(&resp);
    os_malloc_trace_dump();
    ESP_LOG_WRAPPER_TEST_CHECK_LOG_RECORD("MEM_TRACE", ESP_LOG_INFO, "Num blocks allocated: 0");
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_EQ(0, this->m_alloc_free_call_count);
}

TEST_F(TestHttpServerCb, http_server_cb_on_get_history) // NOLINT
{
    const char* expected_resp
        = "{\n"
          "  \"data\": {\n"
          "    \"coordinates\": \"\",\n"
          "    \"timestamp\": 1615660220,\n"
          "    \"gw_mac\": \"11:22:33:44:55:66\",\n"
          "    \"tags\": {\n"
          "      \"AA:BB:CC:11:22:01\": {\n"
          "        \"rssi\": 50,\n"
          "        \"timestamp\": 1615660219,\n"
          "        \"ble_phy\": \"1M\",\n"
          "        \"ble_chan\": 37,\n"
          "        \"data\": \"2233\"\n"
          "      },\n"
          "      \"AA:BB:CC:11:22:02\": {\n"
          "        \"rssi\": 51,\n"
          "        \"timestamp\": 1615660209,\n"
          "        \"ble_phy\": \"2M\",\n"
          "        \"ble_chan\": 25,\n"
          "        \"ble_tx_power\": 8,\n"
          "        \"data\": \"223344\"\n"
          "      }\n"
          "    }\n"
          "  }\n"
          "}";
    http_server_resp_t resp = http_server_cb_on_get("history", nullptr, false, nullptr);

    ASSERT_EQ(HTTP_RESP_CODE_200, resp.http_resp_code);
    ASSERT_EQ(HTTP_CONTENT_LOCATION_JSON_GENERATOR, resp.content_location);
    ASSERT_TRUE(resp.flag_no_cache);
    ASSERT_EQ(HTTP_CONTENT_TYPE_APPLICATION_JSON, resp.content_type);
    ASSERT_EQ(nullptr, resp.p_content_type_param);
    ASSERT_EQ(HTTP_CONTENT_ENCODING_NONE, resp.content_encoding);

    ASSERT_NE(nullptr, resp.select_location.json_generator.p_json_gen);
    string json_str("");
    while (true)
    {
        const char* p_chunk = json_stream_gen_get_next_chunk(resp.select_location.json_generator.p_json_gen);
        if (nullptr == p_chunk)
        {
            ASSERT_NE(nullptr, p_chunk);
        }

        if ('\0' == p_chunk[0])
        {
            break;
        }
        json_str += string(p_chunk);
    }
    ASSERT_EQ(string(expected_resp), json_str);

    ASSERT_EQ(strlen(expected_resp), resp.content_len);
    TEST_CHECK_LOG_RECORD_HTTP_SERVER(ESP_LOG_DEBUG, string("http_server_cb_on_get /history"));
    TEST_CHECK_LOG_RECORD_HTTP_SERVER(ESP_LOG_INFO, string("Requested /history on 60 seconds interval"));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    http_server_resp_free(&resp);
    os_malloc_trace_dump();
    ESP_LOG_WRAPPER_TEST_CHECK_LOG_RECORD("MEM_TRACE", ESP_LOG_INFO, "Num blocks allocated: 0");
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_EQ(0, this->m_alloc_free_call_count);
}

TEST_F(TestHttpServerCb, http_server_cb_on_get_history_during_fw_update) // NOLINT
{
    this->m_fw_update_is_in_progress = true;
    const char* expected_resp
        = "{\n"
          "  \"data\": {\n"
          "    \"coordinates\": \"\",\n"
          "    \"timestamp\": 1615660220,\n"
          "    \"gw_mac\": \"11:22:33:44:55:66\",\n"
          "    \"tags\": {\n"
          "      \"AA:BB:CC:11:22:01\": {\n"
          "        \"rssi\": 50,\n"
          "        \"timestamp\": 1615660219,\n"
          "        \"ble_phy\": \"1M\",\n"
          "        \"ble_chan\": 37,\n"
          "        \"data\": \"2233\"\n"
          "      },\n"
          "      \"AA:BB:CC:11:22:02\": {\n"
          "        \"rssi\": 51,\n"
          "        \"timestamp\": 1615660209,\n"
          "        \"ble_phy\": \"2M\",\n"
          "        \"ble_chan\": 25,\n"
          "        \"ble_tx_power\": 8,\n"
          "        \"data\": \"223344\"\n"
          "      }\n"
          "    }\n"
          "  }\n"
          "}";
    http_server_resp_t resp = http_server_cb_on_get("history", nullptr, false, nullptr);

    ASSERT_EQ(HTTP_RESP_CODE_200, resp.http_resp_code);
    ASSERT_EQ(HTTP_CONTENT_LOCATION_JSON_GENERATOR, resp.content_location);
    ASSERT_TRUE(resp.flag_no_cache);
    ASSERT_EQ(HTTP_CONTENT_TYPE_APPLICATION_JSON, resp.content_type);
    ASSERT_EQ(nullptr, resp.p_content_type_param);
    ASSERT_EQ(HTTP_CONTENT_ENCODING_NONE, resp.content_encoding);

    ASSERT_NE(nullptr, resp.select_location.json_generator.p_json_gen);
    string json_str("");
    while (true)
    {
        const char* p_chunk = json_stream_gen_get_next_chunk(resp.select_location.json_generator.p_json_gen);
        if (nullptr == p_chunk)
        {
            ASSERT_NE(nullptr, p_chunk);
        }

        if ('\0' == p_chunk[0])
        {
            break;
        }
        json_str += string(p_chunk);
    }
    ASSERT_EQ(string(expected_resp), json_str);

    ASSERT_EQ(strlen(expected_resp), resp.content_len);
    TEST_CHECK_LOG_RECORD_HTTP_SERVER(ESP_LOG_DEBUG, string("http_server_cb_on_get /history"));
    TEST_CHECK_LOG_RECORD_HTTP_SERVER(ESP_LOG_INFO, string("Requested /history on 60 seconds interval"));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    http_server_resp_free(&resp);
    os_malloc_trace_dump();
    ESP_LOG_WRAPPER_TEST_CHECK_LOG_RECORD("MEM_TRACE", ESP_LOG_INFO, "Num blocks allocated: 0");
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_EQ(0, this->m_alloc_free_call_count);
}

TEST_F(TestHttpServerCb, http_server_cb_on_get_history_with_time_interval_20) // NOLINT
{
    const char* expected_resp
        = "{\n"
          "  \"data\": {\n"
          "    \"coordinates\": \"\",\n"
          "    \"timestamp\": 1615660220,\n"
          "    \"gw_mac\": \"11:22:33:44:55:66\",\n"
          "    \"tags\": {\n"
          "      \"AA:BB:CC:11:22:01\": {\n"
          "        \"rssi\": 50,\n"
          "        \"timestamp\": 1615660219,\n"
          "        \"ble_phy\": \"1M\",\n"
          "        \"ble_chan\": 37,\n"
          "        \"data\": \"2233\"\n"
          "      },\n"
          "      \"AA:BB:CC:11:22:02\": {\n"
          "        \"rssi\": 51,\n"
          "        \"timestamp\": 1615660209,\n"
          "        \"ble_phy\": \"2M\",\n"
          "        \"ble_chan\": 25,\n"
          "        \"ble_tx_power\": 8,\n"
          "        \"data\": \"223344\"\n"
          "      }\n"
          "    }\n"
          "  }\n"
          "}";
    http_server_resp_t resp = http_server_cb_on_get("history", "time=20", false, nullptr);

    ASSERT_EQ(HTTP_RESP_CODE_200, resp.http_resp_code);
    ASSERT_EQ(HTTP_CONTENT_LOCATION_JSON_GENERATOR, resp.content_location);
    ASSERT_TRUE(resp.flag_no_cache);
    ASSERT_EQ(HTTP_CONTENT_TYPE_APPLICATION_JSON, resp.content_type);
    ASSERT_EQ(nullptr, resp.p_content_type_param);
    ASSERT_EQ(HTTP_CONTENT_ENCODING_NONE, resp.content_encoding);

    ASSERT_NE(nullptr, resp.select_location.json_generator.p_json_gen);
    string json_str("");
    while (true)
    {
        const char* p_chunk = json_stream_gen_get_next_chunk(resp.select_location.json_generator.p_json_gen);
        if (nullptr == p_chunk)
        {
            ASSERT_NE(nullptr, p_chunk);
        }

        if ('\0' == p_chunk[0])
        {
            break;
        }
        json_str += string(p_chunk);
    }
    ASSERT_EQ(string(expected_resp), json_str);
    ASSERT_EQ(strlen(expected_resp), resp.content_len);

    TEST_CHECK_LOG_RECORD_HTTP_SERVER(ESP_LOG_DEBUG, string("http_server_cb_on_get /history?time=20"));
    TEST_CHECK_LOG_RECORD_HTTP_SERVER(ESP_LOG_DEBUG, string("HTTP params: time=20"));
    TEST_CHECK_LOG_RECORD_HTTP_SERVER(ESP_LOG_DEBUG, string("Can't find key 'decode=' in URL params"));
    TEST_CHECK_LOG_RECORD_HTTP_SERVER(ESP_LOG_INFO, string("Requested /history on 20 seconds interval"));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    http_server_resp_free(&resp);
    os_malloc_trace_dump();
    ESP_LOG_WRAPPER_TEST_CHECK_LOG_RECORD("MEM_TRACE", ESP_LOG_INFO, "Num blocks allocated: 0");
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_EQ(0, this->m_alloc_free_call_count);
}

TEST_F(TestHttpServerCb, http_server_cb_on_get_history_without_timestamps) // NOLINT
{
    gw_cfg_t gw_cfg_tmp = { 0 };
    gw_cfg_get_copy(&gw_cfg_tmp);
    gw_cfg_tmp.ruuvi_cfg.ntp.ntp_use = false;
    gw_cfg_update_ruuvi_cfg(&gw_cfg_tmp.ruuvi_cfg);

    const char* expected_resp
        = "{\n"
          "  \"data\": {\n"
          "    \"coordinates\": \"\",\n"
          "    \"gw_mac\": \"11:22:33:44:55:66\",\n"
          "    \"tags\": {\n"
          "      \"AA:BB:CC:11:22:01\": {\n"
          "        \"rssi\": 50,\n"
          "        \"counter\": 1615660219,\n"
          "        \"ble_phy\": \"1M\",\n"
          "        \"ble_chan\": 37,\n"
          "        \"data\": \"2233\"\n"
          "      },\n"
          "      \"AA:BB:CC:11:22:02\": {\n"
          "        \"rssi\": 51,\n"
          "        \"counter\": 1615660209,\n"
          "        \"ble_phy\": \"2M\",\n"
          "        \"ble_chan\": 25,\n"
          "        \"ble_tx_power\": 8,\n"
          "        \"data\": \"223344\"\n"
          "      }\n"
          "    }\n"
          "  }\n"
          "}";
    http_server_resp_t resp = http_server_cb_on_get("history", nullptr, false, nullptr);

    ASSERT_EQ(HTTP_RESP_CODE_200, resp.http_resp_code);
    ASSERT_EQ(HTTP_CONTENT_LOCATION_JSON_GENERATOR, resp.content_location);
    ASSERT_TRUE(resp.flag_no_cache);
    ASSERT_EQ(HTTP_CONTENT_TYPE_APPLICATION_JSON, resp.content_type);
    ASSERT_EQ(nullptr, resp.p_content_type_param);
    ASSERT_EQ(HTTP_CONTENT_ENCODING_NONE, resp.content_encoding);

    ASSERT_NE(nullptr, resp.select_location.json_generator.p_json_gen);
    string json_str("");
    while (true)
    {
        const char* p_chunk = json_stream_gen_get_next_chunk(resp.select_location.json_generator.p_json_gen);
        if (nullptr == p_chunk)
        {
            ASSERT_NE(nullptr, p_chunk);
        }

        if ('\0' == p_chunk[0])
        {
            break;
        }
        json_str += string(p_chunk);
    }
    ASSERT_EQ(string(expected_resp), json_str);
    ASSERT_EQ(strlen(expected_resp), resp.content_len);

    TEST_CHECK_LOG_RECORD_HTTP_SERVER(ESP_LOG_DEBUG, string("http_server_cb_on_get /history"));
    TEST_CHECK_LOG_RECORD_HTTP_SERVER(ESP_LOG_INFO, string("Requested /history (without filtering)"));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    http_server_resp_free(&resp);
    os_malloc_trace_dump();
    ESP_LOG_WRAPPER_TEST_CHECK_LOG_RECORD("MEM_TRACE", ESP_LOG_INFO, "Num blocks allocated: 0");
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_EQ(0, this->m_alloc_free_call_count);
}

TEST_F(TestHttpServerCb, http_server_cb_on_get_history_without_timestamps_with_filter_counter_10) // NOLINT
{
    gw_cfg_t gw_cfg_tmp = { 0 };
    gw_cfg_get_copy(&gw_cfg_tmp);
    gw_cfg_tmp.ruuvi_cfg.ntp.ntp_use = false;
    gw_cfg_update_ruuvi_cfg(&gw_cfg_tmp.ruuvi_cfg);

    const char* expected_resp
        = "{\n"
          "  \"data\": {\n"
          "    \"coordinates\": \"\",\n"
          "    \"gw_mac\": \"11:22:33:44:55:66\",\n"
          "    \"tags\": {\n"
          "      \"AA:BB:CC:11:22:01\": {\n"
          "        \"rssi\": 50,\n"
          "        \"counter\": 1615660219,\n"
          "        \"ble_phy\": \"1M\",\n"
          "        \"ble_chan\": 37,\n"
          "        \"data\": \"2233\"\n"
          "      },\n"
          "      \"AA:BB:CC:11:22:02\": {\n"
          "        \"rssi\": 51,\n"
          "        \"counter\": 1615660209,\n"
          "        \"ble_phy\": \"2M\",\n"
          "        \"ble_chan\": 25,\n"
          "        \"ble_tx_power\": 8,\n"
          "        \"data\": \"223344\"\n"
          "      }\n"
          "    }\n"
          "  }\n"
          "}";
    http_server_resp_t resp = http_server_cb_on_get("history", "counter=10", false, nullptr);

    ASSERT_EQ(HTTP_RESP_CODE_200, resp.http_resp_code);
    ASSERT_EQ(HTTP_CONTENT_LOCATION_JSON_GENERATOR, resp.content_location);
    ASSERT_TRUE(resp.flag_no_cache);
    ASSERT_EQ(HTTP_CONTENT_TYPE_APPLICATION_JSON, resp.content_type);
    ASSERT_EQ(nullptr, resp.p_content_type_param);
    ASSERT_EQ(HTTP_CONTENT_ENCODING_NONE, resp.content_encoding);

    ASSERT_NE(nullptr, resp.select_location.json_generator.p_json_gen);
    string json_str("");
    while (true)
    {
        const char* p_chunk = json_stream_gen_get_next_chunk(resp.select_location.json_generator.p_json_gen);
        if (nullptr == p_chunk)
        {
            ASSERT_NE(nullptr, p_chunk);
        }

        if ('\0' == p_chunk[0])
        {
            break;
        }
        json_str += string(p_chunk);
    }
    ASSERT_EQ(string(expected_resp), json_str);
    ASSERT_EQ(strlen(expected_resp), resp.content_len);

    TEST_CHECK_LOG_RECORD_HTTP_SERVER(ESP_LOG_DEBUG, string("http_server_cb_on_get /history?counter=10"));
    TEST_CHECK_LOG_RECORD_HTTP_SERVER(ESP_LOG_DEBUG, string("HTTP params: counter=10"));
    TEST_CHECK_LOG_RECORD_HTTP_SERVER(ESP_LOG_DEBUG, string("Can't find key 'decode=' in URL params"));
    TEST_CHECK_LOG_RECORD_HTTP_SERVER(ESP_LOG_INFO, string("Requested /history starting from counter 10"));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    http_server_resp_free(&resp);
    os_malloc_trace_dump();
    ESP_LOG_WRAPPER_TEST_CHECK_LOG_RECORD("MEM_TRACE", ESP_LOG_INFO, "Num blocks allocated: 0");
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_EQ(0, this->m_alloc_free_call_count);
}

TEST_F(TestHttpServerCb, http_server_cb_on_post_network_cfg) // NOLINT
{
    const bool flag_access_from_lan = false;

    const char*              expected_resp = "{}";
    const http_server_resp_t resp          = http_server_cb_on_post_ruuvi(
        "{"
                 "\"use_eth\":true,\n"
                 "\"eth_dhcp\":true,\n"
                 "\"company_use_filtering\":false\n"
                 "}",
        flag_access_from_lan);

    ASSERT_TRUE(this->m_flag_settings_saved_to_flash);
    ASSERT_TRUE(this->m_flag_settings_ethernet_ip_updated);

    ASSERT_EQ(HTTP_RESP_CODE_200, resp.http_resp_code);
    ASSERT_EQ(HTTP_CONTENT_LOCATION_FLASH_MEM, resp.content_location);
    ASSERT_TRUE(resp.flag_no_cache);
    ASSERT_EQ(HTTP_CONTENT_TYPE_APPLICATION_JSON, resp.content_type);
    ASSERT_EQ(nullptr, resp.p_content_type_param);
    ASSERT_EQ(strlen(expected_resp), resp.content_len);
    ASSERT_EQ(HTTP_CONTENT_ENCODING_NONE, resp.content_encoding);
    ASSERT_NE(nullptr, resp.select_location.flash.p_buf);
    ASSERT_EQ(string(expected_resp), string(reinterpret_cast<const char*>(resp.select_location.flash.p_buf)));
    TEST_CHECK_LOG_RECORD_HTTP_SERVER(ESP_LOG_DEBUG, string("POST /ruuvi.json, flag_access_from_lan=0"));
    TEST_CHECK_LOG_RECORD_HTTP_SERVER(
        ESP_LOG_DEBUG,
        string("POST /ruuvi.json, body: {\"use_eth\":true,\n\"eth_dhcp\":true,\n\"company_use_filtering\":false\n}"));
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, "Gateway SETTINGS (via HTTP):");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "use_eth: 1");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "eth_dhcp: 1");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, "config: Use eth: yes");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, "config: eth: use DHCP: yes");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, "config: eth: static IP: ");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, "config: eth: netmask: ");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, "config: eth: GW: ");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, "config: eth: DNS1: ");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, "config: eth: DNS2: ");
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    os_malloc_trace_dump();
    ESP_LOG_WRAPPER_TEST_CHECK_LOG_RECORD("MEM_TRACE", ESP_LOG_INFO, "Num blocks allocated: 0");
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_EQ(0, this->m_alloc_free_call_count);
}

TEST_F(TestHttpServerCb, http_server_cb_on_post_network_cfg_from_lan) // NOLINT
{
    const bool flag_access_from_lan = true;

    const char*              expected_resp = "{}";
    const http_server_resp_t resp          = http_server_cb_on_post_ruuvi(
        "{"
                 "\"use_eth\":true,\n"
                 "\"eth_dhcp\":true,\n"
                 "\"company_use_filtering\":false\n"
                 "}",
        flag_access_from_lan);

    ASSERT_TRUE(this->m_flag_settings_saved_to_flash);
    ASSERT_FALSE(this->m_flag_settings_ethernet_ip_updated);

    ASSERT_EQ(HTTP_RESP_CODE_200, resp.http_resp_code);
    ASSERT_EQ(HTTP_CONTENT_LOCATION_FLASH_MEM, resp.content_location);
    ASSERT_TRUE(resp.flag_no_cache);
    ASSERT_EQ(HTTP_CONTENT_TYPE_APPLICATION_JSON, resp.content_type);
    ASSERT_EQ(nullptr, resp.p_content_type_param);
    ASSERT_EQ(strlen(expected_resp), resp.content_len);
    ASSERT_EQ(HTTP_CONTENT_ENCODING_NONE, resp.content_encoding);
    ASSERT_NE(nullptr, resp.select_location.flash.p_buf);
    ASSERT_EQ(string(expected_resp), string(reinterpret_cast<const char*>(resp.select_location.flash.p_buf)));
    TEST_CHECK_LOG_RECORD_HTTP_SERVER(ESP_LOG_DEBUG, string("POST /ruuvi.json, flag_access_from_lan=1"));
    TEST_CHECK_LOG_RECORD_HTTP_SERVER(
        ESP_LOG_DEBUG,
        string("POST /ruuvi.json, body: {\"use_eth\":true,\n\"eth_dhcp\":true,\n\"company_use_filtering\":false\n}"));
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, "Gateway SETTINGS (via HTTP):");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "remote_cfg_use: not found");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_WARN, "Can't find key 'remote_cfg_use' in config-json");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "remote_cfg_url: not found");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_WARN, "Can't find key 'remote_cfg_url' in config-json");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "remote_cfg_auth_type: not found");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_WARN, "Can't find key 'remote_cfg_auth_type' in config-json");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "remote_cfg_use_ssl_client_cert: not found");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_WARN, "Can't find key 'remote_cfg_use_ssl_client_cert' in config-json");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "remote_cfg_use_ssl_server_cert: not found");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_WARN, "Can't find key 'remote_cfg_use_ssl_server_cert' in config-json");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "remote_cfg_refresh_interval_minutes: not found or invalid");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_WARN, "Can't find key 'remote_cfg_refresh_interval_minutes' in config-json");

    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "use_http_ruuvi: not found");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_WARN, "Can't find key 'use_http_ruuvi' in config-json");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "use_http: not found");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_WARN, "Can't find key 'use_http' in config-json");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "http_data_format: not found");
    TEST_CHECK_LOG_RECORD_GW_CFG(
        ESP_LOG_DEBUG,
        "Can't find key 'http_data_format' in config-json, use default value 0");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "http_auth: not found");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "Can't find key 'http_auth' in config-json, use default value 0");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "http_url: not found");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "Can't find key 'http_url' in config-json");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "http_period: not found or invalid");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "Can't find key 'http_period' in config-json");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "http_use_extra_http_path: not found");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "Can't find key 'http_use_extra_http_path' in config-json");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "http_use_extra_http_query: not found");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "Can't find key 'http_use_extra_http_query' in config-json");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "http_use_extra_http_headers: not found");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "Can't find key 'http_use_extra_http_headers' in config-json");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "http_use_ssl_client_cert: not found");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "Can't find key 'http_use_ssl_client_cert' in config-json");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "http_use_ssl_server_cert: not found");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "Can't find key 'http_use_ssl_server_cert' in config-json");

    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "use_http_stat: not found");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_WARN, "Can't find key 'use_http_stat' in config-json");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "http_stat_url: not found");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_WARN, "Can't find key 'http_stat_url' in config-json");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "http_stat_user: not found");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_WARN, "Can't find key 'http_stat_user' in config-json");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "http_stat_pass: not found");
    TEST_CHECK_LOG_RECORD_GW_CFG(
        ESP_LOG_INFO,
        "Can't find key 'http_stat_pass' in config-json, leave the previous value unchanged");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "http_stat_use_ssl_client_cert: not found");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_WARN, "Can't find key 'http_stat_use_ssl_client_cert' in config-json");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "http_stat_use_ssl_server_cert: not found");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_WARN, "Can't find key 'http_stat_use_ssl_server_cert' in config-json");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "use_mqtt: not found");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_WARN, "Can't find key 'use_mqtt' in config-json");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "mqtt_disable_retained_messages: not found");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_WARN, "Can't find key 'mqtt_disable_retained_messages' in config-json");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "mqtt_transport: not found");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_WARN, "Can't find key 'mqtt_transport' in config-json");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "mqtt_data_format: not found");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_WARN, "Can't find key 'mqtt_data_format' in config-json");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "mqtt_server: not found");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_WARN, "Can't find key 'mqtt_server' in config-json");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "mqtt_port: not found or invalid");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_WARN, "Can't find key 'mqtt_port' in config-json");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "mqtt_sending_interval: not found or invalid");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_WARN, "Can't find key 'mqtt_sending_interval' in config-json");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "mqtt_prefix: not found");
    TEST_CHECK_LOG_RECORD_GW_CFG(
        ESP_LOG_WARN,
        "Can't find key 'mqtt_prefix' in config-json, use default value: ruuvi/11:22:33:44:55:66/");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "mqtt_client_id: not found");
    TEST_CHECK_LOG_RECORD_GW_CFG(
        ESP_LOG_WARN,
        "Can't find key 'mqtt_client_id' in config-json, use default value: 11:22:33:44:55:66");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "mqtt_user: not found");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_WARN, "Can't find key 'mqtt_user' in config-json");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "mqtt_pass: not found");
    TEST_CHECK_LOG_RECORD_GW_CFG(
        ESP_LOG_INFO,
        "Can't find key 'mqtt_pass' in config-json, leave the previous value unchanged");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "lan_auth_type: not found");
    TEST_CHECK_LOG_RECORD_GW_CFG(
        ESP_LOG_INFO,
        "Can't find key 'lan_auth_type' in config-json, leave the previous value unchanged");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "lan_auth_api_key: not found");
    TEST_CHECK_LOG_RECORD_GW_CFG(
        ESP_LOG_INFO,
        "Can't find key 'lan_auth_api_key' in config-json, leave the previous value unchanged");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "lan_auth_api_key_rw: not found");
    TEST_CHECK_LOG_RECORD_GW_CFG(
        ESP_LOG_INFO,
        "Can't find key 'lan_auth_api_key_rw' in config-json, leave the previous value unchanged");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "auto_update_cycle: not found");
    TEST_CHECK_LOG_RECORD_GW_CFG(
        ESP_LOG_WARN,
        "Can't find key 'auto_update_cycle' in config-json, leave the previous value unchanged");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "auto_update_weekdays_bitmask: not found or invalid");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_WARN, "Can't find key 'auto_update_weekdays_bitmask' in config-json");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "auto_update_interval_from: not found or invalid");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_WARN, "Can't find key 'auto_update_interval_from' in config-json");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "auto_update_interval_to: not found or invalid");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_WARN, "Can't find key 'auto_update_interval_to' in config-json");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "auto_update_tz_offset_hours: not found or invalid");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_WARN, "Can't find key 'auto_update_tz_offset_hours' in config-json");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "ntp_use: not found");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_WARN, "Can't find key 'ntp_use' in config-json");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "ntp_use_dhcp: not found");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_WARN, "Can't find key 'ntp_use_dhcp' in config-json");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "ntp_server1: not found");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_WARN, "Can't find key 'ntp_server1' in config-json");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "ntp_server2: not found");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_WARN, "Can't find key 'ntp_server2' in config-json");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "ntp_server3: not found");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_WARN, "Can't find key 'ntp_server3' in config-json");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "ntp_server4: not found");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_WARN, "Can't find key 'ntp_server4' in config-json");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "company_id: not found or invalid");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_WARN, "Can't find key 'company_id' in config-json");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "company_use_filtering: 0");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "scan_default: not found");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_WARN, "Can't find key 'scan_default' in config-json");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "scan_filter_allow_listed: not found");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_WARN, "Can't find key 'scan_filter_allow_listed' in config-json");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_WARN, "Can't find key 'scan_filter_list' in config-json");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "coordinates: not found");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_WARN, "Can't find key 'coordinates' in config-json");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "fw_update_url: not found");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_WARN, "Can't find key 'fw_update_url' in config-json");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, string("config: use remote cfg: 0"));
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, string("config: remote cfg: URL: "));
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, string("config: remote cfg: auth_type: none"));
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, string("config: remote cfg: use SSL client cert: 0"));
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, string("config: remote cfg: use SSL server cert: 0"));
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, string("config: remote cfg: refresh_interval_minutes: 0"));

    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, string("config: use http ruuvi: 1"));
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, string("config: use http: 0"));

    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, string("config: use http_stat: 1"));
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, string("config: http_stat url: " RUUVI_GATEWAY_HTTP_STATUS_URL));
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, string("config: http_stat user: "));
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, string("config: http_stat pass: "));
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, string("config: http_stat: use SSL client cert: 0"));
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, string("config: http_stat: use SSL server cert: 0"));
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, string("config: use mqtt: 0"));
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, string("config: mqtt disable retained messages: 0"));
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, string("config: mqtt transport: TCP"));
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, string("config: mqtt data format: ruuvi_raw"));
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, string("config: mqtt server: test.mosquitto.org"));
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, string("config: mqtt port: 1883"));
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, string("config: mqtt sending interval: 0"));
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, string("config: mqtt prefix: ruuvi/11:22:33:44:55:66/"));
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, string("config: mqtt client id: 11:22:33:44:55:66"));
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, string("config: mqtt user: "));
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, string("config: mqtt password: "));
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, string("config: mqtt: use SSL client cert: 0"));
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, string("config: mqtt: use SSL server cert: 0"));
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, string("config: LAN auth type: lan_auth_default"));
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, string("config: LAN auth user: Admin"));
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, string("config: LAN auth pass: f32dd273cd874d98ec4fc21d534e3e61"));
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, string("config: LAN auth API key: "));
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, string("config: LAN auth API key (RW): "));
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, string("config: Auto update cycle: regular"));
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, string("config: Auto update weekdays_bitmask: 0x7f"));
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, string("config: Auto update interval: 00:00..24:00"));
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, string("config: Auto update TZ: UTC+3"));
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, string("config: NTP: Use: yes"));
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, string("config: NTP: Use DHCP: no"));
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, string("config: NTP: Server1: time.google.com"));
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, string("config: NTP: Server2: time.cloudflare.com"));
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, string("config: NTP: Server3: pool.ntp.org"));
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, string("config: NTP: Server4: time.ruuvi.com"));
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, string("config: use company id filter: 0"));
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, string("config: company id: 0x0499"));
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, string("config: use scan coded phy: 0"));
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, string("config: use scan 1mbit/phy: 1"));
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, string("config: use scan 2mbit/phy: 1"));
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, string("config: use scan channel 37: 1"));
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, string("config: use scan channel 38: 1"));
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, string("config: use scan channel 39: 1"));
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, string("config: scan default       : 1"));
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, string("config: use scan filter: no"));
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, string("config: coordinates: "));
    TEST_CHECK_LOG_RECORD_GW_CFG(
        ESP_LOG_INFO,
        string("config: fw_update: url: https://network.ruuvi.com/firmwareupdate"));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    os_malloc_trace_dump();
    ESP_LOG_WRAPPER_TEST_CHECK_LOG_RECORD("MEM_TRACE", ESP_LOG_INFO, "Num blocks allocated: 0");
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_EQ(0, this->m_alloc_free_call_count);
}

TEST_F(TestHttpServerCb, http_server_cb_on_post_ruuvi_ok_mqtt_tcp) // NOLINT
{
    const bool flag_access_from_lan = false;

    const char*              expected_resp = "{}";
    const http_server_resp_t resp          = http_server_cb_on_post_ruuvi(
        "{"
                 "\"remote_cfg_use\":false,\n"
                 "\"remote_cfg_url\":\"\",\n"
                 "\"remote_cfg_auth_type\":\"none\",\n"
                 "\t\"remote_cfg_use_ssl_client_cert\":\tfalse,\n"
                 "\t\"remote_cfg_use_ssl_server_cert\":\tfalse,\n"
                 "\"remote_cfg_refresh_interval_minutes\":0,\n"
                 "\"use_http_ruuvi\":false,"
                 "\"use_http_ruuvi\":false,"
                 "\"use_http\":false,"
                 "\"http_data_format\":\"ruuvi\","
                 "\"http_auth\":\"none\","
                 "\"http_url\":\"" RUUVI_GATEWAY_HTTP_DEFAULT_URL
        "\","
                 "\"http_user\":\"\","
                 "\"http_pass\":\"\","
                 "\"use_http_stat\":true,"
                 "\"http_stat_url\":\"" RUUVI_GATEWAY_HTTP_STATUS_URL
        "\","
                 "\"http_stat_user\":\"\","
                 "\"http_stat_pass\":\"\","
                 "\t\"http_stat_use_ssl_client_cert\":\tfalse,\n"
                 "\t\"http_stat_use_ssl_server_cert\":\tfalse,\n"
                 "\"use_mqtt\":true,"
                 "\"mqtt_disable_retained_messages\":false,"
                 "\"mqtt_transport\":\"TCP\","
                 "\t\"mqtt_data_format\":\t\"ruuvi_raw\",\n"
                 "\"mqtt_server\":\"test.mosquitto.org\","
                 "\"mqtt_port\":1883,"
                 "\"mqtt_sending_interval\":0,"
                 "\"mqtt_prefix\":\"ruuvi/30:AE:A4:02:84:A4\","
                 "\"mqtt_client_id\":\"30:AE:A4:02:84:A4\","
                 "\"mqtt_user\":\"\","
                 "\"mqtt_pass\":\"\","
                 "\t\"mqtt_use_ssl_client_cert\":\tfalse,\n"
                 "\t\"mqtt_use_ssl_server_cert\":\tfalse,\n"
                 "\"company_use_filtering\":true\n"
                 "}",
        flag_access_from_lan);

    ASSERT_TRUE(this->m_flag_settings_saved_to_flash);
    ASSERT_FALSE(this->m_flag_settings_ethernet_ip_updated);

    ASSERT_EQ(HTTP_RESP_CODE_200, resp.http_resp_code);
    ASSERT_EQ(HTTP_CONTENT_LOCATION_FLASH_MEM, resp.content_location);
    ASSERT_TRUE(resp.flag_no_cache);
    ASSERT_EQ(HTTP_CONTENT_TYPE_APPLICATION_JSON, resp.content_type);
    ASSERT_EQ(nullptr, resp.p_content_type_param);
    ASSERT_EQ(strlen(expected_resp), resp.content_len);
    ASSERT_EQ(HTTP_CONTENT_ENCODING_NONE, resp.content_encoding);
    ASSERT_NE(nullptr, resp.select_location.flash.p_buf);
    ASSERT_EQ(string(expected_resp), string(reinterpret_cast<const char*>(resp.select_location.flash.p_buf)));
    TEST_CHECK_LOG_RECORD_HTTP_SERVER(ESP_LOG_DEBUG, string("POST /ruuvi.json, flag_access_from_lan=0"));
    TEST_CHECK_LOG_RECORD_HTTP_SERVER(
        ESP_LOG_DEBUG,
        string("POST /ruuvi.json, body: {"
               "\"remote_cfg_use\":false,\n"
               "\"remote_cfg_url\":\"\",\n"
               "\"remote_cfg_auth_type\":\"none\",\n"
               "\t\"remote_cfg_use_ssl_client_cert\":\tfalse,\n"
               "\t\"remote_cfg_use_ssl_server_cert\":\tfalse,\n"
               "\"remote_cfg_refresh_interval_minutes\":0,\n"
               "\"use_http_ruuvi\":false,"
               "\"use_http_ruuvi\":false,"
               "\"use_http\":false,"
               "\"http_data_format\":\"ruuvi\","
               "\"http_auth\":\"none\","
               "\"http_url\":\"https://network.ruuvi.com/record\","
               "\"http_user\":\"\","
               "\"http_pass\":\"\","
               "\"use_http_stat\":true,"
               "\"http_stat_url\":\"https://network.ruuvi.com/status\","
               "\"http_stat_user\":\"\","
               "\"http_stat_pass\":\"\","
               "\t\"http_stat_use_ssl_client_cert\":\tfalse,\n"
               "\t\"http_stat_use_ssl_server_cert\":\tfalse,\n"
               "\"use_mqtt\":true,"
               "\"mqtt_disable_retained_messages\":false,"
               "\"mqtt_transport\":\"TCP\","
               "\t\"mqtt_data_format\":\t\"ruuvi_raw\","
               "\n\"mqtt_server\":\"test.mosquitto.org\","
               "\"mqtt_port\":1883,"
               "\"mqtt_sending_interval\":0,"
               "\"mqtt_prefix\":\"ruuvi/30:AE:A4:02:84:A4\","
               "\"mqtt_client_id\":\"30:AE:A4:02:84:A4\","
               "\"mqtt_user\":\"\","
               "\"mqtt_pass\":\"\","
               "\t\"mqtt_use_ssl_client_cert\":\tfalse,\n"
               "\t\"mqtt_use_ssl_server_cert\":\tfalse,\n"
               "\"company_use_filtering\":true\n"
               "}"));
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, "Gateway SETTINGS (via HTTP):");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "remote_cfg_use: 0");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "remote_cfg_url: ");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "remote_cfg_auth_type: none");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "remote_cfg_use_ssl_client_cert: 0");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "remote_cfg_use_ssl_server_cert: 0");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "remote_cfg_refresh_interval_minutes: 0");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "use_http_ruuvi: 0");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "use_http: 0");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "http_data_format: ruuvi");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "http_auth: none");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "http_url: " RUUVI_GATEWAY_HTTP_DEFAULT_URL);
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "http_period: not found or invalid");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "Can't find key 'http_period' in config-json");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "http_use_extra_http_path: not found");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "Can't find key 'http_use_extra_http_path' in config-json");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "http_use_extra_http_query: not found");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "Can't find key 'http_use_extra_http_query' in config-json");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "http_use_extra_http_headers: not found");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "Can't find key 'http_use_extra_http_headers' in config-json");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "http_use_ssl_client_cert: not found");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "Can't find key 'http_use_ssl_client_cert' in config-json");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "http_use_ssl_server_cert: not found");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "Can't find key 'http_use_ssl_server_cert' in config-json");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "use_http_stat: 1");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "http_stat_url: " RUUVI_GATEWAY_HTTP_STATUS_URL);
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "http_stat_user: ");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "http_stat_pass: ");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "http_stat_use_ssl_client_cert: 0");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "http_stat_use_ssl_server_cert: 0");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "use_mqtt: 1");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "mqtt_disable_retained_messages: 0");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "mqtt_transport: TCP");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "mqtt_data_format: ruuvi_raw");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "mqtt_server: test.mosquitto.org");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "mqtt_port: 1883");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "mqtt_sending_interval: 0");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "mqtt_prefix: ruuvi/30:AE:A4:02:84:A4");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "mqtt_client_id: 30:AE:A4:02:84:A4");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "mqtt_user: ");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "mqtt_pass: ");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "mqtt_use_ssl_client_cert: 0");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "mqtt_use_ssl_server_cert: 0");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "lan_auth_type: not found");
    TEST_CHECK_LOG_RECORD_GW_CFG(
        ESP_LOG_INFO,
        "Can't find key 'lan_auth_type' in config-json, leave the previous value unchanged");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "lan_auth_api_key: not found");
    TEST_CHECK_LOG_RECORD_GW_CFG(
        ESP_LOG_INFO,
        "Can't find key 'lan_auth_api_key' in config-json, leave the previous value unchanged");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "lan_auth_api_key_rw: not found");
    TEST_CHECK_LOG_RECORD_GW_CFG(
        ESP_LOG_INFO,
        "Can't find key 'lan_auth_api_key_rw' in config-json, leave the previous value unchanged");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "auto_update_cycle: not found");
    TEST_CHECK_LOG_RECORD_GW_CFG(
        ESP_LOG_WARN,
        "Can't find key 'auto_update_cycle' in config-json, leave the previous value unchanged");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "auto_update_weekdays_bitmask: not found or invalid");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_WARN, "Can't find key 'auto_update_weekdays_bitmask' in config-json");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "auto_update_interval_from: not found or invalid");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_WARN, "Can't find key 'auto_update_interval_from' in config-json");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "auto_update_interval_to: not found or invalid");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_WARN, "Can't find key 'auto_update_interval_to' in config-json");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "auto_update_tz_offset_hours: not found or invalid");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_WARN, "Can't find key 'auto_update_tz_offset_hours' in config-json");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "ntp_use: not found");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_WARN, "Can't find key 'ntp_use' in config-json");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "ntp_use_dhcp: not found");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_WARN, "Can't find key 'ntp_use_dhcp' in config-json");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "ntp_server1: not found");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_WARN, "Can't find key 'ntp_server1' in config-json");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "ntp_server2: not found");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_WARN, "Can't find key 'ntp_server2' in config-json");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "ntp_server3: not found");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_WARN, "Can't find key 'ntp_server3' in config-json");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "ntp_server4: not found");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_WARN, "Can't find key 'ntp_server4' in config-json");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "company_id: not found or invalid");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_WARN, "Can't find key 'company_id' in config-json");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "company_use_filtering: 1");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "scan_default: not found");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_WARN, "Can't find key 'scan_default' in config-json");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "scan_filter_allow_listed: not found");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_WARN, "Can't find key 'scan_filter_allow_listed' in config-json");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_WARN, "Can't find key 'scan_filter_list' in config-json");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "coordinates: not found");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_WARN, "Can't find key 'coordinates' in config-json");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "fw_update_url: not found");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_WARN, "Can't find key 'fw_update_url' in config-json");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, string("config: use remote cfg: 0"));
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, string("config: remote cfg: URL: "));
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, string("config: remote cfg: auth_type: none"));
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, string("config: remote cfg: use SSL client cert: 0"));
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, string("config: remote cfg: use SSL server cert: 0"));
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, string("config: remote cfg: refresh_interval_minutes: 0"));
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, string("config: use http ruuvi: 0"));
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, string("config: use http: 0"));
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, string("config: use http_stat: 1"));
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, string("config: http_stat url: " RUUVI_GATEWAY_HTTP_STATUS_URL));
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, string("config: http_stat user: "));
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, string("config: http_stat pass: "));
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, string("config: http_stat: use SSL client cert: 0"));
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, string("config: http_stat: use SSL server cert: 0"));
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, string("config: use mqtt: 1"));
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, string("config: mqtt disable retained messages: 0"));
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, string("config: mqtt transport: TCP"));
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, string("config: mqtt data format: ruuvi_raw"));
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, string("config: mqtt server: test.mosquitto.org"));
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, string("config: mqtt port: 1883"));
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, string("config: mqtt sending interval: 0"));
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, string("config: mqtt prefix: ruuvi/30:AE:A4:02:84:A4"));
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, string("config: mqtt client id: 30:AE:A4:02:84:A4"));
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, string("config: mqtt user: "));
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, string("config: mqtt password: "));
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, string("config: mqtt: use SSL client cert: 0"));
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, string("config: mqtt: use SSL server cert: 0"));
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, string("config: LAN auth type: lan_auth_default"));
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, string("config: LAN auth user: Admin"));
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, string("config: LAN auth pass: f32dd273cd874d98ec4fc21d534e3e61"));
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, string("config: LAN auth API key: "));
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, string("config: LAN auth API key (RW): "));
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, string("config: Auto update cycle: regular"));
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, string("config: Auto update weekdays_bitmask: 0x7f"));
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, string("config: Auto update interval: 00:00..24:00"));
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, string("config: Auto update TZ: UTC+3"));
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, string("config: NTP: Use: yes"));
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, string("config: NTP: Use DHCP: no"));
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, string("config: NTP: Server1: time.google.com"));
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, string("config: NTP: Server2: time.cloudflare.com"));
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, string("config: NTP: Server3: pool.ntp.org"));
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, string("config: NTP: Server4: time.ruuvi.com"));
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, string("config: use company id filter: 1"));
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, string("config: company id: 0x0499"));
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, string("config: use scan coded phy: 0"));
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, string("config: use scan 1mbit/phy: 1"));
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, string("config: use scan 2mbit/phy: 1"));
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, string("config: use scan channel 37: 1"));
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, string("config: use scan channel 38: 1"));
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, string("config: use scan channel 39: 1"));
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, string("config: scan default       : 1"));
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, string("config: use scan filter: no"));
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, string("config: coordinates: "));
    TEST_CHECK_LOG_RECORD_GW_CFG(
        ESP_LOG_INFO,
        string("config: fw_update: url: https://network.ruuvi.com/firmwareupdate"));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    os_malloc_trace_dump();
    ESP_LOG_WRAPPER_TEST_CHECK_LOG_RECORD("MEM_TRACE", ESP_LOG_INFO, "Num blocks allocated: 0");
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_EQ(0, this->m_alloc_free_call_count);
}

TEST_F(TestHttpServerCb, http_server_cb_on_post_ruuvi_malloc_failed1) // NOLINT
{
    const bool flag_access_from_lan = false;

    this->m_malloc_cnt            = 0;
    this->m_malloc_fail_on_cnt    = 1;
    const http_server_resp_t resp = http_server_cb_on_post_ruuvi(
        "{"
        "\"use_mqtt\":true,"
        "\"mqtt_disable_retained_messages\":false,"
        "\"mqtt_server\":\"test.mosquitto.org\","
        "\"mqtt_port\":1883,"
        "\"mqtt_sending_interval\":0,"
        "\"mqtt_prefix\":\"ruuvi/30:AE:A4:02:84:A4\","
        "\"mqtt_client_id\":\"30:AE:A4:02:84:A4\","
        "\"mqtt_user\":\"\","
        "\"mqtt_pass\":\"\","
        "\"use_http_ruuvi\":false,"
        "\"use_http\":false,"
        "\"http_data_format\":\"ruuvi\","
        "\"http_auth\":\"none\","
        "\"http_url\":\"" RUUVI_GATEWAY_HTTP_DEFAULT_URL
        "\","
        "\"http_user\":\"\","
        "\"http_pass\":\"\","
        "\"use_http_stat\":true,"
        "\"http_stat_url\":\"" RUUVI_GATEWAY_HTTP_STATUS_URL
        "\","
        "\"http_stat_user\":\"\","
        "\"http_stat_pass\":\"\","
        "\"company_use_filtering\":true"
        "}",
        flag_access_from_lan);

    ASSERT_FALSE(this->m_flag_settings_saved_to_flash);
    ASSERT_FALSE(this->m_flag_settings_ethernet_ip_updated);

    ASSERT_EQ(HTTP_RESP_CODE_503, resp.http_resp_code);
    ASSERT_EQ(HTTP_CONTENT_LOCATION_NO_CONTENT, resp.content_location);
    ASSERT_TRUE(resp.flag_no_cache);
    ASSERT_EQ(HTTP_CONTENT_TYPE_TEXT_HTML, resp.content_type);
    ASSERT_EQ(nullptr, resp.p_content_type_param);
    ASSERT_EQ(0, resp.content_len);
    ASSERT_EQ(HTTP_CONTENT_ENCODING_NONE, resp.content_encoding);
    TEST_CHECK_LOG_RECORD_HTTP_SERVER(ESP_LOG_DEBUG, string("POST /ruuvi.json, flag_access_from_lan=0"));
    TEST_CHECK_LOG_RECORD_HTTP_SERVER(
        ESP_LOG_DEBUG,
        string("POST /ruuvi.json, body: {"
               "\"use_mqtt\":true,"
               "\"mqtt_disable_retained_messages\":false,"
               "\"mqtt_server\":\"test.mosquitto.org\","
               "\"mqtt_port\":1883,"
               "\"mqtt_sending_interval\":0,"
               "\"mqtt_prefix\":\"ruuvi/30:AE:A4:02:84:A4\","
               "\"mqtt_client_id\":\"30:AE:A4:02:84:A4\","
               "\"mqtt_user\":\"\","
               "\"mqtt_pass\":\"\","
               "\"use_http_ruuvi\":false,"
               "\"use_http\":false,"
               "\"http_data_format\":\"ruuvi\","
               "\"http_auth\":\"none\","
               "\"http_url\":\"https://network.ruuvi.com/record\","
               "\"http_user\":\"\","
               "\"http_pass\":\"\","
               "\"use_http_stat\":true,"
               "\"http_stat_url\":\"https://network.ruuvi.com/status\","
               "\"http_stat_user\":\"\","
               "\"http_stat_pass\":\"\","
               "\"company_use_filtering\":true"
               "}"));
    TEST_CHECK_LOG_RECORD_HTTP_SERVER(ESP_LOG_ERROR, string("Failed to allocate memory for gw_cfg"));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    os_malloc_trace_dump();
    ESP_LOG_WRAPPER_TEST_CHECK_LOG_RECORD("MEM_TRACE", ESP_LOG_INFO, "Num blocks allocated: 0");
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_EQ(0, this->m_alloc_free_call_count);
}

TEST_F(TestHttpServerCb, http_server_cb_on_post_ruuvi_malloc_failed2) // NOLINT
{
    const bool flag_access_from_lan = false;

    this->m_malloc_cnt            = 0;
    this->m_malloc_fail_on_cnt    = 2;
    const http_server_resp_t resp = http_server_cb_on_post_ruuvi(
        "{"
        "\"use_mqtt\":true,"
        "\"mqtt_disable_retained_messages\":false,"
        "\"mqtt_server\":\"test.mosquitto.org\","
        "\"mqtt_port\":1883,"
        "\"mqtt_sending_interval\":0,"
        "\"mqtt_prefix\":\"ruuvi/30:AE:A4:02:84:A4\","
        "\"mqtt_client_id\":\"30:AE:A4:02:84:A4\","
        "\"mqtt_user\":\"\","
        "\"mqtt_pass\":\"\","
        "\"use_http_ruuvi\":false,"
        "\"use_http\":false,"
        "\"http_data_format\":\"ruuvi\","
        "\"http_auth\":\"none\","
        "\"http_url\":\"" RUUVI_GATEWAY_HTTP_DEFAULT_URL
        "\","
        "\"http_user\":\"\","
        "\"http_pass\":\"\","
        "\"use_http_stat\":true,"
        "\"http_stat_url\":\"" RUUVI_GATEWAY_HTTP_STATUS_URL
        "\","
        "\"http_stat_user\":\"\","
        "\"http_stat_pass\":\"\","
        "\"company_use_filtering\":true"
        "}",
        flag_access_from_lan);

    ASSERT_FALSE(this->m_flag_settings_saved_to_flash);
    ASSERT_FALSE(this->m_flag_settings_ethernet_ip_updated);

    ASSERT_EQ(HTTP_RESP_CODE_503, resp.http_resp_code);
    ASSERT_EQ(HTTP_CONTENT_LOCATION_NO_CONTENT, resp.content_location);
    ASSERT_TRUE(resp.flag_no_cache);
    ASSERT_EQ(HTTP_CONTENT_TYPE_TEXT_HTML, resp.content_type);
    ASSERT_EQ(nullptr, resp.p_content_type_param);
    ASSERT_EQ(0, resp.content_len);
    ASSERT_EQ(HTTP_CONTENT_ENCODING_NONE, resp.content_encoding);
    TEST_CHECK_LOG_RECORD_HTTP_SERVER(ESP_LOG_DEBUG, string("POST /ruuvi.json, flag_access_from_lan=0"));
    TEST_CHECK_LOG_RECORD_HTTP_SERVER(
        ESP_LOG_DEBUG,
        string("POST /ruuvi.json, body: {"
               "\"use_mqtt\":true,"
               "\"mqtt_disable_retained_messages\":false,"
               "\"mqtt_server\":\"test.mosquitto.org\","
               "\"mqtt_port\":1883,"
               "\"mqtt_sending_interval\":0,"
               "\"mqtt_prefix\":\"ruuvi/30:AE:A4:02:84:A4\","
               "\"mqtt_client_id\":\"30:AE:A4:02:84:A4\","
               "\"mqtt_user\":\"\","
               "\"mqtt_pass\":\"\","
               "\"use_http_ruuvi\":false,"
               "\"use_http\":false,"
               "\"http_data_format\":\"ruuvi\","
               "\"http_auth\":\"none\","
               "\"http_url\":\"https://network.ruuvi.com/record\","
               "\"http_user\":\"\","
               "\"http_pass\":\"\","
               "\"use_http_stat\":true,"
               "\"http_stat_url\":\"https://network.ruuvi.com/status\","
               "\"http_stat_user\":\"\","
               "\"http_stat_pass\":\"\","
               "\"company_use_filtering\":true"
               "}"));
    TEST_CHECK_LOG_RECORD_HTTP_SERVER(ESP_LOG_ERROR, string("Failed to parse json or no memory"));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    os_malloc_trace_dump();
    ESP_LOG_WRAPPER_TEST_CHECK_LOG_RECORD("MEM_TRACE", ESP_LOG_INFO, "Num blocks allocated: 0");
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_EQ(0, this->m_alloc_free_call_count);
}

TEST_F(TestHttpServerCb, http_server_cb_on_post_ruuvi_malloc_failed3) // NOLINT
{
    const bool flag_access_from_lan = false;

    this->m_malloc_cnt            = 0;
    this->m_malloc_fail_on_cnt    = 3;
    const http_server_resp_t resp = http_server_cb_on_post_ruuvi(
        "{"
        "\"use_mqtt\":true,"
        "\"mqtt_disable_retained_messages\":false,"
        "\"mqtt_server\":\"test.mosquitto.org\","
        "\"mqtt_port\":1883,"
        "\"mqtt_sending_interval\":0,"
        "\"mqtt_prefix\":\"ruuvi/30:AE:A4:02:84:A4\","
        "\"mqtt_client_id\":\"30:AE:A4:02:84:A4\","
        "\"mqtt_user\":\"\","
        "\"mqtt_pass\":\"\","
        "\"use_http_ruuvi\":false,"
        "\"use_http\":false,"
        "\"http_data_format\":\"ruuvi\","
        "\"http_auth\":\"none\","
        "\"http_url\":\"" RUUVI_GATEWAY_HTTP_DEFAULT_URL
        "\","
        "\"http_user\":\"\","
        "\"http_pass\":\"\","
        "\"use_http_stat\":true,"
        "\"http_stat_url\":\"" RUUVI_GATEWAY_HTTP_STATUS_URL
        "\","
        "\"http_stat_user\":\"\","
        "\"http_stat_pass\":\"\","
        "\"company_use_filtering\":true"
        "}",
        flag_access_from_lan);

    ASSERT_FALSE(this->m_flag_settings_saved_to_flash);
    ASSERT_FALSE(this->m_flag_settings_ethernet_ip_updated);

    ASSERT_EQ(HTTP_RESP_CODE_503, resp.http_resp_code);
    ASSERT_EQ(HTTP_CONTENT_LOCATION_NO_CONTENT, resp.content_location);
    ASSERT_TRUE(resp.flag_no_cache);
    ASSERT_EQ(HTTP_CONTENT_TYPE_TEXT_HTML, resp.content_type);
    ASSERT_EQ(nullptr, resp.p_content_type_param);
    ASSERT_EQ(0, resp.content_len);
    ASSERT_EQ(HTTP_CONTENT_ENCODING_NONE, resp.content_encoding);
    TEST_CHECK_LOG_RECORD_HTTP_SERVER(ESP_LOG_DEBUG, string("POST /ruuvi.json, flag_access_from_lan=0"));
    TEST_CHECK_LOG_RECORD_HTTP_SERVER(
        ESP_LOG_DEBUG,
        string("POST /ruuvi.json, body: {"
               "\"use_mqtt\":true,"
               "\"mqtt_disable_retained_messages\":false,"
               "\"mqtt_server\":\"test.mosquitto.org\","
               "\"mqtt_port\":1883,"
               "\"mqtt_sending_interval\":0,"
               "\"mqtt_prefix\":\"ruuvi/30:AE:A4:02:84:A4\","
               "\"mqtt_client_id\":\"30:AE:A4:02:84:A4\","
               "\"mqtt_user\":\"\","
               "\"mqtt_pass\":\"\","
               "\"use_http_ruuvi\":false,"
               "\"use_http\":false,"
               "\"http_data_format\":\"ruuvi\","
               "\"http_auth\":\"none\","
               "\"http_url\":\"https://network.ruuvi.com/record\","
               "\"http_user\":\"\","
               "\"http_pass\":\"\","
               "\"use_http_stat\":true,"
               "\"http_stat_url\":\"https://network.ruuvi.com/status\","
               "\"http_stat_user\":\"\","
               "\"http_stat_pass\":\"\","
               "\"company_use_filtering\":true"
               "}"));
    TEST_CHECK_LOG_RECORD_HTTP_SERVER(ESP_LOG_ERROR, string("Failed to parse json or no memory"));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    os_malloc_trace_dump();
    ESP_LOG_WRAPPER_TEST_CHECK_LOG_RECORD("MEM_TRACE", ESP_LOG_INFO, "Num blocks allocated: 0");
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_EQ(0, this->m_alloc_free_call_count);
}

TEST_F(TestHttpServerCb, http_server_cb_on_post_ruuvi_json_ok_save_prev_lan_auth) // NOLINT
{
    const char* expected_resp = "{}";
    {
        gw_cfg_t gw_cfg = { 0 };
        gw_cfg_default_get(&gw_cfg);
        gw_cfg.ruuvi_cfg.lan_auth.lan_auth_type = HTTP_SERVER_AUTH_TYPE_RUUVI;
        (void)snprintf(
            gw_cfg.ruuvi_cfg.lan_auth.lan_auth_user.buf,
            sizeof(gw_cfg.ruuvi_cfg.lan_auth.lan_auth_user.buf),
            "user1");
        (void)snprintf(
            gw_cfg.ruuvi_cfg.lan_auth.lan_auth_pass.buf,
            sizeof(gw_cfg.ruuvi_cfg.lan_auth.lan_auth_pass.buf),
            "password1");
        gw_cfg_update(&gw_cfg);
    }
    const http_server_resp_t resp = http_server_cb_on_post(
        "ruuvi.json",
        nullptr,
        "{"
        "\"remote_cfg_use\":false,\n"
        "\"remote_cfg_url\":\"\",\n"
        "\"remote_cfg_auth_type\":\"none\",\n"
        "\t\"remote_cfg_use_ssl_client_cert\":\tfalse,\n"
        "\t\"remote_cfg_use_ssl_server_cert\":\tfalse,\n"
        "\"remote_cfg_refresh_interval_minutes\":0,\n"
        "\"use_http_ruuvi\":false,"
        "\"use_http\":false,"
        "\"http_data_format\":\"ruuvi\","
        "\"http_auth\":\"none\","
        "\"http_url\":\"" RUUVI_GATEWAY_HTTP_DEFAULT_URL
        "\","
        "\"http_user\":\"\","
        "\"http_pass\":\"\","
        "\"use_http_stat\":true,"
        "\"http_stat_url\":\"" RUUVI_GATEWAY_HTTP_STATUS_URL
        "\","
        "\"http_stat_user\":\"\","
        "\"http_stat_pass\":\"\","
        "\t\"http_stat_use_ssl_client_cert\":\tfalse,\n"
        "\t\"http_stat_use_ssl_server_cert\":\tfalse,\n"
        "\"use_mqtt\":true,"
        "\"mqtt_disable_retained_messages\":false,"
        "\"mqtt_transport\":\"TCP\","
        "\t\"mqtt_data_format\":\t\"ruuvi_raw\",\n"
        "\"mqtt_server\":\"test.mosquitto.org\","
        "\"mqtt_port\":1883,"
        "\"mqtt_sending_interval\":0,"
        "\"mqtt_prefix\":\"ruuvi/30:AE:A4:02:84:A4\","
        "\"mqtt_client_id\":\"30:AE:A4:02:84:A4\","
        "\"mqtt_user\":\"\","
        "\"mqtt_pass\":\"\","
        "\t\"mqtt_use_ssl_client_cert\":\tfalse,\n"
        "\t\"mqtt_use_ssl_server_cert\":\tfalse,\n"
        "\"lan_auth_api_key\":\"wH3F9SIiAA3rhG32aJki2Z7ekdFc0vtxuDhxl39zFvw=\","
        "\"company_use_filtering\":true"
        "}",
        false);

    ASSERT_TRUE(this->m_flag_settings_saved_to_flash);
    ASSERT_FALSE(this->m_flag_settings_ethernet_ip_updated);

    ASSERT_EQ(HTTP_RESP_CODE_200, resp.http_resp_code);
    ASSERT_EQ(HTTP_CONTENT_LOCATION_FLASH_MEM, resp.content_location);
    ASSERT_TRUE(resp.flag_no_cache);
    ASSERT_EQ(HTTP_CONTENT_TYPE_APPLICATION_JSON, resp.content_type);
    ASSERT_EQ(nullptr, resp.p_content_type_param);
    ASSERT_EQ(strlen(expected_resp), resp.content_len);
    ASSERT_EQ(HTTP_CONTENT_ENCODING_NONE, resp.content_encoding);
    ASSERT_NE(nullptr, resp.select_location.flash.p_buf);
    ASSERT_EQ(string(expected_resp), string(reinterpret_cast<const char*>(resp.select_location.flash.p_buf)));
    TEST_CHECK_LOG_RECORD_HTTP_SERVER(ESP_LOG_DEBUG, string("http_server_cb_on_post /ruuvi.json, params="));
    TEST_CHECK_LOG_RECORD_HTTP_SERVER(
        ESP_LOG_INFO,
        string("http_server_cb_on_post: Clear all saved TLS session tickets"));
    TEST_CHECK_LOG_RECORD_HTTP_SERVER(ESP_LOG_DEBUG, string("POST /ruuvi.json, flag_access_from_lan=0"));
    TEST_CHECK_LOG_RECORD_HTTP_SERVER(
        ESP_LOG_DEBUG,
        string("POST /ruuvi.json, body: {"
               "\"remote_cfg_use\":false,\n"
               "\"remote_cfg_url\":\"\",\n"
               "\"remote_cfg_auth_type\":\"none\",\n"
               "\t\"remote_cfg_use_ssl_client_cert\":\tfalse,\n"
               "\t\"remote_cfg_use_ssl_server_cert\":\tfalse,\n"
               "\"remote_cfg_refresh_interval_minutes\":0,\n"
               "\"use_http_ruuvi\":false,"
               "\"use_http\":false,"
               "\"http_data_format\":\"ruuvi\","
               "\"http_auth\":\"none\","
               "\"http_url\":\"https://network.ruuvi.com/record\","
               "\"http_user\":\"\","
               "\"http_pass\":\"\","
               "\"use_http_stat\":true,"
               "\"http_stat_url\":\"https://network.ruuvi.com/status\","
               "\"http_stat_user\":\"\","
               "\"http_stat_pass\":\"\","
               "\t\"http_stat_use_ssl_client_cert\":\tfalse,\n"
               "\t\"http_stat_use_ssl_server_cert\":\tfalse,\n"
               "\"use_mqtt\":true,"
               "\"mqtt_disable_retained_messages\":false,"
               "\"mqtt_transport\":\"TCP\","
               "\t\"mqtt_data_format\":\t\"ruuvi_raw\",\n"
               "\"mqtt_server\":\"test.mosquitto.org\","
               "\"mqtt_port\":1883,\"mqtt_sending_interval\":0,"
               "\"mqtt_prefix\":\"ruuvi/30:AE:A4:02:84:A4\","
               "\"mqtt_client_id\":\"30:AE:A4:02:84:A4\","
               "\"mqtt_user\":\"\","
               "\"mqtt_pass\":\"\","
               "\t\"mqtt_use_ssl_client_cert\":\tfalse,\n"
               "\t\"mqtt_use_ssl_server_cert\":\tfalse,\n"
               "\"lan_auth_api_key\":\"wH3F9SIiAA3rhG32aJki2Z7ekdFc0vtxuDhxl39zFvw=\","
               "\"company_use_filtering\":true"
               "}"));
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, "Gateway SETTINGS (via HTTP):");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "remote_cfg_use: 0");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "remote_cfg_url: ");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "remote_cfg_auth_type: none");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "remote_cfg_use_ssl_client_cert: 0");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "remote_cfg_use_ssl_server_cert: 0");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "remote_cfg_refresh_interval_minutes: 0");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "use_http_ruuvi: 0");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "use_http: 0");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "http_data_format: ruuvi");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "http_auth: none");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "http_url: " RUUVI_GATEWAY_HTTP_DEFAULT_URL);
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "http_period: not found or invalid");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "Can't find key 'http_period' in config-json");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "http_use_extra_http_path: not found");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "Can't find key 'http_use_extra_http_path' in config-json");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "http_use_extra_http_query: not found");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "Can't find key 'http_use_extra_http_query' in config-json");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "http_use_extra_http_headers: not found");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "Can't find key 'http_use_extra_http_headers' in config-json");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "http_use_ssl_client_cert: not found");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "Can't find key 'http_use_ssl_client_cert' in config-json");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "http_use_ssl_server_cert: not found");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "Can't find key 'http_use_ssl_server_cert' in config-json");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "use_http_stat: 1");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "http_stat_url: " RUUVI_GATEWAY_HTTP_STATUS_URL);
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "http_stat_user: ");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "http_stat_pass: ");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "http_stat_use_ssl_client_cert: 0");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "http_stat_use_ssl_server_cert: 0");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "use_mqtt: 1");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "mqtt_disable_retained_messages: 0");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "mqtt_transport: TCP");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "mqtt_data_format: ruuvi_raw");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "mqtt_server: test.mosquitto.org");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "mqtt_port: 1883");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "mqtt_sending_interval: 0");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "mqtt_prefix: ruuvi/30:AE:A4:02:84:A4");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "mqtt_client_id: 30:AE:A4:02:84:A4");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "mqtt_user: ");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "mqtt_pass: ");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "mqtt_use_ssl_client_cert: 0");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "mqtt_use_ssl_server_cert: 0");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "lan_auth_type: not found");
    TEST_CHECK_LOG_RECORD_GW_CFG(
        ESP_LOG_INFO,
        "Can't find key 'lan_auth_type' in config-json, leave the previous value unchanged");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "lan_auth_api_key: wH3F9SIiAA3rhG32aJki2Z7ekdFc0vtxuDhxl39zFvw=");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "lan_auth_api_key_rw: not found");
    TEST_CHECK_LOG_RECORD_GW_CFG(
        ESP_LOG_INFO,
        "Can't find key 'lan_auth_api_key_rw' in config-json, leave the previous value unchanged");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "auto_update_cycle: not found");
    TEST_CHECK_LOG_RECORD_GW_CFG(
        ESP_LOG_WARN,
        "Can't find key 'auto_update_cycle' in config-json, leave the previous value unchanged");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "auto_update_weekdays_bitmask: not found or invalid");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_WARN, "Can't find key 'auto_update_weekdays_bitmask' in config-json");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "auto_update_interval_from: not found or invalid");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_WARN, "Can't find key 'auto_update_interval_from' in config-json");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "auto_update_interval_to: not found or invalid");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_WARN, "Can't find key 'auto_update_interval_to' in config-json");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "auto_update_tz_offset_hours: not found or invalid");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_WARN, "Can't find key 'auto_update_tz_offset_hours' in config-json");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "ntp_use: not found");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_WARN, "Can't find key 'ntp_use' in config-json");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "ntp_use_dhcp: not found");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_WARN, "Can't find key 'ntp_use_dhcp' in config-json");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "ntp_server1: not found");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_WARN, "Can't find key 'ntp_server1' in config-json");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "ntp_server2: not found");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_WARN, "Can't find key 'ntp_server2' in config-json");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "ntp_server3: not found");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_WARN, "Can't find key 'ntp_server3' in config-json");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "ntp_server4: not found");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_WARN, "Can't find key 'ntp_server4' in config-json");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "company_id: not found or invalid");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_WARN, "Can't find key 'company_id' in config-json");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "company_use_filtering: 1");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "scan_default: not found");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_WARN, "Can't find key 'scan_default' in config-json");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "scan_filter_allow_listed: not found");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_WARN, "Can't find key 'scan_filter_allow_listed' in config-json");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_WARN, "Can't find key 'scan_filter_list' in config-json");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "coordinates: not found");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_WARN, "Can't find key 'coordinates' in config-json");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "fw_update_url: not found");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_WARN, "Can't find key 'fw_update_url' in config-json");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, string("config: use remote cfg: 0"));
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, string("config: remote cfg: URL: "));
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, string("config: remote cfg: auth_type: none"));
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, string("config: remote cfg: use SSL client cert: 0"));
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, string("config: remote cfg: use SSL server cert: 0"));
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, string("config: remote cfg: refresh_interval_minutes: 0"));
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, string("config: use http ruuvi: 0"));
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, string("config: use http: 0"));
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, string("config: use http_stat: 1"));
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, string("config: http_stat url: " RUUVI_GATEWAY_HTTP_STATUS_URL));
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, string("config: http_stat user: "));
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, string("config: http_stat pass: "));
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, string("config: http_stat: use SSL client cert: 0"));
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, string("config: http_stat: use SSL server cert: 0"));
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, string("config: use mqtt: 1"));
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, string("config: mqtt disable retained messages: 0"));
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, string("config: mqtt transport: TCP"));
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, string("config: mqtt data format: ruuvi_raw"));
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, string("config: mqtt server: test.mosquitto.org"));
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, string("config: mqtt port: 1883"));
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, string("config: mqtt sending interval: 0"));
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, string("config: mqtt prefix: ruuvi/30:AE:A4:02:84:A4"));
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, string("config: mqtt client id: 30:AE:A4:02:84:A4"));
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, string("config: mqtt user: "));
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, string("config: mqtt password: "));
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, string("config: mqtt: use SSL client cert: 0"));
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, string("config: mqtt: use SSL server cert: 0"));
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, string("config: LAN auth type: lan_auth_ruuvi"));
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, string("config: LAN auth user: user1"));
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, string("config: LAN auth pass: password1"));
    TEST_CHECK_LOG_RECORD_GW_CFG(
        ESP_LOG_DEBUG,
        string("config: LAN auth API key: wH3F9SIiAA3rhG32aJki2Z7ekdFc0vtxuDhxl39zFvw="));
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, string("config: LAN auth API key (RW): "));
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, string("config: Auto update cycle: regular"));
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, string("config: Auto update weekdays_bitmask: 0x7f"));
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, string("config: Auto update interval: 00:00..24:00"));
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, string("config: Auto update TZ: UTC+3"));
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, string("config: NTP: Use: yes"));
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, string("config: NTP: Use DHCP: no"));
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, string("config: NTP: Server1: time.google.com"));
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, string("config: NTP: Server2: time.cloudflare.com"));
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, string("config: NTP: Server3: pool.ntp.org"));
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, string("config: NTP: Server4: time.ruuvi.com"));
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, string("config: use company id filter: 1"));
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, string("config: company id: 0x0499"));
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, string("config: use scan coded phy: 0"));
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, string("config: use scan 1mbit/phy: 1"));
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, string("config: use scan 2mbit/phy: 1"));
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, string("config: use scan channel 37: 1"));
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, string("config: use scan channel 38: 1"));
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, string("config: use scan channel 39: 1"));
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, string("config: scan default       : 1"));
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, string("config: use scan filter: no"));
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, string("config: coordinates: "));
    TEST_CHECK_LOG_RECORD_GW_CFG(
        ESP_LOG_INFO,
        string("config: fw_update: url: https://network.ruuvi.com/firmwareupdate"));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    os_malloc_trace_dump();
    ESP_LOG_WRAPPER_TEST_CHECK_LOG_RECORD("MEM_TRACE", ESP_LOG_INFO, "Num blocks allocated: 0");
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_EQ(0, this->m_alloc_free_call_count);
}

TEST_F(TestHttpServerCb, http_server_cb_on_post_ruuvi_json_ok_overwrite_lan_auth) // NOLINT
{
    const char* expected_resp = "{}";

    {
        gw_cfg_t gw_cfg = { 0 };
        gw_cfg_default_get(&gw_cfg);
        gw_cfg.ruuvi_cfg.lan_auth.lan_auth_type = HTTP_SERVER_AUTH_TYPE_RUUVI;
        (void)snprintf(
            gw_cfg.ruuvi_cfg.lan_auth.lan_auth_user.buf,
            sizeof(gw_cfg.ruuvi_cfg.lan_auth.lan_auth_user.buf),
            "user1");
        (void)snprintf(
            gw_cfg.ruuvi_cfg.lan_auth.lan_auth_pass.buf,
            sizeof(gw_cfg.ruuvi_cfg.lan_auth.lan_auth_pass.buf),
            "password1");
        gw_cfg_update(&gw_cfg);
    }

    const http_server_resp_t resp = http_server_cb_on_post(
        "ruuvi.json",
        nullptr,
        "{"
        "\"remote_cfg_use\":false,\n"
        "\"remote_cfg_url\":\"\",\n"
        "\"remote_cfg_auth_type\":\"none\",\n"
        "\"remote_cfg_use_ssl_client_cert\":\tfalse,\n"
        "\"remote_cfg_use_ssl_server_cert\":\tfalse,\n"
        "\"remote_cfg_refresh_interval_minutes\":0,\n"
        "\"use_http_ruuvi\":false,"
        "\"use_http\":false,"
        "\"http_data_format\":\"ruuvi\","
        "\"http_auth\":\"none\","
        "\"http_url\":\"" RUUVI_GATEWAY_HTTP_DEFAULT_URL
        "\","
        "\"http_user\":\"\","
        "\"http_pass\":\"\","
        "\"use_http_stat\":true,"
        "\"http_stat_url\":\"" RUUVI_GATEWAY_HTTP_STATUS_URL
        "\","
        "\"http_stat_user\":\"\","
        "\"http_stat_pass\":\"\","
        "\"http_stat_use_ssl_client_cert\":\tfalse,\n"
        "\"http_stat_use_ssl_server_cert\":\tfalse,\n"
        "\"use_mqtt\":true,"
        "\"mqtt_disable_retained_messages\":false,"
        "\"mqtt_transport\":\"TCP\","
        "\t\"mqtt_data_format\":\t\"ruuvi_raw\",\n"
        "\"mqtt_server\":\"test.mosquitto.org\","
        "\"mqtt_port\":1883,"
        "\"mqtt_sending_interval\":0,"
        "\"mqtt_prefix\":\"ruuvi/30:AE:A4:02:84:A4\","
        "\"mqtt_client_id\":\"30:AE:A4:02:84:A4\","
        "\"mqtt_user\":\"\","
        "\"mqtt_pass\":\"\","
        "\"mqtt_use_ssl_client_cert\":\tfalse,\n"
        "\"mqtt_use_ssl_server_cert\":\tfalse,\n"
        "\"lan_auth_type\":\"lan_auth_digest\","
        "\"lan_auth_user\":\"user2\","
        "\"lan_auth_pass\":\"password2\","
        "\"lan_auth_api_key\":\"6kl/fd/c+3qvWm3Mhmwgh3BWNp+HDRQiLp/X0PuwG8Q=\","
        "\"lan_auth_api_key_rw\":\"KAv9oAT0c1XzbCF9N/Bnj2mgVR7R4QbBn/L3Wq5/zuI=\","
        "\"company_use_filtering\":true"
        "}",
        false);

    ASSERT_TRUE(this->m_flag_settings_saved_to_flash);
    ASSERT_FALSE(this->m_flag_settings_ethernet_ip_updated);

    ASSERT_EQ(HTTP_RESP_CODE_200, resp.http_resp_code);
    ASSERT_EQ(HTTP_CONTENT_LOCATION_FLASH_MEM, resp.content_location);
    ASSERT_TRUE(resp.flag_no_cache);
    ASSERT_EQ(HTTP_CONTENT_TYPE_APPLICATION_JSON, resp.content_type);
    ASSERT_EQ(nullptr, resp.p_content_type_param);
    ASSERT_EQ(strlen(expected_resp), resp.content_len);
    ASSERT_EQ(HTTP_CONTENT_ENCODING_NONE, resp.content_encoding);
    ASSERT_NE(nullptr, resp.select_location.flash.p_buf);
    ASSERT_EQ(string(expected_resp), string(reinterpret_cast<const char*>(resp.select_location.flash.p_buf)));
    TEST_CHECK_LOG_RECORD_HTTP_SERVER(ESP_LOG_DEBUG, string("http_server_cb_on_post /ruuvi.json, params="));
    TEST_CHECK_LOG_RECORD_HTTP_SERVER(
        ESP_LOG_INFO,
        string("http_server_cb_on_post: Clear all saved TLS session tickets"));
    TEST_CHECK_LOG_RECORD_HTTP_SERVER(ESP_LOG_DEBUG, string("POST /ruuvi.json, flag_access_from_lan=0"));
    TEST_CHECK_LOG_RECORD_HTTP_SERVER(
        ESP_LOG_DEBUG,
        string("POST /ruuvi.json, body: {"
               "\"remote_cfg_use\":false,\n"
               "\"remote_cfg_url\":\"\",\n"
               "\"remote_cfg_auth_type\":\"none\",\n"
               "\"remote_cfg_use_ssl_client_cert\":\tfalse,\n"
               "\"remote_cfg_use_ssl_server_cert\":\tfalse,\n"
               "\"remote_cfg_refresh_interval_minutes\":0,\n"
               "\"use_http_ruuvi\":false,"
               "\"use_http\":false,"
               "\"http_data_format\":\"ruuvi\","
               "\"http_auth\":\"none\","
               "\"http_url\":\"https://network.ruuvi.com/record\","
               "\"http_user\":\"\","
               "\"http_pass\":\"\","
               "\"use_http_stat\":true,"
               "\"http_stat_url\":\"https://network.ruuvi.com/status\","
               "\"http_stat_user\":\"\","
               "\"http_stat_pass\":\"\","
               "\"http_stat_use_ssl_client_cert\":\tfalse,\n"
               "\"http_stat_use_ssl_server_cert\":\tfalse,\n"
               "\"use_mqtt\":true,"
               "\"mqtt_disable_retained_messages\":false,"
               "\"mqtt_transport\":\"TCP\","
               "\t\"mqtt_data_format\":\t\"ruuvi_raw\",\n"
               "\"mqtt_server\":\"test.mosquitto.org\","
               "\"mqtt_port\":1883,"
               "\"mqtt_sending_interval\":0,"
               "\"mqtt_prefix\":\"ruuvi/30:AE:A4:02:84:A4\","
               "\"mqtt_client_id\":\"30:AE:A4:02:84:A4\","
               "\"mqtt_user\":\"\","
               "\"mqtt_pass\":\"\","
               "\"mqtt_use_ssl_client_cert\":\tfalse,\n"
               "\"mqtt_use_ssl_server_cert\":\tfalse,\n"
               "\"lan_auth_type\":\"lan_auth_digest\","
               "\"lan_auth_user\":\"user2\","
               "\"lan_auth_pass\":\"password2\","
               "\"lan_auth_api_key\":\"6kl/fd/c+3qvWm3Mhmwgh3BWNp+HDRQiLp/X0PuwG8Q=\","
               "\"lan_auth_api_key_rw\":\"KAv9oAT0c1XzbCF9N/Bnj2mgVR7R4QbBn/L3Wq5/zuI=\","
               "\"company_use_filtering\":true"
               "}"));
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, "Gateway SETTINGS (via HTTP):");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "remote_cfg_use: 0");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "remote_cfg_url: ");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "remote_cfg_auth_type: none");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "remote_cfg_use_ssl_client_cert: 0");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "remote_cfg_use_ssl_server_cert: 0");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "remote_cfg_refresh_interval_minutes: 0");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "use_http_ruuvi: 0");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "use_http: 0");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "http_data_format: ruuvi");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "http_auth: none");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "http_url: " RUUVI_GATEWAY_HTTP_DEFAULT_URL);
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "http_period: not found or invalid");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "Can't find key 'http_period' in config-json");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "http_use_extra_http_path: not found");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "Can't find key 'http_use_extra_http_path' in config-json");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "http_use_extra_http_query: not found");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "Can't find key 'http_use_extra_http_query' in config-json");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "http_use_extra_http_headers: not found");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "Can't find key 'http_use_extra_http_headers' in config-json");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "http_use_ssl_client_cert: not found");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "Can't find key 'http_use_ssl_client_cert' in config-json");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "http_use_ssl_server_cert: not found");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "Can't find key 'http_use_ssl_server_cert' in config-json");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "use_http_stat: 1");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "http_stat_url: " RUUVI_GATEWAY_HTTP_STATUS_URL);
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "http_stat_user: ");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "http_stat_pass: ");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "http_stat_use_ssl_client_cert: 0");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "http_stat_use_ssl_server_cert: 0");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "use_mqtt: 1");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "mqtt_disable_retained_messages: 0");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "mqtt_transport: TCP");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "mqtt_data_format: ruuvi_raw");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "mqtt_server: test.mosquitto.org");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "mqtt_port: 1883");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "mqtt_sending_interval: 0");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "mqtt_prefix: ruuvi/30:AE:A4:02:84:A4");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "mqtt_client_id: 30:AE:A4:02:84:A4");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "mqtt_user: ");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "mqtt_pass: ");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "mqtt_use_ssl_client_cert: 0");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "mqtt_use_ssl_server_cert: 0");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "lan_auth_type: lan_auth_digest");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "lan_auth_user: user2");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "lan_auth_pass: password2");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "lan_auth_api_key: 6kl/fd/c+3qvWm3Mhmwgh3BWNp+HDRQiLp/X0PuwG8Q=");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "lan_auth_api_key_rw: KAv9oAT0c1XzbCF9N/Bnj2mgVR7R4QbBn/L3Wq5/zuI=");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "auto_update_cycle: not found");
    TEST_CHECK_LOG_RECORD_GW_CFG(
        ESP_LOG_WARN,
        "Can't find key 'auto_update_cycle' in config-json, leave the previous value unchanged");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "auto_update_weekdays_bitmask: not found or invalid");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_WARN, "Can't find key 'auto_update_weekdays_bitmask' in config-json");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "auto_update_interval_from: not found or invalid");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_WARN, "Can't find key 'auto_update_interval_from' in config-json");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "auto_update_interval_to: not found or invalid");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_WARN, "Can't find key 'auto_update_interval_to' in config-json");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "auto_update_tz_offset_hours: not found or invalid");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_WARN, "Can't find key 'auto_update_tz_offset_hours' in config-json");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "ntp_use: not found");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_WARN, "Can't find key 'ntp_use' in config-json");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "ntp_use_dhcp: not found");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_WARN, "Can't find key 'ntp_use_dhcp' in config-json");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "ntp_server1: not found");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_WARN, "Can't find key 'ntp_server1' in config-json");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "ntp_server2: not found");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_WARN, "Can't find key 'ntp_server2' in config-json");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "ntp_server3: not found");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_WARN, "Can't find key 'ntp_server3' in config-json");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "ntp_server4: not found");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_WARN, "Can't find key 'ntp_server4' in config-json");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "company_id: not found or invalid");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_WARN, "Can't find key 'company_id' in config-json");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "company_use_filtering: 1");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "scan_default: not found");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_WARN, "Can't find key 'scan_default' in config-json");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "scan_filter_allow_listed: not found");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_WARN, "Can't find key 'scan_filter_allow_listed' in config-json");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_WARN, "Can't find key 'scan_filter_list' in config-json");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "coordinates: not found");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_WARN, "Can't find key 'coordinates' in config-json");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "fw_update_url: not found");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_WARN, "Can't find key 'fw_update_url' in config-json");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, string("config: use remote cfg: 0"));
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, string("config: remote cfg: URL: "));
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, string("config: remote cfg: auth_type: none"));
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, string("config: remote cfg: use SSL client cert: 0"));
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, string("config: remote cfg: use SSL server cert: 0"));
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, string("config: remote cfg: refresh_interval_minutes: 0"));
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, string("config: use http ruuvi: 0"));
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, string("config: use http: 0"));
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, string("config: use http_stat: 1"));
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, string("config: http_stat url: " RUUVI_GATEWAY_HTTP_STATUS_URL));
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, string("config: http_stat user: "));
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, string("config: http_stat pass: "));
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, string("config: http_stat: use SSL client cert: 0"));
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, string("config: http_stat: use SSL server cert: 0"));
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, string("config: use mqtt: 1"));
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, string("config: mqtt disable retained messages: 0"));
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, string("config: mqtt transport: TCP"));
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, string("config: mqtt data format: ruuvi_raw"));
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, string("config: mqtt server: test.mosquitto.org"));
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, string("config: mqtt port: 1883"));
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, string("config: mqtt sending interval: 0"));
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, string("config: mqtt prefix: ruuvi/30:AE:A4:02:84:A4"));
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, string("config: mqtt client id: 30:AE:A4:02:84:A4"));
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, string("config: mqtt user: "));
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, string("config: mqtt password: "));
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, string("config: mqtt: use SSL client cert: 0"));
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, string("config: mqtt: use SSL server cert: 0"));
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, string("config: LAN auth type: lan_auth_digest"));
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, string("config: LAN auth user: user2"));
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, string("config: LAN auth pass: password2"));
    TEST_CHECK_LOG_RECORD_GW_CFG(
        ESP_LOG_DEBUG,
        string("config: LAN auth API key: 6kl/fd/c+3qvWm3Mhmwgh3BWNp+HDRQiLp/X0PuwG8Q="));
    TEST_CHECK_LOG_RECORD_GW_CFG(
        ESP_LOG_DEBUG,
        string("config: LAN auth API key (RW): KAv9oAT0c1XzbCF9N/Bnj2mgVR7R4QbBn/L3Wq5/zuI="));
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, string("config: Auto update cycle: regular"));
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, string("config: Auto update weekdays_bitmask: 0x7f"));
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, string("config: Auto update interval: 00:00..24:00"));
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, string("config: Auto update TZ: UTC+3"));
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, string("config: NTP: Use: yes"));
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, string("config: NTP: Use DHCP: no"));
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, string("config: NTP: Server1: time.google.com"));
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, string("config: NTP: Server2: time.cloudflare.com"));
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, string("config: NTP: Server3: pool.ntp.org"));
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, string("config: NTP: Server4: time.ruuvi.com"));
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, string("config: use company id filter: 1"));
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, string("config: company id: 0x0499"));
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, string("config: use scan coded phy: 0"));
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, string("config: use scan 1mbit/phy: 1"));
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, string("config: use scan 2mbit/phy: 1"));
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, string("config: use scan channel 37: 1"));
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, string("config: use scan channel 38: 1"));
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, string("config: use scan channel 39: 1"));
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, string("config: scan default       : 1"));
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, string("config: use scan filter: no"));
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, string("config: coordinates: "));
    TEST_CHECK_LOG_RECORD_GW_CFG(
        ESP_LOG_INFO,
        string("config: fw_update: url: https://network.ruuvi.com/firmwareupdate"));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    os_malloc_trace_dump();
    ESP_LOG_WRAPPER_TEST_CHECK_LOG_RECORD("MEM_TRACE", ESP_LOG_INFO, "Num blocks allocated: 0");
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_EQ(0, this->m_alloc_free_call_count);
}

TEST_F(TestHttpServerCb, http_server_cb_on_post_ruuvi_json_ok) // NOLINT
{
    const char*              expected_resp = "{}";
    const http_server_resp_t resp          = http_server_cb_on_post(
        "ruuvi.json",
        nullptr,
        "{"
                 "\"remote_cfg_use\":false,\n"
                 "\"remote_cfg_url\":\"\",\n"
                 "\"remote_cfg_auth_type\":\"none\",\n"
                 "\"remote_cfg_use_ssl_client_cert\":\tfalse,\n"
                 "\"remote_cfg_use_ssl_server_cert\":\tfalse,\n"
                 "\"remote_cfg_refresh_interval_minutes\":0,\n"
                 "\"use_http_ruuvi\":false,"
                 "\"use_http\":false,"
                 "\"http_data_format\":\"ruuvi\","
                 "\"http_auth\":\"none\","
                 "\"http_url\":\"" RUUVI_GATEWAY_HTTP_DEFAULT_URL
        "\","
                 "\"http_user\":\"\","
                 "\"http_pass\":\"\","
                 "\"use_http_stat\":true,"
                 "\"http_stat_url\":\"" RUUVI_GATEWAY_HTTP_STATUS_URL
        "\","
                 "\"http_stat_user\":\"\","
                 "\"http_stat_pass\":\"\","
                 "\"http_stat_use_ssl_client_cert\":\tfalse,\n"
                 "\"http_stat_use_ssl_server_cert\":\tfalse,\n"
                 "\"use_mqtt\":true,"
                 "\"mqtt_disable_retained_messages\":false,"
                 "\"mqtt_transport\":\"TCP\","
                 "\t\"mqtt_data_format\":\t\"ruuvi_raw\",\n"
                 "\"mqtt_server\":\"test.mosquitto.org\","
                 "\"mqtt_port\":1883,"
                 "\"mqtt_sending_interval\":0,"
                 "\"mqtt_prefix\":\"ruuvi/30:AE:A4:02:84:A4\","
                 "\"mqtt_client_id\":\"30:AE:A4:02:84:A4\","
                 "\"mqtt_user\":\"\","
                 "\"mqtt_pass\":\"\","
                 "\"mqtt_use_ssl_client_cert\":\tfalse,\n"
                 "\"mqtt_use_ssl_server_cert\":\tfalse,\n"
                 "\"company_use_filtering\":true"
                 "}",
        false);

    ASSERT_TRUE(this->m_flag_settings_saved_to_flash);
    ASSERT_FALSE(this->m_flag_settings_ethernet_ip_updated);

    ASSERT_EQ(HTTP_RESP_CODE_200, resp.http_resp_code);
    ASSERT_EQ(HTTP_CONTENT_LOCATION_FLASH_MEM, resp.content_location);
    ASSERT_TRUE(resp.flag_no_cache);
    ASSERT_EQ(HTTP_CONTENT_TYPE_APPLICATION_JSON, resp.content_type);
    ASSERT_EQ(nullptr, resp.p_content_type_param);
    ASSERT_EQ(strlen(expected_resp), resp.content_len);
    ASSERT_EQ(HTTP_CONTENT_ENCODING_NONE, resp.content_encoding);
    ASSERT_NE(nullptr, resp.select_location.flash.p_buf);
    ASSERT_EQ(string(expected_resp), string(reinterpret_cast<const char*>(resp.select_location.flash.p_buf)));
    TEST_CHECK_LOG_RECORD_HTTP_SERVER(ESP_LOG_DEBUG, string("http_server_cb_on_post /ruuvi.json, params="));
    TEST_CHECK_LOG_RECORD_HTTP_SERVER(
        ESP_LOG_INFO,
        string("http_server_cb_on_post: Clear all saved TLS session tickets"));
    TEST_CHECK_LOG_RECORD_HTTP_SERVER(ESP_LOG_DEBUG, string("POST /ruuvi.json, flag_access_from_lan=0"));
    TEST_CHECK_LOG_RECORD_HTTP_SERVER(
        ESP_LOG_DEBUG,
        string("POST /ruuvi.json, body: {"
               "\"remote_cfg_use\":false,\n"
               "\"remote_cfg_url\":\"\","
               "\n\"remote_cfg_auth_type\":\"none\",\n"
               "\"remote_cfg_use_ssl_client_cert\":\tfalse,\n"
               "\"remote_cfg_use_ssl_server_cert\":\tfalse,\n"
               "\"remote_cfg_refresh_interval_minutes\":0,\n"
               "\"use_http_ruuvi\":false,"
               "\"use_http\":false,"
               "\"http_data_format\":\"ruuvi\","
               "\"http_auth\":\"none\","
               "\"http_url\":\"https://network.ruuvi.com/record\","
               "\"http_user\":\"\","
               "\"http_pass\":\"\","
               "\"use_http_stat\":true,"
               "\"http_stat_url\":\"https://network.ruuvi.com/status\","
               "\"http_stat_user\":\"\","
               "\"http_stat_pass\":\"\","
               "\"http_stat_use_ssl_client_cert\":\tfalse,\n"
               "\"http_stat_use_ssl_server_cert\":\tfalse,\n"
               "\"use_mqtt\":true,"
               "\"mqtt_disable_retained_messages\":false,"
               "\"mqtt_transport\":\"TCP\","
               "\t\"mqtt_data_format\":\t\"ruuvi_raw\",\n"
               "\"mqtt_server\":\"test.mosquitto.org\","
               "\"mqtt_port\":1883,"
               "\"mqtt_sending_interval\":0,"
               "\"mqtt_prefix\":\"ruuvi/30:AE:A4:02:84:A4\","
               "\"mqtt_client_id\":\"30:AE:A4:02:84:A4\","
               "\"mqtt_user\":\"\","
               "\"mqtt_pass\":\"\","
               "\"mqtt_use_ssl_client_cert\":\tfalse,\n"
               "\"mqtt_use_ssl_server_cert\":\tfalse,\n"
               "\"company_use_filtering\":true"
               "}"));

    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, "Gateway SETTINGS (via HTTP):");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "remote_cfg_use: 0");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "remote_cfg_url: ");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "remote_cfg_auth_type: none");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "remote_cfg_use_ssl_client_cert: 0");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "remote_cfg_use_ssl_server_cert: 0");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "remote_cfg_refresh_interval_minutes: 0");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "use_http_ruuvi: 0");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "use_http: 0");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "http_data_format: ruuvi");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "http_auth: none");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "http_url: " RUUVI_GATEWAY_HTTP_DEFAULT_URL);
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "http_period: not found or invalid");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "Can't find key 'http_period' in config-json");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "http_use_extra_http_path: not found");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "Can't find key 'http_use_extra_http_path' in config-json");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "http_use_extra_http_query: not found");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "Can't find key 'http_use_extra_http_query' in config-json");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "http_use_extra_http_headers: not found");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "Can't find key 'http_use_extra_http_headers' in config-json");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "http_use_ssl_client_cert: not found");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "Can't find key 'http_use_ssl_client_cert' in config-json");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "http_use_ssl_server_cert: not found");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "Can't find key 'http_use_ssl_server_cert' in config-json");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "use_http_stat: 1");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "http_stat_url: " RUUVI_GATEWAY_HTTP_STATUS_URL);
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "http_stat_user: ");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "http_stat_pass: ");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "http_stat_use_ssl_client_cert: 0");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "http_stat_use_ssl_server_cert: 0");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "use_mqtt: 1");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "mqtt_disable_retained_messages: 0");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "mqtt_transport: TCP");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "mqtt_data_format: ruuvi_raw");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "mqtt_server: test.mosquitto.org");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "mqtt_port: 1883");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "mqtt_sending_interval: 0");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "mqtt_prefix: ruuvi/30:AE:A4:02:84:A4");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "mqtt_client_id: 30:AE:A4:02:84:A4");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "mqtt_user: ");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "mqtt_pass: ");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "mqtt_use_ssl_client_cert: 0");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "mqtt_use_ssl_server_cert: 0");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "lan_auth_type: not found");
    TEST_CHECK_LOG_RECORD_GW_CFG(
        ESP_LOG_INFO,
        "Can't find key 'lan_auth_type' in config-json, leave the previous value unchanged");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "lan_auth_api_key: not found");
    TEST_CHECK_LOG_RECORD_GW_CFG(
        ESP_LOG_INFO,
        "Can't find key 'lan_auth_api_key' in config-json, leave the previous value unchanged");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "lan_auth_api_key_rw: not found");
    TEST_CHECK_LOG_RECORD_GW_CFG(
        ESP_LOG_INFO,
        "Can't find key 'lan_auth_api_key_rw' in config-json, leave the previous value unchanged");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "auto_update_cycle: not found");
    TEST_CHECK_LOG_RECORD_GW_CFG(
        ESP_LOG_WARN,
        "Can't find key 'auto_update_cycle' in config-json, leave the previous value unchanged");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "auto_update_weekdays_bitmask: not found or invalid");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_WARN, "Can't find key 'auto_update_weekdays_bitmask' in config-json");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "auto_update_interval_from: not found or invalid");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_WARN, "Can't find key 'auto_update_interval_from' in config-json");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "auto_update_interval_to: not found or invalid");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_WARN, "Can't find key 'auto_update_interval_to' in config-json");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "auto_update_tz_offset_hours: not found or invalid");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_WARN, "Can't find key 'auto_update_tz_offset_hours' in config-json");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "ntp_use: not found");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_WARN, "Can't find key 'ntp_use' in config-json");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "ntp_use_dhcp: not found");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_WARN, "Can't find key 'ntp_use_dhcp' in config-json");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "ntp_server1: not found");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_WARN, "Can't find key 'ntp_server1' in config-json");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "ntp_server2: not found");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_WARN, "Can't find key 'ntp_server2' in config-json");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "ntp_server3: not found");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_WARN, "Can't find key 'ntp_server3' in config-json");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "ntp_server4: not found");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_WARN, "Can't find key 'ntp_server4' in config-json");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "company_id: not found or invalid");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_WARN, "Can't find key 'company_id' in config-json");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "company_use_filtering: 1");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "scan_default: not found");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_WARN, "Can't find key 'scan_default' in config-json");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "scan_filter_allow_listed: not found");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_WARN, "Can't find key 'scan_filter_allow_listed' in config-json");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_WARN, "Can't find key 'scan_filter_list' in config-json");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "coordinates: not found");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_WARN, "Can't find key 'coordinates' in config-json");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "fw_update_url: not found");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_WARN, "Can't find key 'fw_update_url' in config-json");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, string("config: use remote cfg: 0"));
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, string("config: remote cfg: URL: "));
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, string("config: remote cfg: auth_type: none"));
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, string("config: remote cfg: use SSL client cert: 0"));
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, string("config: remote cfg: use SSL server cert: 0"));
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, string("config: remote cfg: refresh_interval_minutes: 0"));
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, string("config: use http ruuvi: 0"));
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, string("config: use http: 0"));
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, string("config: use http_stat: 1"));
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, string("config: http_stat url: " RUUVI_GATEWAY_HTTP_STATUS_URL));
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, string("config: http_stat user: "));
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, string("config: http_stat pass: "));
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, string("config: http_stat: use SSL client cert: 0"));
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, string("config: http_stat: use SSL server cert: 0"));
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, string("config: use mqtt: 1"));
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, string("config: mqtt disable retained messages: 0"));
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, string("config: mqtt transport: TCP"));
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, string("config: mqtt data format: ruuvi_raw"));
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, string("config: mqtt server: test.mosquitto.org"));
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, string("config: mqtt port: 1883"));
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, string("config: mqtt sending interval: 0"));
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, string("config: mqtt prefix: ruuvi/30:AE:A4:02:84:A4"));
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, string("config: mqtt client id: 30:AE:A4:02:84:A4"));
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, string("config: mqtt user: "));
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, string("config: mqtt password: "));
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, string("config: mqtt: use SSL client cert: 0"));
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, string("config: mqtt: use SSL server cert: 0"));
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, string("config: LAN auth type: lan_auth_default"));
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, string("config: LAN auth user: Admin"));
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, string("config: LAN auth pass: f32dd273cd874d98ec4fc21d534e3e61"));
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, string("config: LAN auth API key: "));
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, string("config: LAN auth API key (RW): "));
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, string("config: Auto update cycle: regular"));
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, string("config: Auto update weekdays_bitmask: 0x7f"));
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, string("config: Auto update interval: 00:00..24:00"));
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, string("config: Auto update TZ: UTC+3"));
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, string("config: NTP: Use: yes"));
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, string("config: NTP: Use DHCP: no"));
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, string("config: NTP: Server1: time.google.com"));
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, string("config: NTP: Server2: time.cloudflare.com"));
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, string("config: NTP: Server3: pool.ntp.org"));
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, string("config: NTP: Server4: time.ruuvi.com"));
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, string("config: use company id filter: 1"));
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, string("config: company id: 0x0499"));
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, string("config: use scan coded phy: 0"));
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, string("config: use scan 1mbit/phy: 1"));
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, string("config: use scan 2mbit/phy: 1"));
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, string("config: use scan channel 37: 1"));
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, string("config: use scan channel 38: 1"));
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, string("config: use scan channel 39: 1"));
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, string("config: scan default       : 1"));
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, string("config: use scan filter: no"));
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, string("config: coordinates: "));
    TEST_CHECK_LOG_RECORD_GW_CFG(
        ESP_LOG_INFO,
        string("config: fw_update: url: https://network.ruuvi.com/firmwareupdate"));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    os_malloc_trace_dump();
    ESP_LOG_WRAPPER_TEST_CHECK_LOG_RECORD("MEM_TRACE", ESP_LOG_INFO, "Num blocks allocated: 0");
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_EQ(0, this->m_alloc_free_call_count);
}

TEST_F(TestHttpServerCb, http_server_cb_on_post_ruuvi_json_ok_wifi_ap_active) // NOLINT
{
    this->m_flag_is_ap_active              = true;
    const char*              expected_resp = "{}";
    const http_server_resp_t resp          = http_server_cb_on_post(
        "ruuvi.json",
        nullptr,
        "{"
                 "\"remote_cfg_use\":false,\n"
                 "\"remote_cfg_url\":\"\",\n"
                 "\"remote_cfg_auth_type\":\"none\",\n"
                 "\"remote_cfg_use_ssl_client_cert\":\tfalse,\n"
                 "\"remote_cfg_use_ssl_server_cert\":\tfalse,\n"
                 "\"remote_cfg_refresh_interval_minutes\":0,\n"
                 "\"use_http_ruuvi\":false,"
                 "\"use_http\":false,"
                 "\"http_data_format\":\"ruuvi\","
                 "\"http_auth\":\"none\","
                 "\"http_url\":\"" RUUVI_GATEWAY_HTTP_DEFAULT_URL
        "\","
                 "\"http_user\":\"\","
                 "\"http_pass\":\"\","
                 "\"use_http_stat\":true,"
                 "\"http_stat_url\":\"" RUUVI_GATEWAY_HTTP_STATUS_URL
        "\","
                 "\"http_stat_user\":\"\","
                 "\"http_stat_pass\":\"\","
                 "\"http_stat_use_ssl_client_cert\":\tfalse,\n"
                 "\"http_stat_use_ssl_server_cert\":\tfalse,\n"
                 "\"use_mqtt\":true,"
                 "\"mqtt_disable_retained_messages\":false,"
                 "\"mqtt_transport\":\"TCP\","
                 "\t\"mqtt_data_format\":\t\"ruuvi_raw\",\n"
                 "\"mqtt_server\":\"test.mosquitto.org\","
                 "\"mqtt_port\":1883,"
                 "\"mqtt_sending_interval\":0,"
                 "\"mqtt_prefix\":\"ruuvi/30:AE:A4:02:84:A4\","
                 "\"mqtt_client_id\":\"30:AE:A4:02:84:A4\","
                 "\"mqtt_user\":\"\","
                 "\"mqtt_pass\":\"\","
                 "\"mqtt_use_ssl_client_cert\":\tfalse,\n"
                 "\"mqtt_use_ssl_server_cert\":\tfalse,\n"
                 "\"company_use_filtering\":true"
                 "}",
        false);

    ASSERT_TRUE(this->m_flag_settings_saved_to_flash);
    ASSERT_FALSE(this->m_flag_settings_ethernet_ip_updated);

    ASSERT_EQ(HTTP_RESP_CODE_200, resp.http_resp_code);
    ASSERT_EQ(HTTP_CONTENT_LOCATION_FLASH_MEM, resp.content_location);
    ASSERT_TRUE(resp.flag_no_cache);
    ASSERT_EQ(HTTP_CONTENT_TYPE_APPLICATION_JSON, resp.content_type);
    ASSERT_EQ(nullptr, resp.p_content_type_param);
    ASSERT_EQ(strlen(expected_resp), resp.content_len);
    ASSERT_EQ(HTTP_CONTENT_ENCODING_NONE, resp.content_encoding);
    ASSERT_NE(nullptr, resp.select_location.flash.p_buf);
    ASSERT_EQ(string(expected_resp), string(reinterpret_cast<const char*>(resp.select_location.flash.p_buf)));
    TEST_CHECK_LOG_RECORD_HTTP_SERVER(ESP_LOG_DEBUG, string("http_server_cb_on_post /ruuvi.json, params="));
    TEST_CHECK_LOG_RECORD_HTTP_SERVER(
        ESP_LOG_INFO,
        string("http_server_cb_on_post: Clear all saved TLS session tickets"));
    TEST_CHECK_LOG_RECORD_HTTP_SERVER(ESP_LOG_DEBUG, string("POST /ruuvi.json, flag_access_from_lan=0"));
    TEST_CHECK_LOG_RECORD_HTTP_SERVER(
        ESP_LOG_DEBUG,
        string("POST /ruuvi.json, body: {"
               "\"remote_cfg_use\":false,\n"
               "\"remote_cfg_url\":\"\",\n"
               "\"remote_cfg_auth_type\":\"none\",\n"
               "\"remote_cfg_use_ssl_client_cert\":\tfalse,\n"
               "\"remote_cfg_use_ssl_server_cert\":\tfalse,\n"
               "\"remote_cfg_refresh_interval_minutes\":0,\n"
               "\"use_http_ruuvi\":false,"
               "\"use_http\":false,"
               "\"http_data_format\":\"ruuvi\","
               "\"http_auth\":\"none\","
               "\"http_url\":\"https://network.ruuvi.com/record\","
               "\"http_user\":\"\","
               "\"http_pass\":\"\","
               "\"use_http_stat\":true,"
               "\"http_stat_url\":\"https://network.ruuvi.com/status\","
               "\"http_stat_user\":\"\","
               "\"http_stat_pass\":\"\","
               "\"http_stat_use_ssl_client_cert\":\tfalse,\n"
               "\"http_stat_use_ssl_server_cert\":\tfalse,\n"
               "\"use_mqtt\":true,"
               "\"mqtt_disable_retained_messages\":false,"
               "\"mqtt_transport\":\"TCP\","
               "\t\"mqtt_data_format\":\t\"ruuvi_raw\",\n"
               "\"mqtt_server\":\"test.mosquitto.org\","
               "\"mqtt_port\":1883,"
               "\"mqtt_sending_interval\":0,"
               "\"mqtt_prefix\":\"ruuvi/30:AE:A4:02:84:A4\","
               "\"mqtt_client_id\":\"30:AE:A4:02:84:A4\","
               "\"mqtt_user\":\"\","
               "\"mqtt_pass\":\"\","
               "\"mqtt_use_ssl_client_cert\":\tfalse,\n"
               "\"mqtt_use_ssl_server_cert\":\tfalse,\n"
               "\"company_use_filtering\":true"
               "}"));

    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, "Gateway SETTINGS (via HTTP):");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "remote_cfg_use: 0");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "remote_cfg_url: ");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "remote_cfg_auth_type: none");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "remote_cfg_use_ssl_client_cert: 0");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "remote_cfg_use_ssl_server_cert: 0");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "remote_cfg_refresh_interval_minutes: 0");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "use_http_ruuvi: 0");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "use_http: 0");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "http_data_format: ruuvi");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "http_auth: none");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "http_url: " RUUVI_GATEWAY_HTTP_DEFAULT_URL);
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "http_period: not found or invalid");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "Can't find key 'http_period' in config-json");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "http_use_extra_http_path: not found");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "Can't find key 'http_use_extra_http_path' in config-json");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "http_use_extra_http_query: not found");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "Can't find key 'http_use_extra_http_query' in config-json");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "http_use_extra_http_headers: not found");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "Can't find key 'http_use_extra_http_headers' in config-json");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "http_use_ssl_client_cert: not found");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "Can't find key 'http_use_ssl_client_cert' in config-json");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "http_use_ssl_server_cert: not found");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "Can't find key 'http_use_ssl_server_cert' in config-json");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "use_http_stat: 1");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "http_stat_url: " RUUVI_GATEWAY_HTTP_STATUS_URL);
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "http_stat_user: ");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "http_stat_pass: ");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "http_stat_use_ssl_client_cert: 0");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "http_stat_use_ssl_server_cert: 0");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "use_mqtt: 1");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "mqtt_disable_retained_messages: 0");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "mqtt_transport: TCP");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "mqtt_data_format: ruuvi_raw");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "mqtt_server: test.mosquitto.org");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "mqtt_port: 1883");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "mqtt_sending_interval: 0");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "mqtt_prefix: ruuvi/30:AE:A4:02:84:A4");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "mqtt_client_id: 30:AE:A4:02:84:A4");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "mqtt_user: ");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "mqtt_pass: ");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "mqtt_use_ssl_client_cert: 0");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "mqtt_use_ssl_server_cert: 0");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "lan_auth_type: not found");
    TEST_CHECK_LOG_RECORD_GW_CFG(
        ESP_LOG_INFO,
        "Can't find key 'lan_auth_type' in config-json, leave the previous value unchanged");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "lan_auth_api_key: not found");
    TEST_CHECK_LOG_RECORD_GW_CFG(
        ESP_LOG_INFO,
        "Can't find key 'lan_auth_api_key' in config-json, leave the previous value unchanged");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "lan_auth_api_key_rw: not found");
    TEST_CHECK_LOG_RECORD_GW_CFG(
        ESP_LOG_INFO,
        "Can't find key 'lan_auth_api_key_rw' in config-json, leave the previous value unchanged");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "auto_update_cycle: not found");
    TEST_CHECK_LOG_RECORD_GW_CFG(
        ESP_LOG_WARN,
        "Can't find key 'auto_update_cycle' in config-json, leave the previous value unchanged");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "auto_update_weekdays_bitmask: not found or invalid");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_WARN, "Can't find key 'auto_update_weekdays_bitmask' in config-json");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "auto_update_interval_from: not found or invalid");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_WARN, "Can't find key 'auto_update_interval_from' in config-json");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "auto_update_interval_to: not found or invalid");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_WARN, "Can't find key 'auto_update_interval_to' in config-json");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "auto_update_tz_offset_hours: not found or invalid");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_WARN, "Can't find key 'auto_update_tz_offset_hours' in config-json");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "ntp_use: not found");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_WARN, "Can't find key 'ntp_use' in config-json");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "ntp_use_dhcp: not found");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_WARN, "Can't find key 'ntp_use_dhcp' in config-json");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "ntp_server1: not found");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_WARN, "Can't find key 'ntp_server1' in config-json");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "ntp_server2: not found");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_WARN, "Can't find key 'ntp_server2' in config-json");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "ntp_server3: not found");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_WARN, "Can't find key 'ntp_server3' in config-json");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "ntp_server4: not found");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_WARN, "Can't find key 'ntp_server4' in config-json");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "company_id: not found or invalid");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_WARN, "Can't find key 'company_id' in config-json");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "company_use_filtering: 1");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "scan_default: not found");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_WARN, "Can't find key 'scan_default' in config-json");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "scan_filter_allow_listed: not found");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_WARN, "Can't find key 'scan_filter_allow_listed' in config-json");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_WARN, "Can't find key 'scan_filter_list' in config-json");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "coordinates: not found");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_WARN, "Can't find key 'coordinates' in config-json");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "fw_update_url: not found");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_WARN, "Can't find key 'fw_update_url' in config-json");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, string("config: use remote cfg: 0"));
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, string("config: remote cfg: URL: "));
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, string("config: remote cfg: auth_type: none"));
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, string("config: remote cfg: use SSL client cert: 0"));
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, string("config: remote cfg: use SSL server cert: 0"));
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, string("config: remote cfg: refresh_interval_minutes: 0"));
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, string("config: use http ruuvi: 0"));
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, string("config: use http: 0"));
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, string("config: use http_stat: 1"));
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, string("config: http_stat url: " RUUVI_GATEWAY_HTTP_STATUS_URL));
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, string("config: http_stat user: "));
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, string("config: http_stat pass: "));
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, string("config: http_stat: use SSL client cert: 0"));
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, string("config: http_stat: use SSL server cert: 0"));
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, string("config: use mqtt: 1"));
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, string("config: mqtt disable retained messages: 0"));
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, string("config: mqtt transport: TCP"));
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, string("config: mqtt data format: ruuvi_raw"));
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, string("config: mqtt server: test.mosquitto.org"));
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, string("config: mqtt port: 1883"));
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, string("config: mqtt sending interval: 0"));
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, string("config: mqtt prefix: ruuvi/30:AE:A4:02:84:A4"));
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, string("config: mqtt client id: 30:AE:A4:02:84:A4"));
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, string("config: mqtt user: "));
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, string("config: mqtt password: "));
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, string("config: mqtt: use SSL client cert: 0"));
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, string("config: mqtt: use SSL server cert: 0"));
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, string("config: LAN auth type: lan_auth_default"));
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, string("config: LAN auth user: Admin"));
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, string("config: LAN auth pass: f32dd273cd874d98ec4fc21d534e3e61"));
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, string("config: LAN auth API key: "));
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, string("config: LAN auth API key (RW): "));
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, string("config: Auto update cycle: regular"));
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, string("config: Auto update weekdays_bitmask: 0x7f"));
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, string("config: Auto update interval: 00:00..24:00"));
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, string("config: Auto update TZ: UTC+3"));
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, string("config: NTP: Use: yes"));
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, string("config: NTP: Use DHCP: no"));
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, string("config: NTP: Server1: time.google.com"));
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, string("config: NTP: Server2: time.cloudflare.com"));
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, string("config: NTP: Server3: pool.ntp.org"));
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, string("config: NTP: Server4: time.ruuvi.com"));
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, string("config: use company id filter: 1"));
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, string("config: company id: 0x0499"));
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, string("config: use scan coded phy: 0"));
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, string("config: use scan 1mbit/phy: 1"));
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, string("config: use scan 2mbit/phy: 1"));
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, string("config: use scan channel 37: 1"));
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, string("config: use scan channel 38: 1"));
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, string("config: use scan channel 39: 1"));
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, string("config: scan default       : 1"));
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, string("config: use scan filter: no"));
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, string("config: coordinates: "));
    TEST_CHECK_LOG_RECORD_GW_CFG(
        ESP_LOG_INFO,
        string("config: fw_update: url: https://network.ruuvi.com/firmwareupdate"));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    os_malloc_trace_dump();
    ESP_LOG_WRAPPER_TEST_CHECK_LOG_RECORD("MEM_TRACE", ESP_LOG_INFO, "Num blocks allocated: 0");
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_EQ(0, this->m_alloc_free_call_count);
}

TEST_F(TestHttpServerCb, http_server_cb_on_post_unknown_json) // NOLINT
{
    const http_server_resp_t resp = http_server_cb_on_post(
        "unknown.json",
        nullptr,
        "{"
        "\"use_mqtt\":true,"
        "\"mqtt_disable_retained_messages\":false,"
        "\"mqtt_server\":\"test.mosquitto.org\","
        "\"mqtt_port\":1883,"
        "\"mqtt_sending_interval\":0,"
        "\"mqtt_prefix\":\"ruuvi/30:AE:A4:02:84:A4\","
        "\"mqtt_client_id\":\"30:AE:A4:02:84:A4\","
        "\"mqtt_user\":\"\","
        "\"mqtt_pass\":\"\","
        "\"mqtt_use_ssl_client_cert\":\tfalse,\n"
        "\"mqtt_use_ssl_server_cert\":\tfalse,\n"
        "\"use_http_ruuvi\":false,"
        "\"use_http\":false,"
        "\"http_url\":\"" RUUVI_GATEWAY_HTTP_DEFAULT_URL
        "\","
        "\"http_user\":\"\","
        "\"http_pass\":\"\","
        "\"use_http_stat\":true,"
        "\"http_stat_url\":\"" RUUVI_GATEWAY_HTTP_STATUS_URL
        "\","
        "\"http_stat_user\":\"\","
        "\"http_stat_pass\":\"\","
        "\"company_use_filtering\":true"
        "}",
        false);

    ASSERT_FALSE(this->m_flag_settings_saved_to_flash);
    ASSERT_FALSE(this->m_flag_settings_ethernet_ip_updated);

    ASSERT_EQ(HTTP_RESP_CODE_404, resp.http_resp_code);
    ASSERT_EQ(HTTP_CONTENT_LOCATION_NO_CONTENT, resp.content_location);
    ASSERT_TRUE(resp.flag_no_cache);
    ASSERT_EQ(HTTP_CONTENT_TYPE_TEXT_HTML, resp.content_type);
    ASSERT_EQ(nullptr, resp.p_content_type_param);
    ASSERT_EQ(0, resp.content_len);
    ASSERT_EQ(HTTP_CONTENT_ENCODING_NONE, resp.content_encoding);
    TEST_CHECK_LOG_RECORD_HTTP_SERVER(ESP_LOG_DEBUG, string("http_server_cb_on_post /unknown.json, params="));
    TEST_CHECK_LOG_RECORD_HTTP_SERVER(ESP_LOG_WARN, string("POST /unknown.json"));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    os_malloc_trace_dump();
    ESP_LOG_WRAPPER_TEST_CHECK_LOG_RECORD("MEM_TRACE", ESP_LOG_INFO, "Num blocks allocated: 0");
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_EQ(0, this->m_alloc_free_call_count);
}

TEST_F(TestHttpServerCb, http_server_cb_on_delete) // NOLINT
{
    const http_server_resp_t resp = http_server_cb_on_delete("unknown.json", nullptr, false, nullptr);

    ASSERT_EQ(HTTP_RESP_CODE_404, resp.http_resp_code);
    ASSERT_EQ(HTTP_CONTENT_LOCATION_NO_CONTENT, resp.content_location);
    ASSERT_TRUE(resp.flag_no_cache);
    ASSERT_EQ(HTTP_CONTENT_TYPE_TEXT_HTML, resp.content_type);
    ASSERT_EQ(nullptr, resp.p_content_type_param);
    ASSERT_EQ(0, resp.content_len);
    ASSERT_EQ(HTTP_CONTENT_ENCODING_NONE, resp.content_encoding);
    TEST_CHECK_LOG_RECORD_HTTP_SERVER(ESP_LOG_INFO, string("DELETE /unknown.json, params="));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    os_malloc_trace_dump();
    ESP_LOG_WRAPPER_TEST_CHECK_LOG_RECORD("MEM_TRACE", ESP_LOG_INFO, "Num blocks allocated: 0");
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_EQ(0, this->m_alloc_free_call_count);
}

TEST_F(TestHttpServerCb, http_server_cb_on_get_extra_cfg__no_file_param) // NOLINT
{
    const http_server_resp_t resp = http_server_cb_on_get("extra_cfg", "param=abc", false, nullptr);
    ASSERT_EQ(HTTP_RESP_CODE_400, resp.http_resp_code);
    esp_log_wrapper_clear();
    os_malloc_trace_dump();
    ESP_LOG_WRAPPER_TEST_CHECK_LOG_RECORD("MEM_TRACE", ESP_LOG_INFO, "Num blocks allocated: 0");
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_EQ(0, this->m_alloc_free_call_count);
}

TEST_F(TestHttpServerCb, http_server_cb_on_get_extra_cfg__unknown_file) // NOLINT
{
    const http_server_resp_t resp = http_server_cb_on_get("extra_cfg", "file=unknown_file", false, nullptr);
    ASSERT_EQ(HTTP_RESP_CODE_400, resp.http_resp_code);
    esp_log_wrapper_clear();
    os_malloc_trace_dump();
    ESP_LOG_WRAPPER_TEST_CHECK_LOG_RECORD("MEM_TRACE", ESP_LOG_INFO, "Num blocks allocated: 0");
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_EQ(0, this->m_alloc_free_call_count);
}

TEST_F(TestHttpServerCb, http_server_cb_on_get_extra_cfg__non_blob_file_access_denied) // NOLINT
{
    const http_server_resp_t resp = http_server_cb_on_get(
        "extra_cfg",
        "file=" GW_CFG_STORAGE_SSL_HTTP_CLI_CERT,
        false,
        nullptr);
    ASSERT_EQ(HTTP_RESP_CODE_403, resp.http_resp_code);
    esp_log_wrapper_clear();
    os_malloc_trace_dump();
    ESP_LOG_WRAPPER_TEST_CHECK_LOG_RECORD("MEM_TRACE", ESP_LOG_INFO, "Num blocks allocated: 0");
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_EQ(0, this->m_alloc_free_call_count);
}

TEST_F(TestHttpServerCb, http_server_cb_on_get_extra_cfg__blob_file_not_exist) // NOLINT
{
    g_extra_cfg_storage.p_file_name      = GW_CFG_STORAGE_HTTP_PATH;
    g_extra_cfg_storage.flag_file_exists = false;
    const http_server_resp_t resp        = http_server_cb_on_get(
        "extra_cfg",
        "file=" GW_CFG_STORAGE_HTTP_PATH,
        false,
        nullptr);
    ASSERT_EQ(HTTP_RESP_CODE_404, resp.http_resp_code);
    esp_log_wrapper_clear();
    os_malloc_trace_dump();
    ESP_LOG_WRAPPER_TEST_CHECK_LOG_RECORD("MEM_TRACE", ESP_LOG_INFO, "Num blocks allocated: 0");
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_EQ(0, this->m_alloc_free_call_count);
}

TEST_F(TestHttpServerCb, http_server_cb_on_get_extra_cfg__blob_file_read_fail) // NOLINT
{
    g_extra_cfg_storage.p_file_name      = GW_CFG_STORAGE_HTTP_PATH;
    g_extra_cfg_storage.flag_file_exists = true;
    g_extra_cfg_storage.file_size        = 10;
    g_extra_cfg_storage.p_content        = nullptr;
    const http_server_resp_t resp        = http_server_cb_on_get(
        "extra_cfg",
        "file=" GW_CFG_STORAGE_HTTP_PATH,
        false,
        nullptr);
    ASSERT_EQ(HTTP_RESP_CODE_500, resp.http_resp_code);
    esp_log_wrapper_clear();
    os_malloc_trace_dump();
    ESP_LOG_WRAPPER_TEST_CHECK_LOG_RECORD("MEM_TRACE", ESP_LOG_INFO, "Num blocks allocated: 0");
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_EQ(0, this->m_alloc_free_call_count);
}

TEST_F(TestHttpServerCb, http_server_cb_on_get_extra_cfg__blob_file_ok) // NOLINT
{
    g_extra_cfg_storage.p_file_name      = GW_CFG_STORAGE_HTTP_PATH;
    g_extra_cfg_storage.flag_file_exists = true;
    g_extra_cfg_storage.file_size        = 14;
    g_extra_cfg_storage.p_content        = "/my/extra/path";
    http_server_resp_t resp = http_server_cb_on_get("extra_cfg", "file=" GW_CFG_STORAGE_HTTP_PATH, false, nullptr);
    ASSERT_EQ(HTTP_RESP_CODE_200, resp.http_resp_code);
    ASSERT_EQ(HTTP_CONTENT_LOCATION_HEAP, resp.content_location);
    ASSERT_EQ(HTTP_CONTENT_TYPE_TEXT_PLAIN, resp.content_type);
    ASSERT_EQ(14, resp.content_len);
    ASSERT_EQ(string("/my/extra/path"), string((const char*)resp.select_location.heap.p_buf, resp.content_len));
    http_server_resp_free(&resp);
    esp_log_wrapper_clear();
    os_malloc_trace_dump();
    ESP_LOG_WRAPPER_TEST_CHECK_LOG_RECORD("MEM_TRACE", ESP_LOG_INFO, "Num blocks allocated: 0");
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_EQ(0, this->m_alloc_free_call_count);
}

TEST_F(TestHttpServerCb, http_server_cb_on_post_extra_cfg__no_file_param) // NOLINT
{
    const http_server_resp_t resp = http_server_cb_on_post("extra_cfg", "param=abc", "content", false);
    ASSERT_EQ(HTTP_RESP_CODE_400, resp.http_resp_code);
    esp_log_wrapper_clear();
    os_malloc_trace_dump();
    ESP_LOG_WRAPPER_TEST_CHECK_LOG_RECORD("MEM_TRACE", ESP_LOG_INFO, "Num blocks allocated: 0");
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_EQ(0, this->m_alloc_free_call_count);
}

TEST_F(TestHttpServerCb, http_server_cb_on_post_extra_cfg__unknown_file) // NOLINT
{
    const http_server_resp_t resp = http_server_cb_on_post("extra_cfg", "file=unknown_file", "content", false);
    ASSERT_EQ(HTTP_RESP_CODE_400, resp.http_resp_code);
    esp_log_wrapper_clear();
    os_malloc_trace_dump();
    ESP_LOG_WRAPPER_TEST_CHECK_LOG_RECORD("MEM_TRACE", ESP_LOG_INFO, "Num blocks allocated: 0");
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_EQ(0, this->m_alloc_free_call_count);
}

TEST_F(TestHttpServerCb, http_server_cb_on_post_extra_cfg__blob_write_ok) // NOLINT
{
    g_extra_cfg_storage.flag_write_success = true;
    http_server_resp_t resp                = http_server_cb_on_post(
        "extra_cfg",
        "file=" GW_CFG_STORAGE_HTTP_PATH,
        "/my/extra/path",
        false);
    ASSERT_EQ(HTTP_RESP_CODE_200, resp.http_resp_code);
    http_server_resp_free(&resp);
    esp_log_wrapper_clear();
    os_malloc_trace_dump();
    ESP_LOG_WRAPPER_TEST_CHECK_LOG_RECORD("MEM_TRACE", ESP_LOG_INFO, "Num blocks allocated: 0");
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_EQ(0, this->m_alloc_free_call_count);
}

TEST_F(TestHttpServerCb, http_server_cb_on_post_extra_cfg__blob_write_fail) // NOLINT
{
    g_extra_cfg_storage.flag_write_success = false;
    const http_server_resp_t resp          = http_server_cb_on_post(
        "extra_cfg",
        "file=" GW_CFG_STORAGE_HTTP_PATH,
        "/my/extra/path",
        false);
    ASSERT_EQ(HTTP_RESP_CODE_500, resp.http_resp_code);
    esp_log_wrapper_clear();
    os_malloc_trace_dump();
    ESP_LOG_WRAPPER_TEST_CHECK_LOG_RECORD("MEM_TRACE", ESP_LOG_INFO, "Num blocks allocated: 0");
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_EQ(0, this->m_alloc_free_call_count);
}

TEST_F(TestHttpServerCb, http_server_cb_on_post_extra_cfg__http_headers_without_crlf) // NOLINT
{
    g_extra_cfg_storage.flag_write_success = true;
    const http_server_resp_t resp          = http_server_cb_on_post(
        "extra_cfg",
        "file=" GW_CFG_STORAGE_HTTP_HEADERS,
        "X-My-Header: value",
        false);
    ASSERT_EQ(HTTP_RESP_CODE_400, resp.http_resp_code);
    esp_log_wrapper_clear();
    os_malloc_trace_dump();
    ESP_LOG_WRAPPER_TEST_CHECK_LOG_RECORD("MEM_TRACE", ESP_LOG_INFO, "Num blocks allocated: 0");
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_EQ(0, this->m_alloc_free_call_count);
}

TEST_F(TestHttpServerCb, http_server_cb_on_post_extra_cfg__http_headers_with_crlf_ok) // NOLINT
{
    g_extra_cfg_storage.flag_write_success = true;
    http_server_resp_t resp                = http_server_cb_on_post(
        "extra_cfg",
        "file=" GW_CFG_STORAGE_HTTP_HEADERS,
        "X-My-Header: value\r\n",
        false);
    ASSERT_EQ(HTTP_RESP_CODE_200, resp.http_resp_code);
    http_server_resp_free(&resp);
    esp_log_wrapper_clear();
    os_malloc_trace_dump();
    ESP_LOG_WRAPPER_TEST_CHECK_LOG_RECORD("MEM_TRACE", ESP_LOG_INFO, "Num blocks allocated: 0");
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_EQ(0, this->m_alloc_free_call_count);
}

TEST_F(TestHttpServerCb, http_server_cb_on_post_extra_cfg__string_write_ok) // NOLINT
{
    g_extra_cfg_storage.flag_write_success = true;
    http_server_resp_t resp                = http_server_cb_on_post(
        "extra_cfg",
        "file=" GW_CFG_STORAGE_SSL_HTTP_CLI_CERT,
        "-----BEGIN CERTIFICATE-----\ntest\n-----END CERTIFICATE-----\n",
        false);
    ASSERT_EQ(HTTP_RESP_CODE_200, resp.http_resp_code);
    http_server_resp_free(&resp);
    esp_log_wrapper_clear();
    os_malloc_trace_dump();
    ESP_LOG_WRAPPER_TEST_CHECK_LOG_RECORD("MEM_TRACE", ESP_LOG_INFO, "Num blocks allocated: 0");
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_EQ(0, this->m_alloc_free_call_count);
}

TEST_F(TestHttpServerCb, http_server_cb_on_delete_extra_cfg__blob_file_ok) // NOLINT
{
    g_extra_cfg_storage.flag_delete_success = true;
    http_server_resp_t resp = http_server_cb_on_delete("extra_cfg", "file=" GW_CFG_STORAGE_HTTP_PATH, false, nullptr);
    ASSERT_EQ(HTTP_RESP_CODE_200, resp.http_resp_code);
    http_server_resp_free(&resp);
    esp_log_wrapper_clear();
    os_malloc_trace_dump();
    ESP_LOG_WRAPPER_TEST_CHECK_LOG_RECORD("MEM_TRACE", ESP_LOG_INFO, "Num blocks allocated: 0");
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_EQ(0, this->m_alloc_free_call_count);
}

TEST_F(TestHttpServerCb, http_server_cb_on_delete_extra_cfg__no_file_param) // NOLINT
{
    const http_server_resp_t resp = http_server_cb_on_delete("extra_cfg", "param=abc", false, nullptr);
    ASSERT_EQ(HTTP_RESP_CODE_400, resp.http_resp_code);
    esp_log_wrapper_clear();
    os_malloc_trace_dump();
    ESP_LOG_WRAPPER_TEST_CHECK_LOG_RECORD("MEM_TRACE", ESP_LOG_INFO, "Num blocks allocated: 0");
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_EQ(0, this->m_alloc_free_call_count);
}

TEST_F(TestHttpServerCb, http_server_cb_on_delete_extra_cfg__unknown_file) // NOLINT
{
    const http_server_resp_t resp = http_server_cb_on_delete("extra_cfg", "file=unknown_file", false, nullptr);
    ASSERT_EQ(HTTP_RESP_CODE_400, resp.http_resp_code);
    esp_log_wrapper_clear();
    os_malloc_trace_dump();
    ESP_LOG_WRAPPER_TEST_CHECK_LOG_RECORD("MEM_TRACE", ESP_LOG_INFO, "Num blocks allocated: 0");
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_EQ(0, this->m_alloc_free_call_count);
}

TEST_F(TestHttpServerCb, http_server_cb_on_delete_extra_cfg__delete_fail) // NOLINT
{
    g_extra_cfg_storage.flag_delete_success = false;
    const http_server_resp_t resp           = http_server_cb_on_delete(
        "extra_cfg",
        "file=" GW_CFG_STORAGE_HTTP_PATH,
        false,
        nullptr);
    ASSERT_EQ(HTTP_RESP_CODE_500, resp.http_resp_code);
    esp_log_wrapper_clear();
    os_malloc_trace_dump();
    ESP_LOG_WRAPPER_TEST_CHECK_LOG_RECORD("MEM_TRACE", ESP_LOG_INFO, "Num blocks allocated: 0");
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_EQ(0, this->m_alloc_free_call_count);
}

TEST_F(TestHttpServerCb, test_http_download_is_url_valid) // NOLINT
{
    ASSERT_FALSE(http_download_is_url_valid(""));
    ASSERT_FALSE(http_download_is_url_valid("abc.com"));
    ASSERT_FALSE(http_download_is_url_valid("http://ab"));
    ASSERT_FALSE(http_download_is_url_valid("http://abc"));
    ASSERT_FALSE(http_download_is_url_valid("https://ab"));
    ASSERT_FALSE(http_download_is_url_valid("https://abc"));
    ASSERT_TRUE(http_download_is_url_valid("http://a.c"));
    ASSERT_TRUE(http_download_is_url_valid("https://a.c"));
    ASSERT_TRUE(http_download_is_url_valid("http://a.c:80"));
    ASSERT_TRUE(http_download_is_url_valid("https://a.c:443"));
    os_malloc_trace_dump();
    ESP_LOG_WRAPPER_TEST_CHECK_LOG_RECORD("MEM_TRACE", ESP_LOG_INFO, "Num blocks allocated: 0");
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_EQ(0, this->m_alloc_free_call_count);
}

TEST_F(
    TestHttpServerCb,
    http_server_cb_on_get_validate_url_check_fw_update_url__good_json_without_beta__updates_mode_regular__update_needed) // NOLINT
{
    {
        gw_cfg_t gw_cfg = { 0 };
        gw_cfg_default_get(&gw_cfg);
        gw_cfg.ruuvi_cfg.auto_update.auto_update_cycle = AUTO_UPDATE_CYCLE_TYPE_REGULAR;
        gw_cfg_update(&gw_cfg);
        esp_log_wrapper_clear();
    }
    const char* const p_firmwareUpdateJson
        = "{"
          "  \"latest\": {"
          "    \"version\": \"v1.14.3\","
          "    \"url\": \"https://fwupdate.ruuvi.com/v1.14.3\","
          "    \"created_at\": \"2023-10-06T11:26:07Z\""
          "  }"
          "}";
    this->m_firmware_update_resp             = str_buf_printf_with_alloc("%s", p_firmwareUpdateJson);
    const char* arr_of_binaries[]            = { "https://fwupdate.ruuvi.com/v1.14.3/ruuvi_gateway_esp.bin",
                                                 "https://fwupdate.ruuvi.com/v1.14.3/fatfs_gwui.bin",
                                                 "https://fwupdate.ruuvi.com/v1.14.3/fatfs_nrf52.bin" };
    this->m_http_check_with_auth_arr_of_urls = arr_of_binaries;
    this->m_http_check_with_auth_num_of_urls = sizeof(arr_of_binaries) / sizeof(arr_of_binaries[0]);

    const char* const expected_json
        = "{\"status\": 200, \"json\": {  \"latest\": {    \"version\": \"v1.14.3\",    \"url\": "
          "\"https://fwupdate.ruuvi.com/v1.14.3\",    \"created_at\": \"2023-10-06T11:26:07Z\"  }}}";

    const char* const p_uri_params
        = "validate_type=check_fw_update_url&url=https://network.ruuvi.com/firmwareupdate&auth_type=none";

    http_server_resp_t resp = http_server_cb_on_get("validate_url", p_uri_params, true, nullptr);

    ASSERT_EQ(HTTP_RESP_CODE_200, resp.http_resp_code);
    ASSERT_EQ(HTTP_CONTENT_LOCATION_HEAP, resp.content_location);
    ASSERT_TRUE(resp.flag_no_cache);
    ASSERT_EQ(HTTP_CONTENT_TYPE_APPLICATION_JSON, resp.content_type);
    ASSERT_EQ(nullptr, resp.p_content_type_param);
    ASSERT_NE(nullptr, resp.select_location.heap.p_buf);
    ASSERT_EQ(string(expected_json), string(reinterpret_cast<const char*>(resp.select_location.heap.p_buf)));
    ASSERT_EQ(strlen(expected_json), resp.content_len);
    ASSERT_EQ(HTTP_CONTENT_ENCODING_NONE, resp.content_encoding);
    ASSERT_EQ(
        "D http_server: http_server_cb_on_get /validate_url?validate_type=check_fw_update_url&url=https://network.ruuvi.com/firmwareupdate&auth_type=none\n"
        "I http_server: http_server_cb_on_get: Clear all saved TLS session tickets\n"
        "D http_server: HTTP params: auth_type=none\n"
        "D http_server: HTTP params: key 'auth_type=': value (encoded): none\n"
        "D http_server: HTTP params: key 'auth_type=': value (decoded): none\n"
        "D http_server: HTTP params: url=https://network.ruuvi.com/firmwareupdate\n"
        "D http_server: HTTP params: key 'url=': value (encoded): https://network.ruuvi.com/firmwareupdate\n"
        "D http_server: HTTP params: key 'url=': value (decoded): https://network.ruuvi.com/firmwareupdate\n"
        "D http_server: Can't find key 'user=' in URL params\n"
        "E http_server: HTTP params: Can't find 'user='\n"
        "D http_server: Can't find key 'encrypted_password=' in URL params\n"
        "E http_server: HTTP params: Can't find 'encrypted_password='\n"
        "D http_server: Can't find key 'use_saved_password=' in URL params\n"
        "E http_server: HTTP params: Can't find 'use_saved_password='\n"
        "E http_server: Can't find key: use_saved_password=\n"
        "E http_server: Can't find prefix 'use_ssl_client_cert='\n"
        "E http_server: Can't find prefix 'use_ssl_server_cert='\n"
        "D http_server: Can't find prefix 'use_extra_http_path='\n"
        "D http_server: Can't find prefix 'use_extra_http_query='\n"
        "D http_server: Can't find prefix 'use_extra_http_headers='\n"
        "D http_server: Got URL from params: https://network.ruuvi.com/firmwareupdate\n"
        "I http_server: Found validate_type: check_fw_update_url\n"
        "I http_download: cb_on_http_download_json_data: buf_size=" + std::to_string(strlen(p_firmwareUpdateJson)) + "\n"
        "I http_download: http_download_json: completed within 0 ticks\n"
        "I http_server: Firmware update info (json): " + string(p_firmwareUpdateJson) + "\n"
        "I http_download: http_check: completed within 0 ticks\n"
        "D http_server: Feed watchdog\n"
        "I http_download: http_check: completed within 0 ticks\n"
        "D http_server: Feed watchdog\n"
        "I http_download: http_check: completed within 0 ticks\n"
        "D http_server: Feed watchdog\n",
        esp_log_wrapper_get_logs());
    http_server_resp_free(&resp);
    os_malloc_trace_dump();
    ESP_LOG_WRAPPER_TEST_CHECK_LOG_RECORD("MEM_TRACE", ESP_LOG_INFO, "Num blocks allocated: 0");
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_EQ(0, this->m_alloc_free_call_count);
}

TEST_F(
    TestHttpServerCb,
    http_server_cb_on_get_validate_url_check_fw_update_url__good_json_without_beta__updates_mode_beta__update_needed) // NOLINT
{
    {
        gw_cfg_t gw_cfg = { 0 };
        gw_cfg_default_get(&gw_cfg);
        gw_cfg.ruuvi_cfg.auto_update.auto_update_cycle = AUTO_UPDATE_CYCLE_TYPE_BETA_TESTER;
        gw_cfg_update(&gw_cfg);
        esp_log_wrapper_clear();
    }
    const char* const p_firmwareUpdateJson
        = "{"
          "  \"latest\": {"
          "    \"version\": \"v1.14.3\","
          "    \"url\": \"https://fwupdate.ruuvi.com/v1.14.3\","
          "    \"created_at\": \"2023-10-06T11:26:07Z\""
          "  }"
          "}";
    this->m_firmware_update_resp             = str_buf_printf_with_alloc("%s", p_firmwareUpdateJson);
    const char* arr_of_binaries[]            = { "https://fwupdate.ruuvi.com/v1.14.3/ruuvi_gateway_esp.bin",
                                                 "https://fwupdate.ruuvi.com/v1.14.3/fatfs_gwui.bin",
                                                 "https://fwupdate.ruuvi.com/v1.14.3/fatfs_nrf52.bin" };
    this->m_http_check_with_auth_arr_of_urls = arr_of_binaries;
    this->m_http_check_with_auth_num_of_urls = sizeof(arr_of_binaries) / sizeof(arr_of_binaries[0]);

    const char* const expected_json
        = "{\"status\": 200, \"json\": {  \"latest\": {    \"version\": \"v1.14.3\",    \"url\": "
          "\"https://fwupdate.ruuvi.com/v1.14.3\",    \"created_at\": \"2023-10-06T11:26:07Z\"  }}}";

    const char* const p_uri_params
        = "validate_type=check_fw_update_url&url=https://network.ruuvi.com/firmwareupdate&auth_type=none";

    http_server_resp_t resp = http_server_cb_on_get("validate_url", p_uri_params, true, nullptr);

    ASSERT_EQ(HTTP_RESP_CODE_200, resp.http_resp_code);
    ASSERT_EQ(HTTP_CONTENT_LOCATION_HEAP, resp.content_location);
    ASSERT_TRUE(resp.flag_no_cache);
    ASSERT_EQ(HTTP_CONTENT_TYPE_APPLICATION_JSON, resp.content_type);
    ASSERT_EQ(nullptr, resp.p_content_type_param);
    ASSERT_NE(nullptr, resp.select_location.heap.p_buf);
    ASSERT_EQ(string(expected_json), string(reinterpret_cast<const char*>(resp.select_location.heap.p_buf)));
    ASSERT_EQ(strlen(expected_json), resp.content_len);
    ASSERT_EQ(HTTP_CONTENT_ENCODING_NONE, resp.content_encoding);
    ASSERT_EQ(
        "D http_server: http_server_cb_on_get /validate_url?validate_type=check_fw_update_url&url=https://network.ruuvi.com/firmwareupdate&auth_type=none\n"
        "I http_server: http_server_cb_on_get: Clear all saved TLS session tickets\n"
        "D http_server: HTTP params: auth_type=none\n"
        "D http_server: HTTP params: key 'auth_type=': value (encoded): none\n"
        "D http_server: HTTP params: key 'auth_type=': value (decoded): none\n"
        "D http_server: HTTP params: url=https://network.ruuvi.com/firmwareupdate\n"
        "D http_server: HTTP params: key 'url=': value (encoded): https://network.ruuvi.com/firmwareupdate\n"
        "D http_server: HTTP params: key 'url=': value (decoded): https://network.ruuvi.com/firmwareupdate\n"
        "D http_server: Can't find key 'user=' in URL params\n"
        "E http_server: HTTP params: Can't find 'user='\n"
        "D http_server: Can't find key 'encrypted_password=' in URL params\n"
        "E http_server: HTTP params: Can't find 'encrypted_password='\n"
        "D http_server: Can't find key 'use_saved_password=' in URL params\n"
        "E http_server: HTTP params: Can't find 'use_saved_password='\n"
        "E http_server: Can't find key: use_saved_password=\n"
        "E http_server: Can't find prefix 'use_ssl_client_cert='\n"
        "E http_server: Can't find prefix 'use_ssl_server_cert='\n"
        "D http_server: Can't find prefix 'use_extra_http_path='\n"
        "D http_server: Can't find prefix 'use_extra_http_query='\n"
        "D http_server: Can't find prefix 'use_extra_http_headers='\n"
        "D http_server: Got URL from params: https://network.ruuvi.com/firmwareupdate\n"
        "I http_server: Found validate_type: check_fw_update_url\n"
        "I http_download: cb_on_http_download_json_data: buf_size=" + std::to_string(strlen(p_firmwareUpdateJson)) + "\n"
        "I http_download: http_download_json: completed within 0 ticks\n"
        "I http_server: Firmware update info (json): " + string(p_firmwareUpdateJson) + "\n"
        "I http_download: http_check: completed within 0 ticks\n"
        "D http_server: Feed watchdog\n"
        "I http_download: http_check: completed within 0 ticks\n"
        "D http_server: Feed watchdog\n"
        "I http_download: http_check: completed within 0 ticks\n"
        "D http_server: Feed watchdog\n",
        esp_log_wrapper_get_logs());
    http_server_resp_free(&resp);
    os_malloc_trace_dump();
    ESP_LOG_WRAPPER_TEST_CHECK_LOG_RECORD("MEM_TRACE", ESP_LOG_INFO, "Num blocks allocated: 0");
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_EQ(0, this->m_alloc_free_call_count);
}

TEST_F(
    TestHttpServerCb,
    http_server_cb_on_get_validate_url_check_fw_update_url__good_json_without_beta__updates_mode_manual__update_needed) // NOLINT
{
    {
        gw_cfg_t gw_cfg = { 0 };
        gw_cfg_default_get(&gw_cfg);
        gw_cfg.ruuvi_cfg.auto_update.auto_update_cycle = AUTO_UPDATE_CYCLE_TYPE_MANUAL;
        gw_cfg_update(&gw_cfg);
        esp_log_wrapper_clear();
    }
    const char* const p_firmwareUpdateJson
        = "{"
          "  \"latest\": {"
          "    \"version\": \"v1.14.3\","
          "    \"url\": \"https://fwupdate.ruuvi.com/v1.14.3\","
          "    \"created_at\": \"2023-10-06T11:26:07Z\""
          "  }"
          "}";
    this->m_firmware_update_resp             = str_buf_printf_with_alloc("%s", p_firmwareUpdateJson);
    const char* arr_of_binaries[]            = { "https://fwupdate.ruuvi.com/v1.14.3/ruuvi_gateway_esp.bin",
                                                 "https://fwupdate.ruuvi.com/v1.14.3/fatfs_gwui.bin",
                                                 "https://fwupdate.ruuvi.com/v1.14.3/fatfs_nrf52.bin" };
    this->m_http_check_with_auth_arr_of_urls = arr_of_binaries;
    this->m_http_check_with_auth_num_of_urls = sizeof(arr_of_binaries) / sizeof(arr_of_binaries[0]);

    const char* const expected_json
        = "{\"status\": 200, \"json\": {  \"latest\": {    \"version\": \"v1.14.3\",    \"url\": "
          "\"https://fwupdate.ruuvi.com/v1.14.3\",    \"created_at\": \"2023-10-06T11:26:07Z\"  }}}";

    const char* const p_uri_params
        = "validate_type=check_fw_update_url&url=https://network.ruuvi.com/firmwareupdate&auth_type=none";

    http_server_resp_t resp = http_server_cb_on_get("validate_url", p_uri_params, true, nullptr);

    ASSERT_EQ(HTTP_RESP_CODE_200, resp.http_resp_code);
    ASSERT_EQ(HTTP_CONTENT_LOCATION_HEAP, resp.content_location);
    ASSERT_TRUE(resp.flag_no_cache);
    ASSERT_EQ(HTTP_CONTENT_TYPE_APPLICATION_JSON, resp.content_type);
    ASSERT_EQ(nullptr, resp.p_content_type_param);
    ASSERT_NE(nullptr, resp.select_location.heap.p_buf);
    ASSERT_EQ(string(expected_json), string(reinterpret_cast<const char*>(resp.select_location.heap.p_buf)));
    ASSERT_EQ(strlen(expected_json), resp.content_len);
    ASSERT_EQ(HTTP_CONTENT_ENCODING_NONE, resp.content_encoding);
    ASSERT_EQ(
        "D http_server: http_server_cb_on_get /validate_url?validate_type=check_fw_update_url&url=https://network.ruuvi.com/firmwareupdate&auth_type=none\n"
        "I http_server: http_server_cb_on_get: Clear all saved TLS session tickets\n"
        "D http_server: HTTP params: auth_type=none\n"
        "D http_server: HTTP params: key 'auth_type=': value (encoded): none\n"
        "D http_server: HTTP params: key 'auth_type=': value (decoded): none\n"
        "D http_server: HTTP params: url=https://network.ruuvi.com/firmwareupdate\n"
        "D http_server: HTTP params: key 'url=': value (encoded): https://network.ruuvi.com/firmwareupdate\n"
        "D http_server: HTTP params: key 'url=': value (decoded): https://network.ruuvi.com/firmwareupdate\n"
        "D http_server: Can't find key 'user=' in URL params\n"
        "E http_server: HTTP params: Can't find 'user='\n"
        "D http_server: Can't find key 'encrypted_password=' in URL params\n"
        "E http_server: HTTP params: Can't find 'encrypted_password='\n"
        "D http_server: Can't find key 'use_saved_password=' in URL params\n"
        "E http_server: HTTP params: Can't find 'use_saved_password='\n"
        "E http_server: Can't find key: use_saved_password=\n"
        "E http_server: Can't find prefix 'use_ssl_client_cert='\n"
        "E http_server: Can't find prefix 'use_ssl_server_cert='\n"
        "D http_server: Can't find prefix 'use_extra_http_path='\n"
        "D http_server: Can't find prefix 'use_extra_http_query='\n"
        "D http_server: Can't find prefix 'use_extra_http_headers='\n"
        "D http_server: Got URL from params: https://network.ruuvi.com/firmwareupdate\n"
        "I http_server: Found validate_type: check_fw_update_url\n"
        "I http_download: cb_on_http_download_json_data: buf_size=" + std::to_string(strlen(p_firmwareUpdateJson)) + "\n"
        "I http_download: http_download_json: completed within 0 ticks\n"
        "I http_server: Firmware update info (json): " + string(p_firmwareUpdateJson) + "\n"
        "I http_download: http_check: completed within 0 ticks\n"
        "D http_server: Feed watchdog\n"
        "I http_download: http_check: completed within 0 ticks\n"
        "D http_server: Feed watchdog\n"
        "I http_download: http_check: completed within 0 ticks\n"
        "D http_server: Feed watchdog\n",
        esp_log_wrapper_get_logs());
    http_server_resp_free(&resp);
    os_malloc_trace_dump();
    ESP_LOG_WRAPPER_TEST_CHECK_LOG_RECORD("MEM_TRACE", ESP_LOG_INFO, "Num blocks allocated: 0");
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_EQ(0, this->m_alloc_free_call_count);
}

TEST_F(
    TestHttpServerCb,
    http_server_cb_on_get_validate_url_check_fw_update_url__good_json_without_beta__updates_mode_regular__update_not_needed) // NOLINT
{
    {
        gw_cfg_t gw_cfg = { 0 };
        gw_cfg_default_get(&gw_cfg);
        gw_cfg.ruuvi_cfg.auto_update.auto_update_cycle = AUTO_UPDATE_CYCLE_TYPE_REGULAR;
        gw_cfg_update(&gw_cfg);
        esp_log_wrapper_clear();
    }
    const char* const p_firmwareUpdateJson
        = "{"
          "  \"latest\": {"
          "    \"version\": \"v1.3.3\","
          "    \"url\": \"https://fwupdate.ruuvi.com/v1.3.3\","
          "    \"created_at\": \"2023-10-06T11:26:07Z\""
          "  }"
          "}";
    this->m_firmware_update_resp             = str_buf_printf_with_alloc("%s", p_firmwareUpdateJson);
    const char* arr_of_binaries[]            = { "https://fwupdate.ruuvi.com/v1.3.3/ruuvi_gateway_esp.bin",
                                                 "https://fwupdate.ruuvi.com/v1.3.3/fatfs_gwui.bin",
                                                 "https://fwupdate.ruuvi.com/v1.3.3/fatfs_nrf52.bin" };
    this->m_http_check_with_auth_arr_of_urls = arr_of_binaries;
    this->m_http_check_with_auth_num_of_urls = sizeof(arr_of_binaries) / sizeof(arr_of_binaries[0]);

    const char* const expected_json
        = "{\"status\": 200, \"json\": {  \"latest\": {    \"version\": \"v1.3.3\",    \"url\": "
          "\"https://fwupdate.ruuvi.com/v1.3.3\",    \"created_at\": \"2023-10-06T11:26:07Z\"  }}}";

    const char* const p_uri_params
        = "validate_type=check_fw_update_url&url=https://network.ruuvi.com/firmwareupdate&auth_type=none";

    http_server_resp_t resp = http_server_cb_on_get("validate_url", p_uri_params, true, nullptr);

    ASSERT_EQ(HTTP_RESP_CODE_200, resp.http_resp_code);
    ASSERT_EQ(HTTP_CONTENT_LOCATION_HEAP, resp.content_location);
    ASSERT_TRUE(resp.flag_no_cache);
    ASSERT_EQ(HTTP_CONTENT_TYPE_APPLICATION_JSON, resp.content_type);
    ASSERT_EQ(nullptr, resp.p_content_type_param);
    ASSERT_NE(nullptr, resp.select_location.heap.p_buf);
    ASSERT_EQ(string(expected_json), string(reinterpret_cast<const char*>(resp.select_location.heap.p_buf)));
    ASSERT_EQ(strlen(expected_json), resp.content_len);
    ASSERT_EQ(HTTP_CONTENT_ENCODING_NONE, resp.content_encoding);
    ASSERT_EQ(
        "D http_server: http_server_cb_on_get /validate_url?validate_type=check_fw_update_url&url=https://network.ruuvi.com/firmwareupdate&auth_type=none\n"
        "I http_server: http_server_cb_on_get: Clear all saved TLS session tickets\n"
        "D http_server: HTTP params: auth_type=none\n"
        "D http_server: HTTP params: key 'auth_type=': value (encoded): none\n"
        "D http_server: HTTP params: key 'auth_type=': value (decoded): none\n"
        "D http_server: HTTP params: url=https://network.ruuvi.com/firmwareupdate\n"
        "D http_server: HTTP params: key 'url=': value (encoded): https://network.ruuvi.com/firmwareupdate\n"
        "D http_server: HTTP params: key 'url=': value (decoded): https://network.ruuvi.com/firmwareupdate\n"
        "D http_server: Can't find key 'user=' in URL params\n"
        "E http_server: HTTP params: Can't find 'user='\n"
        "D http_server: Can't find key 'encrypted_password=' in URL params\n"
        "E http_server: HTTP params: Can't find 'encrypted_password='\n"
        "D http_server: Can't find key 'use_saved_password=' in URL params\n"
        "E http_server: HTTP params: Can't find 'use_saved_password='\n"
        "E http_server: Can't find key: use_saved_password=\n"
        "E http_server: Can't find prefix 'use_ssl_client_cert='\n"
        "E http_server: Can't find prefix 'use_ssl_server_cert='\n"
        "D http_server: Can't find prefix 'use_extra_http_path='\n"
        "D http_server: Can't find prefix 'use_extra_http_query='\n"
        "D http_server: Can't find prefix 'use_extra_http_headers='\n"
        "D http_server: Got URL from params: https://network.ruuvi.com/firmwareupdate\n"
        "I http_server: Found validate_type: check_fw_update_url\n"
        "I http_download: cb_on_http_download_json_data: buf_size=" + std::to_string(strlen(p_firmwareUpdateJson)) + "\n"
        "I http_download: http_download_json: completed within 0 ticks\n"
        "I http_server: Firmware update info (json): " + string(p_firmwareUpdateJson) + "\n"
        "I http_download: http_check: completed within 0 ticks\n"
        "D http_server: Feed watchdog\n"
        "I http_download: http_check: completed within 0 ticks\n"
        "D http_server: Feed watchdog\n"
        "I http_download: http_check: completed within 0 ticks\n"
        "D http_server: Feed watchdog\n",
        esp_log_wrapper_get_logs());
    http_server_resp_free(&resp);
    os_malloc_trace_dump();
    ESP_LOG_WRAPPER_TEST_CHECK_LOG_RECORD("MEM_TRACE", ESP_LOG_INFO, "Num blocks allocated: 0");
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_EQ(0, this->m_alloc_free_call_count);
}

TEST_F(
    TestHttpServerCb,
    http_server_cb_on_get_validate_url_check_fw_update_url__good_json_without_beta__updates_mode_beta__update_not_needed) // NOLINT
{
    {
        gw_cfg_t gw_cfg = { 0 };
        gw_cfg_default_get(&gw_cfg);
        gw_cfg.ruuvi_cfg.auto_update.auto_update_cycle = AUTO_UPDATE_CYCLE_TYPE_BETA_TESTER;
        gw_cfg_update(&gw_cfg);
        esp_log_wrapper_clear();
    }
    const char* const p_firmwareUpdateJson
        = "{"
          "  \"latest\": {"
          "    \"version\": \"v1.3.3\","
          "    \"url\": \"https://fwupdate.ruuvi.com/v1.3.3\","
          "    \"created_at\": \"2023-10-06T11:26:07Z\""
          "  }"
          "}";
    this->m_firmware_update_resp             = str_buf_printf_with_alloc("%s", p_firmwareUpdateJson);
    const char* arr_of_binaries[]            = { "https://fwupdate.ruuvi.com/v1.3.3/ruuvi_gateway_esp.bin",
                                                 "https://fwupdate.ruuvi.com/v1.3.3/fatfs_gwui.bin",
                                                 "https://fwupdate.ruuvi.com/v1.3.3/fatfs_nrf52.bin" };
    this->m_http_check_with_auth_arr_of_urls = arr_of_binaries;
    this->m_http_check_with_auth_num_of_urls = sizeof(arr_of_binaries) / sizeof(arr_of_binaries[0]);

    const char* const expected_json
        = "{\"status\": 200, \"json\": {  \"latest\": {    \"version\": \"v1.3.3\",    \"url\": "
          "\"https://fwupdate.ruuvi.com/v1.3.3\",    \"created_at\": \"2023-10-06T11:26:07Z\"  }}}";

    const char* const p_uri_params
        = "validate_type=check_fw_update_url&url=https://network.ruuvi.com/firmwareupdate&auth_type=none";

    http_server_resp_t resp = http_server_cb_on_get("validate_url", p_uri_params, true, nullptr);

    ASSERT_EQ(HTTP_RESP_CODE_200, resp.http_resp_code);
    ASSERT_EQ(HTTP_CONTENT_LOCATION_HEAP, resp.content_location);
    ASSERT_TRUE(resp.flag_no_cache);
    ASSERT_EQ(HTTP_CONTENT_TYPE_APPLICATION_JSON, resp.content_type);
    ASSERT_EQ(nullptr, resp.p_content_type_param);
    ASSERT_NE(nullptr, resp.select_location.heap.p_buf);
    ASSERT_EQ(string(expected_json), string(reinterpret_cast<const char*>(resp.select_location.heap.p_buf)));
    ASSERT_EQ(strlen(expected_json), resp.content_len);
    ASSERT_EQ(HTTP_CONTENT_ENCODING_NONE, resp.content_encoding);
    ASSERT_EQ(
        "D http_server: http_server_cb_on_get /validate_url?validate_type=check_fw_update_url&url=https://network.ruuvi.com/firmwareupdate&auth_type=none\n"
        "I http_server: http_server_cb_on_get: Clear all saved TLS session tickets\n"
        "D http_server: HTTP params: auth_type=none\n"
        "D http_server: HTTP params: key 'auth_type=': value (encoded): none\n"
        "D http_server: HTTP params: key 'auth_type=': value (decoded): none\n"
        "D http_server: HTTP params: url=https://network.ruuvi.com/firmwareupdate\n"
        "D http_server: HTTP params: key 'url=': value (encoded): https://network.ruuvi.com/firmwareupdate\n"
        "D http_server: HTTP params: key 'url=': value (decoded): https://network.ruuvi.com/firmwareupdate\n"
        "D http_server: Can't find key 'user=' in URL params\n"
        "E http_server: HTTP params: Can't find 'user='\n"
        "D http_server: Can't find key 'encrypted_password=' in URL params\n"
        "E http_server: HTTP params: Can't find 'encrypted_password='\n"
        "D http_server: Can't find key 'use_saved_password=' in URL params\n"
        "E http_server: HTTP params: Can't find 'use_saved_password='\n"
        "E http_server: Can't find key: use_saved_password=\n"
        "E http_server: Can't find prefix 'use_ssl_client_cert='\n"
        "E http_server: Can't find prefix 'use_ssl_server_cert='\n"
        "D http_server: Can't find prefix 'use_extra_http_path='\n"
        "D http_server: Can't find prefix 'use_extra_http_query='\n"
        "D http_server: Can't find prefix 'use_extra_http_headers='\n"
        "D http_server: Got URL from params: https://network.ruuvi.com/firmwareupdate\n"
        "I http_server: Found validate_type: check_fw_update_url\n"
        "I http_download: cb_on_http_download_json_data: buf_size=" + std::to_string(strlen(p_firmwareUpdateJson)) + "\n"
        "I http_download: http_download_json: completed within 0 ticks\n"
        "I http_server: Firmware update info (json): " + string(p_firmwareUpdateJson) + "\n"
        "I http_download: http_check: completed within 0 ticks\n"
        "D http_server: Feed watchdog\n"
        "I http_download: http_check: completed within 0 ticks\n"
        "D http_server: Feed watchdog\n"
        "I http_download: http_check: completed within 0 ticks\n"
        "D http_server: Feed watchdog\n",
        esp_log_wrapper_get_logs());
    http_server_resp_free(&resp);
    os_malloc_trace_dump();
    ESP_LOG_WRAPPER_TEST_CHECK_LOG_RECORD("MEM_TRACE", ESP_LOG_INFO, "Num blocks allocated: 0");
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_EQ(0, this->m_alloc_free_call_count);
}

TEST_F(
    TestHttpServerCb,
    http_server_cb_on_get_validate_url_check_fw_update_url__good_json_without_beta__updates_mode_manual__update_not_needed) // NOLINT
{
    {
        gw_cfg_t gw_cfg = { 0 };
        gw_cfg_default_get(&gw_cfg);
        gw_cfg.ruuvi_cfg.auto_update.auto_update_cycle = AUTO_UPDATE_CYCLE_TYPE_MANUAL;
        gw_cfg_update(&gw_cfg);
        esp_log_wrapper_clear();
    }
    const char* const p_firmwareUpdateJson
        = "{"
          "  \"latest\": {"
          "    \"version\": \"v1.3.3\","
          "    \"url\": \"https://fwupdate.ruuvi.com/v1.3.3\","
          "    \"created_at\": \"2023-10-06T11:26:07Z\""
          "  }"
          "}";
    this->m_firmware_update_resp             = str_buf_printf_with_alloc("%s", p_firmwareUpdateJson);
    const char* arr_of_binaries[]            = { "https://fwupdate.ruuvi.com/v1.3.3/ruuvi_gateway_esp.bin",
                                                 "https://fwupdate.ruuvi.com/v1.3.3/fatfs_gwui.bin",
                                                 "https://fwupdate.ruuvi.com/v1.3.3/fatfs_nrf52.bin" };
    this->m_http_check_with_auth_arr_of_urls = arr_of_binaries;
    this->m_http_check_with_auth_num_of_urls = sizeof(arr_of_binaries) / sizeof(arr_of_binaries[0]);

    const char* const expected_json
        = "{\"status\": 200, \"json\": {  \"latest\": {    \"version\": \"v1.3.3\",    \"url\": "
          "\"https://fwupdate.ruuvi.com/v1.3.3\",    \"created_at\": \"2023-10-06T11:26:07Z\"  }}}";

    const char* const p_uri_params
        = "validate_type=check_fw_update_url&url=https://network.ruuvi.com/firmwareupdate&auth_type=none";

    http_server_resp_t resp = http_server_cb_on_get("validate_url", p_uri_params, true, nullptr);

    ASSERT_EQ(HTTP_RESP_CODE_200, resp.http_resp_code);
    ASSERT_EQ(HTTP_CONTENT_LOCATION_HEAP, resp.content_location);
    ASSERT_TRUE(resp.flag_no_cache);
    ASSERT_EQ(HTTP_CONTENT_TYPE_APPLICATION_JSON, resp.content_type);
    ASSERT_EQ(nullptr, resp.p_content_type_param);
    ASSERT_NE(nullptr, resp.select_location.heap.p_buf);
    ASSERT_EQ(string(expected_json), string(reinterpret_cast<const char*>(resp.select_location.heap.p_buf)));
    ASSERT_EQ(strlen(expected_json), resp.content_len);
    ASSERT_EQ(HTTP_CONTENT_ENCODING_NONE, resp.content_encoding);
    ASSERT_EQ(
        "D http_server: http_server_cb_on_get /validate_url?validate_type=check_fw_update_url&url=https://network.ruuvi.com/firmwareupdate&auth_type=none\n"
        "I http_server: http_server_cb_on_get: Clear all saved TLS session tickets\n"
        "D http_server: HTTP params: auth_type=none\n"
        "D http_server: HTTP params: key 'auth_type=': value (encoded): none\n"
        "D http_server: HTTP params: key 'auth_type=': value (decoded): none\n"
        "D http_server: HTTP params: url=https://network.ruuvi.com/firmwareupdate\n"
        "D http_server: HTTP params: key 'url=': value (encoded): https://network.ruuvi.com/firmwareupdate\n"
        "D http_server: HTTP params: key 'url=': value (decoded): https://network.ruuvi.com/firmwareupdate\n"
        "D http_server: Can't find key 'user=' in URL params\n"
        "E http_server: HTTP params: Can't find 'user='\n"
        "D http_server: Can't find key 'encrypted_password=' in URL params\n"
        "E http_server: HTTP params: Can't find 'encrypted_password='\n"
        "D http_server: Can't find key 'use_saved_password=' in URL params\n"
        "E http_server: HTTP params: Can't find 'use_saved_password='\n"
        "E http_server: Can't find key: use_saved_password=\n"
        "E http_server: Can't find prefix 'use_ssl_client_cert='\n"
        "E http_server: Can't find prefix 'use_ssl_server_cert='\n"
        "D http_server: Can't find prefix 'use_extra_http_path='\n"
        "D http_server: Can't find prefix 'use_extra_http_query='\n"
        "D http_server: Can't find prefix 'use_extra_http_headers='\n"
        "D http_server: Got URL from params: https://network.ruuvi.com/firmwareupdate\n"
        "I http_server: Found validate_type: check_fw_update_url\n"
        "I http_download: cb_on_http_download_json_data: buf_size=" + std::to_string(strlen(p_firmwareUpdateJson)) + "\n"
        "I http_download: http_download_json: completed within 0 ticks\n"
        "I http_server: Firmware update info (json): " + string(p_firmwareUpdateJson) + "\n"
        "I http_download: http_check: completed within 0 ticks\n"
        "D http_server: Feed watchdog\n"
        "I http_download: http_check: completed within 0 ticks\n"
        "D http_server: Feed watchdog\n"
        "I http_download: http_check: completed within 0 ticks\n"
        "D http_server: Feed watchdog\n",
        esp_log_wrapper_get_logs());
    http_server_resp_free(&resp);
    os_malloc_trace_dump();
    ESP_LOG_WRAPPER_TEST_CHECK_LOG_RECORD("MEM_TRACE", ESP_LOG_INFO, "Num blocks allocated: 0");
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_EQ(0, this->m_alloc_free_call_count);
}

TEST_F(
    TestHttpServerCb,
    http_server_cb_on_get_validate_url_check_fw_update_url__good_json_with_beta__updates_mode_regular__update_needed) // NOLINT
{
    {
        gw_cfg_t gw_cfg = { 0 };
        gw_cfg_default_get(&gw_cfg);
        gw_cfg.ruuvi_cfg.auto_update.auto_update_cycle = AUTO_UPDATE_CYCLE_TYPE_REGULAR;
        gw_cfg_update(&gw_cfg);
        esp_log_wrapper_clear();
    }
    const char* const p_firmwareUpdateJson
        = "{"
          "  \"latest\": {"
          "    \"version\": \"v1.14.3\","
          "    \"url\": \"https://fwupdate.ruuvi.com/v1.14.3\","
          "    \"created_at\": \"2023-10-06T11:26:07Z\""
          "  },"
          "  \"beta\": {"
          "    \"version\": \"v1.14.4\","
          "    \"url\": \"https://github.com/ruuvi/ruuvi.gateway_esp.c/releases/download/v1.14.4\","
          "    \"created_at\": \"2023-09-19T11:16:48Z\""
          "  }"
          "}";
    this->m_firmware_update_resp             = str_buf_printf_with_alloc("%s", p_firmwareUpdateJson);
    const char* arr_of_binaries[]            = { "https://fwupdate.ruuvi.com/v1.14.3/ruuvi_gateway_esp.bin",
                                                 "https://fwupdate.ruuvi.com/v1.14.3/fatfs_gwui.bin",
                                                 "https://fwupdate.ruuvi.com/v1.14.3/fatfs_nrf52.bin" };
    this->m_http_check_with_auth_arr_of_urls = arr_of_binaries;
    this->m_http_check_with_auth_num_of_urls = sizeof(arr_of_binaries) / sizeof(arr_of_binaries[0]);

    const char* const expected_json
        = "{\"status\": 200, \"json\": {  \"latest\": {    \"version\": \"v1.14.3\",    \"url\": "
          "\"https://fwupdate.ruuvi.com/v1.14.3\",    \"created_at\": \"2023-10-06T11:26:07Z\"  },  \"beta\": {    "
          "\"version\": \"v1.14.4\",    \"url\": "
          "\"https://github.com/ruuvi/ruuvi.gateway_esp.c/releases/download/v1.14.4\",    \"created_at\": "
          "\"2023-09-19T11:16:48Z\"  }}}";

    const char* const p_uri_params
        = "validate_type=check_fw_update_url&url=https://network.ruuvi.com/firmwareupdate&auth_type=none";

    http_server_resp_t resp = http_server_cb_on_get("validate_url", p_uri_params, true, nullptr);

    ASSERT_EQ(HTTP_RESP_CODE_200, resp.http_resp_code);
    ASSERT_EQ(HTTP_CONTENT_LOCATION_HEAP, resp.content_location);
    ASSERT_TRUE(resp.flag_no_cache);
    ASSERT_EQ(HTTP_CONTENT_TYPE_APPLICATION_JSON, resp.content_type);
    ASSERT_EQ(nullptr, resp.p_content_type_param);
    ASSERT_NE(nullptr, resp.select_location.heap.p_buf);
    ASSERT_EQ(string(expected_json), string(reinterpret_cast<const char*>(resp.select_location.heap.p_buf)));
    ASSERT_EQ(strlen(expected_json), resp.content_len);
    ASSERT_EQ(HTTP_CONTENT_ENCODING_NONE, resp.content_encoding);
    ASSERT_EQ(
        "D http_server: http_server_cb_on_get /validate_url?validate_type=check_fw_update_url&url=https://network.ruuvi.com/firmwareupdate&auth_type=none\n"
        "I http_server: http_server_cb_on_get: Clear all saved TLS session tickets\n"
        "D http_server: HTTP params: auth_type=none\n"
        "D http_server: HTTP params: key 'auth_type=': value (encoded): none\n"
        "D http_server: HTTP params: key 'auth_type=': value (decoded): none\n"
        "D http_server: HTTP params: url=https://network.ruuvi.com/firmwareupdate\n"
        "D http_server: HTTP params: key 'url=': value (encoded): https://network.ruuvi.com/firmwareupdate\n"
        "D http_server: HTTP params: key 'url=': value (decoded): https://network.ruuvi.com/firmwareupdate\n"
        "D http_server: Can't find key 'user=' in URL params\n"
        "E http_server: HTTP params: Can't find 'user='\n"
        "D http_server: Can't find key 'encrypted_password=' in URL params\n"
        "E http_server: HTTP params: Can't find 'encrypted_password='\n"
        "D http_server: Can't find key 'use_saved_password=' in URL params\n"
        "E http_server: HTTP params: Can't find 'use_saved_password='\n"
        "E http_server: Can't find key: use_saved_password=\n"
        "E http_server: Can't find prefix 'use_ssl_client_cert='\n"
        "E http_server: Can't find prefix 'use_ssl_server_cert='\n"
        "D http_server: Can't find prefix 'use_extra_http_path='\n"
        "D http_server: Can't find prefix 'use_extra_http_query='\n"
        "D http_server: Can't find prefix 'use_extra_http_headers='\n"
        "D http_server: Got URL from params: https://network.ruuvi.com/firmwareupdate\n"
        "I http_server: Found validate_type: check_fw_update_url\n"
        "I http_download: cb_on_http_download_json_data: buf_size=" + std::to_string(strlen(p_firmwareUpdateJson)) + "\n"
        "I http_download: http_download_json: completed within 0 ticks\n"
        "I http_server: Firmware update info (json): " + string(p_firmwareUpdateJson) + "\n"
        "I http_download: http_check: completed within 0 ticks\n"
        "D http_server: Feed watchdog\n"
        "I http_download: http_check: completed within 0 ticks\n"
        "D http_server: Feed watchdog\n"
        "I http_download: http_check: completed within 0 ticks\n"
        "D http_server: Feed watchdog\n",
        esp_log_wrapper_get_logs());
    http_server_resp_free(&resp);
    os_malloc_trace_dump();
    ESP_LOG_WRAPPER_TEST_CHECK_LOG_RECORD("MEM_TRACE", ESP_LOG_INFO, "Num blocks allocated: 0");
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_EQ(0, this->m_alloc_free_call_count);
}

TEST_F(
    TestHttpServerCb,
    http_server_cb_on_get_validate_url_check_fw_update_url__good_json_with_beta__updates_mode_beta__update_needed) // NOLINT
{
    {
        gw_cfg_t gw_cfg = { 0 };
        gw_cfg_default_get(&gw_cfg);
        gw_cfg.ruuvi_cfg.auto_update.auto_update_cycle = AUTO_UPDATE_CYCLE_TYPE_BETA_TESTER;
        gw_cfg_update(&gw_cfg);
        esp_log_wrapper_clear();
    }
    const char* const p_firmwareUpdateJson
        = "{"
          "  \"latest\": {"
          "    \"version\": \"v1.14.3\","
          "    \"url\": \"https://fwupdate.ruuvi.com/v1.14.3\","
          "    \"created_at\": \"2023-10-06T11:26:07Z\""
          "  },"
          "  \"beta\": {"
          "    \"version\": \"v1.14.4\","
          "    \"url\": \"https://github.com/ruuvi/ruuvi.gateway_esp.c/releases/download/v1.14.4\","
          "    \"created_at\": \"2023-09-19T11:16:48Z\""
          "  }"
          "}";
    this->m_firmware_update_resp  = str_buf_printf_with_alloc("%s", p_firmwareUpdateJson);
    const char* arr_of_binaries[] = {
        "https://github.com/ruuvi/ruuvi.gateway_esp.c/releases/download/v1.14.4/ruuvi_gateway_esp.bin",
        "https://github.com/ruuvi/ruuvi.gateway_esp.c/releases/download/v1.14.4/fatfs_gwui.bin",
        "https://github.com/ruuvi/ruuvi.gateway_esp.c/releases/download/v1.14.4/fatfs_nrf52.bin"
    };
    this->m_http_check_with_auth_arr_of_urls = arr_of_binaries;
    this->m_http_check_with_auth_num_of_urls = sizeof(arr_of_binaries) / sizeof(arr_of_binaries[0]);

    const char* const expected_json
        = "{\"status\": 200, \"json\": {  \"latest\": {    \"version\": \"v1.14.3\",    \"url\": "
          "\"https://fwupdate.ruuvi.com/v1.14.3\",    \"created_at\": \"2023-10-06T11:26:07Z\"  },  \"beta\": {    "
          "\"version\": \"v1.14.4\",    \"url\": "
          "\"https://github.com/ruuvi/ruuvi.gateway_esp.c/releases/download/v1.14.4\",    \"created_at\": "
          "\"2023-09-19T11:16:48Z\"  }}}";

    const char* const p_uri_params
        = "validate_type=check_fw_update_url&url=https://network.ruuvi.com/firmwareupdate&auth_type=none";

    http_server_resp_t resp = http_server_cb_on_get("validate_url", p_uri_params, true, nullptr);

    ASSERT_EQ(HTTP_RESP_CODE_200, resp.http_resp_code);
    ASSERT_EQ(HTTP_CONTENT_LOCATION_HEAP, resp.content_location);
    ASSERT_TRUE(resp.flag_no_cache);
    ASSERT_EQ(HTTP_CONTENT_TYPE_APPLICATION_JSON, resp.content_type);
    ASSERT_EQ(nullptr, resp.p_content_type_param);
    ASSERT_NE(nullptr, resp.select_location.heap.p_buf);
    ASSERT_EQ(string(expected_json), string(reinterpret_cast<const char*>(resp.select_location.heap.p_buf)));
    ASSERT_EQ(strlen(expected_json), resp.content_len);
    ASSERT_EQ(HTTP_CONTENT_ENCODING_NONE, resp.content_encoding);
    ASSERT_EQ(
        "D http_server: http_server_cb_on_get /validate_url?validate_type=check_fw_update_url&url=https://network.ruuvi.com/firmwareupdate&auth_type=none\n"
        "I http_server: http_server_cb_on_get: Clear all saved TLS session tickets\n"
        "D http_server: HTTP params: auth_type=none\n"
        "D http_server: HTTP params: key 'auth_type=': value (encoded): none\n"
        "D http_server: HTTP params: key 'auth_type=': value (decoded): none\n"
        "D http_server: HTTP params: url=https://network.ruuvi.com/firmwareupdate\n"
        "D http_server: HTTP params: key 'url=': value (encoded): https://network.ruuvi.com/firmwareupdate\n"
        "D http_server: HTTP params: key 'url=': value (decoded): https://network.ruuvi.com/firmwareupdate\n"
        "D http_server: Can't find key 'user=' in URL params\n"
        "E http_server: HTTP params: Can't find 'user='\n"
        "D http_server: Can't find key 'encrypted_password=' in URL params\n"
        "E http_server: HTTP params: Can't find 'encrypted_password='\n"
        "D http_server: Can't find key 'use_saved_password=' in URL params\n"
        "E http_server: HTTP params: Can't find 'use_saved_password='\n"
        "E http_server: Can't find key: use_saved_password=\n"
        "E http_server: Can't find prefix 'use_ssl_client_cert='\n"
        "E http_server: Can't find prefix 'use_ssl_server_cert='\n"
        "D http_server: Can't find prefix 'use_extra_http_path='\n"
        "D http_server: Can't find prefix 'use_extra_http_query='\n"
        "D http_server: Can't find prefix 'use_extra_http_headers='\n"
        "D http_server: Got URL from params: https://network.ruuvi.com/firmwareupdate\n"
        "I http_server: Found validate_type: check_fw_update_url\n"
        "I http_download: cb_on_http_download_json_data: buf_size=" + std::to_string(strlen(p_firmwareUpdateJson)) + "\n"
        "I http_download: http_download_json: completed within 0 ticks\n"
        "I http_server: Firmware update info (json): " + string(p_firmwareUpdateJson) + "\n"
        "I http_download: http_check: completed within 0 ticks\n"
        "D http_server: Feed watchdog\n"
        "I http_download: http_check: completed within 0 ticks\n"
        "D http_server: Feed watchdog\n"
        "I http_download: http_check: completed within 0 ticks\n"
        "D http_server: Feed watchdog\n",
        esp_log_wrapper_get_logs());
    http_server_resp_free(&resp);
    os_malloc_trace_dump();
    ESP_LOG_WRAPPER_TEST_CHECK_LOG_RECORD("MEM_TRACE", ESP_LOG_INFO, "Num blocks allocated: 0");
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_EQ(0, this->m_alloc_free_call_count);
}

TEST_F(
    TestHttpServerCb,
    http_server_cb_on_get_validate_url_check_fw_update_url__good_json_with_beta__updates_mode_manual__update_needed) // NOLINT
{
    {
        gw_cfg_t gw_cfg = { 0 };
        gw_cfg_default_get(&gw_cfg);
        gw_cfg.ruuvi_cfg.auto_update.auto_update_cycle = AUTO_UPDATE_CYCLE_TYPE_MANUAL;
        gw_cfg_update(&gw_cfg);
        esp_log_wrapper_clear();
    }
    const char* const p_firmwareUpdateJson
        = "{"
          "  \"latest\": {"
          "    \"version\": \"v1.14.3\","
          "    \"url\": \"https://fwupdate.ruuvi.com/v1.14.3\","
          "    \"created_at\": \"2023-10-06T11:26:07Z\""
          "  },"
          "  \"beta\": {"
          "    \"version\": \"v1.14.4\","
          "    \"url\": \"https://github.com/ruuvi/ruuvi.gateway_esp.c/releases/download/v1.14.4\","
          "    \"created_at\": \"2023-09-19T11:16:48Z\""
          "  }"
          "}";
    this->m_firmware_update_resp             = str_buf_printf_with_alloc("%s", p_firmwareUpdateJson);
    const char* arr_of_binaries[]            = { "https://fwupdate.ruuvi.com/v1.14.3/ruuvi_gateway_esp.bin",
                                                 "https://fwupdate.ruuvi.com/v1.14.3/fatfs_gwui.bin",
                                                 "https://fwupdate.ruuvi.com/v1.14.3/fatfs_nrf52.bin" };
    this->m_http_check_with_auth_arr_of_urls = arr_of_binaries;
    this->m_http_check_with_auth_num_of_urls = sizeof(arr_of_binaries) / sizeof(arr_of_binaries[0]);

    const char* const expected_json
        = "{\"status\": 200, \"json\": {  \"latest\": {    \"version\": \"v1.14.3\",    \"url\": "
          "\"https://fwupdate.ruuvi.com/v1.14.3\",    \"created_at\": \"2023-10-06T11:26:07Z\"  },  \"beta\": {    "
          "\"version\": \"v1.14.4\",    \"url\": "
          "\"https://github.com/ruuvi/ruuvi.gateway_esp.c/releases/download/v1.14.4\",    \"created_at\": "
          "\"2023-09-19T11:16:48Z\"  }}}";

    const char* const p_uri_params
        = "validate_type=check_fw_update_url&url=https://network.ruuvi.com/firmwareupdate&auth_type=none";

    http_server_resp_t resp = http_server_cb_on_get("validate_url", p_uri_params, true, nullptr);

    ASSERT_EQ(HTTP_RESP_CODE_200, resp.http_resp_code);
    ASSERT_EQ(HTTP_CONTENT_LOCATION_HEAP, resp.content_location);
    ASSERT_TRUE(resp.flag_no_cache);
    ASSERT_EQ(HTTP_CONTENT_TYPE_APPLICATION_JSON, resp.content_type);
    ASSERT_EQ(nullptr, resp.p_content_type_param);
    ASSERT_NE(nullptr, resp.select_location.heap.p_buf);
    ASSERT_EQ(string(expected_json), string(reinterpret_cast<const char*>(resp.select_location.heap.p_buf)));
    ASSERT_EQ(strlen(expected_json), resp.content_len);
    ASSERT_EQ(HTTP_CONTENT_ENCODING_NONE, resp.content_encoding);
    ASSERT_EQ(
        "D http_server: http_server_cb_on_get /validate_url?validate_type=check_fw_update_url&url=https://network.ruuvi.com/firmwareupdate&auth_type=none\n"
        "I http_server: http_server_cb_on_get: Clear all saved TLS session tickets\n"
        "D http_server: HTTP params: auth_type=none\n"
        "D http_server: HTTP params: key 'auth_type=': value (encoded): none\n"
        "D http_server: HTTP params: key 'auth_type=': value (decoded): none\n"
        "D http_server: HTTP params: url=https://network.ruuvi.com/firmwareupdate\n"
        "D http_server: HTTP params: key 'url=': value (encoded): https://network.ruuvi.com/firmwareupdate\n"
        "D http_server: HTTP params: key 'url=': value (decoded): https://network.ruuvi.com/firmwareupdate\n"
        "D http_server: Can't find key 'user=' in URL params\n"
        "E http_server: HTTP params: Can't find 'user='\n"
        "D http_server: Can't find key 'encrypted_password=' in URL params\n"
        "E http_server: HTTP params: Can't find 'encrypted_password='\n"
        "D http_server: Can't find key 'use_saved_password=' in URL params\n"
        "E http_server: HTTP params: Can't find 'use_saved_password='\n"
        "E http_server: Can't find key: use_saved_password=\n"
        "E http_server: Can't find prefix 'use_ssl_client_cert='\n"
        "E http_server: Can't find prefix 'use_ssl_server_cert='\n"
        "D http_server: Can't find prefix 'use_extra_http_path='\n"
        "D http_server: Can't find prefix 'use_extra_http_query='\n"
        "D http_server: Can't find prefix 'use_extra_http_headers='\n"
        "D http_server: Got URL from params: https://network.ruuvi.com/firmwareupdate\n"
        "I http_server: Found validate_type: check_fw_update_url\n"
        "I http_download: cb_on_http_download_json_data: buf_size=" + std::to_string(strlen(p_firmwareUpdateJson)) + "\n"
        "I http_download: http_download_json: completed within 0 ticks\n"
        "I http_server: Firmware update info (json): " + string(p_firmwareUpdateJson) + "\n"
        "I http_download: http_check: completed within 0 ticks\n"
        "D http_server: Feed watchdog\n"
        "I http_download: http_check: completed within 0 ticks\n"
        "D http_server: Feed watchdog\n"
        "I http_download: http_check: completed within 0 ticks\n"
        "D http_server: Feed watchdog\n",
        esp_log_wrapper_get_logs());
    http_server_resp_free(&resp);
    os_malloc_trace_dump();
    ESP_LOG_WRAPPER_TEST_CHECK_LOG_RECORD("MEM_TRACE", ESP_LOG_INFO, "Num blocks allocated: 0");
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_EQ(0, this->m_alloc_free_call_count);
}

TEST_F(
    TestHttpServerCb,
    http_server_cb_on_get_validate_url_check_fw_update_url__good_json_with_beta__updates_mode_regular__update_not_needed) // NOLINT
{
    {
        gw_cfg_t gw_cfg = { 0 };
        gw_cfg_default_get(&gw_cfg);
        gw_cfg.ruuvi_cfg.auto_update.auto_update_cycle = AUTO_UPDATE_CYCLE_TYPE_REGULAR;
        gw_cfg_update(&gw_cfg);
        esp_log_wrapper_clear();
    }
    const char* const p_firmwareUpdateJson
        = "{"
          "  \"latest\": {"
          "    \"version\": \"v1.3.3\","
          "    \"url\": \"https://fwupdate.ruuvi.com/v1.3.3\","
          "    \"created_at\": \"2023-10-06T11:26:07Z\""
          "  },"
          "  \"beta\": {"
          "    \"version\": \"v1.3.4\","
          "    \"url\": \"https://github.com/ruuvi/ruuvi.gateway_esp.c/releases/download/v1.3.4\","
          "    \"created_at\": \"2023-09-19T11:16:48Z\""
          "  }"
          "}";
    this->m_firmware_update_resp             = str_buf_printf_with_alloc("%s", p_firmwareUpdateJson);
    const char* arr_of_binaries[]            = { "https://fwupdate.ruuvi.com/v1.3.3/ruuvi_gateway_esp.bin",
                                                 "https://fwupdate.ruuvi.com/v1.3.3/fatfs_gwui.bin",
                                                 "https://fwupdate.ruuvi.com/v1.3.3/fatfs_nrf52.bin" };
    this->m_http_check_with_auth_arr_of_urls = arr_of_binaries;
    this->m_http_check_with_auth_num_of_urls = sizeof(arr_of_binaries) / sizeof(arr_of_binaries[0]);

    const char* const expected_json
        = "{\"status\": 200, \"json\": {  \"latest\": {    \"version\": \"v1.3.3\",    \"url\": "
          "\"https://fwupdate.ruuvi.com/v1.3.3\",    \"created_at\": \"2023-10-06T11:26:07Z\"  },  \"beta\": {    "
          "\"version\": \"v1.3.4\",    \"url\": "
          "\"https://github.com/ruuvi/ruuvi.gateway_esp.c/releases/download/v1.3.4\",    \"created_at\": "
          "\"2023-09-19T11:16:48Z\"  }}}";

    const char* const p_uri_params
        = "validate_type=check_fw_update_url&url=https://network.ruuvi.com/firmwareupdate&auth_type=none";

    http_server_resp_t resp = http_server_cb_on_get("validate_url", p_uri_params, true, nullptr);

    ASSERT_EQ(HTTP_RESP_CODE_200, resp.http_resp_code);
    ASSERT_EQ(HTTP_CONTENT_LOCATION_HEAP, resp.content_location);
    ASSERT_TRUE(resp.flag_no_cache);
    ASSERT_EQ(HTTP_CONTENT_TYPE_APPLICATION_JSON, resp.content_type);
    ASSERT_EQ(nullptr, resp.p_content_type_param);
    ASSERT_NE(nullptr, resp.select_location.heap.p_buf);
    ASSERT_EQ(string(expected_json), string(reinterpret_cast<const char*>(resp.select_location.heap.p_buf)));
    ASSERT_EQ(strlen(expected_json), resp.content_len);
    ASSERT_EQ(HTTP_CONTENT_ENCODING_NONE, resp.content_encoding);
    ASSERT_EQ(
        "D http_server: http_server_cb_on_get /validate_url?validate_type=check_fw_update_url&url=https://network.ruuvi.com/firmwareupdate&auth_type=none\n"
        "I http_server: http_server_cb_on_get: Clear all saved TLS session tickets\n"
        "D http_server: HTTP params: auth_type=none\n"
        "D http_server: HTTP params: key 'auth_type=': value (encoded): none\n"
        "D http_server: HTTP params: key 'auth_type=': value (decoded): none\n"
        "D http_server: HTTP params: url=https://network.ruuvi.com/firmwareupdate\n"
        "D http_server: HTTP params: key 'url=': value (encoded): https://network.ruuvi.com/firmwareupdate\n"
        "D http_server: HTTP params: key 'url=': value (decoded): https://network.ruuvi.com/firmwareupdate\n"
        "D http_server: Can't find key 'user=' in URL params\n"
        "E http_server: HTTP params: Can't find 'user='\n"
        "D http_server: Can't find key 'encrypted_password=' in URL params\n"
        "E http_server: HTTP params: Can't find 'encrypted_password='\n"
        "D http_server: Can't find key 'use_saved_password=' in URL params\n"
        "E http_server: HTTP params: Can't find 'use_saved_password='\n"
        "E http_server: Can't find key: use_saved_password=\n"
        "E http_server: Can't find prefix 'use_ssl_client_cert='\n"
        "E http_server: Can't find prefix 'use_ssl_server_cert='\n"
        "D http_server: Can't find prefix 'use_extra_http_path='\n"
        "D http_server: Can't find prefix 'use_extra_http_query='\n"
        "D http_server: Can't find prefix 'use_extra_http_headers='\n"
        "D http_server: Got URL from params: https://network.ruuvi.com/firmwareupdate\n"
        "I http_server: Found validate_type: check_fw_update_url\n"
        "I http_download: cb_on_http_download_json_data: buf_size=" + std::to_string(strlen(p_firmwareUpdateJson)) + "\n"
        "I http_download: http_download_json: completed within 0 ticks\n"
        "I http_server: Firmware update info (json): " + string(p_firmwareUpdateJson) + "\n"
        "I http_download: http_check: completed within 0 ticks\n"
        "D http_server: Feed watchdog\n"
        "I http_download: http_check: completed within 0 ticks\n"
        "D http_server: Feed watchdog\n"
        "I http_download: http_check: completed within 0 ticks\n"
        "D http_server: Feed watchdog\n",
        esp_log_wrapper_get_logs());
    http_server_resp_free(&resp);
    os_malloc_trace_dump();
    ESP_LOG_WRAPPER_TEST_CHECK_LOG_RECORD("MEM_TRACE", ESP_LOG_INFO, "Num blocks allocated: 0");
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_EQ(0, this->m_alloc_free_call_count);
}

TEST_F(
    TestHttpServerCb,
    http_server_cb_on_get_validate_url_check_fw_update_url__good_json_with_beta__updates_mode_beta__update_not_needed) // NOLINT
{
    {
        gw_cfg_t gw_cfg = { 0 };
        gw_cfg_default_get(&gw_cfg);
        gw_cfg.ruuvi_cfg.auto_update.auto_update_cycle = AUTO_UPDATE_CYCLE_TYPE_BETA_TESTER;
        gw_cfg_update(&gw_cfg);
        esp_log_wrapper_clear();
    }
    const char* const p_firmwareUpdateJson
        = "{"
          "  \"latest\": {"
          "    \"version\": \"v1.3.2\","
          "    \"url\": \"https://fwupdate.ruuvi.com/v1.3.2\","
          "    \"created_at\": \"2023-10-06T11:26:07Z\""
          "  },"
          "  \"beta\": {"
          "    \"version\": \"v1.3.3\","
          "    \"url\": \"https://github.com/ruuvi/ruuvi.gateway_esp.c/releases/download/v1.3.3\","
          "    \"created_at\": \"2023-09-19T11:16:48Z\""
          "  }"
          "}";
    this->m_firmware_update_resp  = str_buf_printf_with_alloc("%s", p_firmwareUpdateJson);
    const char* arr_of_binaries[] = {
        "https://github.com/ruuvi/ruuvi.gateway_esp.c/releases/download/v1.3.3/ruuvi_gateway_esp.bin",
        "https://github.com/ruuvi/ruuvi.gateway_esp.c/releases/download/v1.3.3/fatfs_gwui.bin",
        "https://github.com/ruuvi/ruuvi.gateway_esp.c/releases/download/v1.3.3/fatfs_nrf52.bin"
    };
    this->m_http_check_with_auth_arr_of_urls = arr_of_binaries;
    this->m_http_check_with_auth_num_of_urls = sizeof(arr_of_binaries) / sizeof(arr_of_binaries[0]);

    const char* const expected_json
        = "{\"status\": 200, \"json\": {  \"latest\": {    \"version\": \"v1.3.2\",    \"url\": "
          "\"https://fwupdate.ruuvi.com/v1.3.2\",    \"created_at\": \"2023-10-06T11:26:07Z\"  },  \"beta\": {    "
          "\"version\": \"v1.3.3\",    \"url\": "
          "\"https://github.com/ruuvi/ruuvi.gateway_esp.c/releases/download/v1.3.3\",    \"created_at\": "
          "\"2023-09-19T11:16:48Z\"  }}}";

    const char* const p_uri_params
        = "validate_type=check_fw_update_url&url=https://network.ruuvi.com/firmwareupdate&auth_type=none";

    http_server_resp_t resp = http_server_cb_on_get("validate_url", p_uri_params, true, nullptr);

    ASSERT_EQ(HTTP_RESP_CODE_200, resp.http_resp_code);
    ASSERT_EQ(HTTP_CONTENT_LOCATION_HEAP, resp.content_location);
    ASSERT_TRUE(resp.flag_no_cache);
    ASSERT_EQ(HTTP_CONTENT_TYPE_APPLICATION_JSON, resp.content_type);
    ASSERT_EQ(nullptr, resp.p_content_type_param);
    ASSERT_NE(nullptr, resp.select_location.heap.p_buf);
    ASSERT_EQ(string(expected_json), string(reinterpret_cast<const char*>(resp.select_location.heap.p_buf)));
    ASSERT_EQ(strlen(expected_json), resp.content_len);
    ASSERT_EQ(HTTP_CONTENT_ENCODING_NONE, resp.content_encoding);
    ASSERT_EQ(
        "D http_server: http_server_cb_on_get /validate_url?validate_type=check_fw_update_url&url=https://network.ruuvi.com/firmwareupdate&auth_type=none\n"
        "I http_server: http_server_cb_on_get: Clear all saved TLS session tickets\n"
        "D http_server: HTTP params: auth_type=none\n"
        "D http_server: HTTP params: key 'auth_type=': value (encoded): none\n"
        "D http_server: HTTP params: key 'auth_type=': value (decoded): none\n"
        "D http_server: HTTP params: url=https://network.ruuvi.com/firmwareupdate\n"
        "D http_server: HTTP params: key 'url=': value (encoded): https://network.ruuvi.com/firmwareupdate\n"
        "D http_server: HTTP params: key 'url=': value (decoded): https://network.ruuvi.com/firmwareupdate\n"
        "D http_server: Can't find key 'user=' in URL params\n"
        "E http_server: HTTP params: Can't find 'user='\n"
        "D http_server: Can't find key 'encrypted_password=' in URL params\n"
        "E http_server: HTTP params: Can't find 'encrypted_password='\n"
        "D http_server: Can't find key 'use_saved_password=' in URL params\n"
        "E http_server: HTTP params: Can't find 'use_saved_password='\n"
        "E http_server: Can't find key: use_saved_password=\n"
        "E http_server: Can't find prefix 'use_ssl_client_cert='\n"
        "E http_server: Can't find prefix 'use_ssl_server_cert='\n"
        "D http_server: Can't find prefix 'use_extra_http_path='\n"
        "D http_server: Can't find prefix 'use_extra_http_query='\n"
        "D http_server: Can't find prefix 'use_extra_http_headers='\n"
        "D http_server: Got URL from params: https://network.ruuvi.com/firmwareupdate\n"
        "I http_server: Found validate_type: check_fw_update_url\n"
        "I http_download: cb_on_http_download_json_data: buf_size=" + std::to_string(strlen(p_firmwareUpdateJson)) + "\n"
        "I http_download: http_download_json: completed within 0 ticks\n"
        "I http_server: Firmware update info (json): " + string(p_firmwareUpdateJson) + "\n"
        "I http_download: http_check: completed within 0 ticks\n"
        "D http_server: Feed watchdog\n"
        "I http_download: http_check: completed within 0 ticks\n"
        "D http_server: Feed watchdog\n"
        "I http_download: http_check: completed within 0 ticks\n"
        "D http_server: Feed watchdog\n",
        esp_log_wrapper_get_logs());
    http_server_resp_free(&resp);
    os_malloc_trace_dump();
    ESP_LOG_WRAPPER_TEST_CHECK_LOG_RECORD("MEM_TRACE", ESP_LOG_INFO, "Num blocks allocated: 0");
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_EQ(0, this->m_alloc_free_call_count);
}

TEST_F(
    TestHttpServerCb,
    http_server_cb_on_get_validate_url_check_fw_update_url__good_json_with_beta__updates_mode_manual__update_not_needed) // NOLINT
{
    {
        gw_cfg_t gw_cfg = { 0 };
        gw_cfg_default_get(&gw_cfg);
        gw_cfg.ruuvi_cfg.auto_update.auto_update_cycle = AUTO_UPDATE_CYCLE_TYPE_MANUAL;
        gw_cfg_update(&gw_cfg);
        esp_log_wrapper_clear();
    }
    const char* const p_firmwareUpdateJson
        = "{"
          "  \"latest\": {"
          "    \"version\": \"v1.3.3\","
          "    \"url\": \"https://fwupdate.ruuvi.com/v1.3.3\","
          "    \"created_at\": \"2023-10-06T11:26:07Z\""
          "  },"
          "  \"beta\": {"
          "    \"version\": \"v1.3.4\","
          "    \"url\": \"https://github.com/ruuvi/ruuvi.gateway_esp.c/releases/download/v1.3.4\","
          "    \"created_at\": \"2023-09-19T11:16:48Z\""
          "  }"
          "}";
    this->m_firmware_update_resp             = str_buf_printf_with_alloc("%s", p_firmwareUpdateJson);
    const char* arr_of_binaries[]            = { "https://fwupdate.ruuvi.com/v1.3.3/ruuvi_gateway_esp.bin",
                                                 "https://fwupdate.ruuvi.com/v1.3.3/fatfs_gwui.bin",
                                                 "https://fwupdate.ruuvi.com/v1.3.3/fatfs_nrf52.bin" };
    this->m_http_check_with_auth_arr_of_urls = arr_of_binaries;
    this->m_http_check_with_auth_num_of_urls = sizeof(arr_of_binaries) / sizeof(arr_of_binaries[0]);

    const char* const expected_json
        = "{\"status\": 200, \"json\": {  \"latest\": {    \"version\": \"v1.3.3\",    \"url\": "
          "\"https://fwupdate.ruuvi.com/v1.3.3\",    \"created_at\": \"2023-10-06T11:26:07Z\"  },  \"beta\": {    "
          "\"version\": \"v1.3.4\",    \"url\": "
          "\"https://github.com/ruuvi/ruuvi.gateway_esp.c/releases/download/v1.3.4\",    \"created_at\": "
          "\"2023-09-19T11:16:48Z\"  }}}";

    const char* const p_uri_params
        = "validate_type=check_fw_update_url&url=https://network.ruuvi.com/firmwareupdate&auth_type=none";

    http_server_resp_t resp = http_server_cb_on_get("validate_url", p_uri_params, true, nullptr);

    ASSERT_EQ(HTTP_RESP_CODE_200, resp.http_resp_code);
    ASSERT_EQ(HTTP_CONTENT_LOCATION_HEAP, resp.content_location);
    ASSERT_TRUE(resp.flag_no_cache);
    ASSERT_EQ(HTTP_CONTENT_TYPE_APPLICATION_JSON, resp.content_type);
    ASSERT_EQ(nullptr, resp.p_content_type_param);
    ASSERT_NE(nullptr, resp.select_location.heap.p_buf);
    ASSERT_EQ(string(expected_json), string(reinterpret_cast<const char*>(resp.select_location.heap.p_buf)));
    ASSERT_EQ(strlen(expected_json), resp.content_len);
    ASSERT_EQ(HTTP_CONTENT_ENCODING_NONE, resp.content_encoding);
    ASSERT_EQ(
        "D http_server: http_server_cb_on_get /validate_url?validate_type=check_fw_update_url&url=https://network.ruuvi.com/firmwareupdate&auth_type=none\n"
        "I http_server: http_server_cb_on_get: Clear all saved TLS session tickets\n"
        "D http_server: HTTP params: auth_type=none\n"
        "D http_server: HTTP params: key 'auth_type=': value (encoded): none\n"
        "D http_server: HTTP params: key 'auth_type=': value (decoded): none\n"
        "D http_server: HTTP params: url=https://network.ruuvi.com/firmwareupdate\n"
        "D http_server: HTTP params: key 'url=': value (encoded): https://network.ruuvi.com/firmwareupdate\n"
        "D http_server: HTTP params: key 'url=': value (decoded): https://network.ruuvi.com/firmwareupdate\n"
        "D http_server: Can't find key 'user=' in URL params\n"
        "E http_server: HTTP params: Can't find 'user='\n"
        "D http_server: Can't find key 'encrypted_password=' in URL params\n"
        "E http_server: HTTP params: Can't find 'encrypted_password='\n"
        "D http_server: Can't find key 'use_saved_password=' in URL params\n"
        "E http_server: HTTP params: Can't find 'use_saved_password='\n"
        "E http_server: Can't find key: use_saved_password=\n"
        "E http_server: Can't find prefix 'use_ssl_client_cert='\n"
        "E http_server: Can't find prefix 'use_ssl_server_cert='\n"
        "D http_server: Can't find prefix 'use_extra_http_path='\n"
        "D http_server: Can't find prefix 'use_extra_http_query='\n"
        "D http_server: Can't find prefix 'use_extra_http_headers='\n"
        "D http_server: Got URL from params: https://network.ruuvi.com/firmwareupdate\n"
        "I http_server: Found validate_type: check_fw_update_url\n"
        "I http_download: cb_on_http_download_json_data: buf_size=" + std::to_string(strlen(p_firmwareUpdateJson)) + "\n"
        "I http_download: http_download_json: completed within 0 ticks\n"
        "I http_server: Firmware update info (json): " + string(p_firmwareUpdateJson) + "\n"
        "I http_download: http_check: completed within 0 ticks\n"
        "D http_server: Feed watchdog\n"
        "I http_download: http_check: completed within 0 ticks\n"
        "D http_server: Feed watchdog\n"
        "I http_download: http_check: completed within 0 ticks\n"
        "D http_server: Feed watchdog\n",
        esp_log_wrapper_get_logs());
    http_server_resp_free(&resp);
    os_malloc_trace_dump();
    ESP_LOG_WRAPPER_TEST_CHECK_LOG_RECORD("MEM_TRACE", ESP_LOG_INFO, "Num blocks allocated: 0");
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_EQ(0, this->m_alloc_free_call_count);
}

TEST_F(TestHttpServerCb, http_server_cb_on_get_validate_url_check_fw_update_url__good_json_missing_files) // NOLINT
{
    const char* const p_firmwareUpdateJson
        = "{"
          "  \"latest\": {"
          "    \"version\": \"v1.14.3\","
          "    \"url\": \"https://fwupdate.ruuvi.com/v1.14.3\","
          "    \"created_at\": \"2023-10-06T11:26:07Z\""
          "  }"
          "}";
    this->m_firmware_update_resp             = str_buf_printf_with_alloc("%s", p_firmwareUpdateJson);
    this->m_http_check_with_auth_arr_of_urls = nullptr;
    this->m_http_check_with_auth_num_of_urls = 0;

    const char* const expected_json
        = "{\n\t\"status\":\t404,\n\t\"message\":\t\"Failed to download ruuvi_gateway_esp.bin\"\n}";

    const char* const p_uri_params
        = "validate_type=check_fw_update_url&url=https://network.ruuvi.com/firmwareupdate&auth_type=none";

    http_server_resp_t resp = http_server_cb_on_get("validate_url", p_uri_params, true, nullptr);

    ASSERT_EQ(HTTP_RESP_CODE_200, resp.http_resp_code);
    ASSERT_EQ(HTTP_CONTENT_LOCATION_HEAP, resp.content_location);
    ASSERT_TRUE(resp.flag_no_cache);
    ASSERT_EQ(HTTP_CONTENT_TYPE_APPLICATION_JSON, resp.content_type);
    ASSERT_EQ(nullptr, resp.p_content_type_param);
    ASSERT_NE(nullptr, resp.select_location.heap.p_buf);
    ASSERT_EQ(string(expected_json), string(reinterpret_cast<const char*>(resp.select_location.heap.p_buf)));
    ASSERT_EQ(strlen(expected_json), resp.content_len);
    ASSERT_EQ(HTTP_CONTENT_ENCODING_NONE, resp.content_encoding);
    ASSERT_EQ(
        "D http_server: http_server_cb_on_get /validate_url?validate_type=check_fw_update_url&url=https://network.ruuvi.com/firmwareupdate&auth_type=none\n"
        "I http_server: http_server_cb_on_get: Clear all saved TLS session tickets\n"
        "D http_server: HTTP params: auth_type=none\n"
        "D http_server: HTTP params: key 'auth_type=': value (encoded): none\n"
        "D http_server: HTTP params: key 'auth_type=': value (decoded): none\n"
        "D http_server: HTTP params: url=https://network.ruuvi.com/firmwareupdate\n"
        "D http_server: HTTP params: key 'url=': value (encoded): https://network.ruuvi.com/firmwareupdate\n"
        "D http_server: HTTP params: key 'url=': value (decoded): https://network.ruuvi.com/firmwareupdate\n"
        "D http_server: Can't find key 'user=' in URL params\n"
        "E http_server: HTTP params: Can't find 'user='\n"
        "D http_server: Can't find key 'encrypted_password=' in URL params\n"
        "E http_server: HTTP params: Can't find 'encrypted_password='\n"
        "D http_server: Can't find key 'use_saved_password=' in URL params\n"
        "E http_server: HTTP params: Can't find 'use_saved_password='\n"
        "E http_server: Can't find key: use_saved_password=\n"
        "E http_server: Can't find prefix 'use_ssl_client_cert='\n"
        "E http_server: Can't find prefix 'use_ssl_server_cert='\n"
        "D http_server: Can't find prefix 'use_extra_http_path='\n"
        "D http_server: Can't find prefix 'use_extra_http_query='\n"
        "D http_server: Can't find prefix 'use_extra_http_headers='\n"
        "D http_server: Got URL from params: https://network.ruuvi.com/firmwareupdate\n"
        "I http_server: Found validate_type: check_fw_update_url\n"
        "I http_download: cb_on_http_download_json_data: buf_size=" + std::to_string(strlen(p_firmwareUpdateJson)) + "\n"
            "I http_download: http_download_json: completed within 0 ticks\n"
            "I http_server: Firmware update info (json): " + string(p_firmwareUpdateJson) + "\n"
            "E http_download: http_check failed for URL: https://fwupdate.ruuvi.com/v1.14.3/ruuvi_gateway_esp.bin\n"
            "I http_download: http_check: completed within 0 ticks\n"
            "E http_server: Failed to download ruuvi_gateway_esp.bin, HTTP error: 404\n"
            "E http_server: Failed to download firmware update: http_resp_code=404, failed to download file: ruuvi_gateway_esp.bin\n",
        esp_log_wrapper_get_logs());
    http_server_resp_free(&resp);
    os_malloc_trace_dump();
    ESP_LOG_WRAPPER_TEST_CHECK_LOG_RECORD("MEM_TRACE", ESP_LOG_INFO, "Num blocks allocated: 0");
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_EQ(0, this->m_alloc_free_call_count);
}

TEST_F(TestHttpServerCb, http_server_cb_on_get_validate_url_check_fw_update_url__empty_json) // NOLINT
{
    const char* const p_firmwareUpdateJson
        = "{"
          "  \"latest\": {"
          "    \"version\": \"\","
          "    \"url\": \"\","
          "    \"created_at\": \"\""
          "  }"
          "}";
    this->m_firmware_update_resp             = str_buf_printf_with_alloc("%s", p_firmwareUpdateJson);
    this->m_http_check_with_auth_arr_of_urls = nullptr;
    this->m_http_check_with_auth_num_of_urls = 0;

    const char* const expected_json
        = "{\n\t\"status\":\t502,\n\t\"message\":\t\"Server returned incorrect json: could not get latest/url\"\n}";

    const char* const p_uri_params
        = "validate_type=check_fw_update_url&url=https://network.ruuvi.com/firmwareupdate&auth_type=none";

    http_server_resp_t resp = http_server_cb_on_get("validate_url", p_uri_params, true, nullptr);

    ASSERT_EQ(HTTP_RESP_CODE_200, resp.http_resp_code);
    ASSERT_EQ(HTTP_CONTENT_LOCATION_HEAP, resp.content_location);
    ASSERT_TRUE(resp.flag_no_cache);
    ASSERT_EQ(HTTP_CONTENT_TYPE_APPLICATION_JSON, resp.content_type);
    ASSERT_EQ(nullptr, resp.p_content_type_param);
    ASSERT_NE(nullptr, resp.select_location.heap.p_buf);
    ASSERT_EQ(string(expected_json), string(reinterpret_cast<const char*>(resp.select_location.heap.p_buf)));
    ASSERT_EQ(strlen(expected_json), resp.content_len);
    ASSERT_EQ(HTTP_CONTENT_ENCODING_NONE, resp.content_encoding);
    ASSERT_EQ(
        "D http_server: http_server_cb_on_get /validate_url?validate_type=check_fw_update_url&url=https://network.ruuvi.com/firmwareupdate&auth_type=none\n"
        "I http_server: http_server_cb_on_get: Clear all saved TLS session tickets\n"
        "D http_server: HTTP params: auth_type=none\n"
        "D http_server: HTTP params: key 'auth_type=': value (encoded): none\n"
        "D http_server: HTTP params: key 'auth_type=': value (decoded): none\n"
        "D http_server: HTTP params: url=https://network.ruuvi.com/firmwareupdate\n"
        "D http_server: HTTP params: key 'url=': value (encoded): https://network.ruuvi.com/firmwareupdate\n"
        "D http_server: HTTP params: key 'url=': value (decoded): https://network.ruuvi.com/firmwareupdate\n"
        "D http_server: Can't find key 'user=' in URL params\n"
        "E http_server: HTTP params: Can't find 'user='\n"
        "D http_server: Can't find key 'encrypted_password=' in URL params\n"
        "E http_server: HTTP params: Can't find 'encrypted_password='\n"
        "D http_server: Can't find key 'use_saved_password=' in URL params\n"
        "E http_server: HTTP params: Can't find 'use_saved_password='\n"
        "E http_server: Can't find key: use_saved_password=\n"
        "E http_server: Can't find prefix 'use_ssl_client_cert='\n"
        "E http_server: Can't find prefix 'use_ssl_server_cert='\n"
        "D http_server: Can't find prefix 'use_extra_http_path='\n"
        "D http_server: Can't find prefix 'use_extra_http_query='\n"
        "D http_server: Can't find prefix 'use_extra_http_headers='\n"
        "D http_server: Got URL from params: https://network.ruuvi.com/firmwareupdate\n"
        "I http_server: Found validate_type: check_fw_update_url\n"
        "I http_download: cb_on_http_download_json_data: buf_size=" + std::to_string(strlen(p_firmwareUpdateJson)) + "\n"
            "I http_download: http_download_json: completed within 0 ticks\n"
            "I http_server: Firmware update info (json): " + string(p_firmwareUpdateJson) + "\n"
            "E http_server: Firmware_update info 'latest/version' is empty\n"
            "E http_server: Failed to parse fw_update_info: " + string(p_firmwareUpdateJson) + "\n",
        esp_log_wrapper_get_logs());
    http_server_resp_free(&resp);
    os_malloc_trace_dump();
    ESP_LOG_WRAPPER_TEST_CHECK_LOG_RECORD("MEM_TRACE", ESP_LOG_INFO, "Num blocks allocated: 0");
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_EQ(0, this->m_alloc_free_call_count);
}

TEST_F(TestHttpServerCb, http_server_cb_on_get_validate_url_check_fw_update_url__invalid_json) // NOLINT
{
    const char* const p_firmwareUpdateJson
        = "{"
          "  \"latest\": {"
          "    \"version\": \"v1.14.3\","
          "    \"url\": \"https://fwupdate.ruuvi.com/v1.14.3\","
          "    \"created_at\": \"2023-10-06T11:26:07Z\""
          "  }";
    this->m_firmware_update_resp             = str_buf_printf_with_alloc("%s", p_firmwareUpdateJson);
    this->m_http_check_with_auth_arr_of_urls = nullptr;
    this->m_http_check_with_auth_num_of_urls = 0;

    const char* const expected_json
        = "{\n\t\"status\":\t502,\n\t\"message\":\t\"Server returned incorrect json: could not parse json\"\n}";

    const char* const p_uri_params
        = "validate_type=check_fw_update_url&url=https://network.ruuvi.com/firmwareupdate&auth_type=none";

    http_server_resp_t resp = http_server_cb_on_get("validate_url", p_uri_params, true, nullptr);

    ASSERT_EQ(HTTP_RESP_CODE_200, resp.http_resp_code);
    ASSERT_EQ(HTTP_CONTENT_LOCATION_HEAP, resp.content_location);
    ASSERT_TRUE(resp.flag_no_cache);
    ASSERT_EQ(HTTP_CONTENT_TYPE_APPLICATION_JSON, resp.content_type);
    ASSERT_EQ(nullptr, resp.p_content_type_param);
    ASSERT_NE(nullptr, resp.select_location.heap.p_buf);
    ASSERT_EQ(string(expected_json), string(reinterpret_cast<const char*>(resp.select_location.heap.p_buf)));
    ASSERT_EQ(strlen(expected_json), resp.content_len);
    ASSERT_EQ(HTTP_CONTENT_ENCODING_NONE, resp.content_encoding);
    ASSERT_EQ(
        "D http_server: http_server_cb_on_get /validate_url?validate_type=check_fw_update_url&url=https://network.ruuvi.com/firmwareupdate&auth_type=none\n"
        "I http_server: http_server_cb_on_get: Clear all saved TLS session tickets\n"
        "D http_server: HTTP params: auth_type=none\n"
        "D http_server: HTTP params: key 'auth_type=': value (encoded): none\n"
        "D http_server: HTTP params: key 'auth_type=': value (decoded): none\n"
        "D http_server: HTTP params: url=https://network.ruuvi.com/firmwareupdate\n"
        "D http_server: HTTP params: key 'url=': value (encoded): https://network.ruuvi.com/firmwareupdate\n"
        "D http_server: HTTP params: key 'url=': value (decoded): https://network.ruuvi.com/firmwareupdate\n"
        "D http_server: Can't find key 'user=' in URL params\n"
        "E http_server: HTTP params: Can't find 'user='\n"
        "D http_server: Can't find key 'encrypted_password=' in URL params\n"
        "E http_server: HTTP params: Can't find 'encrypted_password='\n"
        "D http_server: Can't find key 'use_saved_password=' in URL params\n"
        "E http_server: HTTP params: Can't find 'use_saved_password='\n"
        "E http_server: Can't find key: use_saved_password=\n"
        "E http_server: Can't find prefix 'use_ssl_client_cert='\n"
        "E http_server: Can't find prefix 'use_ssl_server_cert='\n"
        "D http_server: Can't find prefix 'use_extra_http_path='\n"
        "D http_server: Can't find prefix 'use_extra_http_query='\n"
        "D http_server: Can't find prefix 'use_extra_http_headers='\n"
        "D http_server: Got URL from params: https://network.ruuvi.com/firmwareupdate\n"
        "I http_server: Found validate_type: check_fw_update_url\n"
        "I http_download: cb_on_http_download_json_data: buf_size=" + std::to_string(strlen(p_firmwareUpdateJson)) + "\n"
        "I http_download: http_download_json: completed within 0 ticks\n"
        "I http_server: Firmware update info (json): " + string(p_firmwareUpdateJson) + "\n"
        "E http_server: Failed to parse fw_update_info: " + string(p_firmwareUpdateJson) + "\n",
        esp_log_wrapper_get_logs());
    http_server_resp_free(&resp);
    os_malloc_trace_dump();
    ESP_LOG_WRAPPER_TEST_CHECK_LOG_RECORD("MEM_TRACE", ESP_LOG_INFO, "Num blocks allocated: 0");
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_EQ(0, this->m_alloc_free_call_count);
}

TEST_F(TestHttpServerCb, http_server_cb_on_get_validate_url_check_fw_update_url__empty_version) // NOLINT
{
    const char* const p_firmwareUpdateJson
        = "{"
          "  \"latest\": {"
          "    \"version\": \"\","
          "    \"url\": \"https://fwupdate.ruuvi.com/v1.14.3\","
          "    \"created_at\": \"2023-10-06T11:26:07Z\""
          "  }"
          "}";
    this->m_firmware_update_resp             = str_buf_printf_with_alloc("%s", p_firmwareUpdateJson);
    this->m_http_check_with_auth_arr_of_urls = nullptr;
    this->m_http_check_with_auth_num_of_urls = 0;

    const char* const expected_json
        = "{\n\t\"status\":\t502,\n\t\"message\":\t\"Server returned incorrect json: could not get latest/url\"\n}";

    const char* const p_uri_params
        = "validate_type=check_fw_update_url&url=https://network.ruuvi.com/firmwareupdate&auth_type=none";

    http_server_resp_t resp = http_server_cb_on_get("validate_url", p_uri_params, true, nullptr);

    ASSERT_EQ(HTTP_RESP_CODE_200, resp.http_resp_code);
    ASSERT_EQ(HTTP_CONTENT_LOCATION_HEAP, resp.content_location);
    ASSERT_TRUE(resp.flag_no_cache);
    ASSERT_EQ(HTTP_CONTENT_TYPE_APPLICATION_JSON, resp.content_type);
    ASSERT_EQ(nullptr, resp.p_content_type_param);
    ASSERT_NE(nullptr, resp.select_location.heap.p_buf);
    ASSERT_EQ(string(expected_json), string(reinterpret_cast<const char*>(resp.select_location.heap.p_buf)));
    ASSERT_EQ(strlen(expected_json), resp.content_len);
    ASSERT_EQ(HTTP_CONTENT_ENCODING_NONE, resp.content_encoding);
    ASSERT_EQ(
        "D http_server: http_server_cb_on_get /validate_url?validate_type=check_fw_update_url&url=https://network.ruuvi.com/firmwareupdate&auth_type=none\n"
        "I http_server: http_server_cb_on_get: Clear all saved TLS session tickets\n"
        "D http_server: HTTP params: auth_type=none\n"
        "D http_server: HTTP params: key 'auth_type=': value (encoded): none\n"
        "D http_server: HTTP params: key 'auth_type=': value (decoded): none\n"
        "D http_server: HTTP params: url=https://network.ruuvi.com/firmwareupdate\n"
        "D http_server: HTTP params: key 'url=': value (encoded): https://network.ruuvi.com/firmwareupdate\n"
        "D http_server: HTTP params: key 'url=': value (decoded): https://network.ruuvi.com/firmwareupdate\n"
        "D http_server: Can't find key 'user=' in URL params\n"
        "E http_server: HTTP params: Can't find 'user='\n"
        "D http_server: Can't find key 'encrypted_password=' in URL params\n"
        "E http_server: HTTP params: Can't find 'encrypted_password='\n"
        "D http_server: Can't find key 'use_saved_password=' in URL params\n"
        "E http_server: HTTP params: Can't find 'use_saved_password='\n"
        "E http_server: Can't find key: use_saved_password=\n"
        "E http_server: Can't find prefix 'use_ssl_client_cert='\n"
        "E http_server: Can't find prefix 'use_ssl_server_cert='\n"
        "D http_server: Can't find prefix 'use_extra_http_path='\n"
        "D http_server: Can't find prefix 'use_extra_http_query='\n"
        "D http_server: Can't find prefix 'use_extra_http_headers='\n"
        "D http_server: Got URL from params: https://network.ruuvi.com/firmwareupdate\n"
        "I http_server: Found validate_type: check_fw_update_url\n"
        "I http_download: cb_on_http_download_json_data: buf_size=" + std::to_string(strlen(p_firmwareUpdateJson)) + "\n"
        "I http_download: http_download_json: completed within 0 ticks\n"
        "I http_server: Firmware update info (json): " + string(p_firmwareUpdateJson) + "\n"
        "E http_server: Firmware_update info 'latest/version' is empty\n"
        "E http_server: Failed to parse fw_update_info: " + string(p_firmwareUpdateJson) + "\n",
        esp_log_wrapper_get_logs());
    http_server_resp_free(&resp);
    os_malloc_trace_dump();
    ESP_LOG_WRAPPER_TEST_CHECK_LOG_RECORD("MEM_TRACE", ESP_LOG_INFO, "Num blocks allocated: 0");
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_EQ(0, this->m_alloc_free_call_count);
}

TEST_F(TestHttpServerCb, http_server_cb_on_get_validate_url_check_fw_update_url__no_version) // NOLINT
{
    const char* const p_firmwareUpdateJson
        = "{"
          "  \"latest\": {"
          "    \"url\": \"https://fwupdate.ruuvi.com/v1.14.3\","
          "    \"created_at\": \"2023-10-06T11:26:07Z\""
          "  }"
          "}";
    this->m_firmware_update_resp             = str_buf_printf_with_alloc("%s", p_firmwareUpdateJson);
    this->m_http_check_with_auth_arr_of_urls = nullptr;
    this->m_http_check_with_auth_num_of_urls = 0;

    const char* const expected_json
        = "{\n\t\"status\":\t502,\n\t\"message\":\t\"Server returned incorrect json: could not get latest/url\"\n}";

    const char* const p_uri_params
        = "validate_type=check_fw_update_url&url=https://network.ruuvi.com/firmwareupdate&auth_type=none";

    http_server_resp_t resp = http_server_cb_on_get("validate_url", p_uri_params, true, nullptr);

    ASSERT_EQ(HTTP_RESP_CODE_200, resp.http_resp_code);
    ASSERT_EQ(HTTP_CONTENT_LOCATION_HEAP, resp.content_location);
    ASSERT_TRUE(resp.flag_no_cache);
    ASSERT_EQ(HTTP_CONTENT_TYPE_APPLICATION_JSON, resp.content_type);
    ASSERT_EQ(nullptr, resp.p_content_type_param);
    ASSERT_NE(nullptr, resp.select_location.heap.p_buf);
    ASSERT_EQ(string(expected_json), string(reinterpret_cast<const char*>(resp.select_location.heap.p_buf)));
    ASSERT_EQ(strlen(expected_json), resp.content_len);
    ASSERT_EQ(HTTP_CONTENT_ENCODING_NONE, resp.content_encoding);
    ASSERT_EQ(
        "D http_server: http_server_cb_on_get /validate_url?validate_type=check_fw_update_url&url=https://network.ruuvi.com/firmwareupdate&auth_type=none\n"
        "I http_server: http_server_cb_on_get: Clear all saved TLS session tickets\n"
        "D http_server: HTTP params: auth_type=none\n"
        "D http_server: HTTP params: key 'auth_type=': value (encoded): none\n"
        "D http_server: HTTP params: key 'auth_type=': value (decoded): none\n"
        "D http_server: HTTP params: url=https://network.ruuvi.com/firmwareupdate\n"
        "D http_server: HTTP params: key 'url=': value (encoded): https://network.ruuvi.com/firmwareupdate\n"
        "D http_server: HTTP params: key 'url=': value (decoded): https://network.ruuvi.com/firmwareupdate\n"
        "D http_server: Can't find key 'user=' in URL params\n"
        "E http_server: HTTP params: Can't find 'user='\n"
        "D http_server: Can't find key 'encrypted_password=' in URL params\n"
        "E http_server: HTTP params: Can't find 'encrypted_password='\n"
        "D http_server: Can't find key 'use_saved_password=' in URL params\n"
        "E http_server: HTTP params: Can't find 'use_saved_password='\n"
        "E http_server: Can't find key: use_saved_password=\n"
        "E http_server: Can't find prefix 'use_ssl_client_cert='\n"
        "E http_server: Can't find prefix 'use_ssl_server_cert='\n"
        "D http_server: Can't find prefix 'use_extra_http_path='\n"
        "D http_server: Can't find prefix 'use_extra_http_query='\n"
        "D http_server: Can't find prefix 'use_extra_http_headers='\n"
        "D http_server: Got URL from params: https://network.ruuvi.com/firmwareupdate\n"
        "I http_server: Found validate_type: check_fw_update_url\n"
        "I http_download: cb_on_http_download_json_data: buf_size=" + std::to_string(strlen(p_firmwareUpdateJson)) + "\n"
        "I http_download: http_download_json: completed within 0 ticks\n"
        "I http_server: Firmware update info (json): " + string(p_firmwareUpdateJson) + "\n"
        "E http_server: Can't find key 'latest/version' in firmware_update info\n"
        "E http_server: Failed to parse fw_update_info: " + string(p_firmwareUpdateJson) + "\n",
        esp_log_wrapper_get_logs());
    http_server_resp_free(&resp);
    os_malloc_trace_dump();
    ESP_LOG_WRAPPER_TEST_CHECK_LOG_RECORD("MEM_TRACE", ESP_LOG_INFO, "Num blocks allocated: 0");
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_EQ(0, this->m_alloc_free_call_count);
}

TEST_F(TestHttpServerCb, http_server_cb_on_get_validate_url_check_fw_update_url__empty_url) // NOLINT
{
    const char* const p_firmwareUpdateJson
        = "{"
          "  \"latest\": {"
          "    \"version\": \"v1.14.3\","
          "    \"url\": \"\","
          "    \"created_at\": \"2023-10-06T11:26:07Z\""
          "  }"
          "}";
    this->m_firmware_update_resp             = str_buf_printf_with_alloc("%s", p_firmwareUpdateJson);
    this->m_http_check_with_auth_arr_of_urls = nullptr;
    this->m_http_check_with_auth_num_of_urls = 0;

    const char* const expected_json
        = "{\n\t\"status\":\t502,\n\t\"message\":\t\"Server returned incorrect json: could not get latest/url\"\n}";

    const char* const p_uri_params
        = "validate_type=check_fw_update_url&url=https://network.ruuvi.com/firmwareupdate&auth_type=none";

    http_server_resp_t resp = http_server_cb_on_get("validate_url", p_uri_params, true, nullptr);

    ASSERT_EQ(HTTP_RESP_CODE_200, resp.http_resp_code);
    ASSERT_EQ(HTTP_CONTENT_LOCATION_HEAP, resp.content_location);
    ASSERT_TRUE(resp.flag_no_cache);
    ASSERT_EQ(HTTP_CONTENT_TYPE_APPLICATION_JSON, resp.content_type);
    ASSERT_EQ(nullptr, resp.p_content_type_param);
    ASSERT_NE(nullptr, resp.select_location.heap.p_buf);
    ASSERT_EQ(string(expected_json), string(reinterpret_cast<const char*>(resp.select_location.heap.p_buf)));
    ASSERT_EQ(strlen(expected_json), resp.content_len);
    ASSERT_EQ(HTTP_CONTENT_ENCODING_NONE, resp.content_encoding);
    ASSERT_EQ(
        "D http_server: http_server_cb_on_get /validate_url?validate_type=check_fw_update_url&url=https://network.ruuvi.com/firmwareupdate&auth_type=none\n"
        "I http_server: http_server_cb_on_get: Clear all saved TLS session tickets\n"
        "D http_server: HTTP params: auth_type=none\n"
        "D http_server: HTTP params: key 'auth_type=': value (encoded): none\n"
        "D http_server: HTTP params: key 'auth_type=': value (decoded): none\n"
        "D http_server: HTTP params: url=https://network.ruuvi.com/firmwareupdate\n"
        "D http_server: HTTP params: key 'url=': value (encoded): https://network.ruuvi.com/firmwareupdate\n"
        "D http_server: HTTP params: key 'url=': value (decoded): https://network.ruuvi.com/firmwareupdate\n"
        "D http_server: Can't find key 'user=' in URL params\n"
        "E http_server: HTTP params: Can't find 'user='\n"
        "D http_server: Can't find key 'encrypted_password=' in URL params\n"
        "E http_server: HTTP params: Can't find 'encrypted_password='\n"
        "D http_server: Can't find key 'use_saved_password=' in URL params\n"
        "E http_server: HTTP params: Can't find 'use_saved_password='\n"
        "E http_server: Can't find key: use_saved_password=\n"
        "E http_server: Can't find prefix 'use_ssl_client_cert='\n"
        "E http_server: Can't find prefix 'use_ssl_server_cert='\n"
        "D http_server: Can't find prefix 'use_extra_http_path='\n"
        "D http_server: Can't find prefix 'use_extra_http_query='\n"
        "D http_server: Can't find prefix 'use_extra_http_headers='\n"
        "D http_server: Got URL from params: https://network.ruuvi.com/firmwareupdate\n"
        "I http_server: Found validate_type: check_fw_update_url\n"
        "I http_download: cb_on_http_download_json_data: buf_size=" + std::to_string(strlen(p_firmwareUpdateJson)) + "\n"
        "I http_download: http_download_json: completed within 0 ticks\n"
        "I http_server: Firmware update info (json): " + string(p_firmwareUpdateJson) + "\n"
        "E http_server: Firmware_update info 'latest/url' is empty\n"
        "E http_server: Failed to parse fw_update_info: " + string(p_firmwareUpdateJson) + "\n",
        esp_log_wrapper_get_logs());
    http_server_resp_free(&resp);
    os_malloc_trace_dump();
    ESP_LOG_WRAPPER_TEST_CHECK_LOG_RECORD("MEM_TRACE", ESP_LOG_INFO, "Num blocks allocated: 0");
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_EQ(0, this->m_alloc_free_call_count);
}

TEST_F(TestHttpServerCb, http_server_cb_on_get_validate_url_check_fw_update_url__no_url) // NOLINT
{
    const char* const p_firmwareUpdateJson
        = "{"
          "  \"latest\": {"
          "    \"version\": \"v1.14.3\","
          "    \"created_at\": \"2023-10-06T11:26:07Z\""
          "  }"
          "}";
    this->m_firmware_update_resp             = str_buf_printf_with_alloc("%s", p_firmwareUpdateJson);
    this->m_http_check_with_auth_arr_of_urls = nullptr;
    this->m_http_check_with_auth_num_of_urls = 0;

    const char* const expected_json
        = "{\n\t\"status\":\t502,\n\t\"message\":\t\"Server returned incorrect json: could not get latest/url\"\n}";

    const char* const p_uri_params
        = "validate_type=check_fw_update_url&url=https://network.ruuvi.com/firmwareupdate&auth_type=none";

    http_server_resp_t resp = http_server_cb_on_get("validate_url", p_uri_params, true, nullptr);

    ASSERT_EQ(HTTP_RESP_CODE_200, resp.http_resp_code);
    ASSERT_EQ(HTTP_CONTENT_LOCATION_HEAP, resp.content_location);
    ASSERT_TRUE(resp.flag_no_cache);
    ASSERT_EQ(HTTP_CONTENT_TYPE_APPLICATION_JSON, resp.content_type);
    ASSERT_EQ(nullptr, resp.p_content_type_param);
    ASSERT_NE(nullptr, resp.select_location.heap.p_buf);
    ASSERT_EQ(string(expected_json), string(reinterpret_cast<const char*>(resp.select_location.heap.p_buf)));
    ASSERT_EQ(strlen(expected_json), resp.content_len);
    ASSERT_EQ(HTTP_CONTENT_ENCODING_NONE, resp.content_encoding);
    ASSERT_EQ(
        "D http_server: http_server_cb_on_get /validate_url?validate_type=check_fw_update_url&url=https://network.ruuvi.com/firmwareupdate&auth_type=none\n"
        "I http_server: http_server_cb_on_get: Clear all saved TLS session tickets\n"
        "D http_server: HTTP params: auth_type=none\n"
        "D http_server: HTTP params: key 'auth_type=': value (encoded): none\n"
        "D http_server: HTTP params: key 'auth_type=': value (decoded): none\n"
        "D http_server: HTTP params: url=https://network.ruuvi.com/firmwareupdate\n"
        "D http_server: HTTP params: key 'url=': value (encoded): https://network.ruuvi.com/firmwareupdate\n"
        "D http_server: HTTP params: key 'url=': value (decoded): https://network.ruuvi.com/firmwareupdate\n"
        "D http_server: Can't find key 'user=' in URL params\n"
        "E http_server: HTTP params: Can't find 'user='\n"
        "D http_server: Can't find key 'encrypted_password=' in URL params\n"
        "E http_server: HTTP params: Can't find 'encrypted_password='\n"
        "D http_server: Can't find key 'use_saved_password=' in URL params\n"
        "E http_server: HTTP params: Can't find 'use_saved_password='\n"
        "E http_server: Can't find key: use_saved_password=\n"
        "E http_server: Can't find prefix 'use_ssl_client_cert='\n"
        "E http_server: Can't find prefix 'use_ssl_server_cert='\n"
        "D http_server: Can't find prefix 'use_extra_http_path='\n"
        "D http_server: Can't find prefix 'use_extra_http_query='\n"
        "D http_server: Can't find prefix 'use_extra_http_headers='\n"
        "D http_server: Got URL from params: https://network.ruuvi.com/firmwareupdate\n"
        "I http_server: Found validate_type: check_fw_update_url\n"
        "I http_download: cb_on_http_download_json_data: buf_size=" + std::to_string(strlen(p_firmwareUpdateJson)) + "\n"
        "I http_download: http_download_json: completed within 0 ticks\n"
        "I http_server: Firmware update info (json): " + string(p_firmwareUpdateJson) + "\n"
        "E http_server: Can't find key 'latest/url' in firmware_update info\n"
        "E http_server: Failed to parse fw_update_info: " + string(p_firmwareUpdateJson) + "\n",
        esp_log_wrapper_get_logs());
    http_server_resp_free(&resp);
    os_malloc_trace_dump();
    ESP_LOG_WRAPPER_TEST_CHECK_LOG_RECORD("MEM_TRACE", ESP_LOG_INFO, "Num blocks allocated: 0");
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_EQ(0, this->m_alloc_free_call_count);
}

TEST_F(TestHttpServerCb, http_server_cb_on_get_validate_url_check_fw_update_url__invalid_url) // NOLINT
{
    const char* const p_firmwareUpdateJson
        = "{"
          "  \"latest\": {"
          "    \"version\": \"v1.14.3\","
          "    \"url\": \"qqq://abc.com\","
          "    \"created_at\": \"2023-10-06T11:26:07Z\""
          "  }"
          "}";
    this->m_firmware_update_resp             = str_buf_printf_with_alloc("%s", p_firmwareUpdateJson);
    this->m_http_check_with_auth_arr_of_urls = nullptr;
    this->m_http_check_with_auth_num_of_urls = 0;

    const char* const expected_json
        = "{\n\t\"status\":\t502,\n\t\"message\":\t\"Server returned incorrect json: could not get latest/url\"\n}";

    const char* const p_uri_params
        = "validate_type=check_fw_update_url&url=https://network.ruuvi.com/firmwareupdate&auth_type=none";

    http_server_resp_t resp = http_server_cb_on_get("validate_url", p_uri_params, true, nullptr);

    ASSERT_EQ(HTTP_RESP_CODE_200, resp.http_resp_code);
    ASSERT_EQ(HTTP_CONTENT_LOCATION_HEAP, resp.content_location);
    ASSERT_TRUE(resp.flag_no_cache);
    ASSERT_EQ(HTTP_CONTENT_TYPE_APPLICATION_JSON, resp.content_type);
    ASSERT_EQ(nullptr, resp.p_content_type_param);
    ASSERT_NE(nullptr, resp.select_location.heap.p_buf);
    ASSERT_EQ(string(expected_json), string(reinterpret_cast<const char*>(resp.select_location.heap.p_buf)));
    ASSERT_EQ(strlen(expected_json), resp.content_len);
    ASSERT_EQ(HTTP_CONTENT_ENCODING_NONE, resp.content_encoding);
    ASSERT_EQ(
        "D http_server: http_server_cb_on_get /validate_url?validate_type=check_fw_update_url&url=https://network.ruuvi.com/firmwareupdate&auth_type=none\n"
        "I http_server: http_server_cb_on_get: Clear all saved TLS session tickets\n"
        "D http_server: HTTP params: auth_type=none\n"
        "D http_server: HTTP params: key 'auth_type=': value (encoded): none\n"
        "D http_server: HTTP params: key 'auth_type=': value (decoded): none\n"
        "D http_server: HTTP params: url=https://network.ruuvi.com/firmwareupdate\n"
        "D http_server: HTTP params: key 'url=': value (encoded): https://network.ruuvi.com/firmwareupdate\n"
        "D http_server: HTTP params: key 'url=': value (decoded): https://network.ruuvi.com/firmwareupdate\n"
        "D http_server: Can't find key 'user=' in URL params\n"
        "E http_server: HTTP params: Can't find 'user='\n"
        "D http_server: Can't find key 'encrypted_password=' in URL params\n"
        "E http_server: HTTP params: Can't find 'encrypted_password='\n"
        "D http_server: Can't find key 'use_saved_password=' in URL params\n"
        "E http_server: HTTP params: Can't find 'use_saved_password='\n"
        "E http_server: Can't find key: use_saved_password=\n"
        "E http_server: Can't find prefix 'use_ssl_client_cert='\n"
        "E http_server: Can't find prefix 'use_ssl_server_cert='\n"
        "D http_server: Can't find prefix 'use_extra_http_path='\n"
        "D http_server: Can't find prefix 'use_extra_http_query='\n"
        "D http_server: Can't find prefix 'use_extra_http_headers='\n"
        "D http_server: Got URL from params: https://network.ruuvi.com/firmwareupdate\n"
        "I http_server: Found validate_type: check_fw_update_url\n"
        "I http_download: cb_on_http_download_json_data: buf_size=" + std::to_string(strlen(p_firmwareUpdateJson)) + "\n"
        "I http_download: http_download_json: completed within 0 ticks\n"
        "I http_server: Firmware update info (json): " + string(p_firmwareUpdateJson) + "\n"
        "E http_server: Firmware_update info 'latest/url' is invalid: qqq://abc.com\n"
        "E http_server: Failed to parse fw_update_info: " + string(p_firmwareUpdateJson) + "\n",
        esp_log_wrapper_get_logs());
    http_server_resp_free(&resp);
    os_malloc_trace_dump();
    ESP_LOG_WRAPPER_TEST_CHECK_LOG_RECORD("MEM_TRACE", ESP_LOG_INFO, "Num blocks allocated: 0");
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_EQ(0, this->m_alloc_free_call_count);
}

TEST_F(TestHttpServerCb, http_server_cb_on_get_validate_url_check_fw_update_url__no_latest) // NOLINT
{
    const char* const p_firmwareUpdateJson
        = "{"
          "}";
    this->m_firmware_update_resp             = str_buf_printf_with_alloc("%s", p_firmwareUpdateJson);
    this->m_http_check_with_auth_arr_of_urls = nullptr;
    this->m_http_check_with_auth_num_of_urls = 0;

    const char* const expected_json
        = "{\n\t\"status\":\t502,\n\t\"message\":\t\"Server returned incorrect json: could not get latest/url\"\n}";

    const char* const p_uri_params
        = "validate_type=check_fw_update_url&url=https://network.ruuvi.com/firmwareupdate&auth_type=none";

    http_server_resp_t resp = http_server_cb_on_get("validate_url", p_uri_params, true, nullptr);

    ASSERT_EQ(HTTP_RESP_CODE_200, resp.http_resp_code);
    ASSERT_EQ(HTTP_CONTENT_LOCATION_HEAP, resp.content_location);
    ASSERT_TRUE(resp.flag_no_cache);
    ASSERT_EQ(HTTP_CONTENT_TYPE_APPLICATION_JSON, resp.content_type);
    ASSERT_EQ(nullptr, resp.p_content_type_param);
    ASSERT_NE(nullptr, resp.select_location.heap.p_buf);
    ASSERT_EQ(string(expected_json), string(reinterpret_cast<const char*>(resp.select_location.heap.p_buf)));
    ASSERT_EQ(strlen(expected_json), resp.content_len);
    ASSERT_EQ(HTTP_CONTENT_ENCODING_NONE, resp.content_encoding);
    ASSERT_EQ(
        "D http_server: http_server_cb_on_get /validate_url?validate_type=check_fw_update_url&url=https://network.ruuvi.com/firmwareupdate&auth_type=none\n"
        "I http_server: http_server_cb_on_get: Clear all saved TLS session tickets\n"
        "D http_server: HTTP params: auth_type=none\n"
        "D http_server: HTTP params: key 'auth_type=': value (encoded): none\n"
        "D http_server: HTTP params: key 'auth_type=': value (decoded): none\n"
        "D http_server: HTTP params: url=https://network.ruuvi.com/firmwareupdate\n"
        "D http_server: HTTP params: key 'url=': value (encoded): https://network.ruuvi.com/firmwareupdate\n"
        "D http_server: HTTP params: key 'url=': value (decoded): https://network.ruuvi.com/firmwareupdate\n"
        "D http_server: Can't find key 'user=' in URL params\n"
        "E http_server: HTTP params: Can't find 'user='\n"
        "D http_server: Can't find key 'encrypted_password=' in URL params\n"
        "E http_server: HTTP params: Can't find 'encrypted_password='\n"
        "D http_server: Can't find key 'use_saved_password=' in URL params\n"
        "E http_server: HTTP params: Can't find 'use_saved_password='\n"
        "E http_server: Can't find key: use_saved_password=\n"
        "E http_server: Can't find prefix 'use_ssl_client_cert='\n"
        "E http_server: Can't find prefix 'use_ssl_server_cert='\n"
        "D http_server: Can't find prefix 'use_extra_http_path='\n"
        "D http_server: Can't find prefix 'use_extra_http_query='\n"
        "D http_server: Can't find prefix 'use_extra_http_headers='\n"
        "D http_server: Got URL from params: https://network.ruuvi.com/firmwareupdate\n"
        "I http_server: Found validate_type: check_fw_update_url\n"
        "I http_download: cb_on_http_download_json_data: buf_size=" + std::to_string(strlen(p_firmwareUpdateJson)) + "\n"
        "I http_download: http_download_json: completed within 0 ticks\n"
        "I http_server: Firmware update info (json): " + string(p_firmwareUpdateJson) + "\n"
        "E http_server: Can't find key 'latest' in firmware_update info\n"
        "E http_server: Failed to parse fw_update_info: " + string(p_firmwareUpdateJson) + "\n",
        esp_log_wrapper_get_logs());
    http_server_resp_free(&resp);
    os_malloc_trace_dump();
    ESP_LOG_WRAPPER_TEST_CHECK_LOG_RECORD("MEM_TRACE", ESP_LOG_INFO, "Num blocks allocated: 0");
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_EQ(0, this->m_alloc_free_call_count);
}

TEST_F(TestHttpServerCb, http_server_cb_on_get_validate_url_check_fw_update_url__empty_resp) // NOLINT
{
    const char* const p_firmwareUpdateJson   = "";
    this->m_firmware_update_resp             = str_buf_printf_with_alloc("%s", p_firmwareUpdateJson);
    this->m_http_check_with_auth_arr_of_urls = nullptr;
    this->m_http_check_with_auth_num_of_urls = 0;

    const char* const expected_json
        = "{\n\t\"status\":\t502,\n\t\"message\":\t\"Server returned incorrect json: could not parse json\"\n}";

    const char* const p_uri_params
        = "validate_type=check_fw_update_url&url=https://network.ruuvi.com/firmwareupdate&auth_type=none";

    http_server_resp_t resp = http_server_cb_on_get("validate_url", p_uri_params, true, nullptr);

    ASSERT_EQ(HTTP_RESP_CODE_200, resp.http_resp_code);
    ASSERT_EQ(HTTP_CONTENT_LOCATION_HEAP, resp.content_location);
    ASSERT_TRUE(resp.flag_no_cache);
    ASSERT_EQ(HTTP_CONTENT_TYPE_APPLICATION_JSON, resp.content_type);
    ASSERT_EQ(nullptr, resp.p_content_type_param);
    ASSERT_NE(nullptr, resp.select_location.heap.p_buf);
    ASSERT_EQ(string(expected_json), string(reinterpret_cast<const char*>(resp.select_location.heap.p_buf)));
    ASSERT_EQ(strlen(expected_json), resp.content_len);
    ASSERT_EQ(HTTP_CONTENT_ENCODING_NONE, resp.content_encoding);
    ASSERT_EQ(
        "D http_server: http_server_cb_on_get /validate_url?validate_type=check_fw_update_url&url=https://network.ruuvi.com/firmwareupdate&auth_type=none\n"
        "I http_server: http_server_cb_on_get: Clear all saved TLS session tickets\n"
        "D http_server: HTTP params: auth_type=none\n"
        "D http_server: HTTP params: key 'auth_type=': value (encoded): none\n"
        "D http_server: HTTP params: key 'auth_type=': value (decoded): none\n"
        "D http_server: HTTP params: url=https://network.ruuvi.com/firmwareupdate\n"
        "D http_server: HTTP params: key 'url=': value (encoded): https://network.ruuvi.com/firmwareupdate\n"
        "D http_server: HTTP params: key 'url=': value (decoded): https://network.ruuvi.com/firmwareupdate\n"
        "D http_server: Can't find key 'user=' in URL params\n"
        "E http_server: HTTP params: Can't find 'user='\n"
        "D http_server: Can't find key 'encrypted_password=' in URL params\n"
        "E http_server: HTTP params: Can't find 'encrypted_password='\n"
        "D http_server: Can't find key 'use_saved_password=' in URL params\n"
        "E http_server: HTTP params: Can't find 'use_saved_password='\n"
        "E http_server: Can't find key: use_saved_password=\n"
        "E http_server: Can't find prefix 'use_ssl_client_cert='\n"
        "E http_server: Can't find prefix 'use_ssl_server_cert='\n"
        "D http_server: Can't find prefix 'use_extra_http_path='\n"
        "D http_server: Can't find prefix 'use_extra_http_query='\n"
        "D http_server: Can't find prefix 'use_extra_http_headers='\n"
        "D http_server: Got URL from params: https://network.ruuvi.com/firmwareupdate\n"
        "I http_server: Found validate_type: check_fw_update_url\n"
        "I http_download: cb_on_http_download_json_data: buf_size=" + std::to_string(strlen(p_firmwareUpdateJson)) + "\n"
        "I http_download: http_download_json: completed within 0 ticks\n"
        "I http_server: Firmware update info (json): " + string(p_firmwareUpdateJson) + "\n"
        "E http_server: Failed to parse fw_update_info: " + string(p_firmwareUpdateJson) + "\n",
        esp_log_wrapper_get_logs());
    http_server_resp_free(&resp);
    os_malloc_trace_dump();
    ESP_LOG_WRAPPER_TEST_CHECK_LOG_RECORD("MEM_TRACE", ESP_LOG_INFO, "Num blocks allocated: 0");
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_EQ(0, this->m_alloc_free_call_count);
}

TEST_F(TestHttpServerCb, http_server_cb_on_get_validate_url_check_fw_update_url__json_with_beta_empty_version) // NOLINT
{
    const char* const p_firmwareUpdateJson
        = "{"
          "  \"latest\": {"
          "    \"version\": \"v1.14.3\","
          "    \"url\": \"https://fwupdate.ruuvi.com/v1.14.3\","
          "    \"created_at\": \"2023-10-06T11:26:07Z\""
          "  },"
          "  \"beta\": {"
          "    \"version\": \"\","
          "    \"url\": \"https://github.com/ruuvi/ruuvi.gateway_esp.c/releases/download/v1.14.2\","
          "    \"created_at\": \"2023-09-19T11:16:48Z\""
          "  }"
          "}";
    this->m_firmware_update_resp             = str_buf_printf_with_alloc("%s", p_firmwareUpdateJson);
    const char* arr_of_binaries[]            = { "https://fwupdate.ruuvi.com/v1.14.3/ruuvi_gateway_esp.bin",
                                                 "https://fwupdate.ruuvi.com/v1.14.3/fatfs_gwui.bin",
                                                 "https://fwupdate.ruuvi.com/v1.14.3/fatfs_nrf52.bin" };
    this->m_http_check_with_auth_arr_of_urls = arr_of_binaries;
    this->m_http_check_with_auth_num_of_urls = sizeof(arr_of_binaries) / sizeof(arr_of_binaries[0]);

    const char* const expected_json
        = "{\"status\": 200, \"json\": {  \"latest\": {    \"version\": \"v1.14.3\",    \"url\": "
          "\"https://fwupdate.ruuvi.com/v1.14.3\",    \"created_at\": \"2023-10-06T11:26:07Z\"  },  \"beta\": {    "
          "\"version\": \"\",    \"url\": "
          "\"https://github.com/ruuvi/ruuvi.gateway_esp.c/releases/download/v1.14.2\",    \"created_at\": "
          "\"2023-09-19T11:16:48Z\"  }}}";

    const char* const p_uri_params
        = "validate_type=check_fw_update_url&url=https://network.ruuvi.com/firmwareupdate&auth_type=none";

    http_server_resp_t resp = http_server_cb_on_get("validate_url", p_uri_params, true, nullptr);

    ASSERT_EQ(HTTP_RESP_CODE_200, resp.http_resp_code);
    ASSERT_EQ(HTTP_CONTENT_LOCATION_HEAP, resp.content_location);
    ASSERT_TRUE(resp.flag_no_cache);
    ASSERT_EQ(HTTP_CONTENT_TYPE_APPLICATION_JSON, resp.content_type);
    ASSERT_EQ(nullptr, resp.p_content_type_param);
    ASSERT_NE(nullptr, resp.select_location.heap.p_buf);
    ASSERT_EQ(string(expected_json), string(reinterpret_cast<const char*>(resp.select_location.heap.p_buf)));
    ASSERT_EQ(strlen(expected_json), resp.content_len);
    ASSERT_EQ(HTTP_CONTENT_ENCODING_NONE, resp.content_encoding);
    ASSERT_EQ(
        "D http_server: http_server_cb_on_get /validate_url?validate_type=check_fw_update_url&url=https://network.ruuvi.com/firmwareupdate&auth_type=none\n"
        "I http_server: http_server_cb_on_get: Clear all saved TLS session tickets\n"
        "D http_server: HTTP params: auth_type=none\n"
        "D http_server: HTTP params: key 'auth_type=': value (encoded): none\n"
        "D http_server: HTTP params: key 'auth_type=': value (decoded): none\n"
        "D http_server: HTTP params: url=https://network.ruuvi.com/firmwareupdate\n"
        "D http_server: HTTP params: key 'url=': value (encoded): https://network.ruuvi.com/firmwareupdate\n"
        "D http_server: HTTP params: key 'url=': value (decoded): https://network.ruuvi.com/firmwareupdate\n"
        "D http_server: Can't find key 'user=' in URL params\n"
        "E http_server: HTTP params: Can't find 'user='\n"
        "D http_server: Can't find key 'encrypted_password=' in URL params\n"
        "E http_server: HTTP params: Can't find 'encrypted_password='\n"
        "D http_server: Can't find key 'use_saved_password=' in URL params\n"
        "E http_server: HTTP params: Can't find 'use_saved_password='\n"
        "E http_server: Can't find key: use_saved_password=\n"
        "E http_server: Can't find prefix 'use_ssl_client_cert='\n"
        "E http_server: Can't find prefix 'use_ssl_server_cert='\n"
        "D http_server: Can't find prefix 'use_extra_http_path='\n"
        "D http_server: Can't find prefix 'use_extra_http_query='\n"
        "D http_server: Can't find prefix 'use_extra_http_headers='\n"
        "D http_server: Got URL from params: https://network.ruuvi.com/firmwareupdate\n"
        "I http_server: Found validate_type: check_fw_update_url\n"
        "I http_download: cb_on_http_download_json_data: buf_size=" + std::to_string(strlen(p_firmwareUpdateJson)) + "\n"
        "I http_download: http_download_json: completed within 0 ticks\n"
        "I http_server: Firmware update info (json): " + string(p_firmwareUpdateJson) + "\n"
        "I http_download: http_check: completed within 0 ticks\n"
        "D http_server: Feed watchdog\n"
        "I http_download: http_check: completed within 0 ticks\n"
        "D http_server: Feed watchdog\n"
        "I http_download: http_check: completed within 0 ticks\n"
        "D http_server: Feed watchdog\n",
        esp_log_wrapper_get_logs());
    http_server_resp_free(&resp);
    os_malloc_trace_dump();
    ESP_LOG_WRAPPER_TEST_CHECK_LOG_RECORD("MEM_TRACE", ESP_LOG_INFO, "Num blocks allocated: 0");
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_EQ(0, this->m_alloc_free_call_count);
}

TEST_F(TestHttpServerCb, http_server_cb_on_get_validate_url_check_fw_update_url__json_with_beta_no_version) // NOLINT
{
    const char* const p_firmwareUpdateJson
        = "{"
          "  \"latest\": {"
          "    \"version\": \"v1.14.3\","
          "    \"url\": \"https://fwupdate.ruuvi.com/v1.14.3\","
          "    \"created_at\": \"2023-10-06T11:26:07Z\""
          "  },"
          "  \"beta\": {"
          "    \"url\": \"https://github.com/ruuvi/ruuvi.gateway_esp.c/releases/download/v1.14.2\","
          "    \"created_at\": \"2023-09-19T11:16:48Z\""
          "  }"
          "}";
    this->m_firmware_update_resp             = str_buf_printf_with_alloc("%s", p_firmwareUpdateJson);
    const char* arr_of_binaries[]            = { "https://fwupdate.ruuvi.com/v1.14.3/ruuvi_gateway_esp.bin",
                                                 "https://fwupdate.ruuvi.com/v1.14.3/fatfs_gwui.bin",
                                                 "https://fwupdate.ruuvi.com/v1.14.3/fatfs_nrf52.bin" };
    this->m_http_check_with_auth_arr_of_urls = arr_of_binaries;
    this->m_http_check_with_auth_num_of_urls = sizeof(arr_of_binaries) / sizeof(arr_of_binaries[0]);

    const char* const expected_json
        = "{\"status\": 200, \"json\": {  \"latest\": {    \"version\": \"v1.14.3\",    \"url\": "
          "\"https://fwupdate.ruuvi.com/v1.14.3\",    \"created_at\": \"2023-10-06T11:26:07Z\"  },  \"beta\": {    "
          "\"url\": \"https://github.com/ruuvi/ruuvi.gateway_esp.c/releases/download/v1.14.2\",    \"created_at\": "
          "\"2023-09-19T11:16:48Z\"  }}}";

    const char* const p_uri_params
        = "validate_type=check_fw_update_url&url=https://network.ruuvi.com/firmwareupdate&auth_type=none";

    http_server_resp_t resp = http_server_cb_on_get("validate_url", p_uri_params, true, nullptr);

    ASSERT_EQ(HTTP_RESP_CODE_200, resp.http_resp_code);
    ASSERT_EQ(HTTP_CONTENT_LOCATION_HEAP, resp.content_location);
    ASSERT_TRUE(resp.flag_no_cache);
    ASSERT_EQ(HTTP_CONTENT_TYPE_APPLICATION_JSON, resp.content_type);
    ASSERT_EQ(nullptr, resp.p_content_type_param);
    ASSERT_NE(nullptr, resp.select_location.heap.p_buf);
    ASSERT_EQ(string(expected_json), string(reinterpret_cast<const char*>(resp.select_location.heap.p_buf)));
    ASSERT_EQ(strlen(expected_json), resp.content_len);
    ASSERT_EQ(HTTP_CONTENT_ENCODING_NONE, resp.content_encoding);
    ASSERT_EQ(
        "D http_server: http_server_cb_on_get /validate_url?validate_type=check_fw_update_url&url=https://network.ruuvi.com/firmwareupdate&auth_type=none\n"
        "I http_server: http_server_cb_on_get: Clear all saved TLS session tickets\n"
        "D http_server: HTTP params: auth_type=none\n"
        "D http_server: HTTP params: key 'auth_type=': value (encoded): none\n"
        "D http_server: HTTP params: key 'auth_type=': value (decoded): none\n"
        "D http_server: HTTP params: url=https://network.ruuvi.com/firmwareupdate\n"
        "D http_server: HTTP params: key 'url=': value (encoded): https://network.ruuvi.com/firmwareupdate\n"
        "D http_server: HTTP params: key 'url=': value (decoded): https://network.ruuvi.com/firmwareupdate\n"
        "D http_server: Can't find key 'user=' in URL params\n"
        "E http_server: HTTP params: Can't find 'user='\n"
        "D http_server: Can't find key 'encrypted_password=' in URL params\n"
        "E http_server: HTTP params: Can't find 'encrypted_password='\n"
        "D http_server: Can't find key 'use_saved_password=' in URL params\n"
        "E http_server: HTTP params: Can't find 'use_saved_password='\n"
        "E http_server: Can't find key: use_saved_password=\n"
        "E http_server: Can't find prefix 'use_ssl_client_cert='\n"
        "E http_server: Can't find prefix 'use_ssl_server_cert='\n"
        "D http_server: Can't find prefix 'use_extra_http_path='\n"
        "D http_server: Can't find prefix 'use_extra_http_query='\n"
        "D http_server: Can't find prefix 'use_extra_http_headers='\n"
        "D http_server: Got URL from params: https://network.ruuvi.com/firmwareupdate\n"
        "I http_server: Found validate_type: check_fw_update_url\n"
        "I http_download: cb_on_http_download_json_data: buf_size=" + std::to_string(strlen(p_firmwareUpdateJson)) + "\n"
        "I http_download: http_download_json: completed within 0 ticks\n"
        "I http_server: Firmware update info (json): " + string(p_firmwareUpdateJson) + "\n"
        "I http_download: http_check: completed within 0 ticks\n"
        "D http_server: Feed watchdog\n"
        "I http_download: http_check: completed within 0 ticks\n"
        "D http_server: Feed watchdog\n"
        "I http_download: http_check: completed within 0 ticks\n"
        "D http_server: Feed watchdog\n",
        esp_log_wrapper_get_logs());
    http_server_resp_free(&resp);
    os_malloc_trace_dump();
    ESP_LOG_WRAPPER_TEST_CHECK_LOG_RECORD("MEM_TRACE", ESP_LOG_INFO, "Num blocks allocated: 0");
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_EQ(0, this->m_alloc_free_call_count);
}

TEST_F(TestHttpServerCb, http_server_cb_on_get_validate_url_check_fw_update_url__json_with_beta_empty_url) // NOLINT
{
    const char* const p_firmwareUpdateJson
        = "{"
          "  \"latest\": {"
          "    \"version\": \"v1.14.3\","
          "    \"url\": \"https://fwupdate.ruuvi.com/v1.14.3\","
          "    \"created_at\": \"2023-10-06T11:26:07Z\""
          "  },"
          "  \"beta\": {"
          "    \"version\": \"v1.14.2\","
          "    \"url\": \"\","
          "    \"created_at\": \"2023-09-19T11:16:48Z\""
          "  }"
          "}";
    this->m_firmware_update_resp             = str_buf_printf_with_alloc("%s", p_firmwareUpdateJson);
    const char* arr_of_binaries[]            = { "https://fwupdate.ruuvi.com/v1.14.3/ruuvi_gateway_esp.bin",
                                                 "https://fwupdate.ruuvi.com/v1.14.3/fatfs_gwui.bin",
                                                 "https://fwupdate.ruuvi.com/v1.14.3/fatfs_nrf52.bin" };
    this->m_http_check_with_auth_arr_of_urls = arr_of_binaries;
    this->m_http_check_with_auth_num_of_urls = sizeof(arr_of_binaries) / sizeof(arr_of_binaries[0]);

    const char* const expected_json
        = "{\"status\": 200, \"json\": {  \"latest\": {    \"version\": \"v1.14.3\",    \"url\": "
          "\"https://fwupdate.ruuvi.com/v1.14.3\",    \"created_at\": \"2023-10-06T11:26:07Z\"  },  \"beta\": {    "
          "\"version\": \"v1.14.2\",    \"url\": \"\",    \"created_at\": "
          "\"2023-09-19T11:16:48Z\"  }}}";

    const char* const p_uri_params
        = "validate_type=check_fw_update_url&url=https://network.ruuvi.com/firmwareupdate&auth_type=none";

    http_server_resp_t resp = http_server_cb_on_get("validate_url", p_uri_params, true, nullptr);

    ASSERT_EQ(HTTP_RESP_CODE_200, resp.http_resp_code);
    ASSERT_EQ(HTTP_CONTENT_LOCATION_HEAP, resp.content_location);
    ASSERT_TRUE(resp.flag_no_cache);
    ASSERT_EQ(HTTP_CONTENT_TYPE_APPLICATION_JSON, resp.content_type);
    ASSERT_EQ(nullptr, resp.p_content_type_param);
    ASSERT_NE(nullptr, resp.select_location.heap.p_buf);
    ASSERT_EQ(string(expected_json), string(reinterpret_cast<const char*>(resp.select_location.heap.p_buf)));
    ASSERT_EQ(strlen(expected_json), resp.content_len);
    ASSERT_EQ(HTTP_CONTENT_ENCODING_NONE, resp.content_encoding);
    ASSERT_EQ(
        "D http_server: http_server_cb_on_get /validate_url?validate_type=check_fw_update_url&url=https://network.ruuvi.com/firmwareupdate&auth_type=none\n"
        "I http_server: http_server_cb_on_get: Clear all saved TLS session tickets\n"
        "D http_server: HTTP params: auth_type=none\n"
        "D http_server: HTTP params: key 'auth_type=': value (encoded): none\n"
        "D http_server: HTTP params: key 'auth_type=': value (decoded): none\n"
        "D http_server: HTTP params: url=https://network.ruuvi.com/firmwareupdate\n"
        "D http_server: HTTP params: key 'url=': value (encoded): https://network.ruuvi.com/firmwareupdate\n"
        "D http_server: HTTP params: key 'url=': value (decoded): https://network.ruuvi.com/firmwareupdate\n"
        "D http_server: Can't find key 'user=' in URL params\n"
        "E http_server: HTTP params: Can't find 'user='\n"
        "D http_server: Can't find key 'encrypted_password=' in URL params\n"
        "E http_server: HTTP params: Can't find 'encrypted_password='\n"
        "D http_server: Can't find key 'use_saved_password=' in URL params\n"
        "E http_server: HTTP params: Can't find 'use_saved_password='\n"
        "E http_server: Can't find key: use_saved_password=\n"
        "E http_server: Can't find prefix 'use_ssl_client_cert='\n"
        "E http_server: Can't find prefix 'use_ssl_server_cert='\n"
        "D http_server: Can't find prefix 'use_extra_http_path='\n"
        "D http_server: Can't find prefix 'use_extra_http_query='\n"
        "D http_server: Can't find prefix 'use_extra_http_headers='\n"
        "D http_server: Got URL from params: https://network.ruuvi.com/firmwareupdate\n"
        "I http_server: Found validate_type: check_fw_update_url\n"
        "I http_download: cb_on_http_download_json_data: buf_size=" + std::to_string(strlen(p_firmwareUpdateJson)) + "\n"
            "I http_download: http_download_json: completed within 0 ticks\n"
            "I http_server: Firmware update info (json): " + string(p_firmwareUpdateJson) + "\n"
            "I http_download: http_check: completed within 0 ticks\n"
            "D http_server: Feed watchdog\n"
            "I http_download: http_check: completed within 0 ticks\n"
            "D http_server: Feed watchdog\n"
            "I http_download: http_check: completed within 0 ticks\n"
            "D http_server: Feed watchdog\n",
        esp_log_wrapper_get_logs());
    http_server_resp_free(&resp);
    os_malloc_trace_dump();
    ESP_LOG_WRAPPER_TEST_CHECK_LOG_RECORD("MEM_TRACE", ESP_LOG_INFO, "Num blocks allocated: 0");
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_EQ(0, this->m_alloc_free_call_count);
}

TEST_F(TestHttpServerCb, http_server_cb_on_get_validate_url_check_fw_update_url__json_with_beta_invalid_url) // NOLINT
{
    const char* const p_firmwareUpdateJson
        = "{"
          "  \"latest\": {"
          "    \"version\": \"v1.14.3\","
          "    \"url\": \"https://fwupdate.ruuvi.com/v1.14.3\","
          "    \"created_at\": \"2023-10-06T11:26:07Z\""
          "  },"
          "  \"beta\": {"
          "    \"version\": \"v1.14.2\","
          "    \"url\": \"qqq://abc.com\","
          "    \"created_at\": \"2023-09-19T11:16:48Z\""
          "  }"
          "}";
    this->m_firmware_update_resp             = str_buf_printf_with_alloc("%s", p_firmwareUpdateJson);
    const char* arr_of_binaries[]            = { "https://fwupdate.ruuvi.com/v1.14.3/ruuvi_gateway_esp.bin",
                                                 "https://fwupdate.ruuvi.com/v1.14.3/fatfs_gwui.bin",
                                                 "https://fwupdate.ruuvi.com/v1.14.3/fatfs_nrf52.bin" };
    this->m_http_check_with_auth_arr_of_urls = arr_of_binaries;
    this->m_http_check_with_auth_num_of_urls = sizeof(arr_of_binaries) / sizeof(arr_of_binaries[0]);

    const char* const expected_json
        = "{\"status\": 200, \"json\": {  \"latest\": {    \"version\": \"v1.14.3\",    \"url\": "
          "\"https://fwupdate.ruuvi.com/v1.14.3\",    \"created_at\": \"2023-10-06T11:26:07Z\"  },  \"beta\": {    "
          "\"version\": \"v1.14.2\",    \"url\": \"qqq://abc.com\",    \"created_at\": "
          "\"2023-09-19T11:16:48Z\"  }}}";

    const char* const p_uri_params
        = "validate_type=check_fw_update_url&url=https://network.ruuvi.com/firmwareupdate&auth_type=none";

    http_server_resp_t resp = http_server_cb_on_get("validate_url", p_uri_params, true, nullptr);

    ASSERT_EQ(HTTP_RESP_CODE_200, resp.http_resp_code);
    ASSERT_EQ(HTTP_CONTENT_LOCATION_HEAP, resp.content_location);
    ASSERT_TRUE(resp.flag_no_cache);
    ASSERT_EQ(HTTP_CONTENT_TYPE_APPLICATION_JSON, resp.content_type);
    ASSERT_EQ(nullptr, resp.p_content_type_param);
    ASSERT_NE(nullptr, resp.select_location.heap.p_buf);
    ASSERT_EQ(string(expected_json), string(reinterpret_cast<const char*>(resp.select_location.heap.p_buf)));
    ASSERT_EQ(strlen(expected_json), resp.content_len);
    ASSERT_EQ(HTTP_CONTENT_ENCODING_NONE, resp.content_encoding);
    ASSERT_EQ(
        "D http_server: http_server_cb_on_get /validate_url?validate_type=check_fw_update_url&url=https://network.ruuvi.com/firmwareupdate&auth_type=none\n"
        "I http_server: http_server_cb_on_get: Clear all saved TLS session tickets\n"
        "D http_server: HTTP params: auth_type=none\n"
        "D http_server: HTTP params: key 'auth_type=': value (encoded): none\n"
        "D http_server: HTTP params: key 'auth_type=': value (decoded): none\n"
        "D http_server: HTTP params: url=https://network.ruuvi.com/firmwareupdate\n"
        "D http_server: HTTP params: key 'url=': value (encoded): https://network.ruuvi.com/firmwareupdate\n"
        "D http_server: HTTP params: key 'url=': value (decoded): https://network.ruuvi.com/firmwareupdate\n"
        "D http_server: Can't find key 'user=' in URL params\n"
        "E http_server: HTTP params: Can't find 'user='\n"
        "D http_server: Can't find key 'encrypted_password=' in URL params\n"
        "E http_server: HTTP params: Can't find 'encrypted_password='\n"
        "D http_server: Can't find key 'use_saved_password=' in URL params\n"
        "E http_server: HTTP params: Can't find 'use_saved_password='\n"
        "E http_server: Can't find key: use_saved_password=\n"
        "E http_server: Can't find prefix 'use_ssl_client_cert='\n"
        "E http_server: Can't find prefix 'use_ssl_server_cert='\n"
        "D http_server: Can't find prefix 'use_extra_http_path='\n"
        "D http_server: Can't find prefix 'use_extra_http_query='\n"
        "D http_server: Can't find prefix 'use_extra_http_headers='\n"
        "D http_server: Got URL from params: https://network.ruuvi.com/firmwareupdate\n"
        "I http_server: Found validate_type: check_fw_update_url\n"
        "I http_download: cb_on_http_download_json_data: buf_size=" + std::to_string(strlen(p_firmwareUpdateJson)) + "\n"
        "I http_download: http_download_json: completed within 0 ticks\n"
        "I http_server: Firmware update info (json): " + string(p_firmwareUpdateJson) + "\n"
        "I http_download: http_check: completed within 0 ticks\n"
        "D http_server: Feed watchdog\n"
        "I http_download: http_check: completed within 0 ticks\n"
        "D http_server: Feed watchdog\n"
        "I http_download: http_check: completed within 0 ticks\n"
        "D http_server: Feed watchdog\n",
        esp_log_wrapper_get_logs());
    http_server_resp_free(&resp);
    os_malloc_trace_dump();
    ESP_LOG_WRAPPER_TEST_CHECK_LOG_RECORD("MEM_TRACE", ESP_LOG_INFO, "Num blocks allocated: 0");
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_EQ(0, this->m_alloc_free_call_count);
}

TEST_F(TestHttpServerCb, http_server_cb_on_get_validate_url_check_fw_update_url__json_with_beta_no_url) // NOLINT
{
    const char* const p_firmwareUpdateJson
        = "{"
          "  \"latest\": {"
          "    \"version\": \"v1.14.3\","
          "    \"url\": \"https://fwupdate.ruuvi.com/v1.14.3\","
          "    \"created_at\": \"2023-10-06T11:26:07Z\""
          "  },"
          "  \"beta\": {"
          "    \"version\": \"v1.14.2\","
          "    \"created_at\": \"2023-09-19T11:16:48Z\""
          "  }"
          "}";
    this->m_firmware_update_resp             = str_buf_printf_with_alloc("%s", p_firmwareUpdateJson);
    const char* arr_of_binaries[]            = { "https://fwupdate.ruuvi.com/v1.14.3/ruuvi_gateway_esp.bin",
                                                 "https://fwupdate.ruuvi.com/v1.14.3/fatfs_gwui.bin",
                                                 "https://fwupdate.ruuvi.com/v1.14.3/fatfs_nrf52.bin" };
    this->m_http_check_with_auth_arr_of_urls = arr_of_binaries;
    this->m_http_check_with_auth_num_of_urls = sizeof(arr_of_binaries) / sizeof(arr_of_binaries[0]);

    const char* const expected_json
        = "{\"status\": 200, \"json\": {  \"latest\": {    \"version\": \"v1.14.3\",    \"url\": "
          "\"https://fwupdate.ruuvi.com/v1.14.3\",    \"created_at\": \"2023-10-06T11:26:07Z\"  },  \"beta\": {    "
          "\"version\": \"v1.14.2\",    \"created_at\": \"2023-09-19T11:16:48Z\"  }}}";

    const char* const p_uri_params
        = "validate_type=check_fw_update_url&url=https://network.ruuvi.com/firmwareupdate&auth_type=none";

    http_server_resp_t resp = http_server_cb_on_get("validate_url", p_uri_params, true, nullptr);

    ASSERT_EQ(HTTP_RESP_CODE_200, resp.http_resp_code);
    ASSERT_EQ(HTTP_CONTENT_LOCATION_HEAP, resp.content_location);
    ASSERT_TRUE(resp.flag_no_cache);
    ASSERT_EQ(HTTP_CONTENT_TYPE_APPLICATION_JSON, resp.content_type);
    ASSERT_EQ(nullptr, resp.p_content_type_param);
    ASSERT_NE(nullptr, resp.select_location.heap.p_buf);
    ASSERT_EQ(string(expected_json), string(reinterpret_cast<const char*>(resp.select_location.heap.p_buf)));
    ASSERT_EQ(strlen(expected_json), resp.content_len);
    ASSERT_EQ(HTTP_CONTENT_ENCODING_NONE, resp.content_encoding);
    ASSERT_EQ(
        "D http_server: http_server_cb_on_get /validate_url?validate_type=check_fw_update_url&url=https://network.ruuvi.com/firmwareupdate&auth_type=none\n"
        "I http_server: http_server_cb_on_get: Clear all saved TLS session tickets\n"
        "D http_server: HTTP params: auth_type=none\n"
        "D http_server: HTTP params: key 'auth_type=': value (encoded): none\n"
        "D http_server: HTTP params: key 'auth_type=': value (decoded): none\n"
        "D http_server: HTTP params: url=https://network.ruuvi.com/firmwareupdate\n"
        "D http_server: HTTP params: key 'url=': value (encoded): https://network.ruuvi.com/firmwareupdate\n"
        "D http_server: HTTP params: key 'url=': value (decoded): https://network.ruuvi.com/firmwareupdate\n"
        "D http_server: Can't find key 'user=' in URL params\n"
        "E http_server: HTTP params: Can't find 'user='\n"
        "D http_server: Can't find key 'encrypted_password=' in URL params\n"
        "E http_server: HTTP params: Can't find 'encrypted_password='\n"
        "D http_server: Can't find key 'use_saved_password=' in URL params\n"
        "E http_server: HTTP params: Can't find 'use_saved_password='\n"
        "E http_server: Can't find key: use_saved_password=\n"
        "E http_server: Can't find prefix 'use_ssl_client_cert='\n"
        "E http_server: Can't find prefix 'use_ssl_server_cert='\n"
        "D http_server: Can't find prefix 'use_extra_http_path='\n"
        "D http_server: Can't find prefix 'use_extra_http_query='\n"
        "D http_server: Can't find prefix 'use_extra_http_headers='\n"
        "D http_server: Got URL from params: https://network.ruuvi.com/firmwareupdate\n"
        "I http_server: Found validate_type: check_fw_update_url\n"
        "I http_download: cb_on_http_download_json_data: buf_size=" + std::to_string(strlen(p_firmwareUpdateJson)) + "\n"
        "I http_download: http_download_json: completed within 0 ticks\n"
        "I http_server: Firmware update info (json): " + string(p_firmwareUpdateJson) + "\n"
        "I http_download: http_check: completed within 0 ticks\n"
        "D http_server: Feed watchdog\n"
        "I http_download: http_check: completed within 0 ticks\n"
        "D http_server: Feed watchdog\n"
        "I http_download: http_check: completed within 0 ticks\n"
        "D http_server: Feed watchdog\n",
        esp_log_wrapper_get_logs());
    http_server_resp_free(&resp);
    os_malloc_trace_dump();
    ESP_LOG_WRAPPER_TEST_CHECK_LOG_RECORD("MEM_TRACE", ESP_LOG_INFO, "Num blocks allocated: 0");
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_EQ(0, this->m_alloc_free_call_count);
}

TEST_F(TestHttpServerCb, http_server_cb_on_get_validate_url_check_post_advs__custom_http) // NOLINT
{
    const char* const expected_json = "{\n\t\"status\":\t200,\n\t\"message\":\t\"{}\"\n}";

    const char* const p_uri_params
        = "validate_type=check_post_advs"
          "&url=https://myserver.com/"
          "&auth_type=none"
          "&use_ssl_client_cert=false"
          "&use_ssl_server_cert=false"
          "&use_extra_http_path=false"
          "&use_extra_http_query=false"
          "&use_extra_http_headers=false";

    http_server_resp_t resp = http_server_cb_on_get("validate_url", p_uri_params, true, nullptr);

    ASSERT_EQ(HTTP_RESP_CODE_200, resp.http_resp_code);
    ASSERT_EQ(HTTP_CONTENT_LOCATION_HEAP, resp.content_location);
    ASSERT_TRUE(resp.flag_no_cache);
    ASSERT_EQ(HTTP_CONTENT_TYPE_APPLICATION_JSON, resp.content_type);
    ASSERT_EQ(nullptr, resp.p_content_type_param);
    ASSERT_NE(nullptr, resp.select_location.heap.p_buf);
    ASSERT_EQ(string(expected_json), string(reinterpret_cast<const char*>(resp.select_location.heap.p_buf)));
    ASSERT_EQ(HTTP_CONTENT_ENCODING_NONE, resp.content_encoding);

    ASSERT_EQ(
        "D http_server: http_server_cb_on_get "
        "/validate_url?validate_type=check_post_advs"
        "&url=https://myserver.com/"
        "&auth_type=none&"
        "use_ssl_client_cert=false"
        "&use_ssl_server_cert=false"
        "&use_extra_http_path=false"
        "&use_extra_http_query=false"
        "&use_extra_http_headers=false\n"
        "I http_server: http_server_cb_on_get: Clear all saved TLS session tickets\n"
        "D http_server: HTTP params: auth_type=none\n"
        "D http_server: HTTP params: key 'auth_type=': value (encoded): none\n"
        "D http_server: HTTP params: key 'auth_type=': value (decoded): none\n"
        "D http_server: HTTP params: url=https://myserver.com/\n"
        "D http_server: HTTP params: key 'url=': value (encoded): https://myserver.com/\n"
        "D http_server: HTTP params: key 'url=': value (decoded): https://myserver.com/\n"
        "D http_server: Can't find key 'user=' in URL params\n"
        "E http_server: HTTP params: Can't find 'user='\n"
        "D http_server: Can't find key 'encrypted_password=' in URL params\n"
        "E http_server: HTTP params: Can't find 'encrypted_password='\n"
        "D http_server: Can't find key 'use_saved_password=' in URL params\n"
        "E http_server: HTTP params: Can't find 'use_saved_password='\n"
        "E http_server: Can't find key: use_saved_password=\n"
        "I http_server: Found use_ssl_client_cert: false\n"
        "I http_server: Found use_ssl_server_cert: false\n"
        "I http_server: Found use_extra_http_path: false\n"
        "I http_server: Found use_extra_http_query: false\n"
        "I http_server: Found use_extra_http_headers: false\n"
        "D http_server: Got URL from params: https://myserver.com/\n"
        "I http_server: Found validate_type: check_post_advs\n"
        "I http_server: Validate URL (POST advs): https://myserver.com/\n"
        "I http_server: Validate URL (POST advs): auth_type=none\n"
        "I http_server: Validate URL (POST advs): user=NULL\n"
        "I http_server: Validate URL (POST advs): use_saved_password=0\n"
        "D http_server: Validate URL (POST advs): saved_password=\n"
        "D http_server: Validate URL (POST advs): password=NULL\n"
        "I http_server: Validate URL (POST advs): use_ssl_client_cert=0\n"
        "I http_server: Validate URL (POST advs): use_ssl_server_cert=0\n"
        "I http_server: Validate URL (POST advs): use_extra_http_path=0\n"
        "I http_server: Validate URL (POST advs): use_extra_http_query=0\n"
        "I http_server: Validate URL (POST advs): use_extra_http_headers=0\n",
        esp_log_wrapper_get_logs());
    http_server_resp_free(&resp);
    os_malloc_trace_dump();
    ESP_LOG_WRAPPER_TEST_CHECK_LOG_RECORD("MEM_TRACE", ESP_LOG_INFO, "Num blocks allocated: 0");
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_EQ(0, this->m_alloc_free_call_count);
}

TEST_F(
    TestHttpServerCb,
    http_server_cb_on_get_validate_url_check_post_advs__custom_http_with_extra_path_query_and_headers) // NOLINT
{
    const char* const expected_json = "{\n\t\"status\":\t200,\n\t\"message\":\t\"{}\"\n}";

    const char* const p_uri_params
        = "validate_type=check_post_advs"
          "&url=https://myserver.com:8000"
          "&auth_type=none"
          "&use_ssl_client_cert=false"
          "&use_ssl_server_cert=false"
          "&use_extra_http_path=true"
          "&use_extra_http_query=true"
          "&use_extra_http_headers=true";

    http_server_resp_t resp = http_server_cb_on_get("validate_url", p_uri_params, true, nullptr);

    ASSERT_EQ(HTTP_RESP_CODE_200, resp.http_resp_code);
    ASSERT_EQ(HTTP_CONTENT_LOCATION_HEAP, resp.content_location);
    ASSERT_TRUE(resp.flag_no_cache);
    ASSERT_EQ(HTTP_CONTENT_TYPE_APPLICATION_JSON, resp.content_type);
    ASSERT_EQ(nullptr, resp.p_content_type_param);
    ASSERT_NE(nullptr, resp.select_location.heap.p_buf);
    ASSERT_EQ(string(expected_json), string(reinterpret_cast<const char*>(resp.select_location.heap.p_buf)));
    ASSERT_EQ(HTTP_CONTENT_ENCODING_NONE, resp.content_encoding);

    ASSERT_EQ(
        "D http_server: http_server_cb_on_get "
        "/validate_url"
        "?validate_type=check_post_advs"
        "&url=https://myserver.com:8000"
        "&auth_type=none&"
        "use_ssl_client_cert=false"
        "&use_ssl_server_cert=false"
        "&use_extra_http_path=true"
        "&use_extra_http_query=true"
        "&use_extra_http_headers=true\n"
        "I http_server: http_server_cb_on_get: Clear all saved TLS session tickets\n"
        "D http_server: HTTP params: auth_type=none\n"
        "D http_server: HTTP params: key 'auth_type=': value (encoded): none\n"
        "D http_server: HTTP params: key 'auth_type=': value (decoded): none\n"
        "D http_server: HTTP params: url=https://myserver.com:8000\n"
        "D http_server: HTTP params: key 'url=': value (encoded): https://myserver.com:8000\n"
        "D http_server: HTTP params: key 'url=': value (decoded): https://myserver.com:8000\n"
        "D http_server: Can't find key 'user=' in URL params\n"
        "E http_server: HTTP params: Can't find 'user='\n"
        "D http_server: Can't find key 'encrypted_password=' in URL params\n"
        "E http_server: HTTP params: Can't find 'encrypted_password='\n"
        "D http_server: Can't find key 'use_saved_password=' in URL params\n"
        "E http_server: HTTP params: Can't find 'use_saved_password='\n"
        "E http_server: Can't find key: use_saved_password=\n"
        "I http_server: Found use_ssl_client_cert: false\n"
        "I http_server: Found use_ssl_server_cert: false\n"
        "I http_server: Found use_extra_http_path: true\n"
        "I http_server: Found use_extra_http_query: true\n"
        "I http_server: Found use_extra_http_headers: true\n"
        "D http_server: Got URL from params: https://myserver.com:8000\n"
        "I http_server: Found validate_type: check_post_advs\n"
        "I http_server: Validate URL (POST advs): https://myserver.com:8000\n"
        "I http_server: Validate URL (POST advs): auth_type=none\n"
        "I http_server: Validate URL (POST advs): user=NULL\n"
        "I http_server: Validate URL (POST advs): use_saved_password=0\n"
        "D http_server: Validate URL (POST advs): saved_password=\n"
        "D http_server: Validate URL (POST advs): password=NULL\n"
        "I http_server: Validate URL (POST advs): use_ssl_client_cert=0\n"
        "I http_server: Validate URL (POST advs): use_ssl_server_cert=0\n"
        "I http_server: Validate URL (POST advs): use_extra_http_path=1\n"
        "I http_server: Validate URL (POST advs): use_extra_http_query=1\n"
        "I http_server: Validate URL (POST advs): use_extra_http_headers=1\n",
        esp_log_wrapper_get_logs());
    http_server_resp_free(&resp);
    os_malloc_trace_dump();
    ESP_LOG_WRAPPER_TEST_CHECK_LOG_RECORD("MEM_TRACE", ESP_LOG_INFO, "Num blocks allocated: 0");
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_EQ(0, this->m_alloc_free_call_count);
}

TEST_F(TestHttpServerCb, test_http_server_cb_on_user_req__update_cycle_regular__latest_only) // NOLINT
{
    {
        gw_cfg_t gw_cfg = { 0 };
        gw_cfg_default_get(&gw_cfg);
        gw_cfg.ruuvi_cfg.auto_update.auto_update_cycle = AUTO_UPDATE_CYCLE_TYPE_REGULAR;
        gw_cfg_update(&gw_cfg);
        esp_log_wrapper_clear();
    }

    const char* const p_firmwareUpdateJson
        = "{"
          "  \"latest\": {"
          "    \"version\": \"v1.14.3\","
          "    \"url\": \"https://fwupdate.ruuvi.com/v1.14.3\","
          "    \"created_at\": \"2023-10-06T11:26:07Z\""
          "  }"
          "}";
    this->m_firmware_update_resp             = str_buf_printf_with_alloc("%s", p_firmwareUpdateJson);
    const char* arr_of_binaries[]            = { "https://fwupdate.ruuvi.com/v1.14.3/ruuvi_gateway_esp.bin",
                                                 "https://fwupdate.ruuvi.com/v1.14.3/fatfs_gwui.bin",
                                                 "https://fwupdate.ruuvi.com/v1.14.3/fatfs_nrf52.bin" };
    this->m_http_check_with_auth_arr_of_urls = arr_of_binaries;
    this->m_http_check_with_auth_num_of_urls = sizeof(arr_of_binaries) / sizeof(arr_of_binaries[0]);

    http_server_cb_on_user_req(HTTP_SERVER_USER_REQ_CODE_DOWNLOAD_LATEST_RELEASE_INFO);

    ASSERT_EQ(FW_UPDATE_REASON_AUTO, this->m_fw_updating_reason);
    ASSERT_EQ(string("https://fwupdate.ruuvi.com/v1.14.3"), fw_update_get_binaries_url());

    ASSERT_EQ(
        "I http_server: Download firmware update info\n"
        "I http_download: cb_on_http_download_json_data: buf_size=" + std::to_string(strlen(p_firmwareUpdateJson)) + "\n"
        "I http_download: http_download_json: completed within 0 ticks\n"
        "I http_server: Firmware update info (json): " + string(p_firmwareUpdateJson) + "\n"
        "I http_server: Update is required (current version: v1.3.3, latest version: https://fwupdate.ruuvi.com/v1.14.3)\n"
        "I http_download: http_check: completed within 0 ticks\n"
        "D http_server: Feed watchdog\n"
        "I http_download: http_check: completed within 0 ticks\n"
        "D http_server: Feed watchdog\n"
        "I http_download: http_check: completed within 0 ticks\n"
        "D http_server: Feed watchdog\n"
        "I http_server: Run firmware auto-updating from URL: https://fwupdate.ruuvi.com/v1.14.3\n",
        esp_log_wrapper_get_logs());
    os_malloc_trace_dump();
    ESP_LOG_WRAPPER_TEST_CHECK_LOG_RECORD("MEM_TRACE", ESP_LOG_INFO, "Num blocks allocated: 0");
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_EQ(0, this->m_alloc_free_call_count);
}

TEST_F(TestHttpServerCb, test_http_server_cb_on_user_req__update_cycle_regular__latest_and_beta) // NOLINT
{
    {
        gw_cfg_t gw_cfg = { 0 };
        gw_cfg_default_get(&gw_cfg);
        gw_cfg.ruuvi_cfg.auto_update.auto_update_cycle = AUTO_UPDATE_CYCLE_TYPE_REGULAR;
        gw_cfg_update(&gw_cfg);
        esp_log_wrapper_clear();
    }

    const char* const p_firmwareUpdateJson
        = "{"
          "  \"latest\": {"
          "    \"version\": \"v1.14.3\","
          "    \"url\": \"https://fwupdate.ruuvi.com/v1.14.3\","
          "    \"created_at\": \"2023-10-06T11:26:07Z\""
          "  },"
          "  \"beta\": {"
          "    \"version\": \"v1.14.4\","
          "    \"url\": \"https://github.com/ruuvi/ruuvi.gateway_esp.c/releases/download/v1.14.4\","
          "    \"created_at\": \"2023-09-19T11:16:48Z\""
          "  }"
          "}";
    this->m_firmware_update_resp             = str_buf_printf_with_alloc("%s", p_firmwareUpdateJson);
    const char* arr_of_binaries[]            = { "https://fwupdate.ruuvi.com/v1.14.3/ruuvi_gateway_esp.bin",
                                                 "https://fwupdate.ruuvi.com/v1.14.3/fatfs_gwui.bin",
                                                 "https://fwupdate.ruuvi.com/v1.14.3/fatfs_nrf52.bin" };
    this->m_http_check_with_auth_arr_of_urls = arr_of_binaries;
    this->m_http_check_with_auth_num_of_urls = sizeof(arr_of_binaries) / sizeof(arr_of_binaries[0]);

    http_server_cb_on_user_req(HTTP_SERVER_USER_REQ_CODE_DOWNLOAD_LATEST_RELEASE_INFO);

    ASSERT_EQ(FW_UPDATE_REASON_AUTO, this->m_fw_updating_reason);
    ASSERT_EQ(string("https://fwupdate.ruuvi.com/v1.14.3"), fw_update_get_binaries_url());

    ASSERT_EQ(
        "I http_server: Download firmware update info\n"
        "I http_download: cb_on_http_download_json_data: buf_size=" + std::to_string(strlen(p_firmwareUpdateJson)) + "\n"
        "I http_download: http_download_json: completed within 0 ticks\n"
        "I http_server: Firmware update info (json): " + string(p_firmwareUpdateJson) + "\n"
        "I http_server: Update is required (current version: v1.3.3, latest version: https://fwupdate.ruuvi.com/v1.14.3)\n"
        "I http_download: http_check: completed within 0 ticks\n"
        "D http_server: Feed watchdog\n"
        "I http_download: http_check: completed within 0 ticks\n"
        "D http_server: Feed watchdog\n"
        "I http_download: http_check: completed within 0 ticks\n"
        "D http_server: Feed watchdog\n"
        "I http_server: Run firmware auto-updating from URL: https://fwupdate.ruuvi.com/v1.14.3\n",
        esp_log_wrapper_get_logs());
    os_malloc_trace_dump();
    ESP_LOG_WRAPPER_TEST_CHECK_LOG_RECORD("MEM_TRACE", ESP_LOG_INFO, "Num blocks allocated: 0");
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_EQ(0, this->m_alloc_free_call_count);
}

TEST_F(TestHttpServerCb, test_http_server_cb_on_user_req__update_cycle_regular__latest_only_no_update_needed) // NOLINT
{
    {
        gw_cfg_t gw_cfg = { 0 };
        gw_cfg_default_get(&gw_cfg);
        gw_cfg.ruuvi_cfg.auto_update.auto_update_cycle = AUTO_UPDATE_CYCLE_TYPE_REGULAR;
        gw_cfg_update(&gw_cfg);
        esp_log_wrapper_clear();
    }

    const char* const p_firmwareUpdateJson
        = "{"
          "  \"latest\": {"
          "    \"version\": \"v1.3.3\","
          "    \"url\": \"https://fwupdate.ruuvi.com/v1.3.3\","
          "    \"created_at\": \"2023-10-06T11:26:07Z\""
          "  }"
          "}";
    this->m_firmware_update_resp             = str_buf_printf_with_alloc("%s", p_firmwareUpdateJson);
    const char* arr_of_binaries[]            = { "https://fwupdate.ruuvi.com/v1.3.3/ruuvi_gateway_esp.bin",
                                                 "https://fwupdate.ruuvi.com/v1.3.3/fatfs_gwui.bin",
                                                 "https://fwupdate.ruuvi.com/v1.3.3/fatfs_nrf52.bin" };
    this->m_http_check_with_auth_arr_of_urls = arr_of_binaries;
    this->m_http_check_with_auth_num_of_urls = sizeof(arr_of_binaries) / sizeof(arr_of_binaries[0]);

    http_server_cb_on_user_req(HTTP_SERVER_USER_REQ_CODE_DOWNLOAD_LATEST_RELEASE_INFO);

    ASSERT_EQ(FW_UPDATE_REASON_NONE, this->m_fw_updating_reason);

    ASSERT_EQ(
            "I http_server: Download firmware update info\n"
            "I http_download: cb_on_http_download_json_data: buf_size=" + std::to_string(strlen(p_firmwareUpdateJson)) + "\n"
            "I http_download: http_download_json: completed within 0 ticks\n"
            "I http_server: Firmware update info (json): " + string(p_firmwareUpdateJson) + "\n"
            "I http_server: Firmware update: No update is required, the latest version is already installed\n",
            esp_log_wrapper_get_logs());
    os_malloc_trace_dump();
    ESP_LOG_WRAPPER_TEST_CHECK_LOG_RECORD("MEM_TRACE", ESP_LOG_INFO, "Num blocks allocated: 0");
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_EQ(0, this->m_alloc_free_call_count);
}

TEST_F(
    TestHttpServerCb,
    test_http_server_cb_on_user_req__update_cycle_regular__latest_and_beta_no_update_needed) // NOLINT
{
    {
        gw_cfg_t gw_cfg = { 0 };
        gw_cfg_default_get(&gw_cfg);
        gw_cfg.ruuvi_cfg.auto_update.auto_update_cycle = AUTO_UPDATE_CYCLE_TYPE_REGULAR;
        gw_cfg_update(&gw_cfg);
        esp_log_wrapper_clear();
    }

    const char* const p_firmwareUpdateJson
        = "{"
          "  \"latest\": {"
          "    \"version\": \"v1.3.3\","
          "    \"url\": \"https://fwupdate.ruuvi.com/v1.3.3\","
          "    \"created_at\": \"2023-10-06T11:26:07Z\""
          "  },"
          "  \"beta\": {"
          "    \"version\": \"v1.3.4\","
          "    \"url\": \"https://fwupdate.ruuvi.com/v1.3.4\","
          "    \"created_at\": \"2023-12-06T11:26:07Z\""
          "  }"
          "}";
    this->m_firmware_update_resp             = str_buf_printf_with_alloc("%s", p_firmwareUpdateJson);
    const char* arr_of_binaries[]            = { "https://fwupdate.ruuvi.com/v1.3.3/ruuvi_gateway_esp.bin",
                                                 "https://fwupdate.ruuvi.com/v1.3.3/fatfs_gwui.bin",
                                                 "https://fwupdate.ruuvi.com/v1.3.3/fatfs_nrf52.bin" };
    this->m_http_check_with_auth_arr_of_urls = arr_of_binaries;
    this->m_http_check_with_auth_num_of_urls = sizeof(arr_of_binaries) / sizeof(arr_of_binaries[0]);

    http_server_cb_on_user_req(HTTP_SERVER_USER_REQ_CODE_DOWNLOAD_LATEST_RELEASE_INFO);

    ASSERT_EQ(FW_UPDATE_REASON_NONE, this->m_fw_updating_reason);

    ASSERT_EQ(
            "I http_server: Download firmware update info\n"
            "I http_download: cb_on_http_download_json_data: buf_size=" + std::to_string(strlen(p_firmwareUpdateJson)) + "\n"
            "I http_download: http_download_json: completed within 0 ticks\n"
            "I http_server: Firmware update info (json): " + string(p_firmwareUpdateJson) + "\n"
            "I http_server: Firmware update: No update is required, the latest version is already installed\n",
            esp_log_wrapper_get_logs());
    os_malloc_trace_dump();
    ESP_LOG_WRAPPER_TEST_CHECK_LOG_RECORD("MEM_TRACE", ESP_LOG_INFO, "Num blocks allocated: 0");
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_EQ(0, this->m_alloc_free_call_count);
}

TEST_F(TestHttpServerCb, test_http_server_cb_on_user_req__update_cycle_beta__latest_and_beta) // NOLINT
{
    {
        gw_cfg_t gw_cfg = { 0 };
        gw_cfg_default_get(&gw_cfg);
        gw_cfg.ruuvi_cfg.auto_update.auto_update_cycle = AUTO_UPDATE_CYCLE_TYPE_BETA_TESTER;
        gw_cfg_update(&gw_cfg);
        esp_log_wrapper_clear();
    }

    const char* const p_firmwareUpdateJson
        = "{"
          "  \"latest\": {"
          "    \"version\": \"v1.14.3\","
          "    \"url\": \"https://fwupdate.ruuvi.com/v1.14.3\","
          "    \"created_at\": \"2023-10-06T11:26:07Z\""
          "  },"
          "  \"beta\": {"
          "    \"version\": \"v1.14.4\","
          "    \"url\": \"https://github.com/ruuvi/ruuvi.gateway_esp.c/releases/download/v1.14.4\","
          "    \"created_at\": \"2023-09-19T11:16:48Z\""
          "  }"
          "}";
    this->m_firmware_update_resp  = str_buf_printf_with_alloc("%s", p_firmwareUpdateJson);
    const char* arr_of_binaries[] = {
        "https://github.com/ruuvi/ruuvi.gateway_esp.c/releases/download/v1.14.4/ruuvi_gateway_esp.bin",
        "https://github.com/ruuvi/ruuvi.gateway_esp.c/releases/download/v1.14.4/fatfs_gwui.bin",
        "https://github.com/ruuvi/ruuvi.gateway_esp.c/releases/download/v1.14.4/fatfs_nrf52.bin"
    };
    this->m_http_check_with_auth_arr_of_urls = arr_of_binaries;
    this->m_http_check_with_auth_num_of_urls = sizeof(arr_of_binaries) / sizeof(arr_of_binaries[0]);

    http_server_cb_on_user_req(HTTP_SERVER_USER_REQ_CODE_DOWNLOAD_LATEST_RELEASE_INFO);

    ASSERT_EQ(FW_UPDATE_REASON_AUTO, this->m_fw_updating_reason);
    ASSERT_EQ(
        string("https://github.com/ruuvi/ruuvi.gateway_esp.c/releases/download/v1.14.4"),
        fw_update_get_binaries_url());

    ASSERT_EQ(
            "I http_server: Download firmware update info\n"
            "I http_download: cb_on_http_download_json_data: buf_size=" + std::to_string(strlen(p_firmwareUpdateJson)) + "\n"
            "I http_download: http_download_json: completed within 0 ticks\n"
            "I http_server: Firmware update info (json): " + string(p_firmwareUpdateJson) + "\n"
            "I http_server: Update is required (current version: v1.3.3, beta version: https://github.com/ruuvi/ruuvi.gateway_esp.c/releases/download/v1.14.4)\n"
            "I http_download: http_check: completed within 0 ticks\n"
            "D http_server: Feed watchdog\n"
            "I http_download: http_check: completed within 0 ticks\n"
            "D http_server: Feed watchdog\n"
            "I http_download: http_check: completed within 0 ticks\n"
            "D http_server: Feed watchdog\n"
            "I http_server: Run firmware auto-updating from URL: https://github.com/ruuvi/ruuvi.gateway_esp.c/releases/download/v1.14.4\n",
            esp_log_wrapper_get_logs());
    os_malloc_trace_dump();
    ESP_LOG_WRAPPER_TEST_CHECK_LOG_RECORD("MEM_TRACE", ESP_LOG_INFO, "Num blocks allocated: 0");
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_EQ(0, this->m_alloc_free_call_count);
}

TEST_F(TestHttpServerCb, test_http_server_cb_on_user_req__update_cycle_beta__latest) // NOLINT
{
    {
        gw_cfg_t gw_cfg = { 0 };
        gw_cfg_default_get(&gw_cfg);
        gw_cfg.ruuvi_cfg.auto_update.auto_update_cycle = AUTO_UPDATE_CYCLE_TYPE_BETA_TESTER;
        gw_cfg_update(&gw_cfg);
        esp_log_wrapper_clear();
    }

    const char* const p_firmwareUpdateJson
        = "{"
          "  \"latest\": {"
          "    \"version\": \"v1.14.3\","
          "    \"url\": \"https://fwupdate.ruuvi.com/v1.14.3\","
          "    \"created_at\": \"2023-10-06T11:26:07Z\""
          "  }"
          "}";
    this->m_firmware_update_resp             = str_buf_printf_with_alloc("%s", p_firmwareUpdateJson);
    const char* arr_of_binaries[]            = { "https://fwupdate.ruuvi.com/v1.14.3/ruuvi_gateway_esp.bin",
                                                 "https://fwupdate.ruuvi.com/v1.14.3/fatfs_gwui.bin",
                                                 "https://fwupdate.ruuvi.com/v1.14.3/fatfs_nrf52.bin" };
    this->m_http_check_with_auth_arr_of_urls = arr_of_binaries;
    this->m_http_check_with_auth_num_of_urls = sizeof(arr_of_binaries) / sizeof(arr_of_binaries[0]);

    http_server_cb_on_user_req(HTTP_SERVER_USER_REQ_CODE_DOWNLOAD_LATEST_RELEASE_INFO);

    ASSERT_EQ(FW_UPDATE_REASON_AUTO, this->m_fw_updating_reason);
    ASSERT_EQ(string("https://fwupdate.ruuvi.com/v1.14.3"), fw_update_get_binaries_url());

    ASSERT_EQ(
            "I http_server: Download firmware update info\n"
            "I http_download: cb_on_http_download_json_data: buf_size=" + std::to_string(strlen(p_firmwareUpdateJson)) + "\n"
            "I http_download: http_download_json: completed within 0 ticks\n"
            "I http_server: Firmware update info (json): " + string(p_firmwareUpdateJson) + "\n"
            "I http_server: Update is required (current version: v1.3.3, latest version: https://fwupdate.ruuvi.com/v1.14.3)\n"
            "I http_download: http_check: completed within 0 ticks\n"
            "D http_server: Feed watchdog\n"
            "I http_download: http_check: completed within 0 ticks\n"
            "D http_server: Feed watchdog\n"
            "I http_download: http_check: completed within 0 ticks\n"
            "D http_server: Feed watchdog\n"
            "I http_server: Run firmware auto-updating from URL: https://fwupdate.ruuvi.com/v1.14.3\n",
            esp_log_wrapper_get_logs());
    os_malloc_trace_dump();
    ESP_LOG_WRAPPER_TEST_CHECK_LOG_RECORD("MEM_TRACE", ESP_LOG_INFO, "Num blocks allocated: 0");
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_EQ(0, this->m_alloc_free_call_count);
}

TEST_F(TestHttpServerCb, test_http_server_cb_on_user_req__update_cycle_beta__latest_only_no_update_needed) // NOLINT
{
    {
        gw_cfg_t gw_cfg = { 0 };
        gw_cfg_default_get(&gw_cfg);
        gw_cfg.ruuvi_cfg.auto_update.auto_update_cycle = AUTO_UPDATE_CYCLE_TYPE_BETA_TESTER;
        gw_cfg_update(&gw_cfg);
        esp_log_wrapper_clear();
    }

    const char* const p_firmwareUpdateJson
        = "{"
          "  \"latest\": {"
          "    \"version\": \"v1.3.3\","
          "    \"url\": \"https://fwupdate.ruuvi.com/v1.3.3\","
          "    \"created_at\": \"2023-10-06T11:26:07Z\""
          "  }"
          "}";
    this->m_firmware_update_resp             = str_buf_printf_with_alloc("%s", p_firmwareUpdateJson);
    const char* arr_of_binaries[]            = { "https://fwupdate.ruuvi.com/v1.3.3/ruuvi_gateway_esp.bin",
                                                 "https://fwupdate.ruuvi.com/v1.3.3/fatfs_gwui.bin",
                                                 "https://fwupdate.ruuvi.com/v1.3.3/fatfs_nrf52.bin" };
    this->m_http_check_with_auth_arr_of_urls = arr_of_binaries;
    this->m_http_check_with_auth_num_of_urls = sizeof(arr_of_binaries) / sizeof(arr_of_binaries[0]);

    http_server_cb_on_user_req(HTTP_SERVER_USER_REQ_CODE_DOWNLOAD_LATEST_RELEASE_INFO);

    ASSERT_EQ(FW_UPDATE_REASON_NONE, this->m_fw_updating_reason);

    ASSERT_EQ(
            "I http_server: Download firmware update info\n"
            "I http_download: cb_on_http_download_json_data: buf_size=" + std::to_string(strlen(p_firmwareUpdateJson)) + "\n"
            "I http_download: http_download_json: completed within 0 ticks\n"
            "I http_server: Firmware update info (json): " + string(p_firmwareUpdateJson) + "\n"
            "I http_server: Firmware update: No update is required, the latest version is already installed\n",
            esp_log_wrapper_get_logs());
    os_malloc_trace_dump();
    ESP_LOG_WRAPPER_TEST_CHECK_LOG_RECORD("MEM_TRACE", ESP_LOG_INFO, "Num blocks allocated: 0");
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_EQ(0, this->m_alloc_free_call_count);
}

TEST_F(TestHttpServerCb, test_http_server_cb_on_user_req__update_cycle_beta__latest_and_beta_no_update_needed) // NOLINT
{
    {
        gw_cfg_t gw_cfg = { 0 };
        gw_cfg_default_get(&gw_cfg);
        gw_cfg.ruuvi_cfg.auto_update.auto_update_cycle = AUTO_UPDATE_CYCLE_TYPE_BETA_TESTER;
        gw_cfg_update(&gw_cfg);
        esp_log_wrapper_clear();
    }

    const char* const p_firmwareUpdateJson
        = "{"
          "  \"latest\": {"
          "    \"version\": \"v1.3.2\","
          "    \"url\": \"https://fwupdate.ruuvi.com/v1.3.2\","
          "    \"created_at\": \"2023-10-06T11:26:07Z\""
          "  },"
          "  \"beta\": {"
          "    \"version\": \"v1.3.3\","
          "    \"url\": \"https://fwupdate.ruuvi.com/v1.3.3\","
          "    \"created_at\": \"2023-12-06T11:26:07Z\""
          "  }"
          "}";
    this->m_firmware_update_resp             = str_buf_printf_with_alloc("%s", p_firmwareUpdateJson);
    const char* arr_of_binaries[]            = { "https://fwupdate.ruuvi.com/v1.3.3/ruuvi_gateway_esp.bin",
                                                 "https://fwupdate.ruuvi.com/v1.3.3/fatfs_gwui.bin",
                                                 "https://fwupdate.ruuvi.com/v1.3.3/fatfs_nrf52.bin" };
    this->m_http_check_with_auth_arr_of_urls = arr_of_binaries;
    this->m_http_check_with_auth_num_of_urls = sizeof(arr_of_binaries) / sizeof(arr_of_binaries[0]);

    http_server_cb_on_user_req(HTTP_SERVER_USER_REQ_CODE_DOWNLOAD_LATEST_RELEASE_INFO);

    ASSERT_EQ(FW_UPDATE_REASON_NONE, this->m_fw_updating_reason);

    ASSERT_EQ(
            "I http_server: Download firmware update info\n"
            "I http_download: cb_on_http_download_json_data: buf_size=" + std::to_string(strlen(p_firmwareUpdateJson)) + "\n"
            "I http_download: http_download_json: completed within 0 ticks\n"
            "I http_server: Firmware update info (json): " + string(p_firmwareUpdateJson) + "\n"
            "I http_server: Firmware update: No update is required, the latest version is already installed\n",
            esp_log_wrapper_get_logs());
    os_malloc_trace_dump();
    ESP_LOG_WRAPPER_TEST_CHECK_LOG_RECORD("MEM_TRACE", ESP_LOG_INFO, "Num blocks allocated: 0");
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_EQ(0, this->m_alloc_free_call_count);
}

TEST_F(TestHttpServerCb, test_http_server_cb_on_user_req__update_cycle_manual__latest_and_beta) // NOLINT
{
    {
        gw_cfg_t gw_cfg = { 0 };
        gw_cfg_default_get(&gw_cfg);
        gw_cfg.ruuvi_cfg.auto_update.auto_update_cycle = AUTO_UPDATE_CYCLE_TYPE_MANUAL;
        gw_cfg_update(&gw_cfg);
        esp_log_wrapper_clear();
    }

    const char* const p_firmwareUpdateJson
        = "{"
          "  \"latest\": {"
          "    \"version\": \"v1.14.3\","
          "    \"url\": \"https://fwupdate.ruuvi.com/v1.14.3\","
          "    \"created_at\": \"2023-10-06T11:26:07Z\""
          "  },"
          "  \"beta\": {"
          "    \"version\": \"v1.14.4\","
          "    \"url\": \"https://github.com/ruuvi/ruuvi.gateway_esp.c/releases/download/v1.14.4\","
          "    \"created_at\": \"2023-09-19T11:16:48Z\""
          "  }"
          "}";
    this->m_firmware_update_resp  = str_buf_printf_with_alloc("%s", p_firmwareUpdateJson);
    const char* arr_of_binaries[] = {
        "https://github.com/ruuvi/ruuvi.gateway_esp.c/releases/download/v1.14.4/ruuvi_gateway_esp.bin",
        "https://github.com/ruuvi/ruuvi.gateway_esp.c/releases/download/v1.14.4/fatfs_gwui.bin",
        "https://github.com/ruuvi/ruuvi.gateway_esp.c/releases/download/v1.14.4/fatfs_nrf52.bin"
    };
    this->m_http_check_with_auth_arr_of_urls = arr_of_binaries;
    this->m_http_check_with_auth_num_of_urls = sizeof(arr_of_binaries) / sizeof(arr_of_binaries[0]);

    http_server_cb_on_user_req(HTTP_SERVER_USER_REQ_CODE_DOWNLOAD_LATEST_RELEASE_INFO);

    ASSERT_EQ(FW_UPDATE_REASON_NONE, this->m_fw_updating_reason);

    ASSERT_EQ(
            "I http_server: Download firmware update info\n"
            "I http_download: cb_on_http_download_json_data: buf_size=" + std::to_string(strlen(p_firmwareUpdateJson)) + "\n"
            "I http_download: http_download_json: completed within 0 ticks\n"
            "I http_server: Firmware update info (json): " + string(p_firmwareUpdateJson) + "\n"
            "I http_server: Firmware update: No update is required, the latest version is already installed\n",
            esp_log_wrapper_get_logs());
    os_malloc_trace_dump();
    ESP_LOG_WRAPPER_TEST_CHECK_LOG_RECORD("MEM_TRACE", ESP_LOG_INFO, "Num blocks allocated: 0");
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_EQ(0, this->m_alloc_free_call_count);
}

TEST_F(TestHttpServerCb, test_http_server_cb_on_user_req__update_cycle_manual__latest) // NOLINT
{
    {
        gw_cfg_t gw_cfg = { 0 };
        gw_cfg_default_get(&gw_cfg);
        gw_cfg.ruuvi_cfg.auto_update.auto_update_cycle = AUTO_UPDATE_CYCLE_TYPE_MANUAL;
        gw_cfg_update(&gw_cfg);
        esp_log_wrapper_clear();
    }

    const char* const p_firmwareUpdateJson
        = "{"
          "  \"latest\": {"
          "    \"version\": \"v1.14.3\","
          "    \"url\": \"https://fwupdate.ruuvi.com/v1.14.3\","
          "    \"created_at\": \"2023-10-06T11:26:07Z\""
          "  }"
          "}";
    this->m_firmware_update_resp             = str_buf_printf_with_alloc("%s", p_firmwareUpdateJson);
    const char* arr_of_binaries[]            = { "https://fwupdate.ruuvi.com/v1.14.3/ruuvi_gateway_esp.bin",
                                                 "https://fwupdate.ruuvi.com/v1.14.3/fatfs_gwui.bin",
                                                 "https://fwupdate.ruuvi.com/v1.14.3/fatfs_nrf52.bin" };
    this->m_http_check_with_auth_arr_of_urls = arr_of_binaries;
    this->m_http_check_with_auth_num_of_urls = sizeof(arr_of_binaries) / sizeof(arr_of_binaries[0]);

    http_server_cb_on_user_req(HTTP_SERVER_USER_REQ_CODE_DOWNLOAD_LATEST_RELEASE_INFO);

    ASSERT_EQ(FW_UPDATE_REASON_NONE, this->m_fw_updating_reason);

    ASSERT_EQ(
            "I http_server: Download firmware update info\n"
            "I http_download: cb_on_http_download_json_data: buf_size=" + std::to_string(strlen(p_firmwareUpdateJson)) + "\n"
            "I http_download: http_download_json: completed within 0 ticks\n"
            "I http_server: Firmware update info (json): " + string(p_firmwareUpdateJson) + "\n"
            "I http_server: Firmware update: No update is required, the latest version is already installed\n",
            esp_log_wrapper_get_logs());
    os_malloc_trace_dump();
    ESP_LOG_WRAPPER_TEST_CHECK_LOG_RECORD("MEM_TRACE", ESP_LOG_INFO, "Num blocks allocated: 0");
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_EQ(0, this->m_alloc_free_call_count);
}

TEST_F(TestHttpServerCb, test_http_server_cb_on_user_req__update_cycle_manual__latest_only_no_update_needed) // NOLINT
{
    {
        gw_cfg_t gw_cfg = { 0 };
        gw_cfg_default_get(&gw_cfg);
        gw_cfg.ruuvi_cfg.auto_update.auto_update_cycle = AUTO_UPDATE_CYCLE_TYPE_MANUAL;
        gw_cfg_update(&gw_cfg);
        esp_log_wrapper_clear();
    }

    const char* const p_firmwareUpdateJson
        = "{"
          "  \"latest\": {"
          "    \"version\": \"v1.3.3\","
          "    \"url\": \"https://fwupdate.ruuvi.com/v1.3.3\","
          "    \"created_at\": \"2023-10-06T11:26:07Z\""
          "  }"
          "}";
    this->m_firmware_update_resp             = str_buf_printf_with_alloc("%s", p_firmwareUpdateJson);
    const char* arr_of_binaries[]            = { "https://fwupdate.ruuvi.com/v1.3.3/ruuvi_gateway_esp.bin",
                                                 "https://fwupdate.ruuvi.com/v1.3.3/fatfs_gwui.bin",
                                                 "https://fwupdate.ruuvi.com/v1.3.3/fatfs_nrf52.bin" };
    this->m_http_check_with_auth_arr_of_urls = arr_of_binaries;
    this->m_http_check_with_auth_num_of_urls = sizeof(arr_of_binaries) / sizeof(arr_of_binaries[0]);

    http_server_cb_on_user_req(HTTP_SERVER_USER_REQ_CODE_DOWNLOAD_LATEST_RELEASE_INFO);

    ASSERT_EQ(FW_UPDATE_REASON_NONE, this->m_fw_updating_reason);

    ASSERT_EQ(
            "I http_server: Download firmware update info\n"
            "I http_download: cb_on_http_download_json_data: buf_size=" + std::to_string(strlen(p_firmwareUpdateJson)) + "\n"
            "I http_download: http_download_json: completed within 0 ticks\n"
            "I http_server: Firmware update info (json): " + string(p_firmwareUpdateJson) + "\n"
            "I http_server: Firmware update: No update is required, the latest version is already installed\n",
            esp_log_wrapper_get_logs());
    os_malloc_trace_dump();
    ESP_LOG_WRAPPER_TEST_CHECK_LOG_RECORD("MEM_TRACE", ESP_LOG_INFO, "Num blocks allocated: 0");
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_EQ(0, this->m_alloc_free_call_count);
}

TEST_F(
    TestHttpServerCb,
    test_http_server_cb_on_user_req__update_cycle_manual__latest_and_beta_no_update_needed) // NOLINT
{
    {
        gw_cfg_t gw_cfg = { 0 };
        gw_cfg_default_get(&gw_cfg);
        gw_cfg.ruuvi_cfg.auto_update.auto_update_cycle = AUTO_UPDATE_CYCLE_TYPE_MANUAL;
        gw_cfg_update(&gw_cfg);
        esp_log_wrapper_clear();
    }

    const char* const p_firmwareUpdateJson
        = "{"
          "  \"latest\": {"
          "    \"version\": \"v1.3.3\","
          "    \"url\": \"https://fwupdate.ruuvi.com/v1.3.3\","
          "    \"created_at\": \"2023-10-06T11:26:07Z\""
          "  },"
          "  \"beta\": {"
          "    \"version\": \"v1.3.4\","
          "    \"url\": \"https://fwupdate.ruuvi.com/v1.3.4\","
          "    \"created_at\": \"2023-12-06T11:26:07Z\""
          "  }"
          "}";
    this->m_firmware_update_resp             = str_buf_printf_with_alloc("%s", p_firmwareUpdateJson);
    const char* arr_of_binaries[]            = { "https://fwupdate.ruuvi.com/v1.3.3/ruuvi_gateway_esp.bin",
                                                 "https://fwupdate.ruuvi.com/v1.3.3/fatfs_gwui.bin",
                                                 "https://fwupdate.ruuvi.com/v1.3.3/fatfs_nrf52.bin" };
    this->m_http_check_with_auth_arr_of_urls = arr_of_binaries;
    this->m_http_check_with_auth_num_of_urls = sizeof(arr_of_binaries) / sizeof(arr_of_binaries[0]);

    http_server_cb_on_user_req(HTTP_SERVER_USER_REQ_CODE_DOWNLOAD_LATEST_RELEASE_INFO);

    ASSERT_EQ(FW_UPDATE_REASON_NONE, this->m_fw_updating_reason);

    ASSERT_EQ(
            "I http_server: Download firmware update info\n"
            "I http_download: cb_on_http_download_json_data: buf_size=" + std::to_string(strlen(p_firmwareUpdateJson)) + "\n"
            "I http_download: http_download_json: completed within 0 ticks\n"
            "I http_server: Firmware update info (json): " + string(p_firmwareUpdateJson) + "\n"
            "I http_server: Firmware update: No update is required, the latest version is already installed\n",
            esp_log_wrapper_get_logs());
    os_malloc_trace_dump();
    ESP_LOG_WRAPPER_TEST_CHECK_LOG_RECORD("MEM_TRACE", ESP_LOG_INFO, "Num blocks allocated: 0");
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_EQ(0, this->m_alloc_free_call_count);
}

TEST_F(TestHttpServerCb, test_http_server_cb_on_user_req__update_cycle_regular__empty_version) // NOLINT
{
    {
        gw_cfg_t gw_cfg = { 0 };
        gw_cfg_default_get(&gw_cfg);
        gw_cfg.ruuvi_cfg.auto_update.auto_update_cycle = AUTO_UPDATE_CYCLE_TYPE_REGULAR;
        gw_cfg_update(&gw_cfg);
        esp_log_wrapper_clear();
    }

    const char* const p_firmwareUpdateJson
        = "{"
          "  \"latest\": {"
          "    \"version\": \"\","
          "    \"url\": \"\","
          "    \"created_at\": \"\""
          "  }"
          "}";
    this->m_firmware_update_resp = str_buf_printf_with_alloc("%s", p_firmwareUpdateJson);

    http_server_cb_on_user_req(HTTP_SERVER_USER_REQ_CODE_DOWNLOAD_LATEST_RELEASE_INFO);

    ASSERT_EQ(FW_UPDATE_REASON_NONE, this->m_fw_updating_reason);

    ASSERT_EQ(
            "I http_server: Download firmware update info\n"
            "I http_download: cb_on_http_download_json_data: buf_size=" + std::to_string(strlen(p_firmwareUpdateJson)) + "\n"
            "I http_download: http_download_json: completed within 0 ticks\n"
            "I http_server: Firmware update info (json): " + string(p_firmwareUpdateJson) + "\n"
            "E http_server: Firmware_update info 'latest/version' is empty\n"
            "I http_server: Firmware update: No update is required, the latest version is already installed\n",
            esp_log_wrapper_get_logs());
    os_malloc_trace_dump();
    ESP_LOG_WRAPPER_TEST_CHECK_LOG_RECORD("MEM_TRACE", ESP_LOG_INFO, "Num blocks allocated: 0");
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_EQ(0, this->m_alloc_free_call_count);
}

TEST_F(TestHttpServerCb, http_server_cb_on_post_fw_update_reset_blocked_during_fw_update) // NOLINT
{
    this->m_fw_update_is_in_progress = true;
    const http_server_resp_t resp    = http_server_cb_on_post("fw_update_reset", nullptr, nullptr, false);

    ASSERT_EQ(HTTP_RESP_CODE_409, resp.http_resp_code);
    TEST_CHECK_LOG_RECORD_HTTP_SERVER(ESP_LOG_DEBUG, string("http_server_cb_on_post /fw_update_reset, params="));
    TEST_CHECK_LOG_RECORD_HTTP_SERVER(
        ESP_LOG_ERROR,
        string("FW update in progress, cannot handle POST request: /fw_update_reset, params=, flag_access_from_lan=0"));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    os_malloc_trace_dump();
    ESP_LOG_WRAPPER_TEST_CHECK_LOG_RECORD("MEM_TRACE", ESP_LOG_INFO, "Num blocks allocated: 0");
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_EQ(0, this->m_alloc_free_call_count);
}

TEST_F(TestHttpServerCb, http_server_cb_on_post_ruuvi_json_blocked_during_fw_update) // NOLINT
{
    this->m_fw_update_is_in_progress = true;
    const http_server_resp_t resp    = http_server_cb_on_post("ruuvi.json", nullptr, "{}", false);

    ASSERT_EQ(HTTP_RESP_CODE_409, resp.http_resp_code);
    TEST_CHECK_LOG_RECORD_HTTP_SERVER(ESP_LOG_DEBUG, string("http_server_cb_on_post /ruuvi.json, params="));
    TEST_CHECK_LOG_RECORD_HTTP_SERVER(
        ESP_LOG_ERROR,
        string("FW update in progress, cannot handle POST request: /ruuvi.json, params=, flag_access_from_lan=0"));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    os_malloc_trace_dump();
    ESP_LOG_WRAPPER_TEST_CHECK_LOG_RECORD("MEM_TRACE", ESP_LOG_INFO, "Num blocks allocated: 0");
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_EQ(0, this->m_alloc_free_call_count);
}

TEST_F(TestHttpServerCb, http_server_cb_on_post_bluetooth_scanning_json_blocked_during_fw_update) // NOLINT
{
    this->m_fw_update_is_in_progress = true;
    const http_server_resp_t resp    = http_server_cb_on_post("bluetooth_scanning.json", nullptr, "{}", false);

    ASSERT_EQ(HTTP_RESP_CODE_409, resp.http_resp_code);
    TEST_CHECK_LOG_RECORD_HTTP_SERVER(
        ESP_LOG_DEBUG,
        string("http_server_cb_on_post /bluetooth_scanning.json, params="));
    TEST_CHECK_LOG_RECORD_HTTP_SERVER(
        ESP_LOG_ERROR,
        string("FW update in progress, cannot handle POST request: /bluetooth_scanning.json, params=, "
               "flag_access_from_lan=0"));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    os_malloc_trace_dump();
    ESP_LOG_WRAPPER_TEST_CHECK_LOG_RECORD("MEM_TRACE", ESP_LOG_INFO, "Num blocks allocated: 0");
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_EQ(0, this->m_alloc_free_call_count);
}

TEST_F(TestHttpServerCb, http_server_cb_on_post_fw_update_json_blocked_during_fw_update) // NOLINT
{
    this->m_fw_update_is_in_progress = true;
    const http_server_resp_t resp    = http_server_cb_on_post("fw_update.json", nullptr, "{}", false);

    ASSERT_EQ(HTTP_RESP_CODE_409, resp.http_resp_code);
    TEST_CHECK_LOG_RECORD_HTTP_SERVER(ESP_LOG_DEBUG, string("http_server_cb_on_post /fw_update.json, params="));
    TEST_CHECK_LOG_RECORD_HTTP_SERVER(
        ESP_LOG_ERROR,
        string("FW update in progress, cannot handle POST request: /fw_update.json, params=, flag_access_from_lan=0"));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    os_malloc_trace_dump();
    ESP_LOG_WRAPPER_TEST_CHECK_LOG_RECORD("MEM_TRACE", ESP_LOG_INFO, "Num blocks allocated: 0");
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_EQ(0, this->m_alloc_free_call_count);
}

TEST_F(TestHttpServerCb, http_server_cb_on_post_fw_update_url_json_blocked_during_fw_update) // NOLINT
{
    this->m_fw_update_is_in_progress = true;
    const http_server_resp_t resp    = http_server_cb_on_post("fw_update_url.json", nullptr, "{}", false);

    ASSERT_EQ(HTTP_RESP_CODE_409, resp.http_resp_code);
    TEST_CHECK_LOG_RECORD_HTTP_SERVER(ESP_LOG_DEBUG, string("http_server_cb_on_post /fw_update_url.json, params="));
    TEST_CHECK_LOG_RECORD_HTTP_SERVER(
        ESP_LOG_ERROR,
        string(
            "FW update in progress, cannot handle POST request: /fw_update_url.json, params=, flag_access_from_lan=0"));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    os_malloc_trace_dump();
    ESP_LOG_WRAPPER_TEST_CHECK_LOG_RECORD("MEM_TRACE", ESP_LOG_INFO, "Num blocks allocated: 0");
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_EQ(0, this->m_alloc_free_call_count);
}

TEST_F(TestHttpServerCb, http_server_cb_on_post_fw_update_url_blocked_during_fw_update) // NOLINT
{
    this->m_fw_update_is_in_progress = true;
    const http_server_resp_t resp    = http_server_cb_on_post("fw_update_url", nullptr, "{}", false);

    ASSERT_EQ(HTTP_RESP_CODE_409, resp.http_resp_code);
    TEST_CHECK_LOG_RECORD_HTTP_SERVER(ESP_LOG_DEBUG, string("http_server_cb_on_post /fw_update_url, params="));
    TEST_CHECK_LOG_RECORD_HTTP_SERVER(
        ESP_LOG_ERROR,
        string("FW update in progress, cannot handle POST request: /fw_update_url, params=, flag_access_from_lan=0"));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    os_malloc_trace_dump();
    ESP_LOG_WRAPPER_TEST_CHECK_LOG_RECORD("MEM_TRACE", ESP_LOG_INFO, "Num blocks allocated: 0");
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_EQ(0, this->m_alloc_free_call_count);
}

TEST_F(TestHttpServerCb, http_server_cb_on_post_ssl_cert_blocked_during_fw_update) // NOLINT
{
    this->m_fw_update_is_in_progress = true;
    const http_server_resp_t resp    = http_server_cb_on_post("ssl_cert", nullptr, "{}", false);

    ASSERT_EQ(HTTP_RESP_CODE_409, resp.http_resp_code);
    TEST_CHECK_LOG_RECORD_HTTP_SERVER(ESP_LOG_DEBUG, string("http_server_cb_on_post /ssl_cert, params="));
    TEST_CHECK_LOG_RECORD_HTTP_SERVER(
        ESP_LOG_ERROR,
        string("FW update in progress, cannot handle POST request: /ssl_cert, params=, flag_access_from_lan=0"));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    os_malloc_trace_dump();
    ESP_LOG_WRAPPER_TEST_CHECK_LOG_RECORD("MEM_TRACE", ESP_LOG_INFO, "Num blocks allocated: 0");
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_EQ(0, this->m_alloc_free_call_count);
}

TEST_F(TestHttpServerCb, http_server_cb_on_post_init_storage_blocked_during_fw_update) // NOLINT
{
    this->m_fw_update_is_in_progress = true;
    const http_server_resp_t resp    = http_server_cb_on_post("init_storage", nullptr, "{}", false);

    ASSERT_EQ(HTTP_RESP_CODE_409, resp.http_resp_code);
    TEST_CHECK_LOG_RECORD_HTTP_SERVER(ESP_LOG_DEBUG, string("http_server_cb_on_post /init_storage, params="));
    TEST_CHECK_LOG_RECORD_HTTP_SERVER(
        ESP_LOG_ERROR,
        string("FW update in progress, cannot handle POST request: /init_storage, params=, flag_access_from_lan=0"));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    os_malloc_trace_dump();
    ESP_LOG_WRAPPER_TEST_CHECK_LOG_RECORD("MEM_TRACE", ESP_LOG_INFO, "Num blocks allocated: 0");
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_EQ(0, this->m_alloc_free_call_count);
}

TEST_F(TestHttpServerCb, http_server_cb_on_post_any_blocked_during_fw_update) // NOLINT
{
    this->m_fw_update_is_in_progress = true;
    const http_server_resp_t resp    = http_server_cb_on_post("any_request", nullptr, "{}", false);

    ASSERT_EQ(HTTP_RESP_CODE_409, resp.http_resp_code);
    TEST_CHECK_LOG_RECORD_HTTP_SERVER(ESP_LOG_DEBUG, string("http_server_cb_on_post /any_request, params="));
    TEST_CHECK_LOG_RECORD_HTTP_SERVER(
        ESP_LOG_ERROR,
        string("FW update in progress, cannot handle POST request: /any_request, params=, flag_access_from_lan=0"));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    os_malloc_trace_dump();
    ESP_LOG_WRAPPER_TEST_CHECK_LOG_RECORD("MEM_TRACE", ESP_LOG_INFO, "Num blocks allocated: 0");
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_EQ(0, this->m_alloc_free_call_count);
}

TEST_F(TestHttpServerCb, http_server_cb_on_get_validate_url_blocked_during_fw_update) // NOLINT
{
    this->m_fw_update_is_in_progress = true;
    const http_server_resp_t resp    = http_server_cb_on_get("validate_url", nullptr, false, nullptr);

    ASSERT_EQ(HTTP_RESP_CODE_409, resp.http_resp_code);
    TEST_CHECK_LOG_RECORD_HTTP_SERVER(ESP_LOG_DEBUG, string("http_server_cb_on_get /validate_url"));
    TEST_CHECK_LOG_RECORD_HTTP_SERVER(
        ESP_LOG_ERROR,
        string("FW update in progress, cannot handle GET request: /validate_url"));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    os_malloc_trace_dump();
    ESP_LOG_WRAPPER_TEST_CHECK_LOG_RECORD("MEM_TRACE", ESP_LOG_INFO, "Num blocks allocated: 0");
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_EQ(0, this->m_alloc_free_call_count);
}

TEST_F(TestHttpServerCb, http_server_cb_on_delete_blocked_during_fw_update) // NOLINT
{
    this->m_fw_update_is_in_progress = true;
    const http_server_resp_t resp    = http_server_cb_on_delete("unknown.json", nullptr, false, nullptr);

    ASSERT_EQ(HTTP_RESP_CODE_409, resp.http_resp_code);
    ASSERT_EQ(HTTP_CONTENT_LOCATION_NO_CONTENT, resp.content_location);
    ASSERT_TRUE(resp.flag_no_cache);
    ASSERT_EQ(HTTP_CONTENT_TYPE_TEXT_HTML, resp.content_type);
    ASSERT_EQ(nullptr, resp.p_content_type_param);
    ASSERT_EQ(0, resp.content_len);
    ASSERT_EQ(HTTP_CONTENT_ENCODING_NONE, resp.content_encoding);
    TEST_CHECK_LOG_RECORD_HTTP_SERVER(ESP_LOG_INFO, string("DELETE /unknown.json, params="));
    TEST_CHECK_LOG_RECORD_HTTP_SERVER(
        ESP_LOG_ERROR,
        string("FW update in progress, cannot handle DELETE request: /unknown.json, params="));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    os_malloc_trace_dump();
    ESP_LOG_WRAPPER_TEST_CHECK_LOG_RECORD("MEM_TRACE", ESP_LOG_INFO, "Num blocks allocated: 0");
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_EQ(0, this->m_alloc_free_call_count);
}

/*** http_download_json / cb_on_http_download_json_data tests
 * *******************************************************************************************************/

TEST_F(TestHttpServerCb, http_download_json__ok_single_chunk_with_known_content_length) // NOLINT
{
    const char* p_json_resp              = "{\"version\":\"1.0\"}";
    this->m_mock_http_download_with_auth = [&](const http_download_param_with_auth_t* p_param,
                                               http_download_cb_on_data_t             p_cb_on_data,
                                               void*                                  p_user_data,
                                               bool flag_use_big_tls_buf) -> http_server_resp_t {
        const size_t len = strlen(p_json_resp);
        p_cb_on_data((const uint8_t*)p_json_resp, len, 0, len, HTTP_RESP_CODE_200, 0, p_user_data);
        const str_buf_t str_buf = str_buf_printf_with_alloc("%s", p_json_resp);
        return http_server_resp_200_json_in_heap(str_buf.buf);
    };

    esp_log_wrapper_clear();
    const http_download_param_with_auth_t params = {
        .base = {
            .p_url                   = "https://example.com/data.json",
            .timeout_seconds         = 10,
            .flag_feed_task_watchdog = false,
            .flag_free_memory        = false,
            .p_server_cert           = NULL,
            .p_client_cert           = NULL,
            .p_client_key            = NULL,
        },
        .auth_type           = GW_CFG_HTTP_AUTH_TYPE_NONE,
        .p_http_auth         = NULL,
        .p_extra_header_item = NULL,
    };
    http_server_download_info_t info = http_download_json(&params);

    ASSERT_FALSE(info.is_error);
    ASSERT_EQ(HTTP_RESP_CODE_200, info.http_resp_code);
    ASSERT_NE(nullptr, info.p_json_buf);
    ASSERT_EQ(string(p_json_resp), string(info.p_json_buf));

    os_free(info.p_json_buf);
    os_malloc_trace_dump();
    TEST_CHECK_LOG_RECORD_HTTP_DOWNLOAD(ESP_LOG_INFO, "cb_on_http_download_json_data: buf_size=17");
    TEST_CHECK_LOG_RECORD_HTTP_DOWNLOAD(ESP_LOG_INFO, "http_download_json: completed within 0 ticks");
    ESP_LOG_WRAPPER_TEST_CHECK_LOG_RECORD("MEM_TRACE", ESP_LOG_INFO, "Num blocks allocated: 0");
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_EQ(0, this->m_alloc_free_call_count);
}

TEST_F(TestHttpServerCb, http_download_json__ok_single_chunk_with_unknown_content_length) // NOLINT
{
    const char* p_json_resp              = "{\"key\":\"val\"}";
    this->m_mock_http_download_with_auth = [&](const http_download_param_with_auth_t* p_param,
                                               http_download_cb_on_data_t             p_cb_on_data,
                                               void*                                  p_user_data,
                                               bool flag_use_big_tls_buf) -> http_server_resp_t {
        const size_t len = strlen(p_json_resp);
        // content_length=0 means unknown
        p_cb_on_data((const uint8_t*)p_json_resp, len, 0, 0, HTTP_RESP_CODE_200, 0, p_user_data);
        const str_buf_t str_buf = str_buf_printf_with_alloc("%s", p_json_resp);
        return http_server_resp_200_json_in_heap(str_buf.buf);
    };

    esp_log_wrapper_clear();
    const http_download_param_with_auth_t params = {
        .base = {
            .p_url                   = "https://example.com/data.json",
            .timeout_seconds         = 10,
            .flag_feed_task_watchdog = false,
            .flag_free_memory        = false,
            .p_server_cert           = NULL,
            .p_client_cert           = NULL,
            .p_client_key            = NULL,
        },
        .auth_type           = GW_CFG_HTTP_AUTH_TYPE_NONE,
        .p_http_auth         = NULL,
        .p_extra_header_item = NULL,
    };
    http_server_download_info_t info = http_download_json(&params);

    ASSERT_FALSE(info.is_error);
    ASSERT_EQ(HTTP_RESP_CODE_200, info.http_resp_code);
    ASSERT_NE(nullptr, info.p_json_buf);
    ASSERT_EQ(string(p_json_resp), string(info.p_json_buf));

    os_free(info.p_json_buf);
    os_malloc_trace_dump();
    TEST_CHECK_LOG_RECORD_HTTP_DOWNLOAD(ESP_LOG_INFO, "cb_on_http_download_json_data: buf_size=13");
    TEST_CHECK_LOG_RECORD_HTTP_DOWNLOAD(ESP_LOG_INFO, "http_download_json: completed within 0 ticks");
    ESP_LOG_WRAPPER_TEST_CHECK_LOG_RECORD("MEM_TRACE", ESP_LOG_INFO, "Num blocks allocated: 0");
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_EQ(0, this->m_alloc_free_call_count);
}

TEST_F(TestHttpServerCb, http_download_json__ok_two_chunks_unknown_content_length) // NOLINT
{
    const char* p_chunk1                 = "{\"key\":";
    const char* p_chunk2                 = "\"val\"}";
    this->m_mock_http_download_with_auth = [&](const http_download_param_with_auth_t* p_param,
                                               http_download_cb_on_data_t             p_cb_on_data,
                                               void*                                  p_user_data,
                                               bool flag_use_big_tls_buf) -> http_server_resp_t {
        const size_t len1 = strlen(p_chunk1);
        const size_t len2 = strlen(p_chunk2);
        // content_length=0 means unknown, will trigger realloc on second chunk
        p_cb_on_data((const uint8_t*)p_chunk1, len1, 0, 0, HTTP_RESP_CODE_200, 0, p_user_data);
        p_cb_on_data((const uint8_t*)p_chunk2, len2, len1, 0, HTTP_RESP_CODE_200, 0, p_user_data);
        const str_buf_t str_buf = str_buf_printf_with_alloc("{\"key\":\"val\"}");
        return http_server_resp_200_json_in_heap(str_buf.buf);
    };

    esp_log_wrapper_clear();
    const http_download_param_with_auth_t params = {
        .base = {
            .p_url                   = "https://example.com/data.json",
            .timeout_seconds         = 10,
            .flag_feed_task_watchdog = false,
            .flag_free_memory        = false,
            .p_server_cert           = NULL,
            .p_client_cert           = NULL,
            .p_client_key            = NULL,
        },
        .auth_type           = GW_CFG_HTTP_AUTH_TYPE_NONE,
        .p_http_auth         = NULL,
        .p_extra_header_item = NULL,
    };
    http_server_download_info_t info = http_download_json(&params);

    ASSERT_FALSE(info.is_error);
    ASSERT_EQ(HTTP_RESP_CODE_200, info.http_resp_code);
    ASSERT_NE(nullptr, info.p_json_buf);
    ASSERT_EQ(string("{\"key\":\"val\"}"), string(info.p_json_buf));

    os_free(info.p_json_buf);
    os_malloc_trace_dump();
    TEST_CHECK_LOG_RECORD_HTTP_DOWNLOAD(ESP_LOG_INFO, "cb_on_http_download_json_data: buf_size=7");
    TEST_CHECK_LOG_RECORD_HTTP_DOWNLOAD(ESP_LOG_INFO, "cb_on_http_download_json_data: buf_size=6");
    TEST_CHECK_LOG_RECORD_HTTP_DOWNLOAD(ESP_LOG_INFO, "Reallocate 14 bytes");
    TEST_CHECK_LOG_RECORD_HTTP_DOWNLOAD(ESP_LOG_INFO, "http_download_json: completed within 0 ticks");
    ESP_LOG_WRAPPER_TEST_CHECK_LOG_RECORD("MEM_TRACE", ESP_LOG_INFO, "Num blocks allocated: 0");
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_EQ(0, this->m_alloc_free_call_count);
}

TEST_F(TestHttpServerCb, http_download_json__callback_redirect_301) // NOLINT
{
    const char* p_json_resp              = "{\"result\":\"ok\"}";
    this->m_mock_http_download_with_auth = [&](const http_download_param_with_auth_t* p_param,
                                               http_download_cb_on_data_t             p_cb_on_data,
                                               void*                                  p_user_data,
                                               bool flag_use_big_tls_buf) -> http_server_resp_t {
        // First a redirect chunk (should be skipped), then real data
        const uint8_t redirect_data[] = "Moved";
        p_cb_on_data(redirect_data, sizeof(redirect_data) - 1, 0, 0, HTTP_RESP_CODE_301, 0, p_user_data);
        const size_t len = strlen(p_json_resp);
        p_cb_on_data((const uint8_t*)p_json_resp, len, 0, len, HTTP_RESP_CODE_200, 0, p_user_data);
        const str_buf_t str_buf = str_buf_printf_with_alloc("%s", p_json_resp);
        return http_server_resp_200_json_in_heap(str_buf.buf);
    };

    esp_log_wrapper_clear();
    const http_download_param_with_auth_t params = {
        .base = {
            .p_url                   = "https://example.com/data.json",
            .timeout_seconds         = 10,
            .flag_feed_task_watchdog = false,
            .flag_free_memory        = false,
            .p_server_cert           = NULL,
            .p_client_cert           = NULL,
            .p_client_key            = NULL,
        },
        .auth_type           = GW_CFG_HTTP_AUTH_TYPE_NONE,
        .p_http_auth         = NULL,
        .p_extra_header_item = NULL,
    };
    http_server_download_info_t info = http_download_json(&params);

    ASSERT_FALSE(info.is_error);
    ASSERT_EQ(HTTP_RESP_CODE_200, info.http_resp_code);
    ASSERT_NE(nullptr, info.p_json_buf);
    ASSERT_EQ(string(p_json_resp), string(info.p_json_buf));

    os_free(info.p_json_buf);
    os_malloc_trace_dump();
    TEST_CHECK_LOG_RECORD_HTTP_DOWNLOAD(ESP_LOG_INFO, "cb_on_http_download_json_data: buf_size=5");
    TEST_CHECK_LOG_RECORD_HTTP_DOWNLOAD(ESP_LOG_INFO, "Got HTTP error 301: Redirect to another location");
    TEST_CHECK_LOG_RECORD_HTTP_DOWNLOAD(ESP_LOG_INFO, "cb_on_http_download_json_data: buf_size=15");
    TEST_CHECK_LOG_RECORD_HTTP_DOWNLOAD(ESP_LOG_INFO, "http_download_json: completed within 0 ticks");
    ESP_LOG_WRAPPER_TEST_CHECK_LOG_RECORD("MEM_TRACE", ESP_LOG_INFO, "Num blocks allocated: 0");
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_EQ(0, this->m_alloc_free_call_count);
}

TEST_F(TestHttpServerCb, http_download_json__callback_redirect_302) // NOLINT
{
    const char* p_json_resp              = "{\"result\":\"ok\"}";
    this->m_mock_http_download_with_auth = [&](const http_download_param_with_auth_t* p_param,
                                               http_download_cb_on_data_t             p_cb_on_data,
                                               void*                                  p_user_data,
                                               bool flag_use_big_tls_buf) -> http_server_resp_t {
        const uint8_t redirect_data[] = "Found";
        p_cb_on_data(redirect_data, sizeof(redirect_data) - 1, 0, 0, HTTP_RESP_CODE_302, 0, p_user_data);
        const size_t len = strlen(p_json_resp);
        p_cb_on_data((const uint8_t*)p_json_resp, len, 0, len, HTTP_RESP_CODE_200, 0, p_user_data);
        const str_buf_t str_buf = str_buf_printf_with_alloc("%s", p_json_resp);
        return http_server_resp_200_json_in_heap(str_buf.buf);
    };

    esp_log_wrapper_clear();
    const http_download_param_with_auth_t params = {
        .base = {
            .p_url                   = "https://example.com/data.json",
            .timeout_seconds         = 10,
            .flag_feed_task_watchdog = false,
            .flag_free_memory        = false,
            .p_server_cert           = NULL,
            .p_client_cert           = NULL,
            .p_client_key            = NULL,
        },
        .auth_type           = GW_CFG_HTTP_AUTH_TYPE_NONE,
        .p_http_auth         = NULL,
        .p_extra_header_item = NULL,
    };
    http_server_download_info_t info = http_download_json(&params);

    ASSERT_FALSE(info.is_error);
    ASSERT_EQ(HTTP_RESP_CODE_200, info.http_resp_code);
    ASSERT_NE(nullptr, info.p_json_buf);
    ASSERT_EQ(string(p_json_resp), string(info.p_json_buf));

    os_free(info.p_json_buf);
    os_malloc_trace_dump();
    TEST_CHECK_LOG_RECORD_HTTP_DOWNLOAD(ESP_LOG_INFO, "cb_on_http_download_json_data: buf_size=5");
    TEST_CHECK_LOG_RECORD_HTTP_DOWNLOAD(ESP_LOG_INFO, "Got HTTP error 302: Redirect to another location");
    TEST_CHECK_LOG_RECORD_HTTP_DOWNLOAD(ESP_LOG_INFO, "cb_on_http_download_json_data: buf_size=15");
    TEST_CHECK_LOG_RECORD_HTTP_DOWNLOAD(ESP_LOG_INFO, "http_download_json: completed within 0 ticks");
    ESP_LOG_WRAPPER_TEST_CHECK_LOG_RECORD("MEM_TRACE", ESP_LOG_INFO, "Num blocks allocated: 0");
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_EQ(0, this->m_alloc_free_call_count);
}

TEST_F(TestHttpServerCb, http_download_json__callback_is_error_already_set) // NOLINT
{
    this->m_mock_http_download_with_auth = [&](const http_download_param_with_auth_t* p_param,
                                               http_download_cb_on_data_t             p_cb_on_data,
                                               void*                                  p_user_data,
                                               bool flag_use_big_tls_buf) -> http_server_resp_t {
        http_server_download_info_t* p_info = (http_server_download_info_t*)p_user_data;
        p_info->is_error                    = true;
        const uint8_t data[]                = "some data";
        const bool    result = p_cb_on_data(data, sizeof(data) - 1, 0, 9, HTTP_RESP_CODE_200, 0, p_user_data);
        EXPECT_FALSE(result);
        return http_server_resp_500();
    };

    esp_log_wrapper_clear();
    const http_download_param_with_auth_t params = {
        .base = {
            .p_url                   = "https://example.com/data.json",
            .timeout_seconds         = 10,
            .flag_feed_task_watchdog = false,
            .flag_free_memory        = false,
            .p_server_cert           = NULL,
            .p_client_cert           = NULL,
            .p_client_key            = NULL,
        },
        .auth_type           = GW_CFG_HTTP_AUTH_TYPE_NONE,
        .p_http_auth         = NULL,
        .p_extra_header_item = NULL,
    };
    http_server_download_info_t info = http_download_json(&params);

    ASSERT_TRUE(info.is_error);
    ASSERT_EQ(HTTP_RESP_CODE_500, info.http_resp_code);
    ASSERT_EQ(nullptr, info.p_json_buf);

    os_malloc_trace_dump();
    TEST_CHECK_LOG_RECORD_HTTP_DOWNLOAD(ESP_LOG_INFO, "cb_on_http_download_json_data: buf_size=9");
    TEST_CHECK_LOG_RECORD_HTTP_DOWNLOAD(ESP_LOG_ERROR, "Error occurred while downloading");
    TEST_CHECK_LOG_RECORD_HTTP_DOWNLOAD(ESP_LOG_ERROR, "Invalid content location: 0");
    TEST_CHECK_LOG_RECORD_HTTP_DOWNLOAD(
        ESP_LOG_ERROR,
        "http_download failed for URL: https://example.com/data.json, resp_code=500, content: <NULL>");
    TEST_CHECK_LOG_RECORD_HTTP_DOWNLOAD(ESP_LOG_INFO, "http_download_json: completed within 0 ticks");
    ESP_LOG_WRAPPER_TEST_CHECK_LOG_RECORD("MEM_TRACE", ESP_LOG_INFO, "Num blocks allocated: 0");
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_EQ(0, this->m_alloc_free_call_count);
}

TEST_F(TestHttpServerCb, http_download_json__callback_malloc_fail_first_alloc) // NOLINT
{
    this->m_mock_http_download_with_auth = [&](const http_download_param_with_auth_t* p_param,
                                               http_download_cb_on_data_t             p_cb_on_data,
                                               void*                                  p_user_data,
                                               bool flag_use_big_tls_buf) -> http_server_resp_t {
        g_pTestClass->m_malloc_fail_on_cnt = g_pTestClass->m_malloc_cnt + 1;
        const uint8_t data[]               = "{\"test\":1}";
        const bool    result
            = p_cb_on_data(data, sizeof(data) - 1, 0, sizeof(data) - 1, HTTP_RESP_CODE_200, 0, p_user_data);
        EXPECT_FALSE(result);
        // Return 200 with no content
        const http_server_resp_t resp = {
            .http_resp_code       = HTTP_RESP_CODE_200,
            .content_location     = HTTP_CONTENT_LOCATION_NO_CONTENT,
            .flag_no_cache        = true,
            .flag_add_header_date = true,
            .content_type         = HTTP_CONTENT_TYPE_TEXT_HTML,
            .p_content_type_param = NULL,
            .content_len          = 0,
            .content_encoding     = HTTP_CONTENT_ENCODING_NONE,
        };
        return resp;
    };

    esp_log_wrapper_clear();
    const http_download_param_with_auth_t params = {
        .base = {
            .p_url                   = "https://example.com/data.json",
            .timeout_seconds         = 10,
            .flag_feed_task_watchdog = false,
            .flag_free_memory        = false,
            .p_server_cert           = NULL,
            .p_client_cert           = NULL,
            .p_client_key            = NULL,
        },
        .auth_type           = GW_CFG_HTTP_AUTH_TYPE_NONE,
        .p_http_auth         = NULL,
        .p_extra_header_item = NULL,
    };
    http_server_download_info_t info = http_download_json(&params);

    ASSERT_TRUE(info.is_error);
    ASSERT_EQ(nullptr, info.p_json_buf);

    os_malloc_trace_dump();
    TEST_CHECK_LOG_RECORD_HTTP_DOWNLOAD(ESP_LOG_INFO, "cb_on_http_download_json_data: buf_size=10");
    TEST_CHECK_LOG_RECORD_HTTP_DOWNLOAD(ESP_LOG_ERROR, "Can't allocate 11 bytes for json file");
    TEST_CHECK_LOG_RECORD_HTTP_DOWNLOAD(ESP_LOG_ERROR, "http_download returned NULL buffer");
    TEST_CHECK_LOG_RECORD_HTTP_DOWNLOAD(ESP_LOG_INFO, "http_download_json: completed within 0 ticks");
    ESP_LOG_WRAPPER_TEST_CHECK_LOG_RECORD("MEM_TRACE", ESP_LOG_INFO, "Num blocks allocated: 0");
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_EQ(0, this->m_alloc_free_call_count);
}

TEST_F(TestHttpServerCb, http_download_json__callback_realloc_fail) // NOLINT
{
    const char* p_chunk1                 = "{\"key\":";
    const char* p_chunk2                 = "\"val\"}";
    this->m_mock_http_download_with_auth = [&](const http_download_param_with_auth_t* p_param,
                                               http_download_cb_on_data_t             p_cb_on_data,
                                               void*                                  p_user_data,
                                               bool flag_use_big_tls_buf) -> http_server_resp_t {
        const size_t len1 = strlen(p_chunk1);
        const size_t len2 = strlen(p_chunk2);
        // First chunk with content_length=0 (unknown)
        bool result = p_cb_on_data((const uint8_t*)p_chunk1, len1, 0, 0, HTTP_RESP_CODE_200, 0, p_user_data);
        EXPECT_TRUE(result);
        // Make next malloc (realloc) fail
        g_pTestClass->m_malloc_fail_on_cnt = g_pTestClass->m_malloc_cnt + 1;
        result = p_cb_on_data((const uint8_t*)p_chunk2, len2, len1, 0, HTTP_RESP_CODE_200, 0, p_user_data);
        EXPECT_FALSE(result);
        const http_server_resp_t resp = {
            .http_resp_code       = HTTP_RESP_CODE_200,
            .content_location     = HTTP_CONTENT_LOCATION_NO_CONTENT,
            .flag_no_cache        = true,
            .flag_add_header_date = true,
            .content_type         = HTTP_CONTENT_TYPE_TEXT_HTML,
            .p_content_type_param = NULL,
            .content_len          = 0,
            .content_encoding     = HTTP_CONTENT_ENCODING_NONE,
        };
        return resp;
    };

    esp_log_wrapper_clear();
    const http_download_param_with_auth_t params = {
        .base = {
            .p_url                   = "https://example.com/data.json",
            .timeout_seconds         = 10,
            .flag_feed_task_watchdog = false,
            .flag_free_memory        = false,
            .p_server_cert           = NULL,
            .p_client_cert           = NULL,
            .p_client_key            = NULL,
        },
        .auth_type           = GW_CFG_HTTP_AUTH_TYPE_NONE,
        .p_http_auth         = NULL,
        .p_extra_header_item = NULL,
    };
    http_server_download_info_t info = http_download_json(&params);

    ASSERT_TRUE(info.is_error);
    ASSERT_EQ(nullptr, info.p_json_buf);

    os_malloc_trace_dump();
    TEST_CHECK_LOG_RECORD_HTTP_DOWNLOAD(ESP_LOG_INFO, "cb_on_http_download_json_data: buf_size=7");
    TEST_CHECK_LOG_RECORD_HTTP_DOWNLOAD(ESP_LOG_INFO, "cb_on_http_download_json_data: buf_size=6");
    TEST_CHECK_LOG_RECORD_HTTP_DOWNLOAD(ESP_LOG_INFO, "Reallocate 14 bytes");
    TEST_CHECK_LOG_RECORD_HTTP_DOWNLOAD(ESP_LOG_ERROR, "Can't allocate 14 bytes for json file");
    TEST_CHECK_LOG_RECORD_HTTP_DOWNLOAD(ESP_LOG_ERROR, "http_download returned NULL buffer");
    TEST_CHECK_LOG_RECORD_HTTP_DOWNLOAD(ESP_LOG_INFO, "http_download_json: completed within 0 ticks");
    ESP_LOG_WRAPPER_TEST_CHECK_LOG_RECORD("MEM_TRACE", ESP_LOG_INFO, "Num blocks allocated: 0");
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_EQ(0, this->m_alloc_free_call_count);
}

TEST_F(TestHttpServerCb, http_download_json__callback_buffer_overflow) // NOLINT
{
    this->m_mock_http_download_with_auth = [&](const http_download_param_with_auth_t* p_param,
                                               http_download_cb_on_data_t             p_cb_on_data,
                                               void*                                  p_user_data,
                                               bool flag_use_big_tls_buf) -> http_server_resp_t {
        const char* p_data = "0123456789";
        // Allocate buffer for 5 byte content (content_length=5 -> buf_size=6)
        bool result = p_cb_on_data((const uint8_t*)p_data, 5, 0, 5, HTTP_RESP_CODE_200, 0, p_user_data);
        EXPECT_TRUE(result);
        // Now send more data at offset 5 with buf_size 5, offset+buf_size=10 >= json_buf_size(6)
        result = p_cb_on_data((const uint8_t*)p_data, 5, 5, 5, HTTP_RESP_CODE_200, 0, p_user_data);
        EXPECT_FALSE(result);
        const http_server_resp_t resp = {
            .http_resp_code       = HTTP_RESP_CODE_200,
            .content_location     = HTTP_CONTENT_LOCATION_NO_CONTENT,
            .flag_no_cache        = true,
            .flag_add_header_date = true,
            .content_type         = HTTP_CONTENT_TYPE_TEXT_HTML,
            .p_content_type_param = NULL,
            .content_len          = 0,
            .content_encoding     = HTTP_CONTENT_ENCODING_NONE,
        };
        return resp;
    };

    esp_log_wrapper_clear();
    const http_download_param_with_auth_t params = {
        .base = {
            .p_url                   = "https://example.com/data.json",
            .timeout_seconds         = 10,
            .flag_feed_task_watchdog = false,
            .flag_free_memory        = false,
            .p_server_cert           = NULL,
            .p_client_cert           = NULL,
            .p_client_key            = NULL,
        },
        .auth_type           = GW_CFG_HTTP_AUTH_TYPE_NONE,
        .p_http_auth         = NULL,
        .p_extra_header_item = NULL,
    };
    http_server_download_info_t info = http_download_json(&params);

    ASSERT_TRUE(info.is_error);
    ASSERT_EQ(nullptr, info.p_json_buf);

    os_malloc_trace_dump();
    TEST_CHECK_LOG_RECORD_HTTP_DOWNLOAD(ESP_LOG_INFO, "cb_on_http_download_json_data: buf_size=5");
    TEST_CHECK_LOG_RECORD_HTTP_DOWNLOAD(ESP_LOG_INFO, "cb_on_http_download_json_data: buf_size=5");
    TEST_CHECK_LOG_RECORD_HTTP_DOWNLOAD(
        ESP_LOG_ERROR,
        "Buffer overflow while downloading json file: json_buf_size=6, offset=5, len=5");
    TEST_CHECK_LOG_RECORD_HTTP_DOWNLOAD(ESP_LOG_ERROR, "http_download returned NULL buffer");
    TEST_CHECK_LOG_RECORD_HTTP_DOWNLOAD(ESP_LOG_INFO, "http_download_json: completed within 0 ticks");
    ESP_LOG_WRAPPER_TEST_CHECK_LOG_RECORD("MEM_TRACE", ESP_LOG_INFO, "Num blocks allocated: 0");
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_EQ(0, this->m_alloc_free_call_count);
}

TEST_F(TestHttpServerCb, http_download_json__callback_buffer_overflow_offset_exceeds_buf_size) // NOLINT
{
    this->m_mock_http_download_with_auth = [&](const http_download_param_with_auth_t* p_param,
                                               http_download_cb_on_data_t             p_cb_on_data,
                                               void*                                  p_user_data,
                                               bool flag_use_big_tls_buf) -> http_server_resp_t {
        const char* p_data = "ABCDE";
        // First alloc with content_length=5 (buf_size=6)
        bool result = p_cb_on_data((const uint8_t*)p_data, 3, 0, 5, HTTP_RESP_CODE_200, 0, p_user_data);
        EXPECT_TRUE(result);
        // offset=10 which is >= json_buf_size=6
        result = p_cb_on_data((const uint8_t*)p_data, 2, 10, 5, HTTP_RESP_CODE_200, 0, p_user_data);
        EXPECT_FALSE(result);
        const http_server_resp_t resp = {
            .http_resp_code       = HTTP_RESP_CODE_200,
            .content_location     = HTTP_CONTENT_LOCATION_NO_CONTENT,
            .flag_no_cache        = true,
            .flag_add_header_date = true,
            .content_type         = HTTP_CONTENT_TYPE_TEXT_HTML,
            .p_content_type_param = NULL,
            .content_len          = 0,
            .content_encoding     = HTTP_CONTENT_ENCODING_NONE,
        };
        return resp;
    };

    esp_log_wrapper_clear();
    const http_download_param_with_auth_t params = {
        .base = {
            .p_url                   = "https://example.com/data.json",
            .timeout_seconds         = 10,
            .flag_feed_task_watchdog = false,
            .flag_free_memory        = false,
            .p_server_cert           = NULL,
            .p_client_cert           = NULL,
            .p_client_key            = NULL,
        },
        .auth_type           = GW_CFG_HTTP_AUTH_TYPE_NONE,
        .p_http_auth         = NULL,
        .p_extra_header_item = NULL,
    };
    http_server_download_info_t info = http_download_json(&params);

    ASSERT_TRUE(info.is_error);
    ASSERT_EQ(nullptr, info.p_json_buf);

    os_malloc_trace_dump();
    TEST_CHECK_LOG_RECORD_HTTP_DOWNLOAD(ESP_LOG_INFO, "cb_on_http_download_json_data: buf_size=3");
    TEST_CHECK_LOG_RECORD_HTTP_DOWNLOAD(ESP_LOG_INFO, "cb_on_http_download_json_data: buf_size=2");
    TEST_CHECK_LOG_RECORD_HTTP_DOWNLOAD(
        ESP_LOG_ERROR,
        "Buffer overflow while downloading json file: json_buf_size=6, offset=10, len=2");
    TEST_CHECK_LOG_RECORD_HTTP_DOWNLOAD(ESP_LOG_ERROR, "http_download returned NULL buffer");
    TEST_CHECK_LOG_RECORD_HTTP_DOWNLOAD(ESP_LOG_INFO, "http_download_json: completed within 0 ticks");
    ESP_LOG_WRAPPER_TEST_CHECK_LOG_RECORD("MEM_TRACE", ESP_LOG_INFO, "Num blocks allocated: 0");
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_EQ(0, this->m_alloc_free_call_count);
}

TEST_F(TestHttpServerCb, http_download_json__resp_not_200_with_heap_content) // NOLINT
{
    this->m_mock_http_download_with_auth = [&](const http_download_param_with_auth_t* p_param,
                                               http_download_cb_on_data_t             p_cb_on_data,
                                               void*                                  p_user_data,
                                               bool flag_use_big_tls_buf) -> http_server_resp_t {
        // Don't call the callback, just return 502 with error content in heap
        const str_buf_t str_buf = str_buf_printf_with_alloc("{\"error\":\"bad gateway\"}");
        return http_server_resp_json_in_heap(HTTP_RESP_CODE_502, str_buf.buf);
    };

    esp_log_wrapper_clear();
    const http_download_param_with_auth_t params = {
        .base = {
            .p_url                   = "https://example.com/data.json",
            .timeout_seconds         = 10,
            .flag_feed_task_watchdog = false,
            .flag_free_memory        = false,
            .p_server_cert           = NULL,
            .p_client_cert           = NULL,
            .p_client_key            = NULL,
        },
        .auth_type           = GW_CFG_HTTP_AUTH_TYPE_NONE,
        .p_http_auth         = NULL,
        .p_extra_header_item = NULL,
    };
    http_server_download_info_t info = http_download_json(&params);

    ASSERT_TRUE(info.is_error);
    ASSERT_EQ(HTTP_RESP_CODE_502, info.http_resp_code);
    ASSERT_NE(nullptr, info.p_json_buf);
    ASSERT_EQ(string("{\"error\":\"bad gateway\"}"), string(info.p_json_buf));

    os_free(info.p_json_buf);
    os_malloc_trace_dump();
    TEST_CHECK_LOG_RECORD_HTTP_DOWNLOAD(
        ESP_LOG_ERROR,
        "http_download failed for URL: https://example.com/data.json, resp_code=502, "
        "content: {\"error\":\"bad gateway\"}");
    TEST_CHECK_LOG_RECORD_HTTP_DOWNLOAD(ESP_LOG_INFO, "http_download_json: completed within 0 ticks");
    ESP_LOG_WRAPPER_TEST_CHECK_LOG_RECORD("MEM_TRACE", ESP_LOG_INFO, "Num blocks allocated: 0");
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_EQ(0, this->m_alloc_free_call_count);
}

TEST_F(TestHttpServerCb, http_download_json__resp_not_200_with_heap_content_and_callback_had_json) // NOLINT
{
    this->m_mock_http_download_with_auth = [&](const http_download_param_with_auth_t* p_param,
                                               http_download_cb_on_data_t             p_cb_on_data,
                                               void*                                  p_user_data,
                                               bool flag_use_big_tls_buf) -> http_server_resp_t {
        // Callback receives some data (allocating info.p_json_buf)
        const char* p_data = "{\"partial\":true}";
        p_cb_on_data((const uint8_t*)p_data, strlen(p_data), 0, strlen(p_data), HTTP_RESP_CODE_200, 0, p_user_data);
        // But return 502 with error content in heap — should free the callback's json_buf
        const str_buf_t str_buf = str_buf_printf_with_alloc("{\"error\":\"bad gateway\"}");
        return http_server_resp_json_in_heap(HTTP_RESP_CODE_502, str_buf.buf);
    };

    esp_log_wrapper_clear();
    const http_download_param_with_auth_t params = {
        .base = {
            .p_url                   = "https://example.com/data.json",
            .timeout_seconds         = 10,
            .flag_feed_task_watchdog = false,
            .flag_free_memory        = false,
            .p_server_cert           = NULL,
            .p_client_cert           = NULL,
            .p_client_key            = NULL,
        },
        .auth_type           = GW_CFG_HTTP_AUTH_TYPE_NONE,
        .p_http_auth         = NULL,
        .p_extra_header_item = NULL,
    };
    http_server_download_info_t info = http_download_json(&params);

    ASSERT_TRUE(info.is_error);
    ASSERT_EQ(HTTP_RESP_CODE_502, info.http_resp_code);
    ASSERT_NE(nullptr, info.p_json_buf);
    ASSERT_EQ(string("{\"error\":\"bad gateway\"}"), string(info.p_json_buf));

    os_free(info.p_json_buf);
    os_malloc_trace_dump();
    TEST_CHECK_LOG_RECORD_HTTP_DOWNLOAD(ESP_LOG_INFO, "cb_on_http_download_json_data: buf_size=16");
    TEST_CHECK_LOG_RECORD_HTTP_DOWNLOAD(
        ESP_LOG_ERROR,
        "http_download failed for URL: https://example.com/data.json, resp_code=502, "
        "content: {\"error\":\"bad gateway\"}");
    TEST_CHECK_LOG_RECORD_HTTP_DOWNLOAD(ESP_LOG_INFO, "http_download_json: completed within 0 ticks");
    ESP_LOG_WRAPPER_TEST_CHECK_LOG_RECORD("MEM_TRACE", ESP_LOG_INFO, "Num blocks allocated: 0");
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_EQ(0, this->m_alloc_free_call_count);
}

TEST_F(TestHttpServerCb, http_download_json__resp_not_200_no_content_no_json_buf) // NOLINT
{
    this->m_mock_http_download_with_auth = [&](const http_download_param_with_auth_t* p_param,
                                               http_download_cb_on_data_t             p_cb_on_data,
                                               void*                                  p_user_data,
                                               bool flag_use_big_tls_buf) -> http_server_resp_t {
        // Return 500 with no content at all
        return http_server_resp_500();
    };

    esp_log_wrapper_clear();
    const http_download_param_with_auth_t params = {
        .base = {
            .p_url                   = "https://example.com/data.json",
            .timeout_seconds         = 10,
            .flag_feed_task_watchdog = false,
            .flag_free_memory        = false,
            .p_server_cert           = NULL,
            .p_client_cert           = NULL,
            .p_client_key            = NULL,
        },
        .auth_type           = GW_CFG_HTTP_AUTH_TYPE_NONE,
        .p_http_auth         = NULL,
        .p_extra_header_item = NULL,
    };
    http_server_download_info_t info = http_download_json(&params);

    ASSERT_TRUE(info.is_error);
    ASSERT_EQ(HTTP_RESP_CODE_500, info.http_resp_code);
    ASSERT_EQ(nullptr, info.p_json_buf);

    os_malloc_trace_dump();
    TEST_CHECK_LOG_RECORD_HTTP_DOWNLOAD(ESP_LOG_ERROR, "Invalid content location: 0");
    TEST_CHECK_LOG_RECORD_HTTP_DOWNLOAD(
        ESP_LOG_ERROR,
        "http_download failed for URL: https://example.com/data.json, resp_code=500, content: <NULL>");
    TEST_CHECK_LOG_RECORD_HTTP_DOWNLOAD(ESP_LOG_INFO, "http_download_json: completed within 0 ticks");
    ESP_LOG_WRAPPER_TEST_CHECK_LOG_RECORD("MEM_TRACE", ESP_LOG_INFO, "Num blocks allocated: 0");
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_EQ(0, this->m_alloc_free_call_count);
}

TEST_F(TestHttpServerCb, http_download_json__resp_not_200_no_content_with_json_buf_from_callback) // NOLINT
{
    this->m_mock_http_download_with_auth = [&](const http_download_param_with_auth_t* p_param,
                                               http_download_cb_on_data_t             p_cb_on_data,
                                               void*                                  p_user_data,
                                               bool flag_use_big_tls_buf) -> http_server_resp_t {
        // Callback receives data first
        const char* p_data = "{\"data\":1}";
        p_cb_on_data((const uint8_t*)p_data, strlen(p_data), 0, strlen(p_data), HTTP_RESP_CODE_200, 0, p_user_data);
        // Return 500 with no content (NO_CONTENT location)
        return http_server_resp_500();
    };

    esp_log_wrapper_clear();
    const http_download_param_with_auth_t params = {
        .base = {
            .p_url                   = "https://example.com/data.json",
            .timeout_seconds         = 10,
            .flag_feed_task_watchdog = false,
            .flag_free_memory        = false,
            .p_server_cert           = NULL,
            .p_client_cert           = NULL,
            .p_client_key            = NULL,
        },
        .auth_type           = GW_CFG_HTTP_AUTH_TYPE_NONE,
        .p_http_auth         = NULL,
        .p_extra_header_item = NULL,
    };
    http_server_download_info_t info = http_download_json(&params);

    ASSERT_TRUE(info.is_error);
    ASSERT_EQ(HTTP_RESP_CODE_500, info.http_resp_code);
    // The json_buf from callback is still present (not freed in this branch)
    ASSERT_NE(nullptr, info.p_json_buf);
    ASSERT_EQ(string("{\"data\":1}"), string(info.p_json_buf));

    os_free(info.p_json_buf);
    os_malloc_trace_dump();
    TEST_CHECK_LOG_RECORD_HTTP_DOWNLOAD(ESP_LOG_INFO, "cb_on_http_download_json_data: buf_size=10");
    TEST_CHECK_LOG_RECORD_HTTP_DOWNLOAD(ESP_LOG_ERROR, "Invalid content location: 0");
    TEST_CHECK_LOG_RECORD_HTTP_DOWNLOAD(
        ESP_LOG_ERROR,
        "http_download failed for URL: https://example.com/data.json, resp_code=500, content: {\"data\":1}");
    TEST_CHECK_LOG_RECORD_HTTP_DOWNLOAD(ESP_LOG_INFO, "http_download_json: completed within 0 ticks");
    ESP_LOG_WRAPPER_TEST_CHECK_LOG_RECORD("MEM_TRACE", ESP_LOG_INFO, "Num blocks allocated: 0");
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_EQ(0, this->m_alloc_free_call_count);
}

TEST_F(TestHttpServerCb, http_download_json__resp_200_but_info_resp_code_not_200_with_json_buf) // NOLINT
{
    this->m_mock_http_download_with_auth = [&](const http_download_param_with_auth_t* p_param,
                                               http_download_cb_on_data_t             p_cb_on_data,
                                               void*                                  p_user_data,
                                               bool flag_use_big_tls_buf) -> http_server_resp_t {
        // Callback gets data with non-200 resp code (e.g. 404)
        const char* p_data = "{\"error\":\"not found\"}";
        p_cb_on_data((const uint8_t*)p_data, strlen(p_data), 0, strlen(p_data), HTTP_RESP_CODE_404, 0, p_user_data);
        // But main resp is 200
        const str_buf_t str_buf = str_buf_printf_with_alloc("{}");
        return http_server_resp_200_json_in_heap(str_buf.buf);
    };

    esp_log_wrapper_clear();
    const http_download_param_with_auth_t params = {
        .base = {
            .p_url                   = "https://example.com/data.json",
            .timeout_seconds         = 10,
            .flag_feed_task_watchdog = false,
            .flag_free_memory        = false,
            .p_server_cert           = NULL,
            .p_client_cert           = NULL,
            .p_client_key            = NULL,
        },
        .auth_type           = GW_CFG_HTTP_AUTH_TYPE_NONE,
        .p_http_auth         = NULL,
        .p_extra_header_item = NULL,
    };
    http_server_download_info_t info = http_download_json(&params);

    ASSERT_TRUE(info.is_error);
    ASSERT_EQ(HTTP_RESP_CODE_404, info.http_resp_code);
    ASSERT_NE(nullptr, info.p_json_buf);
    ASSERT_EQ(string("{\"error\":\"not found\"}"), string(info.p_json_buf));

    os_free(info.p_json_buf);
    os_malloc_trace_dump();
    TEST_CHECK_LOG_RECORD_HTTP_DOWNLOAD(ESP_LOG_INFO, "cb_on_http_download_json_data: buf_size=21");
    TEST_CHECK_LOG_RECORD_HTTP_DOWNLOAD(
        ESP_LOG_ERROR,
        "http_download failed, HTTP resp code 404: {\"error\":\"not found\"}");
    TEST_CHECK_LOG_RECORD_HTTP_DOWNLOAD(ESP_LOG_INFO, "http_download_json: completed within 0 ticks");
    ESP_LOG_WRAPPER_TEST_CHECK_LOG_RECORD("MEM_TRACE", ESP_LOG_INFO, "Num blocks allocated: 0");
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_EQ(0, this->m_alloc_free_call_count);
}

TEST_F(TestHttpServerCb, http_download_json__resp_200_but_info_resp_code_not_200_no_json_buf) // NOLINT
{
    this->m_mock_http_download_with_auth = [&](const http_download_param_with_auth_t* p_param,
                                               http_download_cb_on_data_t             p_cb_on_data,
                                               void*                                  p_user_data,
                                               bool flag_use_big_tls_buf) -> http_server_resp_t {
        // Simulate: callback sets non-200 resp code but no json_buf
        http_server_download_info_t* p_info = (http_server_download_info_t*)p_user_data;
        p_info->http_resp_code              = HTTP_RESP_CODE_503;
        // Don't allocate any json_buf
        // Return 200 resp
        const str_buf_t str_buf = str_buf_printf_with_alloc("{}");
        return http_server_resp_200_json_in_heap(str_buf.buf);
    };

    esp_log_wrapper_clear();
    const http_download_param_with_auth_t params = {
        .base = {
            .p_url                   = "https://example.com/data.json",
            .timeout_seconds         = 10,
            .flag_feed_task_watchdog = false,
            .flag_free_memory        = false,
            .p_server_cert           = NULL,
            .p_client_cert           = NULL,
            .p_client_key            = NULL,
        },
        .auth_type           = GW_CFG_HTTP_AUTH_TYPE_NONE,
        .p_http_auth         = NULL,
        .p_extra_header_item = NULL,
    };
    http_server_download_info_t info = http_download_json(&params);

    ASSERT_TRUE(info.is_error);
    ASSERT_EQ(HTTP_RESP_CODE_503, info.http_resp_code);
    ASSERT_EQ(nullptr, info.p_json_buf);

    os_malloc_trace_dump();
    TEST_CHECK_LOG_RECORD_HTTP_DOWNLOAD(ESP_LOG_ERROR, "http_download failed, HTTP resp code 503");
    TEST_CHECK_LOG_RECORD_HTTP_DOWNLOAD(ESP_LOG_INFO, "http_download_json: completed within 0 ticks");
    ESP_LOG_WRAPPER_TEST_CHECK_LOG_RECORD("MEM_TRACE", ESP_LOG_INFO, "Num blocks allocated: 0");
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_EQ(0, this->m_alloc_free_call_count);
}

TEST_F(TestHttpServerCb, http_download_json__resp_200_but_null_json_buf) // NOLINT
{
    this->m_mock_http_download_with_auth = [&](const http_download_param_with_auth_t* p_param,
                                               http_download_cb_on_data_t             p_cb_on_data,
                                               void*                                  p_user_data,
                                               bool flag_use_big_tls_buf) -> http_server_resp_t {
        // Don't call callback at all — no data received
        const str_buf_t str_buf = str_buf_printf_with_alloc("{}");
        return http_server_resp_200_json_in_heap(str_buf.buf);
    };

    esp_log_wrapper_clear();
    const http_download_param_with_auth_t params = {
        .base = {
            .p_url                   = "https://example.com/data.json",
            .timeout_seconds         = 10,
            .flag_feed_task_watchdog = false,
            .flag_free_memory        = false,
            .p_server_cert           = NULL,
            .p_client_cert           = NULL,
            .p_client_key            = NULL,
        },
        .auth_type           = GW_CFG_HTTP_AUTH_TYPE_NONE,
        .p_http_auth         = NULL,
        .p_extra_header_item = NULL,
    };
    http_server_download_info_t info = http_download_json(&params);

    ASSERT_TRUE(info.is_error);
    // is_error with http_resp_code still 200 -> changed to 400
    ASSERT_EQ(HTTP_RESP_CODE_400, info.http_resp_code);
    ASSERT_EQ(nullptr, info.p_json_buf);

    os_malloc_trace_dump();
    TEST_CHECK_LOG_RECORD_HTTP_DOWNLOAD(ESP_LOG_ERROR, "http_download returned NULL buffer");
    TEST_CHECK_LOG_RECORD_HTTP_DOWNLOAD(ESP_LOG_INFO, "http_download_json: completed within 0 ticks");
    ESP_LOG_WRAPPER_TEST_CHECK_LOG_RECORD("MEM_TRACE", ESP_LOG_INFO, "Num blocks allocated: 0");
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_EQ(0, this->m_alloc_free_call_count);
}

TEST_F(TestHttpServerCb, http_download_json__callback_two_chunks_known_content_length) // NOLINT
{
    // Two chunks with known content_length — second chunk should NOT trigger realloc
    const char*  p_chunk1                = "{\"k\":";
    const char*  p_chunk2                = "\"v\"}";
    const size_t total_len               = strlen(p_chunk1) + strlen(p_chunk2);
    this->m_mock_http_download_with_auth = [&](const http_download_param_with_auth_t* p_param,
                                               http_download_cb_on_data_t             p_cb_on_data,
                                               void*                                  p_user_data,
                                               bool flag_use_big_tls_buf) -> http_server_resp_t {
        const size_t len1 = strlen(p_chunk1);
        const size_t len2 = strlen(p_chunk2);
        p_cb_on_data((const uint8_t*)p_chunk1, len1, 0, total_len, HTTP_RESP_CODE_200, 0, p_user_data);
        p_cb_on_data((const uint8_t*)p_chunk2, len2, len1, total_len, HTTP_RESP_CODE_200, 0, p_user_data);
        const str_buf_t str_buf = str_buf_printf_with_alloc("{\"k\":\"v\"}");
        return http_server_resp_200_json_in_heap(str_buf.buf);
    };

    esp_log_wrapper_clear();
    const http_download_param_with_auth_t params = {
        .base = {
            .p_url                   = "https://example.com/data.json",
            .timeout_seconds         = 10,
            .flag_feed_task_watchdog = false,
            .flag_free_memory        = false,
            .p_server_cert           = NULL,
            .p_client_cert           = NULL,
            .p_client_key            = NULL,
        },
        .auth_type           = GW_CFG_HTTP_AUTH_TYPE_NONE,
        .p_http_auth         = NULL,
        .p_extra_header_item = NULL,
    };
    http_server_download_info_t info = http_download_json(&params);

    ASSERT_FALSE(info.is_error);
    ASSERT_EQ(HTTP_RESP_CODE_200, info.http_resp_code);
    ASSERT_NE(nullptr, info.p_json_buf);
    ASSERT_EQ(string("{\"k\":\"v\"}"), string(info.p_json_buf));

    os_free(info.p_json_buf);
    os_malloc_trace_dump();
    TEST_CHECK_LOG_RECORD_HTTP_DOWNLOAD(ESP_LOG_INFO, "cb_on_http_download_json_data: buf_size=5");
    TEST_CHECK_LOG_RECORD_HTTP_DOWNLOAD(ESP_LOG_INFO, "cb_on_http_download_json_data: buf_size=4");
    TEST_CHECK_LOG_RECORD_HTTP_DOWNLOAD(ESP_LOG_INFO, "http_download_json: completed within 0 ticks");
    ESP_LOG_WRAPPER_TEST_CHECK_LOG_RECORD("MEM_TRACE", ESP_LOG_INFO, "Num blocks allocated: 0");
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_EQ(0, this->m_alloc_free_call_count);
}

TEST_F(TestHttpServerCb, http_download_firmware_update_info__ok) // NOLINT
{
    const char* p_json_resp = "{\"latest\":{\"version\":\"v1.15.0\",\"url\":\"https://fwupdate.ruuvi.com/v1.15.0\"}}";
    this->m_mock_http_download_with_auth = [&](const http_download_param_with_auth_t* p_param,
                                               http_download_cb_on_data_t             p_cb_on_data,
                                               void*                                  p_user_data,
                                               bool flag_use_big_tls_buf) -> http_server_resp_t {
        EXPECT_EQ(string("https://network.ruuvi.com/firmwareupdate"), string(p_param->base.p_url));
        EXPECT_EQ(30, p_param->base.timeout_seconds);
        EXPECT_TRUE(p_param->base.flag_feed_task_watchdog);
        EXPECT_FALSE(flag_use_big_tls_buf);
        const size_t len = strlen(p_json_resp);
        p_cb_on_data((const uint8_t*)p_json_resp, len, 0, len, HTTP_RESP_CODE_200, 0, p_user_data);
        const str_buf_t str_buf = str_buf_printf_with_alloc("%s", p_json_resp);
        return http_server_resp_200_json_in_heap(str_buf.buf);
    };

    esp_log_wrapper_clear();
    http_server_download_info_t info = http_download_firmware_update_info(
        "https://network.ruuvi.com/firmwareupdate",
        false);

    ASSERT_FALSE(info.is_error);
    ASSERT_EQ(HTTP_RESP_CODE_200, info.http_resp_code);
    ASSERT_NE(nullptr, info.p_json_buf);
    ASSERT_EQ(string(p_json_resp), string(info.p_json_buf));

    os_free(info.p_json_buf);
    os_malloc_trace_dump();
    TEST_CHECK_LOG_RECORD_HTTP_DOWNLOAD(
        ESP_LOG_INFO,
        string("cb_on_http_download_json_data: buf_size=") + to_string(strlen(p_json_resp)));
    TEST_CHECK_LOG_RECORD_HTTP_DOWNLOAD(ESP_LOG_INFO, "http_download_json: completed within 0 ticks");
    ESP_LOG_WRAPPER_TEST_CHECK_LOG_RECORD("MEM_TRACE", ESP_LOG_INFO, "Num blocks allocated: 0");
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_EQ(0, this->m_alloc_free_call_count);
}
/* ============================================================================
 * Tests for http_server_resp_json_firmware_update
 *
 * Cover the time-seeding logic added for issue #1288: when the gateway's clock
 * is not synchronised (NTP disabled, or NTP enabled but no successful sync),
 * the X-Request-Timestamp value from the web UI (exposed via
 * http_server_get_request_timestamp()) is applied via settimeofday() before
 * the outbound HTTPS request to the firmware-update server.
 * ============================================================================
 */
extern "C" http_server_resp_t
http_server_resp_json_firmware_update(void);
namespace
{
const char* const g_test_fw_update_json
    = "{\"latest\":{\"version\":\"v1.15.0\",\"url\":\"https://fwupdate.ruuvi.com/v1.15.0\"}}";
void
set_ntp_use(const bool ntp_use)
{
    gw_cfg_t gw_cfg = {};
    gw_cfg_default_get(&gw_cfg);
    gw_cfg.ruuvi_cfg.ntp.ntp_use = ntp_use;
    gw_cfg_update(&gw_cfg);
}
} // namespace
TEST_F(TestHttpServerCb, json_firmware_update__ntp_enabled_and_time_synchronized__no_settimeofday) // NOLINT
{
    set_ntp_use(true);
    this->m_time_is_synchronized = true;
    this->m_request_timestamp    = 1735689600; // 2025-01-01 00:00:00 UTC
    this->m_firmware_update_resp = str_buf_printf_with_alloc("%s", g_test_fw_update_json);
    esp_log_wrapper_clear();
    http_server_resp_t resp = http_server_resp_json_firmware_update();
    ASSERT_EQ(HTTP_RESP_CODE_200, resp.http_resp_code);
    ASSERT_EQ(HTTP_CONTENT_LOCATION_HEAP, resp.content_location);
    ASSERT_EQ(HTTP_CONTENT_TYPE_APPLICATION_JSON, resp.content_type);
    ASSERT_NE(nullptr, resp.select_location.heap.p_buf);
    ASSERT_EQ(string(g_test_fw_update_json), string(reinterpret_cast<const char*>(resp.select_location.heap.p_buf)));
    // X-Request-Timestamp must NOT be applied when the clock is already in sync.
    ASSERT_EQ(0U, this->m_settimeofday_call_cnt);
    ASSERT_EQ(
        "I http_download: cb_on_http_download_json_data: buf_size=" + to_string(strlen(g_test_fw_update_json)) + "\n"
        "I http_download: http_download_json: completed within 0 ticks\n"
        "I http_server: Firmware update info (json): " + string(g_test_fw_update_json) + "\n",
        esp_log_wrapper_get_logs());
    http_server_resp_free(&resp);
    os_malloc_trace_dump();
    ESP_LOG_WRAPPER_TEST_CHECK_LOG_RECORD("MEM_TRACE", ESP_LOG_INFO, "Num blocks allocated: 0");
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_EQ(0, this->m_alloc_free_call_count);
}
TEST_F(TestHttpServerCb, json_firmware_update__ntp_disabled_no_request_timestamp__no_settimeofday) // NOLINT
{
    set_ntp_use(false);
    this->m_time_is_synchronized = false;
    this->m_request_timestamp    = 0; // header absent -> exposed as 0
    this->m_firmware_update_resp = str_buf_printf_with_alloc("%s", g_test_fw_update_json);
    esp_log_wrapper_clear();
    http_server_resp_t resp = http_server_resp_json_firmware_update();
    ASSERT_EQ(HTTP_RESP_CODE_200, resp.http_resp_code);
    ASSERT_EQ(HTTP_CONTENT_LOCATION_HEAP, resp.content_location);
    ASSERT_EQ(string(g_test_fw_update_json), string(reinterpret_cast<const char*>(resp.select_location.heap.p_buf)));
    // No header timestamp -> settimeofday must not be called.
    ASSERT_EQ(0U, this->m_settimeofday_call_cnt);
    ASSERT_EQ(
        "I http_download: cb_on_http_download_json_data: buf_size=" + to_string(strlen(g_test_fw_update_json)) + "\n"
        "I http_download: http_download_json: completed within 0 ticks\n"
        "I http_server: Firmware update info (json): " + string(g_test_fw_update_json) + "\n",
        esp_log_wrapper_get_logs());
    http_server_resp_free(&resp);
    os_malloc_trace_dump();
    ESP_LOG_WRAPPER_TEST_CHECK_LOG_RECORD("MEM_TRACE", ESP_LOG_INFO, "Num blocks allocated: 0");
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_EQ(0, this->m_alloc_free_call_count);
}
TEST_F(TestHttpServerCb, json_firmware_update__ntp_disabled_with_request_timestamp__settimeofday_called) // NOLINT
{
    set_ntp_use(false);
    this->m_time_is_synchronized = false;
    this->m_request_timestamp    = 1735689600; // 2025-01-01 00:00:00 UTC
    this->m_firmware_update_resp = str_buf_printf_with_alloc("%s", g_test_fw_update_json);
    esp_log_wrapper_clear();
    http_server_resp_t resp = http_server_resp_json_firmware_update();
    ASSERT_EQ(HTTP_RESP_CODE_200, resp.http_resp_code);
    // Clock seeding must occur with the value from the X-Request-Timestamp header.
    ASSERT_EQ(1U, this->m_settimeofday_call_cnt);
    ASSERT_EQ(1735689600, this->m_settimeofday_tv.tv_sec);
    ASSERT_EQ(0, this->m_settimeofday_tv.tv_usec);
    ASSERT_EQ(
        "I http_server: Set system time to 1735689600\n"
        "I http_download: cb_on_http_download_json_data: buf_size=" + to_string(strlen(g_test_fw_update_json)) + "\n"
        "I http_download: http_download_json: completed within 0 ticks\n"
        "I http_server: Firmware update info (json): " + string(g_test_fw_update_json) + "\n",
        esp_log_wrapper_get_logs());
    http_server_resp_free(&resp);
    os_malloc_trace_dump();
    ESP_LOG_WRAPPER_TEST_CHECK_LOG_RECORD("MEM_TRACE", ESP_LOG_INFO, "Num blocks allocated: 0");
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_EQ(0, this->m_alloc_free_call_count);
}
TEST_F(TestHttpServerCb, json_firmware_update__ntp_enabled_but_not_synchronized__settimeofday_called) // NOLINT
{
    set_ntp_use(true);
    this->m_time_is_synchronized = false;      // NTP enabled but clock not yet in sync
    this->m_request_timestamp    = 1748908800; // 2025-06-03 00:00:00 UTC
    this->m_firmware_update_resp = str_buf_printf_with_alloc("%s", g_test_fw_update_json);
    esp_log_wrapper_clear();
    http_server_resp_t resp = http_server_resp_json_firmware_update();
    ASSERT_EQ(HTTP_RESP_CODE_200, resp.http_resp_code);
    // The clock has not been synchronised yet, so the header value must be applied.
    ASSERT_EQ(1U, this->m_settimeofday_call_cnt);
    ASSERT_EQ(1748908800, this->m_settimeofday_tv.tv_sec);
    ASSERT_EQ(
        "I http_server: Set system time to 1748908800\n"
        "I http_download: cb_on_http_download_json_data: buf_size=" + to_string(strlen(g_test_fw_update_json)) + "\n"
        "I http_download: http_download_json: completed within 0 ticks\n"
        "I http_server: Firmware update info (json): " + string(g_test_fw_update_json) + "\n",
        esp_log_wrapper_get_logs());
    http_server_resp_free(&resp);
    os_malloc_trace_dump();
    ESP_LOG_WRAPPER_TEST_CHECK_LOG_RECORD("MEM_TRACE", ESP_LOG_INFO, "Num blocks allocated: 0");
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_EQ(0, this->m_alloc_free_call_count);
}
TEST_F(TestHttpServerCb, json_firmware_update__download_error_propagated_504) // NOLINT
{
    set_ntp_use(false);
    this->m_time_is_synchronized = false;
    this->m_request_timestamp    = 0;
    // Override gw_cfg fw_update_url so the default mock returns http_server_resp_400() (is_error).
    {
        gw_cfg_t gw_cfg = {};
        gw_cfg_default_get(&gw_cfg);
        (void)snprintf(
            gw_cfg.ruuvi_cfg.fw_update.fw_update_url,
            sizeof(gw_cfg.ruuvi_cfg.fw_update.fw_update_url),
            "%s",
            "https://unknown.server/firmwareupdate");
        gw_cfg_update(&gw_cfg);
    }
    esp_log_wrapper_clear();
    http_server_resp_t resp = http_server_resp_json_firmware_update();
    ASSERT_EQ(HTTP_RESP_CODE_504, resp.http_resp_code);
    ASSERT_EQ(0U, this->m_settimeofday_call_cnt);
    esp_log_wrapper_clear();
    http_server_resp_free(&resp);
    os_malloc_trace_dump();
    ESP_LOG_WRAPPER_TEST_CHECK_LOG_RECORD("MEM_TRACE", ESP_LOG_INFO, "Num blocks allocated: 0");
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_EQ(0, this->m_alloc_free_call_count);
}
TEST_F(TestHttpServerCb, json_firmware_update__settimeofday_failure_logs_error) // NOLINT
{
    set_ntp_use(false);
    this->m_time_is_synchronized = false;
    this->m_request_timestamp    = 1735689600; // 2025-01-01 00:00:00 UTC
    this->m_settimeofday_fail    = true;
    this->m_firmware_update_resp = str_buf_printf_with_alloc("%s", g_test_fw_update_json);
    esp_log_wrapper_clear();
    http_server_resp_t resp = http_server_resp_json_firmware_update();
    ASSERT_EQ(HTTP_RESP_CODE_200, resp.http_resp_code);
    // settimeofday was attempted but failed -> success log must NOT be present,
    // error log must be present.
    ASSERT_EQ(1U, this->m_settimeofday_call_cnt);
    ASSERT_EQ(1735689600, this->m_settimeofday_tv.tv_sec);
    ASSERT_EQ(
        "E http_server: Failed to set system time to 1735689600\n"
        "I http_download: cb_on_http_download_json_data: buf_size=" + to_string(strlen(g_test_fw_update_json)) + "\n"
        "I http_download: http_download_json: completed within 0 ticks\n"
        "I http_server: Firmware update info (json): " + string(g_test_fw_update_json) + "\n",
        esp_log_wrapper_get_logs());
    http_server_resp_free(&resp);
    os_malloc_trace_dump();
    ESP_LOG_WRAPPER_TEST_CHECK_LOG_RECORD("MEM_TRACE", ESP_LOG_INFO, "Num blocks allocated: 0");
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_EQ(0, this->m_alloc_free_call_count);
}
