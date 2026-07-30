# Test group 5.3-15B: Hardware Replacement Support of the DUT

Provision 5.3-15B — Status: **R**. Related IXIT: `IXIT 9-ReplSup`.

---

## Test case 5.3-15B-1 (conceptual)

**Purpose**: To conceptually assess whether the hardware replacement procedures declared in
`IXIT 9-ReplSup` are suitable for replacing the DUT or its hardware components while preserving
system security and operational integrity.

---

### Test Unit A: Suitability Assessment of the Hardware Replacement Method

**Testing Methodology**: The test laboratory evaluated the modular unit replacement steps,
configuration restoration pathways, and credential handling rules declared in `IXIT 9-ReplSup`
against the conceptual replacement criteria in ETSI TS 103 701 Section 5.3.15B.1.

| Declared Replacement Step (`IXIT 9-ReplSup`) | Mechanism & Procedure Executed                                                                                                  | Security & Operational Assessment                                                                                                          | Unit Verdict |
|:---------------------------------------------|:--------------------------------------------------------------------------------------------------------------------------------|:-------------------------------------------------------------------------------------------------------------------------------------------|:------------:|
| **1. Hardware Unit Swap**                    | Suspected or faulty unit is physically replaced with a new Ruuvi Gateway unit.                                                  | Full-unit modular replacement matches the single-board embedded hardware design of the DUT.                                                |   **PASS**   |
| **2. Configuration Restoration**             | Operator configures the replacement unit via the Web-UI setup wizard or restores a saved JSON configuration backup.             | Provides both rapid setup and automated configuration migration options without requiring specialized factory programming tools.           |   **PASS**   |
| **3. Credential & Wi-Fi Handling**           | Wi-Fi passwords are intentionally excluded from backup exports; operators manually re-enter Wi-Fi credentials post-restoration. | **Enforces Credential Security.** Prevents sensitive wireless network credentials from being stored in plaintext or portable backup files. |   **PASS**   |
| **4. Identity & Key Isolation**              | Each unit possesses a factory-unique `DEVICEID`, unique MAC address, and unique default Web-UI password.                        | Guarantees cryptographic role isolation; security credentials and keys from the replaced unit are never reused.                            |   **PASS**   |

**Assessment Justification**:

* The replacement workflow in `IXIT 9-ReplSup` provides a structured, secure, and complete method
  for swapping hardware.
* Forcing unique silicon identities (`DEVICEID`) and requiring explicit re-entry of sensitive Wi-Fi
  credentials ensures that replacing a physical gateway unit does not expose stored network
  credentials or reuse compromised keys across hardware units.

**Verdict**: **PASS**

---

## Test case 5.3-15B-2 (functional)

**Purpose**: To functionally verify that replacing a DUT with a new unit using the method in
`IXIT 9-ReplSup` successfully restores full system connectivity, BLE scanning capability, and
telemetry relaying.

---

### Test Units Functional Assessment Matrix

| Test Unit / Phase                                 | Action Executed                                                                                                                                   | Observed Functional DUT Behavior                                                                                                                                                                                                                                      | Unit Verdict |
|:--------------------------------------------------|:--------------------------------------------------------------------------------------------------------------------------------------------------|:----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|:------------:|
| **Unit a: Hardware Replacement Execution**        | Disconnect faulty DUT and deploy a new replacement Ruuvi Gateway unit. Apply configuration backup file and re-enter Wi-Fi credentials via Web-UI. | The replacement unit processes the backup file cleanly, applies interface parameters, and establishes network backhaul connectivity.                                                                                                                                  |   **PASS**   |
| **Unit b: Functional & Security Parity Regained** | Verify operational telemetry streaming, passive BLE radio scanning, and TLS channel creation on the replacement device.                           | **Full Service Restored.** The replacement unit resumes passive BLE advertisement scanning (`SoftComp-nRF52FW`), establishes secure TLS channels (HTTPS/MQTTS) using its own unique identity, and relays sensor data smoothly to target endpoints (`IXIT 9-ReplSup`). |   **PASS**   |

**Assessment Justification**: Functional testing confirms that deploying a replacement Ruuvi Gateway
unit and applying settings restoration per `IXIT 9-ReplSup` successfully restores full operational
connectivity, telemetry forwarding, and BLE scanning parity under a distinct cryptographic hardware
identity.

**Verdict**: **PASS**

---

## Group Summary

The Ruuvi Gateway complies fully with Recommendation Provision 5.3-15B of `ETSI EN 303 645`. The
hardware replacement procedures declared in `IXIT 9-ReplSup` (modular unit swap, backup/restore
configuration migration, unique unit `DEVICEID` assignment, and mandatory Wi-Fi re-authentication)
provide a secure, practical replacement path. Functional testing confirms that replacing a unit
successfully restores complete system connectivity, BLE scanning, and telemetry relaying without
security compromise.

**Group Verdict**: **PASS**
