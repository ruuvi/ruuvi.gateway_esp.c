# Test group 5.3-6B: Configuration of Update Notifications (Enable and Disable)

Provision 5.3-6B — Status: **R C (16) (h)**. Related IXIT: `IXIT 7-UpdMech`, `IXIT 26-UserDec`.

---

## Test case 5.3-6B-1 (conceptual)

**Purpose**: To conceptually assess whether every update mechanism in `IXIT 7-UpdMech` that provides
update notifications ("User Notification") provides the user with the ability to enable and disable
update notifications according to its "Configuration" declarations.

### Test Unit A: Conceptual Assessment of Update Notification Controls

**Testing Methodology**: The test laboratory evaluated the "User Notification" and "Configuration"
declarations for every update mechanism in `IXIT 7-UpdMech` and cross-referenced the user
configuration controls in `IXIT 26-UserDec` (`UserDec-3-Onboarding-Firmware-Update` and
`UserDec-5-Automatic-Updates`).

| Mechanism ID    | Provides Update Notifications? (`IXIT 7-UpdMech`) | Notification Delivery Vector                                                                                                                 | Configurable to Enable / Disable Notifications? | User Control & Handoff Mechanism (`IXIT 26-UserDec`)                                                                                                                                                                                                                                                                                                                                                                                                                                                                                           | Case Verdict |
|:----------------|:-------------------------------------------------:|:---------------------------------------------------------------------------------------------------------------------------------------------|:-----------------------------------------------:|:-----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|:------------:|
| `UpdMech-WebUI` |                      **Yes**                      | Text-based version availability details displayed directly on the "Software Update" dashboard page (`UserDec-3-Onboarding-Firmware-Update`). |                     **Yes**                     | **Enabled:** Selecting `Auto update` or `Auto update (for beta testers)` in `UserDec-5-Automatic-Updates` targets the single selected release track on `UserDec-3-Onboarding-Firmware-Update`.<br>**Disabled / Manual Mode:** Selecting `Manual updates only` in `UserDec-5-Automatic-Updates` configures the "Software Update" page (`UserDec-3-Onboarding-Firmware-Update`) to query both `latest` and `beta` channels simultaneously, displaying all available versions so the operator can choose which update to apply or choose to skip. |   **PASS**   |
| `UpdMech-Auto`  |                      **No**                       | Silent background operation; no physical LED or network notification messages emitted.                                                       |                       N/A                       | Does not emit notifications; background tasks check, stage, and apply updates silently according to schedule.                                                                                                                                                                                                                                                                                                                                                                                                                                  |   **PASS**   |
| `UpdMech-USB`   |                      **No**                       | Offline physical serial flasher; no update notification mechanism present.                                                                   |                       N/A                       | Local engineering serial connection; update notifications are not applicable.                                                                                                                                                                                                                                                                                                                                                                                                                                                                  |   **PASS**   |

**Assessment Justification**:

* `UpdMech-WebUI` is the sole update mechanism on the DUT that provides user update notifications (
  displaying update availability text on the "Software Update" page
  `UserDec-3-Onboarding-Firmware-Update`).
* The user configures how these update notifications are queried and presented via
  `UserDec-5-Automatic-Updates`. When set to `Auto update` or `Auto update (for beta testers)`, the
  notification displays updates for the chosen channel. When set to `Manual updates only`, automated
  background updates are disabled, and the "Software Update" page (
  `UserDec-3-Onboarding-Firmware-Update`) evaluates both `release` and `beta` index descriptors,
  presenting all available version choices to the administrator or allowing them to skip.
* `UpdMech-Auto` operates silently without user notifications, satisfying the assignment criteria
  for Test Case 5.3-6B-1.

**Verdict**: **PASS**

---

## Test case 5.3-6B-2 (functional)

**Purpose**: To functionally verify that the user can enable and disable update notifications on the
DUT for update mechanisms that provide notification features.

### Test Unit A: Functional Assessment of Notification Enable/Disable Controls

**Testing Methodology**: The test laboratory functionally tested the update notification behavior of
`UpdMech-WebUI` under single-channel enabled states (`Auto update`) and manual dual-channel
inspection states (`Manual updates only`) while simulating available release and beta updates on
`https://network.ruuvi.com/firmwareupdate`.

| Operational Configuration Tested                       | User Interaction & Policy Selection                                                                                                                                                 | Observed Functional DUT Behavior & UI Notification                                                                                                                                                                                                                                                                                                                           | Case Verdict |
|:-------------------------------------------------------|:------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|:-----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|:------------:|
| **Notification Single-Channel Target Path**            | User sets Update Policy to `Auto update` (or `Auto update for beta testers`) in `UserDec-5-Automatic-Updates` (or enters `UserDec-3-Onboarding-Firmware-Update` during onboarding). | Upon querying the version index and detecting a newer version string on the selected channel, the DUT displays an informational text notification on the `UserDec-3-Onboarding-Firmware-Update` page showing the single target version available for update.                                                                                                                 |   **PASS**   |
| **Notification Manual / Dual-Channel Inspection Path** | User sets Update Policy to `Manual updates only` in `UserDec-5-Automatic-Updates`.                                                                                                  | Automated background update execution is disabled. When the user navigates to `UserDec-3-Onboarding-Firmware-Update`, the DUT queries both `latest` and `beta` index descriptors. If both contain newer versions, both are displayed as selectable text choices alongside a "Skip / Keep Current" option. If no new beta exists, only the release version text is displayed. |   **PASS**   |

**Assessment Justification**: Functional testing confirms that update notifications are rendered as
clear text information on `UserDec-3-Onboarding-Firmware-Update`. Toggling settings in
`UserDec-5-Automatic-Updates` successfully controls whether the device checks a single automated
channel or executes manual dual-channel (`release` and `beta`) inspection.

**Verdict**: **PASS**

---

## Group Summary

The Ruuvi Gateway complies fully with Provision 5.3-6B of `ETSI EN 303 645`. For the update
mechanism that provides user notifications (`UpdMech-WebUI`), the user is provided with clear
configuration controls in the Web-UI (`UserDec-5-Automatic-Updates` in `IXIT 26-UserDec`) to
configure single-channel update notifications or enable manual dual-channel (`release` and `beta`)
inspection and version selection on the "Software Update" page (
`UserDec-3-Onboarding-Firmware-Update`).

**Group Verdict**: **PASS**
