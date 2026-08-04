# Test group 5.3-6A: Configuration of Automatic Updates (Enable, Disable, Postpone)

Provision 5.3-6A — Status: **R C (16) (h)**. Related IXIT: `IXIT 7-UpdMech`, `IXIT 26-UserDec`.

---

## Test case 5.3-6A-1 (conceptual)

**Purpose**: To conceptually assess whether every automatic update mechanism defined in
`IXIT 7-UpdMech` provides the user with the ability to enable, disable, and postpone the automatic
installation of updates according to its documented "Configuration" settings.

### Test Unit A: Identification of Automatic Update Mechanisms

**Testing Methodology**: The test laboratory evaluated all update mechanisms in `IXIT 7-UpdMech` to
identify those that support update execution without requiring user interaction ("Initiation and
Interaction").

| Mechanism ID    | Delivery Medium            | Supports Execution Without User Interaction? | Automatic Update Classification      | Unit Verdict |
|:----------------|:---------------------------|:--------------------------------------------:|:-------------------------------------|:------------:|
| `UpdMech-Auto`  | Network (HTTPS / Port 443) |                   **Yes**                    | **Automatic Update Mechanism**       |   **PASS**   |
| `UpdMech-WebUI` | Network (HTTPS / Port 443) |                    **No**                    | On-Demand / User-Initiated Mechanism |   **PASS**   |
| `UpdMech-USB`   | Local Port (USB-UART)      |                    **No**                    | Offline Physical Maintenance Flasher |   **PASS**   |

---

### Test Unit B: Assessment of User Control Capabilities (Enable, Disable, Postpone)

**Testing Methodology**: For `UpdMech-Auto`, the test laboratory verified that the user is provided
with controls to enable, disable, and postpone automatic update installation in `IXIT 7-UpdMech` ("
Configuration") and `IXIT 26-UserDec` (`UserDec-5-Automatic-Updates`).

| Automatic Mechanism | Target Capability Required | Implemented Configuration Mechanism (`IXIT 7-UpdMech` / `IXIT 26-UserDec`)                            | Capability Assessment & Handoff Logic                                                                                                                                  | Capability Verdict |
|:--------------------|:---------------------------|:------------------------------------------------------------------------------------------------------|:-----------------------------------------------------------------------------------------------------------------------------------------------------------------------|:------------------:|
| `UpdMech-Auto`      | **1. Enable**              | Select `Auto update` (Regular release channel) or `Auto update (for beta testers)`.                   | Fully enables background version checks (`https://network.ruuvi.com/firmwareupdate`), automated downloading, RSA signature verification, and installation.             |      **PASS**      |
| `UpdMech-Auto`      | **2. Disable**             | Select `Manual updates only`.                                                                         | Completely deactivates the background update timer task. No automatic version checking or background update installation is performed.                                 |      **PASS**      |
| `UpdMech-Auto`      | **3. Postpone**            | Configure active schedule mask (permitted weekdays and specific daily time-of-day execution windows). | Postpones/defers background updates and system restarts outside of the user-designated operational window, allowing operators to prevent updates during peak activity. |      **PASS**      |

**Assessment Justification**:

* **Unit A:** `UpdMech-Auto` is correctly identified as the sole automatic update mechanism on the
  DUT.
* **Unit B:** Review of `IXIT 7-UpdMech` and `IXIT 26-UserDec` demonstrates that `UpdMech-Auto`
  provides complete user control:
  * **Enable:** The user can enable automatic updates by selecting the regular or beta channel.
  * **Disable:** The user can disable automatic updates by toggling the policy to
    `Manual updates only`.
  * **Postpone:** The user can postpone/restrict automatic updates by defining custom weekday and
    time-of-day execution filters, deferring installations to permitted maintenance windows.

**Verdict**: **PASS**

---

## Test case 5.3-6A-2 (functional)

**Purpose**: To functionally assess on the DUT that the user is provided with the ability to enable,
disable, and postpone the automatic installation of software updates for `UpdMech-Auto`.

### Test Unit A: Functional Assessment of Automatic Update Controls

**Testing Methodology**: The test laboratory configured each automatic update setting on the DUT via
the Web-UI (`UserDec-5-Automatic-Updates`), observing the functional background update task
behavior, network traffic, and installation timing.

| Automatic Mechanism | Action Executed on DUT                                                                                                           | Observed DUT Functional Behavior & Network Traffic Audit                                                                                                                                                                 | Unit Verdict |
|:--------------------|:---------------------------------------------------------------------------------------------------------------------------------|:-------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|:------------:|
| `UpdMech-Auto`      | **1. Enable Functional Test:** Select `Auto update` policy in LAN Web-UI.                                                        | The DUT initializes the background update task, querying `https://network.ruuvi.com/firmwareupdate` 2 hours post-boot and every 12 hours thereafter.                                                                     |   **PASS**   |
| `UpdMech-Auto`      | **2. Disable Functional Test:** Select `Manual updates only` policy in Web-UI.                                                   | The background update task deactivates immediately. Network packet captures confirm zero outbound background polling requests are sent to `network.ruuvi.com`.                                                           |   **PASS**   |
| `UpdMech-Auto`      | **3. Postpone Functional Test:** Set schedule mask to restrict updates to Sunday 02:00–04:00 while a newer version is available. | When a newer signed firmware release is published on `network.ruuvi.com`, the DUT defers background downloading and rebooting outside the allowed window, executing the update strictly when the scheduled window opens. |   **PASS**   |

**Assessment Justification**: Functional testing on the DUT verifies that the automatic update
mechanism (`UpdMech-Auto`) can be enabled, disabled, and postponed by the user as described in
`IXIT 7-UpdMech` and `IXIT 26-UserDec`. Selecting `Manual updates only` halts background polling
completely, while configuring schedule masks defers background installation and rebooting until the
designated maintenance window.

**Verdict**: **PASS**

---

## Group Summary

The Ruuvi Gateway complies fully with Provision 5.3-6A of `ETSI EN 303 645`. The automatic update
mechanism (`UpdMech-Auto`) provides the user with clear controls via the Web-UI (
`UserDec-5-Automatic-Updates` in `IXIT 26-UserDec`) to enable, disable, or postpone automatic
updates through configurable schedule masks. Conceptual and functional assessments confirm that all
three control states operate reliably on the DUT.

**Group Verdict**: **PASS**
