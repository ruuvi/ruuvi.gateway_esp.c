# Test group 5.6-5: Software Services Running by Default Are Minimized

Provision 5.6-5 — Status: **R**. Related IXIT: `IXIT 13-SoftServ`.

---

## Test case 5.6-5-1 (conceptual)

**Purpose**: To conceptually assess whether every software service marked as enabled by default in
`IXIT 13-SoftServ` is necessary for the intended use or operation of the DUT according to its
description and justification (`a`).

---

### Test Units Conceptual Assessment Matrix

#### Test Unit A: Assessment of Default-Enabled Software Services Necessity

* **Requirement**: For each software service in `IXIT 13-SoftServ` marked as "Enabled" by default,
  evaluate whether the declared "Description" and "Justification" provide a valid operational
  necessity for the intended use of the DUT.

| Software Service ID (`IXIT 13-SoftServ`)      | Default Status | Functional Purpose & Operational Justification                                                          | Necessity Assessment for Intended Use                                                                         | Unit Verdict |
|:----------------------------------------------|:--------------:|:--------------------------------------------------------------------------------------------------------|:--------------------------------------------------------------------------------------------------------------|:------------:|
| **`SoftServ-Local-Management-WebUI`**         |  **Enabled**   | Local HTTP server on Port 80 for device onboarding, network configuration, and diagnostics.             | **Necessary.** Essential for local device setup, credential management, and status monitoring.                |   **PASS**   |
| **`SoftServ-Local-Programmatic-API`**         |  **Enabled**   | REST API (`/history`, `/ruuvi.json`) for local M2M automation integration.                              | **Necessary.** Enables local industrial and home automation controllers to query metrics and deploy settings. |   **PASS**   |
| **`SoftServ-CoProcessor-Verification-Loop`**  |  **Enabled**   | Internal boot-time SWD integrity check of nRF52 co-processor code blocks.                               | **Necessary.** Critical anti-tamper mechanism preventing persistent firmware compromise of the radio layer.   |   **PASS**   |
| **`SoftServ-Telemetry-Relay-RuuviCloud`**     |  **Enabled**   | Outbound HTTPS client streaming sensor payloads to `https://network.ruuvi.com/record`.                  | **Necessary.** Primary out-of-the-box product function for remote sensor data visualization.                  |   **PASS**   |
| **`SoftServ-System-Diagnostics-Reporting`**   |  **Enabled**   | Outbound HTTPS client sending heartbeat health metrics to `https://network.ruuvi.com/status`.           | **Necessary.** Required for fleet health tracking and preventive maintenance monitoring.                      |   **PASS**   |
| **`SoftServ-Network-Infrastructure-Clients`** |  **Enabled**   | Background TCP/IP clients (DHCP client, DNS resolver, SNTP time client).                                | **Necessary.** Essential core network utilities for dynamic IP lease, domain resolution, and TLS time sync.   |   **PASS**   |
| **`SoftServ-OTA-Firmware-Updater`**           |  **Enabled**   | Outbound HTTPS client polling for signed security updates (`https://network.ruuvi.com/firmwareupdate`). | **Necessary.** Critical for delivering automated software patches and bug fixes.                              |   **PASS**   |
| **`SoftServ-Centralized-Remote-Config`**      |  **Disabled**  | Outbound periodic polling for remote enterprise configuration manifests.                                | **Minimized Surface.** Optional enterprise feature; disabled by default until explicitly enabled by user.     |   **PASS**   |
| **`SoftServ-Telemetry-Relay-CustomHTTP`**     |  **Disabled**  | Outbound HTTP/HTTPS data posting to custom third-party URLs.                                            | **Minimized Surface.** Optional data routing path; disabled by default until explicitly configured.           |   **PASS**   |
| **`SoftServ-Telemetry-Relay-MQTT`**           |  **Disabled**  | Long-lived MQTT/MQTTS/WS/WSS telemetry streaming to custom brokers.                                     | **Minimized Surface.** Optional streaming protocol; disabled by default until explicitly configured.          |   **PASS**   |
| **`SoftServ-Hotspot-Orchestration`**          | **Transient**  | Local AP, DNS redirection, and DHCP server for setup captive portal.                                    | **Minimized Surface.** Active strictly during unconfigured initial setup; shuts down after setup or timeout.  |   **PASS**   |
| **`SoftServ-WiFi-WPS-Onboarding`**            | **Transient**  | Wi-Fi Protected Setup (WPS) client helper sub-service.                                                  | **Minimized Surface.** Active strictly during user-initiated push-button pairing windows.                     |   **PASS**   |

**Assessment Justification**: Every software service enabled by default in `IXIT 13-SoftServ` is
directly required for primary device operation, local management, core network infrastructure, or
security maintenance (OTA patches and co-processor integrity verification). All optional third-party
telemetry and enterprise configuration services remain disabled by default, ensuring the software
attack surface is strictly minimized.

**Verdict**: **PASS**

---

## Summary Matrix for Test Case 5.6-5-1

| Test Case          | Purpose / Focus                          | Assessment Summary                                                                                                        | Unit Verdict |
|:-------------------|:-----------------------------------------|:--------------------------------------------------------------------------------------------------------------------------|:------------:|
| **5.6-5-1 Unit a** | Necessity Assessment of Enabled Services | All default-enabled software services are necessary for primary operation, network stack functions, or security patching. |   **PASS**   |

---

## Group Summary

The Ruuvi Gateway complies fully with Recommendation Provision 5.6-5 of `ETSI EN 303 645`. The
technical documentation (`IXIT 13-SoftServ`) confirms that software services running by default are
strictly limited to those necessary for intended device operations (local Web-UI/API, Ruuvi Cloud
telemetry, system diagnostics, core network clients, and OTA updates). Optional features—such as
custom HTTP posting, MQTT streaming, and remote corporate management sync—are kept disabled by
default, ensuring an optimized and minimized software attack surface.

**Group Verdict**: **PASS**
