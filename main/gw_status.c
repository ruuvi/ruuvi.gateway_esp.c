/**
 * @file gw_status.h
 * @author TheSomeMan
 * @date 2022-05-05
 * @copyright Ruuvi Innovations Ltd, license BSD-3-Clause.
 */

#include "gw_status.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"

#define LOG_LOCAL_LEVEL LOG_LEVEL_INFO
#include "log.h"

#define WIFI_CONNECTED_BIT (1U << 0U)
#define MQTT_CONNECTED_BIT (1U << 1U)
#define ETH_CONNECTED_BIT  (1U << 4U)
#define ETH_LINK_UP_BIT    (1U << 5U)
#define NRF_STATUS_BIT     (1U << 7U)

static const char TAG[] = "gw_status";

static EventGroupHandle_t g_p_ev_grp_status_bits;

bool
gw_status_init(void)
{
    g_p_ev_grp_status_bits = xEventGroupCreate();
    if (NULL == g_p_ev_grp_status_bits)
    {
        LOG_ERR("%s failed", "xEventGroupCreate");
        return false;
    }
    return true;
}

void
gw_status_set_wifi_connected(void)
{
    xEventGroupSetBits(g_p_ev_grp_status_bits, WIFI_CONNECTED_BIT);
}

void
gw_status_clear_wifi_connected(void)
{
    xEventGroupClearBits(g_p_ev_grp_status_bits, WIFI_CONNECTED_BIT);
}

void
gw_status_set_eth_connected(void)
{
    xEventGroupSetBits(g_p_ev_grp_status_bits, ETH_CONNECTED_BIT);
}

void
gw_status_clear_eth_connected(void)
{
    xEventGroupClearBits(g_p_ev_grp_status_bits, ETH_CONNECTED_BIT | ETH_LINK_UP_BIT);
}

void
gw_status_set_eth_link_up(void)
{
    xEventGroupSetBits(g_p_ev_grp_status_bits, ETH_LINK_UP_BIT);
}

bool
gw_status_is_eth_link_up(void)
{
    if (0 != (xEventGroupGetBits(g_p_ev_grp_status_bits) & ETH_LINK_UP_BIT))
    {
        return true;
    }
    return false;
}

bool
gw_status_is_network_connected(void)
{
    if (0 != (xEventGroupGetBits(g_p_ev_grp_status_bits) & (WIFI_CONNECTED_BIT | ETH_CONNECTED_BIT)))
    {
        return true;
    }
    return false;
}

void
gw_status_set_mqtt_connected(void)
{
    xEventGroupSetBits(g_p_ev_grp_status_bits, MQTT_CONNECTED_BIT);
}

void
gw_status_clear_mqtt_connected(void)
{
    xEventGroupClearBits(g_p_ev_grp_status_bits, MQTT_CONNECTED_BIT);
}

bool
gw_status_is_mqtt_connected(void)
{
    if (0 != (xEventGroupGetBits(g_p_ev_grp_status_bits) & MQTT_CONNECTED_BIT))
    {
        return true;
    }
    return false;
}

void
gw_status_set_nrf_status(void)
{
    xEventGroupSetBits(g_p_ev_grp_status_bits, NRF_STATUS_BIT);
}

void
gw_status_clear_nrf_status(void)
{
    xEventGroupClearBits(g_p_ev_grp_status_bits, NRF_STATUS_BIT);
}

bool
gw_status_get_nrf_status(void)
{
    if (0 != (xEventGroupGetBits(g_p_ev_grp_status_bits) & NRF_STATUS_BIT))
    {
        return true;
    }
    return false;
}
