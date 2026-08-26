# Test group 6-7: Data Aggregation Principles for Personal Data

Provision 6-7 — Status: **R**. Related IXIT: `IXIT 21-PersData`.

---

## Condition & Scope Assessment (`ETSI EN 303 645` Recommendation 6-7)

* **Provision Scope Requirement**: Applies to personal data processed by the DUT or associated
  services where the *sole purpose* of collecting and processing the data is to compute an aggregate
  result.
* **DUT Capabilities & IXIT Audit**:
  * As cataloged in `IXIT 21-PersData` across all six declared personal data categories (
    `PersData-Network-IP-Footprints`, `PersData-Gateway-LAN-MAC`, `PersData-Hardware-DeviceID`,
    `PersData-Gateway-MAC-Identifier`, `PersData-Custom-Target-Access-Secrets`,
    `PersData-BLE-Sensor-Telemetry`), the "Aggregation" attribute is explicitly set to **`No`**.
  * Personal data is collected and processed for direct operational purposes — including Layer 2/3
    network packet routing, Web-UI session validation, cryptographic HMAC payload signing, client
    mTLS authentication, and real-time sensor measurement forwarding — and **not** for the sole
    purpose of computing aggregate statistical results.
* **Applicability Result**: Provision 6-7 is **Not Applicable (N/A)** to the Ruuvi Gateway platform.

---

## Test case 6-7-1 (conceptual)

**Purpose**: To conceptually assess whether data whose sole purpose is to be computed into an
aggregate result is minimal (`a`), aggregated as early as possible (`b`), and retained for a
minimized duration (`c`).

---

### Test Units Conceptual Assessment Matrix

| Personal Data Category ID (`IXIT 21-PersData`) | Aggregation Flag (`IXIT 21-PersData`) | Sole Purpose of Data Collection                                     | Conceptual Aggregation Assessment             | Unit Verdict |
|:-----------------------------------------------|:-------------------------------------:|:--------------------------------------------------------------------|:----------------------------------------------|:------------:|
| **`PersData-Network-IP-Footprints`**           |                 `No`                  | Operational network socket routing & Web-UI session validation.     | Not processed for sole aggregate computation. |   **N/A**    |
| **`PersData-Gateway-LAN-MAC`**                 |                 `No`                  | Layer 2 Ethernet frame switching & ARP query resolution.            | Not processed for sole aggregate computation. |   **N/A**    |
| **`PersData-Hardware-DeviceID`**               |                 `No`                  | Factory default Web-UI password & HMAC telemetry signing root seed. | Not processed for sole aggregate computation. |   **N/A**    |
| **`PersData-Gateway-MAC-Identifier`**          |                 `No`                  | Primary origin header for telemetry envelope server mapping.        | Not processed for sole aggregate computation. |   **N/A**    |
| **`PersData-Custom-Target-Access-Secrets`**    |                 `No`                  | Machine authentication & mTLS client encryption assets.             | Not processed for sole aggregate computation. |   **N/A**    |
| **`PersData-BLE-Sensor-Telemetry`**            |                 `No`                  | Real-time environmental metrics forwarding & time-series graphing.  | Not processed for sole aggregate computation. |   **N/A**    |

* **Conceptual Assessment Justification**: `IXIT 21-PersData` confirms that no personal data
  category collected by the DUT has the sole purpose of computing an aggregate result. Test units
  `a`, `b`, and `c` evaluate as Not Applicable.

* **Verdict**: **PASS (N/A)**

---

## Test case 6-7-2 (functional)

**Purpose**: To functionally assess whether the data aggregation mechanisms operating on personal
data perform strictly as specified in `IXIT 21-PersData` (`a`, `b`).

---

### Test Units Functional Assessment Matrix

| Test Unit        | Functional Assessment Scope                                                        | Functional Evaluation & Verification Result                                                                                                                                     | Unit Verdict |
|:-----------------|:-----------------------------------------------------------------------------------|:--------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|:------------:|
| **6-7-2 Unit a** | Creation of typical personal data foreseen to be aggregated in `IXIT 21-PersData`. | Network wire captures and firmware task audits confirm zero personal data categories are designated or collected for sole aggregation processing.                               |   **N/A**    |
| **6-7-2 Unit b** | Functional verification of aggregation mechanisms on personal data.                | No aggregation-only processing loops exist in the firmware or associated cloud backend services. Data is processed directly for operational forwarding and dashboard rendering. |   **N/A**    |

* **Functional Assessment Justification**: Because no personal data category in `IXIT 21-PersData`
  is designated for sole aggregation processing, functional testing confirms there is no indication
  that data aggregation differs from IXIT declarations.

* **Verdict**: **PASS (N/A)**

---

## Summary Matrix for Test Case 6-7-1 & 6-7-2

| Test Case           | Purpose / Focus                         | Assessment Summary                                                                                                                     |  Unit Verdict  |
|:--------------------|:----------------------------------------|:---------------------------------------------------------------------------------------------------------------------------------------|:--------------:|
| **6-7-1 Units a–c** | Conceptual Aggregation Principles Audit | No personal data category in `IXIT 21-PersData` is collected for the sole purpose of aggregate computation.                            | **PASS (N/A)** |
| **6-7-2 Units a–b** | Functional Aggregation Verification     | Audits confirm zero personal data is processed via sole-aggregation pipelines; operational data forwarding matches IXIT documentation. | **PASS (N/A)** |

---

## Group Summary

The Ruuvi Gateway complies with Recommendation Provision 6-7 of `ETSI EN 303 645`. As declared in
`IXIT 21-PersData`, the device and its associated services do not collect or process any personal
data for the sole purpose of computing aggregate results (`Aggregation: No`). All personal data
categories are processed directly to support essential network routing, user authentication,
cryptographic signing, or real-time environmental telemetry forwarding. Consequently, the specific
aggregation principles under Test Group 6-7 evaluate as Not Applicable (N/A) with a final group
verdict of PASS.

**Group Verdict**: **PASS (N/A)**
