# Test group 6-1: Transparent User Information About Personal Data Processing

Provision 6-1 — Status: **M**. Related IXIT: `IXIT 21-PersData`, `IXIT 2-UserInfo`.

---

## Test case 6-1-1 (conceptual)

**Purpose**: To conceptually assess whether the method of documenting the description, purpose,
authorized processing parties, and retention lifecycle for each category of personal data in
`IXIT 21-PersData` is suitable under `IXIT 2-UserInfo` ("Documentation of Personal Data") to provide
consumers with clear and transparent information (`a`).

---

### Test Units Conceptual Assessment Matrix

#### Test Unit A: Assessment of Information Transparency and Suitability

| Personal Data Category ID (`IXIT 21-PersData`) | Category Description & Data Type                           | Documented Purpose of Processing                                                                                       | Authorized Processing Organizations                                                                | Storage Duration & Lifecycle Bounds                                                                                                  | Suitability & Transparency Audit (`IXIT 2-UserInfo`)                                                                                | Unit Verdict |
|:-----------------------------------------------|:-----------------------------------------------------------|:-----------------------------------------------------------------------------------------------------------------------|:---------------------------------------------------------------------------------------------------|:-------------------------------------------------------------------------------------------------------------------------------------|:------------------------------------------------------------------------------------------------------------------------------------|:------------:|
| **`PersData-Network-IP-Footprints`**           | Network Address Metadata (Public WAN IP & Station LAN IP). | Facilitates local network routing, packet switching, Web-UI session validation, and secure TLS socket transport setup. | Local Network Admin, Upstream ISPs, Ruuvi Innovations Oy (Ruuvi Cloud), Custom Server Operators.   | RAM-only during active links; erased on reboot or link drop. Static IPs retained in `nvs` flash until deleted/reset.                 | **Clear & Transparent.** Documented in Ruuvi Privacy Policy (`ruuvi.com/privacy/`) and firmware architecture manuals.               |   **PASS**   |
| **`PersData-Gateway-LAN-MAC`**                 | Local Network Hardware Interface MAC (ESP32).              | Used strictly at Layer 2 for Ethernet frame switching, ARP queries, and local DHCP IP reservations on the LAN.         | Local Network Admin, Local Network Infrastructure (Routers/Switches). *Never sent to Ruuvi Cloud.* | Persistent on hardware registry cells. Visible on local network segment while media cable/Wi-Fi link is connected.                   | **Clear & Transparent.** Explicitly identified as isolated local network metadata; documented under local privacy disclosures.      |   **PASS**   |
| **`PersData-Hardware-DeviceID`**               | Unique 64-bit Microcontroller Hardware ID (`nRF52 FICR`).  | Factory default Web-UI password seed; baseline cryptographic root seed for HMAC-SHA256 telemetry payload signing.      | Ruuvi Innovations Oy (Manufacturing identity databases). *Never sent raw over network interfaces.* | Burned permanently into non-volatile factory silicon (FICR); immutable for device physical lifespan.                                 | **Clear & Transparent.** Clearly disclosed as factory root credential and HMAC signing seed in product privacy notice.              |   **PASS**   |
| **`PersData-Gateway-MAC-Identifier`**          | Radio Controller Bluetooth MAC (`gw_mac`).                 | Primary system identity string wrapped in outbound JSON telemetry envelopes to map metrics back to a specific gateway. | Ruuvi Innovations Oy, Authorized operators of user-configured custom HTTP/MQTT ingestion servers.  | Persistent on radio chip. Written to `ruuvi.json` (`nvs`) during setup; persists for operational lifetime of device setup.           | **Clear & Transparent.** Documented as explicit telemetry origin header in privacy policy and data format specs (`docs.ruuvi.com`). |   **PASS**   |
| **`PersData-Custom-Target-Access-Secrets`**    | Private API Keys, Passwords & mTLS Certs.                  | Facilitates client-side machine authentication and mTLS encryption when pushing sensor payloads to custom servers.     | Authenticated System Administrator, Designated third-party destination server validation handlers. | Alphanumeric keys stored in `ruuvi.json` (`nvs`); x509 certificates stored in `gw_cfg_def`. Retained until erased via reset loop.    | **Clear & Transparent.** Disclosed as user-managed security assets in privacy notice and Web-UI setup manuals.                      |   **PASS**   |
| **`PersData-BLE-Sensor-Telemetry`**            | Environmental Metrics & BLE Tag Payloads.                  | Real-time environmental monitoring, trend graphing, and alerting (may infer household occupancy or activity patterns). | Ruuvi Innovations Oy (Ruuvi Cloud), Custom Server Operators, Authenticated Local API Clients.      | **On-Device:** Volatile RAM queues (wiped on reboot/reset). **Cloud:** Retained for active subscription; purged on account deletion. | **Clear & Transparent.** Fully disclosed in Ruuvi Privacy Policy with explicit instructions on data retention and account deletion. |   **PASS**   |

* **Conceptual Assessment Justification**: `IXIT 2-UserInfo` ("Documentation of Personal Data")
  links directly to the published Ruuvi Privacy Policy (`https://ruuvi.com/privacy/`) and firmware
  architecture manuals (`https://docs.ruuvi.com/ruuvi-gateway-firmware/`). These publications
  clearly and transparently declare all four required elements for every personal data category: (1)
  type of processed data, (2) purpose of processing, (3) authorized processing organizations, and (
  4) storage duration/lifecycle.

* **Unit A Verdict**: **PASS**

---

## Test case 6-1-2 (functional)

**Purpose**: To functionally check that the user information provided to consumers about what
personal data is processed, for what purpose, by whom, and for how long is clear and transparent (
`a`), and accurately reflects the actual implementation declared in `IXIT 21-PersData` (`b`).

---

### Test Units Functional Assessment Matrix

#### Test Unit A & B: Functional Verification of Published User Information Accuracy and Transparency

**Testing Methodology**: The test laboratory accessed the public user documentation and privacy
policy vectors (`https://ruuvi.com/privacy/` and `https://docs.ruuvi.com/ruuvi-gateway-firmware/`),
auditing the published statements against functional wire captures (Wireshark), Web-UI configuration
panels, and memory inspection.

| Personal Data Category ID (`IXIT 21-PersData`) | Public User Documentation Vector (`IXIT 2-UserInfo`)                 | Documented Claims vs. Actual Functional Implementation Audit                                                                                            | Transparency & Accuracy Evaluation (Units a & b)                                                               | Unit Verdict |
|:-----------------------------------------------|:---------------------------------------------------------------------|:--------------------------------------------------------------------------------------------------------------------------------------------------------|:---------------------------------------------------------------------------------------------------------------|:------------:|
| **`PersData-Network-IP-Footprints`**           | Ruuvi Privacy Notice (Section: *Ruuvi Gateway*) & Firmware Tech Docs | **Confirmed.** IP addresses are used strictly for local socket setup and WAN TLS routing. Outbound packet headers match documented server endpoints.    | **Clear & Transparent.** User information accurately reflects network packet behavior.                         |   **PASS**   |
| **`PersData-Gateway-LAN-MAC`**                 | Ruuvi Privacy Notice & Firmware Tech Docs                            | **Confirmed.** Wire captures confirm the ESP32 LAN MAC is used exclusively for local L2 ARP/DHCP frames and is **never** transmitted to Ruuvi Cloud.    | **Clear & Transparent.** Isolation claims match physical wire captures precisely.                              |   **PASS**   |
| **`PersData-Hardware-DeviceID`**               | Ruuvi Privacy Notice & Casing Label Guide                            | **Confirmed.** Network traffic sniffer confirms raw 64-bit FICR `DEVICEID` is never emitted over WAN/LAN; used locally for Web-UI password & HMAC seed. | **Clear & Transparent.** Label and privacy disclosures accurately explain its hardware authentication role.    |   **PASS**   |
| **`PersData-Gateway-MAC-Identifier`**          | Ruuvi Privacy Notice & Data Format Manuals                           | **Confirmed.** `gw_mac` string appears as JSON origin header in outbound HTTPS POST payloads to `https://network.ruuvi.com/record` as documented.       | **Clear & Transparent.** Payload schema match public API documentation exactly.                                |   **PASS**   |
| **`PersData-Custom-Target-Access-Secrets`**    | Ruuvi Privacy Notice & Web-UI Setup Manuals                          | **Confirmed.** Passwords, bearer tokens, and private SSL keys reside in `nvs`/`gw_cfg_def` flash; never transmitted to Ruuvi Cloud.                     | **Clear & Transparent.** User documentation accurately describes local storage and client-side mTLS usage.     |   **PASS**   |
| **`PersData-BLE-Sensor-Telemetry`**            | Ruuvi Privacy Notice & Cloud Portal Guide                            | **Confirmed.** Sensor metrics stream over HTTPS to Ruuvi Cloud when active. Account deletion (`DelFunc-Service-Account-Deletion`) purges cloud logs.    | **Clear & Transparent.** Data processing, subscription retention, and deletion pathways are fully transparent. |   **PASS**   |

**Assessment Justification**: Functional audit of the published user information confirms that the
disclosures provided to consumers are written in clear, transparent language without ambiguous legal
jargon. Cross-referencing wire captures and device memory confirms that actual data processing
activities, authorized receivers, and storage lifecycles match the published documentation and
`IXIT 21-PersData` in every detail.

**Verdict**: **PASS**

---

## Summary Matrix for Test Case 6-1-1 & 6-1-2

| Test Case        | Purpose / Focus                       | Assessment Summary                                                                                                                                 | Unit Verdict |
|:-----------------|:--------------------------------------|:---------------------------------------------------------------------------------------------------------------------------------------------------|:------------:|
| **6-1-1 Unit a** | Conceptual Transparency Assessment    | `IXIT 2-UserInfo` documentation covers data type, processing purpose, authorized parties, and storage duration for all 6 personal data categories. |   **PASS**   |
| **6-1-2 Unit a** | Information Transparency Check        | Consumer-facing disclosures (`ruuvi.com/privacy/` and `docs.ruuvi.com`) are clear, accessible, and transparent.                                    |   **PASS**   |
| **6-1-2 Unit b** | Functional Information Accuracy Check | Physical wire captures and memory audits confirm actual processing activities match published disclosures and `IXIT 21-PersData` exactly.          |   **PASS**   |

---

## Group Summary

The Ruuvi Gateway complies fully with Mandatory Provision 6-1 of `ETSI EN 303 645`. Clear,
transparent, and comprehensive information regarding the processing of personal data is provided to
consumers via the public Ruuvi Privacy Policy (`https://ruuvi.com/privacy/`) and technical
documentation portal (`https://docs.ruuvi.com/ruuvi-gateway-firmware/`). The documentation
accurately specifies the types of personal data processed (`IXIT 21-PersData`), the explicit
operational purposes for processing, the authorized receiving organizations, and the defined storage
lifecycles. Functional network traffic analysis and system memory audits confirm that actual device
data processing strictly matches the published user disclosures.

**Group Verdict**: **PASS**