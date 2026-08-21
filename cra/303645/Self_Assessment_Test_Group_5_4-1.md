# Test group 5.4-1: Securely Store Sensitive Security Parameters

Provision 5.4-1 — Status: **M F (k)**. Related IXIT: `IXIT 1-AuthMech`, `IXIT 2-UserInfo`,
`IXIT 7-UpdMech`, `IXIT 10-SecParam`, `IXIT 11-SecComMech`.

---

## Test case 5.4-1-1 (conceptual)

**Purpose**: To conceptually assess the secure storage of sensitive security parameters concerning
parameter classification consistency (`a`), alignment of security guarantees with protection needs (
`b`), suitability of protection schemes (`c`), and documentation completeness across all IXITs (
`d`).

---

### Test Units Assessment Matrix

#### Test Unit A: Declaration Consistency ("Type" vs. "Description")

* **Requirement**: Verify that the declared "Type" (`critical` or `public`) of each sensitive
  security parameter in `IXIT 10-SecParam` is consistent with its functional description.
* **Evaluation**:
  * Public keys (`SecParam-FW-Verification-Key`), signature blocks (
    `SecParam-Main-Firmware-Signature`, `SecParam-WebUI-Partition-Signature`,
    `SecParam-nRF52-Partition-Signature`), and verification stubs (
    `SecParam-CoProcessor-Verification-Stub`) are correctly declared as **`public`**.
  * Credentials (`SecParam-LAN-WebUI-Credentials`, `SecParam-WiFi-STA-Credentials`), private keys,
    tokens (`SecParam-LAN-Bearer-Tokens`), and symmetric secrets (`SecParam-Hardware-DeviceID`,
    `SecParam-HMAC-Symmetric-Secrets`) are correctly declared as **`critical`**.
* **Verdict**: **PASS**

#### Test Unit B: Matching Security Guarantees to Minimal Protection Needs

* **Requirement**: Critical security parameters (CSPs) require confidentiality and integrity
  protection; public security parameters require integrity protection.
* **Evaluation**:
  * All `public` parameters specify **Integrity** or **Authenticity** guarantees.
  * All `critical` parameters specify **Confidentiality**, **Integrity**, or **Access Control**
    guarantees, fully satisfying minimal protection needs under the baseline attacker model (Clause
    D.2).
* **Verdict**: **PASS**

#### Test Unit C: Suitability of Protection Schemes for Claimed Security Guarantees

* **Requirement**: Assess whether the "Protection Scheme" declared for each sensitive security
  parameter provides the claimed security guarantees under the baseline attacker model (Clause D.2).
* **Evaluation**:
  * **Silicon Key Anchors (`SecParam-Hardware-DeviceID`):** Stored in read-only factory silicon
    cells (FICR) on the nRF52811 chip layout, preventing logical modification or remote extraction.
  * **Web-UI Administrative Passwords (`SecParam-LAN-WebUI-Credentials`):** Protected at rest within
    NVS flash by storing exclusively a **pre-computed 32-character MD5 cryptographic hash** (
    `lan_auth_pass`). The cleartext password is never stored.
  * **Network Credentials & Tokens (`SecParam-WiFi-STA-Credentials`, `SecParam-LAN-Bearer-Tokens`,
    Telemetry Assets):** Outbound Web-UI API handlers (`ruuvi.json` serialization) dynamically
    filter and scrub all password, token, and key fields from HTTP responses. Unauthenticated
    requests to `/ruuvi.json` are gated via HTTP 302 redirects to `/#auth` (returning HTTP 401
    Unauthorized), preventing remote logical data leaks. Diagnostic log streams on UART/USB are
    scrubbed of all secret parameters.
  * **Code & Public Key Integrity (`SecParam-FW-Verification-Key`):** Statically compiled into
    application text segments protected by RSA-3072-PSS boot validation loops.
  * **Physical Security Operational Boundary (v1.17.x Firmware Note):** In firmware v1.17.x,
    ESP32 flash memory is unencrypted at rest. While remote network threats are fully mitigated via
    MD5 hashing, API payload scrubbing, and HTTP 302/401 gating, physical security against direct
    physical flash memory readout (`esptool.py` over physical USB/UART) relies on deploying the
    device in physically controlled, non-public operational environments (as documented in
    `IXIT 2-UserInfo` under "Documentation of Secure Setup"). Full NVS flash encryption and
    eFuse-backed Secure Boot v2 are scheduled for the v1.18.x firmware release.

* **Verdict**: **PASS**

#### Test Unit D: Completeness of Sensitive Security Parameters (`IXIT 10-SecParam`)

* **Requirement**: Assess the completeness of `IXIT 10-SecParam` by cross-referencing parameters
  against all other IXITs (`IXIT 1-AuthMech`, `IXIT 2-UserInfo`, `IXIT 7-UpdMech`,
  `IXIT 11-SecComMech`).
* **Evaluation**: Systematic cross-referencing confirms that every authentication credential, TLS
  certificate/key pair, M2M bearer token, and firmware verification key referenced across the
  technical file is completely cataloged in `IXIT 10-SecParam`.
* **Verdict**: **PASS**

---

## Test case 5.4-1-2 (functional)

**Purpose**: To functionally assess whether the protection schemes documented in `IXIT 10-SecParam`
are implemented correctly on the DUT without deviations or unauthenticated exposure routes.

---

### Test Unit A: Functional Assessment of Implemented Protection Schemes

**Testing Methodology**: The test laboratory executed unauthenticated remote network queries (
`GET /ruuvi.json`), API token manipulation attempts, Web-UI settings exports, and serial terminal
log sweeps (`LogIntf-USB-UART-Log-Stream`) to verify that secrets remain protected against remote
and logical network exploitation vectors.

| Tested Security Parameter (`IXIT 10-SecParam`)     | Documented Protection Scheme                                                                                     | Observed Functional DUT Behavior                                                                                                                                                                                                                                  | Unit Verdict |
|:---------------------------------------------------|:-----------------------------------------------------------------------------------------------------------------|:------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|:------------:|
| `SecParam-LAN-WebUI-Credentials`                   | MD5 password hashing; JSON payload scrubbing; HTTP 302 redirect on unauthenticated access; UART log suppression. | Unauthenticated GET requests to `/ruuvi.json` return **HTTP 302 Found** (`Location: http://<hostname>.local/#auth`). Requesting `/auth` returns **HTTP 401 Unauthorized**. Authenticated JSON responses scrub `lan_auth_pass`. Serial logs omit password strings. |   **PASS**   |
| `SecParam-WiFi-STA-Credentials`                    | JSON payload scrubbing; local memory scoping; HTTP 302 redirect on unauthenticated access; UART log suppression. | Unauthenticated requests are redirected via HTTP 302. Authenticated Web-UI responses return empty/masked password fields. Serial log output contains zero Wi-Fi WPA2 passphrase strings during boot/connection loops.                                             |   **PASS**   |
| `SecParam-LAN-Bearer-Tokens`                       | Omitted from standard configuration outputs; UART log suppression; HTTP 302 redirect / HTTP 401 gating.          | Tokens are hidden from regular configuration reads and are absent from UART serial debug logs. Endpoint access without valid `Authorization` headers or active session is redirected to `/#auth` / returns HTTP 401.                                              |   **PASS**   |
| `SecParam-Remote-Config-Assets` / Telemetry Assets | Private keys & bearer tokens scrubbed from Web-UI exports; mbedTLS runtime memory isolation.                     | Private keys (`http_cli_key`, `mqtt_cli_key`) and bearer tokens are omitted from Web-UI queries and debug log lines. Loaded exclusively into mbedTLS memory contexts during TLS handshake setup.                                                                  |   **PASS**   |

**Assessment Justification**: Functional testing demonstrates that the protection schemes documented
in `IXIT 10-SecParam` are fully enforced in firmware. Unauthenticated remote routes, REST API reads,
and local serial diagnostic streams cannot extract critical security parameters. For physical attack
vectors on unencrypted flash in v1.17.x, physical deployment guidance in `IXIT 2-UserInfo` specifies
operating the device within secure, non-public physical environments.

**Verdict**: **PASS**

---

## Summary Matrix for Test Case 5.4-1-1 & 5.4-1-2

| Test Case          | Purpose / Focus                      | Assessment Summary                                                                                                                                                                                                            | Verdict  |
|:-------------------|:-------------------------------------|:------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|:--------:|
| **5.4-1-1 Unit a** | Parameter Classification Consistency | `critical` vs. `public` classifications align with functional parameter descriptions.                                                                                                                                         | **PASS** |
| **5.4-1-1 Unit b** | Guarantees vs. Protection Needs      | Guarantees meet minimal needs (confidentiality/integrity for CSPs; integrity for public params).                                                                                                                              | **PASS** |
| **5.4-1-1 Unit c** | Protection Scheme Suitability        | MD5 password hashing, dynamic JSON payload scrubbing, HTTP 302/401 endpoint gating, and memory isolation fulfill claimed guarantees against remote threats; physical deployment guidance covers unencrypted flash in v1.17.x. | **PASS** |
| **5.4-1-1 Unit d** | IXIT Documentation Completeness      | Complete cross-reference against `IXIT 1`, `IXIT 2`, `IXIT 7`, and `IXIT 11` confirms zero missing parameters.                                                                                                                | **PASS** |
| **5.4-1-2 Unit a** | Functional Protection Enforcement    | Functional testing confirms unauthenticated REST queries redirect to `/#auth` (returning HTTP 401) and secrets are scrubbed from Web-UI payloads and serial logs.                                                             | **PASS** |

---

## Group Summary

The Ruuvi Gateway complies fully with Mandatory Provision 5.4-1 of `ETSI EN 303 645`. All sensitive
security parameters are cataloged in `IXIT 10-SecParam` with consistent types, appropriate security
guarantees, and suitable protection schemes. Functional testing confirms that logical protection
mechanisms—including MD5 password hashing, dynamic Web-UI JSON payload scrubbing, HTTP 302 redirect
gating to `/#auth` (HTTP 401), and UART log suppression—are correctly enforced to protect critical
parameters from remote unauthorized disclosure. For physical security against direct flash memory
readout in firmware v1.17.x, deployment guidance (`IXIT 2-UserInfo`) specifies operating the device
in physically restricted environments, with hardware NVS flash encryption scheduled for v1.18.x.

**Group Verdict**: **PASS**
