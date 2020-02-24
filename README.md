# Ruuvi Gateway ESP32 firmware

Developed with:
* ESP-IDF version [v4.0](https://github.com/espressif/esp-idf/releases/tag/v4.0)
* ESP32-DevKitC V4
* Waveshare LAN8720 ETH board
* nRF52832 devkit

### Features

On start wifi access point is active. Access point serves a web page for setting the wifi network. It also has configuration for ethernet, where to post BLE scan results (HTTP/MQTT), filter only Ruuvitags or all BLE devices.

Ethernet is automatically used if cable is plugged in and wifi will be turned off. To access the configuration again: unplug ethernet, hold reset button 3 seconds and device will reboot.

Needs nRF52 running the Gateway firmware and connected to UART pins.

### IO pins:

ESP32 | Function
--|--
2 | Reset button
23 | LED
4 | UART TX
5 | UART RX

### Ethernet:
ESP32 | LAN87210
-|-
0 | nINT/REFCLK
16 | NC/CLOCK_ENABLE*
17 | MDC
18 | MDIO
19 | TX0
21 | TX_EN
22 | TX1
25 | RX0
26 | RX1
27 | CRS/RX_DV

*ESP32 will use external clock signal from LAN8720 and some modifications are needed for that:

* [WaveShare LAN8720 modification for clock](https://sautter.com/blog/ethernet-on-esp32-using-lan8720/)
* Remove capacitor C15 from onboard Boot button (SW1) to make clock signal work from LAN8720

### Configure the project

```
idf.py menuconfig
```

* Set serial port under Serial Flasher Options.

### Build and Flash

Build the project and flash it to the board, then run monitor tool to view serial output:

```
idf.py -p PORT flash monitor
```

(To exit the serial monitor, type ``Ctrl-]``.)

See the Getting Started Guide for full steps to configure and use ESP-IDF to build projects.
