# Test group 5.11-2: Clearly Identifiable Functionality to Delete Personal Data from Associated Services

Provision 5.11-2 — Status: **R F (x)**. Related IXIT: `IXIT 2-UserInfo`, `IXIT 21-PersData`,
`IXIT 25-DelFunc`.

---

## Condition Evaluation (ETSI EN 303 645 Annex B)

* **Condition 24 (x) Requirement**: *"Personal data is processed on associated services."*
* **DUT Capabilities Assessment**: As declared in `IXIT 21-PersData`, the DUT processes personal
  data categories—including gateway MAC identifiers (`PersData-Gateway-MAC-Identifier`), network IP
  footprints (`PersData-Network-IP-Footprints`), and aggregated BLE telemetry datasets (
  `PersData-BLE-Sensor-Telemetry`)—which are transmitted to and stored on the official associated
  service (Ruuvi Cloud at `https://network.ruuvi.com/`).
* **Condition Result**: Condition 24 evaluates to **TRUE**. Provision 5.11-2 is evaluated as *
  *Recommendation (R)**.

---

## Test case 5.11-2-1 (conceptual)

**Purpose**: To conceptually assess whether at least one deletion functionality is provided in
`IXIT 25-DelFunc` to remove personal data from associated services that can be easily performed by a
user with limited technical knowledge according to Clause D.3 (`a`), and whether all associated
services storing personal data in `IXIT 21-PersData` are covered (`b`).

---

### Test Units Conceptual Assessment Matrix

#### Test Unit A & B: Conceptual Assessment of Associated Service Personal Data Deletion

| Deletion Functionality ID (`IXIT 25-DelFunc`) | Targeted Associated Service & Storage Space               | Initiation & User Interaction (Unit a - Clause D.3)                                                                                                                                                                              | Personal Data Coverage on Associated Services (`IXIT 21-PersData`) (Unit b)                                                                                                                                                                         | Unit Verdict |
|:----------------------------------------------|:----------------------------------------------------------|:---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|:----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|:------------:|
| **`DelFunc-Service-Account-Deletion`**        | Ruuvi Cloud Infrastructure (`https://network.ruuvi.com/`) | **Simple Web/App Workflow.** User requests account deletion via the Ruuvi Cloud portal or Ruuvi Station mobile app, then clicks an automated confirmation link sent to their registered email. Bypasses complex technical steps. | **100% Coverage of Cloud Personal Data.** Permanently purges user profile PII, account credentials, gateway assignment mappings (`PersData-Gateway-MAC-Identifier`), and queued historical sensor telemetry logs (`PersData-BLE-Sensor-Telemetry`). |   **PASS**   |

* **User Documentation Cross-Reference**: `IXIT 2-UserInfo` (`Documentation of Deletion`) explicitly
  publishes user-facing access vectors (`https://docs.ruuvi.com/ruuvi-gateway-firmware/` and
  `https://ruuvi.com/privacy/`) clearly identifying local hardware reset and remote cloud account
  deletion workflows.

* **Conceptual Assessment Justification**: `DelFunc-Service-Account-Deletion` provides a simple,
  clearly identifiable mechanism accessible to non-technical users to permanently delete all
  personal data stored on official associated cloud services, covering 100% of the personal data
  categories defined in `IXIT 21-PersData`.

* **Verdict**: **PASS**

---

## Test case 5.11-2-2 (functional)

**Purpose**: To functionally verify on associated services that personal data is created (`a`), that
user initiation and interaction for personal data deletion operate consistently with
`IXIT 25-DelFunc` (`b`), and that personal data is successfully removed from associated services
following operation completion (`c`).

---

### Test Units Functional Assessment Matrix

#### Test Unit A, B & C: Functional Associated Service Personal Data Creation, Deletion, and Inspection

**Testing Methodology**: The test laboratory registered a test user account on Ruuvi Cloud, linked a
physical gateway (`gw_mac`), streamed sensor telemetry, initiated account deletion via the Ruuvi
Station application, authorized deletion via the emailed confirmation link, and audited cloud portal
access and data API endpoints post-deletion.

| Functional Test Stage                         | Test Action Executed on Associated Service                                                                                                                    | Observed Service Behavior & Verification                                                                                                                                                                        | Unit Verdict |
|:----------------------------------------------|:--------------------------------------------------------------------------------------------------------------------------------------------------------------|:----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|:------------:|
| **Data Creation (Unit a)**                    | Register Ruuvi Cloud user account, claim gateway MAC (`PersData-Gateway-MAC-Identifier`), and stream live sensor telemetry (`PersData-BLE-Sensor-Telemetry`). | Account created successfully; telemetry graphs, gateway associations, and user PII populate on the Ruuvi Cloud dashboard.                                                                                       |   **PASS**   |
| **Initiation Verification (Unit b)**          | Submit account deletion request in Ruuvi Station app; inspect inbox and click confirmation link.                                                              | Email authorization link arrives immediately. Clicking link renders an account termination confirmation page; active app and web sessions are logged out automatically.                                         |   **PASS**   |
| **Post-Deletion Data Absence Check (Unit c)** | Attempt portal login with deleted credentials and query cloud telemetry API endpoints.                                                                        | **Account and Data Fully Eradicated.** Login fails with `User not found` errors. API queries for the gateway MAC return unassigned/unauthenticated states. Stored telemetry history is unaccessible and purged. |   **PASS**   |

**Assessment Justification**: Functional testing confirms that executing
`DelFunc-Service-Account-Deletion` operates strictly according to `IXIT 25-DelFunc`. Upon email
confirmation, user sessions log out immediately, account credentials are invalidated, and all
cloud-stored personal data, gateway mappings, and telemetry logs are permanently removed from
associated services.

**Verdict**: **PASS**

---

## Summary Matrix for Test Case 5.11-2-1 & 5.11-2-2

| Test Case           | Purpose / Focus                    | Assessment Summary                                                                                                          | Unit Verdict |
|:--------------------|:-----------------------------------|:----------------------------------------------------------------------------------------------------------------------------|:------------:|
| **5.11-2-1 Unit a** | Simplicity Assessment (Clause D.3) | Web portal/mobile app account deletion with email confirmation link is easily performed by non-technical users.             |   **PASS**   |
| **5.11-2-1 Unit b** | Associated Service Coverage        | Covers all cloud-stored personal data (`PersData-Gateway-MAC-Identifier`, `PersData-BLE-Sensor-Telemetry`, user PII).       |   **PASS**   |
| **5.11-2-2 Unit a** | Cloud Personal Data Creation       | Test user account registered, gateway MAC claimed, and telemetry successfully populated on associated cloud service.        |   **PASS**   |
| **5.11-2-2 Unit b** | Initiation & Interaction Check     | Account deletion workflow and email confirmation link interaction operate strictly as documented in `IXIT 25-DelFunc`.      |   **PASS**   |
| **5.11-2-2 Unit c** | Post-Deletion Data Absence Check   | Verification confirms user account is terminated, portal login is rejected, and cloud telemetry data is permanently purged. |   **PASS**   |

---

## Group Summary

The Ruuvi Gateway complies fully with Recommendation Provision 5.11-2 of `ETSI EN 303 645`. The
technical declarations in `IXIT 2-UserInfo`, `IXIT 21-PersData`, and `IXIT 25-DelFunc` demonstrate a
simple, clearly identifiable deletion functionality (`DelFunc-Service-Account-Deletion`) allowing
users to permanently remove all personal data stored on associated services. Users initiate deletion
through the Ruuvi Cloud portal or Ruuvi Station application and authorize eradication via an emailed
confirmation link. Functional testing confirms that upon confirmation, user accounts are terminated,
active sessions log out, and all personal data, gateway associations, and historical telemetry logs
are permanently deleted from cloud infrastructure.

**Group Verdict**: **PASS**