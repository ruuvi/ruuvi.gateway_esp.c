# Gateway Configuration

Here you can find a complete Ruuvi Gateway configuration guide for advanced users.

## Current firmware version information

Firmware version information can be found at the bottom of each page:

![start_page](images/start_page.png)

There are two version numbers:
* Ruuvi Gateway firmware version (v1.15.0). [GitHub repo](https://github.com/ruuvi/ruuvi.gateway_esp.c)
* Ruuvi Gateway nRF52 co-processor firmware version (v1.0.0) [GitHub repo](https://github.com/ruuvi/ruuvi.gateway_nrf.c)


## Connect Ruuvi Gateway to the Internet

### Ethernet – Advanced options

If you want to configure your Ruuvi Gateway to use a static IP address 
please uncheck **Use DHCP** after selecting the Ethernet connection option.  
The following fields are required for this configuration:

* **Static IP**
* **Netmask**
* **Gateway**
* **DNS IP 1**
* **DNS IP 2** (optional)

![ethernet_advanced_options](images/ethernet_advanced_options.png)

### Wi-Fi – Advanced Options

A hidden Wi-Fi network can be used for connecting your Ruuvi Gateway to internet. 
This setting is shown in the Wi-Fi Network configuration page under Advanced.

Select **Connect Manually** to see the following required fields:

* **SSID**
* **Password**

![wifi_advanced_options](images/wifi_advanced_options.png)

## Firmware update

It is always recommended to keep your Ruuvi Gateway up-to-date.

Your firmware version and update availability will be checked on the **Software Update** page 
during the configuration process. 
If an update is available, you will be offered to install it from the default location 
(recommended).

Alternatively you can configure your own update server, which should return a JSON file containing 
the version number and URL from where the firmware binaries can be downloaded, see example:
```json
{
  "latest": {
    "version":"v1.15.0",
    "url":"https://fwupdate.ruuvi.com/v1.15.0",
    "created_at":"2023-12-12T12:00:00Z"
  }
}
```

This option is available under **Advanced**:

![software_update_advanced_options1](images/software_update_advanced_options1.png)

Or, you can choose your own location for the firmware binaries by checking **Don't use the software 
update provided by Ruuvi but download binary files from URL address instead** under **Advanced**:

![software_update_advanced_options](images/software_update_advanced_options2.png)

If you want to use a previous release version, you can provide a link 
to GitHub `https://github.com/ruuvi/ruuvi.gateway_esp.c/releases/download/<VERSION_TAG>` 
or to the Ruuvi update server `https://fwupdate.ruuvi.com/<VERSION_TAG>`
where you need to replace **<VERSION_TAG>** with the version number you need, for example "v1.15.0".

For example: `https://github.com/ruuvi/ruuvi.gateway_esp.c/releases/download/v1.13.0`

Here you will find a list of the releases available for download: 
https://github.com/ruuvi/ruuvi.gateway_esp.c/releases

If you want to use a developer build, you can provide a link to the Jenkins build server
`https://jenkins.ruuvi.com/job/ruuvi_gateway_esp-PR/<BUILD>/artifact/build/`
where you need to replace **<BUILD>** with the build number you need.

For example: `https://jenkins.ruuvi.com/job/ruuvi_gateway_esp-PR/805/artifact/build/`

To start the firmware update process press the *Update* button.  
The firmware update process takes a few minutes,and you will see the progress on the screen.
It's important to make sure that your Ruuvi Gateway remains powered during this time.

![software_update_progress](images/software_update_progress.png)

You can also skip this step by pressing *Continue without update*.

![software_update](images/software_update.png)

## Automatic updates

Your Ruuvi Gateway has the ability to automatically check for and install new firmware updates 
using a scheduler. We recommend that you select **Auto Update**, which accepts new versions for 
installation 2 weeks after the release date.

Advanced users can also select a special **Auto Update for beta-testers** mode that accepts new 
firmware versions and automatically installs them as they become available.

Selecting **Manual updates only** disables automatic firmware updates for your gateway.

![automatic_updates_select_mode](images/automatic_updates_select_mode.png)

The automatic update scheduler allows you to specify when you want automatic updates to be 
fetched and installed. You can schedule updates to be checked by:

* **Weekdays**
* **Timeslot**
* **Time zone**

![automatic_updates_advanced_options](images/automatic_updates_advanced_options.png)

## Automatic Configuration Download

You can configure the gateway to download its configuration from a remote server (checking for 
an updated configuration will be performed with a set period).

You just need to specify the base URL from where the configuration file with the Gateway settings 
can be downloaded. 

If you specify a folder name as the base URL (any URL that does not end with '.json'), it will first
attempt to read the configuration file for the Gateway from <GW_MAC>.json (e.g. AABBCCDDEEFF.json).
If this file fails, it will attempt to read the overall configuration from gw_cfg.json.

The configuration file can also be generated dynamically by the server, which requires the server to
analyse the HTTP header 'ruuvi_gw_mac', which contains the MAC address in the format XX:XX:XX:XX:XX:
XX.

After entering the base URL, press the Check button to validate the URL and check that the
configuration file exists. Next, after pressing the Download button, the new configuration will be
downloaded, which completes the configuration process. After that, Ruuvi Gateway will periodically
check for configuration updates and download them. The polling period is set in the configuration
file, which is downloaded from the server.

![automatic_configuration_download_advanced_options](images/automatic_configuration_download_advanced_options.png)

The gateway configuration file on the remote server must contain at least the following attributes:
* **remote_cfg_use**: true
* **remote_cfg_url**: URL
* **remote_cfg_refresh_interval_minutes**: a period of checking for an updated configuration (in 
  minutes)
* **remote_cfg_auth_type**: authentication type ('**no**' if authentication is not required)

Example of minimal gw_cfg.json:
```json
{
  "remote_cfg_use": true, 
  "remote_cfg_url": "http://192.168.1.101:7000/", 
  "remote_cfg_refresh_interval_minutes": 10,
  "remote_cfg_auth_type": "no" 
}
```

All configuration attributes not specified in the configuration file will retain their previous 
value after the new configuration is loaded from the remote server.

Configuration downloads with basic HTTP authentication are also supported:

![automatic_configuration_download_auth_basic](images/automatic_configuration_download_auth_basic.png)

Example of corresponding minimal gw_cfg.json:
```json
{
  "remote_cfg_use": true, 
  "remote_cfg_url": "http://192.168.1.101:7000/", 
  "remote_cfg_refresh_interval_minutes": 10,
  "remote_cfg_auth_type": "basic",
  "remote_cfg_auth_basic_user": "user1",
  "remote_cfg_auth_basic_pass": "password1"
}
```

Or bearer authentication (using a token):

![automatic_configuration_download_auth_bearer](images/automatic_configuration_download_auth_bearer.png)

Example of corresponding minimal gw_cfg.json:
```json
{
  "remote_cfg_use": true, 
  "remote_cfg_url": "http://192.168.1.101:7000/", 
  "remote_cfg_refresh_interval_minutes": 10,
  "remote_cfg_auth_type": "bearer",
  "remote_cfg_auth_bearer_token": "my_secret_token_123"
}
```

It also supports client authentication via SSL by enabling the upload of a client certificate and 
its associated private key, ensuring secure and verified client-server communication:

![automatic_configuration_download_client_ssl_cert](images/automatic_configuration_download_client_ssl_cert.png)

Example of corresponding minimal gw_cfg.json:
```json
{
  "remote_cfg_use": true, 
  "remote_cfg_url": "http://192.168.1.101:7000/", 
  "remote_cfg_refresh_interval_minutes": 10,
  "remote_cfg_auth_type": "none",
  "remote_cfg_use_ssl_client_cert": true
}
```

You can use a server SSL Certificate if you want to be independent of 
public Certificate Authorities (CAs) or if you have deployed a self-signed certificate 
on the HTTPS server, giving you greater control and customization over your security infrastructure:

![automatic_configuration_download_server_ssl_cert](images/automatic_configuration_download_server_ssl_cert.png)

Example of corresponding minimal gw_cfg.json:
```json
{
  "remote_cfg_use": true, 
  "remote_cfg_url": "http://192.168.1.101:7000/", 
  "remote_cfg_refresh_interval_minutes": 10,
  "remote_cfg_auth_type": "none",
  "remote_cfg_use_ssl_server_cert": true
}
```

## LAN Access Settings

You can restrict or allow access to your Ruuvi Gateway from the local network.

By default, the access is **Password protected** using the default password (unique device ID which 
is printed on the bottom of the Ruuvi Gateway):

![access_settings_default](images/access_settings_default.png)

If you want to configure a custom **Username** and **password**, select **Protected with a 
custom password** (under **Advanced settings**):

![access_settings_custom_password](images/access_settings_custom_password.png)

You can also prohibit access by password by selecting **No remote configurable** or allow access 
without a password by selecting **Remote configurable without a password**.

It is also possible to configure access to the Ruuvi Gateway from LAN using bearer 
authentication (under **Advanced settings**):

![access_settings_advanced_options](images/access_settings_advanced_options.png)

If you set a token for read-only access, you can read data from the "/history" and 
"/metrics" endpoints, or read the gateway configuration. To be able to configure the gateway 
programmatically, a full (read/write) access token must be enabled.

Examples:
* Request "/history" using read-only bearer token:
    ```shell
    curl -v http://<RUUVI_GW_IP>/history 
      -H "Authorization: Bearer Uj+4tj24unVekco/lTLTRyxUfv1J8M6U+sbNsKTWRr0="
    ```
* Request "/metrics" using read-only bearer token:
    ```shell
    curl -v http://<RUUVI_GW_IP>/metrics 
      -H "Authorization: Bearer Uj+4tj24unVekco/lTLTRyxUfv1J8M6U+sbNsKTWRr0="
    ```
* Download the gateway configuration using the read-only access bearer token and save it to file 
  'ruuvi.json':
    ```shell
    curl -v http://<RUUVI_GW_IP>/ruuvi.json 
      -H "Authorization: Bearer Uj+4tj24unVekco/lTLTRyxUfv1J8M6U+sbNsKTWRr0=" 
      --output ruuvi.json
    ```
* Upload the gateway configuration using the read-write access bearer token 
  from the file './ruuvi.json' (note that the network part of the configuration will be ignored):
    ```shell
    curl -v http://<RUUVI_GW_IP>/ruuvi.json 
      -H "Authorization: Bearer 1SDrQH1FkH+pON0GsSjt2gYeMSP02uYqfuu7LWdaBvY=" 
      -d @./ruuvi.json 
    ```
* Starting firmware update from URL:
    ```shell
    curl -v http://<RUUVI_GW_IP>/fw_update.json 
      -H "Authorization: Bearer 1SDrQH1FkH+pON0GsSjt2gYeMSP02uYqfuu7LWdaBvY=" 
      -H 'Content-Type: application/json' 
      -d '{"url":"https://my_server.com:7000/gw_firmware/"}'
    ```
* A forced download of gateway configuration from the remote server (you must pre-configure the 
  gateway and set the remote server URL):
    ```shell
    curl -v http://<RUUVI_GW_IP>/gw_cfg_download 
      -H "Authorization: Bearer 1SDrQH1FkH+pON0GsSjt2gYeMSP02uYqfuu7LWdaBvY=" -d ''
    ```
  
## Custom HTTP(S) / MQTT(S) sever

Ruuvi Gateway connects to the Ruuvi Cloud by default. However, you can configure it to use an 
HTTP(S) or MQTT server to route data from your nearby sensors to a custom location instead.

Ruuvi Gateway supports client authentication via SSL for HTTPS and MQTTS by enabling the upload 
of a client certificate and its associated private key.

Also, you can use a server SSL Certificate if you want to be independent of public Certificate 
Authorities (CAs) or if you have deployed a self-signed certificate on the HTTPS/MQTTS server.

![cloud_options_advanced_settings](images/cloud_options_advanced_settings.png)

You can enable or disable sending data to Ruuvi Cloud:

![custom_server_ruuvi_cloud](images/custom_server_ruuvi_cloud.png)

Or configure to send data to your own server via HTTP/HTTPS (you can configure sending to both 
Ruuvi Cloud and your own server):

![custom_server_http](images/custom_server_http.png)

You can choose the data format for sending to your own server:
* Raw data without decoding (default)
* Raw data and decoded data
* Decoded data only

**Note**: Decoding is supported only for Ruuvi sensors.

It is possible to set initial sending interval for HTTP(S) server (in seconds).
But this value can be overridden by the server by sending "X-Ruuvi-Gateway-Rate" 
in the HTTP response header.

Also, you can enable relaying data to MQTT server:

![custom_server_mqtt](images/custom_server_mqtt.png)

You can choose the data format for sending to your own server:
* Raw data without decoding (default)
* Raw data and decoded data
* Decoded data only
 
**Note**: Decoding is supported only for Ruuvi sensors.

In case the data flow from the sensors is too large, you can set the period with which the data
will be sent over MQTT by defining sending interval.

In the Statistics section, you can configure the sending of statistics to Ruuvi Cloud or your own
server, or disable the sending of statistics:

![custom_server_statistics](images/custom_server_statistics.png)

## Time Synchronization Options

If the default set of NTP servers is not accessible, then you can specify the addresses of other NTP
servers or use DHCP to obtain a list of NTP servers automatically. If the Ruuvi Gateway does not
have access to the Internet and NTP, you can disable time synchronisation, but in this case, relayed
messages will not contain timestamps.

To configure NTP servers you need to enable advanced settings in the **Cloud Options**:

![cloud_options_advanced_settings](images/cloud_options_advanced_settings.png)

After that, you will be able to choose how to synchronise the time:

![time_sync_default](images/time_sync_default.png)

![time_sync_custom](images/time_sync_custom.png)

## Bluetooth Scanning Settings

To get access to these settings you need to enable advanced settings in the **Cloud Options**:

![cloud_options_advanced_settings](images/cloud_options_advanced_settings.png)

By default, Ruuvi Gateway scans only for Ruuvi sensors (filtered by BLE SIG member ID 0x0499)

![bluetooth_scanning_default](images/bluetooth_scanning_default.png)

Also, you can enable Bluetooth long range (also known as Coded PHY).
It is a new mode introduced in Bluetooth Version 5.0 to extend the range of Bluetooth devices from
30-100 feet to ranges of 1 kilometer and beyond.

**Note**: Most existing devices only transmit on 1 Mbps PHY. 1 Mbps and Coded PHYs modulations are
scanned sequentially, so scanning both PHYs will result in at least 50% packet loss on one of the
PHYs.

![bluetooth_scanning_ruuvi_and_long_range](images/bluetooth_scanning_ruuvi_and_long_range.png)

If you want to relay data from more than just Ruuvi sensors, you need to select the "**All 
(including third party beacons)**" option and adjust the scanned PHYs, Bluetooth channels used 
and the ability to use extended payload (BLE extended advertising):

![bluetooth_scanning_all](images/bluetooth_scanning_all.png)

**Use channel**. Each enabled BLE channel is scanned sequentially for a minimum of 7000 ms per
channel, for a total of 21000 ms if all 3 channels are enabled. At least one channel must be active.

It is possible to filter out the relayed Bluetooth sensors. 
You can use whitelist mode if you only want to share data for specific sensors, 
or blacklist mode if you want to share data from all sensors except the specified list.

![bluetooth_filtering](images/bluetooth_filtering.png)

If you don't see some sensors, try to press on **Refresh list**. If a sensor is offline, 
you can add it manually by entering its MAC address and pressing on the **Add** button.
