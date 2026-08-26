# Test group 5.5-3: Cryptographic Algorithms and Primitives Are Updatable

Provision 5.5-3 — Status: **R**. Related IXIT: `IXIT 6-SoftComp`, `IXIT 7-UpdMech`.

---

## Test case 5.5-3-1 (conceptual)

**Purpose**: To conceptually assess whether every software component indicating cryptographic usage
in `IXIT 6-SoftComp` references a valid update mechanism (`a`), and to verify that the manufacturer
explicitly considers the side effects of replacing those cryptographic algorithms and primitives (
`b`).

---

### Test Units Conceptual Assessment Matrix

#### Test Unit A: Verification of Referenced Update Mechanisms

* **Requirement**: For each software component in `IXIT 6-SoftComp` indicating "Cryptographic Usage:
  Yes", verify that a valid update mechanism in `IXIT 7-UpdMech` is referenced.
* **Evaluation**: All four software components utilizing cryptographic primitives reference active
  update mechanisms:
  * `SoftComp-SecondBoot`: Updatable via local direct serial connection (`UpdMech-USB`).
  * `SoftComp-MainFW`: Fully updatable via network OTA tracks (`UpdMech-WebUI`, `UpdMech-Auto`) and
    local serial connection (`UpdMech-USB`).
  * `SoftComp-nRF52FW`: Fully updatable via host-driven flash updates over network tracks (
    `UpdMech-WebUI`, `UpdMech-Auto`) and local serial connection (`UpdMech-USB`).
  * `SoftComp-WebUI`: Fully updatable via dual FATFS filesystem partition staging (`UpdMech-WebUI`,
    `UpdMech-Auto`, `UpdMech-USB`).
* **Verdict**: **PASS**

#### Test Unit B: Assessment of Side Effects Management

* **Requirement**: Verify that the manufacturer evaluates potential side effects (e.g., hardware
  constraints, NVS schema migration, dynamic memory allocation limits) when updating or replacing
  cryptographic algorithms and primitives.
* **Evaluation**:

| Software Component ID (`IXIT 6-SoftComp`) | Cryptographic Primitives Employed                                      | Referenced Update Mechanism                        | Side Effects Considered & Evaluation Strategy                                                                                                                                                        | Unit Verdict |
|:------------------------------------------|:-----------------------------------------------------------------------|:---------------------------------------------------|:-----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|:------------:|
| `SoftComp-ROMBoot`                        | None (Standard instruction jumps)                                      | N/A (Mask ROM)                                     | **N/A.** Immutable silicon hardware; no cryptographic usage.                                                                                                                                         |   **N/A**    |
| `SoftComp-SecondBoot`                     | Partition layout hash verification                                     | `UpdMech-USB`                                      | **Boot Validation Testing.** SDD evaluates early boot sequence constraints, flash partition table alignments, and bootloader size boundaries prior to release.                                       |   **PASS**   |
| `SoftComp-MainFW`                         | `mbedtls` (TLS 1.2/1.3, AES-GCM), `HMAC-SHA256`, `MD5`, `RSA-3072-PSS` | `UpdMech-WebUI`<br>`UpdMech-Auto`<br>`UpdMech-USB` | **Comprehensive CI/CD Sweeps.** Evaluates NVS configuration persistence, TLS cloud endpoint compatibility, and heap memory allocation constraints under mbedTLS pre-allocated buffer configurations. |   **PASS**   |
| `SoftComp-nRF52FW`                        | SWD RAM-injected SHA-256 boot flash verification stub                  | `UpdMech-WebUI`<br>`UpdMech-Auto`<br>`UpdMech-USB` | **SWD Remediation Verification.** SDD tests inter-chip SWD communication timing, RAM stub execution boundaries, and automated co-processor code restoration loops.                                   |   **PASS**   |
| `SoftComp-WebUI`                          | `crypto-js` / `elliptic` (ECDH P-256, AES-CBC)                         | `UpdMech-WebUI`<br>`UpdMech-Auto`<br>`UpdMech-USB` | **Handshake Integrity Checks.** Evaluates cross-browser JS crypto compatibility, local nonced challenge-response verification, and session key exchange stability.                                   |   **PASS**   |

* **Assessment Justification**: All software components employing cryptographic primitives are
  updatable via documented OTA or local interfaces. The manufacturer systematically evaluates update
  side effects—including NVS configuration persistence, partition alignment, and dynamic memory
  boundaries—through continuous integration (CI) test suites and regression sweeps prior to release.

* **Verdict**: **PASS**

---

## Summary Matrix for Test Case 5.5-3-1

| Test Unit          | Purpose / Focus              | Assessment Summary                                                                                               | Verdict  |
|:-------------------|:-----------------------------|:-----------------------------------------------------------------------------------------------------------------|:--------:|
| **5.5-3-1 Unit a** | Referenced Update Mechanisms | Every software component using cryptography references an active update mechanism in `IXIT 7-UpdMech`.           | **PASS** |
| **5.5-3-1 Unit b** | Side Effects Management      | Potential side effects (memory limits, partition tables, NVS compatibility) are systematically evaluated by SDD. | **PASS** |

---

## Group Summary

The Ruuvi Gateway complies fully with Recommendation Provision 5.5-3 of `ETSI EN 303 645`. All
software components utilizing cryptographic primitives (`SoftComp-SecondBoot`, `SoftComp-MainFW`,
`SoftComp-nRF52FW`, `SoftComp-WebUI`) are updatable via documented over-the-air (`UpdMech-WebUI`,
`UpdMech-Auto`) or local serial (`UpdMech-USB`) update mechanisms. Side effects of replacing or
updating cryptographic algorithms are rigorously evaluated through CI regression testing, partition
boundary verification, and memory safety sweeps.

**Group Verdict**: **PASS**
