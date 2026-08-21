# Test group 5.9-2: System Recovery and Local Functionality Maintenance Following Outages

Provision 5.9-2 — Status: **R**. Related IXIT: `IXIT 23-ResMech`.

---

## Test case 5.9-2-1 (conceptual)

**Purpose**: To conceptually assess the resilience mechanisms in `IXIT 23-ResMech` concerning
protection against network and power outages (`a`), ensure that the DUT remains operating and
locally functional during a loss of network connectivity (`b`), and confirm that the DUT resumes
connectivity and functionality in the same or improved state after a power loss (`c`).

---

### Test Units Conceptual Assessment Matrix

#### Test Unit A, B & C: Conceptual Assessment of Local Functionality & Post-Outage Recovery

| Resilience Mechanism ID (`IXIT 23-ResMech`)                                                     | Outage Category (`Type`) | Technical Recovery Mechanism                                                                                                                         | Assessment of Local Functionality & Post-Outage State Restoration                                                                                                                                                                                                                                                                                    | Unit Verdict |
|:------------------------------------------------------------------------------------------------|:-------------------------|:-----------------------------------------------------------------------------------------------------------------------------------------------------|:-----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|:------------:|
| **`ResMech-Net-Link-Layer-Auto-Recovery`**<br>**`ResMech-Net-Telemetry-Protocol-Reconnection`** | **Network connectivity** | Infinite Wi-Fi re-association, Ethernet cable hot-plug detection, non-blocking HTTP async retry timers (67s backoff), and MQTT auto-reconnect (10s). | **Unit b (Local Functionality Maintained).** During total network disconnection, the nRF52 co-processor continues passive BLE radio scanning, and the ESP32 buffers advertisement metrics into local RAM queues without task deadlocks or heap exhaustion. Local diagnostic serial streams (`LogIntf-USB-UART-Log-Stream`) remain fully operational. |   **PASS**   |
| **`ResMech-Power-NVS-Wear-Leveling`**<br>**`ResMech-Firmware-Redundancy-And-Rollback`**         | **Power outage**         | Append-only NVS transaction logs across `nvs` and `gw_cfg_def` partitions; dual-slot application/data partition fallback.                            | **Unit c (Clean State Restoration).** After power loss, NVS append-only logs restore exact pre-outage user configurations (`ruuvi.json`, Wi-Fi/Ethernet station credentials, and custom SSL certificates). Dual-slot verification ensures the DUT boots into a cryptographically signed operational image in the same state as before power loss.    |   **PASS**   |

**Assessment Justification**: The resilience declarations in `IXIT 23-ResMech` fulfill all
conceptual criteria under Provision 5.9-2:

1. **Local Operational Continuity (Unit b):** Disruption of WAN/LAN connectivity does not stall
   internal processing loops; BLE radio scanning, local memory queuing, and diagnostic logging
   continue uninterrupted.
2. **Clean Power Restoration (Unit c):** Abrupt power drops do not corrupt persistent settings or
   lock up boot loops. Upon cold boot, append-only NVS transaction logs restore the exact pre-outage
   operational state, and network auto-recovery loops restore cloud data streaming without manual
   intervention.

**Verdict**: **PASS**

---

## Test case 5.9-2-2 (functional)

**Purpose**: To functionally verify on the DUT that local functionality remains operational during
network connectivity outages (`a`), and that the DUT automatically resumes connectivity and
functionality in the same or improved state following power supply restoration (`b`).

---

### Test Units Functional Assessment Matrix

#### Test Unit A & B: Functional Local Functionality and Power Restoration Inspection

**Testing Methodology**: The test laboratory executed active network link disruptions (disconnecting
Ethernet cables and suppressing Wi-Fi APs) while triggering local BLE sensor tag broadcasts, and
performed hard power supply cuts during active telemetry operations, verifying post-recovery data
visibility on the Ruuvi Cloud dashboard (`https://network.ruuvi.com/record`).

| Functional Test Scenario                             | Outage Type & Action Executed on DUT                                                                                                                       | Observed Local Functionality & Telemetry Behavior                                                                                                                                 | Post-Outage Restoration Verification                                                                                                                                                          | Unit Verdict |
|:-----------------------------------------------------|:-----------------------------------------------------------------------------------------------------------------------------------------------------------|:----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|:----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|:------------:|
| **Network Interruption & Local Functionality Check** | **Network Outage (Unit a):** Disconnect network cable while actively broadcasting BLE tag environmental events (RuuviTag temperature/movement).            | The DUT remains active. BLE radio scanning continues uninterrupted, buffering sensor payloads in RAM queues. Diagnostic USB-UART logs confirm active scanning loop execution.     | Upon re-connecting the network cable, the DUT re-links, renews its DHCP lease, flushes buffered/current telemetry to Ruuvi Cloud, and sensor events immediately appear on the user dashboard. |   **PASS**   |
| **Power Interruption & State Restoration Check**     | **Power Outage (Unit b):** Trigger local BLE sensor events, perform hard USB power cut during operation, restore power, and trigger new BLE sensor events. | Abrupt power loss cuts execution. Upon power re-application, the DUT cold-boots cleanly, reloads saved `ruuvi.json` parameters from NVS, and re-establishes network connectivity. | Newly triggered BLE sensor tag environmental events are captured, processed, and successfully transmitted to Ruuvi Cloud, confirming restoration to the exact pre-outage operational state.   |   **PASS**   |

**Assessment Justification**: Functional testing confirms that during network outages, the DUT
maintains local operational functionality (continuous BLE scanning and memory queuing) without
locking up. Following power restoration, the DUT cold-boots cleanly, restores exact saved NVS
parameters, re-establishes network connections, and resumes telemetry streaming to cloud dashboards
in the same operational state as before the outage.

**Verdict**: **PASS**

---

## Summary Matrix for Test Case 5.9-2-1 & 5.9-2-2

| Test Case          | Purpose / Focus                   | Assessment Summary                                                                                                                 | Unit Verdict |
|:-------------------|:----------------------------------|:-----------------------------------------------------------------------------------------------------------------------------------|:------------:|
| **5.9-2-1 Unit a** | Conceptual Outage Assessment      | Resilience mechanisms in `IXIT 23-ResMech` appropriately protect against network and power disruptions.                            |   **PASS**   |
| **5.9-2-1 Unit b** | Local Functionality Assessment    | During network outages, BLE scanning, RAM buffering, and serial logging continue operating without deadlocks.                      |   **PASS**   |
| **5.9-2-1 Unit c** | Clean Power Recovery Assessment   | NVS append-only logs and dual-slot fallback ensure the DUT cold-boots into the exact pre-outage state.                             |   **PASS**   |
| **5.9-2-2 Unit a** | Functional Network Outage Testing | Network disconnect checks confirm local BLE scanning continues and telemetry resumes streaming to cloud upon re-link.              |   **PASS**   |
| **5.9-2-2 Unit b** | Functional Power Recovery Testing | Power cut testing confirms clean cold-boot recovery, exact NVS settings restoration, and immediate post-power telemetry streaming. |   **PASS**   |

---

## Group Summary

The Ruuvi Gateway complies fully with Recommendation Provision 5.9-2 of `ETSI EN 303 645`. The
technical declarations in `IXIT 23-ResMech` and functional testing demonstrate that the DUT remains
operating and locally functional during network connectivity outages (continuous BLE radio scanning,
RAM payload buffering, and local serial logging). Following abrupt power loss, append-only NVS
transaction wear-leveling and dual-slot partition fallback ensure the device recovers cleanly to its
exact pre-outage configuration state, automatically re-establishing network connectivity and
resuming telemetry streaming to cloud dashboards without requiring manual intervention.

**Group Verdict**: **PASS**
