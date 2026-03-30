# IXIT 9-ReplSup: Replacement Support

The following information describes the isolation and hardware replacement procedures for the Ruuvi
Gateway (DUT).

## Table C.9: IXIT 9-ReplSup (Replacement Support)

### Isolation

To isolate the Ruuvi Gateway from the network and prevent it from affecting other systems, the
following steps are performed:

1. **Network Disconnection**: Disconnect the Ethernet cable (if used) or revoke the Gateway's 
   access credentials from the local Wi-Fi Access Point.
2. **Power Isolation**: Unplug the USB-C power cable to completely shut down the device.
3. **Logical Removal**: Remove the Gateway's unique MAC address or Client ID from the target MQTT
   broker or cloud dashboard to prevent unauthorized data injection. Once isolated, the device has
   no path to transmit sensor data or receive configuration updates, effectively neutralizing its
   presence in the ecosystem.

### Hardware Replacement

The Ruuvi Gateway is designed as a modular unit that can be replaced as a whole:

1. **Hardware Swap**: The suspected or faulty unit is replaced with a new Ruuvi Gateway unit.
2. Configuration Method:
   - **Manual Setup**: The user configures the new unit from scratch using the Web-UI 
     configuration wizard.
   - **Backup & Restore**: The user can upload a settings backup file from a previous unit.
3. Credential Handling:
   - **Wi-Fi**: For security reasons, Wi-Fi passwords are not included in the backup file.
     If the Gateway is connected via Wi-Fi, the user must manually re-enter the SSID credentials 
     before the settings restoration.
   - **Ethernet**: If the Gateway is connected via Ethernet (DHCP or Static), no additional
     credential entry is required.
4. **Unique Identity**: Each replacement unit will have its own unique hardware DEVICEID and
   corresponding unique default password, ensuring that security credentials from a previous unit
   are not reused.

### Connectivity and Functionality

Upon replacement and configuration, the new Ruuvi Gateway restores full functionality, including:
- **BLE Scanning**: Resuming the collection of data from nearby Ruuvi sensors.
- **Data Relaying**: Forwarding telemetry to the configured cloud or local endpoints (HTTP/MQTT).
- **Security Parity**: Re-establishing secure TLS tunnels (MQTTS/HTTPS) using the new unit's unique
  cryptographic identity.