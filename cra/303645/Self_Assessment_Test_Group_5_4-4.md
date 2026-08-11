# Test group 5.4-4: Generation of Critical Security Parameters for Software Updates and Service Communication

Provision 5.4-4 — Status: **M F (m)**. Related IXIT: `IXIT 10-SecParam`, `IXIT 11-SecComMech`.

---

## Test case 5.4-4-1 (conceptual)

**Purpose**: To conceptually assess whether all Critical Security Parameters (CSPs) used for
software update integrity/authenticity checks or service communication protection are explicitly
documented in `IXIT 10-SecParam` (`a`), and to verify that their generation mechanisms ensure
per-device uniqueness and reduce the risk of automated attacks (`b`).

---

### Test Units Conceptual Assessment Matrix

#### Test Unit A: Assessment of IXIT Documentation Completeness for Parameter Generation

* **Requirement**: Check whether all CSPs used for integrity/authenticity checks of software updates
  or for protecting communication with associated services have their generation mechanisms
  documented in `IXIT 10-SecParam`.
* **Evaluation**:
  * **Software Update Integrity Parameters:** Software update verification relies on
    `SecParam-FW-Verification-Key` (RSA-3072 public key) and signature blocks (
    `SecParam-Main-Firmware-Signature`). Because these are classified as **`public`** security
    parameters, they do not constitute Critical Security Parameters under Provision 5.4-4.
  * **Service Communication CSPs:** All CSPs used for protecting service communication pathways are
    fully documented in `IXIT 10-SecParam`:
    * `SecParam-HMAC-Symmetric-Secrets`: Documented as initialized from the unique 64-bit hardware
      silicon register (`SecParam-Hardware-DeviceID`) and dynamically rotatable via high-entropy
      cloud server headers.
    * `SecParam-LAN-Bearer-Tokens`: Documented as generated via client-side Web Crypto API
      high-entropy random byte arrays (`crypto.lib.WordArray.random(32)`).
    * `SecParam-LAN-WebUI-Credentials`: Documented as initialized to the unique hardware $DEVICEID$
      and updated via client-side MD5 challenge-response hashing.
* **Verdict**: **PASS**

#### Test Unit B: Assessment of Per-Device Uniqueness and Automated Attack Mitigation

* **Requirement**: Assess whether the "Generation Mechanism" for each communication CSP ensures that
  the parameter is unique per device and produced via a mechanism that mitigates automated attacks
  against classes of devices (per Notes 1–3).
* **Evaluation**:

| Communication CSP (`IXIT 10-SecParam`) | Declared Generation Mechanism                                                                                                     | Per-Device Uniqueness & Attack Mitigation Assessment                                                                                                                                                                                                                    | Unit Verdict |
|:---------------------------------------|:----------------------------------------------------------------------------------------------------------------------------------|:------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|:------------:|
| `SecParam-HMAC-Symmetric-Secrets`      | Default seed extracted from factory silicon FICR registers ($DEVICEID$); rotatable via cloud response headers (`Ruuvi-HMAC-KEY`). | **Unique Per Device & Rotatable.** Anchoring the default seed to the factory-burned 64-bit silicon ID ensures no two physical units share the same default HMAC signing key. Runtime rotation allows servers to provision 256-bit cryptographically secure random keys. |   **PASS**   |
| `SecParam-LAN-Bearer-Tokens`           | Browser client-side generation using `crypto.enc.Base64.stringify(crypto.SHA256(crypto.lib.WordArray.random(32)))`.               | **High-Entropy & Unique.** Utilizes high-entropy pseudo-random number generators (PRNG) to produce 256-bit unique tokens per generation event, eliminating class-wide default token risks.                                                                              |   **PASS**   |
| `SecParam-LAN-WebUI-Credentials`       | Initial default mapped to $DEVICEID$; custom updates generated via client-side MD5 hashing (`user:gateway:password`).             | **Unique Default & Hashed.** Default passwords match the unique 64-bit silicon ID, preventing class-wide default password attacks. Custom passwords are saved solely as MD5 digests.                                                                                    |   **PASS**   |

* **Assessment Justification**: All critical security parameters used to secure communications are
  produced via mechanisms that guarantee per-device uniqueness (factory silicon registers or
  high-entropy PRNGs), effectively eliminating class-wide automated attacks against the gateway
  fleet.

* **Verdict**: **PASS**

---

## Summary Matrix for Test Case 5.4-4-1

| Test Unit          | Purpose / Focus                          | Assessment Summary                                                                                                                     | Verdict  |
|:-------------------|:-----------------------------------------|:---------------------------------------------------------------------------------------------------------------------------------------|:--------:|
| **5.4-4-1 Unit a** | Generation Mechanism Documentation       | All CSPs used for service communication protection have their generation mechanisms fully documented in `IXIT 10-SecParam`.            | **PASS** |
| **5.4-4-1 Unit b** | Uniqueness & Automated Attack Mitigation | CSP generation mechanisms rely on factory silicon registers ($DEVICEID$) and 256-bit PRNG entropy, guaranteeing per-device uniqueness. | **PASS** |

---

## Group Summary

The Ruuvi Gateway complies fully with Mandatory Provision 5.4-4 of `ETSI EN 303 645`. Software
update integrity relies on public key cryptography, while all Critical Security Parameters (CSPs)
used for service communication protection (`SecParam-HMAC-Symmetric-Secrets`,
`SecParam-LAN-Bearer-Tokens`, `SecParam-LAN-WebUI-Credentials`) are generated via mechanisms that
guarantee per-device uniqueness. Default secrets derive from factory-burned silicon
registers ($DEVICEID$), and M2M tokens utilize 256-bit PRNG entropy, successfully mitigating
class-wide automated attacks.

**Group Verdict**: **PASS**
