# Test group 5.11-4: Clear Confirmation of User Data Deletion

Provision 5.11-4 — Status: **R**. Related IXIT: `IXIT 2-UserInfo`, `IXIT 21-PersData`,
`IXIT 25-DelFunc`.

---

## Test case 5.11-4-1 (functional)

**Purpose**: To functionally assess whether the user is provided with a clear confirmation that data
has been successfully deleted when executing each deletion functionality in `IXIT 25-DelFunc`
addressing personal data stored on the DUT or associated services (`a`–`c`).

---

### Test Units Functional Assessment Matrix

#### Test Unit A, B & C: Functional Assessment of Deletion Confirmation Clarity

**Testing Methodology**: The test laboratory created typical personal data on the DUT and Ruuvi
Cloud (`a`), executed each deletion functionality in `IXIT 25-DelFunc` according to
`IXIT 2-UserInfo` (`b`), and evaluated whether the system provided clear, transparent, and
unambiguous confirmation signals that the corresponding data was successfully deleted (`c`).

| Deletion Functionality ID (`IXIT 25-DelFunc`) | Target Data & Storage Location                    | Executed Deletion Action (`IXIT 2-UserInfo`)                                                                                                                                            | Assessment of Confirmation Clarity & Feedback (Unit c)                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                         | Unit Verdict |
|:----------------------------------------------|:--------------------------------------------------|:----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|:-------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|:------------:|
| **`DelFunc-Hardware-Factory-Reset`**          | On-Device Flash Partitions (`nvs` / `gw_cfg_def`) | Press and hold the physical `CONFIGURE` button through the initial restart (~5–6 s) until the status LED blinks red 200 ms on / 200 ms off (~11 s total hold), then release the button. | **Clear Visual & Operational Confirmation.** The firmware signals erasure completion via `leds_notify_cfg_erased()` with the `LEDS_BLINKING_ON_CFG_ERASED` pattern (repeating red 200 ms on / 200 ms off) while holding the button. Upon button release, the gateway executes another restart via `gateway_restart_immediate_no_cleanup()`, and operational confirmation is established as former Wi-Fi station connections drop, the previous Web-UI session terminates, and the setup hotspot (`Configure Ruuvi Gateway XXXX`) becomes active requiring default credentials. |   **PASS**   |
| **`DelFunc-Service-Account-Deletion`**        | Associated Service (Ruuvi Cloud Infrastructure)   | Request account deletion in the Ruuvi Station mobile app or web portal and confirm via the authorization link sent to the user's registered email address.                              | **Clear Visual & Session Confirmation.** Upon clicking the emailed authorization link, active app and browser sessions are logged out automatically, and the web interface displays an explicit, positive confirmation message stating that the user profile and associated data have been permanently erased.                                                                                                                                                                                                                                                                 |   **PASS**   |

* **Functional Assessment Justification**:
  1. **Personal Data Creation (Unit a):** Personal data (Wi-Fi station credentials, Web-UI
     passwords, M2M Bearer tokens, custom SSL keys, user profile PII, and telemetry streams) was
     created on the DUT and Ruuvi Cloud prior to test execution.
  2. **Execution According to Documentation (Unit b):** Both deletion functionalities were executed
     strictly following the user documentation referenced in `IXIT 2-UserInfo`.
  3. **Confirmation Design Clarity (Unit c):** Both deletion mechanisms provide clear, unambiguous
     confirmation of successful completion. On-device reset provides an explicit visual LED
     completion pattern (red 200 ms on / 200 ms off) followed by a release-triggered restart into
     the unconfigured captive hotspot state. Associated service deletion provides an explicit visual
     confirmation message on the interface accompanied by automatic session teardown.

* **Verdict**: **PASS**

---

## Summary Matrix for Test Case 5.11-4-1

| Test Case           | Purpose / Focus                         | Assessment Summary                                                                                                                                                                  | Unit Verdict |
|:--------------------|:----------------------------------------|:------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|:------------:|
| **5.11-4-1 Unit a** | Typical Personal Data Creation          | Personal data created on DUT (Wi-Fi, passwords, tokens, SSL keys) and Ruuvi Cloud (account profile, telemetry).                                                                     |   **PASS**   |
| **5.11-4-1 Unit b** | Execution of Deletion Functionalities   | Executed on-device hardware factory reset (~11 s hold, release upon LED pattern) and associated cloud account deletion according to `IXIT 2-UserInfo`.                              |   **PASS**   |
| **5.11-4-1 Unit c** | Deletion Confirmation Design Assessment | LED 200 ms on/off pattern prior to release, release-triggered restart into setup hotspot, and cloud confirmation screen/session logout provide clear confirmation of data deletion. |   **PASS**   |

---

## Group Summary

The Ruuvi Gateway complies fully with Recommendation Provision 5.11-4 of `ETSI EN 303 645`. The
system provides clear and transparent confirmations that user data has been deleted for all deletion
functionalities cataloged in `IXIT 25-DelFunc`. On-device factory reset
(`DelFunc-Hardware-Factory-Reset`) provides deterministic visual confirmation via the
`LEDS_BLINKING_ON_CFG_ERASED` status LED pattern (red 200 ms on / 200 ms off) once partition
formatting finishes; upon releasing the button, the device restarts into the unconfigured setup
hotspot (`Configure Ruuvi Gateway XXXX`) requiring factory credentials.
Associated service account deletion (`DelFunc-Service-Account-Deletion`) provides explicit visual
confirmation on the web interface accompanied by automatic session logout, verifying successful data
eradication.

**Group Verdict**: **PASS**
