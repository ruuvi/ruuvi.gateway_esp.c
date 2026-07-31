# Test group 5.6-4B: Physical Protection of Physical Debug Interfaces

Provision 5.6-4B — Status: **M F (q)**. Related IXIT: `IXIT 15-PhyIntf`.

---

## Condition Evaluation (`ETSI EN 303 645` Annex B)

* **Condition `q` Requirement**: *"Physical debug interfaces are present."*
* **DUT Capabilities Assessment**: As declared in `IXIT 15-PhyIntf`, the DUT incorporates physical
  hardware debug interfaces, specifically the internal nRF52811 Serial Wire Debug (SWD) pad array (
  `PhyIntf-SWD-nRF52`) and the external Type-C USB virtual serial port (`PhyIntf-USB`).
* **Condition Result**: Condition q evaluates to **TRUE**. Provision 5.6-4B is **Mandatory (M)**.

---

## Test case 5.6-4B-1 (conceptual)

**Purpose**: To conceptually assess whether every physical debug interface that is a physical port
declared in `IXIT 15-PhyIntf` is physically protected by the DUT (`a`).

---

### Test Units Conceptual Assessment Matrix

#### Test Unit A: Assessment of Physical Protection for Debug Ports

* **Requirement**: For each physical interface in `IXIT 15-PhyIntf` indicated as a debug interface (
  `Debug Interface: Yes`) and categorized as a physical port (`Type: Physical port`), assess whether
  the protection mechanisms include physical protection provided by the DUT enclosure or hardware
  architecture.
* **Evaluation**:
  * **`PhyIntf-SWD-nRF52` (Internal Hardware SWD Debug Port):** The SWD connector pads are located
    exclusively on internal copper layers of the mainboard PCB, completely enclosed within the
    device's plastic casing shell. Physical access to the pads requires using tools (a screwdriver)
    to open the enclosure, providing full casing-level physical protection.
  * **`PhyIntf-USB` (External Type-C USB Port):** Physically exposed externally to provide power
    input to the device. Physical and structural hardware protections apply: the port is connected
    to a dedicated virtual USB-to-UART bridge that transmits only passive, read-only system log
    streams (`TX-only`). Hardware-level boot-strap pin constraints prevent runtime memory access or
    unauthorized code execution over this port.

| Debug Interface ID (`IXIT 15-PhyIntf`) | Interface Type                | Declared Debug Capability       | Physical Protection Mechanism                                                                                                                              | Unit Verdict |
|:---------------------------------------|:------------------------------|:--------------------------------|:-----------------------------------------------------------------------------------------------------------------------------------------------------------|:------------:|
| **`PhyIntf-SWD-nRF52`**                | Physical Port (Internal Pads) | Yes (Factory Provisioning Only) | **Enclosure Deterrence.** Fully enclosed inside plastic casing; requires tools (screwdriver) to access physical PCB pads.                                  |   **PASS**   |
| **`PhyIntf-USB`**                      | Physical Port                 | Yes (Passive Log Output Only)   | **Hardware-Restricted Functionality.** External port restricted to one-way passive TX log output. Hardware boot-strap strapping prevents runtime flashing. |   **PASS**   |

* **Conceptual Assessment Justification**: All physical debug interfaces declared in
  `IXIT 15-PhyIntf` are physically protected by the DUT through enclosure tool-access requirements
  or hardware-tier physical interface restrictions.

* **Verdict**: **PASS**

---

## Test case 5.6-4B-2 (functional)

**Purpose**: To functionally check on the DUT that all physical debug interfaces that are physical
ports are physically protected by the device casing or hardware architecture (`a`).

---

### Test Units Functional Assessment Matrix

#### Test Unit A: Functional Verification of Physical Debug Protection

**Testing Methodology**: The test laboratory performed physical teardown audits, visual enclosure
inspections, and hardware interface checks on the DUT to verify the physical protection of all debug
ports.

| Functional Test Scenario       | Target Physical Debug Port | Action Executed on DUT                                                        | Observed Functional DUT Behavior                                                                                                                             | Unit Verdict |
|:-------------------------------|:---------------------------|:------------------------------------------------------------------------------|:-------------------------------------------------------------------------------------------------------------------------------------------------------------|:------------:|
| **Enclosure Protection Audit** | `PhyIntf-SWD-nRF52`        | Inspect the external cased DUT for exposed SWD debug pins or header openings. | External enclosure exposes no SWD pads or debug headers. Pads are accessible strictly after opening the screwed plastic casing shell using tools.            |   **PASS**   |
| **Exposed Port Debug Probe**   | `PhyIntf-USB`              | Inspect external Type-C USB port and attempt interactive debugging session.   | Port serves as physical power input and streams passive text logs. Terminal checks confirm zero interactive debugging shell or execution console is exposed. |   **PASS**   |

**Assessment Justification**: Functional physical inspection confirms that internal hardware debug
pads (`PhyIntf-SWD-nRF52`) are physically protected inside the tool-accessible plastic casing, and
the externally exposed USB port (`PhyIntf-USB`) exposes no physical or logical debugging controls.

**Verdict**: **PASS**

---

## Summary Matrix for Test Case 5.6-4B-1 & 5.6-4B-2

| Test Case           | Purpose / Focus                           | Assessment Summary                                                                                            | Unit Verdict |
|:--------------------|:------------------------------------------|:--------------------------------------------------------------------------------------------------------------|:------------:|
| **5.6-4B-1 Unit a** | Conceptual Physical Protection Check      | Internal SWD pads are protected by the tool-accessible casing; exposed USB is hardware-restricted to TX logs. |   **PASS**   |
| **5.6-4B-2 Unit a** | Functional Physical Protection Inspection | Physical audit verifies that internal debug pads are fully enclosed behind the plastic housing shell.         |   **PASS**   |

---

## Group Summary

The Ruuvi Gateway complies fully with Mandatory Provision 5.6-4B of `ETSI EN 303 645`. All physical
debug interfaces declared in `IXIT 15-PhyIntf` are physically protected by the DUT. The nRF52
co-processor SWD debug port (`PhyIntf-SWD-nRF52`) is physically enclosed inside the plastic device
housing, requiring tools (a screwdriver) to open. The externally exposed Type-C USB port (
`PhyIntf-USB`) is structurally restricted at the hardware level to passive, one-way system log
streaming and power delivery, preventing interactive hardware debugging.

**Group Verdict**: **PASS**
