# Test group 5.3-14: Rationale for Absence of Updates and Hardware Replacement Support

Provision 5.3-14 — Status: **R C (3)**. Related IXIT: `IXIT 2-UserInfo`, `IXIT 6-SoftComp`,
`IXIT 7-UpdMech`.

---

## Condition Evaluation (ETSI EN 303 645 Annex B)

* **Condition 3 Requirement**: *"The software components are Placed on the market or deployed
  without software update support (i.e. software components are not updatable)."*
* **DUT Capabilities Assessment**:
  * As declared in `IXIT 6-SoftComp` and verified across Test Groups 5.3-2 through 5.3-10, all
    operational software components (`SoftComp-MainFW`, `SoftComp-nRF52FW`, `SoftComp-WebUI`)
    support over-the-air (OTA) updates via `UpdMech-WebUI` and `UpdMech-Auto`.
  * Low-level bootloader maintenance (`SoftComp-SecondBoot`) is updatable via local serial
    flashing (`UpdMech-USB`).
* **Condition Result**: Condition 3 evaluates to **FALSE**. Provision 5.3-14 is **Not Applicable (
  N/A)**.

---

## Test case 5.3-14-1 (conceptual)

**Purpose**: To conceptually assess whether the publication of the rationale for absence of updates
and hardware replacement support (`IXIT 2-UserInfo`) is understandable for a user with limited
technical knowledge (per Clause D.3).

### Test Unit A: Conceptual Assessment of Non-Updatability Rationale

**Testing Methodology**: The test laboratory evaluated the applicability of Test Case 5.3-14-1
against the update declarations in `IXIT 6-SoftComp` and `IXIT 7-UpdMech`.

| Assessment Parameter             | Declared DUT Property (`IXIT 2-UserInfo` / `IXIT 6-SoftComp`)                                        | Condition & Applicability Assessment                                                                 | Unit Verdict |
|:---------------------------------|:-----------------------------------------------------------------------------------------------------|:-----------------------------------------------------------------------------------------------------|:------------:|
| **Updatability Status**          | All core software components support OTA or local serial updates (`ota_0`/`ota_1` dual-bank layout). | Because the DUT is fully updatable, no "rationale for absence of updates" is required or applicable. |   **N/A**    |
| **Hardware Replacement Support** | `Documentation of Replacement` states N/A due to complete remote OTA software maintainability.       | Hardware replacement schedules due to lack of software updatability are not applicable.              |   **N/A**    |

**Verdict**: **N/A**

---

## Test case 5.3-14-2 (functional)

**Purpose**: To functionally check whether user information regarding the rationale for absence of
software updates and hardware replacement support is published, unrestricted, and complete.

### Test Units Functional Assessment Matrix

| Test Unit  | Focus / Requirement                                           | Observed Status & Justification                                                                           | Unit Verdict |
|:-----------|:--------------------------------------------------------------|:----------------------------------------------------------------------------------------------------------|:------------:|
| **Unit a** | User information access provided as described                 | **N/A.** The DUT is updatable; no resource for absence of updates is required.                            |   **N/A**    |
| **Unit b** | Resource accessible without restrictions                      | **N/A.** Provision condition 3 is not met.                                                                |   **N/A**    |
| **Unit c** | Published rationale contains reason for absence of updates    | **N/A.** The platform actively supports firmware update lifecycle management (`IXIT 7-UpdMech`).          |   **N/A**    |
| **Unit d** | Published hardware replacement plan details period and method | **N/A.** OTA maintainability eliminates the requirement for non-updatable hardware replacement schedules. |   **N/A**    |

**Verdict**: **N/A**

---

## Group Summary

Provision 5.3-14 of `ETSI EN 303 645` is **Not Applicable (N/A)** to the Ruuvi Gateway. The
provision is conditional on Condition 3 (*"software components are not updatable"*). Because the
Ruuvi Gateway provides comprehensive remote over-the-air update mechanisms (`UpdMech-WebUI`,
`UpdMech-Auto`) and local USB recovery paths (`UpdMech-USB`) across all software components,
Condition 3 is false, rendering this test group N/A.

**Group Verdict**: **N/A (Condition 3 Not Satisfied)**
