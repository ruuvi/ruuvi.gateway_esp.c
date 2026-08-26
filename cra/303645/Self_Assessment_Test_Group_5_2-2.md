# Test group 5.2-2: Vulnerabilities are Acted on in a Timely Manner

Provision 5.2-2 — Status: **R**. Related IXIT: `IXIT 3-VulnTypes`, `IXIT 2-UserInfo`, `IXIT 4-Conf`.

---

## Test case 5.2-2-1 (conceptual)

**Purpose**: To conceptually assess whether the action workflows and designated time frames
established for identified vulnerability classes facilitate prompt mitigation under consideration of
the published vulnerability disclosure policy, and to verify the presence of formal implementation
confirmations.

### Test Unit A: Vulnerability Action and Timeline Assessment

**Evaluation Criteria**: The evaluation laboratory analyzed the alignment between the corporate
triage loops managed by the Security Incident Team (SIT) and the Software Development Department (
SDD) against the public commitments posted on the manufacturer's VDP portal.

| Vulnerability Class ID | Scope / Target Subsystem   | Stated Action Loop & Coordination                |      Resolution Time Frame       | Unit Verdict |
|:-----------------------|:---------------------------|:-------------------------------------------------|:--------------------------------:|:------------:|
| `RuuviGw-Vuln1`        | Web-UI & Frontend Node.js  | SDD dependency audits and package updates        |             90 Days              |   **PASS**   |
| `RuuviGw-Vuln2`        | ESP32 Core Firmware        | Upstream coordination with Espressif Systems     |             90 Days              |   **PASS**   |
| `RuuviGw-Vuln3`        | nRF52 BLE Stack            | Internal Nordic nRF5 SDK binary replacement      |             90 Days              |   **PASS**   |
| `RuuviGw-Vuln4`        | Network (Wi-Fi/IP Stack)   | LwIP mitigation and Espressif upstream fixes     |             90 Days              |   **PASS**   |
| `RuuviGw-Vuln5`        | Cryptography (mbedTLS)     | High-Priority expedited CI/CD hotfix track       |             90 Days              |   **PASS**   |
| `RuuviGw-Vuln6`        | Hardware & Physical Layout | Physical access mitigation / eFuse debug lockout | 90 Days<br>(Advisory inside 30d) |   **PASS**   |

**Assessment Justification**:

* **VDP Alignment:** The initial triage response milestones align with the public policy disclosed
  in `IXIT 2-UserInfo`. The 3-business-day intake window easily satisfies the maximum 7-day initial
  processing boundary documented across all classes in `IXIT 3-VulnTypes`.
* **Third-Party Collaboration:** Responsibilities for upstream vendor interactions (Espressif
  Systems and Nordic Semiconductor) are clearly defined. The SDD maintains explicit protocols to
  implement local "sanitization" software layers if vendor-tier patches experience deployment
  delays, ensuring device security is maintained.
* **Remediation Timelines:** The 90-day maximum lifecycle resolution constraint matches modern IoT
  industry baselines and provides sufficient time for firmware validation, continuous integration (
  CI) regression runs, and RSA-3072 signature generation blocks before Over-the-Air (OTA) deployment
  via `IXIT 7-UpdMech`.

---

### Test Unit B: Verification of Precondition Confirmations

| Compliance Prerequisite                   | Checked Document Target   | Stated Status Parameter | Alignment Verdict |
|:------------------------------------------|:--------------------------|:-----------------------:|:-----------------:|
| **Confirmation of Vulnerability Actions** | `IXIT 4-Conf` (Section 1) |         **Yes**         |     **PASS**      |

**Assessment**: Direct cross-referencing of the compliance registry confirms that `IXIT 4-Conf`
contains an explicit, unambiguous "Yes" declaration for the item **Confirmation of Vulnerability
Actions**. The file confirms that the required corporate infrastructure is deployed and that active
engineers are briefed on the resolution milestones.

**Verdict**: **PASS**

---

## Group Summary

The Ruuvi Gateway complies fully with Provision 5.2-2 of ETSI EN 303 645. The technical
implementation documentation maps out response workflows for all relevant vulnerability categories (
`IXIT 3-VulnTypes`). These internal engineering timelines align with the public commitments declared
under `IXIT 2-UserInfo`, and the operational mechanisms are formally confirmed as established within
`IXIT 4-Conf`.

**Group Verdict**: **PASS**
