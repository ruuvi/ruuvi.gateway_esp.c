# Ruuvi Gateway ESP32 firmware

### Features

On start wifi access point is active. Access point serves a web page for setting the wifi network. It also has configuration for ethernet, where to post BLE scan results (HTTP/MQTT), filter only Ruuvitags or all BLE devices.

Ethernet is automatically used if cable is plugged in and wifi will be turned off. To access the configuration again: unplug ethernet, hold reset button 3 seconds and device will reboot.

Needs nRF52 running and connected to UART pins.

IO pins:
```
#define LED_PIN 23
```
Uart:
```
#define TXD_PIN (GPIO_NUM_4)
#define RXD_PIN (GPIO_NUM_5)
```
Ethernet:
```
#define ETH_MDC_GPIO 17
#define ETH_MDIO_GPIO 18
RESET_BUTTON 2
RMII clock input (nINT/REFCLK) 0
LAN_CLOCK_ENABLE 16
TX0 19
TX1 22
TX_EN 21
RX0 25
RX1 26
CRS/RX_DV 27
```

### Configure the project

```
idf.py menuconfig
```

* Set serial port under Serial Flasher Options.

### Build and Flash

Developed with ESP_IDF version v4.0-rc

Build the project and flash it to the board, then run monitor tool to view serial output:

```
idf.py -p PORT flash monitor
```

(To exit the serial monitor, type ``Ctrl-]``.)

See the Getting Started Guide for full steps to configure and use ESP-IDF to build projects.
