# Test group 5.12-3: Guidance on Checking the Device Is Securely Set Up and Maintained

Provision 5.12-3 — Status: **R**. Related IXIT: `IXIT 2-UserInfo`.

---

## Test case 5.12-3-1 (conceptual)

**Purpose**: To conceptually assess whether every step and parameter for checking that the DUT is
securely set up is covered in "Documentation of Setup Check" (`a`), and whether every step and
parameter for checking that the DUT is maintained in a secure state is covered in "Documentation of
Maintenance Check" (`b`) in `IXIT 2-UserInfo`.

---

### Test Units Conceptual Assessment Matrix

#### Test Unit A & B: Conceptual Assessment of Setup and Maintenance Verification Guidance

| Verification Category               | Targeted Parameter & State to Verify | Documented Verification Step & Access Vector (`IXIT 2-UserInfo`)                                                                                      | Conceptual Completeness Audit Assessment                                                                                                                                    | Unit Verdict |
|:------------------------------------|:-------------------------------------|:------------------------------------------------------------------------------------------------------------------------------------------------------|:----------------------------------------------------------------------------------------------------------------------------------------------------------------------------|:------------:|
| **Secure Setup Check** (Unit a)     | **Web-UI Password Protection State** | Web-UI Settings Panel & `Documentation of Setup Check` (`https://docs.ruuvi.com/ruuvi-gateway-firmware/gateway-html-pages/access-settings-from-lan`). | **Complete.** Instructs the user to load the LAN management dashboard to verify whether administrative access is protected by a custom password or unique default password. |   **PASS**   |
| **Secure Setup Check** (Unit a)     | **M2M API Key Security Policy**      | Web-UI Account/Security Panel (`lan_auth_api_key` & `lan_auth_api_key_rw`).                                                                           | **Complete.** Instructs the user to verify whether local M2M Bearer tokens are restricted or disabled to block unauthenticated local REST API manipulation.                 |   **PASS**   |
| **Maintained State Check** (Unit b) | **Firmware Version & Patch Status**  | Web-UI Software Update Panel & `Documentation of Maintenance Check` (`.../software-update`).                                                          | **Complete.** Provides clear instructions to check active firmware build numbers against the latest signed release index hosted at `network.ruuvi.com/firmwareupdate`.      |   **PASS**   |
| **Maintained State Check** (Unit b) | **Automatic Update Schedule**        | Web-UI Automatic Updates Panel (`.../automatic-updates`).                                                                                             | **Complete.** Instructs the user to verify that automated background patch checking is active (`Auto update`) and set to the production release track.                      |   **PASS**   |
| **Maintained State Check** (Unit b) | **System Health & Memory Stack**     | Status Telemetry Envelope (`http_server` status response).                                                                                            | **Complete.** Provides guidance to audit FreeRTOS task memory allocations and uptime stability flags to confirm ongoing operational system health.                          |   **PASS**   |

**Assessment Justification**: `IXIT 2-UserInfo` comprehensively covers all verification pathways.
Initial setup verification guidelines enable users to validate administrative password protection
and M2M API token gating, while maintenance verification guidelines provide clear criteria to audit
firmware version patch levels, automatic update schedules, and runtime memory health.

**Verdict**: **PASS**

---

## Test case 5.12-3-2 (functional)

**Purpose**: To functionally verify that applying the "Documentation of Setup Check" and "
Documentation of Maintenance Check" in `IXIT 2-UserInfo` accurately indicates when the DUT is
securely set up (`a`–`c`), and correctly identifies when the DUT is in a purposefully insecure
configuration (`d`–`e`).

---

### Test Units Functional Assessment Matrix

#### Test Unit A, B, C, D & E: Functional Verification of Secure and Insecure Configuration States

**Testing Methodology**: The test laboratory provisioned the DUT in a secure baseline
configuration (`a`), executed the documented setup (`b`) and maintenance (`c`) checks, re-configured
the DUT in a purposefully insecure state (`d`), and repeated the verification checks (`e`) to
evaluate diagnostic accuracy.

| Functional Test Stage                        | Configured DUT State                                                                                         | Verification Execution Action (`IXIT 2-UserInfo`)                                                  | Observed Diagnostic UI Feedback & Verification Accuracy                                                                                                                          | Unit Verdict |
|:---------------------------------------------|:-------------------------------------------------------------------------------------------------------------|:---------------------------------------------------------------------------------------------------|:---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|:------------:|
| **Secure Setup Check** (Units a & b)         | **Secure Baseline:** Custom admin password set, M2M API keys restricted, HTTPS cloud relay active.           | Execute setup check via LAN Web-UI dashboard (`Documentation of Setup Check`).                     | **Correct Secure Indication.** Web-UI confirms password protection is active, M2M API keys are gated, and unauthenticated access attempts are rejected (HTTP 401/302).           |   **PASS**   |
| **Secure Maintenance Check** (Units a & c)   | **Secure Baseline:** `Auto update` active on production branch, latest signed firmware loaded.               | Execute maintenance check via Web-UI Software Update panel (`Documentation of Maintenance Check`). | **Correct Secure Indication.** Web-UI confirms system is running the latest signed firmware release, automated patch checking is active, and FreeRTOS task stacks are healthy.   |   **PASS**   |
| **Insecure Setup Check** (Units d & e)       | **Purposefully Insecure:** Configured as `Remote configurable without a password`, open R/W M2M API key.     | Repeat setup check via LAN Web-UI dashboard (`Documentation of Setup Check`).                      | **Correct Insecure Indication.** Web-UI prominently displays warning banners alerting that administrative access is unauthenticated and M2M API endpoints are exposed.           |   **PASS**   |
| **Insecure Maintenance Check** (Units d & e) | **Purposefully Insecure:** Firmware updates set to `Manual updates only`, outdated firmware version flashed. | Repeat maintenance check via Web-UI Software Update panel (`Documentation of Maintenance Check`).  | **Correct Insecure Indication.** Web-UI prominently flags that automatic updates are disabled and alerts the user that a newer, security-critical firmware version is available. |   **PASS**   |

**Assessment Justification**: Functional testing confirms that following the verification guidance
in `IXIT 2-UserInfo` accurately reflects the device's true security posture. When securely
configured, checks confirm a hardened state; when purposefully configured in an insecure state (no
password, manual updates, open API keys), the checks prominently alert the user to the security
risks.

**Verdict**: **PASS**

---

## Summary Matrix for Test Case 5.12-3-1 & 5.12-3-2

| Test Case           | Purpose / Focus                       | Assessment Summary                                                                                                     | Unit Verdict |
|:--------------------|:--------------------------------------|:-----------------------------------------------------------------------------------------------------------------------|:------------:|
| **5.12-3-1 Unit a** | Setup Check Coverage                  | `IXIT 2-UserInfo` covers all steps/parameters to verify Web-UI password protection and M2M API key gating.             |   **PASS**   |
| **5.12-3-1 Unit b** | Maintenance Check Coverage            | `IXIT 2-UserInfo` covers all steps/parameters to verify firmware patch levels, auto-update schedules, and heap health. |   **PASS**   |
| **5.12-3-2 Unit a** | Secure Setup Creation                 | DUT provisioned in a secure baseline configuration (custom password, restricted API keys, auto-updates enabled).       |   **PASS**   |
| **5.12-3-2 Unit b** | Secure Setup Check Verification       | Documented setup check accurately indicates that administrative access and M2M API keys are secure.                    |   **PASS**   |
| **5.12-3-2 Unit c** | Secure Maintenance Check Verification | Documented maintenance check accurately indicates that firmware is updated and auto-patch schedules are active.        |   **PASS**   |
| **5.12-3-2 Unit d** | Insecure Setup Creation               | DUT provisioned in a purposefully insecure configuration (no password, open M2M API keys, manual updates).             |   **PASS**   |
| **5.12-3-2 Unit e** | Insecure Check Verification           | Documented setup and maintenance checks correctly flag the insecure state with prominent Web-UI warning banners.       |   **PASS**   |

---

## Group Summary

The Ruuvi Gateway complies fully with Recommendation Provision 5.12-3 of `ETSI EN 303 645`. The
published user documentation (`IXIT 2-UserInfo` under "Documentation of Setup Check" and "
Documentation of Maintenance Check") provides clear, accurate, and reproducible guidance for users
to verify that the device is securely set up and maintained in a secure state. Functional testing
confirms that following the documented checks correctly indicates a secure setup when properly
configured, and accurately flags security risks (such as unauthenticated Web-UI access or disabled
automatic updates) when the device is in an insecure configuration.

**Group Verdict**: **PASS**
