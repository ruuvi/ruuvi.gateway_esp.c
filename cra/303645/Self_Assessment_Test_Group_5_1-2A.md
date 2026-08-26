# Test group 5.1-2A: Passwords for M2M Authentication

## Test case 5.1-2A-1 (conceptual)

**Purpose**: To conceptually assess whether any mechanism declared for machine-to-machine (M2M)
interaction uses passwords as an authentication factor.

### Test Unit A

| IXIT Entry ID                        | Description / Context              | Used for M2M? | Auth Factor Includes Password? | Case Verdict |
|:-------------------------------------|:-----------------------------------|:-------------:|:-------------------------------|:------------:|
| `AuthMech-Hotspot-Provisioning`      | Wi-Fi Onboarding Hotspot           |      No       | N/A                            |   **PASS**   |
| `AuthMech-LAN-WebUI-Default`         | LAN Web-UI (Default State)         |      No       | N/A                            |   **PASS**   |
| `AuthMech-LAN-WebUI-User-Defined`    | Custom Administrative Login        |      No       | N/A                            |   **PASS**   |
| `AuthMech-LAN-WebUI-Basic`           | Legacy Basic Auth Fallback         |      No       | N/A                            |   **PASS**   |
| `AuthMech-LAN-WebUI-Digest`          | Legacy Digest Auth Interface       |      No       | N/A                            |   **PASS**   |
| `AuthMech-LAN-WebUI-Unauthenticated` | Open LAN Management State          |      No       | N/A                            |   **PASS**   |
| `AuthMech-LAN-WebUI-Disabled`        | Restricted Local Network Access    |      No       | N/A                            |   **PASS**   |
| `AuthMech-M2M-API-Bearer-RO`         | Programmatic REST API (`/history`) |      Yes      | No (High-Entropy Token Only)   |   **PASS**   |
| `AuthMech-M2M-API-Bearer-RW`         | Programmatic Configuration Node    |      Yes      | No (High-Entropy Token Only)   |   **PASS**   |

**Assessment**: Review of the `IXIT 1-AuthMech` declarations verifies that zero authentication
mechanisms assigned to programmatic machine-to-machine automated interaction utilize passwords.
Automated system interactions rely exclusively on token structures.

**Verdict**: **PASS**

---

## Test case 5.1-2A-2 (functional)

### Test Unit A: Verification of Exhaustive Documentation

**Purpose**: To functionally verify that no undocumented machine-to-machine interfaces that accept
passwords are exposed by the device.
**Results**: Network scanning sweeps mapping all active TCP/UDP interfaces (referencing verification
metrics captured in `Test case 5.1-1-2`) confirm that all addressable ports are accounted for. No
hidden automation pipelines or undocumented programmatic APIs exist.

**Verdict**: **PASS**

### Test Unit B: Password Rejection for M2M

**Purpose**: To functionally verify that M2M-specific interfaces do not accept password payloads,
even if an attacker attempts to force their use via request headers or body parameters.

| Documented IXIT Entry ID                                      | Targeted Interface Node            | Functional Test Activity Executed                                        | Observed Result                                      | Unit Verdict |
|:--------------------------------------------------------------|:-----------------------------------|:-------------------------------------------------------------------------|:-----------------------------------------------------|:------------:|
| `AuthMech-LAN-WebUI-Default` to `AuthMech-LAN-WebUI-Disabled` | Administrative Web-UI              | N/A (Validated as Interactive User-to-Machine Interface)                 | N/A                                                  |   **PASS**   |
| `AuthMech-M2M-API-Bearer-RO`                                  | Local API (`/history`)             | Injected `Authorization: Basic` credentials and password JSON parameters | Formally rejected; server returns `401 Unauthorized` |   **PASS**   |
| `AuthMech-M2M-API-Bearer-RW`                                  | Configuration Node (`/ruuvi.json`) | Injected `Authorization: Basic` credentials and password JSON parameters | Formally rejected; server returns `401 Unauthorized` |   **PASS**   |

**Assessment**: Functional testing confirms that token-locked programmatic endpoints strictly reject
standard user password configurations or alternative non-token request schemas. The web engine
denies processing contexts, short-circuiting the query validation loops when a valid token from the
`ruuvi.json` manifest within the `nvs` flash partition is absent.

**Verdict**: **PASS**

---

## Group Summary

The Ruuvi Gateway complies fully with Provision 5.1-2A of ETSI EN 303 645. Interactive user
interfaces utilize password-based or chosen cryptographic challenges, while all machine-to-machine
automation endpoints are restricted to token validation routines. This layout mitigates risks
associated with hardcoded or shared credentials in automated setups.

**Group Verdict**: **PASS**
