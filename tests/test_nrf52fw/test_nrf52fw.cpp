/**
 * @file test_bin2hex.cpp
 * @author TheSomeMan
 * @date 2020-08-27
 * @copyright Ruuvi Innovations Ltd, license BSD-3-Clause.
 */

#include "nrf52fw.h"
#include <string>
#include <sys/stat.h>
#include <ftw.h>
#include "gtest/gtest.h"
#include "esp_log_wrapper.hpp"
#include "esp_err.h"
#include "esp_vfs_fat.h"
#include "esp32/rom/crc.h"
#include "freertos/FreeRTOS.h"

using namespace std;

/*** Google-test class implementation
 * *********************************************************************************/

struct FlashFatFs_Tag
{
    int stub;
};

class TestNRF52Fw;
static TestNRF52Fw *g_pTestClass;

class NRF52Fw_VFS_FAT_MountInfo
{
public:
    string                     base_path;
    string                     partition_label;
    esp_vfs_fat_mount_config_t mount_config;
    bool                       flag_mounted;
    esp_err_t                  mount_err;
    esp_err_t                  unmount_err;
    wl_handle_t                wl_handle;
};

class MemSegment
{
public:
    uint32_t         segmentAddr;
    vector<uint32_t> data;

    MemSegment(const uint32_t addr, const uint32_t num_words, const uint32_t *p_buf)
        : segmentAddr(addr)
    {
        for (uint32_t i = 0; i < num_words; ++i)
        {
            this->data.push_back(p_buf[i]);
        }
    }

    void
    append(const uint32_t addr, const uint32_t num_words, const uint32_t *p_buf)
    {
        const uint32_t segmentEndAddr = this->segmentAddr + this->data.size() * sizeof(uint32_t);
        assert(addr == segmentEndAddr);
        for (uint32_t i = 0; i < num_words; ++i)
        {
            this->data.push_back(p_buf[i]);
        }
    }
};

static int
remove_file(const char *filename, const struct stat *status, int flag, struct FTW *p_info)
{
    return remove(filename);
}

static void
remove_dir_with_files(const char *path)
{
    struct stat st = { 0 };
    if (stat(path, &st) == 0)
    {
        if (S_ISDIR(st.st_mode))
        {
            const int res = nftw(path, remove_file, 10, FTW_DEPTH | FTW_MOUNT | FTW_PHYS);
            assert(0 == res);
        }
        else
        {
            const int res = remove(path);
            assert(0 == res);
        }
    }
}

class TestNRF52Fw : public ::testing::Test
{
private:
protected:
    FILE *        m_fd;
    FlashFatFs_t *m_p_ffs;
    const char *  m_mount_point_dir;
    const char *  m_mount_point;
    const char *  m_info_txt_name;

    void
    SetUp() override
    {
#define FS_NRF52_MOUNT_POINT "fs_nrf52"
        this->m_mount_point_dir = FS_NRF52_MOUNT_POINT;
        this->m_mount_point     = "/" FS_NRF52_MOUNT_POINT;
        this->m_info_txt_name   = "info.txt";
        {
            remove_dir_with_files(this->m_mount_point_dir);
            mkdir(this->m_mount_point_dir, 0700);
        }
        this->m_fd = nullptr;
        esp_log_wrapper_init();
        g_pTestClass = this;

        this->m_result_nrf52swd_init          = true;
        this->m_result_nrf52swd_check_id_code = true;
        this->m_result_nrf52swd_debug_halt    = true;
        this->m_result_nrf52swd_debug_run     = true;
        this->m_result_nrf52swd_erase_all     = true;
        this->m_cnt_nrf52swd_erase_all        = 0;
        this->m_mount_info.flag_mounted       = false;
        this->m_mount_info.mount_err          = ESP_OK;
        this->m_mount_info.unmount_err        = ESP_OK;
        this->m_mount_info.wl_handle          = 0;
    }

    void
    TearDown() override
    {
        g_pTestClass = nullptr;
        this->m_memSegmentsRead.clear();
        this->m_memSegmentsWrite.clear();
        nrf52fw_simulate_file_read_error(false);
        if (nullptr != m_fd)
        {
            fclose(m_fd);
            m_fd = nullptr;
        }
        if (nullptr != m_p_ffs)
        {
            flashfatfs_unmount(m_p_ffs);
            m_p_ffs = nullptr;
        }
        {
            remove_dir_with_files(this->m_mount_point_dir);
        }
        esp_log_wrapper_deinit();
    }

    FILE *
    open_file(const char *file_name, const char *mode)
    {
        char full_path_info_txt[40];
        snprintf(full_path_info_txt, sizeof(full_path_info_txt), "%s/%s", this->m_mount_point_dir, file_name);
        return fopen(full_path_info_txt, mode);
    }

public:
    TestNRF52Fw();

    ~TestNRF52Fw() override;

    bool                      m_result_nrf52swd_init;
    bool                      m_result_nrf52swd_check_id_code;
    bool                      m_result_nrf52swd_debug_halt;
    bool                      m_result_nrf52swd_debug_run;
    bool                      m_result_nrf52swd_erase_all;
    uint32_t                  m_cnt_nrf52swd_erase_all;
    NRF52Fw_VFS_FAT_MountInfo m_mount_info;
    vector<MemSegment>        m_memSegmentsWrite;
    vector<MemSegment>        m_memSegmentsRead;

    bool
    write_mem(const uint32_t addr, const uint32_t num_words, const uint32_t *p_buf)
    {
        for (auto &x : this->m_memSegmentsWrite)
        {
            const uint32_t segmentEndAddr = x.segmentAddr + x.data.size() * sizeof(uint32_t);
            const uint32_t endAddr        = addr + num_words * sizeof(uint32_t);
            if (addr == x.segmentAddr)
            {
                return false;
            }
            assert((addr < x.segmentAddr) || (addr >= segmentEndAddr));
            assert((endAddr < x.segmentAddr) || (endAddr >= segmentEndAddr));
            if (addr == segmentEndAddr)
            {
                x.append(addr, num_words, p_buf);
                return true;
            }
        }
        this->m_memSegmentsWrite.emplace_back(addr, num_words, p_buf);
        return true;
    }

    bool
    read_mem(const uint32_t addr, const uint32_t num_words, uint32_t *p_buf)
    {
        assert(0 == (addr % sizeof(uint32_t)));
        for (const auto &x : this->m_memSegmentsRead)
        {
            const uint32_t segmentEndAddr = x.segmentAddr + x.data.size() * sizeof(uint32_t);
            if ((addr >= x.segmentAddr) && (addr < segmentEndAddr))
            {
                const uint32_t offset = (addr - x.segmentAddr) / sizeof(uint32_t);
                for (uint32_t i = 0; i < num_words; ++i)
                {
                    p_buf[i] = x.data[offset + i];
                }
                return true;
            }
        }
        return false;
    }
};

TestNRF52Fw::TestNRF52Fw()
    : Test()
{
}

TestNRF52Fw::~TestNRF52Fw() = default;

extern "C" {

bool
nrf52swd_init(void)
{
    return g_pTestClass->m_result_nrf52swd_init;
}

void
nrf52swd_deinit(void)
{
}

bool
nrf52swd_reset(const bool flag_reset)
{
    return true;
}

bool
nrf52swd_check_id_code(void)
{
    return g_pTestClass->m_result_nrf52swd_check_id_code;
}

bool
nrf52swd_debug_halt(void)
{
    return g_pTestClass->m_result_nrf52swd_debug_halt;
}

bool
nrf52swd_debug_run(void)
{
    return g_pTestClass->m_result_nrf52swd_debug_run;
}

bool
nrf52swd_erase_all(void)
{
    g_pTestClass->m_cnt_nrf52swd_erase_all += 1;
    return g_pTestClass->m_result_nrf52swd_erase_all;
}

bool
nrf52swd_write_mem(const uint32_t addr, const uint32_t num_words, const uint32_t *p_buf)
{
    return g_pTestClass->write_mem(addr, num_words, p_buf);
}

bool
nrf52swd_read_mem(const uint32_t addr, const uint32_t num_words, uint32_t *p_buf)
{
    return g_pTestClass->read_mem(addr, num_words, p_buf);
}

esp_err_t
esp_vfs_fat_spiflash_mount(
    const char *                      base_path,
    const char *                      partition_label,
    const esp_vfs_fat_mount_config_t *mount_config,
    wl_handle_t *                     wl_handle)
{
    g_pTestClass->m_mount_info.base_path       = string(base_path);
    g_pTestClass->m_mount_info.partition_label = string(partition_label);
    g_pTestClass->m_mount_info.mount_config    = *mount_config;
    *wl_handle                                 = g_pTestClass->m_mount_info.wl_handle;
    g_pTestClass->m_mount_info.flag_mounted    = true;
    return g_pTestClass->m_mount_info.mount_err;
}

esp_err_t
esp_vfs_fat_spiflash_unmount(const char *base_path, wl_handle_t wl_handle)
{
    assert(g_pTestClass->m_mount_info.flag_mounted);
    assert(g_pTestClass->m_mount_info.wl_handle == wl_handle);
    g_pTestClass->m_mount_info.flag_mounted = false;
    return g_pTestClass->m_mount_info.unmount_err;
}

void
vTaskDelay(const TickType_t xTicksToDelay)
{
}

} // extern "C"

#define TEST_CHECK_LOG_RECORD_EX(tag_, level_, msg_) \
    do \
    { \
        ASSERT_FALSE(esp_log_wrapper_is_empty()); \
        const LogRecord log_record = esp_log_wrapper_pop(); \
        ASSERT_EQ(level_, log_record.level); \
        ASSERT_EQ(string(tag_), log_record.tag); \
        ASSERT_EQ(string(msg_), log_record.message); \
    } while (0)

#define TEST_CHECK_LOG_RECORD(level_, msg_) TEST_CHECK_LOG_RECORD_EX("nRF52Fw", level_, msg_);

#define TEST_CHECK_LOG_RECORD_FLASH_FAT_FS(level_, msg_) TEST_CHECK_LOG_RECORD_EX("FlashFatFS", level_, msg_);

/*** Unit-Tests
 * *******************************************************************************************************/

TEST_F(TestNRF52Fw, test_parse_version_digit_0) // NOLINT
{
    uint8_t val = 1;
    ASSERT_TRUE(nrf52fw_parse_version_digit("0", nullptr, &val));
    ASSERT_EQ(0, val);
}

TEST_F(TestNRF52Fw, test_parse_version_digit_1) // NOLINT
{
    uint8_t val = 0;
    ASSERT_TRUE(nrf52fw_parse_version_digit("1", nullptr, &val));
    ASSERT_EQ(1, val);
}

TEST_F(TestNRF52Fw, test_parse_version_digit_2) // NOLINT
{
    uint8_t val = 0;
    ASSERT_TRUE(nrf52fw_parse_version_digit("2", nullptr, &val));
    ASSERT_EQ(2, val);
}

TEST_F(TestNRF52Fw, test_parse_version_digit_255) // NOLINT
{
    uint8_t val = 0;
    ASSERT_TRUE(nrf52fw_parse_version_digit("255", nullptr, &val));
    ASSERT_EQ(255, val);
}

TEST_F(TestNRF52Fw, test_parse_version_digit_256) // NOLINT
{
    uint8_t val = 0;
    ASSERT_FALSE(nrf52fw_parse_version_digit("256", nullptr, &val));
    ASSERT_EQ(0, val);
}

TEST_F(TestNRF52Fw, test_parse_version_digit_minus_1) // NOLINT
{
    uint8_t val = 0;
    ASSERT_FALSE(nrf52fw_parse_version_digit("-1", nullptr, &val));
    ASSERT_EQ(0, val);
}

TEST_F(TestNRF52Fw, test_parse_version_digit_abc) // NOLINT
{
    uint8_t val = 0;
    ASSERT_FALSE(nrf52fw_parse_version_digit("abc", nullptr, &val));
    ASSERT_EQ(0, val);
}

TEST_F(TestNRF52Fw, test_parse_version_digit_1q) // NOLINT
{
    uint8_t val = 0;
    ASSERT_FALSE(nrf52fw_parse_version_digit("1q", nullptr, &val));
    ASSERT_EQ(0, val);
}

TEST_F(TestNRF52Fw, test_parse_version_digit_ext_123_dot) // NOLINT
{
    uint8_t     val         = 0;
    const char *version_str = "123.";
    const char *token_end   = strchr(version_str, '.');
    ASSERT_TRUE(nrf52fw_parse_version_digit(version_str, token_end, &val));
    ASSERT_EQ(123, val);
}

TEST_F(TestNRF52Fw, test_parse_version_digit_ext_12q_dot) // NOLINT
{
    uint8_t     val         = 0;
    const char *version_str = "12q.";
    const char *token_end   = strchr(version_str, '.');
    ASSERT_FALSE(nrf52fw_parse_version_digit(version_str, token_end, &val));
    ASSERT_EQ(0, val);
}

TEST_F(TestNRF52Fw, test_parse_version_ok_1_2_3) // NOLINT
{
    NRF52Fw_Version_t fw_ver = { 0 };
    ASSERT_TRUE(nrf52fw_parse_version("1.2.3", &fw_ver));
    ASSERT_EQ(0x01020300, fw_ver.version);
}

TEST_F(TestNRF52Fw, test_parse_version_ok_1_2_3_with_null_output) // NOLINT
{
    ASSERT_TRUE(nrf52fw_parse_version("1.2.3", nullptr));
}

TEST_F(TestNRF52Fw, test_parse_version_fail_1_2_) // NOLINT
{
    NRF52Fw_Version_t fw_ver = { 0 };
    ASSERT_FALSE(nrf52fw_parse_version("1.2.", &fw_ver));
    ASSERT_EQ(0, fw_ver.version);
}

TEST_F(TestNRF52Fw, test_parse_version_fail_1_2) // NOLINT
{
    NRF52Fw_Version_t fw_ver = { 0 };
    ASSERT_FALSE(nrf52fw_parse_version("1.2", &fw_ver));
    ASSERT_EQ(0, fw_ver.version);
}

TEST_F(TestNRF52Fw, test_parse_version_fail_1_) // NOLINT
{
    NRF52Fw_Version_t fw_ver = { 0 };
    ASSERT_FALSE(nrf52fw_parse_version("1.", &fw_ver));
    ASSERT_EQ(0, fw_ver.version);
}

TEST_F(TestNRF52Fw, test_parse_version_fail_1) // NOLINT
{
    NRF52Fw_Version_t fw_ver = { 0 };
    ASSERT_FALSE(nrf52fw_parse_version("1", &fw_ver));
    ASSERT_EQ(0, fw_ver.version);
}

TEST_F(TestNRF52Fw, test_parse_version_fail_empty) // NOLINT
{
    NRF52Fw_Version_t fw_ver = { 0 };
    ASSERT_FALSE(nrf52fw_parse_version("", &fw_ver));
    ASSERT_EQ(0, fw_ver.version);
}

TEST_F(TestNRF52Fw, test_parse_version_fail_null) // NOLINT
{
    NRF52Fw_Version_t fw_ver = { 0 };
    ASSERT_FALSE(nrf52fw_parse_version(nullptr, &fw_ver));
    ASSERT_EQ(0, fw_ver.version);
}

TEST_F(TestNRF52Fw, test_parse_version_line_ok_1_2_3) // NOLINT
{
    NRF52Fw_Version_t fw_ver = { 0 };
    ASSERT_TRUE(nrf52fw_parse_version_line("# v1.2.3", &fw_ver));
    ASSERT_EQ(0x01020300, fw_ver.version);
}

TEST_F(TestNRF52Fw, test_parse_version_line_fail_1) // NOLINT
{
    NRF52Fw_Version_t fw_ver = { 0 };
    ASSERT_FALSE(nrf52fw_parse_version_line("# 1.2.3", &fw_ver));
    ASSERT_EQ(0x00000000, fw_ver.version);
}

TEST_F(TestNRF52Fw, test_parse_version_line_fail_2) // NOLINT
{
    NRF52Fw_Version_t fw_ver = { 0 };
    ASSERT_FALSE(nrf52fw_parse_version_line("v1.2.3", &fw_ver));
    ASSERT_EQ(0x00000000, fw_ver.version);
}

TEST_F(TestNRF52Fw, test_nrf52fw_line_rstrip_empty) // NOLINT
{
    char buf[40];
    //    snprintf(buf, sizeof(buf), "");
    buf[0] = '\0';
    nrf52fw_line_rstrip(buf, sizeof(buf));
    ASSERT_EQ(string(""), string(buf));
}

TEST_F(TestNRF52Fw, test_nrf52fw_line_rstrip_abc) // NOLINT
{
    char buf[40];
    snprintf(buf, sizeof(buf), "abc");
    nrf52fw_line_rstrip(buf, sizeof(buf));
    ASSERT_EQ(string("abc"), string(buf));
}

TEST_F(TestNRF52Fw, test_nrf52fw_line_rstrip_abc_cr) // NOLINT
{
    char buf[40];
    snprintf(buf, sizeof(buf), "abc\r");
    nrf52fw_line_rstrip(buf, sizeof(buf));
    ASSERT_EQ(string("abc"), string(buf));
}

TEST_F(TestNRF52Fw, test_nrf52fw_line_rstrip_abc_lf) // NOLINT
{
    char buf[40];
    snprintf(buf, sizeof(buf), "abc\n");
    nrf52fw_line_rstrip(buf, sizeof(buf));
    ASSERT_EQ(string("abc"), string(buf));
}

TEST_F(TestNRF52Fw, test_nrf52fw_line_rstrip_abc_cr_lf) // NOLINT
{
    char buf[40];
    snprintf(buf, sizeof(buf), "abc\r\n");
    nrf52fw_line_rstrip(buf, sizeof(buf));
    ASSERT_EQ(string("abc"), string(buf));
}

TEST_F(TestNRF52Fw, test_nrf52fw_line_rstrip_abc_space_tab_cr_lf) // NOLINT
{
    char buf[40];
    snprintf(buf, sizeof(buf), "abc \t \r\n");
    nrf52fw_line_rstrip(buf, sizeof(buf));
    ASSERT_EQ(string("abc"), string(buf));
}

TEST_F(TestNRF52Fw, test_nrf52fw_parse_segment_info_line_empty) // NOLINT
{
    NRF52Fw_Segment_t segment = { 0 };
    ASSERT_FALSE(nrf52fw_parse_segment_info_line("", &segment));
}

TEST_F(TestNRF52Fw, test_nrf52fw_parse_segment_info_line_ok) // NOLINT
{
    NRF52Fw_Segment_t segment = { 0 };
    ASSERT_TRUE(nrf52fw_parse_segment_info_line("0x00001000 151016 segment_2.bin 0x0e326e66", &segment));
    ASSERT_EQ(0x00001000, segment.address);
    ASSERT_EQ(151016, segment.size);
    ASSERT_EQ(0x0e326e66, segment.crc);
    ASSERT_EQ(string("segment_2.bin"), string(segment.file_name));
}

TEST_F(TestNRF52Fw, test_nrf52fw_parse_segment_info_line_ok_alt) // NOLINT
{
    NRF52Fw_Segment_t segment = { 0 };
    ASSERT_TRUE(nrf52fw_parse_segment_info_line("00001000 \t 0x24DE8 \t segment_2.bin \t 0e326e66", &segment));
    ASSERT_EQ(0x00001000, segment.address);
    ASSERT_EQ(151016, segment.size);
    ASSERT_EQ(0x0e326e66, segment.crc);
    ASSERT_EQ(string("segment_2.bin"), string(segment.file_name));
}

TEST_F(TestNRF52Fw, test_nrf52fw_parse_segment_info_line_fail_1) // NOLINT
{
    NRF52Fw_Segment_t segment = { 0 };
    ASSERT_FALSE(nrf52fw_parse_segment_info_line("0x00001000 151016 segment_2.bin 0x0e326e6q", &segment));
}

TEST_F(TestNRF52Fw, test_nrf52fw_parse_segment_info_line_fail_2) // NOLINT
{
    NRF52Fw_Segment_t segment = { 0 };
    ASSERT_FALSE(nrf52fw_parse_segment_info_line("0x00001000 151016 segment_2.bin ", &segment));
}

TEST_F(TestNRF52Fw, test_nrf52fw_parse_segment_info_line_fail_3) // NOLINT
{
    NRF52Fw_Segment_t segment = { 0 };
    ASSERT_FALSE(nrf52fw_parse_segment_info_line("0x00001000 151016 segment_2.bin", &segment));
}

TEST_F(TestNRF52Fw, test_nrf52fw_parse_segment_info_line_fail_4) // NOLINT
{
    NRF52Fw_Segment_t segment = { 0 };
    ASSERT_FALSE(nrf52fw_parse_segment_info_line("0x00001000 151016 0x0e326e6", &segment));
}

TEST_F(TestNRF52Fw, test_nrf52fw_parse_segment_info_line_fail_5) // NOLINT
{
    NRF52Fw_Segment_t segment = { 0 };
    ASSERT_FALSE(nrf52fw_parse_segment_info_line("0x00001000 151016q segment_2.bin 0x0e326e6", &segment));
}

TEST_F(TestNRF52Fw, test_nrf52fw_parse_segment_info_line_fail_6) // NOLINT
{
    NRF52Fw_Segment_t segment = { 0 };
    ASSERT_FALSE(nrf52fw_parse_segment_info_line("0x00001000q 151016 segment_2.bin 0x0e326e6", &segment));
}

TEST_F(TestNRF52Fw, test_nrf52fw_parse_info_file_1) // NOLINT
{
    const char *p_path_info_txt = "info.txt";
    {
        this->m_fd = this->open_file(p_path_info_txt, "w");
        ASSERT_NE(nullptr, this->m_fd);
        fputs("# v1.4.2\n", this->m_fd);
        fputs("0x00000000 2816 segment_1.bin 0xc9924e37\n", this->m_fd);
        fputs("0x00001000 151016 segment_2.bin 0x0e326e66\n", this->m_fd);
        fputs("0x00026000 24448 segment_3.bin 0x9c7cffe7\n", this->m_fd);
        fclose(this->m_fd);
        this->m_fd = nullptr;
    }
    {
        this->m_fd = this->open_file(p_path_info_txt, "r");
        ASSERT_NE(nullptr, this->m_fd);

        NRF52Fw_Info_t info = { 0 };
        ASSERT_EQ(0, nrf52fw_parse_info_file(this->m_fd, &info));
        ASSERT_EQ(0x01040200, info.fw_ver.version);
        ASSERT_EQ(3, info.num_segments);

        {
            const NRF52Fw_Segment_t *p_segment = &info.segments[0];
            ASSERT_EQ(0x00000000, p_segment->address);
            ASSERT_EQ(2816, p_segment->size);
            ASSERT_EQ(string("segment_1.bin"), string(p_segment->file_name));
            ASSERT_EQ(0xc9924e37, p_segment->crc);
        }
        {
            const NRF52Fw_Segment_t *p_segment = &info.segments[1];
            ASSERT_EQ(0x00001000, p_segment->address);
            ASSERT_EQ(151016, p_segment->size);
            ASSERT_EQ(string("segment_2.bin"), string(p_segment->file_name));
            ASSERT_EQ(0x0e326e66, p_segment->crc);
        }
        {
            const NRF52Fw_Segment_t *p_segment = &info.segments[2];
            ASSERT_EQ(0x00026000, p_segment->address);
            ASSERT_EQ(24448, p_segment->size);
            ASSERT_EQ(string("segment_3.bin"), string(p_segment->file_name));
            ASSERT_EQ(0x9c7cffe7, p_segment->crc);
        }

        fclose(this->m_fd);
        this->m_fd = nullptr;
    }
}

TEST_F(TestNRF52Fw, test_nrf52fw_parse_info_file_no_version) // NOLINT
{
    const char *p_path_info_txt = "info.txt";
    {
        this->m_fd = this->open_file(p_path_info_txt, "w");
        ASSERT_NE(nullptr, this->m_fd);
        fputs("0x00000000 2816 segment_1.bin 0xc9924e37\n", this->m_fd);
        fputs("0x00001000 151016 segment_2.bin 0x0e326e66\n", this->m_fd);
        fputs("0x00026000 24448 segment_3.bin 0x9c7cffe7\n", this->m_fd);
        fclose(this->m_fd);
        this->m_fd = nullptr;
    }
    {
        this->m_fd = this->open_file(p_path_info_txt, "r");
        ASSERT_NE(nullptr, this->m_fd);

        NRF52Fw_Info_t info = { 0 };
        ASSERT_EQ(1, nrf52fw_parse_info_file(this->m_fd, &info));
        ASSERT_EQ(0, info.fw_ver.version);
        ASSERT_EQ(0, info.num_segments);

        fclose(this->m_fd);
        this->m_fd = nullptr;
    }
}

TEST_F(TestNRF52Fw, test_nrf52fw_parse_info_file_bad_line_2) // NOLINT
{
    const char *p_path_info_txt = "info.txt";
    {
        this->m_fd = this->open_file(p_path_info_txt, "w");
        ASSERT_NE(nullptr, this->m_fd);
        fputs("# v1.4.2\n", this->m_fd);
        fputs("0x00000000 2816zzzz segment_1.bin 0xc9924e37\n", this->m_fd);
        fputs("0x00001000 151016 segment_2.bin 0x0e326e66\n", this->m_fd);
        fputs("0x00026000 24448 segment_3.bin 0x9c7cffe7\n", this->m_fd);
        fclose(this->m_fd);
        this->m_fd = nullptr;
    }
    {
        this->m_fd = this->open_file(p_path_info_txt, "r");
        ASSERT_NE(nullptr, this->m_fd);

        NRF52Fw_Info_t info = { 0 };
        ASSERT_EQ(2, nrf52fw_parse_info_file(this->m_fd, &info));
        ASSERT_EQ(0x01040200, info.fw_ver.version);
        ASSERT_EQ(0, info.num_segments);

        fclose(this->m_fd);
        this->m_fd = nullptr;
    }
}

TEST_F(TestNRF52Fw, test_nrf52fw_parse_info_file_segments_overflow) // NOLINT
{
    const char *p_path_info_txt = "info.txt";
    {
        this->m_fd = this->open_file(p_path_info_txt, "w");
        ASSERT_NE(nullptr, this->m_fd);
        fputs("# v1.4.2\n", this->m_fd);
        fputs("0x00000000 2816 segment_1.bin 0xc9924e37\n", this->m_fd);
        fputs("0x00001000 151016 segment_2.bin 0x0e326e66\n", this->m_fd);
        fputs("0x00026000 24448 segment_3.bin 0x9c7cffe7\n", this->m_fd);
        fputs("0x00027000 24448 segment_4.bin 0x9c7cffe7\n", this->m_fd);
        fputs("0x00028000 24448 segment_5.bin 0x9c7cffe7\n", this->m_fd);
        fputs("0x00029000 24448 segment_6.bin 0x9c7cffe7\n", this->m_fd);
        fclose(this->m_fd);
        this->m_fd = nullptr;
    }
    {
        this->m_fd = this->open_file(p_path_info_txt, "r");
        ASSERT_NE(nullptr, this->m_fd);

        NRF52Fw_Info_t info = { 0 };
        ASSERT_EQ(7, nrf52fw_parse_info_file(this->m_fd, &info));
        ASSERT_EQ(0x01040200, info.fw_ver.version);
        ASSERT_EQ(5, info.num_segments);

        fclose(this->m_fd);
        this->m_fd = nullptr;
    }
}

TEST_F(TestNRF52Fw, nrf52fw_read_info_txt) // NOLINT
{
    {
        this->m_fd = open_file(this->m_info_txt_name, "w");
        ASSERT_NE(nullptr, this->m_fd);
        fputs("# v1.4.2\n", this->m_fd);
        fputs("0x00000000 2816 segment_1.bin 0xc9924e37\n", this->m_fd);
        fputs("0x00001000 151016 segment_2.bin 0x0e326e66\n", this->m_fd);
        fputs("0x00026000 24448 segment_3.bin 0x9c7cffe7\n", this->m_fd);
        fclose(this->m_fd);
        this->m_fd = nullptr;
    }
    {
        this->m_p_ffs = flashfatfs_mount(this->m_mount_point, GW_NRF_PARTITION, 2);
        ASSERT_NE(nullptr, this->m_p_ffs);

        NRF52Fw_Info_t info = { 0 };
        ASSERT_TRUE(nrf52fw_read_info_txt(this->m_p_ffs, this->m_info_txt_name, &info));

        ASSERT_EQ(0x01040200, info.fw_ver.version);
        ASSERT_EQ(3, info.num_segments);

        {
            const NRF52Fw_Segment_t *p_segment = &info.segments[0];
            ASSERT_EQ(0x00000000, p_segment->address);
            ASSERT_EQ(2816, p_segment->size);
            ASSERT_EQ(string("segment_1.bin"), string(p_segment->file_name));
            ASSERT_EQ(0xc9924e37, p_segment->crc);
        }
        {
            const NRF52Fw_Segment_t *p_segment = &info.segments[1];
            ASSERT_EQ(0x00001000, p_segment->address);
            ASSERT_EQ(151016, p_segment->size);
            ASSERT_EQ(string("segment_2.bin"), string(p_segment->file_name));
            ASSERT_EQ(0x0e326e66, p_segment->crc);
        }
        {
            const NRF52Fw_Segment_t *p_segment = &info.segments[2];
            ASSERT_EQ(0x00026000, p_segment->address);
            ASSERT_EQ(24448, p_segment->size);
            ASSERT_EQ(string("segment_3.bin"), string(p_segment->file_name));
            ASSERT_EQ(0x9c7cffe7, p_segment->crc);
        }
        ASSERT_TRUE(flashfatfs_unmount(this->m_p_ffs));
        this->m_p_ffs = nullptr;
    }
}

TEST_F(TestNRF52Fw, nrf52fw_read_info_txt_no_file) // NOLINT
{
    NRF52Fw_Info_t info = { 0 };

    this->m_p_ffs = flashfatfs_mount(this->m_mount_point, GW_NRF_PARTITION, 2);
    ASSERT_NE(nullptr, this->m_p_ffs);

    ASSERT_FALSE(nrf52fw_read_info_txt(this->m_p_ffs, this->m_info_txt_name, &info));

    ASSERT_TRUE(flashfatfs_unmount(this->m_p_ffs));
    this->m_p_ffs = nullptr;

    ASSERT_EQ(0, info.fw_ver.version);
    ASSERT_EQ(0, info.num_segments);

    TEST_CHECK_LOG_RECORD_FLASH_FAT_FS(ESP_LOG_INFO, "Partition 'fatfs_nrf52' mounted successfully to /fs_nrf52");
    TEST_CHECK_LOG_RECORD_FLASH_FAT_FS(ESP_LOG_ERROR, "Can't open: ./fs_nrf52/info.txt");
    TEST_CHECK_LOG_RECORD(ESP_LOG_ERROR, "nrf52fw_read_info_txt: Can't open: info.txt");
    TEST_CHECK_LOG_RECORD_FLASH_FAT_FS(ESP_LOG_INFO, "Unmount ./fs_nrf52");
    ASSERT_TRUE(esp_log_wrapper_is_empty());
}

TEST_F(TestNRF52Fw, nrf52fw_read_info_txt_bad_line_3) // NOLINT
{
    {
        this->m_fd = open_file(this->m_info_txt_name, "w");
        ASSERT_NE(nullptr, this->m_fd);
        fputs("# v1.4.2\n", this->m_fd);
        fputs("0x00000000 2816 segment_1.bin 0xc9924e37\n", this->m_fd);
        fputs("0x00001000q 151016 segment_2.bin 0x0e326e66\n", this->m_fd);
        fputs("0x00026000 24448 segment_3.bin 0x9c7cffe7\n", this->m_fd);
        fclose(this->m_fd);
        this->m_fd = nullptr;
    }
    this->m_p_ffs = flashfatfs_mount(this->m_mount_point, GW_NRF_PARTITION, 2);
    ASSERT_NE(nullptr, this->m_p_ffs);
    {
        NRF52Fw_Info_t info = { 0 };
        ASSERT_FALSE(nrf52fw_read_info_txt(this->m_p_ffs, this->m_info_txt_name, &info));

        ASSERT_EQ(0x01040200, info.fw_ver.version);
        ASSERT_EQ(1, info.num_segments);
    }
    ASSERT_TRUE(flashfatfs_unmount(this->m_p_ffs));
    this->m_p_ffs = nullptr;
    TEST_CHECK_LOG_RECORD_FLASH_FAT_FS(ESP_LOG_INFO, "Partition 'fatfs_nrf52' mounted successfully to /fs_nrf52");
    TEST_CHECK_LOG_RECORD(ESP_LOG_ERROR, "nrf52fw_read_info_txt: Failed to parse 'info.txt' at line 3");
    TEST_CHECK_LOG_RECORD_FLASH_FAT_FS(ESP_LOG_INFO, "Unmount ./fs_nrf52");
    ASSERT_TRUE(esp_log_wrapper_is_empty());
}

TEST_F(TestNRF52Fw, nrf52fw_read_current_fw_version_ok) // NOLINT
{
    const uint32_t version = 0x01020300;
    this->m_memSegmentsRead.emplace_back(MemSegment(0x10001080, 1, &version));

    uint32_t fw_ver = 0;
    ASSERT_TRUE(nrf52fw_read_current_fw_version(&fw_ver));
    ASSERT_EQ(version, fw_ver);
}

TEST_F(TestNRF52Fw, nrf52fw_read_current_fw_version_fail) // NOLINT
{
    uint32_t fw_ver = 0;
    ASSERT_FALSE(nrf52fw_read_current_fw_version(&fw_ver));
}

TEST_F(TestNRF52Fw, nrf52fw_write_current_fw_version_ok) // NOLINT
{
    const uint32_t version = 0x01020300;

    ASSERT_TRUE(nrf52fw_write_current_fw_version(version));
    ASSERT_EQ(1, this->m_memSegmentsWrite.size());
    {
        ASSERT_EQ(0x10001080, this->m_memSegmentsWrite[0].segmentAddr); // UICR
        ASSERT_EQ(1, this->m_memSegmentsWrite[0].data.size());
        ASSERT_EQ(version, this->m_memSegmentsWrite[0].data[0]);
    }
}

TEST_F(TestNRF52Fw, nrf52fw_write_current_fw_version_fail) // NOLINT
{
    const uint32_t version = 0;
    this->m_memSegmentsWrite.emplace_back(MemSegment(0x10001080, 1, &version));

    ASSERT_FALSE(nrf52fw_write_current_fw_version(version));
}

// TEST_F(TestNRF52Fw, nrf52fw_nrf52fw_update_firmware_if_necessary) // NOLINT
//{
//    const char *p_path_info_txt = "info.txt";
//    {
//        this->m_fd = this->open_file(p_path_info_txt, "w");
//        ASSERT_NE(nullptr, this->m_fd);
//        fputs("# v1.4.2\n", this->m_fd);
//        fputs("0x00000000 2816 segment_1.bin 0xc9924e37\n", this->m_fd);
//        fputs("0x00001000 151016 segment_2.bin 0x0e326e66\n", this->m_fd);
//        fputs("0x00026000 24448 segment_3.bin 0x9c7cffe7\n", this->m_fd);
//        fclose(this->m_fd);
//        this->m_fd = nullptr;
//    }
//
//    ASSERT_TRUE(nrf52fw_update_firmware_if_necessary());
//}

TEST_F(TestNRF52Fw, nrf52fw_flash_write_segment_ok_16_words) // NOLINT
{
    const char *segment_path = "segment_1.bin";
    uint32_t    segment_buf[16];
    for (int i = 0; i < sizeof(segment_buf) / sizeof(segment_buf[0]); ++i)
    {
        segment_buf[i] = 0xAA000000 + i;
    }

    {
        this->m_fd = this->open_file(segment_path, "wb");
        ASSERT_NE(nullptr, this->m_fd);
        fwrite(segment_buf, sizeof(segment_buf[0]), sizeof(segment_buf) / sizeof(segment_buf[0]), this->m_fd);
        fclose(this->m_fd);
        this->m_fd = nullptr;
    }

    this->m_fd = this->open_file(segment_path, "rb");
    ASSERT_NE(nullptr, this->m_fd);

    NRF52Fw_TmpBuf_t tmp_buf = { 0 };

    const uint32_t segment_addr = 0x00001000;
    this->m_memSegmentsRead.emplace_back(
        MemSegment(segment_addr, sizeof(segment_buf) / sizeof(segment_buf[0]), segment_buf));

    ASSERT_TRUE(nrf52fw_flash_write_segment(fileno(this->m_fd), &tmp_buf, segment_addr, sizeof(segment_buf)));
    ASSERT_EQ(1, this->m_memSegmentsWrite.size());
    {
        ASSERT_EQ(this->m_memSegmentsRead[0].segmentAddr, this->m_memSegmentsWrite[0].segmentAddr);
        ASSERT_EQ(this->m_memSegmentsRead[0].data, this->m_memSegmentsWrite[0].data);
    }
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, "Writing 0x00001000...");
    ASSERT_TRUE(esp_log_wrapper_is_empty());
}

TEST_F(TestNRF52Fw, nrf52fw_flash_write_segment_ok_257_words) // NOLINT
{
    const char *segment_path = "segment_1.bin";
    uint32_t    segment_buf[257];
    for (int i = 0; i < sizeof(segment_buf) / sizeof(segment_buf[0]); ++i)
    {
        segment_buf[i] = 0xAA000000 + i;
    }

    {
        this->m_fd = this->open_file(segment_path, "wb");
        ASSERT_NE(nullptr, this->m_fd);
        fwrite(segment_buf, sizeof(segment_buf[0]), sizeof(segment_buf) / sizeof(segment_buf[0]), this->m_fd);
        fclose(this->m_fd);
        this->m_fd = nullptr;
    }

    this->m_fd = this->open_file(segment_path, "rb");
    ASSERT_NE(nullptr, this->m_fd);

    NRF52Fw_TmpBuf_t tmp_buf = { 0 };

    const uint32_t segment_addr = 0x00001000;
    this->m_memSegmentsRead.emplace_back(
        MemSegment(segment_addr, sizeof(segment_buf) / sizeof(segment_buf[0]), segment_buf));

    ASSERT_TRUE(nrf52fw_flash_write_segment(fileno(this->m_fd), &tmp_buf, segment_addr, sizeof(segment_buf)));
    ASSERT_EQ(1, this->m_memSegmentsWrite.size());
    {
        ASSERT_EQ(this->m_memSegmentsRead[0].segmentAddr, this->m_memSegmentsWrite[0].segmentAddr);
        ASSERT_EQ(this->m_memSegmentsRead[0].data, this->m_memSegmentsWrite[0].data);
    }
}

TEST_F(TestNRF52Fw, nrf52fw_flash_write_segment_error_on_verify) // NOLINT
{
    const char *segment_path = "segment_1.bin";
    uint32_t    segment_buf[16];
    for (int i = 0; i < sizeof(segment_buf) / sizeof(segment_buf[0]); ++i)
    {
        segment_buf[i] = 0xAA000000 + i;
    }

    {
        this->m_fd = this->open_file(segment_path, "wb");
        ASSERT_NE(nullptr, this->m_fd);
        fwrite(segment_buf, sizeof(segment_buf[0]), sizeof(segment_buf) / sizeof(segment_buf[0]), this->m_fd);
        fclose(this->m_fd);
        this->m_fd = nullptr;
    }

    this->m_fd = this->open_file(segment_path, "rb");
    ASSERT_NE(nullptr, this->m_fd);

    NRF52Fw_TmpBuf_t tmp_buf = { 0 };

    const uint32_t segment_addr = 0x00001000;
    segment_buf[1] += 1U;
    this->m_memSegmentsRead.emplace_back(
        MemSegment(segment_addr, sizeof(segment_buf) / sizeof(segment_buf[0]), segment_buf));
    segment_buf[1] -= 1U;

    ASSERT_FALSE(nrf52fw_flash_write_segment(fileno(this->m_fd), &tmp_buf, segment_addr, sizeof(segment_buf)));
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, "Writing 0x00001000...");
    TEST_CHECK_LOG_RECORD(ESP_LOG_ERROR, "nrf52fw_flash_write_segment: verify failed");
    ASSERT_TRUE(esp_log_wrapper_is_empty());
}

TEST_F(TestNRF52Fw, nrf52fw_flash_write_segment_error_on_read) // NOLINT
{
    const char *segment_path = "segment_1.bin";
    uint32_t    segment_buf[16];
    for (int i = 0; i < sizeof(segment_buf) / sizeof(segment_buf[0]); ++i)
    {
        segment_buf[i] = 0xAA000000 + i;
    }

    {
        this->m_fd = this->open_file(segment_path, "wb");
        ASSERT_NE(nullptr, this->m_fd);
        fwrite(segment_buf, sizeof(segment_buf[0]), sizeof(segment_buf) / sizeof(segment_buf[0]), this->m_fd);
        fclose(this->m_fd);
        this->m_fd = nullptr;
    }

    this->m_fd = this->open_file(segment_path, "rb");
    ASSERT_NE(nullptr, this->m_fd);

    NRF52Fw_TmpBuf_t tmp_buf = { 0 };

    const uint32_t segment_addr = 0x00001000;

    ASSERT_FALSE(nrf52fw_flash_write_segment(fileno(this->m_fd), &tmp_buf, segment_addr, sizeof(segment_buf)));
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, "Writing 0x00001000...");
    TEST_CHECK_LOG_RECORD(ESP_LOG_ERROR, "nrf52fw_flash_write_segment: nrf52swd_read_mem failed");
    ASSERT_TRUE(esp_log_wrapper_is_empty());
}

TEST_F(TestNRF52Fw, nrf52fw_flash_write_segment_error_on_write) // NOLINT
{
    const char *segment_path = "segment_1.bin";
    uint32_t    segment_buf[16];
    for (int i = 0; i < sizeof(segment_buf) / sizeof(segment_buf[0]); ++i)
    {
        segment_buf[i] = 0xAA000000 + i;
    }

    {
        this->m_fd = this->open_file(segment_path, "wb");
        ASSERT_NE(nullptr, this->m_fd);
        fwrite(segment_buf, sizeof(segment_buf[0]), sizeof(segment_buf) / sizeof(segment_buf[0]), this->m_fd);
        fclose(this->m_fd);
        this->m_fd = nullptr;
    }

    this->m_fd = this->open_file(segment_path, "rb");
    ASSERT_NE(nullptr, this->m_fd);

    NRF52Fw_TmpBuf_t tmp_buf = { 0 };

    const uint32_t segment_addr = 0x00001000;
    this->m_memSegmentsWrite.emplace_back(
        MemSegment(segment_addr, sizeof(segment_buf) / sizeof(segment_buf[0]), segment_buf));

    ASSERT_FALSE(nrf52fw_flash_write_segment(fileno(this->m_fd), &tmp_buf, segment_addr, sizeof(segment_buf)));
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, "Writing 0x00001000...");
    TEST_CHECK_LOG_RECORD(ESP_LOG_ERROR, "nrf52fw_flash_write_segment: nrf52swd_write_mem failed");
    ASSERT_TRUE(esp_log_wrapper_is_empty());
}

TEST_F(TestNRF52Fw, nrf52fw_flash_write_segment_fail_file_greater_than_expected) // NOLINT
{
    const char *segment_path = "segment_1.bin";
    uint32_t    segment_buf[16];
    for (int i = 0; i < sizeof(segment_buf) / sizeof(segment_buf[0]); ++i)
    {
        segment_buf[i] = 0xAA000000 + i;
    }

    {
        this->m_fd = this->open_file(segment_path, "wb");
        ASSERT_NE(nullptr, this->m_fd);
        fwrite(segment_buf, sizeof(segment_buf[0]), sizeof(segment_buf) / sizeof(segment_buf[0]), this->m_fd);
        fclose(this->m_fd);
        this->m_fd = nullptr;
    }

    this->m_fd = this->open_file(segment_path, "rb");
    ASSERT_NE(nullptr, this->m_fd);

    NRF52Fw_TmpBuf_t tmp_buf = { 0 };

    const uint32_t segment_addr = 0x00001000;
    this->m_memSegmentsRead.emplace_back(
        MemSegment(segment_addr, sizeof(segment_buf) / sizeof(segment_buf[0]) - 1, segment_buf));

    ASSERT_FALSE(nrf52fw_flash_write_segment(
        fileno(this->m_fd),
        &tmp_buf,
        segment_addr,
        sizeof(segment_buf) - sizeof(uint32_t)));
    TEST_CHECK_LOG_RECORD(ESP_LOG_ERROR, "nrf52fw_flash_write_segment: offset 64 greater than segment len 60");
    ASSERT_TRUE(esp_log_wrapper_is_empty());
}

TEST_F(TestNRF52Fw, nrf52fw_flash_write_segment_fail_bad_length) // NOLINT
{
    const char *segment_path = "segment_1.bin";
    uint32_t    segment_buf[16];
    for (int i = 0; i < sizeof(segment_buf) / sizeof(segment_buf[0]); ++i)
    {
        segment_buf[i] = 0xAA000000 + i;
    }

    {
        this->m_fd = this->open_file(segment_path, "wb");
        ASSERT_NE(nullptr, this->m_fd);
        fwrite(segment_buf, 1, sizeof(segment_buf) - 1, this->m_fd);
        fclose(this->m_fd);
        this->m_fd = nullptr;
    }

    this->m_fd = this->open_file(segment_path, "rb");
    ASSERT_NE(nullptr, this->m_fd);

    NRF52Fw_TmpBuf_t tmp_buf = { 0 };

    const uint32_t segment_addr = 0x00001000;
    this->m_memSegmentsRead.emplace_back(
        MemSegment(segment_addr, sizeof(segment_buf) / sizeof(segment_buf[0]), segment_buf));

    ASSERT_FALSE(nrf52fw_flash_write_segment(fileno(this->m_fd), &tmp_buf, segment_addr, sizeof(segment_buf)));
    TEST_CHECK_LOG_RECORD(ESP_LOG_ERROR, "nrf52fw_flash_write_segment: bad len 63");
    ASSERT_TRUE(esp_log_wrapper_is_empty());
}

TEST_F(TestNRF52Fw, nrf52fw_flash_write_segment_fail_after_file_read_error) // NOLINT
{
    const char *segment_path = "segment_1.bin";
    uint32_t    segment_buf[16];
    for (int i = 0; i < sizeof(segment_buf) / sizeof(segment_buf[0]); ++i)
    {
        segment_buf[i] = 0xAA000000 + i;
    }

    {
        this->m_fd = this->open_file(segment_path, "wb");
        ASSERT_NE(nullptr, this->m_fd);
        fwrite(segment_buf, 1, sizeof(segment_buf) - 1, this->m_fd);
        fclose(this->m_fd);
        this->m_fd = nullptr;
    }

    this->m_fd = this->open_file(segment_path, "rb");
    ASSERT_NE(nullptr, this->m_fd);

    NRF52Fw_TmpBuf_t tmp_buf = { 0 };

    const uint32_t segment_addr = 0x00001000;
    this->m_memSegmentsRead.emplace_back(
        MemSegment(segment_addr, sizeof(segment_buf) / sizeof(segment_buf[0]), segment_buf));

    nrf52fw_simulate_file_read_error(true);
    ASSERT_FALSE(nrf52fw_flash_write_segment(fileno(this->m_fd), &tmp_buf, segment_addr, sizeof(segment_buf)));
    TEST_CHECK_LOG_RECORD(ESP_LOG_ERROR, "nrf52fw_flash_write_segment: nrf52fw_file_read failed");
    ASSERT_TRUE(esp_log_wrapper_is_empty());
}

TEST_F(TestNRF52Fw, nrf52fw_flash_write_segment_from_file_ok) // NOLINT
{
    const char *segment_path = "segment_1.bin";
    uint32_t    segment_buf[16];
    for (int i = 0; i < sizeof(segment_buf) / sizeof(segment_buf[0]); ++i)
    {
        segment_buf[i] = 0xAA000000 + i;
    }

    {
        this->m_fd = open_file(segment_path, "wb");
        ASSERT_NE(nullptr, this->m_fd);
        fwrite(segment_buf, sizeof(segment_buf[0]), sizeof(segment_buf) / sizeof(segment_buf[0]), this->m_fd);
        fclose(this->m_fd);
        this->m_fd = nullptr;
    }

    NRF52Fw_TmpBuf_t tmp_buf = { 0 };

    const uint32_t segment_addr = 0x00001000;
    this->m_memSegmentsRead.emplace_back(
        MemSegment(segment_addr, sizeof(segment_buf) / sizeof(segment_buf[0]), segment_buf));

    this->m_p_ffs = flashfatfs_mount(this->m_mount_point, GW_NRF_PARTITION, 2);
    ASSERT_NE(nullptr, this->m_p_ffs);

    ASSERT_TRUE(nrf52fw_flash_write_segment_from_file(
        this->m_p_ffs,
        segment_path,
        &tmp_buf,
        segment_addr,
        sizeof(segment_buf)));

    flashfatfs_unmount(this->m_p_ffs);
    this->m_p_ffs = nullptr;

    ASSERT_EQ(1, this->m_memSegmentsWrite.size());
    {
        ASSERT_EQ(this->m_memSegmentsRead[0].segmentAddr, this->m_memSegmentsWrite[0].segmentAddr);
        ASSERT_EQ(this->m_memSegmentsRead[0].data, this->m_memSegmentsWrite[0].data);
    }
    TEST_CHECK_LOG_RECORD_FLASH_FAT_FS(ESP_LOG_INFO, "Partition 'fatfs_nrf52' mounted successfully to /fs_nrf52");
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, "Writing 0x00001000...");
    TEST_CHECK_LOG_RECORD_FLASH_FAT_FS(ESP_LOG_INFO, "Unmount ./fs_nrf52");
    ASSERT_TRUE(esp_log_wrapper_is_empty());
}

TEST_F(TestNRF52Fw, nrf52fw_flash_write_segment_from_file_error_on_writing_segment) // NOLINT
{
    const char *segment_path = "segment_1.bin";
    uint32_t    segment_buf[16];
    for (int i = 0; i < sizeof(segment_buf) / sizeof(segment_buf[0]); ++i)
    {
        segment_buf[i] = 0xAA000000 + i;
    }

    {
        this->m_fd = open_file(segment_path, "wb");
        ASSERT_NE(nullptr, this->m_fd);
        fwrite(segment_buf, sizeof(segment_buf[0]), sizeof(segment_buf) / sizeof(segment_buf[0]), this->m_fd);
        fclose(this->m_fd);
        this->m_fd = nullptr;
    }

    NRF52Fw_TmpBuf_t tmp_buf = { 0 };

    const uint32_t segment_addr = 0x00001000;
    this->m_memSegmentsRead.emplace_back(
        MemSegment(segment_addr, sizeof(segment_buf) / sizeof(segment_buf[0]), segment_buf));
    this->m_memSegmentsWrite.emplace_back(
        MemSegment(segment_addr, sizeof(segment_buf) / sizeof(segment_buf[0]), segment_buf));

    this->m_p_ffs = flashfatfs_mount(this->m_mount_point, GW_NRF_PARTITION, 2);
    ASSERT_NE(nullptr, this->m_p_ffs);

    ASSERT_FALSE(nrf52fw_flash_write_segment_from_file(
        this->m_p_ffs,
        segment_path,
        &tmp_buf,
        segment_addr,
        sizeof(segment_buf)));

    flashfatfs_unmount(this->m_p_ffs);
    this->m_p_ffs = nullptr;

    TEST_CHECK_LOG_RECORD_FLASH_FAT_FS(ESP_LOG_INFO, "Partition 'fatfs_nrf52' mounted successfully to /fs_nrf52");
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, "Writing 0x00001000...");
    TEST_CHECK_LOG_RECORD(ESP_LOG_ERROR, "nrf52fw_flash_write_segment: nrf52swd_write_mem failed");
    TEST_CHECK_LOG_RECORD(
        ESP_LOG_ERROR,
        "nrf52fw_flash_write_segment_from_file: Failed to write segment 0x00001000 from 'segment_1.bin'");
    TEST_CHECK_LOG_RECORD_FLASH_FAT_FS(ESP_LOG_INFO, "Unmount ./fs_nrf52");
    ASSERT_TRUE(esp_log_wrapper_is_empty());
}

TEST_F(TestNRF52Fw, nrf52fw_flash_write_segment_from_file_error_no_file) // NOLINT
{
    const char *segment_path = "segment_1.bin";
    uint32_t    segment_buf[16];
    for (int i = 0; i < sizeof(segment_buf) / sizeof(segment_buf[0]); ++i)
    {
        segment_buf[i] = 0xAA000000 + i;
    }

    NRF52Fw_TmpBuf_t tmp_buf = { 0 };

    const uint32_t segment_addr = 0x00001000;
    this->m_memSegmentsRead.emplace_back(
        MemSegment(segment_addr, sizeof(segment_buf) / sizeof(segment_buf[0]), segment_buf));

    this->m_p_ffs = flashfatfs_mount(this->m_mount_point, GW_NRF_PARTITION, 2);
    ASSERT_NE(nullptr, this->m_p_ffs);

    ASSERT_FALSE(nrf52fw_flash_write_segment_from_file(
        this->m_p_ffs,
        segment_path,
        &tmp_buf,
        segment_addr,
        sizeof(segment_buf)));

    flashfatfs_unmount(this->m_p_ffs);
    this->m_p_ffs = nullptr;

    TEST_CHECK_LOG_RECORD_FLASH_FAT_FS(ESP_LOG_INFO, "Partition 'fatfs_nrf52' mounted successfully to /fs_nrf52");
    TEST_CHECK_LOG_RECORD_FLASH_FAT_FS(ESP_LOG_ERROR, "Can't open: ./fs_nrf52/segment_1.bin");
    TEST_CHECK_LOG_RECORD(ESP_LOG_ERROR, "nrf52fw_flash_write_segment_from_file: Can't open 'segment_1.bin'");
    TEST_CHECK_LOG_RECORD_FLASH_FAT_FS(ESP_LOG_INFO, "Unmount ./fs_nrf52");
    ASSERT_TRUE(esp_log_wrapper_is_empty());
}

TEST_F(TestNRF52Fw, nrf52fw_flash_write_firmware_ok) // NOLINT
{
    NRF52Fw_Info_t fw_info = { 0 };
    fw_info.fw_ver.version = 0x01020300;
    fw_info.num_segments   = 2;
    {
        NRF52Fw_Segment_t *p_segment = &fw_info.segments[0];
        snprintf(p_segment->file_name, sizeof(p_segment->file_name), "segment_1.bin");
        p_segment->address = 0x00000000;
        p_segment->size    = 516;
        p_segment->crc     = 0;
    }
    {
        NRF52Fw_Segment_t *p_segment = &fw_info.segments[1];
        snprintf(p_segment->file_name, sizeof(p_segment->file_name), "segment_2.bin");
        p_segment->address = 0x00001000;
        p_segment->size    = 1028;
        p_segment->crc     = 0;
    }

    {
        NRF52Fw_Segment_t *p_segment      = &fw_info.segments[0];
        uint32_t *         p_segment_buf1 = (uint32_t *)malloc(p_segment->size);
        ASSERT_NE(nullptr, p_segment_buf1);
        for (unsigned i = 0; i < p_segment->size / sizeof(uint32_t); ++i)
        {
            p_segment_buf1[i] = 0xAA000000 + i;
        }
        this->m_fd = this->open_file(p_segment->file_name, "wb");
        ASSERT_NE(nullptr, this->m_fd);
        fwrite(p_segment_buf1, 1, p_segment->size, this->m_fd);
        fclose(this->m_fd);
        this->m_fd = nullptr;
        this->m_memSegmentsRead.emplace_back(
            MemSegment(p_segment->address, p_segment->size / sizeof(uint32_t), p_segment_buf1));
        free(p_segment_buf1);
    }

    {
        NRF52Fw_Segment_t *p_segment      = &fw_info.segments[1];
        uint32_t *         p_segment_buf2 = (uint32_t *)malloc(p_segment->size);
        ASSERT_NE(nullptr, p_segment_buf2);
        for (unsigned i = 0; i < p_segment->size / sizeof(uint32_t); ++i)
        {
            p_segment_buf2[i] = 0xBB000000 + i;
        }
        this->m_fd = this->open_file(p_segment->file_name, "wb");
        ASSERT_NE(nullptr, this->m_fd);
        fwrite(p_segment_buf2, 1, p_segment->size, this->m_fd);
        fclose(this->m_fd);
        this->m_fd = nullptr;
        this->m_memSegmentsRead.emplace_back(
            MemSegment(p_segment->address, p_segment->size / sizeof(uint32_t), p_segment_buf2));
        free(p_segment_buf2);
    }

    NRF52Fw_TmpBuf_t tmp_buf = { 0 };

    this->m_p_ffs = flashfatfs_mount(this->m_mount_point, GW_NRF_PARTITION, 2);
    ASSERT_NE(nullptr, this->m_p_ffs);

    ASSERT_TRUE(nrf52fw_flash_write_firmware(this->m_p_ffs, &tmp_buf, &fw_info));

    flashfatfs_unmount(this->m_p_ffs);
    this->m_p_ffs = nullptr;

    ASSERT_EQ(3, this->m_memSegmentsWrite.size());
    {
        ASSERT_EQ(this->m_memSegmentsRead[0].segmentAddr, this->m_memSegmentsWrite[0].segmentAddr);
        ASSERT_EQ(this->m_memSegmentsRead[0].data, this->m_memSegmentsWrite[0].data);
    }
    {
        ASSERT_EQ(this->m_memSegmentsRead[1].segmentAddr, this->m_memSegmentsWrite[1].segmentAddr);
        ASSERT_EQ(this->m_memSegmentsRead[1].data, this->m_memSegmentsWrite[1].data);
    }
    {
        ASSERT_EQ(0x10001080, this->m_memSegmentsWrite[2].segmentAddr); // IUCR
        ASSERT_EQ(1, this->m_memSegmentsWrite[2].data.size());
        ASSERT_EQ(fw_info.fw_ver.version, this->m_memSegmentsWrite[2].data[0]);
    }
    TEST_CHECK_LOG_RECORD_FLASH_FAT_FS(ESP_LOG_INFO, "Partition 'fatfs_nrf52' mounted successfully to /fs_nrf52");
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, "Erasing flash memory...");
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, "Flash 2 segments");
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, "Flash segment 0: 0x00000000 size=516 from segment_1.bin");
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, "Writing 0x00000000...");
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, "Writing 0x00000200...");
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, "Flash segment 1: 0x00001000 size=1028 from segment_2.bin");
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, "Writing 0x00001000...");
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, "Writing 0x00001200...");
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, "Writing 0x00001400...");
    TEST_CHECK_LOG_RECORD_FLASH_FAT_FS(ESP_LOG_INFO, "Unmount ./fs_nrf52");
    ASSERT_TRUE(esp_log_wrapper_is_empty());
}

TEST_F(TestNRF52Fw, nrf52fw_flash_write_firmware_error_writing_version) // NOLINT
{
    NRF52Fw_Info_t fw_info = { 0 };
    fw_info.fw_ver.version = 0x01020300;
    fw_info.num_segments   = 2;
    {
        NRF52Fw_Segment_t *p_segment = &fw_info.segments[0];
        snprintf(p_segment->file_name, sizeof(p_segment->file_name), "segment_1.bin");
        p_segment->address = 0x00000000;
        p_segment->size    = 516;
        p_segment->crc     = 0;
    }
    {
        NRF52Fw_Segment_t *p_segment = &fw_info.segments[1];
        snprintf(p_segment->file_name, sizeof(p_segment->file_name), "segment_2.bin");
        p_segment->address = 0x00001000;
        p_segment->size    = 1028;
        p_segment->crc     = 0;
    }

    {
        NRF52Fw_Segment_t *p_segment      = &fw_info.segments[0];
        uint32_t *         p_segment_buf1 = (uint32_t *)malloc(p_segment->size);
        ASSERT_NE(nullptr, p_segment_buf1);
        for (unsigned i = 0; i < p_segment->size / sizeof(uint32_t); ++i)
        {
            p_segment_buf1[i] = 0xAA000000 + i;
        }
        this->m_fd = this->open_file(p_segment->file_name, "wb");
        ASSERT_NE(nullptr, this->m_fd);
        fwrite(p_segment_buf1, 1, p_segment->size, this->m_fd);
        fclose(this->m_fd);
        this->m_fd = nullptr;
        this->m_memSegmentsRead.emplace_back(
            MemSegment(p_segment->address, p_segment->size / sizeof(uint32_t), p_segment_buf1));
        free(p_segment_buf1);
    }

    {
        NRF52Fw_Segment_t *p_segment      = &fw_info.segments[1];
        uint32_t *         p_segment_buf2 = (uint32_t *)malloc(p_segment->size);
        ASSERT_NE(nullptr, p_segment_buf2);
        for (unsigned i = 0; i < p_segment->size / sizeof(uint32_t); ++i)
        {
            p_segment_buf2[i] = 0xBB000000 + i;
        }
        this->m_fd = this->open_file(p_segment->file_name, "wb");
        ASSERT_NE(nullptr, this->m_fd);
        fwrite(p_segment_buf2, 1, p_segment->size, this->m_fd);
        fclose(this->m_fd);
        this->m_fd = nullptr;
        this->m_memSegmentsRead.emplace_back(
            MemSegment(p_segment->address, p_segment->size / sizeof(uint32_t), p_segment_buf2));
        free(p_segment_buf2);
    }

    NRF52Fw_TmpBuf_t tmp_buf = { 0 };

    this->m_p_ffs = flashfatfs_mount(this->m_mount_point, GW_NRF_PARTITION, 2);
    ASSERT_NE(nullptr, this->m_p_ffs);

    {
        const uint32_t version = 0;
        this->m_memSegmentsWrite.emplace_back(MemSegment(0x10001080, 1, &version));
    }

    ASSERT_FALSE(nrf52fw_flash_write_firmware(this->m_p_ffs, &tmp_buf, &fw_info));

    flashfatfs_unmount(this->m_p_ffs);
    this->m_p_ffs = nullptr;

    ASSERT_EQ(3, this->m_memSegmentsWrite.size());
    {
        ASSERT_EQ(this->m_memSegmentsRead[0].segmentAddr, this->m_memSegmentsWrite[1].segmentAddr);
        ASSERT_EQ(this->m_memSegmentsRead[0].data, this->m_memSegmentsWrite[1].data);
    }
    {
        ASSERT_EQ(this->m_memSegmentsRead[1].segmentAddr, this->m_memSegmentsWrite[2].segmentAddr);
        ASSERT_EQ(this->m_memSegmentsRead[1].data, this->m_memSegmentsWrite[2].data);
    }
    TEST_CHECK_LOG_RECORD_FLASH_FAT_FS(ESP_LOG_INFO, "Partition 'fatfs_nrf52' mounted successfully to /fs_nrf52");
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, "Erasing flash memory...");
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, "Flash 2 segments");
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, "Flash segment 0: 0x00000000 size=516 from segment_1.bin");
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, "Writing 0x00000000...");
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, "Writing 0x00000200...");
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, "Flash segment 1: 0x00001000 size=1028 from segment_2.bin");
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, "Writing 0x00001000...");
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, "Writing 0x00001200...");
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, "Writing 0x00001400...");
    TEST_CHECK_LOG_RECORD(ESP_LOG_ERROR, "nrf52fw_flash_write_firmware: Failed to write firmware version");
    TEST_CHECK_LOG_RECORD_FLASH_FAT_FS(ESP_LOG_INFO, "Unmount ./fs_nrf52");
    ASSERT_TRUE(esp_log_wrapper_is_empty());
}

TEST_F(TestNRF52Fw, nrf52fw_flash_write_firmware_error_writing_segment) // NOLINT
{
    NRF52Fw_Info_t fw_info = { 0 };
    fw_info.fw_ver.version = 0x01020300;
    fw_info.num_segments   = 2;
    {
        NRF52Fw_Segment_t *p_segment = &fw_info.segments[0];
        snprintf(p_segment->file_name, sizeof(p_segment->file_name), "segment_1.bin");
        p_segment->address = 0x00000000;
        p_segment->size    = 516;
        p_segment->crc     = 0;
    }
    {
        NRF52Fw_Segment_t *p_segment = &fw_info.segments[1];
        snprintf(p_segment->file_name, sizeof(p_segment->file_name), "segment_2.bin");
        p_segment->address = 0x00001000;
        p_segment->size    = 1028;
        p_segment->crc     = 0;
    }

    {
        NRF52Fw_Segment_t *p_segment      = &fw_info.segments[0];
        uint32_t *         p_segment_buf1 = (uint32_t *)malloc(p_segment->size);
        ASSERT_NE(nullptr, p_segment_buf1);
        for (unsigned i = 0; i < p_segment->size / sizeof(uint32_t); ++i)
        {
            p_segment_buf1[i] = 0xAA000000 + i;
        }
        this->m_fd = this->open_file(p_segment->file_name, "wb");
        ASSERT_NE(nullptr, this->m_fd);
        fwrite(p_segment_buf1, 1, p_segment->size, this->m_fd);
        fclose(this->m_fd);
        this->m_fd = nullptr;
        this->m_memSegmentsRead.emplace_back(
            MemSegment(p_segment->address, p_segment->size / sizeof(uint32_t), p_segment_buf1));
        free(p_segment_buf1);
    }

    {
        NRF52Fw_Segment_t *p_segment      = &fw_info.segments[1];
        uint32_t *         p_segment_buf2 = (uint32_t *)malloc(p_segment->size);
        ASSERT_NE(nullptr, p_segment_buf2);
        for (unsigned i = 0; i < p_segment->size / sizeof(uint32_t); ++i)
        {
            p_segment_buf2[i] = 0xBB000000 + i;
        }
        this->m_fd = this->open_file(p_segment->file_name, "wb");
        ASSERT_NE(nullptr, this->m_fd);
        fwrite(p_segment_buf2, 1, p_segment->size, this->m_fd);
        fclose(this->m_fd);
        this->m_fd = nullptr;
        this->m_memSegmentsRead.emplace_back(
            MemSegment(p_segment->address, p_segment->size / sizeof(uint32_t), p_segment_buf2));
        free(p_segment_buf2);
    }

    NRF52Fw_TmpBuf_t tmp_buf = { 0 };

    this->m_p_ffs = flashfatfs_mount(this->m_mount_point, GW_NRF_PARTITION, 2);
    ASSERT_NE(nullptr, this->m_p_ffs);

    {
        const uint32_t stub = 0;
        this->m_memSegmentsWrite.emplace_back(MemSegment(fw_info.segments[1].address, 1, &stub));
    }

    ASSERT_FALSE(nrf52fw_flash_write_firmware(this->m_p_ffs, &tmp_buf, &fw_info));

    flashfatfs_unmount(this->m_p_ffs);
    this->m_p_ffs = nullptr;

    ASSERT_EQ(2, this->m_memSegmentsWrite.size());
    {
        ASSERT_EQ(this->m_memSegmentsRead[0].segmentAddr, this->m_memSegmentsWrite[1].segmentAddr);
        ASSERT_EQ(this->m_memSegmentsRead[0].data, this->m_memSegmentsWrite[1].data);
    }
    TEST_CHECK_LOG_RECORD_FLASH_FAT_FS(ESP_LOG_INFO, "Partition 'fatfs_nrf52' mounted successfully to /fs_nrf52");
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, "Erasing flash memory...");
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, "Flash 2 segments");
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, "Flash segment 0: 0x00000000 size=516 from segment_1.bin");
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, "Writing 0x00000000...");
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, "Writing 0x00000200...");
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, "Flash segment 1: 0x00001000 size=1028 from segment_2.bin");
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, "Writing 0x00001000...");
    TEST_CHECK_LOG_RECORD(ESP_LOG_ERROR, "nrf52fw_flash_write_segment: nrf52swd_write_mem failed");
    TEST_CHECK_LOG_RECORD(
        ESP_LOG_ERROR,
        "nrf52fw_flash_write_segment_from_file: Failed to write segment 0x00001000 from 'segment_2.bin'");
    TEST_CHECK_LOG_RECORD(
        ESP_LOG_ERROR,
        "nrf52fw_flash_write_firmware: Failed to write segment 1: 0x00001000 from segment_2.bin");
    TEST_CHECK_LOG_RECORD_FLASH_FAT_FS(ESP_LOG_INFO, "Unmount ./fs_nrf52");
    ASSERT_TRUE(esp_log_wrapper_is_empty());
}

TEST_F(TestNRF52Fw, nrf52fw_flash_write_firmware_error_erasing) // NOLINT
{
    NRF52Fw_Info_t fw_info = { 0 };
    fw_info.fw_ver.version = 0x01020300;
    fw_info.num_segments   = 2;
    {
        NRF52Fw_Segment_t *p_segment = &fw_info.segments[0];
        snprintf(p_segment->file_name, sizeof(p_segment->file_name), "segment_1.bin");
        p_segment->address = 0x00000000;
        p_segment->size    = 516;
        p_segment->crc     = 0;
    }
    {
        NRF52Fw_Segment_t *p_segment = &fw_info.segments[1];
        snprintf(p_segment->file_name, sizeof(p_segment->file_name), "segment_2.bin");
        p_segment->address = 0x00001000;
        p_segment->size    = 1028;
        p_segment->crc     = 0;
    }

    {
        NRF52Fw_Segment_t *p_segment      = &fw_info.segments[0];
        uint32_t *         p_segment_buf1 = (uint32_t *)malloc(p_segment->size);
        ASSERT_NE(nullptr, p_segment_buf1);
        for (unsigned i = 0; i < p_segment->size / sizeof(uint32_t); ++i)
        {
            p_segment_buf1[i] = 0xAA000000 + i;
        }
        this->m_fd = this->open_file(p_segment->file_name, "wb");
        ASSERT_NE(nullptr, this->m_fd);
        fwrite(p_segment_buf1, 1, p_segment->size, this->m_fd);
        fclose(this->m_fd);
        this->m_fd = nullptr;
        this->m_memSegmentsRead.emplace_back(
            MemSegment(p_segment->address, p_segment->size / sizeof(uint32_t), p_segment_buf1));
        free(p_segment_buf1);
    }

    {
        NRF52Fw_Segment_t *p_segment      = &fw_info.segments[1];
        uint32_t *         p_segment_buf2 = (uint32_t *)malloc(p_segment->size);
        ASSERT_NE(nullptr, p_segment_buf2);
        for (unsigned i = 0; i < p_segment->size / sizeof(uint32_t); ++i)
        {
            p_segment_buf2[i] = 0xBB000000 + i;
        }
        this->m_fd = this->open_file(p_segment->file_name, "wb");
        ASSERT_NE(nullptr, this->m_fd);
        fwrite(p_segment_buf2, 1, p_segment->size, this->m_fd);
        fclose(this->m_fd);
        this->m_fd = nullptr;
        this->m_memSegmentsRead.emplace_back(
            MemSegment(p_segment->address, p_segment->size / sizeof(uint32_t), p_segment_buf2));
        free(p_segment_buf2);
    }

    NRF52Fw_TmpBuf_t tmp_buf = { 0 };

    this->m_p_ffs = flashfatfs_mount(this->m_mount_point, GW_NRF_PARTITION, 2);
    ASSERT_NE(nullptr, this->m_p_ffs);

    this->m_result_nrf52swd_erase_all = false;

    ASSERT_FALSE(nrf52fw_flash_write_firmware(this->m_p_ffs, &tmp_buf, &fw_info));

    flashfatfs_unmount(this->m_p_ffs);
    this->m_p_ffs = nullptr;

    ASSERT_EQ(0, this->m_memSegmentsWrite.size());
    TEST_CHECK_LOG_RECORD_FLASH_FAT_FS(ESP_LOG_INFO, "Partition 'fatfs_nrf52' mounted successfully to /fs_nrf52");
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, "Erasing flash memory...");
    TEST_CHECK_LOG_RECORD(ESP_LOG_ERROR, "nrf52fw_erase_flash: nrf52swd_erase_all failed");
    TEST_CHECK_LOG_RECORD(ESP_LOG_ERROR, "nrf52fw_flash_write_firmware: nrf52fw_erase_flash failed");
    TEST_CHECK_LOG_RECORD_FLASH_FAT_FS(ESP_LOG_INFO, "Unmount ./fs_nrf52");
    ASSERT_TRUE(esp_log_wrapper_is_empty());
}

TEST_F(TestNRF52Fw, nrf52fw_calc_segment_crc_ok) // NOLINT
{
    const char *                segment_path = "segment_1.bin";
    const size_t                segment_size = 2816;
    std::unique_ptr<uint32_t[]> segment_buf(new uint32_t[segment_size / sizeof(uint32_t)]);
    for (int i = 0; i < segment_size / sizeof(uint32_t); ++i)
    {
        segment_buf[i] = 0xAA000000 + i;
    }

    {
        this->m_fd = this->open_file(segment_path, "wb");
        ASSERT_NE(nullptr, this->m_fd);
        fwrite(segment_buf.get(), 1, segment_size, this->m_fd);
        fclose(this->m_fd);
        this->m_fd = nullptr;
    }

    this->m_fd = this->open_file(segment_path, "rb");
    ASSERT_NE(nullptr, this->m_fd);

    NRF52Fw_TmpBuf_t tmp_buf = { 0 };

    uint32_t crc = 0;
    ASSERT_TRUE(nrf52fw_calc_segment_crc(fileno(this->m_fd), &tmp_buf, segment_size, &crc));
    ASSERT_EQ(0x7546C71EU, crc);
    ASSERT_TRUE(esp_log_wrapper_is_empty());
}

TEST_F(TestNRF52Fw, nrf52fw_calc_segment_crc_fail_file_greater_than_expected) // NOLINT
{
    const char *                segment_path = "segment_1.bin";
    const size_t                segment_size = 2816;
    std::unique_ptr<uint32_t[]> segment_buf(new uint32_t[segment_size / sizeof(uint32_t)]);
    for (int i = 0; i < segment_size / sizeof(uint32_t); ++i)
    {
        segment_buf[i] = 0xAA000000 + i;
    }

    {
        this->m_fd = this->open_file(segment_path, "wb");
        ASSERT_NE(nullptr, this->m_fd);
        fwrite(segment_buf.get(), 1, segment_size, this->m_fd);
        fclose(this->m_fd);
        this->m_fd = nullptr;
    }

    this->m_fd = this->open_file(segment_path, "rb");
    ASSERT_NE(nullptr, this->m_fd);

    NRF52Fw_TmpBuf_t tmp_buf = { 0 };

    uint32_t crc = 0;
    ASSERT_FALSE(nrf52fw_calc_segment_crc(fileno(this->m_fd), &tmp_buf, segment_size - sizeof(uint32_t), &crc));
    TEST_CHECK_LOG_RECORD(ESP_LOG_ERROR, "nrf52fw_calc_segment_crc: offset 2816 greater than segment len 2812");
    ASSERT_TRUE(esp_log_wrapper_is_empty());
}

TEST_F(TestNRF52Fw, nrf52fw_calc_segment_crc_fail_bad_length) // NOLINT
{
    const char *                segment_path = "segment_1.bin";
    const size_t                segment_size = 2816;
    std::unique_ptr<uint32_t[]> segment_buf(new uint32_t[segment_size / sizeof(uint32_t)]);
    for (int i = 0; i < segment_size / sizeof(uint32_t); ++i)
    {
        segment_buf[i] = 0xAA000000 + i;
    }

    {
        this->m_fd = this->open_file(segment_path, "wb");
        ASSERT_NE(nullptr, this->m_fd);
        fwrite(segment_buf.get(), 1, segment_size - 1, this->m_fd);
        fclose(this->m_fd);
        this->m_fd = nullptr;
    }

    this->m_fd = this->open_file(segment_path, "rb");
    ASSERT_NE(nullptr, this->m_fd);

    NRF52Fw_TmpBuf_t tmp_buf = { 0 };

    uint32_t crc = 0;
    ASSERT_FALSE(nrf52fw_calc_segment_crc(fileno(this->m_fd), &tmp_buf, segment_size, &crc));
    TEST_CHECK_LOG_RECORD(ESP_LOG_ERROR, "nrf52fw_calc_segment_crc: bad len 255");
    ASSERT_TRUE(esp_log_wrapper_is_empty());
}

TEST_F(TestNRF52Fw, nrf52fw_calc_segment_crc_fail_after_file_read_error) // NOLINT
{
    const char *                segment_path = "segment_1.bin";
    const size_t                segment_size = 2816;
    std::unique_ptr<uint32_t[]> segment_buf(new uint32_t[segment_size / sizeof(uint32_t)]);
    for (int i = 0; i < segment_size / sizeof(uint32_t); ++i)
    {
        segment_buf[i] = 0xAA000000 + i;
    }

    {
        this->m_fd = this->open_file(segment_path, "wb");
        ASSERT_NE(nullptr, this->m_fd);
        fwrite(segment_buf.get(), 1, segment_size, this->m_fd);
        fclose(this->m_fd);
        this->m_fd = nullptr;
    }

    this->m_fd = this->open_file(segment_path, "rb");
    ASSERT_NE(nullptr, this->m_fd);

    NRF52Fw_TmpBuf_t tmp_buf = { 0 };

    nrf52fw_simulate_file_read_error(true);
    uint32_t crc = 0;
    ASSERT_FALSE(nrf52fw_calc_segment_crc(fileno(this->m_fd), &tmp_buf, segment_size, &crc));
    TEST_CHECK_LOG_RECORD(ESP_LOG_ERROR, "nrf52fw_calc_segment_crc: nrf52fw_file_read failed");
    ASSERT_TRUE(esp_log_wrapper_is_empty());
}

TEST_F(TestNRF52Fw, nrf52fw_check_firmware_ok) // NOLINT
{
    const char *                segment1_path = "segment_1.bin";
    const size_t                segment1_size = 2816;
    std::unique_ptr<uint32_t[]> segment1_buf(new uint32_t[segment1_size / sizeof(uint32_t)]);
    for (int i = 0; i < segment1_size / sizeof(uint32_t); ++i)
    {
        segment1_buf[i] = 0xAA000000 + i;
    }
    const uint32_t segment1_crc = crc32_le(0, reinterpret_cast<const uint8_t *>(segment1_buf.get()), segment1_size);

    const char *                segment2_path = "segment_2.bin";
    const size_t                segment2_size = 3500;
    std::unique_ptr<uint32_t[]> segment2_buf(new uint32_t[segment2_size / sizeof(uint32_t)]);
    for (int i = 0; i < segment2_size / sizeof(uint32_t); ++i)
    {
        segment2_buf[i] = 0xAA000000 + i;
    }
    const uint32_t segment2_crc = crc32_le(0, reinterpret_cast<const uint8_t *>(segment2_buf.get()), segment2_size);

    NRF52Fw_Info_t fw_info = { 0 };
    fw_info.fw_ver.version = 0x03040500;
    fw_info.num_segments   = 2;
    {
        NRF52Fw_Segment_t *p_segment = &fw_info.segments[0];
        snprintf(p_segment->file_name, sizeof(p_segment->file_name), "%s", segment1_path);
        p_segment->address = 0x00000000;
        p_segment->size    = segment1_size;
        p_segment->crc     = segment1_crc;
    }
    {
        NRF52Fw_Segment_t *p_segment = &fw_info.segments[1];
        snprintf(p_segment->file_name, sizeof(p_segment->file_name), "%s", segment2_path);
        p_segment->address = 0x00001000;
        p_segment->size    = segment2_size;
        p_segment->crc     = segment2_crc;
    }

    {
        this->m_fd = this->open_file(segment1_path, "wb");
        ASSERT_NE(nullptr, this->m_fd);
        fwrite(segment1_buf.get(), 1, segment1_size, this->m_fd);
        fclose(this->m_fd);
        this->m_fd = nullptr;
    }
    {
        this->m_fd = this->open_file(segment2_path, "wb");
        ASSERT_NE(nullptr, this->m_fd);
        fwrite(segment2_buf.get(), 1, segment2_size, this->m_fd);
        fclose(this->m_fd);
        this->m_fd = nullptr;
    }

    NRF52Fw_TmpBuf_t tmp_buf = { 0 };

    this->m_p_ffs = flashfatfs_mount(this->m_mount_point, GW_NRF_PARTITION, 2);
    ASSERT_NE(nullptr, this->m_p_ffs);

    ASSERT_TRUE(nrf52fw_check_firmware(this->m_p_ffs, &tmp_buf, &fw_info));

    flashfatfs_unmount(this->m_p_ffs);
    this->m_p_ffs = nullptr;

    TEST_CHECK_LOG_RECORD_FLASH_FAT_FS(ESP_LOG_INFO, "Partition 'fatfs_nrf52' mounted successfully to /fs_nrf52");
    TEST_CHECK_LOG_RECORD_FLASH_FAT_FS(ESP_LOG_INFO, "Unmount ./fs_nrf52");
    ASSERT_TRUE(esp_log_wrapper_is_empty());
}

TEST_F(TestNRF52Fw, nrf52fw_check_firmware_error_bad_crc) // NOLINT
{
    const char *                segment1_path = "segment_1.bin";
    const size_t                segment1_size = 2816;
    std::unique_ptr<uint32_t[]> segment1_buf(new uint32_t[segment1_size / sizeof(uint32_t)]);
    for (int i = 0; i < segment1_size / sizeof(uint32_t); ++i)
    {
        segment1_buf[i] = 0xAA000000 + i;
    }
    const uint32_t segment1_crc = crc32_le(0, reinterpret_cast<const uint8_t *>(segment1_buf.get()), segment1_size);

    const char *                segment2_path = "segment_2.bin";
    const size_t                segment2_size = 3500;
    std::unique_ptr<uint32_t[]> segment2_buf(new uint32_t[segment2_size / sizeof(uint32_t)]);
    for (int i = 0; i < segment2_size / sizeof(uint32_t); ++i)
    {
        segment2_buf[i] = 0xAA000000 + i;
    }
    const uint32_t segment2_crc = crc32_le(0, reinterpret_cast<const uint8_t *>(segment2_buf.get()), segment2_size);

    NRF52Fw_Info_t fw_info = { 0 };
    fw_info.fw_ver.version = 0x03040500;
    fw_info.num_segments   = 2;
    {
        NRF52Fw_Segment_t *p_segment = &fw_info.segments[0];
        snprintf(p_segment->file_name, sizeof(p_segment->file_name), "%s", segment1_path);
        p_segment->address = 0x00000000;
        p_segment->size    = segment1_size;
        p_segment->crc     = segment1_crc;
    }
    {
        NRF52Fw_Segment_t *p_segment = &fw_info.segments[1];
        snprintf(p_segment->file_name, sizeof(p_segment->file_name), "%s", segment2_path);
        p_segment->address = 0x00001000;
        p_segment->size    = segment2_size;
        p_segment->crc     = segment2_crc + 1;
    }

    {
        this->m_fd = this->open_file(segment1_path, "wb");
        ASSERT_NE(nullptr, this->m_fd);
        fwrite(segment1_buf.get(), 1, segment1_size, this->m_fd);
        fclose(this->m_fd);
        this->m_fd = nullptr;
    }
    {
        this->m_fd = this->open_file(segment2_path, "wb");
        ASSERT_NE(nullptr, this->m_fd);
        fwrite(segment2_buf.get(), 1, segment2_size, this->m_fd);
        fclose(this->m_fd);
        this->m_fd = nullptr;
    }

    NRF52Fw_TmpBuf_t tmp_buf = { 0 };

    this->m_p_ffs = flashfatfs_mount(this->m_mount_point, GW_NRF_PARTITION, 2);
    ASSERT_NE(nullptr, this->m_p_ffs);

    ASSERT_FALSE(nrf52fw_check_firmware(this->m_p_ffs, &tmp_buf, &fw_info));

    flashfatfs_unmount(this->m_p_ffs);
    this->m_p_ffs = nullptr;

    TEST_CHECK_LOG_RECORD_FLASH_FAT_FS(ESP_LOG_INFO, "Partition 'fatfs_nrf52' mounted successfully to /fs_nrf52");
    TEST_CHECK_LOG_RECORD(ESP_LOG_ERROR, "nrf52fw_check_firmware: nrf52fw_calc_segment_crc failed");
    TEST_CHECK_LOG_RECORD(
        ESP_LOG_ERROR,
        "nrf52fw_check_firmware: Segment 1: 0x00001000: expected CRC: 0xa433c76e, actual CRC: 0xa433c76d");
    TEST_CHECK_LOG_RECORD_FLASH_FAT_FS(ESP_LOG_INFO, "Unmount ./fs_nrf52");
    ASSERT_TRUE(esp_log_wrapper_is_empty());
}

TEST_F(TestNRF52Fw, nrf52fw_check_firmware_fail_open_file) // NOLINT
{
    const char *                segment1_path = "segment_1.bin";
    const size_t                segment1_size = 2816;
    std::unique_ptr<uint32_t[]> segment1_buf(new uint32_t[segment1_size / sizeof(uint32_t)]);
    for (int i = 0; i < segment1_size / sizeof(uint32_t); ++i)
    {
        segment1_buf[i] = 0xAA000000 + i;
    }
    const uint32_t segment1_crc = crc32_le(0, reinterpret_cast<const uint8_t *>(segment1_buf.get()), segment1_size);

    const char *                segment2_path = "segment_2.bin";
    const size_t                segment2_size = 3500;
    std::unique_ptr<uint32_t[]> segment2_buf(new uint32_t[segment2_size / sizeof(uint32_t)]);
    for (int i = 0; i < segment2_size / sizeof(uint32_t); ++i)
    {
        segment2_buf[i] = 0xAA000000 + i;
    }
    const uint32_t segment2_crc = crc32_le(0, reinterpret_cast<const uint8_t *>(segment2_buf.get()), segment2_size);

    NRF52Fw_Info_t fw_info = { 0 };
    fw_info.fw_ver.version = 0x03040500;
    fw_info.num_segments   = 2;
    {
        NRF52Fw_Segment_t *p_segment = &fw_info.segments[0];
        snprintf(p_segment->file_name, sizeof(p_segment->file_name), "%s", segment1_path);
        p_segment->address = 0x00000000;
        p_segment->size    = segment1_size;
        p_segment->crc     = segment1_crc;
    }
    {
        NRF52Fw_Segment_t *p_segment = &fw_info.segments[1];
        snprintf(p_segment->file_name, sizeof(p_segment->file_name), "%s", segment2_path);
        p_segment->address = 0x00001000;
        p_segment->size    = segment2_size;
        p_segment->crc     = segment2_crc;
    }

    {
        this->m_fd = this->open_file(segment1_path, "wb");
        ASSERT_NE(nullptr, this->m_fd);
        fwrite(segment1_buf.get(), 1, segment1_size, this->m_fd);
        fclose(this->m_fd);
        this->m_fd = nullptr;
    }

    NRF52Fw_TmpBuf_t tmp_buf = { 0 };

    this->m_p_ffs = flashfatfs_mount(this->m_mount_point, GW_NRF_PARTITION, 2);
    ASSERT_NE(nullptr, this->m_p_ffs);

    ASSERT_FALSE(nrf52fw_check_firmware(this->m_p_ffs, &tmp_buf, &fw_info));

    flashfatfs_unmount(this->m_p_ffs);
    this->m_p_ffs = nullptr;

    TEST_CHECK_LOG_RECORD_FLASH_FAT_FS(ESP_LOG_INFO, "Partition 'fatfs_nrf52' mounted successfully to /fs_nrf52");
    TEST_CHECK_LOG_RECORD_FLASH_FAT_FS(ESP_LOG_ERROR, "Can't open: ./fs_nrf52/segment_2.bin");
    TEST_CHECK_LOG_RECORD(ESP_LOG_ERROR, "nrf52fw_check_firmware: Can't open 'segment_2.bin'");
    TEST_CHECK_LOG_RECORD_FLASH_FAT_FS(ESP_LOG_INFO, "Unmount ./fs_nrf52");
    ASSERT_TRUE(esp_log_wrapper_is_empty());
}

TEST_F(TestNRF52Fw, nrf52fw_check_firmware_fail_calc_segment_crc) // NOLINT
{
    const char *                segment1_path = "segment_1.bin";
    const size_t                segment1_size = 2816;
    std::unique_ptr<uint32_t[]> segment1_buf(new uint32_t[segment1_size / sizeof(uint32_t)]);
    for (int i = 0; i < segment1_size / sizeof(uint32_t); ++i)
    {
        segment1_buf[i] = 0xAA000000 + i;
    }
    const uint32_t segment1_crc = crc32_le(0, reinterpret_cast<const uint8_t *>(segment1_buf.get()), segment1_size);

    const char *                segment2_path = "segment_2.bin";
    const size_t                segment2_size = 3500;
    std::unique_ptr<uint32_t[]> segment2_buf(new uint32_t[segment2_size / sizeof(uint32_t)]);
    for (int i = 0; i < segment2_size / sizeof(uint32_t); ++i)
    {
        segment2_buf[i] = 0xAA000000 + i;
    }
    const uint32_t segment2_crc = crc32_le(0, reinterpret_cast<const uint8_t *>(segment2_buf.get()), segment2_size);

    NRF52Fw_Info_t fw_info = { 0 };
    fw_info.fw_ver.version = 0x03040500;
    fw_info.num_segments   = 2;
    {
        NRF52Fw_Segment_t *p_segment = &fw_info.segments[0];
        snprintf(p_segment->file_name, sizeof(p_segment->file_name), "%s", segment1_path);
        p_segment->address = 0x00000000;
        p_segment->size    = segment1_size;
        p_segment->crc     = segment1_crc;
    }
    {
        NRF52Fw_Segment_t *p_segment = &fw_info.segments[1];
        snprintf(p_segment->file_name, sizeof(p_segment->file_name), "%s", segment2_path);
        p_segment->address = 0x00001000;
        p_segment->size    = segment2_size;
        p_segment->crc     = segment2_crc;
    }

    {
        this->m_fd = this->open_file(segment1_path, "wb");
        ASSERT_NE(nullptr, this->m_fd);
        fwrite(segment1_buf.get(), 1, segment1_size, this->m_fd);
        fclose(this->m_fd);
        this->m_fd = nullptr;
    }
    {
        this->m_fd = this->open_file(segment2_path, "wb");
        ASSERT_NE(nullptr, this->m_fd);
        fwrite(segment2_buf.get(), 1, segment2_size, this->m_fd);
        fclose(this->m_fd);
        this->m_fd = nullptr;
    }

    NRF52Fw_TmpBuf_t tmp_buf = { 0 };

    this->m_p_ffs = flashfatfs_mount(this->m_mount_point, GW_NRF_PARTITION, 2);
    ASSERT_NE(nullptr, this->m_p_ffs);

    nrf52fw_simulate_file_read_error(true);
    ASSERT_FALSE(nrf52fw_check_firmware(this->m_p_ffs, &tmp_buf, &fw_info));

    flashfatfs_unmount(this->m_p_ffs);
    this->m_p_ffs = nullptr;

    TEST_CHECK_LOG_RECORD_FLASH_FAT_FS(ESP_LOG_INFO, "Partition 'fatfs_nrf52' mounted successfully to /fs_nrf52");
    TEST_CHECK_LOG_RECORD(ESP_LOG_ERROR, "nrf52fw_calc_segment_crc: nrf52fw_file_read failed");
    TEST_CHECK_LOG_RECORD(ESP_LOG_ERROR, "nrf52fw_check_firmware: nrf52fw_calc_segment_crc failed");
    TEST_CHECK_LOG_RECORD_FLASH_FAT_FS(ESP_LOG_INFO, "Unmount ./fs_nrf52");
    ASSERT_TRUE(esp_log_wrapper_is_empty());
}

TEST_F(TestNRF52Fw, nrf52fw_nrf52fw_update_firmware_if_necessary__update_not_needed) // NOLINT
{
    const char *segment1_path = "segment_1.bin";
    const char *segment2_path = "segment_2.bin";
    const char *segment3_path = "segment_3.bin";

    const size_t segment1_size = 2816;
    const size_t segment2_size = 151016;
    const size_t segment3_size = 24448;

    uint32_t segment1_crc = 0;
    uint32_t segment2_crc = 0;
    uint32_t segment3_crc = 0;

    {
        std::unique_ptr<uint32_t[]> segment1_buf(new uint32_t[segment1_size / sizeof(uint32_t)]);
        for (int i = 0; i < segment1_size / sizeof(uint32_t); ++i)
        {
            segment1_buf[i] = 0xAA000000 + i;
        }
        segment1_crc = crc32_le(0, reinterpret_cast<const uint8_t *>(segment1_buf.get()), segment1_size);
        {
            this->m_fd = this->open_file(segment1_path, "wb");
            ASSERT_NE(nullptr, this->m_fd);
            fwrite(segment1_buf.get(), 1, segment1_size, this->m_fd);
            fclose(this->m_fd);
            this->m_fd = nullptr;
        }
    }

    {
        std::unique_ptr<uint32_t[]> segment2_buf(new uint32_t[segment2_size / sizeof(uint32_t)]);
        for (int i = 0; i < segment2_size / sizeof(uint32_t); ++i)
        {
            segment2_buf[i] = 0xBB000000 + i;
        }
        segment2_crc = crc32_le(0, reinterpret_cast<const uint8_t *>(segment2_buf.get()), segment2_size);
        {
            this->m_fd = this->open_file(segment2_path, "wb");
            ASSERT_NE(nullptr, this->m_fd);
            fwrite(segment2_buf.get(), 1, segment2_size, this->m_fd);
            fclose(this->m_fd);
            this->m_fd = nullptr;
        }
    }

    {
        std::unique_ptr<uint32_t[]> segment3_buf(new uint32_t[segment3_size / sizeof(uint32_t)]);
        for (int i = 0; i < segment3_size / sizeof(uint32_t); ++i)
        {
            segment3_buf[i] = 0xCC000000 + i;
        }
        segment3_crc = crc32_le(0, reinterpret_cast<const uint8_t *>(segment3_buf.get()), segment3_size);
        {
            this->m_fd = this->open_file(segment3_path, "wb");
            ASSERT_NE(nullptr, this->m_fd);
            fwrite(segment3_buf.get(), 1, segment3_size, this->m_fd);
            fclose(this->m_fd);
            this->m_fd = nullptr;
        }
    }

    {
        this->m_fd = this->open_file("info.txt", "w");
        ASSERT_NE(nullptr, this->m_fd);
        fprintf(this->m_fd, "# v1.2.3\n");
        fprintf(this->m_fd, "0x00000000 %u %s 0x%08x\n", (unsigned)segment1_size, segment1_path, segment1_crc);
        fprintf(this->m_fd, "0x00001000 %u %s 0x%08x\n", (unsigned)segment2_size, segment2_path, segment2_crc);
        fprintf(this->m_fd, "0x00026000 %u %s 0x%08x\n", (unsigned)segment3_size, segment3_path, segment3_crc);
        fclose(this->m_fd);
        this->m_fd = nullptr;
    }

    {
        const uint32_t version = 0x01020300;
        this->m_memSegmentsRead.emplace_back(MemSegment(0x10001080, 1, &version));
    }

    ASSERT_TRUE(nrf52fw_update_firmware_if_necessary());

    ASSERT_EQ(0, this->m_memSegmentsWrite.size());
    ASSERT_EQ(0, this->m_cnt_nrf52swd_erase_all);

    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, "Hardware reset nRF52: true");
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, "Hardware reset nRF52: false");
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, "Init SWD");
    TEST_CHECK_LOG_RECORD_FLASH_FAT_FS(ESP_LOG_INFO, "Partition 'fatfs_nrf52' mounted successfully to /fs_nrf52");
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, "Firmware on FatFS: v1.2.3");
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, "Firmware on nRF52: v1.2.3");
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, "Firmware updating is not needed");
    TEST_CHECK_LOG_RECORD_FLASH_FAT_FS(ESP_LOG_INFO, "Unmount ./fs_nrf52");
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, "Deinit SWD");
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, "Hardware reset nRF52: true");
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, "Hardware reset nRF52: false");
    ASSERT_TRUE(esp_log_wrapper_is_empty());
}

TEST_F(TestNRF52Fw, nrf52fw_nrf52fw_update_firmware_if_necessary__update_required) // NOLINT
{
    const char *segment1_path = "segment_1.bin";
    const char *segment2_path = "segment_2.bin";
    const char *segment3_path = "segment_3.bin";

    const size_t segment1_size = 2816;
    const size_t segment2_size = 151016;
    const size_t segment3_size = 24448;

    uint32_t segment1_crc = 0;
    uint32_t segment2_crc = 0;
    uint32_t segment3_crc = 0;

    std::unique_ptr<uint32_t[]> segment1_buf(new uint32_t[segment1_size / sizeof(uint32_t)]);
    {
        for (int i = 0; i < segment1_size / sizeof(uint32_t); ++i)
        {
            segment1_buf[i] = 0xAA000000 + i;
        }
        segment1_crc = crc32_le(0, reinterpret_cast<const uint8_t *>(segment1_buf.get()), segment1_size);
        {
            this->m_fd = this->open_file(segment1_path, "wb");
            ASSERT_NE(nullptr, this->m_fd);
            fwrite(segment1_buf.get(), 1, segment1_size, this->m_fd);
            fclose(this->m_fd);
            this->m_fd = nullptr;
        }
    }

    std::unique_ptr<uint32_t[]> segment2_buf(new uint32_t[segment2_size / sizeof(uint32_t)]);
    {
        for (int i = 0; i < segment2_size / sizeof(uint32_t); ++i)
        {
            segment2_buf[i] = 0xBB000000 + i;
        }
        segment2_crc = crc32_le(0, reinterpret_cast<const uint8_t *>(segment2_buf.get()), segment2_size);
        {
            this->m_fd = this->open_file(segment2_path, "wb");
            ASSERT_NE(nullptr, this->m_fd);
            fwrite(segment2_buf.get(), 1, segment2_size, this->m_fd);
            fclose(this->m_fd);
            this->m_fd = nullptr;
        }
    }

    std::unique_ptr<uint32_t[]> segment3_buf(new uint32_t[segment3_size / sizeof(uint32_t)]);
    {
        for (int i = 0; i < segment3_size / sizeof(uint32_t); ++i)
        {
            segment3_buf[i] = 0xCC000000 + i;
        }
        segment3_crc = crc32_le(0, reinterpret_cast<const uint8_t *>(segment3_buf.get()), segment3_size);
        {
            this->m_fd = this->open_file(segment3_path, "wb");
            ASSERT_NE(nullptr, this->m_fd);
            fwrite(segment3_buf.get(), 1, segment3_size, this->m_fd);
            fclose(this->m_fd);
            this->m_fd = nullptr;
        }
    }

    {
        this->m_fd = this->open_file("info.txt", "w");
        ASSERT_NE(nullptr, this->m_fd);
        fprintf(this->m_fd, "# v1.2.3\n");
        fprintf(this->m_fd, "0x00000000 %u %s 0x%08x\n", (unsigned)segment1_size, segment1_path, segment1_crc);
        fprintf(this->m_fd, "0x00001000 %u %s 0x%08x\n", (unsigned)segment2_size, segment2_path, segment2_crc);
        fprintf(this->m_fd, "0x00026000 %u %s 0x%08x\n", (unsigned)segment3_size, segment3_path, segment3_crc);
        fclose(this->m_fd);
        this->m_fd = nullptr;
    }

    {
        const uint32_t version = 0x01020000;
        this->m_memSegmentsRead.emplace_back(MemSegment(0x10001080, 1, &version));
    }
    this->m_memSegmentsRead.emplace_back(MemSegment(0x00000000, segment1_size / sizeof(uint32_t), segment1_buf.get()));
    this->m_memSegmentsRead.emplace_back(MemSegment(0x00001000, segment2_size / sizeof(uint32_t), segment2_buf.get()));
    this->m_memSegmentsRead.emplace_back(MemSegment(0x00026000, segment3_size / sizeof(uint32_t), segment3_buf.get()));

    ASSERT_TRUE(nrf52fw_update_firmware_if_necessary());

    ASSERT_EQ(1, this->m_cnt_nrf52swd_erase_all);
    ASSERT_EQ(4, this->m_memSegmentsWrite.size());

    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, "Hardware reset nRF52: true");
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, "Hardware reset nRF52: false");
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, "Init SWD");
    TEST_CHECK_LOG_RECORD_FLASH_FAT_FS(ESP_LOG_INFO, "Partition 'fatfs_nrf52' mounted successfully to /fs_nrf52");
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, "Firmware on FatFS: v1.2.3");
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, "Firmware on nRF52: v1.2.0");
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, "Need to update firmware on nRF52");
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, "Erasing flash memory...");
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, "Flash 3 segments");
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, "Flash segment 0: 0x00000000 size=2816 from segment_1.bin");
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, "Writing 0x00000000...");
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, "Writing 0x00000200...");
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, "Writing 0x00000400...");
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, "Writing 0x00000600...");
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, "Writing 0x00000800...");
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, "Writing 0x00000a00...");
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, "Flash segment 1: 0x00001000 size=151016 from segment_2.bin");
    for (uint32_t offset = 0; offset < segment2_size; offset += 512)
    {
        char buf[80];
        snprintf(buf, sizeof(buf), "Writing 0x%08x...", (unsigned)(0x00001000U + offset));
        TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, buf);
    }
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, "Flash segment 2: 0x00026000 size=24448 from segment_3.bin");
    for (uint32_t offset = 0; offset < segment3_size; offset += 512)
    {
        char buf[80];
        snprintf(buf, sizeof(buf), "Writing 0x%08x...", (unsigned)(0x00026000U + offset));
        TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, buf);
    }
    TEST_CHECK_LOG_RECORD_FLASH_FAT_FS(ESP_LOG_INFO, "Unmount ./fs_nrf52");
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, "Deinit SWD");
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, "Hardware reset nRF52: true");
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, "Hardware reset nRF52: false");
    ASSERT_TRUE(esp_log_wrapper_is_empty());
}

TEST_F(TestNRF52Fw, nrf52fw_nrf52fw_update_firmware_if_necessary__error_init_swd) // NOLINT
{
    const char *segment1_path = "segment_1.bin";
    const char *segment2_path = "segment_2.bin";
    const char *segment3_path = "segment_3.bin";

    const size_t segment1_size = 2816;
    const size_t segment2_size = 151016;
    const size_t segment3_size = 24448;

    uint32_t segment1_crc = 0;
    uint32_t segment2_crc = 0;
    uint32_t segment3_crc = 0;

    {
        std::unique_ptr<uint32_t[]> segment1_buf(new uint32_t[segment1_size / sizeof(uint32_t)]);
        for (int i = 0; i < segment1_size / sizeof(uint32_t); ++i)
        {
            segment1_buf[i] = 0xAA000000 + i;
        }
        segment1_crc = crc32_le(0, reinterpret_cast<const uint8_t *>(segment1_buf.get()), segment1_size);
        {
            this->m_fd = this->open_file(segment1_path, "wb");
            ASSERT_NE(nullptr, this->m_fd);
            fwrite(segment1_buf.get(), 1, segment1_size, this->m_fd);
            fclose(this->m_fd);
            this->m_fd = nullptr;
        }
    }

    {
        std::unique_ptr<uint32_t[]> segment2_buf(new uint32_t[segment2_size / sizeof(uint32_t)]);
        for (int i = 0; i < segment2_size / sizeof(uint32_t); ++i)
        {
            segment2_buf[i] = 0xBB000000 + i;
        }
        segment2_crc = crc32_le(0, reinterpret_cast<const uint8_t *>(segment2_buf.get()), segment2_size);
        {
            this->m_fd = this->open_file(segment2_path, "wb");
            ASSERT_NE(nullptr, this->m_fd);
            fwrite(segment2_buf.get(), 1, segment2_size, this->m_fd);
            fclose(this->m_fd);
            this->m_fd = nullptr;
        }
    }

    {
        std::unique_ptr<uint32_t[]> segment3_buf(new uint32_t[segment3_size / sizeof(uint32_t)]);
        for (int i = 0; i < segment3_size / sizeof(uint32_t); ++i)
        {
            segment3_buf[i] = 0xCC000000 + i;
        }
        segment3_crc = crc32_le(0, reinterpret_cast<const uint8_t *>(segment3_buf.get()), segment3_size);
        {
            this->m_fd = this->open_file(segment3_path, "wb");
            ASSERT_NE(nullptr, this->m_fd);
            fwrite(segment3_buf.get(), 1, segment3_size, this->m_fd);
            fclose(this->m_fd);
            this->m_fd = nullptr;
        }
    }

    {
        this->m_fd = this->open_file("info.txt", "w");
        ASSERT_NE(nullptr, this->m_fd);
        fprintf(this->m_fd, "# v1.2.3\n");
        fprintf(this->m_fd, "0x00000000 %u %s 0x%08x\n", (unsigned)segment1_size, segment1_path, segment1_crc);
        fprintf(this->m_fd, "0x00001000 %u %s 0x%08x\n", (unsigned)segment2_size, segment2_path, segment2_crc);
        fprintf(this->m_fd, "0x00026000 %u %s 0x%08x\n", (unsigned)segment3_size, segment3_path, segment3_crc);
        fclose(this->m_fd);
        this->m_fd = nullptr;
    }

    {
        const uint32_t version = 0x01020300;
        this->m_memSegmentsRead.emplace_back(MemSegment(0x10001080, 1, &version));
    }

    this->m_result_nrf52swd_init = false;
    ASSERT_FALSE(nrf52fw_update_firmware_if_necessary());

    ASSERT_EQ(0, this->m_cnt_nrf52swd_erase_all);
    ASSERT_EQ(0, this->m_memSegmentsWrite.size());

    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, "Hardware reset nRF52: true");
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, "Hardware reset nRF52: false");
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, "Init SWD");
    TEST_CHECK_LOG_RECORD(ESP_LOG_ERROR, "nrf52swd_init failed");
    TEST_CHECK_LOG_RECORD(ESP_LOG_ERROR, "nrf52fw_update_firmware_if_necessary_step0: nrf52fw_init_swd failed");
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, "Hardware reset nRF52: true");
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, "Hardware reset nRF52: false");
    ASSERT_TRUE(esp_log_wrapper_is_empty());
}

TEST_F(TestNRF52Fw, nrf52fw_nrf52fw_update_firmware_if_necessary__debug_run_failed) // NOLINT
{
    const char *segment1_path = "segment_1.bin";
    const char *segment2_path = "segment_2.bin";
    const char *segment3_path = "segment_3.bin";

    const size_t segment1_size = 2816;
    const size_t segment2_size = 151016;
    const size_t segment3_size = 24448;

    uint32_t segment1_crc = 0;
    uint32_t segment2_crc = 0;
    uint32_t segment3_crc = 0;

    {
        std::unique_ptr<uint32_t[]> segment1_buf(new uint32_t[segment1_size / sizeof(uint32_t)]);
        for (int i = 0; i < segment1_size / sizeof(uint32_t); ++i)
        {
            segment1_buf[i] = 0xAA000000 + i;
        }
        segment1_crc = crc32_le(0, reinterpret_cast<const uint8_t *>(segment1_buf.get()), segment1_size);
        {
            this->m_fd = this->open_file(segment1_path, "wb");
            ASSERT_NE(nullptr, this->m_fd);
            fwrite(segment1_buf.get(), 1, segment1_size, this->m_fd);
            fclose(this->m_fd);
            this->m_fd = nullptr;
        }
    }

    {
        std::unique_ptr<uint32_t[]> segment2_buf(new uint32_t[segment2_size / sizeof(uint32_t)]);
        for (int i = 0; i < segment2_size / sizeof(uint32_t); ++i)
        {
            segment2_buf[i] = 0xBB000000 + i;
        }
        segment2_crc = crc32_le(0, reinterpret_cast<const uint8_t *>(segment2_buf.get()), segment2_size);
        {
            this->m_fd = this->open_file(segment2_path, "wb");
            ASSERT_NE(nullptr, this->m_fd);
            fwrite(segment2_buf.get(), 1, segment2_size, this->m_fd);
            fclose(this->m_fd);
            this->m_fd = nullptr;
        }
    }

    {
        std::unique_ptr<uint32_t[]> segment3_buf(new uint32_t[segment3_size / sizeof(uint32_t)]);
        for (int i = 0; i < segment3_size / sizeof(uint32_t); ++i)
        {
            segment3_buf[i] = 0xCC000000 + i;
        }
        segment3_crc = crc32_le(0, reinterpret_cast<const uint8_t *>(segment3_buf.get()), segment3_size);
        {
            this->m_fd = this->open_file(segment3_path, "wb");
            ASSERT_NE(nullptr, this->m_fd);
            fwrite(segment3_buf.get(), 1, segment3_size, this->m_fd);
            fclose(this->m_fd);
            this->m_fd = nullptr;
        }
    }

    {
        this->m_fd = this->open_file("info.txt", "w");
        ASSERT_NE(nullptr, this->m_fd);
        fprintf(this->m_fd, "# v1.2.3\n");
        fprintf(this->m_fd, "0x00000000 %u %s 0x%08x\n", (unsigned)segment1_size, segment1_path, segment1_crc);
        fprintf(this->m_fd, "0x00001000 %u %s 0x%08x\n", (unsigned)segment2_size, segment2_path, segment2_crc);
        fprintf(this->m_fd, "0x00026000 %u %s 0x%08x\n", (unsigned)segment3_size, segment3_path, segment3_crc);
        fclose(this->m_fd);
        this->m_fd = nullptr;
    }

    {
        const uint32_t version = 0x01020300;
        this->m_memSegmentsRead.emplace_back(MemSegment(0x10001080, 1, &version));
    }

    this->m_result_nrf52swd_debug_run = false;
    ASSERT_FALSE(nrf52fw_update_firmware_if_necessary());

    ASSERT_EQ(0, this->m_memSegmentsWrite.size());
    ASSERT_EQ(0, this->m_cnt_nrf52swd_erase_all);

    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, "Hardware reset nRF52: true");
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, "Hardware reset nRF52: false");
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, "Init SWD");
    TEST_CHECK_LOG_RECORD_FLASH_FAT_FS(ESP_LOG_INFO, "Partition 'fatfs_nrf52' mounted successfully to /fs_nrf52");
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, "Firmware on FatFS: v1.2.3");
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, "Firmware on nRF52: v1.2.3");
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, "Firmware updating is not needed");
    TEST_CHECK_LOG_RECORD_FLASH_FAT_FS(ESP_LOG_INFO, "Unmount ./fs_nrf52");
    TEST_CHECK_LOG_RECORD(ESP_LOG_ERROR, "nrf52fw_update_firmware_if_necessary_step0: nrf52swd_debug_run failed");
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, "Deinit SWD");
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, "Hardware reset nRF52: true");
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, "Hardware reset nRF52: false");
    ASSERT_TRUE(esp_log_wrapper_is_empty());
}

TEST_F(TestNRF52Fw, nrf52fw_nrf52fw_update_firmware_if_necessary__error_mount_failed) // NOLINT
{
    const char *segment1_path = "segment_1.bin";
    const char *segment2_path = "segment_2.bin";
    const char *segment3_path = "segment_3.bin";

    const size_t segment1_size = 2816;
    const size_t segment2_size = 151016;
    const size_t segment3_size = 24448;

    uint32_t segment1_crc = 0;
    uint32_t segment2_crc = 0;
    uint32_t segment3_crc = 0;

    {
        std::unique_ptr<uint32_t[]> segment1_buf(new uint32_t[segment1_size / sizeof(uint32_t)]);
        for (int i = 0; i < segment1_size / sizeof(uint32_t); ++i)
        {
            segment1_buf[i] = 0xAA000000 + i;
        }
        segment1_crc = crc32_le(0, reinterpret_cast<const uint8_t *>(segment1_buf.get()), segment1_size);
        {
            this->m_fd = this->open_file(segment1_path, "wb");
            ASSERT_NE(nullptr, this->m_fd);
            fwrite(segment1_buf.get(), 1, segment1_size, this->m_fd);
            fclose(this->m_fd);
            this->m_fd = nullptr;
        }
    }

    {
        std::unique_ptr<uint32_t[]> segment2_buf(new uint32_t[segment2_size / sizeof(uint32_t)]);
        for (int i = 0; i < segment2_size / sizeof(uint32_t); ++i)
        {
            segment2_buf[i] = 0xBB000000 + i;
        }
        segment2_crc = crc32_le(0, reinterpret_cast<const uint8_t *>(segment2_buf.get()), segment2_size);
        {
            this->m_fd = this->open_file(segment2_path, "wb");
            ASSERT_NE(nullptr, this->m_fd);
            fwrite(segment2_buf.get(), 1, segment2_size, this->m_fd);
            fclose(this->m_fd);
            this->m_fd = nullptr;
        }
    }

    {
        std::unique_ptr<uint32_t[]> segment3_buf(new uint32_t[segment3_size / sizeof(uint32_t)]);
        for (int i = 0; i < segment3_size / sizeof(uint32_t); ++i)
        {
            segment3_buf[i] = 0xCC000000 + i;
        }
        segment3_crc = crc32_le(0, reinterpret_cast<const uint8_t *>(segment3_buf.get()), segment3_size);
        {
            this->m_fd = this->open_file(segment3_path, "wb");
            ASSERT_NE(nullptr, this->m_fd);
            fwrite(segment3_buf.get(), 1, segment3_size, this->m_fd);
            fclose(this->m_fd);
            this->m_fd = nullptr;
        }
    }

    {
        this->m_fd = this->open_file("info.txt", "w");
        ASSERT_NE(nullptr, this->m_fd);
        fprintf(this->m_fd, "# v1.2.3\n");
        fprintf(this->m_fd, "0x00000000 %u %s 0x%08x\n", (unsigned)segment1_size, segment1_path, segment1_crc);
        fprintf(this->m_fd, "0x00001000 %u %s 0x%08x\n", (unsigned)segment2_size, segment2_path, segment2_crc);
        fprintf(this->m_fd, "0x00026000 %u %s 0x%08x\n", (unsigned)segment3_size, segment3_path, segment3_crc);
        fclose(this->m_fd);
        this->m_fd = nullptr;
    }

    {
        const uint32_t version = 0x01020300;
        this->m_memSegmentsRead.emplace_back(MemSegment(0x10001080, 1, &version));
    }

    this->m_mount_info.mount_err = ESP_ERR_NOT_FOUND;
    ASSERT_FALSE(nrf52fw_update_firmware_if_necessary());

    ASSERT_EQ(0, this->m_memSegmentsWrite.size());
    ASSERT_EQ(0, this->m_cnt_nrf52swd_erase_all);

    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, "Hardware reset nRF52: true");
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, "Hardware reset nRF52: false");
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, "Init SWD");
    TEST_CHECK_LOG_RECORD_FLASH_FAT_FS(ESP_LOG_ERROR, "flashfatfs_mount: esp_vfs_fat_spiflash_mount failed, err=261");
    TEST_CHECK_LOG_RECORD(ESP_LOG_ERROR, "nrf52fw_update_firmware_if_necessary_step1: flashfatfs_mount failed");
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, "Deinit SWD");
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, "Hardware reset nRF52: true");
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, "Hardware reset nRF52: false");
    ASSERT_TRUE(esp_log_wrapper_is_empty());
}

TEST_F(TestNRF52Fw, nrf52fw_nrf52fw_update_firmware_if_necessary__error_read_info_txt) // NOLINT
{
    const char *segment1_path = "segment_1.bin";
    const char *segment2_path = "segment_2.bin";
    const char *segment3_path = "segment_3.bin";

    const size_t segment1_size = 2816;
    const size_t segment2_size = 151016;
    const size_t segment3_size = 24448;

    uint32_t segment1_crc = 0;
    uint32_t segment2_crc = 0;
    uint32_t segment3_crc = 0;

    {
        std::unique_ptr<uint32_t[]> segment1_buf(new uint32_t[segment1_size / sizeof(uint32_t)]);
        for (int i = 0; i < segment1_size / sizeof(uint32_t); ++i)
        {
            segment1_buf[i] = 0xAA000000 + i;
        }
        segment1_crc = crc32_le(0, reinterpret_cast<const uint8_t *>(segment1_buf.get()), segment1_size);
        {
            this->m_fd = this->open_file(segment1_path, "wb");
            ASSERT_NE(nullptr, this->m_fd);
            fwrite(segment1_buf.get(), 1, segment1_size, this->m_fd);
            fclose(this->m_fd);
            this->m_fd = nullptr;
        }
    }

    {
        std::unique_ptr<uint32_t[]> segment2_buf(new uint32_t[segment2_size / sizeof(uint32_t)]);
        for (int i = 0; i < segment2_size / sizeof(uint32_t); ++i)
        {
            segment2_buf[i] = 0xBB000000 + i;
        }
        segment2_crc = crc32_le(0, reinterpret_cast<const uint8_t *>(segment2_buf.get()), segment2_size);
        {
            this->m_fd = this->open_file(segment2_path, "wb");
            ASSERT_NE(nullptr, this->m_fd);
            fwrite(segment2_buf.get(), 1, segment2_size, this->m_fd);
            fclose(this->m_fd);
            this->m_fd = nullptr;
        }
    }

    {
        std::unique_ptr<uint32_t[]> segment3_buf(new uint32_t[segment3_size / sizeof(uint32_t)]);
        for (int i = 0; i < segment3_size / sizeof(uint32_t); ++i)
        {
            segment3_buf[i] = 0xCC000000 + i;
        }
        segment3_crc = crc32_le(0, reinterpret_cast<const uint8_t *>(segment3_buf.get()), segment3_size);
        {
            this->m_fd = this->open_file(segment3_path, "wb");
            ASSERT_NE(nullptr, this->m_fd);
            fwrite(segment3_buf.get(), 1, segment3_size, this->m_fd);
            fclose(this->m_fd);
            this->m_fd = nullptr;
        }
    }

    // do not create "info.txt"

    {
        const uint32_t version = 0x01020300;
        this->m_memSegmentsRead.emplace_back(MemSegment(0x10001080, 1, &version));
    }

    ASSERT_FALSE(nrf52fw_update_firmware_if_necessary());

    ASSERT_EQ(0, this->m_memSegmentsWrite.size());
    ASSERT_EQ(0, this->m_cnt_nrf52swd_erase_all);

    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, "Hardware reset nRF52: true");
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, "Hardware reset nRF52: false");
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, "Init SWD");
    TEST_CHECK_LOG_RECORD_FLASH_FAT_FS(ESP_LOG_INFO, "Partition 'fatfs_nrf52' mounted successfully to /fs_nrf52");
    TEST_CHECK_LOG_RECORD_FLASH_FAT_FS(ESP_LOG_ERROR, "Can't open: ./fs_nrf52/info.txt");
    TEST_CHECK_LOG_RECORD(ESP_LOG_ERROR, "nrf52fw_read_info_txt: Can't open: info.txt");
    TEST_CHECK_LOG_RECORD(ESP_LOG_ERROR, "nrf52fw_update_firmware_if_necessary_step3: nrf52fw_read_info_txt failed");
    TEST_CHECK_LOG_RECORD_FLASH_FAT_FS(ESP_LOG_INFO, "Unmount ./fs_nrf52");
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, "Deinit SWD");
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, "Hardware reset nRF52: true");
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, "Hardware reset nRF52: false");
    ASSERT_TRUE(esp_log_wrapper_is_empty());
}

TEST_F(TestNRF52Fw, nrf52fw_nrf52fw_update_firmware_if_necessary__error_read_version) // NOLINT
{
    const char *segment1_path = "segment_1.bin";
    const char *segment2_path = "segment_2.bin";
    const char *segment3_path = "segment_3.bin";

    const size_t segment1_size = 2816;
    const size_t segment2_size = 151016;
    const size_t segment3_size = 24448;

    uint32_t segment1_crc = 0;
    uint32_t segment2_crc = 0;
    uint32_t segment3_crc = 0;

    {
        std::unique_ptr<uint32_t[]> segment1_buf(new uint32_t[segment1_size / sizeof(uint32_t)]);
        for (int i = 0; i < segment1_size / sizeof(uint32_t); ++i)
        {
            segment1_buf[i] = 0xAA000000 + i;
        }
        segment1_crc = crc32_le(0, reinterpret_cast<const uint8_t *>(segment1_buf.get()), segment1_size);
        {
            this->m_fd = this->open_file(segment1_path, "wb");
            ASSERT_NE(nullptr, this->m_fd);
            fwrite(segment1_buf.get(), 1, segment1_size, this->m_fd);
            fclose(this->m_fd);
            this->m_fd = nullptr;
        }
    }

    {
        std::unique_ptr<uint32_t[]> segment2_buf(new uint32_t[segment2_size / sizeof(uint32_t)]);
        for (int i = 0; i < segment2_size / sizeof(uint32_t); ++i)
        {
            segment2_buf[i] = 0xBB000000 + i;
        }
        segment2_crc = crc32_le(0, reinterpret_cast<const uint8_t *>(segment2_buf.get()), segment2_size);
        {
            this->m_fd = this->open_file(segment2_path, "wb");
            ASSERT_NE(nullptr, this->m_fd);
            fwrite(segment2_buf.get(), 1, segment2_size, this->m_fd);
            fclose(this->m_fd);
            this->m_fd = nullptr;
        }
    }

    {
        std::unique_ptr<uint32_t[]> segment3_buf(new uint32_t[segment3_size / sizeof(uint32_t)]);
        for (int i = 0; i < segment3_size / sizeof(uint32_t); ++i)
        {
            segment3_buf[i] = 0xCC000000 + i;
        }
        segment3_crc = crc32_le(0, reinterpret_cast<const uint8_t *>(segment3_buf.get()), segment3_size);
        {
            this->m_fd = this->open_file(segment3_path, "wb");
            ASSERT_NE(nullptr, this->m_fd);
            fwrite(segment3_buf.get(), 1, segment3_size, this->m_fd);
            fclose(this->m_fd);
            this->m_fd = nullptr;
        }
    }

    {
        this->m_fd = this->open_file("info.txt", "w");
        ASSERT_NE(nullptr, this->m_fd);
        fprintf(this->m_fd, "# v1.2.3\n");
        fprintf(this->m_fd, "0x00000000 %u %s 0x%08x\n", (unsigned)segment1_size, segment1_path, segment1_crc);
        fprintf(this->m_fd, "0x00001000 %u %s 0x%08x\n", (unsigned)segment2_size, segment2_path, segment2_crc);
        fprintf(this->m_fd, "0x00026000 %u %s 0x%08x\n", (unsigned)segment3_size, segment3_path, segment3_crc);
        fclose(this->m_fd);
        this->m_fd = nullptr;
    }

    ASSERT_FALSE(nrf52fw_update_firmware_if_necessary());

    ASSERT_EQ(0, this->m_memSegmentsWrite.size());
    ASSERT_EQ(0, this->m_cnt_nrf52swd_erase_all);

    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, "Hardware reset nRF52: true");
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, "Hardware reset nRF52: false");
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, "Init SWD");
    TEST_CHECK_LOG_RECORD_FLASH_FAT_FS(ESP_LOG_INFO, "Partition 'fatfs_nrf52' mounted successfully to /fs_nrf52");
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, "Firmware on FatFS: v1.2.3");
    TEST_CHECK_LOG_RECORD(
        ESP_LOG_ERROR,
        "nrf52fw_update_firmware_if_necessary_step3: nrf52fw_read_current_fw_version failed");
    TEST_CHECK_LOG_RECORD_FLASH_FAT_FS(ESP_LOG_INFO, "Unmount ./fs_nrf52");
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, "Deinit SWD");
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, "Hardware reset nRF52: true");
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, "Hardware reset nRF52: false");
    ASSERT_TRUE(esp_log_wrapper_is_empty());
}

TEST_F(TestNRF52Fw, nrf52fw_nrf52fw_update_firmware_if_necessary__error_check_firmware) // NOLINT
{
    const char *segment1_path = "segment_1.bin";
    const char *segment2_path = "segment_2.bin";
    const char *segment3_path = "segment_3.bin";

    const size_t segment1_size = 2816;
    const size_t segment2_size = 151016;
    const size_t segment3_size = 24448;

    uint32_t segment1_crc = 0;
    uint32_t segment2_crc = 0;
    uint32_t segment3_crc = 0;

    {
        std::unique_ptr<uint32_t[]> segment1_buf(new uint32_t[segment1_size / sizeof(uint32_t)]);
        for (int i = 0; i < segment1_size / sizeof(uint32_t); ++i)
        {
            segment1_buf[i] = 0xAA000000 + i;
        }
        segment1_crc = crc32_le(0, reinterpret_cast<const uint8_t *>(segment1_buf.get()), segment1_size);
        {
            this->m_fd = this->open_file(segment1_path, "wb");
            ASSERT_NE(nullptr, this->m_fd);
            fwrite(segment1_buf.get(), 1, segment1_size, this->m_fd);
            fclose(this->m_fd);
            this->m_fd = nullptr;
        }
    }

    {
        std::unique_ptr<uint32_t[]> segment2_buf(new uint32_t[segment2_size / sizeof(uint32_t)]);
        for (int i = 0; i < segment2_size / sizeof(uint32_t); ++i)
        {
            segment2_buf[i] = 0xBB000000 + i;
        }
        segment2_crc = crc32_le(0, reinterpret_cast<const uint8_t *>(segment2_buf.get()), segment2_size);
        {
            this->m_fd = this->open_file(segment2_path, "wb");
            ASSERT_NE(nullptr, this->m_fd);
            fwrite(segment2_buf.get(), 1, segment2_size, this->m_fd);
            fclose(this->m_fd);
            this->m_fd = nullptr;
        }
    }

    {
        std::unique_ptr<uint32_t[]> segment3_buf(new uint32_t[segment3_size / sizeof(uint32_t)]);
        for (int i = 0; i < segment3_size / sizeof(uint32_t); ++i)
        {
            segment3_buf[i] = 0xCC000000 + i;
        }
        segment3_crc = crc32_le(0, reinterpret_cast<const uint8_t *>(segment3_buf.get()), segment3_size);
        {
            this->m_fd = this->open_file(segment3_path, "wb");
            ASSERT_NE(nullptr, this->m_fd);
            fwrite(segment3_buf.get(), 1, segment3_size, this->m_fd);
            fclose(this->m_fd);
            this->m_fd = nullptr;
        }
    }

    {
        this->m_fd = this->open_file("info.txt", "w");
        ASSERT_NE(nullptr, this->m_fd);
        fprintf(this->m_fd, "# v1.2.3\n");
        fprintf(this->m_fd, "0x00000000 %u %s 0x%08x\n", (unsigned)segment1_size, segment1_path, segment1_crc);
        fprintf(this->m_fd, "0x00001000 %u %s 0x%08x\n", (unsigned)segment2_size, segment2_path, segment2_crc + 1);
        fprintf(this->m_fd, "0x00026000 %u %s 0x%08x\n", (unsigned)segment3_size, segment3_path, segment3_crc);
        fclose(this->m_fd);
        this->m_fd = nullptr;
    }

    {
        const uint32_t version = 0x01020000;
        this->m_memSegmentsRead.emplace_back(MemSegment(0x10001080, 1, &version));
    }

    ASSERT_FALSE(nrf52fw_update_firmware_if_necessary());

    ASSERT_EQ(0, this->m_memSegmentsWrite.size());
    ASSERT_EQ(0, this->m_cnt_nrf52swd_erase_all);

    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, "Hardware reset nRF52: true");
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, "Hardware reset nRF52: false");
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, "Init SWD");
    TEST_CHECK_LOG_RECORD_FLASH_FAT_FS(ESP_LOG_INFO, "Partition 'fatfs_nrf52' mounted successfully to /fs_nrf52");
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, "Firmware on FatFS: v1.2.3");
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, "Firmware on nRF52: v1.2.0");
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, "Need to update firmware on nRF52");
    TEST_CHECK_LOG_RECORD(ESP_LOG_ERROR, "nrf52fw_check_firmware: nrf52fw_calc_segment_crc failed");
    TEST_CHECK_LOG_RECORD(
        ESP_LOG_ERROR,
        "nrf52fw_check_firmware: Segment 1: 0x00001000: expected CRC: 0x5b3ddbc1, actual CRC: 0x5b3ddbc0");
    TEST_CHECK_LOG_RECORD(ESP_LOG_ERROR, "nrf52fw_update_firmware_if_necessary_step3: nrf52fw_check_firmware failed");
    TEST_CHECK_LOG_RECORD_FLASH_FAT_FS(ESP_LOG_INFO, "Unmount ./fs_nrf52");
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, "Deinit SWD");
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, "Hardware reset nRF52: true");
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, "Hardware reset nRF52: false");
    ASSERT_TRUE(esp_log_wrapper_is_empty());
}

TEST_F(TestNRF52Fw, nrf52fw_nrf52fw_update_firmware_if_necessary__error_write_firmware) // NOLINT
{
    const char *segment1_path = "segment_1.bin";
    const char *segment2_path = "segment_2.bin";
    const char *segment3_path = "segment_3.bin";

    const size_t segment1_size = 2816;
    const size_t segment2_size = 151016;
    const size_t segment3_size = 24448;

    uint32_t segment1_crc = 0;
    uint32_t segment2_crc = 0;
    uint32_t segment3_crc = 0;

    {
        std::unique_ptr<uint32_t[]> segment1_buf(new uint32_t[segment1_size / sizeof(uint32_t)]);
        for (int i = 0; i < segment1_size / sizeof(uint32_t); ++i)
        {
            segment1_buf[i] = 0xAA000000 + i;
        }
        segment1_crc = crc32_le(0, reinterpret_cast<const uint8_t *>(segment1_buf.get()), segment1_size);
        {
            this->m_fd = this->open_file(segment1_path, "wb");
            ASSERT_NE(nullptr, this->m_fd);
            fwrite(segment1_buf.get(), 1, segment1_size, this->m_fd);
            fclose(this->m_fd);
            this->m_fd = nullptr;
        }
    }

    {
        std::unique_ptr<uint32_t[]> segment2_buf(new uint32_t[segment2_size / sizeof(uint32_t)]);
        for (int i = 0; i < segment2_size / sizeof(uint32_t); ++i)
        {
            segment2_buf[i] = 0xBB000000 + i;
        }
        segment2_crc = crc32_le(0, reinterpret_cast<const uint8_t *>(segment2_buf.get()), segment2_size);
        {
            this->m_fd = this->open_file(segment2_path, "wb");
            ASSERT_NE(nullptr, this->m_fd);
            fwrite(segment2_buf.get(), 1, segment2_size, this->m_fd);
            fclose(this->m_fd);
            this->m_fd = nullptr;
        }
    }

    {
        std::unique_ptr<uint32_t[]> segment3_buf(new uint32_t[segment3_size / sizeof(uint32_t)]);
        for (int i = 0; i < segment3_size / sizeof(uint32_t); ++i)
        {
            segment3_buf[i] = 0xCC000000 + i;
        }
        segment3_crc = crc32_le(0, reinterpret_cast<const uint8_t *>(segment3_buf.get()), segment3_size);
        {
            this->m_fd = this->open_file(segment3_path, "wb");
            ASSERT_NE(nullptr, this->m_fd);
            fwrite(segment3_buf.get(), 1, segment3_size, this->m_fd);
            fclose(this->m_fd);
            this->m_fd = nullptr;
        }
    }

    {
        this->m_fd = this->open_file("info.txt", "w");
        ASSERT_NE(nullptr, this->m_fd);
        fprintf(this->m_fd, "# v1.2.3\n");
        fprintf(this->m_fd, "0x00000000 %u %s 0x%08x\n", (unsigned)segment1_size, segment1_path, segment1_crc);
        fprintf(this->m_fd, "0x00001000 %u %s 0x%08x\n", (unsigned)segment2_size, segment2_path, segment2_crc);
        fprintf(this->m_fd, "0x00026000 %u %s 0x%08x\n", (unsigned)segment3_size, segment3_path, segment3_crc);
        fclose(this->m_fd);
        this->m_fd = nullptr;
    }

    {
        const uint32_t version = 0x01020000;
        this->m_memSegmentsRead.emplace_back(MemSegment(0x10001080, 1, &version));
    }
    {
        const uint32_t stub = 0;
        this->m_memSegmentsWrite.emplace_back(MemSegment(0x00000000, 1, &stub));
    }

    ASSERT_FALSE(nrf52fw_update_firmware_if_necessary());

    ASSERT_EQ(1, this->m_memSegmentsWrite.size());
    ASSERT_EQ(1, this->m_cnt_nrf52swd_erase_all);

    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, "Hardware reset nRF52: true");
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, "Hardware reset nRF52: false");
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, "Init SWD");
    TEST_CHECK_LOG_RECORD_FLASH_FAT_FS(ESP_LOG_INFO, "Partition 'fatfs_nrf52' mounted successfully to /fs_nrf52");
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, "Firmware on FatFS: v1.2.3");
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, "Firmware on nRF52: v1.2.0");
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, "Need to update firmware on nRF52");
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, "Erasing flash memory...");
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, "Flash 3 segments");
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, "Flash segment 0: 0x00000000 size=2816 from segment_1.bin");
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, "Writing 0x00000000...");
    TEST_CHECK_LOG_RECORD(ESP_LOG_ERROR, "nrf52fw_flash_write_segment: nrf52swd_write_mem failed");
    TEST_CHECK_LOG_RECORD(
        ESP_LOG_ERROR,
        "nrf52fw_flash_write_segment_from_file: Failed to write segment 0x00000000 from 'segment_1.bin'");
    TEST_CHECK_LOG_RECORD(
        ESP_LOG_ERROR,
        "nrf52fw_flash_write_firmware: Failed to write segment 0: 0x00000000 from segment_1.bin");
    TEST_CHECK_LOG_RECORD(
        ESP_LOG_ERROR,
        "nrf52fw_update_firmware_if_necessary_step3: nrf52fw_flash_write_firmware failed");
    TEST_CHECK_LOG_RECORD_FLASH_FAT_FS(ESP_LOG_INFO, "Unmount ./fs_nrf52");
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, "Deinit SWD");
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, "Hardware reset nRF52: true");
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, "Hardware reset nRF52: false");
    ASSERT_TRUE(esp_log_wrapper_is_empty());
}
