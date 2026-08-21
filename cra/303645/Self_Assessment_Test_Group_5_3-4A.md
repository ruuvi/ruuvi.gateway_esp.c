# Test group 5.3-4A: Ability to Configure Update Mechanisms to Be Automated

Provision 5.3-4A — Status: **R F (g)**. Related IXIT: `IXIT 7-UpdMech`.

---

## Test case 5.3-4A-1 (conceptual)

**Purpose**: To conceptually assess whether at least one update mechanism declared in
`IXIT 7-UpdMech` is designed and documented with configuration options that allow update checking,
downloading, and installation to operate automatically.

### Test Unit A: Assessment of Automatable Update Mechanisms

**Testing Methodology**: Each update mechanism in `IXIT 7-UpdMech` was assessed against its "
Configuration" and "Initiation and Interaction" declarations to determine if it can operate in an
automated mode.

| Mechanism ID    | Delivery Medium              | Configurable to be Automated? |     Default Initialized State      | Configuration Options & Automation Capabilities                                                                                                                                                                                                                                                                                                                                                                                            | Unit Verdict |
|:----------------|:-----------------------------|:-----------------------------:|:----------------------------------:|:-------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|:------------:|
| `UpdMech-Auto`  | Network (HTTPS / Port 443)   |            **Yes**            | **Enabled** (`Auto update` policy) | **Fully Automatable.** Enabled by default upon initial boot. Background task automatically polls version descriptors from `https://network.ruuvi.com/firmwareupdate` (~2h post-boot, then every ~12h), downloads multi-part binary packages, verifies RSA signatures, stages to inactive flash slots, and executes automated reboots during low-activity periods. Supports optional user schedule masks (permitted weekdays/time windows). |   **PASS**   |
| `UpdMech-WebUI` | Network (HTTPS / Port 443)   |            **No**             |          N/A (On-Demand)           | **Manual / User-Initiated.** Requires an explicit user interaction (clicking "Update" in the local Web-UI dashboard) to trigger the download and staging loop.                                                                                                                                                                                                                                                                             |   **PASS**   |
| `UpdMech-USB`   | Local Port (USB-UART Bridge) |            **No**             |         N/A (Maintenance)          | **Manual Physical Engineering Action.** Requires physical cable connection and execution of external flashing utilities (`esptool.py`). Cannot be automated over the air or by device software.                                                                                                                                                                                                                                            |   **PASS**   |

**Assessment Justification**: Conceptual review confirms that `UpdMech-Auto` is explicitly designed
and configurable to be automated. It handles both checking for update availability and executing the
full installation sequence without requiring operator intervention. The assignment condition of Test
Case 5.3-4A-1 is satisfied because at least one update mechanism (`UpdMech-Auto`) is configurable to
be automated (and is enabled by default).

**Verdict**: **PASS**

---

## Test case 5.3-4A-2 (functional)

**Purpose**: To functionally verify that the DUT's runtime configuration permits the automatic
update mechanism (`UpdMech-Auto`) to autonomously check for, download, verify, and apply firmware
updates in runtime environments.

### Test Unit A: Functional Assessment of Automated Update Execution

**Testing Methodology**: The test laboratory evaluated the functional runtime behavior of
`UpdMech-Auto` under default factory settings and custom calendar schedule configurations.

| Operational Phase Tested              | Observed Functional Runtime Behavior                                                                                                                                                   | Verification Result |
|:--------------------------------------|:---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|:-------------------:|
| **1. Availability Check**             | Background scheduler automatically initiates an HTTPS GET request to `https://network.ruuvi.com/firmwareupdate` ~2 hours post-boot without user interaction.                           |    **Verified**     |
| **2. Descriptor Parsing**             | DUT parses the JSON descriptor, compares version strings, and identifies newly available release binaries (`ruuvi_gateway_esp.bin`, `fatfs_gwui.bin`, `fatfs_nrf52.bin`).              |    **Verified**     |
| **3. Automated Fetch & Staging**      | DUT streams binaries over HTTPS into inactive flash slots (`ota_0`/`ota_1`, `fatfs_gwui`/`fatfs_gwui_2`, `fatfs_nrf52`/`fatfs_nrf52_2`) in the background.                             |    **Verified**     |
| **4. Cryptographic Validation**       | `esp_image_verify` validates main application RSA-3072-PSS signatures post-download. Post-reboot, bootloader checks auxiliary filesystem signatures and SWD-injected nRF52 RAM checks. |    **Verified**     |
| **5. Autonomous Reboot & Activation** | DUT executes `gateway_restart()`, activates the new partition set, restores configuration from NVS, and resumes telemetry streaming smoothly.                                          |    **Verified**     |
| **6. Schedule Mask Enforcement**      | When an optional time window constraint is configured (e.g., allowed only between 02:00–04:00), background update checks and reboots defer until within the designated window.         |    **Verified**     |

**Assessment Justification**: Functional testing confirms that `UpdMech-Auto` operates autonomously
in the initialized state. The mechanism successfully executes periodic background update checks,
binary downloads, signature verification, partition staging, and device restarts without requiring
any user prompting or manual intervention.

**Verdict**: **PASS**

---

## Group Summary

The Ruuvi Gateway complies fully with Provision 5.3-4A of `ETSI EN 303 645`. The platform implements
a dedicated automatic update mechanism (`UpdMech-Auto`) that is enabled by default in the factory
initialized state (`Auto update`). Conceptual evaluation and functional runtime testing confirm that
`UpdMech-Auto` autonomously manages update availability checking, downloading, cryptographic
signature verification, partition staging, and system activation.

**Group Verdict**: **PASS**
