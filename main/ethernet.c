#include <stdio.h>
#include <string.h>
#include "esp_err.h"
#include "esp_eth_com.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "leds.h"
#include "tcpip_adapter.h"
#include "esp_eth.h"
#include "esp_event.h"
#include "esp_log.h"
#include "driver/gpio.h"
#include "sdkconfig.h"
#include "ethernet.h"
#include "ruuvidongle.h"
#include "wifi_manager.h"
#include "mqtt.h"
#include "time_task.h"
#define LOG_LOCAL_LEVEL ESP_LOG_DEBUG


#define ETH_PHY_ADDR 1
#define ETH_MDC_GPIO 17
#define ETH_MDIO_GPIO 18
#define ETH_PHY_RST_GPIO -1 //disabled

// Cloudfare public DNS
const char *dns_fallback_server = "1.1.1.1";

static const char *TAG = "eth";

/** Event handler for Ethernet events */
static void eth_event_handler(void *arg, esp_event_base_t event_base,
							  int32_t event_id, void *event_data)
{
	uint8_t mac_addr[6] = {0};
	/* we can get the ethernet driver handle from event data */
	esp_eth_handle_t eth_handle = *(esp_eth_handle_t *)event_data;

	switch (event_id) {
	case ETHERNET_EVENT_CONNECTED:
		esp_eth_ioctl(eth_handle, ETH_CMD_G_MAC_ADDR, mac_addr);
		ESP_LOGI(TAG, "Ethernet Link Up");
		ESP_LOGI(TAG, "Ethernet HW Addr %02x:%02x:%02x:%02x:%02x:%02x",
				 mac_addr[0], mac_addr[1], mac_addr[2], mac_addr[3], mac_addr[4], mac_addr[5]);
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
static void got_ip_event_handler(void *arg, esp_event_base_t event_base,
								 int32_t event_id, void *event_data)
{
	ip_event_got_ip_t *event = (ip_event_got_ip_t *) event_data;
	const tcpip_adapter_ip_info_t *ip_info = &event->ip_info;

	ESP_LOGI(TAG, "Ethernet Got IP Address");
	ESP_LOGI(TAG, "~~~~~~~~~~~");
	ESP_LOGI(TAG, "ETHIP:" IPSTR, IP2STR(&ip_info->ip));
	ESP_LOGI(TAG, "ETHMASK:" IPSTR, IP2STR(&ip_info->netmask));
	ESP_LOGI(TAG, "ETHGW:" IPSTR, IP2STR(&ip_info->gw));
	ESP_LOGI(TAG, "~~~~~~~~~~~");

	ethernet_connection_ok_cb();
}

void ethernet_update_ip()
{
	struct dongle_config *c = &m_dongle_config;

	bool error = false;
	int ret = 0;
	if (m_dongle_config.eth_dhcp) {
		ESP_LOGI(TAG, "Using ETH DHCP");
		ret = tcpip_adapter_dhcpc_start(TCPIP_ADAPTER_IF_ETH);
		if (ret) {
			ESP_LOGE(TAG, "dhcpc start error: 0x%02x", ret);
		}
	} else {
		ESP_LOGI(TAG, "Using static IP");

		tcpip_adapter_ip_info_t ipInfo;
		memset(&ipInfo, 0, sizeof(ipInfo));
		if (!ip4addr_aton(c->eth_static_ip, &ipInfo.ip)) {
			ESP_LOGE(TAG, "invalid eth static ip: %s", c->eth_static_ip);
			error = true;
		}
		if (!ip4addr_aton(c->eth_netmask, &ipInfo.netmask)) {
			ESP_LOGE(TAG, "invalid eth netmask: %s", c->eth_netmask);
			error = true;
		}
		if (!ip4addr_aton(c->eth_gw, &ipInfo.gw)) {
			ESP_LOGE(TAG, "invalid eth gw: %s", c->eth_gw);
			error = true;
		}

		ret = tcpip_adapter_dhcpc_stop(TCPIP_ADAPTER_IF_ETH);
		if (ret == ESP_ERR_TCPIP_ADAPTER_DHCP_ALREADY_STOPPED) {
			ESP_LOGW(TAG, "DHCP client already stopped");
		} else if (ret != ESP_OK) {
			ESP_LOGE(TAG, "DHCP client stop error: 0x%02x", ret);
			error = true;
		}

		ret = tcpip_adapter_set_ip_info(TCPIP_ADAPTER_IF_ETH, &ipInfo);
		if (ret != ESP_OK || error) {
			ESP_LOGE(TAG, "Invalid ip settings for ETH, err: 0x%02x", ret);
		}

		tcpip_adapter_dns_info_t dns1;
		dns1.ip.type = IPADDR_TYPE_V4;
		if (ip4addr_aton(c->eth_dns1, (ip4_addr_t*)&dns1.ip.u_addr)) {
			ret = tcpip_adapter_set_dns_info(TCPIP_ADAPTER_IF_ETH, TCPIP_ADAPTER_DNS_MAIN, &dns1);
			if (ret) {
				ESP_LOGE(TAG, "dns1 error: 0x%02x", ret);
			}
		} else {
			ESP_LOGE(TAG, "invalid dns1 server: %s", c->eth_dns1);
		}

		tcpip_adapter_dns_info_t dns2;
		dns2.ip.type = IPADDR_TYPE_V4;
		if (ip4addr_aton(c->eth_dns2, (ip4_addr_t*)&dns2.ip.u_addr)) {
			ret = tcpip_adapter_set_dns_info(TCPIP_ADAPTER_IF_ETH, TCPIP_ADAPTER_DNS_BACKUP, &dns2);
			if (ret) {
				ESP_LOGE(TAG, "dns2 error: 0x%02x", ret);
			}
		} else {
			ESP_LOGE(TAG, "invalid dns2 server: %s", c->eth_dns2);
		}
	}

	//set DNS fallback also for DHCP settings
	tcpip_adapter_dns_info_t dns_fallback;
	dns_fallback.ip.type = IPADDR_TYPE_V4;
	if (ip4addr_aton(dns_fallback_server, (ip4_addr_t*)&dns_fallback.ip.u_addr)) {
		ret = tcpip_adapter_set_dns_info(TCPIP_ADAPTER_IF_ETH, TCPIP_ADAPTER_DNS_FALLBACK, &dns_fallback);
		if (ret) {
			ESP_LOGE(TAG, "%s: dns_fallback error: 0x%02x", __func__, ret);
		}
	} else {
		ESP_LOGE(TAG, "invalid dns fallback server: %s", dns_fallback_server);
	}

	if (!error) {
		ESP_LOGI(TAG, "ETH ip updated");
	}
}

void ethernet_init()
{
	esp_log_level_set(TAG, ESP_LOG_DEBUG);

	ethernet_update_ip();

	tcpip_adapter_init();

	ESP_ERROR_CHECK(tcpip_adapter_set_default_eth_handlers());

	ESP_ERROR_CHECK(esp_event_handler_register(ETH_EVENT, ESP_EVENT_ANY_ID, &eth_event_handler, NULL));
	ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_ETH_GOT_IP, &got_ip_event_handler, NULL));

	eth_mac_config_t mac_config = ETH_MAC_DEFAULT_CONFIG();
	eth_phy_config_t phy_config = ETH_PHY_DEFAULT_CONFIG();
	phy_config.phy_addr = ETH_PHY_ADDR;
	phy_config.reset_gpio_num = ETH_PHY_RST_GPIO;

	mac_config.smi_mdc_gpio_num = ETH_MDC_GPIO;
	mac_config.smi_mdio_gpio_num = ETH_MDIO_GPIO;
	esp_eth_mac_t *mac = esp_eth_mac_new_esp32(&mac_config);

	esp_eth_phy_t *phy = esp_eth_phy_new_lan8720(&phy_config);

	esp_eth_config_t config = ETH_DEFAULT_CONFIG(mac, phy);
	esp_eth_handle_t eth_handle = NULL;
	ESP_ERROR_CHECK(esp_eth_driver_install(&config, &eth_handle));
	ESP_ERROR_CHECK(esp_eth_start(eth_handle));
}