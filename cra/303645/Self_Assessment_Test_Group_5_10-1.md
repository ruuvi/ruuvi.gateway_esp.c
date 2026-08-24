# Test group 5.10-1: Examination of System Telemetry Data for Security Anomalies

Provision 5.10-1 — Status: **R F (w)**. Related IXIT: `IXIT 24-TelData`.

---

## Condition Evaluation (`ETSI EN 303 645` Annex B)

* **Condition 23 (w) Requirement**: *"Telemetry data is collected."*
* **DUT Capabilities Assessment**: As declared in `IXIT 24-TelData`, the DUT collects and transmits
  outbound system telemetry data over network interfaces, including system health status
  heartbeats (`TelData-System-Status-Heartbeat`), crash panic diagnostics (
  `TelData-Crash-Panic-Diagnostics`), and BLE environmental sensor payloads (
  `TelData-Environmental-Sensor-Payloads`).
* **Condition Result**: Condition 23 evaluates to **TRUE**. Provision 5.10-1 is evaluated as *
  *Recommendation (R)**.

---

## Test case 5.10-1-1 (conceptual)

**Purpose**: To conceptually check whether at least one security anomaly examination is specified in
`IXIT 24-TelData` (`a`), and assess whether the associated telemetry data is suited for examining
and identifying security anomalies (`b`).

---

### Test Units Conceptual Assessment Matrix

#### Test Unit A & B: Presence and Suitability Assessment of Telemetry Security Examinations

| Telemetry Schema ID (`IXIT 24-TelData`)     | Collected Telemetry Metrics & Parameters                                                                                                                                                     | Declared Security Examination Workflow                                                                                                     | Suitability for Security Anomaly Detection (Unit b)                                                                                                                                                                     | Unit Verdict |
|:--------------------------------------------|:---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|:-------------------------------------------------------------------------------------------------------------------------------------------|:------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|:------------:|
| **`TelData-System-Status-Heartbeat`**       | FreeRTOS task stack metrics (`MIN_FREE_STACK_SIZE`), heap memory sizes (`TOTAL_FREE_BYTES_INTERNAL`, `LARGEST_FREE_BLOCK_INTERNAL`), uptime, and connection loss counters (`NUM_CONN_LOST`). | Automated server-side tracing routines process incoming metrics at `https://network.ruuvi.com/status` (or custom collector).               | **Highly Suited.** Stack utilization and heap statistics directly enable detection of memory leak vulnerabilities, task starvation, resource exhaustion, and active Denial-of-Service (DoS) buffer flooding attacks.    |   **PASS**   |
| **`TelData-Crash-Panic-Diagnostics`**       | CPU registers, backtrace stack arrays, `RESET_REASON`, panic strings (`RESET_INFO`), and build ELF hashes.                                                                                   | Post-reboot crash reports at `https://network.ruuvi.com/status` are analyzed by engineering staff using GDB tools against ELF symbol maps. | **Highly Suited.** Core dumps and backtraces allow engineers to determine if system crashes stem from coding bugs or malicious memory transgression attempts (e.g., stack buffer overflows or code injection exploits). |   **PASS**   |
| **`TelData-Environmental-Sensor-Payloads`** | Raw BLE advertisement payloads (`data`), gateway MAC (`gw_mac`), timestamp, and RSSI.                                                                                                        | N/A (Stateless environmental metrics relay).                                                                                               | **Not Applicable.** Treated purely as stateless environmental payload forwarding; not evaluated for device security anomalies.                                                                                          |   **PASS**   |

* **Unit A Assessment Justification**: `IXIT 24-TelData` explicitly provides two distinct security
  anomaly examination workflows (`TelData-System-Status-Heartbeat` and
  `TelData-Crash-Panic-Diagnostics`), fulfilling the requirement for at least one security
  examination.
* **Unit B Assessment Justification**: The collected telemetry data parameters—specifically internal
  FreeRTOS task stack boundaries, internal heap memory allocation blocks, connection failure
  counters, and post-reboot CPU panic backtraces—are directly suited for detecting memory
  exhaustion, DoS attacks, and buffer overflow exploit attempts.

* **Verdict**: **PASS**

---

## Summary Matrix for Test Case 5.10-1-1

| Test Case           | Purpose / Focus                   | Assessment Summary                                                                                                                           | Unit Verdict |
|:--------------------|:----------------------------------|:---------------------------------------------------------------------------------------------------------------------------------------------|:------------:|
| **5.10-1-1 Unit a** | Presence of Security Examination  | `IXIT 24-TelData` provides explicit security examination mechanisms for system status heartbeats and crash panic diagnostics.                |   **PASS**   |
| **5.10-1-1 Unit b** | Suitability for Anomaly Detection | Collected FreeRTOS stack/heap metrics and CPU panic backtraces are highly suited for identifying DoS attacks and memory corruption exploits. |   **PASS**   |

---

## Group Summary

The Ruuvi Gateway complies fully with Recommendation Provision 5.10-1 of `ETSI EN 303 645`. System
telemetry data collected by the device (`IXIT 24-TelData`) is systematically examined outside the
DUT for security anomalies. Outbound system status heartbeats (`TelData-System-Status-Heartbeat`)
transmit FreeRTOS task stack high-water marks and internal heap allocation metrics to
`https://network.ruuvi.com/status` to detect memory leaks and DoS buffer flooding vectors.
Post-reboot crash diagnostics (`TelData-Crash-Panic-Diagnostics`) report CPU registers and panic
backtraces to enable GDB analysis of memory transgression and buffer overflow exploit attempts.

**Group Verdict**: **PASS**
