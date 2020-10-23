/**
 * @file ethernet.c
 * @author Jukka Saari
 * @date 2020-01-27
 * @copyright Ruuvi Innovations Ltd, license BSD-3-Clause.
 */

#include "ethernet.h"
#include "driver/gpio.h"
#include "esp_err.h"
#include "esp_eth.h"
#include "esp_eth_com.h"
#include "esp_event.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/task.h"
#include "leds.h"
#include "mqtt.h"
#include "ruuvi_gateway.h"
#include "sdkconfig.h"
#include "tcpip_adapter.h"
#include "time_task.h"
#include "wifi_manager.h"
#include <stdio.h>
#include <string.h>

#define LAN_CLOCK_ENABLE 2
#define ETH_PHY_ADDR     0
#define ETH_MDC_GPIO     23
#define ETH_MDIO_GPIO    18
#define ETH_PHY_RST_GPIO -1 // disabled

// Cloudfare public DNS
const char *dns_fallback_server = "1.1.1.1";

static const char *TAG = "eth";

/** Event handler for Ethernet events */
static void
eth_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    (void)arg;
    (void)event_base;
    uint8_t mac_addr[6] = { 0 };
    /* we can get the ethernet driver handle from event data */
    esp_eth_handle_t eth_handle = *(esp_eth_handle_t *)event_data;

    switch (event_id)
    {
        case ETHERNET_EVENT_CONNECTED:
            esp_eth_ioctl(eth_handle, ETH_CMD_G_MAC_ADDR, mac_addr);
            ESP_LOGI(TAG, "Ethernet Link Up");
            ESP_LOGI(
                TAG,
                "Ethernet HW Addr %02x:%02x:%02x:%02x:%02x:%02x",
                mac_addr[0],
                mac_addr[1],
                mac_addr[2],
                mac_addr[3],
                mac_addr[4],
                mac_addr[5]);
            ethernet_link_up_cb();
            break;

        case ETHERNET_EVENT_DISCONNECTED:
            ESP_LOGI(TAG, "Ethernet Link Down");
            ethernet_link_down_cb();
            break;

        case ETHERNET_EVENT_START:
            ESP_LOGI(TAG, "Ethernet Started");
            break;

        case ETHERNET_EVENT_STOP:
            ESP_LOGI(TAG, "Ethernet Stopped");
            break;

        default:
            break;
    }
}

/** Event handler for IP_EVENT_ETH_GOT_IP */
static void
got_ip_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    (void)arg;
    (void)event_base;
    (void)event_id;
    ip_event_got_ip_t *            event   = (ip_event_got_ip_t *)event_data;
    const tcpip_adapter_ip_info_t *ip_info = &event->ip_info;
    ESP_LOGI(TAG, "Ethernet Got IP Address");
    ESP_LOGI(TAG, "~~~~~~~~~~~");
    ESP_LOGI(TAG, "ETHIP:" IPSTR, IP2STR(&ip_info->ip));
    ESP_LOGI(TAG, "ETHMASK:" IPSTR, IP2STR(&ip_info->netmask));
    ESP_LOGI(TAG, "ETHGW:" IPSTR, IP2STR(&ip_info->gw));
    ESP_LOGI(TAG, "~~~~~~~~~~~");
    ethernet_connection_ok_cb();
}

static bool
ethernet_update_ip_dhcp(void)
{
    ESP_LOGI(TAG, "Using ETH DHCP");
    const esp_err_t ret = tcpip_adapter_dhcpc_start(TCPIP_ADAPTER_IF_ETH);
    if (ESP_OK != ret)
    {
        ESP_LOGE(TAG, "dhcpc start error: 0x%02x", (unsigned)ret);
        return false;
    }
    return true;
}

static bool
eth_tcpip_adapter_dhcpc_stop(void)
{
    const esp_err_t ret = tcpip_adapter_dhcpc_stop(TCPIP_ADAPTER_IF_ETH);
    if (ESP_OK != ret)
    {
        if (ESP_ERR_TCPIP_ADAPTER_DHCP_ALREADY_STOPPED == ret)
        {
            ESP_LOGW(TAG, "DHCP client already stopped");
        }
        else
        {
            ESP_LOGE(TAG, "DHCP client stop error: 0x%02x", ret);
            return false;
        }
    }
    return true;
}

static void
eth_tcpip_adapter_set_dns_info(const char *p_dns_ip, const tcpip_adapter_dns_type_t type)
{
    const char *dns_server = "Undef";
    switch (type)
    {
        case TCPIP_ADAPTER_DNS_MAIN:
            dns_server = "DNS1";
            break;
        case TCPIP_ADAPTER_DNS_BACKUP:
            dns_server = "DNS2";
            break;
        case TCPIP_ADAPTER_DNS_FALLBACK:
            dns_server = "DNS_FALLBACK";
            break;
        default:
            break;
    }

    tcpip_adapter_dns_info_t dns_info = {
        .ip = {
            .type = IPADDR_TYPE_V4,
        },
    };

    if (0 == ip4addr_aton(p_dns_ip, &dns_info.ip.u_addr.ip4))
    {
        ESP_LOGE(TAG, "Set DNS info failed for %s, invalid server: %s", dns_server, p_dns_ip);
        return;
    }
    const esp_err_t ret = tcpip_adapter_set_dns_info(TCPIP_ADAPTER_IF_ETH, type, &dns_info);
    if (ESP_OK != ret)
    {
        ESP_LOGE(TAG, "Set DNS info failed for %s, error: 0x%02x", dns_server, ret);
        return;
    }
}

static bool
ethernet_update_ip_static(void)
{
    ruuvi_gateway_config_t *p_gw_cfg = &g_gateway_config;
    tcpip_adapter_ip_info_t ipInfo   = { 0 };

    ESP_LOGI(TAG, "Using static IP");

    if (0 == ip4addr_aton(p_gw_cfg->eth_static_ip, &ipInfo.ip))
    {
        ESP_LOGE(TAG, "invalid eth static ip: %s", p_gw_cfg->eth_static_ip);
        return false;
    }

    if (0 == ip4addr_aton(p_gw_cfg->eth_netmask, &ipInfo.netmask))
    {
        ESP_LOGE(TAG, "invalid eth netmask: %s", p_gw_cfg->eth_netmask);
        return false;
    }

    if (0 == ip4addr_aton(p_gw_cfg->eth_gw, &ipInfo.gw))
    {
        ESP_LOGE(TAG, "invalid eth gw: %s", p_gw_cfg->eth_gw);
        return false;
    }

    if (!eth_tcpip_adapter_dhcpc_stop())
    {
        return false;
    }

    const esp_err_t ret = tcpip_adapter_set_ip_info(TCPIP_ADAPTER_IF_ETH, &ipInfo);
    if (ESP_OK != ret)
    {
        ESP_LOGE(TAG, "Failed to configure IP settings for ETH, err: 0x%02x", ret);
        return false;
    }

    eth_tcpip_adapter_set_dns_info(p_gw_cfg->eth_dns1, TCPIP_ADAPTER_DNS_MAIN);
    eth_tcpip_adapter_set_dns_info(p_gw_cfg->eth_dns2, TCPIP_ADAPTER_DNS_BACKUP);
    return true;
}

void
ethernet_update_ip(void)
{
    if (g_gateway_config.eth_dhcp)
    {
        if (!ethernet_update_ip_dhcp())
        {
            return;
        }
    }
    else
    {
        if (!ethernet_update_ip_static())
        {
            return;
        }
    }

    // set DNS fallback also for DHCP settings
    eth_tcpip_adapter_set_dns_info(dns_fallback_server, TCPIP_ADAPTER_DNS_FALLBACK);

    ESP_LOGI(TAG, "ETH ip updated");
}

void
ethernet_init(void)
{
    esp_log_level_set(TAG, ESP_LOG_DEBUG);
    gpio_set_direction(LAN_CLOCK_ENABLE, GPIO_MODE_OUTPUT);
    gpio_set_level(LAN_CLOCK_ENABLE, 1);
    ethernet_update_ip();
    tcpip_adapter_init();
    ESP_ERROR_CHECK(tcpip_adapter_set_default_eth_handlers());
    ESP_ERROR_CHECK(esp_event_handler_register(ETH_EVENT, ESP_EVENT_ANY_ID, &eth_event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_ETH_GOT_IP, &got_ip_event_handler, NULL));
    eth_mac_config_t mac_config    = ETH_MAC_DEFAULT_CONFIG();
    eth_phy_config_t phy_config    = ETH_PHY_DEFAULT_CONFIG();
    phy_config.phy_addr            = ETH_PHY_ADDR;
    phy_config.reset_gpio_num      = ETH_PHY_RST_GPIO;
    mac_config.smi_mdc_gpio_num    = ETH_MDC_GPIO;
    mac_config.smi_mdio_gpio_num   = ETH_MDIO_GPIO;
    mac_config.sw_reset_timeout_ms = 500;
    esp_eth_mac_t *  mac           = esp_eth_mac_new_esp32(&mac_config);
    esp_eth_phy_t *  phy           = esp_eth_phy_new_lan8720(&phy_config);
    esp_eth_config_t config        = ETH_DEFAULT_CONFIG(mac, phy);
    esp_eth_handle_t eth_handle    = NULL;
    esp_err_t        err_code      = esp_eth_driver_install(&config, &eth_handle);
    if (ESP_OK == err_code)
    {
        err_code = esp_eth_start(eth_handle);
        if (ESP_OK != err_code)
        {
            ESP_LOGE(TAG, "Ethernet start failed");
        }
    }
    else
    {
        ESP_LOGE(TAG, "Ethernet driver install failed");
    }
}