# Test group 5.3-13: Defined Support Period for Software Updates Is Published

Provision 5.3-13 — Status: **M**. Related IXIT: `IXIT 2-UserInfo`, `IXIT 6-SoftComp`.

---

## Test case 5.3-13-1 (conceptual)

**Purpose**: To conceptually assess whether the published location and language defining the
software update support period (`IXIT 2-UserInfo`) is understandable and comprehensible for a
consumer with limited technical knowledge (per Clause D.3).

---

### Test Unit A: Assessment of Understandability and Accessibility

**Testing Methodology**: The test laboratory evaluated the accessibility and clarity of the
published support period resource (`https://ruuvi.com/terms/lifecycle-promises/`) referenced in
`IXIT 2-UserInfo`.

| Evaluation Criterion (ETSI TS 103 701)    | Declared Resource & Implementation Details (`IXIT 2-UserInfo`)           | Compliance & Audit Assessment                                                                                                                                                                                                                                     | Unit Verdict |
|:------------------------------------------|:-------------------------------------------------------------------------|:------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|:------------:|
| **Understandability & Comprehensibility** | Target Publication Vector: `https://ruuvi.com/terms/lifecycle-promises/` | **DOCUMENTATION GAP IDENTIFIED.** The manufacturer maintains a public lifecycle terms page (`https://ruuvi.com/terms/lifecycle-promises/`), but the **Ruuvi Gateway** model is not currently listed or covered by defined support period statements on that page. |   **FAIL**   |
| **Search Engine / Model Finding**         | Product model designation: "Ruuvi Gateway"                               | Non-technical consumers searching for the model name alongside lifecycle keywords cannot currently find a binding support period commitment for the gateway hardware.                                                                                             |   **FAIL**   |

* **Unit A Assessment Justification**: Because the Ruuvi Gateway is not yet explicitly listed with a
  defined support period on the manufacturer's lifecycle promises page, the publication is currently
  incomplete and incomprehensible for end consumers regarding this specific device.
* **Unit A Verdict**: **FAIL (Pre-Submission Documentation Blocker)**

---

## Test case 5.3-13-2 (functional)

**Purpose**: To functionally check whether user information providing access to the published
support period is available as described (`a`), whether the resource is accessible without
restriction (`b`), and whether the published support period actually defines the software update
support duration for all updatable components (`c`).

---

### Test Units Functional Assessment Matrix

| Test Unit  | Purpose / Focus                                    | Verification Steps & Observed Status                                                                                                                                                                                                                                             | Unit Verdict |
|:-----------|:---------------------------------------------------|:---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|:------------:|
| **Unit a** | **Information Provided as Described**              | Evaluated `IXIT 2-UserInfo`. While the target URL (`https://ruuvi.com/terms/lifecycle-promises/`) is declared, the page content omits the Ruuvi Gateway product entry.                                                                                                           |   **FAIL**   |
| **Unit b** | **Unrestricted Public Access**                     | The web portal `https://ruuvi.com/terms/lifecycle-promises/` is publicly accessible over standard HTTPS without user registration, paywalls, or authentication barriers.                                                                                                         |   **PASS**   |
| **Unit c** | **Support Period Defined for Software Components** | `IXIT 2-UserInfo` currently contains an unresolved placeholder (`? years`). No explicit timeframe commitment (e.g., *"5 years from product placement on market"*) is published covering updatable software components (`SoftComp-MainFW`, `SoftComp-nRF52FW`, `SoftComp-WebUI`). |   **FAIL**   |

* **Unit Assessment Justification**: The resource URL itself is publicly accessible without
  restriction (satisfying Unit `b`), but because the page omits the Ruuvi Gateway and lacks a
  binding timeframe definition (failing Units `a` and `c`), the functional test case fails.
* **Test Case 5.3-13-2 Verdict**: **FAIL (Pre-Submission Documentation Blocker)**

---

## Required Action Items Prior to Formal Submission

To achieve a **PASS** verdict for Provision 5.3-13, the following steps must be completed before
submitting the Technical File to the Testing Laboratory (TL):

1. **Update Corporate Lifecycle Page:**

* Publish an explicit lifecycle commitment for the Ruuvi Gateway on
  `https://ruuvi.com/terms/lifecycle-promises/`.
* *Example text:* *"Ruuvi Gateway software components receive security updates and vulnerability
  maintenance for a minimum of 5 years following the date the product model is placed on the
  market."*

2. **Update `IXIT 2-UserInfo` Declarations:**

* Remove the documentation gap warning note and update the placeholder `? years` string to match the
  published timeframe value.

---

## Group Summary

The Ruuvi Gateway currently fails Provision 5.3-13 of `ETSI EN 303 645` due to an open documentation
gap in `IXIT 2-UserInfo`. While the designated publication URL (
`https://ruuvi.com/terms/lifecycle-promises/`) is publicly accessible, it does not currently publish
a binding software update support period for the Ruuvi Gateway. This mandatory requirement must be
resolved prior to formal compliance certification.

**Group Verdict**: **FAIL (Mandatory Documentation Gap — Must Resolve Before Submission)**
