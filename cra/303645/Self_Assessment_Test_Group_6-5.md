# Test group 6-5: Consumer Information About Processing Telemetry Data

Provision 6-5 — Status: **M**. Related IXIT: `IXIT 2-UserInfo`, `IXIT 24-TelData`.

---

## Test case 6-5-1 (conceptual)

**Purpose**: To conceptually assess whether the "Documentation of Telemetry Data" in
`IXIT 2-UserInfo` is suitable for the consumer to obtain information about the processing of
telemetry data (`a`).

---

### Test Units Conceptual Assessment Matrix

#### Test Unit A: Assessment of Information Accessibility and Suitability

| Documentation Entry (`IXIT 2-UserInfo`) | Publication Vector & Access URL                                                                                                                                         | Information Provided to Consumer                                                                                                                                                                                     | Suitability Evaluation                                                                                                                            | Unit Verdict |
|:----------------------------------------|:------------------------------------------------------------------------------------------------------------------------------------------------------------------------|:---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|:--------------------------------------------------------------------------------------------------------------------------------------------------|:------------:|
| **Documentation of Telemetry Data**     | Technical Documentation Portal (`https://docs.ruuvi.com/ruuvi-gateway-firmware/data-formats/http-gateway-status`) & Ruuvi Privacy Notice (`https://ruuvi.com/privacy/`) | Publicly describes structural payload schemas, status heartbeat fields, crash log strings (`RESET_INFO`), collection interval limits, egress target URLs (`https://network.ruuvi.com/status`), and opt-out controls. | **Suitable.** Information is published in an easily accessible online portal using clear language suitable for consumers and technical operators. |   **PASS**   |

* **Conceptual Assessment Justification**: `IXIT 2-UserInfo` under "Documentation of Telemetry Data"
  provides direct, public access vectors (`docs.ruuvi.com` and `ruuvi.com/privacy/`) where consumers
  can easily obtain complete information regarding what telemetry data is processed, how it is
  collected, who processes it, and how to opt-out.

* **Unit A Verdict**: **PASS**

---

## Test case 6-5-2 (functional)

**Purpose**: To functionally assess whether the provided information about processing telemetry data
is accessible as described (`a`), matches the declared purposes in `IXIT 24-TelData` (`b`),
describes what telemetry data is collected (`c`), and completely describes how telemetry data is
used, by whom, and for what purposes (`d`).

---

### Test Units Functional Assessment Matrix

#### Test Unit A, B, C & D: Functional Audit of Published Telemetry Documentation

**Testing Methodology**: The test laboratory accessed the telemetry documentation published at
`https://docs.ruuvi.com/ruuvi-gateway-firmware/data-formats/http-gateway-status` (`a`),
cross-referenced the published telemetry descriptions against `IXIT 24-TelData` (`b`), verified that
all collected telemetry payloads are documented (`c`), and confirmed that processing usages,
authorized entities, and purposes are completely described (`d`).

| Telemetry Schema ID (`IXIT 24-TelData`)     | Published Telemetry Description (`IXIT 2-UserInfo`)               | Alignment with IXIT Purpose (`IXIT 24-TelData`) (Unit b)                                                                                                       | Description of Collected Metrics (Unit c)                                                                                                         | Description of Usage, Authorized Parties & Purpose (Unit d)                                                                                                            | Unit Verdict |
|:--------------------------------------------|:------------------------------------------------------------------|:---------------------------------------------------------------------------------------------------------------------------------------------------------------|:--------------------------------------------------------------------------------------------------------------------------------------------------|:-----------------------------------------------------------------------------------------------------------------------------------------------------------------------|:------------:|
| **`TelData-System-Status-Heartbeat`**       | Documented under `http-gateway-status` data format page.          | **Matches IXIT.** Accurately states purpose is calculating reliability baselines, debugging heap/stack allocations, and optimizing firmware stability.         | **Complete.** Documents task stack sizes (`MIN_FREE_STACK_SIZE`), free heap memory, uptime, connection loss counters, and beacon tracking counts. | **Complete.** Declares metrics are transmitted to `https://network.ruuvi.com/status` for server-side health tracing by Ruuvi Innovations Oy; notes opt-out via Step 8. |   **PASS**   |
| **`TelData-Crash-Panic-Diagnostics`**       | Documented under `http-gateway-status` crash reporting section.   | **Matches IXIT.** Accurately states purpose is capturing kernel exception backtraces and register dumps to diagnose runtime deadlocks and memory corruption.   | **Complete.** Documents CPU registers, register dumps, `RESET_REASON`, panic strings (`RESET_INFO`), and ELF build hashes.                        | **Complete.** Declares post-reboot crash reports are transmitted to Ruuvi engineering staff for GDB symbol analysis; private custom target option noted.               |   **PASS**   |
| **`TelData-Environmental-Sensor-Payloads`** | Documented under core data routing and HTTP/MQTT payload formats. | **Matches IXIT.** Accurately states purpose is routing raw environmental BLE advertisement metrics (temperature, humidity, air pressure, motion) to endpoints. | **Complete.** Documents `gw_mac`, timestamp, RSSI, BLE PHY/channel, and raw advertisement hex payload arrays (`data`).                            | **Complete.** Declares data is routed to `https://network.ruuvi.com/record` or user-defined HTTP/MQTT targets for real-time monitoring and graphing.                   |   **PASS**   |

* **Functional Assessment Justification**:
  1. **Accessibility (Unit a):** Telemetry documentation is easily obtainable via public URLs
     without authentication or paywalls.
  2. **IXIT Purpose Alignment (Unit b):** Published telemetry descriptions match the purposes
     defined in `IXIT 24-TelData` precisely.
  3. **Collected Data Completeness (Unit c):** Every collected telemetry payload element—including
     task stack high-water marks, free heap memory, crash panic backtraces, and raw BLE sensor hex
     blocks—is explicitly described.
  4. **Usage, Recipient, and Purpose Transparency (Unit d):** The documentation completely describes
     who processes the telemetry data (Ruuvi Innovations Oy or custom server operators), how it is
     used (health tracing, crash analysis, environmental graphing), and how users can opt-out of
     statistics transmission during setup Step 8 (`UserDec-8`).

* **Verdict**: **PASS**

---

## Summary Matrix for Test Case 6-5-1 & 6-5-2

| Test Case        | Purpose / Focus                     | Assessment Summary                                                                                                                            | Unit Verdict |
|:-----------------|:------------------------------------|:----------------------------------------------------------------------------------------------------------------------------------------------|:------------:|
| **6-5-1 Unit a** | Conceptual Accessibility Assessment | "Documentation of Telemetry Data" in `IXIT 2-UserInfo` provides clear, accessible public vectors (`docs.ruuvi.com` and `ruuvi.com/privacy/`). |   **PASS**   |
| **6-5-2 Unit a** | Documentation Access Check          | Telemetry information can be obtained cleanly via documented public URLs without paywalls or login restrictions.                              |   **PASS**   |
| **6-5-2 Unit b** | IXIT Alignment Check                | Published telemetry descriptions match the processing purposes declared in `IXIT 24-TelData` in every detail.                                 |   **PASS**   |
| **6-5-2 Unit c** | Collected Metrics Description Check | Published documentation completely describes all collected telemetry parameters (stack sizes, heap memory, crash backtraces, BLE hex data).   |   **PASS**   |
| **6-5-2 Unit d** | Processing Usage & Entity Check     | Published documentation completely describes who uses the telemetry data, for what operational purposes, and how to opt-out.                  |   **PASS**   |

---

## Group Summary

The Ruuvi Gateway complies fully with Mandatory Provision 6-5 of `ETSI EN 303 645`. Information
concerning the processing of telemetry data is suitably provided to consumers via the public Ruuvi
technical documentation portal (
`https://docs.ruuvi.com/ruuvi-gateway-firmware/data-formats/http-gateway-status`) and privacy
policy (`https://ruuvi.com/privacy/`). Functional auditing confirms that the published information
is easily obtainable, matches the technical declarations in `IXIT 24-TelData` precisely, completely
describes what telemetry metrics are collected (system health stack sizes, post-reboot panic
backtraces, raw BLE environmental sensor payloads), and transparently details who processes the
data, how it is used for maintenance and debugging, and how consumers can opt-out during onboarding.

**Group Verdict**: **PASS**
