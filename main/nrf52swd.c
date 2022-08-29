/**
 * @file nrf52swd.c
 * @author TheSomeMan
 * @date 2020-09-10
 * @copyright Ruuvi Innovations Ltd, license BSD-3-Clause.
 */

#include "nrf52swd.h"
#include <stdbool.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <driver/spi_master.h>
#include <driver/gpio.h>
#include "libswd.h"
#include "ruuvi_board_gwesp.h"
#include "gpio_switch_ctrl.h"

#if RUUVI_TESTS_NRF52SWD
#define LOG_LOCAL_LEVEL LOG_LEVEL_DEBUG
#else
#define LOG_LOCAL_LEVEL LOG_LEVEL_INFO
#endif
#include "log.h"

static const char *TAG = "SWD";

typedef int LibSWD_ReturnCode_t;
typedef int LibSWD_IdCode_t;
typedef int LibSWD_Data_t;

#define NRF52SWD_LOG_ERR(func, err) \
    do \
    { \
        LOG_ERR_VAL(err, "%s failed", func); \
    } while (0)

static const spi_bus_config_t pinsSPI = {
    .mosi_io_num     = RB_ESP32_GPIO_MUX_NRF52_SWD_IO,
    .miso_io_num     = GPIO_NUM_NC, // not connected
    .sclk_io_num     = RB_ESP32_GPIO_MUX_NRF52_SWD_CLK,
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
static bool                g_nrf52swd_is_gpio_cfg_nreset_initialized;
static bool                g_nrf52swd_is_spi_initialized;
static bool                g_nrf52swd_is_spi_device_added;
static bool                g_nrf52swd_switch_ctrl_activated;

bool
nrf52swd_init_gpio_cfg_nreset(void)
{
    if (g_nrf52swd_is_gpio_cfg_nreset_initialized)
    {
        return true;
    }
    const gpio_config_t io_conf_nrf52_nreset = {
        .pin_bit_mask = (1ULL << (uint32_t)RB_ESP32_GPIO_MUX_NRF52_NRST),
        .mode         = GPIO_MODE_OUTPUT,
        .pull_up_en   = 1,
        .pull_down_en = 0,
        .intr_type    = GPIO_INTR_DISABLE,
    };
    const esp_err_t err = gpio_config(&io_conf_nrf52_nreset);
    if (ESP_OK != err)
    {
        NRF52SWD_LOG_ERR("gpio_config(nRF52 nRESET)", err);
        return false;
    }
    g_nrf52swd_is_gpio_cfg_nreset_initialized = true;
    return true;
}

bool
nrf52swd_deinit_gpio_cfg_nreset(void)
{
    if (!g_nrf52swd_is_gpio_cfg_nreset_initialized)
    {
        return true;
    }
    const gpio_config_t io_conf_nrf52_nreset = {
        .pin_bit_mask = (1ULL << (uint32_t)RB_ESP32_GPIO_MUX_NRF52_NRST),
        .mode         = GPIO_MODE_DISABLE,
        .pull_up_en   = 0,
        .pull_down_en = 0,
        .intr_type    = GPIO_INTR_DISABLE,
    };
    const esp_err_t err = gpio_config(&io_conf_nrf52_nreset);
    if (ESP_OK != err)
    {
        NRF52SWD_LOG_ERR("gpio_config(nRF52 nRESET)", err);
        return false;
    }
    g_nrf52swd_is_gpio_cfg_nreset_initialized = false;
    return true;
}

NRF52SWD_STATIC
bool
nrf52swd_init_spi_init(void)
{
    LOG_DBG("spi_bus_initialize");
    esp_err_t err = spi_bus_initialize(HSPI_HOST, &pinsSPI, 0);
    if (ESP_OK != err)
    {
        NRF52SWD_LOG_ERR("spi_bus_initialize", err);
        return false;
    }
    return true;
}

NRF52SWD_STATIC
bool
nrf52swd_init_spi_add_device(void)
{
    LOG_DBG("spi_bus_add_device");
    const esp_err_t err = spi_bus_add_device(HSPI_HOST, &confSPI, &g_nrf52swd_device_spi);
    if (ESP_OK != err)
    {
        NRF52SWD_LOG_ERR("spi_bus_add_device", err);
        return false;
    }
    return true;
}

static bool
nrf52swd_libswd_debug_init(void)
{
    LOG_DBG("libswd_debug_init");
    const LibSWD_ReturnCode_t ret_val = libswd_debug_init(gp_nrf52swd_libswd_ctx, LIBSWD_OPERATION_EXECUTE);
    if (ret_val < 0)
    {
        NRF52SWD_LOG_ERR("libswd_debug_init", ret_val);
        return false;
    }
    return true;
}

NRF52SWD_STATIC
bool
nrf52swd_gpio_init(void)
{
    if (!nrf52swd_init_gpio_cfg_nreset())
    {
        return false;
    }

    gpio_switch_ctrl_activate();
    g_nrf52swd_switch_ctrl_activated = true;

    return true;
}

static bool
nrf52swd_init_internal(void)
{
    LOG_INFO("nRF52 SWD init");
    if (!nrf52swd_gpio_init())
    {
        return false;
    }

    nrf52swd_reset(false);
    if (!nrf52swd_init_spi_init())
    {
        return false;
    }

    g_nrf52swd_is_spi_initialized = true;
    if (!nrf52swd_init_spi_add_device())
    {
        return false;
    }

    g_nrf52swd_is_spi_device_added = true;
    LOG_DBG("libswd_init");
    gp_nrf52swd_libswd_ctx = libswd_init();
    if (NULL == gp_nrf52swd_libswd_ctx)
    {
        NRF52SWD_LOG_ERR("libswd_init", -1);
        return false;
    }

    libswd_log_level_set(gp_nrf52swd_libswd_ctx, LIBSWD_LOGLEVEL_DEBUG);
    gp_nrf52swd_libswd_ctx->driver->device = &g_nrf52swd_device_spi;
    if (!nrf52swd_libswd_debug_init())
    {
        return false;
    }

    LOG_DBG("nrf52swd_init ok");
    return true;
}

bool
nrf52swd_init(void)
{
    if (!nrf52swd_init_internal())
    {
        nrf52swd_deinit();
        return false;
    }
    return true;
}

void
nrf52swd_deinit(void)
{
    LOG_INFO("nRF52 SWD deinit");
    if (NULL != gp_nrf52swd_libswd_ctx)
    {
        LOG_DBG("libswd_deinit");
        libswd_deinit(gp_nrf52swd_libswd_ctx);
        gp_nrf52swd_libswd_ctx = NULL;
    }
    if (g_nrf52swd_is_spi_device_added)
    {
        g_nrf52swd_is_spi_device_added = false;

        LOG_DBG("spi_bus_remove_device");
        const esp_err_t err = spi_bus_remove_device(g_nrf52swd_device_spi);
        if (ESP_OK != err)
        {
            LOG_ERR("%s: spi_bus_remove_device failed, err=%d", __func__, err);
        }
        g_nrf52swd_device_spi = NULL;
    }
    if (g_nrf52swd_is_spi_initialized)
    {
        g_nrf52swd_is_spi_initialized = false;

        LOG_DBG("spi_bus_free");
        esp_err_t err = spi_bus_free(HSPI_HOST);
        if (ESP_OK != err)
        {
            LOG_ERR("%s: spi_bus_free failed, err=%d", __func__, err);
        }
    }
    if (g_nrf52swd_switch_ctrl_activated)
    {
        gpio_switch_ctrl_deactivate();
        g_nrf52swd_switch_ctrl_activated = false;
    }
}

bool
nrf52swd_reset(const bool flag_reset)
{
    esp_err_t res = ESP_OK;
    if (flag_reset)
    {
        res = gpio_set_level(RB_ESP32_GPIO_MUX_NRF52_NRST, 0U);
    }
    else
    {
        res = gpio_set_level(RB_ESP32_GPIO_MUX_NRF52_NRST, 1U);
    }
    if (ESP_OK != res)
    {
        NRF52SWD_LOG_ERR("gpio_set_level", res);
        return false;
    }
    return true;
}

bool
nrf52swd_check_id_code(void)
{
    LibSWD_IdCode_t *p_idcode_ptr = NULL;
    LOG_DBG("libswd_dap_detect");
    const LibSWD_ReturnCode_t dap_res = libswd_dap_detect(
        gp_nrf52swd_libswd_ctx,
        LIBSWD_OPERATION_EXECUTE,
        &p_idcode_ptr);
    if (LIBSWD_OK != dap_res)
    {
        NRF52SWD_LOG_ERR("libswd_dap_detect", dap_res);
        return false;
    }
    const uint32_t id_code          = *p_idcode_ptr;
    const uint32_t expected_id_code = 0x2ba01477;
    if (expected_id_code != id_code)
    {
        LOG_ERR("Wrong nRF52 ID code 0x%08x (expected 0x%08x)", id_code, expected_id_code);
        return false;
    }
    LOG_INFO("IDCODE: 0x%08x", (unsigned)id_code);
    return true;
}

bool
nrf52swd_debug_halt(void)
{
    const LibSWD_ReturnCode_t ret_val = libswd_debug_halt(gp_nrf52swd_libswd_ctx, LIBSWD_OPERATION_EXECUTE);
    if (ret_val < 0)
    {
        NRF52SWD_LOG_ERR("libswd_debug_halt", ret_val);
        return false;
    }
    return true;
}

bool
nrf52swd_debug_enable_reset_vector_catch(void)
{
    const LibSWD_ReturnCode_t ret_val = libswd_debug_enable_reset_vector_catch(
        gp_nrf52swd_libswd_ctx,
        LIBSWD_OPERATION_EXECUTE);
    if (ret_val < 0)
    {
        NRF52SWD_LOG_ERR("libswd_debug_enable_reset_vector_catch", ret_val);
        return false;
    }
    return true;
}

bool
nrf52swd_debug_reset(void)
{
    const LibSWD_ReturnCode_t ret_val = libswd_debug_reset(gp_nrf52swd_libswd_ctx, LIBSWD_OPERATION_EXECUTE);
    if (ret_val < 0)
    {
        NRF52SWD_LOG_ERR("libswd_debug_reset", ret_val);
        return false;
    }
    return true;
}

bool
nrf52swd_debug_run(void)
{
    LOG_INFO("Run nRF52 firmware");
    const LibSWD_ReturnCode_t ret_val = libswd_debug_run(gp_nrf52swd_libswd_ctx, LIBSWD_OPERATION_EXECUTE);
    if (ret_val < 0)
    {
        NRF52SWD_LOG_ERR("libswd_debug_run", ret_val);
        return false;
    }
    return true;
}

NRF52SWD_STATIC
bool
nrf52swd_read_reg(const uint32_t reg_addr, uint32_t *p_val)
{
    const LibSWD_ReturnCode_t ret_val = libswd_memap_read_int_32(
        gp_nrf52swd_libswd_ctx,
        LIBSWD_OPERATION_EXECUTE,
        reg_addr,
        1,
        (LibSWD_Data_t *)p_val);
    if (LIBSWD_OK != ret_val)
    {
        LOG_ERR("%s: libswd_memap_read_int_32(0x%08x) failed, err=%d", __func__, reg_addr, ret_val);
        return false;
    }
    return true;
}

NRF52SWD_STATIC
bool
nrf52swd_write_reg(const uint32_t reg_addr, const uint32_t val)
{
    LibSWD_Data_t             data_val = val;
    const LibSWD_ReturnCode_t ret_val  = libswd_memap_write_int_32(
        gp_nrf52swd_libswd_ctx,
        LIBSWD_OPERATION_EXECUTE,
        reg_addr,
        1,
        &data_val);
    if (LIBSWD_OK != ret_val)
    {
        LOG_ERR("%s: libswd_memap_write_int_32(0x%08x) failed, err=%d", __func__, reg_addr, ret_val);
        return false;
    }
    return true;
}

NRF52SWD_STATIC
bool
nrf51swd_nvmc_is_ready_or_err(bool *p_result)
{
    *p_result        = false;
    uint32_t reg_val = 0;
    if (!nrf52swd_read_reg(NRF52_NVMC_REG_READY, &reg_val))
    {
        NRF52SWD_LOG_ERR("nrf52swd_read_reg(REG_READY)", -1);
        return true;
    }
    if (0 != (reg_val & NRF52_NVMC_REG_READY__MASK))
    {
        *p_result = true;
        return true;
    }
    return false;
}

NRF52SWD_STATIC
bool
nrf51swd_nvmc_wait_while_busy(void)
{
    bool result = false;
    while (!nrf51swd_nvmc_is_ready_or_err(&result))
    {
        vTaskDelay(1);
    }
    return result;
}

bool
nrf52swd_erase_all(void)
{
    LOG_INFO("nRF52: Erase all flash");
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
