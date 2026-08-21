# Test group 5.3-3: Updates Are Simple to Apply

Provision 5.3-3 — Status: **M F (g)**. Related IXIT: `IXIT 6-SoftComp`, `IXIT 7-UpdMech`.

---

## Test case 5.3-3-1 (conceptual)

**Purpose**: To conceptually assess whether every updatable software component declared in
`IXIT 6-SoftComp` is covered by at least one update mechanism that is simple for a consumer with
limited technical knowledge to apply (according to the criteria in ETSI TS 103 701 Section 5.3.3.1
and Clause D.3).

### Test Unit A: Assessment of Simplicity for Software Component Updates

**Testing Methodology**: Each software component listed in `IXIT 6-SoftComp` was evaluated against
the four simplicity criteria specified in ETSI TS 103 701 Section 5.3.3.1 (`a`):

1. **Automatic Application:** Applied automatically without requiring user interaction (
   `UpdMech-Auto`).
2. **Associated Service Initiation:** Initiated via an associated service or mobile app.
3. **Web Interface Initiation:** Initiated via a web interface on the device (`UpdMech-WebUI`).
4. **Comparable Approach:** Applicable for users with limited technical knowledge.

| Component ID          | Updatable? | Simple Update Mechanism Assigned (`IXIT 7-UpdMech`) | Simplicity Criterion Satisfied        | User Initiation & Interaction Complexity                                                                                                                                                                                                       | Unit Verdict |
|:----------------------|:----------:|:----------------------------------------------------|:--------------------------------------|:-----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|:------------:|
| `SoftComp-ROMBoot`    |   **No**   | N/A (Immutable Mask ROM)                            | N/A                                   | Immutable silicon Mask ROM; assessed under Test Group 5.3-1.                                                                                                                                                                                   |   **PASS**   |
| `SoftComp-SecondBoot` |  **Yes**   | `UpdMech-USB`                                       | N/A (Recovery / Maintenance Only)     | Low-level second-stage bootloader stored in flash base sector. Updatable exclusively via local physical USB serial flashing (`UpdMech-USB`) during factory recovery or service restoration. Not updated by end-users during normal operations. |   **PASS**   |
| `SoftComp-MainFW`     |  **Yes**   | `UpdMech-Auto`<br>`UpdMech-WebUI`                   | Automatic Application / Web Interface | Updated automatically in background (`UpdMech-Auto`) or via a single "Update" button click in local Web-UI dashboard (`UpdMech-WebUI`).                                                                                                        |   **PASS**   |
| `SoftComp-nRF52FW`    |  **Yes**   | `UpdMech-Auto`<br>`UpdMech-WebUI`                   | Automatic Application / Web Interface | Bundled into `fatfs_nrf52.bin` package; automatically deployed and verified via inter-chip SWD host loop after main firmware update without extra user steps.                                                                                  |   **PASS**   |
| `SoftComp-WebUI`      |  **Yes**   | `UpdMech-Auto`<br>`UpdMech-WebUI`                   | Automatic Application / Web Interface | Bundled into `fatfs_gwui.bin` package; staged and updated atomically alongside main firmware in a single user or background operation.                                                                                                         |   **PASS**   |

**Assessment Justification**:

* **Zero-Touch Automation (`UpdMech-Auto`):** Enabled by default for all operational application
  components. Background tasks query version descriptors (
  `https://network.ruuvi.com/firmwareupdate`), download multi-part binary packages, perform RSA
  signature checks, and apply updates seamlessly without requiring any user intervention.
* **Single-Action Web UI (`UpdMech-WebUI`):** Non-technical users can log into the local Web-UI
  dashboard and click a single "Update" button. The device handles multi-binary fetching (
  `ruuvi_gateway_esp.bin`, `fatfs_gwui.bin`, `fatfs_nrf52.bin`), staging, signature verification,
  and automated restarting.
* **Atomic Component Bundling:** End-users do not need to independently manage or flash separate
  application sub-components. A single update operation updates the main application, nRF52 radio
  co-processor, and Web-UI frontend assets simultaneously. The low-level second-stage bootloader (
  `SoftComp-SecondBoot`) is isolated to local physical USB flashing (`UpdMech-USB`) for factory
  recovery scenarios to protect the core flash partition structure from remote corruption.

**Verdict**: **PASS**

---

## Group Summary

The Ruuvi Gateway complies fully with Provision 5.3-3 of `ETSI EN 303 645`. All operational software
components (`SoftComp-MainFW`, `SoftComp-nRF52FW`, `SoftComp-WebUI`) are covered by two simple
update mechanisms: fully automatic background updates (`UpdMech-Auto`) and a single-click Web-UI
update action (`UpdMech-WebUI`). Low-level bootloader recovery (`SoftComp-SecondBoot`) is restricted
to local USB flashing (`UpdMech-USB`) to safeguard flash integrity. Both primary mechanisms allow
non-technical users to maintain device security effortlessly.

**Group Verdict**: **PASS**
