# Test group 5.3-4B: Automatic Update Mechanisms Are Enabled During Initialization After User Consent

Provision 5.3-4B — Status: **R C (16) (h)**. Related IXIT: `IXIT 7-UpdMech`, `IXIT 26-UserDec`.

---

## Test case 5.3-4B-1 (conceptual)

**Purpose**: To conceptually assess whether the user's consent to enable automatic update mechanisms
is explicitly queried during device initialization (`IXIT 26-UserDec`), and whether granting or
denying consent appropriately enables or disables the automatic update service (`IXIT 7-UpdMech`).

### Test Units A, B & C: Assessment of Initialization Consent Querying & Configuration Handling

**Testing Methodology**: The test laboratory identified all automatic update mechanisms in
`IXIT 7-UpdMech` (Unit A), cross-referenced the onboarding wizard decision points in
`IXIT 26-UserDec` (Unit B), and evaluated the configuration handoff logic when consent is granted or
denied (Unit C).

| Identified Automatic Update Mechanism (`IXIT 7-UpdMech`) | Queried During Onboarding Initialization? (`IXIT 26-UserDec`) | Consent Query Mechanism & Options Presented                                                                                                                                        | System Behavior When Consent Granted (`Auto update`)                                                                                                                                | System Behavior When Consent Denied (`Manual updates only`)                                                                                                                          | Unit Verdict |
|:---------------------------------------------------------|:-------------------------------------------------------------:|:-----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|:------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|:-------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|:------------:|
| `UpdMech-Auto`                                           |            **Yes** (`UserDec-5-Automatic-Updates`)            | Step 5 of the onboarding setup wizard explicitly prompts the user to select an update policy: `Auto update` (Default), `Auto update (for beta testers)`, or `Manual updates only`. | Background update scheduler task (`UpdMech-Auto`) is initialized and active. Periodic 12-hour background version polling to `https://network.ruuvi.com/firmwareupdate` is executed. | Background automatic update checking and automatic image installation routines are completely disabled. The device performs updates only upon manual user request (`UpdMech-WebUI`). |   **PASS**   |

**Assessment Justification**:

* **Unit A:** `UpdMech-Auto` is identified as an automatic update mechanism because it executes
  update checking, binary downloads, signature validation, and partition staging without requiring
  user interaction.
* **Unit B:** Cross-referencing `UserDec-5-Automatic-Updates` in `IXIT 26-UserDec` confirms that
  user consent and policy selection are explicitly queried during Step 5 of the initial setup wizard
  before device provisioning completes.
* **Unit C:** When the user confirms consent by retaining or selecting `Auto update` (or
  `Auto update for beta testers`), the DUT enables `UpdMech-Auto`. When the user denies consent by
  selecting `Manual updates only`, `UpdMech-Auto` is disabled by the firmware runtime.

**Verdict**: **PASS**

---

## Test case 5.3-4B-2 (functional)

**Purpose**: To functionally verify during the device initialization process that user consent to
enable automatic secure updates is queried, and that granting or denying consent correctly sets the
operational state of the automatic update background mechanism (`UpdMech-Auto`).

### Test Units A, B & C: Functional Onboarding Consent Verification

**Testing Methodology**: The test laboratory executed full onboarding initialization workflows on a
freshly reset DUT, testing both consent acceptance (`Auto update`) and consent denial (
`Manual updates only`) paths while simulating remote update availability.

| Functional Test Scenario                 | User Interaction During Initialization                                                 | Observed Post-Initialization DUT Behavior                                                                                                                                                                                                               | Case Verdict |
|:-----------------------------------------|:---------------------------------------------------------------------------------------|:--------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|:------------:|
| **Unit A: Consent Prompt Verification**  | Operator boots a factory-default DUT and progresses through the setup wizard.          | Step 5 ("Automatic Updates") explicitly renders policy selection controls (`Auto update`, `Auto update for beta testers`, `Manual updates only`) before deployment completes.                                                                           |   **PASS**   |
| **Unit B: Granted Consent Verification** | Operator selects `Auto update` (granting consent) and completes initialization.        | The DUT initializes background timer tasks. Upon simulating a new release descriptor on `https://network.ruuvi.com/firmwareupdate`, the DUT automatically downloads, verifies RSA signatures, stages, and applies the update without user intervention. |   **PASS**   |
| **Unit C: Denied Consent Verification**  | Operator selects `Manual updates only` (denying consent) and completes initialization. | The background update timer task remains inactive. When simulating a new release descriptor on the update server, the DUT performs zero background downloads or updates. Updates occur only when manually triggered via the Web-UI (`UpdMech-WebUI`).   |   **PASS**   |

**Assessment Justification**: Functional testing confirms that user consent for automatic updates is
explicitly requested during initial onboarding setup. Granting consent reliably activates automatic
background updates (`UpdMech-Auto`), while denying consent ensures that background update routines
remain completely disabled.

**Verdict**: **PASS**

---

## Group Summary

The Ruuvi Gateway complies fully with Provision 5.3-4B of `ETSI EN 303 645`. During initial device
setup (`UserDec-5-Automatic-Updates` in `IXIT 26-UserDec`), the onboarding wizard explicitly queries
user consent to enable automatic updates. Functional testing confirms that granting consent enables
automatic background patch delivery (`UpdMech-Auto`), while denying consent (`Manual updates only`)
reliably disables all automated background update checks.

**Group Verdict**: **PASS**
