# Test group 5.3-10: Trust Relationship is Validated for Network-Based Updates

Provision 5.3-10 — Status: **M F (j)**. Related IXIT: `IXIT 7-UpdMech`.

---

## Test case 5.3-10-1 (conceptual/functional)

**Purpose**: To conceptually assess whether network-based update mechanisms in `IXIT 7-UpdMech` verify software updates via a valid trust relationship (`a` & `b`), and to functionally verify that all network-based update mechanisms present on the DUT are completely documented in `IXIT 7-UpdMech` (`c`).

---

### Test Units Assessment Matrix

#### Test Unit A: Verification of Authenticity & Integrity Baseline
* **Requirement**: Application of Test Units `a` and `b` from Test Case 5.3-9-1 across all update mechanisms.
* **Evaluation**: All update mechanisms (`UpdMech-WebUI`, `UpdMech-Auto`, `UpdMech-USB`) employ RSA-3072-PSS digital signatures over SHA-256 digests. The DUT itself validates payload authenticity and structural integrity on-device prior to partition execution, ensuring modified or corrupted binaries are rejected.
* **Verdict**: **PASS**

#### Test Unit B: Trust Relationship Validation for Network-Based Update Mechanisms
* **Requirement**: For each network-based update mechanism (`UpdMech-WebUI` and `UpdMech-Auto`), the verification of integrity and authenticity must rely on a valid trust relationship.
* **Evaluation**:

| Network-Based Update Mechanism | Trust Relationship Anchors Implemented | Role Isolation & Administrative Controls | Trust Relationship Assessment | Unit Verdict |
| :--- | :--- | :--- | :--- | :---: |
| `UpdMech-WebUI` | 1. **Authenticated Transport Channel:** Outbound TLS 1.2/1.3 session to `https://network.ruuvi.com/firmwareupdate` and `https://fwupdate.ruuvi.com`.<br>2. **Digital Signature Verification:** RSA-3072-PSS public key (`SecParam-FW-Verification-Key`) embedded in main application text segment.<br>3. **User Authorization:** Operator explicitly authenticates into local Web-UI and initiates the update action. | **Strict Infrastructure Role Separation:** Binary deployment to `https://fwupdate.ruuvi.com/` and JSON index publishing on `https://network.ruuvi.com/firmwareupdate` are restricted exclusively to authorized administrative infrastructure custodians. Standard firmware developers cannot publish releases, preventing unauthorized update injection. | Valid multi-tier trust relationship anchored by HTTPS transport security, embedded RSA-3072-PSS verification, and administrative infrastructure access controls. | **PASS** |
| `UpdMech-Auto` | 1. **Authenticated Transport Channel:** Outbound TLS 1.2/1.3 connection to production update servers.<br>2. **Digital Signature Verification:** RSA-3072-PSS verification over `ruuvi_gateway_esp.bin`, auxiliary filesystems (`fatfs_gwui`/`fatfs_nrf52`), and boot SWD nRF52 RAM digests. | **Infrastructure Access Control:** Release channels are cryptographically signed using isolated CI/CD secrets (`GitHub Secrets`) scoped strictly to protected release branches and deployed exclusively by authorized administrative personnel. | Valid multi-tier trust relationship anchored by TLS channel security, embedded public-key validation, and administrative release gating. | **PASS** |

* **Verdict**: **PASS**

#### Test Unit C: Completeness of IXIT Documentation (Functional Network Scan)
* **Requirement**: Assess whether any network-based update mechanisms not documented in `IXIT 7-UpdMech` are available via network interfaces on the DUT.
* **Testing Methodology**: The test laboratory executed network traffic monitoring, port scanning (`nmap`), and dynamic packet analysis tools (Wireshark) during system boot, routine operation, and administrative Web-UI sessions.
* **Evaluation**: Network interface analysis confirmed that the DUT exposes no undocumented or hidden update channels, backdoors, unencrypted HTTP update listeners, or proprietary network management protocols. All network update traffic is strictly restricted to the documented HTTPS endpoints declared under `UpdMech-WebUI` and `UpdMech-Auto` in `IXIT 7-UpdMech`.
* **Verdict**: **PASS**

---

## Summary Matrix for Test Case 5.3-10-1

| Test Unit | Purpose / Focus | Security Mechanism & Verification Strategy | Unit Verdict |
| :--- | :--- | :--- | :---: |
| **Unit A** | Authenticity & Integrity Baseline | RSA-3072-PSS / SHA-256 on-device signature verification (per Test Group 5.3-9). | **PASS** |
| **Unit B** | Trust Relationship Validation | Multi-tier trust anchored by TLS transport, embedded RSA public key, and administrative role isolation on release servers. | **PASS** |
| **Unit C** | Documentation Completeness | Functional network scanning confirmed zero undocumented network update mechanisms. | **PASS** |

---

## Group Summary

The Ruuvi Gateway complies fully with Provision 5.3-10 of `ETSI EN 303 645`. All network-based update mechanisms (`UpdMech-WebUI` and `UpdMech-Auto`) rely on a valid trust relationship combining authenticated TLS transport channels, embedded RSA-3072-PSS public-key verification, and strict administrative role separation governing release infrastructure deployment. Functional network analysis confirmed that all network update mechanisms on the DUT are fully documented in `IXIT 7-UpdMech`.

**Group Verdict**: **PASS**
