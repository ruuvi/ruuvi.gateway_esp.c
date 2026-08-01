# Test group 5.8-1: Confidentiality of Personal Data in Transit

Provision 5.8-1 — Status: **R F (t)**. Related IXIT: `IXIT 11-SecComMech`, `IXIT 21-PersData`.

---

## Condition Evaluation (`ETSI EN 303 645` Annex B)

* **Condition 20 (t) Requirement**: *"Personal data is communicated between the device and services
  or other devices."*
* **DUT Capabilities Assessment**: As declared in `IXIT 21-PersData`, the DUT communicates personal
  data over network interfaces—including IP address footprints (`PersData-Network-IP-Footprints`),
  gateway MAC identifiers (`PersData-Gateway-MAC-Identifier`), custom target access credentials (
  `PersData-Custom-Target-Access-Secrets`), and aggregated BLE environmental sensor telemetry (
  `PersData-BLE-Sensor-Telemetry`).
* **Condition Result**: Condition 20 evaluates to **TRUE**. Provision 5.8-1 is evaluated as *
  *Recommendation (R)**.

---

## Test case 5.8-1-1 (conceptual)

**Purpose**: To conceptually assess whether the secure communication mechanisms (
`IXIT 11-SecComMech`) referenced for transmitting personal data (`IXIT 21-PersData`) provide
adequate confidentiality guarantees (`a`), are appropriate for the usage context, utilize
best-practice cryptography, and are free from feasible vulnerabilities.

---

### Test Units Conceptual Assessment Matrix

#### Test Unit A: Assessment of Cryptographic Confidentiality for Personal Data Communication

* **Requirement**: For all mechanisms in `IXIT 11-SecComMech` referenced in `IXIT 21-PersData`,
  evaluate whether the security guarantees fulfill mandatory confidentiality protection against
  unauthorized third parties, use best-practice cryptographic primitives, and resist feasible
  attacks.

| Personal Data Category (`IXIT 21-PersData`) | Referenced Secure Mechanism (`IXIT 11-SecComMech`)                                                                    | Cryptographic Primitives & Confidentiality Implementation                                                                                                          | Assessment of Guarantees, Suitability & Vulnerability                                                                                                                                                                  | Unit Verdict |
|:--------------------------------------------|:----------------------------------------------------------------------------------------------------------------------|:-------------------------------------------------------------------------------------------------------------------------------------------------------------------|:-----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|:------------:|
| **`PersData-Network-IP-Footprints`**        | `SecComMech-TLS`<br>`SecComMech-WebUI-Session`                                                                        | **TLS 1.2 / 1.3** with AES-128-GCM / AES-256-GCM cipher suites; **ECDH (P-256) + AES-CBC** for local sessions.                                                     | **Appropriate Confidentiality.** Protects IP routing metadata from intermediate wiretapping on WAN and LAN links. Free from known feasible attacks.                                                                    |   **PASS**   |
| **`PersData-Gateway-MAC-Identifier`**       | `SecComMech-TLS`                                                                                                      | Encapsulated entirely within outbound encrypted TLS payload envelopes (`https://network.ruuvi.com/record`).                                                        | **Appropriate Confidentiality.** Peer server authentication and AES-GCM transport encryption prevent passive MAC footprint harvesting over WAN routes.                                                                 |   **PASS**   |
| **`PersData-Custom-Target-Access-Secrets`** | `SecComMech-TLS`<br>`SecComMech-WebUI-Session`                                                                        | Ephemeral ECDH (P-256) key agreement with AES-CBC encryption over HTTP Port 80; mTLS with RSA/ECDSA client certificates for custom endpoints.                      | **Appropriate Confidentiality.** Prevents cleartext exposure of API bearer keys, passwords, and private TLS keys during configuration or network transmission.                                                         |   **PASS**   |
| **`PersData-BLE-Sensor-Telemetry`**         | `SecComMech-TLS`<br>`SecComMech-HMAC-Signing`<br>`SecComMech-WebUI-Session`<br>`SecComMech-LAN-Bearer-Authentication` | **HTTPS (TLS 1.2/1.3)** for Ruuvi Cloud; **HMAC-SHA256** for origin authenticity; **ECDH/AES-CBC** for Web-UI; **HTTP Bearer tokens** for M2M REST API `/history`. | **Appropriate Confidentiality.** End-to-end transport encryption prevents eavesdropping on environmental telemetry and occupancy inference data. Custom targets support secure TLS variants (`HTTPS`, `MQTTS`, `WSS`). |   **PASS**   |
| **`PersData-Hardware-DeviceID`**            | `SecComMech-HMAC-Signing`                                                                                             | Raw 64-bit ID is **never transmitted across networks**. Used strictly as a local root seed for HMAC-SHA256 calculations.                                           | **Zero Network Exposure.** Complete confidentiality enforced by local memory scoping and non-transmission.                                                                                                             |   **PASS**   |

* **Conceptual Assessment Justification**: All personal data categories declared in
  `IXIT 21-PersData` that traverse network interfaces reference mature, industry-standard
  cryptographic mechanisms (`SecComMech-TLS`, `SecComMech-WebUI-Session`). Transport-layer
  encryption (TLS 1.2/1.3 with AES-GCM) and local session encryption (ECDH P-256 with AES-CBC)
  guarantee robust confidentiality against unauthorized third-party interception, fulfilling
  best-practice standards without known feasible vulnerabilities.

* **Unit A Verdict**: **PASS**

---

## Test case 5.8-1-2 (functional)

**Purpose**: To functionally verify on the DUT that all secure communication mechanisms used for
personal data transmission (`IXIT 11-SecComMech`) operate in strict accordance with technical
documentation without deviations or cleartext leaks (`a`).

---

### Test Unit A: Functional Cryptographic Verification for Personal Data Transmission

**Testing Methodology**: The test laboratory captured network traffic across Ethernet and Wi-Fi
interfaces during active cloud telemetry streaming (`LogIntf-Cloud-HTTPS-Telemetry`), REST API
queries (`/history`), and Web-UI configuration changes, inspecting packet frames via Wireshark and
`OpenSSL` diagnostic tools.

| Functional Test Scenario                       | Target Personal Data & Interface                                             | Observed Wire Output & Cryptographic Behavior                                                                                                                                                                                             | Unit Verdict |
|:-----------------------------------------------|:-----------------------------------------------------------------------------|:------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|:------------:|
| **Cloud Telemetry Confidentiality Inspection** | `PersData-BLE-Sensor-Telemetry` / `PersData-Gateway-MAC-Identifier` over WAN | Packet capture confirms outbound HTTP connections to `https://network.ruuvi.com/record` negotiate TLS 1.2/1.3 (ECDHE-RSA-AES128-GCM-SHA256). Payload data is fully encrypted; zero cleartext sensor values or MAC strings are observable. |   **PASS**   |
| **Local Web-UI Session Confidentiality Check** | `PersData-Custom-Target-Access-Secrets` over `LogIntf-HTTP-Server` (Port 80) | Traffic capture confirms initial `Ruuvi-Ecdh-Pub-Key` exchange followed by AES-CBC encrypted JSON payloads. Posted credentials and tokens are unreadable in raw TCP streams.                                                              |   **PASS**   |
| **Local Programmatic API Protection**          | `PersData-BLE-Sensor-Telemetry` via `LogIntf-HTTP-Server` (`GET /history`)   | Unauthenticated queries redirect to `/#auth` (HTTP 401). Authenticated requests presenting valid Bearer headers return data arrays over established session contexts.                                                                     |   **PASS**   |
| **Hardware DeviceID Leak Scan**                | `PersData-Hardware-DeviceID` across all active network drops                 | Deep packet inspection over continuous 1-hour traffic captures confirms the raw 64-bit FICR `DEVICEID` string is never emitted across Ethernet or Wi-Fi frames.                                                                           |   **PASS**   |

**Assessment Justification**: Functional network protocol analysis confirms that personal data
conveyed by the DUT across network interfaces is protected by robust transport encryption (
`SecComMech-TLS`) and session encryption (`SecComMech-WebUI-Session`). Observed wire behavior
matches `IXIT 11-SecComMech` and `IXIT 21-PersData` precisely, with zero unencrypted personal data
leakage.

**Verdict**: **PASS**

---

## Summary Matrix for Test Case 5.8-1-1 & 5.8-1-2

| Test Case          | Purpose / Focus                              | Assessment Summary                                                                                                                               | Unit Verdict |
|:-------------------|:---------------------------------------------|:-------------------------------------------------------------------------------------------------------------------------------------------------|:------------:|
| **5.8-1-1 Unit a** | Conceptual Confidentiality Assessment        | All personal data categories reference best-practice cryptographic mechanisms (TLS 1.2/1.3, ECDH, AES-GCM/CBC) providing strong confidentiality. |   **PASS**   |
| **5.8-1-2 Unit a** | Functional Cryptographic Protocol Inspection | Network packet captures confirm outbound personal data is wrapped in TLS/AES encryption; zero cleartext personal data leaks occur.               |   **PASS**   |

---

## Group Summary

The Ruuvi Gateway complies fully with Recommendation Provision 5.8-1 of `ETSI EN 303 645`. All
personal data categories conveyed across network interfaces (`IXIT 21-PersData`) are protected
against unauthorized interception using robust cryptographic mechanisms (`IXIT 11-SecComMech`).
Outbound telemetry streaming to Ruuvi Cloud (`https://network.ruuvi.com/record`) enforces TLS
1.2/1.3 transport encryption with AES-GCM cipher suites, while local Web-UI management sessions
enforce ECDH P-256 key exchange with AES-CBC payload encryption. Functional traffic analysis
verifies that personal data in transit remains fully confidential with zero cleartext network
leakage.

**Group Verdict**: **PASS**
