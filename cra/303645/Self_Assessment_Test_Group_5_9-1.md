# Test group 5.9-1: Resilience to Network Connectivity and Power Outages

Provision 5.9-1 — Status: **R**. Related IXIT: `IXIT 23-ResMech`.

---

## Test case 5.9-1-1 (conceptual)

**Purpose**: To conceptually assess whether the combination of resilience mechanisms in
`IXIT 23-ResMech` is appropriate to protect against network connectivity and power outages (`a`),
and whether each individual mechanism achieves its claimed security guarantees (`b`).

---

### Test Units Conceptual Assessment Matrix

#### Test Unit A & B: Assessment of Resilience Mechanisms and Security Guarantees

| Resilience Mechanism ID (`IXIT 23-ResMech`)       | Outage Category    | Primary Technical Implementation                                                                                                                                  | Claimed Security Guarantee & Conceptual Suitability                                                                                                                      | Unit Verdict |
|:--------------------------------------------------|:-------------------|:------------------------------------------------------------------------------------------------------------------------------------------------------------------|:-------------------------------------------------------------------------------------------------------------------------------------------------------------------------|:------------:|
| **`ResMech-Power-NVS-Wear-Leveling`**             | **Power Outage**   | Append-only transaction log distributed across flash pages in `nvs` and `gw_cfg_def` partitions.                                                                  | **Data Integrity.** Prevents partial-write sector corruption during hard power cuts; safely rolls back to the last complete parameter entry upon boot.                   |   **PASS**   |
| **`ResMech-Firmware-Redundancy-And-Rollback`**    | **Power Outage**   | Dual-slot partition layout (`ota_0`/`ota_1`, `fatfs_gwui`/`_2`, `fatfs_nrf52`/`_2`) with automated ESP-IDF rollback.                                              | **System Availability.** Protects against unbootable states or flash corruption during mid-update power cuts by falling back to verified backup slots.                   |   **PASS**   |
| **`ResMech-Net-Link-Layer-Auto-Recovery`**        | **Network Outage** | Infinite Wi-Fi re-association retry loops and Ethernet PHY hot-plug cable re-engagement detection.                                                                | **System Availability.** Restores L2/L3 network connectivity automatically after local AP dropouts or cabling disruptions without requiring a manual reboot.             |   **PASS**   |
| **`ResMech-Net-Telemetry-Protocol-Reconnection`** | **Network Outage** | MQTT auto-reconnect (10s interval) and HTTP non-blocking async timer switching to a fixed 67s retry delay (`ADV_POST_DELAY_BEFORE_RETRYING_POST_AFTER_ERROR_MS`). | **Availability & Stability.** Prevents task deadlocks or memory exhaustion during cloud backend outages; avoids self-inflicted reconnection storms upon server recovery. |   **PASS**   |
| **`ResMech-Net-Watchdog-Recovery`**               | **Network Outage** | 1-hour last-success epoch watchdog (`RUUVI_NETWORK_WATCHDOG_TIMEOUT_SECONDS`) with distributed reboot timing.                                                     | **System Availability.** Fail-safe recovery from rare task deadlocks or socket leaks, distributing fleet reboots smoothly across rolling 1-hour windows.                 |   **PASS**   |

**Assessment Justification**: The technical mechanisms declared in `IXIT 23-ResMech` comprehensively
address both power and network disruption vectors. Power outages are mitigated via append-only NVS
transaction logging and dual-slot firmware rollback, while network outages are handled by automated
L2 auto-recovery, non-blocking HTTP 67-second retry backoff, and a rolling 1-hour last-success
network watchdog.

**Verdict**: **PASS**

---

## Test case 5.9-1-2 (functional)

**Purpose**: To functionally verify on the DUT that resilience mechanisms operate as documented in
`IXIT 23-ResMech` during active network connectivity interruptions (`a`) and abrupt power supply
terminations (`b`).

---

### Test Units Functional Assessment Matrix

#### Test Unit A & B: Functional Outage Testing and System Recovery Inspection

**Testing Methodology**: The test laboratory executed active network link disruptions (unplugging
Ethernet cables, disabling Wi-Fi APs, blocking Ruuvi Cloud WAN routes) and abrupt power
terminations (pulling Type-C USB power during active REST API writes and firmware streaming) while
monitoring system logs over `LogIntf-USB-UART-Log-Stream`.

| Functional Test Scenario                        | Outage Type & Action Executed on DUT                                                                                          | Observed Functional DUT Behavior & Recovery                                                                                                                                                                                                                        | Unit Verdict |
|:------------------------------------------------|:------------------------------------------------------------------------------------------------------------------------------|:-------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|:------------:|
| **Network Link Interruption Test**              | **Network Outage (Unit a):** Disconnect Ethernet cable during active HTTP telemetry streaming. Re-plug cable after 5 minutes. | Ethernet driver detects PHY loss and pauses tasks. Upon cable re-insertion, PHY re-links, DHCP lease renews, and HTTP telemetry resumes uploading to `https://network.ruuvi.com/record`.                                                                           |   **PASS**   |
| **Cloud Backend Outage & Recovery Test**        | **Network Outage (Unit a):** Block WAN routing to `network.ruuvi.com` for 30 minutes.                                         | HTTP client catches non-2xx status, drops in-flight batch, and switches to 67s retry timer (`adv1_post_timer_relaunch_with_increased_period`). Zero memory leaks or task lockups occur. When WAN opens, posts settle back to 60s cadence (`X-Ruuvi-Gateway-Rate`). |   **PASS**   |
| **Abrupt Power Cut During Configuration Write** | **Power Outage (Unit b):** Pull USB power while issuing `POST /ruuvi.json` settings updates. Restore power.                   | DUT boots cleanly. Append-only NVS wear-leveling detects incomplete transaction tag and restores the previous valid `ruuvi.json` block. Flash memory shows zero corruption.                                                                                        |   **PASS**   |
| **Abrupt Power Cut During OTA Update**          | **Power Outage (Unit b):** Pull USB power during secondary partition OTA binary writing. Restore power.                       | Early boot verification (`esp_image_verify`) detects corrupted image signature in secondary slot, invalidates slot, and executes automated rollback to primary slot. System boots successfully.                                                                    |   **PASS**   |

**Assessment Justification**: Functional outage testing confirms that the DUT recovers cleanly from
both power and network disruptions. Hard power-cuts do not corrupt NVS configuration flash or brick
the device during updates, and network disconnects or cloud server outages are handled gracefully
without task deadlocks, heap exhaustion, or reconnection storms.

**Verdict**: **PASS**

---

## Summary Matrix for Test Case 5.9-1-1 & 5.9-1-2

| Test Case          | Purpose / Focus                   | Assessment Summary                                                                                                              | Unit Verdict |
|:-------------------|:----------------------------------|:--------------------------------------------------------------------------------------------------------------------------------|:------------:|
| **5.9-1-1 Unit a** | Conceptual Protection Assessment  | Resilience mechanisms in `IXIT 23-ResMech` appropriately protect against network and power outage vectors.                      |   **PASS**   |
| **5.9-1-1 Unit b** | Security Guarantees Evaluation    | NVS wear-leveling guarantees Data Integrity; dual-slot rollback, L2 auto-recovery, and protocol retries guarantee Availability. |   **PASS**   |
| **5.9-1-2 Unit a** | Functional Network Outage Testing | Functional link drops and WAN blocks confirm clean protocol recovery, 67s retry backoff, and smooth telemetry resumption.       |   **PASS**   |
| **5.9-1-2 Unit b** | Functional Power Outage Testing   | Power cut testing during writes and OTA updates confirms clean NVS rollback, zero flash corruption, and dual-slot self-healing. |   **PASS**   |

---

## Group Summary

The Ruuvi Gateway complies fully with Recommendation Provision 5.9-1 of `ETSI EN 303 645`. The
technical declarations in `IXIT 23-ResMech` demonstrate robust resilience mechanisms against power
disruptions and network outages. Hard power cuts are mitigated via append-only NVS transaction
logging and dual-slot application/data partition rollback. Network link drops and cloud server
outages are handled via automated Wi-Fi/Ethernet auto-recovery, non-blocking HTTP 67-second retry
backoff, and a rolling 1-hour network watchdog. Functional testing verifies that the device recovers
cleanly from power and network failures without data corruption or lockups.

**Group Verdict**: **PASS**
