# Test group 5.4-2: Tamper-Resistant Storage of Hard-Coded Identities

Provision 5.4-2 — Status: **M F (l)**. Related IXIT: `IXIT 10-SecParam`.

---

## Test case 5.4-2-1 (conceptual)

**Purpose**: To conceptually assess whether every hard-coded identity in `IXIT 10-SecParam` is
explicitly declared as such (`a`), whether its security guarantees include tamper resistance (`b`),
and whether the declared protection scheme provides hardware/software tamper resistance (`c`).

---

### Test Units Conceptual Assessment Matrix

#### Test Unit A: Identification and Explicit Statement of Hard-Coded Identity

* **Requirement**: Verify that every sensitive security parameter in `IXIT 10-SecParam` used as a
  hard-coded identity provides an explicit statement in its "Description".
* **Evaluation**: `SecParam-Hardware-DeviceID` explicitly states that it is the 64-bit
  hardware-unique identifier ($DEVICEID$) extracted from internal silicon registers, serving as the
  static identity of the DUT. No other parameters in `IXIT 10-SecParam` function as hard-coded
  device identities.
* **Verdict**: **PASS**

#### Test Unit B: Security Guarantees for Tamper-Resistance

* **Requirement**: Assess whether the security guarantees for the hard-coded identity include
  protection against physical, electrical, and software tampering.
* **Evaluation**: `SecParam-Hardware-DeviceID` specifies **Integrity** and **Confidentiality**
  guarantees, explicitly declaring the identity as immutable and protected against unauthorized
  modification or remote spoofing.
* **Verdict**: **PASS**

#### Test Unit C: Protection Scheme Suitability for Tamper-Resistance

* **Requirement**: Assess whether the "Protection Scheme" declared for the hard-coded identity
  effectively provides tamper resistance under the baseline attacker model (Clause D.2).
* **Evaluation**:
  * **Silicon Key Storage:** Stored directly inside the Factory Information Configuration
    Registers (FICR) on the nRF52811 silicon layout.
  * **Hardware Immutability:** Burned permanently during chip fabrication by the silicon vendor. The
    registers are read-only hardware cells that cannot be modified, erased, or re-flashed via
    software APIs, JTAG/SWD commands, or firmware updates.
* **Verdict**: **PASS**

---

## Test case 5.4-2-2 (functional)

**Purpose**: To functionally assess whether the protection scheme for the hard-coded identity is
implemented as documented in `IXIT 10-SecParam` without indications of non-conformity or field
alterability.

---

### Test Unit A: Functional Assessment of Tamper-Resistant Implementation

**Testing Methodology**: The test laboratory evaluated the runtime behavior of
`SecParam-Hardware-DeviceID` on the DUT, attempting software modification via local APIs, serial
interface inspection, and firmware flashing operations.

| Tested Parameter (`IXIT 10-SecParam`) | Documented Protection Scheme                                                                   | Observed Functional DUT Behavior                                                                                                                                                                                                                                     | Unit Verdict |
|:--------------------------------------|:-----------------------------------------------------------------------------------------------|:---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|:------------:|
| `SecParam-Hardware-DeviceID`          | Read-only factory silicon cells (FICR) on nRF52811 layout; immutable during product lifecycle. | **Immutability Confirmed.** The 64-bit value is read directly from hardware registers during boot. Firmware update cycles (`UpdMech-WebUI`, `UpdMech-USB`) and full NVS flash erasures leave the register contents untouched, confirming hardware tamper resistance. |   **PASS**   |

**Assessment Justification**: Functional testing demonstrates that `SecParam-Hardware-DeviceID` is
read directly from factory silicon registers (FICR) and cannot be altered or overwritten by software
commands, flash formatting, or firmware updates.

**Verdict**: **PASS**

---

## Summary Matrix for Test Case 5.4-2-1 & 5.4-2-2

| Test Case          | Purpose / Focus                 | Security Mechanism & Verification Strategy                                                           | Unit Verdict |
|:-------------------|:--------------------------------|:-----------------------------------------------------------------------------------------------------|:------------:|
| **5.4-2-1 Unit a** | Explicit Documentation          | `SecParam-Hardware-DeviceID` is explicitly declared as the static hard-coded device identity.        |   **PASS**   |
| **5.4-2-1 Unit b** | Tamper-Resistance Guarantees    | Security guarantees mandate immutability and protection against unauthorized modification.           |   **PASS**   |
| **5.4-2-1 Unit c** | Protection Scheme Suitability   | Stored in read-only factory silicon registers (nRF52811 FICR), providing hardware tamper resistance. |   **PASS**   |
| **5.4-2-2 Unit a** | Functional Implementation Check | Testing confirms identity resists software modification, NVS erasures, and OTA reflashing.           |   **PASS**   |

---

## Group Summary

The Ruuvi Gateway complies fully with Mandatory Provision 5.4-2 of `ETSI EN 303 645`. The hard-coded
unique device identifier (`SecParam-Hardware-DeviceID`) is explicitly documented in
`IXIT 10-SecParam` and stored within immutable, read-only factory silicon registers (nRF52811 FICR).
Functional testing confirms that the identity cannot be tampered with or modified by software
updates, flash erasures, or local configuration changes.

**Group Verdict**: **PASS**
