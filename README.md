# Ruuvi Gateway ESP32 firmware

Developed with:
* ESP-IDF version [v4.3.1](https://github.com/espressif/esp-idf/releases/tag/v4.3.1)
* ESP32-DevKitC V4
* Waveshare LAN8720 ETH board
* nRF52832 devkit

or Ruuvi Gateway boards.

### Features

Latest documentation and roadmap is at https://docs.ruuvi.com/gw-esp32-firmware . 

### IO pins for devkit:
| ESP32  |   Function   |
|--------|--------------|
| GPIO35 | Reset button |
| 4 | UART TX |
| 5 | UART RX |

### nRF52832 devkit
| ESP32  | nRF52832 devkit |     Function     |
|--------|-----------------|------------------|
| GPIO5  | P0.11           | UART: ESP -> NRF |
| GPIO36 | P0.31           | UART: NRF -> ESP |
| GPIO15 | Debug In: 4     | SWD CLK          |
| GPIO16 | Debug In: 2     | SWD I/O          |
| GPIO17 | Debug In: 10    | SWD NRST         |
| 3V3    | Debug In: 1     | VCC +3.3V        |
| GND    | Debug In: 3     | GND              |

### Ethernet:
| ESP32 | LAN87210 |
|----|---|
|  0 | nINT/REFCLK |
| 23 | NC/CLOCK_ENABLE* |
| 15 | MDC |
| 18 | MDIO |
| 19 | TX0 |
| 21 | TX_EN |
| 22 | TX1 |
| 25 | RX0 |
| 26 | RX1 |
| 27 | CRS/RX_DV |

** ESP32 will use external clock signal from LAN8720 and some modifications are needed for that: **
* [WaveShare LAN8720 modification for clock](https://sautter.com/blog/ethernet-on-esp32-using-lan8720/)
* Remove capacitor C15 from onboard Boot button (SW1) to make clock signal work from LAN8720

### nRF52 firmware
nRF52 firmware can run on PCA10040/nRF52832 or on nRF52811 on Ruuvi Gateway. You can find the latest firmware at https://github.com/ruuvi/ruuvi.gateway_nrf.c. 

### Build and Flash

Run `git submodule update --init --recursive` to get updated components. 
Project configuration is stored in sdkconfig-file, you don't need to reconfigure it. 
Build the project and flash it to the board, then run monitor tool to view serial output:

```
idf.py build
idf.py -p PORT flash monitor
```

(To exit the serial monitor, type ``Ctrl-]``.)

See the [Getting Started Guide](https://docs.espressif.com/projects/esp-idf/en/v4.3.1/esp32/get-started/index.html) for full steps to configure and use ESP-IDF to build projects. Please note that you need to install specific ESP-IDF version, current version is at the top of the README. For example `git clone -b v4.3.1 --recursive git@github.com:espressif/esp-idf.git`

### Using prebuilt images
Prebuilt images for development versions of firmware can be found at [Ruuvi Jenkins](https://jenkins.ruuvi.com/job/ruuvi_gateway_esp-PR/). However, these are artifacts of internal development, if you want to just use the gateway you should use [release](https://github.com/ruuvi/ruuvi.gateway_esp.c/releases).

You can flash them with esptool.py. You might need to install CH340 drivers using [these instructions](https://learn.sparkfun.com/tutorials/how-to-install-ch340-drivers#drivers-if-you-need-them)
```
pip install esptool
esptool.py -p (PORT) -b 460800 --before default_reset --after hard_reset --chip esp32  write_flash --flash_mode dio --flash_size detect --flash_freq 40m 0x1000 bootloader.bin 0x8000 partition-table.bin 0xd000 ota_data_initial.bin 0x100000 ruuvi_gateway_esp.bin 0x500000 fatfs_gwui.bin 0x5C0000 fatfs_nrf52.bin 0xB00000 gw_cfg_def.bin
```

or using the Windows GUI tool `Flash Download Tools` ([download](https://www.espressif.com/en/support/download/other-tools)) with these settings:
![alt text](docs/guiflasher.png "Bootloader 0x1000, partition table 0x8000, ota_data_initial 0xD000, ruuvi_gateway_esp 0x100000, fatfs_gwui 0x500000, fatfs_nrf52 0x5C0000, gw_cfg_def 0xB00000")


### Creating a custom default gateway configuration

1. Create copy of "gw_cfg_default" folder, modify gw_cfg_default.json as you require.
2. Execute command to generate binary gw_cfg_def.bin:
```
python $IDF_PATH/components/nvs_flash/nvs_partition_generator/nvs_partition_gen.py generate gw_cfg_def_partition.csv gw_cfg_def.bin 0x40000
```
