# Test group 5.6-4A: Software Disabling or Protection of Debug Interfaces

Provision 5.6-4A — Status: **M F (q)**. Related IXIT: `IXIT 15-PhyIntf`.

---

## Condition Evaluation (`ETSI EN 303 645` Annex B)

* **Condition `q` Requirement**: *"Physical debug interfaces are present."*
* **DUT Capabilities Assessment**: As declared in `IXIT 15-PhyIntf`, the DUT incorporates physical
  hardware debug interfaces, specifically the internal nRF52811 Serial Wire Debug (SWD) pad array (
  `PhyIntf-SWD-nRF52`) and a USB-to-UART diagnostic log stream (`PhyIntf-USB`).
* **Condition Result**: Condition `q` evaluates to **TRUE**. Provision 5.6-4A is **Mandatory (M)**.

---

## Test case 5.6-4A-1 (conceptual)

**Purpose**: To conceptually assess whether all debug interfaces declared in `IXIT 15-PhyIntf` are
protected by a best-practice authentication/access control mechanism (`a`), include a software
mechanism to disable the interface (`b`), and ensure that interfaces not required during normal
operation are disabled permanently or by default (`c`–`d`).

---

### Test Units Conceptual Assessment Matrix

#### Test Unit A & B: Protection and Software Disabling Mechanisms

* **Requirement**: For each debug interface in `IXIT 15-PhyIntf`, verify that an appropriate access
  control mechanism or software disabling mechanism is defined.
* **Evaluation**:
  * **`PhyIntf-SWD-nRF52` (Hardware Debug Port):** Protected by an automated software disabling
    mechanism. During the early application boot phase, the nRF52811 co-processor firmware
    reconfigures the SWD interface pins as standard General Purpose Input/Output (GPIO) pins,
    electrically disabling SWD debugging and memory readout during active runtime.
  * **`PhyIntf-USB` (Serial Log Stream):** Protected logically by a one-way, read-only execution
    paradigm. The software stack exposes only a passive diagnostic text stream (`esp_log`). It does
    not host an interactive command shell, login prompt, or execution runtime interface.
* **Verdict**: **PASS**

#### Test Unit C & D: Runtime Disabling Status Verification

* **Requirement**: Verify that debug interfaces that are not indicated as intermittently required
  are disabled permanently or by default according to "Status" in `IXIT 15-PhyIntf`.
* **Evaluation**:

| Debug Interface ID (`IXIT 15-PhyIntf`) | Declared Debug Capability       | Intermittently Required?             | Software Disabling & Protection Mechanism                                                                                                                                                     |      Runtime Status       | Unit Verdict |
|:---------------------------------------|:--------------------------------|:-------------------------------------|:----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|:-------------------------:|:------------:|
| **`PhyIntf-SWD-nRF52`**                | Yes (Factory Provisioning Only) | **No** (Factory build step only)     | **Software Pin Lockout.** Post-boot firmware remapping of SWD pins to standard GPIOs electrically disables DAP hardware access. Protected physically behind a tool-accessible plastic casing. |  **Disabled** (Runtime)   |   **PASS**   |
| **`PhyIntf-USB`**                      | Yes (Passive Log Output Only)   | **Yes** (Continuous passive logging) | **One-Way Read-Only Execution.** Firmware enforces a non-interactive, TX-only logging stream. Incoming RX characters are ignored. Flashing requires hardware pin strapping.                   | **Protected** (Read-Only) |   **PASS**   |

* **Conceptual Assessment Justification**: All physical debug interfaces declared in
  `IXIT 15-PhyIntf` feature explicit software disabling or access control mechanisms. Hardware SWD
  debugging is disabled at boot via GPIO remapping, and the USB diagnostic interface is restricted
  to passive, non-interactive log streaming.

* **Verdict**: **PASS**

---

## Test case 5.6-4A-2 (functional)

**Purpose**: To functionally verify on the default state of the DUT that all declared debug
interfaces are disabled or protected (`a`), and to verify through physical inspection and hardware
probes that no undocumented physical interfaces can be utilized for debugging (`b`).

---

### Test Units Functional Assessment Matrix

#### Test Unit A & B: Functional Debug Interface Protection and Discovery

**Testing Methodology**: The test laboratory executed physical teardown inspections, attached
hardware debug probes (`SEGGER J-Link`, `OpenOCD`), conducted serial terminal interaction tests, and
probed unlabelled PCB test points to verify debug interface lockout.

| Functional Test Scenario            | Target Interface / Test Point       | Action Executed on DUT                                                                                       | Observed Functional DUT Behavior                                                                                                                     | Unit Verdict |
|:------------------------------------|:------------------------------------|:-------------------------------------------------------------------------------------------------------------|:-----------------------------------------------------------------------------------------------------------------------------------------------------|:------------:|
| **Runtime SWD Debug Lockout Check** | `PhyIntf-SWD-nRF52`                 | Connect SEGGER J-Link probe to internal SWD pads while DUT executes production firmware.                     | J-Link software reports `Cannot connect to target / DAP ACK fault`. Software GPIO remapping successfully blocks runtime hardware debugging.          |   **PASS**   |
| **USB Serial Shell Probe**          | `PhyIntf-USB`                       | Open terminal session (`115200 8N1`) via Type-C USB and send arbitrary CLI commands (`help`, `reset`, `\n`). | Terminal receives passive system log traces (`esp_log`). Sent characters yield zero response or execution feedback; no interactive shell is exposed. |   **PASS**   |
| **Undocumented Debug Port Sweep**   | PCB Test Points & Header Footprints | Probe all unlabelled PCB test pads using multimeter, logic analyzer, and boundary scan tools.                | No active JTAG, SWD, or interactive UART shell listeners were discovered across any PCB test points on the mainboard.                                |   **PASS**   |

**Assessment Justification**: Functional hardware testing confirms that the nRF52 SWD debug port is
disabled post-boot via software pin remapping, the USB port operates strictly as a non-interactive
passive log stream, and no undocumented physical debug interfaces exist on the PCB.

**Verdict**: **PASS**

---

## Summary Matrix for Test Case 5.6-4A-1 & 5.6-4A-2

| Test Case              | Purpose / Focus                        | Assessment Summary                                                                                               | Unit Verdict |
|:-----------------------|:---------------------------------------|:-----------------------------------------------------------------------------------------------------------------|:------------:|
| **5.6-4A-1 Units a-b** | Protection & Disabling Mechanism Check | SWD debug pads are locked out via post-boot GPIO remapping; USB serial is restricted to passive TX logging.      |   **PASS**   |
| **5.6-4A-1 Units c-d** | Runtime Disabling Verification         | Hardware SWD debugging is disabled by default in active production firmware images.                              |   **PASS**   |
| **5.6-4A-2 Unit a**    | Functional Debug Lockout Testing       | J-Link probe attachment fails to connect to SWD DAP post-boot; USB serial rejects interactive command execution. |   **PASS**   |
| **5.6-4A-2 Unit b**    | Undocumented Debug Port Discovery      | Hardware probing confirms zero undocumented JTAG, SWD, or UART debug ports exist on the PCB.                     |   **PASS**   |

---

## Group Summary

The Ruuvi Gateway complies fully with Mandatory Provision 5.6-4A of `ETSI EN 303 645`. All physical
debug interfaces declared in `IXIT 15-PhyIntf` are protected by software disabling mechanisms or
strict access controls. The nRF52 co-processor SWD debug port (`PhyIntf-SWD-nRF52`) is protected
inside the tool-accessible plastic casing and disabled at runtime via post-boot GPIO remapping. The
USB serial interface (`PhyIntf-USB`) is restricted to a passive, non-interactive diagnostic log
stream. Functional testing using J-Link probes and terminal tools verifies that runtime hardware
debugging is blocked and that no undocumented physical debug interfaces exist on the device.

**Group Verdict**: **PASS**
