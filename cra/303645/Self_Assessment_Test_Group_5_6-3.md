# Test group 5.6-3: Physical Interface Exposure Is Minimized

Provision 5.6-3 — Status: **R**. Related IXIT: `IXIT 15-PhyIntf`.

---

## Test case 5.6-3-1 (conceptual)

**Purpose**: To conceptually assess whether all physical interfaces and air interfaces that do not
require operational exposure are protected by the device casing or similar physical measures (`a`),
and to check whether these interfaces are marked as disabled in `IXIT 15-PhyIntf` (`b`).

---

### Test Units Conceptual Assessment Matrix

#### Test Unit A: Assessment of Protection by Device Casing

* **Requirement**: For each physical interface in `IXIT 15-PhyIntf` that does not require
  operational exposure, assess whether its protection means include enclosure protection by the
  device casing (requiring tools to open).
* **Evaluation**:
  * `PhyIntf-SWD-nRF52` (Internal SWD Debug Pads): Does not require exposure during operation.
    Protection includes full physical enclosure inside the plastic casing shell, requiring physical
    tools (a screwdriver) to access the PCB pads.
  * All exposed physical interfaces (`PhyIntf-USB`, `PhyIntf-Ethernet`, `PhyIntf-Configure-Button`)
    and air interfaces (`PhyIntf-WiFi`, `PhyIntf-BLE-nRF52`) are required for normal device
    operation and telemetry functionality.
* **Verdict**: **PASS**

#### Test Unit B: Verification of Disabled Status for Non-Exposed Interfaces

* **Requirement**: Check whether physical interfaces and air interfaces that do not require
  operational exposure are disabled according to "Status" in `IXIT 15-PhyIntf`.
* **Evaluation**:

| Interface ID (`IXIT 15-PhyIntf`) | Interface Type & Exposure Need                       | Enclosure Protection Means                                                                                                    |                 Declared Runtime Status                 | Unit Verdict |
|:---------------------------------|:-----------------------------------------------------|:------------------------------------------------------------------------------------------------------------------------------|:-------------------------------------------------------:|:------------:|
| **`PhyIntf-SWD-nRF52`**          | Physical Port (Internal SWD Pads) / **Not Required** | Internal PCB pads enclosed inside plastic housing; requires tools (screwdriver) to open.                                      |     **Disabled** (Runtime GPIO remapping post-boot)     |   **PASS**   |
| **`PhyIntf-BLE-ESP32`**          | Air Interface / **Not Required**                     | N/A (Air interface macro).                                                                                                    | **Disabled** (Compiled out of firmware binary entirely) |   **PASS**   |
| **`PhyIntf-USB`**                | Physical Port / **Required** (Power & Logs)          | Exposed Type-C jack.                                                                                                          |      **Enabled** (Read-only TX log output stream)       |   **PASS**   |
| **`PhyIntf-Ethernet`**           | Physical Port / **Required** (WAN/LAN)               | Exposed RJ-45 jack.                                                                                                           |                       **Enabled**                       |   **PASS**   |
| **`PhyIntf-WiFi`**               | Air Interface / **Required** (WLAN/Hotspot)          | Internal antenna.                                                                                                             |         **Enabled** (On-Demand / Configurable)          |   **PASS**   |
| **`PhyIntf-BLE-nRF52`**          | Air Interface / **Required** (Sensor Scanning)       | External antenna connected via SKY66113 FEM (controlled via nRF52811 `LNA_CSD`/`LNA_CRX` pins for Wi-Fi co-existence muting). |         **Enabled** (Passive Rx-Only listener)          |   **PASS**   |
| **`PhyIntf-Configure-Button`**   | Mechanical Button / **Required** (Resets)            | Recessed cutout opening.                                                                                                      |         **Enabled** (Debounced GPIO interrupt)          |   **PASS**   |

* **Unit B Verdict**: **PASS**

---

## Test case 5.6-3-2 (functional)

**Purpose**: To functionally check the completeness of `IXIT 15-PhyIntf` regarding exposed physical
interfaces (`a`), verify that non-exposed physical interfaces on the DUT are protected by the device
casing (`b`), and confirm that enabled/disabled statuses match the technical documentation (`c`).

---

### Test Units Functional Assessment Matrix

| Test Unit / Verification Focus                           | Testing Methodology & Physical Audit Tools                                                       | Observed Functional DUT Behavior                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                        | Unit Verdict |
|:---------------------------------------------------------|:-------------------------------------------------------------------------------------------------|:--------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|:------------:|
| **Unit a: Exposed Interface Documentation Completeness** | Physical teardown audit and visual inspection of the external DUT enclosure.                     | Exposed physical interfaces on the DUT are limited to the RJ-45 Ethernet jack (`PhyIntf-Ethernet`), Type-C USB port (`PhyIntf-USB`), external antenna connectors, and recessed push-button (`PhyIntf-Configure-Button`). All are documented as required in `IXIT 15-PhyIntf`.                                                                                                                                                                                                                                                                                                                           |   **PASS**   |
| **Unit b: Casing Protection Audit**                      | Enclosure disassembly inspection.                                                                | Physical inspection confirms that internal SWD pads (`PhyIntf-SWD-nRF52`) are located inside the plastic casing. Access requires opening the screwed enclosure shell with tools.                                                                                                                                                                                                                                                                                                                                                                                                                        |   **PASS**   |
| **Unit c: Enabled / Disabled Status Verification**       | Protocol sniffing, RF spectrum analysis (2.4 GHz), and hardware debug probe attachment attempts. | 1. **ESP32 BLE (`PhyIntf-BLE-ESP32`):** RF sweeps confirm zero Bluetooth advertisements from the ESP32 macro (**Disabled**).<br>2. **SWD Pads (`PhyIntf-SWD-nRF52`):** Attaching a J-Link SWD probe post-boot fails to establish DAP connection (**Disabled** via GPIO remapping).<br>3. **BLE Hardware Front-End (`PhyIntf-BLE-nRF52`):** Oscilloscope/logic analyzer checks on nRF52811 `LNA_CSD` and `LNA_CRX` lines confirm SKY66113 FEM correctly toggles the BLE receiver off during active ESP32 Wi-Fi transmissions, while enforcing connectionless passive reception during listening windows. |   **PASS**   |

**Assessment Justification**: Physical inspection and functional testing confirm that all exposed
physical interfaces and RF connectors are documented and operationally required. Non-exposed
physical interfaces (`PhyIntf-SWD-nRF52`) are protected inside the sealed plastic casing shell,
hardware co-existence controls (SKY66113 FEM muting) manage RF exposure cleanly, and unnecessary
interfaces (ESP32 BLE, post-boot SWD) are effectively disabled in software.

**Verdict**: **PASS**

---

## Summary Matrix for Test Case 5.6-3-1 & 5.6-3-2

| Test Case          | Purpose / Focus                   | Assessment Summary                                                                                                                                              | Unit Verdict |
|:-------------------|:----------------------------------|:----------------------------------------------------------------------------------------------------------------------------------------------------------------|:------------:|
| **5.6-3-1 Unit a** | Casing Protection Assessment      | Unexposed physical debug pads (`PhyIntf-SWD-nRF52`) are protected inside the tool-accessible plastic casing.                                                    |   **PASS**   |
| **5.6-3-1 Unit b** | Disabled Status Verification      | Physical and air interfaces that do not require operational exposure are marked as disabled in `IXIT 15-PhyIntf`.                                               |   **PASS**   |
| **5.6-3-2 Unit a** | Exposed Interface Documentation   | Physical audit confirms all externally exposed ports and RF connectors are fully cataloged as required in `IXIT 15-PhyIntf`.                                    |   **PASS**   |
| **5.6-3-2 Unit b** | Physical Enclosure Inspection     | Teardown inspection confirms internal debug pads are protected behind the screwed plastic enclosure shell.                                                      |   **PASS**   |
| **5.6-3-2 Unit c** | Functional Interface Status Check | Hardware probes and RF sweeps verify that ESP32 BLE is compiled out, SWD debug pads are locked out post-boot, and SKY66113 FEM co-existence operates correctly. |   **PASS**   |

---

## Group Summary

The Ruuvi Gateway complies fully with Recommendation Provision 5.6-3 of `ETSI EN 303 645`. All
externally exposed physical ports (`PhyIntf-Ethernet`, `PhyIntf-USB`, `PhyIntf-Configure-Button`)
and air interfaces/connectors (`PhyIntf-WiFi`, `PhyIntf-BLE-nRF52`) are operationally required and
documented in `IXIT 15-PhyIntf`. The BLE sub-system employs an external antenna fed via a Skyworks
SKY66113 Front-End Module controlled by nRF52811 `LNA_CSD`/`LNA_CRX` pins, enabling dynamic hardware
receiver muting during ESP32 Wi-Fi transmissions while maintaining a passive connectionless scanning
architecture. Physical interfaces that do not require operational exposure (`PhyIntf-SWD-nRF52`) are
protected inside the plastic casing shell (requiring tools to open) and disabled at runtime via
post-boot GPIO remapping. Unnecessary air interfaces (`PhyIntf-BLE-ESP32`) are completely disabled
in software.

**Group Verdict**: **PASS**
