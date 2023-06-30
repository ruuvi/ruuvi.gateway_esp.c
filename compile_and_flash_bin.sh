#!/bin/bash

set -x -e

cd build
ninja ruuvi_gateway_esp.elf
ninja .bin_timestamp

cd ..

esptool.py -p /dev/ttyUSB0 -b 460800 --before default_reset --after hard_reset --chip esp32  write_flash --flash_mode dio --flash_size detect --flash_freq 40m 0xd000 build/ota_data_initial.bin 0x100000 build/ruuvi_gateway_esp.bin

