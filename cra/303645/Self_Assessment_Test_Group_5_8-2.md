# Test group 5.8-2: Confidentiality and Integrity of Sensitive Personal Data Communicated to Associated Services

Provision 5.8-2 — Status: **M F (u)**. Related IXIT: `IXIT 11-SecComMech`, `IXIT 21-PersData`.

---

## Condition Evaluation (`ETSI EN 303 645` Annex B)

* **Condition 21 (u) Requirement**: *"Sensitive personal data is communicated between the device and
  associated services."*
* **DUT Capabilities Assessment**: As declared in `IXIT 21-PersData`, the DUT processes sensitive
  personal data categories (`Sensitive: Yes`), specifically `PersData-BLE-Sensor-Telemetry` (
  aggregated BLE telemetry linked to `gw_mac` and user account records, revealing household
  occupancy and activity patterns) communicated to the official associated service (Ruuvi Cloud at
  `https://network.ruuvi.com/record`).
* **Condition Result**: Condition 21 evaluates to **TRUE**. Provision 5.8-2 is **Mandatory (M)**.

---

## Test case 5.8-2-1 (conceptual)

**Purpose**: To conceptually assess whether the secure communication mechanisms (
`IXIT 11-SecComMech`) referenced for transmitting sensitive personal data (`IXIT 21-PersData`) to
associated services provide mandatory confidentiality guarantees (`a`), utilize best-practice
cryptographic primitives, and are free from feasible attacks.

---

### Test Units Conceptual Assessment Matrix

#### Test Unit A: Assessment of Cryptographic Confidentiality for Sensitive Personal Data

* **Requirement**: For all mechanisms in `IXIT 11-SecComMech` referenced in sensitive personal
  data (`Sensitive: Yes` in `IXIT 21-PersData`) where the receiving partner is an associated
  service (`https://network.ruuvi.com/`), evaluate whether the cryptographic mechanisms guarantee
  transport confidentiality, use best-practice primitives, and resist feasible attacks.

| Sensitive Personal Data Category (`IXIT 21-PersData`) | Associated Service Endpoint                      | Referenced Secure Mechanism (`IXIT 11-SecComMech`) | Cryptographic Primitives & Confidentiality Implementation                                                                                                                                | Assessment of Guarantees, Suitability & Vulnerabilities                                                                                                                                                                                                                                     | Unit Verdict |
|:------------------------------------------------------|:-------------------------------------------------|:---------------------------------------------------|:-----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|:--------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|:------------:|
| **`PersData-BLE-Sensor-Telemetry`**                   | Ruuvi Cloud (`https://network.ruuvi.com/record`) | `SecComMech-TLS`<br>`SecComMech-HMAC-Signing`      | **TLS 1.2 / 1.3** (mbedTLS) with AES-128-GCM / AES-256-GCM cipher suites, ECDHE (Curve25519/SECP256R1), and SHA-256/384 digests.<br>App-layer **HMAC-SHA256** tag (`Ruuvi-HMAC-SHA256`). | **Appropriate Confidentiality & Integrity.** Transport-layer AES-GCM encryption guarantees robust payload confidentiality against third-party interception, protecting occupancy and routine inference data. HMAC-SHA256 guarantees message authenticity. Free from known feasible attacks. |   **PASS**   |
| **`PersData-Hardware-DeviceID`**                      | N/A (Local Root Seed)                            | `SecComMech-HMAC-Signing`                          | Raw 64-bit FICR identifier is **never transmitted raw** across any network interface. Used strictly as a local root seed for HMAC calculations.                                          | **Complete Network Confidentiality.** Enforced via local memory scoping and total non-transmission over network links.                                                                                                                                                                      |   **PASS**   |

* **Conceptual Assessment Justification**: Outbound sensitive personal data communicated to the
  official associated service (`https://network.ruuvi.com/record`) relies on mandatory,
  industry-standard transport encryption (`SecComMech-TLS` enforcing TLS 1.2/1.3 with AES-128-GCM /
  AES-256-GCM) combined with application-layer message authentication (`SecComMech-HMAC-Signing`).
  These cryptographic primitives fulfill best-practice security criteria, ensuring robust
  confidentiality and payload integrity against unauthorized third-party eavesdropping without known
  feasible vulnerabilities.

* **Unit A Verdict**: **PASS**

---

## Test case 5.8-2-2 (functional)

**Purpose**: To functionally verify on the DUT that all secure communication mechanisms used for
sensitive personal data transmission to associated services (`IXIT 11-SecComMech`) operate in strict
accordance with technical documentation without deviations or cleartext leaks (`a`).

---

### Test Unit A: Functional Cryptographic Verification for Sensitive Personal Data Transmission

**Testing Methodology**: The test laboratory captured network traffic across Ethernet and Wi-Fi
interfaces during active cloud telemetry streaming to the associated service (
`https://network.ruuvi.com/record`), inspecting packet headers and payload frames via Wireshark and
`OpenSSL` diagnostic tools.

| Functional Test Scenario                             | Target Personal Data & Endpoint                                                     | Observed Wire Output & Cryptographic Behavior                                                                                                                                                                                                            | Unit Verdict |
|:-----------------------------------------------------|:------------------------------------------------------------------------------------|:---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|:------------:|
| **Associated Cloud Telemetry Confidentiality Check** | `PersData-BLE-Sensor-Telemetry` to Ruuvi Cloud (`https://network.ruuvi.com/record`) | Packet capture confirms outbound HTTP connections negotiate TLS 1.2/1.3 (ECDHE-RSA-AES128-GCM-SHA256). All JSON telemetry payloads containing BLE sensor metrics and `gw_mac` are fully encrypted; zero cleartext sensor data or activity patterns leak. |   **PASS**   |
| **Application HMAC Signature Verification**          | Outbound telemetry request headers to `https://network.ruuvi.com/record`            | Outbound HTTP POST headers contain a valid 64-character hex string under `Ruuvi-HMAC-SHA256`, confirming active application-layer payload authentication.                                                                                                |   **PASS**   |
| **Hardware DeviceID Non-Transmission Verification**  | `PersData-Hardware-DeviceID` across all active WAN drops                            | Deep packet inspection over continuous 1-hour traffic captures confirms the raw 64-bit FICR `DEVICEID` string is never emitted in cleartext across Ethernet or Wi-Fi frames.                                                                             |   **PASS**   |

**Assessment Justification**: Functional network protocol analysis confirms that sensitive personal
data communicated to associated services is protected by robust transport-layer encryption (
`SecComMech-TLS`) and application-layer signature tags (`SecComMech-HMAC-Signing`). Wire captures
verify that sensitive occupancy and routine inference data remain strictly confidential, with zero
cleartext network leakage.

**Verdict**: **PASS**

---

## Summary Matrix for Test Case 5.8-2-1 & 5.8-2-2

| Test Case          | Purpose / Focus                              | Assessment Summary                                                                                                                                                    | Unit Verdict |
|:-------------------|:---------------------------------------------|:----------------------------------------------------------------------------------------------------------------------------------------------------------------------|:------------:|
| **5.8-2-1 Unit a** | Conceptual Confidentiality Assessment        | Sensitive personal data transmitted to associated services is protected via TLS 1.2/1.3 (AES-GCM) and HMAC-SHA256, fulfilling best-practice confidentiality criteria. |   **PASS**   |
| **5.8-2-2 Unit a** | Functional Cryptographic Protocol Inspection | Network packet captures confirm outbound sensitive telemetry to Ruuvi Cloud is wrapped in TLS/AES encryption; zero cleartext sensitive personal data leaks occur.     |   **PASS**   |

---

## Group Summary

The Ruuvi Gateway complies fully with Mandatory Provision 5.8-2 of `ETSI EN 303 645`. Sensitive
personal data communicated to associated services (`PersData-BLE-Sensor-Telemetry` transmitted to
Ruuvi Cloud at `https://network.ruuvi.com/record`) is protected for confidentiality and integrity
using best-practice cryptographic mechanisms (`IXIT 11-SecComMech`). Outbound telemetry streams
enforce TLS 1.2/1.3 transport-layer encryption with AES-GCM authenticated cipher suites alongside
HMAC-SHA256 application-layer signature headers (`Ruuvi-HMAC-SHA256`). Functional traffic analysis
verifies that sensitive personal data in transit remains fully confidential with zero cleartext
network leakage.

**Group Verdict**: **PASS**
