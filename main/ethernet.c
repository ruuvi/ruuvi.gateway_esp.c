/**
 * @file ethernet.c
 * @author Jukka Saari
 * @date 2020-01-27
 * @copyright Ruuvi Innovations Ltd, license BSD-3-Clause.
 */

#include "ethernet.h"
#include <stdio.h>
#include <string.h>
#include "driver/gpio.h"
#include "esp_err.h"
#include "esp_eth.h"
#include "esp_eth_com.h"
#include "esp_event.h"
#include "esp_task_wdt.h"
#include "mqtt.h"
#include "ruuvi_gateway.h"
#include "esp_netif.h"
#include "event_mgr.h"
#include "gw_status.h"

#define LOG_LOCAL_LEVEL LOG_LEVEL_DEBUG
#include "log.h"

#define LAN_CLOCK_ENABLE 2
#define ETH_PHY_ADDR     0
#define ETH_MDC_GPIO     23
#define ETH_MDIO_GPIO    18
#define ETH_PHY_RST_GPIO -1 // disabled

#define SW_RESET_TIMEOUT_MS 500

// Cloudfare public DNS
const char* dns_fallback_server = "1.1.1.1";

static const char* TAG = "ETH";

static esp_eth_handle_t            g_eth_handle;
static ethernet_cb_link_up_t       g_ethernet_link_up_cb;
static ethernet_cb_link_down_t     g_ethernet_link_down_cb;
static ethernet_cb_connection_ok_t g_ethernet_connection_ok_cb;

static void
eth_on_event_connected(esp_eth_handle_t eth_handle)
{
    mac_address_bin_t mac_bin = { 0 };
    esp_eth_ioctl(eth_handle, ETH_CMD_G_MAC_ADDR, &mac_bin.mac[0]);
    LOG_INFO("### Ethernet Link Up");
    const mac_address_str_t mac_str = mac_address_to_str(&mac_bin);
    LOG_INFO("Ethernet HW Addr %s", mac_str.str_buf);
    g_ethernet_link_up_cb();
}

/** Event handler for Ethernet events */
static void
eth_event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data)
{
    (void)arg;
    (void)event_base;
    /* we can get the ethernet driver handle from event data */
    esp_eth_handle_t eth_handle = *(esp_eth_handle_t*)event_data;

    switch (event_id)
    {
        case ETHERNET_EVENT_CONNECTED:
            eth_on_event_connected(eth_handle);
            break;

        case ETHERNET_EVENT_DISCONNECTED:
            LOG_INFO("### Ethernet Link Down");
            g_ethernet_link_down_cb();
            break;

        case ETHERNET_EVENT_START:
            LOG_INFO("### Ethernet Started");
            break;

        case ETHERNET_EVENT_STOP:
            LOG_INFO("### Ethernet Stopped");
            event_mgr_notify(EVENT_MGR_EV_ETH_DISCONNECTED);
            break;

        default:
            break;
    }
}

/** Event handler for IP_EVENT_ETH_GOT_IP */
static void
got_ip_event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data)
{
    (void)arg;
    (void)event_base;
    (void)event_id;
    const ip_event_got_ip_t*   p_event   = (ip_event_got_ip_t*)event_data;
    const esp_netif_ip_info_t* p_ip_info = &p_event->ip_info;
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
    esp_netif_t* const p_netif_eth = esp_netif_get_handle_from_ifkey("ETH_DEF");
    LOG_INFO("Using ETH DHCP");
    const esp_err_t ret = esp_netif_dhcpc_start(p_netif_eth);
    if (ESP_OK != ret)
    {
        LOG_ERR_ESP(ret, "%s failed", "esp_netif_dhcpc_start");
        return false;
    }
    return true;
}

static bool
eth_netif_dhcpc_stop(void)
{
    esp_netif_t* const p_netif_eth = esp_netif_get_handle_from_ifkey("ETH_DEF");
    const esp_err_t    ret         = esp_netif_dhcpc_stop(p_netif_eth);
    if (ESP_OK != ret)
    {
        if ((ESP_ERR_ESP_NETIF_DHCP_ALREADY_STOPPED) == ret)
        {
            LOG_WARN("DHCP client already stopped");
        }
        else
        {
            LOG_ERR_ESP(ret, "%s failed", "esp_netif_dhcpc_stop");
            return false;
        }
    }
    return true;
}

static void
eth_netif_set_dns_info(const char* p_dns_ip, const esp_netif_dns_type_t type)
{
    esp_netif_t* const p_netif_eth = esp_netif_get_handle_from_ifkey("ETH_DEF");
    const char*        dns_server  = "Undef";
    switch (type)
    {
        case ESP_NETIF_DNS_MAIN:
            dns_server = "DNS1";
            break;
        case ESP_NETIF_DNS_BACKUP:
            dns_server = "DNS2";
            break;
        case ESP_NETIF_DNS_FALLBACK:
            dns_server = "DNS_FALLBACK";
            break;
        default:
            break;
    }

    esp_netif_dns_info_t dns_info = {
        .ip = {
            .u_addr = {
                .ip4 = {
                    .addr = 0,
                },
            },
            .type = ESP_IPADDR_TYPE_V4,
        },
    };

    dns_info.ip.u_addr.ip4.addr = esp_ip4addr_aton(p_dns_ip);
    if (0 == dns_info.ip.u_addr.ip4.addr)
    {
        LOG_ERR("Set DNS info failed for %s, invalid server: %s", dns_server, p_dns_ip);
        return;
    }
    const esp_err_t ret = esp_netif_set_dns_info(p_netif_eth, type, &dns_info);
    if (ESP_OK != ret)
    {
        LOG_ERR_ESP(ret, "%s failed for DNS: '%s'", "esp_netif_set_dns_info", dns_server);
        return;
    }
}

static bool
ethernet_update_ip_static(const gw_cfg_eth_t* const p_gw_cfg_eth)
{
    esp_netif_ip_info_t ip_info = { 0 };

    LOG_INFO("Using static IP");

    ip_info.ip.addr = esp_ip4addr_aton(p_gw_cfg_eth->eth_static_ip.buf);
    if (0 == ip_info.ip.addr)
    {
        LOG_ERR("Invalid eth static ip: %s", p_gw_cfg_eth->eth_static_ip.buf);
        return false;
    }

    ip_info.netmask.addr = esp_ip4addr_aton(p_gw_cfg_eth->eth_netmask.buf);
    if (0 == ip_info.netmask.addr)
    {
        LOG_ERR("invalid eth netmask: %s", p_gw_cfg_eth->eth_netmask.buf);
        return false;
    }

    ip_info.gw.addr = esp_ip4addr_aton(p_gw_cfg_eth->eth_gw.buf);
    if (0 == ip_info.gw.addr)
    {
        LOG_ERR("invalid eth gw: %s", p_gw_cfg_eth->eth_gw.buf);
        return false;
    }

    if (!eth_netif_dhcpc_stop())
    {
        return false;
    }

    esp_netif_t* const p_netif_eth = esp_netif_get_handle_from_ifkey("ETH_DEF");
    const esp_err_t    ret         = esp_netif_set_ip_info(p_netif_eth, &ip_info);
    if (ESP_OK != ret)
    {
        LOG_ERR_ESP(ret, "%s failed", "esp_netif_set_ip_info");
        return false;
    }

    eth_netif_set_dns_info(p_gw_cfg_eth->eth_dns1.buf, ESP_NETIF_DNS_MAIN);
    eth_netif_set_dns_info(p_gw_cfg_eth->eth_dns2.buf, ESP_NETIF_DNS_BACKUP);
    return true;
}

void
ethernet_update_ip(void)
{
    const gw_cfg_t* p_gw_cfg = gw_cfg_lock_ro();
    if (p_gw_cfg->eth_cfg.eth_dhcp)
    {
        gw_cfg_unlock_ro(&p_gw_cfg);
        if (!ethernet_update_ip_dhcp())
        {
            return;
        }
    }
    else
    {
        if (!ethernet_update_ip_static(&p_gw_cfg->eth_cfg))
        {
            gw_cfg_unlock_ro(&p_gw_cfg);
            return;
        }
    }
    gw_cfg_unlock_ro(&p_gw_cfg);

    // set DNS fallback also for DHCP settings
    eth_netif_set_dns_info(dns_fallback_server, ESP_NETIF_DNS_FALLBACK);

    LOG_INFO("ETH ip updated");
}

bool
ethernet_init(
    ethernet_cb_link_up_t       ethernet_link_up_cb,
    ethernet_cb_link_down_t     ethernet_link_down_cb,
    ethernet_cb_connection_ok_t ethernet_connection_ok_cb)
{
    LOG_INFO("### Ethernet init");

    g_ethernet_link_up_cb       = ethernet_link_up_cb;
    g_ethernet_link_down_cb     = ethernet_link_down_cb;
    g_ethernet_connection_ok_cb = ethernet_connection_ok_cb;
    gpio_set_direction(LAN_CLOCK_ENABLE, GPIO_MODE_OUTPUT);
    gpio_set_level(LAN_CLOCK_ENABLE, 1);
    esp_netif_init();

    const esp_netif_config_t netif_cfg   = ESP_NETIF_DEFAULT_ETH();
    esp_netif_t* const       p_netif_eth = esp_netif_new(&netif_cfg);
    if (NULL == p_netif_eth)
    {
        LOG_ERR("%s failed", "esp_netif_new");
        return false;
    }
    esp_err_t err_code = esp_eth_set_default_handlers(p_netif_eth);
    if (ESP_OK != err_code)
    {
        LOG_ERR_ESP(err_code, "%s failed", "esp_eth_set_default_handlers");
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

    LOG_INFO("ethernet_update_ip");
    ethernet_update_ip();

    eth_mac_config_t mac_config = ETH_MAC_DEFAULT_CONFIG();
    mac_config.rx_task_stack_size = 3072U;
    eth_phy_config_t phy_config = ETH_PHY_DEFAULT_CONFIG();

    phy_config.phy_addr       = ETH_PHY_ADDR;
    phy_config.reset_gpio_num = ETH_PHY_RST_GPIO;

    mac_config.smi_mdc_gpio_num    = ETH_MDC_GPIO;
    mac_config.smi_mdio_gpio_num   = ETH_MDIO_GPIO;
    mac_config.sw_reset_timeout_ms = SW_RESET_TIMEOUT_MS;

    esp_eth_mac_t*   mac    = esp_eth_mac_new_esp32(&mac_config);
    esp_eth_phy_t*   phy    = esp_eth_phy_new_lan8720(&phy_config);
    esp_eth_config_t config = ETH_DEFAULT_CONFIG(mac, phy);
    LOG_INFO("esp_eth_driver_install");
    err_code = esp_eth_driver_install(&config, &g_eth_handle);
    if (ESP_OK != err_code)
    {
        LOG_ERR_ESP(err_code, "Ethernet driver install failed");
        return false;
    }

    void* p_eth_glue = esp_eth_new_netif_glue(g_eth_handle);
    if (NULL == p_eth_glue)
    {
        LOG_ERR_ESP(err_code, "esp_eth_new_netif_glue failed");
        return false;
    }

    err_code = esp_netif_attach(p_netif_eth, p_eth_glue);
    if (ESP_OK != err_code)
    {
        LOG_ERR_ESP(err_code, "esp_netif_attach failed");
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
}

void
ethernet_start(void)
{
    const wifiman_hostname_t* const p_hostname = gw_cfg_get_hostname();
    if (NULL == g_eth_handle)
    {
        return;
    }
    LOG_INFO("### Ethernet start");

    // esp_eth_start can take up to 4 seconds
    // (see initialization of autonego_timeout_ms in macro ETH_PHY_DEFAULT_CONFIG, which is used in ethernet_init),
    // task watchdog is configured to 5 seconds, and the feeding period of the task watchdog is 1 second,
    // so to prevent the task watchdog from triggering, we must feed it before and after calling esp_eth_start.

    esp_task_wdt_reset();

    esp_err_t err = esp_eth_start(g_eth_handle);

    esp_task_wdt_reset();

    if (ESP_OK != err)
    {
        LOG_ERR_ESP(err, "Ethernet start failed");
    }

    esp_netif_t* const p_netif_eth = esp_netif_get_handle_from_ifkey("ETH_DEF");

    LOG_INFO("### Set hostname for Ethernet interface: %s", p_hostname->buf);
    err = esp_netif_set_hostname(p_netif_eth, p_hostname->buf);
    if (ESP_OK != err)
    {
        LOG_ERR_ESP(err, "%s failed", "esp_netif_set_hostname");
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
    gw_status_clear_eth_connected();
    esp_err_t err_code = esp_eth_stop(g_eth_handle);
    if (ESP_OK != err_code)
    {
        LOG_ERR_ESP(err_code, "Ethernet stop failed");
    }
}
