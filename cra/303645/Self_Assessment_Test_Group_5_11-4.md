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

| Deletion Functionality ID (`IXIT 25-DelFunc`) | Target Data & Storage Location                    | Executed Deletion Action (`IXIT 2-UserInfo`)                                            | Assessment of Confirmation Clarity & Feedback (Unit c)                                                                                                                                                                                                                                               | Unit Verdict |
|:----------------------------------------------|:--------------------------------------------------|:----------------------------------------------------------------------------------------|:-----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|:------------:|
| **`DelFunc-Hardware-Factory-Reset`**          | On-Device Flash Partitions (`nvs` / `gw_cfg_def`) | Press and hold physical `CONFIGURE` button for 7 seconds or longer.                     | **Clear Physical & Operational Confirmation.** The DUT executes an immediate hardware reboot (`gateway_restart()`), drops active Wi-Fi station links, renders the former Web-UI unreachable, and activates the configuration hotspot (`Configure Ruuvi Gateway XXXX`) requiring default credentials. |   **PASS**   |
| **`DelFunc-Service-Account-Deletion`**        | Associated Service (Ruuvi Cloud Infrastructure)   | Request account deletion in Ruuvi Station app/portal and click email confirmation link. | **Clear Visual & Session Confirmation.** Upon clicking the emailed link, active app and browser sessions are logged out automatically, and the web interface displays an explicit, positive confirmation message stating that the user profile and data have been permanently erased.                |   **PASS**   |

* **Functional Assessment Justification**:
  1. **Personal Data Creation (Unit a):** Personal data (Wi-Fi station credentials, Web-UI
     passwords, M2M Bearer tokens, custom SSL keys, user profile PII, and telemetry streams) was
     created on the DUT and Ruuvi Cloud prior to test execution.
  2. **Execution According to Documentation (Unit b):** Both deletion functionalities were executed
     strictly following the published user documentation referenced in `IXIT 2-UserInfo`.
  3. **Confirmation Design Clarity (Unit c):** Both deletion mechanisms provide clear, unambiguous
     confirmation of successful completion. On-device reset provides physical reboot and hotspot
     re-activation feedback, while associated service deletion provides an explicit visual
     confirmation screen accompanied by automatic session teardown.

* **Verdict**: **PASS**

---

## Summary Matrix for Test Case 5.11-4-1

| Test Case           | Purpose / Focus                         | Assessment Summary                                                                                                           | Unit Verdict |
|:--------------------|:----------------------------------------|:-----------------------------------------------------------------------------------------------------------------------------|:------------:|
| **5.11-4-1 Unit a** | Typical Personal Data Creation          | Personal data created on DUT (Wi-Fi, passwords, tokens, SSL keys) and Ruuvi Cloud (account profile, telemetry).              |   **PASS**   |
| **5.11-4-1 Unit b** | Execution of Deletion Functionalities   | Executed on-device hardware factory reset and associated cloud account deletion according to `IXIT 2-UserInfo`.              |   **PASS**   |
| **5.11-4-1 Unit c** | Deletion Confirmation Design Assessment | Hardware reboot/hotspot activation and cloud confirmation screen/session logout provide clear confirmation of data deletion. |   **PASS**   |

---

## Group Summary

The Ruuvi Gateway complies fully with Recommendation Provision 5.11-4 of `ETSI EN 303 645`. The
system provides clear and transparent confirmations that user data has been deleted for all deletion
functionalities cataloged in `IXIT 25-DelFunc`. On-device factory reset (
`DelFunc-Hardware-Factory-Reset`) provides immediate physical confirmation via hardware reboot,
station link drop, and setup hotspot activation requiring default credentials. Associated service
account deletion (`DelFunc-Service-Account-Deletion`) provides explicit visual confirmation on the
web interface accompanied by automatic session logout, verifying successful data eradication.

**Group Verdict**: **PASS**
