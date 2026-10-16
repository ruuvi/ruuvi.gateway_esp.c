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

| User Interface ID (`IXIT 27-UserIntf`)      | Mapped Input Validation Method (`IXIT 29-InpVal`)                | Application-Layer Data Input Validation Mechanics                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                     | Effectiveness Assessment against System Manipulation & Failures                                                                                                                                                                                    |    Unit Verdict    |
|:--------------------------------------------|:-----------------------------------------------------------------|:----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|:---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|:------------------:|
| **`UserIntf-Physical-Configure-Button`**    | `InpVal-Button-Timing-State-Validation`                          | GPIO events reach `reset_task_handle_sig_configure_button_event()`. Press starts a five-second software restart timer; release cancels it, disconnects the current network and opens the hotspot if needed. Press and release within two seconds is conservative operator guidance, not a firmware branch threshold. After restart, use a 3 seconds window to release and keep the button released; the current boot-time sample is reported at approximately 3600 ms. An explicit minimum of 3000 ms is pending in [#1657](https://github.com/ruuvi/ruuvi.gateway_esp.c/issues/1657). A held button at that sample selects partition erasure; release cannot cancel it. Hold until the red 200 ms on/off completion signal, then release to trigger another restart. | Runtime timing and boot-state checks are implemented. No timed debounce filter is implemented; presses released within 50 ms may activate the hotspot. The filter is pending in [#1658](https://github.com/ruuvi/ruuvi.gateway_esp.c/issues/1658). | **REVIEW PENDING** |
| **`UserIntf-Local-Hotspot-Captive-Portal`** | `InpVal-JSON-Schema-Validation`<br>`InpVal-M2M-Token-Validation` | Captive portal onboarding forms submit configuration payloads to `POST /ruuvi.json`. Input fields are subjected to strict JSON schema validation, type checking (SSID strings, Wi-Fi keys, static IP syntax), and string length boundary limits.                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                      | **Effective.** Rejects malformed JSON syntax, invalid data types, or out-of-bounds string lengths with HTTP 400 Bad Request, preserving non-volatile configuration memory.                                                                         |      **PASS**      |
| **`UserIntf-LAN-Management-WebUI`**         | `InpVal-JSON-Schema-Validation`<br>`InpVal-M2M-Token-Validation` | Local Web-UI configuration forms and REST API endpoints validate inbound JSON schemas, token strings, and parameter types. Administrative endpoints strictly enforce Bearer token verification.                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                       | **Effective.** Prevents injection attacks, buffer overflows, and unauthorized setting mutations. Out-of-bounds input or missing fields return HTTP 400 Bad Request or HTTP 401 Unauthorized errors.                                                |      **PASS**      |

* **Conceptual Assessment Justification**:
  1. **UI Coverage (Unit a):** 100% of user interfaces cataloged in `IXIT 27-UserIntf` (the physical
     `CONFIGURE` button, local setup hotspot captive portal, and LAN Web-UI) are mapped to dedicated
     input validation and state-handling mechanisms in `IXIT 29-InpVal`.
  2. **Validation Effectiveness (Unit b):** Runtime restart timing and boot-state sampling are
     implemented for the physical button. Timed debounce and an explicit minimum boot sampling time
     remain pending under #1658 and #1657. The network-interface assessments below retain their
     prior recorded results; this correction supplies no new fuzzing or live-DUT evidence.

* **Verdict**: **REVIEW PENDING** (physical-button controls and evidence)

---

## Test case 5.13-1A-2 (functional)

**Purpose**: To functionally verify that all user interfaces of the DUT are completely documented in
`IXIT 27-UserIntf` (`a`), and that the data input validation methods in `IXIT 29-InpVal` effectively
prevent the processing of unexpected or malicious input data (`b`).

---

### Test Units Functional Assessment Matrix

#### Test Unit A & B: Functional User Interface Audit and Fuzzing / Vulnerability Inspection

**Evidence status:** The network scan and fuzzing statements below are prior recorded assessments.
The physical-button entry is corrected from source review and the manufacturer's timing report;
there is no new switch-chatter injection result. Functional verification of the requested debounce
and minimum sampling time remains pending. Do not treat the corrected procedure as a new live PASS.

| User Interface ID (`IXIT 27-UserIntf`)      | Functional Fuzzing / Injection Action Executed                                                                               | Observed DUT Behavior & System Resilience                                                                                                                                                                                                                                                                                                                       | Unit Verdict       |
|:--------------------------------------------|:-----------------------------------------------------------------------------------------------------------------------------|:----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|:-------------------|
| **`UserIntf-Physical-Configure-Button`**    | Verify deliberate short presses, restart holds, boot-time release/held states, and pulses below, at, and above 50 ms.        | Current behavior is described from source review and the reported approximately 3600 ms boot-time sample. No new live-DUT result is supplied here. Timed debounce is absent, so the former claim that chatter was filtered is withdrawn. The 3000 ms minimum and 50 ms pulse rejection require implementation and boundary-test evidence under #1657 and #1658. | **REVIEW PENDING** |
| **`UserIntf-Local-Hotspot-Captive-Portal`** | Web vulnerability scan and fuzzed JSON submissions (10 KB+ SSID strings, non-ASCII characters, and type-inversion payloads). | Scanners confirm the absence of injection vulnerabilities. Fuzzed JSON payloads exceeding buffer boundaries or containing invalid syntax are cleanly rejected with HTTP 400 Bad Request without causing task lockups or firmware crashes.                                                                                                                       | **PASS**           |
| **`UserIntf-LAN-Management-WebUI`**         | Submit malformed JSON structures, omitted required fields, truncated Bearer tokens, and out-of-range network parameters.     | The HTTP application layer validates all incoming schemas, returning HTTP 400 for bad payloads and HTTP 401 for invalid credentials. Stored parameters in NVS remain uncorrupted.                                                                                                                                                                               | **PASS**           |

* **Functional Assessment Justification**:
  1. **UI Documentation Completeness (Unit a):** Inspection of the physical enclosure, hardware
     schematics, and running firmware services confirms that all user interfaces (`CONFIGURE`
     button, captive portal wizard, and LAN Web-UI) are documented completely in `IXIT 27-UserIntf`.
  2. **Unexpected Data Processing Prevention (Unit b):** The prior HTTP payload assessment remains
     recorded below. A timed button debounce control is absent; short-pulse rejection and minimum
     boot sampling time must be verified after #1658 and #1657 are implemented.

* **Verdict**: **REVIEW PENDING** (physical-button controls and evidence)

---

## Summary Matrix for Test Case 5.13-1A-1 & 5.13-1A-2

| Test Case            | Purpose / Focus                            | Assessment Summary                                                                                                 |    Unit Verdict    |
|:---------------------|:-------------------------------------------|:-------------------------------------------------------------------------------------------------------------------|:------------------:|
| **5.13-1A-1 Unit a** | User Interface Coverage Check              | All user interfaces in `IXIT 27-UserIntf` are mapped to input validation mechanisms in `IXIT 29-InpVal`.           |      **PASS**      |
| **5.13-1A-1 Unit b** | Validation Effectiveness Evaluation        | Runtime restart timing and boot-state sampling exist; minimum sampling time and timed debounce remain pending.     | **REVIEW PENDING** |
| **5.13-1A-2 Unit a** | Interface Documentation Completeness       | Physical and software interface inspections confirm complete documentation across all operational user interfaces. |      **PASS**      |
| **5.13-1A-2 Unit b** | Functional Fuzzing & Robustness Inspection | Prior API fuzzing assessment retained; button timing and pulse-rejection evidence pending under #1657 and #1658.   | **REVIEW PENDING** |

---

## Group Summary

The user interfaces are mapped to the methods in `IXIT 29-InpVal`. For the physical button,
press and release within two seconds is operator guidance for opening the hotspot without reboot;
the runtime firmware has a five-second restart timer and no two-second branch. After restart,
use a 3 seconds window to release and keep the button released; the current sample is reported at
approximately 3600 ms. A held button at that sample starts partition erasure. Wait for the red
200 ms on / 200 ms off completion signal, then release to trigger another restart.

The explicit minimum sampling time and timed rejection of presses released within 50 ms are
pending under [#1657](https://github.com/ruuvi/ruuvi.gateway_esp.c/issues/1657) and
[#1658](https://github.com/ruuvi/ruuvi.gateway_esp.c/issues/1658), both milestone `v1.17.6`.
The earlier claim of implemented debounce and successful chatter filtering is withdrawn. The prior
network-interface assessments remain recorded; an unqualified group PASS requires review of the
button controls and completed functional evidence.

**Group Verdict**: **REVIEW PENDING**
