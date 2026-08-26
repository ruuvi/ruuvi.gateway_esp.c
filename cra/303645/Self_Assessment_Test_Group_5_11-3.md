# Test group 5.11-3: Clear and Concise Instructions for Deleting User Data

Provision 5.11-3 — Status: **R**. Related IXIT: `IXIT 2-UserInfo`, `IXIT 21-PersData`,
`IXIT 25-DelFunc`.

---

## Test case 5.11-3-1 (functional)

**Purpose**: To functionally assess whether the user documentation in `IXIT 2-UserInfo` covers all
deletion functionalities declared in `IXIT 25-DelFunc` (`b`), and whether following the documented
instructions successfully and concisely deletes personal data stored on the DUT and associated
services (`a`, `c`).

---

### Test Units Functional Assessment Matrix

#### Test Unit A, B & C: Functional Assessment of Deletion Documentation Coverage, Conciseness, and Step Completion

**Testing Methodology**: The test laboratory created typical personal data on the DUT and Ruuvi
Cloud (`a`), verified that `IXIT 2-UserInfo` covers all deletion mechanisms in `IXIT 25-DelFunc` (
`b`), and executed each deletion procedure strictly according to the published user documentation to
assess conciseness, accuracy, and step completeness (`c`).

| Deletion Functionality ID (`IXIT 25-DelFunc`) | Target Data & Storage Location                    | Documentation Coverage in `IXIT 2-UserInfo` (Unit b)                                                                                                      | Documented Steps & Execution Accuracy Evaluation (Unit c)                                                                                                                                                                                                     | Unit Verdict |
|:----------------------------------------------|:--------------------------------------------------|:----------------------------------------------------------------------------------------------------------------------------------------------------------|:--------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|:------------:|
| **`DelFunc-Hardware-Factory-Reset`**          | On-Device Flash Partitions (`nvs` / `gw_cfg_def`) | **Fully Covered.** Documented under `Documentation of Deletion` in `IXIT 2-UserInfo` with access vector `https://docs.ruuvi.com/ruuvi-gateway-firmware/`. | **Concise & Accurate.** Instructions direct the user to press and hold the physical `CONFIGURE` button for 7 seconds or longer. Executing this step cleanly formats `nvs` and `gw_cfg_def` partitions, drops station links, and activates the setup hotspot.  |   **PASS**   |
| **`DelFunc-Service-Account-Deletion`**        | Associated Service (Ruuvi Cloud Infrastructure)   | **Fully Covered.** Documented under `Documentation of Deletion` in `IXIT 2-UserInfo` with access vector `https://ruuvi.com/privacy/`.                     | **Concise & Accurate.** Instructions direct the user to request account deletion in the Ruuvi Cloud portal or Ruuvi Station app and confirm via an emailed link. Executing these steps logs out sessions, revokes access, and purges cloud PII and telemetry. |   **PASS**   |

* **Functional Assessment Justification**:
  1. **Documentation Coverage (Unit b):** Every deletion functionality cataloged in
     `IXIT 25-DelFunc` is explicitly covered in the published user documentation referenced in
     `IXIT 2-UserInfo`.
  2. **Conciseness and Step Completeness (Unit c):** Both deletion procedures are described
     concisely without unnecessary technical jargon and include all necessary steps. Following the
     documented instructions step-by-step results in 100% data deletion from both on-device flash
     storage and associated cloud infrastructure.

* **Verdict**: **PASS**

---

## Summary Matrix for Test Case 5.11-3-1

| Test Case           | Purpose / Focus                          | Assessment Summary                                                                                                  | Unit Verdict |
|:--------------------|:-----------------------------------------|:--------------------------------------------------------------------------------------------------------------------|:------------:|
| **5.11-3-1 Unit a** | Typical Personal Data Creation           | Personal data created on DUT (Wi-Fi, passwords, tokens, SSL keys) and Ruuvi Cloud (account profile, telemetry).     |   **PASS**   |
| **5.11-3-1 Unit b** | Documentation Coverage Check             | `IXIT 2-UserInfo` (`Documentation of Deletion`) covers 100% of deletion functionalities in `IXIT 25-DelFunc`.       |   **PASS**   |
| **5.11-3-1 Unit c** | Instruction Conciseness & Accuracy Check | Executing documented steps for hardware reset and cloud account deletion cleanly purges all targeted personal data. |   **PASS**   |

---

## Group Summary

The Ruuvi Gateway complies fully with Recommendation Provision 5.11-3 of `ETSI EN 303 645`. The user
documentation (`IXIT 2-UserInfo`) provides clear, accurate, and concise instructions for deleting
personal data from both the device (`DelFunc-Hardware-Factory-Reset`) and associated cloud
services (`DelFunc-Service-Account-Deletion`). Functional testing confirms that all deletion
mechanisms are fully covered in the published user documentation (`docs.ruuvi.com` and
`ruuvi.com/privacy/`), and following the documented instructions step-by-step results in complete
data eradication.

**Group Verdict**: **PASS**
