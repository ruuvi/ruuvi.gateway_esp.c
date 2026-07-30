# Test group 5.3-12: User Notification in Case of Disruptive Software Updates

Provision 5.3-12 — Status: **R C (18)**. Related IXIT: `IXIT 7-UpdMech`, `IXIT 26-UserDec`.

---

## Test case 5.3-12-1 (conceptual)

**Purpose**: To conceptually assess whether every update mechanism in `IXIT 7-UpdMech` appropriately
notifies the user or constrains operational disruption when a software update temporarily disrupts
the basic functioning of the DUT.

---

### Test Unit A: Assessment of Notification & Disruption Management

**Testing Methodology**: The test laboratory evaluated the "User Notification", "Description", and "
Configuration" declarations for every update mechanism in `IXIT 7-UpdMech` to assess how operational
disruptions (such as telemetry pipeline halts and system reboots) are communicated to or controlled
by the user.

| Mechanism ID    |                Basic Functioning Disrupted During Update?                 | Notification Vector & Disruption Minimization Strategy                                                                                                                                                                                                                                                                                                              | Compliance & Disruption Management Assessment                                                                                                                                                                                                       | Case Verdict |
|:----------------|:-------------------------------------------------------------------------:|:--------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|:----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|:------------:|
| `UpdMech-WebUI` | **Yes** (Telemetry processing and HTTP server halt during system restart) | Upon successful download and cryptographic verification, the Web-UI explicitly renders the completion and restart notification: *"All good! Update was successful. The device will restart automatically after a few seconds..."* immediately before invoking `gateway_restart()`.                                                                                  | **Explicit User Notification.** The operator manually initiates the update and is presented with an explicit, plain-text UI notification informing them of the automated restart and providing instructions for reconnecting post-reboot.           |   **PASS**   |
| `UpdMech-Auto`  |   **Yes** (Telemetry streaming halts during binary download and reboot)   | Initial version checking (`https://network.ruuvi.com/firmwareupdate`) occurs asynchronously without impact to BLE scanning. Upon detecting a new release, telemetry relaying (HTTP) and streaming (MQTT) are halted prior to binary downloading and automated reboot. Execution is constrained to user-configured schedule windows (`UserDec-5-Automatic-Updates`). | **Pre-Authorized Window Disruption Control.** Telemetry disruption and system reboot are constrained strictly to user-defined calendar schedule windows (permitted weekdays and time-of-day masks) authorized by the operator during configuration. |   **PASS**   |
| `UpdMech-USB`   |             **Yes** (Hardware held in bootloader reset mode)              | Physical engineering serial connection; flasher tool output displays real-time sector erase, programming, and verification status messages on the host console.                                                                                                                                                                                                     | **Direct Console Feedback.** Flashing is a manual physical action; progress and chip reset status are continuously rendered by the host CLI tool (`esptool.py`).                                                                                    |   **PASS**   |

**Assessment Justification**:

* **User-Initiated Updates (`UpdMech-WebUI`):** The DUT explicitly notifies the operator via an
  on-screen modal text message immediately prior to automatic restart, ensuring complete operator
  awareness of brief service disruption and providing clear post-reboot reconnection guidance.
* **Automated Background Updates (`UpdMech-Auto`):** Non-disruptive version polling identifies
  available updates asynchronously. Once an update is triggered, telemetry streams are closed
  cleanly, and binary downloading/rebooting executes within user-configured maintenance schedule
  windows (`UserDec-5-Automatic-Updates` in `IXIT 26-UserDec`), preventing unexpected operational
  downtime.

**Verdict**: **PASS**

---

## Summary Matrix for Test Case 5.3-12-1

| Mechanism ID    | Delivery Medium              | Notification & Disruption Control Strategy                                                                                       | Unit Verdict |
|:----------------|:-----------------------------|:---------------------------------------------------------------------------------------------------------------------------------|:------------:|
| `UpdMech-WebUI` | Network (HTTPS / Web-UI)     | Explicit post-download restart notification message displayed in Web-UI prior to `gateway_restart()`.                            |   **PASS**   |
| `UpdMech-Auto`  | Network (HTTPS / Background) | Asynchronous version polling; clean telemetry pipeline shutdown and automated reboot constrained to user-scheduled time windows. |   **PASS**   |
| `UpdMech-USB`   | Local Port (USB-UART)        | Continuous terminal output and chip status feedback rendered by local host tool (`esptool.py`).                                  |   **PASS**   |

---

## Group Summary

The Ruuvi Gateway aligns with Recommendation Provision 5.3-12 of `ETSI EN 303 645`. For
user-initiated updates (`UpdMech-WebUI`), the Web-UI explicitly notifies the operator of successful
update completion and impending automated restart. For automated background updates (
`UpdMech-Auto`), version polling is non-disruptive, while telemetry shutdown and system reboot are
constrained strictly to user-configured schedule windows (`IXIT 26-UserDec`).

**Group Verdict**: **PASS**
