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
#define LOG_LOCAL_LEVEL LOG_LEVEL_DEBUG
#include "log.h"

#define LAN_CLOCK_ENABLE 2
#define ETH_PHY_ADDR     0
#define ETH_MDC_GPIO     23
#define ETH_MDIO_GPIO    18
#define ETH_PHY_RST_GPIO -1 // disabled

#define SW_RESET_TIMEOUT_MS 500

// Cloudfare public DNS
const char *dns_fallback_server = "1.1.1.1";

static const char *TAG = "ETH";

static esp_eth_handle_t g_eth_handle;
static void (*g_ethernet_link_up_cb)(void);
static void (*g_ethernet_link_down_cb)(void);
static void (*g_ethernet_connection_ok_cb)(const tcpip_adapter_ip_info_t *p_ip_info);

static void
eth_on_event_connected(esp_eth_handle_t eth_handle)
{
    mac_address_bin_t mac_bin = { 0 };
    esp_eth_ioctl(eth_handle, ETH_CMD_G_MAC_ADDR, &mac_bin.mac[0]);
    LOG_INFO("Ethernet Link Up");
    const mac_address_str_t mac_str = mac_address_to_str(&mac_bin);
    LOG_INFO("Ethernet HW Addr %s", mac_str.str_buf);
    g_ethernet_link_up_cb();
}

/** Event handler for Ethernet events */
static void
eth_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    (void)arg;
    (void)event_base;
    /* we can get the ethernet driver handle from event data */
    esp_eth_handle_t eth_handle = *(esp_eth_handle_t *)event_data;

    switch (event_id)
    {
        case ETHERNET_EVENT_CONNECTED:
            eth_on_event_connected(eth_handle);
            break;

        case ETHERNET_EVENT_DISCONNECTED:
            LOG_INFO("Ethernet Link Down");
            g_ethernet_link_down_cb();
            break;

        case ETHERNET_EVENT_START:
            LOG_INFO("Ethernet Started");
            break;

        case ETHERNET_EVENT_STOP:
            LOG_INFO("Ethernet Stopped");
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
    const ip_event_got_ip_t *      p_event   = (ip_event_got_ip_t *)event_data;
    const tcpip_adapter_ip_info_t *p_ip_info = &p_event->ip_info;
    LOG_INFO("Ethernet Got IP Address");
    LOG_INFO("~~~~~~~~~~~");
    LOG_INFO("ETH:IP:" IPSTR, IP2STR(&p_ip_info->ip));
    LOG_INFO("ETH:MASK:" IPSTR, IP2STR(&p_ip_info->netmask));
    LOG_INFO("ETH:GW:" IPSTR, IP2STR(&p_ip_info->gw));
    LOG_INFO("~~~~~~~~~~~");
    g_ethernet_connection_ok_cb(p_ip_info);
}

static bool
ethernet_update_ip_dhcp(void)
{
    LOG_INFO("Using ETH DHCP");
    const esp_err_t ret = tcpip_adapter_dhcpc_start(TCPIP_ADAPTER_IF_ETH);
    if (ESP_OK != ret)
    {
        LOG_ERR_ESP(ret, "%s failed", "tcpip_adapter_dhcpc_start");
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
        if ((ESP_ERR_TCPIP_ADAPTER_DHCP_ALREADY_STOPPED) == ret)
        {
            LOG_WARN("DHCP client already stopped");
        }
        else
        {
            LOG_ERR_ESP(ret, "%s failed", "tcpip_adapter_dhcpc_stop");
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
            .u_addr = {
                .ip4 = {
                    .addr = 0,
                },
            },
            .type = IPADDR_TYPE_V4,
        },
    };

    if (0 == ip4addr_aton(p_dns_ip, &dns_info.ip.u_addr.ip4))
    {
        LOG_ERR("Set DNS info failed for %s, invalid server: %s", dns_server, p_dns_ip);
        return;
    }
    const esp_err_t ret = tcpip_adapter_set_dns_info(TCPIP_ADAPTER_IF_ETH, type, &dns_info);
    if (ESP_OK != ret)
    {
        LOG_ERR_ESP(ret, "%s failed for DNS: '%s'", "tcpip_adapter_set_dns_info", dns_server);
        return;
    }
}

static bool
ethernet_update_ip_static(void)
{
    ruuvi_gateway_config_t *p_gw_cfg = &g_gateway_config;
    tcpip_adapter_ip_info_t ip_info  = { 0 };

    LOG_INFO("Using static IP");

    if (0 == ip4addr_aton(p_gw_cfg->eth.eth_static_ip, &ip_info.ip))
    {
        LOG_ERR("Invalid eth static ip: %s", p_gw_cfg->eth.eth_static_ip);
        return false;
    }

    if (0 == ip4addr_aton(p_gw_cfg->eth.eth_netmask, &ip_info.netmask))
    {
        LOG_ERR("invalid eth netmask: %s", p_gw_cfg->eth.eth_netmask);
        return false;
    }

    if (0 == ip4addr_aton(p_gw_cfg->eth.eth_gw, &ip_info.gw))
    {
        LOG_ERR("invalid eth gw: %s", p_gw_cfg->eth.eth_gw);
        return false;
    }

    if (!eth_tcpip_adapter_dhcpc_stop())
    {
        return false;
    }

    const esp_err_t ret = tcpip_adapter_set_ip_info(TCPIP_ADAPTER_IF_ETH, &ip_info);
    if (ESP_OK != ret)
    {
        LOG_ERR_ESP(ret, "%s failed", "tcpip_adapter_set_ip_info");
        return false;
    }

    eth_tcpip_adapter_set_dns_info(p_gw_cfg->eth.eth_dns1, TCPIP_ADAPTER_DNS_MAIN);
    eth_tcpip_adapter_set_dns_info(p_gw_cfg->eth.eth_dns2, TCPIP_ADAPTER_DNS_BACKUP);
    return true;
}

void
ethernet_update_ip(void)
{
    if (g_gateway_config.eth.eth_dhcp)
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

    LOG_INFO("ETH ip updated");
}

bool
ethernet_init(
    void (*ethernet_link_up_cb)(void),
    void (*ethernet_link_down_cb)(void),
    void (*ethernet_connection_ok_cb)(const tcpip_adapter_ip_info_t *p_ip_info))
{
    LOG_INFO("Ethernet init");
    g_ethernet_link_up_cb       = ethernet_link_up_cb;
    g_ethernet_link_down_cb     = ethernet_link_down_cb;
    g_ethernet_connection_ok_cb = ethernet_connection_ok_cb;
    gpio_set_direction(LAN_CLOCK_ENABLE, GPIO_MODE_OUTPUT);
    gpio_set_level(LAN_CLOCK_ENABLE, 1);
    ethernet_update_ip();
    tcpip_adapter_init();
    esp_err_t err_code = tcpip_adapter_set_default_eth_handlers();
    if (ESP_OK != err_code)
    {
        LOG_ERR_ESP(err_code, "%s failed", "tcpip_adapter_set_default_eth_handlers");
        return false;
    }
    err_code = esp_event_handler_register(ETH_EVENT, ESP_EVENT_ANY_ID, &eth_event_handler, NULL);
    if (ESP_OK != err_code)
    {
        LOG_ERR_ESP(err_code, "%s failed", "esp_event_handler_register");
        return false;
    }
    err_code = esp_event_handler_register(IP_EVENT, IP_EVENT_ETH_GOT_IP, &got_ip_event_handler, NULL);
    if (ESP_OK != err_code)
    {
        LOG_ERR_ESP(err_code, "%s failed", "esp_event_handler_register");
        return false;
    }
    eth_mac_config_t mac_config = ETH_MAC_DEFAULT_CONFIG();
    eth_phy_config_t phy_config = ETH_PHY_DEFAULT_CONFIG();

    phy_config.phy_addr       = ETH_PHY_ADDR;
    phy_config.reset_gpio_num = ETH_PHY_RST_GPIO;

    mac_config.smi_mdc_gpio_num    = ETH_MDC_GPIO;
    mac_config.smi_mdio_gpio_num   = ETH_MDIO_GPIO;
    mac_config.sw_reset_timeout_ms = SW_RESET_TIMEOUT_MS;

    esp_eth_mac_t *  mac    = esp_eth_mac_new_esp32(&mac_config);
    esp_eth_phy_t *  phy    = esp_eth_phy_new_lan8720(&phy_config);
    esp_eth_config_t config = ETH_DEFAULT_CONFIG(mac, phy);
    err_code                = esp_eth_driver_install(&config, &g_eth_handle);
    if (ESP_OK != err_code)
    {
        LOG_ERR_ESP(err_code, "Ethernet driver install failed");
        return false;
    }
    return true;
}

void
ethernet_deinit(void)
{
    if (NULL == g_eth_handle)
    {
        return;
    }
    LOG_INFO("Ethernet deinit");

    esp_err_t err_code = esp_eth_stop(g_eth_handle);
    if (ESP_OK != err_code)
    {
        LOG_ERR_ESP(err_code, "Ethernet stop failed");
    }

    err_code = esp_eth_driver_uninstall(g_eth_handle);

    g_eth_handle = NULL;
    if (ESP_OK != err_code)
    {
        LOG_ERR_ESP(err_code, "Ethernet driver uninstall failed");
    }

    err_code = esp_event_handler_unregister(ETH_EVENT, ESP_EVENT_ANY_ID, &eth_event_handler);
    if (ESP_OK != err_code)
    {
        LOG_ERR_ESP(err_code, "Ethernet event handler unregister failed: %s", "ESP_EVENT_ANY_ID");
    }

    esp_event_handler_unregister(IP_EVENT, IP_EVENT_ETH_GOT_IP, &got_ip_event_handler);
    if (ESP_OK != err_code)
    {
        LOG_ERR_ESP(err_code, "Ethernet event handler unregister failed: %s", "IP_EVENT_ETH_GOT_IP");
    }

    tcpip_adapter_clear_default_eth_handlers();
}

void
ethernet_start(void)
{
    if (NULL == g_eth_handle)
    {
        return;
    }
    LOG_INFO("Ethernet start");
    esp_err_t err_code = esp_eth_start(g_eth_handle);
    if (ESP_OK != err_code)
    {
        LOG_ERR_ESP(err_code, "Ethernet start failed");
    }
}

void
ethernet_stop(void)
{
    if (NULL == g_eth_handle)
    {
        return;
    }
    LOG_INFO("Ethernet stop");
    esp_err_t err_code = esp_eth_stop(g_eth_handle);
    if (ESP_OK != err_code)
    {
        LOG_ERR_ESP(err_code, "Ethernet stop failed");
    }
}
