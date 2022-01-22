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
#include "gw_cfg_default.h"
#include "nrf52fw.h"

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
static TestHttpServerCb *g_pTestClass;

struct flash_fat_fs_t
{
    string                   mount_point;
    string                   partition_label;
    flash_fat_fs_num_files_t max_files;
};

extern "C" {

volatile uint32_t g_cnt_cfg_button_pressed;

const char *
os_task_get_name(void)
{
    static const char g_task_name[] = "main";
    return const_cast<char *>(g_task_name);
}

uint32_t
esp_random(void)
{
    return 0;
}

const char *
fw_update_get_cur_version(void)
{
    return "v1.3.3";
}

nrf52fw_version_str_t g_nrf52_firmware_version = { "v0.7.1" };

bool
http_download(
    const char *const          p_url,
    http_download_cb_on_data_t cb_on_data,
    void *const                p_user_data,
    const bool                 flag_feed_task_watchdog)
{
    return false;
}

void
fw_update_set_extra_info_for_status_json_update_start(void)
{
}

void
fw_update_set_extra_info_for_status_json_update_failed(const char *const p_message)
{
}

bool
fw_update_is_url_valid(void)
{
    return true;
}

bool
json_fw_update_parse_http_body(const char *const p_body)
{
    return false;
}

bool
fw_update_run(const fw_updating_reason_e fw_updating_reason)
{
    return true;
}

mac_address_str_t
nrf52_get_mac_address_str(void)
{
    mac_address_str_t mac_str = { 0 };
    str_buf_t         str_buf = {
        .buf  = mac_str.str_buf,
        .size = sizeof(mac_str.str_buf),
        .idx  = 0,
    };
    str_buf_printf(&str_buf, "AA:BB:CC:DD:EE:FF");
    return mac_str;
}

nrf52_device_id_str_t
nrf52_get_device_id_str(void)
{
    nrf52_device_id_str_t dev_id_str = { 0 };
    str_buf_t             str_buf    = {
        .buf  = dev_id_str.str_buf,
        .size = sizeof(dev_id_str.str_buf),
        .idx  = 0,
    };
    str_buf_printf(&str_buf, "11:22:33:44:55:66:77:88");
    return dev_id_str;
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
wifi_manager_stop_ap(void)
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
restart_services(void)
{
}

} // extern "C"

class MemAllocTrace
{
    vector<void *> allocated_mem;

    std::vector<void *>::iterator
    find(void *ptr)
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
    add(void *ptr)
    {
        auto iter = find(ptr);
        assert(iter == this->allocated_mem.end()); // ptr was found in the list of allocated memory blocks
        this->allocated_mem.push_back(ptr);
    }
    void
    remove(void *ptr)
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
        this->m_flag_settings_sent_to_nrf         = false;
        this->m_flag_settings_ethernet_ip_updated = false;

        gw_cfg_init();
        snprintf(g_gw_mac_sta_str.str_buf, sizeof(g_gw_mac_sta_str.str_buf), "11:22:33:44:55:66");

        gw_cfg_default_set_lan_auth_password("\xFFpassword_md5\xFF");
    }

    void
    TearDown() override
    {
        http_server_cb_deinit();
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
    bool                  m_flag_settings_sent_to_nrf;
    bool                  m_flag_settings_ethernet_ip_updated;
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
    , m_flag_settings_sent_to_nrf(false)
    , m_flag_settings_ethernet_ip_updated(false)
    , m_is_fatfs_mounted(false)
    , m_is_fatfs_mount_fail(false)
    , m_fd(-1)
    , Test()
{
}

extern "C" {

void *
os_malloc(const size_t size)
{
    if (++g_pTestClass->m_malloc_cnt == g_pTestClass->m_malloc_fail_on_cnt)
    {
        return nullptr;
    }
    void *ptr = malloc(size);
    assert(nullptr != ptr);
    g_pTestClass->m_mem_alloc_trace.add(ptr);
    return ptr;
}

void
os_free_internal(void *ptr)
{
    g_pTestClass->m_mem_alloc_trace.remove(ptr);
    free(ptr);
}

void *
os_calloc(const size_t nmemb, const size_t size)
{
    if (++g_pTestClass->m_malloc_cnt == g_pTestClass->m_malloc_fail_on_cnt)
    {
        return nullptr;
    }
    void *ptr = calloc(nmemb, size);
    assert(nullptr != ptr);
    g_pTestClass->m_mem_alloc_trace.add(ptr);
    return ptr;
}

ATTR_NONNULL(1)
bool
os_realloc_safe_and_clean(void **const p_ptr, const size_t size)
{
    void *ptr       = *p_ptr;
    void *p_new_ptr = realloc(ptr, size);
    if (NULL == p_new_ptr)
    {
        os_free(*p_ptr);
        return false;
    }
    *p_ptr = p_new_ptr;
    return true;
}

os_mutex_recursive_t
os_mutex_recursive_create_static(os_mutex_recursive_static_t *const p_mutex_static)
{
    return nullptr;
}

void
os_mutex_recursive_delete(os_mutex_recursive_t *const ph_mutex)
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

char *
metrics_generate(void)
{
    const char *p_metrics_str = "metrics_info";
    char *      p_buf         = static_cast<char *>(os_malloc(strlen(p_metrics_str) + 1));
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
adv_table_history_read(adv_report_table_t *const p_reports, const time_t cur_time, const uint32_t time_interval_seconds)
{
    (void)time_interval_seconds;
    p_reports->num_of_advs = 2;
    {
        adv_report_t *const p_adv   = &p_reports->table[0];
        p_adv->timestamp            = cur_time - 1;
        const mac_address_bin_t mac = { 0xAAU, 0xBBU, 0xCCU, 0x11U, 0x22U, 0x01U };
        p_adv->tag_mac              = mac;
        p_adv->rssi                 = 50;
        const uint8_t data_buf[]    = { 0x22U, 0x33U };
        p_adv->data_len             = sizeof(data_buf);
        memcpy(p_adv->data_buf, data_buf, sizeof(data_buf));
    }
    {
        adv_report_t *const p_adv   = &p_reports->table[1];
        p_adv->timestamp            = cur_time - 11;
        const mac_address_bin_t mac = { 0xAAU, 0xBBU, 0xCCU, 0x11U, 0x22U, 0x02U };
        p_adv->tag_mac              = mac;
        p_adv->rssi                 = 51;
        const uint8_t data_buf[]    = { 0x22U, 0x33U, 0x44U };
        p_adv->data_len             = sizeof(data_buf);
        memcpy(p_adv->data_buf, data_buf, sizeof(data_buf));
    }
}

bool
settings_save_to_flash(const char *const p_json_str)
{
    g_pTestClass->m_flag_settings_saved_to_flash = true;
    return true;
}

void
ethernet_update_ip(void)
{
    g_pTestClass->m_flag_settings_ethernet_ip_updated = true;
}

void
ruuvi_send_nrf_settings(const ruuvi_gateway_config_t *p_config)
{
    g_pTestClass->m_flag_settings_sent_to_nrf = true;
}

const flash_fat_fs_t *
flashfatfs_mount(const char *mount_point, const char *partition_label, const flash_fat_fs_num_files_t max_files)
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
flashfatfs_unmount(const flash_fat_fs_t **pp_ffs)
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
flashfatfs_get_file_size(const flash_fat_fs_t *p_ffs, const char *file_path, size_t *p_size)
{
    assert(p_ffs == &g_pTestClass->m_fatfs);
    for (auto &file : g_pTestClass->m_files)
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
flashfatfs_open(const flash_fat_fs_t *p_ffs, const char *file_path)
{
    assert(p_ffs == &g_pTestClass->m_fatfs);
    for (auto &file : g_pTestClass->m_files)
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
fw_update_set_url(const char *const p_url_fmt, ...)
{
    va_list ap;
    va_start(ap, p_url_fmt);
    vsnprintf(g_pTestClass->fw_update_url.data(), g_pTestClass->fw_update_url.size(), p_url_fmt, ap);
    va_end(ap);
}

const char *
fw_update_get_url(void)
{
    return g_pTestClass->fw_update_url.data();
}

} // extern "C"

TestHttpServerCb::~TestHttpServerCb() = default;

#define TEST_CHECK_LOG_RECORD_HTTP_SERVER(level_, msg_) \
    ESP_LOG_WRAPPER_TEST_CHECK_LOG_RECORD("http_server", level_, msg_)

#define TEST_CHECK_LOG_RECORD_GW_CFG(level_, msg_) ESP_LOG_WRAPPER_TEST_CHECK_LOG_RECORD("gw_cfg", level_, msg_)

static ruuvi_gateway_config_t
get_gateway_config_default()
{
    ruuvi_gateway_config_t gw_cfg {};
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
    const char *expected_json
        = "{\n"
          "\t\"fw_ver\":\t\"v1.3.3\",\n"
          "\t\"nrf52_fw_ver\":\t\"v0.7.1\",\n"
          "\t\"use_eth\":\tfalse,\n"
          "\t\"eth_dhcp\":\ttrue,\n"
          "\t\"eth_static_ip\":\t\"\",\n"
          "\t\"eth_netmask\":\t\"\",\n"
          "\t\"eth_gw\":\t\"\",\n"
          "\t\"eth_dns1\":\t\"\",\n"
          "\t\"eth_dns2\":\t\"\",\n"
          "\t\"use_http\":\tfalse,\n"
          "\t\"http_url\":\t\"" RUUVI_GATEWAY_HTTP_DEFAULT_URL
          "\",\n"
          "\t\"http_user\":\t\"\",\n"
          "\t\"use_http_stat\":\ttrue,\n"
          "\t\"http_stat_url\":\t\"" RUUVI_GATEWAY_HTTP_STATUS_URL
          "\",\n"
          "\t\"http_stat_user\":\t\"\",\n"
          "\t\"use_mqtt\":\ttrue,\n"
          "\t\"mqtt_transport\":\t\"TCP\",\n"
          "\t\"mqtt_server\":\t\"test.mosquitto.org\",\n"
          "\t\"mqtt_port\":\t1883,\n"
          "\t\"mqtt_prefix\":\t\"ruuvi/30:AE:A4:02:84:A4\",\n"
          "\t\"mqtt_client_id\":\t\"30:AE:A4:02:84:A4\",\n"
          "\t\"mqtt_user\":\t\"\",\n"
          "\t\"lan_auth_type\":\t\"lan_auth_default\",\n"
          "\t\"lan_auth_user\":\t\"Admin\",\n"
          "\t\"lan_auth_api_key\":\t\"wH3F9SIiAA3rhG32aJki2Z7ekdFc0vtxuDhxl39zFvw=\",\n"
          "\t\"auto_update_cycle\":\t\"regular\",\n"
          "\t\"auto_update_weekdays_bitmask\":\t127,\n"
          "\t\"auto_update_interval_from\":\t0,\n"
          "\t\"auto_update_interval_to\":\t24,\n"
          "\t\"auto_update_tz_offset_hours\":\t3,\n"
          "\t\"gw_mac\":\t\"11:22:33:44:55:66\",\n"
          "\t\"use_filtering\":\ttrue,\n"
          "\t\"company_id\":\t\"0x0499\",\n"
          "\t\"coordinates\":\t\"\",\n"
          "\t\"use_coded_phy\":\tfalse,\n"
          "\t\"use_1mbit_phy\":\ttrue,\n"
          "\t\"use_extended_payload\":\ttrue,\n"
          "\t\"use_channel_37\":\ttrue,\n"
          "\t\"use_channel_38\":\ttrue,\n"
          "\t\"use_channel_39\":\ttrue\n"
          "}";
    bool                   flag_network_cfg = false;
    ruuvi_gateway_config_t gw_cfg           = get_gateway_config_default();
    ASSERT_TRUE(
        json_ruuvi_parse_http_body(
            "{"
            "\"use_mqtt\":true,"
            "\"mqtt_server\":\"test.mosquitto.org\","
            "\"mqtt_port\":1883,"
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
            "\"lan_auth_api_key\":\"wH3F9SIiAA3rhG32aJki2Z7ekdFc0vtxuDhxl39zFvw=\","
            "\"auto_update_cycle\":\"regular\","
            "\"auto_update_weekdays_bitmask\":127,"
            "\"auto_update_interval_from\":0,"
            "\"auto_update_interval_to\":24,"
            "\"auto_update_tz_offset_hours\":3,"
            "\"use_filtering\":true"
            "}",
            &gw_cfg,
            &flag_network_cfg));
    gw_cfg_update(&gw_cfg, false);
    ASSERT_FALSE(flag_network_cfg);
    snprintf(g_gw_mac_sta_str.str_buf, sizeof(g_gw_mac_sta_str.str_buf), "11:22:33:44:55:66");

    esp_log_wrapper_clear();
    const http_server_resp_t resp = http_server_resp_json_ruuvi();

    ASSERT_EQ(HTTP_RESP_CODE_200, resp.http_resp_code);
    ASSERT_EQ(HTTP_CONTENT_LOCATION_HEAP, resp.content_location);
    ASSERT_TRUE(resp.flag_no_cache);
    ASSERT_EQ(HTTP_CONENT_TYPE_APPLICATION_JSON, resp.content_type);
    ASSERT_EQ(nullptr, resp.p_content_type_param);
    ASSERT_EQ(string(expected_json), string(reinterpret_cast<const char *>(resp.select_location.memory.p_buf)));
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

    bool                   flag_network_cfg = false;
    ruuvi_gateway_config_t gw_cfg           = get_gateway_config_default();
    ASSERT_FALSE(
        json_ruuvi_parse_http_body(
            "{"
            "\"use_mqtt\":true,"
            "\"mqtt_server\":\"test.mosquitto.org\","
            "\"mqtt_port\":1883,"
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
            "\"use_filtering\":true"
            "}",
            &gw_cfg,
            &flag_network_cfg));
    ASSERT_FALSE(flag_network_cfg);
}

TEST_F(TestHttpServerCb, resp_json_ruuvi_malloc_failed_2) // NOLINT
{
    bool                   flag_network_cfg = false;
    ruuvi_gateway_config_t gw_cfg           = get_gateway_config_default();
    ASSERT_TRUE(
        json_ruuvi_parse_http_body(
            "{"
            "\"use_mqtt\":true,"
            "\"mqtt_server\":\"test.mosquitto.org\","
            "\"mqtt_port\":1883,"
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
            "\"use_filtering\":true"
            "}",
            &gw_cfg,
            &flag_network_cfg));
    ASSERT_FALSE(flag_network_cfg);
    gw_cfg_update(&gw_cfg, false);
    snprintf(g_gw_mac_sta_str.str_buf, sizeof(g_gw_mac_sta_str.str_buf), "11:22:33:44:55:66");

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
    const char *expected_json
        = "{\n"
          "\t\"fw_ver\":\t\"v1.3.3\",\n"
          "\t\"nrf52_fw_ver\":\t\"v0.7.1\",\n"
          "\t\"use_eth\":\tfalse,\n"
          "\t\"eth_dhcp\":\ttrue,\n"
          "\t\"eth_static_ip\":\t\"\",\n"
          "\t\"eth_netmask\":\t\"\",\n"
          "\t\"eth_gw\":\t\"\",\n"
          "\t\"eth_dns1\":\t\"\",\n"
          "\t\"eth_dns2\":\t\"\",\n"
          "\t\"use_http\":\tfalse,\n"
          "\t\"http_url\":\t\"" RUUVI_GATEWAY_HTTP_DEFAULT_URL
          "\",\n"
          "\t\"http_user\":\t\"\",\n"
          "\t\"use_http_stat\":\ttrue,\n"
          "\t\"http_stat_url\":\t\"" RUUVI_GATEWAY_HTTP_STATUS_URL
          "\",\n"
          "\t\"http_stat_user\":\t\"\",\n"
          "\t\"use_mqtt\":\ttrue,\n"
          "\t\"mqtt_transport\":\t\"TCP\",\n"
          "\t\"mqtt_server\":\t\"test.mosquitto.org\",\n"
          "\t\"mqtt_port\":\t1883,\n"
          "\t\"mqtt_prefix\":\t\"ruuvi/30:AE:A4:02:84:A4\",\n"
          "\t\"mqtt_client_id\":\t\"30:AE:A4:02:84:A4\",\n"
          "\t\"mqtt_user\":\t\"\",\n"
          "\t\"lan_auth_type\":\t\"lan_auth_default\",\n"
          "\t\"lan_auth_user\":\t\"Admin\",\n"
          "\t\"lan_auth_api_key\":\t\"\",\n"
          "\t\"auto_update_cycle\":\t\"beta\",\n"
          "\t\"auto_update_weekdays_bitmask\":\t126,\n"
          "\t\"auto_update_interval_from\":\t1,\n"
          "\t\"auto_update_interval_to\":\t23,\n"
          "\t\"auto_update_tz_offset_hours\":\t-3,\n"
          "\t\"gw_mac\":\t\"11:22:33:44:55:66\",\n"
          "\t\"use_filtering\":\ttrue,\n"
          "\t\"company_id\":\t\"0x0499\",\n"
          "\t\"coordinates\":\t\"\",\n"
          "\t\"use_coded_phy\":\tfalse,\n"
          "\t\"use_1mbit_phy\":\ttrue,\n"
          "\t\"use_extended_payload\":\ttrue,\n"
          "\t\"use_channel_37\":\ttrue,\n"
          "\t\"use_channel_38\":\ttrue,\n"
          "\t\"use_channel_39\":\ttrue\n"
          "}";
    bool                   flag_network_cfg = false;
    ruuvi_gateway_config_t gw_cfg           = get_gateway_config_default();
    ASSERT_TRUE(
        json_ruuvi_parse_http_body(
            "{"
            "\"use_mqtt\":true,"
            "\"mqtt_server\":\"test.mosquitto.org\","
            "\"mqtt_port\":1883,"
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
            "\"use_filtering\":true"
            "}",
            &gw_cfg,
            &flag_network_cfg));
    ASSERT_FALSE(flag_network_cfg);
    gw_cfg_update(&gw_cfg, false);
    snprintf(g_gw_mac_sta_str.str_buf, sizeof(g_gw_mac_sta_str.str_buf), "11:22:33:44:55:66");

    esp_log_wrapper_clear();
    const http_server_resp_t resp = http_server_resp_json("ruuvi.json", false);

    ASSERT_EQ(HTTP_RESP_CODE_200, resp.http_resp_code);
    ASSERT_EQ(HTTP_CONTENT_LOCATION_HEAP, resp.content_location);
    ASSERT_TRUE(resp.flag_no_cache);
    ASSERT_EQ(HTTP_CONENT_TYPE_APPLICATION_JSON, resp.content_type);
    ASSERT_EQ(nullptr, resp.p_content_type_param);
    ASSERT_EQ(string(expected_json), string(reinterpret_cast<const char *>(resp.select_location.memory.p_buf)));
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
    const char *             expected_resp = "metrics_info";
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
    ASSERT_EQ(string(expected_resp), string(reinterpret_cast<const char *>(resp.select_location.memory.p_buf)));
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
    const char *            expected_resp = "index_html_content";
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
    const char *            file_name     = "a1234567890123456789012345678901234567890123456789012345678901234567890";
    const char *            expected_resp = "index_html_content";
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
    const char *            expected_resp = "index_html_content";
    const file_descriptor_t fd            = 1;
    ASSERT_TRUE(http_server_cb_init(GW_GWUI_PARTITION));
    FileInfo fileInfo = FileInfo("index.html", expected_resp);
    this->m_files.emplace_back(fileInfo);
    this->m_fd = fd;

    const http_server_resp_t resp = http_server_resp_file("index.html", HTTP_RESP_CODE_200);
    ASSERT_EQ(HTTP_RESP_CODE_200, resp.http_resp_code);
    ASSERT_EQ(HTTP_CONTENT_LOCATION_FATFS, resp.content_location);
    ASSERT_FALSE(resp.flag_no_cache);
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
    const char *            expected_resp = "index_html_content";
    const file_descriptor_t fd            = 2;
    ASSERT_TRUE(http_server_cb_init(GW_GWUI_PARTITION));
    FileInfo fileInfo = FileInfo("index.html.gz", expected_resp);
    this->m_files.emplace_back(fileInfo);
    this->m_fd = fd;

    const http_server_resp_t resp = http_server_resp_file("index.html", HTTP_RESP_CODE_200);
    ASSERT_EQ(HTTP_RESP_CODE_200, resp.http_resp_code);
    ASSERT_EQ(HTTP_CONTENT_LOCATION_FATFS, resp.content_location);
    ASSERT_FALSE(resp.flag_no_cache);
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
    const char *            expected_resp = "app_js_gzipped";
    const file_descriptor_t fd            = 1;
    ASSERT_TRUE(http_server_cb_init(GW_GWUI_PARTITION));
    FileInfo fileInfo = FileInfo("app.js.gz", expected_resp);
    this->m_files.emplace_back(fileInfo);
    this->m_fd = fd;

    const http_server_resp_t resp = http_server_resp_file("app.js", HTTP_RESP_CODE_200);
    ASSERT_EQ(HTTP_RESP_CODE_200, resp.http_resp_code);
    ASSERT_EQ(HTTP_CONTENT_LOCATION_FATFS, resp.content_location);
    ASSERT_FALSE(resp.flag_no_cache);
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
    const char *            expected_resp = "slyle_css_gzipped";
    const file_descriptor_t fd            = 1;
    ASSERT_TRUE(http_server_cb_init(GW_GWUI_PARTITION));
    FileInfo fileInfo = FileInfo("style.css.gz", expected_resp);
    this->m_files.emplace_back(fileInfo);
    this->m_fd = fd;

    const http_server_resp_t resp = http_server_resp_file("style.css", HTTP_RESP_CODE_200);
    ASSERT_EQ(HTTP_RESP_CODE_200, resp.http_resp_code);
    ASSERT_EQ(HTTP_CONTENT_LOCATION_FATFS, resp.content_location);
    ASSERT_FALSE(resp.flag_no_cache);
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
    const char *            expected_resp = "binary_data";
    const file_descriptor_t fd            = 1;
    ASSERT_TRUE(http_server_cb_init(GW_GWUI_PARTITION));
    FileInfo fileInfo = FileInfo("binary", expected_resp);
    this->m_files.emplace_back(fileInfo);
    this->m_fd = fd;

    const http_server_resp_t resp = http_server_resp_file("binary", HTTP_RESP_CODE_200);
    ASSERT_EQ(HTTP_RESP_CODE_200, resp.http_resp_code);
    ASSERT_EQ(HTTP_CONTENT_LOCATION_FATFS, resp.content_location);
    ASSERT_FALSE(resp.flag_no_cache);
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
    const char *            expected_resp = "index_html_content";
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

    const http_server_resp_t resp = http_server_cb_on_get("", false, nullptr);
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
    const char *            expected_resp = "index_html_content";
    const file_descriptor_t fd            = 1;
    ASSERT_TRUE(http_server_cb_init(GW_GWUI_PARTITION));
    FileInfo fileInfo = FileInfo("index.html.gz", expected_resp);
    this->m_files.emplace_back(fileInfo);
    this->m_fd = fd;

    const http_server_resp_t resp = http_server_cb_on_get("index.html", false, nullptr);
    ASSERT_EQ(HTTP_RESP_CODE_200, resp.http_resp_code);
    ASSERT_EQ(HTTP_CONTENT_LOCATION_FATFS, resp.content_location);
    ASSERT_FALSE(resp.flag_no_cache);
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
    const char *            expected_resp = "app_js_gzipped";
    const file_descriptor_t fd            = 1;
    ASSERT_TRUE(http_server_cb_init(GW_GWUI_PARTITION));
    FileInfo fileInfo = FileInfo("app.js.gz", expected_resp);
    this->m_files.emplace_back(fileInfo);
    this->m_fd = fd;

    const http_server_resp_t resp = http_server_cb_on_get("app.js", false, nullptr);
    ASSERT_EQ(HTTP_RESP_CODE_200, resp.http_resp_code);
    ASSERT_EQ(HTTP_CONTENT_LOCATION_FATFS, resp.content_location);
    ASSERT_FALSE(resp.flag_no_cache);
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
    const char *expected_json
        = "{\n"
          "\t\"fw_ver\":\t\"v1.3.3\",\n"
          "\t\"nrf52_fw_ver\":\t\"v0.7.1\",\n"
          "\t\"use_eth\":\tfalse,\n"
          "\t\"eth_dhcp\":\ttrue,\n"
          "\t\"eth_static_ip\":\t\"\",\n"
          "\t\"eth_netmask\":\t\"\",\n"
          "\t\"eth_gw\":\t\"\",\n"
          "\t\"eth_dns1\":\t\"\",\n"
          "\t\"eth_dns2\":\t\"\",\n"
          "\t\"use_http\":\tfalse,\n"
          "\t\"http_url\":\t\"" RUUVI_GATEWAY_HTTP_DEFAULT_URL
          "\",\n"
          "\t\"http_user\":\t\"\",\n"
          "\t\"use_http_stat\":\ttrue,\n"
          "\t\"http_stat_url\":\t\"" RUUVI_GATEWAY_HTTP_STATUS_URL
          "\",\n"
          "\t\"http_stat_user\":\t\"\",\n"
          "\t\"use_mqtt\":\ttrue,\n"
          "\t\"mqtt_transport\":\t\"TCP\",\n"
          "\t\"mqtt_server\":\t\"test.mosquitto.org\",\n"
          "\t\"mqtt_port\":\t1883,\n"
          "\t\"mqtt_prefix\":\t\"ruuvi/30:AE:A4:02:84:A4\",\n"
          "\t\"mqtt_client_id\":\t\"30:AE:A4:02:84:A4\",\n"
          "\t\"mqtt_user\":\t\"\",\n"
          "\t\"lan_auth_type\":\t\"lan_auth_default\",\n"
          "\t\"lan_auth_user\":\t\"Admin\",\n"
          "\t\"lan_auth_api_key\":\t\"6kl/fd/c+3qvWm3Mhmwgh3BWNp+HDRQiLp/X0PuwG8Q=\",\n"
          "\t\"auto_update_cycle\":\t\"regular\",\n"
          "\t\"auto_update_weekdays_bitmask\":\t127,\n"
          "\t\"auto_update_interval_from\":\t0,\n"
          "\t\"auto_update_interval_to\":\t24,\n"
          "\t\"auto_update_tz_offset_hours\":\t3,\n"
          "\t\"gw_mac\":\t\"11:22:33:44:55:66\",\n"
          "\t\"use_filtering\":\ttrue,\n"
          "\t\"company_id\":\t\"0x0499\",\n"
          "\t\"coordinates\":\t\"\",\n"
          "\t\"use_coded_phy\":\tfalse,\n"
          "\t\"use_1mbit_phy\":\ttrue,\n"
          "\t\"use_extended_payload\":\ttrue,\n"
          "\t\"use_channel_37\":\ttrue,\n"
          "\t\"use_channel_38\":\ttrue,\n"
          "\t\"use_channel_39\":\ttrue\n"
          "}";
    bool                   flag_network_cfg = false;
    ruuvi_gateway_config_t gw_cfg           = get_gateway_config_default();
    ASSERT_TRUE(
        json_ruuvi_parse_http_body(
            "{"
            "\"use_mqtt\":true,"
            "\"mqtt_server\":\"test.mosquitto.org\","
            "\"mqtt_port\":1883,"
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
            "\"lan_auth_api_key\":\"6kl/fd/c+3qvWm3Mhmwgh3BWNp+HDRQiLp/X0PuwG8Q=\","
            "\"use_filtering\":true"
            "}",
            &gw_cfg,
            &flag_network_cfg));
    ASSERT_FALSE(flag_network_cfg);
    gw_cfg_update(&gw_cfg, false);
    snprintf(g_gw_mac_sta_str.str_buf, sizeof(g_gw_mac_sta_str.str_buf), "11:22:33:44:55:66");

    esp_log_wrapper_clear();
    const http_server_resp_t resp = http_server_cb_on_get("ruuvi.json", false, nullptr);

    ASSERT_EQ(HTTP_RESP_CODE_200, resp.http_resp_code);
    ASSERT_EQ(HTTP_CONTENT_LOCATION_HEAP, resp.content_location);
    ASSERT_TRUE(resp.flag_no_cache);
    ASSERT_EQ(HTTP_CONENT_TYPE_APPLICATION_JSON, resp.content_type);
    ASSERT_EQ(nullptr, resp.p_content_type_param);
    ASSERT_EQ(string(expected_json), string(reinterpret_cast<const char *>(resp.select_location.memory.p_buf)));
    ASSERT_EQ(strlen(expected_json), resp.content_len);
    ASSERT_EQ(HTTP_CONENT_ENCODING_NONE, resp.content_encoding);
    ASSERT_NE(nullptr, resp.select_location.memory.p_buf);
    TEST_CHECK_LOG_RECORD_HTTP_SERVER(ESP_LOG_DEBUG, string("http_server_cb_on_get /ruuvi.json"));
    TEST_CHECK_LOG_RECORD_HTTP_SERVER(ESP_LOG_INFO, string("ruuvi.json: ") + string(expected_json));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
}

TEST_F(TestHttpServerCb, http_server_cb_on_get_metrics) // NOLINT
{
    const char *             expected_resp = "metrics_info";
    const http_server_resp_t resp          = http_server_cb_on_get("metrics", false, nullptr);

    ASSERT_EQ(HTTP_RESP_CODE_200, resp.http_resp_code);
    ASSERT_EQ(HTTP_CONTENT_LOCATION_HEAP, resp.content_location);
    ASSERT_TRUE(resp.flag_no_cache);
    ASSERT_EQ(HTTP_CONENT_TYPE_TEXT_PLAIN, resp.content_type);
    ASSERT_NE(nullptr, resp.p_content_type_param);
    ASSERT_EQ(string("version=0.0.4"), string(resp.p_content_type_param));
    ASSERT_EQ(strlen(expected_resp), resp.content_len);
    ASSERT_EQ(HTTP_CONENT_ENCODING_NONE, resp.content_encoding);
    ASSERT_NE(nullptr, resp.select_location.memory.p_buf);
    ASSERT_EQ(string(expected_resp), string(reinterpret_cast<const char *>(resp.select_location.memory.p_buf)));
    TEST_CHECK_LOG_RECORD_HTTP_SERVER(ESP_LOG_DEBUG, string("http_server_cb_on_get /metrics"));
    TEST_CHECK_LOG_RECORD_HTTP_SERVER(ESP_LOG_INFO, string("metrics: ") + string(expected_resp));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
}

TEST_F(TestHttpServerCb, http_server_cb_on_get_history) // NOLINT
{
    const char *expected_resp
        = "{\n"
          "\t\"data\":\t{\n"
          "\t\t\"coordinates\":\t\"\",\n"
          "\t\t\"timestamp\":\t\"1615660220\",\n"
          "\t\t\"gw_mac\":\t\"11:22:33:44:55:66\",\n"
          "\t\t\"tags\":\t{\n\t\t\t\"AA:BB:CC:11:22:01\":\t{\n"
          "\t\t\t\t\"rssi\":\t50,\n"
          "\t\t\t\t\"timestamp\":\t\"1615660219\",\n"
          "\t\t\t\t\"data\":\t\"2233\"\n"
          "\t\t\t},\n"
          "\t\t\t\"AA:BB:CC:11:22:02\":\t{\n"
          "\t\t\t\t\"rssi\":\t51,\n"
          "\t\t\t\t\"timestamp\":\t\"1615660209\",\n"
          "\t\t\t\t\"data\":\t\"223344\"\n"
          "\t\t\t}\n"
          "\t\t}\n"
          "\t}\n"
          "}";
    const http_server_resp_t resp = http_server_cb_on_get("history", false, nullptr);

    ASSERT_EQ(HTTP_RESP_CODE_200, resp.http_resp_code);
    ASSERT_EQ(HTTP_CONTENT_LOCATION_HEAP, resp.content_location);
    ASSERT_TRUE(resp.flag_no_cache);
    ASSERT_EQ(HTTP_CONENT_TYPE_APPLICATION_JSON, resp.content_type);
    ASSERT_EQ(nullptr, resp.p_content_type_param);
    ASSERT_EQ(HTTP_CONENT_ENCODING_NONE, resp.content_encoding);
    ASSERT_NE(nullptr, resp.select_location.memory.p_buf);
    ASSERT_EQ(string(expected_resp), string(reinterpret_cast<const char *>(resp.select_location.memory.p_buf)));
    ASSERT_EQ(strlen(expected_resp), resp.content_len);
    TEST_CHECK_LOG_RECORD_HTTP_SERVER(ESP_LOG_DEBUG, string("http_server_cb_on_get /history"));
    TEST_CHECK_LOG_RECORD_HTTP_SERVER(ESP_LOG_INFO, string("History on 60 seconds interval: ") + string(expected_resp));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
}

TEST_F(TestHttpServerCb, http_server_cb_on_get_history_with_time_interval_20) // NOLINT
{
    const char *expected_resp
        = "{\n"
          "\t\"data\":\t{\n"
          "\t\t\"coordinates\":\t\"\",\n"
          "\t\t\"timestamp\":\t\"1615660220\",\n"
          "\t\t\"gw_mac\":\t\"11:22:33:44:55:66\",\n"
          "\t\t\"tags\":\t{\n\t\t\t\"AA:BB:CC:11:22:01\":\t{\n"
          "\t\t\t\t\"rssi\":\t50,\n"
          "\t\t\t\t\"timestamp\":\t\"1615660219\",\n"
          "\t\t\t\t\"data\":\t\"2233\"\n"
          "\t\t\t},\n"
          "\t\t\t\"AA:BB:CC:11:22:02\":\t{\n"
          "\t\t\t\t\"rssi\":\t51,\n"
          "\t\t\t\t\"timestamp\":\t\"1615660209\",\n"
          "\t\t\t\t\"data\":\t\"223344\"\n"
          "\t\t\t}\n"
          "\t\t}\n"
          "\t}\n"
          "}";
    const http_server_resp_t resp = http_server_cb_on_get("history?time=20", false, nullptr);

    ASSERT_EQ(HTTP_RESP_CODE_200, resp.http_resp_code);
    ASSERT_EQ(HTTP_CONTENT_LOCATION_HEAP, resp.content_location);
    ASSERT_TRUE(resp.flag_no_cache);
    ASSERT_EQ(HTTP_CONENT_TYPE_APPLICATION_JSON, resp.content_type);
    ASSERT_EQ(nullptr, resp.p_content_type_param);
    ASSERT_EQ(strlen(expected_resp), resp.content_len);
    ASSERT_EQ(HTTP_CONENT_ENCODING_NONE, resp.content_encoding);
    ASSERT_NE(nullptr, resp.select_location.memory.p_buf);
    ASSERT_EQ(string(expected_resp), string(reinterpret_cast<const char *>(resp.select_location.memory.p_buf)));
    TEST_CHECK_LOG_RECORD_HTTP_SERVER(ESP_LOG_DEBUG, string("http_server_cb_on_get /history?time=20"));
    TEST_CHECK_LOG_RECORD_HTTP_SERVER(ESP_LOG_INFO, string("History on 20 seconds interval: ") + string(expected_resp));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
}

TEST_F(TestHttpServerCb, http_server_cb_on_post_ruuvi_ok_mqtt_tcp) // NOLINT
{
    const char *             expected_resp = "{}";
    const http_server_resp_t resp          = http_server_cb_on_post_ruuvi(
        "{"
        "\"use_mqtt\":true,"
        "\"mqtt_transport\":\"TCP\","
        "\"mqtt_server\":\"test.mosquitto.org\","
        "\"mqtt_port\":1883,"
        "\"mqtt_prefix\":\"ruuvi/30:AE:A4:02:84:A4\","
        "\"mqtt_client_id\":\"30:AE:A4:02:84:A4\","
        "\"mqtt_user\":\"\","
        "\"mqtt_pass\":\"\","
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
        "\"use_filtering\":true"
        "}");

    ASSERT_TRUE(this->m_flag_settings_saved_to_flash);
    ASSERT_TRUE(this->m_flag_settings_sent_to_nrf);
    ASSERT_TRUE(this->m_flag_settings_ethernet_ip_updated);

    ASSERT_EQ(HTTP_RESP_CODE_200, resp.http_resp_code);
    ASSERT_EQ(HTTP_CONTENT_LOCATION_FLASH_MEM, resp.content_location);
    ASSERT_FALSE(resp.flag_no_cache);
    ASSERT_EQ(HTTP_CONENT_TYPE_APPLICATION_JSON, resp.content_type);
    ASSERT_EQ(nullptr, resp.p_content_type_param);
    ASSERT_EQ(strlen(expected_resp), resp.content_len);
    ASSERT_EQ(HTTP_CONENT_ENCODING_NONE, resp.content_encoding);
    ASSERT_NE(nullptr, resp.select_location.memory.p_buf);
    ASSERT_EQ(string(expected_resp), string(reinterpret_cast<const char *>(resp.select_location.memory.p_buf)));
    TEST_CHECK_LOG_RECORD_HTTP_SERVER(ESP_LOG_DEBUG, string("POST /ruuvi.json"));
    TEST_CHECK_LOG_RECORD_HTTP_SERVER(ESP_LOG_DEBUG, "Got SETTINGS:");
    TEST_CHECK_LOG_RECORD_HTTP_SERVER(ESP_LOG_DEBUG, "use_mqtt: 1");
    TEST_CHECK_LOG_RECORD_HTTP_SERVER(ESP_LOG_DEBUG, "mqtt_transport: TCP");
    TEST_CHECK_LOG_RECORD_HTTP_SERVER(ESP_LOG_DEBUG, "mqtt_server: test.mosquitto.org");
    TEST_CHECK_LOG_RECORD_HTTP_SERVER(ESP_LOG_DEBUG, "mqtt_prefix: ruuvi/30:AE:A4:02:84:A4");
    TEST_CHECK_LOG_RECORD_HTTP_SERVER(ESP_LOG_DEBUG, "mqtt_client_id: 30:AE:A4:02:84:A4");
    TEST_CHECK_LOG_RECORD_HTTP_SERVER(ESP_LOG_DEBUG, "mqtt_port: 1883");
    TEST_CHECK_LOG_RECORD_HTTP_SERVER(ESP_LOG_DEBUG, "mqtt_user: ");
    TEST_CHECK_LOG_RECORD_HTTP_SERVER(ESP_LOG_DEBUG, "mqtt_pass: ");
    TEST_CHECK_LOG_RECORD_HTTP_SERVER(ESP_LOG_DEBUG, "use_http: 0");
    TEST_CHECK_LOG_RECORD_HTTP_SERVER(ESP_LOG_DEBUG, "http_url: " RUUVI_GATEWAY_HTTP_DEFAULT_URL);
    TEST_CHECK_LOG_RECORD_HTTP_SERVER(ESP_LOG_DEBUG, "http_user: ");
    TEST_CHECK_LOG_RECORD_HTTP_SERVER(ESP_LOG_DEBUG, "http_pass: ");
    TEST_CHECK_LOG_RECORD_HTTP_SERVER(ESP_LOG_DEBUG, "use_http_stat: 1");
    TEST_CHECK_LOG_RECORD_HTTP_SERVER(ESP_LOG_DEBUG, "http_stat_url: " RUUVI_GATEWAY_HTTP_STATUS_URL);
    TEST_CHECK_LOG_RECORD_HTTP_SERVER(ESP_LOG_DEBUG, "http_stat_user: ");
    TEST_CHECK_LOG_RECORD_HTTP_SERVER(ESP_LOG_DEBUG, "http_stat_pass: ");
    TEST_CHECK_LOG_RECORD_HTTP_SERVER(ESP_LOG_ERROR, "lan_auth_type not found");
    TEST_CHECK_LOG_RECORD_HTTP_SERVER(ESP_LOG_INFO, "Use previous LAN auth settings");
    TEST_CHECK_LOG_RECORD_HTTP_SERVER(ESP_LOG_ERROR, "lan_auth_api_key not found");
    TEST_CHECK_LOG_RECORD_HTTP_SERVER(ESP_LOG_ERROR, "auto_update_cycle not found");
    TEST_CHECK_LOG_RECORD_HTTP_SERVER(ESP_LOG_ERROR, "auto_update_weekdays_bitmask not found or invalid");
    TEST_CHECK_LOG_RECORD_HTTP_SERVER(ESP_LOG_ERROR, "auto_update_interval_from not found or invalid");
    TEST_CHECK_LOG_RECORD_HTTP_SERVER(ESP_LOG_ERROR, "auto_update_interval_to not found or invalid");
    TEST_CHECK_LOG_RECORD_HTTP_SERVER(ESP_LOG_ERROR, "auto_update_tz_offset_hours not found or invalid");
    TEST_CHECK_LOG_RECORD_HTTP_SERVER(ESP_LOG_DEBUG, "use_filtering: 1");
    TEST_CHECK_LOG_RECORD_HTTP_SERVER(ESP_LOG_ERROR, "company_id not found or invalid");
    TEST_CHECK_LOG_RECORD_HTTP_SERVER(ESP_LOG_ERROR, "coordinates not found");
    TEST_CHECK_LOG_RECORD_HTTP_SERVER(ESP_LOG_ERROR, "use_coded_phy not found");
    TEST_CHECK_LOG_RECORD_HTTP_SERVER(ESP_LOG_ERROR, "use_1mbit_phy not found");
    TEST_CHECK_LOG_RECORD_HTTP_SERVER(ESP_LOG_ERROR, "use_extended_payload not found");
    TEST_CHECK_LOG_RECORD_HTTP_SERVER(ESP_LOG_ERROR, "use_channel_37 not found");
    TEST_CHECK_LOG_RECORD_HTTP_SERVER(ESP_LOG_ERROR, "use_channel_38 not found");
    TEST_CHECK_LOG_RECORD_HTTP_SERVER(ESP_LOG_ERROR, "use_channel_39 not found");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, string("Gateway SETTINGS:"));
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, string("config: use eth: 0"));
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, string("config: use eth dhcp: 1"));
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, string("config: eth static ip: "));
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, string("config: eth netmask: "));
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, string("config: eth gw: "));
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, string("config: eth dns1: "));
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, string("config: eth dns2: "));
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, string("config: use mqtt: 1"));
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, string("config: mqtt transport: TCP"));
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, string("config: mqtt server: test.mosquitto.org"));
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, string("config: mqtt port: 1883"));
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, string("config: mqtt use default prefix: 0"));
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, string("config: mqtt prefix: ruuvi/30:AE:A4:02:84:A4"));
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, string("config: mqtt client id: 30:AE:A4:02:84:A4"));
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, string("config: mqtt user: "));
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, string("config: mqtt password: ********"));
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, string("config: use http: 0"));
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, string("config: http url: " RUUVI_GATEWAY_HTTP_DEFAULT_URL));
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, string("config: http user: "));
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, string("config: http pass: ********"));
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, string("config: use http_stat: 1"));
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, string("config: http_stat url: " RUUVI_GATEWAY_HTTP_STATUS_URL));
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, string("config: http_stat user: "));
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, string("config: http_stat pass: ********"));
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, string("config: LAN auth type: lan_auth_ruuvi"));
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, string("config: LAN auth user: Admin"));
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, string("config: LAN auth pass: ********"));
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, string("config: LAN auth API key: "));
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, string("config: Auto update cycle: regular"));
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, string("config: Auto update weekdays_bitmask: 0x7f"));
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, string("config: Auto update interval: 00:00..24:00"));
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, string("config: Auto update TZ: UTC+3"));
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, string("config: coordinates: "));
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, string("config: use company id filter: 1"));
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, string("config: company id: 0x0499"));
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, string("config: use scan coded phy: 0"));
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, string("config: use scan 1mbit/phy: 1"));
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, string("config: use scan extended payload: 1"));
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, string("config: use scan channel 37: 1"));
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, string("config: use scan channel 38: 1"));
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, string("config: use scan channel 39: 1"));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
}

TEST_F(TestHttpServerCb, http_server_cb_on_post_ruuvi_malloc_failed1) // NOLINT
{
    this->m_malloc_cnt            = 0;
    this->m_malloc_fail_on_cnt    = 1;
    const http_server_resp_t resp = http_server_cb_on_post_ruuvi(
        "{"
        "\"use_mqtt\":true,"
        "\"mqtt_server\":\"test.mosquitto.org\","
        "\"mqtt_port\":1883,"
        "\"mqtt_prefix\":\"ruuvi/30:AE:A4:02:84:A4\","
        "\"mqtt_client_id\":\"30:AE:A4:02:84:A4\","
        "\"mqtt_user\":\"\","
        "\"mqtt_pass\":\"\","
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
        "\"use_filtering\":true"
        "}");

    ASSERT_FALSE(this->m_flag_settings_saved_to_flash);
    ASSERT_FALSE(this->m_flag_settings_sent_to_nrf);
    ASSERT_FALSE(this->m_flag_settings_ethernet_ip_updated);

    ASSERT_EQ(HTTP_RESP_CODE_503, resp.http_resp_code);
    ASSERT_EQ(HTTP_CONTENT_LOCATION_NO_CONTENT, resp.content_location);
    ASSERT_TRUE(resp.flag_no_cache);
    ASSERT_EQ(HTTP_CONENT_TYPE_TEXT_HTML, resp.content_type);
    ASSERT_EQ(nullptr, resp.p_content_type_param);
    ASSERT_EQ(0, resp.content_len);
    ASSERT_EQ(HTTP_CONENT_ENCODING_NONE, resp.content_encoding);
    ASSERT_EQ(nullptr, resp.select_location.memory.p_buf);
    TEST_CHECK_LOG_RECORD_HTTP_SERVER(ESP_LOG_DEBUG, string("POST /ruuvi.json"));
    TEST_CHECK_LOG_RECORD_HTTP_SERVER(ESP_LOG_ERROR, string("Failed to allocate memory for gw_cfg"));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
}

TEST_F(TestHttpServerCb, http_server_cb_on_post_ruuvi_malloc_failed2) // NOLINT
{
    this->m_malloc_cnt            = 0;
    this->m_malloc_fail_on_cnt    = 2;
    const http_server_resp_t resp = http_server_cb_on_post_ruuvi(
        "{"
        "\"use_mqtt\":true,"
        "\"mqtt_server\":\"test.mosquitto.org\","
        "\"mqtt_port\":1883,"
        "\"mqtt_prefix\":\"ruuvi/30:AE:A4:02:84:A4\","
        "\"mqtt_client_id\":\"30:AE:A4:02:84:A4\","
        "\"mqtt_user\":\"\","
        "\"mqtt_pass\":\"\","
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
        "\"use_filtering\":true"
        "}");

    ASSERT_FALSE(this->m_flag_settings_saved_to_flash);
    ASSERT_FALSE(this->m_flag_settings_sent_to_nrf);
    ASSERT_FALSE(this->m_flag_settings_ethernet_ip_updated);

    ASSERT_EQ(HTTP_RESP_CODE_503, resp.http_resp_code);
    ASSERT_EQ(HTTP_CONTENT_LOCATION_NO_CONTENT, resp.content_location);
    ASSERT_TRUE(resp.flag_no_cache);
    ASSERT_EQ(HTTP_CONENT_TYPE_TEXT_HTML, resp.content_type);
    ASSERT_EQ(nullptr, resp.p_content_type_param);
    ASSERT_EQ(0, resp.content_len);
    ASSERT_EQ(HTTP_CONENT_ENCODING_NONE, resp.content_encoding);
    ASSERT_EQ(nullptr, resp.select_location.memory.p_buf);
    TEST_CHECK_LOG_RECORD_HTTP_SERVER(ESP_LOG_DEBUG, string("POST /ruuvi.json"));
    TEST_CHECK_LOG_RECORD_HTTP_SERVER(ESP_LOG_ERROR, string("Failed to parse json or no memory"));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
}

TEST_F(TestHttpServerCb, http_server_cb_on_post_ruuvi_malloc_failed3) // NOLINT
{
    this->m_malloc_cnt            = 0;
    this->m_malloc_fail_on_cnt    = 3;
    const http_server_resp_t resp = http_server_cb_on_post_ruuvi(
        "{"
        "\"use_mqtt\":true,"
        "\"mqtt_server\":\"test.mosquitto.org\","
        "\"mqtt_port\":1883,"
        "\"mqtt_prefix\":\"ruuvi/30:AE:A4:02:84:A4\","
        "\"mqtt_client_id\":\"30:AE:A4:02:84:A4\","
        "\"mqtt_user\":\"\","
        "\"mqtt_pass\":\"\","
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
        "\"use_filtering\":true"
        "}");

    ASSERT_FALSE(this->m_flag_settings_saved_to_flash);
    ASSERT_FALSE(this->m_flag_settings_sent_to_nrf);
    ASSERT_FALSE(this->m_flag_settings_ethernet_ip_updated);

    ASSERT_EQ(HTTP_RESP_CODE_503, resp.http_resp_code);
    ASSERT_EQ(HTTP_CONTENT_LOCATION_NO_CONTENT, resp.content_location);
    ASSERT_TRUE(resp.flag_no_cache);
    ASSERT_EQ(HTTP_CONENT_TYPE_TEXT_HTML, resp.content_type);
    ASSERT_EQ(nullptr, resp.p_content_type_param);
    ASSERT_EQ(0, resp.content_len);
    ASSERT_EQ(HTTP_CONENT_ENCODING_NONE, resp.content_encoding);
    ASSERT_EQ(nullptr, resp.select_location.memory.p_buf);
    TEST_CHECK_LOG_RECORD_HTTP_SERVER(ESP_LOG_DEBUG, string("POST /ruuvi.json"));
    TEST_CHECK_LOG_RECORD_HTTP_SERVER(ESP_LOG_ERROR, string("Failed to parse json or no memory"));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
}

TEST_F(TestHttpServerCb, http_server_cb_on_post_ruuvi_json_ok_save_prev_lan_auth) // NOLINT
{
    const char *expected_resp = "{}";
    {
        ruuvi_gateway_config_t *p_gw_cfg = gw_cfg_lock_rw();
        snprintf(
            p_gw_cfg->lan_auth.lan_auth_type,
            sizeof(p_gw_cfg->lan_auth.lan_auth_type),
            "%s",
            HTTP_SERVER_AUTH_TYPE_STR_RUUVI);
        snprintf(p_gw_cfg->lan_auth.lan_auth_user, sizeof(p_gw_cfg->lan_auth.lan_auth_user), "user1");
        snprintf(p_gw_cfg->lan_auth.lan_auth_pass, sizeof(p_gw_cfg->lan_auth.lan_auth_pass), "password1");
        gw_cfg_unlock_rw(&p_gw_cfg);
    }
    const http_server_resp_t resp = http_server_cb_on_post(
        "ruuvi.json",
        "{"
        "\"use_mqtt\":true,"
        "\"mqtt_transport\":\"TCP\","
        "\"mqtt_server\":\"test.mosquitto.org\","
        "\"mqtt_port\":1883,"
        "\"mqtt_prefix\":\"ruuvi/30:AE:A4:02:84:A4\","
        "\"mqtt_client_id\":\"30:AE:A4:02:84:A4\","
        "\"mqtt_user\":\"\","
        "\"mqtt_pass\":\"\","
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
        "\"lan_auth_api_key\":\"wH3F9SIiAA3rhG32aJki2Z7ekdFc0vtxuDhxl39zFvw=\","
        "\"use_filtering\":true"
        "}",
        false);

    ASSERT_TRUE(this->m_flag_settings_saved_to_flash);
    ASSERT_TRUE(this->m_flag_settings_sent_to_nrf);
    ASSERT_TRUE(this->m_flag_settings_ethernet_ip_updated);

    ASSERT_EQ(HTTP_RESP_CODE_200, resp.http_resp_code);
    ASSERT_EQ(HTTP_CONTENT_LOCATION_FLASH_MEM, resp.content_location);
    ASSERT_FALSE(resp.flag_no_cache);
    ASSERT_EQ(HTTP_CONENT_TYPE_APPLICATION_JSON, resp.content_type);
    ASSERT_EQ(nullptr, resp.p_content_type_param);
    ASSERT_EQ(strlen(expected_resp), resp.content_len);
    ASSERT_EQ(HTTP_CONENT_ENCODING_NONE, resp.content_encoding);
    ASSERT_NE(nullptr, resp.select_location.memory.p_buf);
    ASSERT_EQ(string(expected_resp), string(reinterpret_cast<const char *>(resp.select_location.memory.p_buf)));
    TEST_CHECK_LOG_RECORD_HTTP_SERVER(ESP_LOG_DEBUG, string("POST /ruuvi.json"));
    TEST_CHECK_LOG_RECORD_HTTP_SERVER(ESP_LOG_DEBUG, "Got SETTINGS:");
    TEST_CHECK_LOG_RECORD_HTTP_SERVER(ESP_LOG_DEBUG, "use_mqtt: 1");
    TEST_CHECK_LOG_RECORD_HTTP_SERVER(ESP_LOG_DEBUG, "mqtt_transport: TCP");
    TEST_CHECK_LOG_RECORD_HTTP_SERVER(ESP_LOG_DEBUG, "mqtt_server: test.mosquitto.org");
    TEST_CHECK_LOG_RECORD_HTTP_SERVER(ESP_LOG_DEBUG, "mqtt_prefix: ruuvi/30:AE:A4:02:84:A4");
    TEST_CHECK_LOG_RECORD_HTTP_SERVER(ESP_LOG_DEBUG, "mqtt_client_id: 30:AE:A4:02:84:A4");
    TEST_CHECK_LOG_RECORD_HTTP_SERVER(ESP_LOG_DEBUG, "mqtt_port: 1883");
    TEST_CHECK_LOG_RECORD_HTTP_SERVER(ESP_LOG_DEBUG, "mqtt_user: ");
    TEST_CHECK_LOG_RECORD_HTTP_SERVER(ESP_LOG_DEBUG, "mqtt_pass: ");
    TEST_CHECK_LOG_RECORD_HTTP_SERVER(ESP_LOG_DEBUG, "use_http: 0");
    TEST_CHECK_LOG_RECORD_HTTP_SERVER(ESP_LOG_DEBUG, "http_url: " RUUVI_GATEWAY_HTTP_DEFAULT_URL);
    TEST_CHECK_LOG_RECORD_HTTP_SERVER(ESP_LOG_DEBUG, "http_user: ");
    TEST_CHECK_LOG_RECORD_HTTP_SERVER(ESP_LOG_DEBUG, "http_pass: ");
    TEST_CHECK_LOG_RECORD_HTTP_SERVER(ESP_LOG_DEBUG, "use_http_stat: 1");
    TEST_CHECK_LOG_RECORD_HTTP_SERVER(ESP_LOG_DEBUG, "http_stat_url: " RUUVI_GATEWAY_HTTP_STATUS_URL);
    TEST_CHECK_LOG_RECORD_HTTP_SERVER(ESP_LOG_DEBUG, "http_stat_user: ");
    TEST_CHECK_LOG_RECORD_HTTP_SERVER(ESP_LOG_DEBUG, "http_stat_pass: ");
    TEST_CHECK_LOG_RECORD_HTTP_SERVER(ESP_LOG_ERROR, "lan_auth_type not found");
    TEST_CHECK_LOG_RECORD_HTTP_SERVER(ESP_LOG_INFO, "Use previous LAN auth settings");
    TEST_CHECK_LOG_RECORD_HTTP_SERVER(ESP_LOG_DEBUG, "lan_auth_api_key: wH3F9SIiAA3rhG32aJki2Z7ekdFc0vtxuDhxl39zFvw=");
    TEST_CHECK_LOG_RECORD_HTTP_SERVER(ESP_LOG_ERROR, "auto_update_cycle not found");
    TEST_CHECK_LOG_RECORD_HTTP_SERVER(ESP_LOG_ERROR, "auto_update_weekdays_bitmask not found or invalid");
    TEST_CHECK_LOG_RECORD_HTTP_SERVER(ESP_LOG_ERROR, "auto_update_interval_from not found or invalid");
    TEST_CHECK_LOG_RECORD_HTTP_SERVER(ESP_LOG_ERROR, "auto_update_interval_to not found or invalid");
    TEST_CHECK_LOG_RECORD_HTTP_SERVER(ESP_LOG_ERROR, "auto_update_tz_offset_hours not found or invalid");
    TEST_CHECK_LOG_RECORD_HTTP_SERVER(ESP_LOG_DEBUG, "use_filtering: 1");
    TEST_CHECK_LOG_RECORD_HTTP_SERVER(ESP_LOG_ERROR, "company_id not found or invalid");
    TEST_CHECK_LOG_RECORD_HTTP_SERVER(ESP_LOG_ERROR, "coordinates not found");
    TEST_CHECK_LOG_RECORD_HTTP_SERVER(ESP_LOG_ERROR, "use_coded_phy not found");
    TEST_CHECK_LOG_RECORD_HTTP_SERVER(ESP_LOG_ERROR, "use_1mbit_phy not found");
    TEST_CHECK_LOG_RECORD_HTTP_SERVER(ESP_LOG_ERROR, "use_extended_payload not found");
    TEST_CHECK_LOG_RECORD_HTTP_SERVER(ESP_LOG_ERROR, "use_channel_37 not found");
    TEST_CHECK_LOG_RECORD_HTTP_SERVER(ESP_LOG_ERROR, "use_channel_38 not found");
    TEST_CHECK_LOG_RECORD_HTTP_SERVER(ESP_LOG_ERROR, "use_channel_39 not found");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, string("Gateway SETTINGS:"));
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, string("config: use eth: 0"));
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, string("config: use eth dhcp: 1"));
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, string("config: eth static ip: "));
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, string("config: eth netmask: "));
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, string("config: eth gw: "));
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, string("config: eth dns1: "));
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, string("config: eth dns2: "));
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, string("config: use mqtt: 1"));
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, string("config: mqtt transport: TCP"));
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, string("config: mqtt server: test.mosquitto.org"));
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, string("config: mqtt port: 1883"));
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, string("config: mqtt use default prefix: 0"));
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, string("config: mqtt prefix: ruuvi/30:AE:A4:02:84:A4"));
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, string("config: mqtt client id: 30:AE:A4:02:84:A4"));
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, string("config: mqtt user: "));
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, string("config: mqtt password: ********"));
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, string("config: use http: 0"));
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, string("config: http url: " RUUVI_GATEWAY_HTTP_DEFAULT_URL));
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, string("config: http user: "));
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, string("config: http pass: ********"));
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, string("config: use http_stat: 1"));
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, string("config: http_stat url: " RUUVI_GATEWAY_HTTP_STATUS_URL));
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, string("config: http_stat user: "));
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, string("config: http_stat pass: ********"));
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, string("config: LAN auth type: lan_auth_ruuvi"));
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, string("config: LAN auth user: user1"));
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, string("config: LAN auth pass: ********"));
    TEST_CHECK_LOG_RECORD_GW_CFG(
        ESP_LOG_INFO,
        string("config: LAN auth API key: wH3F9SIiAA3rhG32aJki2Z7ekdFc0vtxuDhxl39zFvw="));
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, string("config: Auto update cycle: regular"));
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, string("config: Auto update weekdays_bitmask: 0x7f"));
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, string("config: Auto update interval: 00:00..24:00"));
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, string("config: Auto update TZ: UTC+3"));
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, string("config: coordinates: "));
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, string("config: use company id filter: 1"));
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, string("config: company id: 0x0499"));
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, string("config: use scan coded phy: 0"));
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, string("config: use scan 1mbit/phy: 1"));
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, string("config: use scan extended payload: 1"));
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, string("config: use scan channel 37: 1"));
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, string("config: use scan channel 38: 1"));
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, string("config: use scan channel 39: 1"));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
}

TEST_F(TestHttpServerCb, http_server_cb_on_post_ruuvi_json_ok_overwrite_lan_auth) // NOLINT
{
    const char *expected_resp = "{}";

    {
        ruuvi_gateway_config_t *p_gw_cfg = gw_cfg_lock_rw();
        snprintf(
            p_gw_cfg->lan_auth.lan_auth_type,
            sizeof(p_gw_cfg->lan_auth.lan_auth_type),
            "%s",
            HTTP_SERVER_AUTH_TYPE_STR_RUUVI);
        snprintf(p_gw_cfg->lan_auth.lan_auth_user, sizeof(p_gw_cfg->lan_auth.lan_auth_user), "user1");
        snprintf(p_gw_cfg->lan_auth.lan_auth_pass, sizeof(p_gw_cfg->lan_auth.lan_auth_pass), "password1");
        gw_cfg_unlock_rw(&p_gw_cfg);
    }

    const http_server_resp_t resp = http_server_cb_on_post(
        "ruuvi.json",
        "{"
        "\"use_mqtt\":true,"
        "\"mqtt_transport\":\"TCP\","
        "\"mqtt_server\":\"test.mosquitto.org\","
        "\"mqtt_port\":1883,"
        "\"mqtt_prefix\":\"ruuvi/30:AE:A4:02:84:A4\","
        "\"mqtt_client_id\":\"30:AE:A4:02:84:A4\","
        "\"mqtt_user\":\"\","
        "\"mqtt_pass\":\"\","
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
        "\"lan_auth_type\":\"lan_auth_digest\","
        "\"lan_auth_user\":\"user2\","
        "\"lan_auth_pass\":\"password2\","
        "\"lan_auth_api_key\":\"6kl/fd/c+3qvWm3Mhmwgh3BWNp+HDRQiLp/X0PuwG8Q=\","
        "\"use_filtering\":true"
        "}",
        false);

    ASSERT_TRUE(this->m_flag_settings_saved_to_flash);
    ASSERT_TRUE(this->m_flag_settings_sent_to_nrf);
    ASSERT_TRUE(this->m_flag_settings_ethernet_ip_updated);

    ASSERT_EQ(HTTP_RESP_CODE_200, resp.http_resp_code);
    ASSERT_EQ(HTTP_CONTENT_LOCATION_FLASH_MEM, resp.content_location);
    ASSERT_FALSE(resp.flag_no_cache);
    ASSERT_EQ(HTTP_CONENT_TYPE_APPLICATION_JSON, resp.content_type);
    ASSERT_EQ(nullptr, resp.p_content_type_param);
    ASSERT_EQ(strlen(expected_resp), resp.content_len);
    ASSERT_EQ(HTTP_CONENT_ENCODING_NONE, resp.content_encoding);
    ASSERT_NE(nullptr, resp.select_location.memory.p_buf);
    ASSERT_EQ(string(expected_resp), string(reinterpret_cast<const char *>(resp.select_location.memory.p_buf)));
    TEST_CHECK_LOG_RECORD_HTTP_SERVER(ESP_LOG_DEBUG, string("POST /ruuvi.json"));
    TEST_CHECK_LOG_RECORD_HTTP_SERVER(ESP_LOG_DEBUG, "Got SETTINGS:");
    TEST_CHECK_LOG_RECORD_HTTP_SERVER(ESP_LOG_DEBUG, "use_mqtt: 1");
    TEST_CHECK_LOG_RECORD_HTTP_SERVER(ESP_LOG_DEBUG, "mqtt_transport: TCP");
    TEST_CHECK_LOG_RECORD_HTTP_SERVER(ESP_LOG_DEBUG, "mqtt_server: test.mosquitto.org");
    TEST_CHECK_LOG_RECORD_HTTP_SERVER(ESP_LOG_DEBUG, "mqtt_prefix: ruuvi/30:AE:A4:02:84:A4");
    TEST_CHECK_LOG_RECORD_HTTP_SERVER(ESP_LOG_DEBUG, "mqtt_client_id: 30:AE:A4:02:84:A4");
    TEST_CHECK_LOG_RECORD_HTTP_SERVER(ESP_LOG_DEBUG, "mqtt_port: 1883");
    TEST_CHECK_LOG_RECORD_HTTP_SERVER(ESP_LOG_DEBUG, "mqtt_user: ");
    TEST_CHECK_LOG_RECORD_HTTP_SERVER(ESP_LOG_DEBUG, "mqtt_pass: ");
    TEST_CHECK_LOG_RECORD_HTTP_SERVER(ESP_LOG_DEBUG, "use_http: 0");
    TEST_CHECK_LOG_RECORD_HTTP_SERVER(ESP_LOG_DEBUG, "http_url: " RUUVI_GATEWAY_HTTP_DEFAULT_URL);
    TEST_CHECK_LOG_RECORD_HTTP_SERVER(ESP_LOG_DEBUG, "http_user: ");
    TEST_CHECK_LOG_RECORD_HTTP_SERVER(ESP_LOG_DEBUG, "http_pass: ");
    TEST_CHECK_LOG_RECORD_HTTP_SERVER(ESP_LOG_DEBUG, "use_http_stat: 1");
    TEST_CHECK_LOG_RECORD_HTTP_SERVER(ESP_LOG_DEBUG, "http_stat_url: " RUUVI_GATEWAY_HTTP_STATUS_URL);
    TEST_CHECK_LOG_RECORD_HTTP_SERVER(ESP_LOG_DEBUG, "http_stat_user: ");
    TEST_CHECK_LOG_RECORD_HTTP_SERVER(ESP_LOG_DEBUG, "http_stat_pass: ");
    TEST_CHECK_LOG_RECORD_HTTP_SERVER(ESP_LOG_DEBUG, "lan_auth_type: lan_auth_digest");
    TEST_CHECK_LOG_RECORD_HTTP_SERVER(ESP_LOG_DEBUG, "lan_auth_user: user2");
    TEST_CHECK_LOG_RECORD_HTTP_SERVER(ESP_LOG_DEBUG, "lan_auth_pass: password2");
    TEST_CHECK_LOG_RECORD_HTTP_SERVER(ESP_LOG_DEBUG, "lan_auth_api_key: 6kl/fd/c+3qvWm3Mhmwgh3BWNp+HDRQiLp/X0PuwG8Q=");
    TEST_CHECK_LOG_RECORD_HTTP_SERVER(ESP_LOG_ERROR, "auto_update_cycle not found");
    TEST_CHECK_LOG_RECORD_HTTP_SERVER(ESP_LOG_ERROR, "auto_update_weekdays_bitmask not found or invalid");
    TEST_CHECK_LOG_RECORD_HTTP_SERVER(ESP_LOG_ERROR, "auto_update_interval_from not found or invalid");
    TEST_CHECK_LOG_RECORD_HTTP_SERVER(ESP_LOG_ERROR, "auto_update_interval_to not found or invalid");
    TEST_CHECK_LOG_RECORD_HTTP_SERVER(ESP_LOG_ERROR, "auto_update_tz_offset_hours not found or invalid");
    TEST_CHECK_LOG_RECORD_HTTP_SERVER(ESP_LOG_DEBUG, "use_filtering: 1");
    TEST_CHECK_LOG_RECORD_HTTP_SERVER(ESP_LOG_ERROR, "company_id not found or invalid");
    TEST_CHECK_LOG_RECORD_HTTP_SERVER(ESP_LOG_ERROR, "coordinates not found");
    TEST_CHECK_LOG_RECORD_HTTP_SERVER(ESP_LOG_ERROR, "use_coded_phy not found");
    TEST_CHECK_LOG_RECORD_HTTP_SERVER(ESP_LOG_ERROR, "use_1mbit_phy not found");
    TEST_CHECK_LOG_RECORD_HTTP_SERVER(ESP_LOG_ERROR, "use_extended_payload not found");
    TEST_CHECK_LOG_RECORD_HTTP_SERVER(ESP_LOG_ERROR, "use_channel_37 not found");
    TEST_CHECK_LOG_RECORD_HTTP_SERVER(ESP_LOG_ERROR, "use_channel_38 not found");
    TEST_CHECK_LOG_RECORD_HTTP_SERVER(ESP_LOG_ERROR, "use_channel_39 not found");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, string("Gateway SETTINGS:"));
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, string("config: use eth: 0"));
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, string("config: use eth dhcp: 1"));
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, string("config: eth static ip: "));
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, string("config: eth netmask: "));
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, string("config: eth gw: "));
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, string("config: eth dns1: "));
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, string("config: eth dns2: "));
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, string("config: use mqtt: 1"));
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, string("config: mqtt transport: TCP"));
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, string("config: mqtt server: test.mosquitto.org"));
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, string("config: mqtt port: 1883"));
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, string("config: mqtt use default prefix: 0"));
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, string("config: mqtt prefix: ruuvi/30:AE:A4:02:84:A4"));
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, string("config: mqtt client id: 30:AE:A4:02:84:A4"));
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, string("config: mqtt user: "));
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, string("config: mqtt password: ********"));
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, string("config: use http: 0"));
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, string("config: http url: " RUUVI_GATEWAY_HTTP_DEFAULT_URL));
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, string("config: http user: "));
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, string("config: http pass: ********"));
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, string("config: use http_stat: 1"));
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, string("config: http_stat url: " RUUVI_GATEWAY_HTTP_STATUS_URL));
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, string("config: http_stat user: "));
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, string("config: http_stat pass: ********"));
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, string("config: LAN auth type: lan_auth_digest"));
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, string("config: LAN auth user: user2"));
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, string("config: LAN auth pass: ********"));
    TEST_CHECK_LOG_RECORD_GW_CFG(
        ESP_LOG_INFO,
        string("config: LAN auth API key: 6kl/fd/c+3qvWm3Mhmwgh3BWNp+HDRQiLp/X0PuwG8Q="));
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, string("config: Auto update cycle: regular"));
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, string("config: Auto update weekdays_bitmask: 0x7f"));
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, string("config: Auto update interval: 00:00..24:00"));
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, string("config: Auto update TZ: UTC+3"));
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, string("config: coordinates: "));
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, string("config: use company id filter: 1"));
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, string("config: company id: 0x0499"));
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, string("config: use scan coded phy: 0"));
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, string("config: use scan 1mbit/phy: 1"));
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, string("config: use scan extended payload: 1"));
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, string("config: use scan channel 37: 1"));
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, string("config: use scan channel 38: 1"));
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, string("config: use scan channel 39: 1"));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
}

TEST_F(TestHttpServerCb, http_server_cb_on_post_ruuvi_json_ok) // NOLINT
{
    const char *             expected_resp = "{}";
    const http_server_resp_t resp          = http_server_cb_on_post(
        "ruuvi.json",
        "{"
        "\"use_mqtt\":true,"
        "\"mqtt_transport\":\"TCP\","
        "\"mqtt_server\":\"test.mosquitto.org\","
        "\"mqtt_port\":1883,"
        "\"mqtt_prefix\":\"ruuvi/30:AE:A4:02:84:A4\","
        "\"mqtt_client_id\":\"30:AE:A4:02:84:A4\","
        "\"mqtt_user\":\"\","
        "\"mqtt_pass\":\"\","
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
        "\"use_filtering\":true"
        "}",
        false);

    ASSERT_TRUE(this->m_flag_settings_saved_to_flash);
    ASSERT_TRUE(this->m_flag_settings_sent_to_nrf);
    ASSERT_TRUE(this->m_flag_settings_ethernet_ip_updated);

    ASSERT_EQ(HTTP_RESP_CODE_200, resp.http_resp_code);
    ASSERT_EQ(HTTP_CONTENT_LOCATION_FLASH_MEM, resp.content_location);
    ASSERT_FALSE(resp.flag_no_cache);
    ASSERT_EQ(HTTP_CONENT_TYPE_APPLICATION_JSON, resp.content_type);
    ASSERT_EQ(nullptr, resp.p_content_type_param);
    ASSERT_EQ(strlen(expected_resp), resp.content_len);
    ASSERT_EQ(HTTP_CONENT_ENCODING_NONE, resp.content_encoding);
    ASSERT_NE(nullptr, resp.select_location.memory.p_buf);
    ASSERT_EQ(string(expected_resp), string(reinterpret_cast<const char *>(resp.select_location.memory.p_buf)));
    TEST_CHECK_LOG_RECORD_HTTP_SERVER(ESP_LOG_DEBUG, string("POST /ruuvi.json"));
    TEST_CHECK_LOG_RECORD_HTTP_SERVER(ESP_LOG_DEBUG, "Got SETTINGS:");
    TEST_CHECK_LOG_RECORD_HTTP_SERVER(ESP_LOG_DEBUG, "use_mqtt: 1");
    TEST_CHECK_LOG_RECORD_HTTP_SERVER(ESP_LOG_DEBUG, "mqtt_transport: TCP");
    TEST_CHECK_LOG_RECORD_HTTP_SERVER(ESP_LOG_DEBUG, "mqtt_server: test.mosquitto.org");
    TEST_CHECK_LOG_RECORD_HTTP_SERVER(ESP_LOG_DEBUG, "mqtt_prefix: ruuvi/30:AE:A4:02:84:A4");
    TEST_CHECK_LOG_RECORD_HTTP_SERVER(ESP_LOG_DEBUG, "mqtt_client_id: 30:AE:A4:02:84:A4");
    TEST_CHECK_LOG_RECORD_HTTP_SERVER(ESP_LOG_DEBUG, "mqtt_port: 1883");
    TEST_CHECK_LOG_RECORD_HTTP_SERVER(ESP_LOG_DEBUG, "mqtt_user: ");
    TEST_CHECK_LOG_RECORD_HTTP_SERVER(ESP_LOG_DEBUG, "mqtt_pass: ");
    TEST_CHECK_LOG_RECORD_HTTP_SERVER(ESP_LOG_DEBUG, "use_http: 0");
    TEST_CHECK_LOG_RECORD_HTTP_SERVER(ESP_LOG_DEBUG, "http_url: " RUUVI_GATEWAY_HTTP_DEFAULT_URL);
    TEST_CHECK_LOG_RECORD_HTTP_SERVER(ESP_LOG_DEBUG, "http_user: ");
    TEST_CHECK_LOG_RECORD_HTTP_SERVER(ESP_LOG_DEBUG, "http_pass: ");
    TEST_CHECK_LOG_RECORD_HTTP_SERVER(ESP_LOG_DEBUG, "use_http_stat: 1");
    TEST_CHECK_LOG_RECORD_HTTP_SERVER(ESP_LOG_DEBUG, "http_stat_url: " RUUVI_GATEWAY_HTTP_STATUS_URL);
    TEST_CHECK_LOG_RECORD_HTTP_SERVER(ESP_LOG_DEBUG, "http_stat_user: ");
    TEST_CHECK_LOG_RECORD_HTTP_SERVER(ESP_LOG_DEBUG, "http_stat_pass: ");
    TEST_CHECK_LOG_RECORD_HTTP_SERVER(ESP_LOG_ERROR, "lan_auth_type not found");
    TEST_CHECK_LOG_RECORD_HTTP_SERVER(ESP_LOG_INFO, "Use previous LAN auth settings");
    TEST_CHECK_LOG_RECORD_HTTP_SERVER(ESP_LOG_ERROR, "lan_auth_api_key not found");
    TEST_CHECK_LOG_RECORD_HTTP_SERVER(ESP_LOG_ERROR, "auto_update_cycle not found");
    TEST_CHECK_LOG_RECORD_HTTP_SERVER(ESP_LOG_ERROR, "auto_update_weekdays_bitmask not found or invalid");
    TEST_CHECK_LOG_RECORD_HTTP_SERVER(ESP_LOG_ERROR, "auto_update_interval_from not found or invalid");
    TEST_CHECK_LOG_RECORD_HTTP_SERVER(ESP_LOG_ERROR, "auto_update_interval_to not found or invalid");
    TEST_CHECK_LOG_RECORD_HTTP_SERVER(ESP_LOG_ERROR, "auto_update_tz_offset_hours not found or invalid");
    TEST_CHECK_LOG_RECORD_HTTP_SERVER(ESP_LOG_DEBUG, "use_filtering: 1");
    TEST_CHECK_LOG_RECORD_HTTP_SERVER(ESP_LOG_ERROR, "company_id not found or invalid");
    TEST_CHECK_LOG_RECORD_HTTP_SERVER(ESP_LOG_ERROR, "coordinates not found");
    TEST_CHECK_LOG_RECORD_HTTP_SERVER(ESP_LOG_ERROR, "use_coded_phy not found");
    TEST_CHECK_LOG_RECORD_HTTP_SERVER(ESP_LOG_ERROR, "use_1mbit_phy not found");
    TEST_CHECK_LOG_RECORD_HTTP_SERVER(ESP_LOG_ERROR, "use_extended_payload not found");
    TEST_CHECK_LOG_RECORD_HTTP_SERVER(ESP_LOG_ERROR, "use_channel_37 not found");
    TEST_CHECK_LOG_RECORD_HTTP_SERVER(ESP_LOG_ERROR, "use_channel_38 not found");
    TEST_CHECK_LOG_RECORD_HTTP_SERVER(ESP_LOG_ERROR, "use_channel_39 not found");
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, string("Gateway SETTINGS:"));
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, string("config: use eth: 0"));
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, string("config: use eth dhcp: 1"));
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, string("config: eth static ip: "));
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, string("config: eth netmask: "));
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, string("config: eth gw: "));
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, string("config: eth dns1: "));
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, string("config: eth dns2: "));
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, string("config: use mqtt: 1"));
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, string("config: mqtt transport: TCP"));
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, string("config: mqtt server: test.mosquitto.org"));
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, string("config: mqtt port: 1883"));
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, string("config: mqtt use default prefix: 0"));
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, string("config: mqtt prefix: ruuvi/30:AE:A4:02:84:A4"));
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, string("config: mqtt client id: 30:AE:A4:02:84:A4"));
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, string("config: mqtt user: "));
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, string("config: mqtt password: ********"));
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, string("config: use http: 0"));
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, string("config: http url: " RUUVI_GATEWAY_HTTP_DEFAULT_URL));
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, string("config: http user: "));
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, string("config: http pass: ********"));
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, string("config: use http_stat: 1"));
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, string("config: http_stat url: " RUUVI_GATEWAY_HTTP_STATUS_URL));
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, string("config: http_stat user: "));
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, string("config: http_stat pass: ********"));
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, string("config: LAN auth type: lan_auth_ruuvi"));
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, string("config: LAN auth user: Admin"));
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, string("config: LAN auth pass: ********"));
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, string("config: LAN auth API key: "));
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, string("config: Auto update cycle: regular"));
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, string("config: Auto update weekdays_bitmask: 0x7f"));
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, string("config: Auto update interval: 00:00..24:00"));
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, string("config: Auto update TZ: UTC+3"));
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, string("config: coordinates: "));
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, string("config: use company id filter: 1"));
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, string("config: company id: 0x0499"));
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, string("config: use scan coded phy: 0"));
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, string("config: use scan 1mbit/phy: 1"));
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, string("config: use scan extended payload: 1"));
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, string("config: use scan channel 37: 1"));
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, string("config: use scan channel 38: 1"));
    TEST_CHECK_LOG_RECORD_GW_CFG(ESP_LOG_INFO, string("config: use scan channel 39: 1"));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
}

TEST_F(TestHttpServerCb, http_server_cb_on_post_unknown_json) // NOLINT
{
    const http_server_resp_t resp = http_server_cb_on_post(
        "unknown.json",
        "{"
        "\"use_mqtt\":true,"
        "\"mqtt_server\":\"test.mosquitto.org\","
        "\"mqtt_port\":1883,"
        "\"mqtt_prefix\":\"ruuvi/30:AE:A4:02:84:A4\","
        "\"mqtt_client_id\":\"30:AE:A4:02:84:A4\","
        "\"mqtt_user\":\"\","
        "\"mqtt_pass\":\"\","
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
        "\"use_filtering\":true"
        "}",
        false);

    ASSERT_FALSE(this->m_flag_settings_saved_to_flash);
    ASSERT_FALSE(this->m_flag_settings_sent_to_nrf);
    ASSERT_FALSE(this->m_flag_settings_ethernet_ip_updated);

    ASSERT_EQ(HTTP_RESP_CODE_404, resp.http_resp_code);
    ASSERT_EQ(HTTP_CONTENT_LOCATION_NO_CONTENT, resp.content_location);
    ASSERT_TRUE(resp.flag_no_cache);
    ASSERT_EQ(HTTP_CONENT_TYPE_TEXT_HTML, resp.content_type);
    ASSERT_EQ(nullptr, resp.p_content_type_param);
    ASSERT_EQ(0, resp.content_len);
    ASSERT_EQ(HTTP_CONENT_ENCODING_NONE, resp.content_encoding);
    ASSERT_EQ(nullptr, resp.select_location.memory.p_buf);
    TEST_CHECK_LOG_RECORD_HTTP_SERVER(ESP_LOG_WARN, string("POST /unknown.json"));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
}

TEST_F(TestHttpServerCb, http_server_cb_on_delete) // NOLINT
{
    const http_server_resp_t resp = http_server_cb_on_delete("unknown.json", false, nullptr);

    ASSERT_EQ(HTTP_RESP_CODE_404, resp.http_resp_code);
    ASSERT_EQ(HTTP_CONTENT_LOCATION_NO_CONTENT, resp.content_location);
    ASSERT_TRUE(resp.flag_no_cache);
    ASSERT_EQ(HTTP_CONENT_TYPE_TEXT_HTML, resp.content_type);
    ASSERT_EQ(nullptr, resp.p_content_type_param);
    ASSERT_EQ(0, resp.content_len);
    ASSERT_EQ(HTTP_CONENT_ENCODING_NONE, resp.content_encoding);
    ASSERT_EQ(nullptr, resp.select_location.memory.p_buf);
    TEST_CHECK_LOG_RECORD_HTTP_SERVER(ESP_LOG_WARN, string("DELETE /unknown.json"));
    ASSERT_TRUE(esp_log_wrapper_is_empty());
}
