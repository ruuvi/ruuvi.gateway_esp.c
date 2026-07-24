# IXIT 28-LogIntf: Logical Interfaces

The following declarations map the complete logical interface profile of the Ruuvi Gateway (DUT),
specifying cross-references to the physical ports, operational statuses, unauthenticated data
exposure risks, and transport layer resilience measures.

## Table C.28: IXIT 28-LogIntf (Logical Interfaces)

### **ID**: LogIntf-HTTP-Server

#### Description

The DUT hosts an internal HTTP server (Port 80) acting as the execution environment for the Web-UI
and primary local configuration wizard. It handles endpoint execution routines such as `/auth` (
authentication binds) and `/history` (sensor data logs).

#### Access

* `PhyIntf-Ethernet` (Accessible over local area network)
* `PhyIntf-WiFi` (Accessible over local wireless station link or transient provisioning hotspot)

#### Status

Enabled. Required to host the Gateway Web-UI configuration panel and allow network-layer telemetry
path setup.

#### Disclosed Information

Unauthenticated requests to the root server disclose generic structural HTTP response headers. By
default, these headers reveal only the server type platform signature (`lwIP/ESP-IDF HTTP Server`)
without disclosing specific software patch versions or framework revisions. Unauthenticated access
to system parameters or active sensor historical arrays is strictly blocked, returning standard
`401 Unauthorized` error frames. This carries low security relevance.

#### Resilience Measures

The HTTP server layer operates an incoming-only socket management paradigm backed by an LwIP
connection state queue. Session setup payloads are handled via clean token-based processing
boundaries. If unauthenticated TCP socket starvation floods occur, the interface may experience
transient response delays (denial of service mitigation state), but internal credential values,
memory spaces, and system data integrity properties are maintained within protected flash
partitions.

---

### **ID**: LogIntf-Cloud-HTTPS-Telemetry

#### Description

The DUT acts as an outbound HTTP client utilizing secure TLS sockets (`HTTPS/TCP`) to periodically
stream accumulated BLE sensor payloads to the standard cloud collection server located at
`https://network.ruuvi.com/record`.

#### Access

* `PhyIntf-Ethernet` (Remotely accessible via WAN network path)
* `PhyIntf-WiFi` (Remotely accessible via WAN network path)

#### Status

Enabled by default. Fulfills the primary device function of shipping real-world telemetry values to
remote cloud storage platforms.

#### Disclosed Information

None. Payload envelopes are wrapped completely within an outbound encrypted TLS session path,
ensuring confidentiality across intermediate routing hops.

#### Resilience Measures

The outbound communication layer utilizes the mature `mbedtls` engine, ensuring strict compliance
with proper RFC TLS state machines, ordered cipher handshakes, and strict connection reset
mechanisms. If network connectivity drops or Ruuvi Cloud throttles connections, the application
returns to its scheduled HTTP posting loop: successful responses are paced by the cloud-provided
`X-Ruuvi-Gateway-Rate` value of 60 seconds, while failed posts use the fixed 67-second retry period
declared under `ResMech-Net-Telemetry-Protocol-Reconnection`. The one-hour network watchdog provides
last-resort recovery for rare stuck states without relying on synchronized fleet-wide reconnects.

---

### **ID**: LogIntf-Custom-HTTP-Telemetry

#### Description

User-configurable outbound telemetry target client dedicated to sending sensor datasets via standard
HTTP POST or encrypted HTTPS POST endpoints. This process can run concurrently and independently of
other telemetry loops.

#### Access

* `PhyIntf-Ethernet` (Remotely accessible via network path)
* `PhyIntf-WiFi` (Remotely accessible via network path)

#### Status

Disabled by default. Activated strictly when the administrator configures a custom HTTP/HTTPS
destination URL via the Web-UI settings panel.

#### Disclosed Information

None if encrypted transport (HTTPS) is configured by the user. If the operator explicitly configures
a cleartext HTTP destination on their network, sensor payload matrices are transmitted unencrypted
on the wire.

#### Resilience Measures

Governed by independent application-layer connection loops. Failed posts cleanly release the HTTP
client state and switch the affected periodic timer to the fixed 67-second retry delay, without
interrupting concurrent streams. If all configured telemetry paths remain unable to refresh
successful network activity for one hour, the network watchdog restarts the gateway as a last-resort
recovery mechanism.

---

### **ID**: LogIntf-Custom-Stream-Telemetry

#### Description

User-configurable outbound telemetry stream client dedicated to persistent connections using MQTT,
TLS-secured MQTTS, raw WebSockets (WS), or Secure WebSockets (WSS). Operates in parallel and
independently of the HTTP/HTTPS targets.

#### Access

* `PhyIntf-Ethernet` (Remotely accessible via network path)
* `PhyIntf-WiFi` (Remotely accessible via network path)

#### Status

Disabled by default. Activated strictly when the user enables and saves custom MQTT or WebSocket
broker settings in the Web-UI.

#### Disclosed Information

None if encrypted profiles (MQTTS/WSS) are utilized. Cleartext variations (MQTT/WS) disclose
real-time sensor measurements to intermediate network hops if selected by the user.

#### Resilience Measures

Maintains long-lived TCP keep-alive structures. Employs independent state machine tracking for
automated reconnect logic, handling abrupt broker disconnections or network switches with the
ESP-MQTT automatic reconnect interval of 10 seconds. If all configured telemetry paths remain
unreachable for one hour, the network watchdog provides last-resort recovery; exponential or
randomized MQTT reconnect backoff is not implemented in this firmware path.

---

### **ID**: LogIntf-Cloud-HTTPS-Status

#### Description

The DUT operates an outbound status reporting client pushing heartbeat signals and basic diagnostic
health metrics to the manufacturer infrastructure at `https://network.ruuvi.com/status`.

#### Access

* `PhyIntf-Ethernet` (Remotely accessible via WAN network path)
* `PhyIntf-WiFi` (Remotely accessible via WAN network path)

#### Status

Enabled by default. Used to report general operational tracking metrics to maintenance dashboards.

#### Disclosed Information

None. Protected completely via standard TLS encapsulation paths.

#### Resilience Measures

Shared mbedtls transport logic applies. Ordered handshake checks protect the lifecycle of the data
path against connection drops or downstream target throttling events.

---

### **ID**: LogIntf-FW-Update-Client

#### Description

The DUT runs a background maintenance update utility performing periodic polling actions against the
official version index host at `https://network.ruuvi.com/firmwareupdate`. The returned JSON
descriptor carries version details from which individual binary images (`ruuvi_gateway_esp.bin`,
`fatfs_gwui.bin`, `fatfs_nrf52.bin`) are downloaded over HTTPS. For release builds, the base URL is
`https://fwupdate.ruuvi.com/<version>`; for beta builds, it maps to the designated repository asset
release path on GitHub `https://github.com/ruuvi/ruuvi.gateway_esp.c/releases/download/<version>/`.

#### Access

* `PhyIntf-Ethernet` (Remotely accessible via WAN network path)
* `PhyIntf-WiFi` (Remotely accessible via WAN network path)

#### Status

Enabled by default (linked directly to the operational update logic).

#### Disclosed Information

None. The communication is fully wrapped inside outbound HTTPS requests.

#### Resilience Measures

The system relies on an orderly TLS connection loop sequence. Downstream updates are
cryptographically signed using a manufacturer-hardened private RSA-3072 key, and auxiliary assets
are verified post-reboot against signatures embedded in the main binary text segment, protecting the
device from initialization failures or corrupted binaries.

---

### **ID**: LogIntf-Time-NTP-Client

#### Description

The DUT drives a native network background time sync process utilizing the UDP Network Time
Protocol (NTP) to coordinate internal wall-clock timestamps across four target pools:
`time.google.com`, `time.cloudflare.com`, `pool.ntp.org`, and `time.ruuvi.com`.

#### Access

* `PhyIntf-Ethernet` (Remotely accessible via WAN network path)
* `PhyIntf-WiFi` (Remotely accessible via WAN network path)

#### Status

Enabled. Critical to maintaining accurate diagnostic system clocks for TLS handshake verification
checks and sensor history validation arrays.

#### Disclosed Information

Exposes basic packet frame flags (NTP Version, Transmit Timestamp). This information carries zero
risk for underlying device operations and contains no security-relevant context.

#### Resilience Measures

The internal SNTP engine follows standard LwIP multi-server selection logic in poll mode. Normal
resynchronization uses the configured SNTP update delay (`CONFIG_LWIP_SNTP_UPDATE_DELAY`,
3600000 ms / 1 hour). If targeted endpoint resources fail to respond, the engine cycles through the
configured system pool paths and uses the LwIP SNTP retry timeout behavior, where retry timeout is
doubled up to the stack-defined maximum. This NTP polling behavior is independent of the telemetry
network watchdog described in `ResMech-Net-Watchdog-Recovery`.

---

### **ID**: LogIntf-Network-DHCP-Client

#### Description

The DUT implements an outbound DHCP client subsystem (UDP Ports 67/68) to dynamically obtain IP
address assignments, subnet masks, default gateways, and local DNS configurations from an upstream
network router over Ethernet or Wi-Fi.

#### Access

* `PhyIntf-Ethernet` (Remotely accessible via LAN network path)
* `PhyIntf-WiFi` (Remotely accessible via WLAN network path)

#### Status

Enabled by default. Necessary for dynamic IP onboarding under standard dynamic network
infrastructure rules.

#### Disclosed Information

Standard transaction frames (DHCP Discover and Request) contain basic device structural details,
including the hardware MAC address and the hostname string. No user security credentials or
sensitive parameters are exposed.

#### Resilience Measures

Managed natively by the LwIP TCP/IP stack within ESP-IDF. Includes integrated transaction
retransmission, randomized backoff, and fallback execution logic to maintain local state machines if
a lease is temporarily refused by the upstream server.

---

### **ID**: LogIntf-Hotspot-DHCP-Server

#### Description

When operating in its initial Wi-Fi provisioning hotspot mode, the DUT runs an internal DHCP server
utility (UDP Ports 67/68) to automatically allocate transient local IP addresses, subnet masks, and
default gateways to connecting client configurations (smartphones or laptops).

#### Access

* `PhyIntf-WiFi` (Locally accessible via the setup wireless medium)

#### Status

Active on-demand (only operational while the transient configuration hotspot interface is awake).

#### Disclosed Information

Standard transaction structures (DHCP Discover/Offer/Request/Ack) containing basic pool layout
frames. Exposes no sensitive credential sets or internal persistent routing tables.

#### Resilience Measures

Managed natively via standard network library components within the underlying ESP-IDF stack.
Sockets and lease resource pools are entirely destroyed when the provisioning window times out or
closes upon successful connection completion.

---

### **ID**: LogIntf-Network-DNS-Client

#### Description

The DUT implements a standard Domain Name System (DNS) resolver client over UDP (Port 53) to map
literal alphanumeric domain descriptors to valid IP addresses.

#### Access

* `PhyIntf-Ethernet` (Remotely accessible via network path)
* `PhyIntf-WiFi` (Remotely accessible via network path)

#### Status

Enabled. Crucial for establishing valid connection tracking links to telemetry and update endpoints.

#### Disclosed Information

Transmits outgoing lookup requests revealing target domain profiles.

#### Resilience Measures

Employs standard fallback mechanisms embedded within the underlying ESP-IDF TCP/IP framework stack,
including request timeout limits and secondary IP target server fallbacks.

---

### **ID**: LogIntf-Hotspot-DNS-Server

#### Description

When operating in its initial Wi-Fi provisioning hotspot mode, the DUT shifts behavior to act as a
local inbound DNS redirection server (Port 53), capturing incoming requests to resolve default web
pages and routing them straight to the local Captive Portal initialization panel.

#### Access

* `PhyIntf-WiFi` (Locally accessible via the setup wireless medium)

#### Status

Active on-demand (only operational while the transient configuration hotspot interface is awake).

#### Disclosed Information

None. This interface forces a structural redirect for any query straight to the device's own local
HTTP address.

#### Resilience Measures

Strictly bounded at the application layer. Sockets are completely discarded once the provisioning
phase terminates or logs an internal timeout boundary.

---

### **ID**: LogIntf-WiFi-WPS-Client

#### Description

The DUT includes an integrated Wi-Fi Protected Setup (WPS) client subsystem utilizing the underlying
ESP-IDF wireless controller features to automatically capture local network configuration attributes
and Wi-Fi security keys from a coordinating physical Access Point.

#### Access

* `PhyIntf-WiFi` (Locally addressable over the air via 802.11 management frames)

#### Status

Active on-demand (only triggered during explicit user interaction periods in the initialization
configuration lifecycle).

#### Disclosed Information

Exposes transient device-identifying attributes inside probe request management vectors during the
standard EAP-WPS credential exchange sequence.

#### Resilience Measures

Enforces a hardcoded operational listener execution timeout (2 minutes). The underlying radio
controller systematically disables the handshake routine to protect the interface from persistent
sniffing traps if the target registrar fails to respond.

---

### **ID**: LogIntf-USB-UART-Log-Stream

#### Description

The DUT operates a continuous logical log stream task over the virtual USB-UART serial interface
bridge (operating at 115200 baud). It prints runtime operational telemetry, BLE scanning metrics,
and standard diagnostic messages.

#### Access

* `PhyIntf-USB` (Locally accessible via direct physical connection to the Type-C port)

#### Status

Enabled. Essential for local device diagnostics, performance monitoring, and system debugging during
active deployment.

#### Disclosed Information

Discloses real-time system behaviors, BLE packet handling events, and general warning/error flags.
**No security-critical parameters**, such as plaintext user configuration blocks, encryption
secrets, or authentication hashes, are ever written to the log stream.

#### Resilience Measures

The console log buffer operates on a non-blocking FIFO ring buffer scheme handled within the ESP-IDF
logging framework (`esp_log`). If the terminal serial interface drops or the host machine fails to
read, logs drop cleanly without causing runtime core lockups or buffer overflows.

---

### **ID**: LogIntf-USB-Boot-Flasher

#### Description

The hardware-level ROM bootloader communication interface that intercepts system execution flags
upon reset to facilitate local firmware flashing, image partition reading, and full factory chip
erasing using development tools like `esptool.py`.

#### Access

* `PhyIntf-USB` (Locally accessible via direct physical connection to the Type-C port)

#### Status

Enabled. Required to flash initial production code at the factory and allow local low-level
maintenance recovery if an online network update fails.

#### Debug Interface

Yes (Physical / ROM Level).

#### Disclosed Information

Unauthenticated access allows low-level chip identification, flash partition table reading, and
custom binary writing. This interface is highly security-relevant because it targets raw hardware
flash access.

#### Resilience Measures

Activated programmatically over the USB interface by toggling the USB-to-UART bridge control lines (
DTR/RTS) to reset the chip and force entry into the bootloader state. The interface operates
strictly synchronously and is completely isolated from the network stack, containing the threat
boundary to actors with immediate physical connection access to the physical USB port.

---

### **ID**: LogIntf-Internal-SWD-Bus

#### Description

An unexposed internal hardware-tier inter-chip debug communication channel connecting the master
ESP32 system microcontroller to the peripheral nRF52811 co-processor debug access port (DAP). It is
driven programmatically by the master application layer via `libswd` to run early boot-time code
integrity checks and force low-level binary restoration sequences.

#### Access

* Locally isolated on dedicated PCB trace copper layers entirely within the device enclosure. It has
  no physical exposure outside the housing and cannot be addressed over any external wire segment,
  network protocol socket, or over-the-air communication vector.

#### Status

Enabled. Active during early boot system initialization routines and firmware component flash
writing phases.

#### Debug Interface

Yes (Internal Physical / Inter-Chip Master-Target Debug Link).

#### Disclosed Information

Enables complete bare-metal host access to the nRF52811 memory blocks, hardware control units,
register arrays, and internal flash partitions. Access is security-relevant as it handles the raw
code footprint of the co-processor radio layer.

#### Resilience Measures

The communication channel operates strictly point-to-point under total local programmatic control of
the master ESP32 kernel. It relies entirely on the structural physical enclosure isolation of the
gateway casing. Because it maps to internal PCB tracks, it is structurally invulnerable to remote
network eavesdropping, Man-in-the-Middle (MitM) payload manipulation, or distributed socket
starvation injections.

---

## Summary Matrix for the Technical File

| Interface ID                      | Type / Port        | Protocol          | Remote Network Access? | Initial Status | Security Relevant Disclosed Info                         |
|:----------------------------------|:-------------------|:------------------|:----------------------:|:---------------|:---------------------------------------------------------|
| `LogIntf-HTTP-Server`             | Server / 80        | HTTP              |    Yes (LAN / WLAN)    | Enabled        | None (Generic platform header signature only)            |
| `LogIntf-Cloud-HTTPS-Telemetry`   | Client / 443       | HTTPS             |   Yes (WAN Network)    | Enabled        | None (TLS Encrypted Payload)                             |
| `LogIntf-Custom-HTTP-Telemetry`   | Client / Config    | HTTP/HTTPS        |  Yes (Local Network)   | Disabled       | None if HTTPS transport is selected                      |
| `LogIntf-Custom-Stream-Telemetry` | Client / Config    | MQTT/MQTTS/WS/WSS |  Yes (Local Network)   | Disabled       | None if MQTTS/WSS transport is selected                  |
| `LogIntf-Cloud-HTTPS-Status`      | Client / 443       | HTTPS             |   Yes (WAN Network)    | Enabled        | None (TLS Encrypted Payload)                             |
| `LogIntf-FW-Update-Client`        | Client / 443       | HTTPS             |   Yes (WAN Network)    | Enabled        | None (RSA Signed Firmware Payload Validation)            |
| `LogIntf-Time-NTP-Client`         | Client / 123       | NTP / UDP         |   Yes (WAN Network)    | Enabled        | None                                                     |
| `LogIntf-Network-DHCP-Client`     | Client / 67, 68    | DHCP / UDP        |  Yes (Local Network)   | Enabled        | None (Exposes MAC and Hostname strings)                  |
| `LogIntf-Hotspot-DHCP-Server`     | Server / 67, 68    | DHCP / UDP        | No (Local Media Only)  | Transient      | None                                                     |
| `LogIntf-Network-DNS-Client`      | Client / 53        | DNS / UDP         |  Yes (Local Network)   | Enabled        | None (Reveals lookup destination domains)                |
| `LogIntf-Hotspot-DNS-Server`      | Server / 53        | DNS / UDP         | No (Local Media Only)  | Transient      | None (Captive Portal structural redirection only)        |
| `LogIntf-WiFi-WPS-Client`         | Client / Mgmt      | 802.11 / WPS      | No (Local Media Only)  | Transient      | None (Exposes transient device attributes)               |
| `LogIntf-USB-UART-Log-Stream`     | Stream / Serial    | UART / Text       |  No (Local Port Only)  | Enabled        | Operational diagnostic traces only (No secrets)          |
| `LogIntf-USB-Boot-Flasher`        | Server / ROM       | ROM Bootloader    |  No (Local Port Only)  | Enabled        | Full hardware flash memory control access                |
| `LogIntf-Internal-SWD-Bus`        | Master-Target Link | SWD               |   No (Internal Only)   | Enabled        | Bare-metal access to nRF52 register and flash structures |
