/**
 * @file nrf52swd.c
 * @author TheSomeMan
 * @date 2020-09-10
 * @copyright Ruuvi Innovations Ltd, license BSD-3-Clause.
 */

#include "nrf52swd.h"
#include <stdbool.h>
#include <driver/spi_master.h>
#include <driver/gpio.h>
#include "libswd.h"
#include "esp_log.h"

static const char *TAG = "SWD";

typedef int LibSWD_ReturnCode_t;
typedef int LibSWD_IdCode_t;
typedef int LibSWD_Data_t;

#define NRF52_NVMC_BASE                    0x4001E000UL
#define NRF52_NVMC_REG_READY               (NRF52_NVMC_BASE + 0x400U)
#define NRF52_NVMC_REG_CONFIG              (NRF52_NVMC_BASE + 0x504U)
#define NRF52_NVMC_REG_ERASEPAGE           (NRF52_NVMC_BASE + 0x508U)
#define NRF52_NVMC_REG_ERASEALL            (NRF52_NVMC_BASE + 0x50CU)
#define NRF52_NVMC_REG_ERASEUICR           (NRF52_NVMC_BASE + 0x514U)
#define NRF52_NVMC_REG_ERASEPAGEPARTIAL    (NRF52_NVMC_BASE + 0x518U)
#define NRF52_NVMC_REG_ERASEPAGEPARTIALCFG (NRF52_NVMC_BASE + 0x51CU)

#define NRF52_NVMC_REG_READY__MASK (0x00000001UL)

#define NRF52_NVMC_REG_CONFIG__WEN_MASK (0x03UL)
#define NRF52_NVMC_REG_CONFIG__WEN_REN  (0U)
#define NRF52_NVMC_REG_CONFIG__WEN_WEN  (1U)
#define NRF52_NVMC_REG_CONFIG__WEN_EEN  (2U)

#define NRF52_NVMC_REG_ERASEALL__NO_OP (0U)
#define NRF52_NVMC_REG_ERASEALL__ERASE (1U)

#define NRF52_GPIO_SWD_CLK GPIO_NUM_15
#define NRF52_GPIO_SWD_IO  GPIO_NUM_16
#define NRF52_GPIO_NRST    GPIO_NUM_17

#define NRF52SWD_LOG_ERR(func, err) \
    do \
    { \
        ESP_LOGE(TAG, "%s: %s failed, err=%d", __func__, func, err); \
    } while (0)

static const spi_bus_config_t pinsSPI = {
    .mosi_io_num     = NRF52_GPIO_SWD_IO,
    .miso_io_num     = GPIO_NUM_NC, // not connected
    .sclk_io_num     = NRF52_GPIO_SWD_CLK,
    .quadwp_io_num   = GPIO_NUM_NC,
    .quadhd_io_num   = GPIO_NUM_NC,
    .max_transfer_sz = 0,
    .flags           = 0,
    .intr_flags      = 0,
};

static const spi_device_interface_config_t confSPI = {
    .command_bits     = 0,
    .address_bits     = 0,
    .dummy_bits       = 0,
    .mode             = 0,
    .duty_cycle_pos   = 0,
    .cs_ena_pretrans  = 0,
    .cs_ena_posttrans = 0,
    .clock_speed_hz   = 2000000,
    .input_delay_ns   = 0,
    .spics_io_num     = -1,
    .flags            = SPI_DEVICE_3WIRE | SPI_DEVICE_HALFDUPLEX | SPI_DEVICE_BIT_LSBFIRST,
    .queue_size       = 24,
    .pre_cb           = NULL,
    .post_cb          = NULL,
};

static spi_device_handle_t g_nrf52swd_device_spi;
static libswd_ctx_t *      gp_nrf52swd_libswd_ctx;
static bool                g_nrf52swd_is_spi_initialized;
static bool                g_nrf52swd_is_spi_device_added;

static bool
nrf52swd_init_gpio_cfg_nreset(void)
{
    bool                result               = false;
    const gpio_config_t io_conf_nrf52_nreset = {
        .pin_bit_mask = (1ULL << (unsigned)NRF52_GPIO_NRST),
        .mode         = GPIO_MODE_OUTPUT,
        .pull_up_en   = 1,
        .pull_down_en = 0,
        .intr_type    = GPIO_INTR_DISABLE,
    };
    const esp_err_t err = gpio_config(&io_conf_nrf52_nreset);
    if (ESP_OK != err)
    {
        NRF52SWD_LOG_ERR("gpio_config(nRF52 nRESET)", err);
    }
    else
    {
        result = true;
    }
    return result;
}

static bool
nrf52swd_init_spi_init(void)
{
    bool            result = false;
    const esp_err_t err    = spi_bus_initialize(HSPI_HOST, &pinsSPI, 0);
    if (ESP_OK != err)
    {
        NRF52SWD_LOG_ERR("spi_bus_initialize", err);
    }
    else
    {
        result = true;
    }
    return result;
}

static bool
nrf52swd_init_spi_add_device(void)
{
    bool            result = false;
    const esp_err_t err    = spi_bus_add_device(HSPI_HOST, &confSPI, &g_nrf52swd_device_spi);
    if (ESP_OK != err)
    {
        NRF52SWD_LOG_ERR("spi_bus_add_device", err);
    }
    else
    {
        result = true;
    }
    return result;
}

bool
nrf52swd_init(void)
{
    ESP_LOGI(TAG, "nRF52 SWD init");
    bool result = nrf52swd_init_gpio_cfg_nreset();
    if (result)
    {
        nrf52swd_reset(false);
        result = nrf52swd_init_spi_init();
    }

    if (result)
    {
        g_nrf52swd_is_spi_initialized = true;
        result                        = nrf52swd_init_spi_add_device();
    }

    if (result)
    {
        g_nrf52swd_is_spi_device_added = true;
        ESP_LOGD(TAG, "libswd_init");
        gp_nrf52swd_libswd_ctx = libswd_init();
        if (NULL == gp_nrf52swd_libswd_ctx)
        {
            NRF52SWD_LOG_ERR("libswd_init", -1);
            result = false;
        }
    }

    if (result)
    {
        libswd_log_level_set(gp_nrf52swd_libswd_ctx, LIBSWD_LOGLEVEL_DEBUG);
        gp_nrf52swd_libswd_ctx->driver->device = &g_nrf52swd_device_spi;
        ESP_LOGD(TAG, "libswd_debug_init");
        const LibSWD_ReturnCode_t ret_val = libswd_debug_init(gp_nrf52swd_libswd_ctx, LIBSWD_OPERATION_EXECUTE);
        if (ret_val < 0)
        {
            NRF52SWD_LOG_ERR("libswd_debug_init", ret_val);
            result = false;
        }
    }

    if (result)
    {
        ESP_LOGD(TAG, "nrf52swd_init ok");
    }
    else
    {
        nrf52swd_deinit();
    }
    return result;
}

void
nrf52swd_deinit(void)
{
    ESP_LOGI(TAG, "nRF52 SWD deinit");
    if (NULL != gp_nrf52swd_libswd_ctx)
    {
        libswd_deinit(gp_nrf52swd_libswd_ctx);
        gp_nrf52swd_libswd_ctx = NULL;
    }
    if (g_nrf52swd_is_spi_device_added)
    {
        g_nrf52swd_is_spi_device_added = false;

        const esp_err_t err = spi_bus_remove_device(g_nrf52swd_device_spi);
        if (ESP_OK != err)
        {
            ESP_LOGE(TAG, "%s: spi_bus_remove_device failed, err=%d", __func__, err);
        }
        g_nrf52swd_device_spi = NULL;
    }
    if (g_nrf52swd_is_spi_initialized)
    {
        g_nrf52swd_is_spi_initialized = false;

        const esp_err_t err = spi_bus_free(HSPI_HOST);
        if (ESP_OK != err)
        {
            ESP_LOGE(TAG, "%s: spi_bus_free failed, err=%d", __func__, err);
        }
    }
}

bool
nrf52swd_reset(const bool flag_reset)
{
    esp_err_t res = ESP_OK;
    if (flag_reset)
    {
        res = gpio_set_level(NRF52_GPIO_NRST, 0U);
    }
    else
    {
        res = gpio_set_level(NRF52_GPIO_NRST, 1U);
    }
    bool result = false;
    if (ESP_OK != res)
    {
        NRF52SWD_LOG_ERR("gpio_set_level", res);
    }
    else
    {
        result = true;
    }
    return result;
}

bool
nrf52swd_check_id_code(void)
{
    bool             result       = false;
    LibSWD_IdCode_t *p_idcode_ptr = NULL;
    ESP_LOGD(TAG, "libswd_dap_detect");
    const LibSWD_ReturnCode_t dap_res = libswd_dap_detect(
        gp_nrf52swd_libswd_ctx,
        LIBSWD_OPERATION_EXECUTE,
        &p_idcode_ptr);
    if (LIBSWD_OK != dap_res)
    {
        NRF52SWD_LOG_ERR("libswd_dap_detect", dap_res);
    }
    else
    {
        const uint32_t id_code          = *p_idcode_ptr;
        const uint32_t expected_id_code = 0x2ba01477;
        if (expected_id_code != id_code)
        {
            ESP_LOGE(TAG, "Wrong nRF52 ID code 0x%08x (expected 0x%08x)", id_code, expected_id_code);
        }
        else
        {
            ESP_LOGI(TAG, "IDCODE: 0x%08x", (unsigned)id_code);
            result = true;
        }
    }
    return result;
}

bool
nrf52swd_debug_halt(void)
{
    bool                      result  = false;
    const LibSWD_ReturnCode_t ret_val = libswd_debug_halt(gp_nrf52swd_libswd_ctx, LIBSWD_OPERATION_EXECUTE);
    if (ret_val < 0)
    {
        NRF52SWD_LOG_ERR("libswd_debug_halt", ret_val);
    }
    else
    {
        result = true;
    }
    return result;
}

bool
nrf52swd_debug_run(void)
{
    bool result = false;
    ESP_LOGI(TAG, "Run nRF52 firmware");
    const LibSWD_ReturnCode_t ret_val = libswd_debug_run(gp_nrf52swd_libswd_ctx, LIBSWD_OPERATION_EXECUTE);
    if (ret_val < 0)
    {
        NRF52SWD_LOG_ERR("libswd_debug_run", ret_val);
    }
    else
    {
        result = true;
    }
    return result;
}

static bool
nrf52swd_read_reg(const uint32_t reg_addr, uint32_t *p_val)
{
    bool                      result  = false;
    const LibSWD_ReturnCode_t ret_val = libswd_memap_read_int_32(
        gp_nrf52swd_libswd_ctx,
        LIBSWD_OPERATION_EXECUTE,
        reg_addr,
        1,
        (LibSWD_Data_t *)p_val);
    if (LIBSWD_OK != ret_val)
    {
        ESP_LOGE(TAG, "%s: libswd_memap_read_int_32(0x%08x) failed, err=%d", __func__, reg_addr, ret_val);
    }
    else
    {
        result = true;
    }
    return result;
}

static bool
nrf52swd_write_reg(const uint32_t reg_addr, const uint32_t val)
{
    bool                      result  = false;
    const LibSWD_ReturnCode_t ret_val = libswd_memap_write_int_32(
        gp_nrf52swd_libswd_ctx,
        LIBSWD_OPERATION_EXECUTE,
        reg_addr,
        1,
        (LibSWD_Data_t *)&val);
    if (LIBSWD_OK != ret_val)
    {
        ESP_LOGE(TAG, "%s: libswd_memap_write_int_32(0x%08x) failed, err=%d", __func__, reg_addr, ret_val);
    }
    else
    {
        result = true;
    }
    return result;
}

static bool
nrf51swd_nvmc_wait_while_busy(void)
{
    bool result = false;
    for (;;)
    {
        uint32_t reg_val = 0;
        if (!nrf52swd_read_reg(NRF52_NVMC_REG_READY, &reg_val))
        {
            NRF52SWD_LOG_ERR("nrf52swd_read_reg(REG_READY)", -1);
            break;
        }
        if (0 != (reg_val & NRF52_NVMC_REG_READY__MASK))
        {
            result = true;
            break;
        }
    }
    return result;
}

bool
nrf52swd_erase_page(const uint32_t page_address)
{
    if (!nrf51swd_nvmc_wait_while_busy())
    {
        NRF52SWD_LOG_ERR("nrf51swd_nvmc_wait_while_busy", -1);
        return false;
    }
    if (!nrf52swd_write_reg(NRF52_NVMC_REG_CONFIG, NRF52_NVMC_REG_CONFIG__WEN_EEN))
    {
        NRF52SWD_LOG_ERR("nrf52swd_write_reg(REG_CONFIG):=EEN", -1);
        return false;
    }
    if (!nrf52swd_write_reg(NRF52_NVMC_REG_ERASEPAGE, page_address))
    {
        NRF52SWD_LOG_ERR("nrf52swd_write_reg(REG_ERASEPAGE)", -1);
        return false;
    }
    if (!nrf51swd_nvmc_wait_while_busy())
    {
        NRF52SWD_LOG_ERR("nrf51swd_nvmc_wait_while_busy", -1);
        return false;
    }
    if (!nrf52swd_write_reg(NRF52_NVMC_REG_CONFIG, NRF52_NVMC_REG_CONFIG__WEN_REN))
    {
        NRF52SWD_LOG_ERR("nrf52swd_write_reg(REG_CONFIG):=REN", -1);
        return false;
    }
    return true;
}

bool
nrf52swd_erase_all(void)
{
    ESP_LOGI(TAG, "nRF52: Erase all flash");
    if (!nrf51swd_nvmc_wait_while_busy())
    {
        NRF52SWD_LOG_ERR("nrf51swd_nvmc_wait_while_busy", -1);
        return false;
    }
    if (!nrf52swd_write_reg(NRF52_NVMC_REG_CONFIG, NRF52_NVMC_REG_CONFIG__WEN_EEN))
    {
        NRF52SWD_LOG_ERR("nrf52swd_write_reg(REG_CONFIG):=EEN", -1);
        return false;
    }
    if (!nrf52swd_write_reg(NRF52_NVMC_REG_ERASEALL, NRF52_NVMC_REG_ERASEALL__ERASE))
    {
        NRF52SWD_LOG_ERR("nrf52swd_write_reg(REG_ERASEALL)", -1);
        return false;
    }
    if (!nrf51swd_nvmc_wait_while_busy())
    {
        NRF52SWD_LOG_ERR("nrf51swd_nvmc_wait_while_busy", -1);
        return false;
    }
    if (!nrf52swd_write_reg(NRF52_NVMC_REG_CONFIG, NRF52_NVMC_REG_CONFIG__WEN_REN))
    {
        NRF52SWD_LOG_ERR("nrf52swd_write_reg(REG_CONFIG):=REN", -1);
        return false;
    }
    return true;
}

bool
nrf52swd_read_mem(const uint32_t addr, const uint32_t num_words, uint32_t *p_buf)
{
    bool                      result = false;
    const LibSWD_ReturnCode_t res    = libswd_memap_read_int_32(
        gp_nrf52swd_libswd_ctx,
        LIBSWD_OPERATION_EXECUTE,
        addr,
        num_words,
        (LibSWD_Data_t *)p_buf);
    if (LIBSWD_OK != res)
    {
        NRF52SWD_LOG_ERR("libswd_memap_read_int_32", res);
    }
    else
    {
        result = true;
    }
    return result;
}

bool
nrf52swd_write_mem(const uint32_t addr, const uint32_t num_words, const uint32_t *p_buf)
{
    if (!nrf51swd_nvmc_wait_while_busy())
    {
        NRF52SWD_LOG_ERR("nrf51swd_nvmc_wait_while_busy", -1);
        return false;
    }
    if (!nrf52swd_write_reg(NRF52_NVMC_REG_CONFIG, NRF52_NVMC_REG_CONFIG__WEN_WEN))
    {
        NRF52SWD_LOG_ERR("nrf52swd_write_reg(REG_CONFIG):=WEN", -1);
        return false;
    }
    const LibSWD_ReturnCode_t res = libswd_memap_write_int_32(
        gp_nrf52swd_libswd_ctx,
        LIBSWD_OPERATION_EXECUTE,
        addr,
        num_words,
        (LibSWD_Data_t *)p_buf);
    if (LIBSWD_OK != res)
    {
        NRF52SWD_LOG_ERR("libswd_memap_write_int_32", res);
        return false;
    }
    if (!nrf51swd_nvmc_wait_while_busy())
    {
        NRF52SWD_LOG_ERR("nrf51swd_nvmc_wait_while_busy", -1);
        return false;
    }
    if (!nrf52swd_write_reg(NRF52_NVMC_REG_CONFIG, NRF52_NVMC_REG_CONFIG__WEN_REN))
    {
        NRF52SWD_LOG_ERR("nrf52swd_write_reg(REG_CONFIG):=REN", -1);
        return false;
    }
    return true;
}
