# Test group 5.3-1: All Non-Immutable Software Components are Securely Updateable

Provision 5.3-1 — Status: **R F (f)**. Related IXIT: `IXIT 6-SoftComp`, `IXIT 7-UpdMech`.

---

## Test case 5.3-1-1 (conceptual)

**Purpose**: To conceptually assess whether every software component defined in `IXIT 6-SoftComp` is
either securely updateable through a defined mechanism or immutable due to legitimate security or
practicability reasons.

### Test Unit A: Assessment of Components Without Update Mechanisms

| Component ID       | Stated Update Mechanism | Physical / Logical Storage Vector | Justification for Absence of Update Mechanism                                                                                                                                                     | Case Verdict |
|:-------------------|:------------------------|:----------------------------------|:--------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|:------------:|
| `SoftComp-ROMBoot` | None (Empty)            | Silicon Mask ROM Block (ESP32)    | Hardware-integrated Mask ROM burned during chip fabrication. Structurally unalterable; immutable due to fundamental hardware practicability and foundational chain-of-trust security constraints. |   **PASS**   |

**Assessment Justification**: `SoftComp-ROMBoot` is the sole software component lacking an update
mechanism. Because it is permanently etched into silicon Mask ROM, updating it is physically
impossible. Its immutability serves as the hardware-enforced root of execution for subsequent boot
stages.

---

### Test Unit B: Conceptual Assessment of Referenced Update Mechanisms

**Testing Methodology**: Evaluated against the security requirements of Test Case 5.3-2-1 for all
update mechanisms referenced across updatable components in `IXIT 6-SoftComp`.

| Component ID          | Referenced Update Mechanisms (`IXIT 7-UpdMech`)    | Security & Anti-Misuse Safeguards                                                                                                                                     | Unit Verdict |
|:----------------------|:---------------------------------------------------|:----------------------------------------------------------------------------------------------------------------------------------------------------------------------|:------------:|
| `SoftComp-SecondBoot` | `UpdMech-USB`                                      | Requires physical proximity access over USB-UART bridge. Post-flash execution is verified at boot time.                                                               |   **PASS**   |
| `SoftComp-MainFW`     | `UpdMech-WebUI`<br>`UpdMech-Auto`<br>`UpdMech-USB` | RSA-3072-PSS / SHA-256 signature verification over `ota_0`/`ota_1` slots. Dual-bank layout ensures automated rollback if image verification fails.                    |   **PASS**   |
| `SoftComp-nRF52FW`    | `UpdMech-WebUI`<br>`UpdMech-Auto`<br>`UpdMech-USB` | Bundled in main OTA payload (`fatfs_nrf52`/`fatfs_nrf52_2`). Boot-time SWD RAM stub injects SHA-256 verification across nRF52 flash; auto-restores image on mismatch. |   **PASS**   |
| `SoftComp-WebUI`      | `UpdMech-WebUI`<br>`UpdMech-Auto`<br>`UpdMech-USB` | Staged in read-only filesystems (`fatfs_gwui`/`fatfs_gwui_2`). Verified via RSA-3072-PSS signature blocks prior to active partition handoff.                          |   **PASS**   |

**Assessment Justification**: All updatable components reference valid update mechanisms defined in
`IXIT 7-UpdMech`. Each network-based mechanism (`UpdMech-WebUI`, `UpdMech-Auto`) enforces
application-layer RSA-3072-PSS signature verification and dual-slot A/B flash partition redundancy,
preventing unauthorized code execution, Man-in-the-Middle (MitM) payload manipulation, or bricking.
The local interface (`UpdMech-USB`) requires physical access to the device.

**Verdict**: **PASS**

---

## Test case 5.3-1-2 (functional)

**Purpose**: To functionally assess the effectiveness of the update mechanisms in preventing misuse
during update operations.

### Test Unit A: Functional Assessment of Update Mechanisms

**Testing Methodology**: Evaluated by applying the functional test criteria specified in Test Case
5.3-2-2 across every update mechanism referenced in `IXIT 6-SoftComp`.

| Mechanism Tested | Operational Execution Path             | Anti-Misuse Functional Behavior Verified                                                                                                                                 | Case Verdict |
|:-----------------|:---------------------------------------|:-------------------------------------------------------------------------------------------------------------------------------------------------------------------------|:------------:|
| `UpdMech-WebUI`  | User-initiated HTTPS update via Web-UI | Rejects unsigned or corrupted binary images. Inactive slot staging ensures active application continuity until RSA verification succeeds.                                |   **PASS**   |
| `UpdMech-Auto`   | Automated background update task       | Queries `https://network.ruuvi.com/firmwareupdate` securely over TLS. Invalid payload signatures trigger immediate rejection and rollback without disrupting operations. |   **PASS**   |
| `UpdMech-USB`    | Physical USB-UART serial flashing      | `esptool.py` flasher conducts MD5 transport checks. The main application layer executes RSA signature validation at boot before committing configuration slots.          |   **PASS**   |

**Assessment Justification**: Functional checks confirm that all non-immutable components (
`SoftComp-SecondBoot`, `SoftComp-MainFW`, `SoftComp-nRF52FW`, `SoftComp-WebUI`) can be updated using
their assigned mechanisms, and that invalid, tampered, or improperly signed binaries cannot be
executed under any tested condition.

**Verdict**: **PASS**

---

## Group Summary

The Ruuvi Gateway complies fully with Provision 5.3-1 of `ETSI EN 303 645`. The only non-updatable
component (`SoftComp-ROMBoot`) is immutable due to silicon Mask ROM constraints. All other software
components are securely updateable via the mechanisms declared in `IXIT 7-UpdMech`. Cryptographic
signature validation (RSA-3072-PSS), SWD-driven co-processor verification, and dual-slot partition
redundancy ensure these mechanisms cannot be misused by an attacker.

**Group Verdict**: **PASS**
