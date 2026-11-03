# Test group 5.6-1: Unused Network and Logical Interfaces Are Disabled

Provision 5.6-1 — Status: **M F (p)**. Related IXIT: `IXIT 15-PhyIntf`, `IXIT 28-LogIntf`.

**Evidence status**: Live-DUT tests have been executed, as confirmed by the maintainer. The
logical-interface discovery script is being merged separately in
[#1685](https://github.com/ruuvi/ruuvi.gateway_esp.c/pull/1685). Integration and review of the run
evidence in these assessments is deferred to a separate documentation task after that PR and
[#1683](https://github.com/ruuvi/ruuvi.gateway_esp.c/pull/1683) and
[#1684](https://github.com/ruuvi/ruuvi.gateway_esp.c/pull/1684) have merged into `master`.
The mDNS and dynamic SNTP descriptions below are source-derived expectations; this documentation
update asserts no new live-DUT PASS. Existing physical-interface results are retained from the
prior assessment and are not revalidated here. **PENDING** below means pending evidence integration
and verdict review, not that live-DUT tests have not been executed.

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

| Logical Interface ID (`IXIT 28-LogIntf`) | Network Access Vector                                       | Operational State & Status      | Purpose & Functional Justification                                                                                                                                                                   | Unit Verdict |
|:-----------------------------------------|:------------------------------------------------------------|:--------------------------------|:-----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|:------------:|
| **`LogIntf-HTTP-Server`**                | Local LAN / Port 80                                         | Enabled                         | **Valid Justification.** Primary inbound local administrative interface for Web-UI setup, REST API endpoints, and cloud target configuration.                                                        |   **PASS**   |
| **`LogIntf-mDNS-Responder`**             | Local LAN / UDP Port 5353                                   | Enabled while network-connected | **Valid Justification.** Resolves the configured hostname under `.local` and advertises `_http._tcp` on TCP Port 80 for local management service discovery. It does not accept configuration writes. |   **PASS**   |
| **`LogIntf-Cloud-HTTPS-Telemetry`**      | WAN / TCP Port 443                                          | Enabled                         | **Valid Justification.** Core device functionality for sending periodic sensor telemetry data to Ruuvi Cloud backend via encrypted TLS.                                                              |   **PASS**   |
| **`LogIntf-Cloud-HTTPS-Status`**         | WAN / TCP Port 443                                          | Enabled                         | **Valid Justification.** Outbound operational heartbeat and health diagnostic reporting to cloud infrastructure.                                                                                     |   **PASS**   |
| **`LogIntf-FW-Update-Client`**           | WAN / TCP Port 443                                          | Enabled                         | **Valid Justification.** Outbound polling for security patches and downloading cryptographically signed OTA binaries.                                                                                |   **PASS**   |
| **`LogIntf-Time-NTP-Client`**            | WAN / server UDP Port 123; dynamic local source 49152-65535 | Enabled                         | **Valid Justification.** Required to synchronize system wall-clock time for valid TLS certificate validation. The local source port may change after reboot or SNTP restart.                         |   **PASS**   |
| **`LogIntf-Network-DHCP-Client`**        | Local LAN / UDP Ports 67, 68                                | Enabled                         | **Valid Justification.** Essential network client to dynamically obtain IP address assignments from local gateways.                                                                                  |   **PASS**   |
| **`LogIntf-Network-DNS-Client`**         | Local LAN / UDP Port 53                                     | Enabled                         | **Valid Justification.** Essential network client to resolve domain names for telemetry endpoints and update servers.                                                                                |   **PASS**   |
| **`LogIntf-Hotspot-DHCP-Server`**        | Local Wireless AP / UDP Ports 67, 68                        | Transient (On-Demand)           | **Valid Justification.** Transient setup AP DHCP server; automatically terminates upon connection or 1-hour timeout.                                                                                 |   **PASS**   |
| **`LogIntf-Hotspot-DNS-Server`**         | Local Wireless AP / UDP Port 53                             | Transient (On-Demand)           | **Valid Justification.** Transient setup AP DNS server for captive portal redirection; automatically terminates upon setup completion.                                                               |   **PASS**   |
| **`LogIntf-Custom-HTTP-Telemetry`**      | Network Path                                                | Disabled                        | **Minimized Attack Surface.** Disabled by default; active strictly when configured by administrator.                                                                                                 |   **PASS**   |
| **`LogIntf-Custom-Stream-Telemetry`**    | Network Path                                                | Disabled                        | **Minimized Attack Surface.** Disabled by default; active strictly when custom MQTT/WS profiles are provisioned.                                                                                     |   **PASS**   |

* **Unit B Verdict**: **PASS**

---

## Test case 5.6-1-2 (functional)

**Purpose**: To functionally verify that interface statuses match `IXIT 15-PhyIntf` and
`IXIT 28-LogIntf` (`a`), and to verify through physical inspection and network scanning that no
undocumented physical interfaces (`b`) or logical network interfaces (`c`) are accessible on the
DUT.

---

### Test Units Functional Assessment Matrix

Unit c is based on firmware source review and the declarations in
[IXIT 28-LogIntf](ixit_28-LogIntf.md). In the initialized, network-connected state, the source-derived
expectation is HTTP on TCP/80 and mDNS on UDP/5353. The outbound SNTP client's local UDP source port
is dynamically allocated from 49152-65535 using a boot-randomized allocator and may change after
reboot or SNTP restart. A scan may report it as `open|filtered`; identifying it as an SNTP client
socket requires correlation with captured outbound requests to server UDP/123. It is not an inbound
NTP service. These expectations are not recorded observations from the live-DUT runs. Source review
alone does not establish that no undocumented listeners are accessible on the DUT.

| Test Unit / Verification Focus               | Assessment Methodology & Evidence Basis                                                            | Retained Observations / Source-Derived Expectations                                                                                                                                                                  | Unit Verdict |
|:---------------------------------------------|:---------------------------------------------------------------------------------------------------|:---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|:------------:|
| **Unit a: Interface Status Matching**        | Protocol testing and network interface state checks during startup, setup, and operational phases. | Interface statuses match IXIT declarations. ESP32 BLE emits no advertisements; nRF52 SWD pins do not respond to debug probes post-boot.                                                                              |   **PASS**   |
| **Unit b: Undocumented Physical Interfaces** | Hardware disassembly, PCB trace inspection, and RF spectrum analyzer sweeps (2.4 GHz).             | Physical inspection confirms mainboard trace mapping matches `IXIT 15-PhyIntf`. RF sweeps detect only expected Wi-Fi and passive BLE signals.                                                                        |   **PASS**   |
| **Unit c: Undocumented Logical Interfaces**  | Source review; integration and review of full-range TCP/UDP scans and traffic captures pending.    | Expected: documented HTTP/mDNS responders and a dynamic outbound SNTP client socket. Actual listener inventory, SNTP traffic correlation, and absence of undocumented interfaces require review of the run evidence. | **PENDING**  |

**Assessment Justification**: The prior results for Units a and b are retained. They do not establish
functional coverage of the newly documented mDNS responder or dynamic SNTP client socket. Source
review supports the revised logical-interface expectations, while the completeness conclusion for
Unit c awaits integration and review of scan and traffic-capture evidence.

**Verdict**: **PENDING** — integration and review of functional evidence for the revised inventory.

### Follow-Up After Test-Script Merge

After #1683, #1684, and #1685 merge into `master`, the separate documentation task must:

* Update Unit c from the reviewed scan and packet-capture evidence associated with #1685,
  referencing the merged script revision, tested firmware, run date, and evidence artifacts.
  Record actual Ethernet/Wi-Fi and operating-state coverage, HTTP/mDNS observations, SNTP
  request/source-port correlation, scan limitations, and configuration restoration results.
* Update affected interface-status observations in Unit a only where the evidence supports them;
  do not treat logical-interface scans as new physical-interface evidence.
* Reconcile the inventory conclusions with Test Unit 5.5-5-2 b (#1684), replace expectations with
  supported observations, and update the unit, case, summary, group, and ICS entries. Merging
  scripts alone does not establish a functional PASS.

---

## Summary Matrix for Test Case 5.6-1-1 & 5.6-1-2

| Test Case             | Purpose / Focus                        | Assessment Summary                                                                                                               | Unit Verdict |
|:----------------------|:---------------------------------------|:---------------------------------------------------------------------------------------------------------------------------------|:------------:|
| **5.6-1-1 Unit a**    | Justification of Physical Interfaces   | All enabled physical/air interfaces have valid functional justifications; unused macros are disabled.                            |   **PASS**   |
| **5.6-1-1 Unit b**    | Justification of Logical Interfaces    | HTTP management, mDNS discovery, transient setup services, and outbound clients have source-derived operational justifications.  |   **PASS**   |
| **5.6-1-2 Units a-b** | Interface Status / Physical Inspection | Prior interface-status and physical-inspection results retained; no new mDNS/SNTP functional coverage asserted.                  |   **PASS**   |
| **5.6-1-2 Unit c**    | Logical Interface Scanning             | Source-derived HTTP/mDNS and dynamic SNTP expectations documented; scan/capture evidence integration and verdict review pending. | **PENDING**  |

---

## Group Summary

The conceptual assessment supports the operational justifications required by Mandatory Provision
5.6-1 of `ETSI EN 303 645`. All enabled
physical interfaces (`PhyIntf-Ethernet`, `PhyIntf-WiFi`, `PhyIntf-BLE-nRF52`, `PhyIntf-USB`) and
logical interfaces (`IXIT 28-LogIntf`) have valid functional justifications. Unnecessary
interfaces—such as the ESP32 BLE radio macro and post-boot SWD debug pads—are completely disabled.
Prior physical-interface results are retained. Completeness of the revised logical-interface
inventory, including mDNS and the dynamic SNTP client socket, remains pending integration and
review of the live-DUT scan and capture evidence in the follow-up documentation task.

**Group Verdict**: **PENDING** — Unit 5.6-1-2 c evidence integration and verdict review outstanding.
