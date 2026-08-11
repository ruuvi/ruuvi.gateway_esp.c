# Test group 5.3-11: Method and Content of Information Provided for Required Security Updates

Provision 5.3-11 — Status: **R C (17)**. Related IXIT: `IXIT 7-UpdMech`, `IXIT 26-UserDec`.

---

## Test case 5.3-11-1 (conceptual)

**Purpose**: To conceptually assess whether the method used to inform the user about the
availability of required security updates is recognizable and apparent (`a`), and whether
information regarding the risks mitigated by the update is available to the user (`b`).

---

### Test Units Conceptual Assessment Matrix

#### Test Unit A: Assessment of Notification Visibility and Clarity (Recognizable & Apparent)

* **Requirement**: Evaluate whether the method to inform the user about required security updates is
  recognizable and apparent.
* **Evaluation**:
  * For `UpdMech-WebUI`, when the operator accesses the "Software Update" dashboard page (
    `UserDec-3-Onboarding-Firmware-Update`), the DUT queries the version index at
    `https://network.ruuvi.com/firmwareupdate`. If a newer release or beta version is identified,
    the Web-UI displays plain-text version availability details, making update availability
    recognizable and apparent.
  * For `UpdMech-Auto`, background updates execute autonomously without user intervention or
    disruptive popups, ensuring seamless operational patch management.

#### Test Unit B: Assessment of Notification Content and Mitigated Risk Context

* **Requirement**: Evaluate whether the user notification includes information about the risks
  mitigated by the update.
* **Evaluation**:
  * The local Web-UI on the DUT displays the specific version number string of the available update
    on `UserDec-3-Onboarding-Firmware-Update`.
  * Detailed information regarding mitigated security risks, fixed vulnerabilities (CVEs), and
    feature changelogs is published externally on the official Ruuvi public repository release
    notes (`https://github.com/ruuvi/ruuvi.gateway_esp.c/releases`) accompanying each firmware
    release tag. This allows operators to review full risk mitigation contexts prior to or following
    update application.

---

### Update Mechanism Assessment Summary

| Mechanism ID    | Delivery Medium              | Unit a: Notification Method Recognizable & Apparent?                                                                                            | Unit b: Mitigated Risk Information Included?                                                                                                                                                                                                          | Case Verdict |
|:----------------|:-----------------------------|:------------------------------------------------------------------------------------------------------------------------------------------------|:------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|:------------:|
| `UpdMech-WebUI` | Network (HTTPS / Web-UI)     | **Yes.** Clear, plain-text target version number displayed directly on the "Software Update" page (`UserDec-3-Onboarding-Firmware-Update`).     | **Provided via Official Release Notes.** The DUT renders the target version string; detailed risk mitigation descriptions and changelogs are published on the official public release page (`https://github.com/ruuvi/ruuvi.gateway_esp.c/releases`). |   **PASS**   |
| `UpdMech-Auto`  | Network (HTTPS / Background) | **N/A (Silent Automation).** Operates silently in the background to ensure zero-touch patch management without interrupting gateway operations. | **N/A.** Operational logs record executed version transitions; public release notes document mitigated risks for each deployed tag.                                                                                                                   |   **PASS**   |
| `UpdMech-USB`   | Local Port (USB-UART)        | **N/A.** Offline physical serial flasher; does not perform network update checks or UI notifications.                                           | **N/A.** Firmware version details supplied manually via engineering release documentation.                                                                                                                                                            |   **PASS**   |

---

## Summary Matrix for Test Case 5.3-11-1

| Test Unit  | Purpose / Focus                             | Implementation Safeguard & Verification Strategy                                                                                     | Unit Verdict |
|:-----------|:--------------------------------------------|:-------------------------------------------------------------------------------------------------------------------------------------|:------------:|
| **Unit A** | Recognizable & Apparent Notification Method | Plain-text version number displayed on the "Software Update" page (`UserDec-3-Onboarding-Firmware-Update`).                          |   **PASS**   |
| **Unit B** | Information on Mitigated Risks              | Target version string shown on DUT; detailed changelogs and risk mitigation descriptions published on official GitHub release pages. |   **PASS**   |

---

## Group Summary

The Ruuvi Gateway aligns with Recommendation Provision 5.3-11 of `ETSI EN 303 645`. For
user-initiated network updates (`UpdMech-WebUI`), the DUT displays the available target version
string directly on the "Software Update" page (`UserDec-3-Onboarding-Firmware-Update`), with
detailed risk mitigation and CVE fix information provided via official public release documentation
on GitHub.

**Group Verdict**: **PASS**
