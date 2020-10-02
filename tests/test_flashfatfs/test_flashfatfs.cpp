/**
 * @file test_bin2hex.cpp
 * @author TheSomeMan
 * @date 2020-08-27
 * @copyright Ruuvi Innovations Ltd, license BSD-3-Clause.
 */

#include "flashfatfs.h"
#include <string>
#include <sys/stat.h>
#include <ftw.h>
#include "gtest/gtest.h"
#include "esp_log_wrapper.hpp"
#include "esp_err.h"
#include "esp_vfs_fat.h"

using namespace std;

/*** Google-test class implementation
 * *********************************************************************************/

struct FlashFatFs_Tag
{
    char *      mount_point;
    wl_handle_t wl_handle;
};

class TestNRF52Fw;
static TestNRF52Fw *g_pTestClass;

class NRF52Fw_VFS_FAT_MountInfo
{
public:
    string                     base_path;
    string                     partition_label;
    esp_vfs_fat_mount_config_t mount_config = {};
    bool                       flag_mounted = false;
    esp_err_t                  mount_err    = ESP_OK;
    esp_err_t                  unmount_err  = ESP_OK;
    wl_handle_t                wl_handle    = 0;
};

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

    void
    SetUp() override
    {
#define FS_NRF52_MOUNT_POINT "fs_nrf52"
        this->m_mount_point_dir = FS_NRF52_MOUNT_POINT;
        this->m_mount_point     = "/" FS_NRF52_MOUNT_POINT;
        {
            remove_dir_with_files(this->m_mount_point_dir);
            mkdir(this->m_mount_point_dir, 0700);
        }
        this->m_fd    = nullptr;
        this->m_p_ffs = nullptr;
        esp_log_wrapper_init();
        g_pTestClass = this;

        this->m_malloc_cnt              = 0;
        this->m_malloc_fail_on_cnt      = 0;
        this->m_mount_info.flag_mounted = false;
        this->m_mount_info.mount_err    = ESP_OK;
        this->m_mount_info.unmount_err  = ESP_OK;
        this->m_mount_info.wl_handle    = 0;
    }

    void
    TearDown() override
    {
        g_pTestClass = nullptr;
        if (nullptr != this->m_fd)
        {
            fclose(this->m_fd);
            this->m_fd = nullptr;
        }
        if (nullptr != this->m_p_ffs)
        {
            flashfatfs_unmount(this->m_p_ffs);
            this->m_p_ffs = nullptr;
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

    uint32_t                  m_malloc_cnt;
    uint32_t                  m_malloc_fail_on_cnt;
    NRF52Fw_VFS_FAT_MountInfo m_mount_info;
    MemAllocTrace             m_mem_alloc_trace;
};

TestNRF52Fw::TestNRF52Fw()
    : Test()
{
}

TestNRF52Fw::~TestNRF52Fw() = default;

extern "C" {

void *
app_malloc(const size_t size)
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
app_free(void *ptr)
{
    g_pTestClass->m_mem_alloc_trace.remove(ptr);
    free(ptr);
}

void *
app_calloc(const size_t nmemb, const size_t size)
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

#define TEST_CHECK_LOG_RECORD_FLASH_FAT_FS(level_, msg_) TEST_CHECK_LOG_RECORD_EX("FlashFatFS", level_, msg_);

/*** Unit-Tests
 * *******************************************************************************************************/

TEST_F(TestNRF52Fw, flashfatfs_mount_ok_rel_path) // NOLINT
{
    const wl_handle_t wl_handle  = 25;
    const int         max_files  = 1;
    this->m_mount_info.wl_handle = wl_handle;

    this->m_p_ffs = flashfatfs_mount("fs_nrf52", GW_NRF_PARTITION, max_files);

    ASSERT_NE(nullptr, this->m_p_ffs);
    ASSERT_EQ(string("fs_nrf52"), string(this->m_p_ffs->mount_point));
    ASSERT_EQ(wl_handle, this->m_p_ffs->wl_handle);
    ASSERT_TRUE(this->m_mount_info.flag_mounted);
    ASSERT_EQ("fs_nrf52", this->m_mount_info.base_path);
    ASSERT_EQ(GW_NRF_PARTITION, this->m_mount_info.partition_label);
    ASSERT_FALSE(this->m_mount_info.mount_config.format_if_mount_failed);
    ASSERT_EQ(max_files, this->m_mount_info.mount_config.max_files);
    ASSERT_EQ(512U, this->m_mount_info.mount_config.allocation_unit_size);

    ASSERT_TRUE(flashfatfs_unmount(this->m_p_ffs));
    this->m_p_ffs = nullptr;

    TEST_CHECK_LOG_RECORD_FLASH_FAT_FS(ESP_LOG_INFO, "Partition 'fatfs_nrf52' mounted successfully to fs_nrf52");
    TEST_CHECK_LOG_RECORD_FLASH_FAT_FS(ESP_LOG_INFO, "Unmount fs_nrf52");
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_TRUE(this->m_mem_alloc_trace.is_empty());
}

TEST_F(TestNRF52Fw, flashfatfs_mount_ok_abs_path) // NOLINT
{
    const wl_handle_t wl_handle  = 25;
    const int         max_files  = 1;
    this->m_mount_info.wl_handle = wl_handle;

    this->m_p_ffs = flashfatfs_mount("/fs_nrf52", GW_NRF_PARTITION, max_files);

    ASSERT_NE(nullptr, this->m_p_ffs);
    ASSERT_EQ(string("./fs_nrf52"), string(this->m_p_ffs->mount_point));
    ASSERT_EQ(wl_handle, this->m_p_ffs->wl_handle);
    ASSERT_TRUE(this->m_mount_info.flag_mounted);
    ASSERT_EQ("/fs_nrf52", this->m_mount_info.base_path);
    ASSERT_EQ(GW_NRF_PARTITION, this->m_mount_info.partition_label);
    ASSERT_FALSE(this->m_mount_info.mount_config.format_if_mount_failed);
    ASSERT_EQ(max_files, this->m_mount_info.mount_config.max_files);
    ASSERT_EQ(512U, this->m_mount_info.mount_config.allocation_unit_size);

    ASSERT_TRUE(flashfatfs_unmount(this->m_p_ffs));
    this->m_p_ffs = nullptr;

    TEST_CHECK_LOG_RECORD_FLASH_FAT_FS(ESP_LOG_INFO, "Partition 'fatfs_nrf52' mounted successfully to /fs_nrf52");
    TEST_CHECK_LOG_RECORD_FLASH_FAT_FS(ESP_LOG_INFO, "Unmount ./fs_nrf52");
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_TRUE(this->m_mem_alloc_trace.is_empty());
}

TEST_F(TestNRF52Fw, flashfatfs_mount_ok_unmount_failed) // NOLINT
{
    const wl_handle_t wl_handle  = 25;
    const int         max_files  = 1;
    this->m_mount_info.wl_handle = wl_handle;

    this->m_p_ffs = flashfatfs_mount("fs_nrf52", GW_NRF_PARTITION, max_files);

    ASSERT_NE(nullptr, this->m_p_ffs);

    this->m_mount_info.unmount_err = ESP_ERR_NOT_SUPPORTED;
    ASSERT_FALSE(flashfatfs_unmount(this->m_p_ffs));
    this->m_p_ffs = nullptr;

    TEST_CHECK_LOG_RECORD_FLASH_FAT_FS(ESP_LOG_INFO, "Partition 'fatfs_nrf52' mounted successfully to fs_nrf52");
    TEST_CHECK_LOG_RECORD_FLASH_FAT_FS(ESP_LOG_INFO, "Unmount fs_nrf52");
    TEST_CHECK_LOG_RECORD_FLASH_FAT_FS(
        ESP_LOG_ERROR,
        "flashfatfs_unmount: esp_vfs_fat_spiflash_unmount failed, err=262");
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_TRUE(this->m_mem_alloc_trace.is_empty());
}

TEST_F(TestNRF52Fw, flashfatfs_mount_failed_no_mem) // NOLINT
{
    const int max_files        = 1;
    this->m_malloc_fail_on_cnt = 1;
    this->m_p_ffs              = flashfatfs_mount(this->m_mount_point, GW_NRF_PARTITION, max_files);
    ASSERT_EQ(nullptr, this->m_p_ffs);

    TEST_CHECK_LOG_RECORD_FLASH_FAT_FS(ESP_LOG_ERROR, "flashfatfs_mount: Can't allocate memory");
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_TRUE(this->m_mem_alloc_trace.is_empty());
}

TEST_F(TestNRF52Fw, flashfatfs_mount_failed_on_spiflash_mount) // NOLINT
{
    const int max_files          = 1;
    this->m_mount_info.mount_err = ESP_ERR_NOT_FOUND;
    this->m_p_ffs                = flashfatfs_mount(this->m_mount_point, GW_NRF_PARTITION, max_files);
    ASSERT_EQ(nullptr, this->m_p_ffs);

    TEST_CHECK_LOG_RECORD_FLASH_FAT_FS(ESP_LOG_ERROR, "flashfatfs_mount: esp_vfs_fat_spiflash_mount failed, err=261");
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_TRUE(this->m_mem_alloc_trace.is_empty());
}

TEST_F(TestNRF52Fw, flashfatfs_open_ok) // NOLINT
{
    const wl_handle_t wl_handle  = 25;
    const int         max_files  = 1;
    this->m_mount_info.wl_handle = wl_handle;

    const char *test_file_name = "test1.txt";
    const char *test_text      = "test123\n";
    {
        char tmp_file_path[80];
        snprintf(tmp_file_path, sizeof(tmp_file_path), "%s/%s", this->m_mount_point_dir, test_file_name);
        FILE *fd = fopen(tmp_file_path, "w");
        ASSERT_NE(nullptr, fd);
        fprintf(fd, "%s", test_text);
        fclose(fd);
    }

    this->m_p_ffs = flashfatfs_mount("fs_nrf52", GW_NRF_PARTITION, max_files);
    ASSERT_NE(nullptr, this->m_p_ffs);

    FileDescriptor_t fd = flashfatfs_open(this->m_p_ffs, test_file_name);
    ASSERT_GE(fd, 0);

    {
        char      buf[80];
        const int len = read(fd, buf, sizeof(buf));
        ASSERT_EQ(8, len);
        buf[len] = '\0';
        ASSERT_EQ(string(test_text), string(buf));
    }

    close(fd);

    ASSERT_TRUE(flashfatfs_unmount(this->m_p_ffs));
    this->m_p_ffs = nullptr;

    TEST_CHECK_LOG_RECORD_FLASH_FAT_FS(ESP_LOG_INFO, "Partition 'fatfs_nrf52' mounted successfully to fs_nrf52");
    TEST_CHECK_LOG_RECORD_FLASH_FAT_FS(ESP_LOG_INFO, "Unmount fs_nrf52");
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_TRUE(this->m_mem_alloc_trace.is_empty());
}

TEST_F(TestNRF52Fw, flashfatfs_open_failed) // NOLINT
{
    const wl_handle_t wl_handle  = 25;
    const int         max_files  = 1;
    this->m_mount_info.wl_handle = wl_handle;

    const char *test_file_name = "test1.txt";

    this->m_p_ffs = flashfatfs_mount("fs_nrf52", GW_NRF_PARTITION, max_files);
    ASSERT_NE(nullptr, this->m_p_ffs);

    FileDescriptor_t fd = flashfatfs_open(this->m_p_ffs, test_file_name);
    ASSERT_LT(fd, 0);

    ASSERT_TRUE(flashfatfs_unmount(this->m_p_ffs));
    this->m_p_ffs = nullptr;

    TEST_CHECK_LOG_RECORD_FLASH_FAT_FS(ESP_LOG_INFO, "Partition 'fatfs_nrf52' mounted successfully to fs_nrf52");
    TEST_CHECK_LOG_RECORD_FLASH_FAT_FS(ESP_LOG_ERROR, "Can't open: fs_nrf52/test1.txt");
    TEST_CHECK_LOG_RECORD_FLASH_FAT_FS(ESP_LOG_INFO, "Unmount fs_nrf52");
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_TRUE(this->m_mem_alloc_trace.is_empty());
}

TEST_F(TestNRF52Fw, flashfatfs_fopen_ascii_ok) // NOLINT
{
    const wl_handle_t wl_handle  = 25;
    const int         max_files  = 1;
    this->m_mount_info.wl_handle = wl_handle;

    const char *test_file_name = "test1.txt";
    const char *test_text      = "test123\n";
    {
        char tmp_file_path[80];
        snprintf(tmp_file_path, sizeof(tmp_file_path), "%s/%s", this->m_mount_point_dir, test_file_name);
        FILE *fd = fopen(tmp_file_path, "w");
        ASSERT_NE(nullptr, fd);
        fprintf(fd, "%s", test_text);
        fclose(fd);
    }

    this->m_p_ffs = flashfatfs_mount("fs_nrf52", GW_NRF_PARTITION, max_files);
    ASSERT_NE(nullptr, this->m_p_ffs);

    const bool flag_use_binary_mode = false;
    FILE *     p_fd                 = flashfatfs_fopen(this->m_p_ffs, test_file_name, flag_use_binary_mode);
    ASSERT_NE(nullptr, p_fd);

    {
        char      buf[80];
        const int len = fread(buf, 1, sizeof(buf), p_fd);
        ASSERT_EQ(8, len);
        buf[len] = '\0';
        ASSERT_EQ(string(test_text), string(buf));
    }

    fclose(p_fd);

    ASSERT_TRUE(flashfatfs_unmount(this->m_p_ffs));
    this->m_p_ffs = nullptr;

    TEST_CHECK_LOG_RECORD_FLASH_FAT_FS(ESP_LOG_INFO, "Partition 'fatfs_nrf52' mounted successfully to fs_nrf52");
    TEST_CHECK_LOG_RECORD_FLASH_FAT_FS(ESP_LOG_INFO, "Unmount fs_nrf52");
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_TRUE(this->m_mem_alloc_trace.is_empty());
}

TEST_F(TestNRF52Fw, flashfatfs_fopen_binary_ok) // NOLINT
{
    const wl_handle_t wl_handle  = 25;
    const int         max_files  = 1;
    this->m_mount_info.wl_handle = wl_handle;

    const char *test_file_name = "test1.txt";
    const char *test_text      = "test123\n";
    {
        char tmp_file_path[80];
        snprintf(tmp_file_path, sizeof(tmp_file_path), "%s/%s", this->m_mount_point_dir, test_file_name);
        FILE *fd = fopen(tmp_file_path, "w");
        ASSERT_NE(nullptr, fd);
        fprintf(fd, "%s", test_text);
        fclose(fd);
    }

    this->m_p_ffs = flashfatfs_mount("fs_nrf52", GW_NRF_PARTITION, max_files);
    ASSERT_NE(nullptr, this->m_p_ffs);

    const bool flag_use_binary_mode = true;
    FILE *     p_fd                 = flashfatfs_fopen(this->m_p_ffs, test_file_name, flag_use_binary_mode);
    ASSERT_NE(nullptr, p_fd);

    {
        char      buf[80];
        const int len = fread(buf, 1, sizeof(buf), p_fd);
        ASSERT_EQ(8, len);
        buf[len] = '\0';
        ASSERT_EQ(string(test_text), string(buf));
    }

    fclose(p_fd);

    ASSERT_TRUE(flashfatfs_unmount(this->m_p_ffs));
    this->m_p_ffs = nullptr;

    TEST_CHECK_LOG_RECORD_FLASH_FAT_FS(ESP_LOG_INFO, "Partition 'fatfs_nrf52' mounted successfully to fs_nrf52");
    TEST_CHECK_LOG_RECORD_FLASH_FAT_FS(ESP_LOG_INFO, "Unmount fs_nrf52");
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_TRUE(this->m_mem_alloc_trace.is_empty());
}

TEST_F(TestNRF52Fw, flashfatfs_fopen_failed) // NOLINT
{
    const wl_handle_t wl_handle  = 25;
    const int         max_files  = 1;
    this->m_mount_info.wl_handle = wl_handle;

    const char *test_file_name = "test1.txt";

    this->m_p_ffs = flashfatfs_mount("fs_nrf52", GW_NRF_PARTITION, max_files);
    ASSERT_NE(nullptr, this->m_p_ffs);

    const bool flag_use_binary_mode = false;
    FILE *     p_fd                 = flashfatfs_fopen(this->m_p_ffs, test_file_name, flag_use_binary_mode);
    ASSERT_EQ(nullptr, p_fd);

    ASSERT_TRUE(flashfatfs_unmount(this->m_p_ffs));
    this->m_p_ffs = nullptr;

    TEST_CHECK_LOG_RECORD_FLASH_FAT_FS(ESP_LOG_INFO, "Partition 'fatfs_nrf52' mounted successfully to fs_nrf52");
    TEST_CHECK_LOG_RECORD_FLASH_FAT_FS(ESP_LOG_ERROR, "Can't open: fs_nrf52/test1.txt");
    TEST_CHECK_LOG_RECORD_FLASH_FAT_FS(ESP_LOG_INFO, "Unmount fs_nrf52");
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_TRUE(this->m_mem_alloc_trace.is_empty());
}
