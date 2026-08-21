# Test group 5.4-3: No Hard-Coded Critical Security Parameters in Software

Provision 5.4-3 — Status: **M**. Related IXIT: `IXIT 1-AuthMech`, `IXIT 10-SecParam`,
`IXIT 11-SecComMech`.

---

## Test case 5.4-3-1 (conceptual)

**Purpose**: To conceptually assess whether any critical security parameter (CSP) declared in
`IXIT 10-SecParam` is hard-coded in the device software source code (`a`), and to evaluate whether
provisioning mechanisms ensure that hard-coded CSPs (if any) are replaced prior to operational use (
`b`).

---

### Test Units Conceptual Assessment Matrix

#### Test Unit A: Assessment of IXIT Declarations for Hard-Coded Parameters

* **Requirement**: For all parameters in `IXIT 10-SecParam`, check whether any CSP is hard-coded in
  source code and ensure this fact is reflected in its "Description" and "Provisioning Mechanism".
* **Evaluation**:
  * Comprehensive review of `IXIT 10-SecParam` confirms that **no Critical Security Parameters (
    CSPs) are hard-coded in source code**.
  * Parameters compiled into source code text segments (`SecParam-FW-Verification-Key`,
    `SecParam-CoProcessor-Verification-Stub`) are strictly **public security parameters**, which are
    explicitly permitted by `ETSI EN 303 645` (Section 5.4.3.0 Note).
  * All CSPs (administrative credentials, Wi-Fi WPA2 passphrases, TLS client private keys, M2M
    bearer tokens, and HMAC symmetric keys) are either read dynamically from hardware
    registers ($DEVICEID$), dynamically generated per device, or provisioned by the user.
* **Verdict**: **PASS**

#### Test Unit B: Provisioning Mechanism for Hard-Coded Parameters

* **Requirement**: Assess whether provisioning mechanisms ensure that hard-coded CSPs are replaced
  or overridden before operational deployment.
* **Evaluation**: Because zero CSPs are hard-coded in the source code, no replacement provisioning
  mechanisms for source-embedded secrets are required.
  * *Default Password Initialization:* The factory default administrative credential (
    `SecParam-LAN-WebUI-Credentials`) is derived at runtime from the unique 64-bit hardware
    register (`SecParam-Hardware-DeviceID`), ensuring that every device possesses a unique default
    password out-of-the-box without hard-coding credentials in firmware binary images.
  * *M2M Bearer Tokens:* Pseudo-randomly generated per installation via browser-side 256-bit
    cryptographic entropy routines (`crypto.lib.WordArray.random(32)`).
* **Verdict**: **PASS**

---

## Test case 5.4-3-2 (functional)

**Purpose**: To functionally verify that no common or static hard-coded critical security parameters
exist across software instances and that dynamic provisioning mechanisms are applied during
operation.

---

### Test Unit A: Functional Assessment of Dynamic Provisioning & Parameter Uniqueness

**Testing Methodology**: The test laboratory inspected compiled firmware binary images (
`ruuvi_gateway_esp.bin`), evaluated runtime initialization behavior across multiple DUT units, and
verified password generation routines.

| Checked Parameter Category (`IXIT 10-SecParam`)                                       | Implementation & Source Code Status                                                      | Observed Functional DUT Behavior                                                                                                                                       | Unit Verdict |
|:--------------------------------------------------------------------------------------|:-----------------------------------------------------------------------------------------|:-----------------------------------------------------------------------------------------------------------------------------------------------------------------------|:------------:|
| **Administrative Credentials** (`SecParam-LAN-WebUI-Credentials`)                     | Not hard-coded in source code. Derived at boot from nRF52811 FICR register ($DEVICEID$). | Inspecting two distinct physical DUT units running identical firmware binaries confirms each presents a distinct, unique default Web-UI password (`lan_auth_default`). |   **PASS**   |
| **Wi-Fi & Telemetry Credentials** (`SecParam-WiFi-STA-Credentials`, Telemetry Assets) | Not hard-coded in source code. Stored in `nvs` partition (`ruuvi.json`).                 | Freshly flashed units initialize with empty credential structures. User entry via onboarding wizard or Web-UI populates unique credentials.                            |   **PASS**   |
| **M2M API Tokens** (`SecParam-LAN-Bearer-Tokens`)                                     | Not hard-coded in source code. Generated client-side via JavaScript crypto library.      | Generating tokens on multiple units yields distinct, high-entropy 256-bit random strings (`lan_auth_api_key`).                                                         |   **PASS**   |
| **Public Keys & Verifiers** (`SecParam-FW-Verification-Key`)                          | Statically compiled in firmware text segment.                                            | Stored in public application text space; parameter is public, not a CSP.                                                                                               |   **PASS**   |

**Assessment Justification**: Firmware binary analysis and multi-device functional testing confirm
that no critical security parameters are hard-coded in source code. Default credentials and secret
keys are dynamically anchored to unique hardware registers or generated per deployment instance.

**Verdict**: **PASS**

---

## Summary Matrix for Test Case 5.4-3-1 & 5.4-3-2

| Test Case          | Purpose / Focus                   | Assessment Summary                                                                           | Verdict  |
|:-------------------|:----------------------------------|:---------------------------------------------------------------------------------------------|:--------:|
| **5.4-3-1 Unit a** | IXIT Declaration Consistency      | No CSPs are hard-coded in source; compiled keys are strictly public parameters.              | **PASS** |
| **5.4-3-1 Unit b** | Provisioning Mechanism Evaluation | Factory default credentials derive dynamically from unique silicon registers ($DEVICEID$).   | **PASS** |
| **5.4-3-2 Unit a** | Functional Source & Binary Check  | Multi-device verification confirms unique default credentials and zero static embedded CSPs. | **PASS** |

---

## Group Summary

The Ruuvi Gateway complies fully with Mandatory Provision 5.4-3 of `ETSI EN 303 645`. No critical
security parameters are hard-coded in the device software source code or compiled binary
executables. Factory default administrative credentials and symmetric root keys are dynamically
derived at boot time from the unique, read-only hardware register (`SecParam-Hardware-DeviceID`),
ensuring that every gateway model possesses unique credentials out-of-the-box.

**Group Verdict**: **PASS**
