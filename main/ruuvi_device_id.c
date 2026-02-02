/**
 * @file ruuvi_device_id.c
 * @author TheSomeMan
 * @date 2021-07-08
 * @copyright Ruuvi Innovations Ltd, license BSD-3-Clause.
 */

#include "ruuvi_device_id.h"
#include <string.h>
#include <esp_attr.h>
#include "str_buf.h"
#include "os_mutex.h"
#include "api.h"
#include "ruuvi_endpoint_ca_uart.h"
#include "settings.h"

#define LOG_LOCAL_LEVEL LOG_LEVEL_INFO
#include "log.h"

static const char TAG[] = "nrf52";

#define RUUVI_GET_NRF52_ID_NUM_RETRIES (3U)
#define RUUVI_GET_NRF52_ID_DELAY_MS    (1000U)
#define RUUVI_GET_NRF52_ID_STEP_MS     (100U)

#define BYTE_FF (0xFFU)

static nrf52_device_info_t  g_nrf52_device_info = { 0 };
static volatile bool        g_ruuvi_device_id_flag_nrf52_id_received;
static os_mutex_t IRAM_ATTR g_ruuvi_device_id_mutex;
static os_mutex_static_t    g_ruuvi_device_id_mutex_mem;

void
ruuvi_device_id_init(void)
{
    g_ruuvi_device_id_mutex = os_mutex_create_static(&g_ruuvi_device_id_mutex_mem);
    memset(&g_nrf52_device_info, 0, sizeof(g_nrf52_device_info));
    g_ruuvi_device_id_flag_nrf52_id_received = false;
}

void
ruuvi_device_id_deinit(void)
{
    os_mutex_lock(g_ruuvi_device_id_mutex);
    g_ruuvi_device_id_flag_nrf52_id_received = false;
    memset(&g_nrf52_device_info, 0, sizeof(g_nrf52_device_info));
    os_mutex_unlock(g_ruuvi_device_id_mutex);
    os_mutex_delete(&g_ruuvi_device_id_mutex);
}

void
ruuvi_device_id_set(const nrf52_device_id_t* const p_nrf52_device_id, const mac_address_bin_t* const p_nrf52_mac_addr)
{
    os_mutex_lock(g_ruuvi_device_id_mutex);
    g_nrf52_device_info.nrf52_device_id      = *p_nrf52_device_id;
    g_nrf52_device_info.nrf52_mac_addr       = *p_nrf52_mac_addr;
    g_ruuvi_device_id_flag_nrf52_id_received = true;
    os_mutex_unlock(g_ruuvi_device_id_mutex);
}

static bool
ruuvi_device_id_is_set(nrf52_device_info_t* const p_nrf52_device_info)
{
    os_mutex_lock(g_ruuvi_device_id_mutex);
    const bool flag_is_set = g_ruuvi_device_id_flag_nrf52_id_received;
    if (flag_is_set)
    {
        *p_nrf52_device_info = g_nrf52_device_info;
    }
    os_mutex_unlock(g_ruuvi_device_id_mutex);
    return flag_is_set;
}

nrf52_device_info_t
ruuvi_device_id_request_and_wait(void)
{
    nrf52_device_info_t nrf52_device_info = { 0 };

    ruuvi_device_id_init();

    const uint32_t delay_ms = RUUVI_GET_NRF52_ID_DELAY_MS;
    const uint32_t step_ms  = RUUVI_GET_NRF52_ID_STEP_MS;
    for (uint32_t i = 0; i < RUUVI_GET_NRF52_ID_NUM_RETRIES; ++i)
    {
        if (ruuvi_device_id_is_set(&nrf52_device_info))
        {
            break;
        }
        LOG_INFO("Request nRF52 ID");
        api_send_get_device_id(RE_CA_UART_GET_DEVICE_ID);

        for (uint32_t j = 0; j < (delay_ms / step_ms); ++j)
        {
            vTaskDelay(step_ms / portTICK_PERIOD_MS);
            if (ruuvi_device_id_is_set(&nrf52_device_info))
            {
                break;
            }
        }
    }
    if (!ruuvi_device_id_is_set(&nrf52_device_info))
    {
        LOG_ERR("Failed to read nRF52 DEVICE ID");
        for (int32_t i = 0; i < sizeof(nrf52_device_info.nrf52_device_id.id); ++i)
        {
            nrf52_device_info.nrf52_device_id.id[i] = BYTE_FF;
        }
        nrf52_device_info.nrf52_mac_addr = settings_read_mac_addr();
    }
    else
    {
        settings_update_mac_addr(nrf52_device_info.nrf52_mac_addr);
    }
    return nrf52_device_info;
}
