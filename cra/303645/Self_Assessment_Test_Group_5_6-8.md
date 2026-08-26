# Test group 5.6-8: Hardware-Level Access Control for Memory

Provision 5.6-8 — Status: **R**. Related IXIT: `IXIT 18-AccCtrl`.

---

## Test case 5.6-8-1 (conceptual)

**Purpose**: To conceptually assess whether the hardware-level access control mechanisms documented
in `IXIT 18-AccCtrl` are implemented at the hardware tier (`a`) and effectively control access to
memory spaces (`b`).

---

### Test Units Conceptual Assessment Matrix

#### Test Unit A & B: Hardware Implementation and Memory Access Control Assessment

* **Requirement**: For each hardware-level access control mechanism in `IXIT 18-AccCtrl`, assess
  whether the mechanism is implemented at the hardware level (including software embedded in
  hardware) (`a`), and whether it effectively controls access to protected memory spaces (`b`).

| Access Control ID (`IXIT 18-AccCtrl`)             | Target Memory Space Protected                                                    | Hardware Implementation Level (Unit a)                                                                                       | Memory Access Control Functionality (Unit b)                                                                                                                  | Unit Verdict |
|:--------------------------------------------------|:---------------------------------------------------------------------------------|:-----------------------------------------------------------------------------------------------------------------------------|:--------------------------------------------------------------------------------------------------------------------------------------------------------------|:------------:|
| **`AccCtrl-Hardware-Flash-Partition-Isolation`**  | Application Code (`ota_0`/`ota_1`) vs. Non-Executable Storage (`nvs`, `fatfs_*`) | **Hardware Tier.** ESP32 silicon MMU cache boundaries dividing flash access across Instruction (IBUS) and Data (DBUS) buses. | **Effective Memory Control.** Hardware bus separation prevents execution threads from executing code vectors or jumping to addresses in DBUS data partitions. |   **PASS**   |
| **`AccCtrl-CoProcessor-Bus-Separation`**          | nRF52811 Silicon Registers, Flash, and Internal RAM                              | **Hardware Tier.** Dual-MCU circuit-level isolation connected strictly via UART, with zero shared RAM or DMA channels.       | **Effective Memory Control.** Prevents network-side software compromises on the ESP32 from reading, mutating, or corrupting nRF52 co-processor memory.        |   **PASS**   |
| **`AccCtrl-Hardware-Debug-Interface-Protection`** | Raw System Flash Partition Tables & Live RAM Contexts                            | **Hardware Tier.** Physical JTAG/SWD pad enclosure and ROM bootloader hardware DTR/RTS strapping controls.                   | **Effective Memory Control.** Restricts low-level flash programming and prevents unauthorized runtime memory extraction via debug probes.                     |   **PASS**   |

**Assessment Justification**: The technical declarations in `IXIT 18-AccCtrl` demonstrate robust
hardware-level memory access controls operating at the silicon and circuit layout tiers. Bus-level
MMU mappings (IBUS vs. DBUS) isolate executable code from non-executable storage partitions, while
multi-MCU circuit separation isolates radio co-processor memory from the primary network system.

**Verdict**: **PASS**

---

## Summary Matrix for Test Case 5.6-8-1

| Test Case          | Purpose / Focus                     | Assessment Summary                                                                                                                        | Unit Verdict |
|:-------------------|:------------------------------------|:------------------------------------------------------------------------------------------------------------------------------------------|:------------:|
| **5.6-8-1 Unit a** | Hardware Level Implementation Check | MMU bus mappings, dual-MCU circuit isolation, and ROM bootloader controls operate at the hardware/silicon level.                          |   **PASS**   |
| **5.6-8-1 Unit b** | Memory Access Control Functionality | Hardware mechanisms effectively isolate executable app code from data partitions and protect co-processor memory from network compromise. |   **PASS**   |

---

## Group Summary

The Ruuvi Gateway complies fully with Recommendation Provision 5.6-8 of `ETSI EN 303 645`. The
technical documentation (`IXIT 18-AccCtrl`) demonstrates that hardware-level access control
mechanisms effectively control access to device memory spaces. Silicon-tier MMU cache mappings
enforce strict IBUS/DBUS isolation between executable code slots and non-executable data
partitions (`nvs`, `fatfs`), while physical circuit separation between the ESP32 master and nRF52
co-processor prevents network-side exploits from accessing radio sub-system memory.

**Group Verdict**: **PASS**
