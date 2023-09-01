/**
 * @file test_bin2hex.cpp
 * @author TheSomeMan
 * @date 2020-08-27
 * @copyright Ruuvi Innovations Ltd, license BSD-3-Clause.
 */

#include "http_server_cb.h"
#include <cstring>
#include <utility>
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

http_server_resp_t
http_download_with_auth(
    const http_download_param_with_auth_t* const p_param,
    http_download_cb_on_data_t const             p_cb_on_data,
    void* const                                  p_user_data)
{
    return http_server_resp_400();
}

bool
http_check_with_auth(const http_download_param_with_auth_t* const p_param, http_resp_code_e* const p_http_resp_code)
{
    return false;
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
fw_update_is_url_valid(void)
{
    return true;
}

bool
json_fw_update_parse_http_body(const char* const p_body)
{
    return false;
}

str_buf_t
json_fw_update_url_parse_http_body_get_url(const char* const p_body)
{
    return str_buf_init_null();
}

bool
fw_update_run(const fw_updating_reason_e fw_updating_reason)
{
    return true;
}

void
adv_post_disable_retransmission(void)
{
}

void
adv_post_enable_retransmission(void)
{
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
adv_post_last_successful_network_comm_timestamp_update(void)
{
}

void
settings_save_to_flash(const gw_cfg_t* const p_gw_cfg);

bool
time_is_synchronized(void)
{
    return true;
}

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
    const http_server_resp_t resp = {
        .http_resp_code       = HTTP_RESP_CODE_500,
        .content_location     = HTTP_CONTENT_LOCATION_NO_CONTENT,
        .flag_no_cache        = true,
        .flag_add_header_date = true,
        .content_type         = HTTP_CONENT_TYPE_TEXT_HTML,
        .p_content_type_param = NULL,
        .content_len          = 0,
        .content_encoding     = HTTP_CONENT_ENCODING_NONE,
        .select_location = {
            .memory = {
                .p_buf = NULL,
            },
        },
    };
    return resp;
}

http_server_resp_t
http_check_post_stat(const http_check_params_t* const p_params, const TimeUnitsSeconds_t timeout_seconds)
{
    const http_server_resp_t resp = {
        .http_resp_code       = HTTP_RESP_CODE_500,
        .content_location     = HTTP_CONTENT_LOCATION_NO_CONTENT,
        .flag_no_cache        = true,
        .flag_add_header_date = true,
        .content_type         = HTTP_CONENT_TYPE_TEXT_HTML,
        .p_content_type_param = NULL,
        .content_len          = 0,
        .content_encoding     = HTTP_CONENT_ENCODING_NONE,
        .select_location = {
            .memory = {
                .p_buf = NULL,
            },
        },
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
        .content_type         = HTTP_CONENT_TYPE_TEXT_HTML,
        .p_content_type_param = NULL,
        .content_len          = 0,
        .content_encoding     = HTTP_CONENT_ENCODING_NONE,
        .select_location = {
            .memory = {
                .p_buf = NULL,
            },
        },
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
main_task_send_sig_deactivate_cfg_cmode(void)
{
}

void
ruuvi_send_nrf_settings(void)
{
}

void
adv_post_send_sig_ble_scan_changed(void)
{
}

void
timer_cfg_mode_deactivation_start_with_short_delay(void)
{
}

bool
gw_cfg_storage_check(void)
{
    return false;
}

bool
gw_cfg_storage_check_file(const char* const p_file_name)
{
    return false;
}

str_buf_t
gw_cfg_storage_read_file(const char* const p_file_name)
{
    return str_buf_init_null();
}

bool
gw_cfg_storage_write_file(const char* const p_file_name, const char* const p_content)
{
    return false;
}

bool
gw_cfg_storage_delete_file(const char* const p_file_name)
{
    return false;
}

bool
partition_table_check_and_update(void)
{
    return true;
}

} // extern "C"

class MemAllocTrace
{
    vector<void*> allocated_mem;

    std::vector<void*>::iterator
    find(void* ptr)
    {
        for (auto iter = this->allocated_mem.begin(); iter != this->allocated_mem.end(); ++iter)
        {
            if (*iter == ptr)
            {
                return iter;
            }
        }
        return this->allocated_mem.end();
    }

public:
    void
    add(void* ptr)
    {
        auto iter = find(ptr);
        assert(iter == this->allocated_mem.end()); // ptr was found in the list of allocated memory blocks
        this->allocated_mem.push_back(ptr);
    }
    void
    remove(void* ptr)
    {
        auto iter = find(ptr);
        assert(iter != this->allocated_mem.end()); // ptr was not found in the list of allocated memory blocks
        this->allocated_mem.erase(iter);
    }
    bool
    is_empty()
    {
        return this->allocated_mem.empty();
    }
};

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
        esp_log_wrapper_init();
        g_pTestClass = this;

        cJSON_Hooks hooks = {
            .malloc_fn = &os_malloc,
            .free_fn   = &os_free_internal,
        };
        cJSON_InitHooks(&hooks);

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
    }

    void
    TearDown() override
    {
        http_server_cb_deinit();
        gw_cfg_deinit();
        gw_cfg_default_deinit();
        this->m_files.clear();
        this->m_fd   = -1;
        g_pTestClass = nullptr;
        esp_log_wrapper_deinit();
    }

public:
    TestHttpServerCb();

    ~TestHttpServerCb() override;

    MemAllocTrace         m_mem_alloc_trace;
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
    , Test()
{
}

extern "C" {

void*
os_malloc(const size_t size)
{
    if (++g_pTestClass->m_malloc_cnt == g_pTestClass->m_malloc_fail_on_cnt)
    {
        return nullptr;
    }
    void* ptr = malloc(size);
    assert(nullptr != ptr);
    g_pTestClass->m_mem_alloc_trace.add(ptr);
    return ptr;
}

void
os_free_internal(void* ptr)
{
    g_pTestClass->m_mem_alloc_trace.remove(ptr);
    free(ptr);
}

void*
os_calloc(const size_t nmemb, const size_t size)
{
    if (++g_pTestClass->m_malloc_cnt == g_pTestClass->m_malloc_fail_on_cnt)
    {
        return nullptr;
    }
    void* ptr = calloc(nmemb, size);
    assert(nullptr != ptr);
    g_pTestClass->m_mem_alloc_trace.add(ptr);
    return ptr;
}

ATTR_NONNULL(1)
bool
os_realloc_safe_and_clean(void** const p_ptr, const size_t size)
{
    void* ptr       = *p_ptr;
    void* p_new_ptr = realloc(ptr, size);
    if (nullptr == p_new_ptr)
    {
        os_free(*p_ptr);
        return false;
    }
    *p_ptr = p_new_ptr;
    return true;
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
    (void)ph_mutex;
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
fw_update_set_url(const char* const p_url_fmt, ...)
{
    va_list ap;
    va_start(ap, p_url_fmt);
    (void)vsnprintf(g_pTestClass->fw_update_url.data(), g_pTestClass->fw_update_url.size(), p_url_fmt, ap);
    va_end(ap);
}

const char*
fw_update_get_url(void)
{
    return g_pTestClass->fw_update_url.data();
}

} // extern "C"

TestHttpServerCb::~TestHttpServerCb() = default;

#define TEST_CHECK_LOG_RECORD_HTTP_SERVER(level_, msg_) \
    ESP_LOG_WRAPPER_TEST_CHECK_LOG_RECORD("http_server", level_, msg_)

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
}

TEST_F(TestHttpServerCb, http_server_cb_deinit_of_not_initialized) // NOLINT
{
    ASSERT_FALSE(g_pTestClass->m_is_fatfs_mounted);
    http_server_cb_deinit();
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_FALSE(g_pTestClass->m_is_fatfs_mounted);
}

TEST_F(TestHttpServerCb, http_server_cb_init_failed) // NOLINT
{
    ASSERT_FALSE(g_pTestClass->m_is_fatfs_mounted);
    this->m_is_fatfs_mount_fail = true;
    ASSERT_FALSE(http_server_cb_init(GW_GWUI_PARTITION));
    ASSERT_FALSE(g_pTestClass->m_is_fatfs_mounted);
    TEST_CHECK_LOG_RECORD_HTTP_SERVER(ESP_LOG_ERROR, "flashfatfs_mount: failed to mount partition 'fatfs_gwui'");
    ASSERT_TRUE(esp_log_wrapper_is_empty());
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
          "\t\"scan_extended_payload\":\ttrue,\n"
          "\t\"scan_channel_37\":\ttrue,\n"
          "\t\"scan_channel_38\":\ttrue,\n"
          "\t\"scan_channel_39\":\ttrue,\n"
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
    const http_server_resp_t resp = http_server_resp_json_ruuvi();

    ASSERT_EQ(HTTP_RESP_CODE_200, resp.http_resp_code);
    ASSERT_EQ(HTTP_CONTENT_LOCATION_HEAP, resp.content_location);
    ASSERT_TRUE(resp.flag_no_cache);
    ASSERT_EQ(HTTP_CONENT_TYPE_APPLICATION_JSON, resp.content_type);
    ASSERT_EQ(nullptr, resp.p_content_type_param);
    ASSERT_EQ(string(expected_json), string(reinterpret_cast<const char*>(resp.select_location.memory.p_buf)));
    ASSERT_EQ(strlen(expected_json), resp.content_len);
    ASSERT_EQ(HTTP_CONENT_ENCODING_NONE, resp.content_encoding);
    ASSERT_NE(nullptr, resp.select_location.memory.p_buf);
    TEST_CHECK_LOG_RECORD_HTTP_SERVER(ESP_LOG_INFO, string("ruuvi.json: ") + string(expected_json));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
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
    ASSERT_EQ(HTTP_CONENT_TYPE_TEXT_HTML, resp.content_type);
    ASSERT_EQ(nullptr, resp.p_content_type_param);
    ASSERT_EQ(0, resp.content_len);
    ASSERT_EQ(HTTP_CONENT_ENCODING_NONE, resp.content_encoding);
    ASSERT_EQ(nullptr, resp.select_location.memory.p_buf);
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_ERROR, string("Can't add json item: fw_ver"));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
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
          "\t\"http_data_format\":\t\"ruuvi\",\n"
          "\t\"http_auth\":\t\"none\",\n"
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
          "\t\"scan_extended_payload\":\ttrue,\n"
          "\t\"scan_channel_37\":\ttrue,\n"
          "\t\"scan_channel_38\":\ttrue,\n"
          "\t\"scan_channel_39\":\ttrue,\n"
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
    const http_server_resp_t resp = http_server_resp_json("ruuvi.json", false);

    ASSERT_EQ(HTTP_RESP_CODE_200, resp.http_resp_code);
    ASSERT_EQ(HTTP_CONTENT_LOCATION_HEAP, resp.content_location);
    ASSERT_TRUE(resp.flag_no_cache);
    ASSERT_EQ(HTTP_CONENT_TYPE_APPLICATION_JSON, resp.content_type);
    ASSERT_EQ(nullptr, resp.p_content_type_param);
    ASSERT_EQ(string(expected_json), string(reinterpret_cast<const char*>(resp.select_location.memory.p_buf)));
    ASSERT_EQ(strlen(expected_json), resp.content_len);
    ASSERT_EQ(HTTP_CONENT_ENCODING_NONE, resp.content_encoding);
    ASSERT_NE(nullptr, resp.select_location.memory.p_buf);
    TEST_CHECK_LOG_RECORD_HTTP_SERVER(ESP_LOG_INFO, string("ruuvi.json: ") + string(expected_json));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
}

TEST_F(TestHttpServerCb, resp_json_unknown) // NOLINT
{
    const http_server_resp_t resp = http_server_resp_json("unknown.json", false);

    ASSERT_EQ(HTTP_RESP_CODE_404, resp.http_resp_code);
    ASSERT_EQ(HTTP_CONTENT_LOCATION_NO_CONTENT, resp.content_location);
    ASSERT_TRUE(resp.flag_no_cache);
    ASSERT_EQ(HTTP_CONENT_TYPE_TEXT_HTML, resp.content_type);
    ASSERT_EQ(nullptr, resp.p_content_type_param);
    ASSERT_EQ(0, resp.content_len);
    ASSERT_EQ(HTTP_CONENT_ENCODING_NONE, resp.content_encoding);
    ASSERT_EQ(nullptr, resp.select_location.memory.p_buf);
    TEST_CHECK_LOG_RECORD_HTTP_SERVER(ESP_LOG_WARN, string("Request to unknown json: unknown.json"));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
}

TEST_F(TestHttpServerCb, resp_metrics_ok) // NOLINT
{
    const char*              expected_resp = "metrics_info";
    const http_server_resp_t resp          = http_server_resp_metrics();

    ASSERT_EQ(HTTP_RESP_CODE_200, resp.http_resp_code);
    ASSERT_EQ(HTTP_CONTENT_LOCATION_HEAP, resp.content_location);
    ASSERT_TRUE(resp.flag_no_cache);
    ASSERT_EQ(HTTP_CONENT_TYPE_TEXT_PLAIN, resp.content_type);
    ASSERT_NE(nullptr, resp.p_content_type_param);
    ASSERT_EQ(string("version=0.0.4"), string(resp.p_content_type_param));
    ASSERT_EQ(strlen(expected_resp), resp.content_len);
    ASSERT_EQ(HTTP_CONENT_ENCODING_NONE, resp.content_encoding);
    ASSERT_NE(nullptr, resp.select_location.memory.p_buf);
    ASSERT_EQ(string(expected_resp), string(reinterpret_cast<const char*>(resp.select_location.memory.p_buf)));
    TEST_CHECK_LOG_RECORD_HTTP_SERVER(ESP_LOG_INFO, string("metrics: ") + string(expected_resp));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
}

TEST_F(TestHttpServerCb, resp_metrics_malloc_failed) // NOLINT
{
    g_pTestClass->m_malloc_fail_on_cnt = 1;
    const http_server_resp_t resp      = http_server_resp_metrics();

    ASSERT_EQ(HTTP_RESP_CODE_503, resp.http_resp_code);
    ASSERT_EQ(HTTP_CONTENT_LOCATION_NO_CONTENT, resp.content_location);
    ASSERT_TRUE(resp.flag_no_cache);
    ASSERT_EQ(HTTP_CONENT_TYPE_TEXT_HTML, resp.content_type);
    ASSERT_EQ(nullptr, resp.p_content_type_param);
    ASSERT_EQ(0, resp.content_len);
    ASSERT_EQ(HTTP_CONENT_ENCODING_NONE, resp.content_encoding);
    ASSERT_EQ(nullptr, resp.select_location.memory.p_buf);
    TEST_CHECK_LOG_RECORD_HTTP_SERVER(ESP_LOG_ERROR, string("Not enough memory"));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
}

TEST_F(TestHttpServerCb, http_get_content_type_by_ext) // NOLINT
{
    ASSERT_EQ(HTTP_CONENT_TYPE_TEXT_HTML, http_get_content_type_by_ext(".html"));
    ASSERT_EQ(HTTP_CONENT_TYPE_TEXT_CSS, http_get_content_type_by_ext(".css"));
    ASSERT_EQ(HTTP_CONENT_TYPE_TEXT_CSS, http_get_content_type_by_ext(".scss"));
    ASSERT_EQ(HTTP_CONENT_TYPE_TEXT_JAVASCRIPT, http_get_content_type_by_ext(".js"));
    ASSERT_EQ(HTTP_CONENT_TYPE_IMAGE_PNG, http_get_content_type_by_ext(".png"));
    ASSERT_EQ(HTTP_CONENT_TYPE_IMAGE_SVG_XML, http_get_content_type_by_ext(".svg"));
    ASSERT_EQ(HTTP_CONENT_TYPE_APPLICATION_OCTET_STREAM, http_get_content_type_by_ext(".ttf"));
    ASSERT_EQ(HTTP_CONENT_TYPE_APPLICATION_OCTET_STREAM, http_get_content_type_by_ext(".unk"));
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
    ASSERT_EQ(HTTP_CONENT_TYPE_TEXT_HTML, resp.content_type);
    ASSERT_EQ(nullptr, resp.p_content_type_param);
    ASSERT_EQ(0, resp.content_len);
    ASSERT_EQ(HTTP_CONENT_ENCODING_NONE, resp.content_encoding);
    ASSERT_EQ(nullptr, resp.select_location.memory.p_buf);
    TEST_CHECK_LOG_RECORD_HTTP_SERVER(ESP_LOG_DEBUG, "Try to find file: index.html");
    TEST_CHECK_LOG_RECORD_HTTP_SERVER(ESP_LOG_ERROR, "GWUI partition is not ready");
    ASSERT_TRUE(esp_log_wrapper_is_empty());
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
    ASSERT_EQ(HTTP_CONENT_TYPE_TEXT_HTML, resp.content_type);
    ASSERT_EQ(nullptr, resp.p_content_type_param);
    ASSERT_EQ(0, resp.content_len);
    ASSERT_EQ(HTTP_CONENT_ENCODING_NONE, resp.content_encoding);
    ASSERT_EQ(nullptr, resp.select_location.memory.p_buf);
    TEST_CHECK_LOG_RECORD_HTTP_SERVER(ESP_LOG_DEBUG, string("Try to find file: ") + string(file_name));
    TEST_CHECK_LOG_RECORD_HTTP_SERVER(
        ESP_LOG_ERROR,
        string("Temporary buffer is not enough for the file path '") + string(file_name) + string("'"));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
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
    ASSERT_EQ(HTTP_CONENT_TYPE_TEXT_HTML, resp.content_type);
    ASSERT_EQ(nullptr, resp.p_content_type_param);
    ASSERT_EQ(strlen(expected_resp), resp.content_len);
    ASSERT_EQ(HTTP_CONENT_ENCODING_NONE, resp.content_encoding);
    ASSERT_EQ(fd, resp.select_location.fatfs.fd);
    TEST_CHECK_LOG_RECORD_HTTP_SERVER(ESP_LOG_DEBUG, "Try to find file: index.html");
    TEST_CHECK_LOG_RECORD_HTTP_SERVER(ESP_LOG_DEBUG, "File index.html was opened successfully, fd=1");
    ASSERT_TRUE(esp_log_wrapper_is_empty());
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
    ASSERT_EQ(HTTP_CONENT_TYPE_TEXT_HTML, resp.content_type);
    ASSERT_EQ(nullptr, resp.p_content_type_param);
    ASSERT_EQ(strlen(expected_resp), resp.content_len);
    ASSERT_EQ(HTTP_CONENT_ENCODING_GZIP, resp.content_encoding);
    ASSERT_EQ(fd, resp.select_location.fatfs.fd);
    TEST_CHECK_LOG_RECORD_HTTP_SERVER(ESP_LOG_DEBUG, string("Try to find file: index.html"));
    TEST_CHECK_LOG_RECORD_HTTP_SERVER(ESP_LOG_DEBUG, "File index.html.gz was opened successfully, fd=2");
    ASSERT_TRUE(esp_log_wrapper_is_empty());
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
    ASSERT_EQ(HTTP_CONENT_TYPE_TEXT_JAVASCRIPT, resp.content_type);
    ASSERT_EQ(nullptr, resp.p_content_type_param);
    ASSERT_EQ(strlen(expected_resp), resp.content_len);
    ASSERT_EQ(HTTP_CONENT_ENCODING_GZIP, resp.content_encoding);
    ASSERT_EQ(fd, resp.select_location.fatfs.fd);
    TEST_CHECK_LOG_RECORD_HTTP_SERVER(ESP_LOG_DEBUG, string("Try to find file: app.js"));
    TEST_CHECK_LOG_RECORD_HTTP_SERVER(ESP_LOG_DEBUG, "File app.js.gz was opened successfully, fd=1");
    ASSERT_TRUE(esp_log_wrapper_is_empty());
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
    ASSERT_EQ(HTTP_CONENT_TYPE_TEXT_CSS, resp.content_type);
    ASSERT_EQ(nullptr, resp.p_content_type_param);
    ASSERT_EQ(strlen(expected_resp), resp.content_len);
    ASSERT_EQ(HTTP_CONENT_ENCODING_GZIP, resp.content_encoding);
    ASSERT_EQ(fd, resp.select_location.fatfs.fd);
    TEST_CHECK_LOG_RECORD_HTTP_SERVER(ESP_LOG_DEBUG, string("Try to find file: style.css"));
    TEST_CHECK_LOG_RECORD_HTTP_SERVER(ESP_LOG_DEBUG, "File style.css.gz was opened successfully, fd=1");
    ASSERT_TRUE(esp_log_wrapper_is_empty());
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
    ASSERT_EQ(HTTP_CONENT_TYPE_APPLICATION_OCTET_STREAM, resp.content_type);
    ASSERT_EQ(nullptr, resp.p_content_type_param);
    ASSERT_EQ(strlen(expected_resp), resp.content_len);
    ASSERT_EQ(HTTP_CONENT_ENCODING_NONE, resp.content_encoding);
    ASSERT_EQ(fd, resp.select_location.fatfs.fd);
    TEST_CHECK_LOG_RECORD_HTTP_SERVER(ESP_LOG_DEBUG, string("Try to find file: binary"));
    TEST_CHECK_LOG_RECORD_HTTP_SERVER(ESP_LOG_DEBUG, "File binary was opened successfully, fd=1");
    ASSERT_TRUE(esp_log_wrapper_is_empty());
}

TEST_F(TestHttpServerCb, resp_file_unknown_html) // NOLINT
{
    ASSERT_TRUE(http_server_cb_init(GW_GWUI_PARTITION));

    const http_server_resp_t resp = http_server_resp_file("unknown.html", HTTP_RESP_CODE_200);
    ASSERT_EQ(HTTP_RESP_CODE_404, resp.http_resp_code);
    ASSERT_EQ(HTTP_CONTENT_LOCATION_NO_CONTENT, resp.content_location);
    ASSERT_TRUE(resp.flag_no_cache);
    ASSERT_EQ(HTTP_CONENT_TYPE_TEXT_HTML, resp.content_type);
    ASSERT_EQ(nullptr, resp.p_content_type_param);
    ASSERT_EQ(0, resp.content_len);
    ASSERT_EQ(HTTP_CONENT_ENCODING_NONE, resp.content_encoding);
    ASSERT_EQ(nullptr, resp.select_location.memory.p_buf);
    TEST_CHECK_LOG_RECORD_HTTP_SERVER(ESP_LOG_DEBUG, string("Try to find file: unknown.html"));
    TEST_CHECK_LOG_RECORD_HTTP_SERVER(ESP_LOG_ERROR, string("Can't find file: unknown.html"));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
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
    ASSERT_EQ(HTTP_CONENT_TYPE_TEXT_HTML, resp.content_type);
    ASSERT_EQ(nullptr, resp.p_content_type_param);
    ASSERT_EQ(0, resp.content_len);
    ASSERT_EQ(HTTP_CONENT_ENCODING_NONE, resp.content_encoding);
    ASSERT_EQ(nullptr, resp.select_location.memory.p_buf);
    TEST_CHECK_LOG_RECORD_HTTP_SERVER(ESP_LOG_DEBUG, "Try to find file: index.html");
    TEST_CHECK_LOG_RECORD_HTTP_SERVER(ESP_LOG_ERROR, string("Can't open file: index.html"));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
}

TEST_F(TestHttpServerCb, http_server_cb_on_get_default) // NOLINT
{
    ASSERT_TRUE(http_server_cb_init(GW_GWUI_PARTITION));
    this->m_fd = -1;

    const http_server_resp_t resp = http_server_cb_on_get("", nullptr, false, nullptr);
    ASSERT_EQ(HTTP_RESP_CODE_404, resp.http_resp_code);
    ASSERT_EQ(HTTP_CONTENT_LOCATION_NO_CONTENT, resp.content_location);
    ASSERT_TRUE(resp.flag_no_cache);
    ASSERT_EQ(HTTP_CONENT_TYPE_TEXT_HTML, resp.content_type);
    ASSERT_EQ(nullptr, resp.p_content_type_param);
    ASSERT_EQ(0, resp.content_len);
    ASSERT_EQ(HTTP_CONENT_ENCODING_NONE, resp.content_encoding);
    ASSERT_EQ(0, resp.select_location.fatfs.fd);
    TEST_CHECK_LOG_RECORD_HTTP_SERVER(ESP_LOG_DEBUG, string("http_server_cb_on_get /"));
    TEST_CHECK_LOG_RECORD_HTTP_SERVER(ESP_LOG_DEBUG, string("Try to find file: ruuvi.html"));
    TEST_CHECK_LOG_RECORD_HTTP_SERVER(ESP_LOG_ERROR, "Can't find file: ruuvi.html");
    ASSERT_TRUE(esp_log_wrapper_is_empty());
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
    ASSERT_EQ(HTTP_CONENT_TYPE_TEXT_HTML, resp.content_type);
    ASSERT_EQ(nullptr, resp.p_content_type_param);
    ASSERT_EQ(strlen(expected_resp), resp.content_len);
    ASSERT_EQ(HTTP_CONENT_ENCODING_GZIP, resp.content_encoding);
    ASSERT_EQ(fd, resp.select_location.fatfs.fd);
    TEST_CHECK_LOG_RECORD_HTTP_SERVER(ESP_LOG_DEBUG, string("http_server_cb_on_get /index.html"));
    TEST_CHECK_LOG_RECORD_HTTP_SERVER(ESP_LOG_DEBUG, string("Try to find file: index.html"));
    TEST_CHECK_LOG_RECORD_HTTP_SERVER(ESP_LOG_DEBUG, "File index.html.gz was opened successfully, fd=1");
    ASSERT_TRUE(esp_log_wrapper_is_empty());
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
    ASSERT_EQ(HTTP_CONENT_TYPE_TEXT_JAVASCRIPT, resp.content_type);
    ASSERT_EQ(nullptr, resp.p_content_type_param);
    ASSERT_EQ(strlen(expected_resp), resp.content_len);
    ASSERT_EQ(HTTP_CONENT_ENCODING_GZIP, resp.content_encoding);
    ASSERT_EQ(fd, resp.select_location.fatfs.fd);
    TEST_CHECK_LOG_RECORD_HTTP_SERVER(ESP_LOG_DEBUG, string("http_server_cb_on_get /app.js"));
    TEST_CHECK_LOG_RECORD_HTTP_SERVER(ESP_LOG_DEBUG, string("Try to find file: app.js"));
    TEST_CHECK_LOG_RECORD_HTTP_SERVER(ESP_LOG_DEBUG, "File app.js.gz was opened successfully, fd=1");
    ASSERT_TRUE(esp_log_wrapper_is_empty());
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
          "\t\"scan_extended_payload\":\ttrue,\n"
          "\t\"scan_channel_37\":\ttrue,\n"
          "\t\"scan_channel_38\":\ttrue,\n"
          "\t\"scan_channel_39\":\ttrue,\n"
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
    const http_server_resp_t resp = http_server_cb_on_get("ruuvi.json", nullptr, false, nullptr);

    ASSERT_EQ(HTTP_RESP_CODE_200, resp.http_resp_code);
    ASSERT_EQ(HTTP_CONTENT_LOCATION_HEAP, resp.content_location);
    ASSERT_TRUE(resp.flag_no_cache);
    ASSERT_EQ(HTTP_CONENT_TYPE_APPLICATION_JSON, resp.content_type);
    ASSERT_EQ(nullptr, resp.p_content_type_param);
    ASSERT_EQ(string(expected_json), string(reinterpret_cast<const char*>(resp.select_location.memory.p_buf)));
    ASSERT_EQ(strlen(expected_json), resp.content_len);
    ASSERT_EQ(HTTP_CONENT_ENCODING_NONE, resp.content_encoding);
    ASSERT_NE(nullptr, resp.select_location.memory.p_buf);
    TEST_CHECK_LOG_RECORD_HTTP_SERVER(ESP_LOG_DEBUG, string("http_server_cb_on_get /ruuvi.json"));
    TEST_CHECK_LOG_RECORD_HTTP_SERVER(ESP_LOG_INFO, string("ruuvi.json: ") + string(expected_json));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
}

TEST_F(TestHttpServerCb, http_server_cb_on_get_metrics) // NOLINT
{
    const char*              expected_resp = "metrics_info";
    const http_server_resp_t resp          = http_server_cb_on_get("metrics", nullptr, false, nullptr);

    ASSERT_EQ(HTTP_RESP_CODE_200, resp.http_resp_code);
    ASSERT_EQ(HTTP_CONTENT_LOCATION_HEAP, resp.content_location);
    ASSERT_TRUE(resp.flag_no_cache);
    ASSERT_EQ(HTTP_CONENT_TYPE_TEXT_PLAIN, resp.content_type);
    ASSERT_NE(nullptr, resp.p_content_type_param);
    ASSERT_EQ(string("version=0.0.4"), string(resp.p_content_type_param));
    ASSERT_EQ(strlen(expected_resp), resp.content_len);
    ASSERT_EQ(HTTP_CONENT_ENCODING_NONE, resp.content_encoding);
    ASSERT_NE(nullptr, resp.select_location.memory.p_buf);
    ASSERT_EQ(string(expected_resp), string(reinterpret_cast<const char*>(resp.select_location.memory.p_buf)));
    TEST_CHECK_LOG_RECORD_HTTP_SERVER(ESP_LOG_DEBUG, string("http_server_cb_on_get /metrics"));
    TEST_CHECK_LOG_RECORD_HTTP_SERVER(ESP_LOG_INFO, string("metrics: ") + string(expected_resp));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
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
          "        \"data\": \"2233\"\n"
          "      },\n"
          "      \"AA:BB:CC:11:22:02\": {\n"
          "        \"rssi\": 51,\n"
          "        \"timestamp\": 1615660209,\n"
          "        \"data\": \"223344\"\n"
          "      }\n"
          "    }\n"
          "  }\n"
          "}";
    const http_server_resp_t resp = http_server_cb_on_get("history", nullptr, false, nullptr);

    ASSERT_EQ(HTTP_RESP_CODE_200, resp.http_resp_code);
    ASSERT_EQ(HTTP_CONTENT_LOCATION_JSON_GENERATOR, resp.content_location);
    ASSERT_TRUE(resp.flag_no_cache);
    ASSERT_EQ(HTTP_CONENT_TYPE_APPLICATION_JSON, resp.content_type);
    ASSERT_EQ(nullptr, resp.p_content_type_param);
    ASSERT_EQ(HTTP_CONENT_ENCODING_NONE, resp.content_encoding);

    ASSERT_NE(nullptr, resp.select_location.json_generator.p_json_gen);
    string json_str("");
    while (true)
    {
        const char* p_chunk = json_stream_gen_get_next_chunk(resp.select_location.json_generator.p_json_gen);
        if (nullptr == p_chunk)
        {
            ASSERT_FALSE(nullptr == p_chunk);
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
          "        \"data\": \"2233\"\n"
          "      },\n"
          "      \"AA:BB:CC:11:22:02\": {\n"
          "        \"rssi\": 51,\n"
          "        \"timestamp\": 1615660209,\n"
          "        \"data\": \"223344\"\n"
          "      }\n"
          "    }\n"
          "  }\n"
          "}";
    const http_server_resp_t resp = http_server_cb_on_get("history", "time=20", false, nullptr);

    ASSERT_EQ(HTTP_RESP_CODE_200, resp.http_resp_code);
    ASSERT_EQ(HTTP_CONTENT_LOCATION_JSON_GENERATOR, resp.content_location);
    ASSERT_TRUE(resp.flag_no_cache);
    ASSERT_EQ(HTTP_CONENT_TYPE_APPLICATION_JSON, resp.content_type);
    ASSERT_EQ(nullptr, resp.p_content_type_param);
    ASSERT_EQ(strlen(expected_resp), resp.content_len);
    ASSERT_EQ(HTTP_CONENT_ENCODING_NONE, resp.content_encoding);

    ASSERT_NE(nullptr, resp.select_location.json_generator.p_json_gen);
    string json_str("");
    while (true)
    {
        const char* p_chunk = json_stream_gen_get_next_chunk(resp.select_location.json_generator.p_json_gen);
        if (nullptr == p_chunk)
        {
            ASSERT_FALSE(nullptr == p_chunk);
        }

        if ('\0' == p_chunk[0])
        {
            break;
        }
        json_str += string(p_chunk);
    }
    ASSERT_EQ(string(expected_resp), json_str);

    TEST_CHECK_LOG_RECORD_HTTP_SERVER(ESP_LOG_DEBUG, string("http_server_cb_on_get /history?time=20"));
    TEST_CHECK_LOG_RECORD_HTTP_SERVER(ESP_LOG_DEBUG, string("HTTP params: time=20"));
    TEST_CHECK_LOG_RECORD_HTTP_SERVER(ESP_LOG_DEBUG, string("Can't find key 'decode=' in URL params"));
    TEST_CHECK_LOG_RECORD_HTTP_SERVER(ESP_LOG_INFO, string("Requested /history on 20 seconds interval"));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
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
          "        \"data\": \"2233\"\n"
          "      },\n"
          "      \"AA:BB:CC:11:22:02\": {\n"
          "        \"rssi\": 51,\n"
          "        \"counter\": 1615660209,\n"
          "        \"data\": \"223344\"\n"
          "      }\n"
          "    }\n"
          "  }\n"
          "}";
    const http_server_resp_t resp = http_server_cb_on_get("history", nullptr, false, nullptr);

    ASSERT_EQ(HTTP_RESP_CODE_200, resp.http_resp_code);
    ASSERT_EQ(HTTP_CONTENT_LOCATION_JSON_GENERATOR, resp.content_location);
    ASSERT_TRUE(resp.flag_no_cache);
    ASSERT_EQ(HTTP_CONENT_TYPE_APPLICATION_JSON, resp.content_type);
    ASSERT_EQ(nullptr, resp.p_content_type_param);
    ASSERT_EQ(strlen(expected_resp), resp.content_len);
    ASSERT_EQ(HTTP_CONENT_ENCODING_NONE, resp.content_encoding);

    ASSERT_NE(nullptr, resp.select_location.json_generator.p_json_gen);
    string json_str("");
    while (true)
    {
        const char* p_chunk = json_stream_gen_get_next_chunk(resp.select_location.json_generator.p_json_gen);
        if (nullptr == p_chunk)
        {
            ASSERT_FALSE(nullptr == p_chunk);
        }

        if ('\0' == p_chunk[0])
        {
            break;
        }
        json_str += string(p_chunk);
    }
    ASSERT_EQ(string(expected_resp), json_str);

    TEST_CHECK_LOG_RECORD_HTTP_SERVER(ESP_LOG_DEBUG, string("http_server_cb_on_get /history"));
    TEST_CHECK_LOG_RECORD_HTTP_SERVER(ESP_LOG_INFO, string("Requested /history (without filtering)"));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
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
          "        \"data\": \"2233\"\n"
          "      },\n"
          "      \"AA:BB:CC:11:22:02\": {\n"
          "        \"rssi\": 51,\n"
          "        \"counter\": 1615660209,\n"
          "        \"data\": \"223344\"\n"
          "      }\n"
          "    }\n"
          "  }\n"
          "}";
    const http_server_resp_t resp = http_server_cb_on_get("history", "counter=10", false, nullptr);

    ASSERT_EQ(HTTP_RESP_CODE_200, resp.http_resp_code);
    ASSERT_EQ(HTTP_CONTENT_LOCATION_JSON_GENERATOR, resp.content_location);
    ASSERT_TRUE(resp.flag_no_cache);
    ASSERT_EQ(HTTP_CONENT_TYPE_APPLICATION_JSON, resp.content_type);
    ASSERT_EQ(nullptr, resp.p_content_type_param);
    ASSERT_EQ(strlen(expected_resp), resp.content_len);
    ASSERT_EQ(HTTP_CONENT_ENCODING_NONE, resp.content_encoding);

    ASSERT_NE(nullptr, resp.select_location.json_generator.p_json_gen);
    string json_str("");
    while (true)
    {
        const char* p_chunk = json_stream_gen_get_next_chunk(resp.select_location.json_generator.p_json_gen);
        if (nullptr == p_chunk)
        {
            ASSERT_FALSE(nullptr == p_chunk);
        }

        if ('\0' == p_chunk[0])
        {
            break;
        }
        json_str += string(p_chunk);
    }
    ASSERT_EQ(string(expected_resp), json_str);

    TEST_CHECK_LOG_RECORD_HTTP_SERVER(ESP_LOG_DEBUG, string("http_server_cb_on_get /history?counter=10"));
    TEST_CHECK_LOG_RECORD_HTTP_SERVER(ESP_LOG_DEBUG, string("HTTP params: counter=10"));
    TEST_CHECK_LOG_RECORD_HTTP_SERVER(ESP_LOG_DEBUG, string("Can't find key 'decode=' in URL params"));
    TEST_CHECK_LOG_RECORD_HTTP_SERVER(ESP_LOG_INFO, string("Requested /history starting from counter 10"));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
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
    ASSERT_EQ(HTTP_CONENT_TYPE_APPLICATION_JSON, resp.content_type);
    ASSERT_EQ(nullptr, resp.p_content_type_param);
    ASSERT_EQ(strlen(expected_resp), resp.content_len);
    ASSERT_EQ(HTTP_CONENT_ENCODING_NONE, resp.content_encoding);
    ASSERT_NE(nullptr, resp.select_location.memory.p_buf);
    ASSERT_EQ(string(expected_resp), string(reinterpret_cast<const char*>(resp.select_location.memory.p_buf)));
    TEST_CHECK_LOG_RECORD_HTTP_SERVER(ESP_LOG_DEBUG, string("POST /ruuvi.json, flag_access_from_lan=0"));
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
    ASSERT_EQ(HTTP_CONENT_TYPE_APPLICATION_JSON, resp.content_type);
    ASSERT_EQ(nullptr, resp.p_content_type_param);
    ASSERT_EQ(strlen(expected_resp), resp.content_len);
    ASSERT_EQ(HTTP_CONENT_ENCODING_NONE, resp.content_encoding);
    ASSERT_NE(nullptr, resp.select_location.memory.p_buf);
    ASSERT_EQ(string(expected_resp), string(reinterpret_cast<const char*>(resp.select_location.memory.p_buf)));
    TEST_CHECK_LOG_RECORD_HTTP_SERVER(ESP_LOG_DEBUG, string("POST /ruuvi.json, flag_access_from_lan=1"));
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
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "scan_coded_phy: not found");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_WARN, "Can't find key 'scan_coded_phy' in config-json");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "scan_1mbit_phy: not found");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_WARN, "Can't find key 'scan_1mbit_phy' in config-json");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "scan_extended_payload: not found");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_WARN, "Can't find key 'scan_extended_payload' in config-json");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "scan_channel_37: not found");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_WARN, "Can't find key 'scan_channel_37' in config-json");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "scan_channel_38: not found");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_WARN, "Can't find key 'scan_channel_38' in config-json");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "scan_channel_39: not found");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_WARN, "Can't find key 'scan_channel_39' in config-json");
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
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, string("config: NTP: Server3: time.nist.gov"));
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, string("config: NTP: Server4: pool.ntp.org"));
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, string("config: use company id filter: 0"));
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, string("config: company id: 0x0499"));
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, string("config: use scan coded phy: 0"));
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, string("config: use scan 1mbit/phy: 1"));
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, string("config: use scan extended payload: 1"));
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, string("config: use scan channel 37: 1"));
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, string("config: use scan channel 38: 1"));
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, string("config: use scan channel 39: 1"));
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, string("config: use scan filter: no"));
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, string("config: coordinates: "));
    TEST_CHECK_LOG_RECORD_GW_CFG(
        ESP_LOG_INFO,
        string("config: fw_update: url: https://network.ruuvi.com/firmwareupdate"));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
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
    ASSERT_EQ(HTTP_CONENT_TYPE_APPLICATION_JSON, resp.content_type);
    ASSERT_EQ(nullptr, resp.p_content_type_param);
    ASSERT_EQ(strlen(expected_resp), resp.content_len);
    ASSERT_EQ(HTTP_CONENT_ENCODING_NONE, resp.content_encoding);
    ASSERT_NE(nullptr, resp.select_location.memory.p_buf);
    ASSERT_EQ(string(expected_resp), string(reinterpret_cast<const char*>(resp.select_location.memory.p_buf)));
    TEST_CHECK_LOG_RECORD_HTTP_SERVER(ESP_LOG_DEBUG, string("POST /ruuvi.json, flag_access_from_lan=0"));
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, "Gateway SETTINGS (via HTTP):");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "remote_cfg_use: 0");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "remote_cfg_url: ");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "remote_cfg_auth_type: none");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "remote_cfg_use_ssl_client_cert: 0");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "remote_cfg_use_ssl_server_cert: 0");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "remote_cfg_refresh_interval_minutes: 0");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "use_http_ruuvi: 0");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "use_http: 0");
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
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "scan_coded_phy: not found");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_WARN, "Can't find key 'scan_coded_phy' in config-json");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "scan_1mbit_phy: not found");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_WARN, "Can't find key 'scan_1mbit_phy' in config-json");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "scan_extended_payload: not found");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_WARN, "Can't find key 'scan_extended_payload' in config-json");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "scan_channel_37: not found");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_WARN, "Can't find key 'scan_channel_37' in config-json");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "scan_channel_38: not found");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_WARN, "Can't find key 'scan_channel_38' in config-json");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "scan_channel_39: not found");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_WARN, "Can't find key 'scan_channel_39' in config-json");
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
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, string("config: NTP: Server3: time.nist.gov"));
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, string("config: NTP: Server4: pool.ntp.org"));
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, string("config: use company id filter: 1"));
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, string("config: company id: 0x0499"));
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, string("config: use scan coded phy: 0"));
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, string("config: use scan 1mbit/phy: 1"));
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, string("config: use scan extended payload: 1"));
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, string("config: use scan channel 37: 1"));
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, string("config: use scan channel 38: 1"));
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, string("config: use scan channel 39: 1"));
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, string("config: use scan filter: no"));
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, string("config: coordinates: "));
    TEST_CHECK_LOG_RECORD_GW_CFG(
        ESP_LOG_INFO,
        string("config: fw_update: url: https://network.ruuvi.com/firmwareupdate"));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
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
    ASSERT_EQ(HTTP_CONENT_TYPE_TEXT_HTML, resp.content_type);
    ASSERT_EQ(nullptr, resp.p_content_type_param);
    ASSERT_EQ(0, resp.content_len);
    ASSERT_EQ(HTTP_CONENT_ENCODING_NONE, resp.content_encoding);
    ASSERT_EQ(nullptr, resp.select_location.memory.p_buf);
    TEST_CHECK_LOG_RECORD_HTTP_SERVER(ESP_LOG_DEBUG, string("POST /ruuvi.json, flag_access_from_lan=0"));
    TEST_CHECK_LOG_RECORD_HTTP_SERVER(ESP_LOG_ERROR, string("Failed to allocate memory for gw_cfg"));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
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
    ASSERT_EQ(HTTP_CONENT_TYPE_TEXT_HTML, resp.content_type);
    ASSERT_EQ(nullptr, resp.p_content_type_param);
    ASSERT_EQ(0, resp.content_len);
    ASSERT_EQ(HTTP_CONENT_ENCODING_NONE, resp.content_encoding);
    ASSERT_EQ(nullptr, resp.select_location.memory.p_buf);
    TEST_CHECK_LOG_RECORD_HTTP_SERVER(ESP_LOG_DEBUG, string("POST /ruuvi.json, flag_access_from_lan=0"));
    TEST_CHECK_LOG_RECORD_HTTP_SERVER(ESP_LOG_ERROR, string("Failed to parse json or no memory"));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
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
    ASSERT_EQ(HTTP_CONENT_TYPE_TEXT_HTML, resp.content_type);
    ASSERT_EQ(nullptr, resp.p_content_type_param);
    ASSERT_EQ(0, resp.content_len);
    ASSERT_EQ(HTTP_CONENT_ENCODING_NONE, resp.content_encoding);
    ASSERT_EQ(nullptr, resp.select_location.memory.p_buf);
    TEST_CHECK_LOG_RECORD_HTTP_SERVER(ESP_LOG_DEBUG, string("POST /ruuvi.json, flag_access_from_lan=0"));
    TEST_CHECK_LOG_RECORD_HTTP_SERVER(ESP_LOG_ERROR, string("Failed to parse json or no memory"));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
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
    ASSERT_EQ(HTTP_CONENT_TYPE_APPLICATION_JSON, resp.content_type);
    ASSERT_EQ(nullptr, resp.p_content_type_param);
    ASSERT_EQ(strlen(expected_resp), resp.content_len);
    ASSERT_EQ(HTTP_CONENT_ENCODING_NONE, resp.content_encoding);
    ASSERT_NE(nullptr, resp.select_location.memory.p_buf);
    ASSERT_EQ(string(expected_resp), string(reinterpret_cast<const char*>(resp.select_location.memory.p_buf)));
    TEST_CHECK_LOG_RECORD_HTTP_SERVER(ESP_LOG_DEBUG, string("http_server_cb_on_post /ruuvi.json, params="));
    TEST_CHECK_LOG_RECORD_HTTP_SERVER(ESP_LOG_DEBUG, string("POST /ruuvi.json, flag_access_from_lan=0"));
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, "Gateway SETTINGS (via HTTP):");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "remote_cfg_use: 0");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "remote_cfg_url: ");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "remote_cfg_auth_type: none");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "remote_cfg_use_ssl_client_cert: 0");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "remote_cfg_use_ssl_server_cert: 0");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "remote_cfg_refresh_interval_minutes: 0");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "use_http_ruuvi: 0");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "use_http: 0");
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
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "scan_coded_phy: not found");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_WARN, "Can't find key 'scan_coded_phy' in config-json");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "scan_1mbit_phy: not found");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_WARN, "Can't find key 'scan_1mbit_phy' in config-json");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "scan_extended_payload: not found");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_WARN, "Can't find key 'scan_extended_payload' in config-json");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "scan_channel_37: not found");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_WARN, "Can't find key 'scan_channel_37' in config-json");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "scan_channel_38: not found");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_WARN, "Can't find key 'scan_channel_38' in config-json");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "scan_channel_39: not found");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_WARN, "Can't find key 'scan_channel_39' in config-json");
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
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, string("config: NTP: Server3: time.nist.gov"));
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, string("config: NTP: Server4: pool.ntp.org"));
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, string("config: use company id filter: 1"));
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, string("config: company id: 0x0499"));
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, string("config: use scan coded phy: 0"));
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, string("config: use scan 1mbit/phy: 1"));
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, string("config: use scan extended payload: 1"));
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, string("config: use scan channel 37: 1"));
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, string("config: use scan channel 38: 1"));
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, string("config: use scan channel 39: 1"));
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, string("config: use scan filter: no"));
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, string("config: coordinates: "));
    TEST_CHECK_LOG_RECORD_GW_CFG(
        ESP_LOG_INFO,
        string("config: fw_update: url: https://network.ruuvi.com/firmwareupdate"));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
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
    ASSERT_EQ(HTTP_CONENT_TYPE_APPLICATION_JSON, resp.content_type);
    ASSERT_EQ(nullptr, resp.p_content_type_param);
    ASSERT_EQ(strlen(expected_resp), resp.content_len);
    ASSERT_EQ(HTTP_CONENT_ENCODING_NONE, resp.content_encoding);
    ASSERT_NE(nullptr, resp.select_location.memory.p_buf);
    ASSERT_EQ(string(expected_resp), string(reinterpret_cast<const char*>(resp.select_location.memory.p_buf)));
    TEST_CHECK_LOG_RECORD_HTTP_SERVER(ESP_LOG_DEBUG, string("http_server_cb_on_post /ruuvi.json, params="));
    TEST_CHECK_LOG_RECORD_HTTP_SERVER(ESP_LOG_DEBUG, string("POST /ruuvi.json, flag_access_from_lan=0"));
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, "Gateway SETTINGS (via HTTP):");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "remote_cfg_use: 0");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "remote_cfg_url: ");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "remote_cfg_auth_type: none");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "remote_cfg_use_ssl_client_cert: 0");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "remote_cfg_use_ssl_server_cert: 0");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "remote_cfg_refresh_interval_minutes: 0");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "use_http_ruuvi: 0");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "use_http: 0");
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
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "scan_coded_phy: not found");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_WARN, "Can't find key 'scan_coded_phy' in config-json");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "scan_1mbit_phy: not found");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_WARN, "Can't find key 'scan_1mbit_phy' in config-json");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "scan_extended_payload: not found");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_WARN, "Can't find key 'scan_extended_payload' in config-json");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "scan_channel_37: not found");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_WARN, "Can't find key 'scan_channel_37' in config-json");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "scan_channel_38: not found");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_WARN, "Can't find key 'scan_channel_38' in config-json");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "scan_channel_39: not found");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_WARN, "Can't find key 'scan_channel_39' in config-json");
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
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, string("config: NTP: Server3: time.nist.gov"));
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, string("config: NTP: Server4: pool.ntp.org"));
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, string("config: use company id filter: 1"));
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, string("config: company id: 0x0499"));
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, string("config: use scan coded phy: 0"));
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, string("config: use scan 1mbit/phy: 1"));
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, string("config: use scan extended payload: 1"));
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, string("config: use scan channel 37: 1"));
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, string("config: use scan channel 38: 1"));
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, string("config: use scan channel 39: 1"));
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, string("config: use scan filter: no"));
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, string("config: coordinates: "));
    TEST_CHECK_LOG_RECORD_GW_CFG(
        ESP_LOG_INFO,
        string("config: fw_update: url: https://network.ruuvi.com/firmwareupdate"));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
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
    ASSERT_EQ(HTTP_CONENT_TYPE_APPLICATION_JSON, resp.content_type);
    ASSERT_EQ(nullptr, resp.p_content_type_param);
    ASSERT_EQ(strlen(expected_resp), resp.content_len);
    ASSERT_EQ(HTTP_CONENT_ENCODING_NONE, resp.content_encoding);
    ASSERT_NE(nullptr, resp.select_location.memory.p_buf);
    ASSERT_EQ(string(expected_resp), string(reinterpret_cast<const char*>(resp.select_location.memory.p_buf)));
    TEST_CHECK_LOG_RECORD_HTTP_SERVER(ESP_LOG_DEBUG, string("http_server_cb_on_post /ruuvi.json, params="));
    TEST_CHECK_LOG_RECORD_HTTP_SERVER(ESP_LOG_DEBUG, string("POST /ruuvi.json, flag_access_from_lan=0"));

    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, "Gateway SETTINGS (via HTTP):");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "remote_cfg_use: 0");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "remote_cfg_url: ");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "remote_cfg_auth_type: none");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "remote_cfg_use_ssl_client_cert: 0");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "remote_cfg_use_ssl_server_cert: 0");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "remote_cfg_refresh_interval_minutes: 0");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "use_http_ruuvi: 0");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "use_http: 0");
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
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "scan_coded_phy: not found");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_WARN, "Can't find key 'scan_coded_phy' in config-json");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "scan_1mbit_phy: not found");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_WARN, "Can't find key 'scan_1mbit_phy' in config-json");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "scan_extended_payload: not found");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_WARN, "Can't find key 'scan_extended_payload' in config-json");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "scan_channel_37: not found");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_WARN, "Can't find key 'scan_channel_37' in config-json");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "scan_channel_38: not found");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_WARN, "Can't find key 'scan_channel_38' in config-json");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "scan_channel_39: not found");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_WARN, "Can't find key 'scan_channel_39' in config-json");
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
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, string("config: NTP: Server3: time.nist.gov"));
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, string("config: NTP: Server4: pool.ntp.org"));
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, string("config: use company id filter: 1"));
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, string("config: company id: 0x0499"));
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, string("config: use scan coded phy: 0"));
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, string("config: use scan 1mbit/phy: 1"));
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, string("config: use scan extended payload: 1"));
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, string("config: use scan channel 37: 1"));
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, string("config: use scan channel 38: 1"));
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, string("config: use scan channel 39: 1"));
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, string("config: use scan filter: no"));
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, string("config: coordinates: "));
    TEST_CHECK_LOG_RECORD_GW_CFG(
        ESP_LOG_INFO,
        string("config: fw_update: url: https://network.ruuvi.com/firmwareupdate"));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
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
    ASSERT_EQ(HTTP_CONENT_TYPE_APPLICATION_JSON, resp.content_type);
    ASSERT_EQ(nullptr, resp.p_content_type_param);
    ASSERT_EQ(strlen(expected_resp), resp.content_len);
    ASSERT_EQ(HTTP_CONENT_ENCODING_NONE, resp.content_encoding);
    ASSERT_NE(nullptr, resp.select_location.memory.p_buf);
    ASSERT_EQ(string(expected_resp), string(reinterpret_cast<const char*>(resp.select_location.memory.p_buf)));
    TEST_CHECK_LOG_RECORD_HTTP_SERVER(ESP_LOG_DEBUG, string("http_server_cb_on_post /ruuvi.json, params="));
    TEST_CHECK_LOG_RECORD_HTTP_SERVER(ESP_LOG_DEBUG, string("POST /ruuvi.json, flag_access_from_lan=0"));

    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, "Gateway SETTINGS (via HTTP):");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "remote_cfg_use: 0");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "remote_cfg_url: ");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "remote_cfg_auth_type: none");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "remote_cfg_use_ssl_client_cert: 0");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "remote_cfg_use_ssl_server_cert: 0");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "remote_cfg_refresh_interval_minutes: 0");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "use_http_ruuvi: 0");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "use_http: 0");
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
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "scan_coded_phy: not found");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_WARN, "Can't find key 'scan_coded_phy' in config-json");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "scan_1mbit_phy: not found");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_WARN, "Can't find key 'scan_1mbit_phy' in config-json");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "scan_extended_payload: not found");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_WARN, "Can't find key 'scan_extended_payload' in config-json");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "scan_channel_37: not found");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_WARN, "Can't find key 'scan_channel_37' in config-json");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "scan_channel_38: not found");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_WARN, "Can't find key 'scan_channel_38' in config-json");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_DEBUG, "scan_channel_39: not found");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_WARN, "Can't find key 'scan_channel_39' in config-json");
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
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, string("config: NTP: Server3: time.nist.gov"));
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, string("config: NTP: Server4: pool.ntp.org"));
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, string("config: use company id filter: 1"));
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, string("config: company id: 0x0499"));
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, string("config: use scan coded phy: 0"));
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, string("config: use scan 1mbit/phy: 1"));
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, string("config: use scan extended payload: 1"));
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, string("config: use scan channel 37: 1"));
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, string("config: use scan channel 38: 1"));
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, string("config: use scan channel 39: 1"));
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, string("config: use scan filter: no"));
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, string("config: coordinates: "));
    TEST_CHECK_LOG_RECORD_GW_CFG(
        ESP_LOG_INFO,
        string("config: fw_update: url: https://network.ruuvi.com/firmwareupdate"));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
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
    ASSERT_EQ(HTTP_CONENT_TYPE_TEXT_HTML, resp.content_type);
    ASSERT_EQ(nullptr, resp.p_content_type_param);
    ASSERT_EQ(0, resp.content_len);
    ASSERT_EQ(HTTP_CONENT_ENCODING_NONE, resp.content_encoding);
    ASSERT_EQ(nullptr, resp.select_location.memory.p_buf);
    TEST_CHECK_LOG_RECORD_HTTP_SERVER(ESP_LOG_DEBUG, string("http_server_cb_on_post /unknown.json, params="));
    TEST_CHECK_LOG_RECORD_HTTP_SERVER(ESP_LOG_WARN, string("POST /unknown.json"));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
}

TEST_F(TestHttpServerCb, http_server_cb_on_delete) // NOLINT
{
    const http_server_resp_t resp = http_server_cb_on_delete("unknown.json", nullptr, false, nullptr);

    ASSERT_EQ(HTTP_RESP_CODE_404, resp.http_resp_code);
    ASSERT_EQ(HTTP_CONTENT_LOCATION_NO_CONTENT, resp.content_location);
    ASSERT_TRUE(resp.flag_no_cache);
    ASSERT_EQ(HTTP_CONENT_TYPE_TEXT_HTML, resp.content_type);
    ASSERT_EQ(nullptr, resp.p_content_type_param);
    ASSERT_EQ(0, resp.content_len);
    ASSERT_EQ(HTTP_CONENT_ENCODING_NONE, resp.content_encoding);
    ASSERT_EQ(nullptr, resp.select_location.memory.p_buf);
    TEST_CHECK_LOG_RECORD_HTTP_SERVER(ESP_LOG_INFO, string("DELETE /unknown.json, params="));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
}
