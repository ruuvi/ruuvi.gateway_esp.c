# Test group 5.13-1A: Application-Layer Input Data Validation via User Interfaces

Provision 5.13-1A — Status: **M**. Related IXIT: `IXIT 27-UserIntf`, `IXIT 29-InpVal`.

---

## Test case 5.13-1A-1 (conceptual)

**Purpose**: To conceptually assess whether every user interface in `IXIT 27-UserIntf` is covered by
at least one input validation method in `IXIT 29-InpVal` (`a`), and whether each validation method
effectively validates input data format, value, cardinality, and ordering to prevent system
manipulations and processing failures (`b`).

---

### Test Units Conceptual Assessment Matrix

#### Test Unit A & B: User Interface Coverage and Input Validation Effectiveness

| User Interface ID (`IXIT 27-UserIntf`)      | Mapped Input Validation Method (`IXIT 29-InpVal`)                | Application-Layer Data Input Validation Mechanics                                                                                      | Effectiveness Assessment against System Manipulation & Failures                                                                                       | Unit Verdict |
|:--------------------------------------------|:-----------------------------------------------------------------|:---------------------------------------------------------------------------------------------------------------------------------------|:------------------------------------------------------------------------------------------------------------------------------------------------------|:------------:|
| **`UserIntf-Physical-Configure-Button`**    | Hardware GPIO Timer Debounce Logic                               | Physical GPIO interrupt filtering with strict timing boundaries (short-press < 2s for hotspot; long-press ≥ 7s for NVS erase).         | **Effective.** Debounce filtering prevents mechanical switch noise or rapid button chatter from triggering unexpected reset states.                   |   **PASS**   |
| **`UserIntf-Local-Hotspot-Captive-Portal`** | `InpVal-JSON-Schema-Validation`<br>`InpVal-M2M-Token-Validation` | Captive portal onboarding forms target `POST /ruuvi.json`. Input fields pass rigid type checking (SSID strings, passkeys, IP formats). | **Effective.** Rejects malformed JSON syntax, invalid data types, or out-of-bounds string lengths with HTTP 400 Bad Request, preserving NVS settings. |   **PASS**   |
| **`UserIntf-LAN-Management-WebUI`**         | `InpVal-JSON-Schema-Validation`<br>`InpVal-M2M-Token-Validation` | LAN Web-UI configuration forms and REST API endpoints. Input fields are parsed against schema rules and alphanumeric Bearer tokens.    | **Effective.** Prevents injection attacks, buffer overflows, and unauthorized setting mutations. Out-of-bounds input returns HTTP 400/401 errors.     |   **PASS**   |

* **Conceptual Assessment Justification**:
  1. **UI Coverage (Unit a):** 100% of the user interfaces cataloged in `IXIT 27-UserIntf` are
     covered by dedicated input validation mechanisms in `IXIT 29-InpVal`.
  2. **Validation Effectiveness (Unit b):** Application-layer JSON schema validation enforces strict
     data types, format limits, and payload buffer bounds. Malformed or malicious input is rejected
     cleanly with HTTP 400/401 status codes, ensuring that active parameters in non-volatile flash (
     `nvs` and `gw_cfg_def`) remain uncorrupted.

* **Verdict**: **PASS**

---

## Test case 5.13-1A-2 (functional)

**Purpose**: To functionally verify that all user interfaces of the DUT are completely documented in
`IXIT 27-UserIntf` (`a`), and that the data input validation methods in `IXIT 29-InpVal` effectively
prevent the processing of unexpected or malicious input data (`b`).

---

### Test Units Functional Assessment Matrix

#### Test Unit A & B: Functional User Interface Audit and Fuzzing / Vulnerability Inspection

**Testing Methodology**: The test laboratory performed a complete physical and software interface
audit against user documentation (`a`), and executed automated web application vulnerability
scanning (Nikto, OWASP ZAP) and REST API payload fuzzing (Burp Suite, custom scripts sending
malformed JSON, out-of-bounds strings, and invalid data types to `POST /ruuvi.json`) (`b`).

| User Interface ID (`IXIT 27-UserIntf`)      | Functional Fuzzing / Injection Action Executed                                                                   | Observed DUT Behavior & System Resilience                                                                                                                                     | Unit Verdict |
|:--------------------------------------------|:-----------------------------------------------------------------------------------------------------------------|:------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|:------------:|
| **`UserIntf-Physical-Configure-Button`**    | Trigger rapid mechanical button chatter, electrical noise spikes, and transient 4-second button holds.           | The DUT filters mechanical chatter. Transient holds (< 7s) do not trigger NVS formatting; only holds ≥ 7s invoke clean system restart.                                        |   **PASS**   |
| **`UserIntf-Local-Hotspot-Captive-Portal`** | Web scanner sweep and fuzzed JSON submission (10KB+ SSIDs, special characters, string-in-integer injections).    | Web scanner confirms zero XSS, SQLi, or Command Injection vulnerabilities. Fuzzed JSON payloads return HTTP 400 Bad Request cleanly without task crashes.                     |   **PASS**   |
| **`UserIntf-LAN-Management-WebUI`**         | Submit malformed JSON schemas, missing required key fields, out-of-bounds M2M Bearer tokens, and bad IP formats. | The HTTP task layer rejects malformed JSON with HTTP 400, drops invalid Bearer tokens with HTTP 401, and logs error traces over serial. Active NVS settings remain unchanged. |   **PASS**   |

* **Functional Assessment Justification**:
  1. **UI Documentation Completeness (Unit a):** Physical mainboard inspection and software
     walkthrough confirm that all user interfaces (`Configure` button, Captive Portal, LAN Web-UI)
     are documented in `IXIT 27-UserIntf`.
  2. **Unexpected Data Processing Prevention (Unit b):** Automated web vulnerability scanning and
     intensive API payload fuzzing confirm that the DUT validates all application-layer data inputs
     effectively. Malformed inputs, out-of-bounds payloads, and type mismatches are rejected cleanly
     without system crashes, task deadlocks, or flash memory corruption.

* **Verdict**: **PASS**

---

## Summary Matrix for Test Case 5.13-1A-1 & 5.13-1A-2

| Test Case            | Purpose / Focus                      | Assessment Summary                                                                                                     | Unit Verdict |
|:---------------------|:-------------------------------------|:-----------------------------------------------------------------------------------------------------------------------|:------------:|
| **5.13-1A-1 Unit a** | User Interface Coverage Check        | 100% of user interfaces in `IXIT 27-UserIntf` are covered by input validation methods in `IXIT 29-InpVal`.             |   **PASS**   |
| **5.13-1A-1 Unit b** | Validation Effectiveness Evaluation  | JSON schema checks and Bearer token validation effectively enforce format, type, and buffer bounds.                    |   **PASS**   |
| **5.13-1A-2 Unit a** | Interface Documentation Completeness | Physical and software interface audit confirms all user interfaces are completely documented in IXIT.                  |   **PASS**   |
| **5.13-1A-2 Unit b** | Functional Fuzzing & Scan Inspection | Web vulnerability scanning and API payload fuzzing verify malformed inputs return HTTP 400/401 without system lockups. |   **PASS**   |

---

## Group Summary

The Ruuvi Gateway complies fully with Mandatory Provision 5.13-1A of `ETSI EN 303 645`. All
application-layer user interfaces cataloged in `IXIT 27-UserIntf` (the physical `CONFIGURE` button,
Captive Portal onboarding wizard, and LAN management Web-UI) enforce robust data input validation
mechanisms (`IXIT 29-InpVal`). Configuration payloads submitted via `POST /ruuvi.json` are subjected
to strict JSON schema validation, type checking, and string length limits. Automated web application
security scanning and REST API payload fuzzing verify that unexpected, malformed, or out-of-bounds
input data is rejected with explicit HTTP 400 Bad Request or HTTP 401 Unauthorized status codes,
preventing system manipulations, task crashes, and memory corruption exploits.

**Group Verdict**: **PASS**
