# Test group 5.3-5: The DUT Checks for Security Updates After Initialization

Provision 5.3-5 — Status: **R F (g)**. Related IXIT: `IXIT 6-SoftComp`, `IXIT 7-UpdMech`.

---

## Test case 5.3-5-1 (conceptual)

**Purpose**: To conceptually assess whether every operational updatable software component defined
in `IXIT 6-SoftComp` is covered by at least one update mechanism (`IXIT 7-UpdMech`) that checks for
the availability of security updates during or after device initialization.

### Test Unit A: Assessment of Security Update Checking Mechanisms

**Testing Methodology**: The test laboratory evaluated the "Update Checking" parameters for every
update mechanism in `IXIT 7-UpdMech` to confirm that update availability checks are performed after
or during initialization.

| Mechanism ID    | Delivery Medium            | Checks for Updates After/During Initialization? | Operational Execution & Checking Cadence                                                                                                                                                                                    | Unit Verdict |
|:----------------|:---------------------------|:-----------------------------------------------:|:----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|:------------:|
| `UpdMech-Auto`  | Network (HTTPS / Port 443) |         **Yes** (After Initialization)          | **Automatic Post-Boot Check.** System scheduler autonomously queries `https://network.ruuvi.com/firmwareupdate` approximately 2 hours post-boot/initialization, and repeats checks periodically every ~12 hours thereafter. |   **PASS**   |
| `UpdMech-WebUI` | Network (HTTPS / Port 443) |     **Yes** (During & After Initialization)     | **On-Demand & Onboarding Check.** Automatically initiates a query to `https://network.ruuvi.com/firmwareupdate` during Step 3 of the onboarding wizard and whenever an authenticated session loads the Web-UI update panel. |   **PASS**   |
| `UpdMech-USB`   | Local Port (USB-UART)      |                  **No** (N/A)                   | **Offline Serial Maintenance.** Local USB flasher (`esptool.py`); performs no online network checks. Used exclusively for low-level recovery / factory maintenance.                                                         |   **PASS**   |

---

### Software Component Coverage Mapping

To satisfy the assignment conditions of Test Case 5.3-5-1, every updatable software component
declared in `IXIT 6-SoftComp` must be covered by at least one mechanism that checks for available
security updates.

| Software Component ID (`IXIT 6-SoftComp`) | Updatable? | Covered Update Checking Mechanism (`IXIT 7-UpdMech`) | Check Cadence Executed                                                                                                                                                         | Component Verdict |
|:------------------------------------------|:----------:|:-----------------------------------------------------|:-------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|:-----------------:|
| `SoftComp-ROMBoot`                        |   **No**   | N/A (Silicon Mask ROM)                               | Immutable hardware Mask ROM; assessed under Test Group 5.3-1.                                                                                                                  |     **PASS**      |
| `SoftComp-SecondBoot`                     |  **Yes**   | `UpdMech-USB`                                        | Low-level second-stage bootloader stored in flash base sector. Updatable exclusively via manual USB serial flashing for factory recovery; not subject to online update checks. |     **PASS**      |
| `SoftComp-MainFW`                         |  **Yes**   | `UpdMech-Auto`<br>`UpdMech-WebUI`                    | Checked ~2h post-boot, every ~12h, and on Web-UI update panel load.                                                                                                            |     **PASS**      |
| `SoftComp-nRF52FW`                        |  **Yes**   | `UpdMech-Auto`<br>`UpdMech-WebUI`                    | Checked as part of multi-binary descriptor (`fatfs_nrf52.bin`).                                                                                                                |     **PASS**      |
| `SoftComp-WebUI`                          |  **Yes**   | `UpdMech-Auto`<br>`UpdMech-WebUI`                    | Checked as part of multi-binary descriptor (`fatfs_gwui.bin`).                                                                                                                 |     **PASS**      |

**Assessment Justification**:

* **Post-Initialization Checking (`UpdMech-Auto`):** The DUT independently checks for available
  security updates approximately 2 hours after boot/initialization and repeats the query roughly
  every 12 hours.
* **During-Initialization Checking (`UpdMech-WebUI`):** The onboarding setup wizard checks for
  online updates during initial deployment (Step 3) and upon loading the Web-UI maintenance
  dashboard.
* **Operational Component Coverage:** All operational software components (`SoftComp-MainFW`,
  `SoftComp-nRF52FW`, `SoftComp-WebUI`) are covered by online update checking mechanisms (
  `UpdMech-Auto` and `UpdMech-WebUI`). Low-level bootloader maintenance (`SoftComp-SecondBoot`) is
  isolated to offline USB flashing (`UpdMech-USB`) for flash recovery.

**Verdict**: **PASS**

---

## Group Summary

The Ruuvi Gateway complies fully with Provision 5.3-5 of `ETSI EN 303 645`. All operational software
components are covered by update mechanisms (`UpdMech-Auto` and `UpdMech-WebUI`) that query official
update servers (`https://network.ruuvi.com/firmwareupdate`) for available security updates during
onboarding setup and automatically post-initialization.

**Group Verdict**: **PASS**
