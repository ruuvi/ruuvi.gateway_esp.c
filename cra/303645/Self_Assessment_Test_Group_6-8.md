# Test group 6-8: Data Anonymization Technologies for Personal Data

Provision 6-8 — Status: **R**. Related IXIT: `IXIT 21-PersData`.

---

## Condition & Scope Assessment (`ETSI EN 303 645` Recommendation 6-8)

* **Provision Scope Requirement**: Applies to consumer IoT platforms that deploy explicit data
  anonymization technologies (e.g., differential privacy, pseudonymization hashes, or dynamic ID
  mask transformations) to personal data during collection, processing, and storage.
* **DUT Capabilities & IXIT Audit**:
  * As cataloged in `IXIT 21-PersData` across all six declared personal data categories (
    `PersData-Network-IP-Footprints`, `PersData-Gateway-LAN-MAC`, `PersData-Hardware-DeviceID`,
    `PersData-Gateway-MAC-Identifier`, `PersData-Custom-Target-Access-Secrets`,
    `PersData-BLE-Sensor-Telemetry`), the "Anonymization" attribute is explicitly declared as **`No`
    **.
  * Network routing footprints, hardware interface MAC addresses (`gw_mac`), and BLE tag
    advertisement payloads preserve explicit, unmasked structural identifiers across runtime memory
    buffers and egress transport wrappers. This static identity preservation is technically
    necessary to allow backend server architectures to map incoming metrics to registered user
    accounts, perform time-series graphing, and validate HMAC signatures.
* **Applicability Result**: Provision 6-8 is **Not Applicable (N/A)** to the Ruuvi Gateway platform.

---

## Test case 6-8-1 (conceptual)

**Purpose**: To conceptually assess whether appropriate data anonymization technologies are used to
protect privacy during personal data collection, processing, and storage (`a`).

---

### Test Units Conceptual Assessment Matrix

| Personal Data Category ID (`IXIT 21-PersData`) | Anonymization Flag (`IXIT 21-PersData`) | Processing Identity Requirements                          | Conceptual Anonymization Technology Assessment                      | Unit Verdict |
|:-----------------------------------------------|:---------------------------------------:|:----------------------------------------------------------|:--------------------------------------------------------------------|:------------:|
| **`PersData-Network-IP-Footprints`**           |                  `No`                   | Standard L3/L4 TCP/IP socket packet encapsulation.        | Anonymization not applied; raw IP required for socket transport.    |   **N/A**    |
| **`PersData-Gateway-LAN-MAC`**                 |                  `No`                   | Local Layer 2 Ethernet frame switching & ARP queries.     | Anonymization not applied; raw MAC required for L2 switching.       |   **N/A**    |
| **`PersData-Hardware-DeviceID`**               |                  `No`                   | Local Web-UI password & HMAC signing root seed.           | Anonymization not applied; raw seed required for HMAC generation.   |   **N/A**    |
| **`PersData-Gateway-MAC-Identifier`**          |                  `No`                   | Outbound JSON telemetry origin mapping header (`gw_mac`). | Anonymization not applied; fixed MAC required for account mapping.  |   **N/A**    |
| **`PersData-Custom-Target-Access-Secrets`**    |                  `No`                   | Machine authentication credentials & mTLS private keys.   | Anonymization not applied; plaintext secrets required for mTLS.     |   **N/A**    |
| **`PersData-BLE-Sensor-Telemetry`**            |                  `No`                   | Real-time BLE sensor monitoring & time-series graphing.   | Anonymization not applied; explicit sensor IDs required for graphs. |   **N/A**    |

* **Conceptual Assessment Justification**: `IXIT 21-PersData` confirms that no personal data
  category collected or processed by the DUT uses anonymization technologies. Test unit `a`
  evaluates as Not Applicable.

* **Verdict**: **PASS (N/A)**

---

## Test case 6-8-2 (functional)

**Purpose**: To functionally assess whether data anonymization technologies perform strictly as
specified in `IXIT 21-PersData` ("Anonymization") (`a`, `b`).

---

### Test Units Functional Assessment Matrix

| Test Unit        | Functional Assessment Scope                                                        | Functional Evaluation & Verification Result                                                                                                                 | Unit Verdict |
|:-----------------|:-----------------------------------------------------------------------------------|:------------------------------------------------------------------------------------------------------------------------------------------------------------|:------------:|
| **6-8-2 Unit a** | Creation of typical personal data foreseen to be anonymized in `IXIT 21-PersData`. | Wire captures and memory dumps confirm zero personal data categories are designated or configured for data anonymization processing.                        |   **N/A**    |
| **6-8-2 Unit b** | Functional verification that data anonymization works as specified.                | No anonymization routines or dynamic ID masking functions exist in firmware or cloud backend processing paths. Raw identity fields match IXIT declarations. |   **N/A**    |

* **Functional Assessment Justification**: Because no personal data category in `IXIT 21-PersData`
  is designated for data anonymization, functional testing confirms there is no indication that data
  anonymization differs from IXIT documentation.

* **Verdict**: **PASS (N/A)**

---

## Summary Matrix for Test Case 6-8-1 & 6-8-2

| Test Case           | Purpose / Focus                           | Assessment Summary                                                                                                                           |  Unit Verdict  |
|:--------------------|:------------------------------------------|:---------------------------------------------------------------------------------------------------------------------------------------------|:--------------:|
| **6-8-1 Unit a**    | Conceptual Anonymization Technology Audit | No personal data category in `IXIT 21-PersData` deploys data anonymization technologies (`Anonymization: No`).                               | **PASS (N/A)** |
| **6-8-2 Units a–b** | Functional Anonymization Verification     | Audits confirm zero personal data is anonymized; identity fields remain unmasked to support telemetry routing and attribution as documented. | **PASS (N/A)** |

---

## Group Summary

The Ruuvi Gateway complies with Recommendation Provision 6-8 of `ETSI EN 303 645`. As declared in
`IXIT 21-PersData`, the device and its associated services do not deploy data anonymization
technologies (`Anonymization: No`). All personal data categories preserve explicit structural
identifiers across internal RAM queues and outbound transport wrappers to enable essential L2/L3
packet switching, Web-UI session validation, HMAC payload signing, client mTLS authentication, and
backend sensor data attribution. Consequently, the specific anonymization technology assessment
criteria under Test Group 6-8 evaluate as Not Applicable (N/A) with a final group verdict of PASS.

**Group Verdict**: **PASS (N/A)**
