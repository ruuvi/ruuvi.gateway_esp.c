/**
 * @file test_bin2hex.cpp
 * @author TheSomeMan
 * @date 2020-08-27
 * @copyright Ruuvi Innovations Ltd, license BSD-3-Clause.
 */

#include "nrf52fw.h"
#include <string>
#include <driver/gpio.h>
#include <driver/spi_master.h>
#include "gtest/gtest.h"
#include "esp_log_wrapper.hpp"
#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "libswd.h"
#include "nrf52swd.h"

using namespace std;

/*** Google-test class implementation
 * *********************************************************************************/

class TestNRF52Swd;
static TestNRF52Swd *g_pTestClass;

extern "C" {

typedef struct spi_device_t
{
    int stub;
} spi_device_t;

const char *
os_task_get_name(void)
{
    static const char g_task_name[] = "main";
    return const_cast<char *>(g_task_name);
}

/*** gpio_switch_ctrl.c stub functions
 * *************************************************************************************/

void
gpio_switch_ctrl_activate(void)
{
}

void
gpio_switch_ctrl_deactivate(void)
{
}

} // extern "C"

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

class TestNRF52Swd : public ::testing::Test
{
private:
protected:
    void
    SetUp() override
    {
        esp_log_wrapper_init();
        g_pTestClass           = this;
        this->m_vTaskDelay_cnt = 0;
        {
            const gpio_config_t gpio_cfg = { 0 };
            for (uint32_t i = 0; i < sizeof(this->m_gpio_config) / sizeof(this->m_gpio_config[0]); ++i)
            {
                this->m_gpio_config[i] = gpio_cfg;
            }
            this->m_gpio_config_result    = ESP_OK;
            this->m_gpio_nrf52_nrst_level = ~0U;
            this->m_gpio_set_level_result = ESP_OK;
        }
        {
            this->m_spi_host                      = (spi_host_device_t)-1;
            const spi_bus_config_t spi_buf_config = { 0 };
            this->m_spi_bus_config                = spi_buf_config;
            this->m_spi_dma_chan                  = -1;
            this->m_spi_bus_initialize_result     = ESP_OK;
            this->m_spi_bus_free_result           = ESP_OK;
        }
        {
            this->m_spi_add_device_host                    = (spi_host_device_t)-1;
            const spi_device_interface_config_t dev_config = { 0 };
            this->m_spi_add_device_config                  = dev_config;
            this->m_spi_add_device_desc.stub               = 0;
            this->m_spi_add_device_result                  = ESP_OK;
            this->m_spi_remove_device_result               = ESP_OK;
        }
        {
            this->m_libswd_init_failed = false;
            libswd_ctx_t libswd_ctx    = { nullptr };
            this->m_libswd_ctx         = libswd_ctx;
        }

        {
            this->m_libswd_debug_init_result    = 0;
            this->m_libswd_debug_init_operation = (libswd_operation_t)0;
        }
        {
            this->m_libswd_dap_detect_operation = (libswd_operation_t)0;
            this->m_libswd_dap_detect_idcode    = 0;
            this->m_libswd_dap_detect_result    = 0;
        }
        {
            this->m_libswd_debug_halt_operation = (libswd_operation_t)0;
            this->m_libswd_debug_halt_result    = 0;
        }
        {
            this->m_libswd_debug_enable_reset_vector_catch_operation = (libswd_operation_t)0;
            this->m_libswd_debug_enable_reset_vector_catch_result    = 0;
        }
        {
            this->m_libswd_debug_reset_operation = (libswd_operation_t)0;
            this->m_libswd_debug_reset_result    = 0;
        }
        {
            this->m_libswd_debug_run_operation = (libswd_operation_t)0;
            this->m_libswd_debug_run_result    = 0;
        }
        {
            this->m_libswd_read_int_32_operation = (libswd_operation_t)0;
            this->m_libswd_read_int_32_result    = 0;
        }
        {
            this->m_libswd_write_int_32_operation       = (libswd_operation_t)0;
            this->m_libswd_write_int_32_cnt_before_fail = 0;
        }
        {
            this->m_nvmc_reg_ready_cnt             = 0;
            this->m_nvmc_reg_ready_cnt_before_fail = 0;
        }
        {
            this->m_nvmc_reg_config_cnt_before_fail = 0;
        }
    }

    void
    TearDown() override
    {
        nrf52swd_deinit_gpio_cfg_nreset();
        g_pTestClass = nullptr;
        esp_log_wrapper_deinit();
        this->m_memSegmentsRead.clear();
        this->m_memSegmentsWrite.clear();
    }

public:
    TestNRF52Swd();

    ~TestNRF52Swd() override;

    uint32_t m_vTaskDelay_cnt;

    gpio_config_t m_gpio_config[64];
    esp_err_t     m_gpio_config_result;
    uint32_t      m_gpio_nrf52_nrst_level;
    esp_err_t     m_gpio_set_level_result;

    spi_host_device_t m_spi_host;
    spi_bus_config_t  m_spi_bus_config;
    int               m_spi_dma_chan;
    esp_err_t         m_spi_bus_initialize_result;
    esp_err_t         m_spi_bus_free_result;

    spi_host_device_t             m_spi_add_device_host;
    spi_device_interface_config_t m_spi_add_device_config;
    spi_device_t                  m_spi_add_device_desc;
    esp_err_t                     m_spi_add_device_result;
    esp_err_t                     m_spi_remove_device_result;

    libswd_ctx_t    m_libswd_ctx;
    libswd_driver_t m_libswd_driver;
    bool            m_libswd_init_failed;

    int                m_libswd_debug_init_result;
    libswd_operation_t m_libswd_debug_init_operation;

    libswd_operation_t m_libswd_dap_detect_operation;
    int                m_libswd_dap_detect_idcode;
    int                m_libswd_dap_detect_result;

    libswd_operation_t m_libswd_debug_halt_operation;
    int                m_libswd_debug_halt_result;

    libswd_operation_t m_libswd_debug_enable_reset_vector_catch_operation;
    int                m_libswd_debug_enable_reset_vector_catch_result;

    libswd_operation_t m_libswd_debug_reset_operation;
    int                m_libswd_debug_reset_result;

    libswd_operation_t m_libswd_debug_run_operation;
    int                m_libswd_debug_run_result;

    vector<MemSegment> m_memSegmentsWrite;
    vector<MemSegment> m_memSegmentsRead;

    libswd_operation_t m_libswd_read_int_32_operation;
    int                m_libswd_read_int_32_result;

    libswd_operation_t m_libswd_write_int_32_operation;
    uint32_t           m_libswd_write_int_32_cnt_before_fail;

    uint32_t m_nvmc_reg_ready_cnt;
    uint32_t m_nvmc_reg_ready_cnt_before_fail;

    uint32_t m_nvmc_reg_config_cnt_before_fail;
    uint32_t m_nvmc_reg_erase_all_cnt_before_fail;

    bool
    write_mem(const uint32_t addr, const uint32_t num_words, const uint32_t *p_buf)
    {
        this->m_memSegmentsWrite.emplace_back(addr, num_words, p_buf);
        if ((0x4001E000UL + 0x504U) == addr)
        {
            if (this->m_nvmc_reg_config_cnt_before_fail > 0)
            {
                if (0 == --this->m_nvmc_reg_config_cnt_before_fail)
                {
                    return false;
                }
            }
        }
        else if ((0x4001E000UL + 0x50CU) == addr)
        {
            if (this->m_nvmc_reg_erase_all_cnt_before_fail > 0)
            {
                if (0 == --this->m_nvmc_reg_erase_all_cnt_before_fail)
                {
                    return false;
                }
            }
        }
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

TestNRF52Swd::TestNRF52Swd()
    : Test()
{
}

TestNRF52Swd::~TestNRF52Swd() = default;

extern "C" {

void
vTaskDelay(const TickType_t xTicksToDelay)
{
    g_pTestClass->m_vTaskDelay_cnt += 1;
}

esp_err_t
gpio_config(const gpio_config_t *pGPIOConfig)
{
    for (uint32_t bit_idx = 0; bit_idx < 64; ++bit_idx)
    {
        if (0 != (pGPIOConfig->pin_bit_mask & (1LLU << bit_idx)))
        {
            g_pTestClass->m_gpio_config[bit_idx] = *pGPIOConfig;
        }
    }
    return g_pTestClass->m_gpio_config_result;
}

esp_err_t
gpio_set_level(gpio_num_t gpio_num, uint32_t level)
{
    if (0 == ((1ULL << (uint32_t)gpio_num) & g_pTestClass->m_gpio_config[gpio_num].pin_bit_mask))
    {
        assert(0);
    }
    assert(0 != ((1ULL << (uint32_t)gpio_num) & g_pTestClass->m_gpio_config[gpio_num].pin_bit_mask));
    g_pTestClass->m_gpio_nrf52_nrst_level = level;
    return g_pTestClass->m_gpio_set_level_result;
}

esp_err_t
spi_bus_initialize(spi_host_device_t host, const spi_bus_config_t *bus_config, int dma_chan)
{
    g_pTestClass->m_spi_host       = host;
    g_pTestClass->m_spi_bus_config = *bus_config;
    g_pTestClass->m_spi_dma_chan   = dma_chan;
    return g_pTestClass->m_spi_bus_initialize_result;
}

esp_err_t
spi_bus_free(spi_host_device_t host)
{
    return g_pTestClass->m_spi_bus_free_result;
}

esp_err_t
spi_bus_add_device(spi_host_device_t host, const spi_device_interface_config_t *dev_config, spi_device_handle_t *handle)
{
    g_pTestClass->m_spi_add_device_host   = host;
    g_pTestClass->m_spi_add_device_config = *dev_config;
    *handle                               = &g_pTestClass->m_spi_add_device_desc;
    return g_pTestClass->m_spi_add_device_result;
}

esp_err_t
spi_bus_remove_device(spi_device_handle_t handle)
{
    assert(handle == &g_pTestClass->m_spi_add_device_desc);
    g_pTestClass->m_spi_add_device_host = (spi_host_device_t)-1;
    return g_pTestClass->m_spi_remove_device_result;
}

libswd_ctx_t *
libswd_init(void)
{
    if (g_pTestClass->m_libswd_init_failed)
    {
        return nullptr;
    }
    g_pTestClass->m_libswd_ctx.driver = &g_pTestClass->m_libswd_driver;

    g_pTestClass->m_libswd_ctx.config.initialized = LIBSWD_TRUE;
    return &g_pTestClass->m_libswd_ctx;
}

int
libswd_deinit(libswd_ctx_t *libswdctx)
{
    assert(&g_pTestClass->m_libswd_ctx == libswdctx);
    return 0;
}

int
libswd_log_level_set(libswd_ctx_t *libswdctx, libswd_loglevel_t loglevel)
{
    assert(nullptr != libswdctx);
    libswdctx->config.loglevel = loglevel;
    return 0;
}

int
libswd_debug_init(libswd_ctx_t *libswdctx, libswd_operation_t operation)
{
    assert(&g_pTestClass->m_libswd_ctx == libswdctx);
    g_pTestClass->m_libswd_debug_init_operation = operation;
    return g_pTestClass->m_libswd_debug_init_result;
}

int
libswd_dap_detect(libswd_ctx_t *libswdctx, libswd_operation_t operation, int **idcode)
{
    assert(&g_pTestClass->m_libswd_ctx == libswdctx);
    g_pTestClass->m_libswd_dap_detect_operation = operation;

    *idcode = &g_pTestClass->m_libswd_dap_detect_idcode;
    return g_pTestClass->m_libswd_dap_detect_result;
}

int
libswd_debug_halt(libswd_ctx_t *libswdctx, libswd_operation_t operation)
{
    assert(&g_pTestClass->m_libswd_ctx == libswdctx);
    g_pTestClass->m_libswd_debug_halt_operation = operation;
    return g_pTestClass->m_libswd_debug_halt_result;
}

int
libswd_debug_enable_reset_vector_catch(libswd_ctx_t *libswdctx, libswd_operation_t operation)
{
    assert(&g_pTestClass->m_libswd_ctx == libswdctx);
    g_pTestClass->m_libswd_debug_enable_reset_vector_catch_operation = operation;
    return g_pTestClass->m_libswd_debug_enable_reset_vector_catch_result;
}

int
libswd_debug_reset(libswd_ctx_t *libswdctx, libswd_operation_t operation)
{
    assert(&g_pTestClass->m_libswd_ctx == libswdctx);
    g_pTestClass->m_libswd_debug_reset_operation = operation;
    return g_pTestClass->m_libswd_debug_reset_result;
}

int
libswd_debug_run(libswd_ctx_t *libswdctx, libswd_operation_t operation)
{
    assert(&g_pTestClass->m_libswd_ctx == libswdctx);
    g_pTestClass->m_libswd_debug_run_operation = operation;
    return g_pTestClass->m_libswd_debug_run_result;
}

int
libswd_memap_read_int_32(libswd_ctx_t *libswdctx, libswd_operation_t operation, int addr, int count, int *data)
{
    assert(&g_pTestClass->m_libswd_ctx == libswdctx);
    g_pTestClass->m_libswd_read_int_32_operation = operation;
    if (0x4001E400UL == addr)
    {
        if (0 == g_pTestClass->m_nvmc_reg_ready_cnt)
        {
            *data = 1;
        }
        else
        {
            g_pTestClass->m_nvmc_reg_ready_cnt -= 1;
            *data = 0;
        }
        if (0 == g_pTestClass->m_nvmc_reg_ready_cnt_before_fail)
        {
            return -1;
        }
        else
        {
            g_pTestClass->m_nvmc_reg_ready_cnt_before_fail -= 1;
            return 0;
        }
    }
    else
    {
        if (!g_pTestClass->read_mem((uint32_t)addr, (uint32_t)count, (uint32_t *)data))
        {
            return -1;
        }
        return g_pTestClass->m_libswd_read_int_32_result;
    }
}

int
libswd_memap_write_int_32(libswd_ctx_t *libswdctx, libswd_operation_t operation, int addr, int count, int *data)
{
    assert(&g_pTestClass->m_libswd_ctx == libswdctx);
    g_pTestClass->m_libswd_write_int_32_operation = operation;
    if (!g_pTestClass->write_mem((uint32_t)addr, (uint32_t)count, (uint32_t *)data))
    {
        return -1;
    }
    if (g_pTestClass->m_libswd_write_int_32_cnt_before_fail > 0)
    {
        if (0 == --g_pTestClass->m_libswd_write_int_32_cnt_before_fail)
        {
            return -1;
        }
    }
    return 0;
}

} // extern "C"

#define TEST_CHECK_LOG_RECORD(level_, msg_) ESP_LOG_WRAPPER_TEST_CHECK_LOG_RECORD("SWD", level_, msg_)

#define TEST_CHECK_LOG_RECORD_WITH_FUNC(level_, func_, msg_) \
    ESP_LOG_WRAPPER_TEST_CHECK_LOG_RECORD_WITH_FUNC("SWD", level_, func_, msg_)

/*** Unit-Tests
 * *******************************************************************************************************/

TEST_F(TestNRF52Swd, init_gpio_cfg_nreset_ok) // NOLINT
{
    ASSERT_TRUE(nrf52swd_init_gpio_cfg_nreset());
    ASSERT_EQ(1ULL << (unsigned)GPIO_NUM_17, this->m_gpio_config[GPIO_NUM_17].pin_bit_mask);
    ASSERT_EQ(GPIO_MODE_OUTPUT, this->m_gpio_config[GPIO_NUM_17].mode);
    ASSERT_EQ(1, this->m_gpio_config[GPIO_NUM_17].pull_up_en);
    ASSERT_EQ(0, this->m_gpio_config[GPIO_NUM_17].pull_down_en);
    ASSERT_EQ(GPIO_INTR_DISABLE, this->m_gpio_config[GPIO_NUM_17].intr_type);
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_TRUE(nrf52swd_deinit_gpio_cfg_nreset());
    ASSERT_TRUE(nrf52swd_deinit_gpio_cfg_nreset()); // try to deinit twice
}

TEST_F(TestNRF52Swd, init_gpio_cfg_nreset_twice) // NOLINT
{
    ASSERT_TRUE(nrf52swd_init_gpio_cfg_nreset());
    ASSERT_TRUE(nrf52swd_init_gpio_cfg_nreset());
    ASSERT_EQ(1ULL << (unsigned)GPIO_NUM_17, this->m_gpio_config[GPIO_NUM_17].pin_bit_mask);
    ASSERT_EQ(GPIO_MODE_OUTPUT, this->m_gpio_config[GPIO_NUM_17].mode);
    ASSERT_EQ(1, this->m_gpio_config[GPIO_NUM_17].pull_up_en);
    ASSERT_EQ(0, this->m_gpio_config[GPIO_NUM_17].pull_down_en);
    ASSERT_EQ(GPIO_INTR_DISABLE, this->m_gpio_config[GPIO_NUM_17].intr_type);
    ASSERT_TRUE(esp_log_wrapper_is_empty());
}

TEST_F(TestNRF52Swd, init_gpio_cfg_nreset_fail) // NOLINT
{
    this->m_gpio_config_result = ESP_FAIL;
    ASSERT_FALSE(nrf52swd_init_gpio_cfg_nreset());

    TEST_CHECK_LOG_RECORD_WITH_FUNC(
        ESP_LOG_ERROR,
        "nrf52swd_init_gpio_cfg_nreset",
        "gpio_config(nRF52 nRESET) failed, err=-1");
    ASSERT_TRUE(esp_log_wrapper_is_empty());
}

TEST_F(TestNRF52Swd, init_gpio_cfg_nreset_ok_deinit_fail) // NOLINT
{
    ASSERT_TRUE(nrf52swd_init_gpio_cfg_nreset());
    ASSERT_EQ(1ULL << (unsigned)GPIO_NUM_17, this->m_gpio_config[GPIO_NUM_17].pin_bit_mask);
    ASSERT_EQ(GPIO_MODE_OUTPUT, this->m_gpio_config[GPIO_NUM_17].mode);
    ASSERT_EQ(1, this->m_gpio_config[GPIO_NUM_17].pull_up_en);
    ASSERT_EQ(0, this->m_gpio_config[GPIO_NUM_17].pull_down_en);
    ASSERT_EQ(GPIO_INTR_DISABLE, this->m_gpio_config[GPIO_NUM_17].intr_type);
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    this->m_gpio_config_result = ESP_FAIL;
    ASSERT_FALSE(nrf52swd_deinit_gpio_cfg_nreset());
}

TEST_F(TestNRF52Swd, init_spi_init_ok) // NOLINT
{
    ASSERT_TRUE(nrf52swd_init_spi_init());

    ASSERT_EQ(HSPI_HOST, this->m_spi_host);
    ASSERT_EQ(0, this->m_spi_dma_chan);
    ASSERT_EQ(GPIO_NUM_16, this->m_spi_bus_config.mosi_io_num);
    ASSERT_EQ(GPIO_NUM_NC, this->m_spi_bus_config.miso_io_num);
    ASSERT_EQ(GPIO_NUM_15, this->m_spi_bus_config.sclk_io_num);
    ASSERT_EQ(GPIO_NUM_NC, this->m_spi_bus_config.quadwp_io_num);
    ASSERT_EQ(GPIO_NUM_NC, this->m_spi_bus_config.quadhd_io_num);
    ASSERT_EQ(0, this->m_spi_bus_config.max_transfer_sz);
    ASSERT_EQ(0, this->m_spi_bus_config.flags);
    ASSERT_EQ(0, this->m_spi_bus_config.intr_flags);

    TEST_CHECK_LOG_RECORD(ESP_LOG_DEBUG, "spi_bus_initialize");
    ASSERT_TRUE(esp_log_wrapper_is_empty());
}

TEST_F(TestNRF52Swd, init_spi_init_fail) // NOLINT
{
    this->m_spi_bus_initialize_result = ESP_FAIL;
    ASSERT_FALSE(nrf52swd_init_spi_init());

    TEST_CHECK_LOG_RECORD(ESP_LOG_DEBUG, "spi_bus_initialize");
    TEST_CHECK_LOG_RECORD_WITH_FUNC(ESP_LOG_ERROR, "nrf52swd_init_spi_init", "spi_bus_initialize failed, err=-1");
    ASSERT_TRUE(esp_log_wrapper_is_empty());
}

TEST_F(TestNRF52Swd, init_spi_add_device_ok) // NOLINT
{
    ASSERT_TRUE(nrf52swd_init_spi_add_device());

    ASSERT_EQ(HSPI_HOST, this->m_spi_add_device_host);
    ASSERT_EQ(0, this->m_spi_add_device_config.command_bits);
    ASSERT_EQ(0, this->m_spi_add_device_config.address_bits);
    ASSERT_EQ(0, this->m_spi_add_device_config.dummy_bits);
    ASSERT_EQ(0, this->m_spi_add_device_config.mode);
    ASSERT_EQ(0, this->m_spi_add_device_config.duty_cycle_pos);
    ASSERT_EQ(0, this->m_spi_add_device_config.cs_ena_pretrans);
    ASSERT_EQ(0, this->m_spi_add_device_config.cs_ena_posttrans);
    ASSERT_EQ(2000000, this->m_spi_add_device_config.clock_speed_hz);
    ASSERT_EQ(0, this->m_spi_add_device_config.input_delay_ns);
    ASSERT_EQ(-1, this->m_spi_add_device_config.spics_io_num);
    ASSERT_EQ(SPI_DEVICE_3WIRE | SPI_DEVICE_HALFDUPLEX | SPI_DEVICE_BIT_LSBFIRST, this->m_spi_add_device_config.flags);
    ASSERT_EQ(24, this->m_spi_add_device_config.queue_size);
    ASSERT_EQ(nullptr, this->m_spi_add_device_config.pre_cb);
    ASSERT_EQ(nullptr, this->m_spi_add_device_config.post_cb);

    TEST_CHECK_LOG_RECORD(ESP_LOG_DEBUG, "spi_bus_add_device");
    ASSERT_TRUE(esp_log_wrapper_is_empty());
}

TEST_F(TestNRF52Swd, init_spi_add_device_fail) // NOLINT
{
    this->m_spi_add_device_result = ESP_FAIL;
    ASSERT_FALSE(nrf52swd_init_spi_add_device());
    TEST_CHECK_LOG_RECORD(ESP_LOG_DEBUG, "spi_bus_add_device");
    TEST_CHECK_LOG_RECORD_WITH_FUNC(ESP_LOG_ERROR, "nrf52swd_init_spi_add_device", "spi_bus_add_device failed, err=-1");
    ASSERT_TRUE(esp_log_wrapper_is_empty());
}

TEST_F(TestNRF52Swd, init_ok_deinit_ok) // NOLINT
{
    ASSERT_TRUE(nrf52swd_init());

    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, "nRF52 SWD init");
    TEST_CHECK_LOG_RECORD(ESP_LOG_DEBUG, "spi_bus_initialize");
    TEST_CHECK_LOG_RECORD(ESP_LOG_DEBUG, "spi_bus_add_device");
    TEST_CHECK_LOG_RECORD(ESP_LOG_DEBUG, "libswd_init");
    TEST_CHECK_LOG_RECORD(ESP_LOG_DEBUG, "libswd_debug_init");
    TEST_CHECK_LOG_RECORD(ESP_LOG_DEBUG, "nrf52swd_init ok");
    ASSERT_NE(nullptr, this->m_libswd_ctx.driver);
    ASSERT_NE(nullptr, this->m_libswd_ctx.driver->device);
    ASSERT_EQ(LIBSWD_OPERATION_EXECUTE, this->m_libswd_debug_init_operation);
    ASSERT_TRUE(esp_log_wrapper_is_empty());

    nrf52swd_deinit();
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, "nRF52 SWD deinit");
    TEST_CHECK_LOG_RECORD(ESP_LOG_DEBUG, "libswd_deinit");
    TEST_CHECK_LOG_RECORD(ESP_LOG_DEBUG, "spi_bus_remove_device");
    TEST_CHECK_LOG_RECORD(ESP_LOG_DEBUG, "spi_bus_free");
    ASSERT_TRUE(esp_log_wrapper_is_empty());
}

TEST_F(TestNRF52Swd, init_ok_deinit_fail_at_spi_bus_remove_device) // NOLINT
{
    ASSERT_TRUE(nrf52swd_init());

    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, "nRF52 SWD init");
    TEST_CHECK_LOG_RECORD(ESP_LOG_DEBUG, "spi_bus_initialize");
    TEST_CHECK_LOG_RECORD(ESP_LOG_DEBUG, "spi_bus_add_device");
    TEST_CHECK_LOG_RECORD(ESP_LOG_DEBUG, "libswd_init");
    TEST_CHECK_LOG_RECORD(ESP_LOG_DEBUG, "libswd_debug_init");
    TEST_CHECK_LOG_RECORD(ESP_LOG_DEBUG, "nrf52swd_init ok");
    ASSERT_NE(nullptr, this->m_libswd_ctx.driver);
    ASSERT_NE(nullptr, this->m_libswd_ctx.driver->device);
    ASSERT_EQ(LIBSWD_OPERATION_EXECUTE, this->m_libswd_debug_init_operation);
    ASSERT_TRUE(esp_log_wrapper_is_empty());

    this->m_spi_remove_device_result = ESP_FAIL;
    nrf52swd_deinit();
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, "nRF52 SWD deinit");
    TEST_CHECK_LOG_RECORD(ESP_LOG_DEBUG, "libswd_deinit");
    TEST_CHECK_LOG_RECORD(ESP_LOG_DEBUG, "spi_bus_remove_device");
    TEST_CHECK_LOG_RECORD(ESP_LOG_ERROR, "nrf52swd_deinit: spi_bus_remove_device failed, err=-1");
    TEST_CHECK_LOG_RECORD(ESP_LOG_DEBUG, "spi_bus_free");
    ASSERT_TRUE(esp_log_wrapper_is_empty());
}

TEST_F(TestNRF52Swd, init_ok_deinit_fail_at_spi_bus_free) // NOLINT
{
    ASSERT_TRUE(nrf52swd_init());

    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, "nRF52 SWD init");
    TEST_CHECK_LOG_RECORD(ESP_LOG_DEBUG, "spi_bus_initialize");
    TEST_CHECK_LOG_RECORD(ESP_LOG_DEBUG, "spi_bus_add_device");
    TEST_CHECK_LOG_RECORD(ESP_LOG_DEBUG, "libswd_init");
    TEST_CHECK_LOG_RECORD(ESP_LOG_DEBUG, "libswd_debug_init");
    TEST_CHECK_LOG_RECORD(ESP_LOG_DEBUG, "nrf52swd_init ok");
    ASSERT_NE(nullptr, this->m_libswd_ctx.driver);
    ASSERT_NE(nullptr, this->m_libswd_ctx.driver->device);
    ASSERT_EQ(LIBSWD_OPERATION_EXECUTE, this->m_libswd_debug_init_operation);
    ASSERT_TRUE(esp_log_wrapper_is_empty());

    this->m_spi_bus_free_result = ESP_FAIL;
    nrf52swd_deinit();
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, "nRF52 SWD deinit");
    TEST_CHECK_LOG_RECORD(ESP_LOG_DEBUG, "libswd_deinit");
    TEST_CHECK_LOG_RECORD(ESP_LOG_DEBUG, "spi_bus_remove_device");
    TEST_CHECK_LOG_RECORD(ESP_LOG_DEBUG, "spi_bus_free");
    TEST_CHECK_LOG_RECORD(ESP_LOG_ERROR, "nrf52swd_deinit: spi_bus_free failed, err=-1");
    ASSERT_TRUE(esp_log_wrapper_is_empty());
}

TEST_F(TestNRF52Swd, init_failed_on_init_gpio_cfg_nreset) // NOLINT
{
    this->m_gpio_config_result = ESP_FAIL;
    ASSERT_FALSE(nrf52swd_init());

    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, "nRF52 SWD init");
    TEST_CHECK_LOG_RECORD_WITH_FUNC(
        ESP_LOG_ERROR,
        "nrf52swd_init_gpio_cfg_nreset",
        "gpio_config(nRF52 nRESET) failed, err=-1");
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, "nRF52 SWD deinit");
    ASSERT_TRUE(esp_log_wrapper_is_empty());
}

TEST_F(TestNRF52Swd, init_failed_on_init_spi) // NOLINT
{
    this->m_spi_bus_initialize_result = ESP_FAIL;
    ASSERT_FALSE(nrf52swd_init());

    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, "nRF52 SWD init");
    TEST_CHECK_LOG_RECORD(ESP_LOG_DEBUG, "spi_bus_initialize");
    TEST_CHECK_LOG_RECORD_WITH_FUNC(ESP_LOG_ERROR, "nrf52swd_init_spi_init", "spi_bus_initialize failed, err=-1");
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, "nRF52 SWD deinit");
    ASSERT_TRUE(esp_log_wrapper_is_empty());
}

TEST_F(TestNRF52Swd, init_failed_on_spi_add_device) // NOLINT
{
    this->m_spi_add_device_result = ESP_FAIL;
    ASSERT_FALSE(nrf52swd_init());

    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, "nRF52 SWD init");
    TEST_CHECK_LOG_RECORD(ESP_LOG_DEBUG, "spi_bus_initialize");
    TEST_CHECK_LOG_RECORD(ESP_LOG_DEBUG, "spi_bus_add_device");
    TEST_CHECK_LOG_RECORD_WITH_FUNC(ESP_LOG_ERROR, "nrf52swd_init_spi_add_device", "spi_bus_add_device failed, err=-1");
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, "nRF52 SWD deinit");
    TEST_CHECK_LOG_RECORD(ESP_LOG_DEBUG, "spi_bus_free");
    ASSERT_TRUE(esp_log_wrapper_is_empty());
}

TEST_F(TestNRF52Swd, init_failed_on_libswd_init) // NOLINT
{
    this->m_libswd_init_failed = true;
    ASSERT_FALSE(nrf52swd_init());

    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, "nRF52 SWD init");
    TEST_CHECK_LOG_RECORD(ESP_LOG_DEBUG, "spi_bus_initialize");
    TEST_CHECK_LOG_RECORD(ESP_LOG_DEBUG, "spi_bus_add_device");
    TEST_CHECK_LOG_RECORD(ESP_LOG_DEBUG, "libswd_init");
    TEST_CHECK_LOG_RECORD_WITH_FUNC(ESP_LOG_ERROR, "nrf52swd_init_internal", "libswd_init failed, err=-1");
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, "nRF52 SWD deinit");
    TEST_CHECK_LOG_RECORD(ESP_LOG_DEBUG, "spi_bus_remove_device");
    TEST_CHECK_LOG_RECORD(ESP_LOG_DEBUG, "spi_bus_free");
    ASSERT_TRUE(esp_log_wrapper_is_empty());
}

TEST_F(TestNRF52Swd, init_failed_on_libswd_debug_init) // NOLINT
{
    this->m_libswd_debug_init_result = -1;
    ASSERT_FALSE(nrf52swd_init());

    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, "nRF52 SWD init");
    TEST_CHECK_LOG_RECORD(ESP_LOG_DEBUG, "spi_bus_initialize");
    TEST_CHECK_LOG_RECORD(ESP_LOG_DEBUG, "spi_bus_add_device");
    TEST_CHECK_LOG_RECORD(ESP_LOG_DEBUG, "libswd_init");
    TEST_CHECK_LOG_RECORD(ESP_LOG_DEBUG, "libswd_debug_init");
    TEST_CHECK_LOG_RECORD_WITH_FUNC(ESP_LOG_ERROR, "nrf52swd_libswd_debug_init", "libswd_debug_init failed, err=-1");
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, "nRF52 SWD deinit");
    TEST_CHECK_LOG_RECORD(ESP_LOG_DEBUG, "libswd_deinit");
    TEST_CHECK_LOG_RECORD(ESP_LOG_DEBUG, "spi_bus_remove_device");
    TEST_CHECK_LOG_RECORD(ESP_LOG_DEBUG, "spi_bus_free");
    ASSERT_TRUE(esp_log_wrapper_is_empty());
}

TEST_F(TestNRF52Swd, nrf52swd_reset_ok) // NOLINT
{
    ASSERT_TRUE(nrf52swd_init_gpio_cfg_nreset());
    ASSERT_TRUE(esp_log_wrapper_is_empty());

    ASSERT_EQ(~0U, this->m_gpio_nrf52_nrst_level);
    ASSERT_TRUE(nrf52swd_reset(true));
    ASSERT_EQ(0U, this->m_gpio_nrf52_nrst_level);
    ASSERT_TRUE(nrf52swd_reset(false));
    ASSERT_EQ(1U, this->m_gpio_nrf52_nrst_level);

    ASSERT_TRUE(esp_log_wrapper_is_empty());
}

TEST_F(TestNRF52Swd, nrf52swd_reset_fail) // NOLINT
{
    ASSERT_TRUE(nrf52swd_init_gpio_cfg_nreset());
    ASSERT_TRUE(esp_log_wrapper_is_empty());

    this->m_gpio_set_level_result = ESP_FAIL;
    ASSERT_EQ(~0U, this->m_gpio_nrf52_nrst_level);
    ASSERT_FALSE(nrf52swd_reset(true));

    TEST_CHECK_LOG_RECORD_WITH_FUNC(ESP_LOG_ERROR, "nrf52swd_reset", "gpio_set_level failed, err=-1");
    ASSERT_TRUE(esp_log_wrapper_is_empty());
}

TEST_F(TestNRF52Swd, nrf52swd_check_id_code_ok) // NOLINT
{
    ASSERT_TRUE(nrf52swd_init());
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, "nRF52 SWD init");
    TEST_CHECK_LOG_RECORD(ESP_LOG_DEBUG, "spi_bus_initialize");
    TEST_CHECK_LOG_RECORD(ESP_LOG_DEBUG, "spi_bus_add_device");
    TEST_CHECK_LOG_RECORD(ESP_LOG_DEBUG, "libswd_init");
    TEST_CHECK_LOG_RECORD(ESP_LOG_DEBUG, "libswd_debug_init");
    TEST_CHECK_LOG_RECORD(ESP_LOG_DEBUG, "nrf52swd_init ok");
    ASSERT_TRUE(esp_log_wrapper_is_empty());

    ASSERT_TRUE(esp_log_wrapper_is_empty());

    this->m_libswd_dap_detect_idcode = 0x2ba01477;
    ASSERT_TRUE(nrf52swd_check_id_code());
    TEST_CHECK_LOG_RECORD(ESP_LOG_DEBUG, "libswd_dap_detect");
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, "IDCODE: 0x2ba01477");
    ASSERT_TRUE(esp_log_wrapper_is_empty());
}

TEST_F(TestNRF52Swd, nrf52swd_check_id_code_fail_wrong_id_code) // NOLINT
{
    ASSERT_TRUE(nrf52swd_init());
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, "nRF52 SWD init");
    TEST_CHECK_LOG_RECORD(ESP_LOG_DEBUG, "spi_bus_initialize");
    TEST_CHECK_LOG_RECORD(ESP_LOG_DEBUG, "spi_bus_add_device");
    TEST_CHECK_LOG_RECORD(ESP_LOG_DEBUG, "libswd_init");
    TEST_CHECK_LOG_RECORD(ESP_LOG_DEBUG, "libswd_debug_init");
    TEST_CHECK_LOG_RECORD(ESP_LOG_DEBUG, "nrf52swd_init ok");
    ASSERT_TRUE(esp_log_wrapper_is_empty());

    this->m_libswd_dap_detect_idcode = -1;
    ASSERT_FALSE(nrf52swd_check_id_code());
    TEST_CHECK_LOG_RECORD(ESP_LOG_DEBUG, "libswd_dap_detect");
    TEST_CHECK_LOG_RECORD(ESP_LOG_ERROR, "Wrong nRF52 ID code 0xffffffff (expected 0x2ba01477)");
    ASSERT_TRUE(esp_log_wrapper_is_empty());
}

TEST_F(TestNRF52Swd, nrf52swd_check_id_code_fail_dap_detect) // NOLINT
{
    ASSERT_TRUE(nrf52swd_init());
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, "nRF52 SWD init");
    TEST_CHECK_LOG_RECORD(ESP_LOG_DEBUG, "spi_bus_initialize");
    TEST_CHECK_LOG_RECORD(ESP_LOG_DEBUG, "spi_bus_add_device");
    TEST_CHECK_LOG_RECORD(ESP_LOG_DEBUG, "libswd_init");
    TEST_CHECK_LOG_RECORD(ESP_LOG_DEBUG, "libswd_debug_init");
    TEST_CHECK_LOG_RECORD(ESP_LOG_DEBUG, "nrf52swd_init ok");
    ASSERT_TRUE(esp_log_wrapper_is_empty());

    this->m_libswd_dap_detect_result = ESP_FAIL;
    ASSERT_FALSE(nrf52swd_check_id_code());
    TEST_CHECK_LOG_RECORD(ESP_LOG_DEBUG, "libswd_dap_detect");
    TEST_CHECK_LOG_RECORD_WITH_FUNC(ESP_LOG_ERROR, "nrf52swd_check_id_code", "libswd_dap_detect failed, err=-1");
    ASSERT_TRUE(esp_log_wrapper_is_empty());
}

TEST_F(TestNRF52Swd, nrf52swd_debug_halt_ok) // NOLINT
{
    ASSERT_TRUE(nrf52swd_init());
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, "nRF52 SWD init");
    TEST_CHECK_LOG_RECORD(ESP_LOG_DEBUG, "spi_bus_initialize");
    TEST_CHECK_LOG_RECORD(ESP_LOG_DEBUG, "spi_bus_add_device");
    TEST_CHECK_LOG_RECORD(ESP_LOG_DEBUG, "libswd_init");
    TEST_CHECK_LOG_RECORD(ESP_LOG_DEBUG, "libswd_debug_init");
    TEST_CHECK_LOG_RECORD(ESP_LOG_DEBUG, "nrf52swd_init ok");
    ASSERT_TRUE(esp_log_wrapper_is_empty());

    ASSERT_TRUE(nrf52swd_debug_halt());
    ASSERT_EQ(LIBSWD_OPERATION_EXECUTE, this->m_libswd_debug_halt_operation);
    ASSERT_TRUE(esp_log_wrapper_is_empty());
}

TEST_F(TestNRF52Swd, nrf52swd_debug_halt_failed) // NOLINT
{
    ASSERT_TRUE(nrf52swd_init());
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, "nRF52 SWD init");
    TEST_CHECK_LOG_RECORD(ESP_LOG_DEBUG, "spi_bus_initialize");
    TEST_CHECK_LOG_RECORD(ESP_LOG_DEBUG, "spi_bus_add_device");
    TEST_CHECK_LOG_RECORD(ESP_LOG_DEBUG, "libswd_init");
    TEST_CHECK_LOG_RECORD(ESP_LOG_DEBUG, "libswd_debug_init");
    TEST_CHECK_LOG_RECORD(ESP_LOG_DEBUG, "nrf52swd_init ok");
    ASSERT_TRUE(esp_log_wrapper_is_empty());

    this->m_libswd_debug_halt_result = -1;
    ASSERT_FALSE(nrf52swd_debug_halt());
    TEST_CHECK_LOG_RECORD_WITH_FUNC(ESP_LOG_ERROR, "nrf52swd_debug_halt", "libswd_debug_halt failed, err=-1");
    ASSERT_TRUE(esp_log_wrapper_is_empty());
}

TEST_F(TestNRF52Swd, nrf52swd_debug_enable_reset_vector_catch_ok) // NOLINT
{
    ASSERT_TRUE(nrf52swd_init());
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, "nRF52 SWD init");
    TEST_CHECK_LOG_RECORD(ESP_LOG_DEBUG, "spi_bus_initialize");
    TEST_CHECK_LOG_RECORD(ESP_LOG_DEBUG, "spi_bus_add_device");
    TEST_CHECK_LOG_RECORD(ESP_LOG_DEBUG, "libswd_init");
    TEST_CHECK_LOG_RECORD(ESP_LOG_DEBUG, "libswd_debug_init");
    TEST_CHECK_LOG_RECORD(ESP_LOG_DEBUG, "nrf52swd_init ok");
    ASSERT_TRUE(esp_log_wrapper_is_empty());

    ASSERT_TRUE(nrf52swd_debug_enable_reset_vector_catch());
    ASSERT_EQ(LIBSWD_OPERATION_EXECUTE, this->m_libswd_debug_enable_reset_vector_catch_operation);
    ASSERT_TRUE(esp_log_wrapper_is_empty());
}

TEST_F(TestNRF52Swd, nrf52swd_debug_enable_reset_vector_catch_failed) // NOLINT
{
    ASSERT_TRUE(nrf52swd_init());
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, "nRF52 SWD init");
    TEST_CHECK_LOG_RECORD(ESP_LOG_DEBUG, "spi_bus_initialize");
    TEST_CHECK_LOG_RECORD(ESP_LOG_DEBUG, "spi_bus_add_device");
    TEST_CHECK_LOG_RECORD(ESP_LOG_DEBUG, "libswd_init");
    TEST_CHECK_LOG_RECORD(ESP_LOG_DEBUG, "libswd_debug_init");
    TEST_CHECK_LOG_RECORD(ESP_LOG_DEBUG, "nrf52swd_init ok");
    ASSERT_TRUE(esp_log_wrapper_is_empty());

    this->m_libswd_debug_enable_reset_vector_catch_result = -1;
    ASSERT_FALSE(nrf52swd_debug_enable_reset_vector_catch());
    TEST_CHECK_LOG_RECORD_WITH_FUNC(
        ESP_LOG_ERROR,
        "nrf52swd_debug_enable_reset_vector_catch",
        "libswd_debug_enable_reset_vector_catch failed, err=-1");
    ASSERT_TRUE(esp_log_wrapper_is_empty());
}

TEST_F(TestNRF52Swd, nrf52swd_debug_reset_ok) // NOLINT
{
    ASSERT_TRUE(nrf52swd_init());
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, "nRF52 SWD init");
    TEST_CHECK_LOG_RECORD(ESP_LOG_DEBUG, "spi_bus_initialize");
    TEST_CHECK_LOG_RECORD(ESP_LOG_DEBUG, "spi_bus_add_device");
    TEST_CHECK_LOG_RECORD(ESP_LOG_DEBUG, "libswd_init");
    TEST_CHECK_LOG_RECORD(ESP_LOG_DEBUG, "libswd_debug_init");
    TEST_CHECK_LOG_RECORD(ESP_LOG_DEBUG, "nrf52swd_init ok");
    ASSERT_TRUE(esp_log_wrapper_is_empty());

    ASSERT_TRUE(nrf52swd_debug_reset());
    ASSERT_EQ(LIBSWD_OPERATION_EXECUTE, this->m_libswd_debug_reset_operation);
    ASSERT_TRUE(esp_log_wrapper_is_empty());
}

TEST_F(TestNRF52Swd, nrf52swd_debug_reset_failed) // NOLINT
{
    ASSERT_TRUE(nrf52swd_init());
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, "nRF52 SWD init");
    TEST_CHECK_LOG_RECORD(ESP_LOG_DEBUG, "spi_bus_initialize");
    TEST_CHECK_LOG_RECORD(ESP_LOG_DEBUG, "spi_bus_add_device");
    TEST_CHECK_LOG_RECORD(ESP_LOG_DEBUG, "libswd_init");
    TEST_CHECK_LOG_RECORD(ESP_LOG_DEBUG, "libswd_debug_init");
    TEST_CHECK_LOG_RECORD(ESP_LOG_DEBUG, "nrf52swd_init ok");
    ASSERT_TRUE(esp_log_wrapper_is_empty());

    this->m_libswd_debug_reset_result = -1;
    ASSERT_FALSE(nrf52swd_debug_reset());
    TEST_CHECK_LOG_RECORD_WITH_FUNC(ESP_LOG_ERROR, "nrf52swd_debug_reset", "libswd_debug_reset failed, err=-1");
    ASSERT_TRUE(esp_log_wrapper_is_empty());
}

TEST_F(TestNRF52Swd, nrf52swd_debug_run_ok) // NOLINT
{
    ASSERT_TRUE(nrf52swd_init());
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, "nRF52 SWD init");
    TEST_CHECK_LOG_RECORD(ESP_LOG_DEBUG, "spi_bus_initialize");
    TEST_CHECK_LOG_RECORD(ESP_LOG_DEBUG, "spi_bus_add_device");
    TEST_CHECK_LOG_RECORD(ESP_LOG_DEBUG, "libswd_init");
    TEST_CHECK_LOG_RECORD(ESP_LOG_DEBUG, "libswd_debug_init");
    TEST_CHECK_LOG_RECORD(ESP_LOG_DEBUG, "nrf52swd_init ok");
    ASSERT_TRUE(esp_log_wrapper_is_empty());

    ASSERT_TRUE(nrf52swd_debug_run());
    ASSERT_EQ(LIBSWD_OPERATION_EXECUTE, this->m_libswd_debug_run_operation);
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, "Run nRF52 firmware");
    ASSERT_TRUE(esp_log_wrapper_is_empty());
}

TEST_F(TestNRF52Swd, nrf52swd_debug_run_failed) // NOLINT
{
    ASSERT_TRUE(nrf52swd_init());
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, "nRF52 SWD init");
    TEST_CHECK_LOG_RECORD(ESP_LOG_DEBUG, "spi_bus_initialize");
    TEST_CHECK_LOG_RECORD(ESP_LOG_DEBUG, "spi_bus_add_device");
    TEST_CHECK_LOG_RECORD(ESP_LOG_DEBUG, "libswd_init");
    TEST_CHECK_LOG_RECORD(ESP_LOG_DEBUG, "libswd_debug_init");
    TEST_CHECK_LOG_RECORD(ESP_LOG_DEBUG, "nrf52swd_init ok");
    ASSERT_TRUE(esp_log_wrapper_is_empty());

    this->m_libswd_debug_run_result = -1;
    ASSERT_FALSE(nrf52swd_debug_run());
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, "Run nRF52 firmware");
    TEST_CHECK_LOG_RECORD_WITH_FUNC(ESP_LOG_ERROR, "nrf52swd_debug_run", "libswd_debug_run failed, err=-1");
    ASSERT_TRUE(esp_log_wrapper_is_empty());
}

TEST_F(TestNRF52Swd, nrf52swd_read_reg_ok) // NOLINT
{
    ASSERT_TRUE(nrf52swd_init());
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, "nRF52 SWD init");
    TEST_CHECK_LOG_RECORD(ESP_LOG_DEBUG, "spi_bus_initialize");
    TEST_CHECK_LOG_RECORD(ESP_LOG_DEBUG, "spi_bus_add_device");
    TEST_CHECK_LOG_RECORD(ESP_LOG_DEBUG, "libswd_init");
    TEST_CHECK_LOG_RECORD(ESP_LOG_DEBUG, "libswd_debug_init");
    TEST_CHECK_LOG_RECORD(ESP_LOG_DEBUG, "nrf52swd_init ok");
    ASSERT_TRUE(esp_log_wrapper_is_empty());

    const uint32_t reg_addr = 0x00010000U;
    const uint32_t exp_val  = 1234U;

    this->m_memSegmentsRead.emplace_back(reg_addr, 1, &exp_val);

    uint32_t val = 0;
    ASSERT_TRUE(nrf52swd_read_reg(reg_addr, &val));
    ASSERT_EQ(LIBSWD_OPERATION_EXECUTE, this->m_libswd_read_int_32_operation);
    ASSERT_EQ(exp_val, val);

    ASSERT_TRUE(esp_log_wrapper_is_empty());
}

TEST_F(TestNRF52Swd, nrf52swd_read_reg_fail) // NOLINT
{
    ASSERT_TRUE(nrf52swd_init());
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, "nRF52 SWD init");
    TEST_CHECK_LOG_RECORD(ESP_LOG_DEBUG, "spi_bus_initialize");
    TEST_CHECK_LOG_RECORD(ESP_LOG_DEBUG, "spi_bus_add_device");
    TEST_CHECK_LOG_RECORD(ESP_LOG_DEBUG, "libswd_init");
    TEST_CHECK_LOG_RECORD(ESP_LOG_DEBUG, "libswd_debug_init");
    TEST_CHECK_LOG_RECORD(ESP_LOG_DEBUG, "nrf52swd_init ok");
    ASSERT_TRUE(esp_log_wrapper_is_empty());

    const uint32_t reg_addr = 0x00010000U;
    const uint32_t exp_val  = 1234U;

    this->m_memSegmentsRead.emplace_back(reg_addr, 1, &exp_val);

    uint32_t val                      = 0;
    this->m_libswd_read_int_32_result = -1;
    ASSERT_FALSE(nrf52swd_read_reg(reg_addr, &val));
    ASSERT_EQ(LIBSWD_OPERATION_EXECUTE, this->m_libswd_read_int_32_operation);

    TEST_CHECK_LOG_RECORD(ESP_LOG_ERROR, "nrf52swd_read_reg: libswd_memap_read_int_32(0x00010000) failed, err=-1");
    ASSERT_TRUE(esp_log_wrapper_is_empty());
}

TEST_F(TestNRF52Swd, nrf52swd_write_reg_ok) // NOLINT
{
    ASSERT_TRUE(nrf52swd_init());
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, "nRF52 SWD init");
    TEST_CHECK_LOG_RECORD(ESP_LOG_DEBUG, "spi_bus_initialize");
    TEST_CHECK_LOG_RECORD(ESP_LOG_DEBUG, "spi_bus_add_device");
    TEST_CHECK_LOG_RECORD(ESP_LOG_DEBUG, "libswd_init");
    TEST_CHECK_LOG_RECORD(ESP_LOG_DEBUG, "libswd_debug_init");
    TEST_CHECK_LOG_RECORD(ESP_LOG_DEBUG, "nrf52swd_init ok");
    ASSERT_TRUE(esp_log_wrapper_is_empty());

    const uint32_t reg_addr = 0x00010000U;
    const uint32_t val      = 1234U;

    ASSERT_TRUE(nrf52swd_write_reg(reg_addr, val));
    ASSERT_EQ(LIBSWD_OPERATION_EXECUTE, this->m_libswd_write_int_32_operation);
    ASSERT_EQ(1, this->m_memSegmentsWrite.size());
    {
        ASSERT_EQ(reg_addr, this->m_memSegmentsWrite[0].segmentAddr);
        ASSERT_EQ(1, this->m_memSegmentsWrite[0].data.size());
        ASSERT_EQ(val, this->m_memSegmentsWrite[0].data[0]);
    }

    ASSERT_TRUE(esp_log_wrapper_is_empty());
}

TEST_F(TestNRF52Swd, nrf52swd_write_reg_fail) // NOLINT
{
    ASSERT_TRUE(nrf52swd_init());
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, "nRF52 SWD init");
    TEST_CHECK_LOG_RECORD(ESP_LOG_DEBUG, "spi_bus_initialize");
    TEST_CHECK_LOG_RECORD(ESP_LOG_DEBUG, "spi_bus_add_device");
    TEST_CHECK_LOG_RECORD(ESP_LOG_DEBUG, "libswd_init");
    TEST_CHECK_LOG_RECORD(ESP_LOG_DEBUG, "libswd_debug_init");
    TEST_CHECK_LOG_RECORD(ESP_LOG_DEBUG, "nrf52swd_init ok");
    ASSERT_TRUE(esp_log_wrapper_is_empty());

    const uint32_t reg_addr = 0x00010000U;
    const uint32_t val      = 1234U;

    this->m_libswd_write_int_32_cnt_before_fail = 1;
    ASSERT_FALSE(nrf52swd_write_reg(reg_addr, val));
    ASSERT_EQ(LIBSWD_OPERATION_EXECUTE, this->m_libswd_write_int_32_operation);

    TEST_CHECK_LOG_RECORD(ESP_LOG_ERROR, "nrf52swd_write_reg: libswd_memap_write_int_32(0x00010000) failed, err=-1");
    ASSERT_TRUE(esp_log_wrapper_is_empty());
}

TEST_F(TestNRF52Swd, nrf52swd_nvmc_is_ready_or_err_ok_ready) // NOLINT
{
    ASSERT_TRUE(nrf52swd_init());
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, "nRF52 SWD init");
    TEST_CHECK_LOG_RECORD(ESP_LOG_DEBUG, "spi_bus_initialize");
    TEST_CHECK_LOG_RECORD(ESP_LOG_DEBUG, "spi_bus_add_device");
    TEST_CHECK_LOG_RECORD(ESP_LOG_DEBUG, "libswd_init");
    TEST_CHECK_LOG_RECORD(ESP_LOG_DEBUG, "libswd_debug_init");
    TEST_CHECK_LOG_RECORD(ESP_LOG_DEBUG, "nrf52swd_init ok");
    ASSERT_TRUE(esp_log_wrapper_is_empty());

    this->m_nvmc_reg_ready_cnt             = 0U;
    this->m_nvmc_reg_ready_cnt_before_fail = 1;

    bool result = false;
    ASSERT_TRUE(nrf51swd_nvmc_is_ready_or_err(&result));
    ASSERT_EQ(LIBSWD_OPERATION_EXECUTE, this->m_libswd_read_int_32_operation);
    ASSERT_TRUE(result);

    ASSERT_TRUE(esp_log_wrapper_is_empty());
}

TEST_F(TestNRF52Swd, nrf52swd_nvmc_is_ready_or_err_ok_not_ready) // NOLINT
{
    ASSERT_TRUE(nrf52swd_init());
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, "nRF52 SWD init");
    TEST_CHECK_LOG_RECORD(ESP_LOG_DEBUG, "spi_bus_initialize");
    TEST_CHECK_LOG_RECORD(ESP_LOG_DEBUG, "spi_bus_add_device");
    TEST_CHECK_LOG_RECORD(ESP_LOG_DEBUG, "libswd_init");
    TEST_CHECK_LOG_RECORD(ESP_LOG_DEBUG, "libswd_debug_init");
    TEST_CHECK_LOG_RECORD(ESP_LOG_DEBUG, "nrf52swd_init ok");
    ASSERT_TRUE(esp_log_wrapper_is_empty());

    this->m_nvmc_reg_ready_cnt             = 1U;
    this->m_nvmc_reg_ready_cnt_before_fail = 2;

    bool result = false;
    ASSERT_FALSE(nrf51swd_nvmc_is_ready_or_err(&result));
    ASSERT_EQ(LIBSWD_OPERATION_EXECUTE, this->m_libswd_read_int_32_operation);

    ASSERT_TRUE(esp_log_wrapper_is_empty());
}

TEST_F(TestNRF52Swd, nrf52swd_nvmc_is_ready_or_err_fail) // NOLINT
{
    ASSERT_TRUE(nrf52swd_init());
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, "nRF52 SWD init");
    TEST_CHECK_LOG_RECORD(ESP_LOG_DEBUG, "spi_bus_initialize");
    TEST_CHECK_LOG_RECORD(ESP_LOG_DEBUG, "spi_bus_add_device");
    TEST_CHECK_LOG_RECORD(ESP_LOG_DEBUG, "libswd_init");
    TEST_CHECK_LOG_RECORD(ESP_LOG_DEBUG, "libswd_debug_init");
    TEST_CHECK_LOG_RECORD(ESP_LOG_DEBUG, "nrf52swd_init ok");
    ASSERT_TRUE(esp_log_wrapper_is_empty());

    this->m_nvmc_reg_ready_cnt             = 0U;
    this->m_nvmc_reg_ready_cnt_before_fail = 0;

    bool result = false;
    ASSERT_TRUE(nrf51swd_nvmc_is_ready_or_err(&result));
    ASSERT_EQ(LIBSWD_OPERATION_EXECUTE, this->m_libswd_read_int_32_operation);
    ASSERT_FALSE(result);

    TEST_CHECK_LOG_RECORD(ESP_LOG_ERROR, "nrf52swd_read_reg: libswd_memap_read_int_32(0x4001e400) failed, err=-1");
    TEST_CHECK_LOG_RECORD_WITH_FUNC(
        ESP_LOG_ERROR,
        "nrf51swd_nvmc_is_ready_or_err",
        "nrf52swd_read_reg(REG_READY) failed, err=-1");
    ASSERT_TRUE(esp_log_wrapper_is_empty());
}

TEST_F(TestNRF52Swd, nrf51swd_nvmc_wait_while_busy_ok_0) // NOLINT
{
    ASSERT_TRUE(nrf52swd_init());
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, "nRF52 SWD init");
    TEST_CHECK_LOG_RECORD(ESP_LOG_DEBUG, "spi_bus_initialize");
    TEST_CHECK_LOG_RECORD(ESP_LOG_DEBUG, "spi_bus_add_device");
    TEST_CHECK_LOG_RECORD(ESP_LOG_DEBUG, "libswd_init");
    TEST_CHECK_LOG_RECORD(ESP_LOG_DEBUG, "libswd_debug_init");
    TEST_CHECK_LOG_RECORD(ESP_LOG_DEBUG, "nrf52swd_init ok");
    ASSERT_TRUE(esp_log_wrapper_is_empty());

    this->m_nvmc_reg_ready_cnt             = 0U;
    this->m_nvmc_reg_ready_cnt_before_fail = 1;

    ASSERT_TRUE(nrf51swd_nvmc_wait_while_busy());
    ASSERT_EQ(0, this->m_vTaskDelay_cnt);

    ASSERT_TRUE(esp_log_wrapper_is_empty());
}

TEST_F(TestNRF52Swd, nrf51swd_nvmc_wait_while_busy_ok_5) // NOLINT
{
    ASSERT_TRUE(nrf52swd_init());
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, "nRF52 SWD init");
    TEST_CHECK_LOG_RECORD(ESP_LOG_DEBUG, "spi_bus_initialize");
    TEST_CHECK_LOG_RECORD(ESP_LOG_DEBUG, "spi_bus_add_device");
    TEST_CHECK_LOG_RECORD(ESP_LOG_DEBUG, "libswd_init");
    TEST_CHECK_LOG_RECORD(ESP_LOG_DEBUG, "libswd_debug_init");
    TEST_CHECK_LOG_RECORD(ESP_LOG_DEBUG, "nrf52swd_init ok");
    ASSERT_TRUE(esp_log_wrapper_is_empty());

    this->m_nvmc_reg_ready_cnt             = 5U;
    this->m_nvmc_reg_ready_cnt_before_fail = 6;

    ASSERT_TRUE(nrf51swd_nvmc_wait_while_busy());
    ASSERT_EQ(5, this->m_vTaskDelay_cnt);

    ASSERT_TRUE(esp_log_wrapper_is_empty());
}

TEST_F(TestNRF52Swd, nrf51swd_nvmc_wait_while_busy_fail_3) // NOLINT
{
    ASSERT_TRUE(nrf52swd_init());
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, "nRF52 SWD init");
    TEST_CHECK_LOG_RECORD(ESP_LOG_DEBUG, "spi_bus_initialize");
    TEST_CHECK_LOG_RECORD(ESP_LOG_DEBUG, "spi_bus_add_device");
    TEST_CHECK_LOG_RECORD(ESP_LOG_DEBUG, "libswd_init");
    TEST_CHECK_LOG_RECORD(ESP_LOG_DEBUG, "libswd_debug_init");
    TEST_CHECK_LOG_RECORD(ESP_LOG_DEBUG, "nrf52swd_init ok");
    ASSERT_TRUE(esp_log_wrapper_is_empty());

    this->m_nvmc_reg_ready_cnt             = 3U;
    this->m_nvmc_reg_ready_cnt_before_fail = 3;

    ASSERT_FALSE(nrf51swd_nvmc_wait_while_busy());
    ASSERT_EQ(3, this->m_vTaskDelay_cnt);

    TEST_CHECK_LOG_RECORD(ESP_LOG_ERROR, "nrf52swd_read_reg: libswd_memap_read_int_32(0x4001e400) failed, err=-1");
    TEST_CHECK_LOG_RECORD_WITH_FUNC(
        ESP_LOG_ERROR,
        "nrf51swd_nvmc_is_ready_or_err",
        "nrf52swd_read_reg(REG_READY) failed, err=-1");
    ASSERT_TRUE(esp_log_wrapper_is_empty());
}

TEST_F(TestNRF52Swd, nrf52swd_nrf52swd_erase_all_ok) // NOLINT
{
    ASSERT_TRUE(nrf52swd_init());
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, "nRF52 SWD init");
    TEST_CHECK_LOG_RECORD(ESP_LOG_DEBUG, "spi_bus_initialize");
    TEST_CHECK_LOG_RECORD(ESP_LOG_DEBUG, "spi_bus_add_device");
    TEST_CHECK_LOG_RECORD(ESP_LOG_DEBUG, "libswd_init");
    TEST_CHECK_LOG_RECORD(ESP_LOG_DEBUG, "libswd_debug_init");
    TEST_CHECK_LOG_RECORD(ESP_LOG_DEBUG, "nrf52swd_init ok");
    ASSERT_TRUE(esp_log_wrapper_is_empty());

    this->m_nvmc_reg_ready_cnt             = 0U;
    this->m_nvmc_reg_ready_cnt_before_fail = 2;

    ASSERT_TRUE(nrf52swd_erase_all());
    ASSERT_EQ(3, this->m_memSegmentsWrite.size());
    {
        ASSERT_EQ(0x4001E000UL + 0x504U, this->m_memSegmentsWrite[0].segmentAddr);
        ASSERT_EQ(1, this->m_memSegmentsWrite[0].data.size());
        ASSERT_EQ(2U, this->m_memSegmentsWrite[0].data[0]);
    }
    {
        ASSERT_EQ(0x4001E000UL + 0x50CU, this->m_memSegmentsWrite[1].segmentAddr);
        ASSERT_EQ(1, this->m_memSegmentsWrite[1].data.size());
        ASSERT_EQ(1U, this->m_memSegmentsWrite[1].data[0]);
    }
    {
        ASSERT_EQ(0x4001E000UL + 0x504U, this->m_memSegmentsWrite[2].segmentAddr);
        ASSERT_EQ(1, this->m_memSegmentsWrite[2].data.size());
        ASSERT_EQ(0U, this->m_memSegmentsWrite[2].data[0]);
    }

    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, "nRF52: Erase all flash");
    ASSERT_TRUE(esp_log_wrapper_is_empty());
}

TEST_F(TestNRF52Swd, nrf52swd_nrf52swd_erase_all_fail_on_first_wait) // NOLINT
{
    ASSERT_TRUE(nrf52swd_init());
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, "nRF52 SWD init");
    TEST_CHECK_LOG_RECORD(ESP_LOG_DEBUG, "spi_bus_initialize");
    TEST_CHECK_LOG_RECORD(ESP_LOG_DEBUG, "spi_bus_add_device");
    TEST_CHECK_LOG_RECORD(ESP_LOG_DEBUG, "libswd_init");
    TEST_CHECK_LOG_RECORD(ESP_LOG_DEBUG, "libswd_debug_init");
    TEST_CHECK_LOG_RECORD(ESP_LOG_DEBUG, "nrf52swd_init ok");
    ASSERT_TRUE(esp_log_wrapper_is_empty());

    this->m_nvmc_reg_ready_cnt             = 0U;
    this->m_nvmc_reg_ready_cnt_before_fail = 0;

    ASSERT_FALSE(nrf52swd_erase_all());
    ASSERT_EQ(0, this->m_memSegmentsWrite.size());

    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, "nRF52: Erase all flash");
    TEST_CHECK_LOG_RECORD(ESP_LOG_ERROR, "nrf52swd_read_reg: libswd_memap_read_int_32(0x4001e400) failed, err=-1");
    TEST_CHECK_LOG_RECORD_WITH_FUNC(
        ESP_LOG_ERROR,
        "nrf51swd_nvmc_is_ready_or_err",
        "nrf52swd_read_reg(REG_READY) failed, err=-1");
    TEST_CHECK_LOG_RECORD_WITH_FUNC(
        ESP_LOG_ERROR,
        "nrf52swd_erase_all",
        "nrf51swd_nvmc_wait_while_busy failed, err=-1");
    ASSERT_TRUE(esp_log_wrapper_is_empty());
}

TEST_F(TestNRF52Swd, nrf52swd_nrf52swd_erase_all_fail_on_second_wait) // NOLINT
{
    ASSERT_TRUE(nrf52swd_init());
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, "nRF52 SWD init");
    TEST_CHECK_LOG_RECORD(ESP_LOG_DEBUG, "spi_bus_initialize");
    TEST_CHECK_LOG_RECORD(ESP_LOG_DEBUG, "spi_bus_add_device");
    TEST_CHECK_LOG_RECORD(ESP_LOG_DEBUG, "libswd_init");
    TEST_CHECK_LOG_RECORD(ESP_LOG_DEBUG, "libswd_debug_init");
    TEST_CHECK_LOG_RECORD(ESP_LOG_DEBUG, "nrf52swd_init ok");
    ASSERT_TRUE(esp_log_wrapper_is_empty());

    this->m_nvmc_reg_ready_cnt             = 0U;
    this->m_nvmc_reg_ready_cnt_before_fail = 1;

    ASSERT_FALSE(nrf52swd_erase_all());
    ASSERT_EQ(2, this->m_memSegmentsWrite.size());
    {
        ASSERT_EQ(0x4001E000UL + 0x504U, this->m_memSegmentsWrite[0].segmentAddr);
        ASSERT_EQ(1, this->m_memSegmentsWrite[0].data.size());
        ASSERT_EQ(2U, this->m_memSegmentsWrite[0].data[0]);
    }
    {
        ASSERT_EQ(0x4001E000UL + 0x50CU, this->m_memSegmentsWrite[1].segmentAddr);
        ASSERT_EQ(1, this->m_memSegmentsWrite[1].data.size());
        ASSERT_EQ(1U, this->m_memSegmentsWrite[1].data[0]);
    }

    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, "nRF52: Erase all flash");
    TEST_CHECK_LOG_RECORD(ESP_LOG_ERROR, "nrf52swd_read_reg: libswd_memap_read_int_32(0x4001e400) failed, err=-1");
    TEST_CHECK_LOG_RECORD_WITH_FUNC(
        ESP_LOG_ERROR,
        "nrf51swd_nvmc_is_ready_or_err",
        "nrf52swd_read_reg(REG_READY) failed, err=-1");
    TEST_CHECK_LOG_RECORD_WITH_FUNC(
        ESP_LOG_ERROR,
        "nrf52swd_erase_all",
        "nrf51swd_nvmc_wait_while_busy failed, err=-1");
    ASSERT_TRUE(esp_log_wrapper_is_empty());
}

TEST_F(TestNRF52Swd, nrf52swd_nrf52swd_erase_all_fail_on_write_reg_config_wen_een) // NOLINT
{
    ASSERT_TRUE(nrf52swd_init());
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, "nRF52 SWD init");
    TEST_CHECK_LOG_RECORD(ESP_LOG_DEBUG, "spi_bus_initialize");
    TEST_CHECK_LOG_RECORD(ESP_LOG_DEBUG, "spi_bus_add_device");
    TEST_CHECK_LOG_RECORD(ESP_LOG_DEBUG, "libswd_init");
    TEST_CHECK_LOG_RECORD(ESP_LOG_DEBUG, "libswd_debug_init");
    TEST_CHECK_LOG_RECORD(ESP_LOG_DEBUG, "nrf52swd_init ok");
    ASSERT_TRUE(esp_log_wrapper_is_empty());

    this->m_nvmc_reg_ready_cnt              = 0U;
    this->m_nvmc_reg_ready_cnt_before_fail  = 2;
    this->m_nvmc_reg_config_cnt_before_fail = 1;

    ASSERT_FALSE(nrf52swd_erase_all());
    ASSERT_EQ(1, this->m_memSegmentsWrite.size());
    {
        ASSERT_EQ(0x4001E000UL + 0x504U, this->m_memSegmentsWrite[0].segmentAddr);
        ASSERT_EQ(1, this->m_memSegmentsWrite[0].data.size());
        ASSERT_EQ(2U, this->m_memSegmentsWrite[0].data[0]);
    }

    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, "nRF52: Erase all flash");
    TEST_CHECK_LOG_RECORD(ESP_LOG_ERROR, "nrf52swd_write_reg: libswd_memap_write_int_32(0x4001e504) failed, err=-1");
    TEST_CHECK_LOG_RECORD_WITH_FUNC(
        ESP_LOG_ERROR,
        "nrf52swd_erase_all",
        "nrf52swd_write_reg(REG_CONFIG):=EEN failed, err=-1");
    ASSERT_TRUE(esp_log_wrapper_is_empty());
}

TEST_F(TestNRF52Swd, nrf52swd_nrf52swd_erase_all_fail_on_write_reg_config_wen_ren) // NOLINT
{
    ASSERT_TRUE(nrf52swd_init());
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, "nRF52 SWD init");
    TEST_CHECK_LOG_RECORD(ESP_LOG_DEBUG, "spi_bus_initialize");
    TEST_CHECK_LOG_RECORD(ESP_LOG_DEBUG, "spi_bus_add_device");
    TEST_CHECK_LOG_RECORD(ESP_LOG_DEBUG, "libswd_init");
    TEST_CHECK_LOG_RECORD(ESP_LOG_DEBUG, "libswd_debug_init");
    TEST_CHECK_LOG_RECORD(ESP_LOG_DEBUG, "nrf52swd_init ok");
    ASSERT_TRUE(esp_log_wrapper_is_empty());

    this->m_nvmc_reg_ready_cnt              = 0U;
    this->m_nvmc_reg_ready_cnt_before_fail  = 2;
    this->m_nvmc_reg_config_cnt_before_fail = 2;

    ASSERT_FALSE(nrf52swd_erase_all());
    ASSERT_EQ(3, this->m_memSegmentsWrite.size());
    {
        ASSERT_EQ(0x4001E000UL + 0x504U, this->m_memSegmentsWrite[0].segmentAddr);
        ASSERT_EQ(1, this->m_memSegmentsWrite[0].data.size());
        ASSERT_EQ(2U, this->m_memSegmentsWrite[0].data[0]);
    }
    {
        ASSERT_EQ(0x4001E000UL + 0x50CU, this->m_memSegmentsWrite[1].segmentAddr);
        ASSERT_EQ(1, this->m_memSegmentsWrite[1].data.size());
        ASSERT_EQ(1U, this->m_memSegmentsWrite[1].data[0]);
    }
    {
        ASSERT_EQ(0x4001E000UL + 0x504U, this->m_memSegmentsWrite[2].segmentAddr);
        ASSERT_EQ(1, this->m_memSegmentsWrite[2].data.size());
        ASSERT_EQ(0U, this->m_memSegmentsWrite[2].data[0]);
    }

    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, "nRF52: Erase all flash");
    TEST_CHECK_LOG_RECORD(ESP_LOG_ERROR, "nrf52swd_write_reg: libswd_memap_write_int_32(0x4001e504) failed, err=-1");
    TEST_CHECK_LOG_RECORD_WITH_FUNC(
        ESP_LOG_ERROR,
        "nrf52swd_erase_all",
        "nrf52swd_write_reg(REG_CONFIG):=REN failed, err=-1");
    ASSERT_TRUE(esp_log_wrapper_is_empty());
}

TEST_F(TestNRF52Swd, nrf52swd_nrf52swd_erase_all_fail_on_write_reg_erase_all) // NOLINT
{
    ASSERT_TRUE(nrf52swd_init());
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, "nRF52 SWD init");
    TEST_CHECK_LOG_RECORD(ESP_LOG_DEBUG, "spi_bus_initialize");
    TEST_CHECK_LOG_RECORD(ESP_LOG_DEBUG, "spi_bus_add_device");
    TEST_CHECK_LOG_RECORD(ESP_LOG_DEBUG, "libswd_init");
    TEST_CHECK_LOG_RECORD(ESP_LOG_DEBUG, "libswd_debug_init");
    TEST_CHECK_LOG_RECORD(ESP_LOG_DEBUG, "nrf52swd_init ok");
    ASSERT_TRUE(esp_log_wrapper_is_empty());

    this->m_nvmc_reg_ready_cnt                 = 0U;
    this->m_nvmc_reg_ready_cnt_before_fail     = 2;
    this->m_nvmc_reg_erase_all_cnt_before_fail = 1;

    ASSERT_FALSE(nrf52swd_erase_all());
    ASSERT_EQ(2, this->m_memSegmentsWrite.size());
    {
        ASSERT_EQ(0x4001E000UL + 0x504U, this->m_memSegmentsWrite[0].segmentAddr);
        ASSERT_EQ(1, this->m_memSegmentsWrite[0].data.size());
        ASSERT_EQ(2U, this->m_memSegmentsWrite[0].data[0]);
    }
    {
        ASSERT_EQ(0x4001E000UL + 0x50CU, this->m_memSegmentsWrite[1].segmentAddr);
        ASSERT_EQ(1, this->m_memSegmentsWrite[1].data.size());
        ASSERT_EQ(1U, this->m_memSegmentsWrite[1].data[0]);
    }

    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, "nRF52: Erase all flash");
    TEST_CHECK_LOG_RECORD(ESP_LOG_ERROR, "nrf52swd_write_reg: libswd_memap_write_int_32(0x4001e50c) failed, err=-1");
    TEST_CHECK_LOG_RECORD_WITH_FUNC(
        ESP_LOG_ERROR,
        "nrf52swd_erase_all",
        "nrf52swd_write_reg(REG_ERASEALL) failed, err=-1");
    ASSERT_TRUE(esp_log_wrapper_is_empty());
}

TEST_F(TestNRF52Swd, nrf52swd_read_mem_ok) // NOLINT
{
    ASSERT_TRUE(nrf52swd_init());
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, "nRF52 SWD init");
    TEST_CHECK_LOG_RECORD(ESP_LOG_DEBUG, "spi_bus_initialize");
    TEST_CHECK_LOG_RECORD(ESP_LOG_DEBUG, "spi_bus_add_device");
    TEST_CHECK_LOG_RECORD(ESP_LOG_DEBUG, "libswd_init");
    TEST_CHECK_LOG_RECORD(ESP_LOG_DEBUG, "libswd_debug_init");
    TEST_CHECK_LOG_RECORD(ESP_LOG_DEBUG, "nrf52swd_init ok");
    ASSERT_TRUE(esp_log_wrapper_is_empty());

    const uint32_t reg_addr    = 0x00010000U;
    const uint32_t exp_vals[5] = { 5, 6, 7, 8, 9 };

    this->m_memSegmentsRead.emplace_back(reg_addr, sizeof(exp_vals) / sizeof(exp_vals[0]), exp_vals);

    uint32_t arr_of_vals[5] = { 0 };
    ASSERT_TRUE(nrf52swd_read_mem(reg_addr, sizeof(arr_of_vals) / sizeof(arr_of_vals[0]), arr_of_vals));
    ASSERT_EQ(LIBSWD_OPERATION_EXECUTE, this->m_libswd_read_int_32_operation);
    ASSERT_EQ(exp_vals[0], arr_of_vals[0]);
    ASSERT_EQ(exp_vals[1], arr_of_vals[1]);
    ASSERT_EQ(exp_vals[2], arr_of_vals[2]);
    ASSERT_EQ(exp_vals[3], arr_of_vals[3]);
    ASSERT_EQ(exp_vals[4], arr_of_vals[4]);

    ASSERT_TRUE(esp_log_wrapper_is_empty());
}

TEST_F(TestNRF52Swd, nrf52swd_read_mem_fail) // NOLINT
{
    ASSERT_TRUE(nrf52swd_init());
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, "nRF52 SWD init");
    TEST_CHECK_LOG_RECORD(ESP_LOG_DEBUG, "spi_bus_initialize");
    TEST_CHECK_LOG_RECORD(ESP_LOG_DEBUG, "spi_bus_add_device");
    TEST_CHECK_LOG_RECORD(ESP_LOG_DEBUG, "libswd_init");
    TEST_CHECK_LOG_RECORD(ESP_LOG_DEBUG, "libswd_debug_init");
    TEST_CHECK_LOG_RECORD(ESP_LOG_DEBUG, "nrf52swd_init ok");
    ASSERT_TRUE(esp_log_wrapper_is_empty());

    const uint32_t reg_addr    = 0x00010000U;
    const uint32_t exp_vals[5] = { 5, 6, 7, 8, 9 };

    this->m_memSegmentsRead.emplace_back(reg_addr, sizeof(exp_vals) / sizeof(exp_vals[0]), exp_vals);

    this->m_libswd_read_int_32_result = -1;
    uint32_t arr_of_vals[5]           = { 0 };
    ASSERT_FALSE(nrf52swd_read_mem(reg_addr, sizeof(arr_of_vals) / sizeof(arr_of_vals[0]), arr_of_vals));
    ASSERT_EQ(LIBSWD_OPERATION_EXECUTE, this->m_libswd_read_int_32_operation);

    TEST_CHECK_LOG_RECORD_WITH_FUNC(ESP_LOG_ERROR, "nrf52swd_read_mem", "libswd_memap_read_int_32 failed, err=-1");
    ASSERT_TRUE(esp_log_wrapper_is_empty());
}

TEST_F(TestNRF52Swd, nrf52swd_write_mem_ok) // NOLINT
{
    ASSERT_TRUE(nrf52swd_init());
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, "nRF52 SWD init");
    TEST_CHECK_LOG_RECORD(ESP_LOG_DEBUG, "spi_bus_initialize");
    TEST_CHECK_LOG_RECORD(ESP_LOG_DEBUG, "spi_bus_add_device");
    TEST_CHECK_LOG_RECORD(ESP_LOG_DEBUG, "libswd_init");
    TEST_CHECK_LOG_RECORD(ESP_LOG_DEBUG, "libswd_debug_init");
    TEST_CHECK_LOG_RECORD(ESP_LOG_DEBUG, "nrf52swd_init ok");
    ASSERT_TRUE(esp_log_wrapper_is_empty());

    this->m_nvmc_reg_ready_cnt             = 0U;
    this->m_nvmc_reg_ready_cnt_before_fail = 2;

    const uint32_t reg_addr       = 0x00010000U;
    const uint32_t arr_of_vals[5] = { 5, 6, 7, 8, 9 };

    ASSERT_TRUE(nrf52swd_write_mem(reg_addr, sizeof(arr_of_vals) / sizeof(arr_of_vals[0]), arr_of_vals));
    ASSERT_EQ(3, this->m_memSegmentsWrite.size());
    {
        ASSERT_EQ(0x4001E000UL + 0x504U, this->m_memSegmentsWrite[0].segmentAddr);
        ASSERT_EQ(1, this->m_memSegmentsWrite[0].data.size());
        ASSERT_EQ(1U, this->m_memSegmentsWrite[0].data[0]);
    }
    {
        ASSERT_EQ(reg_addr, this->m_memSegmentsWrite[1].segmentAddr);
        ASSERT_EQ(sizeof(arr_of_vals) / sizeof(arr_of_vals[0]), this->m_memSegmentsWrite[1].data.size());
        ASSERT_EQ(arr_of_vals[0], this->m_memSegmentsWrite[1].data[0]);
        ASSERT_EQ(arr_of_vals[1], this->m_memSegmentsWrite[1].data[1]);
        ASSERT_EQ(arr_of_vals[2], this->m_memSegmentsWrite[1].data[2]);
        ASSERT_EQ(arr_of_vals[3], this->m_memSegmentsWrite[1].data[3]);
        ASSERT_EQ(arr_of_vals[4], this->m_memSegmentsWrite[1].data[4]);
    }
    {
        ASSERT_EQ(0x4001E000UL + 0x504U, this->m_memSegmentsWrite[2].segmentAddr);
        ASSERT_EQ(1, this->m_memSegmentsWrite[2].data.size());
        ASSERT_EQ(0U, this->m_memSegmentsWrite[2].data[0]);
    }

    ASSERT_TRUE(esp_log_wrapper_is_empty());
}

TEST_F(TestNRF52Swd, nrf52swd_nrf52swd_write_mem_fail_on_first_wait) // NOLINT
{
    ASSERT_TRUE(nrf52swd_init());
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, "nRF52 SWD init");
    TEST_CHECK_LOG_RECORD(ESP_LOG_DEBUG, "spi_bus_initialize");
    TEST_CHECK_LOG_RECORD(ESP_LOG_DEBUG, "spi_bus_add_device");
    TEST_CHECK_LOG_RECORD(ESP_LOG_DEBUG, "libswd_init");
    TEST_CHECK_LOG_RECORD(ESP_LOG_DEBUG, "libswd_debug_init");
    TEST_CHECK_LOG_RECORD(ESP_LOG_DEBUG, "nrf52swd_init ok");
    ASSERT_TRUE(esp_log_wrapper_is_empty());

    this->m_nvmc_reg_ready_cnt             = 0U;
    this->m_nvmc_reg_ready_cnt_before_fail = 0;

    const uint32_t reg_addr       = 0x00010000U;
    const uint32_t arr_of_vals[5] = { 5, 6, 7, 8, 9 };

    ASSERT_FALSE(nrf52swd_write_mem(reg_addr, sizeof(arr_of_vals) / sizeof(arr_of_vals[0]), arr_of_vals));
    ASSERT_EQ(0, this->m_memSegmentsWrite.size());

    TEST_CHECK_LOG_RECORD(ESP_LOG_ERROR, "nrf52swd_read_reg: libswd_memap_read_int_32(0x4001e400) failed, err=-1");
    TEST_CHECK_LOG_RECORD_WITH_FUNC(
        ESP_LOG_ERROR,
        "nrf51swd_nvmc_is_ready_or_err",
        "nrf52swd_read_reg(REG_READY) failed, err=-1");
    TEST_CHECK_LOG_RECORD_WITH_FUNC(
        ESP_LOG_ERROR,
        "nrf52swd_write_mem",
        "nrf51swd_nvmc_wait_while_busy failed, err=-1");
    ASSERT_TRUE(esp_log_wrapper_is_empty());
}

TEST_F(TestNRF52Swd, nrf52swd_nrf52swd_write_mem_fail_on_second_wait) // NOLINT
{
    ASSERT_TRUE(nrf52swd_init());
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, "nRF52 SWD init");
    TEST_CHECK_LOG_RECORD(ESP_LOG_DEBUG, "spi_bus_initialize");
    TEST_CHECK_LOG_RECORD(ESP_LOG_DEBUG, "spi_bus_add_device");
    TEST_CHECK_LOG_RECORD(ESP_LOG_DEBUG, "libswd_init");
    TEST_CHECK_LOG_RECORD(ESP_LOG_DEBUG, "libswd_debug_init");
    TEST_CHECK_LOG_RECORD(ESP_LOG_DEBUG, "nrf52swd_init ok");
    ASSERT_TRUE(esp_log_wrapper_is_empty());

    this->m_nvmc_reg_ready_cnt             = 0U;
    this->m_nvmc_reg_ready_cnt_before_fail = 1;

    const uint32_t reg_addr       = 0x00010000U;
    const uint32_t arr_of_vals[5] = { 5, 6, 7, 8, 9 };

    ASSERT_FALSE(nrf52swd_write_mem(reg_addr, sizeof(arr_of_vals) / sizeof(arr_of_vals[0]), arr_of_vals));
    ASSERT_EQ(2, this->m_memSegmentsWrite.size());
    {
        ASSERT_EQ(0x4001E000UL + 0x504U, this->m_memSegmentsWrite[0].segmentAddr);
        ASSERT_EQ(1, this->m_memSegmentsWrite[0].data.size());
        ASSERT_EQ(1U, this->m_memSegmentsWrite[0].data[0]);
    }
    {
        ASSERT_EQ(reg_addr, this->m_memSegmentsWrite[1].segmentAddr);
        ASSERT_EQ(sizeof(arr_of_vals) / sizeof(arr_of_vals[0]), this->m_memSegmentsWrite[1].data.size());
        ASSERT_EQ(arr_of_vals[0], this->m_memSegmentsWrite[1].data[0]);
        ASSERT_EQ(arr_of_vals[1], this->m_memSegmentsWrite[1].data[1]);
        ASSERT_EQ(arr_of_vals[2], this->m_memSegmentsWrite[1].data[2]);
        ASSERT_EQ(arr_of_vals[3], this->m_memSegmentsWrite[1].data[3]);
        ASSERT_EQ(arr_of_vals[4], this->m_memSegmentsWrite[1].data[4]);
    }

    TEST_CHECK_LOG_RECORD(ESP_LOG_ERROR, "nrf52swd_read_reg: libswd_memap_read_int_32(0x4001e400) failed, err=-1");
    TEST_CHECK_LOG_RECORD_WITH_FUNC(
        ESP_LOG_ERROR,
        "nrf51swd_nvmc_is_ready_or_err",
        "nrf52swd_read_reg(REG_READY) failed, err=-1");
    TEST_CHECK_LOG_RECORD_WITH_FUNC(
        ESP_LOG_ERROR,
        "nrf52swd_write_mem",
        "nrf51swd_nvmc_wait_while_busy failed, err=-1");
    ASSERT_TRUE(esp_log_wrapper_is_empty());
}

TEST_F(TestNRF52Swd, nrf52swd_nrf52swd_write_mem_fail_on_write_reg_config_wen_wen) // NOLINT
{
    ASSERT_TRUE(nrf52swd_init());
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, "nRF52 SWD init");
    TEST_CHECK_LOG_RECORD(ESP_LOG_DEBUG, "spi_bus_initialize");
    TEST_CHECK_LOG_RECORD(ESP_LOG_DEBUG, "spi_bus_add_device");
    TEST_CHECK_LOG_RECORD(ESP_LOG_DEBUG, "libswd_init");
    TEST_CHECK_LOG_RECORD(ESP_LOG_DEBUG, "libswd_debug_init");
    TEST_CHECK_LOG_RECORD(ESP_LOG_DEBUG, "nrf52swd_init ok");
    ASSERT_TRUE(esp_log_wrapper_is_empty());

    this->m_nvmc_reg_ready_cnt              = 0U;
    this->m_nvmc_reg_ready_cnt_before_fail  = 2;
    this->m_nvmc_reg_config_cnt_before_fail = 1;

    const uint32_t reg_addr       = 0x00010000U;
    const uint32_t arr_of_vals[5] = { 5, 6, 7, 8, 9 };

    ASSERT_FALSE(nrf52swd_write_mem(reg_addr, sizeof(arr_of_vals) / sizeof(arr_of_vals[0]), arr_of_vals));
    ASSERT_EQ(1, this->m_memSegmentsWrite.size());
    {
        ASSERT_EQ(0x4001E000UL + 0x504U, this->m_memSegmentsWrite[0].segmentAddr);
        ASSERT_EQ(1, this->m_memSegmentsWrite[0].data.size());
        ASSERT_EQ(1U, this->m_memSegmentsWrite[0].data[0]);
    }

    TEST_CHECK_LOG_RECORD(ESP_LOG_ERROR, "nrf52swd_write_reg: libswd_memap_write_int_32(0x4001e504) failed, err=-1");
    TEST_CHECK_LOG_RECORD_WITH_FUNC(
        ESP_LOG_ERROR,
        "nrf52swd_write_mem",
        "nrf52swd_write_reg(REG_CONFIG):=WEN failed, err=-1");
    ASSERT_TRUE(esp_log_wrapper_is_empty());
}

TEST_F(TestNRF52Swd, nrf52swd_nrf52swd_write_mem_fail_on_write_reg_config_wen_ren) // NOLINT
{
    ASSERT_TRUE(nrf52swd_init());
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, "nRF52 SWD init");
    TEST_CHECK_LOG_RECORD(ESP_LOG_DEBUG, "spi_bus_initialize");
    TEST_CHECK_LOG_RECORD(ESP_LOG_DEBUG, "spi_bus_add_device");
    TEST_CHECK_LOG_RECORD(ESP_LOG_DEBUG, "libswd_init");
    TEST_CHECK_LOG_RECORD(ESP_LOG_DEBUG, "libswd_debug_init");
    TEST_CHECK_LOG_RECORD(ESP_LOG_DEBUG, "nrf52swd_init ok");
    ASSERT_TRUE(esp_log_wrapper_is_empty());

    this->m_nvmc_reg_ready_cnt              = 0U;
    this->m_nvmc_reg_ready_cnt_before_fail  = 2;
    this->m_nvmc_reg_config_cnt_before_fail = 2;

    const uint32_t reg_addr       = 0x00010000U;
    const uint32_t arr_of_vals[5] = { 5, 6, 7, 8, 9 };

    ASSERT_FALSE(nrf52swd_write_mem(reg_addr, sizeof(arr_of_vals) / sizeof(arr_of_vals[0]), arr_of_vals));
    ASSERT_EQ(3, this->m_memSegmentsWrite.size());
    {
        ASSERT_EQ(0x4001E000UL + 0x504U, this->m_memSegmentsWrite[0].segmentAddr);
        ASSERT_EQ(1, this->m_memSegmentsWrite[0].data.size());
        ASSERT_EQ(1U, this->m_memSegmentsWrite[0].data[0]);
    }
    {
        ASSERT_EQ(reg_addr, this->m_memSegmentsWrite[1].segmentAddr);
        ASSERT_EQ(sizeof(arr_of_vals) / sizeof(arr_of_vals[0]), this->m_memSegmentsWrite[1].data.size());
        ASSERT_EQ(arr_of_vals[0], this->m_memSegmentsWrite[1].data[0]);
        ASSERT_EQ(arr_of_vals[1], this->m_memSegmentsWrite[1].data[1]);
        ASSERT_EQ(arr_of_vals[2], this->m_memSegmentsWrite[1].data[2]);
        ASSERT_EQ(arr_of_vals[3], this->m_memSegmentsWrite[1].data[3]);
        ASSERT_EQ(arr_of_vals[4], this->m_memSegmentsWrite[1].data[4]);
    }
    {
        ASSERT_EQ(0x4001E000UL + 0x504U, this->m_memSegmentsWrite[2].segmentAddr);
        ASSERT_EQ(1, this->m_memSegmentsWrite[2].data.size());
        ASSERT_EQ(0U, this->m_memSegmentsWrite[2].data[0]);
    }

    TEST_CHECK_LOG_RECORD(ESP_LOG_ERROR, "nrf52swd_write_reg: libswd_memap_write_int_32(0x4001e504) failed, err=-1");
    TEST_CHECK_LOG_RECORD_WITH_FUNC(
        ESP_LOG_ERROR,
        "nrf52swd_write_mem",
        "nrf52swd_write_reg(REG_CONFIG):=REN failed, err=-1");
    ASSERT_TRUE(esp_log_wrapper_is_empty());
}

TEST_F(TestNRF52Swd, nrf52swd_nrf52swd_write_mem_fail_on_write) // NOLINT
{
    ASSERT_TRUE(nrf52swd_init());
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, "nRF52 SWD init");
    TEST_CHECK_LOG_RECORD(ESP_LOG_DEBUG, "spi_bus_initialize");
    TEST_CHECK_LOG_RECORD(ESP_LOG_DEBUG, "spi_bus_add_device");
    TEST_CHECK_LOG_RECORD(ESP_LOG_DEBUG, "libswd_init");
    TEST_CHECK_LOG_RECORD(ESP_LOG_DEBUG, "libswd_debug_init");
    TEST_CHECK_LOG_RECORD(ESP_LOG_DEBUG, "nrf52swd_init ok");
    ASSERT_TRUE(esp_log_wrapper_is_empty());

    this->m_nvmc_reg_ready_cnt                  = 0U;
    this->m_nvmc_reg_ready_cnt_before_fail      = 2;
    this->m_libswd_write_int_32_cnt_before_fail = 2;

    const uint32_t reg_addr       = 0x00010000U;
    const uint32_t arr_of_vals[5] = { 5, 6, 7, 8, 9 };

    ASSERT_FALSE(nrf52swd_write_mem(reg_addr, sizeof(arr_of_vals) / sizeof(arr_of_vals[0]), arr_of_vals));
    ASSERT_EQ(2, this->m_memSegmentsWrite.size());
    {
        ASSERT_EQ(0x4001E000UL + 0x504U, this->m_memSegmentsWrite[0].segmentAddr);
        ASSERT_EQ(1, this->m_memSegmentsWrite[0].data.size());
        ASSERT_EQ(1U, this->m_memSegmentsWrite[0].data[0]);
    }
    {
        ASSERT_EQ(reg_addr, this->m_memSegmentsWrite[1].segmentAddr);
        ASSERT_EQ(sizeof(arr_of_vals) / sizeof(arr_of_vals[0]), this->m_memSegmentsWrite[1].data.size());
        ASSERT_EQ(arr_of_vals[0], this->m_memSegmentsWrite[1].data[0]);
        ASSERT_EQ(arr_of_vals[1], this->m_memSegmentsWrite[1].data[1]);
        ASSERT_EQ(arr_of_vals[2], this->m_memSegmentsWrite[1].data[2]);
        ASSERT_EQ(arr_of_vals[3], this->m_memSegmentsWrite[1].data[3]);
        ASSERT_EQ(arr_of_vals[4], this->m_memSegmentsWrite[1].data[4]);
    }

    TEST_CHECK_LOG_RECORD_WITH_FUNC(ESP_LOG_ERROR, "nrf52swd_write_mem", "libswd_memap_write_int_32 failed, err=-1");
    ASSERT_TRUE(esp_log_wrapper_is_empty());
}
