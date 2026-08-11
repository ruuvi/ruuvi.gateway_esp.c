# Test group 5.3-8: Security Updates Are Deployed in a Timely Manner

Provision 5.3-8 — Status: **M C (12)**. Related IXIT: `IXIT 8-UpdProc`, `IXIT 4-Conf`.

---

## Test case 5.3-8-1 (conceptual)

**Purpose**: To conceptually assess whether the management procedures defined in `IXIT 8-UpdProc`
facilitate the timely deployment of security updates (`a`), and to verify that formal operational
confirmation is declared in `IXIT 4-Conf` (`b`).

---

### Test Unit A: Assessment of Update Deployment Management Procedures & Timeframes

**Testing Methodology**: The test laboratory evaluated the management procedures, organizational
responsibilities, timeframes, and procedural safeguards declared in `IXIT 8-UpdProc` against the
timely deployment criteria in `ETSI TS 103 701` Section 5.3.8.1.

| Evaluation Criterion (`ETSI TS 103 701`)  | Declared Management & Implementation Details (`IXIT 8-UpdProc`)                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                  | Assessment & Timeliness Justification                                                                                                                                                                 | Criterion Verdict |
|:------------------------------------------|:---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|:------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|:-----------------:|
| **1. Process Steps & Process Flow**       | **6-Stage Managed Lifecycle:**<br>1. *Initiation:* Triage and ticket creation in GitHub Issues.<br>2. *Development:* Patch development, ESP-IDF/nRF5 SDK backporting, and dependency updates.<br>3. *Verification:* QA exploitation simulation and baseline regression loops.<br>4. *Signing:* Automated RSA-3072-PSS image signing in ephemeral CI/CD environments.<br>5. *Deployment:* Staging to `https://fwupdate.ruuvi.com` and index update at `https://network.ruuvi.com/firmwareupdate`.<br>6. *Communication:* Publication of security advisory and detailed changelog. | Well-defined, end-to-end procedural workflow minimizes operational friction and ensures systematic progress from vulnerability intake to global patch availability.                                   |     **PASS**      |
| **2. Roles & Responsibilities**           | **Clear Division of Duties:**<br>* **Security Incident Team (SIT):** Intake management, risk scoring, and vendor coordination.<br>* **Software Development Department (SDD):** Patch engineering, RSA signing, and server deployment.<br>* **QA / Testing Group:** Exploitation simulation and regression testing.                                                                                                                                                                                                                                                               | Explicit role definitions eliminate organizational ambiguity and streamline cross-departmental collaboration during security incidents.                                                               |     **PASS**      |
| **3. Third-Party & Vendor Collaboration** | Continuous tracking and backporting procedures for upstream dependencies, including Espressif Systems (ESP-IDF), Nordic Semiconductor (nRF5 SDK), and mbedTLS repositories.                                                                                                                                                                                                                                                                                                                                                                                                      | Established channels for monitoring and integrating third-party upstream security patches ensure rapid response to library-level vulnerabilities.                                                     |     **PASS**      |
| **4. Defined Response Timeframes**        | **Target Response Windows:**<br>* *Standard Security Updates:* Maximum **90 days** from initial tracking to server deployment.<br>* *Critical / Emergency Fixes:* Maximum **90 days** for expedited engineering, testing, and automated rollout.                                                                                                                                                                                                                                                                                                                                 | A 90-day maximum window from vulnerability confirmation to global deployment aligns with established industry best practices (e.g., CERT/CC, Google Project Zero baselines) for consumer IoT devices. |     **PASS**      |
| **5. Safeguards & Staged Rollout**        | Automated CI/CD pipelines with integrated static analysis (SonarCloud). Beta track deployment (`https://github.com/ruuvi/ruuvi.gateway_esp.c/releases/download/`) permits field stability validation prior to general release.                                                                                                                                                                                                                                                                                                                                                   | Automated build/signing pipelines and staged beta tracks prevent regression delays and accelerate stable patch releases.                                                                              |     **PASS**      |

* **Unit A Assessment Justification**: The management procedures in `IXIT 8-UpdProc` establish a
  clear, structured, and accountable process flow with defined 90-day maximum timeframes, robust
  CI/CD automation, and clear departmental responsibilities, fully facilitating timely deployment of
  security updates.
* **Unit A Verdict**: **PASS**

---

### Test Unit B: Confirmation of Implementation

**Testing Methodology**: The test laboratory checked `IXIT 4-Conf` ("Confirmation of Update
Procedures") to verify that formal management confirmation is declared.

| Confirmation Requirement (`IXIT 4-Conf`) | Declared Status | Confirmation Scope & Text Verification                                                                                                                                                                                                                                                                                                       | Unit Verdict |
|:-----------------------------------------|:---------------:|:---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|:------------:|
| **Confirmation of Update Procedures**    |     **Yes**     | Formally confirms that for every software update procedure declared in `IXIT 8-UpdProc`, the required corporate infrastructure is active, operators are briefed, branch-scoped release secrets are configured, and server deployment channels (`https://network.ruuvi.com/firmwareupdate` and `https://fwupdate.ruuvi.com`) are operational. |   **PASS**   |

* **Unit B Assessment Justification**: `IXIT 4-Conf` explicitly states "Yes" for "Confirmation of
  Update Procedures" and confirms that all necessary infrastructure and briefed personnel are in
  place to execute the update procedures within designated timeframes.
* **Unit B Verdict**: **PASS**

---

## Summary Matrix for Test Case 5.3-8-1

| Test Unit  | Purpose / Focus                                                         | Reference Document | Unit Verdict |
|:-----------|:------------------------------------------------------------------------|:-------------------|:------------:|
| **Unit A** | Assessment of update management procedure, roles, and 90-day timeframes | `IXIT 8-UpdProc`   |   **PASS**   |
| **Unit B** | Verification of formal management confirmation                          | `IXIT 4-Conf`      |   **PASS**   |

---

## Group Summary

The Ruuvi Gateway complies fully with Provision 5.3-8 of `ETSI EN 303 645`. The manufacturer has
established a structured 6-stage security update lifecycle (`IXIT 8-UpdProc`) with clear
departmental responsibilities (SIT, SDD, QA), automated CI/CD build and signing pipelines, and
defined 90-day response timeframes. Operational deployment readiness is formally confirmed in
`IXIT 4-Conf`.

**Group Verdict**: **PASS**
