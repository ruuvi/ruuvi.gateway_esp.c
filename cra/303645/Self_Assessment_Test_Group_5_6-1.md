# Test group 5.6-1: Unused Network and Logical Interfaces Are Disabled

Provision 5.6-1 — Status: **M F (p)**. Related IXIT: `IXIT 15-PhyIntf`, `IXIT 28-LogIntf`.

---

## Test case 5.6-1-1 (conceptual)

**Purpose**: To conceptually assess whether every enabled physical network interface (
`IXIT 15-PhyIntf`) (`a`) and every network-accessible logical interface (`IXIT 28-LogIntf`) (`b`)
has a valid operational justification for being enabled.

---

### Test Units Conceptual Assessment Matrix

#### Test Unit A: Assessment of Physical Network Interfaces (`IXIT 15-PhyIntf`)

* **Requirement**: For each physical interface marked as "Enabled" in `IXIT 15-PhyIntf`, assess
  whether the declared "Description" provides a valid justification for its enabled status.

| Physical Interface ID (`IXIT 15-PhyIntf`) | Type                    | Declared Status         | Purpose & Functional Justification                                                                                                                                | Unit Verdict |
|:------------------------------------------|:------------------------|:------------------------|:------------------------------------------------------------------------------------------------------------------------------------------------------------------|:------------:|
| **`PhyIntf-Ethernet`**                    | Network / Physical Jack | **Enabled**             | **Valid Justification.** Primary physical backhaul required for streaming BLE telemetry to cloud servers and local configuration.                                 |   **PASS**   |
| **`PhyIntf-WiFi`**                        | Network / Air Interface | **Enabled** (On-Demand) | **Valid Justification.** Secondary wireless backhaul and transient setup wizard access point.                                                                     |   **PASS**   |
| **`PhyIntf-BLE-nRF52`**                   | Network / Air Interface | **Enabled**             | **Valid Justification.** Essential for scanning and ingesting BLE broadcast advertisements from environmental sensors. Enforces Rx-Only connectionless operation. |   **PASS**   |
| **`PhyIntf-BLE-ESP32`**                   | Air Interface           | **Disabled**            | **Minimized Attack Surface.** Bluetooth macro inside ESP32 SoC is completely disabled in software and compiled out.                                               |   **PASS**   |
| **`PhyIntf-SWD-nRF52`**                   | Internal PCB Pads       | **Disabled** (Runtime)  | **Minimized Attack Surface.** SWD debug pins are remapped to standard GPIOs post-boot, electrically disabling runtime debugging.                                  |   **PASS**   |
| **`PhyIntf-USB`**                         | Physical Port           | **Enabled**             | **Valid Justification.** Primary power delivery and passive, read-only diagnostic console log output. Reflashing requires hardware pin strapping.                 |   **PASS**   |

* **Unit A Verdict**: **PASS**

#### Test Unit B: Assessment of Network-Accessible Logical Interfaces (`IXIT 28-LogIntf`)

* **Requirement**: For each logical interface marked as "Enabled" (or "Active on-demand") and
  accessible via a network interface in `IXIT 28-LogIntf`, assess whether its operational purpose
  provides a valid justification.

| Logical Interface ID (`IXIT 28-LogIntf`) | Network Access Vector                | Operational State & Status | Purpose & Functional Justification                                                                                                            | Unit Verdict |
|:-----------------------------------------|:-------------------------------------|:---------------------------|:----------------------------------------------------------------------------------------------------------------------------------------------|:------------:|
| **`LogIntf-HTTP-Server`**                | Local LAN / Port 80                  | Enabled                    | **Valid Justification.** Primary inbound local administrative interface for Web-UI setup, REST API endpoints, and cloud target configuration. |   **PASS**   |
| **`LogIntf-Cloud-HTTPS-Telemetry`**      | WAN / TCP Port 443                   | Enabled                    | **Valid Justification.** Core device functionality for sending periodic sensor telemetry data to Ruuvi Cloud backend via encrypted TLS.       |   **PASS**   |
| **`LogIntf-Cloud-HTTPS-Status`**         | WAN / TCP Port 443                   | Enabled                    | **Valid Justification.** Outbound operational heartbeat and health diagnostic reporting to cloud infrastructure.                              |   **PASS**   |
| **`LogIntf-FW-Update-Client`**           | WAN / TCP Port 443                   | Enabled                    | **Valid Justification.** Outbound polling for security patches and downloading cryptographically signed OTA binaries.                         |   **PASS**   |
| **`LogIntf-Time-NTP-Client`**            | WAN / UDP Port 123                   | Enabled                    | **Valid Justification.** Required to synchronize system wall-clock time for valid TLS certificate validation.                                 |   **PASS**   |
| **`LogIntf-Network-DHCP-Client`**        | Local LAN / UDP Ports 67, 68         | Enabled                    | **Valid Justification.** Essential network client to dynamically obtain IP address assignments from local gateways.                           |   **PASS**   |
| **`LogIntf-Network-DNS-Client`**         | Local LAN / UDP Port 53              | Enabled                    | **Valid Justification.** Essential network client to resolve domain names for telemetry endpoints and update servers.                         |   **PASS**   |
| **`LogIntf-Hotspot-DHCP-Server`**        | Local Wireless AP / UDP Ports 67, 68 | Transient (On-Demand)      | **Valid Justification.** Transient setup AP DHCP server; automatically terminates upon connection or 1-hour timeout.                          |   **PASS**   |
| **`LogIntf-Hotspot-DNS-Server`**         | Local Wireless AP / UDP Port 53      | Transient (On-Demand)      | **Valid Justification.** Transient setup AP DNS server for captive portal redirection; automatically terminates upon setup completion.        |   **PASS**   |
| **`LogIntf-Custom-HTTP-Telemetry`**      | Network Path                         | Disabled                   | **Minimized Attack Surface.** Disabled by default; active strictly when configured by administrator.                                          |   **PASS**   |
| **`LogIntf-Custom-Stream-Telemetry`**    | Network Path                         | Disabled                   | **Minimized Attack Surface.** Disabled by default; active strictly when custom MQTT/WS profiles are provisioned.                              |   **PASS**   |

* **Unit B Verdict**: **PASS**

---

## Test case 5.6-1-2 (functional)

**Purpose**: To functionally verify that interface statuses match `IXIT 15-PhyIntf` and
`IXIT 28-LogIntf` (`a`), and to verify through physical inspection and network scanning that no
undocumented physical interfaces (`b`) or logical network interfaces (`c`) are accessible on the
DUT.

---

### Test Units Functional Assessment Matrix

| Test Unit / Verification Focus               | Testing Methodology & Tools Executed                                                               | Observed Functional DUT Behavior                                                                                                                                                 | Unit Verdict |
|:---------------------------------------------|:---------------------------------------------------------------------------------------------------|:---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|:------------:|
| **Unit a: Interface Status Matching**        | Protocol testing and network interface state checks during startup, setup, and operational phases. | Interface statuses match IXIT declarations. ESP32 BLE emits no advertisements; nRF52 SWD pins do not respond to debug probes post-boot.                                          |   **PASS**   |
| **Unit b: Undocumented Physical Interfaces** | Hardware disassembly, PCB trace inspection, and RF spectrum analyzer sweeps (2.4 GHz).             | Physical inspection confirms mainboard trace mapping matches `IXIT 15-PhyIntf`. RF sweeps detect only expected Wi-Fi and passive BLE signals.                                    |   **PASS**   |
| **Unit c: Undocumented Logical Interfaces**  | Comprehensive port scanning (`nmap -p 1-65535 -sV -sU`) across Ethernet and Wi-Fi drops.           | Port scanning confirms the DUT exposes only Port 80 (`LogIntf-HTTP-Server`). No hidden SSH, Telnet, or diagnostic ports exist. All open sockets correspond to `IXIT 28-LogIntf`. |   **PASS**   |

**Assessment Justification**: Functional network scanning and physical PCB inspection confirm that
all disabled or internal interfaces are effectively deactivated, exposed listening sockets match
`IXIT 28-LogIntf` precisely, and zero undocumented physical or logical network interfaces exist on
the DUT.

**Verdict**: **PASS**

---

## Summary Matrix for Test Case 5.6-1-1 & 5.6-1-2

| Test Case             | Purpose / Focus                      | Assessment Summary                                                                                              | Unit Verdict |
|:----------------------|:-------------------------------------|:----------------------------------------------------------------------------------------------------------------|:------------:|
| **5.6-1-1 Unit a**    | Justification of Physical Interfaces | All enabled physical/air interfaces have valid functional justifications; unused macros are disabled.           |   **PASS**   |
| **5.6-1-1 Unit b**    | Justification of Logical Interfaces  | Inbound listening ports are limited strictly to authenticated Port 80 HTTP server and transient setup hotspot.  |   **PASS**   |
| **5.6-1-2 Units a-c** | Functional Interface Scanning        | Physical inspection and `nmap` port scans confirm zero undocumented physical ports or hidden listening sockets. |   **PASS**   |

---

## Group Summary

The Ruuvi Gateway complies fully with Mandatory Provision 5.6-1 of `ETSI EN 303 645`. All enabled
physical interfaces (`PhyIntf-Ethernet`, `PhyIntf-WiFi`, `PhyIntf-BLE-nRF52`, `PhyIntf-USB`) and
logical interfaces (`IXIT 28-LogIntf`) have valid functional justifications. Unnecessary
interfaces—such as the ESP32 BLE radio macro and post-boot SWD debug pads—are completely disabled.
Full network port scanning (`nmap`) and physical PCB audits verify that no undocumented physical or
logical network interfaces are accessible on the DUT.

**Group Verdict**: **PASS**
