#!/bin/bash
set -e
set -o pipefail

# Check for mandatory port argument
if [ -z "$1" ]; then
	echo ""
	echo "Error: Serial port argument is required."
	echo "Usage: $(basename "$0") <port>"
	exit 1
fi
PORT="$1"

# Stop ESP32 CPU to prevent interference during flashing nRF52
esptool.py --chip esp32 --port "$PORT" --after no_reset read_mem 0x40000000

echo ""
echo "ESP32 CPU has stopped."
echo "1. Please connect the unpowered J-Link to the nRF52 on Ruuvi Gateway."
echo "2. Then power on the J-Link."
echo "3. After that, press Enter to continue with flashing the nRF52 firmware."
# Wait for user confirmation to proceed
read -r
echo "Connecting to nRF52 via J-Link..."
# Erase nRF52 flash
if ! nrfjprog --recover; then
	echo ""
	echo "ERROR: nrfjprog --recover failed. Is the J-Link connected and powered? Exiting." >&2
	exit 1
fi
# Write firmware version v2.0.0 to nRF52 UICR
nrfjprog --memwr 0x10001080 --val 0x02000000
# Write the patched nRF52 firmware to the flash
JLinkExe -device NRF52811_XXAA -if SWD -speed 4000 -autoconnect 1 -CommanderScript flash_script.jlink

echo ""
echo "Flashing complete."
echo "You can now connect to $PORT to collect logs after rebooting the gateway."
echo "Please reboot the gateway manually by pressing the reset button."
echo "Or you can reboot it with the command:"
echo "esptool.py --chip esp32 --port $PORT --after hard_reset read_mem 0x40000000"
