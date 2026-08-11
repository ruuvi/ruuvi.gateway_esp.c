# Test group 5.3-7: Best Practice Cryptography Used for Update Mechanisms

Provision 5.3-7 — Status: **M F (g)**. Related IXIT: `IXIT 7-UpdMech`.

---

## Test case 5.3-7-1 (conceptual)

**Purpose**: To conceptually assess whether the cryptographic methods employed across all update
mechanisms defined in `IXIT 7-UpdMech` provide necessary security guarantees (`a`), are appropriate
to achieve these guarantees (`b`), align with best practice reference catalogues (`c`), and are not
vulnerable to feasible attacks (`d`).

---

### Test Units Assessment Matrix

#### Test Unit A: Appropriateness of Security Guarantees

* **Requirement**: The security guarantees must at least fulfill the protection of **Integrity** and
  **Authenticity** for firmware updates.
* **Evaluation**: `IXIT 7-UpdMech` explicitly guarantees **Integrity** (detecting corrupted,
  incomplete, or tampered payload blocks) and **Authenticity** (verifying that updates originate
  strictly from the manufacturer holding the private signing key). These guarantees are appropriate
  and necessary for preventing unauthorized firmware execution.
* **Verdict**: **PASS**

#### Test Unit B: Appropriateness of the Mechanism Architecture

* **Requirement**: The holistic design of each update mechanism must effectively achieve the stated
  security guarantees.
* **Evaluation**:
  * **`UpdMech-WebUI` & `UpdMech-Auto`**: Combine transport-layer encryption/authentication (TLS
    1.2/1.3 over HTTPS) with application-layer digital signatures (RSA-3072-PSS over SHA-256).
    Post-boot cross-verification of auxiliary filesystems (`fatfs_gwui` / `fatfs_nrf52`) and SWD
    RAM-injected SHA-256 digests over the nRF52 co-processor flash guarantee end-to-end authenticity
    and integrity across all software layers.
  * **`UpdMech-USB`**: Enforces physical proximity access. Flasher tool executes MD5 transfer
    checks, while post-reset boot processes execute RSA-3072-PSS self-signature parsing and
    auxiliary filesystem signature validation before initialization.
* **Verdict**: **PASS**

#### Test Unit C: Best Practice Cryptography Reference Catalogue Evaluation

* **Requirement**: Cryptographic primitives, parameter lengths, and padding schemes must be
  recognized as best practice in reference catalogues (e.g., SOGIS Agreed Cryptographic Mechanisms,
  ETSI TR 103 621, NIST SP 800-57 / SP 800-131A).

| Cryptographic Primitive / Protocol | Implementation Details (`IXIT 7-UpdMech`)    | Reference Standard / Catalogue Alignment                                                                                           | Evaluated Security Level | Compliance Status |
|:-----------------------------------|:---------------------------------------------|:-----------------------------------------------------------------------------------------------------------------------------------|:------------------------:|:-----------------:|
| **Digital Signature Scheme**       | **RSA-3072** with **RSA-PSS** padding        | **SOGIS v1.1 / NIST SP 800-57:** RSA $\ge 3000$ bits provides 128-bit security strength; recognized for long-term use beyond 2030. |         128-bit          | **Best Practice** |
| **Cryptographic Hash Function**    | **SHA-256**                                  | **SOGIS v1.1 / ETSI TR 103 621:** Fully approved collision-resistant hash primitive.                                               |         128-bit          | **Best Practice** |
| **Signature Padding Scheme**       | **RSA-PSS** (Probabilistic Signature Scheme) | **PKCS #1 v2.2 / SOGIS:** Superior provable security strength compared to legacy PKCS#1 v1.5 padding.                              |           High           | **Best Practice** |
| **Transport Security**             | **TLS 1.2 / TLS 1.3**                        | **ETSI TS 103 645 / SOGIS:** Industry standard for secure authenticated network transport over public networks.                    |           High           | **Best Practice** |

* **Verdict**: **PASS**

#### Test Unit D: Resilience Against Feasible Cryptanalytic Attacks

* **Requirement**: Used cryptographic details must not be known to be vulnerable to feasible
  attacks (considering the baseline attacker model in Clause D.2).
* **Evaluation**:
  * **RSA-3072-PSS / SHA-256**: No feasible mathematical or cryptanalytic attacks exist against
    3072-bit RSA keys or SHA-256 collision resistance under current or near-future technologies. The
    use of RSA-PSS padding mathematically eliminates padding oracle vulnerabilities inherent to
    legacy PKCS#1 v1.5 implementations (e.g., Bleichenbacher-style attacks).
  * **Key Management Isolation**: The private RSA-3072 key is isolated within encrypted CI/CD build
    secrets (`GitHub Secrets`) scoped strictly to protected branches, ensuring the key is never
    exposed on the DUT or developer endpoints. The public verification key (
    `SecParam-FW-Verification-Key`) is embedded directly in the main application text segment.
* **Verdict**: **PASS**

---

## Summary Matrix for Test Case 5.3-7-1

| Mechanism ID    | Delivery Medium              | Unit A (Guarantees) | Unit B (Mechanism Design) |    Unit C (Best Practice Catalogue)     | Unit D (Feasible Attack Assessment) | Case Verdict |
|:----------------|:-----------------------------|:-------------------:|:-------------------------:|:---------------------------------------:|:-----------------------------------:|:------------:|
| `UpdMech-WebUI` | Network (HTTPS / Port 443)   |      **PASS**       |         **PASS**          | **PASS** (RSA-3072-PSS / SHA-256 / TLS) |   **PASS** (No feasible attacks)    |   **PASS**   |
| `UpdMech-Auto`  | Network (HTTPS / Port 443)   |      **PASS**       |         **PASS**          | **PASS** (RSA-3072-PSS / SHA-256 / TLS) |   **PASS** (No feasible attacks)    |   **PASS**   |
| `UpdMech-USB`   | Local Port (USB-UART Bridge) |      **PASS**       |         **PASS**          |    **PASS** (RSA-3072-PSS / SHA-256)    |   **PASS** (No feasible attacks)    |   **PASS**   |

---

## Group Summary

The Ruuvi Gateway complies fully with Provision 5.3-7 of `ETSI EN 303 645`. All update mechanisms (
`UpdMech-WebUI`, `UpdMech-Auto`, `UpdMech-USB`) employ best practice cryptography (RSA-3072 with
RSA-PSS padding, SHA-256 hashing, and TLS 1.2/1.3 transport security) as recognized in SOGIS, NIST,
and ETSI reference catalogues. The cryptographic design ensures end-to-end integrity and
authenticity without vulnerability to feasible cryptanalytic attacks.

**Group Verdict**: **PASS**
