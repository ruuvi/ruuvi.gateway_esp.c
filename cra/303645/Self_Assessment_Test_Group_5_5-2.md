# Test group 5.5-2: Network and Security Functionalities Are Reviewed or Evaluated

Provision 5.5-2 — Status: **R**. Related IXIT: `IXIT 12-NetSecImpl`.

---

## Test case 5.5-2-1 (conceptual)

**Purpose**: To conceptually assess whether all network and security implementations defined in
`IXIT 12-NetSecImpl` have been reviewed or evaluated (`a`), and whether the review/evaluation
methods and reports cover the full functional scope of each implementation (`b`).

---

### Test Units Conceptual Assessment Matrix

#### Test Unit A: Assessment of Review/Evaluation Methods

* **Requirement**: For each implementation in `IXIT 12-NetSecImpl`, verify that an appropriate
  review or evaluation method is declared.
* **Evaluation**: All four network and security implementations in `IXIT 12-NetSecImpl` declare
  explicit review and evaluation methods:
  * `NetSecImpl-Framework-Stack`: Automated SAST (SonarCloud), compiler analysis, CI
    test suite, and mandatory senior developer PR review.
  * `NetSecImpl-Crypto-Library`: AddressSanitizer (`-fsanitize=address`) memory corruption sweeps
    and automated mbedTLS SSL test suites (`test_suite_ssl_pre_allocated_buffers`).
  * `NetSecImpl-Web-Application-Core`: Dual-senior developer manual peer review and JavaScript
    static analysis.
  * `NetSecImpl-CoProcessor-Firmware`: Manual engineering code review focused on passive Rx-Only
    radio boundary isolation.
* **Verdict**: **PASS**

#### Test Unit B: Coverage of Implementation Scope in Reports

* **Requirement**: Assess whether the review/evaluation method and its associated report cover the
  full functional scope described in "Description" for each implementation.
* **Evaluation**:

| Implementation ID (`IXIT 12-NetSecImpl`) | Component & Version Scope                         | Review/Evaluation Method & Scope Coverage Assessment                                                                                                                                                                                                                                | Unit Verdict |
|:-----------------------------------------|:--------------------------------------------------|:------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|:------------:|
| `NetSecImpl-Framework-Stack`             | ESP-IDF v4.2.5 (lwIP / FreeRTOS)                  | **Full Stack Coverage.** CI test suites and SonarCloud SAST cover L2/L4 sockets, MQTT telemetry engines, HTTP/HTTPS pipelines, and NVS storage drivers. Mandatory senior developer PR approvals gate all code merges.                                                               |   **PASS**   |
| `NetSecImpl-Crypto-Library`              | mbedTLS (Custom Backport / Pre-Allocated Buffers) | **Memory Safety & Cipher Coverage.** GitHub Actions pipeline (`test-mbedtls`) compiles code with `-fsanitize=address` and runs custom SSL test scripts (`run-test_suite_ssl-sanitize.sh`, `run-test_suite_ssl-reduced_buffer_size.sh`) to detect pointer leaks or buffer overflows. |   **PASS**   |
| `NetSecImpl-Web-Application-Core`        | Web-UI (`auth.mjs` / `crypto.mjs`)                | **Session & Crypto Scope.** Dual-engineer peer review evaluated the ECDH key agreement (`Ruuvi-Ecdh-Pub-Key`), AES-CBC UI payload encryption, and nonced MD5 password hashing in `ruuvi.gwui.html`.                                                                                 |   **PASS**   |
| `NetSecImpl-CoProcessor-Firmware`        | nRF5 SDK v15.3.0 (nRF52811)                       | **Radio Isolation Scope.** Engineering code review verified that the BLE scanning engine operates strictly as a connectionless, Rx-Only passive listener, ignoring incoming pairing requests.                                                                                       |   **PASS**   |

* **Verdict**: **PASS**

---

## Test case 5.5-2-2 (functional)

**Purpose**: To functionally check whether the identification (name and version) of each network and
security implementation on the DUT matches the identification specified in `IXIT 12-NetSecImpl` and
its associated review reports.

---

### Test Unit A: Functional Identification Verification

**Testing Methodology**: The test laboratory inspected build logs, system diagnostic status
telemetry (`/status`), UART boot console outputs, and public GitHub repository release tags to
cross-check component names and version strings.

| Implementation ID (`IXIT 12-NetSecImpl`) | Expected Name & Version    | Observed DUT / CI Report Identification                                                                                                      | Unit Verdict |
|:-----------------------------------------|:---------------------------|:---------------------------------------------------------------------------------------------------------------------------------------------|:------------:|
| `NetSecImpl-Framework-Stack`             | ESP-IDF v4.2.5             | UART boot log and build manifest confirm `ESP-IDF v4.2.5`, matching the SonarCloud and CI build test reports in `ruuvi/ruuvi.gateway_esp.c`. |   **PASS**   |
| `NetSecImpl-Crypto-Library`              | mbedTLS (Custom Backport)  | Runtime TLS stack identifies as mbedTLS with custom backported TLS 1.3 patches, matching the `test-mbedtls` workflow logs.                   |   **PASS**   |
| `NetSecImpl-Web-Application-Core`        | Web-UI (`ruuvi.gwui.html`) | Local Web-UI bundle version string and `crypto.mjs` payload structures match the audited release tags in `ruuvi/ruuvi.gwui.html`.            |   **PASS**   |
| `NetSecImpl-CoProcessor-Firmware`        | nRF5 SDK v15.3.0           | nRF52 co-processor firmware header identifies build matching `ruuvi/ruuvi.gateway_nrf.c` release artifacts.                                  |   **PASS**   |

**Assessment Justification**: Functional inspection confirms that all component names and version
strings reported by the DUT match the technical declarations in `IXIT 12-NetSecImpl` and the
corresponding CI/audit report histories.

**Verdict**: **PASS**

---

## Summary Matrix for Test Case 5.5-2-1 & 5.5-2-2

| Test Case          | Purpose / Focus               | Assessment Summary                                                                                       | Verdict  |
|:-------------------|:------------------------------|:---------------------------------------------------------------------------------------------------------|:--------:|
| **5.5-2-1 Unit a** | Assessment of Review Methods  | Every network/security implementation has a defined review method (SAST, AddressSanitizer, peer review). | **PASS** |
| **5.5-2-1 Unit b** | Implementation Scope Coverage | Review methods cover full operational scopes (ESP-IDF, mbedTLS crypto, Web-UI crypto, nRF52 BLE).        | **PASS** |
| **5.5-2-2 Unit a** | Functional Version Matching   | DUT runtime versions (ESP-IDF v4.2.5, nRF5 SDK v15.3.0) match the CI and audit report identifications.   | **PASS** |

---

## Group Summary

The Ruuvi Gateway complies fully with Recommendation Provision 5.5-2 of `ETSI EN 303 645`. All
network and security implementations (`IXIT 12-NetSecImpl`) undergo systematic reviews and
evaluations—including automated SAST (SonarCloud), AddressSanitizer (`-fsanitize=address`) memory
testing for custom mbedTLS buffers, senior developer peer reviews, and CI regression suites.
Functional verification confirms that the component names and versions running on the DUT match
those documented in the technical file and test reports.

**Group Verdict**: **PASS**
